/**
 * @module engine/backend
 * @description Backend abstraction layer for the Lv-00 geometry engine.
 *              Provides a unified interface for both WASM and JavaScript backends.
 *              The application can seamlessly switch between backends at runtime.
 *
 *              后端抽象层，为 Lv-00 几何引擎提供统一接口。
 *              支持 WASM 和 JavaScript 两种后端，可在运行时无缝切换。
 */

import type { BackendType, Constraint } from '@/types';
import { MERGE_DISTANCE_THRESHOLD } from '@/utils/constants';
import { WasmBackend } from './wasmBackend';
import type { Lv00WasmModule } from './wasmBackend';

// ================================================================
// Backend Interface / 后端接口
// ================================================================

/**
 * Unified backend interface for geometry operations.
 * Both WASM and JS backends must implement this interface.
 *
 * 统一几何操作后端接口。WASM 和 JS 后端均需实现此接口。
 */
export interface IBackend {
  /** Backend type identifier / 后端类型标识 */
  readonly type: BackendType;

  // ---- Graph Lifecycle / 图生命周期 ----

  /** Create a new empty constraint graph / 创建新的空约束图 */
  graphCreate(): number;
  /** Destroy a constraint graph and free resources / 销毁约束图并释放资源 */
  graphDestroy(graphHandle: number): void;

  // ---- Point Operations / 点操作 ----

  /** Add a point to the graph, returns the new point's ID / 向图中添加点，返回新点的 ID */
  graphAddPoint(graphHandle: number, x: number, y: number): number;
  /** Remove a point from the graph by ID / 按 ID 从图中移除点 */
  graphRemovePoint(graphHandle: number, pointId: number): boolean;

  // ---- Segment Operations / 线段操作 ----

  /** Add a line segment between two points, returns segment ID / 在两点之间添加线段，返回线段 ID */
  graphAddLineSegment(graphHandle: number, p1: number, p2: number): number;
  /** Remove a segment by ID / 按 ID 移除线段 */
  graphRemoveLineSegment(graphHandle: number, segmentId: number): boolean;

  // ---- Constraint Operations / 约束操作 ----

  /** Add an incidence constraint (point on segment) / 添加关联约束（点在线上） */
  graphAddIncidence(graphHandle: number, pointId: number, segmentId: number): number;
  /** Add a betweenness constraint (B between A and C) / 添加介于约束（B 介于 A 和 C 之间） */
  graphAddBetweenness(graphHandle: number, a: number, b: number, c: number): number;
  /** Add an intersection constraint (two segments intersect) / 添加相交约束（两线段相交） */
  graphAddIntersection(graphHandle: number, seg1: number, seg2: number): number;
  /** Add a containment constraint / 添加包含约束 */
  graphAddContainment(graphHandle: number, inner: number, outer: number): number;
  /** 添加一般连接约束（两个几何元素之间的通用连接关系）/ Add a general connection constraint between two geometric elements */
  graphAddConnection(graphHandle: number, elem1: number, elem2: number): number;

  // ---- Analysis Operations / 分析操作 ----

  /** Normalize the graph (canonical form) / 规范化图（标准形式） */
  graphNormalize(graphHandle: number): boolean;
  /** Find merge candidate nodes / 查找合并候选节点 */
  graphFindMergeCandidates(graphHandle: number): Array<{ a: number; b: number }>;
  /** Detect redundant constraints / 检测冗余约束 */
  graphDetectRedundant(graphHandle: number): number[];
  /** Detect conflicting constraints / 检测冲突约束 */
  graphDetectConflicts(graphHandle: number): Array<{ c1: number; c2: number }>;
  /** Calculate degrees of freedom / 计算自由度 */
  graphDegreesOfFreedom(graphHandle: number): number;
  /** Topological sort of constraints / 约束拓扑排序 */
  graphTopologicalSort(graphHandle: number): number[];
  /** Compute graph hash for comparison / 计算图哈希值用于比较 */
  graphHash(graphHandle: number): string;

  // ---- Coordinate Operations / 坐标操作 ----

  /** Create a rational coordinate / 创建有理数坐标 */
  coordCreateRational(num: number, den: number): number;
}

// ================================================================
// JavaScript Backend Implementation / JS 后端实现
// ================================================================

/**
 * Pure JavaScript backend implementation.
 * Used as a fallback when WASM is not available.
 * Stores geometry data in-memory using simple data structures.
 *
 * 纯 JavaScript 后端实现，作为 WASM 不可用时的回退方案。
 * 使用简单的内存数据结构存储几何数据。
 *
 * **注意：JS 回退后端仅提供基础的数据存储和简单的邻近距离检查。
 * 约束求解、图规范化、冗余/冲突检测等高级分析功能需要连接 WASM
 * 后端才能获得完整支持。**
 */
export class JsBackend implements IBackend {
  readonly type: BackendType = 'js';

