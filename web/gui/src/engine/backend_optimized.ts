/**
 * @module engine/backend
 * @description Backend abstraction layer for the Lv-00 geometry engine (Optimized v3.5.1).
 *              Provides a unified interface for both WASM and JavaScript backends.
 *              The application can seamlessly switch between backends at runtime.
 *
 *              后端抽象层，为 Lv-00 几何引擎提供统一接口。
 *              支持 WASM 和 JavaScript 两种后端，可在运行时无缝切换。
 *
 * @version 3.5.1
 * @author Lv-00 Development Team
 */

import type { BackendType, Constraint } from '@/types';
import { MERGE_DISTANCE_THRESHOLD } from '@/utils/constants';
import { logger } from '../services/logger';
import { WasmBackend } from './wasmBackend';
import type { Lv00WasmModule } from './wasmBackend';

// ================================================================
// Constants / 常量
// ================================================================

/** Default backend initialization timeout in milliseconds */
const DEFAULT_BACKEND_TIMEOUT_MS = 5000;

/** WASM module memory configuration */
const WASM_MEMORY_CONFIG = {
  initial: 32 * 1024 * 1024, // 32MB
  maximum: 256 * 1024 * 1024, // 256MB
} as const;

// ================================================================
// Type Definitions / 类型定义
// ================================================================

/**
 * Backend initialization options
 * 后端初始化选项
 */
export interface BackendOptions {
  /** Preferred backend type / 首选后端类型 */
  preferredBackend?: BackendType;
  /** Timeout for WASM initialization / WASM 初始化超时 */
  wasmTimeoutMs?: number;
  /** Enable debug logging / 启用调试日志 */
  debug?: boolean;
}

/**
 * Backend initialization result
 * 后端初始化结果
 */
export interface BackendInitResult {
  /** Initialized backend type / 已初始化的后端类型 */
  type: BackendType;
  /** Whether WASM is fully functional / WASM 是否完全可用 */
  wasmAvailable: boolean;
  /** Error message if initialization failed / 初始化失败时的错误消息 */
  error?: string;
}

/**
 * Graph data structure for JS backend
 * JS 后端图数据结构
 */
interface JsGraphData {
  points: Map<number, JsPoint>;
  segments: Map<number, JsSegment>;
  constraints: Constraint[];
  /** Graph metadata / 图元数据 */
  metadata: {
    createdAt: number;
    lastModified: number;
  };
}

interface JsPoint {
  id: number;
  x: number;
  y: number;
  /** Optional label / 可选标签 */
  label?: string;
}

interface JsSegment {
  id: number;
  p1: number;
  p2: number;
}

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
  
  /** Backend capabilities / 后端能力 */
  readonly capabilities: BackendCapabilities;

  // ---- Graph Lifecycle / 图生命周期 ----
  graphCreate(): number;
  graphDestroy(graphHandle: number): void;

  // ---- Point Operations / 点操作 ----
  graphAddPoint(graphHandle: number, x: number, y: number): number;
  graphRemovePoint(graphHandle: number, pointId: number): boolean;

  // ---- Segment Operations / 线段操作 ----
  graphAddLineSegment(graphHandle: number, p1: number, p2: number): number;
  graphRemoveLineSegment(graphHandle: number, segmentId: number): boolean;

  // ---- Constraint Operations / 约束操作 ----
  graphAddIncidence(graphHandle: number, pointId: number, segmentId: number): number;
  graphAddBetweenness(graphHandle: number, a: number, b: number, c: number): number;
  graphAddIntersection(graphHandle: number, seg1: number, seg2: number): number;
  graphAddContainment(graphHandle: number, inner: number, outer: number): number;
  graphAddConnection(graphHandle: number, elem1: number, elem2: number): number;

  // ---- Analysis Operations / 分析操作 ----
  graphNormalize(graphHandle: number): boolean;
  graphFindMergeCandidates(graphHandle: number): Array<{ a: number; b: number }>;
  graphDetectRedundant(graphHandle: number): number[];
  graphDetectConflicts(graphHandle: number): Array<{ c1: number; c2: number }>;
  graphDegreesOfFreedom(graphHandle: number): number;
  graphTopologicalSort(graphHandle: number): number[];
  graphHash(graphHandle: number): string;

  // ---- Coordinate Operations / 坐标操作 ----
  coordCreateRational(num: number, den: number): number;
}

/**
 * Backend capabilities flags
 * 后端能力标志
 */
