import React from 'react';
import Layout from './Layout';
import ErrorBoundary from '../L2-components/ErrorBoundary';
import ToastContainer from '../L2-components/Toast';
import { useUIStore } from '../L5-core/store/uiStore';

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

import type { DrawCmd, CanvasEvent, TableRow, TreeNode, TopoBlock, TopoEdge } from '../L3-modules/types';

interface AppData {
  drawCommands: DrawCmd[];
  textValue: string;
  tableRows: TableRow[];
  selectedRowId: number | null;
  treeData: TreeNode | null;
  topoBlocks: TopoBlock[];
  topoEdges: TopoEdge[];
  nodes: number;
  constraints: number;
}

export function App() {
  const activeModule = useUIStore((s) => s.activeModule);

  const data: AppData = {
    drawCommands: [],
    textValue: 'point A at (100, 200)\npoint B at (400, 150)\nsegment AB between A and B',
    tableRows: [],
    selectedRowId: null,
    treeData: null,
    topoBlocks: [],
    topoEdges: [],
    nodes: 0,
    constraints: 0,
  };

  const handleCanvasEvent = (e: CanvasEvent) => {
    console.log('[Canvas]', e.type, e.screenX, e.screenY);
  };

  const handleTextChange = (text: string) => {
    console.log('[Text] changed:', text.length, 'chars');
  };

  const handleRowClick = (id: number) => {
    console.log('[Table] row clicked:', id);
  };

  const handleNodeSelect = (nodeId: number) => {
    console.log('[Tree] node selected:', nodeId);
  };

  const handleTerminalSubmit = (cmd: string) => {
    console.log('[Terminal] command:', cmd);
  };

  const handleBlockMove = (blockId: number, x: number, y: number) => {
    console.log('[Topology] block', blockId, 'moved to', x, y);
  };

  const handleFormulaSubmit = (text: string) => {
    console.log('[Formula] evaluate:', text);
  };

  const handleUndo = () => console.log('Undo');
  const handleRedo = () => console.log('Redo');
  const handleNormalize = () => console.log('Normalize');

  const renderCanvasContent = () => {
    switch (activeModule) {
      case 'canvas':
        return (
          <CanvasView
            commands={data.drawCommands}
            onCanvasEvent={handleCanvasEvent}
            width={1100}
            height={650}
          />
        );
      case 'text':
        return <TextView value={data.textValue} onChange={handleTextChange} />;
      case 'table':
        return <TableView rows={data.tableRows} selectedId={data.selectedRowId} onRowClick={handleRowClick} />;
      case 'tree':
        return <TreeView tree={data.treeData} onNodeSelect={handleNodeSelect} />;
      case 'terminal':
        return <TerminalView onSubmit={handleTerminalSubmit} />;
      case 'topology':
        return <TopologyView blocks={data.topoBlocks} edges={data.topoEdges} onBlockMove={handleBlockMove} />;
      default:
        return (
          <div className="empty-state">
            <div className="empty-state-icon">{'\u2B21'}</div>
            <div>Select a view module</div>
          </div>
        );
    }
  };

  const renderRightPanel = () => {
    switch (activeModule) {
      case 'formula': return <FormulaPanel onSubmit={handleFormulaSubmit} />;
      case 'graph': return <GraphPanel />;
      case 'block': return <BlockPanel />;
      case 'proof': return <ProofPanel />;
      case 'type': return <TypePanel />;
      case 'recurse': return <RecursePanel />;
      case 'engine': return <EnginePanel />;
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
        nodes={data.nodes}
        constraints={data.constraints}
        version="3.0.0"
        onUndo={handleUndo}
        onRedo={handleRedo}
        onNormalize={handleNormalize}
        canUndo={false}
        canRedo={false}
      />
      <ToastContainer />
    </ErrorBoundary>
  );
}