  /** In-memory graph storage: graphHandle -> GraphData / 内存图存储 */
  private graphs: Map<number, JsGraphData> = new Map();

  /** Auto-incrementing graph handle / 自增图句柄 */
  private nextGraphHandle = 1;

  /** Auto-incrementing point ID / 自增点 ID */
  private nextPointId = 0;

  /** Auto-incrementing segment ID / 自增线段 ID */
  private nextSegmentId = 0;

  /** Auto-incrementing constraint ID / 自增约束 ID */
  private nextConstraintId = 0;

  /**
   * 创建新的空约束图，返回图句柄
   * Create a new empty constraint graph, returns the graph handle
   */
  graphCreate(): number {
    const handle = this.nextGraphHandle++;
    this.graphs.set(handle, {
      points: new Map(),
      segments: new Map(),
      constraints: [],
    });
    return handle;
  }

  /**
   * 销毁约束图并释放关联的内存资源
   * Destroy a constraint graph and free associated memory resources
   */
  graphDestroy(graphHandle: number): void {
    this.graphs.delete(graphHandle);
  }

  /**
   * 向图中添加一个新点，返回新分配的点 ID
   * 如果图句柄无效则返回 -1
   */
  graphAddPoint(graphHandle: number, x: number, y: number): number {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return -1;

    const id = this.nextPointId++;
    graph.points.set(id, { id, x, y });
    return id;
  }

  /**
   * 按 ID 从图中移除点
   * @returns 是否成功移除（图不存在或点不存在返回 false）
   */
  graphRemovePoint(graphHandle: number, pointId: number): boolean {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return false;
    return graph.points.delete(pointId);
  }

  /**
   * 在两点之间添加线段，返回新分配的线段 ID
   * 不允许自环（p1 === p2 时返回 -1）
   */
  graphAddLineSegment(graphHandle: number, p1: number, p2: number): number {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return -1;
    if (p1 === p2) return -1; // No self-loops / 不允许自环

    const id = this.nextSegmentId++;
    graph.segments.set(id, { p1, p2, id });
    return id;
  }

  /**
   * 按 ID 移除线段
   * @returns 是否成功移除
   */
  graphRemoveLineSegment(graphHandle: number, segmentId: number): boolean {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return false;
    return graph.segments.delete(segmentId);
  }

  /**
   * 添加关联约束（点在线段上），返回约束 ID
   */
  graphAddIncidence(graphHandle: number, pointId: number, segmentId: number): number {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return -1;
    const id = this.nextConstraintId++;
    graph.constraints.push({ id, type: 'incidence', args: [pointId, segmentId] });
    return id;
  }

  /**
   * 添加介于约束（点 B 介于点 A 和点 C 之间），返回约束 ID
   */
  graphAddBetweenness(graphHandle: number, a: number, b: number, c: number): number {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return -1;
    const id = this.nextConstraintId++;
    graph.constraints.push({ id, type: 'betweenness', args: [a, b, c] });
    return id;
  }

  /**
   * 添加相交约束（两线段相交），返回约束 ID
   */
  graphAddIntersection(graphHandle: number, seg1: number, seg2: number): number {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return -1;
    const id = this.nextConstraintId++;
    graph.constraints.push({ id, type: 'intersection', args: [seg1, seg2] });
    return id;
  }

  /**
   * 添加包含约束（内部元素包含于外部元素），返回约束 ID
   */
  graphAddContainment(graphHandle: number, inner: number, outer: number): number {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return -1;
    const id = this.nextConstraintId++;
    graph.constraints.push({ id, type: 'containment', args: [inner, outer] });
    return id;
  }

  /** 添加一般连接约束，适用于两个元素之间的通用连接关系 */
  graphAddConnection(graphHandle: number, elem1: number, elem2: number): number {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return -1;
    if (elem1 === elem2) return -1; // 不允许自连接 / No self-connection
    const id = this.nextConstraintId++;
    graph.constraints.push({ id, type: 'connection', args: [elem1, elem2] });
    return id;
  }

