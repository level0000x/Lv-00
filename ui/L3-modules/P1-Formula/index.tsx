import React, { useState } from 'react';
import { useUIStore } from '../../L5-core/store/uiStore';

/* ── tiny inline components (no L2 dependency yet) ── */

const COLORS = ['#4fc3f7', '#81c784', '#ffb74d', '#e57373', '#ba68c8', '#4dd0e1'];

interface Expr {
  id: string;
  text: string;
  color: string;
  visible: boolean;
}

/* ── ExpressionList ── */
const ExpressionList: React.FC<{
  expressions: Expr[];
  onToggle: (id: string) => void;
  onDelete: (id: string) => void;
  onTextChange: (id: string, text: string) => void;
  onColorChange: (id: string, color: string) => void;
}> = ({ expressions, onToggle, onDelete, onTextChange, onColorChange }) => (
  <div style={{ display: 'flex', flexDirection: 'column', gap: 2 }}>
    {expressions.map((expr) => (
      <div
        key={expr.id}
        style={{
          display: 'flex',
          alignItems: 'center',
          gap: 6,
          padding: '4px 6px',
          borderRadius: 4,
          background: expr.visible ? 'transparent' : 'var(--color-bg-secondary, #1a1a2e)',
          opacity: expr.visible ? 1 : 0.5,
        }}
      >
        {/* visibility toggle */}
        <button
          onClick={() => onToggle(expr.id)}
          title={expr.visible ? '隐藏 Hide' : '显示 Show'}
          style={{
            background: 'none',
            border: 'none',
            cursor: 'pointer',
            color: expr.visible ? expr.color : 'var(--color-text-tertiary, #555)',
            fontSize: 'var(--font-size-sm, 12px)',
            width: 18,
            height: 18,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
          }}
        >
          {expr.visible ? '\u25CF' : '\u25CB'}
        </button>

        {/* color swatch */}
        <label
          style={{
            width: 12,
            height: 12,
            borderRadius: 2,
            background: expr.color,
            cursor: 'pointer',
            flexShrink: 0,
          }}
        >
          <input
            type="color"
            value={expr.color}
            onChange={(e) => onColorChange(expr.id, e.target.value)}
            style={{ opacity: 0, width: 0, height: 0, position: 'absolute' }}
          />
        </label>

        {/* text input */}
        <input
          type="text"
          value={expr.text}
          onChange={(e) => onTextChange(expr.id, e.target.value)}
          style={{
            flex: 1,
            background: 'transparent',
            border: 'none',
            color: 'var(--color-text-primary)',
            fontFamily: 'var(--font-mono)',
            fontSize: 'var(--font-size-sm, 12px)',
            outline: 'none',
            minWidth: 0,
          }}
        />

        {/* delete */}
        <button
          onClick={() => onDelete(expr.id)}
          title="删除 Delete"
          style={{
            background: 'none',
            border: 'none',
            cursor: 'pointer',
            color: 'var(--color-text-tertiary, #555)',
            fontSize: 'var(--font-size-sm, 12px)',
            padding: 0,
          }}
        >
          \u00D7
        </button>
      </div>
    ))}
  </div>
);

/* ── FormulaPanel ── */

interface FormulaPanelProps {
  onSubmit?: (text: string) => void;
}

let nextId = 10;

export const FormulaPanel: React.FC<FormulaPanelProps> = ({ onSubmit }) => {
  const input = useUIStore((s) => s.formulaInput);
  const setInput = useUIStore((s) => s.setFormulaInput);
  const format = useUIStore((s) => s.formulaOutputFormat);
  const setFormat = useUIStore((s) => s.setFormulaOutputFormat);

  const [expressions, setExpressions] = useState<Expr[]>([
    { id: '1', text: 'A = (100, 200)', color: COLORS[0], visible: true },
    { id: '2', text: 'B = (400, 150)', color: COLORS[1], visible: true },
    { id: '3', text: 'C = (250, 350)', color: COLORS[2], visible: true },
    { id: '4', text: 'circle(A, |AB|)', color: COLORS[3], visible: true },
    { id: '5', text: 'midpoint(A, B)', color: COLORS[4], visible: true },
  ]);

  const addExpression = () => {
    setExpressions((prev) => [
      ...prev,
      { id: String(++nextId), text: '', color: COLORS[prev.length % COLORS.length], visible: true },
    ]);
  };

  const toggleExpr = (id: string) =>
    setExpressions((prev) => prev.map((e) => (e.id === id ? { ...e, visible: !e.visible } : e)));

  const deleteExpr = (id: string) =>
    setExpressions((prev) => prev.filter((e) => e.id !== id));

  const changeText = (id: string, text: string) =>
    setExpressions((prev) => prev.map((e) => (e.id === id ? { ...e, text } : e)));

  const changeColor = (id: string, color: string) =>
    setExpressions((prev) => prev.map((e) => (e.id === id ? { ...e, color } : e)));

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', gap: 8 }}>
      {/* header row */}
      <div style={{ display: 'flex', gap: 8, justifyContent: 'space-between', alignItems: 'center' }}>
        <span style={{ fontSize: 'var(--font-size-sm, 12px)', color: 'var(--color-text-secondary)' }}>
          表达式列表 Expression List
        </span>
        <button
          className="btn btn-small"
          onClick={addExpression}
          title="添加表达式 Add Expression"
        >
          + 添加 Add
        </button>
      </div>

      {/* expression list */}
      <div
        style={{
          flex: 1,
          overflowY: 'auto',
          padding: '4px 0',
          borderBottom: '1px solid var(--color-border-secondary)',
        }}
      >
        <ExpressionList
          expressions={expressions}
          onToggle={toggleExpr}
          onDelete={deleteExpr}
          onTextChange={changeText}
          onColorChange={changeColor}
        />
      </div>

      {/* command input */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: 6 }}>
        <div style={{ display: 'flex', gap: 8, justifyContent: 'space-between', alignItems: 'center' }}>
          <span style={{ fontSize: 'var(--font-size-xs, 11px)', color: 'var(--color-text-secondary)' }}>
            命令输入 Command Input
          </span>
          <select
            className="input"
            value={format}
            onChange={(e) => setFormat(e.target.value as 'latex' | 'python' | 'dsl')}
            style={{
              cursor: 'pointer',
              width: 120,
              padding: '2px 6px',
              fontSize: 'var(--font-size-xs, 11px)',
              background: 'var(--color-bg-secondary)',
              color: 'var(--color-text-primary)',
              border: '1px solid var(--color-border-secondary)',
              borderRadius: 4,
            }}
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
          placeholder={"point A at (100, 200);\npoint B at (400, 150);\nsegment AB between A and B;"}
          spellCheck={false}
          rows={3}
        />
        <button className="btn btn-primary" onClick={() => onSubmit?.(input)}>
          求值 Evaluate
        </button>
      </div>
    </div>
  );
};
