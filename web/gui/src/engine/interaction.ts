/**
 * @module engine/interaction
 * @description Framework-agnostic interaction manager for the geometry canvas.
 *              Handles mouse, touch, and wheel events for tools including
 *              select, point, segment, pan, region, and probe.
 *              Keyboard shortcuts are managed by hooks/useKeyboard.ts.
 *              This class has NO React or framework dependencies.
 *
 *              与框架无关的几何画布交互管理器。
 *              处理鼠标、触摸和滚轮事件，支持以下工具：
 *              选择、加点、线段、平移、区域和探针。
 *              键盘快捷键由 hooks/useKeyboard.ts 统一管理。
 *              本类无 React 或框架依赖。
 */

import type { ViewportState } from '@/engine/renderer';
import type { Point, Segment, ToolType } from '@/types';
import { ZOOM_SMOOTH, SCALE_MIN, SCALE_MAX } from '@/utils/constants';
import { generateUniqueId } from '@/utils/idGenerator';
import { CIRCLE_APPROX_SIDES, MOUSE_MOVE_THROTTLE_MS } from '@/utils/constants';

// ================================================================
// Callback Interfaces / 回调接口
// ================================================================

/**
 * 交互管理器回调接口（合并定义）
 * InteractionCallbacks - 主机应用提供的回调接口，交互管理器通过此接口更新应用状态。
 *
 * 包含必需方法和可选方法：
 * - 必需方法：getViewport, getTool, addPoint, requestRender 等
 * - 可选方法：getSelectedPoint, updateDragPointPosition, undo 等
 *   可选方法通过 optional chaining (?) 安全调用
 */
export interface InteractionCallbacks {
  // ---- 必需方法 / Required Methods ----
  /** 获取当前视口状态 / Get current viewport state */
  getViewport: () => ViewportState;
  /** 获取当前工具 / Get current tool */
  getTool: () => ToolType;
  /** 获取所有点 / Get all points */
  getPoints: () => Point[];
  /** 设置视口缩放比例 / Set viewport scale */
  setScale: (scale: number) => void;
  /** 设置视口偏移量 / Set viewport offset */
  setOffset: (offsetX: number, offsetY: number) => void;
  /** 设置鼠标世界坐标 / Set mouse world coordinates */
  setMouseWorld: (x: number, y: number) => void;
  /** 设置鼠标屏幕坐标 / Set mouse screen coordinates */
  setMouseScreen: (x: number, y: number) => void;
  /** 设置选中的点 / Set selected point */
  setSelectedPoint: (point: Point | null) => void;
  /** 设置悬停的点 / Set hovered point */
  setHoveredPoint: (point: Point | null) => void;
  /** 设置多选的点 / Set multi-selected points */
  setSelectedPoints: (points: Point[]) => void;
  /** 设置拖拽状态 / Set dragging state */
  setIsDragging: (dragging: boolean) => void;
  /** 设置点拖拽状态 / Set point dragging state */
  setIsDraggingPoint: (dragging: boolean) => void;
  /** 设置框选状态 / Set box selection state */
  setIsBoxSelecting: (selecting: boolean) => void;
  /** 设置框选起始位置 / Set box selection start position */
  setBoxSelectStart: (start: { x: number; y: number } | null) => void;
  /** 设置拖拽起始位置 / Set drag start position */
  setDragStart: (start: { x: number; y: number } | null) => void;
  /** 设置正在拖拽的点 / Set the point being dragged */
  setDragPoint: (point: Point | null) => void;
  /** 添加新点 / Add a new point */
  addPoint: (point: Point) => void;
  /** 添加新线段 / Add a new segment */
  addSegment: (segment: Segment) => void;
  /** 设置线段工具的第一个端点 / Set segment first point selection */
  setSegmentFirstPoint: (point: Point | null) => void;
  /** 添加区域顶点 / Add a region point */
  addRegionPoint: (point: Point) => void;
  /** 清空区域顶点 / Clear region points */
  clearRegionPoints: () => void;
  /** 保存撤销状态 / Save undo state */
  saveUndoState: () => void;
  /** 查找屏幕坐标处的点 / Find point at screen position */
  findPointAt: (screenX: number, screenY: number) => Point | null;
  /** 屏幕坐标转世界坐标 / Convert screen to world coordinates */
  screenToWorld: (sx: number, sy: number) => { x: number; y: number };
  /** 触发重新渲染 / Trigger a re-render */
  requestRender: () => void;
  /** 更新状态栏消息 / Update status message */
  setStatusMessage: (message: string) => void;
  /** 显示右键菜单 / Show context menu */
  showContextMenu: (x: number, y: number, targetPoint?: Point, targetSegment?: Segment, worldX?: number, worldY?: number) => void;
  /** 隐藏右键菜单 / Hide context menu */
  hideContextMenu: () => void;
  /** 切换搜索面板可见性 / Toggle search visibility */
  toggleSearch: () => void;
  /** 重置视图到原点 / Reset view to origin */
  resetView: () => void;
  /** 设置当前工具 / Set the active tool */
  setTool: (tool: ToolType) => void;

