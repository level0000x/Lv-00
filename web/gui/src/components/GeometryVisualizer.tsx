/**
 * @module components/GeometryVisualizer
 * @description 几何可视化组件 - 实现基础几何图形渲染（点、线、圆、多边形）
 *              以及逻辑结构图可视化功能
 *
 * 主要功能 / Key Features:
 * - 渲染基础几何图形：点、线段、圆、多边形
 * - 支持逻辑结构图可视化（树形图、流程图）
 * - 提供交互式几何构造工具
 * - 支持动画和过渡效果
 * - 与 LV-00 核心几何引擎集成
 */

import React, { useRef, useEffect, useCallback, useState } from 'react';
import { useAppStore } from '@/stores';

/* ============ 类型定义 ============ */

export interface Point2D {
  x: number;
  y: number;
  id?: string;
  label?: string;
  color?: string;
  size?: number;
}

export interface Line2D {
  start: Point2D;
  end: Point2D;
  id?: string;
  color?: string;
  width?: number;
  dashed?: boolean;
}

export interface Circle2D {
  center: Point2D;
  radius: number;
  id?: string;
  color?: string;
  fillColor?: string;
  width?: number;
}

export interface Polygon2D {
  points: Point2D[];
  id?: string;
  color?: string;
  fillColor?: string;
  closed?: boolean;
}

export interface LogicNode {
  id: string;
  label: string;
  x: number;
  y: number;
  type: 'root' | 'node' | 'leaf';
  children?: string[];
  parent?: string;
  color?: string;
}

export interface LogicEdge {
  from: string;
  to: string;
  label?: string;
  color?: string;
}

export interface LogicStructure {
  nodes: LogicNode[];
  edges: LogicEdge[];
}

export interface GeometryScene {
  points?: Point2D[];
  lines?: Line2D[];
  circles?: Circle2D[];
  polygons?: Polygon2D[];
  logicStructure?: LogicStructure;
}

export interface GeometryVisualizerProps {
  scene?: GeometryScene;
  width?: number;
  height?: number;
  showGrid?: boolean;
  showAxes?: boolean;
  interactive?: boolean;
  onPointClick?: (point: Point2D) => void;
  onLineClick?: (line: Line2D) => void;
  onCircleClick?: (circle: Circle2D) => void;
  className?: string;
}

/* ============ 辅助函数 ============ */

const DEFAULT_COLORS = {
  point: '#3b82f6',
  line: '#1f2937',
  circle: '#10b981',
  polygon: '#f59e0b',
  grid: '#e5e7eb',
  axis: '#9ca3af',
  text: '#374151',
  logicRoot: '#dc2626',
  logicNode: '#2563eb',
  logicLeaf: '#059669',
};

function worldToScreen(
  x: number,
  y: number,
  viewport: { scale: number; offsetX: number; offsetY: number; width: number; height: number }
): { x: number; y: number } {
  return {
    x: x * viewport.scale + viewport.offsetX + viewport.width / 2,
    y: -y * viewport.scale + viewport.offsetY + viewport.height / 2,
  };
}

function screenToWorld(
  sx: number,
  sy: number,
  viewport: { scale: number; offsetX: number; offsetY: number; width: number; height: number }
): { x: number; y: number } {
  return {
    x: (sx - viewport.offsetX - viewport.width / 2) / viewport.scale,
    y: -(sy - viewport.offsetY - viewport.height / 2) / viewport.scale,
  };
}

/* ============ 渲染函数 ============ */

