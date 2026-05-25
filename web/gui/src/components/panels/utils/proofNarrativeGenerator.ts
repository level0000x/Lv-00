/**
 * @module components/panels/utils/proofNarrativeGenerator
 * @description 自然语言证明生成工具。
 *              根据 proofSteps 比较相邻快照，生成 AlphaGeometry 风格的
 *              自然语言步骤描述，以及当前步骤的内联描述。
 *
 *              Natural language proof generator.
 *              Compares adjacent snapshots to generate AlphaGeometry-style
 *              natural language step descriptions.
 */

import type { Point, Segment, Constraint } from '@/types';
import { detectConflicts } from '@/utils/geometryAlgorithms';

// ================================================================
// 类型定义 / Type Definitions
// ================================================================

/** 证明步骤快照（包含几何状态） */
export interface ProofSnapshot {
  points: Point[];
  segments: Segment[];
  constraints: Constraint[];
  regions?: unknown[];
  ports?: unknown[];
  funcBlocks?: unknown[];
  timestamp?: number;
}

/** 动词映射（中英文） */
interface Verb {
  ch: string;
  en: string;
}

// ================================================================
// 常量 / Constants
// ================================================================

/** 常用动词列表 */
const VERBS: Verb[] = [
  { ch: '构造', en: 'Construct' },
  { ch: '连接', en: 'Connect' },
  { ch: '添加约束', en: 'Add constraint' },
  { ch: '移动', en: 'Move' },
  { ch: '删除', en: 'Delete' },
  { ch: '修改', en: 'Modify' },
];

// ================================================================
// 自然语言证明生成 / Natural Language Proof Generation
// ================================================================

/**
 * 根据 proofSteps 比较相邻快照，生成 AlphaGeometry 风格的自然语言步骤描述。
 *
 * @param proofSteps - 证明步骤快照数组
 * @param currentStepIndex - 当前步骤索引
 * @param searchStrategy - 搜索策略名称
 * @param strategyNote - 策略注释
 * @param stepNote - 步骤注释
 * @param constraints - 当前约束集合（用于总结）
 * @returns 自然语言证明字符串
 */
