import React, { useRef, useEffect, useCallback } from 'react';
import { DrawCmd, CanvasEvent } from '../types';
import { useCanvasStore } from '../../L5-core/store/canvasStore';

interface CanvasViewProps {
  commands: DrawCmd[];
  onCanvasEvent?: (e: CanvasEvent) => void;
  width?: number;
  height?: number;
}

export const CanvasView: React.FC<CanvasViewProps> = ({
  commands,
  onCanvasEvent,
  width = 1100,
  height = 650,
}) => {
  const ref = useRef<HTMLCanvasElement>(null);
  const dpr = useCanvasStore((s) => s.dpr);
  const setCanvasSize = useCanvasStore((s) => s.setCanvasSize);

  useEffect(() => {
    setCanvasSize(width, height);
  }, [width, height, setCanvasSize]);

  useEffect(() => {
    const c = ref.current;
    if (!c) return;
    const ctx = c.getContext('2d');
    if (!ctx) return;
    let animId: number;
    let alive = true;

    const render = () => {
      if (!alive) return;
      const cw = c.width / dpr;
      const ch = c.height / dpr;
      ctx.fillStyle = '#0a0e14';
      ctx.fillRect(0, 0, cw, ch);
      drawGrid(ctx, cw, ch);
      renderCommands(ctx, commands);
      animId = requestAnimationFrame(render);
    };

    render();
    return () => { alive = false; cancelAnimationFrame(animId); };
  }, [commands, dpr]);

  const toCanvas = useCallback((cx: number, cy: number) => {
    const rect = ref.current?.getBoundingClientRect();
    if (!rect) return { x: 0, y: 0 };
    return { x: (cx - rect.left) * dpr, y: (cy - rect.top) * dpr };
  }, [dpr]);

  return (
    <canvas
      ref={ref}
      width={width * dpr}
      height={height * dpr}
      style={{ display: 'block', background: '#0a0e14' }}
      onMouseDown={(e) => {
        const { x, y } = toCanvas(e.clientX, e.clientY);
        onCanvasEvent?.({ type: 'mousedown', screenX: x, screenY: y, button: e.button, shiftKey: e.shiftKey });
      }}
      onMouseMove={(e) => {
        const { x, y } = toCanvas(e.clientX, e.clientY);
        onCanvasEvent?.({ type: 'mousemove', screenX: x, screenY: y });
      }}
      onMouseUp={(e) => {
        const { x, y } = toCanvas(e.clientX, e.clientY);
        onCanvasEvent?.({ type: 'mouseup', screenX: x, screenY: y, button: e.button });
      }}
      onWheel={(e) => {
        e.preventDefault();
        onCanvasEvent?.({ type: 'wheel', screenX: 0, screenY: 0, delta: e.deltaY > 0 ? -1 : 1 });
      }}
      onContextMenu={(e) => e.preventDefault()}
    />
  );
};

function drawGrid(ctx: CanvasRenderingContext2D, w: number, h: number) {
  ctx.strokeStyle = '#21262d';
  ctx.lineWidth = 0.5;
  const gs = 50;
  if (gs < 6) return;
  ctx.beginPath();
  for (let x = 0; x < w; x += gs) { ctx.moveTo(x, 0); ctx.lineTo(x, h); }
  for (let y = 0; y < h; y += gs) { ctx.moveTo(0, y); ctx.lineTo(w, y); }
  ctx.stroke();
}

function renderCommands(ctx: CanvasRenderingContext2D, cmds: DrawCmd[]) {
  for (const c of cmds) {
    const col = c.color ?? 'rgba(255,255,255,0.8)';
    switch (c.type) {
      case 'LINE':
        ctx.beginPath();
        ctx.moveTo(c.x1, c.y1);
        ctx.lineTo(c.x2 ?? c.x1, c.y2 ?? c.y1);
        ctx.strokeStyle = col;
        ctx.lineWidth = c.lineWidth ?? 2;
        if (c.style === 'DASHED') ctx.setLineDash([8, 6]);
        else if (c.style === 'DOTTED') ctx.setLineDash([3, 5]);
        else ctx.setLineDash([]);
        ctx.stroke();
        break;
      case 'POINT':
        ctx.beginPath();
        ctx.arc(c.x1, c.y1, c.radius ?? 5, 0, 2 * Math.PI);
        ctx.fillStyle = col;
        ctx.fill();
        break;
      case 'TEXT':
        ctx.fillStyle = col;
        ctx.font = "13px 'Consolas',monospace";
        ctx.fillText(c.text ?? '', c.x1, c.y1);
        break;
      case 'HIGHLIGHT':
        ctx.save();
        ctx.shadowColor = 'rgba(255,255,255,0.35)';
        ctx.shadowBlur = 20;
        ctx.beginPath();
        ctx.arc(c.x1, c.y1, c.radius ?? 14, 0, 2 * Math.PI);
        ctx.strokeStyle = col;
        ctx.lineWidth = 2.5;
        ctx.stroke();
        ctx.restore();
        break;
    }
  }
}
