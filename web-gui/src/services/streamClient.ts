/**
 * @module services/streamClient
 * @description Lv-00 引擎流式事件 WebSocket 客户端。
 *              提供与 C 引擎 / Python stream_bridge 的实时事件通信能力。
 *
 *              核心功能：
 *              - WebSocket 连接生命周期管理（自动重连、指数退避）
 *              - 引擎流式事件的接收、解析和转发
 *              - 连接状态暴露给 UI（disconnected / connecting / connected / error）
 *              - 事件类别过滤和统计
 *              - 保活心跳检测
 *
 *              后端兼容性：
 *              - C 引擎 stream.c 输出的 JSON-RPC 通知流
 *              - Python stream_bridge.py 的 WebSocket 服务器
 *              - 兼容 SSE 模式的 stream_bridge 作为备选
 *
 *              使用方式：
 *              ```typescript
 *              import { createStreamClient } from '@/services/streamClient';
 *              const client = createStreamClient('ws://localhost:3456', callbacks);
 *              client.connect();
 *              ```
 */

import type {
  EngineStreamEvent,
  EngineStreamState,
  EngineStreamCategory,
} from '@/types';

// ================================================================
// 类型定义 / Type Definitions
// ================================================================

/** 流式客户端回调集合 / Stream Client Callbacks */
export interface StreamClientCallbacks {
  /** 连接状态变化时调用 */
  onStateChange: (state: EngineStreamState) => void;
  /** 收到新的流式事件时调用 */
  onEvent: (event: EngineStreamEvent) => void;
  /** 统计信息更新时调用 */
  onStatsUpdate: (total: number, eps: number) => void;
  /** 发生错误时调用 */
  onError: (error: Error) => void;
}

/** 流式客户端配置选项 / Stream Client Configuration */
export interface StreamClientOptions {
  /** 自动重连是否启用，默认 true */
  autoReconnect?: boolean;
  /** 最大重连次数，默认 10 次 */
  maxReconnectAttempts?: number;
  /** 初始重连延迟（毫秒），默认 1000 */
  initialReconnectDelay?: number;
  /** 最大重连延迟（毫秒），默认 30000 */
  maxReconnectDelay?: number;
  /** 心跳间隔（毫秒），默认 15000 */
  heartbeatInterval?: number;
  /** 心跳超时时间（毫秒），默认 5000 */
  heartbeatTimeout?: number;
  /** 事件日志最大保留数，默认 1000 */
  maxEventLog?: number;
}

/** 流式客户端结果对象 / Stream Client Result */
export interface StreamClient {
  /** 连接到 WebSocket 服务器 */
  connect: () => void;
  /** 断开连接 */
  disconnect: () => void;
  /** 获取当前连接状态 */
  getState: () => EngineStreamState;
  /** 获取事件日志（最近 N 条） */
  getEventLog: (limit?: number) => EngineStreamEvent[];
  /** 手动发送心跳 */
  sendHeartbeat: () => void;
  /** 销毁客户端并释放所有资源 */
  destroy: () => void;
}

// ================================================================
// 默认配置 / Default Configuration
// ================================================================

const DEFAULT_OPTIONS: Required<StreamClientOptions> = {
  autoReconnect: true,
  maxReconnectAttempts: 10,
  initialReconnectDelay: 1000,
  maxReconnectDelay: 30000,
  heartbeatInterval: 15000,
  heartbeatTimeout: 5000,
  maxEventLog: 1000,
};

// ================================================================
// 事件解析辅助 / Event Parsing Helpers
// ================================================================

/**
 * 解析引擎流式事件，将原始 JSON 转换为 EngineStreamEvent。
 *
 * 支持两种格式：
 * 1. JSON-RPC 通知格式：{ "jsonrpc": "2.0", "method": "...", "params": {...} }
 * 2. 直接事件格式：{ "type": "...", "description": "...", ... }
 *
 * @param raw - 原始 JSON 数据
 * @returns 解析后的引擎流式事件，或 null（如果格式无法识别）
 */
