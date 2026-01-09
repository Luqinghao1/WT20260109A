/*
 * 文件名: wt_plottingwidget.h
 * 文件作用: 图表分析主界面头文件
 * 功能描述:
 * 1. 管理试井分析曲线的创建、显示、修改和删除。
 * 2. 响应左侧列表的双击事件，控制右侧 ChartWidget 的显示内容。
 * 3. 管理数据导出（全部/部分）及默认路径。
 */

#ifndef WT_PLOTTINGWIDGET_H
#define WT_PLOTTINGWIDGET_H

#include <QWidget>
#include <QStandardItemModel>
#include <QMap>
#include <QListWidgetItem>
#include "chartwidget.h"
#include "chartwindow.h"

// 曲线配置结构体
struct CurveInfo {
    QString name;
    QString legendName;
    int type; // 0: Simple, 1: Stacked (P+Q), 2: Derivative
    int xCol, yCol;
    QVector<double> xData, yData;

    QCPScatterStyle::ScatterShape pointShape;
    QColor pointColor;
    Qt::PenStyle lineStyle;
    QColor lineColor;

    // Type 1 Specific (Pressure + Rate)
    int x2Col, y2Col;
    QVector<double> x2Data, y2Data;
    QString prodLegendName;
    int prodGraphType; // 0: Step, 1: Scatter
    QColor prodColor;

    // Type 2 Specific (Pressure + Derivative)
    int testType;
    double initialPressure;
    double LSpacing;
    bool isSmooth;
    int smoothFactor;
    QVector<double> derivData;
    QCPScatterStyle::ScatterShape derivShape;
    QColor derivPointColor;
    Qt::PenStyle derivLineStyle;
    QColor derivLineColor;

    QJsonObject toJson() const;
    static CurveInfo fromJson(const QJsonObject& json);
};

namespace Ui {
class WT_PlottingWidget;
}

class WT_PlottingWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WT_PlottingWidget(QWidget *parent = nullptr);
    ~WT_PlottingWidget();

    void setDataModel(QStandardItemModel* model);

    // 设置项目文件夹路径 (已弃用，改用 ModelParameter)
    void setProjectFolderPath(const QString& path);

    void loadProjectData();
    void saveProjectData();

    // 清除所有图表
    void clearAllPlots();

    // 更新图表标题 (供外部或内部设置调用)
    void updateChartTitle(const QString& title);

private slots:
    void on_btn_NewCurve_clicked();
    void on_btn_PressureRate_clicked();
    void on_btn_Derivative_clicked();

    void on_listWidget_Curves_itemDoubleClicked(QListWidgetItem *item);

    // 保存按钮槽函数
    void on_btn_Save_clicked();

    void on_btn_Manage_clicked();
    void on_btn_Delete_clicked();

    void onExportDataTriggered();
    void onGraphClicked(QCPAbstractPlottable *plottable, int dataIndex, QMouseEvent *event);

private:
    Ui::WT_PlottingWidget *ui;
    QStandardItemModel* m_dataModel;
    // QString m_projectPath; // [删除] 移除 redundant 成员

    QMap<QString, CurveInfo> m_curves;
    QString m_currentDisplayedCurve;

    QList<QWidget*> m_openedWindows;

    bool m_isSelectingForExport;
    int m_selectionStep;
    double m_exportStartIndex;
    double m_exportEndIndex;

    QCPGraph* m_graphPress;
    QCPGraph* m_graphProd;

    void addCurveToPlot(const CurveInfo& info);
    void drawStackedPlot(const CurveInfo& info);
    void drawDerivativePlot(const CurveInfo& info);

    void executeExport(bool fullRange, double start = 0, double end = 0);
    double getProductionValueAt(double t, const CurveInfo& info);
    QListWidgetItem* getCurrentSelectedItem();

    void applyDialogStyle(QWidget* dialog);
};

#endif // WT_PLOTTINGWIDGET_H
