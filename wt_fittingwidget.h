/*
 * 文件名: wt_fittingwidget.h
 * 文件作用: 试井拟合分析主界面类的头文件
 * 功能描述:
 * 1. 定义拟合分析界面的主要控件成员变量和布局逻辑。
 * 2. 声明用于Levenberg-Marquardt非线性回归拟合的核心算法函数。
 * 3. 声明观测数据（时间、压差、导数）的管理函数。
 * 4. 集成 ChartWidget 以统一图表显示和交互体验。
 */

#ifndef WT_FITTINGWIDGET_H
#define WT_FITTINGWIDGET_H

#include <QWidget>
#include <QMap>
#include <QVector>
#include <QFutureWatcher>
#include <QJsonObject>
#include <QStandardItemModel>
#include "modelmanager.h" // 包含 ModelManager 的 ModelType 定义
#include "mousezoom.h"
#include "chartwidget.h"  // [新增] 引入图表组件头文件
#include "fittingparameterchart.h"
#include "paramselectdialog.h"

namespace Ui { class FittingWidget; }

class FittingWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FittingWidget(QWidget *parent = nullptr);
    ~FittingWidget();

    // 设置模型管理器
    void setModelManager(ModelManager* m);
    // 设置项目数据模型
    void setProjectDataModel(QStandardItemModel* model);

    // 设置观测数据
    void setObservedData(const QVector<double>& t, const QVector<double>& deltaP, const QVector<double>& deriv);
    // 更新基础参数
    void updateBasicParameters();

    // 状态保存与加载
    void loadFittingState(const QJsonObject& data = QJsonObject());
    QJsonObject getJsonState() const;

signals:
    // 拟合完成信号
    void fittingCompleted(ModelManager::ModelType modelType, const QMap<QString, double>& parameters);
    // 迭代更新信号
    void sigIterationUpdated(double error, QMap<QString, double> currentParams, QVector<double> t, QVector<double> p, QVector<double> d);
    // 进度信号
    void sigProgress(int progress);
    // 请求保存信号
    void sigRequestSave();

private slots:
    // 数据加载与模型选择
    void on_btnLoadData_clicked();
    void on_btn_modelSelect_clicked();

    // 参数管理
    void on_btnSelectParams_clicked();
    void on_btnResetParams_clicked();

    // 拟合控制
    void on_btnRunFit_clicked();
    void on_btnStop_clicked();
    void on_btnImportModel_clicked();

    // 结果导出
    void on_btnExportData_clicked();   // 导出参数
    void on_btnSaveFit_clicked();      // 保存结果
    void on_btnExportReport_clicked(); // 导出报告

    // [新增] 响应 ChartWidget 的导出曲线数据请求
    void onExportCurveData();

    // 内部拟合逻辑槽函数
    void onIterationUpdate(double err, const QMap<QString,double>& p, const QVector<double>& t, const QVector<double>& p_curve, const QVector<double>& d_curve);
    void onFitFinished();
    void onSliderWeightChanged(int value);

private:
    Ui::FittingWidget *ui;
    ModelManager* m_modelManager;
    QStandardItemModel* m_projectModel;

    // [修改] 使用 ChartWidget 管理图表
    ChartWidget* m_chartWidget;
    MouseZoom* m_plot; // 指向 m_chartWidget 内部的绘图对象，保持原有代码兼容性
    QCPTextElement* m_plotTitle;

    // 当前模型类型
    ModelManager::ModelType m_currentModelType;

    // 参数表格管理类
    FittingParameterChart* m_paramChart;

    // 观测数据缓存
    QVector<double> m_obsTime;
    QVector<double> m_obsDeltaP;
    QVector<double> m_obsDerivative;

    // 拟合状态控制
    bool m_isFitting;
    bool m_stopRequested;
    QFutureWatcher<void> m_watcher;

    // 初始化图表设置
    void setupPlot();
    // 初始化默认模型
    void initializeDefaultModel();
    // 更新模型曲线
    void updateModelCurve();

    // 核心拟合算法函数 (Levenberg-Marquardt)
    void runOptimizationTask(ModelManager::ModelType modelType, QList<FitParameter> fitParams, double weight);
    void runLevenbergMarquardtOptimization(ModelManager::ModelType modelType, QList<FitParameter> params, double weight);
    QVector<double> calculateResiduals(const QMap<QString, double>& params, ModelManager::ModelType modelType, double weight);
    QVector<QVector<double>> computeJacobian(const QMap<QString, double>& params, const QVector<double>& residuals, const QVector<int>& fitIndices, ModelManager::ModelType modelType, const QList<FitParameter>& currentFitParams, double weight);
    QVector<double> solveLinearSystem(const QVector<QVector<double>>& A, const QVector<double>& b);
    double calculateSumSquaredError(const QVector<double>& residuals);

    // 辅助绘图函数
    QString getPlotImageBase64();
    void plotCurves(const QVector<double>& t, const QVector<double>& p, const QVector<double>& d, bool isModel);
};

#endif // WT_FITTINGWIDGET_H
