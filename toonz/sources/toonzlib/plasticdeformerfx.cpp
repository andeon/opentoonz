#include "toonz/plasticdeformerfx.h"

// TnzLib includes
#include "toonz/txsheet.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshcell.h"
#include "toonz/tcolumnfx.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/dpiscale.h"
#include "toonz/stage.h"
#include "toonz/tlog.h"

// TnzExt includes
#include "ext/plasticskeleton.h"
#include "ext/plasticdeformerstorage.h"
#include "ext/ttexturesstorage.h"
#include "ext/plasticvisualsettings.h"
#include "ext/meshutils.h"

// TnzBase includes
#include "trenderer.h"

// TnzCore includes
#include "tgl.h"
#include "tofflinegl.h"
#include "tgldisplaylistsmanager.h"
#include "tconvert.h"
#include "trop.h"

#include <QOpenGLFramebufferObject>
#include <QOffscreenSurface>
#include <QSurfaceFormat>
#include <QOpenGLContext>
#include <QImage>
#include <algorithm>  // For std::min/std::max in bbox union

FX_IDENTIFIER_IS_HIDDEN(PlasticDeformerFx, "plasticDeformerFx")

//***************************************************************************************************
//    Local namespace
//***************************************************************************************************

namespace {

std::string toString(const TAffine &aff) {
  // Note: toString distinguishes + and - 0. This is a problem when comparing
  // aliases, so near-zero values are explicitly rounded to 0.
  return (areAlmostEqual(aff.a11, 0.0) ? "0" : ::to_string(aff.a11, 5)) + "," +
         (areAlmostEqual(aff.a12, 0.0) ? "0" : ::to_string(aff.a12, 5)) + "," +
         (areAlmostEqual(aff.a13, 0.0) ? "0" : ::to_string(aff.a13, 5)) + "," +
         (areAlmostEqual(aff.a21, 0.0) ? "0" : ::to_string(aff.a21, 5)) + "," +
         (areAlmostEqual(aff.a22, 0.0) ? "0" : ::to_string(aff.a22, 5)) + "," +
         (areAlmostEqual(aff.a23, 0.0) ? "0" : ::to_string(aff.a23, 5));
}

//-----------------------------------------------------------------------------------

std::string toString(SkVD *vd, double sdFrame) {
  std::string result;

  for (int p = 0; p < SkVD::PARAMS_COUNT; ++p)
    result += ::to_string(vd->m_params[p]->getValue(sdFrame), 5) + " ";

  return result;
}

//-----------------------------------------------------------------------------------

std::string toString(const PlasticSkeleton::vertex_type &vx) {
  // TODO: Add z and rigidity support
  return ::to_string(vx.P().x, 5) + " " + ::to_string(vx.P().y, 5);
}

//-----------------------------------------------------------------------------------

std::string toString(const PlasticSkeletonDeformationP &sd, double sdFrame) {
  std::string result;

  const PlasticSkeletonP &skeleton = sd->skeleton(sdFrame);
  if (!skeleton || skeleton->empty()) return result;

  const tcg::list<PlasticSkeleton::vertex_type> &vertices =
      skeleton->vertices();

  tcg::list<PlasticSkeleton::vertex_type>::const_iterator vt,
      vEnd(vertices.end());

  result = toString(*vertices.begin());
  for (vt = vertices.begin(); vt != vEnd; ++vt) {
    result += "; " + toString(*vt);
    result += " " + toString(sd->vertexDeformation(vt->name()), sdFrame);
  }

  return result;
}

// Safe pixel copy function with bounds checking
void safePixelCopy(TRasterP targetRaster, const QImage &img, bool was64bit) {
  if (!targetRaster) {
    throw std::runtime_error("Target raster is null");
  }

  if (img.isNull()) {
    throw std::runtime_error("Source image is null");
  }

  TDimension d = targetRaster->getSize();

  // Validate dimensions
  if (img.width() != d.lx || img.height() != d.ly) {
    throw std::runtime_error("Image dimension mismatch");
  }

  int expectedBytesPerLine = d.lx * 4;  // RGBA
  if (img.bytesPerLine() < expectedBytesPerLine) {
    throw std::runtime_error("Invalid image stride");
  }

  if (was64bit) {
    TRaster64P target64(targetRaster);
    TRaster32P tempRaster(d);

    if (!tempRaster) {
      throw std::runtime_error("Failed to allocate temporary raster");
    }

    const uchar *srcPix = img.constBits();
    uchar *tempPix      = tempRaster->getRawData();

    // Safe line-by-line copy
    for (int y = 0; y < d.ly; ++y) {
      const uchar *srcRow = srcPix + (y * img.bytesPerLine());
      uchar *dstRow       = tempPix + (y * tempRaster->getWrap());
      memcpy(dstRow, srcRow, expectedBytesPerLine);
    }

    TRop::convert(target64, tempRaster);
  } else {
    TRaster32P target32(targetRaster);
    const uchar *srcPix = img.constBits();
    uchar *dstPix       = target32->getRawData();

    for (int y = 0; y < d.ly; ++y) {
      const uchar *srcRow = srcPix + (y * img.bytesPerLine());
      uchar *dstRow       = dstPix + (y * target32->getWrap());
      memcpy(dstRow, srcRow, expectedBytesPerLine);
    }
  }
}

}  // namespace