function parseRawEvent(raw: unknown): EngineStreamEvent | null {
  if (typeof raw !== 'object' || raw === null) return null;

  const data = raw as Record<string, unknown>;

  // 格式 1: JSON-RPC 通知格式
  if (data.jsonrpc === '2.0' && data.method && data.params) {
    const params = data.params as Record<string, unknown>;
    return {
      type: String(data.method),
      type_name: String(params.type_name ?? data.method),
      color: String(params.color ?? '#888888'),
      category: inferCategory(String(params.category ?? String(data.method))),
      timestamp_ms: Number(params.timestamp_ms ?? Date.now()),
      step: Number(params.step ?? -1),
      total_steps: Number(params.total_steps ?? -1),
      node_id: Number(params.node_id ?? -1),
      constraint_id: Number(params.constraint_id ?? -1),
      rule_id: Number(params.rule_id ?? -1),
      var_id: Number(params.var_id ?? -1),
      description: String(params.description ?? ''),
      detail: params.detail ? String(params.detail) : null,
      progress: Number(params.progress ?? -1),
      numeric_value: Number(params.numeric_value ?? 0),
      graph_snapshot: params.graph_snapshot ? String(params.graph_snapshot) : null,
    };
  }

  // 格式 2: 直接事件格式（C 引擎 stream_event_to_json 输出）
  if (data.type && typeof data.type === 'string') {
    return {
      type: String(data.type),
      type_name: String(data.type_name ?? data.type),
      color: String(data.color ?? '#888888'),
      category: inferCategory(String(data.category ?? data.type)),
      timestamp_ms: Number(data.timestamp_ms ?? Date.now()),
      step: Number(data.step ?? -1),
      total_steps: Number(data.total_steps ?? -1),
      node_id: Number(data.node_id ?? -1),
      constraint_id: Number(data.constraint_id ?? -1),
      rule_id: Number(data.rule_id ?? -1),
      var_id: Number(data.var_id ?? -1),
      description: String(data.description ?? ''),
      detail: data.detail ? String(data.detail) : null,
      progress: Number(data.progress ?? -1),
      numeric_value: Number(data.numeric_value ?? 0),
      graph_snapshot: data.graph_snapshot ? String(data.graph_snapshot) : null,
    };
  }

  return null;
}

/**
 * 根据事件类型字符串推断事件类别。
 *
 * @param typeStr - 事件类型字符串（如 "ENGINE_START"、"NORMALIZE_MERGE" 等）
 * @returns 对应的事件类别
 */
function inferCategory(typeStr: string): EngineStreamCategory {
  const upper = typeStr.toUpperCase();
  if (upper.startsWith('ENGINE')) return 'engine';
  if (upper.startsWith('NORMALIZE')) return 'normalize';
  if (upper.startsWith('REWRITE')) return 'rewrite';
  if (upper.startsWith('SOLVE') || upper.startsWith('GROEBNER') || upper.startsWith('VARIABLE')) return 'solve';
  if (upper.startsWith('PROOF')) return 'proof';
  if (upper.startsWith('FUNC_BLOCK') || upper.startsWith('FUNCTION')) return 'func_block';
  if (upper.startsWith('CONFLICT')) return 'conflict';
  return 'info';
}

// ================================================================
// 流式客户端工厂 / Stream Client Factory
// ================================================================

/**
 * 创建 Lv-00 引擎流式事件 WebSocket 客户端。
 *
 * 功能特性：
 * - 自动重连（指数退避：1s → 2s → 4s → ... → 30s，最多 10 次）
 * - 保活心跳检测（每 15 秒发送 ping，5 秒超时）
 * - 事件日志缓存（最多保留 1000 条）
 * - 连接状态实时通知（disconnected / connecting / connected / error）
 *
 * @param url - WebSocket 服务器 URL（如 'ws://localhost:3456'）
 * @param callbacks - 事件回调集合
 * @param options - 可选的配置选项
 * @returns 流式客户端控制对象
 */
