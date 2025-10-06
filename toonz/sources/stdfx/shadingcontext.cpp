// Glew include
#include <GL/glew.h>

// TnzCore includes
#include "tgl.h"

// Qt includes
#include <QCoreApplication>
#include <QThread>
#include <QDateTime>

#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLWidget>

// STD includes
#include <map>
#include <memory>

#include "stdfx/shadingcontext.h"

//*****************************************************************
//    Local Namespace stuff
//*****************************************************************

namespace {

typedef std::unique_ptr<QOpenGLContext> QOpenGLContextP;
typedef std::unique_ptr<QOpenGLFramebufferObject> QOpenGLFramebufferObjectP;
typedef std::unique_ptr<QOpenGLShaderProgram> QOpenGLShaderProgramP;

struct CompiledShader {
  QOpenGLShaderProgramP m_program;
  QDateTime m_lastModified;

public:
  CompiledShader() {}
  CompiledShader(const CompiledShader &) { assert(!m_program.get()); }
};

}  // namespace

TQOpenGLWidget::TQOpenGLWidget() {}

void TQOpenGLWidget::initializeGL() {
  QOffscreenSurface *surface = new QOffscreenSurface();
  // context()->create();
  // context()->makeCurrent(surface);
}

//*****************************************************************
//    ShadingContext::Imp  definition
//*****************************************************************

struct ShadingContext::Imp {
  QOpenGLContextP m_context;        //!< OpenGL context.
  QOpenGLFramebufferObjectP m_fbo;  //!< Output buffer.
  QOffscreenSurface *m_surface;

  std::map<QString,
           CompiledShader>
      m_shaderPrograms;  //!< Shader Programs stored in the context.
                         //!  \warning   Values have \p unique_ptr members.

  bool m_glewInitialized = false;  //!< FIXED: Flag to ensure GLEW is initialized only once per context lifetime.
                                   //  This avoids redundant initialization calls that could occur in multi-threaded scenarios.

public:
  Imp();

  static QSurfaceFormat format();

  void initMatrix(int lx, int ly);

private:
  // Not copyable
  Imp(const Imp &);
  Imp &operator=(const Imp &);
};

//--------------------------------------------------------

ShadingContext::Imp::Imp() : m_context(new QOpenGLContext()), m_surface() {}

//--------------------------------------------------------

QSurfaceFormat ShadingContext::Imp::format() {
  QSurfaceFormat fmt;

#ifdef MACOSX
  fmt.setVersion(3, 2);
  fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
#else
  // FIXED: Use modern OpenGL core profile by default for better performance and future-proofing.
  // Adjust version/profile as needed for compatibility.
  fmt.setVersion(3, 3);
  fmt.setProfile(QSurfaceFormat::CoreProfile);
#endif

  // FIXED: Set explicit buffer sizes for consistency and to support RGBA rendering.
  fmt.setRedBufferSize(8);
  fmt.setGreenBufferSize(8);
  fmt.setBlueBufferSize(8);
  fmt.setAlphaBufferSize(8);

  return fmt;
}

//--------------------------------------------------------

