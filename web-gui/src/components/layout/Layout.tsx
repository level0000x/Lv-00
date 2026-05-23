/**
 * @module components/layout/Layout
 * @description 主应用程序布局组件。
 *              组合 Header、SidebarLeft、Canvas、SidebarRight 和 StatusBar，
 *              并在侧边栏与画布之间提供可拖拽的调整大小手柄。
 *
 *              导出逻辑已提取至 hooks/useExport 中，以实现更好的
 *              关注点分离和可测试性。导入逻辑保留在组件内（涉及 UI 交互）。
 */

import React, { useCallback, useRef, useEffect } from 'react';
import Header from './Header';
import SidebarLeft from './SidebarLeft';
import SidebarRight from './SidebarRight';
import StatusBar from './StatusBar';
import GeometryCanvas from '@/components/canvas/GeometryCanvas';
import { useKeyboard } from '@/hooks/useKeyboard';
import { useExport } from '@/hooks/useExport';
import { useAppStore } from '@/stores';

/**
 * Layout - 应用程序主外壳
 *
 * 三栏布局，包含：
 * - Header（顶部）
 * - SidebarLeft | ResizeHandle | Canvas | ResizeHandle | SidebarRight（中部）
 * - StatusBar（底部）
 *
 * 拖拽调整大小手柄允许用户拖动以调整侧边栏宽度。
 * 全局键盘快捷键通过 useKeyboard hook 管理。
 */
