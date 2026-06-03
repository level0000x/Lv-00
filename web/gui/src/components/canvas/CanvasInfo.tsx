/**
 * @module components/canvas/CanvasInfo
 * @description 画布信息栏组件，包含网格/坐标轴/标签切换按钮和缩放显示。
 *              Canvas information bar with grid/axes/labels toggles and zoom display.
 */

import React from 'react';
import { useAppStore } from '@/stores';

/**
 * CanvasInfo - 画布浮动信息栏 / Floating info bar at the top center of the canvas
 *
 * 提供网格、坐标轴和标签显示的切换按钮，以及缩放百分比指示器。
 * Provides toggle buttons for grid, axes, and labels display,
 * plus a zoom percentage indicator.
 */
const CanvasInfo: React.FC = () => {
  const showGrid = useAppStore((s) => s.showGrid);
  const showAxes = useAppStore((s) => s.showAxes);
  const showLabels = useAppStore((s) => s.showLabels);
  const scale = useAppStore((s) => s.scale);
  const toggleGrid = useAppStore((s) => s.toggleGrid);
  const toggleAxes = useAppStore((s) => s.toggleAxes);
  const toggleLabels = useAppStore((s) => s.toggleLabels);

  return (
    <div className="canvas-info">
      <button
        className={`canvas-info-btn ${showGrid ? 'active' : ''}`}
        id="btnShowGrid"
        onClick={toggleGrid}
        aria-pressed={showGrid}
      >
        GRID / 网格: {showGrid ? 'ON' : 'OFF'}
      </button>
      <button
        className={`canvas-info-btn ${showAxes ? 'active' : ''}`}
        id="btnShowAxes"
        onClick={toggleAxes}
        aria-pressed={showAxes}
      >
        AXES / 坐标轴: {showAxes ? 'ON' : 'OFF'}
      </button>
      <button
        className={`canvas-info-btn ${showLabels ? 'active' : ''}`}
        id="btnShowLabels"
        onClick={toggleLabels}
        aria-pressed={showLabels}
      >
        LABELS / 标签: {showLabels ? 'ON' : 'OFF'}
      </button>
      <span className="canvas-zoom-display" id="zoomDisplay">
        {Math.round(scale * 100)}%
      </span>
    </div>
  );
};

export default React.memo(CanvasInfo);
