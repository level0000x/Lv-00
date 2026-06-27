import React, { useRef, useEffect, useCallback, useState } from 'react';
import { useGeometryStore, GeoObject } from '../../L5-core/store/geometryStore';

/* ---- Props ---- */
interface CanvasViewProps {
  activeTool?: string;
  width?: number;
  height?: number;
  onStatusMessage?: (msg: string) => void;
}

const GRID_SIZE = 50;

/* ================================================================
 * CanvasView -- fully driven by useGeometryStore
 * ================================================================ */
export const CanvasView: React.FC<CanvasViewProps> = ({
  activeTool = 'select',
  width: propWidth,
  height: propHeight,
  onStatusMessage,
}) => {
  const ref = useRef<HTMLCanvasElement>(null);
  const containerRef = useRef<HTMLDivElement>(null);
  const [size, setSize] = useState({ w: propWidth ?? 800, h: propHeight ?? 600 });
  const [hover, setHover] = useState<{ x: number; y: number } | null>(null);
  const [dragPointId, setDragPointId] = useState<string | null>(null);
  const [firstClickId, setFirstClickId] = useState<string | null>(null);

  // Read store state
  const objects = useGeometryStore((s) => s.objects);
  const selectedIds = useGeometryStore((s) => s.selectedIds);
  const addObject = useGeometryStore((s) => s.addObject);
  const moveObject = useGeometryStore((s) => s.moveObject);
  const selectObjects = useGeometryStore((s) => s.selectObjects);
  const getPointById = useGeometryStore((s) => s.getPointById);
  const getDistance = useGeometryStore((s) => s.getDistance);

  // ResizeObserver
  useEffect(() => {
    if (propWidth && propHeight) {
      setSize({ w: propWidth, h: propHeight });
      return;
    }
    const container = containerRef.current;
    if (!container) return;
    const ro = new ResizeObserver((entries) => {
      for (const entry of entries) {
        const { width, height } = entry.contentRect;
        if (width > 0 && height > 0) {
          setSize({ w: Math.floor(width), h: Math.floor(height) });
        }
      }
    });
    ro.observe(container);
    return () => ro.disconnect();
  }, [propWidth, propHeight]);

  const dpr = typeof window !== 'undefined' ? window.devicePixelRatio || 1 : 1;

  // Canvas size sync
  useEffect(() => {
    const c = ref.current;
    if (!c) return;
    c.width = size.w * dpr;
    c.height = size.h * dpr;
    c.style.width = size.w + 'px';
    c.style.height = size.h + 'px';
  }, [size.w, size.h, dpr]);

  // Find a point near given coordinates
  const findNearPoint = useCallback((x: number, y: number, threshold = 14): GeoObject | null => {
    let best: GeoObject | null = null;
    let bestDist = threshold;
    for (const o of objects) {
      if (o.type !== 'point' || !o.visible) continue;
      if (o.x === undefined || o.y === undefined) continue;
      const dx = o.x - x, dy = o.y - y;
      const d = Math.sqrt(dx * dx + dy * dy);
      if (d < bestDist) { bestDist = d; best = o; }
    }
    return best;
  }, [objects]);

  // Client -> canvas scene coordinates
  const toScene = useCallback((clientX: number, clientY: number) => {
    const rect = ref.current?.getBoundingClientRect();
    if (!rect) return { x: 0, y: 0 };
    return { x: clientX - rect.left, y: clientY - rect.top };
  }, []);

  const status = useCallback((msg: string) => {
    onStatusMessage?.(msg);
  }, [onStatusMessage]);

  // Mouse down
  const handleMouseDown = useCallback((e: React.MouseEvent<HTMLCanvasElement>) => {
    if (e.button !== 0) return;
    const { x, y } = toScene(e.clientX, e.clientY);

    if (activeTool === 'select') {
      const near = findNearPoint(x, y);
      if (near) {
        selectObjects([near.id]);
        setDragPointId(near.id);
      } else {
        selectObjects([]);
      }
    } else if (activeTool === 'point') {
      const id = addObject({ type: 'point', label: '', x, y, visible: true });
      selectObjects([id]);
      status(`点 Point added (${Math.round(x)}, ${Math.round(y)})`);
    } else if (activeTool === 'segment') {
      const near = findNearPoint(x, y);
      if (near) {
        if (!firstClickId) {
          setFirstClickId(near.id);
          status(`线段 Segment: 选择第二个端点 select second endpoint`);
        } else if (firstClickId !== near.id) {
          const startPt = getPointById(firstClickId);
          const id = addObject({
            type: 'segment',
            label: `${startPt ? '?' : ''}${near.label}`,
            startId: firstClickId,
            endId: near.id,
            visible: true,
          });
          // auto-label
          const sp = objects.find(o => o.id === firstClickId);
          useGeometryStore.getState().updateObject(id, {
            label: `${sp?.label ?? '?'}${near.label}`,
          });
          selectObjects([id]);
          const dist = getDistance(firstClickId, near.id);
          status(`线段 Segment ${sp?.label ?? '?'}${near.label} = ${dist.toFixed(1)}`);
          setFirstClickId(null);
        }
      }
    } else if (activeTool === 'circle') {
      const near = findNearPoint(x, y);
      if (near) {
        if (!firstClickId) {
          setFirstClickId(near.id);
          status(`圆 Circle: 选择半径点 select radius point`);
        } else if (firstClickId !== near.id) {
          const r = getDistance(firstClickId, near.id);
          const centerPt = getPointById(firstClickId);
          const id = addObject({
            type: 'circle',
            label: `circle(${centerPt ? '' : '?'})`,
            centerId: firstClickId,
            radiusPointId: near.id,
            radius: r,
            visible: true,
          });
          // auto-label
          const cp = objects.find(o => o.id === firstClickId);
          useGeometryStore.getState().updateObject(id, {
            label: `C(${cp?.label ?? '?'})`,
          });
          selectObjects([id]);
          status(`圆 Circle center=${cp?.label ?? '?'} radius=${r.toFixed(1)}`);
          setFirstClickId(null);
        }
      }
    }
  }, [activeTool, toScene, findNearPoint, firstClickId, addObject, selectObjects, status, getDistance, getPointById, objects]);

  const handleMouseMove = useCallback((e: React.MouseEvent<HTMLCanvasElement>) => {
    const { x, y } = toScene(e.clientX, e.clientY);
    setHover({ x, y });

    if (dragPointId && activeTool === 'select') {
      moveObject(dragPointId, x, y);
    }
  }, [toScene, dragPointId, activeTool, moveObject]);

  const handleMouseUp = useCallback(() => {
    setDragPointId(null);
  }, []);

  const cursorStyle = activeTool === 'select'
    ? (dragPointId ? 'grabbing' : 'default')
    : 'crosshair';

  // Render loop
  useEffect(() => {
    const c = ref.current;
    if (!c) return;
    const ctx = c.getContext('2d');
    if (!ctx) return;
    let animId: number;
    let alive = true;

    const render = () => {
      if (!alive) return;

      ctx.save();
      ctx.scale(dpr, dpr);

      // Background
      ctx.fillStyle = '#0a0e14';
      ctx.fillRect(0, 0, size.w, size.h);

      // Grid
      drawGrid(ctx, size.w, size.h);

      // Axes
      drawAxes(ctx, size.w, size.h);

      // Current store snapshot
      const s = useGeometryStore.getState();
      const ptMap = new Map<string, GeoObject>();
      for (const o of s.objects) {
        if (o.type === 'point') ptMap.set(o.id, o);
      }

      // Draw lines (behind everything)
      for (const o of s.objects) {
        if (o.type !== 'line' || !o.visible) continue;
        const from = ptMap.get(o.startId!);
        const to = ptMap.get(o.endId!);
        if (!from || !to || from.x === undefined || from.y === undefined || to.x === undefined || to.y === undefined) continue;
        const sel = s.selectedIds.includes(o.id);
        drawLineShape(ctx, from.x, from.y, to.x, to.y, o.color, 1.5, 'dashed', sel);
      }

      // Draw segments
      for (const o of s.objects) {
        if (o.type !== 'segment' || !o.visible) continue;
        const from = ptMap.get(o.startId!);
        const to = ptMap.get(o.endId!);
        if (!from || !to || from.x === undefined || from.y === undefined || to.x === undefined || to.y === undefined) continue;
        const sel = s.selectedIds.includes(o.id);
        drawLineShape(ctx, from.x, from.y, to.x, to.y, o.color, 2, 'solid', sel);
      }

      // Draw circles
      for (const o of s.objects) {
        if (o.type !== 'circle' || !o.visible) continue;
        const center = ptMap.get(o.centerId!);
        if (!center || center.x === undefined || center.y === undefined) continue;
        const sel = s.selectedIds.includes(o.id);
        drawCircleShape(ctx, center.x, center.y, o.radius ?? 50, o.color, 2, 'solid', sel, o.label);
      }

      // Draw points (on top)
      for (const o of s.objects) {
        if (o.type !== 'point' || !o.visible) continue;
        if (o.x === undefined || o.y === undefined) continue;
        const sel = s.selectedIds.includes(o.id);
        drawPointShape(ctx, o.x, o.y, o.color, o.label, sel);
      }

      // Hover crosshair for drawing modes
      const currentHover = hover;
      const currentTool = activeTool;
      const currentFirstId = firstClickId;

      if (currentHover && currentTool !== 'select') {
        ctx.strokeStyle = 'rgba(255,255,255,0.25)';
        ctx.lineWidth = 0.5;
        ctx.setLineDash([4, 4]);
        ctx.beginPath();
        ctx.moveTo(currentHover.x, 0);
        ctx.lineTo(currentHover.x, size.h);
        ctx.moveTo(0, currentHover.y);
        ctx.lineTo(size.w, currentHover.y);
        ctx.stroke();
        ctx.setLineDash([]);

        ctx.font = "11px 'Consolas', monospace";
        ctx.fillStyle = 'rgba(255,255,255,0.7)';
        ctx.fillText(`(${Math.round(currentHover.x)}, ${Math.round(currentHover.y)})`, currentHover.x + 14, currentHover.y - 10);
      }

      // First-click indicator glow
      if (currentFirstId) {
        const fp = s.objects.find((p) => p.id === currentFirstId);
        if (fp && fp.x !== undefined && fp.y !== undefined) {
          ctx.save();
          ctx.shadowColor = 'rgba(0,188,212,0.6)';
          ctx.shadowBlur = 14;
          ctx.beginPath();
          ctx.arc(fp.x, fp.y, 12, 0, 2 * Math.PI);
          ctx.strokeStyle = '#00bcd4';
          ctx.lineWidth = 2;
          ctx.stroke();
          ctx.restore();
        }
      }

      ctx.restore();
      animId = requestAnimationFrame(render);
    };

    render();
    return () => { alive = false; cancelAnimationFrame(animId); };
  }, [size.w, size.h, dpr, hover, activeTool, firstClickId]);

  return (
    <div ref={containerRef} style={{ position: 'relative', width: '100%', height: '100%', overflow: 'hidden' }}>
      <canvas
        ref={ref}
        style={{ display: 'block', background: 'var(--color-bg-canvas)', cursor: cursorStyle }}
        onMouseDown={handleMouseDown}
        onMouseMove={handleMouseMove}
        onMouseUp={handleMouseUp}
        onContextMenu={(e) => e.preventDefault()}
      />
    </div>
  );
};

