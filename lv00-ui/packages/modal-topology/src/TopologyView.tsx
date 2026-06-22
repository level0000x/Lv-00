// ============================================================
// @lv00/modal-topology — M6 拓扑蓝图视图 (TopologyView)
// React Flow 节点-连接器可视化 + 拖拽布局 + 自定义函数块节点
// ============================================================

import React, { useEffect, useState, useCallback, useMemo } from 'react';
import { SceneController } from '@lv00/scene-controller';
import { BlockNode, TopologyEdge, TopologyGraph } from '@lv00/protocol';

// ReactFlow 类型声明
declare const ReactFlowC: any;
declare const ControlsC: any;
declare const BackgroundC: any;
declare const MiniMapC: any;
type RFNode = { id: string; type: string; position: { x: number; y: number }; data: any };
type RFEdge = { id: string; source: string; target: string; style?: any };

interface TopologyViewProps {
  controller: SceneController;
}

// ---- 自定义块节点（纯 React） ----

const BlockNodeView: React.FC<{ data: any }> = ({ data }) => (
  <div style={{
    background: 'linear-gradient(135deg, #1a1a2e 0%, #16213e 100%)',
    border: '2px solid #4caf50',
    borderRadius: 10,
    padding: '10px 14px',
    minWidth: 140,
    color: '#c8c8c8',
    boxShadow: '0 4px 16px rgba(0,0,0,0.3)',
  }}>
    <div style={{ fontWeight: 'bold', marginBottom: 8, textAlign: 'center', fontSize: 13 }}>
      {data.label}
    </div>
    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
      <div>
        {(data.inputs ?? []).map((p: any) => (
          <div key={p.id} style={{ display: 'flex', alignItems: 'center', gap: 6, fontSize: 11, color: '#888', marginBottom: 2 }}>
            <span style={{
              display: 'inline-block', width: 10, height: 10, borderRadius: '50%',
              background: '#4caf50', border: '1px solid #333',
            }} />
            {p.name}
          </div>
        ))}
      </div>
      <div style={{ textAlign: 'right' }}>
        {(data.outputs ?? []).map((p: any) => (
          <div key={p.id} style={{ display: 'flex', alignItems: 'center', justifyContent: 'flex-end', gap: 6, fontSize: 11, color: '#888', marginBottom: 2 }}>
            {p.name}
            <span style={{
              display: 'inline-block', width: 10, height: 10, borderRadius: '50%',
              background: '#ff9800', border: '1px solid #333',
            }} />
          </div>
        ))}
      </div>
    </div>
  </div>
);

// ---- 降级：无 ReactFlow 时的纯 SVG 视图 ----

