/**
 * @module components/panels/utils/narrativeGenerator
 * @description 叙述生成工具。
 *              Penrose 风格的自动化几何叙述生成器，包括模式检测、
 *              叙述步骤生成和统计计算。
 *
 *              Narrative generator utility.
 *              Penrose-style automated geometric narrative generator,
 *              including pattern detection, narrative step generation,
 *              and statistics computation.
 */

import type { Point, Segment, Constraint } from '@/types';

// ================================================================
// 类型定义 / Type Definitions
// ================================================================

/** 检测到的几何模式类型 */
export type PatternType =
  | 'triangle'
  | 'equilateral_triangle'
  | 'square'
  | 'rectangle'
  | 'circle'
  | 'midpoint'
  | 'angle_bisector'
  | 'perpendicular_bisector'
  | 'intersection'
  | 'free_construction';

/** 模式检测结果 */
export interface PatternInfo {
  type: PatternType;
  label: string;
  labelZh: string;
  detail: string;
  detailZh: string;
}

/** 叙述步骤 */
export interface NarrativeStep {
  step: number;
  text: string;
  textZh: string;
}

/** 完整叙述 */
export interface Narrative {
  pattern: PatternInfo;
  summary: string;
  summaryZh: string;
  steps: NarrativeStep[];
  stats: {
    pointCount: number;
    segmentCount: number;
    constraintCount: number;
    area?: number;
    perimeter?: number;
  };
}

/** 叙述生成设置 */
export type NarrativeStyle = 'detailed' | 'concise' | 'educational';
export type NarrativeLanguage = 'zh' | 'en';

export interface NarrativeSettings {
  style: NarrativeStyle;
  language: NarrativeLanguage;
  showConstraints: boolean;
  showMeasurements: boolean;
}

/** 模式图标映射 */
export const PATTERN_ICONS: Record<PatternType, string> = {
  triangle: '\u25B3',
  equilateral_triangle: '\u25B3',
  square: '\u25A1',
  rectangle: '\u25AD',
  circle: '\u25CB',
  midpoint: '\u2022',
  angle_bisector: '\u2220',
  perpendicular_bisector: '\u22A5',
  intersection: '\u2715',
  free_construction: '\u221E',
};

// ================================================================
// 工具函数 / Utility Functions
// ================================================================

/** 将节点 ID 映射为字母标签（A, B, C, ...） */
export function idToLabel(id: number, sortedIds: number[]): string {
  const idx = sortedIds.indexOf(id);
  if (idx < 0) return `N${id}`;
  if (idx < 26) return String.fromCharCode(65 + idx);
  return `P${idx - 25}`;
}

/** 计算两点之间的距离 */
export function distance(a: Point, b: Point): number {
  const dx = a.x - b.x;
  const dy = a.y - b.y;
  return Math.sqrt(dx * dx + dy * dy);
}

/** 检查三点是否构成等边三角形（在容差范围内） */
export function isEquilateral(a: Point, b: Point, c: Point, tolerance = 0.05): boolean {
  const ab = distance(a, b);
  const bc = distance(b, c);
  const ca = distance(c, a);
  const avg = (ab + bc + ca) / 3;
  if (avg < 0.001) return false;
  return Math.abs(ab - avg) / avg < tolerance &&
         Math.abs(bc - avg) / avg < tolerance &&
         Math.abs(ca - avg) / avg < tolerance;
}

/** 检查四点是否构成正方形（在容差范围内） */
export function isSquare(a: Point, b: Point, c: Point, d: Point, tolerance = 0.05): boolean {
  if (typeof a?.x !== 'number' || typeof a?.y !== 'number') return false;
  const pts = [a, b, c, d];
  const dists: number[] = [];
  for (let i = 0; i < 4; i++) {
    dists.push(distance(pts[i]!, pts[(i + 1) % 4]!));
  }
  const avgDist = dists.reduce((s, v) => s + v, 0) / 4;
  if (avgDist < 0.001) return false;
  return dists.every((d) => Math.abs(d - avgDist) / avgDist < tolerance);
}

