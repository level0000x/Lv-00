/**
 * @module stores/canvasStore
 * @description 画布视图状态管理。
 *              管理画布的缩放、平移、DPR、渲染尺寸以及显示选项（网格、坐标轴、标签）。
 *              从原先 stores/index.ts（~850 行单体 Store）拆分而来，遵循单一职责原则。
 */

import { create } from 'zustand';
import type { ThemeColors } from '@/types';
import { SCALE_MIN, SCALE_MAX } from '@/utils/constants';

// ================================================================
// 画布视图状态接口 / Canvas View State Interface
// ================================================================

/**
 * 画布视图相关状态。
 * 所有坐标变换和显示配置集中管理在此 Store 中。
 */
export interface CanvasState {
  // ---- 视口变换 / Viewport Transform ----
  /** 画布缩放比例（0.01 ~ 10000） */
  scale: number;
  /** 画布水平平移偏移（世界坐标） */
  offsetX: number;
  /** 画布垂直平移偏移（世界坐标） */
  offsetY: number;
  /** 设备像素比，用于高 DPI 渲染 */
  dpr: number;
  /** Canvas 元素的实际渲染宽度（CSS 像素），用于精确坐标转换 */
  canvasWidth: number;
  /** Canvas 元素的实际渲染高度（CSS 像素） */
  canvasHeight: number;

  // ---- 显示选项 / Display Options ----
  /** 是否显示参考网格 */
  showGrid: boolean;
  /** 是否显示坐标轴 */
  showAxes: boolean;
  /** 是否显示点的标签文字 */
  showLabels: boolean;

  // ---- 主题颜色 / Theme Colors ----
  /**
   * 当前主题对应的画布渲染颜色方案。
   * 由 useTheme hook 在主题切换时同步更新。
   */
  themeColors: ThemeColors;

  // ---- Actions / 操作方法 ----
  /** 设置画布缩放比例，自动钳制到 [0.01, 10000] 范围 */
  setScale: (scale: number) => void;
  /** 设置画布平移偏移 */
  setOffset: (offsetX: number, offsetY: number) => void;
  /** 设置设备像素比 */
  setDpr: (dpr: number) => void;
  /** 同步 Canvas 元素的实际渲染尺寸（用于视口坐标计算） */
  setCanvasSize: (width: number, height: number) => void;
  /** 切换参考网格的显示/隐藏 */
  toggleGrid: () => void;
  /** 切换坐标轴的显示/隐藏 */
  toggleAxes: () => void;
  /** 切换点标签的显示/隐藏 */
  toggleLabels: () => void;
  /** 设置主题颜色方案 */
  setThemeColors: (colors: ThemeColors) => void;
  /** 重置视图：缩放=1，偏移归零，显示选项保持默认 */
  resetView: () => void;
}

// ================================================================
// 默认主题颜色 / Default Theme Colors
// ================================================================

/** 深色主题默认画布颜色方案 */
const DARK_THEME_COLORS: ThemeColors = {
  canvasBg: '#0d1117',
  grid: '#21262d',
  axis: '#484f58',
  point: '#58a6ff',
  pointSelected: '#f0883e',
  pointHover: '#79c0ff',
  segment: '#3fb950',
  text: '#c9d1d9',
  port: '#79c0ff',
  portHover: '#58a6ff',
  funcBlockFill: 'rgba(88, 166, 255, 0.1)',
  funcBlockStroke: '#58a6ff',
  funcBlockText: '#c9d1d9',
};

/** 浅色主题默认画布颜色方案 */
const LIGHT_THEME_COLORS: ThemeColors = {
  canvasBg: '#ffffff',
  grid: '#e1e4e8',
  axis: '#8b949e',
  point: '#0969da',
  pointSelected: '#cf222e',
  pointHover: '#58a6ff',
  segment: '#1a7f37',
  text: '#24292f',
  port: '#0969da',
  portHover: '#58a6ff',
  funcBlockFill: 'rgba(9, 105, 218, 0.08)',
  funcBlockStroke: '#0969da',
  funcBlockText: '#24292f',
};

// ================================================================
// Store 实现 / Store Implementation
// ================================================================

/**
 * 画布视图 Store。
 * 独立管理所有画布渲染相关的状态，不依赖其他 Store。
 *
 * @example
 * ```typescript
 * const scale = useCanvasStore((s) => s.scale);
 * const setScale = useCanvasStore((s) => s.setScale);
 * const { showGrid, toggleGrid } = useCanvasStore();
 * ```
 */
export const useCanvasStore = create<CanvasState>((set) => ({
  // ---- 初始状态 / Initial State ----
  scale: 1,
  offsetX: 0,
  offsetY: 0,
  dpr: typeof window !== 'undefined' ? window.devicePixelRatio || 1 : 1,
  canvasWidth: 0,
  canvasHeight: 0,

  showGrid: true,
  showAxes: true,
  showLabels: true,

  themeColors: DARK_THEME_COLORS,

  // ---- Actions / 操作方法 ----

  /**
   * 设置画布缩放比例。
   * 自动钳制到 [0.01, 10000] 范围，防止数值溢出。
   */
  setScale: (scale) =>
    set({ scale: Math.max(SCALE_MIN, Math.min(SCALE_MAX, scale)) }),

  /**
   * 设置画布平移偏移。
   * @param offsetX - 水平偏移（世界坐标）
   * @param offsetY - 垂直偏移（世界坐标）
   */
  setOffset: (offsetX, offsetY) => set({ offsetX, offsetY }),

  /**
   * 设置设备像素比。
   * 通常在窗口移动/DPI 变化时由 useCanvas hook 调用。
   */
  setDpr: (dpr) => set({ dpr }),

  /**
   * 同步 Canvas 元素的实际渲染尺寸。
   * 用于 viewport 坐标转换（屏幕坐标 ↔ 世界坐标）。
   * @param width - CSS 像素宽度
   * @param height - CSS 像素高度
   */
  setCanvasSize: (width, height) =>
    set({ canvasWidth: width, canvasHeight: height }),

  /** 切换参考网格的显示/隐藏 */
  toggleGrid: () => set((state) => ({ showGrid: !state.showGrid })),

  /** 切换坐标轴的显示/隐藏 */
  toggleAxes: () => set((state) => ({ showAxes: !state.showAxes })),

  /** 切换点标签的显示/隐藏 */
  toggleLabels: () => set((state) => ({ showLabels: !state.showLabels })),

  /**
   * 设置主题颜色方案。
   * 由 useTheme hook 在主题切换时调用，将最新的 ThemeColors 对象写入 Store。
   */
  setThemeColors: (colors) => set({ themeColors: colors }),

  /**
   * 重置视图到初始状态。
   * 缩放=1，偏移归零，显示选项保持当前值。
   */
  resetView: () => set({ scale: 1, offsetX: 0, offsetY: 0 }),
}));

/**
 * 根据主题名称获取对应的默认颜色方案。
 * 供 useTheme hook 使用。
 */
export function getThemeColors(theme: 'dark' | 'light'): ThemeColors {
  return theme === 'light' ? LIGHT_THEME_COLORS : DARK_THEME_COLORS;
}