/* ================================================================
 * Drawing helpers
 * ================================================================ */

function drawGrid(ctx: CanvasRenderingContext2D, w: number, h: number) {
  ctx.strokeStyle = '#21262d';
  ctx.lineWidth = 0.5;
  ctx.beginPath();
  for (let x = 0; x <= w; x += GRID_SIZE) { ctx.moveTo(x, 0); ctx.lineTo(x, h); }
  for (let y = 0; y <= h; y += GRID_SIZE) { ctx.moveTo(0, y); ctx.lineTo(w, y); }
  ctx.stroke();
}

function drawAxes(ctx: CanvasRenderingContext2D, w: number, h: number) {
  ctx.strokeStyle = 'rgba(255,255,255,0.08)';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(0, h / 2);
  ctx.lineTo(w, h / 2);
  ctx.stroke();
  ctx.beginPath();
  ctx.moveTo(w / 2, 0);
  ctx.lineTo(w / 2, h);
  ctx.stroke();

  ctx.fillStyle = 'rgba(255,255,255,0.15)';
  ctx.font = "9px 'Consolas', monospace";
  for (let x = 0; x <= w; x += 100) {
    if (x === Math.floor(w / 2)) continue;
    ctx.fillText(String(x - Math.floor(w / 2)), x, h / 2 + 12);
  }
  for (let y = 0; y <= h; y += 100) {
    if (y === Math.floor(h / 2)) continue;
    ctx.fillText(String(Math.floor(h / 2) - y), w / 2 + 6, y + 3);
  }
}

