# 命题与证明系统 (Proposition and Proof System)

## 模块概述

命题与证明系统是 Lv-00 的核心证明机制，实现了"构造即证明"的理念。系统支持命题模式定义、合一检查、证明导航器和不可构造性证明，为几何证明提供完整的工具链。

## 核心设计原则

1. **构造即证明**：几何构造通过合一检查验证是否满足命题
2. **BHK 解释**：遵循直觉主义逻辑的 Brouwer-Heyting-Kolmogorov 解释
3. **信任颜色**：不同证明步骤携带不同的信任标记
4. **可导航**：证明的每一步都可查看、回放和导出

## 数据类型定义

### 命题模式

```c
typedef struct Proposition {
    int id;
    char *name;
    char *description;
    
    // 端口声明
    struct {
        int port_id;
        bool is_input;
        char *type_description;
        ConstraintGraph *type_region;
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
    
    // 证明状态
    ProofStatus status;
    
    // 证明构造（得证后填充）
    ConstraintGraph *proof_construction;
} Proposition;
```

### 证明状态

```c
typedef enum {
    PROPOSITION_UNPROVEN,      // 未证明（蓝色虚框）
    PROPOSITION_PROVEN,        // 已证明（绿色实框）
    PROPOSITION_UNCONSTRUCTIBLE_GREEN,  // 已证不可构造（绿色）
    PROPOSITION_UNCONSTRUCTIBLE_YELLOW, // 条件性不可构造（黄色）
    PROPOSITION_IN_PROGRESS    // 证明进行中
} ProofStatus;
```

### 证明步骤

```c
typedef struct ProofStep {
    int step_number;
    StepType type;
    
    union {
        struct { int node_id; } add_node;
        struct { int constraint_id; } add_constraint;
        struct { int rule_id; } rewrite;
        struct { int func_block_id; } func_apply;
        struct { int prop_id; } lemma_apply;
    } data;
    
    // 依赖关系
    int *depends_on;
    int depend_count;
    
    // 状态快照
    ConstraintGraph *graph_snapshot;
    
    // 元数据
    char *description;
    int timestamp;
} ProofStep;

typedef enum {
    STEP_ADD_NODE,
    STEP_ADD_CONSTRAINT,
    STEP_REWRITE,
    STEP_FUNC_APPLY,
    STEP_LEMMA_APPLY,
    STEP_NORMALIZE,
    STEP_UNIFY
} StepType;
```

### 证明记录

```c
typedef struct Proof {
    int id;
    Proposition *proposition;
    
    ProofStep **steps;
    int step_count;
    
    // 信任颜色
    TrustColor overall_trust;
    
    // 依赖
    struct {
        Proposition **lemmas;
        int lemma_count;
        char **external_references;
        int external_count;
    } dependencies;
    
    // 导出格式
    char *coq_export;
    char *lean_export;
    char *latex_export;
} Proof;
```

## 1. 命题模式定义

### 命题创建

```c
Proposition *proposition_create(
    const char *name,
    int input_port_count,
    int output_port_count
);
```

**命题模式组成**：

1. **输入/输出端口**：声明命题期望的证物应具备的外部端口
2. **等待填充的几何模式**：虚线绘制的约束骨架，表示证物构造需满足的几何结构
3. **前置条件区域**（可选）：声明输入应满足的额外几何约束
4. **后置条件**（可选）：声明输出应满足的额外约束

### 模式示例

**勾股定理命题**：
```
输入端口：
  - 直角三角形 ABC（∠C = 90°）

几何模式：
  - 点 A、B、C
  - 线段 AB、BC、AC
  - 直角约束（AC ⟂ BC）

后置条件：
  - AC² + BC² = AB²
```

## 2. 合一检查

### 合一流程

```c
UnifyResult proof_unify(
    ConstraintGraph *construction,
    Proposition *proposition,
    bool auto_normalize
);
```

**执行流程**：

1. **图规范化**：
   - 对构造图执行图规范化遍
   - 对命题模式图执行图规范化遍

2. **模板展开**：
   - 展开命题模式中的所有约束模板实例
   - 转换为正则形式的基本约束图

3. **三层匹配**：
   - 端口类型匹配
   - 约束类型匹配
   - 符号坐标判等

4. **结果判定**：
   - 若所有层面匹配成功，合一通过
   - 命题虚线框变为实线（命题得证）