  // ---- 可选方法 / Optional Methods ----
  /** 获取当前选中的点（可选）/ Get currently selected point (optional) */
  getSelectedPoint?: () => Point | null;
  /** 获取线段工具的首个端点（可选）/ Get segment tool's first point (optional) */
  getSegmentFirstPoint?: () => Point | null;
  /** 获取区域顶点数量（可选）/ Get region vertex count (optional) */
  getRegionPointCount?: () => number;
  /** 更新被拖拽点的位置（可选）/ Update dragged point position (optional) */
  updateDragPointPosition?: (point: Point) => void;
  /** 更新框选矩形尺寸（可选）/ Update box selection rect dimensions (optional) */
  updateBoxSelectRect?: (x1: number, y1: number, x2: number, y2: number) => void;
  /** 撤销操作（可选）/ Undo (optional) */
  undo?: () => void;
  /** 重做操作（可选）/ Redo (optional) */
  redo?: () => void;
  /** 删除选中的点（可选）/ Delete selected point (optional) */
  removeSelectedPoint?: (id: number) => void;
  /** 查找屏幕坐标处的线段（可选）/ Find segment at screen position (optional) */
  findSegmentAt?: (screenX: number, screenY: number) => Segment | null;
}

// ================================================================
// Interaction Manager Class / 交互管理器类
// ================================================================

/**
 * Manages all user interaction with the geometry canvas.
 *
 * Handles:
 * - Mouse events (down, move, up, leave, wheel, contextmenu)
 * - Touch events (start, move, end) for mobile support
 * - Tool-specific behaviors (select, point, segment, pan, region, probe)
 *
 * Note: Keyboard shortcuts are now handled by hooks/useKeyboard.ts
 * to avoid double-registration of keydown listeners.
 *
 * @example
 * ```typescript
 * const interaction = new InteractionManager(canvas, renderer, callbacks);
 * interaction.attach();  // Start listening to events
 * interaction.detach();  // Stop listening (cleanup)
 * ```
 */
export class InteractionManager {
  private canvas: HTMLCanvasElement;
  private callbacks: InteractionCallbacks;
  private boundHandlers: {
    mouseDown: (e: MouseEvent) => void;
    mouseMove: (e: MouseEvent) => void;
    mouseUp: (e: MouseEvent) => void;
    mouseLeave: (e: MouseEvent) => void;
    wheel: (e: WheelEvent) => void;
    contextMenu: (e: MouseEvent) => void;
    touchStart: (e: TouchEvent) => void;
    touchMove: (e: TouchEvent) => void;
    touchEnd: (e: TouchEvent) => void;
    // 键盘事件已移除：统一由 hooks/useKeyboard.ts 管理，避免双重注册
  };

  /** 内部拖拽状态 / Internal drag state */
  private isDragging = false;
  /** 拖拽起始时的屏幕坐标 / Screen coordinates at drag start */
  private dragStartScreen: { x: number; y: number } | null = null;
  /** 拖拽起始时的视口偏移量 / Viewport offset at drag start */
  private dragStartOffset: { x: number; y: number } | null = null;

  /** 内部点拖拽状态 / Internal point drag state */
  private isDraggingPoint = false;
  /** 当前正在拖拽的点 / The point currently being dragged */
  private dragPoint: Point | null = null;

  /** 内部框选状态 / Internal box selection state */
  private isBoxSelecting = false;
  /** 框选起始时的屏幕坐标 / Screen coordinates at box selection start */
  private boxSelectStartScreen: { x: number; y: number } | null = null;

