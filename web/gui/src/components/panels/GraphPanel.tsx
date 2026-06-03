/**
 * @module components/panels/GraphPanel
 * @description 图模块面板。
 *              提供节点/线段/区域操作、约束管理、分析工具、统计和预设。
 *
 *              Graph module panel.
 *              Provides node/segment/region operations, constraint management,
 *              analysis tools, statistics, and presets for the constraint graph.
 *
 *              预设几何和分析工具已提取到 utils/ 目录：
 *              - graphPresets.ts: 预设几何配置生成
 *              - graphAnalysis.ts: 分析结果格式化 + 合并操作工具
 */

import React, { useState, useCallback, useMemo } from 'react';
import Panel from './Panel';
import { useAppStore } from '@/stores';
import type { Constraint } from '@/types';
import { generateUniqueId } from '@/utils/idGenerator';
import {
  calculateIntersection,
  normalizeGraph,
  detectRedundantConstraints,
  detectConflicts,
  findMergeCandidates,
} from '@/utils/geometryAlgorithms';

// ---- 提取的工具模块 / Extracted utility modules ----
import { GRAPH_PRESETS, generatePresetGeometry } from './utils/graphPresets';
import { formatDOF, formatTopoSort, formatGraphHash } from './utils/graphAnalysis';

/**
 * Simple inline selector component for choosing a point or segment.
 * 简单的内联选择器组件，用于选择点或线段。
 */
const SelectorDropdown: React.FC<{
  label: string;
  value: string;
  onChange: (val: string) => void;
  options: Array<{ value: string; label: string }>;
}> = ({ label, value, onChange, options }) => (
  <div className="input-row" style={{ marginBottom: 4 }}>
    <label style={{ minWidth: 60, fontSize: 11 }}>{label}</label>
    <select
      className="input-field"
      value={value}
      onChange={(e) => onChange(e.target.value)}
      style={{ fontSize: 11 }}
    >
      <option value="">-- / 选择 --</option>
      {options.map((opt) => (
        <option key={opt.value} value={opt.value}>
          {opt.label}
        </option>
      ))}
    </select>
  </div>
);

/**
 * GraphPanel - Graph module sidebar panel
 * GraphPanel - 图模块侧边栏面板
 *
 * Sections / 面板分区:
 * - NODE OPS: Add/remove points and segments / 节点操作：添加/删除点和线段
 * - CONSTRAINTS: Add incidence, betweenness, intersection, containment / 约束：添加关联、介于、相交、包含
 * - ANALYSIS: Normalize, merge candidates, redundancy, conflicts, DOF, topo sort, hash / 分析：归一化、合并候选、冗余、冲突、自由度、拓扑排序、哈希
 * - STATISTICS: Node count, constraint count, merged count / 统计：节点数、约束数、已合并数
 * - PRESETS: Quick-load graph configurations / 预设：快速加载图配置
 * - CLEAR: Reset the graph / 清空：重置图
 */
