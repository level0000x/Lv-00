/**
 * @module utils/penroseRenderer
 * @description Penrose 风格自动可视化渲染器。
 *              借鉴 Penrose (https://penrose.cs.cmu.edu/) 的 Domain-Substance-Style
 *              三层分离架构，在 Lv-00 几何系统中实现"用代码描述数学关系，
 *              自动生成可视化"的叙事方式。
 *
 *              核心组件：
 *              1. Domain（领域模式）—— 声明几何类型和谓词
 *              2. Substance（物质程序）—— 编码几何事实和关系
 *              3. Style（样式模式）—— 将几何实体映射到视觉样式
 *              4. AutoLayout —— 力导向自动布局引擎
 *              5. SVG Generator —— 生成完整 SVG 标记
 */

import type { Point, Segment, Constraint } from '@/types';

// ================================================================
// 第一部分：领域模式定义
// ================================================================

/**
 * 几何类型定义 —— 领域中的实体类别。
 * 每个类型描述该几何实体的形状、维度等信息。
 */
export interface PenroseType {
  /** 类型标识符 */
  name: string;
  /** 类型描述 */
  description: string;
  /** 维度：0=点, 1=线, 2=区域 */
  dimension: number;
  /** 该类型实体是否可拖动 */
  draggable: boolean;
  /** 默认可视化形状 */
  defaultShape: 'circle' | 'square' | 'line' | 'label' | 'arrow' | 'region';
}

/**
 * 谓词定义 —— 描述几何实体之间的约束关系。
 * 每个谓词有一个名称和支持的参数类型列表（按索引对应）。
 */
export interface PenrosePredicate {
  /** 谓词标识符 */
  name: string;
  /** 谓词描述 */
  description: string;
  /** 参数类型列表（按顺序对应） */
  argTypes: string[];
  /** 是否对称（参数顺序无关） */
  symmetric: boolean;
}

/**
 * 领域模式 —— 定义整个几何可视化领域的完整类型系统。
 * 包含所有合法类型和谓词，是 Substance 和 Style 的"语法"基础。
 */
export interface PenroseDomain {
  /** 领域名称 */
  name: string;
  /** 领域描述 */
  description: string;
  /** 该领域所有类型定义 */
  types: { [name: string]: PenroseType };
  /** 该领域所有谓词定义 */
  predicates: { [name: string]: PenrosePredicate };
}

// ================================================================
// 第二部分：物质程序
// ================================================================

/**
 * 对象声明 —— 声明一个具有特定类型的几何实体。
 */
export interface PenroseObject {
  /** 类型名称（必须在 Domain 中定义） */
  type: string;
  /** 对象名称（可读标识符） */
  name: string;
}

/**
 * 语句 —— 描述几何实体之间的关系。
 */
export interface PenroseStatement {
  /** 谓词名称（必须在 Domain 中定义） */
  predicate: string;
  /** 参数的对象名称列表 */
  args: string[];
}

/**
 * 物质程序 —— 编码特定几何场景中的事实。
 * 声明所有存在的几何实体以及它们之间的关系。
 */
export interface PenroseSubstance {
  /** 场景名称 */
  name: string;
  /** 来自 Lv-00 的点列表 */
  points: Point[];
  /** 来自 Lv-00 的线段列表 */
  segments: Segment[];
  /** 来自 Lv-00 的约束列表 */
  constraints: Constraint[];
  /** 映射表：Lv-00 Point ID -> 对象名称 */
  pointNameMap: Map<number, string>;
  /** 声明的对象列表（自动生成） */
  objects: PenroseObject[];
  /** 声明的语句列表（自动生成） */
  statements: PenroseStatement[];
}

// ================================================================
// 第三部分：样式模式
// ================================================================

/**
 * 视觉样式 —— 定义几何类型的渲染属性。
 */
export interface VisualStyle {
  /** 形状：圆形、方形、线段、标签、箭头、区域 */
  shape: 'circle' | 'square' | 'line' | 'label' | 'arrow' | 'region';
  /** 填充色（十六进制） */
  color: string;
  /** 尺寸（半径/边长/线宽，像素） */
  size: number;
  /** 描边宽度（像素） */
  strokeWidth: number;
  /** 填充不透明度 0-1 */
  fillOpacity?: number;
  /** 描边颜色 */
  strokeColor?: string;
  /** 是否显示标签 */
  label?: boolean;
  /** 字体大小（仅标签类型） */
  fontSize?: number;
  /** 字体族 */
  fontFamily?: string;
}

/**
 * 样式模式 —— 将几何类型和谓词映射到视觉样式。
 * 还包含画布尺寸定义。
 */
export interface PenroseStyle {
  /** 画布尺寸 */
  canvas: {
    width: number;
    height: number;
  };
  /** 背景色 */
  background?: string;
  /** 类型到视觉样式的映射 */
  typeStyles: { [type: string]: VisualStyle };
  /** 谓词到视觉样式的映射 */
  predicateStyles: { [pred: string]: VisualStyle };
}

/**
 * 定位后的对象 —— 包含布局计算后的坐标。
 */
export interface PositionedObject {
  /** 对象名称 */
  name: string;
  /** 类型名称 */
  type: string;
  /** 布局后的 X 坐标 */
  x: number;
  /** 布局后的 Y 坐标 */
  y: number;
  /** 对象半径（用于力导向避碰） */
  radius: number;
  /** 是否固定位置（锚点） */
  fixed: boolean;
  /** 关联的视觉样式 */
  style: VisualStyle;
}

/**
 * 边 —— 对象之间的连接关系，用于力导向计算。
 */
