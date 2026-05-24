/**
 * @module stores
 * @description Zustand 状态管理统一导出入口。
 *
 *              原先 ~850 行的单体 Store（stores/index.ts）已按单一职责原则拆分为：
 *              - canvasStore  : 画布视图状态（缩放、平移、DPR、显示选项、主题颜色）
 *              - geometryStore: 几何数据 + 撤销/重做历史
 *              - interactionStore: 工具、选择、拖拽、鼠标坐标、右键菜单、搜索
 *              - aiStore      : AI 助手聊天、流式事件、模型参数
 *              - uiStore      : 主题、模态框、Toast、面板、侧边栏、后端、日志等
 *              - aiService    : AI 模拟响应逻辑（从 Store 中分离）
 *
 *              本文件提供两层导出：
 *              1. 独立 Store Hook（推荐新代码使用）：
 *                 useCanvasStore, useGeometryStore, useInteractionStore,
 *                 useAIStore, useUIStore
 *              2. 兼容性 useAppStore（保持向后兼容）：
 *                 聚合所有子 Store 的状态和操作，签名与原单体 Store 完全一致。
 *                 现有 28 个引用 useAppStore 的组件无需任何修改。
 */

import { create } from 'zustand';
import type {
  Point,
  Segment,
  Constraint,
  Region,
  ToolType,
  ModuleType,
  Theme,
  ThemeColors,
  LogLevel,
  LogEntry,
  Toast,
  ToastVariant,
  ContextMenuState,
  ModalConfig,
  PerfStats,
  UndoSnapshot,
  FormulaSyntax,
  FormulaOutputFormat,
  ResizeState,
  StreamingEntry,
  ChatMessage,
  EngineStreamEvent,
  EngineStreamCategoryFilter,
} from '@/types';

// ================================================================
// 导入子 Store / Import Sub-Stores
// ================================================================

import { useCanvasStore } from './canvasStore';
import { useGeometryStore } from './geometryStore';
import { useInteractionStore } from './interactionStore';
import { useAIStore } from './aiStore';
import { useUIStore } from './uiStore';

// ================================================================
// 导出子 Store / Re-export Sub-Stores
// ================================================================

export { useCanvasStore, getThemeColors } from './canvasStore';
export type { CanvasState } from './canvasStore';

export { useGeometryStore } from './geometryStore';
export type { GeometryState } from './geometryStore';

export { useInteractionStore } from './interactionStore';
export type { InteractionState } from './interactionStore';

export { useAIStore } from './aiStore';
export type { AIState } from './aiStore';

export { useUIStore } from './uiStore';
export type { UIState } from './uiStore';

// ================================================================
// 应用状态聚合接口 / Aggregated AppState Interface
// ================================================================

/**
 * 完整的应用状态接口（聚合所有子 Store），
 * 与原 stores/index.ts 中的 AppState 接口保持完全一致。
 */
interface AppState {
  // ---- 活跃模块与主题 / Active Module & Theme (uiStore) ----
  activeModule: ModuleType;
  theme: Theme;
  /** 当前画布工具 */
  tool: ToolType;

  // ---- 几何数据 / Geometry Data (geometryStore) ----
  points: Point[];
  segments: Segment[];
  constraints: Constraint[];
  regions: Region[];

  // ---- 画布视图状态 / Canvas View State (canvasStore) ----
  scale: number;
  offsetX: number;
  offsetY: number;
  dpr: number;
  canvasWidth: number;
  canvasHeight: number;

  // ---- 选择状态 / Selection State (interactionStore) ----
  selectedPoint: Point | null;
  selectedPoints: Point[];
  hoveredPoint: Point | null;

  // ---- 交互状态 / Interaction State (interactionStore) ----
  isDragging: boolean;
  isDraggingPoint: boolean;
  isBoxSelecting: boolean;
  boxSelectStart: { x: number; y: number } | null;
  dragStart: { x: number; y: number } | null;
  dragPoint: Point | null;
  regionPoints: Point[];
  segmentFirstPoint: Point | null;

  // ---- 鼠标坐标 / Mouse Coordinates (interactionStore) ----
  mouseWorldX: number;
  mouseWorldY: number;
  mouseScreenX: number;
  mouseScreenY: number;

  // ---- 画布显示选项 / Canvas Display Options (canvasStore) ----
  showGrid: boolean;
  showAxes: boolean;
  showLabels: boolean;

  // ---- 画布主题颜色 / Theme Colors (canvasStore) ----
  themeColors: ThemeColors;

