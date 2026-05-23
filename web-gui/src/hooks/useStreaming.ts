/**
 * @module hooks/useStreaming
 * @description Custom hook for managing SSE (Server-Sent Events) connections,
 *              parsing streaming events, auto-reconnection, and event filtering.
 *              Integrates with the Zustand store for state management.
 *
 *              自定义 Hook，用于管理 SSE（服务器发送事件）连接、
 *              解析流式事件、自动重连和事件过滤。
 *              与 Zustand store 集成进行状态管理。
 */

import { useState, useCallback, useRef, useEffect } from 'react';
import { useAppStore } from '@/stores';
import type { StreamingEvent, StreamFilter } from '@/types';
import { SSE_RECONNECT_DELAY, SSE_MAX_RECONNECT_ATTEMPTS } from '@/utils/constants';
import { getEventCategory } from '@/types';

// ================================================================
// Types / 类型定义
// ================================================================

interface UseStreamingOptions {
  /** SSE endpoint URL (optional, for future real backend integration) */
  /** SSE 端点 URL（可选，用于未来的真实后端集成） */
  url?: string;
  /** Auto-reconnect on connection failure */
  /** 连接失败时自动重连 */
  autoReconnect?: boolean;
  /** Maximum reconnection attempts */
  /** 最大重连尝试次数 */
  maxReconnectAttempts?: number;
  /** Reconnection delay in ms */
  /** 重连延迟（毫秒） */
  reconnectDelay?: number;
  /** Event filter categories to listen to */
  /** 要监听的事件过滤类别 */
  filterCategories?: string[];
}

interface UseStreamingReturn {
  /** Whether the SSE connection is currently active */
  /** SSE 连接是否当前处于活动状态 */
  connected: boolean;
  /** Whether the connection is currently reconnecting */
  /** 连接是否当前正在重连 */
  reconnecting: boolean;
  /** Number of reconnection attempts so far */
  /** 目前已尝试的重连次数 */
  reconnectAttempts: number;
  /** All streaming events (from store) */
  /** 所有流式事件（来自 store） */
  events: StreamingEvent[];
  /** Current filter state (from store) */
  /** 当前过滤状态（来自 store） */
  filters: StreamFilter[];
  /** Manually connect to the SSE endpoint */
  /** 手动连接到 SSE 端点 */
  connect: () => void;
  /** Manually disconnect from the SSE endpoint */
  /** 手动断开 SSE 端点连接 */
  disconnect: () => void;
  /** Add a simulated event (for testing) */
  /** 添加模拟事件（用于测试） */
  addSimulatedEvent: (event: StreamingEvent) => void;
  /** Clear all events */
  /** 清除所有事件 */
  clearEvents: () => void;
  /** Toggle a specific filter */
  /** 切换特定过滤器 */
  toggleFilter: (type: string) => void;
}

// ================================================================
// Hook Implementation / Hook 实现
// ================================================================

/**
 * useStreaming - Hook for managing SSE streaming connections
 *                管理 SSE 流式连接的 Hook
 *
 * Features:
 * - SSE connection management (connect/disconnect)
 * - Automatic reconnection with exponential backoff
 * - Event parsing and dispatch to store
 * - Event filtering integration
 * - Simulated event support for development
 *
 * 特性：
 * - SSE 连接管理（连接/断开）
 * - 指数退避自动重连
 * - 事件解析并分发到 store
 * - 事件过滤集成
 * - 开发用模拟事件支持
 */
