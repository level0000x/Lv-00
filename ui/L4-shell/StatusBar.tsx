import React from 'react';
import { ModuleKey } from '../L5-core/types';

interface StatusBarProps {
  nodes: number;
  constraints: number;
  connectionState: string;
  statusMessage?: string;
  version?: string;
  mouseWorldX?: number;
  mouseWorldY?: number;
  activeModule?: ModuleKey;
}

const StatusBar: React.FC<StatusBarProps> = ({
  nodes,
  constraints,
  connectionState,
  statusMessage = '就绪 Ready',
  version = '3.4.0',
  mouseWorldX,
  mouseWorldY,
}) => (
  <div className="status-bar">
    <div className="status-left">
      <div className={`connection-status ${connectionState}`}>
        <span className="connection-dot" />
      </div>
      <div className="status-bar-item">
        {nodes} 节点 nodes
      </div>
      <div className="status-bar-item">
        {constraints} 约束 constraints
      </div>
    </div>
    <div className="status-center">{statusMessage}</div>
    <div className="status-right">
      {mouseWorldX !== undefined && mouseWorldY !== undefined && (
        <div className="status-bar-item" style={{ fontFamily: 'var(--font-mono)', fontSize: 11 }}>
          {mouseWorldX.toFixed(1)}, {mouseWorldY.toFixed(1)}
        </div>
      )}
      <div className="status-bar-item">Lv-00 DSL</div>
      <div className="status-bar-item">UTF-8</div>
      <div className="status-bar-item">v{version}</div>
    </div>
  </div>
);

export default StatusBar;
