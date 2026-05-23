/**
 * @module components/streaming/StreamEventItem
 * @description Individual streaming event display component.
 *              Renders different styles based on event type with
 *              collapsible details, timestamp, and step number.
 *              Supports all 39 StreamEventType values from C kernel stream.h.
 *              单个流式事件显示组件。根据事件类型渲染不同样式，
 *              支持可折叠详情、时间戳和步骤编号。
 *              支持 C 内核 stream.h 中全部 39 种 StreamEventType。
 */

import React, { useState, useCallback } from 'react';
import type { StreamingEvent, EngineStreamEvent } from '@/types';
import { EngineEventType } from '@/types';

// ================================================================
// 事件类型配置（39 种类型，与 C 内核 stream.h 对齐） / Event type configuration (39 types, aligned with C kernel stream.h)
// ================================================================

interface EventStyle {
  icon: string;
  color: string;
  label: string;
  labelZh: string;
}

/**
 * 全部 39 种 StreamEventType 的事件样式配置。
 * 颜色与 C 内核 stream.c 中的颜色定义对齐。
 * Event styles for all 39 StreamEventType values.
 * Colors are aligned with C kernel stream.c color definitions.
 */
const EVENT_STYLES: Record<number, EventStyle> = {
  /* ---- 引擎生命周期 (0-2) / Engine lifecycle ---- */
  [EngineEventType.ENGINE_START]:  { icon: '\u25B6', color: '#3fb950', label: 'Engine Start',        labelZh: '引擎启动' },
  [EngineEventType.ENGINE_DONE]:   { icon: '\u2714', color: '#3fb950', label: 'Engine Done',         labelZh: '引擎完成' },
  [EngineEventType.ENGINE_PAUSED]: { icon: '\u23F8', color: '#8b949e', label: 'Engine Paused',       labelZh: '引擎暂停' },
  /* ---- 归一化 (3-5) / Normalization ---- */
  [EngineEventType.NORMALIZE_START]: { icon: '\u25C7', color: '#a371f7', label: 'Normalize Start',     labelZh: '归一化开始' },
  [EngineEventType.NORMALIZE_MERGE]: { icon: '\u2726', color: '#a371f7', label: 'Normalize Merge',     labelZh: '归一化合并' },
  [EngineEventType.NORMALIZE_DONE]:  { icon: '\u2714', color: '#a371f7', label: 'Normalize Done',      labelZh: '归一化完成' },
  /* ---- 重写引擎 (6-11) / Rewrite engine ---- */
  [EngineEventType.REWRITE_START]:       { icon: '\u25B6', color: '#a371f7', label: 'Rewrite Start',       labelZh: '重写开始' },
  [EngineEventType.REWRITE_RULE_LOADED]: { icon: '\uD83D\uDCDD', color: '#a371f7', label: 'Rule Loaded',        labelZh: '规则加载' },
  [EngineEventType.REWRITE_MATCH_FOUND]: { icon: '\uD83D\uDD0D', color: '#a371f7', label: 'Match Found',        labelZh: '匹配发现' },
  [EngineEventType.REWRITE_APPLIED]:     { icon: '\u270F', color: '#a371f7', label: 'Rewrite Applied',     labelZh: '重写应用' },
  [EngineEventType.REWRITE_ROLLBACK]:    { icon: '\u21A9', color: '#a371f7', label: 'Rewrite Rollback',    labelZh: '重写回滚' },
  [EngineEventType.REWRITE_DONE]:        { icon: '\u2714', color: '#a371f7', label: 'Rewrite Done',        labelZh: '重写完成' },
  /* ---- 代数求解 (12-16) / Algebraic solving ---- */
  [EngineEventType.SOLVE_START]:              { icon: '\u25B6', color: '#a371f7', label: 'Solve Start',         labelZh: '求解开始' },
  [EngineEventType.SOLVE_EQUATION_EXTRACTED]: { icon: '\u2211', color: '#a371f7', label: 'Equation Extracted',  labelZh: '方程提取' },
  [EngineEventType.SOLVE_GROEBNER_STEP]:      { icon: '\u27F3', color: '#a371f7', label: 'Groebner Step',       labelZh: 'Groebner 步骤' },
  [EngineEventType.SOLVE_VARIABLE_RESOLVED]:  { icon: '\u2714', color: '#a371f7', label: 'Variable Resolved',   labelZh: '变量求解' },
  [EngineEventType.SOLVE_DONE]:               { icon: '\u2714', color: '#a371f7', label: 'Solve Done',          labelZh: '求解完成' },
  /* ---- 证明系统 (17-21) / Proof system ---- */
  [EngineEventType.PROOF_STEP_ADDED]:          { icon: '\u002B', color: '#a371f7', label: 'Proof Step Added',    labelZh: '证明步骤添加' },
  [EngineEventType.PROOF_STEP_APPLIED]:        { icon: '\u2713', color: '#a371f7', label: 'Proof Step Applied',  labelZh: '证明步骤应用' },
  [EngineEventType.PROOF_UNIFY]:               { icon: '\u2261', color: '#a371f7', label: 'Proof Unify',         labelZh: '证明合一' },
  [EngineEventType.PROOF_COLOR_UPDATE]:        { icon: '\u25CF', color: '#a371f7', label: 'Proof Color Update',  labelZh: '证明颜色更新' },
  [EngineEventType.PROOF_DEPENDENCY_CHANGE]:   { icon: '\u21C4', color: '#a371f7', label: 'Proof Dep Change',    labelZh: '证明依赖变更' },
  /* ---- 函数块系统 (22-29) / Function block system ---- */
  [EngineEventType.FUNC_BLOCK_PACK_START]:         { icon: '\u25B6', color: '#39d353', label: 'FB Pack Start',       labelZh: '函数块打包开始' },
  [EngineEventType.FUNC_BLOCK_PACK_DONE]:          { icon: '\u2714', color: '#39d353', label: 'FB Pack Done',        labelZh: '函数块打包完成' },
  [EngineEventType.FUNC_BLOCK_INSTANTIATE_START]:  { icon: '\u25B6', color: '#39d353', label: 'FB Instantiate Start', labelZh: '函数块实例化开始' },
  [EngineEventType.FUNC_BLOCK_INSTANTIATE_DONE]:   { icon: '\u2714', color: '#39d353', label: 'FB Instantiate Done',  labelZh: '函数块实例化完成' },
  [EngineEventType.FUNC_BLOCK_PARTIAL_APPLY]:      { icon: '\u223F', color: '#39d353', label: 'FB Partial Apply',    labelZh: '函数块部分应用' },
  [EngineEventType.FUNC_BLOCK_DETERMINISM_CHECK]:  { icon: '\u26A0', color: '#39d353', label: 'FB Determinism',      labelZh: '函数块确定性检查' },
  [EngineEventType.FUNC_BLOCK_CAPTURE_AVOID]:      { icon: '\u21BA', color: '#39d353', label: 'FB Capture Avoid',    labelZh: '函数块捕获避免' },
  [EngineEventType.FUNC_BLOCK_CROSS_BOUNDARY]:     { icon: '\u21C6', color: '#39d353', label: 'FB Cross Boundary',   labelZh: '函数块跨边界' },
  /* ---- 冲突与错误 (30-35) / Conflicts & errors ---- */
  [EngineEventType.CONFLICT_DETECTED]: { icon: '\u26A0', color: '#f0883e', label: 'Conflict Detected',   labelZh: '冲突检测' },
  [EngineEventType.CONSTRAINT_ADDED]:  { icon: '\u002B', color: '#f0883e', label: 'Constraint Added',    labelZh: '约束添加' },
  [EngineEventType.NODE_ADDED]:        { icon: '\u25CF', color: '#f0883e', label: 'Node Added',          labelZh: '节点添加' },
  [EngineEventType.CIRCUIT_TRIP]:      { icon: '\u26A1', color: '#f0883e', label: 'Circuit Trip',        labelZh: '断路器触发' },
  [EngineEventType.ERROR]:             { icon: '\u274C', color: '#f85149', label: 'Error',               labelZh: '错误' },
  [EngineEventType.WARNING]:           { icon: '\u26A0', color: '#d29922', label: 'Warning',             labelZh: '警告' },
  /* ---- 信息 (36-38) / Information ---- */
  [EngineEventType.INFO]:           { icon: '\u2139', color: '#8b949e', label: 'Info',                labelZh: '信息' },
  [EngineEventType.PROGRESS]:       { icon: '\uD83D\uDCCA', color: '#58a6ff', label: 'Progress',     labelZh: '进度' },
  [EngineEventType.GRAPH_SNAPSHOT]: { icon: '\uD83D\uDDBC', color: '#c9d1d9', label: 'Graph Snapshot', labelZh: '图快照' },
};

