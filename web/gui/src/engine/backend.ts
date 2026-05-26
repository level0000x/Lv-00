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
import { logger } from '../services/logger'; // [安全修复 H-01] 使用统一 logger 替代 console.log/warn/info
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
   * 【JS 回退后端限制 / JS Fallback Limitation】
   * 当前 JS 回退后端不支持完整的图规范化计算，始终返回 true。
   * 图规范化涉及复杂的符号代数推理，需要 C 引擎 / WASM 后端提供完整支持。
   * 请连接 WASM 后端以获取完整的图规范化功能。
   *
   * The current JS fallback backend does not support full graph normalization
   * and always returns true. Graph normalization requires complex symbolic
   * algebraic reasoning that needs the C engine / WASM backend.
   * Please connect the WASM backend for complete graph normalization support.
   */
  graphNormalize(_graphHandle: number): boolean {
    // [安全修复 H-01] 使用统一 logger 替代 console.warn，便于日志级别管理和统一采集
    logger.warn(
      'graphNormalize() — JS 回退后端限制 | JS Fallback Limitation\n' +
      '  当前 JS 回退后端不支持完整的图规范化计算，始终返回 true。\n' +
      '  The JS fallback backend does not support full graph normalization and always returns true.\n' +
      '  请连接 WASM 后端以获取完整功能。\n' +
      '  Please connect the WASM backend for complete functionality.'
    );
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
   * 【JS 回退后端限制 / JS Fallback Limitation】
   * 当前 JS 回退后端不支持冗余约束检测，始终返回空数组。
   * 冗余约束检测涉及约束系统的依赖分析和消解，需要 C 引擎 / WASM 后端提供完整支持。
   * 请连接 WASM 后端以获取完整的冗余检测功能。
   *
   * The current JS fallback backend does not support redundant constraint detection
   * and always returns an empty array. Redundant constraint detection requires
   * dependency analysis and resolution of constraint systems, which needs the
   * C engine / WASM backend.
   * Please connect the WASM backend for complete redundant detection support.
   */
  graphDetectRedundant(_graphHandle: number): number[] {
    // [安全修复 H-01] 使用统一 logger 替代 console.warn
    logger.warn(
      'graphDetectRedundant() — JS 回退后端限制 | JS Fallback Limitation\n' +
      '  当前 JS 回退后端不支持冗余约束检测，始终返回空数组。\n' +
      '  The JS fallback backend does not support redundant constraint detection and always returns an empty array.\n' +
      '  请连接 WASM 后端以获取完整功能。\n' +
      '  Please connect the WASM backend for complete functionality.'
    );
    return [];
  }

  /**
   * 检测冲突约束 / Detect conflicting constraints
   *
   * 【JS 回退后端限制 / JS Fallback Limitation】
   * 当前 JS 回退后端不支持约束冲突检测，始终返回空数组。
   * 约束冲突检测涉及约束满足性和一致性检查，需要 C 引擎 / WASM 后端提供完整支持。
   * 请连接 WASM 后端以获取完整的冲突检测功能。
   *
   * The current JS fallback backend does not support constraint conflict detection
   * and always returns an empty array. Constraint conflict detection requires
   * satisfiability and consistency checking, which needs the C engine / WASM backend.
   * Please connect the WASM backend for complete conflict detection support.
   */
  graphDetectConflicts(_graphHandle: number): Array<{ c1: number; c2: number }> {
    // [安全修复 H-01] 使用统一 logger 替代 console.warn
    logger.warn(
      'graphDetectConflicts() — JS 回退后端限制 | JS Fallback Limitation\n' +
      '  当前 JS 回退后端不支持约束冲突检测，始终返回空数组。\n' +
      '  The JS fallback backend does not support constraint conflict detection and always returns an empty array.\n' +
      '  请连接 WASM 后端以获取完整功能。\n' +
      '  Please connect the WASM backend for complete functionality.'
    );
    return [];
  }

  /**
   * 计算图的自由度（简化公式）
   * DOF = 2 * 点数 - 约束数
   *
   * 【注意】此为简化计算，未考虑约束的秩和线性依赖关系。
   * 精确的自由度计算需要 WASM 后端的支持。
   *
   * Calculate degrees of freedom (simplified formula).
   * Note: This is a simplified calculation that does not account for
   * constraint rank and linear dependencies. Use the WASM backend
   * for accurate DOF computation.
   */
  graphDegreesOfFreedom(graphHandle: number): number {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return 0;
    // DOF = 2 * numPoints - numConstraints (simplified)
    return 2 * graph.points.size - graph.constraints.length;
  }

  /**
   * 约束拓扑排序（简化实现）
   * 当前 JS 回退后端仅按约束添加顺序返回 ID 列表，
   * 未执行真正的拓扑排序。完整实现需要 WASM 后端。
   *
   * Topological sort of constraints (simplified implementation).
   * The JS fallback backend returns constraint IDs in insertion order.
   * Use the WASM backend for proper topological sorting.
   */
  graphTopologicalSort(graphHandle: number): number[] {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return [];
    return graph.constraints.map((c) => c.id);
  }

  /**
   * 计算图的哈希值（简化实现）
   * 基于点数、线段数和约束数生成简单的哈希值，
   * 用于快速比较两个图是否可能相同。
   * 不保证无碰撞，精确哈希需要 WASM 后端。
   *
   * Compute a simple graph hash based on point/segment/constraint counts.
   * This is a lightweight fingerprint for quick comparison, not collision-free.
   * Use the WASM backend for cryptographic-quality graph hashing.
   */
  graphHash(graphHandle: number): string {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return '';

    // Simple hash based on point count and constraint count
    const data = `${graph.points.size}:${graph.segments.size}:${graph.constraints.length}`;
    let hash = 0;
    for (let i = 0; i < data.length; i++) {
      const char = data.charCodeAt(i);
      hash = ((hash << 5) - hash + char) | 0;
    }
    return Math.abs(hash).toString(16);
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
        // [安全修复 H-01] 使用统一 logger 替代 console.info/warn/log
        logger.info('WASM module loaded from global Lv00Module factory');
        logger.info('从全局 Lv00Module 工厂加载 WASM 模块');
      } catch (err) {
        logger.warn('Global Lv00Module factory failed:', err);
        logger.warn('全局 Lv00Module 工厂初始化失败');
      }
    }

    if (!wasmInstance) {
      /* 尝试动态导入 / Try dynamic import */
      try {
        // @ts-expect-error - /lv00_web.js is loaded at runtime from public directory
        const moduleFactory = await import('/lv00_web.js');
        if (typeof moduleFactory.default === 'function') {
          wasmInstance = await (moduleFactory.default as () => Promise<Lv00WasmModule>)();
          logger.info('WASM module loaded via dynamic import from /lv00_web.js');
          logger.info('通过动态导入从 /lv00_web.js 加载 WASM 模块');
        } else if (typeof moduleFactory.default === 'object' && moduleFactory.default !== null) {
          /* 模块已经是实例化的对象（非 MODULARIZE 模式） */
          wasmInstance = moduleFactory.default as unknown as Lv00WasmModule;
          logger.info('WASM module loaded as pre-initialized instance');
          logger.info('WASM 模块已作为预初始化实例加载');
        }
      } catch (err) {
        /* 动态导入失败（模块文件不存在或加载错误），静默回退 */
        logger.info('Dynamic WASM import failed, falling back to JS backend');
        logger.info('动态 WASM 导入失败，回退到 JS 后端');
      }
    }

    if (wasmInstance) {
      try {
        /* 验证 WASM 模块是否包含必要的导出函数 */
        const requiredFunc = '_web_graph_create';
        if (typeof wasmInstance[requiredFunc] !== 'function') {
          logger.warn(
            `WASM module missing required export "${requiredFunc}", falling back to JS`
          );
          logger.warn(`WASM 模块缺少必需的导出函数 "${requiredFunc}"，回退到 JS`);
        } else {
          /* 成功初始化 WASM 后端 */
          this.backend = new WasmBackend(wasmInstance);
          this.graphHandle = this.backend.graphCreate();
          logger.info('WASM backend initialized successfully');
          logger.info('WASM 后端初始化成功');
          return 'wasm';
        }
      } catch (err) {
        logger.warn('WASM backend construction failed:', err);
        logger.warn('WASM 后端构建失败');
      }
    }

    /* 回退到 JS 后端 / Fall back to JS backend */
    this.backend = new JsBackend();
    this.graphHandle = this.backend.graphCreate();
    logger.info('JS backend initialized (fallback mode)');
    logger.info('JS 后端已初始化（回退模式）');
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
