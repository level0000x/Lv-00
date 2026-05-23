/**
 * Lv-00 Formula Renderer（优化版）
 * 使用 KaTeX CDN 渲染数学公式
 *
 * @description 动态加载 KaTeX CDN 资源，提供数学公式的
 *              客户端渲染能力。支持 LaTeX 公式的实时渲染。
 * @module FormulaRenderer
 * @version 1.0.0
 *
 * 严格 ES5 语法（无 class, const, let, arrow functions, template literals,
 * destructuring, default params, spread）
 *
 * 用法：
 *   FormulaRenderer.init(function() {
 *       FormulaRenderer.renderToElement(el, '\\frac{1}{2}', true);
 *   });
 */

var FormulaRenderer = (function() {
    'use strict';

    // ---- 内部状态 ----
    var _ready = false;
    var _lastError = null;
    var _initCallbacks = [];
    var _loading = false;

    // ---- 工具函数 ----

    /**
     * 动态加载 CSS 文件
     */
    function _loadCSS(url) {
        var link = document.createElement('link');
        link.rel = 'stylesheet';
        link.type = 'text/css';
        link.href = url;
        document.head.appendChild(link);
    }

    /**
     * 动态加载 JS 文件（回调风格）
     */
    function _loadScript(url, callback) {
        var script = document.createElement('script');
        script.type = 'text/javascript';
        script.src = url;
        script.onload = function() {
            callback(null);
        };
        script.onerror = function() {
            callback(new Error('Failed to load script: ' + url));
        };
        document.head.appendChild(script);
    }

    /**
     * 确保容器元素存在
     */
    function _ensureElement(el) {
        if (!el) {
            _lastError = 'Target element is null or undefined';
            return false;
        }
        if (typeof el === 'string') {
            el = document.getElementById(el);
            if (!el) {
                _lastError = 'Element not found by id: ' + el;
                return false;
            }
        }
        return true;
    }

    /**
     * 创建公式卡片 DOM
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

            // 加载 KaTeX CSS
            _loadCSS('https://cdn.jsdelivr.net/npm/katex@0.16.9/dist/katex.min.css');

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
