

#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#include <memory>

#include "tmachine.h"
#include "texception.h"
#include "tfilepath.h"
#include "tiio_png.h"
#include "tiio.h"
#include "../compatibility/tfile_io.h"

#include "png.h"

#include "tpixel.h"
#include "tpixelutils.h"

using namespace std;
//------------------------------------------------------------

extern "C" {

static void tnz_abort(jmp_buf, int) {}

static void tnz_error_fun(png_structp pngPtr, png_const_charp error_message) {
  *(int *)png_get_error_ptr(pngPtr) = 0;
}
}

#if !defined(TNZ_LITTLE_ENDIAN)
#error "TNZ_LITTLE_ENDIAN undefined !!"
#endif

//=========================================================
/* Check for the older version of libpng */

#if defined(PNG_LIBPNG_VER)
#if (PNG_LIBPNG_VER < 10527)
extern "C" {
static png_uint_32 png_get_current_row_number(const png_structp png_ptr) {
  /* See the comments in png.h - this is the sub-image row when reading and
   * interlaced image.
   */
  if (png_ptr != NULL) return png_ptr->row_number;

  return PNG_UINT_32_MAX; /* help the app not to fail silently */
}

static png_byte png_get_current_pass_number(const png_structp png_ptr) {
  if (png_ptr != NULL) return png_ptr->pass;
  return 8; /* invalid */
}
}
#endif
#else
#error "PNG_LIBPNG_VER undefined, libpng too old?"
#endif

//=========================================================

inline USHORT mySwap(USHORT val) {
#if TNZ_LITTLE_ENDIAN
  // Correct byte swapping for little-endian systems
  return (val >> 8) | (val << 8);
#else
  return val;
#endif
}

//=========================================================

class PngReader final : public Tiio::Reader {
  FILE *m_chan;
  png_structp m_png_ptr;
  png_infop m_info_ptr, m_end_info_ptr;
  int m_bit_depth, m_color_type, m_interlace_type;
  int m_compression_type, m_filter_type;
  unsigned int m_sig_read;
  int m_y;
  bool m_is16bitEnabled;
  std::unique_ptr<unsigned char[]> m_rowBuffer;
  std::unique_ptr<unsigned char[]> m_tempBuffer;  // Temporary buffer for interlace
  int m_canDelete;
  int m_channels;
  int m_rowBytes;

public:
  PngReader()
      : m_chan(0)
      , m_png_ptr(0)
      , m_info_ptr(0)
      , m_end_info_ptr(0)
      , m_bit_depth(0)
      , m_color_type(0)
      , m_interlace_type(0)
      , m_compression_type(0)
      , m_filter_type(0)
      , m_sig_read(0)
      , m_y(0)
      , m_is16bitEnabled(true)
      , m_canDelete(0)
      , m_channels(0)
      , m_rowBytes(0) {}

  ~PngReader() {
    if (m_canDelete == 1) {
      png_destroy_read_struct(&m_png_ptr, &m_info_ptr, &m_end_info_ptr);
    }
  }

  bool read16BitIsEnabled() const override { return m_is16bitEnabled; }

  void enable16BitRead(bool enabled) override { m_is16bitEnabled = enabled; }

  void open(FILE *file) override {
    try {
      m_chan = file;
    } catch (...) {
      throw TException("Can't open file");
    }

    unsigned char signature[8];  // 1 to 8 bytes
    size_t bytesRead = fread(signature, 1, sizeof signature, m_chan);
    if (bytesRead != sizeof signature) {
      throw TException("Can't read PNG signature");
    }

    bool isPng = !png_sig_cmp(signature, 0, sizeof signature);
    if (!isPng) {
      throw TException("Not a valid PNG file");
    }

    m_png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, &m_canDelete,
                                       tnz_error_fun, 0);
    if (!m_png_ptr) {
      throw TException("Unable to create PNG read structure");
    }

#if defined(PNG_LIBPNG_VER)
#if (PNG_LIBPNG_VER >= 10527)
    png_set_longjmp_fn(m_png_ptr, tnz_abort,
                       sizeof(jmp_buf)); /* ignore all fatal errors */
#endif                                   // (PNG_LIBPNG_VER >= 10527)
#endif                                   // defined(PNG_LIBPNG_VER)

