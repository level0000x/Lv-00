/**
 * @module types
 * @description Core type definitions for the Lv-00 GUI application.
 *              Defines all shared interfaces, enums, and type aliases used
 *              across the application including geometry, modules, tools,
 *              themes, and state management.
 *
 *              Lv-00 GUI 应用的核心类型定义。
 *              定义了整个应用中使用的所有共享接口、枚举和类型别名，
 *              包括几何类型、模块类型、工具类型、主题类型和状态管理类型。
 */

// ================================================================
// Geometry Types / 几何类型
// ================================================================

/**
 * Represents a 2D point in world coordinates
 * 表示世界坐标系中的二维点
 *
 * @property id - Unique node identifier from the backend / 后端分配的唯一节点标识
 * @property x - World coordinate X / 世界坐标 X
 * @property y - World coordinate Y / 世界坐标 Y
 */
export interface Point {
  id: number;
  x: number;
  y: number;
}

/**
 * Represents a line segment connecting two points
 * 表示连接两个点的线段
 *
 * @property p1 - Start point node ID / 起点节点 ID
 * @property p2 - End point node ID / 终点节点 ID
 * @property id - Unique segment identifier from the backend / 后端分配的唯一线段标识
 */
export interface Segment {
  p1: number;
  p2: number;
  id: number;
}

/**
 * 约束类型：几何引擎支持的约束关系
 * Constraint types supported by the geometry engine
 * - incidence:   点在线段上 / Point lies on a line/segment
 * - betweenness: 点在两点之间 / Point B is between points A and C
 * - intersection: 两线段相交 / Two segments intersect at a point
 * - containment:  点/区域包含于另一区域 / A point/region is contained within another region
 * - connection:   一般连接关系 / General connection between geometric elements
 */
export type ConstraintType =
  | 'incidence'
  | 'betweenness'
  | 'intersection'
  | 'containment'
  | 'connection';

/**
 * Represents a geometric constraint between elements
 * 表示几何元素之间的约束关系
 *
 * @property id - Unique constraint identifier / 唯一约束标识
 * @property type - The type of constraint / 约束类型
 * @property args - Constraint arguments (node IDs, segment IDs, etc.) / 约束参数（节点 ID、线段 ID 等）
 */
export interface Constraint {
  id: number;
  type: ConstraintType;
  args: number[];
}

/**
 * Represents a defined region (polygon) on the canvas
 * 表示画布上定义的区域（多边形）
 *
 * @property id - Unique region identifier / 唯一区域标识
 * @property points - Array of Point objects forming the region boundary / 构成区域边界的点数组
 */
export interface Region {
  id: number;
  points: Point[];
}

/**
 * 端口方向：输入端口接收数据，输出端口发送数据
 * Port direction: input ports receive data, output ports send data
 */
export type PortDirection = 'input' | 'output';

/**
 * 代表函数块上的一个连接端口
 * Represents a connection port on a function block
 * @property id - 端口唯一标识 / Unique port identifier
 * @property x - 端口的世界坐标 X / Port world coordinate X
 * @property y - 端口的世界坐标 Y / Port world coordinate Y
 * @property direction - 端口方向（输入/输出）/ Port direction (input/output)
 * @property parentBlockId - 所属函数块 ID / Parent function block ID
 */
export interface Port {
  id: number;
  x: number;
  y: number;
  direction: PortDirection;
  parentBlockId: number;
}

/**
 * 代表一个函数块（封装几何构造的可复用模块）
 * Represents a function block (reusable module encapsulating geometric constructions)
 * @property id - 函数块唯一标识 / Unique function block identifier
 * @property x - 函数块左上角的世界坐标 X / Top-left world coordinate X
 * @property y - 函数块左上角的世界坐标 Y / Top-left world coordinate Y
 * @property width - 函数块的宽度（世界坐标）/ Function block width in world units
 * @property height - 函数块的高度（世界坐标）/ Function block height in world units
 * @property label - 函数块的显示名称 / Display label for the function block
 * @property category - 函数块分类（如 "construction", "measurement", "transform"）
 *                       Function block category
 * @property inputPorts - 输入端口 ID 列表 / List of input port IDs
 * @property outputPorts - 输出端口 ID 列表 / List of output port IDs
 */
