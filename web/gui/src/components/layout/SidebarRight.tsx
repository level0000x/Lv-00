/**
 * @module components/layout/SidebarRight
 * @description 右侧边栏面板容器。
 *              包含属性面板、约束图面板、几何叙事面板、类型探索器、
 *              依赖关系面板和增强的流式输出面板。
 *              流式面板支持引擎流式事件分类过滤和自动滚动。
 *
 *              Right sidebar panel container.
 *              Contains properties panel, constraint graph panel, narrative export panel,
 *              type explorer, dependencies panel, and enhanced streaming output panel.
 *              Streaming panel supports engine stream event category filtering and auto-scroll.
 */

import React, { useRef, useEffect, useMemo, useCallback } from 'react';
import { useAppStore, useAIStore } from '@/stores';
import Panel from '@/components/panels/Panel';
import ConstraintGraphPanel from '@/components/panels/ConstraintGraphPanel';
import NarrativeExport from '@/components/panels/NarrativeExport';
import TypeExplorer from '@/components/panels/TypeExplorer';
import { MAX_VISIBLE_STREAM_EVENTS } from '@/utils/constants';
import { getCategoryColor } from '@/utils/categoryColors';
import type { EngineStreamEvent } from '@/types';

// ================================================================
// 子组件：流式事件条目 / Stream Event Item
// ================================================================

/**
 * 单条引擎流式事件的渲染组件。
 *
 * 使用 React.memo 包裹以避免在父组件重渲染时不必要的事件条目重渲染。
 * 仅当事件数据本身变化时才重新渲染。
 *
 * @param event - 引擎流式事件数据
 */
const StreamEventItem: React.FC<{ event: EngineStreamEvent }> = React.memo(({ event }) => {
  const catColor = getCategoryColor(event.category);

  return (
    <div
      className="stream-event-item"
      style={{ borderLeftColor: catColor }}
    >
      <div className="stream-event-header">
        {/* 事件类型 badge */}
        <span
          className="stream-event-type"
          style={{ color: catColor }}
        >
          {event.type}
        </span>
        {/* 步骤号（仅当 step >= 0 时显示） */}
        {event.step >= 0 && (
          <span className="stream-event-step">
            #{event.step}
          </span>
        )}
        {/* 事件描述 */}
        <span className="stream-event-desc">
          {event.description}
        </span>
      </div>
    </div>
  );
});

StreamEventItem.displayName = 'StreamEventItem';

// ================================================================
// 子组件：流式日志条目 / Streaming Log Entry
// ================================================================

/**
 * 单条流式日志条目的渲染组件。
 *
 * 使用 React.memo 包裹以优化大量日志条目的渲染性能。
 *
 * @param entry - 流式日志条目数据
 */
const StreamingLogEntry: React.FC<{
  entry: { id: string; time: string; badge: string; step?: number; description: string };
}> = React.memo(({ entry }) => (
  <div key={entry.id} className="streaming-entry">
    <span className="streaming-time">{entry.time}</span>
    <span
      className={`streaming-badge streaming-badge-${entry.badge.toLowerCase()}`}
    >
      {entry.badge}
    </span>
    {entry.step !== undefined && (
      <span className="streaming-step">#{entry.step}</span>
    )}
    <span className="streaming-desc">{entry.description}</span>
  </div>
));

StreamingLogEntry.displayName = 'StreamingLogEntry';

// ================================================================
// SidebarRight 组件 / SidebarRight Component
// ================================================================

/**
 * SidebarRight - 右侧边栏容器
 *
 * 显示上下文信息面板（从上到下）：
 * 1. Properties / 属性：显示选中元素的详情（ID、坐标）
 * 2. ConstraintGraph / 约束图：FRONTIER 风格的约束关系图
 * 3. NarrativeExport / 几何叙事：Penrose 风格的几何叙事导出
 * 4. TypeExplorer / 类型探索器：类型路径探索
 * 5. Dependencies / 依赖：显示依赖关系树
 * 6. Streaming / 流式输出：增强的流式输出面板，包含：
 *    - 分类过滤器按钮行（带颜色标识和计数 badge）
 *    - 引擎流式事件列表（最多 MAX_VISIBLE_STREAM_EVENTS 条，自动滚动到最新）
 *    - 流式日志条目列表
 */
