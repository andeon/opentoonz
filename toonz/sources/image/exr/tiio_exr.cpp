#define TINYEXR_USE_MINIZ 0
#include "zlib.h"

#define TINYEXR_OTMOD_IMPLEMENTATION
#include "tinyexr_otmod.h"

#include "tiio_exr.h"
#include "tpixel.h"

#include <QMap>
#include <QString>
#include <algorithm>  // std::min

namespace {

// ---------------------------------------------------------------------
// Conversion utilities (NO gamma for EXR – EXR is always linear)
// ---------------------------------------------------------------------

// Convert float (linear) → unsigned char (8-bit) with clamp + rounding
inline unsigned char float_to_uchar(float f) {
  int i = static_cast<int>(f * 255.0f + 0.5f);
  return static_cast<unsigned char>(i < 0 ? 0 : (i > 255 ? 255 : i));
}

// Convert unsigned char (8-bit) → float (linear)
inline float uchar_to_float(unsigned char uc) { return uc / 255.0f; }

// Convert float (linear) → unsigned short (16-bit) with clamp + rounding
inline unsigned short float_to_ushort(float f) {
  int i = static_cast<int>(f * 65535.0f + 0.5f);
  return static_cast<unsigned short>(i < 0 ? 0 : (i > 65535 ? 65535 : i));
}

// Convert unsigned short (16-bit) → float (linear)
inline float ushort_to_float(unsigned short us) { return us / 65535.0f; }

// ---------------------------------------------------------------------
// UI strings for compression types
// ---------------------------------------------------------------------
const QMap<int, std::wstring> ExrCompTypeStr = {
    {TINYEXR_COMPRESSIONTYPE_NONE, L"None"},
    {TINYEXR_COMPRESSIONTYPE_RLE, L"RLE"},
    {TINYEXR_COMPRESSIONTYPE_ZIPS, L"ZIPS"},
    {TINYEXR_COMPRESSIONTYPE_ZIP, L"ZIP"},
    {TINYEXR_COMPRESSIONTYPE_PIZ, L"PIZ"}};

const std::wstring EXR_STORAGETYPE_SCANLINE = L"Store Image as Scanlines";
const std::wstring EXR_STORAGETYPE_TILE     = L"Store Image as Tiles";

}  // namespace

//**************************************************************************
//    ExrReader implementation
//**************************************************************************

class ExrReader final : public Tiio::Reader {
  float* m_rgbaBuf;         // Full image buffer in float format (RGBA order)
  int m_row;                // Current scanline being processed
  EXRHeader* m_exr_header;  // EXR file header (freed after image load)
  FILE* m_fp;               // File handle for reading
  float m_colorSpaceGamma;  // NOT used for EXR (kept for interface compatibility)

public:
  ExrReader();
  ~ExrReader();

  void open(FILE* file) override;
  Tiio::RowOrder getRowOrder() const override;
  bool read16BitIsEnabled() const override;
  int skipLines(int lineCount) override;
  void readLine(char* buffer, int x0, int x1, int shrink) override;
  void readLine(short* buffer, int x0, int x1, int shrink) override;
  void readLine(float* buffer, int x0, int x1, int shrink) override;

  void loadImage();  // Load entire image into memory
  void setColorSpaceGamma(const double gamma) override {
    m_colorSpaceGamma = static_cast<float>(gamma);
  }
};

ExrReader::ExrReader()
    : m_rgbaBuf(nullptr)
    , m_row(0)
    , m_exr_header(nullptr)
    , m_colorSpaceGamma(1.0f) {}

ExrReader::~ExrReader() {
  if (m_rgbaBuf) free(m_rgbaBuf);
  if (m_exr_header) FreeEXRHeader(m_exr_header);
}

/* --------------------------------------------------------------- */
/* Open EXR file and read header information */
void ExrReader::open(FILE* file) {
  m_fp         = file;
  m_exr_header = new EXRHeader();
  const char* err;
  {
    int ret = LoadEXRHeaderFromFileHandle(*m_exr_header, file, &err);
    if (ret != 0) {
      FreeEXRHeader(m_exr_header);
      delete m_exr_header;
      m_exr_header = nullptr;
      throw(std::string(err));
    }
  }

  // Calculate image dimensions from data window
  m_info.m_lx =
      m_exr_header->data_window.max_x - m_exr_header->data_window.min_x + 1;
  m_info.m_ly =
      m_exr_header->data_window.max_y - m_exr_header->data_window.min_y + 1;

  m_info.m_samplePerPixel = m_exr_header->num_channels;

  // Determine bits per sample (always return float raster → 32-bit)
  m_info.m_bitsPerSample = 32;
}

