/**
 * @module components/panels/FormulaPanel
 * @description 公式模块侧边栏面板（重构版）。
 *
 *              本文件为组合层，将原有 1273 行的单体组件拆分为多个子组件和 hooks：
 *              - FormulaInput: 公式输入区域（输入框、解析按钮、格式选择、实时预览）
 *              - FormulaOutput: 公式输出区域（渲染结果、日志）
 *              - FormulaExamples: 示例公式列表
 *              - FormulaHistory: 历史记录管理（撤销/重做、清空、导出）
 *              - FormulaSyntaxGuide: 语法指南
 *              - useFormulaHistory: 历史记录 hook
 *              - useFormulaSync: 公式双向同步 hook
 *
 *              实现功能：
 *              1. PARSE —— 解析公式 DSL 文本，显示解析后的命令列表
 *              2. RENDER —— 执行解析后的公式，在画布上创建几何图元
 *              3. FORMULA -> GRAPH —— 解析 + 执行 + 创建几何
 *              4. GRAPH -> FORMULA —— 从当前画布几何生成 DSL 文本
 *              5. 示例加载 —— 使用真实 DSL 语法
 *              6. 错误显示 —— 在输出区域内联显示解析错误
 *              7. SOLVE —— 约束求解（Gauss-Seidel 迭代松弛）
 *              8. 输出格式转换 —— DSL / LaTeX / Python
 */

import React, { useState, useCallback } from 'react';
import Panel from './Panel';
import { useAppStore } from '@/stores';
import type { FormulaOutputFormat } from '@/types';
import {
  parseFormula,
  parseAndExecuteFormula,
  generateDSLFromGeometry,
} from '@/utils/formulaParser';
import { MAX_PANEL_LOG_ENTRIES } from '@/utils/constants';
import { generateUniqueId } from '@/utils/idGenerator';

// 导入拆分后的子组件
import FormulaInput from './formula/FormulaInput';
import FormulaOutput from './formula/FormulaOutput';
import FormulaExamples from './formula/FormulaExamples';
import FormulaHistory from './formula/FormulaHistory';
import FormulaSyntaxGuide from './formula/FormulaSyntaxGuide';

// 导入拆分后的 hooks
import { useFormulaHistory } from '@/hooks/useFormulaHistory';
import { useFormulaSync } from '@/hooks/useFormulaSync';

/** 日志截断长度 / Max characters to show for formula input in log messages */
const LOG_TRUNCATE_LENGTH = 50;

/**
 * FormulaPanel - 公式模块侧边栏面板（组合层）
 *
 * 面板分区:
 * - INPUT   : 公式文本输入 + 语法模式选择 + 实时预览（FormulaInput 子组件）
 * - CONVERT : 公式 <-> 图形转换 + 输出格式选择
 * - OUTPUT  : 解析/渲染结果展示（FormulaOutput 子组件）
 * - EXAMPLES: 预设公式示例快速加载（FormulaExamples 子组件）
 * - ACTIONS : 清空、导出按钮（FormulaHistory 子组件）
 * - SYNTAX GUIDE: DSL 语法速查（FormulaSyntaxGuide 子组件）
 * - LOG     : 公式操作日志（FormulaOutput 子组件）
 */
