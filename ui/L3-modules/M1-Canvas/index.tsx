import React, { useRef, useEffect, useCallback, useState } from 'react';
import { DrawCmd } from '../types';
import { useCanvasStore } from '../../L5-core/store/canvasStore';
import { trustColorToCSS } from '../../L5-core/protocol';

/* ---- Local geometric objects ---- */
interface LocalPoint {
  kind: 'point';
  id: string;
  label: string;
  x: number;
  y: number;
  color: string;
}

interface LocalSegment {
  kind: 'segment';
  id: string;
  fromId: string;
  toId: string;
  color: string;
}

interface LocalCircle {
  kind: 'circle';
  id: string;
  centerId: string;
  edgeId: string;
  color: string;
}

type LocalObject = LocalPoint | LocalSegment | LocalCircle;

/* ---- Props ---- */
interface CanvasViewProps {
  commands: DrawCmd[];
  onMouseDown?: (sx: number, sy: number, btn: number) => void;
  onMouseMove?: (sx: number, sy: number) => void;
  onMouseUp?: (sx: number, sy: number, btn: number) => void;
  onWheel?: (delta: number) => void;
  width?: number;
  height?: number;
  activeTool?: string; // 'select' | 'point' | 'segment' | 'line' | 'circle' | 'angle' | 'pan'
  onObjectsChange?: (objects: LocalObject[]) => void;
}

/* ---- Auto label counter ---- */
let labelCounter = 0;
function nextLabel(): string {
  const letters = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ';
  const idx = labelCounter % 26;
  const suffix = labelCounter >= 26 ? Math.floor(labelCounter / 26) : '';
  labelCounter++;
  return letters[idx] + suffix;
}

