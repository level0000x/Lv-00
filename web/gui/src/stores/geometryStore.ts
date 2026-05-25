/**
 * @module stores/geometryStore
 * @description 几何数据状态管理。
 *              管理所有几何图元（点、线段、约束、区域）以及完整的撤销/重做历史。
 *              从原先 stores/index.ts（~850 行单体 Store）拆分而来，遵循单一职责原则。
 *
 *              级联删除规则：
 *              - removePoint: 同时删除关联的线段、约束和包含该点的区域
 *              - removeSegment: 同时删除引用该线段 ID 的约束
 *
 *              撤销/重做性能说明：
 *              - 每次快照对所有数组进行一层浅拷贝（Point/Segment 为扁平对象，足够安全）
 *              - Constraint 的 args 数组和 Region 的 points 数组做了额外的数组拷贝
 *              - 最多保留 MAX_UNDO_HISTORY (50) 条快照，防止内存过度占用
 */

import { create } from 'zustand';
import type {
  Point,
  Segment,
  Constraint,
  Region,
  Port,
  FuncBlock,
  UndoSnapshot,
} from '@/types';
import { MAX_UNDO_HISTORY } from '@/utils/constants';

// ================================================================
// 几何状态接口 / Geometry State Interface
// ================================================================

/**
 * 几何数据与操作历史状态。
 * 所有几何图元的 CRUD 操作以及撤销/重做逻辑集中在此 Store 中。
 */
export interface GeometryState {
  // ---- 几何图元 / Geometry Primitives ----
  /** 所有点 */
  points: Point[];
  /** 所有线段 */
  segments: Segment[];
  /** 所有约束 */
  constraints: Constraint[];
  /** 所有区域 */
  regions: Region[];

  // ---- 撤销/重做 / Undo/Redo ----
  /** 撤销历史栈（最多保留 50 条快照，栈顶为最近状态） */
  undoStack: UndoSnapshot[];
  /** 重做历史栈（存储被撤销的快照，新操作会清空此栈） */
  redoStack: UndoSnapshot[];

  // ---- Actions: 几何图元操作 / Geometry Primitive Actions ----
  /** 添加一个点 */
  addPoint: (point: Point) => void;
  /** 添加一条线段 */
  addSegment: (segment: Segment) => void;
  /** 添加一个约束 */
  addConstraint: (constraint: Constraint) => void;
  /** 添加一个区域 */
  addRegion: (region: Region) => void;
  /** 删除一个点（同时删除关联的线段） */
  removePoint: (id: number) => void;
  /** 删除一条线段 */
  removeSegment: (id: number) => void;
  /** 批量设置点数据 */
  setPoints: (points: Point[]) => void;
  /** 批量设置线段数据 */
  setSegments: (segments: Segment[]) => void;
  /** 批量设置约束数据 */
  setConstraints: (constraints: Constraint[]) => void;
  /** 批量设置区域数据 */
  setRegions: (regions: Region[]) => void;
  /** 清空所有几何数据 */
  clearAll: () => void;

  // ---- Actions: 撤销/重做 / Undo/Redo Actions ----
  /**
   * 保存当前状态快照到撤销栈。
   * 在执行任何修改几何数据的操作之前调用此方法。
   * 新快照推入撤销栈；新操作使重做栈失效，因此清空重做栈。
   */
  saveUndoState: () => void;
  /**
   * 撤销操作。
   * 将当前状态保存到重做栈，从撤销栈弹出上一状态并恢复。
   */
  undo: () => void;
  /**
   * 重做操作。
   * 将当前状态保存回撤销栈，从重做栈弹出下一状态并恢复。
   */
  redo: () => void;
}

// ================================================================
// 辅助函数 / Helper Functions
// ================================================================

/**
 * 外部数据源注册机制。
 *
 * 当 portsStore 和 funcBlocksStore 独立创建后，通过调用
 * `registerPortsSource()` 和 `registerFuncBlocksSource()` 注入数据获取函数，
 * createSnapshot / restoreFromSnapshot 即可自动同步这些状态。
 *
 * 当前默认返回空数组，不产生运行时错误；集成真实 Store 时注入对应 getter。
 *
 * External data source registration mechanism.
 * When portsStore and funcBlocksStore are created separately,
 * call registerPortsSource() / registerFuncBlocksSource() to inject
 * data getter functions so that createSnapshot/restoreFromSnapshot
 * automatically sync these states.
 * Currently defaults to empty arrays; no runtime errors occur.
 */
let _portsSource: (() => Port[]) | null = null;
let _funcBlocksSource: (() => FuncBlock[]) | null = null;

/** 注册端口数据源（由 portsStore 初始化时调用） */
export function registerPortsSource(getPorts: () => Port[]): void {
  _portsSource = getPorts;
}

