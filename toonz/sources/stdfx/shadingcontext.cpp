

#include "stdfx/shadingcontext.h"

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
#include <QSurfaceFormat>

// STD includes
#include <map>
#include <memory>
#include <cassert>
#include <vector>

namespace {

// Shared OpenGL context and surface
QOpenGLContext *g_sharedContext = nullptr;
QOffscreenSurface *g_sharedSurface = nullptr;
bool g_initialized = false;

// UPDATED: Explicit format for shaders (GL 3.3 Core for Qt 5.15/NVIDIA)
QSurfaceFormat sharedFormat() {
  QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
  fmt.setVersion(3, 3);
  fmt.setProfile(QSurfaceFormat::CoreProfile);
  fmt.setSwapBehavior(QSurfaceFormat::SingleBuffer);
  fmt.setDepthBufferSize(0);
  fmt.setStencilBufferSize(0);
  fmt.setRedBufferSize(8);  // RGBA8 for TRaster32P
  fmt.setGreenBufferSize(8);
  fmt.setBlueBufferSize(8);
  fmt.setAlphaBufferSize(8);
  return fmt;
}

void initializeSharedContext() {
  if (g_initialized) return;

  g_sharedSurface = new QOffscreenSurface();
  g_sharedSurface->setFormat(sharedFormat());
  g_sharedSurface->create();  // FIXED: create() is void; check isValid() after
  if (!g_sharedSurface->isValid()) {
    qWarning() << "Failed to create shared offscreen surface";
    delete g_sharedSurface;
    g_sharedSurface = nullptr;
    return;
  }

  g_sharedContext = new QOpenGLContext();
  g_sharedContext->setFormat(g_sharedSurface->format());
  if (!g_sharedContext->create()) {
    qWarning() << "Failed to create shared OpenGL context";
    delete g_sharedSurface;
    g_sharedSurface = nullptr;
    return;
  }

  g_sharedContext->makeCurrent(g_sharedSurface);
  glewExperimental = GL_TRUE;  // Safe for 3.3+
  GLenum err = glewInit();
  if (err != GLEW_OK) {
    qWarning() << "GLEW init failed:" << glewGetErrorString(err);
  }
  g_sharedContext->doneCurrent();

  g_initialized = true;
}

// OPTIONAL: Static cleanup on app exit (add to a module dtor if needed)
struct SharedCleanup {
  ~SharedCleanup() {
    if (g_sharedContext) {
      g_sharedContext->doneCurrent();
      delete g_sharedContext;
      g_sharedContext = nullptr;
    }
    delete g_sharedSurface;
    g_sharedSurface = nullptr;
    g_initialized = false;
  }
};
static SharedCleanup s_cleanup;  // Runs at static destruction

typedef std::unique_ptr<QOpenGLFramebufferObject> QOpenGLFramebufferObjectP;
typedef std::unique_ptr<QOpenGLShaderProgram> QOpenGLShaderProgramP;

struct CompiledShader {
  QOpenGLShaderProgramP m_program;
  QDateTime m_lastModified;

  CompiledShader() {}
  CompiledShader(const CompiledShader &) { assert(!m_program.get()); }
};

}  // namespace

//*****************************************************************
//    Local Namespace stuff
//*****************************************************************

//--------------------------------------------------------

TQOpenGLWidget::TQOpenGLWidget() {}

void TQOpenGLWidget::initializeGL() {
  // UPDATED: Ensure shared init (in case widget renders first)
  ::initializeSharedContext();
  // For GUI previews, you could makeCurrent() here if needed
}

//*****************************************************************
//    ShadingContext::Imp  definition
//*****************************************************************

struct ShadingContext::Imp {
  QOpenGLFramebufferObjectP m_fbo;  //!< Output buffer.
  std::map<QString, CompiledShader> m_shaderPrograms;  //!< Shader Programs stored in the context.

  void initMatrix(int lx, int ly) {
    glViewport(0, 0, lx, ly);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, lx, 0, ly);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
  }
};

//*****************************************************************
//    ShadingContext  implementation
//*****************************************************************

// UPDATED: Ctor ignores surface (globals); call init if needed
ShadingContext::ShadingContext(QOffscreenSurface * /*surface*/) : m_imp(new Imp) {
  ::initializeSharedContext();  // Always ensure ready
}

