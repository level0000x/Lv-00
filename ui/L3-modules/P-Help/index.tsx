import React from 'react';
import { SHORTCUTS } from '../../L5-core/hooks/useKeyboard';

export const HelpPanel: React.FC = () => (
  <div style={{ padding: 12, color: 'var(--color-text-secondary)', fontSize: 'var(--font-size-md)' }}>
    <h3 style={{ color: 'var(--color-module-help)', marginBottom: 8, fontSize: 'var(--font-size-lg)' }}>Help & Shortcuts</h3>

    <h4 style={{ fontSize: 'var(--font-size-sm)', color: 'var(--color-text-primary)', marginTop: 12, marginBottom: 6 }}>Keyboard Shortcuts</h4>
    <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
      {SHORTCUTS.map((s) => (
        <div key={s.label} style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '4px 0' }}>
          <span>{s.label}</span>
          <span className="kbd">{s.keys.join('+')}</span>
        </div>
      ))}
    </div>

    <h4 style={{ fontSize: 'var(--font-size-sm)', color: 'var(--color-text-primary)', marginTop: 16, marginBottom: 6 }}>Terminal Commands</h4>
    <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
      <div><span style={{ color: 'var(--color-accent)' }}>help</span> - Show this help</div>
      <div><span style={{ color: 'var(--color-accent)' }}>clear</span> - Clear terminal</div>
      <div><span style={{ color: 'var(--color-accent)' }}>normalize</span> - Normalize graph</div>
      <div><span style={{ color: 'var(--color-accent)' }}>add point &lt;name&gt; at (&lt;x&gt;,&lt;y&gt;)</span></div>
    </div>
  </div>
);
