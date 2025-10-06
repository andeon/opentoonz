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
#include <vector>  // For transformFeedback

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

ShadingContext::Imp::Imp() : m_context(new QOpenGLContext()), m_surface(nullptr), m_fbo(nullptr) {}

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

  // Add basic buffers for compatibility
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
  if (m_imp->m_surface && !m_imp->m_surface->isValid()) {
    m_imp->m_surface->create();
  }
  m_imp->m_context->setFormat(Imp::format());
  if (!m_imp->m_context->create()) {
    qWarning() << "Failed to create OpenGL context - no GL support?";
    return;
  }
  if (!m_imp->m_context->isValid()) {
    qWarning() << "OpenGL context invalid after creation";
    return;
  }

  makeCurrent();

  glewExperimental = GL_TRUE;
  
  GLenum err = glewInit();
  if (err != GLEW_OK) {
    qWarning() << "GLEW init failed:" << (const char *)glewGetErrorString(err);
  }

  doneCurrent();
}

//--------------------------------------------------------

ShadingContext::~ShadingContext() {
  if (m_imp->m_context && m_imp->m_context->isValid()) {
    m_imp->m_context->moveToThread(QThread::currentThread());
    m_imp->m_context->doneCurrent();
  }
}

//--------------------------------------------------------

ShadingContext::Support ShadingContext::support() {
  if (!QOpenGLShaderProgram::hasOpenGLShaderPrograms()) return NO_SHADERS;
  
  // Quick touch test: try minimal init (static, no full ctor)
  QOffscreenSurface testSurface;
  if (!testSurface.create()) return NO_OPENGL;
  
  QOpenGLContext testCtx;
  testCtx.setFormat(Imp::format());
  if (!testCtx.create() || !testCtx.isValid()) return NO_OPENGL;
  
  if (!testCtx.makeCurrent(&testSurface)) return NO_OPENGL;
  
  // Minimal GLEW check
  glewExperimental = GL_TRUE;
  GLenum err = glewInit();
  testCtx.doneCurrent();
  
  return (err == GLEW_OK) ? OK : NO_GLEW;
}

//--------------------------------------------------------

bool ShadingContext::isValid() const { 
  return m_imp->m_context && m_imp->m_context->isValid(); 
}

//--------------------------------------------------------

void ShadingContext::makeCurrent() {
  if (!m_imp->m_context || !m_imp->m_surface) {
    qWarning() << "OpenGL context or surface is null!";
    return;
  }
  if (!m_imp->m_context->isValid()) {
    qWarning() << "Cannot make invalid OpenGL context current";
    return;
  }

  m_imp->m_context->moveToThread(QThread::currentThread());
  bool success = m_imp->m_context->makeCurrent(m_imp->m_surface);
  if (!success) {
    qWarning() << "Failed to make OpenGL context current (thread/display issue?)";
  }
}

//--------------------------------------------------------

void ShadingContext::doneCurrent() {
  if (m_imp->m_context && m_imp->m_context->isValid()) {
    m_imp->m_context->doneCurrent();
  }
}

// Helper for GL ops
bool ShadingContext::beginGL() {
  makeCurrent();
  return m_imp->m_context && m_imp->m_context->isValid() && glGetError() == GL_NO_ERROR;
}

//--------------------------------------------------------

void ShadingContext::resize(int lx, int ly,
                            const QOpenGLFramebufferObjectFormat &fmt) {
  if (m_imp->m_fbo.get() && m_imp->m_fbo->width() == lx &&
      m_imp->m_fbo->height() == ly && m_imp->m_fbo->format() == fmt)
    return;

  if (lx == 0 || ly == 0) {
    m_imp->m_fbo.reset(nullptr);
    return;
  }

  if (!beginGL()) {
    qWarning() << "Cannot resize FBO - invalid context";
    return;
  }

  if (m_imp->m_fbo.get()) {
    m_imp->m_fbo->release();
  }

  m_imp->m_fbo.reset(new QOpenGLFramebufferObject(lx, ly, fmt));
  if (!m_imp->m_fbo->isValid()) {
    qWarning() << "Invalid FBO after resize";
    m_imp->m_fbo.reset(nullptr);
    doneCurrent();
    return;
  }

  m_imp->m_fbo->bind();
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

  return (st != m_imp->m_shaderPrograms.end()) ? st->second.m_program.get() : nullptr;
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
             : std::make_pair((QOpenGLShaderProgram *)nullptr, QDateTime());
}

//--------------------------------------------------------

GLuint ShadingContext::loadTexture(const TRasterP &src, GLuint texUnit) {
  if (!beginGL()) {
    qWarning() << "Cannot load texture - invalid GL context";
    return 0;
  }

  glActiveTexture(GL_TEXTURE0 + texUnit);

  GLuint texId;
  glGenTextures(1, &texId);
  glBindTexture(GL_TEXTURE_2D, texId);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

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

  GLenum err = glGetError();
  doneCurrent();
  if (err != GL_NO_ERROR) {
    qWarning() << "Texture load error: " << err;
    unloadTexture(texId);
    return 0;
  }
  return texId;
}

//----------------------------------------------------------------------

void ShadingContext::unloadTexture(GLuint texId) {
  if (texId == 0) return;
  if (!beginGL()) return;
  glDeleteTextures(1, &texId);
  doneCurrent();
}

//--------------------------------------------------------

void ShadingContext::draw(const TRasterP &dst) {
  assert("ShadingContext::resize() must be invoked at least once before this" &&
         m_imp->m_fbo.get());

  if (!beginGL()) {
    qWarning() << "Cannot draw - invalid GL context";
    return;
  }

  int lx = dst->getLx(),
      ly = dst->getLy();

  m_imp->m_fbo->bind();
  m_imp->initMatrix(lx, ly);

  // Clear the buffer to avoid garbage
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glColor3f(1.0f, 1.0f, 1.0f);
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
    glReadPixels(0, 0, lx, ly, GL_BGRA, GL_UNSIGNED_BYTE,
                 dst->getRawData());
  else {
    assert(TRaster64P(dst));
    glReadPixels(0, 0, lx, ly, GL_BGRA, GL_UNSIGNED_SHORT,
                 dst->getRawData());
  }

  GLenum err = glGetError();

  m_imp->m_fbo->release();
  doneCurrent();

  if (err != GL_NO_ERROR) {
    qWarning() << "Draw error: " << err;
  }
}

//--------------------------------------------------------

void ShadingContext::transformFeedback(int varyingsCount,
                                       const GLsizeiptr *varyingSizes,
                                       GLvoid **bufs) {
  if (!beginGL() || varyingsCount <= 0) {
    qWarning() << "Cannot transform feedback - invalid context or count";
    return;
  }

  // Generate buffer objects
  std::vector<GLuint> bufferObjectNames(varyingsCount, 0);

  glGenBuffers(varyingsCount, bufferObjectNames.data());

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
  glDeleteBuffers(varyingsCount, bufferObjectNames.data());

  GLenum err = glGetError();
  doneCurrent();
  if (err != GL_NO_ERROR) {
    qWarning() << "Transform feedback error: " << err;
  }
}