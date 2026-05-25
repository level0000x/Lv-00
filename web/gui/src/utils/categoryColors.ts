/**
 * @module utils/categoryColors
 * @description 流式事件类别颜色映射工具函数。
 *              从 SidebarRight.tsx 中提取，用于渲染流式事件条目
 *              左侧的竖线指示器和过滤器按钮的颜色标识。
 *
 *              Stream event category color mapping utility.
 *              Extracted from SidebarRight.tsx for reuse and testability.
 */

// ================================================================
// 颜色映射函数 / Color Mapping Function
// ================================================================

/**
 * 根据事件类别返回对应的 CSS 颜色值，
 * 用于渲染流式事件条目左侧的竖线指示器和过滤器按钮。
 *
 * 颜色映射规则：
 * - engine: 信任绿（引擎启动/停止等核心事件）
 * - normalize/rewrite/solve/proof: 主题强调色（几何推导过程）
 * - func_block: 绿色（函数块相关事件）
 * - conflict: 信任红（冲突/错误事件）
 * - info: 信任蓝（信息类事件）
 * - 其他: 文本灰色（兜底）
 *
 * @param category - 事件类别字符串
 *                   (engine/normalize/rewrite/solve/proof/func_block/conflict/info)
 * @returns CSS 颜色值（CSS 变量名或十六进制颜色值）
 *
 * @example
 * getCategoryColor('engine');   // 'var(--color-trust-green)'
 * getCategoryColor('conflict'); // 'var(--color-trust-red)'
 * getCategoryColor('unknown');  // 'var(--color-text-muted)'
 */
export function getCategoryColor(category: string): string {
  switch (category) {
    case 'engine':
      return 'var(--color-trust-green)';
    case 'normalize':
    case 'rewrite':
    case 'solve':
    case 'proof':
      return 'var(--color-accent)';
    case 'func_block':
      return '#39d353';
    case 'conflict':
      return 'var(--color-trust-red)';
    case 'info':
      return 'var(--color-trust-blue)';
    default:
      return 'var(--color-text-muted)';
  }
}
