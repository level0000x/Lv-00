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
 *
 *              核心业务逻辑已提取到 utils/ 目录：
 *              - proofSvgGenerator.ts: SVG 几何视图生成 + HTML 导出
 *              - proofCoqGenerator.ts: Coq 脚本生成 + 反证法叙述
 *              - proofNarrativeGenerator.ts: 自然语言证明生成
 *              - proofSearchTree.ts: 回溯搜索树构建
 */

import React, { useState, useCallback, useMemo, useEffect, useRef } from 'react';
import Panel from './Panel';
import { useAppStore } from '@/stores';
import { detectConflicts } from '@/utils/geometryAlgorithms';

// ---- 提取的工具模块 / Extracted utility modules ----
import { generateProofSvg, generateHtmlProof } from './utils/proofSvgGenerator';
import type { ProofSnapshot } from './utils/proofSvgGenerator';
import { generateCoqScript, generateExFalsoNarrative } from './utils/proofCoqGenerator';
import { generateNlProof, generateCurrentStepDescription } from './utils/proofNarrativeGenerator';
import { buildBacktrackTree } from './utils/proofSearchTree';
import type { BacktrackTreeNode } from './utils/proofSearchTree';

// ---- 安全 SVG 渲染组件 / Safe SVG rendering component ----
import ProofSvgView from './utils/ProofSvgView';

// ---- 通用统计行组件 / Reusable statistics row component ----
import StatsRow from '@/components/common/StatsRow';
import type { StatsItem } from '@/components/common/StatsRow';

