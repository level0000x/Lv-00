# Lv-00 几何元语言 - 项目展示

> **唯一将几何构造、计算程序、一阶逻辑证明三者统一于同一语法体系的元语言。**

## 🎯 项目概述

**Lv-00** 是一种**新的品类**——几何元语言（Geometric Meta-Language）。它不是：

- ❌ 供人调用的几何算法包（如 CGAL）
- ❌ 自动解题的 AI 证明器（如 AlphaGeometry）
- ❌ 依附外部证明器的数学库（如 LeanGeo）

**它是**：一种让你同时完成"画图、算数、证明"三件事的语言。几何对象同时是：
- **程序** - 可执行的构造指令
- **数据** - 可序列化的几何表示
- **证明** - 可验证的几何命题

### 生态定位

```
上层应用（CGAL / CAD / AI求解器 / 教育工具）
        ↑ 它们需要精确语义
   Lv-00：几何元语言（提供精确语义）
        ↑ 它们提供形式化基础
底层框架（Lean / Coq / 一阶逻辑 / 约束求解）
```

**一句话：Lv-00 不是现有工具的替代品，而是一种以前不存在的品类。**

## ✨ 核心特性

### 1. 符号坐标系统
- ✅ 有理数、代数数、二次扩域、超越数
- ✅ 任意精度算术（基于GMP）
- ✅ 完整的算术运算和比较

### 2. 约束图系统
- ✅ 点、线段、区域等几何对象
- ✅ 关联、之间、相交等约束关系
- ✅ 自动归一化（合并等价节点）

### 3. 函数块系统
- ✅ 几何构造的函数式抽象
- ✅ 打包、实例化、部分应用
- ✅ 组合子（顺序、并行、选择）

### 4. 证明系统
- ✅ 命题创建和管理
- ✅ 证明导航器
- ✅ 合一检查
- ✅ 爆炸原理（Ex Falso）

### 5. 类型系统
- ✅ 宇宙层级（Universe Levels）
- ✅ 类型等价检查
- ✅ 类型推断

### 6. 递归系统
- ✅ 测度系统（符号/非符号）
- ✅ 递归深度监控
- ✅ 终止检查

## 🏗️ 项目架构

```
Lv-00/
├── 📁 include/lv00/     # 14个头文件
│   ├── lv00.h           # 主头文件
│   ├── symbolic_coord.h # 符号坐标
│   ├── constraint_graph.h
│   └── ...
├── 📁 src/              # 14个源文件
│   ├── symbolic_coord.c
│   ├── constraint_graph.c
│   └── ...
├── 📁 tests/            # 13个测试文件
│   ├── test_basic.c
│   ├── test_symbolic_coord.c
│   └── ...
├── 📁 python/           # Python绑定
│   ├── lv00/           # Python包
│   ├── tests/          # Python测试
│   └── examples/       # Python示例
├── 📁 examples/         # C示例程序
├── 📁 docs/             # 文档
├── 📁 fuzz/             # 模糊测试
├── 📁 scripts/          # 工具脚本
└── 📁 .github/workflows/# 5个CI工作流
```

## 📊 项目统计

| 指标 | 数值 |
|------|------|
| **源代码行数** | ~15,000 行 C 代码 |
| **头文件** | 14 个 |
| **源文件** | 14 个 |
| **测试文件** | 13 个 C 测试 + 2 个 Python 测试 |
| **测试用例** | 100+ 个 |
| **CI/CD 工作流** | 5 个 |
| **示例程序** | 3 个 C + 1 个 Python |
| **文档文件** | 15+ 个 |

## 🧪 测试覆盖

### C 单元测试（13个，全部通过 ✅）

| 测试模块 | 测试数量 | 状态 |
|---------|---------|------|
| test_basic | 基础功能测试 | ✅ |
| test_edge_cases | 边界情况 | ✅ |
| test_complex_graph | 复杂图 | ✅ |
| test_normalization | 归一化 | ✅ |
| test_unify | 合一检查 | ✅ |
| test_func_block | 函数块 | ✅ |
| test_solver | 求解器 | ✅ |
| test_rewrite | 重写系统 | ✅ |
| test_type_system | 类型系统 | ✅ |
| test_proof | 证明系统 | ✅ |
| test_axiom_pkg | 公理包 | ✅ |
| test_module | 模块系统 | ✅ |
| test_recursion | 递归系统 | ✅ |

### Python 测试（2个文件，35+ 测试用例）

- `test_symbolic_coord.py` - 符号坐标测试
- `test_graph.py` - 图操作测试

### CI/CD 工作流（5个）

1. **ci.yml** - 主CI（Linux/macOS/Windows + 覆盖率 + 静态分析）
2. **benchmark.yml** - 性能基准测试
3. **fuzz.yml** - 模糊测试（每周运行）
4. **python.yml** - Python绑定测试（12种组合）

