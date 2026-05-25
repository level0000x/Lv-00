/**
 * @module hooks/useDebounce
 * @description 防抖 Hook —— 对快速变化的值进行延迟更新，
 *              常用于搜索输入、窗口 resize 等高频场景。
 *
 *              原理：在 delay 毫秒内如果没有新值到达，才将当前值同步到 debouncedValue。
 *              每次值变化都会重置计时器，从而避免中间状态触发不必要的副作用。
 */

import { useState, useEffect } from 'react';

/**
 * useDebounce - 防抖 Hook
 *
 * 对 value 进行防抖处理，返回在指定延迟后稳定下来的值。
 * 典型用途：搜索框输入防抖，减少不必要的过滤计算或 API 请求。
 *
 * @param value - 需要防抖的原始值（通常来自输入框 onChange）
 * @param delay - 防抖延迟（毫秒），默认 300ms
 * @returns 防抖后的稳定值
 *
 * @example
 * ```tsx
 * const [query, setQuery] = useState('');
 * const debouncedQuery = useDebounce(query, 300);
 *
 * useEffect(() => {
 *   // 仅在用户停止输入 300ms 后执行搜索
 *   performSearch(debouncedQuery);
 * }, [debouncedQuery]);
 * ```
 */
export function useDebounce<T>(value: T, delay: number = 300): T {
  const [debouncedValue, setDebouncedValue] = useState<T>(value);

  useEffect(() => {
    // 每次 value 变化时重置计时器
    const timer = setTimeout(() => {
      setDebouncedValue(value);
    }, delay);

    // value 或 delay 变化时清除上一次的计时器
    return () => clearTimeout(timer);
  }, [value, delay]);

  return debouncedValue;
}

export default useDebounce;