    m_canDelete = 1;
    m_info_ptr  = png_create_info_struct(m_png_ptr);
    if (!m_info_ptr) {
      png_destroy_read_struct(&m_png_ptr, (png_infopp)0, (png_infopp)0);
      throw TException("Unable to create PNG info structure");
    }
    m_end_info_ptr = png_create_info_struct(m_png_ptr);
    if (!m_end_info_ptr) {
      png_destroy_read_struct(&m_png_ptr, &m_info_ptr, (png_infopp)0);
      throw TException("Unable to create PNG end info structure");
    }

    // Set up error handling with setjmp
    if (setjmp(png_jmpbuf(m_png_ptr))) {
      png_destroy_read_struct(&m_png_ptr, &m_info_ptr, &m_end_info_ptr);
      m_png_ptr = nullptr;
      throw TException("PNG initialization failed");
    }

    png_init_io(m_png_ptr, m_chan);

    png_set_sig_bytes(m_png_ptr, sizeof signature);

    png_read_info(m_png_ptr, m_info_ptr);

    if (png_get_valid(m_png_ptr, m_info_ptr, PNG_INFO_pHYs)) {
      png_uint_32 xdpi = png_get_x_pixels_per_meter(m_png_ptr, m_info_ptr);
      png_uint_32 ydpi = png_get_y_pixels_per_meter(m_png_ptr, m_info_ptr);
      m_info.m_dpix    = tround(xdpi * 0.0254);
      m_info.m_dpiy    = tround(ydpi * 0.0254);
    }

    png_uint_32 lx = 0, ly = 0;
    png_get_IHDR(m_png_ptr, m_info_ptr, &lx, &ly, &m_bit_depth, &m_color_type,
                 &m_interlace_type, &m_compression_type, &m_filter_type);
    m_info.m_lx = lx;
    m_info.m_ly = ly;

    m_info.m_bitsPerSample = m_bit_depth;

    if (m_color_type == PNG_COLOR_TYPE_PALETTE) {
      png_set_palette_to_rgb(m_png_ptr);
      png_set_filler(m_png_ptr, 0xFF, PNG_FILLER_AFTER);  // Add alpha if none
    }

    if (m_color_type == PNG_COLOR_TYPE_GRAY && m_bit_depth < 8) {
      png_set_expand_gray_1_2_4_to_8(m_png_ptr);
    }

    if (png_get_valid(m_png_ptr, m_info_ptr, PNG_INFO_tRNS)) {
      png_set_tRNS_to_alpha(m_png_ptr);
    }

    if (m_bit_depth == 16 && !m_is16bitEnabled) {
      png_set_strip_16(m_png_ptr);
    }

    if (m_color_type == PNG_COLOR_TYPE_GRAY ||
        m_color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
      png_set_gray_to_rgb(m_png_ptr);
    }

#if defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR)
    png_set_bgr(m_png_ptr);
    png_set_swap_alpha(m_png_ptr);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
    png_set_bgr(m_png_ptr);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB)
    png_set_swap_alpha(m_png_ptr);
