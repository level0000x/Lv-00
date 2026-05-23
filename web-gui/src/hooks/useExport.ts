/**
 * @module hooks/useExport
 * @description 几何数据导出逻辑 Hook。
 *              从 Layout.tsx 中提取导出功能，提供独立的
 *              导出几何数据为 JSON 文件的逻辑。
 *
 *              Geometry data export logic hook.
 *              Extracts export functionality from Layout.tsx, providing
 *              standalone logic for exporting geometry data to JSON files.
 *
 *              特性 / Features：
 *              - 安全的 DOM 操作（try-catch 包裹）
 *                Safe DOM operations (try-catch wrapped)
 *              - 组件卸载时自动清理（定时器、Blob URL、DOM 元素）
 *                Automatic cleanup on unmount (timers, Blob URLs, DOM elements)
 *              - 防重复导出（isExporting 状态锁）
 *                Duplicate export prevention (isExporting state lock)
 *              - 导出后自动 Toast 通知和日志记录
 *                Automatic toast notification and logging after export
 *              - 导出内容包含：点坐标、线段、区域、约束、视口状态
 *                Export content includes: point coordinates, segments, regions, constraints, viewport state
 *
 *              使用 / Usage：
 *              ```typescript
 *              const { exportGeometry, isExporting } = useExport();
 *              ```
 */

import { useState, useRef, useEffect, useCallback } from 'react';
import { useAppStore } from '@/stores';
import { APP_VERSION, TOAST_DURATION_DEFAULT } from '@/utils/constants';

// ================================================================
// 常量 / Constants
// ================================================================

/** 导出文件版本号 / Export file version */
const EXPORT_VERSION = APP_VERSION;

/** 导出后 DOM 清理的延迟时间（毫秒），确保浏览器文件下载对话框完成 */
/** Delay for DOM cleanup after export (ms), ensuring browser download dialog completes */
const EXPORT_CLEANUP_DELAY_MS = 500;

/** 导出文件的默认名称前缀 / Default export file name prefix */
const EXPORT_FILE_NAME_PREFIX = 'lv00_export';

/** Toast 自动消失时间（毫秒） / Toast auto-dismiss duration (ms) */
const TOAST_DURATION_MS = TOAST_DURATION_DEFAULT;

// ================================================================
// 类型定义 / Type Definitions
// ================================================================

/** 导出结果 / Export Result */
export interface ExportResult {
  /** 是否导出成功 / Whether the export succeeded */
  success: boolean;
  /** 错误消息（仅在失败时）/ Error message (only on failure) */
  error?: string;
  /** 导出文件名（仅在成功时）/ Export file name (only on success) */
  fileName?: string;
}

// ================================================================
// Hook 实现 / Hook Implementation
// ================================================================

/**
 * useExport - 几何数据导出逻辑 Hook
 *            Geometry data export logic hook
 *
 * 提供导出当前几何数据为 JSON 文件的功能。
 * 自动处理 DOM 清理、内存释放、错误恢复和超时管理。
 *
 * Provides functionality to export current geometry data as a JSON file.
 * Automatically handles DOM cleanup, memory release, error recovery, and timeout management.
 *
 * @returns 导出操作接口 / Export operation interface
 *   - exportGeometry: 执行导出的函数 / Function to perform export
 *   - isExporting: 是否正在导出中 / Whether an export is in progress
 */
