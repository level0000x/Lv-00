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
 *
 * @example 推荐方式（新代码）
 * ```typescript
 * import { useCanvasStore } from '@/stores';
 * const scale = useCanvasStore((s) => s.scale);
 * ```
 *
 * @example 兼容方式（现有代码）
 * ```typescript
 * import { useAppStore } from '@/stores';
 * const scale = useAppStore((s) => s.scale);
 * ```
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
 *
 * 注意：此接口仅用于类型定义和向后兼容，实际状态分散在各个子 Store 中。
 * 新代码应直接使用各子 Store 的类型接口。
 */
interface AppState {
  // ---- 活跃模块与主题 / Active Module & Theme (uiStore) ----
  /** 当前活跃的功能模块面板（如 'formula', 'geometry', 'ai'） */
  activeModule: ModuleType;
  /** 当前 UI 主题（'dark' 或 'light'） */
  theme: Theme;
  /** 当前画布工具（如 'select', 'point', 'segment', 'region'） */
  tool: ToolType;

  // ---- 几何数据 / Geometry Data (geometryStore) ----
  /** 所有点的列表 */
  points: Point[];
  /** 所有线段的列表 */
  segments: Segment[];
  /** 所有约束的列表 */
  constraints: Constraint[];
  /** 所有区域的列表 */
  regions: Region[];

  // ---- 画布视图状态 / Canvas View State (canvasStore) ----
  /** 画布缩放比例 */
  scale: number;
  /** 画布水平平移偏移（世界坐标） */
  offsetX: number;
  /** 画布垂直平移偏移（世界坐标） */
  offsetY: number;
  /** 设备像素比（用于高 DPI 渲染） */
  dpr: number;
  /** Canvas 元素的实际渲染宽度（CSS 像素） */
  canvasWidth: number;
  /** Canvas 元素的实际渲染高度（CSS 像素） */
  canvasHeight: number;

  // ---- 选择状态 / Selection State (interactionStore) ----
  /** 当前选中的单个点（null 表示未选中） */
  selectedPoint: Point | null;
  /** 多选（框选）中的点列表 */
  selectedPoints: Point[];
  /** 当前鼠标悬停的点（null 表示无悬停） */
  hoveredPoint: Point | null;

  // ---- 交互状态 / Interaction State (interactionStore) ----
  /** 是否正在平移画布 */
  isDragging: boolean;
  /** 是否正在拖拽某个点 */
  isDraggingPoint: boolean;
  /** 是否正在进行框选 */
  isBoxSelecting: boolean;
  /** 框选起始屏幕坐标（null 表示未开始框选） */
  boxSelectStart: { x: number; y: number } | null;
  /** 拖拽起始屏幕坐标（null 表示未开始拖拽） */
  dragStart: { x: number; y: number } | null;
  /** 当前正在被拖拽的点对象（null 表示无拖拽点） */
  dragPoint: Point | null;
  /** 区域定义过程中已点击的顶点列表 */
  regionPoints: Point[];
  /** 线段工具已选中的第一个端点（null 表示未选择） */
  segmentFirstPoint: Point | null;

  // ---- 鼠标坐标 / Mouse Coordinates (interactionStore) ----
  /** 鼠标在世界坐标系中的 X 坐标 */
  mouseWorldX: number;
  /** 鼠标在世界坐标系中的 Y 坐标 */
  mouseWorldY: number;
  /** 鼠标在屏幕坐标系中的 X 坐标（相对于 Canvas 左上角） */
  mouseScreenX: number;
  /** 鼠标在屏幕坐标系中的 Y 坐标（相对于 Canvas 左上角） */
  mouseScreenY: number;

  // ---- 画布显示选项 / Canvas Display Options (canvasStore) ----
  /** 是否显示参考网格 */
  showGrid: boolean;
  /** 是否显示坐标轴 */
  showAxes: boolean;
  /** 是否显示点的标签文字 */
  showLabels: boolean;

  // ---- 画布主题颜色 / Theme Colors (canvasStore) ----
  /** 当前主题对应的画布渲染颜色方案 */
  themeColors: ThemeColors;

