/**
 * @module components/panels/utils/narrativeSvg
 * @description 叙述模块 SVG 生成工具。
 *              为当前几何构造生成带标注的 SVG 可视化图，
 *              包含网格、线段、约束关系、点标签和统计信息。
 *
 *              Narrative module SVG generator.
 *              Generates annotated SVG visualization for the current
 *              geometric construction, including grid, segments,
 *              constraints, point labels, and statistics.
 */

import type { Point, Segment, Constraint } from '@/types';
import { idToLabel, distance } from './narrativeGenerator';
import type { PatternInfo } from './narrativeGenerator';

// ================================================================
// SVG 生成 / SVG Generation
// ================================================================

/**
 * 为当前几何构造生成带标注的 SVG 可视化图。
 * 包含网格线、标题栏、线段（带长度标签）、约束关系（虚线）、
 * 点（带字母标签）和统计信息页脚。
 *
 * @param points - 当前点集合
 * @param segments - 当前线段集合
 * @param constraints - 当前约束集合
 * @param pattern - 检测到的模式信息
 * @returns SVG 字符串
 */
export function generateNarrativeSVG(
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

  // 计算包围盒（带边距）
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

  // SVG 画布尺寸
  const svgW = Math.max(400, Math.min(800, viewW * 60));
  const svgH = Math.max(300, Math.min(600, viewH * 60));

  // 世界坐标 -> SVG 坐标变换（Y 轴翻转）
  const tx = (x: number) => ((x - viewMinX) / viewW) * svgW;
  const ty = (y: number) => svgH - ((y - viewMinY) / viewH) * svgH;

  const sortedIds = [...points].sort((a, b) => a.id - b.id).map((p) => p.id);
  const pointMap = new Map<number, Point>();
  for (const p of points) pointMap.set(p.id, p);

  // 颜色方案（匹配暗色主题和信任色系统）
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

  // 网格线
  const gridStep = viewW / 10;
  for (let gx = viewMinX - (viewMinX % gridStep); gx <= viewMaxX; gx += gridStep) {
    const sx = tx(gx);
    svgContent += `  <line x1="${sx}" y1="0" x2="${sx}" y2="${svgH}" stroke="${colors.grid}" stroke-width="0.5"/>\n`;
  }
  for (let gy = viewMinY - (viewMinY % gridStep); gy <= viewMaxY; gy += gridStep) {
    const sy = ty(gy);
    svgContent += `  <line x1="0" y1="${sy}" x2="${svgW}" y2="${sy}" stroke="${colors.grid}" stroke-width="0.5"/>\n`;
  }

  // 标题栏背景
  svgContent += `  <rect x="10" y="8" width="${svgW - 20}" height="32" rx="4" fill="#1a1a2ecc" stroke="${colors.segmentDim}" stroke-width="0.5"/>\n`;
  svgContent += `  <text x="${svgW / 2}" y="30" text-anchor="middle" fill="${colors.title}" font-family="monospace" font-size="13" font-weight="bold">${pattern.label} / ${pattern.labelZh}</text>\n`;

  // 线段
  for (const seg of segments) {
    const p1 = pointMap.get(seg.p1);
    const p2 = pointMap.get(seg.p2);
    if (!p1 || !p2) continue;
    const x1 = tx(p1.x);
    const y1 = ty(p1.y);
    const x2 = tx(p2.x);
    const y2 = ty(p2.y);
    svgContent += `  <line x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}" stroke="${colors.segment}" stroke-width="2" stroke-linecap="round"/>\n`;

    // 中点长度标签
    const midX = (x1 + x2) / 2;
    const midY = (y1 + y2) / 2;
    const d = distance(p1, p2);
    const labelP1 = idToLabel(seg.p1, sortedIds);
    const labelP2 = idToLabel(seg.p2, sortedIds);
    svgContent += `  <text x="${midX}" y="${midY - 6}" text-anchor="middle" fill="${colors.textDim}" font-family="monospace" font-size="9">${labelP1}${labelP2}=${d.toFixed(2)}</text>\n`;
  }

  // 约束关系（虚线 / 特殊标记）
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
      for (const argId of c.args) {
        const pt = pointMap.get(argId);
        if (!pt) continue;
        const px = tx(pt.x), py = ty(pt.y);
        svgContent += `  <circle cx="${px}" cy="${py}" r="7" fill="none" stroke="${colors.constraint}" stroke-width="0.8" stroke-dasharray="3,2"/>\n`;
      }
    }
  }

  // 点（最后绘制，在最上层）
  for (const p of points) {
    const cx = tx(p.x);
    const cy = ty(p.y);
    const label = idToLabel(p.id, sortedIds);
    svgContent += `  <circle cx="${cx}" cy="${cy}" r="5" fill="${colors.point}" stroke="${colors.pointStroke}" stroke-width="1.5"/>\n`;
    svgContent += `  <text x="${cx}" y="${cy - 10}" text-anchor="middle" fill="${colors.text}" font-family="monospace" font-size="12" font-weight="bold">${label}</text>\n`;
  }

  // 统计信息页脚
  const footerY = svgH - 8;
  svgContent += `  <text x="${svgW / 2}" y="${footerY}" text-anchor="middle" fill="${colors.textDim}" font-family="monospace" font-size="10">Points: ${points.length} | Segments: ${segments.length} | Constraints: ${constraints.length}</text>\n`;

  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${svgW}" height="${svgH}" viewBox="0 0 ${svgW} ${svgH}" style="background:${colors.bg};font-family:monospace">
${svgContent}</svg>`;

  return svg;
}
