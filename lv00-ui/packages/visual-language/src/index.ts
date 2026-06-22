// ============================================================
// @lv00/visual-language — L6 共享视觉语言 v1.0
// 颜色系统、Z-Index 分层、线型、图标、字体、面板布局
// 全模态统一引用，单一事实来源
// ============================================================

// ---- 颜色定义接口 ----

export interface ColorDefinition {
  name: string;
  hex: string;
  rgba: string;
  description: string;
}

// ---- Trust 颜色系统 (10色) ----

export const TRUST_COLORS: Record<string, ColorDefinition> = {
  GREEN: {
    name: 'GREEN',
    hex: '#22C55E',
    rgba: 'rgba(34,197,94,1)',
    description: '全构造——可由公理直接导出',
  },
  LIGHT_GREEN: {
    name: 'LIGHT_GREEN',
    hex: '#86EFAC',
    rgba: 'rgba(134,239,172,1)',
    description: '标准化构造——经归一化后确认',
  },
  YELLOW: {
    name: 'YELLOW',
    hex: '#EAB308',
    rgba: 'rgba(234,179,8,1)',
    description: '约束求解——由求解器确定',
  },
  ORANGE: {
    name: 'ORANGE',
    hex: '#FB923C',
    rgba: 'rgba(251,146,60,1)',
    description: '数值近似——浮点计算得到',
  },
  DARK_ORANGE: {
    name: 'DARK_ORANGE',
    hex: '#EA580C',
    rgba: 'rgba(234,88,12,1)',
    description: '非构造性 + 数值假设',
  },
  RED: {
    name: 'RED',
    hex: '#EF4444',
    rgba: 'rgba(239,68,68,1)',
    description: '矛盾节点——约束冲突',
  },
  GRAY: {
    name: 'GRAY',
    hex: '#9CA3AF',
    rgba: 'rgba(156,163,175,1)',
    description: '未初始化/未知状态',
  },
  BLUE: {
    name: 'BLUE',
    hex: '#3B82F6',
    rgba: 'rgba(59,130,246,1)',
    description: '用户输入——手动构造',
  },
  PURPLE: {
    name: 'PURPLE',
    hex: '#A855F7',
    rgba: 'rgba(168,85,247,1)',
    description: '形式化验证通过',
  },
  CYAN: {
    name: 'CYAN',
    hex: '#06B6D4',
    rgba: 'rgba(6,182,212,1)',
    description: '互操作/FFI 引入',
  },
};

// 将 Trust 名称映射为 32-bit RGBA
export function trustColorToRGBA32(name: string): number {
  const def = TRUST_COLORS[name];
  if (!def) return 0xFF9CA3AF; // 默认 GRAY
  const hex = def.hex;
  const r = parseInt(hex.slice(1, 3), 16);
  const g = parseInt(hex.slice(3, 5), 16);
  const b = parseInt(hex.slice(5, 7), 16);
  return (0xFF << 24) | (r << 16) | (g << 8) | b;
}

// ---- 深色主题 ----

export const DARK_THEME = {
  background: '#0a0a0a',
  panel: '#111111',
  panelHover: '#161616',
  border: '#222222',
  borderFocus: '#4caf50',
  text: '#c8c8c8',
  textDim: '#666666',
  textMuted: '#444444',
  accent: '#4caf50',
  accentDim: '#2e7d32',
  warning: '#ff9800',
  error: '#f44336',
  info: '#2196f3',
} as const;

// ---- 浅色主题（备选） ----

export const LIGHT_THEME = {
  background: '#f5f5f5',
  panel: '#ffffff',
  panelHover: '#f0f0f0',
  border: '#e0e0e0',
  borderFocus: '#4caf50',
  text: '#222222',
  textDim: '#888888',
  textMuted: '#bbbbbb',
  accent: '#4caf50',
  accentDim: '#2e7d32',
  warning: '#ff9800',
  error: '#f44336',
  info: '#2196f3',
} as const;

export type ThemeColors = typeof DARK_THEME;

// ---- Z-Index 分层 ----

export const Z_INDEX = {
  CANVAS_GRID: 1,
  CANVAS_OBJECTS: 2,
  CANVAS_HIGHLIGHT: 5,
  TOOLBAR: 10,
  CONTEXT_MENU: 50,
  TOOLTIP: 100,
  MODAL_OVERLAY: 200,
  MODAL_CONTENT: 1000,
  NOTIFICATION: 2000,
} as const;

// ---- 线型规范 ----

export interface LineStyleDef {
  dashArray: number[];
  width: number;
  color: string;
}

