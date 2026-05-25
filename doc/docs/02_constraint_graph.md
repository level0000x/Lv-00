# 约束图核心 (Constraint Graph Core)

## 模块概述

约束图核心是 Lv-00 的基础数据结构层，负责维护几何体（点、线段、区域、端口、函数块）及其相互关系。所有几何构造都以约束图的形式存储和操作，构成系统的"关联几何引擎"。

## 核心设计原则

1. **五项基本关系**：内核仅维护关联、之间、相交、包含、连接五种基本关系
2. **公理中立**：不内建距离、角度概念，这些由公理系统包定义
3. **端口归属标记**：每个节点维护 namespace_depth、parent_block_id、is_formal_param 三个字段
4. **约束冲突预处理**：添加约束时进行静态冲突检测

## 数据类型定义

### 节点类型枚举

```c
typedef enum {
    GEOM_POINT,          // 点 - 由符号坐标定义，零维
    GEOM_LINE_SEGMENT,   // 线段 - 由两个端点定义，有向
    GEOM_REGION,         // 区域 - 由边界线段序列形成的闭合环
    GEOM_PORT,           // 端口 - 函数块的对外接口
    GEOM_FUNCTION_BLOCK  // 函数块 - 封装内部约束子图的复合节点
} GeomNodeType;
```

### 约束类型枚举

```c
typedef enum {
    CONSTRAINT_INCIDENCE,     // 关联 - 点在线上、点在区域边界
    CONSTRAINT_BETWEENNESS,   // 之间 - 点在线段上、点在两点之间
    CONSTRAINT_INTERSECTION,  // 相交 - 线与线相交、线与区域边界相交
    CONSTRAINT_CONTAINMENT,   // 包含 - 区域包含点、区域包含区域
    CONSTRAINT_CONNECTION     // 连接 - 端口与端口相连（有向）
} ConstraintType;
```

### 几何节点结构

```c
typedef struct GeomNode {
    int id;                          // 全局唯一 ID（自增，永不复用）
    GeomNodeType type;               // 节点类型
    SymbolicCoord **coords;          // 符号坐标数组
    int coord_count;                 // 坐标数量
    
    // 端口归属标记系统
    int namespace_depth;             // 嵌套深度（全局画布为 0）
    int parent_block_id;             // 所属函数块 ID（全局为 -1）
    bool is_formal_param;            // 是否为形式参数（仅 PORT 有效）
    
    // 函数块专用字段
    int *internal_nodes;             // 内部节点 ID 列表（仅 FUNCTION_BLOCK）
    int internal_count;
    int *input_ports;                // 输入端口 ID 列表
    int input_count;
    int *output_ports;               // 输出端口 ID 列表
    int output_count;
    
    TrustColor trust;                // 信任颜色
} GeomNode;
```

### 约束结构

```c
typedef struct Constraint {
    int id;                          // 全局唯一 ID
    ConstraintType type;             // 约束类型
    int *participants;               // 参与者节点 ID 数组
    int participant_count;           // 参与者数量
    int template_id;                 // 模板 ID（若由模板展开）
    void *template_params;           // 模板参数
    bool is_redundant;               // 是否冗余
} Constraint;
```

### 约束图结构

```c
typedef struct ConstraintGraph {
    GeomNode **nodes;                // 节点数组
    int node_count;
    int node_capacity;
    int next_node_id;                // 下一个分配的节点 ID
    
    Constraint **constraints;        // 约束数组
    int constraint_count;
    int constraint_capacity;
    int next_constraint_id;          // 下一个分配的约束 ID
    
    // 快速查找索引
    HashTable *node_index;           // ID -> 节点指针
    HashTable *constraint_index;     // ID -> 约束指针
} ConstraintGraph;
```

## 1. 节点操作

### 添加节点

```c
AddNodeResult graph_add_point(
    ConstraintGraph *graph,
    SymbolicCoord *x,
    SymbolicCoord *y
);

AddNodeResult graph_add_line_segment(
    ConstraintGraph *graph,
    int endpoints[2]                 // 两个端点 ID
);

AddNodeResult graph_add_region(
    ConstraintGraph *graph,
    int *segments,                   // 边界线段 ID 数组
    int segment_count
);

AddNodeResult graph_add_port(
    ConstraintGraph *graph,
    bool is_input                    // 是否为输入端口
);

AddNodeResult graph_add_function_block(
    ConstraintGraph *graph,
    int *internal_nodes,
    int internal_count,
    int *input_ports,
    int input_count,
    int *output_ports,
    int output_count
);
```

**添加节点流程**：
1. 分配全局唯一 ID（自增，永不复用）
2. 填入类型、坐标数组
3. 端口标记字段默认值：
   - `namespace_depth = 0`
   - `parent_block_id = -1`
   - `is_formal_param = false`
4. 添加到图的节点数组

### 删除节点

