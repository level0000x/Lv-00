# Lv-00 几何元语言系统 v3.0 - 模块文档索引

## 概述

Lv-00 是一门以几何为唯一载体的双模数学元语言。几何体本身是计算的执行者、数据的承载者、证明的见证者。

**核心特性**：
- **几何即符号**：点、线、区域本身就是程序的实体
- **数形不二**：数值只能是几何量
- **构造即证明**：构造是否构成证明取决于合一检查
- **公理中立**：内核不内建距离、角度概念
- **可演进**：公理系统可升级，定理可固化为新规则

## 文档结构

本文档目录包含 Lv-00 v3.0 全部核心模块的详细描述：

### 核心模块

| 文档 | 模块 | 功能描述 |
|------|------|----------|
| [01_symbolic_coord.md](01_symbolic_coord.md) | 符号坐标系统 | 有理数、代数数、二次根式、超越常数的精确表示和运算 |
| [02_constraint_graph.md](02_constraint_graph.md) | 约束图核心 | 点、线段、区域、端口、函数块节点及约束管理 |
| [03_normalization.md](03_normalization.md) | 图规范化遍引擎 | 点合并、线段合并、区域合并、幂等性保证 |
| [04_solver.md](04_solver.md) | 符号代数求解器 | 代数方程转化、Gröbner基求解、多解处理 |
| [05_rewrite.md](05_rewrite.md) | 图重写引擎 | 匹配算法、替换操作、循环检测 |
| [06_unify.md](06_unify.md) | 合一检查 | 端口匹配、约束匹配、坐标判等 |

### 高级功能

| 文档 | 模块 | 功能描述 |
|------|------|----------|
| [07_func_block.md](07_func_block.md) | 函数块系统 | 打包操作、确定性检查、多解选择器、组合子 |
| [08_type_system.md](08_type_system.md) | 类型系统 | 宇宙层级、类型等价检查、类型推断 |
| [09_proof.md](09_proof.md) | 命题与证明系统 | 命题模式、证明导航器、爆炸原理 |
| [10_recursion.md](10_recursion.md) | 递归与条件 | 测度系统、选择器块、递归深度监控 |

## 模块依赖关系

```
符号坐标系统 (01)
    ↓
约束图核心 (02)
    ↓
    ├─→ 图规范化遍引擎 (03)
    ├─→ 符号代数求解器 (04)
    ├─→ 图重写引擎 (05)
    └─→ 合一检查 (06)
            ↓
    ├─→ 函数块系统 (07)
    ├─→ 类型系统 (08)
    ├─→ 命题与证明系统 (09)
    └─→ 递归与条件 (10)
```

## 快速参考

### 信任颜色图例

| 颜色 | 含义 |
|------|------|
| 🟢 绿色 | 全构造，无任何非常规依赖 |
| 🔵 蓝色 | 待完成的证明义务（未探索/资源受限/超出范围） |
| 🟢 绿色实框 | 已证不可构造 |
| 🟡 黄色虚线框 | 条件性不可构造 |
| 🟠 浅橙色实心 | 依赖非构造性 oracle |
| 🟠 浅橙色虚线 | 爆炸原理步骤 |
| 🟡 橙黄色 | 含数值假设 |
| 🟠 深橙色 | 非构造性依赖与数值假设叠加 |

### 核心 API 速查

**引擎初始化**:
```c
LV00Engine *engine = engine_create();
engine_load_axiom_package(engine, "euclidean.lvz");
```

**创建几何体**:
```c
// 创建点
AddNodeResult result = graph_add_point(graph, x, y);
int point_id = graph->next_node_id - 1;

// 创建线段
int endpoints[] = {point1_id, point2_id};
result = graph_add_line_segment(graph, endpoints, 2);
```

**添加约束**:
```c
int participants[] = {point_id, line_id};
graph_add_constraint(graph, CONSTRAINT_INCIDENCE, participants, 2);
```

**打包函数块**:
```c
FuncBlock *fb;
PackResult result = func_block_pack(
    graph,
    internal_nodes, 2,
    input_ports, 1,
    output_ports, 1,
    NULL, 0,
    &fb
);
```

**合一检查**:
```c
Proposition *prop = proposition_create(1, PROPOSITION_ATOMIC);
proposition_set_pattern(prop, pattern_graph);

UnifyResult result = proof_unify(construction_graph, prop, true);
if (result == UNIFY_OK) {
    printf("命题得证！\n");
}
```

## 文件结构

```
Lv-00/
├── include/lv00/          # 头文件
│   ├── lv00.h             # 主头文件
│   ├── symbolic_coord.h   # 符号坐标
│   ├── constraint_graph.h # 约束图
│   ├── normalization.h    # 规范化
│   ├── solver.h           # 求解器
│   ├── rewrite.h          # 重写引擎
│   ├── unify.h            # 合一检查
│   ├── axiom_pkg.h        # 公理包
│   ├── module.h           # 模块系统
│   ├── func_block.h       # 函数块
│   ├── type_system.h      # 类型系统
│   ├── proof.h            # 证明系统
│   ├── recursion.h        # 递归与条件
│   ├── engine.h           # 主引擎
│   └── debug.h            # 调试工具
├── src/                   # 源文件
├── tests/                 # 测试文件
├── docs/                  # 文档目录（本目录）
│   ├── README.md          # 本文档
│   ├── 01_symbolic_coord.md
│   ├── 02_constraint_graph.md
│   ├── 03_normalization.md
│   ├── 04_solver.md
│   ├── 05_rewrite.md
│   ├── 06_unify.md
│   ├── 07_func_block.md
│   ├── 08_type_system.md
│   ├── 09_proof.md
│   └── 10_recursion.md
└── CMakeLists.txt         # 构建配置
```

## 编译说明

### 依赖

- C11 编译器
- GMP 库 (GNU Multiple Precision Arithmetic Library)

### 编译步骤

```bash
mkdir build && cd build
cmake ..
make
```

### 运行测试

```bash
./test_basic
./test_comprehensive
```

## 设计原则

### 公理中立

内核只提供关联几何引擎（点、线、区域的关联、之间、相交、包含、连接关系），不内建距离、角度概念。欧氏几何、双曲几何、集合论、类型论全都是可加载的"公理系统包"。

### 信任边界

- **第一可信基**：C内核（关联几何引擎、符号坐标系统、求解器、重写引擎）
- **第二可信基**：核心公理包的安全重写规则集（需外部汇合性证明）

### 自举路线

1. C实现内核
2. 公理系统编辑器
3. 微自举A：几何可表达性验证
4. 微自举B：几何证明能力验证
5. 可行性原型：λ-演算几何表示
6. 首次自举：命题逻辑验证器

## 版本历史

- **v3.0.0** (当前版本)
  - 完整实现18个核心模块
  - 函数块系统：打包、例化、确定性检查
  - 类型系统：宇宙层级、类型等价检查
  - 命题与证明系统：合一检查、证明导航器
  - 递归与条件：测度系统、选择器块

## 许可证

待定

## 参考文献

- 设计文档：`设计.txt`
- 规划文档：`规划.txt`
- 系统描述：`Lv-00系统描述文档.md`
