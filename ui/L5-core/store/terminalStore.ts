import { create } from 'zustand';
import { TerminalLine, CommandHistory } from '../types';

let lineId = 0;
let cmdId = 0;

interface TerminalState {
  lines: TerminalLine[];
  history: CommandHistory[];
  completions: string[];

  addLine: (text: string, color?: string) => void;
  clearLines: () => void;
  addHistory: (input: string, output: string, success: boolean) => void;
  setCompletions: (c: string[]) => void;
}

export const useTerminalStore = create<TerminalState>((set) => ({
  lines: [
    { id: ++lineId, text: '\u2554\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2557', color: 'var(--color-text-secondary)' },
    { id: ++lineId, text: '\u2551  Lv-00 Terminal v3.0                        \u2551', color: 'var(--color-text-secondary)' },
    { id: ++lineId, text: '\u2551  Type help for commands                      \u2551', color: 'var(--color-text-secondary)' },
    { id: ++lineId, text: '\u255a\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u255d', color: 'var(--color-text-secondary)' },
    { id: ++lineId, text: '', color: 'var(--color-text-primary)' },
  ],
  history: [],
  completions: [],

  addLine: (text, color = 'var(--color-text-primary)') => set((s) => ({
    lines: [...s.lines, { id: ++lineId, text, color }],
  })),
  clearLines: () => set({ lines: [] }),
  addHistory: (input, output, success) => set((s) => ({
    history: [{ input, output, success, timestamp: Date.now(), id: ++cmdId }, ...s.history.slice(0, 99)],
  })),
  setCompletions: (completions) => set({ completions }),
}));
