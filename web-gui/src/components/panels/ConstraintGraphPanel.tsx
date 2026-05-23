/**
 * @module components/panels/ConstraintGraphPanel
 * @description FRONTIER-style standalone constraint graph visualization panel.
 *              Renders the constraint graph as an interactive force-directed
 *              node-link diagram using Canvas-based rendering in a sidebar panel.
 *              Provides bidirectional linking with the main canvas.
 *              FRONTIER 风格的独立约束图可视化面板。
 */

import React, {
  useState,
  useRef,
  useEffect,
  useCallback,
  useMemo,
  memo,
} from 'react';
import { useGeometryStore, useInteractionStore } from '@/stores';
import {
  detectConflicts,
  calculateDOF,
  findMergeCandidates,
} from '@/utils/geometryAlgorithms';
import type { Point, ConstraintType } from '@/types';
import Panel from './Panel';

// ================================================================
// Types / 类型
// ================================================================

/** Node status for FRONTIER-style color coding */
type NodeStatus = 'satisfied' | 'conflict' | 'free' | 'pruned';

interface GraphNode {
  id: number;
  label: string;
  x: number;
  y: number;
  vx: number;
  vy: number;
  status: NodeStatus;
  /** Original point data for tooltip / selection */
  point: Point;
  /** Number of constraints involving this node */
  constraintCount: number;
}

interface GraphEdge {
  source: number;
  target: number;
  constraintType: ConstraintType;
  constraintId: number;
}

interface TooltipState {
  visible: boolean;
  x: number;
  y: number;
  node: GraphNode | null;
}

// ================================================================
// Constants / 常量
// ================================================================

/** Node fill colors by status (FRONTIER style) */
const NODE_COLORS: Record<NodeStatus, string> = {
  satisfied: '#4caf50',
  conflict: '#f44336',
  free: '#42a5f5',
  pruned: '#757575',
};

/** Node status labels for tooltip */
const NODE_STATUS_LABELS: Record<NodeStatus, string> = {
  satisfied: 'SATISFIED / 已满足',
  conflict: 'CONFLICT / 冲突',
  free: 'FREE / 欠约束',
  pruned: 'PRUNED / 已合并',
};

/** Edge stroke colors by constraint type */
const EDGE_COLORS: Record<ConstraintType, string> = {
  incidence: '#00bcd4',
  betweenness: '#ab47bc',
  intersection: '#ff9800',
  containment: '#4caf50',
  connection: '#9e9e9e',
};

const NODE_RADIUS = 14;
const CANVAS_HEIGHT = 250;

const SIM = {
  REPULSION: 4000,
  ATTRACTION: 0.006,
  DAMPING: 0.85,
  CENTER_FORCE: 0.003,
  REST_LENGTH: 100,
  MIN_DIST: 8,
  ITERATIONS: 50,
  STABILIZE_THRESHOLD: 0.5,
} as const;

const MERGE_DISTANCE_THRESHOLD = 15;

// ================================================================
// Graph Construction / 图构建
// ================================================================

/**
 * Determine the status of a point based on constraints, conflicts, and merge candidates.
 */
function classifyPointStatus(
  pointId: number,
  constrainedIds: Set<number>,
  conflictIds: Set<number>,
  candidateIds: Set<number>,
): NodeStatus {
  if (conflictIds.has(pointId)) return 'conflict';
  if (candidateIds.has(pointId)) return 'pruned';
  if (constrainedIds.has(pointId)) return 'satisfied';
  return 'free';
}

/**
 * Build graph nodes and edges from geometry store data.
 */