function getEventStyle(type: number): EventStyle {
  return EVENT_STYLES[type] ?? EVENT_STYLES[EngineEventType.INFO]!;
}

// ================================================================
// Safe JSON serialization / 安全 JSON 序列化
// ================================================================

function safeStringify(data: unknown, maxLen = 10000): string {
  try {
    const str = JSON.stringify(data, null, 2);
    return str && str.length > maxLen ? str.slice(0, maxLen) + '\n... (truncated)' : (str ?? 'null');
  } catch {
    return '[无法序列化 / Unable to serialize]';
  }
}

// ================================================================
// 组件属性 / Component Props
// ================================================================

interface StreamEventItemProps {
  event: StreamingEvent | EngineStreamEvent;
  index: number;
}

/** 类型守卫：检查事件是否为 EngineStreamEvent / Type guard to check if an event is an EngineStreamEvent */
function isEngineStreamEvent(event: StreamingEvent | EngineStreamEvent): event is EngineStreamEvent {
  return 'category' in event && 'timestamp_ms' in event;
}

// ================================================================
// 组件 / Component
// ================================================================

/**
 * StreamEventItem - 显示单个流式事件 / Displays a single streaming event
 *
 * 功能 / Features:
 * - 基于事件类型的彩色图标（0-38，与 C 内核对齐） / Color-coded icon based on event type (0-38, aligned with C kernel)
 * - 步骤编号徽标 / Step number badge
 * - 可折叠详情区域，包含所有 C 引擎字段 / Collapsible detail section with all C engine fields
 * - 时间戳显示 / Timestamp display
 * - 同时支持旧版 StreamingEvent 和新版 EngineStreamEvent / Supports both legacy StreamingEvent and new EngineStreamEvent
 */
