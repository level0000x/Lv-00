/**
 * @module components/panels/TypeExplorer
 * @description 交互式类型路径探索器 / Interactive Type Path Explorer.
 *
 *              基于 include/lv00/type_system.h 的类型系统概念实现：
 *              - 树形展示类型层级关系（宇宙层级）
 *              - 支持点击展开/折叠类型节点
 *              - 显示类型等价关系路径
 *              - 支持类型推断结果的可视化
 *              - 颜色编码不同宇宙层级
 *              - 参考 TypeInferenceRule 和宇宙层级概念
 *
 *              类型层级数据和推断规则已提取到 utils/ 目录：
 *              - typeHierarchy.ts: 类型种类信息、宇宙层级、树构建
 */

import React, { useState, useCallback, useMemo } from 'react';
import Panel from './Panel';
import { useAppStore } from '@/stores';

// ---- 提取的工具模块 / Extracted utility modules ----
import {
  TYPE_KIND_INFO,
  DEFAULT_INFERENCE_RULES,
  DEMO_EQUIV_PATHS,
  buildTypeTree,
  getUniverseColor,
} from './utils/typeHierarchy';
import type {
  TypeKindUI,
  TypeKindInfo,
  TreeNode,
} from './utils/typeHierarchy';

// ================================================================
// 树节点组件 / TreeNode Component
// ================================================================

interface TreeNodeProps {
  node: TreeNode;
  depth: number;
  onToggle: (id: string) => void;
}

const TreeNodeItem: React.FC<TreeNodeProps> = ({ node, depth, onToggle }) => {
  const hasChildren = node.children.length > 0;
  const isExpanded = node.expanded;
  const indentPx = depth * 16 + (hasChildren ? 0 : 12);

  return (
    <div className="type-ex-node" style={{ paddingLeft: `${indentPx}px` }}>
      {/* 节点行 / Node Row */}
      <div
        className="type-ex-node-row"
        onClick={() => hasChildren && onToggle(node.id)}
        style={{
          cursor: hasChildren ? 'pointer' : 'default',
          borderLeft: `3px solid ${node.color}`,
        }}
      >
        {/* 展开/折叠箭头 */}
        <span
          className="type-ex-arrow"
          style={{
            visibility: hasChildren ? 'visible' : 'hidden',
            transform: isExpanded ? 'rotate(0deg)' : 'rotate(-90deg)',
          }}
        >
          &#9660;
        </span>

        {/* 节点颜色圆点 */}
        {node.universeLevel >= 0 && (
          <span
            className="type-ex-dot"
            style={{ backgroundColor: node.color }}
          />
        )}

        {/* 节点标签 */}
        <span className="type-ex-label">
          {node.label}
        </span>

        {/* 符号名 */}
        {node.symbolicName && (
          <span className="type-ex-symbolic">{node.symbolicName}</span>
        )}

        {/* 子节点数量 badge */}
        {hasChildren && (
          <span className="type-ex-badge">{node.children.length}</span>
        )}
      </div>

      {/* 展开内容 / Expanded Children */}
      {isExpanded && hasChildren && (
        <div className="type-ex-children">
          {node.children.map((child) => (
            <TreeNodeItem
              key={child.id}
              node={child}
              depth={depth + 1}
              onToggle={onToggle}
            />
          ))}

          {/* 等价路径详情 / Equivalence Path Detail */}
          {node.data?.equivPath && (
            <div className="type-ex-equiv-detail">
              <div className="type-ex-equiv-status" style={{ color: node.color }}>
                {node.data.equivPath.result === 'equiv' ? 'EQUIVALENT / 等价' :
                 node.data.equivPath.result === 'not_equiv' ? 'NOT EQUIVALENT / 不等价' :
                 node.data.equivPath.result === 'unknown' ? 'UNKNOWN / 未知' :
                 'NEEDS INTERACTION / 需交互式证明'}
              </div>
              <div className="type-ex-equiv-steps-count">
                {node.data.equivPath.steps.length} step(s) / 步骤
              </div>
            </div>
          )}

          {/* 推断规则详情 / Inference Rule Detail */}
          {node.data?.rule && (
            <div className="type-ex-rule-detail">
              <div className="type-ex-rule-src">{node.data.rule.sourceType}</div>
              <span className="type-ex-rule-arrow">&rarr;</span>
              <div className="type-ex-rule-tgt">{node.data.rule.targetKind}</div>
              <span className="type-ex-rule-priority">P{node.data.rule.priority}</span>
            </div>
          )}
        </div>
      )}
    </div>
  );
};

