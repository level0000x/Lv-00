/**
 * @module utils/streamOptimizer
 * @description 流式事件性能优化工具
 *              提供事件节流、批量处理、内存优化等功能
 *
 * 功能特性：
 * - 事件节流（throttle）和防抖（debounce）
 * - 批量处理（batching）
 * - 事件优先级队列
 * - 内存优化（LRU 缓存）
 * - 性能监控
 */

// ================================================================
// 类型定义
// ================================================================

export interface StreamEvent {
  id: string;
  type: string;
  category: string;
  timestamp_ms: number;
  data?: unknown;
  priority?: number;
}

export interface ThrottleOptions {
  /** 节流间隔（毫秒） */
  interval: number;
  /** 是否立即执行第一次 */
  leading?: boolean;
  /** 是否执行最后一次 */
  trailing?: boolean;
}

export interface DebounceOptions {
  /** 防抖延迟（毫秒） */
  delay: number;
  /** 是否立即执行第一次 */
  leading?: boolean;
  /** 最大等待时间 */
  maxWait?: number;
}

export interface BatchOptions {
  /** 批量大小 */
  size: number;
  /** 批量超时（毫秒） */
  timeout: number;
  /** 是否按类别分组 */
  groupByCategory?: boolean;
}

export interface PriorityQueueOptions {
  /** 最大队列大小 */
  maxSize?: number;
  /** 优先级比较函数 */
  priorityCompare?: (a: StreamEvent, b: StreamEvent) => number;
}

export interface LRUCacheOptions {
  /** 最大缓存大小 */
  maxSize: number;
  /** 过期时间（毫秒） */
  ttl?: number;
}

export interface PerformanceMetrics {
  /** 总事件数 */
  totalEvents: number;
  /** 已处理事件数 */
  processedEvents: number;
  /** 丢弃事件数 */
  droppedEvents: number;
  /** 平均处理时间（毫秒） */
  avgProcessingTime: number;
  /** 当前队列大小 */
  queueSize: number;
  /** 内存使用（字节） */
  memoryUsage: number;
}

// ================================================================
// 节流器
// ================================================================

/**
 * Throttler - 事件节流器
 *
 * 限制事件触发频率，确保在指定时间间隔内最多触发一次
 */
export class Throttler<T extends StreamEvent> {
  private lastTime: number = 0;
  private timer: ReturnType<typeof setTimeout> | null = null;
  private pendingEvent: T | null = null;

  constructor(
    private handler: (event: T) => void,
    private options: ThrottleOptions
  ) {}

  /**
   * 处理事件
   */
  process(event: T): void {
    const now = Date.now();
    const elapsed = now - this.lastTime;

    if (elapsed >= this.options.interval) {
      // 可以立即执行
      if (this.options.leading !== false) {
        this.handler(event);
        this.lastTime = now;
      } else {
        this.pendingEvent = event;
        this.scheduleTrailing();
      }
    } else {
      // 在节流窗口内，保存待处理事件
      this.pendingEvent = event;
      this.scheduleTrailing();
    }
  }

  private scheduleTrailing(): void {
    if (this.timer || this.options.trailing === false) return;

    const now = Date.now();
    const remaining = this.options.interval - (now - this.lastTime);

    this.timer = setTimeout(() => {
      this.timer = null;
      if (this.pendingEvent && this.options.trailing !== false) {
        this.handler(this.pendingEvent);
        this.lastTime = Date.now();
        this.pendingEvent = null;
      }
    }, remaining);
  }

  /**
   * 清除所有待处理事件
   */
  clear(): void {
    if (this.timer) {
      clearTimeout(this.timer);
      this.timer = null;
    }
    this.pendingEvent = null;
  }

  /**
   * 立即执行待处理事件
   */
  flush(): void {
    if (this.pendingEvent) {
      this.handler(this.pendingEvent);
      this.lastTime = Date.now();
      this.pendingEvent = null;
    }
    this.clear();
  }
}

// ================================================================
// 防抖器
// ================================================================

/**
 * Debouncer - 事件防抖器
 *
 * 延迟事件处理，只在事件停止触发一段时间后执行
 */
export class Debouncer<T extends StreamEvent> {
  private timer: ReturnType<typeof setTimeout> | null = null;
  private pendingEvent: T | null = null;
  private lastInvokeTime: number = 0;

  constructor(
    private handler: (event: T) => void,
    private options: DebounceOptions
  ) {}