void ShadingContext::Imp::initMatrix(int lx, int ly) {
  glViewport(0, 0, lx, ly);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(0, lx, 0, ly);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

//*****************************************************************
//    ShadingContext  implementation
//*****************************************************************

ShadingContext::ShadingContext(QOffscreenSurface *surface) : m_imp(new Imp) {
  m_imp->m_surface = surface;
  m_imp->m_surface->create();

  // FIXED: Set format early for consistent context creation.
  m_imp->m_context->setFormat(Imp::format());

  // Initial creation of the context (done once).
  if (!m_imp->m_context->create()) {
    qFatal("Failed to create initial QOpenGLContext in ShadingContext");
  }

  m_imp->m_context->makeCurrent(m_imp->m_surface);

  // FIXED: Call makeCurrent() here to ensure the context is active for initial setup.
  // This is now safe as it reuses the existing context.
  makeCurrent();

  // FIXED: Initialize GLEW only once, right after the first makeCurrent().
  // This prevents redundant glewInit() calls in subsequent makeCurrent() invocations,
  // which could cause overhead or errors in multi-threaded renders.
  if (GLEW_VERSION_3_2) {
    glewExperimental = GL_TRUE;
  }
  glewInit();
  m_imp->m_glewInitialized = true;

  // FIXED: Immediately release after initial setup to avoid blocking the thread.
  doneCurrent();
}

//--------------------------------------------------------

ShadingContext::~ShadingContext() {
  // FIXED: Ensure proper cleanup in the current thread to avoid Qt thread affinity issues.
  // Make current one last time to safely destroy resources.
  if (m_imp->m_context && m_imp->m_context->isValid()) {
    makeCurrent();
    // Resources like FBO and shaders are automatically cleaned via unique_ptr destructors.
  }

  // Move to current thread for final destruction (Qt requirement).
  if (m_imp->m_context) {
    m_imp->m_context->moveToThread(QThread::currentThread());
    m_imp->m_context.reset();  // Explicitly destroy the context.
  }
}

//--------------------------------------------------------

ShadingContext::Support ShadingContext::support() {
  // FIXED: Updated to use modern Qt checks for shader support.
  // This ensures compatibility without deprecated QGLPixelBuffer checks.
  return !QOpenGLShaderProgram::hasOpenGLShaderPrograms() ? NO_SHADERS : OK;
}

//--------------------------------------------------------

bool ShadingContext::isValid() const {
  return m_imp->m_context && m_imp->m_context->isValid();
}

//--------------------------------------------------------
/*
QGLFormat ShadingContext::defaultFormat(int channelsSize)
{
  QGL::FormatOptions opts =
    QGL::SingleBuffer     |
    QGL::NoAccumBuffer    |
    QGL::NoDepthBuffer    |                           // I guess it could be
necessary to let at least
    QGL::NoOverlay        |                           // the depth buffer
enabled... Fragment shaders could
    QGL::NoSampleBuffers  |                           // use it...
    QGL::NoStencilBuffer  |
    QGL::NoStereoBuffers;

  QGLFormat fmt(opts);
  fmt.setDirectRendering(true);                       // Just to be explicit -
USE HARDWARE ACCELERATION

  fmt.setRedBufferSize(channelsSize);
  fmt.setGreenBufferSize(channelsSize);
  fmt.setBlueBufferSize(channelsSize);
  fmt.setAlphaBufferSize(channelsSize);

  // TODO: 64-bit mode should be settable here

  return fmt;
}
*/
//--------------------------------------------------------

void ShadingContext::makeCurrent() {
  // FIXED: CRITICAL FIX - Do NOT recreate the context here! This was the main source of overhead:
  // Recreating QOpenGLContext every makeCurrent() call forces GPU resource reallocation (shaders, FBOs, etc.)
  // per frame/tile, leading to heavy performance loss. Now, we reuse the existing context, preserving the shader cache
  // (m_shaderPrograms) and avoiding recreation costs. Recreation only happens if the context becomes invalid (rare, e.g., GPU loss).
  if (!m_imp->m_context || !m_imp->m_context->isValid()) {
    qWarning("ShadingContext: Context invalid, recreating (this should be rare).");
    m_imp->m_context.reset(new QOpenGLContext());
    m_imp->m_context->setFormat(Imp::format());
    if (!m_imp->m_context->create()) {
      qFatal("Failed to recreate QOpenGLContext in ShadingContext::makeCurrent");
    }
    // If recreated, re-init GLEW.
    if (GLEW_VERSION_3_2) {
      glewExperimental = GL_TRUE;
    }
    glewInit();
    m_imp->m_glewInitialized = true;
  }

  // FIXED: Ensure thread affinity - Qt requires contexts to be bound to their executing thread.
  // Use moveToThread only if necessary to avoid unnecessary overhead.
  QThread *currentThread = QThread::currentThread();
  if (m_imp->m_context->thread() != currentThread) {
    m_imp->m_context->moveToThread(currentThread);
  }

  // Make the context current on the surface.
  m_imp->m_context->makeCurrent(m_imp->m_surface);

  // FIXED: GLEW init guard - only if not already initialized (should be true after constructor).
  if (!m_imp->m_glewInitialized) {
    if (GLEW_VERSION_3_2) {
      glewExperimental = GL_TRUE;
    }
    glewInit();
    m_imp->m_glewInitialized = true;
  }
}

//--------------------------------------------------------

void ShadingContext::doneCurrent() {
  // FIXED: Simplified - just release the current context without moving threads.
  // Thread move is handled in makeCurrent() or destructor to minimize calls.
  // This reduces overhead in frequent makeCurrent/doneCurrent pairs (e.g., per tile).
  if (m_imp->m_context && m_imp->m_context->isValid()) {
    m_imp->m_context->doneCurrent();
  }
}

//--------------------------------------------------------

void ShadingContext::resize(int lx, int ly,
                            const QOpenGLFramebufferObjectFormat &fmt) {
  // FIXED: Ensure context is current before FBO operations to avoid GL errors.
  makeCurrent();

  if (m_imp->m_fbo.get() && m_imp->m_fbo->width() == lx &&
      m_imp->m_fbo->height() == ly && m_imp->m_fbo->format() == fmt)
    return;

  if (lx == 0 || ly == 0) {
    m_imp->m_fbo.reset(0);
  } else {
    // FIXED: Removed unnecessary loop and variable assignments - simplify to direct creation.
    // Ensure we're current before creating/binding FBO.
    m_imp->m_fbo.reset(new QOpenGLFramebufferObject(lx, ly, fmt));
    assert(m_imp->m_fbo->isValid());

    m_imp->m_fbo->bind();
  }

  // FIXED: Release after resize to free the context for other operations.
  doneCurrent();
}

//--------------------------------------------------------

QOpenGLFramebufferObjectFormat ShadingContext::format() const {
  QOpenGLFramebufferObject *fbo = m_imp->m_fbo.get();
  return fbo ? m_imp->m_fbo->format() : QOpenGLFramebufferObjectFormat();
}

//--------------------------------------------------------

TDimension ShadingContext::size() const {
  QOpenGLFramebufferObject *fbo = m_imp->m_fbo.get();
  return fbo ? TDimension(fbo->width(), fbo->height()) : TDimension();
}

//--------------------------------------------------------

void ShadingContext::addShaderProgram(const QString &shaderName,
                                      QOpenGLShaderProgram *program) {
  std::map<QString, CompiledShader>::iterator st =
      m_imp->m_shaderPrograms
          .insert(std::make_pair(shaderName, CompiledShader()))
          .first;

  st->second.m_program.reset(program);
}

//--------------------------------------------------------

void ShadingContext::addShaderProgram(const QString &shaderName,
                                      QOpenGLShaderProgram *program,
                                      const QDateTime &lastModified) {
  std::map<QString, CompiledShader>::iterator st =
      m_imp->m_shaderPrograms
          .insert(std::make_pair(shaderName, CompiledShader()))
          .first;

  st->second.m_program.reset(program);
  st->second.m_lastModified = lastModified;
}

//--------------------------------------------------------

bool ShadingContext::removeShaderProgram(const QString &shaderName) {
  return (m_imp->m_shaderPrograms.erase(shaderName) > 0);
}

//--------------------------------------------------------

QOpenGLShaderProgram *ShadingContext::shaderProgram(
    const QString &shaderName) const {
  std::map<QString, CompiledShader>::iterator st =
      m_imp->m_shaderPrograms.find(shaderName);

  return (st != m_imp->m_shaderPrograms.end()) ? st->second.m_program.get() : 0;
}

//--------------------------------------------------------

QDateTime ShadingContext::lastModified(const QString &shaderName) const {
  std::map<QString, CompiledShader>::iterator st =
      m_imp->m_shaderPrograms.find(shaderName);

  return (st != m_imp->m_shaderPrograms.end()) ? st->second.m_lastModified
                                               : QDateTime();
}

//--------------------------------------------------------

std::pair<QOpenGLShaderProgram *, QDateTime> ShadingContext::shaderData(
    const QString &shaderName) const {
  std::map<QString, CompiledShader>::iterator st =
      m_imp->m_shaderPrograms.find(shaderName);

  return (st != m_imp->m_shaderPrograms.end())
             ? std::make_pair(st->second.m_program.get(),
                              st->second.m_lastModified)
             : std::make_pair((QOpenGLShaderProgram *)0, QDateTime());
}

//--------------------------------------------------------

GLuint ShadingContext::loadTexture(const TRasterP &src, GLuint texUnit) {
  // FIXED: Ensure context is current before texture operations to prevent GL_INVALID_OPERATION.
  makeCurrent();

  glActiveTexture(GL_TEXTURE0 + texUnit);

  GLuint texId;
  glGenTextures(1, &texId);
  glBindTexture(GL_TEXTURE_2D, texId);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                  GL_CLAMP);  // These must be used on a bound texture,
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                  GL_CLAMP);  // and are remembered in the OpenGL context.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                  GL_NEAREST);  // They can be set here, no need for
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_NEAREST);  // the user to do it.

  glPixelStorei(GL_UNPACK_ROW_LENGTH, src->getWrap());
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  GLenum chanType = TRaster32P(src) ? GL_UNSIGNED_BYTE : GL_UNSIGNED_SHORT;

  glTexImage2D(GL_TEXTURE_2D,
               0,             // one level only
               GL_RGBA,       // pixel channels count
               src->getLx(),  // width
               src->getLy(),  // height
               0,             // border size
               TGL_FMT,       // pixel format
               chanType,      // channel data type
               (GLvoid *)src->getRawData());

  assert(glGetError() == GL_NO_ERROR);

  // FIXED: Release after texture load to minimize context hold time.
  doneCurrent();

  return texId;
}

