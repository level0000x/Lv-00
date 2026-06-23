import React from 'react';

interface StatusBarProps {
  nodes: number;
  constraints: number;
  connectionState: string;
  statusMessage?: string;
  version?: string;
  mouseWorldX?: number;
  mouseWorldY?: number;
}

const StatusBar: React.FC<StatusBarProps> = ({
  nodes,
  constraints,
  connectionState,
  statusMessage = 'Ready',
  version = '3.0.0',
  mouseWorldX,
  mouseWorldY,
}) => (
  <div className="status-bar">
    <div className="status-left">
      <span className={`connection-status ${connectionState}`}>
        <span className="connection-dot" />
        {connectionState === 'connected' ? 'Connected' : connectionState === 'connecting' ? 'Connecting...' : 'Disconnected'}
      </span>
      <span>{nodes} nodes</span>
      <span>{constraints} constraints</span>
    </div>
    <div className="status-center">{statusMessage}</div>
    <div className="status-right">
      {mouseWorldX !== undefined && mouseWorldY !== undefined && (
        <span>{mouseWorldX.toFixed(1)}, {mouseWorldY.toFixed(1)}</span>
      )}
      <span>v{version}</span>
    </div>
  </div>
);

export default StatusBar;
