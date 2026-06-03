/**
 * @module hooks/commonHooks
 * @description React 自定义 Hooks 工具库
 *
 *              提供常用的自定义 Hooks，统一开发模式，提高代码复用性。
 *
 *              功能特性：
 *              - useAsync: 异步操作状态管理
 *              - useToggle: 布尔状态切换
 *              - useCounter: 计数器
 *              - useDebouncedValue: 防抖值
 *              - useThrottledCallback: 节流回调
 *              - useLocalStorage: 本地存储
 *              - usePrevious: 上一个值
 *              - useDeepCompareEffect: 深度比较副作用
 *
 *              【优化说明】v3.6.0
 *              - 完整的 TypeScript 类型定义
 *              - 标准化的返回值接口
 *              - 完善的 JSDoc 注释
 *
 * @since 3.6.0
 */

import { useState, useEffect, useCallback, useRef } from 'react';

// ================================================================
// useAsync - 异步操作状态管理
// ================================================================

/**
 * 异步操作状态接口
 */
export interface AsyncState<T> {
  /** 数据 */
  data: T | null;
  /** 加载状态 */
  loading: boolean;
  /** 错误信息 */
  error: Error | null;
  /** 是否完成（无论成功或失败） */
  isComplete: boolean;
}

/**
 * useAsync - 管理异步操作的状态
 *
 * @param asyncFn - 异步函数
 * @param deps - 依赖数组
 * @returns 状态对象和操作方法
 *
 * @example
 * const { data, loading, error, execute, reset } = useAsync(
 *   () => fetch('/api/data').then(r => r.json()),
 *   []
 * );
 */
