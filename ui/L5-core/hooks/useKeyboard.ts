import { useEffect } from 'react';
import { useUIStore } from '../store/uiStore';
import { useCanvasStore } from '../store/canvasStore';

const bindings: Record<string, () => void> = {};

export function registerShortcut(key: string, fn: () => void) {
  bindings[key] = fn;
  return () => { delete bindings[key]; };
}

export function useKeyboard() {
  const toggleGrid = useCanvasStore((s) => s.toggleGrid);
  const toggleAxes = useCanvasStore((s) => s.toggleAxes);
  const toggleLabels = useCanvasStore((s) => s.toggleLabels);
  const resetView = useCanvasStore((s) => s.resetView);
  const theme = useUIStore((s) => s.theme);
  const setTheme = useUIStore((s) => s.setTheme);

  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      const mod = e.ctrlKey || e.metaKey;
      const key = `${mod ? 'Ctrl+' : ''}${e.key}`;

      if (bindings[key]) {
        e.preventDefault();
        bindings[key]();
        return;
      }

      if (mod && e.key === 'g') { e.preventDefault(); toggleGrid(); return; }
      if (mod && e.key === 'a') { e.preventDefault(); toggleAxes(); return; }
      if (mod && e.key === 'l') { e.preventDefault(); toggleLabels(); return; }
      if (mod && e.key === '0') { e.preventDefault(); resetView(); return; }
      if (mod && e.key === 't') { e.preventDefault(); setTheme(theme === 'dark' ? 'light' : 'dark'); return; }
    };

    window.addEventListener('keydown', handler);
    return () => window.removeEventListener('keydown', handler);
  }, [toggleGrid, toggleAxes, toggleLabels, resetView, theme, setTheme]);
}

export const SHORTCUTS = [
  { keys: ['Ctrl', 'G'], label: '网格 Grid' },
  { keys: ['Ctrl', 'A'], label: '坐标轴 Axes' },
  { keys: ['Ctrl', 'L'], label: '标签 Labels' },
  { keys: ['Ctrl', '0'], label: '重置视图 Reset View' },
  { keys: ['Ctrl', 'T'], label: '主题 Theme' },
  { keys: ['\u221E', 'Mouse Wheel'], label: '缩放 Zoom' },
  { keys: ['\u21B5', 'Drag'], label: '平移 Pan' },
] as const;