#elif !defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
#error "unknown channel order"
#endif

    // Update info after all transformations
    png_read_update_info(m_png_ptr, m_info_ptr);

    // Refresh variables after update
    m_channels = png_get_channels(m_png_ptr, m_info_ptr);
    m_rowBytes = png_get_rowbytes(m_png_ptr, m_info_ptr);
    png_get_IHDR(m_png_ptr, m_info_ptr, &lx, &ly, &m_bit_depth, &m_color_type,
                 &m_interlace_type, &m_compression_type, &m_filter_type);
    m_info.m_samplePerPixel = m_channels;

    // Allocate row buffer based on updated rowBytes
    m_rowBuffer.reset(new unsigned char[m_rowBytes]);
  }

  void readLine(char *buffer, int x0, int x1, int shrink) override {
    if (setjmp(png_jmpbuf(m_png_ptr))) {
      throw TException("PNG read error");
    }

    if (m_interlace_type == PNG_INTERLACE_ADAM7) {
      readLineInterlace(buffer, x0, x1, shrink);
    } else {
      png_read_row(m_png_ptr, m_rowBuffer.get(), nullptr);
      writeRow(buffer, x0, x1);
    }
    m_y++;
  }

  void readLine(short *buffer, int x0, int x1, int shrink) override {
    if (setjmp(png_jmpbuf(m_png_ptr))) {
      throw TException("PNG read error");
    }

    if (m_interlace_type == PNG_INTERLACE_ADAM7) {
      readLineInterlace(buffer, x0, x1, shrink);
    } else {
      png_read_row(m_png_ptr, m_rowBuffer.get(), nullptr);
      writeRow(buffer, x0, x1);
    }
    m_y++;
  }

  int skipLines(int lineCount) override {
    if (setjmp(png_jmpbuf(m_png_ptr))) {
      throw TException("PNG read error");
    }

    for (int i = 0; i < lineCount; ++i) {
      png_read_row(m_png_ptr, m_rowBuffer.get(), nullptr);
      m_y++;
    }
    return lineCount;
  }

  Tiio::RowOrder getRowOrder() const override { return Tiio::TOP2BOTTOM; }

private:
  void writeRow(char *buffer, int x0, int x1) {
    bool hasAlpha = (m_channels == 4);

    if (m_bit_depth == 16) {
      // 16-bit not supported for char* buffer; assume strip_16 was applied if enabled
      throw TException("16-bit PNG not supported for 8-bit buffer");
    } else {
      TPixel32 *pix = (TPixel32 *)buffer + x0;
      int srcIdx = x0 * m_channels;
      for (int j = x0; j <= x1; ++j, ++pix) {
        if (hasAlpha) {
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB)
          pix->m = m_rowBuffer[srcIdx++];
          pix->r = m_rowBuffer[srcIdx++];
          pix->g = m_rowBuffer[srcIdx++];
          pix->b = m_rowBuffer[srcIdx++];
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
          pix->r = m_rowBuffer[srcIdx++];
          pix->g = m_rowBuffer[srcIdx++];
          pix->b = m_rowBuffer[srcIdx++];
          pix->m = m_rowBuffer[srcIdx++];
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
          pix->b = m_rowBuffer[srcIdx++];
          pix->g = m_rowBuffer[srcIdx++];
          pix->r = m_rowBuffer[srcIdx++];
          pix->m = m_rowBuffer[srcIdx++];
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR)
          pix->m = m_rowBuffer[srcIdx++];
          pix->b = m_rowBuffer[srcIdx++];
          pix->g = m_rowBuffer[srcIdx++];
          pix->r = m_rowBuffer[srcIdx++];
#else
#error "unknown channel order"
#endif
          premult(*pix);
        } else {
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB) || defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
          pix->r = m_rowBuffer[srcIdx++];
          pix->g = m_rowBuffer[srcIdx++];
          pix->b = m_rowBuffer[srcIdx++];
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR) || defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
          pix->b = m_rowBuffer[srcIdx++];
          pix->g = m_rowBuffer[srcIdx++];
          pix->r = m_rowBuffer[srcIdx++];
