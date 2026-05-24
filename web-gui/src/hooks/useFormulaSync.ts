/**
 * @module hooks/useFormulaSync
 * @description 公式双向同步 Hook。
 *              实现画布几何与公式 DSL 文本之间的自动双向同步。
 *
 * 功能特性：
 * - 画布 -> 公式：当画布几何元素变化时自动生成 DSL
 * - 公式 -> 画布：通过外部执行函数实现
 * - 同步状态跟踪（idle / syncing / synced）
 * - 防止公式执行时的死循环
 */

import { useState, useCallback, useEffect, useRef } from 'react';
import { useAppStore } from '@/stores';
import { generateDSLFromGeometry } from '@/utils/formulaParser';
import type { Point, Segment, Constraint } from '@/types';

/**
 * useFormulaSync - 公式双向同步 Hook
 *
 * @returns 同步相关的状态和操作方法
 */
export function useFormulaSync() {
  /** 同步开关状态 */
  const [syncEnabled, setSyncEnabled] = useState<boolean>(false);
  /** 同步状态指示器 */
  const [syncStatus, setSyncStatus] = useState<'idle' | 'syncing' | 'synced'>('idle');

  /** 上次同步的公式文本，用于去重 */
  const lastSyncedFormulaRef = useRef<string>('');
  /** 公式是否正在执行中，防止同步死循环 */
  const isExecutingFormula = useRef(false);

  // Store 选择器
  const setFormulaInput = useAppStore((s) => s.setFormulaInput);
  const points = useAppStore((s) => s.points);
  const segments = useAppStore((s) => s.segments);
  const constraints = useAppStore((s) => s.constraints);

  /**
   * 切换同步开关。
   * 首次开启时立即从画布同步一次。
   */
  const handleToggleSync = useCallback(() => {
    setSyncEnabled((prev) => {
      const next = !prev;
      if (next && points.length > 0) {
        // 首次开启时立即从 canvas 同步一次
        const dsl = generateDSLFromGeometry(points, segments, constraints);
        lastSyncedFormulaRef.current = dsl;
        setFormulaInput(dsl);
        setSyncStatus('synced');
      } else if (!next) {
        setSyncStatus('idle');
      }
      return next;
    });
  }, [points, segments, constraints, setFormulaInput]);

  /**
   * 当 syncEnabled 且 points/segments/constraints 变化时自动同步。
   * 公式正在执行中时跳过同步以避免死循环。
   */
  useEffect(() => {
    if (!syncEnabled) return;
    if (points.length === 0) return;
    // 公式正在执行中（来自 handleRender/handleFormulaToGraph），跳过同步避免死循环
    if (isExecutingFormula.current) return;
    const dsl = generateDSLFromGeometry(points, segments, constraints);
    if (dsl === lastSyncedFormulaRef.current) return; // 无变化则跳过
    lastSyncedFormulaRef.current = dsl;
    setSyncStatus('syncing');
    const t = setTimeout(() => {
      setFormulaInput(dsl);
      setSyncStatus('synced');
    }, 150);
    return () => clearTimeout(t);
  }, [syncEnabled, points, segments, constraints, setFormulaInput]);

  return {
    syncEnabled,
    syncStatus,
    handleToggleSync,
    isExecutingFormula,
  };
}

export default useFormulaSync;
