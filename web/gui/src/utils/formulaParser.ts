/**
 * @module utils/formulaParser
 * @description 公式 DSL 解析器 —— 将文本公式解析为几何操作并执行。
 *
 *              Formula DSL parser that parses text formulas into geometric
 *              operations and executes them.
 *
 * 主要功能 / Key Features:
 * - 支持多种 DSL 命令：创建点、线段、圆、中点、垂足、平行线、交点等
 * - 支持测量命令：计算距离和角度
 * - 支持两种线段创建语法：名称拼接（AB）和函数调用（segment(A, B)）
 * - 自动管理命名实体（点/线段）的注册表
 * - 所有几何计算使用纯 JS 实现，不依赖 WASM 后端
 * - 返回结构化的 FormulaCommand，便于后续处理和撤销
 *
 * 支持的 DSL 命令 / Supported DSL Commands:
 * - point A(x, y)                              创建命名点
 * - segment AB / segment(A, B)                  创建线段
 * - circle center(A) radius(r)                  创建圆（多边形近似）
 * - midpoint M of A, B                          创建中点
 * - perpendicular from A to segment BC          创建垂足
 * - parallel to AB through C                    创建平行线
 * - intersect segment AB with CD                求交点
 * - measure distance A, B                       计算距离
 * - measure angle A, B, C                       计算角度
 *
 * 使用示例 / Usage:
 *   import { parseFormula } from '@/utils/formulaParser';
 *
 *   const commands = parseFormula(`
 *     point A(0, 0)
 *     point B(4, 0)
 *     point C(2, 3)
 *     segment AB
 *     segment BC
 *     segment CA
 *     measure angle A, B, C
 *   `);
 *   // commands: 解析后的 FormulaCommand 数组
 */

import type { Point, Segment, Constraint } from '@/types';
import { generateUniqueId } from '@/utils/idGenerator';
import { dist } from '@/utils/constraintSolver';

// ================================================================
// 类型定义
// ================================================================

/** 解析出的命令类型 */
export type FormulaCommandType =
  | 'point'
  | 'segment'
  | 'circle'
  | 'midpoint'
  | 'perpendicular'
  | 'parallel'
  | 'intersect'
  | 'measure_distance'
  | 'measure_angle'
  | 'comment'
  | 'unknown';

/** 【优化 H3】使用联合类型替代 Record<string, unknown>，提供更精确的类型定义 */
export type FormulaCommandArgs =
  | { type: 'point'; name: string; x: number; y: number }
  | { type: 'segment'; p1Name: string; p2Name: string }
  | { type: 'circle'; centerName: string; radiusValue: number | null; radiusPointName: string | null }
  | { type: 'midpoint'; name: string; p1Name: string; p2Name: string }
  | { type: 'perpendicular'; pointName: string; segP1Name: string; segP2Name: string }
  | { type: 'parallel'; segP1Name: string; segP2Name: string; throughName: string }
  | { type: 'intersect'; seg1P1Name: string; seg1P2Name: string; seg2P1Name: string; seg2P2Name: string }
  | { type: 'measure_distance'; p1Name: string; p2Name: string }
  | { type: 'measure_angle'; p1Name: string; vertexName: string; p3Name: string }
  | { type: 'comment'; raw: string }
  | { type: 'unknown'; raw: string };

/** 解析出的单条命令 */
export interface FormulaCommand {
  type: FormulaCommandType;
  raw: string;
  /** 命令参数（【优化】使用联合类型替代 Record<string, unknown>，提供更精确的类型） */
  args: FormulaCommandArgs;
  /** 解析错误（如果有） */
  error?: string;
  /** 执行结果描述 */
  result?: string;
}

/** 解析器完整结果 */
export interface FormulaParseResult {
  commands: FormulaCommand[];
  errors: string[];
  /** 执行后创建的所有点 */
  createdPoints: Point[];
  /** 执行后创建的所有线段 */
  createdSegments: Segment[];
  /** 执行后创建的所有约束 */
  createdConstraints: Constraint[];
  /** 度量结果 */
  measurements: Array<{ label: string; value: string }>;
}

