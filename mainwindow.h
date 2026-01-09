/*
 * mainwindow.h
 * 文件作用：主窗口类头文件
 * 功能描述：
 * 1. 声明主窗口框架及各个子功能模块指针。
 * 2. 引入 ModelManager 头文件以访问模型系统。
 * 3. [修改] 将 DataEditorWidget 替换为 WT_DataWidget。
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QTimer>
#include <QStandardItemModel>
#include "modelmanager.h"

class NavBtn;
class WT_ProjectWidget;
class WT_DataWidget; // [修改] 类名变更
class WT_PlottingWidget;
class FittingPage;
class SettingsWidget;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // 初始化主程序逻辑
    void init();

    // 各功能模块界面初始化函数
    void initProjectForm();     // 项目界面
    void initDataEditorForm();  // 数据界面
    void initModelForm();       // 模型界面
    void initPlottingForm();    // 图表界面
    void initFittingForm();     // 拟合界面
    void initPredictionForm();  // 预测界面 (新增)

private slots:
    // 项目相关的槽函数
    void onProjectOpened(bool isNew);
    void onProjectClosed();
    void onFileLoaded(const QString& filePath, const QString& fileType);

    // 数据交互与分析相关的槽函数
    void onPlotAnalysisCompleted(const QString &analysisType, const QMap<QString, double> &results);
    void onDataReadyForPlotting();
    void onTransferDataToPlotting();
    void onDataEditorDataChanged();

    // 设置与计算相关的槽函数
    void onSystemSettingsChanged();
    void onPerformanceSettingsChanged();
    void onModelCalculationCompleted(const QString &analysisType, const QMap<QString, double> &results);
    void onFittingProgressChanged(int progress);

private:
    Ui::MainWindow *ui;

    // 各个功能页面的指针
    WT_ProjectWidget* m_ProjectWidget;
    WT_DataWidget* m_DataEditorWidget; // [修改] 指针类型变更
    ModelManager* m_ModelManager;
    WT_PlottingWidget* m_PlottingWidget;
    FittingPage* m_FittingPage;
    SettingsWidget* m_SettingsWidget;

    QMap<QString, NavBtn*> m_NavBtnMap; // 导航按钮映射表
    QTimer m_timer;                     // 系统时间定时器
    bool m_hasValidData = false;        // 数据是否有效标志
    bool m_isProjectLoaded = false;     // 项目是否加载标志

    // 辅助私有函数
    void transferDataFromEditorToPlotting();
    void updateNavigationState();
    void transferDataToFitting();

    QStandardItemModel* getDataEditorModel() const;
    QString getCurrentFileName() const;
    bool hasDataLoaded();

    QString getMessageBoxStyle() const;
};

#endif // MAINWINDOW_H