interface LayoutEdge {
  source: string;
  target: string;
  /** 边的谓词名称（决定视觉样式） */
  predicate: string;
  /** 理想长度（像素） */
  idealLength: number;
  /** 弹簧强度（0-1） */
  strength: number;
}

// ================================================================
// 第四部分：预构建领域
// ================================================================

/**
 * 欧几里得几何领域 —— 处理平面几何中的点、线段、约束关系。
 */
export const euclideanGeometryDomain: PenroseDomain = {
  name: 'EuclideanGeometry',
  description: '欧几里得平面几何领域 — 点、线段及其约束关系的可视化',
  types: {
    Point: {
      name: 'Point',
      description: '二维欧几里得空间中的一个点',
      dimension: 0,
      draggable: true,
      defaultShape: 'circle',
    },
    Segment: {
      name: 'Segment',
      description: '连接两个点的线段',
      dimension: 1,
      draggable: false,
      defaultShape: 'line',
    },
    IntersectionPoint: {
      name: 'IntersectionPoint',
      description: '两条线段的交点',
      dimension: 0,
      draggable: false,
      defaultShape: 'circle',
      },
    ConstrainedPoint: {
      name: 'ConstrainedPoint',
      description: '受约束条件限制的点',
      dimension: 0,
      draggable: false,
      defaultShape: 'square',
    },
  },
  predicates: {
    incidence: {
      name: 'incidence',
      description: '点在线上',
      argTypes: ['Point', 'Segment'],
      symmetric: false,
    },
    betweenness: {
      name: 'betweenness',
      description: 'B 在 A 和 C 之间',
      argTypes: ['Point', 'Point', 'Point'],
      symmetric: false,
    },
    intersection: {
      name: 'intersection',
      description: '两线段相交于一点',
      argTypes: ['IntersectionPoint', 'Segment', 'Segment'],
      symmetric: true,
    },
    connection: {
      name: 'connection',
      description: '两点之间的连接关系',
      argTypes: ['Point', 'Point'],
      symmetric: true,
    },
    containment: {
      name: 'containment',
      description: '点/区域包含于另一区域',
      argTypes: ['Point', 'Point', 'Point'],
      symmetric: false,
    },
  },
};

/**
 * 解析几何领域 —— 处理坐标系中的点、曲线和方程关系。
 */
export const analyticGeometryDomain: PenroseDomain = {
  name: 'AnalyticGeometry',
  description: '解析几何领域 — 坐标系中点、曲线、方程关系的可视化',
  types: {
    CoordinatePoint: {
      name: 'CoordinatePoint',
      description: '坐标系中的一个点（具有 (x, y) 坐标）',
      dimension: 0,
      draggable: true,
      defaultShape: 'circle',
    },
    CurveSegment: {
      name: 'CurveSegment',
      description: '坐标空间中的曲线段',
      dimension: 1,
      draggable: false,
      defaultShape: 'line',
    },
    FunctionGraph: {
      name: 'FunctionGraph',
      description: '函数图像的图形表示',
      dimension: 1,
      draggable: false,
      defaultShape: 'line',
    },
    BoundingBox: {
      name: 'BoundingBox',
      description: '包围盒区域',
      dimension: 2,
      draggable: false,
      defaultShape: 'region',
    },
  },
  predicates: {
    passesThrough: {
      name: 'passesThrough',
      description: '曲线通过某点',
      argTypes: ['CurveSegment', 'CoordinatePoint'],
      symmetric: false,
    },
    tangent: {
      name: 'tangent',
      description: '两曲线在某点相切',
      argTypes: ['CurveSegment', 'CurveSegment', 'CoordinatePoint'],
      symmetric: true,
    },
    inside: {
      name: 'inside',
      description: '点在区域内',
      argTypes: ['CoordinatePoint', 'BoundingBox'],
      symmetric: false,
    },
    collinear: {
      name: 'collinear',
      description: '三点共线',
      argTypes: ['CoordinatePoint', 'CoordinatePoint', 'CoordinatePoint'],
      symmetric: true,
    },
  },
};

/**
 * 约束图领域 —— 将 Lv-00 约束图直接映射为可视化实体。
 * 这是连接 Lv-00 核心引擎与 Penrose 可视化系统的关键桥接领域。
 */
export const constraintGraphDomain: PenroseDomain = {
  name: 'ConstraintGraph',
  description: '约束图领域 — 将 Lv-00 约束图直接映射为可视化实体',
  types: {
    GraphNode: {
      name: 'GraphNode',
      description: '约束图中的一个节点（代表 Lv-00 中的点）',
      dimension: 0,
      draggable: true,
      defaultShape: 'circle',
    },
    GraphEdge: {
      name: 'GraphEdge',
      description: '约束图中的一条边（代表 Lv-00 中的线段或对偶边）',
      dimension: 1,
      draggable: false,
      defaultShape: 'line',
    },
    ConstraintNode: {
      name: 'ConstraintNode',
      description: '约束图中的约束节点',
      dimension: 0,
      draggable: true,
      defaultShape: 'square',
    },
    RegionNode: {
      name: 'RegionNode',
      description: '约束图中的区域节点',
      dimension: 2,
      draggable: false,
      defaultShape: 'region',
    },
  },
  predicates: {
    incident: {
      name: 'incident',
      description: '节点关联于边',
      argTypes: ['GraphNode', 'GraphEdge'],
      symmetric: false,
    },
    constrained: {
      name: 'constrained',
      description: '节点受约束限制',
      argTypes: ['GraphNode', 'ConstraintNode'],
      symmetric: false,
    },
    adjacent: {
      name: 'adjacent',
      description: '两个节点相邻',
      argTypes: ['GraphNode', 'GraphNode'],
      symmetric: true,
    },
    dependsOn: {
      name: 'dependsOn',
      description: '约束依赖于节点',
      argTypes: ['ConstraintNode', 'GraphNode'],
      symmetric: false,
    },
  },
};

