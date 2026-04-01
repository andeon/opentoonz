

#include "tcodec.h"
#include "trastercm.h"
#include "tpixel.h"
#include "tbigmemorymanager.h"
#include "tsystem.h"
//#include "tstopwatch.h"
#include "timagecache.h"
#include "trasterimage.h"

//#include "snappy-c.h"
#if defined(LZ4_STATIC)
#include "lz4frame_static.h"
#else
#include "lz4frame.h"
#endif

#include <QDir>
#include <QProcess>
#include <QCoreApplication>

using namespace std;

namespace {
class Header {
public:
  enum RasType {
    Raster32RGBM,
    Raster64RGBM,
    Raster32CM,
    RasterGR8,
    RasterGR16,
    RasterUnknown
  };

  Header(const TRasterP &ras);
  ~Header() {}
  TRasterP createRaster() const;
  int getRasterSize() const;
  int getPixelSize() const;
  int m_lx;
  int m_ly;
  RasType m_rasType;
  Header(void *mem) { memcpy(this, mem, sizeof(Header)); }

private:
  Header();  // not implemented
};

//------------------------------------------------------------------------------

Header::Header(const TRasterP &ras) {
  assert(ras);
  m_lx = ras->getLx();
  m_ly = ras->getLy();
  TRaster32P ras32(ras);
  if (ras32)
    m_rasType = Raster32RGBM;
  else {
    TRasterCM32P rasCM32(ras);
    if (rasCM32)
      m_rasType = Raster32CM;
    else {
      TRaster64P ras64(ras);
      if (ras64)
        m_rasType = Raster64RGBM;
      else {
        TRasterGR8P rasGR8(ras);
        if (rasGR8)
          m_rasType = RasterGR8;
        else {
          TRasterGR16P rasGR16(ras);
          if (rasGR16)
            m_rasType = RasterGR16;
          else {
            assert(!"Unknown RasterType");
            m_rasType = RasterUnknown;
          }
        }
      }
    }
  }
}

//------------------------------------------------------------------------------

TRasterP Header::createRaster() const {
  switch (m_rasType) {
  case Raster32RGBM:
    return TRaster32P(m_lx, m_ly);
    break;
  case Raster32CM:
    return TRasterCM32P(m_lx, m_ly);
    break;
  case Raster64RGBM:
    return TRaster64P(m_lx, m_ly);
    break;
  case RasterGR8:
    return TRasterGR8P(m_lx, m_ly);
    break;
  case RasterGR16:
    return TRasterGR16P(m_lx, m_ly);
    break;
  default:
    assert(0);
    return TRasterP();
    break;
  }
  return TRasterP();
}

//------------------------------------------------------------------------------

int Header::getRasterSize() const {
  switch (m_rasType) {
  case Raster32RGBM:
    return 4 * m_lx * m_ly;
    break;
  case Raster32CM:
    return 4 * m_lx * m_ly;
    break;
  case Raster64RGBM:
    return 8 * m_lx * m_ly;
    break;
  case RasterGR8:
    return m_lx * m_ly;
    break;
  default:
    assert(0);
    return 0;
    break;
  }
}

//------------------------------------------------------------------------------

int Header::getPixelSize() const {
  switch (m_rasType) {
    case Raster32RGBM:
    case Raster32CM:
      return 4;
    case Raster64RGBM:
      return 8;
    case RasterGR8:
      return 1;
    case RasterGR16:
      return 2;
    default:
      return 0;
  }
}
//------------------------------------------------------------------------------
}  // anonymous namespace

//------------------------------------------------------------------------------
//	TRasterCodecSnappy
//------------------------------------------------------------------------------

