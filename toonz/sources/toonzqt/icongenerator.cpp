

// TnzCore includes
#include "tfilepath.h"
#include "tfiletype.h"
#include "tstream.h"
#include "tsystem.h"
#include "timagecache.h"
#include "tpixelutils.h"
#include "tropcm.h"
#include "timageinfo.h"
#include "timage_io.h"
#include "tlevel_io.h"
#include "tofflinegl.h"
#include "tgl.h"
#include "tvectorrenderdata.h"
#include "tstroke.h"
#include "tthreadmessage.h"
#include "tpalette.h"
#include "trasterimage.h"
#include "tvectorimage.h"
#include "ttoonzimage.h"
#include "tmeshimage.h"

// TnzExt includes
#include "ext/meshutils.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/txsheet.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshlevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshchildlevel.h"
#include "toonz/tstageobjectspline.h"
#include "toonz/preferences.h"
#include "toonz/sceneresources.h"
#include "toonz/stage2.h"

// TnzQt includes
#include "toonzqt/gutil.h"

#include "toonzqt/icongenerator.h"

//=============================================================================

//===================================
//
//    Local namespace
//
//-----------------------------------

namespace {
const TDimension IconSize(80, 60);
TDimension FilmstripIconSize(0, 0);

// Access name-based storage
std::set<std::string> iconsMap;
QMutex iconsMapMutex;  // ADDED: Mutex for thread safety

typedef std::set<std::string>::iterator IconIterator;

//-----------------------------------------------------------------------------

// Returns true if the image request was already submitted.
bool getIcon(const std::string &iconName, QPixmap &pix,
             TXshSimpleLevel *simpleLevel = 0,
             TDimension standardSize      = TDimension(0, 0)) {
  QMutexLocker locker(&iconsMapMutex);  // ADDED: Thread safety
  IconIterator it;
  it = iconsMap.find(iconName);

  if (it != iconsMap.end()) {
    TImageP im;
    try {
      im = TImageCache::instance()->get(iconName, false);
    } catch (...) {
      return false;
    }

    if (!im) return false;

    TToonzImage *timgp = dynamic_cast<TToonzImage *>(im.getPointer());

    if (simpleLevel && timgp) {
      IconGenerator::Settings settings =
          IconGenerator::instance()->getSettings();

      TRaster32P icon(timgp->getSize());
      if (!icon || !icon->getRawData()) return false;

      icon->clear();
      icon->fill((settings.m_blackBgCheck) ? TPixel::Black : TPixel::White);
      if (settings.m_transparencyCheck || settings.m_inkIndex != -1 ||
          settings.m_paintIndex != -1) {
        TRop::CmappedQuickputSettings s;
        s.m_globalColorScale  = TPixel32::Black;
        s.m_inksOnly          = false;
        s.m_transparencyCheck = settings.m_transparencyCheck;
        s.m_blackBgCheck      = settings.m_blackBgCheck;
        s.m_inkIndex          = settings.m_inkIndex;
        s.m_paintIndex        = settings.m_paintIndex;
        Preferences::instance()->getTranspCheckData(
            s.m_transpCheckBg, s.m_transpCheckInk, s.m_transpCheckPaint);

        TRop::quickPut(icon, timgp->getRaster(), simpleLevel->getPalette(),
                       TAffine(), s);
      } else
        TRop::quickPut(icon, timgp->getRaster(), simpleLevel->getPalette(),
                       TAffine());
      pix = rasterToQPixmap(icon, false);
      return true;
    }
    TRasterImageP img = static_cast<TRasterImageP>(im);

    if (!img) {
      pix = QPixmap();
      return true;
    }

    TRasterP ras = img->getRaster();
    if (!ras || !ras->getRawData()) {
      pix = QPixmap();
      return true;
    }

    assert(!(TRasterGR8P)img->getRaster());
    const TRaster32P &ras32 = img->getRaster();
    bool isHighDpi          = false;
    if (standardSize != TDimension(0, 0) &&
        ras32->getSize().lx > standardSize.lx &&
        ras32->getSize().ly > standardSize.ly)
      isHighDpi = true;
    pix = rasterToQPixmap(ras32, false, isHighDpi);
    return true;
  }

  return false;
}

//-----------------------------------------------------------------------------

void setIcon(const std::string &iconName, const TRaster32P &icon) {
  if (iconName.empty()) return;

  QMutexLocker locker(&iconsMapMutex);  // ADDED: Thread safety

  if (iconsMap.find(iconName) != iconsMap.end()) {
    // ADDED: Validate raster before caching
    if (!icon || !icon->getRawData() || icon->getLx() <= 0 ||
        icon->getLy() <= 0) {
      return;
    }

    try {
      TImageCache::instance()->add(iconName, TRasterImageP(icon), true);
    } catch (...) {
      // Ignore cache errors
    }
  }
}

//-----------------------------------------------------------------------------
/*! Cache icon data in TToonzImage format if ToonzImageIconRenderer generates
 * them
 */
void setIcon_TnzImg(const std::string &iconName, const TRasterCM32P &icon) {
  if (iconName.empty()) return;

  QMutexLocker locker(&iconsMapMutex);  // ADDED: Thread safety

  if (iconsMap.find(iconName) != iconsMap.end()) {
    // ADDED: Validate raster before caching
    if (!icon || !icon->getRawData() || icon->getLx() <= 0 ||
        icon->getLy() <= 0) {
      return;
    }

    try {
      TImageCache::instance()->add(
          iconName, TToonzImageP(icon, TRect(icon->getSize())), true);
    } catch (...) {
      // Ignore cache errors
    }
  }
}

//-----------------------------------------------------------------------------

void removeIcon(const std::string &iconName) {
  QMutexLocker locker(&iconsMapMutex);  // ADDED: Thread safety

  IconIterator it;
  it = iconsMap.find(iconName);
  if (it != iconsMap.end()) {
    try {
      TImageCache::instance()->remove(iconName);
    } catch (...) {
      // Ignore removal errors
    }
  }
  iconsMap.erase(iconName);
}

//-----------------------------------------------------------------------------

bool isUnpremultiplied(const TRaster32P &r) {
  if (!r || !r->getRawData()) return false;

  int lx = r->getLx();
  int y  = r->getLy();
  r->lock();
  while (--y >= 0) {
    TPixel32 *pix    = r->pixels(y);
    TPixel32 *endPix = pix + lx;
    while (pix < endPix) {
      if (pix->r > pix->m || pix->g > pix->m || pix->b > pix->m) {
        r->unlock();
        return true;
      }
      ++pix;
    }
  }
  r->unlock();
  return false;
}

//-----------------------------------------------------------------------------

void makeChessBackground(const TRaster32P &ras) {
  if (!ras || !ras->getRawData()) return;

  ras->lock();

  const TPixel32 gray1(230, 230, 230, 255), gray2(180, 180, 180, 255);

  int lx = ras->getLx(), ly = ras->getLy();
  for (int y = 0; y != ly; ++y) {
    TPixel32 *pix = ras->pixels(y), *lineEnd = pix + lx;

    int yCol = (y & 4);

    for (int x = 0; pix != lineEnd; ++x, ++pix)
      if (pix->m != 255) *pix = overPix((x & 4) == yCol ? gray1 : gray2, *pix);
  }

  ras->unlock();
}

}  // namespace

//=============================================================================

//==========================================
//
//    Image-to-Icon conversion methods
//
//------------------------------------------