Tiio::RowOrder ExrReader::getRowOrder() const { return Tiio::TOP2BOTTOM; }

bool ExrReader::read16BitIsEnabled() const { return true; }

int ExrReader::skipLines(int lineCount) {
  m_row += lineCount;
  return lineCount;
}

/* --------------------------------------------------------------- */
/* Load entire EXR image into memory buffer */
void ExrReader::loadImage() {
  assert(!m_rgbaBuf);
  const char* err;
  {
    int ret =
        LoadEXRImageBufFromFileHandle(&m_rgbaBuf, *m_exr_header, m_fp, &err);
    if (ret != 0) {
      FreeEXRHeader(m_exr_header);
      delete m_exr_header;
      m_exr_header = nullptr;
      throw(std::string(err));
    }
  }
  // Header memory is freed after loading image
  FreeEXRHeader(m_exr_header);
  delete m_exr_header;
  m_exr_header = nullptr;
}

// ---------------------------------------------------------------------
// readLine 8-bit (LDR) – convert linear EXR float → sRGB 8-bit
// ---------------------------------------------------------------------
void ExrReader::readLine(char* buffer, int x0, int x1, int shrink) {
  const int pixelSize = 4;
  if (m_row < 0 || m_row >= m_info.m_ly) {
    memset(buffer, 0, (x1 - x0 + 1) * pixelSize);
    m_row++;
    return;
  }

  if (!m_rgbaBuf) loadImage();

  TPixel32* pix = (TPixel32*)buffer + x0;
  float* v      = m_rgbaBuf + m_row * m_info.m_lx * 4 + x0 * 4;

  int width = (x1 - x0) / shrink + 1;

  for (int i = 0; i < width; ++i) {
    pix->r = float_to_uchar(v[0]);
    pix->g = float_to_uchar(v[1]);
    pix->b = float_to_uchar(v[2]);
    pix->m = float_to_uchar(v[3]);  // Alpha is linear

    v += shrink * 4;
    pix += shrink;
  }
  m_row++;
}

// ---------------------------------------------------------------------
// readLine 16-bit (LDR) – convert linear EXR float → 16-bit
// ---------------------------------------------------------------------
void ExrReader::readLine(short* buffer, int x0, int x1, int shrink) {
  const int pixelSize = 8;
  if (m_row < 0 || m_row >= m_info.m_ly) {
    memset(buffer, 0, (x1 - x0 + 1) * pixelSize);
    m_row++;
    return;
  }

  if (!m_rgbaBuf) loadImage();

  TPixel64* pix = (TPixel64*)buffer + x0;
  float* v      = m_rgbaBuf + m_row * m_info.m_lx * 4 + x0 * 4;

  int width = (x1 - x0) / shrink + 1;

  for (int i = 0; i < width; ++i) {
    pix->r = float_to_ushort(v[0]);
    pix->g = float_to_ushort(v[1]);
    pix->b = float_to_ushort(v[2]);
    pix->m = float_to_ushort(v[3]);  // Alpha is linear

    v += shrink * 4;
    pix += shrink;
  }
  m_row++;
}

// ---------------------------------------------------------------------
// readLine float – return linear EXR values directly (HDR)
// ---------------------------------------------------------------------
void ExrReader::readLine(float* buffer, int x0, int x1, int shrink) {
  const int pixelSize = 16;
  if (m_row < 0 || m_row >= m_info.m_ly) {
    memset(buffer, 0, (x1 - x0 + 1) * pixelSize);
    m_row++;
    return;
  }

  if (!m_rgbaBuf) loadImage();

  TPixelF* pix = (TPixelF*)buffer + x0;
  float* v     = m_rgbaBuf + m_row * m_info.m_lx * 4 + x0 * 4;

  int width = (x1 - x0) / shrink + 1;

  for (int i = 0; i < width; ++i) {
    pix->r = v[0];
    pix->g = v[1];
    pix->b = v[2];
    pix->m = v[3];  // Alpha is linear (may be >1.0 in premultiplied cases)

    v += shrink * 4;
    pix += shrink;
  }
  m_row++;
}

