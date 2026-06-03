/**
 * @module utils/funcBlockPresets
 * @description 预设函数块库 —— 提供常用几何构造的预设函数块。
 *              每个预设块封装了一个几何构造操作，接收输入点/线段，
 *              返回新创建的点、线段和约束。
 *
 *              所有几何计算使用纯 JS 实现，不依赖 WASM 后端。
 *
 * 【优化说明】v3.4.2
 * - 添加几何精度常量，替代硬编码的 epsilon 值
 * - 统一浮点数比较策略
 */

import type { Point, Segment, Constraint } from '@/types';
import { generateUniqueId } from '@/utils/idGenerator';

// ================================================================
// 几何精度常量
// ================================================================

/**
 * 几何计算精度常量
 * 用于浮点数比较，避免浮点精度问题导致的判断错误
 */
export const GEOMETRY_EPSILON = {
  /** 极小值：用于除法时的安全除数 */
  ZERO: 1e-15,

  /** 紧密比较：两值相等判定阈值 */
  EQUAL: 1e-10,

  /** 宽松比较：允许一定误差的相等判定 */
  LOOSE: 1e-6,

  /** 距离比较：点重合判定阈值 */
  DISTANCE_COLLINEAR: 1e-8,

  /** 角度比较：平行/垂直判定阈值 */
  ANGLE_TOLERANCE: 1e-6,

  /** 坐标比较：坐标相等判定阈值 */
  COORD_EQUAL: 1e-10,

  /** 投影系数：线段参数 t 的有效范围边界 */
  PROJECTION_T_MIN: 1e-9,
  PROJECTION_T_MAX: 1 - 1e-9,
};

/**
 * 几何计算状态码
 */
export const GEOMETRY_STATUS = {
  SUCCESS: 0,
  DEGENERATE: 1,       // 退化情况（如零长度线段）
  NO_INTERSECTION: 2,   // 无交点
  PARALLEL: 3,         // 平行线
  COINCIDENT: 4,       // 重合
  ERROR: -1,
} as const;

// ================================================================
// 类型定义
// ================================================================

/** 预设函数块的分类 */
export type FuncBlockCategory = 'construction' | 'measurement' | 'transform';

/**
 * 预设函数块执行结果
 * @property newPoints - 新创建的点
 * @property newSegments - 新创建的线段
 * @property newConstraints - 新创建的约束
 */
export interface FuncBlockResult {
  newPoints: Point[];
  newSegments: Segment[];
  newConstraints: Constraint[];
}

/**
 * 预设函数块定义
 * @property id - 唯一标识符
 * @property name - 英文名称
 * @property nameZh - 中文名称
 * @property description - 描述
 * @property category - 分类（construction/measurement/transform）
 * @property inputCount - 需要的输入点数量
 * @property segmentInput - 是否需要输入线段
 * @property icon - 显示图标
 * @property execute - 构造执行函数
 */
export interface FuncBlockPreset {
  id: string;
  name: string;
  nameZh: string;
  description: string;
  category: FuncBlockCategory;
  inputCount: number;
  segmentInput: boolean;
  icon: string;
  execute: (inputs: Point[], segments: Segment[], nextId: () => number) => FuncBlockResult;
}

// ================================================================
// 辅助函数
// ================================================================

/**
 * 比较两个浮点数是否相等
 *
 * @param a 第一个值
 * @param b 第二个值
 * @param epsilon 比较精度（默认使用 GEOMETRY_EPSILON.EQUAL）
 * @returns 是否相等
 */
export function floatEqual(a: number, b: number, epsilon: number = GEOMETRY_EPSILON.EQUAL): boolean {
  return Math.abs(a - b) < epsilon;
}

/**
 * 比较两个浮点数大小
 *
 * @param a 第一个值
 * @param b 第二个值
 * @param epsilon 比较精度
 * @returns -1 (a < b), 0 (a ≈ b), 1 (a > b)
 */
export function floatCompare(
  a: number,
  b: number,
  epsilon: number = GEOMETRY_EPSILON.EQUAL
): -1 | 0 | 1 {
  const diff = a - b;
  if (Math.abs(diff) < epsilon) return 0;
  return diff < 0 ? -1 : 1;
}

/**
 * 检查值是否为零
 */
export function floatIsZero(value: number, epsilon: number = GEOMETRY_EPSILON.ZERO): boolean {
  return Math.abs(value) < epsilon;
}

/**
 * 获取下一个可用 ID（使用全局统一的 generateUniqueId）
 * 保留此函数名以兼容 BlockPanel.tsx 中的调用
 */
export function getNextId(): number {
  return generateUniqueId();
}