//--------------------------------------------------------

ShadingContext::~ShadingContext() {
  // Destructor of QGLPixelBuffer calls QOpenGLContext::makeCurrent()
  // internally,
  // so the current thread must be the owner of QGLPixelBuffer context,
  // when the destructor of m_imp->m_context is called.
  // (Note: Globals handled separately)
}

//--------------------------------------------------------

// NEW: Static check
bool ShadingContext::isInitialized() {
  return g_initialized && g_sharedContext && g_sharedContext->isValid();
}

//--------------------------------------------------------

ShadingContext::Support ShadingContext::support() {
  return !QOpenGLShaderProgram::hasOpenGLShaderPrograms() ? NO_SHADERS : OK;
}

//--------------------------------------------------------

bool ShadingContext::isValid() const {
  return g_sharedContext && g_sharedContext->isValid();
}

//--------------------------------------------------------

// UPDATED: Add thread migration for multi-thread safety
void ShadingContext::makeCurrent() {
  if (!isInitialized()) {
    qWarning() << "Shared context not initialized—call ctor first!";
    return;
  }
  g_sharedContext->moveToThread(QThread::currentThread());
  g_sharedContext->makeCurrent(g_sharedSurface);
}

//--------------------------------------------------------

void ShadingContext::doneCurrent() {
  if (isInitialized()) {
    g_sharedContext->doneCurrent();
  }
}

//--------------------------------------------------------

void ShadingContext::resize(int lx, int ly,
                            const QOpenGLFramebufferObjectFormat &fmt) {
  if (!isInitialized()) {
    qWarning() << "Shared context not ready for resize";
    return;
  }
  if (m_imp->m_fbo &&
      m_imp->m_fbo->width() == lx &&
      m_imp->m_fbo->height() == ly &&
      m_imp->m_fbo->format() == fmt)
    return;

  if (lx == 0 || ly == 0) {
    m_imp->m_fbo.reset();
  } else {
    makeCurrent();  // Ensure bound for create
    m_imp->m_fbo.reset(new QOpenGLFramebufferObject(lx, ly, fmt));
    if (!m_imp->m_fbo->isValid()) {
      qWarning() << "Invalid FBO (" << lx << "x" << ly << ")";
    } else {
      m_imp->m_fbo->bind();
    }
    doneCurrent();
  }
}

//--------------------------------------------------------

QOpenGLFramebufferObjectFormat ShadingContext::format() const {
  return m_imp->m_fbo ? m_imp->m_fbo->format() : QOpenGLFramebufferObjectFormat();
}

//--------------------------------------------------------

TDimension ShadingContext::size() const {
  return m_imp->m_fbo ? TDimension(m_imp->m_fbo->width(), m_imp->m_fbo->height()) : TDimension();
}

//--------------------------------------------------------

void ShadingContext::addShaderProgram(const QString &shaderName,
                                      QOpenGLShaderProgram *program) {
  auto &compiled = m_imp->m_shaderPrograms[shaderName];
  compiled.m_program.reset(program);
}

//--------------------------------------------------------

void ShadingContext::addShaderProgram(const QString &shaderName,
                                      QOpenGLShaderProgram *program,
                                      const QDateTime &lastModified) {
  auto &compiled = m_imp->m_shaderPrograms[shaderName];
  compiled.m_program.reset(program);
  compiled.m_lastModified = lastModified;
}

//--------------------------------------------------------

bool ShadingContext::removeShaderProgram(const QString &shaderName) {
  return m_imp->m_shaderPrograms.erase(shaderName) > 0;
}

//--------------------------------------------------------

QOpenGLShaderProgram *ShadingContext::shaderProgram(const QString &shaderName) const {
  auto it = m_imp->m_shaderPrograms.find(shaderName);
  return it != m_imp->m_shaderPrograms.end() ? it->second.m_program.get() : nullptr;
}

//--------------------------------------------------------

QDateTime ShadingContext::lastModified(const QString &shaderName) const {
  auto it = m_imp->m_shaderPrograms.find(shaderName);
  return it != m_imp->m_shaderPrograms.end() ? it->second.m_lastModified : QDateTime();
}

