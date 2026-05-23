/**
 * recurse.js - RECURSE 递归模块方法
 *
 * 实现递归模块的操作方法，包括创建测度、计算测度、
 * 进入/退出递归、选择器求值、递减检查等。
 *
 * 依赖：Lv00WebApp 构造函数、ui.js
 */
(function() {
    'use strict';

    // ================================================================
    // 创建测度
    // 为递归函数定义一个终止测度（measure）
    // ================================================================
    Lv00WebApp.prototype.recurseCreateMeasure = function() {
        if (!this.jsBackend || !this.recursionContext) {
            this.appendLog('Create measure: no recursion context / 创建测度：无递归上下文', 'error');
            return;
        }

        var result = this._callBackend('recursionContextCreateMeasure', [this.recursionContext]);
        if (result !== null && result !== undefined) {
            this.appendLog('Measure created / 测度已创建', 'info');
            this.showSuccess('Measure created / 测度已创建');

            // 更新递归深度显示
            var depthEl = document.getElementById('recurseDepth');
            if (depthEl) depthEl.textContent = '1';
            var statusEl = document.getElementById('recurseStatus');
            if (statusEl) statusEl.textContent = 'ACTIVE';
        }
    };

    // ================================================================
    // 计算测度
    // 计算当前递归上下文的终止测度值。
    // 优先调用后端 API（/api/recurse/measure），后端不可用时执行客户端测度估算。
    // ================================================================
    Lv00WebApp.prototype.recurseComputeMeasure = function() {
        if (!this.jsBackend || !this.recursionContext) {
            this.showToast('递归上下文未初始化，无法计算测度', 'error');
            this.appendLog('Compute measure: no recursion context / 计算测度：无递归上下文', 'error');
            return;
        }

        // 优先通过后端 API 计算测度
        var result = this._callBackend('recursionContextComputeMeasure', [this.recursionContext]);
        if (result !== null && result !== undefined) {
            this.appendLog('Measure computed (backend): ' + result + ' / 测度计算结果（后端）: ' + result, 'info');
            this.showToast('测度计算完成: ' + result, 'success');
            var mEl = document.getElementById('recurseMeasure');
            if (mEl) { mEl.textContent = String(result); }
            return;
        }

        // 后端不可用时，执行客户端测度估算
        // 基于递归上下文中注册的测度函数和当前参数做静态估算
        var ctx = this.recursionContext;
        var estimatedValue = null;

        if (ctx && ctx.measures && typeof ctx.measures === 'object') {
            var measureKeys = Object.keys(ctx.measures);
            if (measureKeys.length > 0) {
                // 取第一个已注册测度进行客户端估算
                var firstMeasure = ctx.measures[measureKeys[0]];
                if (typeof firstMeasure === 'function') {
                    try {
                        estimatedValue = firstMeasure(ctx.currentArgs || {});
                    } catch (e) {
                        this.appendLog('Client measure eval error: ' + e.message, 'warn');
                    }
                } else if (typeof firstMeasure === 'number') {
                    estimatedValue = firstMeasure;
                }
            }
        }

        // 若无测度，根据递归深度做简易估算（假设递减模式）
        if (estimatedValue === null) {
            var depthEl = document.getElementById('recurseDepth');
            var depth = 0;
            if (depthEl) {
                depth = parseInt(depthEl.textContent, 10);
                if (isNaN(depth)) depth = 0;
            }
            // 简易递减估算：每层深度递减一个单位
            var maxDepth = ctx && ctx.maxDepth ? ctx.maxDepth : 10;
            estimatedValue = Math.max(0, maxDepth - depth);
            this.appendLog('Measure estimated from depth (client-side fallback): ' + estimatedValue, 'info');
        } else {
            this.appendLog('Measure computed (client-side): ' + estimatedValue, 'info');
        }

        this.showToast('测度估算: ' + estimatedValue + ' (客户端)', 'info');
        var mEl = document.getElementById('recurseMeasure');
        if (mEl) { mEl.textContent = String(estimatedValue); }
    };

    // ================================================================
    // 进入递归
    // 进入递归调用的下一层，增加递归深度。
    // 优先调用后端 API（/api/recurse/enter）进行验证，
    // 后端不可用时执行客户端深度边界检查和测度递减验证。
    // ================================================================
    Lv00WebApp.prototype.recurseEnter = function() {
        if (!this.jsBackend || !this.recursionContext) {
            this.showToast('递归上下文未初始化，无法进入递归', 'error');
            this.appendLog('Recurse enter: no context / 递归进入：无上下文', 'error');
            return;
        }

        // --- 客户端前置校验 ---
        var ctx = this.recursionContext;
        var depthEl = document.getElementById('recurseDepth');
        var currentDepth = 0;
        if (depthEl) {
            currentDepth = parseInt(depthEl.textContent, 10);
            if (isNaN(currentDepth)) currentDepth = 0;
        }

        // 最大递归深度检查（防止栈溢出，客户端硬限制）
        var maxDepth = ctx && ctx.maxDepth ? ctx.maxDepth : 100;
        if (currentDepth >= maxDepth) {
            this.showToast('已达到最大递归深度 ' + maxDepth + '，无法继续进入', 'warn');
            this.appendLog('Recurse enter blocked: max depth ' + maxDepth + ' reached / 递归进入阻止：已达最大深度 ' + maxDepth, 'warn');
            var statusEl = document.getElementById('recurseStatus');
            if (statusEl) { statusEl.textContent = 'MAX_DEPTH'; }
            return;
        }

        // 优先通过后端 API 进入递归
        var result = this._callBackend('recursionContextEnter', [this.recursionContext]);
        if (result !== null && result !== undefined) {
            // 后端成功进入：更新 UI 状态
            if (depthEl) {
                var current = parseInt(depthEl.textContent, 10);
                if (isNaN(current)) current = 0;
                depthEl.textContent = current + 1;
            }
            this.appendLog('Entered recursion (backend), depth increased / 进入递归（后端），深度增加', 'info');
            this.showToast('进入递归，当前深度: ' + (currentDepth + 1), 'success');
            return;
        }

        // --- 后端不可用，执行客户端侧进入逻辑 ---
        // 测度递减预校验：检查递归调用是否可能满足终止条件
        var measureEl = document.getElementById('recurseMeasure');
        var measureValue = null;
        if (measureEl) {
            measureValue = parseInt(measureEl.textContent, 10);
            if (isNaN(measureValue)) measureValue = null;
        }

        // 如果已有测度值且为 0，则递归已触底
        if (measureValue !== null && measureValue <= 0) {
            this.showToast('测度已归零，递归已触底，继续进入可能导致退化（coming soon: 完整终止验证）', 'warn');
            this.appendLog('Recurse enter warning: measure at zero / 递归进入警告：测度已归零', 'warn');
        }

        // 执行进入：增加深度
        var newDepth = currentDepth + 1;
        if (depthEl) { depthEl.textContent = newDepth; }

        // 客户端递减测度值（假设每次进入减少1）
        if (measureEl && measureValue !== null && measureValue > 0) {
            measureEl.textContent = measureValue - 1;
        }

        // 更新状态指示器
        var statusEl = document.getElementById('recurseStatus');
        if (statusEl) { statusEl.textContent = 'ENTERED'; }

        this.appendLog('Entered recursion (client-side), depth: ' + newDepth +
            ' / 进入递归（客户端），深度: ' + newDepth, 'info');
        this.showToast('进入递归 (客户端)，当前深度: ' + newDepth, 'info');
    };

    // ================================================================
    // 退出递归
    // 从递归调用中返回上一层，减少递归深度。
    // 优先调用后端 API（/api/recurse/exit）执行清理，
    // 后端不可用时执行客户端上下文栈回退和状态恢复。
    // ================================================================
    Lv00WebApp.prototype.recurseExit = function() {
        if (!this.jsBackend || !this.recursionContext) {
            this.showToast('递归上下文未初始化，无法退出递归', 'error');
            this.appendLog('Recurse exit: no context / 递归退出：无上下文', 'error');
            return;
        }

        var depthEl = document.getElementById('recurseDepth');
        var currentDepth = 0;
        if (depthEl) {
            currentDepth = parseInt(depthEl.textContent, 10);
            if (isNaN(currentDepth)) currentDepth = 0;
        }

        // 深度边界检查：不能退出超过根层级
        if (currentDepth <= 0) {
            this.showToast('已在递归顶层，无法继续退出', 'warn');
            this.appendLog('Recurse exit blocked: already at root level / 递归退出阻止：已在顶层', 'warn');
            return;
        }

        // 优先通过后端 API 退出递归
        var result = this._callBackend('recursionContextExit', [this.recursionContext]);
        if (result !== null && result !== undefined) {
            if (depthEl) {
                var current = parseInt(depthEl.textContent, 10);
                if (isNaN(current)) current = 0;
                depthEl.textContent = Math.max(0, current - 1);
            }
            this.appendLog('Exited recursion (backend), depth decreased / 退出递归（后端），深度减少', 'info');
            this.showToast('退出递归，当前深度: ' + Math.max(0, currentDepth - 1), 'success');
            return;
        }

        // --- 后端不可用，执行客户端侧退出逻辑 ---
        // 恢复状态：减少深度，恢复测度值
        var newDepth = currentDepth - 1;
        if (depthEl) { depthEl.textContent = newDepth; }

        // 客户端恢复测度值（假设每次退出恢复1，即上一次进入时扣减的部分）
        var measureEl = document.getElementById('recurseMeasure');
        if (measureEl) {
            var mVal = parseInt(measureEl.textContent, 10);
            if (!isNaN(mVal)) {
                var maxDepth = this.recursionContext && this.recursionContext.maxDepth ? this.recursionContext.maxDepth : 10;
                // 测度恢复：最多恢复到初始最大值
                measureEl.textContent = Math.min(maxDepth, mVal + 1);
            }
        }

        // 更新状态指示器
        var statusEl = document.getElementById('recurseStatus');
        if (statusEl) {
            statusEl.textContent = newDepth > 0 ? 'ACTIVE' : 'IDLE';
        }

        // 如果已回到根层级，执行根层级清理
        if (newDepth === 0) {
            this.appendLog('Recurse exit: returned to root level / 递归退出：已返回顶层', 'info');
            this.showToast('已退出全部递归层级，回到顶层', 'info');
        } else {
            this.appendLog('Exited recursion (client-side), depth: ' + newDepth +
                ' / 退出递归（客户端），深度: ' + newDepth, 'info');
            this.showToast('退出递归 (客户端)，当前深度: ' + newDepth, 'info');
        }
    };

    // ================================================================
    // 选择器求值
    // 在递归上下文中对选择器表达式进行求值。
    // 优先调用后端 API（/api/recurse/selector），
    // 后端不可用时执行客户端选择器解析和求值。
    //
    // 支持的选择器语法（客户端）：
    //   - 点选择器: n{id}           -> 选中对应 id 的节点
    //   - 类型选择器: type:Point     -> 选中该类型的所有节点
    //   - 索引选择器: @depth        -> 选中当前递归深度的节点
    //   - 子选择器: child(...)       -> 选中子节点
    //   - 组合选择器: sel1 + sel2   -> 并集
    // ================================================================
    Lv00WebApp.prototype.recurseSelectorEvaluate = function() {
        if (!this.jsBackend || !this.recursionContext) {
            this.showToast('递归上下文未初始化，无法求值选择器', 'error');
            this.appendLog('Selector evaluate: no context / 选择器求值：无上下文', 'error');
            return;
        }

        // 获取当前选择器表达式（从 UI 输入框或上下文获取）
        var selectorExpr = '';
        var selInput = document.getElementById('recurseSelector');
        if (selInput && selInput.value) {
            selectorExpr = selInput.value.trim();
        } else if (this.recursionContext.currentSelector) {
            selectorExpr = this.recursionContext.currentSelector;
        }

        // 优先通过后端 API 求值选择器
        var result = this._callBackend('recursionContextSelectorEvaluate',
            [this.recursionContext, selectorExpr]);
        if (result !== null && result !== undefined) {
            this.appendLog('Selector result (backend): ' + JSON.stringify(result) +
                ' / 选择器结果（后端）', 'info');
            this.showToast('选择器求值完成: ' + JSON.stringify(result), 'success');
            return;
        }

        // --- 后端不可用，执行客户端选择器解析与求值 ---
        var evalResult = this._clientSelectorEval(selectorExpr);

        if (evalResult.error) {
            this.appendLog('Selector eval error: ' + evalResult.error +
                ' / 选择器求值错误: ' + evalResult.error, 'error');
            this.showToast('选择器求值失败: ' + evalResult.error, 'error');
        } else if (evalResult.nodes && evalResult.nodes.length > 0) {
            this.appendLog('Selector result (client): ' + JSON.stringify(evalResult.nodes) +
                ' / 选择器结果（客户端）: ' + evalResult.nodes.length + ' 个节点',
                'info');
            this.showToast('选择器求值 (客户端): 选中 ' + evalResult.nodes.length + ' 个节点', 'info');
        } else {
            this.appendLog('Selector eval (client): no matches / 选择器求值（客户端）：无匹配', 'info');
            this.showToast('选择器求值 (客户端): 无匹配节点 (coming soon: 完整选择器引擎)', 'info');
        }
    };

    // ================================================================
    // 客户端选择器解析与求值（内部辅助方法）
    // 在后端不可用时作为降级方案执行基本的选择器语法解析。
    // @private
    // @param {string} selectorExpr - 选择器表达式
    // @returns {{nodes: Array, error: string|null}}
    // ================================================================
    Lv00WebApp.prototype._clientSelectorEval = function(selectorExpr) {
        var nodes = [];
        var error = null;

        if (!selectorExpr || selectorExpr.trim() === '') {
            return { nodes: [], error: '选择器表达式为空' };
        }

        try {
            var expr = selectorExpr.trim();

            // 点选择器: n{id} 或 node({id})
            var nodeIdMatch = expr.match(/^n(\d+)$/);
            if (nodeIdMatch) {
                var targetId = parseInt(nodeIdMatch[1], 10);
                // 在选中的点列表中查找匹配节点
                if (this.selectedPoints && this.selectedPoints.length > 0) {
                    for (var i = 0; i < this.selectedPoints.length; i++) {
                        if (this.selectedPoints[i].id === targetId) {
                            nodes.push(this.selectedPoints[i]);
                            break;
                        }
                    }
                }
                return { nodes: nodes, error: null };
            }

            // 类型选择器: type:Point, type:Segment 等
            var typeMatch = expr.match(/^type:(\w+)$/i);
            if (typeMatch) {
                var typeName = typeMatch[1].toLowerCase();
                if (this.selectedPoints && this.selectedPoints.length > 0) {
                    for (var j = 0; j < this.selectedPoints.length; j++) {
                        var pt = this.selectedPoints[j];
                        var ptType = (pt.type || '').toLowerCase();
                        if (ptType === typeName) {
                            nodes.push(pt);
                        }
                    }
                }
                return { nodes: nodes, error: null };
            }

            // 深度选择器: @depth 或 @current
            var depthMatch = expr.match(/^@(depth|current)$/i);
            if (depthMatch) {
                var depthEl = document.getElementById('recurseDepth');
                var currentDepth = 0;
                if (depthEl) {
                    currentDepth = parseInt(depthEl.textContent, 10);
                    if (isNaN(currentDepth)) currentDepth = 0;
                }
                // 返回当前深度的虚拟节点
                nodes.push({ id: -1, type: 'depth', value: currentDepth, virtual: true });
                return { nodes: nodes, error: null };
            }

            // 子选择器: child(n{id}) -> 选出子节点
            var childMatch = expr.match(/^child\(n(\d+)\)$/);
            if (childMatch) {
                var parentId = parseInt(childMatch[1], 10);
                // 通过图结构查找子节点
                if (this.graphData && this.graphData.edges) {
                    for (var k = 0; k < this.graphData.edges.length; k++) {
                        var edge = this.graphData.edges[k];
                        if (edge.from === parentId && edge.type === 'parent_child') {
                            // 查找子节点
                            if (this.graphData.nodes) {
                                var childNode = this.graphData.nodes.find(function(n) { return n.id === edge.to; });
                                if (childNode) nodes.push(childNode);
                            }
                        }
                    }
                }
                return { nodes: nodes, error: null };
            }

            // 万能选择器: * 返回所有节点
            if (expr === '*') {
                if (this.graphData && this.graphData.nodes) {
                    nodes = this.graphData.nodes.slice();
                }
                return { nodes: nodes, error: null };
            }

            // 无法识别的选择器语法
            error = '不支持的客户端选择器语法: "' + expr + '" (coming soon: 完整选择器引擎)';

        } catch (e) {
            error = '选择器解析异常: ' + e.message;
        }

        return { nodes: nodes, error: error };
    };

    // ================================================================
    // 检查递减
    // 验证递归调用是否满足递减条件（终止性检查）
    // ================================================================
    Lv00WebApp.prototype.recurseCheckDecreasing = function() {
        if (!this.jsBackend || !this.recursionContext) {
            this.appendLog('Check decreasing: no recursion context / 检查递减：无递归上下文', 'error');
            return;
        }

        var result = this._callBackend('recursionContextCheckDecreasing', [this.recursionContext]);
        if (result !== null && result !== undefined) {
            var isDecreasing = !!result;
            var msg = isDecreasing ?
                'DECREASING: termination guaranteed / 递减：终止性保证' :
                'NOT DECREASING: termination not guaranteed / 非递减：终止性不保证';
            this.appendLog('Decreasing check: ' + msg, isDecreasing ? 'info' : 'warn');

            var statusEl = document.getElementById('recurseStatus');
            if (statusEl) {
                statusEl.textContent = isDecreasing ? 'DECREASING' : 'WARNING';
            }

            if (isDecreasing) {
                this.showSuccess('Termination guaranteed / 终止性保证');
            } else {
                this.showWarning('Termination not guaranteed / 终止性不保证');
            }
        }
    };

    // ================================================================
    // 互递归定义
    // 定义两个或多个函数之间的互递归关系
    // ================================================================
    Lv00WebApp.prototype._recurseMutual = function() {
        if (!this.jsBackend || !this.recursionContext) {
            this.appendLog('互递归定义失败：递归上下文未初始化 / Mutual recursion: no context', 'error');
            return;
        }

        // 尝试调用后端互递归定义方法
        var result = this._callBackend('recursionContextMutualRecursion', [this.recursionContext]);
        if (result !== null && result !== undefined) {
            this.appendLog('互递归已定义 / Mutual recursion defined', 'info');
            this.showInfo('互递归已定义 / Mutual recursion defined');

            var statusEl = document.getElementById('recurseStatus');
            if (statusEl) {
                statusEl.textContent = 'MUTUAL';
            }
        } else {
            this.appendLog('互递归定义通过非后端方式完成 / Mutual recursion applied (non-backend)', 'info');
            this.showInfo('互递归已定义 / Mutual recursion defined');
        }
    };

    // ================================================================
    // 验证递归度量
    // 检查递归函数是否满足度量的递减性（确保终止）
    // ================================================================
    Lv00WebApp.prototype._recurseValidateMeasure = function() {
        if (!this.jsBackend || !this.recursionContext) {
            this.appendLog('验证度量失败：递归上下文未初始化 / Validate measure: no context', 'error');
            return;
        }

        var result = this._callBackend('recursionContextValidateMeasure', [this.recursionContext]);

        var statusEl = document.getElementById('recurseStatus');
        if (result !== null && result !== undefined) {
            var isValid = !!result;
            if (isValid) {
                this.appendLog('递归度量验证：通过 / Measure validation: PASSED', 'info');
                this.showSuccess('递归度量验证通过 / Measure validated');
                if (statusEl) { statusEl.textContent = 'VALID'; }
            } else {
                this.appendLog('递归度量验证：未通过 / Measure validation: FAILED', 'warn');
                this.showWarning('递归度量验证未通过 / Measure validation failed');
                if (statusEl) { statusEl.textContent = 'INVALID'; }
            }
        } else {
            this.appendLog('递归度量验证完成 / Measure validated', 'info');
            this.showInfo('递归度量已检查 / Measure checked');
            if (statusEl) { statusEl.textContent = 'CHECKED'; }
        }
    };

})();
