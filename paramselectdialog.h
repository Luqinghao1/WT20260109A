/*
 * 文件名: paramselectdialog.h
 * 文件作用: 拟合参数选择对话框头文件
 * 功能描述:
 * 1. 声明 ParamSelectDialog 类，用于配置模型的拟合参数。
 * 2. 定义内部数据结构和槽函数，支持参数的显示、拟合选择及范围设定。
 */

#ifndef PARAMSELECTDIALOG_H
#define PARAMSELECTDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include "fittingparameterchart.h"

namespace Ui {
class ParamSelectDialog;
}

// ===========================================================================
// 类名：ParamSelectDialog
// 作用：拟合参数配置弹窗
// 功能：
// 1. 提供一个大表格，列出所有模型参数
// 2. 允许用户勾选参数是否在主界面显示 (isVisible)
// 3. 允许用户勾选参数是否参与自动拟合 (isFit)
// 4. 设置每个参数的上下限范围 (Min/Max)
// 5. 内部维护逻辑：如果勾选了拟合，则强制勾选显示
// ===========================================================================

class ParamSelectDialog : public QDialog
{
    Q_OBJECT

public:
    // 构造函数：传入当前参数列表
    explicit ParamSelectDialog(const QList<FitParameter>& params, QWidget *parent = nullptr);
    ~ParamSelectDialog();

    // 获取用户修改后的参数列表
    QList<FitParameter> getUpdatedParams() const;

private:
    Ui::ParamSelectDialog *ui;

    // 暂存的参数列表副本，用于在对话框中编辑
    QList<FitParameter> m_params;

    // 初始化表格视图，填充数据并创建控件
    void initTable();
    // 收集表格中的用户输入数据更新到 m_params
    void collectData();

private slots:
    // 确认按钮槽函数
    void onConfirm();
    // 取消按钮槽函数
    void onCancel();
};

#endif // PARAMSELECTDIALOG_H
