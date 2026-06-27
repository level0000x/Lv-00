import React from 'react';

interface CheckboxProps {
  checked: boolean;
  onChange: (v: boolean) => void;
  label?: string;
}

export const Checkbox: React.FC<CheckboxProps> = ({ checked, onChange, label }) => (
  <label style={{ display: 'flex', alignItems: 'center', gap: 6, cursor: 'pointer', fontSize: 'var(--font-size-sm)', color: 'var(--color-text-secondary)' }}>
    <input type="checkbox" checked={checked} onChange={(e) => onChange(e.target.checked)}
      style={{ accentColor: 'var(--color-accent)', width: 14, height: 14, cursor: 'pointer' }} />
    {label}
  </label>
);

export default Checkbox;