export interface FuncBlock {
  id: number;
  x: number;
  y: number;
  width: number;
  height: number;
  label: string;
  category: string;
  inputPorts: number[];
  outputPorts: number[];
}

// ================================================================
// Graph Data / 图数据
// ================================================================

/**
 * Represents the complete constraint graph state
 * 表示完整的约束图状态
 *
 * @property points - All points in the graph / 图中所有点
 * @property segments - All segments in the graph / 图中所有线段
 * @property constraints - All constraints in the graph / 图中所有约束
 * @property regions - All defined regions / 所有定义的区域
 * @property ports - All defined ports / 所有定义的端口
 * @property funcBlocks - All function blocks / 所有函数块
 */
export interface GraphData {
  points: Point[];
  segments: Segment[];
  constraints: Constraint[];
  regions: Region[];
  ports: Port[];
  funcBlocks: FuncBlock[];
}

// ================================================================
// Tool Types / 工具类型
// ================================================================

/**
 * Available canvas interaction tools
 * - select: Select/move points, box selection
 * - point: Add new points by clicking
 * - segment: Connect two points with a segment
 * - compass: Draw circles (approximated as 24-sided polygons)
 * - pan: Pan the canvas viewport
 * - region: Define polygon regions by clicking vertices
 * - probe: Inspect point coordinates on hover
 */
export type ToolType = 'select' | 'point' | 'segment' | 'compass' | 'pan' | 'region' | 'probe';

/**
 * Tool display configuration
 * @property id - Tool identifier matching ToolType
 * @property icon - Single character icon for the toolbar button
 * @property label - Short label for the toolbar button
 * @property tooltip - Tooltip text (bilingual)
 * @property cursor - CSS cursor style when tool is active
 */
export interface ToolConfig {
  id: ToolType;
  icon: string;
  label: string;
  tooltip: string;
  cursor: string;
}

// ================================================================
// Module Types / 模块类型
// ================================================================

/**
 * Available application modules
 * 可用的应用模块，每个模块对应左侧边栏中的一个专用面板
 *
 * Each module corresponds to a specialized panel in the left sidebar
 * 每个模块对应左侧边栏中的一个专用面板
 */
export type ModuleType =
  | 'formula'
  | 'graph'
  | 'block'
  | 'proof'
  | 'type'
  | 'recurse'
  | 'engine'
  | 'debug'
  | 'help'
  | 'assistant';

/**
 * Module registration configuration
 * 模块注册配置
 *
 * @property id - Module identifier matching ModuleType / 模块标识（匹配 ModuleType）
 * @property label - Display name (English) / 显示名称（英文）
 * @property icon - Single character icon for the tab / 标签页图标（单字符）
 * @property tooltip - Tooltip text (bilingual) / 工具提示文本（双语）
 * @property color - CSS variable name for the module's accent color / 模块强调色的 CSS 变量名
 */
export interface ModuleConfig {
  id: ModuleType;
  label: string;
  icon: string;
  tooltip: string;
  color: string;
}

// ================================================================
// Theme Types / 主题类型
// ================================================================

/**
 * Supported application themes
 * - dark: Dark background with light foreground elements (default)
 * - light: Light background with dark foreground elements
 */
export type Theme = 'dark' | 'light';

/**
 * Canvas rendering color scheme for a given theme
 * @property canvasBg - Canvas background color
 * @property grid - Grid line color
 * @property axis - Coordinate axis color
 * @property point - Default point color
 * @property pointSelected - Selected point color
 * @property pointHover - Hovered point color
 * @property segment - Segment line color
 * @property text - Label/text color
 */
