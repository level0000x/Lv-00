import React, { useState, useEffect, useCallback } from 'react';
import Layout from './Layout';
import ErrorBoundary from '../L2-components/ErrorBoundary';
import ToastContainer from '../L2-components/Toast';
import CommandPalette, { CommandItem } from '../L2-components/CommandPalette';
import { useUIStore } from '../L5-core/store/uiStore';
import { KernelBridge, createMockBridge } from '../L5-core/protocol';
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

const bridge: KernelBridge = createMockBridge();

function renderView(key: ViewKey, bridge: KernelBridge) {
  switch (key) {
    case 'canvas':
      return (
        <CanvasView
          commands={bridge.getDrawCommands(0, 0, 1, 1100, 650).cmds}
          onMouseDown={(sx, sy, btn) => bridge.sendCanvasEvent({ type: 'MOUSE_DOWN', screenX: sx, screenY: sy, button: btn, shiftDown: false, ctrlDown: false, wheelDelta: 0, key: '' })}
          onMouseMove={(sx, sy) => bridge.sendCanvasEvent({ type: 'MOUSE_MOVE', screenX: sx, screenY: sy, button: 0, shiftDown: false, ctrlDown: false, wheelDelta: 0, key: '' })}
          onMouseUp={(sx, sy, btn) => bridge.sendCanvasEvent({ type: 'MOUSE_UP', screenX: sx, screenY: sy, button: btn, shiftDown: false, ctrlDown: false, wheelDelta: 0, key: '' })}
          onWheel={(delta) => bridge.sendCanvasEvent({ type: 'MOUSE_WHEEL', screenX: 0, screenY: 0, button: 0, shiftDown: false, ctrlDown: false, wheelDelta: delta, key: '' })}
          width={1100}
          height={650}
        />
      );
    case 'text':
      return <TextView value={bridge.getDslText()} onChange={(t) => bridge.executeTerminal(t)} />;
    case 'table':
      return <TableView rows={bridge.getTableRows()} onRowClick={(id) => bridge.sendTableSelect({ rowId: id, ctrlDown: false })} />;
    case 'tree':
      return <TreeView tree={bridge.getTree()} onNodeSelect={(nid) => console.log('[Tree] select', nid)} />;
    case 'terminal':
      return <TerminalView onSubmit={(cmd) => { const r = bridge.executeTerminal(cmd); console.log('[Terminal]', r.output); }} />;
    case 'topology': {
      const g = bridge.getTopology();
      return <TopologyView blocks={g.blocks} edges={g.edges} onBlockMove={(bid, x, y) => bridge.sendBlockDrag({ blockId: bid, newX: x, newY: y })} />;
    }
  }
}

function renderPanel(key: PanelKey) {
  switch (key) {
    case 'formula': return <FormulaPanel onSubmit={(t) => console.log('[Formula]', t)} />;
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
  const activeView = useUIStore((s) => s.activeView);
  const activePanel = useUIStore((s) => s.activePanel);
  const setActivePanel = useUIStore((s) => s.setActivePanel);
  const setActiveView = useUIStore((s) => s.setActiveView);
  const [showPalette, setShowPalette] = useState(false);

  const engineStatus = bridge.getEngineStatus();

  const COMMANDS: CommandItem[] = [
    { id: 'view.canvas', label: '画布 Canvas', shortcut: '1', group: '视图 Views' },
    { id: 'view.text', label: '文本编辑 Text', shortcut: '2', group: '视图 Views' },
    { id: 'view.table', label: '数据表 Table', shortcut: '3', group: '视图 Views' },
    { id: 'view.tree', label: '证明树 Tree', shortcut: '4', group: '视图 Views' },
    { id: 'view.terminal', label: '终端 Terminal', shortcut: '5', group: '视图 Views' },
    { id: 'view.topology', label: '拓扑 Topology', shortcut: '6', group: '视图 Views' },
    { id: 'panel.formula', label: '公式 Formula', shortcut: 'Shift+1', group: '面板 Panels' },
    { id: 'panel.graph', label: '约束图 Graph', shortcut: 'Shift+2', group: '面板 Panels' },
    { id: 'panel.block', label: '函数块 Block', group: '面板 Panels' },
    { id: 'panel.proof', label: '证明 Proof', group: '面板 Panels' },
    { id: 'panel.type', label: '类型 Type', group: '面板 Panels' },
    { id: 'panel.engine', label: '引擎 Engine', group: '面板 Panels' },
    { id: 'panel.debug', label: '调试 Debug', group: '面板 Panels' },
    { id: 'panel.help', label: '帮助 Help', group: '面板 Panels' },
    { id: 'toggle.theme', label: '切换主题 Toggle Theme', shortcut: 'Ctrl+T', group: '操作 Actions' },
    { id: 'toggle.grid', label: '切换网格 Toggle Grid', shortcut: 'Ctrl+G', group: '操作 Actions' },
    { id: 'popout.panel', label: '弹出面板 Pop-out Panel', group: '操作 Actions' },
  ];

  const handleCommand = useCallback((id: string) => {
    const [cat, key] = id.split('.');
    if (cat === 'view') setActiveView(key as ViewKey);
    else if (cat === 'panel') setActivePanel(key as PanelKey);
    else if (id === 'toggle.theme') {
      const store = useUIStore.getState();
      useUIStore.getState().setTheme(store.theme === 'dark' ? 'light' : 'dark');
    }
    else if (id === 'popout.panel') {
      useUIStore.getState().openFloatingPanel(useUIStore.getState().activePanel);
    }
  }, [setActiveView, setActivePanel]);

  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      if ((e.ctrlKey || e.metaKey) && e.key === 'p') { e.preventDefault(); setShowPalette(v => !v); }
    };
    window.addEventListener('keydown', handler);
    return () => window.removeEventListener('keydown', handler);
  }, []);

  return (
    <ErrorBoundary>
      <Layout
        viewContent={renderView(activeView, bridge)}
        panelContent={renderPanel(activePanel)}
        renderPanelContent={renderPanel}
        nodes={engineStatus.nodeCount}
        constraints={engineStatus.constraintCount}
        version="3.4.0"
      />
      <ToastContainer />
      {showPalette && (
        <CommandPalette commands={COMMANDS} onSelect={handleCommand} onClose={() => setShowPalette(false)} />
      )}
    </ErrorBoundary>
  );
}