//***************************************************************************************************
//    RAII OpenGL Context Management
//***************************************************************************************************

class OpenGLContextRAIIS {
private:
  QOpenGLContext *m_context;
  bool m_isCurrent;
  bool m_ownsContext;

public:
  OpenGLContextRAIIS()
      : m_context(new QOpenGLContext())
      , m_isCurrent(false)
      , m_ownsContext(true) {}

  ~OpenGLContextRAIIS() {
    if (m_context && m_isCurrent) {
      m_context->doneCurrent();
    }
    if (m_context && m_ownsContext) {
      m_context->moveToThread(0);
      delete m_context;
    }
  }

  // Disable copy constructor and assignment to prevent resource sharing issues
  OpenGLContextRAIIS(const OpenGLContextRAIIS &)            = delete;
  OpenGLContextRAIIS &operator=(const OpenGLContextRAIIS &) = delete;

  bool initialize(QOffscreenSurface *surface) {
    if (!surface) {
      TSysLog::error("PlasticDeformerFx: Null surface provided");
      return false;
    }

    if (!m_context) {
      TSysLog::error("PlasticDeformerFx: Failed to create OpenGL context");
      return false;
    }

    QOpenGLContext *existing = QOpenGLContext::currentContext();
    if (existing && existing->isValid() && existing->surface() == surface) {
      // Safe reuse of existing context - don't take ownership
      if (m_ownsContext) {
        delete m_context;
        m_ownsContext = false;
      }
      m_context   = existing;
      m_isCurrent = true;  // Already current
      return true;
    }

    // Create new context with sharing if possible
    if (existing) {
      m_context->setShareContext(existing);
    }

    m_context->setFormat(QSurfaceFormat::defaultFormat());

    if (!m_context->create()) {
      TSysLog::error("PlasticDeformerFx: Failed to create OpenGL context");
      return false;
    }

    m_isCurrent = m_context->makeCurrent(surface);
    if (!m_isCurrent) {
      TSysLog::error("PlasticDeformerFx: Failed to make context current");
      return false;
    }

    m_ownsContext = true;
    return true;
  }

  QOpenGLContext *context() { return m_context; }
  operator bool() const { return m_context && m_isCurrent; }
};

//***************************************************************************************************
//    PlasticDeformerFx  implementation
//***************************************************************************************************

PlasticDeformerFx::PlasticDeformerFx() : TRasterFx() {
  addInputPort("source", m_port);
}

//-----------------------------------------------------------------------------------

TFx *PlasticDeformerFx::clone(bool recursive) const {
  PlasticDeformerFx *fx =
      dynamic_cast<PlasticDeformerFx *>(TFx::clone(recursive));
  assert(fx);

  fx->m_xsh = m_xsh;
  fx->m_col = m_col;

  return fx;
}

