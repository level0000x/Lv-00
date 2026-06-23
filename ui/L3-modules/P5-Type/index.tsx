import React from 'react';

export const TypePanel: React.FC = () => (
  <div style={{ padding: 12, color: 'var(--color-text-secondary)', fontSize: 'var(--font-size-md)' }}>
    <h3 style={{ color: 'var(--color-module-type)', marginBottom: 8, fontSize: 'var(--font-size-lg)' }}>Type Explorer</h3>
    <p>Browse the Lv-00 type hierarchy and trait system.</p>
    <div style={{ marginTop: 12, padding: 8, background: 'var(--color-bg-primary)', borderRadius: 6, border: '1px solid var(--color-border-secondary)' }}>
      <div>Point</div>
      <div>Segment</div>
      <div>Region</div>
      <div>Circle</div>
      <div>Triangle</div>
    </div>
  </div>
);
