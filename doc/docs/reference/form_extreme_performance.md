# Lv-00 参考设计：FORM 极端性能优化——基于磁盘的项排序与合并算法

> **版本**: 1.0.0  
> **日期**: 2026-05-24  
> **参考**: [FORM](https://github.com/vermaseren/form) —— 面向高能物理符号计算的极端性能系统  
> **目标**: 借鉴 FORM 的"磁盘即内存"项排序合并哲学，为 Lv-00 大规模符号计算场景（>10万项多项式化简、多变量 Groebner 基计算）提供性能优化路径

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴要点](#2-核心借鉴要点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)

---

## 1. 项目概述

### 1.1 FORM 是什么

FORM 是由 Jos Vermaseren 于 1980 年代发起的符号代数系统，专为高能物理中费曼图计算而设计——这一领域的典型需求是处理包含百万甚至千万项的多项式展开。FORM 不像 Mathematica 或 Maple 那样追求通用符号计算，而是极端聚焦在一个核心场景：**大规模多项式的排序、合并与化简**。

FORM 的核心理念与主流符号代数系统截然不同：

1. **磁盘即内存**：当多项式项数超过物理内存时，FORM 自动将中间结果写入磁盘。项排序算法（如锦标赛排序）直接在磁盘文件上操作，通过流式读取和归并段（merge runs）实现外存排序。2000 年 Tentyukov 和 Vermaseren 发表的"FORM 磁盘排序"论文是该领域的经典。

2. **项优先于表达式**：FORM 不构建完整的表达树（如 Mathematica 的 `FullForm`），而是将每个多项式表示为"项的流"（stream of terms），所有操作——加法、乘法、替换——都在项流上进行。这消除了表达式树的内存开销和指针追赶的缓存未命中。

3. **编译期调度**：FORM 程序是编译执行的——用户编写的 `.frm` 文件被编译为紧凑的字节码，然后由运行时解释器高效执行。模块（module）预编译使频繁调用的子程序无需重复解析。

4. **并行项处理**：FORM 的 TFORM（多线程版）和 ParFORM（MPI 分布式版）将排序和合并任务分发到多核/多节点上，利用项的自然可并行性。

```
* FORM 示例：费曼图积分展开
Symbol x, y, z;
CFunction f;
Local expr = (x + y + z)^20;  * 展开 (x+y+z)^20
.sort                         * 触发排序和合并
Print expr;
.end
```

### 1.2 为什么借鉴 FORM

Lv-00 的现有求解器（`solver.h`）使用 GMP 进行精确有理数运算，Groebner 基计算受限于度<=2。当面对以下场景时，项数可能爆炸式增长：

- 多变量高次多项式系统的满 Groebner 基计算
- 符号坐标下的几何约束多项式展开（如 10 个自由点 × 每个 2 个坐标 × 20 条约束）
- 消元理想计算中的中间结果膨胀

当前 Lv-00 对这些场景无有效应对手段——内存耗尽即求解失败。FORM 的"磁盘比内存更大"哲学为 Lv-00 提供了在有限内存下处理大规模符号计算的可行路径。

---

## 2. 核心借鉴要点

### 2.1 基于磁盘的项排序与合并

FORM 的磁盘排序算法是其性能的基石。其核心思想借鉴自数据库系统的外部排序（external sorting）：

| 阶段 | FORM 做法 | Lv-00 对应 |
|------|----------|-----------|
| **第一阶段：生成归并段** | 读取内存可容纳的项批次，每批次在内存中排序后写入临时磁盘文件 | Lv-00 的多项式按 `mpz_poly` 批次读取，排序后写临时段 |
| **第二阶段：多路归并** | 打开所有归并段文件，用最小堆选择当前最小的项，逐步写入最终输出 | Lv-00 对合并后的 Groebner 基做归并去重 |
| **项比较器** | 基于单项式序（lex/grevlex）的快速比较，FORM 使用预计算的哈希值加速 | Lv-00 已有 `PolynomialDegree` 和 `TermOrder` |
| **原地压缩** | 相同键的项在归并过程中即时合并，不产生临时副本 | Lv-00 的项合并应避免重复分配 |

这一模式对 Lv-00 的借鉴意义：当 `mpz_poly` 的项数超过阈值（如 10,000 项），自动切换到磁盘模式——多项式不再整体存储于内存，而是以"项文件"（term file）的方式持久化。

### 2.2 项流架构 vs 表达式树架构

FORM 的"项流"（term stream）架构与主流符号系统形成根本差异：

| 维度 | 表达式树（Mathematica/SymPy） | 项流（FORM） |
|------|---------------------------|------------|
| 内存模型 | 每个子表达式是一个堆对象（指针链） | 紧凑的项记录数组（连续内存） |
| 缓存友好度 | 低——指针追逐导致大量缓存未命中 | 高——项记录是连续字节块 |
| 中间结果膨胀 | 表达式树的每个中间节点产生内存分配 | 只有排序/合并阶段需要临时文件 |
| 可并行性 | 难以并行——表达式 DAG 存在数据依赖 | 天然可并行——项处理是 embarrassingly parallel |
| 适合场景 | 交互式符号推导（小规模） | 大规模批量代数（百万项级别） |

Lv-00 目前的 `mpz_poly` 和 `SymbolicCoord` 更接近表达式树模型——每个多项式节点独立分配。借鉴 FORM 的项流模型，可以将多项式的大规模中间表示从"表达式 DAG"切换为"项文件流"。

### 2.3 编译期模块化

FORM 的程序结构是高度模块化的：

```
* main.frm —— 主程序
#include declarations.h    * 头文件：声明符号和函数
#call compute_master(20)    * 调用预编译模块
.end

* compute_master.prc —— 预编译模块
#procedure compute_master(N)
   Local F = 0;
   #do i=1,`N'
      Local T`i' = ...;
      F = F + T`i';
   #enddo
   .sort
#endprocedure
```

这种"预编译模块 + 编译期宏展开"的模式，与 Lv-00 的"函数块 + 公理包预加载"形成直接对应。FORM 的模块预编译确保运行时无解析开销——这对 Lv-00 频繁调用的几何构造函数块（如 `midpoint`、`intersection`）有直接参考价值。

---

## 3. Lv-00 映射方案

### 3.1 大规模多项式的磁盘后端

借鉴 FORM 的外部排序架构，为 Lv-00 多项式运算增加磁盘模式：

```c
/**
 * @brief 大规模多项式磁盘后端（FORM 风格）
 *
 * 当 mpz_poly 的项数超过阈值时，不将所有项保留在内存中，
 * 而是将项写入磁盘文件。所有操作（+、*、化简）通过流式
 * 扫描磁盘上的项流来完成。
 */
typedef struct DiskPolyBackend {
    char *temp_dir;              /* 临时文件目录 */
    size_t memory_threshold;     /* 触发磁盘模式的内存阈值（字节） */
    size_t max_run_size;         /* 单个归并段的最大项数 */
    int merge_degree;            /* 多路归并的路数 */

    /* 排序状态 */
    int32_t *runs;               /* 归并段文件描述符数组 */
    int run_count;               /* 当前归并段数 */
    mpz_poly_term **heap;        /* 多路归并的最小堆 */
    int heap_size;
} DiskPolyBackend;

/**
 * @brief 启动磁盘模式的外部排序
 *
 * @param backend     磁盘后端
 * @param terms       输入项数组（可能很大）
 * @param term_count  项数量
 * @param out_file    输出：排序合并后的项文件路径
 * @return 0 成功，-1 失败
 */
int diskpoly_external_sort(DiskPolyBackend *backend,
                            mpz_poly_term *terms,
                            size_t term_count,
                            const char *out_file);
```

### 3.2 项流抽象

为 Lv-00 引入 FORM 风格的"项流迭代器"——代数运算在流上执行，而非在内存中的完整多项式对象上执行：

```c
/**
 * @brief 项流迭代器（FORM 风格）
 *
 * 这是一个惰性求值的项序列——可以来自内存中的 mpz_poly，
 * 也可以来自磁盘上的项文件。上层操作（加法、乘法、替换）
 * 通过流组合子实现，无需知道底层是内存还是磁盘。
 */
typedef struct TermStream {
    /* 虚函数表——FORM 的"项流是统一的抽象" */
    bool (*has_next)(struct TermStream *self);
    mpz_poly_term (*next)(struct TermStream *self);
    void (*reset)(struct TermStream *self);
    void (*destroy)(struct TermStream *self);

    /* 内部状态 */
    void *backend_state;         /* 内存或磁盘后端 */
    TermOrder order;             /* 流保证的项序 */
    size_t estimated_count;      /* 估计项数 */
} TermStream;

/* 从内存多项式创建流 */
TermStream *termstream_from_memory(const mpz_poly *poly);

/* 从磁盘项文件创建流 */
TermStream *termstream_from_file(const char *filepath, TermOrder order);

/* 流加法：合并两个已排序的流 */
TermStream *termstream_add(TermStream *a, TermStream *b);

/* 流乘法：惰性计算 a * b 的每一项 */
TermStream *termstream_mul(TermStream *a, TermStream *b,
                            DiskPolyBackend *disk_backend);
```

### 3.3 磁盘模式与内存模式的自动切换

FORM 的策略是"永不耗尽内存"——系统检测可用内存，在饱和前自动切换到磁盘。Lv-00 应实现类似的透明切换：

```c
/**
 * @brief 透明磁盘/内存切换（FORM 风格）
 *
 * 根据当前可用内存，自动决定多项式运算使用内存路径还是磁盘路径。
 * 上层代码无需感知此切换——TermStream 抽象层负责统一。
 */
typedef enum PolyStorageMode {
    POLY_STORAGE_AUTO,       /* 自动选择（默认） */
    POLY_STORAGE_MEMORY,     /* 强制内存模式 */
    POLY_STORAGE_DISK,       /* 强制磁盘模式 */
} PolyStorageMode;

/**
 * @brief 获取推荐的存储模式
 *
 * 基于 (可用物理内存 / 总内存) 和当前项数估算来选择模式。
 * - RAM > 2 * estimated_memory → 内存模式
 * - 否则 → 磁盘模式
 */
PolyStorageMode diskpoly_recommend_mode(size_t estimated_term_count,
                                         size_t bytes_per_term);

/* 使用示例 */
PolyStorageMode mode = diskpoly_recommend_mode(
    poly_term_count, sizeof(mpz_poly_term));
if (mode == POLY_STORAGE_DISK) {
    /* 路径 A：磁盘模式 —— 多项式以项文件形式存在 */
    DiskPolyBackend *db = diskpoly_backend_create(&config);
    TermStream *a = termstream_from_file("poly_a.terms", order);
    TermStream *b = termstream_from_file("poly_b.terms", order);
    TermStream *sum = termstream_add(a, b);
    /* ... 使用 sum 进行 Groebner 基约化 ... */
} else {
    /* 路径 B：内存模式 —— 现有 fastpath */
    mpz_poly *result = mpz_poly_add(a_in_mem, b_in_mem);
}
```

### 3.4 Groebner 基计算的项合并优化

FORM 中项合并是其最高频操作。在 Buchberger 算法中，每个 S-多项式的计算和约化都涉及大量项排序和合并。借鉴 FORM 的经验：

| Buchberger 步骤 | FORM 项流优化 | Lv-00 实现 |
|----------------|------------|-----------|
| S-多项式 `spoly(f, g)` | 两个已排序项流的惰性减法——不创建完整 S-多项式 | `termstream_sub(f_stream, g_stream)` |
| 约化 `reduce(p, G)` | 对 G 中每个多项式，流式检查首项能否整除 | `termstream_try_reduce(p_stream, G_streams[])` |
| 添加到基 | 新多项式直接以项文件形式添加到归并段集合 | `run_set_add(G_run_files, new_term_file)` |
| 最终基输出 | 多路归并所有段，合并相同项，输出规范基 | `diskpoly_final_merge(G_run_files)` |

### 3.5 映射到现有 solver.h

| 现有 `solver.h` 结构 | FORM 借鉴后的改进 |
|---------------------|-----------------|
| `mpz_poly`（全内存表示） | 增加 `TermStream` 抽象层，透明支持内存/磁盘双模式 |
| `solve_algebraic_system()`（固定内存在内存中） | 增加 `diskpoly_external_sort()` 外包大系统 |
| `groebner_basis()`（度<=2 硬件限制） | 使用磁盘后端解除度数限制，可处理任意次数多项式 |
| `smtsolver_solve()`（无记忆中间结果） | 中间多项式以项文件形式缓存到磁盘，支持断点续算 |
| `SymbolicCoord` 的坐标计算 | 坐标的代数表达式超过阈值时，切换到流模式 |

---

## 4. 实现路线图

### 4.1 第一阶段：项流抽象与内存模式（P4）

| 任务 | 文件 | 说明 |
|------|------|------|
| 定义 `TermStream` 虚函数表和基础操作 | `include/lv00/mpz_poly.h` | 扩展多项式头文件 |
| 实现 `termstream_from_memory` / `_add` / `_sub` | `src/mpz_poly.c` | 内存模式下的流操作 |
| 实现 `diskpoly_recommend_mode()` | `src/mpz_poly.c` | 自动模式选择 |
| 基准测试：内存流 vs 传统 `mpz_poly_add` | `benchmark_results/` | 验证流抽象无性能损失 |

**预估规模**：约 300 行 C 代码

### 4.2 第二阶段：磁盘后端实现（P4+）

| 任务 | 文件 | 说明 |
|------|------|------|
| 定义 `DiskPolyBackend` 和外部排序实现 | `src/disk_poly.c`（新建） | 归并段生成 + 多路归并 |
| 实现 `termstream_from_file` | `src/disk_poly.c` | 从磁盘项文件读取流 |
| 实现 `termstream_mul` 的磁盘模式 | `src/disk_poly.c` | 惰性流乘法 |
| 测试：100 万项多项式的排序和加法 | `tests/` | 验证功能正确性和性能 |

**预估规模**：约 500 行 C 代码

### 4.3 第三阶段：Groebner 基计算集成（远期）

| 任务 | 说明 |
|------|------|
| Groebner 基计算的磁盘模式 | Buchberger 算法中的多项式以项文件形式管理 |
| 增量归并段管理 | 新生成的 S-多项式即时合并到已有归并段 |
| 断点续算 | 将未完成的 Groebner 基计算状态序列化到磁盘后恢复 |
| 编译期预计算 | 频繁使用的公理包多项式预编译为项文件 |

---

## 附录 A：FORM 与 Lv-00 符号计算架构对照

| FORM 概念 | Lv-00 对应概念 | 关键差异 |
|----------|--------------|---------|
| 项流（Term Stream） | `TermStream`（新增抽象层） | Lv-00 原无流抽象 |
| 磁盘排序与归并 | `DiskPolyBackend`（新增） | FORM 在 1990 年代已成熟 |
| 预编译模块（`.prc`） | 函数块预编译 + `FuncBlock` 寄存器 | 结构相似但编译目标不同 |
| 表达式（Expression） | `SymbolicCoord` + 关联多项式 | FORM 无几何约束概念 |
| 多线程（TFORM） | 尚未实现 | 优先级低于磁盘后端 |
| MPI 分布式（ParFORM） | 尚未实现 | 长期规划 |

---

## 附录 B：磁盘阈值选择指南

| 场景 | 推荐阈值 | 理由 |
|------|---------|------|
| 交互式几何编辑（<100 项） | 永远内存模式 | 低级延迟要求，不需要磁盘 I/O |
| 命题证明中的中间计算（<10,000 项） | 内存模式（阈值 50MB） | 大部分证明的中间多项式很小 |
| Groebner 基计算（10,000-1,000,000 项） | 自动切换（阈值 100MB） | 大部分情况在内存中，极端情况切换到磁盘 |
| 消元理想计算（>1,000,000 项） | 始终磁盘模式 | 中间结果天然超过可用内存 |

---

> **文档结束**  
> 本文档详述了 FORM 的"磁盘即内存"极端性能哲学如何指导 Lv-00 的大规模符号计算场景优化。核心结论：引入 `TermStream` 项流抽象层作为内存/磁盘透明切换的桥梁，借鉴 FORM 的外部排序和多路归并算法来处理超过物理内存的多项式系统，将 Lv-00 的 Groebner 基计算能力从"度数<=2 有限内存"扩展到"任意度数磁盘后端"。