//============================================================
//    ExrWriterProperties implementation
//============================================================

Tiio::ExrWriterProperties::ExrWriterProperties()
    : m_compressionType("Compression Type")
    , m_storageType("Storage Type")
    , m_bitsPerPixel("Bits Per Pixel")
    , m_colorSpaceGamma("Color Space Gamma", 0.1, 10.0, 2.2) {
  // internally handles float raster
  m_bitsPerPixel.addValue(L"96(RGB)_HF");    // 3 channels × 16-bit half float
  m_bitsPerPixel.addValue(L"128(RGBA)_HF");  // 4 channels × 16-bit half float
  m_bitsPerPixel.addValue(L"96(RGB)_F");     // 3 channels × 32-bit float
  m_bitsPerPixel.addValue(L"128(RGBA)_F");   // 4 channels × 32-bit float
  m_bitsPerPixel.setValue(L"128(RGBA)_HF");  // Default: RGBA half float

  // Compression options
  m_compressionType.addValue(
      ExrCompTypeStr.value(TINYEXR_COMPRESSIONTYPE_NONE));
  m_compressionType.addValue(ExrCompTypeStr.value(TINYEXR_COMPRESSIONTYPE_RLE));
  m_compressionType.addValue(
      ExrCompTypeStr.value(TINYEXR_COMPRESSIONTYPE_ZIPS));
  m_compressionType.addValue(ExrCompTypeStr.value(TINYEXR_COMPRESSIONTYPE_ZIP));
  m_compressionType.addValue(ExrCompTypeStr.value(TINYEXR_COMPRESSIONTYPE_PIZ));
  m_compressionType.setValue(
      ExrCompTypeStr.value(TINYEXR_COMPRESSIONTYPE_NONE));

  m_storageType.addValue(EXR_STORAGETYPE_SCANLINE);
  m_storageType.addValue(EXR_STORAGETYPE_TILE);
  m_storageType.setValue(EXR_STORAGETYPE_SCANLINE);

  bind(m_bitsPerPixel);
  bind(m_compressionType);
  bind(m_storageType);
  bind(m_colorSpaceGamma);
}

void Tiio::ExrWriterProperties::updateTranslation() {
  m_bitsPerPixel.setQStringName(tr("Bits Per Pixel"));
  m_bitsPerPixel.setItemUIName(L"96(RGB)_HF", tr("48(RGB Half Float)"));
  m_bitsPerPixel.setItemUIName(L"128(RGBA)_HF", tr("64(RGBA Half Float)"));
  m_bitsPerPixel.setItemUIName(L"96(RGB)_F", tr("96(RGB Float)"));
  m_bitsPerPixel.setItemUIName(L"128(RGBA)_F", tr("128(RGBA Float)"));

  m_compressionType.setQStringName(tr("Compression Type"));
  m_compressionType.setItemUIName(
      ExrCompTypeStr.value(TINYEXR_COMPRESSIONTYPE_NONE), tr("No compression"));
  m_compressionType.setItemUIName(
      ExrCompTypeStr.value(TINYEXR_COMPRESSIONTYPE_RLE),
      tr("Run Length Encoding (RLE)"));
  m_compressionType.setItemUIName(
      ExrCompTypeStr.value(TINYEXR_COMPRESSIONTYPE_ZIPS),
      tr("ZIP compression per Scanline (ZIPS)"));
  m_compressionType.setItemUIName(
      ExrCompTypeStr.value(TINYEXR_COMPRESSIONTYPE_ZIP),
      tr("ZIP compression per scanline band (ZIP)"));
  m_compressionType.setItemUIName(
      ExrCompTypeStr.value(TINYEXR_COMPRESSIONTYPE_PIZ),
      tr("PIZ-based wavelet compression (PIZ)"));

  m_storageType.setQStringName(tr("Storage Type"));
  m_storageType.setItemUIName(EXR_STORAGETYPE_SCANLINE, tr("Scan-line based"));
  m_storageType.setItemUIName(EXR_STORAGETYPE_TILE, tr("Tile based"));

  m_colorSpaceGamma.setQStringName(tr("Color Space Gamma"));
}

//============================================================
//    ExrWriter implementation
//============================================================

