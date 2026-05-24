/**
 * @module components/streaming/StreamPanel
 * @description Streaming output panel container with toolbar,
 *              event filters, auto-scroll, timeline view, search, and export.
 *              流式输出面板容器，包含工具栏、事件过滤器、自动滚动、
 *              时间线视图、搜索和导出功能。
 *
 *              使用 8 类别过滤器体系（与 C 核心 stream.h 对齐）：
 *              engine / normalize / rewrite / solve / proof / func_block / conflict / info
 */

import React, { useState, useCallback, useRef, useEffect, useMemo } from 'react';
import { useAppStore } from '@/stores';
import StreamEventItem from './StreamEventItem';
import StreamTimeline from './StreamTimeline';
import StreamSearch from './StreamSearch';
import StreamExport from './StreamExport';

// ================================================================
// 视图模式 / View Modes
// ================================================================

type ViewMode = 'list' | 'timeline';

// ================================================================
// 组件 / Component
// ================================================================

/**
 * StreamPanel - 流式事件输出容器 / Container for streaming event output
 *
 * 功能 / Features:
 * - 工具栏：清除、暂停/继续、自动滚动切换、视图切换、搜索、导出
 * - 8 类别事件过滤器及计数（与 C 核心 stream.h 对齐）
 * - 列表视图 / 时间线视图 双模式切换
 * - 事件搜索与高级过滤
 * - 事件导出（JSON / CSV / Markdown）
 * - 新事件时自动滚动到底部
 * - 连接状态指示器
 */
