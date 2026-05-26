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
 *
 *              公共组件复用：
 *              - ConstraintModal: 统一的约束输入对话框
 *              - StatsRow: 统计信息键值对显示
 *              - Modal: 预设加载确认对话框
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
import ConstraintModal from '@/components/common/ConstraintModal';
import type { ConstraintField } from '@/components/common/ConstraintModal';
import StatsRow from '@/components/common/StatsRow';
import Modal from '@/components/common/Modal';

// ---- 提取的工具模块 / Extracted utility modules ----
import { GRAPH_PRESETS, generatePresetGeometry } from './utils/graphPresets';
import { formatDOF, formatTopoSort, formatGraphHash } from './utils/graphAnalysis';

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
  const showModal = useAppStore((s) => s.showModal);

  // ---- Input state / 输入状态 ----
  const [inputX, setInputX] = useState('');
  const [inputY, setInputY] = useState('');
  const [deleteNodeId, setDeleteNodeId] = useState('');
  const [deleteConstraintId, setDeleteConstraintId] = useState('');

  // ---- Constraint modal state / 约束对话框状态 ----
  /** 约束模态框类型 / Constraint modal type */
  type ConstraintModalType = 'incidence' | 'betweenness' | 'intersection' | 'containment' | null;
  const [constraintModal, setConstraintModal] = useState<ConstraintModalType>(null);

  /** 约束对话框的统一值状态 / Unified value state for constraint modal */
  const [constraintValues, setConstraintValues] = useState<Record<string, string>>({});

  // ---- Preset confirm state / 预设确认对话框状态 ----
  /** 待加载的预设 ID（非 null 时显示确认对话框）/ Pending preset ID (non-null shows confirm dialog) */
  const [pendingPresetId, setPendingPresetId] = useState<string | null>(null);

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

  /** 包含约束的元素选项列表（点 + 线段）/ Combined element options for containment (points + segments) */
  const elementOptions = useMemo(
    () => [
      ...pointOptions.map((o) => ({ ...o, label: `[Point] ${o.label}` })),
      ...segmentOptions.map((o) => ({ ...o, label: `[Seg] ${o.label}` })),
    ],
    [pointOptions, segmentOptions],
  );

  // ================================================================
  // Constraint modal helpers / 约束对话框辅助方法
  // ================================================================

  /**
   * 根据约束类型获取对话框标题和字段定义
   * Get modal title and field definitions based on constraint type
   */
  const getConstraintConfig = useCallback(
    (type: ConstraintModalType): { title: string; fields: ConstraintField[] } | null => {
      if (!type) return null;
      switch (type) {
        case 'incidence':
          return {
            title: 'INCIDENCE / 关联 (点在线上)',
            fields: [
              { label: 'Point / 点', name: 'point', options: pointOptions },
              { label: 'Segment / 线段', name: 'segment', options: segmentOptions },
            ],
          };
        case 'betweenness':
          return {
            title: 'BETWEENNESS / 之间 (B 介于 A 和 C 之间)',
            fields: [
              { label: 'A / 端点A', name: 'a', options: pointOptions },
              { label: 'B / 中间点', name: 'b', options: pointOptions },
              { label: 'C / 端点C', name: 'c', options: pointOptions },
            ],
          };
        case 'intersection':
          return {
            title: 'INTERSECTION / 相交 (两线段相交，自动创建交点)',
            fields: [
              { label: 'Seg 1 / 线段1', name: 'seg1', options: segmentOptions },
              { label: 'Seg 2 / 线段2', name: 'seg2', options: segmentOptions },
            ],
          };
        case 'containment':
          return {
            title: 'CONTAINMENT / 包含 (内部元素包含于外部元素)',
            fields: [
              { label: 'Inner / 内部', name: 'inner', options: elementOptions },
              { label: 'Outer / 外部', name: 'outer', options: elementOptions },
            ],
          };
        default:
          return null;
      }
    },
    [pointOptions, segmentOptions, elementOptions],
  );

  /** 打开约束对话框，重置值状态 / Open constraint modal, reset value state */
  const openConstraintModal = useCallback(
    (type: ConstraintModalType) => {
      setConstraintValues({});
      setConstraintModal(type);
    },
    [],
  );

  /** 关闭约束对话框 / Close constraint modal */
  const closeConstraintModal = useCallback(() => {
    setConstraintModal(null);
    setConstraintValues({});
  }, []);

  // ================================================================
  // NODE OPS / 节点操作
  // ================================================================

  /** 添加点到图中 / Add a point to the graph */
  const handleAddPoint = useCallback(() => {
    const x = parseFloat(inputX) || 0;
    const y = parseFloat(inputY) || 0;
    saveUndoState();
    addPoint({ id: generateUniqueId(), x, y });
    appendLog(`添加点: (${x}, ${y})`, 'info');
    setInputX('');
    setInputY('');
  }, [inputX, inputY, saveUndoState, addPoint, appendLog]);

  /** 添加线段（连接前两个点）/ Add segment (connects first two points) */
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

  /** 删除指定 ID 的节点 / Delete a node by ID */
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

  /** 删除指定 ID 的约束 / Delete a constraint by ID */
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

  /** 清空所有几何数据和分析结果 / Clear all geometry data and analysis results */
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

  /** 确认添加关联约束 / Confirm adding incidence constraint */
  const handleIncidence = useCallback(() => {
    const ptId = parseInt(constraintValues.point ?? '', 10);
    const segId = parseInt(constraintValues.segment ?? '', 10);
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
    closeConstraintModal();
  }, [constraintValues, saveUndoState, addConstraint, addToast, appendLog, closeConstraintModal]);

  /** 确认添加介于约束 / Confirm adding betweenness constraint */
  const handleBetweenness = useCallback(() => {
    const aId = parseInt(constraintValues.a ?? '', 10);
    const bId = parseInt(constraintValues.b ?? '', 10);
    const cId = parseInt(constraintValues.c ?? '', 10);
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
    closeConstraintModal();
  }, [constraintValues, saveUndoState, addConstraint, addToast, appendLog, closeConstraintModal]);

  /** 确认添加相交约束（自动计算交点并创建）/ Confirm adding intersection constraint (auto-computes and creates intersection point) */
  const handleIntersection = useCallback(() => {
    const s1Id = parseInt(constraintValues.seg1 ?? '', 10);
    const s2Id = parseInt(constraintValues.seg2 ?? '', 10);
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

    /** 计算两线段的交点 / Calculate intersection of two segments */
    const intersection = calculateIntersection(
      { p1, p2 },
      { p1: p3, p2: p4 },
    );

    if (!intersection) {
      addToast('warning', '线段不相交 / Segments do not intersect');
      return;
    }

    saveUndoState();

    // 创建交点 / Create the intersection point
    const newPointId = generateUniqueId();
    addPoint({ id: newPointId, x: intersection.x, y: intersection.y });

    // 添加相交约束 / Add the intersection constraint
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
    closeConstraintModal();
  }, [constraintValues, segments, points, saveUndoState, addPoint, addConstraint, addToast, appendLog, closeConstraintModal]);

  /** 确认添加包含约束 / Confirm adding containment constraint */
  const handleContainment = useCallback(() => {
    const innerId = parseInt(constraintValues.inner ?? '', 10);
    const outerId = parseInt(constraintValues.outer ?? '', 10);
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
    closeConstraintModal();
  }, [constraintValues, saveUndoState, addConstraint, addToast, appendLog, closeConstraintModal]);

  /**
   * 根据当前约束对话框类型，分发到对应的确认处理函数
   * Dispatch to the appropriate confirm handler based on current constraint modal type
   */
  const handleConstraintConfirm = useCallback(() => {
    switch (constraintModal) {
      case 'incidence':
        handleIncidence();
        break;
      case 'betweenness':
        handleBetweenness();
        break;
      case 'intersection':
        handleIntersection();
        break;
      case 'containment':
        handleContainment();
        break;
    }
  }, [constraintModal, handleIncidence, handleBetweenness, handleIntersection, handleContainment]);

  // ================================================================
  // ANALYSIS OPERATIONS / 分析操作
  // ================================================================

  /** 归一化图：合并距离过近的点 / Normalize graph: merge points that are too close */
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

  /** 查找可合并的候选点对 / Find merge candidate point pairs */
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

  /** 批准合并两个点 / Approve merging two points */
  const handleApproveMerge = useCallback(
    (a: number, b: number) => {
      saveUndoState();
      const keepId = Math.min(a, b);
      const removeId = Math.max(a, b);

      // 移除 ID 较大的点 / Remove the point with higher ID
      const newPoints = points.filter((p) => p.id !== removeId);
      // 更新引用被移除点的线段 / Update segments referencing the removed point
      const newSegments = segments
        .map((s) => ({
          ...s,
          p1: s.p1 === removeId ? keepId : s.p1,
          p2: s.p2 === removeId ? keepId : s.p2,
        }))
        .filter((s) => s.p1 !== s.p2);
      // 更新约束中的引用 / Update constraint references
      const newConstraints = constraints.map((c) => ({
        ...c,
        args: c.args.map((arg) => (arg === removeId ? keepId : arg)),
      }));

      setPoints(newPoints);
      setSegments(newSegments);
      setConstraints(newConstraints);

      // 从候选列表中移除已处理的条目 / Remove processed entry from candidates list
      setMergeCandidates((prev) => prev.filter((c) => !(c.a === a && c.b === b)));

      appendLog(`合并: P${removeId} -> P${keepId} / Merged: P${removeId} into P${keepId}`, 'info');
      addToast('success', `已合并 P${removeId} -> P${keepId} / Merged`);
    },
    [points, segments, constraints, saveUndoState, setPoints, setSegments, setConstraints, addToast, appendLog],
  );

  /** 跳过合并候选 / Skip a merge candidate */
  const handleRejectMerge = useCallback((a: number, b: number) => {
    setMergeCandidates((prev) => prev.filter((c) => !(c.a === a && c.b === b)));
  }, []);

  /** 检测冗余约束 / Detect redundant constraints */
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

  /** 移除单个冗余约束 / Remove a single redundant constraint */
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

  /** 检测约束冲突 / Detect constraint conflicts */
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

  /** 计算并显示自由度 / Calculate and display degrees of freedom */
  const handleDOF = useCallback(() => {
    const result = formatDOF(points, constraints);
    setAnalysisResult(result);
    appendLog(`自由度: ${result} / Degrees of freedom: ${result}`, 'info');
    addToast('info', result);
  }, [points, constraints, addToast, appendLog]);

  /** 执行拓扑排序 / Perform topological sort */
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

  /** 计算图哈希 / Compute graph hash */
  const handleGraphHash = useCallback(() => {
    const result = formatGraphHash(points, segments, constraints);
    setAnalysisResult(result);
    appendLog(`图哈希: ${result} / Graph hash computed`, 'info');
    addToast('info', result);
  }, [points, segments, constraints, addToast, appendLog]);

  // ================================================================
  // PRESETS / 预设
  // ================================================================

  /**
   * 请求加载预设：如果当前有几何数据，先弹出确认对话框
   * Request loading a preset: if geometry exists, show confirmation dialog first
   */
  const handleLoadPreset = useCallback(
    (presetId: string) => {
      const hasGeometry = points.length > 0 || segments.length > 0;
      if (hasGeometry) {
        // 使用 Modal 替代 window.confirm / Use Modal instead of window.confirm
        setPendingPresetId(presetId);
        showModal({
          id: 'preset-confirm',
          title: 'LOAD PRESET / 加载预设',
          content: '加载预设将清除当前几何数据，是否继续？\nLoading preset will clear current geometry. Continue?',
        });
      } else {
        doLoadPreset(presetId);
      }
    },
    [points, segments, showModal],
  );

  /**
   * 实际执行预设加载（清空当前数据，生成预设几何）
   * Actually execute preset loading (clear current data, generate preset geometry)
   */
  const doLoadPreset = useCallback(
    (presetId: string) => {
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

      // 应用所有新几何数据 / Apply all new geometry data
      for (const p of presetData.points) addPoint(p);
      for (const s of presetData.segments) addSegment(s);
      for (const c of presetData.constraints) addConstraint(c);

      // 居中视图 / Center view on new geometry
      setOffset(0, 0);
      setScale(1);

      appendLog(`加载预设: ${presetId} / Preset loaded: ${presetId}`, 'info');
      addToast('success', `预设已加载 / Preset loaded: ${presetId}`);
    },
    [saveUndoState, clearAll, addPoint, addSegment, addConstraint, setOffset, setScale, addToast, appendLog],
  );

  // ================================================================
  // Constraint modal config / 约束对话框配置（缓存）
  // ================================================================

  /** 当前约束对话框的配置（标题 + 字段）/ Current constraint modal config (title + fields) */
  const constraintConfig = useMemo(
    () => getConstraintConfig(constraintModal),
    [constraintModal, getConstraintConfig],
  );

  // ================================================================
  // RENDER / 渲染
  // ================================================================

  return (
    <>
      {/* NODE OPS / 节点操作 */}
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

      {/* CONSTRAINTS / 约束 */}
      <Panel title="CONSTRAINTS / 约束" panelId="graph-constraints">
        <button className="btn" onClick={() => openConstraintModal('incidence')}>
          INCIDENCE / 关联
        </button>
        <button className="btn" onClick={() => openConstraintModal('betweenness')}>
          BETWEENNESS / 之间
        </button>
        <button className="btn" onClick={() => openConstraintModal('intersection')}>
          INTERSECTION / 相交
        </button>
        <button className="btn" onClick={() => openConstraintModal('containment')}>
          CONTAINMENT / 包含
        </button>

        {/* 统一的约束对话框 / Unified constraint modal */}
        {constraintConfig && (
          <ConstraintModal
            title={constraintConfig.title}
            fields={constraintConfig.fields}
            values={constraintValues}
            onChange={(name, value) =>
              setConstraintValues((prev) => ({ ...prev, [name]: value }))
            }
            onConfirm={handleConstraintConfirm}
            onCancel={closeConstraintModal}
          />
        )}
      </Panel>

      {/* ANALYSIS / 分析 */}
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

        {/* 分析结果显示区域 / Analysis result display */}
        {analysisResult && (
          <StatsRow
            className="gp-analysis-result"
            items={[{ label: 'RESULT / 结果', value: analysisResult }]}
          />
        )}

        {/* 合并候选列表 / Merge candidates list */}
        {mergeCandidates.length > 0 && (
          <div className="gp-list-container">
            <div className="gp-list-title">
              MERGE CANDIDATES / 合并候选 ({mergeCandidates.length})
            </div>
            {mergeCandidates.map((c) => (
              <div
                key={`${c.a}-${c.b}`}
                className="gp-list-item"
              >
                <span>
                  P{c.a} -&gt; P{c.b} (d={c.dist.toFixed(3)})
                </span>
                <div className="gp-list-actions">
                  <button
                    className="btn btn-accent gp-list-btn"
                    onClick={() => handleApproveMerge(c.a, c.b)}
                  >
                    Merge
                  </button>
                  <button
                    className="btn gp-list-btn"
                    onClick={() => handleRejectMerge(c.a, c.b)}
                  >
                    Skip
                  </button>
                </div>
              </div>
            ))}
          </div>
        )}

        {/* 冗余约束列表 / Redundant constraints list */}
        {redundantIds.length > 0 && (
          <div className="gp-list-container">
            <div className="gp-list-title">
              REDUNDANT / 冗余 ({redundantIds.length})
            </div>
            {redundantIds.map((id) => (
              <div
                key={id}
                className="gp-list-item"
              >
                <span>C{id}</span>
                <div className="gp-list-actions">
                  <button
                    className="btn gp-list-btn"
                    onClick={() => handleRemoveRedundant(id)}
                  >
                    Remove
                  </button>
                </div>
              </div>
            ))}
          </div>
        )}

        {/* 冲突对列表 / Conflict pairs list */}
        {conflictPairs.length > 0 && (
          <div className="gp-list-container">
            <div className="gp-list-title">
              CONFLICTS / 冲突 ({conflictPairs.length})
            </div>
            {conflictPairs.map((cp, idx) => (
              <div
                key={idx}
                className="gp-conflict-item"
              >
                <span>C{cp.c1} -&gt; C{cp.c2}: {cp.reason}</span>
              </div>
            ))}
          </div>
        )}
      </Panel>

      {/* STATISTICS / 统计 */}
      <Panel title="STATISTICS / 统计" panelId="graph-stats">
        <StatsRow
          items={[
            { label: 'NODES / 节点数', value: points.length },
            { label: 'SEGMENTS / 线段数', value: segments.length },
            { label: 'CONSTRAINTS / 约束数', value: constraints.length },
            { label: 'MERGED / 已合并', value: mergedCount },
          ]}
        />
      </Panel>

      {/* PRESETS / 预设 */}
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

      {/* CLEAR / 清空 */}
      <Panel title="" panelId="graph-clear">
        <button className="btn" onClick={handleClear}>
          CLEAR ALL / 清空
        </button>
      </Panel>

      {/* 预设加载确认对话框 / Preset load confirmation modal */}
      <Modal
        id="preset-confirm"
        title="LOAD PRESET / 加载预设"
        onConfirm={() => {
          if (pendingPresetId) {
            doLoadPreset(pendingPresetId);
          }
          setPendingPresetId(null);
        }}
        onCancel={() => {
          setPendingPresetId(null);
        }}
        confirmLabel="OK / 确认"
        cancelLabel="CANCEL / 取消"
        danger
      >
        加载预设将清除当前几何数据，是否继续？
        {'\n'}Loading preset will clear current geometry. Continue?
      </Modal>
    </>
  );
};

export default GraphPanel;
