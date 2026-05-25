/**
 * ============================================================================
 *  Lv-00 Web Application - FORMULA Module (formula_module.js)
 * ============================================================================
 *
 *  用途：公式编辑、渲染、双向转换（公式 <-> 图形）的交互逻辑。
 *        挂载到 Lv00WebApp.prototype 上，作为 Lv-00 Web 应用的 FORMULA 面板 UI 层。
 *
 *  语法要求：严格 ES5（无 class, const, let, arrow functions,
 *            template literals, destructuring, default params, spread）。
 *
 *  依赖：
 *    - FormulaParser     (formula_parser.js)   公式解析器
 *    - FormulaRenderer   (formula_renderer.js) 公式渲染器（KaTeX）
 *    - FormulaToGraph    (formula_to_graph.js) 公式 -> 图形转换器
 *    - GraphToFormula    (graph_to_formula.js) 图形 -> 公式转换器
 *    - Lv00WebApp        (app.js)              主应用构造函数
 *
 *  作者：Lv-00 Team
 *  创建日期：2026-05-20
 *  版本：1.0.0
 * ============================================================================
 */

(function() {
    'use strict';

    // ========================================================================
    //  预设示例公式
    // ========================================================================

    var FORMULA_EXAMPLES = {
        'equilateral_triangle': {
            name: '等边三角形',
            syntax: 'dsl',
            code: 'point A(0, 0); point B(2, 0); point C(1, sqrt(3))\nsegment AB(A, B); segment BC(B, C); segment CA(C, A)'
        },
        'circle_equation': {
            name: '圆的方程',
            syntax: 'dsl',
            code: 'x^2 + y^2 = 9'
        },
        'pythagorean_triangle': {
            name: '勾股定理三角形',
            syntax: 'dsl',
            code: 'point A(0, 0); point B(3, 0); point C(0, 4)\nsegment AB(A, B); segment BC(B, C); segment CA(C, A)'
        },
        'perpendicular_bisector': {
            name: '中垂线',
            syntax: 'dsl',
            code: 'point A(-1, 0); point B(1, 0)\nmidpoint M(A, B)'
        },
        'linear_equation': {
            name: '直线方程',
            syntax: 'dsl',
            code: 'y = 2x + 1'
        },
        'triangle_area_latex': {
            name: '三角形面积 (LaTeX)',
            syntax: 'latex',
            code: 'S = \\frac{1}{2} |x_1(y_2-y_3) + x_2(y_3-y_1) + x_3(y_1-y_2)|'
        },
        'distance_python': {
            name: '两点距离 (Python)',
            syntax: 'python',
            code: 'd = sqrt((x2-x1)**2 + (y2-y1)**2)'
        },
        'ellipse_equation': {
            name: '椭圆方程',
            syntax: 'dsl',
            code: 'x^2/4 + y^2/9 = 1'
        }
    };

    // ========================================================================
    //  辅助函数
    // ========================================================================

    /**
     * 创建公式卡片 DOM 元素
     * @param {string} title - 卡片标题
     * @param {string} content - 卡片内容（HTML 或 LaTeX）
     * @returns {HTMLElement} 公式卡片元素
     */
    function _createFormulaCard(title, content) {
        var card = document.createElement('div');
        card.className = 'formula-card';
        var titleEl = document.createElement('div');
        titleEl.className = 'formula-title';
        titleEl.textContent = title;
        card.appendChild(titleEl);
        var contentEl = document.createElement('div');
        contentEl.className = 'formula-content';
        contentEl.textContent = content;
        card.appendChild(contentEl);
        return card;
    }

    /**
     * 创建带标签和代码块的公式卡片 DOM 元素
     * @param {string} label - 卡片标签文本
     * @param {string} codeText - 代码块文本内容
     * @returns {HTMLElement} 公式卡片元素
     */
    function _createCodeCard(label, codeText) {
        var card = document.createElement('div');
        card.className = 'formula-card';
        var labelEl = document.createElement('div');
        labelEl.className = 'formula-card-label';
        labelEl.textContent = label;
        card.appendChild(labelEl);
        var code = document.createElement('pre');
        code.className = 'formula-code-block';
        code.textContent = codeText;
        card.appendChild(code);
        return card;
    }

    /**
     * 安全获取 DOM 元素
     */
    function _el(id) {
        return document.getElementById(id);
    }

    /**
     * 统计 AST 中的语句数量
     */
    function _countStatements(ast) {
        if (!ast) { return 0; }
        if (ast.type === 'compound' && ast.statements) {
            return ast.statements.length;
        }
        return 1;
    }

    /**
     * 向公式日志区域追加一条消息
     */
    function _appendFormulaLog(message, level) {
        var logEl = _el('formulaLog');
        if (!logEl) { return; }
        var entry = document.createElement('div');
        entry.className = 'formula-log-entry formula-log-' + (level || 'info');
        var timeSpan = document.createElement('span');
        timeSpan.className = 'formula-log-time';
        var now = new Date();
        var h = now.getHours().toString();
        if (h.length < 2) { h = '0' + h; }
        var m = now.getMinutes().toString();
        if (m.length < 2) { m = '0' + m; }
        var s = now.getSeconds().toString();
        if (s.length < 2) { s = '0' + s; }
        timeSpan.textContent = h + ':' + m + ':' + s;
        var msgSpan = document.createElement('span');
        msgSpan.className = 'formula-log-msg';
        msgSpan.textContent = message;
        entry.appendChild(timeSpan);
        entry.appendChild(msgSpan);
        logEl.appendChild(entry);
        logEl.scrollTop = logEl.scrollHeight;
    }

    /**
     * 触发文件下载
     */
    function _downloadFile(filename, content, mimeType) {
        var blob = new Blob([content], { type: mimeType || 'text/plain;charset=utf-8' });
        var url = URL.createObjectURL(blob);
        var a = document.createElement('a');
        a.href = url;
        a.download = filename;
        document.body.appendChild(a);
        a.click();
        setTimeout(function() {
            document.body.removeChild(a);
            URL.revokeObjectURL(url);
        }, 100);
    }

    // ========================================================================
    //  Lv00WebApp.prototype 方法定义
    // ========================================================================

    // === FORMULA 模块初始化 ===
    Lv00WebApp.prototype.initFormulaModule = function() {
        var self = this;

        // 模块内部状态
        this._formulaAST = null;
        this._formulaSyntax = 'auto';
        this._formulaOutputFormat = 'latex';

        // 1. 初始化 FormulaRenderer（加载 KaTeX CDN）
        if (typeof FormulaRenderer !== 'undefined') {
            FormulaRenderer.init(function() {
                if (FormulaRenderer.isReady()) {
                    self.appendLog('[FORMULA] KaTeX 渲染器加载完成', 'info');
                    _appendFormulaLog('KaTeX 渲染器加载完成', 'info');
                } else {
                    self.appendLog('[FORMULA] KaTeX 加载失败，将使用纯文本显示', 'warn');
                    _appendFormulaLog('KaTeX 加载失败，将使用纯文本显示', 'warn');
                }
            });
        } else {
            this.appendLog('[FORMULA] FormulaRenderer 未定义，跳过 KaTeX 初始化', 'warn');
        }

        // 2. 绑定所有 FORMULA 面板的按钮事件

        // 解析按钮
        this._bindButton('btnFormulaParse', function() {
            self.formulaParse();
        });

        // 渲染按钮
        this._bindButton('btnFormulaRender', function() {
            self.formulaRender();
        });

        // 公式 -> 图形按钮
        this._bindButton('btnFormulaToGraph', function() {
            self.formulaToGraph();
        });

        // 图形 -> 公式按钮
        this._bindButton('btnGraphToFormula', function() {
            self.graphToFormula();
        });

        // 清空按钮
        this._bindButton('btnFormulaClear', function() {
            self.formulaClear();
        });

        // 导出按钮
        this._bindButton('btnFormulaExport', function() {
            self.formulaExport(self._formulaOutputFormat);
        });

        // 输出格式选择器
        var outputFormatSelect = _el('formulaOutputFormat');
        if (outputFormatSelect) {
            outputFormatSelect.addEventListener('change', function() {
                self.formulaSwitchOutput(outputFormatSelect.value);
            });
        }

        // 3. 设置公式编辑器的键盘快捷键
        var formulaInput = _el('formulaInput');
        if (formulaInput) {
            formulaInput.addEventListener('keydown', function(e) {
                // Ctrl+Enter: 解析并渲染
                if (e.ctrlKey && e.key === 'Enter') {
                    e.preventDefault();
                    self.formulaParse();
                    self.formulaRender();
                }
                // Ctrl+G: 公式 -> 图形
                if (e.ctrlKey && e.key === 'g') {
                    e.preventDefault();
                    self.formulaToGraph();
                }
                // Escape: 清空
                if (e.key === 'Escape') {
                    e.preventDefault();
                    self.formulaClear();
                }
            });

            // 输入时自动检测语法
            formulaInput.addEventListener('input', function() {
                var input = formulaInput.value;
                if (input && input.trim().length > 0) {
                    var detected = 'auto';
                    if (typeof FormulaParser !== 'undefined') {
                        detected = FormulaParser.detectSyntax(input);
                    }
                    self._updateSyntaxIndicator(detected);
                } else {
                    self._updateSyntaxIndicator('');
                }
            });
        }

        this.appendLog('[FORMULA] 模块初始化完成', 'info');
        _appendFormulaLog('FORMULA 模块初始化完成', 'info');
    };

    // === 更新语法类型指示器（内部方法） ===
    Lv00WebApp.prototype._updateSyntaxIndicator = function(syntax) {
        var indicator = _el('formulaSyntaxIndicator');
        if (!indicator) { return; }
        var labels = {
            'auto': 'AUTO',
            'dsl': 'DSL',
            'latex': 'LaTeX',
            'python': 'Python',
            'mixed': 'MIXED',
            'unknown': '???'
        };
        indicator.textContent = labels[syntax] || syntax || '--';
        indicator.className = 'syntax-badge syntax-' + (syntax || 'none');
    };

    // === 公式输入和解析 ===
    /**
     * 公式解析
     *
     * @description 从公式输入框获取用户输入，自动检测语法类型，
     *              调用 FormulaParser 解析为 AST，并显示解析结果摘要。
     *              解析成功后将 AST 存入 this._formulaAST。
     *
     * @returns {void}
     */
    Lv00WebApp.prototype.formulaParse = function() {
        var inputEl = _el('formulaInput');
        if (!inputEl) {
            this.appendLog('[FORMULA] 找不到公式输入框 #formulaInput', 'error');
            return;
        }

        var input = inputEl.value;
        if (!input || !input.trim()) {
            this.appendLog('[FORMULA] 输入为空，请输入公式', 'warn');
            _appendFormulaLog('输入为空，请输入公式', 'warn');
            return;
        }

        // 检查 FormulaParser 是否可用
        if (typeof FormulaParser === 'undefined') {
            this.appendLog('[FORMULA] FormulaParser 未加载', 'error');
            _appendFormulaLog('FormulaParser 未加载，无法解析', 'error');
            return;
        }

        try {
            // 自动检测语法
            var syntax = this._formulaSyntax;
            if (syntax === 'auto') {
                syntax = FormulaParser.detectSyntax(input);
            }

            // 解析
            var ast = FormulaParser.parse(input, syntax);
            this._formulaAST = ast;

            // 更新语法指示器
            this._updateSyntaxIndicator(syntax);

            if (ast) {
                var stmtCount = _countStatements(ast);
                var msg = '[FORMULA] 解析成功: ' + stmtCount + ' 个语句, 语法: ' + syntax;
                this.appendLog(msg, 'info');
                _appendFormulaLog(msg, 'info');

                // 在输出区域显示解析摘要
                // 使用 DOM API 安全构建，避免 innerHTML XSS 风险
                var outputEl = _el('formulaOutput');
                if (outputEl) {
                    while (outputEl.firstChild) { outputEl.removeChild(outputEl.firstChild); }
                    var summary = document.createElement('div');
                    summary.className = 'formula-parse-summary';

                    // 使用 DOM API 安全构建，所有数据来自内部解析器（非用户输入）
                    var strongEl = document.createElement('strong');
                    strongEl.textContent = '解析成功';
                    summary.appendChild(strongEl);
                    summary.appendChild(document.createElement('br'));
                    summary.appendChild(document.createTextNode('语句数: ' + stmtCount));
                    summary.appendChild(document.createElement('br'));
                    summary.appendChild(document.createTextNode('语法: ' + syntax));
                    summary.appendChild(document.createElement('br'));
                    summary.appendChild(document.createTextNode('AST 类型: ' + ast.type));
                    outputEl.appendChild(summary);
                }

                this.updateStatus('FORMULA: 解析成功 / Parse OK (' + stmtCount + ' statements)');
            } else {
                var errMsg = FormulaParser.getLastError() || '未知错误';
                var errorMsg = '[FORMULA] 解析失败: ' + errMsg;
                this.appendLog(errorMsg, 'error');
                _appendFormulaLog(errorMsg, 'error');

                var outputEl2 = _el('formulaOutput');
                if (outputEl2) {
                    while (outputEl2.firstChild) { outputEl2.removeChild(outputEl2.firstChild); }
                    var errorDiv = document.createElement('div');
                    errorDiv.className = 'formula-parse-error';
                    errorDiv.textContent = '解析失败: ' + errMsg;
                    outputEl2.appendChild(errorDiv);
                }

                this.updateStatus('FORMULA: 解析失败 / Parse FAILED');
            }
        } catch (e) {
            var errMsg = e.message || '未知解析错误';
            var errorMsg = '[FORMULA] 解析异常: ' + errMsg;
            this.appendLog(errorMsg, 'error');
            _appendFormulaLog(errorMsg, 'error');
            this._formulaAST = null;

            var outputElErr = _el('formulaOutput');
            if (outputElErr) {
                while (outputElErr.firstChild) { outputElErr.removeChild(outputElErr.firstChild); }
                var errorDiv = document.createElement('div');
                errorDiv.className = 'formula-parse-error';
                errorDiv.textContent = '解析异常: ' + errMsg;
                outputElErr.appendChild(errorDiv);
            }
            this.updateStatus('FORMULA: 解析异常 / Parse ERROR');
        }
    };

    // === 公式渲染 ===
    Lv00WebApp.prototype.formulaRender = function() {
        var outputEl = _el('formulaOutput');
        if (!outputEl) {
            this.appendLog('[FORMULA] 找不到输出区域 #formulaOutput', 'error');
            return;
        }

        // 如果没有 AST，先尝试解析
        if (!this._formulaAST) {
            var inputEl = _el('formulaInput');
            if (inputEl && inputEl.value && inputEl.value.trim()) {
                this.formulaParse();
            }
        }

        var ast = this._formulaAST;
        if (!ast) {
            this.appendLog('[FORMULA] 无 AST 可渲染，请先解析公式', 'warn');
            _appendFormulaLog('无 AST 可渲染，请先解析公式', 'warn');
            return;
        }

        // 检查 FormulaParser 输出方法是否可用
        if (typeof FormulaParser === 'undefined') {
            this.appendLog('[FORMULA] FormulaParser 未加载', 'error');
            return;
        }

        var format = this._formulaOutputFormat;
        var formulas = [];
        var statements = [];

        // 提取语句列表
        if (ast.type === 'compound' && ast.statements) {
            statements = ast.statements;
        } else {
            statements = [ast];
        }

        // 将每个语句转换为指定格式
        for (var i = 0; i < statements.length; i++) {
            var stmt = statements[i];
            var text = '';
            if (format === 'latex') {
                text = FormulaParser.astToLatex(stmt);
            } else if (format === 'python') {
                text = FormulaParser.astToPython(stmt);
            } else {
                text = FormulaParser.astToDSL(stmt);
            }
            formulas.push({
                label: 'Stmt ' + (i + 1),
                latex: format === 'latex' ? text : text.replace(/</g, '&lt;').replace(/>/g, '&gt;'),
                description: text,
                displayMode: true
            });
        }

        // 渲染到输出区域
        while (outputEl.firstChild) { outputEl.removeChild(outputEl.firstChild); }

        if (typeof FormulaRenderer !== 'undefined' && FormulaRenderer.isReady()) {
            // 使用 KaTeX 渲染
            if (format === 'latex') {
                // LaTeX 格式使用 KaTeX 渲染
                FormulaRenderer.renderFormulaList(outputEl, formulas);
            } else {
                // Python / DSL 格式使用代码块显示
                for (var j = 0; j < formulas.length; j++) {
                    outputEl.appendChild(_createCodeCard(formulas[j].label, formulas[j].description));
                }
            }
        } else {
            // KaTeX 不可用，纯文本显示
            for (var k = 0; k < formulas.length; k++) {
                outputEl.appendChild(_createCodeCard(formulas[k].label, formulas[k].description));
            }
        }

        var renderMsg = '[FORMULA] 渲染完成: ' + statements.length + ' 个语句, 格式: ' + format;
        this.appendLog(renderMsg, 'info');
        _appendFormulaLog(renderMsg, 'info');
        this.updateStatus('FORMULA: 渲染完成 / Rendered (' + statements.length + ' stmts, ' + format + ')');
    };

    // === 公式 -> 图形（执行转换） ===
    Lv00WebApp.prototype.formulaToGraph = function() {
        // 如果没有 AST，先尝试解析
        if (!this._formulaAST) {
            var inputEl = _el('formulaInput');
            if (inputEl && inputEl.value && inputEl.value.trim()) {
                this.formulaParse();
            }
        }

        var ast = this._formulaAST;
        if (!ast) {
            this.appendLog('[FORMULA] 无 AST 可转换，请先输入并解析公式', 'warn');
            _appendFormulaLog('无 AST 可转换，请先输入并解析公式', 'warn');
            return;
        }

        // 检查依赖
        if (typeof FormulaToGraph === 'undefined') {
            this.appendLog('[FORMULA] FormulaToGraph 未加载', 'error');
            _appendFormulaLog('FormulaToGraph 未加载', 'error');
            return;
        }

        if (!this.graph) {
            this.appendLog('[FORMULA] 约束图未初始化', 'error');
            _appendFormulaLog('约束图未初始化', 'error');
            return;
        }

        var backend = this.jsBackend;
        if (!backend) {
            this.appendLog('[FORMULA] JS 后端未初始化，无法执行图形转换', 'error');
            _appendFormulaLog('JS 后端未初始化，无法执行图形转换', 'error');
            return;
        }

        // 执行转换
        this.appendLog('[FORMULA] 开始公式 -> 图形转换...', 'info');
        _appendFormulaLog('开始公式 -> 图形转换...', 'info');

        var result = FormulaToGraph.convert(ast, this.graph, backend);

        if (result && result.success) {
            // 统计新创建的节点
            var newPointCount = 0;
            var newSegmentCount = 0;
            var newConstraintCount = 0;

            // [修复 Bug 1] newNodes 是节点 ID 数组（数字），不是对象数组
            // 因此无法通过 .type 属性判断节点类型。
            // 策略：统计 newNodes 总长度作为新节点数，newConstraints 长度作为约束数。
            // 大多数公式操作创建的是点，所以 newNodes 长度近似等于新点数。
            if (result.newNodes) {
                newPointCount = result.newNodes.length;
            }
            if (result.newConstraints) {
                newConstraintCount = result.newConstraints.length;
            }

            // [修复 Bug 2] app.js 中没有 syncFromGraph 方法，
            // 只有 syncPointsFromGraph 和 syncSegmentsFromGraph。
            // 优先调用这两个方法，若不存在则回退到 _syncPointsFromGraph。
            if (typeof this.syncPointsFromGraph === 'function') {
                this.syncPointsFromGraph();
            }
            if (typeof this.syncSegmentsFromGraph === 'function') {
                this.syncSegmentsFromGraph();
            }
            if (typeof this.syncPointsFromGraph !== 'function' &&
                typeof this.syncSegmentsFromGraph !== 'function') {
                // 最终回退：使用内部同步方法
                this._syncPointsFromGraph();
            }

            // 重新渲染 Canvas
            this.render();

            var msg = '[FORMULA] 转换完成: 创建 ' + newPointCount + ' 个点, ' +
                      newSegmentCount + ' 条线段, ' + newConstraintCount + ' 个约束';
            this.appendLog(msg, 'info');
            _appendFormulaLog(msg, 'info');
            this.updateStatus('FORMULA: 转换完成 / Converted (+' + newPointCount + ' pts, +' + newSegmentCount + ' segs)');
        } else {
            var errors = (result && result.errors) ? result.errors : [];
            var errText = '';
            for (var j = 0; j < errors.length; j++) {
                errText += (j > 0 ? '; ' : '') + (errors[j].message || errors[j]);
            }
            var failMsg = '[FORMULA] 转换失败: ' + (errText || '未知错误');
            this.appendLog(failMsg, 'error');
            _appendFormulaLog(failMsg, 'error');
            this.updateStatus('FORMULA: 转换失败 / Convert FAILED');
        }
    };

    // === 图形 -> 公式（从当前图生成公式） ===
    Lv00WebApp.prototype.graphToFormula = function() {
        if (typeof GraphToFormula === 'undefined') {
            this.appendLog('[FORMULA] GraphToFormula 未加载', 'error');
            _appendFormulaLog('GraphToFormula 未加载', 'error');
            return;
        }

        if (!this.graph) {
            this.appendLog('[FORMULA] 约束图未初始化', 'warn');
            _appendFormulaLog('约束图未初始化', 'warn');
            return;
        }

        // 检查图是否有内容
        if (!this.graph.nodes || this.graph.nodes.length === 0) {
            this.appendLog('[FORMULA] 当前图为空，无公式可生成', 'warn');
            _appendFormulaLog('当前图为空，无公式可生成', 'warn');
            return;
        }

        this.appendLog('[FORMULA] 开始图形 -> 公式转换...', 'info');
        _appendFormulaLog('开始图形 -> 公式转换...', 'info');

        // 执行转换
        var result = GraphToFormula.convert(this.graph);

        if (result) {
            var totalFormulas = 0;
            if (result.points) { totalFormulas += result.points.length; }
            if (result.equations) { totalFormulas += result.equations.length; }
            if (result.constraints) { totalFormulas += result.constraints.length; }

            // 在输出区域渲染生成的公式
            var outputEl = _el('formulaOutput');
            if (outputEl) {
                while (outputEl.firstChild) { outputEl.removeChild(outputEl.firstChild); }

                // 渲染点公式
                if (result.points && result.points.length > 0) {
                    var sectionTitle = document.createElement('h3');
                    sectionTitle.className = 'formula-section-title';
                    sectionTitle.textContent = '点 (Points)';
                    outputEl.appendChild(sectionTitle);

                    if (typeof FormulaRenderer !== 'undefined' && FormulaRenderer.isReady()) {
                        var pointFormulas = [];
                        for (var i = 0; i < result.points.length; i++) {
                            pointFormulas.push({
                                label: result.points[i].label || ('P_' + result.points[i].id),
                                latex: result.points[i].latex,
                                description: 'Point ' + result.points[i].id,
                                displayMode: true
                            });
                        }
                        FormulaRenderer.renderFormulaList(outputEl, pointFormulas);
                    } else {
                        for (var pi = 0; pi < result.points.length; pi++) {
                            var pCard = _createFormulaCard(
                                result.points[pi].label || ('P_' + result.points[pi].id),
                                result.points[pi].latex
                            );
                            outputEl.appendChild(pCard);
                        }
                    }
                }

                // 渲染方程
                if (result.equations && result.equations.length > 0) {
                    var eqTitle = document.createElement('h3');
                    eqTitle.className = 'formula-section-title';
                    eqTitle.textContent = '方程 (Equations)';
                    outputEl.appendChild(eqTitle);

                    if (typeof FormulaRenderer !== 'undefined' && FormulaRenderer.isReady()) {
                        var eqFormulas = [];
                        for (var j = 0; j < result.equations.length; j++) {
                            eqFormulas.push({
                                label: 'Eq ' + (j + 1),
                                latex: result.equations[j].latex,
                                description: result.equations[j].description || '',
                                displayMode: true
                            });
                        }
                        FormulaRenderer.renderFormulaList(outputEl, eqFormulas);
                    } else {
                        for (var ei = 0; ei < result.equations.length; ei++) {
                            var eCard = _createFormulaCard('', result.equations[ei].latex);
                            outputEl.appendChild(eCard);
                        }
                    }
                }

                // 渲染约束
                if (result.constraints && result.constraints.length > 0) {
                    var cTitle = document.createElement('h3');
                    cTitle.className = 'formula-section-title';
                    cTitle.textContent = '约束 (Constraints)';
                    outputEl.appendChild(cTitle);

                    for (var ci = 0; ci < result.constraints.length; ci++) {
                        var cCard = _createFormulaCard('', result.constraints[ci].latex || ('Constraint type ' + result.constraints[ci].type));
                        outputEl.appendChild(cCard);
                    }
                }
            }

            // 保存完整 LaTeX 到模块状态
            this._formulaFullLatex = result.fullLatex || '';

            var msg = '[FORMULA] 图形 -> 公式: 生成 ' + totalFormulas + ' 个公式';
            this.appendLog(msg, 'info');
            _appendFormulaLog(msg, 'info');
            this.updateStatus('FORMULA: 图形 -> 公式完成 / Graph->Formula done (' + totalFormulas + ' formulas)');
        } else {
            this.appendLog('[FORMULA] 图形 -> 公式转换失败', 'error');
            _appendFormulaLog('图形 -> 公式转换失败', 'error');
            this.updateStatus('FORMULA: 转换失败 / Convert FAILED');
        }
    };

    // === 双向同步 ===
    Lv00WebApp.prototype.formulaSyncFromGraph = function() {
        if (!this.graph) { return; }
        if (!this.graph.nodes || this.graph.nodes.length === 0) { return; }

        // 仅在图形有实际内容时才自动同步
        if (typeof GraphToFormula === 'undefined') { return; }

        var result = GraphToFormula.convert(this.graph);
        if (!result) { return; }

        // 更新公式显示区域（静默模式，不写日志）
        var outputEl = _el('formulaOutput');
        if (outputEl && result.fullLatex) {
            // 仅在输出区域为空或包含旧同步内容时更新
            if (!outputEl.hasChildNodes() || outputEl.getAttribute('data-sync') === 'true') {
                while (outputEl.firstChild) { outputEl.removeChild(outputEl.firstChild); }
                outputEl.setAttribute('data-sync', 'true');

                var syncLabel = document.createElement('div');
                syncLabel.className = 'formula-sync-label';
                syncLabel.textContent = '[自动同步 / Auto-sync]';
                outputEl.appendChild(syncLabel);

                if (typeof FormulaRenderer !== 'undefined' && FormulaRenderer.isReady()) {
                    FormulaRenderer.renderToElement(outputEl, result.fullLatex, true);
                } else {
                    var pre = document.createElement('pre');
                    pre.className = 'formula-code-block';
                    pre.textContent = result.fullLatex;
                    outputEl.appendChild(pre);
                }
            }
        }

        this._formulaFullLatex = result.fullLatex || '';
    };

    // === 语法切换 ===
    Lv00WebApp.prototype.formulaSwitchSyntax = function(syntax) {
        var validSyntaxes = ['auto', 'latex', 'python', 'dsl'];
        if (validSyntaxes.indexOf(syntax) < 0) {
            this.appendLog('[FORMULA] 无效的语法类型: ' + syntax, 'warn');
            return;
        }

        this._formulaSyntax = syntax;
        this._updateSyntaxIndicator(syntax);

        var msg = '[FORMULA] 语法切换: ' + syntax;
        this.appendLog(msg, 'info');
        _appendFormulaLog(msg, 'info');

        // 如果编辑器有内容，尝试重新解析
        var inputEl = _el('formulaInput');
        if (inputEl && inputEl.value && inputEl.value.trim()) {
            this.formulaParse();
        }
    };

    // === 输出格式切换 ===
    Lv00WebApp.prototype.formulaSwitchOutput = function(format) {
        var validFormats = ['latex', 'python', 'dsl'];
        if (validFormats.indexOf(format) < 0) {
            this.appendLog('[FORMULA] 无效的输出格式: ' + format, 'warn');
            return;
        }

        this._formulaOutputFormat = format;

        var msg = '[FORMULA] 输出格式切换: ' + format;
        this.appendLog(msg, 'info');
        _appendFormulaLog(msg, 'info');

        // 如果有 AST，重新渲染
        if (this._formulaAST) {
            this.formulaRender();
        }
    };

    // === 清空公式编辑器 ===
    Lv00WebApp.prototype.formulaClear = function() {
        var inputEl = _el('formulaInput');
        if (inputEl) {
            inputEl.value = '';
        }

        var outputEl = _el('formulaOutput');
        if (outputEl) {
            while (outputEl.firstChild) { outputEl.removeChild(outputEl.firstChild); }
            outputEl.removeAttribute('data-sync');
        }

        this._formulaAST = null;
        this._formulaFullLatex = '';
        this._updateSyntaxIndicator('');

        this.appendLog('[FORMULA] 编辑器已清空', 'info');
        _appendFormulaLog('编辑器已清空', 'info');
        this.updateStatus('FORMULA: 已清空 / Cleared');
    };

    // === 加载示例公式 ===
    Lv00WebApp.prototype.formulaLoadExample = function(exampleName) {
        var example = FORMULA_EXAMPLES[exampleName];
        if (!example) {
            this.appendLog('[FORMULA] 未找到示例: ' + exampleName, 'warn');
            _appendFormulaLog('未找到示例: ' + exampleName, 'warn');
            return;
        }

        var inputEl = _el('formulaInput');
        if (inputEl) {
            inputEl.value = example.code;
        }

        // 切换到示例推荐的语法模式
        this._formulaSyntax = example.syntax;
        this._updateSyntaxIndicator(example.syntax);

        var msg = '[FORMULA] 加载示例: ' + example.name + ' (语法: ' + example.syntax + ')';
        this.appendLog(msg, 'info');
        _appendFormulaLog(msg, 'info');

        // 自动解析并渲染
        this.formulaParse();
        this.formulaRender();

        this.updateStatus('FORMULA: 示例已加载 / Example loaded - ' + example.name);
    };

    // === 导出公式 ===
    Lv00WebApp.prototype.formulaExport = function(format) {
        format = format || this._formulaOutputFormat || 'latex';

        var content = '';
        var filename = '';
        var mimeType = 'text/plain;charset=utf-8';

        // 如果有图形 -> 公式的完整 LaTeX
        if (this._formulaFullLatex && !this._formulaAST) {
            content = this._formulaFullLatex;
            filename = 'lv00_formulas.tex';
            mimeType = 'application/x-tex;charset=utf-8';
        }
        // 如果有 AST，从 AST 导出
        else if (this._formulaAST) {
            if (typeof FormulaParser === 'undefined') {
                this.appendLog('[FORMULA] FormulaParser 未加载，无法导出', 'error');
                return;
            }

            var statements = [];
            if (this._formulaAST.type === 'compound' && this._formulaAST.statements) {
                statements = this._formulaAST.statements;
            } else {
                statements = [this._formulaAST];
            }

            if (format === 'latex') {
                // LaTeX 文件
                var lines = ['\\documentclass{article}', '\\usepackage{amsmath}', '\\usepackage{amssymb}', '', '\\begin{document}', ''];
                for (var i = 0; i < statements.length; i++) {
                    lines.push('$$ ' + FormulaParser.astToLatex(statements[i]) + ' $$');
                    lines.push('');
                }
                lines.push('\\end{document}');
                content = lines.join('\n');
                filename = 'lv00_formulas.tex';
                mimeType = 'application/x-tex;charset=utf-8';
            } else if (format === 'python') {
                // Python 文件
                var pyLines = ['# Lv-00 Formula Export', '# Generated: ' + new Date().toISOString(), ''];
                for (var j = 0; j < statements.length; j++) {
                    pyLines.push('# Formula ' + (j + 1));
                    pyLines.push(FormulaParser.astToPython(statements[j]));
                    pyLines.push('');
                }
                content = pyLines.join('\n');
                filename = 'lv00_formulas.py';
                mimeType = 'text/x-python;charset=utf-8';
            } else {
                // DSL 文件
                var dslLines = ['// Lv-00 DSL Export', '// Generated: ' + new Date().toISOString(), ''];
                for (var k = 0; k < statements.length; k++) {
                    dslLines.push(FormulaParser.astToDSL(statements[k]));
                    dslLines.push('');
                }
                content = dslLines.join('\n');
                filename = 'lv00_formulas.dsl';
                mimeType = 'text/plain;charset=utf-8';
            }
        } else {
            // 尝试从输入框导出
            var inputEl = _el('formulaInput');
            if (inputEl && inputEl.value && inputEl.value.trim()) {
                content = inputEl.value;
                filename = 'lv00_formulas.txt';
            } else {
                this.appendLog('[FORMULA] 无内容可导出', 'warn');
                _appendFormulaLog('无内容可导出', 'warn');
                return;
            }
        }

        // 如果格式是 html，生成 HTML 文件
        if (format === 'html') {
            var latexContent = content;
            if (this._formulaAST && typeof FormulaParser !== 'undefined') {
                var stmts = [];
                if (this._formulaAST.type === 'compound' && this._formulaAST.statements) {
                    stmts = this._formulaAST.statements;
                } else {
                    stmts = [this._formulaAST];
                }
                var latexParts = [];
                for (var li = 0; li < stmts.length; li++) {
                    latexParts.push(FormulaParser.astToLatex(stmts[li]));
                }
                latexContent = latexParts.join('<br>');
            }

            content =
                '<!DOCTYPE html>\n' +
                '<html>\n<head>\n' +
                '  <meta charset="utf-8">\n' +
                '  <title>Lv-00 Formula Export</title>\n' +
                '  <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/katex@0.16.9/dist/katex.min.css">\n' +
                '</head>\n<body>\n' +
                '<h1>Lv-00 Formula Export</h1>\n' +
                '<div id="formulas">\n' +
                latexContent + '\n' +
                '</div>\n' +
                '<script src="https://cdn.jsdelivr.net/npm/katex@0.16.9/dist/katex.min.js"><\/script>\n' +
                '<script>\n' +
                '  document.addEventListener("DOMContentLoaded", function() {\n' +
                '    var container = document.getElementById("formulas");\n' +
                '    var html = container.innerHTML;\n' +
                '    container.innerHTML = "";\n' +
                '    var parts = html.split("<br>");\n' +
                '    for (var i = 0; i < parts.length; i++) {\n' +
                '      var div = document.createElement("div");\n' +
                '      div.style.margin = "16px 0";\n' +
                '      container.appendChild(div);\n' +
                '      katex.render(parts[i].trim(), div, {displayMode: true, throwOnError: false});\n' +
                '    }\n' +
                '  });\n' +
                '<\/script>\n' +
                '</body>\n</html>';
            filename = 'lv00_formulas.html';
            mimeType = 'text/html;charset=utf-8';
        }

        _downloadFile(filename, content, mimeType);

        var exportMsg = '[FORMULA] 导出成功: ' + filename + ' (格式: ' + format + ')';
        this.appendLog(exportMsg, 'info');
        _appendFormulaLog(exportMsg, 'info');
        this.updateStatus('FORMULA: 已导出 / Exported - ' + filename);
    };

    // === 内部方法：从约束图同步前端点数组 ===
    // [修复 Bug 3] 兼容两种数据模型：
    //   - 旧模型（formula_to_graph.js）使用 node.type === 'point' / 'line_segment'（字符串）
    //   - 新模型（app.js syncPointsFromGraph）使用 node.geomType === 0（点）/ 1（线段）（数字枚举）
    //   优先检查 geomType（数字，来自 lv00_js_backend.js GeomType 枚举），回退到 type 字符串。
    //   确保两种数据模型都能正确识别节点类型，避免图转公式时丢失节点数据。
    Lv00WebApp.prototype._syncPointsFromGraph = function() {
        if (!this.graph || !this.graph.nodes) { return; }

        this.points = [];
        this.segments = [];

        for (var i = 0; i < this.graph.nodes.length; i++) {
            var node = this.graph.nodes[i];

            // 判断节点是否为"点"：优先使用 geomType 数字枚举，回退到 type 字符串
            var isPoint = false;
            if (node.geomType !== undefined) {
                // 新模型：geomType === 0 表示点
                isPoint = (node.geomType === 0);
            } else if (node.type !== undefined) {
                // 旧模型：type === 'point' 表示点
                isPoint = (node.type === 'point');
            }

            // 判断节点是否为"线段"：优先使用 geomType 数字枚举，回退到 type 字符串
            var isSegment = false;
            if (node.geomType !== undefined) {
                // 新模型：geomType === 1 表示线段
                isSegment = (node.geomType === 1);
            } else if (node.type !== undefined) {
                // 旧模型：type === 'line_segment' 或 'segment' 表示线段
                isSegment = (node.type === 'line_segment' || node.type === 'segment');
            }

            if (isPoint) {
                var px = 0, py = 0;
                // 兼容两种坐标格式
                if (node.symbolic_coords && node.coord_count >= 2) {
                    px = node.symbolic_coords[0].value || 0;
                    py = node.symbolic_coords[1].value || 0;
                } else if (node.x !== undefined && node.y !== undefined) {
                    // 新模型可能直接提供 x, y 数值
                    px = typeof node.x === 'number' ? node.x : (node.x.value || 0);
                    py = typeof node.y === 'number' ? node.y : (node.y.value || 0);
                }
                this.points.push({
                    id: node.id,
                    x: px,
                    y: py,
                    nodeId: node.id
                });
            } else if (isSegment) {
                var p1Id = null, p2Id = null;
                if (node.endpoints && node.endpoint_count >= 2) {
                    p1Id = node.endpoints[0];
                    p2Id = node.endpoints[1];
                } else if (node.p1 !== undefined && node.p2 !== undefined) {
                    // 新模型可能直接提供 p1, p2
                    p1Id = node.p1;
                    p2Id = node.p2;
                }
                this.segments.push({
                    id: node.id,
                    p1: p1Id,
                    p2: p2Id,
                    nodeId: node.id
                });
            }
        }
    };

    // === 获取示例公式列表（供外部调用） ===
    Lv00WebApp.prototype.getFormulaExamples = function() {
        var list = [];
        var keys = Object.keys(FORMULA_EXAMPLES);
        for (var i = 0; i < keys.length; i++) {
            list.push({
                key: keys[i],
                name: FORMULA_EXAMPLES[keys[i]].name,
                syntax: FORMULA_EXAMPLES[keys[i]].syntax
            });
        }
        return list;
    };

})();
