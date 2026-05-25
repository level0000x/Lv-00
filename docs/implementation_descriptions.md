# Lv-00 全模块实现描述文档

> 本文档基于 `design_v2.9.md` 功能规格说明书和当前代码库实现状态编写，
> 覆盖全部 14 个待完善模块。每个模块包含函数签名、算法描述、输入/输出定义、
> 与设计规格的对应关系及模块间依赖关系，供开发者直接据此编码实现。

---

## 目录

1. [模块 1: symbolic_coord（符号坐标系统）](#模块-1-symbolic_coord符号坐标系统)
2. [模块 2: constraint_graph（约束图核心）](#模块-2-constraint_graph约束图核心)
3. [模块 3: normalization（图规范化遍引擎）](#模块-3-normalization图规范化遍引擎)
4. [模块 4: solver（符号代数求解器）](#模块-4-solver符号代数求解器)
5. [模块 5: rewrite（图重写引擎）](#模块-5-rewrite图重写引擎)
6. [模块 6: unify（合一检查系统）](#模块-6-unify合一检查系统)
7. [模块 7: func_block（函数块系统）](#模块-7-func_block函数块系统)
8. [模块 8: type_system（类型系统）](#模块-8-type_system类型系统)
9. [模块 9: proof（命题与证明系统）](#模块-9-proof命题与证明系统)
10. [模块 10: recursion（递归与条件系统）](#模块-10-recursion递归与条件系统)
11. [模块 11: engine（引擎核心）](#模块-11-engine引擎核心)
12. [模块 12: axiom_pkg（公理包系统）](#模块-12-axiom_pkg公理包系统)
13. [模块 13: module（模块系统）](#模块-13-module模块系统)
14. [模块 14: debug（调试与性能系统）](#模块-14-debug调试与性能系统)

---

## 模块 1: symbolic_coord（符号坐标系统）

### 当前完成度: 70%

### 待实现功能清单

| 编号 | 功能名称 | 优先级 |
|------|----------|--------|
| 1.1 | `try_priority_rationalization_enhanced()` | 高 |
| 1.2 | `refine_algebraic_precision()` | 高 |
| 1.3 | `algebraic_refine_for_equality()` | 高 |
| 1.4 | `check_nested_sqrt_expandable()` | 中 |
| 1.5 | `symbolic_coord_negate()` | 中 |
| 1.6 | `symbolic_coord_hash()` | 中 |
| 1.7 | 增强 `try_priority_rationalization` 连分式逼近 | 高 |

---

### 1.1 `try_priority_rationalization_enhanced()`

**函数签名：**

```c
/**
 * @brief 增强版优先有理化通路——使用连分式逼近
 *
 * 对结果代数数计算连分式逼近（精度取当前隔离区间宽度的 1/4），
 * 产生有理数候选值 r_approx。在符号层将 r_approx 代入极小多项式
 * 精确求值。若多项式值为零，则该代数数等价于 r_approx。
 *
 * @param coord 待有理化的符号坐标（必须为 ALGEBRAIC 类型）
 * @param out_rational 输出：若成功有理化，返回等价的有理数坐标
 * @return 0 成功有理化，1 无法有理化，-1 错误
 */
int try_priority_rationalization_enhanced(const SymbolicCoord *coord,
                                          SymbolicCoord **out_rational);
```

**算法描述：**

1. 检查 `coord->type == ALGEBRAIC`，否则返回错误。
2. 获取代数数的隔离区间 `[left, right]`，计算区间宽度 `w = right - left`。
3. 以区间中点 `mid = (left + right) / 2` 为起点，精度 `eps = w / 4`。
4. 对 `mid` 执行连分式展开（continued fraction expansion），截断到分母比特数不超过 `BIT_CUTOFF_THRESHOLD / 2` 的最佳有理逼近 `r_approx`。
5. 将 `r_approx` 代入代数数的极小多项式 `p(x)`，使用 GMP 精确整数运算计算 `p(r_approx)`。
6. 若 `p(r_approx) == 0`（精确零），则：
   - 创建新的 `SymbolicCoord`（类型 `RATIONAL`），值为 `r_approx`。
   - 设置 `*out_rational` 为新坐标。
   - 返回 0。
7. 否则返回 1（无法有理化）。

**输入/输出定义：**

| 参数 | 方向 | 类型 | 说明 |
|------|------|------|------|
| coord | 输入 | `const SymbolicCoord*` | 待有理化的代数坐标 |
| out_rational | 输出 | `SymbolicCoord**` | 成功时返回等价有理坐标 |
| 返回值 | 输出 | `int` | 0=成功, 1=无法有理化, -1=错误 |

**设计规格对应：** `design_v2.9.md` 第 1.2 节 "优先有理化通路"。

**依赖模块：** GMP（mpq_t, mpz_t）。

---

### 1.2 `refine_algebraic_precision()`

**函数签名：**

```c
/**
 * @brief 惰性精度提升——加倍隔离区间精度
 *
 * 当两个代数数判等时区间重叠但无法区分时调用。
 * 使用牛顿法将隔离区间精度加倍，直到足以判定不相等，
 * 或达到硬上限 MAX_PRECISION_BITS (100)。
 *
 * @param a 代数数（将被修改）
 * @param max_bits 最大精度位数上限
 * @return 0 成功提升，1 已达上限，-1 错误
 */
int refine_algebraic_precision(Algebraic *a, int max_bits);
```

**算法描述：**

1. 检查当前精度 `a->precision_bits` 是否已达 `max_bits`。若已达，返回 1。
2. 执行牛顿法迭代：对极小多项式 `p(x)` 和导数 `p'(x)`，以当前区间中点为初始值。
3. 每次迭代将区间宽度减半，精度位数 +1。
4. 迭代直到 `a->precision_bits` 翻倍或达到 `max_bits`。
5. 更新 `a->left_bound` 和 `a->right_bound`。

**设计规格对应：** `design_v2.9.md` 第 1.2 节 "隔离区间精度管理"。

**依赖模块：** GMP, `mpz_poly.h`。

---

### 1.3 `algebraic_refine_for_equality()`

**函数签名：**

```c
/**
 * @brief 为代数数判等进行精度提升
 *
 * 比较两个代数数时，若极小多项式相同但隔离区间重叠，
 * 反复加倍精度直到区间不再重叠（判定为不同根）或
 * 确认区间包含同一根（判定为相同）。
 *
 * @param a 第一个代数数
 * @param b 第二个代数数
 * @return 0 相同代数数，1 不同代数数，-1 无法判定
 */
int algebraic_refine_for_equality(Algebraic *a, Algebraic *b);
```

**算法描述：**

1. 先比较极小多项式（逐系数判等）。若不同，返回 1（不同代数数）。
2. 若多项式相同，检查隔离区间是否相交：
   - 若 `a->right_bound < b->left_bound` 或 `b->right_bound < a->left_bound`：不相交，返回 1。
   - 若区间完全包含（一个区间是另一个的子集），返回 0（同一根）。
3. 若区间重叠但无法判定：交替对 `a` 和 `b` 调用 `refine_algebraic_precision()`。
4. 重复步骤 2-3 直到可以判定或达到精度上限。
5. 达到精度上限仍无法判定时返回 -1。

**设计规格对应：** `design_v2.9.md` 第 1.2 节 "判等"。

**依赖模块：** `refine_algebraic_precision()`。

---

### 1.4 `check_nested_sqrt_expandable()`

**函数签名：**

```c
/**
 * @brief 检查 sqrt(a + b*sqrt(n)) 是否可展开为规范的二次根式
 *
 * 若 a^2 - b^2*n 是完全平方数 k^2，则：
 *   sqrt(a + b*sqrt(n)) = sqrt((a+k)/2) + sqrt((a-k)/2) * sign(b)
 * 可展开为两个二次根式之和。
 *
 * @param a 系数 a（有理数）
 * @param b 系数 b（有理数）
 * @param n 根号内的无平方因子正整数
 * @param out_expanded 输出：展开后的二次根式（若可展开）
 * @return true 可展开，false 不可展开
 */
bool check_nested_sqrt_expandable(const Rational *a, const Rational *b,
                                  unsigned int n,
                                  SymbolicCoord **out_expanded);
```

**算法描述：**

1. 计算 `discriminant = a^2 - b^2 * n`（使用 GMP 精确有理运算）。
2. 检查 `discriminant` 是否为完全平方数：取分子和分母分别检查是否为完全平方。
3. 若是完全平方数 `k^2`：
   - 计算 `term1 = (a + k) / 2`，`term2 = (a - k) / 2`。
   - 若 `term1 >= 0` 且 `term2 >= 0`：结果为 `sqrt(term1) + sign(b) * sqrt(term2)`。
   - 创建对应的二次根式坐标。
4. 否则返回 false。

**设计规格对应：** `design_v2.9.md` 第 1.2 节 "平方根嵌套处理"。

**依赖模块：** GMP, `Rational`, `Quadratic`。

---

### 1.5 `symbolic_coord_negate()`

**函数签名：**

```c
/**
 * @brief 符号坐标的一元取反
 *
 * @param coord 输入坐标
 * @return 新的取反后坐标（调用者负责释放）
 */
SymbolicCoord *symbolic_coord_negate(const SymbolicCoord *coord);
```

**算法描述：**

根据坐标类型分发：
- `RATIONAL`: 返回 `rational_create(-numerator, denominator)`。
- `ALGEBRAIC`: 构造新极小多项式 `p(-x)`（交替变号系数），隔离区间取反。
- `QUADRATIC`: 返回 `quadratic_create(-a, -b, n)`。
- `TRANSCENDENTAL`: 返回 `transcendental_create(name)` 并标记为负值（需扩展 Transcendental 结构或使用包装类型）。

**设计规格对应：** 基本代数运算需求。

**依赖模块：** `Rational`, `Algebraic`, `Quadratic`。

---

### 1.6 `symbolic_coord_hash()`

**函数签名：**

```c
/**
 * @brief 计算符号坐标的哈希值（用于规范化分组）
 *
 * 基于符号坐标的规范序列化字符串计算 FNV-1a 哈希。
 *
 * @param coord 输入坐标
 * @return 64 位哈希值
 */
uint64_t symbolic_coord_hash(const SymbolicCoord *coord);
```

**算法描述：**

1. 调用 `symbolic_coord_serialize(coord)` 获取规范字符串。
2. 对字符串执行 FNV-1a 哈希（64 位版本）。
3. 释放序列化字符串，返回哈希值。

**设计规格对应：** `design_v2.9.md` 第 4.2 节 "第一阶段：点合并" 中 "按符号坐标的哈希值分组"。

**依赖模块：** `symbolic_coord_serialize()`。

---

### 1.7 增强 `try_priority_rationalization` 连分式逼近

**修改位置：** `src/symbolic_coord.c` 中现有的 `try_priority_rationalization` 函数。

**当前实现缺陷：** 当前仅检查中点值是否为精确有理数，未使用连分式逼近。

**增强算法：**

```c
/* 在现有 try_priority_rationalization 中添加连分式逼近逻辑 */

static int continued_fraction_approx(double value, double epsilon,
                                      mpq_t result) {
    /* 连分式展开：
     * a0 = floor(value)
     * r0 = value - a0
     * a1 = floor(1/r0)
     * r1 = 1/r0 - a1
     * ...
     * 收敛子 h_n/k_n 给出最佳有理逼近
     */
    mpz_t h_prev, h_curr, k_prev, k_curr;
    mpz_init_set_ui(h_prev, 0);
    mpz_init_set_ui(h_curr, 1);
    mpz_init_set_ui(k_prev, 1);
    mpz_init_set_ui(k_curr, 0);

    double x = value;
    mpz_t a;
    mpz_init(a);

    for (int i = 0; i < 1000; i++) {  /* 最多 1000 项 */
        double floor_x = floor(x);
        mpz_set_d(a, floor_x);

        /* 更新收敛子 */
        mpz_t h_next, k_next;
        mpz_init(h_next);
        mpz_init(k_next);
        mpz_mul(h_next, a, h_curr);
        mpz_add(h_next, h_next, h_prev);
        mpz_mul(k_next, a, k_curr);
        mpz_add(k_next, k_next, k_prev);

        /* 检查分母比特数 */
        if (mpz_sizeinbase(k_next, 2) > BIT_CUTOFF_THRESHOLD / 2) {
            mpz_clear(h_next); mpz_clear(k_next);
            break;
        }

        mpz_set(h_prev, h_curr);
        mpz_set(h_curr, h_next);
        mpz_set(k_prev, k_curr);
        mpz_set(k_curr, k_next);

        /* 检查逼近精度 */
        double approx = mpz_get_d(h_curr) / mpz_get_d(k_curr);
        if (fabs(approx - value) < epsilon) {
            mpz_set(mpq_numref(result), h_curr);
            mpz_set(mpq_denref(result), k_curr);
            mpq_canonicalize(result);
            mpz_clears(h_prev, h_curr, k_prev, k_curr, a, h_next, k_next, NULL);
            return 0;  /* 成功 */
        }

        double r = x - floor_x;
        if (fabs(r) < 1e-15) break;
        x = 1.0 / r;
    }

    mpz_clears(h_prev, h_curr, k_prev, k_curr, a, NULL);
    return -1;  /* 未找到足够精度的逼近 */
}
```

**设计规格对应：** `design_v2.9.md` 第 1.2 节 "优先有理化通路"。

---

## 模块 2: constraint_graph（约束图核心）

### 当前完成度: 60%

### 待实现功能清单

| 编号 | 功能名称 | 优先级 |
|------|----------|--------|
| 2.1 | `graph_remove_node()` 级联删除完善 | 高 |
| 2.2 | `graph_remove_constraint()` 实现 | 高 |
| 2.3 | `graph_detect_redundancy()` 线性依赖检测 | 高 |
| 2.4 | `graph_validate_region_closure()` 增强 | 中 |
| 2.5 | `graph_find_constraints_involving()` | 中 |
| 2.6 | `graph_get_node_count()` / `graph_get_constraint_count()` | 低 |

---

### 2.1 `graph_remove_node()` 级联删除完善

**函数签名：**

```c
/**
 * @brief 级联删除节点及其所有引用约束
 *
 * 删除指定节点，同时删除所有引用该节点的约束。
 * 对于 LINE_SEGMENT 节点，若它是某 REGION 的边界组成部分，
 * 则阻止删除并返回 false。
 *
 * @param graph 约束图
 * @param node_id 要删除的节点 ID
 * @return true 删除成功，false 删除被阻止
 */
bool graph_remove_node(ConstraintGraph *graph, int node_id);
```

**算法描述：**

1. 在 `graph->nodes` 中查找 `node_id`。若不存在，返回 false。
2. 获取节点 `node`，检查其类型：
   - 若为 `GEOM_LINE_SEGMENT`：遍历所有 `GEOM_REGION` 节点，检查该线段是否出现在某区域的 `boundary_segments` 中。若是，返回 false（阻止删除）。
3. 收集所有引用 `node_id` 的约束 ID 列表。
4. 从后向前逐个删除这些约束（调用内部 `remove_constraint_at_index()`）。
5. 从 `graph->nodes` 数组中移除该节点（交换到末尾并缩减数组）。
6. 释放节点内存（包括 `symbolic_coords`, `data.port`, `data.region.boundary_segments` 等）。
7. 返回 true。

**设计规格对应：** `design_v2.9.md` 第 2.3 节 "删除节点：级联删除所有引用该节点的约束"。

**依赖模块：** 无外部依赖。

---

### 2.2 `graph_remove_constraint()` 实现

**函数签名：**

```c
/**
 * @brief 删除约束记录（不删除任何节点）
 *
 * @param graph 约束图
 * @param constraint_id 要删除的约束 ID
 * @return true 删除成功，false 约束不存在
 */
bool graph_remove_constraint(ConstraintGraph *graph, int constraint_id);
```

**算法描述：**

1. 在 `graph->constraints` 中查找 `constraint_id`。若不存在，返回 false。
2. 释放约束的 `participants` 数组。
3. 从 `graph->constraints` 数组中移除该约束（交换到末尾并缩减数组）。
4. 返回 true。

**设计规格对应：** `design_v2.9.md` 第 2.3 节 "删除约束：只删除约束记录，不删除任何节点"。

**依赖模块：** 无外部依赖。

---

### 2.3 `graph_detect_redundancy()` 线性依赖检测

**函数签名：**

```c
/**
 * @brief 检测冗余约束
 *
 * 添加新约束后，检测是否与已有约束线性相关。
 * 例如：已知 AB=3, BC=4，新添 AC=7，则新约束冗余且隐含共线性。
 *
 * @param graph 约束图
 * @param out_count 输出：冗余约束的数量
 * @return 冗余约束 ID 数组（调用者负责释放），NULL 表示无冗余
 */
int *graph_detect_redundant_constraints(ConstraintGraph *graph, int *out_count);
```

**算法描述：**

1. 从约束图中提取所有距离/长度相关的代数方程。
2. 将方程系数矩阵构建为增广矩阵 `[A | b]`。
3. 对矩阵执行高斯消元（使用 GMP 有理数运算）。
4. 检查是否有行可以被其他行线性表示（即该行消元后变为全零行）。
5. 对应的约束标记为冗余。
6. 返回冗余约束 ID 数组。

```c
/* 核心伪代码 */
for each constraint c in graph->constraints:
    extract algebraic equation e from c
    add e to equation list

build coefficient matrix M from equation list
perform Gaussian elimination on M using GMP mpq_t

for i = 0 to row_count:
    if row i is all zeros after elimination:
        mark constraint[i] as redundant
```

**设计规格对应：** `design_v2.9.md` 第 2.3 节 "冗余检测"。

**依赖模块：** GMP, `symbolic_coord`。

---

### 2.4 `graph_validate_region_closure()` 增强

**函数签名：**

```c
/**
 * @brief 验证区域边界是否形成闭合环
 *
 * 检查边界线段是否首尾相接形成闭合环。
 * 每条线段的终点应连接下一条线段的起点，
 * 最后一条线段的终点连接第一条线段的起点。
 * 自交区域允许但生成警告。
 *
 * @param graph 约束图
 * @param region_id 区域节点 ID
 * @return true 闭合，false 不闭合
 */
bool graph_validate_region_closure(ConstraintGraph *graph, int region_id);
```

**增强内容：**

当前实现仅检查线段数量 > 0。增强为：

1. 获取区域的所有边界线段。
2. 对每条线段提取其两个端点坐标。
3. 验证：`segment[i].end == segment[(i+1) % n].start`（使用 `symbolic_coord_compare`）。
4. 额外检查自交：对每对非相邻线段，检查是否相交（使用参数化线段相交检测）。
5. 若自交，输出警告日志但允许创建。

**设计规格对应：** `design_v2.9.md` 第 2.3 节 "区域有效性检查"。

**依赖模块：** `symbolic_coord_compare()`。

---

### 2.5 `graph_find_constraints_involving()`

**函数签名：**

```c
/**
 * @brief 查找所有引用指定节点的约束
 *
 * @param graph 约束图
 * @param node_id 节点 ID
 * @param out_count 输出：找到的约束数量
 * @return 约束 ID 数组（调用者负责释放）
 */
int *graph_find_constraints_involving(ConstraintGraph *graph,
                                      int node_id, int *out_count);
```

**算法描述：**

遍历 `graph->constraints`，对每个约束检查其 `participants` 数组是否包含 `node_id`。收集所有匹配的约束 ID。

**设计规格对应：** 级联删除和冗余检测的辅助函数。

**依赖模块：** 无外部依赖。

---

### 2.6 `graph_get_node_count()` / `graph_get_constraint_count()`

**函数签名：**

```c
int graph_get_node_count(const ConstraintGraph *graph);
int graph_get_constraint_count(const ConstraintGraph *graph);
```

**实现：** 直接返回 `graph->node_count` 和 `graph->constraint_count`。

**设计规格对应：** `design_v2.9.md` 第 18.5 节性能计数器需要。

---

## 模块 3: normalization（图规范化遍引擎）

### 当前完成度: 40%

### 待实现功能清单

| 编号 | 功能名称 | 优先级 |
|------|----------|--------|
| 3.1 | `merge_line_segments()` 独立合并函数 | 高 |
| 3.2 | `merge_regions()` 独立合并函数 | 高 |
| 3.3 | `normalization_log` 结构和记录 | 中 |
| 3.4 | 作用域感知合并确认回调 | 中 |
| 3.5 | 幂等性验证 | 中 |

---

### 3.1 `merge_line_segments()` 独立合并函数

**函数签名：**

```c
/**
 * @brief 第二阶段：合并具有相同端点的线段
 *
 * 遍历所有 LINE_SEGMENT 节点，若两条线段的两个端点
 * （经第一阶段合并后）完全对应相等，则合并线段节点。
 *
 * @param graph 约束图（第一阶段点合并已完成）
 * @param result 规范化结果（追加线段合并信息）
 * @return 合并的线段对数量
 */
int merge_line_segments(ConstraintGraph *graph, NormalizationResult *result);
```

**算法描述：**

1. 遍历所有 `GEOM_LINE_SEGMENT` 节点。
2. 对每对线段 `(i, j)`，比较端点坐标：
   - 获取线段 `i` 的端点坐标 `(x1_i, y1_i)` 和 `(x2_i, y2_i)`。
   - 获取线段 `j` 的端点坐标 `(x1_j, y1_j)` 和 `(x2_j, y2_j)`。
   - 检查 `{(x1_i,y1_i), (x2_i,y2_i)} == {(x1_j,y1_j), (x2_j,y2_j)}`（无序对比较）。
3. 若匹配，使用并查集合并，保留最小 ID。
4. 更新所有引用被合并线段的约束。
5. 移除被合并的线段节点。

**设计规格对应：** `design_v2.9.md` 第 4.2 节 "第二阶段：线段和区域合并"。

**依赖模块：** `symbolic_coord_compare()`, `graph_remove_node()`。

---

### 3.2 `merge_regions()` 独立合并函数

**函数签名：**

```c
/**
 * @brief 第二阶段：合并具有相同边界的区域
 *
 * 遍历所有 REGION 节点，若两个区域的边界线段序列
 * （经第一、第二阶段合并后）完全对应相等，则合并区域节点。
 *
 * @param graph 约束图（第一、二阶段合并已完成）
 * @param result 规范化结果（追加区域合并信息）
 * @return 合并的区域对数量
 */
int merge_regions(ConstraintGraph *graph, NormalizationResult *result);
```

**算法描述：**

1. 遍历所有 `GEOM_REGION` 节点。
2. 对每对区域 `(i, j)`：
   - 检查 `segment_count` 是否相同。
   - 将两个区域的边界线段 ID 数组排序后逐个比较。
3. 若匹配，使用并查集合并，保留最小 ID。
4. 更新所有引用被合并区域的约束。
5. 移除被合并的区域节点。

**设计规格对应：** `design_v2.9.md` 第 4.2 节 "第二阶段：线段和区域合并"。

**依赖模块：** `graph_remove_node()`。

---

### 3.3 `normalization_log` 结构和记录

**函数签名：**

```c
/**
 * @brief 规范化日志条目
 */
typedef struct NormalizationLogEntry {
    int old_node_id;          /* 被合并的节点 ID */
    int retained_node_id;     /* 保留的代表节点 ID */
    bool auto_merged;         /* 是否自动合并（vs 用户确认） */
    GeomType node_type;       /* 节点类型 */
} NormalizationLogEntry;

typedef struct NormalizationLog {
    NormalizationLogEntry *entries;
    int entry_count;
    int capacity;
} NormalizationLog;

/**
 * @brief 创建规范化日志
 */
NormalizationLog *normalization_log_create(void);

/**
 * @brief 添加日志条目
 */
void normalization_log_add(NormalizationLog *log,
                           int old_id, int retained_id,
                           bool auto_merged, GeomType type);

/**
 * @brief 销毁规范化日志
 */
void normalization_log_destroy(NormalizationLog *log);
```

**设计规格对应：** `design_v2.9.md` 第 4.4 节 "规范化日志"。

---

### 3.4 作用域感知合并确认回调

**函数签名：**

```c
/**
 * @brief 合并确认回调函数类型
 *
 * 当规范化引擎发现跨作用域的合并候选时调用此回调。
 *
 * @param node_a_id 第一个节点 ID
 * @param node_b_id 第二个节点 ID
 * @param scope_a 第一个节点的作用域键
 * @param scope_b 第二个节点的作用域键
 * @param user_data 用户数据
 * @return true 确认合并，false 保留两者
 */
typedef bool (*MergeConfirmCallback)(int node_a_id, int node_b_id,
                                      int scope_a, int scope_b,
                                      void *user_data);

/**
 * @brief 设置合并确认回调
 */
void normalization_set_merge_callback(MergeConfirmCallback cb,
                                      void *user_data);
```

**设计规格对应：** `design_v2.9.md` 第 4.2 节 "若作用域不同，生成提示信息，由用户界面弹出确认对话框"。

---

### 3.5 幂等性验证

**函数签名：**

```c
/**
 * @brief 验证规范化结果的幂等性
 *
 * 对已规范化的图再次运行规范化，确认不产生任何变化。
 *
 * @param graph 已规范化的约束图
 * @return true 幂等性成立，false 不成立
 */
bool normalization_verify_idempotency(ConstraintGraph *graph);
```

**算法描述：**

1. 计算当前图的哈希 `hash_before = compute_complete_graph_hash(graph)`。
2. 执行 `graph_normalize(graph, true)`。
3. 计算规范化后的哈希 `hash_after = compute_complete_graph_hash(graph)`。
4. 比较 `hash_before` 和 `hash_after`。若相同，返回 true。

**设计规格对应：** `design_v2.9.md` 第 4.3 节 "幂等性保证"。

**依赖模块：** `compute_complete_graph_hash()`。

---

## 模块 4: solver（符号代数求解器）

### 当前完成度: 30%

### 待实现功能清单

| 编号 | 功能名称 | 优先级 |
|------|----------|--------|
| 4.1 | `groebner_basis_compute()` Buchberger 算法 | 高 |
| 4.2 | `solver_incremental_solve()` 增量求解 | 高 |
| 4.3 | `solver_extract_equations()` 完整方程提取 | 高 |
| 4.4 | 增强 `eliminate_geometry()` 更多模板 | 中 |

---

### 4.1 `groebner_basis_compute()` Buchberger 算法

**函数签名：**

```c
/**
 * @brief 计算多项式系统的 Gröbner 基（Buchberger 算法）
 *
 * 仅处理二次及以下的多项式系统。
 * 若检测到不可约三次及以上的方程，返回 SOLVER_OUT_OF_SCOPE。
 *
 * @param equations 输入多项式方程组
 * @param eq_count 方程数量
 * @param out_basis 输出：Gröbner 基（调用者负责释放）
 * @param out_basis_count 输出：基中多项式数量
 * @return SOLVER_OK 成功，SOLVER_OUT_OF_SCOPE 超出范围
 */
SolverStatus groebner_basis_compute(const mpz_poly_t *equations,
                                     int eq_count,
                                     mpz_poly_t **out_basis,
                                     int *out_basis_count);
```

**算法描述：**

```
Buchberger 算法（简化版，限制 degree <= 2）：

输入：多项式集合 F = {f1, f2, ..., fn}
输出：Gröbner 基 G

1. G = F
2. 对所有 i < j，将 (i, j) 加入待处理对集合 P
3. 当 P 非空时：
   a. 从 P 中取出一个对 (i, j)
   b. 计算 S-多项式 Sij = S(gi, gj)
   c. 对 G 中的每个 gk，用 gk 约简 Sij：Sij = Sij - (LCM/LT(gk)) * gk
   d. 若约简后的 Sij 不为零：
      - 检查 Sij 的次数是否 <= 2。若 > 2，返回 SOLVER_OUT_OF_SCOPE
      - 将 Sij 加入 G
      - 对所有 k，将 (|G|-1, k) 加入 P
4. 返回 G
```

**S-多项式计算：**

```c
/**
 * 计算 S-多项式
 * S(f, g) = (LCM(LT(f), LT(g)) / LT(f)) * f - (LCM(LT(f), LT(g)) / LT(g)) * g
 *
 * 对于单变量多项式，LCM 就是 x^max(deg_f, deg_g)。
 * 对于双变量多项式，需要计算首项的最小公倍数。
 */
static void compute_s_polynomial(const mpz_poly_t *f, const mpz_poly_t *g,
                                  mpz_poly_t *result);
```

**多项式约简：**

```c
/**
 * 用多项式 g 约简多项式 f
 * 反复用 g 的首项消去 f 的首项，直到 f 的首项不被 g 整除
 */
static void polynomial_reduce(mpz_poly_t *f, const mpz_poly_t *g);
```

**设计规格对应：** `design_v2.9.md` 第 5.2 节 "第二步：Gröbner 基求解"。

**依赖模块：** `mpz_poly.h`, GMP。

---

### 4.2 `solver_incremental_solve()` 增量求解

**函数签名：**

```c
/**
 * @brief 增量求解——仅对脏变量重新求解
 *
 * 维护"脏"变量集合，新的求解请求只对脏变量相关的
 * 最小依赖子图重新运行消元和求解。
 *
 * @param graph 约束图
 * @param dirty_var_ids 脏变量 ID 数组
 * @param dirty_count 脏变量数量
 * @param out_result 输出：求解结果
 * @return 求解状态
 */
SolverStatus solver_incremental_solve(ConstraintGraph *graph,
                                       int *dirty_var_ids, int dirty_count,
                                       GröbnerResult **out_result);
```

**算法描述：**

1. 构建脏变量的依赖子图：
   - 从脏变量出发，沿约束关系传播，收集所有受影响的变量。
2. 从子图中提取方程组（调用 `solver_extract_equations()`）。
3. 执行几何推理消元（调用 `eliminate_geometry()`）。
4. 对剩余方程执行 Gröbner 基求解（调用 `groebner_basis_compute()`）。
5. 合并新解与已有解（未受影响的变量保持不变）。
6. 更新脏变量集合（清空已求解的变量）。

**设计规格对应：** `design_v2.9.md` 第 5.6 节 "增量求解"。

**依赖模块：** `solver_extract_equations()`, `eliminate_geometry()`, `groebner_basis_compute()`。

---

### 4.3 `solver_extract_equations()` 完整方程提取

**函数签名：**

```c
/**
 * @brief 从约束图中完整提取代数方程
 *
 * 支持所有约束类型到代数方程的转换：
 * - INCIDENCE: 叉积为零（线性方程）
 * - INTERSECTION: 参数化线性方程组
 * - 距离约束（模板展开后）: 二次方程
 * - 平行/垂直约束: 线性关系
 *
 * @param graph 约束图
 * @param target_var_ids 目标变量节点 ID 数组（可为 NULL 表示全部）
 * @param target_count 目标变量数量
 * @param out_equations 输出：方程数组
 * @param out_eq_count 输出：方程数量
 * @return SOLVER_OK 成功
 */
SolverStatus solver_extract_equations(ConstraintGraph *graph,
                                       int *target_var_ids, int target_count,
                                       mpz_poly_t **out_equations,
                                       int *out_eq_count);
```

**算法描述：**

```
对每个约束 c in graph->constraints:
    switch c->type:
        case INCIDENCE:
            /* 点 P 在线段 AB 延长线上 */
            /* (P - A) x (B - A) = 0 */
            /* 叉积: (Px-Ax)*(By-Ay) - (Py-Ay)*(Bx-Ax) = 0 */
            提取 P, A, B 的坐标，构建线性方程

        case INTERSECTION:
            /* 两线段 L1, L2 相交 */
            /* 参数化: P = A1 + t*(B1-A1) = A2 + s*(B2-A2) */
            /* 产生两个线性方程（x 和 y 分量） */

        case BETWEENNESS:
            /* 不增加独立代数方程，仅用于多解选择 */

        case CONTAINMENT:
            /* 点在区域内：可使用射线法，但代数化较复杂 */
            /* 暂不提取方程，标记为几何约束 */

        case CONNECTION:
            /* 端口连接：不产生代数方程 */
```

**设计规格对应：** `design_v2.9.md` 第 5.1 节 "代数方程转化"。

**依赖模块：** `symbolic_coord`, `constraint_graph`, `mpz_poly.h`。

---

### 4.4 增强 `eliminate_geometry()` 更多模板

**当前实现：** 仅支持基本的相似三角形和勾股定理模板。

**增强模板列表：**

```c
/**
 * @brief 几何推理消元模板类型
 */
typedef enum {
    GEO_TEMPLATE_SIMILAR_TRIANGLES,    /* 相似三角形比例式 */
    GEO_TEMPLATE_PYTHAGOREAN,          /* 勾股定理 */
    GEO_TEMPLATE_PARALLEL_CUT,         /* 平行线截线段比例定理 */
    GEO_TEMPLATE_INSCRIBED_ANGLE,      /* 圆周角定理 */
    GEO_TEMPLATE_POWER_OF_POINT,       /* 幂定理（切割线定理） */
    GEO_TEMPLATE_ANGLE_BISECTOR        /* 角平分线定理 */
} GeoTemplateType;

/**
 * @brief 增强版几何推理消元
 *
 * 应用预定义的几何定理模板进行变量消元。
 * 优先消去可线性求解的变量。
 *
 * @param graph 约束图
 * @param target_var_id 目标消元变量
 * @param eliminate_ids 可消元的变量 ID 数组
 * @param elim_count 可消元变量数量
 * @return 消元状态
 */
SolverStatus eliminate_geometry(ConstraintGraph *graph,
                                 int target_var_id,
                                 int *eliminate_ids, int elim_count);
```

**新增模板实现示例——平行线截线段比例定理：**

```
若检测到以下模式：
- 线段 L1 // L2（平行约束）
- 线段 L3 与 L1, L2 均相交
则产生比例关系：|L3 ∩ L1 到 L3 ∩ L2 在 L3 上的截距| 满足
  |A1B1| / |A2B2| = |OA1| / |OA2|
其中 A1,B1 是 L3 与 L1 的交点，A2,B2 是 L3 与 L2 的交点。
```

**设计规格对应：** `design_v2.9.md` 第 5.2 节 "第一步：几何推理消元"。

---

## 模块 5: rewrite（图重写引擎）

### 当前完成度: 30%

### 待实现功能清单

| 编号 | 功能名称 | 优先级 |
|------|----------|--------|
| 5.1 | `rewrite_with_normalization()` 集成规范化 | 高 |
| 5.2 | `rewrite_validate_measure()` 约简测度验证 | 高 |
| 5.3 | 增强 VF2 的 `coord_equal()` 容差匹配 | 高 |
| 5.4 | 规则热加载/卸载 | 中 |

---

### 5.1 `rewrite_with_normalization()` 集成规范化

**函数签名：**

```c
/**
 * @brief 集成规范化遍的重写流程
 *
 * 在重写循环中，每次规则应用后可选地执行局部规范化，
 * 确保后续匹配不受冗余节点干扰。
 *
 * @param graph 约束图
 * @param rules 重写规则数组
 * @param rule_count 规则数量
 * @param step_limit 步数上限
 * @param normalize_between_steps 是否在步骤间执行规范化
 * @return 重写状态
 */
RewriteStatus rewrite_with_normalization(ConstraintGraph *graph,
                                          RewriteRule **rules,
                                          int rule_count,
                                          int step_limit,
                                          bool normalize_between_steps);
```

**算法描述：**

1. 初始化 WL 哈希历史 `wl_history`。
2. 循环（直到步数上限或无可应用规则）：
   a. 按约简测度降序遍历规则。
   b. 对每条规则调用 `vf2_find_match()`。
   c. 若匹配成功且前置条件满足，执行替换。
   d. 替换后检查约束冲突。若冲突，回滚。
   e. 若 `normalize_between_steps` 为 true，执行 `graph_normalize(graph, false)`。
   f. 计算图哈希，检查循环。
   g. 步数计数器 +1。
3. 返回最终状态。

**设计规格对应：** `design_v2.9.md` 第 6.4 节 "引擎控制循环"。

**依赖模块：** `normalization`, `vf2_find_match()`, `detect_rewrite_loop_wl()`。

---

### 5.2 `rewrite_validate_measure()` 约简测度验证

**函数签名：**

```c
/**
 * @brief 验证规则应用后约简测度确实减少
 *
 * 在规则应用后，验证指定的度量（节点数、约束数、代数次数）
 * 确实按 reduction_measure 声明的量减少。
 *
 * @param graph 规则应用后的约束图
 * @param rule 应用的规则
 * @param graph_before 规则应用前的图哈希（用于比较）
 * @return true 测度确实减少，false 未减少（可能需要回滚）
 */
bool rewrite_validate_measure(ConstraintGraph *graph,
                               const RewriteRule *rule,
                               const GraphHash *graph_before);
```

**算法描述：**

1. 计算 `graph_before` 和当前图的节点数、约束数。
2. 根据 `rule->reduction_measure`：
   - 若 `reduction_measure > 0`：验证 `(旧节点数 - 新节点数) >= reduction_measure` 或 `(旧约束数 - 新约束数) >= reduction_measure`。
   - 若 `reduction_measure == 0`：跳过验证。
   - 若 `reduction_measure < 0`：验证扩展量不超过 `|reduction_measure|`。
3. 返回验证结果。

**设计规格对应：** `design_v2.9.md` 第 6.1 节 "约简测度"。

---

### 5.3 增强 VF2 的 `coord_equal()` 容差匹配

**修改位置：** `src/rewrite.c` 中 `vf2_find_match()` 函数。

**当前缺陷：** VF2 匹配仅比较节点 ID，不支持坐标级容差匹配。

**增强算法：**

```c
/**
 * 在 VF2 匹配中，对 POINT 节点使用 coord_equal() 进行
 * 符号坐标判等，而非要求节点 ID 相同。
 *
 * 修改 VF2 的 is_feasible() 函数：
 */
static bool vf2_is_feasible_enhanced(
    const VF2State *state,
    int pattern_node_idx,
    int target_node_idx,
    const ConstraintGraph *target_graph,
    const RewritePattern *pattern,
    bool local_equivalence_tolerant)
{
    /* 获取模式节点和目标节点 */
    int pattern_var_id = pattern->variable_node_ids[pattern_node_idx];
    GeomNode *target_node = target_graph->nodes[target_node_idx];

    /* 若启用局部等价容忍且为 POINT 节点 */
    if (local_equivalence_tolerant && target_node->type == GEOM_POINT) {
        /* 查找模式中该变量节点的坐标约束 */
        SymbolicCoord *pattern_coord = get_pattern_coord(pattern, pattern_var_id);
        if (pattern_coord && target_node->coord_count > 0) {
            /* 使用符号坐标判等而非 ID 比较 */
            if (symbolic_coord_compare(pattern_coord,
                                        target_node->symbolic_coords[0]) != 0) {
                return false;
            }
            return true;  /* 坐标匹配，允许绑定 */
        }
    }

    /* 默认行为：类型匹配 */
    return true;
}
```

**设计规格对应：** `design_v2.9.md` 第 6.2 节 "对于 POINT 节点，匹配条件不要求节点 ID 相同，而是调用 coord_equal()"。

**依赖模块：** `symbolic_coord_compare()`。

---

### 5.4 规则热加载/卸载

**函数签名：**

```c
/**
 * @brief 从 .lvz 规则包运行时加载重写规则
 *
 * @param filepath 规则包文件路径
 * @param out_rules 输出：加载的规则数组
 * @param out_count 输出：规则数量
 * @return 加载的规则数量，-1 表示错误
 */
int rewrite_rules_load_from_file(const char *filepath,
                                  RewriteRule ***out_rules,
                                  int *out_count);

/**
 * @brief 卸载重写规则
 *
 * 卸载规则不影响已应用该规则的历史步骤。
 *
 * @param rules 规则数组
 * @param count 规则数量
 * @param rule_name 要卸载的规则名称
 * @return true 卸载成功
 */
bool rewrite_rule_unload(RewriteRule **rules, int count,
                          const char *rule_name);
```

**设计规格对应：** `design_v2.9.md` 第 6.7 节 "规则可从 .lvz 规则包在运行时热加载，也可在运行时卸载"。

---

## 模块 6: unify（合一检查系统）

### 当前完成度: 40%

### 待实现功能清单

| 编号 | 功能名称 | 优先级 |
|------|----------|--------|
| 6.1 | 深度子图同构匹配 | 高 |
| 6.2 | 坐标级等价检查 | 高 |
| 6.3 | 模板展开后匹配 | 中 |
| 6.4 | 与规范化遍集成 | 中 |

---

### 6.1 深度子图同构匹配

**函数签名：**

```c
/**
 * @brief 深度子图同构匹配（用于合一检查）
 *
 * 不仅匹配节点 ID，还匹配节点类型、约束类型和符号坐标。
 * 支持变量绑定（命题模式中的负 ID 变量绑定到构造图的实际节点）。
 *
 * @param construction 构造图
 * @param pattern 命题模式图
 * @param out_bindings 输出：变量绑定映射（模式变量 ID -> 构造图节点 ID）
 * @param out_binding_count 输出：绑定数量
 * @return UNIFY_OK 匹配成功，其他值表示失败原因
 */
UnifyStatus unify_subgraph_isomorphism(const ConstraintGraph *construction,
                                        const ConstraintGraph *pattern,
                                        int **out_bindings,
                                        int *out_binding_count);
```

**算法描述：**

1. 构建命题模式图的邻接表表示。
2. 使用改进的 VF2 算法进行子图同构匹配：
   - 模式节点为负 ID 的视为变量节点，可绑定到任意类型兼容的构造图节点。
   - 模式节点为正 ID 的视为固定节点，必须精确匹配。
3. 匹配条件：
   - 节点类型必须兼容。
   - 约束类型和参与者数量必须匹配。
   - 对于 POINT 节点，使用 `symbolic_coord_compare()` 进行坐标判等。
4. 收集所有成功的绑定映射。

**设计规格对应：** `design_v2.9.md` 第 10.2 节 "执行三层匹配"。

**依赖模块：** `symbolic_coord_compare()`, `normalization`。

---

### 6.2 坐标级等价检查

**函数签名：**

```c
/**
 * @brief 在合一检查中进行坐标级等价验证
 *
 * 对命题模式中的每个坐标参数，在构造图中找到
 * 代数判等下完全相等的对应坐标。
 *
 * @param pattern_coords 命题模式坐标数组
 * @param pattern_coord_count 模式坐标数量
 * @param construction_coords 构造图坐标数组
 * @param construction_coord_count 构造图坐标数量
 * @return true 所有坐标等价，false 存在不等价坐标
 */
bool unify_check_coords(SymbolicCoord **pattern_coords, int pattern_coord_count,
                         SymbolicCoord **construction_coords,
                         int construction_coord_count);
```

**算法描述：**

1. 若 `pattern_coord_count != construction_coord_count`，返回 false。
2. 对每对坐标 `(pattern_coords[i], construction_coords[i])`：
   - 调用 `symbolic_coord_compare()` 进行精确判等。
   - 若为代数数且区间重叠，调用 `algebraic_refine_for_equality()` 提升精度。
3. 所有坐标对均等价时返回 true。

**设计规格对应：** `design_v2.9.md` 第 10.2 节 "符号坐标判等：约束的坐标参数在代数判等下完全相等"。

**依赖模块：** `symbolic_coord_compare()`, `algebraic_refine_for_equality()`。

---

### 6.3 模板展开后匹配

**函数签名：**

```c
/**
 * @brief 展开命题模式中的约束模板后再执行合一
 *
 * @param construction 构造图
 * @param proposition 命题（含模板约束）
 * @param axiom_pkgs 可用的公理包（用于模板展开）
 * @param pkg_count 公理包数量
 * @return 合一结果
 */
UnifyStatus proof_unify_with_expansion(ConstraintGraph *construction,
                                        Proposition *proposition,
                                        AxiomPackage **axiom_pkgs,
                                        int pkg_count);
```

**算法描述：**

1. 复制命题的模式图。
2. 遍历模式图中的约束，对每个 `template_id != -1` 的约束：
   - 在公理包中查找对应的 `ConstraintTemplate`。
   - 调用模板的 `expand()` 函数，将模板约束替换为基本约束图。
3. 递归展开至多 8 层（防止无限递归）。
4. 在展开后的模式图上执行标准合一检查。

**设计规格对应：** `design_v2.9.md` 第 10.2 节 "展开命题模式中的所有约束模板实例为正则形式的基本约束图"。

**依赖模块：** `axiom_pkg`, `proof_unify()`。

---

### 6.4 与规范化遍集成

**修改位置：** `src/unify.c` 中 `unify_construction_with_proposition()` 函数。

**当前实现：** 已在合一前调用 `graph_normalize()`，但未使用模板展开和深度子图同构。

**增强流程：**

```c
UnifyStatus unify_construction_with_proposition(
    ConstraintGraph *construction,
    ConstraintGraph *proposition)
{
    /* 步骤 1：对两个图分别执行规范化 */
    NormalizationResult *nc = graph_normalize(construction, true);
    NormalizationResult *np = graph_normalize(proposition, true);

    /* 步骤 2：展开模板约束（新增） */
    /* ... 调用 proof_unify_with_expansion 的展开逻辑 ... */

    /* 步骤 3：执行三层匹配 */
    /* 层 1：端口类型匹配 */
    UnifyStatus port_result = unify_match_ports(construction, proposition);
    if (port_result != UNIFY_STATUS_OK) return port_result;

    /* 层 2：约束类型匹配（使用深度子图同构） */
    UnifyStatus constraint_result = unify_match_constraints_deep(
        construction, proposition);
    if (constraint_result != UNIFY_STATUS_OK) return constraint_result;

    /* 层 3：符号坐标判等 */
    UnifyStatus coord_result = unify_check_coords_all(
        construction, proposition);
    if (coord_result != UNIFY_STATUS_OK) return coord_result;

    return UNIFY_STATUS_OK;
}
```

**设计规格对应：** `design_v2.9.md` 第 10.2 节完整流程。

---

## 模块 7: func_block（函数块系统）

### 当前完成度: 50%

### 待实现功能清单

| 编号 | 功能名称 | 优先级 |
|------|----------|--------|
| 7.1 | `func_block_determinism_check_static()` 完善 | 高 |
| 7.2 | `func_block_determinism_check_dynamic()` 完善 | 高 |
| 7.3 | 增强 beta-归约变量捕获消解 | 高 |
| 7.4 | `func_block_verify_determinism()` 完整流水线 | 中 |

---

### 7.1 `func_block_determinism_check_static()` 完善

**函数签名（已存在于头文件）：**

```c
DeterminismCheckResult func_block_check_determinism_static(
    FuncBlock *fb,
    ConstraintGraph *graph,
    int step_limit);
```

**当前实现缺陷：** 框架存在但内部逻辑不完整。

**完善算法：**

```c
DeterminismCheckResult func_block_check_determinism_static(
    FuncBlock *fb, ConstraintGraph *graph, int step_limit)
{
    /* 1. 提取内部约束子图的方程 */
    mpz_poly_t *equations = NULL;
    int eq_count = 0;
    solver_extract_equations(graph, fb->internal_node_ids,
                              fb->internal_node_count,
                              &equations, &eq_count);

    /* 2. 分析方程系统的线性/非线性程度 */
    bool is_linear = true;
    for (int i = 0; i < eq_count; i++) {
        if (equations[i].degree > 1) {
            is_linear = false;
            break;
        }
    }

    /* 3. 线性系统：多项式时间判定解的唯一性 */
    if (is_linear) {
        /* 计算秩 r 和变量数 n */
        int rank = compute_matrix_rank(equations, eq_count);
        int n_vars = count_variables(equations, eq_count);
        if (rank == n_vars) {
            fb->determinism = DETERMINISM_VERIFIED;
            free(equations);
            return DETERMINISM_CHECK_UNIQUE;
        } else if (rank < n_vars) {
            fb->determinism = DETERMINISM_PARTIALLY_VERIFIED;
            free(equations);
            return DETERMINISM_CHECK_MULTIPLE;
        }
    }

    /* 4. 二次系统：使用符号消元尝试求解 */
    GröbnerResult *result = NULL;
    SolverStatus status = solve_algebraic_system(
        graph, fb->internal_node_ids,
        fb->internal_node_count, &result);

    if (status == SOLVER_UNIQUE) {
        fb->determinism = DETERMINISM_VERIFIED;
        return DETERMINISM_CHECK_UNIQUE;
    } else if (status == SOLVER_MULTIPLE) {
        fb->determinism = DETERMINISM_NON_DETERMINISTIC;
        return DETERMINISM_CHECK_MULTIPLE;
    } else if (status == SOLVER_TIMEOUT) {
        fb->determinism = DETERMINISM_PARTIALLY_VERIFIED;
        return DETERMINISM_CHECK_TIMEOUT;
    }

    free(equations);
    return DETERMINISM_CHECK_OUT_OF_RANGE;
}
```

**设计规格对应：** `design_v2.9.md` 第 8.2 节 "静态层（打包时）"。

**依赖模块：** `solver`, `mpz_poly.h`。

---

### 7.2 `func_block_determinism_check_dynamic()` 完善

**函数签名（已存在于头文件）：**

```c
DeterminismCheckResult func_block_check_determinism_dynamic(
    FuncBlock *fb, ConstraintGraph *graph,
    SymbolicCoord **arg_values, int arg_count,
    GeomNode ***out_solutions, int *out_solution_count);
```

**完善算法：**

1. 将实参值绑定到函数块的输入端口。
2. 对内部约束图执行求解（调用 `solve_algebraic_system()`）。
3. 根据求解结果：
   - `SOLVER_UNIQUE`：返回 `DETERMINISM_CHECK_UNIQUE`，`PARTIALLY_VERIFIED` 可升级为 `VERIFIED`。
   - `SOLVER_MULTIPLE`：
     - 若函数块有选择器，应用选择器过滤候选解。
     - 若过滤后唯一解，返回 `DETERMINISM_CHECK_UNIQUE`。
     - 若仍多解，标记 `NON_DETERMINISTIC`，返回 `DETERMINISM_CHECK_MULTIPLE`。
   - `SOLVER_NO_SOLUTION`：返回 `DETERMINISM_CHECK_NO_SOLUTION`。

**设计规格对应：** `design_v2.9.md` 第 8.2 节 "动态层（应用时）"。

**依赖模块：** `solver`, `selector_apply()`。

---

### 7.3 增强 beta-归约变量捕获消解

**修改位置：** `src/func_block.c` 中 `func_block_instantiate()` 函数。

**当前缺陷：** 变量捕获判定逻辑不完整。

**增强算法（对应设计文档第 3.3 节）：**

```c
/**
 * beta-归约核心：变量捕获消解
 *
 * 复制函数块内部连线时，对每条连线的目标端口 p：
 *
 * 情况 A（形式参数引用）：
 *   p.parent_block_id == 被复制块的ID && p.is_formal_param == true
 *   → 重定向到对应实参输出端口
 *
 * 情况 B（自由变量引用）：
 *   p.parent_block_id != 被复制块的ID
 *   → 保持原连接目标不变
 *
 * 情况 C（内部局部引用）：
 *   p.parent_block_id == 被复制块的ID && p.is_formal_param == false
 *   → 重映射到复制件中对应的新内部节点
 */
static int resolve_beta_binding(
    Port *target_port,
    int copied_block_id,
    const int *old_to_new_id_map,
    int id_map_count,
    const int *input_to_arg_map,
    int arg_map_count)
{
    /* 情况 A：形式参数引用 */
    if (target_port->parent_block_id == copied_block_id &&
        target_port->is_formal_param) {
        /* 在 input_to_arg_map 中查找实参 */
        for (int i = 0; i < arg_map_count; i++) {
            if (input_to_arg_map[i * 2] == target_port->id) {
                return input_to_arg_map[i * 2 + 1];  /* 实参输出端口 */
            }
        }
        return -1;  /* 错误：未找到对应实参 */
    }

    /* 情况 B：自由变量引用 */
    if (target_port->parent_block_id != copied_block_id) {
        return target_port->id;  /* 保持不变 */
    }

    /* 情况 C：内部局部引用 */
    for (int i = 0; i < id_map_count; i++) {
        if (old_to_new_id_map[i * 2] == target_port->id) {
            return old_to_new_id_map[i * 2 + 1];  /* 新内部节点 */
        }
    }

    return -1;  /* 错误：未找到映射 */
}
```

**设计规格对应：** `design_v2.9.md` 第 3.3 节 "变量捕获消解（beta-归约核心）"。

**依赖模块：** `constraint_graph`（Port 结构）。

---

### 7.4 `func_block_verify_determinism()` 完整流水线

**函数签名：**

```c
/**
 * @brief 完整的确定性验证流水线
 *
 * 依次执行静态分析和动态验证，返回最终确定性状态。
 *
 * @param fb 函数块
 * @param graph 约束图
 * @param test_inputs 测试输入值数组（用于动态验证）
 * @param test_input_count 测试输入数量
 * @return 最终确定性状态
 */
DeterminismState func_block_verify_determinism(
    FuncBlock *fb, ConstraintGraph *graph,
    SymbolicCoord ***test_inputs, int test_input_count);
```

**算法描述：**

1. 执行静态分析 `func_block_check_determinism_static(fb, graph, 100)`。
2. 若静态分析确认为 `VERIFIED`，直接返回。
3. 若为 `PARTIALLY_VERIFIED`，对每个测试输入执行动态验证。
4. 若所有动态验证均返回唯一解，升级为 `VERIFIED`。
5. 若任何动态验证返回多解，降级为 `NON_DETERMINISTIC`。

**设计规格对应：** `design_v2.9.md` 第 8.2 节完整确定性检查流程。

---

## 模块 8: type_system（类型系统）

### 当前完成度: 30%

### 待实现功能清单

| 编号 | 功能名称 | 优先级 |
|------|----------|--------|
| 8.1 | `type_attach_to_node()` 自动附加类型信息 | 高 |
| 8.2 | `type_check_cumulative()` 宇宙层级累积性 | 高 |
| 8.3 | `type_check_dependent()` 依赖类型等价 | 中 |
| 8.4 | 增强类型推断与约束图集成 | 中 |

---

### 8.1 `type_attach_to_node()` 自动附加类型信息

**函数签名：**

```c
/**
 * @brief 将类型信息自动附加到几何节点
 *
 * 根据节点类型自动创建并附加 TypeRegion：
 * - GEOM_POINT → TYPE_KIND_POINT (level 0)
 * - GEOM_LINE_SEGMENT → TYPE_KIND_LINE_SEGMENT (level 0)
 * - GEOM_REGION → TYPE_KIND_REGION (level 0)
 * - GEOM_PORT → 从连接的类型区域推断
 * - GEOM_FUNCTION_BLOCK → TYPE_KIND_FUNCTION (level 1)
 *
 * @param ts 类型系统
 * @param node 几何节点
 * @return 附加的类型区域，NULL 表示失败
 */
TypeRegion *type_attach_to_node(TypeSystem *ts, GeomNode *node);
```

**算法描述：**

1. 根据 `node->type` 创建对应的 `TypeRegion`。
2. 对于 `GEOM_PORT`：
   - 若端口有 `type_region`，直接返回。
   - 否则尝试从连接的对方端口推断类型。
3. 对于 `GEOM_FUNCTION_BLOCK`：
   - 创建函数类型 `input_type -> output_type`。
   - 递归附加输入/输出端口的类型。
4. 将类型区域存储在端口的 `type_region` 字段中。

**设计规格对应：** `design_v2.9.md` 第 12.1 节宇宙层级机制。

**依赖模块：** `constraint_graph`, `type_create_*` 系列函数。

---

### 8.2 `type_check_cumulative()` 宇宙层级累积性

**函数签名（已存在于头文件）：**

```c
bool type_check_cumulative(TypeSystem *ts,
                            TypeRegion *lower,
                            TypeRegion *higher);
```

**当前实现缺陷：** 函数签名存在但实现为空壳。

**完善算法：**

```c
bool type_check_cumulative(TypeSystem *ts,
                            TypeRegion *lower,
                            TypeRegion *higher)
{
    if (!ts->cumulative) {
        /* 非累积模式：层级必须严格相等 */
        return type_get_level(lower) == type_get_level(higher);
    }

    /* 累积模式：第 n 层的类型自动属于第 n+1 层 */
    int lower_level = type_get_level(lower);
    int higher_level = type_get_level(higher);

    /* lower 的层级 <= higher 的层级 */
    if (lower_level <= higher_level) return true;

    /* 检查递归：函数类型的累积性 */
    if (lower->kind == TYPE_KIND_FUNCTION && higher->kind == TYPE_KIND_FUNCTION) {
        /* (A -> B) : (i+1) 要求 A : i, B : (i+1) */
        /* 在累积模式下，A 可以在更高层级 */
        return type_check_cumulative(ts, lower->input_type, higher->input_type) &&
               type_check_cumulative(ts, lower->output_type, higher->output_type);
    }

    return false;
}
```

**设计规格对应：** `design_v2.9.md` 第 12.2 节 "累积性"。

**依赖模块：** `type_get_level()`。

---

### 8.3 `type_check_dependent()` 依赖类型等价

**函数签名：**

```c
/**
 * @brief 检查依赖类型 Pi(x:A).B(x) 的等价性
 *
 * 两个依赖类型等价当且仅当：
 * 1. 参数类型 A1 == A2
 * 2. 对所有 x: A，B1(x) == B2(x)
 *
 * @param ts 类型系统
 * @param type1 第一个依赖类型
 * @param type2 第二个依赖类型
 * @param use_rewrite 是否使用重写引擎归一化
 * @return 类型等价检查结果
 */
TypeEquivResult type_check_dependent(TypeSystem *ts,
                                      TypeRegion *type1,
                                      TypeRegion *type2,
                                      bool use_rewrite);
```

**算法描述：**

1. 检查两个类型均为 `TYPE_KIND_DEPENDENT`。
2. 比较参数节点：`type1->param_node_id` 和 `type2->param_node_id`。
3. 比较参数类型：递归调用 `type_check_equivalence()`。
4. 比较体类型：将参数值代入体类型后比较。
5. 若 `use_rewrite` 为 true，使用重写引擎尝试归一化体类型后再比较。

**设计规格对应：** `design_v2.9.md` 第 12.5 节 "依赖类型 Pi(x:A).B(x)"。

**依赖模块：** `type_check_equivalence()`, `rewrite`。

---

### 8.4 增强类型推断与约束图集成

**函数签名（已存在于头文件）：**

```c
bool type_infer_node(TypeSystem *ts, ConstraintGraph *graph,
                      int node_id, TypeRegion **out_type);
```

**增强算法：**

```c
bool type_infer_node(TypeSystem *ts, ConstraintGraph *graph,
                      int node_id, TypeRegion **out_type)
{
    GeomNode *node = graph_get_node(graph, node_id);
    if (!node) return false;

    /* 基本类型直接推断 */
    switch (node->type) {
        case GEOM_POINT:
            *out_type = type_create_point(ts);
            return true;
        case GEOM_LINE_SEGMENT:
            *out_type = type_create_line_segment(ts);
            return true;
        case GEOM_REGION:
            *out_type = type_create_region(ts, NULL, 0);
            return true;
        case GEOM_PORT: {
            /* 从连接的对方端口推断 */
            Port *port = node->data.port;
            if (port->connected_to && port->connected_to->type_region) {
                *out_type = port->connected_to->type_region;
                return true;
            }
            /* 沿约束链推断 */
            int *constraint_ids = graph_find_constraints_involving(
                graph, node_id, &(int){0});
            /* ... 分析约束链推断类型 ... */
            return false;
        }
        case GEOM_FUNCTION_BLOCK: {
            /* 从输入/输出端口类型构建函数类型 */
            TypeRegion *input_type = NULL, *output_type = NULL;
            /* 推断输入端口类型（取所有输入端口的乘积类型） */
            /* 推断输出端口类型 */
            if (input_type && output_type) {
                *out_type = type_create_function(ts, input_type, output_type);
                return true;
            }
            return false;
        }
    }
    return false;
}
```

**设计规格对应：** `design_v2.9.md` 第 12.7 节 "类型推断"。

**依赖模块：** `constraint_graph`, `graph_find_constraints_involving()`。

---

## 模块 9: proof（命题与证明系统）

### 当前完成度: 30%

### 待实现功能清单

| 编号 | 功能名称 | 优先级 |
|------|----------|--------|
| 9.1 | `proof_interactive_step()` 交互式证明步骤 | 高 |
| 9.2 | `proof_auto_search()` 自动证明路径搜索 | 高 |
| 9.3 | 增强 `proof_unify` 完整模板展开 | 高 |
| 9.4 | 证明断点保存/恢复 | 中 |

---

### 9.1 `proof_interactive_step()` 交互式证明步骤

**函数签名：**

```c
/**
 * @brief 用户引导的交互式证明步骤
 *
 * 允许用户在证明过程中手动选择下一步操作：
 * - 添加节点/约束
 * - 应用重写规则
 * - 应用函数块
 * - 执行合一检查
 *
 * @param nav 证明导航器
 * @param step_type 步骤类型
 * @param step_data 步骤数据（根据类型不同而不同）
 * @return 是否成功执行步骤
 */
bool proof_interactive_step(ProofNavigator *nav,
                             ProofStepType step_type,
                             void *step_data);
```

**算法描述：**

1. 根据 `step_type` 创建 `ProofStep`。
2. 设置步骤的依赖关系（依赖当前步骤之前的所有步骤）。
3. 执行步骤对应的操作：
   - `PROOF_STEP_ADD_NODE`: 在构造图中添加节点。
   - `PROOF_STEP_ADD_CONSTRAINT`: 在构造图中添加约束。
   - `PROOF_STEP_REWRITE`: 应用重写规则。
   - `PROOF_STEP_FUNCTION_APP`: 例化函数块。
   - `PROOF_STEP_NORMALIZATION`: 执行规范化。
4. 将步骤添加到导航器。
5. 更新证明状态颜色。

**设计规格对应：** `design_v2.9.md` 第 10.7 节 "步骤幻灯片回放"。

**依赖模块：** `constraint_graph`, `rewrite`, `func_block`, `normalization`。

---

### 9.2 `proof_auto_search()` 自动证明路径搜索

**函数签名：**

```c
/**
 * @brief 自动搜索证明路径
 *
 * 使用 BFS/DFS 在可能的构造空间中搜索满足命题的证明。
 * 每步尝试应用可用的公理包模板和重写规则。
 *
 * @param target_prop 目标命题
 * @param axiom_pkgs 可用的公理包
 * @param pkg_count 公理包数量
 * @param rules 可用的重写规则
 * @param rule_count 规则数量
 * @param max_depth 最大搜索深度
 * @param out_nav 输出：找到的证明导航器（若成功）
 * @return true 找到证明，false 未找到
 */
bool proof_auto_search(Proposition *target_prop,
                        AxiomPackage **axiom_pkgs, int pkg_count,
                        RewriteRule **rules, int rule_count,
                        int max_depth,
                        ProofNavigator **out_nav);
```

**算法描述：**

```
BFS 搜索：

队列 Q = [(初始构造图, 空步骤列表)]
已访问集合 V = {}

while Q 非空:
    (graph, steps) = Q.dequeue()

    if |steps| > max_depth: continue

    hash = compute_complete_graph_hash(graph)
    if hash in V: continue
    V.add(hash)

    /* 尝试合一检查 */
    result = proof_unify(graph, target_prop, true)
    if result == UNIFY_OK:
        构建 ProofNavigator 并返回 true

    /* 生成后继状态 */
    for each template t in axiom_pkgs:
        new_graph = apply_template(graph, t)
        Q.enqueue((new_graph, steps + [t]))

    for each rule r in rules:
        match = find_rewrite_match(graph, r, true)
        if match:
            new_graph = apply_rewrite(graph, r, match)
            Q.enqueue((new_graph, steps + [r]))

return false
```

**设计规格对应：** `design_v2.9.md` 第 11.3 节 "构造空间穷举工具"。

**依赖模块：** `proof_unify()`, `axiom_pkg`, `rewrite`, `normalization`。

---

### 9.3 增强 `proof_unify` 完整模板展开

**函数签名（已存在于头文件）：**

```c
UnifyResult proof_unify(ConstraintGraph *construction,
                         Proposition *proposition,
                         bool normalize_first);
```

**增强内容：**

在现有实现基础上添加：

1. 模板展开步骤（在规范化之后、匹配之前）。
2. 三层匹配的完整实现（当前仅做简单的端口和约束 ID 比较）。
3. 详细的失败报告。

```c
UnifyResult proof_unify(ConstraintGraph *construction,
                         Proposition *proposition,
                         bool normalize_first)
{
    /* 步骤 1：规范化 */
    if (normalize_first) {
        graph_normalize(construction, true);
        graph_normalize(proposition->pattern, true);
    }

    /* 步骤 2：模板展开（新增） */
    expand_templates_in_pattern(proposition->pattern);

    /* 步骤 3：三层匹配 */
    /* 层 1：端口类型匹配 */
    for (int i = 0; i < proposition->input_count; i++) {
        GeomNode *prop_port = graph_get_node(proposition->pattern,
                                              proposition->input_port_ids[i]);
        bool found = false;
        for (int j = 0; j < construction->node_count; j++) {
            GeomNode *con_node = construction->nodes[j];
            if (con_node->type != GEOM_PORT) continue;
            /* 使用类型等价检查而非简单的类型比较 */
            if (type_check_port_compatibility(...)) {
                found = true;
                break;
            }
        }
        if (!found) return UNIFY_PORT_MISMATCH;
    }

    /* 层 2：约束类型匹配（使用子图同构） */
    int *bindings = NULL;
    int binding_count = 0;
    UnifyStatus result = unify_subgraph_isomorphism(
        construction, proposition->pattern,
        &bindings, &binding_count);
    if (result != UNIFY_STATUS_OK) return UNIFY_CONSTRAINT_MISMATCH;

    /* 层 3：符号坐标判等 */
    for (int i = 0; i < binding_count; i++) {
        int pattern_id = bindings[i * 2];
        int construction_id = bindings[i * 2 + 1];
        GeomNode *pn = graph_get_node(proposition->pattern, pattern_id);
        GeomNode *cn = graph_get_node(construction, construction_id);
        if (pn->coord_count > 0 && cn->coord_count > 0) {
            if (symbolic_coord_compare(pn->symbolic_coords[0],
                                        cn->symbolic_coords[0]) != 0) {
                free(bindings);
                return UNIFY_COORD_MISMATCH;
            }
        }
    }

    free(bindings);
    return UNIFY_OK;
}
```

**设计规格对应：** `design_v2.9.md` 第 10.2 节完整合一检查流程。

**依赖模块：** `unify`, `type_system`, `axiom_pkg`。

---

### 9.4 证明断点保存/恢复

**函数签名：**

```c
/**
 * @brief 保存证明断点
 *
 * 序列化当前证明导航器状态（步骤列表、构造图、依赖关系）。
 *
 * @param nav 证明导航器
 * @param filepath 保存文件路径
 * @return 是否成功保存
 */
bool proof_save_breakpoint(ProofNavigator *nav, const char *filepath);

/**
 * @brief 恢复证明断点
 *
 * 从文件加载证明导航器状态，完整恢复上下文。
 *
 * @param filepath 断点文件路径
 * @param out_nav 输出：恢复的证明导航器
 * @return 是否成功恢复
 */
bool proof_restore_breakpoint(const char *filepath,
                               ProofNavigator **out_nav);
```

**算法描述：**

1. 序列化：将 `ProofNavigator` 的所有步骤、构造图快照、依赖关系写入文件。
2. 反序列化：从文件读取并重建 `ProofNavigator`。
3. 恢复后，用户可从断点处继续构造。

**设计规格对应：** `design_v2.9.md` 第 10.7 节 "断点与续证"。

**依赖模块：** 文件 I/O, `constraint_graph` 序列化。

---

## 模块 10: recursion（递归与条件系统）

### 当前完成度: 30%

### 待实现功能清单

| 编号 | 功能名称 | 优先级 |
|------|----------|--------|
| 10.1 | `recursion_validate_measure()` 符号测度递减验证 | 高 |
| 10.2 | 增强选择器块完整分支管理 | 高 |
| 10.3 | 互递归深度追踪 | 中 |

---

### 10.1 `recursion_validate_measure()` 符号测度递减验证

**函数签名：**

```c
/**
 * @brief 验证递归调用中测度严格递减
 *
 * 计算两次递归调用的测度值之差，
 * 使用符号代数不等式引擎判定是否 < 0。
 *
 * @param measure 测度定义
 * @param before_value 递归前的测度值
 * @param after_value 递归后的测度值
 * @return RECURSION_CHECK_RESULT_OK 测度递减，RECURSION_CHECK_RESULT_NOT_DECREASING 未递减
 */
RecursionCheckResult recursion_validate_measure(
    Measure *measure,
    SymbolicCoord *before_value,
    SymbolicCoord *after_value);
```

**算法描述：**

1. 计算差值 `diff = after_value - before_value`（使用 `symbolic_coord_subtract()`）。
2. 判定 `diff < 0`：
   - `RATIONAL`: 直接比较分子分母。
   - `QUADRATIC`: 比较数值近似或使用代数判等。
   - `ALGEBRAIC`: 使用隔离区间判定（若 `right_bound < 0` 则为负）。
3. 若 `diff < 0`，返回 `RECURSION_CHECK_RESULT_OK`。
4. 若 `diff >= 0`，返回 `RECURSION_CHECK_RESULT_NOT_DECREASING`。
5. 若无法判定，返回 `RECURSION_CHECK_RESULT_MEASURE_UNKNOWN`。

**设计规格对应：** `design_v2.9.md` 第 9.2 节 "符号测度：递减性由内核的代数不等式引擎直接判定"。

**依赖模块：** `symbolic_coord`, `measure_compute_value()`。

---

### 10.2 增强选择器块完整分支管理

**修改位置：** `src/recursion.c` 中 `selector_block_evaluate()` 函数。

**当前实现缺陷：** 仅设置分支状态，未管理分支子图的激活/去激活。

**增强算法：**

```c
bool selector_block_evaluate(SelectorBlock *sb, ConstraintGraph *graph)
{
    if (!sb || !graph) return false;

    /* 1. 获取测试点和测试区域 */
    GeomNode *test_point = graph_get_node(graph, sb->test_point_id);
    GeomNode *test_region = graph_get_node(graph, sb->test_region_id);
    if (!test_point || !test_region) return false;

    /* 2. 判定点是否在区域内 */
    /* 使用射线法（ray casting）进行点-多边形包含测试 */
    bool point_inside = check_point_in_region(test_point, test_region, graph);

    /* 3. 根据判定结果设置分支状态 */
    if (point_inside) {
        sb->true_state = BRANCH_ACTIVE;
        sb->false_state = BRANCH_SHADOWED;
    } else {
        sb->true_state = BRANCH_SHADOWED;
        sb->false_state = BRANCH_ACTIVE;
    }

    /* 4. 管理分支子图节点（新增） */
    /* 激活分支的节点保持正常状态 */
    /* 遮蔽分支的节点标记为灰色虚影 */
    int *active_ids = point_inside ?
        sb->true_branch_node_ids : sb->false_branch_node_ids;
    int active_count = point_inside ?
        sb->true_branch_node_count : sb->false_branch_node_count;
    int *shadowed_ids = point_inside ?
        sb->false_branch_node_ids : sb->true_branch_node_ids;
    int shadowed_count = point_inside ?
        sb->false_branch_node_count : sb->true_branch_node_count;

    for (int i = 0; i < active_count; i++) {
        GeomNode *node = graph_get_node(graph, active_ids[i]);
        if (node) node->trust = TRUST_GREEN;  /* 激活 */
    }
    for (int i = 0; i < shadowed_count; i++) {
        GeomNode *node = graph_get_node(graph, shadowed_ids[i]);
        if (node) node->trust = TRUST_BLUE;  /* 虚影 */
    }

    return true;
}

/**
 * @brief 射线法判断点是否在区域内
 */
static bool check_point_in_region(GeomNode *point, GeomNode *region,
                                   ConstraintGraph *graph)
{
    if (point->type != GEOM_POINT || region->type != GEOM_REGION) return false;
    if (point->coord_count < 2) return false;

    double px = symbolic_coord_to_double(point->symbolic_coords[0]);
    double py = symbolic_coord_to_double(point->symbolic_coords[1]);

    int crossings = 0;
    int seg_count = region->data.region.segment_count;

    for (int i = 0; i < seg_count; i++) {
        GeomNode *seg = region->data.region.boundary_segments[i];
        if (!seg || seg->coord_count < 2) continue;

        double x1 = symbolic_coord_to_double(seg->symbolic_coords[0]);
        double y1 = symbolic_coord_to_double(seg->symbolic_coords[1]);
        double x2 = symbolic_coord_to_double(seg->symbolic_coords[2]);
        double y2 = symbolic_coord_to_double(seg->symbolic_coords[3]);

        /* 射线从 (px, py) 向右水平发射 */
        if ((y1 > py) != (y2 > py)) {
            double x_intersect = x1 + (py - y1) / (y2 - y1) * (x2 - x1);
            if (px < x_intersect) crossings++;
        }
    }

    return (crossings % 2) == 1;
}
```

**设计规格对应：** `design_v2.9.md` 第 9.5 节 "条件/选择器块"。

**依赖模块：** `constraint_graph`, `symbolic_coord_to_double()`。

---

### 10.3 互递归深度追踪

**函数签名（已存在于头文件）：**

```c
bool recursion_check_mutual_with_contexts(RecursionContext *ctx_a,
                                           RecursionContext *ctx_b);
```

**增强算法：**

```c
bool recursion_check_mutual_with_contexts(RecursionContext *ctx_a,
                                           RecursionContext *ctx_b)
{
    /* 1. 验证两个上下文使用相同的全局测度 */
    if (ctx_a->active_measure != ctx_b->active_measure) return false;

    /* 2. 合并两个上下文的测度值历史 */
    int total_count = ctx_a->measure_value_count + ctx_b->measure_value_count;
    SymbolicCoord **all_values = malloc(total_count * sizeof(SymbolicCoord*));

    /* 交替插入两个上下文的测度值（模拟交叉调用） */
    int idx = 0;
    int ai = 0, bi = 0;
    while (ai < ctx_a->measure_value_count || bi < ctx_b->measure_value_count) {
        if (ai < ctx_a->measure_value_count)
            all_values[idx++] = ctx_a->measure_values[ai++];
        if (bi < ctx_b->measure_value_count)
            all_values[idx++] = ctx_b->measure_values[bi++];
    }

    /* 3. 验证合并后的序列严格单调递减 */
    for (int i = 1; i < idx; i++) {
        MeasureCompareResult cmp = measure_compare(
            ctx_a->active_measure, all_values[i-1], all_values[i]);
        if (cmp != MEASURE_LESS) {
            free(all_values);
            return false;  /* 不满足单调递减 */
        }
    }

    free(all_values);
    return true;
}
```

**设计规格对应：** `design_v2.9.md` 第 9.3 节 "互递归的测度检查要求两者在同一个全局测度下各自递减"。

**依赖模块：** `measure_compare()`。

---

## 模块 11: engine（引擎核心）

### 当前完成度: 65%

### 待实现功能清单

| 编号 | 功能名称 | 优先级 |
|------|----------|--------|
| 11.1 | `engine_solve()` 完整求解-重写-证明流水线 | 高 |
| 11.2 | `engine_rewrite_and_solve()` 编排工作流 | 高 |
| 11.3 | `engine_handle_circuit_trip()` 位数熔断处理 | 中 |
| 11.4 | 错误恢复集成 | 中 |

---

### 11.1 `engine_solve()` 完整求解-重写-证明流水线

**函数签名：**

```c
/**
 * @brief 完整的求解-重写-证明流水线
 *
 * 依次执行：
 * 1. 约束图规范化
 * 2. 重写引擎（应用所有可用规则）
 * 3. 求解器（提取方程并求解）
 * 4. 合一检查（若提供目标命题）
 *
 * @param engine 引擎实例
 * @param target_prop 目标命题（可为 NULL 表示不执行合一）
 * @param dirty_var_ids 脏变量 ID 数组（可为 NULL 表示全部）
 * @param dirty_count 脏变量数量
 * @param out_result 输出：求解结果
 * @return 引擎状态
 */
EngineStatus engine_solve(LV00Engine *engine,
                           Proposition *target_prop,
                           int *dirty_var_ids, int dirty_count,
                           GröbnerResult **out_result);
```

**算法描述：**

```c
EngineStatus engine_solve(LV00Engine *engine,
                           Proposition *target_prop,
                           int *dirty_var_ids, int dirty_count,
                           GröbnerResult **out_result)
{
    /* 步骤 1：规范化 */
    graph_normalize(engine->main_graph, false);

    /* 步骤 2：重写 */
    RewriteStatus rw_status = rewrite_with_rules(
        engine->main_graph,
        engine->rewrite_rules,
        engine->rewrite_rule_count,
        1000);  /* 默认步数上限 */

    if (rw_status == REWRITE_CONFLUENCE_ISSUE) {
        last_status = ENGINE_CONSTRAINT_CONFLICT;
        return ENGINE_CONSTRAINT_CONFLICT;
    }

    /* 步骤 3：求解 */
    SolverStatus sv_status = solve_algebraic_system(
        engine->main_graph,
        dirty_var_ids, dirty_count,
        out_result);

    if (sv_status == SOLVER_OVERCONSTRAINED) {
        last_status = ENGINE_CONSTRAINT_CONFLICT;
        return ENGINE_CONSTRAINT_CONFLICT;
    }

    /* 步骤 4：合一检查（可选） */
    if (target_prop) {
        UnifyResult unify_result = engine_unify(
            engine, engine->main_graph,
            target_prop->pattern);
        /* ... 处理合一结果 ... */
    }

    last_status = ENGINE_OK;
    return ENGINE_OK;
}
```

**设计规格对应：** 整体系统集成需求。

**依赖模块：** `normalization`, `rewrite`, `solver`, `unify`。

---

### 11.2 `engine_rewrite_and_solve()` 编排工作流

**函数签名：**

```c
/**
 * @brief 编排重写 -> 求解工作流
 *
 * 在重写和求解之间进行迭代，直到达到不动点。
 *
 * @param engine 引擎实例
 * @param max_iterations 最大迭代次数
 * @param out_result 输出：最终求解结果
 * @return 引擎状态
 */
EngineStatus engine_rewrite_and_solve(LV00Engine *engine,
                                       int max_iterations,
                                       GröbnerResult **out_result);
```

**算法描述：**

```
for iter = 1 to max_iterations:
    1. 执行重写 rewrite_with_rules()
    2. 检查图是否变化（哈希比较）
    3. 若图未变化，跳出循环（不动点）
    4. 执行求解 solve_algebraic_system()
    5. 若求解成功且无多解，跳出循环
    6. 若求解产生新约束（如距离约束），添加到图中并继续
```

**设计规格对应：** 重写-求解的迭代收敛需求。

**依赖模块：** `rewrite`, `solver`, `normalization`。

---

### 11.3 `engine_handle_circuit_trip()` 位数熔断处理

**函数签名：**

```c
/**
 * @brief 处理位数熔断事件
 *
 * 当符号坐标运算触发位数熔断时调用。
 * 提供用户选项：忽略、回退、永久降级。
 *
 * @param engine 引擎实例
 * @param overflow_coord 触发熔断的坐标
 * @param action 用户选择的动作
 * @return 引擎状态
 */
EngineStatus engine_handle_circuit_trip(LV00Engine *engine,
                                          SymbolicCoord *overflow_coord,
                                          int action);
```

**算法描述：**

1. `action == 0`（忽略）：标记坐标为 AMBER，继续执行。
2. `action == 1`（回退）：恢复到冻结点快照，撤销所有中间操作。
3. `action == 2`（永久降级）：将坐标替换为高精度数值近似，标记为 AMBER。

**设计规格对应：** `design_v2.9.md` 第 1.5 节 "用户选项"。

**依赖模块：** `symbolic_coord`, `circuit_handle_overflow()`。

---

### 11.4 错误恢复集成

**函数签名：**

```c
/**
 * @brief 引擎错误恢复
 *
 * 在发生错误后尝试恢复到最近的稳定状态。
 *
 * @param engine 引擎实例
 * @return 是否成功恢复
 */
bool engine_recover(LV00Engine *engine);
```

**算法描述：**

1. 检查是否有冻结点快照。
2. 若有，恢复约束图到快照状态。
3. 清空脏变量集合。
4. 重置重写引擎状态。
5. 返回恢复结果。

**设计规格对应：** `design_v2.9.md` 第 18.3 节 "错误处理"。

---

## 模块 12: axiom_pkg（公理包系统）

### 当前完成度: 95%

### 待实现功能清单

| 编号 | 功能名称 | 优先级 |
|------|----------|--------|
| 12.1 | 增强模板测试正则形式验证 | 中 |
| 12.2 | 模板展开缓存 | 低 |

---

### 12.1 增强模板测试正则形式验证

**函数签名：**

```c
/**
 * @brief 验证模板展开结果的正则形式
 *
 * 检查模板展开后的约束图是否符合声明的正则形式描述：
 * - 约束类型组合是否匹配
 * - 节点类型是否匹配
 * - 是否使用了不允许的辅助构造
 *
 * @param tmpl 约束模板
 * @param expanded_graph 展开后的约束图
 * @param canonical_form 正则形式描述（约束类型+节点类型组合模式）
 * @return true 验证通过，false 不通过
 */
bool axiom_template_validate_normal_form(
    const ConstraintTemplate *tmpl,
    const ConstraintGraph *expanded_graph,
    const char *canonical_form);
```

**算法描述：**

1. 解析 `canonical_form` 描述（格式如 `"INCIDENCE(POINT,LINE_SEGMENT)+"`）。
2. 遍历展开图的约束，检查每个约束的类型和参与者类型是否匹配正则形式。
3. 检查展开图中是否包含正则形式未声明的节点类型。
4. 返回验证结果。

**设计规格对应：** `design_v2.9.md` 第 7.3 节 "正则形式强制验证"。

**依赖模块：** `constraint_graph`。

---

### 12.2 模板展开缓存

**函数签名：**

```c
/**
 * @brief 带缓存的模板展开
 *
 * 以参数哈希为键缓存展开结果，避免重复展开。
 *
 * @param tmpl 约束模板
 * @param params 参数数组
 * @param param_count 参数数量
 * @param target 目标约束图
 * @param cache 展开缓存（可为 NULL 表示不使用缓存）
 * @return 是否展开成功
 */
bool axiom_template_expand_cached(
    ConstraintTemplate *tmpl,
    SymbolicCoord **params, int param_count,
    ConstraintGraph *target,
    TemplateExpandCache *cache);

typedef struct TemplateExpandCache {
    uint64_t *param_hashes;       /* 参数哈希数组 */
    ConstraintGraph **cached_graphs; /* 缓存的展开图 */
    int entry_count;
    int capacity;
} TemplateExpandCache;
```

**设计规格对应：** `design_v2.9.md` 第 7.2 节 "展开结果缓存"。

---

## 模块 13: module（模块系统）

### 当前完成度: 85%

### 待实现功能清单

| 编号 | 功能名称 | 优先级 |
|------|----------|--------|
| 13.1 | 语义化版本约束解析 | 中 |
| 13.2 | 完整 nodes/constraints 段解析 | 中 |

---

### 13.1 语义化版本约束解析

**函数签名：**

```c
/**
 * @brief 解析语义化版本约束字符串
 *
 * 支持的格式：
 * - "1.0.0"（精确匹配）
 * - ">=1.0.0"（大于等于）
 * - "^1.0.0"（兼容主版本号，即 >=1.0.0 且 <2.0.0）
 * - "~1.0.0"（兼容次版本号，即 >=1.0.0 且 <1.1.0）
 * - "1.0.0 - 2.0.0"（范围）
 *
 * @param constraint 版本约束字符串
 * @param version 待检查的版本字符串
 * @return true 版本满足约束
 */
bool module_parse_version_constraint(const char *constraint,
                                      const char *version);

/**
 * @brief 比较两个语义化版本
 * @return <0 v1 < v2, 0 v1 == v2, >0 v1 > v2
 */
int module_compare_versions(const char *v1, const char *v2);
```

**算法描述：**

1. 解析版本字符串为 `(major, minor, patch)` 三元组。
2. 根据约束前缀分发：
   - 无前缀：精确匹配。
   - `>=`：逐字段比较。
   - `^`：`major` 相同且 `version >= constraint`。
   - `~`：`major.minor` 相同且 `version >= constraint`。
   - 范围：检查 `version >= lower && version <= upper`。

**设计规格对应：** `design_v2.9.md` 第 15.1 节版本管理需求。

---

### 13.2 完整 nodes/constraints 段解析

**修改位置：** `src/module.c` 中 LVZ 解析器。

**当前实现缺陷：** 解析器支持基本结构，但 nodes 和 constraints 段的解析不完整。

**增强内容：**

```c
/**
 * 在 LVZ 解析器中添加 nodes 段解析：
 *
 * nodes: {
 *     point: { id: 1, coords: ["1/2", "0/1"] }
 *     line_segment: { id: 2, endpoints: [1, 3] }
 *     region: { id: 4, boundary: [2, 5, 6] }
 *     port: { id: 7, type: "input", depth: 0, parent: -1 }
 * }
 *
 * constraints: {
 *     incidence: { id: 1, participants: [1, 2] }
 *     betweenness: { id: 2, participants: [1, 3, 5] }
 * }
 */
static ModuleLoadStatus parse_nodes_section(Module *mod,
                                              LvzLexer *lex);
static ModuleLoadStatus parse_constraints_section(Module *mod,
                                                    LvzLexer *lex);
```

**设计规格对应：** `design_v2.9.md` 第 15.1 节 ".lvz 文件格式"。

---

## 模块 14: debug（调试与性能系统）

### 当前完成度: 95%

### 待实现功能清单

| 编号 | 功能名称 | 优先级 |
|------|----------|--------|
| 14.1 | `debug_assert_normalization_invariants()` | 中 |
| 14.2 | 增强性能计数器报告 | 低 |

---

### 14.1 `debug_assert_normalization_invariants()`

**函数签名：**

```c
/**
 * @brief 规范化后不变量断言
 *
 * 在调试模式下，规范化完成后检查以下不变量：
 * 1. 不存在坐标相同但未合并的节点对（同作用域内）
 * 2. 不存在端点相同但未合并的线段对
 * 3. 不存在边界相同但未合并的区域对
 * 4. 所有约束的参与者 ID 均指向有效节点
 * 5. 约束参与者列表已按 ID 升序排列（稳定化）
 *
 * @param engine 引擎实例
 * @param ctx 调试上下文
 * @return 违反的不变量数量（0 表示全部通过）
 */
int debug_assert_normalization_invariants(const LV00Engine *engine,
                                           DebugContext *ctx);
```

**算法描述：**

```c
int debug_assert_normalization_invariants(const LV00Engine *engine,
                                           DebugContext *ctx)
{
    int violations = 0;
    ConstraintGraph *graph = engine->main_graph;

    /* 不变量 1：同作用域内无坐标相同的未合并节点 */
    for (int i = 0; i < graph->node_count; i++) {
        for (int j = i + 1; j < graph->node_count; j++) {
            GeomNode *a = graph->nodes[i];
            GeomNode *b = graph->nodes[j];
            if (a->type != GEOM_POINT || b->type != GEOM_POINT) continue;
            if (scope_key(a) != scope_key(b)) continue;
            if (coords_equal(a, b)) {
                LOG_ERROR("normalization",
                    "Invariant violation: nodes %d and %d have same coords "
                    "but were not merged", a->id, b->id);
                violations++;
            }
        }
    }

    /* 不变量 2：无端点相同的未合并线段 */
    /* ... 类似检查 ... */

    /* 不变量 4：约束参与者有效性 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        for (int j = 0; j < c->participant_count; j++) {
            if (!graph_get_node(graph, c->participants[j])) {
                LOG_ERROR("normalization",
                    "Invariant violation: constraint %d references "
                    "non-existent node %d", c->id, c->participants[j]);
                violations++;
            }
        }
    }

    /* 不变量 5：参与者列表已排序 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        for (int j = 1; j < c->participant_count; j++) {
            if (c->participants[j-1] > c->participants[j]) {
                LOG_ERROR("normalization",
                    "Invariant violation: constraint %d participants "
                    "not sorted", c->id);
                violations++;
            }
        }
    }

    if (ctx->abort_on_violation && violations > 0) {
        abort();
    }

    ctx->violation_count += violations;
    return violations;
}
```

**设计规格对应：** `design_v2.9.md` 第 3.6 节 "一致性断言"。

**依赖模块：** `engine`, `normalization`, `constraint_graph`。

---

### 14.2 增强性能计数器报告

**修改位置：** `src/debug.c` 中 `debug_counters_report()` 函数。

**增强内容：**

```c
char *debug_counters_report(void)
{
    PerformanceCounters c;
    debug_get_counters(&c);

    /* 计算额外统计 */
    double solver_avg = c.solver_call_count > 0 ?
        (double)c.solver_total_time_us / c.solver_call_count : 0.0;
    double unify_rate = c.unify_check_count > 0 ?
        100.0 * (double)c.unify_success_count / c.unify_check_count : 0.0;

    char *report = malloc(2048);
    snprintf(report, 2048,
        "=== Lv-00 Performance Report ===\n"
        "Nodes:      created=%llu  alive=%llu\n"
        "Constraints: created=%llu  alive=%llu\n"
        "Solver:     calls=%llu  avg=%.1f us\n"
        "Rewrite:    steps=%llu  rules_applied=%llu\n"
        "Unify:      checks=%llu  success=%llu (%.1f%%)\n"
        "Memory:     peak=%llu bytes  current=%llu bytes\n",
        (unsigned long long)c.total_nodes_created,
        (unsigned long long)c.current_nodes_alive,
        (unsigned long long)c.total_constraints_created,
        (unsigned long long)c.current_constraints_alive,
        (unsigned long long)c.solver_call_count,
        solver_avg,
        (unsigned long long)c.rewrite_total_steps,
        (unsigned long long)c.rewrite_rule_applications,
        (unsigned long long)c.unify_check_count,
        (unsigned long long)c.unify_success_count,
        unify_rate,
        (unsigned long long)c.memory_usage_peak,
        (unsigned long long)c.memory_current);

    return report;
}
```

**设计规格对应：** `design_v2.9.md` 第 18.5 节 "性能计数器"。

---

## 附录 A：模块间依赖关系图

```
symbolic_coord ← (被所有模块依赖)
     ↑
constraint_graph ← normalization, solver, rewrite, unify, func_block,
                     type_system, proof, recursion, engine
     ↑
normalization ← unify, rewrite, engine
     ↑
solver ← func_block, engine
     ↑
rewrite ← type_system, proof, engine
     ↑
unify ← proof, engine
     ↑
func_block ← recursion, engine
     ↑
type_system ← proof, engine
     ↑
proof ← engine
     ↑
recursion ← engine
     ↑
axiom_pkg ← module, proof, engine
     ↑
module ← engine
     ↑
engine (顶层集成)
     ↑
debug (横切关注点，被所有模块使用)
```

## 附录 B：实现优先级总结

### 第一阶段：核心基础设施（建议 2-3 周）

| 顺序 | 模块 | 功能 |
|------|------|------|
| 1 | symbolic_coord | 1.7 连分式逼近, 1.2 精度提升, 1.3 判等增强 |
| 2 | constraint_graph | 2.1 级联删除, 2.2 约束删除, 2.3 冗余检测 |
| 3 | normalization | 3.1 线段合并, 3.2 区域合并, 3.5 幂等性 |

### 第二阶段：推理能力（建议 3-4 周）

| 顺序 | 模块 | 功能 |
|------|------|------|
| 4 | solver | 4.1 Gröbner 基, 4.3 方程提取, 4.2 增量求解 |
| 5 | rewrite | 5.3 VF2 增强, 5.1 规范化集成, 5.2 测度验证 |
| 6 | unify | 6.1 子图同构, 6.2 坐标判等, 6.4 规范化集成 |

### 第三阶段：高级功能（建议 3-4 周）

| 顺序 | 模块 | 功能 |
|------|------|------|
| 7 | func_block | 7.3 beta-归约, 7.1 静态分析, 7.2 动态分析 |
| 8 | type_system | 8.2 累积性, 8.1 类型附加, 8.3 依赖类型 |
| 9 | proof | 9.3 合一增强, 9.1 交互式步骤, 9.2 自动搜索 |
| 10 | recursion | 10.1 测度验证, 10.2 选择器块 |

### 第四阶段：集成与收尾（建议 1-2 周）

| 顺序 | 模块 | 功能 |
|------|------|------|
| 11 | engine | 11.1 求解流水线, 11.2 编排, 11.3 熔断处理 |
| 12 | axiom_pkg | 12.1 正则形式验证 |
| 13 | module | 13.1 版本约束 |
| 14 | debug | 14.1 不变量断言 |

---

> 文档版本：1.0
> 最后更新：2026-05-19
> 基于：design_v2.9.md v3.0 功能规格说明书
