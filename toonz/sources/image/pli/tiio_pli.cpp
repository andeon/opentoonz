

#include "tiio_pli.h"
//#include "tstrokeoutline.h"
#include "tsystem.h"
#include "pli_io.h"
//#include "tstrokeutil.h"
#include "tregion.h"
#include "tsimplecolorstyles.h"
#include "tpalette.h"
//#include "tspecialstyleid.h"
#include "tiio.h"
#include "tconvert.h"
#include "tcontenthistory.h"
#include "tstroke.h"

typedef TVectorImage::IntersectionBranch IntersectionBranch;

//=============================================================================
const TSolidColorStyle ConstStyle(TPixel32::Black);

static TSolidColorStyle *CurrStyle = NULL;

namespace {

//---------------------------------------------------------------------------

/**
 * Output stream for writing style parameters to PLI format
 */
class PliOutputStream final : public TOutputStreamInterface {
  std::vector<TStyleParam> *m_stream;

public:
  PliOutputStream(std::vector<TStyleParam> *stream) : m_stream(stream) {}
  TOutputStreamInterface &operator<<(double x) override {
    m_stream->push_back(TStyleParam(x));
    return *this;
  }
  TOutputStreamInterface &operator<<(int x) override {
    m_stream->push_back(TStyleParam(x));
    return *this;
  }
  TOutputStreamInterface &operator<<(std::string x) override {
    m_stream->push_back(TStyleParam(x));
    return *this;
  }
  TOutputStreamInterface &operator<<(USHORT x) override {
    m_stream->push_back(TStyleParam(x));
    return *this;
  }
  TOutputStreamInterface &operator<<(BYTE x) override {
    m_stream->push_back(TStyleParam(x));
    return *this;
  }
  TOutputStreamInterface &operator<<(const TRaster32P &x) override {
    m_stream->push_back(TStyleParam(x));
    return *this;
  }
};

//---------------------------------------------------------------------------

/**
 * Input stream for reading style parameters from PLI format
 * Enhanced with bounds checking for corrupted file recovery
 */
class PliInputStream final : public TInputStreamInterface {
  std::vector<TStyleParam> *m_stream;
  VersionNumber m_version;
  int m_count;

public:
  PliInputStream(std::vector<TStyleParam> *stream, int majorVersion,
                 int minorVersion)
      : m_stream(stream), m_version(majorVersion, minorVersion), m_count(0) {}

  /**
   * Check if we have reached the end of the stream or beyond bounds
   * Throws exception if bounds are exceeded for safe recovery
   */
  void checkBounds() const {
    if (m_count >= m_stream->size()) {
      throw TException("Read beyond stream bounds in PLI file");
    }
  }

  TInputStreamInterface &operator>>(double &x) override {
    checkBounds();
    if ((*m_stream)[m_count].m_type != TStyleParam::SP_DOUBLE) {
      throw TException("Type mismatch: expected double in PLI stream");
    }
    x = (*m_stream)[m_count++].m_numericVal;
    return *this;
  }
  
  TInputStreamInterface &operator>>(int &x) override {
    checkBounds();
    if ((*m_stream)[m_count].m_type != TStyleParam::SP_INT) {
      throw TException("Type mismatch: expected int in PLI stream");
    }
    x = (int)(*m_stream)[m_count++].m_numericVal;
    return *this;
  }
  
  TInputStreamInterface &operator>>(std::string &x) override {
    checkBounds();
    if ((*m_stream)[m_count].m_type == TStyleParam::SP_INT)
      x = std::to_string(static_cast<int>((*m_stream)[m_count++].m_numericVal));
    else {
      if ((*m_stream)[m_count].m_type != TStyleParam::SP_STRING) {
        throw TException("Type mismatch: expected string in PLI stream");
      }
      x = (*m_stream)[m_count++].m_string;
    }
    return *this;
  }
  
  TInputStreamInterface &operator>>(BYTE &x) override {
    checkBounds();
    if ((*m_stream)[m_count].m_type != TStyleParam::SP_BYTE) {
      throw TException("Type mismatch: expected byte in PLI stream");
    }
    x = (BYTE)(*m_stream)[m_count++].m_numericVal;
    return *this;
  }
  
  TInputStreamInterface &operator>>(USHORT &x) override {
    checkBounds();
    if ((*m_stream)[m_count].m_type != TStyleParam::SP_USHORT) {
      throw TException("Type mismatch: expected unsigned short in PLI stream");
    }
    x = (USHORT)(*m_stream)[m_count++].m_numericVal;
    return *this;
  }
  
  TInputStreamInterface &operator>>(TRaster32P &x) override {
    checkBounds();
    if ((*m_stream)[m_count].m_type != TStyleParam::SP_RASTER) {
      throw TException("Type mismatch: expected raster in PLI stream");
    }
    x = (*m_stream)[m_count++].m_r;
    return *this;
  }