export const CanvasView: React.FC<CanvasViewProps> = ({
  commands, onMouseDown, onMouseMove, onMouseUp, onWheel,
  width = 1100, height = 650,
  activeTool = 'select',
  onObjectsChange,
}) => {
  const ref = useRef<HTMLCanvasElement>(null);
  const dpr = useCanvasStore((s) => s.dpr);
  const setCanvasSize = useCanvasStore((s) => s.setCanvasSize);

  /* ---- local state ---- */
  const [objects, setObjects] = useState<LocalObject[]>([]);
  const [hover, setHover] = useState<{ x: number; y: number } | null>(null);
  const [dragPointId, setDragPointId] = useState<string | null>(null);
  const [segFirstId, setSegFirstId] = useState<string | null>(null); // first click for segment/circle
  const [editingLabel, setEditingLabel] = useState<string | null>(null); // point id being renamed
  const [editLabelPos, setEditLabelPos] = useState<{ x: number; y: number }>({ x: 0, y: 0 });
  const [editLabelText, setEditLabelText] = useState('');
  const editInputRef = useRef<HTMLInputElement>(null);

  useEffect(() => { setCanvasSize(width, height); }, [width, height, setCanvasSize]);

  /* ---- helper: find point by proximity ---- */
  const findNearPoint = useCallback((x: number, y: number, threshold = 12): LocalPoint | null => {
    let best: LocalPoint | null = null;
    let bestDist = threshold;
    for (const obj of objects) {
      if (obj.kind !== 'point') continue;
      const dx = obj.x - x, dy = obj.y - y;
      const d = Math.sqrt(dx * dx + dy * dy);
      if (d < bestDist) { bestDist = d; best = obj; }
    }
    return best;
  }, [objects]);

  /* ---- helper: all points for rendering ---- */
  const getPoints = useCallback((): LocalPoint[] => objects.filter((o): o is LocalPoint => o.kind === 'point'), [objects]);

  /* ---- Build local DrawCmds from objects ---- */
  const buildLocalCmds = useCallback((): DrawCmd[] => {
    const cmds: DrawCmd[] = [];
    const points = getPoints();
    const pointMap = new Map(points.map((p) => [p.id, p]));

    for (const obj of objects) {
      if (obj.kind === 'point') {
        cmds.push({
          type: 'POINT', x1: obj.x, y1: obj.y, x2: 0, y2: 0,
          radius: 6, text: obj.label, colorRGBA: 0,
          trustColor: 'GREEN', lineWidth: 2, style: 'SOLID',
        });
        cmds.push({
          type: 'TEXT', x1: obj.x + 10, y1: obj.y - 10, x2: 0, y2: 0,
          radius: 0, text: obj.label, colorRGBA: 0,
          trustColor: 'GREEN', lineWidth: 0, style: 'SOLID',
        });
      } else if (obj.kind === 'segment') {
        const from = pointMap.get(obj.fromId);
        const to = pointMap.get(obj.toId);
        if (from && to) {
          cmds.push({
            type: 'LINE', x1: from.x, y1: from.y, x2: to.x, y2: to.y,
            radius: 0, text: '', colorRGBA: 0,
            trustColor: 'BLUE', lineWidth: 2, style: 'SOLID',
          });
        }
      } else if (obj.kind === 'circle') {
        const center = pointMap.get(obj.centerId);
        const edge = pointMap.get(obj.edgeId);
        if (center && edge) {
          const r = Math.sqrt((edge.x - center.x) ** 2 + (edge.y - center.y) ** 2);
          const steps = 48;
          for (let i = 0; i < steps; i++) {
            const a1 = (2 * Math.PI * i) / steps;
            const a2 = (2 * Math.PI * (i + 1)) / steps;
            cmds.push({
              type: 'LINE',
              x1: center.x + Math.cos(a1) * r, y1: center.y + Math.sin(a1) * r,
              x2: center.x + Math.cos(a2) * r, y2: center.y + Math.sin(a2) * r,
              radius: 0, text: '', colorRGBA: 0,
              trustColor: 'YELLOW', lineWidth: 1.5, style: 'DOTTED',
            });
          }
        }
      }
    }
    return cmds;
  }, [objects, getPoints]);

  /* ---- Render loop ---- */
  useEffect(() => {
    const c = ref.current; if (!c) return;
    const ctx = c.getContext('2d'); if (!ctx) return;
    let animId: number; let alive = true;
    const render = () => {
      if (!alive) return;
      ctx.fillStyle = '#0a0e14'; ctx.fillRect(0, 0, c.width / dpr, c.height / dpr);
      drawGrid(ctx, c.width / dpr, c.height / dpr);
      renderCommands(ctx, commands);
      renderCommands(ctx, buildLocalCmds());

      // hover crosshair for drawing modes
      if (hover && activeTool !== 'select') {
        const hx = hover.x / dpr, hy = hover.y / dpr;
        ctx.strokeStyle = 'rgba(255,255,255,0.25)';
        ctx.lineWidth = 0.5;
        ctx.setLineDash([4, 4]);
        ctx.beginPath();
        ctx.moveTo(hx, 0); ctx.lineTo(hx, c.height / dpr);
        ctx.moveTo(0, hy); ctx.lineTo(c.width / dpr, hy);
        ctx.stroke();
        ctx.setLineDash([]);

        // coordinate tooltip
        const coordText = `(${Math.round(hx)}, ${Math.round(hy)})`;
        ctx.font = "11px 'Consolas', monospace";
        ctx.fillStyle = 'rgba(255,255,255,0.7)';
        ctx.fillText(coordText, hx + 12, hy - 8);
      }

      // segment/circle first-point indicator
      if (segFirstId) {
        const p = getPoints().find((pt) => pt.id === segFirstId);
        if (p) {
          ctx.save();
          ctx.shadowColor = 'rgba(0,188,212,0.6)';
          ctx.shadowBlur = 12;
          ctx.beginPath();
          ctx.arc(p.x, p.y, 10, 0, 2 * Math.PI);
          ctx.strokeStyle = '#00bcd4';
          ctx.lineWidth = 2;
          ctx.stroke();
          ctx.restore();
        }
      }

      animId = requestAnimationFrame(render);
    };
    render();
    return () => { alive = false; cancelAnimationFrame(animId); };
  }, [commands, dpr, activeTool, hover, buildLocalCmds, segFirstId, getPoints]);

  /* ---- Edit label effect ---- */
  useEffect(() => {
    if (editingLabel && editInputRef.current) {
      editInputRef.current.focus();
      editInputRef.current.select();
    }
  }, [editingLabel]);

  /* ---- Coordinate transform ---- */
  const tc = useCallback((cx: number, cy: number) => {
    const r = ref.current?.getBoundingClientRect();
    return r ? { x: (cx - r.left) * dpr, y: (cy - r.top) * dpr } : { x: 0, y: 0 };
  }, [dpr]);

  /* ---- Scene coords (divide by dpr) ---- */
  const toScene = useCallback((cx: number, cy: number) => {
    const r = ref.current?.getBoundingClientRect();
    if (!r) return { x: 0, y: 0 };
    return { x: cx - r.left, y: cy - r.top };
  }, []);

  /* ---- Update objects and notify ---- */
  const updateObjects = useCallback((next: LocalObject[]) => {
    setObjects(next);
    onObjectsChange?.(next);
  }, [onObjectsChange]);

  /* ---- Mouse handlers ---- */
  const handleMouseDown = useCallback((e: React.MouseEvent<HTMLCanvasElement>) => {
    const canvasX = tc(e.clientX, e.clientY).x / dpr;
    const canvasY = tc(e.clientX, e.clientY).y / dpr;
    const p = tc(e.clientX, e.clientY);
    onMouseDown?.(p.x, p.y, e.button);

    if (e.button !== 0) return;

    if (activeTool === 'select') {
      const near = findNearPoint(canvasX, canvasY);
      if (near) setDragPointId(near.id);
    } else if (activeTool === 'point') {
      const label = nextLabel();
      const newPt: LocalPoint = {
        kind: 'point', id: `pt_${Date.now()}`, label,
        x: canvasX, y: canvasY, color: '#3fb950',
      };
      updateObjects([...objects, newPt]);
    } else if (activeTool === 'segment' || activeTool === 'line') {
      const near = findNearPoint(canvasX, canvasY);
      if (near) {
        if (!segFirstId) {
          setSegFirstId(near.id);
        } else if (segFirstId !== near.id) {
          const newSeg: LocalSegment = {
            kind: 'segment', id: `seg_${Date.now()}`,
            fromId: segFirstId, toId: near.id, color: '#58a6ff',
          };
          updateObjects([...objects, newSeg]);
          setSegFirstId(null);
        }
      }
    } else if (activeTool === 'circle') {
      const near = findNearPoint(canvasX, canvasY);
      if (near) {
        if (!segFirstId) {
          setSegFirstId(near.id); // center
        } else if (segFirstId !== near.id) {
          const newCircle: LocalCircle = {
            kind: 'circle', id: `circ_${Date.now()}`,
            centerId: segFirstId, edgeId: near.id, color: '#d29922',
          };
          updateObjects([...objects, newCircle]);
          setSegFirstId(null);
        }
      }
    }
  }, [activeTool, tc, dpr, onMouseDown, findNearPoint, segFirstId, objects, updateObjects]);

  const handleMouseMove = useCallback((e: React.MouseEvent<HTMLCanvasElement>) => {
    const p = tc(e.clientX, e.clientY);
    onMouseMove?.(p.x, p.y);
    setHover({ x: p.x, y: p.y });

    if (dragPointId && activeTool === 'select') {
      const canvasX = p.x / dpr;
      const canvasY = p.y / dpr;
      updateObjects(objects.map((o) =>
        o.kind === 'point' && o.id === dragPointId ? { ...o, x: canvasX, y: canvasY } : o
      ));
    }
  }, [tc, dpr, onMouseMove, dragPointId, activeTool, objects, updateObjects]);

  const handleMouseUp = useCallback((e: React.MouseEvent<HTMLCanvasElement>) => {
    const p = tc(e.clientX, e.clientY);
    onMouseUp?.(p.x, p.y, e.button);
    setDragPointId(null);
  }, [tc, onMouseUp]);

  /* ---- Double click to edit label ---- */
  const handleDblClick = useCallback((e: React.MouseEvent<HTMLCanvasElement>) => {
    const canvasX = tc(e.clientX, e.clientY).x / dpr;
    const canvasY = tc(e.clientX, e.clientY).y / dpr;
    const near = findNearPoint(canvasX, canvasY);
    if (near) {
      setEditingLabel(near.id);
      setEditLabelPos({ x: near.x + 10, y: near.y - 24 });
      setEditLabelText(near.label);
    }
  }, [tc, dpr, findNearPoint]);

  /* ---- Commit label edit ---- */
  const commitLabel = useCallback(() => {
    if (editingLabel && editLabelText.trim()) {
      updateObjects(objects.map((o) =>
        o.kind === 'point' && o.id === editingLabel ? { ...o, label: editLabelText.trim() } : o
      ));
    }
    setEditingLabel(null);
  }, [editingLabel, editLabelText, objects, updateObjects]);

  /* ---- Cursor style ---- */
  const cursorStyle = activeTool === 'select' ? 'default' : 'crosshair';

  return (
    <div style={{ position: 'relative', width, height }}>
      <canvas
        ref={ref}
        width={width * dpr}
        height={height * dpr}
        style={{ display: 'block', background: '#0a0e14', cursor: cursorStyle }}
        onMouseDown={handleMouseDown}
        onMouseMove={handleMouseMove}
        onMouseUp={handleMouseUp}
        onWheel={e => { e.preventDefault(); onWheel?.(e.deltaY > 0 ? -1 : 1); }}
        onContextMenu={e => e.preventDefault()}
        onDoubleClick={handleDblClick}
      />
      {/* Inline label editor */}
      {editingLabel && (
        <input
          ref={editInputRef}
          value={editLabelText}
          onChange={(e) => setEditLabelText(e.target.value)}
          onBlur={commitLabel}
          onKeyDown={(e) => {
            if (e.key === 'Enter') commitLabel();
            if (e.key === 'Escape') setEditingLabel(null);
          }}
          style={{
            position: 'absolute',
            left: editLabelPos.x,
            top: editLabelPos.y,
            background: 'var(--color-bg-elevated)',
            border: '1px solid var(--color-accent)',
            borderRadius: 3,
            color: 'var(--color-text-bright)',
            fontFamily: "'Consolas', monospace",
            fontSize: 13,
            padding: '2px 6px',
            outline: 'none',
            minWidth: 40,
            zIndex: 10,
            boxShadow: 'var(--shadow-md)',
          }}
        />
      )}
    </div>
  );
};

