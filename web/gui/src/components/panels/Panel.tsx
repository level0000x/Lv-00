/**
 * @module components/panels/Panel
 * @description 通用可折叠面板组件。
 *              为所有侧边栏面板提供一致的容器，包含标题栏、
 *              折叠切换按钮和内容区域。
 *              支持受控/非受控混合模式，自动同步折叠状态到全局 UI Store。
 */

import React, { useState, useCallback } from 'react';
import { useUIStore } from '@/stores';

/**
 * Panel 组件的 props 接口 / Panel component props interface
 * @property title - 面板标题文本（双语，如 "PROPERTIES / 属性"）/ Panel title text (bilingual, e.g. "PROPERTIES / 属性")
 * @property panelId - 面板唯一标识符（用于折叠状态同步到全局 Store）/ Unique panel identifier (used for collapse state sync to global Store)
 * @property icon - 标题前显示的可选图标字符 / Optional icon character displayed before the title
 * @property defaultCollapsed - 面板初始是否折叠（默认 false）/ Whether panel is initially collapsed (default: false)
 * @property collapsed - 外部受控的折叠状态（提供后优先于内部 state）/ Externally controlled collapsed state (overrides internal state when provided)
 * @property onToggle - 折叠状态变化时的回调（外部受控模式），参数为新的折叠状态 / Callback when collapsed state changes (external controlled mode), receives new collapsed state
 * @property children - 面板内容 / Panel content
 */
export interface PanelProps {
  title: string;
  panelId: string;
  icon?: string;
  defaultCollapsed?: boolean;
  /** 外部受控的折叠状态。提供后组件进入受控模式，内部 state 作为 fallback / Externally controlled collapsed state. When provided, component enters controlled mode, internal state as fallback */
  collapsed?: boolean;
  /** 折叠状态切换回调。当外部提供时，组件在受控模式下调用此函数 / Collapse toggle callback. When provided externally, component calls this in controlled mode */
  onToggle?: (collapsed: boolean) => void;
  children: React.ReactNode;
}

/**
 * Panel - 通用可折叠侧边栏面板
 *
 * 特性：
 * - 点击标题栏切换折叠/展开状态
 * - 折叠箭头指示器
 * - 平滑过渡动画
 * - 键盘可访问（Enter/Space 切换）
 * - 受控/非受控混合模式：若外部提供了 `collapsed` 和 `onToggle`，则以受控模式运行
 * - 自动通过 panelId 将折叠状态同步到全局 useUIStore
 * - 全局 Store 中的 panelStates 优先于组件内部 state
 * - 与应用程序其余部分保持一致样式
 */
const Panel: React.FC<PanelProps> = ({
  title,
  panelId,
  icon,
  defaultCollapsed = false,
  collapsed: controlledCollapsed,
  onToggle,
  children,
}) => {
  // 内部折叠状态（非受控模式的 fallback）
  const [internalCollapsed, setInternalCollapsed] = useState(defaultCollapsed);

  // 从全局 Store 读取面板折叠状态（通过 panelId 作为 key）
  const globalCollapsed = useUIStore((s) => s.panelStates[panelId]);

  /**
   * 计算最终折叠状态。
   * 优先级：全局 Store > 外部受控 prop > 内部 state
   * - 全局 Store 中存在 panelStates[panelId] 时，以全局状态为准
   * - 否则若外部提供了 controlledCollapsed，以受控值为准
   * - 否则以内部 state 为准（完全非受控模式）
   */
  const resolvedCollapsed =
    globalCollapsed !== undefined
      ? globalCollapsed
      : controlledCollapsed !== undefined
        ? controlledCollapsed
        : internalCollapsed;

  /**
   * 判断当前是否处于外部受控模式。
   * 若外部提供了 `collapsed` prop 并且全局 Store 中没有该面板的状态，
   * 则以受控模式运行（不更新内部 state，而是调用 onToggle 回调）。
   */
  const isControlledExternally =
    globalCollapsed === undefined && controlledCollapsed !== undefined;

  /**
   * 处理折叠/展开切换。
   * 1. 同步更新全局 Store 中的 panelStates
   * 2. 若处于外部受控模式：调用 onToggle 回调
   * 3. 否则：更新内部 state
   */
  const handleToggle = useCallback(() => {
    const newCollapsed = !resolvedCollapsed;

    // 同步到全局 Store（通过 panelId 作为 key）
    useUIStore.getState().togglePanel(panelId);

    if (isControlledExternally) {
      // 受控模式：通知外部
      onToggle?.(newCollapsed);
    } else {
      // 非受控模式：更新内部 state
      setInternalCollapsed(newCollapsed);
      // 即使非受控，也通知外部（若有监听器）
      onToggle?.(newCollapsed);
    }
  }, [resolvedCollapsed, panelId, isControlledExternally, onToggle]);

  return (
    <div className="panel">
      <div
        className="panel-title"
        data-collapse={panelId}
        onClick={handleToggle}
        role="button"
        tabIndex={0}
        aria-expanded={!resolvedCollapsed}
        onKeyDown={(e) => {
          if (e.key === 'Enter' || e.key === ' ') {
            e.preventDefault();
            handleToggle();
          }
        }}
      >
        <span className={`collapse-arrow ${resolvedCollapsed ? 'collapsed' : ''}`}>
          &#9660;
        </span>
        {icon && <span>{icon}</span>}
        {title}
      </div>
      <div className={`panel-body ${resolvedCollapsed ? 'collapsed' : ''}`} id={panelId}>
        {children}
      </div>
    </div>
  );
};

export default Panel;
