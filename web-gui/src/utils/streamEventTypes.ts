/**
 * @module utils/streamEventTypes
 * @description 流式事件类型定义 / Stream event type definitions
 *
 * *** DEPRECATED (2026-05-24) ***
 * 本文件已被 @/types/index.ts 中的 EngineEventType 枚举和
 * EngineStreamCategory 类型取代。
 *
 * 旧版 6 类别体系（info/step/result/error/warning/debug）已迁移为
 * 与 C 核心 stream.h 对齐的 8 类别体系（engine/normalize/rewrite/solve/proof/func_block/conflict/info）。
 *
 * 请使用以下替代方案：
 * - 事件类型枚举：import { EngineEventType } from '@/types'
 * - 事件类别：import type { EngineStreamCategory } from '@/types'
 * - 事件结构：import type { EngineStreamEvent } from '@/types'
 * - 类别映射：import { getEventCategory } from '@/types'
 *
 * 本文件保留仅为向后兼容，新代码请勿使用。
 */

/** 流式事件类别 / Stream event category */
export type StreamEventCategory = 'info' | 'step' | 'result' | 'error' | 'warning' | 'debug';

/** 流式事件类型枚举 / Stream event type enum */
export enum StreamEventType {
  /** 引擎启动 / Engine started */
  ENGINE_START = 0,
  /** 求解开始 / Solve started */
  SOLVE_START = 1,
  /** 求解步骤 / Solve step */
  SOLVE_STEP = 2,
  /** 求解完成 / Solve completed */
  SOLVE_DONE = 3,
  /** 约束添加 / Constraint added */
  CONSTRAINT_ADD = 4,
  /** 约束满足 / Constraint satisfied */
  CONSTRAINT_SAT = 5,
  /** 约束冲突 / Constraint conflict */
  CONSTRAINT_FAIL = 6,
  /** 归一化开始 / Normalization started */
  NORM_START = 7,
  /** 归一化完成 / Normalization completed */
  NORM_DONE = 8,
  /** 证明开始 / Proof started */
  PROOF_START = 9,
  /** 证明步骤 / Proof step */
  PROOF_STEP = 10,
  /** 证明完成 / Proof completed */
  PROOF_DONE = 11,
  /** 错误 / Error */
  ERROR = 12,
  /** 警告 / Warning */
  WARNING = 13,
  /** 调试信息 / Debug info */
  DEBUG = 14,
  /** 系统信息 / System info */
  SYSTEM = 15,
  /** 流开始 / Stream started */
  STREAM_START = 16,
  /** 流结束 / Stream ended */
  STREAM_END = 17,
}

/**
 * 根据事件类型获取事件类别
 *
 * @param eventType - 事件类型编号
 * @returns 事件类别字符串
 */
export function getEventCategory(eventType: number): StreamEventCategory {
  if (eventType <= 3) return 'info';
  if (eventType <= 7) return 'step';
  if (eventType <= 10) return 'result';
  if (eventType <= 13) return 'error';
  if (eventType <= 15) return 'warning';
  return 'debug';
}

/**
 * 根据事件类别获取对应的 CSS 颜色变量
 *
 * @param category - 事件类别
 * @returns CSS 颜色变量值
 */
export function getCategoryColor(category: StreamEventCategory): string {
  const colorMap: Record<StreamEventCategory, string> = {
    info: 'var(--color-trust-blue)',
    step: 'var(--color-trust-green)',
    result: 'var(--color-module-graph)',
    error: 'var(--color-danger)',
    warning: 'var(--color-trust-yellow)',
    debug: 'var(--color-text-muted)',
  };
  return colorMap[category] || colorMap.debug;
}

/**
 * 事件类别到中文标签的映射
 *
 * @param category - 事件类别
 * @returns 中文标签
 */
export function getCategoryLabel(category: StreamEventCategory): string {
  const labelMap: Record<StreamEventCategory, string> = {
    info: '信息',
    step: '步骤',
    result: '结果',
    error: '错误',
    warning: '警告',
    debug: '调试',
  };
  return labelMap[category] || '未知';
}