export function createStreamClient(
  url: string,
  callbacks: StreamClientCallbacks,
  options?: StreamClientOptions,
): StreamClient {
  const opts: Required<StreamClientOptions> = { ...DEFAULT_OPTIONS, ...options };

  let ws: WebSocket | null = null;
  let state: EngineStreamState = 'disconnected';
  let reconnectAttempt = 0;
  let eventLog: EngineStreamEvent[] = [];
  let eventCount = 0;
  let sessionStartMs = 0;
  let lastEventTime = 0;
  let heartbeatTimer: ReturnType<typeof setInterval> | null = null;
  let heartbeatTimeoutTimer: ReturnType<typeof setTimeout> | null = null;

  /** 更新内部状态并通知回调 */
  function setState(newState: EngineStreamState): void {
    state = newState;
    callbacks.onStateChange(newState);
  }

  /** 添加事件到日志并发送统计 */
  function addEvent(event: EngineStreamEvent): void {
    eventLog.push(event);
    if (eventLog.length > opts.maxEventLog) {
      eventLog = eventLog.slice(-opts.maxEventLog);
    }
    eventCount++;
    lastEventTime = Date.now();

    // 计算事件速率（每秒事件数）
    const elapsed = (lastEventTime - sessionStartMs) / 1000;
    const eps = elapsed > 0 ? eventCount / elapsed : 1;
    callbacks.onStatsUpdate(eventCount, eps);
    callbacks.onEvent(event);
  }

  /** 清理所有定时器和连接 */
  function cleanup(): void {
    if (heartbeatTimer !== null) {
      clearInterval(heartbeatTimer);
      heartbeatTimer = null;
    }
    if (heartbeatTimeoutTimer !== null) {
      clearTimeout(heartbeatTimeoutTimer);
      heartbeatTimeoutTimer = null;
    }
  }

  /** 开始心跳检测 */
  function startHeartbeat(): void {
    if (heartbeatTimer !== null) clearInterval(heartbeatTimer);

    heartbeatTimer = setInterval(() => {
      if (!ws || ws.readyState !== WebSocket.OPEN) return;
      try {
        ws.send(JSON.stringify({ jsonrpc: '2.0', method: 'ping', params: {} }));
      } catch {
        // 发送失败，已在 handleDisconnect 中处理
      }

      // 设置心跳超时检测
      if (heartbeatTimeoutTimer !== null) clearTimeout(heartbeatTimeoutTimer);
      heartbeatTimeoutTimer = setTimeout(() => {
        if (ws && ws.readyState === WebSocket.OPEN) {
          // 心跳超时，主动断开并重连
          ws.close();
        }
      }, opts.heartbeatTimeout);
    }, opts.heartbeatInterval);
  }

  /** 处理断开连接 */
  function handleDisconnect(): void {
    cleanup();
    setState('disconnected');
  }

  /** 建立 WebSocket 连接 */
  function connect(): void {
    if (ws && (ws.readyState === WebSocket.CONNECTING || ws.readyState === WebSocket.OPEN)) {
      return;
    }

    cleanup();
    setState('connecting');

    try {
      ws = new WebSocket(url);
    } catch (err) {
      callbacks.onError(new Error(`WebSocket 创建失败: ${(err as Error).message}`));
      setState('error');
      scheduleReconnect();
      return;
    }

    ws.onopen = () => {
      reconnectAttempt = 0;
      sessionStartMs = Date.now();
      eventCount = 0;
      eventLog = [];
      setState('connected');

      // 订阅所有事件类型
      try {
        ws!.send(JSON.stringify({
          jsonrpc: '2.0',
          method: 'subscribe',
          params: { event_mask: -1 }, // -1 = 所有事件类型
          id: 1,
        }));
      } catch {
        // 订阅失败，可能服务器不支持 subscribe 方法，忽略
      }

      startHeartbeat();
    };

    ws.onmessage = (event: MessageEvent) => {
      if (heartbeatTimeoutTimer !== null) {
        clearTimeout(heartbeatTimeoutTimer);
        heartbeatTimeoutTimer = null;
      }

      try {
        const raw = JSON.parse(String(event.data));

        // 处理 pong 响应
        if (raw.result === 'pong' || raw.method === 'pong') return;

        // 解析流式事件
        const engineEvent = parseRawEvent(raw);
        if (engineEvent) {
          addEvent(engineEvent);
        }
      } catch {
        // 非 JSON 消息，忽略（可能是日志文本）
      }
    };

    ws.onerror = () => {
      if (ws?.readyState === WebSocket.CLOSED || ws?.readyState === WebSocket.CLOSING) {
        handleDisconnect();
        scheduleReconnect();
      } else {
        setState('error');
        callbacks.onError(new Error('WebSocket 连接错误'));
      }
    };

    ws.onclose = () => {
      handleDisconnect();
      scheduleReconnect();
    };
  }

  /** 调度重连（指数退避） */
  function scheduleReconnect(): void {
    if (!opts.autoReconnect || reconnectAttempt >= opts.maxReconnectAttempts) {
      setState('error');
      callbacks.onError(new Error(
        `重连失败：已达到最大重连次数 (${opts.maxReconnectAttempts})`,
      ));
      return;
    }

    const delay = Math.min(
      opts.initialReconnectDelay * Math.pow(2, reconnectAttempt),
      opts.maxReconnectDelay,
    );
    reconnectAttempt++;

    setTimeout(() => {
      if (state === 'disconnected' || state === 'error') {
        connect();
      }
    }, delay);
  }

  /** 断开连接 */
  function disconnect(): void {
    opts.autoReconnect = false;
    if (ws) {
      ws.close();
      ws = null;
    }
    cleanup();
    setState('disconnected');
  }

  /** 获取当前连接状态 */
  function getState(): EngineStreamState {
    return state;
  }

  /** 获取事件日志 */
  function getEventLog(limit?: number): EngineStreamEvent[] {
    if (limit !== undefined && limit > 0) {
      return eventLog.slice(-limit);
    }
    return [...eventLog];
  }

  /** 手动发送心跳 */
  function sendHeartbeat(): void {
    if (ws && ws.readyState === WebSocket.OPEN) {
      try {
        ws.send(JSON.stringify({ jsonrpc: '2.0', method: 'ping', params: {} }));
      } catch {
        // 忽略发送错误
      }
    }
  }

  /** 销毁客户端 */
  function destroy(): void {
    disconnect();
    eventLog = [];
  }

  return {
    connect,
    disconnect,
    getState,
    getEventLog,
    sendHeartbeat,
    destroy,
  };
}
