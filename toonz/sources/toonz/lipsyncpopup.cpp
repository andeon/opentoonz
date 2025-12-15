

#include "lipsyncpopup.h"

// Tnz6 includes
#include "tapp.h"
#include "iocommand.h"
#include "menubarcommandids.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/icongenerator.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshcell.h"

// TnzCore includes
#include "filebrowsermodel.h"
#include "xsheetdragtool.h"
#include "historytypes.h"

// Qt includes
#include <QHBoxLayout>
#include <QPushButton>
#include <QMainWindow>
#include <QApplication>
#include <QTextStream>
#include <QPainter>
#include <array>

//=============================================================================
/*! \class LipSyncPopup
    \brief The LipSyncPopup class provides a modal dialog to apply lip sync
           text data to an image column.

    Inherits \b Dialog.
*/
//-----------------------------------------------------------------------------

//============================================================
//  Lip Sync Undo Class
//============================================================

class LipSyncUndo final : public TUndo {
public:
  LipSyncUndo(int col, TXshSimpleLevel *sl, TXshLevelP cl,
              std::vector<TFrameId> activeFrameIds, QStringList textLines,
              int size, std::vector<TFrameId> previousFrameIds,
              std::vector<TXshLevelP> previousLevels, int startFrame);
  void undo() const override;
  void redo() const override;
  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override {
    return QObject::tr("Apply Lip Sync Data");
  }
  int getHistoryType() override { return HistoryType::Xsheet; }

private:
  int m_col;
  int m_startFrame;
  TXshSimpleLevel *m_sl;
  TXshLevelP m_cl;
  QStringList m_textLines;
  int m_lastFrame;
  std::vector<TFrameId> m_previousFrameIds;
  std::vector<TXshLevelP> m_previousLevels;
  std::vector<TFrameId> m_activeFrameIds;
};

//-----------------------------------------------------------------------------
// LipSyncUndo Constructor
//-----------------------------------------------------------------------------

LipSyncUndo::LipSyncUndo(int col, TXshSimpleLevel *sl, TXshLevelP cl,
                         std::vector<TFrameId> activeFrameIds,
                         QStringList textLines, int lastFrame,
                         std::vector<TFrameId> previousFrameIds,
                         std::vector<TXshLevelP> previousLevels, int startFrame)
    : m_col(col)
    , m_startFrame(startFrame)
    , m_sl(sl)
    , m_cl(cl)
    , m_textLines(std::move(textLines))
    , m_lastFrame(lastFrame)
    , m_previousFrameIds(std::move(previousFrameIds))
    , m_previousLevels(std::move(previousLevels))
    , m_activeFrameIds(std::move(activeFrameIds)) {}

//-----------------------------------------------------------------------------
// LipSyncUndo::undo() - Restore previous cell states
//-----------------------------------------------------------------------------

