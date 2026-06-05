/**
 * @module utils/geometryAlgorithms
 * @description 纯 JavaScript 几何算法工具库。
 *              当 WASM 后端不可用时，提供约束分析、几何计算和图操作的回退实现。
 */

import type { Point, Segment, Constraint } from '@/types';
import { MERGE_DISTANCE_THRESHOLD } from '@/utils/constants';

// ================================================================
// 几何计算
// ================================================================

/**
 * 计算两点的中点。
 *
 * @param p1 - 第一个点
 * @param p2 - 第二个点
 * @returns 中点坐标
 */
export function calculateMidpoint(p1: Point, p2: Point): { x: number; y: number } {
  return {
    x: (p1.x + p2.x) / 2,
    y: (p1.y + p2.y) / 2,
  };
}

/**
 * 计算两条线段的交点。
 * 如果线段平行（无交点），返回 null。
 *
 * 使用参数化直线求交公式：
 *   P = p1 + t * (p2 - p1)
 *   Q = p3 + u * (p4 - p3)
 *   求解 t 和 u，然后计算交点。
 *
 * @param s1 - 第一条线段 { p1: Point, p2: Point }
 * @param s2 - 第二条线段 { p1: Point, p2: Point }
 * @returns 交点或 null
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

  // 平行或共线线段
  if (Math.abs(denom) < 1e-10) {
    return null;
  }

  const t = ((c.x - a.x) * dy2 - (c.y - a.y) * dx2) / denom;
  const u = ((c.x - a.x) * dy1 - (c.y - a.y) * dx1) / denom;

  // 检查交点是否在两条线段上（参数范围）
  // 对于图约束，允许延长线上的交点，使用宽松范围
  if (t < -0.01 || t > 1.01 || u < -0.01 || u > 1.01) {
    return null;
  }

  return {
    x: a.x + t * dx1,
    y: a.y + t * dy1,
  };
}

/**
 * 计算以原点为中心的等边三角形的三个顶点。
 *
 * @param side - 三角形边长
 * @returns 等边三角形的 3 个顶点数组
 */
export function calculateEquilateralTriangle(side: number): Array<{ x: number; y: number }> {
  const h = (side * Math.sqrt(3)) / 2;
  // 垂直居中
  const cy = h / 3;

  return [
    { x: 0, y: -cy },                  // 顶部顶点
    { x: -side / 2, y: h - cy },        // 左下顶点
    { x: side / 2, y: h - cy },         // 右下顶点
  ];
}

/**
 * 计算以原点为中心的正方形的四个顶点。
 *
 * @param side - 正方形边长
 * @returns 正方形的 4 个顶点数组
 */
export function calculateSquare(side: number): Array<{ x: number; y: number }> {
  const half = side / 2;
  return [
    { x: -half, y: -half },  // 左下
    { x: half, y: -half },   // 右下
    { x: half, y: half },    // 右上
    { x: -half, y: half },   // 左上
  ];
}

// ================================================================
// 图分析
// ================================================================

