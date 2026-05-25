/**
 * @module components/common/SearchBar
 * @description 节点搜索栏组件 / Node search bar component.
 *              用于搜索点和元素，支持按节点 ID 和坐标值过滤。
 *              Node search bar for searching points and elements.
 *              Supports filtering by node ID and coordinate values.
 *
 *              搜索结果以下拉列表形式显示在输入框下方，
 *              通过 store 中的 searchVisible 状态控制显示/隐藏。
 *
 *              Search results are displayed as a dropdown below the input.
 *              Visibility is controlled via the store's searchVisible state.
 */

import React, { useState, useCallback, useRef, useEffect } from 'react';
import { useAppStore } from '@/stores';
import type { Point } from '@/types';

/**
 * SearchBar - 节点搜索组件 / Node search component
 *
 * 功能特性 / Features:
 * - 文本输入框用于按 ID 或坐标搜索点
 *   Text input for searching points by ID or coordinates
 * - 搜索结果以下拉列表形式显示
 *   Search results displayed as a dropdown list
 * - 实时过滤（每次输入变化时重新计算）
 *   Real-time filtering (recalculated on each input change)
 * - 通过 store 中的 searchVisible 状态控制显示/隐藏
 *   Visibility controlled via store's searchVisible state
 * - Escape 键关闭搜索栏
 *   Escape key closes the search bar
 * - 自动聚焦输入框
 *   Auto-focuses the input field
 */
const SearchBar: React.FC = () => {
  /** 搜索查询文本 / Search query text */
  const [query, setQuery] = useState('');
  /** 过滤后的搜索结果 / Filtered search results */
  const [results, setResults] = useState<Point[]>([]);
  /** 输入框 DOM 引用 / Input element DOM reference */
  const inputRef = useRef<HTMLInputElement>(null);

  const points = useAppStore((s) => s.points);
  const setSelectedPoint = useAppStore((s) => s.setSelectedPoint);
  const addToast = useAppStore((s) => s.addToast);
  const searchVisible = useAppStore((s) => s.searchVisible);
  const setSearchVisible = useAppStore((s) => s.setSearchVisible);

  /**
   * 根据查询词过滤点
   * 支持按节点 ID（如 "n1"）和坐标值（如 "2.5"）进行模糊匹配。
   * 使用 useMemo 优化，仅在 query 或 points 变化时重新计算。
   *
   * Filter points based on query text.
   * Supports fuzzy matching by node ID (e.g., "n1") and coordinate values (e.g., "2.5").
   * Optimized with useMemo, recalculates only when query or points change.
   */
  useEffect(() => {
    if (!query.trim()) {
      setResults([]);
      return;
    }

    const q = query.toLowerCase();
    const filtered = points.filter(
      (p) =>
        `n${p.id}`.includes(q) ||
        `${p.x.toFixed(1)}`.includes(q) ||
        `${p.y.toFixed(1)}`.includes(q),
    );
    setResults(filtered);
  }, [query, points]);

  /**
   * 选中搜索结果中的某个点
   * 设置当前选中点、清空查询文本、关闭搜索栏并显示 Toast 通知。
   *
   * Select a point from search results.
   * Sets the selected point, clears query text, closes search bar,
   * and shows a Toast notification.
   */
  const handleSelect = useCallback(
    (point: Point) => {
      setSelectedPoint(point);
      setQuery('');
      setSearchVisible(false);
      addToast('info', `选中节点 n${point.id} / Selected node n${point.id}`);
    },
    [setSelectedPoint, setSearchVisible, addToast],
  );

  /**
   * 键盘事件处理
   * Escape 键关闭搜索栏并清空查询文本。
   *
   * Keyboard event handler.
   * Escape key closes the search bar and clears the query text.
   */
  const handleKeyDown = useCallback(
    (e: React.KeyboardEvent) => {
      if (e.key === 'Escape') {
        e.preventDefault();
        setSearchVisible(false);
        setQuery('');
      }
    },
    [setSearchVisible],
  );

  // 搜索栏不可见时不渲染 / Don't render when search bar is not visible
  if (!searchVisible) return null;

  return (
    <div className="search-bar active" role="search" aria-label="搜索节点 / Search nodes">
      <input
        ref={inputRef}
        type="text"
        className="search-input"
        placeholder="搜索节点... / Search nodes..."
        value={query}
        onChange={(e) => setQuery(e.target.value)}
        onKeyDown={handleKeyDown}
        autoFocus
        aria-label="搜索节点 / Search nodes"
        aria-expanded={results.length > 0}
        aria-controls="search-results-list"
        role="combobox"
      />
      {results.length > 0 && (
        <div
          className="search-results"
          id="search-results-list"
          role="listbox"
        >
          {results.map((p) => (
            <div
              key={p.id}
              className="search-result-item"
              onClick={() => handleSelect(p)}
              role="option"
              aria-selected={false}
              tabIndex={0}
              onKeyDown={(e) => {
                // 支持键盘选择 / Support keyboard selection
                if (e.key === 'Enter') {
                  e.preventDefault();
                  handleSelect(p);
                }
              }}
            >
              n{p.id} ({p.x.toFixed(1)}, {p.y.toFixed(1)})
            </div>
          ))}
        </div>
      )}
    </div>
  );
};

export default SearchBar;
