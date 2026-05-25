# 符号代数求解器 (Symbolic Algebra Solver)

## 模块概述

符号代数求解器负责从约束图中提取代数方程并求解，将几何约束转化为符号坐标的精确解。求解器覆盖希尔伯特平面几何的线性、二次、比例片段，即尺规可构造片段的完整代数刻画。

## 核心设计原则

1. **符号解优先**：结果始终为符号坐标，而非数值近似
2. **几何推理优先**：优先使用几何定理进行消元，而非直接代数求解
3. **二次边界**：仅处理二次及以下的多项式系统，超出范围时标记并提供辅助建议
4. **增量求解**：仅更新受新约束影响的子图

## 数据类型定义

### 求解结果类型

```c
typedef enum {
    SOLVE_OK,              // 求解成功，唯一解
    SOLVE_MULTIPLE,        // 多解情况
    SOLVE_NO_SOLUTION,     // 无解（过约束）
    SOLVE_UNDERDETERMINED, // 欠约束（有自由度）
    SOLVE_OUT_OF_SCOPE,    // 超出范围（高次方程）
    SOLVE_TIMEOUT          // 求解超时
} SolveResultType;
```

### 求解结果结构

```c
typedef struct SolveResult {
    SolveResultType type;
    
    // 解的值（SOLVE_OK 时有效）
    SymbolicCoord **solutions;
    int solution_count;
    
    // 多解情况（SOLVE_MULTIPLE 时有效）
    struct {
        SymbolicCoord **branch_solutions;
        int branch_count;
        char **branch_descriptions;  // 每个分支的描述
    } multiple;
    
    // 自由度信息（SOLVE_UNDERDETERMINED 时有效）
    struct {
        int free_variable_count;
        int *free_variable_ids;      // 自由变量的节点 ID
    } underdetermined;
    
    // 超出范围信息（SOLVE_OUT_OF_SCOPE 时有效）
    struct {
        char *analysis;              // 方程结构分析
        int suggested_split_points;  // 建议的拆分点数量
        int *split_point_ids;        // 建议拆分点的节点 ID
    } out_of_scope;
    
    // 冲突信息（SOLVE_NO_SOLUTION 时有效）
    struct {
        Constraint **conflicting_constraints;
        int conflict_count;
    } conflict;
} SolveResult;
```

### 代数方程结构

```c
typedef struct AlgebraicEquation {
    mpz_poly_t *polynomial;      // 多项式（以某个变量为主元）
    int main_variable;           // 主元变量索引
    int *involved_variables;     // 涉及的所有变量
    int variable_count;
    EquationSource source;       // 方程来源
} AlgebraicEquation;

typedef enum {
    SOURCE_INCIDENCE,      // 关联约束
    SOURCE_DISTANCE,       // 距离约束
    SOURCE_ANGLE,          // 角度约束
    SOURCE_PARALLEL,       // 平行约束
    SOURCE_PERPENDICULAR,  // 垂直约束
    SOURCE_TEMPLATE        // 模板展开
} EquationSource;
```

### 方程系统

```c
typedef struct EquationSystem {
    AlgebraicEquation **equations;
    int equation_count;
    int *variable_ids;           // 变量节点 ID
    int variable_count;
} EquationSystem;
```

## 1. 代数方程转化

### 约束到方程的映射

| 约束类型 | 代数方程 |
|----------|----------|
| INCIDENCE（点在线段上） | (P - A) × (B - A) = 0（叉积为零，线性方程） |
| BETWEENNESS | 不增加独立方程，仅用于解的选择 |
| INTERSECTION（线段相交） | 参数化后产生线性方程组 |
| DISTANCE（距离为 d） | (x_A - x_B)² + (y_A - y_B)² = d²（二次方程） |
| PARALLEL | 斜率相等（线性关系） |
| PERPENDICULAR | 点积为零（线性关系） |

### 方程提取函数

```c
EquationSystem *extract_equations_from_graph(
    ConstraintGraph *graph,
    int *target_node_ids,
    int target_count
);
```

**提取流程**：
1. 识别目标节点相关的所有约束
2. 将每个约束转化为代数方程
3. 建立变量到节点坐标的映射
4. 返回方程系统

