/**
 * interaction.js - 交互事件处理模块（优化版）
 *
 * 从 app.js 中提取的交互事件处理方法，挂载到 Lv00WebApp.prototype 上。
 * 包含鼠标事件、触摸事件、滚轮缩放、键盘快捷键、
 * 右键菜单、框选、拖拽、主题切换、搜索等功能。
 *
 * 模块结构：
 * - 鼠标/触摸事件：onMouseDown、onMouseMove、onMouseUp、onMouseLeave
 * - 触摸支持：onTouchStart、onTouchMove、onTouchEnd、_getPinchDistance
 * - 滚轮缩放：onWheel
 * - 键盘快捷键：_initKeyboard、onKeyDown、_zoomAtCenter、_resetView
 * - 框选功能：_initBoxSelect、_finishBoxSelect、_cancelBoxSelect
 * - 拖拽点：_initDragPoint、_finishDragPoint
 * - 右键菜单：_initContextMenu
 * - 主题切换：_initThemeToggle
 * - 探测工具：_initProbe
 * - 区域工具：_initRegionTool
 * - 工具提示：_initTooltips
 * - 光标管理：_updateCursor
 * - 事件清理：_cleanupEventListeners
 *
 * @description 处理用户与画布的所有交互操作，包括鼠标、触摸、键盘等输入设备。
 *              支持平移、缩放、框选、拖拽点、右键菜单等多种交互模式。
 * @module interaction
 * @requires Lv00WebApp 构造函数（app.js）
 * @requires render.js（渲染模块）
 * @requires utils.js（辅助工具函数）
 * @since 3.0.0
 */
