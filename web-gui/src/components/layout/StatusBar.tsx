/**
 * @module components/layout/StatusBar
 * @description 底部状态栏组件 / Bottom status bar component.
 *              显示当前工具、统计信息、状态消息、
 *              键盘快捷键、坐标和 FPS。
 *
 *              Displays current tool, statistics, status message,
 *              keyboard shortcuts, coordinates, and FPS.
 */

import React from 'react';
import { useAppStore } from '@/stores';
import { TOOL_NAMES, TOOL_SHORTCUTS } from '@/utils/moduleConfig';
import type { ToolType } from '@/types';

// ================================================================
// 紧凑型全工具快捷键映射 / Compact All-Tool Shortcut Mapping
// ================================================================

/**
 * 每个工具对应的键盘快捷键按键字符 / Keyboard shortcut key character for each tool
 */
const TOOL_COMPACT_KEYS: Record<ToolType, string> = {
  select: 'V',
  point: 'P',
  segment: 'L',
  compass: 'C',
  pan: 'H',
  region: 'R',
  probe: '?',
};

/**
 * 每个工具的紧凑中文名称（用于快捷键提示） / Compact Chinese name for each tool (used in shortcut hints)
 */
const TOOL_COMPACT_NAMES_CN: Record<ToolType, string> = {
  select: '选择',
  point: '加点',
  segment: '线段',
  compass: '圆规',
  pan: '平移',
  region: '区域',
  probe: '探针',
};

/**
 * 所有工具的紧凑型快捷键提示字符串。
 * 格式如: "V=选择 P=加点 L=线段 H=平移 R=区域 ?=探针"
 *
 * Compact shortcut hint string for all tools.
 * Format: "V=选择 P=加点 L=线段 H=平移 R=区域 ?=探针"
 */
const ALL_TOOL_SHORTCUTS_COMPACT: string = (Object.keys(TOOL_COMPACT_KEYS) as ToolType[])
  .map((t) => `${TOOL_COMPACT_KEYS[t]}=${TOOL_COMPACT_NAMES_CN[t]}`)
  .join(' ');

/**
 * StatusBar - 底部状态栏 / Bottom status bar
 *
 * 显示内容 / Display content:
 * - 左侧 / Left: 当前工具名称 / Current tool name、节点/约束数量 / node/constraint count
 * - 中间 / Center: 状态消息 / Status message
 * - 右侧 / Right: 全工具快捷键提示 / All-tool shortcut hints、鼠标坐标 / Mouse coordinates、FPS
 */
const StatusBar: React.FC = () => {
  const tool = useAppStore((s) => s.tool);
  const points = useAppStore((s) => s.points);
  const segments = useAppStore((s) => s.segments);
  const statusMessage = useAppStore((s) => s.statusMessage);
  const mouseWorldX = useAppStore((s) => s.mouseWorldX);
  const mouseWorldY = useAppStore((s) => s.mouseWorldY);
  const perfStats = useAppStore((s) => s.perfStats);

  return (
    <div className="status-bar">
      <div className="status-left">
        <output className="status-tool" id="statusTool" aria-live="polite">
          {TOOL_NAMES[tool] ?? tool}
        </output>
        <output className="status-stats" id="statusStats" aria-live="polite">
          {points.length} nodes, {segments.length} constraints
        </output>
      </div>
      <output className="status-center" id="statusText" aria-live="polite">
        {statusMessage}
      </output>
      <div className="status-right">
        {/* 全工具紧凑快捷键提示（始终显示，方便用户快速查阅） / Compact all-tool shortcut hints (always visible for quick reference) */}
        <span
          className="status-shortcuts"
          id="statusShortcuts"
          title={`键盘快捷键总览 / Keyboard Shortcuts Overview: ${ALL_TOOL_SHORTCUTS_COMPACT}`}
          aria-label={`键盘快捷键 / Keyboard Shortcuts: ${ALL_TOOL_SHORTCUTS_COMPACT}`}
        >
          {ALL_TOOL_SHORTCUTS_COMPACT}
        </span>
        {/* 当前工具详细操作提示 / Current tool detailed action hints */}
        <span className="status-help" id="statusHelp" title="当前工具快捷键 / Current Tool Shortcuts">
          {TOOL_SHORTCUTS[tool] ?? TOOL_SHORTCUTS['select']}
        </span>
        <output className="status-zoom" id="statusCoords" aria-live="polite">
          X: {mouseWorldX.toFixed(2)} Y: {mouseWorldY.toFixed(2)}
        </output>
        <output id="statusFps" aria-live="polite">{perfStats.fps}fps</output>
      </div>
    </div>
  );
};

export default React.memo(StatusBar);
