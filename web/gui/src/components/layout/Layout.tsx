/**
 * @module components/layout/Layout
 * @description 主应用程序布局组件。
 *              组合 Header、SidebarLeft、Canvas、SidebarRight 和 StatusBar，
 *              并在侧边栏与画布之间提供可拖拽的调整大小手柄。
 *
 *              导出逻辑已提取至 hooks/useExport 中。
 *              导入逻辑已提取至 utils/importExport 中。
 *              拖拽调整大小逻辑已提取至 hooks/useResize 中。
 *
 *              Main application layout component.
 *              Composes Header, SidebarLeft, Canvas, SidebarRight, and StatusBar,
 *              with draggable resize handles between sidebars and canvas.
 */

import React, { useCallback, useEffect, useState } from 'react';
import Header from './Header';
import SidebarLeft from './SidebarLeft';
import SidebarRight from './SidebarRight';
import StatusBar from './StatusBar';
import GeometryCanvas from '@/components/canvas/GeometryCanvas';
import ShortcutHelp from '@/components/common/ShortcutHelp';
import { useKeyboard } from '@/hooks/useKeyboard';
import { useResize } from '@/hooks/useResize';
import { useExport } from '@/hooks/useExport';
import { useEngineStream } from '@/hooks/useEngineStream';
import { logger } from '@/services/logger';
import { importJsonFile } from '@/utils/importExport';
import { useAppStore } from '@/stores';

/**
 * Layout - 应用程序主外壳
 *
 * 三栏布局，包含：
 * - Header（顶部）：应用标志、模块标签页、操作按钮
 * - SidebarLeft | ResizeHandle | Canvas | ResizeHandle | SidebarRight（中部）
 * - StatusBar（底部）：工具信息、坐标、FPS
 *
 * 关键设计决策：
 * - 拖拽调整大小通过 useResize hook 管理（复用已有 hook，避免重复逻辑）
 * - JSON 导入通过 importJsonFile 工具函数处理（关注点分离）
 * - JSON 导出通过 useExport hook 管理（包含 DOM 清理逻辑）
 * - 全局键盘快捷键通过 useKeyboard hook 管理
 * - 快捷键帮助面板（? 或 Ctrl+/）在组件内管理（涉及 UI 状态）
 */
