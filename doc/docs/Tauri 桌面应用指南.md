# Tauri 桌面应用指南

本文档介绍如何使用 Tauri v2 将 Lv-00 web-gui 封装为原生桌面应用。

## 目录

- [前置条件](#前置条件)
- [快速开始](#快速开始)
- [开发模式](#开发模式)
- [生产构建](#生产构建)
- [平台特定说明](#平台特定说明)
- [可用的 Tauri 命令](#可用的-tauri-命令)
- [常见问题排查](#常见问题排查)

---

## 前置条件

### 1. 安装 Rust 工具链

访问 [https://rustup.rs/](https://rustup.rs/) 安装 Rust：

```bash
# Windows (PowerShell)
winget install Rustlang.Rustup

# 或使用官方安装器
# 下载 https://rustup.rs/ 并运行
```

安装完成后验证：

```bash
rustc --version
cargo --version
```

### 2. 系统依赖

#### Windows

- **Microsoft Visual Studio C++ Build Tools**：安装 [Visual Studio Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/)，选择"使用 C++ 的桌面开发"工作负载。
- **WebView2 Runtime**：Windows 10/11 通常已预装。如未安装，访问 [https://developer.microsoft.com/en-us/microsoft-edge/webview2/](https://developer.microsoft.com/en-us/microsoft-edge/webview2/)。

#### macOS

```bash
xcode-select --install
```

#### Linux (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install libwebkit2gtk-4.1-dev \
    build-essential \
    curl \
    wget \
    file \
    libxdo-dev \
    libssl-dev \
    libayatana-appindicator3-dev \
    librsvg2-dev
```

> **注意**：不同 Linux 发行版的包名可能不同，请参考 [Tauri 官方文档 - Linux 依赖](https://v2.tauri.app/start/prerequisites/#linux)。

### 3. Node.js 依赖

```bash
cd web-gui
npm install
```

---

## 快速开始

```bash
cd web-gui

# 安装 Node.js 依赖（包含 @tauri-apps/cli 和 @tauri-apps/api）
npm install

# 开发模式启动
npm run tauri:dev

# 生产构建
npm run tauri:build
```

---

## 开发模式

```bash
npm run tauri:dev
```

此命令会：
1. 启动 Vite 开发服务器（`http://localhost:5173`）
2. 编译 Rust 后端
3. 打开桌面窗口并连接到 Vite 开发服务器
4. 启用热重载（前端代码修改会自动刷新，Rust 代码修改会自动重新编译）

### 开发模式特性

- **DevTools**：自动打开开发者工具（F12 或右键 -> 检查）
- **热重载**：前端代码修改即时生效
- **宽松 CSP**：开发阶段允许所有内容安全策略

---

## 生产构建

```bash
npm run tauri:build
```

此命令会：
1. 执行 `npm run build` 构建前端（TypeScript 编译 + Vite 打包）
2. 编译 Rust 后端（release 模式）
3. 生成平台特定的安装包

### 构建产物

构建完成后，安装包位于：

| 平台 | 路径 |
|------|------|
| Windows | `src-tauri/target/release/bundle/msi/` 和 `nsis/` |
| macOS | `src-tauri/target/release/bundle/dmg/` 和 `macos/` |
| Linux | `src-tauri/target/release/bundle/deb/` 和 `appimage/` |

---

## 平台特定说明

### Windows

- **安装包格式**：MSI 和 NSIS 两种格式
- **代码签名**：未签名的应用会触发 Windows SmartScreen 警告，用户需要点击"更多信息" -> "仍要运行"
- **管理员权限**：默认不需要管理员权限

### macOS

- **安装包格式**：DMG
- **代码签名**：未签名的应用需要用户在"系统偏好设置" -> "安全性与隐私"中手动允许
- **Apple Silicon**：Tauri 默认构建通用二进制（Universal Binary），同时支持 Intel 和 Apple Silicon

### Linux

- **安装包格式**：DEB（Debian/Ubuntu）、RPM（Fedora）、AppImage（通用）
- **AppImage**：无需安装，直接运行，推荐用于分发
- **系统托盘**：需要 `libayatana-appindicator3-dev`

---

## 可用的 Tauri 命令

Rust 后端注册了以下命令，可在前端通过 `@tauri-apps/api` 调用：

### 窗口管理

```typescript
import { invoke } from '@tauri-apps/api/core';

// 最小化窗口
await invoke('minimize_window');

// 切换最大化
const isMaximized: boolean = await invoke('toggle_maximize');

// 切换全屏
const isFullscreen: boolean = await invoke('toggle_fullscreen');

// 关闭窗口
await invoke('close_window');

// 设置窗口标题
await invoke('set_title', { title: '新标题' });

// 获取窗口状态
const state = await invoke('get_window_state');
// 返回: { maximized, fullscreen, minimized, width, height, x, y }
```

### 文件操作

```typescript
import { invoke } from '@tauri-apps/api/core';

// 打开文件对话框
const files: string[] | null = await invoke('open_file_dialog', {
  title: '选择几何文件',
  multiple: true,
  filters: [{ name: 'Lv-00 Files', extensions: ['lv00', 'geo', 'json'] }]
});

// 保存文件对话框
const filePath: string | null = await invoke('save_file_dialog', {
  title: '保存几何文件',
  defaultName: 'geometry.lv00',
  filters: [{ name: 'Lv-00 Files', extensions: ['lv00'] }]
});
```

---

## 常见问题排查

### Rust 编译错误

**问题**：`error: linker 'link.exe' not found`

**解决**：安装 Visual Studio Build Tools，确保勾选"使用 C++ 的桌面开发"工作负载。

**问题**：`error: could not find native static library ...`

**解决**：确保系统依赖已正确安装。Windows 上重新运行 Visual Studio Installer 并修复安装。

### 构建速度慢

首次构建需要编译所有 Rust 依赖（包括 Tauri 本身），可能需要 10-30 分钟。后续增量编译会快很多。

**加速建议**：
- 使用 SSD 硬盘
- 确保有足够的内存（建议 8GB+）
- 在 `src-tauri/Cargo.toml` 中使用 `cargo build -j N` 并行编译（N 为 CPU 核心数）

### 前端无法连接

**问题**：桌面窗口打开后显示空白或连接错误

**解决**：
1. 确认 Vite 开发服务器正在运行（`http://localhost:5173`）
2. 检查 `src-tauri/tauri.conf.json` 中的 `devUrl` 配置
3. 确认没有其他程序占用 5173 端口

### WebView2 问题 (Windows)

**问题**：应用启动后崩溃或显示 WebView2 错误

**解决**：
1. 安装/更新 WebView2 Runtime：https://developer.microsoft.com/en-us/microsoft-edge/webview2/
2. 清除 WebView2 缓存：删除 `%LOCALAPPDATA%\Microsoft\EdgeWebView2\`

### 图标缺失

**问题**：构建时提示找不到图标文件

**解决**：参考 `src-tauri/icons/README.md`，使用 `npx tauri icon` 命令从源图片生成所有尺寸的图标。

### 与现有 Vite 构建流程的兼容性

Tauri 是可选的附加功能，不会影响现有的 Vite 开发流程：

- `npm run dev` -- 纯 Vite 开发模式（浏览器）
- `npm run build` -- 纯 Vite 生产构建
- `npm run tauri:dev` -- Tauri 桌面开发模式
- `npm run tauri:build` -- Tauri 桌面生产构建

即使未安装 Rust，前三个命令仍然可以正常工作。

---

## 项目结构

```
web-gui/
  src-tauri/              # Tauri 后端（Rust）
    Cargo.toml            # Rust 依赖配置
    tauri.conf.json       # Tauri 应用配置
    build.rs              # 构建脚本
    src/
      main.rs             # Rust 入口文件
    icons/                # 应用图标
      README.md           # 图标生成说明
  src/                    # 前端源代码（React + TypeScript）
  package.json            # Node.js 依赖和脚本
  vite.config.ts          # Vite 配置
  index.html              # 入口 HTML
```

---

## 参考链接

- [Tauri v2 官方文档](https://v2.tauri.app/)
- [Tauri v2 迁移指南](https://v2.tauri.app/start/migrate/from-tauri-1/)
- [Tauri v2 API 参考](https://v2.tauri.app/reference/)
- [Rust 官方文档](https://doc.rust-lang.org/)