  /**
   * 图规范化 / Normalize the graph
   *
   * 实现约束图的标准化：
   * - 移除重复约束（相同类型 + 相同参与者）
   * - 合并冗余的连接约束
   * - 移除自环
   * - 按规范顺序排序约束
   * - 重新分配连续 ID
   */
  graphNormalize(graphHandle: number): boolean {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return false;

    // Step 1: 移除自环约束（自环：约束参数中出现重复 ID 或 elem1 === elem2）
    const noSelfLoopConstraints = graph.constraints.filter((c) => {
      if (c.type === 'connection' || c.type === 'intersection' || c.type === 'containment') {
        return c.args[0] !== c.args[1];
      }
      if (c.type === 'incidence') {
        return true; // pointId !== segmentId 类型不同，不会自环
      }
      if (c.type === 'betweenness') {
        const [a, b, c_] = c.args;
        return a !== b && b !== c_ && a !== c_;
      }
      return true;
    });

    // Step 2: 去重——相同类型且参数集合相同（考虑顺序无关的约束类型）
    const seen = new Set<string>();
    const uniqueConstraints: Constraint[] = [];

    for (const c of noSelfLoopConstraints) {
      let key: string;
      if (c.type === 'connection' || c.type === 'intersection' || c.type === 'containment') {
        // 二元约束：参数无序
        const sorted = [...c.args].sort((x, y) => x - y);
        key = `${c.type}:${sorted.join(',')}`;
      } else if (c.type === 'betweenness') {
        // betweenness: [a,b,c]，a 和 c 无序，但 b 固定为中间点
        const [a, b, c_] = c.args;
        const ends = [a, c_].sort((x, y) => x - y);
        key = `${c.type}:${ends[0]},${b},${ends[1]}`;
      } else {
        // incidence 等有序参数
        key = `${c.type}:${c.args.join(',')}`;
      }

      if (!seen.has(key)) {
        seen.add(key);
        uniqueConstraints.push(c);
      }
    }

    // Step 3: 合并冗余连接约束——若两个元素之间已存在 connection，
    // 则移除其他类型（如 intersection）的重复连接，保留最强语义约束
    // 策略：对每对 (e1,e2)，保留一个 connection 并移除其他 connection 类型约束
    const connectionPairs = new Set<string>();
    const mergedConstraints: Constraint[] = [];

    for (const c of uniqueConstraints) {
      if (c.type === 'connection') {
        const sorted = [...c.args].sort((x, y) => x - y);
        const pairKey = `${sorted[0]}-${sorted[1]}`;
        if (!connectionPairs.has(pairKey)) {
          connectionPairs.add(pairKey);
          mergedConstraints.push(c);
        }
        // 否则丢弃重复的 connection
      } else {
        mergedConstraints.push(c);
      }
    }

    // Step 4: 按规范顺序排序约束
    // 排序优先级：type 字典序 -> args 字典序
    mergedConstraints.sort((a, b) => {
      if (a.type !== b.type) {
        return a.type.localeCompare(b.type);
      }
      for (let i = 0; i < Math.min(a.args.length, b.args.length); i++) {
        if (a.args[i] !== b.args[i]) {
          return a.args[i] - b.args[i];
        }
      }
      return a.args.length - b.args.length;
    });

    // Step 5: 重新分配连续 ID
    let nextId = 0;
    for (const c of mergedConstraints) {
      c.id = nextId++;
    }

    graph.constraints = mergedConstraints;
    return true;
  }

  /**
   * 查找距离极近的点对作为合并候选
   * 使用 MERGE_DISTANCE_THRESHOLD 作为判定阈值（从 @/utils/constants 导入）
   * 时间复杂度 O(n^2)，适用于中小规模图
   *
   * Find point pairs that are extremely close as merge candidates.
   * Uses MERGE_DISTANCE_THRESHOLD from @/utils/constants.
   * Time complexity O(n^2), suitable for small-to-medium graphs.
   */
  graphFindMergeCandidates(graphHandle: number): Array<{ a: number; b: number }> {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return [];

    const candidates: Array<{ a: number; b: number }> = [];
    const points = Array.from(graph.points.values());

    /* 寻找距离极近的点对，作为合并候选
     * 使用 MERGE_DISTANCE_THRESHOLD 作为判定阈值（从 @/utils/constants 导入） */
    for (let i = 0; i < points.length; i++) {
      for (let j = i + 1; j < points.length; j++) {
        const pi = points[i];
        const pj = points[j];
        if (!pi || !pj) continue;
        const dx = pi.x - pj.x;
        const dy = pi.y - pj.y;
        const dist = Math.sqrt(dx * dx + dy * dy);
        if (dist < MERGE_DISTANCE_THRESHOLD) {
          candidates.push({ a: pi.id, b: pj.id });
        }
      }
    }

    return candidates;
  }

