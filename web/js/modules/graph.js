/**
 * graph.js - GRAPH 图模块方法
 *
 * 实现图模块的所有操作方法，包括节点/线段/区域的增删改查、
 * 约束操作（关联、之间、相交、包含）、分析操作（归一化、
 * 合并候选、冗余检测、冲突检测、自由度、拓扑排序、哈希）。
 *
 * 依赖：Lv00WebApp 构造函数、ui.js、undo.js、interaction.js
 */
(function() {
    'use strict';

    // ================================================================
    // 添加线段
    // 使用选中的两个点创建线段，或使用输入框中的坐标
    // ================================================================
    Lv00WebApp.prototype.graphAddSegment = function() {
        // 优先使用选中的两个点
        if (this.selectedPoints && this.selectedPoints.length === 2) {
            this.addSegment(this.selectedPoints[0].id, this.selectedPoints[1].id);
            this.selectedPoints = [];
            return;
        }

        // 如果只有一个选中点，提示选择第二个点
        if (this.selectedPoint) {
            this.appendLog('SELECT SECOND POINT / 请选择第二个端点', 'info');
            return;
        }

        // 尝试从输入框获取坐标
        var x = this._getInputFloat('inputPointX', NaN);
        var y = this._getInputFloat('inputPointY', NaN);
        if (!isNaN(x) && !isNaN(y)) {
            // 先添加点，再添加线段
            var id1 = this.addPoint(x, y);
            this.appendLog('Now click another point or enter coordinates for second endpoint / 请点击第二个端点', 'info');
        } else {
            this.appendLog('Please select two points or enter coordinates / 请选择两个点或输入坐标', 'warn');
        }
    };

    // ================================================================
    // 添加区域
    // 将当前选中的点创建为一个区域
    // ================================================================
    Lv00WebApp.prototype.graphAddRegion = function() {
        if (!this.selectedPoints || this.selectedPoints.length < 3) {
            this.appendLog('Need at least 3 selected points for a region / 至少需要3个选中点来创建区域', 'warn');
            return;
        }

        this._saveUndoState();
        this.regions.push({
            points: this.selectedPoints.slice()
        });

        this.appendLog('Region created with ' + this.selectedPoints.length + ' points / 区域已创建（' + this.selectedPoints.length + ' 个点）', 'info');
        this.render();
    };

    // ================================================================
    // 删除节点
    // 删除选中的点或通过输入框指定 ID 的节点
    // ================================================================
    Lv00WebApp.prototype.graphRemoveNode = function() {
        if (!this.jsBackend || !this.graph) {
            this.appendLog('Remove node: no backend / 删除节点：无后端', 'error');
            return;
        }

        var nodeId;
        if (this.selectedPoint) {
            nodeId = this.selectedPoint.id;
        } else {
            nodeId = this._getInputInt('inputDeleteNodeId', -1);
        }

        if (nodeId < 0) {
            this.appendLog('Remove node: no node selected or invalid ID / 删除节点：未选择节点或 ID 无效', 'warn');
            return;
        }

        this._saveUndoState();
        var result = this._callBackend('graphRemoveNode', [this.graph, nodeId]);

        // fix: 使用 != null 同时检查 null 和 undefined（JS 隐式类型转换）
        if (result != null) {
            this.syncPointsFromGraph();
            this.syncSegmentsFromGraph();
            this.selectedPoint = null;
            this.selectedPoints = [];
            this.updateStats();
            this.updateProperties(null);
            this.appendLog('Node n' + nodeId + ' removed / 节点 n' + nodeId + ' 已删除', 'info');
            this.render();

            // 节点删除后同步公式显示
            if (typeof this.formulaSyncFromGraph === 'function') {
                this.formulaSyncFromGraph();
            }
        }
    };

    // ================================================================
    // 删除约束
    // 通过输入框指定 ID 删除约束
    // ================================================================
    Lv00WebApp.prototype.graphRemoveConstraint = function() {
        if (!this.jsBackend || !this.graph) {
            this.appendLog('Remove constraint: no backend / 删除约束：无后端', 'error');
            return;
        }

        var constraintId = this._getInputInt('inputDeleteConstraintId', -1);
        if (constraintId < 0) {
            this.appendLog('Remove constraint: invalid ID / 删除约束：ID 无效', 'warn');
            return;
        }

        this._saveUndoState();
        var result = this._callBackend('graphRemoveConstraint', [this.graph, constraintId]);

        if (result !== null && result !== undefined) {
            this.updateStats();
            this.appendLog('Constraint ' + constraintId + ' removed / 约束 ' + constraintId + ' 已删除', 'info');
            this.render();
        }
    };

    // ================================================================
    // 添加关联约束（Incidence）
    // 点在线段上
    // ================================================================
    Lv00WebApp.prototype.graphAddIncidence = function() {
        if (!this.jsBackend || !this.graph) {
            this.appendLog('Add incidence: no backend', 'error');
            return;
        }
        this._saveUndoState();
        var result = this._callBackend('graphAddIncidenceConstraint', [this.graph]);
        if (result !== null && result !== undefined) {
            this.updateStats();
            this.appendLog('Incidence constraint added / 关联约束已添加', 'info');
            this.render();
        }
    };

    // ================================================================
    // 添加之间约束（Betweenness）
    // 点 B 在点 A 和点 C 之间
    // ================================================================
    Lv00WebApp.prototype.graphAddBetweenness = function() {
        if (!this.jsBackend || !this.graph) {
            this.appendLog('Add betweenness: no backend', 'error');
            return;
        }
        this._saveUndoState();
        var result = this._callBackend('graphAddBetweennessConstraint', [this.graph]);
        if (result !== null && result !== undefined) {
            this.updateStats();
            this.appendLog('Betweenness constraint added / 之间约束已添加', 'info');
            this.render();
        }
    };

    // ================================================================
    // 添加相交约束（Intersection）
    // 两条线段相交
    // ================================================================
    Lv00WebApp.prototype.graphAddIntersection = function() {
        if (!this.jsBackend || !this.graph) {
            this.appendLog('Add intersection: no backend', 'error');
            return;
        }
        this._saveUndoState();
        var result = this._callBackend('graphAddIntersectionConstraint', [this.graph]);
        if (result !== null && result !== undefined) {
            this.updateStats();
            this.appendLog('Intersection constraint added / 相交约束已添加', 'info');
            this.render();
        }
    };

    // ================================================================
    // 添加包含约束（Containment）
    // 点在区域内
    // ================================================================
    Lv00WebApp.prototype.graphAddContainment = function() {
        if (!this.jsBackend || !this.graph) {
            this.appendLog('Add containment: no backend', 'error');
            return;
        }
        this._saveUndoState();
        var result = this._callBackend('graphAddContainmentConstraint', [this.graph]);
        if (result !== null && result !== undefined) {
            this.updateStats();
            this.appendLog('Containment constraint added / 包含约束已添加', 'info');
            this.render();
        }
    };

    // ================================================================
    // 规范化图
    // 对图进行规范化处理，合并等价节点
    // ================================================================
    Lv00WebApp.prototype.graphNormalize = function() {
        if (!this.jsBackend || !this.graph) {
            this.appendLog('Normalize: no backend', 'error');
            return;
        }
        this._saveUndoState();
        var result = this._callBackend('graphNormalize', [this.graph]);
        if (result !== null && result !== undefined) {
            this.syncPointsFromGraph();
            this.syncSegmentsFromGraph();
            this.updateStats();
            this.appendLog('Graph normalized / 图已规范化', 'info');
            this.render();
        }
    };

    // ================================================================
    // 查找合并候选
    // 分析图中可以合并的等价节点对
    // ================================================================
    Lv00WebApp.prototype.graphFindMergeCandidates = function() {
        if (!this.jsBackend || !this.graph) {
            this.appendLog('Find merge: no backend', 'error');
            return;
        }
        var result = this._callBackend('graphFindMergeCandidates', [this.graph]);
        if (result !== null && result !== undefined) {
            // fix: 大数据时避免完整 JSON.stringify，防止日志过长
            var mergeStr = (result && typeof result.length === 'number' && result.length > 50)
                ? '[大型数据，已省略序列化 / large data omitted, length=' + result.length + ']'
                : JSON.stringify(result);
            this.appendLog('Merge candidates: ' + mergeStr + ' / 合并候选已找到', 'info');
        }
    };

    // ================================================================
    // 检测冗余约束
    // 查找图中可以被其他约束推导出的冗余约束
    // ================================================================
    Lv00WebApp.prototype.graphDetectRedundant = function() {
        if (!this.jsBackend || !this.graph) {
            this.appendLog('Detect redundant: no backend', 'error');
            return;
        }
        var result = this._callBackend('graphDetectRedundant', [this.graph]);
        if (result !== null && result !== undefined) {
            // fix: 大数据时避免完整 JSON.stringify，防止日志过长
            var redundantStr = (result && typeof result.length === 'number' && result.length > 50)
                ? '[大型数据，已省略序列化 / large data omitted, length=' + result.length + ']'
                : JSON.stringify(result);
            this.appendLog('Redundant constraints: ' + redundantStr + ' / 冗余约束已检测', 'info');
        }
    };

    // ================================================================
    // 检测冲突
    // 查找图中互相矛盾的约束
    // ================================================================
    Lv00WebApp.prototype.graphDetectConflicts = function() {
        if (!this.jsBackend || !this.graph) {
            this.appendLog('Detect conflicts: no backend', 'error');
            return;
        }
        var result = this._callBackend('graphDetectConflicts', [this.graph]);
        if (result !== null && result !== undefined) {
            // fix: 大数据时避免完整 JSON.stringify，防止日志过长
            var conflictStr = (result && typeof result.length === 'number' && result.length > 50)
                ? '[大型数据，已省略序列化 / large data omitted, length=' + result.length + ']'
                : JSON.stringify(result);
            this.appendLog('Conflicts: ' + conflictStr + ' / 冲突已检测', 'info');
        }
    };

    // ================================================================
    // 计算自由度
    // 计算图的自由度（Degrees of Freedom）
    // ================================================================
    Lv00WebApp.prototype.graphDegreesOfFreedom = function() {
        if (!this.jsBackend || !this.graph) {
            this.appendLog('DOF: no backend', 'error');
            return;
        }
        var result = this._callBackend('graphDegreesOfFreedom', [this.graph]);
        if (result !== null && result !== undefined) {
            this.appendLog('Degrees of Freedom: ' + result + ' / 自由度: ' + result, 'info');
            this.showInfo('DOF: ' + result);
        }
    };

    // ================================================================
    // 拓扑排序
    // 对图的约束进行拓扑排序
    // ================================================================
    Lv00WebApp.prototype.graphTopologicalSort = function() {
        if (!this.jsBackend || !this.graph) {
            this.appendLog('Topo sort: no backend', 'error');
            return;
        }
        var result = this._callBackend('graphTopologicalSort', [this.graph]);
        if (result !== null && result !== undefined) {
            this.appendLog('Topological sort: ' + JSON.stringify(result) + ' / 拓扑排序完成', 'info');
        }
    };

    // ================================================================
    // 计算图哈希
    // 生成图的唯一哈希值，用于比较图的等价性
    // ================================================================
    Lv00WebApp.prototype.graphHash = function() {
        if (!this.jsBackend || !this.graph) {
            this.appendLog('Graph hash: no backend', 'error');
            return;
        }
        var result = this._callBackend('graphHash', [this.graph]);
        if (result !== null && result !== undefined) {
            this.appendLog('Graph hash: ' + result + ' / 图哈希: ' + result, 'info');
            this.showInfo('Graph Hash: ' + result);
        }
    };

})();