export function useAsync<T>(
  asyncFn: () => Promise<T>,
  deps: unknown[] = [],
): AsyncState<T> & {
  execute: () => Promise<void>;
  reset: () => void;
} {
  const [state, setState] = useState<AsyncState<T>>({
    data: null,
    loading: false,
    error: null,
    isComplete: false,
  });

  const mountedRef = useRef(true);

  const execute = useCallback(async () => {
    setState((prev) => ({ ...prev, loading: true, error: null }));
    try {
      const result = await asyncFn();
      if (mountedRef.current) {
        setState({
          data: result,
          loading: false,
          error: null,
          isComplete: true,
        });
      }
    } catch (error) {
      if (mountedRef.current) {
        setState({
          data: null,
          loading: false,
          error: error as Error,
          isComplete: true,
        });
      }
    }
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, deps);

  const reset = useCallback(() => {
    setState({
      data: null,
      loading: false,
      error: null,
      isComplete: false,
    });
  }, []);

  useEffect(() => {
    mountedRef.current = true;
    return () => {
      mountedRef.current = false;
    };
  }, []);

  return { ...state, execute, reset };
}

// ================================================================
// useToggle - 布尔状态切换
// ================================================================

/**
 * useToggle - 布尔状态切换 Hook
 *
 * @param initialValue - 初始值
 * @returns [状态, 切换函数, 重置函数]
 *
 * @example
 * const [isOpen, toggle, reset] = useToggle(false);
 */
export function useToggle(
  initialValue = false,
): [boolean, () => void, (value: boolean) => void] {
  const [value, setValue] = useState(initialValue);

  const toggle = useCallback(() => {
    setValue((prev) => !prev);
  }, []);

  const reset = useCallback((newValue?: boolean) => {
    setValue(newValue !== undefined ? newValue : initialValue);
  }, [initialValue]);

  return [value, toggle, reset];
}

// ================================================================
// useCounter - 计数器
// ================================================================

/**
 * useCounter - 计数器 Hook
 *
 * @param initialValue - 初始值
 * @param options - 配置选项
 * @returns 计数器和操作方法
 *
 * @example
 * const { count, increment, decrement, reset } = useCounter(0, {
 *   min: 0,
 *   max: 10,
 * });
 */
export function useCounter(
  initialValue = 0,
  options?: {
    min?: number;
    max?: number;
    step?: number;
  },
) {
  const { min = -Infinity, max = Infinity, step = 1 } = options || {};

  const [count, setCount] = useState(() => {
    const clamped = Math.max(min, Math.min(max, initialValue));
    return clamped;
  });

  const increment = useCallback(() => {
    setCount((prev) => {
      const next = prev + step;
      return Math.min(max, next);
    });
  }, [max, step]);

  const decrement = useCallback(() => {
    setCount((prev) => {
      const next = prev - step;
      return Math.max(min, next);
    });
  }, [min, step]);

  const reset = useCallback(() => {
    setCount(Math.max(min, Math.min(max, initialValue)));
  }, [initialValue, min, max]);

  const set = useCallback(
    (value: number | ((prev: number) => number)) => {
      setCount((prev) => {
        const next = typeof value === 'function' ? (value as (prev: number) => number)(prev) : value;
        return Math.max(min, Math.min(max, next));
      });
    },
    [min, max],
  );

  return { count, increment, decrement, reset, set };
}

// ================================================================
// useDebouncedValue - 防抖值
// ================================================================

/**
 * useDebouncedValue - 防抖值 Hook
 *
 * @param value - 原始值
 * @param delay - 延迟时间（毫秒）
 * @returns 防抖后的值
 *
 * @example
 * const [query, setQuery] = useState('');
 * const debouncedQuery = useDebouncedValue(query, 300);
 */
export function useDebouncedValue<T>(value: T, delay: number): T {
  const [debouncedValue, setDebouncedValue] = useState(value);

  useEffect(() => {
    const timer = setTimeout(() => {
      setDebouncedValue(value);
    }, delay);

    return () => {
      clearTimeout(timer);
    };
  }, [value, delay]);

  return debouncedValue;
}

// ================================================================
// useThrottledCallback - 节流回调
// ================================================================

/**
 * useThrottledCallback - 节流回调 Hook
 *
 * @param callback - 回调函数
 * @param delay - 延迟时间（毫秒）
 * @returns 节流后的回调函数
 *
 * @example
 * const throttledScroll = useThrottledCallback(
 *   () => console.log('scrolling'),
 *   100
 * );
 */
export function useThrottledCallback<T extends (...args: unknown[]) => unknown>(
  callback: T,
  delay: number,
): T {
  const lastCallRef = useRef<number>(0);
  const timeoutRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  const throttled = useCallback(
    (...args: Parameters<T>) => {
      const now = Date.now();
      const timeSinceLastCall = now - lastCallRef.current;

      if (timeSinceLastCall >= delay) {
        lastCallRef.current = now;
        callback(...args);
      } else {
        // 最后一次调用仍然需要执行
        if (timeoutRef.current) {
          clearTimeout(timeoutRef.current);
        }
        timeoutRef.current = setTimeout(() => {
          lastCallRef.current = Date.now();
          callback(...args);
        }, delay - timeSinceLastCall);
      }
    },
    [callback, delay],
  ) as T;

  useEffect(() => {
    return () => {
      if (timeoutRef.current) {
        clearTimeout(timeoutRef.current);
      }
    };
  }, []);

  return throttled;
}

// ================================================================
// useLocalStorage - 本地存储
// ================================================================

/**
 * useLocalStorage - 本地存储 Hook
 *
 * @param key - 存储键名
 * @param initialValue - 初始值
 * @returns [值, 设置函数, 删除函数]
 *
 * @example
 * const [theme, setTheme, clearTheme] = useLocalStorage('theme', 'dark');
 */
export function useLocalStorage<T>(
  key: string,
  initialValue: T,
): [T, (value: T) => void, () => void] {
  const [storedValue, setStoredValue] = useState<T>(() => {
    try {
      const item = window.localStorage.getItem(key);
      return item ? (JSON.parse(item) as T) : initialValue;
    } catch {
      return initialValue;
    }
  });

  const setValue = useCallback(
    (value: T) => {
      try {
        setStoredValue(value);
        window.localStorage.setItem(key, JSON.stringify(value));
      } catch (error) {
        console.error(`[Lv-00 useLocalStorage] Error setting ${key}:`, error);
      }
    },
    [key],
  );

  const removeValue = useCallback(() => {
    try {
      window.localStorage.removeItem(key);
      setStoredValue(initialValue);
    } catch (error) {
      console.error(`[Lv-00 useLocalStorage] Error removing ${key}:`, error);
    }
  }, [key, initialValue]);

  return [storedValue, setValue, removeValue];
}

// ================================================================
// usePrevious - 上一个值
// ================================================================

/**
 * usePrevious - 获取上一个渲染周期的值
 *
 * @param value - 当前值
 * @returns 上一个值
 *
 * @example
 * const [count, setCount] = useState(0);
 * const previousCount = usePrevious(count);
 * // count: 1, previousCount: 0
 */
export function usePrevious<T>(value: T): T | undefined {
  const ref = useRef<T>();

  useEffect(() => {
    ref.current = value;
  }, [value]);

  return ref.current;
}

// ================================================================
// useDeepCompareEffect - 深度比较副作用
// ================================================================

/**
 * useDeepCompareEffect - 深度比较依赖的副作用
 *
 * @param effect - 副作用函数
 * @param deps - 依赖数组（会进行深度比较）
 *
 * @example
 * useDeepCompareEffect(() => {
 *   // 依赖对象变化时执行
 * }, [config]);
 */
export function useDeepCompareEffect(
  effect: React.EffectCallback,
  deps: unknown[],
): void {
  const ref = useRef<unknown[] | undefined>(undefined);

  if (!ref.current) {
    ref.current = deps;
  }

  const prevDeps = ref.current;

  const isChanged = !Object.is(deps, prevDeps);

  useEffect(() => {
    if (isChanged) {
      ref.current = deps;
    }
    return effect();
  }, [isChanged, effect]);
}

// ================================================================
// useUpdateEffect - 更新时副作用（首次不执行）
// ================================================================

/**
 * useUpdateEffect - 仅在依赖更新时执行的副作用
 *
 * @param effect - 副作用函数
 * @param deps - 依赖数组
 *
 * @example
 * useUpdateEffect(() => {
 *   console.log('count changed:', count);
 * }, [count]);
 */
export function useUpdateEffect(
  effect: React.EffectCallback,
  deps: unknown[],
): void {
  const isFirstMount = useRef(true);

  useEffect(() => {
    if (isFirstMount.current) {
      isFirstMount.current = false;
      return;
    }
    return effect();
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, deps);
}

// ================================================================
// useMountEffect - 挂载时副作用（仅执行一次）
// ================================================================

/**
 * useMountEffect - 仅在组件挂载时执行一次的副作用
 *
 * @param effect - 副作用函数（可返回清理函数）
 *
 * @example
 * useMountEffect(() => {
 *   console.log('mounted');
 *   return () => console.log('unmounted');
 * });
 */
export function useMountEffect(effect: React.EffectCallback): void {
  const isFirstMount = useRef(true);

  useEffect(() => {
    if (isFirstMount.current) {
      isFirstMount.current = false;
      return effect();
    }
    return undefined;
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);
}

// ================================================================
// useClickOutside - 点击外部
// ================================================================

/**
 * useClickOutside - 检测点击元素外部
 *
 * @param ref - 目标元素引用
 * @param handler - 点击外部时的回调
 *
 * @example
 * const ref = useRef<HTMLDivElement>(null);
 * useClickOutside(ref, () => setIsOpen(false));
 */
export function useClickOutside<T extends HTMLElement>(
  ref: React.RefObject<T>,
  handler: (event: MouseEvent | TouchEvent) => void,
): void {
  useEffect(() => {
    const listener = (event: MouseEvent | TouchEvent) => {
      if (!ref.current || ref.current.contains(event.target as Node)) {
        return;
      }
      handler(event);
    };

    document.addEventListener('mousedown', listener);
    document.addEventListener('touchstart', listener);

    return () => {
      document.removeEventListener('mousedown', listener);
      document.removeEventListener('touchstart', listener);
    };
  }, [ref, handler]);
}

// ================================================================
// useWindowSize - 窗口尺寸
// ================================================================

/**
 * useWindowSize - 获取窗口尺寸
 *
 * @returns 窗口尺寸对象
 *
 * @example
 * const { width, height } = useWindowSize();
 */
export function useWindowSize(): { width: number; height: number } {
  const [size, setSize] = useState({
    width: window.innerWidth,
    height: window.innerHeight,
  });

  useEffect(() => {
    const handleResize = () => {
      setSize({
        width: window.innerWidth,
        height: window.innerHeight,
      });
    };

    window.addEventListener('resize', handleResize);
    return () => window.removeEventListener('resize', handleResize);
  }, []);

  return size;
}

// ================================================================
// useElementSize - 元素尺寸
// ================================================================

/**
 * useElementSize - 获取元素尺寸
 *
 * @param ref - 目标元素引用
 * @returns 元素尺寸对象
 *
 * @example
 * const ref = useRef<HTMLDivElement>(null);
 * const { width, height } = useElementSize(ref);
 */
export function useElementSize<T extends HTMLElement>(
  ref: React.RefObject<T>,
): { width: number; height: number } {
  const [size, setSize] = useState({ width: 0, height: 0 });

  useEffect(() => {
    const element = ref.current;
    if (!element) return;

    const observer = new ResizeObserver((entries) => {
      for (const entry of entries) {
        const { width, height } = entry.contentRect;
        setSize({ width, height });
      }
    });

    observer.observe(element);
    return () => observer.disconnect();
  }, [ref]);

  return size;
}

// ================================================================
// useMemo - 记忆化计算
// ================================================================

/**
 * useMemoizedFn - 记忆化回调函数
 * 保持函数引用稳定，避免不必要的重新渲染
 *
 * @param fn - 需要记忆化的函数
 * @returns 记忆化后的函数
 *
 * @example
 * const memoizedFn = useMemoizedFn((data) => {
 *   return processData(data);
 * });
 */
export function useMemoizedFn<T extends (...args: unknown[]) => unknown>(
  fn: T,
): T {
  const fnRef = useRef(fn);

  // 更新 ref.current 到最新的函数
  fnRef.current = fn;

  // 返回一个稳定的回调函数
  const memoized = useCallback(
    (...args: Parameters<T>) => fnRef.current(...args),
    [],
  ) as T;

  return memoized;
}

// ================================================================
// useSafeState - 安全状态（卸载后不更新）
// ================================================================

/**
 * useSafeState - 安全状态 Hook
 * 确保组件卸载后不会更新状态，避免内存泄漏警告
 *
 * @param initialValue - 初始值
 * @returns [状态, 设置函数]
 *
 * @example
 * const [data, setData] = useSafeState(null);
 */
export function useSafeState<T>(
  initialValue: T,
): [T, React.Dispatch<React.SetStateAction<T>>] {
  const mountedRef = useRef(true);
  const [state, setState] = useState(initialValue);

  useEffect(() => {
    mountedRef.current = true;
    return () => {
      mountedRef.current = false;
    };
  }, []);

  const safeSetState = useCallback(
    (value: T | ((prev: T) => T)) => {
      if (mountedRef.current) {
        setState(value);
      }
    },
    [],
  );

  return [state, safeSetState];
}

// ================================================================
// 导出所有 Hooks
// ================================================================

export default {
  useAsync,
  useToggle,
  useCounter,
  useDebouncedValue,
  useThrottledCallback,
  useLocalStorage,
  usePrevious,
  useDeepCompareEffect,
  useUpdateEffect,
  useMountEffect,
  useClickOutside,
  useWindowSize,
  useElementSize,
  useMemoizedFn,
  useSafeState,
};
