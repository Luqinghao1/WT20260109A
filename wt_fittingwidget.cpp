/*
 * 文件名: wt_fittingwidget.cpp
 * 文件作用: 试井拟合分析主界面类的实现文件
 * 功能描述:
 * 1. 初始化界面，集成 ChartWidget 作为绘图容器。
 * 2. 实现了多线程 Levenberg-Marquardt 拟合算法。
 * 3. 包含了右侧坐标系动态加载和 35% 比例初始化逻辑。
 * 4. 实现了数据的加载、处理（压差计算、导数计算及平滑）、展示。
 */

#include "wt_fittingwidget.h"
#include "ui_wt_fittingwidget.h"
#include "modelparameter.h"
#include "modelselect.h"
#include "fittingdatadialog.h"
#include "pressurederivativecalculator.h"
#include "pressurederivativecalculator1.h"

#include <QtConcurrent>
#include <QMessageBox>
#include <QDebug>
#include <cmath>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QBuffer>
#include <Eigen/Dense>

// 构造函数：初始化界面、图表和信号连接
FittingWidget::FittingWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FittingWidget),
    m_modelManager(nullptr),
    m_projectModel(nullptr),
    m_chartWidget(nullptr),
    m_plot(nullptr),
    m_plotTitle(nullptr),
    m_currentModelType(ModelManager::Model_1),
    m_isFitting(false)
{
    ui->setupUi(this);

    // 初始化图表组件 ChartWidget 并添加到布局
    m_chartWidget = new ChartWidget(this);
    ui->plotContainer->layout()->addWidget(m_chartWidget);

    // 获取底层绘图指针
    m_plot = m_chartWidget->getPlot();
    m_chartWidget->setTitle("试井解释拟合 (Well Test Fitting)");

    // 连接导出数据信号
    connect(m_chartWidget, &ChartWidget::exportDataTriggered, this, &FittingWidget::onExportCurveData);

    // 设置 Splitter 初始比例 (左侧参数栏 35% : 右侧图表 65%)
    QList<int> sizes;
    sizes << 350 << 650;
    ui->splitter->setSizes(sizes);
    ui->splitter->setCollapsible(0, false);

    // 初始化参数表格控件
    m_paramChart = new FittingParameterChart(ui->tableParams, this);

    // 配置图表样式和轴
    setupPlot();

    // 注册元类型以支持信号槽传递自定义类型
    qRegisterMetaType<QMap<QString,double>>("QMap<QString,double>");
    qRegisterMetaType<ModelManager::ModelType>("ModelManager::ModelType");
    qRegisterMetaType<QVector<double>>("QVector<double>");

    // 连接拟合过程中的信号
    connect(this, &FittingWidget::sigIterationUpdated, this, &FittingWidget::onIterationUpdate, Qt::QueuedConnection);
    connect(this, &FittingWidget::sigProgress, ui->progressBar, &QProgressBar::setValue);
    connect(&m_watcher, &QFutureWatcher<void>::finished, this, &FittingWidget::onFitFinished);

    // 连接权重滑块信号
    connect(ui->sliderWeight, &QSlider::valueChanged, this, &FittingWidget::onSliderWeightChanged);

    // 初始化权重滑块为 50%
    ui->sliderWeight->setRange(0, 100);
    ui->sliderWeight->setValue(50);
    onSliderWeightChanged(50);
}

// 析构函数：清理资源
FittingWidget::~FittingWidget()
{
    delete ui;
}

// 设置模型管理器
void FittingWidget::setModelManager(ModelManager *m)
{
    m_modelManager = m;
    m_paramChart->setModelManager(m);
    // 初始化默认模型参数
    initializeDefaultModel();
}

// 设置项目数据模型
void FittingWidget::setProjectDataModel(QStandardItemModel *model)
{
    m_projectModel = model;
}

// 更新基础参数（预留接口）
void FittingWidget::updateBasicParameters()
{
    // 同步基础参数逻辑
}

// 初始化默认模型状态
void FittingWidget::initializeDefaultModel()
{
    if(!m_modelManager) return;
    m_currentModelType = ModelManager::Model_1;
    ui->btn_modelSelect->setText("当前: " + ModelManager::getModelTypeName(m_currentModelType));
    on_btnResetParams_clicked();
}

