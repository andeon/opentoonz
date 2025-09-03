#include "toolbar.h"
#include "tapp.h"
#include "pane.h"
#include "floatingpanelcommand.h"
#include "tools/toolhandle.h"
#include "tools/tool.h"
#include "tools/toolcommandids.h"
#include "toonzqt/menubarcommand.h"
#include "menubarcommandids.h"

#include "toonz/txshleveltypes.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/preferences.h"
#include "toonz/tscenehandle.h"

// TnzBase includes
#include "tenv.h"

#include <QPainter>
#include <QAction>
#include <QToolButton>
#include <QVBoxLayout>
#include <QObject>
#include <QStringLiteral>

TEnv::IntVar ShowAllToolsToggle("ShowAllToolsToggle", 1);

namespace {

// Struct for button layout with QString and default nullptr for QAction*
struct ToolButton {
  QString toolName;
  bool collapsible;
  QAction *action = nullptr;
};

ToolButton buttonLayout[] = {
    {QStringLiteral(T_Edit), false},
    {QStringLiteral(T_Selection), false},
    {QStringLiteral("Separator_1"), false},
    {QStringLiteral(T_Brush), false},
    {QStringLiteral(T_Geometric), false},
    {QStringLiteral(T_Type), true},
    {QStringLiteral(T_Fill), false},
    {QStringLiteral(T_PaintBrush), false},
    {QStringLiteral("Separator_2"), false},
    {QStringLiteral(T_Eraser), false},
    {QStringLiteral(T_Tape), false},
    {QStringLiteral(T_Finger), false},
    {QStringLiteral("Separator_3"), false},
    {QStringLiteral(T_StylePicker), false},
    {QStringLiteral(T_RGBPicker), false},
    {QStringLiteral(T_Ruler), false},
    {QStringLiteral(T_EditAssistants), false},
    {QStringLiteral("Separator_4"), false},
    {QStringLiteral(T_ControlPointEditor), false},
    {QStringLiteral(T_Pinch), true},
    {QStringLiteral(T_Pump), true},
    {QStringLiteral(T_Magnet), true},
    {QStringLiteral(T_Bender), true},
    {QStringLiteral(T_Iron), true},
    {QStringLiteral(T_Cutter), true},
    {QStringLiteral("Separator_5"), true},
    {QStringLiteral(T_Skeleton), true},
    {QStringLiteral(T_Tracker), true},
    {QStringLiteral(T_Hook), true},
    {QStringLiteral(T_Plastic), true},
    {QStringLiteral("Separator_6"), false},
    {QStringLiteral(T_Zoom), false},
    {QStringLiteral(T_Rotate), true},
    {QStringLiteral(T_Hand), false},
    {QString()},  // marks end
};

}  // namespace

//=============================================================================
// Toolbar
//-----------------------------------------------------------------------------

Toolbar::Toolbar(QWidget *parent, bool isVertical)
    : QToolBar(parent), m_isExpanded(ShowAllToolsToggle != 0) {
  setObjectName(QStringLiteral("toolBar"));
  setMovable(false);

  if (isVertical)
    setOrientation(Qt::Vertical);
  else
    setOrientation(Qt::Horizontal);

  setIconSize(QSize(20, 20));
  setToolButtonStyle(Qt::ToolButtonIconOnly);

  m_expandButton = new QToolButton(this);
  m_expandButton->setObjectName(QStringLiteral("expandButton"));
  m_expandButton->setCheckable(true);
  m_expandButton->setChecked(m_isExpanded);
  m_expandButton->setArrowType(isVertical ? Qt::DownArrow : Qt::RightArrow);

  m_expandAction = addWidget(m_expandButton);

  connect(m_expandButton, &QToolButton::toggled, this, &Toolbar::setIsExpanded);

  updateToolbar();
}

//-----------------------------------------------------------------------------
/*! Layout the tool buttons according to the state of the expandButton
 */
