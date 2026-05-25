/**
 * @module utils/constraintSolver
 * @description 约束求解纯函数工具模块。
 *
 *              从 EnginePanel 中提取的几何约束求解逻辑，
 *              包含以下纯函数（无副作用，不依赖 React 状态）：
 *              - dist              两点距离计算
 *              - solveIncidence    求解 incidence 约束（点投影到线段）
 *              - solveBetweenness  求解 betweenness 约束（中间点验证/调整）
 *              - solveIntersection 求解 intersection 约束（两线段交点）
 *              - performSolve      执行完整的约束求解流程
 *              - normalizePoints   归一化（合并距离过近的点）
 */

import type { Point, Segment, Constraint } from '@/types';

// ================================================================
// 类型定义
// ================================================================

/** 引擎求解结果 */
export interface SolveResult {
  /** 满足的约束数量 */
  satisfied: number;
  /** 冲突的约束数量 */
  conflicts: number;
  /** 总约束数量 */
  total: number;
  /** 约束满足百分比 */
  satisfactionRate: number;
  /** 自由变量数量（未被约束固定的坐标数） */
  freeVariables: number;
  /** 详细日志 */
  details: string[];
}

// ================================================================
// 约束求解辅助函数
// ================================================================

/**
 * 计算两点之间的欧几里得距离
 *
 * @param a - 第一个点
 * @param b - 第二个点
 * @returns 两点之间的距离
 */
export function dist(a: Point, b: Point): number {
  return Math.sqrt((a.x - b.x) ** 2 + (a.y - b.y) ** 2);
}

/**
 * 求解 incidence 约束 —— 将点投影到线段上
 *
 * 通过计算点在线段上的最近投影点来满足 incidence 约束。
 * 投影参数 t 会被限制在 [0, 1] 范围内（线段端点之间）。
 *
 * @param pointId   - 需要投影的点的 ID
 * @param segmentId - 目标线段的 ID
 * @param points    - 所有点的数组
 * @param segments  - 所有线段的数组
 * @returns 包含调整后点坐标和详细信息的对象，如果约束无法满足则返回 null
 */
export function solveIncidence(
  pointId: number,
  segmentId: number,
  points: Point[],
  segments: Segment[],
): { adjustedPoint: Point; detail: string } | null {
  const pt = points.find((p) => p.id === pointId);
  const seg = segments.find((s) => s.id === segmentId);
  if (!pt || !seg) return null;

  const a = points.find((p) => p.id === seg.p1);
  const b = points.find((p) => p.id === seg.p2);
  if (!a || !b) return null;

  // 计算投影
  const dx = b.x - a.x;
  const dy = b.y - a.y;
  const lenSq = dx * dx + dy * dy;
  if (lenSq < 1e-10) return null;

  const t = Math.max(0, Math.min(1, ((pt.x - a.x) * dx + (pt.y - a.y) * dy) / lenSq));
  const projX = a.x + t * dx;
  const projY = a.y + t * dy;

  const error = dist(pt, { id: -1, x: projX, y: projY });
  const adjustedPoint: Point = { id: pt.id, x: projX, y: projY };

  return {
    adjustedPoint,
    detail: `incidence: 点 ${pt.id} 投影到线段 ${seg.id} 上, 误差 ${error.toFixed(4)}`,
  };
}

/**
 * 求解 betweenness 约束 —— 验证/调整中间点
 *
 * 约束 args: [pointA, pointB, pointC]，B 应在 A 和 C 之间。
 * 通过叉积判断共线性，通过点积判断 B 是否在 A 和 C 之间。
 * 如果不满足，则将 B 调整到 A 和 C 的中点。
 *
 * @param constraint - betweenness 约束对象
 * @param points     - 所有点的数组
 * @returns 包含调整后点、是否满足和详细信息的对象
 */
