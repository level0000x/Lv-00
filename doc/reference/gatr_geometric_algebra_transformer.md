# GATr (Geometric Algebra Transformer) 几何代数变换器借鉴设计

> **借鉴项目**：GATr -- Geometric Algebra Transformer（Qualcomm AI Research）
> **核心借鉴点**：射影几何代数 G(3,0,1) 的 16 维多向量统一表示、E(3) 等变线性映射基、几何积与外积的双线性实现、接口函数嵌入/提取几何量、门控非线性激活
> **分类**：P2 中优先级 / 几何代数表示与等变计算基础
> **日期**：2026-05-25

---

## 1. 项目概述

### 1.1 项目简介

GATr（Geometric Algebra Transformer）是 Qualcomm AI Research 开发的几何代数变换器框架，由 Johann Brehmer、Pim de Haan、Soenke Behrends 和 Taco Cohen 合作完成，发表于 NeurIPS 2023。其核心思想是利用**射影几何代数（Projective Geometric Algebra）G(3,0,1)** 将三维欧几里得空间中的各种几何对象（点、方向向量、平移、旋转、平面、射线等）统一编码为 **16 维多向量（multivector）**，并在此基础上构建 E(3) 等变的 Transformer 架构。

GATr 的关键创新在于：它不针对某种特定的几何对象设计专用网络，而是将所有几何对象映射到同一个 16 维代数空间中，使得同一个 Transformer 架构能够处理点云、方向场、旋转场等异构几何数据。这种统一表示消除了传统方法中为不同几何类型分别设计编码器的需求，同时通过 E(3) 等变性保证了物理对称性的严格满足。

### 1.2 技术栈

| 维度 | 内容 |
|:---|:---|
| 编程语言 | Python（PyTorch） |
| 核心依赖 | PyTorch >= 1.12、einops、hydra-core、mlflow |
| 数学基础 | 射影几何代数 G(3,0,1)，即 Clifford 代数 Cl(3,0,1) |
| 多向量维度 | 16 维（标量 1 + 向量 4 + 二向量 6 + 三向量 4 + 伪标量 1） |
| 等变群 | Pin(3,0,1)，覆盖 E(3) = SO(3) semidirect R^3 |
| 许可证 | BSD-3-Clause-Clear |
| 论文 | arXiv:2305.18415，NeurIPS 2023 |

### 1.3 社区活跃度

| 指标 | 数据 |
|:---|:---|
| GitHub Stars | 约 600+ |
| 分支数 | 1（main） |
| 总提交数 | 14 |
| 最近更新 | 2025-02-07（v1.4.3） |
| 贡献者 | Qualcomm AI Research 团队 |
| 论文引用 | NeurIPS 2023 正式收录，被后续等变几何学习工作广泛引用 |

GATr 属于研究导向型项目，代码质量高但社区互动相对有限。其价值主要体现在学术影响力和架构设计的参考意义上。BSD-3-Clause-Clear 许可证允许自由使用、修改和分发，Lv-00 项目（MIT 许可）可自由借鉴其设计思想和算法实现。

---

## 2. 核心借鉴点

### 2.1 GATr 特性 vs Lv-00 现状对照表

| 编号 | GATr 特性 | GATr 实现方式 | Lv-00 现状 | 借鉴价值 |
|:---|:---|:---|:---|:---|
| G1 | 16 维多向量统一表示 | `double[16]` 扁平数组，按 clifford 库排序 | `SymbolicCoord` 仅支持标量坐标，无多向量类型 | 高 |
| G2 | 射影几何代数 G(3,0,1) | 基于平面约定的 PGA，点=三向量，平面=向量 | `euclidean_geometry.h` 支持 Hilbert/Tarski 公理，无 PGA | 高 |
| G3 | E(3) 等变线性映射 | 9 个基矩阵张成等变空间，系数可学习 | `geometry_transform.h` 支持旋转/平移/反射，无等变抽象 | 中 |
| G4 | 几何积（双线性） | 预计算基张量 `(N,16,16,16)`，einsum 实现 | 无几何积运算 | 高 |
| G5 | 外积 | 同几何积，使用不同的预计算基 | 无外积运算 | 中 |
| G6 | 接口函数（embed/extract） | `embed_point`、`extract_point`、`embed_ray` 等 | `GeomEntity` 类型层次，无代数嵌入 | 高 |
| G7 | 门控非线性 | 按阶（grade）分离后独立激活，保持等变性 | 无等变非线性 | 中 |
| G8 | 几何 LayerNorm | 按阶归一化多向量分量 | 无几何感知归一化 | 低 |
| G9 | 齐次坐标 | 点嵌入时自动添加齐次维度分量 | 无齐次坐标支持 | 中 |