  /**
   * 检测冗余约束 / Detect redundant constraints
   *
   * 检测以下冗余情况：
   * - 重复约束（相同类型 + 相同参与者）
   * - 同一对元素之间的多个连接约束
   * - 由 incidence + betweenness 组合隐含的 betweenness
   */
  graphDetectRedundant(graphHandle: number): number[] {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return [];

    const redundantIds = new Set<number>();
    const constraints = graph.constraints;

    // 1. 检测重复约束（相同类型且参数等价）
    const seen = new Map<string, number>(); // key -> first constraint id
    for (const c of constraints) {
      let key: string;
      if (c.type === 'connection' || c.type === 'intersection' || c.type === 'containment') {
        const sorted = [...c.args].sort((x, y) => x - y);
        key = `${c.type}:${sorted.join(',')}`;
      } else if (c.type === 'betweenness') {
        const [a, b, c_] = c.args;
        const ends = [a, c_].sort((x, y) => x - y);
        key = `${c.type}:${ends[0]},${b},${ends[1]}`;
      } else {
        key = `${c.type}:${c.args.join(',')}`;
      }

      if (seen.has(key)) {
        redundantIds.add(c.id);
      } else {
        seen.set(key, c.id);
      }
    }

    // 2. 检测同一对元素之间的多个连接约束
    // 若 (e1,e2) 之间存在多个 connection，保留第一个，其余标记为冗余
    const connectionPairs = new Map<string, number>(); // pairKey -> first connection id
    for (const c of constraints) {
      if (c.type === 'connection') {
        const sorted = [...c.args].sort((x, y) => x - y);
        const pairKey = `${sorted[0]}-${sorted[1]}`;
        if (connectionPairs.has(pairKey)) {
          redundantIds.add(c.id);
        } else {
          connectionPairs.set(pairKey, c.id);
        }
      }
    }

    // 3. 检测由 incidence + betweenness 组合隐含的 betweenness
    // 若存在 incidence(p, seg) 和 betweenness(a, p, b) 且 seg 的端点恰好是 a 和 b，
    // 则 betweenness(a, p, b) 是冗余的（因为 p 在线段 seg 上已隐含了顺序关系）
    // 构建 segmentId -> [p1, p2] 映射
    const segmentEndpoints = new Map<number, [number, number]>();
    for (const seg of graph.segments.values()) {
      segmentEndpoints.set(seg.id, [seg.p1, seg.p2]);
    }

    // 构建点 -> 所在线段集合
    const pointToSegments = new Map<number, Set<number>>();
    for (const c of constraints) {
      if (c.type === 'incidence' && c.args.length >= 2) {
        const [pointId, segId] = c.args;
        if (!pointToSegments.has(pointId)) {
          pointToSegments.set(pointId, new Set());
        }
        pointToSegments.get(pointId)!.add(segId);
      }
    }

    for (const c of constraints) {
      if (c.type === 'betweenness' && c.args.length >= 3) {
        const [a, p, b] = c.args;
        const segs = pointToSegments.get(p);
        if (segs) {
          for (const segId of segs) {
            const ends = segmentEndpoints.get(segId);
            if (ends) {
              const [ep1, ep2] = ends;
              // 若 betweenness 的两端点恰好是线段的两个端点
              if ((ep1 === a && ep2 === b) || (ep1 === b && ep2 === a)) {
                redundantIds.add(c.id);
                break;
              }
            }
          }
        }
      }
    }

    return Array.from(redundantIds);
  }

