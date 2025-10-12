

#include "traster.h"
#include "trastercm.h"
#include "tpalette.h"
#include "tcolorstyles.h"
//#include "trop.h"
#include "tropcm.h"
#include "tpixelutils.h"

#include <memory>

//------------------------------------------------------------------------------

namespace {

//------------------------------------------------------------------------------

template <class PIXOUT, class PIXIN>
void doConvolve_row_9_i(PIXOUT *pixout, int n, PIXIN *pixarr[], long w[]) {
  long w1, w2, w3, w4, w5, w6, w7, w8, w9;
  PIXIN *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9;
  w1 = w[0];
  w2 = w[1];
  w3 = w[2];
  w4 = w[3];
  w5 = w[4];
  w6 = w[5];
  w7 = w[6];
  w8 = w[7];
  w9 = w[8];
  p1 = pixarr[0];
  p2 = pixarr[1];
  p3 = pixarr[2];
  p4 = pixarr[3];
  p5 = pixarr[4];
  p6 = pixarr[5];
  p7 = pixarr[6];
  p8 = pixarr[7];
  p9 = pixarr[8];

  // Calculate right shift based on channel size difference
  int rightShift = 16 +
                   ((int)sizeof(typename PIXIN::Channel) -
                    (int)sizeof(typename PIXOUT::Channel)) *
                       8;

  while (n-- > 0) {
    // Convolve each channel with 3x3 kernel weights
    pixout->r = (typename PIXOUT::Channel)(
        (p1->r * w1 + p2->r * w2 + p3->r * w3 + p4->r * w4 + p5->r * w5 +
         p6->r * w6 + p7->r * w7 + p8->r * w8 + p9->r * w9 + (1 << 15)) >>
        rightShift);
    pixout->g = (typename PIXOUT::Channel)(
        (p1->g * w1 + p2->g * w2 + p3->g * w3 + p4->g * w4 + p5->g * w5 +
         p6->g * w6 + p7->g * w7 + p8->g * w8 + p9->g * w9 + (1 << 15)) >>
        rightShift);
    pixout->b = (typename PIXOUT::Channel)(
        (p1->b * w1 + p2->b * w2 + p3->b * w3 + p4->b * w4 + p5->b * w5 +
         p6->b * w6 + p7->b * w7 + p8->b * w8 + p9->b * w9 + (1 << 15)) >>
        rightShift);
    pixout->m = (typename PIXOUT::Channel)(
        (p1->m * w1 + p2->m * w2 + p3->m * w3 + p4->m * w4 + p5->m * w5 +
         p6->m * w6 + p7->m * w7 + p8->m * w8 + p9->m * w9 + (1 << 15)) >>
        rightShift);

    // Move to next pixel in all arrays
    p1++;
    p2++;
    p3++;
    p4++;
    p5++;
    p6++;
    p7++;
    p8++;
    p9++;
    pixout++;
  }
}

//------------------------------------------------------------------------------

template <class PIXOUT>
void doConvolve_cm32_row_9_i(PIXOUT *pixout, int n, TPixelCM32 *pixarr[],
                             long w[], const std::vector<TPixel32> &paints,
                             const std::vector<TPixel32> &inks) {
  long w1, w2, w3, w4, w5, w6, w7, w8, w9;
  TPixelCM32 *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9;
  TPixel32 val[9];

  w1 = w[0];
  w2 = w[1];
  w3 = w[2];
  w4 = w[3];
  w5 = w[4];
  w6 = w[5];
  w7 = w[6];
  w8 = w[7];
  w9 = w[8];
  p1 = pixarr[0];
  p2 = pixarr[1];
  p3 = pixarr[2];
  p4 = pixarr[3];
  p5 = pixarr[4];
  p6 = pixarr[5];
  p7 = pixarr[6];
  p8 = pixarr[7];
  p9 = pixarr[8];

  // FIX: Create pixarr array directly to sample correct neighbors
  TPixelCM32 **ps = pixarr;

  while (n-- > 0) {
    // FIXED LOOP: Now uses ps[i] to get the correct pixel for each kernel position
    for (int i = 0; i < 9; ++i) {
      int tone  = ps[i]->getTone();
      int paint = ps[i]->getPaint();
      int ink   = ps[i]->getInk();
      if (tone == TPixelCM32::getMaxTone())
        val[i] = paints[paint];
      else if (tone == 0)
        val[i] = inks[ink];
      else
        val[i] =
            blend(inks[ink], paints[paint], tone, TPixelCM32::getMaxTone());
    }

    // Convolve the converted RGB values
    pixout->r = (typename PIXOUT::Channel)(
        (val[0].r * w1 + val[1].r * w2 + val[2].r * w3 + val[3].r * w4 +
         val[4].r * w5 + val[5].r * w6 + val[6].r * w7 + val[7].r * w8 +
         val[8].r * w9 + (1 << 15)) >>
        16);
    pixout->g = (typename PIXOUT::Channel)(
        (val[0].g * w1 + val[1].g * w2 + val[2].g * w3 + val[3].g * w4 +
         val[4].g * w5 + val[5].g * w6 + val[6].g * w7 + val[7].g * w8 +
         val[8].g * w9 + (1 << 15)) >>
        16);
    pixout->b = (typename PIXOUT::Channel)(
        (val[0].b * w1 + val[1].b * w2 + val[2].b * w3 + val[3].b * w4 +
         val[4].b * w5 + val[5].b * w6 + val[6].b * w7 + val[7].b * w8 +
         val[8].b * w9 + (1 << 15)) >>
        16);
    pixout->m = (typename PIXOUT::Channel)(
        (val[0].m * w1 + val[1].m * w2 + val[2].m * w3 + val[3].m * w4 +
         val[4].m * w5 + val[5].m * w6 + val[6].m * w7 + val[7].m * w8 +
         val[8].m * w9 + (1 << 15)) >>
        16);
    
    // Move to next pixel in all arrays
    p1++;
    p2++;
    p3++;
    p4++;
    p5++;
    p6++;
    p7++;
    p8++;
    p9++;
    pixout++;
  }
}

//------------------------------------------------------------------------------

template <class PIXOUT, class PIXIN>
void doConvolve_row_i(PIXOUT *pixout, int n, PIXIN *pixarr[], long w[],
                      int pixn) {
  long ar, ag, ab, am;
  int i;

  // Calculate right shift based on channel size difference
  int rightShift = 16 +
                   ((int)sizeof(typename PIXIN::Channel) -
                    (int)sizeof(typename PIXOUT::Channel)) *
                       8;

  while (n-- > 0) {
    ar = ag = ab = am = 0;
    // Accumulate weighted sum for each channel
    for (i = 0; i < pixn; i++) {
      ar += pixarr[i]->r * w[i];
      ag += pixarr[i]->g * w[i];
      ab += pixarr[i]->b * w[i];
      am += pixarr[i]->m * w[i];
      pixarr[i]++;
    }
    // Apply right shift and store results
    pixout->r = (typename PIXOUT::Channel)((ar + (1 << 15)) >> rightShift);
    pixout->g = (typename PIXOUT::Channel)((ag + (1 << 15)) >> rightShift);
    pixout->b = (typename PIXOUT::Channel)((ab + (1 << 15)) >> rightShift);
    pixout->m = (typename PIXOUT::Channel)((am + (1 << 15)) >> rightShift);

    pixout++;
  }
}

//------------------------------------------------------------------------------

template <class PIXOUT>
void doConvolve_cm32_row_i(PIXOUT *pixout, int n, TPixelCM32 *pixarr[],
                           long w[], int pixn,
                           const std::vector<TPixel32> &paints,
                           const std::vector<TPixel32> &inks) {
  long ar, ag, ab, am;
  int i;

  while (n-- > 0) {
    ar = ag = ab = am = 0;
    for (i = 0; i < pixn; i++) {
      TPixel32 val;
      int tone  = pixarr[i]->getTone();
      int paint = pixarr[i]->getPaint();
      int ink   = pixarr[i]->getInk();
      // Convert CM32 pixel to RGB using palette
      if (tone == TPixelCM32::getMaxTone())
        val = paints[paint];
      else if (tone == 0)
        val = inks[ink];
      else
        val = blend(inks[ink], paints[paint], tone, TPixelCM32::getMaxTone());

      // Accumulate weighted RGB values
      ar += val.r * w[i];
      ag += val.g * w[i];
      ab += val.b * w[i];
      am += val.m * w[i];
      pixarr[i]++;
    }
    // Store convolved result
    pixout->r = (typename PIXOUT::Channel)((ar + (1 << 15)) >> 16);
    pixout->g = (typename PIXOUT::Channel)((ag + (1 << 15)) >> 16);
    pixout->b = (typename PIXOUT::Channel)((ab + (1 << 15)) >> 16);
    pixout->m = (typename PIXOUT::Channel)((am + (1 << 15)) >> 16);
    pixout++;
  }
}

//------------------------------------------------------------------------------

template <class PIXOUT, class PIXIN>
void doConvolve_3_i(TRasterPT<PIXOUT> rout, TRasterPT<PIXIN> rin, int dx,
                    int dy, double conv[]) {
  PIXIN *bufferin;
  PIXOUT *bufferout;
  PIXIN *pixin;
  PIXOUT *pixout;

  PIXIN *pixarr[9];
  long w[9];
  int pixn;
  int wrapin, wrapout;
  int x, y, n;
  int x1, y1, x2, y2;
  int fx1, fy1, fx2, fy2, fx, fy;

  rout->clear();

  wrapin  = rin->getWrap();
  wrapout = rout->getWrap();

  // Calculate the output area affected by convolution
  x1 = std::max(0, -dx - 1);
  y1 = std::max(0, -dy - 1);
  x2 = std::min(rout->getLx() - 1, -dx + rin->getLx());
  y2 = std::min(rout->getLy() - 1, -dy + rin->getLy());

  rin->lock();
  rout->lock();
  bufferin  = rin->pixels();
  bufferout = rout->pixels();

  for (y = y1; y <= y2; y++) {
    // Calculate valid y-range for kernel
    fy1 = std::max(-1, -dy - y);
    fy2 = std::min(1, -dy + rin->getLy() - 1 - y);
    if (fy1 > fy2) continue;
    x      = x1;
    pixout = bufferout + wrapout * y + x;
    pixin  = bufferin + wrapin * (y + dy) + (x + dx);

    while (x <= x2) {
      // Calculate valid x-range for kernel
      fx1 = std::max(-1, -dx - x);
      fx2 = std::min(1, -dx + rin->getLx() - 1 - x);
      // Determine how many pixels we can process in this segment
      if (x > -dx && x < -dx + rin->getLx() - 1)
        n = std::min(-dx + rin->getLx() - 1 - x, x2 - x + 1);
      else
        n = 1;
      if (n < 1) break;
      
      // Build pixel and weight arrays for current kernel position
      pixn = 0;
      for (fy = fy1; fy <= fy2; fy++)
        for (fx = fx1; fx <= fx2; fx++) {
          pixarr[pixn] = pixin + fy * wrapin + fx;
          w[pixn]      = (long)(conv[(fy + 1) * 3 + fx + 1] * (1 << 16));
          pixn++;
        }
      
      // Use optimized 3x3 convolution or generic version
      if (pixn == 9)
        doConvolve_row_9_i<PIXOUT, PIXIN>(pixout, n, pixarr, w);
      else
        doConvolve_row_i<PIXOUT, PIXIN>(pixout, n, pixarr, w, pixn);
      
      x += n;
      pixin += n;
      pixout += n;
    }
  }
  rin->unlock();
  rout->unlock();
}

//------------------------------------------------------------------------------

template <class PIXOUT, class PIXIN>
void doConvolve_i(TRasterPT<PIXOUT> rout, TRasterPT<PIXIN> rin, int dx, int dy,
                  double conv[], int radius) {
  PIXIN *bufferin;
  PIXOUT *bufferout;
  PIXIN *pixin;
  PIXOUT *pixout;

  int radiusSquare = sq(radius);
  std::unique_ptr<PIXIN *[]> pixarr(new PIXIN *[radiusSquare]);
  std::unique_ptr<long[]> w(new long[radiusSquare]);
  int pixn;
  int wrapin, wrapout;
  int x, y, n;
  int x1, y1, x2, y2;
  int fx1, fy1, fx2, fy2, fx, fy;
  int radius1 = radius / 2;
  int radius0 = radius1 - radius + 1;

  rout->clear();

  wrapin  = rin->getWrap();
  wrapout = rout->getWrap();

  // Calculate the output area affected by convolution
  x1 = std::max(0, -dx - 1);
  y1 = std::max(0, -dy - 1);
  x2 = std::min(rout->getLx() - 1, -dx + rin->getLx());
  y2 = std::min(rout->getLy() - 1, -dy + rin->getLy());

  rin->lock();
  rout->lock();
  bufferin  = rin->pixels();
  bufferout = rout->pixels();

  for (y = y1; y <= y2; y++) {
    // Calculate valid y-range for kernel
    fy1 = std::max(radius0, -dy - y);
    fy2 = std::min(radius1, -dy - y + rin->getLy() - 1);
    if (fy1 > fy2) continue;
    x      = x1;
    pixout = bufferout + wrapout * y + x;
    pixin  = bufferin + wrapin * (y + dy) + (x + dx);

    while (x <= x2) {
      // Calculate valid x-range for kernel
      fx1 = std::max(radius0, -dx - x);
      fx2 = std::min(radius1, -dx - x + rin->getLx() - 1);
      // Determine how many pixels we can process in this segment
      if (x > -dx && x < -dx + rin->getLx() - 1)
        n = std::min(-dx + rin->getLx() - 1 - x, x2 - x + 1);
      else
        n = 1;
      if (n < 1) break;
      
      // Build pixel and weight arrays for current kernel position
      pixn = 0;
      for (fy = fy1; fy <= fy2; fy++)
        for (fx = fx1; fx <= fx2; fx++) {
          pixarr[pixn] = pixin + fy * wrapin + fx;
          w[pixn] =
              (long)(conv[(fy - radius0) * radius + fx - radius0] * (1 << 16));
          pixn++;
        }

      // Process convolution for this segment
      doConvolve_row_i<PIXOUT, PIXIN>(pixout, n, pixarr.get(), w.get(), pixn);

      x += n;
      pixin += n;
      pixout += n;
    }
  }

  rin->unlock();
  rout->unlock();
}

//------------------------------------------------------------------------------

template <class PIXOUT>
void doConvolve_cm32_3_i(TRasterPT<PIXOUT> rout, TRasterCM32P rin,
                         const TPaletteP &palette, int dx, int dy,
                         double conv[]) {
  TPixelCM32 *pixin;
  PIXOUT *pixout;
  TPixelCM32 *pixarr[9];
  long w[9];
  int pixn;
  int wrapin, wrapout;
  int x, y, n;
  int x1, y1, x2, y2;
  int fx1, fy1, fx2, fy2, fx, fy;

  rout->clear();

  wrapin  = rin->getWrap();
  wrapout = rout->getWrap();

  // Calculate the output area affected by convolution
  x1 = std::max(0, -dx - 1);
  y1 = std::max(0, -dy - 1);
  x2 = std::min(rout->getLx() - 1, -dx + rin->getLx());
  y2 = std::min(rout->getLy() - 1, -dy + rin->getLy());

  // Prepare color tables from palette
  int colorCount = palette->getStyleCount();
  colorCount     = std::max(
      {colorCount, TPixelCM32::getMaxInk(), TPixelCM32::getMaxPaint()});

  std::vector<TPixel32> paints(colorCount);
  std::vector<TPixel32> inks(colorCount);

  rin->lock();
  rout->lock();
  TPixelCM32 *bufferin = rin->pixels();
  PIXOUT *bufferout    = rout->pixels();

  // Fill paint and ink color tables
  for (int i  = 0; i < palette->getStyleCount(); i++)
    paints[i] = inks[i] = palette->getStyle(i)->getAverageColor();

  for (y = y1; y <= y2; y++) {
    // Calculate valid y-range for kernel
    fy1 = std::max(-1, -dy - y);
    fy2 = std::min(1, -dy + rin->getLy() - 1 - y);
    if (fy1 > fy2) continue;
    x      = x1;
    pixout = bufferout + wrapout * y + x;
    pixin  = bufferin + wrapin * (y + dy) + (x + dx);

    while (x <= x2) {
      // Calculate valid x-range for kernel
      fx1 = std::max(-1, -dx - x);
      fx2 = std::min(1, -dx + rin->getLx() - 1 - x);
      // Determine how many pixels we can process in this segment
      if (x > -dx && x < -dx + rin->getLx() - 1)
        n = std::min(-dx + rin->getLx() - 1 - x, x2 - x + 1);
      else
        n = 1;
      if (n < 1) break;
      
      // Build pixel and weight arrays for current kernel position
      pixn = 0;
      for (fy = fy1; fy <= fy2; fy++)
        for (fx = fx1; fx <= fx2; fx++) {
          pixarr[pixn] = pixin + fy * wrapin + fx;
          w[pixn]      = (long)(conv[(fy + 1) * 3 + fx + 1] * (1 << 16));
          pixn++;
        }
      
      // Use optimized 3x3 convolution or generic version for CM32
      if (pixn == 9)
        doConvolve_cm32_row_9_i<PIXOUT>(pixout, n, pixarr, w, paints, inks);
      else
        doConvolve_cm32_row_i<PIXOUT>(pixout, n, pixarr, w, pixn, paints, inks);
      
      x += n;
      pixin += n;
      pixout += n;
    }
  }
  rin->unlock();
  rout->unlock();
}

//------------------------------------------------------------------------------

template <class PIXOUT>
void doConvolve_cm32_i(TRasterPT<PIXOUT> rout, TRasterCM32P rin,
                       const TPaletteP &palette, int dx, int dy, double conv[],
                       int radius) {
  TPixelCM32 *pixin;
  PIXOUT *pixout;
  int radiusSquare = sq(radius);
  std::unique_ptr<TPixelCM32 *[]> pixarr(new TPixelCM32 *[radiusSquare]);
  std::unique_ptr<long[]> w(new long[radiusSquare]);
  int pixn;
  int wrapin, wrapout;
  int x, y, n;
  int x1, y1, x2, y2;
  int fx1, fy1, fx2, fy2, fx, fy;
  int radius1 = radius / 2;
  int radius0 = radius1 - radius + 1;

  rout->clear();

  wrapin  = rin->getWrap();
  wrapout = rout->getWrap();

  // Calculate the output area affected by convolution
  x1 = std::max(0, -dx - 1);
  y1 = std::max(0, -dy - 1);
  x2 = std::min(rout->getLx() - 1, -dx + rin->getLx());
  y2 = std::min(rout->getLy() - 1, -dy + rin->getLy());

  // Prepare color tables from palette
  int colorCount = palette->getStyleCount();
  colorCount     = std::max(
      {colorCount, TPixelCM32::getMaxInk(), TPixelCM32::getMaxPaint()});

  std::vector<TPixel32> paints(colorCount);
  std::vector<TPixel32> inks(colorCount);

  rin->lock();
  rout->lock();
  TPixelCM32 *bufferin = rin->pixels();
  PIXOUT *bufferout    = rout->pixels();

  // Fill paint and ink color tables
  for (int i  = 0; i < palette->getStyleCount(); i++)
    paints[i] = inks[i] = palette->getStyle(i)->getAverageColor();

  for (y = y1; y <= y2; y++) {
    // Calculate valid y-range for kernel
    fy1 = std::max(radius0, -dy - y);
    fy2 = std::min(radius1, -dy + rin->getLy() - 1 - y);
    if (fy1 > fy2) continue;
    x      = x1;
    pixout = bufferout + wrapout * y + x;
    pixin  = bufferin + wrapin * (y + dy) + (x + dx);

    while (x <= x2) {
      // Calculate valid x-range for kernel
      fx1 = std::max(radius0, -dx - x);
      fx2 = std::min(radius1, -dx + rin->getLx() - 1 - x);
      // Determine how many pixels we can process in this segment
      if (x > -dx && x < -dx + rin->getLx() - 1)
        n = std::min(-dx + rin->getLx() - 1 - x, x2 - x + 1);
      else
        n = 1;
      if (n < 1) break;
      
      // Build pixel and weight arrays for current kernel position
      pixn = 0;
      for (fy = fy1; fy <= fy2; fy++)
        for (fx = fx1; fx <= fx2; fx++) {
          pixarr[pixn] = pixin + fy * wrapin + fx;
          w[pixn] =
              (long)(conv[(fy - radius0) * radius + fx - radius0] * (1 << 16));
          pixn++;
        }

      // Process convolution for CM32 pixels
      doConvolve_cm32_row_i<PIXOUT>(pixout, n, pixarr.get(), w.get(), pixn,
                                    paints, inks);

      x += n;
      pixin += n;
      pixout += n;
    }
  }

  rin->unlock();
  rout->unlock();
}

}  // anonymous namespace

