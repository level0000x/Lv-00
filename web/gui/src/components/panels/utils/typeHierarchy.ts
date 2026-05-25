/**
 * @module components/panels/utils/typeHierarchy
 * @description 类型层级数据定义与树构建工具。
 *              基于 type_system.h 的类型系统概念，提供类型种类信息、
 *              宇宙层级配色和类型层级树的构建功能。
 *
 *              Type hierarchy data definitions and tree builder.
 *              Based on type_system.h concepts, provides type kind info,
 *              universe level colors, and type hierarchy tree construction.
 */

// ================================================================
// 类型定义 / Type Definitions
// ================================================================

/** 类型种类 UI 标识 */
export type TypeKindUI =
  | 'point'
  | 'line_segment'
  | 'region'
  | 'function'
  | 'product'
  | 'sum'
  | 'variable'
  | 'dependent'
  | 'bottom';

/** 类型种类信息 */
export interface TypeKindInfo {
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

/** 类型推断规则 */
export interface InferenceRuleUI {
  id: number;
  sourceType: string;
  targetKind: TypeKindUI;
  priority: number;
  description: string;
  descriptionZh: string;
}

/** 等价路径步骤 */
export interface EquivPathStep {
  stepNumber: number;
  ruleName: string;
  typeBefore: string;
  typeAfter: string;
}

/** 类型等价路径 */
export interface TypeEquivPath {
  sourceType: string;
  targetType: string;
  result: 'equiv' | 'not_equiv' | 'unknown' | 'needs_interaction';
  steps: EquivPathStep[];
}

/** 宇宙层级信息 */
export interface UniverseLevelInfo {
  level: number;
  name: string;
  nameZh: string;
  color: string;
  description: string;
  typeRegionIds: number[];
}

/** 树节点 */
export interface TreeNode {
  id: string;
  label: string;
  labelZh: string;
  symbolicName?: string;
  universeLevel: number;
  color: string;
  expanded: boolean;
  children: TreeNode[];
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

/** 宇宙层级颜色映射 */
export const UNIVERSE_COLORS: Record<number, string> = {
  0: '#51cf66',  // 第0层：基本几何体（点、线段）-- 绿色
  1: '#4dabf7',  // 第1层：类型区域 -- 蓝色
  2: '#ffd43b',  // 第2层：类型族 -- 黄色
  3: '#da77f2',  // 第3层：更高阶类型 -- 紫色
  4: '#ff922b',  // 第4层：元类型 -- 橙色
  5: '#f06595',  // 第5层+：超宇宙 -- 粉色
};

/**
 * 根据宇宙层级获取颜色。
 * @param level - 宇宙层级编号
 * @returns 颜色字符串
 */
export function getUniverseColor(level: number): string {
  return UNIVERSE_COLORS[level] ?? '#868e96';
}

// ================================================================
// 类型种类信息表 / Type Kind Info Table
// ================================================================

/** 类型种类详细信息表，对应 C 头文件中的 TypeKind 枚举 */
export const TYPE_KIND_INFO: Record<TypeKindUI, TypeKindInfo> = {
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
// ================================================================

/** 默认类型推断规则，对应 type_system.h 中的 TypeInferenceRule 结构 */
export const DEFAULT_INFERENCE_RULES: InferenceRuleUI[] = [
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

/** 演示用等价路径 */
export const DEMO_EQUIV_PATHS: TypeEquivPath[] = [
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
// 类型层级树构建 / Type Hierarchy Tree Builder
// ================================================================

/**
 * 构建完整的类型层级树。
 * 包含宇宙层级节点、当前几何状态节点、推断规则节点和等价路径节点。
 *
 * @param pointsCount - 当前点数
 * @param segmentsCount - 当前线段数
 * @param constraintsCount - 当前约束数
 * @param regionsCount - 当前区域数
 * @param portsCount - 当前端口数
 * @param funcBlocksCount - 当前函数块数
 * @param inferenceRules - 推断规则列表
 * @param equivPaths - 等价路径列表
 * @returns 类型层级树节点数组
 */
export function buildTypeTree(
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
    const levelKinds = Object.values(TYPE_KIND_INFO).filter((info) => info.universeLevel === ul.level);

    const levelNode: TreeNode = {
      id: `universe-${ul.level}`,
      label: ul.name,
      labelZh: ul.nameZh,
      universeLevel: ul.level,
      color: ul.color,
      expanded: ul.level <= 1,
      children: [],
    };

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

      if (kindInfo.children && kindInfo.children.length > 0) {
        for (const childKind of kindInfo.children) {
          const childInfo = TYPE_KIND_INFO[childKind];
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

  // 当前几何状态节点
  const currentGeoNode: TreeNode = {
    id: 'current-geo',
    label: 'CURRENT GEOMETRY',
    labelZh: '当前几何状态',
    universeLevel: -1,
    color: 'var(--color-text-primary, #e0e0e0)',
    expanded: true,
    children: [],
  };

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
    currentGeoNode.children.push({
      id: 'geo-segments',
      label: `Segments (${segmentsCount})`,
      labelZh: `线段 (${segmentsCount})`,
      universeLevel: 0,
      color: getUniverseColor(0),
      expanded: false,
      children: [],
      data: { kind: 'line_segment', segmentCount: segmentsCount },
    });
  }

  if (constraintsCount > 0) {
    currentGeoNode.children.push({
      id: 'geo-constraints',
      label: `Constraints (${constraintsCount})`,
      labelZh: `约束 (${constraintsCount})`,
      universeLevel: 1,
      color: getUniverseColor(1),
      expanded: false,
      children: [],
    });
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

  // 推断规则节点
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

  // 等价路径节点
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
