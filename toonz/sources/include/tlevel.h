#pragma once

#ifndef TLEVEL_INCLUDED
#define TLEVEL_INCLUDED

#include "timage.h"
#include "tfilepath.h"

#undef DVAPI
#undef DVVAR
#ifdef TIMAGE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TLevel final : public TSmartObject {
  DECLARE_CLASS_CODE
public:
  typedef std::map<const TFrameId, TImageP> Table;
  typedef Table::iterator Iterator;

private:
  std::string m_name;
  Table *m_table;
  TPalette *m_palette;
  bool m_partialLoad = false; // Added for partial loading

public:
  TLevel() : m_table(new Table()), m_palette(nullptr), m_partialLoad(false) {}
  ~TLevel() { delete m_table; }

private:
  // Not implemented
  TLevel(const TLevel &);
  TLevel &operator=(const TLevel &);

public:
  // Name
  std::string getName() const;
  void setName(std::string name);

  // Frames
  int getFrameCount() const { return (int)m_table->size(); }
  const TImageP &frame(const TFrameId fid);
  const TImageP &frame(int f) { return frame(TFrameId(f)); }
  void setFrame(const TFrameId &fid, const TImageP &img);

  Iterator begin() { return m_table->begin(); }
  Iterator end() { return m_table->end(); }

  // Temporary: used by tinytoonz/filmstrip
  Table *getTable() { return m_table; }

  TPalette *getPalette();
  void setPalette(TPalette *);

  // Enables or disables partial frame loading
  void setPartialLoad(bool enable) { m_partialLoad = enable; }
  // Checks if partial loading is enabled
  bool isPartialLoadEnabled() const { return m_partialLoad; }
};

#ifdef _WIN32
template class DVAPI TSmartPointerT<TLevel>;
#endif

class DVAPI TLevelP final : public TSmartPointerT<TLevel> {
public:
  TLevelP() : TSmartPointerT<TLevel>(new TLevel) {}
  TLevelP(TLevel *level) : TSmartPointerT<TLevel>(level) {}
};

#endif