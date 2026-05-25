/**
 * @module utils/designTokens
 * @description 设计系统令牌 —— 集中管理 UI 颜色、间距、字体等设计常量
 *
 * 【使用说明】
 * 所有 UI 样式应使用此模块中定义的设计令牌，
 * 而非硬编码魔法数字，以保证视觉一致性。
 *
 * 【命名规范】
 * - 颜色: COLOR_* 前缀
 * - 间距: SPACING_* 前缀
 * - 字体大小: FONT_SIZE_* 前缀
 * - 圆角: BORDER_RADIUS_* 前缀
 * - 阴影: SHADOW_* 前缀
 * - Z-Index: Z_INDEX_* 前缀
 *
 * 【优化说明】v3.4.2
 * - 添加完整的 TypeScript 类型定义
 * - 支持 CSS 变量导出
 */

/**
 * 主题类型
 */
export type ThemeType = 'light' | 'dark';

/**
 * 颜色系统
 */
export const COLORS = {
  // 主色调
  PRIMARY: {
    50: '#eff6ff',
    100: '#dbeafe',
    200: '#bfdbfe',
    300: '#93c5fd',
    400: '#60a5fa',
    500: '#3b82f6',
    600: '#2563eb',
    700: '#1d4ed8',
    800: '#1e40af',
    900: '#1e3a8a',
  },

  // 功能色
  SUCCESS: '#51cf66',
  WARNING: '#fcc419',
  ERROR: '#ff6b6b',
  INFO: '#4dabf7',

  // 语义色
  TEXT_PRIMARY: '#1f2937',
  TEXT_SECONDARY: '#6b7280',
  TEXT_DISABLED: '#9ca3af',
  TEXT_INVERSE: '#ffffff',

  // 背景色
  BACKGROUND: {
    DEFAULT: '#ffffff',
    SECONDARY: '#f3f4f6',
    TERTIARY: '#e5e7eb',
    HOVER: '#f9fafb',
    ACTIVE: '#e5e7eb',
  },

  // 边框色
  BORDER: {
    DEFAULT: '#d1d5db',
    FOCUS: '#3b82f6',
    ERROR: '#ef4444',
  },

  // 信任颜色（用于证明系统）
  TRUST: {
    GREEN: '#10b981',     // 绿色：完全构造
    BLUE: '#3b82f6',      // 蓝色：资源受限/未探索
    YELLOW: '#f59e0b',    // 黄色：条件性构造
    ORANGE: '#f97316',    // 橙色：非构造性依赖
    AMBER: '#d97706',     // 琥珀：含数值假设
    RED: '#ef4444',       // 红色：冲突/错误
  },

  // 流式事件类别颜色
  STREAM: {
    ENGINE: '#8b5cf6',      // 紫色：引擎事件
    NORMALIZE: '#06b6d4',   // 青色：归一化
    REWRITE: '#f59e0b',     // 橙色：重写
    SOLVE: '#10b981',       // 绿色：求解
    PROOF: '#3b82f6',      // 蓝色：证明
    FUNC_BLOCK: '#ec4899',  // 粉色：函数块
    CONFLICT: '#ef4444',    // 红色：冲突
    INFO: '#6b7280',       // 灰色：信息
  },

  // 几何元素颜色
  GEOMETRY: {
    POINT: '#1f2937',       // 点：深灰
    SEGMENT: '#374151',     // 线段：中灰
    REGION: '#93c5fd',      // 区域：浅蓝
    CURVE: '#6b7280',       // 曲线：灰
  },

  // 透明度辅助
  OVERLAY: {
    LIGHT: 'rgba(255, 255, 255, 0.8)',
    DARK: 'rgba(0, 0, 0, 0.5)',
  },
} as const;

/**
 * 间距系统（基于 4px 网格）
 */
export const SPACING = {
  /** 0px */
  NONE: 0,
  /** 2px */
  XXS: '2px',
  /** 4px */
  XS: '4px',
  /** 8px */
  SM: '8px',
  /** 12px */
  MD: '12px',
  /** 16px */
  LG: '16px',
  /** 24px */
  XL: '24px',
  /** 32px */
  XXL: '32px',
  /** 48px */
  XXXL: '48px',
  /** 64px */
  HUGE: '64px',
} as const;

/**
 * 字体大小系统
 */
export const FONT_SIZE = {
  /** 10px - 极小标签 */
  XXS: '10px',
  /** 12px - 小标签、注释 */
  XS: '12px',
  /** 14px - 正文小 */
  SM: '14px',
  /** 16px - 正文 */
  BASE: '16px',
  /** 18px - 标题小 */
  LG: '18px',
  /** 20px - 标题 */
  XL: '20px',
  /** 24px - 大标题 */
  XXL: '24px',
  /** 30px - 特大标题 */
  XXXL: '30px',
} as const;

