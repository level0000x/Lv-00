/**
 * @module components/streaming/StreamTimeline
 * @description 流式事件时间线可视化组件 / Streaming event timeline visualization component
 *
 * 将流式事件按时间顺序显示为垂直时间线，按事件类别分组，
 * 支持折叠/展开、选中高亮和统计摘要。
 * Displays streaming events as a vertical timeline ordered by time,
 * grouped by event category with collapse/expand, selection highlight,
 * and summary statistics.
 */

import React, { useState, useCallback, useMemo } from 'react';
import type { StreamingEvent, EngineStreamEvent, EngineStreamCategory } from '@/types';
import { EngineEventType, getEventCategory } from '@/types';

// ================================================================
// 类型守卫 / Type guards
// ================================================================

/** 类型守卫：检查事件是否为 EngineStreamEvent */
function isEngineStreamEvent(event: StreamingEvent | EngineStreamEvent): event is EngineStreamEvent {
  return 'category' in event && 'timestamp_ms' in event;
}

// ================================================================
// 类别配置 / Category configuration
// ================================================================

interface CategoryConfig {
  label: string;
  labelZh: string;
  color: string;
}

/**
 * 事件类别显示配置（与 StreamEventItem 的 EVENT_STYLES 颜色方案一致）
 * Category display configuration (colors aligned with StreamEventItem EVENT_STYLES)
 */
const CATEGORY_CONFIG: Record<EngineStreamCategory, CategoryConfig> = {
  engine:      { label: 'Engine',      labelZh: '引擎',     color: '#3fb950' },
  normalize:   { label: 'Normalize',   labelZh: '归一化',   color: '#a371f7' },
  rewrite:     { label: 'Rewrite',     labelZh: '重写',     color: '#a371f7' },
  solve:       { label: 'Solve',       labelZh: '求解',     color: '#a371f7' },
  proof:       { label: 'Proof',       labelZh: '证明',     color: '#a371f7' },
  func_block:  { label: 'Func Block',  labelZh: '函数块',   color: '#39d353' },
  conflict:    { label: 'Conflict',    labelZh: '冲突',     color: '#f0883e' },
  info:        { label: 'Info',        labelZh: '信息',     color: '#8b949e' },
};

/** 所有类别的有序列表 */
const ALL_CATEGORIES: EngineStreamCategory[] = [
  'engine', 'normalize', 'rewrite', 'solve', 'proof', 'func_block', 'conflict', 'info',
];

// ================================================================
// 事件样式（复用 StreamEventItem 的颜色方案） / Event styles (reusing StreamEventItem color scheme)
// ================================================================

interface EventStyle {
  icon: string;
  color: string;
  label: string;
  labelZh: string;
}

