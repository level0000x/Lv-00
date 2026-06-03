/**
 * @module components/layout/Header
 * @description 顶部导航栏组件。
 *              包含应用程序标志、版本徽章、面包屑导航、
 *              模块标签页和操作按钮（主题、搜索、导入、导出）。
 *
 *              Top navigation bar component.
 *              Contains app logo, version badge, breadcrumb navigation,
 *              module tabs, and action buttons (theme, search, import, export).
 */

import React, { useCallback } from 'react';
import { useAppStore } from '@/stores';
import { MODULE_TABS } from '@/utils/moduleConfig';
import { APP_VERSION } from '@/utils/constants';
import type { ModuleType } from '@/types';

/**
 * Header 组件的 props 接口
 * @property onExport - 导出按钮点击回调函数
 * @property onImport - 导入按钮点击回调函数
 */
interface HeaderProps {
  onExport?: () => void;
  onImport?: () => void;
}

/**
 * 模块标签页子组件。
 *
 * 使用 React.memo 包裹以避免在父组件重渲染时不必要的标签页重渲染。
 * 仅当标签页的激活状态或配置数据变化时才重新渲染。
 *
 * @param tab - 模块标签页配置对象
 * @param isActive - 当前标签页是否处于激活状态
 */
const ModuleTab: React.FC<{
  tab: (typeof MODULE_TABS)[number];
  isActive: boolean;
  onSelect: (id: ModuleType) => void;
}> = React.memo(({ tab, isActive, onSelect }) => {
  return (
    <button
      key={tab.id}
      className={`module-tab ${isActive ? 'active' : ''}`}
      data-module={tab.id}
      data-tooltip={tab.tooltip}
      role="tab"
      aria-selected={isActive}
      aria-controls="sidebarLeft"
      tabIndex={isActive ? 0 : -1}
      onClick={() => onSelect(tab.id)}
    >
      <span className="tab-icon" aria-hidden="true">{tab.icon}</span>
      {tab.label}
    </button>
  );
});

ModuleTab.displayName = 'ModuleTab';

/**
 * Header - 顶部导航栏
 *
 * 布局结构（从左到右）：
 * 1. 应用程序标志（LV-00）+ 版本徽章
 * 2. 面包屑导航（显示当前模块名称）
 * 3. 模块标签页（tablist，用于在左侧边栏各功能面板之间切换）
 * 4. 操作按钮组（主题切换、搜索、导入、导出）
 *
 * 无障碍特性：
 * - 模块标签页使用 role="tablist" / role="tab" / aria-selected 语义
 * - 操作按钮使用 aria-pressed 表示切换状态
 * - 版本徽章使用 aria-label 提供屏幕阅读器可读信息
 * - 图标使用 aria-hidden="true" 避免屏幕阅读器重复朗读
 */
const Header: React.FC<HeaderProps> = ({ onExport, onImport }) => {
  // ---- Store 订阅：仅订阅本组件需要的切片 ----
  const activeModule = useAppStore((s) => s.activeModule);
  const setActiveModule = useAppStore((s) => s.setActiveModule);
  const theme = useAppStore((s) => s.theme);
  const setTheme = useAppStore((s) => s.setTheme);
  const searchVisible = useAppStore((s) => s.searchVisible);
  const setSearchVisible = useAppStore((s) => s.setSearchVisible);

  /**
   * 切换明暗主题。
   * 当前为深色时切换到浅色，反之亦然。
   */
  const handleThemeToggle = useCallback((): void => {
    setTheme(theme === 'dark' ? 'light' : 'dark');
  }, [theme, setTheme]);

  /**
   * 切换搜索面板的显示/隐藏状态。
   */
  const handleSearchToggle = useCallback((): void => {
    setSearchVisible(!searchVisible);
  }, [searchVisible, setSearchVisible]);

  return (
    <header className="header" role="banner">
      {/* 应用程序标志 */}
      <span className="header-title" title="LV-00 Geometry Engine">LV-00</span>
      {/* 版本徽章 */}
      <span className="header-version" aria-label={`版本号 v${APP_VERSION}`}>{`v${APP_VERSION}`}</span>

      {/* 面包屑导航：显示当前激活模块的大写名称 */}
      <nav className="breadcrumb" aria-label="面包屑导航">
        <span className="breadcrumb-current" id="breadcrumbModule">
          {activeModule.toUpperCase()}
        </span>
      </nav>

      {/* 模块标签页导航（支持横向滚动） */}
      <div
        className="module-tabs-scroll-wrapper"
        style={{ overflowX: 'auto', scrollbarWidth: 'thin' }}
      >
        <div className="module-tabs" role="tablist" aria-label="模块导航">
          {MODULE_TABS.map((tab) => (
            <ModuleTab
              key={tab.id}
              tab={tab}
              isActive={activeModule === tab.id}
              onSelect={setActiveModule}
            />
          ))}
        </div>
      </div>

      {/* 操作按钮组 */}
      <div className="header-actions" role="toolbar" aria-label="操作工具栏">
        {/* 主题切换按钮：aria-pressed 表示当前主题状态 */}
        <button
          className="header-action-btn"
          title="切换主题 / Toggle Theme"
          onClick={handleThemeToggle}
          aria-pressed={theme === 'dark'}
          type="button"
        >
          THEME / 主题
        </button>
        {/* 搜索按钮：aria-pressed 表示搜索面板是否打开 */}
        <button
          className="header-action-btn"
          title="搜索 / Search (Ctrl+F)"
          onClick={handleSearchToggle}
          aria-pressed={searchVisible}
          aria-expanded={searchVisible}
          type="button"
        >
          SEARCH / 搜索
        </button>
        {/* 导入按钮 */}
        <button
          className="header-action-btn"
          title="导入 / Import"
          onClick={onImport}
          type="button"
        >
          IMPORT / 导入
        </button>
        {/* 导出按钮 */}
        <button
          className="header-action-btn"
          title="导出 / Export"
          onClick={onExport}
          type="button"
        >
          EXPORT / 导出
        </button>
      </div>
    </header>
  );
};

export default React.memo(Header);