/** 所有预构建领域的集合 */
export const ALL_DOMAINS: { [key: string]: PenroseDomain } = {
  euclideanGeometry: euclideanGeometryDomain,
  analyticGeometry: analyticGeometryDomain,
  constraintGraph: constraintGraphDomain,
};

// ================================================================
// 第五部分：力导向自动布局引擎
// ================================================================

/**
 * 力导向布局的配置参数。
 */
interface LayoutConfig {
  /** 排斥力常数 */
  repulsion: number;
  /** 吸引力常数（弹簧系数） */
  attraction: number;
  /** 阻尼系数（速度衰减） */
  damping: number;
  /** 最大迭代次数 */
  maxIterations: number;
  /** 收敛阈值（平均速度平方和） */
  convergenceThreshold: number;
  /** 最大速度限制 */
  maxVelocity: number;
  /** 画布边距 */
  margin: number;
}

/** 默认布局配置 */
const DEFAULT_LAYOUT_CONFIG: LayoutConfig = {
  repulsion: 5000,
  attraction: 0.01,
  damping: 0.85,
  maxIterations: 100,
  convergenceThreshold: 0.001,
  maxVelocity: 50,
  margin: 40,
};

/**
 * 从 Lv-00 几何数据构建 Substance 程序。
 * 将原始的 Point、Segment、Constraint 数据转换为 Penrose 的
 * 声明式几何事实表示。
 *
 * @param points - Lv-00 中的点列表
 * @param segments - Lv-00 中的线段列表
 * @param constraints - Lv-00 中的约束列表
 * @param domain - 目标领域（默认使用 euclideanGeometry）
 * @returns 构建好的物质程序
 */
export function buildSubstance(
  points: Point[],
  segments: Segment[],
  constraints: Constraint[],
  _domain: PenroseDomain = euclideanGeometryDomain,
): PenroseSubstance {
  // NOTE: _domain is reserved for future multi-domain support.
  // Currently, the function hardcodes euclidean geometry type names.
  // When additional domains are supported, this parameter will be used
  // to select domain-specific type names and style mappings.
  const pointNameMap = new Map<number, string>();
  const objects: PenroseObject[] = [];
  const statements: PenroseStatement[] = [];

  // 声明所有点为 Point 类型
  for (const pt of points) {
    const name = `P${pt.id}`;
    pointNameMap.set(pt.id, name);
    objects.push({ type: 'Point', name });
  }

  // 声明所有线段为 Segment 类型，并添加 incidence 语句
  for (const seg of segments) {
    const segName = `S${seg.id}`;
    objects.push({ type: 'Segment', name: segName });

    const p1Name = pointNameMap.get(seg.p1);
    const p2Name = pointNameMap.get(seg.p2);
    if (p1Name) {
      statements.push({ predicate: 'incidence', args: [p1Name, segName] });
    }
    if (p2Name) {
      statements.push({ predicate: 'incidence', args: [p2Name, segName] });
    }
  }

  // 解析约束并生成语句
  for (const con of constraints) {
    switch (con.type) {
      case 'incidence': {
        // args: [pointId, segmentId]
        if (con.args.length >= 2) {
          const ptName = pointNameMap.get(con.args[0]!);
          const segName = `S${con.args[1]}`;
          if (ptName) {
            statements.push({ predicate: 'incidence', args: [ptName, segName] });
          }
        }
        break;
      }
      case 'betweenness': {
        // args: [A_id, B_id, C_id]
        if (con.args.length >= 3) {
          const aName = pointNameMap.get(con.args[0]!);
          const bName = pointNameMap.get(con.args[1]!);
          const cName = pointNameMap.get(con.args[2]!);
          if (aName && bName && cName) {
            statements.push({ predicate: 'betweenness', args: [aName, bName, cName] });
          }
        }
        break;
      }
      case 'intersection': {
        // args: [intersectionPointId, seg1Id, seg2Id, ...]
        if (con.args.length >= 3) {
          const intName = pointNameMap.get(con.args[0]!);
          const seg1Name = `S${con.args[1]}`;
          const seg2Name = `S${con.args[2]}`;
          if (intName) {
            statements.push({ predicate: 'intersection', args: [intName, seg1Name, seg2Name] });
            // 自动将交点标记为不同样式
            const existing = objects.find((o) => o.name === intName);
            if (existing) {
              existing.type = 'IntersectionPoint';
            }
          }
        }
        break;
      }
      case 'connection': {
        // args: [a_id, b_id, ...]
        if (con.args.length >= 2) {
          const aName = pointNameMap.get(con.args[0]!);
          const bName = pointNameMap.get(con.args[1]!);
          if (aName && bName) {
            statements.push({ predicate: 'connection', args: [aName, bName] });
          }
        }
        break;
      }
      case 'containment': {
        // args: [containedId, containerId, ...]
        if (con.args.length >= 3) {
          const ptnName: string[] = [];
          for (const arg of con.args) {
            const n = pointNameMap.get(arg);
            if (n) ptnName.push(n);
          }
          if (ptnName.length >= 3) {
            statements.push({ predicate: 'containment', args: ptnName });
          }
        }
        break;
      }
    }
  }

  return {
    name: 'Lv-00 Auto-generated Scene',
    points,
    segments,
    constraints,
    pointNameMap,
    objects,
    statements,
  };
}

