/**
 * @module services/streamManager
 * @description 统一流式事件管理器
 *              整合引擎事件流和 AI 助手事件流，提供统一的事件分发机制。
 *
 *              功能特性：
 *              - 双通道事件源：引擎 WebSocket + AI API
 *              - 事件类别过滤和统计
 *              - 自动重连和心跳检测
 *              - 事件缓冲和批量处理
 *              - 状态持久化
 */

import { createStreamClient, type StreamClient, type StreamClientCallbacks } from './streamClient';
import type {
  EngineStreamEvent,
  EngineStreamState,
  EngineStreamCategory,
  EngineStreamCategoryFilter,
} from '@/types';

// ================================================================
// 类型定义 / Type Definitions
// ================================================================

/**
 * 流式事件源类型
 */
export type StreamSourceType = 'engine' | 'ai';

/**
 * 统一流式事件（包装引擎事件或 AI 事件）
 */
export interface UnifiedStreamEvent {
  /** 事件唯一 ID */
  id: string;
  /** 事件来源 */
  source: StreamSourceType;
  /** 引擎事件（当 source === 'engine' 时） */
  engineEvent?: EngineStreamEvent;
  /** AI 事件描述（当 source === 'ai' 时） */
  aiDescription?: string;
  /** 时间戳 */
  timestamp: number;
  /** 事件类别 */
  category: EngineStreamCategory;
  /** 显示颜色 */
  color: string;
}

/**
 * 流式管理器配置
 */
export interface StreamManagerConfig {
  /** 引擎 WebSocket URL */
  engineUrl?: string;
  /** 自动连接引擎 */
  autoConnectEngine?: boolean;
  /** 事件缓冲区大小 */
  eventBufferSize?: number;
  /** 心跳间隔（毫秒） */
  heartbeatInterval?: number;
}

/**
 * 流式管理器回调
 */
export interface StreamManagerCallbacks {
  /** 事件接收回调 */
  onEvent: (event: UnifiedStreamEvent) => void;
  /** 引擎状态变化回调 */
  onEngineStateChange: (state: EngineStreamState) => void;
  /** 统计更新回调 */
  onStatsUpdate: (stats: StreamStats) => void;
  /** 错误回调 */
  onError: (error: Error, source: StreamSourceType) => void;
}

/**
 * 流式统计信息
 */
export interface StreamStats {
  /** 总事件数 */
  totalEvents: number;
  /** 引擎事件数 */
  engineEvents: number;
  /** AI 事件数 */
  aiEvents: number;
  /** 按类别统计 */
  byCategory: Record<EngineStreamCategory, number>;
  /** 事件速率（每秒） */
  eventsPerSecond: number;
  /** 引擎连接状态 */
  engineState: EngineStreamState;
  /** 会话持续时间（毫秒） */
  sessionDuration: number;
}

/**
 * 默认类别过滤器配置
 */
const DEFAULT_CATEGORY_FILTERS: EngineStreamCategoryFilter[] = [
  {
    category: 'engine',
    label: 'Engine',
    labelZh: '引擎',
    enabled: true,
    color: '#3fb950',
    count: 0,
    eventTypes: ['ENGINE_START', 'ENGINE_DONE', 'ENGINE_PAUSED'],
  },
  {
    category: 'normalize',
    label: 'Normalize',
    labelZh: '归一化',
    enabled: true,
    color: '#58a6ff',
    count: 0,
    eventTypes: ['NORMALIZE_START', 'NORMALIZE_MERGE', 'NORMALIZE_DONE'],
  },
  {
    category: 'rewrite',
    label: 'Rewrite',
    labelZh: '重写',
    enabled: true,
    color: '#a371f7',
    count: 0,
    eventTypes: ['REWRITE_START', 'REWRITE_RULE_LOADED', 'REWRITE_MATCH_FOUND', 'REWRITE_APPLIED', 'REWRITE_ROLLBACK', 'REWRITE_DONE'],
  },
  {
    category: 'solve',
    label: 'Solve',
    labelZh: '求解',
    enabled: true,
    color: '#f0883e',
    count: 0,
    eventTypes: ['SOLVE_START', 'SOLVE_EQUATION_EXTRACTED', 'SOLVE_GROEBNER_STEP', 'SOLVE_VARIABLE_RESOLVED', 'SOLVE_DONE'],
  },
  {
    category: 'proof',
    label: 'Proof',
    labelZh: '证明',
    enabled: true,
    color: '#f778ba',
    count: 0,
    eventTypes: ['PROOF_STEP_ADDED', 'PROOF_STEP_APPLIED', 'PROOF_UNIFY', 'PROOF_COLOR_UPDATE', 'PROOF_DEPENDENCY_CHANGE'],
  },
  {
    category: 'func_block',
    label: 'Function',
    labelZh: '函数块',
    enabled: true,
    color: '#39d353',
    count: 0,
    eventTypes: ['FUNC_BLOCK_PACK_START', 'FUNC_BLOCK_PACK_DONE', 'FUNC_BLOCK_INSTANTIATE_START', 'FUNC_BLOCK_INSTANTIATE_DONE', 'FUNC_BLOCK_PARTIAL_APPLY', 'FUNC_BLOCK_DETERMINISM_CHECK', 'FUNC_BLOCK_CAPTURE_AVOID', 'FUNC_BLOCK_CROSS_BOUNDARY'],
  },
  {
    category: 'conflict',
    label: 'Conflict',
    labelZh: '冲突',
    enabled: true,
    color: '#f85149',
    count: 0,
    eventTypes: ['CONFLICT_DETECTED'],
  },
  {
    category: 'info',
    label: 'Info',
    labelZh: '信息',
    enabled: true,
    color: '#8b949e',
    count: 0,
    eventTypes: ['INFO', 'PROGRESS', 'GRAPH_SNAPSHOT', 'NODE_ADDED', 'CONSTRAINT_ADDED', 'WARNING', 'ERROR', 'CIRCUIT_TRIP'],
  },
];