  /** 圆规工具状态 / Compass tool state */
  private compassCenter: Point | null = null;

  /** 鼠标移动节流追踪 / Mouse move throttle tracking */
  private lastMouseMoveTime = 0;
  /** 鼠标移动节流间隔（毫秒）/ Mouse move throttle interval (ms) */
  private mouseMoveThrottleMs = MOUSE_MOVE_THROTTLE_MS;

  constructor(canvas: HTMLCanvasElement, callbacks: InteractionCallbacks) {
    this.canvas = canvas;
    this.callbacks = callbacks;

    // Bind all handlers once for proper add/removeEventListener
    this.boundHandlers = {
      mouseDown: this.onMouseDown.bind(this),
      mouseMove: this.onMouseMove.bind(this),
      mouseUp: this.onMouseUp.bind(this),
      mouseLeave: this.onMouseLeave.bind(this),
      wheel: this.onWheel.bind(this),
      contextMenu: this.onContextMenu.bind(this),
      touchStart: this.onTouchStart.bind(this),
      touchMove: this.onTouchMove.bind(this),
      touchEnd: this.onTouchEnd.bind(this),
      // 键盘事件已移除：统一由 hooks/useKeyboard.ts 管理
    };
  }

  // ================================================================
  // Attach / Detach / 附加与分离
  // ================================================================

  /**
   * Attach all event listeners to the canvas and document.
   * Call this once during component initialization.
   */
  attach(): void {
    const c = this.canvas;
    c.addEventListener('mousedown', this.boundHandlers.mouseDown);
    c.addEventListener('mousemove', this.boundHandlers.mouseMove);
    c.addEventListener('mouseup', this.boundHandlers.mouseUp);
    c.addEventListener('mouseleave', this.boundHandlers.mouseLeave);
    c.addEventListener('wheel', this.boundHandlers.wheel, { passive: false });
    c.addEventListener('contextmenu', this.boundHandlers.contextMenu);
    c.addEventListener('touchstart', this.boundHandlers.touchStart, { passive: false });
    c.addEventListener('touchmove', this.boundHandlers.touchMove, { passive: false });
    c.addEventListener('touchend', this.boundHandlers.touchEnd);
    // 键盘事件已移除：不在此注册 keydown/keyup。
    // 键盘快捷键统一由 hooks/useKeyboard.ts 管理。
  }

  /**
   * Remove all event listeners. Call this on cleanup to prevent memory leaks.
   */
  detach(): void {
    const c = this.canvas;
    c.removeEventListener('mousedown', this.boundHandlers.mouseDown);
    c.removeEventListener('mousemove', this.boundHandlers.mouseMove);
    c.removeEventListener('mouseup', this.boundHandlers.mouseUp);
    c.removeEventListener('mouseleave', this.boundHandlers.mouseLeave);
    c.removeEventListener('wheel', this.boundHandlers.wheel);
    c.removeEventListener('contextmenu', this.boundHandlers.contextMenu);
    c.removeEventListener('touchstart', this.boundHandlers.touchStart);
    c.removeEventListener('touchmove', this.boundHandlers.touchMove);
    c.removeEventListener('touchend', this.boundHandlers.touchEnd);
    // 键盘事件已移除：不再需要 removeEventListener keydown/keyup。
  }

  // ================================================================
  // Mouse Events / 鼠标事件
  // ================================================================