export function useStreaming(options: UseStreamingOptions = {}): UseStreamingReturn {
  const {
    url,
    autoReconnect = true,
    maxReconnectAttempts = SSE_MAX_RECONNECT_ATTEMPTS,
    reconnectDelay = SSE_RECONNECT_DELAY,
    filterCategories,
  } = options;

  const [connected, setConnected] = useState(false);
  const [reconnecting, setReconnecting] = useState(false);
  const [reconnectAttempts, setReconnectAttempts] = useState(0);

  const eventSourceRef = useRef<EventSource | null>(null);
  const reconnectTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const stepCounterRef = useRef(0);

  // Store bindings
  // Store 绑定
  const streamingEvents = useAppStore((s) => s.streamingEvents);
  const streamFilters = useAppStore((s) => s.streamFilters);
  const addStreamEvent = useAppStore((s) => s.addStreamEvent);
  const clearStreamEvents = useAppStore((s) => s.clearStreamEvents);
  const toggleStreamFilter = useAppStore((s) => s.toggleStreamFilter);
  const incrementStreamFilterCount = useAppStore((s) => s.incrementStreamFilterCount);

  // Cleanup on unmount
  // 组件卸载时清理
  useEffect(() => {
    return () => {
      disconnect();
      if (reconnectTimerRef.current) {
        clearTimeout(reconnectTimerRef.current);
      }
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  /**
   * Parse an SSE message and create a StreamingEvent
   * 解析 SSE 消息并创建 StreamingEvent
   */
  const parseSSEMessage = useCallback(
    (data: string): StreamingEvent | null => {
      try {
        const parsed = JSON.parse(data);
        stepCounterRef.current += 1;

        return {
          type: typeof parsed.type === 'number' ? parsed.type : 0,
          description:
            typeof parsed.description === 'string'
              ? parsed.description
              : typeof parsed.message === 'string'
                ? parsed.message
                : data,
          stepNumber: stepCounterRef.current,
          nodeId: typeof parsed.nodeId === 'number' ? parsed.nodeId : undefined,
          data: parsed.data,
        };
      } catch {
        // If not JSON, treat as plain text event
        // 如果不是 JSON，则作为纯文本事件处理
        stepCounterRef.current += 1;
        return {
          type: 0,
          description: data,
          stepNumber: stepCounterRef.current,
        };
      }
    },
    [],
  );

  /**
   * Connect to the SSE endpoint
   * 连接到 SSE 端点
   */
  const connect = useCallback(() => {
    if (!url) {
      // No URL configured - simulated mode
      // 未配置 URL - 模拟模式
      setConnected(false);
      return;
    }

    // Close existing connection
    // 关闭现有连接
    if (eventSourceRef.current) {
      eventSourceRef.current.close();
    }

    try {
      const es = new EventSource(url);

      es.onopen = () => {
        setConnected(true);
        setReconnecting(false);
        setReconnectAttempts(0);
      };

      es.onmessage = (event) => {
        const streamingEvent = parseSSEMessage(event.data);
        if (streamingEvent) {
          // Check if this event type passes the filter
          // 检查此事件类型是否通过过滤器
          const category = getEventCategory(streamingEvent.type);
          if (!filterCategories || filterCategories.includes(category)) {
            addStreamEvent(streamingEvent);
            incrementStreamFilterCount(category);
          }
        }
      };

      es.onerror = () => {
        setConnected(false);
        es.close();
        eventSourceRef.current = null;

        if (autoReconnect && reconnectAttempts < maxReconnectAttempts) {
          setReconnecting(true);
          const nextAttempt = reconnectAttempts + 1;
          setReconnectAttempts(nextAttempt);

          // Exponential backoff
          // 指数退避
          const delay = reconnectDelay * Math.pow(2, nextAttempt - 1);
          reconnectTimerRef.current = setTimeout(() => {
            connect();
          }, delay);
        } else {
          setReconnecting(false);
        }
      };

      eventSourceRef.current = es;
    } catch {
      setConnected(false);
    }
  }, [
    url,
    autoReconnect,
    maxReconnectAttempts,
    reconnectDelay,
    reconnectAttempts,
    filterCategories,
    parseSSEMessage,
    addStreamEvent,
    incrementStreamFilterCount,
  ]);

  /**
   * Disconnect from the SSE endpoint
   * 断开 SSE 端点连接
   */
  const disconnect = useCallback(() => {
    if (eventSourceRef.current) {
      eventSourceRef.current.close();
      eventSourceRef.current = null;
    }
    if (reconnectTimerRef.current) {
      clearTimeout(reconnectTimerRef.current);
      reconnectTimerRef.current = null;
    }
    setConnected(false);
    setReconnecting(false);
    setReconnectAttempts(0);
  }, []);

  /**
   * Add a simulated event (for testing/development)
   * 添加模拟事件（用于测试/开发）
   */
  const addSimulatedEvent = useCallback(
    (event: StreamingEvent) => {
      addStreamEvent(event);
      const category = getEventCategory(event.type);
      incrementStreamFilterCount(category);
    },
    [addStreamEvent, incrementStreamFilterCount],
  );

  /**
   * Clear all events
   * 清除所有事件
   */
  const clearEvents = useCallback(() => {
    clearStreamEvents();
    stepCounterRef.current = 0;
  }, [clearStreamEvents]);

  /**
   * Toggle a specific filter
   * 切换特定过滤器
   */
  const handleToggleFilter = useCallback(
    (type: string) => {
      toggleStreamFilter(type);
    },
    [toggleStreamFilter],
  );

  return {
    connected,
    reconnecting,
    reconnectAttempts,
    events: streamingEvents,
    filters: streamFilters,
    connect,
    disconnect,
    addSimulatedEvent,
    clearEvents,
    toggleFilter: handleToggleFilter,
  };
}

export default useStreaming;
