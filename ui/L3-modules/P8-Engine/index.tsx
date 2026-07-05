import React, { useState, useMemo } from 'react';
import { useGeometryStore } from '../../L5-core/store/geometryStore';

/* ── types ── */

type EngineState = 'idle' | 'running' | 'complete';

/* ── helpers ── */

const ProgressBar: React.FC<{ label: string; value: number; max?: number; color?: string }> = ({
  label, value, max = 100, color = 'var(--color-module-engine, #4dd0e1)',
}) => {
  const pct = Math.min(100, Math.round((value / max) * 100));
  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: 2 }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: 'var(--font-size-xs, 11px)' }}>
        <span style={{ color: 'var(--color-text-tertiary, #555)' }}>{label}</span>
        <span style={{ color: 'var(--color-text-secondary)', fontFamily: 'var(--font-mono)' }}>{pct}%</span>
      </div>
      <div style={{
        height: 4, borderRadius: 2,
        background: 'var(--color-bg-secondary, #1a1a2e)',
        overflow: 'hidden',
      }}>
        <div style={{
          height: '100%', borderRadius: 2,
          width: `${pct}%`,
          background: color,
          transition: 'width 0.3s ease',
        }} />
      </div>
    </div>
  );
};

const StatCard: React.FC<{ label: string; value: string | number; accent?: boolean }> = ({
  label, value, accent = false,
}) => (
  <div style={{
    display: 'flex', flexDirection: 'column', gap: 2,
    padding: '6px 8px', borderRadius: 4,
    background: 'var(--color-bg-primary)',
    border: '1px solid var(--color-border-secondary)',
  }}>
    <span style={{ fontSize: 'var(--font-size-xs, 10px)', color: 'var(--color-text-tertiary, #555)', textTransform: 'uppercase' }}>
      {label}
    </span>
    <span style={{
      fontSize: 'var(--font-size-md, 14px)',
      fontWeight: 700,
      fontFamily: 'var(--font-mono)',
      color: accent ? 'var(--color-module-engine, #4dd0e1)' : 'var(--color-text-primary)',
    }}>
      {value}
    </span>
  </div>
);

/* ── EnginePanel ── */