namespace {
TRaster32P convertToIcon(TVectorImageP vimage, int frame,
                         const TDimension &iconSize,
                         const IconGenerator::Settings &settings) {
  if (!vimage) return TRaster32P();

  // ADDED: Validate vector image data
  if (vimage->getStrokeCount() == 0 && vimage->getRegionCount() == 0) {
    return TRaster32P();  // Empty image
  }

  TPalette *plt = vimage->getPalette();
  if (!plt) return TRaster32P();

  // ADDED: Use smart pointer for automatic cleanup
  std::unique_ptr<TPalette> pltGuard(plt->clone());
  if (!pltGuard) return TRaster32P();
  pltGuard->setFrame(frame);

  TOfflineGL *glContext = IconGenerator::instance()->getOfflineGLContext();
  if (!glContext) return TRaster32P();

  // The image and contained within Imagebox
  // (add a small margin also to prevent problems with empty images)
  TRectD imageBox;
  {
    QMutexLocker sl(vimage->getMutex());
    imageBox = vimage->getBBox().enlarge(.1);
  }

  // ADDED: Check for valid bounding box
  if (imageBox.getLx() <= 0 || imageBox.getLy() <= 0) {
    return TRaster32P();
  }

  TPointD imageCenter = (imageBox.getP00() + imageBox.getP11()) * 0.5;

  // Calculate a transformation matrix that moves the image inside the icon.
  // The scale factor is chosen so that the image is entirely
  // contained in the icon (with a margin of 'margin' pixels)
  const int margin = 10;
  double scx       = (iconSize.lx - margin) / imageBox.getLx();
  double scy       = (iconSize.ly - margin) / imageBox.getLy();
  double sc        = std::min(scx, scy);
  // Add the translation: the center point of the image is at the point
  // middle of the pixmap.
  TPointD iconCenter(iconSize.lx * 0.5, iconSize.ly * 0.5);
  TAffine aff = TScale(sc).place(imageCenter, iconCenter);

  // RenderData
  TVectorRenderData rd(aff, TRect(iconSize), pltGuard.get(), 0, true);

  rd.m_tcheckEnabled     = settings.m_transparencyCheck;
  rd.m_blackBgEnabled    = settings.m_blackBgCheck;
  rd.m_drawRegions       = !settings.m_inksOnly;
  rd.m_inkCheckEnabled   = settings.m_inkIndex != -1;
  rd.m_paintCheckEnabled = settings.m_paintIndex != -1;
  rd.m_colorCheckIndex =
      rd.m_inkCheckEnabled ? settings.m_inkIndex : settings.m_paintIndex;
  rd.m_isIcon = true;

  // Draw the image.
  try {
    glContext->makeCurrent();
    glContext->clear(rd.m_blackBgEnabled ? TPixel::Black : TPixel32::White);
    glContext->draw(vimage, rd);

    TRaster32P ras(iconSize);
    if (!ras || !ras->getRawData()) {
      glContext->doneCurrent();
      return TRaster32P();
    }

    glContext->getRaster(ras);
    glContext->doneCurrent();

    return ras;
  } catch (...) {
    glContext->doneCurrent();
    return TRaster32P();
  }
}

//-------------------------------------------------------------------------

TRaster32P convertToIcon(TToonzImageP timage, int frame,
                         const TDimension &iconSize,
                         const IconGenerator::Settings &settings) {
  if (!timage) return TRaster32P();

  TPalette *plt = timage->getPalette();
  if (!plt) return TRaster32P();

  plt->setFrame(frame);

  TRasterCM32P rasCM32 = timage->getRaster();
  if (!rasCM32.getPointer() || !rasCM32->getRawData()) return TRaster32P();

  int lx     = rasCM32->getSize().lx;
  int ly     = rasCM32->getSize().ly;
  int iconLx = iconSize.lx, iconLy = iconSize.ly;
  if (std::max(double(lx) / iconSize.lx, double(ly) / iconSize.ly) ==
      double(ly) / iconSize.ly)
    iconLx = tround((double(lx) * iconSize.ly) / ly);
  else
    iconLy = tround((double(ly) * iconSize.lx) / lx);

  // icon size with correct aspect ratio
  TDimension iconSize2 = TDimension(iconLx, iconLy);

  TRaster32P icon(iconSize2);
  if (!icon || !icon->getRawData()) return TRaster32P();

  icon->clear();
  icon->fill(settings.m_blackBgCheck ? TPixel::Black : TPixel::White);

  TDimension dim = rasCM32->getSize();
  if (dim != iconSize2) {
    TRasterCM32P auxCM32(icon->getSize());
    if (!auxCM32 || !auxCM32->getRawData()) return TRaster32P();

    auxCM32->clear();
    TRop::makeIcon(auxCM32, rasCM32);
    rasCM32 = auxCM32;
  }

  if (settings.m_transparencyCheck || settings.m_inksOnly ||
      settings.m_inkIndex != -1 || settings.m_paintIndex != -1) {
    TRop::CmappedQuickputSettings s;
    s.m_globalColorScale  = TPixel32::Black;
    s.m_inksOnly          = settings.m_inksOnly;
    s.m_transparencyCheck = settings.m_transparencyCheck;
    s.m_blackBgCheck      = settings.m_blackBgCheck;
    s.m_inkIndex          = settings.m_inkIndex;
    s.m_paintIndex        = settings.m_paintIndex;
    Preferences::instance()->getTranspCheckData(
        s.m_transpCheckBg, s.m_transpCheckInk, s.m_transpCheckPaint);

    TRop::quickPut(icon, rasCM32, timage->getPalette(), TAffine(), s);
  } else
    TRop::quickPut(icon, rasCM32, timage->getPalette(), TAffine());

  assert(iconSize2.lx <= iconSize.lx && iconSize2.ly <= iconSize.ly);
  TRaster32P outIcon(iconSize);
  if (!outIcon || !outIcon->getRawData()) return TRaster32P();

  outIcon->clear();
  int dx = (outIcon->getLx() - icon->getLx()) / 2;
  int dy = (outIcon->getLy() - icon->getLy()) / 2;
  assert(dx >= 0 && dy >= 0);
  TRect box = outIcon->getBounds().enlarge(-dx, -dy);
  TRop::copy(outIcon->extract(box), icon);

  return outIcon;
}

//-------------------------------------------------------------------------

TRaster32P convertToIcon(TRasterImageP rimage, const TDimension &iconSize) {
  if (!rimage) return TRaster32P();

  TRasterP ras = rimage->getRaster();
  if (!ras || !ras->getRawData()) return TRaster32P();

  if (!(TRaster32P)ras && !(TRasterGR8P)ras) return TRaster32P();

  if (ras->getSize() == iconSize) return ras;

  TRaster32P icon(iconSize);
  if (!icon || !icon->getRawData()) return TRaster32P();

  icon->fill(TPixel32(235, 235, 235));

  double sx = (double)icon->getLx() / ras->getLx();
  double sy = (double)icon->getLy() / ras->getLy();
  double sc = sx < sy ? sx : sy;

  TAffine aff = TScale(sc).place(ras->getCenterD(), icon->getCenterD());
  TRop::resample(icon, ras, aff, TRop::Bilinear);
  TRop::addBackground(icon, TPixel32::White);

  return icon;
}

//-------------------------------------------------------------------------

TRaster32P convertToIcon(TMeshImageP mi, int frame, const TDimension &iconSize,
                         const IconGenerator::Settings &settings) {
  if (!mi) return TRaster32P();

  TOfflineGL *glContext = IconGenerator::instance()->getOfflineGLContext();
  if (!glContext) return TRaster32P();

  // The image and contained within Imagebox
  // (add a small margin also to prevent problems with empty images)
  TRectD imageBox;
  imageBox = mi->getBBox().enlarge(.1);

  // ADDED: Check for valid bounding box
  if (imageBox.getLx() <= 0 || imageBox.getLy() <= 0) {
    return TRaster32P();
  }

  TPointD imageCenter(0.5 * (imageBox.getP00() + imageBox.getP11()));

  // Calculate a transformation matrix that moves the image inside the icon.
  // The scale factor is chosen so that the image is entirely
  // contained in the icon (with a margin of 'margin' pixels)
  const int margin = 10;
  double scx       = (iconSize.lx - margin) / imageBox.getLx();
  double scy       = (iconSize.ly - margin) / imageBox.getLy();
  double sc        = std::min(scx, scy);

  // Add the translation: the center point of the image is at the point
  // middle of the pixmap.
  TPointD iconCenter(iconSize.lx * 0.5, iconSize.ly * 0.5);
  TAffine aff = TScale(sc).place(imageCenter, iconCenter);

  // Draw the image.
  try {
    glContext->makeCurrent();
    glContext->clear(settings.m_blackBgCheck ? TPixel::Black : TPixel32::White);

    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glEnable(GL_LINE_SMOOTH);

    glPushMatrix();
    tglMultMatrix(aff);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(0.0f, 1.0f, 0.0f, 0.7f);
    tglDrawEdges(*mi);

    glPopMatrix();

    glPopAttrib();

    TRaster32P ras(iconSize);
    if (!ras || !ras->getRawData()) {
      glContext->doneCurrent();
      return TRaster32P();
    }

    glContext->getRaster(ras);
    glContext->doneCurrent();

    return ras;
  } catch (...) {
    glContext->doneCurrent();
    return TRaster32P();
  }
}

//-------------------------------------------------------------------------

TRaster32P convertToIcon(TImageP image, int frame, const TDimension &iconSize,
                         const IconGenerator::Settings &settings) {
  if (!image) return TRaster32P();

  TRasterImageP ri(image);
  if (ri) return convertToIcon(ri, iconSize);

  TToonzImageP ti(image);
  if (ti) return convertToIcon(ti, frame, iconSize, settings);

  TVectorImageP vi(image);
  if (vi) return convertToIcon(vi, frame, iconSize, settings);

  TMeshImageP mi(image);
  if (mi) return convertToIcon(mi, frame, iconSize, settings);

  return TRaster32P();
}

}  // namespace