//--------------------------------------------------------

std::pair<QOpenGLShaderProgram *, QDateTime>
ShadingContext::shaderData(const QString &shaderName) const {
  auto it = m_imp->m_shaderPrograms.find(shaderName);
  if (it != m_imp->m_shaderPrograms.end())
    return {it->second.m_program.get(), it->second.m_lastModified};
  return {nullptr, QDateTime()};
}

//--------------------------------------------------------

GLuint ShadingContext::loadTexture(const TRasterP &src, GLuint texUnit) {
  glActiveTexture(GL_TEXTURE0 + texUnit);

  GLuint texId;
  glGenTextures(1, &texId);
  glBindTexture(GL_TEXTURE_2D, texId);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  glPixelStorei(GL_UNPACK_ROW_LENGTH, src->getWrap());
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  GLenum chanType = TRaster32P(src) ? GL_UNSIGNED_BYTE : GL_UNSIGNED_SHORT;

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, src->getLx(), src->getLy(), 0,
               TGL_FMT, chanType, (GLvoid *)src->getRawData());

  assert(glGetError() == GL_NO_ERROR);

  return texId;
}

//--------------------------------------------------------

void ShadingContext::unloadTexture(GLuint texId) {
  glDeleteTextures(1, &texId);
}

//--------------------------------------------------------

// UPDATED: Add init guard
void ShadingContext::draw(const TRasterP &dst) {
  if (!isInitialized()) {
    qWarning() << "Shared context not ready for draw";
    return;
  }
  assert(m_imp->m_fbo && "Call resize() first!");

  makeCurrent();  // Ensure bound
  int lx = dst->getLx(), ly = dst->getLy();
  m_imp->initMatrix(lx, ly);

  glBegin(GL_QUADS);
  glVertex2f(0.0f, 0.0f);
  glVertex2f(lx, 0.0f);
  glVertex2f(lx, ly);
  glVertex2f(0.0f, ly);
  glEnd();

  glPixelStorei(GL_PACK_ROW_LENGTH, dst->getWrap());

  if (TRaster32P ras32 = dst)
    glReadPixels(0, 0, lx, ly, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dst->getRawData());
  else {
    assert(TRaster64P(dst));
    glReadPixels(0, 0, lx, ly, GL_BGRA_EXT, GL_UNSIGNED_SHORT, dst->getRawData());
  }

  assert(glGetError() == GL_NO_ERROR);
  doneCurrent();
}

//--------------------------------------------------------

void ShadingContext::transformFeedback(int varyingsCount,
                                       const GLsizeiptr *varyingSizes,
                                       GLvoid **bufs) {
  if (!isInitialized()) {
    qWarning() << "Shared context not ready for transformFeedback";
    return;
  }

  makeCurrent();  // Ensure bound

  std::vector<GLuint> bufferObjectNames(varyingsCount, 0);
  glGenBuffers(varyingsCount, &bufferObjectNames[0]);

  for (int v = 0; v < varyingsCount; ++v) {
    glBindBuffer(GL_ARRAY_BUFFER, bufferObjectNames[v]);
    glBufferData(GL_ARRAY_BUFFER, varyingSizes[v], bufs[v], GL_STATIC_READ);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, v, bufferObjectNames[v]);
  }

  GLuint query;
  glGenQueries(1, &query);
  glEnable(GL_RASTERIZER_DISCARD);
  glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, query);

  glBeginTransformFeedback(GL_POINTS);
  glBegin(GL_POINTS);
  glVertex2f(0.0f, 0.0f);
  glEnd();
  glEndTransformFeedback();

  glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
  glDisable(GL_RASTERIZER_DISCARD);

  GLint count = 0;
  glGetQueryObjectiv(query, GL_QUERY_RESULT, &count);
  glDeleteQueries(1, &query);

  for (int v = 0; v < varyingsCount; ++v) {
    glBindBuffer(GL_ARRAY_BUFFER, bufferObjectNames[v]);
    glGetBufferSubData(GL_ARRAY_BUFFER, 0, varyingSizes[v], bufs[v]);
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDeleteBuffers(varyingsCount, &bufferObjectNames[0]);

  doneCurrent();
}