const GraphPanel: React.FC = () => {
  const points = useAppStore((s) => s.points);
  const segments = useAppStore((s) => s.segments);
  const constraints = useAppStore((s) => s.constraints);
  const addPoint = useAppStore((s) => s.addPoint);
  const addSegment = useAppStore((s) => s.addSegment);
  const addConstraint = useAppStore((s) => s.addConstraint);
  const clearAll = useAppStore((s) => s.clearAll);
  const setPoints = useAppStore((s) => s.setPoints);
  const setSegments = useAppStore((s) => s.setSegments);
  const setConstraints = useAppStore((s) => s.setConstraints);
  const saveUndoState = useAppStore((s) => s.saveUndoState);
  const addToast = useAppStore((s) => s.addToast);
  const appendLog = useAppStore((s) => s.appendLog);
  const setOffset = useAppStore((s) => s.setOffset);
  const setScale = useAppStore((s) => s.setScale);

  // ---- Input state / 输入状态 ----
  const [inputX, setInputX] = useState('');
  const [inputY, setInputY] = useState('');
  const [deleteNodeId, setDeleteNodeId] = useState('');
  const [deleteConstraintId, setDeleteConstraintId] = useState('');

  // ---- Constraint modal state / 约束对话框状态 ----
  /** 约束模态框类型 / Constraint modal type */
  type ConstraintModalType = 'incidence' | 'betweenness' | 'intersection' | 'containment' | null;
  const [constraintModal, setConstraintModal] = useState<ConstraintModalType>(null);
  // Incidence: select point + segment
  const [incidencePointId, setIncidencePointId] = useState('');
  const [incidenceSegmentId, setIncidenceSegmentId] = useState('');
  // Betweenness: select 3 points
  const [betweenA, setBetweenA] = useState('');
  const [betweenB, setBetweenB] = useState('');
  const [betweenC, setBetweenC] = useState('');
  // Intersection: select 2 segments
  const [intersectSeg1, setIntersectSeg1] = useState('');
  const [intersectSeg2, setIntersectSeg2] = useState('');
  // Containment: select inner + outer elements
  const [containInner, setContainInner] = useState('');
  const [containOuter, setContainOuter] = useState('');

  // ---- Analysis result state / 分析结果状态 ----
  const [mergeCandidates, setMergeCandidates] = useState<Array<{ a: number; b: number; dist: number }>>([]);
  const [redundantIds, setRedundantIds] = useState<number[]>([]);
  const [conflictPairs, setConflictPairs] = useState<Array<{ c1: number; c2: number; reason: string }>>([]);
  const [analysisResult, setAnalysisResult] = useState<string>('');

  // ---- Statistics / 统计 ----
  const [mergedCount, setMergedCount] = useState(0);

  // ================================================================
  // Helper: build option lists / 辅助：构建选项列表
  // ================================================================

  /** 点选项列表（缓存） / Point options list (memoized) */
  const pointOptions = useMemo(
    () => points.map((p) => ({
      value: String(p.id),
      label: `P${p.id} (${p.x.toFixed(1)}, ${p.y.toFixed(1)})`,
    })),
    [points],
  );

  /** 线段选项列表（缓存） / Segment options list (memoized) */
  const segmentOptions = useMemo(
    () => segments.map((s) => {
      const p1 = points.find((p) => p.id === s.p1);
      const p2 = points.find((p) => p.id === s.p2);
      return {
        value: String(s.id),
        label: `S${s.id} (P${s.p1}->P${s.p2}${p1 && p2 ? ` [${p1.x.toFixed(0)},${p1.y.toFixed(0)}]->[${p2.x.toFixed(0)},${p2.y.toFixed(0)}]` : ''})`,
      };
    }),
    [segments, points],
  );

  // Combined element options for containment (points + segments)
  const elementOptions = [
    ...pointOptions.map((o) => ({ ...o, label: `[Point] ${o.label}` })),
    ...segmentOptions.map((o) => ({ ...o, label: `[Seg] ${o.label}` })),
  ];

  // ================================================================
  // NODE OPS / 节点操作
  // ================================================================

  const handleAddPoint = useCallback(() => {
    const x = parseFloat(inputX) || 0;
    const y = parseFloat(inputY) || 0;
    saveUndoState();
    addPoint({ id: generateUniqueId(), x, y });
    appendLog(`添加点: (${x}, ${y})`, 'info');
    setInputX('');
    setInputY('');
  }, [inputX, inputY, saveUndoState, addPoint, appendLog]);

  const handleAddSegment = useCallback(() => {
    if (points.length < 2) {
      addToast('warning', '需要至少两个点 / Need at least 2 points');
      return;
    }
    saveUndoState();
    const p1 = points[0]!;
    const p2 = points[1]!;
    addSegment({ p1: p1.id, p2: p2.id, id: generateUniqueId() });
    appendLog(`添加线段: P${p1.id} -> P${p2.id}`, 'info');
  }, [points, saveUndoState, addSegment, addToast, appendLog]);

  const handleDeleteNode = useCallback(() => {
    const id = parseInt(deleteNodeId, 10);
    if (isNaN(id)) {
      addToast('warning', '请输入有效的节点 ID / Enter a valid node ID');
      return;
    }
    saveUndoState();
    useAppStore.getState().removePoint(id);
    appendLog(`删除节点: P${id}`, 'info');
    setDeleteNodeId('');
  }, [deleteNodeId, saveUndoState, addToast, appendLog]);

  const handleDeleteConstraint = useCallback(() => {
    const id = parseInt(deleteConstraintId, 10);
    if (isNaN(id)) {
      addToast('warning', '请输入有效的约束 ID / Enter a valid constraint ID');
      return;
    }
    const exists = constraints.some((c) => c.id === id);
    if (!exists) {
      addToast('warning', `约束 ID ${id} 不存在 / Constraint ID ${id} not found`);
      return;
    }
    saveUndoState();
    const updated = constraints.filter((c) => c.id !== id);
    setConstraints(updated);
    appendLog(`删除约束: C${id}`, 'info');
    setDeleteConstraintId('');
  }, [deleteConstraintId, constraints, saveUndoState, setConstraints, addToast, appendLog]);

  const handleClear = useCallback(() => {
    clearAll();
    setMergeCandidates([]);
    setRedundantIds([]);
    setConflictPairs([]);
    setAnalysisResult('');
    setMergedCount(0);
    appendLog('图已清空 / Graph cleared', 'info');
  }, [clearAll, appendLog]);

  // ================================================================
  // CONSTRAINT OPERATIONS / 约束操作
  // ================================================================

  const handleIncidence = useCallback(() => {
    const ptId = parseInt(incidencePointId, 10);
    const segId = parseInt(incidenceSegmentId, 10);
    if (isNaN(ptId) || isNaN(segId)) {
      addToast('warning', '请选择点和线段 / Select a point and a segment');
      return;
    }
    saveUndoState();
    const newConstraint: Constraint = {
      id: generateUniqueId(),
      type: 'incidence',
      args: [ptId, segId],
    };
    addConstraint(newConstraint);
    appendLog(`添加关联约束: P${ptId} 在 S${segId} 上 / Incidence: P${ptId} on S${segId}`, 'info');
    addToast('success', `关联约束已添加 / Incidence added: P${ptId} on S${segId}`);
    setConstraintModal(null);
    setIncidencePointId('');
    setIncidenceSegmentId('');
  }, [incidencePointId, incidenceSegmentId, saveUndoState, addConstraint, addToast, appendLog]);

  const handleBetweenness = useCallback(() => {
    const aId = parseInt(betweenA, 10);
    const bId = parseInt(betweenB, 10);
    const cId = parseInt(betweenC, 10);
    if (isNaN(aId) || isNaN(bId) || isNaN(cId)) {
      addToast('warning', '请选择三个点 / Select three points (A, B, C)');
      return;
    }
    if (aId === bId || bId === cId || aId === cId) {
      addToast('warning', '三个点必须不同 / All three points must be different');
      return;
    }
    saveUndoState();
    const newConstraint: Constraint = {
      id: generateUniqueId(),
      type: 'betweenness',
      args: [aId, bId, cId],
    };
    addConstraint(newConstraint);
    appendLog(`添加介于约束: P${bId} 介于 P${aId} 和 P${cId} 之间 / Betweenness: P${bId} between P${aId} and P${cId}`, 'info');
    addToast('success', `介于约束已添加 / Betweenness added`);
    setConstraintModal(null);
    setBetweenA('');
    setBetweenB('');
    setBetweenC('');
  }, [betweenA, betweenB, betweenC, saveUndoState, addConstraint, addToast, appendLog]);

  const handleIntersection = useCallback(() => {
    const s1Id = parseInt(intersectSeg1, 10);
    const s2Id = parseInt(intersectSeg2, 10);
    if (isNaN(s1Id) || isNaN(s2Id)) {
      addToast('warning', '请选择两条线段 / Select two segments');
      return;
    }
    if (s1Id === s2Id) {
      addToast('warning', '请选择不同的线段 / Select different segments');
      return;
    }

    const seg1 = segments.find((s) => s.id === s1Id);
    const seg2 = segments.find((s) => s.id === s2Id);
    if (!seg1 || !seg2) {
      addToast('error', '线段未找到 / Segment not found');
      return;
    }

    const p1 = points.find((p) => p.id === seg1.p1);
    const p2 = points.find((p) => p.id === seg1.p2);
    const p3 = points.find((p) => p.id === seg2.p1);
    const p4 = points.find((p) => p.id === seg2.p2);

    if (!p1 || !p2 || !p3 || !p4) {
      addToast('error', '线段端点未找到 / Segment endpoint not found');
      return;
    }

    const intersection = calculateIntersection(
      { p1, p2 },
      { p1: p3, p2: p4 },
    );

    if (!intersection) {
      addToast('warning', '线段不相交 / Segments do not intersect');
      return;
    }

    saveUndoState();

    // Create the intersection point
    const newPointId = generateUniqueId();
    addPoint({ id: newPointId, x: intersection.x, y: intersection.y });

    // Add the intersection constraint
    const newConstraint: Constraint = {
      id: generateUniqueId(),
      type: 'intersection',
      args: [s1Id, s2Id],
    };
    addConstraint(newConstraint);

    appendLog(
      `添加相交约束: S${s1Id} 与 S${s2Id} 相交于 P${newPointId} (${intersection.x.toFixed(2)}, ${intersection.y.toFixed(2)})`,
      'info',
    );
    addToast('success', `相交约束已添加，交点 P${newPointId} / Intersection added`);
    setConstraintModal(null);
    setIntersectSeg1('');
    setIntersectSeg2('');
  }, [intersectSeg1, intersectSeg2, segments, points, saveUndoState, addPoint, addConstraint, addToast, appendLog]);

  const handleContainment = useCallback(() => {
    const innerId = parseInt(containInner, 10);
    const outerId = parseInt(containOuter, 10);
    if (isNaN(innerId) || isNaN(outerId)) {
      addToast('warning', '请选择内部和外部元素 / Select inner and outer elements');
      return;
    }
    if (innerId === outerId) {
      addToast('warning', '内部和外部元素不能相同 / Inner and outer must differ');
      return;
    }
    saveUndoState();
    const newConstraint: Constraint = {
      id: generateUniqueId(),
      type: 'containment',
      args: [innerId, outerId],
    };
    addConstraint(newConstraint);
    appendLog(`添加包含约束: ${innerId} 包含于 ${outerId} / Containment: ${innerId} in ${outerId}`, 'info');
    addToast('success', `包含约束已添加 / Containment added`);
    setConstraintModal(null);
    setContainInner('');
    setContainOuter('');
  }, [containInner, containOuter, saveUndoState, addConstraint, addToast, appendLog]);

  // ================================================================
  // ANALYSIS OPERATIONS / 分析操作
  // ================================================================

  const handleNormalize = useCallback(() => {
    if (points.length === 0) {
      addToast('warning', '没有点可以归一化 / No points to normalize');
      return;
    }
    saveUndoState();
    const result = normalizeGraph(points, segments, constraints);
    setPoints(result.mergedPoints);
    setSegments(result.mergedSegments);
    setConstraints(result.mergedConstraints);
    setMergedCount(result.mergedCount);
    appendLog(`归一化完成: 合并了 ${result.mergedCount} 个点 / Normalized: merged ${result.mergedCount} points`, 'info');
    addToast('success', `归一化完成，合并 ${result.mergedCount} 个点 / Normalized: ${result.mergedCount} merges`);
  }, [points, segments, constraints, saveUndoState, setPoints, setSegments, setConstraints, addToast, appendLog]);

  const handleFindMergeCandidates = useCallback(() => {
    if (points.length < 2) {
      addToast('warning', '需要至少两个点 / Need at least 2 points');
      return;
    }
    const candidates = findMergeCandidates(points);
    setMergeCandidates(candidates);
    if (candidates.length === 0) {
      addToast('info', '未找到合并候选 / No merge candidates found');
      appendLog('查找合并候选: 无结果 / Find merge candidates: none', 'info');
    } else {
      addToast('info', `找到 ${candidates.length} 个合并候选 / Found ${candidates.length} merge candidates`);
      appendLog(`查找合并候选: ${candidates.length} 个 / Found ${candidates.length} candidates`, 'info');
    }
  }, [points, addToast, appendLog]);

  const handleApproveMerge = useCallback(
    (a: number, b: number) => {
      saveUndoState();
      const keepId = Math.min(a, b);
      const removeId = Math.max(a, b);

      // Remove the point with higher ID
      const newPoints = points.filter((p) => p.id !== removeId);
      // Update segments referencing the removed point
      const newSegments = segments
        .map((s) => ({
          ...s,
          p1: s.p1 === removeId ? keepId : s.p1,
          p2: s.p2 === removeId ? keepId : s.p2,
        }))
        .filter((s) => s.p1 !== s.p2);
      // Update constraints
      const newConstraints = constraints.map((c) => ({
        ...c,
        args: c.args.map((arg) => (arg === removeId ? keepId : arg)),
      }));

      setPoints(newPoints);
      setSegments(newSegments);
      setConstraints(newConstraints);

      // Remove from candidates list
      setMergeCandidates((prev) => prev.filter((c) => !(c.a === a && c.b === b)));

      appendLog(`合并: P${removeId} -> P${keepId} / Merged: P${removeId} into P${keepId}`, 'info');
      addToast('success', `已合并 P${removeId} -> P${keepId} / Merged`);
    },
    [points, segments, constraints, saveUndoState, setPoints, setSegments, setConstraints, addToast, appendLog],
  );

  const handleRejectMerge = useCallback((a: number, b: number) => {
    setMergeCandidates((prev) => prev.filter((c) => !(c.a === a && c.b === b)));
  }, []);

  const handleDetectRedundant = useCallback(() => {
    if (constraints.length === 0) {
      addToast('warning', '没有约束可检测 / No constraints to check');
      return;
    }
    const redundant = detectRedundantConstraints(constraints);
    setRedundantIds(redundant);
    if (redundant.length === 0) {
      addToast('info', '未检测到冗余约束 / No redundant constraints detected');
      appendLog('检测冗余: 无冗余 / Detect redundant: none', 'info');
    } else {
      addToast('info', `检测到 ${redundant.length} 个冗余约束 / Detected ${redundant.length} redundant constraints`);
      appendLog(`检测冗余: ${redundant.map((id) => `C${id}`).join(', ')} / Redundant: ${redundant.map((id) => `C${id}`).join(', ')}`, 'info');
    }
  }, [constraints, addToast, appendLog]);

  const handleRemoveRedundant = useCallback(
    (id: number) => {
      saveUndoState();
      const updated = constraints.filter((c) => c.id !== id);
      setConstraints(updated);
      setRedundantIds((prev) => prev.filter((rid) => rid !== id));
      appendLog(`移除冗余约束: C${id} / Removed redundant: C${id}`, 'info');
      addToast('success', `已移除冗余约束 C${id} / Removed redundant C${id}`);
    },
    [constraints, saveUndoState, setConstraints, addToast, appendLog],
  );

  const handleDetectConflicts = useCallback(() => {
    if (constraints.length === 0) {
      addToast('warning', '没有约束可检测 / No constraints to check');
      return;
    }
    const conflicts = detectConflicts(constraints);
    setConflictPairs(conflicts);
    if (conflicts.length === 0) {
      addToast('info', '未检测到冲突 / No conflicts detected');
      appendLog('检测冲突: 无冲突 / Detect conflicts: none', 'info');
    } else {
      addToast('warning', `检测到 ${conflicts.length} 个冲突 / Detected ${conflicts.length} conflicts`);
      appendLog(`检测冲突: ${conflicts.length} 个 / Conflicts: ${conflicts.length}`, 'warn');
    }
  }, [constraints, addToast, appendLog]);

  const handleDOF = useCallback(() => {
    const result = formatDOF(points, constraints);
    setAnalysisResult(result);
    appendLog(`自由度: ${result} / Degrees of freedom: ${result}`, 'info');
    addToast('info', result);
  }, [points, constraints, addToast, appendLog]);

  const handleTopoSort = useCallback(() => {
    if (constraints.length === 0) {
      addToast('warning', '没有约束可排序 / No constraints to sort');
      return;
    }
    const result = formatTopoSort(constraints);
    setAnalysisResult(result);
    appendLog(`拓扑排序完成 / Topo sort done`, 'info');
    addToast('info', result);
  }, [constraints, addToast, appendLog]);

  const handleGraphHash = useCallback(() => {
    const result = formatGraphHash(points, segments, constraints);
    setAnalysisResult(result);
    appendLog(`图哈希: ${result} / Graph hash computed`, 'info');
    addToast('info', result);
  }, [points, segments, constraints, addToast, appendLog]);

  // ================================================================
  // PRESETS / 预设
  // ================================================================

  const handleLoadPreset = useCallback(
    (presetId: string) => {
      const hasGeometry = points.length > 0 || segments.length > 0;
      if (hasGeometry) {
        const confirmed = window.confirm(
          '加载预设将清除当前几何数据，是否继续？\nLoading preset will clear current geometry. Continue?',
        );
        if (!confirmed) return;
      }

      saveUndoState();
      clearAll();
      setMergeCandidates([]);
      setRedundantIds([]);
      setConflictPairs([]);
      setAnalysisResult('');
      setMergedCount(0);

      // 使用提取的预设生成工具 / Use extracted preset generation utility
      const presetData = generatePresetGeometry(presetId);
      if (!presetData) {
        addToast('warning', `未知预设 / Unknown preset: ${presetId}`);
        return;
      }

      // 应用所有新几何数据
      for (const p of presetData.points) addPoint(p);
      for (const s of presetData.segments) addSegment(s);
      for (const c of presetData.constraints) addConstraint(c);

      // 居中视图 / Center view on new geometry
      setOffset(0, 0);
      setScale(1);

      appendLog(`加载预设: ${presetId} / Preset loaded: ${presetId}`, 'info');
      addToast('success', `预设已加载 / Preset loaded: ${presetId}`);
    },
    [points, segments, saveUndoState, clearAll, addPoint, addSegment, addConstraint, setOffset, setScale, addToast, appendLog],
  );

  // ================================================================
  // RENDER / 渲染
  // ================================================================

  return (
    <>
      {/* NODE OPS */}
      <Panel title="NODE OPS / 节点操作" panelId="graph-nodes">
        <div className="input-row">
          <label>X</label>
          <input
            type="text"
            className="input-field"
            placeholder="X 坐标 / X coord"
            aria-label="X 坐标 / X coordinate"
            value={inputX}
            onChange={(e) => setInputX(e.target.value)}
          />
          <label>Y</label>
          <input
            type="text"
            className="input-field"
            placeholder="Y 坐标 / Y coord"
            aria-label="Y 坐标 / Y coordinate"
            value={inputY}
            onChange={(e) => setInputY(e.target.value)}
          />
        </div>
        <button className="btn btn-accent" onClick={handleAddPoint}>
          ADD POINT / 添加点
        </button>
        <button className="btn btn-accent" onClick={handleAddSegment}>
          ADD SEGMENT / 添加线段
        </button>
        <button className="btn" onClick={() => addToast('info', '区域功能开发中 / Region WIP')}>
          ADD REGION / 添加区域
        </button>
        <div className="btn-group-sep" />
        <div className="input-row">
          <input
            type="number"
            placeholder="节点ID / Node ID"
            className="input-field"
            aria-label="节点ID / Node ID"
            value={deleteNodeId}
            onChange={(e) => setDeleteNodeId(e.target.value)}
          />
          <input
            type="number"
            placeholder="约束ID / Constraint ID"
            className="input-field"
            aria-label="约束ID / Constraint ID"
            value={deleteConstraintId}
            onChange={(e) => setDeleteConstraintId(e.target.value)}
          />
        </div>
        <button className="btn" onClick={handleDeleteNode}>
          DELETE NODE / 删除节点
        </button>
        <button className="btn" onClick={handleDeleteConstraint}>
          DELETE CONSTRAINT / 删除约束
        </button>
      </Panel>

      {/* CONSTRAINTS */}
      <Panel title="CONSTRAINTS / 约束" panelId="graph-constraints">
        <button className="btn" onClick={() => setConstraintModal('incidence')}>
          INCIDENCE / 关联
        </button>
        <button className="btn" onClick={() => setConstraintModal('betweenness')}>
          BETWEENNESS / 之间
        </button>
        <button className="btn" onClick={() => setConstraintModal('intersection')}>
          INTERSECTION / 相交
        </button>
        <button className="btn" onClick={() => setConstraintModal('containment')}>
          CONTAINMENT / 包含
        </button>

        {/* Constraint Modal: Incidence */}
        {constraintModal === 'incidence' && (
          <div style={{ marginTop: 6, padding: 8, border: '1px solid var(--border)', borderRadius: 4 }}>
            <div style={{ fontSize: 11, marginBottom: 4, fontWeight: 600 }}>
              INCIDENCE / 关联 (点在线上)
            </div>
            <SelectorDropdown
              label="Point / 点"
              value={incidencePointId}
              onChange={setIncidencePointId}
              options={pointOptions}
            />
            <SelectorDropdown
              label="Segment / 线段"
              value={incidenceSegmentId}
              onChange={setIncidenceSegmentId}
              options={segmentOptions}
            />
            <div style={{ display: 'flex', gap: 4 }}>
              <button className="btn btn-accent" onClick={handleIncidence} style={{ flex: 1, fontSize: 11 }}>
                OK / 确认
              </button>
              <button className="btn" onClick={() => setConstraintModal(null)} style={{ flex: 1, fontSize: 11 }}>
                CANCEL / 取消
              </button>
            </div>
          </div>
        )}

        {/* Constraint Modal: Betweenness */}
        {constraintModal === 'betweenness' && (
          <div style={{ marginTop: 6, padding: 8, border: '1px solid var(--border)', borderRadius: 4 }}>
            <div style={{ fontSize: 11, marginBottom: 4, fontWeight: 600 }}>
              BETWEENNESS / 之间 (B 介于 A 和 C 之间)
            </div>
            <SelectorDropdown
              label="A / 端点A"
              value={betweenA}
              onChange={setBetweenA}
              options={pointOptions}
            />
            <SelectorDropdown
              label="B / 中间点"
              value={betweenB}
              onChange={setBetweenB}
              options={pointOptions}
            />
            <SelectorDropdown
              label="C / 端点C"
              value={betweenC}
              onChange={setBetweenC}
              options={pointOptions}
            />
            <div style={{ display: 'flex', gap: 4 }}>
              <button className="btn btn-accent" onClick={handleBetweenness} style={{ flex: 1, fontSize: 11 }}>
                OK / 确认
              </button>
              <button className="btn" onClick={() => setConstraintModal(null)} style={{ flex: 1, fontSize: 11 }}>
                CANCEL / 取消
              </button>
            </div>
          </div>
        )}

        {/* Constraint Modal: Intersection */}
        {constraintModal === 'intersection' && (
          <div style={{ marginTop: 6, padding: 8, border: '1px solid var(--border)', borderRadius: 4 }}>
            <div style={{ fontSize: 11, marginBottom: 4, fontWeight: 600 }}>
              INTERSECTION / 相交 (两线段相交，自动创建交点)
            </div>
            <SelectorDropdown
              label="Seg 1 / 线段1"
              value={intersectSeg1}
              onChange={setIntersectSeg1}
              options={segmentOptions}
            />
            <SelectorDropdown
              label="Seg 2 / 线段2"
              value={intersectSeg2}
              onChange={setIntersectSeg2}
              options={segmentOptions}
            />
            <div style={{ display: 'flex', gap: 4 }}>
              <button className="btn btn-accent" onClick={handleIntersection} style={{ flex: 1, fontSize: 11 }}>
                OK / 确认
              </button>
              <button className="btn" onClick={() => setConstraintModal(null)} style={{ flex: 1, fontSize: 11 }}>
                CANCEL / 取消
              </button>
            </div>
          </div>
        )}

        {/* Constraint Modal: Containment */}
        {constraintModal === 'containment' && (
          <div style={{ marginTop: 6, padding: 8, border: '1px solid var(--border)', borderRadius: 4 }}>
            <div style={{ fontSize: 11, marginBottom: 4, fontWeight: 600 }}>
              CONTAINMENT / 包含 (内部元素包含于外部元素)
            </div>
            <SelectorDropdown
              label="Inner / 内部"
              value={containInner}
              onChange={setContainInner}
              options={elementOptions}
            />
            <SelectorDropdown
              label="Outer / 外部"
              value={containOuter}
              onChange={setContainOuter}
              options={elementOptions}
            />
            <div style={{ display: 'flex', gap: 4 }}>
              <button className="btn btn-accent" onClick={handleContainment} style={{ flex: 1, fontSize: 11 }}>
                OK / 确认
              </button>
              <button className="btn" onClick={() => setConstraintModal(null)} style={{ flex: 1, fontSize: 11 }}>
                CANCEL / 取消
              </button>
            </div>
          </div>
        )}
      </Panel>

      {/* ANALYSIS */}
      <Panel title="ANALYSIS / 分析" panelId="graph-analysis">
        <button className="btn" onClick={handleNormalize}>
          NORMALIZE / 归一化
        </button>
        <button className="btn" onClick={handleFindMergeCandidates}>
          FIND MERGE CANDIDATES / 查找合并候选
        </button>
        <button className="btn" onClick={handleDetectRedundant}>
          DETECT REDUNDANT / 检测冗余约束
        </button>
        <button className="btn" onClick={handleDetectConflicts}>
          DETECT CONFLICTS / 检测冲突
        </button>
        <div className="btn-group-sep" />
        <button className="btn" onClick={handleDOF}>
          DEGREES OF FREEDOM / 自由度
        </button>
        <button className="btn" onClick={handleTopoSort}>
          TOPOLOGICAL SORT / 拓扑排序
        </button>
        <button className="btn" onClick={handleGraphHash}>
          GRAPH HASH / 图哈希
        </button>

        {/* Analysis result display */}
        {analysisResult && (
          <div className="info-box" style={{ marginTop: 6 }}>
            <div className="info-row">
              <span className="info-label">RESULT / 结果</span>
              <span className="info-value" style={{ fontSize: 10, wordBreak: 'break-all' }}>
                {analysisResult}
              </span>
            </div>
          </div>
        )}

        {/* Merge candidates display */}
        {mergeCandidates.length > 0 && (
          <div style={{ marginTop: 6, fontSize: 11 }}>
            <div style={{ fontWeight: 600, marginBottom: 4 }}>
              MERGE CANDIDATES / 合并候选 ({mergeCandidates.length})
            </div>
            {mergeCandidates.map((c) => (
              <div
                key={`${c.a}-${c.b}`}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                  padding: '2px 0',
                  borderBottom: '1px solid var(--border)',
                }}
              >
                <span>
                  P{c.a} -&gt; P{c.b} (d={c.dist.toFixed(3)})
                </span>
                <div style={{ display: 'flex', gap: 2 }}>
                  <button
                    className="btn btn-accent"
                    onClick={() => handleApproveMerge(c.a, c.b)}
                    style={{ fontSize: 10, padding: '1px 6px' }}
                  >
                    Merge
                  </button>
                  <button
                    className="btn"
                    onClick={() => handleRejectMerge(c.a, c.b)}
                    style={{ fontSize: 10, padding: '1px 6px' }}
                  >
                    Skip
                  </button>
                </div>
              </div>
            ))}
          </div>
        )}

        {/* Redundant constraints display */}
        {redundantIds.length > 0 && (
          <div style={{ marginTop: 6, fontSize: 11 }}>
            <div style={{ fontWeight: 600, marginBottom: 4 }}>
              REDUNDANT / 冗余 ({redundantIds.length})
            </div>
            {redundantIds.map((id) => (
              <div
                key={id}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                  padding: '2px 0',
                  borderBottom: '1px solid var(--border)',
                }}
              >
                <span>C{id}</span>
                <button
                  className="btn"
                  onClick={() => handleRemoveRedundant(id)}
                  style={{ fontSize: 10, padding: '1px 6px' }}
                >
                  Remove
                </button>
              </div>
            ))}
          </div>
        )}

        {/* Conflict pairs display */}
        {conflictPairs.length > 0 && (
          <div style={{ marginTop: 6, fontSize: 11 }}>
            <div style={{ fontWeight: 600, marginBottom: 4 }}>
              CONFLICTS / 冲突 ({conflictPairs.length})
            </div>
            {conflictPairs.map((cp, idx) => (
              <div
                key={idx}
                style={{
                  padding: '2px 0',
                  borderBottom: '1px solid var(--border)',
                }}
              >
                <span>C{cp.c1} -&gt; C{cp.c2}: {cp.reason}</span>
              </div>
            ))}
          </div>
        )}
      </Panel>

      {/* STATISTICS */}
      <Panel title="STATISTICS / 统计" panelId="graph-stats">
        <div className="info-box">
          <div className="info-row">
            <span className="info-label">NODES / 节点数</span>
            <span className="info-value">{points.length}</span>
          </div>
          <div className="info-row">
            <span className="info-label">SEGMENTS / 线段数</span>
            <span className="info-value">{segments.length}</span>
          </div>
          <div className="info-row">
            <span className="info-label">CONSTRAINTS / 约束数</span>
            <span className="info-value">{constraints.length}</span>
          </div>
          <div className="info-row">
            <span className="info-label">MERGED / 已合并</span>
            <span className="info-value">{mergedCount}</span>
          </div>
        </div>
      </Panel>

      {/* PRESETS */}
      <Panel title="PRESETS / 预设" panelId="graph-presets">
        <ul className="examples-list">
          {GRAPH_PRESETS.map((preset) => (
            <li
              key={preset.id}
              data-example={preset.id}
              onClick={() => handleLoadPreset(preset.id)}
            >
              {preset.label}
            </li>
          ))}
        </ul>
      </Panel>

      {/* CLEAR */}
      <Panel title="" panelId="graph-clear">
        <button className="btn" onClick={handleClear}>
          CLEAR ALL / 清空
        </button>
      </Panel>
    </>
  );
};

export default GraphPanel;
