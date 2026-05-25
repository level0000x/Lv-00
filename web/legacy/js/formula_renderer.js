/**
 * @file formula_renderer.js
 * @brief 公式渲染器（Formula Renderer）
 * @description 使用 KaTeX CDN 动态加载并渲染数学公式。
 *              提供公式的客户端渲染能力，支持 LaTeX 公式的实时渲染和公式卡片生成。
 *              内部通过动态创建 <script> 和 <link> 标签加载 KaTeX 资源，
 *              加载完成后执行回调队列中等待的渲染任务。
 *
 * 用法：
 *   FormulaRenderer.init(function() {
 *       FormulaRenderer.renderToElement(el, '\\frac{1}{2}', true);
 *   });
 *
 * @module FormulaRenderer
 * @version 1.0.0
 * @requires 严格 ES5 语法（无 class, const, let, arrow functions, template literals,
 *            destructuring, default params, spread）
 */

var FormulaRenderer = (function() {
    'use strict';

    // ---- 内部状态 ----
    var _ready = false;           // KaTeX 是否加载完成
    var _lastError = null;        // 最近一次错误信息
    var _initCallbacks = [];      // 初始化回调队列（KaTeX 加载完成后依次执行）
    var _loading = false;         // 是否正在加载 KaTeX 资源
    var _CDN_TIMEOUT = 15000;     // CDN 资源加载超时时间（毫秒）

    // ---- 工具函数 ----

    /**
     * 动态加载 CSS 文件（带超时机制）
     * @param {string} url - CSS 文件的 URL 地址
     * @param {Function} [callback] - 加载完成后的回调函数，参数为 (error)
     */
    function _loadCSS(url, callback) {
        var link = document.createElement('link');
        link.rel = 'stylesheet';
        link.type = 'text/css';
        link.href = url;

        if (callback) {
            var timeout = setTimeout(function() {
                callback(new Error('加载样式超时 (' + _CDN_TIMEOUT + 'ms): ' + url));
                if (link.parentNode) link.parentNode.removeChild(link);
            }, _CDN_TIMEOUT);

            link.onload = function() {
                clearTimeout(timeout);
                callback(null);
            };
            link.onerror = function() {
                clearTimeout(timeout);
                callback(new Error('加载样式失败: ' + url));
            };
        }

        document.head.appendChild(link);
    }

    /**
     * 动态加载 JS 文件（回调风格，带超时机制）
     * @param {string} url - JS 文件的 URL 地址
     * @param {Function} callback - 加载完成后的回调函数，参数为 (error)
     */
    function _loadScript(url, callback) {
        var script = document.createElement('script');
        script.type = 'text/javascript';
        script.src = url;

        /* 添加超时机制 */
        var timeout = setTimeout(function() {
            callback(new Error('加载脚本超时 (' + _CDN_TIMEOUT + 'ms): ' + url));
            if (script.parentNode) script.parentNode.removeChild(script);
        }, _CDN_TIMEOUT);

        script.onload = function() {
            clearTimeout(timeout);
            callback(null);
        };
        script.onerror = function() {
            clearTimeout(timeout);
            callback(new Error('加载脚本失败: ' + url));
        };
        document.head.appendChild(script);
    }

    /**
     * 确保容器元素存在
     * @param {HTMLElement|string} el - DOM 元素或元素 ID 字符串
     * @returns {boolean} 元素是否存在且有效
     */
    function _ensureElement(el) {
        if (!el) {
            _lastError = 'Target element is null or undefined';
            return false;
        }
        if (typeof el === 'string') {
            var elId = el; // 保存原始 ID 字符串，避免被 getElementById 返回值覆盖
            el = document.getElementById(elId);
            if (!el) {
                _lastError = 'Element not found by id: ' + elId;
                return false;
            }
        }
        return true;
    }

    /**
     * 创建公式卡片 DOM 元素
     * @param {Object} formula - 公式配置对象
     * @param {string} [formula.label] - 公式标签文本
     * @param {string} formula.latex - LaTeX 公式字符串
     * @param {boolean} [formula.displayMode=true] - 是否使用显示模式（块级公式）
     * @returns {HTMLElement} 公式卡片 DOM 元素
     */
    function _createFormulaCard(formula) {
        var card = document.createElement('div');
        card.className = 'formula-card';

        // 标签行
        if (formula.label) {
            var labelDiv = document.createElement('div');
            labelDiv.className = 'formula-card-label';
            labelDiv.textContent = formula.label;
            card.appendChild(labelDiv);
        }

        // 公式行
        var formulaDiv = document.createElement('div');
        formulaDiv.className = 'formula-card-formula';
        var displayMode = formula.displayMode !== undefined ? formula.displayMode : true;
        try {
            if (typeof katex !== 'undefined') {
                katex.render(formula.latex, formulaDiv, {
                    displayMode: displayMode,
                    throwOnError: false
                });
            } else {
                formulaDiv.textContent = formula.latex;
            }
        } catch (e) {
            formulaDiv.textContent = formula.latex;
            _lastError = 'KaTeX render error: ' + e.message;
        }
        card.appendChild(formulaDiv);

        // 描述行
        if (formula.description) {
            var descDiv = document.createElement('div');
            descDiv.className = 'formula-card-description';
            descDiv.textContent = formula.description;
            card.appendChild(descDiv);
        }

        return card;
    }

    /**
     * 创建章节 DOM
     */
    function _createSection(section) {
        var sectionEl = document.createElement('div');
        sectionEl.className = 'formula-section';

        // 章节标题
        if (section.title) {
            var heading = document.createElement('h3');
            heading.className = 'formula-section-title';
            heading.textContent = section.title;
            sectionEl.appendChild(heading);
        }

        // 公式列表
        if (section.formulas && section.formulas.length > 0) {
            var list = document.createElement('div');
            list.className = 'formula-section-list';
            for (var i = 0; i < section.formulas.length; i++) {
                list.appendChild(_createFormulaCard(section.formulas[i]));
            }
            sectionEl.appendChild(list);
        }

        return sectionEl;
    }

    // ---- 注入深色主题样式 ----
    /**
     * 注入公式渲染所需的 CSS 样式
     * 
     * 注意：这些样式与 main.css 中的定义可能重复，
     * 此处作为动态加载的备用方案，确保在 main.css 未加载时仍能正常显示。
     * 如需修改样式，请同步更新 main.css 中的对应定义。
     */
    function _injectStyles() {
        if (document.getElementById('formula-renderer-styles')) {
            return;
        }
        var style = document.createElement('style');
        style.id = 'formula-renderer-styles';
        style.textContent =
            /* 公式卡片 */
            '.formula-card {' +
            '  background-color: rgba(255, 255, 255, 0.06);' +
            '  border: 1px solid rgba(255, 255, 255, 0.15);' +
            '  border-radius: 8px;' +
            '  padding: 16px 20px;' +
            '  margin-bottom: 12px;' +
            '  color: #e0e0e0;' +
            '  transition: background-color 0.2s ease;' +
            '}' +
            '.formula-card:hover {' +
            '  background-color: rgba(255, 255, 255, 0.1);' +
            '}' +
            '.formula-card-label {' +
            '  font-size: 13px;' +
            '  font-weight: 600;' +
            '  color: #90caf9;' +
            '  margin-bottom: 8px;' +
            '  text-transform: uppercase;' +
            '  letter-spacing: 0.5px;' +
            '}' +
            '.formula-card-formula {' +
            '  font-size: 18px;' +
            '  color: #ffffff;' +
            '  margin: 10px 0;' +
            '  overflow-x: auto;' +
            '  padding: 4px 0;' +
            '}' +
            '.formula-card-formula .katex {' +
            '  color: #ffffff;' +
            '}' +
            '.formula-card-description {' +
            '  font-size: 13px;' +
            '  color: #b0b0b0;' +
            '  margin-top: 8px;' +
            '  line-height: 1.5;' +
            '}' +
            /* 章节 */
            '.formula-section {' +
            '  margin-bottom: 24px;' +
            '}' +
            '.formula-section-title {' +
            '  font-size: 20px;' +
            '  font-weight: 600;' +
            '  color: #e0e0e0;' +
            '  margin-bottom: 16px;' +
            '  padding-bottom: 8px;' +
            '  border-bottom: 1px solid rgba(255, 255, 255, 0.12);' +
            '}' +
            '.formula-section-list {' +
            '  padding-left: 0;' +
            '}' +
            /* 文档标题 */
            '.formula-doc-title {' +
            '  font-size: 28px;' +
            '  font-weight: 700;' +
            '  color: #ffffff;' +
            '  margin-bottom: 24px;' +
            '  padding-bottom: 12px;' +
            '  border-bottom: 2px solid rgba(255, 255, 255, 0.2);' +
            '}' +
            /* 行内公式 */
            '.formula-inline .katex {' +
            '  color: #ffffff;' +
            '  font-size: 1em;' +
            '}' +
            /* 块级公式 */
            '.formula-block {' +
            '  text-align: center;' +
            '  margin: 16px 0;' +
            '  padding: 12px;' +
            '  background-color: rgba(255, 255, 255, 0.04);' +
            '  border-radius: 6px;' +
            '}' +
            '.formula-block .katex {' +
            '  color: #ffffff;' +
            '  font-size: 1.2em;' +
            '}' +
            /* 加载提示 */
            '.formula-loading {' +
            '  color: #888;' +
            '  font-style: italic;' +
            '  padding: 8px;' +
            '}';
        document.head.appendChild(style);
    }

    // ---- 公共 API ----

    return {
        /**
         * 初始化 KaTeX（动态加载 CDN）
         * @param {Function} callback - 加载完成后的回调
         */
        init: function(callback) {
            // 如果已经加载完成，直接回调
            if (_ready) {
                if (callback) callback();
                return;
            }

            // 如果正在加载，排队等待
            if (_loading) {
                if (callback) _initCallbacks.push(callback);
                return;
            }

            _loading = true;
            _lastError = null;
            _injectStyles();

            // 加载 KaTeX CSS（带超时回调，CSS 加载失败不阻塞 JS 加载）
            _loadCSS('https://cdn.jsdelivr.net/npm/katex@0.16.9/dist/katex.min.css', function(cssErr) {
                if (cssErr) {
                    _lastError = cssErr.message;
                    console.warn('[FormulaRenderer] ' + cssErr.message);
                }
            });

            // 加载 KaTeX JS
            _loadScript('https://cdn.jsdelivr.net/npm/katex@0.16.9/dist/katex.min.js', function(err) {
                _loading = false;
                if (err) {
                    _lastError = err.message;
                    // 仍然调用回调，让调用者通过 isReady 检查状态
                    if (callback) callback();
                    return;
                }
                _ready = true;
                if (callback) callback();
                // 执行排队的回调
                for (var i = 0; i < _initCallbacks.length; i++) {
                    _initCallbacks[i]();
                }
                _initCallbacks = [];
            });
        },

        /**
         * 渲染 LaTeX 到指定 DOM 元素
         * @param {HTMLElement|string} element - 目标 DOM 元素或元素 ID
         * @param {string} latex - LaTeX 公式字符串
         * @param {boolean} displayMode - true=块级显示, false=行内显示
         */
        renderToElement: function(element, latex, displayMode) {
            if (!_ensureElement(element)) return false;
            if (typeof element === 'string') {
                element = document.getElementById(element);
            }
            if (!_ready) {
                _lastError = 'KaTeX is not ready. Call FormulaRenderer.init() first.';
                element.textContent = latex;
                return false;
            }
            try {
                katex.render(latex, element, {
                    displayMode: !!displayMode,
                    throwOnError: false
                });
                _lastError = null;
                return true;
            } catch (e) {
                _lastError = 'Render error: ' + e.message;
                element.textContent = latex;
                return false;
            }
        },

        /**
         * 渲染 LaTeX 为 HTML 字符串
         * @param {string} latex - LaTeX 公式字符串
         * @param {boolean} displayMode - true=块级显示, false=行内显示
         * @returns {string} HTML 字符串
         */
        renderToString: function(latex, displayMode) {
            if (!_ready) {
                _lastError = 'KaTeX is not ready. Call FormulaRenderer.init() first.';
                return '<span class="formula-loading">' + latex + '</span>';
            }
            try {
                var html = katex.renderToString(latex, {
                    displayMode: !!displayMode,
                    throwOnError: false
                });
                _lastError = null;
                return html;
            } catch (e) {
                _lastError = 'Render error: ' + e.message;
                return '<span class="formula-loading">' + latex + '</span>';
            }
        },

        /**
         * 渲染公式列表
         * @param {HTMLElement|string} containerElement - 容器 DOM 元素或元素 ID
         * @param {Array} formulas - 公式数组 [{label, latex, description}]
         */
        renderFormulaList: function(containerElement, formulas) {
            if (!_ensureElement(containerElement)) return;
            if (typeof containerElement === 'string') {
                containerElement = document.getElementById(containerElement);
            }
            if (!formulas || !formulas.length) {
                _lastError = 'Formula list is empty';
                return;
            }

            // 清空容器
            containerElement.innerHTML = '';

            for (var i = 0; i < formulas.length; i++) {
                var card = _createFormulaCard(formulas[i]);
                containerElement.appendChild(card);
            }
            _lastError = null;
        },

        /**
         * 渲染完整的公式文档
         * @param {HTMLElement|string} containerElement - 容器 DOM 元素或元素 ID
         * @param {Object} docData - 文档数据 {title, sections: [{title, formulas}]}
         */
        renderDocument: function(containerElement, docData) {
            if (!_ensureElement(containerElement)) return;
            if (typeof containerElement === 'string') {
                containerElement = document.getElementById(containerElement);
            }
            if (!docData) {
                _lastError = 'Document data is null';
                return;
            }

            // 清空容器
            containerElement.innerHTML = '';

            // 文档标题
            if (docData.title) {
                var titleEl = document.createElement('h2');
                titleEl.className = 'formula-doc-title';
                titleEl.textContent = docData.title;
                containerElement.appendChild(titleEl);
            }

            // 各章节
            if (docData.sections && docData.sections.length > 0) {
                for (var i = 0; i < docData.sections.length; i++) {
                    containerElement.appendChild(_createSection(docData.sections[i]));
                }
            }

            _lastError = null;
        },

        /**
         * 检查 KaTeX 是否已加载
         * @returns {boolean}
         */
        isReady: function() {
            return _ready;
        },

        /**
         * 获取错误信息
         * @returns {string|null}
         */
        getLastError: function() {
            return _lastError;
        }
    };
})();
