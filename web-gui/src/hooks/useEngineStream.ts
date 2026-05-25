/**
 * @module hooks/useEngineStream
 * @description 引擎流式事件 React Hook。
 *              封装 StreamManager 为 React Hook，管理其生命周期，
 *              并将引擎事件同步到 Zustand aiStore 中。
 *
 *              功能特性：
 *              - 应用级单例：通过 module-level 变量确保全局只有一个 StreamManager 实例
 *              - 自动连接/断开：组件挂载时连接，卸载时断开
 *              - 事件同步：引擎事件自动推送到 aiStore 的 streamingEvents
 *              - 类别过滤同步：StreamManager 的过滤器状态与 aiStore 同步
 *              - 连接状态暴露：UI 可根据连接状态显示指示器
 *
 *              使用方式：
 *              ```tsx
 *              // 在 Layout 组件中初始化（应用级单例）
 *              useEngineStream({ engineUrl: 'ws://localhost:3456' });
 *
 *              // 在任意组件中读取状态
 *              const { connected, engineState, stats } = useEngineStream();
 *              ```
 */

import { useRef, useEffect, useState, useCallback } from 'react';
import {
  StreamManager,
  createStreamManager,
  type StreamManagerConfig,
  type StreamStats,
  type UnifiedStreamEvent,
} from '@/services/streamManager';
import type { EngineStreamState, EngineStreamCategory } from '@/types';
import { useAIStore } from '@/stores';

// ================================================================
// 类型定义
// ================================================================

interface UseEngineStreamOptions extends StreamManagerConfig {
  /** 是否在组件挂载时自动连接引擎（默认 false） */
  autoConnect?: boolean;
}

interface UseEngineStreamReturn {
  /** 引擎 WebSocket 连接状态 */
  connected: boolean;
  /** 引擎连接详细状态 */
  engineState: EngineStreamState;
  /** 流式统计信息 */
  stats: StreamStats;
  /** 手动连接引擎 */
  connectEngine: () => void;
  /** 手动断开引擎 */
  disconnectEngine: () => void;
  /** 切换指定类别的过滤器 */
  toggleCategory: (category: EngineStreamCategory) => void;
  /** 清空事件缓冲区 */
  clearEvents: () => void;
  /** 重置统计信息 */
  resetStats: () => void;
  /** 获取最近 N 条事件 */
  getRecentEvents: (limit?: number) => UnifiedStreamEvent[];
  /** 推送 AI 事件到统一事件流 */
  pushAIEvent: (description: string, category?: EngineStreamCategory) => void;
}

// ================================================================
// 应用级单例
// ================================================================

/** 全局 StreamManager 单例引用 */
let managerInstance: StreamManager | null = null;
/** 全局引用计数（防止多个组件同时卸载导致管理器被销毁） */
let refCount = 0;

/**
 * 获取或创建全局 StreamManager 单例。
 * 使用引用计数管理生命周期：首次创建时 refCount=1，
 * 后续调用仅增加 refCount。
 */
function getOrCreateManager(
  config: StreamManagerConfig,
  onEvent: (event: UnifiedStreamEvent) => void,
  onStateChange: (state: EngineStreamState) => void,
  onStatsUpdate: (stats: StreamStats) => void,
  onError: (error: Error) => void,
): StreamManager {
  if (managerInstance !== null) {
    refCount++;
    return managerInstance;
  }

  managerInstance = createStreamManager(config, {
    onEvent,
    onEngineStateChange: onStateChange,
    onStatsUpdate,
    onError,
  });
  refCount = 1;
  return managerInstance;
}

/**
 * 释放 StreamManager 单例引用。
 * 仅当 refCount 降为 0 时才实际销毁管理器。
 */
function releaseManager(): void {
  if (refCount <= 0) return;
  refCount--;
  if (refCount === 0 && managerInstance !== null) {
    managerInstance.stop();
    managerInstance = null;
  }
}

// ================================================================
// Hook 实现
// ================================================================

