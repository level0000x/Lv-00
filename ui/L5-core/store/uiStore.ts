import { create } from 'zustand';
import { Theme, ToastItem, LogEntry, ModalConfig, PanelState, ResizeState, PerfStats, ModuleKey, CanvasTool, ConnectionState } from '../types';

let toastId = 0;
let logId = 0;

interface UIState {
  theme: Theme;
  activeModule: ModuleKey;
  tool: CanvasTool;
  backend: 'wasm' | 'js' | null;
  graphHandle: number | null;
  minLogLevel: 'info' | 'warn' | 'error';
  logs: LogEntry[];
  toasts: ToastItem[];
  modal: ModalConfig | null;
  perfStats: PerfStats;
  panelStates: Record<string, boolean>;
  leftSidebarWidth: number;
  rightSidebarWidth: number;
  resizeState: ResizeState | null;
  statusMessage: string;
  connectionState: ConnectionState;

  formulaInput: string;
  formulaOutputFormat: 'latex' | 'python' | 'dsl';

  setTheme: (t: Theme) => void;
  setActiveModule: (m: ModuleKey) => void;
  setTool: (t: CanvasTool) => void;
  setBackend: (b: 'wasm' | 'js' | null) => void;
  setGraphHandle: (h: number | null) => void;
  setMinLogLevel: (l: 'info' | 'warn' | 'error') => void;
  appendLog: (message: string, level: LogEntry['level']) => void;
  clearLogs: () => void;
  addToast: (variant: ToastItem['variant'], message: string, duration?: number) => void;
  removeToast: (id: string) => void;
  showModal: (config: ModalConfig) => void;
  hideModal: () => void;
  updatePerfStats: (stats: Partial<PerfStats>) => void;
  togglePanel: (id: string) => void;
  setPanelCollapsed: (id: string, collapsed: boolean) => void;
  setLeftSidebarWidth: (w: number) => void;
  setRightSidebarWidth: (w: number) => void;
  setResizeState: (s: ResizeState | null) => void;
  setStatusMessage: (m: string) => void;
  setConnectionState: (s: ConnectionState) => void;
  setFormulaInput: (i: string) => void;
  setFormulaOutputFormat: (f: 'latex' | 'python' | 'dsl') => void;
}

export const useUIStore = create<UIState>((set) => ({
  theme: 'dark',
  activeModule: 'canvas',
  tool: 'select',
  backend: null,
  graphHandle: null,
  minLogLevel: 'info',
  logs: [],
  toasts: [],
  modal: null,
  perfStats: { fps: 0, renderCount: 0, avgRenderTime: 0, lastFpsUpdate: 0 },
  panelStates: {},
  leftSidebarWidth: 280,
  rightSidebarWidth: 280,
  resizeState: null,
  statusMessage: 'Ready',
  connectionState: 'disconnected',
  formulaInput: '',
  formulaOutputFormat: 'dsl',

  setTheme: (theme) => set({ theme }),
  setActiveModule: (activeModule) => set({ activeModule }),
  setTool: (tool) => set({ tool }),
  setBackend: (backend) => set({ backend }),
  setGraphHandle: (graphHandle) => set({ graphHandle }),
  setMinLogLevel: (minLogLevel) => set({ minLogLevel }),
  appendLog: (message, level) => set((s) => ({
    logs: [...s.logs.slice(-500), { id: String(++logId), timestamp: Date.now(), level, message }],
  })),
  clearLogs: () => set({ logs: [] }),
  addToast: (variant, message, duration = 3000) => {
    const id = String(++toastId);
    set((s) => ({ toasts: [...s.toasts, { id, variant, message, duration }] }));
    if (duration > 0) setTimeout(() => { useUIStore.getState().removeToast(id); }, duration);
  },
  removeToast: (id) => set((s) => ({ toasts: s.toasts.filter((t) => t.id !== id) })),
  showModal: (modal) => set({ modal }),
  hideModal: () => set({ modal: null }),
  updatePerfStats: (stats) => set((s) => ({ perfStats: { ...s.perfStats, ...stats } })),
  togglePanel: (id) => set((s) => ({ panelStates: { ...s.panelStates, [id]: !s.panelStates[id] } })),
  setPanelCollapsed: (id, collapsed) => set((s) => ({ panelStates: { ...s.panelStates, [id]: collapsed } })),
  setLeftSidebarWidth: (leftSidebarWidth) => set({ leftSidebarWidth }),
  setRightSidebarWidth: (rightSidebarWidth) => set({ rightSidebarWidth }),
  setResizeState: (resizeState) => set({ resizeState }),
  setStatusMessage: (statusMessage) => set({ statusMessage }),
  setConnectionState: (connectionState) => set({ connectionState }),
  setFormulaInput: (formulaInput) => set({ formulaInput }),
  setFormulaOutputFormat: (formulaOutputFormat) => set({ formulaOutputFormat }),
}));