export const EnginePanel: React.FC = () => {
  const objects = useGeometryStore((s) => s.objects);
  const constraints = useGeometryStore((s) => s.constraints);
  const [engineState, setEngineState] = useState<EngineState>('idle');
  const [backend, setBackend] = useState<'wasm-rs' | 'js' | 'none'>('none');

  const statusLabel =
    engineState === 'idle' ? '\u23F3 空闲 Idle' :
    engineState === 'running' ? '\u25B6 运行中 Running' :
    '\u2713 完成 Complete';

  const statusColor =
    engineState === 'idle' ? 'var(--color-text-tertiary, #555)' :
    engineState === 'running' ? 'var(--color-accent, #4fc3f7)' :
    'var(--color-success, #81c784)';

  // Real statistics from store
  const stats = useMemo(() => {
    const counts: Record<string, number> = {};
    objects.forEach((o) => { counts[o.type] = (counts[o.type] || 0) + 1; });
    // Rough estimate: ~200 bytes per object (id, type, label, color, coords, etc.)
    const approxBytes = objects.length * 200;
    const approxKB = (approxBytes / 1024).toFixed(1);

    return {
      total: objects.length,
      points: counts['point'] || 0,
      segments: counts['segment'] || 0,
      circles: counts['circle'] || 0,
      lines: counts['line'] || 0,
      rays: counts['ray'] || 0,
      polygons: counts['polygon'] || 0,
      arcs: counts['arc'] || 0,
      angles: counts['angle'] || 0,
      constraintCount: constraints.length,
      storeSize: approxKB,
      nonPointCount: objects.filter((o) => o.type !== 'point').length,
    };
  }, [objects, constraints]);

  const runDemo = () => {
    useGeometryStore.getState().loadDemoScene();
    setEngineState('running');
    setTimeout(() => setEngineState('complete'), 2000);
  };

  const section = (title: string, children: React.ReactNode) => (
    <div style={{ marginBottom: 10 }}>
      <div style={{
        fontSize: 'var(--font-size-xs, 11px)', fontWeight: 600,
        color: 'var(--color-module-engine, #4dd0e1)', marginBottom: 6,
        textTransform: 'uppercase', letterSpacing: '0.5px',
      }}>
        {title}
      </div>
      {children}
    </div>
  );

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', gap: 8, overflowY: 'auto' }}>
      {/* state indicator + controls */}
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
          <span style={{ width: 8, height: 8, borderRadius: '50%', background: statusColor }} />
          <span style={{ fontSize: 'var(--font-size-sm, 12px)', color: statusColor, fontWeight: 600 }}>
            {statusLabel}
          </span>
        </div>
        <button className="btn btn-small" onClick={runDemo} disabled={engineState === 'running'}>
          {engineState === 'running' ? '...' : '\u25B6 运行 Run'}
        </button>
      </div>

      {/* backend selector */}
      {section('\u540E\u7AEF Backend', (
        <div style={{ display: 'flex', gap: 4 }}>
          {(['wasm-rs', 'js', 'none'] as const).map((b) => (
            <button
              key={b}
              onClick={() => setBackend(b)}
              style={{
                flex: 1, padding: '4px 0', borderRadius: 4,
                border: `1px solid ${backend === b ? 'var(--color-module-engine, #4dd0e1)' : 'var(--color-border-secondary)'}`,
                background: backend === b ? 'var(--color-module-engine, #4dd0e1)' : 'transparent',
                color: backend === b ? '#000' : 'var(--color-text-secondary)',
                cursor: 'pointer', fontSize: 'var(--font-size-xs, 11px)',
                fontFamily: 'var(--font-mono)', fontWeight: 500,
                transition: 'all 0.15s',
              }}
            >
              {b}
            </button>
          ))}
        </div>
      ))}

      {/* stats grid - real data from store */}
      {section('\u7EDF\u8BA1 Statistics', (
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: 6 }}>
          <StatCard label="总对象 Total" value={stats.total} accent />
          <StatCard label="点 Points" value={stats.points} />
          <StatCard label="线段 Segments" value={stats.segments} />
          <StatCard label="圆 Circles" value={stats.circles} />
          <StatCard label="直线 Lines" value={stats.lines} />
          <StatCard label="射线 Rays" value={stats.rays} />
          <StatCard label="多边形 Polygons" value={stats.polygons} />
          <StatCard label="弧 Arcs" value={stats.arcs} />
          <StatCard label="约束 Constraints" value={stats.constraintCount} />
        </div>
      ))}

      {/* performance */}
      {section('\u6027\u80FD Performance', (
        <div style={{ display: 'flex', flexDirection: 'column', gap: 6 }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: 'var(--font-size-xs, 11px)' }}>
            <span style={{ color: 'var(--color-text-tertiary, #555)' }}>构造对象 Constructions</span>
            <span style={{ fontFamily: 'var(--font-mono)', color: 'var(--color-text-primary)' }}>{stats.nonPointCount}</span>
          </div>
          <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: 'var(--font-size-xs, 11px)' }}>
            <span style={{ color: 'var(--color-text-tertiary, #555)' }}>存储估算 Store size</span>
            <span style={{ fontFamily: 'var(--font-mono)', color: 'var(--color-text-primary)' }}>{stats.storeSize} KB</span>
          </div>
          <ProgressBar label="存储占用 Storage" value={Math.min(100, objects.length * 5)} color="#4dd0e1" />
          <ProgressBar label="约束完成 Constraint" value={stats.constraintCount > 0 ? 100 : 0} color="#81c784" />
        </div>
      ))}
    </div>
  );
};
