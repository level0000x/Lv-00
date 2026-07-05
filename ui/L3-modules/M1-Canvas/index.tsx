import React, { useRef, useEffect, useCallback, useState, useMemo } from 'react';
import { useGeometryStore, GeoObject } from '../../L5-core/store/geometryStore';

interface CanvasViewProps {
  activeTool?: string;
  width?: number;
  height?: number;
  onStatusMessage?: (msg: string) => void;
}

const PADDING = 40;

interface Viewport {
  scale: number;
  offsetX: number;
  offsetY: number;
}

export const CanvasView: React.FC<CanvasViewProps> = ({
  activeTool = 'select',
  width: propWidth,
  height: propHeight,
  onStatusMessage,
}) => {
  const ref = useRef<HTMLCanvasElement>(null);
  const containerRef = useRef<HTMLDivElement>(null);
  const [size, setSize] = useState({ w: 800, h: 600 });
  const [hover, setHover] = useState<{ x: number; y: number } | null>(null);
  const [dragPointId, setDragPointId] = useState<string | null>(null);
  const [firstClickId, setFirstClickId] = useState<string | null>(null);
  // Pan state
  const [panStart, setPanStart] = useState<{ mx: number; my: number; ox: number; oy: number } | null>(null);

  const objects = useGeometryStore((s) => s.objects);
  const selectedIds = useGeometryStore((s) => s.selectedIds);
  const addObject = useGeometryStore((s) => s.addObject);
  const moveObject = useGeometryStore((s) => s.moveObject);
  const selectObjects = useGeometryStore((s) => s.selectObjects);
  const getPointById = useGeometryStore((s) => s.getPointById);
  const getDistance = useGeometryStore((s) => s.getDistance);

  // Observe container size
  useEffect(() => {
    if (propWidth && propHeight) { setSize({ w: propWidth, h: propHeight }); return; }
    const measure = () => {
      const container = containerRef.current;
      if (!container) return;
      const rect = container.getBoundingClientRect();
      if (rect.width > 0 && rect.height > 0) {
        setSize({ w: Math.floor(rect.width), h: Math.floor(rect.height) });
      }
    };
    measure();
    const ro = new ResizeObserver(() => measure());
    if (containerRef.current) ro.observe(containerRef.current);
    return () => ro.disconnect();
  }, [propWidth, propHeight]);

  const dpr = typeof window !== 'undefined' ? window.devicePixelRatio || 1 : 1;

  useEffect(() => {
    const c = ref.current;
    if (!c) return;
    c.width = size.w * dpr;
    c.height = size.h * dpr;
    c.style.width = size.w + 'px';
    c.style.height = size.h + 'px';
  }, [size.w, size.h, dpr]);

  // Compute viewport to fit all objects
  const viewport = useMemo<Viewport>(() => {
    const pts = objects
      .filter((o) => o.type === 'point' && o.x != null && o.y != null && o.visible)
      .map((o) => ({ x: o.x!, y: o.y! }));

    if (pts.length === 0) {
      return { scale: 1, offsetX: 0, offsetY: 0 };
    }

    let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
    for (const p of pts) {
      minX = Math.min(minX, p.x);
      minY = Math.min(minY, p.y);
      maxX = Math.max(maxX, p.x);
      maxY = Math.max(maxY, p.y);
    }

    // Also account for circle radii
    for (const o of objects) {
      if (o.type === 'circle' && o.centerId) {
        const center = objects.find((p) => p.id === o.centerId);
        if (center && center.x != null && center.y != null && o.radius) {
          minX = Math.min(minX, center.x - o.radius);
          minY = Math.min(minY, center.y - o.radius);
          maxX = Math.max(maxX, center.x + o.radius);
          maxY = Math.max(maxY, center.y + o.radius);
        }
      }
    }

    const contentW = maxX - minX || 1;
    const contentH = maxY - minY || 1;
    const availW = Math.max(1, size.w - PADDING * 2);
    const availH = Math.max(1, size.h - PADDING * 2);
    const scale = Math.max(0.01, Math.min(availW / contentW, availH / contentH, 2));
    const offsetX = PADDING + (availW - contentW * scale) / 2 - minX * scale;
    const offsetY = PADDING + (availH - contentH * scale) / 2 - minY * scale;

    return { scale, offsetX, offsetY };
  }, [objects, size.w, size.h]);

  // Transform scene coords -> screen coords
  const toScreen = useCallback(
    (sx: number, sy: number) => ({
      x: sx * viewport.scale + viewport.offsetX,
      y: sy * viewport.scale + viewport.offsetY,
    }),
    [viewport]
  );

  // Transform screen coords -> scene coords
  const toScene = useCallback(
    (clientX: number, clientY: number) => {
      const rect = ref.current?.getBoundingClientRect();
      if (!rect) return { x: 0, y: 0 };
      const sx = clientX - rect.left;
      const sy = clientY - rect.top;
      return {
        x: (sx - viewport.offsetX) / viewport.scale,
        y: (sy - viewport.offsetY) / viewport.scale,
      };
    },
    [viewport]
  );

  const status = useCallback((msg: string) => { onStatusMessage?.(msg); }, [onStatusMessage]);

  const findNearPoint = useCallback((sceneX: number, sceneY: number, threshold = 14): GeoObject | null => {
    let best: GeoObject | null = null;
    let bestDist = threshold / viewport.scale;
    for (const o of objects) {
      if (o.type !== 'point' || !o.visible) continue;
      if (o.x === undefined || o.y === undefined) continue;
      const dx = o.x - sceneX, dy = o.y - sceneY;
      const d = Math.sqrt(dx * dx + dy * dy);
      if (d < bestDist) { bestDist = d; best = o; }
    }
    return best;
  }, [objects, viewport.scale]);

  const handleMouseDown = useCallback((e: React.MouseEvent<HTMLCanvasElement>) => {
    if (e.button !== 0) return;
    // Middle button or hand tool = pan
    if (activeTool === 'hand' || e.button === 1) {
      const rect = ref.current?.getBoundingClientRect();
      if (!rect) return;
      setPanStart({ mx: e.clientX, my: e.clientY, ox: viewport.offsetX, oy: viewport.offsetY });
      return;
    }
    const scene = toScene(e.clientX, e.clientY);
    if (activeTool === 'select') {
      const near = findNearPoint(scene.x, scene.y);
      if (near) { selectObjects([near.id]); setDragPointId(near.id); }
      else { selectObjects([]); }
    } else if (activeTool === 'point') {
      const id = addObject({ type: 'point', label: '', x: scene.x, y: scene.y, visible: true });
      selectObjects([id]);
      status('Point added (' + Math.round(scene.x) + ', ' + Math.round(scene.y) + ')');
    } else if (activeTool === 'segment') {
      const near = findNearPoint(scene.x, scene.y);
      if (near) {
        if (!firstClickId) { setFirstClickId(near.id); status('Segment: select second endpoint'); }
        else if (firstClickId !== near.id) {
          const sp = objects.find((o) => o.id === firstClickId);
          const id = addObject({ type: 'segment', label: (sp?.label ?? '?') + near.label, startId: firstClickId, endId: near.id, visible: true });
          selectObjects([id]);
          const dist = getDistance(firstClickId, near.id);
          status('Segment ' + (sp?.label ?? '?') + near.label + ' = ' + dist.toFixed(1));
          setFirstClickId(null);
        }
      }
    } else if (activeTool === 'circle') {
      const near = findNearPoint(scene.x, scene.y);
      if (near) {
        if (!firstClickId) { setFirstClickId(near.id); status('Circle: select radius point'); }
        else if (firstClickId !== near.id) {
          const r = getDistance(firstClickId, near.id);
          const cp = objects.find((o) => o.id === firstClickId);
          const id = addObject({ type: 'circle', label: 'C(' + (cp?.label ?? '?') + ')', centerId: firstClickId, radiusPointId: near.id, radius: r, visible: true });
          selectObjects([id]);
          status('Circle center=' + (cp?.label ?? '?') + ' r=' + r.toFixed(1));
          setFirstClickId(null);
        }
      }
    }
  }, [activeTool, toScene, findNearPoint, firstClickId, addObject, selectObjects, status, getDistance, objects, viewport]);

  const handleMouseMove = useCallback((e: React.MouseEvent<HTMLCanvasElement>) => {
    if (panStart) return; // pan handled below
    const scene = toScene(e.clientX, e.clientY);
    setHover(scene);
    if (dragPointId && activeTool === 'select') { moveObject(dragPointId, scene.x, scene.y); }
  }, [toScene, dragPointId, activeTool, moveObject, panStart]);

  const handleMouseUp = useCallback(() => {
    setDragPointId(null);
    setPanStart(null);
  }, []);

  const cursorStyle = activeTool === 'hand' ? 'grab' : activeTool === 'select' ? (dragPointId ? 'grabbing' : 'default') : 'crosshair';

  // Handle panning via mouse
  useEffect(() => {
    if (!panStart) return;
    const onMove = (e: MouseEvent) => {
      const dx = e.clientX - panStart.mx;
      const dy = e.clientY - panStart.my;
      // We can't mutate viewport directly since it's computed, but we can temporarily override
      // For now, pan is visual only via offset adjustment
    };
    const onUp = () => setPanStart(null);
    document.addEventListener('mousemove', onMove);
    document.addEventListener('mouseup', onUp);
    return () => {
      document.removeEventListener('mousemove', onMove);
      document.removeEventListener('mouseup', onUp);
    };
  }, [panStart]);

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
      ctx.fillStyle = '#0a0e14';
      ctx.fillRect(0, 0, size.w, size.h);

      // Grid (in scene coordinates, transformed to screen)
      const vp = viewport;
      const gridScene = 50; // grid every 50 units in scene space
      const gridScreen = gridScene * vp.scale;

      if (gridScreen > 5) { // only draw grid if cells > 5px
        ctx.strokeStyle = '#21262d';
        ctx.lineWidth = 0.5;
        ctx.beginPath();

        // Vertical grid lines
        const startSceneX = Math.floor(-vp.offsetX / vp.scale / gridScene) * gridScene;
        const endSceneX = Math.ceil((size.w - vp.offsetX) / vp.scale / gridScene) * gridScene;
        for (let sx = startSceneX; sx <= endSceneX; sx += gridScene) {
          const px = sx * vp.scale + vp.offsetX;
          ctx.moveTo(px, 0);
          ctx.lineTo(px, size.h);
        }

        // Horizontal grid lines
        const startSceneY = Math.floor(-vp.offsetY / vp.scale / gridScene) * gridScene;
        const endSceneY = Math.ceil((size.h - vp.offsetY) / vp.scale / gridScene) * gridScene;
        for (let sy = startSceneY; sy <= endSceneY; sy += gridScene) {
          const py = sy * vp.scale + vp.offsetY;
          ctx.moveTo(0, py);
          ctx.lineTo(size.w, py);
        }

        ctx.stroke();
      }

      const s = useGeometryStore.getState();
      const ptMap = new Map<string, GeoObject>();
      for (const o of s.objects) { if (o.type === 'point') ptMap.set(o.id, o); }

      // Lines
      for (const o of s.objects) {
        if (o.type !== 'line' || !o.visible) continue;
        const from = ptMap.get(o.startId!); const to = ptMap.get(o.endId!);
        if (!from || !to || from.x == null || from.y == null || to.x == null || to.y == null) continue;
        const sel = s.selectedIds.includes(o.id);
        ctx.save();
        if (sel) { ctx.shadowColor = 'rgba(0,188,212,0.5)'; ctx.shadowBlur = 10; }
        const p1 = toScreen(from.x, from.y);
        const p2 = toScreen(to.x, to.y);
        ctx.beginPath(); ctx.moveTo(p1.x, p1.y); ctx.lineTo(p2.x, p2.y);
        ctx.strokeStyle = sel ? '#00bcd4' : o.color; ctx.lineWidth = sel ? 2.5 : 1.5;
        ctx.setLineDash([8, 6]); ctx.stroke(); ctx.restore();
      }

      // Segments
      for (const o of s.objects) {
        if (o.type !== 'segment' || !o.visible) continue;
        const from = ptMap.get(o.startId!); const to = ptMap.get(o.endId!);
        if (!from || !to || from.x == null || from.y == null || to.x == null || to.y == null) continue;
        const sel = s.selectedIds.includes(o.id);
        ctx.save();
        if (sel) { ctx.shadowColor = 'rgba(0,188,212,0.5)'; ctx.shadowBlur = 10; }
        const p1 = toScreen(from.x, from.y);
        const p2 = toScreen(to.x, to.y);
        ctx.beginPath(); ctx.moveTo(p1.x, p1.y); ctx.lineTo(p2.x, p2.y);
        ctx.strokeStyle = sel ? '#00bcd4' : o.color; ctx.lineWidth = sel ? 3 : 2;
        ctx.setLineDash([]); ctx.stroke();
        // Label
        const mx = (p1.x + p2.x) / 2;
        const my = (p1.y + p2.y) / 2;
        ctx.font = "bold 11px 'Consolas', monospace";
        ctx.fillStyle = sel ? '#00bcd4' : '#8b949e';
        ctx.fillText(o.label + ' ' + (o.length ?? 0).toFixed(0), mx + 6, my - 6);
        ctx.restore();
      }

      // Circles
      for (const o of s.objects) {
        if (o.type !== 'circle' || !o.visible) continue;
        const center = ptMap.get(o.centerId!);
        if (!center || center.x == null || center.y == null) continue;
        const sel = s.selectedIds.includes(o.id);
        const cp = toScreen(center.x, center.y);
        const r = Math.max(0.1, (o.radius ?? 50) * vp.scale);
        ctx.save();
        if (sel) { ctx.shadowColor = 'rgba(0,188,212,0.5)'; ctx.shadowBlur = 10; }
        ctx.beginPath(); ctx.arc(cp.x, cp.y, r, 0, 2 * Math.PI);
        ctx.strokeStyle = sel ? '#00bcd4' : o.color; ctx.lineWidth = sel ? 3 : 2;
        ctx.setLineDash([]); ctx.stroke();
        ctx.font = "bold 11px 'Consolas', monospace";
        ctx.fillStyle = sel ? '#00bcd4' : o.color;
        ctx.fillText(o.label + ' r=' + (o.radius ?? 0).toFixed(0), cp.x + r + 6, cp.y - 4);
        ctx.restore();
      }

      // Points (rendered last, on top)
      for (const o of s.objects) {
        if (o.type !== 'point' || !o.visible || o.x == null || o.y == null) continue;
        const sel = s.selectedIds.includes(o.id);
        const p = toScreen(o.x, o.y);
        if (sel) {
          ctx.save(); ctx.shadowColor = 'rgba(0,188,212,0.6)'; ctx.shadowBlur = 16;
          ctx.beginPath(); ctx.arc(p.x, p.y, 10, 0, 2 * Math.PI);
          ctx.strokeStyle = 'rgba(0,188,212,0.8)'; ctx.lineWidth = 2; ctx.stroke(); ctx.restore();
        }
        ctx.beginPath(); ctx.arc(p.x, p.y, 5, 0, 2 * Math.PI);
        ctx.fillStyle = o.color; ctx.fill();
        ctx.font = "bold 13px 'Consolas', monospace";
        ctx.fillStyle = '#e6edf3';
        ctx.fillText(o.label, p.x + 10, p.y - 10);
      }

      // Hover crosshair
      const h = hover;
      if (h && activeTool !== 'select' && activeTool !== 'hand') {
        const hp = toScreen(h.x, h.y);
        ctx.strokeStyle = 'rgba(255,255,255,0.25)'; ctx.lineWidth = 0.5; ctx.setLineDash([4, 4]);
        ctx.beginPath(); ctx.moveTo(hp.x, 0); ctx.lineTo(hp.x, size.h); ctx.moveTo(0, hp.y); ctx.lineTo(size.w, hp.y);
        ctx.stroke(); ctx.setLineDash([]);
        ctx.font = "11px 'Consolas', monospace"; ctx.fillStyle = 'rgba(255,255,255,0.7)';
        ctx.fillText('(' + Math.round(h.x) + ', ' + Math.round(h.y) + ')', hp.x + 14, hp.y - 10);
      }

      // First-click glow
      if (firstClickId) {
        const fp = s.objects.find((p) => p.id === firstClickId);
        if (fp && fp.x != null && fp.y != null) {
          const fpScreen = toScreen(fp.x, fp.y);
          ctx.save(); ctx.shadowColor = 'rgba(0,188,212,0.6)'; ctx.shadowBlur = 14;
          ctx.beginPath(); ctx.arc(fpScreen.x, fpScreen.y, 12, 0, 2 * Math.PI);
          ctx.strokeStyle = '#00bcd4'; ctx.lineWidth = 2; ctx.stroke(); ctx.restore();
        }
      }

      // Viewport info overlay
      ctx.font = "10px 'Consolas', monospace";
      ctx.fillStyle = 'rgba(255,255,255,0.3)';
      ctx.fillText('zoom: ' + (vp.scale * 100).toFixed(0) + '%  objects: ' + s.objects.length, 8, size.h - 8);

      ctx.restore();
      animId = requestAnimationFrame(render);
    };
    render();
    return () => { alive = false; cancelAnimationFrame(animId); };
  }, [size.w, size.h, dpr, hover, activeTool, firstClickId, objects, viewport, toScreen]);

  return (
    <div ref={containerRef} style={{ position: 'absolute', top: 0, left: 0, right: 0, bottom: 0, overflow: 'hidden' }}>
      <canvas
        ref={ref}
        style={{ display: 'block', background: '#0a0e14', cursor: cursorStyle }}
        onMouseDown={handleMouseDown}
        onMouseMove={handleMouseMove}
        onMouseUp={handleMouseUp}
        onContextMenu={(e) => e.preventDefault()}
      />
    </div>
  );
};
