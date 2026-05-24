/**
 * @module engine/renderer
 * @description Framework-agnostic Canvas 2D rendering engine.
 *              Handles all drawing operations for the geometry canvas including
 *              grid, axes, points, segments, regions, labels, and HUD elements.
 *              This class has NO React or framework dependencies.
 *              与框架无关的 Canvas 2D 渲染引擎。处理几何画布的所有绘制操作，
 *              包括网格、坐标轴、点、线段、区域、标签和 HUD 元素。
 *              本类无 React 或框架依赖。
 */

import type { Point, Segment, Region, Port, FuncBlock, Theme, ThemeColors, Constraint } from '@/types';
import { getThemeColors } from '@/stores/canvasStore';
import {
  BASE_GRID_SIZE,
  POINT_RADIUS,
  POINT_RADIUS_ACTIVE,
  POINT_OUTER_OFFSET,
  SEGMENT_LINE_WIDTH,
  AXIS_LINE_WIDTH,
} from '@/utils/constants';

// ================================================================
// Rendering Constants / 渲染常量（从 constants.ts 统一导入）
// ================================================================

// ================================================================
// Theme Color Schemes / 主题颜色方案
// 主题颜色已统一到 canvasStore.ts，通过 getThemeColors() 获取
// ================================================================

// ================================================================
// Renderer Configuration / 渲染器配置
// ================================================================

/**
 * Configuration options for the renderer
 * @property showGrid - Whether to render the background grid
 * @property showAxes - Whether to render coordinate axes
 * @property showLabels - Whether to render point labels
 */
export interface RendererConfig {
  showGrid: boolean;
  showAxes: boolean;
  showLabels: boolean;
}

/**
 * Canvas viewport state needed for rendering
 * @property scale - Zoom scale factor
 * @property offsetX - Horizontal pan offset in world coordinates
 * @property offsetY - Vertical pan offset in world coordinates
 * @property dpr - Device pixel ratio for high-DPI rendering
 * @property width - Canvas CSS width in pixels
 * @property height - Canvas CSS height in pixels
 */
export interface ViewportState {
  scale: number;
  offsetX: number;
  offsetY: number;
  dpr: number;
  width: number;
  height: number;
}

// ================================================================
// Renderer Class / 渲染器类
// ================================================================

/**
 * Canvas 2D rendering engine for the Lv-00 geometry application.
 *
 * This class is completely framework-agnostic and can be used with
 * any UI framework or vanilla JavaScript. It handles:
 * - Background clearing
 * - Grid drawing with adaptive spacing
 * - Coordinate axis drawing
 * - Point rendering (normal, selected, hovered states)
 * - Segment rendering
 * - Region rendering (filled polygons)
 * - Point labels
 * - HUD overlay (coordinates, zoom level)
 *
 * @example
 * ```typescript
 * const renderer = new Renderer(canvas, ctx);
 * renderer.render(viewport, config, points, segments, regions, ...);
 * ```
 */
export class Renderer {
  private canvas: HTMLCanvasElement;
  private ctx: CanvasRenderingContext2D;

  constructor(canvas: HTMLCanvasElement, ctx: CanvasRenderingContext2D) {
    this.canvas = canvas;
    this.ctx = ctx;
  }

  // ================================================================
  // Coordinate Transforms / 坐标转换
  // ================================================================

  /**
   * Convert world coordinates to screen (CSS pixel) coordinates
   * @param wx - World X coordinate
   * @param wy - World Y coordinate
   * @param vp - Current viewport state
   * @returns Screen coordinates { x, y }
   */
  worldToScreen(wx: number, wy: number, vp: ViewportState): { x: number; y: number } {
    return {
      x: (wx + vp.offsetX) * vp.scale + vp.width / 2,
      y: (wy + vp.offsetY) * vp.scale + vp.height / 2,
    };
  }

  /**
   * Convert screen (CSS pixel) coordinates to world coordinates
   * @param sx - Screen X coordinate
   * @param sy - Screen Y coordinate
   * @param vp - Current viewport state
   * @returns World coordinates { x, y }
   */
  screenToWorld(sx: number, sy: number, vp: ViewportState): { x: number; y: number } {
    return {
      x: (sx - vp.width / 2) / vp.scale - vp.offsetX,
      y: (sy - vp.height / 2) / vp.scale - vp.offsetY,
    };
  }