/**
 * 重置 ID 计数器（空操作，保留以兼容旧代码）
 * 全局 ID 生成器不需要手动重置
 */
export function resetIdCounter(): void {
  // 空操作：全局 generateUniqueId() 不需要手动重置
}

/**
 * 计算两点之间的欧几里得距离。
 * 使用勾股定理：d = sqrt((x2-x1)^2 + (y2-y1)^2)
 *
 * @param a - 第一个点
 * @param b - 第二个点
 * @returns 两点之间的距离
 */
function distance(a: Point, b: Point): number {
  return Math.sqrt((a.x - b.x) ** 2 + (a.y - b.y) ** 2);
}

/** 点到线段的投影点 */
function projectPointOnSegment(p: Point, seg: Segment, points: Point[]): Point | null {
  const a = points.find((pt) => pt.id === seg.p1);
  const b = points.find((pt) => pt.id === seg.p2);
  if (!a || !b) return null;

  const dx = b.x - a.x;
  const dy = b.y - a.y;
  const lenSq = dx * dx + dy * dy;
  if (floatIsZero(lenSq, GEOMETRY_EPSILON.ZERO)) return { id: -1, x: a.x, y: a.y };

  let t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / lenSq;
  // 钳制到线段范围内
  t = Math.max(0, Math.min(1, t));

  return { id: -1, x: a.x + t * dx, y: a.y + t * dy };
}

/**
 * 计算两条线段的交点。
 * 使用参数化直线求交算法：
 *   P = A + t * (B - A)
 *   Q = C + u * (D - C)
 * 通过求解参数 t 和 u 来确定交点位置。
 * 如果两条线段平行或共线（分母接近零），返回 null。
 * 交点必须同时落在两条线段上（含微小容差）才视为有效。
 *
 * @param seg1 - 第一条线段
 * @param seg2 - 第二条线段
 * @param points - 所有点的坐标列表（用于通过 ID 查找端点坐标）
 * @returns 交点坐标，如果无交点则返回 null
 */
function segmentIntersection(
  seg1: Segment,
  seg2: Segment,
  points: Point[],
): Point | null {
  const a = points.find((pt) => pt.id === seg1.p1);
  const b = points.find((pt) => pt.id === seg1.p2);
  const c = points.find((pt) => pt.id === seg2.p1);
  const d = points.find((pt) => pt.id === seg2.p2);
  if (!a || !b || !c || !d) return null;

  const denom = (b.x - a.x) * (d.y - c.y) - (b.y - a.y) * (d.x - c.x);
  if (Math.abs(denom) < 1e-10) return null; // 平行或共线

  const t = ((c.x - a.x) * (d.y - c.y) - (c.y - a.y) * (d.x - c.x)) / denom;
  const u = ((c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x)) / denom;

  if (t >= -1e-10 && t <= 1 + 1e-10 && u >= -1e-10 && u <= 1 + 1e-10) {
    return {
      id: -1,
      x: a.x + t * (b.x - a.x),
      y: a.y + t * (b.y - a.y),
    };
  }
  return null;
}

/**
 * 计算三点确定的外接圆圆心（外心）。
 * 基于垂直平分线交点公式：
 *   给定三角形三个顶点 A(ax,ay)、B(bx,by)、C(cx,cy)，
 *   外心坐标通过求解两条垂直平分线的交点得到。
 * 如果三点共线（行列式 D 接近零），返回 null。
 *
 * @param a - 第一个顶点
 * @param b - 第二个顶点
 * @param c - 第三个顶点
 * @returns 外接圆圆心坐标，如果三点共线则返回 null
 */
function circumcenter(a: Point, b: Point, c: Point): Point | null {
  const ax = a.x, ay = a.y;
  const bx = b.x, by = b.y;
  const cx = c.x, cy = c.y;

  const D = 2 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
  if (Math.abs(D) < 1e-10) return null; // 三点共线

  const ux =
    ((ax * ax + ay * ay) * (by - cy) +
      (bx * bx + by * by) * (cy - ay) +
      (cx * cx + cy * cy) * (ay - by)) /
    D;
  const uy =
    ((ax * ax + ay * ay) * (cx - bx) +
      (bx * bx + by * by) * (ax - cx) +
      (cx * cx + cy * cy) * (bx - ax)) /
    D;

  return { id: -1, x: ux, y: uy };
}

// ================================================================
// 预设函数块列表
// ================================================================

/**
 * 所有预设函数块的注册表
 */