// ================================================================
// 流式管理器类 / Stream Manager Class
// ================================================================

/**
 * 统一流式事件管理器
 *
 * 管理引擎事件流和 AI 事件流，提供统一的事件分发接口。
 */
export class StreamManager {
  private config: Required<StreamManagerConfig>;
  private callbacks: StreamManagerCallbacks;
  private engineClient: StreamClient | null = null;
  private eventBuffer: UnifiedStreamEvent[] = [];
  private eventIdCounter = 0;
  private sessionStartMs = 0;
  private stats: StreamStats;
  private categoryFilters: EngineStreamCategoryFilter[];
  private enabledCategories: Set<EngineStreamCategory>;

  constructor(config: StreamManagerConfig, callbacks: StreamManagerCallbacks) {
    this.config = {
      engineUrl: config.engineUrl ?? 'ws://localhost:3456',
      autoConnectEngine: config.autoConnectEngine ?? false,
      eventBufferSize: config.eventBufferSize ?? 1000,
      heartbeatInterval: config.heartbeatInterval ?? 15000,
    };
    this.callbacks = callbacks;
    this.categoryFilters = [...DEFAULT_CATEGORY_FILTERS];
    this.enabledCategories = new Set(
      this.categoryFilters.filter(f => f.enabled).map(f => f.category)
    );
    this.stats = this.createEmptyStats();
  }

  private createEmptyStats(): StreamStats {
    return {
      totalEvents: 0,
      engineEvents: 0,
      aiEvents: 0,
      byCategory: {
        engine: 0,
        normalize: 0,
        rewrite: 0,
        solve: 0,
        proof: 0,
        func_block: 0,
        conflict: 0,
        info: 0,
      },
      eventsPerSecond: 0,
      engineState: 'disconnected',
      sessionDuration: 0,
    };
  }

  /**
   * 启动流式管理器
   */
  start(): void {
    this.sessionStartMs = Date.now();
    this.stats = this.createEmptyStats();

    if (this.config.autoConnectEngine) {
      this.connectEngine();
    }
  }

  /**
   * 停止流式管理器
   */
  stop(): void {
    this.disconnectEngine();
    this.eventBuffer = [];
    this.stats = this.createEmptyStats();
  }

  /**
   * 连接引擎 WebSocket
   */
  connectEngine(): void {
    if (this.engineClient) {
      return;
    }

    const streamCallbacks: StreamClientCallbacks = {
      onStateChange: (state) => {
        this.stats.engineState = state;
        this.callbacks.onEngineStateChange(state);
        this.notifyStatsUpdate();
      },
      onEvent: (event) => {
        this.handleEngineEvent(event);
      },
      onStatsUpdate: (_total, eps) => {
        this.stats.eventsPerSecond = eps;
        this.notifyStatsUpdate();
      },
      onError: (error) => {
        this.callbacks.onError(error, 'engine');
      },
    };

    this.engineClient = createStreamClient(this.config.engineUrl, streamCallbacks, {
      autoReconnect: true,
      maxReconnectAttempts: 10,
      heartbeatInterval: this.config.heartbeatInterval,
    });

    this.engineClient.connect();
  }