/**
 * 通过合并邻近点来规范化图。
 * 返回合并映射和合并数量。
 *
 * @param points - 当前点列表
 * @param segments - 当前线段列表
 * @param constraints - 当前约束列表
 * @param threshold - 合并距离阈值
 * @returns 合并结果对象
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

  // 构建合并映射：对每对邻近点，将较大 ID 合并到较小 ID
  for (let i = 0; i < points.length; i++) {
    for (let j = i + 1; j < points.length; j++) {
      const pi = points[i];
      const pj = points[j];
      if (!pi || !pj) continue;

      const dx = pi.x - pj.x;
      const dy = pi.y - pj.y;
      const dist = Math.sqrt(dx * dx + dy * dy);

      if (dist < threshold) {
        // 将较大 ID 合并到较小 ID
        const keepId = Math.min(pi.id, pj.id);
        const removeId = Math.max(pi.id, pj.id);
        mergeMap.set(removeId, keepId);
      }
    }
  }

  // 解析传递性合并（如果 A->B 且 B->C，则 A->C）
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

  // 解析点 ID 的辅助函数
  const resolveId = (id: number): number => {
    let current = id;
    while (resolvedMap.has(current)) {
      const resolved = resolvedMap.get(current);
      if (!resolved) break;
      current = resolved;
    }
    return current;
  };

  // 过滤掉被合并的点（仅保留未被合并的点）
  const mergedPointIds = new Set(resolvedMap.keys());
  const mergedPoints = points.filter((p) => !mergedPointIds.has(p.id));

  // 更新线段：解析点 ID 并移除退化线段
  const mergedSegments = segments
    .map((s) => ({
      ...s,
      p1: resolveId(s.p1),
      p2: resolveId(s.p2),
    }))
    .filter((s) => s.p1 !== s.p2);

  // 移除重复线段
  const seenSegments = new Set<string>();
  const uniqueSegments: Segment[] = [];
  for (const s of mergedSegments) {
    const key = `${Math.min(s.p1, s.p2)}-${Math.max(s.p1, s.p2)}`;
    if (!seenSegments.has(key)) {
      seenSegments.add(key);
      uniqueSegments.push(s);
    }
  }

  // 更新约束：解析所有参数 ID
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
 * 检测冗余（重复）约束。
 * 如果存在另一个相同类型且参数相同的约束（对称类型不考虑顺序），
 * 则该约束被视为冗余。
 *
 * @param constraints - 待检查的约束列表
 * @returns 冗余约束 ID 列表
 */