//=============================================================================

//============================
//
//    IconRenderer class
//
//----------------------------

class IconRenderer : public TThread::Runnable {
  TRaster32P m_icon;
  TDimension m_iconSize;
  std::string m_id;

  bool m_started;
  bool m_terminated;

public:
  IconRenderer(const std::string &id, const TDimension &iconSize);
  virtual ~IconRenderer();

  void run() override = 0;

  void setIcon(const TRaster32P &icon) { m_icon = icon; }
  TRaster32P getIcon() const { return m_icon; }

  TDimension getIconSize() { return m_iconSize; }
  const std::string &getId() const { return m_id; }

  bool &hasStarted() { return m_started; }
  bool &wasTerminated() { return m_terminated; }
};

//-----------------------------------------------------------------------------

IconRenderer::IconRenderer(const std::string &id, const TDimension &iconSize)
    : m_icon()
    , m_iconSize(iconSize)
    , m_id(id)
    , m_started(false)
    , m_terminated(false) {
  connect(this, SIGNAL(started(TThread::RunnableP)), IconGenerator::instance(),
          SLOT(onStarted(TThread::RunnableP)));
  connect(this, SIGNAL(finished(TThread::RunnableP)), IconGenerator::instance(),
          SLOT(onFinished(TThread::RunnableP)));
  connect(this, SIGNAL(canceled(TThread::RunnableP)), IconGenerator::instance(),
          SLOT(onCanceled(TThread::RunnableP)), Qt::QueuedConnection);
  connect(this, SIGNAL(terminated(TThread::RunnableP)),
          IconGenerator::instance(), SLOT(onTerminated(TThread::RunnableP)),
          Qt::QueuedConnection);
}

//-----------------------------------------------------------------------------

IconRenderer::~IconRenderer() {}

//=============================================================================

//===================================
//
//    Specific icon renderers
//
//-----------------------------------

//=============================================================================

//======================================
//    NoImageIconRenderer class
//--------------------------------------

class NoImageIconRenderer final : public IconRenderer {
public:
  NoImageIconRenderer(const std::string &id, const TDimension &iconSize)
      : IconRenderer(id, iconSize) {}
  void run() override {
    try {
      TRaster32P ras(getIconSize());
      if (ras && ras->getRawData()) {
        ras->fill(TPixel32::Gray);
        setIcon(ras);
      }
    } catch (...) {
    }
  }
};

//=============================================================================

//======================================
//    VectorImageIconRenderer class
//--------------------------------------

class VectorImageIconRenderer final : public IconRenderer {
  TVectorImageP m_vimage;
  TXshSimpleLevelP m_sl;
  TFrameId m_fid;
  IconGenerator::Settings m_settings;

public:
  VectorImageIconRenderer(const std::string &id, const TDimension &iconSize,
                          TXshSimpleLevelP sl, const TFrameId &fid,
                          const IconGenerator::Settings &settings)
      : IconRenderer(id, iconSize)
      , m_vimage()
      , m_sl(sl)
      , m_fid(fid)
      , m_settings(settings) {}

  VectorImageIconRenderer(const std::string &id, const TDimension &iconSize,
                          TVectorImageP vimage,
                          const IconGenerator::Settings &settings)
      : IconRenderer(id, iconSize)
      , m_vimage(vimage)
      , m_sl(0)
      , m_fid(-1)
      , m_settings(settings) {}

  TRaster32P generateRaster(const TDimension &iconSize) const;
  void run() override;
};

//-----------------------------------------------------------------------------

TRaster32P VectorImageIconRenderer::generateRaster(
    const TDimension &iconSize) const {
  TVectorImageP vimage;

  int frame = 0;
  if (!m_vimage) {
    assert(m_sl);
    if (!m_sl->isFid(m_fid)) return TRaster32P();
    TImageP image = m_sl->getFrameIcon(m_fid);
    if (!image) return TRaster32P();
    vimage = (TVectorImageP)image;
    if (!vimage) return TRaster32P();
    frame = m_sl->guessIndex(m_fid);
  } else
    vimage = m_vimage;
  assert(vimage);

  TRaster32P ras(convertToIcon(vimage, frame, iconSize, m_settings));

  return ras;
}

//-----------------------------------------------------------------------------

void VectorImageIconRenderer::run() {
  try {
    TRaster32P ras(generateRaster(getIconSize()));

    if (ras && ras->getRawData() && ras->getLx() > 0 && ras->getLy() > 0) {
      setIcon(ras);
    }
  } catch (...) {
  }
}

//=============================================================================

//======================================
//    SplineImageIconRenderer class
//--------------------------------------

class SplineIconRenderer final : public IconRenderer {
  TStageObjectSpline *m_spline;

public:
  SplineIconRenderer(const std::string &id, const TDimension &iconSize,
                     TStageObjectSpline *spline)
      : IconRenderer(id, iconSize), m_spline(spline) {}

  TRaster32P generateRaster(const TDimension &iconSize) const;
  void run() override;
};

//-----------------------------------------------------------------------------

TRaster32P SplineIconRenderer::generateRaster(
    const TDimension &iconSize) const {
  // get the glContext
  TOfflineGL *glContext = IconGenerator::instance()->getOfflineGLContext();
  if (!glContext) return TRaster32P();

  try {
    glContext->makeCurrent();
    glContext->clear(TPixel32::White);

    const TStroke *stroke = m_spline->getStroke();
    assert(stroke);
    if (!stroke) {
      glContext->doneCurrent();
      return TRaster32P();
    }
    TRectD sbbox = stroke->getBBox();

    glColor3d(0, 0, 0);
    double scaleX = 1, scaleY = 1;
    if (sbbox.getLx() > 0.0) scaleX = (double)iconSize.lx / sbbox.getLx();
    if (sbbox.getLy() > 0.0) scaleY = (double)iconSize.ly / sbbox.getLy();
    double scale         = 0.8 * std::min(scaleX, scaleY);
    TPointD centerStroke = 0.5 * (sbbox.getP00() + sbbox.getP11());
    TPointD centerPixmap(iconSize.lx * 0.5, iconSize.ly * 0.5);
    glPushMatrix();
    tglMultMatrix(TScale(scale).place(centerStroke, centerPixmap));
    int n = 50;
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < n; i++)
      tglVertex(stroke->getPoint((double)i / (double)(n - 1)));
    glEnd();
    glPopMatrix();

    TRaster32P ras(iconSize);
    if (!ras || !ras->getRawData()) {
      glContext->doneCurrent();
      return TRaster32P();
    }

    glContext->getRaster(ras);
    glContext->doneCurrent();
    return ras;
  } catch (...) {
    glContext->doneCurrent();
    return TRaster32P();
  }
}

