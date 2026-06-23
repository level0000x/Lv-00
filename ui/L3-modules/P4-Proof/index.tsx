import React from 'react';

export const ProofPanel: React.FC = () => (
  <div style={{ padding: 12, color: 'var(--color-text-secondary)', fontSize: 'var(--font-size-md)' }}>
    <h3 style={{ color: 'var(--color-module-proof)', marginBottom: 8, fontSize: 'var(--font-size-lg)' }}>Proof Navigator</h3>
    <p>Browse and construct proof trees for geometric theorems.</p>
    <div style={{ marginTop: 12, padding: 8, background: 'var(--color-bg-primary)', borderRadius: 6, border: '1px solid var(--color-border-secondary)' }}>
      <div>Proof steps: 0</div>
      <div>Open goals: 0</div>
    </div>
  </div>
);