// 配置图表外观、坐标轴和交互
void FittingWidget::setupPlot() {
    if (!m_plot) return;

    // 启用拖拽和缩放交互
    m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_plot->setBackground(Qt::white);
    m_plot->axisRect()->setBackground(Qt::white);

    // 设置坐标轴为对数坐标
    QSharedPointer<QCPAxisTickerLog> logTicker(new QCPAxisTickerLog);
    m_plot->xAxis->setScaleType(QCPAxis::stLogarithmic); m_plot->xAxis->setTicker(logTicker);
    m_plot->yAxis->setScaleType(QCPAxis::stLogarithmic); m_plot->yAxis->setTicker(logTicker);

    // 设置数字格式
    m_plot->xAxis->setNumberFormat("eb"); m_plot->xAxis->setNumberPrecision(0);
    m_plot->yAxis->setNumberFormat("eb"); m_plot->yAxis->setNumberPrecision(0);

    // 设置轴标签字体
    QFont labelFont("Microsoft YaHei", 10, QFont::Bold);
    QFont tickFont("Microsoft YaHei", 9);
    m_plot->xAxis->setLabel("时间 Time (h)");
    m_plot->yAxis->setLabel("压差 & 导数 Delta P & Derivative (MPa)");
    m_plot->xAxis->setLabelFont(labelFont); m_plot->yAxis->setLabelFont(labelFont);
    m_plot->xAxis->setTickLabelFont(tickFont); m_plot->yAxis->setTickLabelFont(tickFont);

    // 设置顶部和右侧轴可见但无标签
    m_plot->xAxis2->setVisible(true); m_plot->yAxis2->setVisible(true);
    m_plot->xAxis2->setTickLabels(false); m_plot->yAxis2->setTickLabels(false);
    // 联动坐标轴范围
    connect(m_plot->xAxis, SIGNAL(rangeChanged(QCPRange)), m_plot->xAxis2, SLOT(setRange(QCPRange)));
    connect(m_plot->yAxis, SIGNAL(rangeChanged(QCPRange)), m_plot->yAxis2, SLOT(setRange(QCPRange)));
    m_plot->xAxis2->setScaleType(QCPAxis::stLogarithmic); m_plot->yAxis2->setScaleType(QCPAxis::stLogarithmic);
    m_plot->xAxis2->setTicker(logTicker); m_plot->yAxis2->setTicker(logTicker);

    // 设置网格线样式
    m_plot->xAxis->grid()->setVisible(true); m_plot->yAxis->grid()->setVisible(true);
    m_plot->xAxis->grid()->setSubGridVisible(true); m_plot->yAxis->grid()->setSubGridVisible(true);
    m_plot->xAxis->grid()->setPen(QPen(QColor(220, 220, 220), 1, Qt::SolidLine));
    m_plot->yAxis->grid()->setPen(QPen(QColor(220, 220, 220), 1, Qt::SolidLine));
    m_plot->xAxis->grid()->setSubGridPen(QPen(QColor(240, 240, 240), 1, Qt::DotLine));
    m_plot->yAxis->grid()->setSubGridPen(QPen(QColor(240, 240, 240), 1, Qt::DotLine));

    // 设置初始范围
    m_plot->xAxis->setRange(1e-3, 1e3); m_plot->yAxis->setRange(1e-3, 1e2);

    // 添加曲线：实测压差（绿色圆点）
    m_plot->addGraph(); m_plot->graph(0)->setPen(Qt::NoPen);
    m_plot->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, QColor(0, 100, 0), 6));
    m_plot->graph(0)->setName("实测压差");

    // 添加曲线：实测导数（品红三角形）
    m_plot->addGraph(); m_plot->graph(1)->setPen(Qt::NoPen);
    m_plot->graph(1)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssTriangle, Qt::magenta, 6));
    m_plot->graph(1)->setName("实测导数");

    // 添加曲线：理论压差（红色实线）
    m_plot->addGraph(); m_plot->graph(2)->setPen(QPen(Qt::red, 2));
    m_plot->graph(2)->setName("理论压差");

    // 添加曲线：理论导数（蓝色实线）
    m_plot->addGraph(); m_plot->graph(3)->setPen(QPen(Qt::blue, 2));
    m_plot->graph(3)->setName("理论导数");

    // 配置图例
    m_plot->legend->setVisible(true);
    m_plot->legend->setFont(QFont("Microsoft YaHei", 9));
    m_plot->legend->setBrush(QBrush(QColor(255, 255, 255, 200)));
}

// 槽函数：加载观测数据
void FittingWidget::on_btnLoadData_clicked() {
    // 弹出数据加载对话框
    FittingDataDialog dlg(m_projectModel, this);
    if (dlg.exec() != QDialog::Accepted) return;

    // 获取用户配置和预览数据模型
    FittingDataSettings settings = dlg.getSettings();
    QStandardItemModel* sourceModel = dlg.getPreviewModel();

    if (!sourceModel || sourceModel->rowCount() == 0) {
        QMessageBox::warning(this, "警告", "所选数据源为空，无法加载！");
        return;
    }

    QVector<double> rawTime, rawPressureData, finalDeriv;
    int skip = settings.skipRows;
    int rows = sourceModel->rowCount();

    // 遍历数据行，提取时间和压力
    for (int i = skip; i < rows; ++i) {
        QStandardItem* itemT = sourceModel->item(i, settings.timeColIndex);
        QStandardItem* itemP = sourceModel->item(i, settings.pressureColIndex);

        if (itemT && itemP) {
            bool okT, okP;
            double t = itemT->text().toDouble(&okT);
            double p = itemP->text().toDouble(&okP);

            // 过滤无效数据，时间必须大于0
            if (okT && okP && t > 0) {
                rawTime.append(t);
                rawPressureData.append(p);
                // 如果用户选择了现成的导数列，则直接读取
                if (settings.derivColIndex >= 0) {
                    QStandardItem* itemD = sourceModel->item(i, settings.derivColIndex);
                    if (itemD) finalDeriv.append(itemD->text().toDouble());
                    else finalDeriv.append(0.0);
                }
            }
        }
    }

    if (rawTime.isEmpty()) {
        QMessageBox::warning(this, "警告", "未能提取到有效数据。");
        return;
    }

    // 计算压差 (Delta P)
    QVector<double> finalDeltaP;
    double p_shutin = rawPressureData.first();

    for (double p : rawPressureData) {
        double deltaP = 0.0;
        if (settings.testType == Test_Drawdown) {
            // 降落试井：Pi - P
            deltaP = std::abs(settings.initialPressure - p);
        } else {
            // 恢复试井：P - P_start
            deltaP = std::abs(p - p_shutin);
        }
        finalDeltaP.append(deltaP);
    }

    // 处理导数计算
    if (settings.derivColIndex == -1) {
        // 如果未选择导数列，则使用 Bourdet 算法计算
        // [修正] 使用用户设置的 L-Spacing 参数，而不是硬编码的 0.15
        finalDeriv = PressureDerivativeCalculator::calculateBourdetDerivative(rawTime, finalDeltaP, settings.lSpacing);

        // 如果启用了平滑，则进行平滑处理
        if (settings.enableSmoothing) {
            finalDeriv = PressureDerivativeCalculator1::smoothData(finalDeriv, settings.smoothingSpan);
        }
    } else {
        // 如果读取了外部导数列且启用了平滑，也进行平滑处理
        if (settings.enableSmoothing) {
            finalDeriv = PressureDerivativeCalculator1::smoothData(finalDeriv, settings.smoothingSpan);
        }
        // 确保导数数组长度与时间数组一致
        if (finalDeriv.size() != rawTime.size()) {
            finalDeriv.resize(rawTime.size());
        }
    }

    // 更新图表上的观测数据
    setObservedData(rawTime, finalDeltaP, finalDeriv);
    QMessageBox::information(this, "成功", "观测数据已成功加载。");
}