/* ================================================================
 * Canvas drawing helpers
 * ================================================================ */

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
      case 'LINE':
        ctx.beginPath(); ctx.moveTo(c.x1, c.y1); ctx.lineTo(c.x2, c.y2);
        ctx.strokeStyle = col; ctx.lineWidth = c.lineWidth || 2;
        if (c.style === 'DASHED') ctx.setLineDash([8, 6]);
        else if (c.style === 'DOTTED') ctx.setLineDash([3, 5]);
        else ctx.setLineDash([]);
        ctx.stroke(); break;
      case 'POINT':
        ctx.beginPath(); ctx.arc(c.x1, c.y1, c.radius || 5, 0, 2 * Math.PI);
        ctx.fillStyle = col; ctx.fill(); break;
      case 'TEXT':
        ctx.fillStyle = col; ctx.font = "13px 'Consolas',monospace";
        ctx.fillText(c.text, c.x1, c.y1); break;
      case 'HIGHLIGHT':
        ctx.save(); ctx.shadowColor = 'rgba(255,255,255,0.35)'; ctx.shadowBlur = 20;
        ctx.beginPath(); ctx.arc(c.x1, c.y1, c.radius || 14, 0, 2 * Math.PI);
        ctx.strokeStyle = col; ctx.lineWidth = 2.5; ctx.stroke();
        ctx.restore(); break;
    }
  }
}

export type { LocalObject, LocalPoint, LocalSegment, LocalCircle };
