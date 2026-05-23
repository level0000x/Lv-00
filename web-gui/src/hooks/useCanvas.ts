/**
 * @module hooks/useCanvas
 * @description Canvas 初始化和生命周期管理 Hook。
 *              处理高 DPI 设置、尺寸观察和资源清理。
 *
 *              此 Hook 提供通用化的 Canvas 2D 渲染引擎生命周期封装，
 *              可供 GeometryCanvas 等组件直接复用，
 *              避免在每个组件中重复编写 Renderer 初始化逻辑。
 *
 *              设计原则：
 *              1. 关注点分离：Hook 仅负责创建/销毁 Renderer 和 resize，
 *                 不涉及交互、几何数据订阅等业务逻辑。
 *              2. 回调注入：通过 onInit / onResize 回调让调用方自定义
 *                 初始化完成后的附加操作（如同步 canvas 尺寸到 store）。
 *              3. 防抖优化：resize 事件通过 requestAnimationFrame 合并，
 *                 避免高频触发时产生性能抖动。
 */

import { useEffect, useRef, useCallback } from 'react';
import { Renderer, type ViewportState } from '@/engine/renderer';
import { useAppStore } from '@/stores';

/**
 * useCanvas 的配置选项。
 */
export interface UseCanvasOptions {
  /**
   * Canvas 和 Renderer 初始化完成后的回调。
   * 常用于同步 canvas 宽高到 Zustand store、注册交互事件等。
   *
   * @param canvas  - 当前 canvas 元素
   * @param renderer - 已创建好的 Renderer 实例
   */
  onInit?: (canvas: HTMLCanvasElement, renderer: Renderer) => void;

  /**
   * 窗口 resize 完成后的回调。
   * 在 requestAnimationFrame 回调中执行，保证仅在高频 resize 的最后一帧触发。
   *
   * @param canvas  - 当前 canvas 元素
   * @param renderer - Renderer 实例
   */
  onResize?: (canvas: HTMLCanvasElement, renderer: Renderer) => void;
}

/**
 * useCanvas - 通用 Canvas 生命周期管理 Hook
 *
 * 职责：
 * - 从 canvasRef 获取 2D 上下文并创建 Renderer 实例
 * - 调用 renderer.setupCanvas() 处理高 DPI 适配
 * - 监听 window resize 事件，使用 rAF 防抖重新设置 Canvas
 * - 组件卸载时自动清理 Renderer 引用
 *
 * 使用示例：
 * ```tsx
 * const rendererRef = useCanvas(canvasRef, {
 *   onInit: (canvas) => store.setCanvasSize(canvas.offsetWidth, canvas.offsetHeight),
 *   onResize: (canvas) => store.setCanvasSize(canvas.offsetWidth, canvas.offsetHeight),
 * });
 * ```
 *
 * @param canvasRef - 指向 canvas HTML 元素的 React Ref
 * @param options   - 可选的初始化和 resize 回调
 * @returns 指向 Renderer 实例的可变引用，供外部访问渲染引擎
 */
export function useCanvas(
  canvasRef: React.RefObject<HTMLCanvasElement | null>,
  options?: UseCanvasOptions,
): React.MutableRefObject<Renderer | null> {
  const rendererRef = useRef<Renderer | null>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    // 获取 2D 渲染上下文
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    // 创建 Renderer 并执行高 DPI 初始化
    const renderer = new Renderer(canvas, ctx);
    rendererRef.current = renderer;
    renderer.setupCanvas();

    // 调用初始化完成回调（如：同步 canvas 尺寸到 store）
    options?.onInit?.(canvas, renderer);

    /**
     * 窗口尺寸变化处理：重新设置 Canvas 尺寸并更新 DPR。
     * 使用 requestAnimationFrame 防抖，避免高频 resize 事件
     * 导致连续多次重绘。
     */
    let resizeRafId: number | null = null;
    const handleResize = (): void => {
      // 如果已经有待处理的 resize，则跳过本次事件
      if (resizeRafId !== null) return;
      resizeRafId = requestAnimationFrame(() => {
        renderer.setupCanvas();
        useAppStore.getState().setDpr(window.devicePixelRatio || 1);
        // 调用 resize 完成回调
        options?.onResize?.(canvas, renderer);
        resizeRafId = null;
      });
    };

    window.addEventListener('resize', handleResize);

    // 清理函数：移除事件监听、取消待处理的 rAF、释放 Renderer 引用
    return () => {
      window.removeEventListener('resize', handleResize);
      if (resizeRafId !== null) cancelAnimationFrame(resizeRafId);
      rendererRef.current = null;
    };
  }, [canvasRef]); // eslint-disable-line react-hooks/exhaustive-deps

  return rendererRef;
}

/**
 * useViewportState - 获取当前视口状态的便捷 Hook
 *
 * 从 Zustand store 中读取 scale、offset、dpr 等视口参数，
 * 并优先使用 canvas 元素的实际渲染尺寸（而非 window 尺寸），
 * 确保鼠标坐标转换精度。
 *
 * 注意：此 Hook 独立于 useCanvas，可在任何需要视口信息的组件中使用，
 *       不需要依赖 canvas ref。
 *
 * @returns 包含所有视口参数的状态对象（响应式，store 变化时自动更新）
 */
export function useViewportState(): ViewportState {
  const scale = useAppStore((s) => s.scale);
  const offsetX = useAppStore((s) => s.offsetX);
  const offsetY = useAppStore((s) => s.offsetY);
  const dpr = useAppStore((s) => s.dpr);
  const canvasWidth = useAppStore((s) => s.canvasWidth);
  const canvasHeight = useAppStore((s) => s.canvasHeight);

  /* 优先使用 canvas 的实际渲染尺寸，回退到 window 尺寸 */
  return {
    scale,
    offsetX,
    offsetY,
    dpr,
    width: canvasWidth > 0 ? canvasWidth : window.innerWidth,
    height: canvasHeight > 0 ? canvasHeight : window.innerHeight,
  };
}