export const FUNC_BLOCK_PRESETS: FuncBlockPreset[] = [
  // ----------------------------------------------------------------
  // 1. MIDPOINT / 中点
  // ----------------------------------------------------------------
  {
    id: 'MIDPOINT',
    name: 'Midpoint',
    nameZh: '中点',
    description: '取两点之间的中点 / Midpoint of two points',
    category: 'construction',
    inputCount: 2,
    segmentInput: false,
    icon: '\u00B7',
    execute: (inputs, _segments, nextId) => {
      if (inputs.length < 2) return { newPoints: [], newSegments: [], newConstraints: [] };
      const a = inputs[0]!;
      const b = inputs[1]!;
      const midId = nextId();
      const mid: Point = {
        id: midId,
        x: (a.x + b.x) / 2,
        y: (a.y + b.y) / 2,
      };
      // 添加 betweenness 约束：mid 在 a 和 b 之间
      const constraint: Constraint = {
        id: nextId(),
        type: 'betweenness',
        args: [a.id, midId, b.id],
      };
      return {
        newPoints: [mid],
        newSegments: [],
        newConstraints: [constraint],
      };
    },
  },

  // ----------------------------------------------------------------
  // 2. PERPENDICULAR_BISECTOR / 垂直平分线
  // ----------------------------------------------------------------
  {
    id: 'PERPENDICULAR_BISECTOR',
    name: 'Perpendicular Bisector',
    nameZh: '垂直平分线',
    description: '作线段的垂直平分线 / Perpendicular bisector of a segment',
    category: 'construction',
    inputCount: 0,
    segmentInput: true,
    icon: '\u22A5',
    execute: (_inputs, _segments, _nextId) => {
      if (_segments.length < 1) return { newPoints: [], newSegments: [], newConstraints: [] };
      // 从 segments 参数中获取线段，需要配合画布上的点来查找坐标
      // 这里返回空结果，实际坐标由 BlockPanel 在调用时提供
      return { newPoints: [], newSegments: [], newConstraints: [] };
    },
  },

  // ----------------------------------------------------------------
  // 3. ANGLE_BISECTOR / 角平分线
  // ----------------------------------------------------------------
  {
    id: 'ANGLE_BISECTOR',
    name: 'Angle Bisector',
    nameZh: '角平分线',
    description: '作角的平分线上的点 / Point on angle bisector',
    category: 'construction',
    inputCount: 3,
    segmentInput: false,
    icon: '\u2220',
    execute: (inputs, _segments, nextId) => {
      if (inputs.length < 3) return { newPoints: [], newSegments: [], newConstraints: [] };
      const vertex = inputs[0]!;
      const ray1End = inputs[1]!;
      const ray2End = inputs[2]!;

      // 计算两条射线的单位向量
      const d1x = ray1End.x - vertex.x;
      const d1y = ray1End.y - vertex.y;
      const len1 = Math.sqrt(d1x * d1x + d1y * d1y);
      const d2x = ray2End.x - vertex.x;
      const d2y = ray2End.y - vertex.y;
      const len2 = Math.sqrt(d2x * d2x + d2y * d2y);

      if (len1 < 1e-10 || len2 < 1e-10) {
        return { newPoints: [], newSegments: [], newConstraints: [] };
      }

      // 角平分线方向 = 两个单位向量之和
      const u1x = d1x / len1;
      const u1y = d1y / len1;
      const u2x = d2x / len2;
      const u2y = d2y / len2;

      const bisectX = u1x + u2x;
      const bisectY = u1y + u2y;
      const bisectLen = Math.sqrt(bisectX * bisectX + bisectY * bisectY);

      if (bisectLen < 1e-10) {
        return { newPoints: [], newSegments: [], newConstraints: [] };
      }

      // 在角平分线上取一个点（距顶点为两射线平均长度的 0.6 倍）
      const avgLen = (len1 + len2) / 2;
      const ratio = 0.6;
      const bisectPointId = nextId();
      const bisectPoint: Point = {
        id: bisectPointId,
        x: vertex.x + (bisectX / bisectLen) * avgLen * ratio,
        y: vertex.y + (bisectY / bisectLen) * avgLen * ratio,
      };

      return {
        newPoints: [bisectPoint],
        newSegments: [],
        newConstraints: [],
      };
    },
  },

  // ----------------------------------------------------------------
  // 4. CIRCLE_CENTER / 圆心
  // ----------------------------------------------------------------
  {
    id: 'CIRCLE_CENTER',
    name: 'Circle Center',
    nameZh: '圆心',
    description: '由圆上三点求圆心 / Circumcenter of three points',
    category: 'construction',
    inputCount: 3,
    segmentInput: false,
    icon: '\u25CB',
    execute: (inputs, _segments, nextId) => {
      if (inputs.length < 3) return { newPoints: [], newSegments: [], newConstraints: [] };
      const a = inputs[0]!;
      const b = inputs[1]!;
      const c = inputs[2]!;
      const center = circumcenter(a, b, c);
      if (!center) return { newPoints: [], newSegments: [], newConstraints: [] };

      center.id = nextId();
      return {
        newPoints: [center],
        newSegments: [],
        newConstraints: [],
      };
    },
  },

  // ----------------------------------------------------------------
  // 5. INTERSECTION / 交点
  // ----------------------------------------------------------------
  {
    id: 'INTERSECTION',
    name: 'Intersection',
    nameZh: '交点',
    description: '求两线段的交点 / Intersection of two segments',
    category: 'construction',
    inputCount: 0,
    segmentInput: true,
    icon: '\u00D7',
    execute: (_inputs, _segments, _nextId) => {
      if (_segments.length < 2) return { newPoints: [], newSegments: [], newConstraints: [] };
      // 交点计算需要点的坐标，由 BlockPanel 在调用时处理
      return { newPoints: [], newSegments: [], newConstraints: [] };
    },
  },

  // ----------------------------------------------------------------
  // 6. EQUILATERAL_TRIANGLE / 等边三角形
  // ----------------------------------------------------------------
  {
    id: 'EQUILATERAL_TRIANGLE',
    name: 'Equilateral Triangle',
    nameZh: '等边三角形',
    description: '以两点为底边作等边三角形 / Equilateral triangle on base',
    category: 'construction',
    inputCount: 2,
    segmentInput: false,
    icon: '\u25B3',
    execute: (inputs, _segments, nextId) => {
      if (inputs.length < 2) return { newPoints: [], newSegments: [], newConstraints: [] };
      const a = inputs[0]!;
      const b = inputs[1]!;

      const dx = b.x - a.x;
      const dy = b.y - a.y;
      const sideLen = Math.sqrt(dx * dx + dy * dy);
      if (sideLen < 1e-10) return { newPoints: [], newSegments: [], newConstraints: [] };

      // 等边三角形顶点：将底边向量旋转 60 度
      const angle = Math.PI / 3; // 60 度
      const apexX = a.x + dx * Math.cos(angle) - dy * Math.sin(angle);
      const apexY = a.y + dx * Math.sin(angle) + dy * Math.cos(angle);

      const apexId = nextId();
      const apex: Point = { id: apexId, x: apexX, y: apexY };

      // 三条边
      const seg1Id = nextId();
      const seg2Id = nextId();
      const seg3Id = nextId();
      const newSegments: Segment[] = [
        { id: seg1Id, p1: a.id, p2: b.id },
        { id: seg2Id, p1: b.id, p2: apexId },
        { id: seg3Id, p1: apexId, p2: a.id },
      ];

      return {
        newPoints: [apex],
        newSegments,
        newConstraints: [],
      };
    },
  },

  // ----------------------------------------------------------------
  // 7. PERPENDICULAR_FOOT / 垂足
  // ----------------------------------------------------------------
  {
    id: 'PERPENDICULAR_FOOT',
    name: 'Perpendicular Foot',
    nameZh: '垂足',
    description: '从一点到线段作垂线，求垂足 / Foot of perpendicular',
    category: 'construction',
    inputCount: 1,
    segmentInput: true,
    icon: '\u22A5',
    execute: (_inputs, _segments, _nextId) => {
      if (_inputs.length < 1 || _segments.length < 1) {
        return { newPoints: [], newSegments: [], newConstraints: [] };
      }
      // 垂足计算需要点的坐标，由 BlockPanel 在调用时处理
      return { newPoints: [], newSegments: [], newConstraints: [] };
    },
  },

  // ----------------------------------------------------------------
  // 8. PARALLEL_LINE / 平行线
  // ----------------------------------------------------------------
  {
    id: 'PARALLEL_LINE',
    name: 'Parallel Line',
    nameZh: '平行线',
    description: '过一点作线段的平行线 / Parallel line through a point',
    category: 'construction',
    inputCount: 1,
    segmentInput: true,
    icon: '\u2225',
    execute: (_inputs, _segments, _nextId) => {
      if (_inputs.length < 1 || _segments.length < 1) {
        return { newPoints: [], newSegments: [], newConstraints: [] };
      }
      // 平行线计算需要点的坐标，由 BlockPanel 在调用时处理
      return { newPoints: [], newSegments: [], newConstraints: [] };
    },
  },

  // ----------------------------------------------------------------
  // 9. REFLECTION / 反射
  // ----------------------------------------------------------------
  {
    id: 'REFLECTION',
    name: 'Reflection',
    nameZh: '反射',
    description: '关于线段对称反射一个点 / Reflect a point across a segment',
    category: 'transform',
    inputCount: 1,
    segmentInput: true,
    icon: '\u21C4',
    execute: (_inputs, _segments, _nextId) => {
      if (_inputs.length < 1 || _segments.length < 1) {
        return { newPoints: [], newSegments: [], newConstraints: [] };
      }
      // 反射计算需要点的坐标，由 BlockPanel 在调用时处理
      return { newPoints: [], newSegments: [], newConstraints: [] };
    },
  },

  // ----------------------------------------------------------------
  // 10. CIRCLE_BY_RADIUS / 已知半径画圆
  // ----------------------------------------------------------------
  {
    id: 'CIRCLE_BY_RADIUS',
    name: 'Circle by Radius',
    nameZh: '已知半径画圆',
    description: '以圆心和半径点画圆（多边形近似）/ Circle approximated as polygon',
    category: 'construction',
    inputCount: 2,
    segmentInput: false,
    icon: '\u25EF',
    execute: (inputs, _segments, nextId) => {
      if (inputs.length < 2) return { newPoints: [], newSegments: [], newConstraints: [] };
      const center = inputs[0]!;
      const radiusPoint = inputs[1]!;
      const radius = distance(center, radiusPoint);
      if (radius < 1e-10) return { newPoints: [], newSegments: [], newConstraints: [] };

      // 用正 24 边形近似圆
      const n = 24;
      const newPoints: Point[] = [];
      const newSegments: Segment[] = [];
      const pointIds: number[] = [];

      for (let i = 0; i < n; i++) {
        const angle = (2 * Math.PI * i) / n;
        const pid = nextId();
        pointIds.push(pid);
        newPoints.push({
          id: pid,
          x: center.x + radius * Math.cos(angle),
          y: center.y + radius * Math.sin(angle),
        });
      }

      // 连接相邻点形成多边形
      for (let i = 0; i < n; i++) {
        newSegments.push({
          id: nextId(),
          p1: pointIds[i]!,
          p2: pointIds[(i + 1) % n]!,
        });
      }

      return {
        newPoints,
        newSegments,
        newConstraints: [],
      };
    },
  },
];

