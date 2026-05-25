/**
 * @module components/panels/utils/proofSearchTree
 * @description 回溯搜索树构建工具。
 *              基于 undoStack 历史构建 Newclid 风格的回溯搜索树，
 *              用于可视化证明搜索过程中的决策点和回溯操作。
 *
 *              Backtrack search tree builder.
 *              Builds Newclid-style backtrack search trees from undo stack history,
 *              for visualizing decision points and backtrack operations during proof search.
 */

import type { Constraint } from '@/types';

// ================================================================
// 类型定义 / Type Definitions
// ================================================================

/** 撤销栈快照（包含几何状态） */
export interface UndoSnapshot {
  points: Array<{ id: number; x: number; y: number }>;
  segments: Array<{ id: number; p1: number; p2: number }>;
  constraints: Constraint[];
  regions?: unknown[];
  ports?: unknown[];
  funcBlocks?: unknown[];
  timestamp?: number;
}

/** 回溯树节点状态 */
export type TreeNodeStatus = 'success' | 'failure' | 'choice' | 'pruned';

/** 回溯树子节点 */
export interface BacktrackChild {
  id: number;
  label: string;
  status: TreeNodeStatus;
  isBacktrack: boolean;
}

/** 回溯树节点 */
export interface BacktrackTreeNode {
  id: number;
  label: string;
  status: TreeNodeStatus;
  isBacktrack: boolean;
  children: BacktrackChild[];
}

// ================================================================
// 回溯树构建 / Backtrack Tree Construction
// ================================================================

/**
 * 基于 undoStack 历史构建 Newclid 风格的回溯搜索树。
 *
 * 构建逻辑：
 * 1. 每个 undo 快照作为一个决策点
 * 2. 检测回溯点：相邻快照约束数先增后减，标记为回溯
 * 3. 判断节点状态：基于约束数量和点数量
 * 4. 检测策略变更：比较相邻快照的约束类型分布
 * 5. 添加当前状态作为最终节点
 *
 * @param undoStack - 撤销栈快照数组
 * @param constraints - 当前约束集合
 * @param searchStrategy - 搜索策略名称
 * @returns 回溯树节点数组
 */
export function buildBacktrackTree(
  undoStack: UndoSnapshot[],
  constraints: Constraint[],
  searchStrategy: string,
): BacktrackTreeNode[] {
  // 空撤销栈：返回根节点
  if (undoStack.length === 0) {
    return [{
      id: 0,
      label: `ROOT: ${searchStrategy}`,
      status: 'choice',
      isBacktrack: false,
      children: [],
    }];
  }

  const tree: BacktrackTreeNode[] = [];

  for (let i = 0; i < undoStack.length; i++) {
    const snapshot = undoStack[i];
    if (!snapshot) continue;

    const constraintCount = snapshot.constraints ? snapshot.constraints.length : 0;
    const pointCount = snapshot.points ? snapshot.points.length : 0;

    // 判断是否为回溯点：如果下一步的约束数减少，说明发生了撤销/回溯
    let isBacktrack = false;
    if (i > 0) {
      const prev = undoStack[i - 1];
      if (prev && prev.constraints && snapshot.constraints &&
          snapshot.constraints.length < prev.constraints.length) {
        isBacktrack = true;
      }
    }

    // 判断节点状态
    let status: TreeNodeStatus = 'choice';
    if (constraintCount === 0 && pointCount <= 1) {
      status = 'pruned';
    } else if (isBacktrack) {
      status = 'failure';
    } else if (constraintCount >= 3) {
      status = 'success';
    }

    // 检测策略变更：比较相邻快照的约束类型分布
    let strategyLabel = searchStrategy;
    if (i > 0) {
      const prev = undoStack[i - 1];
      if (prev && snapshot.constraints && prev.constraints) {
        const prevTypes = new Set((prev.constraints || []).map((c: { type: string }) => c.type));
        const currTypes = new Set((snapshot.constraints || []).map((c: { type: string }) => c.type));
        // 检查是否有新的约束类型
        const hasNewTypes = [...currTypes].some((t) => !prevTypes.has(t));
        if (hasNewTypes) {
          strategyLabel = `${searchStrategy} [SWITCH]`;
        }
      }
    }

    const node: BacktrackTreeNode = {
      id: i,
      label: isBacktrack
        ? `↩ ${strategyLabel} #${i + 1} (回溯/backtrack)`
        : `${strategyLabel} #${i + 1}`,
      status,
      isBacktrack,
      children: [],
    };

    // 如果非回溯节点，添加其推理子节点（最多3个避免过长）
    if (!isBacktrack && constraintCount > 0) {
      (snapshot.constraints || []).forEach((c: { id: number; type: string }, ci: number) => {
        if (ci < 3) {
          node.children.push({
            id: i * 100 + ci,
            label: `${c.type} #${c.id ?? ci + 1}`,
            status: 'success',
            isBacktrack: false,
          });
        }
      });
    }

    tree.push(node);
  }

  // 添加当前状态作为最终节点
  const currentConstraintCount = constraints.length;
  const finalStatus: TreeNodeStatus =
    currentConstraintCount >= 3 ? 'success' : 'choice';

  const finalNode: BacktrackTreeNode = {
    id: undoStack.length,
    label: `${searchStrategy} #FINAL (当前/current)`,
    status: finalStatus,
    isBacktrack: false,
    children: constraints.slice(0, 3).map((c, ci) => ({
      id: undoStack.length * 100 + ci,
      label: `${c.type} #${c.id ?? ci + 1}`,
      status: 'success' as const,
      isBacktrack: false,
    })),
  };
  tree.push(finalNode);

  return tree;
}