export interface BackendCapabilities {
  /** Supports full graph normalization / 支持完整图规范化 */
  normalization: boolean;
  /** Supports redundant constraint detection / 支持冗余约束检测 */
  redundantDetection: boolean;
  /** Supports conflict detection / 支持冲突检测 */
  conflictDetection: boolean;
  /** Supports exact rational arithmetic / 支持精确有理数运算 */
  exactArithmetic: boolean;
  /** Supports constraint solving / 支持约束求解 */
  constraintSolving: boolean;
}

// ================================================================
// JavaScript Backend Implementation / JS 后端实现
// ================================================================

/**
 * Pure JavaScript backend implementation.
 * Used as a fallback when WASM is not available.
 *
 * 纯 JavaScript 后端实现，作为 WASM 不可用时的回退方案。
 */
export class JsBackend implements IBackend {
  readonly type: BackendType = 'js';
  
  readonly capabilities: BackendCapabilities = {
    normalization: false,
    redundantDetection: false,
    conflictDetection: false,
    exactArithmetic: false,
    constraintSolving: false,
  };

  private graphs: Map<number, JsGraphData> = new Map();
  private nextGraphHandle = 1;
  private nextPointId = 0;
  private nextSegmentId = 0;
  private nextConstraintId = 0;

  /**
   * Create a new empty constraint graph
   * 创建新的空约束图
   */
  graphCreate(): number {
    const handle = this.nextGraphHandle++;
    this.graphs.set(handle, {
      points: new Map(),
      segments: new Map(),
      constraints: [],
      metadata: {
        createdAt: Date.now(),
        lastModified: Date.now(),
      },
    });
    logger.info(`[JsBackend] Created graph ${handle}`);
    return handle;
  }

  /**
   * Destroy a constraint graph
   * 销毁约束图
   */
  graphDestroy(graphHandle: number): void {
    const existed = this.graphs.delete(graphHandle);
    if (existed) {
      logger.info(`[JsBackend] Destroyed graph ${graphHandle}`);
    }
  }

  /**
   * Add a point to the graph
   * 向图中添加点
   */
  graphAddPoint(graphHandle: number, x: number, y: number): number {
    const graph = this.graphs.get(graphHandle);
    if (!graph) {
      logger.warn(`[JsBackend] graphAddPoint: Graph ${graphHandle} not found`);
      return -1;
    }

    const id = this.nextPointId++;
    graph.points.set(id, { id, x, y });
    graph.metadata.lastModified = Date.now();
    return id;
  }

  /**
   * Remove a point from the graph
   * 从图中移除点
   */
  graphRemovePoint(graphHandle: number, pointId: number): boolean {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return false;
    
    const existed = graph.points.delete(pointId);
    if (existed) {
      graph.metadata.lastModified = Date.now();
    }
    return existed;
  }

  /**
   * Add a line segment between two points
   * 在两点之间添加线段
   */
  graphAddLineSegment(graphHandle: number, p1: number, p2: number): number {
    const graph = this.graphs.get(graphHandle);
    if (!graph) {
      logger.warn(`[JsBackend] graphAddLineSegment: Graph ${graphHandle} not found`);
      return -1;
    }
    if (p1 === p2) {
      logger.warn('[JsBackend] graphAddLineSegment: Self-loop not allowed');
      return -1;
    }

    const id = this.nextSegmentId++;
    graph.segments.set(id, { p1, p2, id });
    graph.metadata.lastModified = Date.now();
    return id;
  }

  /**
   * Remove a line segment
   * 移除线段
   */
  graphRemoveLineSegment(graphHandle: number, segmentId: number): boolean {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return false;
    
    const existed = graph.segments.delete(segmentId);
    if (existed) {
      graph.metadata.lastModified = Date.now();
    }
    return existed;
  }

  /**
   * Add an incidence constraint
   * 添加关联约束
   */
  graphAddIncidence(graphHandle: number, pointId: number, segmentId: number): number {
    return this.addConstraint(graphHandle, 'incidence', [pointId, segmentId]);
  }

  /**
   * Add a betweenness constraint
   * 添加介于约束
   */
  graphAddBetweenness(graphHandle: number, a: number, b: number, c: number): number {
    return this.addConstraint(graphHandle, 'betweenness', [a, b, c]);
  }

  /**
   * Add an intersection constraint
   * 添加相交约束
   */
  graphAddIntersection(graphHandle: number, seg1: number, seg2: number): number {
    return this.addConstraint(graphHandle, 'intersection', [seg1, seg2]);
  }

