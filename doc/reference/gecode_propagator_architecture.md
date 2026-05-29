# Lv-00 参考设计：Gecode 传播器架构

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: [Gecode](https://www.gecode.org/) —— 开源约束求解器的传播器（Propagator）架构典范
> **目标**: 借鉴 Gecode 的 Propagator 架构，将 Lv-00 `constraint_graph.h` 中的每种约束类型建模为一个 Propagator，实现变量域变化时自动触发约束传播，映射到现有的约束传播求解策略

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴要点](#2-核心借鉴要点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)

---

## 1. 项目概述

### 1.1 Gecode 是什么

Gecode（Generic Constraint Development Environment）是约束规划社区最成熟的开源求解器之一。它由 Christian Schulte 等人开发，被广泛用于组合优化、调度、配置等领域。Gecode 最具启发性的设计是**传播器（Propagator）架构**——每个约束类型被实现为一个独立的 Propagator 对象，当变量域发生变化时，自动触发约束传播以缩小其他变量的域：

```cpp
// Gecode 示例：定义 "x + y = z" 约束的传播器
class SumPropagator : public Propagator {
public:
    // 构造函数：记录涉及的三个变量
    SumPropagator(Home home, IntVar x, IntVar y, IntVar z);

    // 传播逻辑：当 x/y/z 任一变量域变化时被调用
    virtual ExecStatus propagate(Space& home) {
        // x + y = z → z.min() = max(z.min(), x.min() + y.min())
        GECODE_ME_CHECK(z.gq(home, x.min() + y.min()));
        GECODE_ME_CHECK(z.lq(home, x.max() + y.max()));
        // 同理推 x 和 y 的域
        GECODE_ME_CHECK(x.gq(home, z.min() - y.max()));
        GECODE_ME_CHECK(x.lq(home, z.max() - y.min()));
        GECODE_ME_CHECK(y.gq(home, z.min() - x.max()));
        GECODE_ME_CHECK(y.lq(home, z.max() - x.min()));
        // 如果所有变量都被唯一确定 → 传播完成（subsumed）
        return (x.assigned() && y.assigned() && z.assigned())
               ? ES_SUBSUMED : ES_FIX;
    }
};
```

Gecode 的关键机制：

1. **传播器即约束**：每个约束（如 `x+y=z`、`x<y`、`alldifferent`）是一个 Propagator 对象
2. **变量域事件订阅**：Propagator 订阅它所涉及的变量，当变量域变化时自动被调度
3. **传播循环**：所有 Propagator 被放入一个传播队列，循环执行直到队列为空（达到不动点）
4. **Subsumption**：当传播器确定约束已永久满足（如变量全被赋值），传播器从队列中移除
5. **分支（Branching）**：当传播达到不动点但仍有变量未确定时，选择一个变量进行分叉

### 1.2 为什么借鉴 Gecode

Lv-00 的 `constraint_graph.h` 当前将约束存储为图上的边（`Constraint` 结构体），由求解器（`solver.h`）一次性将所有约束方程提取并求解。这种"批量求解"方式缺少 Gecode 式**增量传播**的能力——当用户在 Web GUI 中拖拽一个点时，Lv-00 应该只传播受影响的约束，而不是重新求解整个系统。借鉴 Gecode 意味着：

1. 每种 `ConstraintType` 变成一个独立的 Propagator 实现
2. 变量域变化时自动触发增量传播（而非批量重解）
3. 传播循环在 `ConstraintGraph` 的邻接结构上自然执行
4. 支持传播器 subsumption（当约束已被满足时自动休眠）

---

## 2. 核心借鉴要点

### 2.1 Propagator 的三要素

| Gecode 概念 | Lv-00 对应概念 | 映射说明 |
|-------------|---------------|---------|
| Propagator 基类 | `ConstraintPropagator` 抽象接口 | 每种约束类型的传播逻辑封装 |
| `propagate(Space&)` | `propagator_propagate(ctx, constraint)` | 传播函数：缩小相关变量域 |
| 变量域 | `SymbolicCoord` 的值域（有理数区间 + 代数条件） | 几何坐标的可取值范围 |
| 事件订阅 | `ConstraintGraph` 的邻接表遍历 | 当节点变化时，遍历其邻接约束 |
| 传播队列 | `PropagationQueue`（按拓扑序排列的约束列表） | 待传播的约束 FIFO 队列 |
| 传播循环 | `propagator_run_loop(graph)` | 借执行直到队列为空 |
| Subsumption | `propagator_is_subsumed()` | 约束永真时从活跃队列移除 |
| Branching | `solver_handle_multiple_solutions()` | 多解分支处理 |
| `ES_FIX` / `ES_SUBSUMED` / `ES_FAILED` | `PROPAGATE_OK` / `PROPAGATE_SUBSUMED` / `PROPAGATE_CONFLICT` | 传播结果状态码 |

### 2.2 五种几何约束类型的 Propagator 设计

| Lv-00 约束类型 | Propagator 名称 | 传播逻辑简述 |
|---------------|----------------|-------------|
| `INCIDENCE`（点在线上） | `IncidencePropagator` | 点 P 的坐标必须满足直线方程 ax+by+c=0；直线 AB 更新时，P 的域缩小到 AB 所在直线上 |
| `BETWEENNESS`（点之间） | `BetweennessPropagator` | P 满足 A-P-B 共线且 P 在 A 和 B 之间 → P 的域缩小到线段 AB 上，且 A.x <= P.x <= B.x |
| `INTERSECTION`（相交） | `IntersectionPropagator` | 两对象相交于 P → P 必须同时满足两个对象的方程（联立约束） |
| `CONTAINMENT`（包含） | `ContainmentPropagator` | P 在区域 R 内 → P 的域缩小到 R 的边界之内（卷绕数 > 0） |
| `CONNECTION`（端口连接） | `ConnectionPropagator` | 函数块端口数据流连接 → 类型检查 + 数据流向验证 |

### 2.3 传播循环的执行模型

```
Gecode 传播循环                      Lv-00 等价流程
──────────────────────────────────────────────────
1. 将初始约束加入传播队列            1. graph 构建完成 → 所有活跃约束入队
2. while 队列非空:
     pop 一个 Propagator            2. propagation_queue_pop()
     propagate() → ES_FIX/FAIL/...  3. propagator_propagate(ctx, constraint)
     if ES_FIX:
       re-schedule if needed       4. 如果相关变量域缩小，将邻接约束再入队
     if ES_SUBSUMED:
       从活跃列表移除               5. constraint_set_dormant()
     if ES_FAILED:
       报告冲突 → 回溯              6. conflict_record → graph_detect_conflicts()
3. 不动点到达 → 分支                7. degree_of_freedom > 0 → solver_handle_multiple_solutions()
```

---

## 3. Lv-00 映射方案

### 3.1 Propagator 核心数据结构

```c
/**
 * @brief 传播结果状态码（Gecode ExecStatus 的等价）
 */
typedef enum {
    PROPAGATE_OK,           /**< 传播成功，变量的域可能已缩小 */
    PROPAGATE_SUBSUMED,     /**< 约束被永久满足，可从活跃队列移除 */
    PROPAGATE_CONFLICT,     /**< 检测到冲突（如变量域变为空） */
    PROPAGATE_NO_CHANGE     /**< 传播完成但变量域无变化 */
} PropagateStatus;

/**
 * @brief 约束传播器抽象接口（Gecode Propagator 等价）
 *
 * 每种 ConstraintType 都有对应的传播器实现。
 * 传播器的核心职责是：当所涉及变量的域发生变化时，
 * 推断并缩小其他相关变量的域。
 */
typedef struct ConstraintPropagator ConstraintPropagator;

struct ConstraintPropagator {
    int constraint_id;                    /**< 关联的约束 ID */
    ConstraintType type;                  /**< 约束类型 */
    int *subscribed_var_ids;              /**< 订阅的变量节点 ID（其域变化时触发传播） */
    int subscribed_count;                 /**< 订阅变量数量 */

    /* 传播逻辑函数指针：由各类型 Propagator 实现 */
    PropagateStatus (*propagate)(ConstraintPropagator *self,
                                  ConstraintGraph *graph);

    /* 检查传播器是否已被 subsumed（约束永久满足） */
    bool (*is_subsumed)(const ConstraintPropagator *self,
                        const ConstraintGraph *graph);

    /* 状态 */
    bool is_active;                       /**< 是否在活跃传播队列中 */
    bool is_dormant;                      /**< 是否已休眠（subsumed） */
    int propagation_count;                /**< 传播次数（用于诊断） */

    /* 传播器的私有数据（不同类型有不同字段） */
    void *priv_data;
};
```

### 3.2 五种几何传播器的具体设计

#### 3.2.1 关联传播器（INCIDENCE）

```c
/**
 * @brief 关联传播器 —— 点在线上
 *
 * 约束: point P ∈ line AB
 * 变量域: P.x, P.y（符号坐标）
 * 传播逻辑:
 *   给定 A=(x_A, y_A), B=(x_B, y_B)，直线 AB 的方程为
 *     (B.y - A.y) * (P.x - A.x) - (B.x - A.x) * (P.y - A.y) = 0
 *   即 P 必须在 AB 所在的直线上。
 *
 *   当 A, B 的域缩小（坐标更确定）时，P 的域缩小为：
 *     - 如果 AB 完全确定 → P 的域缩小为直线上的区间
 *     - 如果 AB 部分确定 → P 的域缩小为更窄的带状区域
 *
 *   当 P 的域缩小时，如果 AB 不完全确定，反向传播：
 *     - P 的域可以约束 AB 的斜率范围
 */
PropagateStatus incidence_propagator_propagate(
    ConstraintPropagator *self, ConstraintGraph *graph)
{
    int point_id = self->subscribed_var_ids[0];
    int line_id  = self->subscribed_var_ids[1];

    // 获取直线 AB 的端点
    // 在 Lv-00 中，线段 AB 的端点通过 INCIDENCE 约束表达
    // 需要通过图遍历找到 A 和 B
    GeomNode *point = graph_get_node(graph, point_id);
    GeomNode **endpoints = find_line_endpoints(graph, line_id);

    if (!endpoints || !endpoints[0] || !endpoints[1])
        return PROPAGATE_NO_CHANGE;

    SymbolicCoord *ax = endpoints[0]->symbolic_coords[0];
    SymbolicCoord *ay = endpoints[0]->symbolic_coords[1];
    SymbolicCoord *bx = endpoints[1]->symbolic_coords[0];
    SymbolicCoord *by = endpoints[1]->symbolic_coords[1];

    // 构造共线方程: (by-ay)*(Px-ax) - (bx-ax)*(Py-ay) = 0
    // 如果 P 的坐标已确定，验证方程 → 不满足则返回 PROPAGATE_CONFLICT
    // 如果 P 的坐标未确定，缩小 P 的域到直线附近

    bool all_assigned = are_all_assigned(ax, ay, bx, by,
                                         point->symbolic_coords[0],
                                         point->symbolic_coords[1]);
    if (all_assigned) {
        return PROPAGATE_SUBSUMED;  // 所有变量已定，约束检验一次性通过
    }

    // 缩小 P 的域（反向传播省略，原理类似）
    return PROPAGATE_OK;
}
```

#### 3.2.2 之间传播器（BETWEENNESS）

```c
/**
 * @brief 之间传播器 —— B 在 A 和 C 之间
 *
 * 约束: A - B - C（共线且有序）
 * 传播逻辑:
 *   1. 首先传播共线性（同 INCIDENCE 逻辑）
 *   2. 然后缩小 B 的域：A.x <= B.x <= C.x（或反向）
 *   3. 缩小 A 的域（如果有已知的 B 位置）: A.x <= B.x
 *   4. 缩小 C 的域（如果有已知的 B 位置）: B.x <= C.x
 */
PropagateStatus betweenness_propagator_propagate(
    ConstraintPropagator *self, ConstraintGraph *graph)
{
    int a_id = self->subscribed_var_ids[0];
    int b_id = self->subscribed_var_ids[1];
    int c_id = self->subscribed_var_ids[2];

    GeomNode *A = graph_get_node(graph, a_id);
    GeomNode *B = graph_get_node(graph, b_id);
    GeomNode *C = graph_get_node(graph, c_id);

    // 1. 共线性传播
    // (复用 incidence propagator 的逻辑)
    PropagateStatus col_status = propagate_collinearity(graph, a_id, b_id, c_id);
    if (col_status == PROPAGATE_CONFLICT)
        return PROPAGATE_CONFLICT;

    // 2. 有序条件传播
    // 如果 A 和 C 的 x 坐标有界，缩小 B.x 的域
    if (A->symbolic_coords[0] && C->symbolic_coords[0]) {
        double a_min = symbolic_coord_to_double(A->symbolic_coords[0], MIN);
        double c_max = symbolic_coord_to_double(C->symbolic_coords[0], MAX);
        tighten_coord_domain(B->symbolic_coords[0], a_min, c_max);
    }

    // 3. 检查子吞条件
    if (are_all_coords_assigned(A) && are_all_coords_assigned(B)
        && are_all_coords_assigned(C))
        return PROPAGATE_SUBSUMED;

    return PROPAGATE_OK;
}
```

#### 3.2.3 相交传播器（INTERSECTION）

```c
/**
 * @brief 相交传播器 —— 两对象相交于一点
 *
 * 约束: line1 ∩ circle = {P}（通用：两个几何对象 obj1 ∩ obj2 = {P}）
 * 传播逻辑:
 *   1. 如果 line1 和 circle 均完全确定 → P 的坐标由联立方程精确求解
 *      → 如果存在两个解，需要 selector（多解选择器）来分派
 *   2. 如果 P 的坐标已部分确定 → 反向约束 obj1 和 obj2 的参数域
 *   3. 使用 Gröbner 基或 SMT 求解联立方程
 */
PropagateStatus intersection_propagator_propagate(
    ConstraintPropagator *self, ConstraintGraph *graph)
{
    int obj1_id = self->subscribed_var_ids[0];
    int obj2_id = self->subscribed_var_ids[1];
    int result_id = self->subscribed_var_ids[2];

    GeomNode *obj1 = graph_get_node(graph, obj1_id);
    GeomNode *obj2 = graph_get_node(graph, obj2_id);
    GeomNode *result = graph_get_node(graph, result_id);

    // 1. 提取两个对象的方程
    // 根据 GeomNode.type 调用相应的方程提取器
    bool obj1_determined = is_fully_determined(obj1);
    bool obj2_determined = is_fully_determined(obj2);

    if (obj1_determined && obj2_determined) {
        // 联立求解 → Gröbner 基
        // 如果涉及高次方程 → SMT
        GroebnerResult *solutions = NULL;
        SolverStatus status = solve_intersection_system(
            graph, obj1_id, obj2_id, &solutions);

        if (status == SOLVER_NO_SOLUTION)
            return PROPAGATE_CONFLICT;

        if (status == SOLVER_UNIQUE) {
            // 单解：将解赋值给 result 的 SymbolicCoord
            assign_result_coords(result, solutions);
            return PROPAGATE_SUBSUMED;
        }

        // 多解：等待 selector 选择（由上层分支逻辑处理）
        return PROPAGATE_OK;
    }

    // 2. 部分确定：通过约束传播缩小变量域
    // 例如：如果 result 的坐标有界 → obj1/obj2 的参数域缩小
    return PROPAGATE_OK;
}
```

#### 3.2.4 包含传播器（CONTAINMENT）

```c
/**
 * @brief 包含传播器 —— 点在区域内
 *
 * 约束: point P ∈ region R
 * 传播逻辑:
 *   1. P 的域缩小到 R 的包围盒内
 *   2. 如果 R 是简单多边形，用射线法判断内外
 *   3. 对于非凸区域，传播更复杂但遵循同样的"域缩小"原则
 */
PropagateStatus containment_propagator_propagate(
    ConstraintPropagator *self, ConstraintGraph *graph)
{
    int point_id = self->subscribed_var_ids[0];
    int region_id = self->subscribed_var_ids[1];

    GeomNode *point = graph_get_node(graph, point_id);
    GeomNode *region = graph_get_node(graph, region_id);

    // 1. 计算区域的轴对齐包围盒
    BoundingBox bbox = compute_region_bbox(graph, region_id);

    // 2. 缩小 P 的域到包围盒内
    tighten_coord_domain(point->symbolic_coords[0], bbox.x_min, bbox.x_max);
    tighten_coord_domain(point->symbolic_coords[1], bbox.y_min, bbox.y_max);

    // 3. 如果区域有符号卷绕数公式，用它缩小 P 的域
    if (region->data.region.boundary_segments) {
        // 复杂的半平面约束传播省略
    }

    if (is_fully_determined(point))
        return PROPAGATE_SUBSUMED;

    return PROPAGATE_OK;
}
```

### 3.3 传播队列与传播循环

```c
/**
 * @brief 传播队列 —— Gecode 传播队列的 Lv-00 等价
 *
 * 一个 FIFO 队列，存储所有活跃（非休眠）的约束传播器。
 * 队列按拓扑序组织：依赖关系较少的约束优先传播。
 */
typedef struct PropagationQueue {
    ConstraintPropagator **queue;     /**< 传播器数组（循环队列） */
    int head;                         /**< 队首索引 */
    int tail;                         /**< 队尾索引 */
    int capacity;                     /**< 队列容量 */
    int size;                         /**< 当前队列大小 */
} PropagationQueue;

/**
 * @brief 运行传播循环（Gecode propagate 的等价）
 *
 * 循环执行步骤：
 *  1. 从传播队列中弹出一个 ConstraintPropagator
 *  2. 调用 propagator.propagate(graph)
 *  3. 根据返回值：
 *     - PROPAGATE_OK → 如果相关变量域缩小，将其邻接约束重新入队
 *     - PROPAGATE_SUBSUMED → 标记为休眠，移出队列
 *     - PROPAGATE_CONFLICT → 记录冲突，返回 false（无解）
 *     - PROPAGATE_NO_CHANGE → 不操作
 *  4. 重复直到队列为空（不动点到达）
 *
 * @param[in,out] graph  约束图
 * @param[in,out] queue  传播队列
 * @param[in]     max_iterations  最大迭代次数（防止无限循环）
 * @return true 达到不动点，false 检测到冲突
 */
bool propagator_run_loop(ConstraintGraph *graph,
                         PropagationQueue *queue,
                         int max_iterations);
```

### 3.4 映射到现有 constraint_graph.h

| 现有结构 / API | 在 Propagator 架构中的角色 |
|---------------|--------------------------|
| `ConstraintGraph.nodes[]` | 变量集合：每个 `GeomNode` 携带其 `SymbolicCoord` 域 |
| `ConstraintGraph.constraints[]` | Propagator 源数据：每个 `Constraint` 对应一个 `ConstraintPropagator` |
| `Constraint.type` | 传播器类型标记：决定使用哪个 `propagate()` 实现 |
| `Constraint.participants[]` | 订阅变量：传播器订阅的变量节点 ID 列表 |
| `graph_find_constraints_involving()` | 传播触发：当节点域变化时，查找其邻接约束并入队 |
| `graph_get_node()` | 变量域读取：在传播中读取当前变量域 |
| `graph_detect_conflicts()` | 冲突检测：传播发现 `PROPAGATE_CONFLICT` 时的冲突提取 |
| `constraint_graph_add_node/constraint()` | 传播器注册：新节点/约束 → 创建对应 Propagator 并入队 |
| `solve_algebraic_system()` | 回退求解：当传播不能完全确定变量时，退回批量求解 |
| `SymbolicCoord` | 变量域：`RATIONAL` 类型天然有界，`ALGEBRAIC` 可通过最小多项式定界 |

### 3.5 增量传播在 Web GUI 拖拽中的应用

```
用户在 Web GUI 中拖拽点 A：
  1. 新坐标 (x', y') 赋值给 A.symbolic_coords
  2. 触发 graph_find_constraints_involving(graph, A.id)
     → 找到所有涉及 A 的约束：[INCIDENCE(A, AB), INCIDENCE(A, CA)]
  3. 将这两个约束对应的 Propagator 入队
  4. propagator_run_loop() 执行：
     a. INCIDENCE(A, AB) 传播 → 如果 B 的坐标有界，AB 的斜率域缩小
     b. INCIDENCE(A, CA) 传播 → 同理
     c. 如果 C 也在 AB/CA 约束中，C 的域可能缩小
     d. 继续传播直到不动点
  5. 将缩小后仍不确定的变量提交给 solver_incremental_solve()
  6. Web GUI 更新画布：已确定的点变绿，不确定的点保持蓝色
```

---

## 4. 实现路线图

### 4.1 第一阶段：Propagator 基础结构（P3）

| 任务 | 文件 | 说明 |
|------|------|------|
| 定义 `ConstraintPropagator`、`PropagateStatus`、`PropagationQueue` | `include/lv00/propagator.h`（新文件） | 传播器核心数据结构 |
| 实现 `propagation_queue_create/push/pop/destroy` | `src/propagator.c`（新文件） | 传播队列管理 |
| 实现 `propagator_run_loop()` | `src/propagator.c` | 传播循环主逻辑 |
| 实现 `constraint_to_propagator()` 注册表 | `src/propagator.c` | 每种 `ConstraintType` → 对应 `propagate()` 函数 |

**预估规模**：约 350 行 C 代码

### 4.2 第二阶段：五种几何传播器实现（P3）

| 任务 | 文件 | 说明 |
|------|------|------|
| 实现 `IncidencePropagator` | `src/propagator_incidence.c`（新文件） | 关联约束传播 |
| 实现 `BetweennessPropagator` | `src/propagator_betweenness.c`（新文件） | 之间约束传播 |
| 实现 `IntersectionPropagator` | `src/propagator_intersection.c`（新文件） | 相交约束传播 |
| 实现 `ContainmentPropagator` | `src/propagator_containment.c`（新文件） | 包含约束传播 |
| 实现 `ConnectionPropagator` | `src/propagator_connection.c`（新文件） | 端口连接传播 |

**预估规模**：约 500 行 C 代码

### 4.3 第三阶段：集成与 Web GUI 交互（P3-P4）

| 任务 | 说明 |
|------|------|
| 在 `constraint_graph_add_constraint()` 中自动创建 Propagator | 每次添加约束时，自动注册对应的传播器 |
| 在 `solver.h` 求解管线中集成传播循环 | `solver_solve()` 前先运行 `propagator_run_loop()` 缩小域 |
| Web GUI 拖拽交互的增量传播 | 节点坐标变化 → 自动触发邻接约束的传播 |
| 传播诊断与可视化 | 在 Web GUI 中展示传播队列状态、活跃/休眠传播器计数 |
| 传播器 subsumption 的图收缩 | subsumed 的约束在可视化中灰显，减少视觉噪音 |

---

## 附录 A：Gecode 与 Lv-00 Propagator 概念对照速查

| Gecode | Lv-00 | 关键差异 |
|--------|-------|---------|
| `Propagator` 抽象类 | `ConstraintPropagator` 结构体 + 函数指针 | Lv-00 用 C 风格的多态（函数指针表） |
| `propagate(Space&, ...)` | `propagator_propagate(self, graph)` | Lv-00 直接传递 ConstraintGraph 而非 Space |
| `IntVar` / `BoolVar` | `SymbolicCoord`（有理数/代数数/二次/超越） | Lv-00 变量类型更丰富（4种坐标类型） |
| `IntVar.min()/max()` | `symbolic_coord_get_lower_bound/upper_bound()` | Lv-00 需要新增域的精确上下界 API |
| `ViewArray<IntView>` | `subscribed_var_ids[]` + `graph_get_node()` | Lv-00 用节点 ID 数组作为视图 |
| `ES_FIX/ES_SUBSUMED/ES_FAILED` | `PROPAGATE_OK/SUBSUMED/CONFLICT/NO_CHANGE` | Lv-00 多了一个 `NO_CHANGE` 状态 |
| `Space::status()` 传播循环 | `propagator_run_loop()` | Lv-00 的循环更显式 |
| `Brancher`（分支） | `solver_handle_multiple_solutions()` | 复用现有的多解处理 |
| `Advisor`（建议者） | `propagation_queue` 中的自动入队逻辑 | Lv-00 用图邻接遍历替代事件订阅 |

---

## 附录 B：传播示例——三角形构造中的增量传播

```
场景: 用户在 Web GUI 中拖动三角形顶点 C

初始状态（传播前）:
  ConstraintGraph:
    A = (0, 0)     [已确定，不可变]
    B = (10, 0)    [已确定，不可变]
    C = (5, y_c)   [y_c 域: [0, 10]]
    AB = segment(A, B)
    BC = segment(B, C)  [约束: INCIDENCE(B, BC), INCIDENCE(C, BC)]
    CA = segment(C, A)  [约束: INCIDENCE(C, CA), INCIDENCE(A, CA)]

用户将 C 从 (5, 5) 拖到 (5, 8):

1. C.symbolic_coords[1] = 8（具体赋值）
2. graph_find_constraints_involving(C.id) → [INCIDENCE(C, BC), INCIDENCE(C, CA)]
3. 传播队列: [IncdProp(BC), IncdProp(CA)]

传播循环:
  Round 1:
    pop IncdProp(BC):
      - C 已确定 → BC 的方程完全确定（B=(10,0), C=(5,8)）
      - BC 上没有其他未定点需要缩小 → PROPAGATE_SUBSUMED
      - IncdProp(BC) 出队，标记休眠

    pop IncdProp(CA):
      - C 和 A 均已确定 → CA 完全确定 → PROPAGATE_SUBSUMED
      - IncdProp(CA) 出队，标记休眠

4. 队列为空 → 不动点到达
5. 所有变量已确定 → 不需要 solver 回退
6. Web GUI 更新：所有点绿色（已确定），约束满足 ✓
```

---

> **文档结束**
> 本文档详述了 Gecode 的 Propagator 架构如何应用于 Lv-00——将 `constraint_graph.h` 中的每种 `ConstraintType` 建模为一个独立的 `ConstraintPropagator`，当变量域变化时自动触发增量约束传播。核心结论：通过引入 `PropagationQueue` + `propagator_run_loop()` 传播循环，Lv-00 可以实现 Gecode 式的增量传播——当用户在 Web GUI 中拖拽一个点时，只传播受影响的约束（而非批量重解整个系统），同时通过 subsumption 机制自动休眠已满足的约束，显著提升交互式几何编辑的响应速度。
