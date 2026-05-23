/**
 * @module components/panels/NarrativeExport
 * @description Penrose-style automated geometric narrative generator.
 *              Given the current geometric construction, automatically generates
 *              a narrative proof/story and an annotated SVG visualization.
 *
 *              Penrose 风格的自动化几何叙述生成器。
 *              根据当前几何构造，自动生成叙述性证明/故事和带标注的 SVG 可视化。
 */

import React, { useState, useCallback, useMemo, useRef } from 'react';
import Panel from './Panel';
import { useAppStore } from '@/stores';
import type { Point, Segment, Constraint } from '@/types';

// ================================================================
// Types / 类型定义
// ================================================================

/** Detected geometric pattern type */
type PatternType =
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

/** Result of pattern detection */
interface PatternInfo {
  type: PatternType;
  label: string;
  labelZh: string;
  detail: string;
  detailZh: string;
}

/** A single step in the generated narrative */
interface NarrativeStep {
  step: number;
  text: string;
  textZh: string;
}

/** The complete generated narrative */
interface Narrative {
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

/** Narrative generation settings */
type NarrativeStyle = 'detailed' | 'concise' | 'educational';
type NarrativeLanguage = 'zh' | 'en';

interface NarrativeSettings {
  style: NarrativeStyle;
  language: NarrativeLanguage;
  showConstraints: boolean;
  showMeasurements: boolean;
}

// ================================================================
// Helpers / 工具函数
// ================================================================

/** Map a node ID to a letter label (A, B, C, ...) */
function idToLabel(id: number, sortedIds: number[]): string {
  const idx = sortedIds.indexOf(id);
  if (idx < 0) return `N${id}`;
  if (idx < 26) return String.fromCharCode(65 + idx);
  return `P${idx - 25}`;
}

/** Calculate distance between two points */
function distance(a: Point, b: Point): number {
  const dx = a.x - b.x;
  const dy = a.y - b.y;
  return Math.sqrt(dx * dx + dy * dy);
}

/** Check if three points form an equilateral triangle within tolerance */
function isEquilateral(a: Point, b: Point, c: Point, tolerance = 0.05): boolean {
  const ab = distance(a, b);
  const bc = distance(b, c);
  const ca = distance(c, a);
  const avg = (ab + bc + ca) / 3;
  if (avg < 0.001) return false;
  return Math.abs(ab - avg) / avg < tolerance &&
         Math.abs(bc - avg) / avg < tolerance &&
         Math.abs(ca - avg) / avg < tolerance;
}

/** Check if four points form a square within tolerance */
function isSquare(a: Point, b: Point, c: Point, d: Point, tolerance = 0.05): boolean {
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

/** Build adjacency from segments: for each point, which other points it connects to */
function buildAdjacency(points: Point[], segments: Segment[]): Map<number, Set<number>> {
  const adj = new Map<number, Set<number>>();
  for (const p of points) adj.set(p.id, new Set());
  for (const seg of segments) {
    adj.get(seg.p1)?.add(seg.p2);
    adj.get(seg.p2)?.add(seg.p1);
  }
  return adj;
}

/** Find a cycle of a given length in the point graph, starting from startId */
function findCycle(adj: Map<number, Set<number>>, startId: number, targetLen: number): number[] | null {
  const visited = new Map<number, number>(); // node -> parent

  function dfs(current: number, depth: number): number[] | null {
    if (depth === targetLen) {
      // Check if we can close back to start
      if (adj.get(current)?.has(startId)) {
        // Reconstruct path
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
// Pattern Detection / 模式检测
// ================================================================

function detectPattern(
  points: Point[],
  segments: Segment[],
  constraints: Constraint[],
): PatternInfo {
  const n = points.length;
  const m = segments.length;

  // No points case
  if (n === 0) {
    return {
      type: 'free_construction',
      label: 'Empty Canvas',
      labelZh: '空白画布',
      detail: 'No points have been placed yet.',
      detailZh: '尚未放置任何点。',
    };
  }

  // Single point
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

  // Check for betweenness (midpoint detection)
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

  // Check for intersection
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

  // Build adjacency for cycle detection
  const adj = buildAdjacency(points, segments);

  // Check for 3-cycle (triangle)
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

  // Check for 4-cycle (quadrilateral)
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

  // Generic construction
  return {
    type: 'free_construction',
    label: 'Free Construction',
    labelZh: '自由构造',
    detail: `A construction with ${n} point(s) and ${m} segment(s). Adding more structure may reveal geometric patterns.`,
    detailZh: `含 ${n} 个点和 ${m} 条线段的自由构造。添加更多结构或许能揭示几何模式。`,
  };
}

// ================================================================
// Narrative Generation / 叙述生成
// ================================================================

function generateNarrative(
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

  // Step 1: Introduce points
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

  // Step 2: Segments
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

  // Step 3: Constraints
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

  // Step 4: Pattern conclusion
  stepNum++;
  steps.push({
    step: stepNum,
    text: `Pattern detected: ${pattern.label}. ${pattern.detail}`,
    textZh: `检测到的模式：${pattern.labelZh}。${pattern.detailZh}`,
  });

  // Educational extra step
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

  // Compute statistics
  const stats: Narrative['stats'] = {
    pointCount: points.length,
    segmentCount: segments.length,
    constraintCount: constraints.length,
  };

  if (settings.showMeasurements) {
    // Compute perimeter if we have a cycle
    let perimeter = 0;
    let area = 0;
    const adj = buildAdjacency(points, segments);

    // Try to find a 3-cycle for area calculation
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

    // If no triangle found, try 4-cycle
    if (perimeter === 0 && points.length >= 4) {
      for (const p of points) {
        const cycle = findCycle(adj, p.id, 4);
        if (cycle && cycle.length === 4) {
          const pts = cycle.map((id) => pointMap.get(id)!);
          for (let i = 0; i < 4; i++) {
            perimeter += distance(pts[i]!, pts[(i + 1) % 4]!);
          }
          // Approximate area using Shoelace formula
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

  // Build summary
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

// ================================================================
// SVG Generation / SVG 生成
// ================================================================

function generateSVG(
  points: Point[],
  segments: Segment[],
  constraints: Constraint[],
  pattern: PatternInfo,
): string {
  if (points.length === 0) {
    return `<svg xmlns="http://www.w3.org/2000/svg" width="400" height="200" style="background:#1a1a2e">
  <text x="200" y="100" text-anchor="middle" fill="#6c6c8a" font-family="monospace" font-size="14">
    No points to render / 无点可渲染
  </text>
</svg>`;
  }

  // Compute bounding box with padding
  const xs = points.map((p) => p.x);
  const ys = points.map((p) => p.y);
  const minX = Math.min(...xs);
  const maxX = Math.max(...xs);
  const minY = Math.min(...ys);
  const maxY = Math.max(...ys);

  const padX = Math.max((maxX - minX) * 0.3, 1);
  const padY = Math.max((maxY - minY) * 0.3, 1);

  const viewMinX = minX - padX;
  const viewMaxX = maxX + padX;
  const viewMinY = minY - padY;
  const viewMaxY = maxY + padY;
  const viewW = viewMaxX - viewMinX;
  const viewH = viewMaxY - viewMinY;

  // SVG canvas size
  const svgW = Math.max(400, Math.min(800, viewW * 60));
  const svgH = Math.max(300, Math.min(600, viewH * 60));

  // Scale world coords to SVG coords (Y-flipped)
  const tx = (x: number) => ((x - viewMinX) / viewW) * svgW;
  const ty = (y: number) => svgH - ((y - viewMinY) / viewH) * svgH;

  const sortedIds = [...points].sort((a, b) => a.id - b.id).map((p) => p.id);
  const pointMap = new Map<number, Point>();
  for (const p of points) pointMap.set(p.id, p);

  // Colors matching dark theme and trust color system
  const colors = {
    bg: '#1a1a2e',
    grid: '#252540',
    point: '#00d4ff',
    pointStroke: '#1a1a2e',
    segment: '#7b8cff',
    segmentDim: '#4a4a6a',
    constraint: '#ff6b9d',
    constraintDash: '#ff6b9d88',
    text: '#c8c8e0',
    textDim: '#6c6c8a',
    title: '#e0e0ff',
    accent: '#00e5a0',
  };

  let svgContent = '';

  // Grid lines
  const gridStep = viewW / 10;
  for (let gx = viewMinX - (viewMinX % gridStep); gx <= viewMaxX; gx += gridStep) {
    const sx = tx(gx);
    svgContent += `  <line x1="${sx}" y1="0" x2="${sx}" y2="${svgH}" stroke="${colors.grid}" stroke-width="0.5"/>\n`;
  }
  for (let gy = viewMinY - (viewMinY % gridStep); gy <= viewMaxY; gy += gridStep) {
    const sy = ty(gy);
    svgContent += `  <line x1="0" y1="${sy}" x2="${svgW}" y2="${sy}" stroke="${colors.grid}" stroke-width="0.5"/>\n`;
  }

  // Title block background
  svgContent += `  <rect x="10" y="8" width="${svgW - 20}" height="32" rx="4" fill="#1a1a2ecc" stroke="${colors.segmentDim}" stroke-width="0.5"/>\n`;
  svgContent += `  <text x="${svgW / 2}" y="30" text-anchor="middle" fill="${colors.title}" font-family="monospace" font-size="13" font-weight="bold">${pattern.label} / ${pattern.labelZh}</text>\n`;

  // Segments
  for (const seg of segments) {
    const p1 = pointMap.get(seg.p1);
    const p2 = pointMap.get(seg.p2);
    if (!p1 || !p2) continue;
    const x1 = tx(p1.x);
    const y1 = ty(p1.y);
    const x2 = tx(p2.x);
    const y2 = ty(p2.y);
    svgContent += `  <line x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}" stroke="${colors.segment}" stroke-width="2" stroke-linecap="round"/>\n`;

    // Length label at midpoint
    const midX = (x1 + x2) / 2;
    const midY = (y1 + y2) / 2;
    const d = distance(p1, p2);
    const labelP1 = idToLabel(seg.p1, sortedIds);
    const labelP2 = idToLabel(seg.p2, sortedIds);
    svgContent += `  <text x="${midX}" y="${midY - 6}" text-anchor="middle" fill="${colors.textDim}" font-family="monospace" font-size="9">${labelP1}${labelP2}=${d.toFixed(2)}</text>\n`;
  }

  // Constraints (dashed / special lines)
  for (const c of constraints) {
    if (c.type === 'betweenness' && c.args.length >= 3) {
      const a = pointMap.get(c.args[0]!);
      const b = pointMap.get(c.args[1]!);
      const cp = pointMap.get(c.args[2]!);
      if (!a || !b || !cp) continue;
      const xA = tx(a.x), yA = ty(a.y);
      const xC = tx(cp.x), yC = ty(cp.y);
      svgContent += `  <line x1="${xA}" y1="${yA}" x2="${xC}" y2="${yC}" stroke="${colors.constraintDash}" stroke-width="1" stroke-dasharray="5,5"/>\n`;
    } else if (c.type === 'incidence') {
      // Draw small marker at the involved point
      for (const argId of c.args) {
        const pt = pointMap.get(argId);
        if (!pt) continue;
        const px = tx(pt.x), py = ty(pt.y);
        svgContent += `  <circle cx="${px}" cy="${py}" r="7" fill="none" stroke="${colors.constraint}" stroke-width="0.8" stroke-dasharray="3,2"/>\n`;
      }
    }
  }

  // Points (drawn on top)
  for (const p of points) {
    const cx = tx(p.x);
    const cy = ty(p.y);
    const label = idToLabel(p.id, sortedIds);
    svgContent += `  <circle cx="${cx}" cy="${cy}" r="5" fill="${colors.point}" stroke="${colors.pointStroke}" stroke-width="1.5"/>\n`;
    svgContent += `  <text x="${cx}" y="${cy - 10}" text-anchor="middle" fill="${colors.text}" font-family="monospace" font-size="12" font-weight="bold">${label}</text>\n`;
  }

  // Stats footer
  const footerY = svgH - 8;
  svgContent += `  <text x="${svgW / 2}" y="${footerY}" text-anchor="middle" fill="${colors.textDim}" font-family="monospace" font-size="10">Points: ${points.length} | Segments: ${segments.length} | Constraints: ${constraints.length}</text>\n`;

  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${svgW}" height="${svgH}" viewBox="0 0 ${svgW} ${svgH}" style="background:${colors.bg};font-family:monospace">
${svgContent}</svg>`;

  return svg;
}

// ================================================================
// Pattern Icon Mapping / 模式图标映射
// ================================================================

const PATTERN_ICONS: Record<PatternType, string> = {
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
// Component / 组件
// ================================================================

const NarrativeExport: React.FC = () => {
  const points = useAppStore((s) => s.points);
  const segments = useAppStore((s) => s.segments);
  const constraints = useAppStore((s) => s.constraints);
  const addToast = useAppStore((s) => s.addToast);

  const [narrative, setNarrative] = useState<Narrative | null>(null);
  const [svgString, setSvgString] = useState<string>('');
  const [settings, setSettings] = useState<NarrativeSettings>({
    style: 'detailed',
    language: 'zh',
    showConstraints: true,
    showMeasurements: true,
  });
  const [settingsOpen, setSettingsOpen] = useState(false);
  const [generating, setGenerating] = useState(false);
  const narrativeRef = useRef<HTMLDivElement>(null);

  // Detect pattern (reactive, always up-to-date)
  const pattern = useMemo(
    () => detectPattern(points, segments, constraints),
    [points, segments, constraints],
  );

  // Generate narrative
  const handleGenerateNarrative = useCallback(() => {
    setGenerating(true);
    // Use setTimeout to allow the "generating" UI flash to render
    setTimeout(() => {
      const narr = generateNarrative(points, segments, constraints, pattern, settings);
      setNarrative(narr);
      setGenerating(false);
    }, 150);
  }, [points, segments, constraints, pattern, settings]);

  // Generate SVG
  const handleGenerateSVG = useCallback(() => {
    const svg = generateSVG(points, segments, constraints, pattern);
    setSvgString(svg);
    addToast('success', settings.language === 'zh' ? 'SVG 已生成' : 'SVG generated');
  }, [points, segments, constraints, pattern, settings.language, addToast]);

  // Copy narrative to clipboard
  const handleCopyNarrative = useCallback(async () => {
    if (!narrative) return;
    const text = settings.language === 'zh'
      ? narrative.steps.map((s) => `步骤${s.step}: ${s.textZh}`).join('\n')
      : narrative.steps.map((s) => `Step ${s.step}: ${s.text}`).join('\n');
    try {
      await navigator.clipboard.writeText(text);
      addToast('success', settings.language === 'zh' ? '叙述已复制到剪贴板' : 'Narrative copied to clipboard');
    } catch {
      addToast('error', settings.language === 'zh' ? '复制失败' : 'Copy failed');
    }
  }, [narrative, settings.language, addToast]);

  // Download SVG
  const handleDownloadSVG = useCallback(() => {
    if (!svgString) return;
    const blob = new Blob([svgString], { type: 'image/svg+xml' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `lv00-construction-${Date.now()}.svg`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
    addToast('success', settings.language === 'zh' ? 'SVG 已下载' : 'SVG downloaded');
  }, [svgString, settings.language, addToast]);

  // Update a setting
  const updateSetting = useCallback(
    <K extends keyof NarrativeSettings>(key: K, value: NarrativeSettings[K]) => {
      setSettings((prev) => ({ ...prev, [key]: value }));
      // Clear previous results when settings change
      setNarrative(null);
      setSvgString('');
    },
    [],
  );

  const icon = PATTERN_ICONS[pattern.type];

  return (
    <Panel
      title="NARRATIVE EXPORT / 叙述导出"
      panelId="panelNarrativeExport"
      icon={'\u2728'}
    >
      {/* Current pattern indicator */}
      <div className="info-box" style={{ marginBottom: 8, textAlign: 'center' }}>
        <div style={{ fontSize: 20, marginBottom: 2 }}>{icon}</div>
        <div style={{ fontSize: 11, color: 'var(--color-text-secondary)' }}>
          {pattern.labelZh}
        </div>
        <div style={{ fontSize: 10, color: 'var(--color-text-muted)', marginTop: 2 }}>
          {settings.language === 'zh' ? pattern.detailZh : pattern.detail}
        </div>
      </div>

      {/* Penrose-style Settings */}
      <details
        open={settingsOpen}
        onToggle={(e) => setSettingsOpen((e.target as HTMLDetailsElement).open)}
        style={{ marginBottom: 8, fontSize: 11 }}
      >
        <summary style={{
          cursor: 'pointer',
          color: 'var(--color-text-secondary)',
          padding: '4px 0',
          userSelect: 'none',
        }}>
          {'\u2699'} PENROSE SETTINGS / 设置
        </summary>
        <div style={{ padding: '8px 0 4px 0' }}>
          {/* Narrative Style */}
          <div className="input-row" style={{ marginBottom: 6 }}>
            <label style={{ minWidth: 60 }}>STYLE / 风格</label>
            <select
              className="select-field"
              value={settings.style}
              onChange={(e) => updateSetting('style', e.target.value as NarrativeStyle)}
              style={{ fontSize: 11, marginBottom: 0 }}
            >
              <option value="detailed">Detailed / 详细</option>
              <option value="concise">Concise / 简洁</option>
              <option value="educational">Educational / 教学</option>
            </select>
          </div>

          {/* Language */}
          <div className="input-row" style={{ marginBottom: 6 }}>
            <label style={{ minWidth: 60 }}>LANG / 语言</label>
            <select
              className="select-field"
              value={settings.language}
              onChange={(e) => updateSetting('language', e.target.value as NarrativeLanguage)}
              style={{ fontSize: 11, marginBottom: 0 }}
            >
              <option value="zh">Chinese / 中文</option>
              <option value="en">English / 英文</option>
            </select>
          </div>

          {/* Show constraints toggle */}
          <div className="input-row" style={{ marginBottom: 6 }}>
            <label style={{ minWidth: 60 }}>CONSTRAINTS</label>
            <select
              className="select-field"
              value={settings.showConstraints ? 'yes' : 'no'}
              onChange={(e) => updateSetting('showConstraints', e.target.value === 'yes')}
              style={{ fontSize: 11, marginBottom: 0 }}
            >
              <option value="yes">Show / 显示</option>
              <option value="no">Hide / 隐藏</option>
            </select>
          </div>

          {/* Show measurements toggle */}
          <div className="input-row">
            <label style={{ minWidth: 60 }}>MEASUREMENTS</label>
            <select
              className="select-field"
              value={settings.showMeasurements ? 'yes' : 'no'}
              onChange={(e) => updateSetting('showMeasurements', e.target.value === 'yes')}
              style={{ fontSize: 11, marginBottom: 0 }}
            >
              <option value="yes">Show / 显示</option>
              <option value="no">Hide / 隐藏</option>
            </select>
          </div>
        </div>
      </details>

      {/* Action Buttons */}
      <button
        className="btn btn-accent"
        onClick={handleGenerateNarrative}
        disabled={points.length === 0 || generating}
      >
        {generating ? '\u23F3 GENERATING... / 生成中...' : '\uD83D\uDCC4 GENERATE NARRATIVE / 生成叙述'}
      </button>

      <button
        className="btn"
        onClick={handleGenerateSVG}
        disabled={points.length === 0}
      >
        {'\uD83D\uDDBC'} EXPORT SVG / 导出SVG
      </button>

      {/* Narrative Display Area */}
      {narrative && (
        <div ref={narrativeRef} style={{ marginTop: 10 }}>
          {/* Pattern header */}
          <div className="info-box" style={{
            marginBottom: 8,
            borderLeft: '3px solid var(--color-accent)',
            padding: '8px 10px',
          }}>
            <div style={{ fontSize: 11, color: 'var(--color-text-secondary)', marginBottom: 2 }}>
              {settings.language === 'zh' ? '\uD83D\uDCD0 检测到：' : '\uD83D\uDCD0 Detected: '}
              {settings.language === 'zh' ? narrative.pattern.labelZh : narrative.pattern.label}
            </div>
            <div style={{ fontSize: 10, color: 'var(--color-text-muted)' }}>
              {settings.language === 'zh' ? narrative.summaryZh : narrative.summary}
            </div>
          </div>

          {/* Steps */}
          <div style={{
            maxHeight: 280,
            overflowY: 'auto',
            border: '1px solid var(--color-border-secondary)',
            borderRadius: 'var(--radius-sm)',
            padding: '6px 8px',
            background: 'var(--color-bg-tertiary)',
            marginBottom: 8,
          }}>
            {narrative.steps.map((s) => (
              <div key={s.step} style={{
                display: 'flex',
                gap: 8,
                padding: '3px 0',
                borderBottom: '1px solid var(--color-border-secondary)',
                fontSize: 11,
              }}>
                <span style={{
                  color: 'var(--color-accent)',
                  fontWeight: 'var(--font-weight-semibold)',
                  minWidth: 24,
                  flexShrink: 0,
                }}>
                  {settings.language === 'zh' ? `步骤${s.step}` : `Step ${s.step}`}
                </span>
                <span style={{ color: 'var(--color-text-primary)', lineHeight: 1.4 }}>
                  {settings.language === 'zh' ? s.textZh : s.text}
                </span>
              </div>
            ))}
          </div>

          {/* Statistics */}
          <div className="info-box">
            <div className="info-row">
              <span className="info-label">
                {settings.language === 'zh' ? '点' : 'Points'}
              </span>
              <span className="info-value">{narrative.stats.pointCount}</span>
            </div>
            <div className="info-row">
              <span className="info-label">
                {settings.language === 'zh' ? '线段' : 'Segments'}
              </span>
              <span className="info-value">{narrative.stats.segmentCount}</span>
            </div>
            <div className="info-row">
              <span className="info-label">
                {settings.language === 'zh' ? '约束' : 'Constraints'}
              </span>
              <span className="info-value">{narrative.stats.constraintCount}</span>
            </div>
            {narrative.stats.area !== undefined && (
              <div className="info-row">
                <span className="info-label">
                  {settings.language === 'zh' ? '面积' : 'Area'}
                </span>
                <span className="info-value">{narrative.stats.area.toFixed(2)}</span>
              </div>
            )}
            {narrative.stats.perimeter !== undefined && (
              <div className="info-row">
                <span className="info-label">
                  {settings.language === 'zh' ? '周长' : 'Perimeter'}
                </span>
                <span className="info-value">{narrative.stats.perimeter.toFixed(2)}</span>
              </div>
            )}
          </div>

          {/* Copy & Download buttons */}
          <div style={{ display: 'flex', gap: 6, marginTop: 8 }}>
            <button
              className="btn btn-small"
              onClick={handleCopyNarrative}
              style={{ flex: 1 }}
            >
              {'\uD83D\uDCCB'} {settings.language === 'zh' ? '复制叙述' : 'COPY NARRATIVE'}
            </button>
            <button
              className="btn btn-small"
              onClick={handleDownloadSVG}
              disabled={!svgString}
              style={{ flex: 1 }}
            >
              {'\u2B07'} {settings.language === 'zh' ? '下载SVG' : 'DOWNLOAD SVG'}
            </button>
          </div>
        </div>
      )}

      {/* Empty state */}
      {!narrative && points.length === 0 && (
        <div style={{
          textAlign: 'center',
          padding: '20px 10px',
          color: 'var(--color-text-muted)',
          fontSize: 12,
        }}>
          <div style={{ fontSize: 32, marginBottom: 8, opacity: 0.4 }}>{'\u221E'}</div>
          <div>{'\u2190'} Place points to begin / 放置点开始构造</div>
        </div>
      )}

      {/* No narrative generated yet but points exist */}
      {!narrative && points.length > 0 && (
        <div style={{
          textAlign: 'center',
          padding: '16px 10px',
          color: 'var(--color-text-muted)',
          fontSize: 12,
        }}>
          <div style={{ marginBottom: 4 }}>
            {settings.language === 'zh'
              ? `点击上方按钮生成叙述 (${points.length} 个点可用)`
              : `Click the button above to generate a narrative (${points.length} point(s) available)`}
          </div>
        </div>
      )}

      {/* SVG preview */}
      {svgString && (
        <div style={{ marginTop: 10 }}>
          <div style={{
            fontSize: 11,
            color: 'var(--color-text-secondary)',
            marginBottom: 4,
            fontWeight: 'var(--font-weight-semibold)',
          }}>
            {'\uD83D\uDDBC'} SVG PREVIEW / 预览
          </div>
          <div
            style={{
              border: '1px solid var(--color-border-secondary)',
              borderRadius: 'var(--radius-sm)',
              overflow: 'hidden',
              background: '#1a1a2e',
            }}
            dangerouslySetInnerHTML={{ __html: svgString }}
          />
        </div>
      )}
    </Panel>
  );
};

export default NarrativeExport;