/*TRasterCodecSnappy::TRasterCodecSnappy(const std::string &name, bool useCache)
  : TRasterCodec(name)
  , m_raster()
  , m_useCache(useCache)
  , m_cacheId("")
{
}

//------------------------------------------------------------------------------

TRasterCodecSnappy::~TRasterCodecSnappy()
{
  if (m_useCache)
    TImageCache::instance()->remove(m_cacheId);
  else
    m_raster = TRasterGR8P();
}

//------------------------------------------------------------------------------

UINT TRasterCodecSnappy::doCompress(const TRasterP &inRas, int allocUnit,
TRasterGR8P& outRas)
{
  assert(inRas);

  assert(inRas->getLx() == inRas->getWrap());


  size_t inDataSize = inRas->getLx() * inRas->getLy() * inRas->getPixelSize();
  size_t maxReqSize = snappy_max_compressed_length(inDataSize);

  if (m_useCache)
    {
    if (m_cacheId=="")
      m_cacheId = TImageCache::instance()->getUniqueId();
    else
      outRas = ((TRasterImageP)TImageCache::instance()->get(m_cacheId,
true))->getRaster();
    }
  else
    outRas = m_raster;

  if (!outRas || outRas->getLx()<(int)maxReqSize)
  {
    outRas = TRasterGR8P();
    m_raster = TRasterGR8P();
    if (m_useCache)
      TImageCache::instance()->remove(m_cacheId);
    outRas = TRasterGR8P(maxReqSize, 1);
    if (m_useCache)
      TImageCache::instance()->add(m_cacheId, TRasterImageP(outRas), true);
    else
      m_raster = outRas;
  }

  outRas->lock();
  char* buffer = (char*) outRas->getRawData();
  if (!buffer)
    return 0;

  inRas->lock();
  char* inData = (char*) inRas->getRawData();

  size_t outSize = maxReqSize;
  snappy_status r = snappy_compress(inData, inDataSize, buffer, &outSize);

  outRas->unlock();
  inRas->unlock();

  if(r != SNAPPY_OK)
    throw TException("compress... something goes bad");

  return outSize;
 }

//------------------------------------------------------------------------------

TRasterP TRasterCodecSnappy::compress(const TRasterP &inRas, int allocUnit,
TINT32 &outDataSize)
{
  TRasterGR8P rasOut;
  UINT outSize = doCompress(inRas, allocUnit, rasOut);
  if (outSize==0)
    return TRasterP();

  UINT headerSize = sizeof(Header);
  if (TBigMemoryManager::instance()->isActive() &&
      TBigMemoryManager::instance()->getAvailableMemoryinKb()<((outSize +
headerSize)>>10))
        return TRasterP();

  TRasterGR8P r8(outSize + headerSize, 1);
  r8->lock();
  UCHAR *memoryChunk = r8->getRawData();
  if (!memoryChunk)
    return TRasterP();
  Header head(inRas);

  memcpy(memoryChunk, &head, headerSize);
  UCHAR *tmp = memoryChunk + headerSize;
  rasOut->lock();
  memcpy(tmp, rasOut->getRawData(), outSize);
  r8->unlock();
  rasOut->unlock();
  outDataSize = outSize + headerSize;
  return r8;
}

//------------------------------------------------------------------------------

bool TRasterCodecSnappy::decompress(const UCHAR* inData, TINT32 inDataSize,
TRasterP &outRas, bool safeMode)
{
  int headerSize = sizeof(Header);

  Header *header= (Header *)inData;
  if (!outRas)
    {
    outRas = header->createRaster();
    if (!outRas)
      throw TException();
    }
  else
    {
    if (outRas->getLx() != outRas->getWrap())
      throw TException();
    }

  int outDataSize = header->getRasterSize();

  char* mc = (char*) inData + headerSize;
  int ds = inDataSize - headerSize;

  size_t outSize;
  snappy_uncompressed_length(mc, ds, &outSize);

  outRas->lock();
  snappy_status rc = snappy_uncompress(mc, ds, (char*) outRas->getRawData(),
&outSize);
  outRas->unlock();

  if (rc != SNAPPY_OK)
    {
    if (safeMode)
      return false;
    else
      {
      throw TException("decompress... something goes bad");
      return false;
      }
    }

  assert(outSize == (size_t)outDataSize);
  return true;
}

//------------------------------------------------------------------------------

void TRasterCodecSnappy::decompress(const TRasterP & compressedRas, TRasterP
&outRas)
{
  int headerSize = sizeof(Header);

  assert(compressedRas->getLy()==1 && compressedRas->getPixelSize()==1);
  UINT inDataSize = compressedRas->getLx();

  compressedRas->lock();

  UCHAR* inData = compressedRas->getRawData();
  Header header(inData);

  if (!outRas)
    {
    outRas = header.createRaster();
    if (!outRas)
      throw TException();
    }
  else
    {
    if (outRas->getLx() != outRas->getWrap())
      throw TException();
    }

  int outDataSize = header.getRasterSize();

  char* mc = (char*) inData + headerSize;
  int ds = inDataSize - headerSize;

  size_t outSize;
  snappy_uncompressed_length(mc, ds, &outSize);

  char* outData = (char*) outRas->getRawData();

  outRas->lock();

  snappy_status rc = snappy_uncompress(mc, ds, outData, &outSize);

  outRas->unlock();
  compressedRas->unlock();

  if (rc != SNAPPY_OK)
    throw TException("decompress... something goes bad");

  assert(outSize == (size_t)outDataSize);
}*/

