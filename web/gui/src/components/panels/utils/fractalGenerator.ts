/**
 * @module components/panels/utils/fractalGenerator
 * @description 分形生成算法工具。
 *              提供 Sierpinski 三角形、Koch 雪花、分形树等经典分形
 *              的生成算法，以及递归终止性验证。
 *
 *              Fractal generation algorithm utility.
 *              Provides generation algorithms for classic fractals
 *              including Sierpinski triangle, Koch snowflake, and fractal tree,
 *              as well as recursion termination validation.
 */

import type { Point, Segment } from '@/types';

// ================================================================
// 类型定义 / Type Definitions
// ================================================================

/** 递归构造规则 */
export interface RecursionRule {
  /** 规则名称 */
  name: string;
  /** 变换类型 */
  transformType: 'sierpinski' | 'koch' | 'fractal_tree' | 'custom_subdivide' | 'custom_replace';
  /** 最大深度限制 */
  maxDepth: number;
  /** 描述 */
  description: string;
  /** 是否为内置规则 */
  builtin?: boolean;
}

/** 终止性验证结果 */
export interface ValidationResult {
  terminates: boolean;
  reason: string;
}

/** 分形生成结果 */
export interface FractalResult {
  points: Point[];
  segments: Segment[];
  totalPoints: number;
  totalSegments: number;
}

// ================================================================
// 内置递归规则 / Built-in Recursion Rules
// ================================================================

/** 内置分形规则列表 */
export const BUILTIN_RULES: RecursionRule[] = [
  {
    name: 'Sierpinski Triangle',
    transformType: 'sierpinski',
    maxDepth: 5,
    description: '谢尔宾斯基三角形 (深度 0-5) / Sierpinski triangle',
    builtin: true,
  },
  {
    name: 'Koch Snowflake',
    transformType: 'koch',
    maxDepth: 4,
    description: 'Koch 雪花 (深度 0-4) / Koch snowflake',
    builtin: true,
  },
  {
    name: 'Fractal Tree',
    transformType: 'fractal_tree',
    maxDepth: 6,
    description: '分形树 (深度 0-6) / Fractal tree',
    builtin: true,
  },
];

// ================================================================
// Sierpinski 三角形 / Sierpinski Triangle
// ================================================================

/**
 * 生成 Sierpinski 三角形。
 * 递归地将三角形分为 4 个子三角形，移除中间的一个。
 *
 * @param p1 - 第一个顶点
 * @param p2 - 第二个顶点
 * @param p3 - 第三个顶点
 * @param depth - 当前递归深度
 * @param maxDepth - 最大递归深度
 * @param idCounter - ID 计数器（可变引用）
 * @returns 分形生成结果
 */
export function generateSierpinski(
  p1: { x: number; y: number },
  p2: { x: number; y: number },
  p3: { x: number; y: number },
  depth: number,
  maxDepth: number,
  idCounter: { value: number },
): FractalResult {
  const result: FractalResult = { points: [], segments: [], totalPoints: 0, totalSegments: 0 };

  if (depth >= maxDepth) {
    // 到达最大深度，绘制当前三角形
    const pid1 = idCounter.value++;
    const pid2 = idCounter.value++;
    const pid3 = idCounter.value++;

    result.points.push(
      { id: pid1, x: p1.x, y: p1.y },
      { id: pid2, x: p2.x, y: p2.y },
      { id: pid3, x: p3.x, y: p3.y },
    );
    result.segments.push(
      { id: idCounter.value++, p1: pid1, p2: pid2 },
      { id: idCounter.value++, p1: pid2, p2: pid3 },
      { id: idCounter.value++, p1: pid3, p2: pid1 },
    );
    result.totalPoints = 3;
    result.totalSegments = 3;
    return result;
  }

  // 计算三边中点
  const mid12 = { x: (p1.x + p2.x) / 2, y: (p1.y + p2.y) / 2 };
  const mid23 = { x: (p2.x + p3.x) / 2, y: (p2.y + p3.y) / 2 };
  const mid31 = { x: (p3.x + p1.x) / 2, y: (p3.y + p1.y) / 2 };

  // 递归生成三个角落的子三角形（跳过中间的）
  const r1 = generateSierpinski(p1, mid12, mid31, depth + 1, maxDepth, idCounter);
  const r2 = generateSierpinski(mid12, p2, mid23, depth + 1, maxDepth, idCounter);
  const r3 = generateSierpinski(mid31, mid23, p3, depth + 1, maxDepth, idCounter);

  result.points.push(...r1.points, ...r2.points, ...r3.points);
  result.segments.push(...r1.segments, ...r2.segments, ...r3.segments);
  result.totalPoints = r1.totalPoints + r2.totalPoints + r3.totalPoints;
  result.totalSegments = r1.totalSegments + r2.totalSegments + r3.totalSegments;
  return result;
}

