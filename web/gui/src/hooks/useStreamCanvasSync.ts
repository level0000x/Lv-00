/**
 * @module hooks/useStreamCanvasSync
 * @description 流式事件与画布同步 Hook
 *              将引擎流式事件实时同步到画布，实现求解过程的动态可视化
 *
 * 功能特性：
 * - 监听流式事件并更新画布状态
 * - 节点高亮动画
 * - 约束添加动画
 * - 求解进度可视化
 * - 事件回放功能
 */

import { useCallback, useEffect, useRef, useState } from 'react';
import { useAppStore } from '@/stores';
import type { EngineStreamEvent } from '@/types';

// ================================================================
// 类型定义
// ================================================================

interface HighlightState {
  nodeId: number;
  color: string;
  startTime: number;
  duration: number;
  type: 'pulse' | 'glow' | 'ripple';
}

interface CanvasSyncConfig {
  /** 高亮持续时间（毫秒） */
  highlightDuration: number;
  /** 动画帧率 */
  animationFps: number;
  /** 是否启用回放 */
  enableReplay: boolean;
  /** 回放速度 */
  replaySpeed: number;
  /** 事件缓冲区大小 */
  eventBufferSize: number;
}

interface CanvasSyncState {
  /** 当前高亮的节点 */
  highlights: Map<number, HighlightState>;
  /** 是否正在回放 */
  isReplaying: boolean;
  /** 回放进度 */
  replayProgress: number;
  /** 当前处理的事件索引 */
  currentEventIndex: number;
  /** 统计信息 */
  stats: {
    totalEvents: number;
    processedEvents: number;
    highlightsTriggered: number;
  };
}

// ================================================================
// 默认配置
// ================================================================

const DEFAULT_CONFIG: CanvasSyncConfig = {
  highlightDuration: 2000,
  animationFps: 60,
  enableReplay: true,
  replaySpeed: 1.0,
  eventBufferSize: 1000,
};

// ================================================================
// Hook 实现
// ================================================================

/**
 * useStreamCanvasSync - 流式事件与画布同步 Hook
 *
 * 监听引擎流式事件，将求解过程实时可视化到画布上。
 */
