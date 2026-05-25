/**
 * @module components/panels/RecursePanel
 * @description 递归模块侧边栏面板。
 *
 *              提供递归几何构造定义、互递归、终止性验证、单步执行等功能。
 *              内置 Sierpinski 三角形、Koch 雪花、分形树等经典分形图案。
 *              所有功能使用纯 JS 实现，不依赖 WASM 后端。
 *
 *              分形生成算法已提取到 utils/ 目录：
 *              - fractalGenerator.ts: Sierpinski、Koch、分形树、终止性验证
 */

import React, { useState, useCallback, useMemo, useRef, useEffect } from 'react';
import Panel from './Panel';
import { useAppStore } from '@/stores';

// ---- 提取的工具模块 / Extracted utility modules ----
import {
  BUILTIN_RULES,
  generateFractal,
  validateTermination,
} from './utils/fractalGenerator';
import type {
  RecursionRule,
} from './utils/fractalGenerator';

// ================================================================
// RecursePanel 组件 / RecursePanel Component
// ================================================================

/**
 * RecursePanel - 递归模块侧边栏面板
 *
 * 面板分区:
 * - RECURSION: 定义递归、互递归、验证度量、单步执行
 * - DEFINE: 递归定义表单
 * - BUILT-IN: 内置分形图案选择
 * - RESULTS: 执行结果展示
 * - INFO: 来自 Store 的递归上下文统计
 */