  // ---- 撤销/重做 / Undo/Redo (geometryStore) ----
  /** 撤销历史栈（栈顶为最近状态，最多 50 条） */
  undoStack: UndoSnapshot[];
  /** 重做历史栈（新操作会清空此栈） */
  redoStack: UndoSnapshot[];

  // ---- 后端 / Backend (uiStore) ----
  /** 活跃的后端类型（'wasm' | 'js' | null） */
  backend: 'wasm' | 'js' | null;
  /** 后端的图句柄（null 表示未创建） */
  graphHandle: number | null;

  // ---- 日志 / Logging (uiStore) ----
  /** 最小显示日志级别（如 'debug', 'info', 'warn', 'error'） */
  minLogLevel: LogLevel;
  /** 应用日志条目列表（最多 500 条） */
  logs: LogEntry[];

  // ---- Toast 通知 / Toast Notifications (uiStore) ----
  /** 活跃的 Toast 通知列表 */
  toasts: Toast[];

  // ---- 右键菜单 / Context Menu (interactionStore) ----
  /** 右键菜单状态（null 表示隐藏） */
  contextMenu: ContextMenuState | null;

  // ---- 模态框 / Modal (uiStore) ----
  /** 活跃的模态框配置（null 表示无模态框） */
  modal: ModalConfig | null;

  // ---- 性能统计 / Performance (uiStore) ----
  /** 渲染性能统计（FPS、渲染次数、平均渲染时间等） */
  perfStats: PerfStats;

  // ---- 面板状态 / Panel State (uiStore) ----
  /** 各面板的折叠状态（key = 面板 ID, value = 是否折叠） */
  panelStates: Record<string, boolean>;

  // ---- 侧边栏 / Sidebar (uiStore) ----
  /** 左侧边栏宽度（px） */
  leftSidebarWidth: number;
  /** 右侧边栏宽度（px） */
  rightSidebarWidth: number;
  /** 当前活跃的拖拽调整大小状态（null 表示无拖拽） */
  resizeState: ResizeState | null;

  // ---- 搜索 / Search (interactionStore) ----
  /** 搜索栏是否可见 */
  searchVisible: boolean;
  /** 当前搜索查询字符串 */
  searchQuery: string;

  // ---- 公式模块 / Formula Module (uiStore) ----
  /** 公式输入文本 */
  formulaInput: string;
  /** 公式语法模式（如 'auto', 'latex', 'asciimath'） */
  formulaSyntax: FormulaSyntax;
  /** 公式输出格式（如 'latex', 'asciimath', 'plain'） */
  formulaOutputFormat: FormulaOutputFormat;

  // ---- 流式日志 / Streaming Entries (aiStore) ----
  /** 流式输出的日志条目列表（最多 500 条） */
  streamingEntries: StreamingEntry[];
  /** 流式输出是否处于活跃状态 */
  streamingActive: boolean;

  // ---- AI 助手 / AI Assistant (aiStore) ----
  /** 聊天对话中的所有消息（最多 100 条） */
  chatMessages: ChatMessage[];
  /** 当前活跃的 AI 提供者 ID */
  activeProvider: string;
  /** AI 是否正在流式输出响应 */
  isStreaming: boolean;
  /** 来自引擎和 AI 后端的统一流式事件列表（最多 500 条） */
  streamingEvents: EngineStreamEvent[];
  /** 流式事件类别过滤器配置（8 类别体系） */
  streamFilters: EngineStreamCategoryFilter[];
  /** 模型温度参数 (0-2) */
  modelTemperature: number;
  /** 模型最大 Token 数 (64-32768) */
  modelMaxTokens: number;

  // ---- 状态栏 / Status Bar (uiStore) ----
  /** 状态栏消息文本 */
  statusMessage: string;

  // ================================================================
  // Actions / 操作方法
  // ================================================================

  // ---- 模块与主题 / Module & Theme ----
  /** 设置当前活跃的功能模块 */
  setActiveModule: (module: ModuleType) => void;
  /** 设置 UI 主题（同时更新 DOM class） */
  setTheme: (theme: Theme) => void;
  /** 设置当前画布工具（同时重置相关交互状态） */
  setTool: (tool: ToolType) => void;

