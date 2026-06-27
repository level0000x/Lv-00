import React, { useState, useEffect, useRef } from 'react';

export interface CommandItem { id: string; label: string; shortcut?: string; group?: string; }

interface CommandPaletteProps { commands: CommandItem[]; onSelect: (id: string) => void; onClose: () => void; }

export const CommandPalette: React.FC<CommandPaletteProps> = ({ commands, onSelect, onClose }) => {
  const [query, setQuery] = useState('');
  const [selectedIndex, setSelectedIndex] = useState(0);
  const inputRef = useRef<HTMLInputElement>(null);
  const filtered = commands.filter(c => c.label.toLowerCase().includes(query.toLowerCase()));

  useEffect(() => { setSelectedIndex(0); }, [query]);
  useEffect(() => { inputRef.current?.focus(); }, []);

  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      if (e.key === 'Escape') { onClose(); e.preventDefault(); }
      if (e.key === 'ArrowDown') { setSelectedIndex(i => Math.min(i + 1, filtered.length - 1)); e.preventDefault(); }
      if (e.key === 'ArrowUp') { setSelectedIndex(i => Math.max(i - 1, 0)); e.preventDefault(); }
      if (e.key === 'Enter' && filtered[selectedIndex]) { onSelect(filtered[selectedIndex].id); onClose(); e.preventDefault(); }
    };
    window.addEventListener('keydown', handler);
    return () => window.removeEventListener('keydown', handler);
  }, [filtered, selectedIndex, onSelect, onClose]);

  return (
    <div className="modal-overlay" onClick={onClose}>
      <div className="command-palette" onClick={e => e.stopPropagation()} style={{
        background: 'var(--color-bg-elevated)', border: '1px solid var(--color-border-primary)',
        borderRadius: 'var(--radius-lg)', boxShadow: 'var(--shadow-xl)', width: 520, maxHeight: 400,
        display: 'flex', flexDirection: 'column', overflow: 'hidden', animation: 'modalIn 0.15s ease-out'
      }}>
        <input ref={inputRef} className="input" placeholder="输入命令 Type a command..."
          value={query} onChange={e => setQuery(e.target.value)}
          style={{ border: 'none', borderRadius: 0, borderBottom: '1px solid var(--color-border-secondary)', padding: '12px 16px', fontSize: 'var(--font-size-md)' }} />
        <div style={{ overflow: 'auto', flex: 1, padding: '4px 0' }}>
          {filtered.length === 0 && <div style={{ padding: 16, textAlign: 'center', color: 'var(--color-text-muted)', fontSize: 'var(--font-size-sm)' }}>无结果 No results</div>}
          {filtered.map((cmd, i) => (
            <div key={cmd.id} onClick={() => { onSelect(cmd.id); onClose(); }}
              style={{ padding: '8px 16px', cursor: 'pointer', display: 'flex', justifyContent: 'space-between', alignItems: 'center',
                background: i === selectedIndex ? 'var(--color-bg-active)' : 'transparent', color: 'var(--color-text-primary)', fontSize: 'var(--font-size-sm)',
                transition: 'background 0.1s' }}
              onMouseEnter={() => setSelectedIndex(i)}>
              <span>{cmd.group && <span style={{ color: 'var(--color-text-muted)', marginRight: 8, fontSize: 'var(--font-size-xs)' }}>{cmd.group}</span>}{cmd.label}</span>
              {cmd.shortcut && <span className="kbd">{cmd.shortcut}</span>}
            </div>
          ))}
        </div>
      </div>
    </div>
  );
};

export default CommandPalette;