const Layout: React.FC = () => {
  // ---- 全局 Hook 初始化 ----

  /** 注册全局键盘快捷键（V/P/L/C/H/R/? 等工具切换和操作快捷键） */
  useKeyboard();

  /** 快捷键帮助面板显示状态 */
  const [showShortcutHelp, setShowShortcutHelp] = useState(false);

  // 初始化引擎流式事件管理器（应用级单例）
  // 当后端 WebSocket 服务可用时，引擎事件会自动同步到 aiStore
  useEngineStream({
    engineUrl: 'ws://localhost:3456',
    autoConnect: false, // 不自动连接，等待用户手动触发或后端就绪
    // 引擎错误时向用户展示 Toast 通知
    onError: (error: Error) => {
      const addToast = useAppStore.getState().addToast;
      addToast('error', `引擎连接错误: ${error.message}`);
      useAppStore.getState().appendLog(`引擎连接错误: ${error.message}`, 'error');
    },
  });

  // 使用提取的导出逻辑 Hook（导出相关状态和 DOM 清理由 hook 内部管理）
  const { exportGeometry, isExporting } = useExport();

  // ---- 侧边栏拖拽调整大小（复用 useResize hook） ----
  const leftResize = useResize('left');
  const rightResize = useResize('right');

  // ---- 隐藏的文件选择 input 引用（用于触发 JSON 导入） ----
  const fileInputRef = React.useRef<HTMLInputElement>(null);

  /**
   * 导出回调：委托给 useExport hook。
   * 当导出正在进行中时忽略重复点击。
   */
  const handleExport = useCallback(() => {
    if (isExporting) return;
    exportGeometry();
  }, [exportGeometry, isExporting]);

  /**
   * 导入回调：触发隐藏的 file input 元素，弹出文件选择对话框。
   */
  const handleImport = useCallback(() => {
    fileInputRef.current?.click();
  }, []);

  /**
   * 处理文件选择变更事件。
   * 将文件读取和数据加载委托给 importJsonFile 工具函数。
   * 重置 input 值以允许重复选择同一文件。
   */
  const handleFileChange = useCallback(
    (e: React.ChangeEvent<HTMLInputElement>) => {
      const file = e.target.files?.[0];
      if (!file) return;

      // 委托给工具函数处理文件读取、JSON 解析、校验和 store 加载
      importJsonFile(file);

      // 重置 input 值，以允许重复选择同一文件
      e.target.value = '';
    },
    [],
  );

  /**
   * 全局键盘快捷键处理 —— 监听 ? 键或 Ctrl+/ 打开快捷键帮助面板。
   * 注意：仅在非输入框焦点状态下触发，避免干扰文本输入。
   */
  useEffect(() => {
    const handleGlobalKeyDown = (e: KeyboardEvent): void => {
      const target = e.target as HTMLElement;
      const isInputFocused =
        target.tagName === 'INPUT' ||
        target.tagName === 'TEXTAREA' ||
        target.tagName === 'SELECT' ||
        target.isContentEditable;

      // 当焦点在输入框/文本域中时不触发快捷键面板
      if (isInputFocused) return;

      // ? 键或 Ctrl+/(Cmd+/) 打开快捷键帮助
      if (e.key === '?' || ((e.ctrlKey || e.metaKey) && e.key === '/')) {
        e.preventDefault();
        setShowShortcutHelp((prev) => {
          const next = !prev;
          logger.debug(next ? '快捷键帮助面板已打开' : '快捷键帮助面板已关闭');
          return next;
        });
      }
      // Escape 键关闭帮助面板
      if (e.key === 'Escape' && showShortcutHelp) {
        setShowShortcutHelp(false);
        logger.debug('快捷键帮助面板已通过 Esc 关闭');
      }
    };

    document.addEventListener('keydown', handleGlobalKeyDown);
    return () => document.removeEventListener('keydown', handleGlobalKeyDown);
  }, [showShortcutHelp]);

  return (
    <div className="app-shell">
      {/* 顶部导航栏 */}
      <Header onExport={handleExport} onImport={handleImport} />

      {/* 隐藏的文件选择 input，用于 JSON 导入 */}
      <input
        ref={fileInputRef}
        type="file"
        accept=".json,application/json"
        style={{ display: 'none' }}
        onChange={handleFileChange}
        aria-hidden="true"
        tabIndex={-1}
      />

      {/* 快捷键帮助面板 —— 按 ? 或 Ctrl+/ 打开/关闭 */}
      <ShortcutHelp
        isVisible={showShortcutHelp}
        onClose={() => setShowShortcutHelp(false)}
      />

      {/* 主内容区域：左侧边栏 + 画布 + 右侧边栏 */}
      <div className="main-container">
        {/* 左侧边栏：根据当前模块渲染对应面板 */}
        <SidebarLeft />

        {/* 左侧边栏拖拽调整大小手柄 */}
        <div
          className={`resize-handle ${leftResize.isDragging ? 'dragging' : ''}`}
          onMouseDown={leftResize.onMouseDown}
          role="separator"
          aria-orientation="vertical"
          aria-label="调整左侧边栏宽度"
        />

        {/* 中央画布区域 */}
        <div className="canvas-area" id="canvasArea">
          <GeometryCanvas />
        </div>

        {/* 右侧边栏拖拽调整大小手柄 */}
        <div
          className={`resize-handle ${rightResize.isDragging ? 'dragging' : ''}`}
          onMouseDown={rightResize.onMouseDown}
          role="separator"
          aria-orientation="vertical"
          aria-label="调整右侧边栏宽度"
        />

        {/* 右侧边栏：属性面板、依赖面板、流式输出面板 */}
        <SidebarRight />
      </div>

      {/* 底部状态栏 */}
      <StatusBar />
    </div>
  );
};

export default Layout;
