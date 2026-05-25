/**
 * @module components/common/Skeleton
 * @description 骨架屏/Loading 占位组件 / Skeleton loading placeholder component.
 *              在内容加载期间显示占位动画，提升用户感知的加载体验。
 *              支持四种变体：文本、矩形、圆形和面板骨架。
 *
 *              Displays placeholder animations during content loading,
 *              improving perceived loading experience.
 *              Supports four variants: text, rectangular, circular, and panel skeleton.
 */

import React from 'react';

/**
 * Skeleton 变体类型 / Skeleton variant type
 * - text        : 文本行骨架（模拟文字段落）/ Text line skeleton (simulates text paragraphs)
 * - rectangular : 矩形区域骨架（模拟图片、卡片等）/ Rectangular area skeleton (simulates images, cards, etc.)
 * - circular    : 圆形骨架（模拟头像、图标等）/ Circular skeleton (simulates avatars, icons, etc.)
 * - panel       : 面板骨架（模拟整个面板结构：标题栏 + 3 行内容）/ Panel skeleton (simulates full panel: title bar + 3 content lines)
 */
export type SkeletonVariant = 'text' | 'rectangular' | 'circular' | 'panel';

/**
 * Skeleton 组件的 props 接口 / Skeleton component props interface
 * @property variant - 骨架变体类型（默认 'text'）/ Skeleton variant type (default: 'text')
 * @property width - 骨架宽度（默认 '100%'）/ Skeleton width (default: '100%')
 * @property height - 骨架高度（variant='text' 时默认 '1em'，circular 时与 width 相等）/ Skeleton height (default: '1em' for text, equals width for circular)
 * @property count - 重复渲染个数（默认 1）/ Number of repetitions (default: 1)
 * @property className - 额外的 CSS 类名 / Additional CSS class names
 */
export interface SkeletonProps {
  /** 骨架变体类型 / Skeleton variant type */
  variant?: SkeletonVariant;
  /** 骨架宽度，支持 CSS 长度字符串（如 '200px', '50%'）或像素数值 */
  width?: string | number;
  /** 骨架高度，支持 CSS 长度字符串或像素数值 */
  height?: string | number;
  /** 重复渲染行数（仅 variant 为 text 或 rectangular 时生效） */
  count?: number;
  /** 额外的 CSS 类名 / Additional CSS class names */
  className?: string;
}

/**
 * 将 string | number 类型的尺寸值规范化为 CSS 值字符串
 * - undefined -> '100%'
 * - number -> '${number}px'
 * - string -> 原样返回
 *
 * Normalize a string | number size value to a CSS value string.
 * - undefined -> '100%'
 * - number -> '${number}px'
 * - string -> returned as-is
 *
 * @param val - 尺寸值 / Size value
 * @returns CSS 值字符串 / CSS value string
 */
function normalizeSize(val: string | number | undefined): string {
  if (val === undefined) return '100%';
  if (typeof val === 'number') return `${val}px`;
  return val;
}

/** 各变体默认的背景色（使用项目 CSS 变量）/ Default background color for all variants (using project CSS variables) */
const SKELETON_BG = 'var(--color-bg-tertiary)';

/** 各变体默认的圆角（使用项目 CSS 变量）/ Default border radius for all variants (using project CSS variables) */
const SKELETON_RADIUS = 'var(--radius-sm)';

/**
 * Skeleton - 骨架屏占位组件 / Skeleton loading placeholder component
 *
 * 在数据加载过程中显示灰底闪烁占位块，减少用户感知的等待时间。
 * 该组件使用项目中已有的 `.loading-pulse` CSS class 实现闪烁动画。
 *
 * Displays gray pulsing placeholder blocks during data loading,
 * reducing perceived wait time. Uses the existing `.loading-pulse`
 * CSS class for the pulse animation.
 *
 * @example
 * ```tsx
 * // 文本骨架 / Text skeleton
 * <Skeleton variant="text" count={3} />
 *
 * // 图片占位 / Image placeholder
 * <Skeleton variant="rectangular" width={200} height={150} />
 *
 * // 圆形头像 / Circular avatar
 * <Skeleton variant="circular" width={48} />
 *
 * // 面板骨架 / Panel skeleton
 * <Skeleton variant="panel" width="100%" />
 * ```
 */