// 设置并绘制观测数据（实测压差和导数）
void FittingWidget::setObservedData(const QVector<double>& t, const QVector<double>& deltaP, const QVector<double>& d) {
    m_obsTime = t;
    m_obsDeltaP = deltaP;
    m_obsDerivative = d;

    // 过滤掉非正值以适应对数坐标系
    QVector<double> vt, vp, vd;
    for(int i=0; i<t.size(); ++i) {
        if(t[i]>1e-8 && deltaP[i]>1e-8) {
            vt << t[i];
            vp << deltaP[i];
            // 导数如果有值则添加，否则用极小值占位防止对数报错
            if(i < d.size() && d[i] > 1e-8) vd << d[i];
            else vd << 1e-10;
        }
    }

    // 更新曲线数据
    m_plot->graph(0)->setData(vt, vp);
    m_plot->graph(1)->setData(vt, vd);

    // 自动缩放坐标轴
    m_plot->rescaleAxes();
    // 确保下限不为0或负数
    if(m_plot->xAxis->range().lower <= 0) m_plot->xAxis->setRangeLower(1e-3);
    if(m_plot->yAxis->range().lower <= 0) m_plot->yAxis->setRangeLower(1e-3);
    m_plot->replot();
}

// 权重滑块改变槽函数：更新界面显示的权重比例
void FittingWidget::onSliderWeightChanged(int value)
{
    double wPressure = value / 100.0;
    double wDerivative = 1.0 - wPressure;
    ui->label_ValDerivative->setText(QString("导数权重: %1").arg(wDerivative, 0, 'f', 2));
    ui->label_ValPressure->setText(QString("压差权重: %1").arg(wPressure, 0, 'f', 2));
}

// 打开参数选择对话框
void FittingWidget::on_btnSelectParams_clicked()
{
    m_paramChart->updateParamsFromTable();
    QList<FitParameter> currentParams = m_paramChart->getParameters();
    ParamSelectDialog dlg(currentParams, this);
    if(dlg.exec() == QDialog::Accepted) {
        m_paramChart->setParameters(dlg.getUpdatedParams());
        updateModelCurve();
    }
}

// 执行拟合
void FittingWidget::on_btnRunFit_clicked() {
    if(m_isFitting) return;
    if(m_obsTime.isEmpty()) {
        QMessageBox::warning(this,"错误","请先加载观测数据。");
        return;
    }

    m_paramChart->updateParamsFromTable();
    m_isFitting = true;
    m_stopRequested = false;
    ui->btnRunFit->setEnabled(false);

    // 准备拟合参数
    ModelManager::ModelType modelType = m_currentModelType;
    QList<FitParameter> paramsCopy = m_paramChart->getParameters();
    double w = ui->sliderWeight->value() / 100.0;

    // 启动异步线程执行拟合任务，避免阻塞 UI
    m_watcher.setFuture(QtConcurrent::run([this, modelType, paramsCopy, w](){
        runOptimizationTask(modelType, paramsCopy, w);
    }));
}

// 停止拟合
void FittingWidget::on_btnStop_clicked() {
    m_stopRequested = true;
}

// 导入模型参数（实际上是刷新曲线）
void FittingWidget::on_btnImportModel_clicked() {
    updateModelCurve();
}

// 重置参数为默认值
void FittingWidget::on_btnResetParams_clicked() {
    if(!m_modelManager) return;
    m_paramChart->resetParams(m_currentModelType);
    updateModelCurve();
}

// 选择解释模型
void FittingWidget::on_btn_modelSelect_clicked() {
    ModelSelect dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString code = dlg.getSelectedModelCode();
        QString name = dlg.getSelectedModelName();

        bool found = false;
        ModelManager::ModelType newType = ModelManager::Model_1;

        // 根据代码映射模型类型
        if (code == "modelwidget1") newType = ModelManager::Model_1;
        else if (code == "modelwidget2") newType = ModelManager::Model_2;
        else if (code == "modelwidget3") newType = ModelManager::Model_3;
        else if (code == "modelwidget4") newType = ModelManager::Model_4;
        else if (code == "modelwidget5") newType = ModelManager::Model_5;
        else if (code == "modelwidget6") newType = ModelManager::Model_6;
        else if (!code.isEmpty()) found = true;

        if (code.startsWith("modelwidget")) found = true;

        if (found) {
            m_paramChart->switchModel(newType);
            m_currentModelType = newType;
            ui->btn_modelSelect->setText("当前: " + name);
            updateModelCurve();
        } else {
            QMessageBox::warning(this, "提示", "所选组合暂无对应的模型。\nCode: " + code);
        }
    }
}