### 点在线段上的方程

```c
AlgebraicEquation *equation_from_incidence(
    GeomNode *point,
    GeomNode *line_segment
);
```

**数学推导**：
- 点 P(x, y) 在线段 AB 上，其中 A(x₁, y₁), B(x₂, y₂)
- 向量 AP = (x - x₁, y - y₁)
- 向量 AB = (x₂ - x₁, y₂ - y₁)
- 叉积为零：(x - x₁)(y₂ - y₁) - (y - y₁)(x₂ - x₁) = 0
- 展开：x(y₂ - y₁) - y(x₂ - x₁) + (y₁x₂ - x₁y₂) = 0

### 距离约束的方程

```c
AlgebraicEquation *equation_from_distance(
    GeomNode *point1,
    GeomNode *point2,
    SymbolicCoord *distance
);
```

**数学推导**：
- 两点 A(x₁, y₁), B(x₂, y₂)
- 距离 d：√[(x₁ - x₂)² + (y₁ - y₂)²] = d
- 平方：(x₁ - x₂)² + (y₁ - y₂)² = d²
- 展开：x₁² - 2x₁x₂ + x₂² + y₁² - 2y₁y₂ + y₂² - d² = 0

## 2. 求解流水线

### 主求解函数

```c
SolveResult solve_equation_system(EquationSystem *system);
```

### 第一步：几何推理消元

```c
EquationSystem *geometric_elimination(EquationSystem *system);
```

**应用的几何定理模板**：

1. **相似三角形比例式**
   - 若 △ABC ~ △DEF，则 AB/DE = BC/EF = AC/DF
   - 转化为代数比例方程

2. **勾股定理**
   - 若 ∠C = 90°，则 AC² + BC² = AB²
   - 直接代入消元

3. **平行线截线段比例定理**
   - 若 AB ∥ CD，则 OA/OC = OB/OD
   - 转化为比例方程

4. **圆幂定理**
   - 相交弦：PA · PB = PC · PD
   - 切割线：PA² = PB · PC

**消元策略**：
- 优先消去可线性求解的变量
- 利用已知几何关系减少方程数量
- 识别并应用标准几何配置

### 第二步：Gröbner 基求解

```c
SolveResult groebner_solve(EquationSystem *system);
```

**适用范围**：仅对消元后仍无法线性求解的子系统使用。

**变量顺序**：
```c
int *determine_variable_order(EquationSystem *system);
```
- 按图的依赖关系排列
- 被依赖的变量排在前面

**次数限制**：
- 仅处理二次及以下的多项式系统
- 若检测到不可约三次及以上的方程，立即标记"超出范围"

**Gröbner 基计算**：
```c
mpz_poly_t **compute_groebner_basis(
    mpz_poly_t **polynomials,
    int poly_count,
    int *variable_order
);
```

### 第三步："超出范围"分析

```c
OutOfScopeAnalysis analyze_out_of_scope(EquationSystem *system);
```

**分析内容**：

1. **因式分解尝试**：
   ```c
   mpz_poly_t **factor_polynomial(mpz_poly_t *poly);
   ```
   - 尝试对多项式进行因式分解
   - 若可分解为多个二次及以下因子
   - 提示"可通过引入辅助线将问题拆分为多个二次步骤"
   - 高亮潜在的拆分点

2. **不可约高次识别**：
   - 若方程为不可约三次（如正七边形边长的极小多项式）
   - 提示"该问题在当前公理包下可能不可构造"
   - 建议尝试归约至已知不可构造问题

## 3. 多解处理

### 多解检测

```c
bool detect_multiple_solutions(EquationSystem *system);
```

**典型多解场景**：
- 圆与直线相交于两点
- 两圆相交于两点
- 二次方程的两个根

### 多解分支生成

```c
SolveResult generate_solution_branches(EquationSystem *system);
```

**分支描述生成**：
```c
char *describe_solution_branch(
    SymbolicCoord **solution,
    int solution_count,
    int branch_index
);
```

**描述示例**：
- "交点1（上方）"
- "交点2（下方）"
- "正根"
- "负根"

### 上层决策接口

