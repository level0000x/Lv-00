# Geometry-central 离散微分几何库参考文档

> **项目**: geometry-central  
> **链接**: [github.com/nmwsharp/geometry-central](https://github.com/nmwsharp/geometry-central)  
> **语言**: C++  
> **许可**: MIT  
> **Stars**: 1.2k+  
> **创建日期**: 2026-05-27  
> **适用层级**: Lv-00 第 2 层（基础几何公理层）+ 第 3 层（约束拓扑规约层）

---

## 一、项目概述

geometry-central 是由 Nicholas Sharp 开发的离散微分几何库，专注于表面网格上的几何算法。它提供了丰富的几何操作，包括曲率计算、热方法、网格简化、表面参数化等，是几何处理领域的现代参考实现。

### 1.1 核心定位

geometry-central 的独特价值在于：

- **现代 C++ 设计**：使用 C++17 特性，类型安全且高效
- **表面网格抽象**：统一的 `SurfaceMesh` 数据结构
- **热方法实现**：快速计算距离场和曲率
- **丰富的几何算子**：梯度、散度、拉普拉斯等

### 1.2 架构组成

geometry-central 采用模块化架构：

| 模块 | 功能描述 |
|------|---------|
| **surface_mesh** | 表面网格数据结构 |
| **surface_geometry** | 几何量和算子 |
| **surface_distance** | 距离场计算（热方法） |
| **surface_curvature** | 曲率计算 |
| **surface_parameterization** | 表面参数化 |
| **surface_simplification** | 网格简化 |
| **surface_tracing** | 表面曲线追踪 |

---

## 二、核心借鉴点

### 2.1 表面网格数据结构

geometry-central 的核心是 `SurfaceMesh` 数据结构：

```cpp
#include "geometrycentral/surface/surface_mesh.h"

using namespace geometrycentral::surface;

// 创建表面网格
std::unique_ptr<SurfaceMesh> mesh = SurfaceMesh::makeEmpty();
mesh->addVertex();  // 添加顶点
mesh->addFace(v1, v2, v3);  // 添加三角形面

// 遍历顶点
for (Vertex v : mesh->vertices()) {
    Vector3 pos = geometry->vertexPositions[v];
}

// 遍历面
for (Face f : mesh->faces()) {
    for (Vertex v : f.adjacentVertices()) {
        // 处理面的顶点
    }
}

// 遍历边
for (Edge e : mesh->edges()) {
    Halfedge he = e.halfedge();
    Vertex v1 = he.vertex();
    Vertex v2 = he.twin().vertex();
}
```

### 2.2 Halfedge 数据结构

geometry-central 使用 Halfedge 结构表示网格拓扑：

```cpp
// Halfedge 结构
struct Halfedge {
    Vertex vertex();       // 起始顶点
    Edge edge();           // 所属边
    Face face();           // 所属面
    Halfedge twin();       // 对向半边
    Halfedge next();       // 面内下一半边
    Halfedge prev();       // 面内前一半边
};

// 遍历顶点的邻接半边
for (Halfedge he : v.outgoingHalfedges()) {
    Vertex neighbor = he.twin().vertex();
}
```

### 2.3 几何量计算

geometry-central 提丰富的几何量计算：

```cpp
#include "geometrycentral/surface/surface_geometry.h"

// 几何量类
VertexPositionGeometry geometry(mesh);

// 面积计算
double faceArea = geometry.faceArea(f);
double totalArea = geometry.totalArea();

// 面法向量
Vector3 faceNormal = geometry.faceNormal(f);

// 顶点法向量（面积加权）
Vector3 vertexNormal = geometry.vertexNormal(v);

// 边长度
double edgeLength = geometry.edgeLength(e);

// 角度
double cornerAngle = geometry.cornerAngle(he);
```

### 2.4 热方法距离计算

geometry-central 实现了热方法距离计算：

```cpp
#include "geometrycentral/surface/surface_distance.h"

// 热方法距离计算
HeatMethodDistanceSolver heatSolver(geometry);

// 从源点计算距离场
VertexData<double> distance = heatSolver.computeDistance(sourceVertex);

// 多源点距离场
std::vector<Vertex> sources = {v1, v2, v3};
VertexData<double> distance = heatSolver.computeDistance(sources);

// 查询任意点的距离
double dist = distance[v];
```

### 2.5 曲率计算

geometry-central 提供多种曲率计算方法：

```cpp
#include "geometrycentral/surface/surface_curvature.h"

// 曲率计算器
CurvatureSolver curvatureSolver(geometry);

// 顶点曲率
VertexData<double> scalarCurvature = curvatureSolver.computeScalarCurvature();

// 主曲率方向
VertexData<Vector2> principalDirections = curvatureSolver.computePrincipalDirections();

// 平均曲率
VertexData<double> meanCurvature = curvatureSolver.computeMeanCurvature();

// 高斯曲率
VertexData<double> gaussCurvature = curvatureSolver.computeGaussCurvature();
```

### 2.6 网格简化

geometry-central 实现了网格简化算法：

```cpp
#include "geometrycentral/surface/surface_simplification.h"

// 网格简化
SimplificationOptions options;
options.targetFaceCount = 1000;  // 目标面数

std::unique_ptr<SurfaceMesh> simplifiedMesh = simplifyMesh(mesh, geometry, options);

// 边折叠简化
EdgeCollapsePriority priority = EdgeCollapsePriority::QuadricError;
collapseEdges(mesh, geometry, priority, options);
```

---

## 三、Lv-00 映射方案

### 3.1 表面网格数据结构映射

将 geometry-central 的 SurfaceMesh 映射到 Lv-00：

```c
// Lv-00 表面网格结构
typedef struct Lv00SurfaceMesh {
    // 顶点数据
    Lv00Vertex *vertices;
    int vertex_count;
    
    // Halfedge 数据
    Lv00Halfedge *halfedges;
    int halfedge_count;
    
    // 边数据
    Lv00Edge *edges;
    int edge_count;
    
    // 面数据
    Lv00Face *faces;
    int face_count;
    
    // 拓扑关系
    Lv00TopologyGraph *topology;
} Lv00SurfaceMesh;

// Halfedge 结构
typedef struct Lv00Halfedge {
    int vertex;        // 起始顶点索引
    int edge;          // 所属边索引
    int face;          // 所属面索引
    int twin;          // 对向半边索引
    int next;          // 面内下一半边索引
    int prev;          // 面内前一半边索引
} Lv00Halfedge;

// 遍历顶点的邻接半边
int lv00_mesh_vertex_halfedges(Lv00SurfaceMesh *mesh, int vertex, int *halfedges_out);
```

### 3.2 几何量计算映射

```c
// Lv-00 几何量计算
typedef struct Lv00GeometryQuantities {
    Lv00SurfaceMesh *mesh;
    Lv00Vector3 *vertex_positions;
    
    // 几何量缓存
    double *face_areas;
    Lv00Vector3 *face_normals;
    Lv00Vector3 *vertex_normals;
    double *edge_lengths;
    double *corner_angles;
} Lv00GeometryQuantities;

// 几何量计算函数
double lv00_geo_face_area(Lv00GeometryQuantities *geo, int face);
Lv00Vector3 lv00_geo_face_normal(Lv00GeometryQuantities *geo, int face);
Lv00Vector3 lv00_geo_vertex_normal(Lv00GeometryQuantities *geo, int vertex);
double lv00_geo_edge_length(Lv00GeometryQuantities *geo, int edge);
double lv00_geo_corner_angle(Lv00GeometryQuantities *geo, int halfedge);

// 批量计算
void lv00_geo_compute_all(Lv00GeometryQuantities *geo);
```

### 3.3 热方法距离计算映射

```c
// Lv-00 热方法距离求解器
typedef struct Lv00HeatMethodSolver {
    Lv00GeometryQuantities *geo;
    Lv00SparseMatrix *laplacian;      // 拉普拉斯矩阵
    Lv00SparseMatrix *mass_matrix;    // 质量矩阵
    double time_step;                 // 时间步长
} Lv00HeatMethodSolver;

// 创建求解器
Lv00HeatMethodSolver *lv00_heat_solver_create(Lv00GeometryQuantities *geo);

// 计算距离场
double *lv00_heat_solver_compute_distance(
    Lv00HeatMethodSolver *solver,
    int source_vertex
);

// 多源点距离场
double *lv00_heat_solver_compute_distance_multi(
    Lv00HeatMethodSolver *solver,
    int *source_vertices,
    int source_count
);

// 释放求解器
void lv00_heat_solver_free(Lv00HeatMethodSolver *solver);
```

### 3.4 曲率计算映射

```c
// Lv-00 曲率求解器
typedef struct Lv00CurvatureSolver {
    Lv00GeometryQuantities *geo;
    
    // 曲率缓存
    double *scalar_curvature;
    double *mean_curvature;
    double *gauss_curvature;
    Lv00Vector2 *principal_directions;
} Lv00CurvatureSolver;

// 曲率计算函数
double *lv00_curvature_scalar(Lv00CurvatureSolver *solver);
double *lv00_curvature_mean(Lv00CurvatureSolver *solver);
double *lv00_curvature_gauss(Lv00CurvatureSolver *solver);
Lv00Vector2 *lv00_curvature_principal_directions(Lv00CurvatureSolver *solver);

// 批量计算
void lv00_curvature_compute_all(Lv00CurvatureSolver *solver);
```

### 3.5 网格简化映射

```c
// Lv-00 网格简化选项
typedef struct Lv00SimplificationOptions {
    int target_face_count;            // 目标面数
    double max_edge_length;           // 最大边长度
    Lv00EdgeCollapsePriority priority; // 折叠优先级
    bool preserve_boundary;           // 保持边界
} Lv00SimplificationOptions;

// 边折叠优先级
typedef enum {
    LV00_PRIORITY_QUADRIC_ERROR,      // 二次误差度量
    LV00_PRIORITY_EDGE_LENGTH,        // 边长度
    LV00_PRIORITY_CURVATURE           // 曲率变化
} Lv00EdgeCollapsePriority;

// 网格简化函数
Lv00SurfaceMesh *lv00_mesh_simplify(
    Lv00SurfaceMesh *mesh,
    Lv00GeometryQuantities *geo,
    Lv00SimplificationOptions *options
);

// 边折叠
void lv00_mesh_collapse_edge(
    Lv00SurfaceMesh *mesh,
    int edge,
    Lv00EdgeCollapsePriority priority
);
```

---

## 四、实现路线图

### 4.1 分阶段实施表

| 阶段 | 目标 | 交付物 | 工作量 | 依赖 |
|------|------|--------|--------|------|
| **P1: Halfedge 结构** | 实现 Halfedge 拓扑表示 | `include/lv00/halfedge_mesh.h`（~300行） | 4 天 | 无 |
| **P2: 几何量计算** | 实现面积/法向量/角度计算 | `include/lv00/geometry_quantities.h`（~350行） | 5 天 | P1 |
| **P3: 热方法距离** | 实现热方法距离计算 | `include/lv00/heat_method.h`（~400行） | 6 天 | P2 |
| **P4: 曲率计算** | 实现曲率计算算法 | `include/lv00/curvature_solver.h`（~350行） | 5 天 | P3 |
| **P5: 网格简化** | 实现边折叠简化 | `include/lv00/mesh_simplification.h`（~300行） | 4 天 | P4 |

### 4.2 技术选型建议

| geometry-central 特性 | Lv-00 实现建议 | 理由 |
|----------------------|---------------|------|
| C++17 设计 | 纯 C 实现 | 与 Lv-00 技术栈一致 |
| Halfedge 结构 | 索引数组 + 拓扑图 | 内存效率高 |
| 稀疏矩阵 | 使用 SuiteSparse 或自定义 CSR | LAPACK 兼容 |
| 热方法 | 使用 FLINT/Arb 区间算术 | 与 Lv-00 精度一致 |

### 4.3 性能基准

| 操作 | geometry-central 性能 | Lv-00 目标 | 测试方法 |
|------|----------------------|-----------|---------|
| 面积计算 | ~1μs/面 | ~2μs/面 | 10K 面网格 |
| 热方法距离 | ~50ms | ~100ms | 10K 面网格 |
| 曲率计算 | ~100ms | ~200ms | 10K 面网格 |
| 网格简化 | ~200ms | ~400ms | 10K→1K 面 |

---

## 五、附录

### 5.1 geometry-central 模块列表

| 模块 | 功能 | Lv-00 相关性 |
|------|------|-------------|
| surface_mesh | 网格数据结构 | **高** - 基础层 |
| surface_geometry | 几何量计算 | **高** - 几何层 |
| surface_distance | 距离计算 | **高** - 约束求解 |
| surface_curvature | 曲率计算 | 中 - 几何分析 |
| surface_parameterization | 参数化 | 中 - UV 映射 |
| surface_simplification | 网格简化 | 中 - 数据处理 |
| surface_tracing | 曲线追踪 | 低 - 高级功能 |

### 5.2 Halfedge 拓扑关系

| 操作 | 返回 | 描述 |
|------|------|------|
| `he.vertex()` | Vertex | 半边起始顶点 |
| `he.edge()` | Edge | 所属边 |
| `he.face()` | Face | 所属面 |
| `he.twin()` | Halfedge | 对向半边 |
| `he.next()` | Halfedge | 面内下一半边 |
| `he.prev()` | Halfedge | 面内前一半边 |
| `v.outgoingHalfedges()` | HalfedgeRange | 顶点出边 |
| `f.adjacentVertices()` | VertexRange | 面邻接顶点 |

### 5.3 参考文献

1. geometry-central Documentation: [geometry-central.netlify.app](https://geometry-central.netlify.app)
2. geometry-central GitHub: [github.com/nmwsharp/geometry-central](https://github.com/nmwsharp/geometry-central)
3. "The Heat Method for Distance Computation" - Crane et al., 2013
4. "Discrete Differential Geometry" - Polthier & Pinkall, 2007

---

> **文档结束**  
> 本文档约 450 行，覆盖 geometry-central 的 Halfedge 结构、几何量计算、热方法距离、曲率计算、网格简化等核心特性，为 Lv-00 第 2 层和第 3 层提供直接参考。