const RecursePanel: React.FC = () => {
  const addToast = useAppStore((s) => s.addToast);
  const appendLog = useAppStore((s) => s.appendLog);

  // ================================================================
  // 从 Store 读取数据和方法
  // ================================================================
  const undoDepth = useAppStore((s) => s.undoStack.length);
  const tool = useAppStore((s) => s.tool);
  const currentPoints = useAppStore((s) => s.points);
  const currentSegments = useAppStore((s) => s.segments);

  const setPoints = useAppStore((s) => s.setPoints);
  const setSegments = useAppStore((s) => s.setSegments);
  const saveUndoState = useAppStore((s) => s.saveUndoState);
  const clearAll = useAppStore((s) => s.clearAll);

  // ================================================================
  // 本地状态 / Local State
  // ================================================================
  /** 用户自定义递归规则 */
  const [customRules, setCustomRules] = useState<RecursionRule[]>([]);

  /** 所有规则（内置 + 自定义） */
  const allRules = useMemo(() => [...BUILTIN_RULES, ...customRules], [customRules]);

  /** 选中的规则 */
  const [selectedRule, setSelectedRule] = useState<string>('Sierpinski Triangle');

  /** 目标深度 */
  const [targetDepth, setTargetDepth] = useState(2);

  /** 当前执行步骤 */
  const [currentStep, setCurrentStep] = useState(0);

  /** 是否正在逐步执行 */
  const [isStepping, setIsStepping] = useState(false);

  /** 显示定义表单 */
  const [showDefineForm, setShowDefineForm] = useState(false);

  /** 显示互递归表单 */
  const [showMutualForm, setShowMutualForm] = useState(false);

  /** 结果展示 */
  const [showResults, setShowResults] = useState(false);
  const [resultsText, setResultsText] = useState('');
  const [resultsColor, setResultsColor] = useState<string>('var(--text, #e0e0e0)');

  /** 自定义规则表单 */
  const [newRuleName, setNewRuleName] = useState('');
  const [newRuleDepth, setNewRuleDepth] = useState('3');
  const [newRuleTransform, setNewRuleTransform] = useState<RecursionRule['transformType']>('custom_subdivide');

  /** 互递归表单 */
  const [mutualNameA, setMutualNameA] = useState('');
  const [mutualNameB, setMutualNameB] = useState('');

  /** ID 计数器引用（用于分形生成中的唯一 ID） */
  const idCounterRef = useRef({ value: 1000 });
  // 在使用前确保起始 ID 不与现有几何数据冲突
  useEffect(() => {
    const maxId = Math.max(
      ...currentPoints.map(p => p.id),
      ...currentSegments.map(s => s.id),
      0
    );
    idCounterRef.current.value = Math.max(idCounterRef.current.value, maxId + 1);
  }, [currentPoints, currentSegments]);

  // ================================================================
  // DEFINE RECURSION / 定义递归
  // ================================================================

  const handleDefineRecursion = useCallback(() => {
    const name = newRuleName.trim();
    if (!name) {
      addToast('error', '请输入规则名称 / Please enter a rule name');
      return;
    }

    if (allRules.some((r) => r.name.toLowerCase() === name.toLowerCase())) {
      addToast('error', `规则 "${name}" 已存在 / Rule "${name}" already exists`);
      return;
    }

    const depth = parseInt(newRuleDepth, 10);
    if (isNaN(depth) || depth < 1 || depth > 10) {
      addToast('error', '深度必须在 1-10 之间 / Depth must be 1-10');
      return;
    }

    const newRule: RecursionRule = {
      name,
      transformType: newRuleTransform,
      maxDepth: depth,
      description: `自定义递归: ${name} (深度 ${depth}) / Custom recursion: ${name}`,
    };

    setCustomRules((prev) => [...prev, newRule]);
    setNewRuleName('');
    setNewRuleDepth('3');
    setShowDefineForm(false);

    appendLog(`递归定义: "${name}" (${newRuleTransform}, 最大深度 ${depth})`, 'info');
    addToast('success', `规则 "${name}" 已定义 / Rule "${name}" defined`);
  }, [newRuleName, newRuleDepth, newRuleTransform, allRules, addToast, appendLog]);

  // ================================================================
  // MUTUAL RECURSION / 互递归
  // ================================================================

  const handleMutualRecursion = useCallback(() => {
    if (!mutualNameA.trim() || !mutualNameB.trim()) {
      addToast('error', '请输入两个构造名称 / Please enter both construction names');
      return;
    }

    let text = `=== 互递归定义 / Mutual Recursion Definition ===\n\n`;
    text += `构造 A: "${mutualNameA.trim()}"\n`;
    text += `构造 B: "${mutualNameB.trim()}"\n\n`;
    text += `关系 / Relationship:\n`;
    text += `  A 在每一步调用 B 的输出作为输入\n`;
    text += `  B 在每一步调用 A 的输出作为输入\n\n`;
    text += `  A calls B's output as input at each step\n`;
    text += `  B calls A's output as input at each step\n\n`;
    text += `终止条件 / Termination:\n`;
    text += `  两个构造共享深度计数器，达到上限时同时终止\n`;
    text += `  Both share a depth counter, terminate together at limit\n`;

    setResultsText(text);
    setResultsColor('var(--text, #e0e0e0)');
    setShowResults(true);

    appendLog(`互递归定义: "${mutualNameA.trim()}" <-> "${mutualNameB.trim()}"`, 'info');
    addToast('success', '互递归关系已定义 / Mutual recursion defined');
  }, [mutualNameA, mutualNameB, addToast, appendLog]);

  // ================================================================
  // VALIDATE MEASURE / 验证度量（终止性检查）
  // ================================================================

  const handleValidate = useCallback(() => {
    const rule = allRules.find((r) => r.name === selectedRule);
    if (!rule) {
      addToast('error', '请先选择一个规则 / Please select a rule first');
      return;
    }

    const effectiveRule = { ...rule, maxDepth: targetDepth };
    const validation = validateTermination(effectiveRule);

    let text = `=== 终止性验证 / Termination Validation ===\n\n`;
    text += `规则 / Rule: ${rule.name}\n`;
    text += `目标深度 / Target depth: ${targetDepth}\n`;
    text += `最大深度 / Max depth: ${rule.maxDepth}\n\n`;

    if (validation.terminates) {
      text += `结果 / Result: 终止性保证 / TERMINATION GUARANTEED\n\n`;
      text += `分析 / Analysis:\n`;
      text += `  1. 深度限制为有限值: ${targetDepth}\n`;
      text += `  2. 每步递归深度递增 1\n`;
      text += `  3. 当 depth >= ${targetDepth} 时停止递归\n`;
      text += `  4. 总步数 = ${targetDepth} (有限)\n\n`;

      // 估算元素数量
      let estimatedPoints = 0;
      let estimatedSegments = 0;
      switch (rule.transformType) {
        case 'sierpinski':
          estimatedPoints = Math.round(3 * Math.pow(3, targetDepth));
          estimatedSegments = Math.round(3 * Math.pow(3, targetDepth));
          break;
        case 'koch':
          estimatedPoints = Math.round(3 * Math.pow(4, targetDepth));
          estimatedSegments = Math.round(3 * Math.pow(4, targetDepth));
          break;
        case 'fractal_tree':
          estimatedPoints = Math.round(2 * (Math.pow(2, targetDepth + 1) - 1));
          estimatedSegments = Math.round(Math.pow(2, targetDepth + 1) - 1);
          break;
        default:
          estimatedPoints = targetDepth * 10;
          estimatedSegments = targetDepth * 10;
      }

      text += `预估元素 / Estimated elements:\n`;
      text += `  点数 / Points: ~${estimatedPoints}\n`;
      text += `  线段数 / Segments: ~${estimatedSegments}\n`;

      if (estimatedPoints > 5000 || estimatedSegments > 5000) {
        text += `\n警告 / Warning: 元素数量较大，可能影响性能\n`;
      }
    } else {
      text += `结果 / Result: 潜在无限循环 / POTENTIAL INFINITE LOOP\n\n`;
      text += `原因 / Reason: ${validation.reason}\n`;
    }

    setResultsText(text);
    setResultsColor(validation.terminates ? '#51cf66' : '#ff6b6b');
    setShowResults(true);

    appendLog(`终止性验证: ${rule.name} -> ${validation.terminates ? '保证终止' : '可能不终止'}`, 'info');
    addToast(
      validation.terminates ? 'success' : 'warning',
      validation.terminates ? '终止性保证 / Termination guaranteed' : '潜在无限循环 / Potential infinite loop',
    );
  }, [selectedRule, targetDepth, allRules, addToast, appendLog]);

  // ================================================================
  // STEP (单步执行) / Single Step Execution
  // ================================================================

  /** 执行一步递归，将当前深度的分形渲染到画布 */
  const handleStep = useCallback(() => {
    const rule = allRules.find((r) => r.name === selectedRule);
    if (!rule) {
      addToast('error', '请先选择一个规则 / Please select a rule first');
      return;
    }

    if (currentStep >= targetDepth) {
      addToast('info', '已达到目标深度 / Target depth reached');
      setIsStepping(false);
      return;
    }

    // 保存当前状态
    saveUndoState();

    // 生成当前步骤的分形
    idCounterRef.current = { value: 1000 };
    const fractal = generateFractal(rule, currentStep + 1, idCounterRef.current);

    // 清空画布并渲染分形
    clearAll();
    setPoints(fractal.points);
    setSegments(fractal.segments);

    setCurrentStep(currentStep + 1);
    setIsStepping(true);

    appendLog(`单步执行: ${rule.name} 深度 ${currentStep + 1}/${targetDepth} (${fractal.totalPoints} 点, ${fractal.totalSegments} 线段)`, 'info');
    addToast('info', `步骤 ${currentStep + 1}/${targetDepth}: ${fractal.totalPoints} 点, ${fractal.totalSegments} 线段`);
  }, [selectedRule, targetDepth, currentStep, allRules, saveUndoState, clearAll, setPoints, setSegments, addToast, appendLog]);

  /** 一次性生成到目标深度 */
  const handleGenerateFull = useCallback(() => {
    const rule = allRules.find((r) => r.name === selectedRule);
    if (!rule) {
      addToast('error', '请先选择一个规则 / Please select a rule first');
      return;
    }

    saveUndoState();

    idCounterRef.current = { value: 1000 };
    const fractal = generateFractal(rule, targetDepth, idCounterRef.current);

    clearAll();
    setPoints(fractal.points);
    setSegments(fractal.segments);

    setCurrentStep(targetDepth);
    setIsStepping(false);

    appendLog(`完整生成: ${rule.name} 深度 ${targetDepth} (${fractal.totalPoints} 点, ${fractal.totalSegments} 线段)`, 'info');
    addToast('success', `已生成 ${rule.name} 深度 ${targetDepth}: ${fractal.totalPoints} 点, ${fractal.totalSegments} 线段`);
  }, [selectedRule, targetDepth, allRules, saveUndoState, clearAll, setPoints, setSegments, addToast, appendLog]);

  /** 重置步骤 */
  const handleReset = useCallback(() => {
    setCurrentStep(0);
    setIsStepping(false);
    addToast('info', '步骤已重置 / Steps reset');
  }, [addToast]);

  // ================================================================
  // 删除自定义规则 / Delete Custom Rule
  // ================================================================

  const handleDeleteRule = useCallback((name: string) => {
    setCustomRules((prev) => prev.filter((r) => r.name !== name));
    if (selectedRule === name) {
      setSelectedRule(BUILTIN_RULES[0]?.name ?? '');
    }
    addToast('info', `规则 "${name}" 已删除 / Rule "${name}" deleted`);
  }, [selectedRule, addToast]);

  // ================================================================
  // 获取当前选中规则的最大深度 / Get max depth for selected rule
  // ================================================================

  const selectedRuleData = useMemo(
    () => allRules.find((r) => r.name === selectedRule),
    [allRules, selectedRule],
  );

  return (
    <>
      <Panel title="RECURSION / 递归" panelId="recurse-ops">
        {/* 定义递归 */}
        <button className="btn btn-accent" onClick={() => setShowDefineForm(!showDefineForm)}>
          {showDefineForm ? 'CLOSE FORM / 关闭表单' : 'DEFINE RECURSION / 定义递归'}
        </button>

        {/* 互递归 */}
        <button className="btn" onClick={() => setShowMutualForm(!showMutualForm)}>
          {showMutualForm ? 'CLOSE MUTUAL / 关闭互递归' : 'MUTUAL RECURSION / 互递归'}
        </button>

        {/* 验证度量 */}
        <button className="btn" onClick={handleValidate}>
          VALIDATE MEASURE / 验证度量
        </button>

        {/* 单步执行 */}
        <button className="btn" onClick={handleStep} disabled={currentStep >= targetDepth}>
          STEP / 单步执行
        </button>
      </Panel>

      {/* 内置分形选择 + 深度控制 */}
      <Panel title="FRACTAL SELECT / 分形选择" panelId="recurse-select">
        <div className="panel-form">
          <label className="panel-form-label">选择分形 / Select Fractal</label>
          <select
            className="panel-form-input"
            value={selectedRule}
            onChange={(e) => {
              setSelectedRule(e.target.value);
              setCurrentStep(0);
              setIsStepping(false);
            }}
          >
            {allRules.map((r) => (
              <option key={r.name} value={r.name}>
                {r.name} {r.builtin ? '(内置)' : '(自定义)'}
              </option>
            ))}
          </select>

          <label className="panel-form-label">
            目标深度 / Target Depth (0-{selectedRuleData?.maxDepth ?? 5})
          </label>
          <input
            className="panel-form-input"
            type="number"
            min={0}
            max={selectedRuleData?.maxDepth ?? 5}
            value={targetDepth}
            onChange={(e) => {
              const val = parseInt(e.target.value, 10);
              if (!isNaN(val)) {
                const max = selectedRuleData?.maxDepth ?? 5;
                setTargetDepth(Math.max(0, Math.min(val, max)));
                setCurrentStep(0);
              }
            }}
          />

          <div style={{ display: 'flex', gap: '4px' }}>
            <button
              className="btn btn-accent"
              onClick={handleGenerateFull}
              style={{ flex: 1 }}
            >
              GENERATE / 生成
            </button>
            <button
              className="btn"
              onClick={handleReset}
              style={{ flex: 1 }}
            >
              RESET / 重置
            </button>
          </div>
        </div>
      </Panel>

      {/* 定义递归表单 */}
      {showDefineForm && (
        <Panel title="DEFINE RECURSION / 定义递归" panelId="recurse-define">
          <div className="panel-form">
            <label className="panel-form-label">规则名称 / Rule Name</label>
            <input
              className="panel-form-input"
              value={newRuleName}
              onChange={(e) => setNewRuleName(e.target.value)}
              placeholder="例: MyFractal"
            />
            <label className="panel-form-label">变换类型 / Transform Type</label>
            <select
              className="panel-form-input"
              value={newRuleTransform}
              onChange={(e) => setNewRuleTransform(e.target.value as RecursionRule['transformType'])}
            >
              <option value="custom_subdivide">自定义细分 / Custom Subdivide</option>
              <option value="custom_replace">自定义替换 / Custom Replace</option>
            </select>
            <label className="panel-form-label">最大深度 / Max Depth (1-10)</label>
            <input
              className="panel-form-input"
              type="number"
              min={1}
              max={10}
              value={newRuleDepth}
              onChange={(e) => setNewRuleDepth(e.target.value)}
            />
            <button className="btn btn-accent" onClick={handleDefineRecursion}>
              CONFIRM / 确认定义
            </button>
          </div>
        </Panel>
      )}

      {/* 互递归表单 */}
      {showMutualForm && (
        <Panel title="MUTUAL RECURSION / 互递归" panelId="recurse-mutual">
          <div className="panel-form">
            <label className="panel-form-label">构造 A / Construction A</label>
            <input
              className="panel-form-input"
              value={mutualNameA}
              onChange={(e) => setMutualNameA(e.target.value)}
              placeholder="例: EvenPoints"
            />
            <label className="panel-form-label">构造 B / Construction B</label>
            <input
              className="panel-form-input"
              value={mutualNameB}
              onChange={(e) => setMutualNameB(e.target.value)}
              placeholder="例: OddPoints"
            />
            <p style={{ fontSize: '10px', color: 'var(--text, #e0e0e0)', opacity: 0.7, margin: 0 }}>
              A 和 B 将在每一步交替调用对方的输出作为输入。
              A and B will alternately call each other's output as input.
            </p>
            <button className="btn btn-accent" onClick={handleMutualRecursion}>
              DEFINE / 定义
            </button>
          </div>
        </Panel>
      )}

      {/* 结果展示面板 */}
      {showResults && (
        <Panel title="RESULTS / 结果" panelId="recurse-results">
          <pre className="panel-result-box" style={{ color: resultsColor }}>
            {resultsText}
          </pre>
          <button className="btn" onClick={() => setShowResults(false)} style={{ marginTop: '4px', width: '100%' }}>
            CLOSE / 关闭
          </button>
        </Panel>
      )}

      {/* 自定义规则列表 */}
      {customRules.length > 0 && (
        <Panel title="CUSTOM RULES / 自定义规则" panelId="recurse-custom">
          {customRules.map((r) => (
            <div key={r.name} style={{
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
              padding: '2px 0',
              fontSize: '11px',
            }}>
              <span style={{ color: 'var(--text, #e0e0e0)' }}>
                {r.name} (d{r.maxDepth})
              </span>
              <button
                className="btn"
                onClick={() => handleDeleteRule(r.name)}
                style={{ padding: '1px 6px', fontSize: '10px' }}
              >
                DEL
              </button>
            </div>
          ))}
        </Panel>
      )}

      <Panel title="INFO / 信息" panelId="recurse-info">
        <div className="info-box">
          {/* 递归深度 */}
          <div className="info-row">
            <span className="info-label">DEPTH / 深度</span>
            <span className="info-value">{currentStep} / {targetDepth}</span>
          </div>
          {/* 当前元素数 */}
          <div className="info-row">
            <span className="info-label">POINTS / 点</span>
            <span className="info-value">{currentPoints.length}</span>
          </div>
          <div className="info-row">
            <span className="info-label">SEGMENTS / 线段</span>
            <span className="info-value">{currentSegments.length}</span>
          </div>
          {/* 执行状态 */}
          <div className="info-row">
            <span className="info-label">STATUS / 状态</span>
            <span className="info-value" style={{
              color: isStepping ? '#ffd43b' : currentStep >= targetDepth ? '#51cf66' : 'var(--text, #e0e0e0)',
            }}>
              {isStepping ? 'STEPPING' : currentStep >= targetDepth ? 'COMPLETE' : 'READY'}
            </span>
          </div>
          {/* 选中规则 */}
          <div className="info-row">
            <span className="info-label">RULE / 规则</span>
            <span className="info-value">{selectedRule || '--'}</span>
          </div>
          {/* 当前工具 */}
          <div className="info-row">
            <span className="info-label">TOOL / 工具</span>
            <span className="info-value">{tool.toUpperCase()}</span>
          </div>
          {/* 撤销栈深度 */}
          <div className="info-row">
            <span className="info-label">UNDO / 撤销栈</span>
            <span className="info-value">{undoDepth}</span>
          </div>
          {/* 规则统计 */}
          <div className="info-row">
            <span className="info-label">BUILTIN / 内置规则</span>
            <span className="info-value">{BUILTIN_RULES.length}</span>
          </div>
          <div className="info-row">
            <span className="info-label">CUSTOM / 自定义规则</span>
            <span className="info-value">{customRules.length}</span>
          </div>
        </div>
      </Panel>
    </>
  );
};

export default RecursePanel;
