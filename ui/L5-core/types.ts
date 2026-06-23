// L5-Core / types.ts — 纯UI类型定义
// 零几何依赖，只定义UI自身的数据结构和状态

export type Theme = 'dark' | 'light';

export type ToastVariant = 'success' | 'error' | 'warning' | 'info';
export interface ToastItem {
  id: string;
  variant: ToastVariant;
  message: string;
  duration: number;
}

export type LogLevel = 'debug' | 'info' | 'warn' | 'error';
export interface LogEntry {
  id: string;
  timestamp: number;
  level: LogLevel;
  message: string;
}

export type BadgeType = 'running' | 'completed' | 'failed' | 'total' | 'timeout' | 'pending';
export type StatusDotType = 'running' | 'completed' | 'failed' | 'pending';
export type ConnectionState = 'connected' | 'disconnected' | 'connecting';

export interface ModalConfig {
  id: string;
  title: string;
  content: string;
  confirmLabel?: string;
  cancelLabel?: string;
  danger?: boolean;
  onConfirm?: () => void;
  onCancel?: () => void;
}

export interface ContextMenuAction {
  id: string;
  label: string;
  shortcut?: string;
}

export interface ContextMenuState {
  x: number;
  y: number;
  items: ContextMenuAction[];
}

export interface PanelState {
  id: string;
  collapsed: boolean;
}

export interface ResizeState {
  sidebar: 'left' | 'right';
  startX: number;
  startWidth: number;
}

export interface PerfStats {
  fps: number;
  renderCount: number;
  avgRenderTime: number;
  lastFpsUpdate: number;
}

export type ModuleKey =
  | 'formula' | 'graph' | 'block' | 'proof' | 'type'
  | 'recurse' | 'engine' | 'debug' | 'help'
  | 'canvas' | 'text' | 'table' | 'tree' | 'terminal' | 'topology';

export interface ModuleTab {
  key: ModuleKey;
  icon: string;
  label: string;
  color: string;
}

export type CanvasTool = 'select' | 'point' | 'segment' | 'pan' | 'region' | 'probe';

export interface Viewport {
  offsetX: number;
  offsetY: number;
  scale: number;
  canvasWidth: number;
  canvasHeight: number;
}

export interface TerminalLine {
  id: number;
  text: string;
  color: string;
}

export interface CommandHistory {
  input: string;
  output: string;
  success: boolean;
  timestamp: number;
}
