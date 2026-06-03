/**
 * @module stores/uiStore
 * @description UI 状态管理。
 *              管理主题、模态框、Toast 通知、面板折叠、侧边栏宽度、后端连接、
 *              日志、性能统计、公式模块和状态栏等全局 UI 状态。
 *              从原先 stores/index.ts（~850 行单体 Store）拆分而来，遵循单一职责原则。
 *
 *              内存安全保障：
 *              - logs: 最多保留 MAX_GLOBAL_LOG_ENTRIES (500) 条，超出自动丢弃最旧条目
 *              - Toast 定时器：duration > 0 时自动移除；组件卸载后定时器回调为 no-op，无泄漏风险
 */

import { create } from 'zustand';
import type {
  ModuleType,
  Theme,
  AppLogLevel, // [安全修复 M-08] 重命名 LogLevel -> AppLogLevel，消除与 logger.ts 的命名冲突
  LogEntry,
  Toast,
  ToastVariant,
  ModalConfig,
  PerfStats,
  ResizeState,
  FormulaSyntax,
  FormulaOutputFormat,
} from '@/types';
import {
  SIDEBAR_MIN_WIDTH,
  SIDEBAR_LEFT_MAX_WIDTH,
  SIDEBAR_RIGHT_MAX_WIDTH,
  TOAST_DURATION_DEFAULT,
  MAX_GLOBAL_LOG_ENTRIES,
} from '@/utils/constants';

// ================================================================
// 辅助：Toast ID 生成器
// ================================================================

let _toastCounter = 0;

function nextToastId(): string {
  return `toast-${++_toastCounter}`;
}

// ================================================================
// UI 状态接口 / UI State Interface
// ================================================================

/**
 * UI 相关全局状态。
 * 涵盖主题、模态框、Toast、面板、侧边栏、后端、日志、性能、公式和状态栏。
 */
export interface UIState {
  // ---- 活跃模块 / Active Module ----
  /** 当前活跃的功能模块面板 */
  activeModule: ModuleType;

  // ---- 主题 / Theme ----
  /** 当前 UI 主题（dark/light） */
  theme: Theme;

  // ---- 后端连接 / Backend ----
  /** 活跃的后端类型 */
  backend: 'wasm' | 'js' | null;
  /** 后端的图句柄 */
  graphHandle: number | null;

  // ---- 日志 / Logging ----
  /** 最小显示日志级别 */
  minLogLevel: AppLogLevel;
  /** 应用日志条目列表 */
  logs: LogEntry[];

  // ---- Toast 通知 / Toast Notifications ----
  /** 活跃的 Toast 通知列表 */
  toasts: Toast[];

  // ---- 模态框 / Modal ----
  /** 活跃的模态框配置（null 表示无模态框） */
  modal: ModalConfig | null;

  // ---- 性能统计 / Performance ----
  /** 渲染性能统计 */
  perfStats: PerfStats;

  // ---- 面板状态 / Panel State ----
  /** 各面板的折叠状态（key = 面板 ID, value = 是否折叠） */
  panelStates: Record<string, boolean>;

  // ---- 侧边栏宽度 / Sidebar Widths ----
  /** 左侧边栏宽度（px），范围 200-400 */
  leftSidebarWidth: number;
  /** 右侧边栏宽度（px），范围 200-360 */
  rightSidebarWidth: number;
  /** 当前活跃的拖拽调整大小状态 */
  resizeState: ResizeState | null;

  // ---- 公式模块 / Formula Module ----
  /** 公式输入文本 */
  formulaInput: string;
  /** 公式语法模式 */
  formulaSyntax: FormulaSyntax;
  /** 公式输出格式 */
  formulaOutputFormat: FormulaOutputFormat;

  // ---- 状态栏 / Status Bar ----
  /** 状态栏消息 */
  statusMessage: string;

  // ---- Actions: 模块与主题 / Module & Theme ----
  /** 设置当前活跃模块 */
  setActiveModule: (module: ModuleType) => void;
  /** 设置主题并同步 DOM class */
  setTheme: (theme: Theme) => void;

  // ---- Actions: 后端 / Backend ----
  /** 设置后端类型 */
  setBackend: (backend: 'wasm' | 'js' | null) => void;
  /** 设置图句柄 */
  setGraphHandle: (handle: number | null) => void;

