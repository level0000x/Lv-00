# Lv-00 参考设计：SageMath 统一接口与多后端路由

> **版本**: 1.0.0  
> **日期**: 2026-05-24  
> **参考**: [SageMath](https://github.com/sagemath/sage) —— 统一接口覆盖 100+ 数学软件包的开源数学系统  
> **目标**: 借鉴 SageMath 的"统一接口 + 多后端引擎路由"架构，对应 Lv-00 的 `engine_scheduler.h` 多后端调度，提供 SageMath 作为最佳实践验证

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴要点](#2-核心借鉴要点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)

---

## 1. 项目概述

### 1.1 SageMath 是什么

SageMath 是由 William Stein 于 2005 年发起的开源数学软件系统，其标语是"Building the Car, Not Reinventing the Wheel"（造车而非重新发明轮子）。SageMath 的核心设计哲学：

1. **统一 Python 接口**：所有数学操作通过统一的 Python API 暴露，用户无需关心底层引擎
2. **多后端引擎路由**：底层调用 GAP（群论）、Singular（代数几何）、Maxima（符号微积分）、PARI/GP（数论）、NumPy/SciPy（数值计算）等 100+ 数学软件包
3. **自动引擎选择**：根据输入类型和操作语义，自动路由到最合适的后端引擎
4. **零拷贝互操作**：在引擎之间传递数据时尽可能使用共享内存/引用，避免序列化开销

```python
# SageMath 示例：用户操作统一接口，底层自动路由
M = matrix([[1, 2], [3, 4]])   # 矩阵 → 存储为 Sage 原生对象
M.det()                         # 求行列式 → 小矩阵用 Sage 原生，大矩阵可能路由到 PARI
M.eigenvalues()                 # 特征值 → 路由到 Maxima 或 NumPy（依矩阵大小和域）
factor(x^10 - 1)                # 因式分解 → 路由到 Singular 或 FLINT（依多项式类型）
```

### 1.2 为什么借鉴 SageMath

Lv-00 的 `engine_scheduler.h` 已经实现了一个多后端求解调度器（Gröbner 基、Z3、cvc5、Singular），但其设计受 polymake 启发，缺少 SageMath 那种"统一的用户接口 + 透明的自动路由 + 回退链"成熟模式的经验教训。SageMath 在 20 年间管理 100+ 后端引擎的经验，为 Lv-00 当前 4 个后端的调度策略提供了直接可参考的最佳实践。

---

## 2. 核心借鉴要点

### 2.1 统一接口 + 多后端路由

| SageMath 概念 | Lv-00 对应概念 | 映射说明 |
|---------------|---------------|---------|
| 统一 Python API（如 `factor()`, `integrate()`） | `scheduler_solve()` 统一入口 | 用户只调用一个函数，不关心后端 |
| 后端引擎注册表 | `SolverBackendType` 枚举 + `smtsolver_*` 注册 | `GROEBNER / SMT_Z3 / SMT_CVC5 / SMT_SINGULAR` |
| 引擎能力描述 | `GraphFeatures` 结构体 | 描述输入问题的特性 |
| 自动路由决策 | `scheduler_select_backend()` | 基于 `GraphFeatures` 选最优后端 |
| 回退链（fallback） | `SCHEDULER_MAX_FALLBACK_DEPTH` | 首选失败时自动尝试次选 |
| 引擎别名 | `scheduler_backend_type_to_string()` | 用户用字符串名引用后端（如 "z3"） |
| 结果统一化 | `GroebnerResult` / `SMTSolverResult` → 统一结果格式 | 无论走哪个后端，返回结构一致 |

### 2.2 SageMath 的路由策略经验

SageMath 20 年的后端路由策略可总结为 5 条经验法则：

| 法则 | SageMath 实践 | Lv-00 对应 |
|------|-------------|-----------|
| **1. 能力优于性能** | 先确保后端能解决问题，再考虑快慢 | `GraphFeatures` 中按约束类型分布选后端 |
| **2. 最小能力边界** | 每个后端声明自己能/不能处理什么 | 每个 `SolverBackendType` 附带能力位掩码 |
| **3. 惰性加载** | 后端库只在使用时才加载 | `smtsolver_create()` 的延迟链接 |
| **4. 缓存路由决策** | 同类问题复用之前的后端选择 | 在 `ConstraintGraph` 中缓存 `last_successful_backend` |
| **5. 显式退化** | 当高性能后端不可用时，静默退化到可用后端 | `scheduler_fallback` 链，带日志和警告 |

### 2.3 SageMath 的引擎互操作模式

SageMath 最精妙的设计之一是引擎间的数据传递采用**零拷贝或最小转换**策略。Lv-00 当前各后端（Gröbner 基、Z3、cvc5、Singular）之间需要独立编码/解码，这引入了不必要的开销。

| 数据传递模式 | SageMath 实现 | Lv-00 潜在实现 |
|-------------|-------------|---------------|
| **共享内存** | Python 对象直接传递给 C 库 | `SymbolicCoord` 的 `mpq_t` 字段被多个后端共享 |
| **惰性转换** | 只在需要时转换格式 | `SMT-LIB2` 编码推迟到 `smtsolver_check()` 最后一刻 |
| **母对象（Parent）** | Sage 的 Parent-Element 模式 | `ConstraintGraph` 作为所有后端操作的中心"母对象" |
| **结果缓存** | 计算结果自动缓存在对象上 | `GeomNode.symbolic_coords[]` 作为解缓存 |

---

## 3. Lv-00 映射方案

### 3.1 统一求解接口

借鉴 SageMath 的 `solve()` 统一接口，将 Lv-00 的多后端调度封装为单一切入点：

```c
/**
 * @brief 统一求解接口 —— Lv-00 的 "sagemath.solve()" 等价物
 *
 * 用户只需提供约束图和求解选项，调度器自动完成：
 * 1. 图特征提取
 * 2. 最优后端选择
 * 3. 求解执行
 * 4. 失败时的自动回退
 * 5. 结果统一化
 *
 * @param graph     约束图（输入，包含所有构造和约束）
 * @param options   求解选项（可为 NULL 使用默认值）
 * @param result    输出：统一求解结果
 * @return 调度器状态
 *
 * 使用示例：
 *   SolveResult result;
 *   scheduler_solve(graph, NULL, &result);
 *   // result.backend_used == SMT_Z3 (自动选择)
 *   // result.solutions[0] 是符号坐标数组
 */
SolveResult scheduler_solve(ConstraintGraph *graph,
                            const SolveOptions *options,
                            SolveResult *out_result);
```

### 3.2 后端能力描述与智能路由

借鉴 SageMath 的后端能力注册表，将 Lv-00 的路由规则形式化为能力矩阵：

```c
/**
 * @brief 后端能力描述符（SageMath 风格）
 *
 * 每个后端声明自身的能力范围，调度器据此做路由决策。
 * 这个位掩码比当前 engine_scheduler.h 中硬编码的 if-else
 * 路由规则更灵活、更可扩展。
 */
typedef struct BackendCapability {
    SolverBackendType type;

    /* 能力位掩码 */
    uint64_t capabilities;          /* 能力位掩码 (CAP_*) */
    int max_variables;              /* 可处理的变量数上限（0=无限制） */
    int max_degree;                 /* 可处理的最高多项式度数（0=无限制） */

    /* 约束类型支持 */
    bool supports_incidence;
    bool supports_betweenness;
    bool supports_intersection;
    bool supports_containment;
    bool supports_quantifiers;      /* 是否支持量词（用于 CONTAINMENT 约束） */

    /* 性能特征 */
    double linear_speed_factor;     /* 线性约束求解速度因子（1.0=基准） */
    double nonlinear_speed_factor;  /* 非线性约束求解速度因子 */
    bool native_geometry_support;   /* 是否有原生几何类型支持 */
} BackendCapability;

/* 能力位掩码位定义 */
#define CAP_LINEAR_ARITHMETIC   (1 << 0)  /* 线性算术理论 */
#define CAP_NONLINEAR_ARITHMETIC (1 << 1) /* 非线性算术理论 */
#define CAP_QUANTIFIERS         (1 << 2)  /* 量词支持 */
#define CAP_GROEBNER            (1 << 3)  /* Gröbner 基方法 */
#define CAP_SYMBOLIC_FACTOR     (1 << 4)  /* 符号因式分解 */
#define CAP_UNSAT_CORE          (1 << 5)  /* UNSAT 核心提取 */
#define CAP_INCREMENTAL         (1 << 6)  /* 增量求解支持 */

/* 预定义的后端能力表 */
static const BackendCapability BACKEND_CAPABILITIES[] = {
    [GROEBNER] = {
        .type = GROEBNER,
        .capabilities = CAP_LINEAR_ARITHMETIC | CAP_NONLINEAR_ARITHMETIC
                      | CAP_GROEBNER | CAP_SYMBOLIC_FACTOR,
        .max_variables = 0,    /* 无硬限制 */
        .max_degree = 2,       /* 度数≤2时最优 */
        .supports_quantifiers = false,
        .linear_speed_factor = 2.0,
        .nonlinear_speed_factor = 1.0,
    },
    [SMT_Z3] = {
        .type = SMT_Z3,
        .capabilities = CAP_LINEAR_ARITHMETIC | CAP_NONLINEAR_ARITHMETIC
                      | CAP_QUANTIFIERS | CAP_UNSAT_CORE | CAP_INCREMENTAL,
        .max_variables = 0,
        .max_degree = 0,
        .supports_quantifiers = true,
        .linear_speed_factor = 1.0,
        .nonlinear_speed_factor = 1.5,
    },
    [SMT_CVC5] = {
        .type = SMT_CVC5,
        .capabilities = CAP_LINEAR_ARITHMETIC | CAP_NONLINEAR_ARITHMETIC
                      | CAP_QUANTIFIERS | CAP_UNSAT_CORE,
        .max_variables = 0,
        .max_degree = 0,
        .supports_quantifiers = true,
        .linear_speed_factor = 1.0,
        .nonlinear_speed_factor = 1.3,
    },
    [SMT_SINGULAR] = {
        .type = SMT_SINGULAR,
        .capabilities = CAP_LINEAR_ARITHMETIC | CAP_NONLINEAR_ARITHMETIC
                      | CAP_GROEBNER | CAP_SYMBOLIC_FACTOR,
        .max_variables = 0,
        .max_degree = 0,
        .supports_quantifiers = false,
        .linear_speed_factor = 1.5,
        .nonlinear_speed_factor = 2.0,
    },
};
```

### 3.3 基于能力矩阵的自动路由

参考 SageMath 的"能力优先"路由策略，将 `engine_scheduler.h` 中的硬编码路由规则替换为基于能力矩阵的评分系统：

```c
/**
 * @brief 基于后端能力矩阵的路由评分（SageMath 风格）
 *
 * 每个后端对给定的 GraphFeatures 计算一个加权分数，
 * 分数最高的后端被选中。这与当前硬编码的 if-else 链
 * 相比，更容易添加新后端、调整路由优先级。
 *
 * 评分维度：
 * 1. 能力覆盖度（约束类型是否被支持）
 * 2. 性能匹配度（变量数/度数是否在后端最优范围内）
 * 3. 可用性（后端是否已注册且可调用）
 */
SolverBackendType scheduler_select_backend_v2(const GraphFeatures *features,
                                               const SolverBackendType *fallback_chain,
                                               int fallback_length);
```

### 3.4 统一结果格式

SageMath 的关键设计之一是**无论走哪个后端，最终返回的 Python 对象类型一致**。Lv-00 当前 `GroebnerResult` 和 `SMTSolverResult` 是不同的结构体。借鉴 SageMath，定义统一结果：

```c
/**
 * @brief 统一求解结果（SageMath 风格）
 *
 * 无论后端是 Gröbner、Z3、cvc5 还是 Singular，
 * 求解结果都以统一的 SolveResult 结构返回。
 * 附加元数据说明实际使用的后端和回退历史。
 */
typedef struct SolveResult {
    bool success;
    SolverStatus status;

    /* 解数据 */
    SymbolicCoord **solutions;      /* 解坐标数组 */
    int solution_count;

    /* 路由元数据 */
    SolverBackendType backend_used;  /* 实际使用的后端 */
    SolverBackendType *fallback_chain; /* 尝试过的回退链 */
    int fallback_count;

    /* 性能指标 */
    double solve_time_ms;            /* 求解耗时（毫秒） */
    int smtlib2_output_size;         /* SMT-LIB2 输出大小（字节）, 0 表示未走 SMT */

    /* 错误诊断 */
    char error_message[512];
} SolveResult;
```

### 3.5 映射到现有 engine_scheduler.h

| 现有 `engine_scheduler.h` 结构 | SageMath 借鉴后的改进 |
|-------------------------------|---------------------|
| `GraphFeatures` | 保留，增加 `preferred_geometry_domain`（欧几里得/射影/双曲） |
| `scheduler_analyze_graph()` | 保留，增加后端能力矩阵的查询接口 |
| `scheduler_select_backend()` | 替换为基于能力评分的选择算法（见 3.3） |
| `SCHEDULER_MAX_ROUTING_RULES` (32) | 保留，但路由规则从硬编码改为能力矩阵动态计算 |
| 硬编码 if-else 路由链 | 替换为 `BackendCapability` 表 + 评分函数 |
| `scheduler_solve()` | 封装为统一接口 `SolveResult scheduler_solve(graph, opts, out_result)` |

---

## 4. 实现路线图

### 4.1 第一阶段：后端能力矩阵（P2）

| 任务 | 文件 | 说明 |
|------|------|------|
| 定义 `BackendCapability` 结构体和能力位掩码 | `include/lv00/engine_scheduler.h` | 扩展现有调度器头文件 |
| 实现 `BACKEND_CAPABILITIES[]` 静态表 | `src/engine_scheduler.cpp` | 预定义 4 个后端的能力描述 |
| 实现 `scheduler_get_backend_capability()` | `src/engine_scheduler.cpp` | 查询后端能力 |
| 实现 `scheduler_select_backend_v2()` | `src/engine_scheduler.cpp` | 基于能力评分的路由算法 |

**预估规模**：约 200 行 C++ 代码

### 4.2 第二阶段：统一结果格式（P2）

| 任务 | 文件 | 说明 |
|------|------|------|
| 定义 `SolveResult` 统一结果结构 | `include/lv00/engine_scheduler.h` | 替代 `GroebnerResult`/`SMTSolverResult` 的对外接口 |
| 实现 `solve_result_create/destroy` | `src/engine_scheduler.cpp` | 统一结果的构造/析构 |
| 将 `scheduler_solve()` 迁移为统一接口 | `src/engine_scheduler.cpp` | 内部仍调用原有后端，输出统一格式 |
| 废弃直接调用 `solve_algebraic_system()` 的旧代码路径 | 全局 | 所有调用方改为 `scheduler_solve()` |

**预估规模**：约 150 行 C++ 代码 + 重构 100 行

### 4.3 第三阶段：惰性加载与流水线优化（P3+）

| 任务 | 说明 |
|------|------|
| 后端库惰性加载 | 类似 SageMath 的"使用时加载"，首次 `smtsolver_create()` 时才 `dlopen` |
| 零拷贝数据传递 | `SymbolicCoord.mpz_value` 直接引用传给后端，避免序列化 |
| 路由决策缓存 | 同类问题复用之前成功的后端选择 |
| 详细性能指标采集 | 每次求解记录耗时、内存、编码大小等指标，用于后续调优 |

---

## 附录 A：SageMath 与 Lv-00 调度器对照速查

| SageMath | Lv-00 engine_scheduler.h | 关键差异 |
|----------|-------------------------|---------|
| `factor(f)` → Singular/FLINT | `scheduler_solve(g)` → Gröbner/SMT | SageMath 无几何约束原生支持 |
| Parent-Element 模式 | `ConstraintGraph` 作为中心母对象 | Lv-00 的母对象是图结构而非代数结构 |
| Python 统一接口 | C API `scheduler_solve()` | Lv-00 无 Python 层 |
| 100+ 后端 | 4 后端 (Gröbner/Z3/cvc5/Singular) | Lv-00 更聚焦几何领域 |
| 用户可指定后端 | `SolverBackendType preferred_backend` | 用户可通过选项覆盖自动路由 |

---

## 附录 B：能力评分伪代码

```
function scheduler_select_backend_v2(features, fallback_chain):
    best_score = -inf
    best_backend = GROEBNER  // 最稳定兜底

    for each backend in [GROEBNER, SMT_Z3, SMT_CVC5, SMT_SINGULAR]:
        cap = BACKEND_CAPABILITIES[backend]

        // 1. 能力覆盖评分
        score = 0
        if features.contains_quantifiers and cap.supports_quantifiers:
            score += 100  // 量词支持 = 必备
        elif features.contains_quantifiers and !cap.supports_quantifiers:
            score = -inf  // 无量词支持 = 不可用

        // 2. 约束类型覆盖
        if features.incidence_count > 0 and cap.supports_incidence:
            score += 10
        if features.betweenness_count > 0 and cap.supports_betweenness:
            score += 10
        // ... 类似处理其他约束类型

        // 3. 性能匹配度
        nonlinear_ratio = features.nonlinear_constraints / features.total_constraints
        if nonlinear_ratio > 0.3:
            score += 30 * cap.nonlinear_speed_factor  // 非线性密集，SMT 更快
        else:
            score += 20 * cap.linear_speed_factor

        // 4. 可用性检查
        if !scheduler_is_backend_available(backend):
            score -= 50

        if score > best_score:
            best_score = score
            best_backend = backend

    return best_backend
```

---

> **文档结束**  
> 本文档详述了 SageMath "统一接口 + 多后端引擎路由"架构如何对应 Lv-00 的 `engine_scheduler.h` 多后端调度。核心结论：SageMath 20 年管理 100+ 后端的 5 条经验法则（能力优先、最小边界、惰性加载、缓存决策、显式退化）可以直接指导 Lv-00 调度器的优化——用 `BackendCapability` 能力矩阵替代当前的硬编码 if-else 路由链，用 `SolveResult` 统一结果格式消除 `GroebnerResult`/`SMTSolverResult` 的差异。