//------------------------------------------------------------------------------
//	TRasterCodecLz4
//------------------------------------------------------------------------------

namespace {
bool lz4decompress(LZ4F_decompressionContext_t lz4dctx, char *out,
                   size_t *out_len_res, const char *in, size_t in_len) {
  size_t out_len = *out_len_res, in_read, out_written;

  *out_len_res = 0;

  while (in_len) {
    out_written = out_len;
    in_read     = in_len;

    size_t res = LZ4F_decompress(lz4dctx, (void *)out, &out_written,
                                 (const void *)in, &in_read, NULL);

    if (LZ4F_isError(res)) return false;

    *out_len_res += out_written;

    out += out_written;
    out_len -= out_written;

    in += in_read;
    in_len -= in_read;
  }

  return true;
}
}  // namespace

TRasterCodecLz4::TRasterCodecLz4(const std::string &name, bool useCache)
    : TRasterCodec(name), m_raster(), m_useCache(useCache), m_cacheId("") {}

//------------------------------------------------------------------------------

TRasterCodecLz4::~TRasterCodecLz4() {
  if (m_useCache)
    TImageCache::instance()->remove(m_cacheId);
  else
    m_raster = TRasterGR8P();
}

//------------------------------------------------------------------------------

UINT TRasterCodecLz4::doCompress(const TRasterP &inRas, int allocUnit,
                                 TRasterGR8P &outRas) {
  assert(inRas);

  assert(inRas->getLx() == inRas->getWrap());

  size_t inDataSize = inRas->getLx() * inRas->getLy() * inRas->getPixelSize();
  size_t maxReqSize = LZ4F_compressFrameBound(inDataSize, NULL);

  if (m_useCache) {
    if (m_cacheId == "")
      m_cacheId = TImageCache::instance()->getUniqueId();
    else
      outRas = ((TRasterImageP)TImageCache::instance()->get(m_cacheId, true))
                   ->getRaster();
  } else
    outRas = m_raster;

  if (!outRas || outRas->getLx() < (int)maxReqSize) {
    outRas   = TRasterGR8P();
    m_raster = TRasterGR8P();
    if (m_useCache) TImageCache::instance()->remove(m_cacheId);
    outRas = TRasterGR8P(maxReqSize, 1);
    if (m_useCache)
      TImageCache::instance()->add(m_cacheId, TRasterImageP(outRas), true);
    else
      m_raster = outRas;
  }

  outRas->lock();
  void *buffer = (void *)outRas->getRawData();
  if (!buffer) return 0;

  inRas->lock();
  const void *inData = (const void *)inRas->getRawData();

  size_t outSize =
      LZ4F_compressFrame(buffer, maxReqSize, inData, inDataSize, NULL);
  outRas->unlock();
  inRas->unlock();

  if (LZ4F_isError(outSize)) throw TException("compress... something goes bad");

  return outSize;
}