### 2.2 多向量分量排序约定

GATr 采用与 Python `clifford` 库一致的 16 维分量排序：

```
索引:  0    1    2    3    4    5     6     7     8     9    10    11    12    13    14    15
分量:  1    e0   e1   e2   e3   e01   e02   e03   e12   e13   e23   e012  e013  e023  e123  e0123
阶数:  0    1    1    1    1    2     2     2     2     2     2     3     3     3     3     4
       标量 ----向量----          ----------二向量----------          ----三向量----          伪标量
```

其中 `e0` 是射影维度（齐次坐标的额外维度），`e1, e2, e3` 对应三维欧几里得空间的三个方向。

### 2.3 几何对象的 PGA 嵌入约定

GATr 采用**平面约定（plane-based convention）**，源自 Roelfs 和 De Keninck 的 "Graded symmetry groups: Plane and simple"：

| 几何对象 | PGA 表示 | 所在阶数 | 说明 |
|:---|:---|:---|:---|
| 标量 | `s * e0`（索引 0） | 0 阶 | 纯标量值 |
| 平面 | `a*e0 + b*e1 + c*e2 + d*e3`（索引 1-4） | 1 阶 | 四分量向量 |
| 点 | `-x*e023 + y*e013 - z*e012 + e123`（索引 11-14） | 3 阶 | 齐次三向量 |
| 射线 | Pluecker 坐标嵌入（索引 5-10） | 2 阶 | 六分量二向量 |
| 旋转 | 转子（标量 + 二向量） | 0+2 阶 | 偶多向量 |
| 平移 | 平移器（标量 + 二向量） | 0+2 阶 | 偶多向量 |

---

## 3. Lv-00 映射方案

### 3.1 多向量基础类型定义

在 Lv-00 的 C 语言架构中，借鉴 GATr 的 16 维多向量表示，定义 `Lv00MultiVector` 类型：

```c
/**
 * @file ga_multivector.h
 * @brief 射影几何代数多向量类型 -- 借鉴 GATr 的 16 维 PGA 表示
 *
 * 多向量分量排序（与 GATr / clifford 库一致）：
 *   索引 0:     标量        (grade 0)
 *   索引 1-4:   向量 e0,e1,e2,e3  (grade 1)
 *   索引 5-10:  二向量 e01..e23   (grade 2)
 *   索引 11-14: 三向量 e012..e023 (grade 3)
 *   索引 15:    伪标量 e0123       (grade 4)
 */
#ifndef LV00_GA_MULTIVECTOR_H
#define LV00_GA_MULTIVECTOR_H

#include <stdbool.h>
#include <stdint.h>
#include "symbolic_coord.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GA_MV_DIM 16

/** 各阶的分量索引范围 */
#define GA_GRADE0_START  0
#define GA_GRADE1_START  1
#define GA_GRADE2_START  5
#define GA_GRADE3_START  11
#define GA_GRADE4_START  15

/**
 * @brief 多向量类型
 *
 * 16 维扁平数组，存储 G(3,0,1) 射影几何代数的完整多向量。
 * 支持数值模式和符号模式（通过 is_symbolic 标志切换）。
 */
typedef struct Lv00MultiVector {
    double components[GA_MV_DIM];            /**< 数值分量 */
    SymbolicCoord *symbolic_components[GA_MV_DIM]; /**< 符号分量（可为 NULL） */
    bool is_symbolic;                         /**< 是否使用符号分量 */
    TrustColor trust;                         /**< 信任颜色 */
} Lv00MultiVector;

Lv00MultiVector ga_mv_zero(void);
Lv00MultiVector ga_mv_scalar(double s);
Lv00MultiVector ga_mv_scalar_sym(const SymbolicCoord *s);
void ga_mv_free(Lv00MultiVector *mv);
Lv00MultiVector ga_mv_copy(const Lv00MultiVector *src);

#ifdef __cplusplus
}
#endif
#endif /* LV00_GA_MULTIVECTOR_H */
```