function drawGrid(
  ctx: CanvasRenderingContext2D,
  viewport: { scale: number; offsetX: number; offsetY: number; width: number; height: number },
  color: string
): void {
  const gridSize = 50;
  const { width, height } = viewport;

  ctx.strokeStyle = color;
  ctx.lineWidth = 1;
  ctx.beginPath();

  // 垂直线
  const startX = Math.floor((-viewport.offsetX - width / 2) / viewport.scale / gridSize) * gridSize;
  const endX = Math.ceil((-viewport.offsetX + width / 2) / viewport.scale / gridSize) * gridSize;
  for (let x = startX; x <= endX; x += gridSize) {
    const screenX = worldToScreen(x, 0, viewport).x;
    ctx.moveTo(screenX, 0);
    ctx.lineTo(screenX, height);
  }

  // 水平线
  const startY = Math.floor((viewport.offsetY - height / 2) / viewport.scale / gridSize) * gridSize;
  const endY = Math.ceil((viewport.offsetY + height / 2) / viewport.scale / gridSize) * gridSize;
  for (let y = startY; y <= endY; y += gridSize) {
    const screenY = worldToScreen(0, y, viewport).y;
    ctx.moveTo(0, screenY);
    ctx.lineTo(width, screenY);
  }

  ctx.stroke();
}

function drawAxes(
  ctx: CanvasRenderingContext2D,
  viewport: { scale: number; offsetX: number; offsetY: number; width: number; height: number },
  color: string
): void {
  const { width, height } = viewport;
  const origin = worldToScreen(0, 0, viewport);

  ctx.strokeStyle = color;
  ctx.lineWidth = 2;
  ctx.beginPath();

  // X轴
  ctx.moveTo(0, origin.y);
  ctx.lineTo(width, origin.y);

  // Y轴
  ctx.moveTo(origin.x, 0);
  ctx.lineTo(origin.x, height);

  ctx.stroke();

  // 箭头
  ctx.fillStyle = color;
  ctx.beginPath();
  ctx.moveTo(width - 10, origin.y - 5);
  ctx.lineTo(width, origin.y);
  ctx.lineTo(width - 10, origin.y + 5);
  ctx.fill();

  ctx.beginPath();
  ctx.moveTo(origin.x - 5, 10);
  ctx.lineTo(origin.x, 0);
  ctx.lineTo(origin.x + 5, 10);
  ctx.fill();
}

function drawPoint(
  ctx: CanvasRenderingContext2D,
  point: Point2D,
  viewport: { scale: number; offsetX: number; offsetY: number; width: number; height: number }
): void {
  const screenPos = worldToScreen(point.x, point.y, viewport);
  const size = (point.size || 4) * (viewport.scale > 1 ? 1 : viewport.scale * 0.5 + 0.5);

  ctx.fillStyle = point.color || DEFAULT_COLORS.point;
  ctx.beginPath();
  ctx.arc(screenPos.x, screenPos.y, size, 0, Math.PI * 2);
  ctx.fill();

  // 绘制标签
  if (point.label) {
    ctx.fillStyle = DEFAULT_COLORS.text;
    ctx.font = '12px sans-serif';
    ctx.fillText(point.label, screenPos.x + size + 4, screenPos.y - size - 4);
  }
}

function drawLine(
  ctx: CanvasRenderingContext2D,
  line: Line2D,
  viewport: { scale: number; offsetX: number; offsetY: number; width: number; height: number }
): void {
  const start = worldToScreen(line.start.x, line.start.y, viewport);
  const end = worldToScreen(line.end.x, line.end.y, viewport);

  ctx.strokeStyle = line.color || DEFAULT_COLORS.line;
  ctx.lineWidth = line.width || 2;

  if (line.dashed) {
    ctx.setLineDash([5, 5]);
  } else {
    ctx.setLineDash([]);
  }

  ctx.beginPath();
  ctx.moveTo(start.x, start.y);
  ctx.lineTo(end.x, end.y);
  ctx.stroke();
  ctx.setLineDash([]);
}

function drawCircle(
  ctx: CanvasRenderingContext2D,
  circle: Circle2D,
  viewport: { scale: number; offsetX: number; offsetY: number; width: number; height: number }
): void {
  const center = worldToScreen(circle.center.x, circle.center.y, viewport);
  const radius = circle.radius * viewport.scale;

  ctx.strokeStyle = circle.color || DEFAULT_COLORS.circle;
  ctx.lineWidth = circle.width || 2;

  if (circle.fillColor) {
    ctx.fillStyle = circle.fillColor;
    ctx.beginPath();
    ctx.arc(center.x, center.y, radius, 0, Math.PI * 2);
    ctx.fill();
  }

  ctx.beginPath();
  ctx.arc(center.x, center.y, radius, 0, Math.PI * 2);
  ctx.stroke();

  // 绘制中心点
  ctx.fillStyle = circle.color || DEFAULT_COLORS.circle;
  ctx.beginPath();
  ctx.arc(center.x, center.y, 3, 0, Math.PI * 2);
  ctx.fill();
}

