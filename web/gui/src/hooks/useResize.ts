/**
 * @module hooks/useResize
 * @description 面板调整大小 Hook，用于可拖拽的侧边栏宽度调整。
 *              管理鼠标拖拽事件以调整左侧和右侧侧边栏的大小。
 */

import { useCallback, useEffect, useRef } from 'react';
import { useAppStore } from '@/stores';
import type { ResizeState } from '@/types';

/**
 * useResize - 侧边栏调整大小管理 Hook
 *
 * @param sidebar - 要管理的侧边栏（'left' 或 'right'）
 * @returns 包含 onMouseDown 处理函数和拖拽状态的对象
 */
export function useResize(
  sidebar: 'left' | 'right',
): {
  onMouseDown: (e: React.MouseEvent) => void;
  isDragging: boolean;
} {
  const resizeState = useAppStore((s) => s.resizeState);
  const setResizeState = useAppStore((s) => s.setResizeState);
  const setLeftSidebarWidth = useAppStore((s) => s.setLeftSidebarWidth);
  const setRightSidebarWidth = useAppStore((s) => s.setRightSidebarWidth);

  const startWidthRef = useRef(0);
  const startXRef = useRef(0);

  const handleMouseDown = useCallback(
    (e: React.MouseEvent) => {
      e.preventDefault();
      const state = useAppStore.getState();
      startXRef.current = e.clientX;
      startWidthRef.current =
        sidebar === 'left' ? state.leftSidebarWidth : state.rightSidebarWidth;

      const newState: ResizeState = {
        sidebar,
        startX: e.clientX,
        startWidth: startWidthRef.current,
      };
      setResizeState(newState);
    },
    [sidebar, setResizeState],
  );

  useEffect(() => {
    if (!resizeState || resizeState.sidebar !== sidebar) return;

    const handleMouseMove = (e: MouseEvent): void => {
      const dx = e.clientX - resizeState.startX;
      if (sidebar === 'left') {
        setLeftSidebarWidth(resizeState.startWidth + dx);
      } else {
        setRightSidebarWidth(resizeState.startWidth - dx);
      }
    };

    const handleMouseUp = (): void => {
      setResizeState(null);
    };

    document.addEventListener('mousemove', handleMouseMove);
    document.addEventListener('mouseup', handleMouseUp);

    return () => {
      document.removeEventListener('mousemove', handleMouseMove);
      document.removeEventListener('mouseup', handleMouseUp);
    };
  }, [resizeState, sidebar, setLeftSidebarWidth, setRightSidebarWidth, setResizeState]);

  return {
    onMouseDown: handleMouseDown,
    isDragging: resizeState !== null && resizeState.sidebar === sidebar,
  };
}
