/**
 * @module components/layout/SidebarLeft
 * @description 左侧边栏面板容器。
 *              根据当前激活的模块渲染对应的功能面板。
 *              同一时间只显示一个模块面板，通过 Header 中的模块标签页切换。
 *
 *              Left sidebar panel container.
 *              Renders the appropriate module panel based on the active module.
 *              Only one module panel is visible at a time, switched via module tabs in Header.
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
 * 模块类型到面板组件的映射表。
 * 键为 ModuleType 联合类型的成员，值为对应的 React 组件。
 * 新增模块时需要在此处注册对应的面板组件。
 */
const MODULE_PANELS: Record<ModuleType, React.ComponentType> = {
  formula: FormulaPanel,   // 公式模块面板
  graph: GraphPanel,       // 图模块面板
  block: BlockPanel,       // 函数块模块面板
  proof: ProofPanel,       // 证明模块面板
  type: TypePanel,         // 类型模块面板
  recurse: RecursePanel,   // 递归模块面板
  engine: EnginePanel,     // 引擎模块面板
  debug: DebugPanel,       // 调试模块面板
  help: HelpPanel,         // 帮助面板
  assistant: AssistantPanel, // AI 助手面板
};

/**
 * SidebarLeft - 左侧边栏容器
 *
 * 工作原理：
 * 1. 从 AppStore 读取当前激活的模块 ID（activeModule）
 * 2. 通过 MODULE_PANELS 映射表查找对应的面板组件
 * 3. 渲染该面板组件（如果映射不存在则返回 null）
 *
 * 性能优化：
 * - 使用 React.memo 包裹，仅在 activeModule 变化时重新渲染
 * - Store 订阅使用精确切片选择器，避免不必要的重渲染
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
