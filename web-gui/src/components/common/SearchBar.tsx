/**
 * @module components/common/SearchBar
 * @description 节点搜索栏组件，用于搜索点和元素。
 *              支持按节点 ID 和坐标值过滤。
 *              Node search bar component for searching points and elements.
 *              Supports filtering by node ID and coordinate values.
 */

import React, { useState, useCallback, useRef, useEffect } from 'react';
import { useAppStore } from '@/stores';
import type { Point } from '@/types';

/**
 * SearchBar - 节点搜索组件 / Node search component
 *
 * 提供文本输入框用于按 ID 或坐标搜索点。
 * 搜索结果以下拉列表形式显示在输入框下方。
 * 通过 store 中的 searchVisible 状态控制显示/隐藏。
 *
 * Provides a text input for searching points by ID or coordinates.
 * Search results are displayed as a dropdown list below the input.
 * Visibility is controlled via the store's searchVisible state.
 */
const SearchBar: React.FC = () => {
  const [query, setQuery] = useState('');
  const [results, setResults] = useState<Point[]>([]);
  const inputRef = useRef<HTMLInputElement>(null);

  const points = useAppStore((s) => s.points);
  const setSelectedPoint = useAppStore((s) => s.setSelectedPoint);
  const addToast = useAppStore((s) => s.addToast);
  const searchVisible = useAppStore((s) => s.searchVisible);
  const setSearchVisible = useAppStore((s) => s.setSearchVisible);

  // 根据查询词过滤点
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

  const handleSelect = useCallback(
    (point: Point) => {
      setSelectedPoint(point);
      setQuery('');
      setSearchVisible(false);
      addToast('info', `选中节点 n${point.id} / Selected node n${point.id}`);
    },
    [setSelectedPoint, setSearchVisible, addToast],
  );

  const handleKeyDown = useCallback(
    (e: React.KeyboardEvent) => {
      if (e.key === 'Escape') {
        setSearchVisible(false);
        setQuery('');
      }
    },
    [setSearchVisible],
  );

  if (!searchVisible) return null;

  return (
    <div className="search-bar active">
      <input
        ref={inputRef}
        type="text"
        className="search-input"
        placeholder="搜索节点... / Search nodes..."
        value={query}
        onChange={(e) => setQuery(e.target.value)}
        onKeyDown={handleKeyDown}
        autoFocus
      />
      {results.length > 0 && (
        <div className="search-results">
          {results.map((p) => (
            <div
              key={p.id}
              className="search-result-item"
              onClick={() => handleSelect(p)}
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