export const LINE_STYLES: Record<string, LineStyleDef> = {
  SOLID: { dashArray: [], width: 2, color: '#c8c8c8' },
  DASHED: { dashArray: [8, 6], width: 1.5, color: '#c8c8c8' },
  DOTTED: { dashArray: [3, 5], width: 1, color: '#888888' },
  THICK_SOLID: { dashArray: [], width: 3, color: '#4caf50' },
  THIN_DASHED: { dashArray: [5, 8], width: 1, color: '#ff9800' },
  ERROR_LINE: { dashArray: [4, 4], width: 2.5, color: '#f44336' },
} as const;

// ---- 字体系统 ----

export const FONTS = {
  mono: "'Cascadia Code', 'Fira Code', 'JetBrains Mono', 'Consolas', monospace",
  sans: "'Segoe UI', 'Inter', -apple-system, BlinkMacSystemFont, sans-serif",
  math: "'KaTeX_Main', 'Times New Roman', serif",
} as const;

export const FONT_SIZES = {
  xs: 10,
  sm: 11,
  md: 13,
  lg: 14,
  xl: 16,
  xxl: 20,
  title: 24,
  hero: 32,
} as const;

// ---- 图标库 (SVG) ----

export const ICONS: Record<string, string> = {
  TOOL_POINT:
    '<svg viewBox="0 0 24 24" width="18" height="18"><circle cx="12" cy="12" r="4" stroke="currentColor" stroke-width="2" fill="none"/></svg>',
  TOOL_LINE:
    '<svg viewBox="0 0 24 24" width="18" height="18"><line x1="4" y1="20" x2="20" y2="4" stroke="currentColor" stroke-width="2"/></svg>',
  TOOL_CIRCLE:
    '<svg viewBox="0 0 24 24" width="18" height="18"><circle cx="12" cy="12" r="8" stroke="currentColor" stroke-width="2" fill="none"/></svg>',
  TOOL_SELECT:
    '<svg viewBox="0 0 24 24" width="18" height="18"><path d="M3 3l6 18 3-8 8-3z" fill="none" stroke="currentColor" stroke-width="2"/></svg>',
  TOOL_PAN:
    '<svg viewBox="0 0 24 24" width="18" height="18"><circle cx="12" cy="12" r="3" fill="currentColor"/><path d="M12 1v6m0 10v6M1 12h6m10 0h6" stroke="currentColor" stroke-width="2"/></svg>',
  TOOL_DELETE:
    '<svg viewBox="0 0 24 24" width="18" height="18"><path d="M3 6h18M8 6V4a2 2 0 012-2h4a2 2 0 012 2v2m3 0v14a2 2 0 01-2 2H7a2 2 0 01-2-2V6h14" fill="none" stroke="currentColor" stroke-width="2"/></svg>',
  UNDO:
    '<svg viewBox="0 0 24 24" width="18" height="18"><path d="M3 7v6h6" stroke="currentColor" stroke-width="2" fill="none"/><path d="M21 17a9 9 0 00-9-9 9 9 0 00-6 2.3L3 13" stroke="currentColor" stroke-width="2" fill="none"/></svg>',
  REDO:
    '<svg viewBox="0 0 24 24" width="18" height="18"><path d="M21 7v6h-6" stroke="currentColor" stroke-width="2" fill="none"/><path d="M3 17a9 9 0 019-9 9 9 0 016 2.3L21 13" stroke="currentColor" stroke-width="2" fill="none"/></svg>',
  ZOOM_IN:
    '<svg viewBox="0 0 24 24" width="18" height="18"><circle cx="11" cy="11" r="7" stroke="currentColor" stroke-width="2" fill="none"/><line x1="21" y1="21" x2="16.65" y2="16.65" stroke="currentColor" stroke-width="2"/><line x1="11" y1="8" x2="11" y2="14" stroke="currentColor" stroke-width="2"/><line x1="8" y1="11" x2="14" y2="11" stroke="currentColor" stroke-width="2"/></svg>',
  ZOOM_OUT:
    '<svg viewBox="0 0 24 24" width="18" height="18"><circle cx="11" cy="11" r="7" stroke="currentColor" stroke-width="2" fill="none"/><line x1="21" y1="21" x2="16.65" y2="16.65" stroke="currentColor" stroke-width="2"/><line x1="8" y1="11" x2="14" y2="11" stroke="currentColor" stroke-width="2"/></svg>',
  FIT_VIEW:
    '<svg viewBox="0 0 24 24" width="18" height="18"><path d="M3 8V3h5M21 8V3h-5M3 16v5h5M21 16v5h-5" stroke="currentColor" stroke-width="2" fill="none"/></svg>',
  NORMALIZE:
    '<svg viewBox="0 0 24 24" width="18" height="18"><circle cx="12" cy="12" r="9" stroke="currentColor" stroke-width="2" fill="none"/><path d="M4 12h16M12 4v16" stroke="currentColor" stroke-width="2"/></svg>',
  SNAPSHOT:
    '<svg viewBox="0 0 24 24" width="18" height="18"><rect x="3" y="3" width="18" height="18" rx="2" stroke="currentColor" stroke-width="2" fill="none"/><circle cx="8.5" cy="8.5" r="1.5" fill="currentColor"/><path d="M21 15l-5-5L5 21" stroke="currentColor" stroke-width="2" fill="none"/></svg>',
  TERMINAL:
    '<svg viewBox="0 0 24 24" width="18" height="18"><path d="M4 17l6-6-6-6M12 19h8" stroke="currentColor" stroke-width="2" fill="none"/></svg>',
  TABLE:
    '<svg viewBox="0 0 24 24" width="18" height="18"><rect x="3" y="3" width="18" height="18" rx="2" stroke="currentColor" stroke-width="2" fill="none"/><line x1="3" y1="9" x2="21" y2="9" stroke="currentColor" stroke-width="2"/><line x1="9" y1="3" x2="9" y2="21" stroke="currentColor" stroke-width="2"/></svg>',
  TREE:
    '<svg viewBox="0 0 24 24" width="18" height="18"><path d="M12 20v-6m0 0l-4 4m4-4l4 4M12 14v-4m0 0l-4 4m4-4l4 4M12 10V4" stroke="currentColor" stroke-width="2" fill="none"/></svg>',
  LAYOUT:
    '<svg viewBox="0 0 24 24" width="18" height="18"><rect x="3" y="3" width="7" height="7" rx="1" stroke="currentColor" stroke-width="2" fill="none"/><rect x="14" y="3" width="7" height="7" rx="1" stroke="currentColor" stroke-width="2" fill="none"/><rect x="3" y="14" width="7" height="7" rx="1" stroke="currentColor" stroke-width="2" fill="none"/><rect x="14" y="14" width="7" height="7" rx="1" stroke="currentColor" stroke-width="2" fill="none"/></svg>',
};

