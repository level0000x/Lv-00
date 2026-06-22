// ============================================================
// @lv00/modal-canvas — M1 几何画布 (CanvasView)
// Canvas 2D 渲染 + 鼠标/键盘交互，与 SceneController 绑定
// ============================================================

import React, { useRef, useEffect, useCallback, useState } from 'react';
import { SceneController } from '@lv00/scene-controller';
import { DrawCommand, LineStyle } from '@lv00/protocol';

interface CanvasViewProps {
  controller: SceneController;
  width?: number;
  height?: number;
  showGrid?: boolean;
}

export const CanvasView: React.FC<CanvasViewProps> = ({
  controller,
  width = 800,
  height = 600,
  showGrid = true,
}) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [isReady, setIsReady] = useState(false);
  const dpr = typeof window !== 'undefined' ? window.devicePixelRatio || 1 : 1;

  // --- 渲染主循环 ---
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    let animationId: number;
    let isRunning = true;
    let lastGridScale = 0;
    let lastOffsetX = 0;
    let lastOffsetY = 0;

    const render = () => {
      if (!isRunning) return;

      const vp = controller.getViewport();
      const w = canvas.width / dpr;
      const h = canvas.height / dpr;

      // 视口参数变化时全量重绘背景
      const needsBgRedraw =
        vp.scale !== lastGridScale ||
        vp.offsetX !== lastOffsetX ||
        vp.offsetY !== lastOffsetY;

      if (needsBgRedraw) {
        // 绘制背景
        ctx.fillStyle = '#0a0a0a';
        ctx.fillRect(0, 0, w, h);

        if (showGrid) {
          drawGrid(ctx, vp, w, h);
        }

        lastGridScale = vp.scale;
        lastOffsetX = vp.offsetX;
        lastOffsetY = vp.offsetY;
      }

      // 获取增量绘制指令并渲染
      const commands = controller.getDrawCommands();
      if (commands.length > 0 || needsBgRedraw) {
        // 非全量模式：先清屏再绘制
        ctx.fillStyle = '#0a0a0a';
        ctx.fillRect(0, 0, w, h);
        if (showGrid) drawGrid(ctx, vp, w, h);
        renderCommands(ctx, commands);
      }

      animationId = requestAnimationFrame(render);
    };

    render();
    setIsReady(true);

    return () => {
      isRunning = false;
      cancelAnimationFrame(animationId);
    };
  }, [controller, dpr, showGrid]);

  // --- 调整视口 canvas 尺寸 ---
  useEffect(() => {
    controller.setViewport({ canvasWidth: width, canvasHeight: height });
  }, [controller, width, height]);

  // --- 坐标转换工具 ---
  const clientToCanvas = useCallback((clientX: number, clientY: number) => {
    const rect = canvasRef.current?.getBoundingClientRect();
    if (!rect) return { x: 0, y: 0 };
    return {
      x: (clientX - rect.left) * dpr,
      y: (clientY - rect.top) * dpr,
    };
  }, [dpr]);

  // --- 事件处理器 ---
  const handleMouseDown = useCallback((e: React.MouseEvent) => {
    const { x, y } = clientToCanvas(e.clientX, e.clientY);
    controller.onKeyDown('Shift', e.shiftKey);
    controller.onMouseDown(x, y, e.button);
  }, [controller, clientToCanvas]);

  const handleMouseMove = useCallback((e: React.MouseEvent) => {
    const { x, y } = clientToCanvas(e.clientX, e.clientY);
    controller.onMouseMove(x, y);
  }, [controller, clientToCanvas]);

  const handleMouseUp = useCallback((e: React.MouseEvent) => {
    const { x, y } = clientToCanvas(e.clientX, e.clientY);
    controller.onMouseUp(x, y, e.button);
  }, [controller, clientToCanvas]);

  const handleWheel = useCallback((e: React.WheelEvent) => {
    e.preventDefault();
    const { x, y } = clientToCanvas(e.clientX, e.clientY);
    const delta = e.deltaY > 0 ? -1 : 1;
    controller.onMouseWheel(delta, x, y);
  }, [controller, clientToCanvas]);

  return (
    <canvas
      ref={canvasRef}
      width={width * dpr}
      height={height * dpr}
      style={{
        width: `${width}px`,
        height: `${height}px`,
        display: 'block',
        background: '#0a0a0a',
        cursor: 'crosshair',
        imageRendering: 'auto',
      }}
      onMouseDown={handleMouseDown}
      onMouseMove={handleMouseMove}
      onMouseUp={handleMouseUp}
      onWheel={handleWheel}
      onContextMenu={e => e.preventDefault()}
    />
  );
};