  // ================================================================
  // Theme Colors / 主题颜色
  // ================================================================

  /**
   * Get the color scheme for the current theme
   * @param theme - Current theme mode
   * @returns ThemeColors object with all rendering colors
   */
  getThemeColors(theme: Theme): ThemeColors {
    return getThemeColors(theme);
  }

  // ================================================================
  // Main Render Method / 主渲染方法
  // ================================================================

  /**
   * Execute the full render pipeline.
   *
   * Render order (back to front):
   * 1. Background
   * 2. Grid
   * 3. Coordinate axes
   * 4. Regions (filled polygons)
   * 5. Segments
   * 6. Points (normal, selected, hovered)
   * 7. Labels
   *
   * @param vp - Current viewport state
   * @param config - Renderer display configuration
   * @param theme - Current theme
   * @param points - Array of points to render
   * @param segments - Array of segments to render
   * @param regions - Array of regions to render
   * @param ports - Array of ports to render
   * @param funcBlocks - Array of function blocks to render
   * @param constraints - Array of constraints to visualize
   * @param selectedPoint - Currently selected point (highlighted)
   * @param hoveredPoint - Currently hovered point (highlighted)
   */
  render(
    vp: ViewportState,
    config: RendererConfig,
    theme: Theme,
    points: Point[],
    segments: Segment[],
    regions: Region[],
    ports: Port[],
    funcBlocks: FuncBlock[],
    constraints: Constraint[],
    selectedPoint: Point | null,
    hoveredPoint: Point | null,
  ): void {
    const ctx = this.ctx;
    const colors = this.getThemeColors(theme);

    // Build lookup maps for O(n+m) instead of O(n*m) per-frame lookups
    // 构建查找映射，将每帧 O(n*m) 降为 O(n+m)
    const pointMap = new Map<number, Point>(points.map((p) => [p.id, p]));
    const segmentMap = new Map<number, Segment>(segments.map((s) => [s.id, s]));

    // 1. Clear background / 清除背景
    ctx.fillStyle = colors.canvasBg;
    ctx.fillRect(0, 0, vp.width, vp.height);

    // 2. Grid / 网格
    if (config.showGrid) {
      this.drawGrid(vp, colors);
    }

    // 3. Axes / 坐标轴
    if (config.showAxes) {
      this.drawAxes(vp, colors);
    }

    // 4. Regions / 区域（填充多边形）
    for (const region of regions) {
      this.drawRegion(region, vp, colors);
    }

    // 5. FuncBlocks / 函数块（在连线下方渲染，作为背景层）
    for (const fb of funcBlocks) {
      const fbPorts = ports.filter((p) => p.parentBlockId === fb.id);
      this.drawFuncBlock(fb, fbPorts, vp, colors);
    }

    // 6. Segments / 线段
    for (const segment of segments) {
      const p1 = pointMap.get(segment.p1);
      const p2 = pointMap.get(segment.p2);
      if (p1 && p2) {
        this.drawSegment(p1, p2, vp, colors);
      }
    }

    // 6.5 Constraints / 约束可视化（在线段之后、点之前）
    this.drawConstraints(constraints, pointMap, segmentMap, vp, colors);

    // 7. Points / 点
    for (const point of points) {
      const isSelected = selectedPoint?.id === point.id;
      const isHovered = hoveredPoint?.id === point.id;
      this.drawPoint(point, vp, colors, isSelected, isHovered);
    }

    // 8. Ports / 端口（在点上方渲染，确保可见性）
    for (const port of ports) {
      this.drawPort(port, vp, colors);
    }

    // 9. Labels / 标签
    if (config.showLabels) {
      for (const point of points) {
        this.drawLabel(point, vp, colors);
      }
    }

    // Performance tracking (caller measures render time externally)
    return;
  }

  // ================================================================
  // Grid Drawing / 网格绘制
  // ================================================================

