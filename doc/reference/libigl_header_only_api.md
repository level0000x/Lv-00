# Lv-00 参考设计：libigl "头文件即库"极简集成模式与几何算法分类组织

> **版本**: 1.0.0  
> **日期**: 2026-05-24  
> **参考**: [libigl](https://github.com/libigl/libigl) —— 仅头文件的 C++ 几何处理库，MPL-2.0 许可  
> **目标**: 借鉴 libigl 的"头文件即库"极简集成方式和按几何算法域分类组织的 API 设计，指导 Lv-00 几何操作模块（`preset_basic_geometry.h` 等）的 API 设计

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴要点](#2-核心借鉴要点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)

---

## 1. 项目概述

### 1.1 libigl 是什么

libigl 是由 Alec Jacobson、Daniele Panozzo 等人发起的 C++ 几何处理库，以"极简集成"为核心设计理念。libigl 的使命宣言是：**一个没有依赖地狱的几何处理库**——用户只需 `#include <igl/readOBJ.h>` 即可使用 OBJ 文件读取功能，无需 CMake 配置、链接器设置或预编译二进制。

libigl 的关键设计决策：

1. **头文件即库（Header-Only）**：所有功能以独立 `.h` 头文件提供。每个函数对应一个 `.h` 文件，函数声明和实现（模板）在同一个头文件中完成。用户只需将 `libigl/include/` 添加到编译器的包含路径即可。无预编译库、无运行时依赖。

2. **按几何算法域分类组织**：libigl 的函数按算法域（algorithmic domain）组织成子目录——`igl/copyleft/`(受限于 MPL 的组件)、`igl/opengl/`(渲染)、`igl/predicates/`(几何谓词)、`igl/embree/`(光线追踪) 等。每个子目录是独立可选的——用户只需包含所需域的头文件。

3. **Eigen 作为唯一强依赖**：libigl 的全部输入输出使用 Eigen 矩阵类型（`Eigen::MatrixXd`、`Eigen::MatrixXi`）。这避免了函数参数列表膨胀——"给我顶点矩阵和面矩阵"即可，无需自定义 Point3D、Vector3D 等类型包装。

4. **函数式 API 风格**：每个算法是一个接受几何输入并返回几何输出的自由函数。不要求用户继承基类或注册到框架中。例如 `igl::cotmatrix(V, F, L)` 计算余切拉普拉斯矩阵。

```
// libigl 示例：读取网格并计算高斯曲率
#include <igl/readOBJ.h>
#include <igl/gaussian_curvature.h>
#include <igl/massmatrix.h>

Eigen::MatrixXd V;   // 顶点 (N×3)
Eigen::MatrixXi F;   // 面 (M×3)
igl::readOBJ("bunny.obj", V, F);

Eigen::MatrixXd K;   // 每顶点高斯曲率
igl::gaussian_curvature(V, F, K);

Eigen::SparseMatrix<double> M;
igl::massmatrix(V, F, igl::MASSMATRIX_TYPE_DEFAULT, M);
```

### 1.2 为什么借鉴 libigl

Lv-00 现有 60+ 个预置函数块头文件（`preset_basic_geometry.h`、`preset_algebraic_geometry.h` 等），它们提供了大量几何操作函数但这些文件之间存在明显的组织问题：

- 头文件间存在交叉依赖——几何函数可能同时需要线性代数和多项式模块
- 用户不清楚"哪个头文件提供哪个函数"——缺少清晰的分类体系
- 预置函数块的注册机制（`func_block_registry.h`）需要一次性加载全部模块，缺乏按需加载能力

libigl 的"一个函数 = 一个头文件"模型虽过于极端，但其"按算法域分类 + 独立可选 + 无交叉依赖"的组织哲学对 Lv-00 的预置函数块 API 设计有直接参考价值。

---

## 2. 核心借鉴要点

### 2.1 "头文件即库"的集成模式

libigl 的极简集成来自三个设计原则的组合：

| 原则 | libigl 做法 | Lv-00 对应 |
|------|-----------|-----------|
| **单头文件自包含** | 每个 `.h` 包含该函数所需的一切（自己 `#include` 依赖） | 每个 `preset_*.h` 应自包含，不依赖外部 `#include` 顺序 |
| **无运行时依赖** | 纯模板实现，无 `.cpp` 编译单元，无 `.so`/`.dll` 链接 | 预置函数块应以 `static inline` 方式注册，无运行时注册 |
| **按需包含** | 用户 `#include <igl/readOBJ.h>` 只引入 OBJ 读取相关代码 | 用户 `#include <lv00/preset_basic_geometry.h>` 只引入基本几何函数块 |
| **模板 + 静态多态** | 通过 C++ 模板提供通用性，无虚函数和 RTTI | Lv-00 使用 C 语言，但可通过宏和函数表实现类似的多态 |

Lv-00 的 C 语言限制使得完全复制"头文件仅模板"模式不可行，但可以借鉴"自包含头文件 + 按需链接"的理念——将预置函数块的注册代码从集中式 `preset_blocks.h` 改造为每个 `preset_*.h` 自注册。

### 2.2 按几何算法域的分类组织

libigl 的目录结构本身就是其 API 文档——用户通过浏览目录树即可发现可用功能：

```
libigl/include/igl/
  ├── readOBJ.h          ← 文件 I/O
  ├── readOFF.h
  ├── writeOBJ.h
  ├── cotmatrix.h        ← 微分几何 / 拉普拉斯
  ├── massmatrix.h
  ├── gaussian_curvature.h
  ├── arap.h             ← 变形 / 参数化
  ├── biharmonic_coordinates.h
  ├── marching_cubes.h   ← 隐式曲面
  ├── boolean/           ← CSG 布尔运算
  │   └── mesh_boolean.h
  ├── copyleft/          ← 受 MPL 互惠条款限制的组件（可选）
  │   ├── cgal/
  │   └── tetgen/
  └── opengl/            ← 渲染（可选）
      └── ViewerCore.h
```

每个子目录对应一个独立可选的算法域。Lv-00 的预置函数块已经按领域有粗略分类（`preset_basic_geometry.h`、`preset_algebraic.h` 等），但分类不够系统化。借鉴 libigl：

| libigl 域 | Lv-00 对应域 | 现有文件 | 改进建议 |
|-----------|-------------|---------|---------|
| 文件 I/O | DSL 语法解析 / 导出 | `formula_parser.h`、`formula_renderer.h` | 集中为 `io/` 子目录 |
| 微分几何 | 几何构造与度量 | `preset_basic_geometry.h`、`preset_differential_geometry.h` | 统一几何操作接口 |
| 离散算子 | 符号坐标求解 | `solver.h`、`symbolic_coord.h` | 将求解器作为一等函数块暴露 |
| 变形/参数化 | 变换与映射 | `preset_transformations.h` | 与几何构造合并 |
| CSG 布尔 | 区域运算 | `preset_geometry_3d.h`（部分） | 增加 `preset_region_ops.h` |
| 渲染（可选） | Web GUI 层 | `web/` | 不属于 C 内核层 |
| 外部依赖（可选） | CAS 后端 | `cas_backend.h`（规划中） | 类似 libigl `copyleft/` 的可选层 |

### 2.3 函数式 API 风格

libigl 每个算法是一个接受矩阵、返回矩阵的自由函数——无状态、无副作用、无对象构造。这种风格对 Lv-00 预置函数块的 API 设计有启发：

```c
/*
 * libigl 风格：函数式几何 API
 *
 * 每个操作接受 ConstraintGraph + 输入参数，返回修改后的
 * ConstraintGraph（或直接在原图上操作）。无全局状态、
 * 无单例模式、无隐式副作用。
 */

/*
 * 类比：igl::cotmatrix(V, F, L)
 * ─────────────────────────────
 * Lv-00 等价：给定点集，计算约束图上的拉普拉斯矩阵（若有物理类比）
 */

/*
 * 类比：igl::readOBJ(filename, V, F)
 * ──────────────────────────────────
 * Lv-00 等价：从 DSL 文本加载几何构造
 * ConstraintGraph *dsl_load(const char *dsl_source, ConstraintGraph *out);
 */

/*
 * 类比：igl::boolean::mesh_boolean(VA, FA, VB, FB, type, VC, FC)
 * ──────────────────────────────────────────────────────────────
 * Lv-00 等价：两个区域（Region）的布尔运算
 * int region_intersect(ConstraintGraph *g, int region_a, int region_b,
 *                       ConstraintGraph *out_subgraph);
 */
```

### 2.4 许可策略的教训

libigl 的一个关键设计教训在于 MPL-2.0 许可证的"copyleft"条款——核心库是 MPL-2.0（宽松），但与 CGAL（GPL）等外部库集成的组件放在独立的 `copyleft/` 子目录中，让用户可以明确选择是否接受 GPL 的传染性。Lv-00 的外部后端集成（如 Singular GPL）应该借鉴此策略——将 GPL 后端集成代码放在独立文件中，编译时通过 CMake 选项显式启用。

---

## 3. Lv-00 映射方案

### 3.1 预置函数块的"自包含头文件"模式

借鉴 libigl，将预置函数块的头文件改造为自包含——每个头文件负责注册自身包含的所有函数块：

```c
/**
 * @brief 预置函数块自注册宏（libigl 风格）
 *
 * 每个 preset_*.h 在文件末尾调用此宏，将本模块的
 * 所有函数块注册到全局注册表中。
 *
 * 设计要点（借鉴 libigl）：
 * 1. 自包含：本头文件包含自己所需的所有 #include
 * 2. 无副作用冲突：多次包含同一头文件不会重复注册
 * 3. 按需加载：用户只 include 需要的头文件
 */
#ifndef PRESET_BASIC_GEOMETRY_REGISTERED
#define PRESET_BASIC_GEOMETRY_REGISTERED 1

static inline void preset_basic_geometry_register(FuncBlockRegistry *reg) {
    /* 每个函数块只注册一次 */
    if (reg->modules_registered & MODULE_BASIC_GEOMETRY) return;
    reg->modules_registered |= MODULE_BASIC_GEOMETRY;

    /* 中点构造 */
    func_block_registry_register(reg,
        func_block_create("midpoint",
            "已知两点构造中点",
            preset_midpoint_inputs, 2,
            preset_midpoint_outputs, 1,
            preset_midpoint_internal,
            PRESET_GEOMETRY_BASIC));

    /* 垂线构造 */
    func_block_registry_register(reg,
        func_block_create("perpendicular",
            "过已知点作已知线的垂线",
            preset_perpendicular_inputs, 2,
            preset_perpendicular_outputs, 1,
            preset_perpendicular_internal,
            PRESET_GEOMETRY_BASIC));

    /* ... 其他基本几何函数块 ... */
}

#endif /* PRESET_BASIC_GEOMETRY_REGISTERED */
```

使用方式借鉴 libigl 的极简风格：

```c
/* 类比 libigl 中的 #include <igl/readOBJ.h> */
#include <lv00/preset_basic_geometry.h>
#include <lv00/preset_transformations.h>

/* 初始化注册表 */
FuncBlockRegistry *reg = func_block_registry_create();

/* 按需注册 —— 只注册 include 的模块 */
preset_basic_geometry_register(reg);
preset_transformations_register(reg);

/* 使用函数块 */
FuncBlock *midpoint = func_block_registry_lookup(reg, "midpoint");
FuncBlock *rotate = func_block_registry_lookup(reg, "rotate");
```

### 3.2 几何算法域分类体系

借鉴 libigl 的目录组织，重新组织 Lv-00 预置函数块的分类：

```
include/lv00/preset/
  ├── core/                    ← 最小核心（始终加载）
  │   ├── preset_types.h       ← 基础类型定义
  │   ├── preset_point.h       ← 点构造
  │   ├── preset_line.h        ← 线段/直线构造
  │   └── preset_circle.h      ← 圆构造
  │
  ├── construction/            ← 几何构造域
  │   ├── preset_midpoint.h
  │   ├── preset_intersection.h
  │   ├── preset_perpendicular.h
  │   ├── preset_angle_bisector.h
  │   └── preset_centers.h     ← 重心/外心/内心/垂心
  │
  ├── transformation/          ← 几何变换域
  │   ├── preset_translate.h
  │   ├── preset_rotate.h
  │   ├── preset_reflect.h
  │   └── preset_scale.h
  │
  ├── measurement/             ← 度量与计算域
  │   ├── preset_distance.h
  │   ├── preset_angle.h
  │   ├── preset_area.h
  │   └── preset_curvature.h
  │
  ├── algebraic/               ← 代数操作域
  │   ├── preset_polynomial.h
  │   ├── preset_groebner.h
  │   ├── preset_factorization.h
  │   └── preset_linear_algebra.h
  │
  ├── logic/                   ← 逻辑与证明域
  │   ├── preset_propositional.h
  │   ├── preset_first_order.h
  │   └── preset_rewrite_rules.h
  │
  ├── analysis/                ← 分析学域（可选）
  │   ├── preset_calculus.h
  │   ├── preset_differential_eq.h
  │   └── preset_measure.h
  │
  └── backend/                 ← 外部后端集成（可选，类似 libigl copyleft/）
      ├── preset_singular.h    ← GPL 许可，需显式启用
      ├── preset_z3.h          ← MIT 许可
      └── preset_cvc5.h        ← BSD 许可
```

### 3.3 按需加载机制

借鉴 libigl 的"包含即使用"哲学，设计按需加载的注册表：

```c
/**
 * @brief 按需加载的函数块注册表（libigl 风格）
 *
 * 预设函数块不再在启动时一次性全部注册。而是：
 * 1. 程序启动时只注册核心域（core/）中的函数块
 * 2. 当 DSL 脚本中引用某域的函数块时，自动加载该域
 * 3. 每个域独立编译——未使用的域完全不参与编译
 */
typedef struct LazyFuncBlockRegistry {
    /* 已加载的模块位掩码 */
    uint64_t loaded_domains;

    /* 每个域的加载函数 */
    void (*loaders[PRESET_DOMAIN_COUNT])(FuncBlockRegistry *reg);

    /* 实际的函数块注册表（只包含已加载域的函数块） */
    FuncBlockRegistry *active_registry;
} LazyFuncBlockRegistry;

/**
 * @brief 查找函数块时自动触发域加载（libigl 风格）
 *
 * 在注册表中查找函数块名，如果未找到且对应域未加载，
 * 则自动加载该域后重试。
 *
 * @param lazy_reg  惰性注册表
 * @param name      函数块名称
 * @return 找到的 FuncBlock，或 NULL
 */
FuncBlock *lazy_registry_lookup(LazyFuncBlockRegistry *lazy_reg,
                                 const char *name);

/*
 * 内部逻辑：
 * 1. 先在 active_registry 中查找
 * 2. 如果未找到，根据名称推断所属域（如 "midpoint" → CONSTRUCTION）
 * 3. 调用 loaders[CONSTRUCTION] 加载该域到 active_registry
 * 4. 在更新后的 active_registry 中重新查找
 */
```

### 3.4 外部后端的许可隔离

借鉴 libigl 的 `copyleft/` 策略，将 GPL 许可的外部后端代码隔离：

```cmake
# CMakeLists.txt —— libigl 风格的许可隔离
option(LV00_ENABLE_SINGULAR "Enable Singular backend (GPL)" OFF)
option(LV00_ENABLE_M2        "Enable Macaulay2 backend (GPL)" OFF)
option(LV00_ENABLE_Z3        "Enable Z3 SMT solver (MIT)" ON)
option(LV00_ENABLE_CVC5      "Enable cvc5 SMT solver (BSD)" ON)

if(LV00_ENABLE_SINGULAR)
    add_subdirectory(src/backend/singular)  # GPL 组件隔离
    target_compile_definitions(lv00_core PRIVATE LV00_HAS_SINGULAR)
endif()
```

### 3.5 映射到现有预置函数块体系

| 现有结构 | libigl 借鉴后的改进 |
|---------|------------------|
| `preset_basic_geometry.h`（单一大文件） | 拆分为 `preset/construction/` 子目录，每个函数块独立注册 |
| `preset_blocks.h`（集中注册全部函数块） | `LazyFuncBlockRegistry` 按需加载，启动时只注册核心域 |
| `preset_advanced_geometry.h` 等混合组织 | 按"构造/变换/度量"三维分类，每个域独立可选 |
| 无许可隔离机制 | 外部后端放在 `preset/backend/`，GPL 组件编译期可选 |
| 无域层次结构 | 引入 `core → construction → measurement → backend` 依赖层次 |

---

## 4. 实现路线图

### 4.1 第一阶段：自包含头文件改造（P4）

| 任务 | 文件 | 说明 |
|------|------|------|
| 定义 `PRESET_REGISTER_MODULE` 宏 | `include/lv00/preset_common.h` | 统一注册模式 |
| 改造 `preset_basic_geometry.h` 为自包含 | 现有文件 | 头文件末尾添加 `_register()` 函数 |
| 改造 `preset_transformations.h` 为自包含 | 现有文件 | 同上 |
| 移除 `preset_blocks.h` 中的集中注册代码 | 现有文件 | 改为调用各模块的 `_register()` |

**预估规模**：约 200 行代码重构

### 4.2 第二阶段：分类目录重组（P4+）

| 任务 | 说明 |
|------|------|
| 创建 `include/lv00/preset/` 子目录结构 | `core/`、`construction/`、`transformation/`、`measurement/`、`algebraic/`、`logic/`、`backend/` |
| 将现有 60+ 头文件按域分配到子目录 | 拆分大文件，每个文件聚焦单一职责 |
| 实现 `LazyFuncBlockRegistry` | 惰性加载注册表 |
| 更新 DSL 编译器以使用惰性注册表 | 编译时按需加载引用的域 |

**预估规模**：约 400 行新代码 + 文件重组

### 4.3 第三阶段：外部后端许可隔离（远期）

| 任务 | 说明 |
|------|------|
| 创建 `src/backend/` 目录和 CMake 选项 | 类似 libigl `copyleft/` 的编译期隔离 |
| 将 Singular/Macaulay2 后端移入独立编译单元 | GPL 组件不在默认编译中 |
| 实现后端探针机制 | 运行时检测后端可用性，优雅降级 |

---

## 附录 A：libigl 与 Lv-00 预置函数块对照

| libigl 概念 | Lv-00 对应概念 | 关键差异 |
|-----------|--------------|---------|
| 独立头文件 (`readOBJ.h`) | 预置函数块模块 (`preset_basic_geometry.h`) | Lv-00 是 C 语言，需显式注册表 |
| 按域子目录 (`copyleft/`, `opengl/`) | `preset/` 子目录分类 | Lv-00 当前为扁平组织 |
| Eigen 矩阵类型 | `SymbolicCoord` + `mpz_poly` | Lv-00 精度更高（有理数）但灵活性更低 |
| 函数式组合 | 函数块组合子 (`compose`, `product`) | Lv-00 是显式的图组合 |
| 模板静态多态 | 函数表 + 宏 | C 语言的限制 |
| 许可证的 `copyleft/` 隔离 | `backend/` 目录 + CMake 选项 | 相同的隔离哲学 |
| 无构建系统依赖 | CMake 必需（C 语言编译） | C 需要编译步骤，无法完全消除 |

---

## 附录 B：按需加载策略选择

| 策略 | 启动延迟 | 内存占用 | 复杂度 | 适用场景 |
|------|---------|---------|--------|---------|
| **全量预加载**（当前） | 高（注册 60+ 函数块） | 高（所有函数块在内存） | 低 | 批处理模式 |
| **启动加载核心 + 首次使用加载域** | 低（仅注册 core/） | 按需增长 | 中 | 交互式使用 |
| **纯惰性加载**（首次 DSL 引用时加载） | 最低 | 最低 | 高 | Web 前端 |
| **预编译模块**（类似 FORM） | 极低 | 中等 | 高 | 生产部署 |

推荐策略：**启动加载核心 + 首次使用加载域**——与此前 FORM 文档中的"编译期预编译模块"方向互补。

---

> **文档结束**  
> 本文档详述了 libigl 的"头文件即库"极简集成方式如何指导 Lv-00 预置函数块 API 的重组。核心结论：(1) 将扁平组织改造为 `preset/core/`、`construction/`、`transformation/`、`measurement/`、`algebraic/`、`logic/`、`backend/` 七域分类体系；(2) 引入 `LazyFuncBlockRegistry` 惰性注册表替代全量预加载，实现类似 libigl 的"包含即使用"按需加载；(3) 将 GPL 许可的外部后端代码隔离到 `backend/` 目录，编译期通过 CMake 选项显式启用。
