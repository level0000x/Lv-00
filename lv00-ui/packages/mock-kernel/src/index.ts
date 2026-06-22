// ============================================================
// @lv00/mock-kernel — Mock L1 裸核引擎
// 在无 C 内核的情况下提供完整的模拟实现，支持 UI 开发和测试
// ============================================================

import {
  Node, Constraint, Proof, Command, CommandType, CommandResult,
  Delta, DeltaRecord, DeltaType, Coord, CoordType,
  coordRational, coordQuadratic, coordToDouble, coordFromDouble,
  NodeType, ConstraintType, Snapshot, makeAddNode
} from '@lv00/protocol';

// ---- 内部状态 ----

let nextNodeId = 1;
let nextConstraintId = 1;
let nextProofId = 1;
let nextSnapshotId = 1;

const nodes: Map<number, Node> = new Map();
const constraints: Map<number, Constraint> = new Map();
const proofs: Map<number, Proof> = new Map();
const snapshots: Map<number, { nodes: Node[]; constraints: Constraint[] }> = new Map();

// Delta 环形缓冲区
const DELTA_CAPACITY = 10000;
const deltaBuffer: DeltaRecord[] = [];
let deltaWriteSeq = 0n;
let deltaReadSeq = 0n;
let globalSeq = 0n;

function nextSeq(): bigint {
  return ++globalSeq;
}

function pushDelta(type: DeltaType, payload: Record<string, unknown>): void {
  const record: DeltaRecord = {
    seq: Number(nextSeq()),
    timestamp: Date.now(),
    type,
    payload,
  };
  if (deltaBuffer.length >= DELTA_CAPACITY) {
    deltaBuffer.shift();
    deltaReadSeq++;
  }
  deltaBuffer.push(record);
  deltaWriteSeq++;
}

// ---- 图生命周期 ----

export function graphCreate(): void {
  nodes.clear();
  constraints.clear();
  proofs.clear();
  snapshots.clear();
  nextNodeId = 1;
  nextConstraintId = 1;
  nextProofId = 1;
  nextSnapshotId = 1;
  deltaBuffer.length = 0;
  deltaWriteSeq = 0n;
  deltaReadSeq = 0n;
  globalSeq = 0n;
}

// ---- 节点操作 ----

export function addNode(node: Partial<Node> & { name: string; coords: Coord[]; type?: NodeType }): number {
  const id = nextNodeId++;
  const fullNode: Node = {
    id,
    type: node.type ?? 'NODE_POINT',
    name: node.name,
    coords: node.coords,
    color: node.color ?? 0xFFFFFFFF,
    parent_block_id: node.parent_block_id ?? 0,
    namespace_depth: node.namespace_depth ?? 0,
  };
  nodes.set(id, fullNode);
  pushDelta(DeltaType.ADD_NODE, { id, type: fullNode.type, name: fullNode.name });
  return id;
}

export function removeNode(id: number): void {
  const node = nodes.get(id);
  if (!node) throw new Error(`Node ${id} not found`);
  nodes.delete(id);
  pushDelta(DeltaType.REMOVE_NODE, { id });
}

export function moveNode(id: number, newCoords: Coord[]): void {
  const node = nodes.get(id);
  if (!node) throw new Error(`Node ${id} not found`);
  node.coords = newCoords;
  pushDelta(DeltaType.MOVE_NODE, { id, coords: newCoords });
}

export function getNode(id: number): Node | undefined {
  return nodes.get(id);
}

export function getAllNodes(): Node[] {
  return Array.from(nodes.values());
}

export function getNodeNames(): string[] {
  return Array.from(nodes.values()).map(n => n.name);
}

// ---- 约束操作 ----

export function addConstraint(type: ConstraintType, participants: number[], name?: string): number {
  const id = nextConstraintId++;
  const constraint: Constraint = {
    id,
    type,
    name: name ?? `c_${id}`,
    participants,
    is_satisfied: true,
  };
  if (type === 'CONSTR_SEGMENT' && participants.length >= 2) {
    constraint.p0 = participants[0];
    constraint.p1 = participants[1];
  }
  constraints.set(id, constraint);
  pushDelta(DeltaType.ADD_CONSTRAINT, { id, type, participants });
  return id;
}

export function removeConstraint(id: number): void {
  const c = constraints.get(id);
  if (!c) throw new Error(`Constraint ${id} not found`);
  constraints.delete(id);
  pushDelta(DeltaType.REMOVE_CONSTRAINT, { id });
}

