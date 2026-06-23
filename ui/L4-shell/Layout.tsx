import React, { useCallback, useRef, useEffect, useState } from 'react';
import Header from './Header';
import SidebarLeft from './SidebarLeft';
import SidebarRight from './SidebarRight';
import StatusBar from './StatusBar';
import { useUIStore } from '../L5-core/store/uiStore';
import { useCanvasStore } from '../L5-core/store/canvasStore';
import { useKeyboard } from '../L5-core/hooks/useKeyboard';
import { useTheme } from '../L5-core/hooks/useTheme';
import { ModuleKey } from '../L5-core/types';

interface LayoutProps {
  canvasContent?: React.ReactNode;
  rightPanelContent?: React.ReactNode;
  nodes?: number;
  constraints?: number;
  version?: string;
  onUndo?: () => void;
  onRedo?: () => void;
  onNormalize?: () => void;
  canUndo?: boolean;
  canRedo?: boolean;
}

const Layout: React.FC<LayoutProps> = ({
  canvasContent,
  rightPanelContent,
  nodes = 0,
  constraints = 0,
  version = '3.0.0',
  onUndo,
  onRedo,
  onNormalize,
  canUndo = false,
  canRedo = false,
}) => {
  useKeyboard();
  useTheme();

  const activeModule = useUIStore((s) => s.activeModule);
  const setActiveModule = useUIStore((s) => s.setActiveModule);
  const leftSidebarWidth = useUIStore((s) => s.leftSidebarWidth);
  const rightSidebarWidth = useUIStore((s) => s.rightSidebarWidth);
  const setLeftSidebarWidth = useUIStore((s) => s.setLeftSidebarWidth);
  const setRightSidebarWidth = useUIStore((s) => s.setRightSidebarWidth);
  const resizeState = useUIStore((s) => s.resizeState);
  const setResizeState = useUIStore((s) => s.setResizeState);
  const connectionState = useUIStore((s) => s.connectionState);
  const statusMessage = useUIStore((s) => s.statusMessage);

  const [searchQuery, setSearchQuery] = useState('');
  const containerRef = useRef<HTMLDivElement>(null);

  const handleLeftResizeStart = useCallback(
    (e: React.MouseEvent) => {
      e.preventDefault();
      setResizeState({ sidebar: 'left', startX: e.clientX, startWidth: leftSidebarWidth });
    },
    [leftSidebarWidth, setResizeState]
  );

  const handleRightResizeStart = useCallback(
    (e: React.MouseEvent) => {
      e.preventDefault();
      setResizeState({ sidebar: 'right', startX: e.clientX, startWidth: rightSidebarWidth });
    },
    [rightSidebarWidth, setResizeState]
  );

  useEffect(() => {
    if (!resizeState) return;
    const onMove = (e: MouseEvent) => {
      const delta = e.clientX - resizeState.startX;
      const newWidth = Math.max(200, Math.min(400, resizeState.startWidth + delta));
      if (resizeState.sidebar === 'left') {
        setLeftSidebarWidth(newWidth);
      } else {
        setRightSidebarWidth(newWidth);
      }
    };
    const onUp = () => setResizeState(null);
    document.addEventListener('mousemove', onMove);
    document.addEventListener('mouseup', onUp);
    return () => {
      document.removeEventListener('mousemove', onMove);
      document.removeEventListener('mouseup', onUp);
    };
  }, [resizeState, setResizeState, setLeftSidebarWidth, setRightSidebarWidth]);

  return (
    <div ref={containerRef} className="app-shell">
      <Header
        activeModule={activeModule}
        onModuleChange={setActiveModule}
        version={version}
        onUndo={onUndo}
        onRedo={onRedo}
        onNormalize={onNormalize}
        canUndo={canUndo}
        canRedo={canRedo}
      />

      <div className="main-container">
        <SidebarLeft
          activeModule={activeModule}
          onModuleSelect={setActiveModule}
          searchQuery={searchQuery}
          onSearchChange={setSearchQuery}
          sidebarWidth={leftSidebarWidth}
        />

        <div className="resize-handle" onMouseDown={handleLeftResizeStart} />

        <div className="canvas-area">
          {canvasContent ?? (
            <div className="empty-state">
              <div className="empty-state-icon">{'\u2B21'}</div>
              <div>Select a module from the sidebar</div>
            </div>
          )}
        </div>

        <div className="resize-handle" onMouseDown={handleRightResizeStart} />

        <SidebarRight width={rightSidebarWidth}>
          {rightPanelContent}
        </SidebarRight>
      </div>

      <StatusBar
        nodes={nodes}
        constraints={constraints}
        connectionState={connectionState}
        statusMessage={statusMessage}
        version={version}
      />
    </div>
  );
};

export default Layout;