//-----------------------------------------------------------------------------------

bool PlasticDeformerFx::canHandle(const TRenderSettings &info, double frame) {
  // Affines are handled directly via OpenGL matrix pushes
  return true;
}

//-----------------------------------------------------------------------------------

std::string PlasticDeformerFx::getAlias(double frame,
                                        const TRenderSettings &info) const {
  std::string alias(getFxType());
  alias += "[";

  if (m_port.isConnected()) {
    TRasterFxP ifx = m_port.getFx();
    assert(ifx);

    alias += ifx->getAlias(frame, info);
  }

  TStageObject *meshColumnObj =
      m_xsh->getStageObject(TStageObjectId::ColumnId(m_col));
  const PlasticSkeletonDeformationP &sd =
      meshColumnObj->getPlasticSkeletonDeformation();
  if (sd) alias += ", " + toString(sd, meshColumnObj->paramsTime(frame));

  alias += "]";

  return alias;
}

//-----------------------------------------------------------------------------------

bool PlasticDeformerFx::doGetBBox(double frame, TRectD &bbox,
                                  const TRenderSettings &info) {
  if (!m_port.isConnected()) return false;

  // Bounding box computation for plastic deformation is complex; return
  // infinite rect
  bbox = TConsts::infiniteRectD;
  return true;
}

//-----------------------------------------------------------------------------------

void PlasticDeformerFx::buildRenderSettings(double frame,
                                            TRenderSettings &info) {
  // This FX handles affines, so defer to input FX's handledAffine() for optimal
  // reference
  m_was64bit = false;
  if (info.m_bpp == 64) {
    m_was64bit = true;
    info.m_bpp = 32;  // Force 32-bpp input for OpenGL compatibility
  }
  info.m_affine = m_port->handledAffine(info, frame);
}

//-----------------------------------------------------------------------------------

bool PlasticDeformerFx::buildTextureDataSl(double frame, TRenderSettings &info,
                                           TAffine &worldLevelToLevelAff) {
  int row = (int)frame;

  // Initialize level variables
  TLevelColumnFx *lcfx       = (TLevelColumnFx *)m_port.getFx();
  TXshLevelColumn *texColumn = lcfx->getColumn();

  const TXshCell &texCell = texColumn->getCell(row);

  TXshSimpleLevel *texSl = texCell.getSimpleLevel();
  const TFrameId &texFid = texCell.getFrameId();

  if (!texSl || texSl->getType() == MESH_XSHLEVEL) return false;

  // Build DPI data
  TPointD texDpi(texSl->getDpi(texFid, 0));
  if (texDpi.x == 0.0 || texDpi.y == 0.0 || texSl->getType() == PLI_XSHLEVEL)
    texDpi.x = texDpi.y = Stage::inch;

  // Build reference transforms (image coordinates, DPI affine removed during
  // render-tree build)
  worldLevelToLevelAff = TScale(texDpi.x / Stage::inch, texDpi.y / Stage::inch);

  // Initialize input render settings
  // For vector images (PLI), allow full affine for quality
  // For raster, use original reference if magnifying; otherwise apply scale to
  // avoid crude minification
  const TAffine &handledAff = TRasterFx::handledAffine(info, frame);

  if (texSl->getType() == PLI_XSHLEVEL) {
    info.m_affine = handledAff;
    buildRenderSettings(frame, info);
  } else {
    info.m_affine = TAffine();
    buildRenderSettings(frame, info);

    // Apply scale if handled affine is minifying (scale < 1.0)
    if (handledAff.a11 < worldLevelToLevelAff.a11)
      info.m_affine =
          TScale(handledAff.a11 / worldLevelToLevelAff.a11) * info.m_affine;
  }

  return true;
}

//-----------------------------------------------------------------------------------

bool PlasticDeformerFx::buildTextureData(double frame, TRenderSettings &info,
                                         TAffine &worldLevelToLevelAff) {
  // Common case (e.g., sub-XSheets): adjust info and match reference
  buildRenderSettings(frame, info);
  worldLevelToLevelAff = TAffine();

  return true;
}

