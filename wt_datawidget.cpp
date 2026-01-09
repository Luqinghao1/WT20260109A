/*
 * 文件名: wt_datawidget.cpp
 * 文件作用: 数据编辑器主窗口实现文件 (原 DataEditorWidget)
 * 功能描述:
 * 1. 管理 QTabWidget，支持多标签页显示多份数据。
 * 2. 实现了多文件同时打开的功能。
 * 3. 实现了数据的同步保存。
 * 4. 负责将工具栏的点击事件转发给当前活动的 DataSingleSheet。
 */

#include "wt_datawidget.h"
#include "ui_wt_datawidget.h"
#include "modelparameter.h"
#include "dataimportdialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

WT_DataWidget::WT_DataWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WT_DataWidget)
{
    ui->setupUi(this);
    initUI();
    setupConnections();
}

WT_DataWidget::~WT_DataWidget()
{
    delete ui;
}

void WT_DataWidget::initUI()
{
    updateButtonsState();
}

void WT_DataWidget::setupConnections()
{
    connect(ui->btnOpenFile, &QPushButton::clicked, this, &WT_DataWidget::onOpenFile);
    connect(ui->btnSave, &QPushButton::clicked, this, &WT_DataWidget::onSave);
    connect(ui->btnExport, &QPushButton::clicked, this, &WT_DataWidget::onExportExcel);

    // 将工具栏按钮连接到本类的槽函数，再由槽函数转发给 CurrentSheet
    connect(ui->btnDefineColumns, &QPushButton::clicked, this, &WT_DataWidget::onDefineColumns);
    connect(ui->btnTimeConvert, &QPushButton::clicked, this, &WT_DataWidget::onTimeConvert);
    connect(ui->btnPressureDropCalc, &QPushButton::clicked, this, &WT_DataWidget::onPressureDropCalc);
    connect(ui->btnCalcPwf, &QPushButton::clicked, this, &WT_DataWidget::onCalcPwf);
    connect(ui->btnErrorCheck, &QPushButton::clicked, this, &WT_DataWidget::onHighlightErrors);

    // TabWidget 信号连接
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &WT_DataWidget::onTabChanged);
    connect(ui->tabWidget, &QTabWidget::tabCloseRequested, this, &WT_DataWidget::onTabCloseRequested);
}

DataSingleSheet* WT_DataWidget::currentSheet() const {
    return qobject_cast<DataSingleSheet*>(ui->tabWidget->currentWidget());
}

QStandardItemModel* WT_DataWidget::getDataModel() const {
    if (auto sheet = currentSheet()) {
        return sheet->getDataModel();
    }
    return nullptr;
}

QString WT_DataWidget::getCurrentFileName() const {
    if (auto sheet = currentSheet()) {
        return sheet->getFilePath();
    }
    return QString();
}

bool WT_DataWidget::hasData() const {
    return ui->tabWidget->count() > 0;
}

void WT_DataWidget::updateButtonsState()
{
    bool hasSheet = (ui->tabWidget->count() > 0);
    ui->btnSave->setEnabled(hasSheet);
    ui->btnExport->setEnabled(hasSheet);
    ui->btnDefineColumns->setEnabled(hasSheet);
    ui->btnTimeConvert->setEnabled(hasSheet);
    ui->btnPressureDropCalc->setEnabled(hasSheet);
    ui->btnCalcPwf->setEnabled(hasSheet);
    ui->btnErrorCheck->setEnabled(hasSheet);

    if (auto sheet = currentSheet()) {
        ui->filePathLabel->setText(sheet->getFilePath());
    } else {
        ui->filePathLabel->setText("未加载文件");
    }
}

void WT_DataWidget::onOpenFile()
{
    QString filter = "所有支持文件 (*.csv *.txt *.xlsx *.xls);;Excel (*.xlsx *.xls);;CSV 文件 (*.csv);;文本文件 (*.txt);;所有文件 (*.*)";
    QStringList paths = QFileDialog::getOpenFileNames(this, "打开数据文件", "", filter);
    if (paths.isEmpty()) return;

    for (const QString& path : paths) {
        if (path.endsWith(".json", Qt::CaseInsensitive)) {
            loadData(path, "json");
            return;
        }

        DataImportDialog dlg(path, this);
        if (dlg.exec() == QDialog::Accepted) {
            DataImportSettings settings = dlg.getSettings();
            createNewTab(path, settings);
        }
    }
}

