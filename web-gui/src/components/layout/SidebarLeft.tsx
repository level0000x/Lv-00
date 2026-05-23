/**
 * @module components/layout/SidebarLeft
 * @description 左侧边栏面板容器 / Left sidebar panel container.
 *              根据当前激活的模块渲染对应的功能面板。
 *              Renders the appropriate module panel based on the active module.
 */

import React from 'react';
import { useAppStore } from '@/stores';
import FormulaPanel from '@/components/panels/FormulaPanel';
import GraphPanel from '@/components/panels/GraphPanel';
import BlockPanel from '@/components/panels/BlockPanel';
import ProofPanel from '@/components/panels/ProofPanel';
import TypePanel from '@/components/panels/TypePanel';
import RecursePanel from '@/components/panels/RecursePanel';
import EnginePanel from '@/components/panels/EnginePanel';
import DebugPanel from '@/components/panels/DebugPanel';
import HelpPanel from '@/components/panels/HelpPanel';
import AssistantPanel from '@/components/panels/AssistantPanel';
import type { ModuleType } from '@/types';

/**
 * Map module types to their panel components
 */
const MODULE_PANELS: Record<ModuleType, React.ComponentType> = {
  formula: FormulaPanel,
  graph: GraphPanel,
  block: BlockPanel,
  proof: ProofPanel,
  type: TypePanel,
  recurse: RecursePanel,
  engine: EnginePanel,
  debug: DebugPanel,
  help: HelpPanel,
  assistant: AssistantPanel,
};

/**
 * SidebarLeft - 左侧边栏容器 / Left sidebar container
 *
 * 渲染当前激活模块对应的面板组件。同一时间只显示一个模块面板。
 * Renders the panel corresponding to the currently active module.
 * Only one module panel is visible at a time.
 */
const SidebarLeft: React.FC = () => {
  const activeModule = useAppStore((s) => s.activeModule);

  const PanelComponent = MODULE_PANELS[activeModule];
  if (!PanelComponent) return null;

  return (
    <div className="sidebar-left" id="sidebarLeft">
      <div className="module-panel">
        <PanelComponent />
      </div>
    </div>
  );
};

export default React.memo(SidebarLeft);
