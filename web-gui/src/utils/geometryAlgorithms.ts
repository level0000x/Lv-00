/**
 * @module utils/geometryAlgorithms
 * @description Pure JavaScript geometry algorithms for the Lv-00 GUI.
 *              Provides fallback implementations for constraint analysis,
 *              geometric calculations, and graph operations when the WASM
 *              backend is not available.
 *
 *              纯 JavaScript 几何算法工具库。
 *              当 WASM 后端不可用时，提供约束分析、几何计算和图操作的回退实现。
 */

import type { Point, Segment, Constraint } from '@/types';
import { MERGE_DISTANCE_THRESHOLD } from '@/utils/constants';

// ================================================================
// Geometric Calculations / 几何计算
// ================================================================

/**
 * Calculate the midpoint of two points.
 * 计算两点的中点。
 *
 * @param p1 - First point / 第一个点
 * @param p2 - Second point / 第二个点
 * @returns Midpoint coordinates / 中点坐标
 */
export function calculateMidpoint(p1: Point, p2: Point): { x: number; y: number } {
  return {
    x: (p1.x + p2.x) / 2,
    y: (p1.y + p2.y) / 2,
  };
}

/**
 * Calculate the intersection point of two line segments.
 * Returns null if the segments are parallel (no intersection).
 *
 * 计算两条线段的交点。
 * 如果线段平行（无交点），返回 null。
 *
 * Uses parametric line intersection:
 *   P = p1 + t * (p2 - p1)
 *   Q = p3 + u * (p4 - p3)
 *   Solve for t and u, then compute intersection.
 *
 * @param s1 - First segment { p1: Point, p2: Point }
 * @param s2 - Second segment { p1: Point, p2: Point }
 * @returns Intersection point or null / 交点或 null
 */
export function calculateIntersection(
  s1: { p1: Point; p2: Point },
  s2: { p1: Point; p2: Point },
): { x: number; y: number } | null {
  const { p1: a, p2: b } = s1;
  const { p1: c, p2: d } = s2;

  const dx1 = b.x - a.x;
  const dy1 = b.y - a.y;
  const dx2 = d.x - c.x;
  const dy2 = d.y - c.y;

  const denom = dx1 * dy2 - dy1 * dx2;

  // Parallel or collinear segments
  if (Math.abs(denom) < 1e-10) {
    return null;
  }

  const t = ((c.x - a.x) * dy2 - (c.y - a.y) * dx2) / denom;
  const u = ((c.x - a.x) * dy1 - (c.y - a.y) * dx1) / denom;

  // Check if intersection lies on both segments (within parameter range)
  // For graph constraints we allow extended lines, so we use a generous range
  if (t < -0.01 || t > 1.01 || u < -0.01 || u > 1.01) {
    return null;
  }

  return {
    x: a.x + t * dx1,
    y: a.y + t * dy1,
  };
}

/**
 * Calculate the three vertices of an equilateral triangle centered at origin.
 * 计算以原点为中心的等边三角形的三个顶点。
 *
 * @param side - Side length of the triangle / 三角形边长
 * @returns Array of 3 points forming the equilateral triangle / 等边三角形的 3 个顶点
 */
export function calculateEquilateralTriangle(side: number): Array<{ x: number; y: number }> {
  const h = (side * Math.sqrt(3)) / 2;
  // Center the triangle vertically
  const cy = h / 3;

  return [
    { x: 0, y: -cy },                  // Top vertex
    { x: -side / 2, y: h - cy },        // Bottom-left vertex
    { x: side / 2, y: h - cy },         // Bottom-right vertex
  ];
}

/**
 * Calculate the four vertices of a square centered at origin.
 * 计算以原点为中心的正方形的四个顶点。
 *
 * @param side - Side length of the square / 正方形边长
 * @returns Array of 4 points forming the square / 正方形的 4 个顶点
 */
export function calculateSquare(side: number): Array<{ x: number; y: number }> {
  const half = side / 2;
  return [
    { x: -half, y: -half },  // Bottom-left
    { x: half, y: -half },   // Bottom-right
    { x: half, y: half },    // Top-right
    { x: -half, y: half },   // Top-left
  ];
}

// ================================================================
// Graph Analysis / 图分析
// ================================================================

/**
 * Normalize a graph by merging nearby points.
 * Returns the merged point mappings and the number of merges performed.
 *
 * 通过合并邻近点来规范化图。
 * 返回合并映射和合并数量。
 *
 * @param points - Current points / 当前点列表
 * @param segments - Current segments / 当前线段列表
 * @param constraints - Current constraints / 当前约束列表
 * @param threshold - Distance threshold for merging / 合并距离阈值
 * @returns Object with merge results / 合并结果
 */