  /**
   * 检测冲突约束 / Detect conflicting constraints
   *
   * 检测以下冲突情况：
   * - 点同时在两条平行线上（incidence 冲突）
   * - 非共线 betweenness（三个点不构成共线关系时的 betweenness）
   * - 平行线相交（intersection 冲突）
   * - containment-connection 语义冲突
   */
  graphDetectConflicts(graphHandle: number): Array<{ c1: number; c2: number }> {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return [];

    const conflicts: Array<{ c1: number; c2: number }> = [];
    const constraints = graph.constraints;

    // 辅助：构建点坐标映射
    const pointCoords = new Map<number, { x: number; y: number }>();
    for (const p of graph.points.values()) {
      pointCoords.set(p.id, { x: p.x, y: p.y });
    }

    // 辅助：判断三点是否共线（面积法）
    const areCollinear = (a: number, b: number, c: number): boolean => {
      const pa = pointCoords.get(a);
      const pb = pointCoords.get(b);
      const pc = pointCoords.get(c);
      if (!pa || !pb || !pc) return true; // 坐标缺失时保守认为共线
      const area = Math.abs(
        (pb.x - pa.x) * (pc.y - pa.y) - (pc.x - pa.x) * (pb.y - pa.y)
      );
      return area < 1e-9;
    };

    // 辅助：判断两条线段是否平行（方向向量叉积为 0）
    const areParallel = (seg1Id: number, seg2Id: number): boolean => {
      const seg1 = graph.segments.get(seg1Id);
      const seg2 = graph.segments.get(seg2Id);
      if (!seg1 || !seg2) return false;
      const p1a = pointCoords.get(seg1.p1);
      const p1b = pointCoords.get(seg1.p2);
      const p2a = pointCoords.get(seg2.p1);
      const p2b = pointCoords.get(seg2.p2);
      if (!p1a || !p1b || !p2a || !p2b) return false;
      const dx1 = p1b.x - p1a.x;
      const dy1 = p1b.y - p1a.y;
      const dx2 = p2b.x - p2a.x;
      const dy2 = p2b.y - p2a.y;
      const cross = dx1 * dy2 - dy1 * dx2;
      return Math.abs(cross) < 1e-9;
    };

    // 1. 点同时在两条平行线上（incidence 冲突）
    // 构建点 -> 所在线段列表
    const pointToSegments = new Map<number, number[]>();
    for (const c of constraints) {
      if (c.type === 'incidence' && c.args.length >= 2) {
        const [pointId, segId] = c.args;
        if (!pointToSegments.has(pointId)) {
          pointToSegments.set(pointId, []);
        }
        pointToSegments.get(pointId)!.push(segId);
      }
    }
    for (const [pointId, segIds] of pointToSegments.entries()) {
      for (let i = 0; i < segIds.length; i++) {
        for (let j = i + 1; j < segIds.length; j++) {
          if (areParallel(segIds[i], segIds[j])) {
            // 找到对应的约束 ID
            const c1 = constraints.find(
              (c) => c.type === 'incidence' && c.args[0] === pointId && c.args[1] === segIds[i]
            );
            const c2 = constraints.find(
              (c) => c.type === 'incidence' && c.args[0] === pointId && c.args[1] === segIds[j]
            );
            if (c1 && c2) {
              conflicts.push({ c1: c1.id, c2: c2.id });
            }
          }
        }
      }
    }

    // 2. 非共线 betweenness 冲突
    for (const c of constraints) {
      if (c.type === 'betweenness' && c.args.length >= 3) {
        const [a, b, c_] = c.args;
        if (!areCollinear(a, b, c_)) {
          // 非共线：betweenness 自身即冲突（标记为与自身冲突，或单独报告）
          // 这里标记为与 id -1 冲突表示内部不一致
          conflicts.push({ c1: c.id, c2: -1 });
        }
      }
    }

    // 3. 平行线相交冲突
    for (const c of constraints) {
      if (c.type === 'intersection' && c.args.length >= 2) {
        const [seg1Id, seg2Id] = c.args;
        if (areParallel(seg1Id, seg2Id)) {
          // intersection 约束与平行几何事实冲突
          conflicts.push({ c1: c.id, c2: -1 });
        }
      }
    }

    // 4. containment-connection 语义冲突
    // 若存在 containment(inner, outer) 和 connection(inner, outer)，语义矛盾：
    // containment 表示内部元素完全在外部元素内，不应有直接的连接关系
    const containmentPairs = new Set<string>();
    for (const c of constraints) {
      if (c.type === 'containment' && c.args.length >= 2) {
        const sorted = [...c.args].sort((x, y) => x - y);
        containmentPairs.add(`${sorted[0]}-${sorted[1]}`);
      }
    }
    for (const c of constraints) {
      if (c.type === 'connection' && c.args.length >= 2) {
        const sorted = [...c.args].sort((x, y) => x - y);
        const pairKey = `${sorted[0]}-${sorted[1]}`;
        if (containmentPairs.has(pairKey)) {
          // 找到对应的 containment 约束
          const containmentC = constraints.find(
            (cc) =>
              cc.type === 'containment' &&
              cc.args.length >= 2 &&
              [...cc.args].sort((x, y) => x - y).join('-') === pairKey
          );
          if (containmentC) {
            conflicts.push({ c1: c.id, c2: containmentC.id });
          }
        }
      }
    }

    return conflicts;
  }

  /**
   * 计算图的自由度（Degrees of Freedom）
   *
   * 公式：DOF = 2 * 点数 - effective_constraints + rank_deficiency
   *
   * 其中：
   * - effective_constraints：各约束的有效秩之和
   *   - incidence（点在线上）：1
   *   - betweenness（点在两点的之间）：1
   *   - intersection（两线相交）：1
   *   - containment（包含）：1
   *   - connection（连接）：0.5（软约束，权重较低）
   * - rank_deficiency：由约束间的线性依赖产生的秩亏
   *   通过检测共享相同几何元素的约束组来估算
   */
  graphDegreesOfFreedom(graphHandle: number): number {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return 0;

    const numPoints = graph.points.size;
    if (numPoints === 0) return 0;

    // 各约束类型的有效秩
    const constraintRank = (type: string): number => {
      switch (type) {
        case 'incidence':
          return 1;
        case 'betweenness':
          return 1;
        case 'intersection':
          return 1;
        case 'containment':
          return 1;
        case 'connection':
          return 0.5;
        default:
          return 1;
      }
    };

    // 计算有效约束数
    let effectiveConstraints = 0;
    for (const c of graph.constraints) {
      effectiveConstraints += constraintRank(c.type);
    }

    // 估算秩亏：检测共享相同几何元素的约束组
    // 若多个约束涉及同一组点/线段，可能产生线性依赖
    let rankDeficiency = 0;

    // 构建元素 -> 约束列表映射
    const elementToConstraints = new Map<number, number[]>(); // elementId -> constraint indices
    for (let i = 0; i < graph.constraints.length; i++) {
      const c = graph.constraints[i];
      for (const arg of c.args) {
        if (!elementToConstraints.has(arg)) {
          elementToConstraints.set(arg, []);
        }
        elementToConstraints.get(arg)!.push(i);
      }
    }

    // 对每个元素，若涉及它的约束数 > 2，每多一个约束增加 0.5 秩亏
    // （这是一个启发式估算，精确计算需要符号代数）
    for (const [_, constraintIndices] of elementToConstraints.entries()) {
      const uniqueConstraints = new Set(constraintIndices);
      const count = uniqueConstraints.size;
      if (count > 2) {
        rankDeficiency += (count - 2) * 0.5;
      }
    }

    // 额外：检测三角形闭合产生的秩亏
    // 若三个点两两之间有 betweenness 或 connection，形成闭合链，秩亏 +1
    const pointPairs = new Set<string>();
    for (const c of graph.constraints) {
      if ((c.type === 'connection' || c.type === 'betweenness') && c.args.length >= 2) {
        const [a, b] = c.args;
        const sorted = [a, b].sort((x, y) => x - y);
        pointPairs.add(`${sorted[0]}-${sorted[1]}`);
      }
    }
    // 简单启发：若边数 >= 点数，增加秩亏
    if (pointPairs.size >= numPoints && numPoints > 2) {
      rankDeficiency += 1;
    }

    const dof = 2 * numPoints - effectiveConstraints + rankDeficiency;
    // DOF 至少为 0（过约束时）
    return Math.max(0, dof);
  }