export interface ThemeColors {
  canvasBg: string;
  grid: string;
  axis: string;
  point: string;
  pointSelected: string;
  pointHover: string;
  segment: string;
  text: string;
  /** 端口颜色 / Port color */
  port: string;
  /** 端口悬停颜色 / Port hover color */
  portHover: string;
  /** 函数块填充色 / Function block fill color */
  funcBlockFill: string;
  /** 函数块边框色 / Function block border color */
  funcBlockStroke: string;
  /** 函数块文本色 / Function block text color */
  funcBlockText: string;
}

// ================================================================
// Backend Types / 后端类型
// ================================================================

/**
 * Backend type identifier / 后端类型标识
 * - wasm: WebAssembly high-performance backend / WebAssembly 高性能后端
 * - js: JavaScript fallback backend / JavaScript 回退后端
 * - null: Backend not initialized / 后端未初始化
 */
export type BackendType = 'wasm' | 'js' | null;

/**
 * Log level for filtering log messages / 日志级别（用于过滤日志消息）
 * Severity order: debug < info < warn < error
 * 严重程度顺序：debug < info < warn < error
 */
export type LogLevel = 'debug' | 'info' | 'warn' | 'error';

/**
 * Log entry structure / 日志条目结构
 *
 * @property timestamp - ISO 8601 timestamp string / ISO 8601 时间戳字符串
 * @property level - Log severity level / 日志严重程度级别
 * @property message - Log message content / 日志消息内容
 */
export interface LogEntry {
  timestamp: string;
  level: LogLevel;
  message: string;
}

// ================================================================
// Toast Types / Toast 通知类型
// ================================================================

/**
 * Toast notification variant / Toast 通知变体
 * - success: Green left border / 绿色左边框
 * - error: Red left border / 红色左边框
 * - warning: Yellow left border / 黄色左边框
 * - info: Blue left border / 蓝色左边框
 */
export type ToastVariant = 'success' | 'error' | 'warning' | 'info';

/**
 * Toast notification configuration / Toast 通知配置
 *
 * @property id - Unique toast identifier / 唯一 Toast 标识
 * @property variant - Visual variant / 视觉变体
 * @property message - Notification message / 通知消息
 * @property duration - Auto-dismiss duration in ms (0 = manual dismiss) / 自动消失时长（毫秒），0 表示手动关闭
 */
export interface Toast {
  id: string;
  variant: ToastVariant;
  message: string;
  duration: number;
}

// ================================================================
// Context Menu Types / 右键菜单类型
// ================================================================

/**
 * Context menu action
 * @property id - Action identifier
 * @property label - Display label (bilingual)
 * @property shortcut - Optional keyboard shortcut display
 */
export interface ContextMenuAction {
  id: string;
  label: string;
  shortcut?: string;
}

/**
 * Context menu position and items
 * @property x - Screen X position
 * @property y - Screen Y position
 * @property items - Menu items to display
 * @property target - Optional target point/segment ID for context actions
 * @property targetType - Type of target: 'point', 'segment', or 'empty'
 * @property worldX - World X coordinate of the click (for empty-space actions)
 * @property worldY - World Y coordinate of the click (for empty-space actions)
 */
export interface ContextMenuState {
  x: number;
  y: number;
  items: ContextMenuAction[];
  target?: number;
  targetType?: 'point' | 'segment' | 'empty';
  worldX?: number;
  worldY?: number;
}

// ================================================================
// Modal Types / 模态框类型
// ================================================================

/**
 * Modal dialog configuration / 模态对话框配置
 *
 * @property id - Modal identifier / 模态框标识
 * @property title - Dialog title (bilingual) / 对话框标题（双语）
 * @property content - Dialog body content (JSX or string) / 对话框正文内容（JSX 或字符串）
 * @property onConfirm - Confirm callback / 确认回调
 * @property onCancel - Cancel callback / 取消回调
 * @property confirmLabel - Confirm button label / 确认按钮标签
 * @property cancelLabel - Cancel button label / 取消按钮标签
 * @property danger - Whether the confirm action is destructive / 确认操作是否具有破坏性
 */