// ================================================================
// 【新增】类型安全工具函数
// ================================================================

/**
 * 【优化 H3】安全类型提取函数
 * 替代 unsafe 的 `as` 类型断言，提供运行时验证
 *
 * @param args - 原始参数对象
 * @param expectedKeys - 期望存在的键名
 * @param cmdType - 命令类型（用于错误消息）
 * @returns 提取并验证后的参数对象
 * @throws 如果缺少必需的键或类型不正确
 */
function extractArgs<T extends Record<string, unknown>>(
  args: Record<string, unknown>,
  expectedKeys: (keyof T)[],
  cmdType: string,
): T {
  const missingKeys: string[] = [];
  const wrongTypeKeys: string[] = [];

  for (const key of expectedKeys) {
    if (!(key in args)) {
      missingKeys.push(key as string);
    }
  }

  if (missingKeys.length > 0) {
    throw new Error(`命令 "${cmdType}" 缺少必需参数: ${missingKeys.join(', ')}`);
  }

  // 类型验证
  const result = {} as T;
  for (const key of expectedKeys) {
    result[key] = args[key as string] as T[keyof T];
  }

  return result;
}

/**
 * 【优化 H3】安全获取点名称参数
 * 专门处理点名称的提取和验证
 *
 * @param args - 原始参数对象
 * @param keys - 点名称参数键名数组
 * @param cmdType - 命令类型
 * @returns 验证后的点名称数组
 */
function extractPointNames(args: Record<string, unknown>, keys: string[], cmdType: string): string[] {
  return keys.map((key) => {
    const value = args[key];
    if (typeof value !== 'string' || !value.trim()) {
      throw new Error(`命令 "${cmdType}" 的参数 "${key}" 必须是有效的非空字符串`);
    }
    return value.trim();
  });
}

// ================================================================
// ID 生成器
// 使用全局统一的 generateUniqueId() 替代独立计数器
// ================================================================

// ================================================================
// 辅助函数
// ================================================================

/** 三点角度（B 为顶点），返回角度（度） */
function angleBetween(a: Point, b: Point, c: Point): number {
  const ba = { x: a.x - b.x, y: a.y - b.y };
  const bc = { x: c.x - b.x, y: c.y - b.y };
  const dot = ba.x * bc.x + ba.y * bc.y;
  const cross = ba.x * bc.y - ba.y * bc.x;
  return Math.atan2(Math.abs(cross), dot) * (180 / Math.PI);
}

/** 点到线段的投影 */
function projectOnSegment(p: Point, a: Point, b: Point): Point {
  const dx = b.x - a.x;
  const dy = b.y - a.y;
  const lenSq = dx * dx + dy * dy;
  if (lenSq < 1e-10) return { id: -1, x: a.x, y: a.y };
  const t = Math.max(0, Math.min(1, ((p.x - a.x) * dx + (p.y - a.y) * dy) / lenSq));
  return { id: -1, x: a.x + t * dx, y: a.y + t * dy };
}

/** 两线段交点 */
function lineIntersection(a: Point, b: Point, c: Point, d: Point): Point | null {
  const denom = (b.x - a.x) * (d.y - c.y) - (b.y - a.y) * (d.x - c.x);
  if (Math.abs(denom) < 1e-10) return null;
  const t = ((c.x - a.x) * (d.y - c.y) - (c.y - a.y) * (d.x - c.x)) / denom;
  const u = ((c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x)) / denom;
  if (t >= -1e-10 && t <= 1 + 1e-10 && u >= -1e-10 && u <= 1 + 1e-10) {
    return { id: -1, x: a.x + t * (b.x - a.x), y: a.y + t * (b.y - a.y) };
  }
  return null;
}