/**
 * useEngineStream - 引擎流式事件管理 Hook
 *
 * 提供对全局 StreamManager 单例的 React 集成：
 * - 自动同步引擎事件到 aiStore
 * - 暴露连接状态和统计信息
 * - 提供手动连接/断开/过滤等操作
 *
 * @param options - 配置选项
 * @returns 流式管理器状态和操作方法
 */
export function useEngineStream(options: UseEngineStreamOptions = {}): UseEngineStreamReturn {
  const {
    engineUrl = 'ws://localhost:3456',
    autoConnect = false,
    eventBufferSize = 1000,
    heartbeatInterval = 15000,
  } = options;

  // ---- 本地状态 / Local State ----
  const [connected, setConnected] = useState(false);
  const [engineState, setEngineState] = useState<EngineStreamState>('disconnected');
  const [stats, setStats] = useState<StreamStats>({
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
  });

  // ---- Store 绑定 / Store Bindings ----
  const addStreamEvent = useAIStore((s) => s.addStreamEvent);
  const incrementStreamFilterCount = useAIStore((s) => s.incrementStreamFilterCount);

  // ---- Refs（避免闭包陈旧引用） ----
  const managerRef = useRef<StreamManager | null>(null);
  const addStreamEventRef = useRef(addStreamEvent);
  const incrementStreamFilterCountRef = useRef(incrementStreamFilterCount);

  // 保持 ref 与最新 store action 同步
  useEffect(() => {
    addStreamEventRef.current = addStreamEvent;
  }, [addStreamEvent]);
  useEffect(() => {
    incrementStreamFilterCountRef.current = incrementStreamFilterCount;
  }, [incrementStreamFilterCount]);

  // ---- 回调定义（使用 ref 避免依赖变化导致重建管理器） ----
  const handleEvent = useCallback((event: UnifiedStreamEvent) => {
    // 将引擎事件同步到 aiStore
    if (event.engineEvent) {
      addStreamEventRef.current(event.engineEvent);
      incrementStreamFilterCountRef.current(event.category);
    }
  }, []);

  const handleStateChange = useCallback((state: EngineStreamState) => {
    setEngineState(state);
    setConnected(state === 'connected');
  }, []);

  const handleStatsUpdate = useCallback((newStats: StreamStats) => {
    setStats(newStats);
  }, []);

  const handleError = useCallback((error: Error) => {
    // 错误已由 StreamManager 内部处理，此处仅做日志记录
    console.error('[Lv00 useEngineStream] Engine stream error:', error.message || error);
    // TODO: 可在此处添加 Toast 通知，向用户展示连接错误信息
  }, []);

  // ---- 生命周期管理 ----
  useEffect(() => {
    const manager = getOrCreateManager(
      { engineUrl, eventBufferSize, heartbeatInterval },
      handleEvent,
      handleStateChange,
      handleStatsUpdate,
      handleError,
    );

    managerRef.current = manager;

    if (autoConnect) {
      manager.connectEngine();
    } else {
      manager.start();
    }

    return () => {
      releaseManager();
      managerRef.current = null;
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // ---- 操作方法 ----

  const connectEngine = useCallback(() => {
    managerRef.current?.connectEngine();
  }, []);

  const disconnectEngine = useCallback(() => {
    managerRef.current?.disconnectEngine();
  }, []);

  const toggleCategory = useCallback((category: EngineStreamCategory) => {
    managerRef.current?.toggleCategory(category);
  }, []);

  const clearEvents = useCallback(() => {
    managerRef.current?.clearEventBuffer();
  }, []);

  const resetStats = useCallback(() => {
    managerRef.current?.resetStats();
  }, []);

  const getRecentEvents = useCallback((limit?: number): UnifiedStreamEvent[] => {
    return managerRef.current?.getEventBuffer(limit) ?? [];
  }, []);

  const pushAIEvent = useCallback((description: string, category: EngineStreamCategory = 'info') => {
    managerRef.current?.pushAIEvent(description, category);
  }, []);

  return {
    connected,
    engineState,
    stats,
    connectEngine,
    disconnectEngine,
    toggleCategory,
    clearEvents,
    resetStats,
    getRecentEvents,
    pushAIEvent,
  };
}

export default useEngineStream;