//------------------------------------------------------------------------------

TRasterP TRasterCodecLz4::compress(const TRasterP &inRas, int allocUnit,
                                   TINT32 &outDataSize) {
  TRasterGR8P rasOut;
  UINT outSize = doCompress(inRas, allocUnit, rasOut);
  if (outSize == 0) return TRasterP();

  UINT headerSize = sizeof(Header);
  if (TBigMemoryManager::instance()->isActive() &&
      TBigMemoryManager::instance()->getAvailableMemoryinKb() <
          ((outSize + headerSize) >> 10))
    return TRasterP();

  TRasterGR8P r8(outSize + headerSize, 1);
  r8->lock();
  UCHAR *memoryChunk = r8->getRawData();
  if (!memoryChunk) return TRasterP();
  Header head(inRas);

  memcpy(memoryChunk, &head, headerSize);
  UCHAR *tmp = memoryChunk + headerSize;
  rasOut->lock();
  memcpy(tmp, rasOut->getRawData(), outSize);
  r8->unlock();
  rasOut->unlock();
  outDataSize = outSize + headerSize;
  return r8;
}

//------------------------------------------------------------------------------

bool TRasterCodecLz4::decompress(const UCHAR *inData, TINT32 inDataSize,
                                 TRasterP &outRas, bool safeMode) {
  // --- Input validation ---
  if (!inData) {
    if (safeMode) return false;
    throw TException("Compressed data pointer is null");
  }
  int headerSize = sizeof(Header);
  if (inDataSize <= headerSize) {
    if (safeMode) return false;
    throw TException("Compressed data too small");
  }

  Header *header = (Header *)inData;

  // --- Header validation ---
  if (header->m_lx <= 0 || header->m_ly <= 0) {
    if (safeMode) return false;
    throw TException("Invalid raster dimensions (zero or negative)");
  }

  if (header->m_rasType < Header::Raster32RGBM || header->m_rasType > Header::RasterGR16) {
    if (safeMode) return false;
    throw TException("Invalid raster type");
  }

  int pixelSize = header->getPixelSize();
  if (pixelSize == 0) {
    if (safeMode) return false;
    throw TException("Invalid raster type (unknown pixel size)");
  }

  // Compute required bytes, checking for overflow
  size_t neededBytes = (size_t)header->m_lx * (size_t)header->m_ly * (size_t)pixelSize;
  const size_t MAX_RASTER_BYTES = 2ULL * 1024 * 1024 * 1024; // 2 GB
  if (neededBytes > MAX_RASTER_BYTES) {
    if (safeMode) return false;
    throw TException("Raster too large (>2GB)");
  }

  // Check available memory if memory manager is active
  if (TBigMemoryManager::instance()->isActive()) {
    size_t neededKB = (neededBytes + 1023) >> 10; // round up to KB
    if (TBigMemoryManager::instance()->getAvailableMemoryinKb() < neededKB) {
      if (safeMode) return false;
      throw TException("Not enough memory to decompress image");
    }
  }

  // --- Create raster if needed ---
  if (!outRas) {
    outRas = header->createRaster();
    if (!outRas) {
      if (safeMode) return false;
      throw TException("Failed to create raster");
    }
  } else {
    if (outRas->getLx() != outRas->getWrap()) {
      if (safeMode) return false;
      throw TException("Invalid raster wrap");
    }
  }

  // Verify raster has valid raw data
  outRas->lock();
  if (!outRas->getRawData()) {
    outRas->unlock();
    if (safeMode) return false;
    throw TException("Raster raw data is null");
  }
  outRas->unlock();

  // --- Decompress ---
  LZ4F_decompressionContext_t lz4dctx;
  LZ4F_errorCode_t err =
      LZ4F_createDecompressionContext(&lz4dctx, LZ4F_VERSION);
  if (LZ4F_isError(err)) {
    if (safeMode) return false;
    throw TException("compress... something goes bad");
  }

  int outDataSize = header->getRasterSize();
  const char *mc = (const char *)(inData + headerSize);
  size_t ds = inDataSize - headerSize;
  size_t outSize = outDataSize;
  char *outData = (char *)outRas->getRawData();

  outRas->lock();
  bool ok = lz4decompress(lz4dctx, outData, &outSize, mc, ds);
  LZ4F_freeDecompressionContext(lz4dctx);
  outRas->unlock();

  if (!ok) {
    if (safeMode) return false;
    throw TException("decompress... something goes bad");
  }

  assert(outSize == (size_t)outDataSize);
  return true;
}

