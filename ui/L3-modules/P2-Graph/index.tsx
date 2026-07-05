import React, { useCallback, useMemo } from 'react';
import { useGeometryStore, GeoObject } from '../../L5-core/store/geometryStore';

/* ── graph data ── */

interface GNode {
  id: string;
  label: string;
  x: number;
  y: number;
  color: string;
  storeId: string;
}

interface GEdge {
  from: string;
  to: string;
  style: 'solid' | 'dashed';
  label: string;
}

/* ── helpers ── */

function connectedComponents(nodes: GNode[], edges: GEdge[]): number {
  if (nodes.length === 0) return 0;
  const parent: Record<string, string> = {};
  const find = (n: string) => {
    if (parent[n] !== n) parent[n] = find(parent[n]);
    return parent[n];
  };
  nodes.forEach((n) => (parent[n.id] = n.id));
  edges.forEach((e) => {
    if (parent[e.from] && parent[e.to]) {
      const a = find(e.from), b = find(e.to);
      if (a !== b) parent[a] = b;
    }
  });
  const roots = new Set<string>();
  nodes.forEach((n) => roots.add(find(n.id)));
  return roots.size;
}

// Scale geometry coordinates to fit SVG viewport
function scaleToViewport(x: number, y: number, objects: GeoObject[]): { x: number; y: number } {
  if (objects.length === 0) return { x: 180, y: 180 };
  const points = objects.filter((o) => o.type === 'point' && o.x !== undefined && o.y !== undefined);
  if (points.length === 0) return { x: 180, y: 180 };

  const xs = points.map((p) => p.x!);
  const ys = points.map((p) => p.y!);
  const minX = Math.min(...xs), maxX = Math.max(...xs);
  const minY = Math.min(...ys), maxY = Math.max(...ys);

  const rangeX = maxX - minX || 1;
  const rangeY = maxY - minY || 1;
  const padding = 40;
  const viewW = 360 - padding * 2;
  const viewH = 360 - padding * 2;

  return {
    x: padding + ((x - minX) / rangeX) * viewW,
    y: padding + ((y - minY) / rangeY) * viewH,
  };
}

/* ── component ── */

