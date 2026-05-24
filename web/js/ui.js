/**
 * ui.js - DOM 操作与 UI 更新模块
 *
 * 从 app.js 中提取的 DOM 操作方法，挂载到 Lv00WebApp.prototype 上。
 * 包含状态栏更新、统计信息更新、属性面板更新、日志系统、
 * Toast 通知、搜索、模态框、示例加载等功能。
 *
 * @description 所有 UI 相关操作均通过此模块完成，包括 DOM 更新、
 *              事件绑定和用户反馈（通知、日志）。
 * @module ui
 * @requires Lv00WebApp 构造函数（app.js）
 * @requires utils.js（辅助工具函数）
 * @since 3.0.0
 */
(function() {
    'use strict';

    // ---- UI 模块常量（从 constants.js 导入）-------------------------------
    /** @constant {Object} UI 模块常量命名空间引用 */
    var uc = Lv00Const.ui;

    // ================================================================
    /**
     * 更新状态栏文本
     *
     * @description 将文本安全写入页面底部状态栏的中央区域。
     *
     * 中文说明：将指定文本安全写入 #statusText 元素，仅使用 textContent 防止注入。
     *
     * @param {string} text - 要显示的状态文本
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype.updateStatus = function(text) {
        try {
            var el = document.getElementById('statusText');
            if (el) el.textContent = String(text);
        } catch (e) {
            console.warn('[Lv-00] updateStatus: 更新状态栏失败:', e.message);
        }
    };

    // ================================================================
    /**
     * 更新统计信息面板
     *
     * @description 刷新节点数和约束数的实时显示，数据来源于
     *              this.points 数组和 this.graph.constraints 数组。
     *
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype.updateStats = function() {
        try {
            var nodeCountEl = document.getElementById('nodeCount');
            var constraintCountEl = document.getElementById('constraintCount');

            if (nodeCountEl) nodeCountEl.textContent = this.points.length;
            if (constraintCountEl && this.graph) {
                constraintCountEl.textContent = this.graph.constraints ? this.graph.constraints.length : 0;
            }
        } catch (e) {
            console.warn('[Lv-00] updateStats: 更新统计信息失败:', e.message);
        }
    };

    // ================================================================
    /**
     * 更新属性面板
     *
     * @description 显示选中点的 ID、X、Y 坐标信息。
     *              当 point 为 null 时显示"未选择"提示。
     *
     * @param {Object|null} point - 选中的点对象 {id, x, y}，null 表示无选择
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype.updateProperties = function(point) {
        var el = document.getElementById('propContent');
        if (!el) return;

        // 使用 DocumentFragment 批量更新 DOM，避免多次重排
        var fragment = document.createDocumentFragment();

        if (!point) {
            var emptyDiv = document.createElement('div');
            emptyDiv.className = 'prop-empty';
            emptyDiv.textContent = 'No selection / 未选择';
            fragment.appendChild(emptyDiv);
        } else {
            // 辅助函数：创建属性行
            var _makePropRow = function(key, value) {
                var row = document.createElement('div');
                row.className = 'prop-row';
                var keySpan = document.createElement('span');
                keySpan.className = 'prop-key';
                keySpan.textContent = key;
                var valSpan = document.createElement('span');
                valSpan.className = 'prop-val';
                valSpan.textContent = value;
                row.appendChild(keySpan);
                row.appendChild(valSpan);
                return row;
            };

            fragment.appendChild(_makePropRow('ID', 'n' + point.id));
            fragment.appendChild(_makePropRow('X', point.x.toFixed(4)));
            fragment.appendChild(_makePropRow('Y', point.y.toFixed(4)));
        }

        // 清空现有内容并一次性插入新内容
        while (el.firstChild) {
            el.removeChild(el.firstChild);
        }
        el.appendChild(fragment);
    };

    // ================================================================
    /**
     * 追加日志条目
     *
     * @description 根据日志级别过滤，自动添加时间戳，限制最大条目数防止内存泄漏。
     *              使用 textContent 而非 innerHTML，防止 XSS 攻击。
     *              使用环形缓冲区管理日志条目，避免频繁 DOM 操作。
     *
     * 中文说明：向日志面板追加消息，使用环形缓冲区数组跟踪条目数量，
     *            超出上限时移除最旧条目，避免 shift() 带来的 O(n) 性能问题。
     *
     * @param {string} message - 日志消息内容
     * @param {string} [level='info'] - 日志级别：'debug' | 'info' | 'warn' | 'error'
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype.appendLog = function(message, level) {
        level = level || 'info';
        var levels = { debug: 0, info: 1, warn: 2, error: 3 };

        // 日志级别过滤：低于当前最小级别的日志不显示
        if (levels[level] < levels[this.minLogLevel]) return;

        try {
            var logContent = document.getElementById('logContent');
            if (!logContent) return;

            var time = new Date().toLocaleTimeString('zh-CN', { hour12: false });
            var entry = document.createElement('div');
            entry.className = 'log-entry log-' + level;

            var timeSpan = document.createElement('span');
            timeSpan.className = 'log-time';
            timeSpan.textContent = '[' + time + ']';

            var msgSpan = document.createElement('span');
            msgSpan.className = 'log-msg';
            // 使用 textContent 避免消息中的 HTML 被解析执行（XSS 防护）
            msgSpan.textContent = message;

            entry.appendChild(timeSpan);
            entry.appendChild(msgSpan);
            logContent.appendChild(entry);
            logContent.scrollTop = logContent.scrollHeight;

            // 环形缓冲区（修复：使用头尾指针实现真正的环形缓冲区，O(1) 读写，避免 shift() 的 O(n) 开销）
            // 中文说明：_logRingBuffer 为固定大小数组，_logHead 指向最旧条目，_logTail 指向下一个写入位置，
            //            _logCount 跟踪当前条目数。满时覆盖最旧位置，无需数组重排。
            if (!this._logRingBuffer) {
                this._logRingBuffer = new Array(uc.MAX_LOG_ENTRIES);
                this._logHead = 0;
                this._logTail = 0;
                this._logCount = 0;
            }
            // 如果缓冲区已满，覆盖最旧位置的 DOM 节点（先移除再写入）
            var writeIdx = this._logTail;
            if (this._logCount >= uc.MAX_LOG_ENTRIES) {
                var oldest = this._logRingBuffer[writeIdx];
                if (oldest && oldest.parentNode) {
                    oldest.parentNode.removeChild(oldest);
                }
                this._logHead = (this._logHead + 1) % uc.MAX_LOG_ENTRIES;
            } else {
                this._logCount++;
            }
            this._logRingBuffer[writeIdx] = entry;
            this._logTail = (writeIdx + 1) % uc.MAX_LOG_ENTRIES;
        } catch (e) {
            console.warn('[Lv-00] appendLog: 写入日志失败:', e.message);
        }
    };

    // ================================================================
    /**
     * Toast 通知系统
     *
     * @description 显示临时通知消息，支持不同类型（info/success/error/warning）
     *              和自定义显示时长，到期后自动播放滑出动画并移除。
     *
     * @param {string} message - 通知消息内容
     * @param {string} [type='info'] - 通知类型：'info' | 'success' | 'error' | 'warning'
     * @param {number} [duration=3000] - 显示时长（毫秒）
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype.showToast = function(message, type, duration) {
        type = type || 'info';
        duration = duration || uc.TOAST_DURATION_DEFAULT;

        try {
            var container = document.getElementById('toastContainer');
            if (!container) return;
            var toast = document.createElement('div');
            toast.className = 'toast toast-' + type;
            toast.textContent = message;

            container.appendChild(toast);

            // 自动移除（带滑出动画）
            setTimeout(function() {
                toast.classList.add('hiding');
                setTimeout(function() {
                    if (toast.parentNode) {
                        toast.parentNode.removeChild(toast);
                    }
                }, uc.TOAST_HIDE_ANIMATION_MS);
            }, duration);
        } catch (e) {
            console.warn('[Lv-00] showToast: 显示通知失败:', e.message);
        }
    };

    // ================================================================
    /**
     * 显示成功通知（便捷方法）
     *
     * @param {string} message - 通知消息内容
     */
    // ================================================================
    Lv00WebApp.prototype.showSuccess = function(message) {
        this.showToast(message, 'success', uc.TOAST_DURATION_DEFAULT);
    };

    // ================================================================
    /**
     * 显示错误通知（便捷方法）
     *
     * @param {string} message - 错误消息内容
     */
    // ================================================================
    Lv00WebApp.prototype.showError = function(message) {
        this.showToast(message, 'error', uc.TOAST_DURATION_ERROR);
    };

    // ================================================================
    /**
     * 显示警告通知（便捷方法）
     *
     * @param {string} message - 警告消息内容
     */
    // ================================================================
    Lv00WebApp.prototype.showWarning = function(message) {
        this.showToast(message, 'warning', uc.TOAST_DURATION_WARNING);
    };

    // ================================================================
    /**
     * 显示信息通知（便捷方法）
     *
     * @param {string} message - 信息内容
     */
    // ================================================================
    Lv00WebApp.prototype.showInfo = function(message) {
        this.showToast(message, 'info', uc.TOAST_DURATION_DEFAULT);
    };

    // ================================================================
    /**
     * 模块面板切换
     *
     * @description 隐藏所有模块面板，显示指定的目标模块面板。
     *              同时更新面包屑导航和模块标签页的激活状态。
     *
     * @param {string} moduleName - 目标模块名称（如 'formula', 'graph', 'proof'）
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype.switchModule = function(moduleName) {
        if (!moduleName || typeof moduleName !== 'string') {
            console.warn('[Lv-00] switchModule: 无效的模块名称:', moduleName);
            return;
        }

        try {
            // 隐藏所有模块面板
            var panels = document.querySelectorAll('.module-panel');
            for (var i = 0; i < panels.length; i++) {
                panels[i].classList.add('panel-hidden');
            }

            // 显示目标模块面板
            var panelId = 'panel' + moduleName.charAt(0).toUpperCase() + moduleName.slice(1);
            var targetPanel = document.getElementById(panelId);
            if (targetPanel) {
                targetPanel.classList.remove('panel-hidden');
            } else {
                console.warn('[Lv-00] switchModule: 未找到面板:', panelId);
            }

            // 更新面包屑导航
            var breadcrumb = document.getElementById('breadcrumbModule');
            if (breadcrumb) {
                breadcrumb.textContent = moduleName.toUpperCase();
            }

            // 更新模块标签页激活状态
            var tabs = document.querySelectorAll('.module-tab');
            for (var j = 0; j < tabs.length; j++) {
                tabs[j].classList.remove('active');
                tabs[j].setAttribute('aria-selected', 'false');
                if (tabs[j].getAttribute('data-module') === moduleName) {
                    tabs[j].classList.add('active');
                    tabs[j].setAttribute('aria-selected', 'true');
                }
            }

            // 面板切换时重置相关显示（依赖树、属性面板等恢复空状态）
            if (typeof this._resetPanelDisplay === 'function') {
                this._resetPanelDisplay();
            }
        } catch (e) {
            console.error('[Lv-00] switchModule: 模块切换失败:', e.message);
            this.appendLog('模块切换失败: ' + e.message, 'error');
        }
    };

    // ================================================================
    /**
     * 显示模态框
     *
     * @description 通过添加 'active' CSS 类来显示指定 ID 的模态框。
     *              增加 DOM 元素存在性和 API 可用性双重验证。
     *
     * 中文说明：安全显示指定 ID 的模态框，先检查元素是否存在
     *            以及是否支持 classList API，防止空引用异常。
     *
     * @param {string} id - 模态框的 DOM 元素 ID
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype._showModal = function(id) {
        if (!id || typeof id !== 'string') return;
        try {
            var modal = document.getElementById(id);
            // 验证元素存在且支持 classList API（兼容性检查）
            if (modal && modal.classList && typeof modal.classList.add === 'function') {
                modal.classList.add('active');
            } else if (modal && !modal.classList) {
                // 回退方案：对于不支持 classList 的极旧浏览器
                console.warn('[Lv-00] _showModal: classList 不可用，使用 className 回退');
                modal.className += ' active';
            }
        } catch (e) {
            console.warn('[Lv-00] _showModal: 显示模态框失败:', e.message);
        }
    };

    // ================================================================
    /**
     * 模态框初始化
     *
     * @description 绑定模态框的确认/取消按钮、遮罩层点击关闭和 ESC 键关闭事件。
     *              通用模态框 (#modalOverlay) 和数值假设模态框均在此处理。
     *
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype._initModals = function() {
        var self = this;

        try {
            // 通用模态框确认按钮
            var okBtn = document.getElementById('modalOk');
            if (okBtn) {
                okBtn.addEventListener('click', function() {
                    var overlay = document.getElementById('modalOverlay');
                    if (overlay) overlay.classList.remove('active');
                });
            }

            // 通用模态框取消按钮
            var cancelBtn = document.getElementById('modalCancel');
            if (cancelBtn) {
                cancelBtn.addEventListener('click', function() {
                    var overlay = document.getElementById('modalOverlay');
                    if (overlay) overlay.classList.remove('active');
                });
            }

            // 点击遮罩层关闭模态框
            var overlay = document.getElementById('modalOverlay');
            if (overlay) {
                overlay.addEventListener('click', function(e) {
                    if (e.target === overlay) {
                        overlay.classList.remove('active');
                    }
                });
            }

            // ESC 键关闭所有模态框（修复：保存引用为实例属性，便于 cleanup 时移除，防止内存泄漏）
            // 中文说明：将 ESC 事件处理器保存为 _escHandler，后续可在 _cleanupModals 中移除
            this._escHandler = function(e) {
                if (e.key === 'Escape') {
                    var overlays = document.querySelectorAll('.modal-overlay.active');
                    for (var i = 0; i < overlays.length; i++) {
                        overlays[i].classList.remove('active');
                    }
                }
            };
            document.addEventListener('keydown', this._escHandler);
        } catch (e) {
            console.error('[Lv-00] _initModals: 模态框初始化失败:', e.message);
        }
    };

    // ================================================================
    /**
     * 清理模态框事件监听器
     *
     * @description 移除 _initModals 和 _initSearch 中注册在 document 上的
     *              ESC 键和 Ctrl+F 快捷键事件监听器，防止内存泄漏。
     *
     * 中文说明：在 cleanup 中被调用，移除保存的事件处理器引用，
     *            防止 DOM 销毁后事件监听器未释放导致的内存泄漏。
     *
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype._cleanupModals = function() {
        try {
            // 移除 ESC 键事件监听器，防止内存泄漏
            if (this._escHandler) {
                document.removeEventListener('keydown', this._escHandler);
                this._escHandler = null;
            }
            // 移除搜索快捷键（Ctrl+F / ESC）事件监听器
            if (this._searchKeyHandler) {
                document.removeEventListener('keydown', this._searchKeyHandler);
                this._searchKeyHandler = null;
            }
        } catch (e) {
            console.warn('[Lv-00] _cleanupModals: 清理模态框事件失败:', e.message);
        }
    };

    // ================================================================
    /**
     * 搜索功能初始化
     *
     * @description 绑定搜索按钮、搜索输入框、Ctrl+F 快捷键和 ESC 关闭事件。
     *              支持按节点 ID 实时搜索，结果以列表形式展示。
     *
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype._initSearch = function() {
        var self = this;
        var searchBtn = document.getElementById('btnSearch');
        var searchBar = document.getElementById('searchBar');
        var searchInput = document.getElementById('searchInput');
        var searchResults = document.getElementById('searchResults');

        if (!searchBtn || !searchBar || !searchInput) return;

        try {
            // 切换搜索栏显示
            searchBtn.addEventListener('click', function() {
                searchBar.classList.toggle('active');
                if (searchBar.classList.contains('active')) {
                    searchInput.focus();
                }
            });

            // Ctrl+F 快捷键打开搜索（修复：保存引用为实例属性，便于 cleanup 时移除，防止内存泄漏）
            // 中文说明：将搜索快捷键处理器保存为 _searchKeyHandler，后续可在 _cleanupModals 中移除
            this._searchKeyHandler = function(e) {
                if ((e.ctrlKey || e.metaKey) && e.key === 'f') {
                    e.preventDefault();
                    searchBar.classList.add('active');
                    searchInput.focus();
                }
                // ESC 关闭搜索
                if (e.key === 'Escape' && searchBar.classList.contains('active')) {
                    searchBar.classList.remove('active');
                    searchInput.value = '';
                    if (searchResults) {
                        // 清空搜索结果（安全：仅清空）
                        while (searchResults.firstChild) {
                            searchResults.removeChild(searchResults.firstChild);
                        }
                    }
                }
            };
            document.addEventListener('keydown', this._searchKeyHandler);

            // 实时搜索（input 事件）
            searchInput.addEventListener('input', function() {
                var query = searchInput.value.trim().toLowerCase();
                if (!searchResults) return;

                // 清空现有搜索结果
                while (searchResults.firstChild) {
                    searchResults.removeChild(searchResults.firstChild);
                }

                if (!query) {
                    return;
                }

                // 使用 DocumentFragment 批量构建搜索结果
                var fragment = document.createDocumentFragment();
                var found = 0;

                // 搜索节点
                for (var i = 0; i < self.points.length; i++) {
                    var point = self.points[i];
                    var label = 'n' + point.id;
                    if (label.indexOf(query) !== -1 || query === String(point.id)) {
                        var item = document.createElement('div');
                        item.className = 'search-result-item';
                        item.textContent = label + ' (' + point.x.toFixed(2) + ', ' + point.y.toFixed(2) + ')';
                        item.setAttribute('data-node-id', point.id);

                        // 点击搜索结果：选中对应节点并居中显示
                        (function(p) {
                            item.addEventListener('click', function() {
                                self.selectedPoint = p;
                                self.offsetX = -p.x;
                                self.offsetY = -p.y;
                                self.updateProperties(p);
                                self.render();
                                searchBar.classList.remove('active');
                                searchInput.value = '';
                                while (searchResults.firstChild) {
                                    searchResults.removeChild(searchResults.firstChild);
                                }
                            });
                        })(point);

                        fragment.appendChild(item);
                        found++;
                        if (found >= uc.MAX_SEARCH_RESULTS) break;
                    }
                }

                if (found === 0) {
                    var noResultDiv = document.createElement('div');
                    noResultDiv.className = 'search-result-item';
                    // 修复：使用 CSS 变量替代硬编码颜色值 '#666'，与主题系统保持一致
                    // 中文说明：通过 var(--color-text-muted) 使文本颜色响应主题切换
                    noResultDiv.style.color = 'var(--color-text-muted)';
                    noResultDiv.textContent = 'No results / 未找到结果';
                    fragment.appendChild(noResultDiv);
                }

                searchResults.appendChild(fragment);
            });
        } catch (e) {
            console.error('[Lv-00] _initSearch: 搜索功能初始化失败:', e.message);
        }
    };

    // ================================================================
    // 示例数据（修复：提取为模块级常量 _EXAMPLES，避免每次调用 loadExample 时重新创建对象）
    // 中文说明：将示例图形定义提取到 IIFE 闭包中，仅初始化一次，减少内存分配和 GC 压力
    // ================================================================
    var _EXAMPLES = {
                'equilateral_triangle': function(app) {
                    // 等边三角形：边长为 4
                    var h = Math.sqrt(3) / 2 * 4;
                    app.addPoint(-2, -h / 3);
                    app.addPoint(2, -h / 3);
                    app.addPoint(0, 2 * h / 3);
                    if (app.points.length >= 3) {
                        app.addSegment(app.points[0].id, app.points[1].id);
                        app.addSegment(app.points[1].id, app.points[2].id);
                        app.addSegment(app.points[2].id, app.points[0].id);
                    }
                    app.appendLog('Loaded: Equilateral Triangle / 等边三角形', 'info');
                },
                'circle_equation': function(app) {
                    // 圆（正十二边形近似），半径 3
                    var r = 3;
                    var n = 12;
                    for (var i = 0; i < n; i++) {
                        var angle = (2 * Math.PI * i) / n;
                        app.addPoint(r * Math.cos(angle), r * Math.sin(angle));
                    }
                    for (var j = 0; j < n; j++) {
                        app.addSegment(app.points[j].id, app.points[(j + 1) % n].id);
                    }
                    app.appendLog('Loaded: Circle (12-gon) / 圆（正十二边形近似）', 'info');
                },
                'pythagorean': function(app) {
                    // 勾股定理：3-4-5 直角三角形
                    app.addPoint(0, 0);
                    app.addPoint(3, 0);
                    app.addPoint(0, 4);
                    if (app.points.length >= 3) {
                        app.addSegment(app.points[0].id, app.points[1].id);
                        app.addSegment(app.points[1].id, app.points[2].id);
                        app.addSegment(app.points[2].id, app.points[0].id);
                    }
                    app.appendLog('Loaded: Pythagorean 3-4-5 / 勾股定理 3-4-5', 'info');
                },
                'midpoint': function(app) {
                    // 中垂线示例
                    app.addPoint(-3, 0);
                    app.addPoint(3, 0);
                    app.addPoint(0, 0);
                    if (app.points.length >= 3) {
                        app.addSegment(app.points[0].id, app.points[1].id);
                    }
                    app.appendLog('Loaded: Midpoint / 中点', 'info');
                },
                'line_equation': function(app) {
                    // 直线方程 y = x/2
                    app.addPoint(-4, -2);
                    app.addPoint(4, 2);
                    if (app.points.length >= 2) {
                        app.addSegment(app.points[0].id, app.points[1].id);
                    }
                    app.appendLog('Loaded: Line Equation y = x/2 / 直线方程', 'info');
                },
                'triangle_area': function(app) {
                    // 三角形面积示例
                    app.addPoint(0, 0);
                    app.addPoint(4, 0);
                    app.addPoint(2, 3);
                    if (app.points.length >= 3) {
                        app.addSegment(app.points[0].id, app.points[1].id);
                        app.addSegment(app.points[1].id, app.points[2].id);
                        app.addSegment(app.points[2].id, app.points[0].id);
                    }
                    app.appendLog('Loaded: Triangle Area / 三角形面积', 'info');
                },
                'distance': function(app) {
                    // 两点距离示例
                    app.addPoint(1, 2);
                    app.addPoint(4, 6);
                    if (app.points.length >= 2) {
                        app.addSegment(app.points[0].id, app.points[1].id);
                    }
                    app.appendLog('Loaded: Distance / 两点距离', 'info');
                },
                'ellipse': function(app) {
                    // 椭圆 a=4, b=2（正十六边形近似）
                    var a = 4, b = 2;
                    var n = 16;
                    for (var i = 0; i < n; i++) {
                        var angle = (2 * Math.PI * i) / n;
                        app.addPoint(a * Math.cos(angle), b * Math.sin(angle));
                    }
                    for (var j = 0; j < n; j++) {
                        app.addSegment(app.points[j].id, app.points[(j + 1) % n].id);
                    }
                    app.appendLog('Loaded: Ellipse a=4, b=2 / 椭圆', 'info');
                },
                'triangle': _EXAMPLES['equilateral_triangle'],  // 复用等边三角形示例
                'square': function(app) {
                    // 正方形 4x4
                    app.addPoint(-2, -2);
                    app.addPoint(2, -2);
                    app.addPoint(2, 2);
                    app.addPoint(-2, 2);
                    if (app.points.length >= 4) {
                        app.addSegment(app.points[0].id, app.points[1].id);
                        app.addSegment(app.points[1].id, app.points[2].id);
                        app.addSegment(app.points[2].id, app.points[3].id);
                        app.addSegment(app.points[3].id, app.points[0].id);
                    }
                    app.appendLog('Loaded: Square / 正方形', 'info');
                },
                'intersection': function(app) {
                    // 线段相交
                    app.addPoint(-3, -1);
                    app.addPoint(3, 1);
                    app.addPoint(-1, -3);
                    app.addPoint(1, 3);
                    if (app.points.length >= 4) {
                        app.addSegment(app.points[0].id, app.points[1].id);
                        app.addSegment(app.points[2].id, app.points[3].id);
                    }
                    app.appendLog('Loaded: Segment Intersection / 线段相交', 'info');
                },
                'midpoint_graph': function(app) {
                    // 中点（Graph 模块预设）
                    app.addPoint(-3, 0);
                    app.addPoint(3, 0);
                    app.addPoint(0, 0);
                    if (app.points.length >= 3) {
                        app.addSegment(app.points[0].id, app.points[1].id);
                    }
                    app.appendLog('Loaded: Midpoint / 中点', 'info');
                }
            };

    // ================================================================
    /**
     * 加载示例图形
     *
     * @description 根据示例名称加载预设的几何图形。
     *              先清空当前画布，再按预设规则添加点和线段。
     *              使用模块级 _EXAMPLES 常量，避免重复创建示例对象。
     *
     * @param {string} name - 示例名称标识符（如 'equilateral_triangle', 'circle_equation'）
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype.loadExample = function(name) {
        if (!name) {
            this.appendLog('loadExample: 示例名称为空', 'warn');
            return;
        }

        try {
            this.clear();
            // 修复：使用模块级 _EXAMPLES 常量，避免每次调用时重新创建示例对象
            // 中文说明：_EXAMPLES 在 IIFE 闭包中仅初始化一次，后续调用直接复用
            var loader = _EXAMPLES[name];
            if (loader) {
                loader(this);
                this.render();
            } else {
                this.appendLog('Unknown example: ' + name + ' / 未知示例', 'warn');
            }
        } catch (e) {
            console.error('[Lv-00] loadExample: 加载示例失败:', e.message);
            this.appendLog('加载示例 ' + name + ' 失败: ' + e.message, 'error');
        }
    };

})();
