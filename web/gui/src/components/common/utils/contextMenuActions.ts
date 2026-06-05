/**
 * @module components/common/utils/contextMenuActions
 * @description 右键菜单业务逻辑工具模块 / Context menu business logic utility module
 *
 * 从 ContextMenu.tsx 中提取的所有菜单操作处理函数。
 * 组件本身仅负责渲染，所有业务逻辑集中在此模块中，
 * 便于测试、维护和复用。
 *
 * Extracted menu action handlers from ContextMenu.tsx.
 * The component itself is only responsible for rendering;
 * all business logic is centralized here for easier testing,
 * maintenance, and reuse.
 */

import type { ContextMenuState, Point, Segment } from '@/types';
import { generateUniqueId } from '@/utils/idGenerator';
import { MERGE_NEAREST_PIXEL_THRESHOLD, MIN_SEGMENT_LENGTH_PX } from '@/utils/constants';
import { parseAndExecuteFormula } from '@/utils/formulaParser';

// ================================================================
// 类型定义 / Type Definitions
// ================================================================

/**
 * Store state 的最小子集，用于 getStore() 返回类型
 */
interface StoreSlice {
  points: Point[];
  segments: Segment[];
  selectedPoints: Point[];
  saveUndoState: () => void;
  removePoint: (id: number) => void;
  removeSegment: (id: number) => void;
  addConstraint: (c: { id: number; type: string; args: number[] }) => void;
  addPoint: (p: Point) => void;
  addSegment: (s: Segment) => void;
  setSegments: (s: Segment[]) => void;
  setSelectedPoints: (pts: Point[]) => void;
}

/**
 * Toast 通知函数类型 / Toast notification function type
 * @param variant - 通知类型（success / warning / error / info）
 * @param message - 通知消息文本
 */
export type ToastFn = (variant: 'success' | 'warning' | 'error' | 'info', message: string) => void;

/**
 * 操作处理函数的上下文参数 / Context parameters for action handler functions
 * @property contextMenu - 当前右键菜单状态
 * @property addToast - Toast 通知函数
 */
export interface ActionContext {
  /** 当前右键菜单状态（包含目标类型、坐标、菜单项等） */
  contextMenu: ContextMenuState;
  /** Toast 通知函数，用于向用户反馈操作结果 */
  addToast: ToastFn;
}

// ================================================================
// 辅助函数 / Helper Functions
// ================================================================

/**
 * 获取当前应用 store 状态的便捷函数
 * 通过 useAppStore.getState() 获取最新的全局状态
 *
 * Convenience function to get the current app store state.
 * Uses useAppStore.getState() to get the latest global state.
 */
function getStore(): StoreSlice {
  // 动态导入避免循环依赖 / Dynamic import to avoid circular dependency
  // eslint-disable-next-line @typescript-eslint/no-require-imports
  const { useAppStore } = require('@/stores');
  return useAppStore.getState();
}

// ================================================================
// 点操作 / Point Actions
// ================================================================

/**
 * 删除指定的点
 * 保存撤销状态后从画布中移除目标点。
 *
 * Delete a specific point.
 * Saves undo state before removing the target point from the canvas.
 *
 * @param ctx - 操作上下文
 */
function handleDeletePoint(ctx: ActionContext): void {
  const { contextMenu, addToast } = ctx;
  const state = getStore();
  const targetId = contextMenu.target;

  if (targetId === undefined) return;

  state.saveUndoState();
  state.removePoint(targetId);
  addToast('success', `已删除点 n${targetId} / Deleted point n${targetId}`);
}

/**
 * 显示指定点的属性信息
 * 以 Toast 通知形式展示点的 ID 和坐标。
 *
 * Show properties of a specific point.
 * Displays the point's ID and coordinates as a Toast notification.
 *
 * @param ctx - 操作上下文
 */
function handlePointProperties(ctx: ActionContext): void {
  const { contextMenu, addToast } = ctx;
  const state = getStore();
  const targetId = contextMenu.target;

  if (targetId === undefined) return;

  const pt = state.points.find((p) => p.id === targetId);
  if (pt) {
    addToast('info', `点 n${pt.id}: (${pt.x.toFixed(2)}, ${pt.y.toFixed(2)}) / Point n${pt.id}`);
  }
}

/**
 * 设置中点约束
 * 需要预先选中 2 个点，将目标点设为这两个点的中点（betweenness 约束）。
 *
 * Set midpoint constraint.
 * Requires 2 pre-selected points; sets the target point as the midpoint
 * (betweenness constraint) of the two selected points.
 *
 * @param ctx - 操作上下文
 */