// 导出拟合参数
void FittingWidget::on_btnExportData_clicked() {
    m_paramChart->updateParamsFromTable();
    QList<FitParameter> params = m_paramChart->getParameters();

    QString defaultDir = ModelParameter::instance()->getProjectPath();
    if(defaultDir.isEmpty()) defaultDir = ".";

    QString fileName = QFileDialog::getSaveFileName(this, "导出拟合参数", defaultDir + "/FittingParameters.csv", "CSV Files (*.csv);;Text Files (*.txt)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);

    if(fileName.endsWith(".csv", Qt::CaseInsensitive)) {
        file.write("\xEF\xBB\xBF"); // BOM header
        out << QString("参数中文名,参数英文名,拟合值,单位\n");
        for(const auto& param : params) {
            QString htmlSym, uniSym, unitStr, dummyName;
            FittingParameterChart::getParamDisplayInfo(param.name, dummyName, htmlSym, uniSym, unitStr);
            if(unitStr == "无因次" || unitStr == "小数") unitStr = "";
            out << QString("%1,%2,%3,%4\n").arg(param.displayName).arg(uniSym).arg(param.value, 0, 'g', 10).arg(unitStr);
        }
    } else {
        for(const auto& param : params) {
            QString htmlSym, uniSym, unitStr, dummyName;
            FittingParameterChart::getParamDisplayInfo(param.name, dummyName, htmlSym, uniSym, unitStr);
            if(unitStr == "无因次" || unitStr == "小数") unitStr = "";
            QString lineStr = QString("%1 (%2): %3 %4").arg(param.displayName).arg(uniSym).arg(param.value, 0, 'g', 10).arg(unitStr);
            out << lineStr.trimmed() << "\n";
        }
    }
    file.close();
    QMessageBox::information(this, "完成", "参数数据已成功导出。");
}

// 导出拟合曲线数据
void FittingWidget::onExportCurveData() {
    QString defaultDir = ModelParameter::instance()->getProjectPath();
    if(defaultDir.isEmpty()) defaultDir = ".";

    QString path = QFileDialog::getSaveFileName(this, "导出拟合曲线数据", defaultDir + "/FittingCurves.csv", "CSV Files (*.csv)");
    if (path.isEmpty()) return;

    auto graphObsP = m_plot->graph(0);
    auto graphObsD = m_plot->graph(1);
    auto graphModP = m_plot->graph(2);
    auto graphModD = m_plot->graph(3);

    if (!graphObsP || !graphModP) return;

    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        out << "Obs_Time,Obs_DP,Obs_Deriv,Model_Time,Model_DP,Model_Deriv\n";

        // 迭代器遍历数据点
        auto itObsP = graphObsP->data()->begin();
        auto itObsD = graphObsD->data()->begin();
        auto itModP = graphModP->data()->begin();
        auto itModD = graphModD->data()->begin();

        auto endObsP = graphObsP->data()->end();
        auto endModP = graphModP->data()->end();

        // 逐行写入CSV
        while (itObsP != endObsP || itModP != endModP) {
            QStringList line;

            if (itObsP != endObsP) {
                line << QString::number(itObsP->key, 'g', 10);
                line << QString::number(itObsP->value, 'g', 10);
                if (itObsD != graphObsD->data()->end()) {
                    line << QString::number(itObsD->value, 'g', 10);
                    ++itObsD;
                } else {
                    line << "";
                }
                ++itObsP;
            } else {
                line << "" << "" << "";
            }

            if (itModP != endModP) {
                line << QString::number(itModP->key, 'g', 10);
                line << QString::number(itModP->value, 'g', 10);
                if (itModD != graphModD->data()->end()) {
                    line << QString::number(itModD->value, 'g', 10);
                    ++itModD;
                } else {
                    line << "";
                }
                ++itModP;
            } else {
                line << "" << "" << "";
            }

            out << line.join(",") << "\n";
        }

        f.close();
        QMessageBox::information(this, "导出成功", "拟合曲线数据已保存。");
    }
}

// 运行优化任务的包装函数
void FittingWidget::runOptimizationTask(ModelManager::ModelType modelType, QList<FitParameter> fitParams, double weight) {
    runLevenbergMarquardtOptimization(modelType, fitParams, weight);
}

