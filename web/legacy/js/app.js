/**
 * Lv-00 Web Visualization Application - 核心构造函数与初始化（v3.0 优化版）
 *
 * 支持 WASM 和纯 JS 双后端，自动检测并选择最优后端。
 *
 * @description Lv-00 几何元语言系统前端主入口，负责初始化 Canvas 渲染、
 *              事件管理、模块面板和撤销/重做等功能。
 *
 * 模块化拆分版本：
 * - utils.js: 通用工具函数（防抖、节流、输入辅助）
 * - render.js: Canvas 渲染方法（绘制网格、坐标轴、点、线段等）
 * - interaction.js: 交互事件处理（鼠标、触摸、键盘、右键菜单等）
 * - ui.js: DOM 操作（状态更新、日志、Toast、搜索、模态框等）
 * - undo.js: 撤销/重做（状态保存、图数据同步）
 * - modules/graph.js: GRAPH 图模块方法
 * - modules/block.js: BLOCK 函数块模块方法（含引擎求解/重写探索器）
 * - modules/proof.js: PROOF 证明模块方法（含 Coq 导出/矛盾证明）
 * - modules/type.js: TYPE 类型模块方法
 * - modules/recurse.js: RECURSE 递归模块方法（含互递归/度量验证）
 * - modules/debug.js: DEBUG 调试模块方法（含调试报告）
 *
 * @module app
 * @author Lv-00 Team
 * @version 3.3.0
 */

'use strict';

// ---- 常量导入（由 constants.js 统一管理）-------------------------------
// Lv00Const 命名空间由 js/constants.js 定义，需在 app.js 之前加载。
// 所有应用级常量通过 Lv00Const 扁平别名访问（如 Lv00Const.SCALE_MIN），
// 也可使用模块级路径（如 Lv00Const.app.SCALE_MIN）。

// ---- 加载步骤提示辅助函数 -----------------------------------------------
/**
 * 更新加载遮罩中的步骤提示状态
 * 将指定步骤标记为已完成（高亮显示），帮助用户了解初始化进度
 * @param {string} stepId - 步骤 li 元素的 ID（如 'loadingStep1'）
 */
function _updateLoadingStep(stepId) {
    var stepEl = document.getElementById(stepId);
    if (stepEl) {
        stepEl.style.opacity = '1';
        stepEl.style.color = '#58a6ff';
    }
}

function Lv00WebApp() {
    // ================================================================
    //  后端与渲染核心
    // ================================================================
    this.backend = null;              // 后端类型: 'wasm' | 'js' | null
    this.wasmModule = null;           // WebAssembly 模块实例
    this.jsBackend = null;            // JS 后端实例（Lv00JSBackend）
    this.graph = null;                // 当前约束图对象

    // ================================================================
    //  Canvas 与几何状态
    // ================================================================
    this.canvas = document.getElementById('geometryCanvas');
    // 容错处理：Canvas 元素不存在时给出明确错误提示，避免后续 getContext 抛出异常
    if (!this.canvas) {
        var errorMsg = '[Lv-00] 致命错误：找不到 Canvas 元素 #geometryCanvas，请检查 index.html 中是否存在该元素';
        console.error(errorMsg);
        // 尝试在页面中显示错误信息
        var loadingEl = document.getElementById('loading');
        if (loadingEl) {
            loadingEl.innerHTML = '<p style="color:#f88;padding:40px;text-align:center;">' +
                'Canvas 元素未找到 / Canvas element not found<br>' +
                '<small style="color:#888;">请检查 index.html 中是否存在 &lt;canvas id="geometryCanvas"&gt;</small></p>';
        }
        throw new Error(errorMsg);
    }
    this.ctx = this.canvas.getContext('2d');
    // 容错处理：getContext 返回 null 时（极罕见，如浏览器不支持 2d context）
    if (!this.ctx) {
        console.error('[Lv-00] 致命错误：无法获取 Canvas 2D 渲染上下文');
        throw new Error('[Lv-00] 无法获取 Canvas 2D 渲染上下文 / Failed to get Canvas 2D context');
    }
    this.points = [];                 // 前端点数据 [{id, x, y}, ...]
    this.segments = [];               // 前线段数据 [{p1, p2, id}, ...]

    // ================================================================
    //  选择与交互状态
    // ================================================================
    this.selectedPoint = null;        // 当前选中的单个点
    this.selectedPoints = [];         // 多选点集合
    this.hoveredPoint = null;         // 鼠标悬停的点
    this.regions = [];                // 已定义的区域 [{points: [...]}, ...]
    this.regionPoints = [];           // 区域绘制中的临时点

    // ================================================================
    //  工具与视图状态
    // ================================================================
    this.currentTool = 'select';      // 当前工具: select|point|segment|pan|region|probe
    this.scale = 1;                   // 画布缩放比例
    this.offsetX = 0;                 // 画布平移偏移 X
    this.offsetY = 0;                 // 画布平移偏移 Y
    this.dpr = window.devicePixelRatio || 1;  // 设备像素比（高 DPI 适配）

    // ================================================================
    //  拖拽相关状态
    // ================================================================
    this.isDragging = false;          // 是否正在拖拽画布
    this.dragStart = null;            // 拖拽起始屏幕坐标 {x, y}
    this.mouseWorldX = 0;             // 鼠标世界坐标 X
    this.mouseWorldY = 0;             // 鼠标世界坐标 Y
    this.mouseScreenX = 0;            // 鼠标屏幕坐标 X
    this.mouseScreenY = 0;            // 鼠标屏幕坐标 Y

    // ================================================================
    //  多选与框选状态
    // ================================================================
    this.isBoxSelecting = false;      // 是否正在框选
    this.boxSelectStart = null;       // 框选起始屏幕坐标

    // ================================================================
    //  拖拽移动点状态
    // ================================================================
    this.isDraggingPoint = false;     // 是否正在拖拽点
    this.dragPoint = null;            // 被拖拽的点对象

    // ================================================================
    //  事件监听器引用（用于 cleanup 时移除，防止内存泄漏）
    // ================================================================
    this._resizeHandler = null;

    // ================================================================
    //  模块状态（与后端子系统一一对应）
    // ================================================================
    this.typeSystem = null;           // 类型系统实例
    this.measureSystem = null;        // 测度系统实例
    this.engine = null;               // 引擎实例
    this.proposition = null;          // 当前命题（证明模块）
    this.proofNavigator = null;       // 证明导航器
    this.recursionContext = null;     // 递归上下文实例
    this.functionBlocks = {};         // 函数块字典 {blockId: {name, inputType, outputType, ...}}

    // ================================================================
    //  撤销/重做栈
    // ================================================================
    this.undoStack = [];              // 撤销栈（最多 50 条历史记录）
    this.redoStack = [];              // 重做栈

    // ================================================================
    //  日志级别（debug < info < warn < error）
    // ================================================================
    this.minLogLevel = 'debug';

    // ================================================================
    //  渲染优化状态
    // ================================================================
    this._renderPending = false;       // 是否有待处理的渲染请求
    this._lastRenderTime = 0;          // 上次渲染时间戳
    this._renderThrottleMs = 16;       // 渲染节流间隔（约 60fps）
    this._dirtyRect = null;            // 脏区域（暂未使用，保留用于增量渲染）
    this._isAnimating = false;         // 是否在动画中

    // ================================================================
    //  交互优化状态
    // ================================================================
    this._lastMouseMoveTime = 0;       // 上次鼠标移动时间
    this._mouseMoveThrottleMs = 8;     // 鼠标移动节流（~120fps 用于精确跟踪）
    this._zoomAnimation = null;        // 缩放动画句柄
    this._panAnimation = null;         // 平移动画句柄

    // ================================================================
    //  键盘状态
    // ================================================================
    this._keysPressed = {};            // 当前按下的键 {key: true}
    this._isTyping = false;            // 是否正在输入框中输入

    // ================================================================
    //  性能监控统计
    // ================================================================
    this._perfStats = {
        renderCount: 0,                // 自上次更新以来的渲染帧数
        lastFpsUpdate: 0,              // 上次 FPS 更新时间戳
        fps: 0,                        // 当前 FPS
        avgRenderTime: 0               // 指数移动平均渲染耗时（ms）
    };

    // ================================================================
    //  函数块视图状态
    // ================================================================
    this._blockViewFolded = false;     // 函数块视图是否折叠

    // ================================================================
    //  画布信息栏显示状态
    // ================================================================
    this._showGrid = true;             // 是否显示网格
    this._showAxes = true;             // 是否显示坐标轴
    this._showLabels = true;           // 是否显示标签

    // ================================================================
    //  性能监控 interval ID（用于清理防止内存泄漏）
    // ================================================================
    this._perfMonitorIntervalId = null; // setInterval 返回的 ID

    // 启动初始化流程
    this.init();
}

