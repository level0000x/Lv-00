/**
 * @module components/common/StatsRow
 * @description 通用统计信息行组件 / Reusable statistics row component
 *
 * 用于在面板底部显示键值对统计信息，替代各面板中重复的 info-box > info-row 模式。
 */

import React from 'react';

/** 统计行数据项 / Statistics row data item */
export interface StatsItem {
  /** 标签文本 / Label text */
  label: string;
  /** 值文本 / Value text */
  value: string | number;
}

interface StatsRowProps {
  /** 统计数据项数组 / Array of statistics items */
  items: StatsItem[];
  /** 额外的 CSS 类名 / Additional CSS class name */
  className?: string;
}

/**
 * 通用统计信息行
 *
 * @description 渲染一组键值对统计信息，使用 info-box > info-row 样式。
 *              支持自定义类名扩展。
 *
 * @param props - 组件属性
 * @returns 统计信息行 JSX
 */
const StatsRow: React.FC<StatsRowProps> = ({ items, className = '' }) => {
  if (!items || items.length === 0) return null;

  return (
    <div className={`info-box ${className}`}>
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
