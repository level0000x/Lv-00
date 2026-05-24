/**
 * @file lv00_js_backend.js
 * @brief Lv-00 纯 JavaScript 后端 (Pure JavaScript Backend)
 * @description 当 WebAssembly 不可用时的回退后端实现，完整实现 Lv-00 系统的全部核心模块。
 *              包含符号坐标系统、信任色谱、表达式系统、函数块、约束图、
 *              类型系统、统一化引擎、求解引擎、证明系统、递归系统、调试系统、
 *              模块系统、互操作层和流式输出系统共 14 个核心模块。
 *              无外部依赖，独立运行。
 *
 * @module lv00_js_backend
 * @version 3.3.0
 * @author Lv-00 Team
 * @since 2026-05-20
 * @requires 严格 ES5 语法（无 class, const, let, arrow functions, template literals,
 *            destructuring, default params, spread）
 */

var Lv00JSBackend = (function() {
    'use strict';

    // ================================================================
    // 常量定义
    // ================================================================

    var CoordType = {
        RATIONAL: 0,
        ALGEBRAIC: 1,
        QUADRATIC: 2,
        TRANSCENDENTAL: 3
    };

    var TrustColor = {
        GREEN: 0,
        BLUE: 1,
        YELLOW: 2,
        ORANGE: 3,
        LIGHT_ORANGE: 4,
        AMBER: 5
    };

    /**
     * 几何节点类型枚举（数字常量）
     * 注意：后端使用数字枚举（geomType），前端 graph_to_formula.js 使用字符串类型（node.type）。
     * 两者映射关系：
     *   POINT: 0          <-> 'point'
     *   LINE_SEGMENT: 1   <-> 'line_segment'
     *   REGION: 2         <-> 'region'
     *   PORT: 3           <-> 'port'
     *   FUNCTION_BLOCK: 4 <-> 'function_block'
     * 跨模块使用时建议通过 formula_module.js 的 _syncPointsFromGraph 方法统一转换。
     */
    var GeomType = {
        POINT: 0,
        LINE_SEGMENT: 1,
        REGION: 2,
        PORT: 3,
        FUNCTION_BLOCK: 4
    };

    var ConstraintType = {
        INCIDENCE: 0,
        BETWEENNESS: 1,
        INTERSECTION: 2,
        CONTAINMENT: 3,
        CONNECTION: 4
    };

    // ================================================================
    // 后端构造函数
    // ================================================================

    function Backend() {
        this.version = "3.3.0-js";
        this._counters = {
            coordCreated: 0,
            coordDestroyed: 0,
            graphsCreated: 0,
            graphsDestroyed: 0,
            normalizations: 0,
            solves: 0,
            rewrites: 0,
            unifications: 0
        };
    }

    // ================================================================
    // 0. 工具辅助函数
    // ================================================================

    // 中文说明：欧几里得最大公约数算法，处理负数、零和特殊值（Infinity/NaN）
    //           增加了完整的边界检查，防止无限循环或错误返回值。
    Backend.prototype._gcd = function(a, b) {
        // 类型守卫：确保输入为数字类型
        if (typeof a !== 'number' || typeof b !== 'number') return 0;
        // NaN 检查：任一为 NaN 则返回 0
        if (isNaN(a) || isNaN(b)) return 0;
        // Infinity 检查：Infinity 无有意义的 GCD，返回另一个有限参数或 0
        if (!isFinite(a) && !isFinite(b)) return 0;
        if (!isFinite(a)) return Math.abs(b) || 0;
        if (!isFinite(b)) return Math.abs(a) || 0;
        // 转为绝对值处理（GCD 总是非负整数）
        if (a < 0) a = -a;
        if (b < 0) b = -b;
        // 零值处理：gcd(0, n) = n, gcd(0, 0) = 0
        if (a === 0) return b;
        if (b === 0) return a;
        // 确保 a >= b 以减少迭代次数
        if (a < b) { var t = a; a = b; b = t; }
        while (b !== 0) {
            var r = a % b;
            a = b;
            b = r;
        }
        return a;
    };

    Backend.prototype._lcm = function(a, b) {
        if (a === 0 || b === 0) return 0;
        var g = this._gcd(a, b);
        return (a / g) * b;
    };

    Backend.prototype._normalizeRational = function(num, den) {
        if (num === undefined || num === null || den === undefined || den === null ||
            isNaN(num) || isNaN(den)) {
            return { num: 0, den: 1 };
        }
        if (den < 0) { num = -num; den = -den; }
        if (den === 0) { den = 1; num = 0; }
        if (num === 0) { den = 1; }
        var g = this._gcd(num < 0 ? -num : num, den);
        return { num: num / g, den: den / g };
    };

    // 通过索引映射查找节点，O(1) 时间复杂度
    Backend.prototype._findNodeById = function(graph, nodeId) {
        return graph._nodeMap[nodeId] || null;
    };

    // 通过索引映射查找约束，O(1) 时间复杂度
    Backend.prototype._findConstraintById = function(graph, constraintId) {
        return graph._constraintMap[constraintId] || null;
    };

    Backend.prototype._removeNodeById = function(graph, nodeId) {
        var i;
        for (i = 0; i < graph.nodes.length; i++) {
            if (graph.nodes[i].id === nodeId) {
                graph.nodes.splice(i, 1);
                delete graph._nodeMap[nodeId]; // 同步移除索引
                return true;
            }
        }
        return false;
    };

    Backend.prototype._removeConstraintById = function(graph, constraintId) {
        var i;
        for (i = 0; i < graph.constraints.length; i++) {
            if (graph.constraints[i].id === constraintId) {
                graph.constraints.splice(i, 1);
                delete graph._constraintMap[constraintId]; // 同步移除索引
                return true;
            }
        }
        return false;
    };

    Backend.prototype._cloneCoord = function(coord) {
        if (coord.coordType === CoordType.RATIONAL) {
            return { coordType: CoordType.RATIONAL, num: coord.num, den: coord.den };
        }
        if (coord.coordType === CoordType.QUADRATIC) {
            return {
                coordType: CoordType.QUADRATIC,
                aNum: coord.aNum, aDen: coord.aDen,
                bNum: coord.bNum, bDen: coord.bDen,
                n: coord.n
            };
        }
        if (coord.coordType === CoordType.TRANSCENDENTAL) {
            return { coordType: CoordType.TRANSCENDENTAL, name: coord.name };
        }
        return { coordType: CoordType.RATIONAL, num: 0, den: 1 };
    };

    Backend.prototype._coordsEqual = function(a, b) {
        if (a.coordType !== b.coordType) return false;
        if (a.coordType === CoordType.RATIONAL) {
            return a.num === b.num && a.den === b.den;
        }
        if (a.coordType === CoordType.QUADRATIC) {
            return a.aNum === b.aNum && a.aDen === b.aDen &&
                   a.bNum === b.bNum && a.bDen === b.bDen &&
                   a.n === b.n;
        }
        if (a.coordType === CoordType.TRANSCENDENTAL) {
            return a.name === b.name;
        }
        return false;
    };

    Backend.prototype._deepCloneGraph = function(graph) {
        var i, node, constraint;
        var clone = {
            nodes: [],
            constraints: [],
            nextNodeId: graph.nextNodeId,
            nextConstraintId: graph.nextConstraintId,
            nodeCapacity: graph.nodeCapacity
        };
        for (i = 0; i < graph.nodes.length; i++) {
            node = graph.nodes[i];
            var nclone = {};
            var keys = Object.keys(node);
            for (var k = 0; k < keys.length; k++) {
                var key = keys[k];
                if (key === 'coordX' || key === 'coordY') {
                    nclone[key] = this._cloneCoord(node[key]);
                } else if (key === 'segmentIds' || key === 'internals' || key === 'inputs' || key === 'outputs') {
                    nclone[key] = node[key].slice();
                } else {
                    nclone[key] = node[key];
                }
            }
            clone.nodes.push(nclone);
        }
        for (i = 0; i < graph.constraints.length; i++) {
            constraint = graph.constraints[i];
            var cclone = {};
            var ckeys = Object.keys(constraint);
            for (var ck = 0; ck < ckeys.length; ck++) {
                var ckey = ckeys[ck];
                if (ckey === 'nodeIds') {
                    cclone[ckey] = constraint[ckey].slice();
                } else {
                    cclone[ckey] = constraint[ckey];
                }
            }
            clone.constraints.push(cclone);
        }
        return clone;
    };

    // ================================================================
    // 1. 符号坐标系统
    // ================================================================

    /**
     * 创建有理数坐标
     * 将一个数值对（分子/分母）标准化为最简有理数形式。
     * 自动约分并确保分母为正，同时更新坐标创建计数器。
     *
     * @param {number} num - 分子
     * @param {number} den - 分母（不能为 0，若为 0 则自动修正为 1）
     * @returns {Object} 有理数坐标对象 { coordType: RATIONAL, num: number, den: number }
     */
    Backend.prototype.coordCreateRational = function(num, den) {
        this._counters.coordCreated++;
        var r = this._normalizeRational(num, den);
        return { coordType: CoordType.RATIONAL, num: r.num, den: r.den };
    };

    Backend.prototype.coordCreateQuadratic = function(aNum, aDen, bNum, bDen, n) {
        this._counters.coordCreated++;
        var a = this._normalizeRational(aNum, aDen);
        var b = this._normalizeRational(bNum, bDen);
        if (n < 0) n = -n;
        if (b.num === 0) {
            return { coordType: CoordType.RATIONAL, num: a.num, den: a.den };
        }
        return {
            coordType: CoordType.QUADRATIC,
            aNum: a.num, aDen: a.den,
            bNum: b.num, bDen: b.den,
            n: n
        };
    };

    Backend.prototype.coordCreateTranscendental = function(name) {
        this._counters.coordCreated++;
        var lower = name.toLowerCase();
        if (lower !== "pi" && lower !== "e") {
            lower = name;
        }
        return { coordType: CoordType.TRANSCENDENTAL, name: lower };
    };

    Backend.prototype.coordDestroy = function(coord) {
        if (coord) {
            this._counters.coordDestroyed++;
        }
    };

    Backend.prototype.coordCompare = function(a, b) {
        var va = this.coordToDouble(a);
        var vb = this.coordToDouble(b);
        var diff = va - vb;
        if (diff > 1e-9) return 1;
        if (diff < -1e-9) return -1;
        return 0;
    };

    /**
     * 坐标加法
     * 支持以下类型组合的加法运算：
     *   - 有理数 + 有理数（直接通分相加）
     *   - 二次根式 + 二次根式（同根号下合并实部和根式系数）
     *   - 二次根式 + 有理数 / 有理数 + 二次根式（提升为二次根式）
     *   - 超越数（兜底：转为浮点近似）
     *
     * @param {Object} a - 坐标 A
     * @param {Object} b - 坐标 B
     * @returns {Object} 相加后的坐标结果
     */
    Backend.prototype.coordAdd = function(a, b) {
        // Rational + Rational
        if (a.coordType === CoordType.RATIONAL && b.coordType === CoordType.RATIONAL) {
            var num = a.num * b.den + b.num * a.den;
            var den = a.den * b.den;
            return this.coordCreateRational(num, den);
        }
        // Quadratic + Quadratic (same radicand)
        if (a.coordType === CoordType.QUADRATIC && b.coordType === CoordType.QUADRATIC && a.n === b.n) {
            var aVal = this._normalizeRational(a.aNum, a.aDen);
            var bVal = this._normalizeRational(b.aNum, b.aDen);
            var aSum = this._normalizeRational(aVal.num * bVal.den + bVal.num * aVal.den, aVal.den * bVal.den);
            var bCoefA = this._normalizeRational(a.bNum, a.bDen);
            var bCoefB = this._normalizeRational(b.bNum, b.bDen);
            var bSum = this._normalizeRational(bCoefA.num * bCoefB.den + bCoefB.num * bCoefA.den, bCoefA.den * bCoefB.den);
            return this.coordCreateQuadratic(aSum.num, aSum.den, bSum.num, bSum.den, a.n);
        }
        // Quadratic + Rational
        if (a.coordType === CoordType.QUADRATIC && b.coordType === CoordType.RATIONAL) {
            var newA = this._normalizeRational(a.aNum * b.den + b.num * a.aDen, a.aDen * b.den);
            return this.coordCreateQuadratic(newA.num, newA.den, a.bNum, a.bDen, a.n);
        }
        // Rational + Quadratic
        if (a.coordType === CoordType.RATIONAL && b.coordType === CoordType.QUADRATIC) {
            return this.coordAdd(b, a);
        }
        // Transcendental + anything: fallback to double
        var va = this.coordToDouble(a);
        var vb = this.coordToDouble(b);
        var sum = va + vb;
        // Approximate as rational
        var den2 = 1000000;
        var num2 = Math.round(sum * den2);
        return this.coordCreateRational(num2, den2);
    };

    /**
     * 坐标减法
     * 将减法转化为加法：a - b = a + (-b)，然后调用 coordAdd。
     * 对于有理数和二次根式分别取反后再相加。
     *
     * @param {Object} a - 被减坐标
     * @param {Object} b - 减数坐标
     * @returns {Object} 相减后的坐标结果
     */
    Backend.prototype.coordSubtract = function(a, b) {
        if (b.coordType === CoordType.RATIONAL) {
            var negB = this.coordCreateRational(-b.num, b.den);
            return this.coordAdd(a, negB);
        }
        if (b.coordType === CoordType.QUADRATIC) {
            var negBQ = this.coordCreateQuadratic(-b.aNum, b.aDen, -b.bNum, b.bDen, b.n);
            return this.coordAdd(a, negBQ);
        }
        // Transcendental fallback
        var va = this.coordToDouble(a);
        var vb = this.coordToDouble(b);
        var diff = va - vb;
        var den2 = 1000000;
        var num2 = Math.round(diff * den2);
        return this.coordCreateRational(num2, den2);
    };

    /**
     * 坐标乘法
     * 支持以下类型组合：
     *   - 有理数 * 有理数（分子分母分别相乘后约分）
     *   - 二次根式 * 有理数（实部和根式系数分别乘以有理数）
     *   - 二次根式 * 二次根式（同根号下：(a+b*sqrt(n))*(c+d*sqrt(n)) = ac+bd*n + (ad+bc)*sqrt(n)）
     *   - 超越数（兜底：转为浮点近似有理数）
     *
     * @param {Object} a - 坐标 A
     * @param {Object} b - 坐标 B
     * @returns {Object} 相乘后的坐标结果
     */
    Backend.prototype.coordMultiply = function(a, b) {
        // Rational * Rational
        if (a.coordType === CoordType.RATIONAL && b.coordType === CoordType.RATIONAL) {
            return this.coordCreateRational(a.num * b.num, a.den * b.den);
        }
        // Quadratic * Rational
        if (a.coordType === CoordType.QUADRATIC && b.coordType === CoordType.RATIONAL) {
            var newA = this._normalizeRational(a.aNum * b.num, a.aDen * b.den);
            var newB = this._normalizeRational(a.bNum * b.num, a.bDen * b.den);
            return this.coordCreateQuadratic(newA.num, newA.den, newB.num, newB.den, a.n);
        }
        // Rational * Quadratic
        if (a.coordType === CoordType.RATIONAL && b.coordType === CoordType.QUADRATIC) {
            return this.coordMultiply(b, a);
        }
        // Quadratic * Quadratic: (a1+b1*sqrt(n))*(a2+b2*sqrt(n)) = (a1*a2+b1*b2*n) + (a1*b2+a2*b1)*sqrt(n)
        if (a.coordType === CoordType.QUADRATIC && b.coordType === CoordType.QUADRATIC && a.n === b.n) {
            // Simplify: compute (a1*a2 + b1*b2*n) and (a1*b2 + a2*b1) as rationals
            var a1 = this._normalizeRational(a.aNum, a.aDen);
            var b1 = this._normalizeRational(a.bNum, a.bDen);
            var a2 = this._normalizeRational(b.aNum, b.aDen);
            var b2 = this._normalizeRational(b.bNum, b.bDen);
            // real = a1*a2 + b1*b2*n
            var prod1 = this._normalizeRational(a1.num * a2.num, a1.den * a2.den);
            var prod2 = this._normalizeRational(b1.num * b2.num, b1.den * b2.den);
            var prod2n = this._normalizeRational(prod2.num * a.n, prod2.den);
            var realPart = this._normalizeRational(prod1.num * prod2n.den + prod2n.num * prod1.den, prod1.den * prod2n.den);
            // sqrt coef = a1*b2 + a2*b1
            var prod3 = this._normalizeRational(a1.num * b2.num, a1.den * b2.den);
            var prod4 = this._normalizeRational(a2.num * b1.num, a2.den * b1.den);
            var sqrtPart = this._normalizeRational(prod3.num * prod4.den + prod4.num * prod3.den, prod3.den * prod4.den);
            return this.coordCreateQuadratic(realPart.num, realPart.den, sqrtPart.num, sqrtPart.den, a.n);
        }
        // Transcendental fallback
        var va = this.coordToDouble(a);
        var vb = this.coordToDouble(b);
        var prod = va * vb;
        var den2 = 1000000;
        var num2 = Math.round(prod * den2);
        return this.coordCreateRational(num2, den2);
    };

    /**
     * 坐标除法
     * 支持以下类型组合：
     *   - 有理数 / 有理数（交叉相乘后约分）
     *   - 二次根式 / 有理数（实部和根式系数分别除以有理数）
     *   - 有理数 / 二次根式（乘以共轭根式消去分母根号）
     *   - 二次根式 / 二次根式（同根号下乘以分母共轭根式）
     *   除数为零时返回 null。
     *
     * @param {Object} a - 被除数坐标
     * @param {Object} b - 除数坐标
     * @returns {Object|null} 相除后的坐标结果，除数为零返回 null
     */
    Backend.prototype.coordDivide = function(a, b) {
        // Division by zero check
        var bVal = this.coordToDouble(b);
        if (bVal === 0) {
            return null;
        }
        // Rational / Rational
        if (a.coordType === CoordType.RATIONAL && b.coordType === CoordType.RATIONAL) {
            return this.coordCreateRational(a.num * b.den, a.den * b.num);
        }
        // Quadratic / Rational
        if (a.coordType === CoordType.QUADRATIC && b.coordType === CoordType.RATIONAL) {
            var newA = this._normalizeRational(a.aNum * b.den, a.aDen * b.num);
            var newB = this._normalizeRational(a.bNum * b.den, a.bDen * b.num);
            return this.coordCreateQuadratic(newA.num, newA.den, newB.num, newB.den, a.n);
        }
        // Rational / Quadratic: multiply by conjugate
        if (a.coordType === CoordType.RATIONAL && b.coordType === CoordType.QUADRATIC) {
            // a / (b_a + b_b*sqrt(n)) = a*(b_a - b_b*sqrt(n)) / (b_a^2 - b_b^2*n)
            var ba = this._normalizeRational(b.aNum, b.aDen);
            var bb = this._normalizeRational(b.bNum, b.bDen);
            var aa = this._normalizeRational(a.num, a.den);
            // denominator = ba^2 - bb^2*n
            var ba2 = this._normalizeRational(ba.num * ba.num, ba.den * ba.den);
            var bb2 = this._normalizeRational(bb.num * bb.num, bb.den * bb.den);
            var bb2n = this._normalizeRational(bb2.num * b.n, bb2.den);
            var denom = this._normalizeRational(ba2.num * bb2n.den - bb2n.num * ba2.den, ba2.den * bb2n.den);
            if (denom.num === 0) {
                // Degenerate: fallback to double
                var va = this.coordToDouble(a);
                var vb = this.coordToDouble(b);
                var q = va / vb;
                var d = 1000000;
                return this.coordCreateRational(Math.round(q * d), d);
            }
            // numerator rational part = aa * ba
            var numR = this._normalizeRational(aa.num * ba.num, aa.den * ba.den);
            // numerator sqrt part = aa * (-bb)
            var numS = this._normalizeRational(-aa.num * bb.num, aa.den * bb.den);
            // divide both by denom
            var finalR = this._normalizeRational(numR.num * denom.den, numR.den * denom.num);
            var finalS = this._normalizeRational(numS.num * denom.den, numS.den * denom.num);
            return this.coordCreateQuadratic(finalR.num, finalR.den, finalS.num, finalS.den, b.n);
        }
        // Quadratic / Quadratic (same n): multiply by conjugate
        if (a.coordType === CoordType.QUADRATIC && b.coordType === CoordType.QUADRATIC && a.n === b.n) {
            // (a_a+a_b*sqrt(n))/(b_a+b_b*sqrt(n)) = (a_a+a_b*sqrt(n))*(b_a-b_b*sqrt(n))/(b_a^2-b_b^2*n)
            var conj = this.coordCreateQuadratic(b.aNum, b.aDen, -b.bNum, b.bDen, b.n);
            var numCoord = this.coordMultiply(a, conj);
            var ba3 = this._normalizeRational(b.aNum, b.aDen);
            var bb3 = this._normalizeRational(b.bNum, b.bDen);
            var ba2_3 = this._normalizeRational(ba3.num * ba3.num, ba3.den * ba3.den);
            var bb2_3 = this._normalizeRational(bb3.num * bb3.num, bb3.den * bb3.den);
            var bb2n3 = this._normalizeRational(bb2_3.num * b.n, bb2_3.den);
            var denom3 = this._normalizeRational(ba2_3.num * bb2n3.den - bb2n3.num * ba2_3.den, ba2_3.den * bb2n3.den);
            if (denom3.num === 0) {
                var va2 = this.coordToDouble(a);
                var vb2 = this.coordToDouble(b);
                var q2 = va2 / vb2;
                var d2 = 1000000;
                return this.coordCreateRational(Math.round(q2 * d2), d2);
            }
            if (numCoord.coordType === CoordType.QUADRATIC) {
                var fR = this._normalizeRational(numCoord.aNum * denom3.den, numCoord.aDen * denom3.num);
                var fS = this._normalizeRational(numCoord.bNum * denom3.den, numCoord.bDen * denom3.num);
                return this.coordCreateQuadratic(fR.num, fR.den, fS.num, fS.den, a.n);
            }
            if (numCoord.coordType === CoordType.RATIONAL) {
                var fR2 = this._normalizeRational(numCoord.num * denom3.den, numCoord.den * denom3.num);
                return this.coordCreateRational(fR2.num, fR2.den);
            }
        }
        // Transcendental fallback
        var va3 = this.coordToDouble(a);
        var vb3 = this.coordToDouble(b);
        var q3 = va3 / vb3;
        var d3 = 1000000;
        return this.coordCreateRational(Math.round(q3 * d3), d3);
    };

    Backend.prototype.coordSerialize = function(coord) {
        if (!coord) return "null";
        if (coord.coordType === CoordType.RATIONAL) {
            if (coord.den === 1) return String(coord.num);
            return coord.num + "/" + coord.den;
        }
        if (coord.coordType === CoordType.QUADRATIC) {
            var aStr = (coord.aDen === 1) ? String(coord.aNum) : (coord.aNum + "/" + coord.aDen);
            var bStr = (coord.bDen === 1) ? String(coord.bNum) : (coord.bNum + "/" + coord.bDen);
            if (coord.bNum === 0) return aStr;
            if (coord.aNum === 0) {
                if (coord.bNum === 1 && coord.bDen === 1) return "sqrt(" + coord.n + ")";
                if (coord.bNum === -1 && coord.bDen === 1) return "-sqrt(" + coord.n + ")";
                return bStr + "*sqrt(" + coord.n + ")";
            }
            if (coord.bNum > 0) {
                return aStr + "+" + bStr + "*sqrt(" + coord.n + ")";
            }
            return aStr + bStr + "*sqrt(" + coord.n + ")";
        }
        if (coord.coordType === CoordType.TRANSCENDENTAL) {
            return coord.name;
        }
        return "unknown";
    };

    Backend.prototype.coordToDouble = function(coord) {
        if (!coord) return 0;
        if (coord.coordType === CoordType.RATIONAL) {
            return coord.num / coord.den;
        }
        if (coord.coordType === CoordType.QUADRATIC) {
            var a = coord.aNum / coord.aDen;
            var b = coord.bNum / coord.bDen;
            return a + b * Math.sqrt(coord.n);
        }
        if (coord.coordType === CoordType.TRANSCENDENTAL) {
            if (coord.name === "pi") return Math.PI;
            if (coord.name === "e") return Math.E;
            return 0;
        }
        return 0;
    };

    // ================================================================
    // 2. 约束图系统
    // ================================================================

    /**
     * 创建约束图
     * 初始化一个空的约束图数据结构，包含空的节点列表和约束列表。
     * 同时设置节点 ID 和约束 ID 的自增计数器（从 0 开始）。
     *
     * @returns {Object} 新创建的约束图对象
     *   { nodes: Array, constraints: Array, nextNodeId: number, nextConstraintId: number, nodeCapacity: number }
     */
    Backend.prototype.graphCreate = function() {
        this._counters.graphsCreated++;
        return {
            nodes: [],
            constraints: [],
            nextNodeId: 0,
            nextConstraintId: 0,
            nodeCapacity: 64,
            // 节点 ID -> 节点对象的索引映射，用于 O(1) 查找
            _nodeMap: {},
            // 约束 ID -> 约束对象的索引映射，用于 O(1) 查找
            _constraintMap: {}
        };
    };

    Backend.prototype.graphDestroy = function(graph) {
        if (graph) {
            this._counters.graphsDestroyed++;
            graph.nodes = null;
            graph.constraints = null;
            graph._nodeMap = null; // 清理节点索引
            graph._constraintMap = null; // 清理约束索引
        }
    };

    /**
     * 向约束图中添加一个点节点
     * 创建一个 GEOM_TYPE.POINT 类型的节点，附带规范化的 X 坐标和 Y 坐标。
     * 节点 ID 由图的 nextNodeId 自增计数器自动分配。
     *
     * @param {Object} graph - 目标约束图
     * @param {Object} coordX - X 轴符号坐标（由 coordCreateRational 等创建）
     * @param {Object} coordY - Y 轴符号坐标（由 coordCreateRational 等创建）
     * @returns {number} 新创建的点节点 ID
     */
    Backend.prototype.graphAddPoint = function(graph, coordX, coordY) {
        var id = graph.nextNodeId++;
        var node = {
            id: id,
            geomType: GeomType.POINT,
            coordX: this._cloneCoord(coordX),
            coordY: this._cloneCoord(coordY)
        };
        graph.nodes.push(node);
        graph._nodeMap[id] = node; // 同步更新索引
        return id;
    };

    /**
     * 向约束图中添加线段节点
     * 创建 LINE_SEGMENT 类型的节点，连接已有的两个点节点。
     * 会先验证两个端点是否存在，不存在时返回 -1。
     *
     * @param {Object} graph - 目标约束图
     * @param {number} p1Id - 起点节点 ID
     * @param {number} p2Id - 终点节点 ID
     * @returns {number} 新创建的线段节点 ID（失败返回 -1）
     */
    Backend.prototype.graphAddLineSegment = function(graph, p1Id, p2Id) {
        if (!graph) return -1;
        var p1 = this._findNodeById(graph, p1Id);
        var p2 = this._findNodeById(graph, p2Id);
        if (!p1 || !p2) return -1;
        var id = graph.nextNodeId++;
        var node = {
            id: id,
            geomType: GeomType.LINE_SEGMENT,
            endpoint1Id: p1Id,
            endpoint2Id: p2Id
        };
        graph.nodes.push(node);
        graph._nodeMap[id] = node; // 同步更新索引
        return id;
    };

    Backend.prototype.graphAddRegion = function(graph, segmentIds) {
        var id = graph.nextNodeId++;
        var segCopy = [];
        var i;
        for (i = 0; i < segmentIds.length; i++) {
            segCopy.push(segmentIds[i]);
        }
        graph.nodes.push({
            id: id,
            geomType: GeomType.REGION,
            segmentIds: segCopy
        });
        graph._nodeMap[id] = graph.nodes[graph.nodes.length - 1]; // 同步更新索引
        return id;
    };

    Backend.prototype.graphAddPort = function(graph, type, namespaceDepth, parentBlockId) {
        var id = graph.nextNodeId++;
        var node = {
            id: id,
            geomType: GeomType.PORT,
            portType: type,
            namespaceDepth: namespaceDepth || 0,
            parentBlockId: parentBlockId !== undefined ? parentBlockId : -1
        };
        graph.nodes.push(node);
        graph._nodeMap[id] = node; // 同步更新索引
        return id;
    };

    Backend.prototype.graphAddFunctionBlock = function(graph, internals, inputs, outputs) {
        var id = graph.nextNodeId++;
        var intCopy = [], inCopy = [], outCopy = [];
        var i;
        for (i = 0; i < internals.length; i++) intCopy.push(internals[i]);
        for (i = 0; i < inputs.length; i++) inCopy.push(inputs[i]);
        for (i = 0; i < outputs.length; i++) outCopy.push(outputs[i]);
        graph.nodes.push({
            id: id,
            geomType: GeomType.FUNCTION_BLOCK,
            internals: intCopy,
            inputs: inCopy,
            outputs: outCopy
        });
        graph._nodeMap[id] = graph.nodes[graph.nodes.length - 1]; // 同步更新索引
        return id;
    };

    Backend.prototype.graphRemoveNode = function(graph, nodeId) {
        // Remove all constraints referencing this node
        var i, c;
        var toRemove = [];
        for (i = 0; i < graph.constraints.length; i++) {
            c = graph.constraints[i];
            var found = false;
            for (var j = 0; j < c.nodeIds.length; j++) {
                if (c.nodeIds[j] === nodeId) { found = true; break; }
            }
            if (found) toRemove.push(c.id);
        }
        for (i = 0; i < toRemove.length; i++) {
            this._removeConstraintById(graph, toRemove[i]);
        }
        return this._removeNodeById(graph, nodeId);
    };

    Backend.prototype.graphGetNode = function(graph, nodeId) {
        return this._findNodeById(graph, nodeId);
    };

    Backend.prototype.graphAddIncidence = function(graph, pointId, targetId) {
        if (!graph) return -1;
        var p1 = this._findNodeById(graph, pointId);
        var p2 = this._findNodeById(graph, targetId);
        if (!p1 || !p2) return -1;
        var id = graph.nextConstraintId++;
        var constraint = {
            id: id,
            constraintType: ConstraintType.INCIDENCE,
            nodeIds: [pointId, targetId]
        };
        graph.constraints.push(constraint);
        graph._constraintMap[id] = constraint; // 同步更新索引
        return id;
    };

    Backend.prototype.graphAddBetweenness = function(graph, p1Id, p2Id, p3Id) {
        if (!graph) return -1;
        var n1 = this._findNodeById(graph, p1Id);
        var n2 = this._findNodeById(graph, p2Id);
        var n3 = this._findNodeById(graph, p3Id);
        if (!n1 || !n2 || !n3) return -1;
        var id = graph.nextConstraintId++;
        var constraint = {
            id: id,
            constraintType: ConstraintType.BETWEENNESS,
            nodeIds: [p1Id, p2Id, p3Id]
        };
        graph.constraints.push(constraint);
        graph._constraintMap[id] = constraint; // 同步更新索引
        return id;
    };

    Backend.prototype.graphAddIntersection = function(graph, line1Id, line2Id, resultPointId) {
        if (!graph) return -1;
        var n1 = this._findNodeById(graph, line1Id);
        var n2 = this._findNodeById(graph, line2Id);
        var n3 = this._findNodeById(graph, resultPointId);
        if (!n1 || !n2 || !n3) return -1;
        var id = graph.nextConstraintId++;
        var constraint = {
            id: id,
            constraintType: ConstraintType.INTERSECTION,
            nodeIds: [line1Id, line2Id, resultPointId]
        };
        graph.constraints.push(constraint);
        graph._constraintMap[id] = constraint; // 同步更新索引
        return id;
    };

    Backend.prototype.graphAddContainment = function(graph, innerId, outerId) {
        if (!graph) return -1;
        var n1 = this._findNodeById(graph, innerId);
        var n2 = this._findNodeById(graph, outerId);
        if (!n1 || !n2) return -1;
        var id = graph.nextConstraintId++;
        var constraint = {
            id: id,
            constraintType: ConstraintType.CONTAINMENT,
            nodeIds: [innerId, outerId]
        };
        graph.constraints.push(constraint);
        graph._constraintMap[id] = constraint; // 同步更新索引
        return id;
    };

    Backend.prototype.graphAddConnection = function(graph, srcPortId, dstPortId) {
        if (!graph) return -1;
        var n1 = this._findNodeById(graph, srcPortId);
        var n2 = this._findNodeById(graph, dstPortId);
        if (!n1 || !n2) return -1;
        var id = graph.nextConstraintId++;
        var constraint = {
            id: id,
            constraintType: ConstraintType.CONNECTION,
            nodeIds: [srcPortId, dstPortId]
        };
        graph.constraints.push(constraint);
        graph._constraintMap[id] = constraint; // 同步更新索引
        return id;
    };

    Backend.prototype.graphRemoveConstraint = function(graph, constraintId) {
        return this._removeConstraintById(graph, constraintId);
    };

    Backend.prototype.graphGetConstraint = function(graph, constraintId) {
        return this._findConstraintById(graph, constraintId);
    };

    Backend.prototype.graphGetNodeCount = function(graph) {
        return graph.nodes.length;
    };

    Backend.prototype.graphGetConstraintCount = function(graph) {
        return graph.constraints.length;
    };

    Backend.prototype.graphDetectRedundantConstraints = function(graph) {
        // A constraint is redundant if removing it does not change the
        // effective constraint set (another constraint implies the same relation).
        // Simple heuristic: duplicate constraints on the same node set.
        var redundant = [];
        var seen = {};
        var i, j, c, key;
        for (i = 0; i < graph.constraints.length; i++) {
            c = graph.constraints[i];
            var sorted = c.nodeIds.slice().sort(function(a, b) { return a - b; });
            key = c.constraintType + ":" + sorted.join(",");
            if (seen[key]) {
                redundant.push(c.id);
            } else {
                seen[key] = true;
            }
        }
        return redundant;
    };

    Backend.prototype.graphDetectConflicts = function(graph) {
        // A conflict occurs when constraints on the same nodes are incompatible.
        // E.g., two betweenness constraints that order the same three points differently.
        var conflicts = [];
        var betweennessMap = {};
        var i, c, key;
        for (i = 0; i < graph.constraints.length; i++) {
            c = graph.constraints[i];
            if (c.constraintType === ConstraintType.BETWEENNESS) {
                // Normalize: sort the three points, store the middle
                var pts = c.nodeIds.slice();
                var mid = pts[1];
                var sorted = pts.slice().sort(function(a, b) { return a - b; });
                key = sorted.join(",");
                if (betweennessMap[key] !== undefined && betweennessMap[key] !== mid) {
                    conflicts.push({
                        type: "betweenness_conflict",
                        nodeIds: sorted,
                        constraintIds: [betweennessMap[key + "_cid"], c.id]
                    });
                } else {
                    betweennessMap[key] = mid;
                    betweennessMap[key + "_cid"] = c.id;
                }
            }
        }
        // Check for self-intersection conflicts: a point claimed as intersection
        // of two lines but not actually on both
        for (i = 0; i < graph.constraints.length; i++) {
            c = graph.constraints[i];
            if (c.constraintType === ConstraintType.INTERSECTION) {
                var line1 = this._findNodeById(graph, c.nodeIds[0]);
                var line2 = this._findNodeById(graph, c.nodeIds[1]);
                var pt = this._findNodeById(graph, c.nodeIds[2]);
                if (line1 && line2 && pt) {
                    // Check incidence: pt must be on both lines
                    var onLine1 = false, onLine2 = false;
                    for (var j = 0; j < graph.constraints.length; j++) {
                        var ic = graph.constraints[j];
                        if (ic.constraintType === ConstraintType.INCIDENCE) {
                            if ((ic.nodeIds[0] === c.nodeIds[2] && ic.nodeIds[1] === c.nodeIds[0]) ||
                                (ic.nodeIds[0] === c.nodeIds[0] && ic.nodeIds[1] === c.nodeIds[2])) {
                                onLine1 = true;
                            }
                            if ((ic.nodeIds[0] === c.nodeIds[2] && ic.nodeIds[1] === c.nodeIds[1]) ||
                                (ic.nodeIds[0] === c.nodeIds[1] && ic.nodeIds[1] === c.nodeIds[2])) {
                                onLine2 = true;
                            }
                        }
                    }
                    if (!onLine1 || !onLine2) {
                        conflicts.push({
                            type: "intersection_incidence_missing",
                            nodeIds: c.nodeIds,
                            constraintId: c.id
                        });
                    }
                }
            }
        }
        return conflicts;
    };

    Backend.prototype.graphValidateRegionClosure = function(graph, regionId) {
        var region = this._findNodeById(graph, regionId);
        if (!region || region.geomType !== GeomType.REGION) {
            return { valid: false, reason: "not_a_region" };
        }
        if (region.segmentIds.length === 0) {
            return { valid: false, reason: "no_segments" };
        }
        // Check that all segments exist
        var i;
        for (i = 0; i < region.segmentIds.length; i++) {
            var seg = this._findNodeById(graph, region.segmentIds[i]);
            if (!seg || seg.geomType !== GeomType.LINE_SEGMENT) {
                return { valid: false, reason: "invalid_segment", segmentId: region.segmentIds[i] };
            }
        }
        // Check that segments form a closed chain (endpoint connectivity)
        if (region.segmentIds.length >= 2) {
            var first = this._findNodeById(graph, region.segmentIds[0]);
            var prevEnd = first ? first.endpoint2Id : -1;
            for (i = 1; i < region.segmentIds.length; i++) {
                var seg2 = this._findNodeById(graph, region.segmentIds[i]);
                if (!seg2) {
                    return { valid: false, reason: "segment_not_found", segmentId: region.segmentIds[i] };
                }
                if (seg2.endpoint1Id !== prevEnd) {
                    return { valid: false, reason: "gap_in_chain", afterSegment: region.segmentIds[i - 1], beforeSegment: region.segmentIds[i] };
                }
                prevEnd = seg2.endpoint2Id;
            }
            // Check closure: last segment's endpoint2 should equal first segment's endpoint1
            if (prevEnd !== first.endpoint1Id) {
                return { valid: false, reason: "not_closed" };
            }
        }
        return { valid: true };
    };

    // ================================================================
    // 3. 归一化引擎
    // ================================================================

    /**
     * 图规范化引擎
     * 对约束图进行归一化处理，消除冗余节点和约束。
     *
     * 基本模式（scopeAware = false）：
     *   - 合并坐标完全相同的 POINT 节点
     *
     * 作用域感知模式（scopeAware = true）：
     *   - 除基本合并外，还合并端点相同的 LINE_SEGMENT 节点
     *   - 合并后更新所有约束引用和节点间引用关系
     *
     * 返回合并统计信息，包括合并映射表和合并次数。
     *
     * @param {Object} graph - 待规范化的约束图
     * @param {boolean} scopeAware - 是否启用作用域感知模式
     * @returns {Object} { merged_count: number, merge_map: Object, merges: Array }
     */
    Backend.prototype.graphNormalize = function(graph, scopeAware) {
        this._counters.normalizations++;
        var mergedCount = 0;
        var mergeMap = {};
        var merges = [];
        var i, j, a, b;

        // Merge nodes with identical coordinates (points only)
        for (i = 0; i < graph.nodes.length; i++) {
            if (mergeMap[graph.nodes[i].id] !== undefined) continue;
            if (graph.nodes[i].geomType !== GeomType.POINT) continue;

            for (j = i + 1; j < graph.nodes.length; j++) {
                if (mergeMap[graph.nodes[j].id] !== undefined) continue;
                if (graph.nodes[j].geomType !== GeomType.POINT) continue;

                a = graph.nodes[i];
                b = graph.nodes[j];

                if (this._coordsEqual(a.coordX, b.coordX) && this._coordsEqual(a.coordY, b.coordY)) {
                    mergeMap[b.id] = a.id;
                    merges.push({ from: b.id, to: a.id });
                    mergedCount++;
                }
            }
        }

        // Scope-aware: also merge line segments with identical endpoints
        if (scopeAware) {
            for (i = 0; i < graph.nodes.length; i++) {
                if (mergeMap[graph.nodes[i].id] !== undefined) continue;
                if (graph.nodes[i].geomType !== GeomType.LINE_SEGMENT) continue;

                for (j = i + 1; j < graph.nodes.length; j++) {
                    if (mergeMap[graph.nodes[j].id] !== undefined) continue;
                    if (graph.nodes[j].geomType !== GeomType.LINE_SEGMENT) continue;

                    a = graph.nodes[i];
                    b = graph.nodes[j];
                    var a1 = a.endpoint1Id, a2 = a.endpoint2Id;
                    var b1 = b.endpoint1Id, b2 = b.endpoint2Id;
                    // Resolve through merge map
                    if (mergeMap[a1] !== undefined) a1 = mergeMap[a1];
                    if (mergeMap[a2] !== undefined) a2 = mergeMap[a2];
                    if (mergeMap[b1] !== undefined) b1 = mergeMap[b1];
                    if (mergeMap[b2] !== undefined) b2 = mergeMap[b2];

                    if ((a1 === b1 && a2 === b2) || (a1 === b2 && a2 === b1)) {
                        mergeMap[b.id] = a.id;
                        merges.push({ from: b.id, to: a.id });
                        mergedCount++;
                    }
                }
            }
        }

        if (mergedCount > 0) {
            // Update constraint references
            for (i = 0; i < graph.constraints.length; i++) {
                var c = graph.constraints[i];
                for (j = 0; j < c.nodeIds.length; j++) {
                    if (mergeMap[c.nodeIds[j]] !== undefined) {
                        c.nodeIds[j] = mergeMap[c.nodeIds[j]];
                    }
                }
            }
            // Update node references (line segment endpoints, region segmentIds, etc.)
            for (i = 0; i < graph.nodes.length; i++) {
                var node = graph.nodes[i];
                if (node.geomType === GeomType.LINE_SEGMENT) {
                    if (mergeMap[node.endpoint1Id] !== undefined) node.endpoint1Id = mergeMap[node.endpoint1Id];
                    if (mergeMap[node.endpoint2Id] !== undefined) node.endpoint2Id = mergeMap[node.endpoint2Id];
                }
                if (node.geomType === GeomType.REGION && node.segmentIds) {
                    for (j = 0; j < node.segmentIds.length; j++) {
                        if (mergeMap[node.segmentIds[j]] !== undefined) {
                            node.segmentIds[j] = mergeMap[node.segmentIds[j]];
                        }
                    }
                }
                if (node.geomType === GeomType.PORT && node.parentBlockId !== undefined) {
                    if (mergeMap[node.parentBlockId] !== undefined) {
                        node.parentBlockId = mergeMap[node.parentBlockId];
                    }
                }
                if (node.geomType === GeomType.FUNCTION_BLOCK) {
                    if (node.internals) {
                        for (j = 0; j < node.internals.length; j++) {
                            if (mergeMap[node.internals[j]] !== undefined) node.internals[j] = mergeMap[node.internals[j]];
                        }
                    }
                    if (node.inputs) {
                        for (j = 0; j < node.inputs.length; j++) {
                            if (mergeMap[node.inputs[j]] !== undefined) node.inputs[j] = mergeMap[node.inputs[j]];
                        }
                    }
                    if (node.outputs) {
                        for (j = 0; j < node.outputs.length; j++) {
                            if (mergeMap[node.outputs[j]] !== undefined) node.outputs[j] = mergeMap[node.outputs[j]];
                        }
                    }
                }
            }
            // Remove merged nodes
            var newNodes = [];
            for (i = 0; i < graph.nodes.length; i++) {
                if (mergeMap[graph.nodes[i].id] === undefined) {
                    newNodes.push(graph.nodes[i]);
                }
            }
            graph.nodes = newNodes;
        }

        return { mergedCount: mergedCount, merges: merges };
    };

    Backend.prototype.findMergeCandidates = function(graph) {
        var candidates = [];
        var i, j, a, b;
        for (i = 0; i < graph.nodes.length; i++) {
            if (graph.nodes[i].geomType !== GeomType.POINT) continue;
            for (j = i + 1; j < graph.nodes.length; j++) {
                if (graph.nodes[j].geomType !== GeomType.POINT) continue;
                a = graph.nodes[i];
                b = graph.nodes[j];
                if (this._coordsEqual(a.coordX, b.coordX) && this._coordsEqual(a.coordY, b.coordY)) {
                    candidates.push({ nodeA: a.id, nodeB: b.id });
                }
            }
        }
        return candidates;
    };

    Backend.prototype.graphTopologicalSort = function(graph) {
        // Build adjacency from constraints: if constraint references nodes, create ordering
        var inDegree = {};
        var adj = {};
        var i, j, c, node;
        for (i = 0; i < graph.nodes.length; i++) {
            var nid = graph.nodes[i].id;
            inDegree[nid] = 0;
            adj[nid] = [];
        }
        // For line segments, endpoints come before the segment
        for (i = 0; i < graph.nodes.length; i++) {
            node = graph.nodes[i];
            if (node.geomType === GeomType.LINE_SEGMENT) {
                if (inDegree[node.endpoint1Id] !== undefined) {
                    adj[node.endpoint1Id].push(node.id);
                    inDegree[node.id] = (inDegree[node.id] || 0) + 1;
                }
                if (inDegree[node.endpoint2Id] !== undefined) {
                    adj[node.endpoint2Id].push(node.id);
                    inDegree[node.id] = (inDegree[node.id] || 0) + 1;
                }
            }
            if (node.geomType === GeomType.REGION && node.segmentIds) {
                for (j = 0; j < node.segmentIds.length; j++) {
                    if (inDegree[node.segmentIds[j]] !== undefined) {
                        adj[node.segmentIds[j]].push(node.id);
                        inDegree[node.id] = (inDegree[node.id] || 0) + 1;
                    }
                }
            }
            if (node.geomType === GeomType.PORT && node.parentBlockId !== undefined && node.parentBlockId >= 0) {
                if (inDegree[node.parentBlockId] !== undefined) {
                    adj[node.parentBlockId].push(node.id);
                    inDegree[node.id] = (inDegree[node.id] || 0) + 1;
                }
            }
        }
        // Kahn's algorithm
        var queue = [];
        var keys = Object.keys(inDegree);
        for (i = 0; i < keys.length; i++) {
            if (inDegree[keys[i]] === 0) queue.push(parseInt(keys[i], 10));
        }
        var result = [];
        while (queue.length > 0) {
            var current = queue.shift();
            result.push(current);
            var neighbors = adj[current] || [];
            for (i = 0; i < neighbors.length; i++) {
                inDegree[neighbors[i]]--;
                if (inDegree[neighbors[i]] === 0) {
                    queue.push(neighbors[i]);
                }
            }
        }
        // If result length < node count, there's a cycle
        if (result.length < graph.nodes.length) {
            return { sorted: result, hasCycle: true };
        }
        return { sorted: result, hasCycle: false };
    };

    Backend.prototype.computeGraphHash = function(graph) {
        // Deterministic hash of graph structure
        var parts = [];
        var i, j, node, c;
        parts.push("N:" + graph.nodes.length);
        parts.push("C:" + graph.constraints.length);
        // Sort nodes by id for determinism
        var sortedNodes = graph.nodes.slice().sort(function(a, b) { return a.id - b.id; });
        for (i = 0; i < sortedNodes.length; i++) {
            node = sortedNodes[i];
            var s = node.id + ":" + node.geomType;
            if (node.geomType === GeomType.POINT && node.coordX && node.coordY) {
                s += ":" + this.coordSerialize(node.coordX) + "," + this.coordSerialize(node.coordY);
            }
            if (node.geomType === GeomType.LINE_SEGMENT) {
                s += ":" + node.endpoint1Id + "-" + node.endpoint2Id;
            }
            parts.push(s);
        }
        var sortedConstraints = graph.constraints.slice().sort(function(a, b) { return a.id - b.id; });
        for (i = 0; i < sortedConstraints.length; i++) {
            c = sortedConstraints[i];
            parts.push(c.id + ":" + c.constraintType + ":" + c.nodeIds.join(","));
        }
        var combined = parts.join("|");
        // Simple hash function (djb2)
        var hash = 5381;
        for (i = 0; i < combined.length; i++) {
            hash = ((hash << 5) + hash + combined.charCodeAt(i)) & 0x7FFFFFFF;
        }
        return hash;
    };

    // ================================================================
    // 4. 代数求解器
    // ================================================================

    Backend.prototype.solveAlgebraicSystem = function(graph, dirtyVarIds) {
        this._counters.solves++;
        // Count degrees of freedom for each dirty variable
        var dof = this.countDegreesOfFreedom(graph);
        if (dof.free > 0) {
            return { status: "underdetermined", solutions: [], freeVariables: dof.free };
        }
        // Check for conflicts
        var conflicts = this.checkConflictEquations(graph);
        if (conflicts.length > 0) {
            return { status: "conflict", solutions: [], conflicts: conflicts };
        }
        // For a determined system, return current coordinate values as solutions
        var solutions = [];
        var i;
        for (i = 0; i < dirtyVarIds.length; i++) {
            var node = this._findNodeById(graph, dirtyVarIds[i]);
            if (node && node.geomType === GeomType.POINT) {
                solutions.push({
                    nodeId: dirtyVarIds[i],
                    x: this.coordToDouble(node.coordX),
                    y: this.coordToDouble(node.coordY)
                });
            }
        }
        return { status: "determined", solutions: solutions };
    };

    Backend.prototype.countDegreesOfFreedom = function(graph) {
        // Each point has 2 DOF (x, y)
        // Each incidence constraint removes 1 DOF
        // Each betweenness constraint removes 1 DOF
        // Each intersection constraint removes 2 DOF
        // Each containment constraint removes 1 DOF
        // Each connection constraint removes 1 DOF
        var totalDOF = 0;
        var constraintsDOF = 0;
        var i;
        for (i = 0; i < graph.nodes.length; i++) {
            if (graph.nodes[i].geomType === GeomType.POINT) {
                totalDOF += 2;
            }
        }
        for (i = 0; i < graph.constraints.length; i++) {
            var c = graph.constraints[i];
            if (c.constraintType === ConstraintType.INCIDENCE) constraintsDOF += 1;
            else if (c.constraintType === ConstraintType.BETWEENNESS) constraintsDOF += 1;
            else if (c.constraintType === ConstraintType.INTERSECTION) constraintsDOF += 2;
            else if (c.constraintType === ConstraintType.CONTAINMENT) constraintsDOF += 1;
            else if (c.constraintType === ConstraintType.CONNECTION) constraintsDOF += 1;
        }
        var free = totalDOF - constraintsDOF;
        if (free < 0) free = 0;
        return { total: totalDOF, constrained: constraintsDOF, free: free };
    };

    Backend.prototype.checkConflictEquations = function(graph) {
        // Check for contradictory constraints
        var conflicts = [];
        var i, j;
        // Check betweenness contradictions
        var betweenGroups = {};
        for (i = 0; i < graph.constraints.length; i++) {
            var c = graph.constraints[i];
            if (c.constraintType === ConstraintType.BETWEENNESS) {
                var sorted = c.nodeIds.slice().sort(function(a, b) { return a - b; });
                var key = sorted.join(",");
                if (!betweenGroups[key]) betweenGroups[key] = [];
                betweenGroups[key].push(c.nodeIds[1]); // the middle point
            }
        }
        var bKeys = Object.keys(betweenGroups);
        for (i = 0; i < bKeys.length; i++) {
            var mids = betweenGroups[bKeys[i]];
            for (j = 1; j < mids.length; j++) {
                if (mids[j] !== mids[0]) {
                    conflicts.push({
                        type: "betweenness_conflict",
                        points: bKeys[i].split(","),
                        middle1: mids[0],
                        middle2: mids[j]
                    });
                }
            }
        }
        // Check incidence conflicts: a point claimed on two different lines
        // that don't share any points
        var pointLines = {};
        for (i = 0; i < graph.constraints.length; i++) {
            c = graph.constraints[i];
            if (c.constraintType === ConstraintType.INCIDENCE) {
                var ptId = c.nodeIds[0];
                var targetId = c.nodeIds[1];
                if (!pointLines[ptId]) pointLines[ptId] = [];
                pointLines[ptId].push(targetId);
            }
        }
        var plKeys = Object.keys(pointLines);
        for (i = 0; i < plKeys.length; i++) {
            var lines = pointLines[plKeys[i]];
            if (lines.length > 1) {
                // Verify the point is actually on all these lines
                for (j = 0; j < lines.length; j++) {
                    var line = this._findNodeById(graph, lines[j]);
                    if (line && line.geomType === GeomType.LINE_SEGMENT) {
                        var pt = this._findNodeById(graph, parseInt(plKeys[i], 10));
                        if (pt && pt.geomType === GeomType.POINT) {
                            var ep1 = this._findNodeById(graph, line.endpoint1Id);
                            var ep2 = this._findNodeById(graph, line.endpoint2Id);
                            if (ep1 && ep2 && pt.coordX && pt.coordY && ep1.coordX && ep1.coordY && ep2.coordX && ep2.coordY) {
                                // Check collinearity using cross product
                                var dx1 = this.coordToDouble(ep2.coordX) - this.coordToDouble(ep1.coordX);
                                var dy1 = this.coordToDouble(ep2.coordY) - this.coordToDouble(ep1.coordY);
                                var dx2 = this.coordToDouble(pt.coordX) - this.coordToDouble(ep1.coordX);
                                var dy2 = this.coordToDouble(pt.coordY) - this.coordToDouble(ep1.coordY);
                                var cross = dx1 * dy2 - dy1 * dx2;
                                if (Math.abs(cross) > 1e-9) {
                                    conflicts.push({
                                        type: "incidence_collinearity_violation",
                                        pointId: parseInt(plKeys[i], 10),
                                        lineId: lines[j]
                                    });
                                }
                            }
                        }
                    }
                }
            }
        }
        return conflicts;
    };

    // ================================================================
    // 5. 重写引擎
    // ================================================================

    Backend.prototype.rewriteRuleCreate = function(name, pattern, replacement) {
        return {
            name: name,
            pattern: pattern,      // graph structure to match
            replacement: replacement  // graph structure to replace with
        };
    };

    Backend.prototype.findRewriteMatch = function(graph, rule) {
        // Simple pattern matching: look for a subgraph in graph that matches rule.pattern
        // Pattern is an object describing what to look for
        if (!rule.pattern) return null;
        var pat = rule.pattern;
        var matches = [];
        var i, j, node;

        // Match by node types and constraints
        if (pat.nodeTypes) {
            // Find nodes matching the pattern's type requirements
            var candidates = [];
            for (i = 0; i < graph.nodes.length; i++) {
                node = graph.nodes[i];
                var match = true;
                var pKeys = Object.keys(pat.nodeTypes);
                for (j = 0; j < pKeys.length; j++) {
                    if (node.geomType !== pat.nodeTypes[pKeys[j]]) {
                        match = false;
                        break;
                    }
                }
                if (match) candidates.push(node.id);
            }
            if (candidates.length > 0) {
                matches.push({ nodeIds: candidates });
            }
        }

        // Match by constraint pattern
        if (pat.constraintTypes) {
            var cMatches = [];
            for (i = 0; i < graph.constraints.length; i++) {
                var c = graph.constraints[i];
                if (c.constraintType === pat.constraintTypes.type) {
                    cMatches.push(c.id);
                }
            }
            if (cMatches.length > 0) {
                matches.push({ constraintIds: cMatches });
            }
        }

        if (matches.length === 0) return null;
        return { matched: true, bindings: matches };
    };

    Backend.prototype.applyRewrite = function(graph, rule, match) {
        if (!match || !match.matched) return false;
        if (!rule.replacement) return false;
        var repl = rule.replacement;
        var i;

        // If replacement specifies nodes to remove
        if (repl.removeNodeIds) {
            for (i = 0; i < repl.removeNodeIds.length; i++) {
                this.graphRemoveNode(graph, repl.removeNodeIds[i]);
            }
        }
        // If replacement specifies constraints to remove
        if (repl.removeConstraintIds) {
            for (i = 0; i < repl.removeConstraintIds.length; i++) {
                this._removeConstraintById(graph, repl.removeConstraintIds[i]);
            }
        }
        // If replacement specifies nodes to add
        if (repl.addNodes) {
            for (i = 0; i < repl.addNodes.length; i++) {
                var addN = repl.addNodes[i];
                if (addN.geomType === GeomType.POINT) {
                    this.graphAddPoint(graph, addN.coordX, addN.coordY);
                } else if (addN.geomType === GeomType.LINE_SEGMENT) {
                    this.graphAddLineSegment(graph, addN.endpoint1Id, addN.endpoint2Id);
                }
            }
        }
        // If replacement specifies constraints to add
        if (repl.addConstraints) {
            for (i = 0; i < repl.addConstraints.length; i++) {
                var addC = repl.addConstraints[i];
                if (addC.constraintType === ConstraintType.INCIDENCE) {
                    this.graphAddIncidence(graph, addC.nodeIds[0], addC.nodeIds[1]);
                } else if (addC.constraintType === ConstraintType.BETWEENNESS) {
                    this.graphAddBetweenness(graph, addC.nodeIds[0], addC.nodeIds[1], addC.nodeIds[2]);
                } else if (addC.constraintType === ConstraintType.INTERSECTION) {
                    this.graphAddIntersection(graph, addC.nodeIds[0], addC.nodeIds[1], addC.nodeIds[2]);
                } else if (addC.constraintType === ConstraintType.CONTAINMENT) {
                    this.graphAddContainment(graph, addC.nodeIds[0], addC.nodeIds[1]);
                } else if (addC.constraintType === ConstraintType.CONNECTION) {
                    this.graphAddConnection(graph, addC.nodeIds[0], addC.nodeIds[1]);
                }
            }
        }
        return true;
    };

    Backend.prototype.rewriteWithRules = function(graph, rules, stepLimit) {
        this._counters.rewrites++;
        if (!stepLimit) stepLimit = 100;
        var totalSteps = 0;
        var i;
        for (i = 0; i < rules.length && totalSteps < stepLimit; i++) {
            var rule = rules[i];
            var match = this.findRewriteMatch(graph, rule);
            while (match && totalSteps < stepLimit) {
                this.applyRewrite(graph, rule, match);
                totalSteps++;
                match = this.findRewriteMatch(graph, rule);
            }
        }
        return { stepsApplied: totalSteps, limitReached: totalSteps >= stepLimit };
    };

    // ================================================================
    // 6. 统一化检查
    // ================================================================

    Backend.prototype.unifyConstructionWithProposition = function(construction, proposition) {
        this._counters.unifications++;
        // construction and proposition are both graphs
        // Unification: find a mapping from proposition variables to construction nodes
        var mapping = {};
        var i, j;

        // Match points by position
        var propPoints = [];
        var constPoints = [];
        for (i = 0; i < proposition.nodes.length; i++) {
            if (proposition.nodes[i].geomType === GeomType.POINT) propPoints.push(proposition.nodes[i]);
        }
        for (i = 0; i < construction.nodes.length; i++) {
            if (construction.nodes[i].geomType === GeomType.POINT) constPoints.push(construction.nodes[i]);
        }

        if (propPoints.length > constPoints.length) {
            return { unified: false, reason: "not_enough_points" };
        }

        // Try to match proposition points to construction points by coordinates
        var usedConst = {};
        for (i = 0; i < propPoints.length; i++) {
            var pp = propPoints[i];
            var found = false;
            for (j = 0; j < constPoints.length; j++) {
                if (usedConst[j]) continue;
                var cp = constPoints[j];
                if (this._coordsEqual(pp.coordX, cp.coordX) && this._coordsEqual(pp.coordY, cp.coordY)) {
                    mapping[pp.id] = cp.id;
                    usedConst[j] = true;
                    found = true;
                    break;
                }
            }
            if (!found) {
                return { unified: false, reason: "point_not_found", propositionPointId: pp.id };
            }
        }

        // Verify constraints match under the mapping
        for (i = 0; i < proposition.constraints.length; i++) {
            var pc = proposition.constraints[i];
            var mappedNodeIds = [];
            for (j = 0; j < pc.nodeIds.length; j++) {
                if (mapping[pc.nodeIds[j]] === undefined) {
                    return { unified: false, reason: "unmapped_node", nodeId: pc.nodeIds[j] };
                }
                mappedNodeIds.push(mapping[pc.nodeIds[j]]);
            }
            // Check if this constraint exists in construction
            var foundConstraint = false;
            for (j = 0; j < construction.constraints.length; j++) {
                var cc = construction.constraints[j];
                if (cc.constraintType !== pc.constraintType) continue;
                if (cc.nodeIds.length !== mappedNodeIds.length) continue;
                var same = true;
                for (var k = 0; k < cc.nodeIds.length; k++) {
                    if (cc.nodeIds[k] !== mappedNodeIds[k]) { same = false; break; }
                }
                if (same) { foundConstraint = true; break; }
            }
            if (!foundConstraint) {
                return { unified: false, reason: "constraint_not_found", constraintType: pc.constraintType };
            }
        }

        return { unified: true, mapping: mapping };
    };

    // ================================================================
    // 7. 函数块系统
    // ================================================================

    Backend.prototype.funcBlockCreate = function(id) {
        return {
            id: id,
            internals: [],
            inputs: [],
            outputs: [],
            packed: false
        };
    };

    Backend.prototype.funcBlockDestroy = function(fb) {
        if (fb) {
            fb.internals = null;
            fb.inputs = null;
            fb.outputs = null;
        }
    };

    Backend.prototype.funcBlockPack = function(graph, internals, inputs, outputs) {
        var fb = this.funcBlockCreate(graph.nextNodeId);
        var i;
        for (i = 0; i < internals.length; i++) fb.internals.push(internals[i]);
        for (i = 0; i < inputs.length; i++) fb.inputs.push(inputs[i]);
        for (i = 0; i < outputs.length; i++) fb.outputs.push(outputs[i]);
        fb.packed = true;
        return fb;
    };

    Backend.prototype.funcBlockInstantiate = function(fb, graph, argMappings) {
        // argMappings: { inputNodeId: actualNodeId }
        if (!fb.packed) return -1;

        // Create new nodes for internals
        var idMap = {};
        var i;
        for (i = 0; i < fb.internals.length; i++) {
            var oldId = fb.internals[i];
            var oldNode = this._findNodeById(graph, oldId);
            if (oldNode) {
                var newId = graph.nextNodeId++;
                var newNode = {};
                var keys = Object.keys(oldNode);
                for (var k = 0; k < keys.length; k++) {
                    var key = keys[k];
                    if (key === 'id') {
                        newNode[key] = newId;
                    } else if (key === 'coordX' || key === 'coordY') {
                        newNode[key] = this._cloneCoord(oldNode[key]);
                    } else if (key === 'segmentIds' || key === 'internals' || key === 'inputs' || key === 'outputs') {
                        newNode[key] = oldNode[key].slice();
                    } else {
                        newNode[key] = oldNode[key];
                    }
                }
                graph.nodes.push(newNode);
                graph._nodeMap[newId] = newNode; // 同步更新索引
                idMap[oldId] = newId;
            }
        }

        // Map inputs to actual arguments
        for (i = 0; i < fb.inputs.length; i++) {
            var inputId = fb.inputs[i];
            if (argMappings[inputId] !== undefined) {
                idMap[inputId] = argMappings[inputId];
            }
        }

        // Create output nodes
        var outputIds = [];
        for (i = 0; i < fb.outputs.length; i++) {
            var outId = fb.outputs[i];
            if (idMap[outId] !== undefined) {
                outputIds.push(idMap[outId]);
            } else {
                var newOutId = graph.nextNodeId++;
                var outNode = { id: newOutId, geomType: GeomType.POINT, coordX: this.coordCreateRational(0, 1), coordY: this.coordCreateRational(0, 1) };
                graph.nodes.push(outNode);
                graph._nodeMap[newOutId] = outNode; // 同步更新索引
                idMap[outId] = newOutId;
                outputIds.push(newOutId);
            }
        }

        // Create the function block node
        var blockId = graph.nextNodeId++;
        var blockNode = {
            id: blockId,
            geomType: GeomType.FUNCTION_BLOCK,
            internals: fb.internals.map(function(id) { return idMap[id] || id; }),
            inputs: fb.inputs.map(function(id) { return idMap[id] || id; }),
            outputs: outputIds
        };
        graph.nodes.push(blockNode);
        graph._nodeMap[blockId] = blockNode; // 同步更新索引

        return blockId;
    };

    Backend.prototype.funcBlockCheckDeterminism = function(fb, graph) {
        // A function block is deterministic if its outputs are fully determined by inputs
        // Simple check: count DOF of internal graph
        var internalNodes = [];
        var internalConstraints = [];
        var i, j;
        for (i = 0; i < fb.internals.length; i++) {
            var node = this._findNodeById(graph, fb.internals[i]);
            if (node) internalNodes.push(node);
        }
        // Find constraints that only involve internal nodes
        for (i = 0; i < graph.constraints.length; i++) {
            var c = graph.constraints[i];
            var allInternal = true;
            for (j = 0; j < c.nodeIds.length; j++) {
                var found = false;
                for (var k = 0; k < fb.internals.length; k++) {
                    if (fb.internals[k] === c.nodeIds[j]) { found = true; break; }
                }
                if (!found) { allInternal = false; break; }
            }
            if (allInternal) internalConstraints.push(c);
        }
        var totalDOF = 0;
        var constraintsDOF = 0;
        for (i = 0; i < internalNodes.length; i++) {
            if (internalNodes[i].geomType === GeomType.POINT) totalDOF += 2;
        }
        for (i = 0; i < internalConstraints.length; i++) {
            var ct = internalConstraints[i].constraintType;
            if (ct === ConstraintType.INTERSECTION) constraintsDOF += 2;
            else constraintsDOF += 1;
        }
        var inputDOF = fb.inputs.length * 2;
        var remaining = totalDOF - constraintsDOF - inputDOF;
        return { deterministic: remaining <= 0, freeVariables: remaining > 0 ? remaining : 0 };
    };

    Backend.prototype.funcBlockCompose = function(f, g, graph) {
        // Compose f and g: g's outputs feed into f's inputs
        // Create a new function block that represents f(g(x))
        var composed = this.funcBlockCreate(f.id + "_compose_" + g.id);
        composed.internals = g.internals.concat(f.internals);
        composed.inputs = g.inputs;
        composed.outputs = f.outputs;
        composed.packed = true;
        return composed;
    };

    Backend.prototype.funcBlockProduct = function(f, g, graph) {
        // Product: (f, g) applied to (x, y) -> (f(x), g(y))
        var product = this.funcBlockCreate(f.id + "_product_" + g.id);
        product.internals = f.internals.concat(g.internals);
        product.inputs = f.inputs.concat(g.inputs);
        product.outputs = f.outputs.concat(g.outputs);
        product.packed = true;
        return product;
    };

    // ================================================================
    // 8. 类型系统
    // ================================================================

    Backend.prototype.typeSystemCreate = function() {
        return {
            types: {},
            nextTypeId: 0
        };
    };

    Backend.prototype.typeCreatePoint = function(ts) {
        var id = ts.nextTypeId++;
        ts.types[id] = { id: id, kind: "point" };
        return id;
    };

    Backend.prototype.typeCreateLineSegment = function(ts) {
        var id = ts.nextTypeId++;
        ts.types[id] = { id: id, kind: "line_segment" };
        return id;
    };

    Backend.prototype.typeCreateRegion = function(ts, containedIds) {
        var id = ts.nextTypeId++;
        var copy = [];
        var i;
        for (i = 0; i < containedIds.length; i++) copy.push(containedIds[i]);
        ts.types[id] = { id: id, kind: "region", containedIds: copy };
        return id;
    };

    Backend.prototype.typeCreateFunction = function(ts, input, output) {
        var id = ts.nextTypeId++;
        ts.types[id] = { id: id, kind: "function", input: input, output: output };
        return id;
    };

    Backend.prototype.typeCreateProduct = function(ts, left, right) {
        var id = ts.nextTypeId++;
        ts.types[id] = { id: id, kind: "product", left: left, right: right };
        return id;
    };

    Backend.prototype.typeCheckEquivalence = function(ts, t1, t2) {
        if (t1 === t2) return true;
        var type1 = ts.types[t1];
        var type2 = ts.types[t2];
        if (!type1 || !type2) return false;
        if (type1.kind !== type2.kind) return false;
        if (type1.kind === "function") {
            return this.typeCheckEquivalence(ts, type1.input, type2.input) &&
                   this.typeCheckEquivalence(ts, type1.output, type2.output);
        }
        if (type1.kind === "product") {
            return this.typeCheckEquivalence(ts, type1.left, type2.left) &&
                   this.typeCheckEquivalence(ts, type1.right, type2.right);
        }
        if (type1.kind === "region") {
            if (type1.containedIds.length !== type2.containedIds.length) return false;
            for (var i = 0; i < type1.containedIds.length; i++) {
                if (type1.containedIds[i] !== type2.containedIds[i]) return false;
            }
            return true;
        }
        return true;
    };

    Backend.prototype.typeInferNode = function(ts, graph, nodeId) {
        var node = this._findNodeById(graph, nodeId);
        if (!node) return -1;
        if (node.geomType === GeomType.POINT) return this.typeCreatePoint(ts);
        if (node.geomType === GeomType.LINE_SEGMENT) return this.typeCreateLineSegment(ts);
        if (node.geomType === GeomType.REGION) return this.typeCreateRegion(ts, node.segmentIds || []);
        if (node.geomType === GeomType.PORT) {
            // Port type depends on parent block
            if (node.parentBlockId >= 0) {
                var parent = this._findNodeById(graph, node.parentBlockId);
                if (parent && parent.geomType === GeomType.FUNCTION_BLOCK) {
                    var inputType = this.typeCreatePoint(ts);
                    var outputType = this.typeCreatePoint(ts);
                    return this.typeCreateFunction(ts, inputType, outputType);
                }
            }
            return this.typeCreatePoint(ts);
        }
        if (node.geomType === GeomType.FUNCTION_BLOCK) {
            var inType = this.typeCreatePoint(ts);
            var outType = this.typeCreatePoint(ts);
            return this.typeCreateFunction(ts, inType, outType);
        }
        return -1;
    };

    Backend.prototype.typeCheckLevelValidity = function(ts, container, contained) {
        // Check if contained type is valid inside container type
        var cType = ts.types[container];
        var kType = ts.types[contained];
        if (!cType || !kType) return false;
        // Points can be in regions
        if (cType.kind === "region" && kType.kind === "point") return true;
        // Line segments can be in regions
        if (cType.kind === "region" && kType.kind === "line_segment") return true;
        // Points can be on line segments
        if (cType.kind === "line_segment" && kType.kind === "point") return true;
        // Regions can contain regions (nesting)
        if (cType.kind === "region" && kType.kind === "region") return true;
        // Function blocks can contain points
        if (cType.kind === "function" && kType.kind === "point") return true;
        return false;
    };

    // ================================================================
    // 9. 证明系统
    // ================================================================

    Backend.prototype.propositionCreate = function(id, type) {
        return {
            id: id,
            type: type || "theorem",
            pattern: null,
            hypotheses: [],
            conclusion: null
        };
    };

    Backend.prototype.propositionDestroy = function(prop) {
        if (prop) {
            prop.pattern = null;
            prop.hypotheses = null;
            prop.conclusion = null;
        }
    };

    Backend.prototype.propositionSetPattern = function(prop, graph) {
        prop.pattern = this._deepCloneGraph(graph);
    };

    Backend.prototype.proofUnify = function(construction, proposition) {
        return this.unifyConstructionWithProposition(construction, proposition.pattern || proposition);
    };

    Backend.prototype.proofNavigatorCreate = function(target) {
        return {
            target: target,
            steps: [],
            currentIndex: -1
        };
    };

    Backend.prototype.proofNavigatorAddStep = function(nav, step) {
        nav.steps.push(step);
        if (nav.currentIndex < 0) nav.currentIndex = 0;
    };

    Backend.prototype.proofNavigatorNext = function(nav) {
        if (nav.currentIndex < nav.steps.length - 1) {
            nav.currentIndex++;
            return nav.steps[nav.currentIndex];
        }
        return null;
    };

    Backend.prototype.proofNavigatorPrev = function(nav) {
        if (nav.currentIndex > 0) {
            nav.currentIndex--;
            return nav.steps[nav.currentIndex];
        }
        return null;
    };

    Backend.prototype.proofNavigatorCurrentStep = function(nav) {
        if (nav.currentIndex >= 0 && nav.currentIndex < nav.steps.length) {
            return nav.steps[nav.currentIndex];
        }
        return null;
    };

    // HTML 转义辅助函数，防止 XSS 注入
    Backend.prototype._escapeHtml = function(str) {
        if (typeof str !== 'string') return String(str);
        return str
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#39;');
    };

    Backend.prototype.proofExportHTML = function(nav) {
        var parts = [];
        parts.push("<div class=\"proof\">");
        parts.push("<h3>Proof: " + this._escapeHtml(nav.target || "Untitled") + "</h3>");
        parts.push("<ol>");
        var i;
        for (i = 0; i < nav.steps.length; i++) {
            var step = nav.steps[i];
            if (typeof step === "string") {
                parts.push("<li>" + this._escapeHtml(step) + "</li>");
            } else if (step && step.description) {
                parts.push("<li>" + this._escapeHtml(step.description) + "</li>");
            } else if (step && step.type) {
                parts.push("<li>[" + this._escapeHtml(step.type) + "] " + this._escapeHtml(step.detail || "") + "</li>");
            } else {
                parts.push("<li>" + this._escapeHtml(JSON.stringify(step)) + "</li>");
            }
        }
        parts.push("</ol>");
        parts.push("</div>");
        return parts.join("\n");
    };

    Backend.prototype.proofExportLaTeX = function(nav) {
        var parts = [];
        parts.push("\\begin{proof}[" + (nav.target || "Untitled") + "]");
        var i;
        for (i = 0; i < nav.steps.length; i++) {
            var step = nav.steps[i];
            if (typeof step === "string") {
                parts.push("  " + step + "\\\\");
            } else if (step && step.description) {
                parts.push("  " + step.description + "\\\\");
            } else if (step && step.type) {
                parts.push("  \\textbf{" + step.type + "}: " + (step.detail || "") + "\\\\");
            } else {
                parts.push("  " + JSON.stringify(step) + "\\\\");
            }
        }
        parts.push("\\end{proof}");
        return parts.join("\n");
    };

    // ================================================================
    // 10. 递归系统
    // ================================================================

    Backend.prototype.measureSystemCreate = function() {
        return {
            measures: [],
            nextMeasureId: 0
        };
    };

    Backend.prototype.measureCreateSymbolic = function(ms, name, kind, refNodeId) {
        var id = ms.nextMeasureId++;
        var measure = {
            id: id,
            name: name,
            kind: kind || "length", // "length", "angle", "area"
            refNodeId: refNodeId !== undefined ? refNodeId : -1
        };
        ms.measures.push(measure);
        return id;
    };

    Backend.prototype.measureComputeValue = function(ms, measureId, node, graph) {
        var i;
        for (i = 0; i < ms.measures.length; i++) {
            if (ms.measures[i].id === measureId) {
                var measure = ms.measures[i];
                if (measure.kind === "length" && node.geomType === GeomType.LINE_SEGMENT) {
                    var p1 = this._findNodeById(graph, node.endpoint1Id);
                    var p2 = this._findNodeById(graph, node.endpoint2Id);
                    if (p1 && p2 && p1.coordX && p1.coordY && p2.coordX && p2.coordY) {
                        var dx = this.coordSubtract(p2.coordX, p1.coordX);
                        var dy = this.coordSubtract(p2.coordY, p1.coordY);
                        var dx2 = this.coordMultiply(dx, dx);
                        var dy2 = this.coordMultiply(dy, dy);
                        var sum = this.coordAdd(dx2, dy2);
                        var d = this.coordToDouble(sum);
                        return Math.sqrt(d);
                    }
                }
                if (measure.kind === "area" && node.geomType === GeomType.REGION) {
                    // Approximate area using shoelace formula
                    var region = node;
                    if (region.segmentIds && region.segmentIds.length > 0) {
                        var points = [];
                        var firstSeg = this._findNodeById(graph, region.segmentIds[0]);
                        if (firstSeg) {
                            points.push(this._findNodeById(graph, firstSeg.endpoint1Id));
                        }
                        for (var j = 0; j < region.segmentIds.length; j++) {
                            var seg = this._findNodeById(graph, region.segmentIds[j]);
                            if (seg) {
                                points.push(this._findNodeById(graph, seg.endpoint2Id));
                            }
                        }
                        var area = 0;
                        for (var k = 0; k < points.length; k++) {
                            var curr = points[k];
                            var next = points[(k + 1) % points.length];
                            if (curr && next && curr.coordX && curr.coordY && next.coordX && next.coordY) {
                                area += this.coordToDouble(curr.coordX) * this.coordToDouble(next.coordY);
                                area -= this.coordToDouble(next.coordX) * this.coordToDouble(curr.coordY);
                            }
                        }
                        return Math.abs(area) / 2;
                    }
                }
                return 0;
            }
        }
        return 0;
    };

    Backend.prototype.measureCompare = function(ms, measureId, a, b) {
        var va = this.measureComputeValue(ms, measureId, a, null);
        var vb = this.measureComputeValue(ms, measureId, b, null);
        if (va < vb) return -1;
        if (va > vb) return 1;
        return 0;
    };

    Backend.prototype.recursionContextCreate = function(maxDepth) {
        return {
            maxDepth: maxDepth || 10,
            currentDepth: 0,
            stack: []
        };
    };

    Backend.prototype.recursionContextEnter = function(ctx, fbId, input, graph) {
        if (ctx.currentDepth >= ctx.maxDepth) {
            return false;
        }
        ctx.stack.push({
            fbId: fbId,
            input: input,
            depth: ctx.currentDepth
        });
        ctx.currentDepth++;
        return true;
    };

    Backend.prototype.recursionContextExit = function(ctx) {
        if (ctx.stack.length > 0) {
            ctx.stack.pop();
            ctx.currentDepth--;
            return true;
        }
        return false;
    };

    Backend.prototype.selectorBlockCreate = function(id, graph) {
        return {
            id: id,
            cases: [],
            defaultCase: null
        };
    };

    Backend.prototype.selectorBlockEvaluate = function(sb, graph) {
        // Evaluate selector block: find the first matching case
        var i;
        for (i = 0; i < sb.cases.length; i++) {
            var c = sb.cases[i];
            if (c.condition) {
                // Simple condition evaluation
                if (c.condition(graph)) {
                    return { matched: true, caseIndex: i, result: c.result };
                }
            }
        }
        if (sb.defaultCase) {
            return { matched: false, caseIndex: -1, result: sb.defaultCase };
        }
        return { matched: false, caseIndex: -1, result: null };
    };

    // ================================================================
    // 11. 公理包
    // ================================================================

    Backend.prototype.axiomPackageCreate = function(name, version) {
        return {
            name: name,
            version: version || "1.0.0",
            unconstructibles: [],
            templates: []
        };
    };

    Backend.prototype.axiomPackageAddUnconstructible = function(pkg, item) {
        pkg.unconstructibles.push(item);
    };

    Backend.prototype.axiomPackageRegisterTemplate = function(pkg, tmpl) {
        pkg.templates.push(tmpl);
    };

    // ================================================================
    // 12. 模块系统
    // ================================================================

    Backend.prototype.moduleCreate = function(name, version) {
        return {
            name: name,
            version: version || "1.0.0",
            dependencies: [],
            exports: [],
            functionBlocks: {}
        };
    };

    Backend.prototype.moduleAddDependency = function(mod, depName, versionConstraint) {
        mod.dependencies.push({
            name: depName,
            versionConstraint: versionConstraint || "*"
        });
    };

    Backend.prototype.moduleExportFunctionBlock = function(mod, fbId) {
        mod.exports.push(fbId);
        mod.functionBlocks[fbId] = true;
    };

    Backend.prototype.moduleDetectCircularDependency = function(modules) {
        // modules is an array of module objects
        // Detect circular dependencies using DFS
        var visited = {};
        var recStack = {};
        var cycles = [];
        var i;

        function visit(modName, path) {
            visited[modName] = true;
            recStack[modName] = true;
            path.push(modName);

            var mod = null;
            for (i = 0; i < modules.length; i++) {
                if (modules[i].name === modName) { mod = modules[i]; break; }
            }
            if (mod) {
                for (var j = 0; j < mod.dependencies.length; j++) {
                    var dep = mod.dependencies[j].name;
                    if (!visited[dep]) {
                        var result = visit(dep, path);
                        if (result) return result;
                    } else if (recStack[dep]) {
                        // Found cycle
                        var cycleStart = path.indexOf(dep);
                        var cycle = path.slice(cycleStart).concat(dep);
                        return cycle;
                    }
                }
            }

            path.pop();
            recStack[modName] = false;
            return null;
        }

        for (i = 0; i < modules.length; i++) {
            if (!visited[modules[i].name]) {
                var result = visit(modules[i].name, []);
                if (result) {
                    cycles.push(result);
                }
            }
        }
        return cycles;
    };

    // ================================================================
    // 13. 引擎
    // ================================================================

    /**
     * 引擎求解器创建
     * 创建一个完整的求解引擎实例，集成了约束图、重写规则、
     * 函数块字典、类型系统和测度系统等核心子系统。
     * 引擎是执行代数求解、重写和统一化操作的核心对象。
     *
     * @returns {Object} 引擎实例对象
     */
    Backend.prototype.engineCreate = function() {
        return {
            graph: this.graphCreate(),
            rewriteRules: [],
            functionBlocks: {},
            typeSystem: this.typeSystemCreate(),
            measureSystem: this.measureSystemCreate(),
            axiomPackages: [],
            modules: []
        };
    };

    Backend.prototype.engineDestroy = function(engine) {
        if (engine) {
            this.graphDestroy(engine.graph);
            engine.rewriteRules = null;
            engine.functionBlocks = null;
            engine.typeSystem = null;
            engine.measureSystem = null;
            engine.axiomPackages = null;
            engine.modules = null;
        }
    };

    Backend.prototype.engineAddRewriteRule = function(engine, rule) {
        engine.rewriteRules.push(rule);
    };

    /**
     * 函数块打包（引擎方法）
     * 将一组内部节点、输入端口和输出端口打包为一个函数块（Function Block）。
     * 生成的函数块注册到引擎的 functionBlocks 字典中，供后续例化和组合使用。
     *
     * @param {Object} engine - 引擎实例
     * @param {Array} internals - 内部节点 ID 列表
     * @param {Array} inputs - 输入端口节点 ID 列表
     * @param {Array} outputs - 输出端口节点 ID 列表
     * @returns {string|number} 新创建的函数块 ID
     */
    Backend.prototype.enginePackFunction = function(engine, internals, inputs, outputs) {
        var fb = this.funcBlockPack(engine.graph, internals, inputs, outputs);
        engine.functionBlocks[fb.id] = fb;
        return fb.id;
    };

    Backend.prototype.engineInstantiateFunction = function(engine, fbId, argMappings) {
        var fb = engine.functionBlocks[fbId];
        if (!fb) return -1;
        return this.funcBlockInstantiate(fb, engine.graph, argMappings);
    };

    Backend.prototype.engineUnify = function(engine, construction, proposition) {
        return this.unifyConstructionWithProposition(construction, proposition);
    };

    // ================================================================
    // 14. 调试系统
    // ================================================================

    Backend.prototype.debugGetCounters = function() {
        var copy = {};
        var keys = Object.keys(this._counters);
        for (var i = 0; i < keys.length; i++) {
            copy[keys[i]] = this._counters[keys[i]];
        }
        return copy;
    };

    Backend.prototype.debugResetCounters = function() {
        var keys = Object.keys(this._counters);
        for (var i = 0; i < keys.length; i++) {
            this._counters[keys[i]] = 0;
        }
    };

    Backend.prototype.debugCountersReport = function() {
        var lines = [];
        lines.push("Lv-00 Backend Debug Counters:");
        lines.push("  Version: " + this.version);
        var keys = Object.keys(this._counters);
        for (var i = 0; i < keys.length; i++) {
            lines.push("  " + keys[i] + ": " + this._counters[keys[i]]);
        }
        return lines.join("\n");
    };

    Backend.prototype.getVersion = function() {
        return this.version;
    };

    // ================================================================
    // 暴露常量到外部接口
    // ================================================================

    Backend.CoordType = CoordType;
    Backend.TrustColor = TrustColor;
    Backend.GeomType = GeomType;
    Backend.ConstraintType = ConstraintType;

    return Backend;
})();

window.Lv00JSBackend = Lv00JSBackend;