function handleSetMidpoint(ctx: ActionContext): void {
  const { contextMenu, addToast } = ctx;
  const state = getStore();
  const targetId = contextMenu.target;

  const selectedPts = state.selectedPoints;
  if (selectedPts.length === 2 && targetId !== undefined) {
    state.saveUndoState();
    state.addConstraint({
      id: generateUniqueId(),
      type: 'betweenness',
      args: [selectedPts[0]!.id, targetId, selectedPts[1]!.id],
    });
    addToast('success', '已设为中点约束 / Midpoint constraint set');
  } else {
    addToast('warning', '请先选择 2 个点 / Select 2 points first');
  }
}

/**
 * 合并最近的点
 * 在像素阈值范围内查找距离目标点最近的点，合并后重定向相关线段。
 *
 * Merge with the nearest point.
 * Finds the closest point within the pixel threshold and merges,
 * redirecting related segments to the surviving point.
 *
 * @param ctx - 操作上下文
 */
function handleMergeNearest(ctx: ActionContext): void {
  const { contextMenu, addToast } = ctx;
  const state = getStore();
  const targetId = contextMenu.target;

  if (targetId === undefined) return;

  const pt = state.points.find((p) => p.id === targetId);
  if (!pt) return;

  // 查找距离最近的点（排除自身）/ Find the nearest point (excluding self)
  let nearest: typeof pt | null = null;
  let minDist = Infinity;
  for (const other of state.points) {
    if (other.id === targetId) continue;
    const dist = Math.sqrt((other.x - pt.x) ** 2 + (other.y - pt.y) ** 2);
    if (dist < minDist) {
      minDist = dist;
      nearest = other;
    }
  }

  if (nearest && minDist < MERGE_NEAREST_PIXEL_THRESHOLD) {
    state.saveUndoState();
    // 将引用目标点的线段重定向到最近点
    // Redirect segments referencing the target point to the nearest point
    const updatedSegments = state.segments.map((s) => {
      if (s.p1 === targetId) return { ...s, p1: nearest.id };
      if (s.p2 === targetId) return { ...s, p2: nearest.id };
      return s;
    });
    state.setSegments(updatedSegments);
    state.removePoint(targetId);
    addToast('success', `已合并 n${targetId} -> n${nearest.id} / Merged`);
  } else {
    addToast('warning', '附近没有可合并的点 / No nearby point to merge');
  }
}

// ================================================================
// 线段操作 / Segment Actions
// ================================================================

/**
 * 删除指定的线段
 * 保存撤销状态后从画布中移除目标线段。
 *
 * Delete a specific segment.
 * Saves undo state before removing the target segment from the canvas.
 *
 * @param ctx - 操作上下文
 */
function handleDeleteSegment(ctx: ActionContext): void {
  const { contextMenu, addToast } = ctx;
  const state = getStore();
  const targetId = contextMenu.target;

  if (targetId === undefined) return;

  state.saveUndoState();
  state.removeSegment(targetId);
  addToast('success', `已删除线段 s${targetId} / Deleted segment s${targetId}`);
}

/**
 * 显示指定线段的属性信息
 * 以 Toast 通知形式展示线段的 ID、端点及坐标。
 *
 * Show properties of a specific segment.
 * Displays the segment's ID, endpoints, and coordinates as a Toast notification.
 *
 * @param ctx - 操作上下文
 */
function handleSegmentProperties(ctx: ActionContext): void {
  const { contextMenu, addToast } = ctx;
  const state = getStore();
  const targetId = contextMenu.target;

  if (targetId === undefined) return;

  const seg = state.segments.find((s) => s.id === targetId);
  if (seg) {
    const p1 = state.points.find((p) => p.id === seg.p1);
    const p2 = state.points.find((p) => p.id === seg.p2);
    addToast(
      'info',
      `线段 s${seg.id}: n${seg.p1} -> n${seg.p2}` +
        (p1 && p2
          ? ` (${p1.x.toFixed(1)},${p1.y.toFixed(1)}) -> (${p2.x.toFixed(1)},${p2.y.toFixed(1)})`
          : ''),
    );
  }
}

/**
 * 在线段中点处添加新点
 * 将原线段拆分为两段，并自动添加 betweenness 约束。
 *
 * Add a new point at the midpoint of a segment.
 * Splits the original segment into two and automatically adds
 * a betweenness constraint.
 *
 * @param ctx - 操作上下文
 */
