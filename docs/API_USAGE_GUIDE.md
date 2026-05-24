# Lv-00 API 使用指南

> 几何元语言 -- 以几何为唯一载体的双模数学元语言系统 v3.2.0。
> 几何体本身是计算的执行者、数据的承载者、证明的见证者。

本文档按照 CGAL 式的"**概念 -> 模型 -> 示例 -> 复杂度**"四部分结构组织，
以几何概念为线索，全面描述 Lv-00 的 API 接口与使用方法。

---

## 目录

1. [符号坐标系统 (Symbolic Coordinate Kernel)](#1-符号坐标系统-symbolic-coordinate-kernel)
2. [约束图 (Constraint Graph)](#2-约束图-constraint-graph)
3. [图规范化 (Graph Normalization Pass)](#3-图规范化-graph-normalization-pass)
4. [符号求解器 (Symbolic Solver)](#4-符号求解器-symbolic-solver)
5. [图重写引擎 (Graph Rewrite Engine)](#5-图重写引擎-graph-rewrite-engine)
6. [合一检查 (Unification Check)](#6-合一检查-unification-check)
7. [函数块系统 (Function Block System)](#7-函数块系统-function-block-system)
8. [类型系统 (Type System)](#8-类型系统-type-system)
9. [命题与证明系统 (Proof System)](#9-命题与证明系统-proof-system)
10. [公理包系统 (Axiom Package System)](#10-公理包系统-axiom-package-system)
- [附录 A: 系统生命周期](#附录-a-系统生命周期)
- [附录 B: 引擎便捷 API](#附录-b-引擎便捷-api)
- [附录 C: 内存管理规则](#附录-c-内存管理规则)
- [附录 D: 错误处理模式](#附录-d-错误处理模式)

---

## 1. 符号坐标系统 (Symbolic Coordinate Kernel)

### 📖 概念定义

符号坐标系统是 Lv-00 的数值基础层，定义了四种互斥且完备的坐标类型：

| 类型 | 数学表示 | 典型来源 |
|------|---------|---------|
| `RATIONAL` | a/b, 其中 a in Z, b in N+ | 直尺构造的初始点、整数网格点 |
| `ALGEBRAIC` | 整系数多项式的实根, 由区间 [(l,r)] 隔离 | 圆规交点、高次方程的几何解 |
| `QUADRATIC` | a + b*sqrt(n), 其中 a,b in Q, n in N | 二次扩域运算结果、直尺圆规可构造数 |
| `TRANSCENDENTAL` | pi, e 等常超越数及与有理数的和/积 | 圆的周长、面积等非代数构造 |

每种坐标类型均带有**信任颜色 (TrustColor)** 属性，构成从 GREEN (完全构造) 到 RED (不可构造)
的七层信誉体系，是 Lv-00 证明系统的基础设施。

### 🏗️ 模型实现

C 代码中，坐标类型以带标签联合体 (tagged union) 方式实现:

```c
struct SymbolicCoord {
    CoordType type;          /* RATIONAL | ALGEBRAIC | QUADRATIC | TRANSCENDENTAL */
    union {
        Rational *rational;         /* 底层 GMP mpq_t */
        Algebraic *algebraic;       /* 最小多项式 + 隔离区间 */
        Quadratic *quadratic;       /* a, b: Rational*, n: unsigned int */
        Transcendental *transcendental; /* name[32] + expr tree */
    } data;
    TrustColor trust;        /* GREEN..RED */
};
```

- Rational 底层使用 GMP `mpq_t`，保证任意精度。
- Algebraic 通过 `mpz_poly_t` 和 `[left_bound, right_bound]` 表示，内置惰性精度细化和连分数有理化。
- Quadratic 表示 a + b*sqrt(n)。
- Transcendental 支持 pi, e 及其符号表达式。

系统包含**位电路 (Digit Circuit)** 机制: 有理数位数超过 BIT_CUTOFF_THRESHOLD (10^6) 时触发跳闸，
支持 A/B 双计划切换 (PLAN_A_FULL_ALGEBRAIC / PLAN_B_QUADRATIC_ONLY)。

### 📝 API 参考

**创建 (Constructors)**

| 函数签名 | 说明 |
|---------|------|
| `SymbolicCoord* symbolic_coord_create_rational(int64_t numer, uint64_t denom)` | 有理坐标 numer/denom |
| `SymbolicCoord* symbolic_coord_create_algebraic(mpz_poly_t *poly, double left, double right)` | 代数坐标 |
| `SymbolicCoord* symbolic_coord_create_quadratic(Rational *a, Rational *b, unsigned int n)` | a + b*sqrt(n) |
| `SymbolicCoord* symbolic_coord_create_transcendental(const char *name)` | 超越数, 如 "pi" |
| `Rational* rational_create(int64_t num, uint64_t denom)` | 独立有理数 |
| `Rational* rational_create_from_mpz(const mpz_t num, const mpz_t denom)` | 从 GMP 大整数 |
| `Algebraic* algebraic_create(mpz_poly_t *poly, double left, double right)` | 独立代数数 |
| `Quadratic* quadratic_create(Rational *a, Rational *b, unsigned int n)` | 独立二次扩张数 |
| `Transcendental* transcendental_create(const char *name)` | 独立超越数 |

**销毁 (Destructors)**

| 函数签名 | 说明 |
|---------|------|
| `void symbolic_coord_destroy(SymbolicCoord *coord)` | 销毁符号坐标 |
| `void rational_destroy(Rational *r)` | 销毁有理数 |
| `void algebraic_destroy(Algebraic *a)` | 销毁代数数 |
| `void quadratic_destroy(Quadratic *q)` | 销毁二次扩张数 |
| `void transcendental_destroy(Transcendental *t)` | 销毁超越数 |

**算术运算** -- 所有运算返回新分配的符号坐标，调用者负责销毁。

| 函数签名 | 说明 |
|---------|------|
| `SymbolicCoord* symbolic_coord_add(const SymbolicCoord *a, const SymbolicCoord *b)` | 加法 |
| `SymbolicCoord* symbolic_coord_subtract(const SymbolicCoord *a, const SymbolicCoord *b)` | 减法 |
| `SymbolicCoord* symbolic_coord_multiply(const SymbolicCoord *a, const SymbolicCoord *b)` | 乘法 |
| `SymbolicCoord* symbolic_coord_divide(const SymbolicCoord *a, const SymbolicCoord *b)` | 除法 |
| `SymbolicCoord* symbolic_coord_negate(const SymbolicCoord *coord)` | 取相反数 |
| `SymbolicCoord* symbolic_coord_pow(const SymbolicCoord *base, unsigned int exponent)` | 幂运算 |
| `SymbolicCoord* symbolic_coord_sqrt(const SymbolicCoord *coord)` | 平方根 |

**有理数层直接运算**

| 函数签名 | 说明 |
|---------|------|
| `Rational* rational_add(const Rational *a, const Rational *b)` | 有理数加法 |
| `Rational* rational_subtract(const Rational *a, const Rational *b)` | 有理数减法 |
| `Rational* rational_multiply(const Rational *a, const Rational *b)` | 有理数乘法 |
| `Rational* rational_divide(const Rational *a, const Rational *b)` | 有理数除法 |

**比较与工具**

| 函数签名 | 说明 |
|---------|------|
| `int symbolic_coord_compare(const SymbolicCoord *a, const SymbolicCoord *b)` | 比较: 返回负/零/正 |
| `bool symbolic_coord_is_zero(const SymbolicCoord *coord)` | 是否为零 |
| `bool symbolic_coord_is_positive(const SymbolicCoord *coord)` | 是否为正 |
| `bool symbolic_coord_is_negative(const SymbolicCoord *coord)` | 是否为负 |
| `SymbolicCoord* symbolic_coord_copy(const SymbolicCoord *src)` | 深拷贝 |
| `double symbolic_coord_to_double(const SymbolicCoord *coord)` | 转换为 double (近似) |
| `uint64_t symbolic_coord_hash(const SymbolicCoord *coord)` | 哈希值 (用于节点分组) |

**序列化**

| 函数签名 | 说明 |
|---------|------|
| `char* symbolic_coord_serialize(const SymbolicCoord *coord)` | 序列化 (调用者 free) |
| `char* rational_serialize(const Rational *r)` | 有理数序列化 |
| `Rational* rational_parse(const char *str)` | 解析 "3/4" 或 "1.5" |
| `char* algebraic_serialize(const Algebraic *a)` | 代数数序列化 |
| `char* quadratic_serialize(const Quadratic *q)` | 二次扩张数序列化 |
| `char* transcendental_serialize(const Transcendental *t)` | 超越数序列化 |

**高级功能**

| 函数签名 | 说明 |
|---------|------|
| `int algebraic_refine_for_equality(Algebraic *a, Algebraic *b, int max_iterations)` | 倍增精度判断相等 |
| `bool algebraic_try_rationalize(Algebraic *a)` | 连分数有理化 |
| `SymbolicCoord* symbolic_coord_try_expand_nested_sqrt(const SymbolicCoord *coord)` | 展开嵌套平方根 |
| `AlgebraicPlan algebraic_get_plan(void)` / `algebraic_set_plan(AlgebraicPlan)` | A/B 计划 |
| `TrustColor symbolic_coord_get_trust(const SymbolicCoord*)` / `symbolic_coord_set_trust(...)` | 信任颜色 |

### ⚡ 复杂度标注

| 操作 | 时间复杂度 | 空间复杂度 | 备注 |
|------|-----------|-----------|------|
| 创建有理坐标 | O(1) | O(1) | GMP mpq_t 初始化 |
| 创建代数坐标 | O(d) | O(d) | d=多项式度数 |
| 有理数四则运算 | O(M(b) log b) | O(b) | M(b)=GMP 大整数乘法复杂度 |
| 代数数加法/乘法 | O(d^3) | O(d^2) | 结式法 |
| 二次扩张四则运算 | O(M(b)) | O(b) | 等价于有理数运算 |
| 比较同类型 | O(1) ~ O(M(b)) | O(1) | 取决于底层数字 |
| 序列化 | O(n) | O(n) | n=字符串长度 |
| 平方根 | O(d^3) | O(d^2) | 创建 Quadratic |
| 精度细化 | O(2^k * M(b)) | O(b) | k=迭代次数 |

### 💡 使用示例

```c
#include "lv00.h"
#include <stdio.h>

int main(void) {
    lv00_init();

    SymbolicCoord *x = symbolic_coord_create_rational(3, 4);  /* 3/4 */
    SymbolicCoord *y = symbolic_coord_create_rational(5, 2);  /* 5/2 */
    SymbolicCoord *sum = symbolic_coord_add(x, y);
    char *s = symbolic_coord_serialize(sum);
    printf("x + y = %s\n", s);
    free(s);

    Rational *a = rational_create(1, 1);
    Rational *b = rational_create(1, 1);
    SymbolicCoord *sqrt2 = symbolic_coord_create_quadratic(a, b, 2);
    printf("sqrt2 type: %d\n", sqrt2->type);  /* QUADRATIC */

    if (symbolic_coord_compare(x, y) < 0) printf("x < y\n");
    printf("trust: %d\n", symbolic_coord_get_trust(x));  /* 0 = GREEN */

    symbolic_coord_destroy(x); symbolic_coord_destroy(y);
    symbolic_coord_destroy(sum); symbolic_coord_destroy(sqrt2);
    lv00_cleanup();
    return 0;
}
```

---

## 2. 约束图 (Constraint Graph)

### 📖 概念定义

约束图是 Lv-00 的核心数据结构，将几何构造编码为带类型的图。包含两种实体:

**节点 (GeomNode)** -- 五种几何类型:

| 节点类型 | 维度 | 说明 |
|---------|------|------|
| `GEOM_POINT` | 0 | 由 n 个符号坐标定位 |
| `GEOM_LINE_SEGMENT` | 1 | 两端点定义的有限线段 |
| `GEOM_REGION` | 2 | 边界线段围成的封闭区域 |
| `GEOM_PORT` | -- | 函数块输入/输出接口, 带多态类型标记 |
| `GEOM_FUNCTION_BLOCK` | -- | 封装几何构造单元, 含内部节点和端口 |

**约束 (Constraint)** -- 五种关系:

| 约束类型 | 语义 | 参与者 |
|---------|------|--------|
| `INCIDENCE` | 点在线上 | (point, line_or_region) |
| `BETWEENNESS` | 三点共线有序 | (p1, p2, p3) |
| `INTERSECTION` | 两对象相交 | (line1, line2, result_point) |
| `CONTAINMENT` | 对象包含 | (inner, outer) |
| `CONNECTION` | 端口连接 | (src_port, dst_port) |

### 🏗️ 模型实现

约束图以**邻接表**组织，通过**哈希索引**支持 O(1) 按 ID 查找:

```c
struct ConstraintGraph {
    GeomNode **nodes;           /* 动态扩容节点数组 */
    int node_count, node_capacity;
    Constraint **constraints;   /* 动态扩容约束数组 */
    int constraint_count, constraint_capacity;
    int next_node_id, next_constraint_id;
    GeomNode **node_index;      /* O(1) 哈希索引 */
    Constraint **constraint_index;
};
```

额外支持冗余检测 (`graph_detect_redundant_constraints`)、冲突分析 (`graph_detect_conflicts`) 和完整 JSON 序列化。

### 📝 API 参考

**生命周期**

| 函数签名 | 说明 |
|---------|------|
| `ConstraintGraph* graph_create(void)` | 创建空图 |
| `void graph_destroy(ConstraintGraph *graph)` | 销毁图及所有内容 |

**添加节点**

| 函数签名 | 返回 |
|---------|------|
| `AddNodeResult graph_add_point(graph, SymbolicCoord **coords, int coord_count)` | ADD_NODE_OK/CONFLICT |
| `AddNodeResult graph_add_line_segment(graph, int endpoint1_id, int endpoint2_id)` | 同上 |
| `AddNodeResult graph_add_region(graph, int *boundary_seg_ids, int seg_count)` | + INVALID_REGION |
| `AddNodeResult graph_add_port(graph, PortType type, int namespace_depth, int parent_block_id)` | 同上 |
| `AddNodeResult graph_add_function_block(graph, int *internal_ids, int n, int *in_ports, int ni, int *out_ports, int no)` | 同上 |

**添加约束**

| 函数签名 | 返回 |
|---------|------|
| `AddConstraintResult graph_add_incidence(graph, int point_id, int line_or_region_id)` | OK/DUPLICATE/CONFLICT |
| `AddConstraintResult graph_add_betweenness(graph, int p1, int p2, int p3)` | 同上 |
| `AddConstraintResult graph_add_intersection(graph, int line1, int line2, int result_pt)` | 同上 |
| `AddConstraintResult graph_add_containment(graph, int inner_id, int outer_id)` | 同上 |
| `AddConstraintResult graph_add_connection(graph, int src_port, int dst_port)` | 同上 |

**查询与修改**

| 函数签名 | 说明 |
|---------|------|
| `GeomNode* graph_get_node(const ConstraintGraph *graph, int node_id)` | O(1) 查节点 |
| `Constraint* graph_get_constraint(const ConstraintGraph *graph, int constraint_id)` | O(1) 查约束 |
| `int graph_get_last_added_node_id(const ConstraintGraph *graph)` | 最近添加的节点 ID |
| `int graph_get_node_count(const ConstraintGraph *graph)` | 节点数 |
| `int graph_get_constraint_count(const ConstraintGraph *graph)` | 约束数 |
| `RemoveNodeResult graph_remove_node(ConstraintGraph *graph, int node_id)` | 删除节点 |
| `RemoveConstraintResult graph_remove_constraint(ConstraintGraph *graph, int idx)` | 删除约束 |
| `int graph_find_constraints_involving(const ConstraintGraph *, int node_id, int *out, int max)` | 查找关联约束 |
| `int graph_detect_redundancy(...)` / `graph_detect_redundant_constraints(...)` | 冗余检测 |
| `CrossBoundaryConstraint* find_cross_boundary_constraints(...)` | 跨边界约束 |

**序列化**

| 函数签名 | 说明 |
|---------|------|
| `char* graph_serialize_to_json(const ConstraintGraph*)` | 图 -> JSON (调用者 free) |
| `ConstraintGraph* graph_deserialize_from_json(const char *json)` | JSON -> 图 (调用者 destroy) |

### ⚡ 复杂度标注

| 操作 | 时间 | 空间 | 备注 |
|------|------|------|------|
| `graph_create` | O(1) | O(1) | |
| `graph_add_point` / `graph_add_line_segment` | O(1) 摊销 | O(1) 摊销 | |
| `graph_add_region` | O(S) | O(S) | S=segment_count |
| `graph_add_function_block` | O(N+P) | O(N+P) | N=内部节点, P=端口 |
| `graph_add_incidence` 等约束 | O(V) | O(1) | V=节点总数 (冗余检测) |
| `graph_get_node` / `graph_get_constraint` | O(1) | -- | 哈希索引 |
| `graph_remove_node` | O(V+E) | O(V+E) | 清理关联约束 |
| `graph_serialize_to_json` | O(V+E) | O(V+E) | |
| `graph_destroy` | O(V+E) | -- | |

### 💡 使用示例

```c
#include "lv00.h"
#include <stdio.h>

int main(void) {
    lv00_init();
    ConstraintGraph *g = graph_create();

    SymbolicCoord *c1[] = { symbolic_coord_create_rational(0,1), symbolic_coord_create_rational(0,1) };
    graph_add_point(g, c1, 2);
    int p1 = graph_get_last_added_node_id(g);
    SymbolicCoord *c2[] = { symbolic_coord_create_rational(3,1), symbolic_coord_create_rational(4,1) };
    graph_add_point(g, c2, 2);
    int p2 = graph_get_last_added_node_id(g);

    graph_add_line_segment(g, p1, p2);
    int L = graph_get_last_added_node_id(g);
    graph_add_incidence(g, p1, L);
    graph_add_incidence(g, p2, L);

    printf("Nodes: %d, Constraints: %d\n", graph_get_node_count(g), graph_get_constraint_count(g));

    char *json = graph_serialize_to_json(g);
    printf("%s\n", json); free(json);
    graph_destroy(g);
    lv00_cleanup();
    return 0;
}
```

---

## 3. 图规范化 (Graph Normalization Pass)

### 📖 概念定义

图规范化遍是约束图上的**恒等保持变换**。将坐标等价但拥有不同节点 ID 的节点合并为单一代表节点，
消除冗余的共线线段和重叠区域，使约束图达到**规范形式 (normal form)**。

核心不变量: 规范化前后的图在语义上等价，但结构被压缩为最小表达。

### 🏗️ 模型实现

基于**并查集 (Union-Find)** 实现:

1. 扫描节点，将坐标等价且类型相同的节点归入等价类
2. 选择代表节点，将其余标记为已合并
3. 重定向关联约束
4. 删除冗余节点
5. 额外执行共线线段合并和重叠区域合并

合并过程通过 `NormalizationLog` 记录，支持用户回调 (`MergeConfirmCallback`) 干预跨作用域合并。

```c
typedef struct NormalizationResult {
    int *merged_node_ids; int merged_count;
    int *original_ids; int *representative_ids;
    bool user_confirmed;
    NormalizationLog *log;
} NormalizationResult;
```

### 📝 API 参考

| 函数签名 | 说明 |
|---------|------|
| `NormalizationResult* graph_normalize(ConstraintGraph *graph, bool scope_aware)` | 执行规范化 |
| `void normalization_result_destroy(NormalizationResult *result)` | 销毁结果 |
| `bool normalization_verify_idempotency(ConstraintGraph *graph)` | 验证幂等性 |
| `int merge_line_segments(ConstraintGraph *graph, NormalizationLog *log)` | 仅合并共线线段 |
| `int merge_regions(ConstraintGraph *graph, NormalizationLog *log)` | 仅合并重叠区域 |
| `NodeMergeCandidate* find_merge_candidates(ConstraintGraph *graph, int *out_count)` | 查找候选 |
| `int apply_merges(ConstraintGraph *graph, NodeMergeCandidate *candidates, int count, bool *confirmed)` | 应用候选 |
| `void merge_candidates_destroy(NodeMergeCandidate *candidates, int count)` | 销毁候选 |
| `void normalization_set_merge_callback(MergeConfirmCallback cb, void *user_data)` | 设置回调 |
| `void graph_topological_sort_stable(ConstraintGraph *graph)` | 拓扑排序 |

### ⚡ 复杂度标注

| 操作 | 时间 | 空间 | 备注 |
|------|------|------|------|
| `graph_normalize` | O(V * alpha(V) + E) | O(V + E) | alpha=反阿克曼函数 |
| `norm_verify_idempotency` | O(V * alpha(V) + E) | O(V + E) | 两次规范化 |
| `find_merge_candidates` | O(V^2) | O(K) | K=候选数 |
| `merge_line_segments` | O(L * alpha(L)) | O(L) | L=线段数 |
| `graph_topological_sort_stable` | O(V + E) | O(V) | |

### 💡 使用示例

```c
#include "lv00.h"
#include <stdio.h>

int main(void) {
    lv00_init();
    ConstraintGraph *g = graph_create();
    SymbolicCoord *c[] = { symbolic_coord_create_rational(0,1), symbolic_coord_create_rational(0,1) };
    graph_add_point(g, c, 2);  /* 节点 0 */
    graph_add_point(g, c, 2);  /* 节点 1 (相同坐标) */
    printf("Before: %d nodes\n", graph_get_node_count(g));  /* 2 */

    NormalizationResult *r = graph_normalize(g, false);
    printf("Merged: %d, After: %d nodes\n", r->merged_count, graph_get_node_count(g));  /* 1 */

    if (normalization_verify_idempotency(g)) printf("Idempotent OK\n");
    normalization_result_destroy(r);
    graph_destroy(g);
    lv00_cleanup();
    return 0;
}
```

---

## 4. 符号求解器 (Symbolic Solver)

### 📖 概念定义

符号求解器将几何约束转换为多项式方程组，并利用 Groebner 基方法求解。
核心路径: **几何约束 -> 代数方程 -> Groebner 基 -> 解的符号表示**。

约束到方程的转换:

| 约束类型 | 代数方程 | 类型 |
|---------|---------|------|
| `INCIDENCE` | 叉积=0 | 线性 |
| `INTERSECTION` | 参数化线性系统 | 线性 |
| `CONTAINMENT` | 卷绕数 | 非线性 (标记超出范围) |
| `BETWEENNESS` | 无独立方程 | 解选择 |

采用**增量求解**策略: 仅重解脏变量子图。

### 🏗️ 模型实现

实现简化版 Buchberger 算法，仅处理度数 <= 2 的系统 (覆盖直尺圆规构造)。
对二次方程通过 `solver_handle_multiple_solutions` 生成解分支，代数数运算使用结式法。

```c
typedef struct GroebnerResult {
    SymbolicCoord **solutions; int solution_count;
    bool unique; bool overdetermined;
} GroebnerResult;
```

### 📝 API 参考

| 函数签名 | 说明 |
|---------|------|
| `SolverStatus solve_algebraic_system(ConstraintGraph*, const int *dirty_var_ids, int n, GroebnerResult **out)` | 主求解 |
| `GroebnerResult* solver_incremental_solve(ConstraintGraph*, const int *dirty, int n)` | 增量求解 |
| `void groebner_result_free(GroebnerResult *result)` | 销毁结果 |
| `SolverStatus eliminate_geometry(ConstraintGraph *graph, int target_var, int *elim, int n)` | 消元 |
| `SolverStatus analyze_out_of_scope(ConstraintGraph *graph, int var_id, char **suggestion)` | 分析超范围 |
| `int count_degrees_of_freedom(ConstraintGraph *graph, int **out_free_var_ids)` | 自由度 |
| `bool check_conflict_equations(const ConstraintGraph *graph)` | 冲突检测 |
| `EquationSystem* equation_system_create(void)` / `equation_system_destroy(...)` | 方程系统 |
| `int solver_extract_equations_full(const ConstraintGraph *graph, EquationSystem *out)` | 全类型提取 |
| `SolverStatus groebner_basis_compute(EquationSystem *system)` | 基计算 (d<=2) |
| `SolverStatus solver_handle_multiple_solutions(...)` | 多解分支 |

**SolverStatus**: SOLVER_OK, SOLVER_UNIQUE, SOLVER_MULTIPLE, SOLVER_NO_SOLUTION,
SOLVER_OVERCONSTRAINED, SOLVER_OUT_OF_SCOPE, SOLVER_TIMEOUT.

### ⚡ 复杂度标注

| 操作 | 时间 (最坏) | 空间 | 备注 |
|------|-----------|------|------|
| `solve_algebraic_system` | O(2^(2^n)) | O(2^n) | n=变量数; d<=2 时简化 |
| `solver_incremental_solve` | O(2^(2^k)) | O(2^k) | k=脏变量数 |
| `groebner_basis_compute` | O(m^2 * d^3) | O(m * d^2) | m=方程数, d=度数 |
| `solver_extract_equations_full` | O(V + E) | O(V) | |
| `count_degrees_of_freedom` | O(V + E) | O(V) | |
| `solver_handle_multiple_solutions` | O(2^k * m) | O(2^k * V) | k=二次方程数 |

### 💡 使用示例

```c
#include "lv00.h"
int main(void) {
    lv00_init();
    ConstraintGraph *g = graph_create();
    SymbolicCoord *c1[] = { symbolic_coord_create_rational(0,1), symbolic_coord_create_rational(0,1) };
    graph_add_point(g, c1, 2); int p1 = graph_get_last_added_node_id(g);
    SymbolicCoord *c2[] = { symbolic_coord_create_rational(3,1), symbolic_coord_create_rational(4,1) };
    graph_add_point(g, c2, 2); int p2 = graph_get_last_added_node_id(g);
    graph_add_line_segment(g, p1, p2); int L = graph_get_last_added_node_id(g);
    graph_add_incidence(g, p1, L); graph_add_incidence(g, p2, L);

    GroebnerResult *result = NULL;
    int dirty[] = {p1, p2};
    SolverStatus st = solve_algebraic_system(g, dirty, 2, &result);
    if (st == SOLVER_UNIQUE) printf("Unique solution: %d solutions\n", result->solution_count);
    groebner_result_free(result);

    int *free_vars; int dof = count_degrees_of_freedom(g, &free_vars);
    printf("DOF: %d\n", dof); if (free_vars) free(free_vars);
    graph_destroy(g); lv00_cleanup(); return 0;
}
```

---

## 5. 图重写引擎 (Graph Rewrite Engine)

### 📖 概念定义

图重写引擎通过**子图同构模式匹配**对约束图执行保结构变换: Pattern + Replacement -> Rewrite。
规则包含三部分: Pattern (VF2 匹配)、Replacement (新子图)、Reduction Measure (循环检测度量)。
集成了 WL 图核哈希循环检测和图快照事务回滚。

### 🏗️ 模型实现

VF2 状态机和 WL 哈希历史:

```c
typedef struct {
    int *core_1, *core_2; int core_count;
    int *in_1, *out_1, *in_2, *out_2;
    int pattern_size, target_size;
} VF2State;

typedef struct {
    uint64_t *hash_history; int history_count, history_pos;
    uint32_t *light_hash_history;
    int light_history_count, light_history_pos;
} WLHashHistory;
```

### 📝 API 参考

| 函数签名 | 说明 |
|---------|------|
| `RewriteRule* rewrite_rule_create(const char *name, RewritePattern*, RewriteReplacement*, int measure)` | 创建规则 |
| `void rewrite_rule_destroy(RewriteRule *rule)` | 销毁规则 |
| `int rewrite_rules_load_from_file(const char *path, RewriteRule ***out, int *count)` | 从 .lvz 加载 |
| `RewriteMatch* find_rewrite_match(ConstraintGraph*, RewriteRule*, bool local_eq)` | 查找匹配 |
| `RewriteStatus apply_rewrite(ConstraintGraph*, RewriteRule*, RewriteMatch*)` | 应用单条 |
| `RewriteStatus rewrite_with_rules(ConstraintGraph*, RewriteRule**, int n, int limit, bool norm)` | 到不动点 |
| `RewriteMatch* vf2_find_match(ConstraintGraph*, RewritePattern*, bool local_eq)` | VF2 算法 |
| `int find_all_non_overlapping_matches(...)` | 所有非重叠匹配 |
| `int rewrite_apply_all_matches(...)` | 批量应用 |
| `uint64_t rewrite_compute_wl_hash(const ConstraintGraph*)` | WL 哈希 |
| `RewriteStatus detect_rewrite_loop_wl(ConstraintGraph*, WLHashHistory*)` | 循环检测 |
| `GraphSnapshot* graph_snapshot_create(...)` / `graph_snapshot_restore(...)` | 事务回滚 |

### ⚡ 复杂度标注

| 操作 | 时间 (最坏) | 空间 | 备注 |
|------|-----------|------|------|
| `vf2_find_match` | O(N! * N) | O(N^2) | 实际剪枝极有效 |
| `rewrite_with_rules` | O(S * N! * N) | O(S * N^2) | S=步数 |
| `rewrite_compute_wl_hash` | O((V+E) * 3) | O(V) | 3 次 WL 迭代 |
| `detect_rewrite_loop_wl` | O(V+E) | O(64*V) | 固定历史窗口 |
| `find_all_non_overlapping_matches` | O(M * N! * N) | O(M * N^2) | |

### 💡 使用示例

```c
#include "lv00.h"
int main(void) {
    lv00_init();
    ConstraintGraph *g = graph_create();
    SymbolicCoord *c1[] = { symbolic_coord_create_rational(0,1), symbolic_coord_create_rational(0,1) };
    graph_add_point(g, c1, 2); SymbolicCoord *c2[] = { symbolic_coord_create_rational(1,1), symbolic_coord_create_rational(0,1) };
    graph_add_point(g, c2, 2);

    RewritePattern pat = { .variable_node_ids=(int[]){0,1}, .var_count=2, .pattern_constraints=NULL, .pattern_constraint_count=0 };
    RewriteReplacement repl = {0};
    RewriteRule *rule = rewrite_rule_create("simplify", &pat, &repl, 1);
    RewriteMatch *match = find_rewrite_match(g, rule, true);
    if (match) { apply_rewrite(g, rule, match); free(match); }

    WLHashHistory hist; wl_history_init(&hist);
    printf("WL: 0x%016llx\n", (unsigned long long)rewrite_compute_wl_hash(g));
    wl_history_destroy(&hist);
    rewrite_rule_destroy(rule); graph_destroy(g); lv00_cleanup(); return 0;
}
```

---

## 6. 合一检查 (Unification Check)

### 📖 概念定义

合一检查验证**构造图**是否满足**命题模式图**。采用三层匹配:

1. 模板层: 检查节点类型和拓扑同构
2. 端口约束层: 验证端口类型兼容性和约束对应
3. 坐标等价层: 验证符号坐标相等

失败时通过 `UnifyFailureInfo` 提供详细位置和原因报告。

### 🏗️ 模型实现

三个版本: 基础版 (仅约束检查)、增强版 (加坐标检查)、优化版 (哈希预过滤)。
精细化匹配函数 (`unify_match_ports`, `unify_match_constraints`, `unify_match_coords`) 提供独立分层检查。

### 📝 API 参考

| 函数签名 | 说明 |
|---------|------|
| `UnifyStatus unify_construction_with_proposition(ConstraintGraph *constr, ConstraintGraph *prop)` | 基础合一 |
| `UnifyStatus unify_construction_with_proposition_coord(ConstraintGraph *constr, ConstraintGraph *prop)` | 带坐标 |
| `UnifyStatus unify_construction_with_proposition_hash_filtered(ConstraintGraph *constr, ConstraintGraph *prop)` | 哈希优化 |
| `UnifyStatus unify_construction_with_proposition_detailed(ConstraintGraph*, ConstraintGraph*, UnifyFailureInfo*)` | 详细报告 |
| `int unify_match_ports(const ConstraintGraph*, const ConstraintGraph*, int *out_bindings)` | 端口匹配 |
| `int unify_match_constraints(const ConstraintGraph*, const ConstraintGraph*, int *out_bindings)` | 约束匹配 |
| `int unify_match_coords(const SymbolicCoord *c1, const SymbolicCoord *c2)` | 坐标判等 |
| `void unify_failure_info_destroy(UnifyFailureInfo *info)` | 释放失败信息 |
| `bool unify_declare_proposition_equivalence(...)` / `unify_find_equivalent_proposition(...)` / `unify_clear_equivalences()` | 命题等价 |
| `bool unify_instantiate_proposition(...)` | 实例化多态命题 |

**UnifyStatus**: OK, PORT_TYPE_MISMATCH, CONSTRAINT_MISMATCH, COORD_MISMATCH,
STRUCTURE_MISMATCH, SCOPE_MISMATCH, FAILED.

### ⚡ 复杂度标注

| 操作 | 时间 | 空间 | 备注 |
|------|------|------|------|
| `unify_construction_with_proposition` | O(|V_p|*|V_c| + |E_p|*|E_c|) | O(|V_p|+|E_p|) | |
| `unify_..._coord` | O(|V_p|*|V_c|*C + ...) | O(|V_p|+|E_p|) | C=坐标比较开销 |
| `unify_..._hash_filtered` | O(|V_p|*H + |E_p|*|E_c|) | O(|V_p|+|E_p|) | H=桶大小 |
| `unify_instantiate_proposition` | O(V + E) | O(V + E) | |

### 💡 使用示例

```c
#include "lv00.h"
int main(void) {
    lv00_init();
    ConstraintGraph *c = graph_create();
    SymbolicCoord *c1[] = { symbolic_coord_create_rational(0,1), symbolic_coord_create_rational(0,1) };
    graph_add_point(c, c1, 2); SymbolicCoord *c2[] = { symbolic_coord_create_rational(1,1), symbolic_coord_create_rational(0,1) };
    graph_add_point(c, c2, 2); graph_add_line_segment(c, 0, 1); graph_add_incidence(c, 0, 2); graph_add_incidence(c, 1, 2);

    ConstraintGraph *p = graph_create();
    SymbolicCoord *p1[] = { symbolic_coord_create_rational(0,1), symbolic_coord_create_rational(0,1) };
    graph_add_point(p, p1, 2); SymbolicCoord *p2[] = { symbolic_coord_create_rational(1,1), symbolic_coord_create_rational(0,1) };
    graph_add_point(p, p2, 2); graph_add_line_segment(p, 0, 1); graph_add_incidence(p, 0, 2); graph_add_incidence(p, 1, 2);

    UnifyStatus st = unify_construction_with_proposition(c, p);
    printf("Unify: %s\n", st == UNIFY_STATUS_OK ? "OK" : "FAIL");
    if (st != UNIFY_STATUS_OK) {
        UnifyFailureInfo fi;
        unify_construction_with_proposition_detailed(c, p, &fi);
        printf("Reason: %s\n", fi.reason_detail);
        unify_failure_info_destroy(&fi);
    }
    graph_destroy(c); graph_destroy(p); lv00_cleanup(); return 0;
}
```

---

## 7. 函数块系统 (Function Block System)

### 📖 概念定义

函数块 (FuncBlock) 是可复用的几何构造单元，遵循 Pack-Instantiate-Compose 三阶段流水线:

1. **Pack**: 封装内部节点、端口和跨边界约束
2. **Instantiate**: 给定实参，展开到目标图
3. **Compose/Product**: g o f 组合或 f x g 并行

集成**确定性检查机制**: 静态分析 + 动态验证确保构造唯一解。

### 🏗️ 模型实现

```c
struct FuncBlock {
    int id; int *internal_node_ids; int internal_node_count;
    int *input_port_ids, *output_port_ids; int input_count, output_count;
    DeterminismState determinism;  /* UNVERIFIED -> VERIFIED | NON_DETERMINISTIC | PARTIALLY_VERIFIED */
    SolutionSelector *selector;
    PortDependency *port_deps; int port_dep_count;
    int *precondition_region_ids; int precondition_count;
    bool has_measure; int measure_node_id;
    char *name, *description;
};
```

### 📝 API 参考

| 函数签名 | 说明 |
|---------|------|
| `FuncBlock* func_block_create(int id)` / `void func_block_destroy(FuncBlock *fb)` | 创建/销毁 |
| `FuncBlock* func_block_copy(const FuncBlock *src)` | 深拷贝 |
| `PackResult func_block_pack(ConstraintGraph*, const int *internal, int ni, const int *inputs, int ni2, const int *outputs, int no, CrossBoundaryAction *cba, int nc, FuncBlock **out)` | 打包 |
| `PackResult func_block_pack_ex(ConstraintGraph*, const PackConfig*, FuncBlock **out)` | 简化打包 |
| `bool func_block_detect_cross_boundary(ConstraintGraph*, const int *internal, int n, CrossBoundaryConstraint **out, int *count)` | 跨界检测 |
| `DeterminismStatus func_block_determinism_check_static(FuncBlock*, const ConstraintGraph*)` | 静态分析 |
| `DeterminismStatus func_block_determinism_check_dynamic(FuncBlock*, ConstraintGraph*, const SymbolicCoord **inputs, int n)` | 动态验证 |
| `DeterminismState func_block_verify_determinism(FuncBlock*, ConstraintGraph*, int step_limit)` | 完整流水线 |
| `InstantiateResult func_block_instantiate(FuncBlock*, ConstraintGraph*, int *mappings, int n, int **new_ids, int *new_count)` | 标准例化 |
| `InstantiateResult func_block_instantiate_capture_avoiding(...)` | 捕获避免例化 |
| `bool func_block_partial_apply(FuncBlock*, ConstraintGraph*, int *fixed, int n, FuncBlock **out)` | 柯里化 |
| `bool func_block_compose(FuncBlock *f, FuncBlock *g, ConstraintGraph*, FuncBlock **out)` | g o f |
| `bool func_block_product(FuncBlock *f, FuncBlock *g, ConstraintGraph*, FuncBlock **out)` | f x g |
| `SolutionSelector* selector_create(SelectorType type)` / `selector_destroy(...)` | 选择器 |

**PackResult**: PACK_OK, PACK_CROSS_BOUNDARY_CONFLICT, PACK_INVALID_NODES, ...
**InstantiateResult**: INSTANTIATE_OK, NO_SOLUTION, MULTIPLE_SOLUTIONS, SELECTOR_NEEDED, ...

### ⚡ 复杂度标注

| 操作 | 时间 | 空间 | 备注 |
|------|------|------|------|
| `func_block_pack` | O(V + E + C) | O(V + P) | |
| `func_block_determinism_check_static` | O(V^2 + E) | O(V) | |
| `func_block_instantiate` | O(|V_fb|^2 + E_fb) | O(V_fb + E_fb) | |
| `func_block_compose` / `func_block_product` | O(|V_f| + |V_g|) | O(V_f + V_g) | |

### 💡 使用示例

```c
#include "lv00.h"
int main(void) {
    lv00_init();
    ConstraintGraph *g = graph_create();
    SymbolicCoord *c1[] = { symbolic_coord_create_rational(0,1), symbolic_coord_create_rational(0,1) };
    graph_add_point(g, c1, 2); int p1 = graph_get_last_added_node_id(g);
    SymbolicCoord *c2[] = { symbolic_coord_create_rational(1,1), symbolic_coord_create_rational(1,1) };
    graph_add_point(g, c2, 2); int p2 = graph_get_last_added_node_id(g);
    graph_add_line_segment(g, p1, p2);
    graph_add_port(g, PORT_INPUT, 0, -1); int in1 = graph_get_last_added_node_id(g);
    graph_add_port(g, PORT_INPUT, 0, -1); int in2 = graph_get_last_added_node_id(g);
    graph_add_port(g, PORT_OUTPUT, 0, -1); int out = graph_get_last_added_node_id(g);

    FuncBlock *fb = NULL;
    int internal[] = {p1, p2, graph_get_last_added_node_id(g) - 2};
    int inputs[] = {in1, in2}, outputs[] = {out};
    PackResult pr = func_block_pack(g, internal, 3, inputs, 2, outputs, 1, NULL, 0, &fb);
    if (pr == PACK_OK) {
        printf("Pack OK, det: %s\n", determinism_state_to_string(func_block_determinism_check_static(fb, g)));
        SymbolicCoord *ac[] = { symbolic_coord_create_rational(5,1), symbolic_coord_create_rational(5,1) };
        graph_add_point(g, ac, 2); int arg = graph_get_last_added_node_id(g);
        int *new_nodes, new_count;
        if (func_block_instantiate(fb, g, (int[]){arg}, 1, &new_nodes, &new_count) == INSTANTIATE_OK) {
            printf("Instantiated: %d new nodes\n", new_count); free(new_nodes);
        }
        func_block_destroy(fb);
    }
    graph_destroy(g); lv00_cleanup(); return 0;
}
```

---

## 8. 类型系统 (Type System)

### 📖 概念定义

类型系统为 Lv-00 几何对象提供多层级语义。核心包括: 9 种基础类型 (POINT, LINE_SEGMENT, REGION,
FUNCTION, PRODUCT, SUM, VARIABLE, DEPENDENT, BOTTOM)、宇宙层级 (第 0 层为基本几何体)、依赖类型 Pi(x:A).B(x)。
类型通过外部映射表附加到节点，不侵入 GeomNode 结构。

### 🏗️ 模型实现

```c
struct TypeRegion {
    int id; TypeKind kind; UniverseLevel level;
    TypeRegion *input_type, *output_type;  /* FUNCTION */
    TypeRegion *left_type, *right_type;    /* PRODUCT */
    TypeRegion *first_type, *second_type;  /* SUM */
    int variable_id; char *variable_name;  /* VARIABLE */
    int param_node_id; TypeRegion *body_type; /* DEPENDENT */
};
struct TypeSystem {
    TypeRegion **type_regions; int type_region_count;
    NodeTypeMapping *node_type_mappings;  /* node_id -> TypeRegion* */
    TypeInferenceRule *inference_rules;
};
```

### 📝 API 参考

| 函数签名 | 说明 |
|---------|------|
| `TypeSystem* type_system_create(void)` / `type_system_destroy(TypeSystem*)` | 生命周期 |
| `TypeRegion* type_create_point(TypeSystem*)` / `type_create_line_segment(...)` / `type_create_region(...)` | 基本类型 |
| `TypeRegion* type_create_function(TypeSystem*, TypeRegion *in, TypeRegion *out)` | A->B |
| `TypeRegion* type_create_product(TypeSystem*, TypeRegion *l, TypeRegion *r)` | A x B |
| `TypeRegion* type_create_sum(TypeSystem*, TypeRegion *f, TypeRegion *s)` | A + B |
| `TypeRegion* type_create_variable(TypeSystem*, const char *name)` | 类型变量 |
| `TypeRegion* type_create_dependent(TypeSystem*, int param_id, TypeRegion *body)` | Pi(x).B |
| `TypeRegion* type_create_bottom(TypeSystem*)` | bottom |
| `void type_region_destroy(TypeRegion*)` | 销毁 |
| `TypeEquivResult type_check_equivalence(TypeSystem*, TypeRegion*, TypeRegion*, bool use_rewrite)` | 等价 |
| `TypeCheckResult type_check_port_compatibility(TypeSystem*, TypeRegion*, TypeRegion*)` | 端口兼容 |
| `bool type_infer_node(TypeSystem*, ConstraintGraph*, int node_id, TypeRegion **out)` | 节点推断 |
| `bool type_attach_to_node(TypeSystem*, int node_id, TypeRegion*)` / `type_get_node_type(...)` | 映射 |
| `bool type_instantiate_variable(TypeSystem*, int var_id, TypeRegion *concrete)` | 变量实例化 |
| `int type_system_register_inference_rule(TypeSystem*, int src, int tgt, int pri, const char *desc)` | 规则注册 |
| `PathExplorer* path_explorer_create(TypeSystem*, TypeRegion *cur, TypeRegion *tgt)` / `apply_rule` / `undo` | 路径探索 |

### ⚡ 复杂度标注

| 操作 | 时间 | 空间 | 备注 |
|------|------|------|------|
| `type_create_*` | O(1) | O(1) | |
| `type_check_equivalence` | O(K*R) / O(K*S*R) | O(D) | K=深度; R=规则数 |
| `type_check_port_compatibility` | O(1) | O(1) | |
| `type_infer_node` | O(1) | O(1) | |
| `type_attach_to_node` / `type_get_node_type` | O(log M) | O(1) | M=映射数 |

### 💡 使用示例

```c
#include "lv00.h"
int main(void) {
    lv00_init();
    TypeSystem *ts = type_system_create();
    TypeRegion *pt = type_create_point(ts);
    TypeRegion *fn = type_create_function(ts, pt, type_create_line_segment(ts));
    TypeEquivResult eq = type_check_equivalence(ts, pt, pt, false);
    printf("Point==Point: %s\n", eq == TYPE_EQUIV_OK ? "yes" : "no");

    ConstraintGraph *g = graph_create();
    SymbolicCoord *c[] = { symbolic_coord_create_rational(0,1), symbolic_coord_create_rational(0,1) };
    graph_add_point(g, c, 2); int pid = graph_get_last_added_node_id(g);
    TypeRegion *inf = NULL;
    if (type_infer_node(ts, g, pid, &inf)) { type_attach_to_node(ts, pid, inf); printf("type: %s\n", type_kind_to_string(inf->kind)); }
    graph_destroy(g); type_system_destroy(ts); lv00_cleanup(); return 0;
}
```

---

## 9. 命题与证明系统 (Proof System)

### 📖 概念定义

命题与证明系统将几何命题编码为带输入端口的约束图模式 (Proposition Pattern)，
通过证明导航器 (ProofNavigator) 以步骤链形式组织证明。集成八层信任颜色体系
(GREEN 全构造 -> DARK_ORANGE 非构造+数值叠加)、爆炸原理和不可构造性检查。

### 🏗️ 模型实现

```c
struct Proposition {
    int id; PropositionType type; ProofColor color;
    int *input_port_ids, *output_port_ids;
    ConstraintGraph *pattern;
    Proposition **sub_props; int sub_prop_count;
};
struct ProofStep {
    int id; ProofStepType type; ProofColor color;
    int node_id, constraint_id, rule_id, func_block_id;
    int *dependency_step_ids; int dependency_count;
    bool is_breakpoint, is_completed;
};
struct ProofNavigator {
    ProofStep **steps; int step_count, current_step;
    Proposition *target_prop; ConstraintGraph *construction;
    ProofDependency *dep_tree;
    bool is_complete; ProofColor final_color;
    int *breakpoint_indices;
    LV00Engine *engine;
};
```

### 📝 API 参考

| 函数签名 | 说明 |
|---------|------|
| `Proposition* proposition_create(int id, PropositionType type)` / `proposition_destroy(...)` | 命题管理 |
| `bool proposition_set_pattern(Proposition*, ConstraintGraph*)` | 设置模式图 |
| `bool proposition_set_input_ports(...)` / `set_output_ports(...)` | 端口设置 |
| `UnifyStatus proof_unify(ConstraintGraph*, Proposition*, bool normalize_first)` | 合一 |
| `UnifyStatus proof_unify_detailed(ConstraintGraph*, Proposition*, char **out)` | 详细合一 |
| `ProofStep* proof_step_create(ProofStepType type)` / `proof_step_destroy(...)` | 步骤 |
| `ProofNavigator* proof_navigator_create(Proposition*, LV00Engine*)` / `destroy(...)` | 导航器 |
| `bool proof_navigator_add_step(ProofNavigator*, ProofStep*)` | 添加步骤 |
| `bool proof_navigator_next(ProofNavigator*)` / `prev(...)` / `goto(int)` | 导航 |
| `ProofStep* proof_navigator_current_step(ProofNavigator*)` | 当前步骤 |
| `ProofColor proof_navigator_compute_final_color(ProofNavigator*)` | 最终颜色 |
| `bool proof_interactive_step(ProofNavigator*, ProofStepType, const void *data)` | 交互式 |
| `bool proof_apply_ex_falso(ProofNavigator*, ConstraintGraph *bottom, Proposition *target)` | 爆炸原理 |
| `UnconstructResult proof_check_unconstructibility(ProofNavigator*, const ConstraintGraph*, const Proposition*, UnconstructInfo*)` | 不可构造 |
| `UnconstructResult proof_attempt_unconstructibility(...)` | 多策略不可构造 |
| `Proposition* proof_instantiate_proposition(const Proposition*, const int *mappings, int count)` | 多态实例化 |
| `bool proof_export_html(ProofNavigator*, const char*)` / `export_latex` / `export_coq` | 导出 |
| `int proof_validate_dependencies(ProofNavigator*, DependencyUpdateResult*, int max)` | 验证依赖 |

### ⚡ 复杂度标注

| 操作 | 时间 | 空间 | 备注 |
|------|------|------|------|
| `proof_navigator_compute_final_color` | O(S^2) | O(S) | S=步骤数 |
| `proof_export_*` | O(S*(V+E)) | O(S*(V+E)) | |
| `proof_check_unconstructibility` | O(K*U) | O(K) | K=已知问题数 |
| `proof_validate_dependencies` | O(N*H) | O(N) | |

### 💡 使用示例

```c
#include "lv00.h"
int main(void) {
    lv00_init();
    Proposition *prop = proposition_create(1, PROPOSITION_ATOMIC);
    ConstraintGraph *pat = graph_create();
    SymbolicCoord *v1[] = { symbolic_coord_create_rational(0,1), symbolic_coord_create_rational(0,1) };
    graph_add_point(pat, v1, 2); SymbolicCoord *v2[] = { symbolic_coord_create_rational(1,1), symbolic_coord_create_rational(0,1) };
    graph_add_point(pat, v2, 2); proposition_set_pattern(prop, pat);

    ProofNavigator *nav = proof_navigator_create(prop, NULL);
    ProofStep *s1 = proof_step_create(PROOF_STEP_ADD_NODE); s1->node_id = 0;
    proof_navigator_add_step(nav, s1);
    proof_navigator_goto(nav, 0);
    printf("Step: %s, Color: %s\n",
        proof_step_type_to_string(proof_navigator_current_step(nav)->type),
        proof_color_to_string(proof_navigator_compute_final_color(nav)));
    proof_navigator_destroy(nav); proposition_destroy(prop); graph_destroy(pat);
    lv00_cleanup(); return 0;
}
```

---

## 10. 公理包系统 (Axiom Package System)

### 📖 概念定义

公理包系统提供可插拔的公理/定理管理。通过约束模板 (ConstraintTemplate) 参数化构造，
注册已知不可构造问题 (三等分角/倍立方/化圆为方等)，使用 SHA-256 完整性验证，
依赖引用追踪支持跨包引用自动降级 (GREEN->AMBER)。

### 🏗️ 模型实现

```c
struct AxiomPackage {
    char *name; char *version;
    ConstraintTemplate *templates; int template_count;
    KnownUnconstructible *known_unconstructibles; int unconstructible_count;
    TemplateExpansionCache *expansion_cache;
    DependencyRef *dep_refs; int dep_ref_count;
};
```

### 📝 API 参考

| 函数签名 | 说明 |
|---------|------|
| `AxiomPackage* axiom_package_create(const char *name, const char *version)` / `destroy(...)` | 生命周期 |
| `AxiomLoadStatus axiom_package_load(AxiomPackage*, const char *filepath)` | 加载 |
| `AxiomSaveStatus axiom_package_save(const AxiomPackage*, const char *filepath)` | 保存 |
| `char* axiom_package_compute_content_hash(AxiomPackage*)` | SHA-256 |
| `bool axiom_package_validate_dependencies(AxiomPackage*, AxiomPackage**, int count)` | 依赖验证 |
| `bool axiom_package_register_template(AxiomPackage*, ConstraintTemplate*)` | 注册模板 |
| `ConstraintTemplate* axiom_package_get_template(AxiomPackage*, const char *name)` | 查找模板 |
| `bool axiom_package_add_known_unconstructible(AxiomPackage*, KnownUnconstructible*)` | 不可构造 |
| `int axiom_package_register_dependency_ref(AxiomPackage*, const char *ref, const char *hash, int node_id)` | 注册依赖 |
| `int axiom_package_validate_dependencies_with_hashes(AxiomPackage*, DependencyRef**, int*)` | 哈希验证 |
| `int axiom_package_auto_degrade_invalidated(AxiomPackage*, ConstraintGraph*)` | 自动降级 |
| `TemplateTestResult axiom_template_run_tests(AxiomPackage*, const char*, TemplateTestCase*, int, TemplateTestCase*, int)` | 双层测试 |

### ⚡ 复杂度标注

| 操作 | 时间 | 空间 | 备注 |
|------|------|------|------|
| `axiom_package_load` | O(N) | O(N) | N=文件大小 |
| `axiom_package_compute_content_hash` | O(T+U+D) | O(T+U+D) | SHA-256 |
| `axiom_package_get_template` | O(T) | O(1) | 线性搜索 |
| `axiom_package_validate_dependencies_with_hashes` | O(D*H) | O(D) | |
| `axiom_package_auto_degrade_invalidated` | O(D*H + I*V) | O(D+I) | |

### 💡 使用示例

```c
#include "lv00.h"
int main(void) {
    lv00_init();
    AxiomPackage *pkg = axiom_package_create("Euclidean Plane", "1.0.0");
    KnownUnconstructible ku = { .name = "三等分角", .reduces_to = "三次方程", .green_verified = true };
    axiom_package_add_known_unconstructible(pkg, &ku);

    char *hash = axiom_package_compute_content_hash(pkg);
    printf("SHA-256: %s\n", hash); free(hash);

    axiom_package_register_dependency_ref(pkg, "ref:group:v1.0", "a1b2c3...", 42);
    int inv_c = 0; DependencyRef *inv = NULL;
    axiom_package_validate_dependencies_with_hashes(pkg, &inv, &inv_c);
    printf("Invalidated: %d\n", inv_c); if (inv) free(inv);

    axiom_package_destroy(pkg);
    lv00_cleanup(); return 0;
}
```

---

## 附录 A: 系统生命周期

| 函数签名 | 说明 |
|---------|------|
| `bool lv00_init(void)` | 初始化 Lv-00 系统 |
| `void lv00_cleanup(void)` | 清理，释放所有资源 |
| `const char* lv00_get_version(void)` | 获取版本字符串 ("3.0.1") |
| `int lv00_get_system_info(char *info, size_t size)` | 系统状态信息 |
| `int lv00_health_check(void)` | 健康检查 (0~100) |
| `bool lv00_is_initialized(void)` | 检查初始化状态 |

## 附录 B: 引擎便捷 API

| 函数签名 | 说明 |
|---------|------|
| `LV00Engine* lv00_engine_create(void)` | 创建引擎 |
| `void lv00_engine_destroy(LV00Engine *engine)` | 销毁引擎 |
| `int lv00_add_point(LV00Engine*, int64_t xn, uint64_t xd, int64_t yn, uint64_t yd)` | 快速添加有理点 |
| `int lv00_add_line_segment(LV00Engine*, int p1, int p2)` | 快速添加线段 |
| `bool lv00_add_constraint_incidence(LV00Engine*, int pid, int lid)` | 快速添加关联 |
| `NormalizationResult* lv00_normalize(LV00Engine*, bool scope_aware)` | 便捷归一化 |
| `EngineSolveResult lv00_solve(LV00Engine*)` | 便捷求解 |

## 附录 C: 内存管理规则

1. **创建-销毁配对**: `*_create` -> `*_destroy`
2. **图中的坐标**: `graph_add_point` 后坐标由图管理，不单独销毁
3. **打包输出**: `func_block_pack` 的 `out_func_block` 由调用者 `func_block_destroy`
4. **输出数组**: `**out_` 参数由调用者 `free()` 或对应 destroy 函数释放
5. **序列化字符串**: `*_serialize` 返回的字符串由调用者 `free()`
6. **系统配对**: `lv00_init()` / `lv00_cleanup()` 必须配对
7. **GMP 数据**: 销毁时自动调用 `mpq_clear` / `mpz_clear`

## 附录 D: 错误处理模式

```c
/* 图操作 */
AddNodeResult nr = graph_add_point(g, coords, 2);
if (nr != ADD_NODE_OK)
    fprintf(stderr, "失败: %s\n", lv00_error_code_to_string(lv00_add_node_result_to_error(nr)));

/* 打包 */
PackResult pr = func_block_pack(g, internal, 3, inputs, 2, outputs, 1, NULL, 0, &fb);
if (pr != PACK_OK) fprintf(stderr, "打包失败: %s\n", pack_result_to_string(pr));

/* 例化 */
InstantiateResult ir = func_block_instantiate(fb, g, mappings, 1, &new_nodes, &new_count);
if (ir != INSTANTIATE_OK) fprintf(stderr, "例化失败: %s\n", instantiate_result_to_string(ir));

/* 求解 */
GroebnerResult *result = NULL;
if (solve_algebraic_system(g, NULL, 0, &result) == SOLVER_NO_SOLUTION)
    fprintf(stderr, "无解\n");

/* 合一详细报告 */
UnifyFailureInfo info;
UnifyStatus us = unify_construction_with_proposition_detailed(constr, prop, &info);
if (us != UNIFY_STATUS_OK) {
    fprintf(stderr, "合一失败: %s (%s)\n", info.description, info.reason_detail);
    unify_failure_info_destroy(&info);
}
```
