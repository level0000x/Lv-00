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
    EditorController* editor_;
    SceneController* scene_;

    // 模态 Dock
    QDockWidget* canvasDock_;       // M1 几何画布
    QDockWidget* textDock_;         // M2 文本编辑器
    QDockWidget* tableDock_;        // M3 表格面板
    QDockWidget* treeDock_;         // M4 依赖树
    QDockWidget* terminalDock_;     // M5 终端
    QDockWidget* topologyDock_;     // M6 拓扑视图

    // 额外 Dock
    QDockWidget* monitorDock_;      // 并发监控 (QWebEngineView)
    QDockWidget* propertyDock_;     // 属性面板

    // 同步定时器
    QTimer* syncTimer_;

    // 画布部件
    CanvasWidget* canvas_;
};
*/
