import React, { useState, useCallback, useRef, useEffect } from 'react';
import { TopoBlock, TopoEdge, TopoPort } from '../types';
import { Empty } from '../shared';

interface TopologyViewProps {
  blocks: TopoBlock[];
  edges: TopoEdge[];
  onBlockMove?: (blockId: number, x: number, y: number) => void;
  onBlocksChange?: (blocks: TopoBlock[]) => void;
  onEdgesChange?: (edges: TopoEdge[]) => void;
}

const BW = 160;
const BH = 80;

/* ---- Context Menu ---- */
interface CtxMenuState {
  x: number; y: number;
  blockId: number;
}

/* ---- Port Editor Panel ---- */
const PortEditor: React.FC<{
  block: TopoBlock;
  onBlockUpdate: (updated: TopoBlock) => void;
  onClose: () => void;
}> = ({ block, onBlockUpdate, onClose }) => {
  const [inputs, setInputs] = useState(block.inputs.map((p) => ({ ...p })));
  const [outputs, setOutputs] = useState(block.outputs.map((p) => ({ ...p })));

  let portIdCounter = useRef(100).current;

  const commit = () => {
    onBlockUpdate({ ...block, inputs, outputs });
  };

  const addInput = () => {
    portIdCounter++;
    setInputs([...inputs, { id: portIdCounter, name: `in_${portIdCounter}` }]);
  };

  const addOutput = () => {
    portIdCounter++;
    setOutputs([...outputs, { id: portIdCounter, name: `out_${portIdCounter}` }]);
  };

  const removeInput = (idx: number) => setInputs(inputs.filter((_, i) => i !== idx));
  const removeOutput = (idx: number) => setOutputs(outputs.filter((_, i) => i !== idx));

  const updateInput = (idx: number, name: string) => {
    const next = [...inputs];
    next[idx] = { ...next[idx], name };
    setInputs(next);
  };

  const updateOutput = (idx: number, name: string) => {
    const next = [...outputs];
    next[idx] = { ...next[idx], name };
    setOutputs(next);
  };

  return (
    <div style={{
      background: 'var(--color-bg-elevated)',
      border: '1px solid var(--color-border-primary)',
      borderRadius: 6,
      padding: 12,
      boxShadow: 'var(--shadow-lg)',
      minWidth: 240,
      fontSize: 12,
    }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 8 }}>
        <span style={{ fontWeight: 600, color: 'var(--color-text-bright)' }}>
          编辑端口 Edit Ports — {block.name}
        </span>
        <button
          className="btn btn-small btn-ghost"
          onClick={() => { commit(); onClose(); }}
          style={{ color: 'var(--color-text-muted)', padding: '0 4px' }}
        >
          done
        </button>
      </div>

      {/* Inputs */}
      <div style={{ marginBottom: 8 }}>
        <div style={{ color: 'var(--color-success)', fontWeight: 600, marginBottom: 4, fontSize: 11, textTransform: 'uppercase' }}>
          输入 Inputs ({inputs.length})
        </div>
        {inputs.map((p, i) => (
          <div key={p.id} style={{ display: 'flex', gap: 4, alignItems: 'center', marginBottom: 3 }}>
            <span style={{ color: 'var(--color-text-muted)', width: 16, textAlign: 'center', fontSize: 10 }}>in</span>
            <input
              value={p.name}
              onChange={(e) => updateInput(i, e.target.value)}
              onBlur={commit}
              onKeyDown={(e) => { if (e.key === 'Enter') commit(); }}
              className="input"
              style={{ flex: 1, fontSize: 11, padding: '2px 6px' }}
            />
            <button
              onClick={() => { removeInput(i); commit(); }}
              style={{ background: 'none', border: 'none', color: 'var(--color-danger)', cursor: 'pointer', fontSize: 12 }}
            >
              x
            </button>
          </div>
        ))}
        <button className="btn btn-small btn-ghost" onClick={() => { addInput(); }} style={{ fontSize: 11 }}>
          + 输入 Input
        </button>
      </div>

      {/* Outputs */}
      <div>
        <div style={{ color: 'var(--color-warning)', fontWeight: 600, marginBottom: 4, fontSize: 11, textTransform: 'uppercase' }}>
          输出 Outputs ({outputs.length})
        </div>
        {outputs.map((p, i) => (
          <div key={p.id} style={{ display: 'flex', gap: 4, alignItems: 'center', marginBottom: 3 }}>
            <span style={{ color: 'var(--color-text-muted)', width: 16, textAlign: 'center', fontSize: 10 }}>out</span>
            <input
              value={p.name}
              onChange={(e) => updateOutput(i, e.target.value)}
              onBlur={commit}
              onKeyDown={(e) => { if (e.key === 'Enter') commit(); }}
              className="input"
              style={{ flex: 1, fontSize: 11, padding: '2px 6px' }}
            />
            <button
              onClick={() => { removeOutput(i); commit(); }}
              style={{ background: 'none', border: 'none', color: 'var(--color-danger)', cursor: 'pointer', fontSize: 12 }}
            >
              x
            </button>
          </div>
        ))}
        <button className="btn btn-small btn-ghost" onClick={() => { addOutput(); }} style={{ fontSize: 11 }}>
          + 输出 Output
        </button>
      </div>
    </div>
  );
};