  /**
   * 鼠标按下事件处理
   * 根据当前工具类型分发到不同的交互逻辑：
   * - point: 在点击位置添加新点
   * - segment: 选择两个点连线
   * - compass: 圆规工具（两次点击画圆）
   * - pan: 开始平移画布
   * - select: 开始拖拽点或框选
   * - region: 添加区域顶点
   */
  private onMouseDown(e: MouseEvent): void {
    e.preventDefault();
    const rect = this.canvas.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;
    const worldPos = this.callbacks.screenToWorld(x, y);
    const tool = this.callbacks.getTool();

    this.callbacks.setMouseWorld(worldPos.x, worldPos.y);
    this.callbacks.setMouseScreen(x, y);

    if (tool === 'point') {
      // Add a new point at click position
      this.callbacks.saveUndoState();
      const newId = generateUniqueId(); // Temporary ID until backend assigns one
      this.callbacks.addPoint({ id: newId, x: worldPos.x, y: worldPos.y });
      this.callbacks.setStatusMessage(
        `ADD POINT (${worldPos.x.toFixed(1)}, ${worldPos.y.toFixed(1)}) / 添加点`,
      );
    } else if (tool === 'segment') {
      // Select two points to connect
      const hit = this.callbacks.findPointAt(x, y);
      if (hit) {
        // We use a simpler approach: check if segmentFirstPoint is set via state
        if (this.callbacks.getSegmentFirstPoint?.()) {
          this.callbacks.saveUndoState();
          const firstPoint = this.callbacks.getSegmentFirstPoint();
          if (!firstPoint) return;
          const firstPointId = firstPoint.id;
          const segId = generateUniqueId();
          this.callbacks.addSegment({
            p1: firstPointId,
            p2: hit.id,
            id: segId,
          });
          this.callbacks.setSegmentFirstPoint(null);
          this.callbacks.setStatusMessage(
            `SEGMENT n${firstPointId} -> n${hit.id} / 添加线段`,
          );
        } else {
          this.callbacks.setSegmentFirstPoint(hit);
          this.callbacks.setStatusMessage('SELECT SECOND POINT / 选择第二个点');
        }
      }
    } else if (tool === 'compass') {
      // 圆规工具：第一次点击设置圆心，第二次点击设置半径
      const hit = this.callbacks.findPointAt(x, y);
      if (!this.compassCenter) {
        // 第一次点击：设置圆心（选择已有点或创建新点）
        const center = hit ?? { id: generateUniqueId(), x: worldPos.x, y: worldPos.y };
        if (!hit) {
          this.callbacks.saveUndoState();
          this.callbacks.addPoint(center);
        }
        this.compassCenter = center;
        this.callbacks.setStatusMessage(
          `COMPASS CENTER n${center.id} (${center.x.toFixed(1)}, ${center.y.toFixed(1)}) / 设置半径`,
        );
      } else {
        // 第二次点击：计算半径，生成 24 边形近似圆
        const radius = Math.sqrt(
          (worldPos.x - this.compassCenter.x) ** 2 +
          (worldPos.y - this.compassCenter.y) ** 2,
        );
        if (radius < 1) {
          this.callbacks.setStatusMessage('RADIUS TOO SMALL / 半径太小');
          this.compassCenter = null;
          this.callbacks.requestRender();
          return;
        }

        this.callbacks.saveUndoState();
        const SIDES = CIRCLE_APPROX_SIDES;
        const circlePoints: Point[] = [];

        // 生成圆上的顶点
        for (let i = 0; i < SIDES; i++) {
          const angle = (2 * Math.PI * i) / SIDES;
          const px = this.compassCenter.x + radius * Math.cos(angle);
          const py = this.compassCenter.y + radius * Math.sin(angle);
          const pId = generateUniqueId();
          const p: Point = { id: pId, x: px, y: py };
          circlePoints.push(p);
          this.callbacks.addPoint(p);
        }

        // 生成边形成闭合多边形
        for (let i = 0; i < SIDES; i++) {
          const nextIdx = (i + 1) % SIDES;
          this.callbacks.addSegment({
            p1: circlePoints[i]!.id,
            p2: circlePoints[nextIdx]!.id,
            id: generateUniqueId(),
          });
        }

        this.callbacks.setStatusMessage(
          `CIRCLE n${this.compassCenter.id} r=${radius.toFixed(1)} / 圆规完成`,
        );
        this.compassCenter = null;
      }
    } else if (tool === 'pan') {
      // Start panning
      this.isDragging = true;
      this.dragStartScreen = { x, y };
      const vp = this.callbacks.getViewport();
      this.dragStartOffset = { x: vp.offsetX, y: vp.offsetY };
      this.callbacks.setIsDragging(true);
      this.canvas.style.cursor = 'grabbing';
    } else if (tool === 'select') {
      if (e.button === 0) {
        const hit = this.callbacks.findPointAt(x, y);
        if (hit) {
          // Start dragging a point
          this.isDraggingPoint = true;
          this.dragPoint = hit;
          this.callbacks.setIsDraggingPoint(true);
          this.callbacks.setSelectedPoint(hit);
          this.canvas.style.cursor = 'grabbing';
        } else {
          // Start box selection
          this.isBoxSelecting = true;
          this.boxSelectStartScreen = { x: e.clientX, y: e.clientY };
          this.callbacks.setIsBoxSelecting(true);
          this.callbacks.setBoxSelectStart({ x: e.clientX, y: e.clientY });
          this.callbacks.setSelectedPoint(null);
          this.callbacks.setSelectedPoints([]);
        }
      }
    } else if (tool === 'region') {
      const hit = this.callbacks.findPointAt(x, y);
      if (hit) {
        this.callbacks.addRegionPoint(hit);
        this.callbacks.setStatusMessage(
          `REGION: ${this.callbacks.getRegionPointCount?.() ?? 0} vertices / 区域顶点`,
        );
      }
    }

    this.callbacks.requestRender();
  }

