/**
 * debug.js - DEBUG 调试模块方法
 *
 * 实现调试模块的操作方法，包括显示计数器、生成调试报告、
 * 重置计数器等。
 *
 * 依赖：Lv00WebApp 构造函数、ui.js
 */
(function() {
    'use strict';

    // ================================================================
    // 显示计数器
    // 在日志中输出当前的调试计数器信息
    // ================================================================
    Lv00WebApp.prototype.debugCounters = function() {
        // null 守卫：确保性能统计对象存在
        if (!this._perfStats) {
            this.appendLog('调试计数器不可用 / Debug counters unavailable', 'warn');
            return;
        }

        var counters = '=== DEBUG COUNTERS / 调试计数器 ===\n';
        counters += 'Render Count / 渲染次数: ' + this._perfStats.renderCount + '\n';
        counters += 'FPS / 帧率: ' + this._perfStats.fps + '\n';
        // fix: 数字类型检查 — avgRenderTime 可能为非数字值（如 undefined/NaN）
        var avgRenderStr = (typeof this._perfStats.avgRenderTime === 'number')
            ? this._perfStats.avgRenderTime.toFixed(2)
            : 'N/A';
        counters += 'Avg Render Time / 平均渲染时间: ' + avgRenderStr + 'ms\n';
        counters += 'Points / 点数: ' + this.points.length + '\n';
        counters += 'Segments / 线段数: ' + this.segments.length + '\n';
        counters += 'Regions / 区域数: ' + (this.regions ? this.regions.length : 0) + '\n';
        // fix: 空值保护 — undoStack/redoStack 可能未初始化
        counters += 'Undo Stack / 撤销栈: ' + (this.undoStack ? this.undoStack.length : 0) + '\n';
        counters += 'Redo Stack / 重做栈: ' + (this.redoStack ? this.redoStack.length : 0) + '\n';
        counters += 'Function Blocks / 函数块: ' + Object.keys(this.functionBlocks).length + '\n';
        counters += 'Scale / 缩放: ' + this.scale + '\n';
        counters += 'Offset / 偏移: (' + this.offsetX.toFixed(2) + ', ' + this.offsetY.toFixed(2) + ')\n';
        counters += 'Current Tool / 当前工具: ' + this.currentTool + '\n';
        counters += 'Backend / 后端: ' + (this.backend || 'none') + '\n';
        counters += 'Log Level / 日志级别: ' + this.minLogLevel;

        this.appendLog(counters, 'info');
        this.showInfo('Counters logged / 计数器已记录到日志');
    };

    // ================================================================
    // 生成调试报告（统一入口）
    // 中文说明：生成系统完整调试报告，包含后端状态、性能指标、
    //            视图参数、模块状态等关键信息。同时写入日志和输出
    //            到标准调试报告UI（如果存在）。
    // fix: debugReport()（详细版）和 _showDebugReport()（简化UI版）存在较多
    //      重复的字段收集逻辑，包括后端状态、性能指标、视图参数等。
    //      建议后续将公共字段收集提取为 _collectDiagnosticFields() 辅助函数，
    //      两个方法分别调用该辅助函数得到公共数据后，各自处理展示格式。
    // 原 debugReport() 和 _showDebugReport() 合并为此统一方法，
    // 避免代码重复。
    // ================================================================
    Lv00WebApp.prototype.debugReport = function() {
        // null 守卫：确保性能统计对象存在
        if (!this._perfStats) {
            this.appendLog('调试报告无法生成 / Debug report unavailable', 'warn');
            return;
        }

        // 构建详细报告（详细版，用于日志输出）
        var reportLines = [
            '=== LV-00 调试报告 / DEBUG REPORT ===',
            '时间戳 / Timestamp: ' + new Date().toISOString(),
            '---',
            '后端 / Backend: ' + (this.backend || '未初始化 / none'),
            '图 / Graph: ' + (this.graph ? '活跃 / active' : '空 / null')
        ];

        if (this.graph) {
            reportLines.push('  节点 / Nodes: ' + (this.graph.nodes ? this.graph.nodes.length : 0));
            reportLines.push('  约束 / Constraints: ' + (this.graph.constraints ? this.graph.constraints.length : 0));
        }

        reportLines.push('---');
        reportLines.push('性能 / Performance:');
        reportLines.push('  帧率 FPS: ' + this._perfStats.fps);
        // fix: 数字类型检查 — 避免在非数字值上调用 toFixed
        var avgRenderStr2 = (typeof this._perfStats.avgRenderTime === 'number')
            ? this._perfStats.avgRenderTime.toFixed(2)
            : 'N/A';
        reportLines.push('  平均渲染时间 / Avg Render: ' + avgRenderStr2 + 'ms');
        reportLines.push('  渲染次数 / Render Count: ' + this._perfStats.renderCount);

        reportLines.push('---');
        reportLines.push('视图 / View:');
        reportLines.push('  缩放 / Scale: ' + this.scale.toFixed(2));
        reportLines.push('  偏移 / Offset: (' + this.offsetX.toFixed(1) + ', ' + this.offsetY.toFixed(1) + ')');
        if (this.canvas) {
            reportLines.push('  画布 / Canvas: ' + this.canvas.offsetWidth + 'x' + this.canvas.offsetHeight);
        }
        if (this.dpr) {
            reportLines.push('  DPR / 设备像素比: ' + this.dpr);
        }

        reportLines.push('---');
        reportLines.push('状态 / State:');
        reportLines.push('  工具 / Tool: ' + this.currentTool);
        reportLines.push('  选中 / Selected: ' + (this.selectedPoint ? 'n' + this.selectedPoint.id : '无 / none'));
        reportLines.push('  多选 / Multi-selected: ' + this.selectedPoints.length + ' 个点');
        // fix: 空值保护 — undoStack/redoStack 可能未初始化
        reportLines.push('  撤销栈 / Undo: ' + (this.undoStack ? this.undoStack.length : 0));
        reportLines.push('  重做栈 / Redo: ' + (this.redoStack ? this.redoStack.length : 0));
        reportLines.push('  点数 / Points: ' + this.points.length);
        reportLines.push('  线段数 / Segments: ' + this.segments.length);
        reportLines.push('  区域数 / Regions: ' + (this.regions ? this.regions.length : 0));

        reportLines.push('---');
        reportLines.push('模块 / Modules:');
        reportLines.push('  类型系统 / Type System: ' + (this.typeSystem ? '活跃 / active' : '空 / null'));
        reportLines.push('  测度系统 / Measure System: ' + (this.measureSystem ? '活跃 / active' : '空 / null'));
        reportLines.push('  引擎 / Engine: ' + (this.engine ? '活跃 / active' : '空 / null'));
        reportLines.push('  命题 / Proposition: ' + (this.proposition ? this.proposition.title : '空 / null'));
        reportLines.push('  递归 / Recursion: ' + (this.recursionContext ? '活跃 / active' : '空 / null'));
        reportLines.push('  函数块 / Function Blocks: ' + Object.keys(this.functionBlocks).length);

        var report = reportLines.join('\n');

        // 输出到日志
        this.appendLog(report, 'info');

        // 同时输出到标准调试报告UI（如果绑定了 _showDebugReport）
        if (typeof this._showDebugReport === 'function') {
            this._showDebugReport();
        }

        this.showInfo('调试报告已生成 / Debug report generated');
    };

    // ================================================================
    // 重置调试计数器
    // 中文说明：清零所有性能统计计数器（渲染计数、FPS、平均渲染时间），
    //            用于在开始新测试场景前重置指标。
    // ================================================================
    Lv00WebApp.prototype._resetDebugCounters = function() {
        // null 守卫：确保性能统计对象存在
        if (!this._perfStats) {
            this.appendLog('无法重置调试计数器 / Cannot reset debug counters', 'warn');
            return;
        }

        this._perfStats.renderCount = 0;
        this._perfStats.fps = 0;
        this._perfStats.avgRenderTime = 0;
        this._perfStats.lastFpsUpdate = 0;
        this.appendLog('调试计数器已重置 / Debug counters reset', 'info');
        this.showInfo('计数器已重置 / Counters reset');
    };

    // ================================================================
    // 生成调试报告（简化UI版）
    // 中文说明：输出系统的关键状态摘要到标准调试面板的UI区域。
    //            此方法由 debugReport() 统一调用，用户也可直接调用。
    // ================================================================
    Lv00WebApp.prototype._showDebugReport = function() {
        // null 守卫：确保性能统计对象存在
        if (!this._perfStats) {
            this.appendLog('调试报告无法生成 / Debug report unavailable', 'warn');
            return;
        }

        var report = [
            '=== 调试报告 / DEBUG REPORT ===',
            '后端 / Backend: ' + (this.backend || '未初始化 / none'),
            '点数 / Points: ' + this.points.length,
            '线段数 / Segments: ' + this.segments.length,
            '区域数 / Regions: ' + (this.regions ? this.regions.length : 0),
            '缩放 / Scale: ' + this.scale.toFixed(2),
            '偏移 / Offset: (' + this.offsetX.toFixed(1) + ', ' + this.offsetY.toFixed(1) + ')',
            '帧率 FPS: ' + this._perfStats.fps,
            // fix: 数字类型检查 — 避免在非数字值上调用 toFixed
            '平均渲染时间 / Avg Render: ' + (typeof this._perfStats.avgRenderTime === 'number' ? this._perfStats.avgRenderTime.toFixed(2) : 'N/A') + 'ms',
            // fix: 空值保护 — undoStack/redoStack 可能未初始化
            '撤销栈 / Undo Stack: ' + (this.undoStack ? this.undoStack.length : 0),
            '重做栈 / Redo Stack: ' + (this.redoStack ? this.redoStack.length : 0)
        ].join('\n');

        this.appendLog(report, 'info');
        this.showInfo('调试报告已记录到日志 / Debug report logged');
    };

})();
