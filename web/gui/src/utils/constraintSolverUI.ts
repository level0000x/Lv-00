/**
 * @module utils/constraintSolverUI
 * @description 约束求解器 UI 工具模块。
 *
 *              从 FormulaPanel 中提取的 Gauss-Seidel 迭代松弛求解器，
 *              用于在 UI 面板中执行几何约束求解。
 *
 *              功能：
 *              1. parseForSolver —— 从 DSL 文本解析点定义和约束定义
 *              2. gaussSeidelSolve —— Gauss-Seidel 迭代松弛求解
 *              3. SolveResult —— 求解结果类型定义
 */

// ================================================================
// 类型定义 / Type Definitions
// ================================================================

/** 求解结果类型 */
export interface SolveResult {
  /** 是否收敛 */
  solved: boolean;
  /** 求解后的点坐标列表 */
  points: Array<{ label: string; x: number; y: number }>;
  /** 约束满足状态列表 */
  constraints: Array<{ desc: string; satisfied: boolean; error?: number }>;
  /** 实际迭代次数 */
  iterations: number;
  /** 错误信息（收敛时为 null） */
  error: string | null;
}

/** 点定义类型 */
export interface PointDef {
  /** x 坐标 */
  x: number;
  /** y 坐标 */
  y: number;
  /** 是否为固定点（不参与松弛） */
  fixed: boolean;
}

/** 约束定义类型 */
export interface ConstraintDef {
  /** 约束类型（distance/midpoint/horizontal/vertical/angle） */
  type: string;
  /** 约束参数（点标签列表） */
  args: string[];
  /** 约束目标值（可选） */
  value?: number;
}

/** parseForSolver 返回类型 */
export interface ParseForSolverResult {
  /** 解析出的点定义映射 */
  pointDefs: Map<string, PointDef>;
  /** 解析出的约束定义列表 */
  constraintDefs: ConstraintDef[];
  /** 解析错误列表 */
  parseErrors: string[];
}

// ================================================================
// parseForSolver —— 从 DSL 文本解析点定义和约束定义
// ================================================================

/**
 * 从公式 DSL 文本中解析点定义和约束定义。
 *
 * 支持的语法：
 * - point A(x, y)          — 点定义
 * - distance(A,B) = 5      — 距离约束
 * - midpoint M of A, B     — 中点约束
 * - horizontal A, B        — 水平约束
 * - vertical A, C          — 垂直约束
 * - angle(A,B,C) = 90      — 角度约束
 *
 * @param text - DSL 公式文本
 * @returns 解析结果，包含点定义、约束定义和解析错误
 */
export function parseForSolver(text: string): ParseForSolverResult {
  const pointDefs = new Map<string, PointDef>();
  const constraintDefs: ConstraintDef[] = [];
  const parseErrors: string[] = [];

  const lines = text.split('\n');
  for (const rawLine of lines) {
    const line = rawLine.trim();
    // 跳过空行和注释
    if (!line || line.startsWith('//') || line.startsWith('#')) continue;

    // 匹配点定义: point A(x, y)
    const pointMatch = line.match(/^point\s+(\w+)\s*\(\s*([^,]+)\s*,\s*([^)]+)\s*\)/i);
    if (pointMatch) {
      const label = pointMatch[1]!;
      const xStr = pointMatch[2]!.trim();
      const yStr = pointMatch[3]!.trim();
      const xVal = Number(xStr);
      const yVal = Number(yStr);
      if (!isNaN(xVal) && !isNaN(yVal)) {
        pointDefs.set(label, { x: xVal, y: yVal, fixed: true });
      } else {
        pointDefs.set(label, { x: 0, y: 0, fixed: false });
      }
      continue;
    }

    // 匹配距离约束: distance(A,B) = 5
    const distMatch = line.match(/distance\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)\s*=\s*([\d.]+)/i);
    if (distMatch) {
      constraintDefs.push({ type: 'distance', args: [distMatch[1]!, distMatch[2]!], value: Number(distMatch[3]!) });
      continue;
    }

    // 匹配中点约束: midpoint M of A, B
    const midMatch = line.match(/^midpoint\s+(\w+)\s+of\s+(\w+)\s*,\s*(\w+)/i);
    if (midMatch) {
      const mLabel = midMatch[1]!;
      if (!pointDefs.has(mLabel)) {
        pointDefs.set(mLabel, { x: 0, y: 0, fixed: false });
      }
      constraintDefs.push({ type: 'midpoint', args: [mLabel, midMatch[2]!, midMatch[3]!] });
      continue;
    }

    // 匹配水平约束: horizontal A, B
    const horizMatch = line.match(/^horizontal\s+(\w+)\s*,\s*(\w+)/i);
    if (horizMatch) {
      constraintDefs.push({ type: 'horizontal', args: [horizMatch[1]!, horizMatch[2]!] });
      continue;
    }

    // 匹配垂直约束: vertical A, C
    const vertMatch = line.match(/^vertical\s+(\w+)\s*,\s*(\w+)/i);
    if (vertMatch) {
      constraintDefs.push({ type: 'vertical', args: [vertMatch[1]!, vertMatch[2]!] });
      continue;
    }

    // 匹配角度约束: angle(A,B,C) = 90
    const angleMatch = line.match(/angle\s*\(\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*\)\s*=\s*([\d.]+)/i);
    if (angleMatch) {
      constraintDefs.push({ type: 'angle', args: [angleMatch[1]!, angleMatch[2]!, angleMatch[3]!], value: Number(angleMatch[4]!) });
      continue;
    }
  }

  return { pointDefs, constraintDefs, parseErrors };
}

