# Lv-00 参考落地设计文档：Asymptote 矢量图形技术绘图语言

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: Asymptote (github.com/vectorgraphics/asymptote) —— C++ 风格的矢量图形编程语言
> **目标**: 将 Asymptote 的 C++ 内核 + 类 C++ 语法、3D 原生支持、路径一等对象模型、LaTeX 数学标注、交互式 3D 输出、模块化标准库六大核心特征映射到 Lv-00 几何元语言

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴点对照表](#2-核心借鉴点对照表)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)
5. [附录](#5-附录)

---

## 1. 项目概述

### 1.1 简介

Asymptote 是一门面向矢量图形技术绘图的编程语言，由 Andy Hammerlindl、John C. Bowman 和 Tom Prince 于 2001 年在阿尔伯塔大学发起。其设计初衷是取代 LaTeX 生态中传统的 `picture` 环境与 MetaPost——前者过于简陋，后者受限于宏展开范式——从而为数学家、物理学家和工程师提供一种"真正基于编程"的高质量图形输出工具。

与传统绘图工具的关键区别：

| 维度 | MetaPost（PSTricks/TikZ） | Asymptote |
|------|---------------------------|-----------|
| **语法范式** | LaTeX 宏展开，反斜杠 + 花括号 | 类 C++ 语法，分号、花括号、类型声明 |
| **计算引擎** | TeX 宏处理器（慢、受限） | C++ 编译型内核（快速、完整图灵完备） |
| **3D 支持** | 无或简陋（需手工计算投影） | 原生 3D：自动投影、消隐、光照计算 |
| **数学标注** | 依赖外部 LaTeX 调用 | 内置 LaTeX 渲染引擎，无缝嵌入 |
| **输出格式** | EPS/PDF（静态） | PDF、PNG、PRC（交互式 3D） |

### 1.2 技术栈

| 组件 | 技术 | 说明 |
|------|------|------|
| **计算内核** | C++ | 所有几何计算（求交、投影、光照）在 C++ 内核中编译执行 |
| **用户层语法** | 类 C++ | 类型声明 (`int`, `real`, `pair`, `triple`, `path`, `guide`)、控制流、函数定义 |
| **图形管线** | OpenGL (3D 预览) + Cairo/Agg (2D 后端) | 屏幕预览走 OpenGL，矢量输出走 Cairo/Agg |
| **字体 / 标注** | LaTeX 数学公式引擎 | 调用系统 LaTeX 安装，渲染数学字符串为图形路径 |
| **输出格式** | PDF / EPS / SVG / PNG / PRC / OBJ | PRC 格式支持内嵌交互式 3D 模型 |
| **标准库** | `asy` 模块（`.asy` 文件） | geometry.asy、graph.asy、three.asy 等标准模块 |
| **许可证** | LGPL 3.0 | 允许链接到的专有软件（宽松传染性许可） |

### 1.3 Lv-00 借鉴动机

Lv-00 目前聚焦于 2D 符号几何——构造约束、坐标求解、证明生成——但在以下方向存在能力缺口：

| 借鉴方向 | Asymptote 特性 | Lv-00 现有基础 | 差距与目标 |
|----------|---------------|---------------|-----------|
| **C 内核天然亲和** | C++ 内核计算 + 类 C++ 语法 | C 内核 (`ConstraintGraph`, `Solver`) | 导出后端可从 C 内核直接生成 Asymptote 源码 |
| **3D 高维可视化** | 原生 3D 投影/消隐/光照 | `high_dim.h` 模块（高维几何表示） | 缺失 3D 输出链路 |
| **路径一等对象** | `path p = (0,0)--(1,1)--cycle;` | `LinearEntity` / `PolygonEntity` 体系 | 需要路径变换、交点计算、面积求值等价映射 |
| **数学公式标注** | 内置 LaTeX 渲染 | 符号表达式 (`expr.h`) | 证明可视化需输出标注完备的几何图 |
| **PRC 交互式 3D** | PDF 嵌入式 3D 旋转/缩放 | 无 3D 交互体验 | Web GUI 可参考其交互模式 |
| **模块化标准库** | geometry / graph / three 模块 | `preset_*.h` 预设函数块体系 | 结构平行，可直接参照设计 |

### 1.4 总体架构对照

```
Asymptote                                Lv-00
───────────────────────────────────────────────────
.asy 源文件                               .lvz DSL 文件
  path p = (0,0)--(1,1);                   line l = segment(p1, p2);
  draw(p, blue+1pt);                       draw_line(l, color_blue);
  label("$x^2$", (0.5,0.5));               label_math("x^2", point(0.5, 0.5));

C++ 内核                                C 内核
  pair → 复数平面 (real, real)            SymbolicCoord → 符号坐标 (expr*)
  path → 三次 Bezier 节点链表              LinearEntity → 符号线实体
  three → 3D 投影矩阵 + PRC 写入           high_dim.h → 高维向量 + 投影

模块化标准库                              preset 预设函数块
  geometry.asy → 欧氏几何                  preset_triangle.h → 三角形构造
  graph.asy → 函数绘图                     preset_graph.h → 函数图构造
  three.asy → 3D 图元 + 变换               （待建）preset_3d.h → 3D 图元

输出格式                                 导出后端
  PDF / PRC / SVG / PNG / OBJ             export_gclc (2D) / export_asymptote (新增)
```

---

## 2. 核心借鉴点对照表

### 2.1 概览

Lv-00 从 Asymptote 中提取六个可直接映射的核心借鉴点，覆盖语法范式、计算模型、数据抽象、输出管道和工程组织五个维度。

| 编号 | 借鉴点 | Asymptote 原始设计 | Lv-00 对应概念 |
|------|--------|-------------------|---------------|
| **a** | C++ 内核 + 类 C++ 语法 | 计算在 C++ 中编译运行，语法非宏展开 | C 内核天然亲和，导出后端可直接生成源码 |
| **b** | 3D 原生支持 | `draw(sphere((0,0,0), 1))` —— 自动投影/消隐/光照 | `high_dim.h` 高维几何可视化链路 |
| **c** | path 作为一等对象 | `path p = (0,0)--(1,1)--(2,0)--cycle` | "几何构造即路径"——`LinearEntity` → 路径链 |
| **d** | LaTeX 数学标注 | 内置 LaTeX 渲染引擎，公式嵌入图形 | 证明可视化输出标注完备的几何图 |
| **e** | 交互式 3D (PRC) | 生成的 3D 图形支持旋转/缩放 | Web GUI 3D 交互参考 |
| **f** | 模块化仓库 | geometry.asy / graph.asy / three.asy | preset_*.h 预设函数块体系 |

### 2.2 借鉴点 a：C++ 内核 + 类 C++ 语法

**Asymptote 侧**：与 MetaPost 和 TikZ 不同，Asymptote 不依赖 TeX 宏展开机制。其语法是以 C++ 为蓝本的高级语言，支持强类型声明、结构体、函数（含重载）、操作符重载、引用和字符串类型。所有图形计算——从求交点 (`intersectionpoints`) 到圆弧弧长 (`arclength`)——均在 C++ 内核中编译并执行。

```asy
// Asymptote: 类 C++ 语法示例
import geometry;                              // 模块导入（类似 #include）
real a = 3, b = 4;                            // 显式浮点类型
pair A = (0,0), B = (a,0);                   // 复平面点对 = (real, real)
var C = rotate(30, A) * B;                   // 绕 A 旋转 B 30°
triangle t = triangle(A, B, C);               // 三角形对象
draw(t, linewidth(bp)+red);                  // 带属性的绘制
label("$c$", midpoint(t.BC), NW);            // 中点 + 方向标注
```

**Lv-00 侧对应**：Lv-00 的 C 内核 (`geometry.c`, `solver.c`, `rewrite.c`) 天然与 Asymptote 的 C++ 内核亲和。几何计算的结果结构体 (`SymbolicCoord`, `LinearEntity`, `CircularEntity`, `ConstraintGraph`) 可直接映射为 Asymptote 的 `pair` / `path` / `circle` 类型。这意味着 Lv-00 可以**直接将约束求解后的几何数据注入 Asymptote 语法模板**——不需要中间转换层。

```c
// Lv-00 侧: 从 C 内核直接生成 Asymptote 源码
void geometry_export_asymptote_pair(FILE *f, SymbolicCoord *c) {
    // SymbolicCoord 中的符号坐标转换为 Asymptote pair
    fprintf(f, "triple A = (%.6f, %.6f, %.6f);\n",
            coord_eval_x(c), coord_eval_y(c), coord_eval_z(c));  // 3D 时 Z=0
}

void geometry_export_asymptote_linear(FILE *f, LinearEntity *e) {
    // 线实体转换为 Asymptote path guide
    fprintf(f, "draw(");
    fprintf(f, "(%.6f,%.6f)--", e->p1.x, e->p1.y);
    fprintf(f, "(%.6f,%.6f)",   e->p2.x, e->p2.y);
    fprintf(f, ");\n");
}
```

### 2.3 借鉴点 b：3D 原生支持

**Asymptote 侧**：Asymptote 的 `three` 模块提供了完整的 3D 图形栈，包括 `projection`（正交/透视投影）、`light`（位置/颜色/角度）、`currentlight`（全局照明模型）、基于画家算法 + BSP 树的消隐，以及平面着色 (`flat`) 和 Gouraud 着色 (`Gouraud`)。

```asy
// Asymptote 3D: 球体 + 光照 + 投影
import three;
size(200);
currentprojection = perspective(5,4,2);       // 透视投影
currentlight = light(diffuse=white, specular=red,
                     (1,2,4));                // 光照（位置+颜色）
draw(sphere((0,0,0), 1), surfacepen=blue);    // 蓝色球体
draw(unitbox, dashed);                        // 单位包围盒
```

**Lv-00 侧对应**：`high_dim.h` 模块已经维护了 `VectorND`（N 维向量）、高维几何体的内部表示。将 N 维几何体投影到 3D（或直接提取 3D 组件）后，即可通过 Asymptote 的 `triple` (3D point) 和 `three` 模块导出。对于 Lv-00 的高维点 `VectorND`，在导出时采用**切片投影法**——只取前三个维度作为 `triple`，用 `matrix` 变换表达更高维度的旋转/投影效果。

```
Lv-00 high_dim.h                      Asymptote three.asy
───────────────────────────────────────────────────────────
VectorND v(5)  →  (v[0], v[1], v[2])  triple V = (x, y, z);
N 维单纯形        →  3D tetrahedron    draw(tetrahedron(...));
N 维超平面交      →  3D 截面线         draw(intersection...);
旋转矩阵(N×N)     →  前 3×3 子块      transform T = rotate(angle, axis);
```

### 2.4 借鉴点 c：路径（path）作为一等对象

**Asymptote 侧**：在 Asymptote 中，`path` 是一种一等类型，可以赋值给变量、作为函数参数/返回值，支持丰富的操作符：

```asy
// Asymptote: path 一等对象
path p = (0,0)--(1,1)--(2,0)--cycle;        // 三角路径
path q = rotate(45) * p;                     // 旋转变换
path r = p & reverse(p);                     // 路径拼接
pair[] pts = intersectionpoints(p, q);       // 交点计算
real arclen = arclength(p);                  // 弧长
real area = area(p);                         // 有向面积
path sub = subpath(p, 0.25, 0.75);           // 子路径（参数化截取）
```

**Lv-00 侧对应**：Lv-00 的"几何构造即路径"概念与 Asymptote 的 `path` 模型高度一致。`LinearEntity` 是路径的基本段，`PolygonEntity` 是闭合路径，`CircularEntity` 是弧段。一套完整的路径转换等价关系如下：

| path 操作 | Asymptote 语法 | Lv-00 等价函数 | 语义 |
|-----------|---------------|---------------|------|
| 路径构造 | `path p = A--B--C--cycle;` | `polygon_from_points(pts, 3)` / `PolygonEntity` | 多点顺序连接 |
| 拼接连线 | `path r = p & q;` | `path_concat(p, q)` → 合并实体列表 | 两条路径首尾相接 |
| 反向 | `path r = reverse(p);` | `path_reverse(p)` → 反转顶点顺序 | 反转方向 |
| 旋转变换 | `path q = rotate(45)*p;` | `path_transform_rotate(p, 45, center)` | 几何变换 |
| 交点计算 | `pair[] pts = intersectionpoints(p1,p2);` | `geom_entity_intersect(e1, e2)` → `PointEntity[]` | 求两对象交点 |
| 面积 | `real a = area(p);` | `polygon_area(p)` → 有向面积（通过 Shoelace 公式） | 面积计算 |
| 子路径 | `subpath(p, t0, t1)` | `path_subpath(p, t0, t1)` → 参数化截取 | 截取部分路径 |

### 2.5 借鉴点 d：LaTeX 数学标注

**Asymptote 侧**：Asymptote 内置 LaTeX 渲染引擎。`label("$...$", position, direction)` 调用系统 LaTeX 安装，将数学字符串栅格化为图形路径（Bezier 曲线），然后作为图形元素嵌入输出文件。

```asy
// Asymptote LaTeX 标注
import graph;
size(200);
draw(circle((0,0), 2));
draw((-2.5,0)--(2.5,0), Arrow);              // 坐标轴 + 箭头
label("$\sin(x)$", (pi, sin(pi)), NE);       // 数学公式标注
label("$\frac{1}{2} \int_0^\infty e^{-x^2} dx$",
      (2, 1.5), fontsize(12pt));              // 复杂积分公式
```

**Lv-00 侧对应**：Lv-00 的证明模块 (`proof.h`) 在证明步骤中引用几何元素。通过 Asymptote 导出后端，可以为**证明可视化**输出包含完整数学标注的图形——每个标注点（如线段比例条件、角度值）都以 LaTeX 公式嵌入图形。

```c
/**
 * @brief 将 Lv-00 证明步骤中的标注信息注入 Asymptote label 语句
 *
 * 语义：每条 proof_step 中的标注（如"∠A = 60°"、"AB:BC = 2:3"）
 * 自动映射为 Asymptote 的 label("$\\angle A = 60^\\circ$", A, NE)
 *
 * @param[in] step    证明步骤（含标注文本 + 引用位置）
 * @param[in] f       输出 FILE*
 */
void proof_label_export_asymptote(ProofStep *step, FILE *f) {
    for (int i = 0; i < step->annotation_count; i++) {
        Annotation *ann = step->annotations[i];
        // 将 LaTeX 字符串写入 label 命令
        fprintf(f, "label(\"$%s$\", (%f,%f), NE);\n",
                ann->latex_text,
                ann->position.x, ann->position.y);
    }
}
```

### 2.6 借鉴点 e：交互式 3D（PRC 格式）

**Asymptote 侧**：Asymptote 可生成嵌入 PDF 的 PRC（Product Representation Compact）3D 模型，用户打开 PDF 后可直接用鼠标旋转、缩放、平移 3D 图形——无需安装任何 3D 查看器。

```asy
// Asymptote PRC 交互式 3D
import three;
settings.prc = true;                          // 启用 PRC 输出
settings.render = 0;                          // 0 = 仅 PRC 矢量，非栅格化
draw(unitcube, blue+opacity(0.5));
draw(unitcircle3, red);
// 用户可在 PDF 阅读器中旋转/缩放此立方体
```

**Lv-00 侧对应**：Lv-00 的 Web GUI (React/TypeScript) 可参考 PRC 的交互模式——基于 `high_dim.h` 的 3D 降维投影结果，通过 Three.js 渲染交互式 3D 预览。参考的交互特性包括：

| PRC 交互特性 | Lv-00 Web GUI 对应 |
|-------------|-------------------|
| 鼠标旋转（orbit） | Three.js `OrbitControls` |
| 滚轮缩放 | `OrbitControls.zoomSpeed` |
| 中键平移 | `OrbitControls.panSpeed` |
| 标注保持朝向 | Three.js `CSS2DRenderer` 标签 |
| 多视图（正视图/侧视图/俯视图） | 预设相机位置快捷键 |
| 截面剖切 | 裁剪平面 (`THREE.Plane`) |
| 测量工具（距离/角度） | 点击两点自动计算并标注 |

### 2.7 借鉴点 f：模块化仓库

**Asymptote 侧**：官方标准库 `asy/` 目录下按领域分模块组织，每个模块是独立的 `.asy` 文件，模块内定义结构体和函数，模块间可交叉引用：

```
Asymptote 标准库结构
├── geometry.asy        // 欧氏几何工具
│     triangle, circle, line, conic
│     intersection(), tangent(), circumcircle()
├── graph.asy           // 函数绘图
│     guide, graph(), axis()
├── three.asy           // 3D 图形
│     sphere(), cylinder(), cone()
│     orthographic(), perspective()
├── math.asy            // 数学函数
│     abs(), exp(), sin(), cos()
├── palette.asy         // 色板
│     Rainbow(), Grayscale()
└── markers.asy         // 标记样式
      StickMarker(), TickMarker()
```

**Lv-00 侧对应**：`preset_*.h` 预设函数块体系与 Asymptote 标准库的结构完全平行：

| Asymptote 模块 | Lv-00 preset 预设函数块 | 对应几何概念 |
|---------------|------------------------|-------------|
| `geometry.asy` | `preset_triangle.h` | 三角形、垂心、重心 |
| (同上) | `preset_quadrilateral.h` | 四边形、平行四边形 |
| (同上) | `preset_circle.h` | 圆、弧度、切线 |
| `graph.asy` | `preset_graph.h` (待建) | 函数图、坐标轴 |
| `three.asy` | `preset_3d.h` (待建) | 3D 图元、投影 |
| `math.asy` | `expr.h` (已有) | 符号数学表达式 |

---

## 3. Lv-00 映射方案

### 3.1 总体设计：新增 Asymptote 导出后端

在 Lv-00 的导出管道中新增一条 Asymptote 后端分支。具体做法是提供一个顶层导出函数，以约束图 (`ConstraintGraph*`) 为输入，以 Asymptote 源码文件为输出。

```c
/**
 * @brief 将 Lv-00 几何约束图导出为 Asymptote 源码文件
 *
 * 遍历 ConstraintGraph 中的所有几何实体（点、线、圆、多边形、高维对象），
 * 逐实体生成对应的 Asymptote 语法，输出一个完整的 .asy 文件。
 *
 * 导出策略：
 *   - PointEntity   → pair P = (x, y);
 *   - LinearEntity  → path p = A--B;  draw(p);
 *   - CircularEntity → circle c = circle(C, r);  draw(c);
 *   - PolygonEntity → path p = A--B--...--cycle;  draw(p);
 *   - VectorND (3D) → triple V = (x, y, z);
 *   - proof 标注    → label("$expr$", pos, dir);
 *   - 颜色映射      → entity_color → Asymptote pen
 *
 * 同时支持两种渲染模式（通过 settings 变量控制）：
 *   - settings.render = 0: 纯 PRC 交互式 3D 输出
 *   - settings.render = 4: 栅格化后嵌入 PDF
 *
 * @param[in] graph    Lv-00 约束图（含所有几何实体 + 证明步骤）
 * @param[in] filepath  输出 .asy 文件路径
 * @param[out] mode   输出模式（ASY_MODE_2D 或 ASY_MODE_3D）
 * @return 0 成功，-1 失败
 */
int geometry_export_asymptote(const ConstraintGraph *graph,
                              const char *filepath,
                              int mode);
```

### 3.2 模块位置

建议在现有目录结构中，于 `NarrativeExport` 模块（或新建 `geometry_export` 模块）下增加 Asymptote 导出后端。

```
src/
├── export/
│   ├── geometry_export_gclc.c       (已有，GCLC 导出)
│   ├── geometry_export_tikz.c       (已有，TikZ 导出)
│   └── geometry_export_asymptote.c  (新增，Asymptote 导出)
├── include/
│   ├── geometry_export_gclc.h       (已有)
│   ├── geometry_export_tikz.h       (已有)
│   └── geometry_export_asymptote.h  (新增)
└── ...
```

### 3.3 实体类型映射表

| Lv-00 实体类型 | Asymptote 导出代码 | 说明 |
|---------------|-------------------|------|
| `PointEntity p(x, y)` | `pair P = (x, y);` | 点映射为 `pair` |
| `PointEntity p(x, y, z)` | `triple P = (x, y, z);` | 3D 点映射为 `triple` |
| `LinearEntity A→B` | `draw(A--B);` | 线段映射为 `path` 绘制 |
| `CircularEntity C, r` | `draw(circle(C, r));` | 圆映射为 `circle` |
| `PolygonEntity pts[0..n-1]` | `draw(A--B--C--cycle);` | 多边形映射为闭合 `path` |
| `PolygonEntity (填充)` | `filldraw(path, fillpen, drawpen);` | 填充 + 描边 |
| `SymbolicCoord` | `p = (c_x, c_y);` | 符号坐标求值后输出 |
| `VectorND` (N≥3) | `triple V = (v[0], v[1], v[2]);` | 取前三维 |
| `ProofStep 标注` | `label("$text$", pos, dir);` | LaTeX 公式标注 |
| `EntityColor` | `pen p = rgb(r,g,b);` | 颜色映射为 `pen` |

### 3.4 笔触（Pen）映射

Asymptote 使用 `pen` 类型统一表示绘制属性（颜色、线宽、线型、透明度）。Lv-00 的 `EntityColor` + 线宽/线型需映射为 Asymptote `pen`：

```c
/**
 * @brief 将 Lv-00 绘制属性转换为 Asymptote pen 声明
 */
void pen_export_from_lv(FILE *f, EntityColor *color,
                        double linewidth, LineStyle style) {
    fprintf(f, "pen p = rgb(%d,%d,%d)", color->r, color->g, color->b);
    if (linewidth > 0) {
        fprintf(f, "+linewidth(%.1f)", linewidth);
    }
    switch (style) {
        case LINE_DASHED:  fprintf(f, "+dashed");   break;
        case LINE_DOTTED:  fprintf(f, "+dotted");   break;
        case LINE_SOLID:   /* default, no modifier */ break;
    }
    fprintf(f, ";\n");
}
```

### 3.5 完整示例：三角形中线定理的 Asymptote 导出

**Lv-00 构造输入**：在三角形 ABC 中，构造中线 AD（D 为 BC 中点）。

**导出的 Asymptote 源码**：

```asy
// ============================================================
// Asymptote 导出: 三角形中线定理可视化
// 生成自: Lv-00 geometry_export_asymptote()
// ============================================================
import geometry;
size(250);
unitsize(1cm);

// --- 点定义 ---
pair A = (0.000000, 0.000000);
pair B = (3.000000, 0.000000);
pair C = (2.000000, 2.500000);
pair D = (2.500000, 1.250000);   // BC 中点

// --- 三角形构造 ---
triangle t = triangle(A, B, C);
draw(t, linewidth(1.2)+black);

// --- 中线 ---
path median = A--D;
draw(median, linewidth(0.8)+blue+dashed);

// --- 标注 ---
dot("$A$", A, SW);
dot("$B$", B, SE);
dot("$C$", C, N);
dot("$D$", D, SE, red);          // D = 中点

// --- 数学标注 ---
label("$|BD| = |DC|$", midpoint(B--C), S);
label("$AD \text{ 为中线}$", (1.8, 2.0), fontsize(9pt));

// --- 证明辅助线 ---
draw(bisector(t), green+linewidth(0.4));

shipout("triangle_median");
```

---

## 4. 实现路线图

### 4.1 分阶段计划

Asymptote 导出后端的实现分两个阶段推进，优先完成 2D 导出以打通全链路，第二阶段扩展 3D 导出以覆盖高维可视化需求。

| 阶段 | 目标 | 核心任务 | 预计产物 | 依赖 |
|------|------|---------|---------|------|
| **Phase 1** | 2D 导出打通全链路 | 几何体 → Asymptote path → PDF 静态图形 | `geometry_export_asymptote.c` + `.h` | 已有 `ConstraintGraph`、实体类型体系 |
| **Phase 2** | 3D 导出 + 交互 | 高维模块 → Asymptote three 模块 → 交互式 3D | `asymptote_3d_export.c` + Web GUI 集成 | `high_dim.h`、Phase 1 完成 |

### 4.2 Phase 1：2D 导出（几何体 → Asymptote path → PDF 静态图形）

**目标**：将 Lv-00 的全部 2D 几何实体（点、线、圆、多边形）及其证明标注，准确、完整地导出为 Asymptote 源码，编译后生成 PDF 矢量图形。

**实现任务清单**：

| 编号 | 任务 | 说明 | 优先级 |
|------|------|------|--------|
| T1.1 | 创建 `geometry_export_asymptote.h` | 声明 `geometry_export_asymptote()` 及辅助函数原型 | P0 |
| T1.2 | 实现点实体导出 | `PointEntity` → `pair P = (x,y);` + `dot()` 标注 | P0 |
| T1.3 | 实现线段实体导出 | `LinearEntity` → `draw(A--B, pen);` | P0 |
| T1.4 | 实现圆实体导出 | `CircularEntity` → `draw(circle(C, r), pen);` | P0 |
| T1.5 | 实现多边形导出 | `PolygonEntity` → `draw(A--B--...--cycle);` | P1 |
| T1.6 | 实现填充/颜色映射 | `EntityColor` → Asymptote `pen` + `filldraw()` | P1 |
| T1.7 | 实现证明标注导出 | `ProofStep` → `label("$...$", pos, dir)` | P1 |
| T1.8 | 实现导出文件模板 | `.asy` 文件头（`import geometry;`、`size()`、`shipout()` 等） | P0 |
| T1.9 | 编写单元测试 | 三角形、四边形、圆相切等典型几何的正确性验证 | P2 |
| T1.10 | 集成 CLI 参数 | `--export-asymptote=output.asy` 命令行开关 | P1 |

**Phase 1 完成标志**：
- 任意 Lv-00 构造的 2D 几何图可一键导出 `.asy` 文件
- 导出的 `.asy` 文件经 `asy -f pdf output.asy` 编译后生成可阅读的 PDF 矢量图
- 标注完备（点、线、角、比例关系）

### 4.3 Phase 2：3D 导出（高维模块 → Asymptote three 模块 → 交互式 3D）

**目标**：将 `high_dim.h` 中的高维几何对象降维至 3D，通过 Asymptote 的 `three` 模块导出，并可选择输出 PRC 交互式 3D 模型或静态栅格化图形。

**实现任务清单**：

| 编号 | 任务 | 说明 | 优先级 |
|------|------|------|--------|
| T2.1 | 实现 3D 点导出 | `VectorND` → `triple V = (x,y,z);` 取前三维 | P0 |
| T2.2 | 实现 3D 图元导出 | 匹配 Asymptote `sphere()` / `cylinder()` / `cone()` 等 | P1 |
| T2.3 | 实现投影模式控制 | `orthographic()` vs `perspective()` 参数映射 | P1 |
| T2.4 | 实现光照参数导出 | `currentlight = light(...)` 映射 | P2 |
| T2.5 | 实现 PRC 开关 | `settings.prc = true` / `settings.render = 0` | P1 |
| T2.6 | 实现 Web GUI 3D 预览 | Three.js 渲染 Asymptote 生成的 OBJ/STL 数据 | P2 |
| T2.7 | 实现 3D 标注 | `label("$...$", triple_pos, projection)` | P1 |
| T2.8 | 集成 Web 预览流水线 | CLI → Asymptote → 3D 数据提取 → Web 展示 | P3 |

**Phase 2 完成标志**：
- `high_dim.h` 中的四面体、超立方体投影等可导出为 Asymptote 3D 图形
- PDF 中包含交互式 PRC 3D 模型（鼠标可旋转/缩放）
- Web GUI 中可预览 3D 几何体

### 4.4 路线图时间线

```
Phase 1: 2D 导出                           Phase 2: 3D 导出 + 交互
├─────────────────────────────────────────┼─────────────────────────────────┤
T1.1 头文件声明                            T2.1 3D 点导出
├── T1.2-T1.4 核心实体导出 (P0)             ├── T2.2 3D 图元导出
├── T1.5-T1.7 高级特性 (P1)                 ├── T2.3 投影控制
├── T1.8 文件模板 (P0)                      ├── T2.4 光照参数
├── T1.9 测试 (P2)                          ├── T2.5 PRC 开关
└── T1.10 CLI 集成 (P1)                     ├── T2.6 Web GUI 3D 预览
                                            ├── T2.7 3D 标注
                                            └── T2.8 Web 流水线集成
```

---

## 5. 附录

### 附录 A：Asymptote 关键语法速查表

| 功能 | Asymptote 语法 | 等效于 |
|------|---------------|--------|
| 点 | `pair P = (x, y);` | 2D 向量 |
| 3D 点 | `triple V = (x, y, z);` | 3D 向量 |
| 线段 | `draw(A--B);` | 两点连线 |
| 折线 | `draw((0,0)--(1,0)--(1,1)--cycle);` | 多段路径 |
| 曲线 | `draw(A..B..C);` | Bezier 平滑曲线 |
| 变换 | `draw(rotate(45)*p);` | 旋转变换 |
| 平移 | `draw(shift(1,2)*p);` | 平移变换 |
| 缩放 | `draw(scale(2)*p);` | 缩放变换 |
| 并 | `path r = p & q;` | 路径拼接 |
| 反 | `path r = reverse(p);` | 路径反转 |
| 交点 | `pair[] pts = intersectionpoints(p, q);` | 求两个 path 的全部交点 |
| 面积 | `real a = area(p);` | 有向面积 |
| 圆 | `draw(circle(C, r));` | 圆心+半径 |
| 标注 | `label("$x^2$", pos);` | LaTeX 数学嵌入 |
| 标点 | `dot("$A$", pos, SW);` | 点 + 标签 |
| 填充 | `fill(p, red);` | 区域填充 |
| 三角形 | `triangle t = triangle(A,B,C);` | 欧氏三角形对象 |
| 中点 | `pair M = midpoint(A--B);` | 线段中点 |

### 附录 B：Lv-00 导出流水线架构

```
Lv-00 内部流程                        Asymptote 导出后端
═══════════════════════════════════    ═══════════════════════════════════

  用户输入的几何构造
  （自然语言或 DSL）
        │
        ▼
  ┌──────────────┐
  │ 语法解析      │
  │ (parser.c)    │
  └──────┬───────┘
         │ AST
         ▼
  ┌──────────────┐
  │ 类型推演      │
  │ (type_check.c)│
  └──────┬───────┘
         │ 类型完备 AST
         ▼
  ┌──────────────┐
  │ 约束图构造    │                 ┌──────────────────────────┐
  │ (constraint.c)│                 │ geometry_export_asymptote │
  └──────┬───────┘                 │                          │
         │ ConstraintGraph         │  遍历 ConstraintGraph     │
         ▼                          │    ├── PointEntity        │
  ┌──────────────┐                  │    │   → pair P = (...)   │
  │ 符号坐标求解  │                  │    ├── LinearEntity      │
  │ (solver.c)    │  ───────────→   │    │   → draw(A--B);     │
  └──────┬───────┘                  │    ├── CircularEntity     │
         │ 已求解坐标               │    │   → circle(C,r)      │
         ▼                          │    ├── PolygonEntity      │
  ┌──────────────┐                  │    │   → filldraw(...)    │
  │ 证明生成      │                  │    ├── proof 标注         │
  │ (proof.c)     │                  │    │   → label("$...$")  │
  └──────┬───────┘                  │    └── 属性映射           │
         │ 证明步骤                  │        → pen/arrow/color │
         ▼                          └──────────┬───────────────┘
  ┌──────────────┐                             │ .asy 文件
  │ 导出后端调度   │ ◄─────────────────────     │
  │ (export.c)    │                             ▼
  └──────┬───────┘                  ┌──────────┴───────────────┐
         │                          │ Asymptote 编译器 (asy)    │
         ├─→ GCLC       (.gc)       │  ─→ PDF/PRC/SVG/PNG      │
         ├─→ TikZ       (.tex)      │  ─→ 静态 2D + 交互 3D    │
         ├─→ Asymptote  (.asy)  ◄───┘                          │
         └─→ (未来: HTML/Three.js)  └──────────────────────────┘
```

### 附录 C：与现有导出后端的对比

| 特性 | GCLC 导出 | TikZ 导出 | Asymptote 导出 (本方案) |
|------|----------|-----------|------------------------|
| **2D 几何精度** | 高（与 Lv-00 约束求解结果 1:1 对应） | 高 | 高（C 内核直接映射 pair） |
| **3D 支持** | 无 | 无 | 原生（three 模块，PRC 交互） |
| **数学标注** | 有限（LaTeX 输出需单独处理） | 优秀（天然 LaTeX 集成） | 优秀（内置 LaTeX 渲染引擎） |
| **编程语言亲和** | 弱（声明式约束语言） | 弱（LaTeX 宏） | 极强（类 C++，与 Lv-00 C 内核天然对应） |
| **输出格式** | LaTeX 编译图 | PDF/PNG | PDF/PRC/SVG/PNG/OBJ |
| **交互性** | 无 | 无 | PRC 3D 旋转/缩放 |
| **现有成熟度** | 已有 | 已有 | 新建 |

### 附录 D：核心引用对照表

| Asymptote 概念 | 官方文档章节 | Lv-00 对应位置 | 映射方式 |
|---------------|-------------|---------------|---------|
| `pair` (2D 点) | 3.2 Variables | `SymbolicCoord` / `PointEntity` | 符号坐标求值后输出 `(x, y)` |
| `triple` (3D 点) | 6.7 Three-dimensional drawing | `VectorND` (取前三维) | `(v[0], v[1], v[2])` |
| `path` (路径) | 5 Paths and guides | `LinearEntity` + `CircularEntity` + `PolygonEntity` | 实体链合成 `path` |
| `guide` (引导线) | 5.1 Guides | 无直接等价 | 可选：Bézier 插值路径 |
| `transform` (仿射变换) | 5.2 Linear transforms | `Matrix4x4` / 变换矩阵 | 变换矩阵反推 Asymptote 操作 |
| `pen` (画笔) | 4 Pens | `EntityColor` + `linewidth` + `linestyle` | 复合 `pen` 构造 |
| `label` (标注) | 4.4 Labels | `ProofStep.annotations[]` | LaTeX 字符串注入 |
| `camera` / `projection` | 6.14 Camera and projection | `high_dim.h` 投影参数 | 正交/透视投影映射 |
| `light` / `currentlight` | 6.17 Lighting | 无（新增可选参数） | 全局光照对象 |
| `shipout` (输出) | 7.1 Shipout | 导出后端 write 调用 | `.asy` 文件结尾 |

---

> **文档结束**
> 本文档详述了 Asymptote 矢量图形技术绘图语言的 C++ 内核 + 类 C++ 语法的六大核心借鉴点，以及如何通过新增 `geometry_export_asymptote()` 导出后端，将 Lv-00 的约束图与证明标注映射为 Asymptote 源码，实现高质量的 2D 矢量图形与 3D 交互式几何可视化。
