/**
 * streaming.js - Lv-00 流式输出核心模块
 *
 * 提供 StreamBridge 类，用于在 JS 后端和前端 UI 之间桥接流式事件。
 * 支持 39 种事件类型（与 C 内核 stream.h StreamEventType 枚举一一对应）、
 * 事件类型分组过滤、实时日志展示、清除日志、SSE 连接超时与自动重连等功能。
 *
 * 中文说明：流式输出核心模块，桥接 JS 后端事件到前端 UI 面板。
 *          支持全部 39 种事件类型的分组过滤、实时日志、SSE 超时重连。
 *
 * 导出：
 *   window.Lv00Streaming = { StreamBridge: StreamBridge, EVENT_TYPES: EVENT_TYPES }
 *
 * 事件类型分组（8 组 39 种）：
 *   ENG(0-2): 引擎生命周期    NRM(3-5): 归一化
 *   RWT(6-11): 重写引擎       SLV(12-16): 代数求解
 *   PRF(17-21): 证明系统       FBK(22-29): 函数块系统
 *   ERR(30-35): 冲突与错误     LOG(36-38): 信息与进度
 *
 * 使用方式：
 *   var bridge = new window.Lv00Streaming.StreamBridge(app);
 *   bridge.init(document.getElementById('streamingContainer'));
 *   bridge.emit({ type: 0, description: '引擎启动', step_number: 0 });
 *   bridge.connectSSE('/api/stream');  // 连接 SSE 端点
 *   bridge.destroy();
 *
 * 依赖：无外部依赖，纯 ES5 DOM API
 * 语法标准：严格 ES5
 * 作者：Lv-00 Team
 * 创建日期：2026-05-20
 * 更新日期：2026-05-21 - 补全全部 31 种事件类型，分组过滤器
 */