// ================================================================
// 初始化入口：尝试 WASM → 回退到 JS 后端
// 中文说明：应用初始化主入口，优先加载 WebAssembly 高性能后端，
//            若 WASM 不可用或初始化失败，则自动回退到纯 JavaScript 后端
// ================================================================

Lv00WebApp.prototype.init = function() {
    var self = this;

    // 更新加载步骤提示：常量已加载（constants.js 在 HTML 中先于此脚本加载）
    _updateLoadingStep('loadingStep1');

    // 优先尝试 WASM 后端
    if (typeof Lv00Module === 'function') {
        Lv00Module().then(function(wasmModule) {
            try {
                self.wasmModule = wasmModule;
                self.backend = 'wasm';
                self.graph = self.wasmModule.ccall('web_graph_create', 'number', [], []);
                self._finishInit();
            } catch (e) {
                console.warn('[Lv-00] WASM 初始化失败，回退到 JS 后端:', e.message);
                self._initJSBackend();
            }
        }).catch(function(e) {
            console.warn('[Lv-00] WebAssembly 不可用，回退到 JS 后端:', e.message);
            self._initJSBackend();
        });
    } else {
        console.warn('[Lv-00] WebAssembly 模块未找到，使用 JS 后端');
        this._initJSBackend();
    }
};

// ================================================================
// 初始化 JS 后端
// 中文说明：创建纯 JavaScript 后端实例作为 WASM 的兜底方案，
//            在 WASM 不可用或加载失败时自动切换到此模式
// ================================================================

Lv00WebApp.prototype._initJSBackend = function() {
    // 更新加载步骤提示：正在初始化后端引擎
    _updateLoadingStep('loadingStep2');

    try {
        // 安全检查：确保 Lv00JSBackend 构造函数存在
        if (typeof Lv00JSBackend === 'undefined') {
            throw new Error('Lv00JSBackend 未定义，请确保 lv00_js_backend.js 已加载');
        }
        this.jsBackend = new Lv00JSBackend();
        this.backend = 'js';
        this.graph = this.jsBackend.graphCreate();
        if (!this.graph) {
            throw new Error('JS 后端图创建失败');
        }
        this._finishInit();
    } catch (e) {
        console.error('[Lv-00] 初始化 JS 后端失败:', e);
        this._showInitError('初始化失败: ' + e.message);
    }
};

// ================================================================
/**
 * 显示初始化错误信息
 *
 * @description 使用 DOM API 安全创建错误提示界面，避免 innerHTML XSS 风险。
 *              在 WASM 或 JS 后端初始化失败时调用，显示错误详情和重试提示。
 *
 * @param {string} message - 错误消息描述
 * @returns {void}
 */
// ================================================================
Lv00WebApp.prototype._showInitError = function(message) {
    var loadingEl = document.getElementById('loading');
    if (!loadingEl) return;

    // 清空现有内容
    while (loadingEl.firstChild) {
        loadingEl.removeChild(loadingEl.firstChild);
    }

    var container = document.createElement('div');
    container.style.cssText = 'color:#f88;text-align:center;padding:40px;';

    var titleDiv = document.createElement('div');
    titleDiv.style.cssText = 'font-size:16px;margin-bottom:10px;';
    titleDiv.textContent = '\u26A0 加载失败 / LOAD FAILED';

    var msgDiv = document.createElement('div');
    msgDiv.style.cssText = 'font-size:12px;color:#888;';
    msgDiv.textContent = message;

    var hintDiv = document.createElement('div');
    hintDiv.style.cssText = 'margin-top:20px;font-size:11px;color:#666;';
    hintDiv.textContent = '请检查浏览器兼容性或刷新页面重试';

    container.appendChild(titleDiv);
    container.appendChild(msgDiv);
    container.appendChild(hintDiv);
    loadingEl.appendChild(container);
};

// ================================================================
// 统一后端调用辅助方法（带错误处理与兜底）
// @param {string} methodName - 后端方法名
// @param {Array} args - 调用参数数组
// @returns {*} 后端方法返回值，失败返回 null
// ================================================================

Lv00WebApp.prototype._callBackend = function(methodName, args) {
    // 守卫 1: 检查 JS 后端是否存在
    if (!this.jsBackend) {
        this.appendLog('操作 "' + methodName + '" 需要 JS 后端，但后端未就绪', 'warn');
        return null;
    }

    // 守卫 2: 检查方法是否存在
    var fn = this.jsBackend[methodName];
    if (typeof fn !== 'function') {
        this.appendLog('后端方法 "' + methodName + '" 不存在', 'error');
        return null;
    }

    try {
        var result = fn.apply(this.jsBackend, args);
        if (result === null || result === undefined) {
            this.appendLog('后端方法 "' + methodName + '" 返回空值 / null/undefined', 'warn');
        }
        return result;
    } catch (e) {
        this.appendLog('后端调用异常 "' + methodName + '": ' + e.message, 'error');
        console.error('[Lv-00] 后端调用异常:', methodName, e);
        return null;
    }
};

// ================================================================
// 完成初始化（核心入口）
// 中文说明：在 WASM 或 JS 后端就绪后统一执行，依次初始化
//            画布、事件监听、各子系统、模块按钮、性能监控。
//            每个子系统使用 safeInit 独立包裹，单个失败不影响整体。
// ================================================================

