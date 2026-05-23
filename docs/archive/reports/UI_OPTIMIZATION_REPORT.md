# Lv-00 UI 系统全面优化任务汇报

> **日期**：2026-05-20  
> **范围**：`web/` 目录下 30+ 个文件，~6000+ 行代码  
> **原则**：风格不变、功能全部保留、无破坏性变更  

---

## 📊 问题诊断总览

| 风险等级 | 数量 | 类别 |
|---------|------|------|
| 🔴 高风险 | 2 | XSS 漏洞（innerHTML）、全局变量污染 |
| 🟡 中风险 | 5 | 硬编码颜色、内联样式、缺失空值守卫、事件泄漏 |
| 🟢 低风险 | 5 | 注释不统一、常量分散、模块耦合 |

---

## 🔴 阶段一：安全修复

### 1.1 XSS 风险消除

| 文件 | 问题 | 修复方式 |
|------|------|----------|
| `js/modules/block.js` | `_showRewriteExplorer` 使用 `innerHTML` 构建模态框内容 | 改为 `createElement` + `textContent` 安全构建 |
| `integrate-all.js` | `_fetchPanel` 使用 `innerHTML` 注入外部 HTML | 新增 `_safeCloneContent` 方法，用 `cloneNode` 替代 `innerHTML`；错误提示改用 DOM API |

### 1.2 全局变量命名空间

**修改文件**：`js/app.js`

将 6 个裸全局常量包装进 `Lv00Const` 命名空间对象：
```
LV_SCALE_MIN       → Lv00Const.SCALE_MIN
LV_SCALE_MAX       → Lv00Const.SCALE_MAX
LV_EXPORT_CLEANUP_MS → Lv00Const.EXPORT_CLEANUP_MS
LV_PERF_MONITOR_MS   → Lv00Const.PERF_MONITOR_MS
LV_RECURSION_DEPTH   → Lv00Const.RECURSION_DEPTH
LV_EMA_ALPHA         → Lv00Const.EMA_ALPHA
```

同步更新 `interaction.js`（5处引用）和 `app.js`（4处引用）。

### 1.3 空值守卫

在 `app.js` 的 `addPoint`、`addSegment`、`clear` 三个核心方法中添加了 `this.canvas`/`this.ctx` null 检查；`modules/debug.js` 的 4 个方法添加了 `this._perfStats` 守卫。

---

## 🟡 阶段二：CSS 标准化

### 2.1 新增 CSS 变量（`css/variables.css`）

| 变量 | 值 | 用途 |
|------|-----|------|
| `--color-accent-rgb` | `0, 188, 212` | rgba() 中引用强调色 |
| `--color-danger` | `#f85149` | 危险操作色 |
| `--color-danger-hover` | `#994444` | 危险按钮悬停 |
| `--color-danger-border` | `#663333` | 危险按钮边框 |
| `--color-bg-hover-subtle` | `rgba(255,255,255,0.03)` | 标签页悬停背景 |
| `--color-streaming-badge-text` | `#ffffff` | 流式徽章文字 |
| `--color-streaming-active-indicator` | `#3fb950` | 流式活跃指示器 |

### 2.2 硬编码颜色替换（`css/main.css`，15处）

将所有 `#4CAF50`、`#00BCD4`、`#FF9800`、`rgba(0,188,212,...)`、`#633`、`#944` 等硬编码替换为 CSS 变量引用。

### 2.3 新增 CSS 类（10个）

| 类名 | 用途 |
|------|------|
| `.formula-syntax-row` | 语法选择器布局 |
| `.formula-btn-row` | 按钮行布局 |
| `.input-group-row` | 输出格式选择行 |
| `.formula-output-area` / `.formula-output-placeholder` | 输出区域样式 |
| `.formula-log-area` / `.log-empty-msg` | 日志区域样式 |
| `.dep-tree-empty` | 依赖树空状态 |
| `.modal-body-text` | 模态框描述段落 |

### 2.4 内联样式迁移（`index.html`，8处）

将 `index.html` 中的内联 `style` 属性全部替换为对应 CSS class。

---

