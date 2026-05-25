# Lv-00 参考落地设计文档：Draco 3D 几何数据压缩库

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: [Draco](https://github.com/google/draco) —— Google 开发的开源 3D 几何数据压缩库
> **目标**: 借鉴 Draco 的预测编码、Edgebreaker 拓扑压缩、属性分层压缩和熵编码后端选择策略，将 Lv-00 的几何构造序列、约束图拓扑和节点属性数据实现高效压缩与序列化

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴要点](#2-核心借鉴要点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)
5. [附录](#5-附录)

---

## 1. 项目概述

### 1.1 Draco 是什么

Draco 是 Google 开发的开源 3D 几何数据压缩库，专门用于压缩和解压缩三维几何网格和点云数据。项目采用 C++ 实现，以 Apache-2.0 许可证发布，目前已被广泛集成到 Chromium、Filament、three.js、Babylon.js 等主流图形引擎中。Draco 的核心设计哲学是：**以解码时间换取存储和传输带宽**——在保持视觉质量几乎不变的前提下，将 obj/ply 格式的原始网格数据压缩到原来的 1/10 到 1/20。

Draco 的压缩策略基于"预测编码 + 熵编码"的混合架构：

```
原始网格 (obj/ply)
    │
    ├─► 拓扑编码器 (Edgebreaker)    ─► 角表 → CLERS 符号序列
    │
    ├─► 几何编码器 (预测编码)       ─► 平行四边形预测残差
    │
    ├─► 属性编码器 (分层压缩)       ─► 法线/UV/颜色分别处理
    │
    └─► 熵编码 (rANS/算术编码)      ─► 符号序列压缩为比特流
                                        │
                                        ▼
                                  .drc 文件 (Draco 压缩格式)
```

关键数值特征：

| 指标 | 数值 |
|------|------|
| 典型压缩比 (obj → draco) | 10:1 至 20:1 |
| 有损模式压缩比 | 可达 50:1 |
| 编码速度 (单线程) | 约 50K 三角面/秒 |
| 解码速度 (单线程) | 约 200K 三角面/秒 |
| Web 解码 (WASM) | 约 80K 三角面/秒 |
| 代码规模 | C++ 约 15 万行 |

### 1.2 Draco 核心机制详解

Draco 的压缩管线由四个核心模块构成，它们之间是流水线关系：

#### 1.2.1 拓扑编码器：Edgebreaker 算法

Edgebreaker 是 Draco 拓扑压缩的核心算法。它通过遍历网格的所有面（三角形），为每个面分配一个 CLERS 符号（C=继续, L=左, E=结束, R=右, S=分裂），从而将整个网格的连接关系编码为五个符号的序列。对于典型的三角形网格，每个面仅需约 1.8 bit。

```
Edgebreaker 遍历示例：

        v0
       /  \
      / C  \
    v1─────v2
      \  R /
       \  /
        v3

CLERS 符号序列: C, R (仅两个符号编码两个三角形)
```

Edgebreaker 的关键优势：
- 每个三角面编码开销极低（约 1.8 bit，原始存储每个面需 3 × 32 bit 索引 = 96 bit）
- 解码只需线性扫描 CLERS 序列即可重建完整拓扑
- 对非流形网格有扩展变体支持

#### 1.2.2 几何编码器：平行四边形预测

对于顶点坐标（通常用 32 位浮点数存储），Draco 不直接存储绝对值，而是采用"预测 + 残差编码"的策略。最常用的预测器是平行四边形预测：

```
给定三角形 △ABC 和其相邻三角形 △BCD（共享边 BC）：

         A          D
        / \        / \
       /   \      /   \
      B─────C    B─────C

平行四边形预测：D_pred = B + C - A
残差：D_delta = D_actual - D_pred

由于 D_delta 通常非常小（邻近顶点几何趋向光滑），可以用更少的比特编码
```

除了平行四边形预测，Draco 还支持：
- **多阶平行四边形预测**：利用更远的邻居顶点（如共享 A 的另一个邻居）进行高阶预测
- **位移预测**：对网格序列（动画网格）利用帧间相关性的时间预测
- **法线感知量化**：对残差进行量化时考虑局部法线方向，优先保留与表面垂直方向的精度

#### 1.2.3 属性编码器：分层分离

Draco 将网格属性分为多个独立的数据流，每个流使用最适合的编码策略：

| 属性类型 | 编码策略 | 关键技术 |
|----------|---------|---------|
| 顶点位置 | 预测 + 残差 + 量化 | 平行四边形预测，可变精度量化 |
| 法线 | 球面坐标 + 八面体映射 | 法线映射到二维八面体投影，2 × 8 bit 编码 |
| 纹理坐标 (UV) | 预测 + 残差 | 与位置类似的平行四边形预测 |
| 顶点颜色 | 预测 + 残差 + 量化 | 颜色空间变换 (RGB→YCoCg) + 差分编码 |
| 通用属性 | 预测 + 残差 | 用户自定义属性可插拔编码器接口 |

这种分层分离设计使得每类属性可以采用最优的压缩策略，同时支持按需解压（如只想加载拓扑而不加载 UV，可以跳过 UV 数据流）。

#### 1.2.4 熵编码后端：rANS 与算术编码

Draco 的熵编码模块将预测残差和 CLERS 符号转换为紧凑的比特流。它支持两种后端：

| 后端 | 特点 | 适用场景 |
|------|------|---------|
| **rANS**（range Asymmetric Numeral Systems） | 高吞吐量，接近算术编码的压缩率，状态机实现简单 | Draco 默认后端 |
| **算术编码** | 理论上最优的熵编码压缩率 | 需要极致压缩率的场景 |

rANS 的优势在于它在压缩率和编码速度之间取得了良好平衡——编码速度是算术编码的 2-3 倍，压缩率仅差约 1%。

### 1.3 为什么借鉴 Draco

Lv-00 的核心数据结构是约束图（`constraint_graph.h`），它存储了几何构造的节点（点、线、圆等）和它们之间的约束关系（距离、角度、共线等）。将复杂几何构造保存为文件（.lvz 格式）时，面临与 3D 网格压缩类似的问题：

1. **构造序列**：类似于网格拓扑（哪些顶点构成哪些面），构造操作序列（"以 A、B 为圆心画圆，交点为 C"）可以高效压缩
2. **坐标数据**：每个节点的坐标具有几何相关性（邻近节点坐标相近），适合预测编码
3. **属性数据**：节点的标签、颜色、锁定状态等属性应与结构数据分离压缩
4. **约束关系**：约束图的边（距离约束、角度约束等）是稀疏的，可以像网格拓扑一样编码

Draco 的预测编码、Edgebreaker 拓扑压缩和属性分层压缩为 Lv-00 的 .lvz 序列化格式设计提供了直接的借鉴路径。

---

## 2. 核心借鉴要点

### 2.1 借鉴点一：预测编码策略

Draco 的预测编码核心思想是：**不存绝对坐标，存预测值与实际值的残差**。由于残差通常远小于绝对值（特别是在光滑曲面上），可以用更少的比特数编码。

在 Lv-00 的几何构造中，许多节点的坐标并非任意的——它们由构造规则决定。例如：

- **中点**：`midpoint(P, A, B)` → `P = (A + B) / 2`，预测值与实际值残差为零
- **垂足**：`foot(P, L, A)` → P 在直线 L 上，残差主要来自浮点舍入
- **交点**：`intersection(P, L1, L2)` → P 满足两个线性方程，残差来自求解精度

对于这些"由其他节点推导出的节点"，天然适合预测编码框架：利用它们的构造关系计算预测坐标，编码实际坐标与预测坐标的差值。

#### 预测编码策略分类

| 预测策略 | Draco 中应用 | Lv-00 可迁移场景 | 残差预期 |
|----------|-------------|-----------------|---------|
| 平行四边形预测 | 网格顶点位置 | 平行四边形构造点、对称点 | 极小（接近零） |
| 线性预测 | 纹理坐标 UV | 等分点、中点、延长线点 | 零（精确构造） |
| 重心预测 | — | 三角形内的点（重心坐标） | 零（精确构造） |
| 多阶预测 | 高阶平滑网格 | 多步构造链的末端节点 | 累积浮点误差 |
| 零阶预测 | 孤立顶点 | 自由点（鼠标点击坐标） | 完整坐标值 |

### 2.2 借鉴点二：Edgebreaker 拓扑压缩

Edgebreaker 将网格拓扑（顶点与三角面的连接关系）编码为 CLERS 符号序列。在 Lv-00 中，约束图同样具有"拓扑结构"——哪些节点之间存在约束关系，以及这些约束构成什么样的子图模式。

Lv-00 约束图的"拓扑编码"概念：

```
约束图（原始）：
  Node A ──[距离=5]──► Node B
    │                    │
  [角度=90°]         [共线]
    │                    │
    ▼                    ▼
  Node C ◄──[等长]─── Node D

约束图拓扑编码（类比 Edgebreaker）：
  1. 选取起始约束边 (A, B, 距离)
  2. 沿约束关系遍历图，为每条边/每个约束模式分配符号
  3. 编码为约束符号序列
```

由于 Lv-00 的约束图不是规则的三角形网格，不能直接套用 Edgebreaker 的 CLERS 符号系统，但可以借鉴其**遍历编码**的思想：用某种规范的遍历顺序（如 BFS/DFS 按约束类型优先级）遍历约束图，将约束类型和连接关系编码为紧凑序列。

### 2.3 借鉴点三：属性压缩分离

Draco 将位置、法线、UV、颜色分属不同的数据流。Lv-00 的每个约束图节点也有多种属性，适合分层分离：

| Lv-00 节点属性 | 压缩策略 | 对应 Draco 策略 |
|---------------|---------|----------------|
| 坐标 (x, y) | 预测编码 + 残差量化 | 顶点位置编码 |
| 节点类型 (点/线/圆/...) | 枚举压缩 (Huffman) | 通用属性编码 |
| 标签文本 | 字符串压缩 (LZ4/Zstd) | 通用属性编码 |
| 约束类型 | 枚举 + 差分编码 | 通用属性编码 |
| 锁定/可见状态 | 位图打包 | 通用属性编码 |
| 颜色/样式 | 颜色空间变换 + 量化 | 顶点颜色编码 |

### 2.4 借鉴点四：熵编码后端选择

Draco 支持 rANS 和算术编码两种熵编码后端。对于 Lv-00 的 .lvz 序列化格式，同样应该支持可插拔的熵编码后端：

| 后端 | 压缩率 | 编码速度 | 解码速度 | Lv-00 推荐场景 |
|------|-------|---------|---------|---------------|
| **rANS** | 接近理论最优 (~1% 差距) | 快 | 快 | 默认选项 |
| **Huffman** | 中等（依赖于符号分布） | 最快 | 最快 | 实时保存/快速预览 |
| **算术编码** | 最优（理论上达到熵极限） | 慢 | 慢 | 长期归档/网络传输 |
| **LZ4/Zstd** | 中等（通用压缩） | 极快/中 | 极快/中 | 字符串类属性数据 |

### 2.5 借鉴对照总表

| Draco 概念 | Draco 实现 | Lv-00 对应 | 借鉴价值（1-5 星） |
|-----------|-----------|-----------|-------------------|
| Edgebreaker 拓扑编码 | CLERS 符号遍历三角形网格 | 约束图 BFS 遍历编码，构造操作序列 | ★★★★★ |
| 平行四边形预测 | `D_pred = B + C - A` | 构造推导节点的坐标预测（中点、交点等） | ★★★★★ |
| 多阶预测 | 利用更多邻居改善预测精度 | 利用多步构造链上下文改善预测 | ★★★★☆ |
| rANS 熵编码 | 不对称数字系统，高吞吐量 | .lvz 格式的符号序列熵编码 | ★★★★★ |
| 属性分层压缩 | 位置/法线/UV/颜色分数据流 | 坐标/类型/标签/约束分层存储 | ★★★★★ |
| 法线八面体映射 | 3D 法线→2D 八面体投影 | —（Lv-00 为 2D，不适用） | ★☆☆☆☆ |
| 自适应量化 | 根据局部曲率调整量化精度 | 根据约束类型调整坐标量化精度 | ★★★★☆ |
| 网格序列压缩 | 帧间差分编码 | 历史状态（undo/redo）差分存储 | ★★★☆☆ |
| 可插拔编码器接口 | 用户自定义属性编码器 | .lvz 格式的自定义属性扩展点 | ★★★★☆ |

---

## 3. Lv-00 映射方案

### 3.1 核心函数：geometry_compress()

将 Lv-00 的约束图编码为 Draco 风格的压缩格式。该函数接收一个约束图，执行以下步骤：

```
geometry_compress(graph) → compressed_buffer
    步骤 1: 拓扑编码 (edgebreaker_encode)
        1.1 选取起始节点（坐标极值点）
        1.2 BFS 遍历约束图，按约束类型优先级决定遍历顺序
        1.3 输出：构造操作符号序列 + 节点连通性编码
    
    步骤 2: 坐标预测编码 (predictive_encode)
        2.1 对每个节点，根据其构造类型选择预测器
        2.2 计算预测坐标与残差
        2.3 对残差进行自适应量化
    
    步骤 3: 属性分层编码 (attribute_layered_encode)
        3.1 节点类型 → 枚举 Huffman 编码
        3.2 标签文本 → LZ4 压缩
        3.3 约束类型 → 差分 + Huffman 编码
        3.4 样式属性 → 位图打包
    
    步骤 4: 熵编码 (entropy_encode)
        4.1 将所有符号序列送入 rANS 编码器
        4.2 输出紧凑比特流
```

#### C 伪代码实现

```c
// =============================================
// lvz_compress.h — Draco 风格的约束图压缩
// =============================================

#include "constraint_graph.h"
#include "lvz_format.h"

/* ================================================================
 * 压缩配置
 * ================================================================ */
typedef enum {
    LVZ_ENTROPY_RANS,        // rANS 熵编码（默认）
    LVZ_ENTROPY_HUFFMAN,     // Huffman 编码
    LVZ_ENTROPY_ARITHMETIC,  // 算术编码
    LVZ_ENTROPY_NONE         // 无熵编码（调试用）
} LvzEntropyBackend;

typedef enum {
    LVZ_PREDICT_PARALLELOGRAM,  // 平行四边形预测
    LVZ_PREDICT_MIDPOINT,       // 中点预测
    LVZ_PREDICT_LINEAR,         // 线性外推/内插
    LVZ_PREDICT_BARYCENTRIC,    // 重心坐标预测
    LVZ_PREDICT_NONE            // 不预测（直接存绝对坐标）
} LvzPredictor;

typedef struct {
    int quantize_bits;                 // 量化比特数 (默认 16)
    LvzEntropyBackend entropy_backend; // 熵编码后端
    bool use_lossy_compress;           // 是否允许有损压缩
    double lossy_tolerance;            // 有损容忍误差
} LvzCompressConfig;

/* ================================================================
 * 阶段 1: Edgebreaker 式拓扑编码
 *
 * 将约束图的构造序列编码为紧凑的符号序列。
 * 类比 Draco Edgebreaker 的 CLERS 符号：
 *   C = CONTINUE  (沿用当前活跃节点继续构造)
 *   L = LEFT_ADD  (新增节点，连接到当前活跃节点左侧)
 *   R = RIGHT_ADD (新增节点，连接到当前活跃节点右侧)
 *   E = END_BRANCH(当前分支结束，回溯)
 *   S = SPLIT     (分裂出新的独立分支)
 * ================================================================ */

typedef enum {
    TOPO_CONTINUE,    // C: 在同一约束链上继续
    TOPO_BRANCH_ADD,  // B: 从当前节点分支出新约束
    TOPO_END_BRANCH,  // E: 当前约束分支结束
    TOPO_SPLIT,       // S: 拆分到新的独立子图
    TOPO_LEAF,        // L: 叶节点（无出边的终端节点）
} TopoSymbol;

// 约束图拓扑 → 符号序列
int edgebreaker_encode_constraint_graph(
    const ConstraintGraph *graph,
    TopoSymbol *out_symbols,
    int *out_node_order   // 输出：节点在序列中的顺序（用于后续坐标编码）
) {
    int symbol_count = 0;
    bool *visited = calloc(graph->node_count, sizeof(bool));
    int *stack = malloc(graph->node_count * sizeof(int));
    int stack_top = 0;

    // 选取起始节点：选择坐标绝对值最大的节点作为起点
    int start_node = select_start_node(graph, CRITERION_EXTREME_COORD);
    visited[start_node] = true;
    stack[stack_top++] = start_node;
    out_node_order[0] = start_node;

    int order_idx = 1;

    while (stack_top > 0) {
        int current = stack[stack_top - 1];
        int constraint_count = count_outgoing_constraints(graph, current);

        if (constraint_count == 0) {
            // 无邻居：叶节点
            out_symbols[symbol_count++] = TOPO_LEAF;
            stack_top--;
            continue;
        }

        // 找下一个未访问的邻居（按约束优先级排序）
        int next = find_next_unvisited_neighbor(graph, current, visited,
                                                  CONSTRAINT_PRIORITY_ORDER);
        if (next >= 0) {
            out_node_order[order_idx++] = next;
            visited[next] = true;

            if (stack_top == 1 || constraint_count == 1) {
                out_symbols[symbol_count++] = TOPO_CONTINUE;
            } else {
                out_symbols[symbol_count++] = TOPO_BRANCH_ADD;
            }

            stack[stack_top++] = next;
        } else {
            // 所有邻居已访问，回溯
            out_symbols[symbol_count++] = TOPO_END_BRANCH;
            stack_top--;
        }
    }

    free(visited);
    free(stack);
    return symbol_count;
}

/* ================================================================
 * 阶段 2: 坐标预测编码
 *
 * 对每个节点，根据其构造类型选择最优预测器，编码残差。
 * ================================================================ */

typedef struct {
    double x, y;
} Coord2D;

// 平行四边形预测器：D_pred = B + C - A
// 用于平行四边形构造点、对称点
Coord2D parallelogram_predict(
    const ConstraintGraph *graph,
    int node_D,
    int node_A, int node_B, int node_C  // 构造依赖节点
) {
    Coord2D ca = graph_get_coord(graph, node_A);
    Coord2D cb = graph_get_coord(graph, node_B);
    Coord2D cc = graph_get_coord(graph, node_C);

    Coord2D predicted;
    predicted.x = cb.x + cc.x - ca.x;
    predicted.y = cb.y + cc.y - ca.y;
    return predicted;
}

// 中点预测器：P_pred = (A + B) / 2
Coord2D midpoint_predict(
    const ConstraintGraph *graph,
    int node_P,
    int node_A, int node_B
) {
    Coord2D ca = graph_get_coord(graph, node_A);
    Coord2D cb = graph_get_coord(graph, node_B);

    Coord2D predicted;
    predicted.x = (ca.x + cb.x) / 2.0;
    predicted.y = (ca.y + cb.y) / 2.0;
    return predicted;
}

// 线性预测器：在 AB 延长线上，按比例 t 插值
Coord2D linear_predict(
    const ConstraintGraph *graph,
    int node_P,
    int node_A, int node_B,
    double t  // t=0 在 A, t=1 在 B, t=1.5 在 B 外延长线上
) {
    Coord2D ca = graph_get_coord(graph, node_A);
    Coord2D cb = graph_get_coord(graph, node_B);

    Coord2D predicted;
    predicted.x = ca.x + t * (cb.x - ca.x);
    predicted.y = ca.y + t * (cb.y - ca.y);
    return predicted;
}

// 自适应预测编码主函数
int predictive_encode_coords(
    const ConstraintGraph *graph,
    const int *node_order,     // Edgebreaker 阶段的节点顺序
    int node_count,
    const LvzCompressConfig *cfg,
    Coord2D *out_residuals     // 输出：量化后的残差
) {
    int coded_count = 0;

    for (int i = 0; i < node_count; i++) {
        int node_id = node_order[i];
        Coord2D actual = graph_get_coord(graph, node_id);

        // 根据节点构造类型选择预测器
        NodeConstructType ctype = graph_get_construct_type(graph, node_id);
        Coord2D predicted = { 0.0, 0.0 };

        switch (ctype) {
        case CONSTRUCT_MIDPOINT: {
            int dep_A = graph_get_dependency(graph, node_id, 0);
            int dep_B = graph_get_dependency(graph, node_id, 1);
            predicted = midpoint_predict(graph, node_id, dep_A, dep_B);
            break;
        }
        case CONSTRUCT_PARALLELOGRAM: {
            int dep[3];
            graph_get_all_dependencies(graph, node_id, dep, 3);
            predicted = parallelogram_predict(graph, node_id,
                                               dep[0], dep[1], dep[2]);
            break;
        }
        case CONSTRUCT_EXTENSION: {
            int dep_A = graph_get_dependency(graph, node_id, 0);
            int dep_B = graph_get_dependency(graph, node_id, 1);
            double ratio = graph_get_extension_ratio(graph, node_id);
            predicted = linear_predict(graph, node_id, dep_A, dep_B, ratio);
            break;
        }
        case CONSTRUCT_FREE:
        default:
            // 自由点/鼠标点击点：无预测，直接编码绝对值
            out_residuals[coded_count].x = actual.x;
            out_residuals[coded_count].y = actual.y;
            coded_count++;
            continue;
        }

        // 计算残差并量化
        double dx = actual.x - predicted.x;
        double dy = actual.y - predicted.y;

        // 自适应量化：将残差映射到 [-(2^(bits-1)), 2^(bits-1)-1]
        double max_coord = graph_get_bounding_span(graph);
        double scale = (double)((1 << (cfg->quantize_bits - 1)) - 1) / max_coord;

        out_residuals[coded_count].x = round(dx * scale);
        out_residuals[coded_count].y = round(dy * scale);
        coded_count++;
    }

    return coded_count;
}

/* ================================================================
 * 阶段 3: 属性分层编码
 * ================================================================ */

// 属性分层结构：将不同类型的数据放入独立数据流
typedef struct {
    uint8_t *node_types;          // 节点类型 (枚举值)
    int node_type_count;
    uint8_t *constraint_types;    // 约束类型 (枚举值)
    int constraint_count;
    char *label_strings;          // 标签文本 (以 \0 分隔)
    int label_total_bytes;
    uint32_t *style_packed;       // 样式打包 (颜色+线宽+可见性)
    int style_count;
} LvzAttributeLayers;

// 分层编码
void attribute_layered_encode(
    const ConstraintGraph *graph,
    LvzAttributeLayers *layers
) {
    // 第 1 层：节点类型（Huffman 编码适合枚举值）
    layers->node_type_count = graph->node_count;
    layers->node_types = malloc(graph->node_count * sizeof(uint8_t));
    for (int i = 0; i < graph->node_count; i++) {
        layers->node_types[i] = (uint8_t)graph_get_node_type(graph, i);
    }

    // 第 2 层：约束类型
    layers->constraint_count = graph->edge_count;
    layers->constraint_types = malloc(graph->edge_count * sizeof(uint8_t));
    for (int i = 0; i < graph->edge_count; i++) {
        layers->constraint_types[i] = (uint8_t)graph_get_edge_type(graph, i);
    }

    // 第 3 层：标签文本（LZ4 压缩）
    layers->label_total_bytes = collect_all_labels(graph,
                                                     &layers->label_strings);

    // 第 4 层：样式属性（位图打包）
    layers->style_count = graph->node_count;
    layers->style_packed = malloc(graph->node_count * sizeof(uint32_t));
    for (int i = 0; i < graph->node_count; i++) {
        uint8_t r, g, b, a;
        graph_get_node_color(graph, i, &r, &g, &b, &a);
        bool locked = graph_is_node_locked(graph, i);
        bool visible = graph_is_node_visible(graph, i);

        // 打包：8bit 颜色 × 4 + 1bit 锁定 + 1bit 可见 = 34 bit → 32 bit 有损
        layers->style_packed[i] =
            ((uint32_t)(r & 0xFC) << 24) |  // 取 6 bit 红色
            ((uint32_t)(g & 0xFC) << 16) |  // 取 6 bit 绿色
            ((uint32_t)(b & 0xFC) << 8)  |  // 取 6 bit 蓝色
            ((uint32_t)(a & 0xC0))       |  // 取 2 bit 透明度
            (locked ? 1 : 0) << 1 |
            (visible ? 1 : 0);
    }
}

/* ================================================================
 * 阶段 4: rANS 熵编码
 * ================================================================ */

// rANS 编码器状态（简化版）
typedef struct {
    uint32_t state;                  // rANS 状态寄存器
    uint8_t *output;
    int output_pos;
    int output_capacity;
    uint32_t freq_table[256];        // 符号频率表
    uint32_t cumulative[257];        // 累积频率
    uint32_t total_freq;             // 总频率
} RansEncoder;

// 初始化 rANS 编码器
void rans_encoder_init(RansEncoder *enc, uint8_t *buffer, int capacity) {
    enc->state = 0x80000000;  // 初始状态（避免下溢）
    enc->output = buffer;
    enc->output_pos = 0;
    enc->output_capacity = capacity;
    memset(enc->freq_table, 0, sizeof(enc->freq_table));
}

// 建立频率表（两轮编码：第一轮统计，第二轮编码）
void rans_build_freq_table(RansEncoder *enc,
                            const uint8_t *symbols, int count) {
    for (int i = 0; i < count; i++) {
        enc->freq_table[symbols[i]]++;
    }
    // 避免零频率（rANS 要求每个符号至少出现一次）
    for (int i = 0; i < 256; i++) {
        if (enc->freq_table[i] == 0)
            enc->freq_table[i] = 1;
    }
    // 计算累积分布
    enc->cumulative[0] = 0;
    for (int i = 0; i < 256; i++) {
        enc->cumulative[i + 1] = enc->cumulative[i] + enc->freq_table[i];
    }
    enc->total_freq = enc->cumulative[256];
}

// rANS 编码单个符号
void rans_encode_symbol(RansEncoder *enc, uint8_t symbol) {
    uint32_t freq = enc->freq_table[symbol];
    uint32_t cum = enc->cumulative[symbol];

    // rANS 核心公式：x' = floor(x / freq) * total + (x % freq) + cum
    if (enc->state >= freq * (enc->total_freq >> 16)) {
        enc->output[enc->output_pos++] = (uint8_t)(enc->state & 0xFF);
        enc->state >>= 8;
        if (enc->state >= freq * (enc->total_freq >> 16)) {
            enc->output[enc->output_pos++] = (uint8_t)(enc->state & 0xFF);
            enc->state >>= 8;
        }
    }
    enc->state = (enc->state / freq) * enc->total_freq
               + (enc->state % freq) + cum;
}

/* ================================================================
 * 主函数：完整的约束图压缩
 * ================================================================ */

typedef struct {
    uint8_t *data;
    size_t size;
} CompressedBuffer;

CompressedBuffer geometry_compress(
    const ConstraintGraph *graph,
    const LvzCompressConfig *cfg
) {
    // 第 1 阶段：拓扑编码
    int node_count = graph->node_count;
    TopoSymbol *topo_symbols = malloc(node_count * 2 * sizeof(TopoSymbol));
    int *node_order = malloc(node_count * sizeof(int));
    int topo_sym_count = edgebreaker_encode_constraint_graph(
        graph, topo_symbols, node_order);

    // 第 2 阶段：坐标预测编码
    Coord2D *residuals = malloc(node_count * sizeof(Coord2D));
    int res_count = predictive_encode_coords(
        graph, node_order, node_count, cfg, residuals);

    // 第 3 阶段：属性分层编码
    LvzAttributeLayers layers;
    attribute_layered_encode(graph, &layers);

    // 第 4 阶段：熵编码（将所有符号序列化并传入 rANS）
    // ... 将所有数据序列化为统一符号流，送入 rANS 编码器

    // 释放临时内存
    free(topo_symbols);
    free(node_order);
    free(residuals);
    free(layers.node_types);
    free(layers.constraint_types);
    free(layers.label_strings);
    free(layers.style_packed);

    // 返回压缩缓冲区
    CompressedBuffer result;
    // ... result 由熵编码器填充
    return result;
}
```

### 3.2 .lvzd 序列化格式设计

受 Draco 的 .drc 格式启发，Lv-00 定义 .lvzd（Lv-00 Zipped Draco-style）紧凑序列化格式：

```
.lvzd 文件结构
═════════════════════════════════════════════════════════════

┌──────────────────────────────────────────────────┐
│ HEADER (32 bytes)                                  │
│  - magic: "LVZD" (4 bytes)                       │
│  - version: uint16 (2 bytes)                     │
│  - flags: uint16 (有损/无损/快速预览等)            │
│  - node_count: uint32 (4 bytes)                  │
│  - edge_count: uint32 (4 bytes)                  │
│  - topo_sym_count: uint32 (4 bytes)              │
│  - attr_layer_count: uint16 (2 bytes)            │
│  - entropy_backend: uint8 (1 byte)               │
│  - reserved: 7 bytes                             │
├──────────────────────────────────────────────────┤
│ TOPOLOGY STREAM (熵编码后的 Edgebreaker 符号序列)    │
│  - [TOPO_CONTINUE|BRANCH|END|SPLIT|LEAF]*       │
├──────────────────────────────────────────────────┤
│ GEOMETRY STREAM (量化残差，每个节点 2×int16 或 2×int32)│
│  - node_order[0]: (dx, dy)                       │
│  - node_order[1]: (dx, dy)                       │
│  - ...                                           │
├──────────────────────────────────────────────────┤
│ ATTRIBUTE LAYERS                                 │
│  - Layer 0: NODE_TYPES (Huffman encoded)         │
│  - Layer 1: CONSTRAINT_TYPES (差分+Huffman)        │
│  - Layer 2: LABELS (LZ4 compressed)              │
│  - Layer 3: STYLES (packed uint32[])             │
├──────────────────────────────────────────────────┤
│ FREQUENCY TABLES (if rANS/arithmetic)            │
│  - 每个符号的频率表（解码器需要）                     │
├──────────────────────────────────────────────────┤
│ CHECKSUM (CRC32, 4 bytes)                        │
└──────────────────────────────────────────────────┘
```

对应的解码函数伪代码：

```c
ConstraintGraph* geometry_decompress(const CompressedBuffer *buf) {
    // 1. 解析 HEADER，获取各数据流偏移和长度
    LvzdHeader header = parse_lvzd_header(buf);

    // 2. 根据 entropy_backend 选择解码器
    // 3. 解码 TOPOLOGY STREAM → TopoSymbol[] + node_order[]
    // 4. 解码 GEOMETRY STREAM → 绝对坐标（反量化+预测加法）
    // 5. 解码 ATTRIBUTE LAYERS → 逐层恢复属性
    // 6. 重建 ConstraintGraph 并返回

    ConstraintGraph *graph = constraint_graph_create();
    // ... 逐步填充 graph
    return graph;
}
```

### 3.3 压缩率预期估算

| 场景 | 原始大小 | 预期压缩后 | 压缩比 |
|------|---------|-----------|--------|
| 简单构造（10 点、8 边，少量约束） | ~1 KB | ~120 B | ~8:1 |
| 中等构造（100 点、150 边，典型几何题） | ~12 KB | ~1.5 KB | ~8:1 |
| 复杂构造（500 点、800 边，完整几何证明图） | ~60 KB | ~5 KB | ~12:1 |
| 大量自由点+标签文本 | ~200 KB | ~40 KB | ~5:1 |
| 纯构造点（推导点为主，残差极小） | ~100 KB | ~3 KB | ~30:1 |

---

## 4. 实现路线图

### 4.1 分阶段实施计划

| 阶段 | 名称 | 目标 | 输入 | 输出 | 估计工期 | 依赖 |
|------|------|------|------|------|---------|------|
| **阶段 1** | Edgebreaker 拓扑编码器 | 实现约束图的 BFS 遍历编码，生成 CLERS 风格符号序列 | ConstraintGraph | TopoSymbol[] + node_order[] | 1-2 周 | constraint_graph.h |
| **阶段 2** | 预测坐标编码 | 实现平行四边形/中点/线性预测器，残差量化 | TopoSymbol[], node_order[], 原始坐标 | 量化残差流 | 1-2 周 | 阶段 1 |
| **阶段 3** | 属性分层编码 | 节点类型/约束类型/标签/样式的分层压缩 | ConstraintGraph 属性 | LvzAttributeLayers | 1 周 | — |
| **阶段 4** | rANS 熵编码后端 | 实现 rANS 编码器/解码器，整合所有数据流 | 所有中间数据流 | .lvzd 文件 | 1-2 周 | 阶段 1-3 |
| **阶段 5** | .lvzd 格式定义与 I/O | 完整文件格式规范，读写 API，版本兼容处理 | — | lvz_format.h, 兼容性测试 | 1 周 | 阶段 4 |
| **阶段 6** | 优化与基准测试 | 压缩率/编解码速度基准测试，瓶颈优化 | 全部实现 | 性能报告 | 1 周 | 阶段 1-5 |

### 4.2 阶段 1 详细任务：Edgebreaker 拓扑编码器

**目标文件**：`src/compress/topo_edgebreaker.h` 和 `src/compress/topo_edgebreaker.c`

**任务清单**：

1. 定义 TopoSymbol 枚举（C/B/E/S/L 五个符号）
2. 实现 `select_start_node()` — 选取起始节点启发式（支持极值坐标/最大度数/最高约束优先级三种策略）
3. 实现 `find_next_unvisited_neighbor()` — 按约束类型优先级排序的邻居选择
4. 实现 `edgebreaker_encode_constraint_graph()` — 主遍历编码函数
5. 实现 `edgebreaker_decode_to_graph()` — 解码器（从符号序列重建图拓扑骨架）
6. 单元测试：小图（三角形、正方形、五角星）编解码往返

**验证标准**：编解码往返后，图节点数、边数、约束类型三者均不丢失。

### 4.3 阶段 2 详细任务：预测坐标编码

**目标文件**：`src/compress/predict_coord.h` 和 `src/compress/predict_coord.c`

**任务清单**：

1. 实现 `parallelogram_predict()` — 平行四边形预测器
2. 实现 `midpoint_predict()` — 中点预测器
3. 实现 `linear_predict()` — 线性内插/外推预测器
4. 实现 `barycentric_predict()` — 重心坐标预测器（三角形内部点）
5. 实现 `adaptive_quantize()` — 自适应量化（根据 bounding span 调整量化精度）
6. 实现 `predictive_encode_coords()` — 预测编码主函数
7. 实现 `predictive_decode_coords()` — 对应解码器
8. 单元测试：每种预测器的往返误差不超过 1e-6（对于 16-bit 量化）

**验证标准**：对于所有构造推导点（中点、交点、垂足等），残差编码后反量化误差 < 1e-4。

### 4.4 阶段 3 详细任务：属性分层编码

**目标文件**：`src/compress/attr_layers.h` 和 `src/compress/attr_layers.c`

**任务清单**：

1. 定义 `LvzAttributeLayers` 结构体
2. 实现节点类型枚举的 Huffman 编码/解码
3. 实现约束类型的差分编码（相邻约束类型差值）+ Huffman
4. 实现标签文本的 LZ4 压缩/解压（集成已有 LZ4 库）
5. 实现样式属性的 32-bit 打包/解包
6. 实现属性层的按需提取（部分解码）

**验证标准**：属性编解码往返 100% 语义一致（标签文本不变，颜色值误差 ≤ 2（低 2 bit 有损））。

### 4.5 阶段 4 详细任务：rANS 熵编码

**目标文件**：`src/compress/rans.h` 和 `src/compress/rans.c`

**任务清单**：

1. 实现 `RansEncoder` 结构体和初始化
2. 实现频率表构建（`rans_build_freq_table`）
3. 实现单符号编码（`rans_encode_symbol`）和解码（`rans_decode_symbol`）
4. 实现流的写入和刷新（处理状态寄存器最终输出）
5. 实现 16-bit / 32-bit 变体以支持大数据量
6. 基准测试：与标准 zlib 压缩比和速度对比

**验证标准**：编解码往返 100% 无损，压缩比与理论熵值的差距 < 5%。

### 4.6 里程碑与交付物

| 里程碑 | 时间节点 | 交付物 | 验收标准 |
|--------|---------|--------|---------|
| M1: 基础拓扑压缩 | 第 2 周末 | topo_edgebreaker.h/c | 小图编解码往返通过 |
| M2: 坐标压缩可用 | 第 3 周末 | predict_coord.h/c | 所有构造点预测编码通过 |
| M3: 属性分层完整 | 第 4 周末 | attr_layers.h/c | 四层属性独立编解码通过 |
| M4: 熵编码集成 | 第 5 周末 | rans.h/c, .lvzd 初版 | 完整压缩管线打通 |
| M5: 生产就绪 | 第 7 周末 | lvz_format.h, 基准报告 | 压缩比 8:1+, 解码 < 10ms (中等构造) |

---

## 5. 附录

### A. Draco 关键资源

- **GitHub 仓库**：[https://github.com/google/draco](https://github.com/google/draco)
- **官方文档**：[https://google.github.io/draco/](https://google.github.io/draco/)
- **JavaScript 解码器**：[https://github.com/google/draco/tree/master/javascript](https://github.com/google/draco/tree/master/javascript)
- **规范草案**：[Draco Bitstream Specification](https://google.github.io/draco/spec/)

### B. Edgebreaker 算法参考

- Rossignac, J. "Edgebreaker: Connectivity compression for triangle meshes." *IEEE TVCG*, 1999.
- Isenburg, M. and Snoeyink, J. "Spirale Reversi: Reverse decoding of the Edgebreaker encoding." *CGTA*, 2001.

### C. rANS 实现参考

- Duda, J. "Asymmetric numeral systems: entropy coding combining speed of Huffman coding with compression rate of arithmetic coding." *arXiv:1311.2540*, 2013.
- Collet, Y. "Finite State Entropy" — 实用的 FSE/rANS 实现：[https://github.com/Cyan4973/FiniteStateEntropy](https://github.com/Cyan4973/FiniteStateEntropy)

### D. Lv-00 相关文件索引

| Lv-00 文件 | 角色 | 与 Draco 借鉴的关系 |
|-----------|------|-------------------|
| `src/core/constraint_graph.h` | 核心数据结构 | 压缩/解压的目标对象 |
| `src/io/lvz_format.h` | .lvz 格式定义 | 直接受 .drc 格式启发 |
| `src/compress/topo_edgebreaker.h` | 拓扑编码器 | Edgebreaker 算法的 Lv-00 迁移 |
| `src/compress/predict_coord.h` | 坐标预测编码器 | 平行四边形/中点/线性预测器 |
| `src/compress/attr_layers.h` | 属性分层编码 | 借鉴 Draco 属性分离思想 |
| `src/compress/rans.h` | 熵编码后端 | rANS 的 C 语言移植 |
| `src/bindings/draco_bridge.c` | Draco 桥接层 | （可选）直接调用 Draco 库进行实验 |

### E. 术语对照

| 英文术语 | 中文翻译 | 说明 |
|---------|---------|------|
| Edgebreaker | 边断裂算法 | 网格拓扑压缩算法 |
| Parallelogram Prediction | 平行四边形预测 | 利用相邻三角形预测顶点坐标 |
| Residual Encoding | 残差编码 | 编码预测值与实际值的差值 |
| rANS | 范围不对称数字系统 | 一种熵编码算法 |
| CLERS Symbols | CLERS 五符号系统 | C=Continue, L=Left, E=End, R=Right, S=Split |
| Adaptive Quantization | 自适应量化 | 根据数值范围动态调整量化步长 |
| Lossless/Lossy | 无损/有损压缩 | 是否允许信息损失 |
| Attribute Layering | 属性分层 | 将不同语义的属性放入独立数据流 |
| Stream | 数据流 | Draco 中每个独立的编码数据段 |


---

> **文档状态**: 初稿完成
> **下一步**: 基于此参考文档启动阶段 1（Edgebreaker 拓扑编码器）的详细设计