const EVENT_STYLES: Record<number, EventStyle> = {
  [EngineEventType.ENGINE_START]:  { icon: '\u25B6', color: '#3fb950', label: 'Engine Start',        labelZh: '引擎启动' },
  [EngineEventType.ENGINE_DONE]:   { icon: '\u2714', color: '#3fb950', label: 'Engine Done',         labelZh: '引擎完成' },
  [EngineEventType.ENGINE_PAUSED]: { icon: '\u23F8', color: '#8b949e', label: 'Engine Paused',       labelZh: '引擎暂停' },
  [EngineEventType.NORMALIZE_START]: { icon: '\u25C7', color: '#a371f7', label: 'Normalize Start',     labelZh: '归一化开始' },
  [EngineEventType.NORMALIZE_MERGE]: { icon: '\u2726', color: '#a371f7', label: 'Normalize Merge',     labelZh: '归一化合并' },
  [EngineEventType.NORMALIZE_DONE]:  { icon: '\u2714', color: '#a371f7', label: 'Normalize Done',      labelZh: '归一化完成' },
  [EngineEventType.REWRITE_START]:       { icon: '\u25B6', color: '#a371f7', label: 'Rewrite Start',       labelZh: '重写开始' },
  [EngineEventType.REWRITE_RULE_LOADED]: { icon: '\uD83D\uDCDD', color: '#a371f7', label: 'Rule Loaded',        labelZh: '规则加载' },
  [EngineEventType.REWRITE_MATCH_FOUND]: { icon: '\uD83D\uDD0D', color: '#a371f7', label: 'Match Found',        labelZh: '匹配发现' },
  [EngineEventType.REWRITE_APPLIED]:     { icon: '\u270F', color: '#a371f7', label: 'Rewrite Applied',     labelZh: '重写应用' },
  [EngineEventType.REWRITE_ROLLBACK]:    { icon: '\u21A9', color: '#a371f7', label: 'Rewrite Rollback',    labelZh: '重写回滚' },
  [EngineEventType.REWRITE_DONE]:        { icon: '\u2714', color: '#a371f7', label: 'Rewrite Done',        labelZh: '重写完成' },
  [EngineEventType.SOLVE_START]:              { icon: '\u25B6', color: '#a371f7', label: 'Solve Start',         labelZh: '求解开始' },
  [EngineEventType.SOLVE_EQUATION_EXTRACTED]: { icon: '\u2211', color: '#a371f7', label: 'Equation Extracted',  labelZh: '方程提取' },
  [EngineEventType.SOLVE_GROEBNER_STEP]:      { icon: '\u27F3', color: '#a371f7', label: 'Groebner Step',       labelZh: 'Groebner 步骤' },
  [EngineEventType.SOLVE_VARIABLE_RESOLVED]:  { icon: '\u2714', color: '#a371f7', label: 'Variable Resolved',   labelZh: '变量求解' },
  [EngineEventType.SOLVE_DONE]:               { icon: '\u2714', color: '#a371f7', label: 'Solve Done',          labelZh: '求解完成' },
  [EngineEventType.PROOF_STEP_ADDED]:          { icon: '\u002B', color: '#a371f7', label: 'Proof Step Added',    labelZh: '证明步骤添加' },
  [EngineEventType.PROOF_STEP_APPLIED]:        { icon: '\u2713', color: '#a371f7', label: 'Proof Step Applied',  labelZh: '证明步骤应用' },
  [EngineEventType.PROOF_UNIFY]:               { icon: '\u2261', color: '#a371f7', label: 'Proof Unify',         labelZh: '证明合一' },
  [EngineEventType.PROOF_COLOR_UPDATE]:        { icon: '\u25CF', color: '#a371f7', label: 'Proof Color Update',  labelZh: '证明颜色更新' },
  [EngineEventType.PROOF_DEPENDENCY_CHANGE]:   { icon: '\u21C4', color: '#a371f7', label: 'Proof Dep Change',    labelZh: '证明依赖变更' },
  [EngineEventType.FUNC_BLOCK_PACK_START]:         { icon: '\u25B6', color: '#39d353', label: 'FB Pack Start',       labelZh: '函数块打包开始' },
  [EngineEventType.FUNC_BLOCK_PACK_DONE]:          { icon: '\u2714', color: '#39d353', label: 'FB Pack Done',        labelZh: '函数块打包完成' },
  [EngineEventType.FUNC_BLOCK_INSTANTIATE_START]:  { icon: '\u25B6', color: '#39d353', label: 'FB Instantiate Start', labelZh: '函数块实例化开始' },
  [EngineEventType.FUNC_BLOCK_INSTANTIATE_DONE]:   { icon: '\u2714', color: '#39d353', label: 'FB Instantiate Done',  labelZh: '函数块实例化完成' },
  [EngineEventType.FUNC_BLOCK_PARTIAL_APPLY]:      { icon: '\u223F', color: '#39d353', label: 'FB Partial Apply',    labelZh: '函数块部分应用' },
  [EngineEventType.FUNC_BLOCK_DETERMINISM_CHECK]:  { icon: '\u26A0', color: '#39d353', label: 'FB Determinism',      labelZh: '函数块确定性检查' },
  [EngineEventType.FUNC_BLOCK_CAPTURE_AVOID]:      { icon: '\u21BA', color: '#39d353', label: 'FB Capture Avoid',    labelZh: '函数块捕获避免' },
  [EngineEventType.FUNC_BLOCK_CROSS_BOUNDARY]:     { icon: '\u21C6', color: '#39d353', label: 'FB Cross Boundary',   labelZh: '函数块跨边界' },
  [EngineEventType.CONFLICT_DETECTED]: { icon: '\u26A0', color: '#f0883e', label: 'Conflict Detected',   labelZh: '冲突检测' },
  [EngineEventType.CONSTRAINT_ADDED]:  { icon: '\u002B', color: '#f0883e', label: 'Constraint Added',    labelZh: '约束添加' },
  [EngineEventType.NODE_ADDED]:        { icon: '\u25CF', color: '#f0883e', label: 'Node Added',          labelZh: '节点添加' },
  [EngineEventType.CIRCUIT_TRIP]:      { icon: '\u26A1', color: '#f0883e', label: 'Circuit Trip',        labelZh: '断路器触发' },
  [EngineEventType.ERROR]:             { icon: '\u274C', color: '#f85149', label: 'Error',               labelZh: '错误' },
  [EngineEventType.WARNING]:           { icon: '\u26A0', color: '#d29922', label: 'Warning',             labelZh: '警告' },
  [EngineEventType.INFO]:           { icon: '\u2139', color: '#8b949e', label: 'Info',                labelZh: '信息' },
  [EngineEventType.PROGRESS]:       { icon: '\uD83D\uDCCA', color: '#58a6ff', label: 'Progress',     labelZh: '进度' },
  [EngineEventType.GRAPH_SNAPSHOT]: { icon: '\uD83D\uDDBC', color: '#c9d1d9', label: 'Graph Snapshot', labelZh: '图快照' },
};