//-----------------------------------------------------------------------------

void SplineIconRenderer::run() {
  TRaster32P raster = generateRaster(getIconSize());
  if (raster && raster->getRawData() && raster->getLx() > 0 &&
      raster->getLy() > 0) {
    setIcon(raster);
  }
}

//=============================================================================

//======================================
//    RasterImageIconRenderer class
//--------------------------------------

class RasterImageIconRenderer final : public IconRenderer {
  TXshSimpleLevelP m_sl;
  TFrameId m_fid;

public:
  RasterImageIconRenderer(const std::string &id, const TDimension &iconSize,
                          TXshSimpleLevelP sl, const TFrameId &fid)
      : IconRenderer(id, iconSize), m_sl(sl), m_fid(fid) {}

  void run() override;
};

//-----------------------------------------------------------------------------

void RasterImageIconRenderer::run() {
  if (!m_sl->isFid(m_fid)) return;

  TImageP image = m_sl->getFrameIcon(m_fid);
  if (!image) return;

  TRasterImageP rimage = (TRasterImageP)image;
  assert(rimage);

  TRaster32P icon(convertToIcon(rimage, getIconSize()));

  if (icon && icon->getRawData() && icon->getLx() > 0 && icon->getLy() > 0) {
    setIcon(icon);
  }
}

//=============================================================================

//======================================
//    ToonzImageIconRenderer class
//--------------------------------------

class ToonzImageIconRenderer final : public IconRenderer {
  TXshSimpleLevelP m_sl;
  TFrameId m_fid;
  IconGenerator::Settings m_settings;

  TRasterCM32P m_tnzImgIcon;

public:
  ToonzImageIconRenderer(const std::string &id, const TDimension &iconSize,
                         TXshSimpleLevelP sl, const TFrameId &fid,
                         const IconGenerator::Settings &settings)
      : IconRenderer(id, iconSize)
      , m_sl(sl)
      , m_fid(fid)
      , m_settings(settings)
      , m_tnzImgIcon(0) {}

  void run() override;

  void setIcon_TnzImg(const TRasterCM32P &timgp) { m_tnzImgIcon = timgp; }
  TRasterCM32P getIcon_TnzImg() const { return m_tnzImgIcon; }
};

//-----------------------------------------------------------------------------

void ToonzImageIconRenderer::run() {
  if (!m_sl->isFid(m_fid)) return;

  TImageP image = m_sl->getFrameIcon(m_fid);
  if (!image) return;

  TRasterImageP rimage(image);
  if (rimage) {
    TRaster32P icon(convertToIcon(rimage, getIconSize()));
    if (icon && icon->getRawData() && icon->getLx() > 0 && icon->getLy() > 0) {
      setIcon(icon);
    }
    return;
  }

  TToonzImageP timage = (TToonzImageP)image;

  TDimension iconSize(getIconSize());
  if (!timage) {
    TRaster32P p(iconSize.lx, iconSize.ly);
    if (p && p->getRawData()) {
      p->fill(TPixelRGBM32::Yellow);
      setIcon(p);
    }
    return;
  }

  TRasterCM32P rasCM32 = timage->getRaster();
  if (!rasCM32.getPointer() || !rasCM32->getRawData()) return;

  int lx     = rasCM32->getSize().lx;
  int ly     = rasCM32->getSize().ly;
  int iconLx = iconSize.lx, iconLy = iconSize.ly;

  TRaster32P icon(iconSize);
  if (!icon || !icon->getRawData()) return;

  icon->fill(m_settings.m_blackBgCheck ? TPixel::Black : TPixel::White);

  if (lx != iconLx && ly != iconLy) {
    // The icons stored in the tlv file don't have the required size.
    // Fetch the original and iconize it.

    image = m_sl->getFrame(m_fid, ImageManager::dontPutInCache,
                           0);  // 0 uses the level properties' subsampling
    if (!image) return;

    timage = (TToonzImageP)image;
    if (!timage) {
      TRaster32P p(iconSize.lx, iconSize.ly);
      if (p && p->getRawData()) {
        p->fill(TPixelRGBM32::Yellow);
        setIcon(p);
      }
      return;
    }

    rasCM32 = timage->getRaster();
    if (!rasCM32.getPointer() || !rasCM32->getRawData()) return;

    TRasterCM32P auxCM32(icon->getSize());
    if (!auxCM32 || !auxCM32->getRawData()) return;

    auxCM32->clear();
    TRop::makeIcon(auxCM32, rasCM32);
    rasCM32 = auxCM32;
  }

  if (!m_sl->getPalette()) return;

  TPaletteP plt = m_sl->getPalette()->clone();
  if (!plt) return;

  int frame = m_sl->guessIndex(m_fid);
  plt->setFrame(frame);

  setIcon_TnzImg(rasCM32);
}

//=============================================================================

//======================================
//    MeshImageIconRenderer class
//--------------------------------------

class MeshImageIconRenderer final : public IconRenderer {
  TMeshImageP m_image;
  TXshSimpleLevelP m_sl;
  TFrameId m_fid;
  IconGenerator::Settings m_settings;

public:
  MeshImageIconRenderer(const std::string &id, const TDimension &iconSize,
                        TXshSimpleLevelP sl, const TFrameId &fid,
                        const IconGenerator::Settings &settings)
      : IconRenderer(id, iconSize)
      , m_image()
      , m_sl(sl)
      , m_fid(fid)
      , m_settings(settings) {}

  MeshImageIconRenderer(const std::string &id, const TDimension &iconSize,
                        TMeshImageP image,
                        const IconGenerator::Settings &settings)
      : IconRenderer(id, iconSize)
      , m_image(image)
      , m_sl(0)
      , m_fid(-1)
      , m_settings(settings) {}

  TRaster32P generateRaster(const TDimension &iconSize) const;
  void run() override;
};

//-----------------------------------------------------------------------------

TRaster32P MeshImageIconRenderer::generateRaster(
    const TDimension &iconSize) const {
  TMeshImageP mi;

  int frame = 0;
  if (!m_image) {
    assert(m_sl);
    if (!m_sl->isFid(m_fid)) return TRaster32P();

    TImageP image = m_sl->getFrameIcon(m_fid);
    if (!image) return TRaster32P();

    mi = (TMeshImageP)image;
    if (!mi) return TRaster32P();

    frame = m_sl->guessIndex(m_fid);
  } else
    mi = m_image;

  assert(mi);

  return convertToIcon(mi, frame, iconSize, m_settings);
}

//-----------------------------------------------------------------------------

void MeshImageIconRenderer::run() {
  try {
    TRaster32P ras(generateRaster(getIconSize()));

    if (ras && ras->getRawData() && ras->getLx() > 0 && ras->getLy() > 0) {
      setIcon(ras);
    }
  } catch (...) {
  }
}

//=============================================================================

//==================================
//    XsheetIconRenderer class
//----------------------------------

class XsheetIconRenderer final : public IconRenderer {
  TXsheet *m_xsheet;
  int m_row;

public:
  XsheetIconRenderer(const std::string &id, const TDimension &iconSize,
                     TXsheet *xsheet, int row = 0)
      : IconRenderer(id, iconSize), m_xsheet(xsheet), m_row(row) {
    if (m_xsheet) {
      assert(m_xsheet->getRefCount() > 0);
      m_xsheet->addRef();
    }
  }

  ~XsheetIconRenderer() {
    if (m_xsheet) m_xsheet->release();
  }

  static std::string getId(TXshChildLevel *level, int row) {
    return "sub:" + ::to_string(level->getName()) + std::to_string(row);
  }

  TRaster32P generateRaster(const TDimension &iconSize) const;
  void run() override;
};

//-----------------------------------------------------------------------------

