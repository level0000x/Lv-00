/**
 * @module hooks/useKeyboard
 * @description Keyboard shortcut management hook.
 *              Provides global keyboard event handling for tool switching,
 *              zoom, undo/redo, and other shortcuts.
 *
 *              键盘快捷键管理 Hook。
 *              提供全局键盘事件处理，包括工具切换、缩放、撤销/重做等快捷键。
 */

import { useEffect, useCallback } from 'react';
import { useAppStore } from '@/stores';
import { ZOOM_STEP } from '@/utils/constants';

/**
 * useKeyboard - Global keyboard shortcuts hook
 *               全局键盘快捷键 Hook
 *
 * Registers keyboard event listeners on mount and cleans up on unmount.
 * Skips events when the user is typing in input fields.
 *
 * 在组件挂载时注册键盘事件监听器，卸载时自动清理。
 * 当用户在输入框中输入时跳过事件处理。
 */
export function useKeyboard(): void {
  const setTool = useAppStore((s) => s.setTool);
  const scale = useAppStore((s) => s.scale);
  const setScale = useAppStore((s) => s.setScale);
  const resetView = useAppStore((s) => s.resetView);
  const undo = useAppStore((s) => s.undo);
  const redo = useAppStore((s) => s.redo);
  const searchVisible = useAppStore((s) => s.searchVisible);
  const setSearchVisible = useAppStore((s) => s.setSearchVisible);
  const hideContextMenu = useAppStore((s) => s.hideContextMenu);
  const clearRegionPoints = useAppStore((s) => s.clearRegionPoints);
  const togglePanel = useAppStore((s) => s.togglePanel);

  const handleKeyDown = useCallback(
    (e: KeyboardEvent) => {
      // Skip if typing in an input field
      // 如果正在输入框中输入，则跳过
      const tag = (e.target as HTMLElement)?.tagName;
      if (tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT') return;

      const ctrl = e.ctrlKey || e.metaKey;
      const key = e.key.toLowerCase();

      switch (key) {
        case 'v':
          // V 键：切换到选择工具 / Switch to select tool
          if (!ctrl) setTool('select');
          break;
        case 'p':
          // P 键：切换到画点工具 / Switch to point tool
          if (!ctrl) setTool('point');
          break;
        case 'l':
          // L 键：切换到画线段工具 / Switch to segment tool
          if (!ctrl) setTool('segment');
          break;
        case 'c':
          // C 键：切换到圆规工具 / Switch to compass tool
          if (!ctrl) setTool('compass');
          break;
        case 'h':
          // H 键：切换到平移工具 / Switch to pan tool
          if (!ctrl) setTool('pan');
          break;
        case 'r':
          // R 键：切换到区域工具 / Switch to region tool
          if (!ctrl) setTool('region');
          break;
        case '?':
          // ? 键：切换到探针工具 / Switch to probe tool
          if (!ctrl) setTool('probe');
          break;
        case '+':
        case '=':
          // +/= 键：放大画布 / Zoom in
          if (!ctrl) setScale(scale * ZOOM_STEP);
          break;
        case '-':
        case '_':
          // -/_ 键：缩小画布 / Zoom out
          if (!ctrl) setScale(scale / ZOOM_STEP);
          break;
        case '0':
          // Ctrl+0 键：重置视图到原点 / Reset view to origin
          if (ctrl) {
            e.preventDefault();
            resetView();
          }
          break;
        case 'z':
          // Ctrl+Z 键：撤销操作 / Undo
          if (ctrl && !e.shiftKey) {
            e.preventDefault();
            undo();
          }
          break;
        case 'y':
          // Ctrl+Y 键：重做操作 / Redo
          if (ctrl) {
            e.preventDefault();
            redo();
          }
          break;
        case 'f':
          // Ctrl+F 键：切换搜索面板 / Toggle search panel
          if (ctrl) {
            e.preventDefault();
            setSearchVisible(!searchVisible);
          }
          break;
        case '/':
          // / 键：切换帮助面板 / Toggle help panel
          if (!ctrl) {
            e.preventDefault();
            togglePanel('help');
          }
          break;
        case 'escape':
          // Escape 键：重置为选择工具、关闭右键菜单、清除区域顶点
          // Reset to select tool, hide context menu, clear region points
          setTool('select');
          hideContextMenu();
          clearRegionPoints();
          break;
      }
    },
    [
      setTool, scale, setScale, resetView, undo, redo,
      searchVisible, setSearchVisible, hideContextMenu, clearRegionPoints,
      togglePanel,
    ],
  );

  useEffect(() => {
    document.addEventListener('keydown', handleKeyDown);
    return () => document.removeEventListener('keydown', handleKeyDown);
  }, [handleKeyDown]);
}