export const GraphPanel: React.FC = () => {
  const objects = useGeometryStore((s) => s.objects);
  const selectedIds = useGeometryStore((s) => s.selectedIds);
  const selectObjects = useGeometryStore((s) => s.selectObjects);

  // Derive nodes from points
  const nodes: GNode[] = useMemo(() => {
    const points = objects.filter((o) => o.type === 'point' && o.x !== undefined && o.y !== undefined);
    return points.map((p) => {
      const scaled = scaleToViewport(p.x!, p.y!, objects);
      return {
        id: p.label,
        label: p.label,
        x: scaled.x,
        y: scaled.y,
        color: p.color,
        storeId: p.id,
      };
    });
  }, [objects]);

  // Derive edges from segments and circles
  const edges: GEdge[] = useMemo(() => {
    const result: GEdge[] = [];
    for (const o of objects) {
      if (o.type === 'segment' && o.startId && o.endId) {
        const startPt = objects.find((p) => p.id === o.startId);
        const endPt = objects.find((p) => p.id === o.endId);
        if (startPt && endPt) {
          result.push({
            from: startPt.label,
            to: endPt.label,
            style: 'solid',
            label: o.label,
          });
        }
      }
      if (o.type === 'circle' && o.centerId && o.radiusPointId) {
        const center = objects.find((p) => p.id === o.centerId);
        const rp = objects.find((p) => p.id === o.radiusPointId);
        if (center && rp) {
          result.push({
            from: center.label,
            to: rp.label,
            style: 'dashed',
            label: o.label,
          });
        }
      }
      if (o.type === 'line' && o.startId && o.endId) {
        const p1 = objects.find((p) => p.id === o.startId);
        const p2 = objects.find((p) => p.id === o.endId);
        if (p1 && p2) {
          result.push({ from: p1.label, to: p2.label, style: 'solid', label: o.label });
        }
      }
      if (o.type === 'ray' && o.startId && o.endId) {
        const p1 = objects.find((p) => p.id === o.startId);
        const p2 = objects.find((p) => p.id === o.endId);
        if (p1 && p2) {
          result.push({ from: p1.label, to: p2.label, style: 'dashed', label: o.label });
        }
      }
    }
    return result;
  }, [objects]);

  const handleMouseDown = useCallback((storeId: string) => {
    selectObjects([storeId]);
  }, [selectObjects]);

  const solidEdges = edges.filter((e) => e.style === 'solid');
  const dashedEdges = edges.filter((e) => e.style === 'dashed');
  const components = connectedComponents(nodes, edges);

  const nodeMap = Object.fromEntries(nodes.map((n) => [n.id, n]));

  const line = (e: GEdge) => {
    const a = nodeMap[e.from], b = nodeMap[e.to];
    if (!a || !b) return null;
    return (
      <line
        key={`${e.from}-${e.to}-${e.style}`}
        x1={a.x} y1={a.y} x2={b.x} y2={b.y}
        stroke="var(--color-border-primary, #444)"
        strokeWidth={e.style === 'solid' ? 1.5 : 1}
        strokeDasharray={e.style === 'dashed' ? '4 3' : undefined}
        opacity={0.7}
      />
    );
  };

  const emptyState = objects.length === 0;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', gap: 8 }}>
      <span style={{ fontSize: 'var(--font-size-sm, 12px)', color: 'var(--color-text-secondary)' }}>
        约束图浏览 Constraint Graph
      </span>

      {/* SVG viewport */}
      <svg
        width="100%"
        height="100%"
        viewBox="0 0 360 360"
        style={{
          flex: 1,
          background: 'var(--color-bg-primary)',
          borderRadius: 6,
          border: '1px solid var(--color-border-secondary)',
          userSelect: 'none',
        }}
      >
        {emptyState && (
          <text
            x={180} y={180}
            textAnchor="middle" dominantBaseline="middle"
            fill="var(--color-text-tertiary, #555)"
            fontSize={12}
            fontFamily="var(--font-mono)"
          >
            暂无对象 No objects
          </text>
        )}
        {/* edges: solid */}
        {solidEdges.map(line)}
        {/* edges: dashed */}
        {dashedEdges.map(line)}
        {/* nodes */}
        {nodes.map((n) => (
          <g key={n.id}>
            <circle
              cx={n.x} cy={n.y} r={16}
              fill={n.color}
              fillOpacity={selectedIds.includes(n.storeId) ? 0.5 : 0.2}
              stroke={n.color}
              strokeWidth={selectedIds.includes(n.storeId) ? 3 : 2}
              style={{ cursor: 'pointer' }}
              onClick={() => handleMouseDown(n.storeId)}
            />
            <text
              x={n.x} y={n.y + 1}
              textAnchor="middle" dominantBaseline="middle"
              fill={n.color}
              fontSize={12}
              fontWeight={600}
              fontFamily="var(--font-mono)"
              style={{ pointerEvents: 'none' }}
            >
              {n.label}
            </text>
          </g>
        ))}
      </svg>

      {/* legend */}
      <div style={{ display: 'flex', gap: 12, fontSize: 'var(--font-size-xs, 11px)', color: 'var(--color-text-secondary)' }}>
        <span>
          <span style={{ display: 'inline-block', width: 16, height: 2, background: 'var(--color-border-primary, #444)', verticalAlign: 'middle', marginRight: 4 }} />
          线段/直线 Segment/Line
        </span>
        <span>
          <span style={{ display: 'inline-block', width: 16, height: 0, borderTop: '2px dashed var(--color-border-primary, #444)', verticalAlign: 'middle', marginRight: 4 }} />
          圆/射线 Circle/Ray
        </span>
      </div>

      {/* stats */}
      <div
        style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(3, 1fr)',
          gap: 6,
          padding: 8,
          background: 'var(--color-bg-primary)',
          borderRadius: 6,
          border: '1px solid var(--color-border-secondary)',
          fontSize: 'var(--font-size-xs, 11px)',
        }}
      >
        <Stat label="节点 Nodes" value={String(nodes.length)} />
        <Stat label="边 Edges" value={String(edges.length)} />
        <Stat label="连通分量 Components" value={String(components)} />
      </div>
    </div>
  );
};

const Stat: React.FC<{ label: string; value: string }> = ({ label, value }) => (
  <div style={{ display: 'flex', flexDirection: 'column', gap: 2 }}>
    <span style={{ color: 'var(--color-text-tertiary, #555)' }}>{label}</span>
    <span style={{ color: 'var(--color-text-primary)', fontWeight: 600, fontFamily: 'var(--font-mono)' }}>{value}</span>
  </div>
);
