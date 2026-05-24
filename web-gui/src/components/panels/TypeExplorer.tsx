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
 */

import React, { useState, useCallback, useMemo } from 'react';
import Panel from './Panel';
import { useAppStore } from '@/stores';

// ================================================================
// 类型系统数据模型 / Type System Data Model
// 基于 type_system.h 的枚举和结构体设计
// ================================================================

/**
 * 宇宙层级 / Universe Level
 * 对应 C 头文件中的 UniverseLevel 类型和 UNIVERSE_BASE/UNIVERSE_TYPE_1 常量
 */
interface UniverseLevelInfo {
  level: number;
  name: string;
  nameZh: string;
  color: string;
  description: string;
  /** 该层级包含的类型区域 ID 列表 */
  typeRegionIds: number[];
}

/**
 * 类型种类 / Type Kind
 * 对应 C 头文件中的 TypeKind 枚举 (TYPE_KIND_POINT..TYPE_KIND_BOTTOM)
 */
type TypeKindUI =
  | 'point'
  | 'line_segment'
  | 'region'
  | 'function'
  | 'product'
  | 'sum'
  | 'variable'
  | 'dependent'
  | 'bottom';

interface TypeKindInfo {
  kind: TypeKindUI;
  name: string;
  nameZh: string;
  symbolicName: string;
  description: string;
  /** 该类型种类所属的宇宙层级 */
  universeLevel: number;
  /** 子类型（如果是复合类型） */
  children?: TypeKindUI[];
}

/**
 * 类型推断规则 / Type Inference Rule
 * 对应 C 头文件中的 TypeInferenceRule 结构体
 */
interface InferenceRuleUI {
  id: number;
  sourceType: string;
  targetKind: TypeKindUI;
  priority: number;
  description: string;
  descriptionZh: string;
}

/**
 * 类型等价路径 / Type Equivalence Path
 * 对应 type_system.h 中的 TypeRewriteStep 和 TypeRewritePath
 */
interface EquivPathStep {
  stepNumber: number;
  ruleName: string;
  typeBefore: string;
  typeAfter: string;
}

interface TypeEquivPath {
  sourceType: string;
  targetType: string;
  result: 'equiv' | 'not_equiv' | 'unknown' | 'needs_interaction';
  steps: EquivPathStep[];
}

/**
 * 树节点 / Tree Node
 */
interface TreeNode {
  id: string;
  label: string;
  labelZh: string;
  /** 显示在标签旁的符号名 */
  symbolicName?: string;
  universeLevel: number;
  color: string;
  expanded: boolean;
  children: TreeNode[];
  /** 该节点关联的数据 */
  data?: {
    kind?: TypeKindUI;
    rule?: InferenceRuleUI;
    equivPath?: TypeEquivPath;
    pointCount?: number;
    segmentCount?: number;
  };
}

// ================================================================
// 宇宙层级配色 / Universe Level Colors
// ================================================================

const UNIVERSE_COLORS: Record<number, string> = {
  0: '#51cf66',  // 第0层：基本几何体（点、线段）—— 绿色
  1: '#4dabf7',  // 第1层：类型区域 —— 蓝色
  2: '#ffd43b',  // 第2层：类型族 —— 黄色
  3: '#da77f2',  // 第3层：更高阶类型 —— 紫色
  4: '#ff922b',  // 第4层：元类型 —— 橙色
  5: '#f06595',  // 第5层+：超宇宙 —— 粉色
};

function getUniverseColor(level: number): string {
  return UNIVERSE_COLORS[level] ?? '#868e96';
}

// ================================================================
// 类型种类信息表 / Type Kind Info Table
// ================================================================