//------------------------------------------------------------------------------

void TRasterCodecLz4::decompress(const TRasterP &compressedRas,
                                 TRasterP &outRas) {
  // --- Input validation ---
  if (!compressedRas) {
    throw TException("Compressed raster is null");
  }
  if (compressedRas->getLy() != 1 || compressedRas->getPixelSize() != 1) {
    throw TException("Invalid compressed raster format");
  }
  UINT inDataSize = compressedRas->getLx();
  int headerSize = sizeof(Header);
  if (inDataSize <= headerSize) {
    throw TException("Compressed raster too small");
  }

  compressedRas->lock();
  UCHAR *inData = compressedRas->getRawData();
  if (!inData) {
    compressedRas->unlock();
    throw TException("Compressed raster has no raw data");
  }

  Header header(inData);

  // --- Header validation ---
  if (header.m_lx <= 0 || header.m_ly <= 0) {
    compressedRas->unlock();
    throw TException("Invalid raster dimensions (zero or negative)");
  }

  if (header.m_rasType < Header::Raster32RGBM || header.m_rasType > Header::RasterGR16) {
    compressedRas->unlock();
    throw TException("Invalid raster type");
  }

  int pixelSize = header.getPixelSize();
  if (pixelSize == 0) {
    compressedRas->unlock();
    throw TException("Invalid raster type (unknown pixel size)");
  }

  size_t neededBytes = (size_t)header.m_lx * (size_t)header.m_ly * (size_t)pixelSize;
  const size_t MAX_RASTER_BYTES = 2ULL * 1024 * 1024 * 1024; // 2 GB
  if (neededBytes > MAX_RASTER_BYTES) {
    compressedRas->unlock();
    throw TException("Raster too large (>2GB)");
  }

  if (TBigMemoryManager::instance()->isActive()) {
    size_t neededKB = (neededBytes + 1023) >> 10;
    if (TBigMemoryManager::instance()->getAvailableMemoryinKb() < neededKB) {
      compressedRas->unlock();
      throw TException("Not enough memory to decompress image");
    }
  }

  // --- Create raster ---
  if (!outRas) {
    outRas = header.createRaster();
    if (!outRas) {
      compressedRas->unlock();
      throw TException("Failed to create raster");
    }
  } else {
    if (outRas->getLx() != outRas->getWrap()) {
      compressedRas->unlock();
      throw TException("Invalid raster wrap");
    }
  }

  // Verify raster has valid raw data
  outRas->lock();
  if (!outRas->getRawData()) {
    outRas->unlock();
    compressedRas->unlock();
    throw TException("Raster raw data is null");
  }
  outRas->unlock();

  // --- Decompress ---
  LZ4F_decompressionContext_t lz4dctx;
  LZ4F_errorCode_t err =
      LZ4F_createDecompressionContext(&lz4dctx, LZ4F_VERSION);
  if (LZ4F_isError(err)) {
    compressedRas->unlock();
    throw TException("compress... something goes bad");
  }

  int outDataSize = header.getRasterSize();
  const char *mc = (const char *)(inData + headerSize);
  size_t ds = inDataSize - headerSize;
  size_t outSize = outDataSize;
  char *outData = (char *)outRas->getRawData();

  outRas->lock();
  bool ok = lz4decompress(lz4dctx, outData, &outSize, mc, ds);
  LZ4F_freeDecompressionContext(lz4dctx);
  outRas->unlock();
  compressedRas->unlock();

  if (!ok) throw TException("decompress... something goes bad");
  assert(outSize == (size_t)outDataSize);
}