Lv00WebApp.prototype._finishInit = function() {
    var self = this;

    // 更新加载步骤提示：正在配置画布与事件
    _updateLoadingStep('loadingStep3');

    /**
     * 安全执行初始化函数，捕获异常但不中断整体流程
     * 使用 console.error 完整输出异常堆栈，方便排查问题
     * @param {string} name - 子系统名称（用于日志）
     * @param {Function} fn - 初始化函数
     */
    function safeInit(name, fn) {
        try {
            fn();
        } catch (e) {
            // 输出完整的错误对象（包含 stack trace），便于定位问题根因
            console.error('[Lv-00] 初始化 ' + name + ' 失败:', e);
            // 额外输出错误消息和堆栈，确保在控制台过滤时仍可见
            if (e && e.stack) {
                console.error('[Lv-00] 堆栈跟踪:', e.stack);
            }
            if (self.appendLog) {
                self.appendLog('初始化子系统 ' + name + ' 失败: ' + (e && e.message ? e.message : String(e)), 'error');
            }
        }
    }

    // 画布设置（必须最先完成）
    try {
        this.setupCanvas();
    } catch (e) {
        console.error('[Lv-00] 画布设置失败:', e);
        this._showInitError('画布初始化失败: ' + e.message);
        return;
    }

    // 事件监听器
    safeInit('事件监听器', function() { self.setupEventListeners(); });

    // 后端模块状态初始化
    if (this.jsBackend) {
        safeInit('类型系统', function() { self.typeSystem = self.jsBackend.typeSystemCreate(); });
        safeInit('测度系统', function() { self.measureSystem = self.jsBackend.measureSystemCreate(); });
        safeInit('引擎', function() { self.engine = self.jsBackend.engineCreate(); });
        safeInit('递归上下文', function() { self.recursionContext = self.jsBackend.recursionContextCreate(Lv00Const.RECURSION_DEPTH); });
    }

    // 更新引擎状态面板
    var backendEl = document.getElementById('engineBackend');
    if (backendEl) {
        backendEl.textContent = this.backend === 'wasm' ? 'WebAssembly' : 'JavaScript';
    }

    // 隐藏加载画面
    var loadingEl = document.getElementById('loading');
    if (loadingEl) loadingEl.classList.add('hidden');

    // 状态更新
    var backendLabel = this.backend === 'wasm' ? 'WebAssembly' : 'JavaScript (Fallback)';
    this.updateStatus('READY / 就绪 [' + backendLabel + ']');
    this.appendLog('Lv-00 几何元语言系统已就绪，选择工具栏开始构造 / Lv-00 Geometric Metalanguage System ready, select a tool to begin [' + backendLabel + ']', 'info');

    // 交互子系统独立初始化
    safeInit('右键菜单',   function() { self._initContextMenu(); });
    safeInit('框选模式',   function() { self._initBoxSelect(); });
    safeInit('拖拽点',     function() { self._initDragPoint(); });
    safeInit('键盘快捷键', function() { self._initKeyboard(); });
    safeInit('探测工具',   function() { self._initProbe(); });
    safeInit('主题切换',   function() { self._initThemeToggle(); });
    safeInit('搜索功能',   function() { self._initSearch(); });
    safeInit('模态框',     function() { self._initModals(); });
    safeInit('区域工具',   function() { self._initRegionTool(); });
    safeInit('工具提示',   function() { self._initTooltips(); });

    // 工具按钮绑定
    safeInit('区域工具按钮', function() { self._bindTool('toolRegion', 'region'); });
    safeInit('探测工具按钮', function() { self._bindTool('toolProbe', 'probe'); });

    // 更新加载步骤提示：正在加载功能模块
    _updateLoadingStep('loadingStep4');

    // 模块按钮与导出按钮
    safeInit('模块按钮', function() { self._bindModuleButtons(); });
    safeInit('导出按钮', function() { self._bindExportButton(); });

    // 性能监控
    safeInit('性能监控', function() { self._startPerfMonitor(); });

    // 公式模块自动初始化（如果已加载）
    if (typeof this.initFormulaModule === 'function') {
        safeInit('公式模块', function() { self.initFormulaModule(); });
    }

    // 初始同步：将当前图形数据同步到公式显示
    if (typeof self.formulaSyncFromGraph === 'function') {
        self.formulaSyncFromGraph();
    }
};

// ================================================================
/**
 * 通用按钮绑定辅助方法
 *
 * @description 为指定 ID 的按钮绑定点击事件处理器，自动添加加载状态（loading class）
 *              并在处理完成后移除。如果按钮处于 disabled 状态则跳过执行。
 *
 * @param {string} id - 按钮 DOM 元素 ID
 * @param {Function} handler - 点击事件处理函数
 * @returns {void}
 */
// ================================================================
Lv00WebApp.prototype._bindButton = function(id, handler) {
    var btn = document.getElementById(id);
    if (btn) {
        btn.addEventListener('click', function(e) {
            if (btn.disabled) return;
            btn.classList.add('loading');
            try {
                handler.call(this, e);
            } finally {
                btn.classList.remove('loading');
            }
        });
    }
};

// ================================================================
/**
 * 工具按钮绑定辅助方法
 *
 * @description 为指定 ID 的工具按钮绑定点击事件，点击时切换到对应的工具模式。
 *
 * @param {string} id - 工具按钮 DOM 元素 ID
 * @param {string} tool - 工具名称标识符（如 'select', 'point', 'segment', 'pan'）
 * @returns {void}
 */
// ================================================================
Lv00WebApp.prototype._bindTool = function(id, tool) {
    var self = this;
    var btn = document.getElementById(id);
    if (btn) {
        btn.addEventListener('click', function() { self.setTool(tool); });
    }
};

// ================================================================
/**
 * 开发中功能按钮的通用绑定辅助方法
 *
 * @description 为开发中的功能按钮提供统一的 try-catch 封装、后端方法可用性检查
 *              和按钮禁用逻辑。消除 BLOCK/PROOF 等模块中大量重复的 try-catch 模式。
 *
 * 中文说明：提取开发中功能按钮的通用绑定逻辑，避免 ~60 行的重复代码。
 *           自动检查 jsBackend[methodName] 是否存在，不存在时禁用按钮并提示。
 *
 * @param {string} btnId - 按钮 DOM 元素 ID
 * @param {string} methodName - 后端方法名
 * @param {string} featureLabel - 功能中文标签（用于 Toast 提示）
 * @param {string} [toastType='warn'] - Toast 类型（'warn' | 'info'）
 * @returns {void}
 */
// ================================================================
Lv00WebApp.prototype._bindDevButton = function(btnId, methodName, featureLabel, toastType) {
    var self = this;
    var toastType = toastType || 'warn';
    this._bindButton(btnId, function() {
        try {
            if (self.jsBackend && typeof self.jsBackend[methodName] === 'function') {
                self.jsBackend[methodName]();
            } else {
                self.showToast(featureLabel + '功能开发中', toastType);
                var btn = document.getElementById(btnId);
                if (btn) { btn.disabled = true; btn.classList.add('disabled'); }
            }
        } catch (e) {
            self.showToast(featureLabel + '操作失败: ' + e.message, 'error');
        }
    });
};

// ================================================================
// 模块按钮绑定（委托给各模块的方法）
// ================================================================

