import React from 'react';

interface SliderProps {
  min?: number;
  max?: number;
  step?: number;
  value: number;
  onChange: (v: number) => void;
  label?: string;
}

export const Slider: React.FC<SliderProps> = ({ min = 0, max = 100, step = 1, value, onChange, label }) => (
  <div style={{ display: 'flex', alignItems: 'center', gap: 8, padding: '4px 0' }}>
    {label && <span style={{ fontSize: 'var(--font-size-sm)', color: 'var(--color-text-secondary)', minWidth: 60 }}>{label}</span>}
    <input type="range" min={min} max={max} step={step} value={value} onChange={(e) => onChange(Number(e.target.value))}
      style={{ flex: 1, accentColor: 'var(--color-accent)', height: 4, cursor: 'pointer' }} />
    <span style={{ fontSize: 'var(--font-size-xs)', fontFamily: 'var(--font-mono)', color: 'var(--color-text-primary)', minWidth: 40, textAlign: 'right' }}>{value}</span>
  </div>
);

export default Slider;
