/**
 * @module utils/format
 * @description 格式化工具函数。
 *              提供数字格式化、坐标显示、时间格式化、文件大小格式化、
 *              缩放百分比显示和字符串截断等功能。
 */

/**
 * 将数字格式化为指定小数位数的字符串。
 * @param value - 要格式化的数字
 * @param decimals - 小数位数（默认 2）
 * @returns 格式化后的数字字符串，非有限值返回 'NaN'
 */
export function formatNumber(value: number, decimals: number = 2): string {
  if (!isFinite(value)) return 'NaN';
  return value.toFixed(decimals);
}

/**
 * 将坐标对格式化为显示字符串。
 * @param x - X 坐标
 * @param y - Y 坐标
 * @returns 格式化的坐标字符串，如 "(3.50, 2.00)"
 */
export function formatCoordinate(x: number, y: number): string {
  return `(${formatNumber(x)}, ${formatNumber(y)})`;
}

/**
 * 将 Date 对象格式化为时间字符串（HH:MM:SS）。
 * @param date - Date 对象（默认当前时间）
 * @returns 格式化的时间字符串，如 "14:30:25"
 */
export function formatTime(date: Date = new Date()): string {
  return date.toLocaleTimeString('en-US', {
    hour12: false,
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  });
}

/**
 * 将 ISO 8601 时间戳格式化为本地化日期时间字符串。
 * @param isoString - ISO 8601 格式的时间戳字符串
 * @returns 中文格式的日期时间字符串
 */
export function formatTimestamp(isoString: string): string {
  const date = new Date(isoString);
  return date.toLocaleString('zh-CN', {
    year: 'numeric',
    month: '2-digit',
    day: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  });
}

/**
 * 将文件大小（字节）格式化为人类可读的字符串。
 * @param bytes - 文件大小（字节）
 * @returns 格式化后的大小字符串，如 "1.5 KB", "2.3 MB"
 */
export function formatFileSize(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  if (bytes < 1024 * 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
  return `${(bytes / (1024 * 1024 * 1024)).toFixed(1)} GB`;
}

/**
 * 将缩放比例格式化为百分比字符串。
 * @param scale - 缩放比例因子（1 = 100%）
 * @returns 格式化的百分比字符串，如 "150%"
 */
export function formatZoom(scale: number): string {
  return `${Math.round(scale * 100)}%`;
}

/**
 * 将字符串截断到指定最大长度，超出部分用省略号代替。
 * @param str - 待截断的字符串
 * @param maxLength - 最大长度（默认 50）
 * @returns 截断后的字符串，超长时末尾添加 "..."
 */
export function truncate(str: string, maxLength: number = 50): string {
  if (str.length <= maxLength) return str;
  return str.slice(0, maxLength - 3) + '...';
}
