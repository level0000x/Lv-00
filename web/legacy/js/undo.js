/**
 * undo.js - 撤销/重做模块
 *
 * 从 app.js 中提取的撤销/重做相关方法，挂载到 Lv00WebApp.prototype 上。
 * 包含状态保存、撤销、重做、图数据同步等功能。
 *
 * @description 提供完整的撤销/重做功能，支持最多 50 步操作历史。
 *              每次修改操作前自动保存图数据快照到撤销栈。
 * @module undo
 * @requires Lv00WebApp 构造函数（app.js）
 * @requires ui.js（appendLog, updateStats 等方法）
 * @since 3.0.0
 */
(function() {
    'use strict';

    // ---- 撤销/重做模块常量（从 constants.js 导入）-------------------------
    /** @constant {Object} 撤销/重做模块常量命名空间引用 */
    var unc = Lv00Const.undo;

    // ================================================================
    /**
     * 安全访问几何类型枚举
     *
     * @description 优先使用后端 GeomType 枚举（lv00_js_backend.js 中定义），
     *              不可用时回退到硬编码值。
     *              供 syncPointsFromGraph / syncSegmentsFromGraph 及
     *              interaction 模块使用。
     *
     * @returns {Object} 几何类型映射对象 { POINT: number, LINE_SEGMENT: number }
     */
    // ================================================================
    Lv00WebApp.prototype._geomType = function() {
        if (this.jsBackend && this.jsBackend.GeomType) {
            return this.jsBackend.GeomType;
        }
        return { POINT: 0, LINE_SEGMENT: 1 };
    };

    // ================================================================
    /**
     * 安全深拷贝方法
     *
     * 中文说明：优先使用浏览器原生 structuredClone() API 进行深拷贝，
     *            不可用时回退到后端 _deepCloneGraph 手动实现，
     *            最终兜底使用 JSON.parse(JSON.stringify())。 
     *
     * @param {Object} obj - 需要深拷贝的对象
     * @returns {Object} 深拷贝后的对象
     */
    // ================================================================
    Lv00WebApp.prototype._safeDeepClone = function(obj) {
        if (!obj) return obj;
        try {
            // 优先使用 structuredClone（现代浏览器原生支持，性能最优）
            if (typeof structuredClone === 'function') {
                return structuredClone(obj);
            }
        } catch (e) {
            console.warn('[Lv-00] structuredClone 失败，回退:', e.message);
        }
        // 回退方案：使用后端手动的 _deepCloneGraph（如果可用）
        if (this.jsBackend && typeof this.jsBackend._deepCloneGraph === 'function') {
            try {
                return this.jsBackend._deepCloneGraph(obj);
            } catch (e2) {
                console.warn('[Lv-00] _deepCloneGraph 回退失败:', e2.message);
            }
        }
        // 最终兜底：JSON 序列化（不保留函数/undefined/循环引用）
        try {
            return JSON.parse(JSON.stringify(obj));
        } catch (e3) {
            console.error('[Lv-00] 所有深拷贝方案均失败:', e3.message);
            return obj; // 返回原始引用作为最后手段
        }
    };

    // ================================================================
    /**
     * 保存撤销状态（快照）
     *
     * @description 在执行修改操作前调用，将当前图数据的深拷贝压入撤销栈。
     *              最多保留 50 个历史状态，超出时自动移除最早的记录（FIFO）。
     *              同时清空重做栈，因为新操作使重做历史失效。
     *
     * 中文说明：在每次修改操作前调用，使用安全深拷贝创建图数据快照。
     *            压入撤销栈，同时清空重做栈（新操作使旧重做记录失效）。
     *
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype._saveUndoState = function() {
        if (!this.jsBackend || !this.graph) return;

        try {
            // 使用安全的深拷贝方法（优先 structuredClone，回退手动实现）
            var snapshot = this._safeDeepClone(this.graph);
            if (!snapshot) {
                console.warn('[Lv-00] _saveUndoState: 图数据快照创建失败');
                return;
            }
            this.undoStack.push(snapshot);
            // 修复：使用 splice(0, 1) 替代 shift() 移除最旧记录，避免 O(n) 数组重排
            // 中文说明：splice 移除头部元素与 shift 功能等价，但更明确的语义表示意图
            //           注意：此处仅在超限时调用（每 50 次 push 才触发 1 次），频率极低
            if (this.undoStack.length > unc.MAX_UNDO_STACK) {
                this.undoStack.splice(0, 1); // 移除最旧的快照（栈底），保持栈大小在限制内
            }
            this.redoStack = [];
        } catch (e) {
            console.error('[Lv-00] _saveUndoState: 保存撤销状态失败:', e.message);
        }
    };

    // ================================================================
    /**
     * 撤销操作 (Undo)
     *
     * @description 从撤销栈弹出上一个状态，将当前状态压入重做栈，
     *              然后恢复到上一个状态并刷新视图。
     *              仅支持 JS 后端模式（WASM 模式暂不支持撤销/重做）。
     *
     * 中文说明：弹出上一步快照恢复图形，当前状态压入重做栈供后续重做。
     *
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype.undo = function() {
        if (!this.jsBackend) {
            this.appendLog('UNDO/REDO not supported in WASM mode', 'warn');
            return;
        }
        if (this.undoStack.length === 0) {
            this.appendLog('UNDO / 撤销: NOTHING TO UNDO / 无可撤销操作', 'warn');
            return;
        }

        try {
            var currentSnapshot = this._safeDeepClone(this.graph);
            this.redoStack.push(currentSnapshot);
            var prevSnapshot = this.undoStack.pop();
            this.graph = prevSnapshot;
            this.syncPointsFromGraph();
            this.syncSegmentsFromGraph();
            this.selectedPoint = null;
            this.selectedPoints = [];
            this.updateStats();
            this.updateProperties(null);
            this.updateStatus('UNDO / 已撤销');
            this.appendLog('UNDO / 撤销', 'info');
            this.render();
        } catch (e) {
            console.error('[Lv-00] undo: 撤销操作失败:', e.message);
            this.appendLog('UNDO 失败: ' + e.message, 'error');
        }
    };

    // ================================================================
    /**
     * 重做操作 (Redo)
     *
     * @description 从重做栈弹出下一个状态，将当前状态压入撤销栈，
     *              然后恢复到下一个状态并刷新视图。
     *              仅支持 JS 后端模式（WASM 模式暂不支持撤销/重做）。
     *
     * 中文说明：重做之前撤销的操作，弹出重做栈快照恢复图形。
     *
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype.redo = function() {
        if (!this.jsBackend) {
            this.appendLog('UNDO/REDO not supported in WASM mode', 'warn');
            return;
        }
        if (this.redoStack.length === 0) {
            this.appendLog('REDO / 重做: NOTHING TO REDO / 无可重做操作', 'warn');
            return;
        }

        try {
            var currentSnapshot = this._safeDeepClone(this.graph);
            this.undoStack.push(currentSnapshot);
            var nextSnapshot = this.redoStack.pop();
            this.graph = nextSnapshot;
            this.syncPointsFromGraph();
            this.syncSegmentsFromGraph();
            this.selectedPoint = null;
            this.selectedPoints = [];
            this.updateStats();
            this.updateProperties(null);
            this.updateStatus('REDO / 已重做');
            this.appendLog('REDO / 重做', 'info');
            this.render();
        } catch (e) {
            console.error('[Lv-00] redo: 重做操作失败:', e.message);
            this.appendLog('REDO 失败: ' + e.message, 'error');
        }
    };

    // ================================================================
    /**
     * 从图数据同步点到本地 points 数组
     *
     * @description 遍历图中的所有节点，提取几何类型为 POINT 的节点，
     *              将其有理数坐标转换为浮点数后存入 points 数组。
     *              同时恢复选中点的引用（通过 ID 匹配）。
     *
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype.syncPointsFromGraph = function() {
        if (!this.jsBackend || !this.graph) return;

        try {
            this.points = [];
            var geomType = this._geomType();
            var i;
            for (i = 0; i < this.graph.nodes.length; i++) {
                var node = this.graph.nodes[i];
                if (node.geomType === geomType.POINT) {
                    var x = 0, y = 0;
                    if (node.coordX) {
                        x = this.jsBackend.coordToDouble(node.coordX);
                    }
                    if (node.coordY) {
                        y = this.jsBackend.coordToDouble(node.coordY);
                    }
                    this.points.push({ id: node.id, x: x, y: y });
                }
            }
            // 恢复选中点的引用
            if (this.selectedPoint) {
                var selId = this.selectedPoint.id;
                this.selectedPoint = null;
                for (i = 0; i < this.points.length; i++) {
                    if (this.points[i].id === selId) {
                        this.selectedPoint = this.points[i];
                        break;
                    }
                }
            }
        } catch (e) {
            console.error('[Lv-00] syncPointsFromGraph: 同步点数据失败:', e.message);
            this.appendLog('同步点数据失败: ' + e.message, 'error');
        }
    };

    // ================================================================
    /**
     * 从图数据同步线段到本地 segments 数组
     *
     * @description 遍历图中的所有节点，提取几何类型为 LINE_SEGMENT 的节点，
     *              将其端点 ID 存入 segments 数组。
     *
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype.syncSegmentsFromGraph = function() {
        if (!this.jsBackend || !this.graph) return;

        try {
            this.segments = [];
            var geomType = this._geomType();
            var i;
            for (i = 0; i < this.graph.nodes.length; i++) {
                var node = this.graph.nodes[i];
                if (node.geomType === geomType.LINE_SEGMENT) {
                    this.segments.push({
                        p1: node.endpoint1Id,
                        p2: node.endpoint2Id,
                        id: node.id
                    });
                }
            }
        } catch (e) {
            console.error('[Lv-00] syncSegmentsFromGraph: 同步线段数据失败:', e.message);
            this.appendLog('同步线段数据失败: ' + e.message, 'error');
        }
    };

})();