function buildGraph(
  points: Point[],
  constraints: import('@/types').Constraint[],
): { nodes: GraphNode[]; edges: GraphEdge[] } {
  if (points.length === 0) return { nodes: [], edges: [] };

  // Build set of point IDs constrained
  const constrainedIds = new Set<number>();
  for (const c of constraints) {
    for (const a of c.args) constrainedIds.add(a);
  }

  // Detect conflicts
  const conflictPairs = detectConflicts(constraints);
  const conflictIds = new Set<number>();
  for (const pair of conflictPairs) {
    const c1 = constraints.find((c) => c.id === pair.c1);
    const c2 = constraints.find((c) => c.id === pair.c2);
    if (c1) for (const a of c1.args) conflictIds.add(a);
    if (c2) for (const a of c2.args) conflictIds.add(a);
  }

  // Find merge candidates (points very close to each other)
  const mergePairs = findMergeCandidates(points, MERGE_DISTANCE_THRESHOLD);
  const candidateIds = new Set<number>();
  for (const pair of mergePairs) {
    candidateIds.add(pair.a);
    candidateIds.add(pair.b);
  }

  // Count constraints per point
  const constraintCounts = new Map<number, number>();
  for (const c of constraints) {
    for (const a of c.args) {
      constraintCounts.set(a, (constraintCounts.get(a) ?? 0) + 1);
    }
  }

  // Build nodes
  const nodes: GraphNode[] = points.map((p) => {
    const status = classifyPointStatus(p.id, constrainedIds, conflictIds, candidateIds);
    return {
      id: p.id,
      label: `P${p.id}`,
      x: 0,
      y: 0,
      vx: 0,
      vy: 0,
      status,
      point: p,
      constraintCount: constraintCounts.get(p.id) ?? 0,
    };
  });

  // Build edges: connect points that appear consecutively in constraint args
  const pointIdSet = new Set(points.map((p) => p.id));
  const edgeSet = new Set<string>();
  const edges: GraphEdge[] = [];

  for (const c of constraints) {
    const args = c.args.filter((a) => pointIdSet.has(a));
    for (let i = 0; i < args.length - 1; i++) {
      const a = args[i]!;
      const b = args[i + 1]!;
      if (a === b) continue;
      const key = a < b ? `${a}-${b}` : `${b}-${a}`;
      if (edgeSet.has(key)) continue;
      edgeSet.add(key);
      edges.push({
        source: a,
        target: b,
        constraintType: c.type,
        constraintId: c.id,
      });
    }
  }

  return { nodes, edges };
}

// ================================================================
// Force-Directed Layout / 力导向布局
// ================================================================

/**
 * Initialize node positions in a circle.
 */
