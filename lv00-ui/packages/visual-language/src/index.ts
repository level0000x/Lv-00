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
 