/**
 * @module utils/constants
 * @description 应用全局常量。
 *              集中管理魔法数字和配置值，避免跨模块硬编码。
 */

// ================================================================
// 版本
// ================================================================

/** 应用程序版本号（需与 lv00.h 中 LV00_VERSION_STRING 保持一致） */
export const APP_VERSION = '3.3.0';

/** 应用程序名称 */
export const APP_NAME = 'Lv-00';

/** 应用程序完整标题 */
export const APP_TITLE = 'Lv-00 -- Symbolic Geometry Engine';

// ================================================================
// 画布
// ================================================================

/** 最小缩放比例 */
export const SCALE_MIN = 0.01;

/** 最大缩放比例 */
export const SCALE_MAX = 10000;

/** 默认缩放比例 */
export const SCALE_DEFAULT = 1;

/** 基础网格间距（CSS 像素） */
export const BASE_GRID_SIZE = 50;

/** 普通点半径（CSS 像素） */
export const POINT_RADIUS = 4;

/** 选中/悬停点半径 */
export const POINT_RADIUS_ACTIVE = 6;

/** 命中检测阈值（CSS 像素） */
export const HIT_THRESHOLD = 10;

/** 线段线宽 */
export const SEGMENT_LINE_WIDTH = 2;

// ================================================================
// 缩放
// ================================================================

/** 键盘缩放步进因子 */
export const ZOOM_STEP = 1.2;

/** 鼠标滚轮平滑缩放因子 */
export const ZOOM_SMOOTH = 1.0015;

// ================================================================
// 撤销重做
// ================================================================

/** 最大撤销历史条目数 */
export const MAX_UNDO_HISTORY = 50;

// ================================================================
// 性能
// ================================================================

/** 性能监控间隔（毫秒） */
export const PERF_MONITOR_INTERVAL = 1000;

/** 目标渲染帧间隔（约 60fps） */
export const RENDER_THROTTLE_MS = 16;

/** 鼠标移动节流间隔（约 120fps） */
export const MOUSE_MOVE_THROTTLE_MS = 8;

/** 渲染时间平滑 EMA 系数 */
export const PERF_EMA_ALPHA = 0.1;

// ================================================================
// 界面
// ================================================================

/** 默认左侧边栏宽度（像素） */
export const SIDEBAR_LEFT_DEFAULT = 280;

/** 默认右侧边栏宽度（像素） */
export const SIDEBAR_RIGHT_DEFAULT = 280;

/** 最小侧边栏宽度（像素） */
export const SIDEBAR_MIN_WIDTH = 200;

/** 最大左侧边栏宽度（像素） */
export const SIDEBAR_LEFT_MAX_WIDTH = 400;

/** 最大右侧边栏宽度（像素） */
export const SIDEBAR_RIGHT_MAX_WIDTH = 360;

/** 默认 Toast 显示时长（毫秒） */
export const TOAST_DURATION_DEFAULT = 3000;

/** 默认递归深度 */
export const RECURSION_DEPTH_DEFAULT = 10;

// ================================================================
// 导出
// ================================================================

/** 导出后 DOM 清理延迟（毫秒） */
export const EXPORT_CLEANUP_MS = 100;

// ================================================================
// 主题
// ================================================================

/** LocalStorage 主题持久化键名 */
export const THEME_STORAGE_KEY = 'lv00-theme';

// ================================================================
// AI 与存储上限
// ================================================================

/** 聊天消息最大保留数 */
export const MAX_CHAT_MESSAGES = 100;

/** 全局日志最大保留条目数 */
export const MAX_GLOBAL_LOG_ENTRIES = 500;

/** 面板日志最大保留条目数 */
export const MAX_PANEL_LOG_ENTRIES = 500;

/** 流式事件最大保留数 */
export const MAX_STREAMING_EVENTS = 500;

/** 流式日志条目最大保留数 */
export const MAX_STREAMING_ENTRIES = 500;

/** 默认模型温度 (0-2) */
export const MODEL_TEMPERATURE_DEFAULT = 0.7;

/** 默认最大 Token 数 (64-32768) */
export const MODEL_MAX_TOKENS_DEFAULT = 2048;

/** 默认 AI 提供者 ID */
export const DEFAULT_AI_PROVIDER = 'openai';

/** AI 模拟流式响应最小延迟（毫秒） */
export const AI_SIMULATED_DELAY_MIN = 80;

/** AI 模拟流式响应最大延迟（毫秒） */
export const AI_SIMULATED_DELAY_MAX = 200;

// ================================================================
// 图操作
// ================================================================

/** 合并候选点距离阈值（画布坐标系） */
export const MERGE_DISTANCE_THRESHOLD = 0.5;

// ================================================================
// 渲染（复用于 renderer.ts）
// ================================================================

/** 选中点外圈偏移 */
export const POINT_OUTER_OFFSET = 3;

/** 坐标轴线宽 */
export const AXIS_LINE_WIDTH = 2;

// ================================================================
// 几何
// ================================================================

/** 圆的近似多边形边数 */
export const CIRCLE_APPROX_SIDES = 24;

/** 浮点相等比较容差 */
export const FP_EPSILON = 1e-10;

/** 共线判断容差 */
export const COLLINEARITY_TOLERANCE = 0.1;

/** 介于判断容差 */
export const BETWEENNESS_TOLERANCE = 0.1;

/** 交点误差阈值 */
export const INTERSECTION_ERROR_THRESHOLD = 0.5;

/** 归一化距离阈值 */
export const NORMALIZE_DISTANCE_THRESHOLD = 0.5;

// ================================================================
// AI / SSE 配置
// ================================================================

/** SSE 重连默认延迟（毫秒） */
export const SSE_RECONNECT_DELAY = 3000;

/** SSE 最大重连次数 */
export const SSE_MAX_RECONNECT_ATTEMPTS = 5;

// ================================================================
// 流式输出
// ================================================================

/** 流式面板最大可见事件数 */
export const MAX_VISIBLE_STREAM_EVENTS = 200;

/** 搜索防抖延迟（毫秒） */
export const SEARCH_DEBOUNCE_MS = 250;

/** 搜索结果最大显示数 */
export const MAX_SEARCH_RESULTS = 50;

// ================================================================
// 右键菜单
// ================================================================

/** 合并最近点距离阈值（像素） */
export const MERGE_NEAREST_PIXEL_THRESHOLD = 50;

/** 线段最短长度阈值（像素） */
export const MIN_SEGMENT_LENGTH_PX = 1;

// ================================================================
// 引擎
// ================================================================

/** 引擎默认 WebSocket 地址 */
export const ENGINE_DEFAULT_WS_URL = 'ws://localhost:3456';

/** 有理数分母精度 */
export const RATIONAL_DENOMINATOR = 1000000;

// ================================================================
// 端口和函数块渲染
// ================================================================

/** 端口半径（CSS 像素） */
export const PORT_RADIUS = 5;

/** 函数块端口半径（CSS 像素） */
export const FUNC_BLOCK_PORT_RADIUS = 4;

/** 关联标记大小（CSS 像素） */
export const INCIDENCE_MARKER_SIZE = 5;
