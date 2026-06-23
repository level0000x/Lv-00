import { create } from 'zustand';
import { Viewport } from '../types';

interface CanvasState {
  viewport: Viewport;
  dpr: number;
  showGrid: boolean;
  showAxes: boolean;
  showLabels: boolean;

  setScale: (scale: number) => void;
  setOffset: (offsetX: number, offsetY: number) => void;
  setDpr: (dpr: number) => void;
  setCanvasSize: (canvasWidth: number, canvasHeight: number) => void;
  resetView: () => void;
  toggleGrid: () => void;
  toggleAxes: () => void;
  toggleLabels: () => void;
}

const defaultViewport: Viewport = {
  offsetX: 0, offsetY: 0, scale: 1, canvasWidth: 800, canvasHeight: 600,
};

export const useCanvasStore = create<CanvasState>((set) => ({
  viewport: { ...defaultViewport },
  dpr: typeof window !== 'undefined' ? window.devicePixelRatio || 1 : 1,
  showGrid: true,
  showAxes: true,
  showLabels: true,

  setScale: (scale) => set((s) => ({ viewport: { ...s.viewport, scale: Math.max(0.1, Math.min(20, scale)) } })),
  setOffset: (offsetX, offsetY) => set((s) => ({ viewport: { ...s.viewport, offsetX, offsetY } })),
  setDpr: (dpr) => set({ dpr }),
  setCanvasSize: (canvasWidth, canvasHeight) => set((s) => ({ viewport: { ...s.viewport, canvasWidth, canvasHeight } })),
  resetView: () => set({ viewport: { ...defaultViewport } }),
  toggleGrid: () => set((s) => ({ showGrid: !s.showGrid })),
  toggleAxes: () => set((s) => ({ showAxes: !s.showAxes })),
  toggleLabels: () => set((s) => ({ showLabels: !s.showLabels })),
}));