  /**
   * 处理事件
   */
  process(event: T): void {
    const now = Date.now();
    this.pendingEvent = event;

    // 立即执行第一次
    if (this.options.leading && now - this.lastInvokeTime >= this.options.delay) {
      this.handler(event);
      this.lastInvokeTime = now;
      this.pendingEvent = null;
      return;
    }

    // 清除之前的定时器
    if (this.timer) {
      clearTimeout(this.timer);
    }

    // 设置新的定时器
    const delay = this.options.maxWait
      ? Math.min(this.options.delay, this.options.maxWait - (now - this.lastInvokeTime))
      : this.options.delay;

    this.timer = setTimeout(() => {
      this.timer = null;
      if (this.pendingEvent) {
        this.handler(this.pendingEvent);
        this.lastInvokeTime = Date.now();
        this.pendingEvent = null;
      }
    }, Math.max(0, delay));
  }

  /**
   * 清除所有待处理事件
   */
  clear(): void {
    if (this.timer) {
      clearTimeout(this.timer);
      this.timer = null;
    }
    this.pendingEvent = null;
  }

  /**
   * 立即执行待处理事件
   */
  flush(): void {
    if (this.pendingEvent) {
      this.handler(this.pendingEvent);
      this.lastInvokeTime = Date.now();
      this.pendingEvent = null;
    }
    this.clear();
  }
}

// ================================================================
// 批量处理器
// ================================================================

/**
 * BatchProcessor - 批量事件处理器
 *
 * 收集事件并批量处理，减少处理次数
 */
export class BatchProcessor<T extends StreamEvent> {
  private batch: T[] = [];
  private timer: ReturnType<typeof setTimeout> | null = null;
  private categoryBatches: Map<string, T[]> = new Map();

  constructor(
    private handler: (events: T[]) => void,
    private options: BatchOptions
  ) {}

  /**
   * 添加事件到批量
   */
  add(event: T): void {
    if (this.options.groupByCategory) {
      const category = event.category;
      if (!this.categoryBatches.has(category)) {
        this.categoryBatches.set(category, []);
      }
      const categoryBatch = this.categoryBatches.get(category)!;
      categoryBatch.push(event);

      if (categoryBatch.length >= this.options.size) {
        this.flushCategory(category);
      }
    } else {
      this.batch.push(event);

      if (this.batch.length >= this.options.size) {
        this.flush();
      }
    }

    // 设置超时定时器
    if (!this.timer) {
      this.timer = setTimeout(() => {
        this.timer = null;
        this.flushAll();
      }, this.options.timeout);
    }
  }

  /**
   * 刷新特定类别的批量
   */
  private flushCategory(category: string): void {
    const categoryBatch = this.categoryBatches.get(category);
    if (categoryBatch && categoryBatch.length > 0) {
      this.handler(categoryBatch);
      this.categoryBatches.set(category, []);
    }
  }

  /**
   * 刷新当前批量
   */
  flush(): void {
    if (this.batch.length > 0) {
      this.handler(this.batch);
      this.batch = [];
    }
    if (this.timer) {
      clearTimeout(this.timer);
      this.timer = null;
    }
  }

  /**
   * 刷新所有批量
   */
  flushAll(): void {
    if (this.options.groupByCategory) {
      for (const category of this.categoryBatches.keys()) {
        this.flushCategory(category);
      }
    } else {
      this.flush();
    }
  }

  /**
   * 获取当前批量大小
   */
  get size(): number {
    if (this.options.groupByCategory) {
      let total = 0;
      for (const batch of this.categoryBatches.values()) {
        total += batch.length;
      }
      return total;
    }
    return this.batch.length;
  }
}

// ================================================================
// 优先级队列
// ================================================================

/**
 * PriorityQueue - 事件优先级队列
 *
 * 按优先级处理事件，确保高优先级事件优先处理
 */
export class PriorityQueue<T extends StreamEvent> {
  private heap: T[] = [];
  private maxSize: number;

  constructor(
    options: PriorityQueueOptions = {}
  ) {
    this.maxSize = options.maxSize ?? 10000;
  }

  /**
   * 添加事件
   */
  enqueue(event: T): boolean {
    if (this.heap.length >= this.maxSize) {
      // 队列已满，丢弃最低优先级事件
      const lowest = this.heap[this.heap.length - 1];
      if (lowest && (event.priority ?? 0) <= (lowest.priority ?? 0)) {
        return false;
      }
      this.heap.pop();
    }

    this.heap.push(event);
    this.bubbleUp(this.heap.length - 1);
    return true;
  }