function getEventStyle(type: number): EventStyle {
  return EVENT_STYLES[type] ?? EVENT_STYLES[EngineEventType.INFO]!;
}

// ================================================================
// 辅助函数 / Helper functions
// ================================================================

/** 获取事件的时间戳（毫秒） */
function getEventTimestamp(event: StreamingEvent | EngineStreamEvent): number {
  if (isEngineStreamEvent(event)) {
    return event.timestamp_ms;
  }
  return event.timestamp ?? Date.now();
}

/** 获取事件的类别 */
function getEventCategoryFromEvent(event: StreamingEvent | EngineStreamEvent): EngineStreamCategory {
  if (isEngineStreamEvent(event)) {
    return event.category;
  }
  // 旧版 StreamingEvent 使用 getEventCategory 映射
  return getEventCategory(event.type);
}

/** 格式化相对时间（毫秒） */
function formatRelativeTime(ms: number): string {
  if (ms < 1000) return `${ms}ms`;
  if (ms < 60000) return `${(ms / 1000).toFixed(1)}s`;
  const min = Math.floor(ms / 60000);
  const sec = ((ms % 60000) / 1000).toFixed(0);
  return `${min}m ${sec}s`;
}

// ================================================================
// 分组事件类型 / Grouped event type
// ================================================================

interface GroupedEvents {
  category: EngineStreamCategory;
  events: Array<{
    event: StreamingEvent | EngineStreamEvent;
    originalIndex: number;
    relativeTime: number;
  }>;
}

// ================================================================
// 子组件：时间线节点 / Sub-component: Timeline node
// ================================================================

interface TimelineNodeProps {
  event: StreamingEvent | EngineStreamEvent;
  originalIndex: number;
  relativeTime: number;
  isSelected: boolean;
  categoryColor: string;
  onSelect: (event: StreamingEvent | EngineStreamEvent, index: number) => void;
}

const TimelineNode = React.memo<TimelineNodeProps>(({
  event,
  originalIndex,
  relativeTime,
  isSelected,
  categoryColor,
  onSelect,
}) => {
  const isEngine = isEngineStreamEvent(event);
  const eventType = typeof event.type === 'number' ? event.type : 0;
  const style = getEventStyle(eventType);
  const nodeColor = isEngine ? event.color : style.color;

  const handleClick = useCallback(() => {
    onSelect(event, originalIndex);
  }, [event, originalIndex, onSelect]);

  const handleKeyDown = useCallback((e: React.KeyboardEvent) => {
    if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      onSelect(event, originalIndex);
    }
  }, [event, originalIndex, onSelect]);

  const step = isEngine ? event.step : event.stepNumber;

  return (
    <div
      className={`stream-timeline-node ${isSelected ? 'stream-timeline-node--selected' : ''}`}
      onClick={handleClick}
      onKeyDown={handleKeyDown}
      role="button"
      tabIndex={0}
      title={`${style.label} / ${style.labelZh}`}
    >
      {/* 时间刻度 / Time scale */}
      <span className="stream-timeline-node__time">
        +{formatRelativeTime(relativeTime)}
      </span>

      {/* 连接线与节点圆点 / Connector line and node dot */}
      <span className="stream-timeline-node__connector">
        <span
          className="stream-timeline-node__dot"
          style={{
            backgroundColor: isSelected ? nodeColor : categoryColor,
            boxShadow: isSelected ? `0 0 0 3px ${nodeColor}40` : undefined,
          }}
        />
      </span>

      {/* 事件内容 / Event content */}
      <div className="stream-timeline-node__content">
        <span className="stream-timeline-node__icon" style={{ color: nodeColor }}>
          {style.icon}
        </span>
        <span className="stream-timeline-node__step">
          #{step >= 0 ? step : '?'}
        </span>
        <span className="stream-timeline-node__type" style={{ color: nodeColor }}>
          {style.label}
        </span>
        <span className="stream-timeline-node__desc">
          {event.description}
        </span>
      </div>
    </div>
  );
});

