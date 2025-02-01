

#include "ruler.h"
#include "sceneviewer.h"
#include "tapp.h"
#include "toonz/tscenehandle.h"
#include "toonzqt/gutil.h"

#include "toonz/toonzscene.h"
#include "toonz/stage.h"
#include "tunit.h"

#include <QPainter>
#include <QMouseEvent>

//=============================================================================
// Ruler
//-----------------------------------------------------------------------------

Ruler::Ruler(QWidget *parent, SceneViewer *viewer, bool vertical)
    : QWidget(parent)
    , m_viewer(viewer)
    , m_vertical(vertical)
    , m_moving(false)
    , m_hiding(false) {
  if (vertical) {
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setFixedWidth(12);
    setToolTip(tr("Click to create an horizontal guide"));
  } else {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(12);
    setToolTip(tr("Click to create a vertical guide"));
  }
  setMouseTracking(true);
}

//-----------------------------------------------------------------------------

Ruler::Guides &Ruler::getGuides() const {
  TSceneProperties *sprop =
      TApp::instance()->getCurrentScene()->getScene()->getProperties();
  return m_vertical ? sprop->getVGuides() : sprop->getHGuides();
}

//-----------------------------------------------------------------------------

int Ruler::getGuideCount() const { return getGuides().size(); }

//-----------------------------------------------------------------------------

double Ruler::getGuide(int index) const {
  Guides &guides = getGuides();
  assert(0 <= index && index < (int)guides.size());
  return guides[index];
}

//-----------------------------------------------------------------------------

double Ruler::getUnit() const {
  double unit = getZoomScale() * Stage::inch;
  return unit;
}

//-----------------------------------------------------------------------------

void Ruler::getIndices(double origin, double iunit, int size, int &i0, int &i1,
                       int &ic) const {
  i0 = 0;
  i1 = -1;
  ic = 0;
  if (origin >= 0) {
    i0    = -tfloor(origin * iunit);
    i1    = i0 + tfloor(size * iunit);
    int d = tceil(-i0, 10);
    i0 += d;
    i1 += d;
    ic += d;
  } else {
    i0 = tceil(-origin * iunit);
    i1 = i0 + tfloor(size * iunit);
    ic = 0;
  }
  if (m_viewer->getIsFlippedX()) {
    i0 = i0 + i1;
    i1 = i0 - i1;
    i0 = i0 - i1;
  }
}

//-----------------------------------------------------------------------------

double Ruler::getZoomScale() const {
  if (m_viewer->is3DView())
    return m_viewer->getZoomScale3D();
  else
    return m_viewer->getViewMatrix().a11;
}

//-----------------------------------------------------------------------------

double Ruler::getPan() const {
  TAffine aff = m_viewer->getViewMatrix();
  if (m_vertical)
    if (m_viewer->is3DView())  // Vertical   3D
      return m_viewer->getPan3D().y;
    else  // Vertical   2D
      return aff.a23 / m_viewer->getDevPixRatio();
  else if (m_viewer->is3DView())  // Horizontal 3D
    return m_viewer->getPan3D().x;
  return aff.a13 / m_viewer->getDevPixRatio();  // Horizontal 2D
}

//-----------------------------------------------------------------------------

// Helper function to find the closest guide
int Ruler::findClosestGuide(double value, double &minDist2) const {
  Guides &guides = getGuides();
  int selected = -1;
  minDist2 = 0;
  for (int i = 0; i < guides.size(); i++) {
    double g = guides[i] / (double)m_viewer->getDevPixRatio();
    double dist2 = (g - value) * (g - value);
    if (selected < 0 || dist2 < minDist2) {
      minDist2 = dist2;
      selected = i;
    }
  }
  return selected;
}

// Helper function to draw the guides
void Ruler::drawGuides(QPainter &p, int x0, int y0, int x1, int y1, bool vertical) {
  Guides &guides = getGuides();
  int count = guides.size();
  if (m_hiding) count--;
  double origin = vertical ? -getPan() + 0.5 * y1 : getPan() + 0.5 * x1;
  double zoom = getZoomScale();

  if (m_viewer->getIsFlippedX() != m_viewer->getIsFlippedY()) {
    zoom = -zoom;
  }

  for (int i = 0; i < count; i++) {
    QColor color = (m_moving && count - 1 == i ? QColor(getHandleDragColor())
                                               : QColor(getHandleColor()));
    double v = guides[i] / (double)m_viewer->getDevPixRatio();
    int pos = (vertical ? (int)(origin - zoom * v) : (int)(origin + zoom * v));
    p.fillRect(QRect(vertical ? x0 : pos - 1, vertical ? pos - 1 : y0,
                     vertical ? x1 - x0 : 2, vertical ? 2 : y1 - y0), QBrush(color));
  }
}

