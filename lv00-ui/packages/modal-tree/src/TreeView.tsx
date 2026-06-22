// ============================================================
// @lv00/modal-tree — M4 依赖树导航器 (TreeView)
// 递归树组件 + Trust颜色标记 + 展开/折叠 + 选中联动
// ============================================================

import React, { useEffect, useState, useCallback, useMemo } from 'react';
import { SceneController } from '@lv00/scene-controller';
import { TreeNode } from '@lv00/protocol';

interface TreeViewProps {
  controller: SceneController;
}

function intToHex(color: number): string {
  const r = (color >> 16) & 0xFF;
  const g = (color >> 8) & 0xFF;
  const b = color & 0xFF;
  return `#${r.toString(16).padStart(2, '0')}${g.toString(16).padStart(2, '0')}${b.toString(16).padStart(2, '0')}`;
}

const STATUS_ICONS: Record<string, string> = {
  proved: '✓',
  pending: '⏳',
  failed: '✗',
  assumed: '📖',
  root: '📁',
};

// ---- 递归树节点组件 ----

const TreeNodeItem: React.FC<{
  node: TreeNode;
  expanded: Set<string>;
  onToggle: (id: string) => void;
  onClick: (node: TreeNode) => void;
  depth: number;
}> = React.memo(({ node, expanded, onToggle, onClick, depth }) => {
  const hasChildren = node.children.length > 0;
  const isExpanded = expanded.has(node.id);
  const colorHex = intToHex(node.color);
  const icon = STATUS_ICONS[node.status] ?? '•';

  const statusTag = node.status !== 'proved' && node.status !== 'root' ? (
    <span style={statusBadgeStyle}>{node.status}</span>
  ) : null;

  return (
    <div style={{ marginLeft: depth > 0 ? 20 : 0 }}>
      <div
        style={nodeRowStyle}
        onClick={() => {
          if (hasChildren) onToggle(node.id);
          if (node.node_id > 0) onClick(node);
        }}
      >
        <span style={{ marginRight: 6, fontSize: 11, width: 14, textAlign: 'center' }}>
          {hasChildren ? (isExpanded ? '▼' : '▶') : ' '}
        </span>
        <span style={{ marginRight: 6 }}>{icon}</span>
        <span style={{ color: colorHex, marginRight: 4 }}>●</span>
        <span style={{ fontSize: 13 }}>{node.label}</span>
        {statusTag}
      </div>
      {hasChildren && isExpanded && (
        <div>
          {node.children.map(child => (
            <TreeNodeItem
              key={child.id}
              node={child}
              expanded={expanded}
              onToggle={onToggle}
              onClick={onClick}
              depth={depth + 1}
            />
          ))}
        </div>
      )}
    </div>
  );
});

TreeNodeItem.displayName = 'TreeNodeItem';

// ---- TreeView 主组件 ----

export const TreeView: React.FC<TreeViewProps> = ({ controller }) => {
  const [treeData, setTreeData] = useState<TreeNode | null>(null);
  const [expandedNodes, setExpandedNodes] = useState<Set<string>>(new Set());

  // --- 数据刷新 ---
  const refreshTree = useCallback(() => {
    const tree = controller.getDependencyTree();
    setTreeData(tree);
    // 默认展开第一层
    if (tree && tree.children.length > 0) {
      setExpandedNodes(prev => {
        const next = new Set(prev);
        next.add(tree.id);
        tree.children.forEach(c => next.add(c.id));
        return next;
      });
    }
  }, [controller]);

  useEffect(() => {
    refreshTree();
    return controller.onStateChange(refreshTree);
  }, [controller, refreshTree]);

  // --- 展开/折叠 ---
  const toggleNode = useCallback((nodeId: string) => {
    setExpandedNodes(prev => {
      const next = new Set(prev);
      if (next.has(nodeId)) next.delete(nodeId);
      else next.add(nodeId);
      return next;
    });
  }, []);

  // --- 点击跳转 ---
  const handleNodeClick = useCallback((node: TreeNode) => {
    if (node.node_id > 0) controller.selectNode(node.node_id);
  }, [controller]);

  if (!treeData) {
    return (
      <div style={{ padding: 20, color: '#666' }}>
        暂无证明依赖数据。请在公理包加载后运行证明。
      </div>
    );
  }

  return (
    <div style={containerStyle}>
      <TreeNodeItem
        node={treeData}
        expanded={expandedNodes}
        onToggle={toggleNode}
        onClick={handleNodeClick}
        depth={0}
      />
    </div>
  );
};

// ---- 内联样式 ----

const containerStyle: React.CSSProperties = {
  padding: '10px 12px',
  overflow: 'auto',
  height: '100%',
  background: '#0a0a0a',
  color: '#c8c8c8',
  userSelect: 'none',
};

const nodeRowStyle: React.CSSProperties = {
  display: 'flex',
  alignItems: 'center',
  cursor: 'pointer',
  padding: '5px 8px',
  borderRadius: 4,
  transition: 'background 0.15s',
  fontSize: 13,
};

const statusBadgeStyle: React.CSSProperties = {
  marginLeft: 10,
  fontSize: 10,
  color: '#888',
  background: '#1a1a1a',
  padding: '1px 8px',
  borderRadius: 10,
  fontWeight: 600,
};
