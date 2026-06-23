// L1-Base / visual.ts — 设计令牌（Design Tokens）
// Trust颜色 + 10模块色 + 字体 + 间距 + 布局常量

export const TRUST = {
  GREEN:        '#3fb950',
  BLUE:         '#58a6ff',
  GREY:         '#8b949e',
  RED:          '#f85149',
  ORANGE:       '#f0883e',
  YELLOW:       '#d29922',
  DARK_ORANGE:  '#db6d28',
} as const;

export const MODULE_COLORS = {
  formula:   { base: '#58a6ff', hover: '#79c0ff', deep: '#388bfd', label: '公式' },
  graph:     { base: '#3fb950', hover: '#56d364', deep: '#2ea043', label: '图' },
  block:     { base: '#bc8cff', hover: '#d2a8ff', deep: '#8957e5', label: '函数块' },
  proof:     { base: '#d2a8ff', hover: '#bc8cff', deep: '#8957e5', label: '证明' },
  type:      { base: '#39d2c0', hover: '#56d4c8', deep: '#2ba89a', label: '类型' },
  recurse:   { base: '#d29922', hover: '#e3b341', deep: '#9e7a1f', label: '递归' },
  engine:    { base: '#00bcd4', hover: '#26c6da', deep: '#0097a7', label: '引擎' },
  debug:     { base: '#f0883e', hover: '#f5a623', deep: '#c25d0e', label: '调试' },
  help:      { base: '#8b949e', hover: '#a5acb5', deep: '#6e7681', label: '帮助' },
} as const;

export function trustColor(name: keyof typeof TRUST): string {
  return TRUST[name];
}

export function moduleColor(key: string): string {
  return (MODULE_COLORS as Record<string, { base: string }>)[key]?.base ?? '#8b949e';
}

export const FONT = {
  mono: "'JetBrains Mono','Fira Code','Consolas','Monaco','Courier New',monospace",
  sans: "-apple-system,BlinkMacSystemFont,'Segoe UI','Noto Sans SC',sans-serif",
} as const;

export const LAYOUT = {
  headerHeight: 40,
  statusBarHeight: 26,
  sidebarWidth: 280,
  sidebarMin: 200,
  sidebarMax: 400,
  resizeHandleWidth: 5,
} as const;

export const ICONS = {
  FORMULA:   'F',
  GRAPH:     'G',
  BLOCK:     'B',
  PROOF:     'P',
  TYPE:      'T',
  RECURSE:   'R',
  ENGINE:    'E',
  DEBUG:     'D',
  HELP:      'H',
  CANVAS:    '\u2B21',
  TEXT:      '\uD83D\uDCDD',
  TABLE:     '\uD83D\uDCCA',
  TREE:      '\uD83C\uDF33',
  TERMINAL:  '\u25B8',
  TOPOLOGY:  '\uD83D\uDD32',
  UNDO:      '\u21B6',
  REDO:      '\u21B7',
  NORMALIZE: '\u22C6',
  FIT:       '\uD83D\uDD0D',
} as const;