const TYPE_KIND_INFO: Record<TypeKindUI, TypeKindInfo> = {
  point: {
    kind: 'point',
    name: 'Point',
    nameZh: '点类型',
    symbolicName: 'Point',
    description: '基本点类型 / Basic point type',
    universeLevel: 0,
  },
  line_segment: {
    kind: 'line_segment',
    name: 'LineSegment',
    nameZh: '线段类型',
    symbolicName: 'Segment',
    description: '基本线段类型 / Basic line segment type',
    universeLevel: 0,
  },
  region: {
    kind: 'region',
    name: 'Region',
    nameZh: '区域类型',
    symbolicName: 'Region(n)',
    description: '多边形区域类型 / Polygon region type',
    universeLevel: 0,
  },
  function: {
    kind: 'function',
    name: 'Function',
    nameZh: '函数类型',
    symbolicName: 'A -> B',
    description: '函数类型（输入->输出）/ Function type (input -> output)',
    universeLevel: 1,
    children: ['point', 'line_segment', 'region', 'function'],
  },
  product: {
    kind: 'product',
    name: 'Product',
    nameZh: '乘积类型',
    symbolicName: 'A x B',
    description: '乘积类型 / Product type',
    universeLevel: 1,
    children: ['point', 'line_segment', 'region', 'function', 'product', 'sum'],
  },
  sum: {
    kind: 'sum',
    name: 'Sum',
    nameZh: '和类型',
    symbolicName: 'A + B',
    description: '和类型 / Sum type',
    universeLevel: 1,
    children: ['point', 'line_segment', 'region', 'function', 'product', 'sum'],
  },
  variable: {
    kind: 'variable',
    name: 'TypeVar',
    nameZh: '类型变量',
    symbolicName: 'alpha',
    description: '类型变量（多态）/ Type variable (polymorphic)',
    universeLevel: 0,
  },
  dependent: {
    kind: 'dependent',
    name: 'Dependent',
    nameZh: '依赖类型',
    symbolicName: 'Pi(x:A).B(x)',
    description: '依赖类型 / Dependent type',
    universeLevel: 2,
    children: ['point', 'line_segment', 'region', 'function'],
  },
  bottom: {
    kind: 'bottom',
    name: 'Bottom',
    nameZh: '底类型',
    symbolicName: '\u22A5',
    description: '底部类型（空类型）/ Bottom type (empty type)',
    universeLevel: 0,
  },
};

// ================================================================
// 默认推断规则 / Default Inference Rules
// 对应 type_system.h 中的 TypeInferenceRule 结构
// ================================================================

const DEFAULT_INFERENCE_RULES: InferenceRuleUI[] = [
  {
    id: 1,
    sourceType: 'GEOM_POINT',
    targetKind: 'point',
    priority: 0,
    description: 'Point node -> Point type',
    descriptionZh: '几何点节点 -> 点类型',
  },
  {
    id: 2,
    sourceType: 'GEOM_SEGMENT',
    targetKind: 'line_segment',
    priority: 0,
    description: 'Segment node -> LineSegment type',
    descriptionZh: '几何线段节点 -> 线段类型',
  },
  {
    id: 3,
    sourceType: 'GEOM_REGION',
    targetKind: 'region',
    priority: 1,
    description: 'Region node -> Region type',
    descriptionZh: '区域节点 -> 区域类型',
  },
  {
    id: 4,
    sourceType: 'GEOM_CONSTRAINT',
    targetKind: 'function',
    priority: 2,
    description: 'Constraint node -> Function type (input -> output)',
    descriptionZh: '约束节点 -> 函数类型（输入->输出）',
  },
  {
    id: 5,
    sourceType: 'GEOM_PORT',
    targetKind: 'variable',
    priority: 3,
    description: 'Port node -> Type variable (polymorphic)',
    descriptionZh: '端口节点 -> 类型变量（多态）',
  },
  {
    id: 6,
    sourceType: 'GEOM_FUNC_BLOCK',
    targetKind: 'dependent',
    priority: 4,
    description: 'Function block -> Dependent type',
    descriptionZh: '函数块 -> 依赖类型',
  },
];

// ================================================================
// 示例等价路径 / Example Equivalence Paths
// ================================================================

const DEMO_EQUIV_PATHS: TypeEquivPath[] = [
  {
    sourceType: 'Triangle',
    targetType: 'Polygon(3)',
    result: 'equiv',
    steps: [
      { stepNumber: 0, ruleName: 'expand', typeBefore: 'Triangle', typeAfter: 'Polygon(3)' },
    ],
  },
  {
    sourceType: 'Square',
    targetType: 'Rectangle',
    result: 'equiv',
    steps: [
      { stepNumber: 0, ruleName: 'expand_def', typeBefore: 'Square', typeAfter: 'Rectangle(4, equal_angles)' },
      { stepNumber: 1, ruleName: 'simplify', typeBefore: 'Rectangle(4, equal_angles)', typeAfter: 'Rectangle' },
    ],
  },
  {
    sourceType: 'Point -> Point',
    targetType: 'Segment',
    result: 'not_equiv',
    steps: [
      { stepNumber: 0, ruleName: 'unfold_function', typeBefore: 'Point -> Point', typeAfter: 'Function(Point, Point)' },
    ],
  },
];

// ================================================================
// 辅助：构建类型层级树 / Build Type Hierarchy Tree
// ================================================================