(function() {
    'use strict';

    // ================================================================
    //  事件类型常量定义
    //  与 C 内核 stream.h 中 StreamEventType 枚举一一对应
    //  值 = 枚举在枚举体中的位置（从 0 开始）
    // ================================================================
    var EVENT_TYPES = {
        /* ---- 引擎生命周期 ---- */
        ENGINE_START: 0,
        ENGINE_DONE: 1,
        ENGINE_PAUSED: 2,
        /* ---- 归一化 ---- */
        NORMALIZE_START: 3,
        NORMALIZE_MERGE: 4,
        NORMALIZE_DONE: 5,
        /* ---- 重写引擎 ---- */
        REWRITE_START: 6,
        REWRITE_RULE_LOADED: 7,
        REWRITE_MATCH_FOUND: 8,
        REWRITE_APPLIED: 9,
        REWRITE_ROLLBACK: 10,
        REWRITE_DONE: 11,
        /* ---- 代数求解 ---- */
        SOLVE_START: 12,
        SOLVE_EQUATION_EXTRACTED: 13,
        SOLVE_GROEBNER_STEP: 14,
        SOLVE_VARIABLE_RESOLVED: 15,
        SOLVE_DONE: 16,
        /* ---- 证明系统 ---- */
        PROOF_STEP_ADDED: 17,
        PROOF_STEP_APPLIED: 18,
        PROOF_UNIFY: 19,
        PROOF_COLOR_UPDATE: 20,
        PROOF_DEPENDENCY_CHANGE: 21,
        /* ---- 函数块系统 ---- */
        FUNC_BLOCK_PACK_START: 22,
        FUNC_BLOCK_PACK_DONE: 23,
        FUNC_BLOCK_INSTANTIATE_START: 24,
        FUNC_BLOCK_INSTANTIATE_DONE: 25,
        FUNC_BLOCK_PARTIAL_APPLY: 26,
        FUNC_BLOCK_DETERMINISM_CHECK: 27,
        FUNC_BLOCK_CAPTURE_AVOID: 28,
        FUNC_BLOCK_CROSS_BOUNDARY: 29,
        /* ---- 冲突与错误 ---- */
        CONFLICT_DETECTED: 30,
        CONSTRAINT_ADDED: 31,
        NODE_ADDED: 32,
        CIRCUIT_TRIP: 33,
        ERROR: 34,
        WARNING: 35,
        /* ---- 信息 ---- */
        INFO: 36,
        PROGRESS: 37,
        GRAPH_SNAPSHOT: 38
    };

    // ================================================================
    //  事件类型元数据映射表
    //  将事件类型数值映射为显示名称、短标签、CSS 类名、颜色方案
    //  颜色方案与 C 内核 stream.c 中 stream_event_color() 保持一致
    // ================================================================
    var EVENT_META = {};
    /* ---- 引擎生命周期 ---- */
    EVENT_META[0]  = { name: '引擎启动 / Engine Start',       shortName: 'ENG-START', cssClass: 'stream-ev-engine',    colorScheme: 'green' };
    EVENT_META[1]  = { name: '引擎完成 / Engine Done',         shortName: 'ENG-DONE',  cssClass: 'stream-ev-engine',    colorScheme: 'green' };
    EVENT_META[2]  = { name: '引擎暂停 / Engine Paused',       shortName: 'ENG-PAUSE', cssClass: 'stream-ev-engine',    colorScheme: 'blue' };
    /* ---- 归一化 ---- */
    EVENT_META[3]  = { name: '规范化开始 / Normalize Start',   shortName: 'NRM-START', cssClass: 'stream-ev-normalize', colorScheme: 'green' };
    EVENT_META[4]  = { name: '节点合并 / Normalize Merge',     shortName: 'NRM-MERGE', cssClass: 'stream-ev-normalize', colorScheme: 'purple' };
    EVENT_META[5]  = { name: '规范化完成 / Normalize Done',    shortName: 'NRM-DONE',  cssClass: 'stream-ev-normalize', colorScheme: 'green' };
    /* ---- 重写引擎 ---- */
    EVENT_META[6]  = { name: '重写开始 / Rewrite Start',       shortName: 'RWT-START', cssClass: 'stream-ev-rewrite',   colorScheme: 'green' };
    EVENT_META[7]  = { name: '规则加载 / Rule Loaded',         shortName: 'RWT-RULE',  cssClass: 'stream-ev-rewrite',   colorScheme: 'blue' };
    EVENT_META[8]  = { name: '匹配找到 / Match Found',         shortName: 'RWT-MATCH', cssClass: 'stream-ev-rewrite',   colorScheme: 'purple' };
    EVENT_META[9]  = { name: '规则应用 / Rule Applied',        shortName: 'RWT-APPLY', cssClass: 'stream-ev-rewrite',   colorScheme: 'purple' };
    EVENT_META[10] = { name: '规则回滚 / Rule Rollback',       shortName: 'RWT-ROLL',  cssClass: 'stream-ev-rewrite',   colorScheme: 'yellow' };
    EVENT_META[11] = { name: '重写完成 / Rewrite Done',        shortName: 'RWT-DONE',  cssClass: 'stream-ev-rewrite',   colorScheme: 'green' };
    /* ---- 代数求解 ---- */
    EVENT_META[12] = { name: '求解开始 / Solve Start',         shortName: 'SLV-START', cssClass: 'stream-ev-solve',     colorScheme: 'green' };
    EVENT_META[13] = { name: '方程提取 / Equation Extracted',  shortName: 'SLV-EQXT',  cssClass: 'stream-ev-solve',     colorScheme: 'blue' };
    EVENT_META[14] = { name: 'Gröbner基步骤 / Groebner Step',  shortName: 'SLV-GRB',   cssClass: 'stream-ev-solve',     colorScheme: 'purple' };
    EVENT_META[15] = { name: '变量解得 / Variable Resolved',   shortName: 'SLV-VAR',   cssClass: 'stream-ev-solve',     colorScheme: 'purple' };
    EVENT_META[16] = { name: '求解完成 / Solve Done',          shortName: 'SLV-DONE',  cssClass: 'stream-ev-solve',     colorScheme: 'green' };
    /* ---- 证明系统 ---- */
    EVENT_META[17] = { name: '证明步骤添加 / Proof Step Added',    shortName: 'PRF-ADD',  cssClass: 'stream-ev-proof', colorScheme: 'blue' };
    EVENT_META[18] = { name: '证明步骤应用 / Proof Step Applied',  shortName: 'PRF-APLY', cssClass: 'stream-ev-proof', colorScheme: 'purple' };
    EVENT_META[19] = { name: '合一检查 / Proof Unify',             shortName: 'PRF-UNFY', cssClass: 'stream-ev-proof', colorScheme: 'purple' };
    EVENT_META[20] = { name: '颜色更新 / Color Update',           shortName: 'PRF-COLR', cssClass: 'stream-ev-proof', colorScheme: 'purple' };
    EVENT_META[21] = { name: '依赖链变化 / Dependency Change',    shortName: 'PRF-DEP',  cssClass: 'stream-ev-proof', colorScheme: 'blue' };
    /* ---- 函数块系统 ---- */
    EVENT_META[22] = { name: '函数打包开始 / FuncBlock Pack Start',  shortName: 'FBK-PACK', cssClass: 'stream-ev-funcblock', colorScheme: 'green' };
    EVENT_META[23] = { name: '函数打包完成 / FuncBlock Pack Done',   shortName: 'FBK-PACK', cssClass: 'stream-ev-funcblock', colorScheme: 'green' };
    EVENT_META[24] = { name: '函数实例化开始 / Instantiate Start',   shortName: 'FBK-INST', cssClass: 'stream-ev-funcblock', colorScheme: 'green' };
    EVENT_META[25] = { name: '函数实例化完成 / Instantiate Done',    shortName: 'FBK-INST', cssClass: 'stream-ev-funcblock', colorScheme: 'green' };
    EVENT_META[26] = { name: '部分应用 / Partial Apply',             shortName: 'FBK-PART', cssClass: 'stream-ev-funcblock', colorScheme: 'blue' };
    EVENT_META[27] = { name: '确定性检查 / Determinism Check',       shortName: 'FBK-DET',  cssClass: 'stream-ev-funcblock', colorScheme: 'blue' };
    EVENT_META[28] = { name: '捕获避免 / Capture Avoid',             shortName: 'FBK-CAP',  cssClass: 'stream-ev-funcblock', colorScheme: 'blue' };
    EVENT_META[29] = { name: '跨边界操作 / Cross Boundary',          shortName: 'FBK-CROSS',cssClass: 'stream-ev-funcblock', colorScheme: 'purple' };
    /* ---- 冲突与错误 ---- */
    EVENT_META[30] = { name: '冲突检测 / Conflict Detected',  shortName: 'CONFLICT',  cssClass: 'stream-ev-conflict', colorScheme: 'red' };
    EVENT_META[31] = { name: '约束添加 / Constraint Added',   shortName: 'CON-ADD',   cssClass: 'stream-ev-conflict', colorScheme: 'blue' };
    EVENT_META[32] = { name: '节点添加 / Node Added',         shortName: 'NODE-ADD',  cssClass: 'stream-ev-conflict', colorScheme: 'blue' };
    EVENT_META[33] = { name: '位数熔断 / Circuit Trip',       shortName: 'CIRCUIT',   cssClass: 'stream-ev-conflict', colorScheme: 'orange' };
    EVENT_META[34] = { name: '错误 / Error',                  shortName: 'ERROR',     cssClass: 'stream-ev-error',   colorScheme: 'red' };
    EVENT_META[35] = { name: '警告 / Warning',                shortName: 'WARN',      cssClass: 'stream-ev-error',   colorScheme: 'yellow' };
    /* ---- 信息 ---- */
    EVENT_META[36] = { name: '信息 / Info',                   shortName: 'INFO',      cssClass: 'stream-ev-info',    colorScheme: 'gray' };
    EVENT_META[37] = { name: '进度 / Progress',               shortName: 'PROGRESS',  cssClass: 'stream-ev-info',    colorScheme: 'blue' };
    EVENT_META[38] = { name: '图快照 / Graph Snapshot',       shortName: 'SNAPSHOT',  cssClass: 'stream-ev-info',    colorScheme: 'lightgray' };

    // ================================================================
    //  流式输出 UI 颜色常量
    //  用于替代 style.cssText 和动态样式设置中的硬编码颜色值
    //  颜色值从 CSS 自定义属性（variables.css）读取，如不存在则回退到默认值。
    // ================================================================

    /**
     * @brief 从 CSS 自定义属性读取颜色值，支持回退默认值
     * @param {string} varName - CSS 变量名（不含 -- 前缀）
     * @param {string} fallback - 回退默认值
     * @returns {string} 颜色值
     */
    function _readCssColor(varName, fallback) {
        try {
            var style = getComputedStyle(document.documentElement);
            var value = style.getPropertyValue('--' + varName).trim();
            return value || fallback;
        } catch (e) {
            return fallback;
        }
    }

    var STREAM_COLORS = {
        success:     _readCssColor('color-streaming-success', '#3fb950'),
        error:       _readCssColor('color-streaming-error', '#f85149'),
        warning:     _readCssColor('color-streaming-warning', '#d29922'),
        info:        _readCssColor('color-streaming-info', '#58a6ff'),
        dim:         _readCssColor('color-streaming-dim', '#8b949e'),
        text:        _readCssColor('color-streaming-text', '#c9d1d9'),
        border:      _readCssColor('color-streaming-border', '#30363d'),
        bgPrimary:   _readCssColor('color-streaming-bg-primary', '#161b22'),
        bgSecondary: _readCssColor('color-streaming-bg-secondary', '#21262d'),
        bgLog:       _readCssColor('color-streaming-bg-log', '#0d1117'),
        bgActive:    _readCssColor('color-streaming-bg-active', '#1f2a3a'),
        bgSuccess:   _readCssColor('color-streaming-bg-success', '#1a2e1f')
    };

    // ================================================================
    //  颜色方案定义
    //  与 C 内核 stream.c 中 STREAM_COLOR_* 宏保持一致
    //  颜色值从 CSS 自定义属性（variables.css）读取
    // ================================================================
    var COLOR_SCHEMES = {
        green:    { text: _readCssColor('color-streaming-scheme-green-text', '#3fb950'),     bg: _readCssColor('color-streaming-scheme-green-bg', '#0d3317') },
        red:      { text: _readCssColor('color-streaming-scheme-red-text', '#f85149'),       bg: _readCssColor('color-streaming-scheme-red-bg', '#3d1214') },
        yellow:   { text: _readCssColor('color-streaming-scheme-yellow-text', '#d29922'),    bg: _readCssColor('color-streaming-scheme-yellow-bg', '#332b0d') },
        orange:   { text: _readCssColor('color-streaming-scheme-orange-text', '#f0883e'),    bg: _readCssColor('color-streaming-scheme-orange-bg', '#331e0d') },
        blue:     { text: _readCssColor('color-streaming-scheme-blue-text', '#58a6ff'),      bg: _readCssColor('color-streaming-scheme-blue-bg', '#0d2c5e') },
        gray:     { text: _readCssColor('color-streaming-scheme-gray-text', '#8b949e'),      bg: _readCssColor('color-streaming-scheme-gray-bg', '#21262d') },
        lightgray:{ text: _readCssColor('color-streaming-scheme-lightgray-text', '#c9d1d9'), bg: _readCssColor('color-streaming-scheme-lightgray-bg', '#161b22') },
        purple:   { text: _readCssColor('color-streaming-scheme-purple-text', '#a371f7'),    bg: _readCssColor('color-streaming-scheme-purple-bg', '#1e0d3d') },
        teal:     { text: _readCssColor('color-streaming-scheme-teal-text', '#39d353'),      bg: _readCssColor('color-streaming-scheme-teal-bg', '#0d3317') }
    };

    // ================================================================
    //  流式桥接器常量（从 constants.js 导入）
    //  中文说明：将硬编码魔法数字替换为从 constants.js 导入的具名常量
    // ================================================================
    /** @constant {Object} 流式模块常量命名空间引用 */
    var sc = Lv00Const.streaming;

    // ================================================================
    //  StreamBridge 构造函数
    //  创建一个流式事件桥接器实例。
    //  中文说明：流式桥接器构造函数，管理事件日志、过滤器、SSE 连接等全部状态。
    //
    //  @param {Object} [app] - Lv00WebApp 实例引用（可选）。
    //                          如果不传，部分功能（如日志回写）将以降级模式运行。
    // ================================================================
    function StreamBridge(app) {
        /** @type {Object|null} Lv00WebApp 实例引用 */
        this._app = app || null;

        /** @type {HTMLElement|null} 流式输出面板的容器 DOM 元素 */
        this._container = null;

        /** @type {number} 事件日志最大保留条数，超过后自动移除最旧条目 */
        this._maxEvents = 200;

        /** @type {Array<Object>} 事件日志环形缓冲区，固定大小避免 shift() O(n) 操作 */
        this._eventLog = new Array(this._maxEvents);
        /** @type {number} 环形缓冲区写入指针（下一个事件写入位置） */
        this._eventHead = 0;
        /** @type {number} 环形缓冲区读取起点（最旧事件的索引） */
        this._eventTail = 0;
        /** @type {number} 环形缓冲区中当前有效事件数 */
        this._eventCount = 0;

        /** @type {Object<number, boolean>} 事件类型过滤器，键为事件类型数值，值为是否显示 */
        this._filters = {};
        // 默认所有已知事件类型均显示
        for (var key in EVENT_TYPES) {
            if (EVENT_TYPES.hasOwnProperty(key)) {
                this._filters[EVENT_TYPES[key]] = true;
            }
        }

        /** @type {boolean} 是否自动滚动到底部 */
        this._autoScroll = true;

        /** @type {boolean} 是否已销毁 */
        this._destroyed = false;

        /** @type {HTMLElement|null} UI 工具栏 DOM 引用 */
        this._uiToolbar = null;

        /** @type {HTMLElement|null} UI 日志内容区 DOM 引用 */
        this._uiLogArea = null;

        /** @type {HTMLElement|null} UI 状态栏 DOM 引用 */
        this._uiStatus = null;

        /** @type {number} 事件计数器，用于生成唯一标识 */
        this._eventCounter = 0;

        // ---- SSE 连接相关状态 ----
        /** @type {EventSource|null} SSE EventSource 实例 */
        this._sseSource = null;
        /** @type {string|null} SSE 连接 URL */
        this._sseUrl = null;
        /** @type {number} SSE 超时时长（毫秒），默认 30 秒 */
        this._sseTimeoutMs = sc.SSE_DEFAULT_TIMEOUT_MS;
        /** @type {number|null} SSE 超时定时器 ID */
        this._sseTimeoutId = null;
        /** @type {number} SSE 重连最大次数，0 表示无限 */
        this._sseMaxRetries = 5;
        /** @type {number} 当前重连计数 */
        this._sseRetryCount = 0;
        /** @type {number} SSE 重连间隔基数（毫秒），指数退避 */
        this._sseRetryBaseMs = sc.SSE_RETRY_BASE_MS;
        /** @type {boolean} 是否开启 SSE 自动重连 */
        this._sseAutoReconnect = true;
    }

    // ================================================================
    //  获取事件类型的元数据信息
    //  如果事件类型未在 EVENT_META 中注册，返回默认元数据。
    //
    //  @param {number} type - 事件类型数值
    //  @returns {{name: string, shortName: string, cssClass: string}}
    // ================================================================
    StreamBridge.prototype._getEventMeta = function(type) {
        if (EVENT_META.hasOwnProperty(type)) {
            return EVENT_META[type];
        }
        // 未知事件类型，返回默认元数据
        return {
            name: '未知事件(' + type + ') / Unknown(' + type + ')',
            shortName: 'EV-' + type,
            cssClass: 'stream-ev-unknown'
        };
    };

    // ================================================================
    //  初始化 UI 面板
    //  在给定的容器 DOM 元素内构建流式输出的完整 UI（工具栏 + 日志区 + 状态栏）。
    //
    //  注意：UI 颜色值已提取到 STREAM_COLORS 常量中，徽章颜色使用 COLOR_SCHEMES。
    //  如需切换主题，修改 STREAM_COLORS 和 COLOR_SCHEMES 即可。
    //
    //  @param {HTMLElement} container - 流式输出面板容器 DOM 元素
    // ================================================================
    StreamBridge.prototype.init = function(container) {
        // 守卫：容器无效则终止
        if (!container) {
            console.warn('[StreamBridge] 初始化失败：容器元素无效');
            return;
        }

        this._container = container;

        // 清空容器现有内容（安全方式：逐个子节点移除）
        while (container.firstChild) {
            container.removeChild(container.firstChild);
        }

        // 应用容器样式
        container.style.cssText = 'display:flex;flex-direction:column;height:100%;overflow:hidden;font-size:11px;font-family:Consolas,monospace;';

        // ---- 构建工具栏 ----
        this._uiToolbar = document.createElement('div');
        this._uiToolbar.style.cssText = 'display:flex;flex-wrap:wrap;gap:3px;align-items:center;padding:3px 4px;border-bottom:1px solid ' + STREAM_COLORS.border + ';background:' + STREAM_COLORS.bgPrimary + ';flex-shrink:0;';

        var self = this;

        // 过滤按钮：按事件类别分组创建切换按钮
        // 分组策略：每个类别只显示一个代表按钮，控制该类别下所有子类型的过滤
        var filterGroups = [
            { label: 'ENG',   types: [0, 1, 2],                          desc: '引擎生命周期' },
            { label: 'NRM',   types: [3, 4, 5],                          desc: '归一化' },
            { label: 'RWT',   types: [6, 7, 8, 9, 10, 11],               desc: '重写引擎' },
            { label: 'SLV',   types: [12, 13, 14, 15, 16],               desc: '代数求解' },
            { label: 'PRF',   types: [17, 18, 19, 20, 21],               desc: '证明系统' },
            { label: 'FBK',   types: [22, 23, 24, 25, 26, 27, 28, 29],  desc: '函数块系统' },
            { label: 'ERR',   types: [30, 31, 32, 33, 34, 35],           desc: '冲突与错误' },
            { label: 'LOG',   types: [36, 37, 38],                        desc: '信息与进度' }
        ];
        var groupColors = [STREAM_COLORS.success, STREAM_COLORS.success, STREAM_COLORS.warning, COLOR_SCHEMES.purple.text, COLOR_SCHEMES.purple.text, COLOR_SCHEMES.teal.text, STREAM_COLORS.error, STREAM_COLORS.dim];

        for (var gi = 0; gi < filterGroups.length; gi++) {
            (function(group, gColor) {
                var filterBtn = document.createElement('button');
                filterBtn.setAttribute('data-filter-group', group.label);
                filterBtn.textContent = group.label;
                filterBtn.title = group.desc;
                filterBtn.style.cssText = 'padding:1px 5px;border:1px solid ' + STREAM_COLORS.border + ';border-radius:3px;background:' + STREAM_COLORS.bgSecondary + ';color:' + STREAM_COLORS.text + ';cursor:pointer;font-size:9px;line-height:1.6;white-space:nowrap;';
                // 默认选中状态
                filterBtn.style.borderColor = gColor;
                filterBtn.style.background = STREAM_COLORS.bgActive;

                // 点击切换该组所有类型事件的过滤状态
                filterBtn.addEventListener('click', function() {
                    // 检查当前是否全部启用
                    var allEnabled = true;
                    for (var ti = 0; ti < group.types.length; ti++) {
                        if (!self._filters[group.types[ti]]) { allEnabled = false; break; }
                    }
                    var newState = !allEnabled;
                    for (var ti = 0; ti < group.types.length; ti++) {
                        self._filters[group.types[ti]] = newState;
                    }
                    if (newState) {
                        filterBtn.style.borderColor = gColor;
                        filterBtn.style.background = STREAM_COLORS.bgActive;
                        filterBtn.style.color = STREAM_COLORS.text;
                    } else {
                        filterBtn.style.borderColor = STREAM_COLORS.border;
                        filterBtn.style.background = STREAM_COLORS.bgSecondary;
                        filterBtn.style.color = STREAM_COLORS.dim;
                    }
                    self._refreshLogDisplay();
                });

                self._uiToolbar.appendChild(filterBtn);
            })(filterGroups[gi], groupColors[gi]);
        }

        // 分隔符
        var sep = document.createElement('span');
        sep.style.cssText = 'flex:1;';
        this._uiToolbar.appendChild(sep);

        // 自动滚动切换按钮
        var autoScrollBtn = document.createElement('button');
        autoScrollBtn.textContent = 'AUTO-SCROLL / 自动滚动';
        autoScrollBtn.style.cssText = 'padding:1px 5px;border:1px solid ' + STREAM_COLORS.border + ';border-radius:3px;background:' + STREAM_COLORS.bgSecondary + ';color:' + STREAM_COLORS.text + ';cursor:pointer;font-size:9px;line-height:1.6;white-space:nowrap;';
        if (this._autoScroll) {
            autoScrollBtn.style.borderColor = STREAM_COLORS.success;
            autoScrollBtn.style.background = STREAM_COLORS.bgSuccess;
        }
        autoScrollBtn.addEventListener('click', function() {
            self._autoScroll = !self._autoScroll;
            if (self._autoScroll) {
                autoScrollBtn.style.borderColor = STREAM_COLORS.success;
                autoScrollBtn.style.background = STREAM_COLORS.bgSuccess;
                self._scrollToBottom();
            } else {
                autoScrollBtn.style.borderColor = STREAM_COLORS.border;
                autoScrollBtn.style.background = STREAM_COLORS.bgSecondary;
            }
        });
        this._uiToolbar.appendChild(autoScrollBtn);

        // 清除日志按钮
        var clearBtn = document.createElement('button');
        clearBtn.textContent = 'CLEAR / 清除';
        clearBtn.style.cssText = 'padding:1px 5px;border:1px solid ' + STREAM_COLORS.border + ';border-radius:3px;background:' + STREAM_COLORS.bgSecondary + ';color:' + STREAM_COLORS.error + ';cursor:pointer;font-size:9px;line-height:1.6;white-space:nowrap;';
        clearBtn.addEventListener('click', function() {
            self.clearLogs();
        });
        this._uiToolbar.appendChild(clearBtn);

        container.appendChild(this._uiToolbar);

        // ---- 构建日志内容区 ----
        this._uiLogArea = document.createElement('div');
        this._uiLogArea.style.cssText = 'flex:1;overflow-y:auto;overflow-x:hidden;padding:4px;background:' + STREAM_COLORS.bgLog + ';';
        this._uiLogArea.setAttribute('role', 'log');
        this._uiLogArea.setAttribute('aria-live', 'polite');
        this._uiLogArea.setAttribute('aria-label', '流式事件日志 / Streaming Event Log');

        // 初始占位提示
        var placeholder = document.createElement('div');
        placeholder.style.cssText = 'color:' + STREAM_COLORS.dim + ';text-align:center;padding:24px 8px;font-size:11px;font-style:italic;';
        placeholder.textContent = '等待流式事件... / Awaiting streaming events...';
        placeholder.setAttribute('data-placeholder', 'true');
        this._uiLogArea.appendChild(placeholder);

        container.appendChild(this._uiLogArea);

        // ---- 构建状态栏 ----
        this._uiStatus = document.createElement('div');
        this._uiStatus.style.cssText = 'display:flex;justify-content:space-between;align-items:center;padding:2px 6px;border-top:1px solid ' + STREAM_COLORS.border + ';background:' + STREAM_COLORS.bgPrimary + ';font-size:9px;color:' + STREAM_COLORS.dim + ';flex-shrink:0;min-height:20px;';
        this._updateStatus(0);
        container.appendChild(this._uiStatus);
    };

    // ================================================================
    //  发射流式事件
    //  将事件对象推送到日志中，并在 UI 面板中实时渲染。
    //
    //  @param {Object} event - 事件对象
    //  @param {number} event.type - 事件类型（-1 到 30 之间的整数）
    //  @param {string} event.description - 事件描述文本
    //  @param {number} [event.step_number] - 步骤编号（可选，默认为 0）
    //  @param {number} [event.node_id] - 关联节点 ID（可选）
    // ================================================================
    StreamBridge.prototype.emit = function(event) {
        // 守卫：已销毁则忽略
        if (this._destroyed) { return; }
        // 守卫：事件对象无效则忽略
        if (!event || typeof event.type !== 'number') { return; }
        // 守卫：事件类型范围校验（允许 -1 用于 SSE 连接事件，0-30 为有效事件类型）
        if (event.type < sc.EVENT_TYPE_MIN || event.type > sc.EVENT_TYPE_MAX) {
            console.warn('[StreamBridge] 忽略无效事件类型: ' + event.type);
            return;
        }

        // 丰富事件对象：补充时间戳和唯一编号
        var enriched = {
            type: event.type,
            description: event.description || '',
            step_number: typeof event.step_number === 'number' ? event.step_number : 0,
            node_id: typeof event.node_id === 'number' ? event.node_id : null,
            timestamp: new Date().toISOString(),
            id: this._eventCounter++
        };

        // 加入日志环形缓冲区（O(1) 写入，无 shift() 的 O(n) 开销）
        this._eventLog[this._eventHead] = enriched;
        this._eventHead = (this._eventHead + 1) % this._maxEvents;

        if (this._eventCount < this._maxEvents) {
            this._eventCount++;
        } else {
            // 缓冲区已满，最旧事件被覆盖，tail 前进
            this._eventTail = (this._eventTail + 1) % this._maxEvents;
        }

        // 如果事件类型未在过滤器中启用，只加到数组但不渲染
        if (this._filters[event.type] !== true) {
            this._updateStatus(this._eventCount);
            return;
        }

        // 渲染该事件到 UI
        this._renderEvent(enriched);
        this._updateStatus(this._eventCount);

        // 自动滚动到底部
        if (this._autoScroll) {
            this._scrollToBottom();
        }

        // 如果关联了 app 实例，尝试回写日志到 app 的日志系统
        if (this._app && typeof this._app.appendLog === 'function') {
            this._app.appendLog('[Stream] ' + event.description, 'info');
        }
    };

    // ================================================================
    //  在 UI 日志区中渲染单个事件条目
    //
    //  @param {Object} event - 已丰富的事件对象
    // ================================================================
    StreamBridge.prototype._renderEvent = function(event) {
        if (!this._uiLogArea) { return; }

        // 移除占位提示（如果它还存在于 DOM 中）
        var placeholder = this._uiLogArea.querySelector('[data-placeholder]');
        if (placeholder) {
            placeholder.parentNode.removeChild(placeholder);
        }

        this._renderEventTo(event, this._uiLogArea);
    };

    // ================================================================
    //  将事件行构建并追加到指定父节点（用于 DocumentFragment 批量插入）
    //
    //  @param {Object} event - 已丰富的事件对象
    //  @param {Node} parent - 目标父节点（HTMLElement 或 DocumentFragment）
    // ================================================================
    StreamBridge.prototype._renderEventTo = function(event, parent) {
        if (!parent) { return; }

        var meta = this._getEventMeta(event.type);

        // 构建事件行容器
        var row = document.createElement('div');
        row.style.cssText = 'display:flex;align-items:flex-start;gap:4px;padding:2px 0;border-bottom:1px solid ' + STREAM_COLORS.bgPrimary + ';line-height:1.5;';
        row.setAttribute('data-event-id', String(event.id));
        row.setAttribute('data-event-type', String(event.type));

        // 时间戳列
        var timeEl = document.createElement('span');
        timeEl.style.cssText = 'color:' + STREAM_COLORS.dim + ';font-size:9px;white-space:nowrap;flex-shrink:0;min-width:60px;';
        timeEl.textContent = this._formatTime(event.timestamp);
        row.appendChild(timeEl);

        // 事件类型徽章
        var badgeEl = document.createElement('span');
        badgeEl.style.cssText = 'display:inline-block;padding:0 4px;border-radius:2px;font-size:9px;font-weight:bold;white-space:nowrap;flex-shrink:0;min-width:56px;text-align:center;';
        badgeEl.textContent = meta.shortName;
        // 根据事件类别应用不同颜色
        this._applyBadgeStyle(badgeEl, event.type);
        row.appendChild(badgeEl);

        // 步骤编号（如果有的话）
        if (typeof event.step_number === 'number') {
            var stepEl = document.createElement('span');
            stepEl.style.cssText = 'color:' + STREAM_COLORS.dim + ';font-size:9px;white-space:nowrap;flex-shrink:0;min-width:32px;text-align:right;';
            stepEl.textContent = '#' + event.step_number;
            row.appendChild(stepEl);
        }

        // 描述文本
        var descEl = document.createElement('span');
        descEl.style.cssText = 'color:' + STREAM_COLORS.text + ';font-size:10px;word-break:break-word;';
        descEl.textContent = event.description;
        row.appendChild(descEl);

        parent.appendChild(row);
    };

    // ================================================================
    //  根据事件类型应用徽章样式
    //  使用 EVENT_META 中的 colorScheme 映射到 COLOR_SCHEMES
    //  与 C 内核 stream.c 中 stream_event_color() 保持一致
    //
    //  @param {HTMLElement} badgeEl - 徽章 DOM 元素
    //  @param {number} type - 事件类型值
    // ================================================================
    StreamBridge.prototype._applyBadgeStyle = function(badgeEl, type) {
        var meta = this._getEventMeta(type);
        var schemeName = meta.colorScheme || 'lightgray';
        var scheme = COLOR_SCHEMES[schemeName] || COLOR_SCHEMES.lightgray;
        badgeEl.style.color = scheme.text;
        badgeEl.style.background = scheme.bg;
    };

    // ================================================================
    //  格式化时间戳为 HH:MM:SS.mmm 显示格式
    //
    //  @param {string} isoString - ISO 8601 时间戳字符串
    //  @returns {string} 格式化后的时间字符串
    // ================================================================
    StreamBridge.prototype._formatTime = function(isoString) {
        if (!isoString) { return '--:--:--'; }
        try {
            var date = new Date(isoString);
            if (isNaN(date.getTime())) { return '--:--:--'; }
            var h = String(date.getHours());
            var m = String(date.getMinutes());
            var s = String(date.getSeconds());
            var ms = String(date.getMilliseconds());
            // 补齐前导零
            if (h.length < 2) { h = '0' + h; }
            if (m.length < 2) { m = '0' + m; }
            if (s.length < 2) { s = '0' + s; }
            while (ms.length < 3) { ms = '0' + ms; }
            return h + ':' + m + ':' + s + '.' + ms;
        } catch (e) {
            return '--:--:--';
        }
    };

    // ================================================================
    //  刷新日志显示（根据当前过滤器重新渲染所有可见事件）
    //  清空日志区，遍历环形缓冲区，仅渲染过滤器启用的类型。
    //  使用 DocumentFragment 批量插入以避免多次 DOM 回流。
    // ================================================================
    StreamBridge.prototype._refreshLogDisplay = function() {
        if (!this._uiLogArea) { return; }

        // 清空日志区（安全方式：逐个子节点移除）
        while (this._uiLogArea.firstChild) {
            this._uiLogArea.removeChild(this._uiLogArea.firstChild);
        }

        // 如果没有事件日志，显示占位提示
        if (this._eventCount === 0) {
            var placeholder = document.createElement('div');
            placeholder.style.cssText = 'color:' + STREAM_COLORS.dim + ';text-align:center;padding:24px 8px;font-size:11px;font-style:italic;';
            placeholder.textContent = '等待流式事件... / Awaiting streaming events...';
            placeholder.setAttribute('data-placeholder', 'true');
            this._uiLogArea.appendChild(placeholder);
            return;
        }

        // 使用 DocumentFragment 批量插入，减少 DOM 回流次数
        var fragment = document.createDocumentFragment();
        var hasVisible = false;

        // 遍历环形缓冲区：从 tail 开始，读取 count 个有效事件
        for (var i = 0; i < this._eventCount; i++) {
            var idx = (this._eventTail + i) % this._maxEvents;
            var evt = this._eventLog[idx];
            if (evt && this._filters[evt.type] === true) {
                hasVisible = true;
                this._renderEventTo(evt, fragment);
            }
        }

        if (hasVisible) {
            this._uiLogArea.appendChild(fragment);
        } else {
            // 如果过滤后无可见事件，显示提示
            var noEventsMsg = document.createElement('div');
            noEventsMsg.style.cssText = 'color:' + STREAM_COLORS.dim + ';text-align:center;padding:24px 8px;font-size:11px;font-style:italic;';
            noEventsMsg.textContent = '所有事件类型已被过滤，不显示任何条目 / All event types filtered';
            this._uiLogArea.appendChild(noEventsMsg);
        }

        if (this._autoScroll) {
            this._scrollToBottom();
        }
    };

    // ================================================================
    //  清除所有事件日志
    //  清空内存中的日志数组和 UI 中的日志区，状态栏归零。
    // ================================================================
    StreamBridge.prototype.clearLogs = function() {
        // 清空内存环形缓冲区（重置指针即可，无需清理数组内容）
        this._eventHead = 0;
        this._eventTail = 0;
        this._eventCount = 0;
        this._eventCounter = 0;

        // 清空 UI 日志区
        if (this._uiLogArea) {
            while (this._uiLogArea.firstChild) {
                this._uiLogArea.removeChild(this._uiLogArea.firstChild);
            }

            // 恢复占位提示
            var placeholder = document.createElement('div');
            placeholder.style.cssText = 'color:' + STREAM_COLORS.dim + ';text-align:center;padding:24px 8px;font-size:11px;font-style:italic;';
            placeholder.textContent = '日志已清除 / Logs cleared';
            placeholder.setAttribute('data-placeholder', 'true');
            this._uiLogArea.appendChild(placeholder);
        }

        // 更新状态栏
        this._updateStatus(0);
    };

    // ================================================================
    //  更新状态栏信息
    //  首次调用时创建 DOM 元素，后续调用直接更新 textContent 避免重建 DOM。
    //
    //  @param {number} totalCount - 总事件条数
    // ================================================================
    StreamBridge.prototype._updateStatus = function(totalCount) {
        if (!this._uiStatus) { return; }

        // 计算可见事件计数（受过滤器影响，遍历环形缓冲区）
        var visibleCount = 0;
        for (var i = 0; i < this._eventCount; i++) {
            var idx = (this._eventTail + i) % this._maxEvents;
            var evt = this._eventLog[idx];
            if (evt && this._filters[evt.type] === true) {
                visibleCount++;
            }
        }

        // 首次调用时创建状态栏子元素，后续直接更新 textContent
        if (this._statusCountEl) {
            this._statusCountEl.textContent = '事件 / Events: ' + totalCount;
            this._statusVisibleEl.textContent = '可见 / Visible: ' + visibleCount;
        } else {
            // 左侧：总事件计数
            this._statusCountEl = document.createElement('span');
            this._statusCountEl.textContent = '事件 / Events: ' + totalCount;
            this._uiStatus.appendChild(this._statusCountEl);

            // 右侧：可见事件计数
            this._statusVisibleEl = document.createElement('span');
            this._statusVisibleEl.textContent = '可见 / Visible: ' + visibleCount;
            this._uiStatus.appendChild(this._statusVisibleEl);
        }
    };

    // ================================================================
    //  滚动日志区到底部
    // ================================================================
    StreamBridge.prototype._scrollToBottom = function() {
        if (this._uiLogArea) {
            this._uiLogArea.scrollTop = this._uiLogArea.scrollHeight;
        }
    };

    // ================================================================
    //  按事件类型筛选
    //  启用或禁用指定类型事件的显示。
    //
    //  @param {number} type - 事件类型数值
    //  @param {boolean} enabled - 是否启用该类型的显示
    // ================================================================
    StreamBridge.prototype.setFilter = function(type, enabled) {
        if (typeof type !== 'number') { return; }
        this._filters[type] = !!enabled;
        this._refreshLogDisplay();
    };

    // ================================================================
    //  获取当前过滤器状态
    //
    //  @returns {Object<number, boolean>} 过滤器状态对象
    // ================================================================
    StreamBridge.prototype.getFilters = function() {
        var result = {};
        for (var key in this._filters) {
            if (this._filters.hasOwnProperty(key)) {
                result[key] = this._filters[key];
            }
        }
        return result;
    };

    // ================================================================
    //  获取所有已记录的事件（按时间顺序）
    //  从环形缓冲区中按 tail->head 顺序收集有效事件。
    //
    //  @returns {Array<Object>} 事件日志数组（浅拷贝）
    // ================================================================
    StreamBridge.prototype.getEvents = function() {
        var result = [];
        for (var i = 0; i < this._eventCount; i++) {
            result.push(this._eventLog[(this._eventTail + i) % this._maxEvents]);
        }
        return result;
    };

    // ================================================================
    //  销毁实例，清理所有资源和 DOM 元素
    //  调用后该实例不应再使用。
    //  中文说明：清理实例所有资源、DOM、SSE 连接和定时器。
    // ================================================================
    StreamBridge.prototype.destroy = function() {
        if (this._destroyed) { return; }

        // 断开 SSE 连接
        this._disconnectSSE();

        // 清理超时定时器
        if (this._sseTimeoutId !== null) {
            clearTimeout(this._sseTimeoutId);
            this._sseTimeoutId = null;
        }

        // 清理 UI
        if (this._container) {
            while (this._container.firstChild) {
                this._container.removeChild(this._container.firstChild);
            }
        }

        // 释放引用
        this._container = null;
        this._uiToolbar = null;
        this._uiLogArea = null;
        this._uiStatus = null;
        this._statusCountEl = null;
        this._statusVisibleEl = null;
        this._eventLog = [];
        this._eventHead = 0;
        this._eventTail = 0;
        this._eventCount = 0;
        this._app = null;
        this._filters = {};
        this._eventCounter = 0;
        this._sseSource = null;
        this._sseUrl = null;
        this._destroyed = true;
    };

    // ================================================================
    //  连接 SSE 端点
    //  中文说明：建立 Server-Sent Events 连接，支持超时检测和自动重连。
    //            使用指数退避策略，最大重连次数可配置。
    //
    //  @param {string} url - SSE 端点 URL
    //  @param {Object} [options] - 可选配置
    //  @param {number} [options.timeout] - 超时时长（毫秒）
    //  @param {number} [options.maxRetries] - 最大重连次数
    //  @param {boolean} [options.autoReconnect] - 是否自动重连
    // ================================================================
    StreamBridge.prototype.connectSSE = function(url, options) {
        var self = this;

        // 守卫：已销毁则忽略
        if (this._destroyed) { return; }
        if (!url) {
            console.warn('[StreamBridge] SSE 连接失败：URL 为空');
            return;
        }

        // 断开现有连接
        this._disconnectSSE();

        // 保存配置
        this._sseUrl = url;
        if (options) {
            if (typeof options.timeout === 'number') { this._sseTimeoutMs = options.timeout; }
            if (typeof options.maxRetries === 'number') { this._sseMaxRetries = options.maxRetries; }
            if (typeof options.autoReconnect === 'boolean') { this._sseAutoReconnect = options.autoReconnect; }
        }
        this._sseRetryCount = 0;

        this._doConnectSSE();
    };

    // ================================================================
    //  执行 SSE 连接（内部方法）
    //  中文说明：创建 EventSource 并绑定事件处理器。
    // ================================================================
    StreamBridge.prototype._doConnectSSE = function() {
        var self = this;

        if (typeof EventSource === 'undefined') {
            console.warn('[StreamBridge] 当前浏览器不支持 EventSource，SSE 不可用');
            return;
        }

        try {
            this._sseSource = new EventSource(this._sseUrl);
        } catch (e) {
            console.error('[StreamBridge] SSE 连接创建失败:', e.message);
            this._handleSSEReconnect();
            return;
        }

        // 消息事件：解析 JSON 后通过 emit 推送到 UI
        this._sseSource.onmessage = function(event) {
            // 收到消息时重置超时计时器
            self._resetSSETimeout();

            try {
                // 解析 SSE 消息数据
                // 安全说明：JSON.parse 本身不会导致原型污染（Object.prototype），
                // 因为 JSON 解析器只创建普通对象，不调用构造函数。
                // 但解析后的数据应通过 emit() 的结构化字段校验后再使用，
                // 避免将不可信的 __proto__/constructor 等字段直接赋值给对象。
                var data = JSON.parse(event.data);
                if (data) {
                    self.emit(data);
                }
            } catch (e) {
                // 非 JSON 消息作为纯文本处理
                self.emit({
                    type: -1,
                    description: event.data || '',
                    step_number: 0
                });
            }
        };

        // 连接打开
        this._sseSource.onopen = function() {
            console.log('[StreamBridge] SSE 连接已建立: ' + self._sseUrl);
            self._sseRetryCount = 0;
            self._resetSSETimeout();
            self.emit({
                type: -1,
                description: 'SSE 连接已建立 / SSE Connected: ' + self._sseUrl,
                step_number: 0
            });
        };

        // 连接错误：自动重连或报告失败
        this._sseSource.onerror = function() {
            console.warn('[StreamBridge] SSE 连接错误: ' + self._sseUrl);
            self._handleSSEReconnect();
        };

        // 启动超时检测
        this._resetSSETimeout();
    };

    // ================================================================
    //  断开 SSE 连接（内部方法）
    //  中文说明：安全关闭 EventSource 并清理超时定时器。
    // ================================================================
    StreamBridge.prototype._disconnectSSE = function() {
        if (this._sseTimeoutId !== null) {
            clearTimeout(this._sseTimeoutId);
            this._sseTimeoutId = null;
        }

        if (this._sseSource) {
            this._sseSource.close();
            this._sseSource = null;
        }
    };

    // ================================================================
    //  重置 SSE 超时定时器
    //  中文说明：每次收到消息时调用，重置超时倒计时。
    // ================================================================
    StreamBridge.prototype._resetSSETimeout = function() {
        var self = this;

        if (this._sseTimeoutId !== null) {
            clearTimeout(this._sseTimeoutId);
            this._sseTimeoutId = null;
        }

        this._sseTimeoutId = setTimeout(function() {
            console.warn('[StreamBridge] SSE 超时，断开连接');
            self._disconnectSSE();
            self.emit({
                type: -1,
                description: 'SSE 连接超时 / SSE Connection Timeout',
                step_number: 0
            });
            self._handleSSEReconnect();
        }, this._sseTimeoutMs);
    };

    // ================================================================
    //  处理 SSE 重连逻辑（内部方法）
    //  中文说明：使用指数退避策略自动重连，直到达到最大重连次数。
    //            重连间隔 = 基数 * 2^已重连次数，最大 30 秒。
    // ================================================================
    StreamBridge.prototype._handleSSEReconnect = function() {
        var self = this;

        if (!this._sseAutoReconnect || this._destroyed || !this._sseUrl) return;

        // 检查重连次数限制
        if (this._sseMaxRetries > 0 && this._sseRetryCount >= this._sseMaxRetries) {
            console.warn('[StreamBridge] SSE 重连已达最大次数 (' + this._sseMaxRetries + ')，停止重连');
            this.emit({
                type: -1,
                description: 'SSE 重连失败，已达最大重连次数 / Max retries reached',
                step_number: 0
            });
            return;
        }

        this._sseRetryCount++;
        // 指数退避：base * 2^retryCount，最大 30 秒
        var delay = Math.min(this._sseRetryBaseMs * Math.pow(2, this._sseRetryCount - 1), sc.SSE_RETRY_MAX_DELAY_MS);

        console.log('[StreamBridge] SSE 将在 ' + delay + 'ms 后重连 (第 ' + this._sseRetryCount + ' 次)');
        this.emit({
            type: -1,
            description: 'SSE 将在 ' + (delay / 1000).toFixed(1) + 's 后重连 (第 ' + this._sseRetryCount + ' 次)',
            step_number: 0
        });

        setTimeout(function() {
            if (!self._destroyed && self._sseUrl) {
                self._doConnectSSE();
            }
        }, delay);
    };

    // ================================================================
    //  获取 SSE 连接状态
    //  中文说明：返回当前 SSE 连接的详细信息。
    //  @returns {Object} SSE 连接状态
    // ================================================================
    StreamBridge.prototype.getSSEStatus = function() {
        return {
            connected: !!(this._sseSource && this._sseSource.readyState === EventSource.OPEN),
            url: this._sseUrl,
            retryCount: this._sseRetryCount,
            maxRetries: this._sseMaxRetries
        };
    };

    // ================================================================
    //  全局导出
    //  挂载到 window.Lv00Streaming，与 integrate-all.js 的引用方式一致
    // ================================================================
    window.Lv00Streaming = {
        StreamBridge: StreamBridge,
        EVENT_TYPES: EVENT_TYPES
    };

})();
