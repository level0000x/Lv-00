/**
 * @module components/common/ContextMenu
 * @description 右键上下文菜单组件 / Right-click context menu component.
 *              从 Zustand store 中读取右键菜单状态并渲染。
 *              支持点、线段、空白空间的不同操作。
 *
 *              本组件仅负责渲染，所有业务逻辑已提取到
 *              utils/contextMenuActions.ts 中。
 *
 *              Reads context menu state from the Zustand store and renders it.
 *              Supports different actions for points, segments, and empty space.
 *
 *              This component is only responsible for rendering.
 *              All business logic has been extracted to utils/contextMenuActions.ts.
 */

import React, { useEffect, useCallback } from 'react';
import { useAppStore } from '@/stores';
import { executeContextMenuAction } from './utils/contextMenuActions';

/**
 * ContextMenu - 右键上下文菜单 / Right-click context menu
 *
 * 在鼠标位置渲染浮动的操作菜单。
 * 点击菜单外或按 Escape 键自动关闭。
 * 根据目标类型（点/线段/空白）执行不同的操作。
 *
 * Renders a floating action menu at the mouse position.
 * Automatically closes when clicking outside or pressing Escape.
 * Executes different actions based on target type (point/segment/empty).
 *
 * 架构说明 / Architecture Notes:
 * - 组件只负责 UI 渲染和事件绑定 / Component only handles UI rendering and event binding
 * - 业务逻辑集中在 utils/contextMenuActions.ts / Business logic centralized in utils/contextMenuActions.ts
 * - 操作分发通过 executeContextMenuAction() 完成 / Action dispatch via executeContextMenuAction()
 */
const ContextMenu: React.FC = () => {
  const contextMenu = useAppStore((s) => s.contextMenu);
  const hideContextMenu = useAppStore((s) => s.hideContextMenu);
  const addToast = useAppStore((s) => s.addToast);

  /**
   * 处理菜单项点击
   * 委托给 executeContextMenuAction 执行业务逻辑，然后关闭菜单。
   *
   * Handle menu item click.
   * Delegates to executeContextMenuAction for business logic, then closes the menu.
   */
  const handleAction = useCallback(
    (actionId: string) => {
      if (!contextMenu) return;

      executeContextMenuAction(actionId, {
        contextMenu,
        addToast,
      });

      hideContextMenu();
    },
    [contextMenu, addToast, hideContextMenu],
  );

  // 按 Escape 关闭 / Close on Escape key
  useEffect(() => {
    if (!contextMenu) return;
    const handler = (e: KeyboardEvent) => {
      if (e.key === 'Escape') {
        e.preventDefault();
        hideContextMenu();
      }
    };
    document.addEventListener('keydown', handler);
    return () => document.removeEventListener('keydown', handler);
  }, [contextMenu, hideContextMenu]);

  if (!contextMenu) return null;

  return (
    <div
      className="context-menu"
      id="contextMenu"
      style={{
        left: contextMenu.x,
        top: contextMenu.y,
        display: 'block',
      }}
      role="menu"
      aria-label="右键菜单 / Context menu"
    >
      {contextMenu.items.map((item) => (
        <button
          key={item.id}
          className="context-menu-item"
          data-action={item.id}
          onClick={() => handleAction(item.id)}
          role="menuitem"
        >
          {item.label}
          {/* 快捷键提示标签 / Shortcut hint badge */}
          {item.shortcut && (
            <span className="context-menu-shortcut" aria-hidden="true">
              {item.shortcut}
            </span>
          )}
        </button>
      ))}
    </div>
  );
};

export default ContextMenu;
