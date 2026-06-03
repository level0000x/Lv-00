/**
 * @module components/common/ShortcutHelp
 * @description 键盘快捷键帮助面板组件 / Keyboard shortcut help panel component.
 *              按 ? 键或 Ctrl+/ 触发打开/关闭，使用 React Portal 渲染到 body。
 *              以分类卡片的形式显示所有可用的键盘快捷键。
 *              Toggle with ? key or Ctrl+/, rendered to body via React Portal.
 *              Displays all available keyboard shortcuts in categorized cards.
 *
 * 样式说明 / Style Notes:
 *   所有样式已从内联 style 对象迁移至 CSS 类（shortcut-help.css），
 *   通过 className 属性引用。视觉表现与迁移前完全一致。
 */

import React, { useCallback, useEffect } from 'react';
import { createPortal } from 'react-dom';

// ============================================================
// 类型定义 / Type Definitions
// ============================================================

/**
 * ShortcutHelp 组件属性
 * @property isVisible - 面板是否可见（由父组件 Layout.tsx 控制）
 * @property onClose - 关闭面板的回调函数
 */
interface ShortcutHelpProps {
  /** 面板是否可见（由父组件控制） */
  isVisible: boolean;
  /** 关闭面板的回调 */
  onClose: () => void;
}

/**
 * 单个快捷键条目 / Single shortcut entry
 */
interface ShortcutItem {
  key: string;
  description: string;
}

/**
 * 快捷键分类 / Shortcut category
 */
interface ShortcutCategory {
  title: string;
  items: ShortcutItem[];
}

// ============================================================
// 快捷键数据 / Shortcut Data
// ============================================================

/** 所有快捷键分类数据 / All shortcut category data */
const SHORTCUT_CATEGORIES: ShortcutCategory[] = [
  {
    /** 工具类 / Tools */
    title: '工具 Tool',
    items: [
      { key: 'V', description: '选择 / Select' },
      { key: 'P', description: '点 / Point' },
      { key: 'L', description: '线段 / Line Segment' },
      { key: 'C', description: '圆规 / Compass' },
      { key: 'H', description: '平移 / Pan' },
      { key: 'R', description: '区域 / Region' },
      { key: 'I', description: '探测 / Probe' },
      { key: 'Esc', description: '取消 / Cancel' },
    ],
  },
  {
    /** 视图类 / View */
    title: '视图 View',
    items: [
      { key: '+/-', description: '缩放 / Zoom' },
      { key: 'Ctrl+0', description: '重置视图 / Reset View' },
      { key: 'Ctrl+1', description: '适应全部 / Fit All' },
      { key: '鼠标滚轮', description: '缩放 / Zoom' },
      { key: '中键拖拽', description: '平移 / Pan' },
    ],
  },
  {
    /** 编辑类 / Edit */
    title: '编辑 Edit',
    items: [
      { key: 'Ctrl+Z', description: '撤销 / Undo' },
      { key: 'Ctrl+Y', description: '重做 / Redo' },
      { key: 'Ctrl+S', description: '导出 JSON / Export JSON' },
      { key: 'Ctrl+O', description: '导入 JSON / Import JSON' },
      { key: 'Delete', description: '删除选中 / Delete Selected' },
    ],
  },
  {
    /** 面板类 / Panel */
    title: '面板 Panel',
    items: [
      { key: 'Ctrl+B', description: '切换块面板 / Toggle Block Panel' },
      { key: 'Ctrl+Shift+F', description: '搜索 / Search' },
      { key: 'Ctrl+K', description: '切换主题 / Toggle Theme' },
      { key: '/', description: '聚焦命令面板 / Focus Command Panel' },
    ],
  },
  {
    /** 画布类 / Canvas */
    title: '画布 Canvas',
    items: [
      { key: 'Shift+点击', description: '多选 / Multi-select' },
      { key: '拖拽', description: '框选 / Box Select' },
      { key: '双击', description: '聚焦点 / Focus Point' },
    ],
  },
];

// ============================================================
// ShortcutHelp 组件 / ShortcutHelp Component
// ============================================================