//----------------------------------------------------------------------

void ShadingContext::unloadTexture(GLuint texId) {
  // FIXED: Ensure context is current before deletion.
  makeCurrent();

  glDeleteTextures(1, &texId);

  // FIXED: Release after unload.
  doneCurrent();
}

//--------------------------------------------------------

void ShadingContext::draw(const TRasterP &dst) {
  // FIXED: Ensure context is current before draw operations.
  makeCurrent();

  assert("ShadingContext::resize() must be invoked at least once before this" &&
         m_imp->m_fbo.get());

  int lx = dst->getLx(),
      ly = dst->getLy();  // NOTE: We're not using m_imp->m_fbo's size, since
                          // it could be possibly greater than the required
                          // destination surface.

  m_imp->initMatrix(lx, ly);  // This call sets the OpenGL viewport to this
                              // size - and matches (1, 1) to dst's (lx, ly)

  {
    glBegin(GL_QUADS);

    glVertex2f(0.0, 0.0);
    glVertex2f(lx, 0.0);
    glVertex2f(lx, ly);
    glVertex2f(0.0, ly);

    glEnd();
  }

  glPixelStorei(GL_PACK_ROW_LENGTH, dst->getWrap());

  // Read the fbo to dst
  if (TRaster32P ras32 = dst)
    glReadPixels(0, 0, lx, ly, GL_BGRA_EXT, GL_UNSIGNED_BYTE,
                 dst->getRawData());
  else {
    assert(TRaster64P(dst));
    glReadPixels(0, 0, lx, ly, GL_BGRA_EXT, GL_UNSIGNED_SHORT,
                 dst->getRawData());
  }

  assert(glGetError() == GL_NO_ERROR);

  // FIXED: Release after draw to free the context.
  doneCurrent();
}