/**
 * 计算力导向自动布局。
 * 使用标准力导向图布局算法：
 * - 节点之间相互排斥（库仑力）
 * - 有连接关系的节点相互吸引（弹簧力）
 * - 通过迭代收敛到稳定布局
 *
 * @param substance - 物质程序
 * @param style - 样式模式
 * @param config - 布局配置（可选）
 * @returns 定位后的对象数组
 */
export function computeLayout(
  substance: PenroseSubstance,
  style: PenroseStyle,
  config?: Partial<LayoutConfig>,
): PositionedObject[] {
  const cfg: LayoutConfig = { ...DEFAULT_LAYOUT_CONFIG, ...config };
  const { width, height } = style.canvas;
  const centerX = width / 2;
  const centerY = height / 2;

  // 为每个对象分配初始位置
  interface LayoutState {
    x: number;
    y: number;
    vx: number;
    vy: number;
    fixed: boolean;
    radius: number;
  }

  const stateMap = new Map<string, LayoutState>();
  const positioned: PositionedObject[] = [];

  for (const obj of substance.objects) {
    const vs: VisualStyle =
      style.typeStyles[obj.type] ??
      defaultVisualStyle(obj.type);

    let startX: number;
    let startY: number;
    let fixed = false;

    // 如果有对应的 Lv-00 点数据，使用实际坐标
    const match = obj.name.match(/^P(\d+)$/);
    if (match) {
      const ptId = parseInt(match[1]!, 10);
      const pt = substance.points.find((p) => p.id === ptId);
      if (pt) {
        startX = pt.x;
        startY = pt.y;
        fixed = true; // Lv-00 点使用实际坐标，固定不动
      } else {
        startX = centerX + (Math.random() - 0.5) * 200;
        startY = centerY + (Math.random() - 0.5) * 200;
      }
    } else {
      // Segment 等对象，放到点之间的位置
      startX = centerX + (Math.random() - 0.5) * 200;
      startY = centerY + (Math.random() - 0.5) * 200;
    }

    stateMap.set(obj.name, {
      x: startX,
      y: startY,
      vx: 0,
      vy: 0,
      fixed,
      radius: vs.size + 5,
    });

    positioned.push({
      name: obj.name,
      type: obj.type,
      x: startX,
      y: startY,
      radius: vs.size + 5,
      fixed,
      style: vs,
    });
  }

  // 构建边列表（来自 statements）
  const edges: LayoutEdge[] = [];
  for (const stmt of substance.statements) {
    if (stmt.args.length >= 2) {
      // 取前两个参数构建边
      edges.push({
        source: stmt.args[0]!,
        target: stmt.args[1]!,
        predicate: stmt.predicate,
        idealLength: 100,
        strength: 0.5,
      });
    }
  }

  // 为 Segment 类型对象计算其端点之间的中点作为初始位置
  for (let i = 0; i < positioned.length; i++) {
    const pos = positioned[i]!;
    if (pos.type === 'Segment' && !pos.fixed) {
      const segMatch = pos.name.match(/^S(\d+)$/);
      if (segMatch) {
        const segId = parseInt(segMatch[1]!, 10);
        const seg = substance.segments.find((s) => s.id === segId);
        if (seg) {
          const p1Name = substance.pointNameMap.get(seg.p1);
          const p2Name = substance.pointNameMap.get(seg.p2);
          const s1 = p1Name ? stateMap.get(p1Name) : null;
          const s2 = p2Name ? stateMap.get(p2Name) : null;
          if (s1 && s2) {
            pos.x = (s1.x + s2.x) / 2;
            pos.y = (s1.y + s2.y) / 2;
            const ls = stateMap.get(pos.name);
            if (ls) {
              ls.x = pos.x;
              ls.y = pos.y;
            }
          }
        }
      }
    }
  }

  // 力导向迭代
  for (let iter = 0; iter < cfg.maxIterations; iter++) {
    // 初始化力向量
    const forcesX = new Map<string, number>();
    const forcesY = new Map<string, number>();
    for (const [name] of stateMap) {
      forcesX.set(name, 0);
      forcesY.set(name, 0);
    }

    // 1. 节点间排斥力（库仑力）: F = k / d^2
    const names = Array.from(stateMap.keys());
    for (let i = 0; i < names.length; i++) {
      for (let j = i + 1; j < names.length; j++) {
        const si = stateMap.get(names[i]!)!;
        const sj = stateMap.get(names[j]!)!;

        const dx = si.x - sj.x;
        const dy = si.y - sj.y;
        let dist = Math.sqrt(dx * dx + dy * dy);
        if (dist < 1) dist = 1;

        const repulsionForce = cfg.repulsion / (dist * dist);
        const fx = (dx / dist) * repulsionForce;
        const fy = (dy / dist) * repulsionForce;

        if (!si.fixed) {
          forcesX.set(names[i]!, (forcesX.get(names[i]!) ?? 0) + fx);
          forcesY.set(names[i]!, (forcesY.get(names[i]!) ?? 0) + fy);
        }
        if (!sj.fixed) {
          forcesX.set(names[j]!, (forcesX.get(names[j]!) ?? 0) - fx);
          forcesY.set(names[j]!, (forcesY.get(names[j]!) ?? 0) - fy);
        }
      }
    }

    // 2. 边吸引力（弹簧力）: F = -k * (d - idealLength)
    for (const edge of edges) {
      const si = stateMap.get(edge.source);
      const sj = stateMap.get(edge.target);
      if (!si || !sj) continue;

      const dx = si.x - sj.x;
      const dy = si.y - sj.y;
      const dist = Math.sqrt(dx * dx + dy * dy) || 1;
      const displacement = dist - edge.idealLength;
      const springForce = cfg.attraction * displacement * edge.strength;

      const fx = (dx / dist) * springForce;
      const fy = (dy / dist) * springForce;

      if (!si.fixed) {
        forcesX.set(edge.source, (forcesX.get(edge.source) ?? 0) - fx);
        forcesY.set(edge.source, (forcesY.get(edge.source) ?? 0) - fy);
      }
      if (!sj.fixed) {
        forcesX.set(edge.target, (forcesX.get(edge.target) ?? 0) + fx);
        forcesY.set(edge.target, (forcesY.get(edge.target) ?? 0) + fy);
      }
    }

    // 3. 中心引力（防止漂移）
    for (const [name, st] of stateMap) {
      if (st.fixed) continue;
      const dx = st.x - centerX;
      const dy = st.y - centerY;
      const centeringForce = 0.001;
      const fx = forcesX.get(name) ?? 0;
      const fy = forcesY.get(name) ?? 0;
      forcesX.set(name, fx - dx * centeringForce);
      forcesY.set(name, fy - dy * centeringForce);
    }

    // 4. 更新速度和位置
    let totalVelocitySquared = 0;
    for (const [name, st] of stateMap) {
      if (st.fixed) continue;

      const fx = forcesX.get(name) ?? 0;
      const fy = forcesY.get(name) ?? 0;

      st.vx = (st.vx + fx) * cfg.damping;
      st.vy = (st.vy + fy) * cfg.damping;

      // 限幅
      const speed = Math.sqrt(st.vx * st.vx + st.vy * st.vy);
      if (speed > cfg.maxVelocity) {
        st.vx = (st.vx / speed) * cfg.maxVelocity;
        st.vy = (st.vy / speed) * cfg.maxVelocity;
      }

      st.x += st.vx;
      st.y += st.vy;

      // 边界约束
      st.x = Math.max(cfg.margin, Math.min(width - cfg.margin, st.x));
      st.y = Math.max(cfg.margin, Math.min(height - cfg.margin, st.y));

      totalVelocitySquared += st.vx * st.vx + st.vy * st.vy;
    }

    // 收敛检测
    const avgVelocitySquared = totalVelocitySquared / stateMap.size;
    if (avgVelocitySquared < cfg.convergenceThreshold && iter > 20) {
      break;
    }
  }

  // 更新 PositionedObject 坐标
  for (const pos of positioned) {
    const st = stateMap.get(pos.name);
    if (st) {
      pos.x = st.x;
      pos.y = st.y;
    }
  }

  return positioned;
}

