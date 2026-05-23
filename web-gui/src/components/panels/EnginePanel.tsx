/**
 * @module components/panels/EnginePanel
 * @description 引擎模块侧边栏面板。
 *
 *              实现功能：
 *              1. SOLVE —— 收集所有约束，执行约束求解（纯 JS 实现）
 *              2. REWRITE + SOLVE —— 先归一化（合并相近点），再求解约束
 *              3. CIRCUIT TRIP —— 模拟电路跳闸，提供 IGNORE/ROLLBACK/DOWNGRADE 选项
 *              4. 引擎状态显示 —— 上次求解结果、约束满足率、自由变量数
 *
 *              所有约束求解使用纯 JS 几何计算，不依赖 WASM 后端。
 */

import React, { useState, useCallback, useRef, useEffect } from 'react';
import Panel from './Panel';
import { useAppStore } from '@/stores';
import type { Point, Segment, Constraint } from '@/types';
import {
  MAX_PANEL_LOG_ENTRIES,
  COLLINEARITY_TOLERANCE,
  BETWEENNESS_TOLERANCE,
  INTERSECTION_ERROR_THRESHOLD,
  NORMALIZE_DISTANCE_THRESHOLD,
} from '@/utils/constants';
import {
  dist,
  solveIncidence,
  solveBetweenness,
  solveIntersection,
  performSolve,
  normalizePoints,
} from '@/utils/constraintSolver';
import type { SolveResult } from '@/utils/constraintSolver';

// ================================================================
// 类型定义 / Type Definitions
// ================================================================

/** 引擎状态 */
interface EngineStatus {
  lastResult: SolveResult | null;
  lastAction: string;
  timestamp: number;
}

// ================================================================
// EnginePanel 组件 / EnginePanel Component
// ================================================================

/**
 * EnginePanel - 引擎模块侧边栏面板
 *
 * 面板分区:
 * - ENGINE: 求解、重写+求解、电路跳闸
 * - STATUS: 引擎状态显示（上次求解结果、约束满足率、自由变量数）
 * - INFO  : 来自 Store 的实时引擎状态和性能统计
 */