// Helper function to draw scale markers
void Ruler::drawScale(QPainter &p, int x0, int y0, int x1, int y1, bool vertical) {
  double unit = getUnit() / UnitParameters::getFieldGuideAspectRatio();
  QColor color = m_scaleColor;
  int i0, i1, ic;
  getIndices(vertical ? (-getPan() + 0.5 * y1) : (getPan() + 0.5 * x1), 1 / unit,
             vertical ? height() : width(), i0, i1, ic);

  for (int i = i0; i <= i1; i++) {
    int pos = (vertical ? (int)(y0 + unit * (i - ic)) : (int)(x0 + unit * (i - ic)));
    int xa = vertical ? x1 - 1 : pos;
    int xb = vertical ? x1 - 1 : pos;
    int ya = vertical ? pos : y1 - 1;
    int yb = vertical ? pos : y1 - 1;

    if (i == ic) {
      p.setPen(m_scaleColor);
      p.drawLine(xa, ya, xb, yb);
    } else {
      p.setPen(color);
      if (i % 10 == 0)
        p.drawLine(xa - 5, ya, xb - 5, yb);  // Major tick
      else if (i % 5 == 0)
        p.drawLine(xa - 3, ya, xb - 3, yb);  // Medium tick
      else
        p.drawLine(xa - 2, ya, xb - 2, yb);  // Minor tick
    }
  }
}

void Ruler::drawVertical(QPainter &p) {
  int w = width();
  int h = height();
  drawGuides(p, 0, 0, w - 1, h - 1, true);  // Draw guides for vertical ruler
  drawScale(p, 0, 0, w - 1, h - 1, true);   // Draw scale for vertical ruler
}

void Ruler::drawHorizontal(QPainter &p) {
  int w = width();
  int h = height();
  drawGuides(p, 0, 0, w - 1, h - 1, false);  // Draw guides for horizontal ruler
  drawScale(p, 0, 0, w - 1, h - 1, false);   // Draw scale for horizontal ruler
}

void Ruler::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.fillRect(QRect(0, 0, width(), height()), QBrush(m_parentBgColor));

  if (m_vertical)
    drawVertical(p);
  else
    drawHorizontal(p);
}

double Ruler::posToValue(const QPoint &p) const {
  double v;
  if (m_vertical)
    if (m_viewer->getIsFlippedX() != m_viewer->getIsFlippedY()) {
      v = (-p.y() + height() / 2 - getPan()) / -getZoomScale();
    } else
      v = (-p.y() + height() / 2 - getPan()) / getZoomScale();
  else
    v = (p.x() - width() / 2 - getPan()) / getZoomScale();
  return v;
}

void Ruler::mousePressEvent(QMouseEvent *e) {
  if (e->button() != Qt::LeftButton && e->button() != Qt::RightButton) return;
  Guides &guides = getGuides();
  double v = posToValue(e->pos());
  m_hiding = false;
  m_moving = false;
  double minDist2;
  int selected = findClosestGuide(v, minDist2);
  if (e->button() == Qt::LeftButton) {
    if (selected < 0 || minDist2 > 25) {
      // Create new guide
      guides.push_back(v * m_viewer->getDevPixRatio());
      m_viewer->update();
    } else if (selected < guides.size() - 1)
      std::swap(guides[selected], guides.back());

    m_moving = true;
    update();
    assert(guides.size() > 0);
  } else if (e->button() == Qt::RightButton) {
    // Handle right-click to delete
    if (selected >= 0) {
      guides.erase(guides.begin() + selected);
      m_viewer->update();
      update();
    }
  }
}

void Ruler::mouseMoveEvent(QMouseEvent *e) {
  if (m_moving) {
    Guides &guides = getGuides();
    double v = posToValue(e->pos());
    guides.back() = v * m_viewer->getDevPixRatio();
    update();
  }
}

void Ruler::mouseReleaseEvent(QMouseEvent *e) {
  if (e->button() != Qt::LeftButton) return;
  m_moving = false;
}

