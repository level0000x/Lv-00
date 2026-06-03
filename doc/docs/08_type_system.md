# 类型系统 (Type System)

## 模块概述

类型系统为 Lv-00 提供几何层面的类型检查机制，支持宇宙层级、类型等价检查、类型推断和多态类型。类型系统确保端口连接的类型安全，同时保持公理中立性。

## 核心设计原则

1. **宇宙层级**：默认启用良基宇宙层级，防止循环包含
2. **累积性**：第 n 层的类型自动属于第 n+1 层
3. **非良基模式**：可选关闭层级检查，支持反基础公理
4. **类型等价通过重写**：类型等价检查使用重写引擎归一化

## 数据类型定义

### 类型区域

```c
typedef struct TypeRegion {
    int id;
    char *name;
    
    // 区域定义
    ConstraintGraph *definition;
    
    // 宇宙层级
    int universe_level;
    
    // 类型变量（多态类型）
    bool is_type_variable;
    char *type_variable_name;
    
    // 依赖类型
    bool is_dependent;
    struct {
        int input_port_id;
        ConstraintGraph *output_type_constructor;
    } dependent;
} TypeRegion;
```

### 类型检查结果

```c
typedef enum {
    TYPE_OK,                    // 类型匹配
    TYPE_MISMATCH,              // 类型不匹配
    TYPE_UNIVERSE_VIOLATION,    // 宇宙层级违反
    TYPE_EQUIVALENCE_UNPROVEN,  // 未能证明等价
    TYPE_INFERENCE_FAILED       // 类型推断失败
} TypeCheckResult;
```

### 类型等价结果

```c
typedef struct TypeEquivalenceResult {
    bool equivalent;
    
    // 若等价，记录重写路径
    struct {
        RewriteRule **rules;
        int rule_count;
    } rewrite_path;
    
    // 若不等价，提供差异信息
    struct {
        char *difference_description;
        ConstraintGraph *diff_graph;
    } difference;
    
    // 若未能证明，提供交互式路径
    bool interactive_path_available;
} TypeEquivalenceResult;
```

## 1. 宇宙层级机制

### 层级定义

```c
#define UNIVERSE_LEVEL_0 0    // 基本几何体（点、线段）
#define UNIVERSE_LEVEL_1 1    // 类型区域（如"所有线段的集合"）
#define UNIVERSE_LEVEL_2 2    // 类型区域的集合
// ... 以此类推
```

**规则**：
- 第 0 层：基本几何体（点、线段）
- 第 1 层：由基本几何体构成的类型区域（如"所有线段的集合"）
- 区域只能包含严格低于其层级的几何体

### 层级检查

```c
TypeCheckResult check_universe_level(
    TypeRegion *container,
    TypeRegion *contained
);
```

**检查规则**：
```c
bool is_valid_containment(TypeRegion *container, TypeRegion *contained) {
    return contained->universe_level < container->universe_level;
}
```

**示例**：
- 第 1 层区域包含第 0 层点：✓ 合法
- 第 1 层区域包含第 1 层线段：✗ 违反层级

### 累积性

第 n 层的类型也自动属于第 n+1 层（累积宇宙）。

```c
bool is_cumulative_subtype(TypeRegion *subtype, TypeRegion *supertype) {
    return subtype->universe_level <= supertype->universe_level;
}
```

**实现**：
- 层级检查时，第 n 层对象可以出现在第 n+1 层区域的内部而不报错

## 2. 非良基模式

### 模式切换

```c
void set_well_founded_mode(bool well_founded);
bool is_well_founded_mode(void);
```

**非良基模式**：
- 关闭层级检查
- 代之以解引理等非良基相容性约束
- 支持 Aczel 反基础公理

**效果**：
- 约束图中可能出现循环包含结构（A 包含 B，B 包含 A）
- 这些结构在几何上对应非良基集合，是合法的数学对象
- 包含关系是图拓扑关系而非代数方程，不增加求解器的代数负担
- 步数熔断和循环检测防止沿包含关系无限展开

## 3. 多态类型

### 类型变量

```c
TypeRegion *create_type_variable(const char *name);
```

**表示**：
- 虚线框表示的可替换类型变量
- 当函数应用到具体类型时，类型变量被替换为具体类型区域

### 类型替换

```c
TypeRegion *substitute_type_variable(
    TypeRegion *polymorphic_type,
    char *var_name,
    TypeRegion *concrete_type
);
```

**替换规则**：
- 遍历类型区域定义中的所有类型引用
- 将匹配的变量名替换为具体类型
- 返回新的类型区域

### 自动适配

```c
TypeRegion *instantiate_polymorphic_type(
    TypeRegion *poly_type,
    TypeRegion **concrete_types,
    int type_count
);
```

当函数应用到具体类型时，类型变量自动适配。

## 4. 依赖类型 Π(x:A).B(x)

### 依赖类型定义

```c
typedef struct DependentType {
    char *pi_variable;           // 绑定变量名
    TypeRegion *input_type;      // A
    ConstraintGraph *output_constructor;  // B(x) 的构造
} DependentType;
```

**语义**：
- 函数块的输出类型区域由输入值通过确定构造给出
- 在端口连接时（编译期），类型等价通过将输入值代入输出类型构造后，与目标类型进行图重写判等来完成

### 依赖类型检查

```c
TypeCheckResult check_dependent_type(
    DependentType *dep_type,
    TypeRegion *input_value,
    TypeRegion *expected_output
);
```

**检查流程**：