```c
DeleteResult graph_remove_node(ConstraintGraph *graph, int node_id);
```

**删除规则**：
- 级联删除所有引用该节点的约束
- 对于线段节点，如果它是某区域的边界组成部分，则阻止删除并报错
- 对于函数块节点，先解包内部节点

### 查询节点

```c
GeomNode *graph_get_node(ConstraintGraph *graph, int node_id);
GeomNode **graph_get_nodes_by_type(ConstraintGraph *graph, GeomNodeType type);
GeomNode **graph_get_nodes_by_trust(ConstraintGraph *graph, TrustColor trust);
```

## 2. 约束操作

### 添加约束

```c
AddConstraintResult graph_add_constraint(
    ConstraintGraph *graph,
    ConstraintType type,
    int *participants,
    int participant_count
);
```

**添加约束流程**：
1. **防重复检测**：检查是否已有相同类型且相同参与者集合的约束存在
2. **代数冲突预处理**：
   - 对于 INCIDENCE、BETWEENNESS 和 INTERSECTION
   - 检测是否有矛盾的代数关系（如线段 AB 的长度在两个约束中被赋予不同的值）
   - 若检测到冲突，立即标记约束为红色并向用户报告
3. **添加约束到图**
4. **冗余检测**：检测是否与已有约束线性相关

### 删除约束

```c
DeleteResult graph_remove_constraint(ConstraintGraph *graph, int constraint_id);
```

**删除规则**：只删除约束记录，不删除任何节点。

### 查询约束

```c
Constraint *graph_get_constraint(ConstraintGraph *graph, int constraint_id);
Constraint **graph_get_constraints_by_node(ConstraintGraph *graph, int node_id);
Constraint **graph_get_constraints_by_type(ConstraintGraph *graph, ConstraintType type);
```

## 3. 约束类型详解

### INCIDENCE（关联）

**语义**：
- 点在线上：POINT 在 LINE_SEGMENT 的延长线上（不要求在线段内部）
- 点在区域边界上：POINT 在构成 REGION 边界的某条线段上

**代数表示**：
- 点 P 在线段 AB 的延长线上 → (P - A) × (B - A) = 0（叉积为零，线性方程）

**参与者**：
- 点在线段上：[point_id, line_segment_id]
- 点在区域边界上：[point_id, region_id, boundary_segment_id]

### BETWEENNESS（之间）

**语义**：
- 要求三个共线点 P1、P2、P3，且 P2 严格位于 P1 和 P3 之间
- 也支持"点与端点重合"的边界情形

**代数表示**：
- 不增加独立代数方程，仅用于在多个解中选择符合方向性约束的那个

**参与者**：[p1_id, p2_id, p3_id]

### INTERSECTION（相交）

**语义**：
- 两条线段相交于一点，系统自动生成或识别交点
- 线段与区域边界相交于一点

**代数表示**：
- 两条线段相交 → 参数化后产生线性方程组

**参与者**：
- 线段相交：[line1_id, line2_id, intersection_point_id]
- 线段与区域相交：[line_id, region_id, intersection_point_id]

### CONTAINMENT（包含）

**语义**：
- 点严格在区域内部
- 区域完全包含另一区域

**代数表示**：
- 点在区域内 → 点在区域边界构成的多边形内部（射线法判定）

**参与者**：
- 点在区域内：[point_id, region_id]
- 区域包含区域：[outer_region_id, inner_region_id]

### CONNECTION（连接）

**语义**：
- 端口与端口相连
- 有向：从输出端口连接到输入端口

**类型检查**：
- 连接时自动触发类型兼容检查
- 提取目标输入端口声明的类型区域
- 与源端口输出的类型区域进行类型等价检查

**参与者**：[source_port_id, target_port_id]

## 4. 区域有效性检查

### 创建区域时的验证

```c
RegionValidity check_region_validity(
    ConstraintGraph *graph,
    int *segments,
    int segment_count
);
```

**验证规则**：
1. **闭合环验证**：
   - 每条线段的终点连接下一条线段的起点
   - 最后一条线段的终点连接第一条线段的起点
   - 不闭合则拒绝创建

2. **自交检测**：
   - 检测区域边界是否自相交
   - 自交区域允许但生成警告

3. **简单多边形验证**（可选）：
   - 验证是否为简单多边形（无自交）
   - 验证方向（顺时针/逆时针）

## 5. 约束冲突检测（预处理）

### 简化约束数据库

维护一个简化约束数据库，仅追踪代数方程层面的冲突检测：

```c
typedef struct SimplifiedConstraintDB {
    // 距离约束：两点间距离
    HashTable *distance_constraints;  // (p1_id, p2_id) -> distance_value
    
    // 角度约束：两线段夹角
    HashTable *angle_constraints;     // (line1_id, line2_id) -> angle_value
    
    // 共线约束
    HashTable *collinear_constraints; // (p1_id, p2_id, p3_id) -> true
} SimplifiedConstraintDB;
```

