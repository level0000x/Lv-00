/**
 * @module components/panels/FormulaPanel
 * @description 公式模块侧边栏面板。
 *
 *              实现功能：
 *              1. PARSE —— 解析公式 DSL 文本，显示解析后的命令列表
 *              2. RENDER —— 执行解析后的公式，在画布上创建几何图元
 *              3. FORMULA -> GRAPH —— 解析 + 执行 + 创建几何
 *              4. GRAPH -> FORMULA —— 从当前画布几何生成 DSL 文本
 *              5. 示例加载 —— 使用真实 DSL 语法
 *              6. 错误显示 —— 在输出区域内联显示解析错误
 */

import React, { useState, useCallback } from 'react';
import Panel from './Panel';
import { useAppStore } from '@/stores';
import type { FormulaSyntax, FormulaOutputFormat } from '@/types';
import {
  parseFormula,
  parseAndExecuteFormula,
  generateDSLFromGeometry,
} from '@/utils/formulaParser';
import { MAX_PANEL_LOG_ENTRIES } from '@/utils/constants';
import { generateId } from '@/utils/idGenerator';

/** 日志截断长度 / Max characters to show for formula input in log messages */
const LOG_TRUNCATE_LENGTH = 50;

/** 安全复制到剪贴板（兼容非 HTTPS 环境） / Safe clipboard copy with fallback */
function copyToClipboard(text: string, addToast: (v: string, m: string) => void): void {
  if (navigator.clipboard && window.isSecureContext) {
    navigator.clipboard.writeText(text).then(
      () => addToast('success', '已复制到剪贴板 / Copied to clipboard'),
      () => fallbackCopy(text, addToast),
    );
  } else {
    fallbackCopy(text, addToast);
  }
}

/** textarea 回退复制方案 */
function fallbackCopy(text: string, addToast: (v: string, m: string) => void): void {
  try {
    const ta = document.createElement('textarea');
    ta.value = text;
    ta.style.position = 'fixed';
    ta.style.opacity = '0';
    document.body.appendChild(ta);
    ta.select();
    document.execCommand('copy');
    document.body.removeChild(ta);
    addToast('success', '已复制到剪贴板 / Copied to clipboard');
  } catch {
    addToast('error', '复制失败 / Copy failed');
  }
}

/**
 * 预设的公式示例列表，使用真实 DSL 语法。
 * 每个示例的 id 对应 DSL 中的常见几何命题。
 */
const FORMULA_EXAMPLES: Array<{ id: string; label: string; code: string }> = [
  {
    id: 'equilateral_triangle',
    label: '等边三角形 / Equilateral Triangle',
    code: `// 等边三角形
point A(0, 0)
point B(4, 0)
point C(2, 3.46)
segment AB
segment BC
segment CA
measure distance A, B
measure angle A, B, C`,
  },
  {
    id: 'circle_equation',
    label: '圆的方程 / Circle Equation',
    code: `// 以原点为圆心、半径为 3 的圆
point O(0, 0)
point R(3, 0)
circle center(O) radius(R)
measure distance O, R`,
  },
  {
    id: 'pythagorean',
    label: '勾股定理 / Pythagorean',
    code: `// 直角三角形 3-4-5
point A(0, 0)
point B(4, 0)
point C(0, 3)
segment AB
segment BC
segment CA
measure distance A, B
measure distance B, C
measure distance C, A
measure angle B, A, C`,
  },
  {
    id: 'midpoint',
    label: '中垂线 / Midpoint',
    code: `// 中点与中垂线
point A(0, 0)
point B(6, 0)
midpoint M of A, B
segment AB
measure distance A, M
measure distance M, B`,
  },
  {
    id: 'line_equation',
    label: '直线方程 / Line Equation',
    code: `// 直线上的点
point A(1, 1)
point B(5, 3)
segment AB
measure distance A, B`,
  },
  {
    id: 'triangle_area',
    label: '三角形面积 / Triangle Area',
    code: `// 三角形（用底和高估算面积）
point A(0, 0)
point B(6, 0)
point C(3, 4)
segment AB
segment BC
segment CA
measure distance A, B
measure distance A, C`,
  },
  {
    id: 'distance',
    label: '两点距离 / Distance',
    code: `// 计算两点之间的距离
point P(1, 2)
point Q(4, 6)
segment PQ
measure distance P, Q`,
  },
  {
    id: 'intersection',
    label: '交点 / Intersection',
    code: `// 两条线段的交点
point A(0, 0)
point B(4, 4)
point C(0, 4)
point D(4, 0)
segment AB
segment CD
intersect segment AB with CD`,
  },
];

