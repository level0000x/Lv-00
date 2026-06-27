import React, { useState, useCallback, useRef, useEffect } from 'react';
import { TreeNode, TreeNodeStatus, trustColorToCSS } from '../types';
import { Empty } from '../shared';

const statusIcon: Record<string, string> = {
  proved: '✓', pending: '⏳', failed: '✗', assumed: '📖', root: '🏠',
};

const statusLabels: Record<TreeNodeStatus, string> = {
  proved: '已证 proved',
  pending: '待证 pending',
  failed: '失败 failed',
  assumed: '假设 assumed',
  root: '根 root',
};

const allStatuses: TreeNodeStatus[] = ['proved', 'pending', 'failed', 'assumed'];

/* ---- Context Menu ---- */
interface CtxMenuState {
  x: number; y: number;
  nodeId: string;
}

const CtxMenu: React.FC<{
  state: CtxMenuState;
  onClose: () => void;
  onRename: (nodeId: string) => void;
  onAddChild: (nodeId: string) => void;
  onDelete: (nodeId: string) => void;
  onStatusChange: (nodeId: string, status: TreeNodeStatus) => void;
  canDelete: boolean;
}> = ({ state, onClose, onRename, onAddChild, onDelete, onStatusChange, canDelete }) => {
  const ref = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const handler = (e: MouseEvent) => {
      if (ref.current && !ref.current.contains(e.target as HTMLElement)) onClose();
    };
    document.addEventListener('mousedown', handler);
    return () => document.removeEventListener('mousedown', handler);
  }, [onClose]);

  return (
    <div
      ref={ref}
      className="context-menu"
      style={{ left: state.x, top: state.y }}
    >
      <div className="context-menu-item" onClick={() => { onRename(state.nodeId); onClose(); }}>
        重命名 Rename
      </div>
      <div className="context-menu-item" onClick={() => { onAddChild(state.nodeId); onClose(); }}>
        添加子节点 Add Child
      </div>
      {canDelete && (
        <div className="context-menu-item" style={{ color: 'var(--color-danger)' }} onClick={() => { onDelete(state.nodeId); onClose(); }}>
          删除 Delete
        </div>
      )}
      <div className="context-menu-separator" />
      <div style={{ padding: '4px 12px', color: 'var(--color-text-muted)', fontSize: 10, textTransform: 'uppercase' }}>
        状态 Status
      </div>
      {allStatuses.map((s) => (
        <div key={s} className="context-menu-item" onClick={() => { onStatusChange(state.nodeId, s); onClose(); }}>
          <span>{statusIcon[s]} {statusLabels[s]}</span>
        </div>
      ))}
    </div>
  );
};

/* ---- Inline label editor ---- */
const LabelEditor: React.FC<{
  value: string;
  onCommit: (val: string) => void;
  onCancel: () => void;
}> = ({ value, onCommit, onCancel }) => {
  const ref = useRef<HTMLInputElement>(null);
  const [val, setVal] = useState(value);

  useEffect(() => {
    ref.current?.focus();
    ref.current?.select();
  }, []);

  return (
    <input
      ref={ref}
      value={val}
      onChange={(e) => setVal(e.target.value)}
      onBlur={() => onCommit(val)}
      onKeyDown={(e) => {
        if (e.key === 'Enter') onCommit(val);
        if (e.key === 'Escape') onCancel();
      }}
      style={{
        background: 'var(--color-bg-primary)',
        border: '1px solid var(--color-accent)',
        borderRadius: 3,
        color: 'var(--color-text-bright)',
        fontFamily: 'var(--font-sans)',
        fontSize: 12,
        padding: '1px 6px',
        outline: 'none',
        width: 160,
        boxShadow: '0 0 0 2px rgba(var(--color-accent-rgb), 0.15)',
      }}
    />
  );
};

/* ---- Tree node item ---- */
interface TreeNodeItemProps {
  node: TreeNode;
  expanded: Set<string>;
  onToggle: (id: string) => void;
  onSelect: (node: TreeNode) => void;
  onCtxMenu: (e: React.MouseEvent, nodeId: string) => void;
  onDblClick: (nodeId: string) => void;
  editingId: string | null;
  onEditCommit: (id: string, val: string) => void;
  onEditCancel: () => void;
  depth: number;
}

