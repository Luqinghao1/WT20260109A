/*
 * 文件名: paramselectdialog.cpp
 * 文件作用: 参数选择配置对话框的具体实现
 * 功能描述:
 * 1. 初始化对话框界面，根据参数列表动态生成表格行。
 * 2. 实现复选框样式的自定义（蓝色选中态），解决默认样式不清晰的问题。
 * 3. 实现“拟合”与“显示”的联动逻辑（拟合必显示）。
 * 4. 收集并返回用户配置的参数。
 */

#include "paramselectdialog.h"
#include "ui_paramselectdialog.h"
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QHBoxLayout>

ParamSelectDialog::ParamSelectDialog(const QList<FitParameter> &params, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ParamSelectDialog),
    m_params(params)
{
    ui->setupUi(this);

    this->setWindowTitle("拟合参数配置");

    // 显式连接信号槽，处理确定和取消操作
    connect(ui->btnOk, &QPushButton::clicked, this, &ParamSelectDialog::onConfirm);
    connect(ui->btnCancel, &QPushButton::clicked, this, &ParamSelectDialog::onCancel);

    // 防止取消按钮截获回车键
    ui->btnCancel->setAutoDefault(false);

    // 初始化表格内容
    initTable();
}

ParamSelectDialog::~ParamSelectDialog()
{
    delete ui;
}