export const TopologyView: React.FC<TopologyViewProps> = ({ controller }) => {
  const [topo, setTopo] = useState<TopologyGraph>({ blocks: [], edges: [] });
  const [dragBlock, setDragBlock] = useState<number | null>(null);
  const [dragOffset, setDragOffset] = useState({ x: 0, y: 0 });

  const refreshTopology = useCallback(() => {
    setTopo(controller.getTopologyView());
  }, [controller]);

  useEffect(() => {
    refreshTopology();
    return controller.onStateChange(refreshTopology);
  }, [controller, refreshTopology]);

  if (topo.blocks.length === 0) {
    return (
      <div style={{ padding: 20, color: '#666', textAlign: 'center' }}>
        暂无函数块。使用命令 <code>pack function</code> 创建块。
      </div>
    );
  }

  const BLOCK_W = 160;
  const BLOCK_H = 80;

  // 自动计算画布大小
  const maxX = Math.max(...topo.blocks.map(b => b.layoutX + BLOCK_W), 600);
  const maxY = Math.max(...topo.blocks.map(b => b.layoutY + BLOCK_H), 400);

  const handleBlockMouseDown = (blockId: number, e: React.MouseEvent) => {
    const block = topo.blocks.find(b => b.id === blockId);
    if (!block) return;
    setDragBlock(blockId);
    setDragOffset({ x: e.clientX - block.layoutX, y: e.clientY - block.layoutY });
  };

  const handleMouseMove = (e: React.MouseEvent) => {
    if (dragBlock === null) return;
    const newX = e.clientX - dragOffset.x;
    const newY = e.clientY - dragOffset.y;
    setTopo(prev => ({
      ...prev,
      blocks: prev.blocks.map(b => b.id === dragBlock ? { ...b, layoutX: newX, layoutY: newY } : b),
    }));
  };

  const handleMouseUp = () => {
    if (dragBlock !== null) {
      const block = topo.blocks.find(b => b.id === dragBlock);
      if (block) controller.setTopologyLayout(dragBlock, block.layoutX, block.layoutY);
    }
    setDragBlock(null);
  };

  return (
    <div style={{ width: '100%', height: '100%', overflow: 'auto', background: '#0a0a0a' }}>
      <svg
        width={maxX + 100}
        height={maxY + 100}
        style={{ minWidth: '100%', minHeight: '100%' }}
        onMouseMove={handleMouseMove}
        onMouseUp={handleMouseUp}
      >
        {/* 网格 */}
        <defs>
          <pattern id="grid" width="30" height="30" patternUnits="userSpaceOnUse">
            <path d="M 30 0 L 0 0 0 30" fill="none" stroke="#1a1a1a" strokeWidth="0.5" />
          </pattern>
        </defs>
        <rect width="100%" height="100%" fill="url(#grid)" />

        {/* 连接线 */}
        {topo.edges.map(edge => {
          const from = topo.blocks.find(b => b.id === edge.from_block);
          const to = topo.blocks.find(b => b.id === edge.to_block);
          if (!from || !to) return null;
          const x1 = from.layoutX + BLOCK_W;
          const y1 = from.layoutY + BLOCK_H / 2;
          const x2 = to.layoutX;
          const y2 = to.layoutY + BLOCK_H / 2;
          const mid = (x1 + x2) / 2;
          return (
            <path
              key={`edge-${edge.from_block}-${edge.to_block}`}
              d={`M${x1},${y1} C${mid},${y1} ${mid},${y2} ${x2},${y2}`}
              fill="none"
              stroke="#4caf50"
              strokeWidth={2}
              opacity={0.6}
            />
          );
        })}

        {/* 块节点 */}
        {topo.blocks.map(block => (
          <g
            key={block.id}
            onMouseDown={e => handleBlockMouseDown(block.id, e)}
            style={{ cursor: 'grab' }}
          >
            <rect
              x={block.layoutX}
              y={block.layoutY}
              width={BLOCK_W}
              height={BLOCK_H}
              rx={10}
              fill="#1a1a2e"
              stroke={dragBlock === block.id ? '#ff9800' : '#4caf50'}
              strokeWidth={2}
            />
            <text
              x={block.layoutX + BLOCK_W / 2}
              y={block.layoutY + 20}
              textAnchor="middle"
              fill="#c8c8c8"
              fontSize={13}
              fontWeight="bold"
            >
              {block.name}
            </text>
            {/* 输入端口 */}
            {block.inputs.map((p, i) => (
              <g key={p.id}>
                <circle cx={block.layoutX} cy={block.layoutY + 35 + i * 16} r={5} fill="#4caf50" stroke="#333" />
                <text x={block.layoutX + 10} y={block.layoutY + 39 + i * 16} fill="#888" fontSize={10}>{p.name}</text>
              </g>
            ))}
            {/* 输出端口 */}
            {block.outputs.map((p, i) => (
              <g key={p.id}>
                <circle cx={block.layoutX + BLOCK_W} cy={block.layoutY + 35 + i * 16} r={5} fill="#ff9800" stroke="#333" />
                <text x={block.layoutX + BLOCK_W - 10} y={block.layoutY + 39 + i * 16} textAnchor="end" fill="#888" fontSize={10}>{p.name}</text>
              </g>
            ))}
            <text
              x={block.layoutX + BLOCK_W / 2}
              y={block.layoutY + BLOCK_H - 8}
              textAnchor="middle"
              fill="#555"
              fontSize={9}
            >
              {block.inputs.length} in / {block.outputs.length} out
            </text>
          </g>
        ))}
      </svg>
    </div>
  );
};