void WT_DataWidget::createNewTab(const QString& filePath, const DataImportSettings& settings) {
    DataSingleSheet* sheet = new DataSingleSheet(this);
    if (sheet->loadData(filePath, settings)) {
        QFileInfo fi(filePath);
        ui->tabWidget->addTab(sheet, fi.fileName());
        ui->tabWidget->setCurrentWidget(sheet);

        connect(sheet, &DataSingleSheet::dataChanged, this, &WT_DataWidget::onSheetDataChanged);

        updateButtonsState();
        emit fileChanged(filePath, "text");
        emit dataChanged();
    } else {
        delete sheet;
        ui->statusLabel->setText("加载文件失败: " + filePath);
    }
}

void WT_DataWidget::loadData(const QString& filePath, const QString& fileType)
{
    if (fileType == "json") {
        return;
    }

    DataImportDialog dlg(filePath, this);
    if (dlg.exec() == QDialog::Accepted) {
        createNewTab(filePath, dlg.getSettings());
    }
}

void WT_DataWidget::onSave() {
    QJsonArray allData;
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        DataSingleSheet* sheet = qobject_cast<DataSingleSheet*>(ui->tabWidget->widget(i));
        if (sheet) {
            allData.append(sheet->saveToJson());
        }
    }

    ModelParameter::instance()->saveTableData(allData);
    ModelParameter::instance()->saveProject();

    QMessageBox::information(this, "保存", "所有标签页数据已同步保存到项目文件。");
}

void WT_DataWidget::loadFromProjectData() {
    clearAllData();
    QJsonArray dataArray = ModelParameter::instance()->getTableData();
    if (dataArray.isEmpty()) {
        ui->statusLabel->setText("无数据");
        return;
    }

    bool isNewFormat = false;
    if (!dataArray.isEmpty()) {
        QJsonValue first = dataArray.first();
        if (first.isObject() && first.toObject().contains("filePath") && first.toObject().contains("data")) {
            isNewFormat = true;
        }
    }

    if (isNewFormat) {
        for (auto val : dataArray) {
            QJsonObject sheetObj = val.toObject();
            DataSingleSheet* sheet = new DataSingleSheet(this);
            sheet->loadFromJson(sheetObj);

            QString path = sheet->getFilePath();
            QFileInfo fi(path);
            ui->tabWidget->addTab(sheet, fi.fileName().isEmpty() ? "恢复数据" : fi.fileName());

            connect(sheet, &DataSingleSheet::dataChanged, this, &WT_DataWidget::onSheetDataChanged);
        }
    } else {
        // 旧版兼容
        DataSingleSheet* sheet = new DataSingleSheet(this);
        QJsonObject sheetObj;
        sheetObj["filePath"] = "Restored Data";
        if (!dataArray.isEmpty() && dataArray.first().toObject().contains("headers")) {
            sheetObj["headers"] = dataArray.first().toObject()["headers"];
        }
        QJsonArray rows;
        for (int i = 1; i < dataArray.size(); ++i) {
            QJsonObject rowObj = dataArray[i].toObject();
            if (rowObj.contains("row_data")) {
                rows.append(rowObj["row_data"]);
            }
        }
        sheetObj["data"] = rows;

        sheet->loadFromJson(sheetObj);
        ui->tabWidget->addTab(sheet, "恢复数据");
        connect(sheet, &DataSingleSheet::dataChanged, this, &WT_DataWidget::onSheetDataChanged);
    }

    updateButtonsState();
    ui->statusLabel->setText("数据已恢复");
}

void WT_DataWidget::clearAllData() {
    ui->tabWidget->clear();
    ui->filePathLabel->setText("未加载文件");
    ui->statusLabel->setText("无数据");
    updateButtonsState();
    emit dataChanged();
}

void WT_DataWidget::onExportExcel() { if (auto s = currentSheet()) s->onExportExcel(); }
void WT_DataWidget::onDefineColumns() { if (auto s = currentSheet()) s->onDefineColumns(); }
void WT_DataWidget::onTimeConvert() { if (auto s = currentSheet()) s->onTimeConvert(); }
void WT_DataWidget::onPressureDropCalc() { if (auto s = currentSheet()) s->onPressureDropCalc(); }
void WT_DataWidget::onCalcPwf() { if (auto s = currentSheet()) s->onCalcPwf(); }
void WT_DataWidget::onHighlightErrors() { if (auto s = currentSheet()) s->onHighlightErrors(); }

void WT_DataWidget::onTabChanged(int index) {
    updateButtonsState();
    emit dataChanged();
}

void WT_DataWidget::onTabCloseRequested(int index) {
    QWidget* widget = ui->tabWidget->widget(index);
    if (widget) {
        ui->tabWidget->removeTab(index);
        delete widget;
    }
    updateButtonsState();
    emit dataChanged();
}

void WT_DataWidget::onSheetDataChanged() {
    if (sender() == currentSheet()) {
        emit dataChanged();
    }
}
