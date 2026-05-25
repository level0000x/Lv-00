/**
 * @module components/common/StatsRow
 * @description 通用统计信息行组件 / Reusable statistics row component
 *
 * 用于在面板底部显示键值对统计信息，替代各面板中重复的 info-box > info-row 模式。
 * 统一了统计信息的 UI 结构，便于维护和扩展。
 *
 * Used to display key-value statistics at the bottom of panels,
 * replacing the repetitive info-box > info-row pattern across panels.
 * Unifies the statistics UI structure for easier maintenance and extension.
 */

import React from 'react';

/**
 * 统计行数据项 / Statistics row data item
 * @property label - 标签文本（如 "点数"）/ Label text (e.g., "Points")
 * @property value - 值文本（如 42 或 "活跃"）/ Value text (e.g., 42 or "Active")
 */
export interface StatsItem {
  /** 标签文本 / Label text */
  label: string;
  /** 值文本，支持字符串和数字类型 / Value text, supports string and number types */
  value: string | number;
}

/**
 * StatsRow 组件的 props 接口 / StatsRow component props interface
 * @property items - 统计数据项数组 / Array of statistics items
 * @property className - 额外的 CSS 类名 / Additional CSS class name
 */
interface StatsRowProps {
  /** 统计数据项数组 / Array of statistics data items */
  items: StatsItem[];
  /** 额外的 CSS 类名 / Additional CSS class name */
  className?: string;
}

/**
 * StatsRow - 通用统计信息行 / Reusable statistics row
 *
 * 渲染一组键值对统计信息，使用 info-box > info-row 样式。
 * 当 items 为空或未定义时不渲染任何内容。
 *
 * Renders a set of key-value statistics using info-box > info-row styles.
 * Renders nothing when items is empty or undefined.
 *
 * @example
 * ```tsx
 * <StatsRow
 *   items={[
 *     { label: '点数', value: 42 },
 *     { label: '线段数', value: 15 },
 *     { label: '状态', value: '活跃' },
 *   ]}
 * />
 * ```
 */
const StatsRow: React.FC<StatsRowProps> = ({ items, className = '' }) => {
  // 空数组或 undefined 时不渲染 / Don't render for empty array or undefined
  if (!items || items.length === 0) return null;

  return (
    <div className={`info-box ${className}`} role="status" aria-label="统计信息 / Statistics">
      {items.map((item, idx) => (
        <div className="info-row" key={idx}>
          <span className="info-label">{item.label}</span>
          <span className="info-value">{String(item.value)}</span>
        </div>
      ))}
    </div>
  );
};

export default StatsRow;
