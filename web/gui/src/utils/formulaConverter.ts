/**
 * @module utils/formulaConverter
 * @description 公式格式转换工具模块。
 *
 *              从 FormulaPanel 中提取的 DSL 格式转换器，
 *              支持将 DSL 文本转换为 LaTeX 和 Python 代码。
 *
 *              功能：
 *              1. toLatex —— DSL -> LaTeX 转换
 *              2. toPython —— DSL -> Python (SymPy) 转换
 */

import { parseFormula } from '@/utils/formulaParser';

// ================================================================
// toLatex —— DSL -> LaTeX 转换
// ================================================================

/**
 * 将 DSL 公式文本转换为 LaTeX 格式。
 *
 * 转换规则：
 * - point A(x, y)  ->  % \coordinate (A) at (x,y);
 * - segment AB     ->  % \draw (A) -- (B);
 * - circle ...     ->  % \draw ...;
 * - midpoint M of A, B  ->  % (M) at midpoint of (A) and (B);
 * - measure ...    ->  % \tkzCalcLength for ...
 * - intersect ...  ->  % \tkzInterLL for ...
 * - comment        ->  原样保留
 * - error          ->  % ERROR: ...
 *
 * @param dsl - DSL 公式文本
 * @returns LaTeX 格式的字符串
 */
export function toLatex(dsl: string): string {
  const lines: string[] = [];
  const result = parseFormula(dsl);
  result.commands.forEach((cmd) => {
    // 注释行原样保留
    if (cmd.type === 'comment') { lines.push(cmd.raw); return; }
    // 错误行转为 LaTeX 注释
    if (cmd.error) { lines.push(`% ERROR: ${cmd.error}`); return; }
    // 解析命令操作符和参数
    const m = cmd.raw.match(/^(\w+)\s+(.+)/);
    if (!m) { lines.push(cmd.raw); return; }
    const op = m[1]!.toLowerCase();
    const rest = m[2]!.trim();
    switch (op) {
      case 'point': {
        const pm = rest.match(/(\w+)\s*\(\s*([^,]+)\s*,\s*([^)]+)\s*\)/i);
        if (pm) lines.push(`% \\coordinate (${pm[1]!}) at (${pm[2]!.trim()},${pm[3]!.trim()});`);
        else lines.push(cmd.raw);
        break;
      }
      case 'segment': {
        const sm = rest.match(/(\w+)\s*(\w+)/i);
        if (sm) lines.push(`% \\draw (${sm[1]}) -- (${sm[2]});`);
        else lines.push(cmd.raw);
        break;
      }
      case 'circle': { lines.push(`% \\draw ${rest};`); break; }
      case 'midpoint': {
        const mm = rest.match(/(\w+)\s+of\s+(\w+)\s*,\s*(\w+)/i);
        if (mm) lines.push(`% (${mm[1]}) at midpoint of (${mm[2]}) and (${mm[3]});`);
        else lines.push(cmd.raw);
        break;
      }
      case 'measure': { lines.push(`% \\tkzCalcLength for ${rest}`); break; }
      case 'intersect': { lines.push(`% \\tkzInterLL for ${rest}`); break; }
      default: lines.push(`% ${cmd.raw}`);
    }
  });
  return lines.join('\n');
}

// ================================================================
// toPython —— DSL -> Python (SymPy) 转换
// ================================================================

/**
 * 将 DSL 公式文本转换为 Python (SymPy) 代码。
 *
 * 转换规则：
 * - 自动添加 `import sympy as sp` 头部
 * - point A(x, y)  ->  A = sp.Point(x, y)
 * - segment AB     ->  AB = sp.Segment(A, B)
 * - circle ...     ->  # circle: ...
 * - midpoint M of A, B  ->  M = sp.Point((A.x + B.x) / 2, (A.y + B.y) / 2)
 * - 其他命令       ->  # 原始命令
 *
 * @param dsl - DSL 公式文本
 * @returns Python (SymPy) 格式的字符串
 */
export function toPython(dsl: string): string {
  const lines: string[] = [];
  lines.push('import sympy as sp');
  lines.push('');
  const result = parseFormula(dsl);
  result.commands.forEach((cmd) => {
    // 注释行原样保留
    if (cmd.type === 'comment') { lines.push(cmd.raw); return; }
    // 错误行转为 Python 注释
    if (cmd.error) { lines.push(`# ERROR: ${cmd.error}`); return; }
    // 解析命令操作符和参数
    const m = cmd.raw.match(/^(\w+)\s+(.+)/);
    if (!m) { lines.push(cmd.raw); return; }
    const op = m[1]!.toLowerCase();
    const rest = m[2]!.trim();
    switch (op) {
      case 'point': {
        const pm = rest.match(/(\w+)\s*\(\s*([^,]+)\s*,\s*([^)]+)\s*\)/i);
        if (pm) lines.push(`${pm[1]!} = sp.Point(${pm[2]!.trim()}, ${pm[3]!.trim()})`);
        else lines.push(cmd.raw);
        break;
      }
      case 'segment': {
        const sm = rest.match(/(\w+)\s*(\w+)/i);
        if (sm) lines.push(`${sm[1]}${sm[2]} = sp.Segment(${sm[1]}, ${sm[2]})`);
        else lines.push(cmd.raw);
        break;
      }
      case 'circle': { lines.push(`# circle: ${rest}`); break; }
      case 'midpoint': {
        const mm = rest.match(/(\w+)\s+of\s+(\w+)\s*,\s*(\w+)/i);
        if (mm) lines.push(`${mm[1]} = sp.Point((${mm[2]}.x + ${mm[3]}.x) / 2, (${mm[2]}.y + ${mm[3]}.y) / 2)`);
        else lines.push(cmd.raw);
        break;
      }
      case 'measure':
      case 'intersect':
      default: lines.push(`# ${cmd.raw}`);
    }
  });
  return lines.join('\n');
}
