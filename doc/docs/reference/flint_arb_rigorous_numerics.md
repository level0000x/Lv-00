# FLINT/Arb 精确数值与区间算术参考文档

> 版本：1.0 | 日期：2026-05 | 语言：全部中文

---

## 1. 项目概述

### 1.1 简介

**FLINT**（Fast Library for Number Theory）是目前世界上最快的数论与多项式 C 语言库。项目托管于 [github.com/flintlib/flint](https://github.com/flintlib/flint)，其前身包括两个独立子项目：FLINT（数论/多项式方向）与 Arb（区间算术/特殊函数方向）。自 2023 年起，Arb 已正式合并入 FLINT 主仓库，成为其内置的区间算术模块。

FLINT 的核心设计哲学是"严格正确优于近似快速"——每一个计算结果都附带可验证的误差界，使用者无需手工估计误差传播，也不依赖浮点舍入惯例。这一哲学与 Lv-00 的"符号-数值双路径"理念高度契合，为 Lv-00 提供了第三条"区间路径"的参考范式。

### 1.2 技术栈

| 维度 | 详情 |
|------|------|
| 语言 | C（C99 标准），部分汇编优化 |
| 依赖 | GMP（GNU Multiple Precision Arithmetic Library）、MPFR（可选） |
| 构建系统 | CMake / Autotools 双轨 |
| 精度模型 | 任意精度有理数（fmpz, fmpq）、任意精度浮点（arf）、区间实数（arb）、区间复数（acb） |
| 核心数据结构 | `arb_t`（实数区间 `[mid ± rad]`）、`acb_t`（复数区间，实部虚部分别为 `arb_t`） |
| 多项式层 | `arb_poly`、`acb_poly`（任意精度多项式，每系数带误差界） |
| 矩阵层 | `arb_mat`、`acb_mat`（带误差界的稠密矩阵运算） |
| 特殊函数 | Gamma、Zeta、Bessel、超几何、椭圆函数等，全部返回严格区间 |

### 1.3 许可证

FLINT/Arb 整体采用 **LGPL v2.1+**（GNU Lesser General Public License）。该许可证允许 Lv-00 在以下前提下进行链接与分发：

- **静态链接**（将 FLINT 编译为 `.a` 后链接入 Lv-00 的 `liblv`）需要 Lv-00 的链接目标同样以 LGPL 兼容方式许可，或提供目标文件供用户重新链接。
- **动态链接**（将 FLINT 编译为 `.dll`/`.so`）则 Lv-00 可保留独立许可证，仅在分发时附带 FLINT 的许可证声明即可。
- 具体许可证文本见仓库中的 `LICENSE` 文件及 GMP/MPFR 各自的许可证。

---

## 2. 核心借鉴点

下表汇总 FLINT/Arb 中与 Lv-00 架构直接相关的设计要点。每一行对应一处"FLINT/Arb 做法"与"Lv-00 对应位置"的映射。

| 序号 | 借鉴点 | 出处（FLINT/Arb） | FLINT/Arb 做法 | Lv-00 对应 |
|------|--------|-------------------|---------------|------------|
| a | **区间算术（Ball Arithmetic）** | `arb.h` / `arb_t` | 每个实数计算返回 `[mid ± rad]` 形式的严格区间。mid 为任意精度浮点中心值，rad 为误差半径。加减乘除、幂运算、三角函数等全部支持区间语义。若 rad 覆盖零且 mid 符号不确定，则返回包含零的区间，绝不猜测符号。 | Lv-00 已有的两条路径："符号路径"（`SymbolicCoord` 精确推导）与"数值路径"（浮点快速近似）之间，新增第三条"区间路径"（`SYMBOLIC_COORD_INTERVAL`），提供"不完全符号但严格正确"的中间方案。 |
| b | **任意精度多项式** | `arb_poly.h` / `acb_poly.h` | 多项式以 `arb_poly_t` 表示，系数为 `arb_struct*`（即每个系数都是区间）。乘法、除法、求根、合成等运算全部附带误差界。例如 `arb_poly_mul(C, A, B, prec)` 返回的多项式在每个系数上都保证真值落在区间内。 | Lv-00 `mpz_poly.h` 中的 `mpz_poly_struct` 当前为精确整数系数多项式，可直接扩展为误差感知模式——新增 `mpz_poly_interval` 类型，包装 `arb_poly_t` 或自行实现区间系数版本。 |
| c | **特殊函数带误差界** | `acb_hypgeom.h` / `arb_hypgeom.h` | Gamma 函数（`acb_gamma`）、Bessel 函数（`acb_hypgeom_bessel_j`）、超几何函数（`acb_hypgeom_pfq`）等全部以严格区间返回。实现采用级数展开 + 余项严格上界的方法，确保"真值一定在返回区间内"。 | Lv-00 `preset_special_functions.h` 中的高级函数表（`PresetSpecialFunction` 枚举及其参数配置）当前仅记录函数元信息。可参考 Arb 的算法为这些函数实现区间求值后端，为渲染与数值检查提供严格保证。 |
| d | **C 语言原生接口** | 全部头文件 | FLINT/Arb 完全以 C 语言实现，接口为 `void arb_add(arb_t z, const arb_t x, const arb_t y, slong prec)` 风格——即输出参数在前（仿 GMP 风格），精度参数在末尾。无需 C++ 运行时、无虚函数、无异常。 | 与 Lv-00 技术栈（纯 C，无运行时依赖）完美兼容。可直接在 `symbolic_coord.h` 中通过 `#include "arb.h"` 绑定，并在 `lv_sym.c` 中实现绑定逻辑。编译时仅需链接 `-lflint -lgmp -lmpfr`。 |
| e | **已验证的线性代数** | `arb_mat.h` | `arb_mat_t` 为元素类型 `arb_struct` 的稠密矩阵。提供 `arb_mat_mul`、`arb_mat_inv`、`arb_mat_solve`、`arb_mat_eigenvalues` 等，全部以区间矩阵形式返回，保证真值落在每个元素的区间内。支持条件数估计（`arb_mat_cond`）以量化求解不确定性。 | Lv-00 的坐标变换计算（`coord_transform.h` 中的仿射/投影变换链）当前使用 `double` 浮点矩阵。可从 `arb_mat` 获得严格性保证：变换后的坐标为一个区间向量，用户可知"真坐标在此区域内的概率为 100%"。 |

### 2.1 详述：区间算术（Ball Arithmetic）——核心借鉴

区间算术是 FLINT/Arb 对整个精确数值计算社区最具影响力的贡献，也是 Lv-00 最重要的借鉴点。

**原理**：任意实数 r 表示为 `arb_t` 结构：

```
typedef struct {
    arf_struct mid;   // 中心值：任意精度浮点
    mag_struct rad;   // 误差半径：严格上界
} arb_struct;
```

语义为：真值一定落在 `[mid - rad, mid + rad]` 内。所有运算在计算时同时传播误差，例如：

```
若 x ∈ [mx ± rx], y ∈ [my ± ry]，则
x + y ∈ [mx + my ± (rx + ry + 1ulp)]
x × y ∈ [mx × my ± (|mx|·ry + |my|·rx + rx·ry + 1ulp)]
```

其中 `1ulp` 是对舍入误差的保守上界。Arb 的"ball"（球）命名来源于复数情形下误差区域为复平面上的圆盘（disk），实数情形下退化为区间。

**对 Lv-00 的意义**：Lv-00 当前的两条路径各有短板：

- **符号路径**：完全精确，但面对非线性方程、超越函数时符号化简不可判定。
- **数值路径**：速度快，但浮点误差可能导致错误的符号判断（例如误判 `expr > 0` 当表达式实际为极小正数但舍入为 0）。

**区间路径**填补了这一鸿沟：比数值路径慢若干常数倍（仍在可用范围内），但提供可验证的严格边界。对于临界情形（表达式值接近零），区间路径返回包含零的区间，由用户决定是否需要进一步符号化简或接受不确定性。

---

## 3. Lv-00 映射方案

### 3.1 新增 `SYMBOLIC_COORD_INTERVAL` 类型

在 `symbolic_coord.h` 中扩展 `SymbolicCoordType` 枚举，并新增以 `arb_t` 为底层存储的坐标类型。

```c
/* === symbolic_coord.h 新增片段 === */

#ifndef LV_SYMBOLIC_COORD_H
#define LV_SYMBOLIC_COORD_H

#include "lv_common.h"

/* ── 如果链接了 FLINT ── */
#ifdef LV_USE_FLINT
#include "arb.h"
#endif

/* ── 扩展 SymbolicCoordType ── */
typedef enum {
    SYMBOLIC_COORD_EXACT,      /* 已有：精确有理数/整数 */
    SYMBOLIC_COORD_DOUBLE,     /* 已有：双精度浮点近似 */
    SYMBOLIC_COORD_INTERVAL,   /* 新增：区间实数（arb） */
} SymbolicCoordType;

/* ── 新增 SymbolicCoord 联合成员 ── */
typedef struct SymbolicCoord {
    SymbolicCoordType type;
    union {
        mpq_t   exact_val;     /* SYMBOLIC_COORD_EXACT */
        double  approx_val;    /* SYMBOLIC_COORD_DOUBLE */
#ifdef LV_USE_FLINT
        arb_t   interval_val;  /* SYMBOLIC_COORD_INTERVAL */
#endif
    } data;
    slong precision;           /* 区间精度（二进制位数） */
} SymbolicCoord;

/* ── 工厂函数 ── */
#ifdef LV_USE_FLINT
void symbolic_coord_init_interval(SymbolicCoord *c, slong prec);
void symbolic_coord_set_interval_d(SymbolicCoord *c, double val, slong prec);
void symbolic_coord_set_interval_mpq(SymbolicCoord *c, const mpq_t val, slong prec);
void symbolic_coord_clear_interval(SymbolicCoord *c);
#endif

/* ── 区间算术包装 ── */
#ifdef LV_USE_FLINT
void symbolic_coord_add(SymbolicCoord *res,
                        const SymbolicCoord *a,
                        const SymbolicCoord *b);
void symbolic_coord_mul(SymbolicCoord *res,
                        const SymbolicCoord *a,
                        const SymbolicCoord *b);
/* ... 更多运算 ... */
#endif

#endif /* LV_SYMBOLIC_COORD_H */
```

### 3.2 求解器区间验证路径

在 `solver.h` 中新增"区间验证"模式——求解器先用浮点路径快速求出候选解，再通过区间算术验证该解的合法性。

```c
/* === solver.h 新增片段 === */

#ifndef LV_SOLVER_H
#define LV_SOLVER_H

#include "symbolic_coord.h"

/* ── 求解验证模式 ── */
typedef enum {
    SOLVER_VERIFY_NONE,       /* 已有：不验证 */
    SOLVER_VERIFY_SYMBOLIC,   /* 已有：符号路径验证 */
    SOLVER_VERIFY_INTERVAL,   /* 新增：区间算术验证 */
} SolverVerifyMode;

/* ── 区间验证结果 ── */
typedef enum {
    INTERVAL_VERIFY_VALID,       /* 解确实落在约束容差内 */
    INTERVAL_VERIFY_INVALID,     /* 解被证明落在约束容差外 */
    INTERVAL_VERIFY_UNCERTAIN,   /* 区间过宽，无法判断（需提精度重算） */
} IntervalVerifyResult;

/* ── 区间验证核心函数 ── */
#ifdef LV_USE_FLINT

/**
 * 对候选解执行区间验证。
 *
 * @param soln       候选解向量（可能来自浮点路径）
 * @param n_vars     变量个数
 * @param constraints 约束数组
 * @param n_cons     约束个数
 * @param prec       区间算术精度（二进制位，典型值 128~256）
 * @return           验证结果
 */
IntervalVerifyResult solver_verify_interval(
    const SymbolicCoord soln[],
    int n_vars,
    const SolverConstraint constraints[],
    int n_cons,
    slong prec
);

#endif /* LV_USE_FLINT */

#endif /* LV_SOLVER_H */
```

### 3.3 多项式区间运算扩展

`mpz_poly.h` 当前仅支持精确整数系数多项式。扩展方案如下：

```c
/* === mpz_poly.h 扩展片段 === */

/* ── 误差感知多项式 ── */
#ifdef LV_USE_FLINT
#include "arb_poly.h"

typedef struct {
    arb_poly_t poly;      /* FLINT 区间多项式 */
    slong     default_prec;  /* 默认求值精度 */
} mpz_poly_interval_t;

/* 从精确 mpz_poly 构造区间多项式 */
void mpz_poly_to_interval(mpz_poly_interval_t *dst,
                          const mpz_poly_struct *src,
                          slong prec);

/* 在区间点 x 处求值，返回严格区间 */
void mpz_poly_eval_interval(arb_t result,
                            const mpz_poly_interval_t *poly,
                            const arb_t x,
                            slong prec);

/* 区间多项式的根隔离（返回根的区间列表） */
void mpz_poly_roots_interval(arb_ptr *roots,
                             int *n_roots,
                             const mpz_poly_interval_t *poly,
                             slong prec);
#endif
```

### 3.4 编译配置

在 CMakeLists.txt 中通过 feature flag 控制区间路径的启用：

```cmake
# 选项：是否启用 FLINT/Arb 区间算术
option(LV_USE_FLINT "Enable FLINT/Arb interval arithmetic" OFF)

if(LV_USE_FLINT)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FLINT REQUIRED flint>=3.0)
    target_compile_definitions(lv PRIVATE LV_USE_FLINT)
    target_include_directories(lv PRIVATE ${FLINT_INCLUDE_DIRS})
    target_link_libraries(lv PRIVATE ${FLINT_LIBRARIES})
endif()
```

---

## 4. 实现路线图

| 阶段 | 内容 | 工期（估） | 依赖 |
|------|------|-----------|------|
| **Phase 1** | **Interval 类型集成** | 2-3 周 | FLINT 编译成功，CMake 集成通过 |
| 1.1 | `SymbolicCoord` 新增 `SYMBOLIC_COORD_INTERVAL` 枚举值与 `arb_t` 联合成员 | 2 天 | - |
| 1.2 | 实现 `symbolic_coord_init_interval`、`set`、`clear` 等生命周期函数 | 3 天 | 1.1 |
| 1.3 | 实现基本四则运算 + 比较函数的区间版本（加、减、乘、除、平方根、绝对值、正负判断） | 5 天 | 1.2 |
| 1.4 | 单元测试：区间运算正确性（包含零区间、宽区间、临界情形） | 5 天 | 1.3 |
| **Phase 2** | **求解器区间验证** | 2-3 周 | Phase 1 完成 |
| 2.1 | `solver.h` 新增 `SolverVerifyMode` 和 `IntervalVerifyResult` 枚举 | 1 天 | - |
| 2.2 | 实现 `solver_verify_interval`：将候选解映射为区间向量，代入约束式计算残差区间 | 5 天 | 2.1 + Phase 1 |
| 2.3 | 实现自动精度提升策略：当 `UNCERTAIN` 返回时，翻倍 `prec` 后重试（最多 N 次） | 3 天 | 2.2 |
| 2.4 | 集成测试：用标准 benchmark（如多项式方程组）对比符号验证与区间验证 | 5 天 | 2.3 |
| **Phase 3** | **多项式区间运算** | 2-3 周 | Phase 1 完成 |
| 3.1 | 实现 `mpz_poly_interval_t` 类型，包装 `arb_poly_t` | 2 天 | - |
| 3.2 | 实现 `mpz_poly_to_interval` 转换函数 | 2 天 | 3.1 |
| 3.3 | 实现 `mpz_poly_eval_interval`（在区间点上求值） | 3 天 | 3.2 |
| 3.4 | 实现 `mpz_poly_roots_interval`（区间根隔离，基于 Durand-Kerner 方法 + 误差后验） | 5 天 | 3.3 |
| 3.5 | 与 `render.c` 集成：绘制函数图像时，若精度不足导致渲染瑕疵，自动切换为区间求值模式 | 5 天 | 3.4 |
| **Phase 4** | **文档与发布** | 1 周 | Phase 1-3 全部完成 |
| 4.1 | 编写 `docs/developer/interval_arithmetic.md`（开发者文档） | 2 天 | - |
| 4.2 | 编写用户手册章节"Lv-00 的三种精度路径" | 1 天 | - |
| 4.3 | 发布说明、已知限制、与符号路径的性能对比数据 | 2 天 | - |

**总工期估计**：约 7-10 周（视团队成员在 FLINT/GMP 上的熟悉程度）。

---

## 5. 附录

### 5.1 参考链接

| 资源 | 链接 |
|------|------|
| FLINT 主仓库 | [https://github.com/flintlib/flint](https://github.com/flintlib/flint) |
| FLINT 官方文档 | [https://flintlib.org/doc/](https://flintlib.org/doc/) |
| Arb 原始论文（Johansson, 2017） | "Arb: Efficient Arbitrary-Precision Midpoint-Radius Interval Arithmetic", *IEEE Trans. Computers* |
| GMP 库 | [https://gmplib.org/](https://gmplib.org/) |
| MPFR 库 | [https://www.mpfr.org/](https://www.mpfr.org/) |
| Ball Arithmetic 综述 | [https://arblib.org/](https://arblib.org/)（Arb 模块专题页） |

### 5.2 Lv-00 符号-数值-区间三条路径对比

| 路径 | 存储类型 | 精确性 | 速度 | 适用场景 |
|------|---------|--------|------|---------|
| **符号路径** | `mpq_t` / `mpz_t` / 符号 AST | 完全精确（符号闭式存在时） | 最慢 | 恒等式验证、几何定理证明、编译期推导 |
| **数值路径** | `double` | 近似（IEEE 754 舍入） | 最快 | 实时渲染、交互拖拽、快速预览 |
| **区间路径** | `arb_t` | 严格（附带误差界） | 中等（约为数值的 5-20x） | 临界判断、方程验证、精度敏感计算 |

### 5.3 关键术语中英对照

| 英文 | 中文 |
|------|------|
| Ball Arithmetic | 球算术 / 区间算术 |
| Midpoint-Radius Interval | 中心-半径区间 |
| Rigorous Numerics | 严格数值 |
| Error Bound | 误差界 |
| Arbitrary Precision | 任意精度 |
| Verified Linear Algebra | 已验证线性代数 |
| Condition Number | 条件数 |
| Root Isolation | 根隔离 |
| ulp (unit in the last place) | 末位单位 |

### 5.4 相关 Lv-00 文档

| 文档 | 路径 |
|------|------|
| 符号坐标系统设计 | `docs/design/symbolic_coord.md`（待创建） |
| 求解器架构文档 | `docs/design/solver.md`（待创建） |
| 多项式模块（mpz_poly） | `docs/reference/mpz_poly.md`（待创建） |
| 渲染管线 | `docs/reference/render.md`（待创建） |

---

> 本文档仅供 Lv-00 项目内部参考。FLINT/Arb 的代码和文档版权归其原作者所有，LGPL 许可证条款适用于任何对 FLINT 代码的修改与分发。