void ParamSelectDialog::initTable()
{
    QStringList headers;
    headers << "显示" << "参数名称" << "当前数值" << "单位" << "拟合变量" << "下限" << "上限";
    ui->tableWidget->setColumnCount(headers.size());
    ui->tableWidget->setHorizontalHeaderLabels(headers);
    ui->tableWidget->setRowCount(m_params.size());

    // 定义复选框的样式表，设置选中状态为蓝色背景，未选中为白色
    // indicator 是复选框的小方框区域
    QString checkBoxStyle =
        "QCheckBox::indicator {"
        "   width: 20px;"
        "   height: 20px;"
        "   border: 1px solid #cccccc;"
        "   border-radius: 3px;"
        "   background-color: white;"
        "}"
        "QCheckBox::indicator:checked {"
        "   background-color: #0078d7;" // 选中时变蓝
        "   border-color: #0078d7;"
        "}"
        "QCheckBox::indicator:hover {"
        "   border-color: #0078d7;"     // 鼠标悬停时边框变蓝
        "}";

    for(int i = 0; i < m_params.size(); ++i) {
        const FitParameter& p = m_params[i];

        // 0. 显示勾选框
        QWidget* pWidgetVis = new QWidget();
        QHBoxLayout* pLayoutVis = new QHBoxLayout(pWidgetVis);
        QCheckBox* chkVis = new QCheckBox();
        chkVis->setChecked(p.isVisible);
        chkVis->setStyleSheet(checkBoxStyle); // 应用自定义蓝色样式
        pLayoutVis->addWidget(chkVis);
        pLayoutVis->setAlignment(Qt::AlignCenter);
        pLayoutVis->setContentsMargins(0,0,0,0);
        ui->tableWidget->setCellWidget(i, 0, pWidgetVis);

        // 1. 参数名称 (只读)
        QString displayNameFull = QString("%1 (%2)").arg(p.displayName).arg(p.name);
        QTableWidgetItem* nameItem = new QTableWidgetItem(displayNameFull);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        nameItem->setData(Qt::UserRole, p.name);
        ui->tableWidget->setItem(i, 1, nameItem);

        // 2. 数值
        QDoubleSpinBox* spinVal = new QDoubleSpinBox();
        spinVal->setRange(-9e9, 9e9); spinVal->setDecimals(6); spinVal->setValue(p.value);
        spinVal->setFrame(false); // 去除边框以融入表格
        ui->tableWidget->setCellWidget(i, 2, spinVal);

        // 3. 单位
        QString dummy, dummy2, dummy3, unitStr;
        FittingParameterChart::getParamDisplayInfo(p.name, dummy, dummy2, dummy3, unitStr);
        if(unitStr == "无因次" || unitStr == "小数") unitStr = "-";
        QTableWidgetItem* unitItem = new QTableWidgetItem(unitStr);
        unitItem->setFlags(unitItem->flags() & ~Qt::ItemIsEditable);
        ui->tableWidget->setItem(i, 3, unitItem);

        // 4. 拟合勾选框
        QWidget* pWidgetFit = new QWidget();
        QHBoxLayout* pLayoutFit = new QHBoxLayout(pWidgetFit);
        QCheckBox* chkFit = new QCheckBox();
        chkFit->setChecked(p.isFit);
        chkFit->setStyleSheet(checkBoxStyle); // 应用自定义蓝色样式
        pLayoutFit->addWidget(chkFit);
        pLayoutFit->setAlignment(Qt::AlignCenter);
        pLayoutFit->setContentsMargins(0,0,0,0);
        ui->tableWidget->setCellWidget(i, 4, pWidgetFit);

        // 约束逻辑：勾选拟合 -> 强制显示
        connect(chkFit, &QCheckBox::checkStateChanged, [chkVis](Qt::CheckState state){
            if (state == Qt::Checked) {
                chkVis->setChecked(true);
                chkVis->setEnabled(false); // 强制显示，不可取消
                // 禁用状态下稍微调低透明度或改变样式以示区别
                chkVis->setStyleSheet("QCheckBox::indicator { width: 20px; height: 20px; border: 1px solid #ccc; border-radius: 3px; background-color: #e0e0e0; } "
                                      "QCheckBox::indicator:checked { background-color: #80bbeb; border-color: #80bbeb; }");
            } else {
                chkVis->setEnabled(true); // 恢复可控
                // 恢复正常蓝色样式
                chkVis->setStyleSheet(
                    "QCheckBox::indicator { width: 20px; height: 20px; border: 1px solid #cccccc; border-radius: 3px; background-color: white; }"
                    "QCheckBox::indicator:checked { background-color: #0078d7; border-color: #0078d7; }"
                    "QCheckBox::indicator:hover { border-color: #0078d7; }"
                    );
            }
        });

        // 初始化状态下的样式处理
        if (p.isFit) {
            chkVis->setChecked(true);
            chkVis->setEnabled(false);
            // 初始化时也应用禁用样式
            chkVis->setStyleSheet("QCheckBox::indicator { width: 20px; height: 20px; border: 1px solid #ccc; border-radius: 3px; background-color: #e0e0e0; } "
                                  "QCheckBox::indicator:checked { background-color: #80bbeb; border-color: #80bbeb; }");
        }

        // 5. 下限
        QDoubleSpinBox* spinMin = new QDoubleSpinBox();
        spinMin->setRange(-9e9, 9e9); spinMin->setDecimals(6); spinMin->setValue(p.min);
        spinMin->setFrame(false);
        ui->tableWidget->setCellWidget(i, 5, spinMin);

        // 6. 上限
        QDoubleSpinBox* spinMax = new QDoubleSpinBox();
        spinMax->setRange(-9e9, 9e9); spinMax->setDecimals(6); spinMax->setValue(p.max);
        spinMax->setFrame(false);
        ui->tableWidget->setCellWidget(i, 6, spinMax);
    }

    ui->tableWidget->resizeColumnsToContents();
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

void ParamSelectDialog::collectData()
{
    for(int i = 0; i < ui->tableWidget->rowCount(); ++i) {
        if(i >= m_params.size()) break;

        // 获取显示状态
        QWidget* wVis = ui->tableWidget->cellWidget(i, 0);
        QCheckBox* chkVis = wVis ? wVis->findChild<QCheckBox*>() : nullptr;
        if(chkVis) m_params[i].isVisible = chkVis->isChecked();

        // 获取数值
        QDoubleSpinBox* spinVal = qobject_cast<QDoubleSpinBox*>(ui->tableWidget->cellWidget(i, 2));
        if(spinVal) m_params[i].value = spinVal->value();

        // 获取拟合选择状态
        QWidget* wFit = ui->tableWidget->cellWidget(i, 4);
        QCheckBox* chkFit = wFit ? wFit->findChild<QCheckBox*>() : nullptr;
        if(chkFit) m_params[i].isFit = chkFit->isChecked();

        // 获取下限
        QDoubleSpinBox* spinMin = qobject_cast<QDoubleSpinBox*>(ui->tableWidget->cellWidget(i, 5));
        if(spinMin) m_params[i].min = spinMin->value();

        // 获取上限
        QDoubleSpinBox* spinMax = qobject_cast<QDoubleSpinBox*>(ui->tableWidget->cellWidget(i, 6));
        if(spinMax) m_params[i].max = spinMax->value();
    }
}

QList<FitParameter> ParamSelectDialog::getUpdatedParams() const
{
    return m_params;
}

// 确定槽函数
void ParamSelectDialog::onConfirm()
{
    collectData();
    accept(); // 关闭对话框并返回 Accepted
}

// 取消槽函数
void ParamSelectDialog::onCancel()
{
    reject(); // 关闭对话框并返回 Rejected
}