/** 根据线段构建邻接表 */
export function buildAdjacency(points: Point[], segments: Segment[]): Map<number, Set<number>> {
  const adj = new Map<number, Set<number>>();
  for (const p of points) adj.set(p.id, new Set());
  for (const seg of segments) {
    adj.get(seg.p1)?.add(seg.p2);
    adj.get(seg.p2)?.add(seg.p1);
  }
  return adj;
}

/** 在点图中查找指定长度的环 */
export function findCycle(adj: Map<number, Set<number>>, startId: number, targetLen: number): number[] | null {
  const visited = new Map<number, number>();

  function dfs(current: number, depth: number): number[] | null {
    if (depth === targetLen) {
      if (adj.get(current)?.has(startId)) {
        const path: number[] = [startId];
        let node = current;
        while (node !== startId) {
          path.push(node);
          node = visited.get(node)!;
        }
        return path;
      }
      return null;
    }
    for (const neighbor of adj.get(current) ?? []) {
      if (!visited.has(neighbor) && neighbor !== startId) {
        visited.set(neighbor, current);
        const result = dfs(neighbor, depth + 1);
        if (result) return result;
        visited.delete(neighbor);
      }
    }
    return null;
  }

  visited.set(startId, startId);
  for (const neighbor of adj.get(startId) ?? []) {
    visited.set(neighbor, startId);
    const result = dfs(neighbor, 2);
    if (result) return result;
    visited.delete(neighbor);
  }
  return null;
}

// ================================================================
// 模式检测 / Pattern Detection
// ================================================================

/**
 * 检测当前几何构造中的模式。
 * 依次检查：空白画布、单点、中点、相交、三角形、四边形等。
 *
 * @param points - 当前点集合
 * @param segments - 当前线段集合
 * @param constraints - 当前约束集合
 * @returns 检测到的模式信息
 */