const Skeleton: React.FC<SkeletonProps> = ({
  variant = 'text',
  width,
  height,
  count = 1,
  className = '',
}) => {
  // 边界检查：count 不能小于 1 / Boundary check: count must not be less than 1
  const safeCount = Math.max(1, count);

  /**
   * 面板骨架变体渲染。
   * 模拟完整的面板结构：标题栏骨架 + 3 行内容骨架行。
   * 内容行的长度依次递减，模拟真实文本排版。
   *
   * Panel skeleton variant rendering.
   * Simulates a complete panel structure: title bar skeleton + 3 content skeleton lines.
   * Content line widths decrease progressively to simulate real text layout.
   */
  if (variant === 'panel') {
    const panelWidth = normalizeSize(width);
    const titleHeight = '28px';
    /** 内容行高度数组 / Content line height array */
    const contentHeights = ['1em', '1em', '0.7em'];
    /** 内容行宽度数组（递减）/ Content line width array (decreasing) */
    const contentWidths = ['100%', '80%', '55%'];

    return (
      <div
        className={`loading-pulse ${className}`.trim()}
        aria-hidden="true"
        style={{
          width: panelWidth,
          display: 'flex',
          flexDirection: 'column',
          gap: '8px',
          padding: '8px 10px',
          border: '1px solid var(--color-border-secondary)',
          borderRadius: SKELETON_RADIUS,
          background: 'var(--color-bg-primary)',
        }}
      >
        {/* 标题栏骨架 / Title bar skeleton */}
        <div
          style={{
            width: panelWidth,
            height: titleHeight,
            background: SKELETON_BG,
            borderRadius: SKELETON_RADIUS,
            marginBottom: '4px',
          }}
        />
        {/* 3 行内容骨架 / 3 content skeleton lines */}
        {contentHeights.map((h, i) => (
          <div
            key={i}
            style={{
              width: contentWidths[i],
              height: h,
              background: SKELETON_BG,
              borderRadius: SKELETON_RADIUS,
            }}
          />
        ))}
      </div>
    );
  }

  /**
   * 圆形骨架变体渲染。
   * height 自动等于 width，形成正圆形。
   *
   * Circular skeleton variant rendering.
   * height automatically equals width, forming a perfect circle.
   */
  if (variant === 'circular') {
    const circularSize = normalizeSize(width ?? 48);
    return (
      <div
        className={`loading-pulse ${className}`.trim()}
        aria-hidden="true"
        style={{
          width: circularSize,
          height: height !== undefined ? normalizeSize(height) : circularSize,
          background: SKELETON_BG,
          borderRadius: '50%',
          flexShrink: 0,
        }}
      />
    );
  }

  /**
   * 文本/矩形骨架变体渲染。
   * text: 圆角较小，模拟文字行高度 / text: smaller radius, simulates text line height
   * rectangular: 圆角适中，用于图片、卡片等占位 / rectangular: medium radius, for images, cards, etc.
   */
  const isText = variant === 'text';
  const defaultHeight = isText ? '1em' : '120px';
  const borderRadius = isText ? 'var(--radius-sm)' : 'var(--radius-md, 4px)';

  // 生成 safeCount 个骨架行 / Generate safeCount skeleton lines
  const items = Array.from({ length: safeCount }, (_, i) => i);

  return (
    <div aria-hidden="true" className={className} style={{ width: normalizeSize(width) }}>
      {items.map((i) => (
        <div
          key={i}
          className="loading-pulse"
          style={{
            width: '100%',
            height: normalizeSize(height || defaultHeight),
            background: SKELETON_BG,
            borderRadius,
            // 最后一行不需要底部间距 / Last line doesn't need bottom spacing
            marginBottom: isText && safeCount > 1 && i < safeCount - 1 ? '8px' : undefined,
          }}
        />
      ))}
    </div>
  );
};

export default Skeleton;