  /**
   * Add a containment constraint
   * 添加包含约束
   */
  graphAddContainment(graphHandle: number, inner: number, outer: number): number {
    return this.addConstraint(graphHandle, 'containment', [inner, outer]);
  }

  /**
   * Add a connection constraint
   * 添加连接约束
   */
  graphAddConnection(graphHandle: number, elem1: number, elem2: number): number {
    if (elem1 === elem2) {
      logger.warn('[JsBackend] graphAddConnection: Self-connection not allowed');
      return -1;
    }
    return this.addConstraint(graphHandle, 'connection', [elem1, elem2]);
  }

  /**
   * Internal method to add a constraint
   * 内部方法：添加约束
   */
  private addConstraint(
    graphHandle: number, 
    type: Constraint['type'], 
    args: number[]
  ): number {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return -1;

    const id = this.nextConstraintId++;
    graph.constraints.push({ id, type, args });
    graph.metadata.lastModified = Date.now();
    return id;
  }

  /**
   * Normalize the graph (JS fallback - always returns true)
   * 图规范化（JS 回退 - 始终返回 true）
   */
  graphNormalize(_graphHandle: number): boolean {
    logger.warn(
      '[JsBackend] graphNormalize: JS fallback does not support full graph normalization. ' +
      'Please use WASM backend for complete functionality.'
    );
    return true;
  }

  /**
   * Find merge candidate nodes
   * 查找合并候选节点
   */
  graphFindMergeCandidates(graphHandle: number): Array<{ a: number; b: number }> {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return [];

    const candidates: Array<{ a: number; b: number }> = [];
    const points = Array.from(graph.points.values());

    // O(n²) distance check - suitable for small graphs
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
   * Detect redundant constraints (JS fallback)
   * 检测冗余约束（JS 回退）
   */
  graphDetectRedundant(_graphHandle: number): number[] {
    logger.warn(
      '[JsBackend] graphDetectRedundant: JS fallback does not support redundant constraint detection.'
    );
    return [];
  }

  /**
   * Detect conflicting constraints (JS fallback)
   * 检测冲突约束（JS 回退）
   */
  graphDetectConflicts(_graphHandle: number): Array<{ c1: number; c2: number }> {
    logger.warn(
      '[JsBackend] graphDetectConflicts: JS fallback does not support conflict detection.'
    );
    return [];
  }

  /**
   * Calculate degrees of freedom (simplified)
   * 计算自由度（简化版）
   */
  graphDegreesOfFreedom(graphHandle: number): number {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return 0;
    // DOF = 2 * numPoints - numConstraints (simplified)
    return 2 * graph.points.size - graph.constraints.length;
  }

  /**
   * Topological sort of constraints (simplified)
   * 约束拓扑排序（简化版）
   */
  graphTopologicalSort(graphHandle: number): number[] {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return [];
    return graph.constraints.map((c) => c.id);
  }

  /**
   * Compute graph hash (simplified)
   * 计算图哈希（简化版）
   */
  graphHash(graphHandle: number): string {
    const graph = this.graphs.get(graphHandle);
    if (!graph) return '';

    const data = `${graph.points.size}:${graph.segments.size}:${graph.constraints.length}`;
    let hash = 0;
    for (let i = 0; i < data.length; i++) {
      const char = data.charCodeAt(i);
      hash = ((hash << 5) - hash + char) | 0;
    }
    return Math.abs(hash).toString(16);
  }

  /**
   * Create a rational coordinate (simplified)
   * 创建有理数坐标（简化版）
   */
  coordCreateRational(num: number, den: number): number {
    return den !== 0 ? num / den : 0;
  }
}

// ================================================================
// Backend Factory / 后端工厂
// ================================================================

/**
 * Backend adapter that manages backend initialization and provides
 * a unified interface for the application.
 *
 * 后端适配器，管理后端初始化并为应用提供统一接口。
 */
export class BackendAdapter {
  private backend: IBackend | null = null;
  private graphHandle: number | null = null;
  private options: BackendOptions;

  constructor(options: BackendOptions = {}) {
    this.options = {
      wasmTimeoutMs: DEFAULT_BACKEND_TIMEOUT_MS,
      debug: false,
      ...options,
    };
  }