/**
 * ShortcutHelp - 键盘快捷键帮助面板 / Keyboard Shortcut Help Panel
 *
 * 功能特性 / Features:
 * - 由父组件 Layout.tsx 控制 ? 键和 Ctrl+/ 的显示/隐藏
 * - 按 ESC 键关闭（通过父组件的键盘监听）
 * - 点击遮罩层关闭 / Click overlay to close
 * - 使用 React Portal 渲染到 document.body / Rendered to document.body via React Portal
 * - 快捷键按分类分卡片展示 / Shortcuts displayed in categorized cards
 * - 使用 CSS 类替代内联样式，便于维护 / Uses CSS classes instead of inline styles
 * - 使用项目 CSS 变量保持视觉一致性 / Uses project CSS variables for visual consistency
 *
 * 注意：键盘监听已统一由 Layout.tsx 管理，本组件不再独立注册 keydown 监听器。
 */
const ShortcutHelp: React.FC<ShortcutHelpProps> = ({ isVisible, onClose }) => {
  // ----------------------------------------------------------
  // 事件处理器 / Event Handlers
  // ----------------------------------------------------------

  /**
   * 点击遮罩层关闭面板（仅当点击目标是遮罩本身）
   * Click overlay to close panel (only when the click target is the overlay itself)
   */
  const handleOverlayClick = useCallback(
    (e: React.MouseEvent<HTMLDivElement>) => {
      if (e.target === e.currentTarget) {
        onClose();
      }
    },
    [onClose],
  );

  // ----------------------------------------------------------
  // 副作用 / Side Effects
  // ----------------------------------------------------------

  /** 面板打开时阻止 body 滚动 / Prevent body scroll when panel is open */
  useEffect(() => {
    if (isVisible) {
      const originalOverflow = document.body.style.overflow;
      document.body.style.overflow = 'hidden';
      return () => {
        document.body.style.overflow = originalOverflow;
      };
    }
  }, [isVisible]);

  // ----------------------------------------------------------
  // 渲染 / Render
  // ----------------------------------------------------------

  if (!isVisible) return null;

  /**
   * 将一个快捷键条目的按键组合字符串渲染为多个 <kbd> 元素
   * 例如 "Ctrl+Shift+F" 会渲染为 [<kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>F</kbd>]
   *
   * Renders a shortcut key combination string into multiple <kbd> elements.
   * E.g., "Ctrl+Shift+F" renders as [<kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>F</kbd>]
   */
  const renderKbd = (keyText: string): React.ReactNode => {
    const parts = keyText.split('+');
    return parts.map((part, idx) => (
      <React.Fragment key={idx}>
        {idx > 0 && <span className="shortcut-help-kbd-sep">+</span>}
        <kbd className="shortcut-help-kbd">{part.trim()}</kbd>
      </React.Fragment>
    ));
  };

  return createPortal(
    <div className="shortcut-help-overlay" onClick={handleOverlayClick} role="dialog" aria-modal="true" aria-label="键盘快捷键帮助">
      <div className="shortcut-help-panel">
        {/* 标题区域 / Title area */}
        <h2 className="shortcut-help-title">键盘快捷键 Keyboard Shortcuts</h2>
        <p className="shortcut-help-subtitle">
          按 <kbd className="shortcut-help-kbd">?</kbd> 或 <kbd className="shortcut-help-kbd">Ctrl+/</kbd> 切换此面板 &middot; 点击遮罩或按{' '}
          <kbd className="shortcut-help-kbd">Esc</kbd> 关闭
        </p>

        {/* 分类网格 / Category grid */}
        <div className="shortcut-help-grid">
          {SHORTCUT_CATEGORIES.map((category) => (
            <div key={category.title} className="shortcut-help-category-card">
              <div className="shortcut-help-category-title">{category.title}</div>
              {category.items.map((item) => (
                <div key={item.description} className="shortcut-help-row">
                  <span className="shortcut-help-keys">
                    {renderKbd(item.key)}
                  </span>
                  <span className="shortcut-help-desc">{item.description}</span>
                </div>
              ))}
            </div>
          ))}
        </div>

        {/* 底部提示 / Footer hint */}
        <div className="shortcut-help-footer">
          <kbd className="shortcut-help-kbd">?</kbd>
          <span>或</span>
          <kbd className="shortcut-help-kbd">Ctrl+/</kbd>
          <span>随时打开此面板 / Press anytime to open this panel</span>
        </div>
      </div>
    </div>,
    document.body,
  );
};

ShortcutHelp.displayName = 'ShortcutHelp';

export default ShortcutHelp;