### 严格边界

合一**不调用约束求解器**来判定语义等价。

**示例**：
- 构造：一个正方形
- 命题：一个四边形，四边相等且四角为直角

合一检查**不会**自动证明"四边相等且四角为直角 ⇒ 正方形"。

任何语义等价但结构不同的构造需要用户通过重写规则显式化简。

## 3. 证明导航器

### 步骤幻灯片回放

```c
typedef struct ProofNavigator {
    Proof *proof;
    int current_step;
    
    // 视图状态
    bool show_normalized;
    bool show_dependencies;
    bool show_trust_colors;
} ProofNavigator;
```

**功能**：

1. **步骤导航**：
   ```c
   void navigator_next_step(ProofNavigator *nav);
   void navigator_prev_step(ProofNavigator *nav);
   void navigator_goto_step(ProofNavigator *nav, int step);
   ```

2. **自动规范化步骤可视化**：
   - 规范化前后的约束图对比
   - 合并的冗余节点以灰色虚影显示
   - 保留的节点以高亮显示

3. **依赖关系图**：
   - 每个步骤可查看其依赖的前驱步骤
   - 影响的后续步骤
   - 以有向图呈现

### 引理块折叠

```c
void navigator_toggle_lemma(ProofNavigator *nav, int lemma_id);
```

- 嵌套引理可折叠以管理复杂度
- 折叠时显示引理名称和证明状态着色

### 断点与续证

```c
void navigator_set_breakpoint(ProofNavigator *nav, int step);
void navigator_continue_from_breakpoint(ProofNavigator *nav);
```

- 证明过程中可设置断点（蓝色未完成标记）
- 保存工程后关闭
- 重新打开时从断点继续构造，上下文完整恢复

## 4. 信任颜色系统

### 颜色定义

```c
typedef enum {
    TRUST_GREEN,              // 全构造，无任何非常规依赖
    TRUST_BLUE_UNEXPLORED,    // 未探索
    TRUST_BLUE_RESOURCE,      // 资源受限
    TRUST_BLUE_OUT_OF_RANGE,  // 超出范围
    TRUST_GREEN_VERIFIED,     // 已证不可构造
    TRUST_YELLOW,             // 条件性不可构造
    TRUST_ORANGE_ORACLE,      // 非构造性oracle
    TRUST_ORANGE_EX_FALSO,    // 爆炸原理
    TRUST_AMBER,              // 数值假设
    TRUST_DARK_ORANGE         // 非构造性+数值假设
} TrustColor;
```

### 颜色语义

| 颜色 | 含义 | 使用场景 |
|------|------|----------|
| 绿色 | 全构造 | 完全构造性证明 |
| 蓝色（未探索） | 待完成的证明义务 | 尚未主动穷尽构造空间 |
| 蓝色（资源受限） | 步数熔断或超时 | 资源限制触发 |
| 蓝色（超出范围） | 求解器超出代数覆盖 | 遇到高次方程 |
| 绿色实框 | 已证不可构造 | 归约到已知不可构造问题 |
| 黄色虚线框 | 条件性不可构造 | 依赖作者断言或依赖已失效 |
| 浅橙色实心 | 依赖非构造性oracle | 使用选择公理等非构造性公理 |
| 浅橙色虚线 | 爆炸原理步骤 | 从矛盾推导任意命题 |
| 橙黄色 | 含数值假设 | 位数熔断后永久降级 |
| 深橙色 | 非构造性+数值假设 | 叠加情况 |

### 图例显示

证明导航器始终显示颜色图例，解释每种颜色的语义。

## 5. ⊥（矛盾）的公理包可定义性

### ⊥ 的几何表示

⊥ **不是硬编码的全局节点**。每个公理包定义：

1. **⊥ 的几何表示**：
   - 默认为无可填充端口的空模式
   - 一个仅有输入端口（接受证物 A）而无输出端口的几何模式
   - 任何试图与 ⊥ 合一的构造必然失败（无可匹配的输出端口）

2. **¬ 的编码**：
   - 默认为 A → ⊥（标准 BHK 解释）
   - 可被公理包覆盖为其他几何编码

3. **矛盾推导行为**：
   - 默认阻塞推导（对应直觉主义逻辑——从矛盾不能任意推导）
   - 可被公理包覆盖为爆炸原理规则（从 ⊥ 可推导任意命题 P）

