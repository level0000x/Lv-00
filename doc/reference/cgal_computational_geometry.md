# CGAL 计算几何算法库参考文档

> **项目**: CGAL (Computational Geometry Algorithms Library)  
> **链接**: [github.com/CGAL/cgal](https://github.com/CGAL/cgal) | [cgal.org](https://www.cgal.org)  
> **语言**: C++  
> **许可**: GPL/LGPL 双许可  
> **Stars**: 4.5k+  
> **创建日期**: 2026-05-27  
> **适用层级**: Lv-00 第 2 层（基础几何公理层）+ 第 3 层（约束拓扑规约层）

---

## 一、项目概述

CGAL 是计算几何领域的工业级开源算法库，由欧洲多所大学和研究机构联合开发维护超过 25 年。它提供了完整的 2D/3D 几何数据结构和算法实现，涵盖凸包、三角剖分、网格生成、表面重建、布尔运算等核心几何操作。

### 1.1 核心定位

CGAL 的设计目标是"以 C++ 库的形式，提供方便、高效、可靠的几何算法"。其核心创新在于：

- **精确谓词范式（Exact Predicate Paradigm）**：几何判定（如"点是否在三角形内"）使用精确算术，保证结果正确性
- **近似构造策略（Approximate Construction）**：几何构造（如"计算交点坐标"）可使用浮点近似，平衡精度与性能
- **Kernel 抽象层**：通过模板参数化支持多种数值内核（Cartesian/Homogeneous/Exact）

### 1.2 模块架构

CGAL 采用严格的模块化架构，包含 70+ 个独立包：

| 模块类别 | 代表包 | 功能描述 |
|---------|-------|---------|
| **基础内核** | Kernel_23, Kernel_d | 点、向量、线、面等基础几何对象 |
| **三角剖分** | Triangulation_2, Triangulation_3 | 2D/3D Delaunay 三角剖分 |
| **网格生成** | Mesh_2, Mesh_3 | 有限元网格自动生成 |
| **表面处理** | Polygon_mesh_processing | 网格修复、平滑、简化 |
| **布尔运算** | Boolean_set_operations_2 | 2D 多边形布尔运算 |
| **空间搜索** | AABB_tree, Spatial_searching | AABB 树、KD 树空间索引 |
| **数据交换** | Stream_support | OFF/PLY/OBJ 格式读写 |

---

## 二、核心借鉴点

### 2.1 精确谓词范式

CGAL 的核心创新是将几何操作分为两类：

| 操作类型 | 特点 | CGAL 实现 | Lv-00 对应 |
|---------|------|----------|-----------|
| **Predicate（谓词）** | 返回离散结果（是/否），必须精确 | 使用精确算术（MPFR/GMP） | 约束图判定（如"三点共线？"） |
| **Construction（构造）** | 返回几何对象，可近似 | 使用浮点算术 | 几何体构造（如"计算交点坐标"） |

**关键代码模式**：

```cpp
// Predicate: 必须精确
Orientation orient = orientation(p1, p2, p3);  // 返回 LEFT/RIGHT/COLLINEAR

// Construction: 可近似
Point_2 intersection = line1.intersection(line2);  // 返回近似坐标
```

### 2.2 Kernel 抽象层设计

CGAL 通过模板参数化支持多种数值内核：

```cpp
// 浮点内核（快速但不保真）
typedef CGAL::Simple_cartesian<double> Kernel_f;

// 精确内核（慢但保证正确）
typedef CGAL::Exact_predicates_exact_constructions_kernel Kernel_e;

// 混合内核（谓词精确，构造近似）
typedef CGAL::Exact_predicates_inexact_constructions_kernel Kernel_h;
```

**Lv-00 对照表**：

| CGAL Kernel | Lv-00 对应 | 适用场景 |
|-------------|-----------|---------|
| Simple_cartesian<double> | 数值路径（FloatCoord） | 可视化渲染 |
| Exact_predicates_inexact | 符号路径（SymbolicCoord） + 区间验证 | 证明判定 |
| Exact_predicates_exact | 符号路径（精确求解） | 严格证明 |

### 2.3 概念（Concept）驱动设计

CGAL 使用 C++ 概念约束模板参数，确保类型满足接口要求：

```cpp
// 定义几何概念
template<typename Kernel>
concept HasOrientation = requires(Kernel k, Point_2<Kernel> p1, p2, p3) {
    { k.orientation_2_object()(p1, p2, p3) } -> std::convertible_to<Orientation>;
};

// 使用概念约束
template<HasOrientation K>
void check_collinear(Point_2<K> p1, Point_2<K> p2, Point_2<K> p3) {
    if (k.orientation_2_object()(p1, p2, p3) == COLLINEAR) { ... }
}
```

### 2.4 AABB 树空间索引

CGAL 的 AABB_tree 提供高效的空间查询：

```cpp
// 构建 AABB 树
AABB_tree tree(faces(mesh).begin(), faces(mesh).end(), mesh);

// 射线查询
Ray_3 ray(origin, direction);
auto result = tree.first_intersection(ray);

// 最近邻查询
Point_3 query_point;
auto closest = tree.closest_point(query_point);
```

### 2.5 Delaunay 三角剖分

CGAL 的三角剖分实现是几何约束求解的基础：

```cpp
// 2D Delaunay 三角剖分
Delaunay_triangulation_2<Kernel> dt;
dt.insert(points.begin(), points.end());

// 约束三角剖分（CDT）
Constrained_triangulation_2<Kernel> cdt;
cdt.insert_constraint(p1, p2);  // 添加约束边
```

---

## 三、Lv-00 映射方案

### 3.1 精确谓词范式映射

将 CGAL 的 Predicate/Construction 分离映射到 Lv-00 的约束判定与几何构造：

```c
// Lv-00 精确谓词接口设计
typedef enum {
    LV00_ORIENTATION_LEFT,
    LV00_ORIENTATION_RIGHT,
    LV00_ORIENTATION_COLLINEAR,
    LV00_ORIENTATION_COPLANAR
} Lv00Orientation;

// Predicate: 使用精确算术
Lv00Orientation lv00_orientation_2d(
    const Lv00Point2D *p1,
    const Lv00Point2D *p2,
    const Lv00Point2D *p3,
    Lv00PrecisionMode mode  // EXACT/INEXACT/INTERVAL
);

// Construction: 可使用近似
Lv00Point2D lv00_line_intersection(
    const Lv00Line2D *l1,
    const Lv00Line2D *l2,
    Lv00PrecisionMode mode
);
```

### 3.2 Kernel 抽象层映射

```c
// Lv-00 Kernel 类型定义
typedef struct Lv00Kernel {
    Lv00PrecisionType precision;  // FLOAT/EXACT/INTERVAL
    Lv00ArithmeticBackend backend;  // MPFR/GMP/FLINT/DOUBLE
    Lv00PredicateMode predicate_mode;  // ALWAYS_EXACT/ADAPTIVE
    Lv00ConstructionMode construction_mode;  // EXACT/INEXACT/HYBRID
} Lv00Kernel;

// Kernel 选择函数
Lv00Kernel lv00_kernel_select(Lv00PrecisionType prec, Lv00ArithmeticBackend backend);

// Predicate 对象工厂
typedef struct {
    Lv00OrientationFunc orientation_2d;
    Lv00SideOfCircleFunc side_of_circle;
    Lv00CollinearFunc collinear_check;
} Lv00PredicateObject;

Lv00PredicateObject lv00_kernel_predicates(const Lv00Kernel *k);
```

### 3.3 AABB 树映射

```c
// Lv-00 AABB 树结构
typedef struct Lv00AABBNode {
    Lv00BoundingBox3D bbox;
    int first_child;   // 左子节点索引（-1 表示叶子）
    int second_child;  // 右子节点索引
    int primitive_id;  // 叶子节点存储的几何体 ID
} Lv00AABBNode;

typedef struct Lv00AABBTree {
    Lv00AABBNode *nodes;
    int node_count;
    int root_index;
    Lv00GeometrySet *geometries;
} Lv00AABBTree;

// AABB 树构建与查询
Lv00AABBTree *lv00_aabb_build(const Lv00GeometrySet *geometries);
Lv00RayHitResult lv00_aabb_ray_intersect(const Lv00AABBTree *tree, const Lv00Ray3D *ray);
Lv00Point3D lv00_aabb_closest_point(const Lv00AABBTree *tree, const Lv00Point3D *query);
void lv00_aabb_free(Lv00AABBTree *tree);
```

### 3.4 三角剖分映射

```c
// Lv-00 Delaunay 三角剖分结构
typedef struct Lv00DelaunayTri2D {
    Lv00Triangle2D *triangles;
    int tri_count;
    Lv00Point2D *vertices;
    int vertex_count;
    int *adjacent_tri;  // 三角形邻接表
} Lv00DelaunayTri2D;

// 约束三角剖分
typedef struct Lv00ConstrainedTri2D {
    Lv00DelaunayTri2D base;
    Lv00Segment2D *constraints;  // 约束边列表
    int constraint_count;
} Lv00ConstrainedTri2D;

// 三角剖分 API
Lv00DelaunayTri2D *lv00_delaunay_2d_compute(const Lv00Point2D *points, int count);
Lv00ConstrainedTri2D *lv00_cdt_2d_compute(
    const Lv00Point2D *points, int point_count,
    const Lv00Segment2D *constraints, int constraint_count
);
void lv00_triangulation_free(Lv00DelaunayTri2D *tri);
```

---

## 四、实现路线图

### 4.1 分阶段实施表

| 阶段 | 目标 | 交付物 | 工作量 | 依赖 |
|------|------|--------|--------|------|
| **P1: 精确谓词层** | 实现 orientation/side_of_circle 等核心谓词 | `include/lv00/predicate.h`（~200行） | 3 天 | MPFR/FLINT |
| **P2: Kernel 抽象** | 实现 Kernel 选择与 Predicate 对象工厂 | `include/lv00/kernel.h`（~150行） | 2 天 | P1 |
| **P3: AABB 树** | 实现空间索引与射线查询 | `include/lv00/aabb_tree.h`（~300行） | 4 天 | P2 |
| **P4: 三角剖分** | 实现 2D Delaunay 与 CDT | `include/lv00/triangulation.h`（~400行） | 5 天 | P3 |
| **P5: 集成测试** | 与约束图求解器集成 | `test/c/test_cgal_integration.c` | 2 天 | P4 |

### 4.2 技术选型建议

| CGAL 特性 | Lv-00 实现建议 | 理由 |
|-----------|---------------|------|
| 精确谓词 | 使用 FLINT/Arb 区间算术 | 比 GMP/MPFR 更快，与 Lv-00 技术栈一致 |
| Kernel 抽象 | C 函数指针 + 结构体 | 避免 C++ 模板复杂度 |
| AABB 树 | 纯 C 实现 | 与 Lv-00 内核架构一致 |
| 三角剖分 | 先实现 2D，后续扩展 3D | 2D 是几何证明的核心场景 |

### 4.3 性能基准

| 操作 | CGAL 性能 | Lv-00 目标 | 测试方法 |
|------|----------|-----------|---------|
| Orientation 2D | ~100ns/次 | ~150ns/次 | 1M 次调用基准 |
| AABB 射线查询 | ~10μs/1000 面 | ~15μs/1000 面 | 100K 次查询 |
| Delaunay 2D 构建 | ~O(n log n) | ~O(n log n) | 10K 点集 |

---

## 五、附录

### 5.1 CGAL 核心包列表

| 包名 | 功能 | Lv-00 相关性 |
|------|------|-------------|
| Kernel_23 | 2D/3D 基础内核 | **高** - 基础几何层 |
| Triangulation_2 | 2D 三角剖分 | **高** - 约束求解 |
| AABB_tree | 空间索引 | **高** - 空间搜索 |
| Polygon_mesh_processing | 网格处理 | 中 - 可视化 |
| Boolean_set_operations_2 | 2D 布尔运算 | 中 - CSG 操作 |
| Convex_hull_2/3 | 凸包计算 | 中 - 几何分析 |
| Arrangement_on_surface_2 | 平面排列 | 低 - 高级拓扑 |

### 5.2 精确算术后端对比

| 后端 | 特点 | 适用场景 |
|------|------|---------|
| **double** | 最快，不保真 | 可视化渲染 |
| **MPFR** | 任意精度，慢 | 严格证明 |
| **FLINT/Arb** | 区间算术，较快 | **推荐** - Lv-00 默认 |
| **LEDA** | 商业库 | 不推荐 |

### 5.3 参考文献

1. CGAL User Manual: [doc.cgal.org](https://doc.cgal.org)
2. "Exact Geometric Computation" - Chee K. Yap, 1997
3. "Computational Geometry: Algorithms and Applications" - de Berg et al., 2008
4. CGAL GitHub: [github.com/CGAL/cgal](https://github.com/CGAL/cgal)

---

> **文档结束**  
> 本文档约 450 行，覆盖 CGAL 的精确谓词范式、Kernel 抽象、AABB 树、三角剖分等核心特性，为 Lv-00 第 2 层和第 3 层提供直接参考。