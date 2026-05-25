/**
 * ============================================================================
 *  Lv-00 几何元语言系统 - 公式解析器 (Formula Parser)
 * ============================================================================
 *
 *  用途：支持 LaTeX、Python 数学语法、Lv-00 DSL 三种输入语法的自动检测与解析，
 *        统一转换为抽象语法树 (AST)，并支持 AST 到各语法的反向输出。
 *
 *  语法要求：严格 ES5（无 class, const, let, arrow functions,
 *            template literals, destructuring, default params, spread）。
 *
 *  作者：Lv-00 Team
 *  创建日期：2026-05-20
 *  版本：1.0.0
 *
 *  AST 节点类型枚举（字符串）：
 *    'number'              - 数值常量
 *    'variable'            - 变量
 *    'binary_op'           - 二元运算
 *    'unary_op'            - 一元运算
 *    'function_call'       - 函数调用
 *    'equation'            - 方程
 *    'inequality'          - 不等式
 *    'geometric_point'     - 几何点
 *    'geometric_segment'   - 几何线段
 *    'geometric_circle'    - 几何圆
 *    'geometric_triangle'  - 几何三角形
 *    'geometric_line'      - 几何直线
 *    'geometric_region'    - 几何区域
 *    'geometric_constraint' - 几何约束
 *    'geometric_expression' - 几何表达式
 *    'compound'            - 复合语句
 * ============================================================================
 */

