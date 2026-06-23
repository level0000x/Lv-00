import React, { useRef, useEffect, useCallback } from 'react';
import { DrawCmd } from '../types';
import { useCanvasStore } from '../../L5-core/store/canvasStore';
import { trustColorToCSS } from '../../L5-core/protocol';

interface CanvasViewProps {
  commands: DrawCmd[];
  onMouseDown?: (sx: number, sy: number, btn: number) => void;
  onMouseMove?: (sx: number, sy: number) => void;
  onMouseUp?: (sx: number, sy: number, btn: number) => void;
  onWheel?: (delta: number) => void;
  width?: number;
  height?: number;
}

export const CanvasView: React.FC<CanvasViewProps> = ({
  commands, onMouseDown, onMouseMove, onMouseUp, onWheel,
  width = 1100, height = 650,
}) => {
  const ref = useRef<HTMLCanvasElement>(null);
  const dpr = useCanvasStore((s) => s.dpr);
  const setCanvasSize = useCanvasStore((s) => s.setCanvasSize);

  useEffect(() => { setCanvasSize(width, height); }, [width, height, setCanvasSize]);

  useEffect(() => {
    const c = ref.current; if (!c) return;
    const ctx = c.getContext('2d'); if (!ctx) return;
    let animId: number; let alive = true;
    const render = () => {
      if (!alive) return;
      ctx.fillStyle = '#0a0e14'; ctx.fillRect(0, 0, c.width / dpr, c.height / dpr);
      drawGrid(ctx, c.width / dpr, c.height / dpr);
      renderCommands(ctx, commands);
      animId = requestAnimationFrame(render);
    };
    render();
    return () => { alive = false; cancelAnimationFrame(animId); };
  }, [commands, dpr]);

  const tc = useCallback((cx: number, cy: number) => {
    const r = ref.current?.getBoundingClientRect();
    return r ? { x: (cx - r.left) * dpr, y: (cy - r.top) * dpr } : { x: 0, y: 0 };
  }, [dpr]);

  return <canvas ref={ref} width={width * dpr} height={height * dpr}
    style={{ display: 'block', background: '#0a0e14' }}
    onMouseDown={e => { const p = tc(e.clientX, e.clientY); onMouseDown?.(p.x, p.y, e.button); }}
    onMouseMove={e => { const p = tc(e.clientX, e.clientY); onMouseMove?.(p.x, p.y); }}
    onMouseUp={e => { const p = tc(e.clientX, e.clientY); onMouseUp?.(p.x, p.y, e.button); }}
    onWheel={e => { e.preventDefault(); onWheel?.(e.deltaY > 0 ? -1 : 1); }}
    onContextMenu={e => e.preventDefault()} />;
};

function drawGrid(ctx: CanvasRenderingContext2D, w: number, h: number) {
  ctx.strokeStyle = '#21262d'; ctx.lineWidth = 0.5;
  ctx.beginPath();
  for (let x = 0; x < w; x += 50) { ctx.moveTo(x, 0); ctx.lineTo(x, h); }
  for (let y = 0; y < h; y += 50) { ctx.moveTo(0, y); ctx.lineTo(w, y); }
  ctx.stroke();
}

function cmdColor(c: DrawCmd): string {
  return trustColorToCSS(c.trustColor);
}

function renderCommands(ctx: CanvasRenderingContext2D, cmds: DrawCmd[]) {
  for (const c of cmds) {
    const col = cmdColor(c);
    switch (c.type) {
      case 'LINE': ctx.beginPath(); ctx.moveTo(c.x1, c.y1); ctx.lineTo(c.x2, c.y2); ctx.strokeStyle = col; ctx.lineWidth = c.lineWidth || 2; if (c.style === 'DASHED') ctx.setLineDash([8, 6]); else if (c.style === 'DOTTED') ctx.setLineDash([3, 5]); else ctx.setLineDash([]); ctx.stroke(); break;
      case 'POINT': ctx.beginPath(); ctx.arc(c.x1, c.y1, c.radius || 5, 0, 2 * Math.PI); ctx.fillStyle = col; ctx.fill(); break;
      case 'TEXT': ctx.fillStyle = col; ctx.font = "13px 'Consolas',monospace"; ctx.fillText(c.text, c.x1, c.y1); break;
      case 'HIGHLIGHT': ctx.save(); ctx.shadowColor = 'rgba(255,255,255,0.35)'; ctx.shadowBlur = 20; ctx.beginPath(); ctx.arc(c.x1, c.y1, c.radius || 14, 0, 2 * Math.PI); ctx.strokeStyle = col; ctx.lineWidth = 2.5; ctx.stroke(); ctx.restore(); break;
    }
  }
}
