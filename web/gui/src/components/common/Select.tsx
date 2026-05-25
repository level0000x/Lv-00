/**
 * @module components/common/Select
 * @description 可复用的下拉选择框组件 / Reusable select dropdown component.
 *              提供与应用程序主题一致的一致化样式。
 *              封装原生 HTML select 元素，简化值回调接口。
 *
 *              Provides consistent application-themed styling.
 *              Wraps the native HTML select element, simplifying the value callback interface.
 */

import React, { useMemo } from 'react';

/**
 * 选项对象类型 / Option object type
 * @property value - 选项值（提交时的值）/ Option value (submitted value)
 * @property label - 选项显示文本 / Option display text
 */
export interface SelectOption {
  value: string;
  label: string;
}

/**
 * Select 组件的 props 接口 / Select component props interface
 *
 * 继承原生 select 元素的所有 HTML 属性（除 onChange 外），
 * 并提供简化的 onChange 回调（直接接收字符串值）。
 *
 * Inherits all native select HTML attributes (except onChange),
 * and provides a simplified onChange callback (receives string value directly).
 *
 * @property options - 选项对象数组 / Array of option objects
 * @property value - 当前选中的值 / Currently selected value
 * @property onChange - 值变化回调，直接传递字符串值 / Value change callback, passes string value directly
 * @property label - 可选的标签文本 / Optional label text
 * @property className - 额外的 CSS 类名 / Additional CSS class names
 * @property disabled - 是否禁用 / Whether the select is disabled
 */
interface SelectProps extends Omit<React.SelectHTMLAttributes<HTMLSelectElement>, 'onChange'> {
  /** 选项对象数组 / Array of option objects */
  options: SelectOption[];
  /** 值变化回调，直接传递字符串值 / Value change callback, passes string directly */
  onChange: (value: string) => void;
  /** 可选的标签文本，显示在选择框上方 / Optional label text, displayed above the select */
  label?: string;
}

/**
 * Select - 可复用的下拉选择框组件 / Reusable select dropdown component
 *
 * 功能特性 / Features:
 * - 统一的应用主题样式 / Unified application-themed styling
 * - 可选的标签（自动关联 htmlFor/id）/ Optional label (auto-linked via htmlFor/id)
 * - 简化的 onChange 接口 / Simplified onChange interface
 * - 正确转发 ref / Proper ref forwarding
 * - 选项为空时的安全渲染 / Safe rendering when options are empty
 *
 * @example
 * ```tsx
 * const options = [
 *   { value: 'a', label: '选项 A' },
 *   { value: 'b', label: '选项 B' },
 * ];
 *
 * <Select
 *   label="类型"
 *   options={options}
 *   value={selected}
 *   onChange={setSelected}
 * />
 * ```
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
    /**
     * 生成选择框的 id，用于与 label 的 htmlFor 关联
     * 优先使用显式传入的 id，否则根据 label 文本自动生成
     *
     * Generate select id for label htmlFor association.
     * Prioritizes explicit id, otherwise auto-generates from label text.
     */
    const selectId = useMemo(
      () => id ?? (label ? `select-${label.replace(/\s+/g, '-').toLowerCase()}` : undefined),
      [id, label],
    );

    return (
      <div className="input-row">
        {label && (
          <label htmlFor={selectId}>{label}</label>
        )}
        <select
          ref={ref}
          id={selectId}
          className={`select-field ${className}`.trim()}
          value={value}
          onChange={(e) => onChange(e.target.value)}
          disabled={disabled}
          aria-label={!label ? rest['aria-label'] : undefined}
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