export interface ModalConfig {
  id: string;
  title: string;
  content: React.ReactNode;
  onConfirm?: () => void;
  onCancel?: () => void;
  confirmLabel?: string;
  cancelLabel?: string;
  danger?: boolean;
}

// ================================================================
// Performance Types / 性能类型
// ================================================================

/**
 * Performance statistics for the render loop / 渲染循环性能统计
 *
 * @property fps - Current frames per second / 当前每秒帧数
 * @property renderCount - Frame count since last FPS update / 自上次 FPS 更新以来的帧计数
 * @property avgRenderTime - Exponential moving average of render time (ms) / 渲染时间指数移动平均（毫秒）
 * @property lastFpsUpdate - Timestamp of last FPS calculation / 上次 FPS 计算的时间戳
 */
export interface PerfStats {
  fps: number;
  renderCount: number;
  avgRenderTime: number;
  lastFpsUpdate: number;
}

// ================================================================
// Undo/Redo Types / 撤销重做类型
// ================================================================

/**
 * Undo/redo snapshot of the complete geometry graph state.
 * 撤销/重做快照：保存完整的几何图状态。
 * @property points - Snapshot of all points / 所有点的快照
 * @property segments - Snapshot of all segments / 所有线段的快照
 * @property constraints - Snapshot of all constraints / 所有约束的快照
 * @property regions - Snapshot of all regions / 所有区域的快照
 * @property timestamp - When the snapshot was taken / 快照时间戳
 */
export interface UndoSnapshot {
  points: Point[];
  segments: Segment[];
  constraints: Constraint[];
  regions: Region[];
  /** 端口快照 / Ports snapshot */
  ports: Port[];
  /** 函数块快照 / Function blocks snapshot */
  funcBlocks: FuncBlock[];
  timestamp: number;
}

// ================================================================
// Panel Types / 面板类型
// ================================================================

/**
 * Panel collapse state
 * @property id - Panel identifier
 * @property collapsed - Whether the panel body is collapsed
 */
export interface PanelState {
  id: string;
  collapsed: boolean;
}

// ================================================================
// Formula Types / 公式类型
// ================================================================

/**
 * Formula syntax modes / 公式语法模式
 * - auto: Auto-detect syntax / 自动检测语法
 * - dsl: Lv-00 domain-specific language / Lv-00 领域特定语言
 * - latex: LaTeX notation / LaTeX 记法
 * - python: Python expression syntax / Python 表达式语法
 */
export type FormulaSyntax = 'auto' | 'dsl' | 'latex' | 'python';

/**
 * Formula output format / 公式输出格式
 * - latex: LaTeX 格式
 * - python: Python 表达式格式
 * - dsl: Lv-00 DSL 格式
 */
export type FormulaOutputFormat = 'latex' | 'python' | 'dsl';

// ================================================================
// Resize Types / 拖拽调整大小类型
// ================================================================

/**
 * Sidebar resize state / 侧边栏拖拽调整大小状态
 *
 * @property sidebar - Which sidebar is being resized / 正在调整大小的侧边栏
 * @property startX - Initial mouse X position / 鼠标初始 X 位置
 * @property startWidth - Initial sidebar width / 侧边栏初始宽度
 */
export interface ResizeState {
  sidebar: 'left' | 'right';
  startX: number;
  startWidth: number;
}

// ================================================================
// Streaming Types / 流式输出类型
// ================================================================

/**
 * Streaming event severity badge
 */
export type StreamingBadge = 'INFO' | 'WARN' | 'ERR' | 'OK' | 'STEP';

/**
 * Streaming log entry / 流式日志条目
 *
 * @property id - Unique entry identifier / 唯一条目标识
 * @property time - Formatted timestamp / 格式化的时间戳
 * @property badge - Severity badge / 严重程度徽标
 * @property step - Optional step number / 可选步骤编号
 * @property description - Event description / 事件描述
 */
