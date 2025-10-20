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

}  // namespace

//***************************************************************************************************
//    RAII OpenGL Context Management
//***************************************************************************************************

class OpenGLContextRAII {
private:
  QOpenGLContext *m_context;
  bool m_isCurrent;

public:
  OpenGLContextRAII() : m_context(new QOpenGLContext()), m_isCurrent(false) {}

  ~OpenGLContextRAII() {
    if (m_context && m_isCurrent) {
      m_context->doneCurrent();
    }
    if (m_context) {
      m_context->moveToThread(
          0);  // Ensure thread-safe deletion for multi-threaded rendering
      delete m_context;
    }
  }

  // Disable copy constructor and assignment to prevent resource sharing issues
  OpenGLContextRAII(const OpenGLContextRAII &)            = delete;
  OpenGLContextRAII &operator=(const OpenGLContextRAII &) = delete;

  bool initialize(QOffscreenSurface *surface) {
    if (!surface || !m_context) return false;

    // Tenta reutilizar ATÉ SE EXISTIR UM CURRENT (mesmo se não for o ideal)
    QOpenGLContext *existing = QOpenGLContext::currentContext();
    if (existing && existing->isValid()) {
      m_context   = existing;  // Reusa o atual (compartilhado implicitamente)
      m_isCurrent = m_context->makeCurrent(surface);
      return m_isCurrent;
    }

    // Share context if one is already current (e.g., for multi-thread
    // compatibility)
    if (QOpenGLContext::currentContext()) {
      m_context->setShareContext(QOpenGLContext::currentContext());
    }

    m_context->setFormat(QSurfaceFormat::defaultFormat());

    if (!m_context->create()) {
      return false;
    }

    m_isCurrent = m_context->makeCurrent(surface);
    return m_isCurrent;
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

    OpenGLContextRAII contextRAII;
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

    fb.bind();

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

      // Retrieve and copy pixels (optimized: manual copy with flip and direct
      // convert)
      QImage img =
          fb.toImage().mirrored(false, true);  // Flip Y in single operation
      if (img.isNull()) {
        throw std::runtime_error("Failed to read FBO image");
      }

      int wrap              = d.lx * sizeof(TPixel32);
      TRasterP targetRaster = tile.getRaster();

      if (m_was64bit) {
        // 64-bit: Copy to temp 32-bit raster, convert, then copy to 64-bit
        // target
        TRaster64P convertedRaster(d);
        TRaster32P tempRaster(d);
        uchar *srcPix = img.bits();
        uchar *dstPix = tempRaster->getRawData();
        for (int y = 0; y < d.ly; ++y) {
          memcpy(dstPix + y * wrap, srcPix + y * wrap, wrap);
        }
        TRop::convert(convertedRaster, tempRaster);
        // Copy to tile (original is 64-bit allocated)
        memcpy(targetRaster->getRawData(), convertedRaster->getRawData(),
               convertedRaster->getLx() * convertedRaster->getLy() *
                   sizeof(TPixel64));
      } else {
        // 32-bit: direct copy (no flip needed after mirrored)
        uchar *srcPix = img.bits();
        uchar *dstPix = targetRaster->getRawData();
        for (int y = 0; y < d.ly; ++y) {
          memcpy(dstPix + y * wrap, srcPix + y * wrap, wrap);
        }
      }

      // Unload texture to prevent memory leaks
      ts->unloadTexture(texId);

    } catch (const std::exception &e) {
      fb.release();
      ts->unloadTexture(texId);
      TSysLog::error(std::string("PlasticDeformerFx: OpenGL draw error - ") +
                     e.what());
      tile.getRaster()->clear();  // Fallback on error
      throw;
    } catch (...) {
      fb.release();
      ts->unloadTexture(texId);
      TSysLog::error("PlasticDeformerFx: Unknown OpenGL error during draw");
      tile.getRaster()->clear();  // Fallback on error
      throw;
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
