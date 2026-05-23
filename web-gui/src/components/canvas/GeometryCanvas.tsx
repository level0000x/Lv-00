/**
 * @module components/canvas/GeometryCanvas
 * @description 主画布组件，封装 Renderer 和 InteractionManager。
 *              Main canvas component that wraps the Renderer and InteractionManager.
 *              处理画布初始化、渲染循环和生命周期管理。
 *              Handles canvas initialization, rendering loop, and lifecycle management.
 */

import React, { useRef, useEffect, useCallback } from 'react';
import { useAppStore } from '@/stores';
import { Renderer, type ViewportState, type RendererConfig } from '@/engine/renderer';
import { InteractionManager, type InteractionCallbacks } from '@/engine/interaction';
import { HIT_THRESHOLD } from '@/utils/constants';
import type { Point, Segment } from '@/types';
import CanvasToolbar from './CanvasToolbar';
import CanvasOverlay from './CanvasOverlay';
import CanvasInfo from './CanvasInfo';

/**
 * 计算点到线段的最短距离（屏幕坐标）
 * 用于右键菜单的线段拾取
 * Calculate the shortest distance from a point to a segment (screen coordinates)
 * Used for segment picking in the context menu
 */
function pointToSegmentDist(
  px: number, py: number,
  x1: number, y1: number,
  x2: number, y2: number,
): number {
  const dx = x2 - x1;
  const dy = y2 - y1;
  const lenSq = dx * dx + dy * dy;
  if (lenSq === 0) return Math.sqrt((px - x1) ** 2 + (py - y1) ** 2);
  let t = ((px - x1) * dx + (py - y1) * dy) / lenSq;
  t = Math.max(0, Math.min(1, t));
  const projX = x1 + t * dx;
  const projY = y1 + t * dy;
  return Math.sqrt((px - projX) ** 2 + (py - projY) ** 2);
}

/**
 * GeometryCanvas - 主画布组件 / Main canvas component
 *
 * 创建并管理 HTML5 Canvas 元素、渲染引擎和交互管理器。
 * 在框架无关的引擎类与 React 状态管理之间提供桥梁。
 * Creates and manages the HTML5 Canvas element, rendering engine,
 * and interaction manager. Provides the bridge between the
 * framework-agnostic engine classes and React state management.
 */
