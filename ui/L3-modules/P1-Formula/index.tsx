import React from 'react';
import { useUIStore } from '../../L5-core/store/uiStore';

interface FormulaPanelProps {
  onSubmit?: (text: string) => void;
}

export const FormulaPanel: React.FC<FormulaPanelProps> = ({ onSubmit }) => {
  const input = useUIStore((s) => s.formulaInput);
  const setInput = useUIStore((s) => s.setFormulaInput);
  const format = useUIStore((s) => s.formulaOutputFormat);
  const setFormat = useUIStore((s) => s.setFormulaOutputFormat);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', gap: 8 }}>
      <div style={{ display: 'flex', gap: 8, justifyContent: 'space-between', alignItems: 'center' }}>
        <span style={{ fontSize: 'var(--font-size-sm)', color: 'var(--color-text-secondary)' }}>
          Formula Input
        </span>
        <select
          className="input"
          value={format}
          onChange={(e) => setFormat(e.target.value as 'latex' | 'python' | 'dsl')}
          style={{ cursor: 'pointer', width: 120, padding: '4px 8px', fontSize: 'var(--font-size-sm)' }}
        >
          <option value="dsl">Lv-00 DSL</option>
          <option value="latex">LaTeX</option>
          <option value="python">Python</option>
        </select>
      </div>
      <textarea
        className="formula-editor"
        value={input}
        onChange={(e) => setInput(e.target.value)}
        placeholder="point A at (100, 200);&#10;point B at (400, 150);&#10;segment AB between A and B;"
        spellCheck={false}
      />
      <button className="btn btn-primary" onClick={() => onSubmit?.(input)}>
        Evaluate
      </button>
    </div>
  );
};