export function solveBetweenness(
  constraint: Constraint,
  points: Point[],
): { adjustedPoint: Point | null; satisfied: boolean; detail: string } {
  const aId = constraint.args[0];
  const bId = constraint.args[1];
  const cId = constraint.args[2];
  if (aId === undefined || bId === undefined || cId === undefined) {
    return { adjustedPoint: null, satisfied: false, detail: `betweenness: 参数不足 (${constraint.args.join(', ')})` };
  }
  const a = points.find((p) => p.id === aId);
  const b = points.find((p) => p.id === bId);
  const c = points.find((p) => p.id === cId);

  if (!a || !b || !c) {
    return { adjustedPoint: null, satisfied: false, detail: `betweenness: 点不存在 (${constraint.args.join(', ')})` };
  }

  // 检查 B 是否在 A 和 C 之间（共线且在中间）
  const abx = b.x - a.x;
  const aby = b.y - a.y;
  const acx = c.x - a.x;
  const acy = c.y - a.y;
  const cross = abx * acy - aby * acx;

  // 叉积接近零 → 共线
  if (Math.abs(cross) > 0.1) {
    return {
      adjustedPoint: null,
      satisfied: false,
      detail: `betweenness: 点 ${bId} 不在 ${aId}-${cId} 线上 (叉积=${cross.toFixed(4)})`,
    };
  }

  // 检查 B 是否在 A 和 C 之间
  const dotAB = abx * abx + aby * aby;
  const dotAC = acx * acx + acy * acy;
  const dotBC = (c.x - b.x) * acx + (c.y - b.y) * acy;

  const isBetween = dotAB >= -0.1 && dotBC >= -0.1 && dotAB <= dotAC + 0.1;

  if (isBetween) {
    return {
      adjustedPoint: null,
      satisfied: true,
      detail: `betweenness: 点 ${bId} 在 ${aId}-${cId} 之间 (已满足)`,
    };
  }

  // 调整 B 到 A 和 C 的中点
  const midX = (a.x + c.x) / 2;
  const midY = (a.y + c.y) / 2;
  return {
    adjustedPoint: { id: bId, x: midX, y: midY },
    satisfied: false,
    detail: `betweenness: 调整点 ${bId} 到中点 (${midX.toFixed(2)}, ${midY.toFixed(2)})`,
  };
}

/**
 * 求解 intersection 约束 —— 计算两线段交点
 *
 * 约束 args: [intersectionPointId, segment1Id, segment2Id]。
 * 使用参数方程法计算两线段的交点坐标。
 *
 * @param constraint - intersection 约束对象
 * @param points     - 所有点的数组
 * @param segments   - 所有线段的数组
 * @returns 包含调整后交点、是否满足和详细信息的对象
 */
export function solveIntersection(
  constraint: Constraint,
  points: Point[],
  segments: Segment[],
): { adjustedPoint: Point | null; satisfied: boolean; detail: string } {
  const [ptId, seg1Id, seg2Id] = constraint.args;
  if (ptId === undefined || seg1Id === undefined || seg2Id === undefined) {
    return { adjustedPoint: null, satisfied: false, detail: 'intersection: 参数不足' };
  }
  const seg1 = segments.find((s) => s.id === seg1Id);
  const seg2 = segments.find((s) => s.id === seg2Id);

  if (!seg1 || !seg2) {
    return { adjustedPoint: null, satisfied: false, detail: 'intersection: 线段不存在' };
  }

  const a = points.find((p) => p.id === seg1.p1);
  const b = points.find((p) => p.id === seg1.p2);
  const c = points.find((p) => p.id === seg2.p1);
  const d = points.find((p) => p.id === seg2.p2);

  if (!a || !b || !c || !d) {
    return { adjustedPoint: null, satisfied: false, detail: 'intersection: 线段端点不存在' };
  }

  const denom = (b.x - a.x) * (d.y - c.y) - (b.y - a.y) * (d.x - c.x);
  if (Math.abs(denom) < 1e-10) {
    return { adjustedPoint: null, satisfied: false, detail: 'intersection: 线段平行，无交点' };
  }

  const t = ((c.x - a.x) * (d.y - c.y) - (c.y - a.y) * (d.x - c.x)) / denom;
  const ix = a.x + t * (b.x - a.x);
  const iy = a.y + t * (b.y - a.y);

  const existingPt = points.find((p) => p.id === ptId);
  if (existingPt) {
    const error = dist(existingPt, { id: -1, x: ix, y: iy });
    return {
      adjustedPoint: { id: ptId, x: ix, y: iy },
      satisfied: error < 0.5,
      detail: `intersection: 交点 (${ix.toFixed(2)}, ${iy.toFixed(2)}), 误差 ${error.toFixed(4)}`,
    };
  }

  return {
    adjustedPoint: { id: ptId, x: ix, y: iy },
    satisfied: true,
    detail: `intersection: 计算交点 (${ix.toFixed(2)}, ${iy.toFixed(2)})`,
  };
}

/**
 * 执行完整的约束求解流程
 *
 * 遍历所有约束，根据约束类型调用对应的求解函数：
 * - incidence: 点投影到线段
 * - betweenness: 中间点验证/调整
 * - intersection: 两线段交点计算
 * - containment/connection: 暂不进行数值求解，直接标记为满足
 *
 * @param points      - 所有点的数组
 * @param segments    - 所有线段的数组
 * @param constraints - 所有约束的数组
 * @returns 求解结果，包含满足数、冲突数、满足率和详细日志
 */
