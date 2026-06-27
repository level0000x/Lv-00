import React from 'react';
import { ViewKey, PanelKey } from '../L5-core/types';
import { useUIStore } from '../L5-core/store/uiStore';

interface HeaderProps {
  version?: string;
}

const VIEW_LABELS: Record<ViewKey, string> = {
  canvas: '\u2B21 画布 Canvas',
  text: '\u270E 文本 Text',
  table: '\u2637 表格 Table',
  tree: '\u2261 树 Tree',
  terminal: '\u25B8 终端 Terminal',
  topology: '\u29BF 拓扑 Topology',
};

const TAB_PANELS: { key: PanelKey; icon: string; label: string }[] = [
  { key: 'formula', icon: 'F', label: '公式 Formula' },
  { key: 'graph', icon: 'G', label: '约束图 Graph' },
  { key: 'block', icon: 'B', label: '函数块 Block' },
  { key: 'proof', icon: 'P', label: '证明 Proof' },
  { key: 'type', icon: 'T', label: '类型 Type' },
  { key: 'recurse', icon: 'R', label: '递归 Recurse' },
  { key: 'engine', icon: 'E', label: '引擎 Engine' },
  { key: 'debug', icon: 'D', label: '调试 Debug' },
  { key: 'help', icon: '?', label: '帮助 Help' },
];

const Header: React.FC<HeaderProps> = ({ version = '3.4.0' }) => {
  const theme = useUIStore((s) => s.theme);
  const setTheme = useUIStore((s) => s.setTheme);
  const activePanel = useUIStore((s) => s.activePanel);
  const setActivePanel = useUIStore((s) => s.setActivePanel);
  const activeView = useUIStore((s) => s.activeView);
  const openFloatingPanel = useUIStore((s) => s.openFloatingPanel);

  return (
    <>
      <div className="title-bar">
        <div className="title-bar-left">
          <div className="title-bar-logo">L</div>
          <span className="title-bar-brand">LV-00</span>
          <span className="title-bar-version">v{version}</span>
        </div>

        <div className="title-bar-center">
          <span className="title-bar-view-indicator">{VIEW_LABELS[activeView]}</span>
          <span style={{ fontSize: 10, color: 'var(--color-text-muted)', marginLeft: 8 }}>
            <span className="kbd" style={{ fontSize: 9 }}>Ctrl+P</span>
          </span>
        </div>

        <div className="title-bar-actions">
          <button
            className="btn-icon"
            onClick={() => openFloatingPanel(activePanel)}
            title="弹出面板 Pop-out Panel"
          >
            {'\u29C9'}
          </button>
          <button
            className="btn-icon"
            onClick={() => setTheme(theme === 'dark' ? 'light' : 'dark')}
            title="切换主题 Toggle Theme (Ctrl+T)"
          >
            {theme === 'dark' ? '\u2600' : '\u263E'}
          </button>
        </div>
      </div>

      <div className="tab-bar" role="tablist">
        {TAB_PANELS.map((tab) => (
          <button
            key={tab.key}
            className={`module-tab ${activePanel === tab.key ? 'active' : ''}`}
            onClick={() => setActivePanel(tab.key)}
            role="tab"
            aria-selected={activePanel === tab.key}
          >
            <span className="tab-icon">{tab.icon}</span>
            {tab.label}
          </button>
        ))}
      </div>
    </>
  );
};

export default Header;
