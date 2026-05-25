# Lv-00 参考设计：Eigen 纯头文件线性代数库的表达式模板与零依赖架构

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: [Eigen](https://gitlab.com/libeigen/eigen) —— C++ 模板线性代数库，MPL 2.0 许可，header-only，零依赖
> **目标**: 借鉴 Eigen 的纯头文件分发模式、表达式模板惰性求值、Geometry 模块、固定大小矩阵栈优化、矩阵分解和 SIMD 向量化六大设计，指导 Lv-00 的轻量头文件分发、符号计算延迟求值、几何变换实现和数值计算加速

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴要点与对照表](#2-核心借鉴要点与对照表)
3. [Lv-00 映射方案与代码示例](#3-lv-00-映射方案与代码示例)
4. [实现路线图](#4-实现路线图)
5. [附录](#5-附录)

---

## 1. 项目概述

### 1.1 Eigen 是什么

Eigen 是由 Gael Guennebaud、Benoit Jacob 等人自 2008 年起开发的 C++ 模板线性代数库，目前在 GitLab 上维护（v5.0 要求 C++14）。Eigen 是科学计算和工程领域使用最广泛的 C++ 线性代数库之一，被 TensorFlow、Ceres Solver、Drake 机器人框架、ROS（Robot Operating System）、CGAL 计算几何算法库、OpenCV 等数千项目作为底层数学依赖。

Eigen 的关键设计决策：

1. **纯头文件、零依赖**：Eigen 的全部代码以 `.h` 头文件形式提供。用户只需将 Eigen 目录复制到项目中并 `#include <Eigen/Core>`，无需编译任何 `.cpp` 文件，无需链接任何 `.a`/`.so`/`.dll`，无需安装 BLAS/LAPACK 等外部库。这一设计使其成为嵌入式系统、交叉编译和学术研究项目的理想选择——零构建系统负担。

2. **表达式模板惰性求值**：Eigen 通过 C++ 表达式模板（Expression Templates）技术实现惰性求值。表达式 `a + b * c` 不立即计算，而是生成一个编译期表达式树 `CwiseBinaryOp<sum, Matrix, CwiseBinaryOp<prod, Matrix, Matrix>>`，在赋值时才求值并展开为内联循环。这消除了临时矩阵的创建，使复杂表达式在一个循环中完成。

3. **Geometry 模块**：Eigen 提供 `Eigen/Geometry` 模块，包含完整的 2D/3D 几何变换基础设施：`Transform`（仿射变换）、`Quaternion`（四元数旋转）、`AngleAxis`（轴角旋转）、`Translation`（平移）、`Scaling`（缩放）、`AlignedBox`（轴对齐包围盒）、`Hyperplane`（超平面）、`ParametrizedLine`（参数化直线）。这比裸矩阵运算的抽象层次更高，直接对应几何语义。

4. **固定大小矩阵栈分配优化**：Eigen 对编译期已知尺寸的矩阵（如 `Matrix4d`、`Vector3f`）使用栈分配（无 `new`/`malloc` 调用），避免了堆分配的开销和内存碎片。这一优化对小几何体（4x4 变换矩阵、3D 向量）的频繁使用场景至关重要。

5. **多种矩阵分解**：Eigen 提供 LU、QR、SVD、Cholesky（LLT/LDLT）、特征值分解（EigenSolver、SelfAdjointEigenSolver）、Schur 分解等。所有分解均有稠密和稀疏版本，且针对不同矩阵大小自动选择最优算法（如对小矩阵使用封闭形式解）。

6. **SIMD 向量化**：Eigen 通过 C++ 模板在编译期检测并启用 SSE2/SSE3/SSSE4/AVX/AVX2/AVX512/NEON/AltiVec 等 SIMD 指令集。用户无需写任何汇编或 intrinsic 代码——表达式模板自动将循环展开为向量化操作。

```
// Eigen 典型用法：线性方程组求解
#include <Eigen/Dense>

Eigen::MatrixXd A(3, 3);
A << 1, 2, 3,
     4, 5, 6,
     7, 8, 10;          // 注意: 不是奇异矩阵

Eigen::VectorXd b(3);
b << 3, 3, 4;

// 方式1：显式分解
Eigen::VectorXd x = A.colPivHouseholderQr().solve(b);

// 方式2：直接求解（内部选择分解算法）
Eigen::VectorXd x2 = A.lu().solve(b);

// 方式3：最小二乘（超定/欠定系统）
Eigen::VectorXd x3 = A.bdcSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(b);

// 几何变换示例
Eigen::Affine3d transform = Eigen::Translation3d(1.0, 2.0, 3.0)
                          * Eigen::AngleAxisd(M_PI / 4, Eigen::Vector3d::UnitZ())
                          * Eigen::Scaling(2.0);
Eigen::Vector3d point(1, 0, 0);
Eigen::Vector3d result = transform * point;  // 应用变换
```

### 1.2 为什么借鉴 Eigen

Lv-00 的核心计算涉及大量线性代数运算：

- **几何约束求解**：构造几何图形时需要求解线性方程组（如交点计算、中点等分）。
- **符号坐标归一化**：将有理多项式系数的符号坐标转化为标准型，涉及矩阵分解（LU、特征值）。
- **几何变换**：平移、旋转、缩放、反射等变换需要矩阵乘法与合成。
- **数值路径**：当符号路径（Groebner 基）不可行时，回退到数值求解，需要高效的矩阵运算。
- **性能关键路径**：大规模约束图可能涉及数百个未知数的方程组，矩阵运算的性能直接影响求解延迟。

Eigen 的六个核心设计直接对应 Lv-00 的六个痛点，详见下文。

---

## 2. 核心借鉴要点与对照表

### 2.1 纯头文件零依赖架构

Eigen 的"纯头文件"架构是其最成功的工程决策。它消除了所有构建系统负担——不需要 CMake find_package、不需要预编译库、不需要管理 ABI 兼容性。这一设计对 Lv-00 的 C 库分发模式有直接启示。

Lv-00 当前的项目结构包含大量 `.h` 头文件和对应的实现代码（通过 CMake 编译为 `liblv00.a`/`liblv00.so` 或 WASM）。虽然 C 语言无法完全实现"纯头文件"（因为 C 没有模板，`static inline` 函数在跨翻译单元时有局限性），但可以借鉴 Eigen 的**单头文件聚合**策略。

| Eigen 做法 | Lv-00 对应策略 |
|-----------|-------------|
| 所有代码在 `.h` 中，无 `.cpp` | 提供 `lv00.h` 聚合头文件（类似 Eigen/Core） |
| 用户只需 `#include <Eigen/Core>` | 用户只需 `#include <lv00/lv00.h>` |
| 模块按需包含（`Eigen/Dense`、`Eigen/Sparse`） | `lv00_geometry.h`、`lv00_solver.h` 等子模块可选 |
| 零外部依赖 | 保持当前零依赖原则（仅标准 C 库） |
| 源码即分发 | 提供 `lv00-amalgamated.h` + `lv00-amalgamated.c` 单文件版本 |

### 2.2 表达式模板惰性求值

Eigen 的表达式模板是其性能优势的核心来源。它以一种对用户透明的方式实现了惰性求值——用户写的公式自然地组合在一起，编译器在赋值点将整个表达式展开为高效的内联循环，消除了中间临时对象。

Lv-00 的符号计算引擎面临类似的问题：表达式 `(a + b) * (c - d)` 如果每一步都展开为完整的符号多项式，会产生大量中间临时对象。借鉴 Eigen 的惰性求值思想，Lv-00 可以设计"符号表达式树"——在最终需要数值化或规范化简时才执行展开。

| Eigen 概念 | Lv-00 对应 |
|-----------|-----------|
| `CwiseBinaryOp<Op, L, R>` 表达式节点 | `SymExpr` 抽象语法树节点 |
| `evalTo(Dest)` 最终求值 | `sym_eval(expr, result)` 惰性触发求值 |
| 编译期内联展开 | 运行时惰性展开（C 语言限制） |
| `NoAlias` 标记避免临时对象 | `in_place` 标志指示原地操作 |
| `forceAlignedAccess()` | 无对应（C 无对齐概念，但可保证缓存友好） |

C 语言的限制意味着 Lv-00 无法使用编译期表达式模板。但运行时惰性求值——将表达式暂存为树结构，在需要时一次性求值——是完全可行的。这也正是 Lv-00 的 `symbolic_coord.h` 模块的本质工作。

### 2.3 Geometry 模块

Eigen 的 `Geometry` 模块是 Lv-00 几何变换实现的直接参考对象。Eigen 将几何变换语义化——用户可以说"绕 Z 轴旋转 45 度"（`AngleAxisd(PI/4, Vector3d::UnitZ())`）而不是手动构造旋转矩阵。这减少了错误可能性并提高了代码可读性。

对于 Lv-00，几何变换是其核心功能——DSL 中的 `rotate`、`translate`、`reflect`、`scale` 等命令需要转换为矩阵运算。借鉴 Eigen 的 Geometry 模块，设计类型安全的变换 API：

| Eigen Geometry | Lv-00 C 端对应 |
|---------------|--------------|
| `Transform<T, 3, Affine>` | `mat4` + `transform_type` 标记（仿射/投影/等距） |
| `Quaternion<T>` | `quat` 结构体 |
| `AngleAxis<T>` | `axis_angle_to_quat(axis, angle)` / `quat_to_axis_angle()` |
| `Translation<T, 3>` | `mat4_translate(tx, ty, tz)` |
| `Scaling<T, 3>` | `mat4_scale(sx, sy, sz)` |
| `Transform::operator*`（变换组合） | `mat4_mul(out, a, b)` |
| `Transform::operator*`（变换点） | `mat4_transform_point(m, in, out)` |
| `AlignedBox<T, 3>` | `aabb` 包围盒 |
| `Hyperplane<T, 3>` | `plane` 平面（法向量+距原点距离） |

### 2.4 固定大小矩阵栈分配优化

Eigen 对 4x4 以下矩阵使用栈分配，避免了堆分配的 malloc/free 开销。在 Lv-00 中，4x4 变换矩阵和 3D 向量是最频繁创建和销毁的对象——每次几何构造、每次约束求解迭代都可能创建数百个临时矩阵。

Lv-00 的 C 语言实现虽然不能像 C++ 模板那样自动选择栈/堆分配，但可以显式提供"小矩阵"类型：

```c
/* 借鉴 Eigen 的固定大小矩阵优化 */

/* 4x4 矩阵 —— 始终栈分配（借鉴 Matrix4d） */
typedef double mat4[16];           /* 列主序，160 字节栈分配 */
typedef float  mat4f[16];          /* 单精度版本，64 字节 */

/* 3x3 矩阵 —— 栈分配 */
typedef double mat3[9];            /* 72 字节栈分配 */

/* 2x2 矩阵 —— 栈分配 */
typedef double mat2[4];            /* 32 字节栈分配 */

/* 动态大小矩阵 —— 堆分配（借鉴 MatrixXd） */
typedef struct {
    int rows, cols;
    int capacity;
    bool owns_data;
    double *data;                  /* 堆分配 */
} dynmat;

/* 在函数内部，总是优先使用固定大小类型 */
void do_something() {
    mat4 transform;                /* 栈上 160 字节 —— 零开销 */
    mat4_translate(transform, 1.0, 2.0, 3.0);

    mat4 rotation;
    mat4_rotate_z(rotation, M_PI / 4);

    mat4 composed;
    mat4_mul(composed, transform, rotation);
    /* transform, rotation, composed 全部在栈上，函数返回时自动释放 */
}
```

### 2.5 多种矩阵分解

Eigen 的矩阵分解体系（LU/QR/SVD/Cholesky/Eigen/Schur）覆盖了线性代数求解的绝大多数场景。每种分解有其适用条件：

| 分解方法 | 适用条件 | 复杂度 | 用途 |
|---------|---------|--------|------|
| LU 分解（PA=LU） | 方阵，非奇异 | O(n^3) | 通用线性方程组求解 |
| QR 分解（A=QR） | 任意矩阵 | O(mn^2) | 最小二乘、秩计算 |
| SVD（A=USV^T） | 任意矩阵 | O(mn^2) | 伪逆、条件数、主成分 |
| Cholesky（A=LL^T） | 对称正定 | O(n^3/3) | 最快分解（但仅对称正定） |
| 特征值分解（A=VDV^-1） | 方阵 | O(n^3) | 对角化、二次型标准化 |
| IDR(S) | 稀疏大型 | O(nz * iter) | 迭代法用于大型稀疏系统 |

Lv-00 的主要分解需求：

- **几何约束求解**：约束方程常形成稀疏线性系统（每个约束只涉及少数几个变量）→ 适合稀疏 LU 或迭代法
- **二次型标准化**：多项式标准化需要特征值分解
- **最小二乘拟合**：曲线/曲面拟合需要 SVD 或 QR

对于 C 语言的 Lv-00，实现完整的稠密线性代数库不可行（工作量大）。推荐策略：**选择性实现最关键的分解**（稠密 LU + 稀疏迭代求解），其余通过可选的外部后端（LAPACK via FFI、Eigen via WASM）提供。

### 2.6 SIMD 向量化

Eigen 通过编译期模板特化自动检测和启用 SIMD 指令集。对于 `Vector4f * Vector4f` 这样的操作，Eigen 生成的代码在 x86-64 上使用 SSE 指令（`mulps`、`addps`）一次处理 4 个 float。

Lv-00 的 C 代码可以在关键路径上手动添加 SIMD 优化：

```c
/* 借鉴 Eigen 的 SIMD 策略，手动实现关键路径 */

#if defined(__SSE2__) || defined(__x86_64__) || defined(_M_X64)
  #define LV00_USE_SSE2 1
#endif

#ifdef LV00_USE_SSE2
  #include <emmintrin.h>  /* SSE2 intrinsics */

  /* 4x4 矩阵乘法 —— SSE2 加速（借鉴 Eigen internal::gemm_pack_rhs） */
  static inline void mat4_mul_sse2(const double a[16],
                                    const double b[16],
                                    double out[16]) {
      __m128d a0 = _mm_load_pd(a + 0);
      __m128d a1 = _mm_load_pd(a + 2);
      __m128d a2 = _mm_load_pd(a + 4);
      __m128d a3 = _mm_load_pd(a + 6);
      __m128d a4 = _mm_load_pd(a + 8);
      __m128d a5 = _mm_load_pd(a + 10);
      __m128d a6 = _mm_load_pd(a + 12);
      __m128d a7 = _mm_load_pd(a + 14);

      for (int col = 0; col < 4; col++) {
          __m128d b0 = _mm_set1_pd(b[col * 4 + 0]);
          __m128d b1 = _mm_set1_pd(b[col * 4 + 1]);
          __m128d b2 = _mm_set1_pd(b[col * 4 + 2]);
          __m128d b3 = _mm_set1_pd(b[col * 4 + 3]);

          __m128d r0 = _mm_add_pd(
              _mm_add_pd(_mm_mul_pd(a0, b0), _mm_mul_pd(a1, b1)),
              _mm_add_pd(_mm_mul_pd(a2, b2), _mm_mul_pd(a3, b3)));
          __m128d r1 = _mm_add_pd(
              _mm_add_pd(_mm_mul_pd(a4, b0), _mm_mul_pd(a5, b1)),
              _mm_add_pd(_mm_mul_pd(a6, b2), _mm_mul_pd(a7, b3)));

          _mm_store_pd(out + col * 4 + 0, r0);
          _mm_store_pd(out + col * 4 + 2, r1);
      }
  }

  #define mat4_mul mat4_mul_sse2
#else
  /* 回退到标量实现 */
  #define mat4_mul mat4_mul_scalar
#endif
```

### 2.7 核心借鉴要点总对照表

| 序号 | 借鉴点 | Eigen 来源 | Lv-00 目标模块 | 借鉴深度 | 优先级 |
|------|--------|-----------|---------------|---------|--------|
| 1 | 纯头文件零依赖 | header-only 架构 | 轻量头文件分发模式 | 架构级 | P2 |
| 2 | 表达式模板惰性求值 | `CwiseBinaryOp` 表达式树 | 符号计算延迟求值 | 架构级 | P3 |
| 3 | Geometry 模块 | `Transform`/`Quaternion`/`AngleAxis` | 几何变换实现 | 实现级 | P2 |
| 4 | 固定大小栈分配 | `Matrix4d` 栈优化 | 小几何体性能优化 | 实现级 | P2 |
| 5 | 矩阵分解体系 | LU/QR/SVD/Cholesky/Eigen | 数值路径矩阵求解 | 架构级 | P4 |
| 6 | SIMD 向量化 | SSE/AVX/NEON 自动检测 | 数值计算加速 | 实现级 | P5 |

---

## 3. Lv-00 映射方案与代码示例

### 3.1 单头文件聚合分发模式

借鉴 Eigen 的 `Eigen/Core` 聚合头，提供 `lv00.h` 作为统一入口。同时提供 Amalgamated 单文件版本，满足嵌入式场景和快速集成的需求。

```c
/**
 * @file include/lv00/lv00.h
 * @brief Lv-00 聚合头文件 —— 借鉴 Eigen/Core 的"一个 include 包含一切"模式
 *
 * 设计要点（借鉴 Eigen）：
 * 1. 单文件入口：用户只需 #include <lv00/lv00.h>
 * 2. 模块可选：通过 #define 控制子模块的包含
 * 3. 无外部依赖：仅依赖标准 C 库，不依赖 BLAS/LAPACK
 * 4. 分发友好：可生成 Amalgamated 单文件版本
 */

#ifndef LV00_H
#define LV00_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── 版本信息 ── */
#define LV00_VERSION_MAJOR 3
#define LV00_VERSION_MINOR 2
#define LV00_VERSION_PATCH 0
#define LV00_VERSION_STRING "3.2.0"

/* ── 基础类型和工具（始终包含，类似 Eigen/Core 中的 Matrix/Array） ── */
#include "lv00/lv00_utils.h"       /* 工具宏、内存管理 */
#include "lv00/error_codes.h"      /* 错误码枚举 */
#include "lv00/geometry_types.h"   /* 几何类型定义 */

/* ── 核心引擎（类似 Eigen/Dense） ── */
#include "lv00/engine.h"           /* 引擎生命周期 */
#include "lv00/constraint_graph.h" /* 约束图 */
#include "lv00/symbolic_coord.h"   /* 符号坐标 */
#include "lv00/normalization.h"    /* 归一化 */
#include "lv00/type_system.h"      /* 类型系统 */

/* ── 求解器（类似 Eigen/LU、Eigen/QR） ── */
#include "lv00/solver.h"           /* 求解器前端 */
#include "lv00/rewrite.h"          /* 重写引擎 */
#include "lv00/unify.h"            /* 统一化 */

/* ── 函数块系统（Lv-00 特有） ── */
#include "lv00/func_block.h"       /* 函数块核心 */
#include "lv00/func_block_registry.h" /* 函数块注册表 */
#include "lv00/func_block_preset.h"   /* 预置函数块集合 */

/* ── 可选模块（类似 Eigen/Sparse、Eigen/Geometry） ── */
#ifdef LV00_ENABLE_ATP
#include "lv00/atp_backend.h"      /* ATP 后端（可选） */
#endif

#ifdef LV00_ENABLE_SMT
#include "lv00/smt_backend.h"      /* SMT 后端（可选） */
#endif

#ifdef LV00_ENABLE_PROOF
#include "lv00/proof.h"            /* 证明引擎（可选） */
#endif

#ifdef LV00_ENABLE_RECURSION
#include "lv00/recursion.h"        /* 递归（可选） */
#endif

#ifdef LV00_ENABLE_HIGH_DIM
#include "lv00/high_dim.h"         /* 高维几何（可选） */
#endif

/* ── 流式输出 ── */
#include "lv00/stream.h"           /* 流式输出引擎 */

#ifdef __cplusplus
}
#endif

#endif /* LV00_H */
```

### 3.2 符号表达式惰性求值

借鉴 Eigen 的表达式模板思想，设计 Lv-00 的符号表达式惰性求值系统：

```c
/**
 * @file include/lv00/sym_expr.h
 * @brief 符号表达式惰性求值 —— 借鉴 Eigen 表达式模板
 *
 * Eigen 在编译期构建表达式树并在赋值时一次性求值。
 * Lv-00 在运行时构建符号表达式树，在需要数值化或
 * 规范化简时触发懒惰求值。
 */

#ifndef SYM_EXPR_H
#define SYM_EXPR_H

#include <stdbool.h>

/* ── 表达式节点类型 ── */
typedef enum {
    SYM_EXPR_VAR,        /* 变量 */
    SYM_EXPR_CONST,      /* 常数 */
    SYM_EXPR_ADD,        /* 加法 */
    SYM_EXPR_SUB,        /* 减法 */
    SYM_EXPR_MUL,        /* 乘法 */
    SYM_EXPR_DIV,        /* 除法 */
    SYM_EXPR_POW,        /* 幂 */
    SYM_EXPR_NEG,        /* 取负 */
    SYM_EXPR_SIN,        /* sin */
    SYM_EXPR_COS,        /* cos */
    SYM_EXPR_SQRT,       /* sqrt */
    SYM_EXPR_ABS,        /* |x| */
} SymExprType;

/* ── 前向声明 ── */
typedef struct SymExprNode SymExprNode;

/**
 * @brief 符号表达式节点 —— 借鉴 Eigen CwiseBinaryOp
 *
 * 二叉树结构表示符号表达式。在需要求值或化简前，
 * 表达式以树形式懒加载存在。
 */
struct SymExprNode {
    SymExprType type;
    union {
        struct { int var_id; } var;                /* 变量引用 */
        struct { double value; } constant;         /* 常量 */
        struct { SymExprNode *lhs, *rhs; } binary; /* 二元运算 */
        struct { SymExprNode *operand; } unary;    /* 一元运算 */
        struct { SymExprNode *base; int exp; } pow; /* 幂运算 */
    };
};

/* ── 构建表达式（借鉴 Eigen 运算符重载的 C 语言等效） ── */

/* 创建变量节点 */
SymExprNode *sym_var(int var_id);

/* 创建常量节点 */
SymExprNode *sym_const(double value);

/* 二元运算（借鉴 operator+/operator-/operator*/operator/） */
SymExprNode *sym_add(SymExprNode *a, SymExprNode *b);
SymExprNode *sym_sub(SymExprNode *a, SymExprNode *b);
SymExprNode *sym_mul(SymExprNode *a, SymExprNode *b);
SymExprNode *sym_div(SymExprNode *a, SymExprNode *b);
SymExprNode *sym_pow(SymExprNode *base, int exp);

/* 一元运算 */
SymExprNode *sym_neg(SymExprNode *a);
SymExprNode *sym_sin(SymExprNode *a);
SymExprNode *sym_cos(SymExprNode *a);
SymExprNode *sym_sqrt(SymExprNode *a);

/* ── 惰性求值（借鉴 Eigen evalTo） ── */

/**
 * @brief 将惰性符号表达式展开为多项式标准型
 *
 * 借鉴 Eigen 的 evalTo()——表达式树在此时被遍历并求值。
 * 所有中间运算在单一遍历中完成，无中间表达式树被创建。
 *
 * @param expr  惰性表达式树
 * @param poly  输出的多项式（规范型）
 * @return 成功返回 0，失败返回错误码
 */
int sym_expand_to_poly(const SymExprNode *expr, void *poly);

/**
 * @brief 对给定变量值数值化表达式
 * @param expr   惰性表达式树
 * @param values 变量赋值表（var_id → double）
 * @return 数值结果
 */
double sym_eval_numeric(const SymExprNode *expr,
                         const double *values);

/* ── 表达式销毁 ── */
void sym_expr_free(SymExprNode *expr);

#endif /* SYM_EXPR_H */
```

**使用示例 —— 展示惰性求值与立即求值的对比：**

```c
/*
 * Eigen 风格：惰性表达式构建
 *
 * 在 Eigen 中: VectorXd r = a + b * c;
 *   等价于: CwiseBinaryOp<plus, VectorXd, CwiseBinaryOp<mul, VectorXd, VectorXd>>
 *   只在 operator= 时求值。
 *
 * Lv-00 等效:
 */

/* 构建惰性表达式树（无实际计算） */
SymExprNode *x = sym_var(0);
SymExprNode *y = sym_var(1);
SymExprNode *z = sym_var(2);

SymExprNode *expr = sym_add(
    sym_mul(sym_const(2.0), x),     /* 2*x */
    sym_add(
        sym_mul(sym_const(3.0), y), /* + 3*y */
        sym_mul(sym_const(4.0), z)  /* + 4*z */
    )
);
/* 此时未进行任何实际多项式展开 */

/* 触发求值：展开为规范多项式 */
Polynomial *poly;
sym_expand_to_poly(expr, poly);
/* 现在 poly = 2*x + 3*y + 4*z */

/* 或者：数值化求值 */
double values[] = {1.0, 2.0, 3.0};  /* x=1, y=2, z=3 */
double result = sym_eval_numeric(expr, values);
/* result = 2*1 + 3*2 + 4*3 = 20.0 */

sym_expr_free(expr);
```

### 3.3 几何变换 API 设计

借鉴 Eigen 的 Geometry 模块，提供语义化的几何变换 API：

```c
/**
 * @file include/lv00/geom_transform.h
 * @brief 几何变换 —— 借鉴 Eigen/Geometry 模块
 *
 * 提供语义化的几何变换操作，减少裸矩阵操作的错误风险。
 * 支持变换的链式组合（借鉴 Eigen 的 Transform operator*）。
 */

#ifndef GEOM_TRANSFORM_H
#define GEOM_TRANSFORM_H

#include "lv00/geometry_types.h"

/* ── 变换类型标记（借鉴 Eigen Transform 模板参数） ── */
typedef enum {
    TRANSFORM_AFFINE,     /* 仿射变换（平移+旋转+缩放+剪切） */
    TRANSFORM_ISOMETRY,   /* 等距变换（平移+旋转+反射，保距） */
    TRANSFORM_SIMILARITY, /* 相似变换（等距+均匀缩放，保角） */
    TRANSFORM_PROJECTIVE, /* 射影变换（最一般） */
} TransformMode;

/**
 * @brief 几何变换 —— 借鉴 Eigen::Transform<T, 3, Affine>
 *
 * 封装 4x4 齐次变换矩阵及其语义类型。
 * 推荐使用构造器函数而非直接操作矩阵元素。
 */
typedef struct {
    mat4 matrix;               /* 4x4 齐次矩阵（列主序） */
    TransformMode mode;        /* 变换类型标记 */
    bool is_identity;          /* 快速判断是否为单位变换 */
} GeomTransform;

/* ── 构造器（借鉴 Eigen 的 Translation / AngleAxis / Scaling） ── */

/** @brief 创建平移变换（借鉴 Eigen::Translation<T, 3>） */
GeomTransform geom_translate(double tx, double ty, double tz);

/** @brief 创建旋转变换-轴角（借鉴 Eigen::AngleAxis<T>） */
GeomTransform geom_rotate_axis(double ax, double ay, double az,
                                 double angle_rad);

/** @brief 创建绕 X 轴旋转 */
GeomTransform geom_rotate_x(double angle_rad);

/** @brief 创建绕 Y 轴旋转 */
GeomTransform geom_rotate_y(double angle_rad);

/** @brief 创建绕 Z 轴旋转 */
GeomTransform geom_rotate_z(double angle_rad);

/** @brief 从四元数创建旋转 */
GeomTransform geom_rotate_quat(const quat *q);

/** @brief 创建缩放变换（借鉴 Eigen::Scaling<T, 3>） */
GeomTransform geom_scale(double sx, double sy, double sz);

/** @brief 创建反射变换-关于平面 */
GeomTransform geom_reflect_plane(double nx, double ny, double nz, double d);

/** @brief 创建绕任意轴（通过两点定义）的旋转变换 */
GeomTransform geom_rotate_around_line(double p1x, double p1y, double p1z,
                                        double p2x, double p2y, double p2z,
                                        double angle_rad);

/* ── 变换组合（借鉴 Eigen::Transform::operator*） ── */

/** @brief 组合两个变换：T1 ∘ T2（先应用 t2，再应用 t1） */
GeomTransform geom_transform_compose(const GeomTransform *t1,
                                       const GeomTransform *t2);

/* ── 变换应用 ── */

/** @brief 对点应用变换（借鉴 transform * point） */
void geom_transform_point(const GeomTransform *t,
                           double px, double py, double pz,
                           double *ox, double *oy, double *oz);

/** @brief 对向量应用变换（忽略平移分量） */
void geom_transform_vector(const GeomTransform *t,
                            double vx, double vy, double vz,
                            double *ox, double *oy, double *oz);

/** @brief 逆变换 */
GeomTransform geom_transform_inverse(const GeomTransform *t);

/** @brief 插值两个变换（用于动画，借鉴 Eigen slerp） */
GeomTransform geom_transform_interpolate(const GeomTransform *a,
                                           const GeomTransform *b,
                                           double t);

#endif /* GEOM_TRANSFORM_H */
```

### 3.4 矩阵分解接口

```c
/**
 * @file include/lv00/matrix_decomp.h
 * @brief 矩阵分解 —— 借鉴 Eigen 的 LU/QR/SVD 体系
 *
 * 为 Lv-00 数值路径提供必要的矩阵分解能力。
 * 当前实现：稠密 LU + 稀疏迭代法。
 * 未来扩展：通过外部后端调用 LAPACK 的 QR/SVD。
 */

#ifndef MATRIX_DECOMP_H
#define MATRIX_DECOMP_H

#include <stdbool.h>

/* ── 稠密 LU 分解（借鉴 Eigen::PartialPivLU） ── */

/**
 * @brief 带部分主元选择的 LU 分解 (PA = LU)
 *
 * 借鉴 Eigen::PartialPivLU::compute()。
 * 用于解决小规模稠密线性系统（如 n < 200 的约束方程组）。
 *
 * @param A        n×n 方阵（列主序，输入时存储 A，输出时存储 L+U）
 * @param n        矩阵维度
 * @param pivots   输出：置换向量 pivots[i] = 交换行索引
 * @return 成功返回 0，矩阵奇异返回 -1
 */
int matrix_lu_decompose(double *A, int n, int *pivots);

/**
 * @brief 使用 LU 分解求解 Ax = b
 * @param LU      LU 分解后的矩阵（matrix_lu_decompose 的输出）
 * @param n       维度
 * @param pivots  置换向量
 * @param b       右端向量（输入时存储 b，输出时存储解 x）
 */
void matrix_lu_solve(const double *LU, int n,
                      const int *pivots, double *b);

/**
 * @brief 使用 LU 分解求逆矩阵
 * @param LU      LU 分解后的矩阵
 * @param n       维度
 * @param pivots  置换向量
 * @param inv     输出：逆矩阵（列主序，n×n）
 */
void matrix_lu_inverse(const double *LU, int n,
                        const int *pivots, double *inv);

/* ── 稀疏迭代求解（借鉴 Eigen::ConjugateGradient / Eigen::BiCGSTAB） ── */

/**
 * @brief 共轭梯度法求解 Ax = b（对称正定）
 * @param n       维度
 * @param matvec  矩阵-向量乘法函数 A * v → out
 * @param b       右端向量
 * @param x       初始猜测/输出解
 * @param max_iter 最大迭代次数
 * @param tol     收敛容差
 * @return 实际迭代次数，-1 表示不收敛
 */
int matrix_cg_solve(int n,
                     void (*matvec)(const double *v, double *out, void *ctx),
                     const double *b, double *x,
                     int max_iter, double tol, void *ctx);

/**
 * @brief BiCGSTAB 法求解（通用非对称）
 *
 * 用于约束图产生的非对称稀疏系统。
 */
int matrix_bicgstab_solve(int n,
                           void (*matvec)(const double *v, double *out, void *ctx),
                           const double *b, double *x,
                           int max_iter, double tol, void *ctx);

/* ── 特征值分解（借鉴 Eigen::SelfAdjointEigenSolver） ── */

/**
 * @brief 对称矩阵特征值分解（Jacobi 迭代法）
 *
 * 用于二次型标准化，如将一般二次曲线 Ax^2+Bxy+Cy^2+... = 0
 * 转化为标准型 λ1*x'^2 + λ2*y'^2 + ... = const。
 */
int matrix_sym_eigen_jacobi(double *A, int n,
                             double *eigenvalues, double *eigenvectors,
                             int max_iter, double tol);

#endif /* MATRIX_DECOMP_H */
```

### 3.5 小矩阵栈分配与缓存策略

```c
/**
 * @brief 小矩阵在栈上分配 —— 借鉴 Eigen 的固定大小矩阵策略
 *
 * 统计 Lv-00 代码路径中矩阵操作的形状分布：
 *
 * | 矩阵大小 | 出现频率 | 每操作字节 | 建议分配 |
 * |---------|---------|-----------|---------|
 * | 2x2     | 高（点/线相交）  | 32 B  | 栈上 |
 * | 3x3     | 高（3D 旋转）   | 72 B  | 栈上 |
 * | 4x4     | 极高（变换矩阵） | 160 B | 栈上 |
 * | 6x6     | 中（3D 约束系统）| 288 B | 栈上 |
 * | n×n     | 低（大规模求解） | 可变   | 堆上（alloc） |
 *
 * 实现策略：提供一个"小矩阵"分配器，当 n ≤ 6 时使用
 * alloca()/VLA 栈分配，否则回退到 malloc()。
 */

#include <alloca.h>  /* 或 _alloca() on MSVC */

/* 小矩阵判定阈值（借鉴 Eigen 的 MaxSizeAtCompileTime） */
#define MATRIX_SMALL_THRESHOLD 6

/**
 * @brief 分配矩阵内存 —— 借鉴 Eigen 的条件栈/堆分配
 * @param rows 行数
 * @param cols 列数
 * @return 连续内存指针（列主序）
 *
 * 当 rows*cols ≤ THRESHOLD^2 时使用 alloca（栈分配、无需释放），
 * 否则使用 malloc（需调用 matrix_free 释放）。
 */
static inline double *matrix_alloc(int rows, int cols) {
    if (rows * cols <= MATRIX_SMALL_THRESHOLD * MATRIX_SMALL_THRESHOLD) {
        /* 栈分配 —— 函数返回时自动释放，零开销 */
        return (double *)alloca(rows * cols * sizeof(double));
    } else {
        /* 堆分配 —— 大矩阵使用 malloc */
        return (double *)malloc(rows * cols * sizeof(double));
    }
}

/* 释放堆分配的矩阵（栈分配的矩阵无需调用此函数） */
static inline void matrix_free(double *data, int rows, int cols) {
    if (rows * cols > MATRIX_SMALL_THRESHOLD * MATRIX_SMALL_THRESHOLD) {
        free(data);
    }
}
```

---

## 4. 实现路线图

### 4.1 第一阶段：几何变换 API + 小矩阵优化（P2）

| 任务 | 文件 | 说明 |
|------|------|------|
| 实现 `mat4`、`mat3`、`mat2` 基础运算 | `include/lv00/geom_math.h` | 借鉴 Eigen Matrix4d 的运算集 |
| 实现 `GeomTransform` 变换类型 | `include/lv00/geom_transform.h` | 借鉴 Eigen::Transform 和 Geometry 模块 |
| 实现 `quat` 四元数运算（slerp、to_matrix 等） | `include/lv00/geom_transform.h` | 借鉴 Eigen::Quaternion |
| 实现小矩阵栈分配宏/函数 | `include/lv00/lv00_utils.h` | 借鉴 Eigen 固定大小矩阵优化 |
| 更新 `preset_transformations.h` 使用新 API | 修改现有文件 | 替换裸矩阵操作为语义化 API |

**预估规模**：约 400 行 C 代码

### 4.2 第二阶段：符号表达式惰性求值 + 单头聚合（P3）

| 任务 | 说明 |
|------|------|
| 实现 `SymExprNode` 符号表达式树 | 惰性构建，借鉴 Eigen 表达式模板哲学 |
| 实现 `sym_expand_to_poly()` 展开 | 将惰性表达式树一次性展开为规范多项式 |
| 实现 `sym_eval_numeric()` 数值化 | 对给定变量值求值 |
| 创建 `lv00.h` 聚合头文件 | 借鉴 Eigen/Core 的统一入口 |
| 提供 Amalgamated 单文件版本 | `lv00-amalgamated.h` + `lv00-amalgamated.c` |

**预估规模**：约 500 行 C 代码 + 构建脚本

### 4.3 第三阶段：矩阵分解（LU + 迭代法）（P4）

| 任务 | 说明 |
|------|------|
| 实现稠密 LU 分解（部分主元） | 借鉴 Eigen::PartialPivLU |
| 实现共轭梯度法（CG） | 借鉴 Eigen::ConjugateGradient |
| 实现 BiCGSTAB 法 | 用于非对称稀疏系统 |
| 实现对称特征值分解（Jacobi） | 用于二次型标准化 |
| 集成到求解器模块 | `solver.h` 的数值路径使用新分解 |

**预估规模**：约 600 行 C 代码

### 4.4 第四阶段：SIMD 加速 + 外部后端集成（P5）

| 任务 | 说明 |
|------|------|
| SSE2/AVX 版本的 `mat4_mul` 和 `vec3` 运算 | 手动 intrinsic 实现 |
| 编译期 SIMD 特性检测宏 | 借鉴 Eigen 的 `EIGEN_VECTORIZE` 检测 |
| LAPACK 后端桥接（可选） | 通过 FFI 调用 BLAS/LAPACK 的 QR/SVD |
| WASM 端编译时 SIMD 启用 | 为 Web 端启用 WASM SIMD 128 |
| 性能基准测试（标量 vs SIMD vs LAPACK） | 量化加速比 |

**预估规模**：约 300 行 C（intrinsic） + 100 行 CMake + 100 行 JS（WASM 配置）

---

## 5. 附录

### 附录 A：Eigen 核心模块与 Lv-00 映射速查

| Eigen 模块 | 头文件 | 核心类/函数 | Lv-00 C 端对应 |
|-----------|--------|-----------|--------------|
| Core | `Eigen/Core` | `Matrix`、`Array`、`Map` | `mat4`、`mat3`、`vec3` |
| Geometry | `Eigen/Geometry` | `Transform`、`Quaternion`、`AngleAxis` | `GeomTransform`、`quat` |
| LU | `Eigen/LU` | `PartialPivLU`、`FullPivLU` | `matrix_lu_decompose()` |
| QR | `Eigen/QR` | `HouseholderQR`、`ColPivHouseholderQR` | （外部后端/LAPACK） |
| SVD | `Eigen/SVD` | `JacobiSVD`、`BDCSVD` | （外部后端/LAPACK） |
| Cholesky | `Eigen/Cholesky` | `LLT`、`LDLT` | （外部后端/LAPACK） |
| Eigenvalues | `Eigen/Eigenvalues` | `EigenSolver`、`SelfAdjointEigenSolver` | `matrix_sym_eigen_jacobi()` |
| Sparse | `Eigen/Sparse` | `SparseMatrix`、`ConjugateGradient` | `matrix_cg_solve()`、`matrix_bicgstab_solve()` |
| IterativeLinearSolvers | `Eigen/IterativeLinearSolvers` | `BiCGSTAB` | `matrix_bicgstab_solve()` |

### 附录 B：表达式模板 vs 运行时惰性求值对比

| 特性 | Eigen 表达式模板（C++） | Lv-00 运行时惰性（C） |
|------|----------------------|---------------------|
| 惰性时机 | 编译期 | 运行时 |
| 零开销 | 是（编译期内联） | 否（有树遍历开销） |
| 类型安全 | 是（编译期检查维度匹配） | 否（运行时检查） |
| 支持条件分支 | 否（纯算术表达式） | 是（运行时决策） |
| 可序列化 | 否（编译期产物） | 是（树结构可序列化） |
| 可调试 | 困难（展开后的代码） | 简单（运行时树遍历） |
| 实现复杂度 | 极高（需要 C++ 模板元编程） | 中等（递归树遍历） |

对于 Lv-00，运行时惰性求值是更合理的选择——C 语言的限制使得编译期表达式模板不可行，而运行时树遍历的额外开销在符号计算场景中可接受（因为这些表达式的求值并非热路径，真正的热点在矩阵分解的数值计算中）。

### 附录 C：与 Lv-00 现有模块的关系

| 现有模块 | 与本文档的关系 |
|---------|-------------|
| `symbolic_coord.h` | 符号坐标的自然表达式形成惰性求值树的节点 |
| `normalization.h` | 归一化步骤是惰性表达式求值的关键触发点 |
| `constraint_graph.h` | 约束方程的求解使用矩阵分解 |
| `solver.h` | 数值路径直接调用本文档的矩阵分解 API |
| `preset_transformations.h` | 使用 `GeomTransform` 替代裸矩阵操作 |
| `preset_linear_algebra.h` | 线性代数预置函数块映射到本文档的矩阵运算 |

---

> **文档结束**
> 本文档详述了 Eigen 的纯头文件零依赖架构、表达式模板惰性求值、Geometry 几何变换模块、固定大小矩阵栈分配优化、矩阵分解体系和 SIMD 向量化六个核心借鉴点，并提供了完整的 C 代码示例和四阶段实现路线图。核心结论：(1) 借鉴 Eigen header-only 模式提供 `lv00.h` 聚合头文件和 Amalgamated 单文件分发；(2) 借鉴表达式模板惰性求值思想设计 `SymExprNode` 符号表达式树，在需要时一次性展开为规范多项式；(3) `GeomTransform` 语义化变换 API 替代裸矩阵操作；(4) 小矩阵（n<=6）使用栈分配避免 malloc 开销；(5) 选择性实现 LU/CG/BiCGSTAB/Jacobi 四种分解覆盖核心求解场景；(6) SIMD 作为远期加速路径。
