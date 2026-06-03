/**
 * @module components/canvas/CanvasToolbar
 * @description 画布工具栏组件，提供工具选择按钮。
 *              Canvas toolbar with tool selection buttons.
 *              提供选择、加点、线段、平移、区域和探针工具的快速访问。
 *              Provides quick access to select, point, segment, pan, region, and probe tools.
 */

import React from 'react';
import { useAppStore } from '@/stores';
import type { ToolType } from '@/types';

/**
 * 工具按钮配置 / Tool button configuration
 */
const TOOLS: Array<{ id: ToolType; icon: string; label: string; title: string }> = [
  { id: 'select', icon: 'S', label: 'SEL', title: 'SELECT / 选择 (V)' },
  { id: 'point', icon: 'P', label: 'PT', title: 'ADD POINT / 添加点 (P)' },
  { id: 'segment', icon: 'L', label: 'SEG', title: 'ADD SEGMENT / 添加线段 (L)' },
  { id: 'compass', icon: 'O', label: 'CMP', title: 'COMPASS / 圆规 (C)' },
  { id: 'pan', icon: 'H', label: 'PAN', title: 'PAN VIEW / 平移视图 (H)' },
  { id: 'region', icon: 'R', label: 'REG', title: 'REGION / 区域 (R)' },
  { id: 'probe', icon: 'I', label: 'PRB', title: 'PROBE / 探测 (I)' },
];

/**
 * CanvasToolbar - 画布浮动工具栏 / Floating toolbar on the canvas
 *
 * 显示工具选择按钮，当前激活的工具高亮。
 * Displays tool selection buttons. The active tool is highlighted.
 */
const CanvasToolbar: React.FC = () => {
  const tool = useAppStore((s) => s.tool);
  const setTool = useAppStore((s) => s.setTool);

  return (
    <div className="canvas-toolbar" role="toolbar" aria-label="画布工具">
      {TOOLS.map((t) => (
        <button
          key={t.id}
          className={`tool-btn ${tool === t.id ? 'active' : ''}`}
          id={`tool${t.id.charAt(0).toUpperCase() + t.id.slice(1)}`}
          title={t.title}
          data-tooltip={t.title}
          aria-label={t.title}
          onClick={() => setTool(t.id)}
        >
          <span className="tool-icon">{t.icon}</span>
          <span className="tool-label">{t.label}</span>
        </button>
      ))}
    </div>
  );
};

export default React.memo(CanvasToolbar);
