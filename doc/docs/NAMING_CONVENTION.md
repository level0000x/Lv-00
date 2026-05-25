# Lv-00 几何对象命名与引用规范

> 版本：v1.0 | 最后更新：2026-05-23
>
> 本规范受 GeoGebra 的对象命名系统启发，并结合 Lv-00 作为几何元语言的独特定位（参见 `competitive_analysis.md` P3 行动项）而制定。

---

## 目录

1. [概述](#1-概述)
2. [点的命名](#2-点的命名)
3. [线与线段的命名](#3-线与线段的命名)
4. [区域的命名](#4-区域的命名)
5. [约束的命名](#5-约束的命名)
6. [函数块的命名](#6-函数块的命名)
7. [端口的命名](#7-端口的命名)
8. [公理的命名](#8-公理的命名)
9. [证明步骤的命名](#9-证明步骤的命名)
10. [与竞品的对比](#10-与竞品的对比)

---

## 1. 概述

### 1.1 设计目标

Lv-00 为几何对象建立了一套一致、可预测的命名体系，服务于以下三个目标：

- **人机共识**：同一命名规则既适合人类阅读推理，也适合机器解析处理。用户书写的点 `A`，在约束图、求解器、证明器中始终是同一个标识符。
- **可推导性**：从命名即可推断对象的类型、来源和依赖关系。例如 `M_AB` 一眼可知是线段 AB 的中点。
- **多体系兼容**：命名不绑定于特定几何体系（欧氏、双曲、椭圆），也不绑定于特定逻辑框架（集合论、类型论），确保公理中立的设计得以贯彻。

### 1.2 核心原则

| 原则 | 说明 | 反例 |
|:---|:---|:---|
| **可读性优先** | 命名应让人类读者无需查表即可理解含义 | `obj_12345` |
| **类型可辨** | 从命名能区分点、线、区域等类型 | 混用 `x` 表示点和线 |
| **来源可溯** | 构造型对象的命名体现其构造参数 | 无名中间变量 |
| **体系无关** | 命名不隐含特定几何体系的前提 | `euc_dist`（隐含欧氏度量） |
| **ASCII 兼容** | 核心标识符使用 ASCII 字符，Unicode 用于展示层 | 存储层用 Unicode 作为主键 |

---

## 2. 点的命名

点是几何构造的基本原子。Lv-00 将点分为三类，分别规定命名方式。

### 2.1 自由点

自由点是由用户直接放置、不受任何约束决定的点。使用**大写拉丁字母**命名。

```
语法：单个大写拉丁字母
字符集：A-Z（不含小写）
保留字：O, I 有特定语义（见 2.3 节）
```

**示例：**

```
A, B, C, P, Q, X, Y, Z          — 通用自由点
A1, A2, B1, B2                   — 同系列自由点（字母 + 数字后缀）
```

**规则细节：**

- 单个大写字母默认为自由点。若在上下文中作为构造点出现，则自动推断为构造点（见 2.2）。
- 数字后缀 `A1` 等价于 `A₁` 的 ASCII 表示。展示层可渲染为下标形式 `A₁`。
- 连续选取：引擎自动跟踪已用字母。若当前上下文中 `A`、`B` 已被使用，下一个自由点默认为 `C`。

### 2.2 依赖点（构造点）

依赖点是通过几何构造由其他对象生成的点。命名采用**功能前缀 + 下划线 + 参数**的格式，使来源一目了然。

```
语法：[功能前缀]_[参数列表]
功能前缀：描述构造操作（见下表）
参数列表：用下划线连接的父对象名称
```

**常用构造点命名表：**

| 构造操作 | 功能前缀 | 命名示例 | 含义 |
|:---|:---|:---|:---|
| 中点 | `M` | `M_AB` | 线段 AB 的中点 |
| 交点 | `X` 或 `I` | `X_l_m`, `I_AB_CD` | 线 l 与 m 的交点；线段 AB 与 CD 的交点 |
| 垂足 | `F` | `F_A_l` | 点 A 到线 l 的垂足 |
| 对称点 | `R` | `R_A_l` | 点 A 关于线 l 的对称点 |
| 重心 | `G` | `G_ABC` | 三角形 ABC 的重心 |
| 垂心 | `H` | `H_ABC` | 三角形 ABC 的垂心 |
| 外心 | `O` | `O_ABC` | 三角形 ABC 的外心 |
| 内心 | `I` | `I_ABC` | 三角形 ABC 的内心 |
| 等分点 | `D` | `D_AB_3` | 将线段 AB 三等分的点（第 3 个） |
| 旋转点 | `R` | `R_A_O_60` | 点 A 绕 O 旋转 60 度后的点 |

**示例：**

```
// 三角形 ABC 的中点三角形
M_AB    — AB 的中点
M_BC    — BC 的中点
M_CA    — CA 的中点

// 进一步构造：中点连线的交点
X_M_AB_M_BC_M_CA_M_AB    — 不推荐：嵌套过深，可读性差
// 推荐：给中间结果命名
P = M_AB
Q = M_BC
X_P_Q_...                  — 简洁可读
```

**规则细节：**

- 参数顺序与构造操作的参数顺序一致。
- 嵌套深度不超过 2 层。若需更深，应引入中间命名。
- 若构造操作产生多个点（如两圆相交产生两个交点），使用数字后缀区分：`X_c1_c2_1`, `X_c1_c2_2`。

### 2.3 特殊点（约定俗成命名）

某些点因历史惯例拥有固定命名，Lv-00 予以保留。

| 命名 | 语义 | 上下文 |
|:---|:---|:---|
| `O` | 圆心 / 原点 | 有且仅有一个圆时，`O` 默认为其圆心；坐标系中 `O` 为原点 |
| `I` | 内心 / 单位点 | 三角形中默认为内心；直线上默认为单位点 |
| `H` | 垂心 | 三角形的垂心 |
| `G` | 重心 | 三角形的重心 |
| `A, B, C` | 三角形顶点 | 默认三角形的顶点，按逆时针排列 |

**注意：** 当 `O` 已作为自由点使用时，圆的圆心必须显式命名（如 `O_c1`）。引擎在解析时按"最近上下文"原则消歧。

---

## 3. 线与线段的命名

### 3.1 直线

直线有两种等价的命名方式，用户可任选其一，引擎内部统一处理。

```
方式一：小写拉丁字母
语法：单个小写拉丁字母
示例：l, m, n, p, q

方式二：两点记号
语法：AB（两个大写字母并置，无分隔符）
示例：AB, BC, PQ

两种方式等价：line l 与 line AB 在语义上无区别
```

**示例：**

```
// 方式一：字母命名
l = line_through(A, B)       — 过 A、B 的直线，命名为 l
m = parallel(l, C)           — 过 C 且平行于 l 的直线，命名为 m

// 方式二：两点命名（隐式）
AB                           — 由点 A、B 确定的直线，命名为 AB
// 若要显式声明：
line AB                      — 明确声明 AB 为直线（而非线段）
```

**上下文消歧：** 当 `AB` 既可能指直线又可能指线段时，上下文决定：
- 在约束 `C lies on AB` 中，`AB` 为直线。
- 在约束 `M is midpoint of AB` 中，`AB` 为线段。
- 显式消歧：`line AB` vs `segment AB`。

### 3.2 线段

线段使用端点记号，并可通过关键字 `segment` 显式声明。

```
语法：segment AB 或 AB（在需要消歧的上下文中使用关键字）
```

**示例：**

```
segment AB                   — 以 A、B 为端点的线段
segment AB ≅ segment CD      — 线段 AB 与线段 CD 全等
midpoint of segment AB       — 线段 AB 的中点
```

**规则细节：**

- 线段自带两个端点端口：`AB.start` = A，`AB.end` = B。
- 线段长度通过属性访问：`|AB|` 或 `length(AB)`。
- 有向线段用箭头记号：展示层渲染为 `AB⃗`，代码层写作 `vec AB`。

### 3.3 射线

射线通过原点和方向点命名。

```
语法：ray AB
含义：以 A 为原点，经过 B 的射线
```

**示例：**

```
ray AB                       — 以 A 为起点、方向指向 B 的射线
ray BA                       — 以 B 为起点、方向指向 A 的射线（不同于 ray AB）
angle_bisector(ray AB, ray AC)  — 角 BAC 的平分线
```

---

## 4. 区域的命名

### 4.1 三角形

三角形使用三个顶点命名，以 `△` 为前缀（展示层）或 `tri` 为前缀（代码层）。

```
展示层：△ABC
代码层：tri ABC 或 triangle ABC
```

**示例：**

```
△ABC                         — 顶点为 A、B、C 的三角形
area(△ABC)                   — 三角形 ABC 的面积
△ABC ≅ △DEF                  — 三角形 ABC 与三角形 DEF 全等
△ABC ~ △PQR                  — 三角形 ABC 与三角形 PQR 相似

// 特殊三角形
right_triangle ABC at A      — 以 A 为直角顶点的直角三角形
isosceles ABC with AB = AC   — AB = AC 的等腰三角形
equilateral ABC              — 等边三角形 ABC
```

**规则细节：**

- 顶点顺序默认为逆时针，但这仅为约定，不强制。
- 三角形的边自动命名：`△ABC` 的三边分别为 `BC`、`CA`、`AB`。
- 三角形的角自动命名：`∠BAC`（顶点 A 处的角）、`∠CBA`（顶点 B 处的角）、`∠ACB`（顶点 C 处的角）。

### 4.2 多边形

多边形使用顶点序列命名，以 `□` 为前缀（展示层）或 `poly` 为前缀（代码层）。

```
展示层：□ABCD
代码层：poly ABCD 或 polygon ABCD
```

**示例：**

```
□ABCD                        — 顶点为 A、B、C、D 的四边形
□ABCDE                       — 五边形
regular_polygon ABCDEF       — 正六边形

// 顶点按顺序连接：A→B→C→D→(闭合回A)
```

**约定：**

- 顶点按连接顺序列出。
- 首尾自动闭合（多边形至少需要 3 个顶点）。
- 对于 `n > 4` 的一般多边形，推荐使用 `polygon(A, B, C, D, E)` 的函数块形式。

### 4.3 圆

圆使用圆心和半径点命名。

```
展示层：⊙(O, A)
代码层：circle(O, A)
含义：以 O 为圆心、以 |OA| 为半径的圆
```

**示例：**

```
⊙(O, A)                      — 以 O 为圆心，过点 A 的圆
circle(P, Q)                 — 以 P 为圆心，以 |PQ| 为半径的圆
⊙(O, r)                      — 以 O 为圆心、半径为 r 的圆（r 为已命名的线段长度）

// 圆心和圆上点的访问
⊙(O, A).center               — → O
⊙(O, A).radius_point         — → A
⊙(O, A).radius               — → |OA|（数值属性）
```

**规则细节：**

- 圆的第二个参数是"半径点"（在圆周上），而非半径值。这延续了几何构造中"两点定圆"的传统。
- 若需以数值半径构造圆，使用 `circle_by_radius(O, r)` 函数块。

### 4.4 其他区域

| 区域类型 | 展示层 | 代码层 | 示例 |
|:---|:---|:---|:---|
| 角区域 | `∠ABC` | `angle(A, B, C)` | 顶点为 B 的角 |
| 扇形 | `⌔(O, A, B)` | `sector(O, A, B)` | 圆心 O、弧 AB 的扇形 |
| 弓形 | `⌓(O, A, B)` | `segment_of_circle(O, A, B)` | 圆心 O、弦 AB 的弓形 |
| 半平面 | `H(l, P)` | `half_plane(l, P)` | 直线 l 包含点 P 一侧的半平面 |

---

## 5. 约束的命名

约束描述几何对象之间的关系。Lv-00 的约束命名遵循"可读自然语言 + 精确符号"的双轨制。

### 5.1 关联约束（Incidence）

表示点与线（或点与圆）的"在上"关系。

```
语法形式：
  "点 lies on 线"          — 完整自然语言
  "线 passes through 点"   — 反向表述
  "点 ∈ 线"                — 符号形式（展示层）

代码示例：
  A lies on l               — 点 A 在直线 l 上
  l passes through B        — 直线 l 经过点 B
  P lies on ⊙(O, A)         — 点 P 在圆上
```

**命名惯例：** 以 "主语 + lies on / passes through + 宾语" 的核心结构表达。

### 5.2 介于约束（Betweenness）

表示共线三点的顺序关系。

```
语法形式：
  "B is between A and C"   — 标准形式

代码示例：
  B is between A and C      — B 在 A 和 C 之间（B 在线段 AC 上）
```

**注意：** 介于约束要求三点共线。若三点未确认共线，引擎应发出警告。

### 5.3 垂直约束（Perpendicular）

```
展示层：
  l ⊥ m                     — 直线 l 垂直于直线 m
  AB ⊥ CD                   — 线段 AB 垂直于线段 CD

代码层：
  l ⟂ m                      — ASCII 表示（Unicode ⟂ U+27C2）
  perp(l, m)                 — 函数块表示
```

### 5.4 平行约束（Parallel）

```
展示层：
  l ∥ m                     — 直线 l 平行于直线 m
  AB ∥ CD                   — 线段 AB 平行于线段 CD

代码层：
  l // m                     — ASCII 表示（双斜线）
  parallel(l, m)             — 函数块表示
```

### 5.5 全等约束（Congruence）

```
展示层：
  AB ≅ CD                   — 线段 AB 与线段 CD 全等
  ∠ABC ≅ ∠DEF               — 角 ABC 与角 DEF 全等
  △ABC ≅ △DEF               — 三角形 ABC 与三角形 DEF 全等

代码层：
  AB ≅ CD
  cong(AB, CD)               — 函数块表示
```

**注意：** 全等符号 `≅` 为 Unicode U+2245。代码层接受 ASCII 替代写法 `~=`。

### 5.5 约束命名总表

| 约束类型 | 自然语言 | 展示层符号 | 代码层符号 | 函数块 |
|:---|:---|:---|:---|:---|
| 关联 | `A lies on l` | `A ∈ l` | `A on l` | `incident(A, l)` |
| 介于 | `B is between A and C` | `A-B-C` | `between(A, B, C)` | `between(A, B, C)` |
| 垂直 | `l perpendicular to m` | `l ⊥ m` | `l ⟂ m` | `perp(l, m)` |
| 平行 | `l parallel to m` | `l ∥ m` | `l ∥ m` | `parallel(l, m)` |
| 全等 | `AB congruent to CD` | `AB ≅ CD` | `AB ≅ CD` | `cong(AB, CD)` |
| 共线 | `A, B, C are collinear` | `coll(A, B, C)` | `collinear(A, B, C)` | `collinear(A, B, C)` |
| 共圆 | `A, B, C, D are concyclic` | `concyc(A,B,C,D)` | `concyclic(A,B,C,D)` | `concyclic(A, B, C, D)` |

---

## 6. 函数块的命名

函数块（Function Block）是 Lv-00 中的构造过程抽象，相当于 GeoGebra 中的"工具"或编程语言中的"函数"。命名规范直接影响代码可读性和 API 一致性。

### 6.1 命名风格：PascalCase

函数块名称使用 PascalCase（大驼峰），每个单词首字母大写。

```
语法：PascalCase 单词序列
示例：Midpoint, AngleBisector, PerpendicularBisector
```

**规则：**

- 不使用下划线分隔单词（`Mid_point` 违反规范）。
- 不使用全小写（`midpoint` 违反规范）。
- 不使用 `get_` 或 `create_` 前缀（函数块本身就是构造操作）。

### 6.2 参数顺序约定

```
[输入对象列表], [构造参数]
```

- **输入对象在前**：被构造操作所依赖的几何对象首先列出。
- **构造参数在后**：非几何参数（如数值、选项）放在最后。

**示例：**

```
Midpoint(A, B)                    — A, B 为输入点
AngleBisector(ray1, ray2)         — 两条射线为输入
PerpendicularBisector(A, B)       — 点 A、B 确定线段
CircleByCenterAndPoint(O, A)      — O 为圆心，A 为半径点
CircleByCenterAndRadius(O, r)     — r 为数值参数（放在最后）
RotatePoint(P, O, 60)             — P 为输入点，O 为旋转中心，60 为角度参数
ScalePoint(P, O, 2)               — P 为输入点，O 为缩放中心，2 为比例因子
```

### 6.3 常用函数块命名表

| 类别 | 函数块名称 | 参数 | 返回值 |
|:---|:---|:---|:---|
| **基本构造** | | | |
| 中点 | `Midpoint` | `(A, B)` | `M_AB` |
| 交点 | `Intersection` | `(l1, l2)` | `X_l1_l2` |
| 垂线 | `PerpendicularLine` | `(l, P)` | 过 P 垂直于 l 的线 |
| 平行线 | `ParallelLine` | `(l, P)` | 过 P 平行于 l 的线 |
| 垂足 | `PerpendicularFoot` | `(P, l)` | `F_P_l` |
| **角的构造** | | | |
| 角平分线 | `AngleBisector` | `(ray1, ray2)` | 角平分线 |
| 角平分线（三点） | `AngleBisector3` | `(A, B, C)` | ∠ABC 的平分线 |
| **线段操作** | | | |
| 中垂线 | `PerpendicularBisector` | `(A, B)` | 线段 AB 的中垂线 |
| 等分点 | `DivideSegment` | `(A, B, n)` | n 个等分点 |
| **圆** | | | |
| 两点定圆 | `CircleByCenterAndPoint` | `(O, A)` | 以 O 为心过 A 的圆 |
| 半径定圆 | `CircleByCenterAndRadius` | `(O, r)` | 以 O 为心半径 r 的圆 |
| 三点定圆 | `CircleByThreePoints` | `(A, B, C)` | 过三点的圆 |
| **变换** | | | |
| 旋转 | `RotatePoint` | `(P, O, angle)` | 旋转后的点 |
| 缩放 | `ScalePoint` | `(P, O, factor)` | 缩放后的点 |
| 平移 | `TranslatePoint` | `(P, vector)` | 平移后的点 |
| 反射 | `ReflectPoint` | `(P, line)` | 反射后的点 |
| **三角形特殊点** | | | |
| 重心 | `Centroid` | `(A, B, C)` | `G_ABC` |
| 垂心 | `Orthocenter` | `(A, B, C)` | `H_ABC` |
| 外心 | `Circumcenter` | `(A, B, C)` | `O_ABC` |
| 内心 | `Incenter` | `(A, B, C)` | `I_ABC` |

---

## 7. 端口的命名

函数块通过端口（Port）与外界交互。端口命名需清晰表达数据的含义和方向。

### 7.1 输入端口

输入端口使用 `snake_case`，以 `类型_描述` 的格式命名。

```
语法：[类型]_[描述]
类型：point, segment, line, ray, circle, angle 等
描述：参数的语义角色
```

**示例：**

```
point_A              — 类型为点的参数，语义角色为"A点"
point_B              — 类型为点的参数，语义角色为"B点"
segment_base         — 类型为线段的参数，语义角色为"基准线段"
line_ref             — 类型为直线的参数，语义角色为"参考线"
circle_source        — 类型为圆的参数，语义角色为"源圆"
angle_input          — 类型为角的参数，语义角色为"输入角"
```

**规则细节：**

- 当参数数量少且角色明确时，可采用简短形式：`A`, `B`, `l`。
- 当参数数量多或容易混淆时，必须使用描述性名称：`segment_base` 而非 `s`。
- 同一函数块内，端口名应保持一致的命名风格。

### 7.2 输出端口

输出端口命名描述"生成了什么"，使用 `snake_case`。

```
语法：[结果描述]
结果描述：中点 → midpoint，垂足 → foot，平分线 → bisector
```

**示例：**

```
midpoint             — 输出为中点
bisector_line        — 输出为角平分线（直线类型）
perp_line            — 输出为垂线
intersection_point   — 输出为交点
center_point         — 输出为圆心
circumcircle         — 输出为外接圆
```

**规则细节：**

- 当输出类型可从名称推断时，可省略类型后缀（如 `midpoint` 隐含返回点类型）。
- 当同一函数块有多个输出时，使用数字后缀区分：`intersection_1`, `intersection_2`。
- 输出端口的类型在函数块定义时声明，引擎在连接时做类型检查。

### 7.3 端口命名示例

以 `PerpendicularBisector` 函数块为例：

```
FunctionBlock: PerpendicularBisector

Input Ports:
  point_A          (类型: Point)      — 线段端点 A
  point_B          (类型: Point)      — 线段端点 B

Output Ports:
  bisector_line    (类型: Line)       — AB 的中垂线
  midpoint         (类型: Point)      — AB 的中点（辅助输出）
```

以 `AngleBisector` 函数块为例：

```
FunctionBlock: AngleBisector3

Input Ports:
  point_vertex     (类型: Point)      — 角的顶点
  point_arm1       (类型: Point)      — 角的第一条边上的点
  point_arm2       (类型: Point)      — 角的第二条边上的点

Output Ports:
  bisector_ray     (类型: Ray)        — 角的平分线（射线形式）
```

---

## 8. 公理的命名

公理命名受 LeanGeo 的分层命名启发，采用 `namespace.layer.axiom_number` 的三段式结构，确保公理的可定位性和可替换性。

### 8.1 命名格式

```
格式：namespace.layer.axiom_number

namespace   — 几何体系（euclidean, hyperbolic, elliptic 等）
layer       — 公理层次（incidence, order, congruence, continuity, parallel）
axiom_number — 该层内的公理编号（I1, I2, I3; B1, B2; C1, C2, ...）
```

### 8.2 欧氏几何公理命名

**关联公理（Incidence）：**

| 公理标识 | 内容 |
|:---|:---|
| `euclidean.incidence.I1` | 过两点有且仅有一条直线 |
| `euclidean.incidence.I2` | 每条直线上至少有两个点 |
| `euclidean.incidence.I3` | 存在不共线的三点 |

**顺序公理（Order / Betweenness）：**

| 公理标识 | 内容 |
|:---|:---|
| `euclidean.order.B1` | 若 B 在 A 和 C 之间，则 A、B、C 共线且 B 在 C 和 A 之间 |
| `euclidean.order.B2` | 对任意两点 A、C，存在 B 使得 B 在 A 和 C 之间 |
| `euclidean.order.B3` | 共线三点中至多有一点在另外两点之间 |
| `euclidean.order.B4` | Pasch 公理 |

**全等公理（Congruence）：**

| 公理标识 | 内容 |
|:---|:---|
| `euclidean.congruence.C1` | 线段迁移公理 |
| `euclidean.congruence.C2` | 线段全等的传递性 |
| `euclidean.congruence.C3` | 线段加减公理 |

**连续公理（Continuity）：**

| 公理标识 | 内容 |
|:---|:---|
| `euclidean.continuity.Dedekind` | Dedekind 连续公理（或 Archimedes 公理） |

**平行公理（Parallel）：**

| 公理标识 | 内容 |
|:---|:---|
| `euclidean.parallel.P` | 过直线外一点有且仅有一条平行线（Playfair 公理） |

### 8.3 可替换性设计

公理命名的分层结构天然支持公理替换。以平行公理为例：

```
// 欧氏几何：加载 Playfair 平行公理
load euclidean.parallel.P

// 双曲几何：卸载 P，加载双曲平行公理
unload euclidean.parallel.P
load hyperbolic.parallel.H

// 椭圆几何：无平行公理
unload euclidean.parallel.P
// 不加载任何替代 → 实现椭圆几何
```

这种设计使得 Lv-00 的"公理中立"原则得以在命名层面贯穿。

### 8.4 其他体系公理命名（示例）

| 公理标识 | 体系 | 层次 | 内容 |
|:---|:---|:---|:---|
| `group_theory.groups.G1` | 群论 | 群 | 结合律 |
| `group_theory.groups.G2` | 群论 | 群 | 单位元存在性 |
| `group_theory.groups.G3` | 群论 | 群 | 逆元存在性 |
| `zfc.set_theory.extensionality` | ZFC | 集合论 | 外延公理 |
| `zfc.set_theory.pairing` | ZFC | 集合论 | 配对公理 |
| `zfc.set_theory.union` | ZFC | 集合论 | 并集公理 |
| `projective.incidence.PI1` | 射影几何 | 关联 | 两点定线 |
| `projective.incidence.PI2` | 射影几何 | 关联 | 两线定点 |

---

## 9. 证明步骤的命名

Lv-00 中，证明过程由一系列步骤组成。步骤的命名需支持自动生成和用户自定义两种模式。

### 9.1 自动生成命名

当用户未显式命名证明步骤时，引擎自动分配序号。

```
格式：Step_[序号]
序号：从 1 开始递增的整数
```

**示例（自动生成）：**

```
Step_1: Given △ABC and point D lies on BC
Step_2: By euclidean.incidence.I1, line AD exists
Step_3: By euclidean.order.B2, there exists E between A and D
Step_4: Construct F = Midpoint(B, C)
Step_5: By euclidean.congruence.C1, ...
```

**规则细节：**

- 序号在单个证明上下文中唯一。
- 步骤重排时序号自动更新。
- 删除某步骤后，后续步骤序号自动递减（保持连续）。

### 9.2 用户自定义标签

用户可为关键步骤指定描述性标签，便于引用和复查。

```
语法：[英文标签]（snake_case 或 PascalCase，推荐 1-3 个单词）
```

**示例（用户标记）：**

```
Step_3: construct_midpoint  |  F = Midpoint(B, C)
Step_4: apply_parallel_axiom  |  By euclidean.parallel.P, line l' exists through F parallel to AB
Step_5: prove_congruence  |  △BFE ≅ △FCD by SAS
Step_7: conclusion  |  Therefore, E is the midpoint of AD
```

**规则细节：**

- 标签在单个证明上下文中唯一。
- 标签用于跨步骤引用：`from Step_3: construct_midpoint, we have F is midpoint of BC`。
- 标签可包含下划线但不能包含空格或特殊符号。

### 9.3 混合模式

自动序号与用户标签可以共存：

```
Step_1:                       |  Given △ABC
Step_2:                       |  D lies on BC
Step_3: define_aux_line       |  Construct line AD
Step_4:                       |  E = Midpoint(A, B)
Step_5: key_parallelism       |  By euclidean.parallel.P, ...
Step_6:                       |  ...
Step_7: final_deduction       |  Q.E.D.
```

---

## 10. 与竞品的对比

下表展示 Lv-00 命名体系与主要竞品（GeoGebra、LeanGeo）的对比，体现 Lv-00 的设计取舍。

### 10.1 总体对比

| 维度 | GeoGebra | LeanGeo | **Lv-00** |
|:---|:---|:---|:---|
| **设计哲学** | 教育导向，零门槛 | 形式化验证，严谨至上 | 人机共识，构造即证明 |
| **点命名** | 大写字母，自动递增 | 依赖类型推导，变量名 | 大写字母 + 构造前缀（`M_AB`） |
| **线命名** | 小写字母或两点 | 依赖类型变量 | 小写字母或两点，显式 `line`/`segment` |
| **函数命名** | 小写开头的驼峰（如 `segment`） | 遵循 Lean 命名惯例 | PascalCase（`Midpoint`） |
| **公理命名** | 不暴露公理层 | `namespace.axiom_name` | `namespace.layer.axiom_number` 三段式 |
| **端口命名** | 无显式端口概念 | 函数参数（类型驱动） | `snake_case` 描述性命名 |
| **证明步骤** | 无形式化证明 | 策略驱动的步骤链 | 自动序号 + 用户标签双轨 |
| **多体系** | 仅欧氏 + 少量非欧 | 聚焦欧氏 | 公理包体系，任意几何/代数 |

### 10.2 关键差异详析

**点命名的差异：**

```
GeoGebra:   A, B, C, D, ...  (自动递增，用户无法干预命名逻辑)
LeanGeo:    (a : Point), (b : Point)  (类型标注驱动)
Lv-00:      A, M_AB, X_l_m  (命名即文档，一眼看出点的来源)
```

**Lv-00 的优势：** 构造点（如 `M_AB`）从命名即可获知其构造方式，无需查看上下文或类型定义。这在人机交互场景中尤为重要——用户可以直接引用 `M_AB` 而不需要先找到 `Midpoint(A, B)` 的调用位置。

**公理命名的差异：**

```
GeoGebra:   (不暴露公理层给用户)
LeanGeo:    incidence_line, between_identity  (扁平命名)
Lv-00:      euclidean.incidence.I1  (三段式，自带体系 + 层次信息)
```

**Lv-00 的优势：** 三段式命名天然支持公理中立和公理替换。当用户切换几何体系时（如从欧氏切到双曲），命名体系能清晰反映哪些公理被替换、哪些被保留。

### 10.3 Lv-00 命名的设计取舍

| 取舍 | 选择 | 理由 |
|:---|:---|:---|
| 函数块用 PascalCase 而非 snake_case | PascalCase | 与主流编程语言（C、JavaScript、Python 类名）惯例一致；在几何 DSL 中视觉区分度高 |
| 端口用 snake_case 而非 camelCase | snake_case | 与 C 语言核心代码风格一致（`src/` 中所有符号均使用 snake_case） |
| 公理加 namespace 前缀 | 加前缀 | 支持多体系共存，为公理中立架构提供命名层面的保障 |
| 对象命名体现构造来源 | 体现 | 符合"构造即证明"理念，命名本身包含推导信息 |
| 允许省略类型关键字 | 允许 | 在上下文清晰时减少噪音（`AB` 而非 `segment AB`） |

---

## 附录 A：命名速查表

### 对象命名速查

| 类别 | 命名格式 | 示例 |
|:---|:---|:---|
| 自由点 | `[A-Z]` | `A`, `P`, `Q` |
| 构造点 | `[前缀]_[参数]` | `M_AB`, `X_l_m`, `F_A_l` |
| 直线 | `[a-z]` 或 `[两点]` | `l`, `AB` |
| 线段 | `segment [两点]` | `segment AB` |
| 射线 | `ray [两点]` | `ray AB` |
| 三角形 | `△[三点]` / `tri [三点]` | `△ABC` |
| 多边形 | `□[顶点序列]` / `poly [序列]` | `□ABCD` |
| 圆 | `⊙(O, A)` / `circle(O, A)` | `⊙(O, A)` |

### 约束符号速查

| 约束 | 展示层 | 代码层 |
|:---|:---|:---|
| 关联 | `∈` | `lies on` |
| 介于 | `A-B-C` | `between` |
| 垂直 | `⊥` | `⟂` 或 `perp` |
| 平行 | `∥` | `∥` 或 `parallel` |
| 全等 | `≅` | `≅` 或 `cong` |
| 相似 | `~` | `~` 或 `similar` |

### 函数块速查

| 操作 | 函数块 | 返回 |
|:---|:---|:---|
| 中点 | `Midpoint(A, B)` | `M_AB` |
| 交点 | `Intersection(l1, l2)` | `X_l1_l2` |
| 垂线 | `PerpendicularLine(l, P)` | 直线 |
| 平行线 | `ParallelLine(l, P)` | 直线 |
| 角平分线 | `AngleBisector(r1, r2)` | 射线 |
| 中垂线 | `PerpendicularBisector(A, B)` | 直线 |
| 外接圆 | `Circumcircle(A, B, C)` | 圆 |
| 重心 | `Centroid(A, B, C)` | `G_ABC` |

---

## 附录 B：修订历史

| 版本 | 日期 | 变更 |
|:---|:---|:---|
| v1.0 | 2026-05-23 | 初始版本。覆盖点、线、区域、约束、函数块、端口、公理、证明步骤的完整命名规范。 |

---

> **相关文档：**
> - `competitive_analysis.md` — P3 行动项：借鉴 GeoGebra 的对象命名体系
> - `design_v2.9.md` — Lv-00 整体架构设计
> - `08_type_system.md` — Lv-00 类型系统设计（与命名规范密切关联）
> - `09_proof.md` — 证明系统设计（证明步骤命名的具体实现）
