/**
 * @module components/panels/utils/forceLayout
 * @description 力导向布局算法。
 *              实现 Coulomb 斥力 + 弹簧引力的力导向图布局，
 *              用于约束图的可视化展示。
 *
 *              Force-directed layout algorithm.
 *              Implements Coulomb repulsion + spring attraction force-directed
 *              graph layout for constraint graph visualization.
 */

// ================================================================
// 类型定义 / Type Definitions
// ================================================================

/** 图节点（包含位置和速度） */
export interface LayoutNode {
  id: number;
  label: string;
  x: number;
  y: number;
  vx: number;
  vy: number;
}

/** 图边 */
export interface LayoutEdge {
  source: number;
  target: number;
}

/** 力导向模拟参数 */
export interface SimParams {
  /** 斥力强度（Coulomb 常数） */
  REPULSION: number;
  /** 弹簧引力强度 */
  ATTRACTION: number;
  /** 速度衰减系数（0-1，越小越快稳定） */
  DAMPING: number;
  /** 向心力强度（防止节点飘出画布） */
  CENTER_FORCE: number;
  /** 弹簧静止长度 */
  REST_LENGTH: number;
  /** 最小距离阈值（避免除零） */
  MIN_DIST: number;
  /** 最大迭代次数 */
  ITERATIONS: number;
  /** 收敛速度阈值 */
  STABILIZE_THRESHOLD: number;
}

// ================================================================
// 默认参数 / Default Parameters
// ================================================================

/** 默认力导向模拟参数 */
export const DEFAULT_SIM_PARAMS: SimParams = {
  REPULSION: 4000,
  ATTRACTION: 0.006,
  DAMPING: 0.85,
  CENTER_FORCE: 0.003,
  REST_LENGTH: 100,
  MIN_DIST: 8,
  ITERATIONS: 50,
  STABILIZE_THRESHOLD: 0.5,
} as const;

// ================================================================
// 布局初始化 / Layout Initialization
// ================================================================

/**
 * 将节点初始化为圆形布局。
 * 节点均匀分布在以画布中心为圆心的圆上，并添加少量随机偏移。
 *
 * @param nodes - 图节点数组（会被就地修改 x, y）
 * @param canvasW - 画布宽度
 * @param canvasH - 画布高度
 */
export function initLayout(
  nodes: LayoutNode[],
  canvasW: number,
  canvasH: number,
): void {
  const cx = canvasW / 2;
  const cy = canvasH / 2;
  const radius = Math.min(canvasW, canvasH) * 0.35;

  if (nodes.length === 1) {
    nodes[0]!.x = cx;
    nodes[0]!.y = cy;
    return;
  }

  for (let i = 0; i < nodes.length; i++) {
    const angle = (2 * Math.PI * i) / nodes.length - Math.PI / 2;
    nodes[i]!.x = cx + Math.cos(angle) * radius + (Math.random() - 0.5) * 20;
    nodes[i]!.y = cy + Math.sin(angle) * radius + (Math.random() - 0.5) * 20;
  }
}

// ================================================================
// 模拟步进 / Simulation Step
// ================================================================

/**
 * 执行一步力导向模拟。
 * 包含 Coulomb 斥力（所有节点对之间）和弹簧引力（沿边方向）。
 * 返回最大速度值，用于收敛判断。
 *
 * @param nodes - 图节点数组（会被就地修改 x, y, vx, vy）
 * @param edges - 图边数组
 * @param canvasW - 画布宽度
 * @param canvasH - 画布高度
 * @param nodeRadius - 节点半径（用于边界约束）
 * @param params - 模拟参数（可选，默认使用 DEFAULT_SIM_PARAMS）
 * @returns 最大速度值
 */
export function stepSimulation(
  nodes: LayoutNode[],
  edges: LayoutEdge[],
  canvasW: number,
  canvasH: number,
  nodeRadius: number = 14,
  params: SimParams = DEFAULT_SIM_PARAMS,
): number {
  const n = nodes.length;
  const cx = canvasW / 2;
  const cy = canvasH / 2;

  // 累积力数组
  const fx = new Float64Array(n);
  const fy = new Float64Array(n);

  // Coulomb 斥力：所有节点对之间
  for (let i = 0; i < n; i++) {
    for (let j = i + 1; j < n; j++) {
      const a = nodes[i]!;
      const b = nodes[j]!;
      let dx = a.x - b.x;
      let dy = a.y - b.y;
      const dist = Math.max(params.MIN_DIST, Math.sqrt(dx * dx + dy * dy));
      const force = params.REPULSION / (dist * dist);
      const fdx = (dx / dist) * force;
      const fdy = (dy / dist) * force;
      fx[i] = (fx[i] ?? 0) + fdx; fy[i] = (fy[i] ?? 0) + fdy;
      fx[j] = (fx[j] ?? 0) - fdx; fy[j] = (fy[j] ?? 0) - fdy;
    }
  }

  // 弹簧引力：沿边方向
  for (const e of edges) {
    const si = nodes.findIndex((nd) => nd.id === e.source);
    const ti = nodes.findIndex((nd) => nd.id === e.target);
    if (si === -1 || ti === -1 || si === ti) continue;
    const a = nodes[si]!;
    const b = nodes[ti]!;
    let dx = b.x - a.x;
    let dy = b.y - a.y;
    const dist = Math.sqrt(dx * dx + dy * dy);
    if (dist < params.MIN_DIST) continue;
    const force = params.ATTRACTION * (dist - params.REST_LENGTH);
    const fdx = (dx / dist) * force;
    const fdy = (dy / dist) * force;
    fx[si] = (fx[si] ?? 0) + fdx; fy[si] = (fy[si] ?? 0) + fdy;
    fx[ti] = (fx[ti] ?? 0) - fdx; fy[ti] = (fy[ti] ?? 0) - fdy;
  }

  // 应用力 + 阻尼 + 向心力
  let maxV = 0;
  for (let i = 0; i < n; i++) {
    const node = nodes[i]!;
    node.vx = (node.vx + fx[i]! + (cx - node.x) * params.CENTER_FORCE) * params.DAMPING;
    node.vy = (node.vy + fy[i]! + (cy - node.y) * params.CENTER_FORCE) * params.DAMPING;
    node.x += node.vx;
    node.y += node.vy;
    // 边界约束：将节点限制在画布内
    node.x = Math.max(nodeRadius, Math.min(canvasW - nodeRadius, node.x));
    node.y = Math.max(nodeRadius, Math.min(canvasH - nodeRadius, node.y));
    const speed = Math.sqrt(node.vx * node.vx + node.vy * node.vy);
    if (speed > maxV) maxV = speed;
  }

  return maxV;
}