  /**
   * Draw adaptive grid lines based on zoom level.
   * Grid spacing adjusts automatically as the user zooms in/out
   * to maintain readable density.
   */
  private drawGrid(vp: ViewportState, colors: ThemeColors): void {
    const ctx = this.ctx;
    let gridSize = BASE_GRID_SIZE;

    // Adaptive grid spacing based on zoom level
    if (vp.scale < 0.1) gridSize = BASE_GRID_SIZE * 10;
    else if (vp.scale < 0.5) gridSize = BASE_GRID_SIZE * 5;
    else if (vp.scale < 1) gridSize = BASE_GRID_SIZE * 2;
    else if (vp.scale >= 10) gridSize = BASE_GRID_SIZE / 10;
    else if (vp.scale >= 5) gridSize = BASE_GRID_SIZE / 5;
    else if (vp.scale >= 2) gridSize = BASE_GRID_SIZE / 2;

    const scaledGrid = gridSize * vp.scale;

    // Calculate visible range in world coordinates
    const worldLeft = -vp.offsetX - vp.width / (2 * vp.scale);
    const worldTop = -vp.offsetY - vp.height / (2 * vp.scale);

    // Calculate grid line start/end in screen coordinates
    const startX = (Math.floor(worldLeft / gridSize) * gridSize + vp.offsetX) * vp.scale + vp.width / 2;
    const startY = (Math.floor(worldTop / gridSize) * gridSize + vp.offsetY) * vp.scale + vp.height / 2;

    ctx.strokeStyle = colors.grid;
    ctx.lineWidth = 1;
    ctx.beginPath();

    // Vertical lines
    for (let x = startX; x < vp.width; x += scaledGrid) {
      ctx.moveTo(Math.round(x) + 0.5, 0);
      ctx.lineTo(Math.round(x) + 0.5, vp.height);
    }

    // Horizontal lines
    for (let y = startY; y < vp.height; y += scaledGrid) {
      ctx.moveTo(0, Math.round(y) + 0.5);
      ctx.lineTo(vp.width, Math.round(y) + 0.5);
    }

    ctx.stroke();
  }

  // ================================================================
  // Axes Drawing / 坐标轴绘制
  // ================================================================

  /**
   * Draw X and Y coordinate axes through the origin.
   */
  private drawAxes(vp: ViewportState, colors: ThemeColors): void {
    const ctx = this.ctx;
    const origin = this.worldToScreen(0, 0, vp);

    ctx.strokeStyle = colors.axis;
    ctx.lineWidth = AXIS_LINE_WIDTH;

    // X axis
    ctx.beginPath();
    ctx.moveTo(0, Math.round(origin.y) + 0.5);
    ctx.lineTo(vp.width, Math.round(origin.y) + 0.5);
    ctx.stroke();

    // Y axis
    ctx.beginPath();
    ctx.moveTo(Math.round(origin.x) + 0.5, 0);
    ctx.lineTo(Math.round(origin.x) + 0.5, vp.height);
    ctx.stroke();

    // Origin label
    ctx.fillStyle = colors.text;
    ctx.font = '10px monospace';
    ctx.fillText('O', origin.x + 4, origin.y + 12);
  }

  // ================================================================
  // Point Drawing / 点绘制
  // ================================================================

  /**
   * Draw a single point with optional selection/hover highlighting.
   */
  private drawPoint(
    point: Point,
    vp: ViewportState,
    colors: ThemeColors,
    isSelected: boolean,
    isHovered: boolean,
  ): void {
    const ctx = this.ctx;
    const screen = this.worldToScreen(point.x, point.y, vp);
    const radius = isSelected || isHovered ? POINT_RADIUS_ACTIVE : POINT_RADIUS;

    // Outer ring for selected points
    if (isSelected) {
      ctx.beginPath();
      ctx.arc(screen.x, screen.y, radius + POINT_OUTER_OFFSET, 0, Math.PI * 2);
      ctx.strokeStyle = colors.pointSelected;
      ctx.lineWidth = 1.5;
      ctx.stroke();
    }

    // Point fill
    ctx.beginPath();
    ctx.arc(screen.x, screen.y, radius, 0, Math.PI * 2);
    ctx.fillStyle = isSelected
      ? colors.pointSelected
      : isHovered
        ? colors.pointHover
        : colors.point;
    ctx.fill();
  }

  // ================================================================
  // Segment Drawing / 线段绘制
  // ================================================================

