# 递归与条件 (Recursion and Conditionals)

## 模块概述

递归与条件模块为 Lv-00 提供递归构造和条件分支的支持。通过测度系统确保递归终止性，通过选择器块实现条件分支，使 Lv-00 能够表达复杂的算法和证明结构。

## 核心设计原则

1. **测度保证终止**：递归块必须声明测度，确保递归参数严格递减
2. **符号测度判定**：递减性由代数不等式引擎直接判定
3. **深度监控**：维护递归调用深度计数器，防止无限递归
4. **条件可视化**：选择器块以实线/虚影区分激活/未激活分支

## 数据类型定义

### 测度定义

```c
typedef struct Measure {
    MeasureType type;
    
    union {
        struct {
            SymbolicCoord *expression;  // 符号测度表达式
        } symbolic;
        
        struct {
            char *name;                 // 测度名称
            ConstraintGraph *order_relation;  // 序关系定义
        } abstract;
    } data;
    
    // 良基关系
    WellFoundedRelation well_founded;
} Measure;

typedef enum {
    MEASURE_SYMBOLIC,   // 符号测度
    MEASURE_ABSTRACT    // 非符号测度
} MeasureType;

typedef enum {
    WF_LESS_THAN,       // 严格小于
    WF_SUBTERM,         // 子项关系
    WF_CUSTOM           // 自定义良基关系
} WellFoundedRelation;
```

### 递归块

```c
typedef struct RecursiveBlock {
    int id;
    char *name;
    
    // 基本函数块结构
    FuncBlock *base_block;
    
    // 递归特性
    bool is_recursive;
    Measure *measure;
    
    // 自引用端口
    int *recursive_input_ports;
    int recursive_input_count;
    int *recursive_output_ports;
    int recursive_output_count;
    
    // 互递归
    struct {
        bool is_mutually_recursive;
        int *partner_block_ids;
        int partner_count;
    } mutual;
} RecursiveBlock;
```

### 递归调用记录

```c
typedef struct RecursionCall {
    int call_id;
    int block_id;
    SymbolicCoord **argument_measures;  // 实参的测度值
    int measure_count;
    int depth;                          // 当前递归深度
    int timestamp;
} RecursionCall;
```

### 选择器块

```c
typedef struct SelectorBlock {
    int id;
    char *name;
    
    // 条件判定
    struct {
        int test_point_id;      // 测试点
        int test_region_id;     // 测试区域
        ContainmentTestType test_type;
    } condition;
    
    // 分支
    struct {
        ConstraintGraph *true_branch;
        ConstraintGraph *false_branch;
    } branches;
    
    // 激活状态
    bool true_branch_active;
    bool false_branch_active;
    bool undetermined;
} SelectorBlock;

typedef enum {
    CONTAINMENT_STRICT,     // 严格在内部
    CONTAINMENT_BOUNDARY,   // 在边界上
    CONTAINMENT_OUTSIDE     // 在外部
} ContainmentTestType;
```

## 1. 递归构造

### 递归块定义

```c
RecursiveBlock *recursive_block_create(
    FuncBlock *base_block,
    Measure *measure
);
```

**递归块特性**：
- 函数块内部可引用该函数块自身的输入/输出端口（自引用）
- 递归块在打包时必须声明测度：一个几何量及其上的良基关系

### 测度声明

```c
void recursive_block_set_measure(
    RecursiveBlock *block,
    Measure *measure
);
```

**测度类型**：

1. **符号测度**：
   - 可归约到符号坐标上的代数表达式
   - 例如：线段长度、区域面积
   - 递减性由内核的代数不等式引擎直接判定

2. **非符号测度**：
   - 公理包定义的抽象序结构
   - 递减性检查由公理包的一级模板提供
   - 加载时验证模板的正确性

### 测度递减性检查

```c
bool check_measure_decrease(
    SymbolicCoord *before,
    SymbolicCoord *after,
    WellFoundedRelation relation
);
```

**符号测度判定**：
```c
bool check_symbolic_measure_decrease(
    SymbolicCoord *old_measure,
    SymbolicCoord *new_measure
) {
    // 计算两次调用的测度值之差
    SymbolicCoord *diff = symbolic_coord_subtract(old_measure, new_measure);
    
    // 判断是否 > 0
    return symbolic_coord_is_positive(diff);
}
```

