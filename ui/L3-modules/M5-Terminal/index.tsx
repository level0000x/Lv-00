import React, { useRef, useState, useEffect, useCallback } from 'react';
import { useGeometryStore } from '../../L5-core/store/geometryStore';

interface TerminalViewProps {
  onSubmit?: (command: string) => void;
  completions?: string[];
}

export const TerminalView: React.FC<TerminalViewProps> = () => {
  const addLine = useCallback((text: string, color?: string) => {
    const s = useGeometryStore.getState();
    // Use internal lines array managed locally
    setLines((prev) => [...prev, { id: lineIdCounter++, text, color: color ?? 'var(--color-text-primary)' }]);
  }, []);

  const clearLines = useCallback(() => {
    setLines([]);
  }, []);

  const [lines, setLines] = useState<{ id: number; text: string; color: string }[]>([
    { id: 0, text: 'Lv-00 几何终端 Geometry Terminal v3.4.0 — 输入 help 查看命令 Type \'help\' for commands', color: 'var(--color-text-secondary)' },
    { id: 1, text: '', color: 'var(--color-text-primary)' },
  ]);
  const [input, setInput] = useState('');
  const outputRef = useRef<HTMLDivElement>(null);
  const inputRef = useRef<HTMLInputElement>(null);

  // Command history
  const [history, setHistory] = useState<string[]>([]);
  const [historyIdx, setHistoryIdx] = useState(-1);

  // Auto-scroll to bottom
  useEffect(() => {
    if (outputRef.current) {
      outputRef.current.scrollTop = outputRef.current.scrollHeight;
    }
  }, [lines]);

  // Tab completion
  const COMMAND_NAMES = [
    'add', 'delete', 'move', 'select', 'list', 'clear', 'demo', 'help', 'stats',
  ];

  const complete = useCallback((current: string): string => {
    if (!current.trim()) return current;
    const parts = current.split(' ');
    const lastWord = parts[parts.length - 1].toLowerCase();
    const matches = COMMAND_NAMES.filter((c) => c.startsWith(lastWord));
    if (matches.length === 1) {
      parts[parts.length - 1] = matches[0];
      return parts.join(' ') + ' ';
    }
    if (matches.length > 1) {
      setLines((prev) => [...prev, { id: lineIdCounter++, text: `  ${matches.join('   ')}`, color: 'var(--color-text-secondary)' }]);
      let prefix = matches[0];
      for (const m of matches) {
        while (!m.startsWith(prefix)) prefix = prefix.slice(0, -1);
      }
      if (prefix.length > lastWord.length) {
        parts[parts.length - 1] = prefix;
        return parts.join(' ');
      }
    }
    return current;
  }, []);

  // Find point by label
  const findPointByLabel = (label: string) => {
    const s = useGeometryStore.getState();
    return s.objects.find((o) => o.type === 'point' && o.label.toLowerCase() === label.toLowerCase());
  };

  // Command execution
  const execCommand = useCallback((raw: string) => {
    const cmd = raw.trim();
    if (!cmd) return;

    setLines((prev) => [...prev, { id: lineIdCounter++, text: `> ${cmd}`, color: 'var(--color-accent)' }]);

    // Save to history
    setHistory((prev) => [cmd, ...prev.filter((h) => h !== cmd).slice(0, 99)]);
    setHistoryIdx(-1);

    const lower = cmd.toLowerCase();

    if (lower === 'help') {
      const help = [
        '',
        '  可用命令 Available commands:',
        '  ──────────────────────────────────────────────',
        '  add point <name> at (<x>, <y>)      添加点 Add point',
        '  add segment <name> between <A> and <B>  添加线段 Add segment',
        '  add circle <name> center <A> radius <B>  添加圆 Add circle',
        '  add line <name> between <A> and <B>  添加直线 Add line',
        '  midpoint <A>, <B>                   中点 Midpoint of A and B',
        '  delete <label>                       删除对象 Delete object',
        '  move <label> to (<x>, <y>)          移动点 Move point',
        '  select <label>, <label>             选择对象 Select objects',
        '  list                                列出所有对象 List all',
        '  clear                               清除全部 Clear all',
        '  demo                                加载演示场景 Load demo',
        '  stats                               统计信息 Statistics',
        '  help                                帮助 Help',
        '',
      ];
      help.forEach((l) => {
        setLines((prev) => [...prev, { id: lineIdCounter++, text: l, color: 'var(--color-text-secondary)' }]);
      });
      return;
    }

    if (lower === 'clear' || lower === 'cls') {
      clearLines();
      return;
    }

    if (lower === 'demo') {
      useGeometryStore.getState().loadDemoScene();
      setLines((prev) => [...prev, { id: lineIdCounter++, text: '  已加载演示场景 Demo scene loaded (6 points, 4 segments, 1 circle, 1 midpoint)', color: 'var(--color-success)' }]);
      return;
    }

    if (lower === 'stats') {
      const s = useGeometryStore.getState();
      const counts: Record<string, number> = {};
      for (const o of s.objects) {
        counts[o.type] = (counts[o.type] || 0) + 1;
      }
      const lines_out = [`  对象统计 Statistics: ${s.objects.length} objects, ${s.constraints.length} constraints`];
      for (const [type, count] of Object.entries(counts)) {
        lines_out.push(`    ${type}: ${count}`);
      }
      lines_out.forEach((l) => {
        setLines((prev) => [...prev, { id: lineIdCounter++, text: l, color: 'var(--color-text-secondary)' }]);
      });
      return;
    }

    if (lower === 'list') {
      const s = useGeometryStore.getState();
      if (s.objects.length === 0) {
        setLines((prev) => [...prev, { id: lineIdCounter++, text: '  (empty) 无对象', color: 'var(--color-text-muted)' }]);
        return;
      }
      setLines((prev) => [...prev, { id: lineIdCounter++, text: `  Objects (${s.objects.length}):`, color: 'var(--color-text-secondary)' }]);
      for (const o of s.objects) {
        let detail = `  [${o.type}] ${o.label} (${o.id})`;
        if (o.type === 'point') detail += ` at (${o.x}, ${o.y})`;
        if (o.type === 'segment') detail += ` start=${o.startId} end=${o.endId}` + (o.length !== undefined ? ` L=${o.length.toFixed(1)}` : '');
        if (o.type === 'circle') detail += ` center=${o.centerId} r=${o.radius?.toFixed(1) ?? '?'}`;
        setLines((prev) => [...prev, { id: lineIdCounter++, text: detail, color: 'var(--color-text-primary)' }]);
      }
      return;
    }

    // Parse "add point <name> at (<x>, <y>)"
    const addPointMatch = cmd.match(/^add\s+point\s+(\w+)\s+at\s*\(\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*\)$/i);
    if (addPointMatch) {
      const label = addPointMatch[1];
      const x = parseFloat(addPointMatch[2]);
      const y = parseFloat(addPointMatch[3]);
      const id = useGeometryStore.getState().addObject({ type: 'point', label, x, y, visible: true });
      setLines((prev) => [...prev, { id: lineIdCounter++, text: `  点 Point ${label} created at (${x}, ${y}) [${id}]`, color: 'var(--color-success)' }]);
      return;
    }

    // Parse "add segment <name> between <A> and <B>"
    const addSegMatch = cmd.match(/^add\s+segment\s+(\w+)\s+between\s+(\w+)\s+and\s+(\w+)$/i);
    if (addSegMatch) {
      const label = addSegMatch[1];
      const ptA = findPointByLabel(addSegMatch[2]);
      const ptB = findPointByLabel(addSegMatch[3]);
      if (!ptA || !ptB) {
        setLines((prev) => [...prev, { id: lineIdCounter++, text: `  错误 Error: point not found — ${!ptA ? addSegMatch[2] : addSegMatch[3]}`, color: 'var(--color-danger)' }]);
        return;
      }
      const id = useGeometryStore.getState().addObject({ type: 'segment', label, startId: ptA.id, endId: ptB.id, visible: true });
      setLines((prev) => [...prev, { id: lineIdCounter++, text: `  线段 Segment ${label} created: ${ptA.label}->${ptB.label} [${id}]`, color: 'var(--color-success)' }]);
      return;
    }

    // Parse "add circle <name> center <A> radius <B>"
    const addCirMatch = cmd.match(/^add\s+circle\s+(\w+)\s+center\s+(\w+)\s+radius\s+(\w+)$/i);
    if (addCirMatch) {
      const label = addCirMatch[1];
      const center = findPointByLabel(addCirMatch[2]);
      const rp = findPointByLabel(addCirMatch[3]);
      if (!center || !rp) {
        setLines((prev) => [...prev, { id: lineIdCounter++, text: `  错误 Error: point not found — ${!center ? addCirMatch[2] : addCirMatch[3]}`, color: 'var(--color-danger)' }]);
        return;
      }
      const r = useGeometryStore.getState().getDistance(center.id, rp.id);
      const id = useGeometryStore.getState().addObject({ type: 'circle', label, centerId: center.id, radiusPointId: rp.id, radius: r, visible: true });
      setLines((prev) => [...prev, { id: lineIdCounter++, text: `  圆 Circle ${label} created: center=${center.label} r=${r.toFixed(1)} [${id}]`, color: 'var(--color-success)' }]);
      return;
    }

    // Parse "add line <name> between <A> and <B>"
    const addLineMatch = cmd.match(/^add\s+line\s+(\w+)\s+between\s+(\w+)\s+and\s+(\w+)$/i);
    if (addLineMatch) {
      const label = addLineMatch[1];
      const ptA = findPointByLabel(addLineMatch[2]);
      const ptB = findPointByLabel(addLineMatch[3]);
      if (!ptA || !ptB) {
        setLines((prev) => [...prev, { id: lineIdCounter++, text: `  错误 Error: point not found — ${!ptA ? addLineMatch[2] : addLineMatch[3]}`, color: 'var(--color-danger)' }]);
        return;
      }
      const id = useGeometryStore.getState().addObject({ type: 'line', label, startId: ptA.id, endId: ptB.id, visible: true });
      setLines((prev) => [...prev, { id: lineIdCounter++, text: `  直线 Line ${label} created: ${ptA.label}->${ptB.label} [${id}]`, color: 'var(--color-success)' }]);
      return;
    }

    // Parse "midpoint <A>, <B>"
    const midMatch = cmd.match(/^midpoint\s+(\w+)\s*,\s*(\w+)$/i);
    if (midMatch) {
      const ptA = findPointByLabel(midMatch[1]);
      const ptB = findPointByLabel(midMatch[2]);
      if (!ptA || !ptB) {
        setLines((prev) => [...prev, { id: lineIdCounter++, text: `  错误 Error: point not found`, color: 'var(--color-danger)' }]);
        return;
      }
      const mx = ((ptA.x ?? 0) + (ptB.x ?? 0)) / 2;
      const my = ((ptA.y ?? 0) + (ptB.y ?? 0)) / 2;
      const id = useGeometryStore.getState().addObject({ type: 'point', label: `M(${ptA.label}${ptB.label})`, x: mx, y: my, visible: true });
      setLines((prev) => [...prev, { id: lineIdCounter++, text: `  中点 Midpoint created at (${mx.toFixed(1)}, ${my.toFixed(1)}) [${id}]`, color: 'var(--color-success)' }]);
      return;
    }

    // Parse "delete <label>"
    const delMatch = cmd.match(/^delete\s+(\w+)$/i);
    if (delMatch) {
      const s = useGeometryStore.getState();
      const obj = s.objects.find((o) => o.label.toLowerCase() === delMatch[1].toLowerCase());
      if (!obj) {
        setLines((prev) => [...prev, { id: lineIdCounter++, text: `  错误 Error: object "${delMatch[1]}" not found`, color: 'var(--color-danger)' }]);
        return;
      }
      useGeometryStore.getState().removeObject(obj.id);
      setLines((prev) => [...prev, { id: lineIdCounter++, text: `  已删除 Deleted ${obj.label} (${obj.type})`, color: 'var(--color-warning)' }]);
      return;
    }

    // Parse "move <label> to (<x>, <y>)"
    const moveMatch = cmd.match(/^move\s+(\w+)\s+to\s*\(\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*\)$/i);
    if (moveMatch) {
      const pt = findPointByLabel(moveMatch[1]);
      if (!pt) {
        setLines((prev) => [...prev, { id: lineIdCounter++, text: `  错误 Error: point "${moveMatch[1]}" not found`, color: 'var(--color-danger)' }]);
        return;
      }
      const x = parseFloat(moveMatch[2]);
      const y = parseFloat(moveMatch[3]);
      useGeometryStore.getState().moveObject(pt.id, x, y);
      setLines((prev) => [...prev, { id: lineIdCounter++, text: `  已移动 Moved ${pt.label} to (${x}, ${y})`, color: 'var(--color-success)' }]);
      return;
    }

    // Parse "select <labels>"
    const selectMatch = cmd.match(/^select\s+(.+)$/i);
    if (selectMatch) {
      const labels = selectMatch[1].split(',').map((s) => s.trim()).filter(Boolean);
      const s = useGeometryStore.getState();
      const ids: string[] = [];
      for (const label of labels) {
        const obj = s.objects.find((o) => o.label.toLowerCase() === label.toLowerCase());
        if (obj) ids.push(obj.id);
      }
      useGeometryStore.getState().selectObjects(ids);
      setLines((prev) => [...prev, { id: lineIdCounter++, text: `  已选择 Selected ${ids.length} object(s): ${labels.join(', ')}`, color: 'var(--color-success)' }]);
      return;
    }

    // Unknown command
    setLines((prev) => [...prev, { id: lineIdCounter++, text: `  未知命令 Unknown command: "${cmd}". 输入 help 查看命令 Type 'help' for commands.`, color: 'var(--color-danger)' }]);
  }, [clearLines]);

  // Key handlers
  const handleKeyDown = useCallback((e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === 'Enter') {
      execCommand(input);
      setInput('');
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
      clearLines();
    }
  }, [input, history, historyIdx, execCommand, complete, clearLines]);

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
          onClick={clearLines}
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

let lineIdCounter = 10;
