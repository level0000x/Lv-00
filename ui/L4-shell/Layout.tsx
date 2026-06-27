import React, { useCallback, useRef, useEffect, useState } from 'react';
import Header from './Header';
import SidebarLeft from './SidebarLeft';
import SidebarRight from './SidebarRight';
import StatusBar from './StatusBar';
import CanvasToolbar from '../L2-components/CanvasToolbar';
import { useUIStore } from '../L5-core/store/uiStore';
import { useKeyboard } from '../L5-core/hooks/useKeyboard';
import { useTheme } from '../L5-core/hooks/useTheme';
import { ViewKey, PanelKey, FloatingPanel } from '../L5-core/types';

interface LayoutProps {
  viewContent?: React.ReactNode;
  panelContent?: React.ReactNode;
  renderPanelContent: (key: PanelKey) => React.ReactNode;
  nodes?: number;
  constraints?: number;
  version?: string;
}

/* ---------- Floating Window ---------- */
const FloatingWindow: React.FC<{
  panel: FloatingPanel;
  onClose: () => void;
  onMove: (id: string, x: number, y: number) => void;
  onResize: (id: string, w: number, h: number) => void;
  onFocus: (id: string) => void;
  children: React.ReactNode;
  title: string;
}> = ({ panel, onClose, onMove, onResize, onFocus, children, title }) => {
  const dragRef = useRef<{ startX: number; startY: number; origX: number; origY: number } | null>(null);
  const resizeRef = useRef<{ startX: number; startY: number; origW: number; origH: number } | null>(null);

  useEffect(() => {
    const onUp = () => { dragRef.current = null; resizeRef.current = null; };
    const onMove = (e: MouseEvent) => {
      if (dragRef.current) {
        const dx = e.clientX - dragRef.current.startX;
        const dy = e.clientY - dragRef.current.startY;
        onMove(panel.id, dragRef.current.origX + dx, dragRef.current.origY + dy);
      }
      if (resizeRef.current) {
        const dx = e.clientX - resizeRef.current.startX;
        const dy = e.clientY - resizeRef.current.startY;
        onResize(panel.id, Math.max(280, resizeRef.current.origW + dx), Math.max(200, resizeRef.current.origH + dy));
      }
    };
    document.addEventListener('mousemove', onMove);
    document.addEventListener('mouseup', onUp);
    return () => { document.removeEventListener('mousemove', onMove); document.removeEventListener('mouseup', onUp); };
  }, [panel.id, onMove, onResize]);

  return (
    <div
      className="floating-window"
      style={{
        left: panel.x,
        top: panel.y,
        width: panel.width,
        height: panel.height,
        zIndex: panel.zIndex,
      }}
      onMouseDown={() => onFocus(panel.id)}
    >
      <div
        className="floating-window-titlebar"
        onMouseDown={(e) => {
          e.preventDefault();
          dragRef.current = { startX: e.clientX, startY: e.clientY, origX: panel.x, origY: panel.y };
        }}
      >
        <span>{title}</span>
        <div className="floating-window-actions">
          <button className="btn-icon" style={{ width: 20, height: 20, fontSize: 12 }} onClick={onClose} title="关闭 Close">{'\u2715'}</button>
        </div>
      </div>
      <div className="floating-window-body">
        {children}
      </div>
      <div
        className="floating-window-resize-handle"
        onMouseDown={(e) => {
          e.preventDefault();
          e.stopPropagation();
          resizeRef.current = { startX: e.clientX, startY: e.clientY, origW: panel.width, origH: panel.height };
        }}
      />
    </div>
  );
};

const PANEL_TITLES: Record<string, string> = {
  formula: '公式输入 Formula Input',
  graph: '约束图 Constraint Graph',
  block: '函数块 Function Blocks',
  proof: '证明导航 Proof Navigator',
  type: '类型浏览 Type Explorer',
  recurse: '递归 Recursion Explorer',
  engine: '引擎状态 Engine Status',
  debug: '调试控制台 Debug Console',
  help: '帮助与快捷键 Help & Keys',
};

