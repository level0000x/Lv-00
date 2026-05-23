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
          if (!ctrl) setTool('select');
          break;
        case 'p':
          if (!ctrl) setTool('point');
          break;
        case 'l':
          if (!ctrl) setTool('segment');
          break;
        case 'c':
          if (!ctrl) setTool('compass');
          break;
        case 'h':
          if (!ctrl) setTool('pan');
          break;
        case 'r':
          if (!ctrl) setTool('region');
          break;
        case '?':
          if (!ctrl) setTool('probe');
          break;
        case '+':
        case '=':
          if (!ctrl) setScale(scale * ZOOM_STEP);
          break;
        case '-':
        case '_':
          if (!ctrl) setScale(scale / ZOOM_STEP);
          break;
        case '0':
          if (ctrl) {
            e.preventDefault();
            resetView();
          }
          break;
        case 'z':
          if (ctrl && !e.shiftKey) {
            e.preventDefault();
            undo();
          }
          break;
        case 'y':
          if (ctrl) {
            e.preventDefault();
            redo();
          }
          break;
        case 'f':
          if (ctrl) {
            e.preventDefault();
            setSearchVisible(!searchVisible);
          }
          break;
        case 'escape':
          setTool('select');
          hideContextMenu();
          clearRegionPoints();
          break;
      }
    },
    [
      setTool, scale, setScale, resetView, undo, redo,
      searchVisible, setSearchVisible, hideContextMenu, clearRegionPoints,
    ],
  );

  useEffect(() => {
    document.addEventListener('keydown', handleKeyDown);
    return () => document.removeEventListener('keydown', handleKeyDown);
  }, [handleKeyDown]);
}
