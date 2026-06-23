import React from 'react';
import { ModuleKey } from '../L5-core/types';
import { moduleColor, ICONS } from '../L1-base/visual';

interface HeaderProps {
  activeModule: ModuleKey;
  onModuleChange: (key: ModuleKey) => void;
  version?: string;
  onUndo?: () => void;
  onRedo?: () => void;
  onNormalize?: () => void;
  canUndo?: boolean;
  canRedo?: boolean;
}

const TAB_MODULES: { key: ModuleKey; icon: string; label: string; color: string }[] = [
  { key: 'formula', icon: ICONS.FORMULA, label: 'Formula', color: 'var(--color-module-formula)' },
  { key: 'graph', icon: ICONS.GRAPH, label: 'Graph', color: 'var(--color-module-graph)' },
  { key: 'block', icon: ICONS.BLOCK, label: 'Block', color: 'var(--color-module-block)' },
  { key: 'proof', icon: ICONS.PROOF, label: 'Proof', color: 'var(--color-module-proof)' },
  { key: 'type', icon: ICONS.TYPE, label: 'Type', color: 'var(--color-module-type)' },
  { key: 'recurse', icon: ICONS.RECURSE, label: 'Recurse', color: 'var(--color-module-recurse)' },
  { key: 'engine', icon: ICONS.ENGINE, label: 'Engine', color: 'var(--color-module-engine)' },
  { key: 'debug', icon: ICONS.DEBUG, label: 'Debug', color: 'var(--color-module-debug)' },
  { key: 'help', icon: ICONS.HELP, label: 'Help', color: 'var(--color-module-help)' },
];

const Header: React.FC<HeaderProps> = ({
  activeModule,
  onModuleChange,
  version = '3.0.0',
  onUndo,
  onRedo,
  onNormalize,
  canUndo,
  canRedo,
}) => (
  <header className="header">
    <span className="header-title">LV-00</span>
    <span className="header-version">{version}</span>
    <div className="module-tabs" role="tablist">
      {TAB_MODULES.map((tab) => (
        <button
          key={tab.key}
          className={`module-tab ${activeModule === tab.key ? 'active' : ''}`}
          onClick={() => onModuleChange(tab.key)}
          role="tab"
          aria-selected={activeModule === tab.key}
          style={
            activeModule === tab.key
              ? { borderBottomColor: tab.color, color: 'var(--color-text-primary)' }
              : {}
          }
        >
          <span className="tab-icon">{tab.icon}</span>
          {tab.label}
        </button>
      ))}
    </div>
    <div className="header-actions">
      <button className="header-action-btn" onClick={onUndo} disabled={!canUndo} title="Undo">
        {ICONS.UNDO}
      </button>
      <button className="header-action-btn" onClick={onRedo} disabled={!canRedo} title="Redo">
        {ICONS.REDO}
      </button>
      <button className="header-action-btn" onClick={onNormalize} title="Normalize">
        {ICONS.NORMALIZE}
      </button>
    </div>
  </header>
);

export default Header;
