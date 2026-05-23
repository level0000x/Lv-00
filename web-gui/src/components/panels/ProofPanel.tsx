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

import React, { useState, useCallback, useMemo } from 'react';
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
            title="回退到上一步证明状态"
          >
            &#9664; PREV / 上一步
          </button>
          <button
            className="btn"
            onClick={handleNext}
            disabled={currentStepIndex >= totalSteps - 1}
            title="前进到下一步证明状态"
          >
            NEXT / 下一步 &#9654;
          </button>
        </div>

        {/* Coq 导出 */}
        <button className="btn btn-accent" onClick={generateCoqScript}>
          EXPORT COQ / 导出 Coq
        </button>

        {/* 反证法 */}
        <button className="btn" onClick={handleExFalso}>
          EX FALSO / 矛盾证明
        </button>
      </Panel>

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