//-----------------------------------------------------------------------------------

void PlasticDeformerFx::doCompute(TTile &tile, double frame,
                                  const TRenderSettings &info) {
  if (!m_port.isConnected()) {
    tile.getRaster()->clear();
    return;
  }

  int row = (int)frame;

  // Build texture data
  TRenderSettings texInfo(info);
  TAffine worldTexLevelToTexLevelAff;

  if (dynamic_cast<TLevelColumnFx *>(m_port.getFx())) {
    if (!buildTextureDataSl(frame, texInfo, worldTexLevelToTexLevelAff)) return;
  } else {
    buildTextureData(frame, texInfo, worldTexLevelToTexLevelAff);
  }

  // Initialize mesh level variables
  const TXshCell &meshCell = m_xsh->getCell(row, m_col);

  TXshSimpleLevel *meshSl = meshCell.getSimpleLevel();
  const TFrameId &meshFid = meshCell.getFrameId();

  if (!meshSl || meshSl->getType() != MESH_XSHLEVEL) return;

  // Retrieve mesh image and deformation
  TStageObject *meshColumnObj =
      m_xsh->getStageObject(TStageObjectId::ColumnId(m_col));

  TMeshImageP mi(meshSl->getFrame(meshFid, false));
  if (!mi) return;

  // Retrieve deformation data
  const PlasticSkeletonDeformationP &sd =
      meshColumnObj->getPlasticSkeletonDeformation();
  assert(sd);

  double sdFrame = meshColumnObj->paramsTime(frame);

  // Build DPI data
  TPointD meshDpi(meshSl->getDpi(meshFid, 0));
  assert(meshDpi.x != 0.0 && meshDpi.y != 0.0);

  // Build reference transforms
  const TAffine &imageToTextureAff           = texInfo.m_affine;
  const TAffine &worldTexLevelToWorldMeshAff = m_texPlacement;
  const TAffine &meshToWorldMeshAff =
      TScale(Stage::inch / meshDpi.x, Stage::inch / meshDpi.y);

  const TAffine &meshToTexLevelAff = worldTexLevelToTexLevelAff *
                                     worldTexLevelToWorldMeshAff.inv() *
                                     meshToWorldMeshAff;
  const TAffine &meshToTextureAff = imageToTextureAff * meshToTexLevelAff;

  // Retrieve deformer data
  TScale worldMeshToMeshAff(meshDpi.x / Stage::inch, meshDpi.y / Stage::inch);

  std::unique_ptr<const PlasticDeformerDataGroup> dataGroup(
      PlasticDeformerStorage::instance()->processOnce(
          sdFrame, mi.getPointer(), sd.getPointer(), sd->skeletonId(sdFrame),
          worldMeshToMeshAff));

  // Build the mesh's bounding box and map it to input reference
  TRectD meshBBox(meshToTextureAff * mi->getBBox());

  // Build the tile's geometry
  TRectD texBBox;
  m_port->getBBox(frame, texBBox, texInfo);

  // Compute union of texBBox and meshBBox properly
  TRectD bbox;
  bbox.x0 = std::min(texBBox.x0, meshBBox.x0);
  bbox.y0 = std::min(texBBox.y0, meshBBox.y0);
  bbox.x1 = std::max(texBBox.x1, meshBBox.x1);
  bbox.y1 = std::max(texBBox.y1, meshBBox.y1);

  // If the resulting bbox is empty, skip rendering
  if (bbox.getLx() <= 0.0 || bbox.getLy() <= 0.0) return;

  // Align to pixel boundaries
  bbox.x0 = tfloor(bbox.x0);
  bbox.y0 = tfloor(bbox.y0);
  bbox.x1 = tceil(bbox.x1);
  bbox.y1 = tceil(bbox.y1);

  TDimension tileSize(tround(bbox.getLx()), tround(bbox.getLy()));

  // Compute the input image
  TTile inTile;
  m_port->allocateAndCompute(inTile, bbox.getP00(), tileSize, TRasterP(), frame,
                             texInfo);

  // Draw the textured mesh using RAII for resource management
  {
    // Prepare texture (depremultiply for OpenGL texture upload)
    TRaster32P tex(inTile.getRaster());
    TRop::depremultiply(tex);
    static TAtomicVar var;
    const std::string &texId = "render_tex " + std::to_string(++var);

    OpenGLContextRAIIS contextRAII;
    if (!contextRAII.initialize(info.m_offScreenSurface.get())) {
      TSysLog::error("PlasticDeformerFx: Failed to initialize OpenGL context");
      tile.getRaster()->clear();  // Fallback
      return;
    }

    TDimension d = tile.getRaster()->getSize();
    QOpenGLFramebufferObject fb(d.lx, d.ly);

    if (!fb.isValid()) {
      TSysLog::error("PlasticDeformerFx: Failed to create FBO");
      tile.getRaster()->clear();  // Fallback
      return;
    }

    // Save current OpenGL state
    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    GLboolean prevBlend   = glIsEnabled(GL_BLEND);
    GLboolean prevTexture = glIsEnabled(GL_TEXTURE_2D);

    GLint prevMatrixMode;
    glGetIntegerv(GL_MATRIX_MODE, &prevMatrixMode);

    fb.bind();

    // Check FBO status after binding using Qt's method
    // Note: QOpenGLFramebufferObject already validates during creation
    // We'll rely on fb.isValid() and check for GL errors instead

    TTexturesStorage *ts = TTexturesStorage::instance();
    try {
      const DrawableTextureDataP &texData = ts->loadTexture(texId, tex, bbox);
      if (!texData) {
        throw std::runtime_error("Failed to load texture data");
      }

      // OpenGL draw setup
      glViewport(0, 0, d.lx, d.ly);
      glClearColor(0, 0, 0, 0);
      glClear(GL_COLOR_BUFFER_BIT);

      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      gluOrtho2D(0, d.lx, 0, d.ly);

      glMatrixMode(GL_MODELVIEW);
      glLoadIdentity();
      tglMultMatrix(TTranslation(-tile.m_pos) * info.m_affine *
                    meshToWorldMeshAff);

      glEnable(GL_BLEND);
      glEnable(GL_TEXTURE_2D);

      tglDraw(*mi, *texData, meshToTextureAff, *dataGroup);

      fb.release();
      glFlush();
      glFinish();  // Ensure draw completes before readback

      // Restore OpenGL state
      glViewport(prevViewport[0], prevViewport[1], prevViewport[2],
                 prevViewport[3]);
      if (prevBlend)
        glEnable(GL_BLEND);
      else
        glDisable(GL_BLEND);
      if (prevTexture)
        glEnable(GL_TEXTURE_2D);
      else
        glDisable(GL_TEXTURE_2D);
      glMatrixMode(prevMatrixMode);

      // Retrieve and copy pixels using safe function
      QImage img =
          fb.toImage().mirrored(false, true);  // Flip Y in single operation
      if (img.isNull()) {
        throw std::runtime_error("Failed to read FBO image");
      }

      // Use safe pixel copy with bounds checking
      safePixelCopy(tile.getRaster(), img, m_was64bit);

      // Unload texture to prevent memory leaks
      ts->unloadTexture(texId);

    } catch (const std::exception &e) {
      fb.release();

      // Restore OpenGL state on error
      glViewport(prevViewport[0], prevViewport[1], prevViewport[2],
                 prevViewport[3]);
      if (prevBlend)
        glEnable(GL_BLEND);
      else
        glDisable(GL_BLEND);
      if (prevTexture)
        glEnable(GL_TEXTURE_2D);
      else
        glDisable(GL_TEXTURE_2D);
      glMatrixMode(prevMatrixMode);

      ts->unloadTexture(texId);
      TSysLog::error(std::string("PlasticDeformerFx: OpenGL draw error - ") +
                     e.what());
      tile.getRaster()->clear();  // Fallback on error
      return;                     // Don't rethrow in production code
    } catch (...) {
      fb.release();

      // Restore OpenGL state on error
      glViewport(prevViewport[0], prevViewport[1], prevViewport[2],
                 prevViewport[3]);
      if (prevBlend)
        glEnable(GL_BLEND);
      else
        glDisable(GL_BLEND);
      if (prevTexture)
        glEnable(GL_TEXTURE_2D);
      else
        glDisable(GL_TEXTURE_2D);
      glMatrixMode(prevMatrixMode);

      ts->unloadTexture(texId);
      TSysLog::error("PlasticDeformerFx: Unknown OpenGL error during draw");
      tile.getRaster()->clear();  // Fallback on error
      return;                     // Don't rethrow in production code
    }

    // Check and log any lingering OpenGL errors
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
      TSysLog::warning("PlasticDeformerFx: GL error after draw - " +
                       std::to_string(err));
    }
  }
}

