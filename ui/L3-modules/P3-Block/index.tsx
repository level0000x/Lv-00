import React from 'react';

export const BlockPanel: React.FC = () => (
  <div style={{ padding: 12, color: 'var(--color-text-secondary)', fontSize: 'var(--font-size-md)' }}>
    <h3 style={{ color: 'var(--color-module-block)', marginBottom: 8, fontSize: 'var(--font-size-lg)' }}>Function Blocks</h3>
    <p>Create and manage function blocks for geometric constructions.</p>
    <div style={{ marginTop: 12 }}>
      <button className="btn btn-small">+ New Block</button>
    </div>
  </div>
);