function initLayout(nodes: GraphNode[], canvasW: number, canvasH: number): void {
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

/**
 * Run one step of Coulomb repulsion + spring attraction on the graph.
 * Returns the maximum velocity magnitude (for convergence check).
 */
function stepSimulation(
  nodes: GraphNode[],
  edges: GraphEdge[],
  canvasW: number,
  canvasH: number,
): number {
  const n = nodes.length;
  const cx = canvasW / 2;
  const cy = canvasH / 2;

  // Accumulated forces
  const fx = new Float64Array(n);
  const fy = new Float64Array(n);

  // Coulomb repulsion between all node pairs
  for (let i = 0; i < n; i++) {
    for (let j = i + 1; j < n; j++) {
      const a = nodes[i]!;
      const b = nodes[j]!;
      let dx = a.x - b.x;
      let dy = a.y - b.y;
      const dist = Math.max(SIM.MIN_DIST, Math.sqrt(dx * dx + dy * dy));
      const force = SIM.REPULSION / (dist * dist);
      const fdx = (dx / dist) * force;
      const fdy = (dy / dist) * force;
      fx[i] = (fx[i] ?? 0) + fdx; fy[i] = (fy[i] ?? 0) + fdy;
      fx[j] = (fx[j] ?? 0) - fdx; fy[j] = (fy[j] ?? 0) - fdy;
    }
  }

  // Spring attraction along edges
  for (const e of edges) {
    const si = nodes.findIndex((nd) => nd.id === e.source);
    const ti = nodes.findIndex((nd) => nd.id === e.target);
    if (si === -1 || ti === -1 || si === ti) continue;
    const a = nodes[si]!;
    const b = nodes[ti]!;
    let dx = b.x - a.x;
    let dy = b.y - a.y;
    const dist = Math.sqrt(dx * dx + dy * dy);
    if (dist < SIM.MIN_DIST) continue;
    const force = SIM.ATTRACTION * (dist - SIM.REST_LENGTH);
    const fdx = (dx / dist) * force;
    const fdy = (dy / dist) * force;
    fx[si] = (fx[si] ?? 0) + fdx; fy[si] = (fy[si] ?? 0) + fdy;
    fx[ti] = (fx[ti] ?? 0) - fdx; fy[ti] = (fy[ti] ?? 0) - fdy;
  }

  // Apply forces with damping + centering
  let maxV = 0;
  for (let i = 0; i < n; i++) {
    const node = nodes[i]!;
    node.vx = (node.vx + fx[i]! + (cx - node.x) * SIM.CENTER_FORCE) * SIM.DAMPING;
    node.vy = (node.vy + fy[i]! + (cy - node.y) * SIM.CENTER_FORCE) * SIM.DAMPING;
    node.x += node.vx;
    node.y += node.vy;
    // Clamp to canvas bounds
    node.x = Math.max(NODE_RADIUS, Math.min(canvasW - NODE_RADIUS, node.x));
    node.y = Math.max(NODE_RADIUS, Math.min(canvasH - NODE_RADIUS, node.y));
    const speed = Math.sqrt(node.vx * node.vx + node.vy * node.vy);
    if (speed > maxV) maxV = speed;
  }

  return maxV;
}

// ================================================================
// Canvas Rendering / Canvas 渲染
// ================================================================

/**
 * Draw a single frame of the constraint graph to the canvas.
 */
function renderFrame(
  ctx: CanvasRenderingContext2D,
  nodes: GraphNode[],
  edges: GraphEdge[],
  canvasW: number,
  canvasH: number,
  selectedId: number | null,
  hoveredId: number | null,
  isLayoutRunning: boolean,
): void {
  const dpr = window.devicePixelRatio || 1;
  ctx.save();
  ctx.scale(dpr, dpr);

  // Clear
  ctx.fillStyle = '#1a1a2e';
  ctx.fillRect(0, 0, canvasW, canvasH);

  if (nodes.length === 0) {
    ctx.fillStyle = '#484f58';
    ctx.font = 'italic 12px sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(
      'No graph data. Add points and constraints to see the graph.',
      canvasW / 2,
      canvasH / 2 - 8,
    );
    ctx.fillText(
      '暂无图数据。请添加点和约束以显示图。',
      canvasW / 2,
      canvasH / 2 + 10,
    );
    ctx.restore();
    return;
  }

  // Draw edges
  const dashedTypes: ConstraintType[] = ['connection'];
  for (const e of edges) {
    const src = nodes.find((nd) => nd.id === e.source);
    const tgt = nodes.find((nd) => nd.id === e.target);
    if (!src || !tgt) continue;
    const isHighlighted =
      hoveredId !== null &&
      (hoveredId === e.source || hoveredId === e.target);
    ctx.beginPath();
    ctx.moveTo(src.x, src.y);
    ctx.lineTo(tgt.x, tgt.y);
    ctx.strokeStyle = EDGE_COLORS[e.constraintType] || '#9e9e9e';
    ctx.lineWidth = isHighlighted ? 2.5 : 1.2;
    ctx.globalAlpha = hoveredId !== null && !isHighlighted ? 0.15 : 0.65;
    if (dashedTypes.includes(e.constraintType)) {
      ctx.setLineDash([4, 3]);
    }
    ctx.stroke();
    ctx.setLineDash([]);
  }
  ctx.globalAlpha = 1;

  // Draw nodes
  for (const node of nodes) {
    const isSelected = selectedId === node.id;
    const isHovered = hoveredId === node.id;
    const isDimmed = hoveredId !== null && !isHovered && !isSelected;
    const alpha = isDimmed ? 0.3 : 1;
    const color = NODE_COLORS[node.status];
    const radius = isHovered || isSelected ? NODE_RADIUS + 2 : NODE_RADIUS;

    ctx.globalAlpha = alpha;

    // Selection glow
    if (isSelected) {
      ctx.beginPath();
      ctx.arc(node.x, node.y, radius + 4, 0, Math.PI * 2);
      ctx.fillStyle = 'rgba(240, 136, 62, 0.2)';
      ctx.fill();
    }

    // Hover glow
    if (isHovered && !isSelected) {
      ctx.beginPath();
      ctx.arc(node.x, node.y, radius + 4, 0, Math.PI * 2);
      ctx.fillStyle = 'rgba(255, 255, 255, 0.1)';
      ctx.fill();
    }

    // Node circle
    ctx.beginPath();
    ctx.arc(node.x, node.y, radius, 0, Math.PI * 2);
    ctx.fillStyle = '#0d1117';
    ctx.fill();
    ctx.strokeStyle = isSelected ? '#f0883e' : isHovered ? '#ffffff' : color;
    ctx.lineWidth = isSelected || isHovered ? 2.5 : 1.5;
    ctx.stroke();

    // Label
    ctx.font = `600 ${isHovered || isSelected ? '10px' : '9px'} monospace`;
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillStyle = color;
    ctx.fillText(node.label, node.x, node.y + 1);

    // Small constraint count badge
    if (node.constraintCount > 0) {
      const badgeText = String(node.constraintCount);
      ctx.font = 'bold 8px monospace';
      const badgeW = ctx.measureText(badgeText).width + 5;
      const badgeX = node.x + NODE_RADIUS - 4;
      const badgeY = node.y - NODE_RADIUS + 4;
      ctx.fillStyle = '#1a1a2e';
      ctx.fillRect(badgeX - badgeW / 2, badgeY - 6, badgeW, 12);
      ctx.fillStyle = '#8b949e';
      ctx.fillText(badgeText, badgeX, badgeY);
    }
  }
  ctx.globalAlpha = 1;

  // Layout running indicator
  if (isLayoutRunning) {
    ctx.fillStyle = 'rgba(26, 26, 46, 0.7)';
    ctx.fillRect(0, 0, 80, 20);
    ctx.font = '9px monospace';
    ctx.fillStyle = '#58a6ff';
    ctx.textAlign = 'left';
    ctx.textBaseline = 'middle';
    ctx.fillText('SIMULATING...', 4, 10);
  }

  ctx.restore();
}