  // ---- 撤销/重做 / Undo/Redo (geometryStore) ----
  undoStack: UndoSnapshot[];
  redoStack: UndoSnapshot[];

  // ---- 后端 / Backend (uiStore) ----
  backend: 'wasm' | 'js' | null;
  graphHandle: number | null;

  // ---- 日志 / Logging (uiStore) ----
  minLogLevel: LogLevel;
  logs: LogEntry[];

  // ---- Toast 通知 / Toast Notifications (uiStore) ----
  toasts: Toast[];

  // ---- 右键菜单 / Context Menu (interactionStore) ----
  contextMenu: ContextMenuState | null;

  // ---- 模态框 / Modal (uiStore) ----
  modal: ModalConfig | null;

  // ---- 性能统计 / Performance (uiStore) ----
  perfStats: PerfStats;

  // ---- 面板状态 / Panel State (uiStore) ----
  panelStates: Record<string, boolean>;

  // ---- 侧边栏 / Sidebar (uiStore) ----
  leftSidebarWidth: number;
  rightSidebarWidth: number;
  resizeState: ResizeState | null;

  // ---- 搜索 / Search (interactionStore) ----
  searchVisible: boolean;
  searchQuery: string;

  // ---- 公式模块 / Formula Module (uiStore) ----
  formulaInput: string;
  formulaSyntax: FormulaSyntax;
  formulaOutputFormat: FormulaOutputFormat;

  // ---- 流式日志 / Streaming Entries (aiStore) ----
  streamingEntries: StreamingEntry[];
  streamingActive: boolean;

  // ---- AI 助手 / AI Assistant (aiStore) ----
  chatMessages: ChatMessage[];
  activeProvider: string;
  isStreaming: boolean;
  streamingEvents: EngineStreamEvent[];
  streamFilters: EngineStreamCategoryFilter[];
  modelTemperature: number;
  modelMaxTokens: number;

  // ---- 状态栏 / Status Bar (uiStore) ----
  statusMessage: string;

  // ================================================================
  // Actions / 操作方法
  // ================================================================

  // ---- 模块与主题 / Module & Theme ----
  setActiveModule: (module: ModuleType) => void;
  setTheme: (theme: Theme) => void;
  setTool: (tool: ToolType) => void;

  // ---- 几何 / Geometry ----
  addPoint: (point: Point) => void;
  addSegment: (segment: Segment) => void;
  removePoint: (id: number) => void;
  removeSegment: (id: number) => void;
  addConstraint: (constraint: Constraint) => void;
  addRegion: (region: Region) => void;
  clearAll: () => void;
  setPoints: (points: Point[]) => void;
  setSegments: (segments: Segment[]) => void;
  setConstraints: (constraints: Constraint[]) => void;
  setRegions: (regions: Region[]) => void;

  // ---- 画布视图 / Canvas View ----
  setScale: (scale: number) => void;
  setOffset: (offsetX: number, offsetY: number) => void;
  setDpr: (dpr: number) => void;
  setCanvasSize: (width: number, height: number) => void;
  resetView: () => void;

  // ---- 选择 / Selection ----
  setSelectedPoint: (point: Point | null) => void;
  setSelectedPoints: (points: Point[]) => void;
  setHoveredPoint: (point: Point | null) => void;

  // ---- 交互 / Interaction ----
  setIsDragging: (dragging: boolean) => void;
  setIsDraggingPoint: (dragging: boolean) => void;
  setIsBoxSelecting: (selecting: boolean) => void;
  setBoxSelectStart: (start: { x: number; y: number } | null) => void;
  setDragStart: (start: { x: number; y: number } | null) => void;
  setDragPoint: (point: Point | null) => void;
  addRegionPoint: (point: Point) => void;
  clearRegionPoints: () => void;
  setSegmentFirstPoint: (point: Point | null) => void;

  // ---- 鼠标 / Mouse ----
  setMouseWorld: (x: number, y: number) => void;
  setMouseScreen: (x: number, y: number) => void;

  // ---- 画布显示 / Canvas Display ----
  toggleGrid: () => void;
  toggleAxes: () => void;
  toggleLabels: () => void;

  // ---- 主题颜色 / Theme Colors ----
  setThemeColors: (colors: ThemeColors) => void;

  // ---- 撤销/重做 / Undo/Redo ----
  saveUndoState: () => void;
  undo: () => void;
  redo: () => void;

  // ---- 后端 / Backend ----
  setBackend: (backend: 'wasm' | 'js' | null) => void;
  setGraphHandle: (handle: number | null) => void;