Lv00WebApp.prototype._bindModuleButtons = function() {
    var self = this;

    // 日志级别选择器
    var logLevelSelect = document.getElementById('selectLogLevel');
    if (logLevelSelect) {
        logLevelSelect.addEventListener('change', function() {
            self.minLogLevel = logLevelSelect.value;
            self.appendLog('日志级别已设置为: ' + self.minLogLevel, 'info');
        });
    }

    // ENGINE 模块按钮
    this._bindButton('btnEngineSolve', function() {
        self.appendLog('引擎求解中... / Engine solving...', 'info');
        self._engineSolve();
    });
    this._bindButton('btnEngineRewriteSolve', function() {
        self.appendLog('重写+求解... / Rewrite+Solve...', 'info');
        self._showRewriteExplorer();
    });
    this._bindButton('btnEngineCircuitTrip', function() {
        self.appendLog('电路跳闸模拟 / Circuit trip simulated', 'warn');
        self._showModal('modalNumericAssumption');
    });

    // PROOF 模块按钮
    this._bindButton('btnProofExportCoq', function() { self._proofExportCoq(); });
    this._bindButton('btnProofExFalso', function() { self._proofExFalso(); });

    // BLOCK 模块按钮
    this._bindButton('btnBlockPartialApply', function() { self._blockPartialApply(); });
    this._bindButton('btnBlockViewFold', function() { self._blockViewFold(); });
    // BLOCK 模块额外按钮（函数块操作）- 使用通用 _bindDevButton 消除重复模式
    this._bindDevButton('btnBlockPack', 'blockPack', '打包');
    this._bindDevButton('btnBlockInstantiate', 'blockInstantiate', '例化');
    this._bindDevButton('btnBlockDeterminism', 'blockDeterminism', '确定性检查');
    this._bindDevButton('btnBlockCompose', 'blockCompose', '组合');
    this._bindDevButton('btnBlockProduct', 'blockProduct', '乘积');

    // PROOF 模块导航按钮 - 使用通用 _bindDevButton 消除重复模式
    this._bindDevButton('btnProofPrev', 'proofNavPrev', '上一步', 'info');
    this._bindDevButton('btnProofNext', 'proofNavNext', '下一步', 'info');

    // RECURSE 模块按钮
    this._bindButton('btnRecurseMutual', function() { self._recurseMutual(); });
    this._bindButton('btnRecurseValidate', function() { self._recurseValidateMeasure(); });

    // DEBUG 模块按钮
    this._bindButton('btnDebugReset', function() { self._resetDebugCounters(); });
    this._bindButton('btnDebugReport', function() { self._showDebugReport(); });

    // 画布信息栏按钮
    this._bindButton('btnShowGrid', function() { self._toggleCanvasGrid(); });
    this._bindButton('btnShowAxes', function() { self._toggleCanvasAxes(); });
    this._bindButton('btnShowLabels', function() { self._toggleCanvasLabels(); });
};

// ================================================================
// 性能监控：每秒更新一次 FPS
// 中文说明：通过 setInterval 定时统计渲染帧率（FPS），
//            使用指数移动平均值平滑渲染耗时，避免突变干扰。
//            初始化前自动清理旧 interval 防止多定时器并发。
// ================================================================

Lv00WebApp.prototype._startPerfMonitor = function() {
    var self = this;

    // 清理之前的 interval，防止重复初始化导致内存泄漏和多 interval 并发 bug
    // 关键修复：在创建新 interval 前必须清除旧 interval，否则会有多个定时器同时运行
    if (this._perfMonitorIntervalId !== null && this._perfMonitorIntervalId !== undefined) {
        clearInterval(this._perfMonitorIntervalId);
        this._perfMonitorIntervalId = null;
    }

    // 安全检查：如果 performance API 不可用则跳过
    if (typeof performance === 'undefined' || typeof performance.now !== 'function') {
        console.warn('[Lv-00] performance.now 不可用，性能监控已禁用');
        return;
    }

    this._perfMonitorIntervalId = setInterval(function() {
        var now = performance.now();
        if (now - self._perfStats.lastFpsUpdate >= Lv00Const.PERF_MONITOR_MS) {
            self._perfStats.fps = self._perfStats.renderCount;
            self._perfStats.renderCount = 0;
            self._perfStats.lastFpsUpdate = now;

            // 更新状态栏 FPS 显示
            var fpsEl = document.getElementById('statusFps');
            if (fpsEl) {
                fpsEl.textContent = self._perfStats.fps + 'fps';
            }
        }
    }, Lv00Const.PERF_MONITOR_MS);
};

// ================================================================
// 画布信息栏切换方法
// ================================================================

// 切换网格显示状态
Lv00WebApp.prototype._toggleCanvasGrid = function() {
    this._showGrid = !this._showGrid;
    var btn = document.getElementById('btnShowGrid');
    if (btn) {
        btn.textContent = this._showGrid ? 'GRID / 网格: ON' : 'GRID / 网格: OFF';
    }
    this.render();
};

// 切换坐标轴显示状态
Lv00WebApp.prototype._toggleCanvasAxes = function() {
    this._showAxes = !this._showAxes;
    var btn = document.getElementById('btnShowAxes');
    if (btn) {
        btn.textContent = this._showAxes ? 'AXES / 坐标轴: ON' : 'AXES / 坐标轴: OFF';
    }
    this.render();
};

// 切换标签显示状态
Lv00WebApp.prototype._toggleCanvasLabels = function() {
    this._showLabels = !this._showLabels;
    var btn = document.getElementById('btnShowLabels');
    if (btn) {
        btn.textContent = this._showLabels ? 'LABELS / 标签: ON' : 'LABELS / 标签: OFF';
    }
    this.render();
};

// ================================================================
// 导出按钮绑定
// ================================================================

Lv00WebApp.prototype._bindExportButton = function() {
    var self = this;
    this._bindButton('btnExport', function() {
        // 构建导出数据对象
        var exportData = {
            version: '3.0.1',
            timestamp: new Date().toISOString(),
            backend: self.backend || 'unknown',
            points: self.points.map(function(p) {
                return { id: p.id, x: Number(p.x.toFixed(4)), y: Number(p.y.toFixed(4)) };
            }),
            segments: self.segments.map(function(s) {
                return { p1: s.p1, p2: s.p2, id: s.id };
            }),
            regions: (self.regions || []).map(function(r) {
                return {
                    points: (r.points || []).map(function(p) { return p.id; })
                };
            }),
            stats: {
                fps: self._perfStats.fps,
                scale: Number(self.scale.toFixed(4)),
                offsetX: Number(self.offsetX.toFixed(4)),
                offsetY: Number(self.offsetY.toFixed(4)),
                undoStackSize: self.undoStack.length,
                redoStackSize: self.redoStack.length
            }
        };

        try {
            var jsonStr = JSON.stringify(exportData, null, 2);
            var blob = new Blob([jsonStr], { type: 'application/json;charset=utf-8' });
            var url = URL.createObjectURL(blob);
            var a = document.createElement('a');
            a.href = url;
            a.download = 'lv00_export_' +
                new Date().toISOString().slice(0, 19).replace(/[T:]/g, '-') +
                '.json';
            document.body.appendChild(a);
            a.click();

            // 清理：延迟移除避免浏览器拦截
            setTimeout(function() {
                document.body.removeChild(a);
                URL.revokeObjectURL(url);
            }, Lv00Const.EXPORT_CLEANUP_MS);

            self.appendLog('数据导出成功 / Export complete: ' + a.download, 'info');
            self.showSuccess('导出完成 / Export complete');

            // 导出完成后同步公式显示
            if (typeof self.formulaSyncFromGraph === 'function') {
                self.formulaSyncFromGraph();
            }
        } catch (e) {
            self.appendLog('导出失败: ' + e.message + ' / Export failed', 'error');
            self.showError('导出失败: ' + e.message);
        }
    });
};

