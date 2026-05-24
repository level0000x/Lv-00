/**
 * @module components/common/ShortcutHelp
 * @description 键盘快捷键帮助面板组件 / Keyboard shortcut help panel component.
 *              按 ? 键或 Ctrl+/ 触发打开/关闭，使用 React Portal 渲染到 body。
 *              以分类卡片的形式显示所有可用的键盘快捷键。
 *              Toggle with ? key or Ctrl+/, rendered to body via React Portal.
 *              Displays all available keyboard shortcuts in categorized cards.
 */

import React, { useState, useCallback, useEffect, useRef } from 'react';
import { createPortal } from 'react-dom';

// ============================================================
// 类型定义 / Type Definitions
// ============================================================

/**
 * 单个快捷键条目 / Single shortcut entry
 * @property key - 快捷键按键组合文本 / The key combination text
 * @property description - 快捷键功能描述 / Description of the shortcut's function
 */
interface ShortcutItem {
  key: string;
  description: string;
}

/**
 * 快捷键分类 / Shortcut category
 * @property title - 分类标题 / Category title
 * @property items - 该分类下的快捷键列表 / List of shortcuts in this category
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
// 内联样式 / Inline Styles
// ============================================================

/** 样式对象集合 / Style object collection */
const styles: Record<string, React.CSSProperties> = {
  /** 遮罩层：半透明背景，覆盖全屏 / Overlay: semi-transparent backdrop covering the full viewport */
  overlay: {
    position: 'fixed',
    inset: 0,
    zIndex: 200,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    backgroundColor: 'rgba(0, 0, 0, 0.6)',
    backdropFilter: 'blur(4px)',
    WebkitBackdropFilter: 'blur(4px)',
    animation: 'shortcutHelpFadeIn 0.15s ease-out',
  },

  /** 面板主体：居中显示，带阴影和圆角 / Panel body: centered with shadow and rounded corners */
  panel: {
    backgroundColor: 'var(--color-bg-secondary)',
    border: '1px solid var(--color-border-primary)',
    borderRadius: 'var(--radius-lg)',
    boxShadow: 'var(--shadow-xl)',
    maxWidth: 720,
    width: 'calc(100vw - 48px)',
    maxHeight: 'calc(100vh - 80px)',
    overflowY: 'auto',
    padding: '28px 32px',
    animation: 'shortcutHelpSlideUp 0.2s ease-out',
  },

  /** 标题行：面板顶部的大标题 / Title row: main heading at the top of the panel */
  title: {
    fontSize: 'var(--font-size-2xl)',
    fontWeight: 600,
    color: 'var(--color-text-bright)',
    marginBottom: 6,
    fontFamily: 'var(--font-sans)',
  },

  /** 副标题：提示用户如何关闭面板 / Subtitle: hint for how to close the panel */
  subtitle: {
    fontSize: 'var(--font-size-sm)',
    color: 'var(--color-text-muted)',
    marginBottom: 24,
    fontFamily: 'var(--font-sans)',
  },

  /** 分类网格容器：两列布局 / Category grid container: two-column layout */
  grid: {
    display: 'grid',
    gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))',
    gap: 20,
  },

  /** 单个分类卡片 / Single category card */
  categoryCard: {
    backgroundColor: 'var(--color-bg-tertiary)',
    border: '1px solid var(--color-border-secondary)',
    borderRadius: 'var(--radius-md)',
    padding: '14px 16px',
  },

  /** 分类标题 / Category title */
  categoryTitle: {
    fontSize: 'var(--font-size-md)',
    fontWeight: 600,
    color: 'var(--color-accent)',
    marginBottom: 10,
    paddingBottom: 8,
    borderBottom: `1px solid var(--color-border-secondary)`,
    fontFamily: 'var(--font-sans)',
    textTransform: 'uppercase' as React.CSSProperties['textTransform'],
    letterSpacing: '0.5px',
  },

  /** 快捷键条目行 / Shortcut item row */
  shortcutRow: {
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'space-between',
    padding: '5px 0',
  },

  /** 快捷键按键标签（模拟 kbd） / Keyboard key label (simulating kbd) */
  kbd: {
    display: 'inline-flex',
    alignItems: 'center',
    justifyContent: 'center',
    padding: '2px 7px',
    minWidth: 22,
    fontSize: '10px',
    fontFamily: 'var(--font-mono)',
    fontWeight: 500,
    color: 'var(--color-text-bright)',
    backgroundColor: 'rgba(var(--color-accent-rgb), 0.1)',
    border: `1px solid rgba(var(--color-accent-rgb), 0.25)`,
    borderRadius: 'var(--radius-sm)',
    lineHeight: 1.6,
    whiteSpace: 'nowrap' as React.CSSProperties['whiteSpace'],
  },

  /** 快捷键功能描述文本 / Shortcut description text */
  description: {
    fontSize: 'var(--font-size-sm)',
    color: 'var(--color-text-secondary)',
    fontFamily: 'var(--font-sans)',
    textAlign: 'right' as React.CSSProperties['textAlign'],
    flex: 1,
    marginLeft: 12,
  },

  /** 底部提示行 / Footer hint row */
  footer: {
    marginTop: 24,
    paddingTop: 14,
    borderTop: `1px solid var(--color-border-secondary)`,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    gap: 8,
    fontSize: 'var(--font-size-sm)',
    color: 'var(--color-text-muted)',
    fontFamily: 'var(--font-sans)',
  },
};

// ============================================================
// ShortcutHelp 组件 / ShortcutHelp Component
// ============================================================