  /**
   * Initialize the backend: tries WASM first, falls back to pure JS.
   *
   * WASM backend provides full C engine functionality (constraint solving,
   * normalization, unification, etc.), JS fallback only provides basic data storage.
   *
   * @returns Backend initialization result
   */
  async init(): Promise<BackendInitResult> {
    // Skip WASM if JS is preferred
    if (this.options.preferredBackend === 'js') {
      return this.initJsBackend();
    }

    // Try WASM first
    try {
      const wasmResult = await this.tryInitWasmBackend();
      if (wasmResult) {
        return wasmResult;
      }
    } catch (error) {
      logger.warn('[BackendAdapter] WASM initialization failed:', error);
    }

    // Fall back to JS
    return this.initJsBackend();
  }

  /**
   * Try to initialize WASM backend
   */
  private async tryInitWasmBackend(): Promise<BackendInitResult | null> {
    const globalModuleFactory = (window as unknown as Record<string, unknown>).Lv00Module;
    let wasmInstance: Lv00WasmModule | null = null;

    // Strategy 1: Check for global factory function
    if (typeof globalModuleFactory === 'function') {
      try {
        wasmInstance = await (globalModuleFactory as () => Promise<Lv00WasmModule>)();
        logger.info('[BackendAdapter] WASM loaded from global Lv00Module factory');
      } catch (err) {
        logger.warn('[BackendAdapter] Global Lv00Module factory failed:', err);
      }
    }

    // Strategy 2: Try dynamic import
    if (!wasmInstance) {
      try {
        // @ts-expect-error - Runtime dynamic import
        const moduleFactory = await import('/lv00_web.js');
        if (typeof moduleFactory.default === 'function') {
          wasmInstance = await (moduleFactory.default as () => Promise<Lv00WasmModule>)();
          logger.info('[BackendAdapter] WASM loaded via dynamic import');
        } else if (typeof moduleFactory.default === 'object' && moduleFactory.default !== null) {
          wasmInstance = moduleFactory.default as unknown as Lv00WasmModule;
          logger.info('[BackendAdapter] WASM loaded as pre-initialized instance');
        }
      } catch (err) {
        logger.info('[BackendAdapter] Dynamic WASM import failed');
      }
    }

    // Validate and initialize WASM backend
    if (wasmInstance) {
      try {
        const requiredFunc = '_web_graph_create';
        if (typeof wasmInstance[requiredFunc] !== 'function') {
          logger.warn(`[BackendAdapter] WASM missing required export: ${requiredFunc}`);
          return null;
        }

        this.backend = new WasmBackend(wasmInstance);
        this.graphHandle = this.backend.graphCreate();
        
        logger.info('[BackendAdapter] WASM backend initialized successfully');
        return {
          type: 'wasm',
          wasmAvailable: true,
        };
      } catch (err) {
        logger.warn('[BackendAdapter] WASM backend construction failed:', err);
      }
    }

    return null;
  }

  /**
   * Initialize JS fallback backend
   */
  private initJsBackend(): BackendInitResult {
    this.backend = new JsBackend();
    this.graphHandle = this.backend.graphCreate();
    
    logger.info('[BackendAdapter] JS backend initialized (fallback mode)');
    return {
      type: 'js',
      wasmAvailable: false,
      error: 'WASM not available, using JS fallback',
    };
  }

  /**
   * Get the current backend instance
   * @throws Error if backend is not initialized
   */
  getBackend(): IBackend {
    if (!this.backend) {
      throw new Error('[BackendAdapter] Backend not initialized');
    }
    return this.backend;
  }

  /**
   * Get the current graph handle
   */
  getGraphHandle(): number {
    if (this.graphHandle === null) {
      throw new Error('[BackendAdapter] Graph not created');
    }
    return this.graphHandle;
  }

  /**
   * Get the backend type
   */
  getBackendType(): BackendType {
    return this.backend?.type ?? null;
  }

  /**
   * Check if WASM backend is active
   */
  isWasmActive(): boolean {
    return this.backend?.type === 'wasm';
  }

  /**
   * Destroy the current graph and create a new empty one
   */
  resetGraph(): void {
    const backend = this.getBackend();
    if (this.graphHandle !== null) {
      backend.graphDestroy(this.graphHandle);
    }
    this.graphHandle = backend.graphCreate();
  }

  /**
   * Cleanup all resources
   */
  destroy(): void {
    if (this.backend && this.graphHandle !== null) {
      this.backend.graphDestroy(this.graphHandle);
    }
    this.backend = null;
    this.graphHandle = null;
  }
}

// ================================================================
// Export types and utilities
// ================================================================

export type { JsGraphData, JsPoint, JsSegment };
export { DEFAULT_BACKEND_TIMEOUT_MS, WASM_MEMORY_CONFIG };
