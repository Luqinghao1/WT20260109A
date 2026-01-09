/*
 * main.cpp
 * 文件作用: 应用程序入口
 * 功能描述:
 * 1. 初始化 QApplication 应用程序对象
 * 2. 设置应用程序的全局窗口图标 (PWT.png)
 * 3. 应用全局样式表 (StyleSheet) 以美化界面控件（包含新增的复选框样式）
 * 4. 设置全局调色板以适配不同系统主题的文本颜色
 * 5. 启动主窗口
 */

#include "mainwindow.h"
#include <QApplication>
#include <QStyleFactory>
#include <QMessageBox>
#include <QFileDialog>
#include <QIcon>

int main(int argc, char *argv[])
{
// 解决 HighDpiScaling 在 Qt6 中已废弃的警告
// 只有在 Qt 6.0 之前的版本才需要手动启用，Qt 6 默认启用
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QApplication app(argc, argv);

    // 设置软件全局图标
    app.setWindowIcon(QIcon(":/new/prefix1/Resource/PWT.png"));

    // 设置全局样式，确保所有对话框和消息框的文本都显示为黑色
    // [修改] 新增了 QCheckBox 的样式定义，实现“未选中空白，选中蓝色”的效果
    QString styleSheet = R"(
        /* 全局黑色文字样式 */
        QLabel, QLineEdit, QComboBox, QPushButton, QToolButton,
        QTreeView, QHeaderView, QTableView, QTabBar, QRadioButton,
        QCheckBox, QGroupBox, QMenu, QMenuBar, QStatusBar,
        QListView, QListWidget, QTextEdit, QPlainTextEdit {
            color: black;
        }

        /* 消息框样式 */
        QMessageBox QLabel {
            color: black;
        }

        /* 文件对话框样式 */
        QFileDialog QLabel, QFileDialog QTreeView, QFileDialog QComboBox {
            color: black;
        }

        /* 数据编辑器样式 */
        DataEditorWidget, DataEditorWidget * {
            color: black;
        }

        QTableView {
            alternate-background-color: #f0f0f0;
            background-color: white;
            gridline-color: #d0d0d0;
        }

        QTableView::item {
            color: black;
        }

        QHeaderView::section {
            background-color: #e0e0e0;
            color: black;
            padding: 4px;
            border: 1px solid #c0c0c0;
        }

        QPushButton {
            background-color: #e0e0e0;
            border: 1px solid #c0c0c0;
            padding: 5px 15px;
            min-width: 80px;
        }

        QPushButton:hover {
            background-color: #d0d0d0;
        }

        QPushButton:pressed {
            background-color: #c0c0c0;
        }

        /* =======================================================
           [新增] 全局复选框 (CheckBox) 样式优化
           实现功能：不勾选是空白框，勾选是蓝色实心框
        ======================================================= */
        QCheckBox {
            spacing: 5px; /* 文字和框之间的距离 */
        }

        /* 指示器（勾选框）的基础样式 */
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid #888888; /* 默认灰色边框 */
            border-radius: 2px;       /* 稍微圆角 */
            background-color: white;  /* 未勾选时背景是白色（空白） */
        }

        /* 鼠标悬停在复选框上时，边框变色 */
        QCheckBox::indicator:hover {
            border: 1px solid #0078d7;
        }

        /* 复选框被“勾选”时的样式 */
        QCheckBox::indicator:checked {
            background-color: #0078d7; /* 选中背景变蓝 */
            border: 1px solid #0078d7; /* 选中边框变蓝 */
            /* 这里使用纯色表示选中。如果需要对号图标，可以使用 image: url(...) */
        }

        /* 勾选状态下的悬停 */
        QCheckBox::indicator:checked:hover {
            background-color: #005a9e; /* 深蓝色 */
            border: 1px solid #005a9e;
        }
    )";

    app.setStyleSheet(styleSheet);

    // 设置所有已存在的对话框类的调色板
    QPalette darkTextPalette;
    darkTextPalette.setColor(QPalette::WindowText, Qt::black);
    darkTextPalette.setColor(QPalette::Text, Qt::black);
    darkTextPalette.setColor(QPalette::ButtonText, Qt::black);

    QApplication::setPalette(darkTextPalette);

    MainWindow w;
    w.show();

    return app.exec();
}