export function detectRedundantConstraints(constraints: Constraint[]): number[] {
  const redundantIds: number[] = [];
  const seen = new Set<string>();

  for (const c of constraints) {
    // 对称类型需要归一化参数顺序
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
 * 检测冲突约束。
 * 当前检测：
 * - 两个介于约束的顺序冲突（如 B 介于 A,C 且 A 介于 B,C）
 *
 * @param constraints - 待检查的约束列表
 * @returns 冲突对列表
 */
export function detectConflicts(constraints: Constraint[]): Array<{ c1: number; c2: number; reason: string }> {
  const conflicts: Array<{ c1: number; c2: number; reason: string }> = [];
  const betweennessConstraints = constraints.filter((c) => c.type === 'betweenness');

  // 检查冲突的介于约束
  for (let i = 0; i < betweennessConstraints.length; i++) {
    for (let j = i + 1; j < betweennessConstraints.length; j++) {
      const ci = betweennessConstraints[i];
      const cj = betweennessConstraints[j];
      if (!ci || !cj) continue;

      const [ai, bi, ci_] = [ci.args[0], ci.args[1], ci.args[2]];
      const [aj, bj, cj_] = [cj.args[0], cj.args[1], cj.args[2]];

      // 相同三元组但不同的中间点
      const setI = new Set([ai, bi, ci_]);
      const setJ = new Set([aj, bj, cj_]);

      if (setI.size === 3 && setJ.size === 3) {
        // 检查是否共享相同的 3 个点
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
 * 计算约束图的自由度。
 * DOF = 2 * 点数 - 独立约束数
 *
 * 每个独立约束减少 1 个自由度。
 * 重复约束仅计数一次。
 *
 * @param points - 点列表
 * @param constraints - 约束列表
 * @returns 自由度
 */
export function calculateDOF(points: Point[], constraints: Constraint[]): number {
  // 统计唯一约束（排除重复）
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
 * 图状态的简单哈希函数。
 * 基于点、线段和约束生成确定性哈希值。
 *
 * @param points - 点列表
 * @param segments - 线段列表
 * @param constraints - 约束列表
 * @returns 十六进制哈希字符串
 */
export function computeGraphHash(
  points: Point[],
  segments: Segment[],
  constraints: Constraint[],
): string {
  // 排序以确保确定性输出
  const sortedPoints = [...points].sort((a, b) => a.id - b.id);
  const sortedSegments = [...segments].sort((a, b) => a.id - b.id);
  const sortedConstraints = [...constraints].sort((a, b) => a.id - b.id);

  const pointData = sortedPoints.map((p) => `${p.id}:${p.x}:${p.y}`).join(';');
  const segData = sortedSegments.map((s) => `${s.id}:${s.p1}:${s.p2}`).join(';');
  const conData = sortedConstraints.map((c) => `${c.id}:${c.type}:${c.args.join(',')}`).join(';');

  const raw = `P[${pointData}]S[${segData}]C[${conData}]`;

  // DJB2 哈希算法
  let hash = 5381;
  for (let i = 0; i < raw.length; i++) {
    hash = ((hash << 5) + hash + raw.charCodeAt(i)) | 0;
  }
  return (hash >>> 0).toString(16).padStart(8, '0');
}

/**
 * 约束的拓扑排序。
 * 引用其他约束创建的点/线段的约束应排在那些约束之后。
 *
 * 使用 Kahn 算法实现真正的拓扑排序：
 * 1. 构建邻接表：如果约束 A 的参数中引用了约束 B 参数中的元素，
 *    且 B 的 ID 小于 A，则 A 依赖于 B（B → A）。
 * 2. 计算每个节点的入度。
 * 3. 从入度为 0 的节点开始处理，逐步减少邻居的入度。
 * 4. 返回排序后的约束 ID 列表。
 *
 * @param constraints - 约束列表
 * @returns 排序后的约束 ID 列表
 */
export function topologicalSort(constraints: Constraint[]): number[] {
  if (constraints.length === 0) return [];

  const ids = constraints.map((c) => c.id);

  // 构建每个约束引用的元素集合
  const argsSet = constraints.map((c) => new Set(c.args));

  // 构建邻接表和入度数组
  // adjacency[i] = 约束 i 依赖的约束索引列表（即 i 必须在这些之后）
  const n = constraints.length;
  const adjacency: number[][] = Array.from({ length: n }, () => []);
  const inDegree = new Array<number>(n).fill(0);

  for (let i = 0; i < n; i++) {
    for (let j = 0; j < n; j++) {
      if (i === j) continue;
      // 如果约束 i 的参数中引用了约束 j 参数中的元素，且 j.id < i.id，
      // 则约束 i 依赖于约束 j
      if (constraints[j].id < constraints[i].id) {
        for (const arg of constraints[i].args) {
          if (argsSet[j].has(arg)) {
            adjacency[j].push(i);
            inDegree[i]++;
            break; // 每对约束只建立一条边
          }
        }
      }
    }
  }

  // Kahn 算法：从入度为 0 的节点开始
  const queue: number[] = [];
  for (let i = 0; i < n; i++) {
    if (inDegree[i] === 0) {
      queue.push(i);
    }
  }

  // 按约束 ID 排序以保证稳定性
  queue.sort((a, b) => constraints[a].id - constraints[b].id);

  const result: number[] = [];
  while (queue.length > 0) {
    const node = queue.shift()!;
    result.push(constraints[node].id);

    for (const neighbor of adjacency[node]) {
      inDegree[neighbor]--;
      if (inDegree[neighbor] === 0) {
        queue.push(neighbor);
        // 保持队列按 ID 排序以确保稳定输出
        queue.sort((a, b) => constraints[a].id - constraints[b].id);
      }
    }
  }

  // 如果存在环，返回未处理的约束（追加到末尾）
  if (result.length < n) {
    for (const c of constraints) {
      if (!result.includes(c.id)) {
        result.push(c.id);
      }
    }
  }

  return result;
}

/**
 * 查找距离非常近的点对作为合并候选。
 *
 * @param points - 点列表
 * @param threshold - 距离阈值
 * @returns 候选对列表
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