// ================================================================
// Koch 雪花 / Koch Snowflake
// ================================================================

/**
 * 生成 Koch 雪花。
 * 将每条线段分为三等分，在中间三分之一处向外凸出形成等边三角形。
 *
 * @param depth - 当前递归深度
 * @param _maxDepth - 最大递归深度（未使用，由 depth 控制）
 * @param idCounter - ID 计数器（可变引用）
 * @returns 分形生成结果
 */
export function generateKochSnowflake(depth: number, _maxDepth: number, idCounter: { value: number }): FractalResult {
  // 初始等边三角形顶点（以原点为中心）
  const size = 200;
  const h = (size * Math.sqrt(3)) / 2;
  const cy = h / 3;

  const p1 = { x: 0, y: -2 * cy };
  const p2 = { x: -size / 2, y: cy };
  const p3 = { x: size / 2, y: cy };

  // 初始三条边（逆时针方向）
  let currentEdges: Array<{ x1: number; y1: number; x2: number; y2: number }> = [
    { x1: p1.x, y1: p1.y, x2: p3.x, y2: p3.y },
    { x1: p3.x, y1: p3.y, x2: p2.x, y2: p2.y },
    { x1: p2.x, y1: p2.y, x2: p1.x, y2: p1.y },
  ];

  // 递归细分每条边
  for (let d = 0; d < depth; d++) {
    const nextEdges: Array<{ x1: number; y1: number; x2: number; y2: number }> = [];
    for (const edge of currentEdges) {
      const dx = edge.x2 - edge.x1;
      const dy = edge.y2 - edge.y1;

      // 三等分点
      const a = { x: edge.x1 + dx / 3, y: edge.y1 + dy / 3 };
      const b = { x: edge.x1 + 2 * dx / 3, y: edge.y1 + 2 * dy / 3 };

      // 向外凸出的顶点（旋转 -60 度）
      const cos60 = Math.cos(-Math.PI / 3);
      const sin60 = Math.sin(-Math.PI / 3);
      const peakX = a.x + (b.x - a.x) * cos60 - (b.y - a.y) * sin60;
      const peakY = a.y + (b.x - a.x) * sin60 + (b.y - a.y) * cos60;
      const peak = { x: peakX, y: peakY };

      // 四条新边
      nextEdges.push(
        { x1: edge.x1, y1: edge.y1, x2: a.x, y2: a.y },
        { x1: a.x, y1: a.y, x2: peak.x, y2: peak.y },
        { x1: peak.x, y1: peak.y, x2: b.x, y2: b.y },
        { x1: b.x, y1: b.y, x2: edge.x2, y2: edge.y2 },
      );
    }
    currentEdges = nextEdges;
  }

  // 将边转换为点和线段
  const pointMap = new Map<string, number>();
  const result: FractalResult = { points: [], segments: [], totalPoints: 0, totalSegments: 0 };

  const getOrCreatePoint = (x: number, y: number): number => {
    // 使用四舍五入避免浮点精度问题
    const key = `${Math.round(x * 100) / 100},${Math.round(y * 100) / 100}`;
    if (pointMap.has(key)) return pointMap.get(key)!;
    const id = idCounter.value++;
    pointMap.set(key, id);
    result.points.push({ id, x, y });
    return id;
  };

  for (const edge of currentEdges) {
    const pid1 = getOrCreatePoint(edge.x1, edge.y1);
    const pid2 = getOrCreatePoint(edge.x2, edge.y2);
    if (pid1 !== pid2) {
      result.segments.push({ id: idCounter.value++, p1: pid1, p2: pid2 });
    }
  }

  result.totalPoints = result.points.length;
  result.totalSegments = result.segments.length;
  return result;
}

// ================================================================
// 分形树 / Fractal Tree
// ================================================================