export function normalizeGraph(
  points: Point[],
  segments: Segment[],
  constraints: Constraint[],
  threshold: number = MERGE_DISTANCE_THRESHOLD,
): {
  mergedCount: number;
  mergeMap: Map<number, number>; // oldId -> keepId
  mergedPoints: Point[];
  mergedSegments: Segment[];
  mergedConstraints: Constraint[];
} {
  const mergeMap = new Map<number, number>();

  // Build merge map: for each pair of nearby points, merge the higher ID into the lower ID
  for (let i = 0; i < points.length; i++) {
    for (let j = i + 1; j < points.length; j++) {
      const pi = points[i];
      const pj = points[j];
      if (!pi || !pj) continue;

      const dx = pi.x - pj.x;
      const dy = pi.y - pj.y;
      const dist = Math.sqrt(dx * dx + dy * dy);

      if (dist < threshold) {
        // Merge the higher ID into the lower ID
        const keepId = Math.min(pi.id, pj.id);
        const removeId = Math.max(pi.id, pj.id);
        mergeMap.set(removeId, keepId);
      }
    }
  }

  // Resolve transitive merges (if A->B and B->C, then A->C)
  const resolvedMap = new Map<number, number>();
  for (const [oldId, newId] of mergeMap) {
    let finalId = newId;
    while (mergeMap.has(finalId)) {
      const mapped = mergeMap.get(finalId);
      if (!mapped) break;
      finalId = mapped;
    }
    resolvedMap.set(oldId, finalId);
  }

  const mergedCount = resolvedMap.size;

  // Helper to resolve a point ID through the merge map
  const resolveId = (id: number): number => {
    let current = id;
    while (resolvedMap.has(current)) {
      const resolved = resolvedMap.get(current);
      if (!resolved) break;
      current = resolved;
    }
    return current;
  };

  // Filter out merged points (keep only points that are not merged away)
  const mergedPointIds = new Set(resolvedMap.keys());
  const mergedPoints = points.filter((p) => !mergedPointIds.has(p.id));

  // Update segments: resolve point IDs and remove degenerate segments
  const mergedSegments = segments
    .map((s) => ({
      ...s,
      p1: resolveId(s.p1),
      p2: resolveId(s.p2),
    }))
    .filter((s) => s.p1 !== s.p2);

  // Remove duplicate segments
  const seenSegments = new Set<string>();
  const uniqueSegments: Segment[] = [];
  for (const s of mergedSegments) {
    const key = `${Math.min(s.p1, s.p2)}-${Math.max(s.p1, s.p2)}`;
    if (!seenSegments.has(key)) {
      seenSegments.add(key);
      uniqueSegments.push(s);
    }
  }

  // Update constraints: resolve all argument IDs
  const mergedConstraints = constraints.map((c) => ({
    ...c,
    args: c.args.map(resolveId),
  }));

  return {
    mergedCount,
    mergeMap: resolvedMap,
    mergedPoints,
    mergedSegments: uniqueSegments,
    mergedConstraints,
  };
}

/**
 * Detect redundant (duplicate) constraints.
 * A constraint is considered redundant if there exists another constraint
 * of the same type with the same arguments (regardless of order for symmetric types).
 *
 * 检测冗余（重复）约束。
 * 如果存在另一个相同类型且参数相同的约束（对称类型不考虑顺序），
 * 则该约束被视为冗余。
 *
 * @param constraints - List of constraints to check / 待检查的约束列表
 * @returns Array of redundant constraint IDs / 冗余约束 ID 列表
 */
export function detectRedundantConstraints(constraints: Constraint[]): number[] {
  const redundantIds: number[] = [];
  const seen = new Set<string>();

  for (const c of constraints) {
    // For symmetric types, normalize argument order
    let key: string;
    if (c.type === 'connection' || c.type === 'intersection') {
      const sortedArgs = [...c.args].sort((a, b) => a - b);
      key = `${c.type}:${sortedArgs.join(',')}`;
    } else {
      key = `${c.type}:${c.args.join(',')}`;
    }

    if (seen.has(key)) {
      redundantIds.push(c.id);
    } else {
      seen.add(key);
    }
  }

  return redundantIds;
}

/**
 * Detect conflicting constraints.
 * Currently detects:
 * - Two betweenness constraints with conflicting orderings (e.g., B between A,C and A between B,C)
 *
 * 检测冲突约束。
 * 当前检测：
 * - 两个介于约束的顺序冲突（如 B 介于 A,C 且 A 介于 B,C）
 *
 * @param constraints - List of constraints to check / 待检查的约束列表
 * @returns Array of conflict pairs / 冲突对列表
 */
export function detectConflicts(constraints: Constraint[]): Array<{ c1: number; c2: number; reason: string }> {
  const conflicts: Array<{ c1: number; c2: number; reason: string }> = [];
  const betweennessConstraints = constraints.filter((c) => c.type === 'betweenness');

  // Check for conflicting betweenness constraints
  for (let i = 0; i < betweennessConstraints.length; i++) {
    for (let j = i + 1; j < betweennessConstraints.length; j++) {
      const ci = betweennessConstraints[i];
      const cj = betweennessConstraints[j];
      if (!ci || !cj) continue;

      const [ai, bi, ci_] = [ci.args[0], ci.args[1], ci.args[2]];
      const [aj, bj, cj_] = [cj.args[0], cj.args[1], cj.args[2]];

      // Same triplet but different middle point
      const setI = new Set([ai, bi, ci_]);
      const setJ = new Set([aj, bj, cj_]);

      if (setI.size === 3 && setJ.size === 3) {
        // Check if they share the same 3 points
        let allSame = true;
        for (const val of setI) {
          if (!setJ.has(val)) {
            allSame = false;
            break;
          }
        }
        if (allSame && bi !== bj) {
          conflicts.push({
            c1: ci.id,
            c2: cj.id,
            reason: `Conflicting betweenness: B${bi} vs B${bj}`,
          });
        }
      }
    }
  }

  return conflicts;
}