  /**
   * Draw a line segment between two points.
   */
  private drawSegment(p1: Point, p2: Point, vp: ViewportState, colors: ThemeColors): void {
    const ctx = this.ctx;
    const s1 = this.worldToScreen(p1.x, p1.y, vp);
    const s2 = this.worldToScreen(p2.x, p2.y, vp);

    ctx.beginPath();
    ctx.moveTo(s1.x, s1.y);
    ctx.lineTo(s2.x, s2.y);
    ctx.strokeStyle = colors.segment;
    ctx.lineWidth = SEGMENT_LINE_WIDTH;
    ctx.stroke();
  }

  // ================================================================
  // Region Drawing / 区域绘制
  // ================================================================

  /**
   * Draw a filled polygon region with semi-transparent fill.
   */
  private drawRegion(region: Region, vp: ViewportState, colors: ThemeColors): void {
    if (region.points.length < 3) return;

    const ctx = this.ctx;
    ctx.beginPath();

    const first = this.worldToScreen(region.points[0]!.x, region.points[0]!.y, vp);
    ctx.moveTo(first.x, first.y);

    for (let i = 1; i < region.points.length; i++) {
      const p = this.worldToScreen(region.points[i]!.x, region.points[i]!.y, vp);
      ctx.lineTo(p.x, p.y);
    }

    ctx.closePath();

    // Semi-transparent fill
    const isLight = colors.canvasBg === '#ffffff';
    ctx.fillStyle = isLight
      ? 'rgba(5, 80, 174, 0.08)'
      : 'rgba(100, 180, 255, 0.1)';
    ctx.fill();

    // Border
    ctx.strokeStyle = isLight
      ? 'rgba(5, 80, 174, 0.3)'
      : 'rgba(100, 180, 255, 0.4)';
    ctx.lineWidth = 1;
    ctx.stroke();
  }

  // ================================================================
  // Port Drawing / 端口绘制
  // ================================================================

  private drawPort(port: Port, vp: ViewportState, colors: ThemeColors): void {
    const ctx = this.ctx;
    const screen = this.worldToScreen(port.x, port.y, vp);
    const radius = 5;
    ctx.beginPath();
    ctx.arc(screen.x, screen.y, radius, 0, Math.PI * 2);
    const isInput = port.direction === 'input';
    ctx.fillStyle = isInput ? colors.port : colors.portHover;
    ctx.fill();
    ctx.strokeStyle = colors.funcBlockStroke;
    ctx.lineWidth = 1;
    ctx.stroke();
  }

  // ================================================================
  // FuncBlock Drawing / 函数块绘制
  // ================================================================

  private drawFuncBlock(fb: FuncBlock, fbPorts: Port[], vp: ViewportState, colors: ThemeColors): void {
    const ctx = this.ctx;
    const x = fb.x - fb.width / 2;
    const y = fb.y - fb.height / 2;
    const screen = this.worldToScreen(x, y, vp);
    const screenWidth = fb.width * vp.scale;
    const screenHeight = fb.height * vp.scale;
    const isLight = colors.canvasBg === '#ffffff';
    ctx.beginPath();
    ctx.roundRect(screen.x, screen.y, screenWidth, screenHeight, 6);
    const categoryColors: Record<string, string> = {
      construction: isLight ? 'rgba(34, 139, 34, 0.1)' : 'rgba(60, 179, 113, 0.15)',
      measurement: isLight ? 'rgba(70, 130, 180, 0.1)' : 'rgba(100, 149, 237, 0.15)',
      transform: isLight ? 'rgba(148, 0, 211, 0.1)' : 'rgba(186, 85, 211, 0.15)',
    };
    ctx.fillStyle = categoryColors[fb.category] || colors.funcBlockFill;
    ctx.fill();
    ctx.strokeStyle = colors.funcBlockStroke;
    ctx.lineWidth = 2;
    ctx.stroke();
    ctx.fillStyle = colors.text;
    ctx.font = '11px monospace';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(fb.label, screen.x + screenWidth / 2, screen.y + screenHeight / 2);
    ctx.textAlign = 'left';
    ctx.textBaseline = 'alphabetic';
    const portRadius = 4;
    for (const port of fbPorts) {
      const portScreen = this.worldToScreen(port.x, port.y, vp);
      ctx.beginPath();
      ctx.arc(portScreen.x, portScreen.y, portRadius, 0, Math.PI * 2);
      ctx.fillStyle = port.direction === 'input' ? colors.port : colors.portHover;
      ctx.fill();
      ctx.strokeStyle = colors.funcBlockStroke;
      ctx.lineWidth = 1;
      ctx.stroke();
    }
  }

