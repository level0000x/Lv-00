/**
 * render.js - Canvas 渲染模块（优化版）
 *
 * 从 app.js 中提取的 Canvas 渲染相关方法，挂载到 Lv00WebApp.prototype 上。
 * 包含主渲染循环、网格绘制、坐标轴绘制、点/线段绘制、
 * 主题颜色管理、HUD 绘制等功能。
 *
 * 渲染流水线（按绘制顺序）：
 *   背景 -> 网格 -> 坐标轴 -> 区域 -> 线段 -> 约束标记
 *        -> 端口 -> 函数块 -> 点 -> 悬停提示 -> HUD
 *
 * @module render
 * @requires Lv00WebApp 构造函数（app.js）
 * @requires utils.js（_getThemeColors 使用全局变量）
 * @since 3.0.0
 */
(function() {
    'use strict';

    // ---- 渲染模块常量（从 constants.js 导入）----------------------------
    /** @constant {Object} 渲染模块常量命名空间引用 */
    var rc = Lv00Const.render;

    // ================================================================
    /**
     * 获取当前主题的 Canvas 渲染颜色方案
     *
     * @description 根据 body 元素上的 'light-theme' CSS 类判断当前主题模式，
     *              返回对应的 Canvas 2D 绘制颜色配置对象。
     *              所有颜色值均为 CSS 颜色字符串，可直接用于 ctx.fillStyle 等属性。
     *
     * 中文说明：根据 body 的 light-theme 类判断当前主题模式，
     *            返回对应亮色/暗色的 Canvas 绘制颜色方案。
     *
     * @returns {Object} 包含以下颜色属性的配置对象：
     *   - canvasBg       {string} 画布背景色
     *   - grid           {string} 网格线颜色
     *   - axis           {string} 坐标轴线颜色
     *   - point          {string} 普通节点的默认颜色
     *   - pointSelected  {string} 选中节点的颜色
     *   - pointHover     {string} 鼠标悬停时节点的颜色
     *   - segment        {string} 线段绘制颜色
     *   - text           {string} 文字颜色
     */
    // ================================================================
    Lv00WebApp.prototype._getThemeColors = function() {
        var isLight = document.body.classList.contains('light-theme');
        if (isLight) {
            // 亮色主题：白色/浅灰背景，深色前景元素
            return {
                canvasBg: '#f5f5f5',
                grid: '#e0e0e0',
                axis: '#ccc',
                point: '#333',
                pointSelected: '#2196f3',
                pointHover: '#4caf50',
                segment: '#666',
                text: '#333'
            };
        }
        // 暗色主题（默认）：深色背景，浅色/高亮前景元素
        return {
            canvasBg: '#080808',
            grid: '#1a1a1a',
            axis: '#2a2a2a',
            point: '#888',
            pointSelected: '#4caf50',
            pointHover: '#2196f3',
            segment: '#555',
            text: '#888'
        };
    };

    /**
     * 主渲染函数
     *
     * @description Canvas 渲染的总入口，负责按序调用所有绘制方法。
     *              使用 requestAnimationFrame 进行节流，保证约 60fps 的稳定渲染帧率。
     *              如果距离上次渲染时间过短，则将渲染请求推迟到下一帧执行。
     *
     * 中文说明：Canvas 渲染主循环入口，按渲染流水线顺序绘制所有元素。
     *            包含帧率节流控制、脏区域标记（预留）、性能统计（EMA平滑）。
     *
     * 渲染流程：
     *   1. 节流检查，必要时延迟渲染
     *   2. 绘制背景色
     *   3. 绘制网格和坐标轴（受 _showGrid/_showAxes 控制）
     *   4. 建立 pointId -> point 映射表，供线段、约束标记等快速查找
     *   5. 按层级顺序绘制：区域 -> 线段 -> 约束标记 -> 端口 -> 函数块 -> 点
     *   6. 绘制悬停提示框（受 _showLabels 控制）
     *   7. 绘制 HUD 叠加层（右下角缩放信息）
     *   8. 更新性能统计（指数移动平均）
     *
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype.render = function() {
        var now = performance.now();

        // 节流渲染：如果距离上次渲染时间过短，则延迟到下一帧
        if (now - this._lastRenderTime < this._renderThrottleMs) {
            if (!this._renderPending) {
                this._renderPending = true;
                var self = this;
                requestAnimationFrame(function() {
                    self._renderPending = false;
                    self.render();
                });
            }
            return;
        }

        this._lastRenderTime = now;
        this._perfStats.renderCount++;
        var renderStart = performance.now();
        var w = this.canvas.offsetWidth;
        var h = this.canvas.offsetHeight;
        var ctx = this.ctx;

        // 缓存主题颜色，避免子方法重复计算
        this._themeColors = this._getThemeColors();
        var tc = this._themeColors;

        // 性能优化：在渲染入口处缓存 getComputedStyle 结果，避免子方法中重复调用触发强制回流
        // 中文说明：getComputedStyle 每次调用都会触发浏览器同步布局计算（forced reflow），
        //            在渲染循环中多次调用会严重拖慢帧率。这里只调用一次，将结果传递给所有子绘制方法。
        var cs = getComputedStyle(document.documentElement);

        // 性能优化：从 CSS 变量 --font-mono 缓存字体族名称，避免子方法中重复读取
        // 中文说明：--font-mono 定义了等宽字体族，子方法只需拼接字号前缀即可使用
        var fontMono = cs.getPropertyValue('--font-mono').trim() || 'Consolas, monospace';

        try {
            // 绘制背景
            ctx.fillStyle = tc.canvasBg;
            ctx.fillRect(0, 0, w, h);

            // 绘制网格和坐标轴（受显示状态标志控制）
            if (this._showGrid !== false) {
                this.drawGrid(w, h);
            }
            if (this._showAxes !== false) {
                this.drawAxes(w, h);
            }

            // 建立 pointId -> point 映射表，避免 O(n) 线性搜索
            var pointMap = {};
            for (var pi = 0; pi < this.points.length; pi++) {
                pointMap[this.points[pi].id] = this.points[pi];
            }
            this._pointMap = pointMap;

            // 建立 selectedPoints 的 Set 映射，将 drawPoint 中的 O(n) 线性搜索优化为 O(1) 查找
            // 中文说明：将多选点数组转换为 Set，drawPoint 中判断 isMultiSelected 时直接 O(1) 查询
            var selectedIdSet = {};
            for (var si = 0; si < this.selectedPoints.length; si++) {
                selectedIdSet[this.selectedPoints[si].id] = true;
            }
            this._selectedPointIdSet = selectedIdSet;

            // 绘制区域（在线段之前，使区域作为背景层）
            this._drawRegions(ctx, cs);

            // 绘制线段
            for (var i = 0; i < this.segments.length; i++) {
                this.drawSegment(this.segments[i]);
            }

            // 绘制约束标记（受显示状态标志控制）
            if (this._showLabels !== false) {
                this._drawConstraintMarkers(ctx, cs, fontMono);
            }

            // 绘制端口
            this._drawPorts(ctx, cs);

            // 绘制函数块
            this._drawFunctionBlocks(ctx, cs, fontMono);

            // 绘制点（选中的点最后绘制，确保在最上层）
            for (var k = 0; k < this.points.length; k++) {
                this.drawPoint(this.points[k]);
            }

            // 悬停提示框（在所有元素之上，受标签显示状态控制）
            if (this.hoveredPoint && this._showLabels !== false) {
                this.drawTooltip(this.hoveredPoint, cs, fontMono);
            }

            // HUD 叠加层（右下角缩放信息）
            this.drawHUD(w, h, cs, fontMono);
        } catch (e) {
            console.error('[Lv-00] render: 渲染过程中发生异常:', e.message);
        }

        // 更新性能统计（指数移动平均，权重 0.9/0.1）
        var renderTime = performance.now() - renderStart;
        this._perfStats.avgRenderTime = this._perfStats.avgRenderTime * (1 - rc.PERF_EMA_ALPHA) + renderTime * rc.PERF_EMA_ALPHA;
    };

    // ================================================================
    // 绘制网格
    //
    // 中文说明：根据缩放级别动态调整网格间距，使视觉密度一致。
    //            网格线随画布平移/缩放平滑移动，辅助用户定位。
    //
    // @param {number} w - 画布宽度（CSS 像素）
    // @param {number} h - 画布高度（CSS 像素）
    // @returns {void}
    // ================================================================
    Lv00WebApp.prototype.drawGrid = function(w, h) {
        var ctx = this.ctx;
        var tc = this._themeColors;

        ctx.strokeStyle = tc.grid;
        ctx.lineWidth = 1;

        // 根据缩放级别计算网格间距
        var gridSize = this._getGridSize();
        // 计算网格偏移量：使网格线跟随画布平移而移动
        var offsetX = (this.offsetX * this.scale) % gridSize;
        var offsetY = (this.offsetY * this.scale) % gridSize;

        ctx.beginPath();

        // 垂直线
        for (var x = offsetX; x < w; x += gridSize) {
            ctx.moveTo(x, 0);
            ctx.lineTo(x, h);
        }

        // 水平线
        for (var y = offsetY; y < h; y += gridSize) {
            ctx.moveTo(0, y);
            ctx.lineTo(w, y);
        }

        ctx.stroke();
    };

    // ================================================================
    // 根据缩放级别计算合适的网格间距
    //
    // 根据当前缩放级别动态调整网格间距，保证不同缩放级别下
    // 视觉上网格密度保持一致（不会太密或太疏）。
    //
    // 缩放级别与网格间距的映射规则：
    //   scale < 0.1   -> 间距 = baseSize * 10（极大缩小时网格变疏）
    //   scale < 0.5   -> 间距 = baseSize * 5
    //   scale < 1     -> 间距 = baseSize * 2
    //   scale < 2     -> 间距 = baseSize（默认 50px）
    //   scale < 5     -> 间距 = baseSize / 2
    //   scale < 10    -> 间距 = baseSize / 5
    //   scale >= 10   -> 间距 = baseSize / 10（极大放大时网格变密）
    //
    // @returns {number} 网格间距（CSS 像素）
    // ================================================================
    Lv00WebApp.prototype._getGridSize = function() {
        var baseSize = rc.BASE_GRID_SIZE;
        var scale = this.scale;

        if (scale < rc.GRID_SCALE_L1) return baseSize * rc.GRID_MULT_10;
        if (scale < rc.GRID_SCALE_L2) return baseSize * rc.GRID_MULT_5;
        if (scale < rc.GRID_SCALE_L3) return baseSize * rc.GRID_MULT_2;
        if (scale < rc.GRID_SCALE_L4) return baseSize;
        if (scale < rc.GRID_SCALE_L5) return baseSize / rc.GRID_DIV_2;
        if (scale < rc.GRID_SCALE_L6) return baseSize / rc.GRID_DIV_5;
        return baseSize / rc.GRID_DIV_10;
    };

    // ================================================================
    // 绘制坐标轴
    //
    // 在世界坐标系的原点位置绘制 X 轴（水平线）和 Y 轴（垂直线）。
    // 坐标轴的交点位于屏幕中心加上偏移后对应世界原点的位置。
    //
    // 坐标轴颜色和线宽通过 _getThemeColors 获取，在亮色和暗色主题下
    // 呈现不同的视觉效果。
    //
    // @param {number} w - 画布宽度（CSS 像素）
    // @param {number} h - 画布高度（CSS 像素）
    // @returns {void}
    // ================================================================
    Lv00WebApp.prototype.drawAxes = function(w, h) {
        var ctx = this.ctx;
        var tc = this._themeColors;

        // 计算世界原点在屏幕上的位置
        var centerX = w / 2 + this.offsetX * this.scale;
        var centerY = h / 2 + this.offsetY * this.scale;

        ctx.strokeStyle = tc.axis;
        ctx.lineWidth = rc.AXIS_LINE_WIDTH;
        ctx.beginPath();

        // X 轴（水平线）
        ctx.moveTo(0, centerY);
        ctx.lineTo(w, centerY);

        // Y 轴（垂直线）
        ctx.moveTo(centerX, 0);
        ctx.lineTo(centerX, h);

        ctx.stroke();
    };

    // ================================================================
    // 绘制单个点（节点）
    //
    // 在世界坐标对应位置绘制一个圆点，根据节点的选中/悬停/多选状态
    // 使用不同的颜色和半径大小来区分视觉状态。
    //
    // 渲染规则：
    //   - 普通状态：小半径（4px），默认颜色
    //   - 悬停状态：中半径（6px），悬停高亮颜色
    //   - 选中状态：中半径（6px），选中颜色
    //   - 多选状态：中半径（6px），默认颜色，外加选中颜色外圈边框
    //   - 选中/悬停/多选时额外绘制外圈边框（radius + 3px）
    //
    // @param {Object} point - 点对象，包含 id（唯一标识）、x（世界X坐标）、y（世界Y坐标）
    // @returns {void}
    // ================================================================
    Lv00WebApp.prototype.drawPoint = function(point) {
        var ctx = this.ctx;
        var tc = this._themeColors;
        var pos = this.worldToScreen(point.x, point.y);

        // 判断点的状态
        var isSelected = this.selectedPoint && this.selectedPoint.id === point.id;
        var isHovered = this.hoveredPoint && this.hoveredPoint.id === point.id;
        // 使用 Set 查找替代 O(n) 线性搜索（性能优化）
        // 中文说明：_selectedPointIdSet 在 render() 中预先构建，这里直接 O(1) 查找
        var isMultiSelected = !!(this._selectedPointIdSet && this._selectedPointIdSet[point.id]);
        var radius = isSelected || isHovered ? rc.POINT_RADIUS_ACTIVE : rc.POINT_RADIUS_NORMAL;
        var color = isSelected ? tc.pointSelected : (isHovered ? tc.pointHover : tc.point);

        ctx.fillStyle = color;
        ctx.beginPath();
        ctx.arc(pos.x, pos.y, radius, 0, Math.PI * 2);
        ctx.fill();

        // 选中/悬停时绘制外圈边框，增强视觉反馈
        if (isSelected || isHovered || isMultiSelected) {
            ctx.strokeStyle = tc.pointSelected;
            ctx.lineWidth = 2;
            ctx.beginPath();
            ctx.arc(pos.x, pos.y, radius + rc.POINT_OUTER_OFFSET, 0, Math.PI * 2);
            ctx.stroke();
        }
    };

    // ================================================================
    // 绘制单条线段
    //
    // 在世界坐标系中连接两个端点绘制直线。
    // 通过 _pointMap 映射表以 O(1) 时间复杂度查找端点坐标，
    // 避免遍历 points 数组的线性搜索。
    //
    // @param {Object} segment - 线段对象，包含以下属性：
    //   p1 - 起点节点的 ID
    //   p2 - 终点节点的 ID
    // @returns {void}
    // ================================================================
    Lv00WebApp.prototype.drawSegment = function(segment) {
        var ctx = this.ctx;
        var tc = this._themeColors;

        // 通过点 ID 映射表快速查找两个端点
        var p1 = this._pointMap[segment.p1];
        var p2 = this._pointMap[segment.p2];

        // 如果任一端点不存在，跳过绘制
        if (!p1 || !p2) return;
        var pos1 = this.worldToScreen(p1.x, p1.y);
        var pos2 = this.worldToScreen(p2.x, p2.y);

        ctx.strokeStyle = tc.segment;
        ctx.lineWidth = rc.SEGMENT_LINE_WIDTH;
        ctx.beginPath();
        ctx.moveTo(pos1.x, pos1.y);
        ctx.lineTo(pos2.x, pos2.y);
        ctx.stroke();
    };

    // ================================================================
    // 绘制悬停提示框
    //
    // 中文说明：鼠标悬停节点时绘制浮动信息框，显示节点ID和精确坐标。
    //            自动边界检测防止超出画布，使用 CSS 变量适配亮/暗主题。
    // 优化点：将硬编码颜色（rgba(20,20,20,0.9)/#444/#aaa）替换为
    //          CSS 变量，实现亮色主题自动适配。
    //
    // @param {Object} point - 悬停的点对象，包含 id、x、y 属性
    // @param {CSSStyleDeclaration} cs - 缓存的 getComputedStyle 结果（性能优化）
    // @param {string} fontMono - 缓存的等宽字体族名称（性能优化）
    // @returns {void}
    // ================================================================
    Lv00WebApp.prototype.drawTooltip = function(point, cs, fontMono) {
        var ctx = this.ctx;
        var pos = this.worldToScreen(point.x, point.y);

        // 格式化显示文本：节点ID + 世界坐标
        var text = 'n' + point.id + ' (' + point.x.toFixed(2) + ', ' + point.y.toFixed(2) + ')';
        // 使用 CSS 变量 --font-mono 拼接字体，避免硬编码字体族
        ctx.font = '11px ' + fontMono;
        var textWidth = ctx.measureText(text).width;
        var padding = 6;
        var boxWidth = textWidth + padding * 2;
        var boxHeight = 20;
        // 默认位置：节点右上方
        var boxX = pos.x + 10;
        var boxY = pos.y - 30;

        // 边界检测：确保提示框不超出画布可视区域
        var w = this.canvas.offsetWidth;
        if (boxX + boxWidth > w) boxX = pos.x - boxWidth - 10;  // 翻转到左侧
        if (boxY < 0) boxY = pos.y + 10;                        // 翻转到下方

        // 获取主题感知的工具提示颜色（从缓存的 CSS 变量读取，适配亮/暗主题）
        // 中文说明：使用 render() 入口处缓存的 cs 对象，避免重复调用 getComputedStyle
        var tooltipBg = cs.getPropertyValue('--color-canvas-tooltip-bg').trim() || 'rgba(20,20,20,0.9)';
        var tooltipBorder = cs.getPropertyValue('--color-canvas-tooltip-border').trim() || '#444';
        var tooltipText = cs.getPropertyValue('--color-canvas-tooltip-text').trim() || '#aaa';

        // 绘制背景
        ctx.fillStyle = tooltipBg;
        ctx.fillRect(boxX, boxY, boxWidth, boxHeight);

        // 绘制边框
        ctx.strokeStyle = tooltipBorder;
        ctx.lineWidth = 1;
        ctx.strokeRect(boxX, boxY, boxWidth, boxHeight);

        // 绘制文字
        ctx.fillStyle = tooltipText;
        ctx.fillText(text, boxX + padding, boxY + 14);
    };

    // ================================================================
    // 绘制 HUD（抬头显示） 叠加信息
    //
    // 在画布右下角以半透明样式显示缩放级别百分比。
    // HUD 信息始终绘制在所有其他元素的最上层。
    // 同时同步更新 DOM 中的 #zoomDisplay 元素为当前缩放百分比。
    //
    // @param {number} w - 画布宽度（CSS 像素）
    // @param {number} h - 画布高度（CSS 像素）
    // @param {CSSStyleDeclaration} cs - 缓存的 getComputedStyle 结果（性能优化）
    // @param {string} fontMono - 缓存的等宽字体族名称（性能优化）
    // @returns {void}
    // ================================================================
    Lv00WebApp.prototype.drawHUD = function(w, h, cs, fontMono) {
        var ctx = this.ctx;

        // 缩放级别显示（右下角画布内）
        // 使用 CSS 变量 --font-mono 拼接字体，避免硬编码字体族
        ctx.font = '10px ' + fontMono;
        // 使用缓存的 CSS 变量获取 HUD 文字颜色，适配亮/暗主题
        var hudTextColor = cs.getPropertyValue('--color-hud-text').trim() || '#444';
        ctx.fillStyle = hudTextColor;
        var zoomText = Math.round(this.scale * 100) + '%';
        ctx.fillText(zoomText, w - 50, h - 10);

        // 同步更新 DOM 中的 zoomDisplay 元素为当前缩放百分比
        var zoomEl = document.getElementById('zoomDisplay');
        if (zoomEl) {
            zoomEl.textContent = zoomText;
        }
    };

    // ================================================================
    // 独立更新缩放显示（不依赖完整渲染）
    // 在工具切换或视图变化时显式调用，确保 #zoomDisplay 立即反映当前缩放状态
    // ================================================================
    Lv00WebApp.prototype._updateZoomDisplay = function() {
        var zoomText = Math.round(this.scale * 100) + '%';
        var zoomEl = document.getElementById('zoomDisplay');
        if (zoomEl) {
            zoomEl.textContent = zoomText;
        }
    };

    // ================================================================
    // 绘制区域（私有方法）
    //
    // 遍历已定义的区域列表，以半透明填充+边框样式将每个区域
    // 绘制在画布上。区域由至少 3 个点组成的多边形定义。
    //
    // 区域绘制在线段之前，使其作为背景层，不会遮挡后续元素。
    // 只有包含至少 3 个顶点的区域才会被渲染。
    //
    // @param {CanvasRenderingContext2D} ctx - Canvas 2D 渲染上下文
    // @param {CSSStyleDeclaration} cs - 缓存的 getComputedStyle 结果（性能优化）
    // @returns {void}
    // ================================================================
    Lv00WebApp.prototype._drawRegions = function(ctx, cs) {
        if (!this.regions || this.regions.length === 0) return;

        // 性能优化：将 CSS 变量读取移到循环外部，避免每个区域重复调用 getComputedStyle
        // 中文说明：区域填充色和边框色在同一帧内不会变化，只需读取一次
        var regionFill = cs.getPropertyValue('--color-region-fill').trim() || 'rgba(100, 180, 255, 0.1)';
        // 修复：CSS 变量名从 --color-region-stroke 改为 --color-region-border，与 variables.css 保持一致
        var regionStroke = cs.getPropertyValue('--color-region-border').trim() || 'rgba(100, 180, 255, 0.4)';

        for (var i = 0; i < this.regions.length; i++) {
            var region = this.regions[i];
            // 跳过顶点数不足的区域（至少需要 3 个点才能形成面）
            if (!region.points || region.points.length < 3) continue;

            // 半透明蓝色填充 + 边框（使用 CSS 变量适配亮/暗主题）
            ctx.fillStyle = regionFill;
            ctx.strokeStyle = regionStroke;
            ctx.lineWidth = 1;
            ctx.beginPath();

            // 将世界坐标转换为屏幕坐标后绘制多边形路径
            var firstPos = this.worldToScreen(region.points[0].x, region.points[0].y);
            ctx.moveTo(firstPos.x, firstPos.y);

            for (var j = 1; j < region.points.length; j++) {
                var pos = this.worldToScreen(region.points[j].x, region.points[j].y);
                ctx.lineTo(pos.x, pos.y);
            }

            ctx.closePath();
            ctx.fill();
            ctx.stroke();
        }
    };

    // ================================================================
    // 绘制约束标记（私有方法）
    //
    // 在图数据中每个约束所对应的第一个节点位置绘制约束类型标记。
    // 不同类型的约束使用不同的字母标识和颜色：
    //
    //   类型 0 - INCIDENCE（关联）     : 标记 "I"，绿色 #4caf50
    //   类型 1 - BETWEENNESS（之间）  : 标记 "B"，橙色 #ff9800
    //   类型 2 - INTERSECTION（相交） : 标记 "X"，红色 #f44336
    //   类型 3 - CONTAINMENT（包含）  : 标记 "C"，蓝色 #2196f3
    //
    // 约束节点通过 arg1 或 nodeId 字段获取，依赖 _pointMap 查找坐标。
    //
    // @param {CanvasRenderingContext2D} ctx - Canvas 2D 渲染上下文
    // @param {CSSStyleDeclaration} cs - 缓存的 getComputedStyle 结果（性能优化）
    // @param {string} fontMono - 缓存的等宽字体族名称（性能优化）
    // @returns {void}
    // ================================================================
    Lv00WebApp.prototype._drawConstraintMarkers = function(ctx, cs, fontMono) {
        if (!this.graph || !this.graph.constraints) return;

        // 性能优化：将约束标记颜色和字体的 CSS 变量读取移到循环外部
        // 中文说明：约束颜色和字体在同一帧内不会变化，只需读取一次
        var constraintColors = {
            0: cs.getPropertyValue('--color-constraint-incidence').trim() || '#4caf50',
            1: cs.getPropertyValue('--color-constraint-betweenness').trim() || '#ff9800',
            2: cs.getPropertyValue('--color-constraint-intersection').trim() || '#f44336',
            3: cs.getPropertyValue('--color-constraint-containment').trim() || '#2196f3'
        };
        // 使用 CSS 变量 --font-mono 拼接字体，避免硬编码字体族
        var markerFont = '9px ' + fontMono;

        for (var i = 0; i < this.graph.constraints.length; i++) {
            var constraint = this.graph.constraints[i];
            var markerText = '';
            var markerColor = '#888';

            // 根据约束类型设置标记文字和对应的颜色
            switch (constraint.type) {
                case 0: // INCIDENCE - 关联约束
                    markerText = 'I';
                    markerColor = constraintColors[0];
                    break;
                case 1: // BETWEENNESS - 介于约束
                    markerText = 'B';
                    markerColor = constraintColors[1];
                    break;
                case 2: // INTERSECTION - 相交约束
                    markerText = 'X';
                    markerColor = constraintColors[2];
                    break;
                case 3: // CONTAINMENT - 包含约束
                    markerText = 'C';
                    markerColor = constraintColors[3];
                    break;
                default:
                    continue;
            }

            // 获取约束涉及的第一个参数节点 ID
            var nodeId = constraint.arg1 !== undefined ? constraint.arg1 :
                         constraint.nodeId !== undefined ? constraint.nodeId : -1;
            if (nodeId < 0) continue;

            // 通过点映射表查找节点坐标
            var point = this._pointMap ? this._pointMap[nodeId] : null;
            if (!point) continue;
            var pos = this.worldToScreen(point.x, point.y);

            // 在节点右上方绘制约束类型标记
            ctx.font = markerFont;
            ctx.fillStyle = markerColor;
            ctx.fillText(markerText, pos.x + 8, pos.y - 8);
        }
    };

    // ================================================================
    // 绘制端口（私有方法）
    //
    // 遍历所有函数块，在每个函数块的端口位置绘制连接点。
    // 端口的颜色根据方向区分：
    //   - 输入端口（direction === 'in'） : 蓝色 #2196f3
    //   - 输出端口（direction 非 'in'） : 绿色 #4caf50
    //
    // 每个端口包含填充圆形和边框，边框颜色跟随主题文字颜色。
    //
    // @param {CanvasRenderingContext2D} ctx - Canvas 2D 渲染上下文
    // @param {CSSStyleDeclaration} cs - 缓存的 getComputedStyle 结果（性能优化）
    // @returns {void}
    // ================================================================
    Lv00WebApp.prototype._drawPorts = function(ctx, cs) {
        if (!this.functionBlocks) return;
        var blockIds = Object.keys(this.functionBlocks);
        if (blockIds.length === 0) return;
        var tc = this._themeColors;

        // 性能优化：将端口颜色的 CSS 变量读取移到循环外部
        // 中文说明：端口颜色在同一帧内不会变化，只需读取一次
        // 修复：CSS 变量名从 --color-port-input 改为 --color-port-in，与 variables.css 保持一致
        var portInputColor = cs.getPropertyValue('--color-port-in').trim() || '#2196f3';
        // 修复：CSS 变量名从 --color-port-output 改为 --color-port-out，与 variables.css 保持一致
        var portOutputColor = cs.getPropertyValue('--color-port-out').trim() || '#4caf50';

        for (var i = 0; i < blockIds.length; i++) {
            var block = this.functionBlocks[blockIds[i]];
            if (!block.ports) continue;

            for (var j = 0; j < block.ports.length; j++) {
                var port = block.ports[j];
                var pos = this.worldToScreen(port.x, port.y);

                // 输入端口用蓝色，输出端口用绿色（使用 CSS 变量适配亮/暗主题）
                ctx.fillStyle = port.direction === 'in' ? portInputColor : portOutputColor;
                ctx.beginPath();
                ctx.arc(pos.x, pos.y, 4, 0, Math.PI * 2);
                ctx.fill();

                // 端口边框，颜色跟随主题
                ctx.strokeStyle = tc.text;
                ctx.lineWidth = 1;
                ctx.beginPath();
                ctx.arc(pos.x, pos.y, 4, 0, Math.PI * 2);
                ctx.stroke();
            }
        }
    };

    // ================================================================
    // 绘制函数块（私有方法）
    //
    // 遍历所有函数块，在画布上绘制每个函数块的矩形背景框、
    // 标题文字和类型签名信息。
    //
    // 渲染内容包括：
    //   1. 矩形背景（深色半透明填充 + 蓝色边框）
    //   2. 函数块名称（蓝色文字，位于左上角）
    //   3. 类型签名（灰色文字，格式：输入类型 -> 输出类型）
    //
    // 函数块的位置和尺寸由 block.x, block.y, block.width, block.height 定义，
    // 未指定时使用默认值（位置 (0,0)，尺寸 120x60）。
    //
    // @param {CanvasRenderingContext2D} ctx - Canvas 2D 渲染上下文
    // @param {CSSStyleDeclaration} cs - 缓存的 getComputedStyle 结果（性能优化）
    // @param {string} fontMono - 缓存的等宽字体族名称（性能优化）
    // @returns {void}
    // ================================================================
    Lv00WebApp.prototype._drawFunctionBlocks = function(ctx, cs, fontMono) {
        if (!this.functionBlocks) return;

        // 如果函数块视图已折叠，跳过绘制
        if (this._blockViewFolded) return;
        var blockIds = Object.keys(this.functionBlocks);
        if (blockIds.length === 0) return;

        // 性能优化：将函数块样式相关的 CSS 变量读取移到循环外部
        // 中文说明：函数块的颜色和字体在同一帧内不会变化，只需读取一次
        var blockBg = cs.getPropertyValue('--color-block-bg').trim() || 'rgba(40, 44, 52, 0.9)';
        var blockBorder = cs.getPropertyValue('--color-block-border').trim() || '#58a6ff';
        var blockTitle = cs.getPropertyValue('--color-block-title').trim() || '#58a6ff';
        // 修复：CSS 变量名从 --color-block-type 改为 --color-block-type-text，与 variables.css 保持一致
        var blockTypeInfo = cs.getPropertyValue('--color-block-type-text').trim() || '#8b949e';
        // 使用 CSS 变量 --font-mono 拼接字体，避免硬编码字体族
        var titleFont = '10px ' + fontMono;
        var typeFont = '9px ' + fontMono;

        for (var i = 0; i < blockIds.length; i++) {
            var block = this.functionBlocks[blockIds[i]];

            // 函数块的位置和尺寸（未指定时使用默认值）
            var bx = block.x !== undefined ? block.x : 0;
            var by = block.y !== undefined ? block.y : 0;
            var bw = block.width || 120;
            var bh = block.height || 60;
            var pos = this.worldToScreen(bx, by);

            // 绘制函数块背景（深色半透明，使用 CSS 变量适配亮/暗主题）
            ctx.fillStyle = blockBg;
            ctx.strokeStyle = blockBorder;
            ctx.lineWidth = 1;
            ctx.fillRect(pos.x, pos.y, bw, bh);
            ctx.strokeRect(pos.x, pos.y, bw, bh);

            // 绘制函数块标题（函数名）
            ctx.font = titleFont;
            ctx.fillStyle = blockTitle;
            ctx.fillText(block.name || blockIds[i], pos.x + 6, pos.y + 14);

            // 绘制类型签名信息（输入类型 -> 输出类型）
            if (block.inputType || block.outputType) {
                ctx.fillStyle = blockTypeInfo;
                ctx.font = typeFont;
                var typeStr = (block.inputType || '?') + ' -> ' + (block.outputType || '?');
                ctx.fillText(typeStr, pos.x + 6, pos.y + 28);
            }
        }
    };

})();