1. 将输入值代入输出类型构造
   ```c
   ConstraintGraph *instantiated_output = substitute_value(
       dep_type->output_constructor,
       dep_type->pi_variable,
       input_value
   );
   ```

2. 与目标类型进行图重写判等
   ```c
   TypeEquivalenceResult equiv = check_type_equivalence(
       instantiated_output,
       expected_output
   );
   ```

## 5. 类型等价检查

### 等价检查流程

```c
TypeEquivalenceResult check_type_equivalence(
    TypeRegion *type1,
    TypeRegion *type2
);
```

**检查步骤**：

1. **重写引擎归一化**：
   ```c
   ConstraintGraph *norm1 = rewrite_normalize(type1->definition);
   ConstraintGraph *norm2 = rewrite_normalize(type2->definition);
   ```
   - 使用核心安全规则集（受汇合性担保）
   - 尝试将两个类型区域归一化

2. **范式比较**：
   - 若两者归约到相同范式，连接有效
   - 返回等价结果和重写路径

3. **未能证明等价**：
   - 不立即判定"不等价"
   - 报告"未能证明等价"
   - 开放交互式路径探索器

### 交互式路径探索

```c
typedef struct PathExplorer {
    ConstraintGraph *current;
    ConstraintGraph *target;
    
    RewriteRule **available_rules;
    int rule_count;
    
    RewriteRule **applied_rules;
    int applied_count;
    
    // 历史记录
    ConstraintGraph **history;
    int history_count;
} PathExplorer;
```

**用户操作**：
1. 左侧显示当前表达式，右侧显示目标表达式
2. 中间列出所有可应用的重写规则
3. 用户点击规则 → 预览重写结果 → 确认应用或撤销
4. 支持回溯历史重写路径
5. 找到归约到目标范式的路径后自动保存并可在后续连接中复用

## 6. 类型推断

### 推断触发

```c
TypeCheckResult infer_type(
    ConstraintGraph *graph,
    int node_id,
    TypeRegion **inferred_type
);
```

**触发时机**：
- 端口连接时，若一端类型未声明（类型区域为空）
- 系统尝试从另一端推断类型

### 推断规则

```c
TypeRegion *infer_type_from_connection(ConstraintGraph *graph, int node_id);
```

**推断来源**：
1. **集合包含链**：
   - 若节点在类型区域 A 内部
   - 且 A 是类型区域 B 的子集
   - 推断节点类型为 B

2. **函数块输入输出关系**：
   - 若节点连接到函数块的输出端口
   - 推断节点类型为该端口的声明类型

3. **约束关系**：
   - 从 INCIDENCE、CONTAINMENT 等约束推断

### 推断结果处理

```c
void apply_inferred_type(
    ConstraintGraph *graph,
    int node_id,
    TypeRegion *inferred_type
);
```

- 推断结果自动填充到未声明的一端
- 若无法唯一推断，提示用户手动指定

## 7. 类型别名

### 别名定义

```c
typedef struct TypeAlias {
    char *alias_name;
    TypeRegion *aliased_type;
} TypeAlias;
```

**示例**：
```
Triangle ≡ 三个不共线点构成的区域
```

### 别名展开

```c
TypeRegion *expand_type_alias(TypeRegion *type);
```

**展开时机**：
- 类型等价检查时自动展开
- 递归展开直到无别名

### 别名注册

```c
void register_type_alias(const char *name, TypeRegion *type);
TypeRegion *lookup_type_alias(const char *name);
```

别名存储在工程中，在类型等价检查时自动展开。

## 8. 端口类型检查

### 连接时类型检查

```c
TypeCheckResult check_port_connection(
    GeomNode *source_port,
    GeomNode *target_port
);
```

**检查流程**：

1. **提取类型**：
   ```c
   TypeRegion *source_type = get_port_type(source_port);
   TypeRegion *target_type = get_port_type(target_port);
   ```

2. **类型等价检查**：
   ```c
   TypeEquivalenceResult equiv = check_type_equivalence(
       source_type,
       target_type
   );
   ```

3. **结果处理**：
   - 若等价，允许连接
   - 若不等价，拒绝连接
   - 若未能证明，标记为"未证明等价"（蓝色虚框）

### 方向性检查

```c
bool check_connection_direction(GeomNode *source, GeomNode *target);
```

**规则**：
- 源端必须是输出端口或全局节点（无方向）
- 目标端必须是输入端口

## 9. 类型错误报告

### 错误信息

```c
typedef struct TypeError {
    TypeCheckResult result;
    int node_id;
    char *message;
    
    struct {
        TypeRegion *expected;
        TypeRegion *actual;
    } type_mismatch;
    
    struct {
        int expected_level;
        int actual_level;
    } universe_violation;
} TypeError;
```

### 错误示例

**类型不匹配**：
```
错误：端口类型不匹配
位置：节点 #42（输入端口）
期望类型：Triangle
实际类型：Quadrilateral
建议：检查函数块的输入类型声明
```

**宇宙层级违反**：
```
错误：宇宙层级违反
位置：类型区域 "AllSets"
包含对象层级：1
区域自身层级：1
规则：区域只能包含严格低于其层级的几何体
建议：提升类型区域的宇宙层级到 2
```

## 实现文件

- **头文件**：`include/lv00/type_system.h`
- **源文件**：`src/type_system.c`

## 测试要点

1. 宇宙层级检查（良基和非良基模式）
2. 累积性规则
3. 多态类型替换
4. 依赖类型检查
5. 类型等价检查（自动和交互式）
6. 类型推断
7. 类型别名展开
8. 端口连接类型检查