function handleAddMidpoint(ctx: ActionContext): void {
  const { contextMenu, addToast } = ctx;
  const state = getStore();
  const targetId = contextMenu.target;

  if (targetId === undefined) return;

  const seg = state.segments.find((s) => s.id === targetId);
  if (!seg) return;

  const p1 = state.points.find((p) => p.id === seg.p1);
  const p2 = state.points.find((p) => p.id === seg.p2);
  if (!p1 || !p2) return;

  state.saveUndoState();

  // 创建中点 / Create midpoint
  const midId = generateUniqueId();
  const midPoint = {
    id: midId,
    x: (p1.x + p2.x) / 2,
    y: (p1.y + p2.y) / 2,
  };
  state.addPoint(midPoint);

  // 将原线段拆分为两段 / Split the original segment into two
  state.removeSegment(targetId);
  state.addSegment({ id: generateUniqueId(), p1: seg.p1, p2: midId });
  state.addSegment({ id: generateUniqueId(), p1: midId, p2: seg.p2 });

  // 添加 betweenness 约束 / Add betweenness constraint
  state.addConstraint({
    id: generateUniqueId(),
    type: 'betweenness',
    args: [seg.p1, midId, seg.p2],
  });

  addToast('success', `已添加中点 n${midId} / Midpoint added`);
}

/**
 * 在线段中点处作垂线
 * 计算线段方向向量并旋转 90 度，在中点处创建垂线段。
 * 垂线长度为原线段长度的一半。
 *
 * Create a perpendicular line at the midpoint of a segment.
 * Calculates the segment direction vector, rotates 90 degrees,
 * and creates a perpendicular segment at the midpoint.
 * The perpendicular length is half the original segment length.
 *
 * @param ctx - 操作上下文
 */
function handleFindPerpendicular(ctx: ActionContext): void {
  const { contextMenu, addToast } = ctx;
  const state = getStore();
  const targetId = contextMenu.target;

  if (targetId === undefined) return;

  const seg = state.segments.find((s) => s.id === targetId);
  if (!seg) return;

  const p1 = state.points.find((p) => p.id === seg.p1);
  const p2 = state.points.find((p) => p.id === seg.p2);
  if (!p1 || !p2) return;

  // 计算线段方向向量 / Calculate segment direction vector
  const dx = p2.x - p1.x;
  const dy = p2.y - p1.y;
  const len = Math.sqrt(dx * dx + dy * dy);

  // 线段长度过短时拒绝操作 / Reject if segment is too short
  if (len < MIN_SEGMENT_LENGTH_PX) {
    addToast('warning', '线段太短 / Segment too short');
    return;
  }

  // 计算中点和法向量 / Calculate midpoint and normal vector
  const midX = (p1.x + p2.x) / 2;
  const midY = (p1.y + p2.y) / 2;
  const perpLen = len / 2;
  const nx = -dy / len; // 法向量 X 分量 / Normal vector X component
  const ny = dx / len; // 法向量 Y 分量 / Normal vector Y component

  // 创建垂线端点 / Create perpendicular endpoint
  const perpEndId = generateUniqueId();
  const perpEnd = {
    id: perpEndId,
    x: midX + nx * perpLen,
    y: midY + ny * perpLen,
  };

  state.saveUndoState();
  state.addPoint(perpEnd);

  // 创建中点（如果尚不存在）/ Create midpoint (if it doesn't exist yet)
  const midId = generateUniqueId();
  const midPoint = { id: midId, x: midX, y: midY };
  state.addPoint(midPoint);
  state.addSegment({ id: generateUniqueId(), p1: midId, p2: perpEndId });

  addToast('success', '已作垂线 / Perpendicular line created');
}

// ================================================================
// 空白空间操作 / Empty Space Actions
// ================================================================

/**
 * 在点击位置添加新点
 * 使用右键菜单的世界坐标创建新点。
 *
 * Add a new point at the click position.
 * Creates a new point using the context menu's world coordinates.
 *
 * @param ctx - 操作上下文
 */
function handleAddPointHere(ctx: ActionContext): void {
  const { contextMenu, addToast } = ctx;
  const state = getStore();

  if (contextMenu.worldX === undefined || contextMenu.worldY === undefined) return;

  state.saveUndoState();
  const newId = generateUniqueId();
  state.addPoint({
    id: newId,
    x: contextMenu.worldX,
    y: contextMenu.worldY,
  });
  addToast('success', `已添加点 n${newId} / Point added`);
}

