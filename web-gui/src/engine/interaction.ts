/**
 * @module engine/interaction
 * @description Framework-agnostic interaction manager for the geometry canvas.
 *              Handles mouse, touch, and wheel events for tools including
 *              select, point, segment, pan, region, and probe.
 *              Keyboard shortcuts are managed by hooks/useKeyboard.ts.
 *              This class has NO React or framework dependencies.
 */

import type { ViewportState } from '@/engine/renderer';
import type { Point, Segment, ToolType } from '@/types';
import { ZOOM_SMOOTH, SCALE_MIN, SCALE_MAX } from '@/utils/constants';
import { generateId } from '@/utils/idGenerator';
import { CIRCLE_APPROX_SIDES, MOUSE_MOVE_THROTTLE_MS } from '@/utils/constants';

// ================================================================
// Callback Interfaces / 回调接口
// ================================================================

/**
 * Callbacks provided by the host application for interaction events.
 * The interaction manager calls these to update application state.
 */
export interface InteractionCallbacks {
  /** Get current viewport state */
  getViewport: () => ViewportState;
  /** Get current tool */
  getTool: () => ToolType;
  /** Get all points */
  getPoints: () => Point[];
  /** Set viewport scale */
  setScale: (scale: number) => void;
  /** Set viewport offset */
  setOffset: (offsetX: number, offsetY: number) => void;
  /** Set mouse world coordinates */
  setMouseWorld: (x: number, y: number) => void;
  /** Set mouse screen coordinates */
  setMouseScreen: (x: number, y: number) => void;
  /** Set selected point */
  setSelectedPoint: (point: Point | null) => void;
  /** Set hovered point */
  setHoveredPoint: (point: Point | null) => void;
  /** Set multi-selected points */
  setSelectedPoints: (points: Point[]) => void;
  /** Set dragging state */
  setIsDragging: (dragging: boolean) => void;
  /** Set point dragging state */
  setIsDraggingPoint: (dragging: boolean) => void;
  /** Set box selection state */
  setIsBoxSelecting: (selecting: boolean) => void;
  /** Set box selection start position */
  setBoxSelectStart: (start: { x: number; y: number } | null) => void;
  /** Set drag start position */
  setDragStart: (start: { x: number; y: number } | null) => void;
  /** Set the point being dragged */
  setDragPoint: (point: Point | null) => void;
  /** Add a new point */
  addPoint: (point: Point) => void;
  /** Add a new segment */
  addSegment: (segment: Segment) => void;
  /** Set segment first point selection */
  setSegmentFirstPoint: (point: Point | null) => void;
  /** Add a region point */
  addRegionPoint: (point: Point) => void;
  /** Clear region points */
  clearRegionPoints: () => void;
  /** Save undo state */
  saveUndoState: () => void;
  /** Find point at screen position */
  findPointAt: (screenX: number, screenY: number) => Point | null;
  /** Convert screen to world coordinates */
  screenToWorld: (sx: number, sy: number) => { x: number; y: number };
  /** Trigger a re-render */
  requestRender: () => void;
  /** Update status message */
  setStatusMessage: (message: string) => void;
  /** Show context menu */
  showContextMenu: (x: number, y: number, targetPoint?: Point, targetSegment?: Segment, worldX?: number, worldY?: number) => void;
  /** Hide context menu */
  hideContextMenu: () => void;
  /** Toggle search visibility */
  toggleSearch: () => void;
  /** Reset view to origin */
  resetView: () => void;
  /** Set the active tool */
  setTool: (tool: ToolType) => void;
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

  /** Internal drag state */
  private isDragging = false;
  private dragStartScreen: { x: number; y: number } | null = null;
  private dragStartOffset: { x: number; y: number } | null = null;

  /** Internal point drag state */
  private isDraggingPoint = false;
  private dragPoint: Point | null = null;

  /** Internal box selection state */
  private isBoxSelecting = false;
  private boxSelectStartScreen: { x: number; y: number } | null = null;

  /** 圆规工具状态 / Compass tool state */
  private compassCenter: Point | null = null;

  /** Throttle tracking */
  private lastMouseMoveTime = 0;
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
      const newId = generateId(); // Temporary ID until backend assigns one
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
          const firstPointId = this.callbacks.getSegmentFirstPoint()!.id;
          const segId = generateId();
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
        const center = hit ?? { id: generateId(), x: worldPos.x, y: worldPos.y };
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
          const pId = generateId();
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
            id: generateId(),
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

  private onMouseLeave(_e: MouseEvent): void {
    this.callbacks.setHoveredPoint(null);
    this.callbacks.requestRender();
  }

  // ================================================================
  // Wheel Zoom / 滚轮缩放
  // ================================================================

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
   * Update the canvas cursor based on the current tool
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

  // These are accessed via the callbacks object with optional chaining
  // in the handler methods above. They allow the host to provide
  // additional functionality without changing the interface.
}

// Extend the callbacks interface with optional methods
export interface InteractionCallbacks {
  /** 获取当前选中的点（可选） */
  getSelectedPoint?: () => Point | null;
  /** 获取线段工具的首个端点（可选） */
  getSegmentFirstPoint?: () => Point | null;
  /** 获取区域顶点数量（可选） */
  getRegionPointCount?: () => number;
  /** 更新被拖拽点的位置（可选） */
  updateDragPointPosition?: (point: Point) => void;
  /** 更新框选矩形尺寸（可选） */
  updateBoxSelectRect?: (x1: number, y1: number, x2: number, y2: number) => void;
  /** 撤销操作（可选） */
  undo?: () => void;
  /** 重做操作（可选） */
  redo?: () => void;
  /** 删除选中的点（可选） */
  removeSelectedPoint?: (id: number) => void;
  /** 查找屏幕坐标处的线段（可选） */
  findSegmentAt?: (screenX: number, screenY: number) => Segment | null;
}
