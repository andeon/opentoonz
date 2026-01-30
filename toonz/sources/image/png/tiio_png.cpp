

#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#include <memory>
#include <vector>

#include "tmachine.h"
#include "texception.h"
#include "tfilepath.h"
#include "tiio_png.h"
#include "tiio.h"
#include "../compatibility/tfile_io.h"

#include "png.h"

#include "tpixel.h"
#include "tpixelutils.h"

//------------------------------------------------------------

// Error handling functions (unchanged)
extern "C" {
static void tnz_abort(jmp_buf, int) {}
static void tnz_error_fun(png_structp pngPtr, png_const_charp error_message) {
  *(int *)png_get_error_ptr(pngPtr) = 0;
}
}

#if !defined(TNZ_LITTLE_ENDIAN)
#error "TNZ_LITTLE_ENDIAN undefined !!"
#endif

// Polyfill for older libpng (unchanged)
#if defined(PNG_LIBPNG_VER)
#if (PNG_LIBPNG_VER < 10527)
extern "C" {
static png_uint_32 png_get_current_row_number(const png_structp png_ptr) {
  if (png_ptr != NULL) return png_ptr->row_number;
  return PNG_UINT_32_MAX;
}

static png_byte png_get_current_pass_number(const png_structp png_ptr) {
  if (png_ptr != NULL) return png_ptr->pass;
  return 8;
}
}
#endif
#else
#error "PNG_LIBPNG_VER undefined, libpng too old?"
#endif

// Byte swap for 16-bit (unchanged)
inline USHORT mySwap(USHORT val) {
#if TNZ_LITTLE_ENDIAN
  return static_cast<USHORT>((val >> 8) | ((val & 0xFF) << 8));
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
  std::unique_ptr<unsigned char[]> m_tempBuffer;  // Full image buffer for interlace deinterlacing
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
    // Basic file opening and signature check (unchanged)
    m_chan = file;
    unsigned char signature[8];
    if (fread(signature, 1, sizeof(signature), m_chan) != sizeof(signature) ||
        png_sig_cmp(signature, 0, sizeof(signature))) {
      throw TException("Not a valid PNG file");
    }

    // Create libpng structures
    m_png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, &m_canDelete, tnz_error_fun, nullptr);
    if (!m_png_ptr) throw TException("Unable to create PNG read structure");
    m_canDelete = 1;
    m_info_ptr = png_create_info_struct(m_png_ptr);
    m_end_info_ptr = png_create_info_struct(m_png_ptr);
    if (!m_info_ptr || !m_end_info_ptr) {
      png_destroy_read_struct(&m_png_ptr, &m_info_ptr, &m_end_info_ptr);
      throw TException("Unable to create PNG info structures");
    }

    if (setjmp(png_jmpbuf(m_png_ptr))) {
      png_destroy_read_struct(&m_png_ptr, &m_info_ptr, &m_end_info_ptr);
      throw TException("PNG initialization failed");
    }

    png_init_io(m_png_ptr, m_chan);
    png_set_sig_bytes(m_png_ptr, sizeof(signature));
    png_read_info(m_png_ptr, m_info_ptr);

    // Read DPI if available
    if (png_get_valid(m_png_ptr, m_info_ptr, PNG_INFO_pHYs)) {
      png_uint_32 xdpi = png_get_x_pixels_per_meter(m_png_ptr, m_info_ptr);
      png_uint_32 ydpi = png_get_y_pixels_per_meter(m_png_ptr, m_info_ptr);
      m_info.m_dpix = tround(xdpi * 0.0254);
      m_info.m_dpiy = tround(ydpi * 0.0254);
    }

    // Get initial IHDR info
    png_uint_32 lx, ly;
    png_get_IHDR(m_png_ptr, m_info_ptr, &lx, &ly, &m_bit_depth, &m_color_type,
                 &m_interlace_type, &m_compression_type, &m_filter_type);
    m_info.m_lx = lx;
    m_info.m_ly = ly;
    m_info.m_bitsPerSample = m_bit_depth;

    // Apply transformations
    if (m_color_type == PNG_COLOR_TYPE_PALETTE) {
      png_set_palette_to_rgb(m_png_ptr);
      png_set_filler(m_png_ptr, 0xFF, PNG_FILLER_AFTER);
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
    if (m_color_type == PNG_COLOR_TYPE_GRAY || m_color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
      png_set_gray_to_rgb(m_png_ptr);
    }

    // Channel order adjustments
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

    // Update info after transformations (critical for correct channels/rowbytes)
    png_read_update_info(m_png_ptr, m_info_ptr);

    // Refresh variables
    m_channels = png_get_channels(m_png_ptr, m_info_ptr);
    m_rowBytes = png_get_rowbytes(m_png_ptr, m_info_ptr);
    png_get_IHDR(m_png_ptr, m_info_ptr, &lx, &ly, &m_bit_depth, &m_color_type,
                 &m_interlace_type, &m_compression_type, &m_filter_type);
    m_info.m_samplePerPixel = m_channels;

    // Allocate row buffer
    m_rowBuffer.reset(new unsigned char[m_rowBytes]);

    // For interlaced, allocate temp buffer for the full image (necessary for manual deinterlacing)
    if (m_interlace_type == PNG_INTERLACE_ADAM7) {
      size_t bufferSize = static_cast<size_t>(m_info.m_ly) * m_rowBytes;
      m_tempBuffer.reset(new unsigned char[bufferSize]);
      memset(m_tempBuffer.get(), 0, bufferSize);  // Initialize to avoid garbage
    }
  }

  void readLine(char* buffer, int x0, int x1, int shrink) override {
    readLineInternal(buffer, x0, x1, shrink, false);
  }

  void readLine(short* buffer, int x0, int x1, int shrink) override {
    readLineInternal(buffer, x0, x1, shrink, true);
  }

  int skipLines(int lineCount) override {
    if (setjmp(png_jmpbuf(m_png_ptr))) throw TException("PNG read error");

    for (int i = 0; i < lineCount; ++i) {
      if (m_interlace_type == PNG_INTERLACE_ADAM7) {
        advanceInterlace();
      } else {
        png_read_row(m_png_ptr, m_rowBuffer.get(), nullptr);
      }
      m_y++;
    }
    return lineCount;
  }

  Tiio::RowOrder getRowOrder() const override { return Tiio::TOP2BOTTOM; }

