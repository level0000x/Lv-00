import React, { useState } from 'react';
import { TreeNode } from '../types';
import { Empty } from '../shared';
import { TRUST } from '../../L1-base/visual';

const statusIcon: Record<string, string> = {
  proved: '\u2713',
  pending: '\u23F3',
  failed: '\u2717',
  assumed: '\uD83D\uDCD6',
  root: '\uD83D\uDCC1',
};

const statusColor: Record<string, string> = {
  proved: TRUST.GREEN,
  pending: TRUST.YELLOW,
  failed: TRUST.RED,
  assumed: TRUST.BLUE,
  root: TRUST.GREEN,
};

const TreeNodeItem: React.FC<{
  node: TreeNode;
  expanded: Set<string>;
  onToggle: (id: string) => void;
  onSelect: (node: TreeNode) => void;
  depth: number;
}> = React.memo(({ node, expanded, onToggle, onSelect, depth }) => {
  const hasChildren = node.children.length > 0;
  const isOpen = expanded.has(node.id);

  return (
    <div style={{ marginLeft: depth > 0 ? 20 : 0 }}>
      <div
        onClick={() => {
          if (hasChildren) onToggle(node.id);
          if (node.nodeId) onSelect(node);
        }}
        style={{
          display: 'flex',
          alignItems: 'center',
          cursor: 'pointer',
          padding: '5px 8px',
          borderRadius: 4,
          fontSize: 12,
          color: 'var(--color-text-primary)',
          gap: 6,
          transition: 'background var(--transition-fast)',
        }}
        onMouseEnter={(e) => {
          (e.currentTarget as HTMLElement).style.background = 'var(--color-bg-hover-subtle)';
        }}
        onMouseLeave={(e) => {
          (e.currentTarget as HTMLElement).style.background = 'transparent';
        }}
      >
        <span style={{ fontSize: 10, width: 12, color: 'var(--color-text-secondary)' }}>
          {hasChildren ? (isOpen ? '\u25BC' : '\u25B6') : ' '}
        </span>
        <span style={{ color: statusColor[node.status] ?? 'var(--color-text-secondary)' }}>
          {statusIcon[node.status] ?? '\u2022'}
        </span>
        <span style={{
          display: 'inline-block', width: 8, height: 8, borderRadius: '50%',
          background: node.color ?? TRUST.GREEN,
        }} />
        <span>{node.label}</span>
      </div>
      {hasChildren && isOpen && node.children.map((c) => (
        <TreeNodeItem
          key={c.id}
          node={c}
          expanded={expanded}
          onToggle={onToggle}
          onSelect={onSelect}
          depth={depth + 1}
        />
      ))}
    </div>
  );
});

TreeNodeItem.displayName = 'TreeNodeItem';

interface TreeViewProps {
  tree: TreeNode | null;
  onNodeSelect?: (nodeId: number) => void;
}

export const TreeView: React.FC<TreeViewProps> = ({ tree, onNodeSelect }) => {
  const [expanded, setExpanded] = useState<Set<string>>(new Set());

  const handleToggle = (id: string) => {
    setExpanded((prev) => {
      const next = new Set(prev);
      if (next.has(id)) {
        next.delete(id);
      } else {
        next.add(id);
      }
      return next;
    });
  };

  if (!tree) return <Empty msg="No dependency data" icon={'\uD83C\uDF33'} />;

  return (
    <div style={{ width: '100%', height: '100%', overflow: 'auto', padding: '4px 0' }}>
      <TreeNodeItem
        node={tree}
        expanded={expanded}
        onToggle={handleToggle}
        onSelect={(n) => {
          if (n.nodeId !== undefined && n.nodeId > 0) onNodeSelect?.(n.nodeId);
        }}
        depth={0}
      />
    </div>
  );
};
