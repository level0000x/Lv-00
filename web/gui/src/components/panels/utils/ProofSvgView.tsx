/**
 * @module components/panels/utils/ProofSvgView
 * @description 安全的 SVG 渲染组件。
 *              使用 useRef + useEffect 将 SVG 字符串注入到容器 div 中，
 *              通过 sanitizeHtml 对 HTML 内容进行消毒处理，防止 XSS 攻击。
 *
 *              Safe SVG rendering component.
 *              Uses useRef + useEffect to inject SVG string into a container div,
 *              sanitizing HTML content via sanitizeHtml to prevent XSS attacks.
 */

import React, { useRef, useEffect } from 'react';
import { sanitizeHtml } from '@/utils/sanitizeHtml';

/**
 * ProofSvgView 组件的 props 接口
 * @property svgString - 需要渲染的 SVG 字符串
 */
interface ProofSvgViewProps {
  /** 需要安全渲染的 SVG 字符串 */
  svgString: string;
}

/**
 * ProofSvgView - 安全的 SVG 渲染组件
 *
 * 使用 sanitizeHtml 对 SVG 字符串进行消毒处理后，
 * 通过 innerHTML 注入到容器 div 中。相比 dangerouslySetInnerHTML，
 * 此组件在注入前会移除危险的脚本标签和事件处理器属性。
 *
 * @example
 * ```tsx
 * <ProofSvgView svgString="<svg>...</svg>" />
 * ```
 */
const ProofSvgView: React.FC<ProofSvgViewProps> = ({ svgString }) => {
  /** 容器 div 的引用 */
  const containerRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const container = containerRef.current;
    if (!container || !svgString) return;

    // 对 SVG 字符串进行消毒处理，移除潜在的危险内容
    container.innerHTML = sanitizeHtml(svgString);

    // 组件卸载时清空容器内容，防止内存泄漏
    return () => {
      if (container) {
        container.innerHTML = '';
      }
    };
  }, [svgString]);

  if (!svgString) return null;

  return (
    <div
      ref={containerRef}
      className="pp-svg-container"
      aria-label="证明几何视图 / Proof geometry view"
      role="img"
    />
  );
};

export default ProofSvgView;