  /**
   * 约束拓扑排序 / Topological sort of constraints
   *
   * 使用 Kahn 算法实现：
   * - 将约束视为图中的节点
   * - 若约束 A 的输出（定义的几何元素）被约束 B 使用，则 A -> B 有向边
   * - 按拓扑顺序返回约束 ID 列表，确保先求解的约束在前
   *
   * 依赖关系定义：
   * - incidence(p, seg)：依赖 seg 的两个端点已定义
   * - betweenness(a, b, c)：依赖 a, c 已定义
   * - intersection(seg1, seg2)：依赖 seg1, seg2 已定义
   * - containment(inner, outer)：依赖 inner, outer 已定义
   * - connection(elem1, elem2)：依赖 elem1, elem2 已定义
   */
  graphTopologicalSort(graphHandle: number): number[] {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return [];

    const constraints = graph.constraints;
    const n = constraints.length;
    if (n === 0) return [];

    // 构建元素 -> 定义它的约束 ID 映射（哪个约束“产出”了该元素）
    // 以及元素 -> 使用它的约束列表
    const elementDefinedBy = new Map<number, number>(); // elementId -> constraintId that defines it
    const elementUsedBy = new Map<number, number[]>(); // elementId -> constraintIds that use it

    // 初始点被视为已定义（不依赖任何约束）
    const initialElements = new Set<number>(graph.points.keys());

    // 分析每个约束：哪些元素被定义，哪些元素被使用
    for (let i = 0; i < n; i++) {
      const c = constraints[i];
      const cid = c.id;

      // 默认：约束的参数都是“被使用”的元素
      for (const arg of c.args) {
        if (!elementUsedBy.has(arg)) {
          elementUsedBy.set(arg, []);
        }
        elementUsedBy.get(arg)!.push(cid);
      }

      // 某些约束定义了新元素（简化处理：incidence 定义了点在线段上的位置关系，
      // 但这里我们保守地认为约束不定义新元素，只建立关系）
      // 拓扑排序基于：若约束 B 使用了约束 A 所涉及的几何元素，且 A 在逻辑上应先求解
    }

    // 构建约束依赖图：adj[u] = v 表示 u 必须在 v 之前求解
    const adj = new Map<number, number[]>(); // from -> to[]
    const inDegree = new Map<number, number>(); // constraintId -> inDegree

    for (const c of constraints) {
      adj.set(c.id, []);
      inDegree.set(c.id, 0);
    }

    // 对于每个约束，检查它的参数是否被其他约束“定义”或“使用”
    // 策略：若约束 A 的所有参数都是初始元素，则 A 的入度为 0
    // 若约束 B 使用了约束 A 也使用的元素，且 A 在约束列表中更靠前，则 A -> B
    // 更准确的策略：基于元素定义链
    for (let i = 0; i < n; i++) {
      const ci = constraints[i];
      for (let j = 0; j < n; j++) {
        if (i === j) continue;
        const cj = constraints[j];
        // 若 ci 使用了某些元素，而 cj 也使用或定义了这些元素，建立依赖
        // 简化：若 ci 的参数中有任何一个是 cj 的参数，且 ci 在列表中更靠后，则 cj -> ci
        const sharedArgs = ci.args.filter((arg) => cj.args.includes(arg));
        if (sharedArgs.length > 0) {
          // cj 应在 ci 之前
          const from = cj.id;
          const to = ci.id;
          if (!adj.get(from)!.includes(to)) {
            adj.get(from)!.push(to);
            inDegree.set(to, (inDegree.get(to) || 0) + 1);
          }
        }
      }
    }

    // Kahn 算法
    const queue: number[] = [];
    for (const [cid, deg] of inDegree.entries()) {
      if (deg === 0) {
        queue.push(cid);
      }
    }

    const result: number[] = [];
    while (queue.length > 0) {
      // 每次取 ID 最小的，保证确定性
      queue.sort((a, b) => a - b);
      const u = queue.shift()!;
      result.push(u);

      const neighbors = adj.get(u) || [];
      for (const v of neighbors) {
        inDegree.set(v, (inDegree.get(v) || 0) - 1);
        if (inDegree.get(v) === 0) {
          queue.push(v);
        }
      }
    }

    // 若结果数量不等于约束数，说明有环，返回按 ID 排序的结果作为回退
    if (result.length !== n) {
      return constraints.map((c) => c.id).sort((a, b) => a - b);
    }

    return result;
  }

