/**
 * ============================================================================
 *  Lv-00 集成脚本 - 模块标签与面板加载器（v2.0 优化版）
 * ============================================================================
 *
 *  中文说明：集成脚本主入口，负责动态加载扩展模块（Help/Assistant/GitHub），
 *             向导航栏注入标签按钮，并通过 fetch 安全加载面板 HTML 内容。
 *             与 ui.js 的模块切换逻辑统一协作，不重复实现切换逻辑。
 *
 *  架构说明：
 *    - 模块切换核心逻辑由 ui.js 中的 Lv00WebApp.prototype.switchModule 统一处理
 *    - 本脚本只负责"加载"面板和"添加"标签，不重复实现切换
 *    - 切换时通过委托回 Lv00WebApp 实例或直接操作 DOM 类名
 *
 *  ⚠ 与 help-panel.html 的职责分工 —— 避免功能重复：
 *  ┌──────────────────────────────────────────────────────────────────┐
 *  │ 本文件（integrate-all.js）角色：                                  │
 *  │   统一的扩展管理器 —— 批量管理 help/assistant/github 三个模块     │
 *  │   _injectTabs()  ➜ 向导航栏注入全部扩展标签                       │
 *  │   _fetchPanel()  ➜ 通过 fetch 加载各个面板 HTML 并安全注入        │
 *  │                                                                  │
 *  │ help-panel.html 内联脚本角色：                                    │
 *  │   帮助面板自包含模块 —— 当 help-panel.html 被独立 fetch 加载时    │
 *  │   addTab()      ➜ 独立注入 Help 标签（与 _injectTabs 互斥）      │
 *  │   loadPanel()   ➜ 独立加载 Help 面板内容（与 _fetchPanel 互斥）   │
 *  │                                                                  │
 *  │ 互斥机制：                                                        │
 *  │   两套方法都通过 querySelector/getElementById 检查 DOM 中是否      │
 *  │   已存在目标元素，先到者注入，后来者自动跳过。因此无论哪方先执行   │
 *  │   都不会产生重复标签或面板。                                      │
 *  └──────────────────────────────────────────────────────────────────┘
 *
 *  语法标准：严格 ES5（与项目整体一致）
 *  依赖：js/constants.js（全局常量配置）
 *  作者：Lv-00 Team
 *  创建日期：2026-05-20
 *  版本：2.0.0
 * ============================================================================
 */

