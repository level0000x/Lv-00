/**
 * @module components/panels/utils/graphAnalysis
 * @description 图模块分析工具。
 *              提供图分析结果的格式化输出，包括自由度、拓扑排序和图哈希。
 *              合并操作的工具函数也在此模块中。
 *
 *              Graph module analysis tools.
 *              Provides formatted output for graph analysis results,
 *              and utility functions for merge operations.
 */

import type { Point, Segment, Constraint } from '@/types';
import {
  calculateDOF,
  topologicalSort,
  computeGraphHash,
} from '@/utils/geometryAlgorithms';

// ================================================================
// 分析结果格式化 / Analysis Result Formatting
// ================================================================

/**
 * 计算并格式化自由度分析结果。
 *
 * @param points - 当前点集合
 * @param constraints - 当前约束集合
 * @returns 格式化的自由度字符串
 */
export function formatDOF(points: Point[], constraints: Constraint[]): string {
  const dof = calculateDOF(points, constraints);
  return `DOF = 2 * ${points.length} - ${constraints.length} = ${dof}`;
}

/**
 * 计算并格式化拓扑排序结果。
 *
 * @param constraints - 当前约束集合
 * @returns 格式化的拓扑排序字符串
 */
export function formatTopoSort(constraints: Constraint[]): string {
  const sorted = topologicalSort(constraints);
  return `拓扑排序: ${sorted.map((id) => `C${id}`).join(' -> ')}`;
}

/**
 * 计算并格式化图哈希结果。
 *
 * @param points - 当前点集合
 * @param segments - 当前线段集合
 * @param constraints - 当前约束集合
 * @returns 格式化的图哈希字符串
 */
export function formatGraphHash(
  points: Point[],
  segments: Segment[],
  constraints: Constraint[],
): string {
  const hash = computeGraphHash(points, segments, constraints);
  return `Hash: 0x${hash}`;
}

// ================================================================
// 合并操作工具 / Merge Operation Utilities
// ================================================================

/** 合并候选对 */
export interface MergeCandidate {
  a: number;
  b: number;
  dist: number;
}

/**
 * 执行两个点的合并操作。
 * 保留 ID 较小的点，更新所有引用被删除点的线段和约束。
 *
 * @param points - 当前点集合
 * @param segments - 当前线段集合
 * @param constraints - 当前约束集合
 * @param a - 第一个点 ID
 * @param b - 第二个点 ID
 * @returns 合并后的新点、线段、约束集合
 */
export function executeMerge(
  points: Point[],
  segments: Segment[],
  constraints: Constraint[],
  a: number,
  b: number,
): { newPoints: Point[]; newSegments: Segment[]; newConstraints: Constraint[] } {
  const keepId = Math.min(a, b);
  const removeId = Math.max(a, b);

  // 移除 ID 较大的点
  const newPoints = points.filter((p) => p.id !== removeId);
  // 更新线段中对被删除点的引用
  const newSegments = segments
    .map((s) => ({
      ...s,
      p1: s.p1 === removeId ? keepId : s.p1,
      p2: s.p2 === removeId ? keepId : s.p2,
    }))
    .filter((s) => s.p1 !== s.p2); // 移除自环
  // 更新约束中对被删除点的引用
  const newConstraints = constraints.map((c) => ({
    ...c,
    args: c.args.map((arg) => (arg === removeId ? keepId : arg)),
  }));

  return { newPoints, newSegments, newConstraints };
}
