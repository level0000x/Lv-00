/**
 * @module utils/format
 * @description 类型安全的格式化工具库
 *
 *              提供数字格式化、坐标显示、时间格式化、文件大小格式化、
 *              缩放百分比显示和字符串截断等功能。
 *
 *              【优化说明】v3.6.0
 *              - 完整的 TypeScript 类型定义
 *              - 增强的输入验证和错误处理
 *              - 更多的格式化选项
 *              - 标准化的返回值（never 返回空字符串而非异常）
 *
 * @since 3.6.0
 */

// ================================================================
// 基础格式化函数
// ================================================================

/**
 * 将数字格式化为指定小数位数的字符串
 *
 * @param value - 要格式化的数字
 * @param decimals - 小数位数（默认 2）
 * @returns 格式化后的数字字符串，非有限值返回 'NaN'
 *
 * @example
 * formatNumber(3.14159); // "3.14"
 * formatNumber(100, 0);   // "100"
 * formatNumber(NaN);      // "NaN"
 */
export function formatNumber(value: number, decimals: number = 2): string {
  if (!isFinite(value)) return 'NaN';
  return value.toFixed(decimals);
}

/**
 * 将数字格式化为带千位分隔符的字符串
 *
 * @param value - 要格式化的数字
 * @param decimals - 小数位数（默认 0，不显示小数）
 * @returns 带千位分隔符的数字字符串
 *
 * @example
 * formatThousands(1234567); // "1,234,567"
 * formatThousands(1234.56, 2); // "1,234.56"
 */
export function formatThousands(value: number, decimals: number = 0): string {
  if (!isFinite(value)) return 'NaN';
  return value.toLocaleString('en-US', {
    minimumFractionDigits: decimals,
    maximumFractionDigits: decimals,
  });
}

/**
 * 将坐标对格式化为显示字符串
 *
 * @param x - X 坐标
 * @param y - Y 坐标
 * @param decimals - 小数位数（默认 2）
 * @returns 格式化的坐标字符串
 *
 * @example
 * formatCoordinate(3.14159, 2.71828); // "(3.14, 2.72)"
 * formatCoordinate(1, 2, 0);         // "(1, 2)"
 */
export function formatCoordinate(x: number, y: number, decimals: number = 2): string {
  return `(${formatNumber(x, decimals)}, ${formatNumber(y, decimals)})`;
}

/**
 * 将坐标数组格式化为显示字符串
 *
 * @param coords - 坐标数组 [x, y]
 * @param decimals - 小数位数（默认 2）
 * @returns 格式化的坐标字符串
 */
export function formatCoordArray(coords: [number, number], decimals: number = 2): string {
  return `(${formatNumber(coords[0], decimals)}, ${formatNumber(coords[1], decimals)})`;
}

// ================================================================
// 时间格式化
// ================================================================

/**
 * 将 Date 对象格式化为时间字符串（HH:MM:SS）
 *
 * @param date - Date 对象（默认当前时间）
 * @returns 格式化的时间字符串
 *
 * @example
 * formatTime();                        // "14:30:25"
 * formatTime(new Date('2024-01-01')); // "00:00:00"
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
 * 将 Date 对象格式化为中文日期字符串
 *
 * @param date - Date 对象（默认当前时间）
 * @returns 格式化的日期字符串
 *
 * @example
 * formatDate();                        // "2024-01-15"
 * formatDate(new Date('2024-01-15')); // "2024-01-15"
 */
export function formatDate(date: Date = new Date()): string {
  return date.toLocaleDateString('zh-CN', {
    year: 'numeric',
    month: '2-digit',
    day: '2-digit',
  });
}

/**
 * 将 Date 对象格式化为中文日期时间字符串
 *
 * @param date - Date 对象（默认当前时间）
 * @returns 格式化的日期时间字符串
 *
 * @example
 * formatDateTime(); // "2024-01-15 14:30:25"
 */
export function formatDateTime(date: Date = new Date()): string {
  return `${formatDate(date)} ${formatTime(date)}`;
}

