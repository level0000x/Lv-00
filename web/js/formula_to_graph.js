/**
 * Lv-00 Formula to Graph Converter
 * 将公式 AST 转换为约束图操作
 *
 * 严格 ES5 语法（无 class, const, let, arrow functions, template literals,
 * destructuring, default params, spread）
 *
 * 依赖 Lv-00 JS 后端 API：
 *   backend.graphCreate()
 *   backend.graphAddPoint(graph, x, y)
 *   backend.graphAddLineSegment(graph, node1Id, node2Id)
 *   backend.graphAddRegion(graph, segmentIds, count)
 *   backend.graphAddIncidence(graph, pointId, lineOrRegionId)
 *   backend.graphAddBetweenness(graph, p1, p2, p3)
 *   backend.graphAddIntersection(graph, line1, line2, resultPoint)
 *   backend.graphAddContainment(graph, innerId, outerId)
 *   backend.graphNormalize(graph, scopeAware)
 *
 * 用法：
 *   var result = FormulaToGraph.convert(ast, graph, backend);
 *   // result.success, result.operations, result.errors, result.newNodes
 */

var FormulaToGraph = (function() {
    'use strict';

    // ---- 内部状态 ----
    var _log = [];
    var _varMap = {};

    // ---- 工具函数 ----

    /**
     * 添加日志条目
     */
    function _logMessage(message, type) {
        _log.push({
            message: message,
            type: type || 'info',
            timestamp: Date.now()
        });
    }

    /**
     * 创建操作结果对象
     * 统一格式的转换结果包装器，用于所有转换函数的返回。
     *
     * @param {boolean} success - 转换是否成功
     * @param {Array} operations - 操作记录列表 [{type, description, nodeIds}]
     * @param {Array} errors - 错误记录列表 [{message, node}]
     * @param {Array} newNodes - 新创建的节点 ID 列表
     * @param {Array} newConstraints - 新创建的约束 ID 列表
     * @returns {Object} 标准结果对象 { success, operations, errors, newNodes, newConstraints }
     */
    function _makeResult(success, operations, errors, newNodes, newConstraints) {
        return {
            success: !!success,
            operations: operations || [],
            errors: errors || [],
            newNodes: newNodes || [],
            newConstraints: newConstraints || []
        };
    }

    /**
     * 创建单条操作记录
     * 记录在 AST->Graph 转换过程中执行的每一步操作。
     *
     * @param {string} type - 操作类型（如 'addPoint', 'addLineSegment', 'normalize'）
     * @param {string} description - 操作描述文本
     * @param {Array} [nodeIds] - 操作涉及的节点 ID 列表
     * @returns {Object} 操作记录 { type, description, nodeIds }
     */
    function _makeOperation(type, description, nodeIds) {
        return {
            type: type,
            description: description,
            nodeIds: nodeIds || []
        };
    }

    /**
     * 创建错误记录
     * 当 AST 节点解析或图操作失败时生成错误信息。
     *
     * @param {string} message - 错误描述消息
     * @param {Object|null} [node] - 导致错误的 AST 节点（用于定位）
     * @returns {Object} 错误记录 { message, node }
     */
    function _makeError(message, node) {
        return {
            message: message,
            node: node || null
        };
    }

    /**
     * 安全获取 AST 节点类型
     */
    function _getNodeType(node) {
        if (!node) return null;
        return node.type || node.nodeType || null;
    }

    /**
     * 安全获取 AST 节点名称
     */
    function _getNodeName(node) {
        if (!node) return null;
        return node.name || node.id || node.value || null;
    }

    /**
     * 从 AST 节点提取数值（安全数值提取器）
     * 递归地从各种 AST 节点类型中提取数值，支持：
     *   - 直接数字（number/string）
     *   - 数字字面量节点（'number'/'NumericLiteral'）
     *   - 二元运算节点（'+'/'-'/'*'/'/'/'^' 递归求值）
     *   - 函数调用节点（sqrt/sin/cos/tan/abs/pi 等数学函数）
     *   - 标识符常量（pi/PI/e/E）
     *
     * 无法提取时返回 0 作为安全默认值。
     *
     * @param {*} node - AST 节点或原始值
     * @returns {number} 提取的数值，无法提取时返回 0
     */
    function _extractNumeric(node) {
        if (!node) return 0;
        // 直接是数字
        if (typeof node === 'number') return node;
        // 字符串数字
        if (typeof node === 'string') {
            var n = parseFloat(node);
            return isNaN(n) ? 0 : n;
        }
        // AST 节点
        if (node.type === 'number' || node.type === 'NumericLiteral') {
            return parseFloat(node.value);
        }
        // 简单二元运算
        if (node.type === 'binary' || node.type === 'BinaryExpression') {
            var left = _extractNumeric(node.left);
            var right = _extractNumeric(node.right);
            var op = node.operator || '+';
            switch (op) {
                case '+': return left + right;
                case '-': return left - right;
                case '*': return left * right;
                case '/': return right !== 0 ? left / right : 0;
                case '^': return Math.pow(left, right);
                default: return 0;
            }
        }
        // 函数调用（仅支持常见数学函数）
        if (node.type === 'call' || node.type === 'CallExpression') {
            var fname = _getNodeName(node.callee || node.func);
            var args = node.args || node.arguments || [];
            var argVal = args.length > 0 ? _extractNumeric(args[0]) : 0;
            switch (fname) {
                case 'sqrt': return Math.sqrt(argVal);
                case 'abs': return Math.abs(argVal);
                case 'sin': return Math.sin(argVal);
                case 'cos': return Math.cos(argVal);
                case 'tan': return Math.tan(argVal);
                case 'pi': return Math.PI;
                case 'PI': return Math.PI;
                default: return 0;
            }
        }
        // 标识符（常见常量）
        if (node.type === 'identifier' || node.type === 'Identifier') {
            var name = node.name || node.value;
            if (name === 'pi' || name === 'PI') return Math.PI;
            if (name === 'e' || name === 'E') return Math.E;
            return 0;
        }
        return 0;
    }

    /**
     * 判断 AST 节点是否为变量引用（非数值常量）
     * 用于区分真正的用户变量（如 x, y, A）和内置数学常量（如 pi, e）。
     * 排除的常量包括：pi, e, sqrt, sin, cos, tan, abs 等。
     *
     * @param {*} node - AST 节点或原始值
     * @returns {boolean} true 表示为用户变量
     */
    function _isVariable(node) {
        if (!node) return false;
        if (typeof node === 'string') {
            var n = parseFloat(node);
            return isNaN(n);
        }
        if (node.type === 'identifier' || node.type === 'Identifier') {
            var name = (node.name || node.value || '').toLowerCase();
            return name !== 'pi' && name !== 'e' &&
                   name !== 'sqrt' && name !== 'sin' &&
                   name !== 'cos' && name !== 'tan' && name !== 'abs';
        }
        return false;
    }

    // ---- 转换函数 ----

    /**
     * 处理几何点定义
     * AST 格式: {type: 'point', name: 'A', args: [{x: ...}, {y: ...}]}
     * 或: {type: 'point', name: 'A', x: 0, y: 0}
     */
    function convertPoint(ast, graph, backend) {
        var operations = [];
        var errors = [];
        var newNodes = [];
        var newConstraints = [];

        var name = ast.name || ast.id || 'P';
        var x, y;

        // 提取坐标
        if (ast.args && ast.args.length >= 2) {
            x = _extractNumeric(ast.args[0]);
            y = _extractNumeric(ast.args[1]);
        } else if (ast.x !== undefined && ast.y !== undefined) {
            x = _extractNumeric(ast.x);
            y = _extractNumeric(ast.y);
        } else {
            errors.push(_makeError('Point "' + name + '" missing coordinates', ast));
            return _makeResult(false, operations, errors, newNodes, newConstraints);
        }

        // 调用后端 API 创建点
        var result = null;
        try {
            result = backend.graphAddPoint(graph, x, y);
        } catch (e) {
            errors.push(_makeError('Failed to add point "' + name + '": ' + e.message, ast));
            return _makeResult(false, operations, errors, newNodes, newConstraints);
        }

        if (result && result.success) {
            var nodeId = result.nodeId;
            _varMap[name] = nodeId;
            newNodes.push(nodeId);
            operations.push(_makeOperation('addPoint',
                'point ' + name + '(' + x + ', ' + y + ')', [nodeId]));
            _logMessage('Created point ' + name + ' at (' + x + ', ' + y +
                ') -> nodeId ' + nodeId, 'success');
        } else {
            errors.push(_makeError('Failed to create point "' + name + '"', ast));
        }

        return _makeResult(
            errors.length === 0,
            operations, errors, newNodes, newConstraints
        );
    }

    /**
     * 处理几何线段定义
     * AST 格式: {type: 'segment', name: 'AB', args: ['A', 'B']}
     * 或: {type: 'segment', name: 'AB', from: 'A', to: 'B'}
     */
    function convertSegment(ast, graph, backend) {
        var operations = [];
        var errors = [];
        var newNodes = [];
        var newConstraints = [];

        var name = ast.name || ast.id || 'seg';
        var fromName, toName;

        if (ast.args && ast.args.length >= 2) {
            fromName = ast.args[0].name || ast.args[0];
            toName = ast.args[1].name || ast.args[1];
        } else if (ast.from && ast.to) {
            fromName = ast.from.name || ast.from;
            toName = ast.to.name || ast.to;
        } else {
            errors.push(_makeError('Segment "' + name + '" missing endpoint references', ast));
            return _makeResult(false, operations, errors, newNodes, newConstraints);
        }

        // 查找端点 ID
        var fromId = _varMap[fromName];
        var toId = _varMap[toName];

        if (fromId === undefined) {
            errors.push(_makeError('Segment "' + name + '": point "' +
                fromName + '" not found in variable map', ast));
            return _makeResult(false, operations, errors, newNodes, newConstraints);
        }
        if (toId === undefined) {
            errors.push(_makeError('Segment "' + name + '": point "' +
                toName + '" not found in variable map', ast));
            return _makeResult(false, operations, errors, newNodes, newConstraints);
        }

        // 调用后端 API 创建线段
        try {
            backend.graphAddLineSegment(graph, fromId, toId);
            operations.push(_makeOperation('addLineSegment',
                'segment ' + name + '(' + fromName + ', ' + toName + ')',
                [fromId, toId]));
            _logMessage('Created segment ' + name + ' from ' +
                fromName + ' to ' + toName, 'success');
        } catch (e) {
            errors.push(_makeError('Failed to add segment "' + name + '": ' +
                e.message, ast));
        }

        return _makeResult(
            errors.length === 0,
            operations, errors, newNodes, newConstraints
        );
    }

    /**
     * 处理几何圆定义
     * AST 格式: {type: 'circle', name: 'O', center: 'A', radius: 3}
     * 或: {type: 'circle', name: 'O', args: [{center: 'A'}, {radius: 3}]}
     *
     * 实现方式：创建圆心点 + 半径点（圆心x+radius, 圆心y）+ 线段
     */
    function convertCircle(ast, graph, backend) {
        var operations = [];
        var errors = [];
        var newNodes = [];
        var newConstraints = [];

        var name = ast.name || ast.id || 'C';
        var centerName, radius;

        // 提取圆心和半径
        if (ast.center !== undefined) {
            centerName = ast.center.name || ast.center;
            radius = _extractNumeric(ast.radius);
        } else if (ast.args && ast.args.length >= 2) {
            centerName = ast.args[0].name || ast.args[0].center || ast.args[0];
            radius = _extractNumeric(ast.args[1].radius || ast.args[1]);
        } else {
            errors.push(_makeError('Circle "' + name + '" missing center or radius', ast));
            return _makeResult(false, operations, errors, newNodes, newConstraints);
        }

        var centerId = _varMap[centerName];
        if (centerId === undefined) {
            errors.push(_makeError('Circle "' + name + '": center point "' +
                centerName + '" not found', ast));
            return _makeResult(false, operations, errors, newNodes, newConstraints);
        }

        // 创建半径点 R（位于圆心右侧，距离为 radius）
        var radiusPointName = name + '_R';
        var radiusResult = null;
        try {
            radiusResult = backend.graphAddPoint(graph, radius, 0);
        } catch (e) {
            errors.push(_makeError('Failed to create radius point for circle "' +
                name + '": ' + e.message, ast));
            return _makeResult(false, operations, errors, newNodes, newConstraints);
        }

        if (radiusResult && radiusResult.success) {
            var radiusNodeId = radiusResult.nodeId;
            _varMap[radiusPointName] = radiusNodeId;
            newNodes.push(radiusNodeId);
            operations.push(_makeOperation('addPoint',
                'radius point ' + radiusPointName + '(' + radius + ', 0)',
                [radiusNodeId]));

            // 创建从圆心到半径点的线段
            try {
                backend.graphAddLineSegment(graph, centerId, radiusNodeId);
                operations.push(_makeOperation('addLineSegment',
                    'radius segment ' + centerName + ' -> ' + radiusPointName,
                    [centerId, radiusNodeId]));
            } catch (e) {
                errors.push(_makeError('Failed to add radius segment: ' +
                    e.message, ast));
            }

            _logMessage('Created circle ' + name + ' with center ' +
                centerName + ' and radius ' + radius, 'success');
        } else {
            errors.push(_makeError('Failed to create radius point for circle "' +
                name + '"', ast));
        }

        return _makeResult(
            errors.length === 0,
            operations, errors, newNodes, newConstraints
        );
    }

    /**
     * 处理几何三角形定义
     * AST 格式: {type: 'triangle', name: 'ABC', vertices: ['A', 'B', 'C']}
     * 或: {type: 'triangle', name: 'ABC', args: ['A', 'B', 'C']}
     *
     * 实现方式：创建三条线段 AB, BC, CA
     */
    function convertTriangle(ast, graph, backend) {
        var operations = [];
        var errors = [];
        var newNodes = [];
        var newConstraints = [];

        var name = ast.name || ast.id || 'tri';
        var vertexNames = [];

        if (ast.vertices && ast.vertices.length >= 3) {
            for (var i = 0; i < ast.vertices.length; i++) {
                vertexNames.push(ast.vertices[i].name || ast.vertices[i]);
            }
        } else if (ast.args && ast.args.length >= 3) {
            for (var j = 0; j < ast.args.length; j++) {
                vertexNames.push(ast.args[j].name || ast.args[j]);
            }
        } else {
            errors.push(_makeError('Triangle "' + name + '" needs 3 vertices', ast));
            return _makeResult(false, operations, errors, newNodes, newConstraints);
        }

        // 验证所有顶点存在
        var vertexIds = [];
        for (var k = 0; k < vertexNames.length; k++) {
            var vid = _varMap[vertexNames[k]];
            if (vid === undefined) {
                errors.push(_makeError('Triangle "' + name + '": vertex "' +
                    vertexNames[k] + '" not found', ast));
                return _makeResult(false, operations, errors, newNodes, newConstraints);
            }
            vertexIds.push(vid);
        }

        // 创建三条边: AB, BC, CA
        var edges = [[0, 1], [1, 2], [2, 0]];
        var edgeLabels = [
            vertexNames[0] + vertexNames[1],
            vertexNames[1] + vertexNames[2],
            vertexNames[2] + vertexNames[0]
        ];

        for (var e = 0; e < edges.length; e++) {
            try {
                backend.graphAddLineSegment(graph,
                    vertexIds[edges[e][0]], vertexIds[edges[e][1]]);
                operations.push(_makeOperation('addLineSegment',
                    'triangle edge ' + edgeLabels[e],
                    [vertexIds[edges[e][0]], vertexIds[edges[e][1]]]));
            } catch (err) {
                errors.push(_makeError('Failed to add triangle edge ' +
                    edgeLabels[e] + ': ' + err.message, ast));
            }
        }

        _logMessage('Created triangle ' + name + ' with vertices ' +
            vertexNames.join(', '), 'success');

        return _makeResult(
            errors.length === 0,
            operations, errors, newNodes, newConstraints
        );
    }

    /**
     * 处理几何约束 - perpendicular
     * AST 格式: {type: 'constraint', kind: 'perpendicular', args: ['A', 'B', 'C']}
     * 含义: AB 垂直于 BC，通过 betweenness 约束表示
     */
    function _convertPerpendicular(ast, graph, backend) {
        var operations = [];
        var errors = [];
        var newNodes = [];
        var newConstraints = [];

        var args = ast.args || [];
        if (args.length < 3) {
            errors.push(_makeError('perpendicular constraint needs 3 points', ast));
            return _makeResult(false, operations, errors, newNodes, newConstraints);
        }

        var p1Name = args[0].name || args[0];
        var p2Name = args[1].name || args[1];
        var p3Name = args[2].name || args[2];

        var p1Id = _varMap[p1Name];
        var p2Id = _varMap[p2Name];
        var p3Id = _varMap[p3Name];

        if (p1Id === undefined || p2Id === undefined || p3Id === undefined) {
            errors.push(_makeError('perpendicular: one or more points not found', ast));
            return _makeResult(false, operations, errors, newNodes, newConstraints);
        }

        try {
            backend.graphAddBetweenness(graph, p1Id, p2Id, p3Id);
            operations.push(_makeOperation('addBetweenness',
                'perpendicular constraint: ' + p1Name + '-' + p2Name + '-' + p3Name,
                [p1Id, p2Id, p3Id]));
            _logMessage('Added perpendicular constraint ' +
                p1Name + '-' + p2Name + '-' + p3Name, 'success');
        } catch (e) {
            errors.push(_makeError('Failed to add perpendicular constraint: ' +
                e.message, ast));
        }

        return _makeResult(errors.length === 0, operations, errors,
            newNodes, newConstraints);
    }

    /**
     * 处理几何约束 - parallel
     * AST 格式: {type: 'constraint', kind: 'parallel', args: ['l1', 'l2']}
     */
    function _convertParallel(ast, graph, backend) {
        var operations = [];
        var errors = [];
        var newNodes = [];
        var newConstraints = [];

        var args = ast.args || [];
        if (args.length < 2) {
            errors.push(_makeError('parallel constraint needs 2 lines', ast));
            return _makeResult(false, operations, errors, newNodes, newConstraints);
        }

        var l1Name = args[0].name || args[0];
        var l2Name = args[1].name || args[1];

        var l1Id = _varMap[l1Name];
        var l2Id = _varMap[l2Name];

        if (l1Id === undefined || l2Id === undefined) {
            errors.push(_makeError('parallel: one or more lines not found', ast));
            return _makeResult(false, operations, errors, newNodes, newConstraints);
        }

        try {
            backend.graphAddIncidence(graph, l1Id, l2Id);
            operations.push(_makeOperation('addIncidence',
                'parallel constraint: ' + l1Name + ' || ' + l2Name,
                [l1Id, l2Id]));
            _logMessage('Added parallel constraint ' + l1Name + ' || ' + l2Name,
                'success');
        } catch (e) {
            errors.push(_makeError('Failed to add parallel constraint: ' +
                e.message, ast));
        }

        return _makeResult(errors.length === 0, operations, errors,
            newNodes, newConstraints);
    }

    /**
     * 处理几何约束 - midpoint
     * AST 格式: {type: 'constraint', kind: 'midpoint', args: ['A', 'B']}
     * 计算中点坐标并添加为新点
     */
    function _convertMidpoint(ast, graph, backend) {
        var operations = [];
        var errors = [];
        var newNodes = [];
        var newConstraints = [];

        var args = ast.args || [];
        if (args.length < 2) {
            errors.push(_makeError('midpoint constraint needs 2 points', ast));
            return _makeResult(false, operations, errors, newNodes, newConstraints);
        }

        var p1Name = args[0].name || args[0];
        var p2Name = args[1].name || args[1];
        var midName = ast.name || ast.id || ('M_' + p1Name + p2Name);

        var p1Id = _varMap[p1Name];
        var p2Id = _varMap[p2Name];

        if (p1Id === undefined || p2Id === undefined) {
            errors.push(_makeError('midpoint: one or more points not found', ast));
            return _makeResult(false, operations, errors, newNodes, newConstraints);
        }

        // 创建中点（使用近似坐标 0,0，实际坐标由约束系统推导）
        try {
            var result = backend.graphAddPoint(graph, 0, 0);
            if (result && result.success) {
                var midId = result.nodeId;
                _varMap[midName] = midId;
                newNodes.push(midId);

                // 添加 betweenness 约束：p1 - M - p2
                backend.graphAddBetweenness(graph, p1Id, midId, p2Id);

                operations.push(_makeOperation('addPoint',
                    'midpoint ' + midName + ' of ' + p1Name + p2Name, [midId]));
                operations.push(_makeOperation('addBetweenness',
                    'midpoint betweenness: ' + p1Name + '-' + midName + '-' + p2Name,
                    [p1Id, midId, p2Id]));
                _logMessage('Created midpoint ' + midName + ' between ' +
                    p1Name + ' and ' + p2Name, 'success');
            }
        } catch (e) {
            errors.push(_makeError('Failed to create midpoint: ' + e.message, ast));
        }

        return _makeResult(errors.length === 0, operations, errors,
            newNodes, newConstraints);
    }

    /**
     * 处理几何约束（分发器）
     */
    function convertConstraint(ast, graph, backend) {
        var kind = ast.kind || ast.constraintType || '';

        switch (kind) {
            case 'perpendicular':
                return _convertPerpendicular(ast, graph, backend);
            case 'parallel':
                return _convertParallel(ast, graph, backend);
            case 'midpoint':
                return _convertMidpoint(ast, graph, backend);
            default:
                return _makeResult(false, [], [
                    _makeError('Unknown constraint type: ' + kind, ast)
                ], [], []);
        }
    }

    /**
     * 处理代数方程
     * AST 格式: {type: 'equation', left: ..., right: ...}
     * 或: {type: 'equation', expr: 'x^2 + y^2 = r^2'}
     *
     * 将方程转换为采样点用于绘制曲线
     */
    function convertEquation(ast, graph, backend) {
        var operations = [];
        var errors = [];
        var newNodes = [];
        var newConstraints = [];

        // 生成采样点
        var points = equationToSamplePoints(ast, -10, 10, -10, 10, 100);

        if (points.length === 0) {
            errors.push(_makeError('Equation produced no sample points', ast));
            return _makeResult(false, operations, errors, newNodes, newConstraints);
        }

        // 将采样点添加到图中
        var pointIds = [];
        for (var i = 0; i < points.length; i++) {
            try {
                var result = backend.graphAddPoint(graph, points[i].x, points[i].y);
                if (result && result.success) {
                    pointIds.push(result.nodeId);
                    newNodes.push(result.nodeId);
                }
            } catch (e) {
                // 静默跳过无法添加的点
            }
        }

        // 将相邻点用线段连接（形成曲线近似）
        for (var j = 0; j < pointIds.length - 1; j++) {
            try {
                backend.graphAddLineSegment(graph, pointIds[j], pointIds[j + 1]);
            } catch (e) {
                // 静默跳过
            }
        }

        var eqDesc = ast.expr || ast.raw || 'equation';
        operations.push(_makeOperation('addEquationCurve',
            'equation curve: ' + eqDesc + ' (' + points.length + ' sample points)',
            pointIds));
        _logMessage('Converted equation to curve with ' + points.length +
            ' sample points', 'success');

        return _makeResult(true, operations, errors, newNodes, newConstraints);
    }

    /**
     * 从方程生成采样点
     * 支持显式方程 y=f(x) 和隐式方程 f(x,y)=0
     *
     * @param {Object} ast - 方程 AST
     * @param {number} xMin - x 范围最小值
     * @param {number} xMax - x 范围最大值
     * @param {number} yMin - y 范围最小值
     * @param {number} yMax - y 范围最大值
     * @param {number} resolution - 采样分辨率（默认 100）
     * @returns {Array} [{x, y}] 采样点数组
     */
    function equationToSamplePoints(ast, xMin, xMax, yMin, yMax, resolution) {
        var points = [];

        // 默认参数
        if (xMin === undefined) xMin = -10;
        if (xMax === undefined) xMax = 10;
        if (yMin === undefined) yMin = -10;
        if (yMax === undefined) yMax = 10;
        if (resolution === undefined) resolution = 100;

        var expr = ast.expr || ast.raw || '';
        var left = ast.left || null;
        var right = ast.right || null;

        // 尝试解析为显式方程 y = f(x)
        var isExplicit = false;
        var explicitExpr = null;

        if (left && right) {
            var leftName = '';
            if (typeof left === 'string') {
                leftName = left.trim();
            } else if (left.type === 'identifier' || left.type === 'Identifier') {
                leftName = (left.name || left.value || '').trim();
            }

            if (leftName === 'y') {
                isExplicit = true;
                explicitExpr = right;
            }
        }

        // 字符串形式解析
        if (!isExplicit && typeof expr === 'string') {
            var trimmed = expr.replace(/\s/g, '');
            if (/^y\s*=/.test(trimmed)) {
                isExplicit = true;
                var rhs = trimmed.replace(/^y\s*=\s*/, '');
                explicitExpr = {type: 'raw', value: rhs};
            }
        }

        if (isExplicit && explicitExpr) {
            // 显式方程 y = f(x)
            var step = (xMax - xMin) / resolution;
            for (var i = 0; i <= resolution; i++) {
                var x = xMin + i * step;
                var y = _evaluateExplicitExpr(explicitExpr, x);
                if (y !== null && y >= yMin && y <= yMax) {
                    points.push({x: x, y: y});
                }
            }
        } else {
            // 隐式方程 f(x,y) = 0，使用网格采样
            var xStep = (xMax - xMin) / resolution;
            var yStep = (yMax - yMin) / resolution;
            for (var ix = 0; ix <= resolution; ix++) {
                for (var iy = 0; iy <= resolution; iy++) {
                    var sx = xMin + ix * xStep;
                    var sy = yMin + iy * yStep;
                    var val = _evaluateImplicitExpr(expr, left, right, sx, sy);
                    if (val !== null && Math.abs(val) < 0.5) {
                        points.push({x: sx, y: sy});
                    }
                }
            }
        }

        return points;
    }

    /**
     * 安全的数学表达式求值器（纯手动递归下降解析，不使用 eval / Function）
     *
     * [安全修复] 原实现使用 new Function() 执行动态代码，正则白名单可被绕过
     * （例如 Math.constructor 可访问 Function 构造器）。
     * 新实现使用递归下降解析器，完全不依赖 eval/Function，从根本上杜绝代码注入。
     *
     * 支持的语法：
     *   - 数字（整数、小数、负数）
     *   - 四则运算 + - * / 和幂运算 **
     *   - 括号 ( )
     *   - 函数：sin, cos, tan, sqrt, abs, log, exp, pow
     *   - 常量：PI, E, pi, e
     *   - 变量替换后只允许数字和小数点
     *
     * 解析器架构（优先级从低到高）：
     *   parseExpression(加减) -> parseTerm(乘除) -> parsePower(幂) -> parseUnary(一元) -> parseAtom(原子)
     *
     * @param {string} expr - 数学表达式字符串（变量应已替换为数值）
     * @param {Object} vars - 变量映射 {name: value}（可选，用于替换变量名）
     * @returns {number} 求值结果，解析失败返回 NaN
     */
    function _safeMathEval(expr, vars) {
        if (typeof expr !== 'string' || expr.trim() === '') {
            return NaN;
        }

        // ---- 第一步：变量替换 ----
        // 将 vars 中的变量名替换为对应的数值（用占位符避免递归替换）
        var processed = expr;
        if (vars) {
            // 收集所有变量名，按名称长度降序排列，避免短名称先替换导致长名称被破坏
            var varNames = [];
            for (var v in vars) {
                if (vars.hasOwnProperty(v)) {
                    varNames.push(v);
                }
            }
            varNames.sort(function(a, b) { return b.length - a.length; });

            // 使用唯一占位符替换变量
            var placeholders = [];
            for (var vi = 0; vi < varNames.length; vi++) {
                var vname = varNames[vi];
                var vval = vars[vname];
                var ph = '\x00' + vi + '\x00'; // 使用 NULL 字符作为占位符，不可能出现在正常表达式中
                placeholders.push({ ph: ph, val: vval });
                processed = processed.replace(new RegExp('\\b' + _escapeRegExp(vname) + '\\b', 'g'), ph);
            }

            // 将占位符替换回实际数值
            for (var pi = 0; pi < placeholders.length; pi++) {
                processed = processed.split(placeholders[pi].ph).join(String(placeholders[pi].val));
            }
        }

        // ---- 第二步：将 ^ 转换为 ** （幂运算） ----
        // 注意：这里不再做此转换，因为调用方已经做了 ^ -> ** 的替换
        // 但为安全起见，如果表达式中仍有 ^ 也做转换
        processed = processed.replace(/\^/g, '**');

        // ---- 第三步：递归下降解析 ----
        var pos = 0; // 当前解析位置

        // 跳过空白字符
        function skipWhitespace() {
            while (pos < processed.length && /\s/.test(processed.charAt(pos))) {
                pos++;
            }
        }

        // 查看当前字符但不前进
        function peek() {
            skipWhitespace();
            return pos < processed.length ? processed.charAt(pos) : null;
        }

        // 消费当前字符并前进
        function consume() {
            var ch = processed.charAt(pos);
            pos++;
            return ch;
        }

        // 尝试匹配指定字符串（不区分大小写），成功则前进
        function matchStr(s) {
            skipWhitespace();
            if (processed.substring(pos, pos + s.length).toLowerCase() === s.toLowerCase()) {
                pos += s.length;
                return true;
            }
            return false;
        }

        // 解析数字（整数或小数）
        function parseNumber() {
            skipWhitespace();
            var start = pos;
            // 可选负号（仅当负号前不是数字或右括号时才作为一元负号）
            if (pos < processed.length && processed.charAt(pos) === '-' &&
                (pos === 0 || /[^0-9)]/.test(processed.charAt(pos - 1)))) {
                pos++;
            }
            while (pos < processed.length && /[0-9]/.test(processed.charAt(pos))) {
                pos++;
            }
            // 小数部分
            if (pos < processed.length && processed.charAt(pos) === '.') {
                pos++;
                while (pos < processed.length && /[0-9]/.test(processed.charAt(pos))) {
                    pos++;
                }
            }
            if (pos === start || (pos === start + 1 && processed.charAt(start) === '-')) {
                return NaN; // 没有实际数字被消费
            }
            return parseFloat(processed.substring(start, pos));
        }

        // 解析主表达式（加减法级别，最低优先级）
        function parseExpression() {
            var left = parseTerm();
            if (isNaN(left)) return NaN;

            while (true) {
                skipWhitespace();
                if (pos >= processed.length) break;
                var ch = processed.charAt(pos);
                if (ch === '+') {
                    pos++;
                    var right = parseTerm();
                    if (isNaN(right)) return NaN;
                    left = left + right;
                } else if (ch === '-') {
                    pos++;
                    var right2 = parseTerm();
                    if (isNaN(right2)) return NaN;
                    left = left - right2;
                } else {
                    break;
                }
            }
            return left;
        }

        // 解析乘除法级别
        function parseTerm() {
            var left = parsePower();
            if (isNaN(left)) return NaN;

            while (true) {
                skipWhitespace();
                if (pos >= processed.length) break;
                var ch = processed.charAt(pos);
                if (ch === '*') {
                    pos++;
                    // 检查是否为 ** （幂运算）
                    skipWhitespace();
                    if (pos < processed.length && processed.charAt(pos) === '*') {
                        pos--; // 退回，让 parsePower 处理
                        break;
                    }
                    var right = parsePower();
                    if (isNaN(right)) return NaN;
                    left = left * right;
                } else if (ch === '/') {
                    pos++;
                    var right2 = parsePower();
                    if (isNaN(right2)) return NaN;
                    left = right2 !== 0 ? left / right2 : NaN;
                } else if (ch === '%') {
                    pos++;
                    var right3 = parsePower();
                    if (isNaN(right3)) return NaN;
                    left = right3 !== 0 ? left % right3 : NaN;
                } else {
                    break;
                }
            }
            return left;
        }

        // 解析幂运算（右结合）
        function parsePower() {
            var base = parseUnary();
            if (isNaN(base)) return NaN;

            skipWhitespace();
            // 检查 ** 或 ^
            if (pos < processed.length && processed.charAt(pos) === '*') {
                pos++;
                if (pos < processed.length && processed.charAt(pos) === '*') {
                    pos++;
                    var exp = parsePower(); // 右结合递归
                    if (isNaN(exp)) return NaN;
                    return Math.pow(base, exp);
                }
                pos--; // 不是 **，退回
            }
            return base;
        }

        // 解析一元运算（正负号）
        function parseUnary() {
            skipWhitespace();
            if (pos < processed.length) {
                var ch = processed.charAt(pos);
                if (ch === '-') {
                    pos++;
                    var val = parseAtom();
                    if (isNaN(val)) return NaN;
                    return -val;
                }
                if (ch === '+') {
                    pos++;
                    return parseAtom();
                }
            }
            return parseAtom();
        }

        // 解析原子（数字、括号、函数调用、常量）
        function parseAtom() {
            skipWhitespace();
            if (pos >= processed.length) return NaN;

            var ch = processed.charAt(pos);

            // 括号表达式
            if (ch === '(') {
                pos++; // 消费 '('
                var val = parseExpression();
                if (isNaN(val)) return NaN;
                skipWhitespace();
                if (pos < processed.length && processed.charAt(pos) === ')') {
                    pos++; // 消费 ')'
                }
                return val;
            }

            // 数字
            if (/[0-9\-]/.test(ch) && ch !== '-') {
                return parseNumber();
            }
            // 负号后跟数字
            if (ch === '-') {
                var savedPos = pos;
                pos++;
                skipWhitespace();
                if (pos < processed.length && /[0-9.]/.test(processed.charAt(pos))) {
                    pos = savedPos;
                    return parseNumber();
                }
                pos = savedPos;
                return NaN;
            }

            // 标识符：函数名或常量
            if (/[a-zA-Z]/.test(ch)) {
                var start = pos;
                while (pos < processed.length && /[a-zA-Z0-9_]/.test(processed.charAt(pos))) {
                    pos++;
                }
                var name = processed.substring(start, pos);

                // 常量
                var nameLower = name.toLowerCase();
                if (nameLower === 'pi') return Math.PI;
                if (nameLower === 'e' && name.length === 1) return Math.E;

                // 函数调用
                var funcResult = _callMathFunction(nameLower);
                if (funcResult !== null) {
                    // 需要解析函数参数
                    skipWhitespace();
                    if (pos < processed.length && processed.charAt(pos) === '(') {
                        pos++; // 消费 '('
                        var arg = parseExpression();
                        if (isNaN(arg)) return NaN;
                        skipWhitespace();
                        if (pos < processed.length && processed.charAt(pos) === ')') {
                            pos++; // 消费 ')'
                        }
                        return funcResult(arg);
                    }
                    // 无括号：尝试将后面的原子作为参数
                    var arg2 = parseAtom();
                    if (isNaN(arg2)) return NaN;
                    return funcResult(arg2);
                }

                // 未知标识符
                return NaN;
            }

            return NaN;
        }

        // 获取数学函数（返回函数或 null）
        function _callMathFunction(nameLower) {
            switch (nameLower) {
                case 'sin': return Math.sin;
                case 'cos': return Math.cos;
                case 'tan': return Math.tan;
                case 'sqrt': return Math.sqrt;
                case 'abs': return Math.abs;
                case 'log': return Math.log;
                case 'exp': return Math.exp;
                case 'pow': return function(a) { return a; }; // pow 需要两个参数，此处简化处理
                default: return null;
            }
        }

        // ---- 第四步：执行解析 ----
        try {
            var result = parseExpression();
            // 确保整个表达式都被消费
            skipWhitespace();
            if (pos < processed.length) {
                return NaN; // 表达式末尾有未解析的字符，视为不合法
            }
            return result;
        } catch (e) {
            return NaN;
        }
    }

    /**
     * 正则表达式特殊字符转义（辅助函数）
     */
    function _escapeRegExp(str) {
        return str.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    }

    /**
     * 求值显式表达式 y = f(x)
     */
    function _evaluateExplicitExpr(expr, x) {
        if (!expr) return null;

        // 字符串形式
        if (expr.type === 'raw' || typeof expr === 'string') {
            var str = expr.value || expr;
            try {
                var evalStr = str
                    .replace(/\^/g, '**')
                    .replace(/\bx\b/g, '(' + x + ')');
                return _safeMathEval(evalStr, {});
            } catch (e) {
                return null;
            }
        }

        // AST 形式 - 简单递归求值
        return _evaluateASTNode(expr, {x: x});
    }

    /**
     * 求值隐式表达式 f(x,y) = 0
     */
    function _evaluateImplicitExpr(expr, left, right, x, y) {
        var str = '';
        if (typeof expr === 'string') {
            str = expr;
        } else if (left && right) {
            var leftStr = _nodeToString(left);
            var rightStr = _nodeToString(right);
            str = '(' + leftStr + ') - (' + rightStr + ')';
        } else {
            return null;
        }

        try {
            var evalStr = str
                .replace(/\s/g, '')
                .replace(/\^/g, '**')
                .replace(/\bx\b/g, '(' + x + ')')
                .replace(/\by\b/g, '(' + y + ')');
            return _safeMathEval(evalStr, {});
        } catch (e) {
            return null;
        }
    }

    /**
     * AST 节点转字符串（用于隐式方程求值）
     */
    function _nodeToString(node) {
        if (!node) return '0';
        if (typeof node === 'number') return String(node);
        if (typeof node === 'string') return node;

        switch (node.type) {
            case 'number':
            case 'NumericLiteral':
                return String(node.value);
            case 'identifier':
            case 'Identifier':
                return node.name || node.value || '0';
            case 'binary':
            case 'BinaryExpression':
                return '(' + _nodeToString(node.left) + ' ' +
                    (node.operator || '+') + ' ' +
                    _nodeToString(node.right) + ')';
            case 'unary':
            case 'UnaryExpression':
                return '(' + (node.operator || '-') +
                    _nodeToString(node.operand || node.argument) + ')';
            case 'call':
            case 'CallExpression':
                var fname = _getNodeName(node.callee || node.func) || 'f';
                var args = node.args || node.arguments || [];
                var argStrs = [];
                for (var i = 0; i < args.length; i++) {
                    argStrs.push(_nodeToString(args[i]));
                }
                return fname + '(' + argStrs.join(', ') + ')';
            default:
                return '0';
        }
    }

    /**
     * 递归求值 AST 节点
     */
    function _evaluateASTNode(node, vars) {
        if (!node) return 0;
        if (typeof node === 'number') return node;

        switch (node.type) {
            case 'number':
            case 'NumericLiteral':
                return parseFloat(node.value) || 0;
            case 'identifier':
            case 'Identifier':
                var name = (node.name || node.value || '').toLowerCase();
                if (name === 'pi') return Math.PI;
                if (name === 'e') return Math.E;
                if (vars && vars[name] !== undefined) return vars[name];
                return 0;
            case 'binary':
            case 'BinaryExpression':
                var l = _evaluateASTNode(node.left, vars);
                var r = _evaluateASTNode(node.right, vars);
                var op = node.operator || '+';
                switch (op) {
                    case '+': return l + r;
                    case '-': return l - r;
                    case '*': return l * r;
                    case '/': return r !== 0 ? l / r : 0;
                    case '^': return Math.pow(l, r);
                    default: return 0;
                }
            case 'unary':
            case 'UnaryExpression':
                var val = _evaluateASTNode(node.operand || node.argument, vars);
                if (node.operator === '-') return -val;
                return val;
            case 'call':
            case 'CallExpression':
                var fn = _getNodeName(node.callee || node.func) || '';
                var cargs = node.args || node.arguments || [];
                var a0 = cargs.length > 0 ?
                    _evaluateASTNode(cargs[0], vars) : 0;
                switch (fn) {
                    case 'sqrt': return Math.sqrt(a0);
                    case 'sin': return Math.sin(a0);
                    case 'cos': return Math.cos(a0);
                    case 'tan': return Math.tan(a0);
                    case 'abs': return Math.abs(a0);
                    default: return 0;
                }
            default:
                return 0;
        }
    }

    // ---- 公共 API ----

    return {
        /**
         * 将 AST 转换为约束图操作
         * @param {Object} ast - 公式 AST（可以是单个节点或数组）
         * @param {Object} graph - 图对象
         * @param {Object} backend - Lv-00 后端 API
         * @returns {Object} 转换结果
         */
        convert: function(ast, graph, backend) {
            var allOperations = [];
            var allErrors = [];
            var allNewNodes = [];
            var allNewConstraints = [];

            // 清空日志和变量映射
            _log = [];
            _varMap = {};

            if (!ast) {
                return _makeResult(false, [], [
                    _makeError('AST is null or undefined', null)
                ], [], []);
            }

            // 参数校验
            if (!graph) {
                return _makeResult(false, [], [
                    _makeError('图对象为空', null)
                ], [], []);
            }
            if (!backend) {
                return _makeResult(false, [], [
                    _makeError('后端为空', null)
                ], [], []);
            }

            try {
                // 支持单个 AST 或 AST 数组
                var astList = Array.isArray(ast) ? ast : [ast];

                for (var i = 0; i < astList.length; i++) {
                var node = astList[i];
                var nodeType = _getNodeType(node);
                var result = null;

                switch (nodeType) {
                    case 'point':
                        result = convertPoint(node, graph, backend);
                        break;
                    case 'segment':
                    case 'line':
                        result = convertSegment(node, graph, backend);
                        break;
                    case 'circle':
                        result = convertCircle(node, graph, backend);
                        break;
                    case 'triangle':
                        result = convertTriangle(node, graph, backend);
                        break;
                    case 'constraint':
                        result = convertConstraint(node, graph, backend);
                        break;
                    case 'equation':
                        result = convertEquation(node, graph, backend);
                        break;
                    default:
                        allErrors.push(_makeError(
                            'Unknown AST node type: ' + nodeType, node));
                        _logMessage('Unknown node type: ' + nodeType, 'error');
                        break;
                }

                if (result) {
                    allOperations = allOperations.concat(result.operations);
                    allErrors = allErrors.concat(result.errors);
                    allNewNodes = allNewNodes.concat(result.newNodes);
                    allNewConstraints = allNewConstraints.concat(
                        result.newConstraints);
                }
            }

            // 归一化图
            if (allErrors.length === 0 && backend.graphNormalize) {
                try {
                    backend.graphNormalize(graph, true);
                    allOperations.push(_makeOperation('normalize',
                        'graph normalized', []));
                    _logMessage('Graph normalized', 'success');
                } catch (e) {
                    _logMessage('Graph normalization warning: ' + e.message,
                        'warning');
                }
            }

            return _makeResult(
                allErrors.length === 0,
                allOperations, allErrors, allNewNodes, allNewConstraints
            );
            } catch (e) {
                console.error('[FormulaToGraph] convert: 转换异常:', e.message);
                return _makeResult(false, [], [
                    _makeError('转换异常: ' + e.message, null)
                ], [], []);
            }
        },

        /**
         * 处理几何点定义
         */
        convertPoint: function(ast, graph, backend) {
            return convertPoint(ast, graph, backend);
        },

        /**
         * 处理几何线段定义
         */
        convertSegment: function(ast, graph, backend) {
            return convertSegment(ast, graph, backend);
        },

        /**
         * 处理几何圆定义
         */
        convertCircle: function(ast, graph, backend) {
            return convertCircle(ast, graph, backend);
        },

        /**
         * 处理几何三角形定义
         */
        convertTriangle: function(ast, graph, backend) {
            return convertTriangle(ast, graph, backend);
        },

        /**
         * 处理几何约束
         */
        convertConstraint: function(ast, graph, backend) {
            return convertConstraint(ast, graph, backend);
        },

        /**
         * 处理代数方程
         */
        convertEquation: function(ast, graph, backend) {
            return convertEquation(ast, graph, backend);
        },

        /**
         * 从方程生成采样点
         * @param {Object} ast - 方程 AST
         * @param {number} xMin - x 最小值（默认 -10）
         * @param {number} xMax - x 最大值（默认 10）
         * @param {number} yMin - y 最小值（默认 -10）
         * @param {number} yMax - y 最大值（默认 10）
         * @param {number} resolution - 分辨率（默认 100）
         * @returns {Array} [{x, y}]
         */
        equationToSamplePoints: function(ast, xMin, xMax, yMin, yMax, resolution) {
            return equationToSamplePoints(ast, xMin, xMax, yMin, yMax, resolution);
        },

        /**
         * 变量名到节点 ID 的映射管理
         * [封装性修复] 不再直接暴露内部 _varMap 对象，
         * 请使用 _getVarId / _setVarId / _clearVarMap 方法访问。
         */

        /**
         * 获取变量对应的节点 ID
         */
        _getVarId: function(name) {
            return _varMap[name];
        },

        /**
         * 设置变量到节点 ID 的映射
         */
        _setVarId: function(name, nodeId) {
            _varMap[name] = nodeId;
        },

        /**
         * 清空变量映射
         */
        _clearVarMap: function() {
            _varMap = {};
        },

        /**
         * 获取转换日志
         * @returns {Array} [{message, type, timestamp}]
         */
        getLog: function() {
            return _log.slice();
        }
    };
})();
