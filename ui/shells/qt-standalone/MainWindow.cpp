// ============================================================
// lv00-ui/shells/qt-standalone — Qt 独立应用外壳 (C++)
// 主窗口 + 6个Dock模态 + 监控内嵌 + 在线程中运行 L2/L3
// 编译: qmake / cmake + Qt 6.5+
// ============================================================

/** @file MainWindow.h */
/*
#pragma once
#include <QMainWindow>
#include <QDockWidget>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QTimer>

class CanvasWidget;
class EditorController;
class SceneController;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onNormalize();
    void onUndo();
    void onRedo();
    void onToggleDock(QDockWidget* dock);
    void onRefreshMonitor();

private:
    void setupMenuBar();
    void setupDockWidgets();
    void setupStatusBar();
    void setupCoreSyncing();
    void saveLayout();
    void restoreLayout();

    // 核心实例
    EditorController* ed