  // ---- 几何 / Geometry ----
  /** 添加一个点到几何图中 */
  addPoint: (point: Point) => void;
  /** 添加一条线段到几何图中 */
  addSegment: (segment: Segment) => void;
  /** 删除指定 ID 的点（同时删除关联的线段和约束） */
  removePoint: (id: number) => void;
  /** 删除指定 ID 的线段（同时删除关联的约束） */
  removeSegment: (id: number) => void;
  /** 添加一个约束到几何图中 */
  addConstraint: (constraint: Constraint) => void;
  /** 添加一个区域到几何图中 */
  addRegion: (region: Region) => void;
  /** 清空所有几何数据（点、线段、约束、区域） */
  clearAll: () => void;
  /** 批量设置所有点（替换现有数据） */
  setPoints: (points: Point[]) => void;
  /** 批量设置所有线段（替换现有数据） */
  setSegments: (segments: Segment[]) => void;
  /** 批量设置所有约束（替换现有数据） */
  setConstraints: (constraints: Constraint[]) => void;
  /** 批量设置所有区域（替换现有数据） */
  setRegions: (regions: Region[]) => void;

  // ---- 画布视图 / Canvas View ----
  /** 设置画布缩放比例（自动钳制到安全范围） */
  setScale: (scale: number) => void;
  /** 设置画布平移偏移 */
  setOffset: (offsetX: number, offsetY: number) => void;
  /** 设置设备像素比 */
  setDpr: (dpr: number) => void;
  /** 同步 Canvas 元素的实际渲染尺寸 */
  setCanvasSize: (width: number, height: number) => void;
  /** 重置视图（缩放=1，偏移归零） */
  resetView: () => void;

  // ---- 选择 / Selection ----
  /** 设置当前选中的单个点 */
  setSelectedPoint: (point: Point | null) => void;
  /** 设置多选中的点列表 */
  setSelectedPoints: (points: Point[]) => void;
  /** 设置鼠标悬停的点 */
  setHoveredPoint: (point: Point | null) => void;

  // ---- 交互 / Interaction ----
  /** 设置画布平移拖拽状态 */
  setIsDragging: (dragging: boolean) => void;
  /** 设置点拖拽状态 */
  setIsDraggingPoint: (dragging: boolean) => void;
  /** 设置框选状态 */
  setIsBoxSelecting: (selecting: boolean) => void;
  /** 设置框选起始坐标 */
  setBoxSelectStart: (start: { x: number; y: number } | null) => void;
  /** 设置拖拽起始坐标 */
  setDragStart: (start: { x: number; y: number } | null) => void;
  /** 设置当前被拖拽的点 */
  setDragPoint: (point: Point | null) => void;
  /** 添加一个区域定义顶点 */
  addRegionPoint: (point: Point) => void;
  /** 清除所有区域定义顶点 */
  clearRegionPoints: () => void;
  /** 设置线段工具的首个端点 */
  setSegmentFirstPoint: (point: Point | null) => void;

  // ---- 鼠标 / Mouse ----
  /** 设置鼠标在世界坐标系中的位置 */
  setMouseWorld: (x: number, y: number) => void;
  /** 设置鼠标在屏幕坐标系中的位置 */
  setMouseScreen: (x: number, y: number) => void;

  // ---- 画布显示 / Canvas Display ----
  /** 切换参考网格的显示/隐藏 */
  toggleGrid: () => void;
  /** 切换坐标轴的显示/隐藏 */
  toggleAxes: () => void;
  /** 切换点标签的显示/隐藏 */
  toggleLabels: () => void;

  // ---- 主题颜色 / Theme Colors ----
  /** 设置主题颜色方案 */
  setThemeColors: (colors: ThemeColors) => void;

  // ---- 撤销/重做 / Undo/Redo ----
  /** 保存当前状态快照到撤销栈（在修改几何数据前调用） */
  saveUndoState: () => void;
  /** 撤销操作：恢复到上一个几何状态 */
  undo: () => void;
  /** 重做操作：恢复刚刚被撤销的状态 */
  redo: () => void;

  // ---- 后端 / Backend ----
  /** 设置后端类型 */
  setBackend: (backend: 'wasm' | 'js' | null) => void;
  /** 设置后端图句柄 */
  setGraphHandle: (handle: number | null) => void;