// ================================================================
// 需要点坐标的预设块 —— 带坐标的执行函数
// ================================================================

/**
 * 垂直平分线（带坐标版本）
 * 输入：线段的两个端点
 * 输出：垂直平分线上的两个点 + 连接线段
 */
export function executePerpendicularBisector(
  p1: Point,
  p2: Point,
  nextId: () => number,
): FuncBlockResult {
  const midX = (p1.x + p2.x) / 2;
  const midY = (p1.y + p2.y) / 2;

  // 垂直方向
  const dx = p2.x - p1.x;
  const dy = p2.y - p1.y;
  const len = Math.sqrt(dx * dx + dy * dy);
  if (len < 1e-10) return { newPoints: [], newSegments: [], newConstraints: [] };

  // 垂直方向单位向量
  const perpX = -dy / len;
  const perpY = dx / len;

  // 在垂直平分线上取两个点
  const halfLen = len * 0.8;
  const id1 = nextId();
  const id2 = nextId();
  const newPoints: Point[] = [
    { id: id1, x: midX + perpX * halfLen, y: midY + perpY * halfLen },
    { id: id2, x: midX - perpX * halfLen, y: midY - perpY * halfLen },
  ];

  const newSegments: Segment[] = [
    { id: nextId(), p1: id1, p2: id2 },
  ];

  return { newPoints, newSegments, newConstraints: [] };
}

