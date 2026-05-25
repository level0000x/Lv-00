# Lv-00 参考落地设计文档：OpenSCAD 脚本编译器

> **版本**: 1.0.0  
> **日期**: 2026-05-24  
> **参考**: OpenSCAD (github.com/openscad/openscad) —— 脚本化 3D CAD 建模  
> **目标**: 将 OpenSCAD 的"脚本即 3D 模型"编译器范式、CSG 操作符链、WASM Web 移植双重验证路径映射到 Lv-00 几何元语言

---

## 目录

1. [OpenSCAD 项目概述与 Lv-00 借鉴动机](#1-openscad-项目概述与-lv-00-借鉴动机)
2. ["脚本即 3D 模型"编译器范式映射](#2-脚本即-3d-模型编译器范式映射)
3. [CSG 操作符链映射到 Lv-00 几何构造语言](#3-csg-操作符链映射到-lv-00-几何构造语言)
4. [WASM Web 移植双重验证路径](#4-wasm-web-移植双重验证路径)
5. [完整示例：泰姬陵圆顶的 CSG 构造](#5-完整示例泰姬陵圆顶的-csg-构造)
6. [编译器流水线与代码生成](#6-编译器流水线与代码生成)
7. [关键数据结构对照表](#7-关键数据结构对照表)

---

## 1. OpenSCAD 项目概述与 Lv-00 借鉴动机

### 1.1 OpenSCAD 的核心范式

OpenSCAD 不同于传统交互式 CAD 软件（如 FreeCAD、Fusion 360），它采用**声明式脚本**定义 3D 模型。其核心范式可提炼为三个关键词：

| 范式 | 说明 | OpenSCAD 中的载体 |
|------|------|-------------------|
| **脚本即模型** | 用编程语言描述几何体，编译后直接生成 3D 网格 | `.scad` 脚本文件 |
| **CSG 操作符链** | 通过并/差/交操作组合基本图元构建复杂形体 | `union()` / `difference()` / `intersection()` |
| **参数化建模** | 变量、循环、条件、模块等语言特性驱动几何参数化 | `module`, `for`, `if`, `$fn` |

### 1.2 Lv-00 借鉴动机

Lv-00 目前聚焦于 2D 构造几何（`PointEntity`, `LinearEntity`, `CircularEntity`, `PolygonEntity` 等），但在以下方向需要 OpenSCAD 的设计智慧：

| 借鉴方向 | OpenSCAD 特性 | Lv-00 现有基础 | 差距与目标 |
|----------|--------------|---------------|-----------|
| **声明式几何 DSL** | SCAD 语言——函数式、无副作用 | `dsl_design_gclc_reference.md` 的 2D DSL | 需要扩展到 3D CSG 构造语法 |
| **CSG 布尔操作** | `union/difference/intersection` 三元操作 | 仅有点/线/圆相交（`geom_entity_intersect`） | 缺失区域布尔操作（多边形并/差/交） |
| **参数化/模块化** | `module` + 参数传递 | `FuncBlock` 函数块系统 | 需要 FuncBlock 的 3D 几何扩展 |
| **WASM Web 移植** | OpenSCAD-WASM 在线预览 | 无 Web 端 3D 预览 | 提出 GCLC+OpenSCAD 双重验证路径 |
| **多边形→网格** | `linear_extrude` + `rotate_extrude` | 2D 多边形实体 | 扩展到 3D 拉伸/旋转构造 |

### 1.3 总体架构对照

```
OpenSCAD                          Lv-00
────────────────────────────────────────────────
.scad 脚本文件                    .lvz DSL 文件
  module house() { ... }           funcblock house() { ... }
  cube([10,10,10]);               csg_cube(10, 10, 10);
  union() { ... }                 csg_union(children);

CSG 操作符                        几何类型 API (新增)
  union()         →                geometry_csg_union()
  difference()    →                geometry_csg_difference()
  intersection()  →                geometry_csg_intersection()

WASM 在线预览                     双重验证路径
  OpenSCAD-WASM                    路径A: GCLC (2D 验证)
  OpenJSCAD                        路径B: OpenSCAD-WASM (3D 验证)
```

---

## 2. "脚本即 3D 模型"编译器范式映射

### 2.1 OpenSCAD 脚本结构分析

OpenSCAD 脚本的核心特征：

```openscad
// OpenSCAD 示例: 简单房屋模型
module house(width, depth, height) {
    // 主体（立方体）
    cube([width, depth, height]);

    // 屋顶（棱柱 → 通过差集切出三角形截面）
    translate([0, 0, height]) {
        rotate([0, 0, 0]) {
            linear_extrude(height=depth) {
                polygon([[0,0], [width/2, height*0.6], [width, 0]]);
            }
        }
    }

    // 门（从主体中挖去）
    translate([width/2 - 5, 0, 0]) {
        cube([10, 1, height * 0.6]);
    }
}

house(20, 15, 10);
```

关键观察：
- **module** = 带参数的命名构造块
- **CSG 层次树** = `union/difference/intersection` 隐式嵌套
- **隐式 union** = 模块内所有语句的默认可视化结果是 union
- **2D→3D 拉伸** = `linear_extrude` / `rotate_extrude` 将 2D 轮廓变为 3D 实体

### 2.2 Lv-00 DSL 中的 3D 脚本范式映射

Lv-00 将 OpenSCAD 的范式映射到已有的 `FuncBlock` + `ConstraintGraph` 体系：

| OpenSCAD 概念 | Lv-00 映射 | 数据流 |
|--------------|-----------|--------|
| `.scad` 文件 | `.lvz` 文件中的 `@geometry` 块 | 文本 → 语法树 → `ConstraintGraph` + `FuncBlock[]` |
| `module name(p1, p2) { ... }` | `funcblock name(p1 : Type1, p2 : Type2) -> CSGNode { ... }` | FuncBlock 创建 + 端口绑定 |
| 隐式 union | `CSGNode` 的子节点列表（默认合并为 union） | 多个子 CSGNode 的 `children` 数组 |
| 基本图元 `cube/sphere/cylinder` | `csg_cube(w,h,d)` / `csg_sphere(r)` / `csg_cylinder(r,h)` | 图元 FuncBlock |
| `linear_extrude` | `csg_extrude_linear(profile_2d, height)` | 2D PolygonEntity → 3D CSGNode |
| `rotate_extrude` | `csg_extrude_rotate(profile_2d, angle)` | 2D PolygonEntity → 旋转体 CSGNode |
| `translate` / `rotate` / `scale` | `csg_transform_translate(node, dx,dy,dz)` 等 | 变换 FuncBlock |
| 变量 + 循环 | Lv-00 已有的 `number` + `for` 控制流 | 编译时常量折叠 |

### 2.3 Lv-00 3D 几何 DSL 语法草案

```
// ============================================================
// Lv-00 3D CSG DSL —— 借鉴 OpenSCAD 的声明式脚本风格
// ============================================================

// --- 基本图元构造 ---
csg_cube(20, 15, 10);                     // 立方体 (宽, 深, 高)
csg_sphere(r=5, $fn=32);                   // 球体 (半径, 精细度)
csg_cylinder(r=3, h=10, $fn=24);           // 圆柱 (半径, 高, 精细度)
csg_polyhedron(vertices, faces);           // 多面体 (顶点列表, 面列表)

// --- 2D → 3D 拉伸 ---
polygon p = regular_polygon(6, r=4);        // 正六边形轮廓
csg_extrude_linear(p, height=10);            // 线性拉伸为六棱柱
csg_extrude_rotate(p, angle=360);            // 旋转拉伸为环形

// --- 变换操作 ---
csg_node t = csg_translate(cube, dx=5, dy=0, dz=0);
csg_node r = csg_rotate(cube, ax=0, ay=0, az=45);
csg_node s = csg_scale(cube, sx=1, sy=2, sz=1);

// --- CSG 布尔操作 ---
csg_node body     = csg_cube(20, 15, 10);
csg_node hole     = csg_translate(csg_cylinder(r=2, h=15), dx=10, dy=7.5, dz=0);
csg_node door     = csg_difference(body, hole);  // 从主体中挖去洞

// --- 模块化（等价于 OpenSCAD module）---
funcblock house(width : Number, depth : Number, height : Number) -> CSGNode {
    csg_node body   = csg_cube(width, depth, height);
    csg_node roof   = csg_extrude_rotate(
        triangle_points(width, height),
        angle=90
    );
    csg_node door_hole = csg_translate(
        csg_cube(width*0.15, depth*0.05, height*0.6),
        dx=width/2, dy=0, dz=height*0.05
    );
    csg_node result = csg_difference(csg_union(body, roof), door_hole);
    return result;
}

// --- 例化 ---
csg_node my_house = house(width=20, depth=15, height=10);
```

### 2.4 3D CSG 树内部表示

在 Lv-00 内部，CSG 操作构造为一棵表达式树：

```c
/**
 * @brief CSG 节点类型 —— 借鉴 OpenSCAD CSG 树设计
 *
 * CSG 树是一种层次结构，叶节点为基本图元（立方体、球体等），
 * 内部节点为布尔操作（并/差/交）或变换操作（平移/旋转/缩放）。
 *
 * 每个 CSG 节点关联一个 FuncBlock（如果由模块产生），
 * 并维护一个 children 链表用于组合操作。
 */
typedef enum {
    CSG_NODE_PRIMITIVE,       /**< 基本图元（立方体/球体/圆柱/多面体） */
    CSG_NODE_UNION,           /**< 并集：children 的布尔并 */
    CSG_NODE_DIFFERENCE,      /**< 差集：children[0] - (children[1] ∪ children[2] ∪ ...) */
    CSG_NODE_INTERSECTION,    /**< 交集：children 的布尔交 */
    CSG_NODE_TRANSFORM,       /**< 仿射变换（平移/旋转/缩放） */
    CSG_NODE_EXTRUDE_LINEAR,  /**< 线性拉伸：2D 轮廓 → 3D 棱柱 */
    CSG_NODE_EXTRUDE_ROTATE,  /**< 旋转拉伸：2D 轮廓 → 3D 旋转体 */
    CSG_NODE_HULL,            /**< 凸包：children 的凸包 */
    CSG_NODE_MINKOWSKI        /**< Minkowski 和 */
} CSGNodeKind;

typedef struct CSGNode {
    CSGNodeKind kind;              /**< 节点类型 */

    /* 节点数据（根据 kind 使用不同的联合体字段） */
    union {
        /* CSG_NODE_PRIMITIVE 时的图元信息 */
        struct {
            int primitive_type;    /**< 0=cube, 1=sphere, 2=cylinder, 3=polyhedron */
            double params[6];      /**< 图元参数 */
        } primitive;

        /* CSG_NODE_TRANSFORM 时的变换矩阵 */
        struct {
            double matrix[16];     /**< 4x4 仿射变换矩阵（列主序） */
        } transform;

        /* CSG_NODE_EXTRUDE_* 时的拉伸信息 */
        struct {
            PolygonEntity *profile; /**< 2D 轮廓多边形 */
            double height_or_angle; /**< 拉伸高度或旋转角度 */
            int segments;           /**< 旋转分段数 */
        } extrude;
    } data;

    /* CSG 树的层次结构 */
    struct CSGNode **children;     /**< 子节点数组 */
    int child_count;               /**< 子节点数量 */
    int child_capacity;            /**< 子节点数组容量 */

    /* 关联的 FuncBlock（如果由模块产生） */
    int func_block_id;             /**< 关联的函数块 ID，-1 表示直接构造 */

    /* 包围盒（轴对齐，3D） */
    struct {
        double x_min, y_min, z_min;
        double x_max, y_max, z_max;
    } bbox;

    /* 可视化属性 */
    char *color;                   /**< 颜色字符串（如 "red", "#FF0000"） */
    double alpha;                  /**< 透明度 (0.0 - 1.0) */
} CSGNode;
```

---

## 3. CSG 操作符链映射到 Lv-00 几何构造语言

### 3.1 CSG 三操作符的语义与 Lv-00 实现

OpenSCAD 的 CSG 核心是三个布尔操作符：

```
union()       { A; B; C; }    →  A ∪ B ∪ C  （所有子对象的体积并集）
difference()  { A; B; C; }    →  A \ (B ∪ C)（从第一个子对象中减去其余）
intersection(){ A; B; C; }    →  A ∩ B ∩ C  （所有子对象的公共体积）
```

**Lv-00 实现策略**：

| 操作符 | OpenSCAD 语义 | Lv-00 内部实现路径 | 复杂度 |
|--------|--------------|-------------------|--------|
| `geometry_csg_union()` | A ∪ B | 合并两个 CSG 树 → 生成并集 BSP 树 → 基于 BSP 输出网格 | O(n log n) |
| `geometry_csg_difference()` | A \ B | A 的 BSP 树 + 反转 B 的节点 → 合并 → 裁剪输出 | O(n log n) |
| `geometry_csg_intersection()` | A ∩ B | A 的 BSP 树 + B 的 BSP 树 → 取交 → 输出公共区域 | O(n log n) |

### 3.2 函数声明设计

三个函数追加到 `geometry_types.h`，作为 CSG 操作的核心 API：

```c
/**
 * @brief CSG 并集：返回 A 和 B 的布尔并
 *
 * 借鉴 OpenSCAD 的 union() 操作。
 * 从两棵 CSG 树构造一棵新的并集树，将两个子节点放入 children[0] 和 children[1]。
 *
 * 语义：result = A ∪ B
 * 内部构造 BSP（二叉空间分割）树进行网格布尔运算，
 * 生成的水密网格作为最终输出。
 *
 * @param[in] a  第一个 CSG 节点（不被修改，内部深拷贝）
 * @param[in] b  第二个 CSG 节点（不被修改，内部深拷贝）
 * @return 新的 CSG 树（调用者负责用 csg_node_destroy 释放），失败返回 NULL
 */
CSGNode *geometry_csg_union(const CSGNode *a, const CSGNode *b);

/**
 * @brief CSG 差集：返回 A 减去 B 的布尔差
 *
 * 借鉴 OpenSCAD 的 difference() 操作。
 * 从 A 的体积中减去 B 的体积。
 *
 * 语义：result = A \ B
 * 实现使用 BSP 树的裁剪算法：
 *   - 先将 A 和 B 分别转换为 BSP 树
 *   - 对 A 的 BSP 树进行"减 B"裁剪
 *   - 裁剪后的多边形重新三角剖分，生成输出网格
 *
 * 注意：如果 B 完全包含 A，结果为空的 CSG 树。
 *
 * @param[in] a  被减 CSG 节点（不被修改）
 * @param[in] b  减去 CSG 节点（不被修改）
 * @return 新的 CSG 树（调用者负责释放），失败返回 NULL
 */
CSGNode *geometry_csg_difference(const CSGNode *a, const CSGNode *b);

/**
 * @brief CSG 交集：返回 A 和 B 的布尔交
 *
 * 借鉴 OpenSCAD 的 intersection() 操作。
 * 取 A 和 B 的公共体积部分。
 *
 * 语义：result = A ∩ B
 * 实现使用 BSP 树裁剪 + 取交集策略：
 *   - 对 A 的 BSP 树裁剪 B，同时对 B 的 BSP 树裁剪 A
 *   - 取两次裁剪结果的交集（几何上等价于 A ∩ B 的边界）
 *
 * @param[in] a  第一个 CSG 节点（不被修改）
 * @param[in] b  第二个 CSG 节点（不被修改）
 * @return 新的 CSG 树（调用者负责释放），失败返回 NULL。若 A 与 B 不相交则返回空树。
 */
CSGNode *geometry_csg_intersection(const CSGNode *a, const CSGNode *b);
```

### 3.3 CSG 操作符链与 FuncBlock 的组合

在 Lv-00 DSL 中，CSG 操作符与 FuncBlock 自然组合：

```
// DSL: 参数化的铰链模型
funcblock hinge(radius : Number, height : Number, pin_radius : Number) -> CSGNode {
    // 主体圆柱
    csg_node body = csg_cylinder(r=radius, h=height);

    // 销孔（挖去）
    csg_node pin_hole = csg_translate(
        csg_cylinder(r=pin_radius, h=height * 1.2),
        dx=0, dy=0, dz=-height * 0.1
    );

    // 主体减去销孔
    csg_node result = geometry_csg_difference(body, pin_hole);
    return result;
}
```

编译后，编译器生成一个 `FuncBlock`，其内部节点包含三个 CSGNode（body, pin_hole, result），result 作为输出端口。函数调用 `hinge(5, 10, 1.5)` 会自动推导确定性。

### 3.4 与 geometry_types.h 现有类型的集成

CSG 操作符扩充了 `GeometryEntity` 的维度——从 2D 平面几何扩展到 3D 体积几何：

```
现有类型层次（2D）                  新增（3D CSG）
─────────────────────────────────────────────────
GeomEntity                          CSGNode
├── PointEntity (0维)               ├── CSG_NODE_PRIMITIVE (基本图元)
├── LinearEntity (1维)              ├── CSG_NODE_UNION (并集)
├── CircularEntity (1维曲线)        ├── CSG_NODE_DIFFERENCE (差集)
├── PolygonEntity (2维区域)         ├── CSG_NODE_INTERSECTION (交集)
├── TriangleEntity (2维区域)        ├── CSG_NODE_TRANSFORM (变换)
└── (Mesh???)                         ├── CSG_NODE_EXTRUDE_LINEAR (线性拉伸)
                                      ├── CSG_NODE_EXTRUDE_ROTATE (旋转拉伸)
                                      ├── CSG_NODE_HULL (凸包)
                                      └── CSG_NODE_MINKOWSKI (Minkowski 和)
```

---

## 4. WASM Web 移植双重验证路径

### 4.1 设计动机：为何需要双重验证

Lv-00 的核心精度诉求是"符号计算优先、数值落地方案可验证"。单一渲染后端存在信任问题：

| 问题 | 单后端风险 | 双后端好处 |
|------|-----------|-----------|
| 渲染偏差 | 某后端的重心定位误差可能不被发现 | 两个独立后端给出不同结果 = 信号需审查 |
| 浮点误差 | 不同引擎的三角剖分策略导致微小差异 | 交叉验证暴露浮点稳定性问题 |
| 3D → 2D 投影 | CSG 3D 模型的 2D 投影可能丢失关键特征 | GCLC 直接验证 2D 约束；OpenSCAD 验证 3D 体积 |
| 生态互操作 | 单后端难以与其他工具交换数据 | SCAD 文件可被 FreeCAD/Blender 导入验证 |

### 4.2 双重验证路径架构

```
Lv-00 .lvz DSL (几何构造 + 证明)
    │
    ├── 路径A: 2D 约束验证 (GCLC)
    │       │
    │       ├── 提取 2D 约束图 (ConstraintGraph)
    │       ├── 生成 GCLC 兼容的 .gc 文件
    │       └── 通过 GCLC 编译为 LaTeX/TikZ → 视觉验证
    │
    └── 路径B: 3D 体积验证 (OpenSCAD)
            │
            ├── 提取 CSG 布尔树 (CSGNode 层次)
            ├── 生成 OpenSCAD 兼容的 .scad 文件
            └── 通过 OpenSCAD-WASM 在线渲染 → 3D 预览验证
```

### 4.3 路径A：GCLC 验证（2D 约束正确性）

GCLC 路径验证 2D 平面几何约束是否满足：

```c
/**
 * @brief 将 Lv-00 约束图导出为 GCLC 兼容的 .gc 文件
 *
 * 从 ConstraintGraph 中提取 2D 几何构造（点、线、圆），
 * 生成 GCLC GC 语言的等价格式。
 *
 * 可用于：
 *  - 验证 Lv-00 的 2D 构造与 GCLC 的渲染结果是否一致
 *  - 生成高精度 LaTeX/TikZ 矢量图
 *  - 作为 Web 端 2D 预览的中间格式
 *
 * @param[in] graph   Lv-00 约束图
 * @param[in] filepath  输出 .gc 文件路径
 * @return 0 成功，-1 失败
 */
int csg_export_to_gclc(const ConstraintGraph *graph, const char *filepath);
```

**GCLC 验证流程**：

```
1. Lv-00 构造 2D 几何（三角形、中线等）
      │
2. 生成 .gc 文件
      │  包含：point A 0 0; point B 600 0; ...
      ▼
3. GCLC 编译器 (WASM)
      │  编译为 LaTeX/TikZ
      ▼
4. 渲染结果
      │  包含：几何图形 + 自动标注
      ▼
5. 对比验证
      │  - Lv-00 的 SymbolicCoord 坐标 vs GCLC 的计算坐标
      │  - Lv-00 的 Constraint 关系 vs GCLC 的 incidence 检查
      ▼
6. 验证通过 → 输出 GREEN；偏差 → 输出 AMBER 并报告差异
```

### 4.4 路径B：OpenSCAD 验证（3D 体积正确性）

OpenSCAD 路径验证 CSG 布尔操作的体积语义：

```c
/**
 * @brief 将 Lv-00 CSG 树导出为 OpenSCAD 兼容的 .scad 文件
 *
 * 递归遍历 CSGNode 树，为每个节点生成对应的 OpenSCAD 语法。
 *
 * 导出映射：
 *   CSG_NODE_PRIMITIVE  → cube/sphere/cylinder/polyhedron
 *   CSG_NODE_UNION      → union() { ... }
 *   CSG_NODE_DIFFERENCE → difference() { ... }
 *   CSG_NODE_INTERSECTION → intersection() { ... }
 *   CSG_NODE_TRANSFORM  → translate/rotate/scale
 *   CSG_NODE_EXTRUDE_*  → linear_extrude/rotate_extrude
 *
 * @param[in] root     CSG 树的根节点
 * @param[in] filepath  输出 .scad 文件路径
 * @return 0 成功，-1 失败
 */
int csg_export_to_openscad(const CSGNode *root, const char *filepath);
```

**OpenSCAD 验证流程**：

```
1. Lv-00 构造 3D CSG 树（如房屋、铰链）
      │
2. 生成 .scad 文件
      │  包含：module house(...); difference() { ... }
      ▼
3. OpenSCAD-WASM (浏览器内渲染)
      │  - 使用 WebAssembly 编译的 OpenSCAD
      │  - 3D 预览、STL 导出、测量工具
      ▼
4. 3D 验证
      │  - 体积计算：Lv-00 符号体积 vs OpenSCAD 数值体积
      │  - 表面网格：网格质量、水密性、非流形检测
      │  - 布尔正确性：手动检查 CSG 操作是否有几何错误
      ▼
5. 交叉验证
      │  - 取 CSG 树的 2D 截面 → 路径A (GCLC)
      │  - 比较截面的约束关系是否一致
      ▼
6. 双路径一致 → GREEN；单路径通过 → YELLOW；均失败 → RED
```

### 4.5 WASM Web 集成技术栈

| 组件 | 技术栈 | 在双重验证中的角色 |
|------|--------|-------------------|
| **Lv-00 核心引擎** | C (Emscripten → WASM) | 提供约束图求解 + CSG 树构造 |
| **GCLC 编译器** | C++ (Emscripten → WASM) | 路径A：2D LaTeX 验证渲染 |
| **OpenSCAD 引擎** | C++ (Emscripten → WASM) | 路径B：3D CSG 验证渲染 |
| **Web UI** | React/TypeScript | 加载 Lv-00 内核 + 双 WASM 后端，呈现结果 |
| **Three.js** | JavaScript | 3D 模型交互式预览（STL/OBJ 格式） |
| **MathJax** | JavaScript | LaTeX 公式渲染（证明步骤可视化） |

---

## 5. 完整示例：泰姬陵圆顶的 CSG 构造

### 5.1 问题描述

构造一个简化版泰姬陵圆顶模型，包含：
- 正方形基座 (cube)
- 圆顶 (半球 + 圆柱)
- 四个尖塔 (细长圆柱)
- 拱形入口 (差集操作)

### 5.2 Lv-00 DSL 源代码

```
// ============================================================
// Lv-00 DSL: 泰姬陵圆顶 CSG 构造
// 借鉴 OpenSCAD 语法，融合 FuncBlock 模块化
// ============================================================

// --- 1. 基本参数 ---
number base_w = 100;
number base_d = 100;
number base_h = 20;
number dome_r = 30;
number tower_r = 3;
number tower_h = 80;
number arch_w = 20;
number arch_h = 30;

// --- 2. 模块：尖塔 ---
funcblock tower(x : Number, y : Number) -> CSGNode {
    csg_node body = csg_cylinder(r=tower_r, h=tower_h);
    csg_node t    = csg_translate(body, dx=x, dy=y, dz=base_h);
    return t;
}

// --- 3. 模块：拱形入口 ---
funcblock arch_entry() -> CSGNode {
    // 矩形门洞
    csg_node rect = csg_translate(
        csg_cube(arch_w, arch_w * 0.3, arch_h),
        dx=-arch_w/2, dy=-base_d/2, dz=0
    );
    // 半圆拱顶
    csg_node arch_top = csg_translate(
        csg_cylinder(r=arch_w/2, h=arch_w*0.3),
        dx=0, dy=-base_d/2 + arch_w * 0.15, dz=arch_h
    );
    csg_node arch = csg_union(rect, arch_top);
    return arch;
}

// --- 4. 主体构造 ---
csg_node base     = csg_cube(base_w, base_d, base_h);

// 圆顶 = 半球 + 底座圆柱
csg_node dome_cyl = csg_translate(
    csg_cylinder(r=dome_r, h=15),
    dx=0, dy=0, dz=base_h
);
csg_node dome_cap = csg_translate(
    csg_sphere(r=dome_r),
    dx=0, dy=0, dz=base_h + 15
);
csg_node dome = csg_union(dome_cyl, dome_cap);

// 四个尖塔
csg_node t1 = tower(-base_w/2 + 5, -base_d/2 + 5);
csg_node t2 = tower( base_w/2 - 5, -base_d/2 + 5);
csg_node t3 = tower(-base_w/2 + 5,  base_d/2 - 5);
csg_node t4 = tower( base_w/2 - 5,  base_d/2 - 5);

// 拱形入口
csg_node arch = arch_entry();

// --- 5. 最终组合 ---
// 步骤: ((base + dome + towers) - arch)
csg_node superstructure = csg_union(csg_union(base, dome),
                                     csg_union(csg_union(t1, t2),
                                               csg_union(t3, t4)));
csg_node final = geometry_csg_difference(superstructure, arch);
```

### 5.3 编译后的内部 CSG 树

```
CSGNode 树结构 (final):
  kind: CSG_NODE_DIFFERENCE
  ├── children[0]: CSG_NODE_UNION
  │   ├── children[0]: CSG_NODE_UNION
  │   │   ├── children[0]: CSG_NODE_PRIMITIVE (cube: 100x100x20) [= base]
  │   │   └── children[1]: CSG_NODE_UNION
  │   │       ├── children[0]: CSG_NODE_PRIMITIVE (cylinder: r=30, h=15) [= dome_cyl]
  │   │       └── children[1]: CSG_NODE_PRIMITIVE (sphere: r=30) [= dome_cap]
  │   └── children[1]: CSG_NODE_UNION
  │       ├── children[0]: CSG_NODE_UNION (t1 + t2)
  │       └── children[1]: CSG_NODE_UNION (t3 + t4)
  └── children[1]: CSG_NODE_UNION [= arch]
      ├── children[0]: CSG_NODE_PRIMITIVE (cube: 20x6x30) [= rect]
      └── children[1]: CSG_NODE_PRIMITIVE (cylinder: r=10, h=6) [= arch_top]

关联 FuncBlock:
  - tower()    → 四个实例共享同一个 FuncBlock，参数不同
  - arch_entry() → 一个实例
```

### 5.4 双重验证输出

```
路径A (GCLC 2D 截面验证):
  截面位置: z = base_h + arch_h/2 (拱门中间高度)
  验证: 截面中四个尖塔的中心是否构成正方形
  验证: 拱门截面宽度是否等于 arch_w
  结果: GREEN — 2D 截面约束全部满足

路径B (OpenSCAD 3D 体积验证):
  体积计算: Lv-00 符号值 ≈ OpenSCAD 数值 (误差 < 1e-6)
  网格检查: 无水密性错误、无非流形边
  结果: GREEN — 3D 渲染一致，无几何错误

双路径交叉验证:
  取 OpenSCAD 3D 模型中 z=base_h+arch_h/2 截面
  → 导出为 2D 多边形
  → 加载到 GCLC 验证路径A 的约束
  结果: 双重验证通过 → PROOF_COLOR_GREEN
```

---

## 6. 编译器流水线与代码生成

### 6.1 编译阶段

```
Lv-00 .lvz DSL 文件 (含 3D CSG 语法)
    │
    ▼
┌─────────────────────────┐
│ Stage 1: 双层解析        │
│ · 2D 构造 (point/line)  │ → ConstraintGraph
│ · 3D CSG (csg_cube/     │   + CSGNode 树
│   csg_difference/...)   │
└────────┬────────────────┘
         │ 混合 AST
         ▼
┌─────────────────────────┐
│ Stage 2: 类型推演与绑定  │
│ · 2D 实体的 TypeRegion   │
│ · 3D CSGNode 的类型标记  │
│ · FuncBlock 输入/输出    │
│   端口类型解析           │
└────────┬────────────────┘
         │ 类型检查通过的 AST
         ▼
┌─────────────────────────┐
│ Stage 3: 2D 约束求解     │
│ · SymbolicCoord 符号坐标 │
│ · Solver 对 2D 构造求值  │
└────────┬────────────────┘
         │ 已求解的 2D 约束图
         ▼
┌─────────────────────────┐
│ Stage 4: 3D CSG 代码生成 │
│ · 将 CSGNode 树转换为    │
│   OpenSCAD .scad 语法    │
│ · 将 2D 截面约束转换为   │
│   GCLC .gc 语法          │
└────────┬────────────────┘
         │ .scad + .gc 文件
         ▼
┌─────────────────────────┐
│ Stage 5: 双重验证        │
│ · 路径A: GCLC-WASM 渲染  │
│ · 路径B: OpenSCAD-WASM   │
│ · 交叉结果比较            │
└────────┬────────────────┘
         │ 验证结果 (ProofColor)
         ▼
      [最终输出]
```

### 6.2 代码生成器映射表

| Lv-00 DSL 语法 | 生成的 OpenSCAD 代码 | 生成的 GCLC 代码 (2D 截面) |
|---------------|---------------------|--------------------------|
| `csg_cube(w, d, h)` | `cube([w, d, h]);` | 矩形 `polygon([[0,0],[w,0],[w,d],[0,d]])` |
| `csg_sphere(r=r)` | `sphere(r=r, $fn=32);` | 圆 `circle center r` |
| `csg_cylinder(r=r, h=h)` | `cylinder(r=r, h=h, $fn=24);` | 矩形（竖直截面） |
| `csg_union(A, B)` | `union() { A_scad; B_scad; }` | (不直接映射，需展平为多边形并集算法) |
| `csg_difference(A, B)` | `difference() { A_scad; B_scad; }` | 多边形差集算法 |
| `csg_intersection(A, B)` | `intersection() { A_scad; B_scad; }` | 多边形交集算法 |
| `csg_translate(N, dx,dy,dz)` | `translate([dx,dy,dz]) N_scad;` | 2D 平移（忽略 dz） |
| `csg_extrude_linear(P, h)` | `linear_extrude(height=h) polygon(P);` | (不适用，此操作创建 3D) |

---

## 7. 关键数据结构对照表

### 7.1 OpenSCAD → Lv-00 数据结构映射

| OpenSCAD 概念 | OpenSCAD 内部 | Lv-00 映射结构 | 文件 |
|--------------|-------------|---------------|------|
| `cube(size)` | `PrimitiveNode` | `CSGNode.kind=CSG_NODE_PRIMITIVE, primitive_type=0` | `geometry_types.h` (新增) |
| `sphere(r)` | `PrimitiveNode` | `CSGNode.kind=CSG_NODE_PRIMITIVE, primitive_type=1` | `geometry_types.h` (新增) |
| `cylinder(r,h)` | `PrimitiveNode` | `CSGNode.kind=CSG_NODE_PRIMITIVE, primitive_type=2` | `geometry_types.h` (新增) |
| `union()` | `CsgOpNode(type=UNION)` | `CSGNode.kind=CSG_NODE_UNION` | `geometry_types.h` (新增) |
| `difference()` | `CsgOpNode(type=DIFFERENCE)` | `CSGNode.kind=CSG_NODE_DIFFERENCE` | `geometry_types.h` (新增) |
| `intersection()` | `CsgOpNode(type=INTERSECTION)` | `CSGNode.kind=CSG_NODE_INTERSECTION` | `geometry_types.h` (新增) |
| `translate/rotate/scale` | `TransformNode(matrix)` | `CSGNode.kind=CSG_NODE_TRANSFORM` | `geometry_types.h` (新增) |
| `linear_extrude` | `LinearExtrudeNode` | `CSGNode.kind=CSG_NODE_EXTRUDE_LINEAR` | `geometry_types.h` (新增) |
| `module` | `ModuleInstantiation` | `FuncBlock` + `CSGNode.func_block_id` | `func_block.h` |
| `$fn` 精细度 | 全局常量 | `CSGNode` 构造时的 `segments` 参数 | `geometry_types.h` |
| CGAL 内核 | `CGAL_Nef_polyhedron` | BSP 树 + 三角剖分 (Lv-00 实现) | `csg_bsp.c` (新增) |
| `render()` | 强制 CGAL 预览 | Lv-00 编译时选项 `--csg-preview=full` | CLI 参数 |

### 7.2 CSG 操作对照表

| OpenSCAD 操作 | 语法 | Lv-00 函数 | 参数约定 |
|--------------|------|-----------|---------|
| 立方体 | `cube([w,d,h]);` | `csg_cube(w, d, h)` | 宽, 深, 高 |
| 球体 | `sphere(r=R, $fn=N);` | `csg_sphere(r=R, segments=N)` | 半径, 分段数 |
| 圆柱 | `cylinder(r=R, h=H, $fn=N);` | `csg_cylinder(r=R, h=H, segments=N)` | 半径, 高, 分段数 |
| 并集 | `union() { A; B; }` | `geometry_csg_union(A, B)` | 两个 CSGNode* 输入 |
| 差集 | `difference() { A; B; }` | `geometry_csg_difference(A, B)` | 两个 CSGNode* 输入 |
| 交集 | `intersection() { A; B; }` | `geometry_csg_intersection(A, B)` | 两个 CSGNode* 输入 |
| 平移 | `translate([x,y,z]) A;` | `csg_translate(A, x, y, z)` | 节点 + 平移量 |
| 旋转 | `rotate([ax,ay,az]) A;` | `csg_rotate(A, ax, ay, az)` | 节点 + 旋转角度 |
| 缩放 | `scale([sx,sy,sz]) A;` | `csg_scale(A, sx, sy, sz)` | 节点 + 缩放因子 |
| 线性拉伸 | `linear_extrude(h=H) polygon(P);` | `csg_extrude_linear(polygon, H)` | PolygonEntity + 高度 |
| 凸包 | `hull() { A; B; }` | `csg_hull(children, count)` | 子节点数组 + 数量 |

### 7.3 文件依赖关系

```
.lvz DSL 文件 (含 3D CSG 语法)
    │
    ├── geometry_types.h      (新增 CSGNode 类型 + CSG 函数声明)
    │   ├── PolygonEntity     (2D 轮廓 → 3D 拉伸)
    │   └── FlatStorage       (3D 网格顶点存储)
    │
    ├── csg_bsp.c             (新增，BSP 布尔运算引擎)
    │   ├── geometry_types.h  (CSGNode 输入/输出)
    │   └── FlatStorage       (网格输出存储)
    │
    ├── csg_codegen.c          (新增，代码生成器)
    │   ├── → OpenSCAD .scad   (export_openscad)
    │   └── → GCLC .gc         (export_gclc)
    │
    └── wasm_bridge.c          (新增，WASM 胶水层)
        ├── Emscripten bindings
        ├── GCLC-WASM 接口
        └── OpenSCAD-WASM 接口
```

---

## 附录 A：Lv-00 3D CSG DSL 完整 BNF 文法（简化版）

```
<csg_stmt>      ::= <csg_primitive>
                |   <csg_boolean>
                |   <csg_transform>
                |   <csg_extrude>
                |   <csg_funcblock_call>

<csg_primitive> ::= 'csg_cube' '(' <expr> ',' <expr> ',' <expr> ')'
                |   'csg_sphere' '(' 'r' '=' <expr> [',' '$fn' '=' <number>] ')'
                |   'csg_cylinder' '(' 'r' '=' <expr> ',' 'h' '=' <expr>
                    [',' '$fn' '=' <number>] ')'
                |   'csg_polyhedron' '(' <point_list> ',' <face_list> ')'

<csg_boolean>   ::= 'csg_union' '(' <csg_node> ',' <csg_node> {',' <csg_node>} ')'
                |   'csg_difference' '(' <csg_node> ',' <csg_node> {',' <csg_node>} ')'
                |   'csg_intersection' '(' <csg_node> ',' <csg_node> {',' <csg_node>} ')'

<csg_transform> ::= 'csg_translate' '(' <csg_node> ',' 'dx' '=' <expr>
                    ',' 'dy' '=' <expr> ',' 'dz' '=' <expr> ')'
                |   'csg_rotate' '(' <csg_node> ',' 'ax' '=' <expr>
                    ',' 'ay' '=' <expr> ',' 'az' '=' <expr> ')'
                |   'csg_scale' '(' <csg_node> ',' 'sx' '=' <expr>
                    ',' 'sy' '=' <expr> ',' 'sz' '=' <expr> ')'

<csg_extrude>   ::= 'csg_extrude_linear' '(' <polygon_expr> ',' 'height' '=' <expr> ')'
                |   'csg_extrude_rotate' '(' <polygon_expr> ',' 'angle' '=' <expr> ')'

<csg_funcblock_call> ::= <identifier> '(' <arg_list> ')'
```

---

> **文档结束**  
> 本文档详述了 OpenSCAD "脚本即 3D 模型"编译器范式如何映射到 Lv-00 DSL 设计，CSG 操作符链如何引入 Lv-00 几何构造语言，以及基于 GCLC+OpenSCAD 的 WASM Web 移植双重验证路径。