// Levenberg-Marquardt 算法实现
void FittingWidget::runLevenbergMarquardtOptimization(ModelManager::ModelType modelType, QList<FitParameter> params, double weight) {
    // 降低 Solver 精度以提高拟合速度
    if(m_modelManager) m_modelManager->setHighPrecision(false);

    // 找出需要拟合的参数索引
    QVector<int> fitIndices;
    for(int i=0; i<params.size(); ++i) {
        if(params[i].isFit) fitIndices.append(i);
    }
    int nParams = fitIndices.size();

    // 如果没有选定拟合参数，直接返回
    if(nParams == 0) {
        QMetaObject::invokeMethod(this, "onFitFinished");
        return;
    }

    // LM 算法参数初始化
    double lambda = 0.01;
    int maxIter = 50;
    double currentSSE = 1e15;

    // 构建参数映射
    QMap<QString, double> currentParamMap;
    for(const auto& p : params) currentParamMap.insert(p.name, p.value);

    // 处理复合参数（如裂缝穿透比 LfD）
    if(currentParamMap.contains("L") && currentParamMap.contains("Lf") && currentParamMap["L"] > 1e-9)
        currentParamMap["LfD"] = currentParamMap["Lf"] / currentParamMap["L"];

    // 计算初始残差
    QVector<double> residuals = calculateResiduals(currentParamMap, modelType, weight);
    currentSSE = calculateSumSquaredError(residuals);

    // 发送初始状态信号
    ModelCurveData curve = m_modelManager->calculateTheoreticalCurve(modelType, currentParamMap);
    emit sigIterationUpdated(currentSSE/residuals.size(), currentParamMap, std::get<0>(curve), std::get<1>(curve), std::get<2>(curve));

    // 迭代循环
    for(int iter = 0; iter < maxIter; ++iter) {
        if(m_stopRequested) break;
        // 收敛条件判断
        if (!residuals.isEmpty() && (currentSSE / residuals.size()) < 3e-3) break;

        emit sigProgress(iter * 100 / maxIter);

        // 计算雅可比矩阵 J
        QVector<QVector<double>> J = computeJacobian(currentParamMap, residuals, fitIndices, modelType, params, weight);
        int nRes = residuals.size();

        // 计算 H = J^T * J 和 g = J^T * r
        QVector<QVector<double>> H(nParams, QVector<double>(nParams, 0.0));
        QVector<double> g(nParams, 0.0);

        for(int k=0; k<nRes; ++k) {
            for(int i=0; i<nParams; ++i) {
                g[i] += J[k][i] * residuals[k];
                for(int j=0; j<=i; ++j) {
                    H[i][j] += J[k][i] * J[k][j];
                }
            }
        }
        // 对称填充 H 矩阵
        for(int i=0; i<nParams; ++i) {
            for(int j=i+1; j<nParams; ++j) {
                H[i][j] = H[j][i];
            }
        }

        bool stepAccepted = false;
        // 尝试更新步骤
        for(int tryIter=0; tryIter<5; ++tryIter) {
            // H_lm = H + lambda * I
            QVector<QVector<double>> H_lm = H;
            for(int i=0; i<nParams; ++i) {
                H_lm[i][i] += lambda * (1.0 + std::abs(H[i][i]));
            }

            QVector<double> negG(nParams);
            for(int i=0;i<nParams;++i) negG[i] = -g[i];

            // 求解线性方程组 H_lm * delta = -g
            QVector<double> delta = solveLinearSystem(H_lm, negG);
            QMap<QString, double> trialMap = currentParamMap;

            // 更新参数值
            for(int i=0; i<nParams; ++i) {
                int pIdx = fitIndices[i];
                QString pName = params[pIdx].name;
                double oldVal = currentParamMap[pName];
                // 对数敏感参数进行对数空间更新
                bool isLog = (oldVal > 1e-12 && pName != "S" && pName != "nf");
                double newVal;

                if(isLog) newVal = pow(10.0, log10(oldVal) + delta[i]);
                else newVal = oldVal + delta[i];

                // 约束参数范围
                newVal = qMax(params[pIdx].min, qMin(newVal, params[pIdx].max));
                trialMap[pName] = newVal;
            }

            // 更新依赖参数
            if(trialMap.contains("L") && trialMap.contains("Lf") && trialMap["L"] > 1e-9)
                trialMap["LfD"] = trialMap["Lf"] / trialMap["L"];

            // 计算新误差
            QVector<double> newRes = calculateResiduals(trialMap, modelType, weight);
            double newSSE = calculateSumSquaredError(newRes);

            // 如果误差减小，接受步长并减小 lambda
            if(newSSE < currentSSE) {
                currentSSE = newSSE;
                currentParamMap = trialMap;
                residuals = newRes;
                lambda /= 10.0;
                stepAccepted = true;
                ModelCurveData iterCurve = m_modelManager->calculateTheoreticalCurve(modelType, currentParamMap);
                emit sigIterationUpdated(currentSSE/nRes, currentParamMap, std::get<0>(iterCurve), std::get<1>(iterCurve), std::get<2>(iterCurve));
                break;
            } else {
                // 否则增大 lambda 增加阻尼
                lambda *= 10.0;
            }
        }
        if(!stepAccepted && lambda > 1e10) break;
    }

    // 恢复高精度计算
    if(m_modelManager) m_modelManager->setHighPrecision(true);

    // 最终更新一次界面
    if(currentParamMap.contains("L") && currentParamMap.contains("Lf") && currentParamMap["L"] > 1e-9)
        currentParamMap["LfD"] = currentParamMap["Lf"] / currentParamMap["L"];

    ModelCurveData finalCurve = m_modelManager->calculateTheoreticalCurve(modelType, currentParamMap);
    emit sigIterationUpdated(currentSSE/residuals.size(), currentParamMap, std::get<0>(finalCurve), std::get<1>(finalCurve), std::get<2>(finalCurve));

    QMetaObject::invokeMethod(this, "onFitFinished");
}

