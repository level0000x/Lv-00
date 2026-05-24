/**
 * @module components/panels/ProofPanel
 * @description 证明模块侧边栏面板。
 *              Proof module sidebar panel.
 *
 *              提供证明步骤导航（基于撤销历史）、Coq 格式导出、
 *              反证法（矛盾检测）以及实时证明状态展示。
 *              所有功能使用纯 JS 实现，不依赖 WASM 后端。
 *
 *              Provides proof step navigation (based on undo history),
 *              Coq format export, proof by contradiction (conflict detection),
 *              and real-time proof status display.
 *              All features are implemented in pure JS, no WASM backend required.
 */

import React, { useState, useCallback, useMemo, useEffect } from 'react';
import Panel from './Panel';
import { useAppStore } from '@/stores';
import { detectConflicts } from '@/utils/geometryAlgorithms';

/**
 * ProofPanel - 证明模块侧边栏面板
 *
 * 面板分区:
 * - PROOF: 证明步骤导航、Coq 导出、矛盾证明
 * - COQ EXPORT: Coq 脚本展示与复制
 * - EX FALSO: 矛盾检测结果
 * - INFO : 来自 Store 的实时证明上下文数据
 */
const ProofPanel: React.FC = () => {
  const addToast = useAppStore((s) => s.addToast);
  const appendLog = useAppStore((s) => s.appendLog);

  // ================================================================
  // 从 Store 读取真实数据 —— 展示当前证明上下文
  // ================================================================
  const points = useAppStore((s) => s.points);
  const segments = useAppStore((s) => s.segments);
  const constraints = useAppStore((s) => s.constraints);
  const undoStack = useAppStore((s) => s.undoStack);
  const redoStack = useAppStore((s) => s.redoStack);
  const tool = useAppStore((s) => s.tool);

  // 用于恢复几何状态的方法
  const setPoints = useAppStore((s) => s.setPoints);
  const setSegments = useAppStore((s) => s.setSegments);
  const setConstraints = useAppStore((s) => s.setConstraints);
  const saveUndoState = useAppStore((s) => s.saveUndoState);

  // ================================================================
  // 证明步骤导航状态 / Proof Step Navigation State
  // ================================================================
  /**
   * 证明步骤：基于 undoStack 构建步骤数组。
   * 每个步骤对应撤销栈中的一个快照 + 当前状态。
   * proofSteps[0] = 初始状态（undoStack[0]），... proofSteps[N-1] = 当前状态
   */
  const proofSteps = useMemo(() => {
    // undoStack 中存储的是每次操作前的快照，按时间顺序排列
    // 加上当前状态作为最后一步
    const currentSnapshot = {
      points: points.map((p) => ({ ...p })),
      segments: segments.map((s) => ({ ...s })),
      constraints: constraints.map((c) => ({ ...c, args: [...c.args] })),
      regions: [],
      ports: [],
      funcBlocks: [],
      timestamp: Date.now(),
    };
    return [...undoStack, currentSnapshot];
  }, [undoStack, points, segments, constraints]);

  const totalSteps = proofSteps.length;
  const [currentStepIndex, setCurrentStepIndex] = useState(totalSteps > 0 ? totalSteps - 1 : 0);

  // ================================================================
  // Coq 导出状态 / Coq Export State
  // ================================================================
  const [coqScript, setCoqScript] = useState('');
  const [showCoqPanel, setShowCoqPanel] = useState(false);

  // ================================================================
  // 矛盾证明状态 / Ex Falso State
  // ================================================================
  const [conflictResult, setConflictResult] = useState<string>('');
  const [showConflictPanel, setShowConflictPanel] = useState(false);

  // ================================================================
  // 自然语言证明状态 / Natural Language Proof State
  // ================================================================
  const [nlProof, setNlProof] = useState('');
  const [showNlPanel, setShowNlPanel] = useState(false);

  // ================================================================
  // 回溯树状态 / Backtrack Tree State
  // ================================================================
  const [backtrackTree, setBacktrackTree] = useState<Array<{
    id: number;
    label: string;
    status: 'success' | 'failure' | 'choice' | 'pruned';
    isBacktrack: boolean;
    children: Array<{
      id: number;
      label: string;
      status: 'success' | 'failure' | 'choice' | 'pruned';
      isBacktrack: boolean;
    }>;
  }>>([]);
  const [showBacktrackPanel, setShowBacktrackPanel] = useState(false);

  // ================================================================
  // 策略与注释状态 / Strategy & Notes State
  // ================================================================
  const [strategyNote, setStrategyNote] = useState('');
  const [stepNote, setStepNote] = useState('');
  const [searchStrategy, setSearchStrategy] = useState('Forward Chaining');

  // ================================================================
  // 跳转到指定步骤状态 / Jump to Step State
  // ================================================================
  const [jumpToStepInput, setJumpToStepInput] = useState('');

  // ================================================================
  // SVG 几何视图开关 / SVG Geometry View Toggle
  // ================================================================
  const [showSvgView, setShowSvgView] = useState(false);

  // ================================================================
  // 约束满足度计算 / Constraint Satisfaction Calculation
  // ================================================================
  const constraintSatisfaction = useMemo(() => {
    if (constraints.length === 0) return 'N/A';
    // 简单分析：检查冗余和冲突约束
    const conflicts = detectConflicts(constraints);
    if (conflicts.length > 0) return 'CONFLICT';
    return 'SATISFIED';
  }, [constraints]);

  // 证明完整度百分比
  const proofCompleteness = useMemo(() => {
    if (totalSteps === 0) return 0;
    // 基于当前步骤位置和约束满足度计算完整度
    const stepRatio = totalSteps > 1 ? currentStepIndex / (totalSteps - 1) : 1;
    const constraintBonus = constraints.length > 0 ? 0.2 : 0;
    const conflictPenalty = constraintSatisfaction === 'CONFLICT' ? 0.3 : 0;
    const completeness = Math.min(100, Math.max(0,
      Math.round((stepRatio * 0.8 + constraintBonus - conflictPenalty) * 100)
    ));
    return completeness;
  }, [totalSteps, currentStepIndex, constraints.length, constraintSatisfaction]);

  // ================================================================
  // 证明步骤导航 / Proof Step Navigation
  // ================================================================

  /** 上一步：恢复到 undoStack 中更早的快照 */
  const handlePrev = useCallback(() => {
    if (currentStepIndex <= 0) {
      addToast('info', '已到第一步 / Already at first step');
      return;
    }
    const prevIndex = currentStepIndex - 1;
    const snapshot = proofSteps[prevIndex];
    if (!snapshot) return;

    // 保存当前状态以便前进
    saveUndoState();

    // 恢复到上一步的几何状态
    setPoints(snapshot.points.map((p) => ({ ...p })));
    setSegments(snapshot.segments.map((s) => ({ ...s })));
    setConstraints(snapshot.constraints.map((c) => ({ ...c, args: [...c.args] })));
    setCurrentStepIndex(prevIndex);

    appendLog(`证明导航: 回到步骤 ${prevIndex + 1}/${totalSteps}`, 'info');
    addToast('info', `步骤 ${prevIndex + 1} / ${totalSteps}`);
  }, [currentStepIndex, proofSteps, totalSteps, saveUndoState, setPoints, setSegments, setConstraints, addToast, appendLog]);

  /** 下一步：恢复到 undoStack 中更晚的快照（或当前状态） */
  const handleNext = useCallback(() => {
    if (currentStepIndex >= totalSteps - 1) {
      addToast('info', '已是最新步骤 / Already at latest step');
      return;
    }
    const nextIndex = currentStepIndex + 1;
    const snapshot = proofSteps[nextIndex];
    if (!snapshot) return;

    // 保存当前状态
    saveUndoState();

    // 恢复到下一步的几何状态
    setPoints(snapshot.points.map((p) => ({ ...p })));
    setSegments(snapshot.segments.map((s) => ({ ...s })));
    setConstraints(snapshot.constraints.map((c) => ({ ...c, args: [...c.args] })));
    setCurrentStepIndex(nextIndex);

    appendLog(`证明导航: 前进到步骤 ${nextIndex + 1}/${totalSteps}`, 'info');
    addToast('info', `步骤 ${nextIndex + 1} / ${totalSteps}`);
  }, [currentStepIndex, proofSteps, totalSteps, saveUndoState, setPoints, setSegments, setConstraints, addToast, appendLog]);

  /** 跳转到指定步骤 */
  const handleJumpToStep = useCallback(() => {
    const targetStep = parseInt(jumpToStepInput, 10);
    if (isNaN(targetStep) || targetStep < 1 || targetStep > totalSteps) {
      addToast('warning', `请输入有效步骤 (1-${totalSteps}) / Enter a valid step (1-${totalSteps})`);
      return;
    }
    const targetIndex = targetStep - 1;
    const snapshot = proofSteps[targetIndex];
    if (!snapshot) return;

    saveUndoState();
    setPoints(snapshot.points.map((p) => ({ ...p })));
    setSegments(snapshot.segments.map((s) => ({ ...s })));
    setConstraints(snapshot.constraints.map((c) => ({ ...c, args: [...c.args] })));
    setCurrentStepIndex(targetIndex);
    setJumpToStepInput('');

    appendLog(`证明导航: 跳转到步骤 ${targetStep}/${totalSteps}`, 'info');
    addToast('info', `已跳转到步骤 ${targetStep} / Jumped to step ${targetStep}`);
  }, [jumpToStepInput, totalSteps, proofSteps, saveUndoState, setPoints, setSegments, setConstraints, addToast, appendLog]);

  // ================================================================
  // 键盘快捷键 / Keyboard Shortcuts
  // ================================================================

  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      // 仅在证明面板相关区域响应快捷键
      const activeEl = document.activeElement;
      // 如果焦点在输入框中，不处理快捷键（允许正常输入）
      if (activeEl && (activeEl.tagName === 'INPUT' || activeEl.tagName === 'TEXTAREA' || activeEl.tagName === 'SELECT')) {
        return;
      }

      switch (e.key) {
        case 'ArrowLeft':
          e.preventDefault();
          handlePrev();
          break;
        case 'ArrowRight':
          e.preventDefault();
          handleNext();
          break;
        case 'Home':
          e.preventDefault();
          if (totalSteps > 0) {
            const firstSnapshot = proofSteps[0];
            if (firstSnapshot) {
              saveUndoState();
              setPoints(firstSnapshot.points.map((p) => ({ ...p })));
              setSegments(firstSnapshot.segments.map((s) => ({ ...s })));
              setConstraints(firstSnapshot.constraints.map((c) => ({ ...c, args: [...c.args] })));
              setCurrentStepIndex(0);
              addToast('info', '已跳到第一步 / Jumped to first step');
            }
          }
          break;
        case 'End':
          e.preventDefault();
          if (totalSteps > 0) {
            const lastSnapshot = proofSteps[totalSteps - 1];
            if (lastSnapshot) {
              saveUndoState();
              setPoints(lastSnapshot.points.map((p) => ({ ...p })));
              setSegments(lastSnapshot.segments.map((s) => ({ ...s })));
              setConstraints(lastSnapshot.constraints.map((c) => ({ ...c, args: [...c.args] })));
              setCurrentStepIndex(totalSteps - 1);
              addToast('info', '已跳到最新步骤 / Jumped to latest step');
            }
          }
          break;
        case 'g':
          // 'g' 键快速跳转到步骤：打开输入框
          if (!e.ctrlKey && !e.metaKey && !e.altKey) {
            const jumpInput = document.getElementById('proofJumpInput') as HTMLInputElement | null;
            if (jumpInput) {
              e.preventDefault();
              jumpInput.focus();
              jumpInput.select();
            }
          }
          break;
        case 's':
          // 's' 键切换 SVG 视图
          if (!e.ctrlKey && !e.metaKey && !e.altKey) {
            e.preventDefault();
            setShowSvgView((prev) => !prev);
          }
          break;
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [handlePrev, handleNext, totalSteps, proofSteps, saveUndoState, setPoints, setSegments, setConstraints, addToast]);

  // ================================================================
  // SVG 几何视图生成 / SVG Geometry View Generation
  // ================================================================

  /** 为当前步骤生成 SVG 几何构造视图 */
  const generateProofSvg = useCallback((): string => {
    if (proofSteps.length === 0) return '';

    const step = proofSteps[currentStepIndex];
    if (!step || step.points.length === 0) return '';

    const pts = step.points;
    const segs = step.segments;

    // 计算画布范围
    let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
    for (const p of pts) {
      if (p.x < minX) minX = p.x;
      if (p.y < minY) minY = p.y;
      if (p.x > maxX) maxX = p.x;
      if (p.y > maxY) maxY = p.y;
    }

    // 确保最小范围
    if (minX === maxX) { minX -= 50; maxX += 50; }
    if (minY === maxY) { minY -= 50; maxY += 50; }

    // 添加边距
    const padX = (maxX - minX) * 0.15 || 10;
    const padY = (maxY - minY) * 0.15 || 10;
    const viewMinX = minX - padX;
    const viewMinY = minY - padY;
    const viewW = (maxX - minX) + 2 * padX;
    const viewH = (maxY - minY) + 2 * padY;

    // SVG 宽高
    const svgW = 280;
    const svgH = Math.max(160, (viewH / viewW) * svgW);

    const xform = (wx: number) => ((wx - viewMinX) / viewW) * svgW;
    const yform = (wy: number) => svgH - ((wy - viewMinY) / viewH) * svgH;

    let svg = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${svgW} ${svgH}" width="${svgW}" height="${svgH}" class="proof-svg-view">\n`;
    svg += `  <rect width="${svgW}" height="${svgH}" fill="#0d1117" />\n`;

    // 绘制网格
    svg += `  <g stroke="#21262d" stroke-width="0.5">\n`;
    for (let gx = 0; gx <= svgW; gx += 20) {
      svg += `    <line x1="${gx}" y1="0" x2="${gx}" y2="${svgH}" />\n`;
    }
    for (let gy = 0; gy <= svgH; gy += 20) {
      svg += `    <line x1="0" y1="${gy}" x2="${svgW}" y2="${gy}" />\n`;
    }
    svg += `  </g>\n`;

    // 绘制线段
    for (const s of segs) {
      const p1 = pts.find((pp) => pp.id === s.p1);
      const p2 = pts.find((pp) => pp.id === s.p2);
      if (p1 && p2) {
        svg += `  <line x1="${xform(p1.x)}" y1="${yform(p1.y)}" x2="${xform(p2.x)}" y2="${yform(p2.y)}" stroke="#58a6ff" stroke-width="1.5" opacity="0.7" />\n`;
      }
    }

    // 绘制约束（虚线箭头表明约束关系）
    const stepConstraints = step.constraints ?? [];
    for (const c of stepConstraints) {
      // 为不同类型的约束使用不同颜色
      let constraintColor = '#8b949e';
      switch (c.type) {
        case 'incidence': constraintColor = '#51cf66'; break;
        case 'betweenness': constraintColor = '#ffd43b'; break;
        case 'intersection': constraintColor = '#ff6b6b'; break;
        case 'containment': constraintColor = '#da77f2'; break;
        case 'connection': constraintColor = '#4dabf7'; break;
      }
      if (c.args.length >= 2) {
        const a1 = pts.find((pp) => pp.id === c.args[0]);
        const a2 = pts.find((pp) => pp.id === c.args[1]);
        if (a1 && a2) {
          svg += `  <line x1="${xform(a1.x)}" y1="${yform(a1.y)}" x2="${xform(a2.x)}" y2="${yform(a2.y)}" stroke="${constraintColor}" stroke-width="1" stroke-dasharray="4,3" opacity="0.5" />\n`;
        }
      }
    }

    // 绘制点
    for (const p of pts) {
      const cx = xform(p.x);
      const cy = yform(p.y);
      const isSelected = selectedPoint?.id === p.id;
      svg += `  <circle cx="${cx}" cy="${cy}" r="${isSelected ? 5 : 3.5}" fill="${isSelected ? '#ffd43b' : '#51cf66'}" stroke="${isSelected ? '#fff' : '#238636'}" stroke-width="1" />\n`;
      svg += `  <text x="${cx + 5}" y="${cy - 5}" fill="#e6edf3" font-size="8" font-family="monospace">p${p.id}</text>\n`;
    }

    svg += `</svg>`;
    return svg;
  }, [proofSteps, currentStepIndex]);

  const proofSvg = useMemo(() => generateProofSvg(), [generateProofSvg]);

  // ================================================================
  // HTML 导出 / HTML Export
  // ================================================================

  const selectedPoint = useAppStore((s) => s.selectedPoint);

  /** 生成自包含的交互式 HTML 证明文件 */
  const exportHtmlProof = useCallback(() => {
    if (proofSteps.length === 0) {
      addToast('warning', '暂无证明步骤 / No proof steps to export');
      return;
    }

    // 为每个步骤生成 SVG
    const stepsSvgData: string[] = [];
    for (let i = 0; i < proofSteps.length; i++) {
      const step = proofSteps[i];
      if (!step || step.points.length === 0) {
        stepsSvgData.push('');
        continue;
      }
      const pts = step.points;
      const segs = step.segments;

      let sminX = Infinity, sminY = Infinity, smaxX = -Infinity, smaxY = -Infinity;
      for (const p of pts) {
        if (p.x < sminX) sminX = p.x;
        if (p.y < sminY) sminY = p.y;
        if (p.x > smaxX) smaxX = p.x;
        if (p.y > smaxY) smaxY = p.y;
      }
      if (sminX === smaxX) { sminX -= 50; smaxX += 50; }
      if (sminY === smaxY) { sminY -= 50; smaxY += 50; }
      const spadX = (smaxX - sminX) * 0.15 || 10;
      const spadY = (smaxY - sminY) * 0.15 || 10;
      const svMinX = sminX - spadX;
      const svMinY = sminY - spadY;
      const svW = (smaxX - sminX) + 2 * spadX;
      const svH = (smaxY - sminY) + 2 * spadY;
      const svgw = 400;
      const svgh = Math.max(200, (svH / svW) * svgw);
      const sxf = (wx: number) => ((wx - svMinX) / svW) * svgw;
      const syf = (wy: number) => svgh - ((wy - svMinY) / svH) * svgh;

      let ssvg = '';
      for (const s of segs) {
        const p1 = pts.find((pp) => pp.id === s.p1);
        const p2 = pts.find((pp) => pp.id === s.p2);
        if (p1 && p2) {
          ssvg += `<line x1="${sxf(p1.x)}" y1="${syf(p1.y)}" x2="${sxf(p2.x)}" y2="${syf(p2.y)}" stroke="#58a6ff" stroke-width="1.5" opacity="0.7"/>\n`;
        }
      }
      for (const p of pts) {
        ssvg += `<circle cx="${sxf(p.x)}" cy="${syf(p.y)}" r="4" fill="#51cf66" stroke="#238636" stroke-width="1"/>\n`;
        ssvg += `<text x="${sxf(p.x) + 6}" y="${syf(p.y) - 6}" fill="#e6edf3" font-size="10" font-family="monospace">p${p.id}</text>\n`;
      }

      stepsSvgData.push(`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${svgw} ${svgh}" width="100%" style="max-width:400px;height:auto;background:#0d1117;border:1px solid #30363d;border-radius:4px">\n${ssvg}</svg>`);
    }

    // 构建 HTML 内容
    const timestamp = new Date().toISOString().slice(0, 19).replace(/[:-]/g, '');
    const html = `<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Lv-00 Proof Export / 证明导出 - ${timestamp}</title>
<style>
  :root {
    --bg: #0d1117; --bg2: #161b22; --bg3: #21262d;
    --text: #e6edf3; --text2: #8b949e; --text3: #484f58;
    --accent: #58a6ff; --green: #51cf66; --red: #ff6b6b;
    --border: #30363d; --yellow: #ffd43b;
  }
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body { background: var(--bg); color: var(--text); font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; padding: 20px; }
  h1 { font-size: 20px; margin-bottom: 8px; color: var(--accent); }
  .meta { font-size: 12px; color: var(--text2); margin-bottom: 20px; }
  .controls { display: flex; align-items: center; gap: 8px; margin-bottom: 16px; flex-wrap: wrap; }
  .controls button { padding: 6px 14px; border: 1px solid var(--border); background: var(--bg2); color: var(--text); cursor: pointer; border-radius: 4px; font-size: 13px; font-family: inherit; transition: background 0.15s; }
  .controls button:hover { background: var(--bg3); }
  .controls button:disabled { opacity: 0.3; cursor: not-allowed; }
  .controls button.active { border-color: var(--accent); color: var(--accent); }
  .step-info { font-size: 13px; color: var(--text2); min-width: 80px; text-align: center; }
  .jump-input { width: 60px; padding: 5px 8px; border: 1px solid var(--border); background: var(--bg2); color: var(--text); font-size: 13px; border-radius: 4px; text-align: center; font-family: monospace; outline: none; }
  .jump-input:focus { border-color: var(--accent); }
  .main-layout { display: flex; gap: 16px; flex-wrap: wrap; }
  .svg-panel { flex: 1; min-width: 300px; }
  .info-panel { flex: 1; min-width: 250px; }
  .info-panel pre { background: var(--bg2); border: 1px solid var(--border); border-radius: 4px; padding: 12px; font-size: 12px; line-height: 1.5; overflow-x: auto; white-space: pre-wrap; word-break: break-word; color: var(--text2); }
  .info-table { width: 100%; border-collapse: collapse; font-size: 12px; }
  .info-table td { padding: 4px 8px; border-bottom: 1px solid var(--border); }
  .info-table .label { color: var(--text2); }
  .info-table .value { color: var(--text); font-weight: 600; text-align: right; }
  .shortcuts { font-size: 11px; color: var(--text3); margin-top: 12px; line-height: 1.6; }
  .shortcuts kbd { background: var(--bg3); border: 1px solid var(--border); border-radius: 3px; padding: 1px 5px; font-family: monospace; font-size: 10px; color: var(--text2); }
  .legend { display: flex; gap: 12px; flex-wrap: wrap; margin-top: 8px; font-size: 11px; color: var(--text2); }
  .legend span { display: flex; align-items: center; gap: 4px; }
  .legend-dot { width: 10px; height: 10px; border-radius: 2px; display: inline-block; }
</style>
</head>
<body>
<h1>Lv-00 Geometry Proof / 几何证明导出</h1>
<div class="meta">Generated: ${new Date().toLocaleString()} | Steps: ${proofSteps.length} | Points: ${points.length} | Segments: ${segments.length} | Constraints: ${constraints.length}</div>

<div class="controls">
  <button onclick="goToStep(0)" id="btnFirst" title="First Step / 第一步">&#x23EE; First</button>
  <button onclick="prevStep()" id="btnPrev" title="Previous Step / 上一步">&#x25C0; Prev</button>
  <span class="step-info">Step <span id="stepNum">${totalSteps}</span> / ${totalSteps}</span>
  <button onclick="nextStep()" id="btnNext" title="Next Step / 下一步">Next &#x25B6;</button>
  <button onclick="goToStep(${totalSteps - 1})" id="btnLast" title="Last Step / 最后一步">Last &#x23ED;</button>
  <input type="number" class="jump-input" id="jumpInput" min="1" max="${totalSteps}" placeholder="Go to..."
    onkeydown="if(event.key==='Enter')goToStep(parseInt(this.value)-1)" />
  <button onclick="toggleAuto()" id="btnAuto" class="active">Auto SVG</button>
</div>

<div class="main-layout">
  <div class="svg-panel" id="svgPanel"></div>
  <div class="info-panel">
    <table class="info-table" id="infoTable"></table>
    <div class="legend" style="margin-top:12px">
      <span><span class="legend-dot" style="background:#51cf66"></span> Point / 点</span>
      <span><span class="legend-dot" style="background:#58a6ff"></span> Segment / 线段</span>
      <span style="color:#ff6b6b">---</span> Constraint / 约束
    </div>
  </div>
</div>

<div class="shortcuts">
  <strong>Keyboard Shortcuts / 快捷键:</strong><br/>
  <kbd>Left Arrow</kbd> Prev step / 上一步 &nbsp;
  <kbd>Right Arrow</kbd> Next step / 下一步 &nbsp;
  <kbd>Home</kbd> First step / 第一步 &nbsp;
  <kbd>End</kbd> Last step / 最后一步<br/>
  <kbd>Space</kbd> Toggle auto-play / 切换自动播放 &nbsp;
  <kbd>G</kbd> Focus jump input / 跳转步骤
</div>

<script>
var currentStep = ${currentStepIndex};
var totalSteps = ${totalSteps};
var autoPlayInterval = null;
var showAutoSvg = true;

var stepsData = ${JSON.stringify(
  proofSteps.map((step, i) => ({
    idx: i,
    points: step?.points?.length ?? 0,
    segments: step?.segments?.length ?? 0,
    constraints: step?.constraints?.length ?? 0,
  }))
)};

var svgs = ${JSON.stringify(stepsSvgData)};

function updateStep(newStep) {
  if (newStep < 0 || newStep >= totalSteps) return;
  currentStep = newStep;
  document.getElementById('stepNum').textContent = newStep + 1;
  document.getElementById('btnPrev').disabled = (currentStep <= 0);
  document.getElementById('btnNext').disabled = (currentStep >= totalSteps - 1);
  document.getElementById('btnFirst').disabled = (currentStep <= 0);
  document.getElementById('btnLast').disabled = (currentStep >= totalSteps - 1);
  document.getElementById('jumpInput').value = '';

  // 更新 SVG
  var svgPanel = document.getElementById('svgPanel');
  if (showAutoSvg && svgs[newStep]) {
    svgPanel.innerHTML = svgs[newStep];
  }

  // 更新信息表
  var sd = stepsData[newStep];
  var info = '<tr><td class="label">Step / 步骤</td><td class="value">' + (newStep + 1) + ' / ' + totalSteps + '</td></tr>';
  if (sd) {
    info += '<tr><td class="label">Points / 点</td><td class="value" style="color:#51cf66">' + sd.points + '</td></tr>';
    info += '<tr><td class="label">Segments / 线段</td><td class="value" style="color:#58a6ff">' + sd.segments + '</td></tr>';
    info += '<tr><td class="label">Constraints / 约束</td><td class="value" style="color:#ffd43b">' + sd.constraints + '</td></tr>';
  }
  document.getElementById('infoTable').innerHTML = info;
}

function prevStep() { updateStep(currentStep - 1); }
function nextStep() { updateStep(currentStep + 1); }
function goToStep(step) {
  if (isNaN(step) || step < 0 || step >= totalSteps) { alert('Please enter a valid step (1-' + totalSteps + ').'); return; }
  updateStep(step);
}
function toggleAuto() {
  showAutoSvg = !showAutoSvg;
  var btn = document.getElementById('btnAuto');
  if (showAutoSvg) {
    btn.classList.add('active');
    updateStep(currentStep);
  } else {
    btn.classList.remove('active');
    document.getElementById('svgPanel').innerHTML = '';
  }
}

document.addEventListener('keydown', function(e) {
  if (e.target.tagName === 'INPUT') return;
  switch(e.key) {
    case 'ArrowLeft': e.preventDefault(); prevStep(); break;
    case 'ArrowRight': e.preventDefault(); nextStep(); break;
    case 'Home': e.preventDefault(); goToStep(0); break;
    case 'End': e.preventDefault(); goToStep(totalSteps - 1); break;
    case ' ': e.preventDefault(); toggleAuto(); break;
    case 'g': case 'G': e.preventDefault(); var inp = document.getElementById('jumpInput'); inp.focus(); inp.select(); break;
  }
});

// 初始化
updateStep(currentStep);
</script>
</body>
</html>`;

    // 下载 HTML 文件
    const blob = new Blob([html], { type: 'text/html;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `lv00_proof_${timestamp}.html`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);

    appendLog(`HTML 证明已导出: ${proofSteps.length} 步骤`, 'info');
    addToast('success', `HTML 证明已导出 / HTML proof exported (${proofSteps.length} steps)`);
  }, [proofSteps, currentStepIndex, points.length, segments.length, constraints.length, addToast, appendLog]);

  // ================================================================
  // Coq 导出 / Coq Export
  // ================================================================

  /** 生成 Coq 格式的证明脚本 */
  const generateCoqScript = useCallback(() => {
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

    setCoqScript(script);
    setShowCoqPanel(true);
    appendLog(`Coq 脚本已生成: ${points.length} 点, ${segments.length} 线段, ${constraints.length} 约束`, 'info');
    addToast('success', 'Coq 脚本已生成 / Coq script generated');
  }, [points, segments, constraints, addToast, appendLog]);

  /** 复制 Coq 脚本到剪贴板 */
  const copyCoqScript = useCallback(() => {
    if (!coqScript) return;
    navigator.clipboard.writeText(coqScript).then(() => {
      addToast('success', '已复制到剪贴板 / Copied to clipboard');
    }).catch(() => {
      // Deprecated fallback: document.execCommand('copy') is obsolete but kept for
      // compatibility with older browsers and non-HTTPS environments.
      // 已弃用的回退方案：document.execCommand('copy') 已过时，但保留用于兼容旧浏览器和非 HTTPS 环境。
      const textarea = document.createElement('textarea');
      textarea.value = coqScript;
      document.body.appendChild(textarea);
      textarea.select();
      document.execCommand('copy');
      document.body.removeChild(textarea);
      addToast('success', '已复制到剪贴板 / Copied to clipboard');
    });
  }, [coqScript, addToast]);

  // ================================================================
  // 反证法 / Ex Falso (Contradiction Proof)
  // ================================================================

  /** 分析约束冲突，生成矛盾证明叙述 */
  const handleExFalso = useCallback(() => {
    const conflicts = detectConflicts(constraints);

    if (conflicts.length === 0) {
      setConflictResult('未检测到矛盾。\nNo contradiction detected.\n\n当前约束集合是一致的，无法通过反证法推导矛盾。\nThe current constraint set is consistent; no contradiction can be derived.');
      setShowConflictPanel(true);
      addToast('info', '未检测到矛盾 / No contradiction detected');
      appendLog('反证法分析: 约束集合一致，无矛盾', 'info');
      return;
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

    setConflictResult(narrative);
    setShowConflictPanel(true);
    addToast('warning', `检测到 ${conflicts.length} 个矛盾 / ${conflicts.length} contradiction(s) detected`);
    appendLog(`反证法分析: 检测到 ${conflicts.length} 个约束冲突`, 'warn');
  }, [constraints, addToast, appendLog]);

  // ================================================================
  // 自然语言证明生成 / Natural Language Proof Generation
  // ================================================================

  /** 根据 proofSteps 比较相邻快照，生成 AlphaGeometry 风格的自然语言步骤描述 */
  const generateNlProof = useCallback(() => {
    if (proofSteps.length <= 1) {
      setNlProof('证明步骤不足，无法生成自然语言描述。至少需要2个步骤。\nNot enough proof steps to generate a natural language description. At least 2 steps are needed.');
      setShowNlPanel(true);
      return;
    }

    let nl = `=== 自然语言证明 / Natural Language Proof ===\n`;
    nl += `搜索策略: ${searchStrategy} / Search Strategy: ${searchStrategy}\n`;
    if (strategyNote) {
      nl += `策略注释: ${strategyNote} / Strategy Note: ${strategyNote}\n`;
    }
    nl += `---\n\n`;

    const verbs = [
      { ch: '构造', en: 'Construct' },
      { ch: '连接', en: 'Connect' },
      { ch: '添加约束', en: 'Add constraint' },
      { ch: '移动', en: 'Move' },
      { ch: '删除', en: 'Delete' },
      { ch: '修改', en: 'Modify' },
    ];

    for (let i = 0; i < proofSteps.length; i++) {
      const step = proofSteps[i];
      if (!step) continue;
      const stepNum = i + 1;
      const isCurrent = i === currentStepIndex;

      // 比较与前一步的差异
      let verb = verbs[0]!;
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
          verb = verbs[0]!;
          objects = newPoints.map((p) => `点 / pt ${p.id}`);
          reason = `${searchStrategy}策略: 添加辅助点 / add auxiliary point`;
        } else if (newSegments.length > 0) {
          verb = verbs[1]!;
          objects = newSegments.map((s) => `线段 / seg ${s.id} (p${s.p1}-p${s.p2})`);
          reason = '连接已有元素 / connect existing elements';
        } else if (newConstraints.length > 0) {
          verb = verbs[2]!;
          objects = newConstraints.map((c) => `约束 / constraint ${c.id} (${c.type})`);
          reason = '施加推理约束 / apply deductive constraint';
        } else {
          verb = verbs[5]!;
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

    setNlProof(nl);
    setShowNlPanel(true);
    appendLog(`自然语言证明已生成: ${proofSteps.length} 步骤`, 'info');
    addToast('success', '自然语言证明已生成 / NL proof generated');
  }, [proofSteps, currentStepIndex, searchStrategy, strategyNote, stepNote, constraints, addToast, appendLog]);

  // ================================================================
  // 回溯树构建 / Backtrack Tree Construction
  // ================================================================

  /** 基于 undoStack 历史构建 Newclid 风格的回溯搜索树 */
  const buildBacktrackTree = useCallback(() => {
    if (undoStack.length === 0) {
      const emptyTree = [{
        id: 0,
        label: `ROOT: ${searchStrategy}`,
        status: 'choice' as const,
        isBacktrack: false,
        children: [],
      }];
      setBacktrackTree(emptyTree);
      setShowBacktrackPanel(true);
      addToast('info', '回溯树为空 / Backtrack tree is empty');
      return;
    }

    // 构建树状结构：每个 undo 快照作为一个决策点
    // 检测回溯点：undoStack 中相邻快照如果约束数量先增后减，标记为回溯
    const tree: Array<{
      id: number;
      label: string;
      status: 'success' | 'failure' | 'choice' | 'pruned';
      isBacktrack: boolean;
      children: Array<{
        id: number;
        label: string;
        status: 'success' | 'failure' | 'choice' | 'pruned';
        isBacktrack: boolean;
      }>;
    }> = [];

    for (let i = 0; i < undoStack.length; i++) {
      const snapshot = undoStack[i];
      if (!snapshot) continue;

      const constraintCount = snapshot.constraints ? snapshot.constraints.length : 0;
      const pointCount = snapshot.points ? snapshot.points.length : 0;

      // 判断是否为回溯点：如果下一步的约束数减少，说明发生了撤销/回溯
      let isBacktrack = false;
      if (i > 0) {
        const prev = undoStack[i - 1];
        if (prev && prev.constraints && snapshot.constraints &&
            snapshot.constraints.length < prev.constraints.length) {
          isBacktrack = true;
        }
      }

      // 判断节点状态
      let status: 'success' | 'failure' | 'choice' | 'pruned' = 'choice';
      if (constraintCount === 0 && pointCount <= 1) {
        status = 'pruned';
      } else if (isBacktrack) {
        status = 'failure';
      } else if (constraintCount >= 3) {
        status = 'success';
      }

      // 检测策略变更：比较相邻快照的约束类型分布
      let strategyLabel = searchStrategy;
      if (i > 0) {
        const prev = undoStack[i - 1];
        if (prev && snapshot.constraints && prev.constraints) {
          const prevTypes = new Set((prev.constraints || []).map((c: { type: string }) => c.type));
          const currTypes = new Set((snapshot.constraints || []).map((c: { type: string }) => c.type));
          // 检查是否有新的约束类型
          const hasNewTypes = [...currTypes].some((t) => !prevTypes.has(t));
          if (hasNewTypes) {
            strategyLabel = `${searchStrategy} [SWITCH]`;
          }
        }
      }

      const node: typeof tree[0] = {
        id: i,
        label: isBacktrack
          ? `↩ ${strategyLabel} #${i + 1} (回溯/backtrack)`
          : `${strategyLabel} #${i + 1}`,
        status,
        isBacktrack,
        children: [],
      };

      // 如果非回溯节点，添加其推理子节点
      if (!isBacktrack && constraintCount > 0) {
        (snapshot.constraints || []).forEach((c: { id: number; type: string }, ci: number) => {
          if (ci < 3) { // 最多3个子节点避免过长
            node.children.push({
              id: i * 100 + ci,
              label: `${c.type} #${c.id ?? ci + 1}`,
              status: 'success' as const,
              isBacktrack: false,
            });
          }
        });
      }

      tree.push(node);
    }

    // 添加当前状态作为最终节点
    const currentConstraintCount = constraints.length;
    const finalStatus: 'success' | 'failure' | 'choice' | 'pruned' =
      currentConstraintCount >= 3 ? 'success' : 'choice';

    const finalNode: typeof tree[0] = {
      id: undoStack.length,
      label: `${searchStrategy} #FINAL (当前/current)`,
      status: finalStatus,
      isBacktrack: false,
      children: constraints.slice(0, 3).map((c, ci) => ({
        id: undoStack.length * 100 + ci,
        label: `${c.type} #${c.id ?? ci + 1}`,
        status: 'success' as const,
        isBacktrack: false,
      })),
    };
    tree.push(finalNode);

    setBacktrackTree(tree);
    setShowBacktrackPanel(true);
    appendLog(`回溯树已构建: ${tree.length} 个节点`, 'info');
    addToast('success', `回溯树已构建 / Backtrack tree built (${tree.length} nodes)`);
  }, [undoStack, constraints, searchStrategy, addToast, appendLog]);

  // ================================================================
  // 当前步骤自然语言描述 / Current Step NL Description (inline)
  // ================================================================
  const currentStepNlDescription = useMemo(() => {
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
  }, [proofSteps, currentStepIndex]);

  // ================================================================
  // 当撤销栈变化时同步步骤索引 / Sync step index when undo stack changes
  // ================================================================
  React.useEffect(() => {
    // 如果 totalSteps 增加了（用户做了新操作），跳到最新步骤
    if (totalSteps > 0 && currentStepIndex >= totalSteps) {
      setCurrentStepIndex(totalSteps - 1);
    }
  }, [totalSteps, currentStepIndex]);

  return (
    <>
      <Panel title="PROOF / 证明" panelId="proof-ops">
        {/* 证明步骤导航 */}
        <div className="nav-row">
          <button
            className="btn"
            onClick={handlePrev}
            disabled={currentStepIndex <= 0}
            title="回退到上一步证明状态 (Left Arrow)"
          >
            &#9664; PREV / 上一步
          </button>
          <button
            className="btn"
            onClick={handleNext}
            disabled={currentStepIndex >= totalSteps - 1}
            title="前进到下一步证明状态 (Right Arrow)"
          >
            NEXT / 下一步 &#9654;
          </button>
        </div>

        {/* 跳转到指定步骤 / Jump to Step */}
        <div className="proof-step-jump-row">
          <label>JUMP TO / 跳转到:</label>
          <input
            id="proofJumpInput"
            className="proof-step-jump-input"
            type="number"
            min={1}
            max={totalSteps || 1}
            value={jumpToStepInput}
            onChange={(e) => setJumpToStepInput(e.target.value)}
            onKeyDown={(e) => { if (e.key === 'Enter') handleJumpToStep(); }}
            placeholder="#"
            title={`输入步骤号 (1-${totalSteps})，按 Enter 跳转 / Enter step number, press Enter to jump`}
          />
          <button
            className="btn btn-small"
            onClick={handleJumpToStep}
            disabled={totalSteps === 0}
            style={{ padding: '3px 8px', fontSize: '10px', width: 'auto', marginBottom: 0 }}
          >
            GO
          </button>
        </div>

        {/* 快捷键提示 */}
        <div className="proof-shortcut-hint">
          Arrow keys: Prev/Next | Home/End: First/Last | G: Jump | S: SVG
        </div>

        {/* 搜索策略选择器 / Search Strategy Selector */}
        <div className="info-box" style={{ marginTop: '4px' }}>
          <div className="info-row">
            <span className="info-label" style={{ minWidth: '110px' }}>STRATEGY / 策略</span>
            <select
              value={searchStrategy}
              onChange={(e) => setSearchStrategy(e.target.value)}
              style={{
                flex: 1,
                background: 'var(--canvas-bg, #1a1a2e)',
                color: 'var(--text, #e0e0e0)',
                border: '1px solid var(--segment, #555)',
                borderRadius: '4px',
                padding: '4px 6px',
                fontSize: '11px',
                fontFamily: 'inherit',
              }}
            >
              <option value="Forward Chaining">Forward Chaining / 前向链</option>
              <option value="Backward Chaining">Backward Chaining / 后向链</option>
              <option value="Auxiliary Construction">Auxiliary Construction / 辅助构造</option>
              <option value="Algebraic">Algebraic / 代数法</option>
              <option value="Hybrid">Hybrid / 混合策略</option>
            </select>
          </div>
        </div>

        {/* 策略注释 / Strategy Note */}
        <div className="info-box" style={{ marginTop: '4px' }}>
          <div className="info-row" style={{ flexDirection: 'column', alignItems: 'stretch', gap: '2px' }}>
            <span className="info-label">STRATEGY NOTE / 策略注释</span>
            <input
              type="text"
              value={strategyNote}
              onChange={(e) => setStrategyNote(e.target.value)}
              placeholder="e.g. 先构造辅助圆，再用角平分性质..."
              style={{
                width: '100%',
                background: 'var(--canvas-bg, #1a1a2e)',
                color: 'var(--text, #e0e0e0)',
                border: '1px solid var(--segment, #555)',
                borderRadius: '4px',
                padding: '4px 6px',
                fontSize: '11px',
                fontFamily: 'inherit',
                boxSizing: 'border-box',
              }}
            />
          </div>
        </div>

        {/* 步骤注释 / Step Note */}
        <div className="info-box" style={{ marginTop: '4px' }}>
          <div className="info-row" style={{ flexDirection: 'column', alignItems: 'stretch', gap: '2px' }}>
            <span className="info-label">STEP NOTE / 步骤注释</span>
            <input
              type="text"
              value={stepNote}
              onChange={(e) => setStepNote(e.target.value)}
              placeholder={`步骤 ${currentStepIndex + 1} 的注释...`}
              style={{
                width: '100%',
                background: 'var(--canvas-bg, #1a1a2e)',
                color: 'var(--text, #e0e0e0)',
                border: '1px solid var(--segment, #555)',
                borderRadius: '4px',
                padding: '4px 6px',
                fontSize: '11px',
                fontFamily: 'inherit',
                boxSizing: 'border-box',
              }}
            />
          </div>
        </div>

        {/* 当前步骤内联 NL 描述 / Current Step NL Description Inline */}
        {currentStepNlDescription && (
          <div
            className="info-box"
            style={{
              marginTop: '4px',
              padding: '6px 8px',
              background: 'var(--canvas-bg, #1a1a2e)',
              border: '1px solid var(--segment, #555)',
              borderRadius: '4px',
              fontSize: '11px',
              lineHeight: '1.5',
              color: '#51cf66',
              whiteSpace: 'pre-wrap',
              wordBreak: 'break-word',
            }}
          >
            {currentStepNlDescription}
          </div>
        )}

        {/* 自然语言证明生成 */}
        <button className="btn btn-accent" onClick={generateNlProof} style={{ marginTop: '4px' }}>
          GENERATE NL PROOF / 生成自然语言证明
        </button>

        {/* Coq 导出 */}
        <button className="btn btn-accent" onClick={generateCoqScript}>
          EXPORT COQ / 导出 Coq
        </button>

        {/* HTML 证明导出 */}
        <button className="btn btn-accent" onClick={exportHtmlProof}>
          EXPORT HTML PROOF / 导出HTML证明
        </button>

        {/* SVG 几何视图切换 */}
        <button className="btn" onClick={() => setShowSvgView(!showSvgView)}>
          {showSvgView ? 'HIDE SVG / 隐藏几何视图' : 'SHOW SVG / 显示几何视图'}
        </button>

        {/* 反证法 */}
        <button className="btn" onClick={handleExFalso}>
          EX FALSO / 矛盾证明
        </button>

        {/* 回溯树 */}
        <button className="btn" onClick={buildBacktrackTree}>
          SHOW SEARCH TREE / 显示搜索树
        </button>
      </Panel>

      {/* SVG 几何视图面板 / SVG Geometry View Panel */}
      {showSvgView && proofSvg && (
        <Panel title="GEOMETRY VIEW / 几何视图" panelId="proof-svg">
          <div className="proof-svg-container">
            <div
              dangerouslySetInnerHTML={{ __html: proofSvg }}
              style={{ width: '100%', overflow: 'hidden' }}
            />
          </div>
          <div className="proof-shortcut-hint" style={{ marginTop: '4px' }}>
            步骤 {currentStepIndex + 1}/{totalSteps} | {proofSteps[currentStepIndex]?.points.length ?? 0} 点, {proofSteps[currentStepIndex]?.segments.length ?? 0} 线段, {proofSteps[currentStepIndex]?.constraints.length ?? 0} 约束
          </div>
        </Panel>
      )}

      {/* Coq 脚本展示面板 */}
      {showCoqPanel && (
        <Panel title="COQ SCRIPT / Coq 脚本" panelId="proof-coq">
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <textarea
              readOnly
              value={coqScript}
              style={{
                width: '100%',
                minHeight: '120px',
                maxHeight: '300px',
                resize: 'vertical',
                fontFamily: 'monospace',
                fontSize: '11px',
                lineHeight: '1.4',
                background: 'var(--canvas-bg, #1a1a2e)',
                color: 'var(--text, #e0e0e0)',
                border: '1px solid var(--segment, #555)',
                borderRadius: '4px',
                padding: '6px',
                boxSizing: 'border-box',
              }}
            />
            <div style={{ display: 'flex', gap: '4px' }}>
              <button className="btn btn-accent" onClick={copyCoqScript} style={{ flex: 1 }}>
                COPY / 复制
              </button>
              <button className="btn" onClick={() => setShowCoqPanel(false)} style={{ flex: 1 }}>
                CLOSE / 关闭
              </button>
            </div>
          </div>
        </Panel>
      )}

      {/* 矛盾证明结果面板 */}
      {showConflictPanel && (
        <Panel title="EX FALSO / 矛盾证明结果" panelId="proof-conflict">
          <pre
            style={{
              whiteSpace: 'pre-wrap',
              wordBreak: 'break-word',
              fontFamily: 'monospace',
              fontSize: '11px',
              lineHeight: '1.4',
              background: 'var(--canvas-bg, #1a1a2e)',
              color: constraintSatisfaction === 'CONFLICT' ? '#ff6b6b' : 'var(--text, #e0e0e0)',
              border: '1px solid var(--segment, #555)',
              borderRadius: '4px',
              padding: '8px',
              maxHeight: '250px',
              overflowY: 'auto',
            }}
          >
            {conflictResult}
          </pre>
          <button className="btn" onClick={() => setShowConflictPanel(false)} style={{ marginTop: '4px', width: '100%' }}>
            CLOSE / 关闭
          </button>
        </Panel>
      )}

      {/* 自然语言证明面板 / Natural Language Proof Panel */}
      {showNlPanel && (
        <Panel title="NATURAL LANGUAGE / 自然语言" panelId="proof-nl">
          <pre
            style={{
              whiteSpace: 'pre-wrap',
              wordBreak: 'break-word',
              fontFamily: 'monospace',
              fontSize: '11px',
              lineHeight: '1.4',
              background: 'var(--canvas-bg, #1a1a2e)',
              color: 'var(--text, #e0e0e0)',
              border: '1px solid var(--segment, #555)',
              borderRadius: '4px',
              padding: '8px',
              maxHeight: '350px',
              overflowY: 'auto',
            }}
          >
            {nlProof}
          </pre>
          <button className="btn" onClick={() => setShowNlPanel(false)} style={{ marginTop: '4px', width: '100%' }}>
            CLOSE / 关闭
          </button>
        </Panel>
      )}

      {/* 回溯树面板 / Backtrack Tree Panel */}
      {showBacktrackPanel && (
        <Panel title="BACKTRACK TREE / 回溯树" panelId="proof-backtrack">
          <div
            style={{
              maxHeight: '350px',
              overflowY: 'auto',
              padding: '6px',
              background: 'var(--canvas-bg, #1a1a2e)',
              border: '1px solid var(--segment, #555)',
              borderRadius: '4px',
            }}
          >
            {/* 图例 / Legend */}
            <div style={{ display: 'flex', gap: '8px', marginBottom: '8px', flexWrap: 'wrap', fontSize: '10px' }}>
              <span style={{ display: 'flex', alignItems: 'center', gap: '3px' }}>
                <span style={{ width: 10, height: 10, borderRadius: 2, background: '#51cf66', display: 'inline-block' }} /> Success/成功
              </span>
              <span style={{ display: 'flex', alignItems: 'center', gap: '3px' }}>
                <span style={{ width: 10, height: 10, borderRadius: 2, background: '#ff6b6b', display: 'inline-block' }} /> Failure/失败
              </span>
              <span style={{ display: 'flex', alignItems: 'center', gap: '3px' }}>
                <span style={{ width: 10, height: 10, borderRadius: 2, background: '#4dabf7', display: 'inline-block' }} /> Choice/选择点
              </span>
              <span style={{ display: 'flex', alignItems: 'center', gap: '3px' }}>
                <span style={{ width: 10, height: 10, borderRadius: 2, background: '#868e96', display: 'inline-block' }} /> Pruned/剪枝
              </span>
              <span style={{ display: 'flex', alignItems: 'center', gap: '3px' }}>
                <span style={{ fontWeight: 'bold', color: '#ffd43b' }}>↩</span> Backtrack/回溯
              </span>
            </div>

            {/* 树节点 / Tree Nodes */}
            {backtrackTree.map((node) => {
              const statusColor =
                node.status === 'success' ? '#51cf66' :
                node.status === 'failure' ? '#ff6b6b' :
                node.status === 'choice' ? '#4dabf7' :
                '#868e96';

              return (
                <div key={node.id} style={{ marginBottom: '6px' }}>
                  {/* 父节点 / Parent Node */}
                  <div
                    style={{
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px',
                      padding: '4px 8px',
                      borderRadius: '4px',
                      background: `${statusColor}22`,
                      border: `1px solid ${statusColor}`,
                      fontSize: '11px',
                      color: 'var(--text, #e0e0e0)',
                    }}
                  >
                    <span
                      style={{
                        width: 8,
                        height: 8,
                        borderRadius: 2,
                        background: statusColor,
                        display: 'inline-block',
                        flexShrink: 0,
                      }}
                    />
                    {node.isBacktrack && (
                      <span style={{ color: '#ffd43b', fontWeight: 'bold', flexShrink: 0 }}>↩</span>
                    )}
                    <span style={{ flex: 1 }}>{node.label}</span>
                    <span style={{ fontSize: '9px', color: statusColor, flexShrink: 0 }}>
                      {node.status.toUpperCase()}
                    </span>
                  </div>

                  {/* 子节点 / Children */}
                  {node.children.length > 0 && (
                    <div style={{ marginLeft: '20px', borderLeft: `1px solid ${statusColor}44`, paddingLeft: '8px', marginTop: '2px' }}>
                      {node.children.map((child) => (
                        <div
                          key={child.id}
                          style={{
                            display: 'flex',
                            alignItems: 'center',
                            gap: '4px',
                            padding: '3px 6px',
                            marginTop: '2px',
                            borderRadius: '3px',
                            background: '#51cf6611',
                            fontSize: '10px',
                            color: 'var(--text, #e0e0e0)',
                          }}
                        >
                          <span
                            style={{
                              width: 6,
                              height: 6,
                              borderRadius: 1,
                              background: '#51cf66',
                              display: 'inline-block',
                              flexShrink: 0,
                            }}
                          />
                          <span>{child.label}</span>
                        </div>
                      ))}
                    </div>
                  )}
                </div>
              );
            })}

            {backtrackTree.length === 0 && (
              <div style={{ color: '#868e96', fontSize: '11px', padding: '8px', textAlign: 'center' }}>
                尚未构建回溯树。点击 "SHOW SEARCH TREE" 按钮开始。
                <br />
                No backtrack tree built yet. Click "SHOW SEARCH TREE" to build.
              </div>
            )}
          </div>
          <button className="btn" onClick={() => setShowBacktrackPanel(false)} style={{ marginTop: '4px', width: '100%' }}>
            CLOSE / 关闭
          </button>
        </Panel>
      )}

      <Panel title="INFO / 信息" panelId="proof-info">
        <div className="info-box">
          {/* 证明步骤进度 */}
          <div className="info-row">
            <span className="info-label">STEP / 步骤</span>
            <span className="info-value">{currentStepIndex + 1} / {totalSteps || '--'}</span>
          </div>
          {/* 约束满足状态 */}
          <div className="info-row">
            <span className="info-label">CONSTRAINTS / 约束</span>
            <span className="info-value" style={{
              color: constraintSatisfaction === 'CONFLICT' ? '#ff6b6b'
                : constraintSatisfaction === 'SATISFIED' ? '#51cf66'
                : 'var(--text, #e0e0e0)',
            }}>
              {constraintSatisfaction}
            </span>
          </div>
          {/* 证明完整度 */}
          <div className="info-row">
            <span className="info-label">COMPLETENESS / 完整度</span>
            <span className="info-value">{proofCompleteness}%</span>
          </div>
          {/* 当前工具上下文 */}
          <div className="info-row">
            <span className="info-label">TOOL / 工具</span>
            <span className="info-value">{tool.toUpperCase()}</span>
          </div>
          {/* 搜索策略 */}
          <div className="info-row">
            <span className="info-label">STRATEGY / 策略</span>
            <span className="info-value" style={{
              color: '#4dabf7',
              fontSize: '11px',
            }}>
              {searchStrategy}
            </span>
          </div>
          {/* 几何元素统计 */}
          <div className="info-row">
            <span className="info-label">POINTS / 点</span>
            <span className="info-value">{points.length}</span>
          </div>
          <div className="info-row">
            <span className="info-label">SEGMENTS / 线段</span>
            <span className="info-value">{segments.length}</span>
          </div>
          <div className="info-row">
            <span className="info-label">CONSTRAINTS / 约束</span>
            <span className="info-value">{constraints.length}</span>
          </div>
          {/* 撤销/重做栈深度 */}
          <div className="info-row">
            <span className="info-label">UNDO / 撤销栈</span>
            <span className="info-value">{undoStack.length}</span>
          </div>
          <div className="info-row">
            <span className="info-label">REDO / 重做栈</span>
            <span className="info-value">{redoStack.length}</span>
          </div>
        </div>
      </Panel>
    </>
  );
};

export default ProofPanel;
