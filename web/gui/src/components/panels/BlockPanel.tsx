/**
 * @module components/panels/BlockPanel
 * @description 函数块模块侧边栏面板。
 *
 *              实现功能：
 *              1. 预设函数块库 -- 提供常用几何构造的快捷操作
 *              2. 打包函数块（Pack）-- 将选中的几何元素封装为可复用函数块
 *              3. 例化（Instantiate）-- 选择已存储的函数块并应用到新的输入点
 *              4. 确定性检查、组合、乘积、部分应用、视图折叠 -- 增强占位按钮
 *
 *              参数输入区（ID/IN/OUT）已连接到本地状态，
 *              并实时展示来自 Store 的几何元素统计。
 *
 *              函数块执行逻辑已提取到 utils/ 目录：
 *              - funcBlockExecutor.ts: 预设执行、输入验证、结果格式化
 */

import React, { useState, useMemo, useCallback } from 'react';
import Panel from './Panel';
import { useAppStore } from '@/stores';
import { MAX_PANEL_LOG_ENTRIES } from '@/utils/constants';
import {
  FUNC_BLOCK_PRESETS,
  getNextId,
  composeBlocks,
  productBlocks,
  partialApplyBlock,
  validateComposition,
} from '@/utils/funcBlockPresets';
import type {
  FuncBlockPreset,
  FuncBlockCategory,
  UserFuncBlock,
} from '@/utils/funcBlockPresets';
import type { Point, Segment } from '@/types';

// ---- 提取的工具模块 / Extracted utility modules ----
import {
  validateInputs,
  executePreset,
  formatExecutionSummary,
} from './utils/funcBlockExecutor';

/**
 * BlockPanel - 函数块模块侧边栏面板
 *
 * 面板分区:
 * - PRESET LIBRARY: 预设函数块库（可滚动列表）
 * - FUNCTION BLOCK: 打包、例化、确定性检查、组合、乘积、部分应用、视图折叠
 * - PARAMETERS    : Block ID、输入/输出类型参数输入
 * - INFO          : 来自 Store 的实时函数块统计
 */
