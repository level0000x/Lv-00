import React, { useState } from 'react';

/* ── type hierarchy ── */

interface TypeNode {
  name: string;
  nameZh: string;
  description: string;
  fields: { name: string; type: string }[];
  children: TypeNode[];
}

const TYPE_TREE: TypeNode[] = [
  {
    name: 'Point', nameZh: '点',
    description: '平面上的一个位置 A position on the plane',
    fields: [{ name: 'x', type: 'f64' }, { name: 'y', type: 'f64' }],
    children: [],
  },
  {
    name: 'Line', nameZh: '线',
    description: '无限延伸的直线 An infinitely extending line',
    fields: [{ name: 'origin', type: 'Point' }, { name: 'direction', type: 'Vec2' }],
    children: [
      {
        name: 'Segment', nameZh: '线段',
        description: '两个端点间的线段 Line segment between two endpoints',
        fields: [{ name: 'start', type: 'Point' }, { name: 'end', type: 'Point' }],
        children: [],
      },
      {
        name: 'Ray', nameZh: '射线',
        description: '从一个点出发的射线 Ray from a point',
        fields: [{ name: 'origin', type: 'Point' }, { name: 'direction', type: 'Vec2' }],
        children: [],
      },
    ],
  },
  {
    name: 'Circle', nameZh: '圆',
    description: '以圆心和半径定义的圆 Circle defined by center and radius',
    fields: [{ name: 'center', type: 'Point' }, { name: 'radius', type: 'f64' }],
    children: [
      {
        name: 'Arc', nameZh: '弧',
        description: '圆的一部分 A portion of a circle',
        fields: [{ name: 'circle', type: 'Circle' }, { name: 'startAngle', type: 'f64' }, { name: 'endAngle', type: 'f64' }],
        children: [],
      },
    ],
  },
  {
    name: 'Polygon', nameZh: '多边形',
    description: '由多个顶点围成的封闭区域 A closed region bounded by vertices',
    fields: [{ name: 'vertices', type: 'Point[]' }],
    children: [
      {
        name: 'Triangle', nameZh: '三角形',
        description: '三条边围成的多边形 Polygon with three edges',
        fields: [{ name: 'A', type: 'Point' }, { name: 'B', type: 'Point' }, { name: 'C', type: 'Point' }],
        children: [],
      },
      {
        name: 'Rectangle', nameZh: '矩形',
        description: '四个直角的多边形 Polygon with four right angles',
        fields: [{ name: 'origin', type: 'Point' }, { name: 'width', type: 'f64' }, { name: 'height', type: 'f64' }],
        children: [],
      },
    ],
  },
  {
    name: 'Angle', nameZh: '角',
    description: '由两条射线形成的角 Angle formed by two rays',
    fields: [{ name: 'vertex', type: 'Point' }, { name: 'arm1', type: 'Ray' }, { name: 'arm2', type: 'Ray' }],
    children: [],
  },
  {
    name: 'Region', nameZh: '区域',
    description: '平面上的有界或无界区域 A bounded or unbounded region on the plane',
    fields: [{ name: 'boundary', type: 'Curve[]' }, { name: 'interior', type: 'Set<Point>' }],
    children: [],
  },
];

/* ── TypeNodeRow ── */

const TypeNodeRow: React.FC<{
  node: TypeNode;
  depth: number;
  expanded: boolean;
  onToggle: () => void;
}> = ({ node, depth, expanded, onToggle }) => {
  const hasChildren = node.children.length > 0;
  const chevron = hasChildren ? (expanded ? '\u25BC' : '\u25B6') : '\u2022';

  return (
    <div>
      {/* type row */}
      <div
        onClick={hasChildren ? onToggle : undefined}
        style={{
          display: 'flex', alignItems: 'center', gap: 6,
          padding: '3px 0', cursor: hasChildren ? 'pointer' : 'default',
          paddingLeft: depth * 16,
          color: 'var(--color-text-primary)',
        }}
      >
        <span style={{ fontSize: 'var(--font-size-xs, 10px)', width: 12, textAlign: 'center', color: 'var(--color-text-tertiary, #555)' }}>
          {chevron}
        </span>
        <span style={{ fontWeight: 600, fontSize: 'var(--font-size-sm, 12px)', fontFamily: 'var(--font-mono)', color: 'var(--color-module-type, #81c784)' }}>
          {node.name}
        </span>
        <span style={{ fontSize: 'var(--font-size-sm, 12px)', color: 'var(--color-text-secondary)' }}>
          {node.nameZh}
        </span>
      </div>

      {/* expanded: description + fields */}
      {expanded && (
        <div style={{ paddingLeft: (depth + 1) * 16 + 18, marginBottom: 6 }}>
          <div style={{ fontSize: 'var(--font-size-xs, 11px)', color: 'var(--color-text-tertiary, #555)', marginBottom: 4 }}>
            {node.description}
          </div>
          {node.fields.length > 0 && (
            <div style={{
              display: 'flex', flexDirection: 'column', gap: 1,
              fontFamily: 'var(--font-mono)', fontSize: 'var(--font-size-xs, 11px)',
            }}>
              {node.fields.map((f) => (
                <span key={f.name}>
                  <span style={{ color: 'var(--color-accent, #4fc3f7)' }}>{f.name}</span>
                  <span style={{ color: 'var(--color-text-tertiary, #555)' }}>{' : '}</span>
                  <span style={{ color: 'var(--color-text-secondary)' }}>{f.type}</span>
                </span>
              ))}
            </div>
          )}
          {/* recurse children */}
          {node.children.map((child) => (
            <TypeNodeRowWrapper key={child.name} node={child} depth={depth + 1} />
          ))}
        </div>
      )}
    </div>
  );
};

/* wrapper with its own expanded state */
const TypeNodeRowWrapper: React.FC<{ node: TypeNode; depth: number }> = ({ node, depth }) => {
  const [expanded, setExpanded] = useState(false);
  return (
    <TypeNodeRow node={node} depth={depth} expanded={expanded} onToggle={() => setExpanded((v) => !v)} />
  );
};

/* ── TypePanel ── */

export const TypePanel: React.FC = () => (
  <div style={{ display: 'flex', flexDirection: 'column', height: '100%', gap: 8 }}>
    <span style={{ fontSize: 'var(--font-size-sm, 12px)', color: 'var(--color-text-secondary)' }}>
      类型浏览 Type Explorer
    </span>
    <div style={{ flex: 1, overflowY: 'auto', display: 'flex', flexDirection: 'column', gap: 2 }}>
      {TYPE_TREE.map((node) => (
        <TypeNodeRowWrapper key={node.name} node={node} depth={0} />
      ))}
    </div>
  </div>
);
