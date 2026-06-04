# Phase 1: 元表示层设计规范

> **阶段**: Phase 1 - 元表示层  
> **版本**: 1.0.0-draft  
> **日期**: 2026-05-29  
> **依赖**: Phase 0 完成（13 原语、DSL 语法、测试框架、结构体审查）

---

## 一、概述

Phase 1 的目标是实现**元表示层**：用几何构造完整编码 C 核的所有数据结构。这是自举的核心——让几何层"理解"自身的实现。

### 1.1 核心思想

```
C 结构体 → 几何隐喻 → 几何实体 → 约束图

例如：
ConstraintGraph 结构体 → 几何区域（REGION）
  - nodes 字段 → 区域内的点集合
  - constraints 字段 → 点之间的连线（约束）
  - node_count 字段 → 区域内点的数量标注
```

### 1.2 与自举设计文档的关系

本文档是 `self_bootstrapping_design.md` 中 Phase 1 的具体实现规范，将"阶段一：元表示层"的抽象规划转化为可执行的工程方案。

---

## 二、几何编码方案

### 2.1 核心映射表

| C 结构体 | 几何隐喻 | 几何实体类型 | 编码细节 |
|---------|---------|-------------|---------|
| `ConstraintGraph` | 画布/区域 | `REGION` | 边界框表示图的边界，内部点表示节点 |
| `GeomNode` | 几何点 | `POINT` | 坐标表示节点属性（类型、ID） |
| `Constraint` | 连线/关联 | `LINE` + `INCIDENCE` | 线段连接两个点，表示约束关系 |
| `FuncBlock` | 子区域/函数块 | `REGION` (嵌套) | 嵌套区域表示函数块封装 |
| `TypeRegion` | 类型空间 | `REGION` + 标注 | 区域属性表示类型种类 |
| `Proposition` | 命题节点 | `POINT` + 标签 | 特殊标记的点 |
| `ProofStep` | 证明链 | `POLYLINE` | 折线表示证明步骤序列 |
| `SymbolicCoord` | 坐标点 | `POINT` | 精确坐标表示符号值 |

### 2.2 详细编码规范

#### 2.2.1 ConstraintGraph 编码

```
ConstraintGraph 的几何表示：

┌─────────────────────────────────────┐
│  REGION (画布)                       │
│  ┌─────────────────────────────┐    │
│  │  边界框表示图的容量边界      │    │
│  │                             │    │
│  │   ●───●      ●              │    │
│  │   │   │     /               │    │
│  │   ●───●    ●────●           │    │
│  │                             │    │
│  │  点 = GeomNode              │    │
│  │  线 = Constraint            │    │
│  │                             │    │
│  │  标注：node_count=5         │    │
│  │  标注：constraint_count=4   │    │
│  └─────────────────────────────┘    │
└─────────────────────────────────────┘
```

**编码细节**：
- 外部区域表示 `ConstraintGraph` 本身
- 内部点表示 `nodes` 数组元素
- 点之间的连线表示 `constraints` 数组元素
- 区域属性标注存储元数据（count、capacity 等）

#### 2.2.2 GeomNode 编码

```
GeomNode 的几何表示：

POINT (节点)
├── 坐标 (x, y) 编码节点类型和 ID
│   ├── x = node_id (整数坐标)
│   └── y = geom_type (枚举值映射)
├── 标签 = 节点名称（可选）
└── 关联约束 = 连线到其他点

坐标编码方案：
- x 坐标 = node_id * 10 (留出空间给子节点)
- y 坐标 = GeomType 枚举值 * 100
  - POINT = 100
  - LINE = 200
  - CIRCLE = 300
  - ...
```

#### 2.2.3 FuncBlock 编码

```
FuncBlock 的几何表示：

REGION (函数块)
├── 边界框 (表示函数块作用域)
├── 输入端口 (边界上的点)
│   └── 标注 "input_0", "input_1", ...
├── 输出端口 (边界上的点)
│   └── 标注 "output_0", "output_1", ...
├── 内部节点 (区域内的点)
│   └── 表示函数块的内部构造
└── 版本标注
    └── "v5.0.0"
```

---

## 三、元表示层架构

### 3.1 模块划分

```
meta_representation/
├── include/
│   └── meta_repr.h           # 元表示公共接口
├── src/
│   ├── meta_repr.c           # 核心实现
│   ├── graph_encoder.c       # ConstraintGraph 编码器
│   ├── node_encoder.c        # GeomNode 编码器
│   ├── constraint_encoder.c  # Constraint 编码器
│   ├── func_block_encoder.c  # FuncBlock 编码器
│   ├── type_encoder.c        # TypeRegion 编码器
│   ├── proposition_encoder.c # Proposition 编码器
│   └── decoder.c             # 解码器（几何→C结构体）
└── tests/
    ├── test_graph_encoding.c
    ├── test_roundtrip.c
    └── test_meta_repr.c
```

### 3.2 核心 API