//------------------------------------------------------------------------------
//	TRasterCodecLZO
//------------------------------------------------------------------------------

namespace {

bool lzoCompress(const QByteArray src, QByteArray &dst) {
#ifdef _WIN32
  QDir exeDir;
  if (QCoreApplication::applicationName() == "ToonzPreview") {
    QStringList paths = QCoreApplication::libraryPaths();
    exeDir            = QDir(paths.first());
  } else
    exeDir = QDir(QCoreApplication::applicationDirPath());
#else
  QDir exeDir(QCoreApplication::applicationDirPath());
#endif  // _WIN32

  QString compressExe = exeDir.filePath("lzocompress");
  QProcess process;
  process.start(compressExe, QStringList() << QString::number(src.size()));
  if (!process.waitForStarted()) return false;
  process.write(src);
  if (!process.waitForFinished()) return false;
  dst = process.readAll();
  return process.exitCode() == 0;
}

bool lzoDecompress(const QByteArray src, int dstSize, QByteArray &dst) {
#ifdef _WIN32
  QDir exeDir;
  if (QCoreApplication::applicationName() == "ToonzPreview") {
    QStringList paths = QCoreApplication::libraryPaths();
    exeDir            = QDir(paths.first());
    exeDir.cdUp();
  } else
    exeDir = QDir(QCoreApplication::applicationDirPath());
#else
  QDir exeDir(QCoreApplication::applicationDirPath());
#endif  // _WIN32
  QString decompressExe = exeDir.filePath("lzodecompress");
  QProcess process;
  process.start(decompressExe, QStringList() << QString::number(dstSize)
                                             << QString::number(src.size()));
  if (!process.waitForStarted()) return false;
  process.write(src);
  if (!process.waitForFinished()) return false;
  dst = process.readAll();
  return process.exitCode() == 0 && dst.size() == dstSize;
}
}  // namespace

//------------------------------------------------------------------------------

TRasterCodecLZO::TRasterCodecLZO(const std::string &name, bool useCache)
    : TRasterCodec(name), m_raster(), m_useCache(useCache), m_cacheId("") {}

//------------------------------------------------------------------------------

TRasterCodecLZO::~TRasterCodecLZO() {
  if (m_useCache)
    TImageCache::instance()->remove(m_cacheId);
  else
    m_raster = TRasterGR8P();
}

//------------------------------------------------------------------------------

UINT TRasterCodecLZO::doCompress(const TRasterP &inRas, int allocUnit,
                                 TRasterGR8P &outRas) {
  assert(inRas);

  assert(inRas->getLx() == inRas->getWrap());

  size_t inDataSize = inRas->getLx() * inRas->getLy() * inRas->getPixelSize();

  // compress data
  inRas->lock();
  char *inData = (char *)inRas->getRawData();
  QByteArray compressedBuffer;
  if (!lzoCompress(QByteArray(inData, inDataSize), compressedBuffer))
    throw TException("LZO compression failed");

  inRas->unlock();

  size_t maxReqSize = compressedBuffer.size();  // we have just done the
                                                // compression: we know the
                                                // actual size

  if (m_useCache) {
    if (m_cacheId == "")
      m_cacheId = TImageCache::instance()->getUniqueId();
    else
      outRas = ((TRasterImageP)TImageCache::instance()->get(m_cacheId, true))
                   ->getRaster();
  } else
    outRas = m_raster;

  if (!outRas || outRas->getLx() < (int)maxReqSize) {
    outRas   = TRasterGR8P();
    m_raster = TRasterGR8P();
    if (m_useCache) TImageCache::instance()->remove(m_cacheId);
    outRas = TRasterGR8P(maxReqSize, 1);
    if (m_useCache)
      TImageCache::instance()->add(m_cacheId, TRasterImageP(outRas), true);
    else
      m_raster = outRas;
  }

  size_t outSize = maxReqSize;
  outRas->lock();
  char *buffer = (char *)outRas->getRawData();  // Change cast types, if needed
  if (!buffer) {
    outRas->unlock();
    return 0;
  }
  memcpy(buffer, compressedBuffer.data(), outSize);
  outRas->unlock();

  return outSize;
}

