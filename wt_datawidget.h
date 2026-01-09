/*
 * 文件名: wt_datawidget.h
 * 文件作用: 数据编辑器主窗口头文件
 * 功能描述:
 * 1. 定义数据编辑器的主界面类 WT_DataWidget (原 DataEditorWidget)。
 * 2. 负责管理多个 DataSingleSheet 实例，支持多文件同时打开。
 * 3. 协调顶部工具栏与当前活动页签的交互。
 * 4. 负责将所有页签数据同步保存到项目文件中。
 */

#ifndef WT_DATAWIDGET_H
#define WT_DATAWIDGET_H

#include <QWidget>
#include <QStandardItemModel>
#include <QJsonArray>
#include "datasinglesheet.h" // 包含单页类

namespace Ui {
class WT_DataWidget;
}

class WT_DataWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WT_DataWidget(QWidget *parent = nullptr);
    ~WT_DataWidget();

    void clearAllData();
    // 从项目参数恢复数据
    void loadFromProjectData();

    // 获取当前活动页的模型（兼容旧接口）
    QStandardItemModel* getDataModel() const;
    void loadData(const QString& filePath, const QString& fileType = "auto");
    QString getCurrentFileName() const;
    bool hasData() const;

signals:
    void dataChanged();
    void fileChanged(const QString& filePath, const QString& fileType);

private slots:
    // 文件操作
    void onOpenFile();
    void onSave();
    void onExportExcel();

    // 工具栏操作（分发给当前页签）
    void onDefineColumns();
    void onTimeConvert();
    void onPressureDropCalc();
    void onCalcPwf();
    void onHighlightErrors();

    // 状态
    void onTabChanged(int index);
    void onTabCloseRequested(int index);
    void onSheetDataChanged();

private:
    Ui::WT_DataWidget *ui;

    void initUI();
    void setupConnections();
    void updateButtonsState();

    // 辅助函数：创建新页签
    void createNewTab(const QString& filePath, const DataImportSettings& settings);
    // 辅助函数：获取当前活动页签
    DataSingleSheet* currentSheet() const;
};

#endif // WT_DATAWIDGET_H