  VersionNumber versionNumber() const override { return m_version; }
};

//---------------------------------------------------------------------------

/**
 * Get color from stroke (currently returns transparent)
 * Placeholder for future implementation
 */
TPixel32 getColor(const TStroke *stroke) {
  return TPixel32::Transparent;
}

//---------------------------------------------------------------------------

/**
 * Find color index in color array, return array size if not found
 */
UINT findColor(const TPixel32 &color, const std::vector<TPixel> &colorArray) {
  for (UINT i = 0; i < colorArray.size(); i++)
    if (colorArray[i] == color) return i;
  return colorArray.size();
}

//---------------------------------------------------------------------------

/**
 * Build palette information for PLI file
 * Handles both static and animated palettes
 */
void buildPalette(ParsedPli *pli, const TImageP img) {
  if (!pli->m_palette_tags.empty()) return;
  TVectorImageP tempVecImg = img;
  TPalette *vPalette       = tempVecImg->getPalette();
  unsigned int i;

  // If there's a reference image, use the first style to store the path
  TFilePath fp;
  if ((fp = vPalette->getRefImgPath()) != TFilePath()) {
    TStyleParam styleParam("refimage" + ::to_string(fp));
    StyleTag *refImageTag = new StyleTag(0, 0, 1, &styleParam);
    pli->m_palette_tags.push_back((PliObjectTag *)refImageTag);
  }

  // For writing palette pages, misuse a style: the first style
  // has all string parameters that match page names
  assert(vPalette->getPageCount());

  std::vector<TStyleParam> pageNames(vPalette->getPageCount());
  for (i = 0; i < pageNames.size(); i++)
    pageNames[i] = TStyleParam(::to_string(vPalette->getPage(i)->getName()));
  StyleTag *pageNamesTag =
      new StyleTag(0, 0, pageNames.size(), pageNames.data());

  pli->m_palette_tags.push_back((PliObjectTag *)pageNamesTag);

  // Write all color styles to the palette
  for (i = 1; i < (unsigned)vPalette->getStyleCount(); i++) {
    TColorStyle *style   = vPalette->getStyle(i);
    TPalette::Page *page = vPalette->getStylePage(i);
    if (!page) continue;
    int pageIndex = page->getIndex();

    std::vector<TStyleParam> stream;
    PliOutputStream chan(&stream);
    style->save(chan);  // Fill the stream with style data

    assert(pageIndex >= 0 && pageIndex <= 65535);
    StyleTag *styleTag =
        new StyleTag(i, pageIndex, stream.size(), stream.data());
    pli->m_palette_tags.push_back((PliObjectTag *)styleTag);
  }

  // Handle animated palettes (multi-palette)
  if (vPalette->isAnimated()) {
    std::set<int> keyFrames;
    for (i = 0; i < (unsigned)vPalette->getStyleCount(); i++)
      for (int j = 0; j < vPalette->getKeyframeCount(i); j++)
        keyFrames.insert(vPalette->getKeyframe(i, j));

    std::set<int>::const_iterator it = keyFrames.begin();
    for (; it != keyFrames.end(); ++it) {
      int frame = *it;
      vPalette->setFrame(frame);
      StyleTag *pageNamesTag = new StyleTag(frame, 0, 0, 0);
      pli->m_palette_tags.push_back((PliObjectTag *)pageNamesTag);
      
      for (i = 1; i < (unsigned)vPalette->getStyleCount(); i++) {
        if (vPalette->isKeyframe(i, frame)) {
          TColorStyle *style   = vPalette->getStyle(i);
          TPalette::Page *page = vPalette->getStylePage(i);
          if (!page) continue;
          int pageIndex = page->getIndex();

          std::vector<TStyleParam> stream;
          PliOutputStream chan(&stream);
          style->save(chan);

          assert(pageIndex >= 0 && pageIndex <= 65535);
          StyleTag *styleTag =
              new StyleTag(i, pageIndex, stream.size(), stream.data());
          pli->m_palette_tags.push_back((PliObjectTag *)styleTag);
        }
      }
    }
  }
}

//---------------------------------------------------------------------------

/**
 * Stroke creation data container
 */
struct CreateStrokeData {
  int m_styleId;
  TStroke::OutlineOptions m_options;

