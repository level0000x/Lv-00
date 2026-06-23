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
              node={ch