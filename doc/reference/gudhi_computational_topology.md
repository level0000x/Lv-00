# GUDHI 计算拓扑库参考文档

> 最后更新：2026-05-24 | 版本：v1.0

---

## 1. 项目概述

### 1.1 简介

GUDHI（Geometry Understanding in Higher Dimensions）是一个专注于**拓扑数据分析（TDA, Topological Data Analysis）**与**高维几何理解**的开源 C++ 库，由 INRIA（法国国家信息与自动化研究所）主导开发并维护。它提供了一套完整的工具链，用于从离散点集（点云）中提取、计算和分析底层空间的拓扑结构。

项目仓库：[github.com/GUDHI/gudhi-devel](https://github.com/GUDHI/gudhi-devel)

GUDHI 的核心能力包括：

- 构建多种单纯复形（Simplicial Complex），如 Rips 复形、Alpha 复形、Cech 复形、Witness 复形等
- 计算持久同调（Persistent Homology），提取拓扑不变量（Betti 数、持久图、持久条形码）
- 提供高效的单纯复形遍历和操作数据结构（Simplex Tree）
- 支持高维数据的降维、聚类和可视化

### 1.2 技术栈

| 层次 | 技术 | 说明 |
|------|------|------|
| 核心计算引擎 | C++ (C++14/17) | 所有拓扑算法和数据结构的高性能实现 |
| 用户接口 | Python (Cython 绑定) | 通过 Cython 暴露给 Python，兼容 NumPy/SciPy |
| 依赖库 | CGAL (计算几何)、Eigen (线性代数)、Boost | 提供底层几何计算和数值支持 |
| 构建系统 | CMake | 跨平台构建 |

GUDHI 的设计哲学是"C++ 负责性能，Python 负责可访问性"。C++ 核心处理计算密集的持久同调、复形构建等任务，Python 绑定则提供类 scikit-learn 风格的 API，使数据科学家能够在 Jupyter 环境中轻松使用。

### 1.3 许可证

GUDHI 采用 **MIT / GPLv3 双许可**模式：

- **MIT 许可**：适用于库的核心算法和数据结构部分，允许在闭源商业项目中使用
- **GPLv3 许可**：适用于依赖 GPL 组件的模块（如部分 CGAL 模块）

对于 Lv-00 项目而言，如果仅链接 GUDHI 的 MIT 许可部分（如 Simplex Tree 和持久同调核心算法），可以避免 GPL 传染性约束。实际集成时需要仔细审查所引用的头文件和链接库的许可证范围。

---

## 2. 核心借鉴点

GUDHI 为 Lv-00 项目的代数拓扑和几何验证体系提供了四个关键的借鉴方向，下面逐一展开。

### 2.1 单纯复形数据结构

**借鉴内容**：0-单纯形（点）、1-单纯形（边）、2-单纯形（三角形）的统一数学表示。

GUDHI 将任意维度的单纯形抽象为 `Simplex` 对象，并通过字典序统一管理。一个 k-单纯形由 k+1 个顶点定义，顶点按编号排序以确保唯一表示。这种统一抽象使得从 0 维到高维的单纯复形操作复用同一套接口。

**对 Lv-00 的启示**：

Lv-00 中的几何体（点、线段、三角形、四面体）天然构成了单纯复形。引入单纯复形表示后，几何体的组合结构可以自动推导拓扑属性，无需手工计算。例如：

- 一个三角形网格 → 一个 2-单纯复形 → 自动计算其 Euler 示性数和 Betti 数
- 一个四面体剖分 → 一个 3-单纯复形 → 验证其边界是否构成一个闭曲面

### 2.2 持久同调

**借鉴内容**：自动检测几何构造中的拓扑不变量。

持久同调的核心思想是：在多个尺度参数下构建"过滤"（filtration），观察拓扑特征（连通分支、孔洞、空洞）的出现（birth）和消失（death）时刻。持续越久的特征越可能是数据的"真实"拓扑结构。

| 拓扑特征 | 同调维度 | 对应 Betti 数 | Lv-00 中的含义 |
|----------|----------|---------------|----------------|
| 连通分支 | H₀ | Betti-0 (β₀) | 几何体分离验证 |
| 一维孔洞/环 | H₁ | Betti-1 (β₁) | 边界完整性检查 |
| 二维空洞/腔 | H₂ | Betti-2 (β₂) | 闭曲面封闭性验证 |

**对 Lv-00 的启示**：

持久同调可以直接作为 Lv-00 **证明的拓扑不变量检查器**。当一个几何构造声称具有某种拓扑性质时（例如"该区域的边界是一个简单闭曲线"），持久同调计算可以自动验证该声明是否正确。

### 2.3 Alpha Complex 与 Cech Complex

**借鉴内容**：从几何点云构建拓扑结构。

两种复形的比较：

| 特性 | Alpha Complex | Cech Complex |
|------|---------------|--------------|
| 构建方式 | Delaunay 三角剖分 + 半径过滤 | 以点为中心作球，球交即边 |
| 计算复杂度 | O(n log n)（得益于 Delaunay） | O(n³)（需检查所有三元组） |
| 拓扑正确性 | 同伦等价于 union of balls | 同伦等价于 union of balls |
| 适用场景 | 2D/3D 点云 | 理论分析、低维验证 |

**对 Lv-00 的启示**：

Lv-00 的几何构造（如点集配置、区域划分）可以视为高维空间中的点云，对应使用 Alpha Complex 进行拓扑分析。对于低维验证场景（2D/3D），Cech Complex 提供更精确的拓扑保证，可作为"黄金标准"。

### 2.4 Witness Complex

**借鉴内容**：大规模点集的高效拓扑近似。

Witness Complex 的关键思想是：从完整点集中选取少量"地标点"（landmarks），其余点作为"见证点"（witnesses），仅在地标点之间构建复形。这种方法将计算复杂度从 O(n²) 降至 O(|L| · n)，其中 |L| 是地标点数量。

**对 Lv-00 的启示**：

Lv-00 中某些复杂几何构造可能产生百万级顶点，此时完整的 Rips 或 Alpha 复形计算代价过高。Witness Complex 提供了一种可控精度的拓扑简化方案，允许在计算效率和拓扑保真度之间权衡。

### 2.5 Simplex Tree 数据结构

**借鉴内容**：高效的单纯复形存储和遍历。

Simplex Tree 是 GUDHI 中的核心数据结构，本质是一棵前缀树（Trie），每个节点代表一个单纯形，路径从根到节点即该单纯形的顶点序列（已排序）。关键操作的时间复杂度：

| 操作 | 复杂度 |
|------|--------|
| 插入单纯形 | O(σ · log N)，σ 为单纯形维数 |
| 查询某单纯形是否存在 | O(σ · log N) |
| 遍历某顶点的上邻接（coface） | O(k)，k 为结果数量 |
| 获取某单纯形的边界（boundary） | O(σ · k)，k 为 facet 数量 |

**对 Lv-00 的启示**：

Simplex Tree 的存储结构可以直接适配 Lv-00 的几何体层级关系。例如，一个三角形的边界（三条边）在 Simplex Tree 中通过查询子节点即可获得，无需显式维护边界映射表。

### 2.6 核心借鉴对照表

| GUDHI 概念 | Lv-00 对应概念 | 映射关系 | 优先级 |
|-----------|---------------|---------|--------|
| Simplex | `Simplex` / `GeometryAtom` | 直接映射，Lv-00 的几何原子即单纯形 | 高 |
| SimplicialComplex | `SimplicialComplex` | Lv-00 的几何体集合即复形 | 高 |
| Filtration | 几何构造步骤序列 | 构造过程的每个步骤对应过滤值 | 高 |
| PersistenceDiagram | Betti 数序列 | 持久图 → Betti 数验证 | 高 |
| AlphaComplex | `PointSetTopology` | 点集拓扑分析 | 中 |
| SimplexTree | `SimplexTree` 内部实现 | 复形的高效存储 | 中 |
| WitnessComplex | `ApproximateTopology` | 大规模几何体的拓扑近似 | 低 |

---

## 3. Lv-00 映射方案

### 3.1 总体思路

Lv-00 现有的 `algebraic_topology.lvz` 和 `point_set_topology.lvz` 目前停留在"纸上公理"阶段——它们声明了拓扑概念的数学定义，但未提供可计算的验证能力。引入 GUDHI 后，这些模块升级为"可计算的拓扑分析"。

核心数据流：

```
几何构造 (lvz)
    │
    ▼
单纯复形 (SimplicialComplex)
    │
    ├── 顶点 → 0-单纯形
    ├── 边   → 1-单纯形
    ├── 面   → 2-单纯形
    └── 体   → 3-单纯形
    │
    ▼
持久同调计算 (GUDHI)
    │
    ▼
Betti 数验证
    │
    ├── β₀ = ?   (预期 vs 实际)
    ├── β₁ = ?   (预期 vs 实际)
    └── β₂ = ?   (预期 vs 实际)
    │
    ▼
拓扑不变量结果
    ├── 通过 → 几何构造有效
    └── 失败 → 生成反例报告
```

### 3.2 C 代码示例：从几何构造到 Betti 数验证

以下代码展示了 Lv-00 如何通过 GUDHI 的 C++ 接口（经 C 包装层暴露）将几何构造转化为可计算的拓扑不变量：

```c
/*
 * 文件：lv00_topology_verify.c
 * 功能：从 Lv-00 几何构造出发，构建单纯复形并计算 Betti 数
 * 依赖：libgudhi_c_wrapper（GUDHI C++ 的 C 语言包装层）
 *
 * 核心流程：
 *   1. 从 Lv-00 几何数据构建 SimplexTree
 *   2. 批量插入单纯形（点/边/三角形）
 *   3. 调用 persist() 计算持久同调
 *   4. 提取各维度的 Betti 数
 *   5. 与预期值比对，返回验证结果
 */

#include "lv00/geometry_types.h"
#include "lv00/algebraic_topology.h"
#include "gudhi_c/simplex_tree.h"
#include "gudhi_c/persistence.h"

/* ---- 步骤 1：从 Lv-00 几何体构建单纯复形 ---- */

gudhi_SimplexTree*
lv00_build_complex_from_geometry(const Lv00Geometry* geom) {
    gudhi_SimplexTree* st = gudhi_simplex_tree_new();

    /* 1a. 插入 0-单纯形（顶点） */
    size_t n_vertices = lv00_geometry_vertex_count(geom);
    for (size_t i = 0; i < n_vertices; i++) {
        /* 过滤值：这里用顶点索引作为简单示例，
           实际应使用构造步骤的序列号 */
        double filtration = (double)i;
        gudhi_simplex_tree_insert_simplex(st, &i, 1, filtration);
    }

    /* 1b. 插入 1-单纯形（边） */
    size_t n_edges = lv00_geometry_edge_count(geom);
    for (size_t j = 0; j < n_edges; j++) {
        Lv00Edge e = lv00_geometry_get_edge(geom, j);
        int vertices[2] = { e.v0, e.v1 };
        double filtration = lv00_edge_construction_step(geom, j);
        gudhi_simplex_tree_insert_simplex(st, vertices, 2, filtration);
    }

    /* 1c. 插入 2-单纯形（三角形） */
    size_t n_faces = lv00_geometry_face_count(geom);
    for (size_t k = 0; k < n_faces; k++) {
        Lv00Face f = lv00_geometry_get_face(geom, k);
        int vertices[3] = { f.v0, f.v1, f.v2 };
        double filtration = lv00_face_construction_step(geom, k);
        gudhi_simplex_tree_insert_simplex(st, vertices, 3, filtration);
    }

    return st;
}

/* ---- 步骤 2：计算持久同调与 Betti 数 ---- */

typedef struct {
    int    betti_0;  /* 连通分支数 */
    int    betti_1;  /* 一维孔洞数（环） */
    int    betti_2;  /* 二维空洞数（腔） */
    double max_persistence_h0;
    double max_persistence_h1;
    double max_persistence_h2;
} Lv00BettiResult;

Lv00BettiResult
lv00_compute_betti_numbers(gudhi_SimplexTree* st) {
    Lv00BettiResult result = {0};

    /* 2a. 初始化过滤并计算持久同调 */
    gudhi_simplex_tree_initialize_filtration(st);
    gudhi_PersistenceResult* persist =
        gudhi_compute_persistence(st, /* homology_dimension_max = */ 3);

    /* 2b. 遍历持久区间，统计各维度的"无限"区间数量
     *     无限区间（death == infinity）的数量即对应该维度的 Betti 数 */
    size_t n_intervals = gudhi_persistence_interval_count(persist);
    for (size_t i = 0; i < n_intervals; i++) {
        int    dim    = gudhi_persistence_interval_dimension(persist, i);
        double birth  = gudhi_persistence_interval_birth(persist, i);
        double death  = gudhi_persistence_interval_death(persist, i);
        double persistence = death - birth;

        if (gudhi_is_infinity(death)) {
            /* 无限存活 → 该维度的 Betti 数贡献 */
            switch (dim) {
                case 0: result.betti_0++; break;
                case 1: result.betti_1++; break;
                case 2: result.betti_2++; break;
            }
        } else {
            /* 有限区间 → 记录最大持久度 */
            switch (dim) {
                case 0:
                    if (persistence > result.max_persistence_h0)
                        result.max_persistence_h0 = persistence;
                    break;
                case 1:
                    if (persistence > result.max_persistence_h1)
                        result.max_persistence_h1 = persistence;
                    break;
                case 2:
                    if (persistence > result.max_persistence_h2)
                        result.max_persistence_h2 = persistence;
                    break;
            }
        }
    }

    gudhi_persistence_free(persist);
    return result;
}

/* ---- 步骤 3：Betti 数验证 ---- */

typedef enum {
    LV00_TOPOLOGY_PASS,
    LV00_TOPOLOGY_FAIL_BETTI0,
    LV00_TOPOLOGY_FAIL_BETTI1,
    LV00_TOPOLOGY_FAIL_BETTI2,
    LV00_TOPOLOGY_ERROR_COMPLEX_NULL,
} Lv00TopologyStatus;

Lv00TopologyStatus
lv00_verify_topology(const Lv00Geometry* geom,
                      int expected_betti_0,
                      int expected_betti_1,
                      int expected_betti_2) {
    gudhi_SimplexTree* st = lv00_build_complex_from_geometry(geom);
    if (!st) return LV00_TOPOLOGY_ERROR_COMPLEX_NULL;

    Lv00BettiResult result = lv00_compute_betti_numbers(st);
    gudhi_simplex_tree_free(st);

    if (result.betti_0 != expected_betti_0)
        return LV00_TOPOLOGY_FAIL_BETTI0;
    if (result.betti_1 != expected_betti_1)
        return LV00_TOPOLOGY_FAIL_BETTI1;
    if (result.betti_2 != expected_betti_2)
        return LV00_TOPOLOGY_FAIL_BETTI2;

    return LV00_TOPOLOGY_PASS;
}
```

### 3.3 映射关系总结表

| Lv-00 模块 | GUDHI 对应组件 | 集成方式 | 状态 |
|-----------|---------------|---------|------|
| `algebraic_topology.lvz` | `gudhi::persistent_cohomology` | 通过 C 包装层调用 C++ API | 待实现 |
| `point_set_topology.lvz` | `gudhi::Alpha_complex` | 点坐标 → AlphaComplex → SimplexTree | 待实现 |
| `geometry_types.h` | `gudhi::Simplex_tree` | 新增 `Simplex` / `SimplicialComplex` 类型 | 待实现 |
| 证明验证器 | `gudhi::Persistence_landscape` | 比较理论预期与计算结果 | 待实现 |

---

## 4. 实现路线图

### 4.1 总体计划

整个集成工作分为三个阶段，预计共需 8-12 周（视资源投入而定）。每个阶段产出独立可用的功能增量。

### 4.2 Phase 1：单纯复形数据模型（第 1-4 周）

**目标**：在 `geometry_types.h` 中新增 `Simplex` 和 `SimplicialComplex` 数据类型，使其能表达 Lv-00 几何体的组合拓扑结构。

**任务清单**：

- [ ] 定义 `Simplex` 结构体（维度、顶点数组、过滤值）
- [ ] 定义 `SimplicialComplex` 结构体（单纯形集合、维度上界）
- [ ] 实现单纯形插入、查询、边界计算、上邻接遍历
- [ ] 实现从 `Lv00Geometry` 到 `SimplicialComplex` 的自动转换
- [ ] 编写单元测试：验证三角形边界包含三条边、K₄ 的 Euler 示性数等

**检验标准**：给定一个已知拓扑的几何体（如环面三角剖分），能正确构建 SimplicialComplex 并手工验证其 f-向量。

### 4.3 Phase 2：持久同调集成（第 5-8 周）

**目标**：编写 GUDHI C++ API 的 C 包装层，并在 Lv-00 中集成持久同调计算。

**任务清单**：

- [ ] 编写 `gudhi_c/` C 包装层（`simplex_tree.h`, `persistence.h`, `alpha_complex.h`）
- [ ] 配置 CMake 构建，链接 GUDHI 和 CGAL 库
- [ ] 实现 `lv00_compute_betti_numbers()` 函数
- [ ] 实现过滤值策略：按构造步骤分配过滤值
- [ ] 添加持久图（Persistence Diagram）和条形码（Barcode）的可视化导出
- [ ] 编写集成测试：标准拓扑空间（球面 S²、环面 T²、射影平面 RP²）

**检验标准**：对球面 S² 计算得 β₀=1, β₁=0, β₂=1；对环面 T² 计算得 β₀=1, β₁=2, β₂=1。

### 4.4 Phase 3：拓扑证明（第 9-12 周）

**目标**：将 Betti 数验证整合到 Lv-00 的证明流程中，实现几何构造的拓扑自动验证。

**任务清单**：

- [ ] 定义 `Lv00TopologyConstraint` 结构（预期 Betti 数 + 最大容忍持久度阈值）
- [ ] 集成到证明管线：几何构造完成后自动触发拓扑检查
- [ ] 失败时生成可读的反例报告（含持久图和差异分析）
- [ ] 对大规模几何体启用 Witness Complex 近似（Phase 2 预留接口，本阶段实现）
- [ ] 性能基准测试：环面剖分（10³ 顶点）、球面剖分（10⁴ 顶点）、随机点集拓扑估计

**检验标准**：对 100 个随机几何构造进行拓扑验证，误报率 < 0.1%，漏报率 < 0.5%。

### 4.5 分阶段汇总表

| 阶段 | 名称 | 时间 | 核心产出 | 依赖 | 风险等级 |
|------|------|------|----------|------|----------|
| Phase 1 | 单纯复形数据模型 | 第 1-4 周 | `Simplex`、`SimplicialComplex` 类型及基础操作 | 无 | 低 |
| Phase 2 | 持久同调集成 | 第 5-8 周 | C 包装层、Betti 数计算、持久图导出 | Phase 1 | 中（CGAL 编译问题） |
| Phase 3 | 拓扑证明 | 第 9-12 周 | 自动拓扑验证、反例报告、性能基准 | Phase 2 | 中（大规模性能、误报控制） |

### 4.6 风险与缓解

| 风险 | 严重程度 | 缓解措施 |
|------|----------|----------|
| CGAL/GUDHI 编译依赖复杂 | 高 | 预编译二进制分发包；Docker 构建环境；若 CGAL 不可用则降级为纯 Rips Complex |
| 大规模复形内存占用过大 | 中 | Witness Complex 近似；稀疏化过滤；按需构建而非全量构建 |
| GPLv3 许可证传染 | 中 | 仅链接 MIT 许可模块；C 包装层隔离；必要时自行实现 Simplex Tree |
| 持久同调计算结果与理论预期不一致 | 低 | 先在小规模已知拓扑空间上验证；与 Gudhi 官方 Tutorial 结果交叉比对 |

---

## 5. 附录

### 5.1 术语表

| 术语 | 英文 | 定义 |
|------|------|------|
| 单纯形 | Simplex | k-单纯形是 k+1 个仿射无关点构成的凸包。0-单纯形是点，1-单纯形是边，2-单纯形是三角形，3-单纯形是四面体 |
| 单纯复形 | Simplicial Complex | 一组单纯形的集合，满足：(1) 若某单纯形在复形中，其所有面也在复形中；(2) 两个单纯形的交要么为空，要么是公共面 |
| 过滤 | Filtration | 嵌套的单纯复形序列 ∅ = K₀ ⊆ K₁ ⊆ ... ⊆ Kₙ = K，每个复形对应一个"尺度"参数 |
| 持久同调 | Persistent Homology | 在多尺度过滤中追踪同调类的出现和消失，产生持久图（Persistence Diagram） |
| Betti 数 | Betti Number | 第 k 个 Betti 数 βₖ 表示 k 维"孔洞"的数量。β₀ = 连通分支数，β₁ = 环/洞数，β₂ = 空隙/腔数 |
| 持久图 | Persistence Diagram | 以 (birth, death) 为坐标的点集，每个点表示一个拓扑特征的诞生和消失尺度 |
| 同伦等价 | Homotopy Equivalence | 两个空间可以通过连续变形相互转化。同伦等价比同胚宽松，保留同调群 |
| f-向量 | f-vector | (f₀, f₁, f₂, ...)，其中 fₖ 为 k-单纯形的数量 |
| Euler 示性数 | Euler Characteristic | χ = Σ(-1)ᵏ fₖ = Σ(-1)ᵏ βₖ |

### 5.2 关键参考资源

| 资源 | 链接 | 说明 |
|------|------|------|
| GUDHI 官方文档 | [gudhi.inria.fr](https://gudhi.inria.fr/) | 用户手册和 API 参考 |
| GUDHI GitHub | [github.com/GUDHI/gudhi-devel](https://github.com/GUDHI/gudhi-devel) | 源代码和 Issue 跟踪 |
| GUDHI Python 教程 | [gudhi.inria.fr/python/latest/](https://gudhi.inria.fr/python/latest/) | Jupyter Notebook 示例集 |
| CGAL 库 | [www.cgal.org](https://www.cgal.org/) | 底层计算几何依赖 |
| Edelsbrunner & Harer, *Computational Topology: An Introduction* | AMS, 2010 | 计算拓扑经典教材 |
| Otter et al., "A roadmap for the computation of persistent homology" | EPJ Data Science, 2017 | 持久同调计算综述论文 |
| Boissonnat et al., *Geometric and Topological Inference* | Cambridge, 2018 | 几何与拓扑推断（GUDHI 理论基础） |

### 5.3 GUDHI 模块速查

| 模块 | 命名空间 | 主要功能 |
|------|----------|----------|
| Simplex Tree | `gudhi::Simplex_tree` | 单纯复形存储、过滤、遍历 |
| Alpha Complex | `gudhi::alpha_complex` | 从点云构建 Alpha 复形 |
| Rips Complex | `gudhi::rips_complex` | 从距离矩阵构建 Vietoris-Rips 复形 |
| Cech Complex | `gudhi::cech_complex` | 精确的球交复形（计算量大） |
| Witness Complex | `gudhi::witness_complex` | 大规模点云的拓扑近似 |
| Persistent Cohomology | `gudhi::persistent_cohomology` | 持久同调计算引擎 |
| Bottleneck Distance | `gudhi::bottleneck` | 持久图之间的 Bottleneck 距离 |
| Persistence Representations | `gudhi::representations` | 持久景观、持久图像等向量化表示 |

### 5.4 Lv-00 中关键 Betti 数参考值

| 拓扑空间 | β₀ | β₁ | β₂ | 说明 |
|----------|-----|-----|-----|------|
| 单点 | 1 | 0 | 0 | 一个连通分支，无其他特征 |
| 线段 | 1 | 0 | 0 | 可缩空间 |
| 圆周 S¹ | 1 | 1 | 0 | 一个连通分支，一个一维环 |
| 球面 S² | 1 | 0 | 1 | 一个连通分支，一个二维腔 |
| 环面 T² | 1 | 2 | 1 | 一个连通分支，两个一维环，一个二维腔 |
| Klein 瓶 | 1 | 1 (Z₂) | 0 | 一维同调有挠（torsion），注意持久同调对挠的处理 |
| 实射影平面 RP² | 1 | 0 (Z₂) | 0 | 有挠，持久同调在 Z₂ 系数下 β₁=1 |
| 实射影空间 RP³ | 1 | 0 (Z₂) | 0 (Z₂) | 注意系数域的选择 |

> **注意**：持久同调在默认系数域（如有理数 Q 或有限域 Z/pZ）下计算，对挠（torsion）不敏感。若 Lv-00 的几何构造可能产生挠元，需要在实现时选择合适的系数域。

---

*本文档为 Lv-00 项目内部参考文件，基于 GUDHI 3.10.x 版本编写。*