// ================================================================
// Sub-components / 子组件
// ================================================================

/** Tooltip that shows on node hover */
const NodeTooltip = memo<{
  tooltip: TooltipState;
  canvasRect: DOMRect | null;
}>(({ tooltip, canvasRect }) => {
  if (!tooltip.visible || !tooltip.node || !canvasRect) return null;

  const { node } = tooltip;
  const left = tooltip.x + 12;
  const top = tooltip.y - 10;

  const displayX = canvasRect ? left - canvasRect.left : left;
  const displayY = canvasRect ? top - canvasRect.top : top;

  return (
    <div
      className="graph-tooltip"
      style={{
        position: 'absolute',
        left: displayX,
        top: displayY,
        transform: 'translateY(-100%)',
        pointerEvents: 'none',
        zIndex: 100,
        background: '#161b22',
        border: '1px solid #30363d',
        borderRadius: 6,
        padding: '6px 10px',
        fontSize: 10,
        color: '#c9d1d9',
        fontFamily: 'monospace',
        boxShadow: '0 4px 12px rgba(0,0,0,0.4)',
        minWidth: 150,
      }}
    >
      <div style={{ fontWeight: 600, color: NODE_COLORS[node.status], marginBottom: 3 }}>
        {node.label} <span style={{ fontWeight: 400, fontSize: 9 }}>
          ({NODE_STATUS_LABELS[node.status]})
        </span>
      </div>
      <div>ID: {node.id}</div>
      <div>X: {node.point.x.toFixed(2)} Y: {node.point.y.toFixed(2)}</div>
      <div>Constraints: {node.constraintCount} 个约束</div>
    </div>
  );
});
NodeTooltip.displayName = 'NodeTooltip';

/** Statistics bar above the canvas */
const StatsBar = memo<{
  nodeCount: number;
  edgeCount: number;
  conflictCount: number;
  dof: number;
}>(({ nodeCount, edgeCount, conflictCount, dof }) => (
  <div
    style={{
      display: 'flex',
      gap: 12,
      padding: '4px 8px',
      borderBottom: '1px solid #30363d',
      background: '#0d1117',
      fontSize: 9,
      fontFamily: 'monospace',
      color: '#8b949e',
    }}
  >
    <span>NODES / 节点: <b style={{ color: '#c9d1d9' }}>{nodeCount}</b></span>
    <span>EDGES / 边: <b style={{ color: '#c9d1d9' }}>{edgeCount}</b></span>
    <span>
      CONFLICTS / 冲突:{' '}
      <b style={{ color: conflictCount > 0 ? '#f44336' : '#3fb950' }}>
        {conflictCount}
      </b>
    </span>
    <span>DOF: <b style={{ color: dof > 0 ? '#42a5f5' : '#c9d1d9' }}>{dof}</b></span>
  </div>
));
StatsBar.displayName = 'StatsBar';