  CreateStrokeData() : m_styleId(-1) {}
};

/**
 * Create stroke from quadratic chain data with error recovery
 * Skips invalid curves and continues with valid ones
 */
void createStrokeWithRecovery(ThickQuadraticChainTag *quadTag, 
                            TVectorImage *outVectImage,
                            const CreateStrokeData &data) {
  if (!quadTag || quadTag->m_numCurves == 0) {
    TSystem::outputDebug("Invalid quadratic chain tag - skipping");
    return;
  }

  try {
    std::vector<TThickQuadratic*> chunks;
    chunks.reserve(quadTag->m_numCurves);

    // Collect valid curves, skip invalid ones
    for (UINT k = 0; k < quadTag->m_numCurves; k++) {
      if (&quadTag->m_curve[k]) {
        chunks.push_back(&quadTag->m_curve[k]);
      } else {
        TSystem::outputDebug("Invalid curve at index " + std::to_string(k) + " - skipping");
      }
    }

    if (chunks.empty()) {
      TSystem::outputDebug("No valid curves in quadratic chain - skipping stroke");
      return;
    }

    TStroke* stroke = TStroke::create(chunks);
    if (!stroke) {
      TSystem::outputDebug("Failed to create stroke from valid curves");
      return;
    }

    if (data.m_styleId != -1) {
      stroke->setStyle(data.m_styleId);
    } else {
      TSystem::outputDebug("Warning: Stroke created with invalid style ID");
    }
    
    stroke->outlineOptions() = data.m_options;

    if (quadTag->m_isLoop) {
      stroke->setSelfLoop();
    }
    
    outVectImage->addStroke(stroke, false);
    
  } catch (const std::exception& e) {
    TSystem::outputDebug("Failed to create stroke: " + std::string(e.what()));
    // Don't rethrow - allow continuation with other strokes
  }
}

/**
 * Original stroke creation function (kept for compatibility)
 */
void createStroke(ThickQuadraticChainTag *quadTag, TVectorImage *outVectImage,
                  const CreateStrokeData &data) {
  std::vector<TThickQuadratic *> chunks(quadTag->m_numCurves);

  for (UINT k = 0; k < quadTag->m_numCurves; k++)
    chunks[k] = &quadTag->m_curve[k];

  TStroke *stroke = TStroke::create(chunks);

  assert(data.m_styleId != -1);
  stroke->setStyle(data.m_styleId);
  stroke->outlineOptions() = data.m_options;

  if (quadTag->m_isLoop) stroke->setSelfLoop();
  outVectImage->addStroke(stroke, false);
}

/**
 * Create group with error recovery - processes valid objects and skips corrupted ones
 */
void createGroupWithRecovery(GroupTag *groupTag, TVectorImage *vi,
                           CreateStrokeData &data) {
  if (!groupTag) {
    TSystem::outputDebug("Invalid group tag encountered");
    return;
  }

  int count = vi->getStrokeCount();
  int validObjects = 0;
  
  // Process each object in the group with individual error handling
  for (int j = 0; j < groupTag->m_numObjects; j++) {
    if (!groupTag->m_object[j]) {
      TSystem::outputDebug("Null object in group - skipping");
      continue;
    }
    
    try {
      switch (groupTag->m_object[j]->m_type) {
        case PliTag::COLOR_NGOBJ:
          if (static_cast<ColorTag*>(groupTag->m_object[j])->m_numColors >= 1) {
            data.m_styleId = static_cast<ColorTag*>(groupTag->m_object[j])->m_color[0];
            validObjects++;
          } else {
            TSystem::outputDebug("Color tag with no colors - using previous style");
          }
          break;
          
        case PliTag::OUTLINE_OPTIONS_GOBJ:
          data.m_options = 
            static_cast<StrokeOutlineOptionsTag*>(groupTag->m_object[j])->m_options;
          validObjects++;
          break;
          
        case PliTag::GROUP_GOBJ:
          createGroupWithRecovery(static_cast<GroupTag*>(groupTag->m_object[j]), vi, data);
          validObjects++;
          break;
          
        case PliTag::THICK_QUADRATIC_CHAIN_GOBJ:
          createStrokeWithRecovery(static_cast<ThickQuadraticChainTag*>(groupTag->m_object[j]), 
                                 vi, data);
          validObjects++;
          break;
          
        default:
          TSystem::outputDebug("Unknown object type in group: " + 
                              std::to_string(groupTag->m_object[j]->m_type));
          break;
      }
    } catch (const std::exception& e) {
      TSystem::outputDebug("Failed to process group object " + std::to_string(j) + 
                          ": " + std::string(e.what()));
      // Continue with next object in group despite errors
    }
  }

  // Only group if we have valid objects and new strokes were added
  if (validObjects > 0 && vi->getStrokeCount() > count) {
    try {
      vi->group(count, vi->getStrokeCount() - count);
    } catch (const std::exception& e) {
      TSystem::outputDebug("Failed to group strokes: " + std::string(e.what()));
      // Continue without grouping - better than crashing
    }
  } else if (validObjects == 0) {
    TSystem::outputDebug("Group contained no valid objects");
  }
}

}  // unnamed namespace

//-----------------------------------------------------------------------------

//===========================================================================

/**
 * Image writer for PLI format - writes individual frames to PLI file
 */
class TImageWriterPli final : public TImageWriter {
public:
  TImageWriterPli(const TFilePath &, const TFrameId &frameId,
                  TLevelWriterPli *);
  ~TImageWriterPli() {}

private:
  UCHAR m_precision;
  // not implemented
  TImageWriterPli(const TImageWriterPli &);
  TImageWriterPli &operator=(const TImageWriterPli &src);

public:
  void save(const TImageP &) override;
  TFrameId m_frameId;

private:
  TLevelWriterPli *m_lwp;
};

//=============

/**
 * Main load function with recovery fallback
 * Attempts normal load first, then falls back to recovery mode if needed
 */
TImageP TImageReaderPli::load() {
  if (!m_lrp->m_doesExist)
    throw TImageException(getFilePath(), "Error file doesn't exist");

  UINT majorVersionNumber, minorVersionNumber;
  m_lrp->m_pli->getVersion(majorVersionNumber, minorVersionNumber);
  
  // Allow loading of older versions with recovery mode
  if (majorVersionNumber <= 5 && (majorVersionNumber != 5 || minorVersionNumber < 5)) {
    TSystem::outputDebug("Loading legacy PLI version with recovery: " + 
                        std::to_string(majorVersionNumber) + "." + 
                        std::to_string(minorVersionNumber));
    return doLoadWithRecovery();
  }

  // Try normal load first, fall back to recovery on failure
  try {
    return doLoad();
  } catch (const std::exception& e) {
    TSystem::outputDebug("Normal load failed, attempting recovery: " + 
                        std::string(e.what()));
    return doLoadWithRecovery();
  }
}

//===========================================================================

/**
 * Read region data from version 4.x PLI files
 */
static void readRegionVersion4x(IntersectionDataTag *tag, TVectorImage *img) {
#ifndef NEW_REGION_FILL
  img->setFillData(tag->m_branchArray, tag->m_branchCount);
#endif
}

//-----------------------------------------------------------------------------

/**
 * Create group from group tag - original version without recovery
 */