export function getConstraint(id: number): Constraint | undefined {
  return constraints.get(id);
}

export function getAllConstraints(): Constraint[] {
  return Array.from(constraints.values());
}

// ---- 归一化 ----

export function normalize(scopeAware: boolean): Array<{ original: number; kept: number }> {
  const mergedPairs: Array<{ original: number; kept: number }> = [];
  const allNodes = Array.from(nodes.values());
  const coordMap = new Map<string, Node[]>();

  for (const node of allNodes) {
    const key = node.coords.map(c => `${c.type}:${coordToDouble(c).toFixed(6)}`).join('|');
    if (!coordMap.has(key)) coordMap.set(key, []);
    coordMap.get(key)!.push(node);
  }

  for (const [, group] of coordMap) {
    if (group.length <= 1) continue;
    const keeper = group[0];
    for (let i = 1; i < group.length; i++) {
      const redundant = group[i];
      mergedPairs.push({ original: redundant.id, kept: keeper.id });
      nodes.delete(redundant.id);
    }
  }

  for (const pair of mergedPairs) {
    pushDelta(DeltaType.MERGE_NODES, { original: pair.original, kept: pair.kept });
  }

  pushDelta(DeltaType.NORMALIZE_COMPLETE, { merged_count: mergedPairs.length });
  return mergedPairs;
}

// ---- 快照 ----

export function snapshot(description: string = ''): number {
  const id = nextSnapshotId++;
  const state = {
    nodes: Array.from(nodes.values()).map(n => ({ ...n, coords: n.coords.map(c => ({ ...c })) })),
    constraints: Array.from(constraints.values()).map(c => ({ ...c, participants: [...c.participants] })),
  };
  snapshots.set(id, state);
  return id;
}

export function restore(snapshotId: number): boolean {
  const state = snapshots.get(snapshotId);
  if (!state) return false;

  nodes.clear();
  constraints.clear();
  for (const n of state.nodes) nodes.set(n.id, n);
  for (const c of state.constraints) constraints.set(c.id, c);

  nextNodeId = Math.max(...Array.from(nodes.keys()), 0) + 1;
  nextConstraintId = Math.max(...Array.from(constraints.keys()), 0) + 1;

  return true;
}

// ---- 增量日志 ----

export function collectDelta(): Delta {
  const records = deltaBuffer.slice(Number(deltaReadSeq));
  const result: Delta = {
    records,
    from_seq: Number(deltaReadSeq),
    to_seq: Number(deltaWriteSeq - 1n),
  };
  deltaReadSeq = deltaWriteSeq;
  deltaBuffer.length = 0;
  return result;
}

export function getDeltaReadSeq(): number {
  return Number(deltaReadSeq);
}

export function getDeltaWriteSeq(): number {
  return Number(deltaWriteSeq);
}

// ---- 命令执行 ----