//------------------------------------------------------------------------------

void TRop::convolve_3_i(TRasterP rout, TRasterP rin, int dx, int dy,
                        double conv[]) {
  TRaster32P rin32 = rin;

  if (rin32) {
    TRaster32P rout32 = rout;
    if (rout32) {
      doConvolve_3_i<TPixel32, TPixel32>(rout32, rin32, dx, dy, conv);
      return;
    }

    TRaster64P rout64 = rout;
    if (rout64) {
      doConvolve_3_i<TPixel64, TPixel32>(rout64, rin32, dx, dy, conv);
      return;
    }
  } else {
    TRaster64P rin64 = rin;
    if (rin64) {
      TRaster32P rout32 = rout;
      if (rout32) {
        doConvolve_3_i<TPixel32, TPixel64>(rout32, rin64, dx, dy, conv);
        return;
      }

      TRaster64P rout64 = rout;
      if (rout64) {
        doConvolve_3_i<TPixel64, TPixel64>(rout64, rin64, dx, dy, conv);
        return;
      }
    }
  }

  throw TRopException("TRop::convolve_3_i: unsupported pixel type");
}

//------------------------------------------------------------------------------

void TRop::convolve_3_i(TRasterP rout, TRasterCM32P rin,
                        const TPaletteP &palette, int dx, int dy,
                        double conv[]) {
  TRaster32P rout32 = rout;

  if (rout32) {
    doConvolve_cm32_3_i<TPixel32>(rout32, rin, palette, dx, dy, conv);
    return;
  }

  TRaster64P rout64 = rout;
  if (rout64) {
    doConvolve_cm32_3_i<TPixel64>(rout64, rin, palette, dx, dy, conv);
    return;
  }

  throw TRopException("TRop::convolve_3_i: unsupported pixel type");
}

