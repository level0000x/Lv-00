# 合一检查 (Unification Check)

## 模块概述

合一检查是 Lv-00 证明系统的核心，负责判定几何构造是否满足命题模式。通过三层匹配（端口类型匹配、约束类型匹配、符号坐标判等），合一检查实现了"构造即证明"的理念。

## 核心设计原则

1. **严格边界**：合一不调用约束求解器来判定语义等价
2. **结构匹配**：仅基于图结构、约束类型和符号坐标精确判等
3. **规范化前置**：合一前自动执行图规范化遍
4. **失败定位**：精确报告不匹配的具体位置

## 数据类型定义

### 命题结构

```c
typedef struct Proposition {
    int id;
    char *name;
    
    // 端口声明
    struct {
        int port_id;
        bool is_input;
        GeomNodeType type;
        char *type_region;       // 类型区域描述
    } *ports;
    int port_count;
    
    // 几何模式（虚线框内容）
    ConstraintGraph *pattern;
    
    // 前置条件
    Constraint **preconditions;
    int precondition_count;
    
    // 后置条件
    Constraint **postconditions;
    int postcondition_count;
} Proposition;
```

### 合一结果

```c
typedef enum {
    UNIFY_OK,               // 合一成功，命题得证
    UNIFY_PORT_MISMATCH,    // 端口类型不匹配
    UNIFY_CONSTRAINT_MISSING, // 约束缺失
    UNIFY_COORD_MISMATCH,   // 坐标不匹配
    UNIFY_TYPE_MISMATCH     // 类型不匹配
} UnifyResult;
```

### 合一失败报告

```c
typedef struct UnifyFailureReport {
    UnifyResult result;
    
    // 失败位置
    struct {
        int proposition_node_id;
        int construction_node_id;
        FailureLayer layer;
    } failure_location;
    
    // 失败详情
    char *description;
    
    // 建议
    char **suggestions;
    int suggestion_count;
} UnifyFailureReport;

typedef enum {
    LAYER_PORT,       // 端口层
    LAYER_CONSTRAINT, // 约束层
    LAYER_COORD       // 坐标层
} FailureLayer;
```

### 合一绑定

```c
typedef struct UnifyBinding {
    int pattern_node_id;       // 命题模式中的节点 ID
    int construction_node_id;  // 构造图中的节点 ID
    BindingType type;
} UnifyBinding;

typedef enum {
    BINDING_EXACT,     // 精确匹配
    BINDING_VARIABLE   // 变量绑定（多态命题）
} BindingType;
```

## 1. 合一检查流程

### 主入口函数

```c
UnifyResult proof_unify(
    ConstraintGraph *construction,
    Proposition *proposition,
    bool auto_normalize,
    UnifyFailureReport **failure_report
);
```

**执行流程**：

1. **图规范化**（若 auto_normalize 为 true）：
   ```c
   NormalizationResult norm_result = graph_normalize(construction);
   ```
   - 对构造图执行图规范化遍
   - 对命题模式图执行图规范化遍

2. **模板展开**：
   ```c
   ConstraintGraph *expanded_pattern = expand_proposition_templates(proposition);
   ```
   - 展开命题模式中的所有约束模板实例
   - 转换为正则形式的基本约束图

3. **三层匹配**：
   - 端口类型匹配
   - 约束类型匹配
   - 符号坐标判等

4. **结果判定**：
   - 若所有层面匹配成功，返回 UNIFY_OK
   - 若任何一层匹配失败，返回具体失败类型和报告

### 合一前准备

```c
ConstraintGraph *prepare_construction_for_unify(
    ConstraintGraph *construction
);

ConstraintGraph *prepare_proposition_for_unify(
    Proposition *proposition
);
```

## 2. 三层匹配

### 第一层：端口类型匹配

```c
UnifyResult match_ports(
    ConstraintGraph *construction,
    Proposition *proposition,
    UnifyBinding **bindings,
    int *binding_count
);
```

**匹配内容**：
- 构造的输出端口类型与命题声明的输出端口类型是否一致
- 类型等价通过重写引擎归一化后比较

**匹配规则**：

| 构造端口类型 | 命题端口类型 | 结果 |
|--------------|--------------|------|
| POINT | POINT | 匹配 |
| LINE_SEGMENT | LINE_SEGMENT | 匹配 |
| REGION | REGION | 匹配 |
| 具体类型 | 变量类型 | 绑定变量 |
| 不同类型 | 不同类型 | 不匹配 |

**变量绑定**：
- 对于多态命题，记录类型变量到具体类型的绑定
- 后续匹配中使用该绑定

### 第二层：约束类型匹配

```c
UnifyResult match_constraints(
    ConstraintGraph *construction,
    ConstraintGraph *pattern,
    UnifyBinding *bindings,
    int binding_count,
    UnifyBinding **new_bindings,
    int *new_binding_count
);
```

**匹配内容**：
- 命题模式中的每个约束，在构造图中寻找对应约束
- 比较约束类型和参与者节点
- 节点通过符号坐标判等匹配（允许不同 ID 但坐标相等）

**匹配算法**：

```c
bool constraints_match(
    Constraint *c1,
    Constraint *c2,
    UnifyBinding *bindings,
    int binding_count
);
```

1. **约束类型比较**：
   - c1->type == c2->type

2. **参与者比较**：
   - 对于每个参与者节点，检查是否存在绑定
   - 若无绑定，检查符号坐标是否相等
   - 若相等，建立新绑定

3. **顺序无关性**：
   - 某些约束的参与者顺序无关（如 INCIDENCE）
   - 考虑所有排列组合

### 第三层：符号坐标判等

```c
bool coords_match(
    SymbolicCoord *c1,
    SymbolicCoord *c2,
    UnifyBinding *bindings,
    int binding_count
);
```

