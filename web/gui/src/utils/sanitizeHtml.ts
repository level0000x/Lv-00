/**
 * @module utils/sanitizeHtml
 * @description HTML 消毒工具函数。
 *              用于清理 AI 返回的 HTML 内容，防止 XSS 攻击。
 *
 * 安全策略：
 * - 移除 <script> 标签及其内容
 * - 移除所有事件处理器属性（onclick, onerror, onload, onmouseover 等）
 * - 移除 javascript: 协议的 URL
 * - 移除 <iframe>, <object>, <embed> 等危险标签
 * - 保留安全的 HTML 标签和样式
 */

/**
 * 需要完全移除的危险标签列表。
 * 这些标签可能被用于执行恶意脚本或嵌入外部内容。
 */
const DANGEROUS_TAGS = ['script', 'iframe', 'object', 'embed', 'applet', 'form'];

/**
 * 对 HTML 内容进行消毒处理，移除潜在的 XSS 攻击向量。
 *
 * 处理步骤：
 * 1. 移除 <script> 标签及其内容
 * 2. 移除其他危险标签（iframe, object, embed 等）
 * 3. 移除所有事件处理器属性（onclick, onerror, onload 等）
 * 4. 移除 javascript: 协议的 URL
 *
 * @param html - 需要消毒的 HTML 字符串
 * @returns 消毒后的安全 HTML 字符串
 *
 * @example
 * ```typescript
 * const safeHtml = sanitizeHtml('<p onclick="alert(1)">Hello</p><script>alert("xss")</script>');
 * // 返回: '<p>Hello</p>'
 * ```
 */
export function sanitizeHtml(html: string): string {
  if (!html) return html;

  let sanitized = html;

  // 步骤 1：移除 <script> 标签及其内容（贪婪匹配）
  // 匹配 <script...>...</script>，包括多行内容
  sanitized = sanitized.replace(
    /<script\b[^<]*(?:(?!<\/script>)<[^<]*)*<\/script>/gi,
    '',
  );

  // 步骤 2：移除其他危险标签（自闭合和普通标签）
  for (const tag of DANGEROUS_TAGS) {
    if (tag === 'script') continue; // 已在步骤 1 处理
    // 移除普通标签：<tag...>...</tag>
    sanitized = sanitized.replace(
      new RegExp(`<${tag}\\b[^>]*>[\\s\\S]*?<\\/${tag}>`, 'gi'),
      '',
    );
    // 移除自闭合标签：<tag... />
    sanitized = sanitized.replace(
      new RegExp(`<${tag}\\b[^>]*/?>`, 'gi'),
      '',
    );
  }

  // 步骤 3：移除所有事件处理器属性
  // 匹配 on*="..." 或 on*='...' 形式的属性
  sanitized = sanitized.replace(
    /\s+on\w+\s*=\s*(?:"[^"]*"|'[^']*'|[^\s>]*)/gi,
    '',
  );

  // 步骤 4：移除 javascript: 协议的 URL（href 和 src 属性中）
  sanitized = sanitized.replace(
    /(href|src|action)\s*=\s*["']?\s*javascript\s*:[^"'>\s]*/gi,
    '$1=""',
  );

  return sanitized;
}

export default sanitizeHtml;
