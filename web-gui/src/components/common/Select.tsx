/**
 * @module components/common/Select
 * @description 可复用的下拉选择框组件。
 *              提供与应用程序主题一致的一致化样式。
 */

import React from 'react';

/**
 * 选项对象类型
 * @property value - 选项值
 * @property label - 选项显示文本
 */
export interface SelectOption {
  value: string;
  label: string;
}

/**
 * Select 组件的 props 接口
 * @property value - 当前选中的值
 * @property onChange - 值变化回调，直接传递字符串值
 * @property options - 选项对象数组
 * @property label - 可选的标签文本
 * @property className - 额外的 CSS 类名
 * @property disabled - 是否禁用
 */
interface SelectProps extends Omit<React.SelectHTMLAttributes<HTMLSelectElement>, 'onChange'> {
  options: SelectOption[];
  onChange: (value: string) => void;
  label?: string;
}

/**
 * Select - 可复用的下拉选择框组件
 *
 * 封装原生 HTML select 元素，提供统一的应用主题样式
 * 和简化的值回调接口。
 */
const Select = React.memo(React.forwardRef<HTMLSelectElement, SelectProps>(
  function Select({
    value,
    onChange,
    options,
    label,
    className = '',
    disabled = false,
    id,
    ...rest
  }, ref) {
    const selectId = id ?? (label ? `select-${label.replace(/\s+/g, '-').toLowerCase()}` : undefined);

    return (
      <div className="input-row">
        {label && <label htmlFor={selectId}>{label}</label>}
        <select
          ref={ref}
          id={selectId}
          className={`select-field ${className}`.trim()}
          value={value}
          onChange={(e) => onChange(e.target.value)}
          disabled={disabled}
          {...rest}
        >
          {options.map((opt) => (
            <option key={opt.value} value={opt.value}>
              {opt.label}
            </option>
          ))}
        </select>
      </div>
    );
  }
));

Select.displayName = 'Select';

export default Select;
