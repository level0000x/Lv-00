import React from 'react';

export const EnginePanel: React.FC = () => (
  <div style={{ padding: 12, color: 'var(--color-text-secondary)', fontSize: 'var(--font-size-md)' }}>
    <h3 style={{ color: 'var(--color-module-engine)', marginBottom: 8, fontSize: 'var(--font-size-lg)' }}>Engine Status</h3>
    <p>Real-time engine resource monitoring and performance metrics.</p>
    <div style={{ marginTop: 12, display: 'flex', flexDirection: 'column', gap: 8 }}>
      <div style={{ padding: 8, background: 'var(--color-bg-primary)', borderRadius: 6, border: '1px solid var(--color-border-secondary)' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
          <span>Status</span>
          <span style={{ color: 'var(--color-success)' }}>Idle</span>
        </div>
        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
          <span>Backend</span>
          <span style={{ color: 'var(--color-accent)' }}>None</span>
        </div>
      </div>
    </div>
  </div>
);
