/**
 * @module utils/moduleConfig
 * @description 集中化模块配置。
 *              从 Header.tsx 和 StatusBar.tsx 中提取模块标签页、
 *              工具名称和键盘快捷键的配置常量，统一维护，
 *              避免跨组件重复定义。
 */

import type { ModuleType, ToolType } from '@/types';

// ================================================================
// 模块标签页配置 / Module Tab Configuration
// ================================================================

/**
 * 模块标签页配置接口
 * @property id - 模块唯一标识，对应 ModuleType 联合类型
 * @property icon - 标签页按钮中显示的单字符图标
 * @property label - 标签页按钮的短文本标签
 * @property tooltip - 鼠标悬停提示（双语）
 * @property color - 模块强调色 CSS 变量名（如 'var(--color-module-formula)'）
 */
export interface ModuleTabConfig {
  id: ModuleType;
  icon: string;
  label: string;
  tooltip: string;
  color: string;
}

/**
 * 所有模块的标签页配置常量数组。
 * 共 10 个模块，对应 Header 组件中的模块导航标签页。
 */
export const MODULE_TABS: ModuleTabConfig[] = [
  { id: 'formula', icon: 'F', label: 'Formula', tooltip: 'FORMULA / 公式模块', color: 'var(--color-module-formula)' },
  { id: 'graph', icon: 'G', label: 'Graph', tooltip: 'GRAPH / 图模块', color: 'var(--color-module-graph)' },
  { id: 'block', icon: 'B', label: 'Block', tooltip: 'BLOCK / 函数块模块', color: 'var(--color-module-block)' },
  { id: 'proof', icon: 'P', label: 'Proof', tooltip: 'PROOF / 证明模块', color: 'var(--color-module-proof)' },
  { id: 'type', icon: 'T', label: 'Type', tooltip: 'TYPE / 类型模块', color: 'var(--color-module-type)' },
  { id: 'recurse', icon: 'R', label: 'Recurse', tooltip: 'RECURSE / 递归模块', color: 'var(--color-module-recurse)' },
  { id: 'engine', icon: 'E', label: 'Engine', tooltip: 'ENGINE / 引擎模块', color: 'var(--color-module-engine)' },
  { id: 'debug', icon: 'D', label: 'Debug', tooltip: 'DEBUG / 调试模块', color: 'var(--color-module-debug)' },
  { id: 'help', icon: '?', label: 'Help', tooltip: 'HELP / 帮助', color: 'var(--color-module-help)' },
  { id: 'assistant', icon: '*', label: 'AI', tooltip: 'ASSISTANT / AI 助手', color: 'var(--color-module-assistant)' },
];

/**
 * 根据模块 ID 查找对应的配置对象。
 * 如果 ID 不在预定义列表中，返回 undefined。
 *
 * @param id - 模块标识符
 * @returns 模块配置对象，或 undefined（未找到时）
 *
 * @example
 * const cfg = getModuleConfig('formula');
 * console.log(cfg?.label); // "Formula"
 */
export function getModuleConfig(id: ModuleType): ModuleTabConfig | undefined {
  return MODULE_TABS.find((tab) => tab.id === id);
}

// ================================================================
// 工具显示名称与快捷键配置 / Tool Display Names & Shortcuts
// ================================================================

/**
 * 工具的显示名称映射（双语）。
 * 用于 StatusBar 左侧工具显示区域。
 */
export const TOOL_NAMES: Record<ToolType, string> = {
  select: 'SELECT / 选择',
  point: 'ADD POINT / 添加点',
  segment: 'ADD SEGMENT / 添加线段',
  compass: 'COMPASS / 圆规',
  pan: 'PAN VIEW / 平移',
  region: 'REGION / 区域',
  probe: 'PROBE / 探测',
};

/**
 * 各工具的键盘快捷键提示映射。
 * 用于 StatusBar 右侧快捷键显示区域。
 */
export const TOOL_SHORTCUTS: Record<ToolType, string> = {
  select: 'V 选择 | P 点 | L 线段 | C 圆规 | H 平移 | +/- 缩放 | Ctrl+Z 撤销 | Ctrl+Y 重做 | Ctrl+0 重置视图',
  point: '点击画布添加点 / Click to add point | Esc 取消 | Ctrl+Z 撤销',
  segment: '点击两点连线 / Click two points | Esc 取消 | Ctrl+Z 撤销',
  compass: '点击设圆心，再点击设半径 / Click center then radius | Esc 取消 | Ctrl+Z 撤销',
  pan: '拖拽平移 / Drag to pan | 滚轮缩放 | Ctrl+0 重置视图',
  region: '点击顶点定义区域 / Click vertices | Esc 取消 | Ctrl+Z 撤销',
  probe: '悬停查看坐标 / Hover to inspect | Esc 取消',
};

/**
 * 根据工具类型获取工具的中英文显示名称。
 * 如果工具类型不在预定义列表中，返回原始字符串作为兜底。
 *
 * @param tool - 工具类型标识符
 * @returns 工具显示名称（双语）
 *
 * @example
 * getToolDisplayName('point'); // "ADD POINT / 添加点"
 * getToolDisplayName('unknown'); // "unknown"
 */
export function getToolDisplayName(tool: ToolType | string): string {
  return TOOL_NAMES[tool as ToolType] ?? tool;
}