### 3.2 几何积实现

GATr 通过预计算的基张量实现高效的批量几何积计算。在 Lv-00 中，由于面向符号计算而非深度学习，采用直接公式计算：

```c
/**
 * @file ga_product.h
 * @brief 几何积与外积运算 -- 借鉴 GATr 的双线性等变映射设计
 *
 * GATr 将几何积实现为三阶张量与两个多向量的 einsum：
 *   output[i] = sum_{j,k} basis[i,j,k] * x[j] * y[k]
 *
 * Lv-00 采用 G(3,0,1) 的显式乘法表实现，面向单个符号多向量的精确计算。
 * 度量签名：e0^2=+1, e1^2=+1, e2^2=+1, e3^2=-1
 */
#ifndef GA_PRODUCT_H
#define GA_PRODUCT_H
#include "ga_multivector.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 几何积 -- 对应 GATr gatr.primitives.bilinear.geometric_product() */
Lv00MultiVector ga_geometric_product(const Lv00MultiVector *x,
                                     const Lv00MultiVector *y);

/** 外积 -- 对应 GATr gatr.primitives.bilinear.outer_product() */
Lv00MultiVector ga_outer_product(const Lv00MultiVector *x,
                                 const Lv00MultiVector *y);

/** 内积 */
Lv00MultiVector ga_inner_product(const Lv00MultiVector *x,
                                 const Lv00MultiVector *y);

/** 反转 -- 对应 GATr gatr.primitives.linear.reverse() */
Lv00MultiVector ga_reverse(const Lv00MultiVector *x);

/** 阶对合 -- 对应 GATr gatr.primitives.linear.grade_involute() */
Lv00MultiVector ga_grade_involute(const Lv00MultiVector *x);

/** 范数平方: <x, x~> = x * reverse(x) 的标量部分 */
double ga_norm_squared(const Lv00MultiVector *x);

#ifdef __cplusplus
}
#endif
#endif /* GA_PRODUCT_H */
```

### 3.3 接口函数：几何量的嵌入与提取

GATr 的 `gatr.interface` 子模块提供了几何量与多向量之间的双向转换函数，建立了"物理世界"与"代数世界"之间的桥梁。在 Lv-00 中，这一桥接机制对应于 `GeomEntity` 类型系统与 `SymbolicCoord` 坐标系统之间的转换：

```c
/**
 * @file ga_interface.h
 * @brief 几何量与多向量的接口函数 -- 借鉴 GATr gatr.interface 模块
 *
 * 对应关系：
 *   GATr embed_point()          -> ga_embed_point()
 *   GATr extract_point()        -> ga_extract_point()
 *   GATr embed_pluecker_ray()   -> ga_embed_ray()
 *   GATr extract_pluecker_ray() -> ga_extract_ray()
 */
#ifndef GA_INTERFACE_H
#define GA_INTERFACE_H
#include "ga_multivector.h"
#include "geometry_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 点 ---- */

/** 将 3D 点嵌入为 PGA 多向量（平面约定：点 -> 三向量）
 *  p = (x,y,z) -> -x*e023 + y*e013 - z*e012 + e123
 *  对应 GATr gatr.interface.point.embed_point() */
Lv00MultiVector ga_embed_point(double x, double y, double z);

/** 符号坐标版本的点嵌入 */
Lv00MultiVector ga_embed_point_sym(const SymbolicCoord *sx,
                                   const SymbolicCoord *sy,
                                   const SymbolicCoord *sz);

/** 从多向量提取 3D 点（除以齐次维度）
 *  对应 GATr gatr.interface.point.extract_point() */
bool ga_extract_point(const Lv00MultiVector *mv,
                      double *out_x, double *out_y, double *out_z,
                      double threshold);

/* ---- 向量 ---- */

/** 方向向量 -> 向量: v -> vx*e1 + vy*e2 + vz*e3 */
Lv00MultiVector ga_embed_vector(double vx, double vy, double vz);
bool ga_extract_vector(const Lv00MultiVector *mv,
                       double *out_vx, double *out_vy, double *out_vz);

/* ---- 平面 ---- */

/** 平面 -> 向量: n.p=d -> d*e0 + nx*e1 + ny*e2 + nz*e3 */
Lv00MultiVector ga_embed_plane(double nx, double ny, double nz, double d);
bool ga_extract_plane(const Lv00MultiVector *mv,
                      double *out_nx, double *out_ny, double *out_nz,
                      double *out_d);

/* ---- 射线 ---- */

/** 射线 -> 二向量（Pluecker 坐标）
 *  对应 GATr gatr.interface.ray.embed_pluecker_ray() */
Lv00MultiVector ga_embed_ray(double ox, double oy, double oz,
                             double dx, double dy, double dz);
bool ga_extract_ray(const Lv00MultiVector *mv,
                    double *out_ox, double *out_oy, double *out_oz,
                    double *out_dx, double *out_dy, double *out_dz);

/* ---- GeomEntity 桥接 ---- */

/** GeomEntity -> PGA 多向量（根据类型自动选择嵌入方式） */
Lv00MultiVector ga_from_geom_entity(const GeomEntity *entity);

/** PGA 多向量 -> GeomEntity */
bool ga_to_geom_entity(const Lv00MultiVector *mv, GeomEntity *out);

#ifdef __cplusplus
}
#endif
#endif /* GA_INTERFACE_H */
```