### 爆炸原理（Ex Falso）

若公理包定义了爆炸原理，系统提供预置函数块 `ex_falso_quodlibet`：

```c
FuncBlock *create_ex_falso_block(void);
```

**特性**：
- 输入端口类型 = ⊥
- 输出端口类型 = 可变的 P（任意命题）
- 内部包含特殊标记节点 "ex falso quodlibet"
- 应用时，在证明图中从 ⊥ 到 P 产生一条浅橙色虚线箭头，标注"ex falso"
- 此步骤在证明导航器中作为独立步骤记录
- 与非构造性 oracle 依赖（浅橙色实心端口标记）在视觉和图例中明确区分

## 6. 不可构造性证明

### 蓝色虚框的子状态

```c
typedef enum {
    BLUE_UNEXPLORED,    // 未探索
    BLUE_RESOURCE,      // 资源受限
    BLUE_OUT_OF_RANGE   // 超出范围
} BlueSubstate;
```

### 归约式不可构造性证明

```c
UnconstructibilityProof *create_unconstructibility_proof(
    Proposition *target,
    Proposition *known_unconstructible
);
```

**证明流程**：

1. **输入目标构造问题**（如"三等分角"）
2. **输出已知不可构造问题**（如"倍立方体"）的构造
3. **用户在模板内完成归约构造**（仅使用当前公理包的全部许可构造子）
4. **若归约构造通过合一检查**：
   - 目标已知不可构造问题被公理包标记为不可构造
   - 蓝色虚框转为"已证不可构造"标记

### 构造空间穷举工具

```c
ExhaustionResult exhaust_construction_space(
    Proposition *target,
    int max_steps,
    int max_auxiliary_points
);
```

用户可对蓝色"未探索"目标触发穷举工具：

**结果**：
- 若找到构造 → 转化为绿色证物
- 若穷举空间内无构造 → 转为"未发现构造"（蓝色，可进一步转不可构造分析）
- 超时 → 转为"资源受限"蓝色

### 依赖链引用格式

公理包的"已知不可构造问题列表"中，每个条目通过依赖链字段引用其依据来源：

**内引用**：
- 指向公理包内的一个引理块
- 记录该引理块的唯一标识符及其内容哈希
- 内容哈希计算：
  1. 提取引理块的约束图规范表示
  2. 按拓扑序重新编号节点
  3. 序列化为标准邻接表格式
  4. 不包含块名称、注释、视觉布局信息、原始节点 ID 等元数据
  5. 对规范表示计算 SHA-256

**外引用**：
- 规范引用字符串，指向公认文献（如"Wantzel 1837"）
- 外引用被视为永久有效
- 可选附带"信任注释"（验证状态和时间戳）

**作者断言**：
- 引用字段为空或标记为"公理包作者断言"
- 系统立即将其识别为黄色基础

### 依赖链断裂自动降级

```c
void verify_dependency_chains(AxiomPackage *pkg);
```

公理包升级后，系统自动重验所有内引用：

- 若内引用指向的引理块内容哈希变化 → 依赖链断裂
- 绿色结论自动降级为黄色"条件性不可构造——依赖已失效"
- 系统提示用户该结论需重新验证
- 若断裂的引用后续被修复，恢复绿色

**降级限域**：降级不影响已完成的归约证明的合一检查结果，只影响结论的着色与信任级别。

## 7. 导出格式

### 交互式 HTML

```c
char *export_proof_to_html(Proof *proof);
```

- 保留步骤导航和几何视图
- 可在浏览器中查看和交互

### LaTeX 证明文本

```c
char *export_proof_to_latex(Proof *proof);
```

- 线性化步骤描述
- 适合学术论文插图

### Coq/Lean 调用序列

```c
char *export_proof_to_coq(Proof *proof);
char *export_proof_to_lean(Proof *proof);
```

- 对外部可信基的引用序列
- 可在 Coq/Lean 中验证

## 实现文件

- **头文件**：`include/lv00/proof.h`
- **源文件**：`src/proof.c`

## 测试要点

1. 命题模式创建和编辑
2. 合一检查的成功和失败场景
3. 证明导航器的步骤回放
4. 信任颜色的正确应用
5. 爆炸原理步骤的标记
6. 不可构造性证明的归约
7. 依赖链的验证和降级
8. 导出格式的正确性
