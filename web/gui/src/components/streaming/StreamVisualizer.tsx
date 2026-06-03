/**
 * @module components/streaming/StreamVisualizer
 * @description 流式事件实时可视化组件
 *              将引擎流式事件与画布实时同步，实现求解过程的动态可视化
 *
 * 功能特性：
 * - 实时事件流可视化（时间线、节点图）
 * - 事件统计仪表盘
 * - 事件详情面板
 * - 画布同步高亮
 * - 事件回放功能
 */

import React, { useState, useCallback, useRef, useEffect, useMemo } from 'react';
import { useAppStore } from '@/stores';
import type { EngineStreamEvent, EngineStreamCategory } from '@/types';

// ================================================================
// 类型定义 / Type Definitions
// ================================================================

/**
 * 事件节点数据（用于 Canvas 绘制）
 * Event node data for Canvas rendering
 */
interface EventNode {
  /** 节点唯一标识（格式 "event-{index}"） */
  id: string;
  /** 关联的引擎流式事件 */
  event: EngineStreamEvent;
  /** 节点在画布上的 X 坐标 */
  x: number;
  /** 节点在画布上的 Y 坐标 */
  y: number;
  /** 节点半径（像素） */
  radius: number;
  /** 节点颜色（CSS 颜色值） */
  color: string;
  /** 事件时间戳（毫秒） */
  timestamp: number;
}

/**
 * 可视化配置
 * Visualization configuration
 */
interface VisualizationConfig {
  /** 可视化模式：时间线（水平排列）或节点图（按类别分组） */
  mode: 'timeline' | 'graph';
  /** 是否自动滚动到最新事件 */
  autoScroll: boolean;
  /** 是否显示事件详情面板 */
  showDetails: boolean;
  /** 高亮持续时间（毫秒） */
  highlightDuration: number;
  /** 最大可见事件数（超出时只显示最近的 N 个） */
  maxVisibleEvents: number;
}

// ================================================================
// 事件可视化组件 / Event Visualization Component
// ================================================================

/**
 * StreamVisualizer - 流式事件实时可视化
 */
