import React, { useState } from 'react';
import { TreeNode, trustColorToCSS } from '../types';
import { Empty } from '../shared';

const statusIcon: Record<string, string> = { proved: '\u2713', pending: '\u23F3', failed: '\u2717', assumed: '\uD83D\uDCD6', root: '\uD83D\uDCC1' };

const TreeNodeItem: React.FC<{ node: TreeNode; expanded: Set<string>; onToggle: (id: string) => void; onSelect: (node: TreeNode) => void; depth: number }> = React.memo(({ node, expanded, onToggle, onSelect, depth }) => {
  const hasChildren = node.children.length > 0;
  const isOpen = expanded.has(node.id);
  const col = trustColorToCSS(node.trustColor);
  return (
    <div style={{ marginLeft: depth > 0 ? 20 : 0 }}>
      <div onClick={() => { if (hasChildren) onToggle(node.id); if (node.nodeId) onSelect(node); }}
        style={{ display: 'flex', alignItems: 'center', cursor: 'pointer', padding: '5px 8px', borderRadius: 4, fontSize: 12, color: 'var(--color-text-primary)', gap: 6, transition: 'background var(--transition-fast)' }}
        onMouseEnter={(e) => { (e.currentTarget as HTMLElement).style.background = 'var(--color-bg-hover-subtle)'; }}
        onMouseLeave={(e) => { (e.currentTarget as HTMLElement).style.background = 'transparent'; }}>
        <span style={{ fontSize: 10, width: 12, color: 'var(--color-text-secondary)' }}>{hasChildren ? (isOpen ? '\u25BC' : '\u25B6') : ' '}</span>
        <span style={{ color: col }}>{statusIcon[node.status] ?? '\u2022'}</span>
        <span style={{ display: 'inline-block', width: 8, height: 8, borderRadius: '50%', background: col }} />
        <span>{node.label}</span>
      </div>
      {hasChildren && isOpen && node.children.map((c) => <TreeNodeItem key={c.id} node={c} expanded={expanded} onToggle={onToggle} onSelect={onSelect} depth={depth + 1} />)}
    </div>
  );
});
TreeNodeItem.displayName = 'TreeNodeItem';

interface TreeViewProps { tree: TreeNode | null; onNodeSelect?: (nodeId: number) => void; }

export const TreeView: React.FC<TreeViewProps> = ({ tree, onNodeSelect }) => {
  const [expanded, setExpanded] = useState<Set<string>>(new Set());
  const toggle = (id: string) => setExpanded(p => { const n = new Set(p); n.has(id) ? n.delete(id) : n.add(id); return n; });
  if (!tree) return <Empty msg="No dependency data" icon={'\uD83C\uDF33'} />;
  return <div style={{ width: '100%', height: '100%', overflow: 'auto', padding: '4px 0' }}>
    <TreeNodeItem node={tree} expanded={expanded} onToggle={toggle} onSelect={(n) => { if (n.nodeId > 0) onNodeSelect?.(n.nodeId); }} depth={0} />
  </div>;
};
