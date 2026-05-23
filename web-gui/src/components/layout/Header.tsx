/**
 * @module components/layout/Header
 * @description 顶部导航栏组件。
 *              包含应用程序标志、版本徽章、面包屑导航、
 *              模块标签页和操作按钮（主题、搜索、导出）。
 */

import React from 'react';
import { useAppStore } from '@/stores';
import { MODULE_TABS } from '@/utils/moduleConfig';
import { APP_VERSION } from '@/utils/constants';

/**
 * Header 组件的 props 接口
 * @property onExport - 导出按钮点击回调
 * @property onImport - 导入按钮点击回调
 */
interface HeaderProps {
  onExport?: () => void;
  onImport?: () => void;
}

/**
 * Header - 顶部导航栏
 *
 * 显示应用程序标志、版本号、面包屑导航、模块标签页和操作按钮。
 * 模块标签页用于在左侧边栏各功能面板之间切换。
 */
const Header: React.FC<HeaderProps> = ({ onExport, onImport }) => {
  const activeModule = useAppStore((s) => s.activeModule);
  const setActiveModule = useAppStore((s) => s.setActiveModule);
  const theme = useAppStore((s) => s.theme);
  const setTheme = useAppStore((s) => s.setTheme);
  const searchVisible = useAppStore((s) => s.searchVisible);
  const setSearchVisible = useAppStore((s) => s.setSearchVisible);

  const handleThemeToggle = (): void => {
    setTheme(theme === 'dark' ? 'light' : 'dark');
  };

  const handleSearchToggle = (): void => {
    setSearchVisible(!searchVisible);
  };

  return (
    <header className="header">
      <span className="header-title" title="LV-00 Geometry Engine">LV-00</span>
      <span className="header-version" aria-label={`版本号 v${APP_VERSION}`}>{`v${APP_VERSION}`}</span>

      <div className="breadcrumb">
        <span className="breadcrumb-current" id="breadcrumbModule">
          {activeModule.toUpperCase()}
        </span>
      </div>

      <div className="module-tabs" role="tablist" aria-label="模块导航">
        {MODULE_TABS.map((tab) => (
          <button
            key={tab.id}
            className={`module-tab ${activeModule === tab.id ? 'active' : ''}`}
            data-module={tab.id}
            data-tooltip={tab.tooltip}
            role="tab"
            aria-selected={activeModule === tab.id}
            onClick={() => setActiveModule(tab.id)}
          >
            <span className="tab-icon">{tab.icon}</span>
            {tab.label}
          </button>
        ))}
      </div>

      <div className="header-actions">
        <button
          className="header-action-btn"
          title="切换主题 / Toggle Theme"
          onClick={handleThemeToggle}
          aria-pressed={theme === 'dark'}
        >
          THEME / 主题
        </button>
        <button
          className="header-action-btn"
          title="搜索 / Search (Ctrl+F)"
          onClick={handleSearchToggle}
          aria-pressed={searchVisible}
        >
          SEARCH / 搜索
        </button>
        <button
          className="header-action-btn"
          title="导入 / Import"
          onClick={onImport}
        >
          IMPORT / 导入
        </button>
        <button
          className="header-action-btn"
          title="导出 / Export"
          onClick={onExport}
        >
          EXPORT / 导出
        </button>
      </div>
    </header>
  );
};

export default React.memo(Header);
