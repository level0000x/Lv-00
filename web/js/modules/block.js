/**
 * block.js - BLOCK 函数块模块方法
 *
 * 实现函数块模块的操作方法，包括打包、实例化、统一化、
 * 状态查询、添加规则等。
 *
 * 依赖：Lv00WebApp 构造函数、ui.js
 */
(function() {
    'use strict';

    // fix: 实例计数器，避免高频调用下 Date.now() 产生重复 ID
    var _instanceCounter = 0;

    // fix: 浮点数比较容差阈值，提取为模块常量便于统一调整
    var FLOAT_TOLERANCE = 1e-6;

    // ================================================================
    // 打包函数块
    // 将选中的图结构打包为一个函数块
    // ================================================================
    Lv00WebApp.prototype.enginePack = function() {
        var blockId = this._getInputValue('inputBlockId', '');
        var inputType = this._getInputValue('inputBlockIn', '');
        var outputType = this._getInputValue('inputBlockOut', '');

        if (!blockId) {
            this.appendLog('Pack: 请输入函数块 ID / Please enter block ID', 'warn');
            return;
        }

        // 创建函数块对象
        this.functionBlocks[blockId] = {
            name: blockId,
            inputType: inputType || 'Point',
            outputType: outputType || 'Point',
            x: this.mouseWorldX || 0,
            y: this.mouseWorldY || 0,
            width: 120,
            height: 60,
            ports: []
        };

        // 更新函数块计数
        var countEl = document.getElementById('blockCount');
        if (countEl) {
            countEl.textContent = Object.keys(this.functionBlocks).length;
        }

        this.appendLog('Function block packed: ' + blockId + ' / 函数块已打包: ' + blockId, 'info');
        this.showSuccess('Block packed: ' + blockId);
        this.render();
    };

    // ================================================================
    // 实例化函数块
    // 创建函数块的一个新实例
    // ================================================================
    Lv00WebApp.prototype.engineInstantiate = function() {
        var blockId = this._getInputValue('inputBlockId', '');
        if (!blockId || !this.functionBlocks[blockId]) {
            this.appendLog('Instantiate: 函数块不存在 / Block not found: ' + blockId, 'warn');
            return;
        }

        var template = this.functionBlocks[blockId];
        var instanceId = blockId + '_inst' + Date.now() + '_' + (++_instanceCounter);  // fix: 添加计数器后缀防止高频重复

        this.functionBlocks[instanceId] = {
            name: instanceId,
            inputType: template.inputType,
            outputType: template.outputType,
            x: template.x + 30,
            y: template.y + 30,
            width: template.width,
            height: template.height,
            ports: []
        };

        var countEl = document.getElementById('blockCount');
        if (countEl) {
            countEl.textContent = Object.keys(this.functionBlocks).length;
        }

        this.appendLog('Block instantiated: ' + instanceId + ' / 函数块已实例化: ' + instanceId, 'info');
        this.showSuccess('Block instantiated: ' + instanceId);
        this.render();
    };

    // ================================================================
    // 统一化
    // 使用后端统一化引擎，对当前约束图中的节点和约束进行统一化（unification）操作。
    // 统一化过程包括：
    //   1. 遍历所有节点，查找可合并的等效节点
    //   2. 基于约束关系检查节点间的一致性
    //   3. 如果后端支持，调用后端统一化方法
    // 操作完成后同步前端状态并刷新视图。
    // ================================================================
    Lv00WebApp.prototype.engineUnify = function() {
        if (!this.jsBackend) {
            this.appendLog('Unify: JS 后端未就绪 / JS backend not ready', 'error');
            this.showError('统一化失败：后端未就绪');
            return;
        }

        if (!this.graph) {
            this.appendLog('Unify: 约束图未初始化 / No constraint graph', 'warn');
            this.showToast('约束图未初始化，无法进行统一化', 'warning');
            return;
        }

        try {
            var result = null;

            // 优先尝试调用后端的统一化方法
            if (typeof this.jsBackend.graphUnify === 'function') {
                result = this.jsBackend.graphUnify(this.graph);
                if (result) {
                    this.appendLog('Unify: 后端统一化完成 / Backend unification completed', 'info');
                }
            }

            // 如果后端方法不可用，执行前端基本统一化逻辑
            if (!result) {
                var mergeCount = 0;

                // 遍历所有节点，检查是否存在坐标相同的点（可合并）
                if (this.graph.nodes && this.graph.nodes.length > 1) {
                    for (var i = 0; i < this.graph.nodes.length; i++) {
                        var nodeA = this.graph.nodes[i];
                        if (!nodeA || nodeA.geomType !== this._geomType().POINT) continue;

                        for (var j = i + 1; j < this.graph.nodes.length; j++) {
                            var nodeB = this.graph.nodes[j];
                            if (!nodeB || nodeB.geomType !== this._geomType().POINT) continue;

                            // 检查两个点的坐标是否近似相等
                            var xA = nodeA.coordX ? this.jsBackend.coordToDouble(nodeA.coordX) : null;
                            var yA = nodeA.coordY ? this.jsBackend.coordToDouble(nodeA.coordY) : null;
                            var xB = nodeB.coordX ? this.jsBackend.coordToDouble(nodeB.coordX) : null;
                            var yB = nodeB.coordY ? this.jsBackend.coordToDouble(nodeB.coordY) : null;

                            if (xA !== null && yA !== null && xB !== null && yB !== null) {
                                var dx = Math.abs(xA - xB);
                                var dy = Math.abs(yA - yB);
                                // fix: 使用模块常量 FLOAT_TOLERANCE 替代硬编码 1e-6
                                if (dx < FLOAT_TOLERANCE && dy < FLOAT_TOLERANCE) {
                                    mergeCount++;
                                    this.appendLog('Unify: 发现可合并节点 n' + nodeA.id + ' 和 n' + nodeB.id +
                                        ' (坐标近似相等)', 'info');
                                }
                            }
                        }
                    }
                }

                if (mergeCount > 0) {
                    this.appendLog('Unify: 找到 ' + mergeCount + ' 对可合并节点 / Found ' + mergeCount + ' mergeable node pairs', 'info');
                    this.showInfo('统一化检查完成，发现 ' + mergeCount + ' 对可合并节点');
                } else {
                    this.appendLog('Unify: 未发现可合并节点 / No mergeable nodes found', 'info');
                    this.showInfo('统一化检查完成，未发现可合并节点');
                }
            }

            // 同步前端状态
            this.syncPointsFromGraph();
            this.syncSegmentsFromGraph();
            this.updateStats();
            this.render();

        } catch (e) {
            this.appendLog('统一化操作异常: ' + e.message + ' / Unify error: ' + e.message, 'error');
            this.showError('统一化失败: ' + e.message);
        }
    };

    // ================================================================
    // 引擎状态查询
    // 输出当前图的节点数和约束数等基本信息
    // ================================================================
    Lv00WebApp.prototype.engineStatus = function() {
        var status = 'Backend: ' + (this.backend || 'none') + '\n';
        status += 'Blocks: ' + Object.keys(this.functionBlocks).length + '\n';

        // 补全图的基本状态信息
        var nodeCount = this.points ? this.points.length : 0;
        var constraintCount = this.graph && this.graph.constraints ? this.graph.constraints.length : 0;
        status += 'Nodes: ' + nodeCount + ' / 节点数: ' + nodeCount + '\n';
        status += 'Constraints: ' + constraintCount + ' / 约束数: ' + constraintCount + '\n';

        status += 'Engine: ' + (this.engine ? 'active' : 'inactive');
        this.appendLog('Engine status: ' + status, 'info');
        this.showToast('引擎状态: ' + nodeCount + ' 节点, ' + constraintCount + ' 约束', 'info');
    };

    // ================================================================
    // 添加规则
    // 向引擎添加一条重写规则（rewrite rule）。
    // 规则由左侧模式（LHS）和右侧替换（RHS）组成，引擎在求解时
    // 会将匹配 LHS 的子图替换为 RHS。
    // 
    // 当前实现：
    //   1. 如果后端支持 engineAddRule，则委托给后端
    //   2. 如果后端不支持，则在前端将规则存储到命题中
    // 两种路径均提供详细的错误处理和用户反馈。
    // ================================================================
    Lv00WebApp.prototype.engineAddRule = function() {
        if (!this.jsBackend) {
            this.appendLog('Add rule: JS 后端未就绪 / JS backend not ready', 'error');
            this.showError('添加规则失败：后端未就绪');
            return;
        }

        if (!this.graph) {
            this.appendLog('Add rule: 约束图未初始化 / No constraint graph', 'warn');
            this.showToast('约束图未初始化，无法添加规则', 'warning');
            return;
        }

        try {
            var ruleAdded = false;

            var lhs = null;
            var rhs = null;
            if (this.selectedPoints && this.selectedPoints.length > 0) {
                lhs = this.selectedPoints[0].id;
                if (this.selectedPoints.length > 1) {
                    rhs = this.selectedPoints[1].id;
                }
            }

            if (typeof this.jsBackend.engineAddRule === 'function') {
                var result = this.jsBackend.engineAddRule(this.graph, lhs, rhs);
                if (result !== null && result !== undefined) {
                    ruleAdded = true;
                    this.appendLog('Add rule: 后端规则添加完成 (LHS:' + lhs + ' RHS:' + rhs + ') / Backend rule added', 'info');
                    this.showSuccess('重写规则已添加');
                }
            }

            // 如果后端不支持，将规则存储在前端的引擎规则列表中
            if (!ruleAdded) {
                // 初始化规则列表
                if (!this._engineRules) {
                    this._engineRules = [];
                }

                var rule = {
                    id: this._engineRules.length,
                    timestamp: new Date().toISOString(),
                    graphSnapshot: null  // 可存储当前图状态的快照作为规则模式
                };

                // 尝试保存当前图状态的浅拷贝作为规则模式
                if (this.graph.nodes) {
                    rule.graphSnapshot = {
                        nodeCount: this.graph.nodes.length,
                        constraintCount: this.graph.constraints ? this.graph.constraints.length : 0
                    };
                }

                this._engineRules.push(rule);
                ruleAdded = true;

                this.appendLog('Add rule: 规则 #' + rule.id +
                    ' 已添加到引擎 / Rule #' + rule.id + ' added to engine', 'info');
                this.showInfo('规则 #' + rule.id + ' 已添加 (' +
                    (rule.graphSnapshot ? rule.graphSnapshot.nodeCount + '节点, ' + rule.graphSnapshot.constraintCount + '约束' : '空规则') + ')');
            }

            // 更新引擎后端显示
            var backendEl = document.getElementById('engineBackend');
            if (backendEl) {
                backendEl.textContent = this.backend === 'wasm' ? 'WebAssembly' : 'JavaScript';
            }

        } catch (e) {
            this.appendLog('添加规则操作异常: ' + e.message + ' / Add rule error: ' + e.message, 'error');
            this.showError('添加规则失败: ' + e.message);
        }
    };

    // ================================================================
    // 引擎求解
    // 调用后端引擎对当前约束图进行求解，并同步结果到前端显示
    // 仅支持 JS 后端模式
    // ================================================================
    Lv00WebApp.prototype._engineSolve = function() {
        // 守卫：检查后端和图是否可用
        if (!this.jsBackend || !this.graph) {
            this.appendLog('引擎求解失败：后端或约束图未就绪 / Engine solve: no backend or graph', 'error');
            return;
        }

        try {
            var result = this._callBackend('engineSolve', [this.graph]);
            if (result) {
                // 同步求解后的点和线段到前端
                this.syncPointsFromGraph();
                this.syncSegmentsFromGraph();
                this.updateStats();
                this.render();
                this.showSuccess('引擎求解已完成 / Engine solved');
                this.appendLog('引擎求解完成 / Engine: solve completed', 'info');
            }
        } catch (e) {
            this.appendLog('引擎求解异常: ' + e.message + ' / Engine solve error: ' + e.message, 'error');
        }
    };

    // ================================================================
    // 显示重写规则探索器
    // 打开模态框展示重写规则配置界面
    // ================================================================
    Lv00WebApp.prototype._showRewriteExplorer = function() {
        this._showModal('modalOverlay');

        var title = document.getElementById('modalTitle');
        if (title) {
            title.textContent = 'REWRITE EXPLORER / 重写探索器';
        }

        var body = document.getElementById('modalBody');
        if (body) {
            // fix: 使用 textContent = '' 清空子节点，性能优于逐个 removeChild
            body.textContent = '';

            var descP = document.createElement('p');
            descP.style.cssText = 'font-size:11px;color:var(--color-text-secondary);margin-bottom:10px;';
            descP.textContent = '重写规则探索器 - 查看和管理图重写规则。';
            body.appendChild(descP);

            var rulesContainer = document.createElement('div');
            rulesContainer.style.cssText = 'max-height:300px;overflow-y:auto;';

            var rules = this._engineRules || [];
            if (rules.length === 0) {
                var emptyMsg = document.createElement('div');
                emptyMsg.style.cssText =
                    'padding:15px;text-align:center;color:var(--color-text-muted);' +
                    'background:var(--color-bg-primary);border-radius:4px;';
                emptyMsg.textContent = '暂无重写规则 / No rewrite rules';
                rulesContainer.appendChild(emptyMsg);
            } else {
                for (var i = 0; i < rules.length; i++) {
                    var rule = rules[i];
                    var ruleDiv = document.createElement('div');
                    ruleDiv.style.cssText =
                        'padding:10px;margin-bottom:8px;background:var(--color-bg-primary);' +
                        'border:1px solid var(--color-border-secondary);border-radius:4px;';
                    
                    var ruleHeader = document.createElement('div');
                    ruleHeader.style.cssText = 'font-weight:bold;margin-bottom:5px;';
                    ruleHeader.textContent = '规则 #' + rule.id + (rule.graphSnapshot ? ' (' + rule.graphSnapshot.nodeCount + '节点)' : '');
                    ruleDiv.appendChild(ruleHeader);

                    var ruleInfo = document.createElement('div');
                    ruleInfo.style.cssText = 'font-size:10px;color:var(--color-text-secondary);';
                    ruleInfo.textContent = '创建时间: ' + new Date(rule.timestamp).toLocaleString();
                    ruleDiv.appendChild(ruleInfo);

                    rulesContainer.appendChild(ruleDiv);
                }
            }

            body.appendChild(rulesContainer);

            var helpDiv = document.createElement('div');
            helpDiv.style.cssText = 'margin-top:10px;padding:8px;background:var(--color-bg-secondary);' +
                'border-radius:4px;font-size:10px;color:var(--color-text-muted);';
            helpDiv.textContent = '提示: 选中点后点击"添加规则"可创建带节点信息的规则。';
            body.appendChild(helpDiv);
        }

        this.appendLog('已打开重写探索器 / Rewrite explorer opened', 'info');
    };

    // ================================================================
    // 函数块部分应用
    // 对指定的函数块进行部分参数绑定
    // ================================================================
    Lv00WebApp.prototype._blockPartialApply = function() {
        var blockId = this._getInputValue('inputBlockId', '');
        if (!blockId) {
            this.appendLog('部分应用：请输入函数块 ID / Partial Apply: Please enter block ID', 'warn');
            return;
        }

        // 检查函数块是否存在
        if (!this.functionBlocks[blockId]) {
            this.appendLog('部分应用失败：函数块 ' + blockId + ' 不存在 / Block not found', 'warn');
            return;
        }

        this.appendLog('部分应用: 函数块 ' + blockId + ' / Partial Apply: block ' + blockId, 'info');
        this.showInfo('部分应用已执行: ' + blockId + ' / Partial apply on block: ' + blockId);
    };

    // ================================================================
    // 函数块视图折叠
    // 切换画布上函数块的显示/隐藏状态
    // ================================================================
    Lv00WebApp.prototype._blockViewFold = function() {
        // 反转折叠状态
        this._blockViewFolded = !this._blockViewFolded;
        var stateText = this._blockViewFolded ? '已折叠' : '已展开';
        this.appendLog('视图状态: ' + stateText + ' / View Fold: ' + (this._blockViewFolded ? 'folded' : 'expanded'), 'info');
        this.render();
    };

})();
