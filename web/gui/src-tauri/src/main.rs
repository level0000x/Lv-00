// Prevents additional console window on Windows in release, DO NOT REMOVE!!
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

//! # Lv-00 桌面应用 - Tauri 后端入口
//!
//! 本模块是 Lv-00 桌面应用的 Rust 后端主入口，基于 Tauri v2 框架构建。
//! 负责窗口管理、系统菜单、文件对话框等桌面原生功能的实现。
//!
//! ## 功能概述
//!
//! - **窗口管理**：最小化、最大化/还原、全屏切换、关闭窗口、设置标题
//! - **系统菜单**：文件、编辑、视图、帮助四个菜单栏，各含实际子菜单项
//! - **文件对话框**：打开文件、保存文件的原生对话框
//! - **窗口状态查询**：获取窗口的位置、大小、最大化/全屏等状态
//!
//! ## 注册的 Tauri 命令
//!
//! | 命令名 | 功能说明 |
//! |--------|----------|
//! | `minimize_window` | 最小化当前窗口 |
//! | `toggle_maximize` | 最大化/还原当前窗口 |
//! | `toggle_fullscreen` | 切换全屏模式 |
//! | `close_window` | 关闭当前窗口 |
//! | `set_title` | 设置窗口标题 |
//! | `open_file_dialog` | 打开文件对话框 |
//! | `save_file_dialog` | 保存文件对话框 |
//! | `get_window_state` | 获取窗口状态信息 |
//!
//! ## 菜单事件
//!
//! 系统菜单项点击后通过 `app.emit()` 发送事件，前端可通过 `listen()` 监听：
//!
//! - `menu://file/new` — 新建文件
//! - `menu://file/open` — 打开文件
//! - `menu://file/save` — 保存文件
//! - `menu://file/exit` — 退出应用
//! - `menu://edit/undo` — 撤销
//! - `menu://edit/redo` — 重做
//! - `menu://edit/select-all` — 全选
//! - `menu://view/fullscreen` — 全屏切换
//! - `menu://view/devtools` — 开发者工具（仅 debug 模式）
//! - `menu://help/about` — 关于
//!
//! ## 使用示例（前端调用）
//!
//! ```javascript
//! import { invoke } from '@tauri-apps/api/core';
//! import { listen } from '@tauri-apps/api/event';
//!
//! // 调用命令
//! await invoke('toggle_fullscreen');
//!
//! // 监听菜单事件
//! await listen('menu://file/new', () => {
//!     console.log('用户点击了新建');
//! });
//! ```

use tauri::{
    command,
    menu::{Menu, MenuItem, PredefinedMenuItem, Submenu},
    Manager,
};

/// 最小化当前窗口
#[command]
async fn minimize_window(window: tauri::Window) -> Result<(), String> {
    window.minimize().map_err(|e| e.to_string())
}

/// 最大化/还原当前窗口
///
/// 返回 `Ok(true)` 表示已最大化，`Ok(false)` 表示已还原
#[command]
async fn toggle_maximize(window: tauri::Window) -> Result<bool, String> {
    if window.is_maximized().map_err(|e| e.to_string())? {
        window.unmaximize().map_err(|e| e.to_string())?;
        Ok(false)
    } else {
        window.maximize().map_err(|e| e.to_string())?;
        Ok(true)
    }
}

/// 切换全屏模式
///
/// 返回 `Ok(true)` 表示已进入全屏，`Ok(false)` 表示已退出全屏
#[command]
async fn toggle_fullscreen(window: tauri::Window) -> Result<bool, String> {
    if window.is_fullscreen().map_err(|e| e.to_string())? {
        window.set_fullscreen(false).map_err(|e| e.to_string())?;
        Ok(false)
    } else {
        window.set_fullscreen(true).map_err(|e| e.to_string())?;
        Ok(true)
    }
}

/// 关闭当前窗口
#[command]
async fn close_window(window: tauri::Window) -> Result<(), String> {
    window.close().map_err(|e| e.to_string())
}

/// 设置窗口标题
#[command]
async fn set_title(window: tauri::Window, title: String) -> Result<(), String> {
    window
        .set_title(&title)
        .map_err(|e| e.to_string())
}