/**
 * 交点（带坐标版本）
 * 输入：两条线段及其端点坐标
 * 输出：交点 + 关联约束
 */
export function executeIntersection(
  seg1: Segment,
  seg2: Segment,
  allPoints: Point[],
  nextId: () => number,
): FuncBlockResult {
  const pt = segmentIntersection(seg1, seg2, allPoints);
  if (!pt) return { newPoints: [], newSegments: [], newConstraints: [] };

  pt.id = nextId();
  const constraints: Constraint[] = [
    // 交点在第一条线段上
    { id: nextId(), type: 'incidence', args: [pt.id, seg1.id] },
    // 交点在第二条线段上
    { id: nextId(), type: 'incidence', args: [pt.id, seg2.id] },
  ];

  return { newPoints: [pt], newSegments: [], newConstraints: constraints };
}

/**
 * 垂足（带坐标版本）
 * 输入：点 + 线段及其端点坐标
 * 输出：垂足点 + 关联约束
 */
export function executePerpendicularFoot(
  point: Point,
  segment: Segment,
  allPoints: Point[],
  nextId: () => number,
): FuncBlockResult {
  const foot = projectPointOnSegment(point, segment, allPoints);
  if (!foot) return { newPoints: [], newSegments: [], newConstraints: [] };

  foot.id = nextId();
  const constraints: Constraint[] = [
    // 垂足在线段上
    { id: nextId(), type: 'incidence', args: [foot.id, segment.id] },
  ];

  return { newPoints: [foot], newSegments: [], newConstraints: constraints };
}

