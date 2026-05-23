/**
 * @module utils/formulaParser
 * @description 公式 DSL 解析器 —— 将文本公式解析为几何操作并执行。
 *
 *              支持的 DSL 命令：
 *              - point A(x, y)         创建命名点
 *              - segment AB             创建线段（两点名称拼接）
 *              - segment(A, B)          创建线段（函数调用形式）
 *              - circle center(A) radius(r) 创建圆（多边形近似）
 *              - midpoint M of A, B     创建中点
 *              - perpendicular from A to segment BC  创建垂足
 *              - parallel to AB through C             创建平行线
 *              - intersect segment AB with CD         求交点
 *              - measure distance A, B                计算距离
 *              - measure angle A, B, C                计算角度
 *
 *              所有几何计算使用纯 JS 实现，不依赖 WASM 后端。
 */

import type { Point, Segment, Constraint } from '@/types';

// ================================================================
// 类型定义 / Type Definitions
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

/** 解析出的单条命令 */
export interface FormulaCommand {
  type: FormulaCommandType;
  raw: string;
  /** 命令参数（根据类型不同含义不同） */
  args: Record<string, unknown>;
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
// ID 生成器 / ID Generator
// ================================================================

let _formulaIdCounter = 2000;

function nextFormulaId(): number {
  return ++_formulaIdCounter;
}

export function resetFormulaIdCounter(): void {
  _formulaIdCounter = 2000;
}

// ================================================================
// 辅助函数 / Helper Functions
// ================================================================

/** 两点距离 */
function dist(a: Point, b: Point): number {
  return Math.sqrt((a.x - b.x) ** 2 + (a.y - b.y) ** 2);
}

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
// DSL 解析器 / DSL Parser
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
// 执行器 / Executor
// ================================================================

/**
 * 执行解析后的命令列表，创建几何图元
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
          const { name, x, y } = cmd.args as { name: string; x: number; y: number };
          const pt: Point = { id: nextFormulaId(), x, y };
          registerPoint(name, pt);
          cmd.result = `创建点 ${name}(${x}, ${y})`;
          break;
        }

        case 'segment': {
          const { p1Name, p2Name } = cmd.args as { p1Name: string; p2Name: string };
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
          const seg: Segment = { id: nextFormulaId(), p1: p1.id, p2: p2.id };
          result.createdSegments.push(seg);
          cmd.result = `创建线段 ${p1Name}${p2Name} (ID: ${seg.id})`;
          break;
        }

        case 'circle': {
          const { centerName, radiusValue, radiusPointName } = cmd.args as {
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
            radius = radiusValue;
          } else if (radiusPointName) {
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
            const pid = nextFormulaId();
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
              id: nextFormulaId(),
              p1: circlePointIds[i]!,
              p2: circlePointIds[(i + 1) % n]!,
            };
            result.createdSegments.push(seg);
          }
          cmd.result = `创建圆 (圆心: ${centerName}, 半径: ${radius.toFixed(2)}, ${n} 边形近似)`;
          break;
        }

        case 'midpoint': {
          const { name, p1Name, p2Name } = cmd.args as {
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
            id: nextFormulaId(),
            x: (p1.x + p2.x) / 2,
            y: (p1.y + p2.y) / 2,
          };
          registerPoint(name, mid);
          // 添加 betweenness 约束
          const constraint: Constraint = {
            id: nextFormulaId(),
            type: 'betweenness',
            args: [p1.id, mid.id, p2.id],
          };
          result.createdConstraints.push(constraint);
          cmd.result = `创建中点 ${name}(${mid.x.toFixed(2)}, ${mid.y.toFixed(2)})`;
          break;
        }

        case 'perpendicular': {
          const { pointName, segP1Name, segP2Name } = cmd.args as {
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
          foot.id = nextFormulaId();
          registerPoint(`foot_${pointName}_${segP1Name}${segP2Name}`, foot);
          // 添加 incidence 约束
          const seg = result.createdSegments.find(
            (s) =>
              (s.p1 === a.id && s.p2 === b.id) ||
              (s.p1 === b.id && s.p2 === a.id),
          );
          const constraint: Constraint = {
            id: nextFormulaId(),
            type: 'incidence',
            args: [foot.id, ...(seg ? [seg.id] : [])],
          };
          result.createdConstraints.push(constraint);
          cmd.result = `创建垂足 (${foot.x.toFixed(2)}, ${foot.y.toFixed(2)})`;
          break;
        }

        case 'parallel': {
          const { segP1Name, segP2Name, throughName } = cmd.args as {
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
          const id1 = nextFormulaId();
          const id2 = nextFormulaId();
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
          result.createdSegments.push({ id: nextFormulaId(), p1: id1, p2: id2 });
          cmd.result = `创建平行线 (过 ${throughName}, 长 ${(halfLen * 2).toFixed(2)})`;
          break;
        }

        case 'intersect': {
          const { seg1P1Name, seg1P2Name, seg2P1Name, seg2P2Name } = cmd.args as {
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
          intersection.id = nextFormulaId();
          registerPoint(`int_${seg1P1Name}${seg1P2Name}_${seg2P1Name}${seg2P2Name}`, intersection);
          cmd.result = `创建交点 (${intersection.x.toFixed(2)}, ${intersection.y.toFixed(2)})`;
          break;
        }

        case 'measure_distance': {
          const { p1Name, p2Name } = cmd.args as { p1Name: string; p2Name: string };
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
          const { p1Name, vertexName, p3Name } = cmd.args as {
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
      const errMsg = `执行命令时出错: ${cmd.raw} - ${(e as Error).message}`;
      cmd.error = errMsg;
      result.errors.push(errMsg);
    }
  }

  return result;
}

// ================================================================
// 公开 API / Public API
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

  // 为 betweenness 约束生成 midpoint DSL
  for (const con of constraints) {
    if (con.type === 'betweenness' && con.args.length === 3) {
      const midName = pointNames.get(con.args[1]!);
      const p1Name = pointNames.get(con.args[0]!);
      const p2Name = pointNames.get(con.args[2]!);
      if (midName && p1Name && p2Name) {
        lines.push(`midpoint ${midName} of ${p1Name}, ${p2Name}`);
      }
    }
  }

  return lines.join('\n');
}