const StreamEventItem: React.FC<StreamEventItemProps> = ({ event, index }) => {
  const [expanded, setExpanded] = useState(false);
  const isEngine = isEngineStreamEvent(event);

  const style = getEventStyle(typeof event.type === 'number' ? event.type : 0);

  const handleToggle = useCallback(() => {
    setExpanded((prev) => !prev);
  }, []);

  const ts = isEngine
    ? event.timestamp_ms
    : event.timestamp;
  const timeStr = (ts ? new Date(ts) : new Date()).toLocaleTimeString('en-US', {
    hour12: false,
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  });

  return (
    <div
      className="stream-event-item"
      style={{ borderLeftColor: style.color }}
      onClick={handleToggle}
      role="button"
      tabIndex={0}
      onKeyDown={(e) => {
        if (e.key === 'Enter' || e.key === ' ') {
          e.preventDefault();
          handleToggle();
        }
      }}
    >
      <div className="stream-event-header">
        <span className="stream-event-icon" style={{ color: style.color }}>
          {style.icon}
        </span>
        <span className="stream-event-step">
          #{isEngine ? event.step : event.stepNumber}
        </span>
        <span className="stream-event-type" style={{ color: style.color }}>
          [{style.label}]
        </span>
        <span className="stream-event-desc">
          {isEngine ? event.description : event.description}
        </span>
        <span className="stream-event-time">{timeStr}</span>
        <span className={`stream-event-expand ${expanded ? 'expanded' : ''}`}>
          {expanded ? '\u25B2' : '\u25BC'}
        </span>
      </div>
      {expanded && (
        <div className="stream-event-details">
          <div className="stream-event-detail-row">
            <span className="stream-event-detail-label">Type / 类型:</span>
            <span>{event.type} - {style.label} ({style.labelZh})</span>
          </div>
          <div className="stream-event-detail-row">
            <span className="stream-event-detail-label">Step / 步骤:</span>
            <span>{isEngine ? event.step : event.stepNumber}</span>
          </div>
          {/* EngineStreamEvent 专属字段 / EngineStreamEvent specific fields */}
          {isEngine && (
            <>
              {event.total_steps >= 0 && (
                <div className="stream-event-detail-row">
                  <span className="stream-event-detail-label">Total Steps / 总步骤:</span>
                  <span>{event.total_steps}</span>
                </div>
              )}
              {event.node_id >= 0 && (
                <div className="stream-event-detail-row">
                  <span className="stream-event-detail-label">Node ID / 节点:</span>
                  <span>{event.node_id}</span>
                </div>
              )}
              {event.constraint_id >= 0 && (
                <div className="stream-event-detail-row">
                  <span className="stream-event-detail-label">Constraint ID / 约束:</span>
                  <span>{event.constraint_id}</span>
                </div>
              )}
              {event.rule_id >= 0 && (
                <div className="stream-event-detail-row">
                  <span className="stream-event-detail-label">Rule ID / 规则:</span>
                  <span>{event.rule_id}</span>
                </div>
              )}
              {event.var_id >= 0 && (
                <div className="stream-event-detail-row">
                  <span className="stream-event-detail-label">Var ID / 变量:</span>
                  <span>{event.var_id}</span>
                </div>
              )}
              {event.progress >= 0 && (
                <div className="stream-event-detail-row">
                  <span className="stream-event-detail-label">Progress / 进度:</span>
                  <span>{(event.progress * 100).toFixed(1)}%</span>
                </div>
              )}
              {event.numeric_value !== 0 && (
                <div className="stream-event-detail-row">
                  <span className="stream-event-detail-label">Numeric Value / 数值:</span>
                  <span>{event.numeric_value}</span>
                </div>
              )}
              {event.detail && (
                <div className="stream-event-detail-row">
                  <span className="stream-event-detail-label">Detail / 详情:</span>
                  <span className="stream-event-data">
                    {(() => {
                      try {
                        return safeStringify(JSON.parse(event.detail));
                      } catch {
                        return event.detail;
                      }
                    })()}
                  </span>
                </div>
              )}
              {event.graph_snapshot && (
                <div className="stream-event-detail-row">
                  <span className="stream-event-detail-label">Graph Snapshot / 图快照:</span>
                  <span className="stream-event-data">
                    {(() => {
                      try {
                        return safeStringify(JSON.parse(event.graph_snapshot));
                      } catch {
                        return event.graph_snapshot;
                      }
                    })()}
                  </span>
                </div>
              )}
              <div className="stream-event-detail-row">
                <span className="stream-event-detail-label">Category / 类别:</span>
                <span>{event.category}</span>
              </div>
            </>
          )}
          {/* 旧版 StreamingEvent 字段 / Legacy StreamingEvent fields */}
          {!isEngine && (
            <>
              {event.nodeId !== undefined && (
                <div className="stream-event-detail-row">
                  <span className="stream-event-detail-label">Node ID:</span>
                  <span>{event.nodeId}</span>
                </div>
              )}
              {event.data !== undefined && (
                <div className="stream-event-detail-row">
                  <span className="stream-event-detail-label">Data / 数据:</span>
                  <span className="stream-event-data">
                    {typeof event.data === 'string'
                      ? event.data
                      : safeStringify(event.data)}
                  </span>
                </div>
              )}
            </>
          )}
          <div className="stream-event-detail-row">
            <span className="stream-event-detail-label">Index / 索引:</span>
            <span>{index}</span>
          </div>
        </div>
      )}
    </div>
  );
};

export default React.memo(StreamEventItem);
