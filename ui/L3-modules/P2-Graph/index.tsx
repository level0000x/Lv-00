import React, { useState, useCallback } from 'react';

/* ── graph data ── */

interface GNode {
  id: string;
  label: string;
  x: number;
  y: number;
  color: string;
}

interface GEdge {
  from: string;
  to: string;
  style: 'solid' | 'dashed';
}

const DEFAULT_NODES: GNode[] = [
  { id: 'A', label: 'A', x: 180, y: 60, color: '#4fc3f7' },
  { id: 'B', label: 'B', x: 60, y: 180, color: '#81c784' },
  { id: 'C', label: 'C', x: 180, y: 300, color: '#ffb74d' },
  { id: 'D', label: 'D', x: 300, y: 180, color: '#e57373' },
  { id: 'E', label: 'E', x: 100, y: 120, color: '#ba68c8' },
  { id: 'F', label: 'F', x: 260, y: 240, color: '#4dd0e1' },
];

const EDGES: GEdge[] = [
  { from: 'A', to: 'B', style: 'solid' },
  { from: 'B', to: 'C', style: 'solid' },
  { from: 'C', to: 'D', style: 'solid' },
  { from: 'D', to: 'A', style: 'solid' },
  { from: 'A', to: 'C', style: 'dashed' },
  { from: 'B', to: 'D', style: 'dashed' },
  { from: 'E', to: 'A', style: 'solid' },
  { from: 'E', to: 'B', style: 'dashed' },
  { from: 'F', to: 'C', style: 'solid' },
  { from: 'F', to: 'D', style: 'dashed' },
];

/* ── helpers ── */

function connectedComponents(nodes: GNode[], edges: GEdge[]): number {
  const parent: Record<string, string> = {};
  const find = (n: string) => {
    if (parent[n] !== n) parent[n] = find(parent[n]);
    return parent[n];
  };
  nodes.forEach((n) => (parent[n.id] = n.id));
  edges.forEach((e) => {
    const a = find(e.from), b = find(e.to);
    if (a !== b) parent[a] = b;
  });
  const roots = new Set<string>();
  nodes.forEach((n) => roots.add(find(n.id)));
  return roots.size;
}

/* ── component ── */

export const GraphPanel: React.FC = () => {
  const [nodes, setNodes] = useState<GNode[]>(DEFAULT_NODES);

  const handleMouseDown = useCallback((id: string) => {
    const startPos = { x: 0, y: 0 };
    const handleMove = (ev: MouseEvent) => {
      const rect = (ev.target as SVGElement).closest('svg')!.getBoundingClientRect();
      const x = ev.clientX - rect.left;
      const y = ev.clientY - rect.top;
      startPos.x = x;
      startPos.y = y;
      setNodes((prev) => prev.map((n) => (n.id === id ? { ...n, x, y } : n)));
    };
    const handleUp = () => {
      window.removeEventListener('mousemove', handleMove);
      window.removeEventListener('mouseup', handleUp);
    };
    window.addEventListener('mousemove', handleMove);
    window.addEventListener('mouseup', handleUp);
  }, []);

  const solidEdges = EDGES.filter((e) => e.style === 'solid');
  const dashedEdges = EDGES.filter((e) => e.style === 'dashed');
  const components = connectedComponents(nodes, EDGES);

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
              fillOpacity={0.2}
              stroke={n.color}
              strokeWidth={2}
              style={{ cursor: 'grab' }}
              onMouseDown={() => handleMouseDown(n.id)}
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
          线段 Segment
        </span>
        <span>
          <span style={{ display: 'inline-block', width: 16, height: 0, borderTop: '2px dashed var(--color-border-primary, #444)', verticalAlign: 'middle', marginRight: 4 }} />
          依赖 Dependency
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
        <Stat label="边 Edges" value={String(EDGES.length)} />
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
