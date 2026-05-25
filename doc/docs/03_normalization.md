# 图规范化遍引擎 (Graph Normalization Pass Engine)

## 模块概述

图规范化遍引擎负责在合一检查前对约束图进行标准化处理，通过合并冗余节点确保图的唯一表示形式。这是保证合一检查正确性的关键步骤，也是"构造即证明"理念的技术基础。

## 核心设计原则

1. **幂等性保证**：规范化后的图再次运行规范化不会产生任何变化
2. **符号坐标判等**：仅基于可判定的符号等价合并节点，不引入公理包的语义等价
3. **作用域感知**：坐标相同的点若属于不同作用域，提示用户确认是否合并
4. **确定性选择**：每次合并保留最小 ID 作为代表，确保结果唯一

## 触发时机

1. **合一检查（Unify）执行前自动调用**
2. **用户通过界面显式请求"规范化视图"**
3. **重写引擎不调用全局规范化**——它使用内置的局部等价容忍

## 数据类型定义

### 规范化日志条目

```c
typedef struct NormalizationLogEntry {
    int old_node_id;           // 被合并的节点 ID
    int new_node_id;           // 保留的代表节点 ID
    bool auto_merged;          // 是否自动合并（还是用户确认后合并）
    int timestamp;             // 规范化步骤序号
} NormalizationLogEntry;
```

### 规范化结果

```c
typedef struct NormalizationResult {
    bool success;
    int merged_point_count;           // 合并的点数量
    int merged_segment_count;         // 合并的线段数量
    int merged_region_count;          // 合并的区域数量
    NormalizationLogEntry *log;       // 规范化日志
    int log_count;
    int *user_confirmation_requests;  // 需要用户确认的点对
    int confirmation_count;
} NormalizationResult;
```

### 并查集结构

```c
typedef struct UnionFind {
    int *parent;              // 父节点数组
    int *rank;                // 秩数组（用于按秩合并）
    int size;
} UnionFind;
```

## 1. 算法流程

### 主入口函数

```c
NormalizationResult graph_normalize(ConstraintGraph *graph);
```

### 第一阶段：点合并

#### 步骤 1：哈希分组

```c
HashTable *group_points_by_hash(ConstraintGraph *graph);
```

1. 遍历图中所有 POINT 节点
2. 按符号坐标的哈希值分组
3. 哈希基于符号坐标的规范序列化计算

#### 步骤 2：组内精确判等

```c
bool points_are_equivalent(
    GeomNode *p1,
    GeomNode *p2,
    bool *needs_user_confirmation
);
```

对每个哈希组（大小 ≥2），内部两两执行 `coord_equal()` 精确判等。

#### 步骤 3：作用域检查

```c
typedef enum {
    MERGE_AUTO,       // 自动合并
    MERGE_CONFIRM,    // 需要用户确认
    MERGE_SKIP        // 跳过（不同作用域）
} MergeDecision;

MergeDecision check_merge_scope(GeomNode *p1, GeomNode *p2);
```

检查它们的作用域归属：
- 若 `parent_block_id` 和 `namespace_depth` 均相同（同一作用域）→ 自动合并
- 若作用域不同 → 生成提示信息，由用户界面弹出确认对话框

#### 步骤 4：并查集合并

```c
void union_find_merge(UnionFind *uf, int a, int b);
```

使用并查集处理传递闭包（A=B, B=C → A、B、C 全部合并）。

**合并规则**：每个等价类选择 ID 最小的节点作为保留代表。

#### 步骤 5：约束更新

```c
void update_constraints_after_merge(
    ConstraintGraph *graph,
    UnionFind *uf,
    NormalizationLogEntry *log
);
```

遍历所有约束，将引用被合并节点的参与者 ID 替换为保留节点 ID。

#### 步骤 6：节点移除

```c
void remove_merged_nodes(
    ConstraintGraph *graph,
    UnionFind *uf,
    NormalizationLogEntry *log
);
```

被合并的节点从图中移除，记录到规范化日志中（合并前后 ID 对）。

### 第二阶段：线段和区域合并

#### 线段合并

```c
int merge_line_segments(ConstraintGraph *graph, NormalizationLogEntry *log);
```

1. 遍历所有 LINE_SEGMENT 节点
2. 若两条线段的两个端点（经第一阶段合并后）完全对应相等
3. 则合并线段节点
4. 同样使用并查集处理传递闭包

**线段判等条件**：
```c
bool segments_are_equivalent(LineSegment *s1, LineSegment *s2) {
    // 端点 ID 经第一阶段合并后比较
    int s1_start = find_representative(s1->start_point_id);
    int s1_end = find_representative(s1->end_point_id);
    int s2_start = find_representative(s2->start_point_id);
    int s2_end = find_representative(s2->end_point_id);
    
    // 有向线段：方向必须一致
    return (s1_start == s2_start && s1_end == s2_end) ||
           // 或者考虑无向等价（根据需求）
           (s1_start == s2_end && s1_end == s2_start);
}
```

#### 区域合并

```c
int merge_regions(ConstraintGraph *graph, NormalizationLogEntry *log);
```

1. 遍历所有 REGION 节点
2. 若两个区域的边界线段序列（经第一、第二阶段合并后）完全对应相等
3. 则合并区域节点

**区域判等条件**：
```c
bool regions_are_equivalent(Region *r1, Region *r2) {
    // 边界线段 ID 经合并后比较
    int *r1_segments = get_representative_segments(r1);
    int *r2_segments = get_representative_segments(r2);
    
    // 考虑循环移位等价（多边形旋转）
    return sequences_are_cyclic_equivalent(r1_segments, r2_segments);
}
```

### 第三阶段：稳定化

#### 拓扑排序稳定化

