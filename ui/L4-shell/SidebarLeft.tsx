import React from 'react';
import { ModuleKey } from '../L5-core/types';
import { moduleColor, ICONS } from '../L1-base/visual';

export interface PanelEntry {
  key: ModuleKey;
  icon: string;
  label: string;
}

const PANEL_MODULES: PanelEntry[] = [
  { key: 'formula', icon: ICONS.FORMULA, label: 'Formula' },
  { key: 'graph', icon: ICONS.GRAPH, label: 'Graph' },
  { key: 'block', icon: ICONS.BLOCK, label: 'Block' },
  { key: 'proof', icon: ICONS.PROOF, label: 'Proof' },
  { key: 'type', icon: ICONS.TYPE, label: 'Type' },
  { key: 'recurse', icon: ICONS.RECURSE, label: 'Recurse' },
  { key: 'engine', icon: ICONS.ENGINE, label: 'Engine' },
  { key: 'debug', icon: ICONS.DEBUG, label: 'Debug' },
  { key: 'canvas', icon: ICONS.CANVAS, label: 'Canvas' },
  { key: 'text', icon: ICONS.TEXT, label: 'Text' },
  { key: 'table', icon: ICONS.TABLE, label: 'Table' },
  { key: 'tree', icon: ICONS.TREE, label: 'Tree' },
  { key: 'terminal', icon: ICONS.TERMINAL, label: 'Terminal' },
  { key: 'topology', icon: ICONS.TOPOLOGY, label: 'Topology' },
  { key: 'help', icon: ICONS.HELP, label: 'Help' },
];

interface SidebarLeftProps {
  activeModule: ModuleKey;
  onModuleSelect: (key: ModuleKey) => void;
  searchQuery: string;
  onSearchChange: (q: string) => void;
  sidebarWidth: number;
}

const SidebarLeft: React.FC<SidebarLeftProps> = ({
  activeModule,
  onModuleSelect,
  searchQuery,
  onSearchChange,
  sidebarWidth,
}) => {
  const filtered = PANEL_MODULES.filter(
    (m) => !searchQuery || m.label.toLowerCase().includes(searchQuery.toLowerCase())
  );

  return (
    <aside className="sidebar-left" style={{ width: sidebarWidth }}>
      <div className="search-box">
        <input
          className="input"
          placeholder="Search modules..."
          value={searchQuery}
          onChange={(e) => onSearchChange(e.target.value)}
        />
      </div>
      <div style={{ flex: 1, overflowY: 'auto', padding: 8 }}>
        {filtered.map((panel) => {
          const active = panel.key === activeModule;
          const color = moduleColor(panel.key);
          return (
            <div
              key={panel.key}
              onClick={() => onModuleSelect(panel.key)}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: 8,
                padding: '8px 12px',
                cursor: 'pointer',
                borderRadius: 6,
                fontSize: 'var(--font-size-md)',
                color: active ? 'var(--color-text-primary)' : 'var(--color-text-secondary)',
                background: active ? 'rgba(var(--color-accent-rgb), 0.08)' : 'transparent',
                borderLeft: active ? `3px solid ${color}` : '3px solid transparent',
                transition: 'all var(--transition-fast)',
                marginBottom: 2,
              }}
              onMouseEnter={(e) => {
                if (!active) (e.currentTarget as HTMLElement).style.background = 'var(--color-bg-hover-subtle)';
              }}
              onMouseLeave={(e) => {
                if (!active) (e.currentTarget as HTMLElement).style.background = 'transparent';
              }}
            >
              <span style={{ fontSize: 14, width: 20, textAlign: 'center' }}>{panel.icon}</span>
              <span>{panel.label}</span>
            </div>
          );
        })}
      </div>
    </aside>
  );
};

export default SidebarLeft;