//------------------------------------------------------------------------------

void TRop::convolve_i(TRasterP rout, TRasterP rin, int dx, int dy,
                      double conv[], int radius) {
  TRaster32P rin32 = rin;

  if (rin32) {
    TRaster32P rout32 = rout;
    if (rout32) {
      doConvolve_i<TPixel32, TPixel32>(rout32, rin32, dx, dy, conv, radius);
      return;
    }

    TRaster64P rout64 = rout;
    if (rout64) {
      doConvolve_i<TPixel64, TPixel32>(rout64, rin32, dx, dy, conv, radius);
      return;
    }
  } else {
    TRaster64P rin64 = rin;
    if (rin64) {
      TRaster32P rout32 = rout;
      if (rout32) {
        doConvolve_i<TPixel32, TPixel64>(rout32, rin64, dx, dy, conv, radius);
        return;
      }

      TRaster64P rout64 = rout;
      if (rout64) {
        doConvolve_i<TPixel64, TPixel64>(rout64, rin64, dx, dy, conv, radius);
        return;
      }
    }
  }

  throw TRopException("TRop::convolve_i: unsupported pixel type");
}

//------------------------------------------------------------------------------

void TRop::convolve_i(TRasterP rout, TRasterCM32P rin, const TPaletteP &palette,
                      int dx, int dy, double conv[], int radius) {
  TRaster32P rout32 = rout;

  if (rout32) {
    doConvolve_cm32_i<TPixel32>(rout32, rin, palette, dx, dy, conv, radius);
    return;
  }

  TRaster64P rout64 = rout;
  if (rout64) {
    doConvolve_cm32_i<TPixel64>(rout64, rin, palette, dx, dy, conv, radius);
    return;
  }

  throw TRopException("TRop::convolve_i: unsupported pixel type");
}
