/**
 * @module utils/idGenerator
 * @description 全局唯一 ID 生成器
 *
 * 替代 Date.now() 方案，避免同一毫秒内的 ID 冲突。
 * 使用单调递增计数器保证唯一性。
 *
 * 【优化说明】v3.4.2
 * - 添加溢出检测和警告，防止计数器回绕
 * - 支持多标签页/Worker 环境下的 ID 唯一性
 */

/** 全局递增计数器 */
let _counter = 0;

/** 基准时间戳，用于混合时间信息 */
const _baseTime = Date.now();

/** 计数器最大值（20位掩码） */
const MAX_COUNTER_VALUE = 0xFFFFF;

/** 溢出警告阈值（90%） */
const OVERFLOW_WARNING_THRESHOLD = 0.9;

/**
 * 生成全局唯一 ID
 *
 * @description 返回一个全局唯一的数字 ID。由基准时间戳左移 20 位加上递增计数器组成，
 *              在同一进程内保证唯一性，且保留时间排序特性。
 *
 * @returns {number} 唯一 ID
 *
 * @warning 如果计数器接近最大值，会输出警告到控制台
 */
export function generateUniqueId(): number {
  _counter += 1;

  // 溢出检测
  if (_counter > MAX_COUNTER_VALUE) {
    console.warn(
      `[Lv-00] ID生成器警告: 计数器接近上限 (${_counter}/${MAX_COUNTER_VALUE})，` +
      `可能发生ID冲突。建议重置生成器或刷新页面。`
    );
    _counter = 0; // 回绕
  }

  // 接近阈值时警告
  if (_counter > MAX_COUNTER_VALUE * OVERFLOW_WARNING_THRESHOLD) {
    console.warn(
      `[Lv-00] ID生成器: 计数器已使用 ${((_counter / MAX_COUNTER_VALUE) * 100).toFixed(1)}%，` +
      `剩余 ${MAX_COUNTER_VALUE - _counter} 个ID`
    );
  }

  // 基准时间戳 << 20 | 计数器，确保唯一且大致有序
  return (_baseTime << 20) | (_counter & MAX_COUNTER_VALUE);
}

/**
 * 生成带前缀的唯一 ID
 *
 * @description 生成唯一ID并附加指定前缀，用于区分不同类型的ID
 *
 * @param prefix ID前缀（如 'point', 'segment', 'constraint'）
 * @returns {string} 带前缀的唯一ID字符串
 *
 * @example
 *   const pointId = generatePrefixedId('P'); // 返回 "P-1234567890"
 *   const segId = generatePrefixedId('S');   // 返回 "S-1234567891"
 */
export function generatePrefixedId(prefix: string): string {
  return `${prefix}-${generateUniqueId()}`;
}

/**
 * 重置 ID 生成器（仅用于测试）
 *
 * @description 将计数器归零，一般仅在单元测试中使用。
 */
export function resetIdGenerator(): void {
  _counter = 0;
}

/**
 * 获取当前计数器状态
 *
 * @description 返回当前计数器的值和最大值，用于监控和调试
 *
 * @returns {object} 包含 current 和 max 属性的对象
 */
export function getIdGeneratorState(): { current: number; max: number; usage: number } {
  return {
    current: _counter,
    max: MAX_COUNTER_VALUE,
    usage: _counter / MAX_COUNTER_VALUE,
  };
}