  /**
   * 鼠标移动事件处理（带节流）
   * 更新鼠标坐标、处理拖拽平移/点拖拽/框选、更新悬停点和光标样式
   */
  private onMouseMove(e: MouseEvent): void {
    const now = performance.now();
    if (now - this.lastMouseMoveTime < this.mouseMoveThrottleMs) return;
    this.lastMouseMoveTime = now;

    const rect = this.canvas.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;
    const worldPos = this.callbacks.screenToWorld(x, y);

    this.callbacks.setMouseWorld(worldPos.x, worldPos.y);
    this.callbacks.setMouseScreen(x, y);

    const tool = this.callbacks.getTool();

    // Handle panning
    if (this.isDragging && this.dragStartScreen && this.dragStartOffset) {
      const dx = (x - this.dragStartScreen.x) / this.callbacks.getViewport().scale;
      const dy = (y - this.dragStartScreen.y) / this.callbacks.getViewport().scale;
      this.callbacks.setOffset(
        this.dragStartOffset.x + dx,
        this.dragStartOffset.y + dy,
      );
    }

    // Handle point dragging
    if (this.isDraggingPoint && this.dragPoint) {
      // Update the dragged point's position
      this.dragPoint = { ...this.dragPoint, x: worldPos.x, y: worldPos.y };
      this.callbacks.updateDragPointPosition?.(this.dragPoint);
    }

    // Handle box selection
    if (this.isBoxSelecting && this.boxSelectStartScreen) {
      // Update selection rect dimensions (handled by React component)
      this.callbacks.updateBoxSelectRect?.(
        this.boxSelectStartScreen.x,
        this.boxSelectStartScreen.y,
        e.clientX,
        e.clientY,
      );
    }

    // Update hovered point
    if (tool === 'select' || tool === 'probe') {
      const hit = this.callbacks.findPointAt(x, y);
      this.callbacks.setHoveredPoint(hit);
    }

    // Update cursor based on tool
    this.updateCursor(tool);

    this.callbacks.requestRender();
  }

  /**
   * 鼠标释放事件处理
   * 结束所有拖拽/框选状态，恢复光标样式
   */
  private onMouseUp(_e: MouseEvent): void {
    // End panning
    if (this.isDragging) {
      this.isDragging = false;
      this.dragStartScreen = null;
      this.dragStartOffset = null;
      this.callbacks.setIsDragging(false);
    }

    // End point dragging
    if (this.isDraggingPoint) {
      this.isDraggingPoint = false;
      this.dragPoint = null;
      this.callbacks.setIsDraggingPoint(false);
    }

    // End box selection
    if (this.isBoxSelecting) {
      this.isBoxSelecting = false;
      this.boxSelectStartScreen = null;
      this.callbacks.setIsBoxSelecting(false);
      this.callbacks.setBoxSelectStart(null);
    }

    this.updateCursor(this.callbacks.getTool());
  }

  /**
   * 鼠标离开画布事件处理
   * 清除悬停点状态并触发重绘
   */
  private onMouseLeave(_e: MouseEvent): void {
    this.callbacks.setHoveredPoint(null);
    this.callbacks.requestRender();
  }

  // ================================================================
  // Wheel Zoom / 滚轮缩放
  // ================================================================

