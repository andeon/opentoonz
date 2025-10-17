#pragma once

#ifndef PLI_IO_H
#define PLI_IO_H

#ifdef _MSC_VER
#pragma warning(disable: 4661)
#pragma warning(disable: 4018)
#endif

#include <memory>
#include <vector>

#include "tfilepath.h"
#include "tvectorimage.h"
#include "tstroke.h"

#include <QString>

/*=====================================================================
This header file defines classes for parsing the PLI format used in the
paperless consortium. The classes are designed based on the file format
described in the related paperless document.
=====================================================================*/

#include "traster.h"
#include "tcurves.h"

/*=====================================================================
Utility macros for exporting classes from the DLL
(DVAPI: Digital Video Application Program Interface)
=====================================================================*/

/*=====================================================================
Base class for PLI tags
=====================================================================*/
/*!
  The base class for the various tags that make up the PLI format.
  Refer to the documentation for details on each tag type.
  Tags can be extracted from the ParsedPli class using the getFirstTag
  and getNextTag methods. The tag type is stored in the m_type member.
*/
class PliTag {
public:
  enum Type {
    NONE      = -1,
    END_CNTRL = 0,
    SET_DATA_8_CNTRL,
    SET_DATA_16_CNTRL,
    SET_DATA_32_CNTRL,
    TEXT,
    PALETTE,
    PALETTE_WITH_ALPHA,
    DUMMY1,
    DUMMY2,
    DUMMY3,
    THICK_QUADRATIC_CHAIN_GOBJ,
    DUMMY4,
    DUMMY5,
    BITMAP_GOBJ,
    GROUP_GOBJ,
    TRANSFORMATION_GOBJ,
    IMAGE_GOBJ,
    COLOR_NGOBJ,
    GEOMETRIC_TRANSFORMATION_GOBJ,
    DOUBLEPAIR_OBJ,
    STYLE_NGOBJ,
    INTERSECTION_DATA_GOBJ,
    IMAGE_BEGIN_GOBJ,
    THICK_QUADRATIC_LOOP_GOBJ,
    OUTLINE_OPTIONS_GOBJ,
    PRECISION_SCALE_GOBJ,
    AUTOCLOSE_TOLERANCE_GOBJ,
    HOW_MANY_TAG_TYPES
  };

  Type m_type;

  PliTag();
  PliTag(Type type);
  virtual ~PliTag() = default;
};

//=====================================================================

class TStyleParam {
public:
  enum Type {
    SP_NONE = 0,
    SP_BYTE,
    SP_INT,
    SP_DOUBLE,
    SP_USHORT,
    SP_RASTER,
    SP_STRING,
    SP_HOWMANY
  };

  Type m_type;
  double m_numericVal;
  TRaster32P m_r;
  std::string m_string;

  TStyleParam() : m_type(SP_NONE), m_numericVal(0), m_r(), m_string() {}
  TStyleParam(const TStyleParam &styleParam)
      : m_type(styleParam.m_type),
        m_numericVal(styleParam.m_numericVal),
        m_r(styleParam.m_r),
        m_string(styleParam.m_string) {}
  TStyleParam(double x) : m_type(SP_DOUBLE), m_numericVal(x), m_r(), m_string() {}
  TStyleParam(int x) : m_type(SP_INT), m_numericVal(x), m_r(), m_string() {}
  TStyleParam(BYTE x) : m_type(SP_BYTE), m_numericVal(x), m_r(), m_string() {}
  TStyleParam(USHORT x) : m_type(SP_USHORT), m_numericVal(x), m_r(), m_string() {}
  TStyleParam(const TRaster32P &x) : m_type(SP_RASTER), m_numericVal(0), m_r(x), m_string() {}
  TStyleParam(const std::string &x) : m_type(SP_STRING), m_numericVal(0), m_r(), m_string(x) {}

  UINT getSize();
};

/*=====================================================================
Subclasses for the hierarchical structure of tags
=====================================================================*/

class PliObjectTag : public PliTag {
protected:
  PliObjectTag();
  PliObjectTag(Type type);
};

/*=====================================================================*/

class PliGeometricTag : public PliObjectTag {
protected:
  PliGeometricTag();
  PliGeometricTag(Type type);
};

/*=====================================================================
Concrete tag classes; their structures match the PLI file format and
the description in the PLI specification document
=====================================================================*/

class TextTag final : public PliObjectTag {
public:
  std::string m_text;

  TextTag();
  TextTag(const TextTag &textTag);
  TextTag(const std::string &text);
};

/*=====================================================================
Palette tag for storing color information
=====================================================================*/
class PaletteTag final : public PliTag {
public:
  TUINT32 m_numColors;
  TPixelRGBM32 *m_color;

  PaletteTag();
  PaletteTag(TUINT32 numColors, TPixelRGBM32 *color);
  PaletteTag(const PaletteTag &paletteTag);
  ~PaletteTag();

  bool setColor(TUINT32 index, const TPixelRGBM32 &color);
};

