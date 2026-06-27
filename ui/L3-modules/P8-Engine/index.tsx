import React, { useState } from 'react';

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

  const runDemo = () => {
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

      {/* stats grid */}
      {section('\u7EDF\u8BA1 Statistics', (
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: 6 }}>
          <StatCard label="节点 Nodes" value={6} />
          <StatCard label="约束 Constraints" value={10} />
          <StatCard label="证明 Proofs" value={2} accent />
          <StatCard label="函数块 Blocks" value={5} />
          <StatCard label="快照 Snapshots" value={3} />
          <StatCard label="撤销深度 Undo" value={12} />
        </div>
      ))}

      {/* performance */}
      {section('\u6027\u80FD Performance', (
        <div style={{ display: 'flex', flexDirection: 'column', gap: 6 }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: 'var(--font-size-xs, 11px)' }}>
            <span style={{ color: 'var(--color-text-tertiary, #555)' }}>上次求解时间 Last solve</span>
            <span style={{ fontFamily: 'var(--font-mono)', color: 'var(--color-text-primary)' }}>1.23 ms</span>
          </div>
          <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: 'var(--font-size-xs, 11px)' }}>
            <span style={{ color: 'var(--color-text-tertiary, #555)' }}>内存 Memory</span>
            <span style={{ fontFamily: 'var(--font-mono)', color: 'var(--color-text-primary)' }}>4.2 MB</span>
          </div>
          <ProgressBar label="内存占用 Memory" value={42} color="#4dd0e1" />
          <ProgressBar label="证明完成 Proof completion" value={engineState === 'complete' ? 100 : engineState === 'running' ? 67 : 0} color="#81c784" />
        </div>
      ))}
    </div>
  );
};
