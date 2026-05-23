/**
 * @module components/canvas/CanvasOverlay
 * @description Canvas 叠加层组件，包含搜索栏、选择矩形、探针工具提示和右键菜单。
 *
 *              搜索栏通过 Ctrl+F 切换显示，输入内容经 useDebounce 防抖处理后
 *              进行节点过滤，避免高频输入时的性能抖动。
 */

import React, { useState, useCallback, useRef, useEffect } from 'react';
import { useAppStore } from '@/stores';
import { useDebounce } from '@/hooks/useDebounce';
import { SEARCH_DEBOUNCE_MS, MAX_SEARCH_RESULTS } from '@/utils/constants';
import type { Point } from '@/types';

/** 搜索防抖延迟（毫秒）：在用户停止输入后执行过滤 / Search debounce delay (ms) */

/**
 * CanvasOverlay - Canvas 画布叠加层
 *
 * 包含功能：
 * - SearchBar    : 可切换的搜索栏（Ctrl+F），支持防抖过滤
 * - SelectionRect: 框选时显示的矩形区域
 * - ProbeTooltip : 探针工具激活时显示的点信息
 * - ContextMenu  : 右键上下文菜单
 */
const CanvasOverlay: React.FC = () => {
  // ================================================================
  // Store 选择器 —— 按功能聚合以减少订阅粒度
  // ================================================================

  /** 搜索相关状态（从 interactionStore 聚合） */
  const { searchVisible, searchQuery, setSearchVisible, setSearchQuery } =
    useAppStore((s) => ({
      searchVisible: s.searchVisible,
      searchQuery: s.searchQuery,
      setSearchVisible: s.setSearchVisible,
      setSearchQuery: s.setSearchQuery,
    }));

  /** 几何数据 —— 搜索过滤的来源 */
  const points = useAppStore((s) => s.points);

  /** 交互状态 —— 工具与悬停点（用于探针工具提示） */
  const { tool, hoveredPoint } = useAppStore((s) => ({
    tool: s.tool,
    hoveredPoint: s.hoveredPoint,
  }));

  /** 右键菜单状态（从 interactionStore 聚合） */
  const { contextMenu, hideContextMenu } = useAppStore((s) => ({
    contextMenu: s.contextMenu,
    hideContextMenu: s.hideContextMenu,
  }));

  /** 选中点操作 */
  const setSelectedPoint = useAppStore((s) => s.setSelectedPoint);

  // ================================================================
  // 搜索防抖 —— 输入框本地值与 store 搜索词分离
  // ================================================================

  /**
   * 输入框的本地值：随用户按键立即更新，不触发过滤。
   * 当 searchVisible 变化时同步 store 中的 searchQuery 到本地值。
   */
  const [localInput, setLocalInput] = useState(searchQuery);

  /** 防抖后的搜索词：用于实际过滤，用户停止输入 SEARCH_DEBOUNCE_MS 后更新 */
  const debouncedQuery = useDebounce(localInput, SEARCH_DEBOUNCE_MS);

  // 当搜索栏显示/隐藏时同步本地值
  useEffect(() => {
    setLocalInput(searchQuery);
  }, [searchVisible, searchQuery]);

  // 当防抖后的值稳定时，同步到 store 并触发过滤
  useEffect(() => {
    setSearchQuery(debouncedQuery);
  }, [debouncedQuery, setSearchQuery]);

  const [searchResults, setSearchResults] = useState<Point[]>([]);
  const searchInputRef = useRef<HTMLInputElement>(null);

  // ================================================================
  // 副作用
  // ================================================================

  /** 搜索栏显示时自动聚焦输入框 */
  useEffect(() => {
    if (searchVisible && searchInputRef.current) {
      searchInputRef.current.focus();
    }
  }, [searchVisible]);

  /**
   * 根据防抖后的搜索词过滤节点。
   * 匹配规则：节点 ID（格式 n{id}）、X 坐标、Y 坐标。
   * 使用 useMemo 缓存空的 searchResults 数组引用。
   */
  useEffect(() => {
    if (!searchQuery.trim()) {
      setSearchResults([]);
      return;
    }
    const query = searchQuery.toLowerCase();
    const results = points.filter(
      (p) =>
        `n${p.id}`.includes(query) ||
        `${p.x}`.includes(query) ||
        `${p.y}`.includes(query),
    );
    setSearchResults(results.slice(0, MAX_SEARCH_RESULTS));
  }, [searchQuery, points]);

  /** 右键菜单打开时，点击任意位置关闭菜单 */
  useEffect(() => {
    if (!contextMenu) return;
    const handler = () => hideContextMenu();
    document.addEventListener('click', handler);
    return () => document.removeEventListener('click', handler);
  }, [contextMenu, hideContextMenu]);

  // ================================================================
  // 事件处理器
  // ================================================================

  /** 搜索栏键盘事件：Escape 关闭搜索并清空输入 */
  const handleSearchKeyDown = useCallback(
    (e: React.KeyboardEvent) => {
      if (e.key === 'Escape') {
        setSearchVisible(false);
        setLocalInput('');
        setSearchQuery('');
      }
    },
    [setSearchVisible, setSearchQuery],
  );

  /** 点击搜索结果：选中节点，关闭搜索栏 */
  const handleSearchResultClick = useCallback(
    (point: Point) => {
      setSelectedPoint(point);
      setSearchVisible(false);
      setLocalInput('');
      setSearchQuery('');
    },
    [setSelectedPoint, setSearchVisible, setSearchQuery],
  );

  /** 输入变化处理：更新本地值（触发防抖），不清空 store 查询词 */
  const handleInputChange = useCallback(
    (e: React.ChangeEvent<HTMLInputElement>) => {
      setLocalInput(e.target.value);
    },
    [],
  );

  // ================================================================
  // 渲染
  // ================================================================
  return (
    <>
      {/* 搜索栏 —— 通过 Ctrl+F 切换，输入经防抖后过滤节点 */}
      <div className={`search-bar ${searchVisible ? 'active' : ''}`} id="searchBar">
        <input
          ref={searchInputRef}
          type="text"
          className="search-input"
          id="searchInput"
          placeholder="搜索节点... / Search nodes..."
          aria-label="搜索节点"
          value={localInput}
          onChange={handleInputChange}
          onKeyDown={handleSearchKeyDown}
        />
        {searchResults.length > 0 && (
          <div className="search-results" id="searchResults">
            {searchResults.map((p) => (
              <div
                key={p.id}
                className="search-result-item"
                onClick={() => handleSearchResultClick(p)}
              >
                n{p.id} ({p.x.toFixed(1)}, {p.y.toFixed(1)})
              </div>
            ))}
          </div>
        )}
      </div>

      {/* 框选矩形 */}
      <div className="selection-rect" id="selectionRect" />

      {/* 探针工具提示：仅在探针工具激活且有悬停点时渲染 */}
      {tool === 'probe' && hoveredPoint && (
        <div className="probe-tooltip" id="probeTooltip">
          <div className="probe-title">POINT / 点</div>
          <div className="probe-row">
            <span className="probe-key">X</span>
            <span className="probe-val">{hoveredPoint.x.toFixed(2)}</span>
          </div>
          <div className="probe-row">
            <span className="probe-key">Y</span>
            <span className="probe-val">{hoveredPoint.y.toFixed(2)}</span>
          </div>
          <div className="probe-row">
            <span className="probe-key">ID</span>
            <span className="probe-val">n{hoveredPoint.id}</span>
          </div>
        </div>
      )}

      {/* 右键上下文菜单 */}
      {contextMenu && (
        <div
          className="context-menu"
          id="contextMenu"
          style={{ left: contextMenu.x, top: contextMenu.y, display: 'block' }}
        >
          {contextMenu.items.map((item) => (
            <button
              key={item.id}
              className="context-menu-item"
              data-action={item.id}
              onClick={() => {
                hideContextMenu();
              }}
            >
              {item.label}
            </button>
          ))}
        </div>
      )}
    </>
  );
};

export default CanvasOverlay;