void Toolbar::updateToolbar() {
  TApp *app = TApp::instance();
  TFrameHandle *frameHandle = app->getCurrentFrame();

  if (frameHandle->isPlaying()) return;

  TXshLevelHandle *currlevel = app->getCurrentLevel();
  TXshLevel *level = currlevel ? currlevel->getLevel() : nullptr;
  int levelType = level ? level->getType() : NO_XSHLEVEL;

  TColumnHandle *colHandle = app->getCurrentColumn();
  int colIndex = colHandle->getColumnIndex();

  int rowIndex = frameHandle->getFrameIndex();

  if (Preferences::instance()->isAutoCreateEnabled() &&
      Preferences::instance()->isAnimationSheetEnabled()) {
    if (levelType == NO_XSHLEVEL) {
      TXsheetHandle *xshHandle = app->getCurrentXsheet();
      TXsheet *xsh = xshHandle->getXsheet();

      if (colIndex >= 0 && !xsh->isColumnEmpty(colIndex)) {
        int r0, r1;
        xsh->getCellRange(colIndex, r0, r1);
        if (0 <= r0 && r0 <= r1) {
          for (int r = std::min(r1, rowIndex); r >= r0; r--) {
            TXshCell cell = xsh->getCell(r, colIndex);
            if (cell.isEmpty()) continue;
            levelType = cell.m_level->getType();
            rowIndex = r;
            break;
          }
          if (levelType == NO_XSHLEVEL) {
            TXshCell cell = xsh->getCell(r0, colIndex);
            levelType = cell.m_level->getType();
            rowIndex = r0;
          }
        }
      }
    }
  }

  m_toolbarLevel = levelType;

  TTool::ToolTargetType targetType = TTool::NoTarget;

  switch (m_toolbarLevel) {
    case OVL_XSHLEVEL:
      targetType = TTool::RasterImage;
      break;
    case TZP_XSHLEVEL:
      targetType = TTool::ToonzImage;
      break;
    case PLI_XSHLEVEL:
      [[fallthrough]];
    default:
      targetType = TTool::VectorImage;
      break;
    case MESH_XSHLEVEL:
      targetType = TTool::MeshImage;
      break;
  }

  // Remove existing actions
  for (auto &btn : buttonLayout) {
    if (!btn.toolName.isEmpty() && btn.action) removeAction(btn.action);
  }
  removeAction(m_expandAction);

  int levelBasedDisplay = Preferences::instance()->getLevelBasedToolsDisplay();

  bool actionEnabled = false;

  for (auto &btn : buttonLayout) {
    if (btn.toolName.isEmpty()) break;  // End of list

    TTool *tool = TTool::getTool(btn.toolName.toUtf8().constData(), targetType);
    if (tool) tool->updateEnabled(rowIndex, colIndex);

    bool isSeparator = btn.toolName.startsWith(QStringLiteral("Separator"));
    
    bool enable = false;
    if (!levelBasedDisplay) {
      enable = true;
    } else if (!tool) {
      enable = actionEnabled;
    } else {
      enable = tool->isEnabled();
    }

    // Plastic tool always enabled to allow mesh creation
    if (!enable && btn.toolName == QStringLiteral(T_Plastic) &&
        (m_toolbarLevel & LEVELCOLUMN_XSHLEVEL)) {
      enable = true;
    }

    if (!m_isExpanded && btn.collapsible) continue;

    if (!btn.action) {
      if (isSeparator) {
        btn.action = addSeparator();
      } else {
        btn.action = CommandManager::instance()->getAction(btn.toolName);
      }
    }

    if (levelBasedDisplay != 2)
      btn.action->setEnabled(enable);
    else if (!enable)
      continue;

    actionEnabled = addAction(btn.action) || actionEnabled;

    if (isSeparator) actionEnabled = false;
  }

  addAction(m_expandAction);

  if (m_isExpanded) {
    m_expandButton->setArrowType(orientation() == Qt::Vertical ? Qt::UpArrow : Qt::LeftArrow);
    m_expandButton->setToolTip(tr("Collapse toolbar"));
  } else {
    m_expandButton->setArrowType(orientation() == Qt::Vertical ? Qt::DownArrow : Qt::RightArrow);
    m_expandButton->setToolTip(tr("Expand toolbar"));
  }

  setUpdatesEnabled(true);
  update();
}