export function generateNlProof(
  proofSteps: ProofSnapshot[],
  currentStepIndex: number,
  searchStrategy: string,
  strategyNote: string,
  stepNote: string,
  constraints: Constraint[],
): string {
  if (proofSteps.length <= 1) {
    return '证明步骤不足，无法生成自然语言描述。至少需要2个步骤。\nNot enough proof steps to generate a natural language description. At least 2 steps are needed.';
  }

  let nl = `=== 自然语言证明 / Natural Language Proof ===\n`;
  nl += `搜索策略: ${searchStrategy} / Search Strategy: ${searchStrategy}\n`;
  if (strategyNote) {
    nl += `策略注释: ${strategyNote} / Strategy Note: ${strategyNote}\n`;
  }
  nl += `---\n\n`;

  for (let i = 0; i < proofSteps.length; i++) {
    const step = proofSteps[i];
    if (!step) continue;
    const stepNum = i + 1;
    const isCurrent = i === currentStepIndex;

    // 比较与前一步的差异
    let verb: Verb;
    let objects: string[] = [];
    let reason = '';

    if (i === 0) {
      verb = { ch: '初始化', en: 'Initialize' };
      if (step.points.length > 0) {
        objects.push(`点集 / point set (${step.points.length} pts)`);
      }
      reason = '起始画布 / initial canvas';
    } else {
      const prev = proofSteps[i - 1];
      if (!prev) continue;

      // 检测新增的点
      const newPoints = step.points.filter((p) => !prev.points.some((pp) => pp.id === p.id));
      // 检测新增的线段
      const newSegments = step.segments.filter((s) => !prev.segments.some((ps) => ps.id === s.id));
      // 检测新增的约束
      const newConstraints = step.constraints.filter((c) => !prev.constraints.some((pc) => pc.id === c.id));

      if (newPoints.length > 0) {
        verb = VERBS[0]!;
        objects = newPoints.map((p) => `点 / pt ${p.id}`);
        reason = `${searchStrategy}策略: 添加辅助点 / add auxiliary point`;
      } else if (newSegments.length > 0) {
        verb = VERBS[1]!;
        objects = newSegments.map((s) => `线段 / seg ${s.id} (p${s.p1}-p${s.p2})`);
        reason = '连接已有元素 / connect existing elements';
      } else if (newConstraints.length > 0) {
        verb = VERBS[2]!;
        objects = newConstraints.map((c) => `约束 / constraint ${c.id} (${c.type})`);
        reason = '施加推理约束 / apply deductive constraint';
      } else {
        verb = VERBS[5]!;
        objects = ['已有元素 / existing elements'];
        reason = '细化或调整 / refinement or adjustment';
      }
    }

    const trustStatus = isCurrent ? 'CURRENT / 当前' : (i < currentStepIndex ? 'VERIFIED / 已验证' : 'AHEAD / 未到达');

    nl += `[步骤 ${stepNum}] `;
    nl += `${verb.ch} / ${verb.en}\n`;
    nl += `    对象 / Objects: ${objects.join(', ')}\n`;
    nl += `    推理 / Reasoning: ${reason}\n`;
    nl += `    状态 / Status: ${trustStatus}\n`;

    if (i === currentStepIndex && stepNote) {
      nl += `    步骤注释 / Step Note: ${stepNote}\n`;
    }

    nl += `\n`;
  }

  // 总结
  nl += `---\n`;
  const conflictCount = detectConflicts(constraints).length;
  if (conflictCount > 0) {
    nl += `结论: 检测到 ${conflictCount} 个约束冲突，证明可能无效。\n`;
    nl += `Conclusion: ${conflictCount} constraint conflict(s) detected. Proof may be invalid.\n`;
  } else {
    nl += `结论: 约束集合一致，构造有效。\n`;
    nl += `Conclusion: Constraint set is consistent. Construction is valid.\n`;
  }

  return nl;
}

// ================================================================
// 当前步骤内联描述 / Current Step Inline Description
// ================================================================

/**
 * 生成当前步骤的自然语言内联描述。
 * 比较当前步骤与前一步的差异，生成简洁的变更描述。
 *
 * @param proofSteps - 证明步骤快照数组
 * @param currentStepIndex - 当前步骤索引
 * @returns 当前步骤描述字符串，如果无效则返回空字符串
 */
export function generateCurrentStepDescription(
  proofSteps: ProofSnapshot[],
  currentStepIndex: number,
): string {
  if (proofSteps.length === 0) return '';
  const idx = currentStepIndex;
  if (idx < 0 || idx >= proofSteps.length) return '';

  const step = proofSteps[idx];
  if (!step) return '';

  if (idx === 0) {
    return `初始化画布，包含 ${step.points.length} 个点。\nInitialize canvas with ${step.points.length} point(s).`;
  }

  const prev = proofSteps[idx - 1];
  if (!prev) return '';

  const newPoints = step.points.filter((p) => !prev.points.some((pp) => pp.id === p.id));
  const newSegments = step.segments.filter((s) => !prev.segments.some((ps) => ps.id === s.id));
  const newConstraints = step.constraints.filter((c) => !prev.constraints.some((pc) => pc.id === c.id));

  const parts: string[] = [];
  if (newPoints.length > 0) {
    parts.push(`构造 ${newPoints.length} 个新点 / Construct ${newPoints.length} new point(s)`);
  }
  if (newSegments.length > 0) {
    parts.push(`连接 ${newSegments.length} 条新线段 / Connect ${newSegments.length} new segment(s)`);
  }
  if (newConstraints.length > 0) {
    parts.push(`施加 ${newConstraints.length} 个新约束 / Apply ${newConstraints.length} new constraint(s)`);
  }
  if (parts.length === 0) {
    parts.push('细化已有元素 / Refine existing elements');
  }

  return parts.join('\n');
}
