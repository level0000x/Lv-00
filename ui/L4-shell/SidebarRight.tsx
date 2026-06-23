import React from 'react';

interface SidebarRightProps {
  width: number;
  children?: React.ReactNode;
}

const SidebarRight: React.FC<SidebarRightProps> = ({ width, children }) => (
  <aside className="sidebar-right" style={{ width }}>
    <div style={{ padding: 12, borderBottom: '1px solid var(--color-border-primary)' }}>
      <span style={{ fontSize: 'var(--font-size-sm)', fontWeight: 600, color: 'var(--color-text-secondary)', textTransform: 'uppercase', letterSpacing: 1 }}>
        Properties
      </span>
    </div>
    <div style={{ flex: 1, overflowY: 'auto', padding: 12, fontSize: 'var(--font-size-md)' }}>
      {children ?? (
        <div style={{ color: 'var(--color-text-muted)', fontStyle: 'italic', padding: 8 }}>
          Select an item to view properties
        </div>
      )}
    </div>
  </aside>
);

export default SidebarRight;