```c
void stabilize_constraint_order(ConstraintGraph *graph);
```

1. 对约束图进行拓扑排序稳定化
2. 按节点 ID 升序遍历约束的参与者列表
3. 重新排列参与者 ID 的顺序以产生唯一确定的约束表示

**稳定化规则**：
- 每个约束的参与者列表按 ID 升序排列
- 约束按（类型，参与者列表）字典序排列

## 2. 幂等性保证

规范化算法设计保证幂等：规范化后的图再次运行规范化不会产生任何变化。

### 幂等性实现机制

1. **确定性选择**：每次合并保留最小 ID 作为代表
2. **固定遍历顺序**：第二阶段和第三阶段使用固定的遍历顺序（按 ID 升序）
3. **无前置假设**：规范化前不假设图已有任何规范形式

### 幂等性验证

```c
bool verify_normalization_idempotence(ConstraintGraph *graph) {
    // 保存原始图状态
    char *original = graph_serialize(graph);
    
    // 第一次规范化
    NormalizationResult result1 = graph_normalize(graph);
    char *after_first = graph_serialize(graph);
    
    // 第二次规范化
    NormalizationResult result2 = graph_normalize(graph);
    char *after_second = graph_serialize(graph);
    
    // 验证第二次规范化没有产生任何变化
    bool idempotent = (result2.merged_point_count == 0 &&
                       result2.merged_segment_count == 0 &&
                       result2.merged_region_count == 0);
    
    // 验证两次序列化结果相同
    bool identical = (strcmp(after_first, after_second) == 0);
    
    // 恢复原始状态
    graph_deserialize(original);
    
    return idempotent && identical;
}
```

## 3. 用户确认对话框

### 跨作用域合并确认

当图规范化遍检测到坐标相同的点属于不同作用域时弹出：

```c
typedef struct MergeConfirmationDialog {
    int point1_id;
    int point1_scope;      // namespace_depth 和 parent_block_id
    SymbolicCoord *point1_coord;
    
    int point2_id;
    int point2_scope;
    SymbolicCoord *point2_coord;
    
    // 用户选择
    enum {
        MERGE,           // 合并为一个点
        KEEP_BOTH,       // 保留两者（不合并）
        CANCEL           // 取消（跳过此次规范化中的该对点）
    } user_choice;
} MergeConfirmationDialog;
```

**对话框显示信息**：
- 两个点的 ID
- 各自的作用域（所属函数块）
- 坐标值（符号和数值近似）

## 4. 规范化日志

### 日志内容

每次自动化规范化记录以下信息：

```c
typedef struct NormalizationLog {
    int step_number;                    // 规范化步骤序号
    int timestamp;                      // 时间戳
    
    struct {
        int old_id;
        int new_id;
        GeomNodeType type;
        bool auto_merged;
    } *merges;
    int merge_count;
    
    struct {
        int constraint_id;
        int old_participant_id;
        int new_participant_id;
    } *constraint_updates;
    int update_count;
} NormalizationLog;
```

### 日志使用

**证明导航器展示**：
- 展开规范化日志
- 以灰色虚影显示被合并的节点
- 以高亮显示保留的节点
- 显示合并前后的图差异

## 5. 符号坐标判等

### 坐标判等函数

```c
bool coord_equal(const SymbolicCoord *a, const SymbolicCoord *b);
```

**判等规则**：

| 类型组合 | 判等方式 |
|----------|----------|
| RATIONAL + RATIONAL | mpq_equal |
| QUADRATIC + QUADRATIC | a、b、n 三元组相等 |
| ALGEBRAIC + ALGEBRAIC | 极小多项式相等 && 隔离区间重叠 |
| TRANSCENDENTAL + TRANSCENDENTAL | name 字符串相等 |
| 不同类型 | 尝试有理化后比较，否则不相等 |

### 哈希函数

```c
uint64_t coord_hash(const SymbolicCoord *coord);
```

**哈希计算**：
- RATIONAL：基于分子分母的哈希组合
- QUADRATIC：基于 a、b、n 的哈希组合
- ALGEBRAIC：基于多项式系数的哈希组合
- TRANSCENDENTAL：基于 name 字符串哈希

## 6. 性能优化

### 空间索引

```c
typedef struct SpatialIndex {
    // 网格索引或 R-树
    GridCell **cells;
    int cell_count;
    
    // 仅对数值近似坐标建立索引
    // 用于快速筛选可能相等的点候选
} SpatialIndex;
```

### 增量规范化

```c
NormalizationResult graph_normalize_incremental(
    ConstraintGraph *graph,
    int *modified_node_ids,
    int modified_count
);
```

仅对修改的节点及其邻域进行规范化，而非全图。

## 7. 与重写引擎的关系

### 分工边界

| 引擎 | 调用时机 | 处理范围 | 等价容忍 |
|------|----------|----------|----------|
| 规范化遍 | 合一检查前、用户请求 | 全图 | 符号坐标严格相等 |
| 重写引擎 | 新约束添加后、用户触发 | 局部匹配 | 局部符号坐标判等 |

### 协作关系

1. 重写引擎使用局部等价容忍，避免频繁调用全局规范化
2. 合一检查前必须调用规范化遍，确保图处于规范形式
3. 规范化日志与重写日志分别记录，证明导航器可展示完整历史

## 实现文件

- **头文件**：`include/lv00/normalization.h`
- **源文件**：`src/normalization.c`

## 测试要点

1. 点的合并（同作用域自动合并、跨作用域确认）
2. 线段的合并（端点相等判断）
3. 区域的合并（边界序列等价判断）
4. 幂等性验证
5. 约束更新的一致性
6. 规范化日志的完整性
7. 增量规范化的正确性
8. 大图的性能测试