//------------------------------------------------------------------------------

TRasterP TRasterCodecLZO::compress(const TRasterP &inRas, int allocUnit,
                                   TINT32 &outDataSize) {
  TRasterGR8P rasOut;
  UINT outSize = doCompress(inRas, allocUnit, rasOut);
  if (outSize == 0) return TRasterP();

  UINT headerSize = sizeof(Header);
  if (TBigMemoryManager::instance()->isActive() &&
      TBigMemoryManager::instance()->getAvailableMemoryinKb() <
          ((outSize + headerSize) >> 10))
    return TRasterP();

  TRasterGR8P r8(outSize + headerSize, 1);
  r8->lock();
  UCHAR *memoryChunk = r8->getRawData();
  if (!memoryChunk) return TRasterP();
  Header head(inRas);

  memcpy(memoryChunk, &head, headerSize);
  UCHAR *tmp = memoryChunk + headerSize;
  rasOut->lock();
  memcpy(tmp, rasOut->getRawData(), outSize);
  r8->unlock();
  rasOut->unlock();
  outDataSize = outSize + headerSize;
  return r8;
}

//------------------------------------------------------------------------------

bool TRasterCodecLZO::decompress(const UCHAR *inData, TINT32 inDataSize,
                                 TRasterP &outRas, bool safeMode) {
  // --- Input validation ---
  if (!inData) {
    if (safeMode) return false;
    throw TException("Compressed data pointer is null");
  }
  int headerSize = sizeof(Header);
  if (inDataSize <= headerSize) {
    if (safeMode) return false;
    throw TException("Compressed data too small");
  }

  Header *header = (Header *)inData;

  // --- Header validation ---
  if (header->m_lx <= 0 || header->m_ly <= 0) {
    if (safeMode) return false;
    throw TException("Invalid raster dimensions (zero or negative)");
  }

  if (header->m_rasType < Header::Raster32RGBM || header->m_rasType > Header::RasterGR16) {
    if (safeMode) return false;
    throw TException("Invalid raster type");
  }

  int pixelSize = header->getPixelSize();
  if (pixelSize == 0) {
    if (safeMode) return false;
    throw TException("Invalid raster type (unknown pixel size)");
  }

  size_t neededBytes = (size_t)header->m_lx * (size_t)header->m_ly * (size_t)pixelSize;
  const size_t MAX_RASTER_BYTES = 2ULL * 1024 * 1024 * 1024; // 2 GB
  if (neededBytes > MAX_RASTER_BYTES) {
    if (safeMode) return false;
    throw TException("Raster too large (>2GB)");
  }

  if (TBigMemoryManager::instance()->isActive()) {
    size_t neededKB = (neededBytes + 1023) >> 10;
    if (TBigMemoryManager::instance()->getAvailableMemoryinKb() < neededKB) {
      if (safeMode) return false;
      throw TException("Not enough memory to decompress image");
    }
  }

  // --- Create raster ---
  if (!outRas) {
    outRas = header->createRaster();
    if (!outRas) {
      if (safeMode) return false;
      throw TException("Failed to create raster");
    }
  } else {
    if (outRas->getLx() != outRas->getWrap()) {
      if (safeMode) return false;
      throw TException("Invalid raster wrap");
    }
  }

  // Verify raster has valid raw data
  outRas->lock();
  if (!outRas->getRawData()) {
    outRas->unlock();
    if (safeMode) return false;
    throw TException("Raster raw data is null");
  }
  outRas->unlock();

  // --- Decompress ---
  int outDataSize = header->getRasterSize();
  char *mc = (char *)inData + headerSize;
  int ds = inDataSize - headerSize;

  size_t outSize = outDataSize;
  QByteArray decompressedBuffer;
  if (!lzoDecompress(QByteArray(mc, ds), outSize, decompressedBuffer)) {
    if (safeMode) return false;
    throw TException("LZO decompression failed");
  }

  outRas->lock();
  memcpy(outRas->getRawData(), decompressedBuffer.data(),
         decompressedBuffer.size());
  outRas->unlock();

  assert(outSize == (size_t)outDataSize);
  return true;
}