  // ---- 日志 / Logging ----
  setMinLogLevel: (level: LogLevel) => void;
  appendLog: (message: string, level: LogLevel) => void;
  clearLogs: () => void;

  // ---- Toast ----
  addToast: (variant: ToastVariant, message: string, duration?: number) => void;
  removeToast: (id: string) => void;

  // ---- 右键菜单 / Context Menu ----
  showContextMenu: (state: ContextMenuState) => void;
  hideContextMenu: () => void;

  // ---- 模态框 / Modal ----
  showModal: (config: ModalConfig) => void;
  hideModal: () => void;

  // ---- 性能 / Performance ----
  updatePerfStats: (stats: Partial<PerfStats>) => void;

  // ---- 面板 / Panel ----
  togglePanel: (id: string) => void;
  setPanelCollapsed: (id: string, collapsed: boolean) => void;

  // ---- 侧边栏 / Sidebar ----
  setLeftSidebarWidth: (width: number) => void;
  setRightSidebarWidth: (width: number) => void;
  setResizeState: (state: ResizeState | null) => void;

  // ---- 搜索 / Search ----
  setSearchVisible: (visible: boolean) => void;
  setSearchQuery: (query: string) => void;

  // ---- 公式 / Formula ----
  setFormulaInput: (input: string) => void;
  setFormulaSyntax: (syntax: FormulaSyntax) => void;
  setFormulaOutputFormat: (format: FormulaOutputFormat) => void;

  // ---- 流式日志 / Streaming ----
  addStreamingEntry: (entry: StreamingEntry) => void;
  clearStreamingEntries: () => void;
  setStreamingActive: (active: boolean) => void;

  // ---- AI 助手 / AI Assistant ----
  addMessage: (message: ChatMessage) => void;
  updateLastAssistantMessage: (content: string) => void;
  clearMessages: () => void;
  setActiveProvider: (providerId: string) => void;
  setIsStreaming: (streaming: boolean) => void;
  addStreamEvent: (event: EngineStreamEvent) => void;
  clearStreamEvents: () => void;
  toggleStreamFilter: (type: string) => void;
  resetStreamFilterCounts: () => void;
  incrementStreamFilterCount: (type: string) => void;
  setModelTemperature: (temp: number) => void;
  setModelMaxTokens: (tokens: number) => void;
  sendMessage: (content: string) => Promise<void>;

  // ---- 状态 / Status ----
  setStatusMessage: (message: string) => void;
}

// ================================================================
// 聚合 Store / Aggregated Store
// ================================================================

/**
 * useAppStore - 聚合所有子 Store 的兼容性 Hook。
 *
 * 通过 Zustand 的 create 创建一个聚合 Store，该 Store 通过订阅机制
 * 与所有子 Store 保持同步，对外暴露与原单体 Store 完全一致的接口。
 *
 * 工作原理：
 * 1. getState() 从所有子 Store 读取最新状态并聚合返回
 * 2. 所有 action 直接委托给对应的子 Store
 * 3. 通过 subscribe 监听子 Store 变化，触发聚合 Store 的 set() 更新
 * 4. Zustand 的 shallow merge 确保仅更新的字段影响聚合状态
 *
 * 性能特性：
 * - 选择器（selector）正常工作：useAppStore(s => s.scale) 仅当 scale 变化时触发重渲染
 * - 子 Store 之间独立更新，不会互相触发不必要的重渲染
 *
 * 使用建议：
 * - 新代码推荐直接使用 useCanvasStore、useGeometryStore 等子 Store
 * - 现有代码无需修改，继续使用 useAppStore 即可
 */
