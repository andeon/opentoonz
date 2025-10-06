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
#include <vector>  // FIX: For transformFeedback

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

ShadingContext::Imp::Imp() : m_context(new QOpenGLContext()), m_surface(nullptr) {}  // FIX: Initialize surface as nullptr

//--------------------------------------------------------

QSurfaceFormat ShadingContext::Imp::format() {
  QSurfaceFormat fmt;

#ifdef MACOSX
  fmt.setVersion(3, 2);
  fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
#else
  fmt.setVersion(2, 1);
  fmt.setProfile(QSurfaceFormat::CompatibilityProfile); 
#endif

  // FIX: Add basic buffers for compatibility
  fmt.setRedBufferSize(8);
  fmt.setGreenBufferSize(8);
  fmt.setBlueBufferSize(8);
  fmt.setAlphaBufferSize(8);
  fmt.setDepthBufferSize(24);

  fmt.setSwapBehavior(QSurfaceFormat::SingleBuffer);

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
  m_imp->m_context->setFormat(Imp::format());  // FIX: Use improved format
  m_imp->m_context->create();
  m_imp->m_context->makeCurrent(m_imp->m_surface);

  makeCurrent();
  glewExperimental = GL_TRUE;  // Always enable
  
  GLenum err = glewInit();
  if (err != GLEW_OK) {
    qWarning() << "GLEW init failed:" << (const char *)glewGetErrorString(err);
  }

  doneCurrent();
}
//--------------------------------------------------------

ShadingContext::~ShadingContext() {
  // Destructor of QGLPixelBuffer calls QOpenGLContext::makeCurrent()
  // internally,
  // so the current thread must be the owner of QGLPixelBuffer context,
  // when the destructor of m_imp->m_context is called.
  // FIX: Adapt for modern and clean up
  m_imp->m_context->moveToThread(QThread::currentThread());
  m_imp->m_context->doneCurrent();
}

//--------------------------------------------------------

ShadingContext::Support ShadingContext::support() {
  // return !QGLPixelBuffer::hasOpenGLPbuffers()
  //           ? NO_PIXEL_BUFFER
  //           : !QOpenGLShaderProgram::hasOpenGLShaderPrograms() ? NO_SHADERS :
  //           OK;
  return !QOpenGLShaderProgram::hasOpenGLShaderPrograms() ? NO_SHADERS : OK;
}

//--------------------------------------------------------

bool ShadingContext::isValid() const { return m_imp->m_context->isValid(); }

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
  if (!m_imp->m_context || !m_imp->m_surface) {
    qWarning() << "OpenGL context or surface is null!";
    return;
  }

  m_imp->m_context->moveToThread(QThread::currentThread());
  m_imp->m_context->makeCurrent(m_imp->m_surface);
}

//--------------------------------------------------------

void ShadingContext::doneCurrent() {
  m_imp->m_context->doneCurrent();
}

//--------------------------------------------------------

void ShadingContext::resize(int lx, int ly,
                            const QOpenGLFramebufferObjectFormat &fmt) {
  if (m_imp->m_fbo.get() && m_imp->m_fbo->width() == lx &&
      m_imp->m_fbo->height() == ly && m_imp->m_fbo->format() == fmt)
    return;

  makeCurrent();  // FIX: Ensure context before FBO

  if (m_imp->m_fbo.get()) {
    m_imp->m_fbo->release();  // FIX: Release the old one
  }

  if (lx == 0 || ly == 0) {
    m_imp->m_fbo.reset(0);
  } else {
    // FIX: Remove infinite loop and unused vars
    m_imp->m_fbo.reset(new QOpenGLFramebufferObject(lx, ly, fmt));
    assert(m_imp->m_fbo->isValid());

    m_imp->m_fbo->bind();
  }

  doneCurrent();  // FIX: Balance the current
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
  makeCurrent();  // FIX: Ensure context for GL calls

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

  doneCurrent();  // FIX: Balance the current
  return texId;
}

//----------------------------------------------------------------------

void ShadingContext::unloadTexture(GLuint texId) {
  makeCurrent();  // FIX: Ensure context
  glDeleteTextures(1, &texId);
  doneCurrent();
}

//--------------------------------------------------------

void ShadingContext::draw(const TRasterP &dst) {
  assert("ShadingContext::resize() must be invoked at least once before this" &&
         m_imp->m_fbo.get());

  makeCurrent();  // FIX: Ensure context

  int lx = dst->getLx(),
      ly = dst->getLy();  // NOTE: We're not using m_imp->m_fbo's size, since
                          // it could be possibly greater than the required
                          // destination surface.

  m_imp->m_fbo->bind();  // FIX: Bind FBO for offscreen render
  m_imp->initMatrix(lx, ly);  // This call sets the OpenGL viewport to this
                              // size - and matches (1, 1) to dst's (lx, ly)

  // FIX: Clear the buffer to avoid garbage
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  {
    glBegin(GL_QUADS);

    glColor3f(1.0f, 1.0f, 1.0f);  // FIX: Add color so the quad doesn't come out transparent/black
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

  m_imp->m_fbo->release();  // FIX: Release FBO
  doneCurrent();
}

//--------------------------------------------------------

void ShadingContext::transformFeedback(int varyingsCount,
                                       const GLsizeiptr *varyingSizes,
                                       GLvoid **bufs) {
  makeCurrent();  // FIX: Ensure context

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

  assert(glGetError() == GL_NO_ERROR);  // FIX: Add assert at the end

  doneCurrent();
}