/**
 * ShortcutHelp - 键盘快捷键帮助面板 / Keyboard Shortcut Help Panel
 *
 * 功能特性 / Features:
 * - 按 ? 键或 Ctrl+/ 触发打开/关闭 / Toggle with ? or Ctrl+/
 * - 按 ESC 键关闭 / Close with ESC key
 * - 点击遮罩层关闭 / Click overlay to close
 * - 使用 React Portal 渲染到 document.body / Rendered to document.body via React Portal
 * - 快捷键按分类分卡片展示 / Shortcuts displayed in categorized cards
 * - 完全使用内联样式，不依赖外部 CSS 文件 / Fully inline-styled, no external CSS dependency
 * - 使用项目 CSS 变量保持视觉一致性 / Uses project CSS variables for visual consistency
 */
const ShortcutHelp: React.FC = () => {
  /** 面板是否可见 / Whether the panel is visible */
  const [isVisible, setIsVisible] = useState(false);

  /** promptTip：标记是否允许 ? 键触发，
   *  当用户在 input/textarea 等输入控件中时，应阻止 ? 触发快捷键面板
   *  Marks whether ? key trigger is allowed.
   *  When the user is focused in an input/textarea control, ? should not trigger the panel. */
  const allowQuestionMark = useRef(true);

  // ----------------------------------------------------------
  // 事件处理器 / Event Handlers
  // ----------------------------------------------------------

  /**
   * 处理全局键盘事件，检测 ? 和 Ctrl+/ 组合键
   * Handles global keyboard events, detecting ? and Ctrl+/ combos
   */
  const handleKeyDown = useCallback(
    (e: KeyboardEvent) => {
      /** 检测 Ctrl+/ (或 Cmd+/ on Mac) / Detect Ctrl+/ (or Cmd+/ on Mac) */
      const isCtrlSlash = (e.ctrlKey || e.metaKey) && e.key === '/';
      /** 检测 ? 键（且不在表单控件中）/ Detect ? key (and not in form controls) */
      const isQuestionMark = e.key === '?' && allowQuestionMark.current;

      if (isCtrlSlash || isQuestionMark) {
        e.preventDefault();
        e.stopPropagation();
        setIsVisible((prev) => !prev);
        return;
      }

      /** ESC 键关闭面板 / ESC key closes the panel */
      if (e.key === 'Escape' && isVisible) {
        e.preventDefault();
        e.stopPropagation();
        setIsVisible(false);
      }
    },
    [isVisible],
  );

  /**
   * 处理 focusin 事件：当用户聚焦到输入控件时，阻止 ? 键触发面板
   * Handles focusin events: when user focuses an input control, block ? from triggering the panel
   */
  const handleFocusIn = useCallback(() => {
    const tag = (document.activeElement?.tagName || '').toLowerCase();
    const isInput =
      tag === 'input' ||
      tag === 'textarea' ||
      tag === 'select' ||
      document.activeElement?.getAttribute('contenteditable') === 'true';
    allowQuestionMark.current = !isInput;
  }, []);

  /**
   * 点击遮罩层关闭面板（仅当点击目标是遮罩本身）
   * Click overlay to close panel (only when the click target is the overlay itself)
   */
  const handleOverlayClick = useCallback(
    (e: React.MouseEvent<HTMLDivElement>) => {
      if (e.target === e.currentTarget) {
        setIsVisible(false);
      }
    },
    [],
  );

  // ----------------------------------------------------------
  // 副作用 / Side Effects
  // ----------------------------------------------------------

  /** 注册全局键盘事件监听 / Register global keyboard event listeners */
  useEffect(() => {
    document.addEventListener('keydown', handleKeyDown, true);
    document.addEventListener('focusin', handleFocusIn);
    return () => {
      document.removeEventListener('keydown', handleKeyDown, true);
      document.removeEventListener('focusin', handleFocusIn);
    };
  }, [handleKeyDown, handleFocusIn]);

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
        {idx > 0 && <span style={{ margin: '0 3px', color: 'var(--color-text-muted)', fontSize: 10 }}>+</span>}
        <kbd style={styles.kbd}>{part.trim()}</kbd>
      </React.Fragment>
    ));
  };

  return createPortal(
    <div style={styles.overlay} onClick={handleOverlayClick} role="dialog" aria-modal="true" aria-label="键盘快捷键帮助">
      <div style={styles.panel}>
        {/* 标题区域 / Title area */}
        <h2 style={styles.title}>键盘快捷键 Keyboard Shortcuts</h2>
        <p style={styles.subtitle}>
          按 <kbd style={styles.kbd}>?</kbd> 或 <kbd style={styles.kbd}>Ctrl+/</kbd> 切换此面板 &middot; 点击遮罩或按{' '}
          <kbd style={styles.kbd}>Esc</kbd> 关闭
        </p>

        {/* 分类网格 / Category grid */}
        <div style={styles.grid}>
          {SHORTCUT_CATEGORIES.map((category) => (
            <div key={category.title} style={styles.categoryCard}>
              <div style={styles.categoryTitle}>{category.title}</div>
              {category.items.map((item) => (
                <div key={item.description} style={styles.shortcutRow}>
                  <span style={{ display: 'inline-flex', alignItems: 'center', gap: 0 }}>
                    {renderKbd(item.key)}
                  </span>
                  <span style={styles.description}>{item.description}</span>
                </div>
              ))}
            </div>
          ))}
        </div>

        {/* 底部提示 / Footer hint */}
        <div style={styles.footer}>
          <kbd style={styles.kbd}>?</kbd>
          <span>或</span>
          <kbd style={styles.kbd}>Ctrl+/</kbd>
          <span>随时打开此面板 / Press anytime to open this panel</span>
        </div>
      </div>
    </div>,
    document.body,
  );
};

ShortcutHelp.displayName = 'ShortcutHelp';

export default ShortcutHelp;
