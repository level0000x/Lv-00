import React from 'react';

export const GraphPanel: React.FC = () => (
  <div style={{ padding: 12, color: 'var(--color-text-secondary)', fontSize: 'var(--font-size-md)' }}>
    <h3 style={{ color: 'var(--color-module-graph)', marginBottom: 8, fontSize: 'var(--font-size-lg)' }}>Graph Viewer</h3>
    <p>Constraint graph visualization and statistics.</p>
    <div style={{ marginTop: 12, padding: 8, background: 'var(--color-bg-primary)', borderRadius: 6, border: '1px solid var(--color-border-secondary)' }}>
      <div>Nodes: 0</div>
      <div>Constraints: 0</div>
      <div>Connected components: 0</div>
    </div>
  </div>
);
