import React, { useRef, useState, useEffect } from 'react';
import { useTerminalStore } from '../../L5-core/store/terminalStore';

interface TerminalViewProps {
  onSubmit?: (command: string) => void;
  completions?: string[];
}

export const TerminalView: React.FC<TerminalViewProps> = ({ onSubmit }) => {
  const lines = useTerminalStore((s) => s.lines);
  const addLine = useTerminalStore((s) => s.addLine);
  const cleanLines = useTerminalStore((s) => s.clearLines);

  const [input, setInput] = useState('');
  const outputRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    outputRef.current?.scrollTo(0, outputRef.current.scrollHeight);
  }, [lines]);

  const submit = (cmd: string) => {
    if (!cmd.trim()) return;
    addLine(`> ${cmd}`, 'var(--color-accent)');

    if (cmd === 'help') {
      addLine('  add point <name> at (<x>, <y>)', 'var(--color-text-secondary)');
      addLine('  add segment <name> between <a> and <b>', 'var(--color-text-secondary)');
      addLine('  normalize', 'var(--color-text-secondary)');
      addLine('  undo | redo', 'var(--color-text-secondary)');
      addLine('  clear | cls', 'var(--color-text-secondary)');
      addLine('  help', 'var(--color-text-secondary)');
      addLine('');
    } else if (cmd === 'clear' || cmd === 'cls') {
      cleanLines();
    } else {
      onSubmit?.(cmd);
    }

    setInput('');
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      <div ref={outputRef} className="terminal-output">
        {lines.map((l) => (
          <div key={l.id} style={{ color: l.color }}>{l.text}</div>
        ))}
      </div>
      <div className="terminal-input-line">
        <span className="terminal-prompt">&gt;</span>
        <input
          value={input}
          onChange={(e) => setInput(e.target.value)}
          onKeyDown={(e) => {
            if (e.key === 'Enter') submit(input);
          }}
          spellCheck={false}
          autoFocus
          style={{
            flex: 1,
            background: 'transparent',
            border: 'none',
            color: 'var(--color-text-primary)',
            fontFamily: 'var(--font-mono)',
            fontSize: 13,
            outline: 'none',
          }}
        />
      </div>
    </div>
  );
};