## 🚀 快速开始

### C 语言

```c
#include "lv00.h"

int main() {
    // 创建约束图
    ConstraintGraph *g = graph_create();
    
    // 创建点
    SymbolicCoord *x = symbolic_coord_create_rational(0, 1);
    SymbolicCoord *y = symbolic_coord_create_rational(0, 1);
    SymbolicCoord *coords[] = {x, y};
    graph_add_point(g, coords, 2);
    
    // 创建线段
    graph_add_line_segment(g, 0, 1);
    
    // 归一化
    NormalizationResult *result = graph_normalize(g, false);
    
    // 清理
    graph_destroy(g);
    return 0;
}
```

### Python

```python
from lv00 import Graph, SymbolicCoord
from fractions import Fraction

# 创建图
g = Graph()

# 添加点
p1 = g.add_point(0, 0)
p2 = g.add_point(Fraction(1, 2), Fraction(3, 4))

# 添加线段
g.add_line_segment(p1, p2)

# 归一化
g.normalize()
```

## 📈 性能指标

基于基准测试（Release模式）：

| 操作 | 性能 |
|------|------|
| 有理数加法 | 10,000,000 ops/sec |
| 创建/销毁有理数 | 3,333,333 ops/sec |
| 添加BETWEENNESS约束 | 333,333 ops/sec |
| 归一化重复点 | 200,000 ops/sec |
| 合一检查 | 100,000 ops/sec |

## 🔧 构建系统

### CMake 选项

| 选项 | 说明 | 默认 |
|------|------|------|
| BUILD_TESTS | 构建测试 | ON |
| BUILD_EXAMPLES | 构建示例 | ON |
| BUILD_SHARED_LIBS | 构建共享库 | OFF |
| BUILD_FUZZERS | 构建模糊测试 | OFF |
| ENABLE_COVERAGE | 代码覆盖率 | OFF |
| ENABLE_SANITIZERS | Sanitizers | OFF |

### 支持平台

- ✅ Linux (Ubuntu, Debian, etc.)
- ✅ macOS
- ✅ Windows (MSYS2/MinGW)

## 🛡️ 质量保证

### 静态分析
- cppcheck
- clang-tidy

### 动态分析
- AddressSanitizer
- UndefinedBehaviorSanitizer
- 模糊测试（libFuzzer）

### 代码覆盖率
- lcov/gcov（C代码）
- pytest-cov（Python代码）

## 📚 文档

- [API使用指南](docs/API_USAGE_GUIDE.md)
- [模块文档](docs/) - 11个模块详细设计
- [实现路线图](IMPLEMENTATION_ROADMAP.md)
- [Python示例](python/examples/)

## 🎯 项目亮点

1. **创新设计** - 几何构造同时作为程序、数据和证明
2. **精确计算** - 基于符号坐标的任意精度几何
3. **全面测试** - 100+测试用例，5个CI工作流
4. **多语言支持** - C库 + Python绑定
5. **生产就绪** - 完整的文档、示例、CI/CD

## 🆚 与同类项目的差异化

Lv-00 在生态中占据一个独特的空白地带：

| 维度 | CGAL | AlphaGeometry | LeanGeo | **Lv-00** |
|:---|:---|:---|:---|:---|
| 本质 | 算法库 | AI解题器 | 形式化库 | **元语言** |
| 几何构造 | ✅ | ✅ | ✅ | ✅ |
| 符号计算 | ✅ | ❌ | ❌ | ✅ |
| 逻辑证明 | ❌ | ✅（自动） | ✅（交互） | ✅（合一） |
| 统一语法 | ❌ | ❌ | ❌ | ✅ |
| 公理中立 | ❌ | ❌ | ❌ | ✅ |
| 可演进 | ❌ | ❌ | ❌ | ✅ |

**Lv-00 的独特价值**：不是把三个工具拼在一起，而是让"构造"这件事本身就是程序、就是证明。用户不需要学习三种语法、切换三种工具——一个统一的语法覆盖全部。

## 🔮 未来方向

- [ ] Web可视化界面
- [ ] Jupyter Notebook扩展
- [ ] 更多几何算法（圆、椭圆、贝塞尔曲线）
- [ ] GPU加速
- [ ] 分布式计算支持

## 📄 许可证

MIT License - 详见 [LICENSE](LICENSE)

## 🙏 致谢

- [GMP](https://gmplib.org/) - 任意精度算术
- [CMake](https://cmake.org/) - 构建系统
- [libFuzzer](https://llvm.org/docs/LibFuzzer.html) - 模糊测试

---

**项目状态**: ✅ 生产就绪 (v3.0.0)