求解器不自动选择分支，而是返回多解信号给上层调用者：

```c
typedef struct SolutionSelector {
    int selected_branch;
    char *selection_criterion;  // 选择准则描述
} SolutionSelector;

SolveResult solve_with_selector(
    EquationSystem *system,
    SolutionSelector *selector
);
```

上层通过选择器或用户交互来决定采用哪个分支。

## 4. 过约束检测

### 矛盾检测

```c
ConflictReport detect_contradiction(EquationSystem *system);
```

**检测时机**：在求解过程中，若代数化简推出矛盾等式（如 0 = 1）。

**响应**：
1. 立即停止对该子系统的求解
2. 标记所有涉及当前待求解变量的约束为冲突约束集
3. 以红色高亮显示

### 冲突约束集识别

```c
Constraint **identify_conflict_set(
    EquationSystem *system,
    int contradiction_equation_id
);
```

**识别算法**：
1. 从矛盾方程回溯依赖关系
2. 识别导致矛盾的最小约束集合
3. 返回冲突约束列表

## 5. 自由度计算

### 自由度分析

```c
FreedomAnalysis analyze_degrees_of_freedom(EquationSystem *system);
```

**计算内容**：
- 变量总数
- 独立方程数
- 自由度 = 变量数 - 独立方程数

### 自由节点报告

```c
typedef struct FreedomAnalysis {
    int total_variables;
    int independent_equations;
    int degrees_of_freedom;
    int *free_variable_ids;
    int free_count;
} FreedomAnalysis;
```

**探针系统显示**：
- 自由节点显示为灰色（有自由度）
- 显示每个自由节点的自由度方向

## 6. 增量求解

### 脏变量追踪

```c
typedef struct SolverState {
    HashTable *known_solutions;     // 变量 ID -> 符号坐标
    HashSet *dirty_variables;        // 自上次求解以来被修改的变量
    HashSet *affected_constraints;   // 受影响的约束
} SolverState;
```

### 增量求解函数

```c
SolveResult solve_incremental(
    ConstraintGraph *graph,
    SolverState *state,
    int *new_constraint_ids,
    int new_count
);
```

**求解流程**：
1. 识别脏变量相关的最小依赖子图
2. 仅对该子图重新运行消元和求解
3. 未受影响的变量保持其已知解不变
4. 更新 SolverState

### 依赖子图提取

```c
ConstraintGraph *extract_dependency_subgraph(
    ConstraintGraph *graph,
    int *seed_variables,
    int seed_count
);
```

**提取规则**：
- 从种子变量出发
- 沿约束关系扩展
- 直到没有新的变量加入

## 7. 辅助构造建议

### 拆分点建议

```c
SplitSuggestion suggest_splits(EquationSystem *system);
```

**建议生成**：
1. 分析高次多项式的结构
2. 识别可引入辅助变量的位置
3. 生成拆分方案

**建议格式**：
```c
typedef struct SplitSuggestion {
    int split_point_count;
    struct {
        int node_id;
        char *description;      // 建议描述
        char *construction_hint; // 构造提示
    } *suggestions;
} SplitSuggestion;
```

### 不可构造性提示

```c
UnconstructibilityHint hint_unconstructibility(EquationSystem *system);
```

**提示内容**：
- 识别可能的不可构造问题类型
- 建议归约目标（如倍立方体、三等分角）
- 提供不可构造性证明模板入口

## 8. 性能优化

### 方程缓存

```c
typedef struct EquationCache {
    HashTable *cache;  // 约束哈希 -> 方程系统
    int hit_count;
    int miss_count;
} EquationCache;
```

### 并行求解

```c
SolveResult solve_parallel(
    EquationSystem *system,
    int num_threads
);
```

对独立的子系统并行求解。

## 实现文件

- **头文件**：`include/lv00/solver.h`
- **源文件**：`src/solver.c`

## 测试要点

1. 基本几何约束的方程转化
2. 线性方程组的求解
3. 二次方程组的求解
4. 多解情况的检测和分支生成
5. 过约束检测和冲突识别
6. 自由度计算
7. 增量求解的正确性
8. 超出范围检测和辅助建议
9. 大系统的性能测试