function drawPolygon(
  ctx: CanvasRenderingContext2D,
  polygon: Polygon2D,
  viewport: { scale: number; offsetX: number; offsetY: number; width: number; height: number }
): void {
  if (polygon.points.length < 2) return;

  ctx.strokeStyle = polygon.color || DEFAULT_COLORS.polygon;
  ctx.lineWidth = 2;

  if (polygon.fillColor) {
    ctx.fillStyle = polygon.fillColor;
  }

  ctx.beginPath();
  const first = worldToScreen(polygon.points[0].x, polygon.points[0].y, viewport);
  ctx.moveTo(first.x, first.y);

  for (let i = 1; i < polygon.points.length; i++) {
    const point = worldToScreen(polygon.points[i].x, polygon.points[i].y, viewport);
    ctx.lineTo(point.x, point.y);
  }

  if (polygon.closed !== false) {
    ctx.closePath();
  }

  if (polygon.fillColor) {
    ctx.fill();
  }
  ctx.stroke();

  // 绘制顶点
  polygon.points.forEach((point) => {
    drawPoint(ctx, { ...point, size: 3 }, viewport);
  });
}

/* ============ 逻辑结构图渲染 ============ */

function drawLogicStructure(
  ctx: CanvasRenderingContext2D,
  structure: LogicStructure,
  viewport: { scale: number; offsetX: number; offsetY: number; width: number; height: number }
): void {
  // 绘制边
  structure.edges.forEach((edge) => {
    const fromNode = structure.nodes.find((n) => n.id === edge.from);
    const toNode = structure.nodes.find((n) => n.id === edge.to);
    if (!fromNode || !toNode) return;

    const from = worldToScreen(fromNode.x, fromNode.y, viewport);
    const to = worldToScreen(toNode.x, toNode.y, viewport);

    ctx.strokeStyle = edge.color || '#6b7280';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(from.x, from.y);
    ctx.lineTo(to.x, to.y);
    ctx.stroke();

    // 绘制箭头
    const angle = Math.atan2(to.y - from.y, to.x - from.x);
    const arrowSize = 8;
    ctx.beginPath();
    ctx.moveTo(to.x - arrowSize * Math.cos(angle - Math.PI / 6), to.y - arrowSize * Math.sin(angle - Math.PI / 6));
    ctx.lineTo(to.x, to.y);
    ctx.lineTo(to.x - arrowSize * Math.cos(angle + Math.PI / 6), to.y - arrowSize * Math.sin(angle + Math.PI / 6));
    ctx.stroke();

    // 绘制边标签
    if (edge.label) {
      ctx.fillStyle = '#6b7280';
      ctx.font = '10px sans-serif';
      const midX = (from.x + to.x) / 2;
      const midY = (from.y + to.y) / 2;
      ctx.fillText(edge.label, midX + 5, midY - 5);
    }
  });

  // 绘制节点
  structure.nodes.forEach((node) => {
    const pos = worldToScreen(node.x, node.y, viewport);
    const size = 30 * (viewport.scale > 1 ? 1 : viewport.scale * 0.5 + 0.5);

    // 根据类型选择颜色
    let color = DEFAULT_COLORS.logicNode;
    if (node.type === 'root') color = DEFAULT_COLORS.logicRoot;
    else if (node.type === 'leaf') color = DEFAULT_COLORS.logicLeaf;
    if (node.color) color = node.color;

    // 绘制节点背景
    ctx.fillStyle = color;
    ctx.beginPath();
    if (node.type === 'root') {
      ctx.rect(pos.x - size, pos.y - size / 2, size * 2, size);
    } else if (node.type === 'leaf') {
      ctx.ellipse(pos.x, pos.y, size, size / 2, 0, 0, Math.PI * 2);
    } else {
      ctx.arc(pos.x, pos.y, size / 2, 0, Math.PI * 2);
    }
    ctx.fill();

    // 绘制边框
    ctx.strokeStyle = '#ffffff';
    ctx.lineWidth = 2;
    ctx.stroke();

    // 绘制标签
    ctx.fillStyle = '#ffffff';
    ctx.font = 'bold 12px sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(node.label, pos.x, pos.y);
    ctx.textAlign = 'left';
    ctx.textBaseline = 'alphabetic';
  });
}

