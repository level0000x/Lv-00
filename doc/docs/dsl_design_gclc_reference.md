# Lv-00 DSL 设计文档：GCLC 参考版

> **版本**: 1.0.0  
> **日期**: 2026-05-24  
> **参考**: GCLC (Geometry Constructions LaTeX Converter) 的 GC 语言语法  
> **目标**: 为 Lv-00 几何元语言设计一套统一几何构造、计算程序与一阶逻辑证明的 DSL

---

## 目录

1. [设计哲学](#1-设计哲学)
2. [DSL 语法总览](#2-dsl-语法总览)
3. [完整关键字表](#3-完整关键字表)
4. [类型系统映射](#4-类型系统映射)
5. [编译映射：DSL 语句 → 内部结构](#5-编译映射dsl-语句--内部结构)
6. [完整示例：三角形构造与中线交点证明](#6-完整示例三角形构造与中线交点证明)
7. [与 GCLC 的对应表](#7-与-gclc-的对应表)
8. [编译流水线](#8-编译流水线)
9. [与 Spell 编译器的关系](#9-与-spell-编译器的关系)

---

## 1. 设计哲学

### 1.1 三层统一

Lv-00 DSL 在设计上统一了三个通常分离的层面：

| 层面 | GCLC 角色 | Lv-00 扩展 |
|------|-----------|------------|
| **几何构造** | `point A 10 20` | 添加类型标注、函数块打包、递归构造 |
| **计算程序** | 无原生支持 | 公式模块（`formula_module.js`）、符号坐标求解器 |
| **一阶逻辑证明** | 无原生支持 | 证明导航器（`ProofNavigator`）、多策略引擎（`ProofMultiStrategy`） |

### 1.2 与 GCLC 的核心差异

| 特性 | GCLC (GC 语言) | Lv-00 DSL |
|------|---------------|-----------|
| 语法风格 | 无分号、空格分隔 | 分号结尾、类似 C 风格 |
| 类型标注 | 无（隐式） | 可选显式类型标注 `:Type` |
| 构造语义 | 过程式 | 声明式 + 过程式混合 |
| 编译目标 | LaTeX (TikZ/PSTricks) | FuncBlock + ConstraintGraph |
| 函数抽象 | 无 | 函数块打包/例化/组合子 |
| 证明支持 | 无 | 多策略证明引擎 |
| 坐标 | 数值坐标 | 符号坐标（`SymbolicCoord`） |
| 确定性 | 无概念 | 确定性状态机（`DeterminismState`） |

---

## 2. DSL 语法总览

### 2.1 基础语法规则

```
// 注释：双斜线
/* 块注释 */

<程序>      ::= <语句>*
<语句>      ::= <构造语句> | <类型声明> | <约束语句> | <证明语句> | <函数块语句>
            |   <计算语句> | <选择器语句> | <条件语句> | <导出语句>

<构造语句>  ::= <点构造> | <线构造> | <圆构造> | <交点构造> | <中点构造>
            |   <垂线构造> | <平行线构造> | <角平分线构造> | <区域构造>
            |   <反射构造> | <旋转构造> | <缩放构造> | <平移构造>
```

### 2.2 GCLC 风格语法（兼容模式）

```
// GCLC 兼容模式 —— 空格分隔，无分号
point A 10 20
point B 50 80
point C 70 30
line a A B
line b B C
circle k A B
intersection D a k
```

### 2.3 Lv-00 原生语法（增强模式）

```
// Lv-00 原生语法 —— 分号结尾，支持类型标注
point A(10, 20);
point B(50, 80);
point C(70, 30);
line a = segment(A, B);
line b = segment(B, C);
circle k = circle_center_radius(A, B);  // 圆心A，过点B
point D = intersection(a, k, selector=pos_root);
```

### 2.4 函数块语法（Lv-00 独有）

```
// 打包一个"已知两点作等边三角形"的构造为函数块
funcblock equilateral_triangle(p1 : Point, p2 : Point) -> Point {
    // 内部构造
    point _M = midpoint(p1, p2);
    line _perp = perpendicular(p1, p2, _M);
    number _h = sqrt(3) / 2 * distance(p1, p2);
    point p3 = point_on_line_at_distance(_perp, _M, _h, selector=pos_root);
    return p3;
}

// 例化函数块
point D = equilateral_triangle(A, B);
```

### 2.5 证明语法（Lv-00 独有）

```
// 声明命题
proposition median_concurrency {
    given: triangle(A, B, C);
    construct: {
        point M_AB = midpoint(A, B);
        point M_BC = midpoint(B, C);
        point M_CA = midpoint(C, A);
        line med_A = segment(A, M_BC);
        line med_B = segment(B, M_CA);
        line med_C = segment(C, M_AB);
    }
    prove: concurrent(med_A, med_B, med_C);
}

// 执行证明
prove median_concurrency using strategy=area_method;
```

---

## 3. 完整关键字表

### 3.1 基础几何类型关键字

| 关键字 | 含义 | GCLC 对应 | Lv-00 内部类型 |
|--------|------|-----------|---------------|
| `point` | 定义几何点 | `point` | `GEOM_POINT` (`GeomNode.type=0`) |
| `line` | 定义直线 | `line` | `GEOM_LINE_SEGMENT` (`GeomNode.type=1`) |
| `segment` | 定义线段 | `segment` (GCLC 扩展) | `GEOM_LINE_SEGMENT` + `INCIDENCE` 约束 |
| `circle` | 定义圆 | `circle` | `GEOM_POINT`(圆心) + 半径约束 |
| `region` | 定义区域 | 无 | `GEOM_REGION` (`GeomNode.type=2`) |
| `port` | 定义端口 | 无 | `GEOM_PORT` (`GeomNode.type=3`) |
| `funcblock` | 定义函数块 | 无 | `GEOM_FUNCTION_BLOCK` (`GeomNode.type=4`) |

### 3.2 构造关键字

| 关键字 | 含义 | 参数 | GCLC 对应 |
|--------|------|------|-----------|
| `midpoint` | 中点构造 | `point M = midpoint(A, B)` | `midpoint` |
| `intersection` | 交点构造 | `point P = intersection(obj1, obj2)` | `intersection` |
| `perpendicular` | 垂线构造 | `line l = perpendicular(P, l_ref)` | `perp` (GCLC 部分) |
| `parallel` | 平行线构造 | `line l = parallel(P, l_ref)` | 无原生支持 |
| `angle_bisector` | 角平分线 | `line l = angle_bisector(A, O, B)` | `bis` |
| `reflect` | 反射点 | `point P' = reflect(P, line)` | 无原生支持 |
| `rotate` | 旋转 | `point P' = rotate(P, center, angle)` | 无原生支持 |
| `scale` | 缩放 | `point P' = scale(P, center, factor)` | 无原生支持 |
| `translate` | 平移 | `point P' = translate(P, vec)` | 无原生支持 |
| `circle_center_radius` | 圆心+半径 | `circle c = circle_center_radius(O, P_on)` | `circle` |
| `circle_three_points` | 三点定圆 | `circle c = circle_three_points(A, B, C)` | 无原生支持 |
| `circumcircle` | 外接圆 | `circle c = circumcircle(A, B, C)` | 无原生支持 |
| `incircle` | 内切圆 | `circle c = incircle(A, B, C)` | 无原生支持 |
| `centroid` | 重心 | `point G = centroid(A, B, C)` | 无原生支持 |
| `orthocenter` | 垂心 | `point H = orthocenter(A, B, C)` | 无原生支持 |
| `circumcenter` | 外心 | `point O = circumcenter(A, B, C)` | 无原生支持 |
| `incenter` | 内心 | `point I = incenter(A, B, C)` | 无原生支持 |

### 3.3 约束关键字

| 关键字 | 含义 | Lv-00 内部 `ConstraintType` |
|--------|------|---------------------------|
| `incident` | 关联约束 | `INCIDENCE` |
| `between` | 之间约束 | `BETWEENNESS` |
| `collinear` | 共线约束（语法糖） | `INCIDENCE` × 2 |
| `concurrent` | 共点约束 | 复合 — 三个 `INCIDENCE` 指向同一点 |
| `intersect` | 相交约束 | `INTERSECTION` |
| `contain` | 包含约束 | `CONTAINMENT` |
| `connect` | 连接约束 | `CONNECTION` |

### 3.4 选择器关键字（解决多解问题，Lv-00 独有）

| 关键字 | 含义 | Lv-00 内部 `SelectorType` |
|--------|------|--------------------------|
| `pos_root` | 取正根 | `SELECTOR_POSITIVE_ROOT` |
| `neg_root` | 取负根 | `SELECTOR_NEGATIVE_ROOT` |
| `in_region` | 取区域内的解 | `SELECTOR_IN_REGION` |
| `nearest_to` | 取最近解 | `SELECTOR_NEAREST_TO_POINT` |
| `custom` | 自定义选择器 | `SELECTOR_CUSTOM` |

### 3.5 证明/命题关键字（Lv-00 独有）

| 关键字 | 含义 | Lv-00 内部结构 |
|--------|------|---------------|
| `proposition` | 声明命题 | `Proposition` |
| `given` | 命题给定条件 | `Proposition.precondition_region_ids` |
| `construct` | 辅助构造 | `ProofStep` (`PROOF_STEP_ADD_NODE`) |
| `prove` | 证明目标 | `Proposition.postcondition_constraint_ids` |
| `prove ... using` | 执行证明 | `ProofNavigator` + `ProofMultiStrategy` |
| `lemma` | 引理声明 | 折叠的 `ProofStep` 块 |
| `axiom` | 公理声明 | 公理包 (`axiom_pkg.h`) |
| `strategy` | 证明策略选择 | `ProofStrategyType` 枚举 |
| `assume` | 数值假设 | `ProofDependency.source=DEP_SOURCE_NUMERIC` |
| `oracle` | 外部求解器调用 | `ProofDependency.source=DEP_SOURCE_ORACLE` |

### 3.6 证明策略关键字

| 关键字 | 含义 | Lv-00 内部 `ProofStrategyType` |
|--------|------|-------------------------------|
| `direct_construction` | 直接构造法 | `PROOF_STRATEGY_DIRECT_CONSTRUCTION` |
| `area_method` | 面积法 | `PROOF_STRATEGY_AREA_METHOD` |
| `grobner_basis` | Groebner 基法 | `PROOF_STRATEGY_GROEBNER_BASIS` |
| `vector_method` | 向量法 | `PROOF_STRATEGY_VECTOR_METHOD` |
| `full_angle` | 全角法 | `PROOF_STRATEGY_FULL_ANGLE_METHOD` |
| `deductive_db` | 演绎数据库 | `PROOF_STRATEGY_DEDUCTIVE_DATABASE` |
| `coordinate` | 坐标法 | `PROOF_STRATEGY_COORDINATE` |
| `oracle` | 外部求解器 | `PROOF_STRATEGY_ORACLE` |

### 3.7 函数块关键字（Lv-00 独有）

| 关键字 | 含义 | Lv-00 内部 |
|--------|------|-----------|
| `funcblock` | 声明函数块 | `FuncBlock` |
| `returns` / `->` | 返回类型 | `FuncBlock.output_port_ids` |
| `pack` | 打包现有构造为函数块 | `func_block_pack()` |
| `instantiate` | 例化函数块 | `func_block_instantiate()` |
| `compose` | 函数组合 g o f | `func_block_compose()` |
| `product` | 函数乘积 f x g | `func_block_product()` |
| `curry` | 部分应用（柯里化） | `func_block_partial_apply()` |
| `verify` | 确定性验证 | `func_block_verify_determinism()` |
| `precondition` | 声明前置条件 | `FuncBlock.precondition_region_ids` |

### 3.8 控制与导出关键字

| 关键字 | 含义 | 说明 |
|--------|------|------|
| `if` / `else` | 条件分支 | 用于构造选择 |
| `for` | 循环构造 | 批量生成几何对象 |
| `export_latex` | 导出 LaTeX | 对应 `proof_export_latex()` |
| `export_html` | 导出 HTML | 对应 `proof_export_html()` |
| `export_coq` | 导出 Coq | 对应 `proof_export_coq()` |
| `export_nl` | 导出自然语言证明 | 对应 `proof_export_natural_language()` |

---

## 4. 类型系统映射

### 4.1 几何类型映射

| DSL 类型 | Lv-00 `GeomType` 枚举 | `GeomNode` 关键字段 |
|----------|----------------------|-------------------|
| `Point` | `GEOM_POINT` (= 0) | `symbolic_coords[0..1]` |
| `Line` / `Segment` | `GEOM_LINE_SEGMENT` (= 1) | `data` (无特殊联合体，端点由 `INCIDENCE` 约束表达) |
| `Region` | `GEOM_REGION` (= 2) | `data.region.boundary_segments[]` |
| `Port` | `GEOM_PORT` (= 3) | `data.port` (方向、命名空间深度、多态标记) |
| `FuncBlock` | `GEOM_FUNCTION_BLOCK` (= 4) | `data.func_block` (内部节点、端口ID、确定性状态) |

### 4.2 约束类型映射

| DSL 约束 | `ConstraintType` | 语义 |
|----------|-----------------|------|
| 点在线上 | `INCIDENCE` | `point P ∈ line l` |
| 有序三点共线 | `BETWEENNESS` | `A-B-C` 共线且 B 在 A 和 C 之间 |
| 两对象相交 | `INTERSECTION` | `obj1 ∩ obj2` |
| 一对象在另一对象内 | `CONTAINMENT` | `obj1 ⊂ obj2` |
| 端口连线 | `CONNECTION` | 函数块端口的数据流连接 |

### 4.3 证明颜色映射

| DSL 颜色标记 | `ProofColor` 枚举 | 含义 |
|-------------|------------------|------|
| `@green` | `PROOF_COLOR_GREEN` | 全构造，无非常规依赖 |
| `@green_verified` | `PROOF_COLOR_GREEN_VERIFIED` | 已证不可构造 |
| `@yellow` | `PROOF_COLOR_YELLOW` | 条件性不可构造 |
| `@orange_oracle` | `PROOF_COLOR_ORANGE_ORACLE` | 依赖非构造性 oracle |
| `@orange_ex_falso` | `PROOF_COLOR_ORANGE_EX_FALSO` | 爆炸原理步骤 |
| `@amber` | `PROOF_COLOR_AMBER` | 含数值假设 |
| `@dark_orange` | `PROOF_COLOR_DARK_ORANGE` | 非构造性依赖 + 数值假设 |

---

## 5. 编译映射：DSL 语句 → 内部结构

### 5.1 点构造

```
// DSL
point A(10, 20);
point B(x_val, y_val);  // 符号坐标
```

**编译为：**

```c
// 1. 创建 GeomNode
GeomNode *node = geom_node_create(GEOM_POINT);

// 2. 设置符号坐标
SymbolicCoord *sx = symbolic_coord_from_double(10.0);
SymbolicCoord *sy = symbolic_coord_from_double(20.0);
node->symbolic_coords[0] = sx;
node->symbolic_coords[1] = sy;

// 3. 添加到约束图
constraint_graph_add_node(graph, node);
```

### 5.2 线段构造

```
// DSL
segment AB(A, B);
line l = segment(A, B);
```

**编译为：**

```c
// 1. 创建线段节点
GeomNode *seg = geom_node_create(GEOM_LINE_SEGMENT);

// 2. 创建两个 INCIDENCE 约束
Constraint *c1 = constraint_create(INCIDENCE);
c1->participants = {A_id, seg_id};
Constraint *c2 = constraint_create(INCIDENCE);
c2->participants = {B_id, seg_id};

// 3. 添加到约束图
constraint_graph_add_constraint(graph, c1);
constraint_graph_add_constraint(graph, c2);
```

### 5.3 交点构造（含选择器）

```
// DSL
point D = intersection(a, k, selector=pos_root);
```

**编译为：**

```c
// 1. 创建交点节点
GeomNode *isect = geom_node_create(GEOM_POINT);

// 2. 创建 INTERSECTION 约束
Constraint *c = constraint_create(INTERSECTION);
c->participants = {isect_id, line_a_id, circle_k_id};

// 3. 创建 SolutionSelector（多解选择器）
SolutionSelector *sel = selector_create(SELECTOR_POSITIVE_ROOT);
func_block_set_selector(func_block, sel);

// 4. 自动打包为 FuncBlock（内部打包）
PackConfig config = {
    .internal_node_ids = {isect_id},
    .internal_count = 1,
    .input_port_ids = {line_a_id, circle_k_id},
    .input_count = 2,
    .output_port_ids = {isect_id},
    .output_count = 1
};
func_block_pack_ex(graph, &config, &out_fb);
```

### 5.4 函数块打包

```
// DSL
funcblock equilateral(p1 : Point, p2 : Point) -> Point {
    point _M = midpoint(p1, p2);
    line _perp = perpendicular(p1, p2, _M);
    number _h = sqrt(3) / 2 * distance(p1, p2);
    point result = point_on_line_at_distance(_perp, _M, _h, selector=pos_root);
    return result;
}
```

**编译为：**

```c
FuncBlock *fb = func_block_create(next_id);

// 内部节点
int internals[] = {_M_id, _perp_id, result_id};
func_block_set_internal_nodes(fb, internals, 3);

// 输入端口（p1, p2）
int inputs[] = {p1_id, p2_id};
func_block_set_input_ports(fb, inputs, 2);

// 输出端口
int outputs[] = {result_id};
func_block_set_output_ports(fb, outputs, 1);

// 设置名称
func_block_set_name(fb, "equilateral");
func_block_set_description(fb, "已知两点作等边三角形");

// 确定性验证
func_block_verify_determinism(fb, graph, /*step_limit=*/1000);
```

### 5.5 命题声明

```
// DSL
proposition median_concurrency {
    given: triangle(A, B, C);
    prove: concurrent(med_A, med_B, med_C);
}
```

**编译为：**

```c
Proposition *prop = proposition_create(prop_id, PROPOSITION_ATOMIC);

// 设置前置条件
int precond_regions[] = {triangle_region_id};
proposition_set_preconditions(prop, precond_regions, 1);

// 设置模式图（pattern）
ConstraintGraph *pattern = build_triangle_pattern(graph, A_id, B_id, C_id);
proposition_set_pattern(prop, pattern);

// 设置后置条件（待证明的约束）
int postcond_constraints[] = {concurrency_constraint_id};
proposition_set_postconditions(prop, postcond_constraints, 1);
```

### 5.6 证明执行

```
// DSL
prove median_concurrency using strategy=area_method;
```

**编译为：**

```c
// 1. 创建证明导航器
ProofNavigator *nav = proof_navigator_create(target_prop, engine);

// 2. 创建多策略引擎
ProofMultiStrategy *mse = proof_multi_strategy_create(nav);

// 3. 激活指定策略
proof_multi_strategy_activate(mse, PROOF_STRATEGY_AREA_METHOD);

// 4. 执行证明
bool success = proof_multi_strategy_execute(mse);

// 5. 计算最终信任颜色
ProofColor final_color = proof_navigator_compute_final_color(nav);

// 6. 导出结果（如需要）
if (export_requested) {
    proof_export_natural_language(nav, "output.txt", PROOF_NL_LANG_ZH_CN);
    proof_export_html(nav, "output.html");
}
```

---

## 6. 完整示例：三角形构造与中线交点证明

### 6.1 问题描述

构造三角形 ABC，作三条中线，证明三条中线交于一点（重心）。

### 6.2 DSL 源代码

```
// ============================================================
// Lv-00 DSL: 三角形构造 + 中线交点证明
// 参考 GCLC 语法，融合 Lv-00 证明能力
// ============================================================

// --- 1. 基础构造 ---

// GCLC 风格：点构造
point A 0 0;
point B 600 0;
point C 300 400;

// Lv-00 风格：线段构造
segment AB = segment(A, B);
segment BC = segment(B, C);
segment CA = segment(C, A);

// --- 2. 中线构造 ---

// 三边中点
point M_AB = midpoint(A, B);   // AB 中点
point M_BC = midpoint(B, C);   // BC 中点
point M_CA = midpoint(C, A);   // CA 中点

// 三条中线
line med_A = segment(A, M_BC);
line med_B = segment(B, M_CA);
line med_C = segment(C, M_AB);

// --- 3. 证明命题：三条中线共点 ---

proposition median_concurrency {
    // 给定前提
    given: triangle(A, B, C);

    // 辅助构造
    construct: {
        point M_AB = midpoint(A, B);
        point M_BC = midpoint(B, C);
        point M_CA = midpoint(C, A);
        line med_A = segment(A, M_BC);
        line med_B = segment(B, M_CA);
        line med_C = segment(C, M_AB);
    }

    // 证明目标
    prove: concurrent(med_A, med_B, med_C);
}

// 使用面积法证明
prove median_concurrency using strategy=area_method;

// 如果面积法失败，回退到直接构造法
// prove median_concurrency using strategy=direct_construction;

// --- 4. 计算重心坐标 ---

number G_x = (A.x + B.x + C.x) / 3;
number G_y = (A.y + B.y + C.y) / 3;
point G = point(G_x, G_y);

// 验证 G 在每条中线上
assert incident(G, med_A);
assert incident(G, med_B);
assert incident(G, med_C);

// --- 5. 导出 ---

export_latex "triangle_medians.tex";
export_html  "triangle_medians.html";
export_nl    "triangle_medians_proof.txt" lang=zh_cn;
```

### 6.3 编译流程图

```
DSL 源代码
    │
    ▼
┌─────────────────┐
│  词法分析 (Lexer) │  ← lexer_shared.h
└────────┬────────┘
         │ token 流
         ▼
┌─────────────────┐
│  语法分析 (Parser)│  ← FormulaParser (formula_module.js)
└────────┬────────┘
         │ AST
         ▼
┌──────────────────────────────┐
│  第一遍：几何构造展开         │
│  · point/line/circle → GeomNode│
│  · segment/intersection →     │
│    FuncBlock 内部打包          │
│  · 建立 ConstraintGraph       │
└────────┬─────────────────────┘
         │ ConstraintGraph
         ▼
┌──────────────────────────────┐
│  第二遍：约束解析与归一化     │
│  · 统一 INCIDENCE/BETWEENNESS│
│  · 图规范化遍历               │
│  · 冗余检测                   │
└────────┬─────────────────────┘
         │ 规范化图
         ▼
┌──────────────────────────────┐
│  第三遍：证明引擎             │
│  · 命题 → Proposition        │
│  · 策略选择 → ProofMultiStrat.│
│  · 执行 → ProofNavigator     │
│  · 合一检查 → proof_unify()  │
└────────┬─────────────────────┘
         │ 证明结果
         ▼
┌──────────────────────────────┐
│  第四遍：导出                 │
│  · LaTeX / HTML / Coq / NL   │
│  · 搜索树 JSON/DOT (Newclid)  │
└──────────────────────────────┘
```

### 6.4 编译后对应的内部结构概览

```
ConstraintGraph:
  Nodes (GeomNode):
    id=1: GEOM_POINT    "A"    (0, 0)
    id=2: GEOM_POINT    "B"    (600, 0)
    id=3: GEOM_POINT    "C"    (300, 400)
    id=4: GEOM_LINE_SEGMENT "AB" (A,B)
    id=5: GEOM_LINE_SEGMENT "BC" (B,C)
    id=6: GEOM_LINE_SEGMENT "CA" (C,A)
    id=7: GEOM_POINT    "M_AB"  (midpoint of A,B)
    id=8: GEOM_POINT    "M_BC"  (midpoint of B,C)
    id=9: GEOM_POINT    "M_CA"  (midpoint of C,A)
    id=10: GEOM_LINE_SEGMENT "med_A" (A,M_BC)
    id=11: GEOM_LINE_SEGMENT "med_B" (B,M_CA)
    id=12: GEOM_LINE_SEGMENT "med_C" (C,M_AB)
    id=13: GEOM_POINT    "G"    (centroid)

  Constraints:
    c1: INCIDENCE(A, AB), INCIDENCE(B, AB)
    c2: INCIDENCE(B, BC), INCIDENCE(C, BC)
    c3: INCIDENCE(C, CA), INCIDENCE(A, CA)
    c4: INTERSECTION(M_AB, AB, perp_bisector_of_AB)
    c5-c7: INCIDENCE(G, med_A/B/C)

  FuncBlocks:
    fb_midpoint:  输入(Point, Point) → 输出(Point)
    fb_centroid:  输入(Point, Point, Point) → 输出(Point)

ProofNavigator:
  current_step: 5
  steps: [ADD_NODE(A), ADD_NODE(B), ADD_NODE(C), ...]
  final_color: PROOF_COLOR_GREEN
  strategy_note: "通过面积法证明三条中线共点于重心"
```

---

## 7. 与 GCLC 的对应表

### 7.1 语法对应

| GCLC (GC 语言) | Lv-00 DSL | 说明 |
|----------------|-----------|------|
| `point A 10 20` | `point A(10, 20);` | Lv-00 使用括号和分号 |
| `point B 50 80` | `point B(50, 80);` | 同上 |
| `line a A B` | `line a = segment(A, B);` | Lv-00 显式标明 segment |
| `line b B C` | `line b = segment(B, C);` | 同上 |
| `circle k A B` | `circle k = circle_center_radius(A, B);` | Lv-00 语义更明确 |
| `intersection D a k` | `point D = intersection(a, k);` | Lv-00 保留选择器选项 |
| `midpoint M A B` | `point M = midpoint(A, B);` | 语义一致 |
| `bis l A O B` | `line l = angle_bisector(A, O, B);` | 角平分线 |
| `perp l P base` | `line l = perpendicular(P, base);` | 垂线 |
| `onsegment P A B` | `assert between(A, P, B);` | 点在线上 |
| `drawsegment A B` | `segment AB = segment(A, B);` | 绘制线段 |
| `drawcircle k center radius` | `circle k = circle_center_radius(O, P_on_circle);` | 绘制圆 |
| `ang_plot a O P` | `number a = angle(O, P);` | 角度标注 |
| `print` (LaTeX 宏) | `export_latex "file.tex";` | 导出 |

### 7.2 能力对应

| 能力 | GCLC | Lv-00 DSL |
|------|------|-----------|
| 几何点构造 | 有 | 有 |
| 线段/直线构造 | 有 | 有 |
| 圆构造 | 有 | 有 |
| 交点构造 | 有（仅线段/圆） | 有（任意几何对象） |
| 中点构造 | 有 | 有 |
| 垂线构造 | 有 | 有 |
| 平行线构造 | 无 | 有 |
| 角平分线构造 | 有 | 有 |
| 反射/旋转/缩放 | 无 | 有 |
| 三点定圆 | 无 | 有 |
| 外心/内心/垂心/重心 | 无 | 有 |
| 类型标注 | 无 | 有 (`:Point`, `:Line`, 等) |
| 函数块抽象 | 无 | 有 (`funcblock`) |
| 函数组合/乘积 | 无 | 有 (`compose`, `product`) |
| 多解选择器 | 无 | 有 (`selector=`) |
| 确定性验证 | 无 | 有 (`verify`) |
| 命题声明 | 无 | 有 (`proposition`) |
| 多策略证明 | 无 | 有 (8 种策略) |
| 证明回溯树 | 无 | 有 (`ProofSearchTree`) |
| 引理折叠 | 无 | 有 (`LEMMA_VIEW_COLLAPSED`) |
| 自然语言证明输出 | 无 | 有 (中/英) |
| LaTeX 导出 | 有 (TikZ/PSTricks) | 有 |
| HTML 导出 | 无 | 有 |
| Coq 导出 | 无 | 有 |
| 符号坐标求解 | 无 | 有 (`solver.h`) |
| 递归构造 | 无 | 有 (`recursion.h`) |
| 不可构造性证明 | 无 | 有 (`UnconstructResult`) |
| 公式 ⇄ 图形双向转换 | 无 | 有 (`formula_module.js`) |

### 7.3 语义对应

| GCLC 概念 | Lv-00 概念 | 映射 |
|-----------|-----------|------|
| 数值坐标 `(10, 20)` | `SymbolicCoord` (符号坐标) | GCLC 坐标可直接转为符号坐标 `symbolic_coord_from_double()` |
| 隐式类型推断 | `GeomType` 枚举 + `TypeRegion` | Lv-00 可选显式类型标注 |
| 构造步骤（线性执行） | `ProofStep` 序列 | GCLC 的每条命令映射为一个 `PROOF_STEP_ADD_NODE` |
| LaTeX 输出 | `proof_export_latex()` | 导出模块 |
| 无证明概念 | `Proposition` + `ProofNavigator` | Lv-00 的核心扩展 |

---

## 8. 编译流水线

### 8.1 阶段概览

| 阶段 | 输入 | 输出 | 对应模块 |
|------|------|------|---------|
| **1. 词法分析** | DSL 文本 | token 流 | `lexer_shared.h`, `FormulaParser` (JS) |
| **2. 语法分析** | token 流 | AST | `FormulaParser.parse()` |
| **3. 几何展开** | AST | `ConstraintGraph` + `FuncBlock[]` | `constraint_graph.h`, `func_block.h` |
| **4. 图归一化** | `ConstraintGraph` (raw) | `ConstraintGraph` (normalized) | `normalization.h` |
| **5. 求解** | 归一化图 | 符号坐标解 | `solver.h` |
| **6. 证明** | 命题 + 归一化图 | 证明树 | `proof.h`, `unify.h` |
| **7. 导出** | 证明树 + 图 | 输出文件 | `proof_export_*()` |

### 8.2 与 Spell 编译器的协同

`SpellCompiler` (四阶段: 开模→提纯→灌注→释放) 与 DSL 编译流水线的对应关系：

| Spell 阶段 | DSL 编译阶段 | 协同方式 |
|-----------|-------------|---------|
| **开模** | 基础构造 (1-3) | `point/line/circle` → 模具框架 |
| **提纯** | 类型标注 + 约束解析 | `:Type` → 元素提纯 |
| **灌注** | 函数块打包 + 重构 | `funcblock` → 结构灌注 |
| **释放** | 证明执行 + 导出 | `prove` → 证明释放 |

---

## 9. 与 Spell 编译器的关系

### 9.1 架构层次

```
┌─────────────────────────────────────────────┐
│              Lv-00 DSL 编译器                │
│  (几何构造 + 计算 + 证明 — 统一语义层)        │
├─────────────────────────────────────────────┤
│              Spell 编译器                     │
│  (法术语义层：开模→提纯→灌注→释放)            │
├─────────────────────────────────────────────┤
│          Lv-00 核心引擎 (C)                    │
│  FuncBlock / ConstraintGraph / ProofNavigator│
│  SymbolicCoord / Unify / Solver              │
├─────────────────────────────────────────────┤
│          Web UI 层 (JS)                       │
│  FormulaModule / SpellCompiler / Renderer    │
└─────────────────────────────────────────────┘
```

### 9.2 DSL 编译器与现有组件的关系

| 现有组件 | 在 DSL 编译中的角色 |
|---------|-------------------|
| `web/js/spell_compiler.js` | 提供四阶段编译框架，可复用为 DSL 的后端语义检查器 |
| `web/js/formula_module.js` | 提供公式解析/渲染/双向转换，DSL 的词法和语法层 |
| `FormulaParser` (JS) | 解析 DSL 文本为 AST (`parse()`, `detectSyntax()`) |
| `FormulaToGraph` (JS) | 将 AST 转换为 `ConstraintGraph` (`convert()`) |
| `GraphToFormula` (JS) | 将 `ConstraintGraph` 反序列化为 DSL 文本 (`convert()`) |
| `include/lv00/func_block.h` | 函数块打包/例化/组合子 —— DSL 的函数抽象编译目标 |
| `include/lv00/proof.h` | 命题/证明导航器/多策略引擎 —— DSL 的证明编译目标 |
| `include/lv00/constraint_graph.h` | 约束图 —— 所有几何构造的底层数据结构 |
| `include/lv00/normalization.h` | 图归一化 —— 编译优化 pass |
| `include/lv00/unify.h` | 合一检查 —— 证明过程中命题模式与构造图的匹配 |
| `include/lv00/solver.h` | 符号坐标求解器 —— 数值计算后端 |
| `include/lv00/type_system.h` | 类型系统 —— DSL 类型标注的验证后端 |

---

## 附录 A：完整 BNF 文法（简化版）

```
<program>       ::= <statement>*

<statement>     ::= <point_decl>
                |   <line_decl>
                |   <circle_decl>
                |   <region_decl>
                |   <funcblock_decl>
                |   <proposition_decl>
                |   <prove_stmt>
                |   <number_decl>
                |   <export_stmt>
                |   <assert_stmt>
                |   ';'   // 空语句

<point_decl>    ::= 'point' <identifier>
                    ( '(' <expr> ',' <expr> ')' | <number> <number> )
                    [ ':' 'Point' ] ';'

<line_decl>     ::= 'line' <identifier> '='
                    ( 'segment' | 'ray' | 'line' )
                    '(' <identifier> ',' <identifier> ')'
                    ';'

<circle_decl>   ::= 'circle' <identifier> '='
                    ( 'circle_center_radius' | 'circle_three_points' |
                      'circumcircle' | 'incircle' )
                    '(' <identifier_list> ')'
                    ';'

<funcblock_decl> ::= 'funcblock' <identifier>
                     '(' <param_list> ')' '->' <type>
                     '{' <statement>* 'return' <identifier> ';' '}'

<proposition_decl> ::= 'proposition' <identifier>
                       '{' <prop_body> '}'

<prop_body>     ::= <given_clause>? <construct_clause>? <prove_clause>

<prove_stmt>    ::= 'prove' <identifier>
                    'using' 'strategy' '=' <strategy_name> ';'

<selector_clause> ::= 'selector' '=' ( 'pos_root' | 'neg_root' |
                     'in_region' | 'nearest_to' | 'custom' )
```

---

## 附录 B：关键词速查索引

```
A
  angle_bisector .......... 角平分线构造
  area_method ............. 面积法证明策略
  assert .................. 断言语句
  assume .................. 数值假设
  axiom ................... 公理声明

B
  between ................. 之间约束

C
  centroid ................ 重心
  circle .................. 圆构造
  circle_center_radius .... 圆心+半径圆
  circle_three_points ..... 三点定圆
  circumcenter ............ 外心
  circumcircle ............ 外接圆
  collinear ............... 共线约束
  compose ................. 函数组合
  concurrent .............. 共点约束
  connect ................. 连接约束
  contain ................. 包含约束
  coordinate .............. 坐标法策略
  curry ................... 部分应用

D
  deductive_db ............ 演绎数据库策略
  direct_construction ..... 直接构造法策略

E
  export_coq .............. 导出 Coq
  export_html ............. 导出 HTML
  export_latex ............ 导出 LaTeX
  export_nl ............... 导出自然语言

F
  full_angle .............. 全角法策略
  funcblock ............... 函数块声明

G
  given ................... 命题给定条件
  grobner_basis ........... Groebner 基策略

I
  incenter ................ 内心
  incircle ................ 内切圆
  incident ................ 关联约束
  instantiate ............. 函数块例化
  intersect ............... 相交约束
  intersection ............ 交点构造

L
  lemma ................... 引理声明
  line .................... 直线构造

M
  midpoint ................ 中点构造

O
  oracle .................. Oracle 求解器策略
  orthocenter ............. 垂心

P
  pack .................... 函数块打包
  parallel ................ 平行线
  perpendicular ........... 垂线
  point ................... 点构造
  pos_root ................ 正根选择器
  precondition ............ 前置条件
  product ................. 函数乘积
  proposition ............. 命题声明
  prove ................... 证明执行

R
  reflect ................. 反射构造
  region .................. 区域构造
  rotate .................. 旋转构造

S
  scale ................... 缩放构造
  segment ................. 线段构造
  strategy ................ 证明策略选择

T
  translate ............... 平移构造

V
  vector_method ........... 向量法策略
  verify .................. 确定性验证
```

---

> **文档结束**  
> 本文档定义了 Lv-00 DSL 的完整语法、编译映射、类型系统和与 GCLC 的对应关系，为 DSL 编译器的实现提供参考规范。