  /**
   * 滚轮缩放事件处理
   * 向上滚动放大，向下滚动缩小，缩放范围受 SCALE_MIN/SCALE_MAX 限制
   */
  private onWheel(e: WheelEvent): void {
    e.preventDefault();
    const vp = this.callbacks.getViewport();
    const factor = e.deltaY < 0 ? ZOOM_SMOOTH : 1 / ZOOM_SMOOTH;
    const newScale = Math.max(SCALE_MIN, Math.min(SCALE_MAX, vp.scale * factor));
    this.callbacks.setScale(newScale);
    this.callbacks.requestRender();
  }

  // ================================================================
  // Context Menu / 右键菜单
  // ================================================================

  /**
   * 右键菜单事件处理
   * 检测点击位置的点和线段，显示对应的上下文菜单
   */
  private onContextMenu(e: MouseEvent): void {
    e.preventDefault();
    const rect = this.canvas.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;
    const worldPos = this.callbacks.screenToWorld(x, y);
    const hitPoint = this.callbacks.findPointAt(x, y);
    const hitSegment = this.callbacks.findSegmentAt?.(x, y);
    this.callbacks.showContextMenu(
      e.clientX,
      e.clientY,
      hitPoint ?? undefined,
      hitSegment ?? undefined,
      worldPos.x,
      worldPos.y,
    );
  }

  // ================================================================
  // Touch Events / 触摸事件
  // ================================================================

  /**
   * 触摸开始事件处理
   * 单指触摸模拟鼠标按下事件
   */
  private onTouchStart(e: TouchEvent): void {
    e.preventDefault();
    if (e.touches.length === 1) {
      const touch = e.touches[0]!;

      // Simulate mouse down
      const mouseEvent = new MouseEvent('mousedown', {
        clientX: touch.clientX,
        clientY: touch.clientY,
        button: 0,
      });
      this.onMouseDown(mouseEvent);
    }
  }

  /**
   * 触摸移动事件处理
   * 单指触摸模拟鼠标移动事件
   */
  private onTouchMove(e: TouchEvent): void {
    e.preventDefault();
    if (e.touches.length === 1) {
      const touch = e.touches[0]!;
      const mouseEvent = new MouseEvent('mousemove', {
        clientX: touch.clientX,
        clientY: touch.clientY,
      });
      this.onMouseMove(mouseEvent);
    }
  }

  /**
   * 触摸结束事件处理
   * 模拟鼠标释放事件
   */
  private onTouchEnd(_e: TouchEvent): void {
    const mouseEvent = new MouseEvent('mouseup', { button: 0 });
    this.onMouseUp(mouseEvent);
  }

  // ================================================================
  // Keyboard Events (REMOVED) / 键盘事件（已移除）
  // ================================================================
  //
  // 键盘快捷键处理已从此处移除，统一由 hooks/useKeyboard.ts 管理。
  // 原因：InteractionManager 和 useKeyboard hook 各自在 document 上
  // 注册 keydown 监听器，导致每一次按键被处理两次（双重注册 bug）。
  // 同时两处代码对 Ctrl 修饰符的判断逻辑不一致（此处要求 Ctrl 切换工具，
  // useKeyboard 要求不按 Ctrl），导致快捷键行为不可预测。
  //
  // 原 onKeyDown() 和 onKeyUp() 方法已删除以消除双重注册。
  // 如需恢复键盘处理，请参考 git 历史或 hooks/useKeyboard.ts。

  // ================================================================
  // Cursor Management / 光标管理
  // ================================================================

  /**
   * 根据当前工具更新画布光标样式
   * 拖拽中显示 'grabbing'，否则根据工具类型显示对应光标
   */
  private updateCursor(tool: ToolType): void {
    if (this.isDragging || this.isDraggingPoint) {
      this.canvas.style.cursor = 'grabbing';
      return;
    }

    const cursors: Record<ToolType, string> = {
      select: 'default',
      point: 'crosshair',
      segment: 'crosshair',
      compass: 'crosshair',
      pan: 'grab',
      region: 'crosshair',
      probe: 'help',
    };

    this.canvas.style.cursor = cursors[tool] ?? 'default';
  }

  // ================================================================
  // Extended Callbacks (optional) / 扩展回调（可选）
  // ================================================================

  // These optional methods are now declared directly in the InteractionCallbacks interface above.
}