  // ================================================================
  // Constraint Drawing / 约束绘制
  // ================================================================

  /**
   * 绘制所有约束的可视化标记。
   * 在线段之后、点之前调用，确保约束标记位于正确的层级。
   *
   * @param constraints - 约束数组
   * @param points - 所有点数组（用于坐标查找）
   * @param segments - 所有线段数组（用于端点查找）
   * @param vp - 当前视口状态
   * @param colors - 当前主题颜色
   */
  drawConstraints(
    constraints: Constraint[],
    pointMap: Map<number, Point>,
    segmentMap: Map<number, Segment>,
    vp: ViewportState,
    colors: ThemeColors,
  ): void {
    for (const constraint of constraints) {
      switch (constraint.type) {
        case 'incidence':
          this.drawIncidenceConstraint(constraint, pointMap, segmentMap, vp, colors);
          break;
        case 'betweenness':
          this.drawBetweennessConstraint(constraint, pointMap, vp, colors);
          break;
        case 'intersection':
          this.drawIntersectionConstraint(constraint, pointMap, vp, colors);
          break;
        case 'containment':
          this.drawContainmentConstraint(constraint, pointMap, segmentMap, vp, colors);
          break;
        case 'connection':
          this.drawConnectionConstraint(constraint, pointMap, segmentMap, vp, colors);
          break;
      }
    }
  }

  /**
   * 绘制关联约束（点在线段上）。
   * 在点所在位置绘制一个小方块标记 □。
   * 颜色：使用青色/蓝色作为强调色。
   */
  private drawIncidenceConstraint(
    constraint: Constraint,
    pointMap: Map<number, Point>,
    _segmentMap: Map<number, Segment>,
    vp: ViewportState,
    _colors: ThemeColors,
  ): void {
    // args: [pointId, segmentId]
    if (constraint.args.length < 1) return;
    const point = pointMap.get(constraint.args[0]!);
    if (!point) return;

    const ctx = this.ctx;
    const screen = this.worldToScreen(point.x, point.y, vp);
    const size = 5; // 方块大小（像素）

    ctx.strokeStyle = '#00bcd4'; // 青色
    ctx.lineWidth = 1.5;
    ctx.strokeRect(screen.x - size, screen.y - size, size * 2, size * 2);
  }

  /**
   * 绘制介于约束（B 在 A 和 C 之间）。
   * 在中间点 B 处绘制一个菱形标记 ◇。
   * 绘制一条连接 A-B-C 的细虚线。
   */
  private drawBetweennessConstraint(
    constraint: Constraint,
    pointMap: Map<number, Point>,
    vp: ViewportState,
    _colors: ThemeColors,
  ): void {
    // args: [pointA_id, pointB_id, pointC_id]
    if (constraint.args.length < 3) return;
    const pointA = pointMap.get(constraint.args[0]!);
    const pointB = pointMap.get(constraint.args[1]!);
    const pointC = pointMap.get(constraint.args[2]!);
    if (!pointA || !pointB || !pointC) return;

    const ctx = this.ctx;

    // 绘制 A-B-C 虚线连接
    const sA = this.worldToScreen(pointA.x, pointA.y, vp);
    const sB = this.worldToScreen(pointB.x, pointB.y, vp);
    const sC = this.worldToScreen(pointC.x, pointC.y, vp);

    ctx.strokeStyle = '#ab47bc'; // 紫色
    ctx.lineWidth = 1;
    ctx.setLineDash([4, 4]);
    ctx.beginPath();
    ctx.moveTo(sA.x, sA.y);
    ctx.lineTo(sB.x, sB.y);
    ctx.lineTo(sC.x, sC.y);
    ctx.stroke();
    ctx.setLineDash([]);

    // 在 B 处绘制菱形标记 ◇
    const size = 5;
    ctx.fillStyle = '#ab47bc';
    ctx.beginPath();
    ctx.moveTo(sB.x, sB.y - size);      // 上
    ctx.lineTo(sB.x + size, sB.y);      // 右
    ctx.lineTo(sB.x, sB.y + size);      // 下
    ctx.lineTo(sB.x - size, sB.y);      // 左
    ctx.closePath();
    ctx.fill();
  }