// ================================================================
/**
 * Canvas 画布设置 - 高 DPI 支持
 *
 * @description 初始化 Canvas 画布，根据设备像素比（devicePixelRatio）配置物理像素尺寸，
 *              确保在高 DPI 屏幕（如 Retina 显示屏）上渲染清晰。
 *              同时注册窗口 resize 事件监听器（带防抖），实现响应式画布尺寸调整。
 *
 * 中文说明：根据 window.devicePixelRatio 缩放 Canvas 物理像素，
 *            使用防抖处理窗口 resize 事件，避免频繁重设画布。
 *
 * @returns {void}
 */
// ================================================================
Lv00WebApp.prototype.setupCanvas = function() {
    var self = this;

    var resize = function() {
        self.dpr = window.devicePixelRatio || 1;
        var dpr = self.dpr;
        var w = self.canvas.offsetWidth;
        var h = self.canvas.offsetHeight;

        // 设置实际像素尺寸（物理像素）
        self.canvas.width = Math.floor(w * dpr);
        self.canvas.height = Math.floor(h * dpr);

        // CSS 尺寸保持不变
        self.canvas.style.width = w + 'px';
        self.canvas.style.height = h + 'px';

        // 重置变换矩阵并应用 DPR 缩放
        self.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

        self.render();
    };

    // 保存 resize 监听器引用，便于 cleanup 时移除
    self._resizeHandler = this._debounce(resize, 100);
    window.addEventListener('resize', self._resizeHandler);
    resize();
};

// ================================================================
/**
 * 事件监听器统一注册
 *
 * @description 注册 Canvas 工具栏按钮点击事件、鼠标/触摸事件、示例列表点击事件、
 *              模块标签页切换事件和面板折叠/展开事件。
 *              所有 Canvas 交互事件（mousedown/mousemove/mouseup/wheel/touch等）
 *              均在此方法中绑定到 this.canvas 上。
 *
 * 中文说明：集中注册所有 DOM 事件监听器，包括工具栏按钮、Canvas 鼠标/触摸事件、
 *            示例列表和模块标签页切换。
 *
 * @returns {void}
 */
// ================================================================
Lv00WebApp.prototype.setupEventListeners = function() {
    var self = this;

    // Canvas 工具栏按钮
    this._bindTool('toolSelect', 'select');
    this._bindTool('toolPoint', 'point');
    this._bindTool('toolSegment', 'segment');
    this._bindTool('toolPan', 'pan');

    // 各模块的子按钮组
    this._bindGraphButtons();
    this._bindTypeButtons();
    this._bindRecurseButtons();
    this._bindEngineButtons();
    this._bindDebugButtons();

    // Canvas 鼠标事件
    this.canvas.addEventListener('mousedown', function(e) { self.onMouseDown(e); });
    this.canvas.addEventListener('mousemove', function(e) { self.onMouseMove(e); });
    this.canvas.addEventListener('mouseup',   function(e) { self.onMouseUp(e); });
    this.canvas.addEventListener('mouseleave',function(e) { self.onMouseLeave(e); });
    this.canvas.addEventListener('wheel',     function(e) { self.onWheel(e); }, { passive: false });

    // Canvas 触摸事件（移动端支持）
    this.canvas.addEventListener('touchstart', function(e) { self.onTouchStart(e); }, { passive: false });
    this.canvas.addEventListener('touchmove',  function(e) { self.onTouchMove(e); },  { passive: false });
    this.canvas.addEventListener('touchend',   function(e) { self.onTouchEnd(e); });

    // 示例列表点击事件
    var exampleItems = document.querySelectorAll('.examples-list li');
    for (var i = 0; i < exampleItems.length; i++) {
        (function(li) {
            li.addEventListener('click', function() {
                self.loadExample(li.dataset.example);
            });
        })(exampleItems[i]);
    }

    // 公式示例列表点击事件（如果存在）
    var formulaExampleItems = document.querySelectorAll('#formulaExamplesList li');
    for (var j = 0; j < formulaExampleItems.length; j++) {
        (function(li) {
            li.addEventListener('click', function() {
                if (typeof self.formulaLoadExample === 'function') {
                    self.formulaLoadExample(li.dataset.example);
                }
            });
        })(formulaExampleItems[j]);
    }

    // 模块标签页切换事件（绑定所有硬编码模块标签）
    var moduleTabs = document.querySelectorAll('.module-tab[data-module]');
    for (var k = 0; k < moduleTabs.length; k++) {
        (function(tab) {
            // 跳过已绑定的事件（由 integrate-all.js 绑定的）
            if (tab._lv00TabBound) return;
            tab._lv00TabBound = true;
            tab.addEventListener('click', function() {
                self.switchModule(tab.getAttribute('data-module'));
            });
        })(moduleTabs[k]);
    }

    // 面板折叠/展开事件
    var collapseHeaders = document.querySelectorAll('.panel-title[data-collapse]');
    for (var m = 0; m < collapseHeaders.length; m++) {
        (function(header) {
            header.addEventListener('click', function() {
                var targetId = header.getAttribute('data-collapse');
                var body = document.getElementById(targetId);
                if (!body) return;
                var arrow = header.querySelector('.collapse-arrow');
                body.classList.toggle('collapsed');
                if (arrow) arrow.classList.toggle('collapsed');
            });
        })(collapseHeaders[m]);
    }
};

// ================================================================
// 模块级辅助函数：按钮绑定（供 _bindGraphButtons 等方法复用）
// 中文说明：提取各模块按钮绑定方法中重复的 bindBtn 局部函数，
//            统一为 Lv00WebApp 静态方法，避免全局命名空间污染。
// ================================================================
Lv00WebApp.bindBtn = function(id, handler) {
    var btn = document.getElementById(id);
    if (btn) btn.addEventListener('click', handler);
};

// ================================================================
// Graph 模块按钮绑定（委托给 modules/graph.js 中的方法）
// ================================================================

