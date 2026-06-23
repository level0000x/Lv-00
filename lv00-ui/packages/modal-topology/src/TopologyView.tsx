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
    return