/* ---------- Layout ---------- */
const Layout: React.FC<LayoutProps> = ({
  viewContent,
  panelContent,
  renderPanelContent,
  nodes = 0,
  constraints = 0,
  version = '3.4.0',
}) => {
  useKeyboard();
  useTheme();

  const activeView = useUIStore((s) => s.activeView);
  const setActiveView = useUIStore((s) => s.setActiveView);
  const activePanel = useUIStore((s) => s.activePanel);
  const setActivePanel = useUIStore((s) => s.setActivePanel);
  const leftSidebarWidth = useUIStore((s) => s.leftSidebarWidth);
  const rightSidebarWidth = useUIStore((s) => s.rightSidebarWidth);
  const setLeftSidebarWidth = useUIStore((s) => s.setLeftSidebarWidth);
  const setRightSidebarWidth = useUIStore((s) => s.setRightSidebarWidth);
  const resizeState = useUIStore((s) => s.resizeState);
  const setResizeState = useUIStore((s) => s.setResizeState);
  const connectionState = useUIStore((s) => s.connectionState);
  const statusMessage = useUIStore((s) => s.statusMessage);
  const floatingPanels = useUIStore((s) => s.floatingPanels);
  const closeFloatingPanel = useUIStore((s) => s.closeFloatingPanel);
  const moveFloatingPanel = useUIStore((s) => s.moveFloatingPanel);
  const resizeFloatingPanel = useUIStore((s) => s.resizeFloatingPanel);
  const bringFloatingToFront = useUIStore((s) => s.bringFloatingToFront);

  const tool = useUIStore((s) => s.tool);
  const setTool = useUIStore((s) => s.setTool);

  const [searchQuery, setSearchQuery] = useState('');
  const [showCanvasToolbar, setShowCanvasToolbar] = useState(true);

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
      const newWidth = Math.max(200, Math.min(380, resizeState.startWidth + delta));
      if (resizeState.sidebar === 'left') setLeftSidebarWidth(newWidth);
      else setRightSidebarWidth(newWidth);
    };
    const onUp = () => setResizeState(null);
    document.addEventListener('mousemove', onMove);
    document.addEventListener('mouseup', onUp);
    return () => { document.removeEventListener('mousemove', onMove); document.removeEventListener('mouseup', onUp); };
  }, [resizeState, setResizeState, setLeftSidebarWidth, setRightSidebarWidth]);

  return (
    <div className="app-shell">
      <Header version={version} />

      <div className="main-container">
        <SidebarLeft
          activeView={activeView}
          activePanel={activePanel}
          onViewSelect={setActiveView}
          onPanelSelect={setActivePanel}
          searchQuery={searchQuery}
          onSearchChange={setSearchQuery}
          sidebarWidth={leftSidebarWidth}
        />

        <div className="resize-handle" onMouseDown={handleLeftResizeStart} />

        <div className="canvas-area">
          {showCanvasToolbar && (
            <CanvasToolbar activeTool={tool} onToolChange={(t) => setTool(t as any)} />
          )}
          {viewContent ?? (
            <div className="empty-state">
              <div className="empty-state-icon">{'\u2B21'}</div>
              <div className="empty-state-title">画布 Canvas</div>
              <div>从侧栏选择视图模块 Select a view module from the sidebar</div>
            </div>
          )}
        </div>

        <div className="resize-handle" onMouseDown={handleRightResizeStart} />

        <SidebarRight width={rightSidebarWidth} activeModule={activePanel}>
          {panelContent}
        </SidebarRight>
      </div>

      <StatusBar
        nodes={nodes}
        constraints={constraints}
        connectionState={connectionState}
        statusMessage={statusMessage}
        version={version}
      />

      {/* Floating windows */}
      {floatingPanels.map((fp) => (
        <FloatingWindow
          key={fp.id}
          panel={fp}
          title={PANEL_TITLES[fp.panelKey] ?? fp.panelKey}
          onClose={() => closeFloatingPanel(fp.id)}
          onMove={moveFloatingPanel}
          onResize={resizeFloatingPanel}
          onFocus={bringFloatingToFront}
        >
          {renderPanelContent(fp.panelKey)}
        </FloatingWindow>
      ))}
    </div>
  );
};

export default Layout;