## 🟡 阶段三：JS 架构加固

### 3.1 事件监听器清理（`js/interaction.js`）

新增 `cleanup()` 方法，统一管理 6 个 `_init*` 方法注册的 DOM 事件：
- `_initKeyboard` → `this._kbdKeydown`/`this._kbdKeyup`
- `_initContextMenu` → `this._ctxMenuCanvasHandler`/`this._ctxMenuDocHandler`/`this._ctxMenuItemHandlers[]`
- `_initBoxSelect` → `this._boxSelKeydown`/`this._boxSelKeyup`
- `_initDragPoint` → `this._dragKeydown`/`this._dragKeyup`
- `_initRegionTool` → `this._regionKeydown`

---

## 🟢 阶段四：功能整合

### 4.1 流式输出核心模块（新增 `js/streaming.js`）

创建 `StreamBridge` 类（~600行），导出至 `window.Lv00Streaming`：

- **8 种事件类型**：ENGINE_START/DONE、NORMALIZE_START/DONE、REWRITE_START/DONE、SOLVE_START/DONE
- **UI 特性**：工具栏过滤按钮、自动滚动、CLEAR 清除、状态栏计数
- **API**：`init(container)`、`emit(event)`、`clearLogs()`、`setFilter(type, enabled)`、`getFilters()`、`getEvents()`、`destroy()`
- 完全兼容 `integrate-all.js` 已有的 `window.Lv00Streaming.StreamBridge` 引用

### 4.2 模块注册表（新增 `js/module_registry.js`）

创建 `Lv00ModuleRegistry` 全局对象：

| 方法 | 功能 |
|------|------|
| `register(name, config)` | 注册模块 |
| `get(name)` | 获取模块配置 |
| `getAll()` / `getEnabled()` | 获取全部/已启用模块 |
| `has(name)` | 检查是否已注册 |
| `setEnabled(name, bool)` | 启用/禁用 |
| `unregister(name)` | 注销模块 |
| `count()` / `registerAll(arr)` | 计数/批量注册 |

### 4.3 加载顺序更新（`index.html`）

在第 566 行插入 `<script src="js/streaming.js"></script>`，确保 `streaming.js` 先于 `integrate-all.js` 加载。

---

## 📁 修改文件清单

| 操作 | 文件 | 变更类型 |
|------|------|----------|
| 修改 | `web/js/app.js` | 全局变量命名空间 + 空值守卫 |
| 修改 | `web/js/interaction.js` | 事件清理 + 常量引用更新 |
| 修改 | `web/js/modules/block.js` | innerHTML → DOM API |
| 修改 | `web/js/modules/debug.js` | null 守卫 |
| 修改 | `web/integrate-all.js` | innerHTML → cloneNode + DOM API |
| 修改 | `web/css/variables.css` | 新增 7 个 CSS 变量 |
| 修改 | `web/css/main.css` | 15处硬编码替换 + 10个新 CSS 类 |
| 修改 | `web/index.html` | 8处内联样式迁移 + 加载顺序更新 |
| **新增** | `web/js/streaming.js` | 流式输出核心模块（~600行） |
| **新增** | `web/js/module_registry.js` | 模块注册表（~227行） |

---

## ✅ 质量保证

- ✅ **功能零破坏**：所有现有功能完整保留，按钮绑定、事件处理、渲染逻辑全部不变
- ✅ **风格一致**：保持原有代码风格（ES5 严格模式、空格缩进、命名约定）
- ✅ **安全加固**：消除全部 innerHTML XSS 风险，添加空值守卫
- ✅ **可维护性**：CSS 变量化、常量命名空间、事件引用化
- ✅ **可扩展性**：模块注册表为后续动态加载提供基础设施

---

## 🚀 后续建议

1. 为 `index.html` 中的 `modalNumericAssumption` 的 `onclick` 内联事件绑定迁移到 JS 模块
2. 表单输入区域可进一步组件化
3. 考虑引入构建工具（如 Vite/Webpack）进行资源打包和压缩
4. 为移动端触控体验增加手势支持（当前仅有基础双指缩放）