/** 注册函数块数据源（由 funcBlocksStore 初始化时调用） */
export function registerFuncBlocksSource(getFuncBlocks: () => FuncBlock[]): void {
  _funcBlocksSource = getFuncBlocks;
}

/** 获取当前端口数据快照（深拷贝，确保不可变性） */
function getPortsSnapshot(): Port[] {
  if (_portsSource === null) return [];
  return _portsSource().map((p) => ({ ...p }));
}

/** 获取当前函数块数据快照（深拷贝，确保不可变性） */
function getFuncBlocksSnapshot(): FuncBlock[] {
  if (_funcBlocksSource === null) return [];
  return _funcBlocksSource().map((fb) => ({
    ...fb,
    inputPorts: [...fb.inputPorts],
    outputPorts: [...fb.outputPorts],
  }));
}

/**
 * 提取当前几何数据的完整快照。
 * 对所有数组进行深拷贝，确保快照不受后续修改影响。
 *
 * ports 和 funcBlocks 数据通过注册的外部数据源获取；
 * 若对应 Store 尚未创建，则使用空数组作为安全回退。
 */
function createSnapshot(state: {
  points: Point[];
  segments: Segment[];
  constraints: Constraint[];
  regions: Region[];
}): UndoSnapshot {
  return {
    points: state.points.map((p) => ({ ...p })),
    segments: state.segments.map((s) => ({ ...s })),
    constraints: state.constraints.map((c) => ({ ...c, args: [...c.args] })),
    regions: state.regions.map((r) => ({
      ...r,
      points: r.points.map((p) => ({ ...p })),
    })),
    ports: getPortsSnapshot(),
    funcBlocks: getFuncBlocksSnapshot(),
    timestamp: Date.now(),
  };
}

/**
 * 从快照恢复几何状态 / Restore geometry state from snapshot
 *
 * @param snapshot - 要恢复的撤销快照
 * @returns 恢复后的几何状态对象
 */
function restoreFromSnapshot(snapshot: UndoSnapshot) {
  return {
    points: snapshot.points.map((p) => ({ ...p })),
    segments: snapshot.segments.map((s) => ({ ...s })),
    constraints: snapshot.constraints.map((c) => ({ ...c, args: [...c.args] })),
    regions: snapshot.regions.map((r) => ({ ...r, points: r.points.map((p) => ({ ...p })) })),
  };
}

// ================================================================
// Store 实现 / Store Implementation
// ================================================================

/**
 * 几何数据 Store。
 * 管理所有几何图元的增删改查以及完整的撤销/重做历史。
 *
 * 撤销/重做机制说明：
 * - undoStack：存储每次操作前的状态快照（最近快照在数组末尾）
 * - redoStack：存储被撤销的状态快照（新操作会清空此栈）
 * - undo()：当前状态 → 推入 redoStack → 从 undoStack 弹出 → 恢复
 * - redo()：当前状态 → 推入 undoStack → 从 redoStack 弹出 → 恢复
 * - saveUndoState()：每次执行几何修改操作前调用，保存当前状态
 *
 * @example
 * ```typescript
 * const { points, addPoint, undo, redo } = useGeometryStore();
 * addPoint({ id: 1, x: 100, y: 200 });
 * undo(); // 撤销上一步添加
 * redo(); // 重做被撤销的添加
 * ```
 */