const SidebarRight: React.FC = () => {
  // ---- 从 AppStore 读取通用状态 ----
  const selectedPoint = useAppStore((s) => s.selectedPoint);
  const streamingEntries = useAppStore((s) => s.streamingEntries);

  // ---- 从 AIStore 读取 AI / 流式事件状态 ----
  const streamingEvents = useAIStore((s) => s.streamingEvents);
  const streamFilters = useAIStore((s) => s.streamFilters);
  const streamingActive = useAIStore((s) => s.streamingActive);
  const toggleStreamFilter = useAIStore((s) => s.toggleStreamFilter);
  const clearStreamEvents = useAIStore((s) => s.clearStreamEvents);
  const clearStreamingEntries = useAppStore((s) => s.clearStreamingEntries);

  // ---- 自动滚动到最新事件 ----
  const eventsEndRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    // 当流式事件数量变化时，自动滚动到最新条目
    eventsEndRef.current?.scrollIntoView({ behavior: 'instant' as ScrollBehavior });
  }, [streamingEvents.length]);

  // ---- 筛选可见事件 / Filter Visible Events ----
  /**
   * 根据 streamFilters 中的启用/禁用状态，
   * 从全部 streamingEvents 中筛选出可见的事件，并截断到 MAX_VISIBLE_EVENTS 条。
   * 使用 useMemo 缓存计算结果，仅在依赖项变化时重新计算。
   */
  const visibleEvents = useMemo(() => {
    // 构建启用类别的集合
    const enabledTypes = new Set(
      streamFilters.filter((f) => f.enabled).map((f) => f.category),
    );

    // 筛选 + 截断（取最新的 MAX_VISIBLE_EVENTS 条）
    const filtered = streamingEvents.filter((ev) =>
      enabledTypes.has(ev.category),
    );
    return filtered.slice(-MAX_VISIBLE_STREAM_EVENTS);
  }, [streamingEvents, streamFilters]);

  // ---- 切换筛选器回调 ----
  const handleFilterToggle = useCallback(
    (filterType: string) => {
      toggleStreamFilter(filterType);
    },
    [toggleStreamFilter],
  );

  // ---- 清除所有流式事件和日志 ----
  const handleClearStreaming = useCallback(() => {
    clearStreamEvents();
    clearStreamingEntries();
  }, [clearStreamEvents, clearStreamingEntries]);

  return (
    <div className="sidebar-right" id="sidebarRight">
      {/* ================================================================ */}
      {/* Properties Panel / 属性面板 */}
      {/* ================================================================ */}
      <Panel title="PROPERTIES / 属性" panelId="props">
        <div className="prop-content" id="propContent">
          {selectedPoint ? (
            <div>
              <div className="prop-row">
                <span className="prop-key">ID / 标识</span>
                <span className="prop-val">n{selectedPoint.id}</span>
              </div>
              <div className="prop-row">
                <span className="prop-key">X</span>
                <span className="prop-val">{selectedPoint.x.toFixed(2)}</span>
              </div>
              <div className="prop-row">
                <span className="prop-key">Y</span>
                <span className="prop-val">{selectedPoint.y.toFixed(2)}</span>
              </div>
            </div>
          ) : (
            <div className="prop-empty">
              选择一个元素查看属性 / Select an element
            </div>
          )}
        </div>
      </Panel>

      {/* ================================================================ */}
      {/* Constraint Graph Panel / 约束图面板（FRONTIER 风格） */}
      {/* ================================================================ */}
      <ConstraintGraphPanel />

      {/* ================================================================ */}
      {/* Narrative Export Panel / 几何叙事面板（Penrose 风格） */}
      {/* ================================================================ */}
      <NarrativeExport />

      {/* ================================================================ */}
      {/* Type Explorer Panel / 类型路径探索器 */}
      {/* ================================================================ */}
      <TypeExplorer />

      {/* ================================================================ */}
      {/* Dependencies Panel / 依赖面板 */}
      {/* ================================================================ */}
      <Panel title="DEPENDENCIES / 依赖" panelId="deps">
        <div className="dep-tree" id="depTree">
          <div className="dep-tree-node dep-tree-root dep-tree-empty">
            暂无依赖关系数据 / No dependency data yet
          </div>
        </div>
      </Panel>

      {/* ================================================================ */}
      {/* Streaming Panel / 流式输出面板（增强版） */}
      {/* ================================================================ */}
      <Panel title={`STREAMING / 流式输出 (${streamingEvents.length})`} panelId="streaming">
        <div className="stream-panel" id="streamingContainer">
          {/* ---- 清除按钮 / Clear Button ---- */}
          <div className="stream-panel-header-actions">
            <button
              className="stream-clear-btn"
              onClick={handleClearStreaming}
              title="清除所有流式事件和日志 / Clear all streaming events and logs"
              type="button"
            >
              CLEAR / 清除
            </button>
          </div>
          {/* ---- 分类过滤器按钮行 / Category Filter Buttons ---- */}
          <div className="stream-panel-filters">
            {streamFilters.map((filter) => (
              <button
                key={filter.category}
                className={`stream-filter-btn ${filter.enabled ? 'active' : 'disabled'}`}
                onClick={() => handleFilterToggle(filter.category)}
                title={`${filter.label} / ${filter.labelZh} (${filter.count})`}
                style={{
                  borderColor: filter.enabled
                    ? getCategoryColor(filter.category)
                    : undefined,
                }}
              >
                <span
                  className="stream-filter-label"
                  style={{
                    color: filter.enabled
                      ? getCategoryColor(filter.category)
                      : undefined,
                  }}
                >
                  {filter.label} / {filter.labelZh}
                </span>
                <span className="stream-filter-count">{filter.count}</span>
              </button>
            ))}
          </div>

          {/* ---- 上部：引擎流式事件列表 / Engine Stream Events ---- */}
          <div className="stream-panel-events">
            {visibleEvents.length === 0 ? (
              <div className="stream-panel-empty">
                {streamingActive
                  ? '等待引擎事件... / Waiting for engine events...'
                  : '暂无引擎事件 / No engine events'}
              </div>
            ) : (
              visibleEvents.map((event, idx) => (
                <StreamEventItem
                  key={`event-${idx}-${event.step}`}
                  event={event}
                />
              ))
            )}
            {/* 自动滚动的锚点元素 */}
            <div ref={eventsEndRef} />
          </div>

          {/* ---- 下部：流式日志条目列表 / Streaming Entries ---- */}
          <div className="streaming-log-container">
            {streamingEntries.length === 0 ? (
              <div className="log-empty-msg">
                暂无流式输出 / No streaming output
              </div>
            ) : (
              <div
                className={`streaming-log ${streamingActive ? 'streaming-active' : ''}`}
              >
                {streamingEntries.map((entry) => (
                  <StreamingLogEntry key={entry.id} entry={entry} />
                ))}
              </div>
            )}
          </div>
        </div>
      </Panel>
    </div>
  );
};

export default React.memo(SidebarRight);
