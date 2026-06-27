import React, { useRef, useState, useEffect, useCallback } from 'react';
import { useTerminalStore } from '../../L5-core/store/terminalStore';

interface TerminalViewProps {
  onSubmit?: (command: string) => void;
  completions?: string[];
}

export const TerminalView: React.FC<TerminalViewProps> = ({ onSubmit, completions = [] }) => {
  const lines = useTerminalStore((s) => s.lines);
  const addLine = useTerminalStore((s) => s.addLine);
  const cleanLines = useTerminalStore((s) => s.clearLines);

  const [input, setInput] = useState('');
  const outputRef = useRef<HTMLDivElement>(null);
  const inputRef = useRef<HTMLInputElement>(null);

  /* ---- Command history ---- */
  const [history, setHistory] = useState<string[]>([]);
  const [historyIdx, setHistoryIdx] = useState(-1);

  /* ---- Auto-scroll to bottom on new output ---- */
  useEffect(() => {
    if (outputRef.current) {
      outputRef.current.scrollTop = outputRef.current.scrollHeight;
    }
  }, [lines]);

  /* ---- Tab completion ---- */
  const complete = useCallback((current: string): string => {
    if (!current.trim()) return current;
    const parts = current.split(' ');
    const lastWord = parts[parts.length - 1].toLowerCase();
    const matches = completions.filter((c) => c.toLowerCase().startsWith(lastWord));
    if (matches.length === 1) {
      parts[parts.length - 1] = matches[0];
      return parts.join(' ') + ' ';
    }
    if (matches.length > 1) {
      // Show completions
      addLine(`  ${matches.join('   ')}`, 'var(--color-text-secondary)');
      // Common prefix
      let prefix = matches[0];
      for (const m of matches) {
        while (!m.toLowerCase().startsWith(prefix.toLowerCase())) prefix = prefix.slice(0, -1);
      }
      if (prefix.length > lastWord.length) {
        parts[parts.length - 1] = prefix;
        return parts.join(' ');
      }
    }
    return current;
  }, [completions, addLine]);

  /* ---- Enhanced help ---- */
  const HELP_LINES = [
    '',
    '  可用命令 Available commands:',
    '  ─────────────────────────────────────────',
    '  add point <name> at (<x>,<y>)    添加点 Add point',
    '  add segment <a> <b>              添加段 Add segment',
    '  add circle center=<a> radius=<b> 添加圆 Add circle',
    '  add line <a> <b>                 添加线 Add line',
    '  normalize                        归一化 Normalize',
    '  undo / redo                      撤销/重做 Undo/Redo',
    '  measure distance <a> <b>         测距 Measure distance',
    '  measure angle <a> <b> <c>        测角 Measure angle',
    '  show grid / hide grid            显示/隐藏网格 Grid',
    '  export svg                       导出SVG Export',
    '  zoom fit                         适配视图 Fit view',
    '  clear / cls                      清屏 Clear',
    '  help                             帮助 Help',
    '',
  ];

  const submit = useCallback((cmd: string) => {
    if (!cmd.trim()) return;
    addLine(`> ${cmd}`, 'var(--color-accent)');

    // Save to history
    setHistory((prev) => [cmd, ...prev.filter((h) => h !== cmd).slice(0, 99)]);
    setHistoryIdx(-1);

    const trimmed = cmd.trim().toLowerCase();

    if (trimmed === 'help') {
      HELP_LINES.forEach((l) => addLine(l, 'var(--color-text-secondary)'));
    } else if (trimmed === 'clear' || trimmed === 'cls') {
      cleanLines();
    } else {
      onSubmit?.(cmd);
    }

    setInput('');
  }, [addLine, cleanLines, onSubmit, HELP_LINES]);

  /* ---- Key handlers ---- */
  const handleKeyDown = useCallback((e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === 'Enter') {
      submit(input);
    } else if (e.key === 'Tab') {
      e.preventDefault();
      const completed = complete(input);
      setInput(completed);
    } else if (e.key === 'ArrowUp') {
      e.preventDefault();
      if (history.length > 0) {
        const newIdx = historyIdx === -1 ? 0 : Math.min(historyIdx + 1, history.length - 1);
        setHistoryIdx(newIdx);
        setInput(history[newIdx]);
      }
    } else if (e.key === 'ArrowDown') {
      e.preventDefault();
      if (historyIdx > 0) {
        const newIdx = historyIdx - 1;
        setHistoryIdx(newIdx);
        setInput(history[newIdx]);
      } else if (historyIdx === 0) {
        setHistoryIdx(-1);
        setInput('');
      }
    } else if (e.key === 'l' && e.ctrlKey) {
      e.preventDefault();
      cleanLines();
    }
  }, [input, history, historyIdx, submit, complete, cleanLines]);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      {/* Toolbar */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: 8,
        padding: '4px 8px',
        borderBottom: '1px solid var(--color-border-secondary)',
        flexShrink: 0,
      }}>
        <button
          className="btn btn-small btn-ghost"
          onClick={() => cleanLines()}
        >
          清屏 Clear
        </button>
        <span style={{
          fontSize: 10,
          color: 'var(--color-text-muted)',
          fontFamily: 'var(--font-mono)',
        }}>
          Tab 补全 | Up/Down 历史 | Ctrl+L 清屏
        </span>
      </div>

      {/* Output area */}
      <div
        ref={outputRef}
        className="terminal-output"
        style={{ flex: 1 }}
      >
        {lines.map((l) => (
          <div key={l.id} style={{ color: l.color, whiteSpace: 'pre-wrap', wordBreak: 'break-all' }}>{l.text}</div>
        ))}
      </div>

      {/* Input line */}
      <div className="terminal-input-line">
        <span className="terminal-prompt">&gt;</span>
        <input
          ref={inputRef}
          value={input}
          onChange={(e) => setInput(e.target.value)}
          onKeyDown={handleKeyDown}
          spellCheck={false}
          autoFocus
          placeholder="输入命令 Type help for commands..."
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
