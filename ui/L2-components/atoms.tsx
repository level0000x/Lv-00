import React from 'react';

export const Badge: React.FC<{ type: 'running' | 'completed' | 'failed' | 'total' | 'timeout' | 'pending'; label: string; count?: number }> = ({ type, label, count }) => (
  <span className={`badge ${type}`}>
    <span className={`status-dot ${type}`} />
    {label}{count !== undefined ? ` ${count}` : ''}
  </span>
);

export const StatusDot: React.FC<{ status: 'running' | 'completed' | 'failed' | 'pending' }> = ({ status }) => (
  <span className={`status-dot ${status}`} />
);

export const ConnectionStatus: React.FC<{ status: 'connected' | 'disconnected' | 'connecting' }> = ({ status }) => (
  <span className={`connection-status ${status}`}>
    <span className="connection-dot" />
    {status === 'connected' ? 'Connected' : status === 'connecting' ? 'Connecting...' : 'Disconnected'}
  </span>
);