/** Toolbar with layout and matrix toggle buttons */
const ToolBar = memo<{
  onRelayout: () => void;
  onToggleMatrix: () => void;
  showMatrix: boolean;
  nodeCount: number;
  isLayoutRunning: boolean;
}>(({ onRelayout, onToggleMatrix, showMatrix, nodeCount, isLayoutRunning }) => (
  <div
    style={{
      display: 'flex',
      gap: 4,
      padding: '4px 8px',
      borderBottom: '1px solid #30363d',
      background: '#0d1117',
      flexWrap: 'wrap',
    }}
  >
    <button
      onClick={onRelayout}
      disabled={nodeCount === 0}
      title="Reset and re-run force-directed layout"
      style={{
        padding: '2px 8px',
        fontSize: 9,
        borderRadius: 3,
        border: `1px solid ${isLayoutRunning ? '#58a6ff' : '#30363d'}`,
        background: isLayoutRunning ? '#1f2937' : '#161b22',
        color: isLayoutRunning ? '#58a6ff' : '#c9d1d9',
        cursor: nodeCount === 0 ? 'not-allowed' : 'pointer',
        whiteSpace: 'nowrap',
        fontFamily: 'monospace',
        opacity: nodeCount === 0 ? 0.4 : 1,
      }}
    >
      {'\u21BB'} RELAYOUT / 重新布局
    </button>
    <button
      onClick={onToggleMatrix}
      disabled={nodeCount === 0}
      title="Toggle adjacency matrix view"
      style={{
        padding: '2px 8px',
        fontSize: 9,
        borderRadius: 3,
        border: `1px solid ${showMatrix ? '#58a6ff' : '#30363d'}`,
        background: showMatrix ? '#1f2937' : '#161b22',
        color: showMatrix ? '#58a6ff' : '#c9d1d9',
        cursor: nodeCount === 0 ? 'not-allowed' : 'pointer',
        whiteSpace: 'nowrap',
        fontFamily: 'monospace',
        opacity: nodeCount === 0 ? 0.4 : 1,
      }}
    >
      {'\u2261'} MATRIX / 矩阵
    </button>
  </div>
));
ToolBar.displayName = 'ToolBar';

// ================================================================
// Main Component / 主组件
// ================================================================

