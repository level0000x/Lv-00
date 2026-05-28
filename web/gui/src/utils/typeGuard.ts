/**
 * @module utils/typeGuard
 * @description 类型守卫与安全类型转换工具库
 *
 *              提供运行时类型验证和安全的类型转换函数，
 *              增强代码的类型安全性和健壮性。
 *
 *              功能特性：
 *              - 基础类型守卫：isString, isNumber, isBoolean, isArray, isObject
 *              - 几何类型守卫：isPoint, isSegment, isConstraint, isRegion
 *              - 安全类型转换：safeParse, safeGet, safeCast
 *              - 验证函数：validate, validateAll
 *              - 防御性编程工具：assert, require
 *
 *              【优化说明】v3.6.0
 *              - 统一的类型守卫接口，提高代码一致性
 *              - 完整的 JSDoc 注释，便于 IDE 智能提示
 *              - 优化的错误处理，减少不必要的异常捕获开销
 *
 * @since 3.6.0
 */

// ================================================================
// 基础类型守卫
// ================================================================

/**
 * 检查值是否为字符串
 * @param val - 待检查的值
 * @returns 是否为字符串
 */
export function isString(val: unknown): val is string {
  return typeof val === 'string';
}

/**
 * 检查值是否为数字（非 NaN）
 * @param val - 待检查的值
 * @returns 是否为有效数字
 */
export function isNumber(val: unknown): val is number {
  return typeof val === 'number' && !Number.isNaN(val);
}

/**
 * 检查值是否为布尔值
 * @param val - 待检查的值
 * @returns 是否为布尔值
 */
export function isBoolean(val: unknown): val is boolean {
  return typeof val === 'boolean';
}

/**
 * 检查值是否为数组
 * @param val - 待检查的值
 * @returns 是否为数组
 */
export function isArray(val: unknown): val is unknown[] {
  return Array.isArray(val);
}

/**
 * 检查值是否为非空对象（排除数组）
 * @param val - 待检查的值
 * @returns 是否为普通对象
 */
export function isObject(val: unknown): val is Record<string, unknown> {
  return typeof val === 'object' && val !== null && !Array.isArray(val);
}

/**
 * 检查值是否为 undefined 或 null
 * @param val - 待检查的值
 * @returns 值是否为 nullish
 */
export function isNullish(val: unknown): val is null | undefined {
  return val === null || val === undefined;
}

/**
 * 检查字符串是否为空（undefined、null、空字符串或仅空白字符）
 * @param val - 待检查的值
 * @returns 字符串是否为空
 */
export function isEmptyString(val: unknown): val is string {
  return isString(val) && val.trim().length === 0;
}

/**
 * 检查数组是否为空
 * @param arr - 待检查的数组
 * @returns 数组是否为空
 */
export function isEmptyArray<T>(arr: unknown): arr is T[] {
  return isArray(arr) && arr.length === 0;
}

// ================================================================
// 类型守卫工厂
// ================================================================

/**
 * 创建基于 key 的类型守卫
 * @param key - 要检查的属性名
 * @param guard - 属性值的类型守卫
 * @returns 复合类型守卫函数
 *
 * @example
 * const hasStringName = hasKeyOf('name', isString);
 * const point = { id: 1, name: 'A' };
 * if (hasStringName(point)) {
 *   console.log(point.name); // 推断为 string
 * }
 */
export function hasKeyOf<K extends string, T>(
  key: K,
  guard: (val: unknown) => val is T,
) {
  return (obj: unknown): obj is Record<K, T> => {
    return isObject(obj) && key in obj && guard(obj[key]);
  };
}

/**
 * 创建多个属性类型守卫
 * @param guards - 属性名到类型守卫的映射
 * @returns 多属性类型守卫函数
 *
 * @example
 * const isPointLike = hasProperties({
 *   x: isNumber,
 *   y: isNumber,
 * });
 * const point = { x: 1, y: 2, id: 1 };
 * if (isPointLike(point)) {
 *   console.log(point.x, point.y); // 推断为 number
 * }
 */
export function hasProperties<T extends Record<string, (val: unknown) => boolean>>(
  guards: T,
) {
  return (obj: unknown): obj is { [K in keyof T]: T[K] extends (val: unknown) => val is infer R ? R : never } => {
    if (!isObject(obj)) return false;
    for (const key in guards) {
      const guard = guards[key];
      if (guard && !guard(obj[key])) return false;
    }
    return true;
  };
}

// ================================================================
// 几何类型守卫
// ================================================================