/**
 * Calculate degrees of freedom for the constraint graph.
 * DOF = 2 * numPoints - numIndependentConstraints
 *
 * Each independent constraint reduces DOF by 1.
 * Duplicate constraints are counted only once.
 *
 * 计算约束图的自由度。
 * DOF = 2 * 点数 - 独立约束数
 *
 * @param points - List of points / 点列表
 * @param constraints - List of constraints / 约束列表
 * @returns Degrees of freedom / 自由度
 */
export function calculateDOF(points: Point[], constraints: Constraint[]): number {
  // Count unique constraints (exclude duplicates)
  const seen = new Set<string>();
  let uniqueCount = 0;

  for (const c of constraints) {
    let key: string;
    if (c.type === 'connection' || c.type === 'intersection') {
      const sortedArgs = [...c.args].sort((a, b) => a - b);
      key = `${c.type}:${sortedArgs.join(',')}`;
    } else {
      key = `${c.type}:${c.args.join(',')}`;
    }

    if (!seen.has(key)) {
      seen.add(key);
      uniqueCount++;
    }
  }

  return 2 * points.length - uniqueCount;
}

/**
 * Simple hash function for graph state comparison.
 * Produces a deterministic hash based on points, segments, and constraints.
 *
 * 图状态的简单哈希函数。
 * 基于点、线段和约束生成确定性哈希值。
 *
 * @param points - List of points / 点列表
 * @param segments - List of segments / 线段列表
 * @param constraints - List of constraints / 约束列表
 * @returns Hex string hash / 十六进制哈希字符串
 */
export function computeGraphHash(
  points: Point[],
  segments: Segment[],
  constraints: Constraint[],
): string {
  // Sort for deterministic output
  const sortedPoints = [...points].sort((a, b) => a.id - b.id);
  const sortedSegments = [...segments].sort((a, b) => a.id - b.id);
  const sortedConstraints = [...constraints].sort((a, b) => a.id - b.id);

  const pointData = sortedPoints.map((p) => `${p.id}:${p.x}:${p.y}`).join(';');
  const segData = sortedSegments.map((s) => `${s.id}:${s.p1}:${s.p2}`).join(';');
  const conData = sortedConstraints.map((c) => `${c.id}:${c.type}:${c.args.join(',')}`).join(';');

  const raw = `P[${pointData}]S[${segData}]C[${conData}]`;

  // DJB2 hash
  let hash = 5381;
  for (let i = 0; i < raw.length; i++) {
    hash = ((hash << 5) + hash + raw.charCodeAt(i)) | 0;
  }
  return (hash >>> 0).toString(16).padStart(8, '0');
}

/**
 * Topological sort of constraints based on dependency.
 * Constraints that reference points/segments created by other constraints
 * should come after those constraints.
 *
 * For now, returns constraints in insertion order (stable sort).
 *
 * 约束的拓扑排序。
 * 引用其他约束创建的点/线段的约束应排在那些约束之后。
 *
 * 当前按插入顺序返回（稳定排序）。
 *
 * @param constraints - List of constraints / 约束列表
 * @returns Sorted constraint IDs / 排序后的约束 ID 列表
 */
export function topologicalSort(constraints: Constraint[]): number[] {
  // Simple implementation: return in order of creation (by ID)
  return [...constraints].sort((a, b) => a.id - b.id).map((c) => c.id);
}

/**
 * Find merge candidate pairs among points that are very close together.
 * 查找距离非常近的点对作为合并候选。
 *
 * @param points - List of points / 点列表
 * @param threshold - Distance threshold / 距离阈值
 * @returns Array of candidate pairs / 候选对列表
 */
export function findMergeCandidates(
  points: Point[],
  threshold: number = MERGE_DISTANCE_THRESHOLD,
): Array<{ a: number; b: number; dist: number }> {
  const candidates: Array<{ a: number; b: number; dist: number }> = [];

  for (let i = 0; i < points.length; i++) {
    for (let j = i + 1; j < points.length; j++) {
      const pi = points[i];
      const pj = points[j];
      if (!pi || !pj) continue;

      const dx = pi.x - pj.x;
      const dy = pi.y - pj.y;
      const dist = Math.sqrt(dx * dx + dy * dy);

      if (dist < threshold) {
        candidates.push({ a: pi.id, b: pj.id, dist });
      }
    }
  }

  return candidates;
}