  /**
   * 断开引擎 WebSocket
   */
  disconnectEngine(): void {
    if (this.engineClient) {
      this.engineClient.destroy();
      this.engineClient = null;
      this.stats.engineState = 'disconnected';
      this.callbacks.onEngineStateChange('disconnected');
    }
  }

  /**
   * 获取引擎连接状态
   */
  getEngineState(): EngineStreamState {
    return this.stats.engineState;
  }

  /**
   * 处理引擎事件
   */
  private handleEngineEvent(event: EngineStreamEvent): void {
    // 检查类别过滤器
    if (!this.enabledCategories.has(event.category)) {
      return;
    }

    const unifiedEvent: UnifiedStreamEvent = {
      id: `engine-${++this.eventIdCounter}`,
      source: 'engine',
      engineEvent: event,
      timestamp: event.timestamp_ms,
      category: event.category,
      color: event.color,
    };

    this.addEvent(unifiedEvent);
    this.updateStats('engine', event.category);
  }

  /**
   * 推送 AI 事件
   */
  pushAIEvent(description: string, category: EngineStreamCategory = 'info'): void {
    const unifiedEvent: UnifiedStreamEvent = {
      id: `ai-${++this.eventIdCounter}`,
      source: 'ai',
      aiDescription: description,
      timestamp: Date.now(),
      category,
      color: this.getCategoryColor(category),
    };

    this.addEvent(unifiedEvent);
    this.updateStats('ai', category);
  }

  /**
   * 添加事件到缓冲区
   */
  private addEvent(event: UnifiedStreamEvent): void {
    this.eventBuffer.push(event);

    // 限制缓冲区大小
    if (this.eventBuffer.length > this.config.eventBufferSize) {
      this.eventBuffer.shift();
    }

    this.callbacks.onEvent(event);
  }

  /**
   * 更新统计信息
   */
  private updateStats(source: StreamSourceType, category: EngineStreamCategory): void {
    this.stats.totalEvents++;
    if (source === 'engine') {
      this.stats.engineEvents++;
    } else {
      this.stats.aiEvents++;
    }
    this.stats.byCategory[category]++;
    this.stats.sessionDuration = Date.now() - this.sessionStartMs;

    // 更新类别过滤器计数
    const filter = this.categoryFilters.find(f => f.category === category);
    if (filter) {
      filter.count++;
    }

    this.notifyStatsUpdate();
  }

  /**
   * 通知统计更新
   */
  private notifyStatsUpdate(): void {
    this.callbacks.onStatsUpdate({ ...this.stats });
  }

  /**
   * 获取类别颜色
   */
  private getCategoryColor(category: EngineStreamCategory): string {
    const filter = this.categoryFilters.find(f => f.category === category);
    return filter?.color ?? '#888888';
  }

  /**
   * 获取统计信息
   */
  getStats(): StreamStats {
    return { ...this.stats };
  }

  /**
   * 获取类别过滤器
   */
  getCategoryFilters(): EngineStreamCategoryFilter[] {
    return this.categoryFilters.map(f => ({ ...f }));
  }

  /**
   * 设置类别过滤器启用状态
   */
  setCategoryEnabled(category: EngineStreamCategory, enabled: boolean): void {
    if (enabled) {
      this.enabledCategories.add(category);
    } else {
      this.enabledCategories.delete(category);
    }

    const filter = this.categoryFilters.find(f => f.category === category);
    if (filter) {
      filter.enabled = enabled;
    }
  }

  /**
   * 切换类别过滤器
   */
  toggleCategory(category: EngineStreamCategory): void {
    const filter = this.categoryFilters.find(f => f.category === category);
    if (filter) {
      this.setCategoryEnabled(category, !filter.enabled);
    }
  }

  /**
   * 获取事件缓冲区
   */
  getEventBuffer(limit?: number): UnifiedStreamEvent[] {
    if (limit !== undefined && limit > 0) {
      return this.eventBuffer.slice(-limit);
    }
    return [...this.eventBuffer];
  }

  /**
   * 清空事件缓冲区
   */
  clearEventBuffer(): void {
    this.eventBuffer = [];
    // 重置类别计数
    for (const filter of this.categoryFilters) {
      filter.count = 0;
    }
  }

  /**
   * 重置统计信息
   */
  resetStats(): void {
    this.stats = this.createEmptyStats();
    this.sessionStartMs = Date.now();
    this.clearEventBuffer();
    this.notifyStatsUpdate();
  }
}

// ================================================================
// 工厂函数 / Factory Function
// ================================================================

/**
 * 创建流式管理器实例
 */
export function createStreamManager(
  config: StreamManagerConfig,
  callbacks: StreamManagerCallbacks
): StreamManager {
  return new StreamManager(config, callbacks);
}
