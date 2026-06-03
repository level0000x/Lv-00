/**
 * @module stores/interactionStore
 * @description 交互状态管理。
 *              管理当前工具选择、点选/框选/拖拽状态、鼠标坐标、右键菜单、搜索栏。
 *              从原先 stores/index.ts（~850 行单体 Store）拆分而来，遵循单一职责原则。
 *
 *              工具切换时的状态清理规则：
 *              切换工具时自动清除以下状态，避免残留数据导致 UI 不一致：
 *              - selectedPoints（多选结果在新工具下不再有效）
 *              - isBoxSelecting / isDraggingPoint（操作状态重置）
 *              - regionPoints / segmentFirstPoint（工具专属中间状态）
 *              - boxSelectStart / dragStart（拖拽起始坐标）
 *              注意：selectedPoint（单选）和 isDragging（画布平移）不在此处清除，
 *              因为它们是跨工具共享的状态。
 */

import { create } from 'zustand';
import type {
  Point,
  ToolType,
  ContextMenuState,
} from '@/types';

// ================================================================
// 交互状态接口 / Interaction State Interface
// ================================================================

/**
 * 交互相关状态。
 * 涵盖画布工具切换、点选/框选/拖拽、鼠标坐标跟踪、右键菜单和搜索栏。
 *
 * 注意：切换工具时会自动清除以下状态以确保 UI 一致性：
 * - 多选点列表 (selectedPoints)
 * - 框选和拖拽点状态 (isBoxSelecting, isDraggingPoint)
 * - 区域点和线段首点 (regionPoints, segmentFirstPoint)
 */
export interface InteractionState {
  // ---- 工具 / Tool ----
  /** 当前活跃的画布工具 */
  tool: ToolType;

  // ---- 选择 / Selection ----
  /** 当前选中的单个点 */
  selectedPoint: Point | null;
  /** 多选（框选）中的点列表 */
  selectedPoints: Point[];
  /** 当前鼠标悬停的点 */
  hoveredPoint: Point | null;

  // ---- 拖拽 / Dragging ----
  /** 是否正在平移画布 */
  isDragging: boolean;
  /** 是否正在拖拽某个点 */
  isDraggingPoint: boolean;
  /** 是否正在进行框选（拉框选择多个点） */
  isBoxSelecting: boolean;
  /** 框选的起始屏幕坐标（clientX, clientY） */
  boxSelectStart: { x: number; y: number } | null;
  /** 拖拽起始屏幕坐标 */
  dragStart: { x: number; y: number } | null;
  /** 当前正在被拖拽的点对象 */
  dragPoint: Point | null;

  // ---- 区域与线段 / Region & Segment ----
  /** 区域定义过程中已点击的顶点列表 */
  regionPoints: Point[];
  /** 线段工具已选中的第一个端点 */
  segmentFirstPoint: Point | null;

  // ---- 鼠标坐标 / Mouse Coordinates ----
  /** 鼠标在世界坐标系中的 X 坐标 */
  mouseWorldX: number;
  /** 鼠标在世界坐标系中的 Y 坐标 */
  mouseWorldY: number;
  /** 鼠标在屏幕坐标系中的 X 坐标（相对于 Canvas 左上角） */
  mouseScreenX: number;
  /** 鼠标在屏幕坐标系中的 Y 坐标（相对于 Canvas 左上角） */
  mouseScreenY: number;

  // ---- 右键菜单 / Context Menu ----
  /** 右键菜单状态（null 表示隐藏） */
  contextMenu: ContextMenuState | null;

  // ---- 搜索 / Search ----
  /** 搜索栏是否可见 */
  searchVisible: boolean;
  /** 当前搜索查询字符串 */
  searchQuery: string;

  // ---- Actions / 操作方法 ----

  /** 设置当前工具，同时重置相关交互状态 */
  setTool: (tool: ToolType) => void;
  /** 设置当前选中的单个点 */
  setSelectedPoint: (point: Point | null) => void;
  /** 设置多选中的点列表 */
  setSelectedPoints: (points: Point[]) => void;
  /** 设置悬停点 */
  setHoveredPoint: (point: Point | null) => void;
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
  /** 添加一个区域顶点 */
  addRegionPoint: (point: Point) => void;
  /** 清除所有区域顶点 */
  clearRegionPoints: () => void;
  /** 设置线段第一个端点 */
  setSegmentFirstPoint: (point: Point | null) => void;
  /** 设置鼠标在世界坐标系中的位置 */
  setMouseWorld: (x: number, y: number) => void;
  /** 设置鼠标在屏幕坐标系中的位置 */
  setMouseScreen: (x: number, y: number) => void;
  /** 显示右键菜单 */
  showContextMenu: (state: ContextMenuState) => void;
  /** 隐藏右键菜单 */
  hideContextMenu: () => void;
  /** 设置搜索栏可见性 */
  setSearchVisible: (visible: boolean) => void;
  /** 设置搜索查询内容 */
  setSearchQuery: (query: string) => void;
}