(function() {
    'use strict';

    // ---- 交互模块常量（从 constants.js 导入）------------------------------
    /** @constant {Object} 交互模块常量命名空间引用 */
    var ic = Lv00Const.interaction;

    // ================================================================
    /**
     * 鼠标按下事件处理
     *
     * @description 根据当前工具类型执行不同的操作逻辑：
     *   - point（添加点工具）：在世界坐标系的点击位置创建一个新的点
     *   - segment（线段工具）：依次选择两个点创建线段连接
     *   - pan（平移工具）：开始拖拽画布平移操作
     *   - select（选择工具）：框选多个点或拖拽单个点
     *
     * 交互流程：
     *   1. 获取鼠标在画布上的相对坐标，转换为世界坐标
     *   2. 根据 currentTool 执行对应逻辑
     *   3. 框选模式下在空白区域按下鼠标开始框选
     *   4. 选择模式下点击点开始拖拽，未点击点则选中点
     *   5. 最后触发一次渲染更新画面
     *
     * @param {MouseEvent} e - 鼠标事件对象
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype.onMouseDown = function(e) {
        e.preventDefault();
        var rect = this.canvas.getBoundingClientRect();
        var x = e.clientX - rect.left;
        var y = e.clientY - rect.top;
        var worldPos = this.screenToWorld(x, y);

        if (this.currentTool === 'point') {
            this.addPoint(worldPos.x, worldPos.y);
        } else if (this.currentTool === 'segment') {
            var point = this.findPointAt(x, y);
            if (point) {
                if (this.selectedPoint) {
                    this.addSegment(this.selectedPoint.id, point.id);
                    this.selectedPoint = null;
                } else {
                    this.selectedPoint = point;
                    this.updateStatus('SELECT SECOND POINT / 选择第二个点');
                }
            }
        } else if (this.currentTool === 'pan') {
            this.isDragging = true;
            this.dragStart = { x: x, y: y };
            this.canvas.style.cursor = 'grabbing';
        } else if (this.currentTool === 'select') {
            // 框选：点击空白区域时开始
            if (e.button === 0 && !this._hitTestPoint(e)) {
                var selRect = this._getSelectionRect();
                this.isBoxSelecting = true;
                this.boxSelectStart = { x: e.clientX, y: e.clientY };
                this.selectedPoints = [];
                this.selectedPoint = null;
                if (selRect) {
                    selRect.style.display = 'block';
                    selRect.style.left = e.clientX + 'px';
                    selRect.style.top = e.clientY + 'px';
                    selRect.style.width = '0px';
                    selRect.style.height = '0px';
                }
            } else if (e.button === 0) {
                // 拖拽点
                var hit = this._findPointAt(e);
                if (hit) {
                    this.isDraggingPoint = true;
                    this.dragPoint = hit;
                    this.canvas.style.cursor = 'grabbing';
                } else {
                    var point = this.findPointAt(x, y);
                    if (point) {
                        this.selectedPoint = point;
                        this.updateProperties(point);
                    } else {
                        this.selectedPoint = null;
                        this.updateProperties(null);
                    }
                }
            }
        }

        this.render();
    };

    // ================================================================
    /**
     * 鼠标移动事件处理
     *
     * @description 处理鼠标在画布上移动时的所有交互逻辑：
     *   - 节流控制：避免过于频繁触发（约 120fps）
     *   - 坐标显示：更新状态栏的世界坐标显示
     *   - 画布平移：在平移工具下拖拽移动画布
     *   - 框选更新：实时更新框选矩形的大小
     *   - 点拖拽：将点拖拽到新位置（支持 Alt 键吸附网格）
     *   - 探测工具：显示悬停点的详细信息
     *   - 悬停检测：检测并高亮鼠标下的点
     *
     * 性能优化：
     *   - 使用节流控制鼠标移动事件频率
     *   - 仅当悬停点发生变化时才触发渲染
     *
     * @param {MouseEvent} e - 鼠标事件对象
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype.onMouseMove = function(e) {
        var now = performance.now();

        // 节流鼠标移动事件
        if (now - this._lastMouseMoveTime < this._mouseMoveThrottleMs) {
            return;
        }
        this._lastMouseMoveTime = now;

        var rect = this.canvas.getBoundingClientRect();
        var x = e.clientX - rect.left;
        var y = e.clientY - rect.top;
        this.mouseScreenX = x;
        this.mouseScreenY = y;

        var worldPos = this.screenToWorld(x, y);
        this.mouseWorldX = worldPos.x;
        this.mouseWorldY = worldPos.y;

        // 更新状态栏坐标显示
        var coordsEl = document.getElementById('statusCoords');
        if (coordsEl) {
            var mx = Math.round(worldPos.x * 10) / 10;
            var my = Math.round(worldPos.y * 10) / 10;
            coordsEl.textContent = 'X:' + mx + ' Y:' + my;
        }

        // 平移画布
        if (this.isDragging && this.currentTool === 'pan') {
            var dx = (x - this.dragStart.x) / this.scale;
            var dy = (y - this.dragStart.y) / this.scale;
            this.offsetX += dx;
            this.offsetY += dy;
            this.dragStart = { x: x, y: y };
        }

        // 框选：更新选择矩形
        if (this.isBoxSelecting) {
            var selRect = this._getSelectionRect();
            if (selRect) {
                var bx = Math.min(e.clientX, this.boxSelectStart.x);
                var by = Math.min(e.clientY, this.boxSelectStart.y);
                var bw = Math.abs(e.clientX - this.boxSelectStart.x);
                var bh = Math.abs(e.clientY - this.boxSelectStart.y);
                selRect.style.left = bx + 'px';
                selRect.style.top = by + 'px';
                selRect.style.width = bw + 'px';
                selRect.style.height = bh + 'px';
            }
        }

        // 拖拽点：更新位置
        if (this.isDraggingPoint && this.dragPoint) {
            var dragWorld = this.screenToWorld(x, y);

            // 如果按住Alt键且显示网格，则将坐标吸附到最近的网格点
            if (this._dragSnapToGrid && this._showGrid !== false) {
                var gridSizeCss = this._getGridSize();  // CSS像素单位的网格间距
                var gridSizeWorld = gridSizeCss / this.scale;  // 转换为世界坐标单位
                dragWorld.x = Math.round(dragWorld.x / gridSizeWorld) * gridSizeWorld;
                dragWorld.y = Math.round(dragWorld.y / gridSizeWorld) * gridSizeWorld;
            }

            this.dragPoint.x = dragWorld.x;
            this.dragPoint.y = dragWorld.y;
            this.render();
            return;
        }

        // 探测工具提示
        if (this.currentTool === 'probe') {
            this._updateProbeTooltip(e);
        }

        // 悬停检测
        var prev = this.hoveredPoint;
        this.hoveredPoint = this.findPointAt(x, y);
        if (this.hoveredPoint !== prev) {
            this._updateCursor();
            this.render();
        }
    };

    // ================================================================
    /**
     * 鼠标松开事件处理
     *
     * @description 结束所有进行中的拖拽操作：
     *   - 平移操作：结束画布平移，恢复默认光标
     *   - 框选操作：调用 _finishBoxSelect 完成框选
     *   - 拖拽点操作：调用 _finishDragPoint 完成点拖拽
     *
     * 与 onMouseLeave 的区别：
     *   - onMouseUp 仅在鼠标松开按钮时触发
     *   - onMouseLeave 在鼠标离开画布区域时触发
     *
     * @param {MouseEvent} e - 鼠标事件对象
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype.onMouseUp = function(e) {
        if (this.currentTool === 'pan') {
            this.isDragging = false;
            this.canvas.style.cursor = 'grab';
        }

        // 框选结束
        if (this.isBoxSelecting) {
            this._finishBoxSelect(e);
        }

        // 拖拽结束
        if (this.isDraggingPoint && this.dragPoint) {
            this._finishDragPoint();
        }
    };

    // ================================================================
    /**
     * 鼠标离开画布事件处理
     *
     * @description 当鼠标从画布区域移动到外部时触发：
     *   - 结束所有进行中的交互状态（平移、框选、拖拽）
     *   - 清除悬停点高亮
     *   - 隐藏探测工具提示框
     *   - 触发最终渲染确保画面状态正确
     *
     * 与 onMouseUp 的区别：
     *   - onMouseLeave 在鼠标完全离开画布时触发
     *   - onMouseUp 仅在松开鼠标按钮时触发
     *
     * @param {MouseEvent} e - 鼠标事件对象
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype.onMouseLeave = function(e) {
        if (this.isDragging) {
            this.isDragging = false;
        }
        if (this.isBoxSelecting) {
            this._cancelBoxSelect();
        }
        if (this.isDraggingPoint) {
            this._finishDragPoint();
        }
        this.hoveredPoint = null;
        this._hideProbeTooltip();
        this.render();
    };

    // ================================================================
    /**
     * 获取选择矩形 DOM 元素（带缓存）
     *
     * @description 获取用于框选的矩形选择框 DOM 元素。
     *              使用实例缓存避免频繁的 DOM 查询操作。
     *
     * @returns {HTMLElement|null} 选择矩形 DOM 元素，未找到返回 null
     */
    // ================================================================
    Lv00WebApp.prototype._getSelectionRect = function() {
        if (!this._selRectCache) {
            this._selRectCache = document.getElementById('selectionRect');
        }
        return this._selRectCache;
    };

    // ================================================================
    /**
     * 完成框选操作
     *
     * @description 当鼠标松开时调用此方法完成框选：
     *   - 隐藏选择矩形框
     *   - 计算选择矩形内的所有点（世界坐标转换后判断是否在矩形内）
     *   - 更新选中点列表 selectedPoints
     *   - 如果只选中一个点，同步更新 selectedPoint
     *   - 刷新属性面板显示
     *   - 记录日志并触发渲染
     *
     * @param {MouseEvent} e - 鼠标事件对象
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype._finishBoxSelect = function(e) {
        this.isBoxSelecting = false;
        var selRect = this._getSelectionRect();
        if (selRect) selRect.style.display = 'none';

        var canvasRect = this.canvas.getBoundingClientRect();
        var x1 = Math.min(e.clientX, this.boxSelectStart.x) - canvasRect.left;
        var y1 = Math.min(e.clientY, this.boxSelectStart.y) - canvasRect.top;
        var x2 = Math.max(e.clientX, this.boxSelectStart.x) - canvasRect.left;
        var y2 = Math.max(e.clientY, this.boxSelectStart.y) - canvasRect.top;

        this.selectedPoints = [];
        for (var i = 0; i < this.points.length; i++) {
            var sx = this.worldToScreenX(this.points[i].x);
            var sy = this.worldToScreenY(this.points[i].y);
            if (sx >= x1 && sx <= x2 && sy >= y1 && sy <= y2) {
                this.selectedPoints.push(this.points[i]);
            }
        }

        if (this.selectedPoints.length === 1) {
            this.selectedPoint = this.selectedPoints[0];
        }

        this.updateProperties(this.selectedPoint);
        this.appendLog('Box selected ' + this.selectedPoints.length + ' points', 'info');
        this.render();
    };

    // ================================================================
    /**
     * 取消框选操作
     *
     * @description 取消当前进行中的框选操作：
     *   - 重置框选状态标志 isBoxSelecting = false
     *   - 隐藏选择矩形框 DOM 元素
     *
     * 使用场景：
     *   - 用户按下 Escape 键取消框选
     *   - 鼠标移出画布区域时自动取消
     *
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype._cancelBoxSelect = function() {
        this.isBoxSelecting = false;
        var selRect = this._getSelectionRect();
        if (selRect) selRect.style.display = 'none';
    };

    // ================================================================
    /**
     * 完成拖拽点操作
     *
     * @description 当拖拽点结束时调用此方法：
     *   - 记录拖拽日志（包含点的 ID 和最终坐标）
     *   - 将拖拽后的坐标同步回后端图数据（使用有理数坐标）
     *   - 保存撤销状态快照（允许用户撤销此次拖拽）
     *   - 重置拖拽相关状态标志
     *   - 更新鼠标光标样式
     *
     * 坐标同步说明：
     *   - 如果后端支持有理数坐标（coordCreateRational），使用分子/分母形式
     *   - 否则直接使用浮点数坐标
     *
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype._finishDragPoint = function() {
        this.appendLog('Moved point ' + this.dragPoint.id + ' to (' + this.dragPoint.x.toFixed(2) + ', ' + this.dragPoint.y.toFixed(2) + ')', 'info');
        // 将坐标同步回后端图数据
        if (this.graph && this.graph.nodes) {
            for (var di = 0; di < this.graph.nodes.length; di++) {
                var node = this.graph.nodes[di];
                if (node.id === this.dragPoint.id && node.geomType === this._geomType().POINT) {
                    if (this.jsBackend && this.jsBackend.coordCreateRational) {
                        node.coordX = this.jsBackend.coordCreateRational(Math.round(this.dragPoint.x * 100), 100);
                        node.coordY = this.jsBackend.coordCreateRational(Math.round(this.dragPoint.y * 100), 100);
                    } else {
                        node.coordX = this.dragPoint.x;
                        node.coordY = this.dragPoint.y;
                    }
                    break;
                }
            }
        }
        this._saveUndoState();
        this.isDraggingPoint = false;
        this.dragPoint = null;
        this._updateCursor();
    };

    // ================================================================
    /**
     * 触摸开始事件处理（移动端支持）
     *
     * @description 处理触摸设备上的手指按下事件：
     *   - 单指触摸：模拟鼠标按下事件（onMouseDown）
     *   - 双指触摸：记录双指初始距离，为缩放做准备
     *
     * 触摸到鼠标事件转换：
     *   - 创建新的 MouseEvent 对象，传递 clientX、clientY 和 button
     *   - 确保与桌面端交互逻辑保持一致
     *
     * @param {TouchEvent} e - 触摸事件对象
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype.onTouchStart = function(e) {
        if (e.touches.length === 1) {
            var touch = e.touches[0];
            var mouseEvent = new MouseEvent('mousedown', {
                clientX: touch.clientX,
                clientY: touch.clientY,
                button: 0
            });
            this.onMouseDown(mouseEvent);
        } else if (e.touches.length === 2) {
            // 双指缩放开始
            this._lastPinchDist = this._getPinchDistance(e.touches);
        }
    };

    // ================================================================
    /**
     * 触摸移动事件处理（移动端支持）
     *
     * @description 处理触摸设备上的手指移动事件：
     *   - 单指触摸：模拟鼠标移动事件（onMouseMove）
     *   - 双指触摸：以双指中点为中心进行缩放
     *
     * 双指缩放算法：
     *   1. 计算双指之间的距离
     *   2. 计算缩放比例（新距离 / 旧距离）
     *   3. 以双指中点为缩放中心点
     *   4. 更新画布 scale 和 offset，保持中点世界坐标不变
     *
     * @param {TouchEvent} e - 触摸事件对象
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype.onTouchMove = function(e) {
        e.preventDefault();
        if (e.touches.length === 1) {
            var touch = e.touches[0];
            var mouseEvent = new MouseEvent('mousemove', {
                clientX: touch.clientX,
                clientY: touch.clientY
            });
            this.onMouseMove(mouseEvent);
        } else if (e.touches.length === 2) {
            // 双指缩放
            var dist = this._getPinchDistance(e.touches);
            if (this._lastPinchDist > 0) {
                var scale = dist / this._lastPinchDist;
                var newScale = Math.max(Lv00Const.SCALE_MIN, Math.min(Lv00Const.SCALE_MAX, this.scale * scale));

                // 以双指中点为中心缩放
                var midX = (e.touches[0].clientX + e.touches[1].clientX) / 2;
                var midY = (e.touches[0].clientY + e.touches[1].clientY) / 2;
                var rect = this.canvas.getBoundingClientRect();
                var canvasX = midX - rect.left;
                var canvasY = midY - rect.top;

                var worldBefore = this.screenToWorld(canvasX, canvasY);
                this.scale = newScale;
                var worldAfter = this.screenToWorld(canvasX, canvasY);

                this.offsetX += worldAfter.x - worldBefore.x;
                this.offsetY += worldAfter.y - worldBefore.y;

                this.render();
            }
            this._lastPinchDist = dist;
        }
    };

    // ================================================================
    /**
     * 触摸结束事件处理（移动端支持）
     *
     * @description 处理触摸设备上的手指抬起事件：
     *   - 模拟鼠标松开事件（onMouseUp）
     *   - 重置双指缩放距离为 0
     *
     * @param {TouchEvent} e - 触摸事件对象
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype.onTouchEnd = function(e) {
        var mouseEvent = new MouseEvent('mouseup', {
            button: 0
        });
        this.onMouseUp(mouseEvent);
        this._lastPinchDist = 0;
    };

    // ================================================================
    /**
     * 计算双指触摸的距离
     *
     * @description 计算两个触摸点之间的欧几里得距离（像素单位）。
     *              用于判断双指缩放的手势幅度。
     *
     * @param {TouchList} touches - 触摸点列表（需包含2个触摸点）
     * @returns {number} 两指之间的像素距离
     */
    // ================================================================
    Lv00WebApp.prototype._getPinchDistance = function(touches) {
        var dx = touches[0].clientX - touches[1].clientX;
        var dy = touches[0].clientY - touches[1].clientY;
        return Math.sqrt(dx * dx + dy * dy);
    };

    // ================================================================
    /**
     * 滚轮缩放事件处理
     *
     * @description 处理鼠标滚轮缩放画布：
     *   - 以鼠标指针位置为中心进行缩放
     *   - 保持鼠标下的世界坐标位置不变（缩放焦点跟随鼠标）
     *   - 使用平滑缩放因子，滚轮滚动越多缩放越快
     *   - 限制缩放范围在 SCALE_MIN 到 SCALE_MAX 之间
     *
     * 缩放算法：
     *   1. 记录缩放前鼠标位置对应的世界坐标
     *   2. 计算新的缩放比例
     *   3. 调整 offset 使鼠标位置的世界坐标保持不变
     *
     * @param {WheelEvent} e - 滚轮事件对象
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype.onWheel = function(e) {
        e.preventDefault();

        var rect = this.canvas.getBoundingClientRect();
        var mx = e.clientX - rect.left;
        var my = e.clientY - rect.top;

        // 缩放前鼠标位置对应的世界坐标
        var worldBefore = this.screenToWorld(mx, my);

        // 平滑缩放因子
        var factor = Math.pow(ic.ZOOM_FACTOR_SMOOTH, -e.deltaY);
        var newScale = this.scale * factor;

        // 限制缩放范围（使用 Lv00Const 命名空间的常量）
        if (newScale < Lv00Const.SCALE_MIN) newScale = Lv00Const.SCALE_MIN;
        if (newScale > Lv00Const.SCALE_MAX) newScale = Lv00Const.SCALE_MAX;

        this.scale = newScale;

        // 调整偏移量，使鼠标位置的世界坐标保持不变
        var worldAfter = this.screenToWorld(mx, my);
        this.offsetX += worldAfter.x - worldBefore.x;
        this.offsetY += worldAfter.y - worldBefore.y;

        this.render();
    };

    // ================================================================
    /**
     * 键盘快捷键初始化
     *
     * @description 注册全局键盘事件监听器，支持：
     *   - 键盘按下事件：触发 onKeyDown 处理快捷键
     *   - 键盘松开事件：更新 _keysPressed 状态
     *   - 忽略输入框中的键盘事件（INPUT、TEXTAREA、SELECT）
     *
     * 事件监听管理：
     *   - 保存事件处理器引用以便后续清理（_cleanupEventListeners）
     *   - 使用箭头函数包裹以保持 this 上下文
     *
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype._initKeyboard = function() {
        var self = this;

        this._kbdKeydown = function(e) {
            // 忽略输入框中的快捷键
            if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA' || e.target.tagName === 'SELECT') {
                return;
            }

            self._keysPressed[e.key] = true;
            self.onKeyDown(e);
        };

        this._kbdKeyup = function(e) {
            self._keysPressed[e.key] = false;
        };

        document.addEventListener('keydown', this._kbdKeydown);
        document.addEventListener('keyup', this._kbdKeyup);
    };

    // ================================================================
    /**
     * 键盘按下事件处理
     *
     * @description 处理各类键盘快捷键操作：
     *   - Ctrl+Z / Cmd+Z：撤销操作
     *   - Ctrl+Y / Cmd+Y 或 Ctrl+Shift+Z：重做操作
     *   - Delete / Backspace：删除选中的节点
     *   - V / Escape：切换到选择工具
     *   - P：切换到添加点工具
     *   - L：切换到线段工具
     *   - H / Space：切换到平移工具
     *   - + / =：放大画布
     *   - - / _：缩小画布
     *   - Ctrl+0：重置视图到默认状态
     *
     * 快捷键优先级：
     *   - 组合键（Ctrl/Cmd + 键）优先于单键
     *   - 删除操作优先于工具切换
     *
     * @param {KeyboardEvent} e - 键盘事件对象
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype.onKeyDown = function(e) {
        var self = this;

        // Ctrl/Cmd + Z: 撤销
        if ((e.ctrlKey || e.metaKey) && e.key === 'z' && !e.shiftKey) {
            e.preventDefault();
            this.undo();
            return;
        }

        // Ctrl/Cmd + Y 或 Ctrl/Cmd + Shift + Z: 重做
        if ((e.ctrlKey || e.metaKey) && (e.key === 'y' || (e.key === 'z' && e.shiftKey))) {
            e.preventDefault();
            this.redo();
            return;
        }

        // Delete: 删除选中节点
        if (e.key === 'Delete' || e.key === 'Backspace') {
            if (this.selectedPoint) {
                this.graphRemoveNode();
            }
            return;
        }

        // 工具切换快捷键
        switch (e.key.toLowerCase()) {
            case 'v':
            case 'escape':
                this.setTool('select');
                break;
            case 'p':
                this.setTool('point');
                break;
            case 'l':
                this.setTool('segment');
                break;
            case 'h':
            case ' ':
                this.setTool('pan');
                break;
            case '+':
            case '=':
                this._zoomAtCenter(ic.ZOOM_FACTOR_STEP);
                break;
            case '-':
            case '_':
                this._zoomAtCenter(1 / ic.ZOOM_FACTOR_STEP);
                break;
            case '0':
                if (e.ctrlKey || e.metaKey) {
                    e.preventDefault();
                    this._resetView();
                }
                break;
        }
    };

    // ================================================================
    /**
     * 以画布中心为基准进行缩放
     *
     * @description 以画布中心点为缩放基准执行缩放操作：
     *   - 记录缩放前画布中心对应的世界坐标
     *   - 应用缩放因子更新 scale
     *   - 调整 offset 使画布中心的世界坐标保持不变
     *   - 限制缩放范围在允许的最小值和最大值之间
     *   - 触发渲染更新画面
     *
     * @param {number} factor - 缩放因子（>1 放大，<1 缩小）
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype._zoomAtCenter = function(factor) {
        var w = this.canvas.offsetWidth;
        var h = this.canvas.offsetHeight;
        var worldBefore = this.screenToWorld(w / 2, h / 2);

        this.scale *= factor;
        if (this.scale < Lv00Const.SCALE_MIN) this.scale = Lv00Const.SCALE_MIN;
        if (this.scale > Lv00Const.SCALE_MAX) this.scale = Lv00Const.SCALE_MAX;

        var worldAfter = this.screenToWorld(w / 2, h / 2);
        this.offsetX += worldAfter.x - worldBefore.x;
        this.offsetY += worldAfter.y - worldBefore.y;

        this.render();
    };

    // ================================================================
    /**
     * 重置视图到默认状态
     *
     * @description 将画布视图恢复到初始状态：
     *   - 缩放比例恢复为 1（100%）
     *   - 水平和垂直偏移量归零
     *   - 触发渲染更新画面
     *   - 记录日志确认操作
     *
     * 调用场景：
     *   - Ctrl+0 快捷键触发
     *   - 用户点击重置按钮
     *
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype._resetView = function() {
        this.scale = 1;
        this.offsetX = 0;
        this.offsetY = 0;
        this.render();
        this.appendLog('View reset / 视图已重置', 'info');
    };

    // ================================================================
    /**
     * 右键菜单初始化
     *
     * @description 在画布上绑定右键菜单事件，支持：
     *   - 右键点击画布：显示右键菜单，选中点击位置的点
     *   - 点击其他区域：隐藏右键菜单
     *   - 菜单项操作：删除节点、查看属性、合并节点、归一化
     *
     * 菜单项说明：
     *   - delete：删除当前选中的节点
     *   - properties：显示选中节点的属性面板
     *   - merge：提示用户选择两个节点进行合并（开发中）
     *   - normalize：对图执行归一化操作
     *
     * 事件管理：
     *   - 保存所有事件处理器引用以便后续清理
     *   - 菜单项事件使用闭包避免循环引用问题
     *
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype._initContextMenu = function() {
        var self = this;
        var menu = document.getElementById('contextMenu');
        if (!menu) return;

        // 保存事件处理器引用以便后续清理
        this._ctxMenuCanvasHandler = function(e) {
            e.preventDefault();
            var point = self._findPointAt(e);
            if (point) {
                self.selectedPoint = point;
                self.render();
            }
            menu.style.left = e.clientX + 'px';
            menu.style.top = e.clientY + 'px';
            menu.classList.add('active');
        };

        this._ctxMenuDocHandler = function(e) {
            if (!menu.contains(e.target)) {
                menu.classList.remove('active');
            }
        };

        // 右键点击画布时显示菜单
        this.canvas.addEventListener('contextmenu', this._ctxMenuCanvasHandler);

        // 点击其他区域时隐藏菜单
        document.addEventListener('click', this._ctxMenuDocHandler);

        // 保存菜单项事件处理器引用
        this._ctxMenuItemHandlers = [];

        // 菜单项点击事件
        var items = menu.querySelectorAll('.context-menu-item');
        for (var i = 0; i < items.length; i++) {
            (function(item) {
                var handler = function() {
                    var action = item.getAttribute('data-action');
                    menu.classList.remove('active');

                    switch (action) {
                        case 'delete':
                            if (self.selectedPoint) {
                                self.graphRemoveNode();
                            }
                            break;
                        case 'properties':
                            self.updateProperties(self.selectedPoint);
                            break;
                        case 'merge':
                            self.appendLog('MERGE: 请选择两个节点进行合并 / Select two nodes to merge', 'info');
                            break;
                        case 'normalize':
                            self.graphNormalize();
                            break;
                    }
                };
                self._ctxMenuItemHandlers.push({ element: item, handler: handler });
                item.addEventListener('click', handler);
            })(items[i]);
        }
    };

    // ================================================================
    /**
     * 框选功能初始化
     *
     * @description 初始化框选功能的额外配置：
     *   - 注册键盘事件监听器检测 Shift 键状态
     *   - Shift + 框选：追加到已有选择（而非替换）
     *
     * 注意：框选核心逻辑已在 interaction 模块的鼠标事件中实现
     *
     * 事件管理：
     *   - 保存事件处理器引用以便后续清理
     *
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype._initBoxSelect = function() {
        // 框选核心逻辑已在 interaction 模块的鼠标事件中实现
        // 此处可添加额外的框选配置
        // 例如：Shift 键追加选择模式
        var self = this;

        this._boxSelKeydown = function(e) {
            if (e.key === 'Shift' && self.isBoxSelecting) {
                // Shift + 框选：追加到已有选择
                self._shiftBoxSelect = true;
            }
        };

        this._boxSelKeyup = function(e) {
            if (e.key === 'Shift') {
                self._shiftBoxSelect = false;
            }
        };

        document.addEventListener('keydown', this._boxSelKeydown);
        document.addEventListener('keyup', this._boxSelKeyup);
    };

    // ================================================================
    /**
     * 拖拽点功能初始化
     *
     * @description 初始化拖拽点功能的额外配置：
     *   - 注册键盘事件监听器检测 Alt 键状态
     *   - Alt + 拖拽：启用网格吸附功能（拖拽点时自动吸附到最近的网格点）
     *
     * 网格吸附说明：
     *   - 将点的坐标四舍五入到最近的网格交点
     *   - 使点更容易对齐，便于构造规则图形
     *
     * 注意：拖拽核心逻辑已在 interaction 模块的鼠标事件中实现
     *
     * 事件管理：
     *   - 保存事件处理器引用以便后续清理
     *
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype._initDragPoint = function() {
        // 拖拽核心逻辑已在 interaction 模块的鼠标事件中实现
        // 此处可添加额外的拖拽配置
        // 例如：按住 Alt 键时吸附到网格
        this._dragSnapToGrid = false;

        var self = this;
        this._dragKeydown = function(e) {
            if (e.key === 'Alt') {
                self._dragSnapToGrid = true;
            }
        };
        this._dragKeyup = function(e) {
            if (e.key === 'Alt') {
                self._dragSnapToGrid = false;
            }
        };
        document.addEventListener('keydown', this._dragKeydown);
        document.addEventListener('keyup', this._dragKeyup);
    };

    // ================================================================
    /**
     * 主题切换初始化
     *
     * @description 初始化主题切换功能：
     *   - 绑定主题切换按钮的点击事件
     *   - 点击时切换 body 的 light-theme CSS 类
     *   - 记录当前主题状态到日志
     *   - 触发渲染以应用新主题的颜色
     *
     * 主题模式：
     *   - 暗色模式（默认）：深色背景，浅色前景
     *   - 亮色模式：白色/浅灰背景，深色前景
     *
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype._initThemeToggle = function() {
        var self = this;
        var btn = document.getElementById('btnThemeToggle');
        if (!btn) return;

        btn.addEventListener('click', function() {
            document.body.classList.toggle('light-theme');
            var isLight = document.body.classList.contains('light-theme');
            self.appendLog('Theme: ' + (isLight ? 'LIGHT / 亮色' : 'DARK / 暗色'), 'info');
            self.render();
        });
    };

    // ================================================================
    /**
     * 探测工具初始化
     *
     * @description 初始化探测工具的提示框显示/隐藏逻辑：
     *   - _updateProbeTooltip：更新探测提示框内容，显示悬停点的详细信息
     *     * 显示内容：点ID（n{id}）、X坐标、Y坐标
     *     * 位置跟随鼠标，带偏移量避免遮挡
     *   - _hideProbeTooltip：隐藏探测提示框
     *
     * 探测工具使用场景：
     *   - 用户选择探测工具（probe tool）
     *   - 鼠标悬停在点上时显示详细信息
     *   - 鼠标移开或离开画布时隐藏提示框
     *
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype._initProbe = function() {
        var self = this;

        // 更新探测提示框内容
        this._updateProbeTooltip = function(e) {
            var tooltip = document.getElementById('probeTooltip');
            if (!tooltip) return;

            var point = this._findPointAt(e);
            if (point) {
                tooltip.classList.add('active');
                tooltip.style.left = (e.clientX + ic.PROBE_TOOLTIP_OFFSET) + 'px';
                tooltip.style.top = (e.clientY + ic.PROBE_TOOLTIP_OFFSET) + 'px';
                document.getElementById('probeX').textContent = point.x.toFixed(3);
                document.getElementById('probeY').textContent = point.y.toFixed(3);
                document.getElementById('probeId').textContent = 'n' + point.id;
            } else {
                tooltip.classList.remove('active');
            }
        };

        // 隐藏探测提示框
        this._hideProbeTooltip = function() {
            var tooltip = document.getElementById('probeTooltip');
            if (tooltip) {
                tooltip.classList.remove('active');
            }
        };
    };

    // ================================================================
    /**
     * 区域工具初始化
     *
     * @description 初始化区域工具的键盘快捷键支持：
     *   - Enter 键：完成区域创建（至少需要 3 个点）
     *   - Escape 键：取消当前区域创建
     *
     * 区域创建流程：
     *   1. 用户选择区域工具（region tool）
     *   2. 点击多个点作为区域顶点
     *   3. 按 Enter 键完成区域创建
     *   4. 按 Escape 键取消创建
     *
     * 注意：点击添加点的逻辑已在 onMouseDown 中处理
     *
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype._initRegionTool = function() {
        // 区域工具的点击逻辑已在 onMouseDown 中处理
        // 当 currentTool === 'region' 时，点击添加点到区域
        // 此处添加区域完成（双击或按 Enter）的处理
        var self = this;

        this._regionKeydown = function(e) {
            if (e.key === 'Enter' && self.currentTool === 'region') {
                if (self.regionPoints && self.regionPoints.length >= 3) {
                    self.regions.push({ points: self.regionPoints.slice() });
                    self.appendLog('Region created with ' + self.regionPoints.length + ' points / 区域已创建', 'info');
                    self.regionPoints = [];
                    self.render();
                } else {
                    self.appendLog('Need at least 3 points for a region / 至少需要3个点来创建区域', 'warn');
                }
            }
            if (e.key === 'Escape' && self.currentTool === 'region') {
                self.regionPoints = [];
                self.appendLog('Region creation cancelled / 区域创建已取消', 'info');
                self.render();
            }
        };

        document.addEventListener('keydown', this._regionKeydown);
    };

    // ================================================================
    /**
     * 工具提示初始化
     *
     * @description 初始化带有 data-tooltip 属性的元素的工具提示。
     *              目前使用纯 CSS :hover 伪类实现。
     *
     * 未来扩展方向：
     *   - 支持动态提示内容（如根据悬停元素动态生成提示）
     *   - 支持不同位置的提示框（top/bottom/left/right）
     *   - 支持提示框箭头指示
     *
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype._initTooltips = function() {
        // 使用 CSS :hover 伪类和 data-tooltip 属性实现
        // 此方法保留用于未来扩展（如动态提示内容）
    };

    // ================================================================
    /**
     * 查找鼠标位置下的点（使用事件对象）
     *
     * @description 将鼠标事件坐标转换为画布相对坐标后查找点。
     *              是 findPointAt 的便捷包装方法。
     *
     * @param {MouseEvent} e - 鼠标事件对象
     * @returns {Object|null} 找到的点对象，未找到返回 null
     */
    // ================================================================
    Lv00WebApp.prototype._findPointAt = function(e) {
        var rect = this.canvas.getBoundingClientRect();
        var x = e.clientX - rect.left;
        var y = e.clientY - rect.top;
        return this.findPointAt(x, y);
    };

    // ================================================================
    /**
     * 命中测试：检测鼠标位置是否在某个点上
     *
     * @description 判断鼠标事件位置是否命中任何点。
     *              是 _findPointAt 的布尔返回值版本。
     *
     * 使用场景：
     *   - 在选择工具中判断是否应该开始框选
     *   - 判断右键菜单应该显示在哪个点上
     *
     * @param {MouseEvent} e - 鼠标事件对象
     * @returns {boolean} 是否命中某个点
     */
    // ================================================================
    Lv00WebApp.prototype._hitTestPoint = function(e) {
        return !!this._findPointAt(e);
    };

    // ================================================================
    /**
     * 查找指定屏幕坐标下的点
     *
     * @description 在所有点中查找距离指定屏幕坐标最近的点。
     *              使用命中阈值判断是否命中，阈值为 HIT_THRESHOLD_BASE / scale。
     *
     * 算法说明：
     *   - 将屏幕坐标转换为世界坐标
     *   - 计算所有点到鼠标位置的距离
     *   - 返回第一个在阈值范围内的点
     *
     * 性能优化：
     *   - 命中阈值随缩放级别动态调整
     *   - 放大时阈值变大（更容易选中点）
     *   - 缩小时阈值变小（更精确选中）
     *
     * @param {number} x - 屏幕 X 坐标（CSS 像素）
     * @param {number} y - 屏幕 Y 坐标（CSS 像素）
     * @returns {Object|null} 找到的点对象，未找到返回 null
     */
    // ================================================================
    Lv00WebApp.prototype.findPointAt = function(x, y) {
        var threshold = ic.HIT_THRESHOLD_BASE / this.scale;
        var worldPos = this.screenToWorld(x, y);

        for (var i = 0; i < this.points.length; i++) {
            var dx = this.points[i].x - worldPos.x;
            var dy = this.points[i].y - worldPos.y;
            var dist = Math.sqrt(dx * dx + dy * dy);
            if (dist < threshold) {
                return this.points[i];
            }
        }
        return null;
    };

    // ================================================================
    /**
     * 更新鼠标光标样式
     *
     * @description 根据当前工具和交互状态更新画布的鼠标光标样式。
     *
     * 光标样式映射：
     *   - select（选择工具）：default（默认箭头）
     *   - point（添加点工具）：crosshair（十字准星）
     *   - segment（线段工具）：crosshair（十字准星）
     *   - pan（平移工具）：grab（抓手）
     *   - region（区域工具）：crosshair（十字准星）
     *   - probe（探测工具）：help（问号）
     *
     * 调用时机：
     *   - 工具切换时
     *   - 悬停状态变化时
     *   - 拖拽状态变化时
     *
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype._updateCursor = function() {
        var cursors = {
            'select': 'default',
            'point': 'crosshair',
            'segment': 'crosshair',
            'pan': 'grab',
            'region': 'crosshair',
            'probe': 'help'
        };
        this.canvas.style.cursor = cursors[this.currentTool] || 'default';
    };

    // ================================================================
    /**
     * 事件监听器清理
     *
     * @description 移除所有由 _init* 方法注册的 DOM 事件监听器。
     *              应在页面卸载前调用（通常绑定到 beforeunload 事件）。
     *
     * 清理内容：
     *   - 键盘事件：_initKeyboard（keydown/keyup）
     *   - 右键菜单：_initContextMenu（contextmenu/click）
     *   - 框选功能：_initBoxSelect（keydown/keyup）
     *   - 拖拽点：_initDragPoint（keydown/keyup）
     *   - 区域工具：_initRegionTool（keydown）
     *
     * 保护机制：
     *   - 所有清理操作均检查对应引用是否存在
     *   - 清理后将引用置为 null 避免重复清理
     *
     * 注意：
     *   - _initProbe 仅设置实例方法，未注册 DOM 事件监听器，无需清理
     *   - _initThemeToggle / _initTooltips 同理
     *
     * @returns {void}
     */
    // ================================================================
    Lv00WebApp.prototype._cleanupEventListeners = function() {
        // --- 键盘事件 (_initKeyboard) ---
        if (this._kbdKeydown) {
            document.removeEventListener('keydown', this._kbdKeydown);
            this._kbdKeydown = null;
        }
        if (this._kbdKeyup) {
            document.removeEventListener('keyup', this._kbdKeyup);
            this._kbdKeyup = null;
        }

        // --- 右键菜单 (_initContextMenu) ---
        if (this._ctxMenuCanvasHandler && this.canvas) {
            this.canvas.removeEventListener('contextmenu', this._ctxMenuCanvasHandler);
            this._ctxMenuCanvasHandler = null;
        }
        if (this._ctxMenuDocHandler) {
            document.removeEventListener('click', this._ctxMenuDocHandler);
            this._ctxMenuDocHandler = null;
        }
        if (this._ctxMenuItemHandlers) {
            for (var i = 0; i < this._ctxMenuItemHandlers.length; i++) {
                var entry = this._ctxMenuItemHandlers[i];
                if (entry && entry.element && entry.handler) {
                    entry.element.removeEventListener('click', entry.handler);
                }
            }
            this._ctxMenuItemHandlers = null;
        }

        // --- 框选 (_initBoxSelect) ---
        if (this._boxSelKeydown) {
            document.removeEventListener('keydown', this._boxSelKeydown);
            this._boxSelKeydown = null;
        }
        if (this._boxSelKeyup) {
            document.removeEventListener('keyup', this._boxSelKeyup);
            this._boxSelKeyup = null;
        }

        // --- 拖拽 (_initDragPoint) ---
        if (this._dragKeydown) {
            document.removeEventListener('keydown', this._dragKeydown);
            this._dragKeydown = null;
        }
        if (this._dragKeyup) {
            document.removeEventListener('keyup', this._dragKeyup);
            this._dragKeyup = null;
        }

        // --- 区域工具 (_initRegionTool) ---
        if (this._regionKeydown) {
            document.removeEventListener('keydown', this._regionKeydown);
            this._regionKeydown = null;
        }

        // _initProbe 仅设置实例方法，未注册 DOM 事件监听器，无需清理
        // _initThemeToggle / _initTooltips 同理

        console.log('[Lv-00] 事件监听器已清理 / Event listeners cleaned up');
    };

})();