/*=====================================================================
Palette tag with alpha channel support
=====================================================================*/
class PaletteWithAlphaTag final : public PliTag {
public:
  TUINT32 m_numColors;
  TPixelRGBM32 *m_color;

  PaletteWithAlphaTag();
  PaletteWithAlphaTag(TUINT32 numColors);
  PaletteWithAlphaTag(TUINT32 numColors, TPixelRGBM32 *color);
  PaletteWithAlphaTag(PaletteWithAlphaTag &paletteTag);
  ~PaletteWithAlphaTag();

  bool setColor(TUINT32 index, const TPixelRGBM32 &color);
};

/*=====================================================================
All geometric tags that contain curve information are instantiations
of this class
=====================================================================*/
class ThickQuadraticChainTag final : public PliGeometricTag {
public:
  TUINT32 m_numCurves;
  std::unique_ptr<TThickQuadratic[]> m_curve;
  bool m_isLoop;
  double m_maxThickness;
  TStroke::OutlineOptions m_outlineOptions;

  ThickQuadraticChainTag()
      : PliGeometricTag(THICK_QUADRATIC_CHAIN_GOBJ),
        m_numCurves(0),
        m_maxThickness(1) {}

  ThickQuadraticChainTag(TUINT32 numCurves, const TThickQuadratic *curve, double maxThickness)
      : PliGeometricTag(THICK_QUADRATIC_CHAIN_GOBJ),
        m_numCurves(numCurves),
        m_maxThickness(maxThickness <= 0 ? 1 : maxThickness) {
    if (m_numCurves > 0) {
      m_curve = std::make_unique<TThickQuadratic[]>(m_numCurves);
      for (UINT i = 0; i < m_numCurves; i++) {
        m_curve[i] = curve[i];
      }
    }
  }

  ThickQuadraticChainTag(const ThickQuadraticChainTag &chainTag)
      : PliGeometricTag(THICK_QUADRATIC_CHAIN_GOBJ),
        m_numCurves(chainTag.m_numCurves),
        m_maxThickness(chainTag.m_maxThickness),
        m_isLoop(chainTag.m_isLoop),
        m_outlineOptions(chainTag.m_outlineOptions) {
    if (m_numCurves > 0) {
      m_curve = std::make_unique<TThickQuadratic[]>(m_numCurves);
      for (UINT i = 0; i < m_numCurves; i++) {
        m_curve[i] = chainTag.m_curve[i];
      }
    }
  }

private:
  // Assignment operator is not implemented
  ThickQuadraticChainTag &operator=(const ThickQuadraticChainTag &) = delete;
};

/*=====================================================================
Bitmap tag (not yet implemented)
=====================================================================*/
class BitmapTag final : public PliGeometricTag {
public:
  enum compressionType { NONE = 0, RLE, HOW_MANY_COMPRESSION };

  TRaster32P m_r;

  BitmapTag();
  BitmapTag(const TRaster32P &r);
  BitmapTag(const BitmapTag &bitmap);
  ~BitmapTag();
};

/*=====================================================================
Color tag (not yet implemented)
=====================================================================*/
class ColorTag final : public PliObjectTag {
public:
  enum styleType {
    STYLE_NONE = 0,
    SOLID,
    LINEAR_GRADIENT,
    RADIAL_GRADIENT,
    STYLE_HOW_MANY
  };
  enum attributeType {
    ATTRIBUTE_NONE = 0,
    EVENODD_LOOP_FILL,
    DIRECTION_LOOP_FILL,
    STROKE_COLOR,
    LEFT_STROKE_COLOR,
    RIGHT_STROKE_COLOR,
    ATTRIBUTE_HOW_MANY
  };

  styleType m_style;
  attributeType m_attribute;
  TUINT32 m_numColors;
  std::unique_ptr<TUINT32[]> m_color;

  ColorTag();
  ColorTag(styleType style, attributeType attribute, TUINT32 numColors, std::unique_ptr<TUINT32[]> color);
  ColorTag(const ColorTag &colorTag);
  ~ColorTag();
};

/*=====================================================================
Style tag for defining style parameters
=====================================================================*/
class StyleTag final : public PliObjectTag {
public:
  USHORT m_id;
  USHORT m_pageIndex;
  int m_numParams;
  std::unique_ptr<TStyleParam[]> m_param;

  StyleTag();
  StyleTag(int id, USHORT pagePaletteIndex, int numParams, TStyleParam *params);
  StyleTag(const StyleTag &styleTag);
  ~StyleTag();
};

/*=====================================================================
Geometric transformation tag for applying affine transformations
=====================================================================*/
class GeometricTransformationTag final : public PliGeometricTag {
public:
  TAffine m_affine;
  PliGeometricTag *m_object;

  GeometricTransformationTag();
  GeometricTransformationTag(const TAffine &affine, PliGeometricTag *object);
  GeometricTransformationTag(const GeometricTransformationTag &transformationTag);
  ~GeometricTransformationTag();
};