export function useStreamCanvasSync(config: Partial<CanvasSyncConfig> = {}) {
  const finalConfig: CanvasSyncConfig = { ...DEFAULT_CONFIG, ...config };

  const streamingEvents = useAppStore((s) => s.streamingEvents);

  const [state, setState] = useState<CanvasSyncState>({
    highlights: new Map(),
    isReplaying: false,
    replayProgress: 0,
    currentEventIndex: 0,
    stats: {
      totalEvents: 0,
      processedEvents: 0,
      highlightsTriggered: 0,
    },
  });

  const eventBufferRef = useRef<EngineStreamEvent[]>([]);
  const animationFrameRef = useRef<number | null>(null);
  const lastProcessedIndexRef = useRef(0);
  /** 活跃动画数量跟踪 ref，用于控制 rAF 循环的启停 */
  const activeAnimationCountRef = useRef(0);

  // 处理单个事件
  const processEvent = useCallback((event: EngineStreamEvent) => {
    // 根据事件类型处理
    switch (event.type) {
      case 'NODE_ADDED':
        // 节点添加事件：高亮新节点
        if (event.node_id >= 0) {
          addHighlight(event.node_id, '#3fb950', 'pulse');
        }
        break;

      case 'CONSTRAINT_ADDED':
        // 约束添加事件：高亮相关节点
        if (event.node_id >= 0) {
          addHighlight(event.node_id, '#58a6ff', 'glow');
        }
        break;

      case 'NORMALIZE_MERGE':
        // 合并事件：高亮被合并的节点
        if (event.node_id >= 0) {
          addHighlight(event.node_id, '#a371f7', 'ripple');
        }
        break;

      case 'SOLVE_VARIABLE_RESOLVED':
        // 变量求解事件：高亮求解的节点
        if (event.node_id >= 0) {
          addHighlight(event.node_id, '#f0883e', 'pulse');
        }
        break;

      case 'PROOF_STEP_APPLIED':
        // 证明步骤事件：高亮相关节点
        if (event.node_id >= 0) {
          addHighlight(event.node_id, '#f778ba', 'glow');
        }
        break;

      case 'CONFLICT_DETECTED':
        // 冲突检测事件：红色警告高亮
        if (event.node_id >= 0) {
          addHighlight(event.node_id, '#f85149', 'ripple');
        }
        break;

      case 'GRAPH_SNAPSHOT':
        // 图快照事件：更新画布
        if (event.graph_snapshot) {
          // 触发画布重绘（通过 state 变更自动触发）
        }
        break;

      default:
        break;
    }

    // 更新统计
    setState(prev => ({
      ...prev,
      stats: {
        ...prev.stats,
        processedEvents: prev.stats.processedEvents + 1,
      },
    }));
  }, []);

  // 添加高亮
  const addHighlight = useCallback((
    nodeId: number,
    color: string,
    type: HighlightState['type'] = 'pulse'
  ) => {
    const highlight: HighlightState = {
      nodeId,
      color,
      startTime: Date.now(),
      duration: finalConfig.highlightDuration,
      type,
    };

    setState(prev => {
      const newHighlights = new Map(prev.highlights);
      const isNew = !newHighlights.has(nodeId);
      newHighlights.set(nodeId, highlight);
      // 仅在新增高亮时递增活跃动画计数
      if (isNew) {
        activeAnimationCountRef.current++;
      }
      return {
        ...prev,
        highlights: newHighlights,
        stats: {
          ...prev.stats,
          highlightsTriggered: prev.stats.highlightsTriggered + 1,
        },
      };
    });
  }, [finalConfig.highlightDuration]);

  // 清理过期高亮
  const cleanupHighlights = useCallback(() => {
    const now = Date.now();
    setState(prev => {
      const newHighlights = new Map(prev.highlights);
      let changed = false;

      for (const [nodeId, highlight] of newHighlights) {
        if (now - highlight.startTime > highlight.duration) {
          newHighlights.delete(nodeId);
          changed = true;
          // 过期高亮递减活跃动画计数
          activeAnimationCountRef.current = Math.max(0, activeAnimationCountRef.current - 1);
        }
      }

      return changed ? { ...prev, highlights: newHighlights } : prev;
    });
  }, []);

  /**
   * 启动 rAF 动画循环。
   * 仅在有活跃动画（activeAnimationCountRef > 0）时运行，
   * 动画全部结束后自动停止，避免空转浪费 CPU 资源。
   */
  const startAnimationLoop = useCallback(() => {
    // 如果已在运行则不重复启动
    if (animationFrameRef.current !== null) return;

    const animate = () => {
      cleanupHighlights();
      // 没有活跃动画时停止循环
      if (activeAnimationCountRef.current <= 0) {
        animationFrameRef.current = null;
        return;
      }
      animationFrameRef.current = requestAnimationFrame(animate);
    };

    animationFrameRef.current = requestAnimationFrame(animate);
  }, [cleanupHighlights]);

  /** 停止 rAF 动画循环 */
  const stopAnimationLoop = useCallback(() => {
    if (animationFrameRef.current !== null) {
      cancelAnimationFrame(animationFrameRef.current);
      animationFrameRef.current = null;
    }
  }, []);

  /**
   * 监听高亮状态变化，动态启停 rAF 动画循环。
   * 有活跃高亮时启动循环，无活跃高亮时停止循环。
   */
  useEffect(() => {
    const currentHighlights = state.highlights;
    if (currentHighlights.size > 0) {
      startAnimationLoop();
    } else {
      stopAnimationLoop();
    }
  }, [state.highlights, startAnimationLoop, stopAnimationLoop]);

  // 组件卸载时清理 rAF
  useEffect(() => {
    return () => {
      stopAnimationLoop();
    };
  }, [stopAnimationLoop]);

  // 监听新事件
  useEffect(() => {
    const newEvents = streamingEvents.slice(lastProcessedIndexRef.current);

    if (newEvents.length > 0) {
      // 缓冲事件
      eventBufferRef.current.push(...newEvents);
      if (eventBufferRef.current.length > finalConfig.eventBufferSize) {
        eventBufferRef.current = eventBufferRef.current.slice(-finalConfig.eventBufferSize);
      }

      // 处理新事件
      newEvents.forEach(event => {
        processEvent(event);
      });

      lastProcessedIndexRef.current = streamingEvents.length;

      // 更新统计
      setState(prev => ({
        ...prev,
        stats: {
          ...prev.stats,
          totalEvents: streamingEvents.length,
        },
      }));
    }
  }, [streamingEvents, processEvent, finalConfig.eventBufferSize]);

  // 开始回放
  const startReplay = useCallback(() => {
    if (eventBufferRef.current.length === 0) return;

    setState(prev => ({
      ...prev,
      isReplaying: true,
      replayProgress: 0,
      currentEventIndex: 0,
    }));
  }, []);

  // 停止回放
  const stopReplay = useCallback(() => {
    setState(prev => ({
      ...prev,
      isReplaying: false,
    }));
  }, []);

  // 回放控制
  useEffect(() => {
    if (!state.isReplaying) return;

    const events = eventBufferRef.current;
    if (state.currentEventIndex >= events.length) {
      stopReplay();
      return;
    }

    const timer = setTimeout(() => {
      const event = events[state.currentEventIndex];
      if (event) {
        processEvent(event);
      }

      setState(prev => ({
        ...prev,
        currentEventIndex: prev.currentEventIndex + 1,
        replayProgress: (prev.currentEventIndex + 1) / events.length,
      }));
    }, 1000 / finalConfig.replaySpeed);

    return () => clearTimeout(timer);
  }, [state.isReplaying, state.currentEventIndex, processEvent, stopReplay, finalConfig.replaySpeed]);

  // 手动触发高亮
  const highlightNode = useCallback((nodeId: number, color?: string, type?: HighlightState['type']) => {
    addHighlight(nodeId, color || '#3fb950', type || 'pulse');
  }, [addHighlight]);

  // 清除所有高亮
  const clearHighlights = useCallback(() => {
    setState(prev => ({
      ...prev,
      highlights: new Map(),
    }));
  }, []);

  // 获取节点高亮状态
  const getNodeHighlight = useCallback((nodeId: number): HighlightState | null => {
    return state.highlights.get(nodeId) || null;
  }, [state.highlights]);

  // 获取高亮动画样式
  const getHighlightStyle = useCallback((nodeId: number): React.CSSProperties => {
    const highlight = state.highlights.get(nodeId);
    if (!highlight) return {};

    const elapsed = Date.now() - highlight.startTime;
    const progress = Math.min(elapsed / highlight.duration, 1);
    const opacity = 1 - progress;

    switch (highlight.type) {
      case 'pulse':
        return {
          boxShadow: `0 0 ${20 * (1 - progress)}px ${highlight.color}`,
          opacity: 0.5 + 0.5 * (1 - progress),
        };
      case 'glow':
        return {
          filter: `drop-shadow(0 0 ${10 * (1 - progress)}px ${highlight.color})`,
        };
      case 'ripple':
        return {
          border: `2px solid ${highlight.color}`,
          borderRadius: '50%',
          opacity,
        };
      default:
        return {};
    }
  }, [state.highlights]);

  return {
    // 状态
    highlights: state.highlights,
    isReplaying: state.isReplaying,
    replayProgress: state.replayProgress,
    stats: state.stats,

    // 方法
    highlightNode,
    clearHighlights,
    getNodeHighlight,
    getHighlightStyle,
    startReplay,
    stopReplay,

    // 配置
    config: finalConfig,
  };
}

export default useStreamCanvasSync;
