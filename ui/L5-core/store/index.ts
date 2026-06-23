import { create } from 'zustand';
import { useUIStore } from './uiStore';
import { useCanvasStore } from './canvasStore';
import { useTerminalStore } from './terminalStore';

export type AppState = ReturnType<typeof useUIStore.getState> &
  ReturnType<typeof useCanvasStore.getState> &
  ReturnType<typeof useTerminalStore.getState>;

export const useAppStore = create<AppState>(() => {
  const init = {
    ...useUIStore.getState(),
    ...useCanvasStore.getState(),
    ...useTerminalStore.getState(),
  };

  useUIStore.subscribe((state) => useAppStore.setState(state));
  useCanvasStore.subscribe((state) => useAppStore.setState(state));
  useTerminalStore.subscribe((state) => useAppStore.setState(state));

  return init;
});
