/**
 * @module components/layout/StatusBar
 * @description 底部状态栏组件。
 *              显示当前工具名称、节点/线段统计信息、状态消息、
 *              全工具快捷键提示、当前工具详细操作提示、鼠标世界坐标和 FPS。
 *
 *              Bottom status bar component.
 *              Displays current tool name, node/segment statistics, status message,
 *              all-tool shortcut hints, current tool action hints, mouse world coordinates, and FPS.
 */

import React, { useMemo } from 'react';
import { useAppStore } from '@/stores';
import { TOOL_NAMES, TOOL_SHORTCUTS } from '@/utils/moduleConfig';
import type { ToolType } from '@/types';

// ================================================================
// 紧凑型全工具快捷键映射 / Compact All-Tool Shortcut Mapping
// ================================================================

/**
 * 每个工具对应的键盘快捷键按键字符。
 * 用于生成底部状态栏右侧的快捷键提示字符串。
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
 * 每个工具的紧凑中文名称。
 * 与 TOOL_COMPACT_KEYS 配合使用，生成 "V=选择 P=加点" 格式的提示。
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
 * 使用 useMemo 在模块加载时计算一次，避免每次渲染重复拼接。
 * 由于所有依赖都是常量，实际上只需计算一次。
 */
const ALL_TOOL_SHORTCUTS_COMPACT: string = (Object.keys(TOOL_COMPACT_KEYS) as ToolType[])
  .map((t) => `${TOOL_COMPACT_KEYS[t]}=${TOOL_COMPACT_NAMES_CN[t]}`)
  .join(' ');

/**
 * StatusBar - 底部状态栏
 *
 * 布局结构（三栏）：
 * - 左侧：当前工具名称 + 节点/线段统计
 * - 中间：状态消息（如操作提示、错误信息等）
 * - 右侧：全工具快捷键提示 + 当前工具详细操作提示 + 鼠标世界坐标 + FPS
 *
 * 性能优化：
 * - 使用 React.memo 包裹，仅在订阅的 store 切片变化时重渲染
 * - ALL_TOOL_SHORTCUTS_COMPACT 为模块级常量，不会重复计算
 * - Store 订阅使用精确切片选择器（tool、points.length、segments.length 等）
 *
 * 无障碍特性：
 * - 使用 <output> 元素标记动态变化的数值信息
 * - aria-live="polite" 让屏幕阅读器在空闲时播报变化
 */
const StatusBar: React.FC = () => {
  // ---- Store 订阅：仅订阅本组件需要的切片 ----
  const tool = useAppStore((s) => s.tool);
  const points = useAppStore((s) => s.points);
  const segments = useAppStore((s) => s.segments);
  const statusMessage = useAppStore((s) => s.statusMessage);
  const mouseWorldX = useAppStore((s) => s.mouseWorldX);
  const mouseWorldY = useAppStore((s) => s.mouseWorldY);
  const perfStats = useAppStore((s) => s.perfStats);

  /**
   * 当前工具的详细操作提示。
   * 使用 useMemo 避免在 tool 未变化时重复查找。
   */
  const currentToolShortcut = useMemo(
    () => TOOL_SHORTCUTS[tool] ?? TOOL_SHORTCUTS['select'],
    [tool],
  );

  return (
    <div className="status-bar" role="status">
      {/* 左侧：当前工具 + 统计信息 */}
      <div className="status-left">
        <output className="status-tool" id="statusTool" aria-live="polite">
          {TOOL_NAMES[tool] ?? tool}
        </output>
        <output className="status-stats" id="statusStats" aria-live="polite">
          {points.length} nodes, {segments.length} segments
        </output>
      </div>

      {/* 中间：状态消息 */}
      <output className="status-center" id="statusText" aria-live="polite">
        {statusMessage}
      </output>

      {/* 右侧：快捷键提示 + 坐标 + FPS */}
      <div className="status-right">
        {/* 全工具紧凑快捷键提示（始终显示，方便用户快速查阅） */}
        <span
          className="status-shortcuts"
          id="statusShortcuts"
          title={`键盘快捷键总览 / Keyboard Shortcuts Overview: ${ALL_TOOL_SHORTCUTS_COMPACT}`}
          aria-label={`键盘快捷键 / Keyboard Shortcuts: ${ALL_TOOL_SHORTCUTS_COMPACT}`}
        >
          {ALL_TOOL_SHORTCUTS_COMPACT}
        </span>
        {/* 当前工具详细操作提示 */}
        <span
          className="status-help"
          id="statusHelp"
          title="当前工具快捷键 / Current Tool Shortcuts"
        >
          {currentToolShortcut}
        </span>
        {/* 鼠标世界坐标 */}
        <output className="status-zoom" id="statusCoords" aria-live="polite">
          X: {mouseWorldX.toFixed(2)} Y: {mouseWorldY.toFixed(2)}
        </output>
        {/* 渲染帧率 */}
        <output id="statusFps" aria-live="polite">{perfStats.fps}fps</output>
      </div>
    </div>
  );
};

export default React.memo(StatusBar);
