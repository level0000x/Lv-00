/**
 * @module components/panels/utils/funcBlockExecutor
 * @description 函数块执行工具。
 *              封装预设函数块的执行逻辑，包括输入验证、
 *              预设分发和结果收集。
 *
 *              Function block executor utility.
 *              Encapsulates preset function block execution logic,
 *              including input validation, preset dispatch,
 *              and result collection.
 */

import type { Point, Segment } from '@/types';
import {
  getNextId,
  executePerpendicularBisector,
  executeIntersection,
  executePerpendicularFoot,
  executeParallelLine,
  executeReflection,
} from '@/utils/funcBlockPresets';
import type { FuncBlockPreset, FuncBlockResult } from '@/utils/funcBlockPresets';

// ================================================================
// 类型定义 / Type Definitions
// ================================================================

/** 执行上下文：提供查找点和线段的方法 */
export interface ExecutionContext {
  /** 根据 ID 查找点 */
  findPoint: (id: number) => Point | undefined;
  /** 根据 ID 查找线段 */
  findSegment: (id: number) => Segment | undefined;
}

/** 执行结果（包含错误信息） */
export interface ExecutionOutcome {
  success: boolean;
  result?: FuncBlockResult;
  error?: string;
}

// ================================================================
// 输入验证 / Input Validation
// ================================================================

/**
 * 验证预设函数块的输入是否满足要求。
 *
 * @param preset - 预设函数块
 * @param inputPoints - 输入点数组
 * @param inputSegments - 输入线段数组
 * @returns 验证错误消息，如果验证通过则返回 null
 */
export function validateInputs(
  preset: FuncBlockPreset,
  inputPoints: Point[],
  inputSegments: Segment[],
): string | null {
  if (preset.segmentInput && inputSegments.length < 1) {
    return '请选择至少一条线段 / Select at least one segment';
  }
  if (!preset.segmentInput && inputPoints.length < preset.inputCount) {
    return `需要 ${preset.inputCount} 个点，已选 ${inputPoints.length} 个 / Need ${preset.inputCount} points, selected ${inputPoints.length}`;
  }
  return null;
}

// ================================================================
// 预设执行 / Preset Execution
// ================================================================

/**
 * 执行预设函数块。
 * 根据预设类型分发到对应的执行函数，返回执行结果。
 *
 * @param preset - 预设函数块
 * @param inputPoints - 输入点数组
 * @param inputSegments - 输入线段数组
 * @param ctx - 执行上下文（提供查找方法）
 * @returns 执行结果
 */
export function executePreset(
  preset: FuncBlockPreset,
  inputPoints: Point[],
  inputSegments: Segment[],
  ctx: ExecutionContext,
): ExecutionOutcome {
  try {
    let result: FuncBlockResult;

    switch (preset.id) {
      case 'PERPENDICULAR_BISECTOR': {
        if (inputSegments.length < 1) {
          return { success: false, error: '请选择一条线段' };
        }
        const seg0 = inputSegments[0];
        if (!seg0) return { success: false, error: '线段不存在' };
        const p1 = ctx.findPoint(seg0.p1);
        const p2 = ctx.findPoint(seg0.p2);
        if (!p1 || !p2) return { success: false, error: '线段端点不存在' };
        result = executePerpendicularBisector(p1, p2, getNextId);
        break;
      }
      case 'INTERSECTION': {
        if (inputSegments.length < 2) {
          return { success: false, error: '请选择两条线段 / Select two segments' };
        }
        const is0 = inputSegments[0];
        const is1 = inputSegments[1];
        if (!is0 || !is1) return { success: false, error: '线段不存在' };
        // 注意：这里需要传入所有点用于交点计算
        const allPoints = inputPoints;
        result = executeIntersection(is0, is1, allPoints, getNextId);
        break;
      }
      case 'PERPENDICULAR_FOOT': {
        if (inputPoints.length < 1 || inputSegments.length < 1) {
          return { success: false, error: '请选择一个点和一条线段 / Select a point and a segment' };
        }
        const pfPt = inputPoints[0];
        const pfSeg = inputSegments[0];
        if (!pfPt || !pfSeg) return { success: false, error: '输入不存在' };
        result = executePerpendicularFoot(pfPt, pfSeg, inputPoints, getNextId);
        break;
      }
      case 'PARALLEL_LINE': {
        if (inputPoints.length < 1 || inputSegments.length < 1) {
          return { success: false, error: '请选择一个点和一条线段 / Select a point and a segment' };
        }
        const plPt = inputPoints[0];
        const plSeg = inputSegments[0];
        if (!plPt || !plSeg) return { success: false, error: '输入不存在' };
        result = executeParallelLine(plPt, plSeg, inputPoints, getNextId);
        break;
      }
      case 'REFLECTION': {
        if (inputPoints.length < 1 || inputSegments.length < 1) {
          return { success: false, error: '请选择一个点和一条线段 / Select a point and a segment' };
        }
        const rfPt = inputPoints[0];
        const rfSeg = inputSegments[0];
        if (!rfPt || !rfSeg) return { success: false, error: '输入不存在' };
        result = executeReflection(rfPt, rfSeg, inputPoints, getNextId);
        break;
      }
      default: {
        // 使用预设自带的 execute 函数
        result = preset.execute(inputPoints, inputSegments, getNextId);
        break;
      }
    }

    return { success: true, result };
  } catch (err) {
    const errMsg = err instanceof Error ? err.message : String(err);
    return { success: false, error: errMsg };
  }
}

/**
 * 生成执行摘要日志。
 *
 * @param preset - 预设函数块
 * @param result - 执行结果
 * @returns 摘要字符串
 */
export function formatExecutionSummary(
  preset: FuncBlockPreset,
  result: FuncBlockResult,
): string {
  return `执行 ${preset.nameZh}: 创建 ${result.newPoints.length} 个点, ${result.newSegments.length} 条线段, ${result.newConstraints.length} 个约束`;
}
