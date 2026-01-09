#include "mousezoom.h"
#include <QApplication>
#include <QMenu>
#include <QAction>
#include <cmath>

MouseZoom::MouseZoom(QWidget *parent)
    : QCustomPlot(parent)
{
    setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectItems);
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QCustomPlot::customContextMenuRequested, this, &MouseZoom::onCustomContextMenuRequested);
}

MouseZoom::~MouseZoom()
{
}

void MouseZoom::wheelEvent(QWheelEvent *event)
{
    Qt::MouseButtons buttons = QApplication::mouseButtons();
    Qt::Orientations orient = Qt::Horizontal | Qt::Vertical;

    if (buttons & Qt::LeftButton) {
        orient = Qt::Horizontal;
    } else if (buttons & Qt::RightButton) {
        orient = Qt::Vertical;
    }

    for(int i=0; i<axisRectCount(); ++i) {
        axisRect(i)->setRangeZoom(orient);
    }

    QCustomPlot::wheelEvent(event);

    for(int i=0; i<axisRectCount(); ++i) {
        axisRect(i)->setRangeZoom(Qt::Horizontal | Qt::Vertical);
    }
}

double MouseZoom::distToSegment(const QPointF& p, const QPointF& s, const QPointF& e)
{
    double l2 = (s.x()-e.x())*(s.x()-e.x()) + (s.y()-e.y())*(s.y()-e.y());
    if (l2 == 0) return std::sqrt((p.x()-s.x())*(p.x()-s.x()) + (p.y()-s.y())*(p.y()-s.y()));
    double t = ((p.x()-s.x())*(e.x()-s.x()) + (p.y()-s.y())*(e.y()-s.y())) / l2;
    t = std::max(0.0, std::min(1.0, t));
    QPointF proj = s + t * (e - s);
    return std::sqrt((p.x()-proj.x())*(p.x()-proj.x()) + (p.y()-proj.y())*(p.y()-proj.y()));
}

void MouseZoom::onCustomContextMenuRequested(const QPoint &pos)
{
    QMenu menu(this);

    QCPAbstractItem* hitItem = nullptr;
    QCPItemLine* hitLine = nullptr;
    QCPItemText* hitText = nullptr;
    double tolerance = 8.0;
    QPointF pMouse = pos;

    // Check lines
    for (int i = 0; i < itemCount(); ++i) {
        if (auto line = qobject_cast<QCPItemLine*>(item(i))) {
            if (line->property("isCharacteristic").isValid()) {
                double x1 = xAxis->coordToPixel(line->start->coords().x());
                double y1 = yAxis->coordToPixel(line->start->coords().y());
                double x2 = xAxis->coordToPixel(line->end->coords().x());
                double y2 = yAxis->coordToPixel(line->end->coords().y());
                if (distToSegment(pMouse, QPointF(x1, y1), QPointF(x2, y2)) < tolerance) {
                    hitLine = line;
                    hitItem = line;
                    break;
                }
            }
        }
    }
    // Check text
    if (!hitItem) {
        for (int i = 0; i < itemCount(); ++i) {
            if (auto text = qobject_cast<QCPItemText*>(item(i))) {
                if (text->selectTest(pMouse, false) < tolerance) {
                    hitText = text;
                    hitItem = text;
                    break;
                }
            }
        }
    }

    if (hitLine) {
        deselectAll();
        hitLine->setSelected(true);
        replot();
        QAction* actNote = menu.addAction("添加/修改 标注");
        connect(actNote, &QAction::triggered, this, [=](){ emit addAnnotationRequested(hitLine); });
        menu.addSeparator();
        QAction* actDel = menu.addAction("删除线段");
        connect(actDel, &QAction::triggered, this, &MouseZoom::deleteSelectedRequested);
    }
    else if (hitText) {
        deselectAll();
        hitText->setSelected(true);
        replot();
        QAction* actEdit = menu.addAction("修改标注文字");
        connect(actEdit, &QAction::triggered, this, [=](){ emit editItemRequested(hitText); });
        menu.addSeparator();
        QAction* actDel = menu.addAction("删除标注");
        connect(actDel, &QAction::triggered, this, &MouseZoom::deleteSelectedRequested);
    }
    else {
        QAction* actSave = menu.addAction(QIcon(), "导出图片");
        connect(actSave, &QAction::triggered, this, &MouseZoom::saveImageRequested);
        QAction* actExport = menu.addAction(QIcon(), "导出数据");
        connect(actExport, &QAction::triggered, this, &MouseZoom::exportDataRequested);
        QMenu* subMenuLine = menu.addMenu("标识线绘制");
        QAction* actK1 = subMenuLine->addAction("斜率 k=1");
        QAction* actK05 = subMenuLine->addAction("斜率 k=1/2");
        QAction* actK025 = subMenuLine->addAction("斜率 k=1/4");
        QAction* actK0 = subMenuLine->addAction("水平线");
        connect(actK1, &QAction::triggered, this, [=](){ emit drawLineRequested(1.0); });
        connect(actK05, &QAction::triggered, this, [=](){ emit drawLineRequested(0.5); });
        connect(actK025, &QAction::triggered, this, [=](){ emit drawLineRequested(0.25); });
        connect(actK0, &QAction::triggered, this, [=](){ emit drawLineRequested(0.0); });
        QAction* actSetting = menu.addAction(QIcon(), "图表设置");
        connect(actSetting, &QAction::triggered, this, &MouseZoom::settingsRequested);
        menu.addSeparator();
        QAction* actReset = menu.addAction(QIcon(), "重置视图");
        connect(actReset, &QAction::triggered, this, &MouseZoom::resetViewRequested);
    }

    menu.exec(mapToGlobal(pos));
}