export interface StreamingEntry {
  id: string;
  time: string;
  badge: StreamingBadge;
  step?: number;
  description: string;
}

// ================================================================
// Engine Stream Event Types / 引擎流式事件类型
// ================================================================

/**
 * Engine stream event categories for filtering in the UI.
 * Mirrors the category system in C engine's stream_parse_filter_mask().
 *
 * 引擎流式事件类别，用于 UI 中的过滤。
 * 与 C 引擎 stream_parse_filter_mask() 中的类别系统对齐。
 */
export type EngineStreamCategory =
  | 'engine'
  | 'normalize'
  | 'rewrite'
  | 'solve'
  | 'proof'
  | 'func_block'
  | 'conflict'
  | 'info';

/**
 * A single engine stream event from the C engine / Python bridge.
 * Fields mirror the C StreamEvent struct and stream_event_to_json() output.
 *
 * 来自 C 引擎 / Python 桥接的单个引擎流式事件。
 * 字段与 C StreamEvent 结构体和 stream_event_to_json() 输出对齐。
 */
export interface EngineStreamEvent {
  /** Event type identifier string (e.g. "ENGINE_START") */
  type: string;
  /** Human-readable event type name in Chinese */
  type_name: string;
  /** CSS color for rendering this event type */
  color: string;
  /** Event category for filtering */
  category: EngineStreamCategory;
  /** Unix timestamp in milliseconds */
  timestamp_ms: number;
  /** Sequential step number (-1 if not applicable) */
  step: number;
  /** Estimated total steps (-1 if unknown) */
  total_steps: number;
  /** Related node ID (-1 if none) */
  node_id: number;
  /** Related constraint ID (-1 if none) */
  constraint_id: number;
  /** Related rule ID (-1 if none) */
  rule_id: number;
  /** Related variable ID (-1 if none) */
  var_id: number;
  /** Human-readable description */
  description: string;
  /** Optional detailed JSON data */
  detail: string | null;
  /** Progress value 0.0-1.0 (-1 if not applicable) */
  progress: number;
  /** Numeric computation result (0 if not applicable) */
  numeric_value: number;
  /** Optional graph snapshot JSON */
  graph_snapshot: string | null;
}

/**
 * Connection state of the engine stream bridge.
 * 引擎流式桥接的连接状态。
 * - disconnected: 未连接 / 未建立连接
 * - connecting: 连接中 / 正在建立连接
 * - connected: 已连接 / 连接已建立
 * - error: 连接错误 / 连接发生错误
 */
export type EngineStreamState = 'disconnected' | 'connecting' | 'connected' | 'error';

/**
 * Category filter configuration for the streaming panel.
 * Each category maps to a set of event types from the C engine.
 *
 * 流式面板的类别过滤配置。
 * 每个类别映射到 C 引擎中的一组事件类型。
 */
export interface EngineStreamCategoryFilter {
  /** Category identifier matching EngineStreamCategory */
  category: EngineStreamCategory;
  /** Display label (English) */
  label: string;
  /** Display label (Chinese) */
  labelZh: string;
  /** Whether events of this category are currently shown */
  enabled: boolean;
  /** CSS color for the category badge */
  color: string;
  /** Number of events received for this category */
  count: number;
  /** Event type strings belonging to this category */
  eventTypes: string[];
}

/**
 * Statistics about engine stream events.
 * 引擎流式事件统计信息。
 */
export interface EngineStreamStats {
  /** Total events received */
  total: number;
  /** Events by category */
  byCategory: Record<string, number>;
  /** Events per second (rolling average) */
  eventsPerSecond: number;
  /** Start time of the current session */
  sessionStartMs: number;
  /** Duration of the current session in ms */
  sessionDurationMs: number;
}