const TreeNodeItem: React.FC<TreeNodeItemProps> = React.memo(({
  node, expanded, onToggle, onSelect, onCtxMenu, onDblClick,
  editingId, onEditCommit, onEditCancel, depth,
}) => {
  const hasChildren = node.children.length > 0;
  const isOpen = expanded.has(node.id);
  const col = trustColorToCSS(node.trustColor);
  const isEditing = editingId === node.id;

  return (
    <div style={{ marginLeft: depth > 0 ? 20 : 0 }}>
      <div
        onClick={() => {
          if (!isEditing) {
            if (hasChildren) onToggle(node.id);
            if (node.nodeId) onSelect(node);
          }
        }}
        onContextMenu={(e) => { e.preventDefault(); onCtxMenu(e, node.id); }}
        onDoubleClick={() => onDblClick(node.id)}
        style={{
          display: 'flex', alignItems: 'center', cursor: 'pointer',
          padding: '5px 8px', borderRadius: 4, fontSize: 12,
          color: 'var(--color-text-primary)', gap: 6,
          transition: 'background var(--transition-fast)',
        }}
        onMouseEnter={(e) => { (e.currentTarget as HTMLElement).style.background = 'var(--color-bg-hover-subtle)'; }}
        onMouseLeave={(e) => { (e.currentTarget as HTMLElement).style.background = 'transparent'; }}
      >
        <span style={{ fontSize: 10, width: 12, color: 'var(--color-text-secondary)' }}>
          {hasChildren ? (isOpen ? '▼' : '▶') : ' '}
        </span>
        <span style={{ color: col }}>{statusIcon[node.status] ?? '•'}</span>
        <span style={{ display: 'inline-block', width: 8, height: 8, borderRadius: '50%', background: col }} />
        {isEditing ? (
          <LabelEditor
            value={node.label}
            onCommit={(val) => onEditCommit(node.id, val)}
            onCancel={onEditCancel}
          />
        ) : (
          <span>{node.label}</span>
        )}
      </div>
      {hasChildren && isOpen && node.children.map((c) => (
        <TreeNodeItem
          key={c.id}
          node={c}
          expanded={expanded}
          onToggle={onToggle}
          onSelect={onSelect}
          onCtxMenu={onCtxMenu}
          onDblClick={onDblClick}
          editingId={editingId}
          onEditCommit={onEditCommit}
          onEditCancel={onEditCancel}
          depth={depth + 1}
        />
      ))}
    </div>
  );
});
TreeNodeItem.displayName = 'TreeNodeItem';

/* ---- Deep helpers ---- */
function deepFind(nodes: TreeNode[], id: string): TreeNode | null {
  for (const n of nodes) {
    if (n.id === id) return n;
    const found = deepFind(n.children, id);
    if (found) return found;
  }
  return null;
}

function deepClone(n: TreeNode): TreeNode {
  return { ...n, children: n.children.map(deepClone) };
}

function deepSetLabel(root: TreeNode, id: string, label: string): TreeNode {
  if (root.id === id) return { ...root, label };
  return { ...root, children: root.children.map((c) => deepSetLabel(c, id, label)) };
}

function deepSetStatus(root: TreeNode, id: string, status: TreeNodeStatus): TreeNode {
  if (root.id === id) return { ...root, status };
  return { ...root, children: root.children.map((c) => deepSetStatus(c, id, status)) };
}

function deepAddChild(root: TreeNode, parentId: string, child: TreeNode): TreeNode {
  if (root.id === parentId) return { ...root, children: [...root.children, child] };
  return { ...root, children: root.children.map((c) => deepAddChild(c, parentId, child)) };
}

function deepDelete(root: TreeNode, id: string): TreeNode {
  if (root.id === id) return { ...root, children: [] }; // can't delete root, clear children
  return {
    ...root,
    children: root.children
      .filter((c) => c.id !== id)
      .map((c) => deepDelete(c, id)),
  };
}

let _nodeIdCounter = 1000;
function nextNodeId() { return ++_nodeIdCounter; }

/* ---- Main Tree View ---- */
interface TreeViewProps {
  tree: TreeNode | null;
  onNodeSelect?: (nodeId: number) => void;
  onTreeChange?: (tree: TreeNode) => void;
}