// ================================================================
// gaussSeidelSolve —— Gauss-Seidel 迭代松弛求解器
// ================================================================

/**
 * Gauss-Seidel 迭代松弛求解器。
 *
 * 算法原理：
 * 对每个约束计算残差，然后按 Gauss-Seidel 方式逐个松弛约束，
 * 每次迭代中直接更新点的坐标（而非同时更新所有点）。
 * 使用阻尼因子控制收敛速度，避免震荡。
 *
 * @param pointDefs - 点定义映射（会被原地修改）
 * @param constraintDefs - 约束定义列表
 * @param maxIterations - 最大迭代次数（默认 500）
 * @param tolerance - 收敛容差（默认 1e-6）
 * @param damping - 阻尼因子（默认 0.5，范围 0~1）
 * @returns 求解结果
 */
export function gaussSeidelSolve(
  pointDefs: Map<string, PointDef>,
  constraintDefs: ConstraintDef[],
  maxIterations: number = 500,
  tolerance: number = 1e-6,
  damping: number = 0.5,
): SolveResult {
  // 前置检查：确保所有约束引用的点都已定义
  for (const c of constraintDefs) {
    for (const arg of c.args) {
      if (!pointDefs.has(arg)) {
        return { solved: false, points: [], constraints: [], iterations: 0, error: `未定义的点: ${arg} / Undefined point: ${arg}` };
      }
    }
  }

  /** 计算两点之间的欧几里得距离 */
  const dist = (a: string, b: string): number => {
    const pa = pointDefs.get(a)!;
    const pb = pointDefs.get(b)!;
    return Math.sqrt((pa.x - pb.x) ** 2 + (pa.y - pb.y) ** 2);
  };

  /** 计算单个约束的残差 */
  const computeResidual = (c: ConstraintDef): number => {
    switch (c.type) {
      case 'distance': return dist(c.args[0]!, c.args[1]!) - (c.value ?? 0);
      case 'midpoint': {
        const m = pointDefs.get(c.args[0]!)!;
        const a = pointDefs.get(c.args[1]!)!;
        const b = pointDefs.get(c.args[2]!)!;
        return Math.sqrt((m.x - (a.x + b.x) / 2) ** 2 + (m.y - (a.y + b.y) / 2) ** 2);
      }
      case 'horizontal': return pointDefs.get(c.args[0]!)!.y - pointDefs.get(c.args[1]!)!.y;
      case 'vertical': return pointDefs.get(c.args[0]!)!.x - pointDefs.get(c.args[1]!)!.x;
      case 'angle': {
        const a = pointDefs.get(c.args[0]!)!;
        const b = pointDefs.get(c.args[1]!)!;
        const cv = pointDefs.get(c.args[2]!)!;
        const v1x = a.x - b.x, v1y = a.y - b.y;
        const v2x = cv.x - b.x, v2y = cv.y - b.y;
        const dot = v1x * v2x + v1y * v2y;
        const cross = v1x * v2y - v1y * v2x;
        return Math.atan2(Math.abs(cross), dot) * 180 / Math.PI - (c.value ?? 0);
      }
      default: return 0;
    }
  };

  /** 松弛单个约束：根据残差调整点的坐标 */
  const relaxConstraint = (c: ConstraintDef): void => {
    const eps = 1e-8;
    switch (c.type) {
      case 'distance': {
        const pa = pointDefs.get(c.args[0]!)!;
        const pb = pointDefs.get(c.args[1]!)!;
        const d = dist(c.args[0]!, c.args[1]!);
        if (d < eps) break;
        const target = c.value ?? 0;
        const ratio = (d - target) / d * damping;
        const dx = (pa.x - pb.x) * ratio;
        const dy = (pa.y - pb.y) * ratio;
        if (!pa.fixed) { pa.x -= dx / 2; pa.y -= dy / 2; }
        if (!pb.fixed) { pb.x += dx / 2; pb.y += dy / 2; }
        break;
      }
      case 'midpoint': {
        const m = pointDefs.get(c.args[0]!)!;
        const a = pointDefs.get(c.args[1]!)!;
        const b = pointDefs.get(c.args[2]!)!;
        if (!m.fixed) { m.x += ((a.x + b.x) / 2 - m.x) * damping; m.y += ((a.y + b.y) / 2 - m.y) * damping; }
        break;
      }
      case 'horizontal': {
        const a = pointDefs.get(c.args[0]!)!;
        const b = pointDefs.get(c.args[1]!)!;
        const avgY = (a.y + b.y) / 2;
        if (!a.fixed) a.y += (avgY - a.y) * damping;
        if (!b.fixed) b.y += (avgY - b.y) * damping;
        break;
      }
      case 'vertical': {
        const a = pointDefs.get(c.args[0]!)!;
        const b = pointDefs.get(c.args[1]!)!;
        const avgX = (a.x + b.x) / 2;
        if (!a.fixed) a.x += (avgX - a.x) * damping;
        if (!b.fixed) b.x += (avgX - b.x) * damping;
        break;
      }
      case 'angle': {
        const a = pointDefs.get(c.args[0]!)!;
        const b = pointDefs.get(c.args[1]!)!;
        const cv = pointDefs.get(c.args[2]!)!;
        const targetAngle = (c.value ?? 0) * Math.PI / 180;
        const v1x = a.x - b.x, v1y = a.y - b.y;
        const v2x = cv.x - b.x, v2y = cv.y - b.y;
        const len1 = Math.sqrt(v1x * v1x + v1y * v1y);
        const len2 = Math.sqrt(v2x * v2x + v2y * v2y);
        if (len1 < eps || len2 < eps) break;
        const currentAngle = Math.atan2(v1x * v2y - v1y * v2x, v1x * v2x + v1y * v2y);
        const angleDiff = targetAngle - currentAngle;
        const rotAngle = angleDiff * damping * 0.3;
        const cosR = Math.cos(rotAngle), sinR = Math.sin(rotAngle);
        if (!a.fixed) {
          const rx = b.x + v1x * cosR - v1y * sinR;
          const ry = b.y + v1x * sinR + v1y * cosR;
          a.x += (rx - a.x) * damping; a.y += (ry - a.y) * damping;
        }
        if (!cv.fixed) {
          const rx = b.x + v2x * cosR - v2y * sinR;
          const ry = b.y + v2x * sinR + v2y * cosR;
          cv.x += (rx - cv.x) * damping; cv.y += (ry - cv.y) * damping;
        }
        break;
      }
    }
  };

  // 主迭代循环
  let converged = false;
  let iterations = 0;
  for (let iter = 0; iter < maxIterations; iter++) {
    iterations = iter + 1;
    let totalResidual = 0;
    for (const c of constraintDefs) {
      const residual = computeResidual(c);
      totalResidual += residual * residual;
      relaxConstraint(c);
    }
    if (Math.sqrt(totalResidual) < tolerance) { converged = true; break; }
  }

  // 收集求解后的点坐标
  const solvedPoints: Array<{ label: string; x: number; y: number }> = [];
  for (const [label, pt] of pointDefs) {
    solvedPoints.push({ label, x: pt.x, y: pt.y });
  }

  // 计算每个约束的满足状态
  const constraintResults = constraintDefs.map((c) => {
    const residual = Math.abs(computeResidual(c));
    let desc = '';
    switch (c.type) {
      case 'distance': desc = `distance(${c.args.join(', ')}) = ${c.value}`; break;
      case 'midpoint': desc = `midpoint ${c.args[0]} of ${c.args[1]}, ${c.args[2]}`; break;
      case 'horizontal': desc = `horizontal ${c.args.join(', ')}`; break;
      case 'vertical': desc = `vertical ${c.args.join(', ')}`; break;
      case 'angle': desc = `angle(${c.args.join(', ')}) = ${c.value}`; break;
    }
    return { desc, satisfied: residual < 0.01, error: residual };
  });

  // 判断未收敛的原因
  const freePointCount = Array.from(pointDefs.values()).filter((p) => !p.fixed).length;
  return {
    solved: converged,
    points: solvedPoints,
    constraints: constraintResults,
    iterations,
    error: converged ? null : (freePointCount === 0
      ? '约束系统过约束或矛盾 / Over-constrained or contradictory system'
      : `未在 ${maxIterations} 次迭代内收敛 / Did not converge in ${maxIterations} iterations`),
  };
}
