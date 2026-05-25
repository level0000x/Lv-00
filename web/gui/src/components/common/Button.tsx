/**
 * @module components/common/Button
 * @description 可复用的按钮组件 / Reusable button component.
 *              支持多种视觉变体（default、primary、accent、ghost、danger），
 *              包含点击涟漪效果和加载状态。
 *              Supports multiple visual variants (default, primary, accent, ghost, danger),
 *              with click ripple effect and loading state.
 */

import React, { useCallback, useRef, useMemo } from 'react';

/**
 * 按钮变体类型 / Button variant type
 * - default  : 标准按钮样式 / Standard button style
 * - primary  : 主要操作按钮 / Primary action button
 * - accent   : 强调色操作按钮 / Accent-colored action button
 * - ghost    : 透明背景按钮 / Transparent background button
 * - danger   : 危险操作按钮 / Destructive action button
 */
export type ButtonVariant = 'default' | 'primary' | 'accent' | 'ghost' | 'danger';

/**
 * Button 组件的 props 接口 / Button component props interface
 * @property variant - 视觉变体（默认 'default'）/ Visual variant (default: 'default')
 * @property small - 是否使用小尺寸 / Use small size
 * @property tooltip - 悬停提示文本，同时用作 title 属性 / Hover tooltip text, also used as title attribute
 * @property shortcut - 键盘快捷键标识，以小型 keycap 样式显示在按钮内容末尾 / Keyboard shortcut label, displayed as a small keycap badge at the end of button content
 * @property children - 按钮内容 / Button content
 * @property onClick - 点击回调 / Click callback
 * @property disabled - 是否禁用 / Disabled state
 * @property title - HTML title 属性提示文本 / HTML title attribute tooltip text
 * @property className - 额外的 CSS 类名 / Additional CSS class names
 * @property type - HTML 按钮 type 属性 / HTML button type attribute
 */
export interface ButtonProps extends React.ButtonHTMLAttributes<HTMLButtonElement> {
  variant?: ButtonVariant;
  small?: boolean;
  /** 悬停提示文本 / Hover tooltip text */
  tooltip?: string;
  /** 键盘快捷键标识，以 keycap 样式展示 / Keyboard shortcut label, rendered as a keycap badge */
  shortcut?: string;
  /** ARIA 标签（无障碍访问） / ARIA label for accessibility */
  ariaLabel?: string;
}

/**
 * 合并多个 ref 引用为一个回调 ref
 * 兼容 React 18 中 forwardRef 与内部 ref 的合并需求。
 *
 * Merge multiple refs into a single callback ref.
 * Compatible with merging forwarded refs and internal refs in React 18.
 *
 * @param refs - 需要合并的 ref 数组（可包含 callback ref 和 object ref）
 * @returns 合并后的回调 ref
 */
function mergeMultipleRefs<T>(
  ...refs: Array<React.Ref<T> | undefined | null>
): React.RefCallback<T> {
  return (node: T | null) => {
    refs.forEach((ref) => {
      if (typeof ref === 'function') {
        ref(node);
      } else if (ref && typeof ref === 'object') {
        (ref as React.MutableRefObject<T | null>).current = node;
      }
    });
  };
}

/**
 * Button - 可复用的按钮组件 / Reusable button component
 *
 * 提供统一的应用主题样式，支持多种视觉变体。
 * 点击时产生涟漪动画效果，支持禁用状态。
 * 正确转发 ref，支持外部获取按钮 DOM 元素。
 *
 * Provides unified application-themed styling with multiple visual variants.
 * Creates a ripple animation effect on click, supports disabled state.
 * Properly forwards ref, allowing external access to the button DOM element.
 */
const Button = React.memo(React.forwardRef<HTMLButtonElement, ButtonProps>(
  function Button({
    variant = 'default',
    small = false,
    children,
    className = '',
    disabled = false,
    onClick,
    tooltip,
    shortcut,
    title,
    ariaLabel,
    type = 'button',
    ...rest
  }, forwardedRef) {
    /** 内部 ref，用于涟漪效果的 DOM 操作 */
    const internalRef = useRef<HTMLButtonElement>(null);

    /** 合并外部 forwardedRef 和内部 internalRef */
    const mergedRef = useMemo(
      () => mergeMultipleRefs(forwardedRef, internalRef),
      [forwardedRef],
    );

    /** 根据变体生成 CSS 类名 / Generate CSS class based on variant */
    const variantClass = variant !== 'default' ? `btn-${variant}` : '';
    /** 根据尺寸生成 CSS 类名 / Generate CSS class based on size */
    const smallClass = small ? 'btn-small' : '';

    /**
     * 点击时创建涟漪效果 / Create ripple effect on click
     *
     * 实现原理：
     * 1. 创建一个绝对定位的 span 元素作为涟漪
     * 2. 根据点击位置计算涟漪的起始坐标
     * 3. 涟漪大小取按钮宽高的最大值以确保覆盖整个按钮
     * 4. 动画结束后自动移除 DOM 元素
     */
    const handleClick = useCallback(
      (e: React.MouseEvent<HTMLButtonElement>) => {
        const button = internalRef.current;
        if (!button || disabled) return;

        // 创建涟漪元素 / Create ripple element
        const ripple = document.createElement('span');
        ripple.className = 'ripple';

        const rect = button.getBoundingClientRect();
        const size = Math.max(rect.width, rect.height);
        ripple.style.width = ripple.style.height = `${size}px`;
        ripple.style.left = `${e.clientX - rect.left - size / 2}px`;
        ripple.style.top = `${e.clientY - rect.top - size / 2}px`;

        button.appendChild(ripple);

        // 动画结束后移除涟漪元素 / Remove ripple element after animation ends
        ripple.addEventListener('animationend', () => {
          ripple.remove();
        });

        // 触发外部点击回调 / Trigger external click callback
        onClick?.(e);
      },
      [disabled, onClick],
    );

    // 组合 title：优先使用显式 title，其次使用 tooltip
    // Merge title: explicit title takes priority, then tooltip
    const resolvedTitle = title ?? tooltip;

    // 组合 aria-label：优先使用显式 ariaLabel，其次使用 resolvedTitle
    // Merge aria-label: explicit ariaLabel takes priority, then resolvedTitle
    const resolvedAriaLabel = ariaLabel ?? resolvedTitle;

    return (
      <button
        ref={mergedRef}
        className={`btn ${variantClass} ${smallClass} ${className}`.trim()}
        disabled={disabled}
        onClick={handleClick}
        title={resolvedTitle}
        aria-label={resolvedAriaLabel}
        aria-disabled={disabled}
        type={type}
        {...rest}
      >
        {children}
        {/* 键盘快捷键 keycap 徽标 / Keyboard shortcut keycap badge */}
        {shortcut && (
          <span className="btn-shortcut" aria-hidden="true">
            {shortcut}
          </span>
        )}
      </button>
    );
  }
));

Button.displayName = 'Button';

export default Button;