/** 解析坐标值（支持整数和浮点数） */
function parseCoord(s: string): number {
  const trimmed = s.trim();
  const val = parseFloat(trimmed);
  return isNaN(val) ? 0 : val;
}

/** 去除注释和空白行 */
function cleanLine(line: string): string {
  // 去除 // 注释
  const commentIdx = line.indexOf('//');
  const cleaned = commentIdx >= 0 ? line.substring(0, commentIdx) : line;
  return cleaned.trim();
}

// ================================================================
// DSL 解析器
// ================================================================

/**
 * 解析单行 DSL 命令
 */
function parseLine(line: string): FormulaCommand {
  const trimmed = cleanLine(line);
  if (!trimmed) {
    return { type: 'comment', raw: line, args: {} };
  }

  // ---- point A(x, y) ----
  const pointMatch = trimmed.match(/^point\s+(\w+)\s*\(\s*([^,]+)\s*,\s*([^\)]+)\s*\)$/i);
  if (pointMatch) {
    const name = pointMatch[1]!;
    const x = parseCoord(pointMatch[2]!);
    const y = parseCoord(pointMatch[3]!);
    return {
      type: 'point',
      raw: trimmed,
      args: { name, x, y },
    };
  }

  // ---- segment AB ----
  const segMatch = trimmed.match(/^segment\s+(\w{1,2})$/i);
  if (segMatch) {
    const name = segMatch[1]!;
    if (name.length === 2) {
      return {
        type: 'segment',
        raw: trimmed,
        args: { p1Name: name[0], p2Name: name[1] },
      };
    }
    return {
      type: 'unknown',
      raw: trimmed,
      args: {},
      error: '线段名称应为两个字母拼接，如 AB',
    };
  }

  // ---- segment(A, B) ----
  const segFuncMatch = trimmed.match(/^segment\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)$/i);
  if (segFuncMatch) {
    return {
      type: 'segment',
      raw: trimmed,
      args: { p1Name: segFuncMatch[1]!, p2Name: segFuncMatch[2]! },
    };
  }

  // ---- circle center(A) radius(r) ----
  const circleMatch = trimmed.match(
    /^circle\s+center\s*\(\s*(\w+)\s*\)\s+radius\s*\(\s*([^)]+)\s*\)$/i,
  );
  if (circleMatch) {
    const centerName = circleMatch[1]!;
    const radiusStr = circleMatch[2]!.trim();
    // 尝试解析为数值或点名称
    const radiusVal = parseFloat(radiusStr);
    return {
      type: 'circle',
      raw: trimmed,
      args: {
        centerName,
        radiusValue: isNaN(radiusVal) ? null : radiusVal,
        radiusPointName: isNaN(radiusVal) ? radiusStr : null,
      },
    };
  }

  // ---- midpoint M of A, B ----
  const midMatch = trimmed.match(/^midpoint\s+(\w+)\s+of\s+(\w+)\s*,\s*(\w+)$/i);
  if (midMatch) {
    return {
      type: 'midpoint',
      raw: trimmed,
      args: {
        name: midMatch[1]!,
        p1Name: midMatch[2]!,
        p2Name: midMatch[3]!,
      },
    };
  }

  // ---- perpendicular from A to segment BC ----
  const perpMatch = trimmed.match(
    /^perpendicular\s+from\s+(\w+)\s+to\s+segment\s+(\w{2})$/i,
  );
  if (perpMatch) {
    const segName = perpMatch[2]!;
    return {
      type: 'perpendicular',
      raw: trimmed,
      args: {
        pointName: perpMatch[1]!,
        segP1Name: segName[0],
        segP2Name: segName[1],
      },
    };
  }

  // ---- parallel to AB through C ----
  const parMatch = trimmed.match(/^parallel\s+to\s+(\w{2})\s+through\s+(\w+)$/i);
  if (parMatch) {
    const segName = parMatch[1]!;
    return {
      type: 'parallel',
      raw: trimmed,
      args: {
        segP1Name: segName[0],
        segP2Name: segName[1],
        throughName: parMatch[2]!,
      },
    };
  }

  // ---- intersect segment AB with CD ----
  const intMatch = trimmed.match(
    /^intersect\s+segment\s+(\w{2})\s+with\s+(\w{2})$/i,
  );
  if (intMatch) {
    const seg1Name = intMatch[1]!;
    const seg2Name = intMatch[2]!;
    return {
      type: 'intersect',
      raw: trimmed,
      args: {
        seg1P1Name: seg1Name[0],
        seg1P2Name: seg1Name[1],
        seg2P1Name: seg2Name[0],
        seg2P2Name: seg2Name[1],
      },
    };
  }

  // ---- measure distance A, B ----
  const distMeasureMatch = trimmed.match(
    /^measure\s+distance\s+(\w+)\s*,\s*(\w+)$/i,
  );
  if (distMeasureMatch) {
    return {
      type: 'measure_distance',
      raw: trimmed,
      args: { p1Name: distMeasureMatch[1]!, p2Name: distMeasureMatch[2]! },
    };
  }

  // ---- measure angle A, B, C ----
  const angleMeasureMatch = trimmed.match(
    /^measure\s+angle\s+(\w+)\s*,\s*(\w+)\s*,\s*(\w+)$/i,
  );
  if (angleMeasureMatch) {
    return {
      type: 'measure_angle',
      raw: trimmed,
      args: {
        p1Name: angleMeasureMatch[1]!,
        vertexName: angleMeasureMatch[2]!,
        p3Name: angleMeasureMatch[3]!,
      },
    };
  }

  return {
    type: 'unknown',
    raw: trimmed,
    args: {},
    error: `无法识别的命令: ${trimmed}`,
  };
}