// ================================================================
// TypeExplorer 主组件 / TypeExplorer Main Component
// ================================================================

const TypeExplorer: React.FC = () => {
  const addToast = useAppStore((s) => s.addToast);
  const appendLog = useAppStore((s) => s.appendLog);

  // 从 Store 读取几何状态
  const points = useAppStore((s) => s.points);
  const segments = useAppStore((s) => s.segments);
  const constraints = useAppStore((s) => s.constraints);
  const regions = useAppStore((s) => s.regions);
  // ports 和 funcBlocks 可能尚未在 AppState 类型中定义，使用安全访问
  const ports = (useAppStore.getState() as unknown as Record<string, unknown>).ports as Array<unknown> | undefined;
  const funcBlocks = (useAppStore.getState() as unknown as Record<string, unknown>).funcBlocks as Array<unknown> | undefined;

  // 状态管理
  const [expandedIds, setExpandedIds] = useState<Set<string>>(() => {
    const s = new Set<string>();
    // 默认展开前两级宇宙
    s.add('universe-0');
    s.add('universe-1');
    s.add('current-geo');
    return s;
  });
  const [highlightFilter, setHighlightFilter] = useState<TypeKindUI | null>(null);
  const [searchText, setSearchText] = useState('');

  // 构建树
  const tree = useMemo(() => {
    return buildTypeTree(
      points.length,
      segments.length,
      constraints.length,
      regions.length,
      ports?.length ?? 0,
      funcBlocks?.length ?? 0,
      DEFAULT_INFERENCE_RULES,
      DEMO_EQUIV_PATHS,
    );
  }, [points.length, segments.length, constraints.length, regions.length, ports?.length, funcBlocks?.length]);

  // 展开/折叠切换
  const handleToggle = useCallback((id: string) => {
    setExpandedIds((prev) => {
      const next = new Set(prev);
      if (next.has(id)) {
        next.delete(id);
      } else {
        next.add(id);
      }
      return next;
    });
  }, []);

  // 全部展开
  const handleExpandAll = useCallback(() => {
    const ids = new Set<string>();
    const collect = (nodes: TreeNode[]) => {
      for (const n of nodes) {
        if (n.children.length > 0) {
          ids.add(n.id);
          collect(n.children);
        }
      }
    };
    collect(tree);
    setExpandedIds(ids);
    addToast('info', '全部展开 / All expanded');
    appendLog('TypeExplorer: 全部节点展开', 'info');
  }, [tree, addToast, appendLog]);

  // 全部折叠
  const handleCollapseAll = useCallback(() => {
    setExpandedIds(new Set());
    addToast('info', '全部折叠 / All collapsed');
    appendLog('TypeExplorer: 全部节点折叠', 'info');
  }, [addToast, appendLog]);

  // 过滤搜索
  const filteredTree = useMemo(() => {
    if (!searchText.trim()) return tree;

    const lower = searchText.toLowerCase();

    // 递归过滤，保留匹配节点及其祖先
    const filterNodes = (nodes: TreeNode[]): TreeNode[] => {
      const result: TreeNode[] = [];
      for (const node of nodes) {
        const labelMatch =
          node.label.toLowerCase().includes(lower) ||
          node.labelZh.toLowerCase().includes(lower) ||
          (node.symbolicName ?? '').toLowerCase().includes(lower);

        const filteredChildren = filterNodes(node.children);

        if (labelMatch || filteredChildren.length > 0) {
          result.push({
            ...node,
            expanded: true, // 搜索时自动展开
            children: filteredChildren,
          });
        }
      }
      return result;
    };

    return filterNodes(tree);
  }, [tree, searchText]);

  // 将 expandedIds 注入到树节点中
  const treeWithExpansion = useMemo(() => {
    const applyExpansion = (nodes: TreeNode[]): TreeNode[] => {
      return nodes.map((node) => ({
        ...node,
        expanded: expandedIds.has(node.id),
        children: applyExpansion(node.children),
      }));
    };
    return applyExpansion(filteredTree);
  }, [filteredTree, expandedIds]);

  // 宇宙层级颜色图例
  const universeLegend = useMemo(() => {
    const levels = [0, 1, 2];
    return levels.map((l) => ({
      level: l,
      color: getUniverseColor(l),
      label: `U${l}`,
      labelZh: l === 0 ? '基本' : l === 1 ? '类型区域' : '类型族',
    }));
  }, []);

  return (
    <>
      <Panel title="TYPE EXPLORER / 类型探索器" panelId="type-explorer-ops">
        {/* 搜索框 */}
        <div className="input-row" style={{ marginBottom: '4px' }}>
          <input
            className="input-field"
            type="text"
            value={searchText}
            onChange={(e) => setSearchText(e.target.value)}
            placeholder="搜索类型... / Search types..."
            style={{ fontSize: '11px', padding: '4px 6px' }}
          />
          {searchText && (
            <button
              className="btn btn-small"
              onClick={() => setSearchText('')}
              style={{ width: 'auto', padding: '4px 8px', marginBottom: 0, fontSize: '10px' }}
            >
              X
            </button>
          )}
        </div>

        {/* 操作按钮行 */}
        <div className="nav-row">
          <button className="btn btn-small" onClick={handleExpandAll} style={{ flex: 1 }}>
            EXPAND ALL / 全部展开
          </button>
          <button className="btn btn-small" onClick={handleCollapseAll} style={{ flex: 1 }}>
            COLLAPSE ALL / 全部折叠
          </button>
        </div>

        {/* 高亮过滤：类型种类选择 */}
        <div style={{ display: 'flex', gap: '3px', flexWrap: 'wrap', marginBottom: '6px' }}>
          <button
            className="btn btn-small"
            onClick={() => setHighlightFilter(null)}
            style={{
              padding: '2px 6px',
              fontSize: '9px',
              marginBottom: 0,
              width: 'auto',
              background: highlightFilter === null ? 'var(--color-bg-overlay, #21262d)' : undefined,
              borderColor: highlightFilter === null ? 'var(--color-accent, #58a6ff)' : undefined,
            }}
          >
            ALL / 全部
          </button>
          {(Object.entries(TYPE_KIND_INFO) as [TypeKindUI, TypeKindInfo][]).map(([kind, info]) => (
            <button
              key={kind}
              className="btn btn-small"
              onClick={() => setHighlightFilter(kind === highlightFilter ? null : kind)}
              style={{
                padding: '2px 6px',
                fontSize: '9px',
                marginBottom: 0,
                width: 'auto',
                background: highlightFilter === kind ? `${getUniverseColor(info.universeLevel)}22` : undefined,
                borderColor: highlightFilter === kind ? getUniverseColor(info.universeLevel) : undefined,
                color: highlightFilter === kind ? getUniverseColor(info.universeLevel) : undefined,
                opacity: highlightFilter && highlightFilter !== kind ? 0.4 : 1,
              }}
            >
              {info.symbolicName}
            </button>
          ))}
        </div>
      </Panel>

      {/* 宇宙层级图例 */}
      <Panel title="UNIVERSE LEGEND / 宇宙层级图例" panelId="type-explorer-legend">
        <div style={{ display: 'flex', gap: '8px', alignItems: 'center', flexWrap: 'wrap' }}>
          {universeLegend.map((item) => (
            <div key={item.level} style={{ display: 'flex', alignItems: 'center', gap: '4px', fontSize: '10px' }}>
              <span
                style={{
                  width: 12,
                  height: 12,
                  borderRadius: 2,
                  backgroundColor: item.color,
                  display: 'inline-block',
                }}
              />
              <span style={{ color: 'var(--text, #e0e0e0)' }}>{item.label}</span>
              <span style={{ color: 'var(--text-muted, #868e96)' }}>{item.labelZh}</span>
            </div>
          ))}
        </div>
      </Panel>

      {/* 类型层级树 */}
      <Panel title="TYPE HIERARCHY / 类型层级" panelId="type-explorer-tree">
        <div className="type-explorer-tree">
          {treeWithExpansion.length === 0 ? (
            <div style={{ color: 'var(--text-muted, #868e96)', fontSize: '11px', textAlign: 'center', padding: '16px' }}>
              暂无类型数据 / No type data available
            </div>
          ) : (
            treeWithExpansion.map((node) => (
              <TreeNodeItem
                key={node.id}
                node={node}
                depth={0}
                onToggle={handleToggle}
              />
            ))
          )}
        </div>
      </Panel>
    </>
  );
};

export default TypeExplorer;
