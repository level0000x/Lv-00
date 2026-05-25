/**
 * @module hooks/useExport
 * @description 几何数据导出逻辑 Hook。
 *              从 Layout.tsx 中提取导出功能，提供独立的
 *              导出几何数据为 JSON 文件的逻辑。
 *
 *              特性：
 *              - 安全的 DOM 操作（try-catch 包裹）
 *              - 组件卸载时自动清理（定时器、Blob URL、DOM 元素）
 *              - 防重复导出（isExporting 状态锁）
 *              - 导出后自动 Toast 通知和日志记录
 *              - 导出内容包含：点坐标、线段、区域、约束、视口状态
 *
 *              使用示例：
 *              ```typescript
 *              const { exportGeometry, isExporting } = useExport();
 *              ```
 */

import { useState, useRef, useEffect, useCallback } from 'react';
import { useAppStore } from '@/stores';
import { APP_VERSION, TOAST_DURATION_DEFAULT } from '@/utils/constants';

// ================================================================
// 常量
// ================================================================

/** 导出文件版本号 */
const EXPORT_VERSION = APP_VERSION;

/** 导出后 DOM 清理的延迟时间（毫秒），确保浏览器文件下载对话框完成 */
const EXPORT_CLEANUP_DELAY_MS = 500;

/** 导出文件的默认名称前缀 */
const EXPORT_FILE_NAME_PREFIX = 'lv00_export';

/** Toast 自动消失时间（毫秒） */
const TOAST_DURATION_MS = TOAST_DURATION_DEFAULT;

// ================================================================
// 类型定义
// ================================================================

/** 导出结果 */
export interface ExportResult {
  /** 是否导出成功 */
  success: boolean;
  /** 错误消息（仅在失败时） */
  error?: string;
  /** 导出文件名（仅在成功时） */
  fileName?: string;
}

// ================================================================
// Hook 实现
// ================================================================

/**
 * useExport - 几何数据导出逻辑 Hook
 *
 * 提供导出当前几何数据为 JSON 文件的功能。
 * 自动处理 DOM 清理、内存释放、错误恢复和超时管理。
 *
 * @returns 导出操作接口
 *   - exportGeometry: 执行导出的函数
 *   - isExporting: 是否正在导出中
 */
export function useExport() {
  const [isExporting, setIsExporting] = useState(false);

  /** 清理定时器引用 */
  const cleanupTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  /** 导出链接 DOM 元素引用 */
  const exportLinkRef = useRef<HTMLAnchorElement | null>(null);
  /** 导出 Blob URL 引用 */
  const exportObjectUrlRef = useRef<string | null>(null);
  /** 标记组件是否已卸载，防止卸载后执行状态更新 */
  const isMountedRef = useRef(true);

  /**
   * 组件卸载时的清理逻辑。
   * 确保在组件已卸载后不会发生内存泄漏或 DOM 操作错误。
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
   */
  const cleanupResources = useCallback(() => {
    // 清除未触发的定时器
    if (cleanupTimerRef.current !== null) {
      clearTimeout(cleanupTimerRef.current);
      cleanupTimerRef.current = null;
    }

    // 安全移除可能残留的导出链接 DOM 元素
    try {
      if (exportLinkRef.current?.parentNode) {
        exportLinkRef.current.parentNode.removeChild(exportLinkRef.current);
      }
    } catch {
      // DOM 元素可能已被移除，忽略错误
    }
    exportLinkRef.current = null;

    // 安全释放可能残留的 Blob URL
    try {
      if (exportObjectUrlRef.current) {
        URL.revokeObjectURL(exportObjectUrlRef.current);
      }
    } catch {
      // URL 可能已失效，忽略错误
    }
    exportObjectUrlRef.current = null;
  }, []);

  /**
   * 生成导出文件名。
   * 格式: lv00_export_YYYY-MM-DD-HH-mm-ss.json
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
   *
   * 导出内容包含：
   * - 版本号和时间戳
   * - 所有点坐标（精确到 4 位小数）
   * - 所有线段（端点引用）
   * - 所有区域定义（顶点 ID 列表）
   * - 所有约束（类型和参数）
   * - 当前视口状态（缩放、偏移）
   *
   * @returns ExportResult 导出结果对象
   */
  const exportGeometry = useCallback((): ExportResult => {
    // 防重复导出检查
    if (isExporting) {
      return { success: false, error: '导出正在进行中' };
    }

    setIsExporting(true);
    cleanupResources();

    try {
      const state = useAppStore.getState();

      // 构建导出数据
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

      // 生成 JSON 字符串并创建 Blob
      const jsonStr = JSON.stringify(exportData, null, 2);
      const blob = new Blob([jsonStr], {
        type: 'application/json;charset=utf-8',
      });
      const url = URL.createObjectURL(blob);
      exportObjectUrlRef.current = url;

      // 创建下载链接并触发点击
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
       */
      const delayedCleanup = () => {
        cleanupResources();
        if (isMountedRef.current) {
          setIsExporting(false);
        }
      };

      // 设置定时器延迟清理
      cleanupTimerRef.current = setTimeout(
        delayedCleanup,
        EXPORT_CLEANUP_DELAY_MS,
      );

      // 如果链接元素失焦（用户已触发下载），立即清理
      a.addEventListener('blur', () => {
        if (cleanupTimerRef.current !== null) {
          clearTimeout(cleanupTimerRef.current);
        }
        delayedCleanup();
      }, { once: true });

      // 成功 Toast 通知
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