// ---- 面板布局 ----

export const PANEL_LAYOUT = {
  leftSidebar: 260,
  rightSidebar: 260,
  mainAreaFlex: 1,
  paddingX: 14,
  paddingY: 12,
  gap: 8,
  headerHeight: 36,
  statusBarHeight: 28,
} as const;

// ---- 快捷键 ----

export const SHORTCUTS = {
  ADD_PROCESS: 'Enter',
  START_ALL: 'Ctrl+Shift+S',
  STOP_ALL: 'Ctrl+Shift+X',
  UNDO: 'Ctrl+Z',
  REDO: 'Ctrl+Shift+Z',
  SAVE: 'Ctrl+S',
  SELECT_ALL: 'Ctrl+A',
  DELETE: 'Delete',
  ESCAPE: 'Escape',
  TOOL_SELECT: 'V',
  TOOL_POINT: 'P',
  TOOL_LINE: 'L',
  TOOL_PAN: 'H',
  ZOOM_IN: 'Ctrl+=',
  ZOOM_OUT: 'Ctrl+-',
  FIT_VIEW: 'Ctrl+0',
} as const;

// ---- 工具函数 ----

export function hexToRGBA(hex: string, alpha: number = 1): string {
  const r = parseInt(hex.slice(1, 3), 16);
  const g = parseInt(hex.slice(3, 5), 16);
  const b = parseInt(hex.slice(5, 7), 16);
  return `rgba(${r},${g},${b},${alpha})`;
}

export function intToHex32(color: number): string {
  const r = (color >> 16) & 0xFF;
  const g = (color >> 8) & 0xFF;
  const b = color & 0xFF;
  return `#${r.toString(16).padStart(2, '0')}${g.toString(16).padStart(2, '0')}${b.toString(16).padStart(2, '0')}`;
}

// CSS 自定义属性生成器（用于注入全局样式）
export function generateCSSVariables(theme: ThemeColors = DARK_THEME): string {
  return `
    :root {
      --lv00-bg: ${theme.background};
      --lv00-panel: ${theme.panel};
      --lv00-panel-hover: ${theme.panelHover};
      --lv00-border: ${theme.border};
      --lv00-border-focus: ${theme.borderFocus};
      --lv00-text: ${theme.text};
      --lv00-text-dim: ${theme.textDim};
      --lv00-accent: ${theme.accent};
      --lv00-warning: ${theme.warning};
      --lv00-error: ${theme.error};
      --lv00-info: ${theme.info};
      --lv00-font-mono: ${FONTS.mono};
      --lv00-font-sans: ${FONTS.sans};
    }
  `;
}
