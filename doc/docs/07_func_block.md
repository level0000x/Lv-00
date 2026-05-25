# 函数块系统 (Function Block System)

## 模块概述

函数块系统是 Lv-00 的抽象机制，允许将几何构造封装为可复用的模块。函数块支持打包、实例化、确定性检查、多解选择器和组合子，实现了几何层面的函数式编程。

## 核心设计原则

1. **封装性**：函数块封装内部约束子图，对外仅暴露输入/输出端口
2. **确定性追踪**：函数块维护确定性状态机，确保使用安全
3. **β-归约**：实例化时执行变量捕获消解，实现正确的参数传递
4. **组合子支持**：预置 Compose、Product 等几何化组合子

## 数据类型定义

### 函数块结构

```c
typedef struct FuncBlock {
    int id;                          // 函数块唯一 ID
    char *name;                      // 函数块名称
    
    // 内部子图
    ConstraintGraph *internal_graph;
    int *internal_node_ids;          // 内部节点 ID 列表
    int internal_count;
    
    // 端口
    int *input_port_ids;             // 输入端口 ID 列表
    int input_count;
    int *output_port_ids;            // 输出端口 ID 列表
    int output_count;
    
    // 确定性状态
    DeterminismState determinism;
    
    // 多解选择器
    SolutionSelector *selector;
    
    // 跨边界约束（提升为端口依赖）
    Constraint **boundary_constraints;
    int boundary_constraint_count;
    
    // 元数据
    char *axiom_package;
    int creation_timestamp;
} FuncBlock;
```

### 确定性状态

```c
typedef enum {
    UNVERIFIED,           // 打包完成，尚未进行静态分析
    VERIFIED,             // 静态分析确认解唯一
    NON_DETERMINISTIC,    // 应用时出现过多解情况
    PARTIALLY_VERIFIED    // 静态分析未完成，但未发现冲突
} DeterminismState;
```

### 打包结果

```c
typedef struct PackResult {
    bool success;
    FuncBlock *func_block;
    
    // 冲突信息（打包失败时）
    struct {
        Constraint **conflicts;
        int conflict_count;
        char **conflict_descriptions;
    } conflicts;
    
    // 警告信息
    struct {
        char **warnings;
        int warning_count;
    } warnings;
} PackResult;
```

### 实例化结果

```c
typedef struct InstantiateResult {
    bool success;
    ConstraintGraph *instantiated_graph;
    
    // ID 映射
    struct {
        int old_id;
        int new_id;
    } *id_mappings;
    int mapping_count;
    
    // 多解情况
    struct {
        bool has_multiple_solutions;
        int solution_count;
        ConstraintGraph **solution_graphs;
    } multiple;
    
    // 错误信息
    char *error_message;
} InstantiateResult;
```

## 1. 打包操作 (PackFunction)

### 打包流程

```c
PackResult func_block_pack(
    ConstraintGraph *graph,
    int *internal_nodes,
    int internal_count,
    int *input_ports,
    int input_count,
    int *output_ports,
    int output_count,
    Constraint **boundary_constraints,
    int boundary_count
);
```

**输入**：
- 内部节点 ID 集合
- 输入端口 ID 列表
- 输出端口 ID 列表

**打包前强制检查——跨边界约束**：

```c
CrossBoundaryReport check_cross_boundary_constraints(
    ConstraintGraph *graph,
    int *internal_nodes,
    int internal_count
);
```

1. 遍历图中所有约束
2. 识别涉及两个节点集合（内部节点集合 vs 外部节点集合）的非 CONNECTION 约束
3. 这些约束是"跨边界约束"——封装后它们将被函数块边界遮挡

**用户处理选项**：

| 选项 | 行为 | 图形表示 |
|------|------|----------|
| 提升 | 该约束成为函数块的端口依赖 | 端口旁显示小图标 |
| 断开 | 删除该约束 | 无 |
| 取消 | 放弃本次打包 | 返回编辑状态 |

**图标含义**：
- INCIDENCE：短线穿点
- CONTAINMENT：小圆含小点
- BETWEENNESS：三点一线

### 打包执行

```c
void execute_pack(
    ConstraintGraph *graph,
    PackConfiguration *config,
    FuncBlock *func_block
);
```

**执行步骤**：

1. **创建函数块节点**：
   - 分配新 FUNCTION_BLOCK 节点 ID
   - 设置节点类型和元数据

2. **更新内部节点标记**：
   ```c
   // 端口归属标记更新
   for each internal_node:
       node->namespace_depth = current_depth + 1;
       node->parent_block_id = func_block->id;
   ```

3. **标记输入端口**：
   ```c
   for each input_port:
       port->is_formal_param = true;
   ```

