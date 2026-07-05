import React, { useState, useEffect, useCallback } from 'react';
import Layout from './Layout';
import ErrorBoundary from '../L2-components/ErrorBoundary';
import ToastContainer from '../L2-components/Toast';
import CommandPalette, { CommandItem } from '../L2-components/CommandPalette';
import { useUIStore } from '../L5-core/store/uiStore';
import { useGeometryStore } from '../L5-core/store/geometryStore';
import { ViewKey, PanelKey } from '../L5-core/types';

import { CanvasView } from '../L3-modules/M1-Canvas';
import { TextView } from '../L3-modules/M2-Text';
import { TableView } from '../L3-modules/M3-Table';
import { TreeView } from '../L3-modules/M4-Tree';
import { TerminalView } from '../L3-modules/M5-Terminal';
import { TopologyView } from '../L3-modules/M6-Topology';
import { FormulaPanel } from '../L3-modules/P1-Formula';
import { GraphPanel } from '../L3-modules/P2-Graph';
import { BlockPanel } from '../L3-modules/P3-Block';
import { ProofPanel } from '../L3-modules/P4-Proof';
import { TypePanel } from '../L3-modules/P5-Type';
import { RecursePanel } from '../L3-modules/P6-Recurse';
import { DebugPanel } from '../L3-modules/P7-Debug';
import { EnginePanel } from '../L3-modules/P8-Engine';
import { HelpPanel } from '../L3-modules/P-Help';

function renderView(key: ViewKey, activeTool: string) {
  switch (key) {
    case 'canvas': return <CanvasView activeTool={activeTool} />;
    case 'text': return <TextView value="" onChange={() => {}} />;
    case 'table': return <TableView />;
    case 'tree': return <TreeView />;
    case 'terminal': return <TerminalView />;
    case 'topology': return <TopologyView blocks={[]} edges={[]} onBlockMove={() => {}} />;
    default: return <div className="empty-state">Unknown view</div>;
  }
}

function renderPanel(key: PanelKey) {
  switch (key) {
    case 'formula': return <FormulaPanel />;
    case 'graph': return <GraphPanel />;
    case 'block': return <BlockPanel />;
    case 'proof': return <ProofPanel />;
    case 'type': return <TypePanel />;
    case 'recurse': return <RecursePanel />;
    case 'engine': return <EnginePanel />;
    case 'debug': return <DebugPanel />;
    case 'help': return <HelpPanel />;
  }
}

export function App() {
  const tool = useUIStore((s) => s.tool);
  const activeView = useUIStore((s) => s.activeView);
  const activePanel = useUIStore((s) => s.activePanel);
  const setActivePanel = useUIStore((s) => s.setActivePanel);
  const setActiveView = useUIStore((s) => s.setActiveView);
  const [showPalette, setShowPalette] = useState(false);

  useEffect(() => { useGeometryStore.getState().loadDemoScene(); }, []);

  const COMMANDS: CommandItem[] = [
    { id: 'view.canvas', label: 'Canvas', shortcut: '1', group: 'Views' },
    { id: 'view.text', label: 'Text Editor', shortcut: '2', group: 'Views' },
    { id: 'view.table', label: 'Table', shortcut: '3', group: 'Views' },
    { id: 'view.tree', label: 'Tree', shortcut: '4', group: 'Views' },
    { id: 'view.terminal', label: 'Terminal', shortcut: '5', group: 'Views' },
    { id: 'view.topology', label: 'Topology', shortcut: '6', group: 'Views' },
    { id: 'panel.formula', label: 'Formula', shortcut: 'Shift+1', group: 'Panels' },
    { id: 'panel.graph', label: 'Graph', shortcut: 'Shift+2', group: 'Panels' },
    { id: 'panel.block', label: 'Block', group: 'Panels' },
    { id: 'panel.proof', label: 'Proof', group: 'Panels' },
    { id: 'panel.type', label: 'Type', group: 'Panels' },
    { id: 'panel.engine', label: 'Engine', group: 'Panels' },
    { id: 'panel.debug', label: 'Debug', group: 'Panels' },
    { id: 'panel.help', label: 'Help', group: 'Panels' },
  ];

  const handleCommand = useCallback((id: string) => {
    const [cat, key] = id.split('.');
    if (cat === 'view') setActiveView(key as ViewKey);
    else if (cat === 'panel') setActivePanel(key as PanelKey);
  }, [setActiveView, setActivePanel]);

  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      if ((e.ctrlKey || e.metaKey) && e.key === 'p') { e.preventDefault(); setShowPalette(v => !v); }
    };
    window.addEventListener('keydown', handler);
    return () => window.removeEventListener('keydown', handler);
  }, []);

  const objectCount = useGeometryStore((s) => s.objects.length);
  const constraintCount = useGeometryStore((s) => s.constraints.length);

  return (
    <ErrorBoundary>
      <Layout
        viewContent={renderView(activeView, tool)}
        panelContent={renderPanel(activePanel)}
        renderPanelContent={renderPanel}
        nodes={objectCount}
        constraints={constraintCount}
        version="3.4.0"
      />
      <ToastContainer />
      {showPalette && <CommandPalette commands={COMMANDS} onSelect={handleCommand} onClose={() => setShowPalette(false)} />}
    </ErrorBoundary>
  );
}
