import React from 'react';
import { ModuleKey } from '../L5-core/types';

interface SidebarRightProps {
  width: number;
  activeModule?: ModuleKey;
  children?: React.ReactNode;
}

const MODULE_TITLES: Record<string, string> = {
  formula: '公式输入 Formula Input',
  graph: '约束图 Constraint Graph',
  block: '函数块 Function Blocks',
  proof: '证明导航 Proof Navigator',
  type: '类型浏览 Type Explorer',
  recurse: '递归 Recursion Explorer',
  engine: '引擎状态 Engine Status',
  debug: '调试控制台 Debug Console',
  help: '帮助与快捷键 Help & Keys',
};

const SidebarRight: React.FC<SidebarRightProps> = ({ width, activeModule, children }) => {
  const title = activeModule ? MODULE_TITLES[activeModule] ?? '属性 Properties' : '属性 Properties';

  return (
    <aside className="sidebar-right" style={{ width }}>
      <div className="panel-header">
        <span className="panel-header-title">{title}</span>
      </div>
      <div className="panel-content">
        {children ?? (
          <div className="empty-state" style={{ height: 'auto', padding: 24 }}>
            <div className="empty-state-icon" style={{ fontSize: 28 }}>{'\u29BF'}</div>
            <div className="empty-state-title">未选中 No Selection</div>
            <div style={{ fontSize: 'var(--font-size-xs)' }}>选择项目以查看属性 Select an item to view properties</div>
          </div>
        )}
      </div>
    </aside>
  );
};

export default SidebarRight;