/**
 * 生成分形树。
 * 从主干开始，每个分支末端分裂为两个子分支，角度和长度递减。
 *
 * @param startX - 起点 X 坐标
 * @param startY - 起点 Y 坐标
 * @param length - 当前分支长度
 * @param angle - 当前分支角度（弧度）
 * @param depth - 当前递归深度
 * @param maxDepth - 最大递归深度
 * @param idCounter - ID 计数器（可变引用）
 * @returns 分形生成结果
 */
export function generateFractalTree(
  startX: number,
  startY: number,
  length: number,
  angle: number,
  depth: number,
  maxDepth: number,
  idCounter: { value: number },
): FractalResult {
  const result: FractalResult = { points: [], segments: [], totalPoints: 0, totalSegments: 0 };

  if (depth >= maxDepth || length < 2) {
    return result;
  }

  // 计算分支终点
  const endX = startX + length * Math.cos(angle);
  const endY = startY + length * Math.sin(angle);

  const pid1 = idCounter.value++;
  const pid2 = idCounter.value++;
  result.points.push(
    { id: pid1, x: startX, y: startY },
    { id: pid2, x: endX, y: endY },
  );
  result.segments.push({ id: idCounter.value++, p1: pid1, p2: pid2 });

  // 分支参数
  const branchAngle = Math.PI / 6; // 30 度
  const lengthRatio = 0.67;

  // 递归生成左右子分支
  const leftBranch = generateFractalTree(
    endX, endY,
    length * lengthRatio,
    angle - branchAngle,
    depth + 1, maxDepth, idCounter,
  );
  const rightBranch = generateFractalTree(
    endX, endY,
    length * lengthRatio,
    angle + branchAngle,
    depth + 1, maxDepth, idCounter,
  );

  result.points.push(...leftBranch.points, ...rightBranch.points);
  result.segments.push(...leftBranch.segments, ...rightBranch.segments);
  result.totalPoints = result.points.length;
  result.totalSegments = result.segments.length;
  return result;
}

// ================================================================
// 统一分形入口 / Unified Fractal Entry Point
// ================================================================

/**
 * 根据规则类型和深度生成分形几何。
 *
 * @param rule - 递归规则
 * @param targetDepth - 目标深度
 * @param idCounter - ID 计数器（可变引用）
 * @returns 分形生成结果
 */
export function generateFractal(
  rule: RecursionRule,
  targetDepth: number,
  idCounter: { value: number },
): FractalResult {
  switch (rule.transformType) {
    case 'sierpinski': {
      const size = 200;
      const h = (size * Math.sqrt(3)) / 2;
      const cy = h / 3;
      return generateSierpinski(
        { x: 0, y: -2 * cy },
        { x: -size / 2, y: cy },
        { x: size / 2, y: cy },
        0, targetDepth, idCounter,
      );
    }
    case 'koch':
      return generateKochSnowflake(targetDepth, rule.maxDepth, idCounter);
    case 'fractal_tree':
      return generateFractalTree(0, 100, 80, -Math.PI / 2, 0, targetDepth, idCounter);
    default:
      return { points: [], segments: [], totalPoints: 0, totalSegments: 0 };
  }
}

// ================================================================
// 终止性验证 / Termination Validation
// ================================================================

/**
 * 验证递归是否保证终止。
 * 检查深度限制是否有效，以及变换类型是否保证每步减少规模。
 *
 * @param rule - 递归规则
 * @returns 验证结果
 */
export function validateTermination(rule: RecursionRule): ValidationResult {
  // 检查深度限制是否有限
  if (rule.maxDepth <= 0 || !isFinite(rule.maxDepth)) {
    return {
      terminates: false,
      reason: `深度限制无效 (${rule.maxDepth}) / Invalid depth limit`,
    };
  }

  if (rule.maxDepth > 10) {
    return {
      terminates: false,
      reason: `深度限制过大 (${rule.maxDepth})，可能导致性能问题 / Depth limit too large`,
    };
  }

  // 检查变换类型是否保证每步减少规模
  const reducingTransforms = ['sierpinski', 'koch', 'fractal_tree', 'custom_subdivide'];
  if (reducingTransforms.includes(rule.transformType)) {
    return {
      terminates: true,
      reason: `终止性保证: 深度限制 ${rule.maxDepth}，每步规模递减 / Termination guaranteed: depth ${rule.maxDepth}, reducing measure`,
    };
  }

  return {
    terminates: true,
    reason: `深度限制为 ${rule.maxDepth}，有限步后终止 / Depth limit ${rule.maxDepth}, terminates in finite steps`,
  };
}
