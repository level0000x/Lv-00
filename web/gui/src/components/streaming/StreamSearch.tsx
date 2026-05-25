/**
 * @module components/streaming/StreamSearch
 * @description 流式事件搜索和过滤组件 / Streaming event search and filter component
 *
 * 提供文本搜索、类别多选过滤、时间范围过滤、步骤范围过滤等功能，
 * 并实时高亮匹配文本，显示匹配计数。
 *
 * Features:
 * - Text search with debounce (300ms) / 文本搜索（防抖 300ms）
 * - Category multi-select filter (checkboxes) / 类别多选过滤（复选框）
 * - Time range filter / 时间范围过滤
 * - Step range filter / 步骤范围过滤
 * - Highlighted search results / 搜索结果高亮
 * - Match count display / 匹配计数显示
 * - One-click clear all filters / 一键清除所有过滤条件
 */

import React, { useState, useCallback, useMemo, useRef, useEffect } from 'react';
import type { EngineStreamEvent, EngineStreamCategory } from '@/types';

// ================================================================
// 类型定义 / Type Definitions
// ================================================================

interface StreamSearchProps {
  events: EngineStreamEvent[];
  onFilterChange: (filteredIndices: number[]) => void;
}

// ================================================================
// 常量 / Constants
// ================================================================

/** 所有引擎事件类别 / All engine stream categories */
const ALL_CATEGORIES: EngineStreamCategory[] = [
  'engine',
  'normalize',
  'rewrite',
  'solve',
  'proof',
  'func_block',
  'conflict',
  'info',
];

/** 类别中英双语标签 / Category bilingual labels */
const CATEGORY_LABELS: Record<EngineStreamCategory, { en: string; zh: string }> = {
  engine:     { en: 'Engine',     zh: '引擎' },
  normalize:  { en: 'Normalize',  zh: '归一化' },
  rewrite:    { en: 'Rewrite',    zh: '重写' },
  solve:      { en: 'Solve',      zh: '求解' },
  proof:      { en: 'Proof',      zh: '证明' },
  func_block: { en: 'Func Block', zh: '函数块' },
  conflict:   { en: 'Conflict',   zh: '冲突' },
  info:       { en: 'Info',       zh: '信息' },
};

/** 防抖延迟 / Debounce delay in ms */
const DEBOUNCE_MS = 300;

// ================================================================
// 辅助函数 / Helper Functions
// ================================================================

/**
 * 获取事件的描述文本 / Get the description text of an event
 */
function getEventDescription(event: EngineStreamEvent): string {
  return event.description ?? '';
}

/**
 * 获取事件的时间戳（毫秒）/ Get event timestamp in milliseconds
 */
function getEventTimestamp(event: EngineStreamEvent): number {
  return event.timestamp_ms;
}

/**
 * 获取事件的步骤编号 / Get event step number
 */
function getEventStep(event: EngineStreamEvent): number {
  return event.step;
}

/**
 * 获取事件类别 / Get event category
 * 对于旧版 StreamingEvent，返回 null（无类别信息）
 */
function getEventCategory(event: EngineStreamEvent): EngineStreamCategory | null {
  return event.category;
}

/**
 * 获取事件类型标识 / Get event type identifier for search
 */
function getEventType(event: EngineStreamEvent): string {
  return `${event.type} ${event.type_name}`;
}

/**
 * 转义正则表达式特殊字符 / Escape regex special characters
 */