// 计算残差向量
QVector<double> FittingWidget::calculateResiduals(const QMap<QString, double>& params, ModelManager::ModelType modelType, double weight) {
    if(!m_modelManager || m_obsTime.isEmpty()) return QVector<double>();

    // 调用 Manager 接口计算理论曲线
    ModelCurveData res = m_modelManager->calculateTheoreticalCurve(modelType, params, m_obsTime);
    const QVector<double>& pCal = std::get<1>(res);
    const QVector<double>& dpCal = std::get<2>(res);

    QVector<double> r;
    double wp = weight;
    double wd = 1.0 - weight;

    // 计算压差残差 (对数差值)
    int count = qMin(m_obsDeltaP.size(), pCal.size());
    for(int i=0; i<count; ++i) {
        if(m_obsDeltaP[i] > 1e-10 && pCal[i] > 1e-10)
            r.append( (log(m_obsDeltaP[i]) - log(pCal[i])) * wp );
        else
            r.append(0.0);
    }

    // 计算导数残差 (对数差值)
    int dCount = qMin(m_obsDerivative.size(), dpCal.size());
    dCount = qMin(dCount, count);
    for(int i=0; i<dCount; ++i) {
        if(m_obsDerivative[i] > 1e-10 && dpCal[i] > 1e-10)
            r.append( (log(m_obsDerivative[i]) - log(dpCal[i])) * wd );
        else
            r.append(0.0);
    }
    return r;
}

// 计算雅可比矩阵（有限差分法）
QVector<QVector<double>> FittingWidget::computeJacobian(const QMap<QString, double>& params, const QVector<double>& baseResiduals, const QVector<int>& fitIndices, ModelManager::ModelType modelType, const QList<FitParameter>& currentFitParams, double weight) {
    int nRes = baseResiduals.size();
    int nParams = fitIndices.size();
    QVector<QVector<double>> J(nRes, QVector<double>(nParams));

    for(int j = 0; j < nParams; ++j) {
        int idx = fitIndices[j];
        QString pName = currentFitParams[idx].name;
        double val = params.value(pName);
        bool isLog = (val > 1e-12 && pName != "S" && pName != "nf");

        double h;
        QMap<QString, double> pPlus = params;
        QMap<QString, double> pMinus = params;

        // 确定扰动步长 h
        if(isLog) {
            h = 0.01;
            double valLog = log10(val);
            pPlus[pName] = pow(10.0, valLog + h);
            pMinus[pName] = pow(10.0, valLog - h);
        } else {
            h = 1e-4;
            pPlus[pName] = val + h;
            pMinus[pName] = val - h;
        }

        // 联动更新依赖参数
        auto updateDeps = [](QMap<QString,double>& map) { if(map.contains("L") && map.contains("Lf") && map["L"] > 1e-9) map["LfD"] = map["Lf"] / map["L"]; };
        if(pName == "L" || pName == "Lf") { updateDeps(pPlus); updateDeps(pMinus); }

        QVector<double> rPlus = calculateResiduals(pPlus, modelType, weight);
        QVector<double> rMinus = calculateResiduals(pMinus, modelType, weight);

        if(rPlus.size() == nRes && rMinus.size() == nRes) {
            for(int i=0; i<nRes; ++i) {
                // 中心差分公式
                J[i][j] = (rPlus[i] - rMinus[i]) / (2.0 * h);
            }
        }
    }
    return J;
}

// 求解线性方程组 (Ax = b) 使用 Eigen 库
QVector<double> FittingWidget::solveLinearSystem(const QVector<QVector<double>>& A, const QVector<double>& b) {
    int n = b.size();
    if (n == 0) return QVector<double>();

    Eigen::MatrixXd matA(n, n);
    Eigen::VectorXd vecB(n);

    for (int i = 0; i < n; ++i) {
        vecB(i) = b[i];
        for (int j = 0; j < n; ++j) {
            matA(i, j) = A[i][j];
        }
    }

    // 使用 LDLT 分解求解
    Eigen::VectorXd x = matA.ldlt().solve(vecB);

    QVector<double> res(n);
    for (int i = 0; i < n; ++i) res[i] = x(i);
    return res;
}

// 计算误差平方和
double FittingWidget::calculateSumSquaredError(const QVector<double>& residuals) {
    double sse = 0.0;
    for(double v : residuals) sse += v*v;
    return sse;
}

// 更新界面上的理论曲线
void FittingWidget::updateModelCurve() {
    if(!m_modelManager) {
        QMessageBox::critical(this, "错误", "ModelManager 未初始化！");
        return;
    }
    ui->tableParams->clearFocus();

    m_paramChart->updateParamsFromTable();
    QList<FitParameter> params = m_paramChart->getParameters();

    QMap<QString,double> currentParams;
    for(const auto& p : params) currentParams.insert(p.name, p.value);

    // 计算依赖参数
    if(currentParams.contains("L") && currentParams.contains("Lf") && currentParams["L"] > 1e-9)
        currentParams["LfD"] = currentParams["Lf"] / currentParams["L"];
    else
        currentParams["LfD"] = 0.0;

    ModelManager::ModelType type = m_currentModelType;
    QVector<double> targetT = m_obsTime;
    // 如果没有观测时间，则生成默认的时间序列
    if(targetT.isEmpty()) {
        for(double e = -4; e <= 4; e += 0.1) targetT.append(pow(10, e));
    }

    ModelCurveData res = m_modelManager->calculateTheoreticalCurve(type, currentParams, targetT);
    onIterationUpdate(0, currentParams, std::get<0>(res), std::get<1>(res), std::get<2>(res));
}

// 迭代更新槽函数：刷新误差显示和图表
void FittingWidget::onIterationUpdate(double err, const QMap<QString,double>& p,
                                      const QVector<double>& t, const QVector<double>& p_curve, const QVector<double>& d_curve) {
    ui->label_Error->setText(QString("误差(MSE): %1").arg(err, 0, 'e', 3));

    // 更新表格中的参数值
    ui->tableParams->blockSignals(true);
    for(int i=0; i<ui->tableParams->rowCount(); ++i) {
        QString key = ui->tableParams->item(i, 1)->data(Qt::UserRole).toString();
        if(p.contains(key)) {
            double val = p[key];
            ui->tableParams->item(i, 2)->setText(QString::number(val, 'g', 5));
        }
    }
    ui->tableParams->blockSignals(false);

    // 绘制曲线
    plotCurves(t, p_curve, d_curve, true);
}

