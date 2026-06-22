// ============================================================
// @lv00/modal-terminal — M5 终端/REPL (TerminalView)
// Xterm.js 终端 + Lv00 命令解析 + Tab补全 + 历史命令
// ============================================================

import React, { useRef, useEffect, useState, useCallback } from 'react';
import { SceneController } from '@lv00/scene-controller';

// Xterm 类型声明（运行时由 @xterm/xterm 提供）
declare const Terminal: any;
declare const FitAddon: any;

interface TerminalViewProps {
  controller: SceneController;
}

export const TerminalView: React.FC<TerminalViewProps> = ({ controller }) => {
  const containerRef = useRef<HTMLDivElement>(null);
  const termRef = useRef<any>(null);
  const fitRef = useRef<any>(null);
  const commandBuffer = useRef('');
  const historyRef = useRef<string[]>([]);
  const historyIndex = useRef(-1);

  // --- 初始化 ---
  useEffect(() => {
    if (!containerRef.current || typeof Terminal === 'undefined') return;

    const term = new Terminal({
      theme: {
        background: '#0a0a0a',
        foreground: '#c8c8c8',
        cursor: '#4caf50',
        black: '#0a0a0a',
        red: '#f44336',
        green: '#4caf50',
        yellow: '#ff9800',
        blue: '#2196f3',
        magenta: '#9c27b0',
        cyan: '#00bcd4',
        white: '#ffffff',
      },
      fontSize: 14,
      fontFamily: "'Cascadia Code', 'Fira Code', monospace",
      cursorStyle: 'block',
      cursorBlink: true,
      scrollback: 2000,
      allowTransparency: true,
      allowProposedApi: true,
    });

    const fit = new FitAddon();
    term.loadAddon(fit);
    term.open(containerRef.current);
    fit.fit();

    // 欢迎信息
    term.writeln('');
    term.writeln('  ╔══════════════════════════════════════════════╗');
    term.writeln('  ║   Lv-00 几何元语言 终端 v1.0                  ║');
    term.writeln('  ║   输入 help 查看命令列表                       ║');
    term.writeln('  ╚══════════════════════════════════════════════╝');
    term.writeln('');
    writePrompt();

    // --- 键盘输入处理 ---
    term.onKey((e: { key: string; domEvent: KeyboardEvent }) => {
      const { key, domEvent } = e;
      const code = domEvent.key;

      // Ctrl+C
      if (domEvent.ctrlKey && code === 'c') {
        term.write('^C\r\n');
        commandBuffer.current = '';
        writePrompt();
        return;
      }

      // Ctrl+L 清屏
      if (domEvent.ctrlKey && code === 'l') {
        term.clear();
        writePrompt();
        return;
      }

      // Enter
      if (code === 'Enter') {
        const cmd = commandBuffer.current.trim();
        term.write('\r\n');
        if (cmd) {
          historyRef.current.push(cmd);
          historyIndex.current = historyRef.current.length;
          executeCommand(cmd);
        }
        commandBuffer.current = '';
        writePrompt();
        return;
      }

      // Backspace
      if (code === 'Backspace') {
        if (commandBuffer.current.length > 0) {
          commandBuffer.current = commandBuffer.current.slice(0, -1);
          term.write('\b \b');
        }
        return;
      }

      // Tab 补全
      if (code === 'Tab') {
        domEvent.preventDefault();
        const completions = controller.getCommandCompletions(commandBuffer.current);
        if (completions.length === 1) {
          const suffix = completions[0].slice(commandBuffer.current.length) + ' ';
          commandBuffer.current += suffix;
          term.write(suffix);
        } else if (completions.length > 0 && completions.length <= 10) {
          term.write('\r\n');
          completions.forEach(c => term.writeln(`  ${c}`));
          writePrompt();
          term.write(commandBuffer.current);
        }
        return;
      }

      // 方向键上下：历史命令
      if (code === 'ArrowUp') {
        if (historyRef.current.length > 0 && historyIndex.current > 0) {
          historyIndex.current--;
          replaceInput(historyRef.current[historyIndex.current]);
        }
        return;
      }
      if (code === 'ArrowDown') {
        if (historyIndex.current < historyRef.current.length - 1) {
          historyIndex.current++;
          replaceInput(historyRef.current[historyIndex.current]);
        } else {
          historyIndex.current = historyRef.current.length;
          replaceInput('');
        }
        return;
      }

      // 可打印字符
      if (key.length === 1 && key.charCodeAt(0) >= 32) {
        commandBuffer.current += key;
        term.write(key);
      }
    });

    // 窗口大小适配
    const resizeObs = new ResizeObserver(() => { try { fit.fit(); } catch {} });
    resizeObs.observe(containerRef.current);

    termRef.current = term;
    fitRef.current = fit;

    return () => {
      resizeObs.disconnect();
      term.dispose();
    };
  }, [controller]);

  // --- 辅助函数 ---
  const writePrompt = () => { termRef.current?.write('\r\n> '); };

  const replaceInput = (text: string) => {
    const term = termRef.current;
    if (!term) return;
    // 清除当前行
    term.write('\r\x1b[2K> ');
    commandBuffer.current = text;
    term.write(text);
  };

  const executeCommand = (cmd: string) => {
    const term = termRef.current;
    if (!term) return;

    // 内置命令
    if (cmd === 'help') {
      term.writeln('');
      term.writeln('  add point <name> at (<x>, <y>)   添加点');
      term.writeln('  add segment <name> between <a> and <b>   添加线段');
      term.writeln('  add constraint <type>(<a>, <b>, ...)      添加约束');
      term.writeln('  normalize             执行图归一化');
      term.writeln('  undo / redo           撤销/重做');
      term.writeln('  clear / cls           清屏');
      term.writeln('  help                  显示此帮助');
      term.writeln('');
      return;
    }

    if (cmd === 'clear' || cmd === 'cls') {
      term.clear();
      return;
    }

    if (cmd === 'history') {
      historyRef.current.forEach((h, i) => term.writeln(`  ${i}: ${h}`));
      return;
    }

    // 转发给 SceneController
    const result = controller.executeCommandString(cmd);
    if (result.success) {
      term.writeln(`  ✓ ${result.output || '成功'}`);
    } else {
      term.writeln(`  ✗ ${result.error_message || '执行失败'}`);
    }
  };

  return (
    <div ref={containerRef} style={{ width: '100%', height: '100%', minHeight: 200 }} />
  );
};