function escapeRegex(str: string): string {
  return str.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

/**
 * 高亮匹配文本 / Highlight matched text in a string
 * 返回包含 <mark> 标签的 HTML 字符串
 */
function highlightText(text: string, query: string): string {
  if (!query.trim()) return text;
  const escaped = escapeRegex(query.trim());
  try {
    const regex = new RegExp(`(${escaped})`, 'gi');
    return text.replace(regex, '<mark class="stream-search-highlight">$1</mark>');
  } catch {
    return text;
  }
}

// ================================================================
// 组件 / Component
// ================================================================

const StreamSearch: React.FC<StreamSearchProps> = ({ events, onFilterChange }) => {
  // ---- 搜索文本 / Search text ----
  const [searchText, setSearchText] = useState('');
  const debouncedSearchText = useRef(searchText);
  const debounceTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  // ---- 高级过滤状态 / Advanced filter state ----
  const [showAdvanced, setShowAdvanced] = useState(false);
  const [selectedCategories, setSelectedCategories] = useState<Set<EngineStreamCategory>>(new Set());
  const [timeStart, setTimeStart] = useState('');
  const [timeEnd, setTimeEnd] = useState('');
  const [stepMin, setStepMin] = useState('');
  const [stepMax, setStepMax] = useState('');

  // ---- 搜索框引用 / Search input ref ----
  const searchInputRef = useRef<HTMLInputElement>(null);

  // ================================================================
  // 防抖搜索 / Debounced search
  // ================================================================

  useEffect(() => {
    if (debounceTimerRef.current) {
      clearTimeout(debounceTimerRef.current);
    }
    debounceTimerRef.current = setTimeout(() => {
      debouncedSearchText.current = searchText;
      applyFilters();
    }, DEBOUNCE_MS);

    return () => {
      if (debounceTimerRef.current) {
        clearTimeout(debounceTimerRef.current);
      }
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [searchText, selectedCategories, timeStart, timeEnd, stepMin, stepMax, events]);

  // ================================================================
  // 过滤逻辑 / Filter logic
  // ================================================================

  const applyFilters = useCallback(() => {
    const query = debouncedSearchText.current.trim().toLowerCase();
    const hasCategoryFilter = selectedCategories.size > 0;
    const hasTimeFilter = !!(timeStart || timeEnd);
    const hasStepFilter = !!(stepMin || stepMax);

    const timeStartMs = timeStart ? new Date(timeStart).getTime() : 0;
    const timeEndMs = timeEnd ? new Date(timeEnd).getTime() : Infinity;
    const stepMinNum = stepMin !== '' ? parseInt(stepMin, 10) : -Infinity;
    const stepMaxNum = stepMax !== '' ? parseInt(stepMax, 10) : Infinity;

    const matchedIndices: number[] = [];

    for (let i = 0; i < events.length; i++) {
      const event = events[i]!;

      // 文本搜索 / Text search
      if (query) {
        const desc = getEventDescription(event).toLowerCase();
        const typeStr = getEventType(event).toLowerCase();
        const category = getEventCategory(event);
        const categoryLabel = category
          ? `${CATEGORY_LABELS[category].en} ${CATEGORY_LABELS[category].zh}`.toLowerCase()
          : '';

        const matchesText =
          desc.includes(query) ||
          typeStr.includes(query) ||
          categoryLabel.includes(query);

        if (!matchesText) continue;
      }

      // 类别过滤 / Category filter
      if (hasCategoryFilter) {
        const category = getEventCategory(event);
        if (!category || !selectedCategories.has(category)) continue;
      }

      // 时间范围过滤 / Time range filter
      if (hasTimeFilter) {
        const ts = getEventTimestamp(event);
        if (ts < timeStartMs || ts > timeEndMs) continue;
      }

      // 步骤范围过滤 / Step range filter
      if (hasStepFilter) {
        const step = getEventStep(event);
        if (step < stepMinNum || step > stepMaxNum) continue;
      }

      matchedIndices.push(i);
    }

    onFilterChange(matchedIndices);
  }, [events, onFilterChange, selectedCategories, timeStart, timeEnd, stepMin, stepMax]);

  // ================================================================
  // 匹配计数 / Match count
  // ================================================================

  const filteredIndices = useMemo(() => {
    const query = debouncedSearchText.current.trim().toLowerCase();
    const hasCategoryFilter = selectedCategories.size > 0;
    const hasTimeFilter = !!(timeStart || timeEnd);
    const hasStepFilter = !!(stepMin || stepMax);

    // 如果没有任何过滤条件，返回 null 表示不过滤
    if (!query && !hasCategoryFilter && !hasTimeFilter && !hasStepFilter) {
      return null;
    }

    const timeStartMs = timeStart ? new Date(timeStart).getTime() : 0;
    const timeEndMs = timeEnd ? new Date(timeEnd).getTime() : Infinity;
    const stepMinNum = stepMin !== '' ? parseInt(stepMin, 10) : -Infinity;
    const stepMaxNum = stepMax !== '' ? parseInt(stepMax, 10) : Infinity;

    const matchedIndices: number[] = [];

    for (let i = 0; i < events.length; i++) {
      const event = events[i]!;

      if (query) {
        const desc = getEventDescription(event).toLowerCase();
        const typeStr = getEventType(event).toLowerCase();
        const category = getEventCategory(event);
        const categoryLabel = category
          ? `${CATEGORY_LABELS[category].en} ${CATEGORY_LABELS[category].zh}`.toLowerCase()
          : '';

        const matchesText =
          desc.includes(query) ||
          typeStr.includes(query) ||
          categoryLabel.includes(query);

        if (!matchesText) continue;
      }

      if (hasCategoryFilter) {
        const category = getEventCategory(event);
        if (!category || !selectedCategories.has(category)) continue;
      }

      if (hasTimeFilter) {
        const ts = getEventTimestamp(event);
        if (ts < timeStartMs || ts > timeEndMs) continue;
      }

      if (hasStepFilter) {
        const step = getEventStep(event);
        if (step < stepMinNum || step > stepMaxNum) continue;
      }

      matchedIndices.push(i);
    }

    return matchedIndices;
  }, [events, selectedCategories, timeStart, timeEnd, stepMin, stepMax, searchText]);

  const matchCount = filteredIndices?.length ?? 0;
  const totalCount = events.length;
  const hasActiveFilters = filteredIndices !== null;

  // ================================================================
  // 事件处理 / Event handlers
  // ================================================================

  const handleSearchChange = useCallback((e: React.ChangeEvent<HTMLInputElement>) => {
    setSearchText(e.target.value);
  }, []);

  const handleClearAll = useCallback(() => {
    setSearchText('');
    setSelectedCategories(new Set());
    setTimeStart('');
    setTimeEnd('');
    setStepMin('');
    setStepMax('');
    debouncedSearchText.current = '';
    onFilterChange(events.map((_, i) => i));
    searchInputRef.current?.focus();
  }, [events, onFilterChange]);

  const handleToggleCategory = useCallback((category: EngineStreamCategory) => {
    setSelectedCategories((prev) => {
      const next = new Set(prev);
      if (next.has(category)) {
        next.delete(category);
      } else {
        next.add(category);
      }
      return next;
    });
  }, []);

  const handleToggleAdvanced = useCallback(() => {
    setShowAdvanced((prev) => !prev);
  }, []);

  // ================================================================
  // 高亮描述文本（用于显示）/ Highlighted description for display
  // ================================================================

  // ================================================================
  // 渲染 / Render
  // ================================================================

  return (
    <div className="stream-search">
      {/* 搜索栏 / Search bar */}
      <div className="stream-search-bar">
        <div className="stream-search-input-wrapper">
          <span className="stream-search-icon">{'\uD83D\uDD0D'}</span>
          <input
            ref={searchInputRef}
            type="text"
            className="stream-search-input"
            placeholder="Search events / 搜索事件..."
            value={searchText}
            onChange={handleSearchChange}
          />
          {searchText && (
            <button
              className="stream-search-clear-input"
              onClick={() => {
                setSearchText('');
                searchInputRef.current?.focus();
              }}
              title="Clear / 清除"
            >
              {'\u2715'}
            </button>
          )}
        </div>

        <button
          className="stream-search-advanced-toggle"
          onClick={handleToggleAdvanced}
          title={showAdvanced ? 'Hide filters / 隐藏过滤' : 'Show filters / 显示过滤'}
        >
          {showAdvanced ? '\u25B2' : '\u25BC'}
          <span>{showAdvanced ? 'Hide / 收起' : 'Filter / 过滤'}</span>
        </button>
      </div>

      {/* 高级过滤面板 / Advanced filter panel */}
      {showAdvanced && (
        <div className="stream-search-advanced">
          {/* 类别过滤 / Category filter */}
          <div className="stream-search-filter-group">
            <span className="stream-search-filter-label">
              Category / 类别
            </span>
            <div className="stream-search-checkboxes">
              {ALL_CATEGORIES.map((cat) => (
                <label key={cat} className="stream-search-checkbox-label">
                  <input
                    type="checkbox"
                    className="stream-search-checkbox"
                    checked={selectedCategories.has(cat)}
                    onChange={() => handleToggleCategory(cat)}
                  />
                  <span className="stream-search-checkbox-text">
                    {CATEGORY_LABELS[cat].zh}
                  </span>
                  <span className="stream-search-checkbox-text-en">
                    {CATEGORY_LABELS[cat].en}
                  </span>
                </label>
              ))}
            </div>
          </div>

          {/* 时间范围过滤 / Time range filter */}
          <div className="stream-search-filter-group">
            <span className="stream-search-filter-label">
              Time Range / 时间范围
            </span>
            <div className="stream-search-range">
              <input
                type="datetime-local"
                className="stream-search-range-input"
                value={timeStart}
                onChange={(e) => setTimeStart(e.target.value)}
                title="Start time / 起始时间"
              />
              <span className="stream-search-range-sep">~</span>
              <input
                type="datetime-local"
                className="stream-search-range-input"
                value={timeEnd}
                onChange={(e) => setTimeEnd(e.target.value)}
                title="End time / 结束时间"
              />
            </div>
          </div>

          {/* 步骤范围过滤 / Step range filter */}
          <div className="stream-search-filter-group">
            <span className="stream-search-filter-label">
              Step Range / 步骤范围
            </span>
            <div className="stream-search-range">
              <input
                type="number"
                className="stream-search-range-input stream-search-step-input"
                placeholder="Min / 最小"
                value={stepMin}
                onChange={(e) => setStepMin(e.target.value)}
                min={-1}
                title="Min step / 最小步骤"
              />
              <span className="stream-search-range-sep">~</span>
              <input
                type="number"
                className="stream-search-range-input stream-search-step-input"
                placeholder="Max / 最大"
                value={stepMax}
                onChange={(e) => setStepMax(e.target.value)}
                min={-1}
                title="Max step / 最大步骤"
              />
            </div>
          </div>
        </div>
      )}

      {/* 结果计数与清除 / Result count and clear */}
      <div className="stream-search-status">
        {hasActiveFilters ? (
          <span className="stream-search-count">
            {matchCount} / {totalCount}{' '}
            <span className="stream-search-count-label">
              events matched / 个事件匹配
            </span>
          </span>
        ) : (
          <span className="stream-search-count stream-search-count-all">
            {totalCount}{' '}
            <span className="stream-search-count-label">
              events / 个事件
            </span>
          </span>
        )}
        {hasActiveFilters && (
          <button
            className="stream-search-clear-all"
            onClick={handleClearAll}
            title="Clear all filters / 清除所有过滤"
          >
            {'\u2715'} Clear All / 清除全部
          </button>
        )}
      </div>
    </div>
  );
};

export default StreamSearch;

// ================================================================
// 导出辅助函数供外部使用 / Export helpers for external use
// ================================================================

export { highlightText, getEventDescription, getEventCategory, CATEGORY_LABELS };