Lv00WebApp.prototype._bindGraphButtons = function() {
    var self = this;

    Lv00WebApp.bindBtn('btnGraphAddPoint', function() {
        var x = parseFloat(self._getInputValue('inputPointX', '0')) || 0;
        var y = parseFloat(self._getInputValue('inputPointY', '0')) || 0;
        self.addPoint(x, y);
        // 清空输入框值，仅在 DOM 元素存在时操作 / Clear input values, only when DOM elements exist
        var inputX = document.getElementById('inputPointX');
        var inputY = document.getElementById('inputPointY');
        if (inputX) inputX.value = '';
        if (inputY) inputY.value = '';
    });

    Lv00WebApp.bindBtn('btnGraphAddSegment',       function() { self.graphAddSegment(); });
    Lv00WebApp.bindBtn('btnGraphAddRegion',        function() { self.graphAddRegion(); });
    Lv00WebApp.bindBtn('btnGraphDeleteNode',       function() { self.graphRemoveNode(); });
    Lv00WebApp.bindBtn('btnGraphDeleteConstraint', function() { self.graphRemoveConstraint(); });
    Lv00WebApp.bindBtn('btnGraphIncidence',        function() { self.graphAddIncidence(); });
    Lv00WebApp.bindBtn('btnGraphBetweenness',      function() { self.graphAddBetweenness(); });
    Lv00WebApp.bindBtn('btnGraphIntersection',     function() { self.graphAddIntersection(); });
    Lv00WebApp.bindBtn('btnGraphContainment',      function() { self.graphAddContainment(); });
    Lv00WebApp.bindBtn('btnGraphNormalize',        function() { self.graphNormalize(); });
    Lv00WebApp.bindBtn('btnGraphFindMerge',        function() { self.graphFindMergeCandidates(); });
    Lv00WebApp.bindBtn('btnGraphRedundant',        function() { self.graphDetectRedundant(); });
    Lv00WebApp.bindBtn('btnGraphConflicts',        function() { self.graphDetectConflicts(); });
    Lv00WebApp.bindBtn('btnGraphDOF',              function() { self.graphDegreesOfFreedom(); });
    Lv00WebApp.bindBtn('btnGraphTopoSort',         function() { self.graphTopologicalSort(); });
    Lv00WebApp.bindBtn('btnGraphHash',             function() { self.graphHash(); });
    Lv00WebApp.bindBtn('btnGraphClear', function() {
        // 使用原生 confirm() 而非自定义模态框，以确保清空操作时的确认对话框
        // 在所有浏览器中行为一致且不会被异步渲染干扰
        // Use native confirm() instead of custom modal for reliable synchronous confirmation
        // 注意：当前项目的 _showModal(id) 仅支持显示模态框，不支持传入回调函数。
        //       若未来 _showModal 扩展支持 onConfirm 回调参数，应替换为自定义模态框。
        if (confirm('确定要清空所有数据吗？此操作不可撤销。\nClear all data? This cannot be undone.')) {
            self.clear();
        }
    });
};

// ================================================================
// Type 模块按钮绑定
// ================================================================

Lv00WebApp.prototype._bindTypeButtons = function() {
    var self = this;

    Lv00WebApp.bindBtn('btnTypePoint',    function() { self.typeCreatePoint(); });
    Lv00WebApp.bindBtn('btnTypeSegment',  function() { self.typeCreateSegment(); });
    Lv00WebApp.bindBtn('btnTypeRegion',   function() { self.typeCreateRegion(); });
    Lv00WebApp.bindBtn('btnTypeFunction', function() { self.typeCreateFunction(); });
    Lv00WebApp.bindBtn('btnTypeProduct',  function() { self.typeCreateProduct(); });
    Lv00WebApp.bindBtn('btnTypeEquiv',    function() { self.typeCheckEquiv(); });
    Lv00WebApp.bindBtn('btnTypeInfer',    function() { self.typeInferNode(); });
    Lv00WebApp.bindBtn('btnTypeLevel',    function() { self.typeCheckLevel(); });

    Lv00WebApp.bindBtn('btnTypeCreate',    function() { self.showToast('类型创建功能开发中', 'info'); });
    Lv00WebApp.bindBtn('btnTypeCheck',     function() { self.showToast('类型检查功能开发中', 'info'); });
    Lv00WebApp.bindBtn('btnTypeUnify',     function() { self.showToast('类型统一化功能开发中', 'info'); });
    Lv00WebApp.bindBtn('btnTypeSubtype',   function() { self.showToast('子类型检查功能开发中', 'info'); });

    // 禁用 TYPE 模块中尚在开发中的按钮
    ['btnTypeCreate', 'btnTypeCheck', 'btnTypeUnify', 'btnTypeSubtype'].forEach(function(id) {
        var btn = document.getElementById(id);
        if (btn) {
            btn.disabled = true;
            btn.classList.add('disabled');
            btn.title = '功能开发中 / Feature coming soon';
            var originalText = btn.textContent;
            if (originalText && originalText.indexOf('(即将推出)') === -1) {
                btn.textContent = originalText + ' (即将推出)';
            }
        }
    });
};

// ================================================================
// Recurse 模块按钮绑定
// ================================================================

Lv00WebApp.prototype._bindRecurseButtons = function() {
    var self = this;

    Lv00WebApp.bindBtn('btnRecurseCreateMeasure',   function() { self.recurseCreateMeasure(); });
    Lv00WebApp.bindBtn('btnRecurseComputeMeasure',  function() { self.recurseComputeMeasure(); });
    Lv00WebApp.bindBtn('btnRecurseEnter',           function() { self.recurseEnter(); });
    Lv00WebApp.bindBtn('btnRecurseExit',            function() { self.recurseExit(); });
    Lv00WebApp.bindBtn('btnRecurseSelector',        function() { self.recurseSelectorEvaluate(); });
    Lv00WebApp.bindBtn('btnRecurseCheckDecreasing', function() { self.recurseCheckDecreasing(); });

    Lv00WebApp.bindBtn('btnRecurseDefine', function() { self.showToast('递归定义功能开发中', 'info'); });
    Lv00WebApp.bindBtn('btnRecurseStep',   function() { self.showToast('递归步骤功能开发中', 'info'); });

    // 禁用 RECURSE 模块中尚在开发中的按钮
    ['btnRecurseDefine', 'btnRecurseStep'].forEach(function(id) {
        var btn = document.getElementById(id);
        if (btn) {
            btn.disabled = true;
            btn.classList.add('disabled');
            btn.title = '功能开发中 / Feature coming soon';
            var originalText = btn.textContent;
            if (originalText && originalText.indexOf('(即将推出)') === -1) {
                btn.textContent = originalText + ' (即将推出)';
            }
        }
    });
};

// ================================================================
// Engine 模块按钮绑定
// ================================================================

Lv00WebApp.prototype._bindEngineButtons = function() {
    var self = this;

    Lv00WebApp.bindBtn('btnEngineStatus',      function() { self.engineStatus(); });
    Lv00WebApp.bindBtn('btnEngineAddRule',     function() { self.engineAddRule(); });
    Lv00WebApp.bindBtn('btnEnginePack',        function() { self.enginePack(); });
    Lv00WebApp.bindBtn('btnEngineInstantiate', function() { self.engineInstantiate(); });
    Lv00WebApp.bindBtn('btnEngineUnify',       function() { self.engineUnify(); });
};

// ================================================================
// Debug 模块按钮绑定
// ================================================================

Lv00WebApp.prototype._bindDebugButtons = function() {
    var self = this;

    Lv00WebApp.bindBtn('btnDebugCounters', function() { self.debugCounters(); });
    Lv00WebApp.bindBtn('btnDebugReport',   function() { self.debugReport(); });
};

// ================================================================
// 工具切换
// 中文说明：切换当前活动工具（选择/点/线段/平移/区域/探测），
//            更新工具栏按钮高亮状态、鼠标光标样式和状态栏提示文字。
// ================================================================