export const useGeometryStore = create<GeometryState>((set) => ({
  // ---- 初始状态 / Initial State ----
  points: [],
  segments: [],
  constraints: [],
  regions: [],
  undoStack: [],
  redoStack: [],

  // ================================================================
  // Actions: 几何图元操作 / Geometry Primitive Actions
  // ================================================================

  /** 添加一个点到几何图中 */
  addPoint: (point) =>
    set((state) => ({ points: [...state.points, point] })),

  /** 添加一条线段到几何图中 */
  addSegment: (segment) =>
    set((state) => ({ segments: [...state.segments, segment] })),

  /** 添加一个约束到几何图中 */
  addConstraint: (constraint) =>
    set((state) => ({ constraints: [...state.constraints, constraint] })),

  /** 添加一个区域到几何图中 */
  addRegion: (region) =>
    set((state) => ({ regions: [...state.regions, region] })),

  /**
   * 删除指定 ID 的点。
   * 同时级联删除以此点为端点的所有线段、引用该点的所有约束，
   * 以及包含该点作为顶点的所有区域，保证数据一致性。
   */
  removePoint: (id) =>
    set((state) => ({
      points: state.points.filter((p) => p.id !== id),
      segments: state.segments.filter((s) => s.p1 !== id && s.p2 !== id),
      /* 级联删除引用该点的所有约束，保持数据一致性 */
      constraints: state.constraints.filter(
        (c) => !c.args.includes(id),
      ),
      /* 级联删除包含该点作为顶点的所有区域，保持数据一致性 */
      regions: state.regions.filter(
        (r) => !r.points.some((p) => p.id === id),
      ),
    })),

  /** 删除指定 ID 的线段 */
  removeSegment: (id) =>
    set((state) => ({
      segments: state.segments.filter((s) => s.id !== id),
      /* 级联删除引用该线段的所有约束，保持数据一致性 */
      constraints: state.constraints.filter(
        (c) => !c.args.includes(id),
      ),
    })),

  /** 批量设置所有点（替换现有数据） */
  setPoints: (points) => set({ points }),

  /** 批量设置所有线段（替换现有数据） */
  setSegments: (segments) => set({ segments }),

  /** 批量设置所有约束（替换现有数据） */
  setConstraints: (constraints) => set({ constraints }),

  /** 批量设置所有区域（替换现有数据） */
  setRegions: (regions) => set({ regions }),

  /** 清空所有几何数据（点、线段、约束、区域） */
  clearAll: () =>
    set({
      points: [],
      segments: [],
      constraints: [],
      regions: [],
    }),

  // ================================================================
  // Actions: 撤销/重做 / Undo/Redo Actions
  // ================================================================

  /**
   * 保存当前几何数据的完整状态快照到撤销栈。
   *
   * 调用时机：在任何修改几何数据的操作之前调用（在调用 addPoint、removePoint 等之前）。
   *
   * 行为：
   * 1. 对当前 points、segments、constraints、regions 进行深拷贝，生成快照
   * 2. 快照推入 undoStack 尾部
   * 3. 如果 undoStack 超过 MAX_UNDO_HISTORY (50)，移除最旧的快照
   * 4. 清空 redoStack（因为新操作使旧的重做历史失效）
   */
  saveUndoState: () =>
    set((state) => {
      const snapshot = createSnapshot(state);
      const newUndoStack = [...state.undoStack, snapshot];
      // 保持栈深度在限制内（超过则移除最旧记录）
      if (newUndoStack.length > MAX_UNDO_HISTORY) {
        newUndoStack.splice(0, newUndoStack.length - MAX_UNDO_HISTORY);
      }
      return {
        undoStack: newUndoStack,
        redoStack: [], // 新操作清空重做栈
      };
    }),

  /**
   * 撤销操作：恢复到上一个几何状态。
   *
   * 流程：
   * 1. 若 undoStack 为空，直接返回（无可撤销的操作）
   * 2. 将当前几何状态保存到 redoStack（方便后续重做）
   * 3. 从 undoStack 弹出最顶部的快照
   * 4. 将该快照中的 points、segments、constraints、regions 恢复到当前状态
   *
   * 此方法不应在 undo 操作前调用 saveUndoState（否则会丢失正确的撤销目标）。
   */
  undo: () =>
    set((state) => {
      if (state.undoStack.length === 0) return state;

      const newUndoStack = [...state.undoStack];
      /* 安全弹出：先检查长度确认非空，再安全取值，避免使用 ! 非空断言 */
      const snapshot = newUndoStack.pop();
      if (!snapshot) return state; // 防御性检查，理论上不会触发

      // 将当前状态保存到重做栈，便于后续重做
      const currentSnapshot = createSnapshot(state);
      const newRedoStack = [...state.redoStack, currentSnapshot];
      if (newRedoStack.length > MAX_UNDO_HISTORY) {
        newRedoStack.splice(0, newRedoStack.length - MAX_UNDO_HISTORY);
      }

      // 从快照恢复几何数据
      return {
        undoStack: newUndoStack,
        redoStack: newRedoStack,
        ...restoreFromSnapshot(snapshot),
      };
    }),

  /**
   * 重做操作：恢复刚刚被撤销的状态。
   *
   * 流程：
   * 1. 若 redoStack 为空，直接返回（无可重做的操作）
   * 2. 将当前几何状态保存回 undoStack
   * 3. 从 redoStack 弹出最顶部快照
   * 4. 将该快照中的几何数据恢复到当前状态
   *
   * 重新执行 undo() 可以再次撤销到此重做前的状态。
   */
  redo: () =>
    set((state) => {
      if (state.redoStack.length === 0) return state;

      const newRedoStack = [...state.redoStack];
      /* 安全弹出：先检查长度确认非空，再安全取值，避免使用 ! 非空断言 */
      const snapshot = newRedoStack.pop();
      if (!snapshot) return state; // 防御性检查，理论上不会触发

      // 将当前状态保存回撤销栈，以便再次撤销
      const currentSnapshot = createSnapshot(state);
      const newUndoStack = [...state.undoStack, currentSnapshot];
      if (newUndoStack.length > MAX_UNDO_HISTORY) {
        newUndoStack.splice(0, newUndoStack.length - MAX_UNDO_HISTORY);
      }

      // 从快照恢复几何数据
      return {
        redoStack: newRedoStack,
        undoStack: newUndoStack,
        ...restoreFromSnapshot(snapshot),
      };
    }),
}));
