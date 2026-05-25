/**
 * @module utils/debounce
 * @description 防抖与节流工具函数。
 *              用于限制高频事件（如鼠标移动、窗口 resize、滚动）的触发频率。
 */

/**
 * 创建一个防抖版本的函数。
 *
 * 防抖函数会延迟执行，直到自上次调用起经过指定的等待时间。
 * 如果在等待时间内再次调用，则重置计时器。
 *
 * @param fn - 需要防抖的函数
 * @param wait - 延迟时间（毫秒）
 * @returns 防抖版本的函数
 *
 * @example
 * ```typescript
 * const handleResize = debounce(() => {
 *   console.log('窗口大小已改变！');
 * }, 200);
 * window.addEventListener('resize', handleResize);
 * ```
 */
export function debounce<T extends (...args: unknown[]) => void>(
  fn: T,
  wait: number,
): (...args: Parameters<T>) => void {
  if (typeof fn !== 'function') {
    console.error('[Lv-00] debounce: fn 不是函数类型');
    return () => {};
  }

  let timeoutId: ReturnType<typeof setTimeout> | null = null;

  return function (this: unknown, ...args: Parameters<T>) {
    if (timeoutId !== null) {
      clearTimeout(timeoutId);
    }
    timeoutId = setTimeout(() => {
      fn.apply(this, args);
      timeoutId = null;
    }, wait);
  };
}

/**
 * 创建一个节流版本的函数。
 *
 * 节流函数在指定时间间隔内最多执行一次，
 * 无论它被调用了多少次。最后一次未被执行的调用会保留并
 * 在间隔结束后执行（尾调用）。
 *
 * @param fn - 需要节流的函数
 * @param limit - 两次调用之间的最小间隔（毫秒）
 * @returns 节流版本的函数
 *
 * @example
 * ```typescript
 * const handleMouseMove = throttle((e: MouseEvent) => {
 *   console.log(e.clientX, e.clientY);
 * }, 16); // 约 60fps
 * canvas.addEventListener('mousemove', handleMouseMove);
 * ```
 */
export function throttle<T extends (...args: unknown[]) => void>(
  fn: T,
  limit: number,
): (...args: Parameters<T>) => void {
  if (typeof fn !== 'function') {
    console.error('[Lv-00] throttle: fn 不是函数类型');
    return () => {};
  }

  let inThrottle = false;
  let lastArgs: Parameters<T> | null = null;

  return function (this: unknown, ...args: Parameters<T>) {
    if (!inThrottle) {
      fn.apply(this, args);
      inThrottle = true;
      setTimeout(() => {
        inThrottle = false;
        // 执行被节流拦截的末尾调用
        if (lastArgs) {
          fn.apply(this, lastArgs);
          lastArgs = null;
        }
      }, limit);
    } else {
      lastArgs = args;
    }
  };
}