  /**
   * 取出最高优先级事件
   */
  dequeue(): T | null {
    if (this.heap.length === 0) return null;

    const top = this.heap[0];
    const last = this.heap.pop()!;

    if (this.heap.length > 0) {
      this.heap[0] = last;
      this.bubbleDown(0);
    }

    return top ?? null;
  }

  /**
   * 查看最高优先级事件
   */
  peek(): T | null {
    return this.heap[0] || null;
  }

  private bubbleUp(index: number): void {
    while (index > 0) {
      const parent = Math.floor((index - 1) / 2);
      if (this.compare(index, parent) <= 0) break;

      this.swap(index, parent);
      index = parent;
    }
  }

  private bubbleDown(index: number): void {
    const length = this.heap.length;

    while (true) {
      const left = 2 * index + 1;
      const right = 2 * index + 2;
      let largest = index;

      if (left < length && this.compare(left, largest) > 0) {
        largest = left;
      }
      if (right < length && this.compare(right, largest) > 0) {
        largest = right;
      }

      if (largest === index) break;

      this.swap(index, largest);
      index = largest;
    }
  }

  private compare(i: number, j: number): number {
    const a = this.heap[i];
    const b = this.heap[j];
    if (!a || !b) return 0;
    const priorityDiff = (b.priority ?? 0) - (a.priority ?? 0);
    if (priorityDiff !== 0) return priorityDiff;
    return a.timestamp_ms - b.timestamp_ms;
  }

  private swap(i: number, j: number): void {
    const tmp = this.heap[i]!;
    this.heap[i] = this.heap[j]!;
    this.heap[j] = tmp;
  }

  /**
   * 获取队列大小
   */
  get size(): number {
    return this.heap.length;
  }

  /**
   * 是否为空
   */
  get isEmpty(): boolean {
    return this.heap.length === 0;
  }

  /**
   * 清空队列
   */
  clear(): void {
    this.heap = [];
  }
}

// ================================================================
// LRU 缓存 / LRUCache
// ================================================================

interface CacheEntry<V> {
  value: V;
  timestamp: number;
}

/**
 * LRUCache - 最近最少使用缓存
 *
 * 用于缓存事件数据，减少内存使用
 */
export class LRUCache<K, V> {
  private cache: Map<K, CacheEntry<V>> = new Map();
  private maxSize: number;
  private ttl: number | null;

  constructor(options: LRUCacheOptions) {
    this.maxSize = options.maxSize;
    this.ttl = options.ttl ?? null;
  }

  /**
   * 获取缓存值
   */
  get(key: K): V | undefined {
    const entry = this.cache.get(key);
    if (!entry) return undefined;

    // 检查是否过期
    if (this.ttl && Date.now() - entry.timestamp > this.ttl) {
      this.cache.delete(key);
      return undefined;
    }

    // 移动到最近使用位置
    this.cache.delete(key);
    this.cache.set(key, entry);
    return entry.value;
  }

  /**
   * 设置缓存值
   */
  set(key: K, value: V): void {
    // 如果已存在，先删除
    if (this.cache.has(key)) {
      this.cache.delete(key);
    }

    // 检查是否需要淘汰
    while (this.cache.size >= this.maxSize) {
      const oldestKey = this.cache.keys().next().value;
      if (oldestKey !== undefined) {
        this.cache.delete(oldestKey);
      }
    }

    this.cache.set(key, {
      value,
      timestamp: Date.now(),
    });
  }

  /**
   * 检查是否存在
   */
  has(key: K): boolean {
    const entry = this.cache.get(key);
    if (!entry) return false;

    // 检查是否过期
    if (this.ttl && Date.now() - entry.timestamp > this.ttl) {
      this.cache.delete(key);
      return false;
    }

    return true;
  }

  /**
   * 删除缓存
   */
  delete(key: K): boolean {
    return this.cache.delete(key);
  }

  /**
   * 清空缓存
   */
  clear(): void {
    this.cache.clear();
  }

  /**
   * 获取缓存大小
   */
  get size(): number {
    return this.cache.size;
  }

  /**
   * 获取所有键
   */
  keys(): IterableIterator<K> {
    return this.cache.keys();
  }

  /**
   * 清理过期条目
   */
  cleanup(): number {
    if (!this.ttl) return 0;

    const now = Date.now();
    let cleaned = 0;

    for (const [key, entry] of this.cache.entries()) {
      if (now - entry.timestamp > this.ttl) {
        this.cache.delete(key);
        cleaned++;
      }
    }

    return cleaned;
  }
}

// ================================================================
// 性能监控器
// ================================================================

