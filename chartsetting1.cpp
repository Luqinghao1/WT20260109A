/*
 * chartsetting1.cpp
 * 文件作用：图表设置对话框类的具体实现
 * 功能描述：
 * 1. 初始化对话框界面，加载当前图表的各项参数（范围、标题、对数坐标、网格等）
 * 2. 实现将用户修改的参数应用回图表对象
 * 3. 处理坐标轴数字格式，确保在不使用科学计数法时只保留有效数字
 * 4. [新增] 支持查看和修改曲线名称（图例标题）
 */

#include "chartsetting1.h"
#include "ui_chartsetting1.h"
#include <QDebug>

// 构造函数
ChartSetting1::ChartSetting1(MouseZoom* plot, QCPTextElement* title, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ChartSetting1),
    m_plot(plot),
    m_title(title)
{
    ui->setupUi(this);

    // 设置对话框标题
    this->setWindowTitle("图表设置");

    // [新增] 设置表格列头样式
    ui->tableGraphs->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->tableGraphs->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    // 初始化界面数据
    initData();
}

// 析构函数
ChartSetting1::~ChartSetting1()
{
    delete ui;
}

// 初始化数据：从 m_plot 读取当前状态填入 UI
void ChartSetting1::initData()
{
    if (!m_plot) return;

    // --- 1. 标题设置 ---
    if (m_title) {
        ui->editTitle->setText(m_title->text());
        ui->checkTitleVisible->setChecked(m_title->visible());
    }

    // --- 2. X轴设置 ---
    QCPAxis *x = m_plot->xAxis;
    ui->editXLabel->setText(x->label());
    ui->spinXMin->setValue(x->range().lower);
    ui->spinXMax->setValue(x->range().upper);

    // 检查是否为对数坐标
    bool isLogX = (x->scaleType() == QCPAxis::stLogarithmic);
    ui->checkXLog->setChecked(isLogX);

    // 检查是否启用了科学计数法
    QString xFormat = x->numberFormat();
    ui->checkXSci->setChecked(xFormat.contains("e"));

    // 网格设置
    ui->checkXGrid->setChecked(x->grid()->visible());
    ui->checkXSubGrid->setChecked(x->grid()->subGridVisible());

    // --- 3. Y轴设置 ---
    QCPAxis *y = m_plot->yAxis;
    ui->editYLabel->setText(y->label());
    ui->spinYMin->setValue(y->range().lower);
    ui->spinYMax->setValue(y->range().upper);

    bool isLogY = (y->scaleType() == QCPAxis::stLogarithmic);
    ui->checkYLog->setChecked(isLogY);

    QString yFormat = y->numberFormat();
    ui->checkYSci->setChecked(yFormat.contains("e"));

    ui->checkYGrid->setChecked(y->grid()->visible());
    ui->checkYSubGrid->setChecked(y->grid()->subGridVisible());

    // --- 4. [新增] 曲线列表初始化 ---
    int graphCount = m_plot->graphCount();
    ui->tableGraphs->setRowCount(graphCount);
    for(int i = 0; i < graphCount; ++i) {
        // 第一列：索引
        QTableWidgetItem* indexItem = new QTableWidgetItem(QString::number(i + 1));
        indexItem->setFlags(indexItem->flags() ^ Qt::ItemIsEditable); // 禁止编辑索引
        ui->tableGraphs->setItem(i, 0, indexItem);

        // 第二列：曲线名称 (可编辑)
        QTableWidgetItem* nameItem = new QTableWidgetItem(m_plot->graph(i)->name());
        ui->tableGraphs->setItem(i, 1, nameItem);
    }
}

// 应用设置：将 UI 数据写回 m_plot
void ChartSetting1::applySettings()
{
    if (!m_plot) return;

    // --- 1. 标题应用 ---
    if (m_title) {
        m_title->setText(ui->editTitle->text());
        m_title->setVisible(ui->checkTitleVisible->isChecked());
    }

    // --- 2. X轴应用 ---
    QCPAxis *x = m_plot->xAxis;
    x->setLabel(ui->editXLabel->text());
    x->setRange(ui->spinXMin->value(), ui->spinXMax->value());

    // 设置对数坐标/线性坐标
    if (ui->checkXLog->isChecked()) {
        x->setScaleType(QCPAxis::stLogarithmic);
        QSharedPointer<QCPAxisTickerLog> logTicker(new QCPAxisTickerLog);
        x->setTicker(logTicker);
    } else {
        x->setScaleType(QCPAxis::stLinear);
        QSharedPointer<QCPAxisTicker> linTicker(new QCPAxisTicker);
        x->setTicker(linTicker);
    }

    // 设置数字格式
    if (ui->checkXSci->isChecked()) {
        x->setNumberFormat("eb");
        x->setNumberPrecision(0);
    } else {
        x->setNumberFormat("g");
        x->setNumberPrecision(5);
    }

    x->grid()->setVisible(ui->checkXGrid->isChecked());
    x->grid()->setSubGridVisible(ui->checkXSubGrid->isChecked());

    // --- 3. Y轴应用 ---
    QCPAxis *y = m_plot->yAxis;
    y->setLabel(ui->editYLabel->text());
    y->setRange(ui->spinYMin->value(), ui->spinYMax->value());

    if (ui->checkYLog->isChecked()) {
        y->setScaleType(QCPAxis::stLogarithmic);
        QSharedPointer<QCPAxisTickerLog> logTicker(new QCPAxisTickerLog);
        y->setTicker(logTicker);
    } else {
        y->setScaleType(QCPAxis::stLinear);
        QSharedPointer<QCPAxisTicker> linTicker(new QCPAxisTicker);
        y->setTicker(linTicker);
    }

    // 设置Y轴数字格式
    if (ui->checkYSci->isChecked()) {
        y->setNumberFormat("eb");
        y->setNumberPrecision(0);
    } else {
        y->setNumberFormat("g");
        y->setNumberPrecision(5);
    }

    y->grid()->setVisible(ui->checkYGrid->isChecked());
    y->grid()->setSubGridVisible(ui->checkYSubGrid->isChecked());

    // --- 4. [新增] 应用曲线名称修改 ---
    int graphCount = m_plot->graphCount();
    // 防御性检查：确保表格行数与曲线数一致（虽然模态对话框通常不会变，但安全第一）
    if(ui->tableGraphs->rowCount() == graphCount) {
        for(int i = 0; i < graphCount; ++i) {
            QTableWidgetItem* item = ui->tableGraphs->item(i, 1);
            if(item) {
                QString newName = item->text();
                // 仅当名称变化时更新
                if(m_plot->graph(i)->name() != newName) {
                    m_plot->graph(i)->setName(newName);
                }
            }
        }
    }

    // 刷新图表
    m_plot->replot();
}

// 确定按钮
void ChartSetting1::on_btnOk_clicked()
{
    applySettings();
    accept();
}

// 应用按钮
void ChartSetting1::on_btnApply_clicked()
{
    applySettings();
}

// 取消按钮
void ChartSetting1::on_btnCancel_clicked()
{
    reject();
}
