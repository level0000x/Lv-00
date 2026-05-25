/**
 * @module components/panels/utils/graphPresets
 * @description 图模块预设几何配置。
 *              提供常见几何配置的快速加载功能，包括等边三角形、正方形、
 *              线段相交和中点构造。
 *
 *              Graph module preset geometry configurations.
 *              Provides quick-load functionality for common geometric configurations.
 */

import type { Constraint } from '@/types';
import {
  calculateEquilateralTriangle,
  calculateSquare,
  calculateMidpoint,
} from '@/utils/geometryAlgorithms';
import { generateUniqueId } from '@/utils/idGenerator';

// ================================================================
// 类型定义 / Type Definitions
// ================================================================

/** 预设几何配置项 */
export interface GraphPreset {
  id: string;
  label: string;
}

/** 预设生成的几何数据 */
export interface PresetGeometry {
  points: Array<{ id: number; x: number; y: number }>;
  segments: Array<{ id: number; p1: number; p2: number }>;
  constraints: Constraint[];
}

// ================================================================
// 预设列表 / Preset List
// ================================================================

/** 图预设列表，用于快速加载常见几何配置 */
export const GRAPH_PRESETS: GraphPreset[] = [
  { id: 'triangle', label: 'EQUILATERAL TRIANGLE / 等边三角形' },
  { id: 'square', label: 'SQUARE / 正方形' },
  { id: 'intersection', label: 'SEGMENT INTERSECTION / 线段相交' },
  { id: 'midpoint', label: 'MIDPOINT / 中点' },
];

// ================================================================
// 预设几何生成 / Preset Geometry Generation
// ================================================================

/**
 * 根据预设 ID 生成对应的几何数据（点、线段、约束）。
 *
 * @param presetId - 预设标识符
 * @returns 生成的几何数据，如果预设未知则返回 null
 */
export function generatePresetGeometry(presetId: string): PresetGeometry | null {
  const newPoints: PresetGeometry['points'] = [];
  const newSegments: PresetGeometry['segments'] = [];
  const newConstraints: Constraint[] = [];

  switch (presetId) {
    case 'triangle': {
      // 等边三角形
      const verts = calculateEquilateralTriangle(200);
      const p0 = { id: generateUniqueId(), x: verts[0]!.x, y: verts[0]!.y };
      const p1 = { id: generateUniqueId(), x: verts[1]!.x, y: verts[1]!.y };
      const p2 = { id: generateUniqueId(), x: verts[2]!.x, y: verts[2]!.y };
      newPoints.push(p0, p1, p2);
      newSegments.push(
        { id: generateUniqueId(), p1: p0.id, p2: p1.id },
        { id: generateUniqueId(), p1: p1.id, p2: p2.id },
        { id: generateUniqueId(), p1: p2.id, p2: p0.id },
      );
      return { points: newPoints, segments: newSegments, constraints: newConstraints };
    }
    case 'square': {
      // 正方形
      const verts = calculateSquare(200);
      const p0 = { id: generateUniqueId(), x: verts[0]!.x, y: verts[0]!.y };
      const p1 = { id: generateUniqueId(), x: verts[1]!.x, y: verts[1]!.y };
      const p2 = { id: generateUniqueId(), x: verts[2]!.x, y: verts[2]!.y };
      const p3 = { id: generateUniqueId(), x: verts[3]!.x, y: verts[3]!.y };
      newPoints.push(p0, p1, p2, p3);
      newSegments.push(
        { id: generateUniqueId(), p1: p0.id, p2: p1.id },
        { id: generateUniqueId(), p1: p1.id, p2: p2.id },
        { id: generateUniqueId(), p1: p2.id, p2: p3.id },
        { id: generateUniqueId(), p1: p3.id, p2: p0.id },
      );
      return { points: newPoints, segments: newSegments, constraints: newConstraints };
    }
    case 'intersection': {
      // 两条交叉线段：(-150,-100)->(150,100) 和 (-150,100)->(150,-100)
      const p0 = { id: generateUniqueId(), x: -150, y: -100 };
      const p1 = { id: generateUniqueId(), x: 150, y: 100 };
      const p2 = { id: generateUniqueId(), x: -150, y: 100 };
      const p3 = { id: generateUniqueId(), x: 150, y: -100 };
      // 交点在原点
      const p4 = { id: generateUniqueId(), x: 0, y: 0 };
      newPoints.push(p0, p1, p2, p3, p4);
      const s0 = { id: generateUniqueId(), p1: p0.id, p2: p1.id };
      const s1 = { id: generateUniqueId(), p1: p2.id, p2: p3.id };
      newSegments.push(s0, s1);
      // 相交约束
      newConstraints.push({
        id: generateUniqueId(),
        type: 'intersection',
        args: [s0.id, s1.id],
      });
      // 关联约束：交点在两条线段上
      newConstraints.push({
        id: generateUniqueId(),
        type: 'incidence',
        args: [p4.id, s0.id],
      });
      newConstraints.push({
        id: generateUniqueId(),
        type: 'incidence',
        args: [p4.id, s1.id],
      });
      return { points: newPoints, segments: newSegments, constraints: newConstraints };
    }
    case 'midpoint': {
      // 两点及其中点
      const p0 = { id: generateUniqueId(), x: -150, y: 0 };
      const p1 = { id: generateUniqueId(), x: 150, y: 0 };
      const mid = calculateMidpoint(p0, p1);
      const p2 = { id: generateUniqueId(), x: mid.x, y: mid.y };
      newPoints.push(p0, p1, p2);
      const segId = generateUniqueId();
      newSegments.push({ id: segId, p1: p0.id, p2: p1.id });
      // 介于约束：中点在两个端点之间
      newConstraints.push({
        id: generateUniqueId(),
        type: 'betweenness',
        args: [p0.id, p2.id, p1.id],
      });
      // 关联约束：中点在线段上
      newConstraints.push({
        id: generateUniqueId(),
        type: 'incidence',
        args: [p2.id, segId],
      });
      return { points: newPoints, segments: newSegments, constraints: newConstraints };
    }
    default:
      return null;
  }
}
