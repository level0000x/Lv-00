# Lv-00 几何元语言

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Version](https://img.shields.io/badge/version-3.5.0-blue.svg?style=flat)](CHANGELOG.md)
[![CI](https://github.com/level0000x/Lv-00/actions/workflows/ci.yml/badge.svg)](https://github.com/level0000x/Lv-00/actions/workflows/ci.yml)

> **Lv-00 是一种尝试将几何构造、代数计算、逻辑证明统一于同一语法体系的形式化元语言。**

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

### 3. 层级化架构

采用五层单向依赖架构：
- 层间通过稳定数据结构通信
- 禁止反向依赖
- 支持编译时边界检查

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

### 五层单向依赖架构

```
┌─────────────────────────────────────────────────────────────┐
│ 第5层：输出证明编译层                                         │
│ 职责：命题格式化、证明链生成、跨语言导出                       │
├─────────────────────────────────────────────────────────────┤
│ 第4层：多策略自动推理层                                       │
│ 职责：正向演绎、反向溯源、代数消元、SMT/ATP 调度              │
├─────────────────────────────────────────────────────────────┤
│ 第3层：约束拓扑规约层                                         │
│ 职责：约束图构建、等价节点化简、拓扑归一化                    │
├─────────────────────────────────────────────────────────────┤
│ 第2层：基础几何公理层                                         │
│ 职责：原始几何本体、基础度量关系、固有公理库                   │
├─────────────────────────────────────────────────────────────┤
│ 第1层：词法语法解析层                                         │
│ 职责：BNF 文法、词法规则、AST/Typed IR 生成                  │
└─────────────────────────────────────────────────────────────┘

shared 层：基础类型、错误码、内存管理、日志（所有层共享）
```

### 架构约束

- **单向依赖**：上层依赖下层，下层不得反向依赖
- **稳定契约**：层间通过 AST、Typed IR、Proof Object 等稳定结构通信
- **可验证性**：支持编译时层级边界检查（ENABLE_LAYER_VALIDATION）

---

## 项目结构

```
Lv-00/
├── core/                          # 核心引擎
│   ├── include/lv00/              # 公共 API 头文件
│   │   ├── lv00.h                 # 主头文件
│   │   ├── symbolic_coord.h       # 符号坐标
│   │   ├── constraint_graph.h     # 约束图
│   │   ├── proof.h                # 证明系统
│   │   └── preset_*.h             # 预设模块
│   ├── src/layer1_parser/         # 词法语法解析
│   ├── src/layer2_resource/       # 资源管理
│   ├── src/layer3_geometry/       # 约束求解
│   ├── src/layer4_reasoning/      # 推理引擎
│   └── src/layer5_output/         # 证明导出
├── lv00-formal/                   # Lean 形式化验证
│   ├── Lv00Formal.lean            # 核心形式化
│   └── Theory/                    # 理论验证模块
├── doc/docs/                      # 技术文档
├── tests/                         # 测试套件
├── module/axiom_packages/         # 公理包库
└── README.md                      # 本文件
```

---

## 文档

### 入门文档
- [API 快速入门](doc/docs/API_QUICKSTART.md) - API 使用指南
- [语言规范](doc/docs/LV00_LANGUAGE_SPEC.md) - 语法与语义定义
- [架构设计](doc/docs/ARCHITECTURE_v3.3.md) - 五层架构规范

### 开发文档
- [贡献指南](CONTRIBUTING.md) - 贡献流程
- [提交规范](COMMIT_CONVENTION.md) - Git 提交规范
- [编码规范](doc/docs/CODING_STANDARD_v3.4.2.md) - 代码风格

### 参考文档
- [CHANGELOG.md](CHANGELOG.md) - 版本变更记录
- [VERSION_LOG.md](VERSION_LOG.md) - 版本迭代日志

---

## 技术实现

### 数值系统

| 类型 | 实现方式 | 精度 |
|-----|---------|------|
| 有理数 | GMP mpq_t | 任意精度 |
| 代数数 | 表达式树 | 符号精确 |
| 符号坐标 | 参数化表达式 | 无截断误差 |

### 求解引擎

- **Groebner 基**：多项式理想求解
- **SMT 后端**：一阶逻辑求解（Z3、CVC5）
- **ATP 接口**：自动定理证明器集成
- **SAT/BDD**：命题逻辑可满足性

### 证明系统

- **多策略引擎**：支持 8 种证明策略
- **约束传播**：WFC 风格弧相容算法
- **证明导出**：Lean/Coq 脚本生成

### 性能优化

- SIMD 友好数值存储
- LRU 对象缓存
- 多核并行调度

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

### v3.5.0（当前版本）

已完成功能：
- 五层架构源码迁移
- 55+ 数学理论预设模块
- 等价类管理器与约束传播引擎
- 元证明系统基础实现

### v3.6.0（近期目标）

规划中功能：
- Python DSL 绑定
- WebAssembly 编译目标
- 证明可视化组件

### v4.0.0（中期目标）

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