### 3.4 等变约束传播示例

GATr 的等变线性映射基于 9 个预计算基矩阵，任何 Pin(3,0,1) 等变的线性变换都可以表示为这 9 个基的线性组合。在 Lv-00 的约束求解场景中，这一机制可用于构建**等变约束传播器**：

```c
#include "ga_multivector.h"
#include "ga_interface.h"
#include "ga_product.h"

/**
 * @brief 通过外积计算两点确定的直线
 *
 * 在 PGA 中，两点 p1 ^ p2 的外积给出它们确定的直线。
 */
Lv00MultiVector ga_line_from_two_points(const Lv00MultiVector *p1,
                                        const Lv00MultiVector *p2)
{
    return ga_outer_product(p1, p2);
}

/**
 * @brief 判断三点是否共线（通过外积为零）
 *
 * 在 PGA 中，三点 p1, p2, p3 共线当且仅当 p1 ^ p2 ^ p3 = 0。
 * 优势：无需计算距离或面积，自然支持齐次坐标，可直接符号计算。
 */
bool ga_three_points_collinear(const Lv00MultiVector *p1,
                               const Lv00MultiVector *p2,
                               const Lv00MultiVector *p3)
{
    Lv00MultiVector line = ga_outer_product(p1, p2);
    Lv00MultiVector test = ga_outer_product(&line, p3);
    double norm_sq = ga_norm_squared(&test);
    return norm_sq < 1e-12;
}

/**
 * @brief 判断四点是否共面（通过外积为零）
 *
 * 在 PGA 中，四点共面当且仅当 p1 ^ p2 ^ p3 ^ p4 = 0（伪标量分量为零）。
 */
bool ga_four_points_coplanar(const Lv00MultiVector *p1,
                             const Lv00MultiVector *p2,
                             const Lv00MultiVector *p3,
                             const Lv00MultiVector *p4)
{
    Lv00MultiVector v1 = ga_outer_product(p1, p2);
    Lv00MultiVector v2 = ga_outer_product(&v1, p3);
    Lv00MultiVector test = ga_outer_product(&v2, p4);
    /* 检查伪标量分量（索引 15）是否为零 */
    return test.components[15] < 1e-12;
}
```

### 3.5 与 Lv-00 现有架构的集成点

```
Lv-00 七层架构中的集成位置：

第 1 层：基础类
  +-- ga_multivector.h    (新增) 多向量类型定义
  +-- ga_product.h        (新增) 几何积/外积/内积
  +-- ga_interface.h      (新增) 几何量嵌入/提取

第 2 层：建模数据
  +-- geometry_types.h    (扩展) GeomEntity 增加 mv 字段
  +-- constraint_graph.h  (扩展) 约束边支持多向量约束

第 3 层：算法引擎
  +-- solver.c            (扩展) 支持几何代数约束求解
  +-- rewrite.c           (扩展) 几何积的代数重写规则

第 4 层：证明引擎
  +-- proof.c             (扩展) PGA 共线性/共面性判据
  +-- type_system.c       (扩展) 多向量类型推断

第 5-6 层：可视化/数据交换
  +-- (无需修改)          多向量通过接口函数转换回 GeomEntity

第 7 层：应用框架
  +-- preset_*.h          (扩展) 新增 PGA 预设函数块
```