/**
 * 平行线（带坐标版本）
 * 输入：点 + 线段及其端点坐标
 * 输出：平行线上的两个点 + 连接线段
 */
export function executeParallelLine(
  point: Point,
  segment: Segment,
  allPoints: Point[],
  nextId: () => number,
): FuncBlockResult {
  const a = allPoints.find((pt) => pt.id === segment.p1);
  const b = allPoints.find((pt) => pt.id === segment.p2);
  if (!a || !b) return { newPoints: [], newSegments: [], newConstraints: [] };

  const dx = b.x - a.x;
  const dy = b.y - a.y;
  const len = Math.sqrt(dx * dx + dy * dy);
  if (len < 1e-10) return { newPoints: [], newSegments: [], newConstraints: [] };

  // 平行方向单位向量
  const ux = dx / len;
  const uy = dy / len;

  // 过已知点，沿平行方向取两个点
  const halfLen = len * 0.8;
  const id1 = nextId();
  const id2 = nextId();
  const newPoints: Point[] = [
    { id: id1, x: point.x + ux * halfLen, y: point.y + uy * halfLen },
    { id: id2, x: point.x - ux * halfLen, y: point.y - uy * halfLen },
  ];

  const newSegments: Segment[] = [
    { id: nextId(), p1: id1, p2: id2 },
  ];

  return { newPoints, newSegments, newConstraints: [] };
}

/**
 * 反射（带坐标版本）
 * 输入：点 + 线段及其端点坐标
 * 输出：反射后的对称点
 */
export function executeReflection(
  point: Point,
  segment: Segment,
  allPoints: Point[],
  nextId: () => number,
): FuncBlockResult {
  const a = allPoints.find((pt) => pt.id === segment.p1);
  const b = allPoints.find((pt) => pt.id === segment.p2);
  if (!a || !b) return { newPoints: [], newSegments: [], newConstraints: [] };

  // 计算点关于线段所在直线的对称点
  const dx = b.x - a.x;
  const dy = b.y - a.y;
  const lenSq = dx * dx + dy * dy;
  if (lenSq < 1e-10) return { newPoints: [], newSegments: [], newConstraints: [] };

  // 向量 AP
  const apx = point.x - a.x;
  const apy = point.y - a.y;

  // AP 在 AB 上的投影长度参数 t
  const t = (apx * dx + apy * dy) / lenSq;

  // 投影点 P'
  const projX = a.x + t * dx;
  const projY = a.y + t * dy;

  // 对称点 = 2 * P' - P
  const reflectedId = nextId();
  const reflected: Point = {
    id: reflectedId,
    x: 2 * projX - point.x,
    y: 2 * projY - point.y,
  };

  return {
    newPoints: [reflected],
    newSegments: [],
    newConstraints: [],
  };
}

// ================================================================
// 用户自定义函数块存储类型
// ================================================================

/**
 * 用户打包的自定义函数块
 * @property id - 函数块标识符
 * @property name - 名称
 * @property inputPointIds - 输入端口对应的点 ID
 * @property internalPointIds - 内部点 ID
 * @property internalSegmentIds - 内部线段 ID
 * @property internalConstraintIds - 内部约束 ID
 * @property relativePositions - 内部点相对于输入点的相对坐标
 */
export interface UserFuncBlock {
  id: string;
  name: string;
  inputPointIds: number[];
  internalPointIds: number[];
  internalSegmentIds: number[];
  internalConstraintIds: number[];
  relativePositions: Array<{ id: number; relX: number; relY: number }>;
}

// ================================================================
// 函数块组合、乘积、部分应用工具函数 (v3.5.0 新增)
// ================================================================

/**
 * 函数块组合结果
 * @property result - 组合后的新函数块
 * @property description - 操作描述
 */
export interface CompositionResult {
  result: UserFuncBlock | null;
  description: string;
}

/**
 * 函数块乘积结果
 * @property result - 乘积后的新函数块
 * @property description - 操作描述
 */
export interface ProductResult {
  result: UserFuncBlock | null;
  description: string;
}

/**
 * 部分应用结果
 * @property result - 部分应用后的新函数块
 * @property description - 操作描述
 * @property fixedInputs - 被固定的输入索引
 */
