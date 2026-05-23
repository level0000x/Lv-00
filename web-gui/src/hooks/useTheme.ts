/**
 * @module hooks/useTheme
 * @description Theme management hook.
 *              Provides theme toggle functionality and persists
 *              the user's theme preference.
 *
 *              主题管理 Hook。
 *              提供主题切换功能，并持久化用户的主题偏好设置。
 */

import { useEffect, useCallback } from 'react';
import { useAppStore } from '@/stores';
import type { Theme } from '@/types';
import { THEME_STORAGE_KEY } from '@/utils/constants';

/**
 * useTheme - Theme management hook
 *            主题管理 Hook
 *
 * Features:
 * - Initializes theme from localStorage or system preference
 * - Toggles between dark and light themes
 * - Persists theme choice to localStorage
 * - Applies theme class to document body
 *
 * 特性：
 * - 从 localStorage 或系统偏好初始化主题
 * - 在深色和浅色主题之间切换
 * - 将主题选择持久化到 localStorage
 * - 将主题类应用到 document body
 *
 * @returns Object with current theme and toggle function
 *          包含当前主题和切换函数的对象
 */
export function useTheme(): {
  theme: Theme;
  toggleTheme: () => void;
  setTheme: (theme: Theme) => void;
} {
  const theme = useAppStore((s) => s.theme);
  const setThemeStore = useAppStore((s) => s.setTheme);

  // Initialize theme from localStorage or system preference
  // 从 localStorage 或系统偏好初始化主题
  useEffect(() => {
    try {
      const stored = localStorage.getItem(THEME_STORAGE_KEY) as Theme | null;
      if (stored === 'dark' || stored === 'light') {
        setThemeStore(stored);
      } else if (window.matchMedia('(prefers-color-scheme: light)').matches) {
        setThemeStore('light');
      }
    } catch {
      // localStorage may be unavailable (e.g. in sandboxed iframes)
      // localStorage 可能不可用（例如在沙盒 iframe 中）
    }
  }, [setThemeStore]);

  const setTheme = useCallback(
    (newTheme: Theme) => {
      setThemeStore(newTheme);
      try {
        localStorage.setItem(THEME_STORAGE_KEY, newTheme);
      } catch {
        // Silently fail if localStorage is not available
        // localStorage 不可用时静默失败
      }
    },
    [setThemeStore],
  );

  const toggleTheme = useCallback(() => {
    const newTheme = theme === 'dark' ? 'light' : 'dark';
    setTheme(newTheme);
  }, [theme, setTheme]);

  return { theme, toggleTheme, setTheme };
}