// 拟合完成槽函数
void FittingWidget::onFitFinished() {
    m_isFitting = false;
    ui->btnRunFit->setEnabled(true);
    QMessageBox::information(this, "完成", "拟合完成。");
}

// 绘制曲线的通用方法
void FittingWidget::plotCurves(const QVector<double>& t, const QVector<double>& p, const QVector<double>& d, bool isModel) {
    if (!m_plot) return;

    QVector<double> vt, vp, vd;
    for(int i=0; i<t.size(); ++i) {
        if(t[i]>1e-8 && p[i]>1e-8) {
            vt<<t[i];
            vp<<p[i];
            if(i<d.size() && d[i]>1e-8) vd<<d[i]; else vd<<1e-10;
        }
    }
    if(isModel) {
        m_plot->graph(2)->setData(vt, vp);
        m_plot->graph(3)->setData(vt, vd);

        // 如果没有观测数据，自动缩放以显示理论曲线
        if (m_obsTime.isEmpty() && !vt.isEmpty()) {
            m_plot->rescaleAxes();
            if(m_plot->xAxis->range().lower<=0) m_plot->xAxis->setRangeLower(1e-3);
            if(m_plot->yAxis->range().lower<=0) m_plot->yAxis->setRangeLower(1e-3);
        }
        m_plot->replot();
    }
}

// 导出报告
void FittingWidget::on_btnExportReport_clicked()
{
    m_paramChart->updateParamsFromTable();
    QList<FitParameter> params = m_paramChart->getParameters();

    QString defaultDir = ModelParameter::instance()->getProjectPath();
    if(defaultDir.isEmpty()) defaultDir = ".";
    QString fileName = QFileDialog::getSaveFileName(this, "导出试井分析报告",
                                                    defaultDir + "/WellTestReport.doc",
                                                    "Word 文档 (*.doc);;HTML 文件 (*.html)");
    if(fileName.isEmpty()) return;

    ModelParameter* mp = ModelParameter::instance();

    // 生成 HTML 报告内容
    QString html = "<html><head><style>";
    html += "body { font-family: 'Times New Roman', 'SimSun', serif; }";
    html += "h1 { text-align: center; font-size: 24px; font-weight: bold; margin-bottom: 20px; }";
    html += "h2 { font-size: 18px; font-weight: bold; background-color: #f2f2f2; padding: 5px; border-left: 5px solid #2d89ef; margin-top: 20px; }";
    html += "table { width: 100%; border-collapse: collapse; margin-bottom: 15px; font-size: 14px; }";
    html += "td, th { border: 1px solid #888; padding: 6px; text-align: center; }";
    html += "th { background-color: #e0e0e0; font-weight: bold; }";
    html += ".param-table td { text-align: left; padding-left: 10px; }";
    html += "</style></head><body>";

    html += "<h1>试井解释分析报告</h1>";
    html += "<p style='text-align:right;'>生成日期: " + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm") + "</p>";

    html += "<h2>1. 基础信息</h2>";
    html += "<table class='param-table'>";
    html += "<tr><td width='30%'>项目路径</td><td>" + mp->getProjectPath() + "</td></tr>";
    html += "<tr><td>测试产量 (q)</td><td>" + QString::number(mp->getQ()) + " m³/d</td></tr>";
    html += "<tr><td>有效厚度 (h)</td><td>" + QString::number(mp->getH()) + " m</td></tr>";
    html += "<tr><td>孔隙度 (φ)</td><td>" + QString::number(mp->getPhi()) + "</td></tr>";
    html += "<tr><td>井筒半径 (rw)</td><td>" + QString::number(mp->getRw()) + " m</td></tr>";
    html += "</table>";

    html += "<h2>2. 流体高压物性 (PVT)</h2>";
    html += "<table class='param-table'>";
    html += "<tr><td width='30%'>原油粘度 (μ)</td><td>" + QString::number(mp->getMu()) + " mPa·s</td></tr>";
    html += "<tr><td>体积系数 (B)</td><td>" + QString::number(mp->getB()) + "</td></tr>";
    html += "<tr><td>综合压缩系数 (Ct)</td><td>" + QString::number(mp->getCt()) + " MPa⁻¹</td></tr>";
    html += "</table>";

    html += "<h2>3. 解释模型选择</h2>";
    html += "<p><strong>当前模型:</strong> " + ModelManager::getModelTypeName(m_currentModelType) + "</p>";

    html += "<h2>4. 拟合结果参数</h2>";
    html += "<table>";
    html += "<tr><th>参数名称</th><th>符号</th><th>拟合结果</th><th>单位</th></tr>";
    for(const auto& p : params) {
        QString dummy, symbol, uniSym, unit;
        FittingParameterChart::getParamDisplayInfo(p.name, dummy, symbol, uniSym, unit);
        if(unit == "无因次" || unit == "小数") unit = "-";

        html += "<tr>";
        html += "<td>" + p.displayName + "</td>";
        html += "<td>" + uniSym + "</td>";
        if(p.isFit)
            html += "<td><strong>" + QString::number(p.value, 'g', 6) + "</strong></td>";
        else
            html += "<td>" + QString::number(p.value, 'g', 6) + "</td>";
        html += "<td>" + unit + "</td>";
        html += "</tr>";
    }
    html += "</table>";

    html += "<h2>5. 拟合曲线图</h2>";
    QString imgBase64 = getPlotImageBase64();
    if(!imgBase64.isEmpty()) {
        html += "<div style='text-align:center;'><img src='data:image/png;base64," + imgBase64 + "' width='600' /></div>";
    } else {
        html += "<p>图像导出失败。</p>";
    }

    html += "</body></html>";

    QFile file(fileName);
    if(file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << html;
        file.close();
        QMessageBox::information(this, "导出成功", "报告已保存至:\n" + fileName);
    } else {
        QMessageBox::critical(this, "错误", "无法写入文件，请检查权限或文件是否被占用。");
    }
}