4. **标记输出端口**：
   ```c
   for each output_port:
       port->is_formal_param = false;
   ```

5. **记录跨边界约束**：
   - 将用户选择"提升"的约束记录到函数块

## 2. 确定性检查

### 静态层（打包时）

```c
DeterminismResult static_determinism_check(FuncBlock *func_block);
```

**检查流程**：

1. **线性约束系统**：
   - 若内部是线性约束系统
   - 解的存在唯一性可在多项式时间内判定
   - 若有唯一解，函数块标记为 VERIFIED

2. **二次约束系统**：
   - 若内部涉及二次约束
   - 使用符号代数引擎尝试消元并计算解的个数
   - 若在步数上限（默认 100）内完成且确认为唯一解
   - 标记为 VERIFIED

3. **未完成分析**：
   - 若静态分析未能在步数上限内确认唯一性
   - 但未发现冲突
   - 标记为 PARTIALLY_VERIFIED，允许使用

### 动态层（应用时）

```c
DeterminismResult dynamic_determinism_check(
    FuncBlock *func_block,
    ConstraintGraph *instantiated_graph
);
```

**检查流程**：

1. **唯一解**：
   - 求解器计算具体输入下的实际输出
   - 若产生唯一解，正常输出
   - VERIFIED 状态维持
   - PARTIALLY_VERIFIED 可升级为 VERIFIED

2. **多解**：
   - 若产生多解且用户未提供选择器
   - 应用被拒绝并提示"该函数在此输入下产生多解，请提供选择器"
   - 第一次出现多解后，函数块降级为 NON_DETERMINISTIC
   - 此后每次应用都需要选择器

3. **无解**：
   - 若产生零解
   - 报告"无解——输入不满足前置条件"

### 状态转换图

```
UNVERIFIED --静态分析唯一--> VERIFIED
    |                            |
    |--静态分析未完成--> PARTIALLY_VERIFIED
    |                            |
    |--应用时多解--> NON_DETERMINISTIC <--|
    |                                     |
    |--应用时唯一解--> VERIFIED            |
                                         |
NON_DETERMINISTIC --每次应用--> 需要选择器
```

## 3. 多解选择器

### 选择器定义

```c
typedef struct SolutionSelector {
    SelectorType type;
    union {
        struct {
            int target_point_id;
            ComparisonOp op;           // >, <, =
            SymbolicCoord *reference;
        } coordinate_based;
        
        struct {
            int target_point_id;
            int region_id;
        } containment_based;
        
        struct {
            int target_point_id;
            int reference_point_id;
            ComparisonOp op;           // closest, farthest
        } distance_based;
        
        struct {
            char *custom_predicate;
            int *arg_ids;
            int arg_count;
        } custom;
    } data;
    
    char *description;
} SolutionSelector;
```

### 选择器类型

| 类型 | 描述 | 示例 |
|------|------|------|
| 坐标基准 | 基于坐标值选择 | "取正根" |
| 包含基准 | 基于区域包含选择 | "取位于区域 A 内的交点" |
| 距离基准 | 基于距离选择 | "取距离点 P 最近的解" |
| 自定义 | 用户定义的条件 | 任意谓词 |

### 选择器应用

```c
int apply_selector(
    ConstraintGraph *solutions,
    int solution_count,
    SolutionSelector *selector
);
```

**应用流程**：

1. 对每个候选解评估选择条件
2. 过滤符合条件的解
3. 若过滤后仍有多解，报错要求更精确的选择器或手动选择
4. 返回选中的解索引

### 选择器回退策略

当选择器无法在候选解中找到任何满足条件的解时，**统一返回 `false`**，不再静默回退到任意解或默认解。调用方需根据返回值决定后续处理（例如提示用户提供更宽松的选择条件，或放弃本次操作）。这一策略确保了多解场景下的行为可预测性，避免因静默选择导致几何构造结果不一致。

### 选择器存储

选择器作为函数块定义的一部分存储：
```c
void func_block_set_selector(
    FuncBlock *func_block,
    SolutionSelector *selector
);
```

## 4. 函数应用 (Instantiate)

### 实例化流程

```c
InstantiateResult func_block_instantiate(
    FuncBlock *func_block,
    ConstraintGraph *target_graph,
    int *actual_arg_ids,         // 实参节点 ID（对应输入端口）
    int arg_count,
    int target_depth             // 目标上下文深度
);
```

**输入**：
- 函数块 ID
- 实参连接映射（块的输入端口 → 外部节点的输出端口）

**执行步骤**：

1. **复制内部子图**：
   ```c
   ConstraintGraph *copy = copy_subgraph(func_block->internal_graph);
   ```

