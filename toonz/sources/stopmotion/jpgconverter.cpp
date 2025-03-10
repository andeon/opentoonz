#include "jpgconverter.h"
#include <QFile>
#include <QDataStream>
#include <cstdint>  // For uint64_t
#include <cstdio>   // For fopen, fclose
#include <cstring>  // For memcpy

//=============================================================================
// Constructor and Destructor
//=============================================================================

JpgConverter::JpgConverter() {}
JpgConverter::~JpgConverter() {}

#ifdef WITH_CANON

//=============================================================================
// Canon-specific functionality
//=============================================================================

void JpgConverter::setStream(EdsStreamRef stream) { m_stream = stream; }

void JpgConverter::convertFromJpg() {
    uint64_t mySize = 0;
    unsigned char* data = nullptr;
    EdsError err = EdsGetPointer(m_stream, reinterpret_cast<EdsVoid**>(&data));
    err = EdsGetLength(m_stream, &mySize);

    if (err != EDS_ERR_OK || data == nullptr) {
        emit(imageReady(false));
        return;
    }

    int width, height, pixelFormat;
    int inSubsamp, inColorspace;
    tjhandle tjInstance = tjInitDecompress();
    if (!tjInstance) {
        emit(imageReady(false));
        return;
    }

    if (tjDecompressHeader3(tjInstance, data, mySize, &width, &height, &inSubsamp, &inColorspace) < 0) {
        emit(imageReady(false));
        tjDestroy(tjInstance);
        return;
    }

    if (width <= 0 || height <= 0) {
        emit(imageReady(false));
        tjDestroy(tjInstance);
        return;
    }

    pixelFormat = TJPF_BGRX;
    unsigned char* imgBuf = static_cast<unsigned char*>(tjAlloc(width * height * tjPixelSize[pixelFormat]));
    if (!imgBuf) {
        emit(imageReady(false));
        tjDestroy(tjInstance);
        return;
    }

    int flags = TJFLAG_BOTTOMUP;
    if (tjDecompress2(tjInstance, data, mySize, imgBuf, width, width * tjPixelSize[pixelFormat], height, pixelFormat, flags) < 0) {
        emit(imageReady(false));
        tjFree(imgBuf);
        tjDestroy(tjInstance);
        return;
    }

    m_finalImage = TRaster32P(width, height);
    m_finalImage->lock();
    uchar* rawData = m_finalImage->getRawData();
    memcpy(rawData, imgBuf, width * height * tjPixelSize[pixelFormat]);
    m_finalImage->unlock();

    tjFree(imgBuf);
    tjDestroy(tjInstance);

    if (m_stream) {
        EdsRelease(m_stream);
        m_stream = nullptr;
    }

    emit(imageReady(true));
}

void JpgConverter::run() { convertFromJpg(); }

#endif

//=============================================================================
// Save JPEG
//=============================================================================

void JpgConverter::saveJpg(TRaster32P image, TFilePath path) {
    unsigned char* jpegBuf = nullptr;
    unsigned long jpegSize = 0;
    int pixelFormat = TJPF_BGRX;
    int outQual = 95;
    int subSamp = TJSAMP_411;
    bool success = false;
    tjhandle tjInstance = tjInitCompress();

    if (!tjInstance) {
        return;
    }

    int width = image->getLx();
    int height = image->getLy();
    int flags = TJFLAG_BOTTOMUP;

    image->lock();
    uchar* rawData = image->getRawData();

    if (tjCompress2(tjInstance, rawData, width, 0, height, pixelFormat, &jpegBuf, &jpegSize, subSamp, outQual, flags) >= 0) {
        success = true;
    }

    image->unlock();
    tjDestroy(tjInstance);

    if (success) {
        QFile fullImage(path.getQString());
        if (fullImage.open(QIODevice::WriteOnly)) {
            QDataStream dataStream(&fullImage);
            dataStream.writeRawData(reinterpret_cast<const char*>(jpegBuf), jpegSize);
            fullImage.close();
        }
    }

    if (jpegBuf) {
        tjFree(jpegBuf);
    }
}

//=============================================================================
// Load JPEG
//=============================================================================

bool JpgConverter::loadJpg(TFilePath path, TRaster32P& image) {
    long size;
    int inSubsamp, inColorspace, width, height;
    unsigned long jpegSize;
    unsigned char* jpegBuf = nullptr;
    FILE* jpegFile = nullptr;
    QString qPath = path.getQString();
    QByteArray ba = qPath.toLocal8Bit();
    const char* c_path = ba.data();
    bool success = true;
    tjhandle tjInstance = nullptr;

    // Open the JPEG file
    jpegFile = fopen(c_path, "rb");
    if (!jpegFile) {
        success = false;
    }

    if (success && (fseek(jpegFile, 0, SEEK_END) < 0 || (size = ftell(jpegFile)) < 0 || fseek(jpegFile, 0, SEEK_SET) < 0)) {
        success = false;
    }

    if (success && size == 0) {
        success = false;
    }

    jpegSize = static_cast<unsigned long>(size);

    if (success && !(jpegBuf = static_cast<unsigned char*>(tjAlloc(jpegSize)))) {
        success = false;
    }

    if (success && fread(jpegBuf, jpegSize, 1, jpegFile) != 1) {
        success = false;
    }

    if (jpegFile) {
        fclose(jpegFile);
    }

    if (success && !(tjInstance = tjInitDecompress())) {
        success = false;
    }

    if (success && tjDecompressHeader3(tjInstance, jpegBuf, jpegSize, &width, &height, &inSubsamp, &inColorspace) < 0) {
        success = false;
    }

    int pixelFormat = TJPF_BGRX;
    unsigned char* imgBuf = nullptr;
    if (success && !(imgBuf = tjAlloc(width * height * tjPixelSize[pixelFormat])))) {
        success = false;
    }

    int flags = TJFLAG_BOTTOMUP;
    if (success && tjDecompress2(tjInstance, jpegBuf, jpegSize, imgBuf, width, 0, height, pixelFormat, flags) < 0) {
        success = false;
    }

    tjFree(jpegBuf);
    tjDestroy(tjInstance);

    if (success) {
        image = TRaster32P(width, height);
        image->lock();
        uchar* rawData = image->getRawData();
        memcpy(rawData, imgBuf, width * height * tjPixelSize[pixelFormat]);
        image->unlock();
    }

    if (imgBuf) {
        tjFree(imgBuf);
    }

    return success;
}