#else
#error "unknown channel order"
#endif
          pix->m = 255;
        }
      }
    }
  }

  void writeRow(short *buffer, int x0, int x1) {
    bool hasAlpha = (m_channels == 4);

    TPixel64 *pix = (TPixel64 *)buffer + x0;
    int srcIdx = x0 * m_channels * (m_bit_depth / 8);
    for (int j = x0; j <= x1; ++j, ++pix) {
      if (hasAlpha) {
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB) || defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
        pix->r = mySwap(*(USHORT *)(m_rowBuffer.get() + srcIdx)); srcIdx += 2;
        pix->g = mySwap(*(USHORT *)(m_rowBuffer.get() + srcIdx)); srcIdx += 2;
        pix->b = mySwap(*(USHORT *)(m_rowBuffer.get() + srcIdx)); srcIdx += 2;
        pix->m = mySwap(*(USHORT *)(m_rowBuffer.get() + srcIdx)); srcIdx += 2;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR) || defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
        pix->b = mySwap(*(USHORT *)(m_rowBuffer.get() + srcIdx)); srcIdx += 2;
        pix->g = mySwap(*(USHORT *)(m_rowBuffer.get() + srcIdx)); srcIdx += 2;
        pix->r = mySwap(*(USHORT *)(m_rowBuffer.get() + srcIdx)); srcIdx += 2;
        pix->m = mySwap(*(USHORT *)(m_rowBuffer.get() + srcIdx)); srcIdx += 2;
#else
#error "unknown channel order"
#endif
        premult(*pix);
      } else {
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB) || defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
        pix->r = mySwap(*(USHORT *)(m_rowBuffer.get() + srcIdx)); srcIdx += 2;
        pix->g = mySwap(*(USHORT *)(m_rowBuffer.get() + srcIdx)); srcIdx += 2;
        pix->b = mySwap(*(USHORT *)(m_rowBuffer.get() + srcIdx)); srcIdx += 2;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR) || defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
        pix->b = mySwap(*(USHORT *)(m_rowBuffer.get() + srcIdx)); srcIdx += 2;
        pix->g = mySwap(*(USHORT *)(m_rowBuffer.get() + srcIdx)); srcIdx += 2;
        pix->r = mySwap(*(USHORT *)(m_rowBuffer.get() + srcIdx)); srcIdx += 2;
#else
#error "unknown channel order"
#endif
        pix->m = 65535;
      }
    }
  }

  void copyPixel(int count, int dstX, int dstDx, int dstY) {
    int bytesPerPixel = m_channels * (m_bit_depth / 8);
    for (int i = 0; i < count; ++i) {
      int srcIdx = i * bytesPerPixel;
      int dstIdx = (dstX + i * dstDx) * bytesPerPixel + dstY * m_info.m_lx * bytesPerPixel;
      memcpy(m_tempBuffer.get() + dstIdx, m_rowBuffer.get() + srcIdx, bytesPerPixel);
    }
  }

  void readLineInterlace(void *buffer, int x0, int x1, int shrink, bool isShort) {
    if (!m_tempBuffer) {
      m_tempBuffer.reset(new unsigned char[m_info.m_ly * m_info.m_lx * m_channels * (m_bit_depth / 8)]);
    }

    int pass = png_get_current_pass_number(m_png_ptr);
    int rowNumber = png_get_current_row_number(m_png_ptr);

    int desiredPass = (m_y % 8) / 4 + (m_y % 4) / 2 + (m_y % 2);  // Simplified, but use libpng helpers

    while (pass <= 6) {
      png_read_row(m_png_ptr, m_rowBuffer.get(), nullptr);
      int count, startX, step;

      switch (pass) {
      case 0:
        count = (m_info.m_lx + 7) / 8;
        startX = 0;
        step = 8;
        copyPixel(count, startX, step, rowNumber * 8);
        break;
      case 1:
        count = (m_info.m_lx + 3) / 8;
        startX = 4;
        step = 8;
        copyPixel(count, startX, step, rowNumber * 8);
        break;
      case 2:
        count = (m_info.m_lx + 3) / 4;
        startX = 0;
        step = 4;
        copyPixel(count, startX, step, rowNumber * 4 + 4);  // Adjust y offset
        break;
      case 3:
        count = (m_info.m_lx + 1) / 4;
        startX = 2;
        step = 4;
        copyPixel(count, startX, step, rowNumber * 4);
        break;
      case 4:
        count = (m_info.m_lx + 1) / 2;
        startX = 0;
        step = 2;
        copyPixel(count, startX, step, rowNumber * 2 + 2);
        break;
      case 5:
        count = (m_info.m_lx) / 2;
        startX = 1;
        step = 2;
        copyPixel(count, startX, step, rowNumber * 2);
        break;
      case 6:
        count = m_info.m_lx;
        startX = 0;
        step = 1;
        copyPixel(count, startX, step, rowNumber + 1);
        break;
      }

      pass = png_get_current_pass_number(m_png_ptr);
      rowNumber = png_get_current_row_number(m_png_ptr);
    }

    // Copy the deinterlaced row to m_rowBuffer
    int bytesPerRow = m_info.m_lx * m_channels * (m_bit_depth / 8);
    memcpy(m_rowBuffer.get(), m_tempBuffer.get() + m_y * bytesPerRow, bytesPerRow);

    if (isShort) {
      writeRow((short*)buffer, x0, x1);
    } else {
      writeRow((char*)buffer, x0, x1);
    }

    if (m_y == m_info.m_ly - 1) {
      m_tempBuffer.reset();
    }
  }

  void readLineInterlace(char *buffer, int x0, int x1, int shrink) {
    readLineInterlace((void*)buffer, x0, x1, shrink, false);
  }

  void readLineInterlace(short *buffer, int x0, int x1, int shrink) {
    readLineInterlace((void*)buffer, x0, x1, shrink, true);
  }
};