export function detectPattern(
  points: Point[],
  segments: Segment[],
  constraints: Constraint[],
): PatternInfo {
  const n = points.length;
  const m = segments.length;

  if (n === 0) {
    return {
      type: 'free_construction',
      label: 'Empty Canvas',
      labelZh: '空白画布',
      detail: 'No points have been placed yet.',
      detailZh: '尚未放置任何点。',
    };
  }

  if (n === 1) {
    const p0 = points[0]!;
    return {
      type: 'free_construction',
      label: 'Single Point',
      labelZh: '单点',
      detail: `One free point has been placed at (${p0.x.toFixed(2)}, ${p0.y.toFixed(2)}).`,
      detailZh: `放置了一个自由点，坐标为 (${p0.x.toFixed(2)}, ${p0.y.toFixed(2)})。`,
    };
  }

  const sortedIds = [...points].sort((a, b) => a.id - b.id).map((p) => p.id);
  const pointMap = new Map<number, Point>();
  for (const p of points) pointMap.set(p.id, p);

  // 检查中点约束
  const betweennessConstraints = constraints.filter((c) => c.type === 'betweenness' && c.args.length >= 3);
  if (betweennessConstraints.length > 0) {
    const bc = betweennessConstraints[0]!;
    const bcArgs = bc.args;
    const labelA = idToLabel(bcArgs[0]!, sortedIds);
    const labelB = idToLabel(bcArgs[1]!, sortedIds);
    const labelC = idToLabel(bcArgs[2]!, sortedIds);
    return {
      type: 'midpoint',
      label: 'Midpoint Construction',
      labelZh: '中点构造',
      detail: `Point ${labelB} is between ${labelA} and ${labelC}, suggesting a midpoint or betweenness construction.`,
      detailZh: `点${labelB}在点${labelA}和点${labelC}之间，疑似中点或介于构造。`,
    };
  }

  // 检查相交约束
  const intersectionConstraints = constraints.filter((c) => c.type === 'intersection');
  if (intersectionConstraints.length > 0) {
    return {
      type: 'intersection',
      label: 'Segment Intersection',
      labelZh: '线段相交',
      detail: `Detected ${intersectionConstraints.length} intersection constraint(s).`,
      detailZh: `检测到 ${intersectionConstraints.length} 个相交约束。`,
    };
  }

  const adj = buildAdjacency(points, segments);

  // 检查三角形（3-环）
  if (n >= 3 && m >= 3) {
    for (const p of points) {
      const cycle = findCycle(adj, p.id, 3);
      if (cycle && cycle.length === 3) {
        const [a, b, c] = cycle.map((id) => pointMap.get(id)!);
        const eq = isEquilateral(a!, b!, c!);
        const labelA = idToLabel(cycle[0]!, sortedIds);
        const labelB = idToLabel(cycle[1]!, sortedIds);
        const labelC = idToLabel(cycle[2]!, sortedIds);
        if (eq) {
          return {
            type: 'equilateral_triangle',
            label: 'Equilateral Triangle',
            labelZh: '等边三角形',
            detail: `Points ${labelA}, ${labelB}, ${labelC} form an equilateral triangle.`,
            detailZh: `点${labelA}、${labelB}、${labelC}构成等边三角形。`,
          };
        }
        return {
          type: 'triangle',
          label: 'Triangle',
          labelZh: '三角形',
          detail: `Points ${labelA}, ${labelB}, ${labelC} form a triangle.`,
          detailZh: `点${labelA}、${labelB}、${labelC}构成三角形。`,
        };
      }
    }
  }

  // 检查四边形（4-环）
  if (n >= 4 && m >= 4) {
    for (const p of points) {
      const cycle = findCycle(adj, p.id, 4);
      if (cycle && cycle.length === 4) {
        const pts = cycle.map((id) => pointMap.get(id)!);
        const sq = isSquare(pts[0]!, pts[1]!, pts[2]!, pts[3]!);
        const labelA = idToLabel(cycle[0]!, sortedIds);
        const labelB = idToLabel(cycle[1]!, sortedIds);
        const labelC = idToLabel(cycle[2]!, sortedIds);
        const labelD = idToLabel(cycle[3]!, sortedIds);
        if (sq) {
          return {
            type: 'square',
            label: 'Square',
            labelZh: '正方形',
            detail: `Points ${labelA}, ${labelB}, ${labelC}, ${labelD} form a square.`,
            detailZh: `点${labelA}、${labelB}、${labelC}、${labelD}构成正方形。`,
          };
        }
        return {
          type: 'rectangle',
          label: 'Quadrilateral',
          labelZh: '四边形',
          detail: `Points ${labelA}, ${labelB}, ${labelC}, ${labelD} form a quadrilateral.`,
          detailZh: `点${labelA}、${labelB}、${labelC}、${labelD}构成四边形。`,
        };
      }
    }
  }

  return {
    type: 'free_construction',
    label: 'Free Construction',
    labelZh: '自由构造',
    detail: `A construction with ${n} point(s) and ${m} segment(s). Adding more structure may reveal geometric patterns.`,
    detailZh: `含 ${n} 个点和 ${m} 条线段的自由构造。添加更多结构或许能揭示几何模式。`,
  };
}

// ================================================================
// 叙述生成 / Narrative Generation
// ================================================================

/**
 * 根据当前几何构造和检测到的模式生成叙述。
 * 包含点介绍、线段连接、约束施加、模式结论和教育性注释。
 *
 * @param points - 当前点集合
 * @param segments - 当前线段集合
 * @param constraints - 当前约束集合
 * @param pattern - 检测到的模式信息
 * @param settings - 叙述生成设置
 * @returns 完整的叙述对象
 */