/**
 * 获取默认的视觉样式。
 * 当用户未为某类型显式指定样式时使用。
 */
function defaultVisualStyle(type: string): VisualStyle {
  const defaults: Record<string, VisualStyle> = {
    Point: {
      shape: 'circle',
      color: '#4caf50',
      size: 5,
      strokeWidth: 2,
      fillOpacity: 0.8,
    },
    Segment: {
      shape: 'line',
      color: '#555555',
      size: 2,
      strokeWidth: 2,
    },
    IntersectionPoint: {
      shape: 'circle',
      color: '#ff9800',
      size: 6,
      strokeWidth: 2,
      strokeColor: '#e65100',
    },
    ConstrainedPoint: {
      shape: 'square',
      color: '#ab47bc',
      size: 6,
      strokeWidth: 2,
    },
    GraphNode: {
      shape: 'circle',
      color: '#4caf50',
      size: 6,
      strokeWidth: 2,
      fillOpacity: 0.8,
    },
    GraphEdge: {
      shape: 'line',
      color: '#607d8b',
      size: 2,
      strokeWidth: 2,
    },
    ConstraintNode: {
      shape: 'square',
      color: '#e91e63',
      size: 8,
      strokeWidth: 2,
    },
    RegionNode: {
      shape: 'region',
      color: 'rgba(33, 150, 243, 0.15)',
      size: 1,
      strokeWidth: 1,
      strokeColor: '#2196f3',
      fillOpacity: 0.15,
    },
  };

  return defaults[type] ?? {
    shape: 'circle',
    color: '#888888',
    size: 4,
    strokeWidth: 1,
  };
}

// ================================================================
// 第六部分：SVG 生成器
// ================================================================

/**
 * 将布局后的对象渲染为完整的 SVG 标记字符串。
 * SVG 可直接嵌入 HTML 或作为独立的 .svg 文件保存。
 *
 * @param positioned - 定位后的对象数组
 * @param style - 样式模式
 * @param substance - 物质程序（用于线段/箭头端点查找）
 * @returns 完整的 SVG 字符串
 */
