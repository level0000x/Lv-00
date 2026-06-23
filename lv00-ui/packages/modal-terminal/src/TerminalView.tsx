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
      scrollba