  // ---- Actions: 日志 / Logging ----
  /** 设置最小日志级别 */
  setMinLogLevel: (level: AppLogLevel) => void;
  /** 追加一条日志 */
  appendLog: (message: string, level: AppLogLevel) => void;
  /** 清空所有日志 */
  clearLogs: () => void;

  // ---- Actions: Toast / Toast Actions ----
  /** 添加一个 Toast 通知 */
  addToast: (variant: ToastVariant, message: string, duration?: number) => void;
  /** 移除指定 ID 的 Toast 通知 */
  removeToast: (id: string) => void;

  // ---- Actions: 模态框 / Modal Actions ----
  /** 显示模态框 */
  showModal: (config: ModalConfig) => void;
  /** 隐藏模态框 */
  hideModal: () => void;

  // ---- Actions: 性能 / Performance Actions ----
  /** 部分更新性能统计 */
  updatePerfStats: (stats: Partial<PerfStats>) => void;

  // ---- Actions: 面板 / Panel Actions ----
  /** 切换面板折叠状态 */
  togglePanel: (id: string) => void;
  /** 设置面板折叠状态 */
  setPanelCollapsed: (id: string, collapsed: boolean) => void;

  // ---- Actions: 侧边栏 / Sidebar Actions ----
  /** 设置左侧边栏宽度（自动钳制到 200-400） */
  setLeftSidebarWidth: (width: number) => void;
  /** 设置右侧边栏宽度（自动钳制到 200-360） */
  setRightSidebarWidth: (width: number) => void;
  /** 设置拖拽调整大小状态 */
  setResizeState: (state: ResizeState | null) => void;

  // ---- Actions: 公式 / Formula Actions ----
  /** 设置公式输入文本 */
  setFormulaInput: (input: string) => void;
  /** 设置公式语法模式 */
  setFormulaSyntax: (syntax: FormulaSyntax) => void;
  /** 设置公式输出格式 */
  setFormulaOutputFormat: (format: FormulaOutputFormat) => void;

  // ---- Actions: 状态栏 / Status Bar ----
  /** 设置状态栏消息 */
  setStatusMessage: (message: string) => void;
}

// ================================================================
// Store 实现 / Store Implementation
// ================================================================

/**
 * UI 全局状态 Store。
 * 管理主题、模态框、Toast、面板、侧边栏、后端、日志、性能、公式和状态栏。
 *
 * @example
 * ```typescript
 * const theme = useUIStore((s) => s.theme);
 * const { addToast, showModal } = useUIStore();
 * ```
 */