  /**
   * 绘制相交约束（两线段交点）。
   * 在交点处绘制一个小圆圈标记 ○。
   * 颜色：使用橙色以区分。
   */
  private drawIntersectionConstraint(
    constraint: Constraint,
    pointMap: Map<number, Point>,
    vp: ViewportState,
    _colors: ThemeColors,
  ): void {
    // args: [intersectionPointId, segment1Id, segment2Id, ...]
    if (constraint.args.length < 1) return;
    const point = pointMap.get(constraint.args[0]!);
    if (!point) return;

    const ctx = this.ctx;
    const screen = this.worldToScreen(point.x, point.y, vp);
    const radius = 6;

    ctx.strokeStyle = '#ff9800'; // 橙色
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    ctx.arc(screen.x, screen.y, radius, 0, Math.PI * 2);
    ctx.stroke();
  }

  /**
   * 绘制包含约束（点/区域包含于另一区域）。
   * 绘制一个低透明度的阴影区域。
   */
  private drawContainmentConstraint(
    constraint: Constraint,
    pointMap: Map<number, Point>,
    segmentMap: Map<number, Segment>,
    vp: ViewportState,
    colors: ThemeColors,
  ): void {
    // args: [containedPointId_or_regionId, containingRegionId_or_segmentId, ...]
    // 尝试将 args 解释为点 ID 列表，绘制凸包作为包含区域
    if (constraint.args.length < 2) return;

    const ctx = this.ctx;
    const containedPoints: Point[] = [];

    // 收集所有参数对应的点
    for (const argId of constraint.args) {
      const p = pointMap.get(argId);
      if (p) containedPoints.push(p);
    }

    // 如果至少有 3 个点，绘制阴影区域
    if (containedPoints.length >= 3) {
      const isLight = colors.canvasBg === '#ffffff';
      ctx.fillStyle = isLight
        ? 'rgba(76, 175, 80, 0.08)'  // 绿色低透明度
        : 'rgba(76, 175, 80, 0.12)';
      ctx.strokeStyle = isLight
        ? 'rgba(76, 175, 80, 0.3)'
        : 'rgba(76, 175, 80, 0.4)';
      ctx.lineWidth = 1;

      ctx.beginPath();
      const first = this.worldToScreen(containedPoints[0]!.x, containedPoints[0]!.y, vp);
      ctx.moveTo(first.x, first.y);
      for (let i = 1; i < containedPoints.length; i++) {
        const p = this.worldToScreen(containedPoints[i]!.x, containedPoints[i]!.y, vp);
        ctx.lineTo(p.x, p.y);
      }
      ctx.closePath();
      ctx.fill();
      ctx.stroke();
    } else if (containedPoints.length === 2) {
      // 两个点：绘制线段高亮
      const s1 = this.worldToScreen(containedPoints[0]!.x, containedPoints[0]!.y, vp);
      const s2 = this.worldToScreen(containedPoints[1]!.x, containedPoints[1]!.y, vp);
      const isLight = colors.canvasBg === '#ffffff';
      ctx.strokeStyle = isLight
        ? 'rgba(76, 175, 80, 0.3)'
        : 'rgba(76, 175, 80, 0.4)';
      ctx.lineWidth = 2;
      ctx.setLineDash([6, 3]);
      ctx.beginPath();
      ctx.moveTo(s1.x, s1.y);
      ctx.lineTo(s2.x, s2.y);
      ctx.stroke();
      ctx.setLineDash([]);
    }

    // 尝试查找关联线段并高亮
    for (const argId of constraint.args) {
      const seg = segmentMap.get(argId);
      if (seg) {
        const p1 = pointMap.get(seg.p1);
        const p2 = pointMap.get(seg.p2);
        if (p1 && p2) {
          const sp1 = this.worldToScreen(p1.x, p1.y, vp);
          const sp2 = this.worldToScreen(p2.x, p2.y, vp);
          const isLight = colors.canvasBg === '#ffffff';
          ctx.strokeStyle = isLight
            ? 'rgba(76, 175, 80, 0.5)'
            : 'rgba(76, 175, 80, 0.6)';
          ctx.lineWidth = 2;
          ctx.beginPath();
          ctx.moveTo(sp1.x, sp1.y);
          ctx.lineTo(sp2.x, sp2.y);
          ctx.stroke();
        }
      }
    }
  }

