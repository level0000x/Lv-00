/**
 * @module utils/idGenerator
 * @description 全局唯一 ID 生成器 / Global unique ID generator
 *
 * 替代 Date.now() 方案，避免同一毫秒内的 ID 冲突。
 * Uses a monotonically increasing counter to guarantee uniqueness.
 */

/** 全局递增计数器 / Global incrementing counter */
let _counter = 0;

/** 基准时间戳，用于混合时间信息 / Base timestamp for mixing time info */
const _baseTime = Date.now();

/**
 * 生成全局唯一 ID
 *
 * @description 返回一个全局唯一的数字 ID。由基准时间戳左移 20 位加上递增计数器组成，
 *              在同一进程内保证唯一性，且保留时间排序特性。
 *
 * @returns {number} 唯一 ID
 */
export function generateUniqueId(): number {
  _counter += 1;
  // 基准时间戳 << 20 | 计数器，确保唯一且大致有序
  return (_baseTime << 20) | (_counter & 0xFFFFF);
}

/**
 * 重置 ID 生成器（仅用于测试）
 *
 * @description 将计数器归零，一般仅在单元测试中使用。
 */
export function resetIdGenerator(): void {
  _counter = 0;
}

// === 内部别名（推荐新代码使用 generateUniqueId）
// 所有内部调用方逐步迁移至 generateUniqueId，当前保留别名确保兼容
// TODO(v3.3): 将所有 60+ 处 generateId() 调用替换为 generateUniqueId()
export const generateId = generateUniqueId;