// 无 Xterm.js 降级终端
export const TerminalViewFallback: React.FC<TerminalViewProps> = ({ controller }) => {
  const [output, setOutput] = useState<string[]>([]);
  const [input, setInput] = useState('');
  const outputRef = useRef<HTMLDivElement>(null);

  const handleEnter = (e: React.KeyboardEvent) => {
    if (e.key !== 'Enter') return;
    const cmd = input.trim();
    if (!cmd) return;

    setOutput(prev => [...prev, `> ${cmd}`]);
    if (cmd === 'help') {
      setOutput(prev => [...prev, '  add point <name> at (<x>, <y>)', '  normalize', '  undo / redo', '  clear / cls']);
    } else if (cmd === 'clear' || cmd === 'cls') {
      setOutput([]);
    } else {
      const result = controller.executeCommandString(cmd);
      setOutput(prev => [...prev, result.success ? `  ✓ ${result.output || '成功'}` : `  ✗ ${result.error_message}`]);
    }
    setInput('');
    setTimeout(() => outputRef.current?.scrollTo(0, outputRef.current.scrollHeight), 50);
  };

  return (
    <div style={{ width: '100%', height: '100%', background: '#0a0a0a', color: '#c8c8c8', fontFamily: 'monospace', fontSize: 13, display: 'flex', flexDirection: 'column' }}>
      <div ref={outputRef} style={{ flex: 1, overflow: 'auto', padding: 8, whiteSpace: 'pre-wrap' }}>
        {output.map((line, i) => <div key={i}>{line}</div>)}
      </div>
      <div style={{ display: 'flex', borderTop: '1px solid #222', padding: 4 }}>
        <span style={{ color: '#4caf50', padding: '4px 6px' }}>&gt;</span>
        <input
          value={input}
          onChange={e => setInput(e.target.value)}
          onKeyDown={handleEnter}
          spellCheck={false}
          style={{
            flex: 1, background: 'transparent', border: 'none', color: '#c8c8c8',
            fontFamily: 'monospace', fontSize: 13, outline: 'none',
          }}
          autoFocus
        />
      </div>
    </div>
  );
};