const FormulaPanel: React.FC = () => {
  // ================================================================
  // Store 选择器 —— 公式模块关联状态
  // ================================================================
  const formulaInput = useAppStore((s) => s.formulaInput);
  const formulaSyntax = useAppStore((s) => s.formulaSyntax);
  const formulaOutputFormat = useAppStore((s) => s.formulaOutputFormat);
  const setFormulaInput = useAppStore((s) => s.setFormulaInput);
  const setFormulaSyntax = useAppStore((s) => s.setFormulaSyntax);
  const setFormulaOutputFormat = useAppStore((s) => s.setFormulaOutputFormat);
  const addToast = useAppStore((s) => s.addToast);
  const appendLog = useAppStore((s) => s.appendLog);
  const saveUndoState = useAppStore((s) => s.saveUndoState);
  const addPoint = useAppStore((s) => s.addPoint);
  const addSegment = useAppStore((s) => s.addSegment);
  const addConstraint = useAppStore((s) => s.addConstraint);
  const points = useAppStore((s) => s.points);
  const segments = useAppStore((s) => s.segments);
  const constraints = useAppStore((s) => s.constraints);

  // ================================================================
  // 本地状态 —— 输出内容与日志
  // ================================================================
  const [outputText, setOutputText] = useState<string>('');
  const [logEntries, setLogEntries] = useState<Array<{ time: string; msg: string; id: string }>>([]);

  // ================================================================
  // 使用拆分后的 hooks
  // ================================================================

  /** 公式历史记录管理 */
  const {
    formulaHistory,
    historyIndex,
    pushToHistoryAndSet,
    handleUndoFormula,
    handleRedoFormula,
  } = useFormulaHistory(setFormulaInput, formulaInput, addToast);

  /** 公式双向同步 */
  const {
    syncEnabled,
    syncStatus,
    handleToggleSync,
    isExecutingFormula,
  } = useFormulaSync();

  // ================================================================
  // 操作处理函数
  // ================================================================

  /**
   * 解析公式 —— 调用 DSL 解析器，显示解析后的命令列表
   */
  const handleParse = useCallback(() => {
    if (!formulaInput.trim()) {
      addToast('warning', '请输入公式 / Please enter a formula');
      return;
    }
    const now = new Date().toLocaleTimeString();
    appendLog(`解析公式: ${formulaInput.substring(0, LOG_TRUNCATE_LENGTH)}...`, 'info');
    setLogEntries((prev) => [...prev.slice(-MAX_PANEL_LOG_ENTRIES), { time: now, msg: 'PARSE: 开始解析', id: String(generateUniqueId()) }]);

    // 调用 DSL 解析器
    const result = parseFormula(formulaInput);

    // 构建输出文本
    const lines: string[] = [];
    if (result.errors.length > 0) {
      lines.push('=== 解析错误 / Parse Errors ===');
      result.errors.forEach((err) => {
        const lineMatch = err.match(/^(?:line\s*)?(\d+)[:;]?\s*(.*)/i);
        if (lineMatch) {
          const errLine = parseInt(lineMatch[1]!, 10);
          const errMsg = lineMatch[2]!.trim();
          const rawLines = formulaInput.split('\n');
          const offendingLine = rawLines[errLine - 1] || '';
          lines.push(`  Line ${errLine}: ${offendingLine.trim()}`);
          lines.push(`  ${' '.repeat(7)}^-- ${errMsg}`);
        } else {
          lines.push(`  [ERROR] ${err}`);
        }
        lines.push('');
      });
    }

    lines.push('=== 解析结果 / Parsed Commands ===');
    let validCount = 0;
    result.commands.forEach((cmd, i) => {
      if (cmd.type === 'comment') return;
      if (cmd.error) {
        lines.push(`  ${i + 1}. [${cmd.type}] ${cmd.raw}`);
        lines.push(`     ERROR: ${cmd.error}`);
      } else {
        validCount++;
        lines.push(`  ${i + 1}. [${cmd.type}] ${cmd.raw}`);
      }
    });
    lines.push('');
    lines.push(`共 ${result.commands.length} 条命令, ${validCount} 条有效, ${result.errors.length} 个错误`);

    setOutputText(lines.join('\n'));
    setLogEntries((prev) => [
      ...prev.slice(-MAX_PANEL_LOG_ENTRIES),
      { time: now, msg: `PARSE: ${validCount} 有效, ${result.errors.length} 错误`, id: String(generateUniqueId()) },
    ]);
    addToast(
      result.errors.length > 0 ? 'warning' : 'success',
      `解析完成: ${validCount} 条有效命令${result.errors.length > 0 ? `, ${result.errors.length} 个错误` : ''}`,
    );
  }, [formulaInput, addToast, appendLog]);

  /**
   * 渲染公式 —— 解析并执行，在画布上创建几何图元
   */
  const handleRender = useCallback(() => {
    if (!formulaInput.trim()) {
      addToast('warning', '请输入公式 / Please enter a formula');
      return;
    }
    const now = new Date().toLocaleTimeString();
    appendLog(`渲染公式: ${formulaInput.substring(0, LOG_TRUNCATE_LENGTH)}...`, 'info');
    setLogEntries((prev) => [...prev.slice(-MAX_PANEL_LOG_ENTRIES), { time: now, msg: 'RENDER: 开始执行', id: String(generateUniqueId()) }]);

    saveUndoState();

    // 标记公式正在执行，防止自动同步写入死循环
    isExecutingFormula.current = true;

    try {
      const result = parseAndExecuteFormula(formulaInput, points);

      result.createdPoints.forEach((p) => addPoint(p));
      result.createdSegments.forEach((s) => addSegment(s));
      result.createdConstraints.forEach((c) => addConstraint(c));

      const lines: string[] = [];

      if (result.errors.length === 0) {
        lines.push('\u250C\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2510');
        lines.push('\u2502  \u2705 \u6E32\u67D3\u6210\u529F / Render Success' + ' '.repeat(28) + '\u2502');
        lines.push('\u2502  \uD83D\uDCCD ' + result.createdPoints.length + ' \u4E2A\u70B9 / ' + result.createdPoints.length + ' points' + ' '.repeat(Math.max(0, 26 - String(result.createdPoints.length).length * 2 - 12)) + '\u2502');
        lines.push('\u2502  \uD83D\uDCCF ' + result.createdSegments.length + ' \u6761\u7EBF\u6BB5 / ' + result.createdSegments.length + ' segments' + ' '.repeat(Math.max(0, 26 - String(result.createdSegments.length).length * 2 - 15)) + '\u2502');
        lines.push('\u2502  \uD83D\uDD17 ' + result.createdConstraints.length + ' \u4E2A\u7EA6\u675F / ' + result.createdConstraints.length + ' constraints' + ' '.repeat(Math.max(0, 26 - String(result.createdConstraints.length).length * 2 - 17)) + '\u2502');
        lines.push('\u2514\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2518');
        lines.push('');
      }

      if (result.errors.length > 0) {
        lines.push('=== 错误 / Errors ===');
        result.errors.forEach((err) => lines.push(`  [ERROR] ${err}`));
        lines.push('');
      }

      lines.push('=== 执行结果 / Execution Results ===');
      result.commands.forEach((cmd) => {
        if (cmd.type === 'comment') return;
        if (cmd.result) {
          lines.push(`  [OK] ${cmd.result}`);
        } else if (cmd.error) {
          lines.push(`  [FAIL] ${cmd.error}`);
        }
      });

      if (result.measurements.length > 0) {
        lines.push('');
        lines.push('=== 度量结果 / Measurements ===');
        result.measurements.forEach((m) => {
          lines.push(`  ${m.label} = ${m.value}`);
        });
      }

      lines.push('');
      lines.push(
        `创建: ${result.createdPoints.length} 点, ${result.createdSegments.length} 线段, ${result.createdConstraints.length} 约束`,
      );

      setOutputText(lines.join('\n'));
      setLogEntries((prev) => [
        ...prev.slice(-MAX_PANEL_LOG_ENTRIES),
        {
          time: now,
          msg: `RENDER: ${result.createdPoints.length} 点, ${result.createdSegments.length} 线段`,
          id: String(generateUniqueId()),
        },
      ]);
      addToast(
        result.errors.length > 0 ? 'warning' : 'success',
        `渲染完成: ${result.createdPoints.length} 点, ${result.createdSegments.length} 线段, ${result.createdConstraints.length} 约束`,
      );
    } finally {
      isExecutingFormula.current = false;
    }
  }, [
    formulaInput,
    points,
    saveUndoState,
    addPoint,
    addSegment,
    addConstraint,
    addToast,
    appendLog,
  ]);

  /**
   * 公式转图形 —— 解析 + 执行 + 创建几何
   */
  const handleFormulaToGraph = useCallback(() => {
    if (!formulaInput.trim()) {
      addToast('warning', '请输入公式 / Please enter a formula');
      return;
    }
    const now = new Date().toLocaleTimeString();
    appendLog('公式转图形: 开始', 'info');
    setLogEntries((prev) => [...prev.slice(-MAX_PANEL_LOG_ENTRIES), { time: now, msg: 'F->G: 开始转换', id: String(generateUniqueId()) }]);

    saveUndoState();
    isExecutingFormula.current = true;

    try {
      const result = parseAndExecuteFormula(formulaInput, points);

      result.createdPoints.forEach((p) => addPoint(p));
      result.createdSegments.forEach((s) => addSegment(s));
      result.createdConstraints.forEach((c) => addConstraint(c));

      let summary = `公式转图形: 创建 ${result.createdPoints.length} 点, ${result.createdSegments.length} 线段, ${result.createdConstraints.length} 约束`;
      if (result.errors.length > 0) {
        summary += ` (${result.errors.length} 个错误)`;
      }

      const lines: string[] = ['=== 公式 -> 图形 ==='];
      result.commands.forEach((cmd) => {
        if (cmd.result) lines.push(`  [OK] ${cmd.result}`);
        if (cmd.error) lines.push(`  [ERR] ${cmd.error}`);
      });
      if (result.measurements.length > 0) {
        lines.push('');
        result.measurements.forEach((m) => lines.push(`  ${m.label} = ${m.value}`));
      }
      setOutputText(lines.join('\n'));

      appendLog(summary, 'info');
      setLogEntries((prev) => [...prev.slice(-MAX_PANEL_LOG_ENTRIES), { time: now, msg: `F->G: 完成`, id: String(generateUniqueId()) }]);
      addToast(result.errors.length > 0 ? 'warning' : 'success', summary);
    } finally {
      isExecutingFormula.current = false;
    }
  }, [
    formulaInput,
    points,
    saveUndoState,
    addPoint,
    addSegment,
    addConstraint,
    addToast,
    appendLog,
  ]);

  /**
   * 图形转公式 —— 从当前画布几何生成 DSL 文本
   */
  const handleGraphToFormula = useCallback(() => {
    if (points.length === 0) {
      addToast('warning', '画布上没有几何元素 / No geometry on canvas');
      return;
    }
    const now = new Date().toLocaleTimeString();
    appendLog('图形转公式: 开始', 'info');

    const dsl = generateDSLFromGeometry(points, segments, constraints);
    setFormulaInput(dsl);
    setOutputText(`=== 图形 -> 公式 ===\n\n${dsl}\n\n共 ${points.length} 点, ${segments.length} 线段, ${constraints.length} 约束`);

    setLogEntries((prev) => [
      ...prev.slice(-MAX_PANEL_LOG_ENTRIES),
      { time: now, msg: `G->F: ${points.length} 点, ${segments.length} 线段`, id: String(generateUniqueId()) },
    ]);
    addToast('success', `已生成 DSL: ${points.length} 点, ${segments.length} 线段`);
  }, [points, segments, constraints, setFormulaInput, addToast, appendLog]);

  /**
   * 清空输入框、输出区域和日志。
   */
  const handleClear = useCallback(() => {
    setFormulaInput('');
    setOutputText('');
    setLogEntries([]);
    addToast('info', '已清空 / Cleared');
  }, [setFormulaInput, addToast]);

  /**
   * 点击示例后，将示例代码填充到输入框。
   */
  const handleExampleClick = useCallback(
    (exampleId: string) => {
      // 导入 FORMULA_EXAMPLES 常量（从 FormulaExamples 子组件中提取）
      const FORMULA_EXAMPLES: Array<{ id: string; label: string; code: string }> = [
        { id: 'equilateral_triangle', label: '等边三角形', code: '// 等边三角形\npoint A(0, 0)\npoint B(4, 0)\npoint C(2, 3.46)\nsegment AB\nsegment BC\nsegment CA\nmeasure distance A, B\nmeasure angle A, B, C' },
        { id: 'circle_equation', label: '圆的方程', code: '// 以原点为圆心、半径为 3 的圆\npoint O(0, 0)\npoint R(3, 0)\ncircle center(O) radius(R)\nmeasure distance O, R' },
        { id: 'pythagorean', label: '勾股定理', code: '// 直角三角形 3-4-5\npoint A(0, 0)\npoint B(4, 0)\npoint C(0, 3)\nsegment AB\nsegment BC\nsegment CA\nmeasure distance A, B\nmeasure distance B, C\nmeasure distance C, A\nmeasure angle B, A, C' },
        { id: 'midpoint', label: '中垂线', code: '// 中点与中垂线\npoint A(0, 0)\npoint B(6, 0)\nmidpoint M of A, B\nsegment AB\nmeasure distance A, M\nmeasure distance M, B' },
        { id: 'line_equation', label: '直线方程', code: '// 直线上的点\npoint A(1, 1)\npoint B(5, 3)\nsegment AB\nmeasure distance A, B' },
        { id: 'triangle_area', label: '三角形面积', code: '// 三角形（用底和高估算面积）\npoint A(0, 0)\npoint B(6, 0)\npoint C(3, 4)\nsegment AB\nsegment BC\nsegment CA\nmeasure distance A, B\nmeasure distance A, C' },
        { id: 'distance', label: '两点距离', code: '// 计算两点之间的距离\npoint P(1, 2)\npoint Q(4, 6)\nsegment PQ\nmeasure distance P, Q' },
        { id: 'intersection', label: '交点', code: '// 两条线段的交点\npoint A(0, 0)\npoint B(4, 4)\npoint C(0, 4)\npoint D(4, 0)\nsegment AB\nsegment CD\nintersect segment AB with CD' },
      ];
      const example = FORMULA_EXAMPLES.find((e) => e.id === exampleId);
      if (example) {
        pushToHistoryAndSet(example.code);
        addToast('info', `加载示例: ${example.label}`);
      }
    },
    [addToast, pushToHistoryAndSet],
  );

  // ================================================================
  // 输出格式转换器
  // ================================================================

  /** DSL -> LaTeX 转换 */
  const toLatex = useCallback((dsl: string): string => {
    const lines: string[] = [];
    const result = parseFormula(dsl);
    result.commands.forEach((cmd) => {
      if (cmd.type === 'comment') { lines.push(cmd.raw); return; }
      if (cmd.error) { lines.push(`% ERROR: ${cmd.error}`); return; }
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
  }, []);

  /** DSL -> Python 转换 */
  const toPython = useCallback((dsl: string): string => {
    const lines: string[] = [];
    lines.push('import sympy as sp');
    lines.push('');
    const result = parseFormula(dsl);
    result.commands.forEach((cmd) => {
      if (cmd.type === 'comment') { lines.push(cmd.raw); return; }
      if (cmd.error) { lines.push(`# ERROR: ${cmd.error}`); return; }
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
  }, []);

  /** 输出格式切换时自动转换当前公式 */
  const handleOutputFormatChange = useCallback(
    (newFormat: FormulaOutputFormat) => {
      setFormulaOutputFormat(newFormat);
      if (!formulaInput.trim()) return;
      let converted = formulaInput;
      if (newFormat === 'latex') {
        converted = toLatex(formulaInput);
      } else if (newFormat === 'python') {
        converted = toPython(formulaInput);
      }
      if (newFormat !== 'dsl' && converted !== formulaInput) {
        pushToHistoryAndSet(converted);
        addToast('info', `已转换为 ${newFormat.toUpperCase()} 格式 / Converted to ${newFormat.toUpperCase()}`);
      }
    },
    [formulaInput, toLatex, toPython, pushToHistoryAndSet, setFormulaOutputFormat, addToast],
  );

  // ================================================================
  // SOLVE —— 约束求解（Gauss-Seidel 迭代松弛）
  // ================================================================

  /** 求解结果类型 */
  interface SolveResult {
    solved: boolean;
    points: Array<{ label: string; x: number; y: number }>;
    constraints: Array<{ desc: string; satisfied: boolean; error?: number }>;
    iterations: number;
    error: string | null;
  }

  /**
   * 从公式文本中解析点定义和约束定义。
   */
  const parseForSolver = useCallback((text: string): {
    pointDefs: Map<string, { x: number; y: number; fixed: boolean }>;
    constraintDefs: Array<{ type: string; args: string[]; value?: number }>;
    parseErrors: string[];
  } => {
    const pointDefs = new Map<string, { x: number; y: number; fixed: boolean }>();
    const constraintDefs: Array<{ type: string; args: string[]; value?: number }> = [];
    const parseErrors: string[] = [];

    const lines = text.split('\n');
    for (const rawLine of lines) {
      const line = rawLine.trim();
      if (!line || line.startsWith('//') || line.startsWith('#')) continue;

      const pointMatch = line.match(/^point\s+(\w+)\s*\(\s*([^,]+)\s*,\s*([^)]+)\s*\)/i);
      if (pointMatch) {
        const label = pointMatch[1]!;
        const xStr = pointMatch[2]!.trim();
        const yStr = pointMatch[3]!.trim();
        const xVal = Number(xStr);
        const yVal = Number(yStr);
        if (!isNaN(xVal) && !isNaN(yVal)) {
          pointDefs.set(label, { x: xVal, y: yVal, fixed: true });
        } else {
          pointDefs.set(label, { x: 0, y: 0, fixed: false });
        }
        continue;
      }

      const distMatch = line.match(/distance\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)\s*=\s*([\d.]+)/i);
      if (distMatch) {
        constraintDefs.push({ type: 'distance', args: [distMatch[1]!, distMatch[2]!], value: Number(distMatch[3]!) });
        continue;
      }

      const midMatch = line.match(/^midpoint\s+(\w+)\s+of\s+(\w+)\s*,\s*(\w+)/i);
      if (midMatch) {
        const mLabel = midMatch[1]!;
        if (!pointDefs.has(mLabel)) {
          pointDefs.set(mLabel, { x: 0, y: 0, fixed: false });
        }
        constraintDefs.push({ type: 'midpoint', args: [mLabel, midMatch[2]!, midMatch[3]!] });
        continue;
      }

      const horizMatch = line.match(/^horizontal\s+(\w+)\s*,\s*(\w+)/i);
      if (horizMatch) {
        constraintDefs.push({ type: 'horizontal', args: [horizMatch[1]!, horizMatch[2]!] });
        continue;
      }

      const vertMatch = line.match(/^vertical\s+(\w+)\s*,\s*(\w+)/i);
      if (vertMatch) {
        constraintDefs.push({ type: 'vertical', args: [vertMatch[1]!, vertMatch[2]!] });
        continue;
      }

      const angleMatch = line.match(/angle\s*\(\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*\)\s*=\s*([\d.]+)/i);
      if (angleMatch) {
        constraintDefs.push({ type: 'angle', args: [angleMatch[1]!, angleMatch[2]!, angleMatch[3]!], value: Number(angleMatch[4]!) });
        continue;
      }
    }

    return { pointDefs, constraintDefs, parseErrors };
  }, []);

  /**
   * Gauss-Seidel 迭代松弛求解器。
   */
  const gaussSeidelSolve = useCallback((
    pointDefs: Map<string, { x: number; y: number; fixed: boolean }>,
    constraintDefs: Array<{ type: string; args: string[]; value?: number }>,
    maxIterations: number = 500,
    tolerance: number = 1e-6,
    damping: number = 0.5,
  ): SolveResult => {
    for (const c of constraintDefs) {
      for (const arg of c.args) {
        if (!pointDefs.has(arg)) {
          return { solved: false, points: [], constraints: [], iterations: 0, error: `未定义的点: ${arg} / Undefined point: ${arg}` };
        }
      }
    }

    const dist = (a: string, b: string): number => {
      const pa = pointDefs.get(a)!;
      const pb = pointDefs.get(b)!;
      return Math.sqrt((pa.x - pb.x) ** 2 + (pa.y - pb.y) ** 2);
    };

    const computeResidual = (c: typeof constraintDefs[number]): number => {
      switch (c.type) {
        case 'distance': return dist(c.args[0]!, c.args[1]!) - (c.value ?? 0);
        case 'midpoint': {
          const m = pointDefs.get(c.args[0]!)!;
          const a = pointDefs.get(c.args[1]!)!;
          const b = pointDefs.get(c.args[2]!)!;
          return Math.sqrt((m.x - (a.x + b.x) / 2) ** 2 + (m.y - (a.y + b.y) / 2) ** 2);
        }
        case 'horizontal': return pointDefs.get(c.args[0]!)!.y - pointDefs.get(c.args[1]!)!.y;
        case 'vertical': return pointDefs.get(c.args[0]!)!.x - pointDefs.get(c.args[1]!)!.x;
        case 'angle': {
          const a = pointDefs.get(c.args[0]!)!;
          const b = pointDefs.get(c.args[1]!)!;
          const cv = pointDefs.get(c.args[2]!)!;
          const v1x = a.x - b.x, v1y = a.y - b.y;
          const v2x = cv.x - b.x, v2y = cv.y - b.y;
          const dot = v1x * v2x + v1y * v2y;
          const cross = v1x * v2y - v1y * v2x;
          return Math.atan2(Math.abs(cross), dot) * 180 / Math.PI - (c.value ?? 0);
        }
        default: return 0;
      }
    };

    const relaxConstraint = (c: typeof constraintDefs[number]): void => {
      const eps = 1e-8;
      switch (c.type) {
        case 'distance': {
          const pa = pointDefs.get(c.args[0]!)!;
          const pb = pointDefs.get(c.args[1]!)!;
          const d = dist(c.args[0]!, c.args[1]!);
          if (d < eps) break;
          const target = c.value ?? 0;
          const ratio = (d - target) / d * damping;
          const dx = (pa.x - pb.x) * ratio;
          const dy = (pa.y - pb.y) * ratio;
          if (!pa.fixed) { pa.x -= dx / 2; pa.y -= dy / 2; }
          if (!pb.fixed) { pb.x += dx / 2; pb.y += dy / 2; }
          break;
        }
        case 'midpoint': {
          const m = pointDefs.get(c.args[0]!)!;
          const a = pointDefs.get(c.args[1]!)!;
          const b = pointDefs.get(c.args[2]!)!;
          if (!m.fixed) { m.x += ((a.x + b.x) / 2 - m.x) * damping; m.y += ((a.y + b.y) / 2 - m.y) * damping; }
          break;
        }
        case 'horizontal': {
          const a = pointDefs.get(c.args[0]!)!;
          const b = pointDefs.get(c.args[1]!)!;
          const avgY = (a.y + b.y) / 2;
          if (!a.fixed) a.y += (avgY - a.y) * damping;
          if (!b.fixed) b.y += (avgY - b.y) * damping;
          break;
        }
        case 'vertical': {
          const a = pointDefs.get(c.args[0]!)!;
          const b = pointDefs.get(c.args[1]!)!;
          const avgX = (a.x + b.x) / 2;
          if (!a.fixed) a.x += (avgX - a.x) * damping;
          if (!b.fixed) b.x += (avgX - b.x) * damping;
          break;
        }
        case 'angle': {
          const a = pointDefs.get(c.args[0]!)!;
          const b = pointDefs.get(c.args[1]!)!;
          const cv = pointDefs.get(c.args[2]!)!;
          const targetAngle = (c.value ?? 0) * Math.PI / 180;
          const v1x = a.x - b.x, v1y = a.y - b.y;
          const v2x = cv.x - b.x, v2y = cv.y - b.y;
          const len1 = Math.sqrt(v1x * v1x + v1y * v1y);
          const len2 = Math.sqrt(v2x * v2x + v2y * v2y);
          if (len1 < eps || len2 < eps) break;
          const currentAngle = Math.atan2(v1x * v2y - v1y * v2x, v1x * v2x + v1y * v2y);
          const angleDiff = targetAngle - currentAngle;
          const rotAngle = angleDiff * damping * 0.3;
          const cosR = Math.cos(rotAngle), sinR = Math.sin(rotAngle);
          if (!a.fixed) {
            const rx = b.x + v1x * cosR - v1y * sinR;
            const ry = b.y + v1x * sinR + v1y * cosR;
            a.x += (rx - a.x) * damping; a.y += (ry - a.y) * damping;
          }
          if (!cv.fixed) {
            const rx = b.x + v2x * cosR - v2y * sinR;
            const ry = b.y + v2x * sinR + v2y * cosR;
            cv.x += (rx - cv.x) * damping; cv.y += (ry - cv.y) * damping;
          }
          break;
        }
      }
    };

    let converged = false;
    let iterations = 0;
    for (let iter = 0; iter < maxIterations; iter++) {
      iterations = iter + 1;
      let totalResidual = 0;
      for (const c of constraintDefs) {
        const residual = computeResidual(c);
        totalResidual += residual * residual;
        relaxConstraint(c);
      }
      if (Math.sqrt(totalResidual) < tolerance) { converged = true; break; }
    }

    const solvedPoints: Array<{ label: string; x: number; y: number }> = [];
    for (const [label, pt] of pointDefs) {
      solvedPoints.push({ label, x: pt.x, y: pt.y });
    }

    const constraintResults = constraintDefs.map((c) => {
      const residual = Math.abs(computeResidual(c));
      let desc = '';
      switch (c.type) {
        case 'distance': desc = `distance(${c.args.join(', ')}) = ${c.value}`; break;
        case 'midpoint': desc = `midpoint ${c.args[0]} of ${c.args[1]}, ${c.args[2]}`; break;
        case 'horizontal': desc = `horizontal ${c.args.join(', ')}`; break;
        case 'vertical': desc = `vertical ${c.args.join(', ')}`; break;
        case 'angle': desc = `angle(${c.args.join(', ')}) = ${c.value}`; break;
      }
      return { desc, satisfied: residual < 0.01, error: residual };
    });

    const freePointCount = Array.from(pointDefs.values()).filter((p) => !p.fixed).length;
    return {
      solved: converged,
      points: solvedPoints,
      constraints: constraintResults,
      iterations,
      error: converged ? null : (freePointCount === 0
        ? '约束系统过约束或矛盾 / Over-constrained or contradictory system'
        : `未在 ${maxIterations} 次迭代内收敛 / Did not converge in ${maxIterations} iterations`),
    };
  }, []);

  const handleSolve = useCallback(() => {
    if (!formulaInput.trim()) {
      addToast('warning', '请先输入公式 / Please enter a formula first');
      return;
    }

    const now = new Date().toLocaleTimeString();
    appendLog('求解: 开始约束求解', 'info');
    setLogEntries((prev) => [...prev.slice(-MAX_PANEL_LOG_ENTRIES), { time: now, msg: 'SOLVE: 开始求解', id: String(generateUniqueId()) }]);

    const { pointDefs, constraintDefs, parseErrors } = parseForSolver(formulaInput);

    if (parseErrors.length > 0) {
      setOutputText(`=== 解析错误 / Parse Errors ===\n\n${parseErrors.join('\n')}`);
      addToast('error', `解析错误: ${parseErrors.length} 个`);
      return;
    }

    if (pointDefs.size === 0) {
      setOutputText('=== 求解结果 ===\n\n未检测到点定义。请使用 point A(x, y) 语法定义点。');
      addToast('warning', '未检测到点定义 / No point definitions found');
      return;
    }

    if (constraintDefs.length === 0) {
      setOutputText(`=== 求解结果 ===\n\n检测到 ${pointDefs.size} 个点，但无约束。\n请添加约束，例如:\n  distance(A,B) = 5\n  midpoint M of A, B\n  horizontal A, B\n  vertical A, C\n  angle(A,B,C) = 90`);
      addToast('warning', '未检测到约束 / No constraints found');
      return;
    }

    const result = gaussSeidelSolve(pointDefs, constraintDefs);

    const lines: string[] = [];
    lines.push('=== 约束求解结果 / Constraint Solve Result ===');
    lines.push('');
    if (result.error) { lines.push(`[WARNING] ${result.error}`); lines.push(''); }
    lines.push(`状态 / Status: ${result.solved ? 'CONVERGED / 已收敛' : 'NOT CONVERGED / 未收敛'}`);
    lines.push(`迭代次数 / Iterations: ${result.iterations}`);
    lines.push('');
    lines.push('--- 求解后坐标 / Solved Coordinates ---');
    for (const pt of result.points) {
      const def = pointDefs.get(pt.label);
      const marker = def?.fixed ? '(fixed)' : '(solved)';
      lines.push(`  ${pt.label} = (${pt.x.toFixed(4)}, ${pt.y.toFixed(4)}) ${marker}`);
    }
    lines.push('');
    lines.push('--- 约束满足状态 / Constraint Satisfaction ---');
    for (const c of result.constraints) {
      const status = c.satisfied ? '[OK]' : '[FAIL]';
      const errStr = c.error !== undefined ? ` (error: ${c.error.toExponential(3)})` : '';
      lines.push(`  ${status} ${c.desc}${errStr}`);
    }
    lines.push('');
    const satisfiedCount = result.constraints.filter((c) => c.satisfied).length;
    lines.push(`约束满足: ${satisfiedCount}/${result.constraints.length}`);

    setOutputText(lines.join('\n'));
    setLogEntries((prev) => [
      ...prev.slice(-MAX_PANEL_LOG_ENTRIES),
      {
        time: now,
        msg: `SOLVE: ${result.solved ? '收敛' : '未收敛'}, ${result.iterations} 迭代, ${satisfiedCount}/${result.constraints.length} 满足`,
        id: String(generateUniqueId()),
      },
    ]);
    addToast(
      result.solved ? 'success' : 'warning',
      `求解${result.solved ? '收敛' : '未收敛'}: ${result.iterations} 次迭代, ${satisfiedCount}/${result.constraints.length} 约束满足`,
    );
  }, [formulaInput, parseForSolver, gaussSeidelSolve, addToast, appendLog]);

  // ================================================================
  // 渲染 —— 组合所有子组件
  // ================================================================
  return (
    <>
      {/* INPUT 区域：公式文本输入 + 语法模式选择 + 实时预览 */}
      <FormulaInput
        formulaInput={formulaInput}
        formulaSyntax={formulaSyntax}
        setFormulaInput={setFormulaInput}
        setFormulaSyntax={setFormulaSyntax}
        addToast={addToast}
        appendLog={appendLog}
        setLogEntries={setLogEntries}
        onParse={handleParse}
        onRender={handleRender}
        onSolve={handleSolve}
      />

      {/* CONVERT 区域：公式 <-> 图形转换 */}
      <Panel title="CONVERT / 转换" panelId="formula-convert">
        {/* 公式转图形 —— 解析 DSL 并在画布上创建几何 */}
        <button className="btn btn-accent" onClick={handleFormulaToGraph}>
          FORMULA -&gt; GRAPH / 公式转图形
        </button>
        {/* 图形转公式 —— 从画布几何生成 DSL */}
        <button className="btn btn-accent" onClick={handleGraphToFormula}>
          GRAPH -&gt; FORMULA / 图形转公式
        </button>

        {/* 双向同步开关 */}
        <div className="formula-syntax-row" style={{ marginTop: 6 }}>
          <label>
            SYNC / 同步
            <span
              style={{
                display: 'inline-block',
                width: 8,
                height: 8,
                borderRadius: '50%',
                marginLeft: 6,
                backgroundColor: syncStatus === 'synced' ? '#4caf50' : syncStatus === 'syncing' ? '#ff9800' : '#9e9e9e',
              }}
              title={
                syncStatus === 'synced'
                  ? '已同步 / Synced'
                  : syncStatus === 'syncing'
                  ? '同步中 / Syncing...'
                  : '未同步 / Not syncing'
              }
            />
          </label>
          <button
            className={`btn ${syncEnabled ? 'btn-accent' : ''}`}
            onClick={handleToggleSync}
            style={{ fontSize: '11px', padding: '2px 8px' }}
          >
            {syncEnabled ? '\u{1F517} AUTO SYNC ON / \u81EA\u52A8\u540C\u6B65\u5F00' : '\u{1F517} AUTO SYNC OFF / \u81EA\u52A8\u540C\u6B65\u5173'}
          </button>
        </div>
        {syncEnabled && syncStatus === 'synced' && (
          <div className="info-box" style={{ marginTop: 4, fontSize: '10px', color: '#4caf50' }}>
            Auto-synced from canvas / 已自动从画布同步
          </div>
        )}

        <div className="panel-separator" />
        {/* 输出格式选择：控制转换结果的目标语法 */}
        <div className="input-group-row">
          <label>输出 / Output</label>
          <select
            className="select-field"
            value={formulaOutputFormat}
            onChange={(e) => handleOutputFormatChange(e.target.value as FormulaOutputFormat)}
          >
            <option value="latex">LaTeX</option>
            <option value="python">Python</option>
            <option value="dsl">DSL / 术式</option>
          </select>
        </div>
      </Panel>

      {/* OUTPUT 区域：解析/渲染结果展示 + LOG 日志 */}
      <FormulaOutput outputText={outputText} logEntries={logEntries} />

      {/* EXAMPLES 区域：预设公式示例列表 */}
      <FormulaExamples onExampleClick={handleExampleClick} />

      {/* ACTIONS 区域：历史记录管理（撤销/重做、清空、导出） */}
      <FormulaHistory
        formulaHistory={formulaHistory}
        historyIndex={historyIndex}
        onUndo={handleUndoFormula}
        onRedo={handleRedoFormula}
        onClear={handleClear}
        outputText={outputText}
        addToast={addToast}
      />

      {/* SYNTAX GUIDE 区域：DSL 语法速查 */}
      <FormulaSyntaxGuide />
    </>
  );
};

export default FormulaPanel;