//=========================================================

Tiio::PngWriterProperties::PngWriterProperties()
    : m_matte("Alpha Channel", true) {
  bind(m_matte);
}

void Tiio::PngWriterProperties::updateTranslation() {
  m_matte.setQStringName(tr("Alpha Channel"));
}

//=========================================================

class PngWriter final : public Tiio::Writer {
  png_structp m_png_ptr;
  png_infop m_info_ptr;
  FILE *m_chan;
  bool m_matte;
  std::vector<TPixel> *m_colormap;

public:
  PngWriter() : m_png_ptr(0), m_info_ptr(0), m_matte(true), m_colormap(0) {}

  ~PngWriter() {
    if (m_png_ptr) {
      png_destroy_write_struct(&m_png_ptr, &m_info_ptr);
    }
    if (m_chan) {
      fflush(m_chan);
      m_chan = 0;
    }
  }

  void open(FILE *file, const TImageInfo &info) override {
    m_chan = file;
    m_info = info;

    m_png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!m_png_ptr) {
      throw TException("Unable to create PNG write structure");
    }

    m_info_ptr = png_create_info_struct(m_png_ptr);
    if (!m_info_ptr) {
      png_destroy_write_struct(&m_png_ptr, nullptr);
      throw TException("Unable to create PNG info structure");
    }

    if (setjmp(png_jmpbuf(m_png_ptr))) {
      png_destroy_write_struct(&m_png_ptr, &m_info_ptr);
      throw TException("PNG write initialization failed");
    }

    png_init_io(m_png_ptr, m_chan);

    if (!m_properties) m_properties = new Tiio::PngWriterProperties();

    TBoolProperty *alphaProp = dynamic_cast<TBoolProperty*>(m_properties->getProperty("Alpha Channel"));
    m_matte = alphaProp ? alphaProp->getValue() : true;

    TPointerProperty *colormapProp = dynamic_cast<TPointerProperty*>(m_properties->getProperty("Colormap"));
    m_colormap = colormapProp ? static_cast<std::vector<TPixel>*>(colormapProp->getValue()) : nullptr;

    png_uint_32 x_pixels_per_meter = tround(m_info.m_dpix / 0.0254);
    png_uint_32 y_pixels_per_meter = tround(m_info.m_dpiy / 0.0254);

    int colorType = m_matte ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB;
    int bitDepth = info.m_bitsPerSample;

    if (m_colormap) {
      colorType = PNG_COLOR_TYPE_PALETTE;
      bitDepth = 8;
    }

    png_set_IHDR(m_png_ptr, m_info_ptr, m_info.m_lx, m_info.m_ly, bitDepth, colorType,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

#if defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR)
    png_set_bgr(m_png_ptr);
    png_set_swap_alpha(m_png_ptr);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
    png_set_bgr(m_png_ptr);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB)
    png_set_swap_alpha(m_png_ptr);
#elif !defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
#error "unknown channel order"
#endif

    png_set_pHYs(m_png_ptr, m_info_ptr, x_pixels_per_meter, y_pixels_per_meter, PNG_RESOLUTION_METER);