/**
 * 将 ISO 8601 时间戳格式化为中文日期时间字符串
 *
 * @param isoString - ISO 8601 格式的时间戳字符串
 * @returns 格式化的日期时间字符串
 */
export function formatTimestamp(isoString: string): string {
  const date = new Date(isoString);
  if (isNaN(date.getTime())) return 'Invalid Date';
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
 * 将毫秒数格式化为时长字符串
 *
 * @param ms - 毫秒数
 * @returns 格式化的时长字符串
 *
 * @example
 * formatDuration(3661000); // "1h 1m 1s"
 * formatDuration(45000);   // "45s"
 * formatDuration(500);     // "0.5s"
 */
export function formatDuration(ms: number): string {
  if (ms < 0) return '0s';

  const seconds = Math.floor(ms / 1000);
  const minutes = Math.floor(seconds / 60);
  const hours = Math.floor(minutes / 60);

  if (hours > 0) {
    const remainingMinutes = minutes % 60;
    return remainingMinutes > 0 ? `${hours}h ${remainingMinutes}m` : `${hours}h`;
  }

  if (minutes > 0) {
    const remainingSeconds = seconds % 60;
    return remainingSeconds > 0 ? `${minutes}m ${remainingSeconds}s` : `${minutes}m`;
  }

  if (seconds > 0) {
    return `${seconds}s`;
  }

  return `${ms}ms`;
}

// ================================================================
// 文件大小格式化
// ================================================================

/**
 * 将文件大小（字节）格式化为人类可读的字符串
 *
 * @param bytes - 文件大小（字节）
 * @param decimals - 小数位数（默认 1）
 * @returns 格式化后的大小字符串
 *
 * @example
 * formatFileSize(512);              // "512 B"
 * formatFileSize(1536);            // "1.5 KB"
 * formatFileSize(1048576);         // "1.0 MB"
 * formatFileSize(1073741824);      // "1.0 GB"
 */
export function formatFileSize(bytes: number, decimals: number = 1): string {
  if (bytes < 0) return '0 B';
  if (bytes < 1024) return `${Math.round(bytes)} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(decimals)} KB`;
  if (bytes < 1024 * 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(decimals)} MB`;
  return `${(bytes / (1024 * 1024 * 1024)).toFixed(decimals)} GB`;
}

/**
 * 将字节数转换为人类可读的单位字符串
 *
 * @param bytes - 字节数
 * @param useBinary - 是否使用二进制单位（1024 为基准）
 * @returns 带单位的字符串
 */
export function formatBytes(bytes: number, useBinary = false): string {
  const base = useBinary ? 1024 : 1000;
  const units = useBinary
    ? ['B', 'KiB', 'MiB', 'GiB', 'TiB']
    : ['B', 'KB', 'MB', 'GB', 'TB'];

  if (bytes < 0) return `0 ${units[0]}`;

  let unitIndex = 0;
  let value = bytes;

  while (value >= base && unitIndex < units.length - 1) {
    value /= base;
    unitIndex++;
  }

  return `${value.toFixed(2)} ${units[unitIndex]}`;
}

// ================================================================
// 缩放和百分比
// ================================================================

/**
 * 将缩放比例格式化为百分比字符串
 *
 * @param scale - 缩放比例因子（1 = 100%）
 * @param decimals - 小数位数（默认 0）
 * @returns 格式化的百分比字符串
 *
 * @example
 * formatZoom(1.5);   // "150%"
 * formatZoom(0.75);  // "75%"
 * formatZoom(1.234); // "123%"
 */
export function formatZoom(scale: number, decimals: number = 0): string {
  if (!isFinite(scale)) return 'NaN%';
  return `${(scale * 100).toFixed(decimals)}%`;
}

/**
 * 将百分比值格式化为字符串
 *
 * @param value - 百分比值（0-100）
 * @param decimals - 小数位数（默认 1）
 * @returns 格式化的百分比字符串
 *
 * @example
 * formatPercent(75);     // "75.0%"
 * formatPercent(33.333);  // "33.3%"
 */
export function formatPercent(value: number, decimals: number = 1): string {
  if (!isFinite(value)) return 'NaN%';
  return `${value.toFixed(decimals)}%`;
}

// ================================================================
// 字符串处理
// ================================================================

/**
 * 将字符串截断到指定最大长度，超出部分用省略号代替
 *
 * @param str - 待截断的字符串
 * @param maxLength - 最大长度（默认 50）
 * @param ellipsis - 省略字符（默认 "..."）
 * @returns 截断后的字符串
 *
 * @example
 * truncate('Hello World', 8);          // "Hello..."
 * truncate('Hello', 10);                // "Hello"
 * truncate('Hello World', 8, '…');     // "Hello…"
 */
export function truncate(str: string, maxLength: number = 50, ellipsis: string = '...'): string {
  if (!str || str.length <= maxLength) return str;
  return str.slice(0, Math.max(0, maxLength - ellipsis.length)) + ellipsis;
}

/**
 * 将字符串首字母大写
 *
 * @param str - 输入字符串
 * @returns 首字母大写的字符串
 *
 * @example
 * capitalize('hello');  // "Hello"
 * capitalize('HELLO');  // "HELLO"
 */
export function capitalize(str: string): string {
  if (!str) return str;
  return str.charAt(0).toUpperCase() + str.slice(1);
}

/**
 * 将字符串转换为驼峰命名
 *
 * @param str - 输入字符串（蛇形或短横线命名）
 * @returns 驼峰命名的字符串
 *
 * @example
 * camelCase('hello_world'); // "helloWorld"
 * camelCase('hello-world'); // "helloWorld"
 */
export function camelCase(str: string): string {
  return str.replace(/[-_](\w)/g, (_, c) => (c ? c.toUpperCase() : ''));
}

/**
 * 将字符串转换为帕斯卡命名
 *
 * @param str - 输入字符串
 * @returns 帕斯卡命名的字符串
 *
 * @example
 * pascalCase('hello_world'); // "HelloWorld"
 */
export function pascalCase(str: string): string {
  return capitalize(camelCase(str));
}

/**
 * 将字符串转换为蛇形命名
 *
 * @param str - 输入字符串
 * @returns 蛇形命名的字符串
 *
 * @example
 * snakeCase('HelloWorld'); // "hello_world"
 */
export function snakeCase(str: string): string {
  return str.replace(/[A-Z]/g, (c) => `_${c.toLowerCase()}`);
}

// ================================================================
// 数学格式化
// ================================================================

/**
 * 将数字格式化为科学计数法字符串
 *
 * @param value - 要格式化的数字
 * @param decimals - 小数位数（默认 2）
 * @returns 科学计数法字符串
 *
 * @example
 * formatScientific(1234567);    // "1.23e+6"
 * formatScientific(0.0000123);  // "1.23e-5"
 */
export function formatScientific(value: number, decimals: number = 2): string {
  if (!isFinite(value)) return 'NaN';
  if (value === 0) return '0';
  return value.toExponential(decimals);
}

/**
 * 将数字格式化为 SI 单位前缀字符串
 *
 * @param value - 要格式化的数字
 * @param decimals - 小数位数（默认 1）
 * @returns 带 SI 单位前缀的字符串
 *
 * @example
 * formatSI(1200);   // "1.2k"
 * formatSI(0.001);  // "1.0m"
 */
export function formatSI(value: number, decimals: number = 1): string {
  if (!isFinite(value)) return 'NaN';
  if (value === 0) return '0';

  const prefixes = ['y', 'z', 'a', 'f', 'p', 'n', 'µ', 'm', '', 'k', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y'];
  const base = Math.floor(Math.log10(Math.abs(value)) / 3);
  const index = Math.min(Math.max(base + 8, 0), prefixes.length - 1);
  const scaled = value / Math.pow(10, (index - 8) * 3);

  return `${scaled.toFixed(decimals)}${prefixes[index]}`;
}

/**
 * 将数值范围格式化为人类可读的字符串
 *
 * @param min - 最小值
 * @param max - 最大值
 * @param decimals - 小数位数（默认 2）
 * @returns 范围字符串
 *
 * @example
 * formatRange(1, 10);      // "[1.00, 10.00]"
 * formatRange(0, 100, 0);  // "[0, 100]"
 */
export function formatRange(min: number, max: number, decimals: number = 2): string {
  return `[${formatNumber(min, decimals)}, ${formatNumber(max, decimals)}]`;
}

// ================================================================
// 角度格式化
// ================================================================

/**
 * 将弧度转换为度数
 *
 * @param radians - 弧度值
 * @param decimals - 小数位数（默认 2）
 * @returns 度数值字符串
 *
 * @example
 * formatDegrees(Math.PI);      // "180.00°"
 * formatDegrees(Math.PI / 2);   // "90.00°"
 */
export function formatDegrees(radians: number, decimals: number = 2): string {
  if (!isFinite(radians)) return 'NaN°';
  const degrees = radians * (180 / Math.PI);
  return `${degrees.toFixed(decimals)}°`;
}

/**
 * 将度数转换为带度分秒的字符串
 *
 * @param degrees - 度数值
 * @returns 度分秒字符串
 *
 * @example
 * formatDMS(45.5);        // "45°30'0\""
 * formatDMS(123.456);     // "123°27'21\""
 */
export function formatDMS(degrees: number): string {
  if (!isFinite(degrees)) return 'NaN°';

  const d = Math.floor(degrees);
  const mFloat = (degrees - d) * 60;
  const m = Math.floor(mFloat);
  const s = ((mFloat - m) * 60).toFixed(1);

  return `${d}°${m}'${s}"`;
}

// ================================================================
// ID 格式化
// ================================================================

/**
 * 格式化 ID 为简短形式
 *
 * @param id - 完整 ID
 * @param prefix - 前缀（默认 "ID"）
 * @param length - 显示长度（默认 8）
 * @returns 格式化的 ID
 *
 * @example
 * formatId('point_12345678');      // "point_12..."
 * formatId('user_abc123def', 'U'); // "U:abc1..."
 */
export function formatId(id: string | number, prefix = 'ID', length = 8): string {
  const str = String(id);
  if (str.length <= length) return str;
  return `${prefix}:${str.slice(0, length - 3)}...`;
}

/**
 * 格式化序号（带前导零）
 *
 * @param index - 序号（从 1 开始）
 * @param width - 总位数（默认 3）
 * @returns 格式化的序号
 *
 * @example
 * formatIndex(1);   // "001"
 * formatIndex(12);  // "012"
 * formatIndex(123); // "123"
 */
export function formatIndex(index: number, width: number = 3): string {
  return String(index).padStart(width, '0');
}

// ================================================================
// 快捷格式化函数
// ================================================================

/**
 * 格式化布尔值为可读字符串
 *
 * @param value - 布尔值
 * @param trueText - 真值显示文本（默认 "Yes"）
 * @param falseText - 假值显示文本（默认 "No"）
 * @returns 可读字符串
 */
export function formatBool(value: boolean, trueText = 'Yes', falseText = 'No'): string {
  return value ? trueText : falseText;
}

/**
 * 格式化枚举值为可读字符串
 *
 * @param value - 枚举值
 * @param labels - 值到标签的映射
 * @param fallback - 未匹配时的默认文本
 * @returns 可读字符串
 */
export function formatEnum<T extends string | number>(
  value: T,
  labels: Record<string, string>,
  fallback = 'Unknown',
): string {
  return labels[String(value)] ?? fallback;
}

// ================================================================
// 导出
// ================================================================

export default {
  // 数字
  formatNumber,
  formatThousands,
  formatCoordinate,
  formatCoordArray,
  formatScientific,
  formatSI,
  formatRange,

  // 时间
  formatTime,
  formatDate,
  formatDateTime,
  formatTimestamp,
  formatDuration,

  // 文件
  formatFileSize,
  formatBytes,

  // 百分比
  formatZoom,
  formatPercent,

  // 字符串
  truncate,
  capitalize,
  camelCase,
  pascalCase,
  snakeCase,

  // 角度
  formatDegrees,
  formatDMS,

  // ID
  formatId,
  formatIndex,

  // 布尔和枚举
  formatBool,
  formatEnum,
};
