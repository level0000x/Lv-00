import React from 'react';
import Layout from './Layout';
import ErrorBoundary from '../L2-components/ErrorBoundary';
import ToastContainer from '../L2-components/Toast';
import { useUIStore } from '../L5-core/store/uiStore';
import { KernelBridge, createMockBridge } from '../L5-core/protocol';

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

export function App() {
  const activeModule = useUIStore((s) => s.activeModule);

  const handleCanvasDown = (sx: number, sy: number, btn: number) => bridge.sendCanvasEvent({ type: 'MOUSE_DOWN', screenX: sx, screenY: sy, button: btn, shiftDown: false, ctrlDown: false, wheelDelta: 0, key: '' });
  const handleCanvasMove = (sx: number, sy: number) => bridge.sendCanvasEvent({ type: 'MOUSE_MOVE', screenX: sx, screenY: sy, button: 0, shiftDown: false, ctrlDown: false, wheelDelta: 0, key: '' });
  const handleCanvasUp = (sx: number, sy: number, btn: number) => bridge.sendCanvasEvent({ type: 'MOUSE_UP', screenX: sx, screenY: sy, button: btn, shiftDown: false, ctrlDown: false, wheelDelta: 0, key: '' });
  const handleWheel = (delta: number) => bridge.sendCanvasEvent({ type: 'MOUSE_WHEEL', screenX: 0, screenY: 0, button: 0, shiftDown: false, ctrlDown: false, wheelDelta: delta, key: '' });

  const renderCanvasContent = () => {
    switch (activeModule) {
      case 'canvas': return <CanvasView commands={bridge.getDrawCommands(0, 0, 1, 1100, 650).cmds} onMouseDown={handleCanvasDown} onMouseMove={handleCanvasMove} onMouseUp={handleCanvasUp} onWheel={handleWheel} width={1100} height={650} />;
      case 'text': return <TextView value={bridge.getDslText()} onChange={(t) => bridge.executeTerminal(t)} />;
      case 'table': return <TableView rows={bridge.getTableRows()} onRowClick={(id) => bridge.sendTableSelect({ rowId: id, ctrlDown: false })} />;
      case 'tree': return <TreeView tree={bridge.getTree()} onNodeSelect={(nid) => console.log('[Tree] select', nid)} />;
      case 'terminal': return <TerminalView onSubmit={(cmd) => { const r = bridge.executeTerminal(cmd); console.log('[Terminal]', r.output); }} />;
      case 'topology': {
        const g = bridge.getTopology();
        return <TopologyView blocks={g.blocks} edges={g.edges} onBlockMove={(bid, x, y) => bridge.sendBlockDrag({ blockId: bid, newX: x, newY: y })} />;
      }
      default: return <div className="empty-state"><div className="empty-state-icon">{'\u2B21'}</div><div>Select a view module</div></div>;
    }
  };

  const renderRightPanel = () => {
    switch (activeModule) {
      case 'formula': return <FormulaPanel onSubmit={(t) => console.log('[Formula]', t)} />;
      case 'graph': return <GraphPanel />;
      case 'block': return <BlockPanel />;
      case 'proof': return <ProofPanel />;
      case 'type': return <TypePanel />;
      case 'recurse': return <RecursePanel />;
      case 'engine': {
        const s = bridge.getEngineStatus();
        return <EnginePanel />;
      }
      case 'debug': return <DebugPanel />;
      case 'help': return <HelpPanel />;
      default: return null;
    }
  };

  return (
    <ErrorBoundary>
      <Layout
        canvasContent={renderCanvasContent()}
        rightPanelContent={renderRightPanel()}
        nodes={0}
        constraints={0}
        version="3.0.0"
      />
      <ToastContainer />
    </ErrorBoundary>
  );
}