**判等规则**：

| 类型组合 | 判等方式 |
|----------|----------|
| RATIONAL + RATIONAL | mpq_equal |
| QUADRATIC + QUADRATIC | a、b、n 三元组相等 |
| ALGEBRAIC + ALGEBRAIC | 极小多项式相等 && 隔离区间重叠 |
| TRANSCENDENTAL + TRANSCENDENTAL | name 相等 |
| 不同类型 | 尝试有理化后比较 |

**变量处理**：
- 若坐标包含变量（如参数化坐标），检查变量绑定
- 应用已有绑定后比较

## 3. 严格边界

### 不调用求解器

合一检查**不调用约束求解器**来判定语义等价。

**示例**：
- 构造：一个正方形
- 命题：一个四边形，四边相等且四角为直角

合一检查**不会**：
- 调用求解器证明"四边相等且四角为直角 ⇒ 正方形"
- 自动应用几何定理进行推理

合一检查**会**：
- 检查构造是否显式包含四边相等的约束
- 检查构造是否显式包含四角为直角的约束
- 若缺少任一约束，合一失败

### 语义等价的显式化

任何语义等价但结构不同的构造需要用户通过重写规则显式化简。

```
用户操作：
1. 应用重写规则将构造化简为与命题模式结构相同的形式
2. 再次执行合一检查
3. 合一通过
```

## 4. 失败报告与诊断

### 失败定位

```c
UnifyFailureReport *create_failure_report(
    UnifyResult result,
    int prop_node_id,
    int cons_node_id,
    FailureLayer layer,
    const char *description
);
```

**报告内容**：
- 失败类型（端口/约束/坐标）
- 命题模式中的节点 ID
- 构造图中的节点 ID
- 失败描述
- 修复建议

### 失败示例

**端口不匹配**：
```
失败类型：UNIFY_PORT_MISMATCH
位置：命题输出端口 #1，构造输出端口 #1
描述：命题期望 REGION 类型，构造提供 LINE_SEGMENT 类型
建议：
1. 检查构造的输出端口类型声明
2. 修改构造以产生 REGION 类型输出
3. 修改命题以接受 LINE_SEGMENT 类型
```

**约束缺失**：
```
失败类型：UNIFY_CONSTRAINT_MISSING
位置：命题约束 #3（INCIDENCE）
描述：构造中缺少对应的 INCIDENCE 约束
涉及节点：命题点 P3 在线段 L1 上
建议：
1. 在构造中添加点 P3 与线段 L1 的关联约束
2. 检查点 P3 的坐标是否确实在线段 L1 上
```

**坐标不匹配**：
```
失败类型：UNIFY_COORD_MISMATCH
位置：命题点 P1，构造点 P5
描述：坐标不相等
命题坐标：(1/2, 1/3)
构造坐标：(1/2, 2/3)
建议：
1. 检查构造中点 P5 的坐标计算
2. 确认约束求解是否正确执行
```

## 5. 命题等价变换

### 等价声明

```c
void declare_proposition_equivalence(
    Proposition *p1,
    Proposition *p2,
    RewriteRule *forward_rule,
    RewriteRule *backward_rule
);
```

用户可声明两个命题模式等价（如 P∧Q 等价于 Q∧P）。

### 自动应用

```c
Proposition *apply_equivalence_transformation(
    Proposition *proposition,
    RewriteRule *rule
);
```

等价声明被存储为双向重写规则，在合一前自动应用以在两种结构形式间转换。

## 6. 命题实例化

### 多态命题实例化

```c
Proposition *instantiate_proposition(
    Proposition *polymorphic_prop,
    TypeBinding *type_bindings,
    int binding_count
);
```

对于多态命题（如"对任意集合A，A⊆A"）：
1. 用户将类型变量替换为具体类型区域（如"三角形集合T"）
2. 生成具体命题实例供证明

### 类型绑定

```c
typedef struct TypeBinding {
    char *type_variable;       // 类型变量名（如 "A"）
    char *concrete_type;       // 具体类型（如 "Triangle"）
    ConstraintGraph *type_region; // 类型区域图
} TypeBinding;
```

## 7. 合一成功后的处理

### 命题得证标记

```c
void mark_proposition_proven(
    Proposition *proposition,
    ConstraintGraph *construction,
    UnifyBinding *bindings,
    int binding_count
);
```

**标记效果**：
- 命题虚线框变为实线（命题得证）
- 记录证明构造的引用
- 更新信任颜色为 GREEN

### 证明记录

```c
typedef struct ProofRecord {
    int proposition_id;
    int construction_id;
    UnifyBinding *bindings;
    int binding_count;
    int timestamp;
    char *proof_hash;          // 证明内容哈希
} ProofRecord;
```

## 8. 性能优化

### 缓存机制

```c
typedef struct UnifyCache {
    HashTable *cache;  // 构造哈希 + 命题哈希 -> 合一结果
    int hit_count;
    int miss_count;
} UnifyCache;
```

### 增量合一

```c
UnifyResult unify_incremental(
    ConstraintGraph *construction,
    Proposition *proposition,
    int *modified_node_ids,
    int modified_count,
    UnifyResult previous_result
);
```

仅对修改的部分重新进行合一检查。

## 实现文件

- **头文件**：`include/lv00/unify.h`
- **源文件**：`src/unify.c`

## 测试要点

1. 基本合一（构造与模式完全匹配）
2. 端口类型匹配（包括多态绑定）
3. 约束类型匹配（包括参与者顺序）
4. 符号坐标判等（各种坐标类型）
5. 失败报告的定位准确性
6. 命题等价变换
7. 多态命题实例化
8. 大图的性能测试
