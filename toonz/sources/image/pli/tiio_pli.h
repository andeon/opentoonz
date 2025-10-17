#pragma once

#ifndef TTIO_PLI_INCLUDED
#define TTIO_PLI_INCLUDED

#include <memory>
#include <vector>

#include "tlevel_io.h"

class GroupTag;
class ParsedPli;
class ImageTag;
class TImageWriterPli;
class TImageReaderPli;

//===========================================================================
/*
class TWriterInfoPli final : public TWriterInfo {

public:
 ~TWriterInfoPli() {}
  static TWriterInfo *create(const std::string &ext) { return new
TWriterInfoPli(); }
  TWriterInfo *clone() const {
  return new TWriterInfoPli(*this); }

private:
  TWriterInfoPli() {}
  TWriterInfoPli(const TWriterInfoPli&) {}
};
*/
//===========================================================================

/*!
  TLevelWriterPli:
 */
class TLevelWriterPli final : public TLevelWriter {
  //! object to manage a pli
  std::unique_ptr<ParsedPli> m_pli;

  //! number of frame in pli
  UINT m_frameNumber;

  //  vettore da utilizzare per il calcolo della palette
  std::vector<TPixel> m_colorArray;

public:
  TLevelWriterPli(const TFilePath &path, TPropertyGroup *winfo);
  ~TLevelWriterPli();
  TImageWriterP getFrameWriter(TFrameId fid) override;

  friend class TImageWriterPli;

public:
  static TLevelWriter *create(const TFilePath &f, TPropertyGroup *winfo) {
    return new TLevelWriterPli(f, winfo);
  }

private:
  // not implemented
  TLevelWriterPli(const TLevelWriterPli &);
  TLevelWriterPli &operator=(const TLevelWriterPli &);
};

//===========================================================================

typedef std::pair<ImageTag *, bool> pliFrameInfo;

/*!
  TLevelReaderPli:
 */
class TLevelReaderPli final : public TLevelReader {
public:
  TLevelReaderPli(const TFilePath &path);
  ~TLevelReaderPli();

  /*!
Return info about current pli
*/
  TLevelP loadInfo() override;
  void doReadPalette(bool doReadIt) override;

  /*!
Return an image with Reader information
*/
  TImageReaderP getFrameReader(TFrameId fid) override;

  QString getCreator() override;
  
  // ===== CORRUPTION RECOVERY METHODS =====
  
  /*!
   * Check if the PLI file is likely recoverable despite corruption
   * Returns true if basic file structure is intact enough for partial recovery
   */
  bool isFileLikelyRecoverable() const;
  
  /*!
   * Get list of frames that can likely be recovered from a corrupted file
   * Useful for partial loading when some frames are damaged
   */
  std::vector<TFrameId> getRecoverableFrames() const;
  
  /*!
   * Enable/disable corruption recovery mode
   * When enabled, the loader will attempt to salvage data from corrupted frames
   */
  void enableCorruptionRecovery(bool enable) { m_recoveryEnabled = enable; }
  
  /*!
   * Check if corruption recovery is enabled
   */
  bool isCorruptionRecoveryEnabled() const { return m_recoveryEnabled; }
  
  /*!
   * Get information about what parts of the file are corrupted
   * Returns a human-readable string describing corruption issues
   */
  QString getCorruptionInfo() const;

  friend class TImageReaderPli;

private:
  bool m_init;
  //! struct which contains reference to frame
  std::map<TFrameId, pliFrameInfo> m_mapOfImage;

  //! Reference to pli palette
  TPixel *m_palette;
  bool m_readPalette;
  //!
  TUINT32 m_paletteCount;

  //! flag to check if file exists
  bool m_doesExist;

  //! object to manage a pli
  ParsedPli *m_pli;
  TLevelP m_level;
  
  // ===== CORRUPTION RECOVERY FIELDS =====
  
  //! Enable/disable recovery mode for corrupted files
  bool m_recoveryEnabled;
  
  //! Track which frames were recovered vs normally loaded
  mutable std::map<TFrameId, bool> m_recoveredFrames;
  
  //! Store corruption information for diagnostics
  mutable QString m_corruptionInfo;

public:
  static TLevelReader *create(const TFilePath &f) {
    return new TLevelReaderPli(f);
  }

private:
  // not implemented
  TLevelReaderPli(const TLevelReaderPli &);
  TLevelReaderPli &operator=(const TLevelReaderPli &);
};

//===========================================================================
/*
Classe locale per la lettura di un frame del livello.
*/
class TImageReaderPli final : public TImageReader {
public:
  TFrameId m_frameId;  //<! Current frame id

private:
  // not implemented
  TImageReaderPli(const TImageReaderPli &);
  TImageReaderPli &operator=(const TImageReaderPli &src);

public:
  TImageReaderPli(const TFilePath &f, const TFrameId &frameId,
                  TLevelReaderPli *);
  ~TImageReaderPli() {}

  TImageP load() override;
  TImageP doLoad();
  
  // ===== CORRUPTION RECOVERY METHODS =====
  
  /*!
   * Attempt to load image with recovery from corruption
   * Skips corrupted objects and continues loading valid ones
   * Returns partial image with recoverable data
   */
  TImageP doLoadWithRecovery();
  
  /*!
   * Check if this frame was recovered from corruption
   */
  bool wasRecovered() const { return m_wasRecovered; }
  
  /*!
   * Get information about what parts of the frame were corrupted
   * Returns a human-readable string describing recovery actions
   */
  QString getRecoveryInfo() const;

  TDimension getSize() const;

  TRect getBBox() const;

private:
  //! Size of image
  int m_lx, m_ly;

  //! Reference to level reader
  TLevelReaderPli *m_lrp;
  
  // ===== CORRUPTION RECOVERY FIELDS =====
  
  //! Track if this frame was recovered from corruption
  bool m_wasRecovered;
  
  //! Store recovery information for diagnostics
  mutable QString m_recoveryInfo;
};

// Functions

TPalette *readPalette(GroupTag *paletteTag, int majorVersion, int minorVersion);

// ===== CORRUPTION RECOVERY UTILITY FUNCTIONS =====

namespace PLIRecoveryUtils {
  /*!
   * Validate PLI file structure for basic integrity
   * Returns true if file has valid basic structure for recovery attempts
   */
  bool validatePliStructure(const TFilePath &filePath);
  
  /*!
   * Extract salvageable frame information from corrupted PLI
   * Returns list of frames that appear to have recoverable data
   */
  std::vector<TFrameId> extractRecoverableFrames(const TFilePath &filePath);
  
  /*!
   * Create a default palette for use when palette data is corrupted
   */
  TPalette* createDefaultRecoveryPalette();
  
  /*!
   * Log recovery actions for debugging and user information
   */
  void logRecoveryAction(const QString &action, const QString &details = "");
}

#endif  // TTIO_PLI_INCLUDED