const BlockPanel: React.FC = () => {
  const addToast = useAppStore((s) => s.addToast);
  const appendLog = useAppStore((s) => s.appendLog);
  const saveUndoState = useAppStore((s) => s.saveUndoState);
  const addPoint = useAppStore((s) => s.addPoint);
  const addSegment = useAppStore((s) => s.addSegment);
  const addConstraint = useAppStore((s) => s.addConstraint);
  const points = useAppStore((s) => s.points);
  const segments = useAppStore((s) => s.segments);
  const selectedPoints = useAppStore((s) => s.selectedPoints);
  const setSelectedPoints = useAppStore((s) => s.setSelectedPoints);

  // ================================================================
  // 局部状态 —— 函数块参数输入
  // ================================================================
  const [blockId, setBlockId] = useState('');
  const [blockIn, setBlockIn] = useState('');
  const [blockOut, setBlockOut] = useState('');

  // ================================================================
  // 局部状态 —— 预设函数块选择
  // ================================================================
  /** 当前选中的预设分类过滤器 */
  const [categoryFilter, setCategoryFilter] = useState<FuncBlockCategory | 'all'>('all');
  /** 当前正在选择输入的预设（null 表示未在选择中） */
  const [activePreset, setActivePreset] = useState<FuncBlockPreset | null>(null);
  /** 已选择的输入点 ID 列表 */
  const [selectedInputIds, setSelectedInputIds] = useState<number[]>([]);
  /** 已选择的输入线段 ID 列表 */
  const [selectedSegmentIds, setSelectedSegmentIds] = useState<number[]>([]);
  /** 执行结果信息 */
  const [executionLog, setExecutionLog] = useState<string[]>([]);

  // ================================================================
  // 局部状态 —— 用户自定义函数块
  // ================================================================
  /** 用户打包的函数块列表 */
  const [userBlocks, setUserBlocks] = useState<UserFuncBlock[]>([]);
  /** 当前正在例化的用户函数块 */
  const [instantiatingBlock, setInstantiatingBlock] = useState<UserFuncBlock | null>(null);
  /** 例化时已选择的输入点 */
  const [instInputIds, setInstInputIds] = useState<number[]>([]);

  // ================================================================
  // 局部状态 —— 函数块组合/乘积/部分应用 (v3.5.0 新增)
  // ================================================================
  /** 组合操作：选中的第一个函数块 */
  const [composeSelect1, setComposeSelect1] = useState<UserFuncBlock | null>(null);
  /** 组合操作：选中的第二个函数块 */
  const [composeSelect2, setComposeSelect2] = useState<UserFuncBlock | null>(null);
  /** 乘积操作：选中的第一个函数块 */
  const [productSelect1, setProductSelect1] = useState<UserFuncBlock | null>(null);
  /** 乘积操作：选中的第二个函数块 */
  const [productSelect2, setProductSelect2] = useState<UserFuncBlock | null>(null);

  // ================================================================
  // 从 Store 读取真实数据
  // ================================================================
  const regions = useAppStore((s) => s.regions);
  const constraints = useAppStore((s) => s.constraints);

  // ================================================================
  // 预设函数块列表（按分类过滤）
  // ================================================================
  const filteredPresets = useMemo(() => {
    if (categoryFilter === 'all') return FUNC_BLOCK_PRESETS;
    return FUNC_BLOCK_PRESETS.filter((p) => p.category === categoryFilter);
  }, [categoryFilter]);

  // ================================================================
  // 辅助函数
  // ================================================================

  /** 向执行日志追加一条记录 */
  const log = useCallback((msg: string) => {
    setExecutionLog((prev) => [...prev.slice(-MAX_PANEL_LOG_ENTRIES), msg]);
  }, []);

  /** 清空执行日志 */
  const clearLog = useCallback(() => {
    setExecutionLog([]);
  }, []);

  /** 根据 ID 查找点 */
  const findPoint = useCallback(
    (id: number) => points.find((p) => p.id === id),
    [points],
  );

  /** 根据 ID 查找线段 */
  const findSegment = useCallback(
    (id: number) => segments.find((s) => s.id === id),
    [segments],
  );

  // ================================================================
  // 预设函数块操作
  // ================================================================

  /**
   * 点击预设块 —— 开始选择输入
   */
  const handlePresetClick = useCallback(
    (preset: FuncBlockPreset) => {
      setActivePreset(preset);
      setSelectedInputIds([]);
      setSelectedSegmentIds([]);
      log(`选择预设: ${preset.nameZh} (${preset.name})`);
      log(`需要输入: ${preset.segmentInput ? '线段' : `${preset.inputCount} 个点`}`);
      addToast('info', `请在画布上选择输入: ${preset.nameZh}`);
    },
    [log, addToast],
  );

  /**
   * 从画布已选点中收集输入并执行预设
   */
  const handleExecutePreset = useCallback(() => {
    if (!activePreset) {
      addToast('warning', '请先选择一个预设函数块 / Select a preset first');
      return;
    }

    const preset = activePreset;

    // 收集输入点
    const inputPoints = selectedInputIds
      .map((id) => findPoint(id))
      .filter((p): p is Point => p !== undefined);

    // 收集输入线段
    const inputSegments = selectedSegmentIds
      .map((id) => findSegment(id))
      .filter((s): s is Segment => s !== undefined);

    // 使用提取的输入验证工具
    const validationError = validateInputs(preset, inputPoints, inputSegments);
    if (validationError) {
      addToast('warning', validationError);
      return;
    }

    // 保存撤销状态
    saveUndoState();

    // 使用提取的执行工具
    const outcome = executePreset(preset, inputPoints, inputSegments, { findPoint, findSegment });

    if (!outcome.success || !outcome.result) {
      const errMsg = outcome.error ?? '未知错误';
      addToast('error', `执行预设失败: ${errMsg}`);
      appendLog(`执行预设失败: ${errMsg}`, 'error');
      return;
    }

    // 将结果添加到 Store
    outcome.result.newPoints.forEach((p) => addPoint(p));
    outcome.result.newSegments.forEach((s) => addSegment(s));
    outcome.result.newConstraints.forEach((c) => addConstraint(c));

    // 记录日志
    const summary = formatExecutionSummary(preset, outcome.result);
    log(summary);
    appendLog(summary, 'info');
    addToast('success', summary);

    // 重置选择状态
    setActivePreset(null);
    setSelectedInputIds([]);
    setSelectedSegmentIds([]);
    setSelectedPoints([]);
  }, [
    activePreset,
    selectedInputIds,
    selectedSegmentIds,
    findPoint,
    findSegment,
    points,
    saveUndoState,
    addPoint,
    addSegment,
    addConstraint,
    log,
    appendLog,
    addToast,
    setSelectedPoints,
  ]);

  /**
   * 从当前选中的点中添加输入
   */
  const handleAddSelectedPoints = useCallback(() => {
    if (!activePreset) return;
    const currentIds = [...selectedInputIds];
    for (const pt of selectedPoints) {
      if (!currentIds.includes(pt.id)) {
        currentIds.push(pt.id);
      }
    }
    setSelectedInputIds(currentIds);
    log(`已添加 ${selectedPoints.length} 个点到输入`);
  }, [activePreset, selectedInputIds, selectedPoints, log]);

  /**
   * 取消当前预设选择
   */
  const handleCancelPreset = useCallback(() => {
    setActivePreset(null);
    setSelectedInputIds([]);
    setSelectedSegmentIds([]);
    log('已取消预设选择');
  }, [log]);

  // ================================================================
  // PACK 操作 —— 打包函数块
  // ================================================================
  const handlePack = useCallback(() => {
    if (selectedPoints.length < 2) {
      addToast('warning', '请先在画布上选择至少 2 个点 / Select at least 2 points on canvas');
      return;
    }

    const name = blockId.trim() || `Block_${Date.now().toString(36)}`;
    const inputIds = selectedPoints.slice(0, 2).map((p) => p.id);
    const internalIds = selectedPoints.map((p) => p.id);

    // 计算内部点相对于第一个输入点的相对坐标
    const anchor = selectedPoints[0];
    if (!anchor) {
      addToast('error', '锚点不存在');
      return;
    }
    const relativePositions = selectedPoints.map((p) => ({
      id: p.id,
      relX: p.x - anchor.x,
      relY: p.y - anchor.y,
    }));

    // 查找与选中点关联的线段
    const relatedSegments = segments.filter(
      (s) => internalIds.includes(s.p1) && internalIds.includes(s.p2),
    );

    const newBlock: UserFuncBlock = {
      id: `ub_${Date.now()}`,
      name,
      inputPointIds: inputIds,
      internalPointIds: internalIds,
      internalSegmentIds: relatedSegments.map((s) => s.id),
      internalConstraintIds: [],
      relativePositions,
    };

    setUserBlocks((prev) => [...prev, newBlock]);
    const summary = `打包成功: "${name}" (输入 ${inputIds.length} 点, 内部 ${internalIds.length} 点, ${relatedSegments.length} 线段)`;
    log(summary);
    appendLog(summary, 'info');
    addToast('success', summary);
  }, [selectedPoints, segments, blockId, log, appendLog, addToast]);

  // ================================================================
  // INSTANTIATE 操作 —— 例化函数块
  // ================================================================
  const handleStartInstantiate = useCallback(
    (block: UserFuncBlock) => {
      setInstantiatingBlock(block);
      setInstInputIds([]);
      log(`开始例化: "${block.name}"，需要 ${block.inputPointIds.length} 个输入点`);
      addToast('info', `请选择 ${block.inputPointIds.length} 个新的输入点`);
    },
    [log, addToast],
  );

  const handleExecuteInstantiate = useCallback(() => {
    if (!instantiatingBlock) return;
    if (instInputIds.length < instantiatingBlock.inputPointIds.length) {
      addToast(
        'warning',
        `需要 ${instantiatingBlock.inputPointIds.length} 个输入点，已选 ${instInputIds.length} 个`,
      );
      return;
    }

    const firstInputId = instInputIds[0];
    if (firstInputId === undefined) {
      addToast('error', '输入点 ID 无效');
      return;
    }

    saveUndoState();

    // 获取原始函数块的锚点和相对坐标
    const origAnchor = instantiatingBlock.relativePositions[0];
    const newAnchor = findPoint(firstInputId);
    if (!origAnchor || !newAnchor) {
      addToast('error', '输入点不存在');
      return;
    }

    // 计算偏移量
    const offsetX = newAnchor.x - origAnchor.relX;
    const offsetY = newAnchor.y - origAnchor.relY;

    // 创建新的点（基于相对坐标 + 偏移）
    let createdCount = 0;
    for (const rel of instantiatingBlock.relativePositions) {
      const newId = getNextId();
      addPoint({
        id: newId,
        x: rel.relX + offsetX,
        y: rel.relY + offsetY,
      });
      createdCount++;
    }

    const summary = `例化 "${instantiatingBlock.name}" 完成: 创建 ${createdCount} 个点`;
    log(summary);
    appendLog(summary, 'info');
    addToast('success', summary);

    setInstantiatingBlock(null);
    setInstInputIds([]);
    setSelectedPoints([]);
  }, [
    instantiatingBlock,
    instInputIds,
    findPoint,
    saveUndoState,
    addPoint,
    log,
    appendLog,
    addToast,
    setSelectedPoints,
  ]);

  // ================================================================
  // 增强占位按钮
  // ================================================================

  /** 确定性检查 */
  const handleCheckDeterminism = useCallback(() => {
    const totalConstraints = constraints.length;
    const totalPoints = points.length;
    if (totalConstraints === 0) {
      addToast('info', '当前无约束，无法进行确定性检查 / No constraints to check');
      return;
    }
    // 简单的确定性分析：检查约束数量是否足以确定所有自由变量
    const freeVars = totalPoints * 2; // 每个点有 x, y 两个自由度
    const ratio = freeVars > 0 ? totalConstraints / freeVars : 0;
    let status: string;
    if (ratio >= 1) {
      status = `约束充足 (约束/自由度 = ${ratio.toFixed(2)})，系统可能是确定的`;
    } else {
      status = `约束不足 (约束/自由度 = ${ratio.toFixed(2)})，系统欠约束`;
    }
    addToast('info', `确定性检查: ${status}`);
    appendLog(`确定性检查: ${status}`, 'info');
    log(status);
  }, [constraints.length, points.length, addToast, appendLog, log]);

  // ================================================================
  // 函数块组合、乘积、部分应用操作 (v3.5.0 新增)
  // ================================================================

  /**
   * 组合操作处理 (v3.5.0)
   *
   * 支持两种模式：
   * 1. 无选中块时：提示用户选择函数块
   * 2. 选中第一个块后：继续选择第二个块
   * 3. 两个块都选中后：执行组合并添加结果
   */
  const handleCompose = useCallback(() => {
    if (userBlocks.length < 2) {
      addToast('info', '组合需要至少 2 个已打包的函数块 / Compose requires at least 2 packed blocks');
      return;
    }

    // 如果已选择第一个块，尝试执行组合
    if (composeSelect1) {
      if (!composeSelect2) {
        // 等待选择第二个块
        addToast('info', `已选择 "${composeSelect1.name}"，请继续选择第二个函数块...`);
        return;
      }

      // 验证兼容性
      if (!validateComposition(composeSelect1, composeSelect2)) {
        addToast('warning', '所选函数块不兼容，无法组合');
        setComposeSelect1(null);
        setComposeSelect2(null);
        return;
      }

      // 执行组合
      const result = composeBlocks(composeSelect1, composeSelect2);
      if (result.result) {
        setUserBlocks((prev) => [...prev, result.result]);
        addToast('success', result.description);
        appendLog(result.description, 'info');
        log(result.description);
      }

      // 重置选择状态
      setComposeSelect1(null);
      setComposeSelect2(null);
    } else {
      // 开始选择第一个块
      addToast('info', `组合功能: 当前有 ${userBlocks.length} 个函数块。请选择一个作为前级...`);
      appendLog('组合操作: 选择第一个函数块（前级）', 'info');
    }
  }, [userBlocks, composeSelect1, composeSelect2, addToast, appendLog, log]);

  /**
   * 处理函数块选择（用于组合操作）
   */
  const handleBlockSelectForCompose = useCallback((block: UserFuncBlock) => {
    if (!composeSelect1) {
      // 选择第一个块
      setComposeSelect1(block);
      addToast('info', `已选择 "${block.name}" 作为前级，请选择后级...`);
    } else if (!composeSelect2) {
      // 选择第二个块
      if (block.id === composeSelect1.id) {
        addToast('warning', '不能选择同一个函数块');
        return;
      }
      setComposeSelect2(block);

      // 验证兼容性
      if (!validateComposition(composeSelect1, block)) {
        addToast('warning', '所选函数块不兼容');
        setComposeSelect1(null);
        setComposeSelect2(null);
        return;
      }

      // 自动执行组合
      const result = composeBlocks(composeSelect1, block);
      if (result.result) {
        setUserBlocks((prev) => [...prev, result.result]);
        addToast('success', result.description);
        appendLog(result.description, 'info');
        log(result.description);
      }

      // 重置选择状态
      setComposeSelect1(null);
      setComposeSelect2(null);
    }
  }, [composeSelect1, addToast, appendLog, log]);

  /**
   * 取消组合选择
   */
  const handleCancelCompose = useCallback(() => {
    setComposeSelect1(null);
    setComposeSelect2(null);
    addToast('info', '已取消组合操作');
  }, [addToast]);

  /**
   * 乘积操作处理 (v3.5.0)
   *
   * 支持两种模式：
   * 1. 无选中块时：提示用户选择函数块
   * 2. 选中第一个块后：继续选择第二个块
   * 3. 两个块都选中后：执行乘积并添加结果
   */
  const handleProduct = useCallback(() => {
    if (userBlocks.length < 2) {
      addToast('info', '乘积需要至少 2 个已打包的函数块 / Product requires at least 2 packed blocks');
      return;
    }

    // 如果已选择第一个块，尝试执行乘积
    if (productSelect1) {
      if (!productSelect2) {
        // 等待选择第二个块
        addToast('info', `已选择 "${productSelect1.name}"，请继续选择第二个函数块...`);
        return;
      }

      // 执行乘积
      const result = productBlocks(productSelect1, productSelect2);
      if (result.result) {
        setUserBlocks((prev) => [...prev, result.result!]);
        addToast('success', result.description);
        appendLog(result.description, 'info');
        log(result.description);
      }

      // 重置选择状态
      setProductSelect1(null);
      setProductSelect2(null);
    } else {
      // 开始选择第一个块
      addToast('info', `乘积功能: 当前有 ${userBlocks.length} 个函数块。请选择一个...`);
      appendLog('乘积操作: 选择第一个函数块', 'info');
    }
  }, [userBlocks, productSelect1, productSelect2, addToast, appendLog, log]);

  /**
   * 处理函数块选择（用于乘积操作）
   */
  const handleBlockSelectForProduct = useCallback((block: UserFuncBlock) => {
    if (!productSelect1) {
      // 选择第一个块
      setProductSelect1(block);
      addToast('info', `已选择 "${block.name}"，请选择第二个函数块...`);
    } else if (!productSelect2) {
      // 选择第二个块
      if (block.id === productSelect1.id) {
        addToast('warning', '不能选择同一个函数块');
        return;
      }
      setProductSelect2(block);

      // 自动执行乘积
      const result = productBlocks(productSelect1, block);
      if (result.result) {
        setUserBlocks((prev) => [...prev, result.result]);
        addToast('success', result.description);
        appendLog(result.description, 'info');
        log(result.description);
      }

      // 重置选择状态
      setProductSelect1(null);
      setProductSelect2(null);
    }
  }, [productSelect1, addToast, appendLog, log]);

  /**
   * 取消乘积选择
   */
  const handleCancelProduct = useCallback(() => {
    setProductSelect1(null);
    setProductSelect2(null);
    addToast('info', '已取消乘积操作');
  }, [addToast]);

  /**
   * 部分应用操作处理 (v3.5.0)
   *
   * 简化实现：使用默认的相对坐标值进行部分应用
   * 实际使用中，用户可以通过修改生成的函数块来调整固定值
   */
  const handlePartialApply = useCallback(() => {
    if (userBlocks.length === 0) {
      addToast('info', '部分应用需要至少 1 个已打包的函数块 / Partial apply requires at least 1 packed block');
      return;
    }

    // 选择第一个可用的函数块进行部分应用
    const block = userBlocks[0];
    if (!block || block.inputPointIds.length < 2) {
      addToast('warning', '所选函数块没有足够的输入参数进行部分应用');
      return;
    }

    // 固定第一个输入，使用默认坐标 (0, 0)
    const fixedInputIndices = [0];
    const fixedInputValues = [{ relX: 0, relY: 0 }];

    const result = partialApplyBlock(block, fixedInputIndices, fixedInputValues);
    if (result.result) {
      setUserBlocks((prev) => [...prev, result.result]);
      addToast('success', result.description);
      appendLog(result.description, 'info');
      log(result.description);
    } else {
      addToast('error', `部分应用失败: ${result.description}`);
    }
  }, [userBlocks, addToast, appendLog, log]);

  /** 视图折叠 */
  const handleViewFold = useCallback(() => {
    if (userBlocks.length === 0) {
      addToast('info', '视图折叠需要至少 1 个已打包的函数块 / View fold requires at least 1 packed block');
      return;
    }
    addToast('info', '视图折叠: 将函数块的内部构造折叠为单个盒子视图，或展开显示内部细节。');
    appendLog('视图折叠: 切换函数块的展开/折叠视图', 'info');
  }, [userBlocks.length, addToast, appendLog]);

  // ================================================================
  // 渲染
  // ================================================================
  return (
    <>
      {/* ============================================================ */}
      {/* PRESET LIBRARY / 预设函数块库 */}
      {/* ============================================================ */}
      <Panel title="PRESET LIBRARY / 预设库" panelId="block-presets">
        {/* 分类过滤按钮 */}
        <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap', marginBottom: '8px' }}>
          {(['all', 'construction', 'measurement', 'transform'] as const).map((cat) => (
            <button
              key={cat}
              className={`btn ${categoryFilter === cat ? 'btn-accent' : ''}`}
              style={{ fontSize: '11px', padding: '2px 6px' }}
              onClick={() => setCategoryFilter(cat)}
            >
              {cat === 'all' ? '全部' : cat === 'construction' ? '构造' : cat === 'measurement' ? '度量' : '变换'}
            </button>
          ))}
        </div>

        {/* 预设块列表（可滚动） */}
        <div
          style={{
            maxHeight: '240px',
            overflowY: 'auto',
            border: '1px solid var(--border-color, #333)',
            borderRadius: '4px',
          }}
        >
          {filteredPresets.map((preset) => (
            <div
              key={preset.id}
              role="button"
              tabIndex={0}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
                padding: '6px 8px',
                cursor: 'pointer',
                borderBottom: '1px solid var(--border-color, #222)',
                backgroundColor:
                  activePreset?.id === preset.id
                    ? 'var(--accent-color, #4a9eff22)'
                    : 'transparent',
              }}
              onClick={() => handlePresetClick(preset)}
              onKeyDown={(e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); handlePresetClick(preset); } }}
              title={preset.description}
            >
              <span style={{ fontSize: '16px', width: '20px', textAlign: 'center' }}>
                {preset.icon}
              </span>
              <div style={{ flex: 1, minWidth: 0 }}>
                <div style={{ fontSize: '12px', fontWeight: 600 }}>{preset.nameZh}</div>
                <div style={{ fontSize: '10px', opacity: 0.7, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                  {preset.name}
                </div>
              </div>
              <span style={{ fontSize: '10px', opacity: 0.5 }}>
                {preset.segmentInput ? '线段' : `${preset.inputCount}点`}
              </span>
            </div>
          ))}
        </div>

        {/* 当前预设的输入选择状态 */}
        {activePreset && (
          <div style={{ marginTop: '8px' }}>
            <div style={{ fontSize: '11px', marginBottom: '4px', opacity: 0.8 }}>
              当前: {activePreset.nameZh} | 已选点: {selectedInputIds.length} | 已选线段: {selectedSegmentIds.length}
            </div>
            <div style={{ display: 'flex', gap: '4px' }}>
              <button className="btn" style={{ fontSize: '11px' }} onClick={handleAddSelectedPoints}>
                + 添加已选点
              </button>
              <button className="btn btn-accent" style={{ fontSize: '11px' }} onClick={handleExecutePreset}>
                执行 / EXECUTE
              </button>
              <button className="btn" style={{ fontSize: '11px' }} onClick={handleCancelPreset}>
                取消
              </button>
            </div>
          </div>
        )}
      </Panel>

      {/* ============================================================ */}
      {/* FUNCTION BLOCK / 函数块操作 */}
      {/* ============================================================ */}
      <Panel title="FUNCTION BLOCK / 函数块" panelId="block-func">
        {/* 打包函数块 —— 将选中的几何元素封装为可复用函数 */}
        <button className="btn btn-accent" onClick={handlePack}>
          PACK FUNCTION / 打包函数块
        </button>
        {/* 例化 —— 选择已存储的函数块并应用到新的输入点 */}
        <button className="btn btn-accent" onClick={() => {
          if (userBlocks.length === 0) {
            addToast('warning', '没有已打包的函数块 / No packed blocks available');
            return;
          }
          addToast('info', '请在下方用户函数块列表中选择一个进行例化');
        }}>
          INSTANTIATE / 例化
        </button>
        <div className="btn-group-sep" />
        {/* 确定性检查 —— 分析约束系统的确定性 */}
        <button className="btn" onClick={handleCheckDeterminism}>
          CHECK DETERMINISM / 确定性检查
        </button>
        {/* 组合 —— 串联两个函数块 */}
        <button className="btn" onClick={handleCompose}>
          COMPOSE / 组合
        </button>
        {/* 乘积 —— 并行执行两个函数块 */}
        <button className="btn" onClick={handleProduct}>
          PRODUCT / 乘积
        </button>
        {/* 部分应用 —— 柯里化 */}
        <button className="btn" onClick={handlePartialApply}>
          PARTIAL APPLY / 部分应用
        </button>
        {/* 视图折叠 —— 切换函数块的展开/折叠视图 */}
        <button className="btn" onClick={handleViewFold}>
          VIEW FOLD / 视图折叠
        </button>
      </Panel>

      {/* ============================================================ */}
      {/* USER BLOCKS / 用户函数块列表 */}
      {/* ============================================================ */}
      <Panel title="USER BLOCKS / 用户块" panelId="block-user">
        {/* 组合/乘积操作状态提示 */}
        {(composeSelect1 || productSelect1) && (
          <div style={{ fontSize: '11px', marginBottom: '6px', padding: '4px', backgroundColor: 'var(--accent-color, #4a9eff22)', borderRadius: '4px' }}>
            {composeSelect1 && (
              <div>
                组合操作进行中: 已选 "{composeSelect1.name}"
                <button className="btn" style={{ fontSize: '10px', marginLeft: '6px', padding: '1px 4px' }} onClick={handleCancelCompose}>
                  取消
                </button>
              </div>
            )}
            {productSelect1 && (
              <div>
                乘积操作进行中: 已选 "{productSelect1.name}"
                <button className="btn" style={{ fontSize: '10px', marginLeft: '6px', padding: '1px 4px' }} onClick={handleCancelProduct}>
                  取消
                </button>
              </div>
            )}
          </div>
        )}

        {userBlocks.length === 0 ? (
          <div style={{ fontSize: '11px', opacity: 0.5 }}>
            暂无用户函数块 / No user blocks. Use PACK to create one.
          </div>
        ) : (
          <div style={{ maxHeight: '120px', overflowY: 'auto' }}>
            {userBlocks.map((block) => {
              // 检查是否为当前选中状态
              const isComposeSelected = composeSelect1?.id === block.id;
              const isProductSelected = productSelect1?.id === block.id;

              return (
                <div
                  key={block.id}
                  style={{
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between',
                    padding: '4px 6px',
                    borderBottom: '1px solid var(--border-color, #222)',
                    fontSize: '11px',
                    backgroundColor: isComposeSelected || isProductSelected ? 'var(--accent-color, #4a9eff33)' : 'transparent',
                    cursor: composeSelect1 || productSelect1 ? 'pointer' : 'default',
                  }}
                  onClick={() => {
                    if (composeSelect1) {
                      handleBlockSelectForCompose(block);
                    } else if (productSelect1) {
                      handleBlockSelectForProduct(block);
                    }
                  }}
                >
                  <span>
                    {block.name}
                    {isComposeSelected && <span style={{ marginLeft: '4px', color: '#4a9eff' }}>[组合前级]</span>}
                    {isProductSelected && <span style={{ marginLeft: '4px', color: '#4a9eff' }}>[乘积第一]</span>}
                  </span>
                  <button
                    className="btn"
                    style={{ fontSize: '10px', padding: '1px 6px' }}
                    onClick={(e) => {
                      e.stopPropagation();
                      handleStartInstantiate(block);
                    }}
                  >
                    例化
                  </button>
                </div>
              );
            })}
          </div>
        )}

        {/* 例化输入选择 */}
        {instantiatingBlock && (
          <div style={{ marginTop: '8px' }}>
            <div style={{ fontSize: '11px', marginBottom: '4px' }}>
              例化: {instantiatingBlock.name} | 需 {instantiatingBlock.inputPointIds.length} 点 | 已选 {instInputIds.length} 点
            </div>
            <div style={{ display: 'flex', gap: '4px' }}>
              <button
                className="btn"
                style={{ fontSize: '11px' }}
                onClick={() => {
                  const currentIds = [...instInputIds];
                  for (const pt of selectedPoints) {
                    if (!currentIds.includes(pt.id)) {
                      currentIds.push(pt.id);
                    }
                  }
                  setInstInputIds(currentIds);
                }}
              >
                + 添加已选点
              </button>
              <button
                className="btn btn-accent"
                style={{ fontSize: '11px' }}
                onClick={handleExecuteInstantiate}
              >
                执行
              </button>
              <button
                className="btn"
                style={{ fontSize: '11px' }}
                onClick={() => {
                  setInstantiatingBlock(null);
                  setInstInputIds([]);
                }}
              >
                取消
              </button>
            </div>
          </div>
        )}
      </Panel>

      {/* ============================================================ */}
      {/* PARAMETERS / 参数输入 */}
      {/* ============================================================ */}
      <Panel title="PARAMETERS / 参数" panelId="block-params">
        <div className="input-row">
          <label>ID</label>
          <input
            type="text"
            className="input-field"
            placeholder="函数块标识符 / Block ID"
            value={blockId}
            onChange={(e) => setBlockId(e.target.value)}
          />
        </div>
        <div className="input-row">
          <label>IN</label>
          <input
            type="text"
            className="input-field"
            placeholder="输入类型 / Input types"
            value={blockIn}
            onChange={(e) => setBlockIn(e.target.value)}
          />
        </div>
        <div className="input-row">
          <label>OUT</label>
          <input
            type="text"
            className="input-field"
            placeholder="输出类型 / Output types"
            value={blockOut}
            onChange={(e) => setBlockOut(e.target.value)}
          />
        </div>
      </Panel>

      {/* ============================================================ */}
      {/* EXECUTION LOG / 执行日志 */}
      {/* ============================================================ */}
      <Panel title="EXEC LOG / 执行日志" panelId="block-log">
        <div
          style={{
            maxHeight: '100px',
            overflowY: 'auto',
            fontSize: '10px',
            fontFamily: 'monospace',
          }}
          role="log"
          aria-live="polite"
        >
          {executionLog.length === 0 ? (
            <div style={{ opacity: 0.5 }}>暂无日志 / No logs</div>
          ) : (
            executionLog.map((msg, i) => (
              <div key={i} style={{ padding: '1px 0', borderBottom: '1px solid #222' }}>
                {msg}
              </div>
            ))
          )}
        </div>
        {executionLog.length > 0 && (
          <button className="btn" style={{ fontSize: '10px', marginTop: '4px' }} onClick={clearLog}>
            CLEAR LOG / 清空日志
          </button>
        )}
      </Panel>

      {/* ============================================================ */}
      {/* INFO / 信息统计 */}
      {/* ============================================================ */}
      <Panel title="INFO / 信息" panelId="block-info">
        <div className="info-box">
          <div className="info-row">
            <span className="info-label">POINTS / 点</span>
            <span className="info-value">{points.length}</span>
          </div>
          <div className="info-row">
            <span className="info-label">SEGMENTS / 线段</span>
            <span className="info-value">{segments.length}</span>
          </div>
          <div className="info-row">
            <span className="info-label">REGIONS / 区域</span>
            <span className="info-value">{regions.length}</span>
          </div>
          <div className="info-row">
            <span className="info-label">CONSTRAINTS / 约束</span>
            <span className="info-value">{constraints.length}</span>
          </div>
          <div className="info-row">
            <span className="info-label">USER BLOCKS / 用户块</span>
            <span className="info-value">{userBlocks.length}</span>
          </div>
          <div className="info-row">
            <span className="info-label">STATUS / 状态</span>
            <span className="info-value">{activePreset ? `选择中: ${activePreset.nameZh}` : 'READY'}</span>
          </div>
        </div>
      </Panel>
    </>
  );
};

export default BlockPanel;