export function renderToSVG(
  positioned: PositionedObject[],
  style: PenroseStyle,
  substance?: PenroseSubstance,
): string {
  const { width, height } = style.canvas;
  const bg = style.background ?? '#080808';

  const lines: string[] = [];

  // SVG 头部
  lines.push(
    `<?xml version="1.0" encoding="UTF-8"?>`,
    `<svg xmlns="http://www.w3.org/2000/svg"`,
    `  viewBox="0 0 ${width} ${height}"`,
    `  width="${width}" height="${height}">`,
    ``,
    `  <!-- ${style.canvas.width}x${style.canvas.height} -- Penrose-style auto-generated diagram -->`,
    `  <rect width="${width}" height="${height}" fill="${bg}" rx="4" />`,
    ``,
  );

  // 构建名称索引
  const index = new Map<string, PositionedObject>();
  for (const pos of positioned) {
    index.set(pos.name, pos);
  }

  // 渲染线段和箭头（先渲染，再渲染点以覆盖）
  for (const pos of positioned) {
    const typeStyle = pos.style;

    if (typeStyle.shape === 'line' && substance) {
      // 查找该 Segment 的两个端点
      const seg = substance.segments.find(
        (s) => `S${s.id}` === pos.name,
      );
      if (seg) {
        const p1Name = substance.pointNameMap.get(seg.p1);
        const p2Name = substance.pointNameMap.get(seg.p2);
        const pp1 = p1Name ? index.get(p1Name) : null;
        const pp2 = p2Name ? index.get(p2Name) : null;

        if (pp1 && pp2) {
          const sc = typeStyle.strokeColor ?? typeStyle.color;
          lines.push(`  <!-- Segment ${pos.name} -->`);
          lines.push(
            `  <line x1="${pp1.x}" y1="${pp1.y}"`,
            `        x2="${pp2.x}" y2="${pp2.y}"`,
            `        stroke="${sc}"`,
            `        stroke-width="${typeStyle.strokeWidth}"`,
            `        stroke-linecap="round" />`,
          );
        } else {
          // 没有端点坐标时，仍然绘制假想的线段（使用初始布局坐标）
          lines.push(`  <!-- Segment ${pos.name} (no endpoint data) -->`);
          lines.push(
            `  <line x1="${pos.x - 40}" y1="${pos.y - 20}"`,
            `        x2="${pos.x + 40}" y2="${pos.y + 20}"`,
            `        stroke="${typeStyle.color}"`,
            `        stroke-width="1"`,
            `        stroke-dasharray="4 4"`,
            `        opacity="0.3" />`,
          );
        }
      }
    }

    if (typeStyle.shape === 'arrow' && substance) {
      // 绘制箭头（从 source 到 target）
      const arrowMatch = pos.name.match(/^Arrow_(\w+)_(\w+)$/);
      if (arrowMatch) {
        const srcObj = index.get(arrowMatch[1]!);
        const tgtObj = index.get(arrowMatch[2]!);
        if (srcObj && tgtObj) {
          const sc = typeStyle.strokeColor ?? typeStyle.color;
          lines.push(`  <!-- Arrow ${pos.name} -->`);
          lines.push(
            `  <line x1="${srcObj.x}" y1="${srcObj.y}"`,
            `        x2="${tgtObj.x}" y2="${tgtObj.y}"`,
            `        stroke="${sc}"`,
            `        stroke-width="${typeStyle.strokeWidth}"`,
            `        marker-end="url(#arrowhead)" />`,
          );
        }
      }
    }
  }

  // 渲染区域（多边形）
  for (const pos of positioned) {
    if (pos.style.shape === 'region') {
      const sc = pos.style.strokeColor ?? pos.style.color;
      const fo = pos.style.fillOpacity ?? 0.1;
      lines.push(
        `  <!-- Region ${pos.name} -->`,
        `  <rect x="${pos.x - 50}" y="${pos.y - 50}"`,
        `        width="100" height="100" rx="8"`,
        `        fill="${pos.style.color}"`,
        `        fill-opacity="${fo}"`,
        `        stroke="${sc}"`,
        `        stroke-width="${pos.style.strokeWidth}"`,
        `        stroke-dasharray="6 3" />`,
      );
    }
  }

  // 渲染点和标签
  for (const pos of positioned) {
    const typeStyle = pos.style;

    if (typeStyle.shape === 'circle') {
      const sc = typeStyle.strokeColor ?? typeStyle.color;
      const fo = typeStyle.fillOpacity ?? 0.8;

      lines.push(`  <!-- ${pos.type} ${pos.name} -->`);
      lines.push(
        `  <circle cx="${pos.x}" cy="${pos.y}"`,
        `          r="${typeStyle.size}"`,
        `          fill="${typeStyle.color}"`,
        `          fill-opacity="${fo}"`,
        `          stroke="${sc}"`,
        `          stroke-width="${typeStyle.strokeWidth}" />`,
      );

      if (typeStyle.label !== false) {
        const fs = typeStyle.fontSize ?? 11;
        const ff = typeStyle.fontFamily ?? 'monospace';
        lines.push(
          `  <text x="${pos.x + typeStyle.size + 6}"`,
          `        y="${pos.y + fs / 3}"`,
          `        font-family="${ff}"`,
          `        font-size="${fs}px"`,
          `        fill="${sc}">${pos.name}</text>`,
        );
      }
    }

    if (typeStyle.shape === 'square') {
      const sc = typeStyle.strokeColor ?? typeStyle.color;
      const fo = typeStyle.fillOpacity ?? 0.8;
      const s = typeStyle.size;
      const hs = s / 2;

      lines.push(`  <!-- ${pos.type} ${pos.name} -->`);
      lines.push(
        `  <rect x="${pos.x - hs}" y="${pos.y - hs}"`,
        `        width="${s}" height="${s}" rx="2"`,
        `        fill="${typeStyle.color}"`,
        `        fill-opacity="${fo}"`,
        `        stroke="${sc}"`,
        `        stroke-width="${typeStyle.strokeWidth}" />`,
      );

      if (typeStyle.label !== false) {
        const fs = typeStyle.fontSize ?? 11;
        const ff = typeStyle.fontFamily ?? 'monospace';
        lines.push(
          `  <text x="${pos.x + hs + 6}"`,
          `        y="${pos.y + fs / 3}"`,
          `        font-family="${ff}"`,
          `        font-size="${fs}px"`,
          `        fill="${sc}">${pos.name}</text>`,
        );
      }
    }
  }

  // 渲染约束谓词样式（虚线标注）
  if (substance) {
    for (const pos of positioned) {
      // 查找与 betweenness constraint 相关的标记
      const btStmts = substance.statements.filter(
        (s) =>
          s.predicate === 'betweenness' &&
          (s.args[0] === pos.name ||
            s.args[1] === pos.name ||
            s.args[2] === pos.name),
      );

      if (btStmts.length > 0 && pos.style.shape !== 'line') {
        for (const stmt of btStmts) {
          const a = index.get(stmt.args[0]!);
          const b = index.get(stmt.args[1]!);
          const c = index.get(stmt.args[2]!);
          if (a && b && c) {
            lines.push(
              `  <!-- Betweenness: ${stmt.args[0]} — ${stmt.args[1]} — ${stmt.args[2]} -->`,
              `  <line x1="${a.x}" y1="${a.y}"`,
              `        x2="${c.x}" y2="${c.y}"`,
              `        stroke="#ab47bc"`,
              `        stroke-width="1"`,
              `        stroke-dasharray="4 4"`,
              `        opacity="0.5" />`,
            );
          }
        }
      }
    }

    // 绘制 connection 关系（虚线连接）
    for (const stmt of substance.statements) {
      if (stmt.predicate === 'connection' && stmt.args.length >= 2) {
        const a = index.get(stmt.args[0]!);
        const b = index.get(stmt.args[1]!);
        if (a && b) {
          lines.push(
            `  <!-- Connection: ${stmt.args[0]} — ${stmt.args[1]} -->`,
            `  <line x1="${a.x}" y1="${a.y}"`,
            `        x2="${b.x}" y2="${b.y}"`,
            `        stroke="#78909c"`,
            `        stroke-width="1"`,
            `        stroke-dasharray="2 4"`,
            `        opacity="0.6" />`,
          );
        }
      }
    }
  }

  // 箭头标记定义
  lines.push(
    ``,
    `  <!-- Arrowhead marker definition -->`,
    `  <defs>`,
    `    <marker id="arrowhead"`,
    `            markerWidth="10" markerHeight="7"`,
    `            refX="9" refY="3.5"`,
    `            orient="auto"`,
    `            markerUnits="strokeWidth">`,
    `      <polygon points="0 0, 10 3.5, 0 7"`,
    `               fill="#ffffff" />`,
    `    </marker>`,
    `  </defs>`,
    ``,
  );

  // SVG 尾部
  lines.push(`</svg>`);

  return lines.join('\n');
}