2. **维护 ID 映射表**：
   ```c
   HashTable *id_map = create_id_mapping(
       func_block->internal_node_ids,
       copy->nodes
   );
   ```
   - 包含所有新创建的节点
   - 以及输入/输出端口节点在复制体中的对应关系

3. **更新深度标记**：
   ```c
   for each node in copy:
       node->namespace_depth = target_depth + node->relative_depth;
   ```

4. **β-归约连接**：
   ```c
   void beta_reduction(
       ConstraintGraph *copy,
       HashTable *id_map,
       int *actual_arg_ids,
       FuncBlock *func_block
   );
   ```

### β-归约（变量捕获消解）

对于每条从形式参数端口 p 出发的内部连线：

```c
BetaReductionResult beta_reduce_connection(
    GeomNode *port,
    HashTable *id_map,
    int *actual_arg_ids,
    FuncBlock *func_block
);
```

**O(1) 判定**：

| 情况 | 条件 | 处理 |
|------|------|------|
| A（形式参数引用） | p.parent_block_id == 被复制块ID && p.is_formal_param == true | 重定向到对应实参输出端口 |
| B（自由变量引用） | p.parent_block_id != 被复制块ID | 保持原连接目标不变 |
| C（内部局部引用） | p.parent_block_id == 被复制块ID && p.is_formal_param == false | 重映射到复制件中对应的新内部节点 |

**连接重定向**：
```c
void redirect_connection(
    Constraint *connection,
    int old_target_id,
    int new_target_id
);
```

## 5. 部分应用（柯里化）

### 部分应用实现

```c
FuncBlock *func_block_partial_apply(
    FuncBlock *func_block,
    int *connected_input_indices,
    int *actual_arg_ids,
    int connected_count
);
```

**示例**：
- 原函数块：f: A→B→C（两个输入端口）
- 部分应用：仅连接 A，不连接 B
- 返回新函数块：类型为 B→C

**实现机制**：

1. 创建新函数块包装原块
2. 将已连接的实参 A 固化到内部
3. 剩余输入端口 B 暴露为新函数块的输入端口
4. 输出端口 C 保持不变

## 6. 函数块组合子

### 组合操作的原子性与 ID 回滚

组合子操作（Compose、Product 等）在执行过程中会向约束图添加新节点。若组合操作在中途失败（例如端口类型不匹配、内部连线冲突等），系统会自动将 `graph->next_node_id` 回滚到操作前的值，确保约束图不会残留部分创建的无效节点。这保证了组合操作的原子性。

### Compose (组合)

```c
FuncBlock *combinator_compose(
    FuncBlock *f,    // f: A→B
    FuncBlock *g     // g: B→C
);
// 返回: g∘f: A→C
```

**内部实现**：
- 内部将 f 的输出端口连接到 g 的输入端口
- 封装为新的函数块

### Product (乘积)

```c
FuncBlock *combinator_product(
    FuncBlock *f,    // f: A→B
    FuncBlock *g     // g: C→D
);
// 返回: f×g: A×C→B×D
```

**内部实现**：
- 内部将两组端口并列
- 输入端口：A 和 C
- 输出端口：B 和 D

## 7. 视图折叠/展开

### 视图控制

```c
void func_block_set_expanded(FuncBlock *func_block, bool expanded);
bool func_block_is_expanded(FuncBlock *func_block);
```

**折叠状态**：
- 折叠：只显示输入/输出端口和块名称
- 展开：显示完整内部构造

**注意**：折叠/展开状态保存在视图层，不改变底层约束图。

## 8. 函数块库

### 预置函数块

```c
typedef struct FuncBlockLibrary {
    FuncBlock **blocks;
    int count;
    char *axiom_package;
} FuncBlockLibrary;

FuncBlockLibrary *load_func_block_library(const char *axiom_package);
```

### 预设函数块类别枚举

```c
typedef enum {
    PRESET_CATEGORY_GEOMETRY,       // 几何构造类
    PRESET_CATEGORY_MEASUREMENT,    // 度量计算类
    PRESET_CATEGORY_TRANSFORM,      // 几何变换类
    PRESET_CATEGORY_ALGEBRA,        // 代数运算类
    PRESET_CATEGORY_LOGIC,          // 逻辑推导类
    PRESET_CATEGORY_ANALYSIS        // 分析运算类
} PresetCategory;
```

### 预置函数块（40 个）

系统共提供 40 个预设函数块，按功能分为六大类别。

#### 几何构造类（18 个）