/// 打开文件对话框，返回选中的文件路径
///
/// # 参数
///
/// - `title` — 对话框标题，默认为"打开文件"
/// - `filters` — 文件类型过滤器列表
/// - `multiple` — 是否允许多选，默认为 `false`
///
/// # 返回值
///
/// 返回选中的文件路径列表，若用户取消则返回 `None`
///
/// # 线程安全
///
/// 文件对话框是阻塞操作，通过 `tokio::task::spawn_blocking` 将阻塞调用
/// 移至专用线程池执行，避免阻塞 Tauri 的异步运行时主线程。
#[command]
async fn open_file_dialog(
    app: tauri::AppHandle,
    title: Option<String>,
    filters: Option<Vec<tauri_plugin_dialog::FileFilter>>,
    multiple: Option<bool>,
) -> Result<Option<Vec<String>>, String> {
    use tauri_plugin_dialog::DialogExt;

    let multiple = multiple.unwrap_or(false);

    // 将阻塞的文件对话框操作移至专用线程池，避免阻塞异步运行时
    let file_path = tokio::task::spawn_blocking(move || {
        let dialog = app
            .dialog()
            .file()
            .set_title(title.as_deref().unwrap_or("打开文件"))
            .add_filters(filters.unwrap_or_default());

        if multiple {
            dialog
                .blocking_pick_files()
                .map(|paths| paths.into_iter().map(|p| p.to_string()).collect())
        } else {
            dialog
                .blocking_pick_file()
                .map(|p| vec![p.to_string()])
        }
    })
    .await
    .map_err(|e| e.to_string())?;

    Ok(file_path)
}

/// 保存文件对话框，返回选择的文件路径
///
/// # 参数
///
/// - `title` — 对话框标题，默认为"保存文件"
/// - `default_name` — 默认文件名，默认为"untitled"
/// - `filters` — 文件类型过滤器列表
///
/// # 返回值
///
/// 返回选择的保存路径，若用户取消则返回 `None`
///
/// # 线程安全
///
/// 保存文件对话框是阻塞操作，通过 `tokio::task::spawn_blocking` 将阻塞调用
/// 移至专用线程池执行，避免阻塞 Tauri 的异步运行时主线程。
#[command]
async fn save_file_dialog(
    app: tauri::AppHandle,
    title: Option<String>,
    default_name: Option<String>,
    filters: Option<Vec<tauri_plugin_dialog::FileFilter>>,
) -> Result<Option<String>, String> {
    use tauri_plugin_dialog::DialogExt;

    // 将阻塞的文件对话框操作移至专用线程池，避免阻塞异步运行时
    let file_path = tokio::task::spawn_blocking(move || {
        app.dialog()
            .file()
            .set_title(title.as_deref().unwrap_or("保存文件"))
            .set_file_name(default_name.as_deref().unwrap_or("untitled"))
            .add_filters(filters.unwrap_or_default())
            .blocking_save_file()
            .map(|p| p.to_string())
    })
    .await
    .map_err(|e| e.to_string())?;

    Ok(file_path)
}

/// 获取窗口当前状态信息
///
/// 返回包含窗口位置、大小、最大化/全屏/最小化状态的 JSON 对象。
/// 该命令可供前端用于状态持久化或界面同步。
#[allow(dead_code)]
#[command]
async fn get_window_state(window: tauri::Window) -> Result<serde_json::Value, String> {
    let is_maximized = window.is_maximized().map_err(|e| e.to_string())?;
    let is_fullscreen = window.is_fullscreen().map_err(|e| e.to_string())?;
    let is_minimized = window.is_minimized().map_err(|e| e.to_string())?;
    let size = window.outer_size().map_err(|e| e.to_string())?;
    let position = window.outer_position().map_err(|e| e.to_string())?;

    Ok(serde_json::json!({
        "maximized": is_maximized,
        "fullscreen": is_fullscreen,
        "minimized": is_minimized,
        "width": size.width,
        "height": size.height,
        "x": position.x,
        "y": position.y
    }))
}

