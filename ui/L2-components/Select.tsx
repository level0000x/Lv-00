import React from 'react';

interface SelectOption {
  value: string;
  label: string;
}

interface SelectProps {
  value: string;
  options: SelectOption[];
  onChange: (value: string) => void;
  placeholder?: string;
  disabled?: boolean;
}

const Select: React.FC<SelectProps> = ({ value, options, onChange, placeholder, disabled }) => (
  <select
    className="input"
    value={value}
    onChange={(e) => onChange(e.target.value)}
    disabled={disabled}
    style={{ cursor: 'pointer', paddingRight: 24 }}
  >
    {placeholder && <option value="" disabled>{placeholder}</option>}
    {options.map((opt) => (
      <option key={opt.value} value={opt.value}>{opt.label}</option>
    ))}
  </select>
);

export default Select;