function drawPointShape(ctx: CanvasRenderingContext2D, x: number, y: number, color: string, label: string, selected: boolean) {
  if (selected) {
    ctx.save();
    ctx.shadowColor = 'rgba(0,188,212,0.6)';
    ctx.shadowBlur = 16;
    ctx.beginPath();
    ctx.arc(x, y, 10, 0, 2 * Math.PI);
    ctx.strokeStyle = 'rgba(0,188,212,0.8)';
    ctx.lineWidth = 2;
    ctx.stroke();
    ctx.restore();
  }

  ctx.beginPath();
  ctx.arc(x, y, 5, 0, 2 * Math.PI);
  ctx.fillStyle = color;
  ctx.fill();

  ctx.font = "bold 13px 'Consolas', monospace";
  ctx.fillStyle = '#e6edf3';
  ctx.fillText(label, x + 10, y - 10);
}

function drawLineShape(
  ctx: CanvasRenderingContext2D,
  x1: number, y1: number, x2: number, y2: number,
  color: string, lineWidth: number, style: string,
  selected: boolean,
) {
  ctx.save();
  if (selected) {
    ctx.shadowColor = 'rgba(0,188,212,0.5)';
    ctx.shadowBlur = 10;
  }
  ctx.beginPath();
  ctx.moveTo(x1, y1);
  ctx.lineTo(x2, y2);
  ctx.strokeStyle = selected ? '#00bcd4' : color;
  ctx.lineWidth = selected ? lineWidth + 1 : lineWidth;
  if (style === 'dashed') ctx.setLineDash([8, 6]);
  else if (style === 'dotted') ctx.setLineDash([3, 5]);
  else ctx.setLineDash([]);
  ctx.stroke();
  ctx.restore();
}

function drawCircleShape(
  ctx: CanvasRenderingContext2D,
  cx: number, cy: number, radius: number,
  color: string, lineWidth: number, style: string,
  selected: boolean, label: string,
) {
  ctx.save();
  if (selected) {
    ctx.shadowColor = 'rgba(0,188,212,0.5)';
    ctx.shadowBlur = 10;
  }
  ctx.beginPath();
  ctx.arc(cx, cy, radius, 0, 2 * Math.PI);
  ctx.strokeStyle = selected ? '#00bcd4' : color;
  ctx.lineWidth = selected ? lineWidth + 1 : lineWidth;
  if (style === 'dashed') ctx.setLineDash([8, 6]);
  else if (style === 'dotted') ctx.setLineDash([3, 5]);
  else ctx.setLineDash([]);
  ctx.stroke();
  ctx.restore();

  ctx.font = "bold 12px 'Consolas', monospace";
  ctx.fillStyle = selected ? '#00bcd4' : color;
  ctx.fillText(`${label} r=${radius.toFixed(0)}`, cx + radius + 6, cy - 4);
}