Lv00WebApp.prototype.setTool = function(tool) {
    // 更新当前工具
    this.currentTool = tool;

    // 切换工具时重置交互状态
    this.selectedPoints = [];
    this.isBoxSelecting = false;
    this.isDraggingPoint = false;
    this.regionPoints = [];

    // 更新工具栏按钮激活状态
    var toolBtns = document.querySelectorAll('.tool-btn');
    for (var i = 0; i < toolBtns.length; i++) {
        toolBtns[i].classList.remove('active');
    }
    var toolId = 'tool' + tool.charAt(0).toUpperCase() + tool.slice(1);
    var toolEl = document.getElementById(toolId);
    if (toolEl) toolEl.classList.add('active');

    // 更新状态栏工具名
    var toolNames = {
        'select':  '选择 / SELECT',
        'point':   '添加点 / ADD POINT',
        'segment': '添加线段 / ADD SEGMENT',
        'pan':     '平移 / PAN VIEW',
        'region':  '区域 / REGION',
        'probe':   '探测 / PROBE'
    };
    this.updateStatus(toolNames[tool] || tool);

    // 更新鼠标光标样式
    this._updateCursor();

    // 更新状态栏快捷键提示
    this._updateStatusHelp();
};

// ================================================================
// 更新状态栏键盘快捷键提示
// 根据当前工具显示对应的快捷键组合
// ================================================================

Lv00WebApp.prototype._updateStatusHelp = function() {
    var helpEl = document.getElementById('statusHelp');
    if (!helpEl) return;

    var shortcuts = {
        'select':  'V 选择工具 | P 点工具 | L 线段工具 | H 平移 | +/- 缩放 | Ctrl+Z 撤销 | Ctrl+Y 重做 | Ctrl+0 重置视图',
        'point':   '点击画布添加点 / Click to add point | Esc 取消 | Ctrl+Z 撤销 | Ctrl+0 重置视图',
        'segment': '点击两点连线 / Click two points | Esc 取消 | Ctrl+Z 撤销 | Ctrl+0 重置视图',
        'pan':     '拖拽平移 / Drag to pan | 滚轮缩放 | Ctrl+0 重置视图',
        'region':  '点击顶点定义区域 / Click vertices | Esc 取消 | Ctrl+Z 撤销',
        'probe':   '悬停查看坐标 / Hover to inspect | Esc 取消'
    };

    helpEl.textContent = shortcuts[this.currentTool] || shortcuts['select'];
};

// ================================================================
// 面板切换时重置相关显示的辅助函数
// 在模块切换时调用，清空此前模块特定的数据显示，恢复空状态提示
// ================================================================

Lv00WebApp.prototype._resetPanelDisplay = function() {
    // 重置依赖树为空状态提示
    var depTree = document.getElementById('depTree');
    if (depTree) {
        // 清空现有内容并添加空状态提示
        while (depTree.firstChild) {
            depTree.removeChild(depTree.firstChild);
        }
        var emptyNode = document.createElement('div');
        emptyNode.className = 'dep-tree-node dep-tree-root';
        emptyNode.style.cssText = 'color:#8b949e;text-align:center;font-style:italic;';
        emptyNode.textContent = '暂无依赖关系数据 / No dependency data yet';
        depTree.appendChild(emptyNode);
    }

    // 重置属性面板为空状态
    var propContent = document.getElementById('propContent');
    if (propContent) {
        while (propContent.firstChild) {
            propContent.removeChild(propContent.firstChild);
        }
        var emptyProp = document.createElement('div');
        emptyProp.className = 'prop-empty';
        emptyProp.textContent = '选择一个元素查看属性 / Select an element';
        propContent.appendChild(emptyProp);
    }
};

// ================================================================
/**
 * 坐标转换：世界坐标 -> 屏幕坐标
 *
 * @description 将世界坐标系中的点转换为 Canvas 屏幕坐标。
 *              世界原点映射到画布中心，再加上 offset 偏移和 scale 缩放。
 *
 * 中文说明：世界坐标 (x, y) 先加偏移再缩放，最后平移到画布中心。
 *
 * @param {number} x - 世界 X 坐标
 * @param {number} y - 世界 Y 坐标
 * @returns {{x: number, y: number}} 屏幕坐标对象 {x, y}
 */
// ================================================================
Lv00WebApp.prototype.worldToScreen = function(x, y) {
    return {
        x: (x + this.offsetX) * this.scale + this.canvas.offsetWidth  / 2,
        y: (y + this.offsetY) * this.scale + this.canvas.offsetHeight / 2
    };
};

/**
 * 坐标转换：屏幕坐标 -> 世界坐标
 *
 * @description 将 Canvas 屏幕坐标转换回世界坐标系。
 *              从画布中心反推，先减去偏移再除以缩放。
 *
 * @param {number} x - 屏幕 X 坐标（CSS 像素）
 * @param {number} y - 屏幕 Y 坐标（CSS 像素）
 * @returns {{x: number, y: number}} 世界坐标对象 {x, y}
 */
Lv00WebApp.prototype.screenToWorld = function(x, y) {
    return {
        x: (x - this.canvas.offsetWidth  / 2) / this.scale - this.offsetX,
        y: (y - this.canvas.offsetHeight / 2) / this.scale - this.offsetY
    };
};

/**
 * 坐标转换：世界 X 坐标 -> 屏幕 X 坐标（单轴快捷方法）
 *
 * @param {number} x - 世界 X 坐标
 * @returns {number} 屏幕 X 坐标
 */
Lv00WebApp.prototype.worldToScreenX = function(x) {
    return (x + this.offsetX) * this.scale + this.canvas.offsetWidth / 2;
};

/**
 * 坐标转换：世界 Y 坐标 -> 屏幕 Y 坐标（单轴快捷方法）
 *
 * @param {number} y - 世界 Y 坐标
 * @returns {number} 屏幕 Y 坐标
 */
Lv00WebApp.prototype.worldToScreenY = function(y) {
    return (y + this.offsetY) * this.scale + this.canvas.offsetHeight / 2;
};

// ================================================================
// 添加几何元素点
// 中文说明：在世界坐标系中创建一个新节点，坐标自动四舍五入到一位小数。
//            支持 WASM 和 JS 双后端，添加成功后同步更新公式显示。
// @param {number} x - 世界坐标 X
// @param {number} y - 世界坐标 Y
// @returns {number} 新节点的 ID，-1 表示失败
// ================================================================

Lv00WebApp.prototype.addPoint = function(x, y) {
    // guard: canvas 和 ctx 必须可用
    if (!this.canvas || !this.ctx) {
        console.error('[Lv-00] addPoint: canvas 或 ctx 未就绪 / canvas or ctx not ready');
        this.appendLog('添加点失败：画布未就绪 / Canvas not ready', 'error');
        return -1;
    }

    // 输入验证：坐标必须是有限数字，拒绝 NaN、Infinity、非数字类型
    if (typeof x !== 'number' || typeof y !== 'number' || !isFinite(x) || !isFinite(y)) {
        console.error('[Lv-00] addPoint: 无效坐标 x=' + x + ' y=' + y + ' / invalid coordinates');
        this.appendLog('添加点失败：坐标无效（必须为有限数字）/ Invalid coordinates', 'error');
        return -1;
    }

    this._saveUndoState();

    // 坐标四舍五入到小数点后一位
    var rx = Math.round(x * 10) / 10;
    var ry = Math.round(y * 10) / 10;
    var id;

    if (this.backend === 'wasm') {
        id = this.wasmModule.ccall('web_graph_add_point', 'number',
            ['number', 'number', 'number', 'number', 'number'],
            [this.graph, rx, 1, ry, 1]
        );
    } else {
        var coordX = this.jsBackend.coordCreateRational(rx, 1);
        var coordY = this.jsBackend.coordCreateRational(ry, 1);
        id = this.jsBackend.graphAddPoint(this.graph, coordX, coordY);
    }

    if (id >= 0) {
        this.points.push({ id: id, x: x, y: y });
        this.updateStats();
        this.updateStatus('添加点 n' + id + ' / ADD POINT (' + rx + ', ' + ry + ')');
        this.appendLog('添加点: n' + id + ' (' + rx + ', ' + ry + ')', 'info');
    } else {
        this.appendLog('添加点失败 / Add point failed', 'error');
    }

    this.render();

    // 节点增删后同步公式显示
    if (typeof this.formulaSyncFromGraph === 'function') {
        this.formulaSyncFromGraph();
    }

    return id;
};