/**
 * PerformanceMonitor - 性能监控器
 *
 * 监控流式事件处理性能
 */
export class PerformanceMonitor {
  private totalEvents: number = 0;
  private processedEvents: number = 0;
  private droppedEvents: number = 0;
  private processingTimes: number[] = [];
  private maxSamples: number = 1000;

  /**
   * 记录事件
   */
  recordEvent(): void {
    this.totalEvents++;
  }

  /**
   * 记录处理完成
   */
  recordProcessed(processingTime: number): void {
    this.processedEvents++;
    this.processingTimes.push(processingTime);

    if (this.processingTimes.length > this.maxSamples) {
      this.processingTimes.shift();
    }
  }

  /**
   * 记录丢弃事件
   */
  recordDropped(): void {
    this.droppedEvents++;
  }

  /**
   * 获取性能指标
   */
  getMetrics(queueSize: number = 0): PerformanceMetrics {
    const avgProcessingTime = this.processingTimes.length > 0
      ? this.processingTimes.reduce((a, b) => a + b, 0) / this.processingTimes.length
      : 0;

    // 估算内存使用
    const memoryUsage = this.estimateMemoryUsage();

    return {
      totalEvents: this.totalEvents,
      processedEvents: this.processedEvents,
      droppedEvents: this.droppedEvents,
      avgProcessingTime,
      queueSize,
      memoryUsage,
    };
  }

  private estimateMemoryUsage(): number {
    // 简单估算：每个事件约 200 字节
    const eventMemory = this.totalEvents * 200;
    const timeMemory = this.processingTimes.length * 8;
    return eventMemory + timeMemory;
  }

  /**
   * 重置统计
   */
  reset(): void {
    this.totalEvents = 0;
    this.processedEvents = 0;
    this.droppedEvents = 0;
    this.processingTimes = [];
  }
}

// ================================================================
// 流式事件优化器
// ================================================================

/**
 * StreamOptimizer - 流式事件优化器
 *
 * 综合使用节流、批量处理、优先级队列等技术优化事件处理
 */
export class StreamOptimizer<T extends StreamEvent> {
  private throttler: Throttler<T> | null = null;
  private debouncer: Debouncer<T> | null = null;
  private batchProcessor: BatchProcessor<T> | null = null;
  private priorityQueue: PriorityQueue<T>;
  private monitor: PerformanceMonitor;
  private cache: LRUCache<string, T>;

  constructor(
    private handler: (event: T) => void,
    options: {
      throttle?: ThrottleOptions;
      debounce?: DebounceOptions;
      batch?: BatchOptions;
      priorityQueue?: PriorityQueueOptions;
      cache?: LRUCacheOptions;
    } = {}
  ) {
    if (options.throttle) {
      this.throttler = new Throttler(handler, options.throttle);
    }

    if (options.debounce) {
      this.debouncer = new Debouncer(handler, options.debounce);
    }

    if (options.batch) {
      this.batchProcessor = new BatchProcessor(
        (events) => events.forEach(handler),
        options.batch
      );
    }

    this.priorityQueue = new PriorityQueue(options.priorityQueue);
    this.monitor = new PerformanceMonitor();
    this.cache = new LRUCache(options.cache ?? { maxSize: 1000 });
  }

  /**
   * 处理事件
   */
  process(event: T): void {
    this.monitor.recordEvent();

    // 检查缓存
    if (this.cache.has(event.id)) {
      this.monitor.recordDropped();
      return;
    }

    // 缓存事件
    this.cache.set(event.id, event);

    const startTime = performance.now();

    // 根据配置选择处理方式
    if (this.batchProcessor) {
      this.batchProcessor.add(event);
    } else if (this.throttler) {
      this.throttler.process(event);
    } else if (this.debouncer) {
      this.debouncer.process(event);
    } else {
      this.handler(event);
    }

    this.monitor.recordProcessed(performance.now() - startTime);
  }

  /**
   * 获取性能指标
   */
  getMetrics(): PerformanceMetrics {
    return this.monitor.getMetrics(this.priorityQueue.size);
  }

  /**
   * 刷新所有缓冲
   */
  flush(): void {
    this.throttler?.flush();
    this.debouncer?.flush();
    this.batchProcessor?.flushAll();
  }

  /**
   * 清理资源
   */
  dispose(): void {
    this.throttler?.clear();
    this.debouncer?.clear();
    this.batchProcessor?.flushAll();
    this.priorityQueue.clear();
    this.cache.clear();
  }
}

export default StreamOptimizer;
