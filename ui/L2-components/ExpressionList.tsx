import React from 'react';

interface ExpressionItem { id: string; expr: string; color: string; visible: boolean; }
interface ExpressionListProps { items: ExpressionItem[]; onItemChange?: (id: string, expr: string) => void; onToggleVisibility?: (id: string) => void; onRemove?: (id: string) => void; onAdd?: () => void; }

export const ExpressionList: React.FC<ExpressionListProps> = ({ items, onItemChange, onToggleVisibility, onRemove, onAdd }) => (
  <div style={{ display: 'flex', flexDirection: 'column', gap: 2, height: '100%' }}>
    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '4px 8px' }}>
      <span style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)', textTransform: 'uppercase', letterSpacing: 0.5, fontWeight: 600 }}>表达式 Expressions</span>
      {onAdd && <button className="btn-icon" onClick={onAdd} style={{ width: 22, height: 22, fontSize: 14 }} title="添加 Add">+</button>}
    </div>
    <div style={{ flex: 1, overflow: 'auto' }}>
      {items.map(item => (
        <div key={item.id} style={{ display: 'flex', alignItems: 'center', gap: 6, padding: '4px 8px', borderRadius: 'var(--radius-sm)', transition: 'background 0.1s', cursor: 'default' }}
          onMouseEnter={e => (e.currentTarget as HTMLElement).style.background = 'var(--color-bg-hover-subtle)'}
          onMouseLeave={e => (e.currentTarget as HTMLElement).style.background = 'transparent'}>
          <div style={{ width: 10, height: 10, borderRadius: '50%', background: item.visible ? item.color : 'var(--color-text-muted)', opacity: item.visible ? 1 : 0.3, cursor: 'pointer', flexShrink: 0 }}
            onClick={() => onToggleVisibility?.(item.id)} />
          <input value={item.expr} onChange={e => onItemChange?.(item.id, e.target.value)} spellCheck={false}
            style={{ flex: 1, background: 'transparent', border: 'none', color: item.visible ? 'var(--color-text-primary)' : 'var(--color-text-muted)', fontFamily: 'var(--font-mono)', fontSize: 'var(--font-size-sm)', outline: 'none', opacity: item.visible ? 1 : 0.5, minWidth: 0 }} />
          <button className="btn-icon" onClick={() => onRemove?.(item.id)} style={{ width: 18, height: 18, fontSize: 10, opacity: 0.3 }}
            onMouseEnter={e => (e.currentTarget as HTMLElement).style.opacity = '1'}
            onMouseLeave={e => (e.currentTarget as HTMLElement).style.opacity = '0.3'}>{'\u2715'}</button>
        </div>
      ))}
    </div>
  </div>
);

export default ExpressionList;