void LipSyncUndo::undo() const {
  TXsheet *xsh = TApp::instance()->getCurrentScene()->getScene()->getXsheet();

  for (size_t i = 0; i < m_previousFrameIds.size(); ++i) {
    const int currFrame = static_cast<int>(i) + m_startFrame;
    TXshCell cell       = xsh->getCell(currFrame, m_col);
    cell.m_frameId      = m_previousFrameIds[i];
    cell.m_level        = m_previousLevels[i];
    xsh->setCell(currFrame, m_col, cell);
  }

  auto app = TApp::instance();
  app->getCurrentXsheet()->notifyXsheetChanged();
  app->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------
// LipSyncUndo::redo() - Apply lip sync data to cells
//-----------------------------------------------------------------------------

void LipSyncUndo::redo() const {
  TXsheet *xsh = TApp::instance()->getCurrentScene()->getScene()->getXsheet();
  int i        = 0;
  int currentLine = 0;

  // Pre-calculate phoneme to frame ID mapping
  const std::unordered_map<QString, TFrameId> phonemeMap = {
      {"ai", m_activeFrameIds[0]},    {"e", m_activeFrameIds[1]},
      {"o", m_activeFrameIds[2]},     {"u", m_activeFrameIds[3]},
      {"fv", m_activeFrameIds[4]},    {"l", m_activeFrameIds[5]},
      {"mbp", m_activeFrameIds[6]},   {"wq", m_activeFrameIds[7]},
      {"other", m_activeFrameIds[8]}, {"etc", m_activeFrameIds[8]},
      {"rest", m_activeFrameIds[9]}};

  while (currentLine < m_textLines.size()) {
    int endAt = (currentLine + 2 >= m_textLines.size())
                    ? m_lastFrame
                    : m_textLines.at(currentLine + 2).toInt() - 1;

    if (endAt <= 0) break;
    if (endAt <= i) {
      currentLine += 2;
      continue;
    }

    QString shape = m_textLines.at(currentLine + 1).toLower();

    auto it = phonemeMap.find(shape);
    if (it == phonemeMap.end() || it->second.isEmptyFrame()) {
      currentLine += 2;
      continue;
    }

    TFrameId currentId = it->second;

    while (i < endAt && i < m_lastFrame - m_startFrame) {
      const int currFrame = i + m_startFrame;
      TXshCell cell       = xsh->getCell(currFrame, m_col);

      cell.m_level   = m_sl ? m_sl : m_cl.getPointer();
      cell.m_frameId = currentId;
      xsh->setCell(currFrame, m_col, cell);
      i++;
    }
    currentLine += 2;
  }

  auto app = TApp::instance();
  app->getCurrentXsheet()->notifyXsheetChanged();
  app->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------
// LipSyncPopup Constructor
//-----------------------------------------------------------------------------

LipSyncPopup::LipSyncPopup()
    : Dialog(TApp::instance()->getMainWindow(), true, true, "LipSyncPopup")
    , m_sl(nullptr)
    , m_cl(nullptr)
    , m_childLevel()
    , m_col(0)
    , m_valid(false)
    , m_isEditingLevel(false) {
  setWindowTitle(tr("Apply Lip Sync Data"));
  setFixedSize(860, 400);

  // Create UI widgets
  m_applyButton = new QPushButton(tr("Apply"), this);
  m_applyButton->setEnabled(false);

  // Create phoneme labels
  m_aiLabel    = new QLabel(tr("A I Drawing"));
  m_oLabel     = new QLabel(tr("O Drawing"));
  m_eLabel     = new QLabel(tr("E Drawing"));
  m_uLabel     = new QLabel(tr("U Drawing"));
  m_lLabel     = new QLabel(tr("L Drawing"));
  m_wqLabel    = new QLabel(tr("W Q Drawing"));
  m_mbpLabel   = new QLabel(tr("M B P Drawing"));
  m_fvLabel    = new QLabel(tr("F V Drawing"));
  m_restLabel  = new QLabel(tr("Rest Drawing"));
  m_otherLabel = new QLabel(tr("C D G K N R S Th Y Z"));

  // Create input fields
  m_startAt   = new DVGui::IntLineEdit(this, 0);
  m_restToEnd = new QCheckBox(tr("Extend Rest Drawing to End Marker"), this);

  // Create placeholder images
  const int thumbnailWidth  = 160;
  const int thumbnailHeight = 90;
  QImage placeHolder(thumbnailWidth, thumbnailHeight, QImage::Format_ARGB32);
  placeHolder.fill(Qt::white);

  for (int i = 0; i < 10; ++i) {
    m_pixmaps[i]     = QPixmap::fromImage(placeHolder);
    m_imageLabels[i] = new QLabel(this);
    m_imageLabels[i]->setPixmap(m_pixmaps[i]);
    m_textLabels[i] = new QLabel("temp", this);
  }

  // Create navigation buttons
  for (int i = 0; i < 20; ++i) {
    if (!(i % 2)) {
      m_navButtons[i] = new QPushButton("<", this);
      m_navButtons[i]->setToolTip(tr("Previous Drawing"));
    } else {
      m_navButtons[i] = new QPushButton(">", this);
      m_navButtons[i]->setToolTip(tr("Next Drawing"));
    }
  }

  // Create file field
  m_file = new DVGui::FileField(this, QString(""));
  m_file->setFileMode(QFileDialog::ExistingFile);
  QStringList filters;
  filters << "txt" << "dat";
  m_file->setFilters(QStringList(filters));
  m_file->setMinimumWidth(500);

  setupLayout();
  setupConnections();
}

//-----------------------------------------------------------------------------
// LipSyncPopup::setupLayout() - Helper method for layout setup
//-----------------------------------------------------------------------------

void LipSyncPopup::setupLayout() {
  m_topLayout->setContentsMargins(0, 0, 0, 0);
  m_topLayout->setSpacing(0);

  QGridLayout *phonemeLay = new QGridLayout();
  phonemeLay->setContentsMargins(10, 10, 10, 10);
  phonemeLay->setVerticalSpacing(10);
  phonemeLay->setHorizontalSpacing(10);

  int i = 0;  // navButtons index
  int j = 0;  // imageLabels index
  int k = 0;  // textLabels index

  // First row: Phoneme labels
  phonemeLay->addWidget(m_aiLabel, 0, 0, 1, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_eLabel, 0, 2, 1, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_oLabel, 0, 4, 1, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_uLabel, 0, 6, 1, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_fvLabel, 0, 8, 1, 2, Qt::AlignCenter);

  // Second row: Images
  phonemeLay->addWidget(m_imageLabels[j++], 1, 0, 1, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_imageLabels[j++], 1, 2, 1, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_imageLabels[j++], 1, 4, 1, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_imageLabels[j++], 1, 6, 1, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_imageLabels[j++], 1, 8, 1, 2, Qt::AlignCenter);

  // Third row: Text labels
  phonemeLay->addWidget(m_textLabels[k++], 2, 0, 1, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_textLabels[k++], 2, 2, 1, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_textLabels[k++], 2, 4, 1, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_textLabels[k++], 2, 6, 1, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_textLabels[k++], 2, 8, 1, 2, Qt::AlignCenter);

  // Fourth row: Navigation buttons
  phonemeLay->addWidget(m_navButtons[i++], 3, 0, Qt::AlignCenter);
  phonemeLay->addWidget(m_navButtons[i++], 3, 1, Qt::AlignCenter);
  phonemeLay->addWidget(m_navButtons[i++], 3, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_navButtons[i++], 3, 3, Qt::AlignCenter);
  phonemeLay->addWidget(m_navButtons[i++], 3, 4, Qt::AlignCenter);
  phonemeLay->addWidget(m_navButtons[i++], 3, 5, Qt::AlignCenter);
  phonemeLay->addWidget(m_navButtons[i++], 3, 6, Qt::AlignCenter);
  phonemeLay->addWidget(m_navButtons[i++], 3, 7, Qt::AlignCenter);
  phonemeLay->addWidget(m_navButtons[i++], 3, 8, Qt::AlignCenter);
  phonemeLay->addWidget(m_navButtons[i++], 3, 9, Qt::AlignCenter);

  // Empty row
  phonemeLay->addWidget(new QLabel("", this), 4, Qt::AlignCenter);

  // Fifth row: Phoneme labels
  phonemeLay->addWidget(m_lLabel, 5, 0, 1, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_mbpLabel, 5, 2, 1, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_wqLabel, 5, 4, 1, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_otherLabel, 5, 6, 1, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_restLabel, 5, 8, 1, 2, Qt::AlignCenter);

  // Sixth row: Images
  phonemeLay->addWidget(m_imageLabels[j++], 6, 0, 1, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_imageLabels[j++], 6, 2, 1, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_imageLabels[j++], 6, 4, 1, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_imageLabels[j++], 6, 6, 1, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_imageLabels[j++], 6, 8, 1, 2, Qt::AlignCenter);

  // Seventh row: Text labels
  phonemeLay->addWidget(m_textLabels[k++], 7, 0, 1, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_textLabels[k++], 7, 2, 1, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_textLabels[k++], 7, 4, 1, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_textLabels[k++], 7, 6, 1, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_textLabels[k++], 7, 8, 1, 2, Qt::AlignCenter);

  // Eighth row: Navigation buttons
  phonemeLay->addWidget(m_navButtons[i++], 8, 0, Qt::AlignCenter);
  phonemeLay->addWidget(m_navButtons[i++], 8, 1, Qt::AlignCenter);
  phonemeLay->addWidget(m_navButtons[i++], 8, 2, Qt::AlignCenter);
  phonemeLay->addWidget(m_navButtons[i++], 8, 3, Qt::AlignCenter);
  phonemeLay->addWidget(m_navButtons[i++], 8, 4, Qt::AlignCenter);
  phonemeLay->addWidget(m_navButtons[i++], 8, 5, Qt::AlignCenter);
  phonemeLay->addWidget(m_navButtons[i++], 8, 6, Qt::AlignCenter);
  phonemeLay->addWidget(m_navButtons[i++], 8, 7, Qt::AlignCenter);
  phonemeLay->addWidget(m_navButtons[i++], 8, 8, Qt::AlignCenter);
  phonemeLay->addWidget(m_navButtons[i++], 8, 9, Qt::AlignCenter);

  // Settings row
  phonemeLay->addWidget(new QLabel(tr("Insert at Frame: "), this), 10, 0, 1, 1,
                        Qt::AlignRight);
  phonemeLay->addWidget(m_startAt, 10, 1, 1, 1, Qt::AlignLeft);
  phonemeLay->addWidget(m_restToEnd, 10, 2, 1, 6, Qt::AlignLeft);

  m_topLayout->addLayout(phonemeLay, 0);

  // Bottom layout
  m_buttonLayout->setContentsMargins(0, 0, 0, 0);
  m_buttonLayout->setSpacing(10);

  QHBoxLayout *fileLay = new QHBoxLayout();
  fileLay->addWidget(new QLabel(tr("Lip Sync Data File: "), this),
                     Qt::AlignLeft);
  fileLay->addWidget(m_file);

  m_buttonLayout->addLayout(fileLay);
  m_buttonLayout->addStretch();
  m_buttonLayout->addWidget(m_applyButton);
}

//-----------------------------------------------------------------------------
// LipSyncPopup::setupConnections() - Helper method for signal connections
//-----------------------------------------------------------------------------

void LipSyncPopup::setupConnections() {
  // Modern connections for QPushButton navigation buttons
  for (int i = 0; i < 20; ++i) {
    connect(m_navButtons[i], &QPushButton::clicked, this,
            [this, index = i]() { imageNavClicked(index); });
  }

  // Modern connection for Apply button
  connect(m_applyButton, &QPushButton::clicked, this,
          &LipSyncPopup::onApplyButton);
  connect(m_file, &DVGui::FileField::pathChanged, this,
          &LipSyncPopup::onPathChanged);
  connect(m_startAt, &DVGui::IntLineEdit::editingFinished, this,
          &LipSyncPopup::onStartValueChanged);
}

//-----------------------------------------------------------------------------
// LipSyncPopup::showEvent()
//-----------------------------------------------------------------------------

void LipSyncPopup::showEvent(QShowEvent *event) {
  Dialog::showEvent(event);

  // Reset state
  m_activeFrameIds.clear();
  m_levelFrameIds.clear();
  m_sl         = nullptr;
  m_cl         = nullptr;
  m_childLevel = TXshLevelP();
  m_startAt->setValue(1);
  m_valid = false;
  m_applyButton->setEnabled(false);
  m_textLines.clear();

  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;

  TXsheet *xsh     = scene->getXsheet();
  m_col            = app->getCurrentColumn()->getColumnIndex();
  int row          = app->getCurrentFrame()->getFrame();
  m_isEditingLevel = app->getCurrentFrame()->isEditingLevel();
  m_startAt->setValue(row + 1);
  m_startAt->clearFocus();

  TXshLevelHandle *levelHandle = app->getCurrentLevel();
  m_sl                         = levelHandle->getSimpleLevel();

  if (!m_sl) {
    TXshCell cell = xsh->getCell(row, m_col);
    if (!cell.isEmpty()) {
      m_cl         = cell.m_level->getChildLevel();
      m_childLevel = cell.m_level;
    }
  }

  if (m_cl) {
    DVGui::warning(
        tr("Thumbnails are not available for sub-Xsheets.\n"
           "Please use the frame numbers for reference."));
  }

  if (!m_sl && !m_cl) {
    DVGui::warning(tr("Unable to apply lip sync data to this column type"));
    return;
  }

  // Get frame IDs
  levelHandle->getLevel()->getFids(m_levelFrameIds);
  if (!m_levelFrameIds.empty()) {
    const size_t copyCount =
        std::min(m_levelFrameIds.size(), static_cast<size_t>(10));
    m_activeFrameIds.resize(10, m_levelFrameIds.front());
    std::copy_n(m_levelFrameIds.begin(), copyCount, m_activeFrameIds.begin());
  }
  // Connection and preload
  if (m_sl && !m_iconGeneratorConnected) {
    connect(IconGenerator::instance(), &IconGenerator::iconGenerated, this,
            &LipSyncPopup::onIconGenerated, Qt::QueuedConnection);
    m_iconGeneratorConnected = true;
  }

  preloadThumbnails();  // <<< Generates thumbnails on open
}

//-----------------------------------------------------------------------------
// LipSyncPopup::onApplyButton()
//-----------------------------------------------------------------------------

void LipSyncPopup::onApplyButton() {
  if (!m_valid || (!m_sl && !m_cl)) {
    hide();
    return;
  }

  const int startFrame = m_startAt->getValue() - 1;
  TApp *app            = TApp::instance();
  TXsheet *xsh         = app->getCurrentScene()->getScene()->getXsheet();

  int lastFrame = m_textLines.at(m_textLines.size() - 2).toInt() + startFrame;

  if (m_restToEnd->isChecked()) {
    int r0, r1;
    xsh->getCellRange(m_col, r0, r1);
    if (lastFrame < r1 + 1) lastFrame = r1 + 1;
  }

  // Store previous state for undo
  std::vector<TFrameId> previousFrameIds;
  std::vector<TXshLevelP> previousLevels;
  previousFrameIds.reserve(lastFrame - startFrame);
  previousLevels.reserve(lastFrame - startFrame);

  for (int frame = startFrame; frame < lastFrame; ++frame) {
    TXshCell cell = xsh->getCell(frame, m_col);
    previousFrameIds.push_back(cell.m_frameId);
    previousLevels.push_back(cell.m_level);
  }

  LipSyncUndo *undo =
      new LipSyncUndo(m_col, m_sl, m_childLevel, m_activeFrameIds, m_textLines,
                      lastFrame, previousFrameIds, previousLevels, startFrame);
  TUndoManager::manager()->add(undo);
  undo->redo();
  hide();
}

//-----------------------------------------------------------------------------
// LipSyncPopup::imageNavClicked()
//-----------------------------------------------------------------------------

void LipSyncPopup::imageNavClicked(int id) {
  if (!m_sl && !m_cl || m_levelFrameIds.empty()) return;

  const int direction   = (id % 2) ? 1 : -1;
  const int frameNumber = id / 2;

  auto currentIt = std::find(m_levelFrameIds.begin(), m_levelFrameIds.end(),
                             m_activeFrameIds[frameNumber]);

  if (currentIt == m_levelFrameIds.end()) return;

  int currentIndex = std::distance(m_levelFrameIds.begin(), currentIt);
  int newIndex     = 0;

  if (direction == 1) {
    newIndex = (currentIndex == static_cast<int>(m_levelFrameIds.size()) - 1)
                   ? 0
                   : currentIndex + 1;
  } else {
    newIndex = (currentIndex == 0)
                   ? static_cast<int>(m_levelFrameIds.size()) - 1
                   : currentIndex - 1;
  }

  m_activeFrameIds[frameNumber] = m_levelFrameIds[newIndex];

  // Update only the changed thumbnail
  updateThumbnail(frameNumber);

  update();
}

//-----------------------------------------------------------------------------
// LipSyncPopup::onIconGenerated()
//-----------------------------------------------------------------------------

void LipSyncPopup::onIconGenerated() {
  if (!isVisible()) return;

  for (int i = 0; i < 10; i++) {
    updateThumbnail(i);
  }
  update();
}

//-----------------------------------------------------------------------------
// LipSyncPopup::updateThumbnail()
//-----------------------------------------------------------------------------

void LipSyncPopup::updateThumbnail(int index) {
  if (index < 0 || index >= 10) return;

  if (m_sl || m_cl) {
    QPixmap pm;

    if (m_sl) {
      pm = IconGenerator::instance()->getSizedIcon(
          m_sl, m_activeFrameIds[index], "", TDimension(160, 90));
    } else if (m_cl) {
      auto it        = std::find(m_levelFrameIds.begin(), m_levelFrameIds.end(),
                                 m_activeFrameIds[index]);
      int frameIndex = (it != m_levelFrameIds.end())
                           ? std::distance(m_levelFrameIds.begin(), it)
                           : 0;

      QImage img(160, 90, QImage::Format_ARGB32);
      img.fill(Qt::gray);
      QPainter p(&img);
      p.setPen(Qt::black);
      p.drawText(img.rect(),
                 tr("SubXSheet Frame ") + QString::number(frameIndex + 1),
                 QTextOption(Qt::AlignCenter));
      pm = QPixmap::fromImage(img);
    }

    if (!pm.isNull()) {
      m_pixmaps[index] = pm;
      m_imageLabels[index]->setPixmap(
          pm.scaled(160, 90, Qt::KeepAspectRatio, Qt::FastTransformation));
      m_textLabels[index]->setText(
          tr("Drawing: ") +
          QString::number(m_activeFrameIds[index].getNumber()));
    } else {
      // Simple placeholder shown while loading
      m_textLabels[index]->clear();
    }
  }
}

//-----------------------------------------------------------------------------
// LipSyncPopup::preloadThumbnails()
//-----------------------------------------------------------------------------

void LipSyncPopup::preloadThumbnails() {
  if (!m_sl && !m_cl) {
    for (int i = 0; i < 10; i++) {
      QImage placeHolder(160, 90, QImage::Format_ARGB32);
      placeHolder.fill(Qt::gray);
      m_pixmaps[i] = QPixmap::fromImage(placeHolder);
      m_imageLabels[i]->setPixmap(m_pixmaps[i]);
      m_textLabels[i]->setText("");
    }
    update();
    return;
  }

  if (m_sl) {
    for (int i = 0; i < 10 && i < static_cast<int>(m_activeFrameIds.size());
         i++) {
      IconGenerator::instance()->getSizedIcon(
          m_sl, m_activeFrameIds[i], "",
          TDimension(160, 90));  // <<< suffix vazio
      updateThumbnail(i);
    }
  }
}

//-----------------------------------------------------------------------------
// LipSyncPopup::paintEvent()
//-----------------------------------------------------------------------------

void LipSyncPopup::paintEvent(QPaintEvent *event) {
  Dialog::paintEvent(event);
}

//-----------------------------------------------------------------------------
// LipSyncPopup::onPathChanged()
//-----------------------------------------------------------------------------

void LipSyncPopup::onPathChanged() {
  m_textLines.clear();
  QString path = m_file->getPath();

  if (path.isEmpty()) {
    m_valid = false;
    m_applyButton->setEnabled(false);
    return;
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    DVGui::warning(tr("Unable to open the file:\n%1").arg(file.errorString()));
    m_valid = false;
    m_applyButton->setEnabled(false);
    return;
  }

  QTextStream in(&file);

  while (!in.atEnd()) {
    QString line = in.readLine().trimmed();
    if (line.isEmpty()) continue;
    QStringList entries = line.split(' ', Qt::SkipEmptyParts);
    if (entries.size() != 2) continue;

    bool ok;
    int frameNumber = entries[0].toInt(&ok);
    if (!ok) continue;

    entries[1].toInt(&ok);
    if (ok) continue;

    m_textLines << entries;
  }

  file.close();

  m_valid = (m_textLines.size() > 1);
  m_applyButton->setEnabled(m_valid);

  if (!m_valid) {
    DVGui::warning(tr("Invalid data file or no valid entries found."));
  }
}

//-----------------------------------------------------------------------------
// LipSyncPopup::onStartValueChanged()
//-----------------------------------------------------------------------------

void LipSyncPopup::onStartValueChanged() {
  int value = m_startAt->getValue();
  if (value < 1) m_startAt->setValue(1);
}

//-----------------------------------------------------------------------------
// LipSyncPopup::hideEvent()
//-----------------------------------------------------------------------------
void LipSyncPopup::hideEvent(QHideEvent *event) {
  if (m_iconGeneratorConnected && IconGenerator::instance()) {
    disconnect(IconGenerator::instance(), &IconGenerator::iconGenerated, this,
               &LipSyncPopup::onIconGenerated);
    m_iconGeneratorConnected = false;
  }

  Dialog::hideEvent(event);
}

//-----------------------------------------------------------------------------
// LipSyncPopup::~LipSyncPopup()
//-----------------------------------------------------------------------------
LipSyncPopup::~LipSyncPopup() {
  if (m_iconGeneratorConnected && IconGenerator::instance()) {
    disconnect(IconGenerator::instance(), &IconGenerator::iconGenerated, this,
               &LipSyncPopup::onIconGenerated);
  }
}

//-----------------------------------------------------------------------------
// Command registration
//-----------------------------------------------------------------------------

OpenPopupCommandHandler<LipSyncPopup> openLipSyncPopup(MI_LipSyncPopup);