//------------------------------------------------------------------------------

void TRasterCodecLZO::decompress(const TRasterP &compressedRas,
                                 TRasterP &outRas) {
  // --- Input validation ---
  if (!compressedRas) {
    throw TException("Compressed raster is null");
  }
  if (compressedRas->getLy() != 1 || compressedRas->getPixelSize() != 1) {
    throw TException("Invalid compressed raster format");
  }
  UINT inDataSize = compressedRas->getLx();
  int headerSize = sizeof(Header);
  if (inDataSize <= headerSize) {
    throw TException("Compressed raster too small");
  }

  compressedRas->lock();
  UCHAR *inData = compressedRas->getRawData();
  if (!inData) {
    compressedRas->unlock();
    throw TException("Compressed raster has no raw data");
  }

  Header header(inData);

  // --- Header validation ---
  if (header.m_lx <= 0 || header.m_ly <= 0) {
    compressedRas->unlock();
    throw TException("Invalid raster dimensions (zero or negative)");
  }

  if (header.m_rasType < Header::Raster32RGBM || header.m_rasType > Header::RasterGR16) {
    compressedRas->unlock();
    throw TException("Invalid raster type");
  }

  int pixelSize = header.getPixelSize();
  if (pixelSize == 0) {
    compressedRas->unlock();
    throw TException("Invalid raster type (unknown pixel size)");
  }

  size_t neededBytes = (size_t)header.m_lx * (size_t)header.m_ly * (size_t)pixelSize;
  const size_t MAX_RASTER_BYTES = 2ULL * 1024 * 1024 * 1024; // 2 GB
  if (neededBytes > MAX_RASTER_BYTES) {
    compressedRas->unlock();
    throw TException("Raster too large (>2GB)");
  }

  if (TBigMemoryManager::instance()->isActive()) {
    size_t neededKB = (neededBytes + 1023) >> 10;
    if (TBigMemoryManager::instance()->getAvailableMemoryinKb() < neededKB) {
      compressedRas->unlock();
      throw TException("Not enough memory to decompress image");
    }
  }

  // --- Create raster ---
  if (!outRas) {
    outRas = header.createRaster();
    if (!outRas) {
      compressedRas->unlock();
      throw TException("Failed to create raster");
    }
  } else {
    if (outRas->getLx() != outRas->getWrap()) {
      compressedRas->unlock();
      throw TException("Invalid raster wrap");
    }
  }

  // Verify raster has valid raw data
  outRas->lock();
  if (!outRas->getRawData()) {
    outRas->unlock();
    compressedRas->unlock();
    throw TException("Raster raw data is null");
  }
  outRas->unlock();

  // --- Decompress ---
  int outDataSize = header.getRasterSize();
  char *mc = (char *)inData + headerSize;
  int ds = inDataSize - headerSize;

  size_t outSize = outDataSize;
  QByteArray decompressedBuffer;
  if (!lzoDecompress(QByteArray(mc, ds), outSize, decompressedBuffer)) {
    compressedRas->unlock();
    throw TException("LZO decompression failed");
  }

  outRas->lock();
  memcpy(outRas->getRawData(), decompressedBuffer.data(),
         decompressedBuffer.size());
  outRas->unlock();
  compressedRas->unlock();

  assert(outSize == (size_t)outDataSize);
}