export const useAppStore = create<AppState>((set) => {
  // 从子 Store 获取初始状态
  const initCanvas = useCanvasStore.getState();
  const initGeometry = useGeometryStore.getState();
  const initInteraction = useInteractionStore.getState();
  const initAI = useAIStore.getState();
  const initUI = useUIStore.getState();

  // 订阅所有子 Store，当子 Store 更新时同步到聚合 Store
  // Zustand 的 set 默认使用 shallow merge，因此各子 Store 的字段更新互不干扰
  useCanvasStore.subscribe((state) => set(state));
  useGeometryStore.subscribe((state) => set(state));
  useInteractionStore.subscribe((state) => set(state));
  useAIStore.subscribe((state) => set(state));
  useUIStore.subscribe((state) => set(state));

  // 返回初始状态 + 所有 action（委托给子 Store）
  return {
    // ---- 初始状态 / Initial State (aggregated from sub-stores) ----
    ...initCanvas,
    ...initGeometry,
    ...initInteraction,
    ...initAI,
    ...initUI,

    // ================================================================
    // Actions: 模块与主题 / Module & Theme → uiStore / interactionStore
    // ================================================================
    setActiveModule: (module) => useUIStore.getState().setActiveModule(module),
    setTheme: (theme) => useUIStore.getState().setTheme(theme),
    setTool: (tool) => useInteractionStore.getState().setTool(tool),

    // ================================================================
    // Actions: 几何 / Geometry → geometryStore
    // ================================================================
    addPoint: (point) => useGeometryStore.getState().addPoint(point),
    addSegment: (segment) => useGeometryStore.getState().addSegment(segment),
    removePoint: (id) => useGeometryStore.getState().removePoint(id),
    removeSegment: (id) => useGeometryStore.getState().removeSegment(id),
    addConstraint: (constraint) => useGeometryStore.getState().addConstraint(constraint),
    addRegion: (region) => useGeometryStore.getState().addRegion(region),
    clearAll: () => useGeometryStore.getState().clearAll(),
    setPoints: (points) => useGeometryStore.getState().setPoints(points),
    setSegments: (segments) => useGeometryStore.getState().setSegments(segments),
    setConstraints: (constraints) => useGeometryStore.getState().setConstraints(constraints),
    setRegions: (regions) => useGeometryStore.getState().setRegions(regions),

    // ================================================================
    // Actions: 画布视图 / Canvas View → canvasStore
    // ================================================================
    setScale: (scale) => useCanvasStore.getState().setScale(scale),
    setOffset: (ox, oy) => useCanvasStore.getState().setOffset(ox, oy),
    setDpr: (dpr) => useCanvasStore.getState().setDpr(dpr),
    setCanvasSize: (w, h) => useCanvasStore.getState().setCanvasSize(w, h),
    resetView: () => useCanvasStore.getState().resetView(),

    // ================================================================
    // Actions: 选择 / Selection → interactionStore
    // ================================================================
    setSelectedPoint: (point) => useInteractionStore.getState().setSelectedPoint(point),
    setSelectedPoints: (points) => useInteractionStore.getState().setSelectedPoints(points),
    setHoveredPoint: (point) => useInteractionStore.getState().setHoveredPoint(point),

    // ================================================================
    // Actions: 交互 / Interaction → interactionStore
    // ================================================================
    setIsDragging: (d) => useInteractionStore.getState().setIsDragging(d),
    setIsDraggingPoint: (d) => useInteractionStore.getState().setIsDraggingPoint(d),
    setIsBoxSelecting: (s) => useInteractionStore.getState().setIsBoxSelecting(s),
    setBoxSelectStart: (s) => useInteractionStore.getState().setBoxSelectStart(s),
    setDragStart: (s) => useInteractionStore.getState().setDragStart(s),
    setDragPoint: (p) => useInteractionStore.getState().setDragPoint(p),
    addRegionPoint: (p) => useInteractionStore.getState().addRegionPoint(p),
    clearRegionPoints: () => useInteractionStore.getState().clearRegionPoints(),
    setSegmentFirstPoint: (p) => useInteractionStore.getState().setSegmentFirstPoint(p),

    // ================================================================
    // Actions: 鼠标 / Mouse → interactionStore
    // ================================================================
    setMouseWorld: (x, y) => useInteractionStore.getState().setMouseWorld(x, y),
    setMouseScreen: (x, y) => useInteractionStore.getState().setMouseScreen(x, y),

    // ================================================================
    // Actions: 画布显示 / Canvas Display → canvasStore
    // ================================================================
    toggleGrid: () => useCanvasStore.getState().toggleGrid(),
    toggleAxes: () => useCanvasStore.getState().toggleAxes(),
    toggleLabels: () => useCanvasStore.getState().toggleLabels(),

    // ================================================================
    // Actions: 主题颜色 / Theme Colors → canvasStore
    // ================================================================
    setThemeColors: (colors) => useCanvasStore.getState().setThemeColors(colors),

    // ================================================================
    // Actions: 撤销/重做 / Undo/Redo → geometryStore
    // ================================================================
    saveUndoState: () => useGeometryStore.getState().saveUndoState(),
    undo: () => useGeometryStore.getState().undo(),
    redo: () => useGeometryStore.getState().redo(),

    // ================================================================
    // Actions: 后端 / Backend → uiStore
    // ================================================================
    setBackend: (b) => useUIStore.getState().setBackend(b),
    setGraphHandle: (h) => useUIStore.getState().setGraphHandle(h),

    // ================================================================
    // Actions: 日志 / Logging → uiStore
    // ================================================================
    setMinLogLevel: (l) => useUIStore.getState().setMinLogLevel(l),
    appendLog: (msg, lvl) => useUIStore.getState().appendLog(msg, lvl),
    clearLogs: () => useUIStore.getState().clearLogs(),

    // ================================================================
    // Actions: Toast → uiStore
    // ================================================================
    addToast: (v, msg, dur) => useUIStore.getState().addToast(v, msg, dur),
    removeToast: (id) => useUIStore.getState().removeToast(id),

    // ================================================================
    // Actions: 右键菜单 / Context Menu → interactionStore
    // ================================================================
    showContextMenu: (s) => useInteractionStore.getState().showContextMenu(s),
    hideContextMenu: () => useInteractionStore.getState().hideContextMenu(),

    // ================================================================
    // Actions: 模态框 / Modal → uiStore
    // ================================================================
    showModal: (c) => useUIStore.getState().showModal(c),
    hideModal: () => useUIStore.getState().hideModal(),

    // ================================================================
    // Actions: 性能 / Performance → uiStore
    // ================================================================
    updatePerfStats: (s) => useUIStore.getState().updatePerfStats(s),

    // ================================================================
    // Actions: 面板 / Panel → uiStore
    // ================================================================
    togglePanel: (id) => useUIStore.getState().togglePanel(id),
    setPanelCollapsed: (id, c) => useUIStore.getState().setPanelCollapsed(id, c),

    // ================================================================
    // Actions: 侧边栏 / Sidebar → uiStore
    // ================================================================
    setLeftSidebarWidth: (w) => useUIStore.getState().setLeftSidebarWidth(w),
    setRightSidebarWidth: (w) => useUIStore.getState().setRightSidebarWidth(w),
    setResizeState: (s) => useUIStore.getState().setResizeState(s),

    // ================================================================
    // Actions: 搜索 / Search → interactionStore
    // ================================================================
    setSearchVisible: (v) => useInteractionStore.getState().setSearchVisible(v),
    setSearchQuery: (q) => useInteractionStore.getState().setSearchQuery(q),

    // ================================================================
    // Actions: 公式 / Formula → uiStore
    // ================================================================
    setFormulaInput: (i) => useUIStore.getState().setFormulaInput(i),
    setFormulaSyntax: (s) => useUIStore.getState().setFormulaSyntax(s),
    setFormulaOutputFormat: (f) => useUIStore.getState().setFormulaOutputFormat(f),

    // ================================================================
    // Actions: 流式日志 / Streaming → aiStore
    // ================================================================
    addStreamingEntry: (e) => useAIStore.getState().addStreamingEntry(e),
    clearStreamingEntries: () => useAIStore.getState().clearStreamingEntries(),
    setStreamingActive: (a) => useAIStore.getState().setStreamingActive(a),

    // ================================================================
    // Actions: AI 助手 / AI Assistant → aiStore
    // ================================================================
    addMessage: (m) => useAIStore.getState().addMessage(m),
    updateLastAssistantMessage: (c) => useAIStore.getState().updateLastAssistantMessage(c),
    clearMessages: () => useAIStore.getState().clearMessages(),
    setActiveProvider: (p) => useAIStore.getState().setActiveProvider(p),
    setIsStreaming: (s) => useAIStore.getState().setIsStreaming(s),
    addStreamEvent: (e) => useAIStore.getState().addStreamEvent(e),
    clearStreamEvents: () => useAIStore.getState().clearStreamEvents(),
    toggleStreamFilter: (t) => useAIStore.getState().toggleStreamFilter(t),
    resetStreamFilterCounts: () => useAIStore.getState().resetStreamFilterCounts(),
    incrementStreamFilterCount: (t) => useAIStore.getState().incrementStreamFilterCount(t),
    setModelTemperature: (t) => useAIStore.getState().setModelTemperature(t),
    setModelMaxTokens: (t) => useAIStore.getState().setModelMaxTokens(t),
    sendMessage: (c) => useAIStore.getState().sendMessage(c),

    // ================================================================
    // Actions: 状态栏 / Status Bar → uiStore
    // ================================================================
    setStatusMessage: (m) => useUIStore.getState().setStatusMessage(m),
  } as AppState;
});