### 冲突检测流程

```c
ConflictReport check_constraint_conflict(
    ConstraintGraph *graph,
    Constraint *new_constraint
);
```

**检测时机**：新约束添加时增量检查。

**冲突类型**：
1. **距离冲突**：同一线段被赋予不同长度
2. **角度冲突**：同一夹角被赋予不同角度值
3. **共线冲突**：三点既被要求共线又被要求不共线

**冲突报告**：
```c
typedef struct ConflictReport {
    bool has_conflict;
    int conflict_count;
    Constraint **conflicting_constraints;  // 冲突约束集合
    int **conflicting_nodes;               // 每组冲突涉及的节点
} ConflictReport;
```

**局限性**：预处理仅检测静态可判定的矛盾，不运行完整求解器。更复杂的间接矛盾由求解器在求解阶段发现。

## 6. 冗余约束检测

### 冗余检测算法

```c
RedundancyReport detect_redundant_constraints(ConstraintGraph *graph);
```

**检测规则**：
- 三点 A、B、C，已知 AB=3，BC=4，新添 AC=7，则新约束冗余且隐含共线性
- 冗余约束自动标记但不删除
- 在界面上以半透明显示

## 7. 端口归属标记系统

### 数据结构

每个节点维护三个轻量级字段：

```c
int namespace_depth;     // 节点直接所属函数块的嵌套深度
int parent_block_id;     // 节点直接所属函数块的唯一 ID
bool is_formal_param;    // 是否为形式参数（仅 PORT 有效）
```

### 生命周期维护

**创建全局节点**：
```c
depth = 0;
parent_block_id = -1;
is_formal_param = false;
```

**打包函数块（PackFunction）**：
1. 输入端口节点保留在原上下文深度，作为块对外的接口
2. 内部节点的 namespace_depth 重新基化：
   - 新深度 = 原深度 - 原上下文深度 + 1
3. 内部节点的 parent_block_id 设为新函数块 ID
4. 输入端口节点标记 is_formal_param = true
5. 输出端口标记为 false

**函数应用（Instantiate）**：
1. 复制函数块内部所有节点和约束
2. 所有复制节点的 namespace_depth = 当前上下文深度 + 原节点在块内的相对深度
3. 复制节点的 parent_block_id 保持不变（仍指向原函数块 ID）
4. β-归约连接时进行变量捕获判定

### 变量捕获消解（β-归约核心）

对于每条连线的目标端口 p，进行 O(1) 判定：

| 情况 | 条件 | 处理 |
|------|------|------|
| A（形式参数引用） | p.parent_block_id == 被复制块ID && p.is_formal_param == true | 重定向到对应实参输出端口 |
| B（自由变量引用） | p.parent_block_id != 被复制块ID | 保持原连接目标不变 |
| C（内部局部引用） | p.parent_block_id == 被复制块ID && p.is_formal_param == false | 重映射到复制件中对应的新内部节点 |

## 8. 图遍历与查询

### 邻接查询

```c
GeomNode **graph_get_neighbors(ConstraintGraph *graph, int node_id);
GeomNode **graph_get_connected_ports(ConstraintGraph *graph, int port_id);
```

### 路径查询

```c
PathResult graph_find_path(
    ConstraintGraph *graph,
    int start_node_id,
    int end_node_id,
    ConstraintType *allowed_constraints,
    int allowed_count
);
```

### 子图提取

```c
ConstraintGraph *graph_extract_subgraph(
    ConstraintGraph *graph,
    int *node_ids,
    int node_count
);
```

## 9. 序列化与反序列化

### 序列化

```c
char *graph_serialize(ConstraintGraph *graph);
```

**格式**：MessagePack 二进制格式

**内容**：
- 节点数组：[type, id, coords, coord_count, trust, ...]
- 约束数组：[type, participants, n_parts, template_id, ...]
- 函数块定义：引用内部节点 ID 范围、端口 ID

### 反序列化

```c
ConstraintGraph *graph_deserialize(const char *data, size_t len);
```

## 10. 内存管理

### 内存池

```c
typedef struct NodeMemoryPool {
    GeomNode *pool;
    int *free_list;
    int free_count;
    int capacity;
} NodeMemoryPool;
```

### 引用计数

```c
void graph_node_add_ref(ConstraintGraph *graph, int node_id);
void graph_node_release_ref(ConstraintGraph *graph, int node_id);
```

## 实现文件

- **头文件**：`include/lv00/constraint_graph.h`
- **源文件**：`src/constraint_graph.c`

## 测试要点

1. 节点的创建、删除、查询
2. 约束的添加、删除、冲突检测
3. 区域的有效性检查（闭合环、自交）
4. 端口归属标记的生命周期维护
5. 变量捕获消解的正确性
6. 图的序列化与反序列化
7. 内存管理和引用计数
