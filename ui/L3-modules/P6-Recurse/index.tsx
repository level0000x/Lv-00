import React, { useState } from 'react';

/* ── pattern definitions ── */

interface PatternDef {
  id: string;
  name: string;
  nameZh: string;
  description: string;
  input: string;
  rule: string;
  output: string;
}

const PATTERNS: PatternDef[] = [
  {
    id: 'fractal', name: 'Fractal Subdivision', nameZh: '分形细分',
    description: '递归细分几何图形为自相似子结构 Recursively subdivide into self-similar sub-structures',
    input: 'Polygon', rule: 'subdivide(edges, depth)', output: 'Polygon[]',
  },
  {
    id: 'tiling', name: 'Recursive Tiling', nameZh: '递归平铺',
    description: '递归填充平面区域 Recursively fill planar regions with tiles',
    input: 'Region', rule: 'tile(region, motif)', output: 'Region[]',
  },
  {
    id: 'transform', name: 'Self-similar Transform', nameZh: '自相似变换',
    description: '应用相似变换生成递归图形 Apply similarity transforms to generate recursive figures',
    input: 'Shape', rule: 'transform(shape, matrix, depth)', output: 'Shape[]',
  },
  {
    id: 'mutual', name: 'Mutual Recursion', nameZh: '互相递归',
    description: '两个或多个函数互相调用 Two or more functions calling each other',
    input: 'Graph', rule: 'build(nodes) / link(edges)', output: 'Graph',
  },
];

/* ── Inline Slider ── */

const Slider: React.FC<{
  label: string;
  value: number;
  min: number;
  max: number;
  onChange: (v: number) => void;
}> = ({ label, value, min, max, onChange }) => (
  <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
    <span style={{ fontSize: 'var(--font-size-xs, 11px)', color: 'var(--color-text-secondary)', width: 60, flexShrink: 0 }}>
      {label}
    </span>
    <input
      type="range" min={min} max={max} value={value}
      onChange={(e) => onChange(Number(e.target.value))}
      style={{ flex: 1, accentColor: 'var(--color-module-recurse, #ffb74d)' }}
    />
    <span style={{ fontSize: 'var(--font-size-xs, 11px)', color: 'var(--color-text-primary)', fontFamily: 'var(--font-mono)', width: 20, textAlign: 'right' }}>
      {value}
    </span>
  </div>
);

/* ── RecursePanel ── */

export const RecursePanel: React.FC = () => {
  const [depth, setDepth] = useState(3);
  const [running, setRunning] = useState<string | null>(null);

  const runPattern = (id: string) => {
    setRunning(id);
    setTimeout(() => setRunning(null), 1500);
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', gap: 8 }}>
      <span style={{ fontSize: 'var(--font-size-sm, 12px)', color: 'var(--color-text-secondary)' }}>
        递归模式 Recursion Patterns
      </span>

      {/* depth slider */}
      <div style={{
        padding: '6px 8px', borderRadius: 4,
        background: 'var(--color-bg-primary)',
        border: '1px solid var(--color-border-secondary)',
      }}>
        <Slider
          label="深度 Depth"
          value={depth}
          min={1} max={10}
          onChange={setDepth}
        />
      </div>

      {/* pattern list */}
      <div style={{ flex: 1, overflowY: 'auto', display: 'flex', flexDirection: 'column', gap: 6 }}>
        {PATTERNS.map((p) => (
          <div
            key={p.id}
            style={{
              padding: '8px 10px', borderRadius: 6,
              border: '1px solid var(--color-border-secondary)',
              background: 'var(--color-bg-primary)',
            }}
          >
            {/* header */}
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 4 }}>
              <span style={{ fontWeight: 600, fontSize: 'var(--font-size-sm, 12px)', color: 'var(--color-module-recurse, #ffb74d)' }}>
                {p.nameZh} {p.name}
              </span>
              <button
                className="btn btn-small"
                onClick={() => runPattern(p.id)}
                disabled={running === p.id}
                style={{ fontSize: 'var(--font-size-xs, 11px)' }}
              >
                {running === p.id ? '...' : '\u25B6 运行 Run'}
              </button>
            </div>

            {/* description */}
            <div style={{ fontSize: 'var(--font-size-xs, 11px)', color: 'var(--color-text-tertiary, #555)', marginBottom: 6 }}>
              {p.description}
            </div>

            {/* structure */}
            <div style={{
              display: 'flex', alignItems: 'center', gap: 4,
              fontFamily: 'var(--font-mono)', fontSize: 'var(--font-size-xs, 11px)',
            }}>
              <span style={{
                padding: '1px 6px', borderRadius: 3,
                background: '#81c78422', color: '#81c784', border: '1px solid #81c78444',
              }}>
                {p.input}
              </span>
              <span style={{ color: 'var(--color-text-tertiary, #555)' }}>{'\u2192'}</span>
              <span style={{
                padding: '1px 6px', borderRadius: 3,
                background: '#ffb74d22', color: '#ffb74d', border: '1px solid #ffb74d44',
              }}>
                {p.rule}
              </span>
              <span style={{ color: 'var(--color-text-tertiary, #555)' }}>{'\u2192'}</span>
              <span style={{
                padding: '1px 6px', borderRadius: 3,
                background: '#4fc3f722', color: '#4fc3f7', border: '1px solid #4fc3f744',
              }}>
                {p.output}
              </span>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
};