/**
 * 圆角系统
 */
export const BORDER_RADIUS = {
  /** 0px - 无圆角 */
  NONE: 0,
  /** 2px - 小圆角 */
  SM: '2px',
  /** 4px - 默认圆角 */
  DEFAULT: '4px',
  /** 6px - 中圆角 */
  MD: '6px',
  /** 8px - 大圆角 */
  LG: '8px',
  /** 12px - 特大圆角 */
  XL: '12px',
  /** 9999px - 全圆/药丸形 */
  FULL: '9999px',
} as const;

/**
 * 阴影系统
 */
export const SHADOW = {
  /** 无阴影 */
  NONE: 'none',
  /** 小阴影：卡片、按钮 */
  SM: '0 1px 2px 0 rgba(0, 0, 0, 0.05)',
  /** 默认阴影：浮层 */
  DEFAULT: '0 1px 3px 0 rgba(0, 0, 0, 0.1), 0 1px 2px 0 rgba(0, 0, 0, 0.06)',
  /** 中阴影：下拉菜单 */
  MD: '0 4px 6px -1px rgba(0, 0, 0, 0.1), 0 2px 4px -1px rgba(0, 0, 0, 0.06)',
  /** 大阴影：模态框 */
  LG: '0 10px 15px -3px rgba(0, 0, 0, 0.1), 0 4px 6px -2px rgba(0, 0, 0, 0.05)',
  /** 特大阴影：工具提示 */
  XL: '0 20px 25px -5px rgba(0, 0, 0, 0.1), 0 10px 10px -5px rgba(0, 0, 0, 0.04)',
  /** 内阴影 */
  INNER: 'inset 0 2px 4px 0 rgba(0, 0, 0, 0.06)',
} as const;

/**
 * Z-Index 层级系统
 */
export const Z_INDEX = {
  /** 默认 - 基础元素 */
  DEFAULT: 0,
  /** 工具栏 */
  TOOLBAR: 10,
  /** 下拉菜单 */
  DROPDOWN: 100,
  /** 粘性元素 */
  STICKY: 200,
  /** 固定元素 */
  FIXED: 300,
  /** 模态框背景 */
  MODAL_BACKDROP: 400,
  /** 模态框 */
  MODAL: 500,
  /** 弹出框 */
  POPOVER: 600,
  /** 工具提示 */
  TOOLTIP: 700,
  /** Toast 通知 */
  TOAST: 800,
  /** 最高层 */
  MAX: 9999,
} as const;

/**
 * 动画时长系统
 */
export const TRANSITION = {
  /** 快速：hover 状态 */
  FAST: '100ms',
  /** 正常：默认过渡 */
  NORMAL: '200ms',
  /** 慢速：复杂动画 */
  SLOW: '300ms',
  /** 弹跳：特殊效果 */
  BOUNCE: '400ms',
} as const;

/**
 * 动画缓动函数
 */
export const EASING = {
  /** 线性 */
  LINEAR: 'linear',
  /** 默认 */
  DEFAULT: 'ease-in-out',
  /** 进入 */
  IN: 'ease-in',
  /** 退出 */
  OUT: 'ease-out',
  /** 弹性 */
  BOUNCE: 'cubic-bezier(0.68, -0.55, 0.265, 1.55)',
} as const;

/**
 * 设计令牌 CSS 变量导出
 *
 * 将设计令牌导出为 CSS 变量格式，供样式文件使用
 */
export function exportAsCssVariables(): string {
  const lines: string[] = [
    ':root {',
    '  /* 颜色系统 */',
  ];

  // 展开嵌套对象
  const flattenObject = (obj: Record<string, unknown>, prefix: string): void => {
    for (const [key, value] of Object.entries(obj)) {
      const varName = `--${prefix}-${key}`.toLowerCase();
      if (typeof value === 'object' && value !== null) {
        flattenObject(value as Record<string, unknown>, `${prefix}-${key}`);
      } else {
        lines.push(`  ${varName}: ${value};`);
      }
    }
  };

  flattenObject(COLORS as unknown as Record<string, unknown>, 'color');
  flattenObject(SPACING as unknown as Record<string, unknown>, 'spacing');
  flattenObject(FONT_SIZE as unknown as Record<string, unknown>, 'font-size');
  flattenObject(BORDER_RADIUS as unknown as Record<string, unknown>, 'radius');
  flattenObject(SHADOW as unknown as Record<string, unknown>, 'shadow');
  flattenObject(Z_INDEX as unknown as Record<string, unknown>, 'z');

  lines.push('}');

  return lines.join('\n');
}

// 导出所有设计令牌作为默认对象
export const DESIGN_TOKENS = {
  COLORS,
  SPACING,
  FONT_SIZE,
  BORDER_RADIUS,
  SHADOW,
  Z_INDEX,
  TRANSITION,
  EASING,
} as const;