export function generateNarrative(
  points: Point[],
  segments: Segment[],
  constraints: Constraint[],
  pattern: PatternInfo,
  settings: NarrativeSettings,
): Narrative {
  const sortedIds = [...points].sort((a, b) => a.id - b.id).map((p) => p.id);
  const pointMap = new Map<number, Point>();
  for (const p of points) pointMap.set(p.id, p);

  const steps: NarrativeStep[] = [];
  let stepNum = 0;

  // 步骤1：介绍点
  stepNum++;
  if (points.length === 0) {
    steps.push({
      step: stepNum,
      text: 'The canvas is empty. Start by placing points on the coordinate plane.',
      textZh: '画布为空。请先在坐标平面上放置点。',
    });
  } else {
    const pointDescs: string[] = [];
    const pointDescsZh: string[] = [];
    for (const p of points) {
      const label = idToLabel(p.id, sortedIds);
      pointDescs.push(`point ${label} at (${p.x.toFixed(2)}, ${p.y.toFixed(2)})`);
      pointDescsZh.push(`点${label}，坐标(${p.x.toFixed(2)}, ${p.y.toFixed(2)})`);
    }
    steps.push({
      step: stepNum,
      text: `We begin by constructing ${points.length} point(s): ${pointDescs.join('; ')}.`,
      textZh: `我们首先构造 ${points.length} 个点：${pointDescsZh.join('；')}。`,
    });
  }

  // 步骤2：线段
  if (segments.length > 0) {
    stepNum++;
    const segDescs: string[] = [];
    const segDescsZh: string[] = [];
    for (const seg of segments) {
      const labelP1 = idToLabel(seg.p1, sortedIds);
      const labelP2 = idToLabel(seg.p2, sortedIds);
      segDescs.push(`${labelP1}${labelP2}`);
      const p1 = pointMap.get(seg.p1);
      const p2 = pointMap.get(seg.p2);
      if (p1 && p2) {
        const d = distance(p1, p2);
        segDescsZh.push(`${labelP1}${labelP2}（长度 ${d.toFixed(2)}）`);
      } else {
        segDescsZh.push(`${labelP1}${labelP2}`);
      }
    }
    steps.push({
      step: stepNum,
      text: `We connect the points with ${segments.length} segment(s): ${segDescs.join(', ')}.`,
      textZh: `我们连接各点，构造 ${segments.length} 条线段：${segDescsZh.join('，')}。`,
    });
  }

  // 步骤3：约束
  if (settings.showConstraints && constraints.length > 0) {
    stepNum++;
    const constrDescs: string[] = [];
    const constrDescsZh: string[] = [];
    for (const c of constraints) {
      const typeName = c.type;
      const argLabels = c.args.map((aid) => idToLabel(aid, sortedIds));
      if (c.type === 'betweenness' && argLabels.length >= 3) {
        constrDescs.push(`${argLabels[1]} is between ${argLabels[0]} and ${argLabels[2]}`);
        constrDescsZh.push(`${argLabels[1]} 介于 ${argLabels[0]} 和 ${argLabels[2]} 之间`);
      } else if (c.type === 'incidence') {
        constrDescs.push(`${argLabels.join(', ')} are incident`);
        constrDescsZh.push(`${argLabels.join('、')} 满足关联关系`);
      } else if (c.type === 'intersection') {
        constrDescs.push(`${argLabels.join(' and ')} intersect`);
        constrDescsZh.push(`${argLabels.join('与')} 相交`);
      } else {
        constrDescs.push(`${typeName}(${argLabels.join(', ')})`);
        constrDescsZh.push(`${typeName}(${argLabels.join('，')})`);
      }
    }
    steps.push({
      step: stepNum,
      text: `We impose ${constraints.length} constraint(s): ${constrDescs.join('; ')}.`,
      textZh: `我们添加 ${constraints.length} 个约束：${constrDescsZh.join('；')}。`,
    });
  }

  // 步骤4：模式结论
  stepNum++;
  steps.push({
    step: stepNum,
    text: `Pattern detected: ${pattern.label}. ${pattern.detail}`,
    textZh: `检测到的模式：${pattern.labelZh}。${pattern.detailZh}`,
  });

  // 教育性额外步骤
  if (settings.style === 'educational' && pattern.type !== 'free_construction') {
    stepNum++;
    if (pattern.type === 'triangle' || pattern.type === 'equilateral_triangle') {
      steps.push({
        step: stepNum,
        text: 'Educational note: A triangle is the simplest closed polygon. It is rigid — specifying three side lengths uniquely determines the triangle (SSS congruence). The sum of interior angles is always 180 degrees.',
        textZh: '教学提示：三角形是最简单的闭合多边形，具有刚性——给定三边长度即可唯一确定三角形（SSS全等）。内角和恒为180度。',
      });
    } else if (pattern.type === 'square') {
      steps.push({
        step: stepNum,
        text: 'Educational note: A square is a regular quadrilateral with four equal sides and four right angles. Its diagonals are equal, perpendicular, and bisect each other.',
        textZh: '教学提示：正方形是正四边形，四边相等、四角为直角。其对角线相等、互相垂直且平分。',
      });
    } else if (pattern.type === 'midpoint') {
      steps.push({
        step: stepNum,
        text: 'Educational note: The midpoint divides a segment into two equal parts. The midpoint theorem states that the segment joining the midpoints of two sides of a triangle is parallel to the third side and half its length.',
        textZh: '教学提示：中点将线段分为两等份。中点定理指出，连接三角形两边中点的线段平行于第三边且长度为其一半。',
      });
    }
  }

  // 计算统计信息
  const stats: Narrative['stats'] = {
    pointCount: points.length,
    segmentCount: segments.length,
    constraintCount: constraints.length,
  };

  if (settings.showMeasurements) {
    let perimeter = 0;
    let area = 0;
    const adj = buildAdjacency(points, segments);

    // 尝试找到三角形以计算面积
    if (points.length >= 3) {
      for (const p of points) {
        const cycle = findCycle(adj, p.id, 3);
        if (cycle && cycle.length === 3) {
          const [a, b, c] = cycle.map((id) => pointMap.get(id)!);
          const ab = distance(a!, b!);
          const bc = distance(b!, c!);
          const ca = distance(c!, a!);
          perimeter = ab + bc + ca;
          const s = perimeter / 2;
          area = Math.sqrt(Math.max(0, s * (s - ab) * (s - bc) * (s - ca)));
          break;
        }
      }
    }

    // 如果没有找到三角形，尝试四边形
    if (perimeter === 0 && points.length >= 4) {
      for (const p of points) {
        const cycle = findCycle(adj, p.id, 4);
        if (cycle && cycle.length === 4) {
          const pts = cycle.map((id) => pointMap.get(id)!);
          for (let i = 0; i < 4; i++) {
            perimeter += distance(pts[i]!, pts[(i + 1) % 4]!);
          }
          // 使用 Shoelace 公式近似面积
          const shoelace = pts.reduce((sum, pt, i) => {
            const next = pts[(i + 1) % 4]!;
            return sum + pt.x * next.y - pt.y * next.x;
          }, 0);
          area = Math.abs(shoelace) / 2;
          break;
        }
      }
    }

    if (perimeter > 0) stats.perimeter = perimeter;
    if (area > 0) stats.area = area;
  }

  // 构建摘要
  let summary = `${pattern.label}. ${points.length} point(s), ${segments.length} segment(s), ${constraints.length} constraint(s).`;
  let summaryZh = `${pattern.labelZh}。${points.length} 个点，${segments.length} 条线段，${constraints.length} 个约束。`;
  if (stats.area !== undefined) {
    summary += ` Area: ${stats.area.toFixed(2)} sq units.`;
    summaryZh += ` 面积：${stats.area.toFixed(2)} 平方单位。`;
  }
  if (stats.perimeter !== undefined) {
    summary += ` Perimeter: ${stats.perimeter.toFixed(2)} units.`;
    summaryZh += ` 周长：${stats.perimeter.toFixed(2)} 单位。`;
  }

  return { pattern, summary, summaryZh, steps, stats };
}