    if (m_colormap) {
      png_color palette[256];
      for (size_t i = 0; i < m_colormap->size(); ++i) {
        palette[i].red = (*m_colormap)[i].r;
        palette[i].green = (*m_colormap)[i].g;
        palette[i].blue = (*m_colormap)[i].b;
      }
      png_set_PLTE(m_png_ptr, m_info_ptr, palette, m_colormap->size());

      if (m_matte) {
        png_byte alpha[1] = {0};
        png_color_16 bgcolor = {0};
        png_set_tRNS(m_png_ptr, m_info_ptr, alpha, 1, &bgcolor);
      }
    }

    png_write_info(m_png_ptr, m_info_ptr);
  }

  void writeLine(char *buffer) override {
    if (setjmp(png_jmpbuf(m_png_ptr))) {
      throw TException("PNG write error");
    }

    std::unique_ptr<unsigned char[]> row(new unsigned char[m_info.m_lx * (m_matte ? 4 : 3)]);
    TPixel32 *pix = (TPixel32 *)buffer;
    int k = 0;
    for (int j = 0; j < m_info.m_lx; ++j, ++pix) {
      TPixel32 depix = *pix;
      if (m_matte && depix.m != 0) depremult(depix);

      if (m_matte) {
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB)
        row[k++] = depix.m;
        row[k++] = depix.r;
        row[k++] = depix.g;
        row[k++] = depix.b;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
        row[k++] = depix.r;
        row[k++] = depix.g;
        row[k++] = depix.b;
        row[k++] = depix.m;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR)
        row[k++] = depix.m;
        row[k++] = depix.b;
        row[k++] = depix.g;
        row[k++] = depix.r;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
        row[k++] = depix.b;
        row[k++] = depix.g;
        row[k++] = depix.r;
        row[k++] = depix.m;
#else
#error "unknown channel order"
#endif
      } else {
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB) || defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
        row[k++] = depix.r;
        row[k++] = depix.g;
        row[k++] = depix.b;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR) || defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
        row[k++] = depix.b;
        row[k++] = depix.g;
        row[k++] = depix.r;
#else
#error "unknown channel order"
#endif
      }
    }
    png_write_row(m_png_ptr, row.get());
  }

  void writeLine(short *buffer) override {
    if (setjmp(png_jmpbuf(m_png_ptr))) {
      throw TException("PNG write error");
    }

    std::unique_ptr<unsigned short[]> row(new unsigned short[m_info.m_lx * (m_matte ? 4 : 3)]);
    TPixel64 *pix = (TPixel64 *)buffer;
    int k = 0;
    for (int j = 0; j < m_info.m_lx; ++j, ++pix) {
      TPixel64 depix = *pix;
      if (m_matte && depix.m != 0) depremult(depix);

      if (m_matte) {
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB) || defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
        row[k++] = mySwap(depix.r);
        row[k++] = mySwap(depix.g);
        row[k++] = mySwap(depix.b);
        row[k++] = mySwap(depix.m);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR) || defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
        row[k++] = mySwap(depix.b);
        row[k++] = mySwap(depix.g);
        row[k++] = mySwap(depix.r);
        row[k++] = mySwap(depix.m);
#else
#error "unknown channel order"
#endif
      } else {
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB) || defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
        row[k++] = mySwap(depix.r);
        row[k++] = mySwap(depix.g);
        row[k++] = mySwap(depix.b);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR) || defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
        row[k++] = mySwap(depix.b);
        row[k++] = mySwap(depix.g);
        row[k++] = mySwap(depix.r);
#else
#error "unknown channel order"
#endif
      }
    }
    png_write_row(m_png_ptr, (unsigned char *)row.get());
  }

  void flush() override {
    if (setjmp(png_jmpbuf(m_png_ptr))) {
      throw TException("PNG write error during flush");
    }
    png_write_end(m_png_ptr, m_info_ptr);
    fflush(m_chan);
  }

  Tiio::RowOrder getRowOrder() const override { return Tiio::TOP2BOTTOM; }

  bool write64bitSupported() const override { return true; }

  bool writeAlphaSupported() const override { return m_matte; }
};

//=========================================================

Tiio::Reader *Tiio::makePngReader() { return new PngReader(); }

Tiio::Writer *Tiio::makePngWriter() { return new PngWriter(); }