static void createGroup(GroupTag *groupTag, TVectorImage *vi,
                        CreateStrokeData &data) {
  int count = vi->getStrokeCount();
  for (int j = 0; j < groupTag->m_numObjects; j++) {
    if (groupTag->m_object[j]->m_type == PliTag::COLOR_NGOBJ)
      data.m_styleId = ((ColorTag *)groupTag->m_object[j])->m_color[0];
    else if (groupTag->m_object[j]->m_type == PliTag::OUTLINE_OPTIONS_GOBJ)
      data.m_options =
          ((StrokeOutlineOptionsTag *)groupTag->m_object[j])->m_options;
    else if (groupTag->m_object[j]->m_type == PliTag::GROUP_GOBJ)
      createGroup((GroupTag *)groupTag->m_object[j], vi, data);
    else {
      assert(groupTag->m_object[j]->m_type ==
             PliTag::THICK_QUADRATIC_CHAIN_GOBJ);
      createStroke((ThickQuadraticChainTag *)groupTag->m_object[j], vi, data);
    }
  }

  vi->group(count, vi->getStrokeCount() - count);
}

//-----------------------------------------------------------------------------

/**
 * Normal load function - uses original loading logic
 * Throws exceptions on corruption for fallback to recovery mode
 */
TImageP TImageReaderPli::doLoad() {
  CreateStrokeData strokeData;

  // Prepare output image
  TVectorImage *outVectImage = new TVectorImage(true);
  
  UINT i;
  outVectImage->setAutocloseTolerance(m_lrp->m_pli->getAutocloseTolerance());

  ImageTag *imageTag;

  imageTag = m_lrp->m_pli->loadFrame(m_frameId);
  if (!imageTag)
    throw TImageException(m_path, "Corrupted or invalid image data");

  if (m_lrp->m_mapOfImage[m_frameId].second == false)
    m_lrp->m_mapOfImage[m_frameId].second = true;

  // Process all objects in the image tag
  for (i = 0; i < imageTag->m_numObjects; i++) {
    switch (imageTag->m_object[i]->m_type) {
    case PliTag::GROUP_GOBJ:
      assert(((GroupTag *)imageTag->m_object[i])->m_type == GroupTag::STROKE);
      createGroup((GroupTag *)imageTag->m_object[i], outVectImage, strokeData);
      break;

    case PliTag::INTERSECTION_DATA_GOBJ:
      readRegionVersion4x((IntersectionDataTag *)imageTag->m_object[i],
                          outVectImage);
      break;

    case PliTag::THICK_QUADRATIC_CHAIN_GOBJ:
      createStroke((ThickQuadraticChainTag *)imageTag->m_object[i],
                   outVectImage, strokeData);
      break;

    case PliTag::COLOR_NGOBJ: {
      ColorTag *colorTag = (ColorTag *)imageTag->m_object[i];
      assert(colorTag->m_numColors == 1);
      strokeData.m_styleId = colorTag->m_color[0];
      break;
    }

    case PliTag::OUTLINE_OPTIONS_GOBJ:
      strokeData.m_options =
          ((StrokeOutlineOptionsTag *)imageTag->m_object[i])->m_options;
      break;
      
    case PliTag::AUTOCLOSE_TOLERANCE_GOBJ: {
      AutoCloseToleranceTag *toleranceTag =
          (AutoCloseToleranceTag *)imageTag->m_object[i];
      assert(toleranceTag->m_autoCloseTolerance >= 0);
      outVectImage->setAutocloseTolerance(
          ((double)toleranceTag->m_autoCloseTolerance) / 1000);
      break;
    }
    default:
      break;
    }
  }

#ifdef _DEBUG
  outVectImage->checkIntersections();
#endif

  outVectImage->findRegions();

  return TImageP(outVectImage);
}

/**
 * Recovery load function - attempts to load as much as possible from corrupted frames
 * Skips invalid objects and continues loading valid ones
 */