const StreamVisualizer: React.FC = () => {
  const streamingEvents = useAppStore((s) => s.streamingEvents);
  const isStreaming = useAppStore((s) => s.isStreaming);

  const [config, setConfig] = useState<VisualizationConfig>({
    mode: 'timeline',
    autoScroll: true,
    showDetails: true,
    highlightDuration: 3000,
    maxVisibleEvents: 100,
  });

  const [selectedEvent, setSelectedEvent] = useState<EngineStreamEvent | null>(null);
  const [highlightedNodes, setHighlightedNodes] = useState<Set<string>>(new Set());
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const containerRef = useRef<HTMLDivElement>(null);
  const animationRef = useRef<number | null>(null);

  // 过滤并限制可见事件数量（只显示最近的 N 个事件）
  const visibleEvents = useMemo(() => {
    return streamingEvents.slice(-config.maxVisibleEvents);
  }, [streamingEvents, config.maxVisibleEvents]);

  /**
   * 计算事件节点在画布上的位置
   * 时间线模式：水平等距排列
   * 节点图模式：按类别分行排列
   */
  const eventNodes = useMemo((): EventNode[] => {
    const nodes: EventNode[] = [];
    const width = containerRef.current?.clientWidth ?? 800;
    const height = containerRef.current?.clientHeight ?? 400;
    const padding = 50;

    visibleEvents.forEach((event, index) => {
      const categoryColors: Record<EngineStreamCategory, string> = {
        engine: '#3fb950',
        normalize: '#58a6ff',
        rewrite: '#a371f7',
        solve: '#f0883e',
        proof: '#f778ba',
        func_block: '#39d353',
        conflict: '#f85149',
        info: '#8b949e',
      };

      if (config.mode === 'timeline') {
        // 时间线模式：水平排列
        const x = padding + (index / Math.max(visibleEvents.length - 1, 1)) * (width - 2 * padding);
        const y = height / 2;
        nodes.push({
          id: `event-${index}`,
          event,
          x,
          y,
          radius: 8,
          color: categoryColors[event.category] || '#888888',
          timestamp: event.timestamp_ms,
        });
      } else {
        // 节点图模式：按类别分组
        const categories: EngineStreamCategory[] = [
          'engine', 'normalize', 'rewrite', 'solve', 'proof', 'func_block', 'conflict', 'info'
        ];
        const categoryIndex = categories.indexOf(event.category);
        const categoryY = padding + (categoryIndex / (categories.length - 1)) * (height - 2 * padding);
        const categoryEvents = visibleEvents.filter(e => e.category === event.category);
        const indexInCategory = categoryEvents.indexOf(event);
        const x = padding + (indexInCategory / Math.max(categoryEvents.length - 1, 1)) * (width - 2 * padding);

        nodes.push({
          id: `event-${index}`,
          event,
          x,
          y: categoryY,
          radius: 6,
          color: categoryColors[event.category] || '#888888',
          timestamp: event.timestamp_ms,
        });
      }
    });

    return nodes;
  }, [visibleEvents, config.mode]);

  /**
   * 绘制可视化画布
   * 包含：背景、网格、连接线、事件节点（带发光效果）、类别标签
   */
  const draw = useCallback(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const width = canvas.width;
    const height = canvas.height;

    // 清空画布
    ctx.fillStyle = '#0d1117';
    ctx.fillRect(0, 0, width, height);

    // 绘制网格
    ctx.strokeStyle = '#21262d';
    ctx.lineWidth = 1;
    const gridSize = 40;
    for (let x = 0; x < width; x += gridSize) {
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, height);
      ctx.stroke();
    }
    for (let y = 0; y < height; y += gridSize) {
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(width, y);
      ctx.stroke();
    }

    // 绘制连接线
    if (config.mode === 'timeline' && eventNodes.length > 1) {
      ctx.strokeStyle = '#30363d';
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.moveTo(eventNodes[0]!.x, eventNodes[0]!.y);
      for (let i = 1; i < eventNodes.length; i++) {
        ctx.lineTo(eventNodes[i]!.x, eventNodes[i]!.y);
      }
      ctx.stroke();
    }

    // 绘制事件节点
    eventNodes.forEach((node) => {
      const isHighlighted = highlightedNodes.has(node.id);
      const isSelected = selectedEvent === node.event;

      // 发光效果
      if (isHighlighted || isSelected) {
        const gradient = ctx.createRadialGradient(
          node.x, node.y, 0,
          node.x, node.y, node.radius * 3
        );
        gradient.addColorStop(0, node.color + '80');
        gradient.addColorStop(1, 'transparent');
        ctx.fillStyle = gradient;
        ctx.beginPath();
        ctx.arc(node.x, node.y, node.radius * 3, 0, Math.PI * 2);
        ctx.fill();
      }

      // 节点圆
      ctx.fillStyle = node.color;
      ctx.beginPath();
      ctx.arc(node.x, node.y, isSelected ? node.radius * 1.5 : node.radius, 0, Math.PI * 2);
      ctx.fill();

      // 边框
      if (isSelected) {
        ctx.strokeStyle = '#ffffff';
        ctx.lineWidth = 2;
        ctx.stroke();
      }
    });

    // 绘制类别标签（节点图模式）
    if (config.mode === 'graph') {
      const categories: EngineStreamCategory[] = [
        'engine', 'normalize', 'rewrite', 'solve', 'proof', 'func_block', 'conflict', 'info'
      ];
      const categoryLabels: Record<EngineStreamCategory, string> = {
        engine: '引擎',
        normalize: '归一化',
        rewrite: '重写',
        solve: '求解',
        proof: '证明',
        func_block: '函数块',
        conflict: '冲突',
        info: '信息',
      };

      ctx.font = '12px sans-serif';
      ctx.fillStyle = '#8b949e';
      ctx.textAlign = 'left';

      categories.forEach((cat, index) => {
        const y = 50 + (index / (categories.length - 1)) * (height - 100);
        ctx.fillText(categoryLabels[cat], 10, y + 4);
      });
    }
  }, [eventNodes, highlightedNodes, selectedEvent, config.mode]);

  // 动画循环
  useEffect(() => {
    const animate = () => {
      draw();
      animationRef.current = requestAnimationFrame(animate);
    };
    animate();

    return () => {
      if (animationRef.current) {
        cancelAnimationFrame(animationRef.current);
      }
    };
  }, [draw]);

  /**
   * 处理画布点击事件
   * 查找点击位置最近的事件节点并选中
   */
  const handleCanvasClick = useCallback((e: React.MouseEvent<HTMLCanvasElement>) => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const rect = canvas.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;

    // 查找点击的节点
    for (const node of eventNodes) {
      const distance = Math.sqrt((x - node.x) ** 2 + (y - node.y) ** 2);
      if (distance <= node.radius * 2) {
        setSelectedEvent(node.event);
        return;
      }
    }

    setSelectedEvent(null);
  }, [eventNodes]);

  /**
   * 高亮最新事件（在 highlightDuration 后自动取消高亮）
   */
  useEffect(() => {
    if (visibleEvents.length > 0) {
      const latestId = `event-${visibleEvents.length - 1}`;
      setHighlightedNodes(new Set([latestId]));

      const timer = setTimeout(() => {
        setHighlightedNodes(new Set());
      }, config.highlightDuration);

      return () => clearTimeout(timer);
    }
  }, [visibleEvents.length, config.highlightDuration]);

  /**
   * 按类别统计可见事件数量
   */
  const stats = useMemo(() => {
    const byCategory: Record<EngineStreamCategory, number> = {
      engine: 0,
      normalize: 0,
      rewrite: 0,
      solve: 0,
      proof: 0,
      func_block: 0,
      conflict: 0,
      info: 0,
    };

    visibleEvents.forEach(event => {
      byCategory[event.category]++;
    });

    return {
      total: visibleEvents.length,
      byCategory,
    };
  }, [visibleEvents]);

  return (
    <div className="stream-visualizer">
      {/* 工具栏 */}
      <div className="stream-visualizer-toolbar">
        <div className="toolbar-left">
          <span className="toolbar-title">实时可视化</span>
          <span className="toolbar-count">{stats.total} 事件</span>
          {isStreaming && <span className="streaming-indicator">STREAMING</span>}
        </div>
        <div className="toolbar-right">
          <button
            className={`toolbar-btn ${config.mode === 'timeline' ? 'active' : ''}`}
            onClick={() => setConfig(c => ({ ...c, mode: 'timeline' }))}
            title="时间线模式"
          >
            ⏱
          </button>
          <button
            className={`toolbar-btn ${config.mode === 'graph' ? 'active' : ''}`}
            onClick={() => setConfig(c => ({ ...c, mode: 'graph' }))}
            title="节点图模式"
          >
            ◉
          </button>
          <button
            className={`toolbar-btn ${config.showDetails ? 'active' : ''}`}
            onClick={() => setConfig(c => ({ ...c, showDetails: !c.showDetails }))}
            title="详情面板"
          >
            📋
          </button>
        </div>
      </div>

      {/* 主内容区 */}
      <div className="stream-visualizer-content" ref={containerRef}>
        {/* 画布 */}
        <canvas
          ref={canvasRef}
          width={containerRef.current?.clientWidth ?? 800}
          height={containerRef.current?.clientHeight ?? 400}
          onClick={handleCanvasClick}
          className="stream-canvas"
        />

        {/* 详情面板 */}
        {config.showDetails && selectedEvent && (
          <div className="event-details-panel">
            <div className="details-header">
              <span className="details-type" style={{ color: selectedEvent.color }}>
                {selectedEvent.type}
              </span>
              <button className="close-btn" onClick={() => setSelectedEvent(null)}>×</button>
            </div>
            <div className="details-content">
              <div className="detail-row">
                <span className="label">类型:</span>
                <span className="value">{selectedEvent.type_name}</span>
              </div>
              <div className="detail-row">
                <span className="label">类别:</span>
                <span className="value">{selectedEvent.category}</span>
              </div>
              <div className="detail-row">
                <span className="label">描述:</span>
                <span className="value">{selectedEvent.description}</span>
              </div>
              <div className="detail-row">
                <span className="label">步骤:</span>
                <span className="value">{selectedEvent.step}/{selectedEvent.total_steps}</span>
              </div>
              {selectedEvent.node_id >= 0 && (
                <div className="detail-row">
                  <span className="label">节点:</span>
                  <span className="value">{selectedEvent.node_id}</span>
                </div>
              )}
              {selectedEvent.progress >= 0 && (
                <div className="detail-row">
                  <span className="label">进度:</span>
                  <span className="value">{(selectedEvent.progress * 100).toFixed(1)}%</span>
                </div>
              )}
            </div>
          </div>
        )}
      </div>

      {/* 统计栏 */}
      <div className="stream-visualizer-stats">
        {Object.entries(stats.byCategory).map(([category, count]) => (
          count > 0 && (
            <span key={category} className="stat-item">
              <span className="stat-dot" style={{
                backgroundColor: {
                  engine: '#3fb950',
                  normalize: '#58a6ff',
                  rewrite: '#a371f7',
                  solve: '#f0883e',
                  proof: '#f778ba',
                  func_block: '#39d353',
                  conflict: '#f85149',
                  info: '#8b949e',
                }[category as EngineStreamCategory]
              }} />
              {category}: {count}
            </span>
          )
        ))}
      </div>
    </div>
  );
};

export default StreamVisualizer;