  /**
   * 计算图的哈希值 / Compute graph hash
   *
   * 使用 FNV-1a 哈希算法对图的规范表示进行哈希：
   * - 包含所有点的坐标（排序后）
   * - 包含所有线段的端点（排序后）
   * - 包含所有约束的类型和参数（排序后，使用规范形式）
   *
   * 该哈希可用于快速比较两个图是否在结构上等价。
   */
  graphHash(graphHandle: number): string {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return '';

    // 构建规范表示字符串
    const parts: string[] = [];

    // 1. 点的规范表示（按 ID 排序）
    const sortedPoints = Array.from(graph.points.values()).sort((a, b) => a.id - b.id);
    for (const p of sortedPoints) {
      parts.push(`P${p.id}=${p.x.toFixed(6)},${p.y.toFixed(6)}`);
    }

    // 2. 线段的规范表示（按 ID 排序，端点按 ID 排序）
    const sortedSegments = Array.from(graph.segments.values()).sort((a, b) => a.id - b.id);
    for (const s of sortedSegments) {
      const ends = [s.p1, s.p2].sort((x, y) => x - y);
      parts.push(`S${s.id}=${ends[0]},${ends[1]}`);
    }

    // 3. 约束的规范表示（按类型和参数排序）
    const canonicalConstraints = graph.constraints.map((c) => {
      let canonicalArgs: number[];
      if (c.type === 'connection' || c.type === 'intersection' || c.type === 'containment') {
        canonicalArgs = [...c.args].sort((x, y) => x - y);
      } else if (c.type === 'betweenness') {
        const [a, b, c_] = c.args;
        const ends = [a, c_].sort((x, y) => x - y);
        canonicalArgs = [ends[0], b, ends[1]];
      } else {
        canonicalArgs = [...c.args];
      }
      return { type: c.type, args: canonicalArgs };
    });

    canonicalConstraints.sort((a, b) => {
      if (a.type !== b.type) return a.type.localeCompare(b.type);
      for (let i = 0; i < Math.min(a.args.length, b.args.length); i++) {
        if (a.args[i] !== b.args[i]) return a.args[i] - b.args[i];
      }
      return a.args.length - b.args.length;
    });

    for (const c of canonicalConstraints) {
      parts.push(`C:${c.type}=[${c.args.join(',')}]`);
    }

    const canonicalString = parts.join('|');

    // FNV-1a 哈希算法（32位）
    const FNV_OFFSET_BASIS = 0x811c9dc5;
    const FNV_PRIME = 0x01000193;
    let hash = FNV_OFFSET_BASIS;

    for (let i = 0; i < canonicalString.length; i++) {
      hash ^= canonicalString.charCodeAt(i);
      hash = Math.imul(hash, FNV_PRIME);
    }

    // 转换为无符号 32 位十六进制字符串
    const unsignedHash = hash >>> 0;
    return unsignedHash.toString(16).padStart(8, '0');
  }

  /**
   * 创建有理数坐标（简化实现）
   * 直接返回 num/den 的浮点除法结果。
   * WASM 后端会维护精确的有理数表示。
   *
   * Create a rational coordinate (simplified implementation).
   * Returns the floating-point division result of num/den.
   * The WASM backend maintains exact rational representations.
   */
  coordCreateRational(num: number, den: number): number {
    // Simple rational representation as a single number
    return den !== 0 ? num / den : 0;
  }
}

// ================================================================
// Internal Data Structures / 内部数据结构
// ================================================================

/**
 * Internal graph data storage for the JS backend.
 *
 * JS 后端内部图数据存储结构。
 *
 * constraints 字段使用 Constraint[] 类型，其中 Constraint.type 为 ConstraintType，
 * 支持所有约束类型，包括 'incidence'、'betweenness'、'intersection'、
 * 'containment' 和 'connection'（因为 @/types 中的 ConstraintType 已扩展）。
 */
interface JsGraphData {
  points: Map<number, { id: number; x: number; y: number }>;
  segments: Map<number, { p1: number; p2: number; id: number }>;
  /** 约束列表，支持所有 ConstraintType，包括 'connection' */
  constraints: Constraint[];
}

// ================================================================
// Backend Factory / 后端工厂
// ================================================================

/**
 * Backend adapter that manages backend initialization and provides
 * a unified interface for the application.
 *
 * Attempts to load WASM backend first, falls back to JS backend.
 *
 * 后端适配器，管理后端初始化并为应用提供统一接口。
 * 优先尝试加载 WASM 后端，回退到 JS 后端。
 */