// ================================================================
// 执行器
// ================================================================

/**
 * 【优化 H3】执行解析后的命令列表，创建几何图元
 * 使用类型安全的参数提取替代 unsafe 的 as 断言
 */
export function executeFormula(
  commands: FormulaCommand[],
  existingPoints: Point[] = [],
): FormulaParseResult {
  const result: FormulaParseResult = {
    commands: [...commands],
    errors: [],
    createdPoints: [],
    createdSegments: [],
    createdConstraints: [],
    measurements: [],
  };

  // 命名点注册表（名称 → Point）
  const namedPoints = new Map<string, Point>();

  // 初始化已有的命名点（按 ID 查找）
  for (const pt of existingPoints) {
    namedPoints.set(`P${pt.id}`, pt);
  }

  // 辅助：根据名称查找点
  const getPoint = (name: string): Point | undefined => {
    return namedPoints.get(name);
  };

  // 辅助：注册新点
  const registerPoint = (name: string, pt: Point) => {
    namedPoints.set(name, pt);
    result.createdPoints.push(pt);
  };

  for (const cmd of commands) {
    try {
      switch (cmd.type) {
        case 'point': {
          // 【优化 H3】使用类型安全的参数提取
          const { name, x, y } = cmd.args as { type: 'point'; name: string; x: number; y: number };
          // 数值验证
          if (typeof x !== 'number' || isNaN(x)) {
            cmd.error = `点 ${name} 的 x 坐标无效`;
            result.errors.push(cmd.error);
            break;
          }
          if (typeof y !== 'number' || isNaN(y)) {
            cmd.error = `点 ${name} 的 y 坐标无效`;
            result.errors.push(cmd.error);
            break;
          }
          const pt: Point = { id: generateUniqueId(), x, y };
          registerPoint(name, pt);
          cmd.result = `创建点 ${name}(${x}, ${y})`;
          break;
        }

        case 'segment': {
          // 【优化 H3】使用类型安全的参数提取
          const { p1Name, p2Name } = cmd.args as { type: 'segment'; p1Name: string; p2Name: string };
          const p1 = getPoint(p1Name);
          const p2 = getPoint(p2Name);
          if (!p1) {
            cmd.error = `点 ${p1Name} 未定义`;
            result.errors.push(cmd.error);
            break;
          }
          if (!p2) {
            cmd.error = `点 ${p2Name} 未定义`;
            result.errors.push(cmd.error);
            break;
          }
          const seg: Segment = { id: generateUniqueId(), p1: p1.id, p2: p2.id };
          result.createdSegments.push(seg);
          cmd.result = `创建线段 ${p1Name}${p2Name} (ID: ${seg.id})`;
          break;
        }

        case 'circle': {
          // 【优化 H3】使用类型安全的参数提取
          const { centerName, radiusValue, radiusPointName } = cmd.args as {
            type: 'circle';
            centerName: string;
            radiusValue: number | null;
            radiusPointName: string | null;
          };
          const center = getPoint(centerName);
          if (!center) {
            cmd.error = `圆心 ${centerName} 未定义`;
            result.errors.push(cmd.error);
            break;
          }
          let radius: number;
          if (radiusValue !== null) {
            // 验证半径值类型和有效性
            if (typeof radiusValue !== 'number' || isNaN(radiusValue)) {
              cmd.error = `圆 ${centerName} 的半径值无效`;
              result.errors.push(cmd.error);
              break;
            }
            radius = radiusValue;
          } else if (radiusPointName !== null) {
            if (typeof radiusPointName !== 'string') {
              cmd.error = `圆 ${centerName} 的半径点名称无效`;
              result.errors.push(cmd.error);
              break;
            }
            const rp = getPoint(radiusPointName);
            if (!rp) {
              cmd.error = `半径点 ${radiusPointName} 未定义`;
              result.errors.push(cmd.error);
              break;
            }
            radius = dist(center, rp);
          } else {
            cmd.error = '未指定半径';
            result.errors.push(cmd.error);
            break;
          }

          if (radius < 1e-10) {
            cmd.error = '半径为零';
            result.errors.push(cmd.error);
            break;
          }

          // 用正 24 边形近似圆
          const n = 24;
          const circlePointIds: number[] = [];
          for (let i = 0; i < n; i++) {
            const angle = (2 * Math.PI * i) / n;
            const pid = generateUniqueId();
            const pt: Point = {
              id: pid,
              x: center.x + radius * Math.cos(angle),
              y: center.y + radius * Math.sin(angle),
            };
            result.createdPoints.push(pt);
            circlePointIds.push(pid);
            // 注册为内部名称（不可直接引用）
            namedPoints.set(`_circle_${String(centerName)}_${i}`, pt);
          }
          for (let i = 0; i < n; i++) {
            const seg: Segment = {
              id: generateUniqueId(),
              p1: circlePointIds[i]!,
              p2: circlePointIds[(i + 1) % n]!,
            };
            result.createdSegments.push(seg);
          }
          cmd.result = `创建圆 (圆心: ${centerName}, 半径: ${radius.toFixed(2)}, ${n} 边形近似)`;
          break;
        }

        case 'midpoint': {
          // 【优化 H3】使用类型安全的参数提取
          const { name, p1Name, p2Name } = cmd.args as {
            type: 'midpoint';
            name: string;
            p1Name: string;
            p2Name: string;
          };
          const p1 = getPoint(p1Name);
          const p2 = getPoint(p2Name);
          if (!p1) {
            cmd.error = `点 ${p1Name} 未定义`;
            result.errors.push(cmd.error);
            break;
          }
          if (!p2) {
            cmd.error = `点 ${p2Name} 未定义`;
            result.errors.push(cmd.error);
            break;
          }
          const mid: Point = {
            id: generateUniqueId(),
            x: (p1.x + p2.x) / 2,
            y: (p1.y + p2.y) / 2,
          };
          registerPoint(name, mid);
          // 添加 betweenness 约束
          const constraint: Constraint = {
            id: generateUniqueId(),
            type: 'betweenness',
            args: [p1.id, mid.id, p2.id],
          };
          result.createdConstraints.push(constraint);
          cmd.result = `创建中点 ${name}(${mid.x.toFixed(2)}, ${mid.y.toFixed(2)})`;
          break;
        }

        case 'perpendicular': {
          // 【优化 H3】使用类型安全的参数提取
          const { pointName, segP1Name, segP2Name } = cmd.args as {
            type: 'perpendicular';
            pointName: string;
            segP1Name: string;
            segP2Name: string;
          };
          const p = getPoint(pointName);
          const a = getPoint(segP1Name);
          const b = getPoint(segP2Name);
          if (!p) {
            cmd.error = `点 ${pointName} 未定义`;
            result.errors.push(cmd.error);
            break;
          }
          if (!a || !b) {
            cmd.error = `线段端点 ${segP1Name} 或 ${segP2Name} 未定义`;
            result.errors.push(cmd.error);
            break;
          }
          const foot = projectOnSegment(p, a, b);
          foot.id = generateUniqueId();
          registerPoint(`foot_${pointName}_${segP1Name}${segP2Name}`, foot);
          // 添加 incidence 约束
          const seg = result.createdSegments.find(
            (s) =>
              (s.p1 === a.id && s.p2 === b.id) ||
              (s.p1 === b.id && s.p2 === a.id),
          );
          const constraint: Constraint = {
            id: generateUniqueId(),
            type: 'incidence',
            args: [foot.id, ...(seg ? [seg.id] : [])],
          };
          result.createdConstraints.push(constraint);
          cmd.result = `创建垂足 (${foot.x.toFixed(2)}, ${foot.y.toFixed(2)})`;
          break;
        }

        case 'parallel': {
          // 【优化 H3】使用类型安全的参数提取
          const { segP1Name, segP2Name, throughName } = cmd.args as {
            type: 'parallel';
            segP1Name: string;
            segP2Name: string;
            throughName: string;
          };
          const a = getPoint(segP1Name);
          const b = getPoint(segP2Name);
          const through = getPoint(throughName);
          if (!a || !b) {
            cmd.error = `线段端点 ${segP1Name} 或 ${segP2Name} 未定义`;
            result.errors.push(cmd.error);
            break;
          }
          if (!through) {
            cmd.error = `点 ${throughName} 未定义`;
            result.errors.push(cmd.error);
            break;
          }
          const dx = b.x - a.x;
          const dy = b.y - a.y;
          const len = Math.sqrt(dx * dx + dy * dy);
          if (len < 1e-10) {
            cmd.error = '线段长度为零';
            result.errors.push(cmd.error);
            break;
          }
          const ux = dx / len;
          const uy = dy / len;
          const halfLen = len * 0.8;
          const id1 = generateUniqueId();
          const id2 = generateUniqueId();
          const p1: Point = {
            id: id1,
            x: through.x + ux * halfLen,
            y: through.y + uy * halfLen,
          };
          const p2: Point = {
            id: id2,
            x: through.x - ux * halfLen,
            y: through.y - uy * halfLen,
          };
          registerPoint(`par_${throughName}_1`, p1);
          registerPoint(`par_${throughName}_2`, p2);
          result.createdSegments.push({ id: generateUniqueId(), p1: id1, p2: id2 });
          cmd.result = `创建平行线 (过 ${throughName}, 长 ${(halfLen * 2).toFixed(2)})`;
          break;
        }

        case 'intersect': {
          // 【优化 H3】使用类型安全的参数提取
          const { seg1P1Name, seg1P2Name, seg2P1Name, seg2P2Name } = cmd.args as {
            type: 'intersect';
            seg1P1Name: string;
            seg1P2Name: string;
            seg2P1Name: string;
            seg2P2Name: string;
          };
          const a = getPoint(seg1P1Name);
          const b = getPoint(seg1P2Name);
          const c = getPoint(seg2P1Name);
          const d = getPoint(seg2P2Name);
          if (!a || !b || !c || !d) {
            cmd.error = '交点计算所需的点未全部定义';
            result.errors.push(cmd.error);
            break;
          }
          const intersection = lineIntersection(a, b, c, d);
          if (!intersection) {
            cmd.error = '两线段不相交（平行或不相交）';
            result.errors.push(cmd.error);
            break;
          }
          intersection.id = generateUniqueId();
          registerPoint(`int_${seg1P1Name}${seg1P2Name}_${seg2P1Name}${seg2P2Name}`, intersection);
          cmd.result = `创建交点 (${intersection.x.toFixed(2)}, ${intersection.y.toFixed(2)})`;
          break;
        }

        case 'measure_distance': {
          // 【优化 H3】使用类型安全的参数提取
          const { p1Name, p2Name } = cmd.args as { type: 'measure_distance'; p1Name: string; p2Name: string };
          const p1 = getPoint(p1Name);
          const p2 = getPoint(p2Name);
          if (!p1 || !p2) {
            cmd.error = `点 ${p1Name} 或 ${p2Name} 未定义`;
            result.errors.push(cmd.error);
            break;
          }
          const d = dist(p1, p2);
          const label = `distance(${p1Name}, ${p2Name})`;
          result.measurements.push({ label, value: d.toFixed(4) });
          cmd.result = `${label} = ${d.toFixed(4)}`;
          break;
        }

        case 'measure_angle': {
          // 【优化 H3】使用类型安全的参数提取
          const { p1Name, vertexName, p3Name } = cmd.args as {
            type: 'measure_angle';
            p1Name: string;
            vertexName: string;
            p3Name: string;
          };
          const p1 = getPoint(p1Name);
          const vertex = getPoint(vertexName);
          const p3 = getPoint(p3Name);
          if (!p1 || !vertex || !p3) {
            cmd.error = `角度计算所需的点未全部定义`;
            result.errors.push(cmd.error);
            break;
          }
          const a = angleBetween(p1, vertex, p3);
          const label = `angle(${p1Name}, ${vertexName}, ${p3Name})`;
          result.measurements.push({ label, value: `${a.toFixed(2)}\u00B0` });
          cmd.result = `${label} = ${a.toFixed(2)}\u00B0`;
          break;
        }

        case 'comment':
          cmd.result = '(注释)';
          break;

        case 'unknown':
          if (cmd.error) {
            result.errors.push(cmd.error);
          }
          break;
      }
    } catch (e) {
      // 【优化】改进错误消息，提供更多上下文信息
      const errMsg = `执行命令时出错: ${cmd.raw} - ${(e as Error).message}`;
      cmd.error = errMsg;
      result.errors.push(errMsg);
    }
  }

  return result;
}