const EnginePanel: React.FC = () => {
  const addToast = useAppStore((s) => s.addToast);
  const appendLog = useAppStore((s) => s.appendLog);
  const saveUndoState = useAppStore((s) => s.saveUndoState);
  const undo = useAppStore((s) => s.undo);
  const setPoints = useAppStore((s) => s.setPoints);
  const setSegments = useAppStore((s) => s.setSegments);
  const setConstraints = useAppStore((s) => s.setConstraints);
  const showModal = useAppStore((s) => s.showModal);
  const hideModal = useAppStore((s) => s.hideModal);

  // ================================================================
  // 从 Store 读取真实数据
  // ================================================================
  const backend = useAppStore((s) => s.backend);
  const fps = useAppStore((s) => s.perfStats.fps);
  const renderCount = useAppStore((s) => s.perfStats.renderCount);
  const points = useAppStore((s) => s.points);
  const segments = useAppStore((s) => s.segments);
  const constraints = useAppStore((s) => s.constraints);
  const elementCount = points.length + segments.length + constraints.length;

  // ================================================================
  // 本地状态
  // ================================================================
  const [engineStatus, setEngineStatus] = useState<EngineStatus>({
    lastResult: null,
    lastAction: 'NONE',
    timestamp: 0,
  });
  const [solveLog, setSolveLog] = useState<string[]>([]);

  /** 存储 setTimeout ID 以便在组件卸载时清理 */
  const toastTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  /** 组件卸载时清理定时器 */
  useEffect(() => {
    return () => {
      if (toastTimerRef.current) {
        clearTimeout(toastTimerRef.current);
      }
    };
  }, []);

  /** 格式化后端名称 */
  const backendLabel =
    backend === 'wasm' ? 'WebAssembly'
    : backend === 'js' ? 'JavaScript'
    : 'NONE (JS Fallback)';

  // ================================================================
  // 辅助函数
  // ================================================================

  const log = useCallback((msg: string) => {
    setSolveLog((prev) => [...prev.slice(-MAX_PANEL_LOG_ENTRIES), msg]);
  }, []);

  // ================================================================
  // SOLVE 操作
  // ================================================================
  const handleSolve = useCallback(() => {
    if (constraints.length === 0) {
      addToast('info', '当前无约束，无需求解 / No constraints to solve');
      return;
    }

    const now = Date.now();
    appendLog(`引擎求解: ${constraints.length} 个约束`, 'info');
    log('--- SOLVE 开始 ---');

    // 执行求解
    const result = performSolve(points, segments, constraints);

    // 记录日志
    result.details.forEach((d) => log(d));

    // 如果有需要调整的点，更新 Store
    const hasAdjustments = result.details.some((d) => d.includes('调整'));
    if (hasAdjustments) {
      saveUndoState();
      // 重新执行求解以获取调整后的点
      const adjustedPoints = [...points];
      for (const con of constraints) {
        if (con.type === 'betweenness') {
          const r = solveBetweenness(con, adjustedPoints);
          if (r.adjustedPoint) {
            const idx = adjustedPoints.findIndex((p) => p.id === r.adjustedPoint!.id);
            if (idx >= 0) adjustedPoints[idx] = r.adjustedPoint;
          }
        } else if (con.type === 'intersection') {
          const r = solveIntersection(con, adjustedPoints, segments);
          if (r.adjustedPoint) {
            const existingIdx = adjustedPoints.findIndex((p) => p.id === r.adjustedPoint!.id);
            if (existingIdx >= 0) {
              adjustedPoints[existingIdx] = r.adjustedPoint;
            } else {
              adjustedPoints.push(r.adjustedPoint);
            }
          }
        }
      }
      setPoints(adjustedPoints);
      log('已更新画布上的点坐标');
    }

    // 更新引擎状态
    setEngineStatus({
      lastResult: result,
      lastAction: 'SOLVE',
      timestamp: now,
    });

    const summary = `求解完成: ${result.satisfied} 约束满足, ${result.conflicts} 冲突 (${result.satisfactionRate.toFixed(1)}%)`;
    addToast(
      result.conflicts > 0 ? 'warning' : 'success',
      summary,
    );
    appendLog(summary, result.conflicts > 0 ? 'warn' : 'info');
  }, [
    constraints,
    points,
    segments,
    saveUndoState,
    setPoints,
    addToast,
    appendLog,
    log,
  ]);

  // ================================================================
  // REWRITE + SOLVE 操作
  // ================================================================
  const handleRewriteSolve = useCallback(() => {
    if (points.length === 0) {
      addToast('info', '画布上没有几何元素 / No geometry on canvas');
      return;
    }

    const now = Date.now();
    appendLog('重写+求解: 开始', 'info');
    log('--- REWRITE + SOLVE 开始 ---');

    // 步骤 1: 归一化（合并相近点）
    log('步骤 1: 归一化 (合并相近点)');
    const normResult = normalizePoints(points, segments, constraints, NORMALIZE_DISTANCE_THRESHOLD);
    normResult.details.forEach((d) => log(d));

    if (normResult.mergeCount > 0) {
      saveUndoState();
      setPoints(normResult.mergedPoints);
      setSegments(normResult.mergedSegments);
      setConstraints(normResult.mergedConstraints);
      log(`已更新: ${normResult.mergedPoints.length} 点, ${normResult.mergedSegments.length} 线段, ${normResult.mergedConstraints.length} 约束`);
    } else {
      log('归一化: 无需合并');
    }

    // 步骤 2: 求解约束
    log('步骤 2: 求解约束');
    const solveResult = performSolve(
      normResult.mergedPoints,
      normResult.mergedSegments,
      normResult.mergedConstraints,
    );
    solveResult.details.forEach((d) => log(d));

    // 更新引擎状态
    setEngineStatus({
      lastResult: solveResult,
      lastAction: 'REWRITE+SOLVE',
      timestamp: now,
    });

    const summary = `重写+求解: 合并 ${normResult.mergeCount} 点, ${solveResult.satisfied} 约束满足, ${solveResult.conflicts} 冲突`;
    addToast(
      solveResult.conflicts > 0 ? 'warning' : 'success',
      summary,
    );
    appendLog(summary, solveResult.conflicts > 0 ? 'warn' : 'info');
  }, [
    points,
    segments,
    constraints,
    saveUndoState,
    setPoints,
    setSegments,
    setConstraints,
    addToast,
    appendLog,
    log,
  ]);

  // ================================================================
  // CIRCUIT TRIP 操作
  // ================================================================
  const handleCircuitTrip = useCallback(() => {
    appendLog('电路跳闸: 检测到异常', 'warn');
    log('--- CIRCUIT TRIP ---');
    log('检测到引擎异常! 请选择处理方式:');

    // 显示警告对话框，提供 3 个选项
    showModal({
      id: 'circuit-trip-dialog',
      title: 'CIRCUIT TRIP / 电路跳闸',
      content: (
        <div>
          <p style={{ marginBottom: '12px' }}>
            引擎检测到异常状态，请选择处理方式：
          </p>
          <p style={{ marginBottom: '8px', fontSize: '12px', opacity: 0.8 }}>
            当前状态: {points.length} 点, {segments.length} 线段, {constraints.length} 约束
          </p>
        </div>
      ),
      confirmLabel: 'IGNORE / 忽略',
      cancelLabel: 'ROLLBACK / 回滚',
      danger: false,
      onConfirm: () => {
        // IGNORE —— 继续执行
        hideModal();
        log('CIRCUIT TRIP: 选择 IGNORE —— 继续执行');
        appendLog('电路跳闸: IGNORE —— 继续执行', 'warn');
        addToast('warning', '电路跳闸已忽略，继续执行 / Circuit trip ignored');
        setEngineStatus((prev) => ({
          ...prev,
          lastAction: 'CIRCUIT TRIP (IGNORE)',
          timestamp: Date.now(),
        }));
      },
      onCancel: () => {
        // ROLLBACK —— 撤销上一步操作
        hideModal();
        log('CIRCUIT TRIP: 选择 ROLLBACK —— 撤销上一步');
        appendLog('电路跳闸: ROLLBACK —— 撤销上一步', 'info');
        undo();
        addToast('info', '已回滚到上一步 / Rolled back to previous state');
        setEngineStatus((prev) => ({
          ...prev,
          lastAction: 'CIRCUIT TRIP (ROLLBACK)',
          timestamp: Date.now(),
        }));
      },
    });

    // 额外的 DOWNGRADE 选项通过 toast 提示
    if (toastTimerRef.current) clearTimeout(toastTimerRef.current);
    toastTimerRef.current = setTimeout(() => {
      addToast('warning', '提示: 关闭对话框后，可点击 DOWNGRADE 按钮将异常标记为警告', 5000);
    }, 500);
  }, [points, segments, constraints, showModal, hideModal, undo, addToast, appendLog, log]);

  /** DOWNGRADE 操作 —— 将异常标记为警告 */
  const handleDowngrade = useCallback(() => {
    log('CIRCUIT TRIP: 选择 DOWNGRADE —— 标记为警告');
    appendLog('电路跳闸: DOWNGRADE —— 标记为警告', 'info');
    addToast('info', '异常已降级为警告 / Anomaly downgraded to warning');
    setEngineStatus((prev) => ({
      ...prev,
      lastAction: 'CIRCUIT TRIP (DOWNGRADE)',
      timestamp: Date.now(),
    }));
  }, [addToast, appendLog, log]);

  // ================================================================
  // 渲染
  // ================================================================
  return (
    <>
      <Panel title="ENGINE / 引擎" panelId="engine-ops">
        {/* 求解 —— 收集约束并执行约束求解 */}
        <button className="btn btn-accent" onClick={handleSolve}>
          SOLVE / 求解
        </button>
        {/* 重写+求解 —— 先归一化再求解 */}
        <button className="btn" onClick={handleRewriteSolve}>
          REWRITE + SOLVE / 重写+求解
        </button>
        {/* 电路跳闸 —— 模拟引擎异常 */}
        <button className="btn" onClick={handleCircuitTrip}>
          CIRCUIT TRIP / 电路跳闸
        </button>
        {/* 降级 —— 将异常标记为警告 */}
        <button className="btn" onClick={handleDowngrade}>
          DOWNGRADE / 降级
        </button>
      </Panel>

      {/* ============================================================ */}
      {/* ENGINE STATUS / 引擎状态 */}
      {/* ============================================================ */}
      <Panel title="STATUS / 引擎状态" panelId="engine-status">
        <div className="info-box">
          {/* 上次操作 */}
          <div className="info-row">
            <span className="info-label">LAST ACTION / 上次操作</span>
            <span className="info-value" style={{ fontSize: '10px' }}>
              {engineStatus.lastAction}
            </span>
          </div>
          {/* 上次求解结果 */}
          {engineStatus.lastResult && (
            <>
              <div className="info-row">
                <span className="info-label">SATISFIED / 已满足</span>
                <span className="info-value" style={{ color: engineStatus.lastResult.conflicts === 0 ? '#4caf50' : '#ff9800' }}>
                  {engineStatus.lastResult.satisfied}/{engineStatus.lastResult.total}
                </span>
              </div>
              <div className="info-row">
                <span className="info-label">CONFLICTS / 冲突</span>
                <span className="info-value" style={{ color: engineStatus.lastResult.conflicts > 0 ? '#f44336' : '#4caf50' }}>
                  {engineStatus.lastResult.conflicts}
                </span>
              </div>
              <div className="info-row">
                <span className="info-label">SATISFACTION / 满足率</span>
                <span className="info-value">
                  {engineStatus.lastResult.satisfactionRate.toFixed(1)}%
                </span>
              </div>
              <div className="info-row">
                <span className="info-label">FREE VARS / 自由变量</span>
                <span className="info-value">
                  {engineStatus.lastResult.freeVariables}
                </span>
              </div>
            </>
          )}
          {/* 无求解结果时的占位 */}
          {!engineStatus.lastResult && (
            <div className="info-row">
              <span className="info-label">SATISFACTION / 满足率</span>
              <span className="info-value">--</span>
            </div>
          )}
          {/* 约束满足率进度条 */}
          {engineStatus.lastResult && (
            <div style={{ marginTop: '6px' }}>
              <div
                style={{
                  height: '6px',
                  backgroundColor: '#333',
                  borderRadius: '3px',
                  overflow: 'hidden',
                }}
              >
                <div
                  style={{
                    height: '100%',
                    width: `${engineStatus.lastResult.satisfactionRate}%`,
                    backgroundColor:
                      engineStatus.lastResult.satisfactionRate >= 90
                        ? '#4caf50'
                        : engineStatus.lastResult.satisfactionRate >= 50
                          ? '#ff9800'
                          : '#f44336',
                    borderRadius: '3px',
                    transition: 'width 0.3s ease',
                  }}
                />
              </div>
            </div>
          )}
        </div>
      </Panel>

      {/* ============================================================ */}
      {/* SOLVE LOG / 求解日志 */}
      {/* ============================================================ */}
      <Panel title="SOLVE LOG / 求解日志" panelId="engine-log">
        <div
          style={{
            maxHeight: '160px',
            overflowY: 'auto',
            fontSize: '10px',
            fontFamily: 'monospace',
          }}
          role="log"
          aria-live="polite"
        >
          {solveLog.length === 0 ? (
            <div style={{ opacity: 0.5 }}>暂无日志 / No logs</div>
          ) : (
            solveLog.map((msg, i) => (
              <div
                key={i}
                style={{
                  padding: '1px 0',
                  borderBottom: '1px solid #222',
                  color: msg.includes('ERROR') || msg.includes('冲突')
                    ? '#f44336'
                    : msg.includes('满足') || msg.includes('完成')
                      ? '#4caf50'
                      : msg.includes('调整') || msg.includes('警告')
                        ? '#ff9800'
                        : 'inherit',
                }}
              >
                {msg}
              </div>
            ))
          )}
        </div>
        {solveLog.length > 0 && (
          <button
            className="btn"
            style={{ fontSize: '10px', marginTop: '4px' }}
            onClick={() => setSolveLog([])}
          >
            CLEAR LOG / 清空日志
          </button>
        )}
      </Panel>

      {/* ============================================================ */}
      {/* INFO / 系统信息 */}
      {/* ============================================================ */}
      <Panel title="INFO / 信息" panelId="engine-info">
        <div className="info-box">
          {/* 后端类型 */}
          <div className="info-row">
            <span className="info-label">BACKEND / 后端</span>
            <span className="info-value">{backendLabel}</span>
          </div>
          {/* 性能统计 */}
          <div className="info-row">
            <span className="info-label">FPS / 帧率</span>
            <span className="info-value">{fps}</span>
          </div>
          <div className="info-row">
            <span className="info-label">FRAMES / 帧数</span>
            <span className="info-value">{renderCount}</span>
          </div>
          {/* 几何元素计数 */}
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
          <div className="info-row">
            <span className="info-label">ELEMENTS / 元素</span>
            <span className="info-value">{elementCount}</span>
          </div>
          <div className="info-row">
            <span className="info-label">STATUS / 状态</span>
            <span className="info-value">
              {engineStatus.lastResult
                ? engineStatus.lastResult.conflicts === 0
                  ? 'SOLVED'
                  : 'PARTIAL'
                : 'IDLE'}
            </span>
          </div>
        </div>
      </Panel>
    </>
  );
};

export default EnginePanel;
