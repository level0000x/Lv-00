import React, { useState } from 'react';

/* ── types ── */

interface ProofStep {
  id: number;
  tactic: string;
  result: string;
}

interface TacticDef {
  name: string;
  description: string;
  color: string;
}

const TACTICS: TacticDef[] = [
  { name: 'intro', description: '引入假设 Introduce hypothesis', color: '#4fc3f7' },
  { name: 'apply', description: '应用引理 Apply lemma', color: '#81c784' },
  { name: 'rewrite', description: '重写目标 Rewrite goal', color: '#ffb74d' },
  { name: 'exact', description: '精确匹配 Exact match', color: '#ba68c8' },
  { name: 'sorry', description: '跳过（占位） Admit (placeholder)', color: '#e57373' },
];

const HYPOTHESES = [
  { label: 'h\u2081', text: 'AB \u2261 AC (A\u662F\u7B49\u8170\u4E09\u89D2\u5F62\u9876\u70B9 A is apex of isosceles triangle)' },
  { label: 'h\u2082', text: 'D = midpoint(B, C)' },
  { label: 'h\u2083', text: '\u2220BAD = \u2220CAD' },
];

/* ── ProofPanel ── */

export const ProofPanel: React.FC = () => {
  const [goal] = useState(
    '\u2220BAD = \u2220CAD \u2192 AD \u22A5 BC  (\u7B49\u8170\u4E09\u89D2\u5F62\u5E95\u8FB9\u4E2D\u7EBF\u5782\u76F4\u4E8E\u5E95\u8FB9 Midline of isosceles \u22A5 base)'
  );
  const [steps, setSteps] = useState<ProofStep[]>([
    { id: 1, tactic: 'intro h\u2081', result: 'h\u2081 : AB \u2261 AC \u22A8 \u2220BAD = \u2220CAD \u2192 AD \u22A5 BC' },
  ]);
  const [status, setStatus] = useState<'proved' | 'pending' | 'in-progress'>('in-progress');

  const applyTactic = (t: TacticDef) => {
    const newStep: ProofStep = {
      id: steps.length + 1,
      tactic: t.name,
      result: t.name === 'sorry'
        ? '\u22A8 \u2713 (admitted)'
        : `\u22A8 ${t.name} ${String.fromCharCode(104 + steps.length)} ...`,
    };
    setSteps((prev) => [...prev, newStep]);
    if (t.name === 'exact') setStatus('proved');
  };

  const statusLabel =
    status === 'proved' ? '\u2713 \u5DF2\u8BC1\u660E Proved' :
    status === 'pending' ? '\u23F3 \u5F85\u5904\u7406 Pending' :
    '\u25B6 \u8BC1\u660E\u4E2D In Progress';

  const statusColor =
    status === 'proved' ? 'var(--color-success, #81c784)' :
    status === 'pending' ? 'var(--color-text-tertiary, #555)' :
    'var(--color-accent, #4fc3f7)';

  /* ── render ── */
  const section = (title: string, children: React.ReactNode) => (
    <div style={{ marginBottom: 10 }}>
      <div style={{
        fontSize: 'var(--font-size-xs, 11px)', fontWeight: 600,
        color: 'var(--color-module-proof, #ba68c8)', marginBottom: 4,
        textTransform: 'uppercase', letterSpacing: '0.5px',
      }}>
        {title}
      </div>
      {children}
    </div>
  );

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', gap: 8, overflowY: 'auto' }}>
      {/* status */}
      <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
        <span style={{ width: 8, height: 8, borderRadius: '50%', background: statusColor }} />
        <span style={{ fontSize: 'var(--font-size-sm, 12px)', color: statusColor, fontWeight: 600 }}>
          {statusLabel}
        </span>
      </div>

      {/* current goal */}
      {section('\u5F53\u524D\u76EE\u6807 Current Goal', (
        <div style={{
          padding: '6px 8px', borderRadius: 4,
          background: 'var(--color-bg-secondary, #1a1a2e)',
          border: '1px solid var(--color-border-secondary)',
          fontFamily: 'var(--font-mono)', fontSize: 'var(--font-size-xs, 11px)',
          color: 'var(--color-text-primary)', lineHeight: 1.5,
        }}>
          {goal}
        </div>
      ))}

      {/* context / hypotheses */}
      {section('\u4E0A\u4E0B\u6587 Context', (
        <div style={{ display: 'flex', flexDirection: 'column', gap: 3 }}>
          {HYPOTHESES.map((h) => (
            <div key={h.label} style={{
              fontSize: 'var(--font-size-xs, 11px)', fontFamily: 'var(--font-mono)',
              color: 'var(--color-text-secondary)',
              padding: '2px 0',
            }}>
              <span style={{ color: 'var(--color-module-proof, #ba68c8)', fontWeight: 600 }}>{h.label}</span>
              {' : '}{h.text}
            </div>
          ))}
        </div>
      ))}

      {/* tactics */}
      {section('\u7B56\u7565 Tactics', (
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: 4 }}>
          {TACTICS.map((t) => (
            <button
              key={t.name}
              onClick={() => applyTactic(t)}
              title={t.description}
              style={{
                padding: '3px 8px', borderRadius: 4,
                border: `1px solid ${t.color}44`,
                background: `${t.color}15`,
                color: t.color,
                cursor: 'pointer', fontSize: 'var(--font-size-xs, 11px)',
                fontFamily: 'var(--font-mono)', fontWeight: 500,
              }}
            >
              {t.name}
            </button>
          ))}
        </div>
      ))}

      {/* proof steps */}
      {section('\u8BC1\u660E\u6B65\u9AA4 Proof Steps', (
        <div style={{
          padding: '6px 8px', borderRadius: 4,
          background: 'var(--color-bg-primary)',
          border: '1px solid var(--color-border-secondary)',
          fontFamily: 'var(--font-mono)', fontSize: 'var(--font-size-xs, 11px)',
          maxHeight: 140, overflowY: 'auto',
        }}>
          {steps.map((s) => (
            <div key={s.id} style={{ padding: '2px 0', borderBottom: '1px solid var(--color-border-secondary, #222)' }}>
              <span style={{ color: 'var(--color-text-tertiary, #555)', marginRight: 6 }}>{s.id}.</span>
              <span style={{ color: 'var(--color-module-proof, #ba68c8)' }}>{s.tactic}</span>
              <br />
              <span style={{ color: 'var(--color-text-secondary)', paddingLeft: 16 }}>{s.result}</span>
            </div>
          ))}
        </div>
      ))}
    </div>
  );
};