// ================================================================
// Store 实现 / Store Implementation
// ================================================================

/**
 * 交互状态 Store。
 * 管理画布工具、选择、拖拽、鼠标坐标、右键菜单和搜索栏。
 *
 * @example
 * ```typescript
 * const tool = useInteractionStore((s) => s.tool);
 * const setTool = useInteractionStore((s) => s.setTool);
 * const { mouseWorldX, mouseWorldY } = useInteractionStore();
 * ```
 */
export const useInteractionStore = create<InteractionState>((set) => ({
  // ---- 初始状态 / Initial State ----
  tool: 'select',

  selectedPoint: null,
  selectedPoints: [],
  hoveredPoint: null,

  isDragging: false,
  isDraggingPoint: false,
  isBoxSelecting: false,
  boxSelectStart: null,
  dragStart: null,
  dragPoint: null,

  regionPoints: [],
  segmentFirstPoint: null,

  mouseWorldX: 0,
  mouseWorldY: 0,
  mouseScreenX: 0,
  mouseScreenY: 0,

  contextMenu: null,

  searchVisible: false,
  searchQuery: '',

  // ---- Actions / 操作方法 ----

  /**
   * 切换当前活跃的画布工具。
   * 切换时自动清除与旧工具关联的状态，避免残留数据导致 UI 不一致：
   * - 清除多选点列表（切换到新工具后框选结果不再有效）
   * - 清除框选和拖拽点状态
   * - 清除框选起始坐标和拖拽起始坐标
   * - 清除区域定义顶点列表
   * - 清除线段首点
   */
  setTool: (tool) =>
    set({
      tool,
      selectedPoints: [],
      isBoxSelecting: false,
      isDraggingPoint: false,
      boxSelectStart: null,
      dragStart: null,
      regionPoints: [],
      segmentFirstPoint: null,
    }),

  /** 设置当前选中的单个点 */
  setSelectedPoint: (point) => set({ selectedPoint: point }),

  /** 设置多选中的点列表 */
  setSelectedPoints: (points) => set({ selectedPoints: points }),

  /** 设置鼠标悬停的点 */
  setHoveredPoint: (point) => set({ hoveredPoint: point }),

  /** 设置画布平移拖拽状态 */
  setIsDragging: (dragging) => set({ isDragging: dragging }),

  /** 设置点拖拽状态 */
  setIsDraggingPoint: (dragging) => set({ isDraggingPoint: dragging }),

  /** 设置框选状态 */
  setIsBoxSelecting: (selecting) => set({ isBoxSelecting: selecting }),

  /** 设置框选起始坐标 */
  setBoxSelectStart: (start) => set({ boxSelectStart: start }),

  /** 设置拖拽起始坐标 */
  setDragStart: (start) => set({ dragStart: start }),

  /** 设置当前被拖拽的点 */
  setDragPoint: (point) => set({ dragPoint: point }),

  /** 添加区域定义的一个顶点 */
  addRegionPoint: (point) =>
    set((state) => ({ regionPoints: [...state.regionPoints, point] })),

  /** 清除所有区域定义顶点 */
  clearRegionPoints: () => set({ regionPoints: [] }),

  /** 设置线段工具的首个端点 */
  setSegmentFirstPoint: (point) => set({ segmentFirstPoint: point }),

  /** 设置鼠标在世界坐标系中的位置 */
  setMouseWorld: (x, y) => set({ mouseWorldX: x, mouseWorldY: y }),

  /** 设置鼠标在屏幕坐标系中的位置 */
  setMouseScreen: (x, y) => set({ mouseScreenX: x, mouseScreenY: y }),

  /** 显示右键菜单 */
  showContextMenu: (contextMenu) => set({ contextMenu }),

  /** 隐藏右键菜单 */
  hideContextMenu: () => set({ contextMenu: null }),

  /** 设置搜索栏可见性 */
  setSearchVisible: (visible) => set({ searchVisible: visible }),

  /** 设置搜索查询内容 */
  setSearchQuery: (query) => set({ searchQuery: query }),
}));
