/**
 * graph_to_formula.js
 *
 * Lv-00 几何元语言系统 —— 「图形→公式」转换器
 *
 * 从约束图（Constraint Graph）状态生成数学公式（LaTeX 格式）。
 * 遍历图中的节点（点、线段、区域等）与约束（关联、介于、交点等），
 * 将几何关系翻译为可读的 LaTeX 字符串。
 *
 * 严格使用 ES5 语法（var / function），无 class、const、let、箭头函数、
 * 模板字符串、解构、默认参数、展开运算符。
 *
 * 依赖：无外部依赖，仅依赖 lv00_js_backend.js 中定义的图数据结构。
 *
 * API 入口：
 *   GraphToFormula.convert(graph)            — 整图转换
 *   GraphToFormula.convertNode(graph, id)    — 单节点转换
 *   GraphToFormula.convertConstraint(graph, id) — 单约束转换
 */

var GraphToFormula = (function () {
    'use strict';

    // ================================================================
    //  常量 / 枚举
    // ================================================================

    /** SymbolicCoord.coordType */
    var COORD_RATIONAL       = 0;
    var COORD_ALGEBRAIC      = 1;
    var COORD_QUADRATIC      = 2;
    var COORD_TRANSCENDENTAL = 3;

    /** Constraint.type */
    var CT_INCIDENCE    = 0;  // 点在线段上
    var CT_BETWEENNESS  = 1;  // 介于关系
    var CT_INTERSECTION = 2;  // 交点
    var CT_CONTAINMENT  = 3;  // 包含关系
    var CT_CONNECTION   = 4;  // 连接关系

    /**
     * 几何节点类型（字符串常量）
     * 注意：前端使用字符串类型（node.type），后端 lv00_js_backend.js 使用数字枚举（node.geomType）。
     * 两者映射关系：
     *   'point'          <-> GeomType.POINT: 0
     *   'line_segment'   <-> GeomType.LINE_SEGMENT: 1
     *   'region'         <-> GeomType.REGION: 2
     *   'port'           <-> GeomType.PORT: 3
     *   'function_block' <-> GeomType.FUNCTION_BLOCK: 4
     * 跨模块使用时优先检查 geomType（数字），回退到 type（字符串）。
     */
    /** GeomNode.type */
    var NT_POINT          = 'point';
    var NT_LINE_SEGMENT   = 'line_segment';
    var NT_REGION         = 'region';
    var NT_PORT           = 'port';
    var NT_FUNCTION_BLOCK = 'function_block';

    /**
     * 数字枚举到字符串节点类型的映射（模块级常量）
     * 与 lv00_js_backend.js 中 GeomType 枚举对应，用于 _isNodeType 的 geomType 路径。
     * 提取为模块级常量，避免 _isNodeType 每次调用时重新创建该映射对象。
     *   0 (POINT)          -> 'point'
     *   1 (LINE_SEGMENT)   -> 'line_segment'
     *   2 (REGION)         -> 'region'
     *   3 (PORT)           -> 'port'
     *   4 (FUNCTION_BLOCK) -> 'function_block'
     */
    var GEOMTYPE_TO_STR = {
        0: 'point',
        1: 'line_segment',
        2: 'region',
        3: 'port',
        4: 'function_block'
    };

    // ================================================================
    //  内部辅助函数
    // ================================================================

    /**
     * 构建节点 ID 到节点对象的索引映射
     * @param {Object} graph - 约束图对象
     * @returns {Object} ID 到节点的映射
     */
    function _buildNodeIndex(graph) {
        var index = {};
        if (graph && graph.nodes) {
            for (var i = 0; i < graph.nodes.length; i++) {
                index[graph.nodes[i].id] = graph.nodes[i];
            }
        }
        return index;
    }

    /**
     * 构建约束 ID 到约束对象的索引映射
     * @param {Object} graph - 约束图对象
     * @returns {Object} ID 到约束的映射
     */
    function _buildConstraintIndex(graph) {
        var index = {};
        if (graph && graph.constraints) {
            for (var i = 0; i < graph.constraints.length; i++) {
                index[graph.constraints[i].id] = graph.constraints[i];
            }
        }
        return index;
    }

    /**
     * 通过 id 在图中查找节点
     * 优先使用索引缓存（如果可用），回退到线性遍历。
     *
     * @param {object} graph - 约束图对象，包含 nodes 数组
     * @param {number} nodeId - 要查找的节点 ID
     * @returns {object|null} 匹配的节点对象，未找到返回 null
     */
    function findNode(graph, nodeId) {
        if (!graph || !graph.nodes) return null;
        // 优先使用索引缓存（如果可用）
        if (graph._nodeIndex) {
            return graph._nodeIndex[nodeId] || null;
        }
        // 回退到线性搜索
        var i;
        for (i = 0; i < graph.nodes.length; i++) {
            if (graph.nodes[i].id === nodeId) {
                return graph.nodes[i];
            }
        }
        return null;
    }

    /**
     * 通过 id 在图中查找约束
     * 优先使用索引缓存（如果可用），回退到线性遍历。
     *
     * @param {object} graph - 约束图对象，包含 constraints 数组
     * @param {number} constraintId - 要查找的约束 ID
     * @returns {object|null} 匹配的约束对象，未找到返回 null
     */
    function findConstraint(graph, constraintId) {
        if (!graph || !graph.constraints) return null;
        // 优先使用索引缓存（如果可用）
        if (graph._constraintIndex) {
            return graph._constraintIndex[constraintId] || null;
        }
        // 回退到线性搜索
        var i;
        for (i = 0; i < graph.constraints.length; i++) {
            if (graph.constraints[i].id === constraintId) {
                return graph.constraints[i];
            }
        }
        return null;
    }

    /**
     * 获取图中所有指定类型的节点
     * 遍历节点列表，筛选出所有类型匹配的节点。
     * 用于按节点类型聚合（如获取所有 POINT、LINE_SEGMENT 节点）。
     *
     * @param {object} graph - 约束图对象
     * @param {string} nodeType - 节点类型字符串（如 'point', 'line_segment'）
     * @returns {Array} 匹配类型的所有节点对象数组
     */
    function getNodesByType(graph, nodeType) {
        var result = [];
        var i;
        for (i = 0; i < graph.nodes.length; i++) {
            // 使用统一类型判断函数，兼容 node.type（字符串）和 node.geomType（数字枚举）双路径
            if (_isNodeType(graph.nodes[i], nodeType)) {
                result.push(graph.nodes[i]);
            }
        }
        return result;
    }

    /**
     * 节点类型判断辅助函数（统一处理 geomType/type 双路径）
     * 优先检查 node.type（字符串），回退到 node.geomType（数字枚举）。
     * 解决前端 graph_to_formula.js 使用字符串类型而后端 lv00_js_backend.js 使用数字枚举的数据模型分歧。
     *
     * @param {object} node - 图节点对象
     * @param {string} stringType - 字符串类型（如 'point', 'line_segment'）
     * @returns {boolean} 节点是否匹配指定类型
     */
    function _isNodeType(node, stringType) {
        if (!node) return false;
        if (node.type !== undefined) {
            return node.type === stringType;
        }
        if (node.geomType !== undefined) {
            // 使用模块级常量 GEOMTYPE_TO_STR 替代每次调用时创建的新映射对象
            var mapped = GEOMTYPE_TO_STR[node.geomType];
            return mapped === stringType;
        }
        return false;
    }

    /**
     * 获取参与指定节点的所有约束
     * @param {object} graph
     * @param {number} nodeId
     * @returns {Array}
     */
    function getConstraintsForNode(graph, nodeId) {
        var result = [];
        var i, j;
        for (i = 0; i < graph.constraints.length; i++) {
            var c = graph.constraints[i];
            for (j = 0; j < c.participant_count; j++) {
                if (c.participants[j] === nodeId) {
                    result.push(c);
                    break;
                }
            }
        }
        return result;
    }

    /**
     * 整数转字符串，处理负号
     * @param {number} n
     * @returns {string}
     */
    function intStr(n) {
        if (n < 0) {
            return '-' + String(-n);
        }
        return String(n);
    }

    /**
     * 有理数转 LaTeX 字符串
     * 将分子/分母对转换为 LaTeX 数学格式：
     *   - den == 1 时直接输出整数（如 "5"）
     *   - den != 1 时输出分数形式 \frac{num}{den}
     *
     * @param {number} num - 分子
     * @param {number} den - 分母
     * @returns {string} LaTeX 格式的有理数字符串
     */
    function rationalToLatex(num, den) {
        if (den === 1) {
            return intStr(num);
        }
        return '\\frac{' + intStr(num) + '}{' + intStr(den) + '}';
    }

    /**
     * 判断有理数是否为 0
     */
    function rationalIsZero(num, den) {
        return num === 0;
    }

    /**
     * 判断有理数是否为 1
     */
    function rationalIsOne(num, den) {
        return num === 1 && den === 1;
    }

    /**
     * 判断有理数是否为 -1
     */
    function rationalIsMinusOne(num, den) {
        return num === -1 && den === 1;
    }

    /**
     * 判断有理数是否为正
     */
    function rationalIsPositive(num, den) {
        return num * den > 0;
    }

    /**
     * 有理数转 LaTeX（用于二次根式的系数部分）
     * 当系数为 1 时返回空串，为 -1 时返回 "-"
     */
    function coeffToLatex(num, den) {
        if (rationalIsOne(num, den)) {
            return '';
        }
        if (rationalIsMinusOne(num, den)) {
            return '-';
        }
        return rationalToLatex(num, den);
    }

    // ================================================================
    //  坐标转 LaTeX
    // ================================================================

    /**
     * 将 SymbolicCoord 转为 LaTeX 字符串
     *
     * 规则：
     *   RATIONAL(num/den):       den==1 → "num"；否则 → \frac{num}{den}
     *   QUADRATIC(a+b*sqrt(n)):  按 a, b 的各种情况处理
     *   TRANSCENDENTAL(name):    "pi" → \pi；"e" → e
     *
     * @param {object} coord - SymbolicCoord 对象
     * @returns {string} LaTeX 字符串
     */
    function coordToLatex(coord) {
        if (!coord) {
            return '0';
        }

        switch (coord.coordType) {
            case COORD_RATIONAL:
                return rationalToLatex(coord.num, coord.den);

            case COORD_QUADRATIC:
                return quadraticToLatex(coord);

            case COORD_TRANSCENDENTAL:
                return transcendentalToLatex(coord);

            case COORD_ALGEBRAIC:
                // 代数数通用表示：用名称或标记
                return '\\alpha';

            default:
                return '?';
        }
    }

    /**
     * 二次根式坐标转 LaTeX 字符串
     * coord = { aNum, aDen, bNum, bDen, n }
     * 表示 a + b*sqrt(n)，其中 a = aNum/aDen, b = bNum/bDen。
     *
     * 输出规则处理所有特殊情况：
     *   - b == 0: 仅输出 a 部分
     *   - a == 0, b == 1: 输出 sqrt(n)
     *   - a == 0, b == -1: 输出 -sqrt(n)
     *   - b == 1: 输出 a + sqrt(n)
     *   - b > 0: 输出 a + b*sqrt(n)
     *   - b < 0: 输出 a - |b|*sqrt(n)
     *
     * @param {Object} coord - 二次根式坐标对象
     * @returns {string} LaTeX 格式字符串
     */
    function quadraticToLatex(coord) {
        var aNum = coord.aNum;
        var aDen = coord.aDen;
        var bNum = coord.bNum;
        var bDen = coord.bDen;
        var n    = coord.n;

        var aIsZero = rationalIsZero(aNum, aDen);
        var bIsZero = rationalIsZero(bNum, bDen);
        var bIsOne  = rationalIsOne(bNum, bDen);
        var bIsMinusOne = rationalIsMinusOne(bNum, bDen);
        var bIsPos  = rationalIsPositive(bNum, bDen);

        var aLatex = rationalToLatex(aNum, aDen);
        var sqrtPart = '\\sqrt{' + intStr(n) + '}';

        // b == 0: 仅 a
        if (bIsZero) {
            return aLatex;
        }

        // a == 0 && b == 1: sqrt(n)
        if (aIsZero && bIsOne) {
            return sqrtPart;
        }

        // a == 0 && b == -1: -sqrt(n)
        if (aIsZero && bIsMinusOne) {
            return '-' + sqrtPart;
        }

        // a == 0 && b 其他: b*sqrt(n)
        if (aIsZero) {
            return coeffToLatex(bNum, bDen) + sqrtPart;
        }

        // b == 1: a + sqrt(n)
        if (bIsOne) {
            return aLatex + '+' + sqrtPart;
        }

        // b == -1: a - sqrt(n)
        if (bIsMinusOne) {
            return aLatex + '-' + sqrtPart;
        }

        // b > 0: a + b*sqrt(n)
        if (bIsPos) {
            return aLatex + '+' + rationalToLatex(bNum, bDen) + sqrtPart;
        }

        // b < 0: a - |b|*sqrt(n)
        return aLatex + '-' + rationalToLatex(-bNum, bDen) + sqrtPart;
    }

    /**
     * 超越数坐标转 LaTeX 字符串
     * 将特殊数学常量转换为 LaTeX 命令：
     *   - 'pi' -> \pi
     *   - 'e'  -> e（直接输出字符 e）
     *   - 其他 -> \name（加反斜杠转义为 LaTeX 命令）
     *
     * @param {Object} coord - 超越数坐标对象，包含 name 属性
     * @returns {string} LaTeX 格式字符串
     */
    function transcendentalToLatex(coord) {
        if (coord.name === 'pi') {
            return '\\pi';
        }
        if (coord.name === 'e') {
            return 'e';
        }
        // 其他超越数用名称
        return '\\' + coord.name;
    }

    // ================================================================
    //  标签生成
    // ================================================================

    /**
     * 为节点生成标签
     * 点 -> P_0, P_1, ...
     * 线段 -> l_0, l_1, ...
     * 区域 -> R_0, R_1, ...
     * 端口 -> port_0, port_1, ...
     * 函数块 -> F_0, F_1, ...
     * 使用 _isNodeType 统一处理 node.type（字符串）和 node.geomType（数字枚举）双路径。
     */
    function nodeLabel(node) {
        // 优先使用 node.type 进行精确匹配（switch 无法直接处理 fallback，改用 if-else 链）
        if (_isNodeType(node, NT_POINT)) return 'P_{' + node.id + '}';
        if (_isNodeType(node, NT_LINE_SEGMENT)) return 'l_{' + node.id + '}';
        if (_isNodeType(node, NT_REGION)) return 'R_{' + node.id + '}';
        if (_isNodeType(node, NT_PORT)) return 'port_{' + node.id + '}';
        if (_isNodeType(node, NT_FUNCTION_BLOCK)) return 'F_{' + node.id + '}';
        return 'N_{' + node.id + '}';
    }

    /**
     * 获取点的短标签（用于线段端点表示）
     */
    function pointLabel(node) {
        return 'P_{' + node.id + '}';
    }

    /**
     * 获取线段的端点标签表示，如 P_0P_1
     * 通过 INCIDENCE 约束找到线段上的端点
     */
    function segmentEndpointLabels(graph, segmentNode) {
        var constraints = getConstraintsForNode(graph, segmentNode.id);
        var endpoints = [];
        var i, j;

        for (i = 0; i < constraints.length; i++) {
            var c = constraints[i];
            if (c.type === CT_INCIDENCE) {
                for (j = 0; j < c.participant_count; j++) {
                    var pid = c.participants[j];
                    if (pid !== segmentNode.id) {
                        var pNode = findNode(graph, pid);
                        if (pNode && _isNodeType(pNode, NT_POINT)) {
                            endpoints.push(pNode);
                        }
                    }
                }
            }
        }

        // 如果通过约束找到了两个端点
        if (endpoints.length >= 2) {
            return pointLabel(endpoints[0]) + pointLabel(endpoints[1]);
        }

        // 回退：使用线段自身的坐标信息
        // 线段的 symbolic_coords 通常有 4 个值 [x0, y0, x1, y1]
        if (segmentNode.symbolic_coords && segmentNode.symbolic_coords.length >= 4) {
            return 'P_{' + segmentNode.id + '}^{(1)}P_{' + segmentNode.id + '}^{(2)}';
        }

        return 'l_{' + segmentNode.id + '}';
    }

    /**
     * 从端点标签字符串中提取单个端点标签
     * epLabels 形如 "P_0P_1" 或 "P_{0}P_{1}"
     * @param {string} epLabels
     * @param {number} index - 0 或 1
     * @returns {string|null}
     */
    function extractEndpointLabels(epLabels, index) {
        var labels = [];
        var i = 0;
        var s = epLabels;

        while (i < s.length) {
            if (s.substring(i, i + 2) === 'P_') {
                var j = i + 2;
                if (j < s.length && s.charAt(j) === '{') {
                    // P_{...} 形式
                    var depth = 1;
                    j++;
                    while (j < s.length && depth > 0) {
                        if (s.charAt(j) === '{') { depth++; }
                        if (s.charAt(j) === '}') { depth--; }
                        j++;
                    }
                    labels.push(s.substring(i, j));
                    i = j;
                } else {
                    // P_N 形式（无花括号）
                    while (j < s.length && s.charAt(j) !== 'P' && s.charAt(j) !== 'l') {
                        j++;
                    }
                    labels.push(s.substring(i, j));
                    i = j;
                }
            } else {
                i++;
            }
        }

        if (index < labels.length) {
            return labels[index];
        }
        return null;
    }

    /**
     * 从标签 P_{id} 中提取 id
     */
    function extractIdFromLabel(label) {
        var match = label.match(/P_\{?(\d+)\}?/);
        if (match) {
            return parseInt(match[1], 10);
        }
        return null;
    }

    // ================================================================
    //  公式生成
    // ================================================================

    /**
     * 生成点的坐标公式
     * @param {object} node - 点节点
     * @returns {string} LaTeX
     */
    function pointToLatex(node) {
        var label = pointLabel(node);
        var xLatex, yLatex;

        if (node.symbolic_coords && node.coord_count >= 2) {
            xLatex = coordToLatex(node.symbolic_coords[0]);
            yLatex = coordToLatex(node.symbolic_coords[1]);
        } else {
            xLatex = 'x_{' + node.id + '}';
            yLatex = 'y_{' + node.id + '}';
        }

        return label + ' = \\left(' + xLatex + ', ' + yLatex + '\\right)';
    }

    /**
     * 生成线段长度公式
     * @param {object} graph
     * @param {object} segmentNode - 线段节点
     * @returns {string} LaTeX
     */
    function segmentLengthToLatex(graph, segmentNode) {
        var epLabels = segmentEndpointLabels(graph, segmentNode);
        var p0 = extractEndpointLabels(epLabels, 0);
        var p1 = extractEndpointLabels(epLabels, 1);

        if (p0 && p1) {
            return '|' + p0 + p1 + '| = \\sqrt{(' + p1 + '_x-' + p0 + '_x)^2 + (' + p1 + '_y-' + p0 + '_y)^2}';
        }

        return '|l_{' + segmentNode.id + '}| = \\sqrt{(\\Delta x)^2 + (\\Delta y)^2}';
    }

    /**
     * 从两点推导直线方程 ax + by + c = 0
     * @param {object} graph
     * @param {object} segmentNode - 线段节点
     * @returns {string} LaTeX
     */
    function lineEquationToLatex(graph, segmentNode) {
        var epLabels = segmentEndpointLabels(graph, segmentNode);
        var p0 = extractEndpointLabels(epLabels, 0);
        var p1 = extractEndpointLabels(epLabels, 1);

        if (p0 && p1) {
            return p0 + p1 + ': \\; ' + p0 + '_x(y_1 - y_0) - ' + p1 + '_y(x_1 - x_0) + (x_1 y_0 - x_0 y_1) = 0';
        }

        return 'l_{' + segmentNode.id + '}: ax + by + c = 0';
    }

    /**
     * 生成三角形面积公式
     *
     * 优化建议（当前实现保留原样，仅添加注释说明方向）：
     *   1. getConstraintsForNode 内部对 graph.constraints 做 O(C) 线性扫描，且
     *      线段节点的 segmentEndpointLabels 内部也会再次调用 getConstraintsForNode，
     *      形成嵌套 O(C^2) 扫描。建议为约束按 participant 预建反向索引（Map<id, Constraint[]>），
     *      或使用 graph._nodeIndex / graph._constraintIndex 等已构建索引。
     *   2. 去重阶段使用 points.indexOf() 进行 O(N^2) 去重，建议改用 Set 或 ID->Node Map 记录。
     *   3. findNode/segmentEndpointLabels 内部多次重复查找同一节点，可局部缓存结果。
     *
     * @param {object} graph
     * @param {object} regionNode - 区域节点
     * @returns {string} LaTeX
     */
    function triangleAreaToLatex(graph, regionNode) {
        // 查找与该区域相关的约束，找到三个端点
        var constraints = getConstraintsForNode(graph, regionNode.id);
        var points = [];
        var i, j;

        for (i = 0; i < constraints.length; i++) {
            var c = constraints[i];
            if (c.type === CT_CONTAINMENT) {
                for (j = 0; j < c.participant_count; j++) {
                    var pid = c.participants[j];
                    if (pid !== regionNode.id) {
                        var pNode = findNode(graph, pid);
                        if (pNode) {
                            if (_isNodeType(pNode, NT_POINT)) {
                                points.push(pNode);
                            } else if (_isNodeType(pNode, NT_LINE_SEGMENT)) {
                                // 从线段获取端点
                                var epLabels = segmentEndpointLabels(graph, pNode);
                                var ep0 = extractEndpointLabels(epLabels, 0);
                                var ep1 = extractEndpointLabels(epLabels, 1);
                                if (ep0) {
                                    var id0 = extractIdFromLabel(ep0);
                                    if (id0 !== null) {
                                        var n0 = findNode(graph, id0);
                                        if (n0 && points.indexOf(n0) === -1) {
                                            points.push(n0);
                                        }
                                    }
                                }
                                if (ep1) {
                                    var id1 = extractIdFromLabel(ep1);
                                    if (id1 !== null) {
                                        var n1 = findNode(graph, id1);
                                        if (n1 && points.indexOf(n1) === -1) {
                                            points.push(n1);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // 去重
        var uniquePoints = [];
        for (i = 0; i < points.length; i++) {
            if (uniquePoints.indexOf(points[i]) === -1) {
                uniquePoints.push(points[i]);
            }
        }
        points = uniquePoints;

        var rLabel = 'R_{' + regionNode.id + '}';

        if (points.length >= 3) {
            var pa = pointLabel(points[0]);
            var pb = pointLabel(points[1]);
            var pc = pointLabel(points[2]);
            var xa = 'x_{' + points[0].id + '}';
            var xb = 'x_{' + points[1].id + '}';
            var xc = 'x_{' + points[2].id + '}';
            var ya = 'y_{' + points[0].id + '}';
            var yb = 'y_{' + points[1].id + '}';
            var yc = 'y_{' + points[2].id + '}';

            return 'S_{\\triangle ' + pa + pb + pc + '} = \\frac{1}{2}|' +
                xa + '(' + yb + '-' + yc + ') + ' +
                xb + '(' + yc + '-' + ya + ') + ' +
                xc + '(' + ya + '-' + yb + ')|';
        }

        return 'S_{' + rLabel + '} = \\frac{1}{2}|x_0(y_1-y_2) + x_1(y_2-y_0) + x_2(y_0-y_1)|';
    }

    /**
     * 生成约束关系的 LaTeX
     * @param {object} graph
     * @param {object} constraint - Constraint 对象
     * @returns {string} LaTeX
     */
    function constraintToLatex(graph, constraint) {
        var participants = constraint.participants;
        var nodes = [];
        var i;

        for (i = 0; i < constraint.participant_count; i++) {
            nodes.push(findNode(graph, participants[i]));
        }

        switch (constraint.type) {
            case CT_INCIDENCE:
                return incidenceToLatex(nodes);
            case CT_BETWEENNESS:
                return betweennessToLatex(nodes);
            case CT_INTERSECTION:
                return intersectionToLatex(nodes);
            case CT_CONTAINMENT:
                return containmentToLatex(nodes);
            case CT_CONNECTION:
                return connectionToLatex(nodes);
            default:
                return '\\text{constraint}_{\\text{type ' + constraint.type + '}}';
        }
    }

    /**
     * INCIDENCE: 点在线段上
     * P_0 \in P_1P_2
     */
    function incidenceToLatex(nodes) {
        if (nodes.length >= 2) {
            var pointNode = null;
            var segNode = null;
            var i;

            for (i = 0; i < nodes.length; i++) {
                if (nodes[i] && _isNodeType(nodes[i], NT_POINT)) {
                    pointNode = nodes[i];
                } else if (nodes[i] && _isNodeType(nodes[i], NT_LINE_SEGMENT)) {
                    segNode = nodes[i];
                }
            }

            if (pointNode && segNode) {
                return pointLabel(pointNode) + ' \\in l_{' + segNode.id + '}';
            }

            return nodeLabel(nodes[0]) + ' \\in ' + nodeLabel(nodes[1]);
        }
        return '\\in';
    }

    /**
     * BETWEENNESS: P0 - P1 - P2（P1 在 P0 和 P2 之间）
     */
    function betweennessToLatex(nodes) {
        if (nodes.length >= 3) {
            return pointLabel(nodes[0]) + ' - ' + pointLabel(nodes[1]) + ' - ' + pointLabel(nodes[2]);
        }
        return '- - -';
    }

    /**
     * INTERSECTION: 线段1 ∩ 线段2 = 点
     * P_1P_2 \cap P_3P_4 = P_5
     */
    function intersectionToLatex(nodes) {
        if (nodes.length >= 3) {
            return nodeLabel(nodes[0]) + ' \\cap ' + nodeLabel(nodes[1]) + ' = ' + pointLabel(nodes[2]);
        }
        return '\\cap';
    }

    /**
     * CONTAINMENT: 内部 ⊂ 外部
     * P_0 \subset R_1
     */
    function containmentToLatex(nodes) {
        if (nodes.length >= 2) {
            return nodeLabel(nodes[0]) + ' \\subset ' + nodeLabel(nodes[1]);
        }
        return '\\subset';
    }

    /**
     * CONNECTION: 连接关系
     */
    function connectionToLatex(nodes) {
        if (nodes.length >= 2) {
            return nodeLabel(nodes[0]) + ' \\leftrightarrow ' + nodeLabel(nodes[1]);
        }
        return '\\leftrightarrow';
    }

    /**
     * 生成两点间距离公式
     * @param {object} graph
     * @param {object} pointA
     * @param {object} pointB
     * @returns {string} LaTeX
     */
    function distanceToLatex(graph, pointA, pointB) {
        var la = pointLabel(pointA);
        var lb = pointLabel(pointB);
        var xa = 'x_{' + pointA.id + '}';
        var xb = 'x_{' + pointB.id + '}';
        var ya = 'y_{' + pointA.id + '}';
        var yb = 'y_{' + pointB.id + '}';

        return 'd(' + la + ', ' + lb + ') = \\sqrt{(' + xb + '-' + xa + ')^2 + (' + yb + '-' + ya + ')^2}';
    }

    /**
     * 生成圆方程（如果有圆心和半径信息）
     * @param {object} graph
     * @param {object} regionNode - 区域节点（假设为圆）
     * @returns {string} LaTeX
     */
    function circleEquationToLatex(graph, regionNode) {
        // 查找与区域相关的约束来获取圆心和半径
        var constraints = getConstraintsForNode(graph, regionNode.id);
        var centerPoint = null;
        var radiusPoint = null;
        var i, j;

        for (i = 0; i < constraints.length; i++) {
            var c = constraints[i];
            for (j = 0; j < c.participant_count; j++) {
                var pid = c.participants[j];
                if (pid !== regionNode.id) {
                    var pNode = findNode(graph, pid);
                    if (pNode && _isNodeType(pNode, NT_POINT)) {
                        if (!centerPoint) {
                            centerPoint = pNode;
                        } else {
                            radiusPoint = pNode;
                        }
                    }
                }
            }
        }

        var h, k, r;
        if (centerPoint) {
            h = 'x_{' + centerPoint.id + '}';
            k = 'y_{' + centerPoint.id + '}';
        } else {
            h = 'h';
            k = 'k';
        }

        if (radiusPoint) {
            r = 'd(O, ' + pointLabel(radiusPoint) + ')';
        } else {
            r = 'r';
        }

        return '(x - ' + h + ')^2 + (y - ' + k + ')^2 = ' + r + '^2';
    }

    // ================================================================
    //  完整 LaTeX 文档生成
    // ================================================================

    /**
     * 将公式集合生成完整的 LaTeX 文档
     * @param {object} formulas - convert() 返回的结果
     * @returns {string} 完整 LaTeX 文档
     */
    function toFullLatexDocument(formulas) {
        var lines = [];
        var i;

        lines.push('\\documentclass{article}');
        lines.push('\\usepackage{amsmath}');
        lines.push('\\usepackage{amssymb}');
        lines.push('\\usepackage{geometry}');
        lines.push('\\geometry{a4paper, margin=1in}');
        lines.push('');
        lines.push('\\begin{document}');
        lines.push('');
        lines.push('\\title{Lv-00 Geometric Formulas}');
        lines.push('\\author{Graph-to-Formula Converter}');
        lines.push('\\date{}');
        lines.push('');
        lines.push('\\maketitle');
        lines.push('');

        // 点
        if (formulas.points && formulas.points.length > 0) {
            lines.push('\\section*{Points}');
            lines.push('');
            lines.push('\\begin{align*}');
            for (i = 0; i < formulas.points.length; i++) {
                lines.push('    ' + formulas.points[i].latex + ' \\\\');
            }
            lines.push('\\end{align*}');
            lines.push('');
        }

        // 线段
        if (formulas.segments && formulas.segments.length > 0) {
            lines.push('\\section*{Line Segments}');
            lines.push('');
            lines.push('\\begin{align*}');
            for (i = 0; i < formulas.segments.length; i++) {
                lines.push('    ' + formulas.segments[i].latex + ' \\\\');
            }
            lines.push('\\end{align*}');
            lines.push('');
        }

        // 约束
        if (formulas.constraints && formulas.constraints.length > 0) {
            lines.push('\\section*{Constraints}');
            lines.push('');
            lines.push('\\begin{align*}');
            for (i = 0; i < formulas.constraints.length; i++) {
                lines.push('    ' + formulas.constraints[i].latex + ' \\\\');
            }
            lines.push('\\end{align*}');
            lines.push('');
        }

        // 方程
        if (formulas.equations && formulas.equations.length > 0) {
            lines.push('\\section*{Equations}');
            lines.push('');
            lines.push('\\begin{align*}');
            for (i = 0; i < formulas.equations.length; i++) {
                lines.push('    ' + formulas.equations[i].latex + ' \\\\');
            }
            lines.push('\\end{align*}');
            lines.push('');
        }

        lines.push('\\end{document}');

        return lines.join('\n');
    }

    // ================================================================
    //  主转换逻辑
    // ================================================================

    /**
     * 将整个约束图转换为公式集合
     * @param {object} graph - 约束图
     * @returns {object} 公式集合
     */
    function convert(graph) {
        var result = {
            points: [],
            segments: [],
            constraints: [],
            equations: [],
            fullLatex: ''
        };

        // 参数校验
        if (!graph) {
            console.warn('[GraphToFormula] convert: 图对象为空');
            return result;
        }
        if (!graph.nodes || !Array.isArray(graph.nodes)) {
            console.warn('[GraphToFormula] convert: 图节点列表无效');
            return result;
        }

        // 构建 ID->对象 索引缓存，将 O(n) 查找降为 O(1)
        // 缓存到 graph 对象上，后续 findNode/findConstraint 优先使用
        if (!graph._nodeIndex) {
            graph._nodeIndex = _buildNodeIndex(graph);
        }
        if (!graph._constraintIndex) {
            graph._constraintIndex = _buildConstraintIndex(graph);
        }

        var i, node, c;

        try {
            // 遍历所有节点
        for (i = 0; i < graph.nodes.length; i++) {
            node = graph.nodes[i];

            // 使用统一类型判断函数，兼容 node.type（字符串）和 node.geomType（数字枚举）双路径
            if (_isNodeType(node, NT_POINT)) {
                result.points.push({
                    id: node.id,
                    label: pointLabel(node),
                    latex: pointToLatex(node),
                    coords: node.symbolic_coords ? node.symbolic_coords.slice(0, node.coord_count) : []
                });
            } else if (_isNodeType(node, NT_LINE_SEGMENT)) {
                var epLabels = segmentEndpointLabels(graph, node);
                result.segments.push({
                    id: node.id,
                    label: 'l_{' + node.id + '}',
                    latex: epLabels + ': \\text{from } ' +
                        extractEndpointLabels(epLabels, 0) +
                        ' \\text{ to } ' +
                        extractEndpointLabels(epLabels, 1),
                    endpoints: epLabels
                });

                // 添加线段长度公式到方程
                result.equations.push({
                    latex: segmentLengthToLatex(graph, node),
                    description: 'Length of segment ' + node.id
                });

                // 添加直线方程
                result.equations.push({
                    latex: lineEquationToLatex(graph, node),
                    description: 'Line equation for segment ' + node.id
                });
            } else if (_isNodeType(node, NT_REGION)) {
                // 添加面积公式
                result.equations.push({
                    latex: triangleAreaToLatex(graph, node),
                    description: 'Area of region ' + node.id
                });
            }
        }

        // 遍历所有约束
        for (i = 0; i < graph.constraints.length; i++) {
            c = graph.constraints[i];
            result.constraints.push({
                type: c.type,
                latex: constraintToLatex(graph, c),
                participants: c.participants.slice(0, c.participant_count)
            });
        }

        // 生成完整 LaTeX 文档
        result.fullLatex = toFullLatexDocument(result);

        return result;
        } catch (e) {
            console.error('[GraphToFormula] convert: 转换异常:', e.message);
            result.fullLatex = '转换失败: ' + e.message;
            return result;
        }
    }

    /**
     * 仅转换指定节点
     * @param {object} graph
     * @param {number} nodeId
     * @returns {object} 公式对象
     */
    function convertNode(graph, nodeId) {
        var node = findNode(graph, nodeId);
        if (!node) {
            return { error: 'Node ' + nodeId + ' not found' };
        }

        var result = {
            id: node.id,
            type: node.type,
            label: nodeLabel(node),
            latex: '',
            equations: []
        };

        // 使用统一类型判断函数，兼容 node.type（字符串）和 node.geomType（数字枚举）双路径
        if (_isNodeType(node, NT_POINT)) {
            result.latex = pointToLatex(node);
            result.coords = node.symbolic_coords ? node.symbolic_coords.slice(0, node.coord_count) : [];
        } else if (_isNodeType(node, NT_LINE_SEGMENT)) {
            var epLabels = segmentEndpointLabels(graph, node);
            result.latex = epLabels + ': \\text{from } ' +
                extractEndpointLabels(epLabels, 0) +
                ' \\text{ to } ' +
                extractEndpointLabels(epLabels, 1);
            result.equations.push({
                latex: segmentLengthToLatex(graph, node),
                description: 'Length'
            });
            result.equations.push({
                latex: lineEquationToLatex(graph, node),
                description: 'Line equation'
            });
        } else if (_isNodeType(node, NT_REGION)) {
            result.latex = 'R_{' + node.id + '}';
            result.equations.push({
                latex: triangleAreaToLatex(graph, node),
                description: 'Area'
            });
            result.equations.push({
                latex: circleEquationToLatex(graph, node),
                description: 'Circle equation (if applicable)'
            });
        } else {
            result.latex = nodeLabel(node);
        }

        return result;
    }

    /**
     * 仅转换指定约束
     * @param {object} graph
     * @param {number} constraintId
     * @returns {object} 公式对象
     */
    function convertConstraint(graph, constraintId) {
        var c = findConstraint(graph, constraintId);
        if (!c) {
            return { error: 'Constraint ' + constraintId + ' not found' };
        }

        var typeNames = [
            'INCIDENCE',
            'BETWEENNESS',
            'INTERSECTION',
            'CONTAINMENT',
            'CONNECTION'
        ];

        return {
            id: c.id,
            type: c.type,
            typeName: typeNames[c.type] || ('TYPE_' + c.type),
            latex: constraintToLatex(graph, c),
            participants: c.participants.slice(0, c.participant_count)
        };
    }

    // ================================================================
    //  公开 API
    // ================================================================

    return {
        convert: convert,
        convertNode: convertNode,
        convertConstraint: convertConstraint,
        pointToLatex: pointToLatex,
        segmentLengthToLatex: segmentLengthToLatex,
        lineEquationToLatex: lineEquationToLatex,
        triangleAreaToLatex: triangleAreaToLatex,
        constraintToLatex: constraintToLatex,
        coordToLatex: coordToLatex,
        toFullLatexDocument: toFullLatexDocument
    };

})();
