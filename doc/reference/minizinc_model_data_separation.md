# Lv-00 参考设计：MiniZinc 模型与数据分离

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: [MiniZinc](https://www.minizinc.org/) —— 约束建模语言，模型与数据分离的典范设计
> **目标**: 借鉴 MiniZinc 的"模型+数据分离"架构，将 Lv-00 的公理包（.lvz 通用模型）与具体几何构造（数据实例）的关系形式化，映射到 `axiom_packages/` 组织和 `engine_scheduler.h` 多后端调度

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴要点](#2-核心借鉴要点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)

---

## 1. 项目概述

### 1.1 MiniZinc 是什么

MiniZinc 是 NICTA（现 Data61/CSIRO）和 Monash 大学开发的约束建模语言。它是约束求解社区的事实标准中间语言，被 Gecode、Chuffed、OR-Tools 等数十个求解器支持。MiniZinc 最核心的设计原则是**模型与数据分离**：

```minizinc
% ====== model.mzn (模型：通用约束描述) ======
int: n;                          % 参数声明（数据接口）
array[1..n] of int: demand;      % 参数声明
array[1..n] of var int: supply;  % 决策变量

constraint forall(i in 1..n)(supply[i] >= demand[i]);
solve minimize sum(i in 1..n)(supply[i]);

% ====== data.dzn (数据：具体实例) ======
n = 5;
demand = [10, 25, 15, 30, 20];
```

MiniZinc 的关键机制：

1. **模型文件（.mzn）**：声明参数类型、决策变量、约束关系和目标函数，但不指定具体数值
2. **数据文件（.dzn）**：为模型中的参数提供具体赋值
3. **编译时分离**：`minizinc model.mzn data.dzn` 在编译时将数据和模型合并为 FlatZinc（求解器原生格式）
4. **多后端调度**：同一模型可通过不同的 `--solver` 参数调用不同的后端求解器

### 1.2 为什么借鉴 MiniZinc

Lv-00 的 `axiom_packages/` 目录已经包含 60+ 个 `.lvz` 公理文件（如 `euclidean_plane.lvz`、`group_theory.lvz`），每个文件封装了一个数学理论域的公理集合——这天然就是 MiniZinc 式的"通用模型"。而用户在 Web GUI 中拖拽构造的"具体三角形/圆/证明"——这就是 MiniZinc 式的"数据实例"。借鉴 MiniZinc 意味着：

1. 将 `.lvz` 公理包明确定义为"模型层"（声明参数和公理模式）
2. 将用户的几何构造定义为"数据层"（实例化公理包中的通用结构）
3. 通过 `engine_scheduler.h` 的多后端路由，实现"同一模型 + 同一数据"可被不同后端求解/验证
4. 引入"模型版本管理"和"数据指纹"——当公理包升级时，自动检测已有构造是否仍然有效

---

## 2. 核心借鉴要点

### 2.1 模型与数据分离的三个层次

| MiniZinc 概念 | Lv-00 对应概念 | 映射说明 |
|-------------|---------------|---------|
| 模型文件 `.mzn` | `.lvz` 公理包（如 `euclidean_plane.lvz`） | 声明公理层、原始关系、类型签名 |
| 数据文件 `.dzn` | 用户构造 + `.lvz` 测试存档（`*_test_save.lvz`） | 实例化公理包中的通用模式 |
| 参数声明 `int: n` | `.lvz` 中公理的 `signature` 字段（如 `forall (A B : Point)...`） | 声明"需要多少个点"等参数约束 |
| 决策变量 `var int: x` | `ConstraintGraph` 中未赋值的 `SymbolicCoord` | 符号坐标即决策变量 |
| 约束 `constraint ...` | ConstraintGraph 中的 `Constraint` 边 | 几何关系 = 约束断言 |
| 目标函数 `solve minimize ...` | `optimize maximize/minimize`（见 OR-Tools 篇） | 优化目标 |
| FlatZinc 编译 | `scheduler_analyze_graph()` + `scheduler_solve()` | 图分析 + 分发求解 |
| `--solver` 参数 | `scheduler_select_backend()` | 多后端路由 |

### 2.2 MiniZinc 的多后端调度架构

MiniZinc 支持通过 `--solver` 标志在运行时切换后端：

```
$ minizinc model.mzn data.dzn --solver gecode    # 用 Gecode 求解
$ minizinc model.mzn data.dzn --solver chuffed   # 用 Chuffed 求解
$ minizinc model.mzn data.dzn --solver cplex     # 用 CPLEX 求解
```

这与 Lv-00 的 `engine_scheduler.h` 设计初衷完全吻合——`scheduler_select_backend()` 自动分析 `ConstraintGraph` 特征，为同一问题选择不同的求解后端（Groebner / SMT_Z3 / SMT_CVC5）。

### 2.3 模型版本化与数据指纹

MiniZinc 的模型和数据作为独立文件，各自有版本。当模型升级（如新增约束）时，旧数据文件可能不再兼容。Lv-00 的公理包同样面临版本化问题：

| 版本化需求 | MiniZinc 方式 | Lv-00 方式 |
|-----------|--------------|-----------|
| 模型版本标识 | 文件头注释 `% version: 1.0` | `manifest.json` 的 `"version"` 字段 |
| 依赖声明 | 无内置依赖系统 | `AXIOM_DEPENDENCY_MAP.md` + `INDEX.json` 的 `dependencies` |
| 数据指纹 | 手动管理 | `axiom_package_compute_content_hash()` —— SHA-256 内容哈希 |
| 模型升级检测 | 无 | `axiom_package_validate_dependencies()` |
| 自动降级 | 无 | `axiom_package_auto_degrade_invalidated()` |

---

## 3. Lv-00 映射方案

### 3.1 公理包 = 模型层，用户构造 = 数据层

```
MiniZinc 范式                    Lv-00 等价
──────────────────────────────────────────────────────
 model.mzn           ←→     euclidean_plane.lvz
   - 参数声明                     - 公理层声明 (I1, I2, I3, Parallel, ...)
   - 约束模式                     - 原始关系 (on, between, incident, ...)
   - 解的类型声明                 - 类型签名 (Point, Line, Circle, ...)

 data.dzn            ←→     euclidean_plane_test_save.lvz
   - 参数赋值                     - 具体点坐标 (A=0,0; B=600,0; C=300,400)
   - 具体数据                     - 具体线段/圆构造 + 约束

 FlatZinc            ←→     ConstraintGraph（编译后内部表示）
   - 变量                           - GeomNode（点/线/区域/端口/函数块）
   - 约束                           - Constraint（INCIDENCE/BETWEENNESS/...）
   - 目标函数                       - OptimizationObjective（可选）

 minizinc --solver    ←→     engine_scheduler.h
   - Gecode                        - GROEBNER
   - Chuffed                        - SMT_Z3
   - CPLEX                          - SMT_CVC5
```

### 3.2 Lv-00 的模型-数据实例化管线

```
┌─────────────────────────────────────────────────────────────┐
│              模型层 (axiom_packages/)                        │
│                                                             │
│  euclidean_plane.lvz                                        │
│  ├── manifest.json: {version, namespace, dependencies, ...} │
│  ├── layers[0]: incidence (I1, I2, I3)                     │
│  ├── layers[1]: order (B1, B2, B3, B4)                     │
│  ├── layers[2]: congruence (C1, C2, C3)                    │
│  └── layers[3]: parallels (Parallel)                       │
│                                                             │
│  INDEX.json: {packages: {euclidean_plane: {deps: [...]}}}   │
│  AXIOM_DEPENDENCY_MAP.md: 依赖可视化                        │
└──────────────────────────┬──────────────────────────────────┘
                           │ 加载 (axiom_package_load)
                           ▼
┌─────────────────────────────────────────────────────────────┐
│              数据层 (用户构造 / 测试存档)                      │
│                                                             │
│  euclidean_plane_test_save.lvz                              │
│  ├── @model: euclidean_plane v1.0.0  (引用上层模型)         │
│  ├── @construction:                                         │
│  │     point A(0, 0)                                        │
│  │     point B(600, 0)                                      │
│  │     point C(300, 400)                                    │
│  │     segment AB(A, B)                                     │
│  │     ...                                                  │
│  └── @proof: proposition median_concurrency {...}           │
│                                                             │
│  内容哈希: SHA-256("construction+proof payload")            │
└──────────────────────────┬──────────────────────────────────┘
                           │ 编译 (公式解析 → ConstraintGraph)
                           ▼
┌─────────────────────────────────────────────────────────────┐
│              FlatZinc 等价层 (ConstraintGraph)                │
│                                                             │
│  Nodes (GeomNode): id=1..N                                 │
│  Constraints: INCIDENCE(1,4), BETWEENNESS(2,3,5), ...      │
│  FuncBlocks: 交点/中点等函数块                                │
│                                                             │
│  GraphFeatures: (由 scheduler_analyze_graph 提取)            │
│    variable_nodes=3, incidence_constraints=6, ...           │
└──────────────────────────┬──────────────────────────────────┘
                           │ 路由 (scheduler_select_backend)
                           ▼
┌─────────────────────────────────────────────────────────────┐
│              多后端调度 (engine_scheduler.h)                   │
│                                                             │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                  │
│  │ GROEBNER │  │ SMT_Z3   │  │ SMT_CVC5 │  ...             │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘                  │
│       │             │             │                          │
│       └─────────────┼─────────────┘                          │
│                     ▼                                        │
│              GroebnerResult / SMTSolverResult                │
└─────────────────────────────────────────────────────────────┘
```

### 3.3 数据实例的组成结构

```c
/**
 * @brief Lv-00 数据实例（MiniZinc .dzn 等价结构）
 *
 * 一个数据实例是用户或测试构造的具体几何对象集合，
 * 它引用一个或多个公理包作为"模型框架"，
 * 在模型框架的约束下实例化具体点和关系。
 */
typedef struct LV00DataInstance {
    char *instance_name;              /**< 实例名称 */
    char *instance_id;                /**< 实例 UUID */

    /* 引用的模型（公理包） */
    char **model_references;          /**< 引用的公理包名称数组（如 "euclidean_plane"） */
    char **model_versions;            /**< 对应版本号数组 */
    int model_count;                  /**< 引用的模型数量 */

    /* 数据内容 */
    ConstraintGraph *construction;    /**< 具体几何构造图 */
    Proposition *proof_goal;          /**< 证明目标（可选） */
    ProofNavigator *proof_result;     /**< 证明结果（可选） */

    /* 版本与完整性 */
    char *content_hash;               /**< 数据内容的 SHA-256 哈希 */
    int64_t created_at;               /**< 创建时间戳 */
    int64_t last_verified_at;         /**< 最后一次验证通过的时间戳 */

    /* 与模型的兼容性检查结果 */
    bool model_compatible;            /**< 当前引用的模型是否兼容 */
    char *compatibility_note;         /**< 兼容性说明（如依赖版本不匹配） */
} LV00DataInstance;
```

### 3.4 模型升级时的数据实例验证

当公理包升级（如 `euclidean_plane.lvz v1.0.0 → v2.0.0`），已有的测试存档需要重新验证：

```c
/**
 * @brief 检查数据实例是否与当前加载的公理包兼容
 *
 * 当一个公理包版本升级时，所有引用该包的数据实例需要重新验证。
 * 验证逻辑：
 *  1. 比较 content_hash：如果实例引用模型时的哈希与当前模型哈希一致 → 兼容
 *  2. 如果版本号变化但公理层无增减 → 兼容（小版本更新）
 *  3. 如果公理层有增减 → 遍历实例的 ConstraintGraph，检查是否引用了
 *     已删除的公理或新增了与新公理冲突的约束
 *  4. 输出兼容性报告
 *
 * @param[in] instance     数据实例
 * @param[in] model_pkgs   当前加载的公理包集合
 * @param[in] pkg_count    公理包数量
 * @return true 兼容，false 不兼容（需手动修复或自动降级）
 */
bool data_instance_validate_models(const LV00DataInstance *instance,
                                    AxiomPackage **model_pkgs,
                                    int pkg_count);
```

### 3.5 映射到现有 axiom_packages/ + engine_scheduler.h

| 现有组件 | MiniZinc 角色 | 说明 |
|---------|-------------|------|
| `euclidean_plane.lvz` | 模型文件 `.mzn` | 欧氏几何公理 = 通用约束模式 |
| `*-test_save.lvz` | 数据文件 `.dzn` | 具体几何构造 + 证明 = 数据实例 |
| `manifest.json` (`version`, `dependencies`) | 模型元信息 | 版本号与依赖声明 |
| `INDEX.json` | MiniZinc 的包注册表 | 中央索引，记录所有可用模型 |
| `package_template.json` | 模型模板 | 新建公理包的脚手架 |
| `axiom_package_load()` | `minizinc` 编译器加载 `.mzn` | 解析公理包为内存结构 |
| `axiom_package_compute_content_hash()` | 内容指纹 | 用于版本匹配检测 |
| `axiom_package_validate_dependencies()` | 依赖检查 | 检测公理包是否引用了不存在的依赖 |
| `engine_scheduler.h` | `minizinc --solver` | 多后端选择与分发求解 |
| `scheduler_select_backend()` | 后端路由 | 基于 ConstraintGraph 特征自动选后端 |
| `GraphFeatures` | FlatZinc 的属性向量 | 描述图的变量数、非线性度等特征 |
| `SMTSolverResult` / `GroebnerResult` | 求解器输出 | 对应 `minizinc` 的 `----------` 分隔输出 |

### 3.6 模型-数据分离的 DSL 语法

```
// ============================================================
// Lv-00 模型-数据分离 DSL（MiniZinc 风格）
// ============================================================

// --- 模型层：euclidean_plane.lvz 中的公理声明 ---
@model euclidean_plane version=1.0.0
@namespace euclidean

// 参数声明（MiniZinc 风格的开放参数）
@param n_points : int >= 3;              // 至少需要 3 个点
@param dimension : int = 2;              // 默认 2D 平面

// 公理层：关联公理
@layer incidence order=0 {
    axiom I1: forall (A B : Point), A != B ->
              exists! (l : Line), on(A, l) AND on(B, l);
    axiom I2: forall (l : Line), exists (A B : Point),
              A != B AND on(A, l) AND on(B, l);
    axiom I3: exists (A B C : Point),
              ~collinear(A, B, C);
}

// --- 数据层：用户构造（euclidean_plane_test_save.lvz） ---
@instance triangle_construction
@using model euclidean_plane v1.0.0     // 引用模型

@Data {
    point A(0, 0);                       // 实例化参数
    point B(600, 0);
    point C(300, 400);
    segment AB(A, B);
    segment BC(B, C);
    segment CA(C, A);
}

@Proof {
    proposition median_concurrency {
        given: triangle(A, B, C);
        prove: concurrent(med_A, med_B, med_C);
    }
    prove median_concurrency using strategy=area_method;
}
```

---

## 4. 实现路线图

### 4.1 第一阶段：数据实例结构（P3）

| 任务 | 文件 | 说明 |
|------|------|------|
| 定义 `LV00DataInstance` 结构体 | `include/lv00/data_instance.h`（新文件） | 数据实例的核心数据结构 |
| 实现 `data_instance_create/destroy` | `src/data_instance.c`（新文件） | 数据实例的创建与销毁 |
| 实现数据实例的序列化/反序列化 | `src/data_instance.c` | JSON 格式的 `.lvz` 存档读写 |
| 实现内容哈希计算 | `src/data_instance.c` | SHA-256 指纹，用于版本匹配 |

**预估规模**：约 300 行 C 代码

### 4.2 第二阶段：模型-数据兼容性检查（P3）

| 任务 | 文件 | 说明 |
|------|------|------|
| 实现 `data_instance_validate_models()` | `src/data_instance.c` | 检查实例引用模型的版本兼容性 |
| 增强 `axiom_package_validate_dependencies()` | `src/axiom_pkg.c` | 添加跨包版本范围检查 |
| 实现自动降级策略 | `src/data_instance.c` | 当模型升级导致不兼容时，尝试自动降级 |
| 实现 `.lvz` 测试存档的批量验证工具 | `src/tools/validate_saves.c` | CLI 工具：验证所有测试存档与当前模型兼容 |

**预估规模**：约 350 行 C 代码

### 4.3 第三阶段：多后端透明调度（P3-P4）

| 任务 | 说明 |
|------|------|
| 扩展 `scheduler_select_backend()` | 支持模型层声明的后端偏好（如公理包标注 `preferred_backend: SMT_Z3`） |
| FlatZinc 风格的中间表示导出 | 将 `ConstraintGraph` 导出为 FlatZinc 兼容格式（可选，用于与 MiniZinc 生态互操作） |
| Web GUI 模型浏览器 | 在 Web GUI 中展示可用公理包（类似 MiniZinc IDE 的模型选择器） |
| 同一问题的多后端对比 | 运行同一构造在不同后端上，对比结果（交叉验证） |

---

## 附录 A：MiniZinc 与 Lv-00 概念对照速查

| MiniZinc | Lv-00 | 关键差异 |
|----------|-------|---------|
| `.mzn` + `.dzn` 两个文件 | `.lvz` 统一文件，通过 `@model` / `@instance` 区分 | Lv-00 选择单文件以简化管理 |
| `int: n` 参数声明 | 公理包 `@param` 声明 | Lv-00 参数是几何类型而不是简单整数 |
| `array[1..n] of var int` | `SymbolicCoord` 数组 | Lv-00 的符号坐标支持精确代数数 |
| `constraint forall(...)` | ConstraintGraph 中的 `Constraint` 集合 | Lv-00 约束类型更丰富（5种） |
| `solve minimize sum(...)` | `optimize maximize/minimize` | 扩展自 OR-Tools 借鉴方案 |
| FlatZinc | `ConstraintGraph` + `GraphFeatures` | 概念等价 |
| `--solver gecode` | `engine_scheduler.h` 的 `scheduler_select_backend()` | Lv-00 支持自动路由 + 回退链 |
| 无内置依赖管理 | `AXIOM_DEPENDENCY_MAP.md` + `INDEX.json` | Lv-00 更完善 |
| 无内容哈希 | `axiom_package_compute_content_hash()` | Lv-00 更安全 |

---

## 附录 B：模型升级示例——公理包 v1.0 → v2.0 的兼容性检查

```
场景: euclidean_plane.lvz 从 v1.0.0 升级到 v2.0.0

v1.0.0 公理: I1, I2, I3, B1-B4, C1-C3, Parallel
v2.0.0 公理: I1, I2, I3, B1-B4, C1-C3, Parallel, Playfair,
              Circle_Axiom（新增）

兼容性检查:
  1. 遍历 euclidean_plane_test_save.lvz 中的所有构造
  2. 检查每个构造是否只使用了 v1.0.0 中的公理 → 是 → 兼容
  3. 检查新增的 Circle_Axiom 是否与现有构造冲突 → 否（新公理不约束旧构造）
  4. 内容哈希比较: v1.0.0 模型哈希 ≠ v2.0.0 模型哈希
     → instance.model_compatible = true（因为旧公理无删减）
     → instance.compatibility_note = "v2.0.0 新增 Playfair 和 Circle_Axiom，旧构造仍然有效"

结果: GREEN_VERIFIED —— 旧实例在升级后的模型中仍然有效
```

---

> **文档结束**
> 本文档详述了 MiniZinc "模型与数据分离"架构如何应用于 Lv-00——将 `.lvz` 公理包定义为通用模型（声明参数、公理层、原始关系），将用户构造和测试存档定义为数据实例（实例化模型中的通用模式）。核心结论：通过形式化 `axiom_packages/` 中"模型层"与"数据层"的关系，并借助 `engine_scheduler.h` 的多后端透明调度，Lv-00 可以实现"同一公理模型 + 不同数据实例在不同后端上求解/验证"的灵活架构，同时通过内容哈希和兼容性检查保证模型升级时的实例可靠性。