TImageP TImageReaderPli::doLoadWithRecovery() {
  CreateStrokeData strokeData;
  std::unique_ptr<TVectorImage> outVectImage(new TVectorImage(true));
  
  // Set autoclose tolerance with fallback to default
  try {
    outVectImage->setAutocloseTolerance(m_lrp->m_pli->getAutocloseTolerance());
  } catch (...) {
    TSystem::outputDebug("Failed to read autoclose tolerance, using default");
    outVectImage->setAutocloseTolerance(1.15); // Default value
  }

  ImageTag* imageTag = nullptr;
  
  // Attempt to load frame tag with error handling
  try {
    imageTag = m_lrp->m_pli->loadFrame(m_frameId);
  } catch (const std::exception& e) {
    TSystem::outputDebug("Failed to load frame tag: " + std::string(e.what()));
    // Return empty but valid image rather than failing completely
    return TImageP(outVectImage.release());
  }

  if (!imageTag) {
    TSystem::outputDebug("No image tag found for frame");
    return TImageP(outVectImage.release());
  }

  // Mark frame as loaded even in recovery mode
  if (m_lrp->m_mapOfImage.find(m_frameId) != m_lrp->m_mapOfImage.end() &&
      !m_lrp->m_mapOfImage[m_frameId].second) {
    m_lrp->m_mapOfImage[m_frameId].second = true;
  }

  // Process objects with individual error handling - skip corrupted ones
  for (UINT i = 0; i < imageTag->m_numObjects; i++) {
    try {
      if (!imageTag->m_object[i]) {
        TSystem::outputDebug("Null object at index " + std::to_string(i) + " - skipping");
        continue;
      }
      
      switch (imageTag->m_object[i]->m_type) {
        case PliTag::GROUP_GOBJ:
          try {
            if (static_cast<GroupTag*>(imageTag->m_object[i])->m_type == GroupTag::STROKE) {
              createGroupWithRecovery(static_cast<GroupTag*>(imageTag->m_object[i]), 
                                    outVectImage.get(), strokeData);
            }
          } catch (const std::exception& e) {
            TSystem::outputDebug("Failed to load group object: " + std::string(e.what()));
          }
          break;

        case PliTag::INTERSECTION_DATA_GOBJ:
          try {
            readRegionVersion4x(static_cast<IntersectionDataTag*>(imageTag->m_object[i]), 
                              outVectImage.get());
          } catch (...) {
            TSystem::outputDebug("Failed to load intersection data - regions may be incomplete");
          }
          break;

        case PliTag::THICK_QUADRATIC_CHAIN_GOBJ:
          try {
            createStrokeWithRecovery(static_cast<ThickQuadraticChainTag*>(imageTag->m_object[i]), 
                                   outVectImage.get(), strokeData);
          } catch (const std::exception& e) {
            TSystem::outputDebug("Failed to load stroke: " + std::string(e.what()));
          }
          break;

        case PliTag::COLOR_NGOBJ: {
          try {
            ColorTag* colorTag = static_cast<ColorTag*>(imageTag->m_object[i]);
            if (colorTag->m_numColors >= 1) {
              strokeData.m_styleId = colorTag->m_color[0];
            } else {
              TSystem::outputDebug("Color tag with no colors - style may be incorrect");
            }
          } catch (...) {
            TSystem::outputDebug("Failed to load color data - using previous style");
          }
          break;
        }

        case PliTag::OUTLINE_OPTIONS_GOBJ:
          try {
            strokeData.m_options = 
              static_cast<StrokeOutlineOptionsTag*>(imageTag->m_object[i])->m_options;
          } catch (...) {
            TSystem::outputDebug("Failed to load outline options - using defaults");
            strokeData.m_options = TStroke::OutlineOptions(); // Reset to defaults
          }
          break;

        case PliTag::AUTOCLOSE_TOLERANCE_GOBJ: {
          try {
            AutoCloseToleranceTag* toleranceTag = 
              static_cast<AutoCloseToleranceTag*>(imageTag->m_object[i]);
            if (toleranceTag->m_autoCloseTolerance >= 0) {
              outVectImage->setAutocloseTolerance(
                static_cast<double>(toleranceTag->m_autoCloseTolerance) / 1000);
            }
          } catch (...) {
            TSystem::outputDebug("Failed to load autoclose tolerance - using current value");
          }
          break;
        }
        default:
          TSystem::outputDebug("Unknown object type: " + 
                              std::to_string(imageTag->m_object[i]->m_type));
          break;
      }
    } catch (const std::exception& e) {
      TSystem::outputDebug("Critical error processing object " + std::to_string(i) + 
                          ": " + std::string(e.what()));
      // Continue with next object despite critical error in this one
    }
  }

  // Attempt to find regions, but don't fail if it doesn't work
  try {
    outVectImage->findRegions();
  } catch (const std::exception& e) {
    TSystem::outputDebug("Failed to find regions: " + std::string(e.what()));
    // Return image anyway - it may still have valid strokes
  }

  return TImageP(outVectImage.release());
}

//-----------------------------------------------------------------------------

TDimension TImageReaderPli::getSize() const { return TDimension(-1, -1); }

//-----------------------------------------------------------------------------

TRect TImageReaderPli::getBBox() const { return TRect(); }

//=============================================================================

TImageWriterPli::TImageWriterPli(const TFilePath &f, const TFrameId &frameId,
                                 TLevelWriterPli *pli)
    : TImageWriter(f)
    , m_frameId(frameId)
    , m_lwp(pli)
    , m_precision(2)
{}

//-----------------------------------------------------------------------------

/**
 * Convert stroke to PLI tags for saving
 */
static void putStroke(TStroke *stroke, int &currStyleId,
                      std::vector<PliObjectTag *> &tags) {
  double maxThickness = 0;
  bool nonStdOutline  = false;
  assert(stroke);

  int chunkCount = stroke->getChunkCount();
  std::vector<TThickQuadratic> strokeChain(chunkCount);

  int styleId = stroke->getStyle();
  assert(styleId >= 0);
  if (currStyleId == -1 || styleId != currStyleId) {
    currStyleId = styleId;
    std::unique_ptr<TUINT32[]> color(new TUINT32[1]);
    color[0] = (TUINT32)styleId;

    std::unique_ptr<ColorTag> colorTag(new ColorTag(
        ColorTag::SOLID, ColorTag::STROKE_COLOR, 1, std::move(color)));
    tags.push_back(colorTag.release());
  }

  // If the outline options are non-standard, add the outline info tag
  TStroke::OutlineOptions &options = stroke->outlineOptions();
  if (options.m_capStyle != TStroke::OutlineOptions::ROUND_CAP ||
      options.m_joinStyle != TStroke::OutlineOptions::ROUND_JOIN ||
      options.m_miterLower != 0.0 || options.m_miterUpper != 4.0) {
    StrokeOutlineOptionsTag *outlineOptionsTag =
        new StrokeOutlineOptionsTag(options);
    tags.push_back((PliObjectTag *)outlineOptionsTag);
    nonStdOutline = true;
  }

  UINT k;
  for (k = 0; k < (UINT)chunkCount; ++k) {
    const TThickQuadratic *q = stroke->getChunk(k);
    maxThickness =
        std::max({maxThickness, q->getThickP0().thick, q->getThickP1().thick});
    strokeChain[k] = *q;
  }
  maxThickness = std::max(maxThickness,
                          stroke->getChunk(chunkCount - 1)->getThickP2().thick);

  ThickQuadraticChainTag *quadChainTag =
      new ThickQuadraticChainTag(k, &strokeChain[0], maxThickness);
  quadChainTag->m_isLoop = stroke->isSelfLoop();

  tags.push_back((PliObjectTag *)quadChainTag);

  // Restore default outline settings if we changed them
  if (nonStdOutline) {
    TStroke::OutlineOptions resetoptions;
    StrokeOutlineOptionsTag *outlineOptionsTag =
        new StrokeOutlineOptionsTag(resetoptions);
    tags.push_back((PliObjectTag *)outlineOptionsTag);
  }
}