/**
 * 检查对象是否为有效的 Point
 * @param val - 待检查的值
 * @returns 是否为 Point
 */
export function isPoint(val: unknown): val is { id: number; x: number; y: number } {
  if (!isObject(val)) return false;
  const { id, x, y } = val;
  return isNumber(id) && isNumber(x) && isNumber(y);
}

/**
 * 检查对象是否为有效的 Segment
 * @param val - 待检查的值
 * @returns 是否为 Segment
 */
export function isSegment(val: unknown): val is { p1: number; p2: number; id: number } {
  if (!isObject(val)) return false;
  const { p1, p2, id } = val;
  return isNumber(p1) && isNumber(p2) && isNumber(id);
}

/**
 * 检查对象是否为有效的 Constraint
 * @param val - 待检查的值
 * @returns 是否为 Constraint
 */
export function isConstraint(
  val: unknown,
): val is { id: number; type: string; args: number[] } {
  if (!isObject(val)) return false;
  const { id, type, args } = val;
  return isNumber(id) && isString(type) && isArray(args) && args.every(isNumber);
}

/**
 * 检查对象是否为有效的 Region
 * @param val - 待检查的值
 * @returns 是否为 Region
 */
export function isRegion(val: unknown): val is { id: number; points: unknown[] } {
  if (!isObject(val)) return false;
  const { id, points } = val;
  return isNumber(id) && isArray(points);
}

// ================================================================
// 安全类型转换
// ================================================================

/**
 * 安全解析 JSON（永远不会抛出异常）
 * @param json - JSON 字符串
 * @param fallback - 解析失败时的默认值
 * @returns 解析结果或默认值
 *
 * @example
 * const data = safeParse(jsonString, null);
 * if (data !== null) {
 *   // data 是解析后的对象
 * }
 */
export function safeParse<T>(json: string, fallback: T): T {
  try {
    const result = JSON.parse(json);
    return result as T;
  } catch {
    return fallback;
  }
}

/**
 * 安全获取对象属性（支持嵌套路径和默认值）
 * @param obj - 目标对象
 * @param path - 属性路径（如 'a.b.c'）
 * @param fallback - 属性不存在时的默认值
 * @returns 属性值或默认值
 *
 * @example
 * const name = safeGet(user, 'profile.name', 'Anonymous');
 * const value = safeGet(config, 'settings.theme.color', '#000000');
 */
export function safeGet<T>(
  obj: unknown,
  path: string,
  fallback: T,
): T {
  if (!isObject(obj)) return fallback;

  const keys = path.split('.');
  let current: unknown = obj;

  for (const key of keys) {
    if (!isObject(current)) return fallback;
    if (!(key in current)) return fallback;
    current = (current as Record<string, unknown>)[key];
  }

  return (current as T) ?? fallback;
}

/**
 * 类型断言函数，将 unknown 转换为目标类型
 * 仅在类型守卫验证通过时使用，否则抛出错误
 *
 * @param val - 待转换的值
 * @param guard - 类型守卫函数
 * @param errorMessage - 验证失败时的错误信息
 * @returns 转换后的值
 * @throws 如果类型验证失败
 *
 * @example
 * const point = assertType(unknownValue, isPoint, 'Invalid point');
 */
export function assertType<T>(
  val: unknown,
  guard: (val: unknown) => val is T,
  errorMessage = 'Type assertion failed',
): T {
  if (guard(val)) {
    return val;
  }
  throw new TypeError(errorMessage);
}

/**
 * 类型守卫式的类型断言
 * 如果验证失败，返回 null 而不是抛出异常
 *
 * @param val - 待转换的值
 * @param guard - 类型守卫函数
 * @returns 转换后的值或 null
 *
 * @example
 * const point = safeCast(unknownValue, isPoint);
 * if (point !== null) {
 *   // point 是有效的 Point
 * }
 */
export function safeCast<T>(
  val: unknown,
  guard: (val: unknown) => val is T,
): T | null {
  return guard(val) ? val : null;
}

/**
 * 类型守卫式的批量转换
 * 将数组中的每个元素通过类型守卫，返回有效的元素数组
 *
 * @param arr - 待转换的数组
 * @param guard - 类型守卫函数
 * @returns 过滤后的数组
 *
 * @example
 * const validPoints = filterCast(allData, isPoint);
 * // validPoints 中的所有元素都是有效的 Point
 */
export function filterCast<T>(
  arr: unknown[],
  guard: (val: unknown) => val is T,
): T[] {
  return arr.filter(guard);
}