/* ============ 主组件 ============ */

export const GeometryVisualizer: React.FC<GeometryVisualizerProps> = ({
  scene = {},
  width = 800,
  height = 600,
  showGrid = true,
  showAxes = true,
  interactive = true,
  onPointClick,
  onLineClick,
  onCircleClick,
  className = '',
}) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [viewport, setViewport] = useState({
    scale: 1,
    offsetX: 0,
    offsetY: 0,
    width,
    height,
  });
  const [isDragging, setIsDragging] = useState(false);
  const [dragStart, setDragStart] = useState({ x: 0, y: 0 });

  const render = useCallback(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    // 清空画布
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    // 绘制网格
    if (showGrid) {
      drawGrid(ctx, viewport, DEFAULT_COLORS.grid);
    }

    // 绘制坐标轴
    if (showAxes) {
      drawAxes(ctx, viewport, DEFAULT_COLORS.axis);
    }

    // 绘制几何图形
    if (scene.polygons) {
      scene.polygons.forEach((polygon) => drawPolygon(ctx, polygon, viewport));
    }

    if (scene.circles) {
      scene.circles.forEach((circle) => drawCircle(ctx, circle, viewport));
    }

    if (scene.lines) {
      scene.lines.forEach((line) => drawLine(ctx, line, viewport));
    }

    if (scene.points) {
      scene.points.forEach((point) => drawPoint(ctx, point, viewport));
    }

    // 绘制逻辑结构图
    if (scene.logicStructure) {
      drawLogicStructure(ctx, scene.logicStructure, viewport);
    }
  }, [scene, viewport, showGrid, showAxes]);

  useEffect(() => {
    render();
  }, [render]);

  // 处理鼠标事件
  const handleMouseDown = useCallback(
    (e: React.MouseEvent) => {
      if (!interactive) return;

      const rect = canvasRef.current?.getBoundingClientRect();
      if (!rect) return;

      const x = e.clientX - rect.left;
      const y = e.clientY - rect.top;

      if (e.button === 1 || (e.button === 0 && e.shiftKey)) {
        // 中键或Shift+左键：平移
        setIsDragging(true);
        setDragStart({ x, y });
      }
    },
    [interactive]
  );

  const handleMouseMove = useCallback(
    (e: React.MouseEvent) => {
      if (!interactive || !isDragging) return;

      const rect = canvasRef.current?.getBoundingClientRect();
      if (!rect) return;

      const x = e.clientX - rect.left;
      const y = e.clientY - rect.top;

      const dx = x - dragStart.x;
      const dy = y - dragStart.y;

      setViewport((prev) => ({
        ...prev,
        offsetX: prev.offsetX + dx,
        offsetY: prev.offsetY + dy,
      }));

      setDragStart({ x, y });
    },
    [interactive, isDragging, dragStart]
  );

  const handleMouseUp = useCallback(() => {
    setIsDragging(false);
  }, []);

  const handleWheel = useCallback(
    (e: React.WheelEvent) => {
      if (!interactive) return;
      e.preventDefault();

      const rect = canvasRef.current?.getBoundingClientRect();
      if (!rect) return;

      const mouseX = e.clientX - rect.left;
      const mouseY = e.clientY - rect.top;

      const worldPos = screenToWorld(mouseX, mouseY, viewport);

      const scaleFactor = e.deltaY > 0 ? 0.9 : 1.1;
      const newScale = Math.max(0.1, Math.min(10, viewport.scale * scaleFactor));

      const newOffsetX = mouseX - worldPos.x * newScale - viewport.width / 2;
      const newOffsetY = mouseY + worldPos.y * newScale - viewport.height / 2;

      setViewport((prev) => ({
        ...prev,
        scale: newScale,
        offsetX: newOffsetX,
        offsetY: newOffsetY,
      }));
    },
    [interactive, viewport]
  );

  const handleClick = useCallback(
    (e: React.MouseEvent) => {
      if (!interactive) return;

      const rect = canvasRef.current?.getBoundingClientRect();
      if (!rect) return;

      const x = e.clientX - rect.left;
      const y = e.clientY - rect.top;
      const worldPos = screenToWorld(x, y, viewport);

      // 检查是否点击了点
      if (scene.points) {
        const clickedPoint = scene.points.find((p) => {
          const dist = Math.sqrt((p.x - worldPos.x) ** 2 + (p.y - worldPos.y) ** 2);
          return dist < 10 / viewport.scale;
        });
        if (clickedPoint && onPointClick) {
          onPointClick(clickedPoint);
          return;
        }
      }

      // 检查是否点击了圆
      if (scene.circles) {
        const clickedCircle = scene.circles.find((c) => {
          const dist = Math.sqrt((c.center.x - worldPos.x) ** 2 + (c.center.y - worldPos.y) ** 2);
          return Math.abs(dist - c.radius) < 10 / viewport.scale;
        });
        if (clickedCircle && onCircleClick) {
          onCircleClick(clickedCircle);
          return;
        }
      }

      // 检查是否点击了线段
      if (scene.lines) {
        const clickedLine = scene.lines.find((l) => {
          const dist = pointToLineDistance(worldPos.x, worldPos.y, l.start.x, l.start.y, l.end.x, l.end.y);
          return dist < 10 / viewport.scale;
        });
        if (clickedLine && onLineClick) {
          onLineClick(clickedLine);
        }
      }
    },
    [interactive, viewport, scene, onPointClick, onLineClick, onCircleClick]
  );

  // 辅助函数：计算点到线段的距离
  function pointToLineDistance(px: number, py: number, x1: number, y1: number, x2: number, y2: number): number {
    const A = px - x1;
    const B = py - y1;
    const C = x2 - x1;
    const D = y2 - y1;

    const dot = A * C + B * D;
    const lenSq = C * C + D * D;
    let param = -1;

    if (lenSq !== 0) {
      param = dot / lenSq;
    }

    let xx, yy;

    if (param < 0) {
      xx = x1;
      yy = y1;
    } else if (param > 1) {
      xx = x2;
      yy = y2;
    } else {
      xx = x1 + param * C;
      yy = y1 + param * D;
    }

    const dx = px - xx;
    const dy = py - yy;

    return Math.sqrt(dx * dx + dy * dy);
  }

  return (
    <div className={`geometry-visualizer ${className}`} style={{ position: 'relative' }}>
      <canvas
        ref={canvasRef}
        width={width}
        height={height}
        onMouseDown={handleMouseDown}
        onMouseMove={handleMouseMove}
        onMouseUp={handleMouseUp}
        onMouseLeave={handleMouseUp}
        onWheel={handleWheel}
        onClick={handleClick}
        style={{
          cursor: isDragging ? 'grabbing' : 'default',
          border: '1px solid #e5e7eb',
          borderRadius: '4px',
        }}
      />
      <div
        style={{
          position: 'absolute',
          bottom: '10px',
          left: '10px',
          background: 'rgba(255, 255, 255, 0.9)',
          padding: '8px 12px',
          borderRadius: '4px',
          fontSize: '12px',
          color: '#6b7280',
        }}
      >
        缩放: {(viewport.scale * 100).toFixed(0)}% | 滚轮缩放 | 中键/Shift+拖拽平移
      </div>
    </div>
  );
};

export default GeometryVisualizer;