//-----------------------------------------------------------------------------
// Forward declaration
GroupTag *makeGroup(TVectorImageP &vi, int &currStyleId, int &index,
                    int currDepth);

/**
 * Save image to PLI format
 */
void TImageWriterPli::save(const TImageP &img) {
  TVectorImageP tempVecImg = img;
  int currStyleId          = -1;
  if (!tempVecImg) throw TImageException(m_path, "No data to save");

  // Increment frame counter
  ++m_lwp->m_frameNumber;

  std::unique_ptr<IntersectionBranch[]> v;
  UINT intersectionSize = tempVecImg->getFillData(v);

  // Initialize PLI structure if not already done
  if (!m_lwp->m_pli) {
    m_lwp->m_pli.reset(new ParsedPli(m_lwp->m_frameNumber, m_precision, 40,
                                     tempVecImg->getAutocloseTolerance()));
    m_lwp->m_pli->setCreator(m_lwp->m_creator);
  }

  buildPalette(m_lwp->m_pli.get(), img);

  ParsedPli *pli = m_lwp->m_pli.get();

  pli->setFrameCount(m_lwp->m_frameNumber);

  std::vector<PliObjectTag *> tags;

  // Store precision scale
  {
    int precisionScale    = sq(128);
    pli->precisionScale() = precisionScale;

    PliTag *tag = new PrecisionScaleTag(precisionScale);
    tags.push_back((PliObjectTag *)tag);
  }

  // Update version if multiple suffixes are supported
  if (!TFilePath::useStandard()) pli->setVersion(150, 0);
  
  // Store autoclose tolerance if different from default
  double pliTolerance = m_lwp->m_pli->getAutocloseTolerance();
  if (!areAlmostEqual(tempVecImg->getAutocloseTolerance(), 1.15, 0.001) ||
      !areAlmostEqual(pliTolerance, 1.15, 0.001)) {
    int tolerance =
        (int)((roundf(tempVecImg->getAutocloseTolerance() * 100) / 100) * 1000);
    PliTag *tag = new AutoCloseToleranceTag(tolerance);
    tags.push_back((PliObjectTag *)tag);
    pli->setVersion(120, 0);
  } else {
    pli->setVersion(71, 0);
  }
  
  // Process all strokes in the image
  int numStrokes = tempVecImg->getStrokeCount();
  int i = 0;
  while (i < (UINT)numStrokes) {
    if (tempVecImg->isStrokeGrouped(i))
      tags.push_back(makeGroup(tempVecImg, currStyleId, i, 1));
    else
      putStroke(tempVecImg->getStroke(i++), currStyleId, tags);
  }

  // Add intersection data if present
  if (intersectionSize > 0) {
    PliTag *tag = new IntersectionDataTag(intersectionSize, std::move(v));
    tags.push_back((PliObjectTag *)tag);
  }

  int tagsSize = tags.size();
  std::unique_ptr<ImageTag> imageTagPtr(new ImageTag(
      m_frameId, tagsSize, (tagsSize > 0) ? tags.data() : nullptr));

  pli->addTag(imageTagPtr.release());

  return;
}

//=============================================================================
/**
 * Level writer for PLI format - writes entire level to PLI file
 */
TLevelWriterPli::TLevelWriterPli(const TFilePath &path, TPropertyGroup *winfo)
    : TLevelWriter(path, winfo), m_frameNumber(0) {}

//-----------------------------------------------------------------------------

TLevelWriterPli::~TLevelWriterPli() {
  if (!m_pli) {
    return;
  }
  try {
    // Add palette tag
    CurrStyle = NULL;
    assert(!m_pli->m_palette_tags.empty());
    std::unique_ptr<GroupTag> groupTag(
        new GroupTag(GroupTag::PALETTE, m_pli->m_palette_tags.size(),
                     m_pli->m_palette_tags.data()));
    m_pli->addTag(groupTag.release(), true);
    
    // Add content history if present
    if (m_contentHistory) {
      QString his = m_contentHistory->serialize();
      std::unique_ptr<TextTag> textTag(new TextTag(his.toStdString()));
      m_pli->addTag(textTag.release(), true);
    }
    
    // Write the complete PLI file
    m_pli->writePli(m_path);
  } catch (...) {
    // Swallow exceptions during destruction to prevent termination
    TSystem::outputDebug("Exception during PLI file finalization");
  }
}

//-----------------------------------------------------------------------------

TImageWriterP TLevelWriterPli::getFrameWriter(TFrameId fid) {
  TImageWriterPli *iwm = new TImageWriterPli(m_path, fid, this);
  return TImageWriterP(iwm);
}

//=============================================================================