// ================================================================
// AI Assistant Types / AI 助手类型
// ================================================================

/**
 * Chat message in the AI assistant conversation
 * AI 助手对话中的聊天消息
 *
 * @property id - Unique message identifier / 唯一消息标识
 * @property role - Sender role: user, assistant, or system / 发送者角色：用户、助手或系统
 * @property content - Message content (may contain markdown) / 消息内容（可能包含 Markdown）
 * @property timestamp - Unix timestamp in milliseconds / Unix 时间戳（毫秒）
 * @property isStreaming - Whether the message is currently being streamed / 消息是否正在流式传输
 */
export interface ChatMessage {
  id: string;
  role: 'user' | 'assistant' | 'system';
  content: string;
  timestamp: number;
  isStreaming?: boolean;
}

/**
 * AI provider configuration / AI 提供商配置
 *
 * @property id - Provider identifier / 提供商标识
 * @property name - Provider display name (English) / 提供商显示名称（英文）
 * @property nameZh - Provider display name (Chinese) / 提供商显示名称（中文）
 * @property endpoint - API endpoint URL / API 端点 URL
 * @property apiKey - API key (masked in UI) / API 密钥（UI 中脱敏显示）
 * @property enabled - Whether the provider is active / 提供商是否激活
 */
export interface AIProvider {
  id: string;
  name: string;
  nameZh: string;
  endpoint: string;
  apiKey: string;
  enabled: boolean;
}

/**
 * Streaming event from the AI backend / 来自 AI 后端的流式事件
 *
 * @property type - Event type code (0-17) / 事件类型编码（0-17）
 * @property description - Human-readable event description / 人类可读的事件描述
 * @property stepNumber - Sequential step number in the stream / 流中的顺序步骤编号
 * @property nodeId - Optional associated node ID / 可选的关联节点 ID
 * @property data - Optional additional event data / 可选的附加事件数据
 */
export interface StreamingEvent {
  type: number;
  description: string;
  stepNumber: number;
  nodeId?: number;
  data?: unknown;
  /** Optional Unix timestamp in milliseconds / 可选的 Unix 时间戳（毫秒） */
  timestamp?: number;
}

/**
 * Stream event filter configuration / 流式事件过滤配置
 *
 * @property type - Filter type identifier / 过滤类型标识
 * @property label - Filter label (English) / 过滤标签（英文）
 * @property labelZh - Filter label (Chinese) / 过滤标签（中文）
 * @property enabled - Whether events of this type are shown / 是否显示该类型的事件
 * @property count - Number of events matching this filter / 匹配该过滤条件的事件数量
 */
export interface StreamFilter {
  type: string;
  label: string;
  labelZh: string;
  enabled: boolean;
  count: number;
}

// ================================================================
// AI Provider Types / AI 提供商类型（统一定义）
// ================================================================

/**
 * AI 提供商标识符
 * AI provider identifier
 */
export type AIProviderId = 'openai' | 'deepseek' | 'dashscope' | 'anthropic' | 'gemini' | 'ollama' | 'custom';

/**
 * 右键菜单动作 ID 联合类型
 * Context menu action identifier union type
 */
export type ContextMenuActionId =
  | 'delete-point'
  | 'point-properties'
  | 'delete-segment'
  | 'segment-properties'
  | 'merge-nearest'
  | 'find-perpendicular'
  | 'add-point-here'
  | 'select-all'
  | 'deselect-all'
  | 'fit-view';