export const TopologyView: React.FC<TopologyViewProps> = ({
  blocks: initialBlocks,
  edges: initialEdges,
  onBlockMove,
  onBlocksChange,
  onEdgesChange,
}) => {
  const [localBlocks, setLocalBlocks] = useState<TopoBlock[]>(() => initialBlocks.map((b) => ({ ...b, inputs: b.inputs.map((p) => ({ ...p })), outputs: b.outputs.map((p) => ({ ...p })) })));
  const [localEdges, setLocalEdges] = useState<TopoEdge[]>(() => [...initialEdges]);
  const [dragId, setDragId] = useState<number | null>(null);
  const [dragOff, setDragOff] = useState({ x: 0, y: 0 });
  const [layout, setLayout] = useState<Map<number, { x: number; y: number }>>(new Map(
    initialBlocks.map((b) => [b.id, { x: b.layoutX, y: b.layoutY }])
  ));

  /* Modes */
  const [addEdgeMode, setAddEdgeMode] = useState(false);
  const [addEdgeFirst, setAddEdgeFirst] = useState<number | null>(null);

  /* Selection */
  const [selectedBlockId, setSelectedBlockId] = useState<number | null>(null);
  const [editingBlockId, setEditingBlockId] = useState<number | null>(null);

  /* Context menu */
  const [ctxMenu, setCtxMenu] = useState<CtxMenuState | null>(null);
  const [renamingBlockId, setRenamingBlockId] = useState<number | null>(null);
  const [renameValue, setRenameValue] = useState('');
  const renameRef = useRef<HTMLInputElement>(null);

  /* Sync from props */
  useEffect(() => {
    setLocalBlocks(initialBlocks.map((b) => ({ ...b, inputs: b.inputs.map((p) => ({ ...p })), outputs: b.outputs.map((p) => ({ ...p })) })));
    setLocalEdges([...initialEdges]);
    setLayout(new Map(initialBlocks.map((b) => [b.id, { x: b.layoutX, y: b.layoutY }])));
  }, [initialBlocks, initialEdges]);

  /* Rename effect */
  useEffect(() => {
    if (renamingBlockId !== null && renameRef.current) {
      renameRef.current.focus();
      renameRef.current.select();
    }
  }, [renamingBlockId]);

  /* Click away to close menus */
  useEffect(() => {
    const handler = (e: MouseEvent) => {
      if (ctxMenu && !(e.target as HTMLElement).closest('.context-menu')) {
        setCtxMenu(null);
      }
    };
    document.addEventListener('mousedown', handler);
    return () => document.removeEventListener('mousedown', handler);
  }, [ctxMenu]);

  const notifyBlocks = useCallback((next: TopoBlock[]) => {
    setLocalBlocks(next);
    onBlocksChange?.(next);
  }, [onBlocksChange]);

  const notifyEdges = useCallback((next: TopoEdge[]) => {
    setLocalEdges(next);
    onEdgesChange?.(next);
  }, [onEdgesChange]);

  if (!localBlocks.length) return <Empty msg="无函数块 No function blocks" icon={'⬛'} />;

  const mx = Math.max(...localBlocks.map((b) => (layout.get(b.id)?.x ?? b.layoutX) + BW), 600);
  const my = Math.max(...localBlocks.map((b) => (layout.get(b.id)?.y ?? b.layoutY) + BH), 400);

  const moveBlock = (id: number, x: number, y: number) => {
    setLayout((prev) => {
      const next = new Map(prev);
      next.set(id, { x: Math.round(x / 20) * 20, y: Math.round(y / 20) * 20 });
      return next;
    });
  };

  /* ---- Add block ---- */
  const addBlock = () => {
    const maxId = localBlocks.reduce((m, b) => Math.max(m, b.id), 0);
    const newId = maxId + 1;
    const newBlock: TopoBlock = {
      id: newId,
      name: `Block_${newId}`,
      inputs: [{ id: newId * 100 + 1, name: 'input' }],
      outputs: [{ id: newId * 100 + 2, name: 'output' }],
      layoutX: 100 + (newId % 3) * 200,
      layoutY: 60 + Math.floor(newId / 3) * 120,
    };
    layout.set(newId, { x: newBlock.layoutX, y: newBlock.layoutY });
    notifyBlocks([...localBlocks, newBlock]);
  };

  /* ---- Rename block ---- */
  const startRename = (blockId: number) => {
    const b = localBlocks.find((bl) => bl.id === blockId);
    if (b) {
      setRenamingBlockId(blockId);
      setRenameValue(b.name);
    }
  };

  const commitRename = () => {
    if (renamingBlockId !== null && renameValue.trim()) {
      notifyBlocks(localBlocks.map((b) => b.id === renamingBlockId ? { ...b, name: renameValue.trim() } : b));
    }
    setRenamingBlockId(null);
  };

  /* ---- Delete block ---- */
  const deleteBlock = (blockId: number) => {
    notifyBlocks(localBlocks.filter((b) => b.id !== blockId));
    notifyEdges(localEdges.filter((e) => e.fromBlock !== blockId && e.toBlock !== blockId));
    layout.delete(blockId);
    if (selectedBlockId === blockId) setSelectedBlockId(null);
  };

  /* ---- Block update from port editor ---- */
  const updateBlock = (updated: TopoBlock) => {
    notifyBlocks(localBlocks.map((b) => b.id === updated.id ? updated : b));
  };

  /* ---- Add edge ---- */
  const handleBlockClick = (blockId: number) => {
    if (addEdgeMode) {
      if (addEdgeFirst === null) {
        setAddEdgeFirst(blockId);
      } else if (addEdgeFirst !== blockId) {
        const newEdge: TopoEdge = {
          fromBlock: addEdgeFirst,
          fromPort: 0,
          toBlock: blockId,
          toPort: 0,
        };
        notifyEdges([...localEdges, newEdge]);
        setAddEdgeFirst(null);
      } else {
        setAddEdgeFirst(null);
      }
    } else {
      setSelectedBlockId(blockId === selectedBlockId ? null : blockId);
    }
  };

  /* ---- Delete edge (click on path) ---- */
  const handleEdgeClick = (e: React.MouseEvent, fromBlock: number, toBlock: number) => {
    e.stopPropagation();
    notifyEdges(localEdges.filter((ed) => !(ed.fromBlock === fromBlock && ed.toBlock === toBlock)));
  };

  /* Context menu */
  const handleBlockCtx = (e: React.MouseEvent, blockId: number) => {
    e.preventDefault();
    e.stopPropagation();
    setCtxMenu({ x: e.clientX, y: e.clientY, blockId });
    setSelectedBlockId(blockId);
  };

  const handleBlockDblClick = (blockId: number) => {
    setEditingBlockId(blockId);
  };

  const selectedBlock = localBlocks.find((b) => b.id === selectedBlockId);
  const editingBlock = localBlocks.find((b) => b.id === editingBlockId);

  return (
    <div style={{ width: '100%', height: '100%', display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
      {/* Toolbar */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: 6,
        padding: '4px 8px',
        borderBottom: '1px solid var(--color-border-secondary)',
        flexShrink: 0,
      }}>
        <button className="btn btn-small" onClick={addBlock}>
          + 添加块 Add Block
        </button>
        <button
          className={`btn btn-small ${addEdgeMode ? 'btn-primary' : ''}`}
          onClick={() => { setAddEdgeMode(!addEdgeMode); setAddEdgeFirst(null); }}
        >
          {addEdgeMode ? '连接模式 ON (点两个块)' : '连线 Add Edge'}
        </button>
        {addEdgeMode && addEdgeFirst !== null && (
          <span style={{ fontSize: 11, color: 'var(--color-accent)' }}>
            已选第一个块，请点击目标块 First block selected, click target
          </span>
        )}
        <span style={{ marginLeft: 'auto', fontSize: 10, color: 'var(--color-text-muted)' }}>
          右键菜单 / 双击编辑端口 Right-click / Dbl-click to edit
        </span>
      </div>

      {/* Main content area */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* SVG Canvas */}
        <div style={{ flex: 1, overflow: 'auto' }}>
          <svg
            width={mx + 100}
            height={my + 100}
            style={{ minWidth: '100%', minHeight: '100%' }}
            onClick={() => { if (!addEdgeMode) setSelectedBlockId(null); }}
            onMouseMove={(e) => {
              if (dragId === null) return;
              const svg = (e.currentTarget as SVGSVGElement).closest('div')?.getBoundingClientRect();
              if (!svg) return;
              const x = (e.clientX - svg.left) - dragOff.x;
              const y = (e.clientY - svg.top) - dragOff.y;
              moveBlock(dragId, x, y);
            }}
            onMouseUp={() => {
              if (dragId !== null && layout.has(dragId)) {
                const pos = layout.get(dragId)!;
                onBlockMove?.(dragId, pos.x, pos.y);
              }
              setDragId(null);
            }}
          >
            <defs>
              <pattern id="grid" width="30" height="30" patternUnits="userSpaceOnUse">
                <path d="M 30 0 L 0 0 0 30" fill="none" stroke="var(--color-border-secondary)" strokeWidth="0.5" />
              </pattern>
            </defs>
            <rect width="100%" height="100%" fill="url(#grid)" />

            {/* Edges */}
            {localEdges.map((e) => {
              const f = localBlocks.find((b) => b.id === e.fromBlock);
              const t = localBlocks.find((b) => b.id === e.toBlock);
              if (!f || !t) return null;
              const fPos = layout.get(f.id) ?? { x: f.layoutX, y: f.layoutY };
              const tPos = layout.get(t.id) ?? { x: t.layoutX, y: t.layoutY };
              const x1 = fPos.x + BW;
              const y1 = fPos.y + BH / 2;
              const x2 = tPos.x;
              const y2 = tPos.y + BH / 2;
              const midX = (x1 + x2) / 2;
              return (
                <g key={`e-${e.fromBlock}-${e.toBlock}`}>
                  {/* Invisible thick path for click target */}
                  <path
                    d={`M${x1},${y1} C${midX},${y1} ${midX},${y2} ${x2},${y2}`}
                    fill="none"
                    stroke="transparent"
                    strokeWidth={12}
                    style={{ cursor: 'pointer' }}
                    onClick={(ev) => handleEdgeClick(ev, e.fromBlock, e.toBlock)}
                  />
                  <path
                    d={`M${x1},${y1} C${midX},${y1} ${midX},${y2} ${x2},${y2}`}
                    fill="none"
                    stroke="var(--color-accent)"
                    strokeWidth={2}
                    opacity={0.5}
                    style={{ pointerEvents: 'none' }}
                  />
                </g>
              );
            })}

            {/* Blocks */}
            {localBlocks.map((b) => {
              const pos = layout.get(b.id) ?? { x: b.layoutX, y: b.layoutY };
              const isSelected = b.id === selectedBlockId;
              const isEdgeTarget = addEdgeMode && addEdgeFirst === b.id;
              return (
                <g
                  key={b.id}
                  onMouseDown={(e) => {
                    if (addEdgeMode) return;
                    e.stopPropagation();
                    const svg = (e.currentTarget as SVGGElement).closest('div')?.getBoundingClientRect();
                    if (!svg) return;
                    setDragId(b.id);
                    setDragOff({ x: e.clientX - svg.left - pos.x, y: e.clientY - svg.top - pos.y });
                  }}
                  onClick={(e) => { e.stopPropagation(); handleBlockClick(b.id); }}
                  onContextMenu={(e) => handleBlockCtx(e, b.id)}
                  onDoubleClick={() => handleBlockDblClick(b.id)}
                  style={{ cursor: addEdgeMode ? 'crosshair' : 'grab' }}
                >
                  <rect
                    x={pos.x}
                    y={pos.y}
                    width={BW}
                    height={BH}
                    rx={10}
                    fill="var(--color-bg-secondary)"
                    stroke={isEdgeTarget ? 'var(--color-success)' : isSelected ? 'var(--color-warning)' : dragId === b.id ? 'var(--color-warning)' : 'var(--color-accent)'}
                    strokeWidth={isSelected || isEdgeTarget ? 2.5 : 2}
                  />
                  {/* Block name (or rename input placeholder) */}
                  {renamingBlockId !== b.id && (
                    <text
                      x={pos.x + BW / 2}
                      y={pos.y + 22}
                      textAnchor="middle"
                      fill="var(--color-text-primary)"
                      fontSize={13}
                      fontWeight="bold"
                      style={{ pointerEvents: 'none' }}
                    >
                      {b.name}
                    </text>
                  )}
                  {/* Ports */}
                  {b.inputs.map((p, i) => (
                    <g key={p.id}>
                      <circle cx={pos.x} cy={pos.y + 38 + i * 16} r={5} fill="var(--color-success)" stroke="var(--color-border-primary)" />
                      <text x={pos.x + 10} y={pos.y + 42 + i * 16} fill="var(--color-text-secondary)" fontSize={10}>
                        {p.name}
                      </text>
                    </g>
                  ))}
                  {b.outputs.map((p, i) => (
                    <g key={p.id}>
                      <circle cx={pos.x + BW} cy={pos.y + 38 + i * 16} r={5} fill="var(--color-warning)" stroke="var(--color-border-primary)" />
                      <text x={pos.x + BW - 10} y={pos.y + 42 + i * 16} textAnchor="end" fill="var(--color-text-secondary)" fontSize={10}>
                        {p.name}
                      </text>
                    </g>
                  ))}
                  <text
                    x={pos.x + BW / 2}
                    y={pos.y + BH - 8}
                    textAnchor="middle"
                    fill="var(--color-text-secondary)"
                    fontSize={9}
                  >
                    {b.inputs.length} in / {b.outputs.length} out
                  </text>
                </g>
              );
            })}
          </svg>

          {/* Rename overlay (HTML input positioned over SVG) */}
          {renamingBlockId !== null && (() => {
            const b = localBlocks.find((bl) => bl.id === renamingBlockId);
            if (!b) return null;
            const pos = layout.get(b.id) ?? { x: b.layoutX, y: b.layoutY };
            return (
              <input
                ref={renameRef}
                value={renameValue}
                onChange={(e) => setRenameValue(e.target.value)}
                onBlur={commitRename}
                onKeyDown={(e) => { if (e.key === 'Enter') commitRename(); if (e.key === 'Escape') setRenamingBlockId(null); }}
                style={{
                  position: 'absolute',
                  left: pos.x + BW / 2 - 50,
                  top: pos.y + 10,
                  width: 100,
                  background: 'var(--color-bg-elevated)',
                  border: '1px solid var(--color-accent)',
                  borderRadius: 3,
                  color: 'var(--color-text-bright)',
                  fontFamily: "'Consolas', monospace",
                  fontSize: 13,
                  padding: '2px 6px',
                  outline: 'none',
                  textAlign: 'center',
                  zIndex: 10,
                  boxShadow: 'var(--shadow-md)',
                }}
              />
            );
          })()}
        </div>

        {/* Side panel for port editing */}
        {editingBlock && (
          <div style={{
            width: 260,
            borderLeft: '1px solid var(--color-border-primary)',
            background: 'var(--color-bg-secondary)',
            padding: 8,
            overflow: 'auto',
            flexShrink: 0,
          }}>
            <PortEditor
              block={editingBlock}
              onBlockUpdate={updateBlock}
              onClose={() => setEditingBlockId(null)}
            />
          </div>
        )}
      </div>

      {/* Context Menu */}
      {ctxMenu && (
        <div
          className="context-menu"
          style={{ left: ctxMenu.x, top: ctxMenu.y }}
        >
          <div className="context-menu-item" onClick={() => { startRename(ctxMenu.blockId); setCtxMenu(null); }}>
            重命名 Rename
          </div>
          <div className="context-menu-item" onClick={() => { handleBlockDblClick(ctxMenu.blockId); setCtxMenu(null); }}>
            编辑端口 Edit Ports
          </div>
          <div className="context-menu-separator" />
          <div
            className="context-menu-item"
            style={{ color: 'var(--color-danger)' }}
            onClick={() => { deleteBlock(ctxMenu.blockId); setCtxMenu(null); }}
          >
            删除块 Delete Block
          </div>
        </div>
      )}
    </div>
  );
};