// 获取图表截图的 Base64 编码
QString FittingWidget::getPlotImageBase64()
{
    if(!m_plot) return "";
    QPixmap pixmap = m_plot->toPixmap(800, 600);
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    pixmap.save(&buffer, "PNG");
    return QString::fromLatin1(byteArray.toBase64().data());
}

// 保存拟合状态信号
void FittingWidget::on_btnSaveFit_clicked()
{
    emit sigRequestSave();
}

// 获取当前界面状态的 JSON 对象（用于项目保存）
QJsonObject FittingWidget::getJsonState() const
{
    const_cast<FittingWidget*>(this)->m_paramChart->updateParamsFromTable();
    QList<FitParameter> params = m_paramChart->getParameters();

    QJsonObject root;
    root["modelType"] = (int)m_currentModelType;
    root["modelName"] = ModelManager::getModelTypeName(m_currentModelType);
    root["fitWeightVal"] = ui->sliderWeight->value();

    QJsonObject plotRange;
    plotRange["xMin"] = m_plot->xAxis->range().lower;
    plotRange["xMax"] = m_plot->xAxis->range().upper;
    plotRange["yMin"] = m_plot->yAxis->range().lower;
    plotRange["yMax"] = m_plot->yAxis->range().upper;
    root["plotView"] = plotRange;

    QJsonArray paramsArray;
    for(const auto& p : params) {
        QJsonObject pObj;
        pObj["name"] = p.name;
        pObj["value"] = p.value;
        pObj["isFit"] = p.isFit;
        pObj["min"] = p.min;
        pObj["max"] = p.max;
        pObj["isVisible"] = p.isVisible;
        paramsArray.append(pObj);
    }
    root["parameters"] = paramsArray;

    QJsonArray timeArr, pressArr, derivArr;
    for(double v : m_obsTime) timeArr.append(v);
    for(double v : m_obsDeltaP) pressArr.append(v);
    for(double v : m_obsDerivative) derivArr.append(v);
    QJsonObject obsData;
    obsData["time"] = timeArr;
    obsData["pressure"] = pressArr;
    obsData["derivative"] = derivArr;
    root["observedData"] = obsData;

    return root;
}

// 从 JSON 对象加载界面状态（用于项目加载）
void FittingWidget::loadFittingState(const QJsonObject& root)
{
    if (root.isEmpty()) return;

    if (root.contains("modelType")) {
        int type = root["modelType"].toInt();
        m_currentModelType = (ModelManager::ModelType)type;
        ui->btn_modelSelect->setText("当前: " + ModelManager::getModelTypeName(m_currentModelType));
    }

    m_paramChart->resetParams(m_currentModelType);

    if (root.contains("parameters")) {
        QJsonArray arr = root["parameters"].toArray();
        QList<FitParameter> currentParams = m_paramChart->getParameters();

        for(int i=0; i<arr.size(); ++i) {
            QJsonObject pObj = arr[i].toObject();
            QString name = pObj["name"].toString();

            for(auto& p : currentParams) {
                if(p.name == name) {
                    p.value = pObj["value"].toDouble();
                    p.isFit = pObj["isFit"].toBool();
                    p.min = pObj["min"].toDouble();
                    p.max = pObj["max"].toDouble();
                    if(pObj.contains("isVisible")) {
                        p.isVisible = pObj["isVisible"].toBool();
                    } else {
                        p.isVisible = true;
                    }
                    break;
                }
            }
        }
        m_paramChart->setParameters(currentParams);
    }

    if (root.contains("fitWeightVal")) {
        int val = root["fitWeightVal"].toInt();
        ui->sliderWeight->setValue(val);
    } else if (root.contains("fitWeight")) {
        double w = root["fitWeight"].toDouble();
        ui->sliderWeight->setValue((int)(w * 100));
    }

    if (root.contains("observedData")) {
        QJsonObject obs = root["observedData"].toObject();
        QJsonArray tArr = obs["time"].toArray();
        QJsonArray pArr = obs["pressure"].toArray();
        QJsonArray dArr = obs["derivative"].toArray();

        QVector<double> t, p, d;
        for(auto v : tArr) t.append(v.toDouble());
        for(auto v : pArr) p.append(v.toDouble());
        for(auto v : dArr) d.append(v.toDouble());

        setObservedData(t, p, d);
    }

    updateModelCurve();

    if (root.contains("plotView")) {
        QJsonObject range = root["plotView"].toObject();
        if (range.contains("xMin") && range.contains("xMax")) {
            double xMin = range["xMin"].toDouble();
            double xMax = range["xMax"].toDouble();
            double yMin = range["yMin"].toDouble();
            double yMax = range["yMax"].toDouble();
            if (xMax > xMin && yMax > yMin && xMin > 0 && yMin > 0) {
                m_plot->xAxis->setRange(xMin, xMax);
                m_plot->yAxis->setRange(yMin, yMax);
                m_plot->replot();
            }
        }
    }
}