/**
 * 从剪贴板粘贴 DSL 文本并解析执行
 * 读取系统剪贴板内容，通过公式解析器创建几何对象。
 * 处理剪贴板权限错误和解析错误。
 *
 * Paste DSL text from clipboard and parse/execute it.
 * Reads system clipboard content and creates geometry objects
 * via the formula parser. Handles clipboard permission and parse errors.
 *
 * @param ctx - 操作上下文
 */
async function handlePaste(ctx: ActionContext): Promise<void> {
  const { addToast } = ctx;
  const state = getStore();

  try {
    const clipText = await navigator.clipboard.readText();
    if (!clipText || !clipText.trim()) {
      addToast('warning', '剪贴板为空 / Clipboard is empty');
      return;
    }

    state.saveUndoState();

    const result = parseAndExecuteFormula(clipText.trim(), state.points);

    // 将解析结果添加到画布 / Add parsed results to the canvas
    result.createdPoints.forEach((p) => state.addPoint(p));
    result.createdSegments.forEach((s) => state.addSegment(s));
    result.createdConstraints.forEach((c) => state.addConstraint(c));

    if (result.createdPoints.length > 0 || result.createdSegments.length > 0) {
      addToast(
        'success',
        `粘贴成功: ${result.createdPoints.length} 点, ${result.createdSegments.length} 线段 / Pasted: ${result.createdPoints.length} pts, ${result.createdSegments.length} segs`,
      );
    } else if (result.errors.length > 0) {
      addToast('error', `粘贴失败: ${result.errors[0] ?? '无法解析剪贴板内容'} / Paste failed`);
    } else {
      addToast('info', '剪贴板内容未生成几何对象 / No geometry created from clipboard');
    }
  } catch (err) {
    const msg = err instanceof Error ? err.message : 'Unknown error';
    // 剪贴板 API 不可用（非 HTTPS 或权限被拒）
    // Clipboard API unavailable (non-HTTPS or permission denied)
    if (msg.includes('permission') || msg.includes('denied')) {
      addToast('warning', '剪贴板权限被拒绝，请检查浏览器设置 / Clipboard permission denied');
    } else {
      addToast('error', `粘贴失败: ${msg} / Paste failed: ${msg}`);
    }
  }
}

/**
 * 全选所有点
 * 将画布上所有点设为选中状态。
 *
 * Select all points.
 * Sets all points on the canvas as selected.
 *
 * @param ctx - 操作上下文
 */
function handleSelectAll(ctx: ActionContext): void {
  const { addToast } = ctx;
  const state = getStore();

  state.setSelectedPoints([...state.points]);
  addToast('info', `已全选 ${state.points.length} 个点 / Selected all ${state.points.length} points`);
}

// ================================================================
// 操作分发器 / Action Dispatcher
// ================================================================

/**
 * 操作 ID 到处理函数的映射表
 * Map of action IDs to their handler functions
 */
const ACTION_HANDLERS: Record<string, (ctx: ActionContext) => void | Promise<void>> = {
  // 点操作 / Point actions
  'delete-point': handleDeletePoint,
  'point-properties': handlePointProperties,
  'set-midpoint': handleSetMidpoint,
  'merge-nearest': handleMergeNearest,

  // 线段操作 / Segment actions
  'delete-segment': handleDeleteSegment,
  'segment-properties': handleSegmentProperties,
  'add-midpoint': handleAddMidpoint,
  'find-perpendicular': handleFindPerpendicular,

  // 空白空间操作 / Empty space actions
  'add-point-here': handleAddPointHere,
  'paste': handlePaste,
  'select-all': handleSelectAll,
};

/**
 * 执行右键菜单操作
 * 根据操作 ID 查找对应的处理函数并执行。
 * 对于未知的操作 ID，显示信息提示。
 *
 * Execute a context menu action.
 * Looks up the corresponding handler by action ID and executes it.
 * For unknown action IDs, shows an informational toast.
 *
 * @param actionId - 操作标识符 / Action identifier
 * @param ctx - 操作上下文 / Action context
 */
export function executeContextMenuAction(
  actionId: string,
  ctx: ActionContext,
): void {
  const handler = ACTION_HANDLERS[actionId];

  if (handler) {
    const result = handler(ctx);
    // 处理异步操作（如粘贴）/ Handle async operations (e.g., paste)
    if (result instanceof Promise) {
      result.catch((err) => {
        console.error('[ContextMenu] 操作执行失败 / Action execution failed:', err);
      });
    }
  } else {
    ctx.addToast('info', `操作: ${actionId} / Action: ${actionId}`);
  }
}