---

## 4. 实现路线图

### 4.1 分阶段实施计划

| 阶段 | 时间 | 任务 | 涉及文件 | 优先级 | 交付物 |
|:---|:---|:---|:---|:---|:---|
| 短期-1 | 第 1-2 周 | 定义 `Lv00MultiVector` 基础类型 | `ga_multivector.h/c` | P0 | 多向量类型可用 |
| 短期-2 | 第 2-3 周 | 实现几何积、外积、内积（数值模式） | `ga_product.h/c` | P0 | 核心代数运算可用 |
| 短期-3 | 第 3-4 周 | 实现 embed/extract 接口函数 | `ga_interface.h/c` | P0 | 几何量双向转换可用 |
| 短期-4 | 第 4-5 周 | 实现向量/平面/射线的嵌入与提取 | `ga_interface.h/c` | P1 | 基本几何量全覆盖 |
| 中期-1 | 第 6-8 周 | 扩展 `SymbolicCoord` 支持多向量符号分量 | `ga_multivector.h/c` | P1 | 符号几何代数运算 |
| 中期-2 | 第 8-10 周 | 实现几何积的符号计算模式 | `ga_product.c` | P1 | 精确几何推理 |
| 中期-3 | 第 10-12 周 | 集成到约束图：多向量约束边类型 | `constraint_graph.h/c` | P1 | PGA 约束求解 |
| 中期-4 | 第 12-14 周 | PGA 共线性/共面性判据集成到证明引擎 | `proof.c` | P2 | PGA 辅助证明 |
| 中期-5 | 第 14-16 周 | 几何积的代数重写规则（结合律、分配律） | `rewrite.c` | P2 | PGA 表达式化简 |
| 长期-1 | 第 17-20 周 | 等变约束传播器（借鉴 9 基矩阵思想） | 新增模块 | P2 | 等变约束求解 |
| 长期-2 | 第 20-24 周 | 预设函数块：PGA 几何定理 | `preset_basic_geometry.h` | P3 | PGA 定理库 |
| 长期-3 | 第 24-30 周 | 高维几何代数支持 | `high_dim.h` 扩展 | P3 | 高维 PGA |

### 4.2 关键里程碑

| 里程碑 | 时间节点 | 验收标准 |
|:---|:---|:---|
| M1: 多向量基础设施 | 第 5 周 | `Lv00MultiVector` 类型可用，几何积/外积/内积通过单元测试 |
| M2: 接口函数完备 | 第 5 周 | 点/向量/平面/射线的 embed/extract 全部实现并通过测试 |
| M3: 符号 PGA | 第 12 周 | 符号多向量运算可用，共线性判据在符号模式下正确 |
| M4: 约束集成 | 第 16 周 | 至少 3 个几何定理可通过 PGA 约束求解器证明 |
| M5: 等变传播 | 第 24 周 | 等变约束传播器在约束图中正确工作 |

---

## 5. 附录

### 5.1 GATr 关键 API 列表

#### 原语层（gatr.primitives）

| API | 签名 | 功能 |
|:---|:---|:---|
| `geometric_product` | `(x: Tensor[16], y: Tensor[16]) -> Tensor[16]` | 几何积 xy |
| `outer_product` | `(x: Tensor[16], y: Tensor[16]) -> Tensor[16]` | 外积 x ^ y |
| `equi_linear` | `(x: [..., in_ch, 16], coeffs: [out_ch, in_ch, 9]) -> [..., out_ch, 16]` | Pin 等变线性映射 |
| `reverse` | `(x: Tensor[16]) -> Tensor[16]` | 多向量反转 |
| `grade_involute` | `(x: Tensor[16]) -> Tensor[16]` | 阶对合 |
| `grade_project` | `(x: Tensor[16]) -> Tensor[5, 16]` | 按阶投影 |
| `gated_nonlinearity` | `(x: Tensor[16]) -> Tensor[16]` | 门控非线性 |
| `geometric_attention` | `(q, k, v: Tensor[16]) -> Tensor[16]` | 几何注意力机制 |

#### 接口层（gatr.interface）