/**
 * 引擎流式事件类型枚举
 * 与 C 内核 stream.h 的 StreamEventType 完全一致（39种，0-38）
 *
 * Engine event type enum, fully aligned with C kernel stream.h StreamEventType (39 types, 0-38)
 *
 * 类别分组：
 * - 引擎生命周期 (0-2): ENGINE_START, ENGINE_DONE, ENGINE_PAUSED
 * - 归一化 (3-5): NORMALIZE_START, NORMALIZE_MERGE, NORMALIZE_DONE
 * - 重写引擎 (6-11): REWRITE_START ~ REWRITE_DONE
 * - 代数求解 (12-16): SOLVE_START ~ SOLVE_DONE
 * - 证明系统 (17-21): PROOF_STEP_ADDED ~ PROOF_DEPENDENCY_CHANGE
 * - 函数块系统 (22-29): FUNC_BLOCK_PACK_START ~ FUNC_BLOCK_CROSS_BOUNDARY
 * - 冲突与错误 (30-35): CONFLICT_DETECTED ~ WARNING
 * - 信息 (36-38): INFO, PROGRESS, GRAPH_SNAPSHOT
 */
export enum EngineEventType {
  /* ---- 引擎生命周期 ---- */
  ENGINE_START = 0,
  ENGINE_DONE = 1,
  ENGINE_PAUSED = 2,
  /* ---- 归一化 ---- */
  NORMALIZE_START = 3,
  NORMALIZE_MERGE = 4,
  NORMALIZE_DONE = 5,
  /* ---- 重写引擎 ---- */
  REWRITE_START = 6,
  REWRITE_RULE_LOADED = 7,
  REWRITE_MATCH_FOUND = 8,
  REWRITE_APPLIED = 9,
  REWRITE_ROLLBACK = 10,
  REWRITE_DONE = 11,
  /* ---- 代数求解 ---- */
  SOLVE_START = 12,
  SOLVE_EQUATION_EXTRACTED = 13,
  SOLVE_GROEBNER_STEP = 14,
  SOLVE_VARIABLE_RESOLVED = 15,
  SOLVE_DONE = 16,
  /* ---- 证明系统 ---- */
  PROOF_STEP_ADDED = 17,
  PROOF_STEP_APPLIED = 18,
  PROOF_UNIFY = 19,
  PROOF_COLOR_UPDATE = 20,
  PROOF_DEPENDENCY_CHANGE = 21,
  /* ---- 函数块系统 ---- */
  FUNC_BLOCK_PACK_START = 22,
  FUNC_BLOCK_PACK_DONE = 23,
  FUNC_BLOCK_INSTANTIATE_START = 24,
  FUNC_BLOCK_INSTANTIATE_DONE = 25,
  FUNC_BLOCK_PARTIAL_APPLY = 26,
  FUNC_BLOCK_DETERMINISM_CHECK = 27,
  FUNC_BLOCK_CAPTURE_AVOID = 28,
  FUNC_BLOCK_CROSS_BOUNDARY = 29,
  /* ---- 冲突与错误 ---- */
  CONFLICT_DETECTED = 30,
  CONSTRAINT_ADDED = 31,
  NODE_ADDED = 32,
  CIRCUIT_TRIP = 33,
  ERROR = 34,
  WARNING = 35,
  /* ---- 信息 ---- */
  INFO = 36,
  PROGRESS = 37,
  GRAPH_SNAPSHOT = 38,
}

/**
 * 将引擎事件类型编号映射到类别
 * Maps engine event type number to category
 * 与 C 内核 stream.h 的 39 种 StreamEventType 完全对齐
 * @param eventType 事件类型编号 (0-38)
 * @returns 事件类别
 */
export function getEventCategory(eventType: number): EngineStreamCategory {
  if (eventType <= EngineEventType.ENGINE_PAUSED) return 'engine';
  if (eventType <= EngineEventType.NORMALIZE_DONE) return 'normalize';
  if (eventType <= EngineEventType.REWRITE_DONE) return 'rewrite';
  if (eventType <= EngineEventType.SOLVE_DONE) return 'solve';
  if (eventType <= EngineEventType.PROOF_DEPENDENCY_CHANGE) return 'proof';
  if (eventType <= EngineEventType.FUNC_BLOCK_CROSS_BOUNDARY) return 'func_block';
  if (eventType <= EngineEventType.WARNING) return 'conflict';
  return 'info';
}