TRaster32P XsheetIconRenderer::generateRaster(
    const TDimension &iconSize) const {
  ToonzScene *scene = m_xsheet->getScene();
  if (!scene) return TRaster32P();

  TRaster32P ras(iconSize);
  if (!ras || !ras->getRawData()) return TRaster32P();

  TPixel32 bgColor = scene->getProperties()->getBgColor();
  bgColor.m        = 255;
  ras->fill(bgColor);

  TImageCache::instance()->setEnabled(false);
  // temporarily disable "Visualize Vector As Raster" option to prevent crash.
  // (see the issue #2862)
  bool rasterizePli               = TXshSimpleLevel::m_rasterizePli;
  TXshSimpleLevel::m_rasterizePli = false;

  // All checks are disabled
  try {
    scene->renderFrame(ras, m_row, m_xsheet, false);
  } catch (...) {
    // Ignore rendering errors
  }

  TXshSimpleLevel::m_rasterizePli = rasterizePli;
  TImageCache::instance()->setEnabled(true);

  return ras;
}

//-----------------------------------------------------------------------------

void XsheetIconRenderer::run() {
  TRaster32P ras = generateRaster(getIconSize());
  if (ras && ras->getRawData() && ras->getLx() > 0 && ras->getLy() > 0) {
    setIcon(ras);
  }
}

//=============================================================================

//================================
//    FileIconRenderer class
//--------------------------------

class FileIconRenderer final : public IconRenderer {
  TFilePath m_path;
  TFrameId m_fid;

public:
  FileIconRenderer(const TDimension &iconSize, const TFilePath &path,
                   const TFrameId &fid)
      : IconRenderer(getId(path, fid), iconSize), m_path(path), m_fid(fid) {}

  static std::string getId(const TFilePath &path, const TFrameId &fid);

  void run() override;
};

//-----------------------------------------------------------------------------

std::string FileIconRenderer::getId(const TFilePath &path,
                                    const TFrameId &fid) {
  std::string type(path.getType());

  if (type == "tab" || type == "tnz" ||
      type == "mesh" ||  // meshes are not currently viewable
      TFileType::isViewable(TFileType::getInfo(path))) {
    std::string fidNumber;
    if (fid != TFrameId::NO_FRAME)
      fidNumber = "frame:" + fid.expand(TFrameId::NO_PAD);
    return "$:" + ::to_string(path) + fidNumber;
  }

  // All the other types whose icon is the same for file type, get the same id
  // per type.
  else if (type == "tpl")
    return "$:tpl";
  else if (type == "tzp")
    return "$:tzp";
  else if (type == "svg")
    return "$:svg";
  else if (type == "tzu")
    return "$:tzu";
  else if (TFileType::getInfo(path) == TFileType::AUDIO_LEVEL)
    return "$:audio";
  else if (type == "scr")
    return "$:scr";
  else if (type == "mpath")
    return "$:mpath";
  else if (type == "curve")
    return "$:curve";
  else if (type == "cln")
    return "$:cln";
  else if (type == "tnzbat")
    return "$:tnzbat";
  else
    return "$:unknown";
}

//-----------------------------------------------------------------------------

TRaster32P IconGenerator::generateVectorFileIcon(const TFilePath &path,
                                                 const TDimension &iconSize,
                                                 const TFrameId &fid) {
  TLevelReaderP lr(path);
  if (!lr) return TRaster32P();

  TLevelP level = lr->loadInfo();
  if (level->begin() == level->end()) return TRaster32P();
  TFrameId frameId = fid;
  if (fid == TFrameId::NO_FRAME) frameId = level->begin()->first;
  TImageP img      = lr->getFrameReader(frameId)->load();
  TVectorImageP vi = img;
  if (!vi) return TRaster32P();
  vi->setPalette(level->getPalette());
  VectorImageIconRenderer vir("", iconSize, vi.getPointer(),
                              IconGenerator::Settings());
  return vir.generateRaster(iconSize);
}

//-----------------------------------------------------------------------------

TRaster32P IconGenerator::generateRasterFileIcon(const TFilePath &path,
                                                 const TDimension &iconSize,
                                                 const TFrameId &fid) {
  TImageP img;

  try {
    // Attempt image reading
    TLevelReaderP lr(path);
    if (!lr) return TRaster32P();

    TLevelP level = lr->loadInfo();

    if (level->begin() == level->end()) return TRaster32P();

    TFrameId frameId = fid;
    if (fid == TFrameId::NO_FRAME)  // In case no frame was specified, pick the
      frameId = level->begin()->first;  // first level frame

    TImageReaderP ir = lr->getFrameReader(frameId);
    if (!ir) return TRaster32P();

    if (const TImageInfo *ii = ir->getImageInfo()) {
      int shrinkX = ii->m_lx / iconSize.lx;
      int shrinkY = ii->m_ly / iconSize.ly;
      int shrink  = shrinkX < shrinkY ? shrinkX : shrinkY;

      if (shrink > 1) ir->setShrink(shrink);
    }

    img = (toUpper(path.getType()) == "TLV") ? ir->loadIcon() : ir->load();
  } catch (...) {
    return TRaster32P();
  }

  // Extract a 32-bit fullcolor raster from img
  TRaster32P ras32;

  if (TRasterImageP ri = img) {
    ras32 = ri->getRaster();

    if (!ras32) {
      if (TRasterGR8P rasGR8 = ri->getRaster()) {
        TRaster32P raux(rasGR8->getSize());
        if (raux && raux->getRawData()) {
          TRop::convert(raux, rasGR8);
          ras32 = raux;
        }
      }
    }
  } else if (TToonzImageP ti = img) {
    TRasterCM32P auxRaster = ti->getRaster();
    if (!auxRaster || !auxRaster->getRawData()) return TRaster32P();

    TRaster32P dstRaster(auxRaster->getSize());
    if (!dstRaster || !dstRaster->getRawData()) return TRaster32P();

    if (TPaletteP plt = ti->getPalette())
      TRop::convert(dstRaster, auxRaster, plt, false);
    else
      dstRaster->fill(TPixel32::Magenta);

    ras32 = dstRaster;
  }

  if (!ras32 || !ras32->getRawData()) return TRaster32P();

  TRaster32P icon(iconSize);
  if (!icon || !icon->getRawData()) return TRaster32P();

  double sx = double(iconSize.lx) / ras32->getLx();
  double sy = double(iconSize.ly) / ras32->getLy();
  double sc = std::min(sx, sy);  // show all the image, possibly adding bands

  TAffine aff = TScale(sc).place(ras32->getCenterD(), icon->getCenterD());

  icon->fill(TPixel32(255, 0, 0));  // "bands" color
  TRop::resample(icon, ras32, aff, TRop::Triangle);

  if (icon && icon->getRawData()) {
    if (::isUnpremultiplied(icon)) TRop::premultiply(icon);

    TRectI srcBBoxI = ras32->getBounds();
    TRectD srcBBoxD = aff * TRectD(srcBBoxI.x0, srcBBoxI.y0, srcBBoxI.x1 + 1,
                                   srcBBoxI.y1 + 1);

    TRect bbox = TRect(tfloor(srcBBoxD.x0), tceil(srcBBoxD.y0) - 1,
                       tfloor(srcBBoxD.x1), tceil(srcBBoxD.y1) - 1);

    bbox = (bbox * icon->getBounds()).enlarge(-1);
  } else {
    if (icon && icon->getRawData()) {
      icon->fill(TPixel32(255, 0, 0));
    }
  }

  return icon;
}

//-----------------------------------------------------------------------------

TRaster32P IconGenerator::generateSplineFileIcon(const TFilePath &path,
                                                 const TDimension &iconSize) {
  TStageObjectSpline *spline = new TStageObjectSpline();
  TIStream is(path);
  spline->loadData(is);
  SplineIconRenderer sr("", iconSize, spline);
  TRaster32P icon = sr.generateRaster(iconSize);
  delete spline;
  return icon;
}

