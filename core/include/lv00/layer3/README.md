# Layer 3: Geometry & Topology

几何原语、约束图、拓扑关系的抽象层。

## 模块说明

### 核心概念
- **约束图** (`constraint_graph.h`) — 几何对象与约束的图表示
- **符号坐标** (`symbolic_coord.h`) — 参数化几何表示
- **欧几里得几何** (`euclidean_geometry.h`) — 基础几何对象
- **范式化** (`normalization.h`) — 约束与表达式的标准化
- **传播** (`propagation.h`) — 约束传播机制
- **等价类** (`equiv_class.h`) — 等价关系管理

### 辅助模块
- 高维几何 (`high_dim.h`)
- 几何压缩 (`geometry_compress.h`)
- 几何变换 (`geometry_transform.h`)
- 稀疏线性代数 (`sparse_linear_algebra.h`)
- 多项式 (`mpz_poly.h`)

## 依赖关系

```
     Layer 3 (Geometry)
          ↓ 依赖
     Layer 2 (Resource)
```

**上层依赖者**: Layer 4 (推理), Layer 5 (输出), Layer 6 (可视化)

## 公开 API vs 内部实现

### 公开 API
```c
#include <lv00/layer3/constraint_graph.h>
#include <lv00/layer3/symbolic_coord.h>
#include <lv00/layer3/euclidean_geometry.h>
```

### 内部实现（标记）
- `geometry_types.h` — 仅供本层内部使用
- `geo_utils.h` — 内部工具函数

## 使用指南

### 应该直接使用 Layer 3
- 需要定义几何约束的上层模块
- 几何算法的实现
- 约束验证与检查

### 应该通过 Layer 4 间接使用
- 求解约束系统
- 证明几何定理
- 优化几何配置

## 设计原则

1. **表示清晰** — 几何对象有明确的数学定义
2. **约束表达** — 支持多种约束类型（等距、平行、垂直等）
3. **符号计算** — 保留参数性以支持高层推理
4. **扩展性** — 易于添加新的几何原语

## 文件清单

```
core/include/lv00/layer3/
├── constraint_graph.h
├── symbolic_coord.h
├── euclidean_geometry.h
├── normalization.h
├── propagation.h
├── equiv_class.h
├── high_dim.h
├── geometry_compress.h
├── geometry_transform.h
├── sparse_linear_algebra.h
├── mpz_poly.h
└── ...

core/include/lv00/internal/
├── geometry_types.h       # 内部类型定义
└── geo_utils.h            # 内部工具函数
```

## 维护指南

- Layer 3 应保持**稳定的 API**
- 不应依赖 Layer 4 及以上的功能
- 所有新增功能应文档化
- 更新需考虑对 Layer 4+ 的影响
