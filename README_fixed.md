# Lv-00 几何元语言

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Version](https://img.shields.io/badge/version-1.1.0-blue.svg)](CHANGELOG.md)
[![CI](https://github.com/level0000x/Lv-00/actions/workflows/ci.yml/badge.svg)](https://github.com/level0000x/Lv-00/actions/workflows/ci.yml)
[![C11](https://img.shields.io/badge/C-11-green.svg)](https://en.cppreference.com/w/c)
[![Lean4](https://img.shields.io/badge/Lean-4-purple.svg)](https://lean-lang.org/)

> **Lv-00 是一种尝试将几何构造、代数计算、逻辑证明统一于同一语法体系的形式化元语言。当前版本为 1.1.0，采用严格的 C11 标准实现，并在 Lean 4 中进行形式化验证。**

---

## 目录

- [项目背景](#项目背景)
- [设计目标](#设计目标)
- [核心概念](#核心概念)
- [快速开始](#快速开始)
- [使用示例](#使用示例)
- [系统架构](#系统架构)
- [项目结构](#项目结构)
- [文档](#文档)
- [技术实现](#技术实现)
- [相关工作](#相关工作)
- [开发路线图](#开发路线图)
- [参与贡献](#参与贡献)
- [许可证](#许可证)

---

## 项目背景

### 研究现状

几何计算、符号推理、形式化证明是数学机械化领域的三个重要方向。当前，这三个方向各自发展出相对成熟的工具链：

| 领域 | 典型系统 | 主要功能 |
|-----|---------|---------|
| **几何作图** | GeoGebra、CAD 系统 | 交互式几何建模 |
| **代数计算** | SymPy、MATLAB | 符号与数值计算 |
| **形式证明** | Lean、Coq | 形式化定理证明 |

这些系统在各自的领域内功能完善，但在**跨领域协作**时存在语义断层：几何模型难以直接用于形式证明，符号计算结果难以追溯几何意义。

### 现有尝试

学术界已有若干跨领域融合的探索：

- **GeoCoq**：在 Coq 中形式化平面几何公理体系
- **LeanGeo**：将几何对象嵌入 Lean 类型系统
- **AlphaGeometry**：将几何求解与符号推理结合
- **JGEX**：多策略几何证明引擎

这些工作表明，**在统一框架下整合几何构造、计算与证明是可行且有价值的探索方向**。

---

## 设计目标

Lv-00 项目的目标是探索一种**领域特定形式化语言**，尝试实现以下设计原则：

### 1. 语义统一性

几何对象（点、线、圆等）同时作为：
- 程序的执行实体
- 代数计算的操作数
- 逻辑命题的证明对象

### 2. 精确性优先

采用符号计算而非浮点计算：
- 有理数运算基于 GMP 库
- 代数数使用符号表达式树
- 避免数值精度损失

### 3. 十层层级化架构

采用十层单向依赖架构：
- 层间通过稳定数据结构通信
- 禁止反向依赖
- 支持编译时 `_Static_assert` 边界检查（ENABLE_LAYER_VALIDATION）

### 4. 模块化扩展

- 预设模块系统支持领域扩展
- 公理包支持版本化管理
- 函数块支持组合复用

---

## 核心概念

### 1. 符号坐标 (Symbolic Coordinate)

与浮点数不同，Lv-00 使用精确的符号表示：

```c
// 浮点数：存储近似值
double x = 1.0 / 3.0;  // 存储为 0.3333333333333333

// 有理数：存储精确分数
Expr *x = rational(1, 3);  // 存储为 1/3

// 代数数：存储符号表达式
Expr *sqrt2 = algebraic_sqrt(integer(2));  // 存储为 √2
```

### 2. 约束图 (Constraint Graph)

几何对象及其约束关系以图结构存储：

```
节点: 几何对象 (点、线、圆等)
边:   约束关系 (距离、角度、共线等)

    A ───[d=5]─── B
    │             │
   [θ=60°]      [θ=60°]
    │             │
    └────[d=5]────┘
           C

约束图可推理得出：三角形 ABC 各边相等
```

### 3. 函数块 (Function Block)

可复用、可组合的计算单元：

```c
// 定义中点函数
FuncBlock *midpoint = lv00_fb_create("midpoint", 2);
lv00_fb_define(midpoint, "return point((A.x+B.x)/2, (A.y+B.y)/2);");

// 实例化
Point *M = lv00_fb_apply(midpoint, A, B);
```

### 4. 证明对象 (Proof Object)

证明结果以结构化数据存储：

```c
Proposition *prop = lv00_proposition_eq(distance(A,B), rational(5,1));
Proof *proof = lv00_prove(ctx, prop);

// 支持导出为 Lean 证明脚本
lv00_proof_export_lean(proof, "proof.lean");
```

---

## 快速开始

### 环境依赖

| 依赖 | 版本要求 | 说明 |
|-----|---------|------|
| CMake | ≥ 3.15 | 构建系统 |
| GMP | ≥ 6.0 | 大整数运算 |
| C 编译器 | C11 | GCC/Clang/MSVC |

### 安装依赖

```bash
# Ubuntu/Debian
sudo apt-get install cmake libgmp-dev

# macOS
brew install cmake gmp

# Windows (MSYS2)
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-gmp
```

### 构建

```bash
git clone https://github.com/level0000x/Lv-00.git
cd Lv-00
mkdir build && cd build
cmake ..
cmake --build .
```

### 测试

```bash
ctest --output-on-failure
```

---

## 使用示例

### 示例 1：基础几何计算

```c
#include "lv00.h"
#include <stdio.h>

int main() {
    LV00Context *ctx = lv00_context_create();
    
    // 定义两点
    Point *A = lv00_point(ctx, rational(0, 1), rational(0, 1));
    Point *B = lv00_point(ctx, rational(3, 1), rational(4, 1));
    
    // 创建线段并计算距离
    Segment *AB = lv00_segment(ctx, A, B);
    Expr *length = lv00_distance(ctx, A, B);
    
    printf("AB = ");
    lv00_expr_print(length);  // 输出: 5
    
    lv00_context_destroy(ctx);
    return 0;
}
```

### 示例 2：等边三角形验证

```c
#include "lv00.h"

int main() {
    LV00Context *ctx = lv00_context_create();
    
    // 构造等边三角形
    Point *A = lv00_point(ctx, rational(0, 1), rational(0, 1));
    Point *B = lv00_point(ctx, rational(2, 1), rational(0, 1));
    
    // 以 AB 为底，构造等边三角形
    Circle *c1 = lv00_circle(ctx, A, B);
    Circle *c2 = lv00_circle(ctx, B, A);
    Point *C = lv00_intersection(ctx, c1, c2);
    
    // 验证三边相等
    Expr *ab = lv00_distance(ctx, A, B);
    Expr *bc = lv00_distance(ctx, B, C);
    Expr *ca = lv00_distance(ctx, C, A);
    
    Proposition *eq1 = lv00_proposition_eq(ab, bc);
    Proposition *eq2 = lv00_proposition_eq(bc, ca);
    
    Proof *proof1 = lv00_prove(ctx, eq1);
    Proof *proof2 = lv00_prove(ctx, eq2);
    
    printf("Equilateral: %s\n",
           lv00_proof_valid(proof1) && lv00_proof_valid(proof2) ? "YES" : "NO");
    
    lv00_context_destroy(ctx);
    return 0;
}
```

### 示例 3：使用预设模块

```c
#include "lv00.h"
#include "lv00/preset_euclidean_geometry.h"

int main() {
    LV00Context *ctx = lv00_context_create();
    
    // 加载欧氏几何预设
    lv00_preset_load(ctx, "euclidean_geometry");
    
    // 3-4-5 直角三角形
    Point *A = lv00_point(ctx, rational(0, 1), rational(0, 1));
    Point *B = lv00_point(ctx, rational(3, 1), rational(0, 1));
    Point *C = lv00_point(ctx, rational(0, 1), rational(4, 1));
    
    // 应用勾股定理预设
    Proposition *prop = lv00_preset_apply(ctx, "pythagorean_theorem", A, B, C);
    Proof *proof = lv00_prove(ctx, prop);
    
    lv00_proof_export_lean(proof, "pythagorean.lean");
    
    lv00_context_destroy(ctx);
    return 0;
}
```

---

## 系统架构

### 十层单向依赖架构

```
┌─────────────────────────────────────────────────────────────┐
│ Layer 10 │ Interop        │ Coq/Lean4/OPML 双向桥接         │
│ Layer 9  │ Application    │ 批处理、交互式 REPL             │
│ Layer 8  │ Meta-Verify    │ 类型一致性、完备性、可靠性检查   │
│ Layer 7  │ Orchestration  │ 六阶段流水线编排                │
│ Layer 6  │ Visual         │ 节点图编辑器、几何画布、块调度   │
│ Layer 5  │ Output         │ 流事件、TikZ/Lean 导出、插件     │
│ Layer 4  │ Reasoning      │ 证明引擎、Groebner、SMT/SAT    │
│ Layer 3  │ Geometry       │ 约束图、符号坐标、代数数         │
│ Layer 2  │ Resource       │ 内存池、缓存、上下文、调试       │
│ Layer 1  │ Parser         │ 公式解析、DSL 编译              │
└─────────────────────────────────────────────────────────────┘

shared 层：基础类型、错误码、内存管理、日志（所有层共享）
```

| 层级 | 名称 | 可依赖项 |
|:----:|:-----|:---------|
| L1 | Parser | L2 |
| L2 | Resource | 无（基础层）|
| L3 | Geometry | L2 |
| L4 | Reasoning | L2, L3 |
| L5 | Output | L2, L3, L4 |
| L6 | Visual | L2, L3, L5 |
| L7 | Orchestration | L2-L6 |
| L8 | Meta-Verification | L2, L3, L4 |
| L9 | Application | 所有层 |
| L10 | Interop | L2, L4, L5 |

### 架构约束

- **单向依赖**：上层依赖下层，下层不得反向依赖
- **稳定契约**：层间通过 AST、Typed IR、Proof Object 等稳定结构通信
- **可验证性**：支持编译时层级边界检查（ENABLE_LAYER_VALIDATION）

---

## 项目结构

```
Lv-00/
├── core/                          # 核心引擎
│   ├── include/lv00/              # 公共 API 头文件 (170+)
│   │   ├── lv00.h                 # 主头文件
│   │   ├── symbolic_coord.h       # 符号坐标
│   │   ├── constraint_graph.h     # 约束图
│   │   ├── proof.h                # 证明系统
│   │   ├── func_block.h           # 函数块系统
│   │   └── preset_*.h             # 55+ 数学理论预设
│   └── src/                       # 十层实现
│       ├── layer1_parser/         # 词法语法解析
│       ├── layer2_resource/       # 资源管理
│       ├── layer3_geometry/       # 约束求解
│       ├── layer4_reasoning/      # 推理引擎
│       ├── layer5_output/         # 输出与导出
│       ├── layer6_visual/         # 可视化编程
│       ├── layer7_orchestration/  # 流水线编排
│       ├── layer8_meta_verify/    # 元验证
│       ├── layer9_application/    # 应用入口
│       └── layer10_interop/       # 跨语言桥接
├── formal/                        # Lean 4 形式化验证
│   ├── Lv00Formal.lean            # 核心形式化
│   └── Lv00Formal/                # 理论验证模块
├── module/                        # 扩展模块
│   ├── axiom_packages/            # 公理包库 (.lvz)
│   ├── python/                    # Python 绑定与 DSL
│   └── stream_bridge/             # 流式事件桥接
├── doc/                           # 文档
│   ├── DOCUMENTATION.md           # 完整技术文档 (26章)
│   ├── docs/                      # 技术文档
│   └── reports/                   # 历史汇报
├── examples/                      # 用法示例与模板
├── web/                           # Web GUI 与流监视器
├── test/                          # 测试套件
└── README.md                      # 本文件
```

---

## 文档

### 入门文档
- [完整技术文档 v5.0](doc/DOCUMENTATION.md) - **最新**：26 章全面技术手册
- [API 快速入门](doc/docs/API_QUICKSTART.md) - API 使用指南
- [教程](doc/docs/TUTORIAL.md) - 逐步入门教程
- [语言规范](doc/docs/LV00_LANGUAGE_SPEC.md) - 语法与语义定义
- [架构手册](doc/docs/ARCHITECTURE_MANUAL.md) - 十层架构设计详解

### 开发文档
- [贡献指南](CONTRIBUTING.md) - 贡献流程
- [提交规范](COMMIT_CONVENTION.md) - Git 提交规范
<!-- - [编码规范](doc/docs/CODING_STANDARD_v3.4.2.md) - 代码风格 -->

### 参考文档
- [CHANGELOG.md](CHANGELOG.md) - 版本变更记录
- [VERSION_LOG.md](VERSION_LOG.md) - 版本迭代日志
- [竞品分析](doc/docs/competitive_analysis.md) - 22 个参考项目分析

---

## 技术实现

### 数值系统

| 类型 | 实现方式 | 精度 |
|-----|---------|------|
| 有理数 | GMP mpq_t | 任意精度 |
| 代数数 | 最小多项式 + 隔离区间 | 符号精确 |
| 符号坐标 | 参数化表达式 + 信任颜色 | 无截断误差 |

### 求解引擎

- **Groebner 基**：并行 Buchberger 算法，支持工作窃取调度
- **SMT 后端**：一阶逻辑求解（Z3、CVC5 集成）
- **ATP 接口**：自动定理证明器集成
- **SAT/BDD**：CDCL SAT 求解器、BDD ADD 操作
- **整数规划**：不等式推理与 SOS 分解
- **数值后端**：GMRES(m=30)、BiCGSTAB、共轭梯度法

### 证明系统

- **多策略引擎**：向量法、全角法、演绎数据库、坐标法、面积法、Oracle 法
- **搜索算法**：DFS、BFS、最佳优先、MCTS（UCB1）
- **约束传播**：WFC 风格弧相容算法
- **证明导出**：Lean/Coq 脚本生成、OPML 格式导出

### 可视化与交互

- **节点图编辑器**：Fruchterman-Reingold 力导向布局
- **几何画布**：SVG 渲染、自适应视图
- **块调度器**：Kahn 拓扑排序 + 增量脏块执行
- **四视图同步**：文本 ↔ 节点图 ↔ 块图 ↔ 几何画布

### 渲染后端

- SVG、Cairo 脚本、Three.js HTML、TikZ LaTeX、PPM 光栅化

### 形式化验证

- **Lean 4 形式化**：Hilbert 公理体系、约束图规范化证明、双向桥接
- **Coq 桥接**：Vernacular 生成/导入、38 种 tactic 验证
- **元验证**：类型一致性、完备性、可靠性、非平凡性、往返性五维检查

---

## 相关工作

### 几何形式化

| 项目 | 机构/作者 | 技术路线 |
|-----|---------|---------|
| GeoCoq | 巴黎第十一大学 | 在 Coq 中形式化 Tarski 公理 |
| LeanGeo | A. Humenberger 等 | Lean 4 几何库 |
| Euklides | Budapest 等 | 动态几何证明 |

### 几何 AI

| 项目 | 机构 | 特点 |
|-----|------|-----|
| AlphaGeometry | Google DeepMind | 符号推理 + 神经网络 |
| DDAR | - | 几何自动作图 |

### 约束求解

| 项目 | 应用领域 | 特点 |
|-----|---------|-----|
| CGAL | 几何计算 | 算法库集合 |
| JGEX | 几何证明 | 多策略引擎 |

---

## 开发路线图

### v1.1.0（当前版本）

已完成功能：
- 十层单向依赖架构源码迁移
- 六阶段流水线编排系统
- 元验证五维检查（类型一致性、完备性、可靠性、非平凡性、往返性）
- 可视化编程环境（节点图/几何画布/块调度器、四视图同步）
- 五渲染后端（SVG、Cairo、Three.js、TikZ、PPM）
- Coq/Lean4/OPML 双向桥接
- CDCL SAT 求解器 + BDD ADD + SOS 分解
- GMRES/BiCGSTAB/CG 数值后端
- 55+ 数学理论预设模块
- Lean 4 形式化验证完善

### v1.2.0（近期目标）

规划中功能：
- Python DSL 绑定完善
- WebAssembly 编译目标
- 证明可视化组件
- 约束图增量更新

### v2.0.0（中期目标）

规划中功能：
- Lean 形式化验证完善
- 公理自动发现机制
- 多语言绑定支持

### 长期方向

探索性方向：
- 3D 几何支持
- 微分几何扩展
- 与几何 AI 系统集成

---

## 参与贡献

### 开发流程

```bash
# 1. Fork 仓库
# 2. 克隆本地
git clone https://github.com/YOUR_USERNAME/Lv-00.git

# 3. 创建分支
git checkout -b feature/your-feature

# 4. 开发与测试
# ... 修改代码 ...

# 5. 提交（遵循约定式提交）
git commit -m "feat: 描述变更内容"

# 6. 推送
git push origin feature/your-feature

# 7. 创建 Pull Request
```

### 贡献类型

- **Bug 修复**：代码问题修复
- **功能开发**：新 API 或模块
- **文档完善**：改进文档和示例
- **测试补充**：增加测试覆盖
- **性能优化**：运行效率改进

### 代码规范

- 遵循 C11 标准
- 使用 4 空格缩进
- 函数命名：小写加下划线
- 类型命名：PascalCase
- 宏命名：全大写

详见 [CONTRIBUTING.md](CONTRIBUTING.md) 和 [COMMIT_CONVENTION.md](COMMIT_CONVENTION.md)。

---

## 许可证

[MIT License](LICENSE)

---

## 致谢

- [GMP](https://gmplib.org/) - 任意精度算术支持
- [Lean](https://lean-lang.org/) - 形式化验证框架参考
- [CMake](https://cmake.org/) - 构建系统

---

<div align="center">

**Lv-00** = Level Zero

*探索几何构造与形式化推理的统一表达*

</div>