//-----------------------------------------------------------------------------

TRaster32P IconGenerator::generateMeshFileIcon(const TFilePath &path,
                                               const TDimension &iconSize,
                                               const TFrameId &fid) {
  TLevelReaderP lr(path);
  if (!lr) return TRaster32P();

  TLevelP level = lr->loadInfo();
  if (level->begin() == level->end()) return TRaster32P();

  TFrameId frameId = fid;
  if (fid == TFrameId::NO_FRAME) frameId = level->begin()->first;

  TMeshImageP mi = lr->getFrameReader(frameId)->load();
  if (!mi) return TRaster32P();

  MeshImageIconRenderer mir("", iconSize, mi.getPointer(),
                            IconGenerator::Settings());
  return mir.generateRaster(iconSize);
}

//-----------------------------------------------------------------------------

TRaster32P IconGenerator::generateSceneFileIcon(const TFilePath &path,
                                                const TDimension &iconSize,
                                                int row) {
  if (row == 0 || row == TFrameId::NO_FRAME - 1) {
    TFilePath iconPath =
        path.getParentDir() + "sceneIcons" + (path.getWideName() + L" .png");
    return generateRasterFileIcon(iconPath, iconSize, TFrameId::NO_FRAME);
  } else {
    // obsolete
    ToonzScene scene;
    scene.load(path);
    XsheetIconRenderer ir("", iconSize, scene.getXsheet(), row);
    return ir.generateRaster(iconSize);
  }
}

//-----------------------------------------------------------------------------

void FileIconRenderer::run() {
  TDimension iconSize(getIconSize());
  try {
    TRaster32P iconRaster;
    std::string type(m_path.getType());

    if (type == "tnz" || type == "tab")
      iconRaster = IconGenerator::generateSceneFileIcon(m_path, iconSize,
                                                        m_fid.getNumber() - 1);
    else if (type == "pli")
      iconRaster =
          IconGenerator::generateVectorFileIcon(m_path, iconSize, m_fid);
    else if (type == "tpl") {
      QImage palette(":Resources/paletteicon.svg");
      setIcon(rasterFromQImage(palette));
      return;
    } else if (type == "tzp") {
      QImage palette(":Resources/tzpicon.png");
      setIcon(rasterFromQImage(palette));
      return;
    } else if (type == "svg") {
      QImage svg(getIconPath("svg_icon"));
      setIcon(rasterFromQImage(svg));
      return;
    } else if (type == "tzu") {
      QImage palette(":Resources/tzuicon.png");
      setIcon(rasterFromQImage(palette));
      return;
    } else if (TFileType::getInfo(m_path) == TFileType::AUDIO_LEVEL) {
      QImage loudspeaker(getIconPath("audio_icon"));
      setIcon(rasterFromQImage(loudspeaker));
      return;
    } else if (type == "scr") {
      QImage screensaver(":Resources/savescreen.png");
      setIcon(rasterFromQImage(screensaver));
      return;
    } else if (type == "psd") {
      QImage psdPath(getIconPath("psd_icon"));
      setIcon(rasterFromQImage(psdPath));
      return;
    } else if (type == "mesh")
      iconRaster = IconGenerator::generateMeshFileIcon(m_path, iconSize, m_fid);
    else if (TFileType::isViewable(TFileType::getInfo(m_path)) || type == "tlv")
      iconRaster =
          IconGenerator::generateRasterFileIcon(m_path, iconSize, m_fid);
    else if (type == "mpath") {
      QImage motionPath(getIconPath("motionpath_icon"));
      setIcon(rasterFromQImage(motionPath));
      return;
    } else if (type == "curve") {
      QImage curve(getIconPath("curve_icon"));
      setIcon(rasterFromQImage(curve));
      return;
    } else if (type == "cln") {
      QImage cln(getIconPath("cleanup_icon"));
      setIcon(rasterFromQImage(cln));
      return;
    } else if (type == "tnzbat") {
      QImage tnzBat(getIconPath("tasklist_icon"));
      setIcon(rasterFromQImage(tnzBat));
      return;
    } else if (type == "tls") {
      QImage tls(":Resources/magpie.svg");
      setIcon(rasterFromQImage(tls));
      return;
    } else if (type == "xdts") {
      QImage xdts(getIconPath("xdts_icon"));
      setIcon(rasterFromQImage(xdts));
      return;
    } else if (type == "js") {
      QImage script(getIconPath("script_icon"));
      setIcon(rasterFromQImage(script));
      return;
    } else if (type == "json") {
      QImage json(getIconPath("json_icon"));
      setIcon(rasterFromQImage(json));
      return;
    }

    else {
      QImage unknown(getIconPath("unknown_icon"));
      setIcon(rasterFromQImage(unknown));
      return;
    }
    if (!iconRaster || !iconRaster->getRawData()) {
      QImage broken(getIconPath("broken_icon"));
      setIcon(rasterFromQImage(broken));
      return;
    }
    setIcon(iconRaster);
  } catch (const TImageVersionException &) {
    QImage unknown(getIconPath("unknown_icon"));
    setIcon(rasterFromQImage(unknown));
  } catch (...) {
    QImage broken(getIconPath("broken_icon"));
    setIcon(rasterFromQImage(broken));
  }
}

//=============================================================================

//================================
//    SceneIconRenderer class
//--------------------------------

class SceneIconRenderer final : public IconRenderer {
  ToonzScene *m_toonzScene;

public:
  SceneIconRenderer(const TDimension &iconSize, ToonzScene *scene)
      : IconRenderer(getId(), iconSize), m_toonzScene(scene) {}

  static std::string getId() { return "currentScene"; }

  void run() override;
  TRaster32P generateIcon(const TDimension &iconSize) const;
};

//-----------------------------------------------------------------------------

TRaster32P SceneIconRenderer::generateIcon(const TDimension &iconSize) const {
  TRaster32P ras(iconSize);
  if (!ras || !ras->getRawData()) return TRaster32P();

  TPixel32 bgColor = m_toonzScene->getProperties()->getBgColor();
  bgColor.m        = 255;
  ras->fill(bgColor);

  try {
    m_toonzScene->renderFrame(ras, 0, 0, false);
  } catch (...) {
    // Ignore rendering errors
  }

  return ras;
}

//-----------------------------------------------------------------------------

void SceneIconRenderer::run() {
  TRaster32P icon = generateIcon(getIconSize());
  if (icon && icon->getRawData() && icon->getLx() > 0 && icon->getLy() > 0) {
    setIcon(icon);
  }
}

//=============================================================================

//===================================
//
//    IconGenerator class
//
//-----------------------------------

IconGenerator::IconGenerator() : m_iconSize(FilmstripIconSize) {
  m_executor.setMaxActiveTasks(1);  // Only one thread to render icons...
  m_executor.setDedicatedThreads(true);
}

//-----------------------------------------------------------------------------

IconGenerator::~IconGenerator() {}

//-----------------------------------------------------------------------------

