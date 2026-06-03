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

// 导入从 FormulaExamples 中导出的示例数据常量
import { FORMULA_EXAMPLES } from './formula/FormulaExamples';

// 导入约束求解器工具函数
import { parseForSolver, gaussSeidelSolve } from '@/utils/constraintSolverUI';
import type { SolveResult } from '@/utils/constraintSolverUI';

// 导入格式转换器
import { toLatex, toPython } from '@/utils/formulaConverter';

/** 日志截断长度 / Max characters to show for formula input in log messages */
const LOG_TRUNCATE_LENGTH = 50;

/** Unicode 框线字符常量 —— 用于渲染成功结果的边框绘制 */
const BOX = {
  topLeft: '\u250C',
  topRight: '\u2510',
  bottomLeft: '\u2514',
  bottomRight: '\u2518',
  horizontal: '\u2500',
  vertical: '\u2502',
} as const;

/** 框线宽度（不含两侧边框字符） */
const BOX_INNER_WIDTH = 46;

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
        // 使用 Unicode 框线绘制渲染成功摘要
        const hLine = BOX.horizontal.repeat(BOX_INNER_WIDTH);
        lines.push(`${BOX.topLeft}${hLine}${BOX.topRight}`);
        lines.push(`${BOX.vertical}  \u2705 渲染成功 / Render Success${' '.repeat(28)}${BOX.vertical}`);
        lines.push(`${BOX.vertical}  \uD83D\uDCCD ${result.createdPoints.length} 个点 / ${result.createdPoints.length} points${' '.repeat(Math.max(0, 26 - String(result.createdPoints.length).length * 2 - 12))}${BOX.vertical}`);
        lines.push(`${BOX.vertical}  \uD83D\uDCCF ${result.createdSegments.length} 条线段 / ${result.createdSegments.length} segments${' '.repeat(Math.max(0, 26 - String(result.createdSegments.length).length * 2 - 15))}${BOX.vertical}`);
        lines.push(`${BOX.vertical}  \uD83D\uDD17 ${result.createdConstraints.length} 个约束 / ${result.createdConstraints.length} constraints${' '.repeat(Math.max(0, 26 - String(result.createdConstraints.length).length * 2 - 17))}${BOX.vertical}`);
        lines.push(`${BOX.bottomLeft}${hLine}${BOX.bottomRight}`);
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
   * 使用从 FormulaExamples.tsx 导出的 FORMULA_EXAMPLES 常量。
   */
  const handleExampleClick = useCallback(
    (exampleId: string) => {
      const example = FORMULA_EXAMPLES.find((e) => e.id === exampleId);
      if (example) {
        pushToHistoryAndSet(example.code);
        addToast('info', `加载示例: ${example.label}`);
      }
    },
    [addToast, pushToHistoryAndSet],
  );

  // ================================================================
  // 输出格式切换
  // ================================================================

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
    [formulaInput, pushToHistoryAndSet, setFormulaOutputFormat, addToast],
  );

  // ================================================================
  // SOLVE —— 约束求解（Gauss-Seidel 迭代松弛）
  // ================================================================

  /**
   * 执行约束求解。
   * 使用 parseForSolver 解析 DSL 文本中的点和约束定义，
   * 然后调用 gaussSeidelSolve 进行迭代求解。
   */
  const handleSolve = useCallback(() => {
    if (!formulaInput.trim()) {
      addToast('warning', '请先输入公式 / Please enter a formula first');
      return;
    }

    const now = new Date().toLocaleTimeString();
    appendLog('求解: 开始约束求解', 'info');
    setLogEntries((prev) => [...prev.slice(-MAX_PANEL_LOG_ENTRIES), { time: now, msg: 'SOLVE: 开始求解', id: String(generateUniqueId()) }]);

    // 解析 DSL 文本中的点和约束定义
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

    // 执行 Gauss-Seidel 迭代求解
    const result: SolveResult = gaussSeidelSolve(pointDefs, constraintDefs);

    // 构建求解结果输出文本
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
  }, [formulaInput, addToast, appendLog]);

  // ================================================================
  // 渲染 —— 组合所有子组件
  // ================================================================

  /** 根据同步状态计算状态指示灯的 CSS 类名 */
  const syncDotClassName = `fp-sync-dot fp-sync-dot--${syncStatus === 'synced' ? 'synced' : syncStatus === 'syncing' ? 'syncing' : 'idle'}`;

  /** 根据同步状态生成 title 提示文本 */
  const syncDotTitle = syncStatus === 'synced'
    ? '已同步 / Synced'
    : syncStatus === 'syncing'
    ? '同步中 / Syncing...'
    : '未同步 / Not syncing';

  return (
    <>
      {/* INPUT 区域：公式文本输入 + 语法模式选择 + 实时预览 */}
      <FormulaInput
        formulaInput={formulaInput}
        formulaSyntax={formulaSyntax}
        setFormulaInput={setFormulaInput}
        setFormulaSyntax={setFormulaSyntax}
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
        <div className="formula-syntax-row fp-sync-row">
          <label>
            SYNC / 同步
            <span
              className={syncDotClassName}
              title={syncDotTitle}
            />
          </label>
          <button
            className={`btn ${syncEnabled ? 'btn-accent' : ''} fp-sync-btn`}
            onClick={handleToggleSync}
          >
            {syncEnabled ? '\u{1F517} AUTO SYNC ON / \u81EA\u52A8\u540C\u6B65\u5F00' : '\u{1F517} AUTO SYNC OFF / \u81EA\u52A8\u540C\u6B65\u5173'}
          </button>
        </div>
        {syncEnabled && syncStatus === 'synced' && (
          <div className="info-box fp-sync-info">
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
