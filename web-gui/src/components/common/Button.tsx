/**
 * @module components/common/Button
 * @description 可复用的按钮组件 / Reusable button component.
 *              支持多种视觉变体（default、primary、accent、ghost、danger），
 *              包含点击涟漪效果和加载状态。
 *              Supports multiple visual variants (default, primary, accent, ghost, danger),
 *              with click ripple effect and loading state.
 */

import React, { useCallback, useRef } from 'react';

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
}

/**
 * Button - 可复用的按钮组件 / Reusable button component
 *
 * 提供统一的应用主题样式，支持多种视觉变体。
 * 点击时产生涟漪动画效果，支持禁用状态。
 *
 * Provides unified application-themed styling with multiple visual variants.
 * Creates a ripple animation effect on click, supports disabled state.
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
    ...rest
  }, ref) {
    const internalRef = useRef<HTMLButtonElement>(null);
    const buttonRef = (ref as React.RefObject<HTMLButtonElement>) ?? internalRef;

    const variantClass = variant !== 'default' ? `btn-${variant}` : '';
    const smallClass = small ? 'btn-small' : '';

    /**
     * 点击时创建涟漪效果 / Create ripple effect on click
     */
    const handleClick = useCallback(
      (e: React.MouseEvent<HTMLButtonElement>) => {
        const button = buttonRef.current;
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

        onClick?.(e);
      },
      [disabled, onClick, buttonRef],
    );

    // 组合 title：优先使用显式 title，其次使用 tooltip / Merge title: explicit title first, then tooltip
    const resolvedTitle = title ?? tooltip;

    return (
      <button
        ref={buttonRef}
        className={`btn ${variantClass} ${smallClass} ${className}`.trim()}
        disabled={disabled}
        onClick={handleClick}
        title={resolvedTitle}
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
