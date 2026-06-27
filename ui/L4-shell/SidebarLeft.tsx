import React, { useState } from 'react';
import { ViewKey, PanelKey } from '../L5-core/types';
import { moduleColor } from '../L1-base/visual';

interface SidebarLeftProps {
  activeView: ViewKey;
  activePanel: PanelKey;
  onViewSelect: (key: ViewKey) => void;
  onPanelSelect: (key: PanelKey) => void;
  searchQuery: string;
  onSearchChange: (q: string) => void;
  sidebarWidth: number;
}

interface ModuleEntry {
  key: ViewKey | PanelKey;
  icon: string;
  label: string;
}

const VIEW_MODULES: ModuleEntry[] = [
  { key: 'canvas', icon: '\u2B21', label: '画布 Canvas' },
  { key: 'text', icon: '\u270E', label: '文本编辑 Text' },
  { key: 'table', icon: '\u2637', label: '数据表 Table' },
  { key: 'tree', icon: '\u2261', label: '证明树 Tree' },
  { key: 'terminal', icon: '\u25B8', label: '终端 Terminal' },
  { key: 'topology', icon: '\u29BF', label: '拓扑 Topology' },
];

const PANEL_MODULES: ModuleEntry[] = [
  { key: 'formula', icon: 'F', label: '公式 Formula' },
  { key: 'graph', icon: 'G', label: '约束图 Graph' },
  { key: 'block', icon: 'B', label: '函数块 Block' },
  { key: 'proof', icon: 'P', label: '证明导航 Proof' },
  { key: 'type', icon: 'T', label: '类型浏览 Type' },
  { key: 'recurse', icon: 'R', label: '递归 Recurse' },
  { key: 'engine', icon: 'E', label: '引擎状态 Engine' },
  { key: 'debug', icon: 'D', label: '调试控制台 Debug' },
  { key: 'help', icon: '?', label: '帮助与快捷键 Help' },
];

const SidebarLeft: React.FC<SidebarLeftProps> = ({
  activeView,
  activePanel,
  onViewSelect,
  onPanelSelect,
  searchQuery,
  onSearchChange,
  sidebarWidth,
}) => {
  const [collapsedViews, setCollapsedViews] = useState(false);
  const [collapsedPanels, setCollapsedPanels] = useState(false);

  const filtered = (items: ModuleEntry[]) => {
    if (!searchQuery) return items;
    const q = searchQuery.toLowerCase();
    return items.filter(m => m.label.toLowerCase().includes(q));
  };

  const toggleSection = (section: 'views' | 'panels') => {
    if (section === 'views') setCollapsedViews(v => !v);
    else setCollapsedPanels(v => !v);
  };

  const filteredViews = filtered(VIEW_MODULES);
  const filteredPanels = filtered(PANEL_MODULES);

  return (
    <aside className="sidebar-left" style={{ width: sidebarWidth }}>
      <div className="search-box">
        <div className="search-input-wrapper">
          <span className="search-icon">{'\u2315'}</span>
          <input
            className="input"
            placeholder="搜索模块 Search..."
            value={searchQuery}
            onChange={(e) => onSearchChange(e.target.value)}
          />
        </div>
      </div>

      {/* Views Section — controls center canvas area */}
      <div
        className={`sidebar-section-header ${collapsedViews ? 'collapsed' : ''}`}
        onClick={() => toggleSection('views')}
      >
        <span>视图 Views</span>
        <span className="section-chevron">{'\u25BE'}</span>
      </div>
      {!collapsedViews && (
        <div>
          {filteredViews.length === 0 && (
            <div style={{ padding: '4px 16px', fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)' }}>无结果 No results</div>
          )}
          {filteredViews.map((m) => (
            <div
              key={m.key}
              className={`sidebar-item ${m.key === activeView ? 'active' : ''}`}
              onClick={() => onViewSelect(m.key as ViewKey)}
            >
              <span className="sidebar-item-icon">{m.icon}</span>
              <span className="sidebar-item-label">{m.label}</span>
            </div>
          ))}
        </div>
      )}

      <div className="sidebar-section-divider" />

      {/* Panels Section — controls right sidebar */}
      <div
        className={`sidebar-section-header ${collapsedPanels ? 'collapsed' : ''}`}
        onClick={() => toggleSection('panels')}
      >
        <span>面板 Panels</span>
        <span className="section-chevron">{'\u25BE'}</span>
      </div>
      {!collapsedPanels && (
        <div style={{ paddingBottom: 8 }}>
          {filteredPanels.length === 0 && (
            <div style={{ padding: '4px 16px', fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)' }}>无结果 No results</div>
          )}
          {filteredPanels.map((m) => {
            const color = moduleColor(m.key as string);
            const active = m.key === activePanel;
            return (
              <div
                key={m.key}
                className={`sidebar-item ${active ? 'active' : ''}`}
                onClick={() => onPanelSelect(m.key as PanelKey)}
              >
                <span className="sidebar-item-icon" style={{ color: active ? color : undefined }}>{m.icon}</span>
                <span className="sidebar-item-label">{m.label}</span>
              </div>
            );
          })}
        </div>
      )}
    </aside>
  );
};

export default SidebarLeft;