export function performSolve(
  points: Point[],
  segments: Segment[],
  constraints: Constraint[],
): SolveResult {
  const details: string[] = [];
  let satisfied = 0;
  let conflicts = 0;
  const adjustedPoints = new Map<number, Point>();

  details.push(`开始求解: ${points.length} 点, ${segments.length} 线段, ${constraints.length} 约束`);

  for (const con of constraints) {
    switch (con.type) {
      case 'incidence': {
        if (con.args.length >= 2) {
          const incPtId = con.args[0];
          const incSegId = con.args[1];
          if (incPtId !== undefined && incSegId !== undefined) {
            const result = solveIncidence(incPtId, incSegId, points, segments);
            if (result) {
              adjustedPoints.set(result.adjustedPoint.id, result.adjustedPoint);
              satisfied++;
              details.push(result.detail);
            } else {
              conflicts++;
              details.push(`incidence: 无法满足 (点 ${incPtId} → 线段 ${incSegId})`);
            }
          }
        }
        break;
      }
      case 'betweenness': {
        const result = solveBetweenness(con, points);
        if (result.satisfied) {
          satisfied++;
        } else {
          conflicts++;
          if (result.adjustedPoint) {
            adjustedPoints.set(result.adjustedPoint.id, result.adjustedPoint);
          }
        }
        details.push(result.detail);
        break;
      }
      case 'intersection': {
        const result = solveIntersection(con, points, segments);
        if (result.satisfied) {
          satisfied++;
        } else {
          conflicts++;
          if (result.adjustedPoint) {
            adjustedPoints.set(result.adjustedPoint.id, result.adjustedPoint);
          }
        }
        details.push(result.detail);
        break;
      }
      case 'containment':
      case 'connection':
        // 这些约束类型暂不进行数值求解
        satisfied++;
        details.push(`${con.type}: 跳过（暂不支持数值求解）`);
        break;
    }
  }

  const total = constraints.length;
  const satisfactionRate = total > 0 ? (satisfied / total) * 100 : 100;
  const freeVariables = points.length * 2 - satisfied; // 每个满足的约束大约固定一个自由度

  details.push(`求解完成: ${satisfied} 满足, ${conflicts} 冲突, 满足率 ${satisfactionRate.toFixed(1)}%`);

  return {
    satisfied,
    conflicts,
    total,
    satisfactionRate,
    freeVariables: Math.max(0, freeVariables),
    details,
  };
}

/**
 * 归一化 —— 合并距离过近的点
 *
 * 遍历所有点对，将距离小于阈值的点合并为同一个点（保留先出现的点）。
 * 合并后会同步更新线段和约束中的点引用，并移除自环线段。
 *
 * @param points      - 所有点的数组
 * @param segments    - 所有线段的数组
 * @param constraints - 所有约束的数组
 * @param threshold   - 合并距离阈值，默认 0.5
 * @returns 合并后的点、线段、约束数组，以及合并数量和详细日志
 */
export function normalizePoints(
  points: Point[],
  segments: Segment[],
  constraints: Constraint[],
  threshold: number = 0.5,
): {
  mergedPoints: Point[];
  mergedSegments: Segment[];
  mergedConstraints: Constraint[];
  mergeCount: number;
  details: string[];
} {
  const details: string[] = [];
  const merged = new Map<number, number>(); // oldId -> canonicalId
  const canonicalPoints: Point[] = [];

  // 找出需要合并的点
  for (let i = 0; i < points.length; i++) {
    const pi = points[i]!;
    if (merged.has(pi.id)) continue;

    let canonicalId = pi.id;
    for (let j = i + 1; j < points.length; j++) {
      const pj = points[j]!;
      if (merged.has(pj.id)) continue;
      if (dist(pi, pj) < threshold) {
        merged.set(pj.id, canonicalId);
        details.push(`归一化: 合并点 ${pj.id} → ${canonicalId} (距离 ${dist(pi, pj).toFixed(4)})`);
      }
    }
    canonicalPoints.push(pi);
  }

  // 更新线段引用
  const mergedSegments = segments.map((seg) => ({
    ...seg,
    p1: merged.get(seg.p1) ?? seg.p1,
    p2: merged.get(seg.p2) ?? seg.p2,
  })).filter((seg) => seg.p1 !== seg.p2); // 移除自环

  // 更新约束引用
  const mergedConstraints = constraints.map((con) => ({
    ...con,
    args: con.args.map((arg) => {
      const mapped = merged.get(arg);
      return mapped !== undefined ? mapped : arg;
    }),
  }));

  details.push(`归一化完成: 合并 ${merged.size} 个点, ${points.length - canonicalPoints.length} 个重复`);

  return {
    mergedPoints: canonicalPoints,
    mergedSegments,
    mergedConstraints,
    mergeCount: merged.size,
    details,
  };
}
