import React, { useState } from 'react';

interface ToolItem { id: string; icon: string; label: string; group?: string; }
interface CanvasToolbarProps { activeTool: string; onToolChange: (id: string) => void; }

const TOOLS: ToolItem[] = [
  { id: 'select', icon: '\u271B', label: '选择 Select' },
  { id: 'point', icon: '\u2022', label: '画点 Point' },
  { id: 'segment', icon: '\u2014', label: '线段 Segment' },
  { id: 'line', icon: '\u2194', label: '直线 Line' },
  { id: 'circle', icon: '\u25CB', label: '圆 Circle' },
  { id: 'angle', icon: '\u2220', label: '角度 Angle' },
  { id: 'pan', icon: '\u270B', label: '平移 Pan' },
];

export const CanvasToolbar: React.FC<CanvasToolbarProps> = ({ activeTool, onToolChange }) => {
  const [collapsed, setCollapsed] = useState(false);

  if (collapsed) return (
    <div className="canvas-toolbar" style={{ position: 'absolute', left: 8, top: '50%', transform: 'translateY(-50%)', zIndex: 5, display: 'flex', flexDirection: 'column', gap: 2, background: 'var(--color-bg-secondary)', border: '1px solid var(--color-border-primary)', borderRadius: 'var(--radius-md)', padding: 4, boxShadow: 'var(--shadow-md)' }}>
      <button className="btn-icon" onClick={() => setCollapsed(false)} title="展开 Expand" style={{ width: 32, height: 32 }}>{'\u2261'}</button>
    </div>
  );

  return (
    <div className="canvas-toolbar" style={{ position: 'absolute', left: 8, top: '50%', transform: 'translateY(-50%)', zIndex: 5, display: 'flex', flexDirection: 'column', gap: 2, background: 'var(--color-bg-secondary)', border: '1px solid var(--color-border-primary)', borderRadius: 'var(--radius-md)', padding: 4, boxShadow: 'var(--shadow-md)' }}>
      <button className="btn-icon" onClick={() => setCollapsed(true)} title="收起 Collapse" style={{ width: 32, height: 32, fontSize: 11 }}>{'\u203A'}</button>
      {TOOLS.map(t => (
        <button key={t.id} className={`btn-icon ${activeTool === t.id ? 'active-tool' : ''}`} onClick={() => onToolChange(t.id)}
          title={t.label} style={{ width: 32, height: 32, fontSize: 14, borderRadius: 'var(--radius-sm)', background: activeTool === t.id ? 'var(--color-bg-active)' : undefined, color: activeTool === t.id ? 'var(--color-text-bright)' : undefined }}>
          {t.icon}
        </button>
      ))}
    </div>
  );
};

export default CanvasToolbar;