function buildTypeTree(
  pointsCount: number,
  segmentsCount: number,
  constraintsCount: number,
  regionsCount: number,
  portsCount: number,
  funcBlocksCount: number,
  inferenceRules: InferenceRuleUI[],
  equivPaths: TypeEquivPath[],
): TreeNode[] {
  const universeLevels: UniverseLevelInfo[] = [
    {
      level: 0,
      name: 'UNIVERSE_0 (BASE)',
      nameZh: '第0宇宙层：基本几何体',
      color: getUniverseColor(0),
      description: '基本几何类型：点、线段、区域 / Base geometric types',
      typeRegionIds: [1, 2, 3, 4, 5],
    },
    {
      level: 1,
      name: 'UNIVERSE_1 (TYPE)',
      nameZh: '第1宇宙层：类型区域',
      color: getUniverseColor(1),
      description: '复合类型：函数、乘积、和 / Compound types',
      typeRegionIds: [6, 7, 8],
    },
    {
      level: 2,
      name: 'UNIVERSE_2 (FAMILIES)',
      nameZh: '第2宇宙层：类型族',
      color: getUniverseColor(2),
      description: '依赖类型和高阶类型族 / Dependent types and higher-order families',
      typeRegionIds: [9],
    },
  ];

  const tree: TreeNode[] = [];

  for (const ul of universeLevels) {
    const styles = TYPE_KIND_INFO;
    const levelKinds = Object.values(styles).filter((info) => info.universeLevel === ul.level);

    // 构建宇宙层级节点
    const levelNode: TreeNode = {
      id: `universe-${ul.level}`,
      label: ul.name,
      labelZh: ul.nameZh,
      universeLevel: ul.level,
      color: ul.color,
      expanded: ul.level <= 1,
      children: [],
    };

    // 为每个类型种类创建子节点
    for (const kindInfo of levelKinds) {
      const kindNode: TreeNode = {
        id: `kind-${kindInfo.kind}`,
        label: kindInfo.name,
        labelZh: kindInfo.nameZh,
        symbolicName: kindInfo.symbolicName,
        universeLevel: ul.level,
        color: ul.color,
        expanded: false,
        children: [],
        data: { kind: kindInfo.kind },
      };

      // 添加子类型（如果该类型有子类型定义）
      if (kindInfo.children && kindInfo.children.length > 0) {
        for (const childKind of kindInfo.children) {
          const childInfo = styles[childKind];
          if (childInfo) {
            kindNode.children.push({
              id: `subkind-${kindInfo.kind}-${childKind}`,
              label: childInfo.name,
              labelZh: childInfo.nameZh,
              symbolicName: childInfo.symbolicName,
              universeLevel: childInfo.universeLevel,
              color: getUniverseColor(childInfo.universeLevel),
              expanded: false,
              children: [],
              data: { kind: childKind },
            });
          }
        }
      }

      levelNode.children.push(kindNode);
    }

    tree.push(levelNode);
  }

  // 添加 "当前几何状态" 节点
  const currentGeoNode: TreeNode = {
    id: 'current-geo',
    label: 'CURRENT GEOMETRY',
    labelZh: '当前几何状态',
    universeLevel: -1,
    color: 'var(--color-text-primary, #e0e0e0)',
    expanded: true,
    children: [],
  };

  // 点/线段/区域/约束统计
  if (pointsCount > 0) {
    const ptsNode: TreeNode = {
      id: 'geo-points',
      label: `Points (${pointsCount})`,
      labelZh: `点 (${pointsCount})`,
      universeLevel: 0,
      color: getUniverseColor(0),
      expanded: false,
      children: [],
      data: { kind: 'point', pointCount: pointsCount },
    };
    // 推断的点类型
    const ptRules = inferenceRules.filter((r) => r.sourceType === 'GEOM_POINT');
    for (const rule of ptRules) {
      ptsNode.children.push({
        id: `infer-pt-${rule.id}`,
        label: `${rule.targetKind.toUpperCase()} (P${rule.priority})`,
        labelZh: `${TYPE_KIND_INFO[rule.targetKind]?.nameZh ?? rule.targetKind} (优先级${rule.priority})`,
        universeLevel: 0,
        color: getUniverseColor(0),
        expanded: false,
        children: [],
        data: { rule },
      });
    }
    currentGeoNode.children.push(ptsNode);
  }

  if (segmentsCount > 0) {
    const segsNode: TreeNode = {
      id: 'geo-segments',
      label: `Segments (${segmentsCount})`,
      labelZh: `线段 (${segmentsCount})`,
      universeLevel: 0,
      color: getUniverseColor(0),
      expanded: false,
      children: [],
      data: { kind: 'line_segment', segmentCount: segmentsCount },
    };
    currentGeoNode.children.push(segsNode);
  }

  if (constraintsCount > 0) {
    const consNode: TreeNode = {
      id: 'geo-constraints',
      label: `Constraints (${constraintsCount})`,
      labelZh: `约束 (${constraintsCount})`,
      universeLevel: 1,
      color: getUniverseColor(1),
      expanded: false,
      children: [],
    };
    currentGeoNode.children.push(consNode);
  }

  if (regionsCount > 0 || portsCount > 0 || funcBlocksCount > 0) {
    const miscParts: string[] = [];
    if (regionsCount > 0) miscParts.push(`Regions(${regionsCount})`);
    if (portsCount > 0) miscParts.push(`Ports(${portsCount})`);
    if (funcBlocksCount > 0) miscParts.push(`Blocks(${funcBlocksCount})`);
    currentGeoNode.children.push({
      id: 'geo-misc',
      label: miscParts.join(', '),
      labelZh: miscParts.join(', '),
      universeLevel: 0,
      color: getUniverseColor(0),
      expanded: false,
      children: [],
    });
  }

  tree.push(currentGeoNode);

  // 添加 "推断规则" 节点
  const rulesNode: TreeNode = {
    id: 'rules',
    label: 'INFERENCE RULES',
    labelZh: '推断规则',
    universeLevel: -1,
    color: 'var(--color-text-secondary, #8b949e)',
    expanded: false,
    children: [],
  };

  for (const rule of inferenceRules) {
    const kindInfo = TYPE_KIND_INFO[rule.targetKind];
    rulesNode.children.push({
      id: `rule-${rule.id}`,
      label: `${rule.sourceType} -> ${rule.targetKind.toUpperCase()} [P${rule.priority}]`,
      labelZh: `${rule.sourceType} -> ${kindInfo?.nameZh ?? rule.targetKind} [优先级${rule.priority}]`,
      universeLevel: kindInfo?.universeLevel ?? 0,
      color: getUniverseColor(kindInfo?.universeLevel ?? 0),
      expanded: false,
      children: [],
      data: { rule },
    });
  }

  tree.push(rulesNode);

  // 添加 "等价路径" 节点
  if (equivPaths.length > 0) {
    const equivNode: TreeNode = {
      id: 'equiv-paths',
      label: 'EQUIVALENCE PATHS',
      labelZh: '等价路径',
      universeLevel: -1,
      color: 'var(--color-text-secondary, #8b949e)',
      expanded: false,
      children: [],
    };

    for (const path of equivPaths) {
      const resultIcon = path.result === 'equiv' ? '=' : path.result === 'not_equiv' ? '!=' : '?';
      const resultColor =
        path.result === 'equiv' ? '#51cf66' :
        path.result === 'not_equiv' ? '#ff6b6b' :
        '#ffd43b';

      const pathNode: TreeNode = {
        id: `equiv-${path.sourceType}-${path.targetType}`,
        label: `${path.sourceType} ${resultIcon} ${path.targetType}`,
        labelZh: `${path.sourceType} ${resultIcon} ${path.targetType}`,
        universeLevel: 1,
        color: resultColor,
        expanded: false,
        children: [],
        data: { equivPath: path },
      };

      for (const step of path.steps) {
        pathNode.children.push({
          id: `step-${path.sourceType}-${step.stepNumber}`,
          label: `[${step.stepNumber}] ${step.ruleName}: ${step.typeBefore} -> ${step.typeAfter}`,
          labelZh: `[${step.stepNumber}] ${step.ruleName}: ${step.typeBefore} -> ${step.typeAfter}`,
          universeLevel: 1,
          color: 'var(--color-text-secondary, #8b949e)',
          expanded: false,
          children: [],
        });
      }

      equivNode.children.push(pathNode);
    }

    tree.push(equivNode);
  }

  return tree;
}

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
  const ports = useAppStore((s) => s.ports);
  const funcBlocks = useAppStore((s) => s.funcBlocks);

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
      ports.length,
      funcBlocks.length,
      DEFAULT_INFERENCE_RULES,
      DEMO_EQUIV_PATHS,
    );
  }, [points.length, segments.length, constraints.length, regions.length, ports.length, funcBlocks.length]);

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