private:
  // Advances the PNG stream by reading sub-rows needed to complete the current full row (m_y)
  // Fills m_tempBuffer with contributions from each pass
  void advanceInterlace() {
    int passPng = png_get_current_pass_number(m_png_ptr);
    // Calculate desired pass for current row (Adam7: even rows up to pass 5, odd up to 6)
    int passRow = 5 + (m_y % 2);

    while (passPng <= passRow) {
      png_read_row(m_png_ptr, m_rowBuffer.get(), nullptr);

      int rowNumber = png_get_current_row_number(m_png_ptr);
      int count = 0, startX = 0, step = 1, destY = 0;

      switch (passPng) {
        case 0:
          count = (m_info.m_lx + 7) / 8;
          startX = 0;
          step = 8;
          destY = rowNumber * 8;
          break;
        case 1:
          count = (m_info.m_lx + 3) / 8;
          startX = 4;
          step = 8;
          destY = rowNumber * 8;
          break;
        case 2:
          count = (m_info.m_lx + 3) / 4;
          startX = 0;
          step = 4;
          destY = rowNumber * 4 + 4;
          break;
        case 3:
          count = (m_info.m_lx + 1) / 4;
          startX = 2;
          step = 4;
          destY = rowNumber * 4;
          break;
        case 4:
          count = (m_info.m_lx + 1) / 2;
          startX = 0;
          step = 2;
          destY = rowNumber * 2 + 2;
          break;
        case 5:
          count = (m_info.m_lx) / 2;
          startX = 1;
          step = 2;
          destY = rowNumber * 2;
          break;
        case 6:
          count = m_info.m_lx;
          startX = 0;
          step = 1;
          destY = rowNumber + 1;
          break;
      }

      if (destY < m_info.m_ly) {
        copyPixel(count, startX, step, destY);
      }

      passPng = png_get_current_pass_number(m_png_ptr);
    }
  }

  // Copies pixels from current sub-row to the correct positions in m_tempBuffer
  void copyPixel(int count, int startX, int step, int destY) {
    if (destY < 0 || destY >= m_info.m_ly) return;

    int bpp = m_channels * (m_bit_depth / 8);
    int maxDst = m_info.m_ly * m_rowBytes;

    count = std::min(count, (m_info.m_lx - startX + step - 1) / step);  // Ensure no overflow

    for (int i = 0; i < count; ++i) {
      int dstXPos = startX + i * step;
      if (dstXPos >= m_info.m_lx) break;

      int srcIdx = i * bpp;
      int dstIdx = dstXPos * bpp + destY * m_rowBytes;
      if (dstIdx + bpp > maxDst) break;

      memcpy(m_tempBuffer.get() + dstIdx, m_rowBuffer.get() + srcIdx, bpp);
    }
  }

  // Unified readLine logic
  void readLineInternal(void* buffer, int x0, int x1, int shrink, bool isShort) {
    if (setjmp(png_jmpbuf(m_png_ptr))) throw TException("PNG read error");

    if (m_interlace_type == PNG_INTERLACE_ADAM7) {
      advanceInterlace();
      // Copy completed row from temp to rowBuffer
      memcpy(m_rowBuffer.get(), m_tempBuffer.get() + m_y * m_rowBytes, m_rowBytes);
      if (m_y == m_info.m_ly - 1) m_tempBuffer.reset();  // Free after last row
    } else {
      png_read_row(m_png_ptr, m_rowBuffer.get(), nullptr);
    }

    // Write to user buffer
    if (isShort) {
      writeRowImpl(reinterpret_cast<short*>(buffer), x0, x1);
    } else {
      writeRowImpl(reinterpret_cast<char*>(buffer), x0, x1);
    }

    m_y++;
  }

  // Templated write row to handle 8/16 bit buffers (split loops to avoid type mismatch)
  template <typename PixelType>
  void writeRowImpl(PixelType* buffer, int x0, int x1) {
    const bool is64bit = (sizeof(PixelType) == sizeof(TPixel64));
    const bool hasAlpha = (m_channels == 4);
    const int bpc = m_bit_depth / 8;
    const int srcStride = m_channels * bpc;

    if (is64bit != (bpc == 2)) {
      throw TException("Buffer bit depth mismatch with PNG data");
    }

    if (is64bit) {
      TPixel64* pix_ptr = reinterpret_cast<TPixel64*>(buffer) + x0;
      for (int j = x0; j <= x1; ++j) {
        int srcIdx = j * srcStride;  // Calculate src index for current pixel
        USHORT r, g, b, m = 65535;  // Default alpha full opaque for no-alpha case
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB) || defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
        r = mySwap(*reinterpret_cast<USHORT*>(m_rowBuffer.get() + srcIdx)); srcIdx += 2;
        g = mySwap(*reinterpret_cast<USHORT*>(m_rowBuffer.get() + srcIdx)); srcIdx += 2;
        b = mySwap(*reinterpret_cast<USHORT*>(m_rowBuffer.get() + srcIdx)); srcIdx += 2;
        if (hasAlpha) m = mySwap(*reinterpret_cast<USHORT*>(m_rowBuffer.get() + srcIdx));
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR) || defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
        b = mySwap(*reinterpret_cast<USHORT*>(m_rowBuffer.get() + srcIdx)); srcIdx += 2;
        g = mySwap(*reinterpret_cast<USHORT*>(m_rowBuffer.get() + srcIdx)); srcIdx += 2;
        r = mySwap(*reinterpret_cast<USHORT*>(m_rowBuffer.get() + srcIdx)); srcIdx += 2;
        if (hasAlpha) m = mySwap(*reinterpret_cast<USHORT*>(m_rowBuffer.get() + srcIdx));
#else
#error "unknown channel order"
#endif
        pix_ptr->r = r;
        pix_ptr->g = g;
        pix_ptr->b = b;
        pix_ptr->m = m;
        premult(*pix_ptr);
        ++pix_ptr;  // Advance pointer
      }
    } else {
      TPixel32* pix_ptr = reinterpret_cast<TPixel32*>(buffer) + x0;
      for (int j = x0; j <= x1; ++j) {
        int srcIdx = j * srcStride;  // Calculate src index for current pixel
        UCHAR r, g, b, m = 255;  // Default alpha full opaque for no-alpha case
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB)
        m = m_rowBuffer[srcIdx++];
        r = m_rowBuffer[srcIdx++];
        g = m_rowBuffer[srcIdx++];
        b = m_rowBuffer[srcIdx++];
        if (!hasAlpha) m = 255;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
        r = m_rowBuffer[srcIdx++];
        g = m_rowBuffer[srcIdx++];
        b = m_rowBuffer[srcIdx++];
        m = m_rowBuffer[srcIdx++];
        if (!hasAlpha) m = 255;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
        b = m_rowBuffer[srcIdx++];
        g = m_rowBuffer[srcIdx++];
        r = m_rowBuffer[srcIdx++];
        m = m_rowBuffer[srcIdx++];
        if (!hasAlpha) m = 255;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR)
        m = m_rowBuffer[srcIdx++];
        b = m_rowBuffer[srcIdx++];
        g = m_rowBuffer[srcIdx++];
        r = m_rowBuffer[srcIdx++];
        if (!hasAlpha) m = 255;