// ---- 面板专用样式 / Panel-specific styles ----
import '@/styles/components/proof-panel.css';

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

  /** 当前活跃模块，用于键盘监听范围收窄 */
  const activeModule = useAppStore((s) => s.activeModule);

  // 用于恢复几何状态的方法
  const setPoints = useAppStore((s) => s.setPoints);
  const setSegments = useAppStore((s) => s.setSegments);
  const setConstraints = useAppStore((s) => s.setConstraints);
  const saveUndoState = useAppStore((s) => s.saveUndoState);

  // ================================================================
  // 面板容器引用（用于键盘事件委托）/ Panel container ref for keyboard delegation
  // ================================================================
  const panelContainerRef = useRef<HTMLDivElement>(null);

  // ================================================================
  // 证明步骤导航状态 / Proof Step Navigation State
  // ================================================================
  /**
   * 证明步骤：基于 undoStack 构建步骤数组。
   * 每个步骤对应撤销栈中的一个快照 + 当前状态。
   * proofSteps[0] = 初始状态（undoStack[0]），... proofSteps[N-1] = 当前状态
   */
  const proofSteps = useMemo((): ProofSnapshot[] => {
    // undoStack 中存储的是每次操作前的快照，按时间顺序排列
    // 加上当前状态作为最后一步
    const currentSnapshot: ProofSnapshot = {
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
  const [backtrackTree, setBacktrackTree] = useState<BacktrackTreeNode[]>([]);
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

  /** 恢复到指定步骤的几何状态 */
  const restoreToStep = useCallback((targetIndex: number) => {
    const snapshot = proofSteps[targetIndex];
    if (!snapshot) return;

    // 保存当前状态以便前进
    saveUndoState();

    // 恢复到目标步骤的几何状态
    setPoints(snapshot.points.map((p) => ({ ...p })));
    setSegments(snapshot.segments.map((s) => ({ ...s })));
    setConstraints(snapshot.constraints.map((c) => ({ ...c, args: [...c.args] })));
    setCurrentStepIndex(targetIndex);
  }, [proofSteps, saveUndoState, setPoints, setSegments, setConstraints]);

  /** 上一步：恢复到 undoStack 中更早的快照 */
  const handlePrev = useCallback(() => {
    if (currentStepIndex <= 0) {
      addToast('info', '已到第一步 / Already at first step');
      return;
    }
    const prevIndex = currentStepIndex - 1;
    restoreToStep(prevIndex);
    appendLog(`证明导航: 回到步骤 ${prevIndex + 1}/${totalSteps}`, 'info');
    addToast('info', `步骤 ${prevIndex + 1} / ${totalSteps}`);
  }, [currentStepIndex, totalSteps, restoreToStep, addToast, appendLog]);

  /** 下一步：恢复到 undoStack 中更晚的快照（或当前状态） */
  const handleNext = useCallback(() => {
    if (currentStepIndex >= totalSteps - 1) {
      addToast('info', '已是最新步骤 / Already at latest step');
      return;
    }
    const nextIndex = currentStepIndex + 1;
    restoreToStep(nextIndex);
    appendLog(`证明导航: 前进到步骤 ${nextIndex + 1}/${totalSteps}`, 'info');
    addToast('info', `步骤 ${nextIndex + 1} / ${totalSteps}`);
  }, [currentStepIndex, totalSteps, restoreToStep, addToast, appendLog]);

  /** 跳转到指定步骤 */
  const handleJumpToStep = useCallback(() => {
    const targetStep = parseInt(jumpToStepInput, 10);
    if (isNaN(targetStep) || targetStep < 1 || targetStep > totalSteps) {
      addToast('warning', `请输入有效步骤 (1-${totalSteps}) / Enter a valid step (1-${totalSteps})`);
      return;
    }
    const targetIndex = targetStep - 1;
    restoreToStep(targetIndex);
    setJumpToStepInput('');
    appendLog(`证明导航: 跳转到步骤 ${targetStep}/${totalSteps}`, 'info');
    addToast('info', `已跳转到步骤 ${targetStep} / Jumped to step ${targetStep}`);
  }, [jumpToStepInput, totalSteps, restoreToStep, addToast, appendLog]);

  // ================================================================
  // 键盘快捷键（仅在证明面板可见时响应）/ Keyboard Shortcuts (proof panel only)
  // ================================================================

  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      // 仅在证明模块激活时响应快捷键
      if (activeModule !== 'proof') return;

      // 如果焦点在输入框中，不处理快捷键（允许正常输入）
      const activeEl = document.activeElement;
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
            restoreToStep(0);
            addToast('info', '已跳到第一步 / Jumped to first step');
          }
          break;
        case 'End':
          e.preventDefault();
          if (totalSteps > 0) {
            restoreToStep(totalSteps - 1);
            addToast('info', '已跳到最新步骤 / Jumped to latest step');
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
  }, [handlePrev, handleNext, totalSteps, restoreToStep, addToast, activeModule]);

  // ================================================================
  // SVG 几何视图生成（委托给工具模块） / SVG Geometry View (delegated)
  // ================================================================

  const selectedPoint = useAppStore((s) => s.selectedPoint);

  /** 根据当前步骤的几何数据生成 SVG 字符串 */
  const proofSvg = useMemo(() => {
    const step = proofSteps[currentStepIndex];
    if (!step) return '';
    return generateProofSvg(step, 280, selectedPoint?.id);
  }, [proofSteps, currentStepIndex, selectedPoint]);

  // ================================================================
  // HTML 导出（委托给工具模块） / HTML Export (delegated)
  // ================================================================

  /** 生成并下载自包含的交互式 HTML 证明文件 */
  const exportHtmlProof = useCallback(() => {
    if (proofSteps.length === 0) {
      addToast('warning', '暂无证明步骤 / No proof steps to export');
      return;
    }

    const html = generateHtmlProof(
      proofSteps,
      currentStepIndex,
      points.length,
      segments.length,
      constraints.length,
    );

    // 下载 HTML 文件
    const timestamp = new Date().toISOString().slice(0, 19).replace(/[:-]/g, '');
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
  // Coq 导出（委托给工具模块） / Coq Export (delegated)
  // ================================================================

  /** 生成 Coq 格式的证明脚本 */
  const handleGenerateCoqScript = useCallback(() => {
    const script = generateCoqScript(points, segments, constraints);
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
  // 反证法（委托给工具模块） / Ex Falso (delegated)
  // ================================================================

  /** 分析约束冲突，生成矛盾证明叙述 */
  const handleExFalso = useCallback(() => {
    const narrative = generateExFalsoNarrative(constraints);
    setConflictResult(narrative);
    setShowConflictPanel(true);

    const conflictCount = detectConflicts(constraints).length;
    if (conflictCount === 0) {
      addToast('info', '未检测到矛盾 / No contradiction detected');
      appendLog('反证法分析: 约束集合一致，无矛盾', 'info');
    } else {
      addToast('warning', `检测到 ${conflictCount} 个矛盾 / ${conflictCount} contradiction(s) detected`);
      appendLog(`反证法分析: 检测到 ${conflictCount} 个约束冲突`, 'warn');
    }
  }, [constraints, addToast, appendLog]);

  // ================================================================
  // 自然语言证明生成（委托给工具模块） / NL Proof (delegated)
  // ================================================================

  /** 生成 AlphaGeometry 风格的自然语言步骤描述 */
  const handleGenerateNlProof = useCallback(() => {
    const nl = generateNlProof(
      proofSteps,
      currentStepIndex,
      searchStrategy,
      strategyNote,
      stepNote,
      constraints,
    );
    setNlProof(nl);
    setShowNlPanel(true);
    appendLog(`自然语言证明已生成: ${proofSteps.length} 步骤`, 'info');
    addToast('success', '自然语言证明已生成 / NL proof generated');
  }, [proofSteps, currentStepIndex, searchStrategy, strategyNote, stepNote, constraints, addToast, appendLog]);

  // ================================================================
  // 回溯树构建（委托给工具模块） / Backtrack Tree (delegated)
  // ================================================================

  /** 基于 undoStack 历史构建 Newclid 风格的回溯搜索树 */
  const handleBuildBacktrackTree = useCallback(() => {
    const tree = buildBacktrackTree(undoStack, constraints, searchStrategy);
    setBacktrackTree(tree);
    setShowBacktrackPanel(true);
    appendLog(`回溯树已构建: ${tree.length} 个节点`, 'info');
    addToast('success', `回溯树已构建 / Backtrack tree built (${tree.length} nodes)`);
  }, [undoStack, constraints, searchStrategy, addToast, appendLog]);

  // ================================================================
  // 当前步骤自然语言描述（委托给工具模块） / Current Step NL (delegated)
  // ================================================================

  /** 生成当前步骤的自然语言描述 */
  const currentStepNlDescription = useMemo(() => {
    return generateCurrentStepDescription(proofSteps, currentStepIndex);
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

  // ================================================================
  // 回溯树节点状态到 CSS 类名的映射 / Backtrack tree status -> CSS class mapping
  // ================================================================

  /** 根据节点状态返回对应的 CSS 后缀 */
  const getStatusSuffix = (status: string): string => {
    switch (status) {
      case 'success': return 'success';
      case 'failure': return 'failure';
      case 'choice': return 'choice';
      default: return 'pruned';
    }
  };

  // ================================================================
  // INFO 面板统计数据 / INFO Panel Statistics Data
  // ================================================================

  /** 构建 INFO 面板的统计行数据 */
  const infoStatsItems = useMemo((): StatsItem[] => {
    return [
      { label: 'STEP / 步骤', value: `${currentStepIndex + 1} / ${totalSteps || '--'}` },
      { label: 'CONSTRAINTS / 约束', value: constraintSatisfaction },
      { label: 'COMPLETENESS / 完整度', value: `${proofCompleteness}%` },
      { label: 'TOOL / 工具', value: tool.toUpperCase() },
      { label: 'STRATEGY / 策略', value: searchStrategy },
      { label: 'POINTS / 点', value: points.length },
      { label: 'SEGMENTS / 线段', value: segments.length },
      { label: 'CONSTRAINTS / 约束', value: constraints.length },
      { label: 'UNDO / 撤销栈', value: undoStack.length },
      { label: 'REDO / 重做栈', value: redoStack.length },
    ];
  }, [currentStepIndex, totalSteps, constraintSatisfaction, proofCompleteness, tool, searchStrategy, points.length, segments.length, constraints.length, undoStack.length, redoStack.length]);

  return (
    <div ref={panelContainerRef}>
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
            className="btn btn-small pp-jump-btn"
            onClick={handleJumpToStep}
            disabled={totalSteps === 0}
          >
            GO
          </button>
        </div>

        {/* 快捷键提示 */}
        <div className="proof-shortcut-hint">
          Arrow keys: Prev/Next | Home/End: First/Last | G: Jump | S: SVG
        </div>

        {/* 搜索策略选择器 / Search Strategy Selector */}
        <div className="info-box pp-strategy-box">
          <div className="info-row">
            <span className="info-label" style={{ minWidth: '110px' }}>STRATEGY / 策略</span>
            <select
              value={searchStrategy}
              onChange={(e) => setSearchStrategy(e.target.value)}
              className="pp-strategy-select"
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
        <div className="info-box pp-note-box">
          <div className="info-row pp-note-row">
            <span className="info-label">STRATEGY NOTE / 策略注释</span>
            <input
              type="text"
              value={strategyNote}
              onChange={(e) => setStrategyNote(e.target.value)}
              placeholder="e.g. 先构造辅助圆，再用角平分性质..."
              className="pp-note-input"
            />
          </div>
        </div>

        {/* 步骤注释 / Step Note */}
        <div className="info-box pp-note-box">
          <div className="info-row pp-note-row">
            <span className="info-label">STEP NOTE / 步骤注释</span>
            <input
              type="text"
              value={stepNote}
              onChange={(e) => setStepNote(e.target.value)}
              placeholder={`步骤 ${currentStepIndex + 1} 的注释...`}
              className="pp-note-input"
            />
          </div>
        </div>

        {/* 当前步骤内联 NL 描述 / Current Step NL Description Inline */}
        {currentStepNlDescription && (
          <div className="pp-step-description">
            {currentStepNlDescription}
          </div>
        )}

        {/* 自然语言证明生成 */}
        <button className="btn btn-accent pp-btn-margin-top" onClick={handleGenerateNlProof}>
          GENERATE NL PROOF / 生成自然语言证明
        </button>

        {/* Coq 导出 */}
        <button className="btn btn-accent" onClick={handleGenerateCoqScript}>
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
        <button className="btn" onClick={handleBuildBacktrackTree}>
          SHOW SEARCH TREE / 显示搜索树
        </button>
      </Panel>

      {/* SVG 几何视图面板 / SVG Geometry View Panel */}
      {showSvgView && proofSvg && (
        <Panel title="GEOMETRY VIEW / 几何视图" panelId="proof-svg">
          <div className="proof-svg-container">
            {/* 使用 ProofSvgView 安全渲染 SVG，替代 dangerouslySetInnerHTML */}
            <ProofSvgView svgString={proofSvg} />
          </div>
          <div className="proof-shortcut-hint pp-svg-info">
            步骤 {currentStepIndex + 1}/{totalSteps} | {proofSteps[currentStepIndex]?.points.length ?? 0} 点, {proofSteps[currentStepIndex]?.segments.length ?? 0} 线段, {proofSteps[currentStepIndex]?.constraints.length ?? 0} 约束
          </div>
        </Panel>
      )}

      {/* Coq 脚本展示面板 */}
      {showCoqPanel && (
        <Panel title="COQ SCRIPT / Coq 脚本" panelId="proof-coq">
          <div className="pp-coq-content">
            <textarea
              readOnly
              value={coqScript}
              className="pp-coq-textarea"
            />
            <div className="pp-coq-btn-row">
              <button className="btn btn-accent" onClick={copyCoqScript}>
                COPY / 复制
              </button>
              <button className="btn" onClick={() => setShowCoqPanel(false)}>
                CLOSE / 关闭
              </button>
            </div>
          </div>
        </Panel>
      )}

      {/* 矛盾证明结果面板 */}
      {showConflictPanel && (
        <Panel title="EX FALSO / 矛盾证明结果" panelId="proof-conflict">
          <pre className={`pp-conflict-pre${constraintSatisfaction === 'CONFLICT' ? ' pp-conflict-pre--conflict' : ''}`}>
            {conflictResult}
          </pre>
          <button className="btn pp-full-width-btn" onClick={() => setShowConflictPanel(false)}>
            CLOSE / 关闭
          </button>
        </Panel>
      )}

      {/* 自然语言证明面板 / Natural Language Proof Panel */}
      {showNlPanel && (
        <Panel title="NATURAL LANGUAGE / 自然语言" panelId="proof-nl">
          <pre className="pp-nl-pre">
            {nlProof}
          </pre>
          <button className="btn pp-full-width-btn" onClick={() => setShowNlPanel(false)}>
            CLOSE / 关闭
          </button>
        </Panel>
      )}

      {/* 回溯树面板 / Backtrack Tree Panel */}
      {showBacktrackPanel && (
        <Panel title="BACKTRACK TREE / 回溯树" panelId="proof-backtrack">
          <div className="pp-backtrack-scroll">
            {/* 图例 / Legend */}
            <div className="pp-backtrack-legend">
              <span className="pp-backtrack-legend-item">
                <span className="pp-backtrack-legend-dot pp-backtrack-legend-dot--success" /> Success/成功
              </span>
              <span className="pp-backtrack-legend-item">
                <span className="pp-backtrack-legend-dot pp-backtrack-legend-dot--failure" /> Failure/失败
              </span>
              <span className="pp-backtrack-legend-item">
                <span className="pp-backtrack-legend-dot pp-backtrack-legend-dot--choice" /> Choice/选择点
              </span>
              <span className="pp-backtrack-legend-item">
                <span className="pp-backtrack-legend-dot pp-backtrack-legend-dot--pruned" /> Pruned/剪枝
              </span>
              <span className="pp-backtrack-legend-item">
                <span className="pp-backtrack-legend-arrow">&#8617;</span> Backtrack/回溯
              </span>
            </div>

            {/* 树节点 / Tree Nodes */}
            {backtrackTree.map((node) => {
              const suffix = getStatusSuffix(node.status);

              return (
                <div key={node.id} className="pp-tree-node">
                  {/* 父节点 / Parent Node */}
                  <div className={`pp-tree-parent pp-tree-parent--${suffix}`}>
                    <span className={`pp-tree-status-dot pp-tree-status-dot--${suffix}`} />
                    {node.isBacktrack && (
                      <span className="pp-tree-backtrack-arrow">&#8617;</span>
                    )}
                    <span className="pp-tree-parent-label">{node.label}</span>
                    <span className={`pp-tree-parent-status pp-tree-parent-status--${suffix}`}>
                      {node.status.toUpperCase()}
                    </span>
                  </div>

                  {/* 子节点 / Children */}
                  {node.children.length > 0 && (
                    <div className="pp-tree-children">
                      {node.children.map((child) => (
                        <div
                          key={child.id}
                          className="pp-tree-child"
                        >
                          <span className="pp-tree-child-dot" />
                          <span>{child.label}</span>
                        </div>
                      ))}
                    </div>
                  )}
                </div>
              );
            })}

            {backtrackTree.length === 0 && (
              <div className="pp-backtrack-empty">
                尚未构建回溯树。点击 "SHOW SEARCH TREE" 按钮开始。
                <br />
                No backtrack tree built yet. Click "SHOW SEARCH TREE" to build.
              </div>
            )}
          </div>
          <button className="btn pp-full-width-btn" onClick={() => setShowBacktrackPanel(false)}>
            CLOSE / 关闭
          </button>
        </Panel>
      )}

      {/* INFO 面板：使用 StatsRow 组件展示统计信息 */}
      <Panel title="INFO / 信息" panelId="proof-info">
        <StatsRow items={infoStatsItems} />
      </Panel>
    </div>
  );
};

export default ProofPanel;
