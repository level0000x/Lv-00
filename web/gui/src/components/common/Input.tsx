/**
 * @module components/common/Input
 * @description 可复用的文本输入框组件 / Reusable text input component.
 *              提供与应用程序主题一致的一致化样式和可选标签。
 *              封装原生 HTML input 元素，简化值回调接口（直接传递字符串而非事件对象）。
 *
 *              Provides consistent application-themed styling with an optional label.
 *              Wraps the native HTML input element, simplifying the value callback
 *              interface (passes string directly instead of event object).
 */

import React, { useMemo } from 'react';

/**
 * Input 组件的 props 接口 / Input component props interface
 *
 * 继承原生 input 元素的所有 HTML 属性（除 onChange 外），
 * 并提供简化的 onChange 回调（直接接收字符串值）。
 *
 * Inherits all native input HTML attributes (except onChange),
 * and provides a simplified onChange callback (receives string value directly).
 *
 * @property value - 输入值 / Input value
 * @property onChange - 值变化回调，直接传递字符串值 / Value change callback, passes string value directly
 * @property label - 可选的标签文本 / Optional label text
 * @property placeholder - 占位文本 / Placeholder text
 * @property type - HTML 输入类型（默认 'text'）/ HTML input type (default: 'text')
 * @property className - 额外的 CSS 类名 / Additional CSS class names
 */
interface InputProps extends Omit<React.InputHTMLAttributes<HTMLInputElement>, 'onChange'> {
  /** 可选的标签文本，显示在输入框上方 / Optional label text, displayed above the input */
  label?: string;
  /** 值变化回调，直接传递字符串值而非事件对象 / Value change callback, passes string directly */
  onChange: (value: string) => void;
}

/**
 * Input - 可复用的文本输入框组件 / Reusable text input component
 *
 * 功能特性 / Features:
 * - 统一的应用主题样式 / Unified application-themed styling
 * - 可选的标签（自动关联 htmlFor/id）/ Optional label (auto-linked via htmlFor/id)
 * - 简化的 onChange 接口 / Simplified onChange interface
 * - 正确转发 ref / Proper ref forwarding
 *
 * @example
 * ```tsx
 * <Input
 *   label="名称"
 *   value={name}
 *   onChange={setName}
 *   placeholder="请输入名称..."
 * />
 * ```
 */
const Input = React.memo(React.forwardRef<HTMLInputElement, InputProps>(
  function Input({
    value,
    onChange,
    label,
    placeholder,
    type = 'text',
    className = '',
    id,
    disabled = false,
    ...rest
  }, ref) {
    /**
     * 生成输入框的 id，用于与 label 的 htmlFor 关联
     * 优先使用显式传入的 id，否则根据 label 文本自动生成
     *
     * Generate input id for label htmlFor association.
     * Prioritizes explicit id, otherwise auto-generates from label text.
     */
    const inputId = useMemo(
      () => id ?? (label ? `input-${label.replace(/\s+/g, '-').toLowerCase()}` : undefined),
      [id, label],
    );

    return (
      <div className="input-row">
        {label && (
          <label htmlFor={inputId}>{label}</label>
        )}
        <input
          ref={ref}
          id={inputId}
          type={type}
          className={`input-field ${className}`.trim()}
          value={value}
          onChange={(e) => onChange(e.target.value)}
          placeholder={placeholder}
          disabled={disabled}
          aria-label={!label ? placeholder : undefined}
          {...rest}
        />
      </div>
    );
  }
));

Input.displayName = 'Input';

export default Input;