export const TreeView: React.FC<TreeViewProps> = ({ tree: externalTree, onNodeSelect, onTreeChange }) => {
  const [expanded, setExpanded] = useState<Set<string>>(new Set());
  const [localTree, setLocalTree] = useState<TreeNode | null>(externalTree ? deepClone(externalTree) : null);
  const [ctxMenu, setCtxMenu] = useState<CtxMenuState | null>(null);
  const [editingId, setEditingId] = useState<string | null>(null);

  /* Sync from external */
  useEffect(() => {
    if (externalTree) setLocalTree(deepClone(externalTree));
  }, [externalTree]);

  const notifyChange = useCallback((t: TreeNode) => {
    setLocalTree(t);
    onTreeChange?.(t);
  }, [onTreeChange]);

  const toggle = (id: string) => setExpanded((p) => {
    const n = new Set(p);
    n.has(id) ? n.delete(id) : n.add(id);
    return n;
  });

  /* Context menu actions */
  const handleRename = (nodeId: string) => setEditingId(nodeId);

  const handleAddChild = (parentId: string) => {
    if (!localTree) return;
    const newChild: TreeNode = {
      id: `n_${nextNodeId()}`,
      label: 'New Node 新节点',
      trustColor: 'BLUE',
      status: 'pending',
      nodeId: nextNodeId(),
      children: [],
    };
    const updated = deepAddChild(localTree, parentId, newChild);
    // Auto-expand parent
    if (!expanded.has(parentId)) {
      setExpanded((p) => new Set(p).add(parentId));
    }
    notifyChange(updated);
  };

  const handleDelete = (nodeId: string) => {
    if (!localTree) return;
    const updated = deepDelete(localTree, nodeId);
    notifyChange(updated);
  };

  const handleStatusChange = (nodeId: string, status: TreeNodeStatus) => {
    if (!localTree) return;
    const updated = deepSetStatus(localTree, nodeId, status);
    notifyChange(updated);
  };

  const handleEditCommit = (id: string, val: string) => {
    if (!localTree || !val.trim()) { setEditingId(null); return; }
    const updated = deepSetLabel(localTree, id, val.trim());
    notifyChange(updated);
    setEditingId(null);
  };

  const tree = localTree;

  if (!tree) return <Empty msg="无依赖数据 No dependency data" icon={'🌿'} />;

  return (
    <div style={{ width: '100%', height: '100%', display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
      {/* Toolbar */}
      <div style={{
        padding: '4px 8px',
        borderBottom: '1px solid var(--color-border-secondary)',
        display: 'flex',
        gap: 4,
        flexShrink: 0,
      }}>
        <button
          className="btn btn-small"
          onClick={() => {
            if (tree) handleAddChild(tree.id);
          }}
        >
          + 添加子节点 Add Node
        </button>
        <button
          className="btn btn-small"
          onClick={() => setExpanded(new Set())}
        >
          全部折叠 Collapse
        </button>
        <button
          className="btn btn-small"
          onClick={() => {
            if (tree) {
              const allIds = new Set<string>();
              const collect = (n: TreeNode) => { allIds.add(n.id); n.children.forEach(collect); };
              collect(tree);
              setExpanded(allIds);
            }
          }}
        >
          全部展开 Expand
        </button>
      </div>

      {/* Tree content */}
      <div style={{ flex: 1, overflow: 'auto', padding: '4px 0' }}>
        <TreeNodeItem
          node={tree}
          expanded={expanded}
          onToggle={toggle}
          onSelect={(n) => { if (n.nodeId > 0) onNodeSelect?.(n.nodeId); }}
          onCtxMenu={(e, nodeId) => setCtxMenu({ x: e.clientX, y: e.clientY, nodeId })}
          onDblClick={(nodeId) => setEditingId(nodeId)}
          editingId={editingId}
          onEditCommit={handleEditCommit}
          onEditCancel={() => setEditingId(null)}
          depth={0}
        />
      </div>

      {/* Context menu */}
      {ctxMenu && (
        <CtxMenu
          state={ctxMenu}
          onClose={() => setCtxMenu(null)}
          onRename={handleRename}
          onAddChild={handleAddChild}
          onDelete={handleDelete}
          onStatusChange={handleStatusChange}
          canDelete={ctxMenu.nodeId !== (tree?.id ?? '')}
        />
      )}
    </div>
  );
};