// ================================================================
// 添加线段（连接两个现有节点）
// 中文说明：在两个已存在的节点之间创建线段约束，拒绝自环连接。
//            支持 WASM 和 JS 双后端，成功后同步更新公式显示。
// @param {number} p1 - 起点节点 ID
// @param {number} p2 - 终点节点 ID
// ================================================================

Lv00WebApp.prototype.addSegment = function(p1, p2) {
    // guard: canvas 和 ctx 必须可用
    if (!this.canvas || !this.ctx) {
        console.error('[Lv-00] addSegment: canvas 或 ctx 未就绪 / canvas or ctx not ready');
        this.appendLog('添加线段失败：画布未就绪 / Canvas not ready', 'error');
        return;
    }

    // 输入验证：p1 和 p2 必须是有效的数字节点 ID（非 NaN、非 Infinity、整数）
    if (typeof p1 !== 'number' || typeof p2 !== 'number' || !isFinite(p1) || !isFinite(p2)) {
        console.error('[Lv-00] addSegment: 无效节点 ID p1=' + p1 + ' p2=' + p2 + ' / invalid node IDs');
        this.appendLog('添加线段失败：节点 ID 无效 / Invalid node IDs', 'error');
        return;
    }
    // 验证节点 ID 为非负整数
    if (p1 < 0 || p2 < 0 || Math.floor(p1) !== p1 || Math.floor(p2) !== p2) {
        console.error('[Lv-00] addSegment: 节点 ID 必须为非负整数 p1=' + p1 + ' p2=' + p2);
        this.appendLog('添加线段失败：节点 ID 必须为非负整数 / Node IDs must be non-negative integers', 'error');
        return;
    }

    // 守卫：拒绝自环
    if (p1 === p2) {
        this.appendLog('不能创建自环/自连接 / Cannot create self-loop', 'warn');
        return;
    }

    this._saveUndoState();
    var result;

    if (this.backend === 'wasm') {
        result = this.wasmModule.ccall('web_graph_add_line_segment', 'number',
            ['number', 'number', 'number'],
            [this.graph, p1, p2]
        );
    } else {
        result = this.jsBackend.graphAddLineSegment(this.graph, p1, p2);
    }

    if (result >= 0) {
        this.segments.push({ p1: p1, p2: p2, id: result });
        this.updateStats();
        this.updateStatus('添加线段 / SEGMENT n' + p1 + ' → n' + p2);
        this.appendLog('添加线段: n' + p1 + ' → n' + p2 + ' (id=' + result + ')', 'info');
    } else {
        this.appendLog('添加线段失败 / Add segment failed', 'error');
    }

    this.render();

    // 节点增删后同步公式显示
    if (typeof this.formulaSyncFromGraph === 'function') {
        this.formulaSyncFromGraph();
    }
};

// ================================================================
// 全局清理方法
// 中文说明：在页面卸载或应用销毁时调用，清理所有事件监听器和
//            相关资源（Canvas、DOM 事件、定时器等），防止内存泄漏。
// 与 interaction.js 的 cleanup() 协调工作，依次清理各子系统。
// ================================================================
Lv00WebApp.prototype.cleanup = function() {
    // 0. 移除窗口 resize 事件监听器（防止内存泄漏）
    if (this._resizeHandler) {
        window.removeEventListener('resize', this._resizeHandler);
        this._resizeHandler = null;
    }

    // 1. 清理交互事件监听器（键盘、鼠标、触摸、右键菜单等）
    if (typeof this.cleanupInteraction === 'function') {
        this.cleanupInteraction();
    }

    // 2. 移除 Canvas DOM 引用，释放渲染上下文
    if (this.canvas) {
        this.canvas = null;
    }
    if (this.ctx) {
        this.ctx = null;
    }

    // 3. 清理流式桥接器
    if (this.streamBridge && typeof this.streamBridge.destroy === 'function') {
        this.streamBridge.destroy();
    }

    // 4. 清理后端引用
    this.jsBackend = null;
    this.wasmModule = null;
    this.graph = null;

    // 5. 清理交互事件监听器（键盘、鼠标、触摸、右键菜单等）
    if (typeof this._cleanupEventListeners === 'function') {
        this._cleanupEventListeners();
    }

    // 6. 清理模态框和搜索快捷键事件监听器（ESC、Ctrl+F，防止内存泄漏）
    if (typeof this._cleanupModals === 'function') {
        this._cleanupModals();
    }

    // 7. 日志确认清理完成
    if (typeof console !== 'undefined') {
        console.log('[Lv-00] 应用清理完成 / Application cleaned up');
    }
};

// ================================================================
// 清空全部几何数据
// 中文说明：销毁当前约束图并创建全新空图，重置所有前端状态
//           包括点、线段、选区、区域，最后同步公式显示。
// ================================================================

Lv00WebApp.prototype.clear = function() {
    // guard: canvas 和 ctx 必须可用
    if (!this.canvas || !this.ctx) {
        console.error('[Lv-00] clear: canvas 或 ctx 未就绪 / canvas or ctx not ready');
        this.appendLog('清空失败：画布未就绪 / Canvas not ready', 'error');
        return;
    }

    if (this.backend === 'wasm') {
        this.wasmModule.ccall('web_graph_destroy', null, ['number'], [this.graph]);
        this.graph = this.wasmModule.ccall('web_graph_create', 'number', [], []);
    } else {
        this.jsBackend.graphDestroy(this.graph);
        this.graph = this.jsBackend.graphCreate();
    }

    // 重置所有前端状态
    this.points = [];
    this.segments = [];
    this.selectedPoint = null;
    this.selectedPoints = [];
    this.regions = [];
    this.regionPoints = [];

    this.updateStats();
    this.updateProperties(null);
    this.updateStatus('已清空 / CLEARED');
    this.appendLog('图已清空 / Graph cleared', 'info');
    this.render();

    // 清空后同步公式显示
    if (typeof this.formulaSyncFromGraph === 'function') {
        this.formulaSyncFromGraph();
    }
};

// ================================================================
// DOM 就绪后自动启动应用
// 中文说明：将 Lv00WebApp 实例挂载到 window.lv00App 全局变量，
//            便于其他模块（integration-all.js、调试脚本、浏览器控制台）
//            通过 window.lv00App 直接访问应用实例进行调试和交互。
// ================================================================
document.addEventListener('DOMContentLoaded', function() {
    window.lv00App = new Lv00WebApp();
});