// ================================================================
// 第七部分：集成函数
// ================================================================

/**
 * 预定义的样式模板集合。
 * 用户可以从中选择或自定义。
 */
export const PRESET_STYLES: { [key: string]: PenroseStyle } = {
  /** 暗色主题 —— 适用于 Lv-00 默认暗色界面 */
  dark: {
    canvas: { width: 800, height: 600 },
    background: '#080808',
    typeStyles: {
      Point: {
        shape: 'circle',
        color: '#4caf50',
        size: 5,
        strokeWidth: 2,
        fillOpacity: 0.9,
        label: true,
        fontSize: 11,
        fontFamily: 'Consolas, monospace',
      },
      Segment: {
        shape: 'line',
        color: '#78909c',
        size: 2,
        strokeWidth: 2,
      },
      IntersectionPoint: {
        shape: 'circle',
        color: '#ff9800',
        size: 6,
        strokeWidth: 2,
        strokeColor: '#e65100',
        fillOpacity: 0.9,
        label: true,
        fontSize: 10,
      },
      ConstrainedPoint: {
        shape: 'square',
        color: '#ab47bc',
        size: 6,
        strokeWidth: 2,
        strokeColor: '#7b1fa2',
        label: true,
        fontSize: 10,
      },
    },
    predicateStyles: {
      incidence: {
        shape: 'line',
        color: '#00bcd4',
        size: 1,
        strokeWidth: 1,
      },
      betweenness: {
        shape: 'label',
        color: '#ab47bc',
        size: 1,
        strokeWidth: 1,
        fontSize: 9,
      },
      intersection: {
        shape: 'circle',
        color: '#ff9800',
        size: 4,
        strokeWidth: 1,
      },
      connection: {
        shape: 'line',
        color: '#78909c',
        size: 1,
        strokeWidth: 1,
      },
    },
  },

  /** 亮色主题 */
  light: {
    canvas: { width: 800, height: 600 },
    background: '#fafafa',
    typeStyles: {
      Point: {
        shape: 'circle',
        color: '#2196f3',
        size: 5,
        strokeWidth: 2,
        fillOpacity: 0.9,
        label: true,
        fontSize: 11,
        fontFamily: 'Consolas, monospace',
      },
      Segment: {
        shape: 'line',
        color: '#546e7a',
        size: 2,
        strokeWidth: 2,
      },
      IntersectionPoint: {
        shape: 'circle',
        color: '#ff6d00',
        size: 6,
        strokeWidth: 2,
        strokeColor: '#bf360c',
        fillOpacity: 0.9,
        label: true,
        fontSize: 10,
      },
      ConstrainedPoint: {
        shape: 'square',
        color: '#7b1fa2',
        size: 6,
        strokeWidth: 2,
        strokeColor: '#4a148c',
        label: true,
        fontSize: 10,
      },
    },
    predicateStyles: {
      incidence: {
        shape: 'line',
        color: '#00838f',
        size: 1,
        strokeWidth: 1,
      },
      betweenness: {
        shape: 'label',
        color: '#7b1fa2',
        size: 1,
        strokeWidth: 1,
        fontSize: 9,
      },
      intersection: {
        shape: 'circle',
        color: '#ff6d00',
        size: 4,
        strokeWidth: 1,
      },
      connection: {
        shape: 'line',
        color: '#546e7a',
        size: 1,
        strokeWidth: 1,
      },
    },
  },

  /** 学术论文风格 —— 高对比度，适合导出到论文 */
  academic: {
    canvas: { width: 800, height: 600 },
    background: '#ffffff',
    typeStyles: {
      Point: {
        shape: 'circle',
        color: '#000000',
        size: 4,
        strokeWidth: 1.5,
        fillOpacity: 1,
        label: true,
        fontSize: 12,
        fontFamily: 'Times New Roman, serif',
      },
      Segment: {
        shape: 'line',
        color: '#333333',
        size: 1.5,
        strokeWidth: 1.5,
      },
      IntersectionPoint: {
        shape: 'circle',
        color: '#000000',
        size: 5,
        strokeWidth: 2,
        strokeColor: '#000000',
        fillOpacity: 0,
        label: true,
        fontSize: 11,
      },
      ConstrainedPoint: {
        shape: 'square',
        color: '#000000',
        size: 5,
        strokeWidth: 1.5,
        fillOpacity: 0,
        label: true,
        fontSize: 11,
      },
    },
    predicateStyles: {
      incidence: {
        shape: 'line',
        color: '#000000',
        size: 1,
        strokeWidth: 1,
      },
      betweenness: {
        shape: 'label',
        color: '#333333',
        size: 1,
        strokeWidth: 1,
        fontSize: 10,
      },
      intersection: {
        shape: 'circle',
        color: '#000000',
        size: 3,
        strokeWidth: 1,
      },
      connection: {
        shape: 'line',
        color: '#999999',
        size: 1,
        strokeWidth: 0.5,
      },
    },
  },
};