const Layout: React.FC = () => {
  // 注册全局键盘快捷键
  useKeyboard();

  // 使用提取的导出逻辑 Hook（导出相关状态和DOM清理由hook内部管理）
  const { exportGeometry, isExporting } = useExport();

  const leftSidebarWidth = useAppStore((s) => s.leftSidebarWidth);
  const rightSidebarWidth = useAppStore((s) => s.rightSidebarWidth);
  const setLeftSidebarWidth = useAppStore((s) => s.setLeftSidebarWidth);
  const setRightSidebarWidth = useAppStore((s) => s.setRightSidebarWidth);
  const resizeState = useAppStore((s) => s.resizeState);
  const setResizeState = useAppStore((s) => s.setResizeState);

  const containerRef = useRef<HTMLDivElement>(null);
  /** 隐藏的文件选择 input 引用，用于触发 JSON 导入 */
  const fileInputRef = useRef<HTMLInputElement>(null);

  /** 左侧边栏拖拽调整大小开始 / Left sidebar resize start handler */
  const handleLeftResizeStart = useCallback(
    (e: React.MouseEvent) => {
      e.preventDefault();
      setResizeState({ sidebar: 'left', startX: e.clientX, startWidth: leftSidebarWidth });
    },
    [leftSidebarWidth, setResizeState],
  );

  /** 右侧边栏拖拽调整大小开始 / Right sidebar resize start handler */
  const handleRightResizeStart = useCallback(
    (e: React.MouseEvent) => {
      e.preventDefault();
      setResizeState({ sidebar: 'right', startX: e.clientX, startWidth: rightSidebarWidth });
    },
    [rightSidebarWidth, setResizeState],
  );

  /**
   * 处理拖拽调整大小过程中的鼠标移动事件
   */
  useEffect(() => {
    if (!resizeState) return;

    const handleMouseMove = (e: MouseEvent): void => {
      const dx = e.clientX - resizeState.startX;

      if (resizeState.sidebar === 'left') {
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
  }, [resizeState, setLeftSidebarWidth, setRightSidebarWidth, setResizeState]);

  /** 导出回调：委托给 useExport hook */
  const handleExport = useCallback(() => {
    if (isExporting) return;
    exportGeometry();
  }, [exportGeometry, isExporting]);

  /**
   * 导入 JSON 文件，将几何数据加载到当前会话中。
   * 点击按钮触发隐藏的 file input 元素，弹出文件选择对话框。
   */
  const handleImport = useCallback(() => {
    fileInputRef.current?.click();
  }, []);

  /**
   * 处理文件选择变更事件。
   * 读取用户选择的 JSON 文件，校验格式并加载数据。
   *
   * 数据校验规则：
   * - 根对象必须存在且为 object
   * - 必须包含 points 字段，且为非空数组
   * - 每个 point 元素必须包含 id、x、y 三个字段
   * - 对于 segments、constraints、regions 字段，仅在数据合法时加载
   */
  const handleFileChange = useCallback(
    (e: React.ChangeEvent<HTMLInputElement>) => {
      const file = e.target.files?.[0];
      if (!file) return;

      const reader = new FileReader();

      reader.onload = (event) => {
        try {
          const text = event.target?.result as string;
          const data = JSON.parse(text);

          // 校验根对象
          if (!data || typeof data !== 'object') {
            throw new Error('JSON 文件内容无效：根元素必须是对象');
          }

          // 核心校验：points 字段必须存在且为非空数组
          if (!Array.isArray(data.points) || data.points.length === 0) {
            throw new Error('JSON 文件格式无效：缺少 points 数组或数组为空');
          }

          // 校验 points 数组中每个元素的格式
          for (let i = 0; i < data.points.length; i++) {
            const p = data.points[i];
            if (
              typeof p.id !== 'number' ||
              typeof p.x !== 'number' ||
              typeof p.y !== 'number'
            ) {
              throw new Error(
                `points[${i}] 格式无效：缺少 id、x 或 y 字段，或字段类型不正确`,
              );
            }
          }

          const store = useAppStore.getState();

          // 导入 points
          store.setPoints(data.points);

          // 导入 segments（可选）
          store.setSegments(
            Array.isArray(data.segments) ? data.segments : [],
          );

          // 导入 constraints（可选）
          store.setConstraints(
            Array.isArray(data.constraints) ? data.constraints : [],
          );

          // 导入 regions（可选，需要根据 point ID 重建 Region 对象）
          if (Array.isArray(data.regions)) {
            const importedPoints = data.points;
            const importedRegions = data.regions.map(
              (r: { id: number; points: number[] }) => ({
                id: r.id,
                points: (r.points || []).map((pid: number) => {
                  const found = importedPoints.find(
                    (p: { id: number }) => p.id === pid,
                  );
                  return found || { id: pid, x: 0, y: 0 };
                }),
              }),
            );
            store.setRegions(importedRegions);
          } else {
            store.setRegions([]);
          }

          // 导入 viewport 状态（可选）
          if (data.viewport && typeof data.viewport === 'object') {
            const viewport = data.viewport;
            store.setScale(typeof viewport.scale === 'number' ? viewport.scale : 1);
            store.setOffset(
              typeof viewport.offsetX === 'number' ? viewport.offsetX : 0,
              typeof viewport.offsetY === 'number' ? viewport.offsetY : 0,
            );
          }

          // 生成摘要信息
          const summary = [
            `点: ${data.points.length}`,
            data.segments?.length ? `线段: ${data.segments.length}` : null,
            data.constraints?.length ? `约束: ${data.constraints.length}` : null,
            data.regions?.length ? `区域: ${data.regions.length}` : null,
          ]
            .filter(Boolean)
            .join(', ');

          store.addToast('success', `导入成功: ${summary}`);
          store.appendLog(`导入成功: ${file.name} (${summary})`, 'info');
        } catch (err) {
          const errorMsg = `导入失败: ${(err as Error).message}`;
          useAppStore.getState().addToast('error', errorMsg);
          useAppStore.getState().appendLog(errorMsg, 'error');
        }
      };

      reader.onerror = () => {
        const errorMsg = `导入失败: 无法读取文件 "${file.name}"`;
        useAppStore.getState().addToast('error', errorMsg);
        useAppStore.getState().appendLog(errorMsg, 'error');
      };

      reader.readAsText(file);

      // 重置 input 值，以允许重复选择同一文件
      e.target.value = '';
    },
    [],
  );

  return (
    <div className="app-shell" ref={containerRef}>
      <Header onExport={handleExport} onImport={handleImport} />
      {/* 隐藏的文件选择 input，用于 JSON 导入 */}
      <input
        ref={fileInputRef}
        type="file"
        accept=".json,application/json"
        style={{ display: 'none' }}
        onChange={handleFileChange}
        aria-hidden="true"
      />

      <div className="main-container">
        <SidebarLeft />

        <div
          className={`resize-handle ${resizeState?.sidebar === 'left' ? 'dragging' : ''}`}
          onMouseDown={handleLeftResizeStart}
        />

        <div className="canvas-area" id="canvasArea">
          <GeometryCanvas />
        </div>

        <div
          className={`resize-handle ${resizeState?.sidebar === 'right' ? 'dragging' : ''}`}
          onMouseDown={handleRightResizeStart}
        />

        <SidebarRight />
      </div>

      <StatusBar />
    </div>
  );
};

export default Layout;
