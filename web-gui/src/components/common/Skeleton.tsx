/**
 * @module components/common/Skeleton
 * @description 骨架屏/Loading 占位组件。
 *              在内容加载期间显示占位动画，提升用户感知的加载体验。
 *              支持四种变体：文本、矩形、圆形和面板骨架。
 */

import React from 'react';

/**
 * Skeleton 变体类型。
 * - text        : 文本行骨架（模拟文字段落）
 * - rectangular : 矩形区域骨架（模拟图片、卡片等）
 * - circular    : 圆形骨架（模拟头像、图标等）
 * - panel       : 面板骨架（模拟整个面板结构：标题栏 + 3 行内容）
 */
export type SkeletonVariant = 'text' | 'rectangular' | 'circular' | 'panel';

/**
 * Skeleton 组件的 props 接口。
 * @property variant - 骨架变体类型（默认 'text'）
 * @property width - 骨架宽度（默认 '100%'）
 * @property height - 骨架高度（variant='text' 时默认 '1em'，circular 时与 width 相等）
 * @property count - 重复渲染个数（默认 1）
 * @property className - 额外的 CSS 类名
 */
export interface SkeletonProps {
  /** 骨架变体类型 */
  variant?: SkeletonVariant;
  /** 骨架宽度，支持 CSS 长度字符串（如 '200px', '50%'）或像素数值 */
  width?: string | number;
  /** 骨架高度，支持 CSS 长度字符串或像素数值 */
  height?: string | number;
  /** 重复渲染行数（仅 variant 为 text 或 rectangular 时生效） */
  count?: number;
  /** 额外的 CSS 类名 */
  className?: string;
}

/** 将 string | number 类型的尺寸值规范化为 CSS 值字符串 */
function normalizeSize(val: string | number | undefined): string {
  if (val === undefined) return '100%';
  if (typeof val === 'number') return `${val}px`;
  return val;
}

/** 各变体默认的背景色（使用项目 CSS 变量） */
const SKELETON_BG = 'var(--color-bg-tertiary)';
const SKELETON_RADIUS = 'var(--radius-sm)';

/**
 * Skeleton - 骨架屏占位组件
 *
 * 在数据加载过程中显示灰底闪烁占位块，减少用户感知的等待时间。
 * 该组件使用项目中已有的 `.loading-pulse` CSS class 实现闪烁动画。
 *
 * @example
 * ```tsx
 * // 文本骨架
 * <Skeleton variant="text" count={3} />
 *
 * // 图片占位
 * <Skeleton variant="rectangular" width={200} height={150} />
 *
 * // 圆形头像
 * <Skeleton variant="circular" width={48} />
 *
 * // 面板骨架
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
  /**
   * 面板骨架变体渲染。
   * 模拟完整的面板结构：标题栏骨架 + 3 行内容骨架行。
   * 内容行的长度依次递减，模拟真实文本排版。
   */
  if (variant === 'panel') {
    const panelWidth = normalizeSize(width);
    const titleHeight = '28px';
    const contentHeights = ['1em', '1em', '0.7em'];
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
        {/* 标题栏骨架 */}
        <div
          style={{
            width: panelWidth,
            height: titleHeight,
            background: SKELETON_BG,
            borderRadius: SKELETON_RADIUS,
            marginBottom: '4px',
          }}
        />
        {/* 3 行内容骨架 */}
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
   */
  if (variant === 'circular') {
    const circularSize = normalizeSize(width || 48);
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
   * text: 圆角较小，模拟文字行高度
   * rectangular: 圆角适中，用于图片、卡片等占位
   */
  const isText = variant === 'text';
  const defaultHeight = isText ? '1em' : '120px';
  const borderRadius = isText ? 'var(--radius-sm)' : 'var(--radius-md, 4px)';

  // 生成 count 个骨架行
  const items = Array.from({ length: count }, (_, i) => i);

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
            marginBottom: isText && count > 1 && i < count - 1 ? '8px' : undefined,
          }}
        />
      ))}
    </div>
  );
};

export default Skeleton;