const GeometryCanvas: React.FC = () => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const rendererRef = useRef<Renderer | null>(null);
  const rafIdRef = useRef<number>(0);
  const doRenderRef = useRef<() => void>(() => {});

  // Store selectors
  const scale = useAppStore((s) => s.scale);
  const offsetX = useAppStore((s) => s.offsetX);
  const offsetY = useAppStore((s) => s.offsetY);
  const points = useAppStore((s) => s.points);
  const segments = useAppStore((s) => s.segments);
  const regions = useAppStore((s) => s.regions);
  const selectedPoint = useAppStore((s) => s.selectedPoint);
  const hoveredPoint = useAppStore((s) => s.hoveredPoint);
  const theme = useAppStore((s) => s.theme);
  const showGrid = useAppStore((s) => s.showGrid);
  const showAxes = useAppStore((s) => s.showAxes);
  const showLabels = useAppStore((s) => s.showLabels);
  const mouseWorldX = useAppStore((s) => s.mouseWorldX);
  const mouseWorldY = useAppStore((s) => s.mouseWorldY);

  const setDpr = useAppStore((s) => s.setDpr);
  const setCanvasSize = useAppStore((s) => s.setCanvasSize);
  const setScale = useAppStore((s) => s.setScale);
  const setOffset = useAppStore((s) => s.setOffset);
  const setMouseWorld = useAppStore((s) => s.setMouseWorld);
  const setMouseScreen = useAppStore((s) => s.setMouseScreen);
  const setSelectedPoint = useAppStore((s) => s.setSelectedPoint);
  const setHoveredPoint = useAppStore((s) => s.setHoveredPoint);
  const setSelectedPoints = useAppStore((s) => s.setSelectedPoints);
  const setIsDragging = useAppStore((s) => s.setIsDragging);
  const setIsDraggingPoint = useAppStore((s) => s.setIsDraggingPoint);
  const setIsBoxSelecting = useAppStore((s) => s.setIsBoxSelecting);
  const setBoxSelectStart = useAppStore((s) => s.setBoxSelectStart);
  const setDragStart = useAppStore((s) => s.setDragStart);
  const setDragPoint = useAppStore((s) => s.setDragPoint);
  const addPoint = useAppStore((s) => s.addPoint);
  const addSegment = useAppStore((s) => s.addSegment);
  const saveUndoState = useAppStore((s) => s.saveUndoState);
  const setStatusMessage = useAppStore((s) => s.setStatusMessage);
  const resetView = useAppStore((s) => s.resetView);
  const hideContextMenu = useAppStore((s) => s.hideContextMenu);
  const setSearchVisible = useAppStore((s) => s.setSearchVisible);
  const addRegionPoint = useAppStore((s) => s.addRegionPoint);
  const clearRegionPoints = useAppStore((s) => s.clearRegionPoints);

  /**
   * Perform a single render frame
   */
  const doRender = useCallback(() => {
    const canvas = canvasRef.current;
    const renderer = rendererRef.current;
    if (!canvas || !renderer) return;

    const state = useAppStore.getState();
    const vp: ViewportState = {
      scale: state.scale,
      offsetX: state.offsetX,
      offsetY: state.offsetY,
      dpr: state.dpr,
      width: canvas.offsetWidth,
      height: canvas.offsetHeight,
    };

    const config: RendererConfig = {
      showGrid: state.showGrid,
      showAxes: state.showAxes,
      showLabels: state.showLabels,
    };

    renderer.render(
      vp,
      config,
      state.theme,
      state.points,
      state.segments,
      state.regions,
      [], // ports（暂未在 store 中暴露）
      [], // funcBlocks（暂未在 store 中暴露）
      state.constraints,
      state.selectedPoint,
      state.hoveredPoint,
      state.mouseWorldX,
      state.mouseWorldY,
    );

    // Update FPS
    const now = performance.now();
    const perfStats = { ...state.perfStats };
    perfStats.renderCount++;
    if (now - perfStats.lastFpsUpdate >= 1000) {
      perfStats.fps = perfStats.renderCount;
      perfStats.renderCount = 0;
      perfStats.lastFpsUpdate = now;
    }
    useAppStore.getState().updatePerfStats(perfStats);
  }, []);

  // Keep ref in sync
  doRenderRef.current = doRender;

  /**
   * Initialize canvas, renderer, and interaction manager
   */
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    // Create renderer
    const renderer = new Renderer(canvas, ctx);
    rendererRef.current = renderer;

    // Setup canvas for high-DPI
    renderer.setupCanvas();
    setDpr(window.devicePixelRatio || 1);
    /* 同步 canvas 尺寸到 store，供 useViewportState 等 hook 使用 */
    setCanvasSize(canvas.offsetWidth, canvas.offsetHeight);

    // Build interaction callbacks
    const getViewport = (): ViewportState => {
      const state = useAppStore.getState();
      return {
        scale: state.scale,
        offsetX: state.offsetX,
        offsetY: state.offsetY,
        dpr: state.dpr,
        width: canvas.offsetWidth,
        height: canvas.offsetHeight,
      };
    };

    const callbacks: InteractionCallbacks = {
      getViewport,
      getTool: () => useAppStore.getState().tool,
      getPoints: () => useAppStore.getState().points,
      setScale,
      setOffset,
      setMouseWorld,
      setMouseScreen,
      setSelectedPoint,
      setHoveredPoint,
      setSelectedPoints,
      setIsDragging,
      setIsDraggingPoint,
      setIsBoxSelecting,
      setBoxSelectStart,
      setDragStart,
      setDragPoint,
      addPoint,
      addSegment,
      setSegmentFirstPoint: () => { /* handled internally */ },
      addRegionPoint,
      clearRegionPoints,
      saveUndoState,
      findPointAt: (sx: number, sy: number) => {
        const vp = getViewport();
        const pts = useAppStore.getState().points;
        return renderer.findPointAt(sx, sy, pts, vp);
      },
      findSegmentAt: (sx: number, sy: number) => {
        const vp = getViewport();
        const pts = useAppStore.getState().points;
        const segs = useAppStore.getState().segments;
        const threshold = HIT_THRESHOLD; // 像素阈值 / Pixel threshold
        let nearest: Segment | null = null;
        let minDist = threshold;
        for (const seg of segs) {
          const p1 = pts.find((p) => p.id === seg.p1);
          const p2 = pts.find((p) => p.id === seg.p2);
          if (!p1 || !p2) continue;
          const s1 = renderer.worldToScreen(p1.x, p1.y, vp);
          const s2 = renderer.worldToScreen(p2.x, p2.y, vp);
          const dist = pointToSegmentDist(sx, sy, s1.x, s1.y, s2.x, s2.y);
          if (dist < minDist) {
            minDist = dist;
            nearest = seg;
          }
        }
        return nearest;
      },
      screenToWorld: (sx: number, sy: number) => {
        const vp = getViewport();
        return renderer.screenToWorld(sx, sy, vp);
      },
      requestRender: () => {
        if (rafIdRef.current) cancelAnimationFrame(rafIdRef.current);
        rafIdRef.current = requestAnimationFrame(() => doRenderRef.current());
      },
      setStatusMessage,
      showContextMenu: (
        x: number,
        y: number,
        targetPoint?: Point,
        targetSegment?: Segment,
        worldX?: number,
        worldY?: number,
      ) => {
        const state = useAppStore.getState();

        if (targetPoint) {
          // 右键点击点：显示点相关操作
          state.showContextMenu({
            x,
            y,
            items: [
              { id: 'delete-point', label: 'DELETE POINT / 删除点' },
              { id: 'point-properties', label: 'PROPERTIES / 属性' },
              { id: 'set-midpoint', label: 'SET AS MIDPOINT / 设为中点' },
              { id: 'merge-nearest', label: 'MERGE WITH NEAREST / 合并到最近点' },
            ],
            target: targetPoint.id,
            targetType: 'point',
          });
        } else if (targetSegment) {
          // 右键点击线段：显示线段相关操作
          state.showContextMenu({
            x,
            y,
            items: [
              { id: 'delete-segment', label: 'DELETE SEGMENT / 删除线段' },
              { id: 'segment-properties', label: 'PROPERTIES / 属性' },
              { id: 'add-midpoint', label: 'ADD MIDPOINT / 添加中点' },
              { id: 'find-perpendicular', label: 'FIND PERPENDICULAR / 作垂线' },
            ],
            target: targetSegment.id,
            targetType: 'segment',
          });
        } else {
          // 右键点击空白：显示通用操作
          state.showContextMenu({
            x,
            y,
            items: [
              { id: 'add-point-here', label: 'ADD POINT HERE / 在此添加点' },
              { id: 'paste', label: 'PASTE / 粘贴', shortcut: 'Ctrl+V' },
              { id: 'select-all', label: 'SELECT ALL / 全选', shortcut: 'Ctrl+A' },
            ],
            targetType: 'empty',
            worldX,
            worldY,
          });
        }
      },
      hideContextMenu,
      toggleSearch: () => {
        const current = useAppStore.getState().searchVisible;
        setSearchVisible(!current);
      },
      resetView,
      setTool: useAppStore.getState().setTool,
    };

    // Create interaction manager
    const interaction = new InteractionManager(canvas, callbacks);
    interaction.attach();

    // Handle window resize
    const handleResize = (): void => {
      renderer.setupCanvas();
      setDpr(window.devicePixelRatio || 1);
      /* 同步 canvas 尺寸到 store */
      setCanvasSize(canvas.offsetWidth, canvas.offsetHeight);
    };

    window.addEventListener('resize', handleResize);

    // Initial render
    doRender();

    return () => {
      window.removeEventListener('resize', handleResize);
      interaction.detach();
      if (rafIdRef.current) cancelAnimationFrame(rafIdRef.current);
      rendererRef.current = null;
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  /**
   * Re-render when relevant state changes
   */
  useEffect(() => {
    rafIdRef.current = requestAnimationFrame(doRender);
    return () => {
      if (rafIdRef.current) cancelAnimationFrame(rafIdRef.current);
    };
  }, [
    doRender,
    scale, offsetX, offsetY,
    points, segments, regions,
    selectedPoint, hoveredPoint,
    theme, showGrid, showAxes, showLabels,
    mouseWorldX, mouseWorldY,
  ]);

  return (
    <>
      <canvas
        ref={canvasRef}
        id="geometryCanvas"
        aria-label="几何图形绘制画布"
        role="img"
      />
      <CanvasToolbar />
      <CanvasInfo />
      <CanvasOverlay />
    </>
  );
};

export default GeometryCanvas;