TimelineNode.displayName = 'TimelineNode';

// ================================================================
// 子组件：类别分组 / Sub-component: Category group
// ================================================================

interface CategoryGroupProps {
  group: GroupedEvents;
  isCollapsed: boolean;
  selectedIndex: number;
  onToggle: (category: EngineStreamCategory) => void;
  onEventSelect: (event: StreamingEvent | EngineStreamEvent, index: number) => void;
}

const CategoryGroup = React.memo<CategoryGroupProps>(({
  group,
  isCollapsed,
  selectedIndex,
  onToggle,
  onEventSelect,
}) => {
  const config = CATEGORY_CONFIG[group.category];
  const eventCount = group.events.length;

  const handleToggle = useCallback(() => {
    onToggle(group.category);
  }, [group.category, onToggle]);

  const handleKeyDown = useCallback((e: React.KeyboardEvent) => {
    if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      onToggle(group.category);
    }
  }, [group.category, onToggle]);

  return (
    <div className="stream-timeline-group">
      {/* 分组标题 / Group header */}
      <div
        className="stream-timeline-group__header"
        onClick={handleToggle}
        onKeyDown={handleKeyDown}
        role="button"
        tabIndex={0}
        style={{ borderLeftColor: config.color }}
      >
        <span
          className="stream-timeline-group__indicator"
          style={{ color: config.color }}
        >
          {isCollapsed ? '\u25B6' : '\u25BC'}
        </span>
        <span className="stream-timeline-group__color-bar" style={{ backgroundColor: config.color }} />
        <span className="stream-timeline-group__label">
          {config.label}
        </span>
        <span className="stream-timeline-group__label-zh">
          {config.labelZh}
        </span>
        <span className="stream-timeline-group__count" style={{ color: config.color }}>
          {eventCount}
        </span>
      </div>

      {/* 分组内容 / Group content */}
      {!isCollapsed && (
        <div className="stream-timeline-group__body">
          {group.events.map(({ event, originalIndex, relativeTime }) => (
            <TimelineNode
              key={`timeline-${originalIndex}`}
              event={event}
              originalIndex={originalIndex}
              relativeTime={relativeTime}
              isSelected={selectedIndex === originalIndex}
              categoryColor={config.color}
              onSelect={onEventSelect}
            />
          ))}
        </div>
      )}
    </div>
  );
});

CategoryGroup.displayName = 'CategoryGroup';

// ================================================================
// 子组件：统计摘要 / Sub-component: Statistics summary
// ================================================================

interface StatsSummaryProps {
  categoryCounts: Record<string, number>;
  totalEvents: number;
  durationMs: number;
}

const StatsSummary = React.memo<StatsSummaryProps>(({ categoryCounts, totalEvents, durationMs }) => (
  <div className="stream-timeline-stats">
    <div className="stream-timeline-stats__header">
      <span className="stream-timeline-stats__title">
        Summary / 统计摘要
      </span>
      <span className="stream-timeline-stats__total">
        {totalEvents} events / 事件
      </span>
      <span className="stream-timeline-stats__duration">
        {formatRelativeTime(durationMs)}
      </span>
    </div>
    <div className="stream-timeline-stats__breakdown">
      {ALL_CATEGORIES.map((cat) => {
        const count = categoryCounts[cat] ?? 0;
        if (count === 0) return null;
        const config = CATEGORY_CONFIG[cat];
        return (
          <span
            key={cat}
            className="stream-timeline-stats__badge"
            style={{
              backgroundColor: `${config.color}18`,
              color: config.color,
              borderColor: `${config.color}40`,
            }}
          >
            {config.labelZh} {count}
          </span>
        );
      })}
    </div>
  </div>
));

StatsSummary.displayName = 'StatsSummary';

// ================================================================
// 主组件 Props / Main component Props
// ================================================================

interface StreamTimelineProps {
  /** 流式事件列表 / Streaming event list */
  events: (StreamingEvent | EngineStreamEvent)[];
  /** 事件选中回调 / Event selection callback */
  onEventSelect?: (event: StreamingEvent | EngineStreamEvent, index: number) => void;
}

// ================================================================
// 主组件 / Main component
// ================================================================

