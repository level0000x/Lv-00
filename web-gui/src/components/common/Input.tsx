/**
 * @module components/common/Input
 * @description 可复用的文本输入框组件。
 *              提供与应用程序主题一致的一致化样式和可选标签。
 */

import React from 'react';

/**
 * Input 组件的 props 接口
 * @property value - 输入值
 * @property onChange - 值变化回调，直接传递字符串值
 * @property label - 可选的标签文本
 * @property placeholder - 占位文本
 * @property type - HTML 输入类型（默认 'text'）
 * @property className - 额外的 CSS 类名
 */
interface InputProps extends Omit<React.InputHTMLAttributes<HTMLInputElement>, 'onChange'> {
  label?: string;
  onChange: (value: string) => void;
}

/**
 * Input - 可复用的文本输入框组件
 *
 * 封装原生 HTML input 元素，提供统一的应用主题样式、
 * 可选的标签以及简化后的值回调接口（直接传递字符串而非事件对象）。
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
    ...rest
  }, ref) {
    const inputId = id ?? (label ? `input-${label.replace(/\s+/g, '-').toLowerCase()}` : undefined);

    return (
      <div className="input-row">
        {label && <label htmlFor={inputId}>{label}</label>}
        <input
          ref={ref}
          id={inputId}
          type={type}
          className={`input-field ${className}`.trim()}
          value={value}
          onChange={(e) => onChange(e.target.value)}
          placeholder={placeholder}
          {...rest}
        />
      </div>
    );
  }
));

Input.displayName = 'Input';

export default Input;