// ---- 网格绘制 ----

function drawGrid(ctx: CanvasRenderingContext2D, vp: any, w: number, h: number) {
  ctx.strokeStyle = '#1a1a1a';
  ctx.lineWidth = 0.5;
  ctx.setLineDash([]);

  // 世界坐标系下的网格间距（动态适配缩放）
  const worldStep = Math.pow(10, Math.floor(Math.log10(200 / vp.scale)));
  const gridStep = worldStep * vp.scale;

  if (gridStep < 5) return; // 太密集不绘制

  // 计算起始 offset
  const offsetX = (vp.offsetX * vp.scale) % gridStep;
  const offsetY = (vp.offsetY * vp.scale) % gridStep;

  ctx.beginPath();
  for (let x = offsetX; x < w; x += gridStep) {
    ctx.moveTo(x, 0);
    ctx.lineTo(x, h);
  }
  for (let y = offsetY; y < h; y += gridStep) {
    ctx.moveTo(0, y);
    ctx.lineTo(w, y);
  }
  ctx.stroke();
}

// ---- 渲染指令执行 ----

function renderCommands(ctx: CanvasRenderingContext2D, commands: DrawCommand[]) {
  for (const cmd of commands) {
    const color = intToRGBA(cmd.color);

    switch (cmd.type) {
      case 'LINE':
        ctx.beginPath();
        ctx.moveTo(cmd.x1, cmd.y1);
        ctx.lineTo(cmd.x2 ?? cmd.x1, cmd.y2 ?? cmd.y1);
        ctx.strokeStyle = color;
        ctx.lineWidth = cmd.lineWidth ?? 2;
        applyLineStyle(ctx, cmd.style ?? 'SOLID');
        ctx.stroke();
        break;

      case 'POINT':
        ctx.beginPath();
        ctx.arc(cmd.x1, cmd.y1, cmd.radius ?? 5, 0, 2 * Math.PI);
        ctx.fillStyle = color;
        ctx.fill();
        // 选中节点加外圈
        if ((cmd.radius ?? 5) > 6) {
          ctx.beginPath();
          ctx.arc(cmd.x1, cmd.y1, (cmd.radius ?? 5) + 3, 0, 2 * Math.PI);
          ctx.strokeStyle = 'rgba(255,255,255,0.5)';
          ctx.lineWidth = 2;
          ctx.setLineDash([]);
          ctx.stroke();
        }
        break;

      case 'CIRCLE':
        ctx.beginPath();
        ctx.arc(cmd.x1, cmd.y1, cmd.radius ?? 10, 0, 2 * Math.PI);
        ctx.strokeStyle = color;
        ctx.lineWidth = cmd.lineWidth ?? 2;
        ctx.stroke();
        break;

      case 'TEXT':
        ctx.fillStyle = color;
        ctx.font = '14px monospace';
        ctx.textBaseline = 'bottom';
        ctx.fillText(cmd.text ?? '', cmd.x1, cmd.y1);
        break;

      case 'HIGHLIGHT':
        ctx.save();
        ctx.shadowColor = 'rgba(255,255,255,0.4)';
        ctx.shadowBlur = 24;
        ctx.beginPath();
        ctx.arc(cmd.x1, cmd.y1, cmd.radius ?? 16, 0, 2 * Math.PI);
        ctx.strokeStyle = color;
        ctx.lineWidth = 2.5;
        ctx.setLineDash([]);
        ctx.stroke();
        ctx.restore();
        break;

      case 'RECT':
        ctx.beginPath();
        ctx.rect(cmd.x1, cmd.y1, (cmd.x2 ?? 0) - cmd.x1, (cmd.y2 ?? 0) - cmd.y1);
        ctx.strokeStyle = color;
        ctx.lineWidth = cmd.lineWidth ?? 1;
        applyLineStyle(ctx, cmd.style ?? 'DASHED');
        ctx.stroke();
        break;
    }
  }
}

function intToRGBA(color: number): string {
  const a = ((color >> 24) & 0xFF) / 255;
  const r = (color >> 16) & 0xFF;
  const g = (color >> 8) & 0xFF;
  const b = color & 0xFF;
  return `rgba(${r},${g},${b},${a})`;
}

function applyLineStyle(ctx: CanvasRenderingContext2D, style: LineStyle) {
  switch (style) {
    case 'DASHED': ctx.setLineDash([8, 6]); break;
    case 'DOTTED': ctx.setLineDash([3, 5]); break;
    default: ctx.setLineDash([]); break;
  }
}
