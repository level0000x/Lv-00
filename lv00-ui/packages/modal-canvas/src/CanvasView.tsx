// ============================================================
// @lv00/modal-canvas — M1 几何画布 (CanvasView)
// Canvas 2D 渲染 + 鼠标/键盘交互，与 SceneController 绑定
// ============================================================

import React, { useRef, useEffect, useCallback, useState } from 'react';
import { SceneController } from '@lv00/scene-controller';
import { DrawCommand, LineStyle } from '@lv00/protocol';

interface CanvasViewProps {
  controller: SceneController;
  width?: number;
  height?: number;
  showGrid?: boolean;
}

export const CanvasView: React.FC<CanvasViewProps> = ({
  controller,
  width = 800,
  height = 600,
  showGrid = true,
}) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [isReady, setIsReady] = useState(false);
  const dpr = typeof window !== 'undefined' ? window.devicePixelRatio || 1 : 1;

  // --- 渲染主循环 ---
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    let animationId: number;
    let isRunning = true;
    let lastGridScale = 0;
    let lastOffsetX = 0;
    let lastOffsetY = 0;

    const render = () => {
      if (!isRunning) return;

      const vp = controller.getViewport();
      const w = canvas.width / dpr;
      const h = canvas.height / dpr;

      // 视口参数变化时全量重绘背景
      const needsBgRedraw =
        vp.scale !== lastGridScale ||
        vp.offsetX !== lastOffsetX ||
        vp.offsetY !== lastOffsetY;

      if (needsBgRedraw) {
        // 绘制背景
        ctx.fillStyle = '#0a0a0a';
        ctx.fillRect(0, 0, w, h);

        if (showGrid) {
          drawGrid(ctx, vp, w, h);
        }

        lastGridScale = vp.scale;
        lastOffsetX = vp.offsetX;
        lastOffsetY = vp.offsetY;
      }

      // 获取增量绘制指令并渲染
      const commands = controller.getDrawCommands();
      if (commands.length > 0 || needsBgRedraw) {
        // 非全量模式：先清屏再绘制
        ctx.fillStyle = '#0a0a0a';
        ctx.fillRect(0, 0, w, h);
        if (showGrid) drawGrid(ctx, vp, w, h);
        renderCommands(ctx, commands);
      }

      animationId = requestAnimationFrame(render);
    };

    render();
    setIsReady(true);

    return () => {
      isRunning = false;
      cancelAnimationFrame(animationId);
    };
  }, [controller, dpr, showGrid]);

  // --- 调整视口 canvas 尺寸 ---
  useEffect(() => {
    controller.setViewport({ canvasWidth: width, canvasHeight: height });
  }, [controller, width, height]);

  // --- 坐标转换工具 ---
  const clientToCanvas = useCallback((clientX: number, clientY: number) => {
    const rect = canvasRef.current?.getBoundingClientRect();
    if (!rect) return { x: 0, y: 0 };
    return {
      x: (clientX - rect.left) * dpr,
      y: (clientY - rect.top) * dpr,
    };
  }, [dpr]);

  // --- 事件处理器 ---
  const handleMouseDown = useCallback((e: React.MouseEvent) => {
    const { x, y } = clientToCanvas(e.clientX, e.clientY);
    controller.onKeyDown('Shift', e.shiftKey);
    controller.onMouseDown(x, y, e.button);
  }, [controller, clientToCanvas]);

  const handleMouseMove = useCa