  /**
   * 绘制连接约束（几何元素之间的一般连接）。
   * 在连接的元素之间绘制一条细点线。
   */
  private drawConnectionConstraint(
    constraint: Constraint,
    pointMap: Map<number, Point>,
    segmentMap: Map<number, Segment>,
    vp: ViewportState,
    _colors: ThemeColors,
  ): void {
    // args: [elementId1, elementId2, ...]
    if (constraint.args.length < 2) return;

    const ctx = this.ctx;
    ctx.strokeStyle = '#78909c'; // 蓝灰色
    ctx.lineWidth = 1;
    ctx.setLineDash([2, 4]); // 点线样式

    // 尝试将 args 解析为点或线段的中点
    const screenPositions: Array<{ x: number; y: number }> = [];

    for (const argId of constraint.args) {
      // 先尝试作为点 ID
      const point = pointMap.get(argId);
      if (point) {
        screenPositions.push(this.worldToScreen(point.x, point.y, vp));
        continue;
      }
      // 再尝试作为线段 ID，取中点
      const seg = segmentMap.get(argId);
      if (seg) {
        const p1 = pointMap.get(seg.p1);
        const p2 = pointMap.get(seg.p2);
        if (p1 && p2) {
          const midX = (p1.x + p2.x) / 2;
          const midY = (p1.y + p2.y) / 2;
          screenPositions.push(this.worldToScreen(midX, midY, vp));
        }
      }
    }

    // 绘制连接线
    if (screenPositions.length >= 2) {
      ctx.beginPath();
      ctx.moveTo(screenPositions[0]!.x, screenPositions[0]!.y);
      for (let i = 1; i < screenPositions.length; i++) {
        ctx.lineTo(screenPositions[i]!.x, screenPositions[i]!.y);
      }
      ctx.stroke();
    }

    ctx.setLineDash([]);
  }

  // ================================================================
  // Label Drawing / 标签绘制
  // ================================================================

  /**
   * Draw a point label (node ID) next to the point.
   */
  private drawLabel(point: Point, vp: ViewportState, colors: ThemeColors): void {
    const ctx = this.ctx;
    const screen = this.worldToScreen(point.x, point.y, vp);

    ctx.fillStyle = colors.text;
    ctx.font = '10px monospace';
    ctx.fillText(`n${point.id}`, screen.x + 8, screen.y - 6);
  }

  // ================================================================
  // Canvas Setup / 画布设置
  // ================================================================

  /**
   * Resize the canvas to match its container with high-DPI support.
   * Call this on window resize and initial setup.
   */
  setupCanvas(): void {
    const dpr = window.devicePixelRatio || 1;
    const w = this.canvas.offsetWidth;
    const h = this.canvas.offsetHeight;

    // Set physical pixel dimensions
    this.canvas.width = Math.floor(w * dpr);
    this.canvas.height = Math.floor(h * dpr);

    // CSS dimensions stay the same
    this.canvas.style.width = `${w}px`;
    this.canvas.style.height = `${h}px`;

    // Reset transform and apply DPR scaling
    this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  }

  /**
   * Find the point nearest to a given screen position within a threshold.
   * @param screenX - Screen X coordinate
   * @param screenY - Screen Y coordinate
   * @param points - Array of points to search
   * @param vp - Current viewport state
   * @param threshold - Maximum distance in CSS pixels (default 10)
   * @returns The nearest point within threshold, or null
   */
  findPointAt(
    screenX: number,
    screenY: number,
    points: Point[],
    vp: ViewportState,
    threshold: number = 10,
  ): Point | null {
    let nearest: Point | null = null;
    let minDist = threshold;

    for (const point of points) {
      const screen = this.worldToScreen(point.x, point.y, vp);
      const dx = screen.x - screenX;
      const dy = screen.y - screenY;
      const dist = Math.sqrt(dx * dx + dy * dy);

      if (dist < minDist) {
        minDist = dist;
        nearest = point;
      }
    }

    return nearest;
  }
}