| 名称 | 签名 | 描述 |
|------|------|------|
| midpoint | POINT × POINT → POINT | 中点构造 |
| perpendicular_bisector | LINE_SEGMENT → LINE | 垂直平分线 |
| angle_bisector | POINT × POINT × POINT → LINE | 角平分线 |
| parallel_line | LINE × POINT → LINE | 过点作平行线 |
| perpendicular_line | LINE × POINT → LINE | 过点作垂线 |
| circle_by_center_radius | POINT × DISTANCE → CIRCLE | 圆心+半径作圆 |
| circle_by_three_points | POINT × POINT × POINT → CIRCLE | 三点确定圆 |
| line_intersection | LINE × LINE → POINT | 两直线交点 |
| reflection | POINT × LINE → POINT | 点关于直线的反射 |
| equilateral_triangle | POINT × POINT → POINT | 等边三角形第三顶点 |
| circumcenter | POINT × POINT × POINT → POINT | 外心 |
| incenter | POINT × POINT × POINT → POINT | 内心 |
| centroid | POINT × POINT × POINT → POINT | 重心 |
| orthocenter | POINT × POINT × POINT → POINT | 垂心 |
| foot_of_perpendicular | POINT × LINE → POINT | 垂足 |
| tangent_line_from_point | POINT × CIRCLE → LINE | 过圆外一点作切线 |
| nine_point_circle | POINT × POINT × POINT → CIRCLE | 九点圆 |
| excenter | POINT × POINT × POINT → POINT | 旁心 |

#### 度量计算类（6 个）

| 名称 | 签名 | 描述 |
|------|------|------|
| distance | POINT × POINT → DISTANCE | 两点距离 |
| angle_measure | POINT × POINT × POINT → ANGLE | 角度度量 |
| area_measure | POINT × POINT × POINT → AREA | 三角形面积 |
| perimeter_measure | POINT × POINT × POINT → DISTANCE | 三角形周长 |
| ratio_measure | DISTANCE × DISTANCE → RATIO | 比值度量 |
| slope_measure | POINT × POINT → SLOPE | 斜率度量 |

#### 几何变换类（5 个）

| 名称 | 签名 | 描述 |
|------|------|------|
| translation | POINT × VECTOR → POINT | 平移变换 |
| rotation | POINT × POINT × ANGLE → POINT | 旋转变换 |
| homothety | POINT × POINT × RATIO → POINT | 位似变换 |
| circle_inversion | POINT × CIRCLE → POINT | 圆反演 |
| affine_transform | POINT × MATRIX → POINT | 仿射变换 |

#### 代数运算类（7 个）

| 名称 | 签名 | 描述 |
|------|------|------|
| vector_add | VECTOR × VECTOR → VECTOR | 向量加法 |
| vector_sub | VECTOR × VECTOR → VECTOR | 向量减法 |
| vector_scale | VECTOR × SCALAR → VECTOR | 向量数乘 |
| vector_dot_product | VECTOR × VECTOR → SCALAR | 向量点积 |
| vector_cross_product_magnitude | VECTOR × VECTOR → SCALAR | 向量叉积模 |
| vector_reflect | VECTOR × LINE → VECTOR | 向量反射 |
| vector_project | VECTOR × LINE → VECTOR | 向量投影 |

#### 逻辑推导类（2 个）

| 名称 | 签名 | 描述 |
|------|------|------|
| contradiction_detector | CONSTRAINT_SET → BOOL | 矛盾检测 |
| implication_chain | PROPOSITION × PROPOSITION → PROPOSITION | 蕴含链推导 |

#### 分析运算类（2 个）

| 名称 | 签名 | 描述 |
|------|------|------|
| taylor_approximation | FUNCTION × POINT × INT → FUNCTION | 泰勒近似展开 |
| limit_point | FUNCTION × POINT → POINT | 极限点计算 |

## 实现文件

函数块系统已拆分为多个编译单元，各模块职责如下：

### 头文件

| 文件 | 描述 |
|------|------|
| `include/lv00/func_block.h` | 公共 API 头文件，暴露打包、实例化、组合子等接口 |
| `include/lv00/func_block_internal.h` | 内部共享头文件，供各编译单元共享内部数据结构和辅助函数声明 |

### 源文件

| 文件 | 描述 |
|------|------|
| `src/func_block.c` | 主模块：函数块创建、销毁、端口管理等核心逻辑 |
| `src/func_block_determinism.c` | 确定性检查模块：静态层与动态层的确定性分析 |
| `src/func_block_instantiate.c` | 例化模块：函数块实例化、β-归约、ID 映射 |
| `src/func_block_serialize.c` | 序列化模块：函数块的序列化与反序列化 |

## 测试要点

1. 打包操作的跨边界约束检测
2. 确定性检查的静态层和动态层
3. 多解选择器的应用
4. 函数应用的 β-归约
5. 部分应用（柯里化）
6. 组合子的正确性
7. 视图折叠/展开
8. 复杂嵌套函数块