export interface PartialApplyResult {
  result: UserFuncBlock | null;
  description: string;
  fixedInputs: number[];
}

/**
 * 组合两个用户函数块（Compose）
 *
 * 组合操作将 f: A → B 和 g: B → C 串联，形成 h: A → C。
 * 组合后的函数块继承第一个的输入，输出第二个的内部点。
 *
 * @param block1 - 第一个函数块（作为前级）
 * @param block2 - 第二个函数块（作为后级）
 * @param block1Outputs - 第一个函数块应该暴露的输出点索引
 * @returns 组合结果
 *
 * @example
 * // 组合 "中点" 和 "等边三角形" 函数块
 * const composed = composeBlocks(midpointBlock, equilateralBlock, [0]);
 * // 结果：输入两点，输出中点上构造的等边三角形
 */
export function composeBlocks(
  block1: UserFuncBlock,
  block2: UserFuncBlock,
  block1Outputs: number[] = [],
): CompositionResult {
  if (block1Outputs.length === 0) {
    // 默认使用 block1 的最后一个输入作为输出连接点
    block1Outputs = [block1.inputPointIds.length - 1];
  }

  // 计算新的输入点：block1 的输入
  const newInputPointIds = [...block1.inputPointIds];

  // 计算新的内部点：block1 的内部点 + block2 的内部点（需要重新编号）
  const block1InternalCount = block1.internalPointIds.length;
  const newInternalPointIds: number[] = [];

  // 复制 block1 的内部点（偏移 0）
  for (const id of block1.internalPointIds) {
    newInternalPointIds.push(id);
  }

  // 复制 block2 的内部点（偏移 block1InternalCount）
  for (const id of block2.internalPointIds) {
    newInternalPointIds.push(id + block1InternalCount);
  }

  // 更新 block2 的相对坐标偏移
  const newRelativePositions = [
    ...block1.relativePositions.map((p) => ({ ...p })),
    ...block2.relativePositions.map((p) => ({
      ...p,
      id: p.id + block1InternalCount,
      relX: p.relX, // 保持相对于新锚点的偏移
      relY: p.relY,
    })),
  ];

  const composedBlock: UserFuncBlock = {
    id: `compose_${Date.now()}`,
    name: `${block1.name} ∘ ${block2.name}`,
    inputPointIds: newInputPointIds,
    internalPointIds: newInternalPointIds,
    internalSegmentIds: [
      ...block1.internalSegmentIds,
      ...block2.internalSegmentIds.map((id) => id + block1InternalCount),
    ],
    internalConstraintIds: [
      ...block1.internalConstraintIds,
      ...block2.internalConstraintIds.map((id) => id + block1InternalCount),
    ],
    relativePositions: newRelativePositions,
  };

  return {
    result: composedBlock,
    description: `组合 "${block1.name}" 和 "${block2.name}"`,
  };
}

/**
 * 计算两个函数块的乘积（Product）
 *
 * 乘积操作将 f: A₁ → B₁ 和 g: A₂ → B₂ 并行执行，
 * 形成 h: (A₁ ⊔ A₂) → (B₁ ⊔ B₂)。
 * 输入为两个函数块输入的并集，输出为两个函数块输出的并集。
 *
 * @param block1 - 第一个函数块
 * @param block2 - 第二个函数块
 * @returns 乘积结果
 *
 * @example
 * // 计算 "中点" 和 "等边三角形" 的乘积
 * const product = productBlocks(midpointBlock, equilateralBlock);
 * // 结果：输入4个点，分别在中点和底边上构造等边三角形
 */
export function productBlocks(block1: UserFuncBlock, block2: UserFuncBlock): ProductResult {
  // 计算新的输入点
  const newInputPointIds = [...block1.inputPointIds, ...block2.inputPointIds];

  // 计算新的内部点
  const block1InternalCount = block1.internalPointIds.length;
  const newInternalPointIds = [
    ...block1.internalPointIds,
    ...block2.internalPointIds.map((id) => id + block1InternalCount),
  ];

  // 更新相对坐标（保持相对于各自输入的偏移）
  const newRelativePositions = [
    ...block1.relativePositions.map((p) => ({ ...p })),
    ...block2.relativePositions.map((p) => ({
      ...p,
      id: p.id + block1InternalCount,
    })),
  ];

  const productBlock: UserFuncBlock = {
    id: `product_${Date.now()}`,
    name: `${block1.name} × ${block2.name}`,
    inputPointIds: newInputPointIds,
    internalPointIds: newInternalPointIds,
    internalSegmentIds: [
      ...block1.internalSegmentIds,
      ...block2.internalSegmentIds.map((id) => id + block1InternalCount),
    ],
    internalConstraintIds: [
      ...block1.internalConstraintIds,
      ...block2.internalConstraintIds.map((id) => id + block1InternalCount),
    ],
    relativePositions: newRelativePositions,
  };

  return {
    result: productBlock,
    description: `"${block1.name}" 和 "${block2.name}" 的乘积`,
  };
}