/**
 * FormulaPanel - 公式模块侧边栏面板
 *
 * 面板分区:
 * - INPUT   : 公式文本输入 + 语法模式选择
 * - CONVERT : 公式 <-> 图形转换 + 输出格式选择
 * - OUTPUT  : 解析/渲染结果展示
 * - EXAMPLES: 预设公式示例快速加载
 * - ACTIONS : 清空、导出按钮
 * - LOG     : 公式操作日志
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
    setLogEntries((prev) => [...prev.slice(-MAX_PANEL_LOG_ENTRIES), { time: now, msg: 'PARSE: 开始解析', id: generateId() }]);

    // 调用 DSL 解析器
    const result = parseFormula(formulaInput);

    // 构建输出文本
    const lines: string[] = [];
    if (result.errors.length > 0) {
      lines.push('=== 解析错误 / Parse Errors ===');
      result.errors.forEach((err) => lines.push(`  [ERROR] ${err}`));
      lines.push('');
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
      { time: now, msg: `PARSE: ${validCount} 有效, ${result.errors.length} 错误`, id: generateId() },
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
    setLogEntries((prev) => [...prev.slice(-MAX_PANEL_LOG_ENTRIES), { time: now, msg: 'RENDER: 开始执行', id: generateId() }]);

    // 保存撤销状态
    saveUndoState();

    // 解析并执行
    const result = parseAndExecuteFormula(formulaInput, points);

    // 将结果添加到 Store
    result.createdPoints.forEach((p) => addPoint(p));
    result.createdSegments.forEach((s) => addSegment(s));
    result.createdConstraints.forEach((c) => addConstraint(c));

    // 构建输出文本
    const lines: string[] = [];

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
        id: generateId(),
      },
    ]);
    addToast(
      result.errors.length > 0 ? 'warning' : 'success',
      `渲染完成: ${result.createdPoints.length} 点, ${result.createdSegments.length} 线段, ${result.createdConstraints.length} 约束`,
    );
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
    setLogEntries((prev) => [...prev.slice(-MAX_PANEL_LOG_ENTRIES), { time: now, msg: 'F->G: 开始转换', id: generateId() }]);

    saveUndoState();
    const result = parseAndExecuteFormula(formulaInput, points);

    result.createdPoints.forEach((p) => addPoint(p));
    result.createdSegments.forEach((s) => addSegment(s));
    result.createdConstraints.forEach((c) => addConstraint(c));

    let summary = `公式转图形: 创建 ${result.createdPoints.length} 点, ${result.createdSegments.length} 线段, ${result.createdConstraints.length} 约束`;
    if (result.errors.length > 0) {
      summary += ` (${result.errors.length} 个错误)`;
    }

    // 构建输出
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
    setLogEntries((prev) => [...prev.slice(-MAX_PANEL_LOG_ENTRIES), { time: now, msg: `F->G: 完成`, id: generateId() }]);
    addToast(result.errors.length > 0 ? 'warning' : 'success', summary);
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
      { time: now, msg: `G->F: ${points.length} 点, ${segments.length} 线段`, id: generateId() },
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
      const example = FORMULA_EXAMPLES.find((e) => e.id === exampleId);
      if (example) {
        setFormulaInput(example.code);
        addToast('info', `加载示例: ${example.label}`);
      }
    },
    [setFormulaInput, addToast],
  );

  // ================================================================
  // 渲染
  // ================================================================
  return (
    <>
      {/* INPUT 区域：公式文本输入 + 语法模式选择 */}
      <Panel title="INPUT / 输入" panelId="formula-input">
        {/* 语法模式选择器：控制后端解析器的语法解析策略 */}
        <div className="formula-syntax-row">
          <label>语法 / Syntax</label>
          <select
            className="select-field"
            value={formulaSyntax}
            onChange={(e) => setFormulaSyntax(e.target.value as FormulaSyntax)}
          >
            <option value="auto">AUTO / 自动</option>
            <option value="dsl">DSL / 术式</option>
            <option value="latex">LaTeX</option>
            <option value="python">Python</option>
          </select>
        </div>
        {/* 公式输入框：双向绑定至 uiStore.formulaInput */}
        <textarea
          className="input-field"
          id="formulaInput"
          placeholder={`输入几何 DSL 命令 / Enter geometry DSL...&#10;例如:&#10;point A(0, 0)&#10;point B(4, 0)&#10;segment AB&#10;midpoint M of A, B&#10;measure distance A, B`}
          aria-label="数学公式输入框"
          value={formulaInput}
          onChange={(e) => setFormulaInput(e.target.value)}
          rows={6}
        />
        {/* 操作按钮行：解析与渲染 */}
        <div className="formula-btn-row">
          <button className="btn btn-accent" onClick={handleParse}>
            PARSE / 解析
          </button>
          <button className="btn btn-accent" onClick={handleRender}>
            RENDER / 渲染
          </button>
        </div>
      </Panel>

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
        <div className="panel-separator" />
        {/* 输出格式选择：控制转换结果的目标语法 */}
        <div className="input-group-row">
          <label>输出 / Output</label>
          <select
            className="select-field"
            value={formulaOutputFormat}
            onChange={(e) => setFormulaOutputFormat(e.target.value as FormulaOutputFormat)}
          >
            <option value="latex">LaTeX</option>
            <option value="python">Python</option>
            <option value="dsl">DSL / 术式</option>
          </select>
        </div>
      </Panel>

      {/* OUTPUT 区域：解析/渲染结果展示 */}
      <Panel title="OUTPUT / 输出" panelId="formula-output">
        <div className="formula-output-area" id="formulaOutput">
          {outputText ? (
            <div className="formula-code-block">
              <pre style={{ margin: 0, whiteSpace: 'pre-wrap', fontSize: '11px' }}>
                {outputText}
              </pre>
            </div>
          ) : (
            <div className="formula-output-placeholder">
              等待输入公式... / Waiting for formula input...
            </div>
          )}
        </div>
      </Panel>

      {/* EXAMPLES 区域：预设公式示例列表 */}
      <Panel title="EXAMPLES / 示例" panelId="formula-examples">
        <ul className="examples-list" id="formulaExamplesList">
          {FORMULA_EXAMPLES.map((ex) => (
            <li
              key={ex.id}
              data-example={ex.id}
              role="button"
              tabIndex={0}
              onClick={() => handleExampleClick(ex.id)}
              onKeyDown={(e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); handleExampleClick(ex.id); } }}
            >
              {ex.label}
            </li>
          ))}
        </ul>
      </Panel>

      {/* ACTIONS 区域：全局操作按钮 */}
      <Panel title="ACTIONS / 操作" panelId="formula-actions">
        <button className="btn" onClick={handleClear}>
          CLEAR / 清空
        </button>
        <button
          className="btn"
          onClick={() => {
            if (!outputText) {
              addToast('warning', '没有可导出的内容 / Nothing to export');
              return;
            }
            // 复制到剪贴板
            copyToClipboard(outputText, addToast);
          }}
        >
          EXPORT / 导出
        </button>
      </Panel>

      {/* LOG 区域：公式操作日志 */}
      <Panel title="LOG / 日志" panelId="formula-log">
        <div className="formula-log-area" id="formulaLog">
          {logEntries.length === 0 ? (
            <div className="log-empty-msg">暂无日志 / No logs</div>
          ) : (
            logEntries.map((entry) => (
              <div key={entry.id} className="formula-log-entry">
                <span className="formula-log-time">{entry.time}</span>
                <span className="formula-log-msg">{entry.msg}</span>
              </div>
            ))
          )}
        </div>
      </Panel>
    </>
  );
};

export default FormulaPanel;