**非符号测度判定**：
```c
bool check_abstract_measure_decrease(
    ConstraintGraph *order_relation,
    void *old_value,
    void *new_value
);
```

### 递归调用时检查

```c
RecursionCheckResult check_recursive_call(
    RecursiveBlock *block,
    SymbolicCoord **new_arguments,
    int arg_count,
    RecursionContext *context
);
```

**检查流程**：

1. **提取测度值**：
   ```c
   SymbolicCoord *new_measure = extract_measure(
       block->measure,
       new_arguments
   );
   ```

2. **比较测度**：
   ```c
   SymbolicCoord *old_measure = context->current_measure;
   bool decreasing = check_measure_decrease(
       old_measure,
       new_measure,
       block->measure->well_founded
   );
   ```

3. **结果处理**：
   - 若递减，允许递归调用
   - 若不递减，报错"递归测度未递减"

## 2. 测度系统

### 符号测度表达式

```c
SymbolicCoord *measure_expression_create(
    MeasureExpressionType type,
    void *data
);
```

**支持的测度表达式**：

| 类型 | 表达式 | 示例 |
|------|--------|------|
| 线段长度 | length(segment) | AB 的长度 |
| 区域面积 | area(region) | 三角形 ABC 的面积 |
| 点坐标 | coordinate(point, axis) | 点 P 的 x 坐标 |
| 组合 | expr1 + expr2, expr1 * expr2 | 复合表达式 |

### 非符号测度注册

```c
void register_abstract_measure(
    const char *name,
    ConstraintGraph *order_relation,
    AxiomPackage *pkg
);
```

**加载时验证**：
- 公理包注册非符号测度类型时，需在模板元数据中声明"非符号测度比较器"
- 系统在模板形式化测试阶段，验证声明与实际行为一致
- 若检测到非符号测度，系统提示用户"该测度的递归终止性依赖公理包作者的模板正确性"

### 良基关系

```c
typedef struct WellFoundedRelation {
    char *name;
    
    // 关系判定
    bool (*is_less)(void *a, void *b);
    
    // 不可降链保证
    bool (*is_well_founded)(void);
} WellFoundedRelation;
```

**标准良基关系**：

1. **严格小于**（<）：用于数值测度
2. **子项关系**：用于结构测度（如列表长度）
3. **字典序**：用于多参数测度

## 3. 互递归支持

### 互递归定义

```c
void set_mutual_recursion(
    RecursiveBlock *block,
    int *partner_ids,
    int partner_count
);
```

**互递归特性**：
- 两个函数块可互相引用对方的端口
- 例如：f 调用 g，g 调用 f

### 互递归测度检查

```c
bool check_mutual_recursion_termination(
    RecursionCall *call_chain,
    int chain_length
);
```

**检查要求**：
- 互递归的测度检查要求两者在同一个全局测度下各自递减
- 系统在递归调用的调用链上追踪测度值
- 确保沿着调用链方向单调递减

### 调用链追踪

```c
typedef struct CallChain {
    RecursionCall *calls;
    int length;
    SymbolicCoord *global_measure;
} CallChain;
```

**追踪流程**：
1. 每次递归调用记录当前测度值
2. 检查新测度是否严格小于调用链中所有前驱测度
3. 若违反，报错并显示调用链

## 4. 递归深度监控

### 深度计数器

```c
typedef struct RecursionMonitor {
    int current_depth;
    int max_depth;          // 默认 10000
    bool limit_reached;
} RecursionMonitor;
```

### 深度检查

```c
RecursionLimitResult check_recursion_depth(
    RecursionMonitor *monitor,
    int increment
);
```

**检查流程**：

1. 每次进入递归调用时 +1
2. 达到上限（默认 10000）时暂停
3. 提示用户可能的无限递归
4. 用户可选择：
   - 继续（提升上限或忽略此次警告）
   - 终止

### 深度溢出处理

```c
typedef struct DepthOverflow {
    int current_depth;
    int max_depth;
    RecursionCall *call_stack;
    int stack_depth;
} DepthOverflow;
```

**处理选项**：
- 显示当前调用栈
- 允许增加深度上限
- 允许忽略并继续

## 5. 条件/选择器块

### 选择器块创建