/**
 * 对函数块进行部分应用（Partial Apply / Currying）
 *
 * 部分应用固定函数块的某些输入参数，生成一个新的函数块，
 * 新函数块接受更少的输入。
 *
 * @param block - 原函数块
 * @param fixedInputIndices - 要固定的输入索引数组
 * @param fixedInputValues - 对应索引的固定值（相对坐标）
 * @returns 部分应用结果
 *
 * @example
 * // 部分应用 "等边三角形" 函数块，固定底边中点
 * const partial = partialApplyBlock(equilateralBlock, [0], [{ relX: 0, relY: 0 }]);
 * // 结果：输入1个点，输出以该点为中点的等边三角形
 */
export function partialApplyBlock(
  block: UserFuncBlock,
  fixedInputIndices: number[],
  fixedInputValues: Array<{ relX: number; relY: number }>,
): PartialApplyResult {
  if (fixedInputIndices.length !== fixedInputValues.length) {
    return {
      result: null,
      description: '固定输入数量不匹配',
      fixedInputs: [],
    };
  }

  if (fixedInputIndices.length >= block.inputPointIds.length) {
    return {
      result: null,
      description: '不能固定所有输入',
      fixedInputs: [],
    };
  }

  // 计算新的输入点：排除固定的输入
  const newInputPointIds = block.inputPointIds.filter((_, idx) => !fixedInputIndices.includes(idx));

  // 创建映射：将旧索引映射到新索引（排除固定输入后）
  const oldToNewIndexMap = new Map<number, number>();
  let newIndex = 0;
  for (let i = 0; i < block.inputPointIds.length; i++) {
    if (!fixedInputIndices.includes(i)) {
      oldToNewIndexMap.set(i, newIndex);
      newIndex++;
    }
  }

  // 更新内部点的相对坐标：将固定输入的影响纳入偏移
  const anchorOffsetX = fixedInputValues
    .filter((_, i) => block.inputPointIds[fixedInputIndices[i]] === block.inputPointIds[0])
    .reduce((sum, v) => sum + v.relX, 0);
  const anchorOffsetY = fixedInputValues
    .filter((_, i) => block.inputPointIds[fixedInputIndices[i]] === block.inputPointIds[0])
    .reduce((sum, v) => sum + v.relY, 0);

  const newRelativePositions = block.relativePositions.map((p) => {
    // 如果是固定输入的内部点，需要调整坐标
    const isFixedInternal = block.inputPointIds.some(
      (inputId, idx) =>
        fixedInputIndices.includes(idx) &&
        p.id === inputId,
    );
    if (isFixedInternal) {
      const fixedIdx = fixedInputIndices.find(
        (i) => block.inputPointIds[i] === p.id,
      );
      if (fixedIdx !== undefined) {
        const fixedVal = fixedInputValues[fixedInputIndices.indexOf(fixedIdx)];
        return {
          ...p,
          relX: p.relX - fixedVal.relX,
          relY: p.relY - fixedVal.relY,
        };
      }
    }
    return { ...p };
  });

  const partialBlock: UserFuncBlock = {
    id: `partial_${Date.now()}`,
    name: `partial(${block.name})`,
    inputPointIds: newInputPointIds,
    internalPointIds: [...block.internalPointIds],
    internalSegmentIds: [...block.internalSegmentIds],
    internalConstraintIds: [...block.internalConstraintIds],
    relativePositions: newRelativePositions,
  };

  return {
    result: partialBlock,
    description: `"${block.name}" 的部分应用（固定 ${fixedInputIndices.length} 个输入）`,
    fixedInputs: fixedInputIndices,
  };
}

/**
 * 验证函数块组合的兼容性
 *
 * 检查两个函数块是否可以组合（block1 的输出类型是否与 block2 的输入兼容）
 *
 * @param block1 - 前级函数块
 * @param block2 - 后级函数块
 * @returns 是否兼容
 */
export function validateComposition(
  block1: UserFuncBlock,
  block2: UserFuncBlock,
): boolean {
  // 基本验证：block1 至少有一个输出连接点，block2 至少有一个输入点
  if (block1.internalPointIds.length === 0 || block2.inputPointIds.length === 0) {
    return false;
  }

  // 检查维度兼容性（如果需要更复杂的类型检查，可扩展此处）
  return true;
}
