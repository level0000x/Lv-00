import React from 'react';

export const DebugPanel: React.FC = () => (
  <div style={{ padding: 12, color: 'var(--color-text-secondary)', fontSize: 'var(--font-size-md)' }}>
    <h3 style={{ color: 'var(--color-module-debug)', marginBottom: 8, fontSize: 'var(--font-size-lg)' }}>Debug Console</h3>
    <p>Runtime diagnostics, error logs, and breakpoint management.</p>
    <div style={{ marginTop: 12, padding: 8, background: '#010409', borderRadius: 6, border: '1px solid var(--color-border-secondary)', minHeight: 200, fontFamily: 'var(--font-mono)', fontSize: 'var(--font-size-sm)' }}>
      <div style={{ color: 'var(--color-text-primary)' }}>Available commands:</div>
      <div style={{ color: 'var(--color-accent)' }}>&gt; stats</div>
      <div style={{ color: 'var(--color-accent)' }}>&gt; inspect</div>
      <div style={{ color: 'var(--color-accent)' }}>&gt; trace</div>
    </div>
  </div>
);