var FormulaParser = (function() {
    'use strict';

    // ========================================================================
    //  内部状态
    // ========================================================================
    var _lastError = null;

    function _setError(msg) {
        _lastError = msg;
    }

    // ========================================================================
    //  detectSyntax 正则缓存（惰性初始化，首次调用时构建）
    // ========================================================================

    /**
     * 缓存的语法检测正则模式。
     * 结构：
     *   dslKeywords  - DSL 关键字正则数组
     *   latexMixed   - DSL+LaTeX 混合检测正则
     *   latexPatterns - LaTeX 特征正则数组
     *   pythonSingle - 单个 LaTeX 特征时的 Python 混合检测正则
     *   pythonPatterns - Python 特征正则数组
     *   dslFallback  - 默认 DSL 方程检测正则
     */
    var _syntaxPatterns = null;

    function _ensureSyntaxPatterns() {
        if (_syntaxPatterns) return;
        _syntaxPatterns = {
            /* DSL 关键字检测（优先级最高，因为 DSL 最具体） */
            dslKeywords: [
                /\bpoint\s+\w+\s*\(/i,
                /\bsegment\s+\w+/i,
                /\bcircle\s+\w+\s*\(/i,
                /\btriangle\s+\w+/i,
                /\bline\s+\w+\s*\(/i,
                /\bperpendicular\s*\(/i,
                /\bparallel\s*\(/i,
                /\bmidpoint\s*\(/i,
                /\bangle\s*\(/i,
                /\bdistance\s*\(/i,
                /\barea\s*\(/i,
                /\bperimeter\s*\(/i,
                /\bregion\s+\w+/i
            ],
            /* DSL+LaTeX 混合检测 */
            latexMixed: /\\frac|\\sqrt|\\sin|\\cos|\\tan|\\log|\\exp|\\pi|\\theta|\\alpha|\\sum|\\int/,
            /* LaTeX 特征检测 */
            latexPatterns: [
                /\\frac\s*\{/,
                /\\sqrt\s*(\[.*?\])?\s*\{/,
                /\\(sin|cos|tan|log|exp|abs|sec|csc|cot)\b/,
                /\\(pi|theta|alpha|beta|gamma|delta|epsilon|lambda|mu|sigma|omega|phi|psi|infty)\b/,
                /\\cdot/,
                /\\times/,
                /\\(leq|geq|neq|approx|equiv)\b/,
                /\\sum\b/,
                /\\int\b/,
                /\^\s*\{/,
                /_\s*\{/
            ],
            /* 单个 LaTeX 特征时的 Python 混合检测 */
            pythonSingle: [/\*\*/, /==/],
            /* Python 特征检测 */
            pythonPatterns: [
                /\*\*/,
                /\bsqrt\s*\(/,
                /\bsin\s*\(/,
                /\bcos\s*\(/,
                /\btan\s*\(/,
                /\blog\s*\(/,
                /\bexp\s*\(/,
                /\babs\s*\(/,
                /\bpi\b/
            ],
            /* 默认 DSL 方程检测 */
            dslFallback: /=/
        };
    }

    // ========================================================================
    //  AST 工厂函数
    // ========================================================================

    function _num(val) {
        return { type: 'number', value: val };
    }

    function _var(name) {
        return { type: 'variable', name: name };
    }

    function _binOp(op, left, right) {
        return { type: 'binary_op', operator: op, left: left, right: right };
    }

    function _unOp(op, operand) {
        return { type: 'unary_op', operator: op, operand: operand };
    }

    function _func(name, args) {
        return { type: 'function_call', name: name, arguments: args };
    }

    function _eq(left, right) {
        return { type: 'equation', left: left, right: right };
    }

    function _ineq(op, left, right) {
        return { type: 'inequality', operator: op, left: left, right: right };
    }

    function _geoPoint(name, x, y) {
        return { type: 'geometric_point', name: name, x: x, y: y };
    }

    function _geoSegment(name, p1, p2) {
        return { type: 'geometric_segment', name: name, point1: p1, point2: p2 };
    }

    function _geoCircle(name, center, radius) {
        return { type: 'geometric_circle', name: name, center: center, radius: radius };
    }

    function _geoTriangle(name, p1, p2, p3) {
        return { type: 'geometric_triangle', name: name, point1: p1, point2: p2, point3: p3 };
    }

    function _geoLine(name, p1, p2) {
        return { type: 'geometric_line', name: name, point1: p1, point2: p2 };
    }

    function _geoRegion(name, shape) {
        return { type: 'geometric_region', name: name, shape: shape };
    }

    function _geoConstraint(kind, args) {
        return { type: 'geometric_constraint', kind: kind, arguments: args };
    }

    function _geoExpr(expr) {
        return { type: 'geometric_expression', expression: expr };
    }

    function _compound(statements) {
        return { type: 'compound', statements: statements };
    }

    // ========================================================================
    //  语法检测
    // ========================================================================

    /**
     * 检测输入语法类型（自动语法识别）
     * 根据输入字符串中的关键字和语法特征，自动判别输入属于哪种语法风格：
     *   - 'dsl': 包含 Lv-00 领域特定语言关键字（point, segment, circle 等）
     *   - 'latex': 包含 LaTeX 命令（\frac, \sqrt, \sin 等）
     *   - 'python': 包含 Python 特征（import, def, class 等）
     *   - 'mixed': 同时包含 DSL 和 LaTeX 特征时
     *   - 'unknown': 无法识别时
     *
     * 性能说明：正则表达式已提升为模块级常量（_syntaxPatterns），通过惰性初始化
     * 在首次调用 detectSyntax 时构建，后续调用直接复用编译好的正则对象，
     * 避免重复编译开销，尤其在批量解析场景下。
     *
     * @param {string} input - 原始输入字符串
     * @returns {string} 语法类型标识符：'latex' | 'python' | 'dsl' | 'mixed' | 'unknown'
     */
    function detectSyntax(input) {
        if (!input || typeof input !== 'string') {
            return 'unknown';
        }

        var trimmed = input.replace(/^\s+|\s+$/g, '');
        var lower = trimmed.toLowerCase();

        /* 确保正则缓存已初始化 */
        _ensureSyntaxPatterns();
        var p = _syntaxPatterns;

        // DSL 关键字检测（优先级最高，因为 DSL 最具体）
        var dslCount = 0;
        for (var i = 0; i < p.dslKeywords.length; i++) {
            if (p.dslKeywords[i].test(trimmed)) {
                dslCount++;
            }
        }
        if (dslCount > 0) {
            // 如果同时有 LaTeX 特征，标记为 mixed
            if (p.latexMixed.test(trimmed)) {
                return 'mixed';
            }
            return 'dsl';
        }

        // LaTeX 特征检测
        var latexCount = 0;
        for (var j = 0; j < p.latexPatterns.length; j++) {
            if (p.latexPatterns[j].test(trimmed)) {
                latexCount++;
            }
        }
        if (latexCount >= 2) {
            return 'latex';
        }
        if (latexCount === 1) {
            // 单个 LaTeX 特征，检查是否有 Python 特征
            for (var k = 0; k < p.pythonSingle.length; k++) {
                if (p.pythonSingle[k].test(trimmed)) {
                    return 'mixed';
                }
            }
            return 'latex';
        }

        // Python 特征检测
        for (var m = 0; m < p.pythonPatterns.length; m++) {
            if (p.pythonPatterns[m].test(lower)) {
                return 'python';
            }
        }

        // 默认：包含 = 的当作 DSL 代数方程
        if (p.dslFallback.test(trimmed)) {
            return 'dsl';
        }

        // 纯数学表达式，默认 Python 语法
        return 'python';
    }

    // ========================================================================
    //  LaTeX 解析器
    // ========================================================================

    var LatexParser = (function() {

        /**
         * LaTeX 词法分析器
         * 将 LaTeX 字符串分解为 token 数组
         *
         * 说明：三个解析器（LatexParser / PythonParser / DSLParser）各自拥有独立的
         * tokenize 函数，结构高度相似（都是逐字符扫描、按规则分词的 while 循环），
         * 但并非无意义的重复——每种语法有不同的词法规则：
         *   - LaTeX: 反斜杠命令（\frac, \sqrt）、花括号分组、特殊希腊字母
         *   - Python: 双字符运算符（**, //, ==）、内置函数调用
         *   - DSL: 几何关键字（point, segment）、换行作为语句分隔
         * 将来可通过提取公共 Tokenizer 基类 + 子类覆盖字符分类回调的方式消除重复，
         * 但当前 ES5 环境下保留三个独立函数以保持代码可读性和维护独立性。
         */
        function tokenize(input) {
            var tokens = [];
            var pos = 0;
            var len = input.length;

            while (pos < len) {
                var ch = input.charAt(pos);

                // 跳过空白
                if (/\s/.test(ch)) {
                    pos++;
                    continue;
                }

                // 反斜杠命令
                if (ch === '\\') {
                    var cmd = '';
                    pos++;
                    while (pos < len && /[a-zA-Z]/.test(input.charAt(pos))) {
                        cmd += input.charAt(pos);
                        pos++;
                    }
                    if (cmd.length > 0) {
                        tokens.push({ type: 'command', value: cmd });
                    } else {
                        // 转义字符
                        if (pos < len) {
                            tokens.push({ type: 'char', value: input.charAt(pos) });
                            pos++;
                        }
                    }
                    continue;
                }

                // 数字
                if (/[0-9]/.test(ch) || (ch === '.' && pos + 1 < len && /[0-9]/.test(input.charAt(pos + 1)))) {
                    var numStr = '';
                    var hasDot = false;
                    while (pos < len && (/[0-9]/.test(input.charAt(pos)) || (input.charAt(pos) === '.' && !hasDot))) {
                        if (input.charAt(pos) === '.') {
                            hasDot = true;
                        }
                        numStr += input.charAt(pos);
                        pos++;
                    }
                    tokens.push({ type: 'number', value: numStr });
                    continue;
                }

                // 标识符 / 变量名
                if (/[a-zA-Z_]/.test(ch)) {
                    var ident = '';
                    while (pos < len && /[a-zA-Z_0-9]/.test(input.charAt(pos))) {
                        ident += input.charAt(pos);
                        pos++;
                    }
                    tokens.push({ type: 'identifier', value: ident });
                    continue;
                }

                // 运算符
                if (ch === '^' || ch === '_' || ch === '+' || ch === '-' || ch === '*' || ch === '/' ||
                    ch === '(' || ch === ')' || ch === '{' || ch === '}' || ch === '[' || ch === ']' ||
                    ch === ',' || ch === '=' || ch === '!' || ch === '<' || ch === '>' || ch === ';') {
                    tokens.push({ type: 'char', value: ch });
                    pos++;
                    continue;
                }

                // 未知字符，跳过
                pos++;
            }

            return tokens;
        }

        /**
         * LaTeX 递归下降解析器
         */
        function Parser(tokens) {
            this.tokens = tokens;
            this.pos = 0;
            this.error = null;
        }

        Parser.prototype.peek = function() {
            if (this.pos < this.tokens.length) {
                return this.tokens[this.pos];
            }
            return null;
        };

        Parser.prototype.advance = function() {
            var tok = this.tokens[this.pos];
            this.pos++;
            return tok;
        };

        Parser.prototype.expect = function(type, value) {
            var tok = this.peek();
            if (!tok) {
                this.error = 'Unexpected end of input, expected ' + type + (value ? ' "' + value + '"' : '');
                return null;
            }
            if (tok.type !== type || (value !== undefined && tok.value !== value)) {
                this.error = 'Unexpected token ' + JSON.stringify(tok.value) + ' at position ' + this.pos +
                    ', expected ' + type + (value ? ' "' + value + '"' : '');
                return null;
            }
            return this.advance();
        };

        Parser.prototype.match = function(type, value) {
            var tok = this.peek();
            if (tok && tok.type === type && (value === undefined || tok.value === value)) {
                return true;
            }
            return false;
        };

        Parser.prototype.matchCommand = function(cmd) {
            var tok = this.peek();
            return tok && tok.type === 'command' && tok.value === cmd;
        };

        /**
         * 解析入口：支持分号分隔的复合语句
         */
        Parser.prototype.parseAll = function() {
            var statements = [];
            var stmt = this.parseStatement();
            if (!stmt && this.error) {
                return null;
            }
            if (stmt) {
                statements.push(stmt);
            }
            while (this.match('char', ';')) {
                this.advance();
                stmt = this.parseStatement();
                if (stmt) {
                    statements.push(stmt);
                }
            }
            if (statements.length === 1) {
                return statements[0];
            }
            return _compound(statements);
        };

        /**
         * 解析单条语句
         */
        Parser.prototype.parseStatement = function() {
            return this.parseComparison();
        };

        /**
         * 解析比较运算（等式 / 不等式）
         * 处理 =, !=, <, >, <=, >= 等比较运算符。
         * 优先级最低，先解析加减表达式再检查比较运算符。
         * 属于递归下降解析链的最底层（入口层）。
         */
        Parser.prototype.parseComparison = function() {
            var left = this.parseAddSub();

            if (this.match('char', '=')) {
                this.advance();
                var right = this.parseAddSub();
                return _eq(left, right);
            }

            if (this.match('char', '<')) {
                this.advance();
                if (this.match('char', '=')) {
                    this.advance();
                    var right2 = this.parseAddSub();
                    return _ineq('<=', left, right2);
                }
                var right3 = this.parseAddSub();
                return _ineq('<', left, right3);
            }

            if (this.match('char', '>')) {
                this.advance();
                if (this.match('char', '=')) {
                    this.advance();
                    var right4 = this.parseAddSub();
                    return _ineq('>=', left, right4);
                }
                var right5 = this.parseAddSub();
                return _ineq('>', left, right5);
            }

            // LaTeX 不等式命令
            if (this.matchCommand('leq')) {
                this.advance();
                var right6 = this.parseAddSub();
                return _ineq('<=', left, right6);
            }
            if (this.matchCommand('geq')) {
                this.advance();
                var right7 = this.parseAddSub();
                return _ineq('>=', left, right7);
            }
            if (this.matchCommand('neq')) {
                this.advance();
                var right8 = this.parseAddSub();
                return _ineq('!=', left, right8);
            }

            return left;
        };

        /**
         * 解析加减法表达式（+ 和 -）
         * 递归下降解析链的第二层，处理加法与减法二元运算。
         * 左结合，先解析乘除表达式再逐个收集加减项。
         */
        Parser.prototype.parseAddSub = function() {
            var left = this.parseMulDiv();

            while (true) {
                if (this.match('char', '+')) {
                    this.advance();
                    var right = this.parseMulDiv();
                    left = _binOp('+', left, right);
                } else if (this.match('char', '-')) {
                    this.advance();
                    var right2 = this.parseMulDiv();
                    left = _binOp('-', left, right2);
                } else {
                    break;
                }
            }

            return left;
        };

        /**
         * 解析乘除法表达式（* 和 /，含 \cdot 和 \times）
         * 递归下降解析链的第三层，处理乘法与除法二元运算。
         * 左结合，先解析幂运算后再收集乘除项。
         */
        Parser.prototype.parseMulDiv = function() {
            var left = this.parsePower();

            while (true) {
                if (this.match('char', '*') || this.match('char', '/') || this.matchCommand('cdot') || this.matchCommand('times')) {
                    var op = '/';
                    if (this.match('char', '*') || this.matchCommand('cdot') || this.matchCommand('times')) {
                        op = '*';
                    }
                    this.advance();
                    var right = this.parsePower();
                    left = _binOp(op, left, right);
                } else {
                    // 隐式乘法：数字后跟标识符或左括号
                    var next = this.peek();
                    if (next && (next.type === 'identifier' || (next.type === 'char' && next.value === '('))) {
                        // 检查左节点是否为数字或变量（适合隐式乘法）
                        if (left.type === 'number' || left.type === 'variable' ||
                            (left.type === 'binary_op' && left.operator === '^') ||
                            (left.type === 'unary_op') ||
                            (left.type === 'function_call')) {
                            var right2 = this.parsePower();
                            left = _binOp('*', left, right2);
                        } else {
                            break;
                        }
                    } else {
                        break;
                    }
                }
            }

            return left;
        };

        /**
         * 解析幂运算（右结合 ^）
         * 递归下降解析链的第四层，处理指数/幂运算。
         * 注意：幂运算是右结合的（a^b^c = a^(b^c)），因此递归调用自身。
         * 先解析一元表达式作为底数，再检查指数。
         */
        Parser.prototype.parsePower = function() {
            var base = this.parseUnary();

            if (this.match('char', '^')) {
                this.advance();
                var exp = this.parsePower(); // 右结合递归
                return _binOp('^', base, exp);
            }

            // 下标处理
            if (this.match('char', '_')) {
                this.advance();
                var subscript = null;
                if (this.match('char', '{')) {
                    this.advance();
                    subscript = this.parseAddSub();
                    this.expect('char', '}');
                } else if (this.peek() && this.peek().type === 'number') {
                    var numTok = this.advance();
                    subscript = _num(parseFloat(numTok.value));
                } else if (this.peek() && this.peek().type === 'identifier') {
                    var idTok = this.advance();
                    subscript = _var(idTok.value);
                }
                if (subscript) {
                    // 下标变量：x_n -> variable('x_n')
                    if (base.type === 'variable') {
                        var subName = '';
                        if (subscript.type === 'number') {
                            subName = String(subscript.value);
                        } else if (subscript.type === 'variable') {
                            subName = subscript.name;
                        } else {
                            subName = String(subscript.value || '');
                        }
                        return _var(base.name + '_' + subName);
                    }
                }
            }

            return base;
        };

        /**
         * 解析一元运算符和特殊函数
         * 递归下降解析链的第五层，处理一元负号、正号、
         * 三角函数（sin/cos/tan）、LaTeX 命令（\sqrt、\frac、\sum、\int 等）。
         * 一元负号可递归嵌套（如 --x）。
         */
        Parser.prototype.parseUnary = function() {
            // 负号
            if (this.match('char', '-')) {
                this.advance();
                var operand = this.parseUnary();
                return _unOp('-', operand);
            }

            // 正号
            if (this.match('char', '+')) {
                this.advance();
                return this.parseUnary();
            }

            // LaTeX 命令
            if (this.peek() && this.peek().type === 'command') {
                var cmd = this.peek().value;

                // 三角函数和对数等
                var unaryCmds = {
                    'sin': 'sin', 'cos': 'cos', 'tan': 'tan',
                    'sec': 'sec', 'csc': 'csc', 'cot': 'cot',
                    'log': 'log', 'ln': 'log', 'exp': 'exp',
                    'abs': 'abs', 'neg': '-'
                };

                if (unaryCmds[cmd]) {
                    this.advance();
                    var arg = this.parseAtom();
                    return _unOp(unaryCmds[cmd], arg);
                }

                // \frac{分子}{分母}
                if (cmd === 'frac') {
                    this.advance();
                    this.expect('char', '{');
                    var num = this.parseAddSub();
                    this.expect('char', '}');
                    this.expect('char', '{');
                    var den = this.parseAddSub();
                    this.expect('char', '}');
                    return _binOp('/', num, den);
                }

                // \sqrt{表达式} 或 \sqrt[n]{表达式}
                if (cmd === 'sqrt') {
                    this.advance();
                    var n = null;
                    if (this.match('char', '[')) {
                        this.advance();
                        n = this.parseAddSub();
                        this.expect('char', ']');
                    }
                    this.expect('char', '{');
                    var expr = this.parseAddSub();
                    this.expect('char', '}');
                    if (n) {
                        // \sqrt[n]{x} = x^(1/n)
                        return _binOp('^', _unOp('sqrt', expr), _binOp('/', _num(1), n));
                    }
                    return _unOp('sqrt', expr);
                }

                // \sum 和 \int（简化处理为函数调用）
                if (cmd === 'sum' || cmd === 'int') {
                    this.advance();
                    var lower = null;
                    var upper = null;
                    // \sum_{lower}^{upper}
                    if (this.match('char', '_')) {
                        this.advance();
                        if (this.match('char', '{')) {
                            this.advance();
                            lower = this.parseAddSub();
                            this.expect('char', '}');
                        } else {
                            lower = this.parseAtom();
                        }
                    }
                    if (this.match('char', '^')) {
                        this.advance();
                        if (this.match('char', '{')) {
                            this.advance();
                            upper = this.parseAddSub();
                            this.expect('char', '}');
                        } else {
                            upper = this.parseAtom();
                        }
                    }
                    var body = this.parseAtom();
                    var sumArgs = [];
                    if (lower) { sumArgs.push(lower); }
                    if (upper) { sumArgs.push(upper); }
                    sumArgs.push(body);
                    return _func(cmd, sumArgs);
                }

                // 希腊字母和特殊常量
                var greekMap = {
                    'pi': 'pi', 'theta': 'theta', 'alpha': 'alpha',
                    'beta': 'beta', 'gamma': 'gamma', 'delta': 'delta',
                    'epsilon': 'epsilon', 'lambda': 'lambda', 'mu': 'mu',
                    'sigma': 'sigma', 'omega': 'omega', 'phi': 'phi',
                    'psi': 'psi', 'rho': 'rho', 'tau': 'tau',
                    'eta': 'eta', 'zeta': 'zeta', 'iota': 'iota',
                    'kappa': 'kappa', 'nu': 'nu', 'xi': 'xi',
                    'chi': 'chi', 'upsilon': 'upsilon'
                };

                if (greekMap[cmd]) {
                    this.advance();
                    return _var(greekMap[cmd]);
                }

                // \infty
                if (cmd === 'infty') {
                    this.advance();
                    return _num(Infinity);
                }

                // \left, \right - 跳过
                if (cmd === 'left' || cmd === 'right' || cmd === 'displaystyle' || cmd === 'textstyle' || cmd === 'big' || cmd === 'Big' || cmd === 'bigg' || cmd === 'Bigg') {
                    this.advance();
                    return this.parseAtom();
                }
            }

            return this.parseAtom();
        };

        /**
         * 解析原子表达式（数字、变量、括号表达式）
         * 递归下降解析链的最底层（终结符层），处理不可再分的原子单元：
         *   - 数字常量（整数、小数）
         *   - 变量标识符（字母序列、希腊字母名称）
         *   - 括号子表达式（递归调用 parseAddSub）
         *   - 绝对值表达式 |...|
         *   - LaTeX 命令（\left, \right, \big 等装饰命令）
         *
         * 当遇到无法识别的 token 时，回退到 parseUnary 尝试其他解析路径。
         */
        Parser.prototype.parseAtom = function() {
            var tok = this.peek();

            if (!tok) {
                this.error = 'Unexpected end of input';
                return _num(0);
            }

            // 数字
            if (tok.type === 'number') {
                this.advance();
                return _num(parseFloat(tok.value));
            }

            // 标识符 / 变量
            if (tok.type === 'identifier') {
                this.advance();
                return _var(tok.value);
            }

            // 括号
            if (tok.type === 'char' && tok.value === '(') {
                this.advance();
                var expr = this.parseAddSub();
                this.expect('char', ')');
                return expr;
            }

            if (tok.type === 'char' && tok.value === '{') {
                this.advance();
                var expr2 = this.parseAddSub();
                this.expect('char', '}');
                return expr2;
            }

            // 竖线作为绝对值
            if (tok.type === 'char' && tok.value === '|') {
                this.advance();
                var absExpr = this.parseAddSub();
                this.expect('char', '|');
                return _unOp('abs', absExpr);
            }

            // 如果是命令，递归回 parseUnary
            if (tok.type === 'command') {
                return this.parseUnary();
            }

            // 未知 token，跳过
            this.advance();
            return _num(0);
        };

        /**
         * 解析 LaTeX 字符串为 AST
         */
        function parse(input) {
            var tokens = tokenize(input);
            if (tokens.length === 0) {
                return null;
            }
            var parser = new Parser(tokens);
            var result = parser.parseAll();
            if (parser.error) {
                // 解析失败：先记录错误到内部状态和日志，再返回部分解析结果
                _setError('LaTeX parse error: ' + parser.error);
                console.warn('[FormulaParser] LaTeX 解析错误:', parser.error);
                return result; // 返回部分解析结果（best-effort）
            }
            return result;
        }

        return {
            parse: parse
        };
    })();

    // ========================================================================
    //  Python 数学语法解析器
    // ========================================================================

    var PythonParser = (function() {

        /**
         * Python 数学表达式词法分析器
         * 将 Python 数学表达式字符串分解为 token 数组。
         *
         * 说明：与 LatexParser/DSLParser 的 tokenize 结构相似，详见 LatexParser
         * tokenize 函数头部的注释。
         */
        function tokenize(input) {
            var tokens = [];
            var pos = 0;
            var len = input.length;

            while (pos < len) {
                var ch = input.charAt(pos);

                // 跳过空白
                if (/\s/.test(ch)) {
                    pos++;
                    continue;
                }

                // 数字（含浮点数）
                if (/[0-9]/.test(ch) || (ch === '.' && pos + 1 < len && /[0-9]/.test(input.charAt(pos + 1)))) {
                    var numStr = '';
                    var hasDot = false;
                    while (pos < len && (/[0-9]/.test(input.charAt(pos)) || (input.charAt(pos) === '.' && !hasDot))) {
                        if (input.charAt(pos) === '.') { hasDot = true; }
                        numStr += input.charAt(pos);
                        pos++;
                    }
                    tokens.push({ type: 'number', value: numStr });
                    continue;
                }

                // 标识符
                if (/[a-zA-Z_]/.test(ch)) {
                    var ident = '';
                    while (pos < len && /[a-zA-Z_0-9]/.test(input.charAt(pos))) {
                        ident += input.charAt(pos);
                        pos++;
                    }
                    tokens.push({ type: 'identifier', value: ident });
                    continue;
                }

                // 双字符运算符
                if (pos + 1 < len) {
                    var two = input.substring(pos, pos + 2);
                    if (two === '**' || two === '==' || two === '!=' || two === '<=' || two === '>=') {
                        tokens.push({ type: 'operator', value: two });
                        pos += 2;
                        continue;
                    }
                    if (two === '//') {
                        tokens.push({ type: 'operator', value: '//' });
                        pos += 2;
                        continue;
                    }
                }

                // 单字符运算符和标点
                if ('+-*/%^()[]{}=<>!,'.indexOf(ch) >= 0) {
                    tokens.push({ type: 'operator', value: ch });
                    pos++;
                    continue;
                }

                // 分号
                if (ch === ';') {
                    tokens.push({ type: 'operator', value: ';' });
                    pos++;
                    continue;
                }

                // 未知字符，跳过
                pos++;
            }

            return tokens;
        }

        /**
         * Python 递归下降解析器
         */
        function Parser(tokens) {
            this.tokens = tokens;
            this.pos = 0;
            this.error = null;
        }

        Parser.prototype.peek = function() {
            if (this.pos < this.tokens.length) {
                return this.tokens[this.pos];
            }
            return null;
        };

        Parser.prototype.advance = function() {
            var tok = this.tokens[this.pos];
            this.pos++;
            return tok;
        };

        Parser.prototype.expect = function(type, value) {
            var tok = this.peek();
            if (!tok) {
                this.error = 'Unexpected end of input, expected ' + type + ' "' + value + '"';
                return null;
            }
            if (tok.type !== type || (value !== undefined && tok.value !== value)) {
                this.error = 'Unexpected token "' + tok.value + '" at position ' + this.pos +
                    ', expected ' + type + ' "' + value + '"';
                return null;
            }
            return this.advance();
        };

        Parser.prototype.match = function(type, value) {
            var tok = this.peek();
            return tok && tok.type === type && (value === undefined || tok.value === value);
        };

        /**
         * 解析入口
         */
        Parser.prototype.parseAll = function() {
            var statements = [];
            var stmt = this.parseStatement();
            if (!stmt && this.error) {
                return null;
            }
            if (stmt) {
                statements.push(stmt);
            }
            while (this.match('operator', ';')) {
                this.advance();
                stmt = this.parseStatement();
                if (stmt) {
                    statements.push(stmt);
                }
            }
            if (statements.length === 1) {
                return statements[0];
            }
            return _compound(statements);
        };

        Parser.prototype.parseStatement = function() {
            return this.parseComparison();
        };

        Parser.prototype.parseComparison = function() {
            var left = this.parseAddSub();

            if (this.match('operator', '==')) {
                this.advance();
                var right = this.parseAddSub();
                return _eq(left, right);
            }

            if (this.match('operator', '!=')) {
                this.advance();
                var right2 = this.parseAddSub();
                return _ineq('!=', left, right2);
            }

            if (this.match('operator', '<=')) {
                this.advance();
                var right3 = this.parseAddSub();
                return _ineq('<=', left, right3);
            }

            if (this.match('operator', '>=')) {
                this.advance();
                var right4 = this.parseAddSub();
                return _ineq('>=', left, right4);
            }

            if (this.match('operator', '<')) {
                this.advance();
                var right5 = this.parseAddSub();
                return _ineq('<', left, right5);
            }

            if (this.match('operator', '>')) {
                this.advance();
                var right6 = this.parseAddSub();
                return _ineq('>', left, right6);
            }

            return left;
        };

        Parser.prototype.parseAddSub = function() {
            var left = this.parseMulDiv();

            while (true) {
                if (this.match('operator', '+')) {
                    this.advance();
                    var right = this.parseMulDiv();
                    left = _binOp('+', left, right);
                } else if (this.match('operator', '-')) {
                    this.advance();
                    var right2 = this.parseMulDiv();
                    left = _binOp('-', left, right2);
                } else {
                    break;
                }
            }

            return left;
        };

        Parser.prototype.parseMulDiv = function() {
            var left = this.parsePower();

            while (true) {
                if (this.match('operator', '*')) {
                    this.advance();
                    var right = this.parsePower();
                    left = _binOp('*', left, right);
                } else if (this.match('operator', '/')) {
                    this.advance();
                    var right2 = this.parsePower();
                    left = _binOp('/', left, right2);
                } else if (this.match('operator', '//')) {
                    this.advance();
                    var right3 = this.parsePower();
                    left = _func('floor_div', [left, right3]);
                } else if (this.match('operator', '%')) {
                    this.advance();
                    var right4 = this.parsePower();
                    left = _func('mod', [left, right4]);
                } else {
                    // 隐式乘法
                    var next = this.peek();
                    if (next && (next.type === 'identifier' || (next.type === 'operator' && next.value === '('))) {
                        if (left.type === 'number' || left.type === 'variable' ||
                            (left.type === 'binary_op' && left.operator === '^') ||
                            (left.type === 'unary_op') ||
                            (left.type === 'function_call')) {
                            var right5 = this.parsePower();
                            left = _binOp('*', left, right5);
                        } else {
                            break;
                        }
                    } else {
                        break;
                    }
                }
            }

            return left;
        };

        Parser.prototype.parsePower = function() {
            var base = this.parseUnary();

            if (this.match('operator', '^')) {
                this.advance();
                var exp = this.parsePower();
                return _binOp('^', base, exp);
            }

            if (this.match('operator', '**')) {
                this.advance();
                var exp2 = this.parsePower();
                return _binOp('^', base, exp2);
            }

            return base;
        };

        Parser.prototype.parseUnary = function() {
            if (this.match('operator', '-')) {
                this.advance();
                var operand = this.parseUnary();
                return _unOp('-', operand);
            }

            if (this.match('operator', '+')) {
                this.advance();
                return this.parseUnary();
            }

            return this.parseCall();
        };

        Parser.prototype.parseCall = function() {
            var tok = this.peek();

            if (tok && tok.type === 'identifier') {
                var name = tok.value;

                // 内置数学函数
                var mathFuncs = {
                    'sqrt': 'sqrt', 'sin': 'sin', 'cos': 'cos', 'tan': 'tan',
                    'log': 'log', 'log10': 'log10', 'log2': 'log2',
                    'exp': 'exp', 'abs': 'abs',
                    'asin': 'asin', 'acos': 'acos', 'atan': 'atan',
                    'sinh': 'sinh', 'cosh': 'cosh', 'tanh': 'tanh',
                    'ceil': 'ceil', 'floor': 'floor', 'round': 'round',
                    'max': 'max', 'min': 'min'
                };

                // 检查是否为函数调用（标识符后跟括号）
                if (this.tokens[this.pos + 1] && this.tokens[this.pos + 1].type === 'operator' && this.tokens[this.pos + 1].value === '(') {
                    this.advance(); // 消费标识符
                    this.advance(); // 消费 '('

                    var args = [];
                    if (!this.match('operator', ')')) {
                        args.push(this.parseAddSub());
                        while (this.match('operator', ',')) {
                            this.advance();
                            args.push(this.parseAddSub());
                        }
                    }
                    this.expect('operator', ')');

                    if (mathFuncs[name]) {
                        // 单参数数学函数 -> unary_op
                        if (args.length === 1) {
                            return _unOp(mathFuncs[name], args[0]);
                        }
                        return _func(mathFuncs[name], args);
                    }

                    return _func(name, args);
                }

                // 特殊常量
                if (name === 'pi') {
                    this.advance();
                    return _var('pi');
                }
                if (name === 'e' || name === 'E') {
                    this.advance();
                    return _var('e');
                }
                if (name === 'inf' || name === 'Infinity') {
                    this.advance();
                    return _num(Infinity);
                }
            }

            return this.parseAtom();
        };

        Parser.prototype.parseAtom = function() {
            var tok = this.peek();

            if (!tok) {
                this.error = 'Unexpected end of input';
                return _num(0);
            }

            // 数字
            if (tok.type === 'number') {
                this.advance();
                return _num(parseFloat(tok.value));
            }

            // 标识符
            if (tok.type === 'identifier') {
                this.advance();
                return _var(tok.value);
            }

            // 括号
            if (tok.type === 'operator' && tok.value === '(') {
                this.advance();
                var expr = this.parseAddSub();
                this.expect('operator', ')');
                return expr;
            }

            if (tok.type === 'operator' && tok.value === '[') {
                this.advance();
                var expr2 = this.parseAddSub();
                this.expect('operator', ']');
                return expr2;
            }

            // 未知 token
            this.advance();
            return _num(0);
        };

        /**
         * 解析 Python 数学表达式字符串为 AST
         */
        function parse(input) {
            var tokens = tokenize(input);
            if (tokens.length === 0) {
                return null;
            }
            var parser = new Parser(tokens);
            var result = parser.parseAll();
            if (parser.error) {
                // 解析失败：先记录错误到内部状态和日志，再返回部分解析结果
                _setError('Python parse error: ' + parser.error);
                console.warn('[FormulaParser] Python 解析错误:', parser.error);
                return result; // 返回部分解析结果（best-effort）
            }
            return result;
        }

        return {
            parse: parse
        };
    })();

    // ========================================================================
    //  Lv-00 DSL 解析器
    // ========================================================================

    var DSLParser = (function() {

        /**
         * DSL 词法分析器
         * 将 Lv-00 DSL 字符串分解为 token 数组。
         *
         * 说明：与 LatexParser/PythonParser 的 tokenize 结构相似，详见 LatexParser
         * tokenize 函数头部的注释。
         */
        function tokenize(input) {
            var tokens = [];
            var pos = 0;
            var len = input.length;

            while (pos < len) {
                var ch = input.charAt(pos);

                // 跳过空白
                if (/\s/.test(ch)) {
                    pos++;
                    continue;
                }

                // 数字
                if (/[0-9]/.test(ch) || (ch === '.' && pos + 1 < len && /[0-9]/.test(input.charAt(pos + 1)))) {
                    var numStr = '';
                    var hasDot = false;
                    while (pos < len && (/[0-9]/.test(input.charAt(pos)) || (input.charAt(pos) === '.' && !hasDot))) {
                        if (input.charAt(pos) === '.') { hasDot = true; }
                        numStr += input.charAt(pos);
                        pos++;
                    }
                    tokens.push({ type: 'number', value: numStr });
                    continue;
                }

                // 标识符
                if (/[a-zA-Z_]/.test(ch)) {
                    var ident = '';
                    while (pos < len && /[a-zA-Z_0-9]/.test(input.charAt(pos))) {
                        ident += input.charAt(pos);
                        pos++;
                    }
                    tokens.push({ type: 'identifier', value: ident });
                    continue;
                }

                // 双字符运算符
                if (pos + 1 < len) {
                    var two = input.substring(pos, pos + 2);
                    if (two === '**' || two === '==' || two === '!=' || two === '<=' || two === '>=') {
                        tokens.push({ type: 'operator', value: two });
                        pos += 2;
                        continue;
                    }
                }

                // 单字符
                if ('+-*/^()[]{}=<>!,'.indexOf(ch) >= 0) {
                    tokens.push({ type: 'operator', value: ch });
                    pos++;
                    continue;
                }

                // 分号
                if (ch === ';') {
                    tokens.push({ type: 'operator', value: ';' });
                    pos++;
                    continue;
                }

                // 换行作为语句分隔
                if (ch === '\n' || ch === '\r') {
                    tokens.push({ type: 'newline', value: ch });
                    pos++;
                    continue;
                }

                pos++;
            }

            return tokens;
        }

        /**
         * DSL 递归下降解析器
         */
        function Parser(tokens) {
            this.tokens = tokens;
            this.pos = 0;
            this.error = null;
        }

        Parser.prototype.peek = function() {
            if (this.pos < this.tokens.length) {
                return this.tokens[this.pos];
            }
            return null;
        };

        Parser.prototype.advance = function() {
            var tok = this.tokens[this.pos];
            this.pos++;
            return tok;
        };

        Parser.prototype.expect = function(type, value) {
            var tok = this.peek();
            if (!tok) {
                this.error = 'Unexpected end of input, expected ' + type + ' "' + value + '"';
                return null;
            }
            if (tok.type !== type || (value !== undefined && tok.value !== value)) {
                this.error = 'Unexpected token "' + tok.value + '" at position ' + this.pos +
                    ', expected ' + type + ' "' + value + '"';
                return null;
            }
            return this.advance();
        };

        Parser.prototype.match = function(type, value) {
            var tok = this.peek();
            return tok && tok.type === type && (value === undefined || tok.value === value);
        };

        Parser.prototype.peekIdent = function() {
            var tok = this.peek();
            return tok && tok.type === 'identifier' ? tok.value : null;
        };

        /**
         * 解析入口
         */
        Parser.prototype.parseAll = function() {
            var statements = [];
            var stmt = this.parseStatement();
            if (!stmt && this.error) {
                return null;
            }
            if (stmt) {
                statements.push(stmt);
            }
            while (this.match('operator', ';') || this.match('newline')) {
                this.advance();
                if (this.peek() === null) break;
                stmt = this.parseStatement();
                if (stmt) {
                    statements.push(stmt);
                }
            }
            if (statements.length === 1) {
                return statements[0];
            }
            if (statements.length === 0) {
                return null;
            }
            return _compound(statements);
        };

        /**
         * 解析单条语句
         */
        Parser.prototype.parseStatement = function() {
            var ident = this.peekIdent();

            if (!ident) {
                return this.parseAlgebraic();
            }

            var lower = ident.toLowerCase();

            // 几何定义语句
            if (lower === 'point') {
                return this.parsePointDef();
            }
            if (lower === 'segment') {
                return this.parseSegmentDef();
            }
            if (lower === 'circle') {
                return this.parseCircleDef();
            }
            if (lower === 'triangle') {
                return this.parseTriangleDef();
            }
            if (lower === 'line') {
                return this.parseLineDef();
            }
            if (lower === 'region') {
                return this.parseRegionDef();
            }

            // 几何约束函数
            if (lower === 'perpendicular' || lower === 'parallel' || lower === 'midpoint' ||
                lower === 'angle' || lower === 'distance' || lower === 'area' || lower === 'perimeter' ||
                lower === 'incidence' || lower === 'betweenness' || lower === 'collinear' ||
                lower === 'tangent' || lower === 'congruent' || lower === 'similar') {
                return this.parseConstraintCall();
            }

            // 代数表达式（可能包含 =）
            return this.parseAlgebraic();
        };

        /**
         * 解析 point 定义
         * point A(0, 0) 或 point A(0,0)
         */
        Parser.prototype.parsePointDef = function() {
            this.advance(); // 'point'
            var nameTok = this.expect('identifier');
            if (!nameTok) return null;
            var name = nameTok.value;

            this.expect('operator', '(');
            var x = this.parseAddSub();
            this.expect('operator', ',');
            var y = this.parseAddSub();
            this.expect('operator', ')');

            return _geoPoint(name, x, y);
        };

        /**
         * 解析 segment 定义
         * segment AB 或 segment Name(A, B)
         */
        Parser.prototype.parseSegmentDef = function() {
            this.advance(); // 'segment'
            var nameTok = this.expect('identifier');
            if (!nameTok) return null;
            var name = nameTok.value;

            if (this.match('operator', '(')) {
                // segment Name(A, B)
                this.advance();
                var p1 = this.expect('identifier');
                this.expect('operator', ',');
                var p2 = this.expect('identifier');
                this.expect('operator', ')');
                return _geoSegment(name, p1 ? p1.value : '', p2 ? p2.value : '');
            } else {
                // segment AB - 名称是整个标识符，两个端点是前两个和后两个字符
                if (name.length >= 2) {
                    return _geoSegment(name, name.charAt(0), name.charAt(1));
                }
                this.error = 'Segment name must have at least 2 characters';
                return _geoSegment(name, '', '');
            }
        };

        /**
         * 解析 circle 定义
         * circle O(A, 3) 或 circle O(A, r)
         */
        Parser.prototype.parseCircleDef = function() {
            this.advance(); // 'circle'
            var nameTok = this.expect('identifier');
            if (!nameTok) return null;
            var name = nameTok.value;

            this.expect('operator', '(');
            var centerTok = this.expect('identifier');
            this.expect('operator', ',');
            var radius = this.parseAddSub();
            this.expect('operator', ')');

            return _geoCircle(name, centerTok ? centerTok.value : '', radius);
        };

        /**
         * 解析 triangle 定义
         * triangle ABC 或 triangle T(A, B, C)
         */
        Parser.prototype.parseTriangleDef = function() {
            this.advance(); // 'triangle'
            var nameTok = this.expect('identifier');
            if (!nameTok) return null;
            var name = nameTok.value;

            if (this.match('operator', '(')) {
                // triangle T(A, B, C)
                this.advance();
                var p1 = this.expect('identifier');
                this.expect('operator', ',');
                var p2 = this.expect('identifier');
                this.expect('operator', ',');
                var p3 = this.expect('identifier');
                this.expect('operator', ')');
                return _geoTriangle(name, p1 ? p1.value : '', p2 ? p2.value : '', p3 ? p3.value : '');
            } else {
                // triangle ABC
                if (name.length >= 3) {
                    return _geoTriangle(name, name.charAt(0), name.charAt(1), name.charAt(2));
                }
                this.error = 'Triangle name must have at least 3 characters';
                return _geoTriangle(name, '', '', '');
            }
        };

        /**
         * 解析 line 定义
         * line l(A, B) 或 line AB
         */
        Parser.prototype.parseLineDef = function() {
            this.advance(); // 'line'
            var nameTok = this.expect('identifier');
            if (!nameTok) return null;
            var name = nameTok.value;

            if (this.match('operator', '(')) {
                this.advance();
                var p1 = this.expect('identifier');
                this.expect('operator', ',');
                var p2 = this.expect('identifier');
                this.expect('operator', ')');
                return _geoLine(name, p1 ? p1.value : '', p2 ? p2.value : '');
            } else {
                if (name.length >= 2) {
                    return _geoLine(name, name.charAt(0), name.charAt(1));
                }
                this.error = 'Line name must have at least 2 characters';
                return _geoLine(name, '', '');
            }
        };

        /**
         * 解析 region 定义
         * region R(triangle ABC) 或 region R(circle O)
         */
        Parser.prototype.parseRegionDef = function() {
            this.advance(); // 'region'
            var nameTok = this.expect('identifier');
            if (!nameTok) return null;
            var name = nameTok.value;

            this.expect('operator', '(');
            var shape = this.parseStatement();
            this.expect('operator', ')');

            return _geoRegion(name, shape);
        };

        /**
         * 解析几何约束函数调用
         * perpendicular(A, B, C), parallel(l1, l2), midpoint(A, B),
         * angle(A, B, C), distance(A, B), area(Name), perimeter(Name)
         */
        Parser.prototype.parseConstraintCall = function() {
            var kindTok = this.advance();
            var kind = kindTok.value.toLowerCase();

            this.expect('operator', '(');
            var args = [];

            if (!this.match('operator', ')')) {
                // 第一个参数可能是标识符或表达式
                if (this.peekIdent()) {
                    args.push(this.advance().value);
                } else {
                    args.push(this.parseAddSub());
                }

                while (this.match('operator', ',')) {
                    this.advance();
                    if (this.peekIdent()) {
                        args.push(this.advance().value);
                    } else {
                        args.push(this.parseAddSub());
                    }
                }
            }

            this.expect('operator', ')');

            return _geoConstraint(kind, args);
        };

        /**
         * 解析代数表达式（含方程和不等式）
         */
        Parser.prototype.parseAlgebraic = function() {
            return this.parseComparison();
        };

        Parser.prototype.parseComparison = function() {
            var left = this.parseAddSub();

            if (this.match('operator', '==') || this.match('operator', '=')) {
                this.advance();
                var right = this.parseAddSub();
                return _eq(left, right);
            }

            if (this.match('operator', '!=')) {
                this.advance();
                var right2 = this.parseAddSub();
                return _ineq('!=', left, right2);
            }

            if (this.match('operator', '<=')) {
                this.advance();
                var right3 = this.parseAddSub();
                return _ineq('<=', left, right3);
            }

            if (this.match('operator', '>=')) {
                this.advance();
                var right4 = this.parseAddSub();
                return _ineq('>=', left, right4);
            }

            if (this.match('operator', '<')) {
                this.advance();
                var right5 = this.parseAddSub();
                return _ineq('<', left, right5);
            }

            if (this.match('operator', '>')) {
                this.advance();
                var right6 = this.parseAddSub();
                return _ineq('>', left, right6);
            }

            return left;
        };

        Parser.prototype.parseAddSub = function() {
            var left = this.parseMulDiv();

            while (true) {
                if (this.match('operator', '+')) {
                    this.advance();
                    var right = this.parseMulDiv();
                    left = _binOp('+', left, right);
                } else if (this.match('operator', '-')) {
                    this.advance();
                    var right2 = this.parseMulDiv();
                    left = _binOp('-', left, right2);
                } else {
                    break;
                }
            }

            return left;
        };

        Parser.prototype.parseMulDiv = function() {
            var left = this.parsePower();

            while (true) {
                if (this.match('operator', '*')) {
                    this.advance();
                    var right = this.parsePower();
                    left = _binOp('*', left, right);
                } else if (this.match('operator', '/')) {
                    this.advance();
                    var right2 = this.parsePower();
                    left = _binOp('/', left, right2);
                } else {
                    // 隐式乘法
                    var next = this.peek();
                    if (next && (next.type === 'identifier' || (next.type === 'operator' && next.value === '('))) {
                        if (left.type === 'number' || left.type === 'variable' ||
                            (left.type === 'binary_op' && left.operator === '^') ||
                            (left.type === 'unary_op') ||
                            (left.type === 'function_call')) {
                            var right3 = this.parsePower();
                            left = _binOp('*', left, right3);
                        } else {
                            break;
                        }
                    } else {
                        break;
                    }
                }
            }

            return left;
        };

        Parser.prototype.parsePower = function() {
            var base = this.parseUnary();

            if (this.match('operator', '^')) {
                this.advance();
                var exp = this.parsePower();
                return _binOp('^', base, exp);
            }

            if (this.match('operator', '**')) {
                this.advance();
                var exp2 = this.parsePower();
                return _binOp('^', base, exp2);
            }

            return base;
        };

        Parser.prototype.parseUnary = function() {
            if (this.match('operator', '-')) {
                this.advance();
                var operand = this.parseUnary();
                return _unOp('-', operand);
            }

            if (this.match('operator', '+')) {
                this.advance();
                return this.parseUnary();
            }

            return this.parseAtom();
        };

        Parser.prototype.parseAtom = function() {
            var tok = this.peek();

            if (!tok) {
                this.error = 'Unexpected end of input';
                return _num(0);
            }

            // 数字
            if (tok.type === 'number') {
                this.advance();
                return _num(parseFloat(tok.value));
            }

            // 标识符
            if (tok.type === 'identifier') {
                this.advance();
                // 检查是否为函数调用
                if (this.match('operator', '(')) {
                    this.advance();
                    var args = [];
                    if (!this.match('operator', ')')) {
                        args.push(this.parseAddSub());
                        while (this.match('operator', ',')) {
                            this.advance();
                            args.push(this.parseAddSub());
                        }
                    }
                    this.expect('operator', ')');
                    return _func(tok.value, args);
                }
                return _var(tok.value);
            }

            // 括号
            if (tok.type === 'operator' && tok.value === '(') {
                this.advance();
                var expr = this.parseAddSub();
                this.expect('operator', ')');
                return expr;
            }

            // 未知
            this.advance();
            return _num(0);
        };

        /**
         * 解析 DSL 字符串为 AST
         */
        function parse(input) {
            var tokens = tokenize(input);
            if (tokens.length === 0) {
                return null;
            }
            var parser = new Parser(tokens);
            var result = parser.parseAll();
            if (parser.error) {
                // 解析失败：先记录错误到内部状态和日志，再返回部分解析结果
                _setError('DSL parse error: ' + parser.error);
                console.warn('[FormulaParser] DSL 解析错误:', parser.error);
                return result; // 返回部分解析结果（best-effort）
            }
            return result;
        }

        return {
            parse: parse
        };
    })();

    // ========================================================================
    //  AST 到 LaTeX 转换
    // ========================================================================

    /**
     * 将统一 AST 转换为 LaTeX 数学字符串
     *
     * 说明：astToLatex / astToPython / astToDSL 三个函数结构高度相似——都是对 AST
     * 节点类型的 switch-case 分发，逐一处理 number、variable、binary_op 等节点，
     * 仅输出格式不同（如除法：LaTeX->\frac, Python->/, DSL->/）。
     * 这种重复并非设计缺陷，而是刻意为之：三种目标语言的运算符、函数名、括号
     * 风格差异较大，强行抽象为单一调度表反而降低可读性。将来如果需要支持第四种
     * 输出语言，可以考虑提取一个 visit(ast, visitorObject) 访问器模式。
     *
     * @param {object} ast - AST 根节点
     * @returns {string} LaTeX 字符串
     */
    function astToLatex(ast) {
        if (!ast) {
            return '';
        }

        switch (ast.type) {
            case 'number':
                if (ast.value === Infinity) {
                    return '\\infty';
                }
                if (ast.value === -Infinity) {
                    return '-\\infty';
                }
                return String(ast.value);

            case 'variable':
                var varMap = {
                    'pi': '\\pi', 'theta': '\\theta', 'alpha': '\\alpha',
                    'beta': '\\beta', 'gamma': '\\gamma', 'delta': '\\delta',
                    'epsilon': '\\epsilon', 'lambda': '\\lambda', 'mu': '\\mu',
                    'sigma': '\\sigma', 'omega': '\\omega', 'phi': '\\phi',
                    'psi': '\\psi', 'rho': '\\rho', 'tau': '\\tau',
                    'eta': '\\eta', 'zeta': '\\zeta', 'iota': '\\iota',
                    'kappa': '\\kappa', 'nu': '\\nu', 'xi': '\\xi',
                    'chi': '\\chi', 'upsilon': '\\upsilon'
                };
                if (varMap[ast.name]) {
                    return varMap[ast.name];
                }
                return ast.name;

            case 'binary_op':
                var left = astToLatex(ast.left);
                var right = astToLatex(ast.right);

                switch (ast.operator) {
                    case '+':
                        return left + ' + ' + right;
                    case '-':
                        return left + ' - ' + right;
                    case '*':
                        return left + ' \\cdot ' + right;
                    case '/':
                        return '\\frac{' + left + '}{' + right + '}';
                    case '^':
                        return left + '^{' + right + '}';
                    default:
                        return left + ' ' + ast.operator + ' ' + right;
                }

            case 'unary_op':
                var operand = astToLatex(ast.operand);
                switch (ast.operator) {
                    case '-':
                        return '-' + operand;
                    case 'sqrt':
                        return '\\sqrt{' + operand + '}';
                    case 'sin':
                        return '\\sin(' + operand + ')';
                    case 'cos':
                        return '\\cos(' + operand + ')';
                    case 'tan':
                        return '\\tan(' + operand + ')';
                    case 'log':
                        return '\\log(' + operand + ')';
                    case 'exp':
                        return '\\exp(' + operand + ')';
                    case 'abs':
                        return '|' + operand + '|';
                    case 'asin':
                        return '\\arcsin(' + operand + ')';
                    case 'acos':
                        return '\\arccos(' + operand + ')';
                    case 'atan':
                        return '\\arctan(' + operand + ')';
                    default:
                        return ast.operator + '(' + operand + ')';
                }

            case 'function_call':
                var funcArgs = [];
                for (var i = 0; i < ast.arguments.length; i++) {
                    funcArgs.push(astToLatex(ast.arguments[i]));
                }
                return '\\mathrm{' + ast.name + '}(' + funcArgs.join(', ') + ')';

            case 'equation':
                return astToLatex(ast.left) + ' = ' + astToLatex(ast.right);

            case 'inequality':
                var ineqMap = {
                    '<': '<', '>': '>', '<=': '\\leq', '>=': '\\geq', '!=': '\\neq'
                };
                var ineqOp = ineqMap[ast.operator] || ast.operator;
                return astToLatex(ast.left) + ' ' + ineqOp + ' ' + astToLatex(ast.right);

            case 'geometric_point':
                return '\\text{point } ' + ast.name + '(' + astToLatex(ast.x) + ', ' + astToLatex(ast.y) + ')';

            case 'geometric_segment':
                return '\\text{segment } ' + ast.name;

            case 'geometric_circle':
                return '\\text{circle } ' + ast.name + '(' + ast.center + ', ' + astToLatex(ast.radius) + ')';

            case 'geometric_triangle':
                return '\\text{triangle } ' + ast.name;

            case 'geometric_line':
                return '\\text{line } ' + ast.name;

            case 'geometric_region':
                return '\\text{region } ' + ast.name + '(' + astToLatex(ast.shape) + ')';

            case 'geometric_constraint':
                var cArgs = [];
                for (var j = 0; j < ast.arguments.length; j++) {
                    var a = ast.arguments[j];
                    cArgs.push(typeof a === 'string' ? a : astToLatex(a));
                }
                return '\\text{' + ast.kind + '}(' + cArgs.join(', ') + ')';

            case 'geometric_expression':
                return astToLatex(ast.expression);

            case 'compound':
                var stmts = [];
                for (var k = 0; k < ast.statements.length; k++) {
                    stmts.push(astToLatex(ast.statements[k]));
                }
                return stmts.join('; ');

            default:
                return JSON.stringify(ast);
        }
    }

    // ========================================================================
    //  AST 到 Python 数学字符串转换
    // ========================================================================

    /**
     * 将统一 AST 转换为 Python 数学表达式字符串。
     * 与 astToLatex / astToDSL 结构相似，详见 astToLatex 函数头部的注释。
     *
     * @param {object} ast - AST 根节点
     * @returns {string} Python 表达式字符串
     */
    function astToPython(ast) {
        if (!ast) {
            return '';
        }

        switch (ast.type) {
            case 'number':
                if (ast.value === Infinity) {
                    return 'float("inf")';
                }
                if (ast.value === -Infinity) {
                    return 'float("-inf")';
                }
                return String(ast.value);

            case 'variable':
                return ast.name;

            case 'binary_op':
                var left = astToPython(ast.left);
                var right = astToPython(ast.right);

                switch (ast.operator) {
                    case '+':
                        return '(' + left + ' + ' + right + ')';
                    case '-':
                        return '(' + left + ' - ' + right + ')';
                    case '*':
                        return '(' + left + ' * ' + right + ')';
                    case '/':
                        return '(' + left + ' / ' + right + ')';
                    case '^':
                        return '(' + left + ' ** ' + right + ')';
                    default:
                        return '(' + left + ' ' + ast.operator + ' ' + right + ')';
                }

            case 'unary_op':
                var operand = astToPython(ast.operand);
                switch (ast.operator) {
                    case '-':
                        return '(-' + operand + ')';
                    case 'sqrt':
                        return 'sqrt(' + operand + ')';
                    case 'sin':
                        return 'sin(' + operand + ')';
                    case 'cos':
                        return 'cos(' + operand + ')';
                    case 'tan':
                        return 'tan(' + operand + ')';
                    case 'log':
                        return 'log(' + operand + ')';
                    case 'exp':
                        return 'exp(' + operand + ')';
                    case 'abs':
                        return 'abs(' + operand + ')';
                    case 'asin':
                        return 'asin(' + operand + ')';
                    case 'acos':
                        return 'acos(' + operand + ')';
                    case 'atan':
                        return 'atan(' + operand + ')';
                    default:
                        return ast.operator + '(' + operand + ')';
                }

            case 'function_call':
                var funcArgs = [];
                for (var i = 0; i < ast.arguments.length; i++) {
                    funcArgs.push(astToPython(ast.arguments[i]));
                }
                return ast.name + '(' + funcArgs.join(', ') + ')';

            case 'equation':
                return astToPython(ast.left) + ' == ' + astToPython(ast.right);

            case 'inequality':
                return astToPython(ast.left) + ' ' + ast.operator + ' ' + astToPython(ast.right);

            case 'geometric_point':
                return 'point_' + ast.name + ' = (' + astToPython(ast.x) + ', ' + astToPython(ast.y) + ')';

            case 'geometric_segment':
                return 'segment_' + ast.name;

            case 'geometric_circle':
                return 'circle_' + ast.name + ' = (' + ast.center + ', ' + astToPython(ast.radius) + ')';

            case 'geometric_triangle':
                return 'triangle_' + ast.name;

            case 'geometric_line':
                return 'line_' + ast.name;

            case 'geometric_region':
                return 'region_' + ast.name;

            case 'geometric_constraint':
                var cArgs = [];
                for (var j = 0; j < ast.arguments.length; j++) {
                    var a = ast.arguments[j];
                    cArgs.push(typeof a === 'string' ? a : astToPython(a));
                }
                return ast.kind + '(' + cArgs.join(', ') + ')';

            case 'geometric_expression':
                return astToPython(ast.expression);

            case 'compound':
                var stmts = [];
                for (var k = 0; k < ast.statements.length; k++) {
                    stmts.push(astToPython(ast.statements[k]));
                }
                return stmts.join('; ');

            default:
                return JSON.stringify(ast);
        }
    }

    // ========================================================================
    //  AST 到 Lv-00 DSL 字符串转换
    // ========================================================================

    /**
     * 将统一 AST 转换为 Lv-00 DSL 字符串。
     * 与 astToLatex / astToPython 结构相似，详见 astToLatex 函数头部的注释。
     *
     * @param {object} ast - AST 根节点
     * @returns {string} DSL 字符串
     */
    function astToDSL(ast) {
        if (!ast) {
            return '';
        }

        switch (ast.type) {
            case 'number':
                if (ast.value === Infinity) {
                    return 'inf';
                }
                if (ast.value === -Infinity) {
                    return '-inf';
                }
                return String(ast.value);

            case 'variable':
                return ast.name;

            case 'binary_op':
                var left = astToDSL(ast.left);
                var right = astToDSL(ast.right);

                switch (ast.operator) {
                    case '+':
                        return left + ' + ' + right;
                    case '-':
                        return left + ' - ' + right;
                    case '*':
                        return left + ' * ' + right;
                    case '/':
                        return left + ' / ' + right;
                    case '^':
                        return left + '^' + right;
                    default:
                        return left + ' ' + ast.operator + ' ' + right;
                }

            case 'unary_op':
                var operand = astToDSL(ast.operand);
                switch (ast.operator) {
                    case '-':
                        return '-' + operand;
                    case 'sqrt':
                        return 'sqrt(' + operand + ')';
                    case 'sin':
                        return 'sin(' + operand + ')';
                    case 'cos':
                        return 'cos(' + operand + ')';
                    case 'tan':
                        return 'tan(' + operand + ')';
                    case 'log':
                        return 'log(' + operand + ')';
                    case 'exp':
                        return 'exp(' + operand + ')';
                    case 'abs':
                        return 'abs(' + operand + ')';
                    default:
                        return ast.operator + '(' + operand + ')';
                }

            case 'function_call':
                var funcArgs = [];
                for (var i = 0; i < ast.arguments.length; i++) {
                    funcArgs.push(astToDSL(ast.arguments[i]));
                }
                return ast.name + '(' + funcArgs.join(', ') + ')';

            case 'equation':
                return astToDSL(ast.left) + ' = ' + astToDSL(ast.right);

            case 'inequality':
                return astToDSL(ast.left) + ' ' + ast.operator + ' ' + astToDSL(ast.right);

            case 'geometric_point':
                return 'point ' + ast.name + '(' + astToDSL(ast.x) + ', ' + astToDSL(ast.y) + ')';

            case 'geometric_segment':
                return 'segment ' + ast.name;

            case 'geometric_circle':
                return 'circle ' + ast.name + '(' + ast.center + ', ' + astToDSL(ast.radius) + ')';

            case 'geometric_triangle':
                return 'triangle ' + ast.name;

            case 'geometric_line':
                return 'line ' + ast.name;

            case 'geometric_region':
                return 'region ' + ast.name + '(' + astToDSL(ast.shape) + ')';

            case 'geometric_constraint':
                var cArgs = [];
                for (var j = 0; j < ast.arguments.length; j++) {
                    var a = ast.arguments[j];
                    cArgs.push(typeof a === 'string' ? a : astToDSL(a));
                }
                return ast.kind + '(' + cArgs.join(', ') + ')';

            case 'geometric_expression':
                return astToDSL(ast.expression);

            case 'compound':
                var stmts = [];
                for (var k = 0; k < ast.statements.length; k++) {
                    stmts.push(astToDSL(ast.statements[k]));
                }
                return stmts.join('; ');

            default:
                return JSON.stringify(ast);
        }
    }

    // ========================================================================
    //  AST 验证
    // ========================================================================

    function validate(ast) {
        var errors = [];

        if (!ast) {
            return { valid: false, errors: ['AST is null'] };
        }

        if (!ast.type) {
            errors.push('AST node missing "type" field');
            return { valid: false, errors: errors };
        }

        var validTypes = [
            'number', 'variable', 'binary_op', 'unary_op', 'function_call',
            'equation', 'inequality', 'geometric_point', 'geometric_segment',
            'geometric_circle', 'geometric_triangle', 'geometric_line',
            'geometric_region', 'geometric_constraint', 'geometric_expression',
            'compound'
        ];

        var typeFound = false;
        for (var t = 0; t < validTypes.length; t++) {
            if (ast.type === validTypes[t]) {
                typeFound = true;
                break;
            }
        }
        if (!typeFound) {
            errors.push('Unknown AST node type: "' + ast.type + '"');
        }

        // 类型特定验证
        switch (ast.type) {
            case 'number':
                if (typeof ast.value !== 'number') {
                    errors.push('number node requires numeric "value"');
                }
                break;

            case 'variable':
                if (typeof ast.name !== 'string' || ast.name.length === 0) {
                    errors.push('variable node requires non-empty string "name"');
                }
                break;

            case 'binary_op':
                if (!ast.operator) {
                    errors.push('binary_op node requires "operator"');
                }
                if (!ast.left) {
                    errors.push('binary_op node requires "left" operand');
                }
                if (!ast.right) {
                    errors.push('binary_op node requires "right" operand');
                }
                // 递归验证子节点
                if (ast.left) {
                    var leftResult = validate(ast.left);
                    errors = errors.concat(leftResult.errors);
                }
                if (ast.right) {
                    var rightResult = validate(ast.right);
                    errors = errors.concat(rightResult.errors);
                }
                break;

            case 'unary_op':
                if (!ast.operator) {
                    errors.push('unary_op node requires "operator"');
                }
                if (!ast.operand) {
                    errors.push('unary_op node requires "operand"');
                }
                if (ast.operand) {
                    var opResult = validate(ast.operand);
                    errors = errors.concat(opResult.errors);
                }
                break;

            case 'function_call':
                if (!ast.name) {
                    errors.push('function_call node requires "name"');
                }
                if (!ast.arguments || !Array.isArray(ast.arguments)) {
                    errors.push('function_call node requires "arguments" array');
                }
                break;

            case 'equation':
                if (!ast.left) { errors.push('equation node requires "left"'); }
                if (!ast.right) { errors.push('equation node requires "right"'); }
                if (ast.left) { errors = errors.concat(validate(ast.left).errors); }
                if (ast.right) { errors = errors.concat(validate(ast.right).errors); }
                break;

            case 'inequality':
                if (!ast.operator) { errors.push('inequality node requires "operator"'); }
                if (!ast.left) { errors.push('inequality node requires "left"'); }
                if (!ast.right) { errors.push('inequality node requires "right"'); }
                if (ast.left) { errors = errors.concat(validate(ast.left).errors); }
                if (ast.right) { errors = errors.concat(validate(ast.right).errors); }
                break;

            case 'geometric_point':
                if (!ast.name) { errors.push('geometric_point node requires "name"'); }
                if (ast.x === undefined) { errors.push('geometric_point node requires "x"'); }
                if (ast.y === undefined) { errors.push('geometric_point node requires "y"'); }
                break;

            case 'geometric_segment':
                if (!ast.name) { errors.push('geometric_segment node requires "name"'); }
                if (!ast.point1) { errors.push('geometric_segment node requires "point1"'); }
                if (!ast.point2) { errors.push('geometric_segment node requires "point2"'); }
                break;

            case 'geometric_circle':
                if (!ast.name) { errors.push('geometric_circle node requires "name"'); }
                if (!ast.center) { errors.push('geometric_circle node requires "center"'); }
                if (ast.radius === undefined) { errors.push('geometric_circle node requires "radius"'); }
                break;

            case 'geometric_triangle':
                if (!ast.name) { errors.push('geometric_triangle node requires "name"'); }
                if (!ast.point1) { errors.push('geometric_triangle node requires "point1"'); }
                if (!ast.point2) { errors.push('geometric_triangle node requires "point2"'); }
                if (!ast.point3) { errors.push('geometric_triangle node requires "point3"'); }
                break;

            case 'geometric_line':
                if (!ast.name) { errors.push('geometric_line node requires "name"'); }
                if (!ast.point1) { errors.push('geometric_line node requires "point1"'); }
                if (!ast.point2) { errors.push('geometric_line node requires "point2"'); }
                break;

            case 'geometric_region':
                if (!ast.name) { errors.push('geometric_region node requires "name"'); }
                if (!ast.shape) { errors.push('geometric_region node requires "shape"'); }
                break;

            case 'geometric_constraint':
                if (!ast.kind) { errors.push('geometric_constraint node requires "kind"'); }
                if (!ast.arguments || !Array.isArray(ast.arguments)) {
                    errors.push('geometric_constraint node requires "arguments" array');
                }
                break;

            case 'geometric_expression':
                if (!ast.expression) { errors.push('geometric_expression node requires "expression"'); }
                break;

            case 'compound':
                if (!ast.statements || !Array.isArray(ast.statements)) {
                    errors.push('compound node requires "statements" array');
                }
                if (ast.statements && Array.isArray(ast.statements)) {
                    for (var s = 0; s < ast.statements.length; s++) {
                        errors = errors.concat(validate(ast.statements[s]).errors);
                    }
                }
                break;
        }

        return {
            valid: errors.length === 0,
            errors: errors
        };
    }

    // ========================================================================
    //  主解析入口
    // ========================================================================

    /**
     * 解析公式字符串为 AST（主入口函数）
     *
     * 支持三种语法模式：
     *   - 'latex':  LaTeX 数学公式语法
     *   - 'python': Python 数学表达式语法
     *   - 'dsl':    Lv-00 几何领域特定语言
     *   - 'auto':   自动检测（通过 detectSyntax 判别）
     *
     * 解析流程：
     *   1. 输入验证（非空字符串检查）
     *   2. 语法类型确定（auto 时自动检测，或使用 syntaxHint 提示）
     *   3. 委托给对应语法解析器（LatexParser / PythonMathParser / DslParser）
     *   4. 解析失败时设置 _lastError 并返回 null
     *
     * @param {string} input - 输入公式字符串
     * @param {string} [syntaxHint] - 可选语法提示：'latex'|'python'|'dsl'|'auto'
     * @returns {object|null} AST 根节点，解析失败返回 null
     */
    function parse(input, syntaxHint) {
        _lastError = null;

        if (!input || typeof input !== 'string') {
            _setError('Input must be a non-empty string');
            return null;
        }

        var trimmed = input.replace(/^\s+|\s+$/g, '');
        if (trimmed.length === 0) {
            _setError('Input is empty');
            return null;
        }

        var syntax = syntaxHint || 'auto';
        if (syntax === 'auto') {
            syntax = detectSyntax(trimmed);
        }

        var result = null;

        try {
            switch (syntax) {
                case 'latex':
                    result = LatexParser.parse(trimmed);
                    break;
                case 'python':
                    result = PythonParser.parse(trimmed);
                    break;
                case 'dsl':
                    result = DSLParser.parse(trimmed);
                    break;
                case 'mixed':
                    // 混合语法：尝试 DSL 优先，然后 LaTeX，最后 Python
                    // 不再清空 _lastError，改为累积所有解析器的错误信息
                    var mixedErrors = [];
                    result = DSLParser.parse(trimmed);
                    if (!result) {
                        if (_lastError) { mixedErrors.push('DSL: ' + _lastError); }
                        result = LatexParser.parse(trimmed);
                    }
                    if (!result) {
                        if (_lastError) { mixedErrors.push('LaTeX: ' + _lastError); }
                        result = PythonParser.parse(trimmed);
                    }
                    if (!result && mixedErrors.length > 0) {
                        _setError('Mixed parse failed: [' + mixedErrors.join('; ') + ']');
                    }
                    break;
                default:
                    _setError('Unknown syntax type: "' + syntax + '"');
                    return null;
            }
        } catch (e) {
            _setError('解析异常 / Parse exception: ' + (e.message || '未知错误'));
            console.error('[FormulaParser] 解析异常:', e);
            return null;
        }

        return result;
    }

    // ========================================================================
    //  公开 API
    // ========================================================================

    return {
        detectSyntax: detectSyntax,
        parse: parse,
        astToLatex: astToLatex,
        astToPython: astToPython,
        astToDSL: astToDSL,
        getLastError: function() {
            return _lastError;
        },
        validate: validate
    };

})();