#else
#error "unknown channel order"
#endif
        pix_ptr->r = r;
        pix_ptr->g = g;
        pix_ptr->b = b;
        pix_ptr->m = m;
        premult(*pix_ptr);
        ++pix_ptr;  // Advance pointer
      }
    }
  }
};

//=========================================================

Tiio::PngWriterProperties::PngWriterProperties() : m_matte("Alpha Channel", true) {
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
  int m_currentRow;

public:
  PngWriter()
      : m_png_ptr(0)
      , m_info_ptr(0)
      , m_chan(0)
      , m_matte(true)
      , m_colormap(0)
      , m_currentRow(0) {}

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
    // (Unchanged, as errors were in reader/writer logic)
    m_chan = file;
    m_info = info;
    m_currentRow = 0;

    m_png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!m_png_ptr) throw TException("Unable to create PNG write structure");

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
      size_t paletteSize = std::min(m_colormap->size(), size_t(256));
      for (size_t i = 0; i < paletteSize; ++i) {
        palette[i].red = (*m_colormap)[i].r;
        palette[i].green = (*m_colormap)[i].g;
        palette[i].blue = (*m_colormap)[i].b;
      }
      png_set_PLTE(m_png_ptr, m_info_ptr, palette, static_cast<int>(paletteSize));

      if (m_matte) {
        png_byte alpha[1] = {0};
        png_color_16 bgcolor = {0};
        png_set_tRNS(m_png_ptr, m_info_ptr, alpha, 1, &bgcolor);
      }
    }

    png_write_info(m_png_ptr, m_info_ptr);
  }

  void writeLine(char* buffer) override {
    writeLineInternal(buffer, false);
  }

  void writeLine(short* buffer) override {
    writeLineInternal(buffer, true);
  }

  // Unified write line (fixed pointer arithmetic)
  void writeLineInternal(void* buffer, bool isShort) {
    if (setjmp(png_jmpbuf(m_png_ptr))) throw TException("PNG write error");

    int components = m_matte ? 4 : 3;
    int bpc = isShort ? 2 : 1;
    size_t rowSize = static_cast<size_t>(m_info.m_lx) * components * bpc;
    std::unique_ptr<unsigned char[]> row(new unsigned char[rowSize]);
    int k = 0;

    if (isShort) {
      TPixel64* pix_ptr = reinterpret_cast<TPixel64*>(buffer);
      for (int j = 0; j < m_info.m_lx; ++j) {
        TPixel64 depix = pix_ptr[j];  // Fixed: Use array syntax for pointer + offset
        if (m_matte && depix.m != 0) depremult(depix);
        USHORT r = depix.r, g = depix.g, b = depix.b, m = depix.m;
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB) || defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
        reinterpret_cast<USHORT*>(row.get())[k++] = mySwap(r);
        reinterpret_cast<USHORT*>(row.get())[k++] = mySwap(g);
        reinterpret_cast<USHORT*>(row.get())[k++] = mySwap(b);
        if (m_matte) reinterpret_cast<USHORT*>(row.get())[k++] = mySwap(m);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR) || defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
        reinterpret_cast<USHORT*>(row.get())[k++] = mySwap(b);
        reinterpret_cast<USHORT*>(row.get())[k++] = mySwap(g);
        reinterpret_cast<USHORT*>(row.get())[k++] = mySwap(r);
        if (m_matte) reinterpret_cast<USHORT*>(row.get())[k++] = mySwap(m);
#else
#error "unknown channel order"
#endif
      }
    } else {
      TPixel32* pix_ptr = reinterpret_cast<TPixel32*>(buffer);
      for (int j = 0; j < m_info.m_lx; ++j) {
        TPixel32 depix = pix_ptr[j];  // Fixed: Use array syntax for pointer + offset
        if (m_matte && depix.m != 0) depremult(depix);
        UCHAR r = depix.r, g = depix.g, b = depix.b, m = depix.m;
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB)
        row[k++] = m;
        row[k++] = r;
        row[k++] = g;
        row[k++] = b;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
        row[k++] = r;
        row[k++] = g;
        row[k++] = b;
        row[k++] = m;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR)
        row[k++] = m;
        row[k++] = b;
        row[k++] = g;
        row[k++] = r;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
        row[k++] = b;
        row[k++] = g;
        row[k++] = r;
        row[k++] = m;
#else
#error "unknown channel order"
#endif
        if (!m_matte) {
          --k;  // Adjust if no alpha (skip m write)
        }
      }
    }
    png_write_row(m_png_ptr, row.get());
    m_currentRow++;
  }

  void flush() override {
    if (m_currentRow != m_info.m_ly) throw TException("Not all rows written to PNG");
    if (setjmp(png_jmpbuf(m_png_ptr))) throw TException("PNG flush error");
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