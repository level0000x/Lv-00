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

import React, { useState, useCallback, useEffect, useRef } from 'react';
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
  // 新增本地状态 —— Live Preview / 同步 / 历史 / 变量
  // ================================================================
  const [livePreviewEnabled, setLivePreviewEnabled] = useState<boolean>(false);
  const [syncEnabled, setSyncEnabled] = useState<boolean>(false);
  const [syncStatus, setSyncStatus] = useState<'idle' | 'syncing' | 'synced'>('idle');
  const [previewResult, setPreviewResult] = useState<{
    commands: number;
    validCount: number;
    errors: string[];
    points: Array<{ label: string; x: number; y: number; isSymbolic: boolean; exprX?: string; exprY?: string }>;
    parsedCommands: Array<{ raw: string; type: string; valid: boolean; error?: string; lineNum: number; description?: string }>;
  } | null>(null);
  const [detectedVariables, setDetectedVariables] = useState<string[]>([]);
  const [formulaHistory, setFormulaHistory] = useState<string[]>([]);
  const [historyIndex, setHistoryIndex] = useState<number>(-1);
  const debounceRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const lastSyncedFormulaRef = useRef<string>('');
  const isExecutingFormula = useRef(false);

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
      result.errors.forEach((err) => {
        // 尝试解析错误格式：提取行号和内容
        const lineMatch = err.match(/^(?:line\s*)?(\d+)[:;]?\s*(.*)/i);
        if (lineMatch) {
          const errLine = parseInt(lineMatch[1], 10);
          const errMsg = lineMatch[2].trim();
          // 获取对应行的原始文本
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

    // 标记公式正在执行，防止自动同步写入死循环
    isExecutingFormula.current = true;

    // 解析并执行
    try {
      const result = parseAndExecuteFormula(formulaInput, points);

      // 将结果添加到 Store
      result.createdPoints.forEach((p) => addPoint(p));
      result.createdSegments.forEach((s) => addSegment(s));
      result.createdConstraints.forEach((c) => addConstraint(c));

      // 构建输出文本
      const lines: string[] = [];

      // 当没有错误时，显示摘要卡片
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
          id: generateId(),
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
    setLogEntries((prev) => [...prev.slice(-MAX_PANEL_LOG_ENTRIES), { time: now, msg: 'F->G: 开始转换', id: generateId() }]);

    saveUndoState();

    // 标记公式正在执行，防止自动同步写入死循环
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
        pushToHistoryAndSet(example.code);
        addToast('info', `加载示例: ${example.label}`);
      }
    },
    [addToast],
  );

  // ================================================================
  // 公式历史管理
  // ================================================================

  /** 将当前公式入栈并设置新值。返回新值（用于 caller 复用） */
  const pushToHistoryAndSet = useCallback(
    (newValue: string): string => {
      setFormulaHistory((prev) => {
        const trimmed = prev.slice(0, 10);
        if (trimmed.length > 0 && trimmed[trimmed.length - 1] === newValue) return trimmed;
        return [...trimmed, newValue].slice(-10);
      });
      setHistoryIndex(-1);
      setFormulaInput(newValue);
      return newValue;
    },
    [setFormulaInput],
  );

  /** UNDO FORMULA */
  const handleUndoFormula = useCallback(() => {
    setFormulaHistory((prev) => {
      if (prev.length === 0) {
        addToast('warning', '没有可撤销的公式版本 / No formula versions to undo');
        return prev;
      }
      // 如果当前不在历史中回溯，先把当前值入栈
      let stack = prev;
      const currentFormula = formulaInput;
      if (historyIndex === -1 && currentFormula !== '') {
        stack = [...prev, currentFormula].slice(-10);
      }
      const newIdx = historyIndex === -1 ? stack.length - 2 : historyIndex - 1;
      if (newIdx < 0) {
        addToast('warning', '已到达最早版本 / Reached oldest version');
        return stack;
      }
      setHistoryIndex(newIdx);
      setFormulaInput(stack[newIdx]);
      addToast('info', `已撤销至 v${newIdx + 1}/${stack.length} / Undone to v${newIdx + 1}/${stack.length}`);
      return stack;
    });
  }, [formulaInput, historyIndex, setFormulaInput, addToast]);

  /** REDO FORMULA */
  const handleRedoFormula = useCallback(() => {
    setFormulaHistory((prev) => {
      if (prev.length === 0 || historyIndex >= prev.length - 1) {
        addToast('warning', '没有可重做的公式版本 / No formula versions to redo');
        return prev;
      }
      const newIdx = historyIndex + 1;
      setHistoryIndex(newIdx);
      setFormulaInput(prev[newIdx]);
      addToast('info', `已重做至 v${newIdx + 1}/${prev.length} / Redone to v${newIdx + 1}/${prev.length}`);
      return prev;
    });
  }, [historyIndex, setFormulaInput, addToast]);

  // ================================================================
  // Live Preview —— Kingdon "输入即所见"
  // ================================================================

  /** 检测公式中的符号变量 */
  const detectSymbolicVars = useCallback((text: string): string[] => {
    const varSet = new Set<string>();
    // 匹配 point Label(expr, expr) 中的变量（非数字 token）
    const pointRe = /point\s+(\w+)\s*\(\s*([^,]+)\s*,\s*([^)]+)\s*\)/gi;
    let m: RegExpExecArray | null;
    while ((m = pointRe.exec(text)) !== null) {
      const px = m[2].trim();
      const py = m[3].trim();
      [px, py].forEach((expr) => {
        const tokens = expr.match(/[a-zA-Z_]\w*/g);
        if (tokens) tokens.forEach((t) => { if (!['sin','cos','tan','sqrt','abs','PI','pi','E','e'].includes(t)) varSet.add(t); });
      });
    }
    return Array.from(varSet);
  }, []);

  /** 尝试将 DSL 点坐标中的单个字母变量解析为符号变量 */
  const parsePointWithVars = useCallback((raw: string): {
    label: string;
    x: number;
    y: number;
    isSymbolic: boolean;
    exprX?: string;
    exprY?: string;
  } | null => {
    const m = raw.match(/point\s+(\w+)\s*\(\s*([^,]+)\s*,\s*([^)]+)\s*\)/i);
    if (!m) return null;
    const label = m[1];
    const sx = m[2].trim();
    const sy = m[3].trim();
    const nx = Number(sx);
    const ny = Number(sy);
    if (!isNaN(nx) && !isNaN(ny)) {
      return { label, x: nx, y: ny, isSymbolic: false };
    }
    // 符号变量 —— 尝试简单表达式求值，回退为 0
    let xVal = 0, yVal = 0;
    try {
      // 安全求值：仅支持 a+b, a-b, a*N, a/N 等简单形式，这里保守地取 0
      xVal = 0;
      yVal = 0;
    } catch { /* ignore */ }
    return { label, x: xVal, y: yVal, isSymbolic: true, exprX: sx, exprY: sy };
  }, []);

  /** 构造 mini SVG 预览（200x150） */
  const buildMiniSvg = useCallback(
    (
      pts: Array<{ label: string; x: number; y: number }>,
    ): string => {
      if (pts.length === 0) return '';
      const W = 200, H = 150, PAD = 20;
      const xs = pts.map((p) => p.x);
      const ys = pts.map((p) => p.y);
      const minX = Math.min(...xs);
      const maxX = Math.max(...xs);
      const minY = Math.min(...ys);
      const maxY = Math.max(...ys);
      const rangeX = maxX - minX || 1;
      const rangeY = maxY - minY || 1;
      const scaleX = (W - 2 * PAD) / rangeX;
      const scaleY = (H - 2 * PAD) / rangeY;
      const scale = Math.min(scaleX, scaleY);
      const cx = (minX + maxX) / 2;
      const cy = (minY + maxY) / 2;
      const tx = (px: number) => W / 2 + (px - cx) * scale;
      const ty = (py: number) => H / 2 - (py - cy) * scale;
      let svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${W}" height="${H}" style="background:#fafafa;border:1px solid #ccc;border-radius:4px;">`;
      // 网格
      svg += `<line x1="${PAD}" y1="${H/2}" x2="${W-PAD}" y2="${H/2}" stroke="#e0e0e0" stroke-width="0.5"/>`;
      svg += `<line x1="${W/2}" y1="${PAD}" x2="${W/2}" y2="${H-PAD}" stroke="#e0e0e0" stroke-width="0.5"/>`;
      // 点
      pts.forEach((p) => {
        svg += `<circle cx="${tx(p.x)}" cy="${ty(p.y)}" r="3" fill="#4a90d9" stroke="#2c5f8a" stroke-width="0.5"/>`;
        svg += `<text x="${tx(p.x)+4}" y="${ty(p.y)-2}" font-size="8" fill="#333" font-family="monospace">${p.label}</text>`;
      });
      // 线段（按顺序连）
      for (let i = 1; i < pts.length; i++) {
        svg += `<line x1="${tx(pts[i-1].x)}" y1="${ty(pts[i-1].y)}" x2="${tx(pts[i].x)}" y2="${ty(pts[i].y)}" stroke="#888" stroke-width="0.8"/>`;
      }
      svg += `</svg>`;
      return svg;
    },
    [],
  );

  /** 防抖实时预览 —— 解析公式并更新 previewResult */
  const runLivePreview = useCallback(
    (text: string) => {
      if (!text.trim()) {
        setPreviewResult(null);
        setDetectedVariables([]);
        return;
      }
      const result = parseFormula(text);
      const validCount = result.commands.filter((c) => c.type !== 'comment' && !c.error).length;
      const pts: Array<{ label: string; x: number; y: number; isSymbolic: boolean; exprX?: string; exprY?: string }> = [];

      // 生成每条命令的描述文本
      const commandDescriptions: Record<string, (raw: string) => string> = {
        point: (raw) => {
          const m = raw.match(/point\s+(\w+)\s*\(\s*([^,]+)\s*,\s*([^)]+)\s*\)/i);
          return m ? `\u5C06\u521B\u5EFA\u70B9 (${m[2].trim()},${m[3].trim()})` : '\u5C06\u521B\u5EFA\u70B9';
        },
        segment: () => '\u5C06\u521B\u5EFA\u7EBF\u6BB5 / will create a segment',
        circle: () => '\u5C06\u521B\u5EFA\u5706 / will create a circle',
        midpoint: () => '\u5C06\u521B\u5EFA\u4E2D\u70B9 / will create a midpoint',
        perpendicular: () => '\u5C06\u521B\u5EFA\u5782\u7EBF / will create a perpendicular line',
        parallel: () => '\u5C06\u521B\u5EFA\u5E73\u884C\u7EBF / will create a parallel line',
        intersect: () => '\u5C06\u521B\u5EFA\u4EA4\u70B9 / will create an intersection',
        measure: (raw) => raw.includes('distance') ? '\u5C06\u6D4B\u91CF\u8DDD\u79BB / will measure distance' : '\u5C06\u6D4B\u91CF\u89D2\u5EA6 / will measure angle',
      };

      const parsedCommands: Array<{ raw: string; type: string; valid: boolean; error?: string; lineNum: number; description?: string }> = [];
      result.commands.forEach((cmd, idx) => {
        if (cmd.type === 'comment') return;
        if (cmd.type === 'point' && !cmd.error) {
          const parsed = parsePointWithVars(cmd.raw);
          if (parsed) pts.push(parsed);
        }
        const descFn = commandDescriptions[cmd.type];
        const desc = descFn ? descFn(cmd.raw) : undefined;
        parsedCommands.push({
          raw: cmd.raw,
          type: cmd.type,
          valid: !cmd.error,
          error: cmd.error,
          lineNum: idx + 1,
          description: desc,
        });
      });

      setPreviewResult({
        commands: result.commands.length,
        validCount,
        errors: result.errors,
        points: pts,
        parsedCommands,
      });
      const vars = detectSymbolicVars(text);
      setDetectedVariables(vars);
    },
    [parsePointWithVars, detectSymbolicVars],
  );

  /** 输入框 onChange 带防抖 */
  const handleFormulaChange = useCallback(
    (e: React.ChangeEvent<HTMLTextAreaElement>) => {
      const val = e.target.value;
      setFormulaInput(val);
      if (livePreviewEnabled) {
        if (debounceRef.current) clearTimeout(debounceRef.current);
        debounceRef.current = setTimeout(() => runLivePreview(val), 300);
      }
    },
    [setFormulaInput, livePreviewEnabled, runLivePreview],
  );

  /** 切换实时预览开关 —— 开启时立即预览 */
  const handleToggleLivePreview = useCallback(() => {
    setLivePreviewEnabled((prev) => {
      const next = !prev;
      if (next && formulaInput.trim()) {
        runLivePreview(formulaInput);
      } else if (!next) {
        setPreviewResult(null);
        setDetectedVariables([]);
      }
      return next;
    });
  }, [formulaInput, runLivePreview]);

  // ================================================================
  // 双向同步
  // ================================================================

  /** 同步开关切换 */
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

  /** 当 syncEnabled 且 points/segments/constraints 变化时自动同步 */
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

  // ================================================================
  // 输出格式转换器
  // ================================================================

  /** DSL -> LaTeX 转换 */
  const toLatex = useCallback((dsl: string): string => {
    const lines: string[] = [];
    const result = parseFormula(dsl);
    result.commands.forEach((cmd) => {
      if (cmd.type === 'comment') {
        lines.push(cmd.raw);
        return;
      }
      if (cmd.error) {
        lines.push(`% ERROR: ${cmd.error}`);
        return;
      }
      const m = cmd.raw.match(/^(\w+)\s+(.+)/);
      if (!m) { lines.push(cmd.raw); return; }
      const op = m[1].toLowerCase();
      const rest = m[2].trim();
      switch (op) {
        case 'point': {
          const pm = rest.match(/(\w+)\s*\(\s*([^,]+)\s*,\s*([^)]+)\s*\)/i);
          if (pm) lines.push(`% \\coordinate (${pm[1]}) at (${pm[2].trim()},${pm[3].trim()});`);
          else lines.push(cmd.raw);
          break;
        }
        case 'segment': {
          const sm = rest.match(/(\w+)\s*(\w+)/i);
          if (sm) lines.push(`% \\draw (${sm[1]}) -- (${sm[2]});`);
          else lines.push(cmd.raw);
          break;
        }
        case 'circle': {
          lines.push(`% \\draw ${rest};`);
          break;
        }
        case 'midpoint': {
          const mm = rest.match(/(\w+)\s+of\s+(\w+)\s*,\s*(\w+)/i);
          if (mm) lines.push(`% (${mm[1]}) at midpoint of (${mm[2]}) and (${mm[3]});`);
          else lines.push(cmd.raw);
          break;
        }
        case 'measure': {
          lines.push(`% \\tkzCalcLength for ${rest}`);
          break;
        }
        case 'intersect': {
          lines.push(`% \\tkzInterLL for ${rest}`);
          break;
        }
        default:
          lines.push(`% ${cmd.raw}`);
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
      if (cmd.type === 'comment') {
        lines.push(cmd.raw);
        return;
      }
      if (cmd.error) {
        lines.push(`# ERROR: ${cmd.error}`);
        return;
      }
      const m = cmd.raw.match(/^(\w+)\s+(.+)/);
      if (!m) { lines.push(cmd.raw); return; }
      const op = m[1].toLowerCase();
      const rest = m[2].trim();
      switch (op) {
        case 'point': {
          const pm = rest.match(/(\w+)\s*\(\s*([^,]+)\s*,\s*([^)]+)\s*\)/i);
          if (pm) lines.push(`${pm[1]} = sp.Point(${pm[2].trim()}, ${pm[3].trim()})`);
          else lines.push(cmd.raw);
          break;
        }
        case 'segment': {
          const sm = rest.match(/(\w+)\s*(\w+)/i);
          if (sm) lines.push(`${sm[1]}${sm[2]} = sp.Segment(${sm[1]}, ${sm[2]})`);
          else lines.push(cmd.raw);
          break;
        }
        case 'circle': {
          lines.push(`# circle: ${rest}`);
          break;
        }
        case 'midpoint': {
          const mm = rest.match(/(\w+)\s+of\s+(\w+)\s*,\s*(\w+)/i);
          if (mm) lines.push(`${mm[1]} = sp.Point((${mm[2]}.x + ${mm[3]}.x) / 2, (${mm[2]}.y + ${mm[3]}.y) / 2)`);
          else lines.push(cmd.raw);
          break;
        }
        case 'measure':
        case 'intersect':
        default:
          lines.push(`# ${cmd.raw}`);
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
      // DSL 模式不改动
      if (newFormat !== 'dsl' && converted !== formulaInput) {
        pushToHistoryAndSet(converted);
        addToast('info', `已转换为 ${newFormat.toUpperCase()} 格式 / Converted to ${newFormat.toUpperCase()}`);
      }
    },
    [formulaInput, toLatex, toPython, pushToHistoryAndSet, setFormulaOutputFormat, addToast],
  );

  // ================================================================
  // SOLVE —— 符号变量约束求解（占位）
  // ================================================================

  const handleSolve = useCallback(() => {
    if (detectedVariables.length === 0) {
      addToast('warning', '没有检测到需要求解的变量 / No variables detected to solve');
      return;
    }
    const vars = detectedVariables.join(', ');
    addToast('info', `变量已检测: ${vars}。求解功能待实现 / Variables detected: ${vars}. Solver pending.`);
    setOutputText(`=== 符号变量 / Symbolic Variables ===\n\n检测到变量: ${vars}\n\n正在尝试约束求解...\n(Solver engine is a placeholder — full constraint solving requires backend integration)\n\n建议：为变量指定具体数值，或使用已知点坐标表达式。`);
  }, [detectedVariables, addToast]);

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
        {/* 公式输入框：双向绑定至 uiStore.formulaInput，带防抖实时预览 */}
        <textarea
          className="input-field"
          id="formulaInput"
          placeholder={`输入几何 DSL 命令 / Enter geometry DSL...&#10;例如:&#10;point A(0, 0)&#10;point B(4, 0)&#10;segment AB&#10;midpoint M of A, B&#10;measure distance A, B`}
          aria-label="数学公式输入框"
          value={formulaInput}
          onChange={handleFormulaChange}
          rows={6}
        />

        {/* Live Preview 开关 */}
        <div className="formula-syntax-row" style={{ marginTop: 4 }}>
          <label>LIVE PREVIEW / 实时预览</label>
          <button
            className={`btn ${livePreviewEnabled ? 'btn-accent' : ''}`}
            onClick={handleToggleLivePreview}
            style={{ fontSize: '11px', padding: '2px 8px' }}
          >
            {livePreviewEnabled ? '\u{1F7E2} LIVE ON / \u5B9E\u65F6\u9884\u89C8\u5F00' : '\u{1F534} LIVE OFF / \u5B9E\u65F6\u9884\u89C8\u5173'}
          </button>
        </div>

        {/* 实时预览 —— 逐条命令详情 */}
        {livePreviewEnabled && previewResult && (
          <div
            style={{
              marginTop: 6,
              border: '1px solid #d0d0d0',
              borderRadius: 4,
              padding: '6px 8px',
              backgroundColor: '#fafafa',
              fontSize: '11px',
              fontFamily: 'Consolas, Monaco, "Courier New", monospace',
              maxHeight: 200,
              overflowY: 'auto',
            }}
          >
            <div style={{ fontWeight: 600, marginBottom: 4, color: '#333', fontSize: '12px' }}>
              {'\uD83D\uDCD0'} {'\u5B9E\u65F6\u9884\u89C8 / Live Preview:'}
            </div>
            {previewResult.parsedCommands.length === 0 ? (
              <div style={{ color: '#999', fontStyle: 'italic' }}>{'\u6682\u65E0\u547D\u4EE4 / No commands'}</div>
            ) : (
              previewResult.parsedCommands.map((cmd, i) => (
                <div
                  key={i}
                  style={{
                    color: cmd.valid ? '#2e7d32' : '#d32f2f',
                    lineHeight: 1.6,
                    whiteSpace: 'nowrap',
                    overflow: 'hidden',
                    textOverflow: 'ellipsis',
                  }}
                >
                  {cmd.valid ? (
                    <span>
                      {'  \u2713 '}
                      <span style={{ fontWeight: 600 }}>{cmd.type}</span>
                      {' '}
                      <span>{cmd.raw.replace(/^(\w+)\s+/, '')}</span>
                      {cmd.description && (
                        <span style={{ color: '#666', marginLeft: 4 }}>
                          {'\u2192'} {cmd.description}
                        </span>
                      )}
                    </span>
                  ) : (
                    <span>
                      {'  \u26A0 line '}{cmd.lineNum}{': '}{cmd.error}
                    </span>
                  )}
                </div>
              ))
            )}
            <div
              style={{
                marginTop: 4,
                paddingTop: 4,
                borderTop: '1px solid #e0e0e0',
                color: '#555',
                fontSize: '10px',
                display: 'flex',
                gap: 12,
              }}
            >
              <span>{'\u2713'} {previewResult.validCount} valid</span>
              <span>{'\u26A0'} {previewResult.errors.length} errors</span>
              <span>{'\uD83D\uDCCD'} {previewResult.points.length} points</span>
            </div>
          </div>
        )}

        {/* Mini SVG 预览画布 */}
        {livePreviewEnabled && previewResult && previewResult.points.length > 0 && (
          <div style={{ marginTop: 6, textAlign: 'center' }}>
            <div
              className="formula-svg-preview"
              dangerouslySetInnerHTML={{
                __html: buildMiniSvg(
                  previewResult.points.map((p) => ({ label: p.label, x: p.x, y: p.y })),
                ),
              }}
            />
          </div>
        )}

        {/* 符号变量列表 */}
        {detectedVariables.length > 0 && (
          <div className="info-box" style={{ marginTop: 4, fontSize: '11px' }}>
            <div className="info-row">
              <span>Variables / 变量:</span>
              <span>{detectedVariables.join(', ')}</span>
            </div>
            <button
              className="btn"
              onClick={handleSolve}
              style={{ fontSize: '11px', padding: '2px 8px', marginTop: 4 }}
            >
              SOLVE FOR / 求解
            </button>
          </div>
        )}
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
        {/* 公式历史：撤销/重做 */}
        <div className="formula-btn-row">
          <button
            className="btn"
            onClick={handleUndoFormula}
            disabled={formulaHistory.length === 0 || (historyIndex === -1 && formulaHistory.length === 0)}
            style={{ fontSize: '11px' }}
          >
            UNDO FORMULA / 撤销公式
          </button>
          <button
            className="btn"
            onClick={handleRedoFormula}
            disabled={historyIndex >= formulaHistory.length - 1 || historyIndex === -1}
            style={{ fontSize: '11px' }}
          >
            REDO FORMULA / 重做公式
          </button>
        </div>
        {formulaHistory.length > 0 && (
          <div className="info-box" style={{ marginTop: 4, fontSize: '10px', textAlign: 'center' }}>
            v{historyIndex === -1 ? formulaHistory.length : historyIndex + 1}/{formulaHistory.length}
          </div>
        )}
        <div className="panel-separator" />
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

      {/* SYNTAX GUIDE 区域：DSL 语法速查 */}
      <Panel title="SYNTAX GUIDE / 语法参考" panelId="formula-syntax-guide">
        <details style={{ fontSize: '11px' }}>
          <summary style={{ cursor: 'pointer', fontWeight: 600, color: '#4a90d9', userSelect: 'none' }}>
            {'\u25B6 \u70B9\u51FB\u5C55\u5F00 DSL \u8BED\u6CD5\u53C2\u8003 / Click to expand DSL syntax reference'}
          </summary>
          <div style={{
            marginTop: 6,
            padding: '6px 8px',
            backgroundColor: '#f5f5f5',
            borderRadius: 4,
            border: '1px solid #e0e0e0',
            fontFamily: 'Consolas, Monaco, "Courier New", monospace',
            lineHeight: 1.8,
            maxHeight: 260,
            overflowY: 'auto',
          }}>
            <div style={{ marginBottom: 2, color: '#333' }}>
              <code style={{ color: '#4a90d9' }}>point NAME(x, y)</code>
              <span style={{ color: '#999' }}> - {'\u521B\u5EFA\u4E00\u4E2A\u70B9 / create a point'}</span>
            </div>
            <div style={{ marginBottom: 2, color: '#333' }}>
              <code style={{ color: '#4a90d9' }}>segment NAME</code>{' '}
              <span style={{ color: '#999' }}>{'\u6216 / or'}</span>{' '}
              <code style={{ color: '#4a90d9' }}>segment(A, B)</code>
              <span style={{ color: '#999' }}> - {'\u521B\u5EFA\u4E00\u6761\u7EBF\u6BB5 / create a segment'}</span>
            </div>
            <div style={{ marginBottom: 2, color: '#333' }}>
              <code style={{ color: '#4a90d9' }}>circle center(A) radius(r)</code>
              <span style={{ color: '#999' }}> - {'\u521B\u5EFA\u4E00\u4E2A\u5706 / create a circle'}</span>
            </div>
            <div style={{ marginBottom: 2, color: '#333' }}>
              <code style={{ color: '#4a90d9' }}>midpoint M of A, B</code>
              <span style={{ color: '#999' }}> - {'\u521B\u5EFA\u4E2D\u70B9 / create midpoint'}</span>
            </div>
            <div style={{ marginBottom: 2, color: '#333' }}>
              <code style={{ color: '#4a90d9' }}>perpendicular from A to segment BC</code>
              <span style={{ color: '#999' }}> - {'\u521B\u5EFA\u5782\u7EBF / perpendicular line'}</span>
            </div>
            <div style={{ marginBottom: 2, color: '#333' }}>
              <code style={{ color: '#4a90d9' }}>parallel to AB through C</code>
              <span style={{ color: '#999' }}> - {'\u521B\u5EFA\u5E73\u884C\u7EBF / parallel line'}</span>
            </div>
            <div style={{ marginBottom: 2, color: '#333' }}>
              <code style={{ color: '#4a90d9' }}>intersect segment AB with CD</code>
              <span style={{ color: '#999' }}> - {'\u6C42\u4EA4\u70B9 / find intersection'}</span>
            </div>
            <div style={{ marginBottom: 2, color: '#333' }}>
              <code style={{ color: '#4a90d9' }}>measure distance A, B</code>
              <span style={{ color: '#999' }}> - {'\u6D4B\u91CF\u8DDD\u79BB / measure distance'}</span>
            </div>
            <div style={{ marginBottom: 2, color: '#333' }}>
              <code style={{ color: '#4a90d9' }}>measure angle A, B, C</code>
              <span style={{ color: '#999' }}> - {'\u6D4B\u91CF\u89D2\u5EA6 / measure angle'}</span>
            </div>
          </div>
        </details>
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