//-----------------------------------------------------------------------------------

void PlasticDeformerFx::doDryCompute(TRectD &rect, double frame,
                                     const TRenderSettings &info) {
  if (!m_port.isConnected()) return;

  int row = (int)frame;

  TRenderSettings texInfo(info);
  TAffine worldTexLevelToTexLevelAff;

  if (dynamic_cast<TLevelColumnFx *>(m_port.getFx())) {
    if (!buildTextureDataSl(frame, texInfo, worldTexLevelToTexLevelAff)) return;
  } else {
    buildTextureData(frame, texInfo, worldTexLevelToTexLevelAff);
  }

  const TXshCell &meshCell = m_xsh->getCell(row, m_col);

  TXshSimpleLevel *meshSl = meshCell.getSimpleLevel();
  const TFrameId &meshFid = meshCell.getFrameId();

  if (!meshSl || meshSl->getType() != MESH_XSHLEVEL) return;

  TStageObject *meshColumnObj =
      m_xsh->getStageObject(TStageObjectId::ColumnId(m_col));

  TMeshImageP mi(meshSl->getFrame(meshFid, false));
  if (!mi) return;

  const PlasticSkeletonDeformationP &sd =
      meshColumnObj->getPlasticSkeletonDeformation();
  assert(sd);

  TPointD meshDpi(meshSl->getDpi(meshFid, 0));
  assert(meshDpi.x != 0.0 && meshDpi.y != 0.0);

  const TAffine &textureToImageAff        = texInfo.m_affine;
  const TAffine &worldImageToWorldMeshAff = m_texPlacement;
  const TAffine &meshToWorldMeshAff =
      TScale(Stage::inch / meshDpi.x, Stage::inch / meshDpi.y);

  const TAffine &meshToTextureAff =
      textureToImageAff.inv() * worldTexLevelToTexLevelAff *
      worldImageToWorldMeshAff.inv() * meshToWorldMeshAff;

  // Build the mesh's bounding box and map it to input reference
  TRectD meshBBox(meshToTextureAff * mi->getBBox());

  // Build the tile's geometry
  TRectD texBBox;
  m_port->getBBox(frame, texBBox, texInfo);

  // Compute union of texBBox and meshBBox properly (mirrored from doCompute)
  TRectD bbox;
  bbox.x0 = std::min(texBBox.x0, meshBBox.x0);
  bbox.y0 = std::min(texBBox.y0, meshBBox.y0);
  bbox.x1 = std::max(texBBox.x1, meshBBox.x1);
  bbox.y1 = std::max(texBBox.y1, meshBBox.y1);

  // If the resulting bbox is empty, skip rendering
  if (bbox.getLx() <= 0.0 || bbox.getLy() <= 0.0) return;

  // Align to pixel boundaries
  bbox.x0 = tfloor(bbox.x0);
  bbox.y0 = tfloor(bbox.y0);
  bbox.x1 = tceil(bbox.x1);
  bbox.y1 = tceil(bbox.y1);

  m_port->dryCompute(bbox, frame, texInfo);
}
