#ifndef MOUSEZOOM_H
#define MOUSEZOOM_H

#include "qcustomplot.h"

class MouseZoom : public QCustomPlot
{
    Q_OBJECT

public:
    explicit MouseZoom(QWidget *parent = nullptr);
    ~MouseZoom();

signals:
    void saveImageRequested();
    void exportDataRequested();
    void drawLineRequested(double slope);
    void settingsRequested();
    void resetViewRequested();

    void addAnnotationRequested(QCPItemLine* line);
    void deleteSelectedRequested();
    void editItemRequested(QCPAbstractItem* item);

protected:
    void wheelEvent(QWheelEvent *event) override;

private slots:
    void onCustomContextMenuRequested(const QPoint &pos);

private:
    double distToSegment(const QPointF& p, const QPointF& s, const QPointF& e);
};

#endif // MOUSEZOOM_H
