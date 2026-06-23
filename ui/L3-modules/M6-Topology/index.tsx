import React, { useState } from 'react';
import { TopoBlock, TopoEdge } from '../types';
import { Empty } from '../shared';

interface TopologyViewProps {
  blocks: TopoBlock[];
  edges: TopoEdge[];
  onBlockMove?: (blockId: number, x: number, y: number) => void;
}

const BW = 160;
const BH = 80;

export const TopologyView: React.FC<TopologyViewProps> = ({ blocks, edges, onBlockMove }) => {
  const [dragId, setDragId] = useState<number | null>(null);
  const [dragOff, setDragOff] = useState({ x: 0, y: 0 });
  const [layout, setLayout] = useState<Map<number, { x: number; y: number }>>(new Map(
    blocks.map((b) => [b.id, { x: b.layoutX, y: b.layoutY }])
  ));

  if (!blocks.length) return <Empty msg="No function blocks" icon={'\uD83D\uDD32'} />;

  const mx = Math.max(...blocks.map((b) => (layout.get(b.id)?.x ?? b.layoutX) + BW), 600);
  const my = Math.max(...blocks.map((b) => (layout.get(b.id)?.y ?? b.layoutY) + BH), 400);

  const moveBlock = (id: number, x: number, y: number) => {
    setLayout((prev) => {
      const next = new Map(prev);
      next.set(id, { x: Math.round(x / 20) * 20, y: Math.round(y / 20) * 20 });
      return next;
    });
  };

  return (
    <div style={{ width: '100%', height: '100%', overflow: 'auto' }}>
      <svg
        width={mx + 100}
        height={my + 100}
        style={{ minWidth: '100%', minHeight: '100%' }}
        onMouseMove={(e) => {
          if (dragId === null) return;
          const svg = (e.currentTarget as SVGSVGElement).closest('div')?.getBoundingClientRect();
          if (!svg) return;
          const x = (e.clientX - svg.left) - dragOff.x;
          const y = (e.clientY - svg.top) - dragOff.y;
          moveBlock(dragId, x, y);
        }}
        onMouseUp={() => {
          if (dragId !== null && layout.has(dragId)) {
            const pos = layout.get(dragId)!;
            onBlockMove?.(dragId, pos.x, pos.y);
          }
          setDragId(null);
        }}
      >
        <defs>
          <pattern id="grid" width="30" height="30" patternUnits="userSpaceOnUse">
            <path d="M 30 0 L 0 0 0 30" fill="none" stroke="var(--color-border-secondary)" strokeWidth="0.5" />
          </pattern>
        </defs>
        <rect width="100%" height="100%" fill="url(#grid)" />

        {edges.map((e) => {
          const f = blocks.find((b) => b.id === e.fromBlock);
          const t = blocks.find((b) => b.id === e.toBlock);
          if (!f || !t) return null;
          const fPos = layout.get(f.id) ?? { x: f.layoutX, y: f.layoutY };
          const tPos = layout.get(t.id) ?? { x: t.layoutX, y: t.layoutY };
          const x1 = fPos.x + BW;
          const y1 = fPos.y + BH / 2;
          const x2 = tPos.x;
          const y2 = tPos.y + BH / 2;
          const midX = (x1 + x2) / 2;
          return (
            <path
              key={`e-${e.fromBlock}-${e.toBlock}`}
              d={`M${x1},${y1} C${midX},${y1} ${midX},${y2} ${x2},${y2}`}
              fill="none"
              stroke="var(--color-accent)"
              strokeWidth={2}
              opacity={0.5}
            />
          );
        })}

        {blocks.map((b) => {
          const pos = layout.get(b.id) ?? { x: b.layoutX, y: b.layoutY };
          return (
            <g
              key={b.id}
              onMouseDown={(e) => {
                const svg = (e.currentTarget as SVGGElement).closest('div')?.getBoundingClientRect();
                if (!svg) return;
                setDragId(b.id);
                setDragOff({ x: e.clientX - svg.left - pos.x, y: e.clientY - svg.top - pos.y });
              }}
              style={{ cursor: 'grab' }}
            >
              <rect
                x={pos.x}
                y={pos.y}
                width={BW}
                height={BH}
                rx={10}
                fill="var(--color-bg-secondary)"
                stroke={dragId === b.id ? 'var(--color-warning)' : 'var(--color-accent)'}
                strokeWidth={2}
              />
              <text
                x={pos.x + BW / 2}
                y={pos.y + 22}
                textAnchor="middle"
                fill="var(--color-text-primary)"
                fontSize={13}
                fontWeight="bold"
              >
                {b.name}
              </text>
              {b.inputs.map((p, i) => (
                <g key={p.id}>
                  <circle
                    cx={pos.x}
                    cy={pos.y + 38 + i * 16}
                    r={5}
                    fill="var(--color-success)"
                    stroke="var(--color-border-primary)"
                  />
                  <text
                    x={pos.x + 10}
                    y={pos.y + 42 + i * 16}
                    fill="var(--color-text-secondary)"
                    fontSize={10}
                  >
                    {p.name}
                  </text>
                </g>
              ))}
              {b.outputs.map((p, i) => (
                <g key={p.id}>
                  <circle
                    cx={pos.x + BW}
                    cy={pos.y + 38 + i * 16}
                    r={5}
                    fill="var(--color-warning)"
                    stroke="var(--color-border-primary)"
                  />
                  <text
                    x={pos.x + BW - 10}
                    y={pos.y + 42 + i * 16}
                    textAnchor="end"
                    fill="var(--color-text-secondary)"
                    fontSize={10}
                  >
                    {p.name}
                  </text>
                </g>
              ))}
              <text
                x={pos.x + BW / 2}
                y={pos.y + BH - 8}
                textAnchor="middle"
                fill="var(--color-text-secondary)"
                fontSize={9}
              >
                {b.inputs.length} in / {b.outputs.length} out
              </text>
            </g>
          );
        })}
      </svg>
    </div>
  );
};