// ================================================================
// 验证函数
// ================================================================

/**
 * 验证单个条件，返回验证结果
 * @param condition - 验证条件
 * @param message - 验证失败时的错误信息
 * @returns 是否通过验证
 *
 * @example
 * if (!validate(isNumber(x), 'x must be a number')) {
 *   return;
 * }
 */
export function validate(condition: boolean, message: string): condition is true {
  if (!condition) {
    console.warn(`[Lv-00 Validation] ${message}`);
  }
  return condition;
}

/**
 * 验证所有条件（短路求值，遇 false 停止）
 * @param conditions - 验证条件数组
 * @returns 是否全部通过验证
 *
 * @example
 * const isValid = validateAll(
 *   isNumber(x, 'x must be number'),
 *   x > 0, 'x must be positive',
 * );
 */
export function validateAll(...conditions: Array<[boolean, string]>): boolean {
  for (const [condition, message] of conditions) {
    if (!validate(condition, message)) {
      return false;
    }
  }
  return true;
}

/**
 * 创建验证函数工厂
 * @param validator - 验证函数
 * @returns 封装的验证函数
 *
 * @example
 * const isValidPoint = createValidator((p) => isPoint(p), 'Invalid point');
 * if (!isValidPoint(data)) {
 *   return;
 * }
 */
export function createValidator<T>(
  validator: (val: T) => boolean,
  errorMessage: string,
) {
  return (val: T): boolean => {
    const isValid = validator(val);
    if (!isValid) {
      console.warn(`[Lv-00 Validation] ${errorMessage}`);
    }
    return isValid;
  };
}

// ================================================================
// 防御性编程工具
// ================================================================

/**
 * 断言值不为 null 或 undefined
 * @param val - 待断言的值
 * @param message - 断言失败时的错误信息
 * @returns 值（如果非 nullish）
 * @throws 如果值为 nullish
 *
 * @example
 * const user = assertNotNull(userData, 'User data is required');
 * // user 的类型自动排除 null 和 undefined
 */
export function assertNotNull<T>(
  val: T | null | undefined,
  message = 'Value cannot be null or undefined',
): T {
  if (val === null || val === undefined) {
    throw new Error(`[Lv-00 Assertion] ${message}`);
  }
  return val;
}

/**
 * 断言条件成立
 * @param condition - 断言条件
 * @param message - 断言失败时的错误信息
 * @throws 如果条件不成立
 *
 * @example
 * assert(condition, 'Invalid state');
 */
export function assert(
  condition: boolean,
  message = 'Assertion failed',
): asserts condition {
  if (!condition) {
    throw new Error(`[Lv-00 Assertion] ${message}`);
  }
}

/**
 * 要求值满足条件，否则抛出错误
 * @param val - 待检查的值
 * @param predicate - 条件函数
 * @param message - 不满足条件时的错误信息
 * @returns 值（如果满足条件）
 * @throws 如果不满足条件
 *
 * @example
 * const positive = require(x, (v) => v > 0, 'x must be positive');
 */
export function require<T>(
  val: T,
  predicate: (val: T) => boolean,
  message = 'Requirement not met',
): T {
  if (!predicate(val)) {
    throw new Error(`[Lv-00 Requirement] ${message}`);
  }
  return val;
}

/**
 * 可恢复的断言，验证失败时返回错误信息而不是抛出异常
 * @param condition - 验证条件
 * @param message - 验证失败时的错误信息
 * @returns 错误信息或 null
 *
 * @example
 * const error = tryAssert(isNumber(x), 'x must be number');
 * if (error) {
 *   return error;
 * }
 */
export function tryAssert(
  condition: boolean,
  message: string,
): string | null {
  return condition ? null : `[Lv-00 Assertion] ${message}`;
}

// ================================================================
// 导出所有类型守卫和工具
// ================================================================

export default {
  // 基础类型守卫
  isString,
  isNumber,
  isBoolean,
  isArray,
  isObject,
  isNullish,
  isEmptyString,
  isEmptyArray,

  // 类型守卫工厂
  hasKeyOf,
  hasProperties,

  // 几何类型守卫
  isPoint,
  isSegment,
  isConstraint,
  isRegion,

  // 安全类型转换
  safeParse,
  safeGet,
  assertType,
  safeCast,
  filterCast,

  // 验证函数
  validate,
  validateAll,
  createValidator,

  // 防御性编程
  assertNotNull,
  assert,
  require,
  tryAssert,
};