//--------------------------------------------------------

void ShadingContext::transformFeedback(int varyingsCount,
                                       const GLsizeiptr *varyingSizes,
                                       GLvoid **bufs) {
  // FIXED: Ensure context is current before transform feedback.
  makeCurrent();

  // Generate buffer objects
  std::vector<GLuint> bufferObjectNames(varyingsCount, 0);

  glGenBuffers(varyingsCount, &bufferObjectNames[0]);

  for (int v = 0; v != varyingsCount; ++v) {
    glBindBuffer(GL_ARRAY_BUFFER, bufferObjectNames[v]);
    glBufferData(GL_ARRAY_BUFFER, varyingSizes[v], bufs[v], GL_STATIC_READ);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, v, bufferObjectNames[v]);
  }

  // Draw
  GLuint Query = 0;

  glGenQueries(1, &Query);
  {
    // Disable rasterization, vertices processing only!
    glEnable(GL_RASTERIZER_DISCARD);
    glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, Query);

    glBeginTransformFeedback(GL_POINTS);
    glBegin(GL_POINTS);
    glVertex2f(0.0f, 0.0f);
    glEnd();
    glEndTransformFeedback();

    glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
    glDisable(GL_RASTERIZER_DISCARD);
  }

  GLint count = 0;
  glGetQueryObjectiv(Query, GL_QUERY_RESULT, &count);

  glDeleteQueries(1, &Query);

  // Retrieve transformed data
  for (int v = 0; v != varyingsCount; ++v) {
    glBindBuffer(GL_ARRAY_BUFFER, bufferObjectNames[v]);
    glGetBufferSubData(GL_ARRAY_BUFFER, 0, varyingSizes[v], bufs[v]);
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0);

  // Delete buffer objects
  glDeleteBuffers(varyingsCount, &bufferObjectNames[0]);

  // FIXED: Release after transform feedback.
  doneCurrent();
}