/*=====================================================================
Group tag for grouping multiple objects
=====================================================================*/
class GroupTag final : public PliObjectTag {
public:
  enum {
    NONE = 0,
    STROKE,
    SKETCH_STROKE,
    LOOP,
    FILL_SEED,  // Consists of 1 ColorTag and 1 pointTag
    PALETTE,
    TYPE_HOW_MANY
  };

  UCHAR m_type;
  TUINT32 m_numObjects;
  std::unique_ptr<PliObjectTag *[]> m_object;

  GroupTag();
  GroupTag(UCHAR type, TUINT32 numObjects, PliObjectTag **object);
  GroupTag(UCHAR type, TUINT32 numObjects, std::unique_ptr<PliObjectTag *[]> object);
  GroupTag(const GroupTag &groupTag);
  ~GroupTag();
};

/*=====================================================================
Image tag for storing frame-specific objects
=====================================================================*/
class ImageTag final : public PliObjectTag {
public:
  TFrameId m_numFrame;
  TUINT32 m_numObjects;
  std::unique_ptr<PliObjectTag *[]> m_object;

  ImageTag(const TFrameId &numFrame, TUINT32 numObjects, PliObjectTag **object);
  ImageTag(const TFrameId &frameId, TUINT32 numObjects, std::unique_ptr<PliObjectTag *[]> object);
  ImageTag(const ImageTag &imageTag);
  ~ImageTag();
};

/*=====================================================================
Double pair tag for storing coordinate pairs
=====================================================================*/
class DoublePairTag final : public PliObjectTag {
public:
  double m_first, m_second;

  DoublePairTag();
  DoublePairTag(double x, double y);
  DoublePairTag(const DoublePairTag &pointTag);
  ~DoublePairTag();
};

/*=====================================================================
Intersection data tag for storing intersection information
=====================================================================*/
class IntersectionDataTag final : public PliObjectTag {
public:
  UINT m_branchCount;
  std::unique_ptr<TVectorImage::IntersectionBranch[]> m_branchArray;

  IntersectionDataTag();
  IntersectionDataTag(UINT branchCount, std::unique_ptr<TVectorImage::IntersectionBranch[]> branchArray);
  IntersectionDataTag(const IntersectionDataTag &tag);
  ~IntersectionDataTag();
};

/*=====================================================================
Stroke outline options tag for defining stroke outline properties
=====================================================================*/
class StrokeOutlineOptionsTag final : public PliObjectTag {
public:
  TStroke::OutlineOptions m_options;

  StrokeOutlineOptionsTag();
  StrokeOutlineOptionsTag(const TStroke::OutlineOptions &options);
};

/*=====================================================================
Precision scale tag for defining precision scaling
=====================================================================*/
class PrecisionScaleTag final : public PliObjectTag {
public:
  int m_precisionScale;

  PrecisionScaleTag();
  PrecisionScaleTag(int precisionScale);
};

/*=====================================================================
Autoclose tolerance tag for defining autoclose tolerance
=====================================================================*/
class AutoCloseToleranceTag final : public PliObjectTag {
public:
  int m_autoCloseTolerance;

  AutoCloseToleranceTag();
  AutoCloseToleranceTag(int tolerance);
};

/*=====================================================================
ParsedPli class for storing parsed PLI file information during reading
(reading is performed via the constructor) and for writing (using writePli).
The implementation is opaque at this level through the ParsedPliImp class.
=====================================================================*/
class ParsedPliImp;
class TFilePath;
class TContentHistory;

class ParsedPli {
protected:
  ParsedPliImp *imp;

public:
  void setFrameCount(int);

  ParsedPli();
  ParsedPli(const TFilePath &filename, bool readInfo = false);
  ParsedPli(USHORT framesNumber, UCHAR precision, UCHAR maxThickness, double autocloseTolerance);
  ~ParsedPli();

  QString getCreator() const;
  void setCreator(const QString &creator);

  void getVersion(UINT &majorVersionNumber, UINT &minorVersionNumber) const;
  void setVersion(UINT majorVersionNumber, UINT minorVersionNumber);
  bool addTag(PliTag *tag, bool addFront = false);

  void loadInfo(bool readPalette, TPalette *&palette, TContentHistory *&history);
  ImageTag *loadFrame(const TFrameId &frameId);
  const TFrameId &getFrameNumber(int index);
  int getFrameCount() const;

  double getThickRatio() const;
  double getMaxThickness() const;
  void setMaxThickness(double maxThickness);
  double getAutocloseTolerance() const;
  void setAutocloseTolerance(int tolerance);
  int &precisionScale();

  // Stores the global palette tags
  std::vector<PliObjectTag *> m_palette_tags;

  // Iterates over the tag list
  // Example: for (PliTag *tag = getFirstTag(); tag; tag = getNextTag()) {}
  PliTag *getFirstTag();
  PliTag *getNextTag();

  bool writePli(const TFilePath &filename);

  // Checks if a specific frame can be loaded
  bool isFrameLoadable(TFrameId frame) const {
    for (int i = 0; i < getFrameCount(); ++i) {
      if (getFrameNumber(i) == frame) return true;
    }
    return false;
  }
};

#endif