export function useExport() {
  const [isExporting, setIsExporting] = useState(false);

  /** 清理定时器引用 / Cleanup timer reference */
  const cleanupTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  /** 导出链接 DOM 元素引用 / Export link DOM element reference */
  const exportLinkRef = useRef<HTMLAnchorElement | null>(null);
  /** 导出 Blob URL 引用 / Export Blob URL reference */
  const exportObjectUrlRef = useRef<string | null>(null);
  /** 标记组件是否已卸载，防止卸载后执行状态更新 */
  /** Flag indicating whether the component is unmounted, preventing state updates after unmount */
  const isMountedRef = useRef(true);

  /**
   * 组件卸载时的清理逻辑。
   * 确保在组件已卸载后不会发生内存泄漏或 DOM 操作错误。
   *
   * Cleanup logic on component unmount.
   * Ensures no memory leaks or DOM operation errors after the component has unmounted.
   */
  useEffect(() => {
    isMountedRef.current = true;
    return () => {
      isMountedRef.current = false;
      cleanupResources();
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  /**
   * 清理所有导出相关的资源。
   * Clean up all export-related resources.
   */
  const cleanupResources = useCallback(() => {
    // 清除未触发的定时器 / Clear pending timers
    if (cleanupTimerRef.current !== null) {
      clearTimeout(cleanupTimerRef.current);
      cleanupTimerRef.current = null;
    }

    // 安全移除可能残留的导出链接 DOM 元素
    // Safely remove any remaining export link DOM elements
    try {
      if (exportLinkRef.current?.parentNode) {
        exportLinkRef.current.parentNode.removeChild(exportLinkRef.current);
      }
    } catch {
      // DOM 元素可能已被移除，忽略错误
      // DOM element may have been removed, ignore error
    }
    exportLinkRef.current = null;

    // 安全释放可能残留的 Blob URL
    // Safely release any remaining Blob URLs
    try {
      if (exportObjectUrlRef.current) {
        URL.revokeObjectURL(exportObjectUrlRef.current);
      }
    } catch {
      // URL 可能已失效，忽略错误
      // URL may have expired, ignore error
    }
    exportObjectUrlRef.current = null;
  }, []);

  /**
   * 生成导出文件名。
   * 格式: lv00_export_YYYY-MM-DD-HH-mm-ss.json
   *
   * Generate export file name.
   * Format: lv00_export_YYYY-MM-DD-HH-mm-ss.json
   */
  const generateFileName = useCallback((): string => {
    const timestamp = new Date()
      .toISOString()
      .slice(0, 19)
      .replace(/[T:]/g, '-');
    return `${EXPORT_FILE_NAME_PREFIX}_${timestamp}.json`;
  }, []);

  /**
   * 导出当前几何数据为 JSON 文件。
   * Export current geometry data as a JSON file.
   *
   * 导出内容包含 / Export content includes：
   * - 版本号和时间戳 / Version number and timestamp
   * - 所有点坐标（精确到 4 位小数）/ All point coordinates (rounded to 4 decimal places)
   * - 所有线段（端点引用）/ All segments (endpoint references)
   * - 所有区域定义（顶点 ID 列表）/ All region definitions (vertex ID lists)
   * - 所有约束（类型和参数）/ All constraints (type and parameters)
   * - 当前视口状态（缩放、偏移）/ Current viewport state (scale, offset)
   *
   * @returns ExportResult 导出结果对象 / Export result object
   */
  const exportGeometry = useCallback((): ExportResult => {
    // 防重复导出检查 / Duplicate export prevention check
    if (isExporting) {
      return { success: false, error: '导出正在进行中' };
    }

    setIsExporting(true);
    cleanupResources();

    try {
      const state = useAppStore.getState();

      // 构建导出数据 / Build export data
      const exportData = {
        version: EXPORT_VERSION,
        timestamp: new Date().toISOString(),
        points: state.points.map((p) => ({
          id: p.id,
          x: Number(p.x.toFixed(4)),
          y: Number(p.y.toFixed(4)),
        })),
        segments: state.segments.map((s) => ({
          p1: s.p1,
          p2: s.p2,
          id: s.id,
        })),
        regions: state.regions.map((r) => ({
          id: r.id,
          points: r.points.map((p) => p.id),
        })),
        constraints: state.constraints,
        viewport: {
          scale: state.scale,
          offsetX: state.offsetX,
          offsetY: state.offsetY,
        },
      };

      // 生成 JSON 字符串并创建 Blob / Generate JSON string and create Blob
      const jsonStr = JSON.stringify(exportData, null, 2);
      const blob = new Blob([jsonStr], {
        type: 'application/json;charset=utf-8',
      });
      const url = URL.createObjectURL(blob);
      exportObjectUrlRef.current = url;

      // 创建下载链接并触发点击 / Create download link and trigger click
      const fileName = generateFileName();
      const a = document.createElement('a');
      a.href = url;
      a.download = fileName;
      document.body.appendChild(a);
      exportLinkRef.current = a;
      a.click();

      /**
       * 延迟清理函数。
       * 确保浏览器文件下载对话框有足够时间完成。
       *
       * Delayed cleanup function.
       * Ensures the browser file download dialog has enough time to complete.
       */
      const delayedCleanup = () => {
        cleanupResources();
        if (isMountedRef.current) {
          setIsExporting(false);
        }
      };

      // 设置定时器延迟清理 / Set timer for delayed cleanup
      cleanupTimerRef.current = setTimeout(
        delayedCleanup,
        EXPORT_CLEANUP_DELAY_MS,
      );

      // 如果链接元素失焦（用户已触发下载），立即清理
      // If the link element loses focus (user has triggered download), clean up immediately
      a.addEventListener('blur', () => {
        if (cleanupTimerRef.current !== null) {
          clearTimeout(cleanupTimerRef.current);
        }
        delayedCleanup();
      }, { once: true });

      // 成功 Toast 通知 / Success toast notification
      useAppStore.getState().addToast(
        'success',
        `导出成功: ${fileName} 已下载`,
        TOAST_DURATION_MS,
      );

      return { success: true, fileName };
    } catch (e) {
      const errorMsg = `导出失败: ${(e as Error).message}`;
      if (isMountedRef.current) {
        useAppStore.getState().addToast('error', errorMsg, TOAST_DURATION_MS);
        useAppStore.getState().appendLog(errorMsg, 'error');
        setIsExporting(false);
      }
      return { success: false, error: errorMsg };
    }
  }, [isExporting, cleanupResources, generateFileName]);

  return { exportGeometry, isExporting };
}