| API | 签名 | 功能 |
|:---|:---|:---|
| `embed_point` | `(coords: [..., 3]) -> [..., 16]` | 3D 点 -> 多向量 |
| `extract_point` | `(mv: [..., 16]) -> [..., 3]` | 多向量 -> 3D 点 |
| `embed_vector` | `(vec: [..., 3]) -> [..., 16]` | 3D 向量 -> 多向量 |
| `extract_vector` | `(mv: [..., 16]) -> [..., 3]` | 多向量 -> 3D 向量 |
| `embed_plane` | `(normal: [..., 3], offset: [...]) -> [..., 16]` | 平面 -> 多向量 |
| `extract_plane` | `(mv: [..., 16]) -> ([..., 3], [...])` | 多向量 -> 平面 |
| `embed_pluecker_ray` | `(ray: [..., 6]) -> [..., 16]` | Pluecker 射线 -> 多向量 |
| `extract_pluecker_ray` | `(mv: [..., 16]) -> [..., 6]` | 多向量 -> Pluecker 射线 |
| `embed_rotation` | `(rot: [..., 3, 3]) -> [..., 16]` | 旋转矩阵 -> 转子 |
| `extract_rotation` | `(mv: [..., 16]) -> [..., 3, 3]` | 转子 -> 旋转矩阵 |
| `embed_translation` | `(vec: [..., 3]) -> [..., 16]` | 平移向量 -> 平移器 |
| `extract_translation` | `(mv: [..., 16]) -> [..., 3]` | 平移器 -> 平移向量 |
| `embed_scalar` | `(s: [...]) -> [..., 16]` | 标量 -> 多向量 |
| `extract_scalar` | `(mv: [..., 16]) -> [...]` | 多向量 -> 标量 |

#### 网络层（gatr.layers）

| API | 功能 |
|:---|:---|
| `GATrBlock` | GATr Transformer 块（注意力 + MLP + LayerNorm + 残差） |
| `EquivariantLinear` | 等变线性层（可学习系数） |
| `GeometricAttention` | 几何自注意力层 |
| `GeometricMLP` | 几何 MLP（等变线性 + 门控非线性） |
| `GeometricLayerNorm` | 按阶归一化的 LayerNorm |

### 5.2 G(3,0,1) 射影几何代数基础

#### 基向量与度量

G(3,0,1) 是四维 Clifford 代数，基向量的平方为：

```
e0^2 = +1    (射影维度)
e1^2 = +1    (x 方向)
e2^2 = +1    (y 方向)
e3^2 = -1    (z 方向，注意符号)
e_i * e_j = -e_j * e_i  (i != j，反交换)
```

#### 16 个基元素

```
Grade 0 (标量):     1
Grade 1 (向量):     e0, e1, e2, e3
Grade 2 (二向量):   e01, e02, e03, e12, e13, e23
Grade 3 (三向量):   e012, e013, e023, e123
Grade 4 (伪标量):   e0123
```

#### 几何积乘法表（部分关键项）

```
e1 * e1 = +1        e2 * e2 = +1        e3 * e3 = -1
e1 * e2 = e12       e2 * e1 = -e12
e1 * e3 = e13       e3 * e1 = -e13
e2 * e3 = e23       e3 * e2 = -e23
e0 * e1 = e01       e1 * e0 = -e01
e0 * e123 = e0123   e123 * e0 = -e0123
e12 * e3 = e123     e3 * e12 = -e123
```

### 5.3 参考文献

1. Brehmer J, de Haan P, Behrends S, Cohen T. Geometric Algebra Transformer. NeurIPS 2023. arXiv:2305.18415.
2. Roelfs M, De Keninck S. Graded symmetry groups: Plane and simple. arXiv:2107.03771, 2021.
3. Dorst L. A Guided Tour to the Plane-Based Geometric Algebra PGA. geometricalgebra.org, 2020.
4. Hildenbrand D. Foundations of Geometric Algebra Computing. Springer, 2013.
5. Lasenby J, Lasenby A, Doran C. A Unified Mathematical Language for Physics and Engineering in the 21st Century. Phil. Trans. Royal Society A, 2000.
6. Gunn C. Geometric Algebras for Euclidean Geometry. arXiv:1401.0403, 2014.
7. Lengyel E. Projective Geometric Algebra Done Right. GitHub, 2024.