export function execute(cmd: Command): CommandResult {
  try {
    switch (cmd.type) {
      case CommandType.ADD_NODE: {
        const nodeType = cmd.params[0] === 0 ? 'NODE_POINT' as NodeType : 'NODE_SEGMENT' as NodeType;
        const name = cmd.blob ? new TextDecoder().decode(cmd.blob) : `node_${nextNodeId}`;
        const coords: Coord[] = [];
        let i = 1;
        while (i < cmd.params.length) {
          const ct = cmd.params[i];
          if (ct === 0) {
            coords.push(coordRational(cmd.params[i + 1], cmd.params[i + 2]));
            i += 3;
          } else if (ct === 2) {
            coords.push(coordQuadratic(cmd.params[i + 1], cmd.params[i + 2], cmd.params[i + 3]));
            i += 4;
          } else {
            i += 2;
          }
        }
        const id = addNode({ name, coords, type: nodeType });
        return makeSuccessResult(`Node ${id} added`);
      }

      case CommandType.MOVE_NODE: {
        const nodeId = cmd.params[0];
        // decodeCoords: skip type tag, read num/den
        let i = 1;
        const coords: Coord[] = [];
        while (i < cmd.params.length) {
          const ct = cmd.params[i];
          if (ct === 0) {
            coords.push(coordRational(cmd.params[i + 1], cmd.params[i + 2]));
            i += 3;
          } else if (ct === 2) {
            coords.push(coordQuadratic(cmd.params[i + 1], cmd.params[i + 2], cmd.params[i + 3]));
            i += 4;
          } else {
            i += 2;
          }
        }
        moveNode(nodeId, coords);
        return makeSuccessResult(`Node ${nodeId} moved`);
      }

      case CommandType.DELETE_NODE: {
        removeNode(cmd.params[0]);
        return makeSuccessResult(`Node deleted`);
      }

      case CommandType.ADD_CONSTRAINT: {
        const type = codeToConstraintType(cmd.params[0]);
        const participants = cmd.params.slice(1);
        const id = addConstraint(type, participants);
        return makeSuccessResult(`Constraint ${id} added`);
      }

      case CommandType.DELETE_CONSTRAINT: {
        removeConstraint(cmd.params[0]);
        return makeSuccessResult(`Constraint deleted`);
      }

      case CommandType.NORMALIZE: {
        const scopeAware = cmd.params[0] === 1;
        const pairs = normalize(scopeAware);
        return makeSuccessResult(`Normalized: ${pairs.length} pairs merged`);
      }

      case CommandType.SNAPSHOT: {
        const sid = snapshot();
        return makeSuccessResult(`Snapshot ${sid}`);
      }

      case CommandType.RESTORE: {
        const ok = restore(cmd.params[0]);
        return ok ? makeSuccessResult('Restored') : { success: false, error_code: 1, error_message: 'Snapshot not found', snapshot_id: 0, delta_sequence_start: 0, delta_sequence_end: 0 };
      }

      case CommandType.UNDO:
      case CommandType.REDO:
        return makeSuccessResult('Undo/Redo handled by L2');

      default:
        return { success: false, error_code: 99, error_message: `Unknown command type: ${cmd.type}`, snapshot_id: 0, delta_sequence_start: 0, delta_sequence_end: 0 };
    }
  } catch (e: any) {
    return { success: false, error_code: 1, error_message: e.message, snapshot_id: 0, delta_sequence_start: 0, delta_sequence_end: 0 };
  }
}

function makeSuccessResult(msg: string): CommandResult {
  const seq = Number(deltaWriteSeq);
  return { success: true, error_code: 0, error_message: msg, snapshot_id: nextSnapshotId - 1, delta_sequence_start: seq, delta_sequence_end: seq };
}

function codeToConstraintType(code: number): ConstraintType {
  const map: ConstraintType[] = [
    'CONSTR_INCIDENCE', 'CONSTR_BETWEENNESS', 'CONSTR_INTERSECTION',
    'CONSTR_CONTAINMENT', 'CONSTR_CONNECTION', 'CONSTR_SEGMENT',
    'CONSTR_EQUAL', 'CONSTR_PERPENDICULAR', 'CONSTR_PARALLEL', 'CONSTR_COLLINEAR',
  ];
  return map[code] ?? 'CONSTR_INCIDENCE';
}

// ---- 演示数据 ----

export function loadDemoGeometry(): void {
  graphCreate();

  const A = addNode({ name: 'A', coords: [coordRational(100, 1), coordRational(200, 1)] });
  const B = addNode({ name: 'B', coords: [coordRational(400, 1), coordRational(150, 1)] });
  const C = addNode({ name: 'C', coords: [coordRational(300, 1), coordRational(400, 1)] });
  const D = addNode({ name: 'D', coords: [coordRational(150, 1), coordRational(350, 1)] });
  const O = addNode({ name: 'O', coords: [coordRational(250, 1), coordRational(250, 1)] });

  addConstraint('CONSTR_SEGMENT', [A, B], 'AB');
  addConstraint('CONSTR_SEGMENT', [B, C], 'BC');
  addConstraint('CONSTR_SEGMENT', [C, A], 'CA');
  addConstraint('CONSTR_SEGMENT', [A, D], 'AD');
  addConstraint('CONSTR_SEGMENT', [D, C], 'DC');

  addConstraint('CONSTR_EQUAL', [A, B, C], 'equilateral');
  addConstraint('CONSTR_PARALLEL', [A, D, B, C], 'parallel');

  nodes.get(A)!.color = 0xFF22C55E; // green
  nodes.get(B)!.color = 0xFF22C55E;
  nodes.get(C)!.color = 0xFFEAB308; // yellow
  nodes.get(D)!.color = 0xFF3B82F6; // blue
  nodes.get(O)!.color = 0xFF86EFAC; // light green
}

// ---- 状态查询 ----

export function getState(): { node_count: number; constraint_count: number; proof_count: number } {
  return {
    node_count: nodes.size,
    constraint_count: constraints.size,
    proof_count: proofs.size,
  };
}