  // ---- 日志 / Logging ----
  /** 设置最小日志级别 */
  setMinLogLevel: (level: LogLevel) => void;
  /** 追加一条日志（自动限制总数不超过 500 条） */
  appendLog: (message: string, level: LogLevel) => void;
  /** 清空所有日志 */
  clearLogs: () => void;

  // ---- Toast ----
  /** 添加一个 Toast 通知（可指定自动消失时间） */
  addToast: (variant: ToastVariant, message: string, duration?: number) => void;
  /** 移除指定 ID 的 Toast 通知 */
  removeToast: (id: string) => void;

  // ---- 右键菜单 / Context Menu ----
  /** 显示右键菜单 */
  showContextMenu: (state: ContextMenuState) => void;
  /** 隐藏右键菜单 */
  hideContextMenu: () => void;

  // ---- 模态框 / Modal ----
  /** 显示模态框 */
  showModal: (config: ModalConfig) => void;
  /** 隐藏模态框 */
  hideModal: () => void;

  // ---- 性能 / Performance ----
  /** 部分更新性能统计 */
  updatePerfStats: (stats: Partial<PerfStats>) => void;

  // ---- 面板 / Panel ----
  /** 切换面板折叠/展开状态 */
  togglePanel: (id: string) => void;
  /** 设置面板折叠状态 */
  setPanelCollapsed: (id: string, collapsed: boolean) => void;

  // ---- 侧边栏 / Sidebar ----
  /** 设置左侧边栏宽度（自动钳制到安全范围） */
  setLeftSidebarWidth: (width: number) => void;
  /** 设置右侧边栏宽度（自动钳制到安全范围） */
  setRightSidebarWidth: (width: number) => void;
  /** 设置拖拽调整大小状态 */
  setResizeState: (state: ResizeState | null) => void;

  // ---- 搜索 / Search ----
  /** 设置搜索栏可见性 */
  setSearchVisible: (visible: boolean) => void;
  /** 设置搜索查询内容 */
  setSearchQuery: (query: string) => void;

  // ---- 公式 / Formula ----
  /** 设置公式输入文本 */
  setFormulaInput: (input: string) => void;
  /** 设置公式语法模式 */
  setFormulaSyntax: (syntax: FormulaSyntax) => void;
  /** 设置公式输出格式 */
  setFormulaOutputFormat: (format: FormulaOutputFormat) => void;

  // ---- 流式日志 / Streaming ----
  /** 添加一条流式日志条目（自动限制总数不超过 500 条） */
  addStreamingEntry: (entry: StreamingEntry) => void;
  /** 清空所有流式日志条目 */
  clearStreamingEntries: () => void;
  /** 设置流式输出活跃状态 */
  setStreamingActive: (active: boolean) => void;

  // ---- AI 助手 / AI Assistant ----
  /** 添加一条聊天消息到对话中（自动限制总数不超过 100 条） */
  addMessage: (message: ChatMessage) => void;
  /** 更新最后一条 assistant 消息的内容（用于流式增量更新） */
  updateLastAssistantMessage: (content: string) => void;
  /** 清空所有聊天消息 */
  clearMessages: () => void;
  /** 设置当前活跃的 AI 提供者 */
  setActiveProvider: (providerId: string) => void;
  /** 设置流式输出状态 */
  setIsStreaming: (streaming: boolean) => void;
  /** 添加一个引擎流式事件（自动限制总数不超过 500 条） */
  addStreamEvent: (event: EngineStreamEvent) => void;
  /** 清空所有流式事件并重置过滤器计数 */
  clearStreamEvents: () => void;
  /** 切换指定类别的流式事件过滤器 */
  toggleStreamFilter: (type: string) => void;
  /** 重置所有流式事件过滤器的计数 */
  resetStreamFilterCounts: () => void;
  /** 增加指定类别过滤器的计数 */
  incrementStreamFilterCount: (type: string) => void;
  /** 设置模型温度（自动钳制到 0-2） */
  setModelTemperature: (temp: number) => void;
  /** 设置模型最大 Token 数（自动钳制到 64-32768） */
  setModelMaxTokens: (tokens: number) => void;
  /** 发送用户消息并获取 AI 响应（异步，支持流式输出） */
  sendMessage: (content: string) => Promise<void>;