class ExrWriter final : public Tiio::Writer {
  std::vector<float> m_imageBuf[4];  // Separate buffers for R, G, B, A
  EXRHeader m_header;                // EXR file header
  EXRImage m_image;                  // EXR image data
  int m_row;                         // Current scanline being written
  FILE* m_fp;                        // File handle for writing
  int m_bpp;                         // 96 (RGB) or 128 (RGBA)

public:
  ExrWriter();
  ~ExrWriter();

  void open(FILE* file, const TImageInfo& info) override;
  void writeLine(char* buffer) override;
  void writeLine(short* buffer) override;
  void writeLine(float* buffer) override;

  void flush() override;

  Tiio::RowOrder getRowOrder() const override { return Tiio::TOP2BOTTOM; }
  bool writeAlphaSupported() const override { return m_bpp == 128; }
  bool writeInLinearColorSpace() const override { return true; }
};

ExrWriter::ExrWriter() : m_row(0), m_bpp(96) {}

ExrWriter::~ExrWriter() {
  // Do NOT free m_header.channels / pixel_types manually
  // TinyEXR cleans them with FreeEXRHeader()
}

// ---------------------------------------------------------------------
// Initialize EXR writer and set up header information
// ---------------------------------------------------------------------
void ExrWriter::open(FILE* file, const TImageInfo& info) {
  m_fp   = file;
  m_info = info;

  InitEXRHeader(&m_header);
  InitEXRImage(&m_image);

  if (!m_properties) m_properties = new Tiio::ExrWriterProperties();

  // ----- Parse Bits Per Pixel -----
  TEnumProperty* bitsPerPixel =
      (TEnumProperty*)(m_properties->getProperty("Bits Per Pixel"));
  std::wstring bppStr = bitsPerPixel->getValue();
  m_bpp = (bppStr.find(L"128") != std::wstring::npos) ? 128 : 96;

  // ----- Compression -----
  std::wstring compressionType =
      ((TEnumProperty*)(m_properties->getProperty("Compression Type")))
          ->getValue();
  m_header.compression_type = ExrCompTypeStr.key(compressionType);

  // ----- Storage type (scanline vs tiled) -----
  std::wstring storageType =
      ((TEnumProperty*)(m_properties->getProperty("Storage Type")))->getValue();
  if (storageType == EXR_STORAGETYPE_TILE) {
    m_header.tiled           = 1;
    m_header.tile_size_x     = 128;
    m_header.tile_size_y     = 128;
    m_header.tile_level_mode = TINYEXR_TILE_ONE_LEVEL;
  } else {
    m_header.tiled = 0;
  }

  // ----- Channel count -----
  m_image.num_channels = (m_bpp == 128) ? 4 : 3;

  // Allocate per-channel buffers
  for (int c = 0; c < m_image.num_channels; ++c)
    m_imageBuf[c].resize(m_info.m_lx * m_info.m_ly);

  m_image.width  = m_info.m_lx;
  m_image.height = m_info.m_ly;

  // ----- Channel names (explicit RGBA order) -----
  m_header.num_channels = m_image.num_channels;
  m_header.channels =
      (EXRChannelInfo*)malloc(sizeof(EXRChannelInfo) * m_header.num_channels);

  if (m_bpp == 128) {
    strncpy(m_header.channels[0].name, "R", 255); m_header.channels[0].name[1] = '\0';
    strncpy(m_header.channels[1].name, "G", 255); m_header.channels[1].name[1] = '\0';
    strncpy(m_header.channels[2].name, "B", 255); m_header.channels[2].name[1] = '\0';
    strncpy(m_header.channels[3].name, "A", 255); m_header.channels[3].name[1] = '\0';
  } else {
    strncpy(m_header.channels[0].name, "R", 255); m_header.channels[0].name[1] = '\0';
    strncpy(m_header.channels[1].name, "G", 255); m_header.channels[1].name[1] = '\0';
    strncpy(m_header.channels[2].name, "B", 255); m_header.channels[2].name[1] = '\0';
  }

  // ----- Pixel type (HALF or FLOAT) -----
  int requested_pixel_type =
      (bppStr.find(L"_HF") != std::wstring::npos) ? TINYEXR_PIXELTYPE_HALF
                                                 : TINYEXR_PIXELTYPE_FLOAT;

  m_header.pixel_types = (int*)malloc(sizeof(int) * m_header.num_channels);
  m_header.requested_pixel_types =
      (int*)malloc(sizeof(int) * m_header.num_channels);
  for (int i = 0; i < m_header.num_channels; ++i) {
    m_header.pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;          // input is float
    m_header.requested_pixel_types[i] = requested_pixel_type;  // output type
  }
}

