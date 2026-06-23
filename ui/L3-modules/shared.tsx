import React from 'react';

export const Empty: React.FC<{ msg: string; icon: string }> = ({ msg, icon }) => (
  <div className="empty-state">
    <div className="empty-state-icon">{icon}</div>
    <div>{msg}</div>
  </div>
);