/**
 * StreamTimeline - 流式事件时间线可视化组件
 * Streaming event timeline visualization component
 *
 * 功能 / Features:
 * - 垂直时间线视图，按时间顺序排列事件 / Vertical timeline view with events in chronological order
 * - 按事件类别分组显示（engine/normalize/rewrite/solve/proof/func_block/conflict/info）
 *   Grouped display by event category
 * - 类别颜色编码，与 StreamEventItem 一致 / Category color coding aligned with StreamEventItem
 * - 左侧相对时间刻度 / Left-side relative time scale
 * - 折叠/展开每个类别组 / Collapse/expand each category group
 * - 点击事件节点高亮选中 / Click event node to highlight selection
 * - 底部统计摘要 / Bottom statistics summary
 * - React.memo 性能优化 / React.memo performance optimization
 */
const StreamTimeline: React.FC<StreamTimelineProps> = ({ events, onEventSelect }) => {
  const [collapsedCategories, setCollapsedCategories] = useState<Set<EngineStreamCategory>>(new Set());
  const [selectedIndex, setSelectedIndex] = useState<number>(-1);

  // 计算基准时间（第一个事件的时间戳）
  const baseTimestamp = useMemo(() => {
    if (events.length === 0) return Date.now();
    return getEventTimestamp(events[0]!);
  }, [events]);

  // 按类别分组事件
  const groupedEvents = useMemo(() => {
    const groups: Record<string, GroupedEvents> = {};

    for (const cat of ALL_CATEGORIES) {
      groups[cat] = { category: cat, events: [] };
    }

    events.forEach((event, index) => {
      const category = getEventCategoryFromEvent(event);
      const relativeTime = getEventTimestamp(event) - baseTimestamp;
      if (!groups[category]) {
        groups[category] = { category, events: [] };
      }
      groups[category]!.events.push({
        event,
        originalIndex: index,
        relativeTime,
      });
    });

    // 过滤掉空分组并保持顺序
    return ALL_CATEGORIES
      .filter((cat) => groups[cat]!.events.length > 0)
      .map((cat) => groups[cat]!);
  }, [events, baseTimestamp]);

  // 计算各类别事件数量
  const categoryCounts = useMemo(() => {
    const counts: Record<string, number> = {};
    for (const group of groupedEvents) {
      counts[group.category] = group.events.length;
    }
    return counts;
  }, [groupedEvents]);

  // 计算总持续时间和总事件数
  const { totalEvents, durationMs } = useMemo(() => {
    const total = events.length;
    let duration = 0;
    if (total > 0) {
      duration = getEventTimestamp(events[total - 1]!) - baseTimestamp;
    }
    return { totalEvents: total, durationMs: duration };
  }, [events, baseTimestamp]);

  // 切换类别折叠状态
  const handleToggleCategory = useCallback((category: EngineStreamCategory) => {
    setCollapsedCategories((prev) => {
      const next = new Set(prev);
      if (next.has(category)) {
        next.delete(category);
      } else {
        next.add(category);
      }
      return next;
    });
  }, []);

  // 选中事件
  const handleEventSelect = useCallback((event: StreamingEvent | EngineStreamEvent, index: number) => {
    setSelectedIndex(index);
    onEventSelect?.(event, index);
  }, [onEventSelect]);

  // 空状态
  if (events.length === 0) {
    return (
      <div className="stream-timeline-empty">
        <span className="stream-timeline-empty__icon">{'\u23F3'}</span>
        <span className="stream-timeline-empty__text">
          No events / 暂无事件
        </span>
      </div>
    );
  }

  return (
    <div className="stream-timeline">
      {/* 时间线头部 / Timeline header */}
      <div className="stream-timeline__header">
        <span className="stream-timeline__title">
          Event Timeline / 事件时间线
        </span>
        <span className="stream-timeline__event-count">
          {totalEvents} events / 事件
        </span>
      </div>

      {/* 时间线主体 / Timeline body */}
      <div className="stream-timeline__body">
        {groupedEvents.map((group) => (
          <CategoryGroup
            key={group.category}
            group={group}
            isCollapsed={collapsedCategories.has(group.category)}
            selectedIndex={selectedIndex}
            onToggle={handleToggleCategory}
            onEventSelect={handleEventSelect}
          />
        ))}
      </div>

      {/* 统计摘要 / Statistics summary */}
      <StatsSummary
        categoryCounts={categoryCounts}
        totalEvents={totalEvents}
        durationMs={durationMs}
      />
    </div>
  );
};

export default React.memo(StreamTimeline);