// ---------------------------------------------------------------------
// writeLine 8-bit → convert sRGB 8-bit → linear float
// ---------------------------------------------------------------------
void ExrWriter::writeLine(char* buffer) {
  TPixel32* pix = (TPixel32*)buffer;
  float* r_p = &m_imageBuf[0][m_row * m_info.m_lx];
  float* g_p = &m_imageBuf[1][m_row * m_info.m_lx];
  float* b_p = &m_imageBuf[2][m_row * m_info.m_lx];
  float* a_p = (m_bpp == 128) ? &m_imageBuf[3][m_row * m_info.m_lx] : nullptr;

  for (int x = 0; x < m_info.m_lx; ++x) {
    *r_p++ = uchar_to_float(pix[x].r);
    *g_p++ = uchar_to_float(pix[x].g);
    *b_p++ = uchar_to_float(pix[x].b);
    if (m_bpp == 128) *a_p++ = uchar_to_float(pix[x].m);
  }
  m_row++;
}

// ---------------------------------------------------------------------
// writeLine 16-bit → convert 16-bit → linear float
// ---------------------------------------------------------------------
void ExrWriter::writeLine(short* buffer) {
  TPixel64* pix = (TPixel64*)buffer;
  float* r_p = &m_imageBuf[0][m_row * m_info.m_lx];
  float* g_p = &m_imageBuf[1][m_row * m_info.m_lx];
  float* b_p = &m_imageBuf[2][m_row * m_info.m_lx];
  float* a_p = (m_bpp == 128) ? &m_imageBuf[3][m_row * m_info.m_lx] : nullptr;

  for (int x = 0; x < m_info.m_lx; ++x) {
    *r_p++ = ushort_to_float(pix[x].r);
    *g_p++ = ushort_to_float(pix[x].g);
    *b_p++ = ushort_to_float(pix[x].b);
    if (m_bpp == 128) *a_p++ = ushort_to_float(pix[x].m);
  }
  m_row++;
}

// ---------------------------------------------------------------------
// writeLine float → input is already linear (HDR)
// ---------------------------------------------------------------------
void ExrWriter::writeLine(float* buffer) {
  TPixelF* pix = (TPixelF*)buffer;
  float* r_p = &m_imageBuf[0][m_row * m_info.m_lx];
  float* g_p = &m_imageBuf[1][m_row * m_info.m_lx];
  float* b_p = &m_imageBuf[2][m_row * m_info.m_lx];
  float* a_p = (m_bpp == 128) ? &m_imageBuf[3][m_row * m_info.m_lx] : nullptr;

  for (int x = 0; x < m_info.m_lx; ++x) {
    *r_p++ = pix[x].r;
    *g_p++ = pix[x].g;
    *b_p++ = pix[x].b;
    if (m_bpp == 128) *a_p++ = pix[x].m;
  }
  m_row++;
}

// ---------------------------------------------------------------------
// Flush all scanlines to file
// ---------------------------------------------------------------------
void ExrWriter::flush() {
  if (m_bpp == 128) {
    unsigned char* image_ptr[4] = {
        (unsigned char*)m_imageBuf[0].data(),
        (unsigned char*)m_imageBuf[1].data(),
        (unsigned char*)m_imageBuf[2].data(),
        (unsigned char*)m_imageBuf[3].data()};
    m_image.images = image_ptr;
  } else {
    unsigned char* image_ptr[3] = {
        (unsigned char*)m_imageBuf[0].data(),
        (unsigned char*)m_imageBuf[1].data(),
        (unsigned char*)m_imageBuf[2].data()};
    m_image.images = image_ptr;
  }

  const char* err;
  int ret = SaveEXRImageToFileHandle(&m_image, &m_header, m_fp, &err);
  if (ret != TINYEXR_SUCCESS) {
    FreeEXRHeader(&m_header);
    throw(std::string(err));
  }

  // Clean up TinyEXR header (frees internal arrays)
  FreeEXRHeader(&m_header);
}

//============================================================
//    Factory functions
//============================================================

Tiio::Reader* Tiio::makeExrReader() { return new ExrReader(); }

Tiio::Writer* Tiio::makeExrWriter() { return new ExrWriter(); }