IconGenerator *IconGenerator::instance() {
  static IconGenerator _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

void IconGenerator::setFilmstripIconSize(const TDimension &dim) {
  FilmstripIconSize = dim;
}

//-----------------------------------------------------------------------------

TDimension IconGenerator::getIconSize() const { return FilmstripIconSize; }

//-----------------------------------------------------------------------------

TOfflineGL *IconGenerator::getOfflineGLContext() {
  // One context per rendering thread
  if (!m_contexts.hasLocalData()) {
    TDimension contextSize(std::max(FilmstripIconSize.lx, IconSize.lx),
                           std::max(FilmstripIconSize.ly, IconSize.ly));

    // ADDED: Retry logic for context creation
    TOfflineGL *context = nullptr;
    for (int retry = 0; retry < 3 && !context; ++retry) {
      try {
        context = new TOfflineGL(contextSize);
        if (retry < 2) QThread::msleep(10);
      } catch (...) {
        context = nullptr;
      }
    }

    if (context) {
      m_contexts.setLocalData(context);
    }
  }
  return m_contexts.localData();
}

//-----------------------------------------------------------------------------

void IconGenerator::addTask(const std::string &id,
                            TThread::RunnableP iconRenderer) {
  {
    QMutexLocker locker(&iconsMapMutex);  // ADDED: Thread safety
    iconsMap.insert(id);
  }
  m_executor.addTask(iconRenderer);
}

//-----------------------------------------------------------------------------

QPixmap IconGenerator::getIcon(TXshLevel *xl, const TFrameId &fid,
                               bool filmStrip, bool onDemand) {
  if (!xl) return QPixmap();

  if (TXshChildLevel *cl = xl->getChildLevel()) {
    if (filmStrip) return QPixmap();

    std::string id = XsheetIconRenderer::getId(cl, fid.getNumber() - 1);
    QPixmap pix;
    if (::getIcon(id, pix)) return pix;

    if (onDemand) return pix;

    TDimension iconSize = TDimension(80, 60);

    // The icon must be calculated - add an IconRenderer task.
    addTask(id, new XsheetIconRenderer(id, iconSize, cl->getXsheet()));
  }

  if (TXshSimpleLevel *sl = xl->getSimpleLevel()) {
    // make thumbnails for cleanup preview and cameratest to be the same as
    // normal TLV
    std::string id;
    int status = sl->getFrameStatus(fid);
    if (sl->getType() == TZP_XSHLEVEL &&
        status & TXshSimpleLevel::CleanupPreview) {
      sl->setFrameStatus(fid, status & ~TXshSimpleLevel::CleanupPreview);
      id = sl->getIconId(fid);
      sl->setFrameStatus(fid, status);
    } else
      id = sl->getIconId(fid);

    if (!filmStrip) id += "_small";

    QPixmap pix;
    if (::getIcon(id, pix, xl->getSimpleLevel())) return pix;

    if (onDemand) return pix;

    IconGenerator::Settings oldSettings = m_settings;

    // Disable transparency check for cast and xsheet icons
    if (!filmStrip) m_settings = IconGenerator::Settings();

    TDimension iconSize = filmStrip ? m_iconSize : TDimension(80, 60);

    int type = sl->getType();
    switch (type) {
    case OVL_XSHLEVEL:
    case TZI_XSHLEVEL:
      addTask(id, new RasterImageIconRenderer(id, iconSize, sl, fid));
      break;
    case PLI_XSHLEVEL:
      addTask(id,
              new VectorImageIconRenderer(id, iconSize, sl, fid, m_settings));
      break;
    case TZP_XSHLEVEL:
      // Yep, we could have rasters, due to a cleanupping process
      if (status == TXshSimpleLevel::Scanned)
        addTask(id, new RasterImageIconRenderer(id, iconSize, sl, fid));
      else
        addTask(id,
                new ToonzImageIconRenderer(id, iconSize, sl, fid, m_settings));
      break;
    case MESH_XSHLEVEL:
      addTask(id, new MeshImageIconRenderer(id, iconSize, sl, fid, m_settings));
      break;
    default:
      addTask(id, new NoImageIconRenderer(id, iconSize));
      break;
    }

    m_settings = oldSettings;
  }

  return QPixmap();
}

//-----------------------------------------------------------------------------

QPixmap IconGenerator::getSizedIcon(TXshLevel *xl, const TFrameId &fid,
                                    std::string newId, TDimension dim) {
  if (!xl) return QPixmap();

  if (TXshChildLevel *cl = xl->getChildLevel()) {
    std::string id = XsheetIconRenderer::getId(cl, fid.getNumber() - 1);
    QPixmap pix;
    if (::getIcon(id, pix)) return pix;

    TDimension iconSize = TDimension(80, 60);
    if (dim != TDimension(0, 0)) {
      iconSize = dim;
    }

    addTask(id, new XsheetIconRenderer(id, iconSize, cl->getXsheet()));
  }

  if (TXshSimpleLevel *sl = xl->getSimpleLevel()) {
    // make thumbnails for cleanup preview and cameratest to be the same as
    // normal TLV
    std::string id;
    int status = sl->getFrameStatus(fid);
    if (sl->getType() == TZP_XSHLEVEL &&
        status & TXshSimpleLevel::CleanupPreview) {
      sl->setFrameStatus(fid, status & ~TXshSimpleLevel::CleanupPreview);
      id = sl->getIconId(fid);
      sl->setFrameStatus(fid, status);
    } else
      id = sl->getIconId(fid);

    id += newId;

    QPixmap pix;
    if (::getIcon(id, pix, xl->getSimpleLevel())) return pix;

    IconGenerator::Settings oldSettings = m_settings;

    TDimension iconSize = TDimension(80, 60);
    if (dim != TDimension(0, 0)) {
      iconSize = dim;
    }

    int type = sl->getType();
    switch (type) {
    case OVL_XSHLEVEL:
    case TZI_XSHLEVEL:
      addTask(id, new RasterImageIconRenderer(id, iconSize, sl, fid));
      break;
    case PLI_XSHLEVEL:
      addTask(id,
              new VectorImageIconRenderer(id, iconSize, sl, fid, m_settings));
      break;
    case TZP_XSHLEVEL:
      // Yep, we could have rasters, due to a cleanupping process
      if (status == TXshSimpleLevel::Scanned)
        addTask(id, new RasterImageIconRenderer(id, iconSize, sl, fid));
      else
        addTask(id,
                new ToonzImageIconRenderer(id, iconSize, sl, fid, m_settings));
      break;
    case MESH_XSHLEVEL:
      addTask(id, new MeshImageIconRenderer(id, iconSize, sl, fid, m_settings));
      break;
    default:
      assert(false);
      break;
    }

    m_settings = oldSettings;
  }

  return QPixmap();
}

//-----------------------------------------------------------------------------

void IconGenerator::invalidate(TXshLevel *xl, const TFrameId &fid,
                               bool onlyFilmStrip) {
  if (!xl) return;

  if (TXshSimpleLevel *sl = xl->getSimpleLevel()) {
    std::string id = sl->getIconId(fid);

    int type = sl->getType();

    switch (type) {
    case OVL_XSHLEVEL:
    case TZI_XSHLEVEL:
      addTask(id, new RasterImageIconRenderer(id, getIconSize(), sl, fid));
      break;
    case PLI_XSHLEVEL:
      removeIcon(id);
      addTask(id, new VectorImageIconRenderer(id, getIconSize(), sl, fid,
                                              m_settings));
      break;
    case TZP_XSHLEVEL:
      if (sl->getFrameStatus(fid) == TXshSimpleLevel::Scanned)
        addTask(id, new RasterImageIconRenderer(id, getIconSize(), sl, fid));
      else
        addTask(id, new ToonzImageIconRenderer(id, getIconSize(), sl, fid,
                                               m_settings));
      break;
    case MESH_XSHLEVEL:
      addTask(id, new MeshImageIconRenderer(id, getIconSize(), sl, fid,
                                            m_settings));
      break;
    default:
      addTask(id, new NoImageIconRenderer(id, getIconSize()));
      break;
    }

    if (onlyFilmStrip) return;

    id += "_small";
    {
      QMutexLocker locker(&iconsMapMutex);
      if (iconsMap.find(id) == iconsMap.end()) return;
    }

    // Not-filmstrip icons disable all checks
    IconGenerator::Settings oldSettings = m_settings;
    m_settings.m_transparencyCheck      = false;
    m_settings.m_inkIndex               = -1;
    m_settings.m_paintIndex             = -1;
    m_settings.m_blackBgCheck           = false;

    switch (type) {
    case OVL_XSHLEVEL:
    case TZI_XSHLEVEL:
      addTask(id, new RasterImageIconRenderer(id, TDimension(80, 60), sl, fid));
      break;
    case PLI_XSHLEVEL:
      addTask(id, new VectorImageIconRenderer(id, TDimension(80, 60), sl, fid,
                                              m_settings));
      break;
    case TZP_XSHLEVEL:
      if (sl->getFrameStatus(fid) == TXshSimpleLevel::Scanned)
        addTask(id,
                new RasterImageIconRenderer(id, TDimension(80, 60), sl, fid));
      else
        addTask(id, new ToonzImageIconRenderer(id, TDimension(80, 60), sl, fid,
                                               m_settings));
      break;
    case MESH_XSHLEVEL:
      addTask(id, new MeshImageIconRenderer(id, TDimension(80, 60), sl, fid,
                                            m_settings));
      break;
    default:
      addTask(id, new NoImageIconRenderer(id, TDimension(80, 60)));
      break;
    }

    m_settings = oldSettings;
  } else if (TXshChildLevel *cl = xl->getChildLevel()) {
    if (onlyFilmStrip) return;

    std::string id = XsheetIconRenderer::getId(cl, fid.getNumber() - 1);
    removeIcon(id);

    getIcon(xl, fid);
  }
}

//-----------------------------------------------------------------------------

void IconGenerator::remove(TXshLevel *xl, const TFrameId &fid,
                           bool onlyFilmStrip) {
  if (!xl) return;
  if (TXshSimpleLevel *sl = xl->getSimpleLevel()) {
    std::string id(sl->getIconId(fid));

    removeIcon(id);
    if (!onlyFilmStrip) removeIcon(id + "_small");
  } else {
    TXshChildLevel *cl = xl->getChildLevel();
    if (cl && !onlyFilmStrip)
      removeIcon(XsheetIconRenderer::getId(cl, fid.getNumber() - 1));
  }
}

//-----------------------------------------------------------------------------

QPixmap IconGenerator::getIcon(TStageObjectSpline *spline) {
  if (!spline) return QPixmap();
  std::string iconName = spline->getIconId();

  QPixmap pix;
  if (::getIcon(iconName, pix)) return pix;

  addTask(iconName, new SplineIconRenderer(iconName, getIconSize(), spline));

  return QPixmap();
}

//-----------------------------------------------------------------------------

void IconGenerator::invalidate(TStageObjectSpline *spline) {
  if (!spline) return;
  std::string iconName = spline->getIconId();
  removeIcon(iconName);

  addTask(iconName, new SplineIconRenderer(iconName, getIconSize(), spline));
}

//-----------------------------------------------------------------------------

void IconGenerator::remove(TStageObjectSpline *spline) {
  if (!spline) return;
  std::string iconName = spline->getIconId();
  removeIcon(iconName);
}

//-----------------------------------------------------------------------------

QPixmap IconGenerator::getIcon(const TFilePath &path, const TFrameId &fid) {
  std::string id = FileIconRenderer::getId(path, fid);

  QPixmap pix;
  TDimension fileIconSize(80, 60);
  if (::getIcon(id, pix, 0, fileIconSize)) return pix;

  addTask(id, new FileIconRenderer(fileIconSize, path, fid));

  return QPixmap();
}

//-----------------------------------------------------------------------------

void IconGenerator::invalidate(const TFilePath &path, const TFrameId &fid) {
  std::string id = FileIconRenderer::getId(path, fid);
  removeIcon(id);
  addTask(id, new FileIconRenderer(TDimension(80, 60), path, fid));
}

//-----------------------------------------------------------------------------

void IconGenerator::remove(const TFilePath &path, const TFrameId &fid) {
  removeIcon(FileIconRenderer::getId(path, fid));
}

//-----------------------------------------------------------------------------

QPixmap IconGenerator::getSceneIcon(ToonzScene *scene) {
  std::string id(SceneIconRenderer::getId());

  QPixmap pix;
  if (::getIcon(id, pix)) return pix;

  addTask(id, new SceneIconRenderer(getIconSize(), scene));

  return QPixmap();
}

//-----------------------------------------------------------------------------

void IconGenerator::invalidateSceneIcon() {
  removeIcon(SceneIconRenderer::getId());
}

//-----------------------------------------------------------------------------

void IconGenerator::remap(const std::string &newIconId,
                          const std::string &oldIconId) {
  QMutexLocker locker(&iconsMapMutex);  // ADDED: Thread safety
  IconIterator it = iconsMap.find(oldIconId);
  if (it == iconsMap.end()) return;

  iconsMap.erase(it);
  iconsMap.insert(newIconId);

  try {
    TImageCache::instance()->remap(newIconId, oldIconId);
  } catch (...) {
    // Ignore remap errors
  }
}

//-----------------------------------------------------------------------------

void IconGenerator::clearRequests() { m_executor.cancelAll(); }

//-----------------------------------------------------------------------------

void IconGenerator::clearSceneIcons() {
  QMutexLocker locker(&iconsMapMutex);  // ADDED: Thread safety
  // Eliminate all icons whose prefix is not "$:" (that is, scene-independent
  // images).
  // The abovementioned prefix is internally recognized by the image cache when
  // the scene
  // changes to avoid clearing file browser's icons.

  // Observe that image cache's clear function invoked during scene changes is
  // called through
  // the ImageManager::clear() method, including FilmStrip icons.

  // note the ';' - which follows ':' in the ascii table
  iconsMap.erase(iconsMap.begin(), iconsMap.lower_bound("$:"));
  iconsMap.erase(iconsMap.lower_bound("$;"), iconsMap.end());
}

//-----------------------------------------------------------------------------

void IconGenerator::onStarted(TThread::RunnableP iconRenderer) {
  if (!iconRenderer) return;

  IconRenderer *ir = static_cast<IconRenderer *>(iconRenderer.getPointer());
  if (ir) {
    ir->hasStarted() = true;
  }
}

//-----------------------------------------------------------------------------

void IconGenerator::onCanceled(TThread::RunnableP iconRenderer) {
  if (!iconRenderer) return;

  IconRenderer *ir = static_cast<IconRenderer *>(iconRenderer.getPointer());
  if (ir && !ir->hasStarted()) {
    removeIcon(ir->getId());
  }
}

//-----------------------------------------------------------------------------

void IconGenerator::onFinished(TThread::RunnableP iconRenderer) {
  if (!iconRenderer) return;

  IconRenderer *ir = static_cast<IconRenderer *>(iconRenderer.getPointer());
  if (!ir) return;

  try {
    // if the icon was generated in TToonzImage format, cache it instead
    ToonzImageIconRenderer *tir = dynamic_cast<ToonzImageIconRenderer *>(ir);
    if (tir) {
      TRasterCM32P timgp = tir->getIcon_TnzImg();
      if (timgp && timgp->getRawData() && timgp->getLx() > 0 &&
          timgp->getLy() > 0) {
        ::setIcon_TnzImg(ir->getId(), timgp);
        emit iconGenerated();
        if (ir->wasTerminated()) m_iconsTerminationLoop.quit();
        return;
      }
    }

    // Update the icons map with proper validation
    TRaster32P iconRaster = ir->getIcon();
    if (iconRaster && iconRaster->getRawData() && iconRaster->getLx() > 0 &&
        iconRaster->getLy() > 0) {
      ::setIcon(ir->getId(), iconRaster);
      emit iconGenerated();
    }
  } catch (const std::exception &e) {
    // Log error if needed
  } catch (...) {
    // Handle unknown exceptions
  }

  if (ir->wasTerminated()) m_iconsTerminationLoop.quit();
}

//-----------------------------------------------------------------------------

void IconGenerator::onException(TThread::RunnableP iconRenderer) {
  if (!iconRenderer) return;

  IconRenderer *ir = static_cast<IconRenderer *>(iconRenderer.getPointer());
  if (ir && ir->wasTerminated()) {
    m_iconsTerminationLoop.quit();
  }
}

//-----------------------------------------------------------------------------

void IconGenerator::onTerminated(TThread::RunnableP iconRenderer) {
  if (!iconRenderer) return;

  IconRenderer *ir = static_cast<IconRenderer *>(iconRenderer.getPointer());
  if (ir) {
    ir->wasTerminated() = true;
    m_iconsTerminationLoop.exec();
  }
}
