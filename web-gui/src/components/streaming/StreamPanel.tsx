/**
 * @module components/streaming/StreamPanel
 * @description Streaming output panel container with toolbar,
 *              event filters, and auto-scroll functionality.
 *              流式输出面板容器，包含工具栏、事件过滤器和自动滚动功能。
 */

import React, { useState, useCallback, useRef, useEffect, useMemo } from 'react';
import { useAppStore } from '@/stores';
import { EngineEventType, getEventCategory } from '@/types';
import StreamEventItem from './StreamEventItem';

// ================================================================
// 组件 / Component
// ================================================================

/**
 * StreamPanel - 流式事件输出容器 / Container for streaming event output
 *
 * 功能 / Features:
 * - 工具栏：清除、暂停/继续、自动滚动切换 / Toolbar: clear, pause/resume, auto-scroll toggle
 * - 事件类型过滤器及计数 / Event type filters with counts
 * - 事件统计摘要 / Event statistics summary
 * - 新事件时自动滚动到底部 / Auto-scroll to bottom on new events
 */
const StreamPanel: React.FC = () => {
  const streamingEvents = useAppStore((s) => s.streamingEvents);
  const streamFilters = useAppStore((s) => s.streamFilters);
  const clearStreamEvents = useAppStore((s) => s.clearStreamEvents);
  const toggleStreamFilter = useAppStore((s) => s.toggleStreamFilter);
  const isStreaming = useAppStore((s) => s.isStreaming);

  const [autoScroll, setAutoScroll] = useState(true);
  const [paused, setPaused] = useState(false);
  const scrollRef = useRef<HTMLDivElement>(null);
  const pausedCountRef = useRef(streamingEvents.length);

  // 根据活动过滤器过滤事件；暂停时冻结可见列表 / Filter events based on active filters; when paused, freeze the visible list
  const filteredEvents = useMemo(() => {
    const source = paused
      ? streamingEvents.slice(0, pausedCountRef.current)
      : streamingEvents;

    if (!paused) {
      pausedCountRef.current = streamingEvents.length;
    }

    const enabledTypes = new Set(
      streamFilters.filter((f) => f.enabled).map((f) => f.type),
    );

    return source.filter((event) => {
      const category = getEventCategory(event.type);
      return enabledTypes.has(category);
    });
  }, [streamingEvents, streamFilters, paused]);

  // 自动滚动到底部 / Auto-scroll to bottom
  useEffect(() => {
    if (autoScroll && scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
  }, [filteredEvents.length, autoScroll]);

  const handleClear = useCallback(() => {
    clearStreamEvents();
  }, [clearStreamEvents]);

  const handleTogglePause = useCallback(() => {
    setPaused((prev) => !prev);
  }, []);

  const handleToggleAutoScroll = useCallback(() => {
    setAutoScroll((prev) => !prev);
  }, []);

  const totalEvents = streamingEvents.length;
  const filteredCount = filteredEvents.length;

  return (
    <div className="stream-panel">
      {/* 工具栏 / Toolbar */}
      <div className="stream-panel-toolbar">
        <div className="stream-panel-toolbar-left">
          <span className="stream-panel-title">
            STREAM / 流式输出
          </span>
          <span className="stream-panel-count">
            {filteredCount}/{totalEvents}
          </span>
          {isStreaming && (
            <span className="stream-panel-indicator streaming">
              STREAMING
            </span>
          )}
        </div>
        <div className="stream-panel-toolbar-right">
          <button
            className="stream-panel-tool-btn"
            onClick={handleTogglePause}
            title={paused ? 'Resume / 继续' : 'Pause / 暂停'}
          >
            {paused ? '\u25B6' : '\u23F8'}
          </button>
          <button
            className={`stream-panel-tool-btn ${autoScroll ? 'active' : ''}`}
            onClick={handleToggleAutoScroll}
            title="Auto-scroll / 自动滚动"
          >
            {'\u21E9'}
          </button>
          <button
            className="stream-panel-tool-btn"
            onClick={handleClear}
            title="Clear / 清除"
          >
            {'\u2715'}
          </button>
        </div>
      </div>

      {/* 过滤器 / Filters */}
      <div className="stream-panel-filters">
        {streamFilters.map((filter) => (
          <button
            key={filter.type}
            className={`stream-filter-btn ${filter.enabled ? 'active' : 'disabled'}`}
            onClick={() => toggleStreamFilter(filter.type)}
            title={`${filter.label} / ${filter.labelZh}`}
          >
            <span className="stream-filter-label">
              {filter.labelZh}
            </span>
            {filter.count > 0 && (
              <span className="stream-filter-count">{filter.count}</span>
            )}
          </button>
        ))}
      </div>

      {/* 事件列表 / Event List */}
      <div className="stream-panel-events" ref={scrollRef}>
        {filteredEvents.length === 0 ? (
          <div className="stream-panel-empty">
            {totalEvents === 0
              ? 'No events / 暂无事件'
              : 'All events filtered / 所有事件已过滤'}
          </div>
        ) : (
          filteredEvents.map((event, index) => (
            <StreamEventItem
              key={`${event.stepNumber}-${event.type}-${index}`}
              event={event}
              index={index}
            />
          ))
        )}
      </div>
    </div>
  );
};

// ================================================================
// 辅助函数 / Helpers
// ================================================================

export default StreamPanel;
