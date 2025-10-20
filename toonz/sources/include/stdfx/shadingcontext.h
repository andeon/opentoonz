#pragma once

#ifndef SHADINGCONTEXT_H
#define SHADINGCONTEXT_H

#include <memory>

// GLEW include
#include <GL/glew.h>

// ToonzCore includes
#include "traster.h"

// Qt includes
#include <QDateTime>
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLWidget>

// Export macros
#undef DVAPI
#undef DVVAR

#ifdef TNZSTDFX_EXPORTS
  #define DVAPI DV_EXPORT_API
  #define DVVAR DV_EXPORT_VAR
#else
  #define DVAPI DV_IMPORT_API
  #define DVVAR DV_IMPORT_VAR
#endif

//=========================================================
//    Forward declarations
//=========================================================

class QObject;
class QOpenGLShaderProgram;
class QDateTime;
class QOffscreenSurface;

//=========================================================
//    ShadingContext class declaration
//=========================================================

class DVAPI ShadingContext {
public:
  enum Support { OK, NO_PIXEL_BUFFER, NO_SHADERS };

public:
  ShadingContext(QOffscreenSurface *surface);
  ~ShadingContext();

  //! Returns the current OpenGL shading support status.
  static Support support();

  //! Check if the context is valid (created and initialized properly).
  bool isValid() const;

  //! Make the context current in the current thread.
  void makeCurrent();

  //! Release the current context.
  void doneCurrent();

  /*!
    Resize the output buffer to the given size. Requires the context
    to be current. If either width or height is 0, the output buffer
    is destroyed.
  */
  void resize(int width, int height,
              const QOpenGLFramebufferObjectFormat &format =
                  QOpenGLFramebufferObjectFormat());

  //! Returns the current FBO format.
  QOpenGLFramebufferObjectFormat format() const;

  //! Returns the size of the framebuffer.
  TDimension size() const;

  //! Store a shader program in the context.
  void addShaderProgram(const QString &shaderName,
                        QOpenGLShaderProgram *program);

  //! Store a shader program with last-modified timestamp.
  void addShaderProgram(const QString &shaderName,
                        QOpenGLShaderProgram *program,
                        const QDateTime &lastModified);

  //! Remove a shader program from the context.
  bool removeShaderProgram(const QString &shaderName);

  //! Get a shader program by name.
  QOpenGLShaderProgram *shaderProgram(const QString &shaderName) const;

  //! Get the last-modified time of a shader program.
  QDateTime lastModified(const QString &shaderName) const;

  //! Get both shader program and its last modified time.
  std::pair<QOpenGLShaderProgram *, QDateTime> shaderData(
      const QString &shaderName) const;

  //! Load a TRaster as a texture to a given OpenGL texture unit.
  GLuint loadTexture(const TRasterP &src, GLuint textureUnit);

  //! Unload (delete) a given OpenGL texture.
  void unloadTexture(GLuint textureId);

  //! Render the currently active shader program into the destination raster.
  void draw(const TRasterP &dst);

  //! Perform transform feedback using shader varyings.
  void transformFeedback(int varyingCount, const GLsizeiptr *varyingSizes,
                         GLvoid **buffers);

private:
  struct Imp;
  std::unique_ptr<Imp> m_imp;

  // Not copyable
  ShadingContext(const ShadingContext &) = delete;
  ShadingContext &operator=(const ShadingContext &) = delete;
};

//=========================================================
//    TQOpenGLWidget - Custom widget class
//=========================================================

class TQOpenGLWidget : public QOpenGLWidget {
public:
  TQOpenGLWidget();
  void initializeGL() override;
};

#endif  // SHADINGCONTEXT_H