//-----------------------------------------------------------------------------

void Toolbar::setIsExpanded(bool expand) {
  m_isExpanded = expand;
  ShowAllToolsToggle = expand ? 1 : 0;
  updateToolbar();
}

//-----------------------------------------------------------------------------

Toolbar::~Toolbar() = default;

//-----------------------------------------------------------------------------

bool Toolbar::addAction(QAction *act) {
  if (!act) return false;
  QToolBar::addAction(act);
  return true;
}

//-----------------------------------------------------------------------------

void Toolbar::showEvent(QShowEvent *e) {
  QToolBar::showEvent(e);

  TColumnHandle *columnHandle = TApp::instance()->getCurrentColumn();
  connect(columnHandle, &TColumnHandle::columnIndexSwitched, this, &Toolbar::updateToolbar, Qt::UniqueConnection);

  TFrameHandle *frameHandle = TApp::instance()->getCurrentFrame();
  connect(frameHandle, &TFrameHandle::frameSwitched, this, &Toolbar::updateToolbar, Qt::UniqueConnection);
  connect(frameHandle, &TFrameHandle::frameTypeChanged, this, &Toolbar::updateToolbar, Qt::UniqueConnection);

  TXsheetHandle *xsheetHandle = TApp::instance()->getCurrentXsheet();
  connect(xsheetHandle, &TXsheetHandle::xsheetChanged, this, &Toolbar::updateToolbar, Qt::UniqueConnection);

  connect(TApp::instance()->getCurrentTool(), &ToolHandle::toolSwitched, this, &Toolbar::onToolChanged, Qt::UniqueConnection);

  TXshLevelHandle *levelHandle = TApp::instance()->getCurrentLevel();
  connect(levelHandle, &TXshLevelHandle::xshLevelSwitched, this, &Toolbar::updateToolbar, Qt::UniqueConnection);

  connect(TApp::instance()->getCurrentScene(), &TScene::preferenceChanged, this, &Toolbar::onPreferenceChanged, Qt::UniqueConnection);
}

//-----------------------------------------------------------------------------

void Toolbar::hideEvent(QHideEvent *e) {
  QToolBar::hideEvent(e);

  disconnect(TApp::instance()->getCurrentLevel(), nullptr, this, nullptr);
  disconnect(TApp::instance()->getCurrentTool(), &ToolHandle::toolSwitched, this, &Toolbar::onToolChanged);
  disconnect(TApp::instance()->getCurrentColumn(), &TColumnHandle::columnIndexSwitched, this, &Toolbar::updateToolbar);
  disconnect(TApp::instance()->getCurrentFrame(), &TFrameHandle::frameSwitched, this, &Toolbar::updateToolbar);
  disconnect(TApp::instance()->getCurrentFrame(), &TFrameHandle::frameTypeChanged, this, &Toolbar::updateToolbar);
  disconnect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged, this, &Toolbar::updateToolbar);
  disconnect(TApp::instance()->getCurrentScene(), &TScene::preferenceChanged, this, &Toolbar::onPreferenceChanged);
}

//-----------------------------------------------------------------------------

void Toolbar::onToolChanged() {
  ToolHandle *toolHandle = TApp::instance()->getCurrentTool();
  QString toolName = toolHandle->getRequestedToolName();
  QAction *act = CommandManager::instance()->getAction(toolName);
  if (!act || act->isChecked()) return;
  act->setChecked(true);
}

//-----------------------------------------------------------------------------

void Toolbar::onPreferenceChanged(const QString &prefName) {
  if (prefName == QStringLiteral("ToolbarDisplay") || prefName.isEmpty()) updateToolbar();
}

//=============================================================================

OpenFloatingPanel openToolbarPane(MI_OpenToolbar, "ToolBar", "");
