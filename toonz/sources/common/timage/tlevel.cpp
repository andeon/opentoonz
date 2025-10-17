

#include "tlevel.h"
#include "tpalette.h"

// Defines the class code for TSmartObject
DEFINE_CLASS_CODE(TLevel, 7)

// Returns the name of the level
std::string TLevel::getName() const { return m_name; }

// Sets the name of the level
void TLevel::setName(std::string name) { m_name = name; }

// Retrieves the image associated with the specified frame ID
// Returns an empty TImageP if the frame is not found
const TImageP &TLevel::frame(const TFrameId fid) {
  const static TImageP none;
  assert(m_table);
  Iterator it = m_table->find(fid);
  if (it == m_table->end())
    return none;  // Returns an empty image if the frame ID is not found
  else
    return it->second;
}

// Sets the image for the specified frame ID and assigns the level's palette
void TLevel::setFrame(const TFrameId &fid, const TImageP &img) {
  assert(m_table);
  if (img) img->setPalette(getPalette());
  (*m_table)[fid] = img;
}

// Commented-out code: Retrieves the index of a frame ID in the table
// Note: This implementation is considered inefficient and is not currently used
/*
int TLevel::getIndex(const TFrameId fid)
{
  int index = 0;
  for(Iterator it = m_table->begin(); it != m_table->end(); it++, index++)
    if(it->first == fid) return index;
  return -1;
}
*/

// Returns the palette associated with the level
TPalette *TLevel::getPalette() { return m_palette; }

// Sets the palette for the level and updates all frame images
void TLevel::setPalette(TPalette *palette) {
  if (m_palette == palette) return;
  if (palette) palette->addRef();
  if (m_palette) m_palette->release();
  m_palette = palette;
  for (Iterator it = begin(); it != end(); ++it) {
    TImageP &img = it->second;
    if (img) img->setPalette(m_palette);
  }
}