const StreamPanel: React.FC = () => {
  const streamingEvents = useAppStore((s) => s.streamingEvents);
  const streamFilters = useAppStore((s) => s.streamFilters);
  const clearStreamEvents = useAppStore((s) => s.clearStreamEvents);
  const toggleStreamFilter = useAppStore((s) => s.toggleStreamFilter);
  const isStreaming = useAppStore((s) => s.isStreaming);

  const [autoScroll, setAutoScroll] = useState(true);
  const [paused, setPaused] = useState(false);
  const [viewMode, setViewMode] = useState<ViewMode>('list');
  const [searchVisible, setSearchVisible] = useState(false);
  const [searchFilteredIndices, setSearchFilteredIndices] = useState<number[] | null>(null);
  const scrollRef = useRef<HTMLDivElement>(null);
  const pausedCountRef = useRef(streamingEvents.length);

  // 根据活动过滤器过滤事件；暂停时冻结可见列表
  const filteredEvents = useMemo(() => {
    const source = paused
      ? streamingEvents.slice(0, pausedCountRef.current)
      : streamingEvents;

    if (!paused) {
      pausedCountRef.current = streamingEvents.length;
    }

    const enabledCategories = new Set(
      streamFilters.filter((f) => f.enabled).map((f) => f.category),
    );

    let result = source.filter((event) => {
      return enabledCategories.has(event.category);
    });

    // 应用搜索过滤
    if (searchFilteredIndices !== null) {
      result = result.filter((_, idx) => searchFilteredIndices.includes(idx));
    }

    return result;
  }, [streamingEvents, streamFilters, paused, searchFilteredIndices]);

  // 自动滚动到底部
  useEffect(() => {
    if (autoScroll && scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
  }, [filteredEvents.length, autoScroll]);

  const handleClear = useCallback(() => {
    clearStreamEvents();
    setSearchFilteredIndices(null);
  }, [clearStreamEvents]);

  const handleTogglePause = useCallback(() => {
    setPaused((prev) => !prev);
  }, []);

  const handleToggleAutoScroll = useCallback(() => {
    setAutoScroll((prev) => !prev);
  }, []);

  const handleToggleView = useCallback(() => {
    setViewMode((prev) => (prev === 'list' ? 'timeline' : 'list'));
  }, []);

  const handleToggleSearch = useCallback(() => {
    setSearchVisible((prev) => !prev);
    if (searchVisible) {
      setSearchFilteredIndices(null);
    }
  }, [searchVisible]);

  const handleSearchFilterChange = useCallback((indices: number[]) => {
    setSearchFilteredIndices(indices.length === streamingEvents.length ? null : indices);
  }, [streamingEvents.length]);

  const handleEventSelect = useCallback((_event: unknown, _index: number) => {
    // 预留：事件选中后的联动操作（如高亮画布元素）
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
          {/* 视图切换 / View toggle */}
          <button
            className={`stream-panel-tool-btn ${viewMode === 'timeline' ? 'active' : ''}`}
            onClick={handleToggleView}
            title={viewMode === 'list' ? 'Timeline / 时间线' : 'List / 列表'}
            aria-label={viewMode === 'list' ? 'Timeline / 时间线' : 'List / 列表'}
          >
            {viewMode === 'list' ? '\u23F0' : '\u2630'}
          </button>
          {/* 搜索 / Search */}
          <button
            className={`stream-panel-tool-btn ${searchVisible ? 'active' : ''}`}
            onClick={handleToggleSearch}
            title="Search / 搜索"
            aria-label="Search / 搜索"
          >
            {'\uD83D\uDD0D'}
          </button>
          {/* 导出 / Export */}
          <StreamExport
            events={streamingEvents}
            filteredIndices={searchFilteredIndices ?? undefined}
            fileName={`lv00-stream-${new Date().toISOString().slice(0, 19)}`}
          />
          {/* 暂停/继续 / Pause/Resume */}
          <button
            className="stream-panel-tool-btn"
            onClick={handleTogglePause}
            title={paused ? 'Resume / 继续' : 'Pause / 暂停'}
            aria-label={paused ? 'Resume / 继续' : 'Pause / 暂停'}
          >
            {paused ? '\u25B6' : '\u23F8'}
          </button>
          {/* 自动滚动 / Auto-scroll */}
          <button
            className={`stream-panel-tool-btn ${autoScroll ? 'active' : ''}`}
            onClick={handleToggleAutoScroll}
            title="Auto-scroll / 自动滚动"
            aria-label="Auto-scroll / 自动滚动"
          >
            {'\u21E9'}
          </button>
          {/* 清除 / Clear */}
          <button
            className="stream-panel-tool-btn"
            onClick={handleClear}
            title="Clear / 清除"
            aria-label="Clear / 清除"
          >
            {'\u2715'}
          </button>
        </div>
      </div>

      {/* 搜索面板 / Search Panel */}
      {searchVisible && (
        <StreamSearch
          events={streamingEvents}
          onFilterChange={handleSearchFilterChange}
        />
      )}

      {/* 8 类别过滤器 / 8-Category Filters */}
      <div className="stream-panel-filters">
        {streamFilters.map((filter) => (
          <button
            key={filter.category}
            className={`stream-filter-btn ${filter.enabled ? 'active' : 'disabled'}`}
            onClick={() => toggleStreamFilter(filter.category)}
            title={`${filter.label} / ${filter.labelZh}`}
            aria-pressed={filter.enabled}
            style={filter.enabled ? { borderColor: filter.color } : undefined}
          >
            <span
              className="stream-filter-dot"
              style={filter.enabled ? { backgroundColor: filter.color } : undefined}
            />
            <span className="stream-filter-label">
              {filter.labelZh}
            </span>
            {filter.count > 0 && (
              <span className="stream-filter-count">{filter.count}</span>
            )}
          </button>
        ))}
      </div>

      {/* 事件内容区域 / Event Content Area */}
      <div className="stream-panel-events" ref={scrollRef}>
        {filteredEvents.length === 0 ? (
          <div className="stream-panel-empty">
            <span aria-hidden="true" className="stream-panel-empty-icon">{'\u2139\uFE0F'}</span>
            {totalEvents === 0
              ? 'No events / 暂无事件'
              : 'All events filtered / 所有事件已过滤'}
          </div>
        ) : viewMode === 'timeline' ? (
          <StreamTimeline
            events={filteredEvents}
            onEventSelect={handleEventSelect}
          />
        ) : (
          filteredEvents.map((event, index) => (
            <StreamEventItem
              key={`${event.step}-${event.type}-${index}`}
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
