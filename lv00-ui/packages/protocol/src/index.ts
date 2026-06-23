// ============================================================
// @lv00/protocol — Lv-00 全模态通信协议 v1.0
// 所有层之间通信的共享类型定义
// ============================================================

// ---- 基础几何类型 ----

export type CoordType = 'SYM_RATIONAL' | 'SYM_ALGEBRAIC' | 'SYM_QUADRATIC';

export interface RationalCoord {
  type: 'SYM_RATIONAL';
  num: number;
  den: number;
}

export interface AlgebraicCoord {
  type: 'SYM_ALGEBRAIC';
  coeffs: number[];
  degree: number;
}

export interface QuadraticCoord {
  type: 'SYM_QUADRATIC';
  a: number;
  b: number;
  n: number;
}

export type Coord = RationalCoord | AlgebraicCoord | QuadraticCoord;

export type NodeType = 'NODE_POINT' | 'NODE_SEGMENT' | 'NODE_REGION' | 'NODE_PORT' | 'NODE_FUNCTION_BLOCK';

export interface Node {
  id: number;
  type: NodeType;
  name: string;
  coords: Coord[];
  color: number;          // RGBA 32-bit
  parent_block_id: number;
  namespace_depth: number;
}

export type ConstraintType = 'CONSTR_INCIDENCE' | 'CONSTR_BETWEENNESS' | 'CONSTR_INTERSECTION'
  | 'CONSTR_CONTAINMENT' | 'CONSTR_CONNECTION' | 'CONSTR_SEGMENT'
  | 'CONSTR_EQUAL' | 'CONSTR_PERPENDICULAR' | 'CONSTR_PARALLEL' | 'CONSTR_COLLINEAR';

export interface Constraint {
  id: number;
  type: ConstraintType;
  name: string;
  participants: number[];
  is_satisfied: boolean;
  // segment-specific
  p0?: number;
  p1?: number;
  from_port?: number;
  to_port?: number;
}

export interface Port {
  id: number;
  name: string;
  direction: 'PORT_INPUT' | 'PORT_OUTPUT';
  connected_to: number;
}

export interface Proof {
  id: number;
  name: string;
  root_node_id: number;
  trust_color: number;
  is_proved: boolean;
  depends_on: number[];
  axiom_refs: number[];
  steps: ProofStep[];
}

export interface ProofStep {
  index: number;
  description: string;
  rule_name: string;
  node_ids: number[];
  is_verified: boolean;
}

// ---- 命令系统 ----

export enum CommandType {
  ADD_NODE = 0,
  MOVE_NODE = 1,
  DELETE_NODE = 2,
  ADD_CONSTRAINT = 3,
  DELETE_CONSTRAINT = 4,
  MERGE_NODES = 5,
  APPLY_REWRITE = 6,
  PACK_FUNCTION = 7,
  UNPACK_FUNCTION = 8,
  ADD_REGION = 9,
  DELETE_REGION = 10,
  NORMALIZE = 11,
  SNAPSHOT = 12,
  RESTORE = 13,
  UNDO = 14,
  REDO = 15,
}

export interface Command {
  type: CommandType;
  params: number[];
  blob?: Uint8Array;
}

// Command factory functions
export function makeAddNode(node: Partial<Node> & { name: string; coords: Coord[] }): Command {
  return {
    type: CommandType.ADD_NODE,
    params: [node.type === 'NODE_POINT' ? 0 : 1, ...encodeCoords(node.coords)],
    blob: new TextEncoder().encode(node.name),
  };
}

export function makeMoveNode(nodeId: number, coord: Coord): Command {
  return {
    type: CommandType.MOVE_NODE,
    params: [nodeId, ...encodeCoords([coord])],
  };
}

export function makeDeleteNode(nodeId: number): Command {
  return { type: CommandType.DELETE_NODE, params: [nodeId] };
}

export function makeAddConstraint(type: ConstraintType, participants: number[]): Command {
  return { type: CommandType.ADD_CONSTRAINT, params: [constraintTypeCode(type), ...participants] };
}

export function makeNormalize(scopeAware: boolean): Command {
  return { type: CommandType.NORMALIZE, params: [scopeAware ? 1 : 0] };
}

export function makeUndo(): Command {
  return { type: CommandType.UNDO, params: [] };
}

export function makeRedo(): Command {
  return { type: CommandType.REDO, params: [] };
}

// ---- 执行结果 ----

export interface CommandResult {
  success: boolean;
  error_code: number;
  error_message: string;
  snapshot_id: number;
  delta_sequence_start: number;
  delta_sequence_end: number;
}

// ---- 增量日志 (Delta) ----

export enum DeltaType {
  ADD_NODE = 0,
  REMOVE_NODE = 1,
  MOVE_NODE = 2,
  ADD_CONSTRAINT = 3,
  REMOVE_CONSTRAINT = 4,
  MERGE_NODES = 5,
  NORMALIZE_COMPLETE = 6,
}

export interface DeltaRecord {
  seq: number;
  timestamp: number;
  type: DeltaType;
  payload: Record<string, unknown>;
}

export interface Delta {
  records: DeltaRecord[];
  from_seq: number;
  to_seq: number;
}

// ---- 快照 ----

export interface Snapshot {
  id: number;
  timestamp: number;
  description: string;
  node_count: number;
  constraint_count: number;
}

// ---- 绘制指令 (L3 → M1 Canvas) ----

export type DrawCommandType = 'LINE' | 'CIRCLE' | 'ARC' | 'RECT' | 'TEXT' | 'POINT' | 'PORT' | 'HIGHLIGHT';

export type LineStyle = 'SOLID' | 'DASHED' | 'DOTTED';

export interface DrawCommand {
  type: DrawCommandType;
  x1: number;
  y1: number;
  x2?: number;
  y2?: number;
  radius?: number;
  text?: string;
  color: number;           // RGBA 32-bit
  lineWidth?: number;
  style?: LineStyle;
}

// ---- M3 表格投影 ----

export interface NodeRow {
  id: number;
  name: string;
  type: string;
  coordX: string;
  coordY: string;
  color: number;
  constraint_count: number;
  parent_block: number;
}

export interface ConstraintRow {
  id: number;
  type: string;
  participants: string;
  is_satisfied: boolean;
}

// ---- M4 依赖树 ----

export interface TreeNode {
  id: string;
  label: string;
  color: number;
  status: 'proved' | 'pending' | 'failed' | 'assumed' | 'root';
  children: TreeNode[];
  node_id: number;
}

// ---- M6 拓扑视图 ----

export interface BlockNode {
  id: number;
  name: string;
  inputs: Port[];
  outputs: Port[];
  layoutX: number;
  layoutY: number;
}

expor