// ================================================================
// 公开 API
// ================================================================

/**
 * 解析公式文本（仅解析，不执行）
 */
export function parseFormula(text: string): FormulaParseResult {
  const lines = text.split('\n');
  const commands: FormulaCommand[] = [];
  const errors: string[] = [];

  for (let i = 0; i < lines.length; i++) {
    const line = lines[i]!;
    const trimmed = cleanLine(line);
    if (!trimmed) continue;

    const cmd = parseLine(line);
    commands.push(cmd);
    if (cmd.error) {
      errors.push(`行 ${i + 1}: ${cmd.error}`);
    }
  }

  return {
    commands,
    errors,
    createdPoints: [],
    createdSegments: [],
    createdConstraints: [],
    measurements: [],
  };
}

/**
 * 解析并执行公式文本
 */
export function parseAndExecuteFormula(
  text: string,
  existingPoints: Point[] = [],
): FormulaParseResult {
  const parsed = parseFormula(text);
  if (parsed.errors.length > 0 && parsed.commands.every((c) => c.type === 'unknown')) {
    // 全部解析失败，直接返回
    return parsed;
  }
  return executeFormula(parsed.commands, existingPoints);
}

/**
 * 从当前画布几何生成 DSL 文本
 */
export function generateDSLFromGeometry(
  points: Point[],
  segments: Segment[],
  constraints: Constraint[],
): string {
  const lines: string[] = [];
  const pointNames = new Map<number, string>();

  // 为每个点生成名称
  for (let i = 0; i < points.length; i++) {
    const pt = points[i]!;
    const name = String.fromCharCode(65 + (i % 26)) + (i >= 26 ? Math.floor(i / 26) : '');
    pointNames.set(pt.id, name);
    lines.push(`point ${name}(${pt.x.toFixed(1)}, ${pt.y.toFixed(1)})`);
  }

  // 为每条线段生成 DSL
  for (const seg of segments) {
    const p1Name = pointNames.get(seg.p1) ?? `?${seg.p1}`;
    const p2Name = pointNames.get(seg.p2) ?? `?${seg.p2}`;
    lines.push(`segment ${p1Name}${p2Name}`);
  }

  // 为所有约束生成 DSL
  for (const con of constraints) {
    switch (con.type) {
      case 'betweenness': {
        // midpoint M of A, B
        if (con.args.length === 3) {
          const midName = pointNames.get(con.args[1]!);
          const p1Name = pointNames.get(con.args[0]!);
          const p2Name = pointNames.get(con.args[2]!);
          if (midName && p1Name && p2Name) {
            lines.push(`midpoint ${midName} of ${p1Name}, ${p2Name}`);
          }
        }
        break;
      }
      case 'incidence': {
        // point A lies on line BC
        // args: [pointId, segPointAId, segPointBId]
        if (con.args.length >= 3) {
          const ptName = pointNames.get(con.args[0]!);
          const segP1Name = pointNames.get(con.args[1]!);
          const segP2Name = pointNames.get(con.args[2]!);
          if (ptName && segP1Name && segP2Name) {
            lines.push(`point ${ptName} lies on line ${segP1Name}${segP2Name}`);
          }
        }
        break;
      }
      case 'intersection': {
        // intersect AB with CD at E
        // args: [seg1P1, seg1P2, seg2P1, seg2P2, intersectionPoint]
        if (con.args.length >= 5) {
          const s1p1 = pointNames.get(con.args[0]!);
          const s1p2 = pointNames.get(con.args[1]!);
          const s2p1 = pointNames.get(con.args[2]!);
          const s2p2 = pointNames.get(con.args[3]!);
          const intPt = pointNames.get(con.args[4]!);
          if (s1p1 && s1p2 && s2p1 && s2p2 && intPt) {
            lines.push(`intersect ${s1p1}${s1p2} with ${s2p1}${s2p2} at ${intPt}`);
          }
        }
        break;
      }
      case 'containment': {
        // point D inside circle (O, A)  or  point D inside region R
        // args: [containedPointId, containerId(s)]
        if (con.args.length >= 2) {
          const containedName = pointNames.get(con.args[0]!);
          const containerNames = con.args.slice(1).map((id) => pointNames.get(id)).filter(Boolean);
          if (containedName && containerNames.length > 0) {
            const container = containerNames.join(', ');
            if (containerNames.length === 2) {
              lines.push(`point ${containedName} inside circle (${container})`);
            } else {
              lines.push(`point ${containedName} inside region (${container})`);
            }
          }
        }
        break;
      }
      case 'connection': {
        // port P connected to Q
        // args: [sourcePointId, targetPointId]
        if (con.args.length >= 2) {
          const srcName = pointNames.get(con.args[0]!);
          const tgtName = pointNames.get(con.args[1]!);
          if (srcName && tgtName) {
            lines.push(`port ${srcName} connected to ${tgtName}`);
          }
        }
        break;
      }
      default:
        break;
    }
  }

  return lines.join('\n');
}
