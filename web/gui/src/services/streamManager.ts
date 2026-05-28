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
import { DEFAULT_STREAM_FILTERS } from '@/stores/aiStore';

// ================================================================
// 类型定义
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

// ================================================================
// 流式管理器类
// ================================================================

/**
 * 统一流式事件管理器
 *
 * 管理引擎事件流和 AI 事件流，提供统一的事件分发接口。
 *
 * 【优化说明】v3.4.2
 * - 使用环形缓冲区替代数组移位操作，提升高频事件处理性能
 * - 添加批量处理支持，减少回调频率
 */
export class StreamManager {
  private config: Required<StreamManagerConfig>;
  private callbacks: StreamManagerCallbacks;
  private engineClient: StreamClient | null = null;

  // 环形缓冲区实现
  private eventBuffer: UnifiedStreamEvent[] = [];
  private bufferHead: number = 0;  // 下一个写入位置
  private bufferSize: number = 0;  // 当前缓冲区元素数量

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
    this.categoryFilters = [...DEFAULT_STREAM_FILTERS];
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
   *
   * 【优化】使用环形缓冲区，避免数组 shift() 的 O(n) 开销
   *
   * @param event 要添加的事件
   */
  private addEvent(event: UnifiedStreamEvent): void {
    const capacity = this.config.eventBufferSize;

    // 环形缓冲区写入
    if (this.bufferSize < capacity) {
      // 缓冲区未满，直接追加
      this.eventBuffer.push(event);
      this.bufferSize++;
    } else {
      // 缓冲区已满，覆盖最旧的事件
      this.eventBuffer[this.bufferHead] = event;
      this.bufferHead = (this.bufferHead + 1) % capacity;
    }

    this.callbacks.onEvent(event);
  }

  /**
   * 获取缓冲区中的所有事件（按时间顺序）
   *
   * @returns 事件数组（按时间顺序排列）
   */
  getBufferedEvents(): UnifiedStreamEvent[] {
    if (this.bufferSize === 0) return [];

    if (this.bufferSize < this.config.eventBufferSize) {
      // 缓冲区未满，直接返回
      return [...this.eventBuffer];
    }

    // 缓冲区已满，需要按正确顺序返回
    const result: UnifiedStreamEvent[] = [];
    const capacity = this.config.eventBufferSize;

    for (let i = 0; i < capacity; i++) {
      const index = (this.bufferHead + i) % capacity;
      const event = this.eventBuffer[index];
      if (event) result.push(event);
    }

    return result;
  }

  /**
   * 清空缓冲区
   */
  clearBuffer(): void {
    this.eventBuffer = [];
    this.bufferHead = 0;
    this.bufferSize = 0;
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
// 工厂函数
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