(function() {
    'use strict';

    // ========================================================================
    //  全局命名空间（挂载到 window 以避免污染全局作用域）
    //  外部可通过 window.__lv00Extension 访问
    // ========================================================================
    var _Lv00Ext = {
        /**
         * 标记是否已初始化，防止重复注入
         * @type {boolean}
         */
        _initialized: false,

        /**
         * 流式桥接器引用
         * @type {Object|null}
         */
        _streamBridge: null,

        /**
         * 模块加载超时时间（毫秒），默认 10 秒
         * 从 Lv00Const.integrate.LOAD_TIMEOUT 导入
         * @type {number}
         */
        _loadTimeout: Lv00Const.integrate.LOAD_TIMEOUT,

        /**
         * 扩展模块定义列表
         * 每个模块包含：module（标识符）、icon（图标文字）、label（按钮文字）、panelFile（HTML文件路径）
         *
         * 注意：help 模块的面板文件 help-panel.html 内含独立的内联脚本（addTab/loadPanel），
         * 可由 integrate-all.js 的 _fetchPanel 加载，也可独立运行。两套注入逻辑互斥，不产生重复。
         * @type {Array<Object>}
         */
        _extensions: [
            { module: 'help',      icon: 'H',  label: 'Help',      panelFile: 'help-panel.html' },
            { module: 'assistant',  icon: 'AI', label: 'Assistant', panelFile: 'coding-assistant.html' },
            { module: 'github',     icon: 'GH', label: 'GitHub',    panelFile: 'github-demo.html' }
        ],

        /**
         * 主入口：初始化所有扩展功能
         * 执行顺序：加载 GitHub 集成 JS → 添加标签 → 加载面板 → 绑定事件
         */
        init: function() {
            // 防护：避免重复初始化
            if (this._initialized) { return; }
            this._initialized = true;

            console.log('[Lv-00] 扩展模块初始化中...');

            var self = this;

            // 步骤1: 加载 GitHub 集成模块（异步），带超时和进度提示
            this._showLoadingStatus('正在加载 GitHub 集成...');
            this._loadScriptWithTimeout('github-integrations.js', function(success, timedOut) {
                if (timedOut) {
                    console.warn('[Lv-00] GitHub 集成模块加载超时（继续初始化）');
                } else if (success) {
                    console.log('[Lv-00] GitHub 集成模块已加载');
                    if (window.Lv00Libraries && typeof window.Lv00Libraries.initAll === 'function') {
                        window.Lv00Libraries.initAll()
                            .then(function() {
                                console.log('[Lv-00] Libraries initialized successfully');
                            })
                            .catch(function(err) {
                                console.error('[Lv-00] GitHub 库初始化失败:', err);
                            });
                    }
                } else {
                    console.warn('[Lv-00] GitHub 集成模块加载失败（继续初始化）');
                }

                // 步骤2: 添加扩展标签到导航栏
                self._injectTabs();

                // 步骤3: 加载面板 HTML 内容（带进度提示）
                self._loadPanelsWithProgress();

                // 步骤4: 初始化流式输出桥接器
                self._initStreaming();
            });
        },

        // ====================================================================
        //  在导航栏显示加载状态提示
        //  创建或更新一个临时的加载状态元素，显示在 .header-actions 区域内。
        //  @param {string} message - 加载提示文字
        // ====================================================================
        _showLoadingStatus: function(message) {
            var container = document.querySelector('.header-actions');
            if (!container) { return; }

            var statusEl = document.getElementById('__lv00LoadingStatus');
            if (!statusEl) {
                statusEl = document.createElement('span');
                statusEl.id = '__lv00LoadingStatus';
                statusEl.className = 'header-loading-status';
                // 插入到 header-actions 最前面
                container.insertBefore(statusEl, container.firstChild);
            }
            statusEl.textContent = message;
            statusEl.style.display = 'inline-block';
        },

        // ====================================================================
        //  隐藏导航栏的加载状态提示
        //  延迟 500ms 后淡出并移除，避免闪烁。
        // ====================================================================
        _hideLoadingStatus: function() {
            var statusEl = document.getElementById('__lv00LoadingStatus');
            if (!statusEl) { return; }

            // 添加淡出效果
            statusEl.style.opacity = '0';
            setTimeout(function() {
                if (statusEl.parentNode) {
                    statusEl.parentNode.removeChild(statusEl);
                }
            }, 500);
        },

        // ====================================================================
        //  带超时的脚本加载
        //  在 _loadScript 基础上增加超时机制，超时后回调 timedOut=true。
        //  @param {string} src - 脚本文件路径
        //  @param {function(boolean, boolean)} callback - 回调（success, timedOut）
        // ====================================================================
        _loadScriptWithTimeout: function(src, callback) {
            var self = this;
            var timedOut = false;
            var completed = false;

            // 设置超时定时器
            var timer = setTimeout(function() {
                if (!completed) {
                    timedOut = true;
                    completed = true;
                    self._showLoadingStatus('加载超时: ' + src);
                    setTimeout(function() { callback(false, true); }, 300);
                }
            }, this._loadTimeout);

            this._loadScript(src, function(success) {
                if (completed) { return; } // 已超时，忽略后续回调
                completed = true;
                clearTimeout(timer);
                callback(success, false);
            });
        },

        // ====================================================================
        //  动态加载外部脚本
        //  创建 <script> 标签并注入到 <head>，支持加载成功/失败回调。
        //  @param {string} src - 脚本文件路径
        //  @param {function(boolean)} callback - 加载完成回调（success 参数，true 成功，false 失败）
        // ====================================================================
        _loadScript: function(src, callback) {
            var script = document.createElement('script');
            script.src = src;
            script.onload = function() { callback(true); };
            script.onerror = function() { callback(false); };
            document.head.appendChild(script);
        },

        // ====================================================================
        //  向模块标签栏注入扩展标签按钮
        //  查找 .module-tabs 容器，按顺序插入 help/assistant/github 标签。
        //  help 标签插入最前面，assistant/github 插入 Debug 之前。
        //  自动跳过已存在的标签，防止重复注入。
        //
        //  与 help-panel.html 的 addTab() 互斥：
        //   二者都会尝试注入 Help 标签，通过 querySelector 检查已存在标签，
        //   先到者注入，后来者自动跳过。
        // ====================================================================
        _injectTabs: function() {
            var tabContainer = document.querySelector('.module-tabs');
            if (!tabContainer) {
                console.warn('[Lv-00] 未找到 .module-tabs 容器，标签注入跳过');
                return;
            }

            var extensions = this._extensions;

            for (var i = 0; i < extensions.length; i++) {
                var ext = extensions[i];

                // 防止重复注入
                if (document.querySelector('.module-tab[data-module="' + ext.module + '"]')) {
                    continue;
                }

                var tab = this._createTab(ext);

                // help 标签插入到最前面（在 Formula 标签之前）
                if (ext.module === 'help') {
                    var firstTab = tabContainer.querySelector('.module-tab');
                    if (firstTab) {
                        tabContainer.insertBefore(tab, firstTab);
                    } else {
                        tabContainer.appendChild(tab);
                    }
                } else {
                    // assistant/github 插入到 Debug 标签之前
                    var debugTab = tabContainer.querySelector('[data-module="debug"]');
                    if (debugTab) {
                        tabContainer.insertBefore(tab, debugTab);
                    } else {
                        tabContainer.appendChild(tab);
                    }
                }

                console.log('[Lv-00] ' + ext.label + ' 标签已注入');
            }
        },

        // ====================================================================
        //  创建单个模块标签 DOM 元素
        //  @param {Object} ext - 扩展模块定义
        //  @returns {HTMLElement} 模块标签按钮元素
        // ====================================================================
        _createTab: function(ext) {
            var self = this;  // 捕获 _Lv00Ext 内部引用，避免在回调中直接引用外部变量

            var tab = document.createElement('button');
            tab.className = 'module-tab';
            tab.setAttribute('data-module', ext.module);
            tab.setAttribute('data-tooltip', ext.label + ' / ' + ext.label);

            // 使用 span 构建标签内容（与现有标签结构一致）
            var iconSpan = document.createElement('span');
            iconSpan.className = 'tab-icon';
            iconSpan.textContent = ext.icon;
            tab.appendChild(iconSpan);
            tab.appendChild(document.createTextNode(ext.label));

            // 绑定点击事件：委托给 Lv00WebApp 实例的 switchModule 方法
            // 如果实例不可用，则回退到直接 DOM 操作
            (function(moduleName) {
                tab.addEventListener('click', function() {
                    // 优先使用 Lv00WebApp 实例的方法
                    var app = window.lv00App;
                    if (app && typeof app.switchModule === 'function') {
                        app.switchModule(moduleName);
                    } else {
                        // 回退方案：直接操作 DOM
                        self._switchModuleFallback(moduleName);
                    }
                });
            })(ext.module);

            return tab;
        },

        // ====================================================================
        //  回退方案：直接 DOM 操作实现模块切换（不依赖 Lv00WebApp 实例）
        //  1. 移除所有标签的 active 状态
        //  2. 激活目标标签并显示对应面板，隐藏其他面板
        //  3. 如果面板尚未加载，尝试延迟加载
        //  @param {string} moduleName - 模块名称
        // ====================================================================
        _switchModuleFallback: function(moduleName) {
            // 移除所有标签的 active 状态
            var tabs = document.querySelectorAll('.module-tab');
            for (var i = 0; i < tabs.length; i++) {
                tabs[i].classList.remove('active');
            }

            // 激活目标标签
            var targetTab = document.querySelector('.module-tab[data-module="' + moduleName + '"]');
            if (targetTab) { targetTab.classList.add('active'); }

            // 隐藏所有面板
            var panels = document.querySelectorAll('.module-panel');
            var found = false;
            for (var j = 0; j < panels.length; j++) {
                var panel = panels[j];
                var panelName = panel.id.replace(/^panel/, '').toLowerCase();
                if (panelName === moduleName) {
                    panel.classList.remove('panel-hidden');
                    found = true;
                } else {
                    panel.classList.add('panel-hidden');
                }
            }

            // 如果面板尚未加载，尝试延迟加载
            // 修复：从 _extensions 数组中查找对应扩展对象，而非直接传入字符串 moduleName
            if (!found) {
                var exts = this._extensions;
                for (var k = 0; k < exts.length; k++) {
                    if (exts[k].module === moduleName) {
                        this._loadSinglePanel(exts[k]);
                        break;
                    }
                }
            }

            console.log('[Lv-00] 模块切换（回退模式）: ' + moduleName);
        },

        // ====================================================================
        //  加载所有扩展面板的 HTML 内容
        //  遍历 _extensions 数组，对每个扩展模块调用 _loadSinglePanel。
        //  通过 fetch 请求 HTML 文件，解析后注入到右侧边栏。
        //
        //  与 help-panel.html 的协调：
        //   当 _loadSinglePanel('help') 调用 _fetchPanel('help-panel.html') 时，
        //   会加载 help-panel.html 的 HTML 内容及其内联脚本。help-panel.html 的
        //   addTab/loadPanel 会在检测到已存在元素时自动跳过，由本管理器兜底。
        // ====================================================================
        _loadPanels: function() {
            // 使用 Promise.all 并行加载所有面板，提高加载效率
            // _loadSinglePanel 现返回 Promise，支持并行化
            var extensions = this._extensions;
            var self = this;
            var promises = [];
            for (var i = 0; i < extensions.length; i++) {
                promises.push(self._loadSinglePanel(extensions[i]));
            }
            Promise.all(promises).then(function() {
                console.log('[Lv-00] 所有面板并行加载完成');
            }).catch(function(err) {
                console.error('[Lv-00] 面板并行加载出错:', err);
            });
        },

        // ====================================================================
        //  带进度提示的面板加载
        //  逐个加载扩展面板，每次加载前更新导航栏的加载状态提示，
        //  全部加载完成后自动隐藏提示。
        // ====================================================================
        _loadPanelsWithProgress: function() {
            var extensions = this._extensions;
            var self = this;
            var index = 0;

            function loadNext() {
                if (index >= extensions.length) {
                    // 全部加载完成，隐藏进度提示
                    self._hideLoadingStatus();
                    return;
                }
                var ext = extensions[index];
                self._showLoadingStatus('正在加载 ' + ext.label + ' 面板...');
                index++;

                // 使用带超时的面板加载
                self._loadSinglePanelWithTimeout(ext, function() {
                    loadNext();
                });
            }

            loadNext();
        },

        // ====================================================================
        //  带超时的单面板加载
        //  在 _loadSinglePanel 基础上增加超时机制。
        //  @param {Object} ext - 扩展模块定义
        //  @param {function} [callback] - 加载完成回调（无论成功或失败）
        // ====================================================================
        _loadSinglePanelWithTimeout: function(ext, callback) {
            var panelId = 'panel' + ext.module.charAt(0).toUpperCase() + ext.module.slice(1);
            if (document.getElementById(panelId)) {
                if (callback) { callback(); }
                return;
            }

            var sidebar = document.querySelector('.sidebar-right') || document.getElementById('sidebarRight');
            if (!sidebar) {
                console.warn('[Lv-00] 未找到右侧边栏，面板 ' + ext.module + ' 加载跳过');
                if (callback) { callback(); }
                return;
            }

            // 使用带超时的 fetchPanel
            this._fetchPanelWithTimeout(ext.panelFile, panelId, ext.module, callback);
        },

        // ====================================================================
        //  加载单个面板的 HTML 内容
        //  首先检查面板是否已存在或已有占位 div。如果不存在，通过 fetch 获取
        //  对应的 HTML 文件，解析 DOM 后注入到右侧边栏中。
        //  @param {Object} ext - 扩展模块定义，包含 module 和 panelFile 属性
        //
        //  防重复检查：通过 getElementById 判断面板是否已由 help-panel.html
        //  的 loadPanel() 提前注入，如果是则跳过本方法。
        // ====================================================================
        _loadSinglePanel: function(ext) {
            var self = this;

            // 返回 Promise 以支持 _loadPanels 的 Promise.all 并行化
            return new Promise(function(resolve) {
                // 如果面板已存在，跳过
                var panelId = 'panel' + ext.module.charAt(0).toUpperCase() + ext.module.slice(1);
                if (document.getElementById(panelId)) { resolve(false); return; }

                // 智能检测：help-panel.html 可能已自行加载并设置全局标志
                // help-panel.html 内联脚本的 loadPanel() 执行后会设置 window.__helpPanelLoaded = true
                // 此处检测该标志，避免 integrate-all.js 对 help 面板进行重复加载
                if (ext.module === 'help' && window.__helpPanelLoaded) {
                    console.log('[Lv-00] Help 面板已由 help-panel.html 自行加载，跳过并行加载');
                    resolve(false);
                    return;
                }

                // 获取目标位置：右侧边栏
                var sidebar = document.querySelector('.sidebar-right') || document.getElementById('sidebarRight');
                if (!sidebar) {
                    console.warn('[Lv-00] 未找到右侧边栏，面板 ' + ext.module + ' 加载跳过');
                    resolve(false);
                    return;
                }

                // 通过 fetch 加载面板 HTML（注意：line ~416 已通过 getElementById 检查了面板是否存在，
                // 如果存在则提前返回，故此处无需重复检查 existingPanel）
                self._fetchPanel(ext.panelFile, panelId, ext.module, function(success) {
                    resolve(success);
                });
            });
        },

        // ====================================================================
        //  初始化流式输出桥接器（事件驱动版）
        //  使用 MutationObserver 监听 DOM 变化，等待 streamingContainer 元素出现，
        //  替代旧的 setTimeout 轮询方式，避免浪费 CPU 资源。
        //  一旦容器就绪，立即调用 _initStreamingBridge 创建 StreamBridge 实例。
        // ====================================================================
        _initStreaming: function() {
            var self = this;

            // 首先尝试直接获取容器（大多数情况下已就绪）
            var container = document.getElementById('streamingContainer');
            if (container) {
                self._initStreamingBridge(container);
                return;
            }

            // 容器尚未存在于 DOM，使用 MutationObserver 监听其出现
            // 这比 setTimeout 轮询更高效，由浏览器在 DOM 变化时主动回调
            var observer = new MutationObserver(function(mutations, obs) {
                var c = document.getElementById('streamingContainer');
                if (c) {
                    obs.disconnect();  // 找到后立即停止观察
                    self._initStreamingBridge(c);
                }
            });

            // 观察整个文档子树的变化
            observer.observe(document.body || document.documentElement, {
                childList: true,
                subtree: true
            });

            // 后备超时机制：10 秒后若仍未找到，断开观察器并告警
            // 防止极端情况下 MutationObserver 一直等待
            setTimeout(function() {
                observer.disconnect();
                console.warn('[Lv-00] streamingContainer 未在超时时间内出现（10秒），流式输出未初始化');
            }, 10000);
        },

        // ====================================================================
        //  流式桥接器初始化核心逻辑
        //  从 _initStreaming 中抽离，当 streamingContainer DOM 元素就绪后被调用。
        //  创建 StreamBridge 实例并安装 JS 后端事件钩子。
        //  @param {HTMLElement} container - streamingContainer DOM 元素
        // ====================================================================
        _initStreamingBridge: function(container) {
            var self = this;
            var app = window.lv00App;

            if (!app) {
                console.warn('[Lv-00] Lv00WebApp 实例不可用，流式输出在有限模式下运行');
            }

            // 创建 StreamBridge
            if (window.Lv00Streaming && window.Lv00Streaming.StreamBridge) {
                self._streamBridge = new window.Lv00Streaming.StreamBridge(app || {});
                self._streamBridge.init(container);

                // 添加JS后端事件钩子
                self._hookJSBackendStreaming();

                // 暴露到全局以便其他模块使用
                window.lv00StreamBridge = self._streamBridge;

                console.log('[Lv-00] 流式输出桥接器已初始化');
            } else {
                console.warn('[Lv-00] Lv00Streaming 模块未加载，流式输出不可用');
            }
        },

        // ====================================================================
        //  钩入 JS 后端以模拟流式事件
        //  当使用纯 JS 后端（非 WASM）时，覆盖 app 的关键方法，
        //  在每个操作前后发射模拟流式事件。
        //  覆盖的操作：graphNormalize、engineSolve、graphRewrite、
        //              addPoint、addLineSegment、addConstraint
        //  事件类型与 C 内核 stream.h StreamEventType 枚举一一对应。
        // ====================================================================
        _hookJSBackendStreaming: function() {
            var bridge = this._streamBridge;
            if (!bridge) return;

            /* 钩入 normalize 操作 */
            var app = window.lv00App;
            if (!app) return;

            /* 保存原始方法的引用 */
            var origNormalize = app.graphNormalize;
            var origSolve = app.engineSolve;
            var origRewrite = app.graphRewrite;
            var origAddPoint = app.addPoint;
            var origAddLine = app.addLineSegment;
            var origAddConstraint = app.addConstraint;

            /* 覆盖 normalize 以添加流式事件 */
            if (typeof origNormalize === 'function') {
                app.graphNormalize = function() {
                    bridge.emit({
                        type: 3,  /* NORMALIZE_START */
                        description: '开始图规范化 (JS后端)',
                        step_number: 0
                    });

                    var result = origNormalize.apply(this, arguments);

                    bridge.emit({
                        type: 5,  /* NORMALIZE_DONE */
                        description: '图规范化完成 (JS后端)',
                        step_number: 0,
                        node_id: result ? (result.merged_count || 0) : -1
                    });

                    return result;
                };
            }

            /* 覆盖 solve 以添加流式事件 */
            if (typeof origSolve === 'function') {
                app.engineSolve = function() {
                    bridge.emit({
                        type: 0,  /* ENGINE_START */
                        description: '求解流程启动 (JS后端)',
                        step_number: 0
                    });

                    bridge.emit({
                        type: 12, /* SOLVE_START */
                        description: '开始代数求解 (JS后端)',
                        step_number: 1
                    });

                    var result = origSolve.apply(this, arguments);

                    bridge.emit({
                        type: 16, /* SOLVE_DONE */
                        description: '代数求解完成 (JS后端)',
                        step_number: 2
                    });

                    bridge.emit({
                        type: 1,  /* ENGINE_DONE */
                        description: '求解流程完成 (JS后端)',
                        step_number: 3
                    });

                    return result;
                };
            }

            /* 覆盖 rewrite 以添加流式事件 */
            if (typeof origRewrite === 'function') {
                app.graphRewrite = function() {
                    bridge.emit({
                        type: 6,  /* REWRITE_START */
                        description: '开始重写阶段 (JS后端)',
                        step_number: 0
                    });

                    var result = origRewrite.apply(this, arguments);

                    bridge.emit({
                        type: 11, /* REWRITE_DONE */
                        description: '重写阶段完成 (JS后端)',
                        step_number: 0
                    });

                    return result;
                };
            }

            /* 覆盖 addPoint 以发射 NODE_ADDED 事件 */
            if (typeof origAddPoint === 'function') {
                app.addPoint = function() {
                    var result = origAddPoint.apply(this, arguments);
                    if (result && result.id !== undefined) {
                        bridge.emit({
                            type: 24, /* NODE_ADDED */
                            description: '添加点 #' + result.id + ' (JS后端)',
                            step_number: 0,
                            node_id: result.id
                        });
                    }
                    return result;
                };
            }

            /* 覆盖 addLineSegment 以发射 NODE_ADDED 事件 */
            if (typeof origAddLine === 'function') {
                app.addLineSegment = function() {
                    var result = origAddLine.apply(this, arguments);
                    if (result && result.id !== undefined) {
                        bridge.emit({
                            type: 24, /* NODE_ADDED */
                            description: '添加线段 #' + result.id + ' (JS后端)',
                            step_number: 0,
                            node_id: result.id
                        });
                    }
                    return result;
                };
            }

            /* 覆盖 addConstraint 以发射 CONSTRAINT_ADDED 事件 */
            if (typeof origAddConstraint === 'function') {
                app.addConstraint = function() {
                    var result = origAddConstraint.apply(this, arguments);
                    if (result && result.id !== undefined) {
                        bridge.emit({
                            type: 23, /* CONSTRAINT_ADDED */
                            description: '添加约束 #' + result.id + ' (JS后端)',
                            step_number: 0,
                            constraint_id: result.id
                        });
                    }
                    return result;
                };
            }

            console.log('[Lv-00] JS后端流式事件钩子已安装（覆盖 normalize/solve/rewrite/addPoint/addLine/addConstraint）');
        },
        // ====================================================================
        //  安全克隆 DOM 内容（辅助方法）
        //  使用 cloneNode 代替 innerHTML 以避免 XSS 风险。
        //  清空目标元素后，逐一克隆源元素的所有子节点并追加。
        //  @param {HTMLElement} target - 目标 DOM 元素
        //  @param {HTMLElement} source - 源 DOM 元素
        // ====================================================================
        _safeCloneContent: function(target, source) {
            // 清空目标元素
            while (target.firstChild) {
                target.removeChild(target.firstChild);
            }
            // 安全克隆源元素的所有子节点（cloneNode(true) 深拷贝）
            var children = source.childNodes;
            for (var i = 0; i < children.length; i++) {
                target.appendChild(children[i].cloneNode(true));
            }
        },

        /**
         * 获取面板内容（通过 fetch 加载 HTML）
         * 中文说明：通过 fetch 请求面板 HTML 文件，使用 DOMParser 安全解析，
         *            通过 cloneNode 注入 DOM（避免 innerHTML XSS），
         *            提取内联 script 通过 textContent 安全执行。
         * @param {string} url - 面板 HTML 文件的 URL
         * @param {string} panelId - 面板 DOM 元素 ID（如 "panelHelp"）
         * @param {string} moduleName - 模块名称（用于日志输出）
         * @param {function} [callback] - 加载完成回调（无论成功或失败）
         *
         * 流程：
         *   1. 使用 fetch 加载 HTML 文件
         *   2. 通过 DOMParser 解析 HTML 内容
         *   3. 使用 safeCloneContent（cloneNode）安全注入内容，避免 innerHTML XSS
         *   4. 提取内联的 <script> 标签，通过 textContent 安全注入执行
         *   5. 加载失败时显示友好错误提示（含重试按钮）
         *
         * 与 help-panel.html 的 loadPanel() 互斥：
         *   二者都通过 getElementById 检查 panelHelp 是否已存在，
         *   先到者注入，后来者自动跳过。
         */
        _fetchPanel: function(url, panelId, moduleName, callback) {
            var self = this;

            fetch(url)
                .then(function(response) {
                    if (!response.ok) {
                        throw new Error('HTTP ' + response.status + ' ' + response.statusText);
                    }
                    return response.text();
                })
                .then(function(html) {
                    var parser = new DOMParser();
                    var doc = parser.parseFromString(html, 'text/html');

                    // 查找目标面板元素
                    var panelContent = doc.getElementById(panelId) || doc.querySelector('.module-panel');
                    var targetEl = document.getElementById(panelId);

                    if (targetEl && panelContent) {
                        // 使用安全 DOM 克隆代替 innerHTML
                        self._safeCloneContent(targetEl, panelContent);
                        targetEl.classList.add('module-panel');
                    } else if (targetEl && !panelContent) {
                        // 使用整个 body 内容
                        var bodyContent = doc.querySelector('body');
                        if (bodyContent) {
                            self._safeCloneContent(targetEl, bodyContent);
                        }
                        targetEl.classList.add('module-panel');
                    } else if (panelContent) {
                        // 目标面板不存在，创建新面板并追加到侧边栏
                        var sidebar = document.querySelector('.sidebar-right');
                        if (sidebar) {
                            var wrapper = document.createElement('div');
                            wrapper.id = panelId;
                            wrapper.className = 'module-panel panel-hidden';
                            self._safeCloneContent(wrapper, panelContent);
                            sidebar.appendChild(wrapper);
                        }
                    }

                    // XSS 安全说明：此处处理内联脚本的注入方式与安全性
                    //
                    // 使用 textContent（而非 innerHTML）设置脚本内容，原因：
                    //   1. textContent 将内容视为纯文本，不经过 HTML 解析器
                    //   2. 即使脚本内容包含 HTML 标签，也不会被解析为 DOM 节点
                    //   3. 符合 OWASP XSS 防护规范——避免将不受信任的数据注入 innerHTML
                    //
                    // 安全边界：
                    //   - 脚本内容来自 fetch 加载的本地 HTML 文件（同源策略保护）
                    //   - 使用 DOMParser 解析 HTML 时，仅提取 textContent，不执行内联脚本
                    //   - 通过创建全新 <script> 元素并设置 textContent（而非 innerHTML）来控制执行
                    //   - 浏览器在 script.textContent 赋值时不会触发 HTML 解析
                    //
                    // 潜在风险（已缓解）：
                    //   - 若 fetch 的 HTML 文件被篡改（MITM），HTTPS 的完整性校验会防止此问题
                    //   - 若文件本身包含恶意脚本，由 CSP (Content-Security-Policy) 头进行限制
                    var scripts = doc.querySelectorAll('script');
                    for (var i = 0; i < scripts.length; i++) {
                        if (scripts[i].textContent && scripts[i].textContent.trim()) {
                            var newScript = document.createElement('script');
                            newScript.textContent = scripts[i].textContent;
                            document.head.appendChild(newScript);
                        }
                    }

                    console.log('[Lv-00] ' + moduleName + ' 面板已加载');

                    // 智能检测标志：若成功加载 help 面板，设置全局标志
                    // help-panel.html 内联脚本也可能设置此标志，二者互补
                    // 用于 _loadSinglePanel 中判断是否跳过重复加载
                    if (moduleName === 'help') {
                        window.__helpPanelLoaded = true;
                    }

                    if (callback) { callback(true); }
                })
                .catch(function(err) {
                    console.error('[Lv-00] 面板加载失败 (' + url + '):', err);
                    self._showPanelError(panelId, moduleName, url, err, callback);
                });
        },

        // ====================================================================
        //  带超时的面板加载
        //  在 _fetchPanel 基础上增加超时机制，超时后显示超时提示。
        //  @param {string} url - 面板 HTML 文件的 URL
        //  @param {string} panelId - 面板 DOM 元素 ID
        //  @param {string} moduleName - 模块名称
        //  @param {function} [callback] - 加载完成回调
        // ====================================================================
        _fetchPanelWithTimeout: function(url, panelId, moduleName, callback) {
            var self = this;
            var completed = false;

            // 设置超时定时器
            var timer = setTimeout(function() {
                if (!completed) {
                    completed = true;
                    var timeoutErr = new Error('加载超时（' + (self._loadTimeout / 1000) + '秒）');
                    console.error('[Lv-00] 面板加载超时 (' + url + ')');
                    self._showPanelError(panelId, moduleName, url, timeoutErr, callback, true);
                }
            }, this._loadTimeout);

            // 包装原始回调，确保超时后不再触发
            var wrappedCallback = function(success) {
                if (completed) { return; }
                completed = true;
                clearTimeout(timer);
                if (callback) { callback(success); }
            };

            this._fetchPanel(url, panelId, moduleName, wrappedCallback);
        },

        // ====================================================================
        //  显示友好的面板加载错误提示（含重试按钮）
        //  使用 DOM API 安全构建错误界面，包含错误描述、文件路径和重试按钮。
        //  @param {string} panelId - 面板 DOM 元素 ID
        //  @param {string} moduleName - 模块名称
        //  @param {string} url - 面板文件 URL
        //  @param {Error} err - 错误对象
         //  @param {function} [callback] - 加载完成回调
         //  @param {boolean} [isTimeout] - 是否为超时错误
        // ====================================================================
        _showPanelError: function(panelId, moduleName, url, err, callback, isTimeout) {
            var self = this;
            var targetEl = document.getElementById(panelId);

            // 如果面板元素不存在，创建一个
            if (!targetEl) {
                var sidebar = document.querySelector('.sidebar-right');
                if (!sidebar) {
                    if (callback) { callback(false); }
                    return;
                }
                targetEl = document.createElement('div');
                targetEl.id = panelId;
                targetEl.className = 'module-panel panel-hidden';
                sidebar.appendChild(targetEl);
            }

            // 清空现有内容
            while (targetEl.firstChild) {
                targetEl.removeChild(targetEl.firstChild);
            }

            var panel = document.createElement('div');
            panel.className = 'panel';

            // 标题栏
            var titleDiv = document.createElement('div');
            titleDiv.className = 'panel-title';
            titleDiv.textContent = moduleName.toUpperCase() + ' - 加载失败 / LOAD FAILED';
            panel.appendChild(titleDiv);

            // 错误内容区域
            var bodyDiv = document.createElement('div');
            bodyDiv.className = 'panel-body';
            bodyDiv.style.cssText = 'color:#f85149;text-align:center;padding:20px;font-size:11px;';

            // 错误图标和主要提示
            var warnP = document.createElement('p');
            warnP.style.cssText = 'font-size:13px;margin-bottom:12px;';
            if (isTimeout) {
                warnP.textContent = '\u23F3 面板加载超时 / Panel load timed out';
            } else {
                warnP.textContent = '\u26A0 无法加载面板内容 / Cannot load panel content';
            }
            bodyDiv.appendChild(warnP);

            // 错误详情
            var detailP = document.createElement('p');
            detailP.style.cssText = 'font-size:10px;color:#8b949e;margin-top:8px;';
            detailP.textContent = '错误 / Error: ' + (err.message || '未知错误');
            bodyDiv.appendChild(detailP);

            // 文件路径
            var fileP = document.createElement('p');
            fileP.style.cssText = 'font-size:10px;color:#8b949e;margin-top:4px;word-break:break-all;';
            fileP.textContent = '文件 / File: ' + url;
            bodyDiv.appendChild(fileP);

            // 重试按钮
            var retryBtn = document.createElement('button');
            retryBtn.className = 'btn btn-accent';
            retryBtn.style.cssText = 'margin-top:16px;padding:6px 20px;';
            retryBtn.textContent = 'RETRY / 重试';
            retryBtn.title = '重新加载 ' + moduleName + ' 面板 / Reload ' + moduleName + ' panel';

            // 绑定重试事件：清空错误提示后重新加载面板
            retryBtn.addEventListener('click', function() {
                // 移除错误内容
                while (targetEl.firstChild) {
                    targetEl.removeChild(targetEl.firstChild);
                }
                // 显示重试中提示
                var retryingP = document.createElement('p');
                retryingP.style.cssText = 'color:#58a6ff;text-align:center;padding:20px;font-size:11px;';
                retryingP.textContent = '\u21BB 正在重新加载 / Retrying...';
                targetEl.appendChild(retryingP);

                // 延迟 300ms 后重新加载（带超时）
                setTimeout(function() {
                    self._fetchPanelWithTimeout(url, panelId, moduleName, callback);
                }, 300);
            });
            bodyDiv.appendChild(retryBtn);

            panel.appendChild(bodyDiv);
            targetEl.appendChild(panel);

            if (callback) { callback(false); }
        }
    };

    // ========================================================================
    //  兼容旧版全局导出（window.lv00Integration 保留用于向后兼容）
    //  建议新代码使用 window.__lv00Extension
    // ========================================================================
    window.__lv00Extension = _Lv00Ext;

    // 向后兼容的 lv00Integration 导出（仅保留 init 方法）
    window.lv00Integration = {
        init: function() { _Lv00Ext.init(); },
        addTabs: function() { _Lv00Ext._injectTabs(); },
        loadPanels: function() { _Lv00Ext._loadPanels(); }
    };

    // ========================================================================
    //  自动初始化：在 DOM 就绪时启动
    // ========================================================================
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', function() {
            _Lv00Ext.init();
        });
    } else {
        // DOM 已就绪，直接初始化
        _Lv00Ext.init();
    }

})();