const ConstraintGraphPanel: React.FC = () => {
  // ---- Store state / 商店状态 ----
  const points = useGeometryStore((s) => s.points);
  const constraints = useGeometryStore((s) => s.constraints);
  const selectedPoint = useInteractionStore((s) => s.selectedPoint);
  const hoveredPoint = useInteractionStore((s) => s.hoveredPoint);
  const setSelectedPoint = useInteractionStore((s) => s.setSelectedPoint);
  const setHoveredPoint = useInteractionStore((s) => s.setHoveredPoint);

  // ---- Ref / 引用 ----
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const containerRef = useRef<HTMLDivElement>(null);
  const animRef = useRef<number>(0);
  const layoutRef = useRef<{ running: boolean; cancel: boolean }>({
    running: false,
    cancel: false,
  });
  const pendingRelayoutRef = useRef(false);

  // ---- Component state / 组件状态 ----
  const [showMatrix, setShowMatrix] = useState(false);
  const [isLayoutRunning, setIsLayoutRunning] = useState(false);
  const [hoveredNodeId, setHoveredNodeId] = useState<number | null>(null);
  const [tooltip, setTooltip] = useState<TooltipState>({
    visible: false,
    x: 0,
    y: 0,
    node: null,
  });
  // Store mutable layout state in a ref for render loop access
  const layoutStateRef = useRef<{
    nodes: GraphNode[];
    edges: GraphEdge[];
  }>({ nodes: [], edges: [] });

  // ---- Derived state / 派生状态 ----
  const selectedId = selectedPoint?.id ?? null;
  const hoveredId = hoveredPoint?.id ?? hoveredNodeId;

  const { nodes, edges } = useMemo(
    () => buildGraph(points, constraints),
    [points, constraints],
  );

  const conflictCount = useMemo(() => {
    const conflictPairs = detectConflicts(constraints);
    return conflictPairs.length;
  }, [constraints]);

  const dof = useMemo(() => calculateDOF(points, constraints), [points, constraints]);

  // ================================================================
  // Force-Directed Layout Runner / 力导向布局运行器
  // ================================================================

  const runLayout = useCallback(() => {
    if (nodes.length === 0) return;
    const canvas = canvasRef.current;
    if (!canvas) return;

    // Cancel existing layout
    if (layoutRef.current.running) {
      layoutRef.current.cancel = true;
      pendingRelayoutRef.current = false;
      setTimeout(() => {
        layoutRef.current.cancel = false;
        pendingRelayoutRef.current = true;
      }, 30);
      return;
    }

    const canvasW = canvas.clientWidth;
    const canvasH = canvas.clientHeight;

    // Deep-clone nodes for simulation
    const simNodes: GraphNode[] = nodes.map((n) => ({
      ...n,
      x: 0,
      y: 0,
      vx: 0,
      vy: 0,
    }));

    initLayout(simNodes, canvasW, canvasH);

    layoutStateRef.current = { nodes: simNodes, edges: [...edges] };
    layoutRef.current.running = true;
    layoutRef.current.cancel = false;
    setIsLayoutRunning(true);

    let iter = 0;

    const step = () => {
      if (layoutRef.current.cancel) {
        layoutRef.current.running = false;
        setIsLayoutRunning(false);
        if (pendingRelayoutRef.current) {
          pendingRelayoutRef.current = false;
          setTimeout(runLayout, 30);
        }
        return;
      }

      const maxV = stepSimulation(simNodes, edges, canvasW, canvasH);
      iter++;

      const done =
        iter >= SIM.ITERATIONS ||
        (iter >= 30 && maxV < SIM.STABILIZE_THRESHOLD);

      if (!done) {
        animRef.current = requestAnimationFrame(step);
      } else {
        layoutRef.current.running = false;
        setIsLayoutRunning(false);
      }

      // Render current state to canvas
      const canvasEl = canvasRef.current;
      if (canvasEl) {
        const ctx = canvasEl.getContext('2d');
        if (ctx) {
          renderFrame(
            ctx,
            simNodes,
            edges,
            canvasW,
            canvasH,
            selectedId,
            hoveredId,
            !done,
          );
        }
      }
    };

    animRef.current = requestAnimationFrame(step);
  }, [nodes, edges, selectedId, hoveredId]);

  // ================================================================
  // Effects / 副作用
  // ================================================================

  // Auto-run layout when data changes
  useEffect(() => {
    if (nodes.length > 0) {
      layoutRef.current.cancel = true;
      const t = setTimeout(() => {
        layoutRef.current.cancel = false;
        runLayout();
      }, 120);
      return () => clearTimeout(t);
    } else {
      layoutStateRef.current = { nodes: [], edges: [] };
      const canvas = canvasRef.current;
      if (canvas) {
        const ctx = canvas.getContext('2d');
        if (ctx) {
          const dpr = window.devicePixelRatio || 1;
          canvas.width = canvas.clientWidth * dpr;
          canvas.height = canvas.clientHeight * dpr;
          renderFrame(
            ctx,
            [],
            [],
            canvas.clientWidth,
            canvas.clientHeight,
            null,
            null,
            false,
          );
        }
      }
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [points.length, constraints.length]);

  // Re-render canvas when selection/hover changes
  const renderCanvas = useCallback(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const dpr = window.devicePixelRatio || 1;
    canvas.width = canvas.clientWidth * dpr;
    canvas.height = canvas.clientHeight * dpr;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const displayNodes = layoutRef.current.running
      ? layoutStateRef.current.nodes
      : layoutStateRef.current.nodes.length > 0
        ? layoutStateRef.current.nodes
        : [];
    const displayEdges =
      layoutRef.current.running || layoutStateRef.current.edges.length > 0
        ? layoutStateRef.current.edges
        : edges;

    renderFrame(
      ctx,
      displayNodes.length > 0 ? displayNodes : nodes.map((n) => ({ ...n, x: 0, y: 0, vx: 0, vy: 0 })),
      displayEdges.length > 0 ? displayEdges : edges,
      canvas.clientWidth,
      canvas.clientHeight,
      selectedId,
      hoveredId,
      isLayoutRunning,
    );
  }, [selectedId, hoveredId, isLayoutRunning, nodes, edges]);

  useEffect(() => {
    if (!layoutRef.current.running) {
      renderCanvas();
    }
  }, [renderCanvas, layoutRef.current.running]);

  // Cleanup animation on unmount
  useEffect(() => {
    return () => {
      cancelAnimationFrame(animRef.current);
      layoutRef.current.cancel = true;
    };
  }, []);

  // ================================================================
  // Event Handlers / 事件处理器
  // ================================================================

  const getNodeAtPos = useCallback(
    (mx: number, my: number, canvasRect: DOMRect): GraphNode | null => {
      const displayNodes =
        layoutStateRef.current.nodes.length > 0
          ? layoutStateRef.current.nodes
          : nodes;
      const dpr = window.devicePixelRatio || 1;
      const scaleX = (canvasRect.width * dpr) / canvasRect.width;
      const x = (mx - canvasRect.left) * scaleX;
      const y = (my - canvasRect.top) * scaleX;

      // Search in reverse to pick topmost node
      for (let i = displayNodes.length - 1; i >= 0; i--) {
        const node = displayNodes[i]!;
        const dx = x - node.x;
        const dy = y - node.y;
        if (dx * dx + dy * dy <= (NODE_RADIUS + 4) * (NODE_RADIUS + 4)) {
          return node;
        }
      }
      return null;
    },
    [nodes],
  );

  const handleCanvasMouseMove = useCallback(
    (e: React.MouseEvent<HTMLCanvasElement>) => {
      const canvas = canvasRef.current;
      if (!canvas) return;
      const rect = canvas.getBoundingClientRect();
      const node = getNodeAtPos(e.clientX, e.clientY, rect);

      if (node) {
        setHoveredNodeId(node.id);
        setHoveredPoint(node.point);
        setTooltip({
          visible: true,
          x: e.clientX,
          y: e.clientY,
          node,
        });
      } else {
        setHoveredNodeId(null);
        setHoveredPoint(null);
        setTooltip((prev) => ({ ...prev, visible: false }));
      }
    },
    [getNodeAtPos, setHoveredPoint],
  );

  const handleCanvasMouseLeave = useCallback(() => {
    setHoveredNodeId(null);
    setHoveredPoint(null);
    setTooltip((prev) => ({ ...prev, visible: false }));
  }, [setHoveredPoint]);

  const handleCanvasClick = useCallback(
    (e: React.MouseEvent<HTMLCanvasElement>) => {
      const canvas = canvasRef.current;
      if (!canvas) return;
      const rect = canvas.getBoundingClientRect();
      const node = getNodeAtPos(e.clientX, e.clientY, rect);
      if (node) {
        setSelectedPoint(node.point);
      }
    },
    [getNodeAtPos, setSelectedPoint],
  );

  const handleRelayout = useCallback(() => {
    runLayout();
  }, [runLayout]);

  const handleToggleMatrix = useCallback(() => {
    setShowMatrix((prev) => !prev);
  }, []);

  // ================================================================
  // Adjacency Matrix View / 邻接矩阵视图
  // ================================================================

  const adjacencyMatrix = useMemo(() => {
    if (nodes.length === 0) return { matrix: [] as number[][], labels: [] as string[] };
    const n = nodes.length;
    const nodeIds = nodes.map((nd) => nd.id);
    const labels = nodes.map((nd) => nd.label);
    const matrix: number[][] = Array.from({ length: n }, () => Array(n).fill(0));

    for (const e of edges) {
      const si = nodeIds.indexOf(e.source);
      const ti = nodeIds.indexOf(e.target);
      if (si !== -1 && ti !== -1) {
        matrix[si]![ti] = 1;
        matrix[ti]![si] = 1;
      }
    }
    return { matrix, labels };
  }, [nodes, edges]);

  const matrixView = useMemo(() => {
    if (adjacencyMatrix.matrix.length === 0) return null;
    const { matrix, labels } = adjacencyMatrix;
    const n = matrix.length;
    if (n > 30) {
      return (
        <div style={{ padding: 12, color: '#8b949e', fontSize: 10, fontFamily: 'monospace' }}>
          Matrix too large to display ({n}x{n}). Showing graph view.
        </div>
      );
    }

    return (
      <div
        style={{
          overflow: 'auto',
          maxHeight: CANVAS_HEIGHT,
          padding: '4px 8px',
          fontFamily: 'monospace',
          fontSize: 9,
          background: '#1a1a2e',
        }}
      >
        <div style={{ marginBottom: 4, color: '#58a6ff', fontSize: 10, fontWeight: 600 }}>
          ADJACENCY MATRIX / 邻接矩阵 ({n}x{n})
        </div>
        {/* Header row */}
        <div style={{ display: 'flex' }}>
          <div style={{ width: 36, flexShrink: 0 }} />
          {labels.map((label, i) => (
            <div
              key={i}
              style={{
                width: 18,
                textAlign: 'center',
                color: NODE_COLORS[nodes[i]!.status],
                writingMode: 'vertical-rl',
                transform: 'rotate(180deg)',
                fontSize: 8,
              }}
            >
              {label}
            </div>
          ))}
        </div>
        {/* Matrix rows */}
        {matrix.map((row, ri) => (
          <div key={ri} style={{ display: 'flex', marginTop: 1 }}>
            <div
              style={{
                width: 36,
                flexShrink: 0,
                color: NODE_COLORS[nodes[ri]!.status],
                textAlign: 'right',
                paddingRight: 4,
              }}
            >
              {labels[ri]}
            </div>
            {row.map((val, ci) => (
              <div
                key={ci}
                style={{
                  width: 18,
                  height: 14,
                  textAlign: 'center',
                  lineHeight: '14px',
                  background: val ? 'rgba(88, 166, 255, 0.3)' : 'transparent',
                  color: val ? '#58a6ff' : '#30363d',
                  borderRadius: 1,
                  border: '1px solid #21262d',
                }}
              >
                {val || '\u00B7'}
              </div>
            ))}
          </div>
        ))}
      </div>
    );
  }, [adjacencyMatrix, nodes]);

  // ================================================================
  // Canvas element (memoized to avoid re-creation)
  // ================================================================

  const canvasElement = useMemo(
    () => (
      <div
        ref={containerRef}
        style={{
          position: 'relative',
          height: CANVAS_HEIGHT,
          background: '#1a1a2e',
          borderTop: '1px solid #30363d',
        }}
      >
        <canvas
          ref={canvasRef}
          style={{
            width: '100%',
            height: '100%',
            display: 'block',
            cursor: isLayoutRunning ? 'wait' : 'crosshair',
          }}
          onMouseMove={handleCanvasMouseMove}
          onMouseLeave={handleCanvasMouseLeave}
          onClick={handleCanvasClick}
        />
        <NodeTooltip tooltip={tooltip} canvasRect={canvasRef.current?.getBoundingClientRect() ?? null} />
      </div>
    ),
    [
      handleCanvasMouseMove,
      handleCanvasMouseLeave,
      handleCanvasClick,
      tooltip,
      isLayoutRunning,
    ],
  );

  // ================================================================
  // Render / 渲染
  // ================================================================

  return (
    <Panel title="CONSTRAINT GRAPH / 约束图" panelId="graph-viz">
      <StatsBar
        nodeCount={nodes.length}
        edgeCount={edges.length}
        conflictCount={conflictCount}
        dof={dof}
      />
      <ToolBar
        onRelayout={handleRelayout}
        onToggleMatrix={handleToggleMatrix}
        showMatrix={showMatrix}
        nodeCount={nodes.length}
        isLayoutRunning={isLayoutRunning}
      />
      {showMatrix ? matrixView : canvasElement}
    </Panel>
  );
};

export default memo(ConstraintGraphPanel);