/// 构建系统菜单
///
/// 创建包含文件、编辑、视图、帮助四个子菜单的系统菜单栏。
/// 菜单项点击后通过 `app.emit()` 发送事件，由前端监听处理。
fn build_system_menu(app: &tauri::AppHandle) -> Result<Menu<tauri::Wry>, tauri::Error> {
    // ==================== 文件菜单 ====================
    let file_new = MenuItem::with_id(app, "file_new", "新建(&N)", true, None::<&str>)?;
    let file_open = MenuItem::with_id(app, "file_open", "打开文件(&O)...", true, None::<&str>)?;
    let file_save = MenuItem::with_id(app, "file_save", "保存文件(&S)", true, None::<&str>)?;
    let file_separator = PredefinedMenuItem::separator(app)?;
    let file_exit = MenuItem::with_id(app, "file_exit", "退出(&Q)", true, None::<&str>)?;

    let file_submenu = Submenu::with_items(
        app,
        "文件(&F)",
        true,
        &[&file_new, &file_open, &file_save, &file_separator, &file_exit],
    )?;

    // ==================== 编辑菜单 ====================
    let edit_undo = MenuItem::with_id(app, "edit_undo", "撤销(&Z)", true, None::<&str>)?;
    let edit_redo = MenuItem::with_id(app, "edit_redo", "重做(&Y)", true, None::<&str>)?;
    let edit_separator = PredefinedMenuItem::separator(app)?;
    let edit_select_all =
        MenuItem::with_id(app, "edit_select_all", "全选(&A)", true, None::<&str>)?;

    let edit_submenu = Submenu::with_items(
        app,
        "编辑(&E)",
        true,
        &[&edit_undo, &edit_redo, &edit_separator, &edit_select_all],
    )?;

    // ==================== 视图菜单 ====================
    let view_fullscreen =
        MenuItem::with_id(app, "view_fullscreen", "全屏切换(&F)", true, None::<&str>)?;

    // 开发者工具仅在 debug 模式下显示
    #[cfg(debug_assertions)]
    let view_items: Vec<&dyn tauri::menu::IsMenuItem<tauri::Wry>> = {
        let view_separator = PredefinedMenuItem::separator(app)?;
        let view_devtools =
            MenuItem::with_id(app, "view_devtools", "开发者工具(&D)", true, None::<&str>)?;
        vec![
            &view_fullscreen,
            &view_separator as &dyn tauri::menu::IsMenuItem<tauri::Wry>,
            &view_devtools,
        ]
    };

    #[cfg(not(debug_assertions))]
    let view_items: Vec<&dyn tauri::menu::IsMenuItem<tauri::Wry>> = {
        vec![&view_fullscreen]
    };

    let view_submenu = Submenu::with_items(app, "视图(&V)", true, &view_items)?;

    // ==================== 帮助菜单 ====================
    let help_about = MenuItem::with_id(app, "help_about", "关于(&A)", true, None::<&str>)?;
    let help_submenu = Submenu::with_items(app, "帮助(&H)", true, &[&help_about])?;

    // ==================== 组装菜单栏 ====================
    let menu = Menu::with_items(
        app,
        &[&file_submenu, &edit_submenu, &view_submenu, &help_submenu],
    )?;

    Ok(menu)
}

/// 注册菜单事件监听器
///
/// 为所有菜单项绑定点击事件，通过 `app.emit()` 向前端发送对应事件。
/// 前端可通过 `@tauri-apps/api/event` 的 `listen()` 方法监听这些事件。
fn setup_menu_events(app: &tauri::AppHandle) -> Result<(), tauri::Error> {
    // 文件菜单事件
    app.on_menu_event(|app_handle, event| {
        if let Some(id) = event.id.as_ref() {
            let event_name = match id.as_str() {
                // 文件菜单
                "file_new" => Some("menu://file/new"),
                "file_open" => Some("menu://file/open"),
                "file_save" => Some("menu://file/save"),
                "file_exit" => {
                    // 退出应用：直接关闭主窗口
                    if let Some(window) = app_handle.get_webview_window("main") {
                        let _ = window.close();
                    }
                    None
                }
                // 编辑菜单
                "edit_undo" => Some("menu://edit/undo"),
                "edit_redo" => Some("menu://edit/redo"),
                "edit_select_all" => Some("menu://edit/select-all"),
                // 视图菜单
                "view_fullscreen" => {
                    // 全屏切换：直接调用窗口 API
                    if let Some(window) = app_handle.get_webview_window("main") {
                        let is_fullscreen = window.is_fullscreen().unwrap_or(false);
                        let _ = window.set_fullscreen(!is_fullscreen);
                    }
                    None
                }
                "view_devtools" => {
                    // 开发者工具：切换 devtools 面板
                    if let Some(window) = app_handle.get_webview_window("main") {
                        #[cfg(debug_assertions)]
                        window.open_devtools();
                    }
                    None
                }
                // 帮助菜单
                "help_about" => Some("menu://help/about"),
                // 忽略未识别的菜单项
                _ => None,
            };

            // 向前端发送事件
            if let Some(name) = event_name {
                let _ = app_handle.emit(name, ());
            }
        }
    });

    Ok(())
}

fn main() {
    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_fs::init())
        .invoke_handler(tauri::generate_handler![
            minimize_window,
            toggle_maximize,
            toggle_fullscreen,
            close_window,
            set_title,
            open_file_dialog,
            save_file_dialog,
            get_window_state,
        ])
        .setup(|app| {
            // 构建并设置系统菜单
            let menu = build_system_menu(app.handle())?;
            app.set_menu(menu)?;

            // 注册菜单事件监听
            setup_menu_events(app.handle())?;

            // Debug 模式下自动打开开发者工具
            #[cfg(debug_assertions)]
            {
                let window = app.get_webview_window("main").unwrap();
                window.open_devtools();
            }

            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