  // ---- 状态栏 / Status ----
  /** 设置状态栏消息 */
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
 * 2. 所有 action 直接委托给对应的子 Store（通过 .getState() 获取最新引用）
 * 3. 通过 subscribe 监听子 Store 变化，触发聚合 Store 的 set() 更新
 * 4. Zustand 的 shallow merge 确保仅更新的字段影响聚合状态
 *
 * 性能特性：
 * - 选择器（selector）正常工作：useAppStore(s => s.scale) 仅当 scale 变化时触发重渲染
 * - 子 Store 之间独立更新，不会互相触发不必要的重渲染
 * - 注意：不带 selector 的 useAppStore() 调用会在任何子 Store 更新时触发重渲染，
 *   因此新代码推荐使用带 selector 的方式或直接使用子 Store Hook
 *
 * 内存管理：
 * - subscribe 监听器在模块加载时注册，随应用生命周期存在，无需手动清理
 * - 聚合 Store 本身不持有独立的状态副本，仅作为子 Store 状态的聚合视图
 *
 * 使用建议：
 * - 新代码推荐直接使用 useCanvasStore、useGeometryStore 等子 Store
 * - 现有代码无需修改，继续使用 useAppStore 即可
 */
export const useAppStore = create<AppState>((set) => {
  // 从子 Store 获取初始状态（仅在 Store 创建时执行一次）
  const initCanvas = useCanvasStore.getState();
  const initGeometry = useGeometryStore.getState();
  const initInteraction = useInteractionStore.getState();
  const initAI = useAIStore.getState();
  const initUI = useUIStore.getState();

  // 订阅所有子 Store，当子 Store 更新时同步到聚合 Store。
  // Zustand 的 set 默认使用 shallow merge，因此各子 Store 的字段更新互不干扰。
  // 例如：canvasStore 更新 scale 时，仅 scale 字段被合并到聚合 Store，
  // geometryStore 的 points 等字段保持不变，不会触发额外的引用变化。
  useCanvasStore.subscribe((state) => set(state));
  useGeometryStore.subscribe((state) => set(state));
  useInteractionStore.subscribe((state) => set(state));
  useAIStore.subscribe((state) => set(state));
  useUIStore.subscribe((state) => set(state));

  // 返回初始状态（聚合所有子 Store）+ 所有 action（委托给子 Store）
  return {
    // ---- 初始状态 / Initial State (aggregated from sub-stores) ----
    ...initCanvas,
    ...initGeometry,
    ...initInteraction,
    ...initAI,
    ...initUI,

    // ================================================================
    // Actions: 模块与主题 / Module & Theme -> uiStore / interactionStore
    // ================================================================
    setActiveModule: (module) => useUIStore.getState().setActiveModule(module),
    setTheme: (theme) => useUIStore.getState().setTheme(theme),
    setTool: (tool) => useInteractionStore.getState().setTool(tool),

    // ================================================================
    // Actions: 几何 / Geometry -> geometryStore
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
    // Actions: 画布视图 / Canvas View -> canvasStore
    // ================================================================
    setScale: (scale) => useCanvasStore.getState().setScale(scale),
    setOffset: (ox, oy) => useCanvasStore.getState().setOffset(ox, oy),
    setDpr: (dpr) => useCanvasStore.getState().setDpr(dpr),
    setCanvasSize: (w, h) => useCanvasStore.getState().setCanvasSize(w, h),
    resetView: () => useCanvasStore.getState().resetView(),

    // ================================================================
    // Actions: 选择 / Selection -> interactionStore
    // ================================================================
    setSelectedPoint: (point) => useInteractionStore.getState().setSelectedPoint(point),
    setSelectedPoints: (points) => useInteractionStore.getState().setSelectedPoints(points),
    setHoveredPoint: (point) => useInteractionStore.getState().setHoveredPoint(point),

    // ================================================================
    // Actions: 交互 / Interaction -> interactionStore
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
    // Actions: 鼠标 / Mouse -> interactionStore
    // ================================================================
    setMouseWorld: (x, y) => useInteractionStore.getState().setMouseWorld(x, y),
    setMouseScreen: (x, y) => useInteractionStore.getState().setMouseScreen(x, y),

    // ================================================================
    // Actions: 画布显示 / Canvas Display -> canvasStore
    // ================================================================
    toggleGrid: () => useCanvasStore.getState().toggleGrid(),
    toggleAxes: () => useCanvasStore.getState().toggleAxes(),
    toggleLabels: () => useCanvasStore.getState().toggleLabels(),

    // ================================================================
    // Actions: 主题颜色 / Theme Colors -> canvasStore
    // ================================================================
    setThemeColors: (colors) => useCanvasStore.getState().setThemeColors(colors),

    // ================================================================
    // Actions: 撤销/重做 / Undo/Redo -> geometryStore
    // ================================================================
    saveUndoState: () => useGeometryStore.getState().saveUndoState(),
    undo: () => useGeometryStore.getState().undo(),
    redo: () => useGeometryStore.getState().redo(),

    // ================================================================
    // Actions: 后端 / Backend -> uiStore
    // ================================================================
    setBackend: (b) => useUIStore.getState().setBackend(b),
    setGraphHandle: (h) => useUIStore.getState().setGraphHandle(h),

    // ================================================================
    // Actions: 日志 / Logging -> uiStore
    // ================================================================
    setMinLogLevel: (l) => useUIStore.getState().setMinLogLevel(l),
    appendLog: (msg, lvl) => useUIStore.getState().appendLog(msg, lvl),
    clearLogs: () => useUIStore.getState().clearLogs(),

    // ================================================================
    // Actions: Toast -> uiStore
    // ================================================================
    addToast: (v, msg, dur) => useUIStore.getState().addToast(v, msg, dur),
    removeToast: (id) => useUIStore.getState().removeToast(id),

    // ================================================================
    // Actions: 右键菜单 / Context Menu -> interactionStore
    // ================================================================
    showContextMenu: (s) => useInteractionStore.getState().showContextMenu(s),
    hideContextMenu: () => useInteractionStore.getState().hideContextMenu(),

    // ================================================================
    // Actions: 模态框 / Modal -> uiStore
    // ================================================================
    showModal: (c) => useUIStore.getState().showModal(c),
    hideModal: () => useUIStore.getState().hideModal(),

    // ================================================================
    // Actions: 性能 / Performance -> uiStore
    // ================================================================
    updatePerfStats: (s) => useUIStore.getState().updatePerfStats(s),

    // ================================================================
    // Actions: 面板 / Panel -> uiStore
    // ================================================================
    togglePanel: (id) => useUIStore.getState().togglePanel(id),
    setPanelCollapsed: (id, c) => useUIStore.getState().setPanelCollapsed(id, c),

    // ================================================================
    // Actions: 侧边栏 / Sidebar -> uiStore
    // ================================================================
    setLeftSidebarWidth: (w) => useUIStore.getState().setLeftSidebarWidth(w),
    setRightSidebarWidth: (w) => useUIStore.getState().setRightSidebarWidth(w),
    setResizeState: (s) => useUIStore.getState().setResizeState(s),

    // ================================================================
    // Actions: 搜索 / Search -> interactionStore
    // ================================================================
    setSearchVisible: (v) => useInteractionStore.getState().setSearchVisible(v),
    setSearchQuery: (q) => useInteractionStore.getState().setSearchQuery(q),

    // ================================================================
    // Actions: 公式 / Formula -> uiStore
    // ================================================================
    setFormulaInput: (i) => useUIStore.getState().setFormulaInput(i),
    setFormulaSyntax: (s) => useUIStore.getState().setFormulaSyntax(s),
    setFormulaOutputFormat: (f) => useUIStore.getState().setFormulaOutputFormat(f),

    // ================================================================
    // Actions: 流式日志 / Streaming -> aiStore
    // ================================================================
    addStreamingEntry: (e) => useAIStore.getState().addStreamingEntry(e),
    clearStreamingEntries: () => useAIStore.getState().clearStreamingEntries(),
    setStreamingActive: (a) => useAIStore.getState().setStreamingActive(a),

    // ================================================================
    // Actions: AI 助手 / AI Assistant -> aiStore
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
    // Actions: 状态栏 / Status Bar -> uiStore
    // ================================================================
    setStatusMessage: (m) => useUIStore.getState().setStatusMessage(m),
  } as AppState;
});