```c
/* ========== 编码 API ========== */

/**
 * @brief 将 ConstraintGraph 编码为几何表示
 * @param graph 约束图
 * @return 几何表示的约束图（新的 ConstraintGraph 实例）
 */
ConstraintGraph *meta_repr_encode_graph(const ConstraintGraph *graph);

/**
 * @brief 将 GeomNode 编码为几何点
 * @param node 几何节点
 * @return 编码后的点坐标和属性
 */
GeomNode *meta_repr_encode_node(const GeomNode *node);

/**
 * @brief 将 FuncBlock 编码为几何区域
 * @param block 函数块
 * @return 编码后的区域
 */
GeomNode *meta_repr_encode_func_block(const FuncBlock *block);

/* ========== 解码 API ========== */

/**
 * @brief 从几何表示解码为 ConstraintGraph
 * @param encoded_graph 编码后的几何表示
 * @return 解码后的约束图
 */
ConstraintGraph *meta_repr_decode_graph(const ConstraintGraph *encoded_graph);

/**
 * @brief 从几何点解码为 GeomNode
 * @param encoded_node 编码后的点
 * @return 解码后的节点
 */
GeomNode *meta_repr_decode_node(const GeomNode *encoded_node);

/* ========== 验证 API ========== */

/**
 * @brief 验证编码-解码往返正确性
 * @param original 原始结构体
 * @param decoded 解码后的结构体
 * @return 是否等价
 */
bool meta_repr_verify_roundtrip(const void *original, const void *decoded);

/**
 * @brief 比较两个几何表示是否同构
 * @param a 几何表示 A
 * @param b 几何表示 B
 * @return 是否同构等价
 */
bool meta_repr_isomorphic(const ConstraintGraph *a, const ConstraintGraph *b);
```

---

## 四、实现路线图

### Week 1: 基础编码器

- [ ] 实现 `meta_repr_encode_node()` - 节点编码
- [ ] 实现 `meta_repr_encode_constraint()` - 约束编码
- [ ] 实现 `meta_repr_encode_graph()` - 图编码（基础版）
- [ ] 单元测试：编码正确性

### Week 2: 解码器与往返验证

- [ ] 实现 `meta_repr_decode_node()` - 节点解码
- [ ] 实现 `meta_repr_decode_graph()` - 图解码
- [ ] 实现 `meta_repr_verify_roundtrip()` - 往返验证
- [ ] 单元测试：往返正确性

### Week 3: 复杂结构体编码

- [ ] 实现 `meta_repr_encode_func_block()` - 函数块编码
- [ ] 实现 `meta_repr_encode_type_region()` - 类型区域编码
- [ ] 实现 `meta_repr_encode_proposition()` - 命题编码
- [ ] 单元测试：复杂结构体

### Week 4: 同构验证与优化

- [ ] 实现 `meta_repr_isomorphic()` - 同构比较
- [ ] 性能优化：缓存、增量编码
- [ ] 集成测试：完整流程
- [ ] 文档完善

---

## 五、编码示例

### 示例 1: 简单图编码

```c
/* 原始 C 结构体 */
ConstraintGraph *graph = graph_create();
int p1 = graph_add_point(graph, NULL, 0);
int p2 = graph_add_point(graph, NULL, 0);
int l1 = graph_add_line_segment(graph, p1, p2);

/* 编码为几何表示 */
ConstraintGraph *encoded = meta_repr_encode_graph(graph);

/* 编码后的几何表示包含：
 * - 外部区域（表示 ConstraintGraph）
 * - 两个内部点（表示 p1, p2）
 * - 一条连线（表示 l1）
 * - 标注：node_count=2, constraint_count=1
 */
```

### 示例 2: 函数块编码

```c
/* 原始函数块 */
FuncBlock *block = func_block_create(1);
func_block_set_name(block, "midpoint");
func_block_set_input_ports(block, (int[]){0, 1}, 2);
func_block_set_output_ports(block, (int[]){2}, 1);

/* 编码为几何区域 */
GeomNode *encoded = meta_repr_encode_func_block(block);

/* 编码后的几何表示包含：
 * - 嵌套区域（表示函数块）
 * - 边界上的输入端口点
 * - 边界上的输出端口点
 * - 内部构造点
 * - 标注：name="midpoint", version="5.0.0"
 */
```

---

## 六、与 13 个最小原语的关系

元表示层的实现直接支持 13 个最小原语中的以下原语：

| 原语 | 元表示层支持 |
|-----|-------------|
| `geo-create-node` | 编码器生成几何点 |
| `geo-create-constraint` | 编码器生成几何连线 |
| `geo-pack` | 函数块编码为嵌套区域 |
| `geo-serialize` | 几何表示序列化 |
| `geo-deserialize` | 几何表示反序列化 |
| `geo-query` | 几何表示查询 |

---

## 七、成功标准

| 标准 | 度量方法 | 目标 |
|-----|---------|------|
| 编码正确性 | 编码后几何表示与原始结构体语义一致 | 100% |
| 解码正确性 | 解码后结构体与原始结构体等价 | 100% |
| 往返正确性 | 编码→解码→比较，结果等价 | 100% |
| 同构检测 | 相同结构的不同实例识别为同构 | 100% |
| 性能 | 编码 1000 节点图的时间 | < 100ms |

---

## 八、参考文档

- [自举架构设计](self_bootstrapping_design.md)
- [13 个最小原语](geometric_primitives.md)
- [结构体审查报告](struct_normalization_review.md)
- [函数块系统](07_func_block.md)
- [类型系统](08_type_system.md)