```c
SelectorBlock *selector_block_create(
    int test_point_id,
    int test_region_id,
    ContainmentTestType test_type
);
```

**选择器块结构**：
- 两个分支子图（true_branch 和 false_branch）
- 基于几何包含关系的判定结果激活对应分支

### 条件判定

```c
SelectorResult evaluate_selector_condition(SelectorBlock *selector);
```

**判定结果**：

| 结果 | 条件 | 分支状态 |
|------|------|----------|
| TRUE | 点在区域内 | true_branch 激活（实线），false_branch 虚影 |
| FALSE | 点在区域外 | false_branch 激活（实线），true_branch 虚影 |
| BOUNDARY | 点在边界上 | 两个分支均半透明，提示需要更多约束 |
| UNDETERMINED | 区域边界未清晰界定 | 两个分支均半透明 |

### 分支激活

```c
void activate_branch(
    SelectorBlock *selector,
    bool true_branch,
    ConstraintGraph *target_graph
);
```

**激活效果**：
- 激活的分支子图以实线显示
- 未激活的分支子图保持虚影（灰色半透明）
- 未确定时，两个分支均保持半透明

### 选择器块应用

```c
void apply_selector_block(
    SelectorBlock *selector,
    ConstraintGraph *graph,
    int insertion_point
);
```

**应用流程**：
1. 评估条件
2. 根据结果激活对应分支
3. 将激活分支合并到目标图
4. 记录选择器块应用步骤

## 6. 模式匹配

### 对和类型的模式匹配

```c
typedef struct PatternMatch {
    int tag_value;
    ConstraintGraph *pattern;
    ConstraintGraph *branch;
} PatternMatch;
```

**实现**：
- 对和类型（∨）的模式匹配实现为选择器块的泛化
- 分析析取证物的标记（tag）
- 根据标记选择对应分支子图
- 不匹配的分支保持灰色虚影

### 模式匹配块

```c
PatternMatchBlock *pattern_match_block_create(
    int value_port_id,
    PatternMatch *cases,
    int case_count
);
```

**工作流程**：
1. 读取值端口的标记
2. 匹配对应的模式分支
3. 激活匹配的分支
4. 绑定模式变量

## 7. 固定点组合子

### Y 组合子

```c
FuncBlock *create_y_combinator(void);
```

**Y 组合子定义**：
```
Y = λf.(λx.f(x x))(λx.f(x x))
```

**在 Lv-00 中的实现**：
- Y 组合子本身是一个函数块
- 内部包含自引用端口
- 用于递归的标准实现

### Y 组合子应用

```c
ConstraintGraph *apply_y_combinator(
    FuncBlock *f,
    ConstraintGraph *arg
);
```

**归约规则**：
1. Y F 单步展开 → F(Y F)
2. 验证自引用端口的 parent_block_id 传递正确性
3. Y F 3 多步归约到范式（如 3! = 6）
4. 验证深层递归展开中共享子图的引用一致性

### 终止性安全机制

```c
bool check_y_combinator_termination(
    FuncBlock *f,
    Measure *measure
);
```

**安全要求**：
- Y 组合子只能用于递归测度可判定的场景
- 当测度不可判定时，添加额外步数限制
- 用户可手动设置最大递归展开次数

## 8. 递归与证明

### 结构归纳法

```c
Proof *structural_induction_proof(
    Proposition *base_case,
    Proposition *inductive_step,
    Measure *structural_measure
);
```

**结构归纳**：
- 基于结构测度的归纳证明
- 基础情形：最小结构
- 归纳步骤：若对子结构成立，则对整体成立

### 良基归纳法

```c
Proof *well_founded_induction_proof(
    Proposition *proposition,
    WellFoundedRelation *relation
);
```

**良基归纳**：
- 基于任意良基关系的归纳证明
- 证明：若对所有更小元素成立，则对当前元素成立

## 实现文件

- **头文件**：`include/lv00/recursion.h`
- **源文件**：`src/recursion.c`

## 测试要点

1. 符号测度的递减性检查
2. 非符号测度的模板验证
3. 互递归的测度追踪
4. 递归深度监控和溢出处理
5. 选择器块的条件判定
6. 模式匹配的分支选择
7. Y 组合子的归约
8. 结构归纳法证明
