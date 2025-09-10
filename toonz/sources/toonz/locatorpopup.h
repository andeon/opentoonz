#pragma once

#ifndef LOCATORPOPUP_H
#define LOCATORPOPUP_H

#include <QFrame>

class SceneViewer;
class TPointD;
class QShowEvent;
class QHideEvent;

//=============================================================================
// LoactorPopup
//-----------------------------------------------------------------------------

class LocatorPopup : public QFrame {
  Q_OBJECT
  SceneViewer* m_viewer;
  bool m_initialZoom;

public:
  LocatorPopup(QWidget* parent = 0, Qt::WindowFlags flags = Qt::WindowFlags());
  ~LocatorPopup() {}

  SceneViewer* viewer() { return m_viewer; }
  void onChangeViewAff(const TPointD& curPos);

protected:
  void showEvent(QShowEvent *event) override;
  void hideEvent(QHideEvent *event) override;

protected slots:
  void changeWindowTitle();
};

#endif