/**
 * 从当前 Lv-00 几何存储数据中自动生成 Penrose 风格的可视化 SVG。
 *
 * 这是面向终端用户的主要入口函数。它：
 * 1. 从 Lv-00 的点、线段、约束中构建 Substance 程序
 * 2. 应用用户选择（或默认）的样式模式
 * 3. 运行力导向自动布局算法
 * 4. 生成完整的 SVG 标记
 *
 * @param points - Lv-00 中的点列表
 * @param segments - Lv-00 中的线段列表
 * @param constraints - Lv-00 中的约束列表
 * @param style - 可选的样式模式（默认使用 'dark' 预设）
 * @param domain - 可选的目标领域（默认使用 euclideanGeometry）
 * @param layoutConfig - 可选的布局配置
 * @returns 完整的 SVG 字符串
 *
 * @example
 * ```typescript
 * import { generatePenroseDiagram, PRESET_STYLES } from '@/utils/penroseRenderer';
 * import { useGeometryStore } from '@/stores/geometryStore';
 *
 * const { points, segments, constraints } = useGeometryStore.getState();
 * const svg = generatePenroseDiagram(points, segments, constraints, PRESET_STYLES.dark);
 * // 将 svg 保存为文件或嵌入到页面中
 * ```
 */
export function generatePenroseDiagram(
  points: Point[],
  segments: Segment[],
  constraints: Constraint[],
  style?: PenroseStyle,
  domain?: PenroseDomain,
  layoutConfig?: Partial<LayoutConfig>,
): string {
  // 使用默认参数
  const effectiveStyle = (style ?? PRESET_STYLES.dark)!;
  const effectiveDomain = domain ?? euclideanGeometryDomain;

  // 步骤 1: 构建 Substance 程序
  const substance = buildSubstance(points, segments, constraints, effectiveDomain);

  // 步骤 2: 计算自动布局
  const positioned = computeLayout(substance, effectiveStyle, layoutConfig);

  // 步骤 3: 生成 SVG
  const svg = renderToSVG(positioned, effectiveStyle, substance);

  return svg;
}

/**
 * 将生成的 SVG 标记包装为独立 HTML 页面。
 * 方便用户在浏览器中预览或将可视化分享给他人。
 *
 * @param svgMarkup - SVG 标记字符串
 * @param title - 页面标题
 * @returns 完整的 HTML 字符串
 */
export function wrapSVGInHTML(svgMarkup: string, title: string = 'Lv-00 Penrose Diagram'): string {
  return `<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>${title}</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
      background: #111;
    }
    svg { max-width: 100%; height: auto; border-radius: 8px; box-shadow: 0 4px 24px rgba(0,0,0,0.5); }
  </style>
</head>
<body>
${svgMarkup}
</body>
</html>`;
}

/**
 * 导出完整的 Penrose 可视化（包含元数据）为可下载的 Blob。
 *
 * @param svgMarkup - SVG 标记字符串
 * @returns Blob 对象
 */
export function svgToBlob(svgMarkup: string): Blob {
  return new Blob([svgMarkup], { type: 'image/svg+xml;charset=utf-8' });
}

/**
 * 触发浏览器下载 SVG 文件。
 *
 * @param svgMarkup - SVG 标记字符串
 * @param filename - 文件名（不含路径）
 */
export function downloadSVG(svgMarkup: string, filename: string = 'penrose-diagram.svg'): void {
  const blob = svgToBlob(svgMarkup);
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a');
  link.href = url;
  link.download = filename;
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  URL.revokeObjectURL(url);
}