/**
 * Level reader for PLI format with enhanced corruption recovery
 */
TLevelReaderPli::TLevelReaderPli(const TFilePath &path)
    : TLevelReader(path)
    , m_palette(nullptr)
    , m_paletteCount(0)
    , m_doesExist(false)
    , m_pli(nullptr)
    , m_readPalette(true)
    , m_level()
    , m_init(false) {
  if (!(m_doesExist = TFileStatus(path).doesExist()))
    throw TImageException(getFilePath(), "Error file doesn't exist");
}

//-----------------------------------------------------------------------------

TLevelReaderPli::~TLevelReaderPli() { delete m_pli; }

//-----------------------------------------------------------------------------

TImageReaderP TLevelReaderPli::getFrameReader(TFrameId fid) {
  TImageReaderPli *irm = new TImageReaderPli(m_path, fid, this);
  return TImageReaderP(irm);
}

//-----------------------------------------------------------------------------

/**
 * Read palette from PLI file with error recovery
 * Creates basic palette even if palette data is corrupted
 */
TPalette *readPalette(GroupTag *paletteTag, int majorVersion,
                      int minorVersion) {
  bool newPli = (majorVersion > 5 || (majorVersion == 5 && minorVersion >= 6));

  TPalette *palette = new TPalette();
  // Remove default style #1 from page
  palette->getPage(0)->removeStyle(1);
  int frame = -1;

  bool pagesRead = false;

  // First two style tags in palette are special:
  // First may contain reference image path
  // Second contains page names
  for (unsigned int i = 0; i < paletteTag->m_numObjects; i++) {
    // Skip null objects in palette
    if (!paletteTag->m_object[i]) {
      TSystem::outputDebug("Null object in palette - skipping");
      continue;
    }
    
    StyleTag *styleTag = (StyleTag *)paletteTag->m_object[i];

    // First tag might be reference image path
    if (i == 0 && styleTag->m_numParams == 1 &&
        strncmp(styleTag->m_param[0].m_string.c_str(), "refimage", 8) == 0) {
      try {
        palette->setRefImgPath(
            TFilePath(styleTag->m_param[0].m_string.c_str() + 8));
      } catch (...) {
        TSystem::outputDebug("Failed to set reference image path");
      }
      continue;
    }

    // Safety check for tag type
    if (styleTag->m_type != PliTag::STYLE_NGOBJ) {
      TSystem::outputDebug("Unexpected tag type in palette - skipping");
      continue;
    }
    
    int id        = styleTag->m_id;
    int pageIndex = styleTag->m_pageIndex;
    
    // Read page names from special style tag
    if (!pagesRead && newPli) {
      pagesRead = true;
      // Safety checks for page names tag
      if (id == 0 && pageIndex == 0 && palette->getPageCount() == 1) {
        for (int j = 0; j < styleTag->m_numParams; j++) {
          try {
            if (styleTag->m_param[j].m_type == TStyleParam::SP_STRING) {
              if (j == 0)
                palette->getPage(0)->setName(
                    ::to_wstring(styleTag->m_param[j].m_string));
              else {
                palette->addPage(::to_wstring(styleTag->m_param[j].m_string));
              }
            }
          } catch (...) {
            TSystem::outputDebug("Failed to read page name at index " + std::to_string(j));
          }
        }
      }
      continue;
    }
    
    // Empty style tag indicates palette keyframe
    if (styleTag->m_numParams == 0) {
      frame = styleTag->m_id;
      try {
        palette->setFrame(frame);
      } catch (...) {
        TSystem::outputDebug("Failed to set palette frame");
        frame = -1;
      }
      continue;
    }
    
    TPalette::Page *page = nullptr;

    // Skip colors not in any page (special value 65535)
    if (pageIndex < 65535) {
      try {
        page = palette->getPage(pageIndex);
      } catch (...) {
        TSystem::outputDebug("Invalid page index: " + std::to_string(pageIndex));
        page = nullptr;
      }
    } else {
      continue;
    }

    if (!page) {
      TSystem::outputDebug("Page not found for index: " + std::to_string(pageIndex));
      continue;
    }

    // Load style parameters with error handling
    std::vector<TStyleParam> params(styleTag->m_numParams);
    for (int j = 0; j < styleTag->m_numParams; j++)
      params[j] = styleTag->m_param[j];

    try {
      PliInputStream chan(&params, majorVersion, minorVersion);
      TColorStyle *style = TColorStyle::load(chan);
      
      if (!style) {
        TSystem::outputDebug("Failed to load color style for ID: " + std::to_string(id));
        continue;
      }
      
      assert(id > 0);
      if (id < palette->getStyleCount()) {
        if (frame > -1) {
          try {
            TColorStyle *oldStyle = palette->getStyle(id);
            if (oldStyle) {
              oldStyle->copy(*style);
              palette->setKeyframe(id, frame);
            }
          } catch (...) {
            TSystem::outputDebug("Failed to update style keyframe");
          }
        } else {
          palette->setStyle(id, style);
        }
      } else {
        // New style - should not happen for animated palettes
        assert(frame == -1);
        try {
          while (palette->getStyleCount() < id) palette->addStyle(TPixel32::Red);
          if (page)
            page->addStyle(palette->addStyle(style));
          else
            palette->addStyle(style);
        } catch (...) {
          TSystem::outputDebug("Failed to add new style to palette");
          delete style;
        }
      }
      
      // Add style to page if not a keyframe
      if (id > 0 && page && frame == -1) {
        try {
          page->addStyle(id);
        } catch (...) {
          TSystem::outputDebug("Failed to add style to page");
        }
      }
    } catch (const std::exception& e) {
      TSystem::outputDebug("Error loading style " + std::to_string(id) + 
                          ": " + std::string(e.what()));
      // Continue with next style rather than failing entire palette
    }
  }
  
  // Reset to frame 0
  try {
    palette->setFrame(0);
  } catch (...) {
    TSystem::outputDebug("Failed to reset palette frame");
  }
  
  return palette;
}

