# Boost.Geometry 空间索引与几何算法参考文档

> **项目**: Boost.Geometry (GGL)  
> **链接**: [boost.org/libs/geometry](https://www.boost.org/libs/geometry)  
> **语言**: C++  
> **许可**: Boost Software License  
> **Stars**: N/A（Boost 子库）  
> **创建日期**: 2026-05-27  
> **适用层级**: Lv-00 第 2 层（基础几何公理层）+ 第 3 层（约束拓扑规约层）

---

## 一、项目概述

Boost.Geometry 是 Boost C++ 库的一部分，提供几何算法和空间索引功能。它采用 Generic Geometry Library (GGL) 设计理念，支持多种几何类型和坐标系统，是 C++ 几何编程的标准参考实现。

### 1.1 核心定位

Boost.Geometry 的独特价值在于：

- **泛型设计**：通过模板支持多种几何类型和坐标系统
- **空间索引**：R-tree、KD-tree 等高效空间搜索结构
- **标准算法**：凸包、距离、面积、交集等几何算法
- **Boost 集成**：与 Boost 其他库无缝协作

### 1.2 架构组成

Boost.Geometry 采用分层架构：

| 层级 | 模块 | 功能描述 |
|------|------|---------|
| **核心层** | `core` | 点、线、面等几何概念 |
| **算法层** | `algorithms` | 凸包、距离、交集等算法 |
| **索引层** | `index` | R-tree 空间索引 |
| **策略层** | `strategies` | 可定制算法策略 |
| **IO 层** | `io` | WKT/GEOJSON 格式读写 |

---

## 二、核心借鉴点

### 2.1 几何概念设计

Boost.Geometry 使用 C++ 概念定义几何类型：

```cpp
#include <boost/geometry.hpp>

namespace bg = boost::geometry;

// 定义点类型
typedef bg::model::point<double, 2, bg::cs::cartesian> point_2d;
typedef bg::model::point<double, 3, bg::cs::cartesian> point_3d;

// 定义几何类型
typedef bg::model::linestring<point_2d> linestring_2d;
typedef bg::model::polygon<point_2d> polygon_2d;
typedef bg::model::box<point_2d> box_2d;
typedef bg::model::segment<point_2d> segment_2d;
typedef bg::model::ring<point_2d> ring_2d;
typedef bg::model::multi_point<point_2d> multi_point_2d;
typedef bg::model::multi_linestring<point_2d> multi_linestring_2d;
typedef bg::model::multi_polygon<point_2d> multi_polygon_2d;
```

### 2.2 空间索引 R-tree

Boost.Geometry 提供高效的 R-tree 空间索引：

```cpp
#include <boost/geometry/index.hpp>

namespace bgi = boost::geometry::index;

// 定义 R-tree 类型
typedef bgi::rtree<point_2d, bgi::quadratic<16>> rtree_2d;
typedef bgi::rtree<box_2d, bgi::rstar<16>> rtree_box;

// 创建 R-tree
rtree_2d rtree;
rtree.insert(point_2d(1, 2));
rtree.insert(point_2d(3, 4));
rtree.insert(point_2d(5, 6));

// 空间查询
box_2d query_box(point_2d(0, 0), point_2d(4, 4));
std::vector<point_2d> result;
rtree.query(bgi::within(query_box), result);

// 最近邻查询
std::vector<point_2d> nearest;
rtree.query(bgi::nearest(point_2d(2, 3), 5), nearest);  // 5 个最近邻

// KNN 查询
point_2d query_point(2, 3);
rtree.query(bgi::nearest(query_point, 10), nearest);  // 10 个最近邻
```

### 2.3 几何算法

Boost.Geometry 提丰富的几何算法：

```cpp
// 距离计算
double dist = bg::distance(point_2d(0, 0), point_2d(3, 4));

// 面积计算
polygon_2d poly;
bg::read_wkt("POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))", poly);
double area = bg::area(poly);

// 凸包计算
std::vector<point_2d> points = {...};
std::vector<point_2d> hull;
bg::convex_hull(points, hull);

// 交集计算
polygon_2d poly1, poly2, result;
bg::intersection(poly1, poly2, result);

// 布尔运算
bg::union_(poly1, poly2, result);
bg::difference(poly1, poly2, result);
bg::sym_difference(poly1, poly2, result);

// 包含关系
bool within = bg::within(point_2d(5, 5), poly);
bool covers = bg::covers(poly, box_2d(point_2d(0, 0), point_2d(10, 10)));

// 相交检测
bool intersects = bg::intersects(poly1, poly2);
```

### 2.4 策略模式

Boost.Geometry 使用策略模式定制算法行为：

```cpp
// 距离策略
bg::strategy::distance::haversine<double> haversine(6371.0);  // 地球半径
double geo_dist = bg::distance(point1, point2, haversine);

// 凸包策略
bg::strategy::convex_hull::graham_jarvis<> convex_hull_strategy;
bg::convex_hull(points, hull, convex_hull_strategy);

// 侧边策略
bg::strategy::side::side_by_triangle<> side_strategy;
bg::side_info side = bg::side(p1, p2, p3, side_strategy);
```

### 2.5 WKT 格式读写

Boost.Geometry 支持 WKT 格式：

```cpp
// WKT 读取
point_2d p;
bg::read_wkt("POINT(3 4)", p);

linestring_2d line;
bg::read_wkt("LINESTRING(0 0, 1 1, 2 2)", line);

polygon_2d poly;
bg::read_wkt("POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))", poly);

// WKT 写入
std::string wkt = bg::wkt(p);
std::string wkt_line = bg::wkt(line);
std::string wkt_poly = bg::wkt(poly);
```

---

## 三、Lv-00 映射方案

### 3.1 几何概念映射

将 Boost.Geometry 的几何概念映射到 Lv-00：

```c
// Lv-00 几何类型定义
typedef struct Lv00Point2D {
    double x, y;
} Lv00Point2D;

typedef struct Lv00Point3D {
    double x, y, z;
} Lv00Point3D;

typedef struct Lv00LineString2D {
    Lv00Point2D *points;
    int point_count;
} Lv00LineString2D;

typedef struct Lv00Polygon2D {
    Lv00Point2D *outer_ring;
    int outer_count;
    Lv00Point2D **inner_rings;
    int *inner_counts;
    int inner_ring_count;
} Lv00Polygon2D;

typedef struct Lv00Box2D {
    Lv00Point2D min_corner;
    Lv00Point2D max_corner;
} Lv00Box2D;

typedef struct Lv00Segment2D {
    Lv00Point2D p1, p2;
} Lv00Segment2D;

typedef struct Lv00Ring2D {
    Lv00Point2D *points;
    int point_count;
    bool is_closed;
} Lv00Ring2D;
```

### 3.2 空间索引映射

```c
// Lv-00 R-tree 结构
typedef struct Lv00RTreeNode {
    Lv00Box2D bbox;                  // 节点边界框
    int first_child;                 // 左子节点索引
    int second_child;                // 右子节点索引
    int element_id;                  // 叶子节点存储的元素 ID
    int level;                       // 节点层级
} Lv00RTreeNode;

typedef struct Lv00RTree {
    Lv00RTreeNode *nodes;
    int node_count;
    int root_index;
    int max_elements_per_node;       // 每节点最大元素数
    Lv00RTreeSplitAlgorithm split_algorithm;  // 分裂算法
} Lv00RTree;

// R-tree 分裂算法
typedef enum {
    LV00_SPLIT_LINEAR,               // 线性分裂
    LV00_SPLIT_QUADRATIC,            // 二次分裂
    LV00_SPLIT_RSTAR                 // R* 分裂
} Lv00RTreeSplitAlgorithm;

// R-tree 操作
Lv00RTree *lv00_rtree_create(int max_elements_per_node, Lv00RTreeSplitAlgorithm split);
void lv00_rtree_insert(Lv00RTree *tree, Lv00Box2D bbox, int element_id);
void lv00_rtree_remove(Lv00RTree *tree, Lv00Box2D bbox, int element_id);

// 空间查询
int lv00_rtree_query_within(Lv00RTree *tree, Lv00Box2D query_box, int *result_ids);
int lv00_rtree_query_nearest(Lv00RTree *tree, Lv00Point2D query_point, int k, int *result_ids);
int lv00_rtree_query_intersects(Lv00RTree *tree, Lv00Box2D query_box, int *result_ids);
```

### 3.3 几何算法映射

```c
// Lv-00 几何算法
double lv00_geo_distance_point_point(Lv00Point2D *p1, Lv00Point2D *p2);
double lv00_geo_distance_point_line(Lv00Point2D *p, Lv00LineString2D *line);
double lv00_geo_distance_point_polygon(Lv00Point2D *p, Lv00Polygon2D *poly);

double lv00_geo_area_polygon(Lv00Polygon2D *poly);
double lv00_geo_length_linestring(Lv00LineString2D *line);

// 凸包计算
Lv00Ring2D *lv00_geo_convex_hull(Lv00Point2D *points, int point_count);

// 交集计算
Lv00Polygon2D *lv00_geo_intersection(Lv00Polygon2D *poly1, Lv00Polygon2D *poly2);
Lv00Polygon2D *lv00_geo_union(Lv00Polygon2D *poly1, Lv00Polygon2D *poly2);
Lv00Polygon2D *lv00_geo_difference(Lv00Polygon2D *poly1, Lv00Polygon2D *poly2);

// 包含关系
bool lv00_geo_within_point_polygon(Lv00Point2D *p, Lv00Polygon2D *poly);
bool lv00_geo_within_polygon_polygon(Lv00Polygon2D *poly1, Lv00Polygon2D *poly2);
bool lv00_geo_intersects_polygon_polygon(Lv00Polygon2D *poly1, Lv00Polygon2D *poly2);
```

### 3.4 策略模式映射

```c
// Lv-00 策略结构
typedef struct Lv00DistanceStrategy {
    Lv00DistanceType type;           // EUCLIDEAN/Haversine/MANHATTAN
    double earth_radius;             // 地球半径（用于 Haversine）
} Lv00DistanceStrategy;

typedef struct Lv00ConvexHullStrategy {
    Lv00ConvexHullType type;         // Graham/Jarvis/Andrew
} Lv00ConvexHullStrategy;

typedef struct Lv00SideStrategy {
    Lv00SideType type;               // Triangle/Exact/Robust
} Lv00SideStrategy;

// 策略应用
double lv00_geo_distance_with_strategy(
    Lv00Point2D *p1, Lv00Point2D *p2, Lv00DistanceStrategy *strategy
);

Lv00Ring2D *lv00_geo_convex_hull_with_strategy(
    Lv00Point2D *points, int point_count, Lv00ConvexHullStrategy *strategy
);
```

### 3.5 WKT 格式映射

```c
// Lv-00 WKT 读写
Lv00Point2D *lv00_wkt_read_point(const char *wkt);
Lv00LineString2D *lv00_wkt_read_linestring(const char *wkt);
Lv00Polygon2D *lv00_wkt_read_polygon(const char *wkt);

char *lv00_wkt_write_point(Lv00Point2D *p);
char *lv00_wkt_write_linestring(Lv00LineString2D *line);
char *lv00_wkt_write_polygon(Lv00Polygon2D *poly);

// WKT 解析器
typedef struct Lv00WKTParser {
    const char *buffer;
    int pos;
    int length;
} Lv00WKTParser;

Lv00WKTParser *lv00_wkt_parser_create(const char *wkt);
void lv00_wkt_parser_free(Lv00WKTParser *parser);
```

---

## 四、实现路线图

### 4.1 分阶段实施表

| 阶段 | 目标 | 交付物 | 工作量 | 依赖 |
|------|------|--------|--------|------|
| **P1: 几何类型定义** | 定义点、线、面等几何类型 | `include/lv00/geometry_types.h`（~200行） | 2 天 | 无 |
| **P2: R-tree 空间索引** | 实现 R-tree 结构和查询 | `include/lv00/rtree_index.h`（~400行） | 5 天 | P1 |
| **P3: 几何算法** | 实现距离、面积、凸包算法 | `include/lv00/geometry_algorithms.h`（~350行） | 4 天 | P2 |
| **P4: 策略模式** | 实现可定制策略接口 | `include/lv00/geometry_strategies.h`（~250行） | 3 天 | P3 |
| **P5: WKT 格式** | 实现 WKT 读写功能 | `include/lv00/wkt_format.h`（~300行） | 3 天 | P4 |

### 4.2 技术选型建议

| Boost.Geometry 特性 | Lv-00 实现建议 | 理由 |
|---------------------|---------------|------|
| C++ 模板泛型 | C 结构体 + 函数指针 | 与 Lv-00 技术栈一致 |
| R-tree 分裂算法 | 先实现 Quadratic，后续扩展 R* | Quadratic 实现简单 |
| 凸包算法 | 使用 Andrew 算法 | 比 Graham-Jarvis 更高效 |
| WKT 解析 | 手写解析器 | 避免依赖外部库 |

### 4.3 性能基准

| 操作 | Boost.Geometry 性能 | Lv-00 目标 | 测试方法 |
|------|---------------------|-----------|---------|
| R-tree 插入 | ~10μs/元素 | ~20μs/元素 | 10K 元素插入 |
| R-tree 查询 | ~1μs/查询 | ~2μs/查询 | 10K 次查询 |
| 凸包计算 | ~O(n log n) | ~O(n log n) | 10K 点集 |
| 面积计算 | ~1μs/多边形 | ~2μs/多边形 | 1K 多边形 |

---

## 五、附录

### 5.1 Boost.Geometry 几何类型列表

| 类型 | WKT 名称 | 描述 |
|------|---------|------|
| `point` | POINT | 点 |
| `linestring` | LINESTRING | 线串 |
| `polygon` | POLYGON | 多边形 |
| `box` | BOX | 边界框 |
| `segment` | SEGMENT | 线段 |
| `ring` | RING | 环（多边形边界） |
| `multi_point` | MULTIPOINT | 点集合 |
| `multi_linestring` | MULTILINESTRING | 线串集合 |
| `multi_polygon` | MULTIPOLYGON | 多边形集合 |

### 5.2 Boost.Geometry 算法列表

| 算法 | 函数 | 描述 |
|------|------|------|
| 距离 | `distance` | 两几何体距离 |
| 面积 | `area` | 多边形面积 |
| 长度 | `length` | 线串长度 |
| 凸包 | `convex_hull` | 凸包计算 |
| 交集 | `intersection` | 交集计算 |
| 并集 | `union_` | 并集计算 |
| 差集 | `difference` | 差集计算 |
| 对称差集 | `sym_difference` | 对称差集计算 |
| 包含 | `within` | 包含检测 |
| 覆盖 | `covers` | 覆盖检测 |
| 相交 | `intersects` | 相交检测 |
| 相等 | `equals` | 相等检测 |

### 5.3 参考文献

1. Boost.Geometry Documentation: [boost.org/libs/geometry](https://www.boost.org/libs/geometry)
2. Boost.Geometry Source: [github.com/boostorg/geometry](https://github.com/boostorg/geometry)
3. "Generic Geometry Library" - Barend Gehrels, 2009
4. "R-tree: A Dynamic Index Structure for Spatial Searching" - Guttman, 1984

---

> **文档结束**  
> 本文档约 420 行，覆盖 Boost.Geometry 的几何概念、空间索引、几何算法、策略模式、WKT 格式等核心特性，为 Lv-00 第 2 层和第 3 层提供直接参考。