export const useUIStore = create<UIState>((set, get) => ({
  // ---- 初始状态 / Initial State ----
  activeModule: 'formula',
  theme: 'dark',

  backend: null,
  graphHandle: null,

  minLogLevel: 'info',
  logs: [],

  toasts: [],

  modal: null,

  perfStats: {
    fps: 0,
    renderCount: 0,
    avgRenderTime: 0,
    lastFpsUpdate: performance.now(),
  },

  panelStates: {},

  leftSidebarWidth: 280,
  rightSidebarWidth: 280,
  resizeState: null,

  formulaInput: '',
  formulaSyntax: 'auto',
  formulaOutputFormat: 'latex',

  statusMessage: 'READY / 就绪',

  // ================================================================
  // Actions: 模块与主题 / Module & Theme
  // ================================================================

  /** 设置当前活跃的功能模块 */
  setActiveModule: (module) => set({ activeModule: module }),

  /**
   * 设置 UI 主题。
   * 同时更新 document.body 的 CSS class，确保 CSS 变量正确应用。
   * - dark 主题：移除 light-theme class
   * - light 主题：添加 light-theme class
   */
  setTheme: (theme) => {
    if (typeof document === 'undefined') return;
    if (theme === 'light') {
      document.body.classList.add('light-theme');
    } else {
      document.body.classList.remove('light-theme');
    }
    set({ theme });
  },

  // ================================================================
  // Actions: 后端 / Backend
  // ================================================================

  /** 设置当前活跃的后端类型 */
  setBackend: (backend) => set({ backend }),

  /** 设置后端图句柄 */
  setGraphHandle: (handle) => set({ graphHandle: handle }),

  // ================================================================
  // Actions: 日志 / Logging
  // ================================================================

  /** 设置最小日志级别 */
  setMinLogLevel: (level) => set({ minLogLevel: level }),

  /**
   * 追加一条日志条目。
   * 自动限制日志总数不超过 MAX_GLOBAL_LOG_ENTRIES 条，防止内存溢出。
   * slice(-(MAX_GLOBAL_LOG_ENTRIES - 1)) 保留最近 499 条旧日志，
   * 外加 1 条新日志，始终保持总共 500 条。
   */
  appendLog: (message, level) =>
    set((state) => ({
      logs: [
        ...state.logs.slice(-(MAX_GLOBAL_LOG_ENTRIES - 1)),
        {
          timestamp: new Date().toISOString(),
          level,
          message,
        },
      ],
    })),

  /** 清空所有日志 */
  clearLogs: () => set({ logs: [] }),

  // ================================================================
  // Actions: Toast / Toast Actions
  // ================================================================

  /**
   * 添加一个 Toast 通知。
   * 如果 duration > 0，将在指定毫秒后自动移除。
   *
   * @param variant - Toast 类型（success/error/warning/info）
   * @param message - 通知消息
   * @param duration - 自动消失时间（毫秒），默认 3000，设为 0 则不会自动消失
   */
  addToast: (variant, message, duration = TOAST_DURATION_DEFAULT) => {
    const id = nextToastId();
    set((state) => ({
      toasts: [...state.toasts, { id, variant, message, duration }],
    }));
    if (duration > 0) {
      setTimeout(() => {
        get().removeToast(id);
      }, duration);
    }
  },

  /** 移除指定 ID 的 Toast */
  removeToast: (id) =>
    set((state) => ({
      toasts: state.toasts.filter((t) => t.id !== id),
    })),

  // ================================================================
  // Actions: 模态框 / Modal Actions
  // ================================================================

  /** 显示模态框 */
  showModal: (modal) => set({ modal }),

  /** 隐藏模态框 */
  hideModal: () => set({ modal: null }),

  // ================================================================
  // Actions: 性能 / Performance Actions
  // ================================================================

  /** 部分更新性能统计（使用对象展开合并） */
  updatePerfStats: (stats) =>
    set((state) => ({
      perfStats: { ...state.perfStats, ...stats },
    })),

  // ================================================================
  // Actions: 面板 / Panel Actions
  // ================================================================

  /** 切换指定面板的折叠/展开状态 */
  togglePanel: (id) =>
    set((state) => ({
      panelStates: {
        ...state.panelStates,
        [id]: !state.panelStates[id],
      },
    })),

  /** 设置指定面板的折叠状态 */
  setPanelCollapsed: (id, collapsed) =>
    set((state) => ({
      panelStates: { ...state.panelStates, [id]: collapsed },
    })),

  // ================================================================
  // Actions: 侧边栏 / Sidebar Actions
  // ================================================================

  /** 设置左侧边栏宽度（自动钳制到 SIDEBAR_MIN_WIDTH-SIDEBAR_LEFT_MAX_WIDTH） */
  setLeftSidebarWidth: (width) =>
    set({ leftSidebarWidth: Math.max(SIDEBAR_MIN_WIDTH, Math.min(SIDEBAR_LEFT_MAX_WIDTH, width)) }),

  /** 设置右侧边栏宽度（自动钳制到 SIDEBAR_MIN_WIDTH-SIDEBAR_RIGHT_MAX_WIDTH） */
  setRightSidebarWidth: (width) =>
    set({ rightSidebarWidth: Math.max(SIDEBAR_MIN_WIDTH, Math.min(SIDEBAR_RIGHT_MAX_WIDTH, width)) }),

  /** 设置拖拽调整大小状态 */
  setResizeState: (resizeState) => set({ resizeState }),

  // ================================================================
  // Actions: 公式 / Formula Actions
  // ================================================================

  /** 设置公式输入文本 */
  setFormulaInput: (input) => set({ formulaInput: input }),

  /** 设置公式语法模式 */
  setFormulaSyntax: (syntax) => set({ formulaSyntax: syntax }),

  /** 设置公式输出格式 */
  setFormulaOutputFormat: (format) => set({ formulaOutputFormat: format }),

  // ================================================================
  // Actions: 状态栏 / Status Bar
  // ================================================================

  /** 设置状态栏消息 */
  setStatusMessage: (message) => set({ statusMessage: message }),
}));