/*
 * Control whether to read palette during level loading
 */
void TLevelReaderPli::doReadPalette(bool doReadIt) { m_readPalette = doReadIt; }

QString TLevelReaderPli::getCreator() {
  loadInfo();

  if (m_pli) return m_pli->getCreator();

  return "";
}

/**
 * Load level information with enhanced error recovery
 * Attempts to load as much information as possible from corrupted files
 */
TLevelP TLevelReaderPli::loadInfo() {
  if (m_init) return m_level;

  m_init = true;

  try {
    assert(!m_pli);
    m_pli = new ParsedPli(getFilePath());
    UINT majorVersionNumber, minorVersionNumber;
    m_pli->getVersion(majorVersionNumber, minorVersionNumber);
    
    // Allow loading of older versions with warnings
    if (majorVersionNumber <= 5 && (majorVersionNumber != 5 || minorVersionNumber < 5)) {
      TSystem::outputDebug("Loading legacy PLI version: " + 
                          std::to_string(majorVersionNumber) + "." + 
                          std::to_string(minorVersionNumber) + 
                          " - some features may be limited");
    }
    
    TPalette *palette = nullptr;
    try {
      m_pli->loadInfo(m_readPalette, palette, m_contentHistory);
    } catch (const TException& e) {
      QString msg = QString::fromStdString(::to_string(e.getMessage()));
      if (msg.contains("Not all frames loaded")) {
        m_level->setPartialLoad(true);
        TSystem::outputDebug("Partial load detected: " + msg.toStdString());
      } else {
        TSystem::outputDebug("Error loading palette info: " + 
                            std::string(e.getMessage()));
      }
      // Create default palette if loading failed
      palette = new TPalette();
    } catch (const std::exception& e) {
      TSystem::outputDebug("Failed to load palette info: " + std::string(e.what()));
      palette = new TPalette(); // Fallback palette
    } catch (...) {
      TSystem::outputDebug("Unknown error loading palette info");
      palette = new TPalette(); // Fallback palette
    }

    if (palette) {
      m_level->setPalette(palette);
    }

    // Load available frames, skip corrupted ones
    int loadedFrames = 0;
    for (int i = 0; i < m_pli->getFrameCount(); i++) {
      TFrameId frameId = m_pli->getFrameNumber(i);
      try {
        // Verify frame is loadable before adding to level
        if (m_pli->isFrameLoadable(frameId)) {
          m_level->setFrame(frameId, TVectorImageP());
          loadedFrames++;
        } else {
          TSystem::outputDebug("Frame " + frameId.expand() + " is not loadable - skipping");
        }
      } catch (const std::exception& e) {
        TSystem::outputDebug("Frame " + frameId.expand() + 
                            " may be corrupted: " + std::string(e.what()));
        // Add frame anyway for recovery attempt during individual load
        m_level->setFrame(frameId, TVectorImageP());
        loadedFrames++;
      }
    }
    
    // Mark level as partial load if we couldn't load all frames
    if (loadedFrames < m_pli->getFrameCount()) {
      m_level->setPartialLoad(true);
      TSystem::outputDebug("Partial load: " + std::to_string(loadedFrames) + 
                          "/" + std::to_string(m_pli->getFrameCount()) + 
                          " frames loaded");
    }
    
  } catch (std::exception &e) {
    TSystem::outputDebug("Critical error loading PLI info: " + std::string(e.what()));
    
    // Create empty level as fallback rather than complete failure
    m_level = TLevelP();
    if (m_pli) {
      delete m_pli;
      m_pli = nullptr;
    }
    
    throw TImageException(getFilePath(), 
                         "File may be partially corrupted: " + std::string(e.what()));
  } catch (...) {
    TSystem::outputDebug("Unknown critical error loading PLI file");
    m_level = TLevelP();
    if (m_pli) {
      delete m_pli;
      m_pli = nullptr;
    }
    throw;
  }

  return m_level;
}

//-----------------------------------------------------------------------------

TImageReaderPli::TImageReaderPli(const TFilePath &f, const TFrameId &frameId,
                                 TLevelReaderPli *pli)
    : TImageReader(f), m_frameId(frameId), m_lrp(pli) {}

//=============================================================================

/**
 * Create group structure from vector image strokes
 * Recursively processes nested groups
 */
GroupTag *makeGroup(TVectorImageP &vi, int &currStyleId, int &index,
                    int currDepth) {
  std::vector<PliObjectTag *> tags;
  int i = index;
  while (i < (UINT)vi->getStrokeCount() &&
         vi->getCommonGroupDepth(i, index) >= currDepth) {
    int strokeDepth = vi->getGroupDepth(i);
    if (strokeDepth == currDepth)
      putStroke(vi->getStroke(i++), currStyleId, tags);
    else if (strokeDepth > currDepth)
      tags.push_back(makeGroup(vi, currStyleId, i, currDepth + 1));
    else
      assert(false);
  }
  index = i;
  return new GroupTag(GroupTag::STROKE, tags.size(), tags.data());
}

//=============================================================================