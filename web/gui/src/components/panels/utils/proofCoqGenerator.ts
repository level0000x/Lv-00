/**
 * @module components/panels/utils/proofCoqGenerator
 * @description Coq 格式证明脚本生成工具。
 *              根据当前几何构造（点、线段、约束）生成 Coq 格式的证明脚本，
 *              并支持矛盾检测与反证法叙述生成。
 *
 *              Coq format proof script generator.
 *              Generates Coq format proof scripts from current geometric construction,
 *              and supports contradiction detection and proof by contradiction narratives.
 */

import type { Point, Segment, Constraint } from '@/types';
import { detectConflicts } from '@/utils/geometryAlgorithms';

// ================================================================
// Coq 脚本生成 / Coq Script Generation
// ================================================================

/**
 * 生成 Coq 格式的证明脚本。
 * 包含点声明、线段声明、约束声明和证明总结。
 *
 * @param points - 当前点集合
 * @param segments - 当前线段集合
 * @param constraints - 当前约束集合
 * @returns Coq 脚本字符串
 */
export function generateCoqScript(
  points: Point[],
  segments: Segment[],
  constraints: Constraint[],
): string {
  const timestamp = new Date().toISOString().slice(0, 19).replace(/[:-]/g, '');
  let script = `(* Lv-00 几何证明自动生成 *)\n`;
  script += `(* Generated: ${new Date().toLocaleString()} *)\n\n`;
  script += `Lemma geometry_${timestamp} : True.\n`;
  script += `Proof.\n`;

  // 生成点声明
  if (points.length > 0) {
    script += `  (* 构造点 / Construct points *)\n`;
    for (const p of points) {
      script += `  exists point p${p.id} at (${p.x.toFixed(2)}, ${p.y.toFixed(2)}).\n`;
    }
    script += `\n`;
  }

  // 生成线段声明
  if (segments.length > 0) {
    script += `  (* 构造线段 / Construct segments *)\n`;
    for (const s of segments) {
      script += `  exists segment s${s.id} connecting p${s.p1} and p${s.p2}.\n`;
    }
    script += `\n`;
  }

  // 生成约束声明
  if (constraints.length > 0) {
    script += `  (* 施加约束 / Apply constraints *)\n`;
    for (const c of constraints) {
      const argsStr = c.args.map((a) => `p${a}`).join(', ');
      switch (c.type) {
        case 'incidence':
          script += `  constraint (incidence) on (${argsStr}).\n`;
          break;
        case 'betweenness':
          script += `  constraint (betweenness) on (${argsStr}).\n`;
          break;
        case 'intersection':
          script += `  constraint (intersection) on (${argsStr}).\n`;
          break;
        case 'containment':
          script += `  constraint (containment) on (${argsStr}).\n`;
          break;
        case 'connection':
          script += `  constraint (connection) on (${argsStr}).\n`;
          break;
        default:
          script += `  constraint (${c.type}) on (${argsStr}).\n`;
      }
    }
    script += `\n`;
  }

  // 证明总结
  const conflictCount = detectConflicts(constraints).length;
  if (conflictCount > 0) {
    script += `  (* 检测到 ${conflictCount} 个约束冲突 *)\n`;
    script += `  ex_falso.\n`;
    script += `  contradiction.\n`;
  } else {
    script += `  (* 无约束冲突，构造有效 *)\n`;
    script += `  trivial.\n`;
  }

  script += `Qed.\n`;

  return script;
}

// ================================================================
// 反证法 / Ex Falso (Contradiction Proof)
// ================================================================

/**
 * 分析约束冲突，生成反证法（矛盾证明）叙述。
 * 如果约束集合一致，则返回无矛盾信息。
 *
 * @param constraints - 当前约束集合
 * @returns 矛盾证明叙述字符串
 */
export function generateExFalsoNarrative(constraints: Constraint[]): string {
  const conflicts = detectConflicts(constraints);

  if (conflicts.length === 0) {
    return '未检测到矛盾。\nNo contradiction detected.\n\n当前约束集合是一致的，无法通过反证法推导矛盾。\nThe current constraint set is consistent; no contradiction can be derived.';
  }

  // 构建矛盾证明叙述
  let narrative = `=== 反证法证明 / Proof by Contradiction ===\n\n`;
  narrative += `检测到 ${conflicts.length} 个约束冲突:\n`;
  narrative += `Detected ${conflicts.length} constraint conflict(s):\n\n`;

  for (let i = 0; i < conflicts.length; i++) {
    const cf = conflicts[i];
    if (!cf) continue;
    narrative += `[冲突 ${i + 1}] 约束 #${cf.c1} 与 约束 #${cf.c2}\n`;
    narrative += `  Conflict #${i + 1}: Constraint #${cf.c1} vs Constraint #${cf.c2}\n`;
    narrative += `  原因 / Reason: ${cf.reason}\n\n`;
  }

  narrative += `---\n`;
  narrative += `证明叙述 / Proof Narrative:\n\n`;
  narrative += `1. 假设所有约束同时成立。\n`;
  narrative += `   Assume all constraints hold simultaneously.\n\n`;

  const firstConflict = conflicts[0];
  if (firstConflict) {
    narrative += `2. 由约束 #${firstConflict.c1} 可得: ${firstConflict.reason}\n`;
    narrative += `   From constraint #${firstConflict.c1}: ${firstConflict.reason}\n\n`;
    narrative += `3. 由约束 #${firstConflict.c2} 可得: 与上述结论矛盾。\n`;
    narrative += `   From constraint #${firstConflict.c2}: Contradicts the above conclusion.\n\n`;
  }
  narrative += `4. 因此，假设不成立。约束集合存在矛盾。\n`;
  narrative += `   Therefore, the assumption fails. The constraint set is inconsistent.\n\n`;
  narrative += `QED. Ex falso quodlibet.\n`;

  return narrative;
}