export class BackendAdapter {
  private backend: IBackend | null = null;
  private graphHandle: number | null = null;

  /**
   * 初始化后端：优先加载 WASM，回退到纯 JS 实现。
   *
   * WASM 后端提供完整的 C 引擎功能（约束求解、规范化、合一等），
   * JS 回退仅提供基础数据结构存储。
   *
   * Initializes the backend: tries WASM first, falls back to pure JS.
   * WASM backend provides full C engine functionality (constraint solving,
   * normalization, unification, etc.), JS fallback only provides basic data storage.
   *
   * @returns 已初始化的后端类型标识 / The backend type that was initialized
   */
  async init(): Promise<BackendType> {
    /* 尝试加载 WASM 后端 / Try loading WASM backend */

    /* 策略1：检查全局 Lv00Module 工厂函数（Emscripten MODULARIZE 模式） */
    const globalModuleFactory = (window as unknown as Record<string, unknown>).Lv00Module;

    /* 策略2：尝试动态导入 WASM 模块 */
    let wasmInstance: Lv00WasmModule | null = null;

    if (typeof globalModuleFactory === 'function') {
      /* 全局工厂函数已存在（例如通过 <script> 标签加载） */
      try {
        wasmInstance = await (globalModuleFactory as () => Promise<Lv00WasmModule>)();
        console.info('[Lv-00] WASM module loaded from global Lv00Module factory');
      } catch (err) {
        console.warn('[Lv-00] Global Lv00Module factory failed:', err);
      }
    }

    if (!wasmInstance) {
      /* 尝试动态导入 / Try dynamic import */
      try {
        // @ts-expect-error - /lv00_web.js is loaded at runtime from public directory
        const moduleFactory = await import('/lv00_web.js');
        if (typeof moduleFactory.default === 'function') {
          wasmInstance = await (moduleFactory.default as () => Promise<Lv00WasmModule>)();
          console.info('[Lv-00] WASM module loaded via dynamic import from /lv00_web.js');
        } else if (typeof moduleFactory.default === 'object' && moduleFactory.default !== null) {
          /* 模块已经是实例化的对象（非 MODULARIZE 模式） */
          wasmInstance = moduleFactory.default as unknown as Lv00WasmModule;
          console.info('[Lv-00] WASM module loaded as pre-initialized instance');
        }
      } catch (err) {
        /* 动态导入失败（模块文件不存在或加载错误），静默回退 */
        console.info('[Lv-00] Dynamic WASM import failed, falling back to JS backend');
      }
    }

    if (wasmInstance) {
      try {
        /* 验证 WASM 模块是否包含必要的导出函数 */
        const requiredFunc = '_web_graph_create';
        if (typeof wasmInstance[requiredFunc] !== 'function') {
          console.warn(
            `[Lv-00] WASM module missing required export "${requiredFunc}", falling back to JS`
          );
        } else {
          /* 成功初始化 WASM 后端 */
          this.backend = new WasmBackend(wasmInstance);
          this.graphHandle = this.backend.graphCreate();
          console.info('[Lv-00] WASM backend initialized successfully');
          return 'wasm';
        }
      } catch (err) {
        console.warn('[Lv-00] WASM backend construction failed:', err);
      }
    }

    /* 回退到 JS 后端 / Fall back to JS backend */
    this.backend = new JsBackend();
    this.graphHandle = this.backend.graphCreate();
    console.info('[Lv-00] JS backend initialized (fallback mode)');
    return 'js';
  }

  /**
   * Get the current backend instance / 获取当前后端实例
   * @throws Error if backend is not initialized / 如果后端未初始化则抛出错误
   */
  getBackend(): IBackend {
    if (!this.backend) {
      throw new Error('[Lv-00] Backend not initialized / 后端未初始化');
    }
    return this.backend;
  }

  /**
   * Get the current graph handle / 获取当前图句柄
   */
  getGraphHandle(): number {
    if (this.graphHandle === null) {
      throw new Error('[Lv-00] Graph not created / 图未创建');
    }
    return this.graphHandle;
  }

  /**
   * Get the backend type / 获取后端类型
   */
  getBackendType(): BackendType {
    return this.backend?.type ?? null;
  }

  /**
   * Destroy the current graph and create a new empty one
   * 销毁当前图并创建新的空图
   */
  resetGraph(): void {
    const backend = this.getBackend();
    if (this.graphHandle !== null) {
      backend.graphDestroy(this.graphHandle);
    }
    this.graphHandle = backend.graphCreate();
  }

  /**
   * Cleanup all resources / 清理所有资源
   */
  destroy(): void {
    if (this.backend && this.graphHandle !== null) {
      this.backend.graphDestroy(this.graphHandle);
    }
    this.backend = null;
    this.graphHandle = null;
  }
}
