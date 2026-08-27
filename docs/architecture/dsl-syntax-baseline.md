# Lv-00 DSL 现状基线（语法糖吸收前）

> 状态：基线盘点 v1.1（2026-08-27）
> 用途：作为语法糖设计的对照基准；标注"lv 家族 vs 独立 dsl_compiler"问题
>
> **v1.1 修正（2026-08-27）**：v1.0 把基线误记为"两套 DSL（dsl_compiler vs
> lv_loader）"。经用户纠正与代码核查，实际是**三格式图景**：用户书写的
> **lv 家族**（`.lv` 规约 + `.lvz` 公理/预设，共享词法基础设施）+
> 引擎内部独立通道 **dsl_compiler**（自建词法，非用户书写语言）。

---

## 1. 三格式图景（关键设计问题）

Lv-00 有**三套语法实现**，其中 `.lv` 与 `.lvz` 同属 lv 家族（共享词法基础），
dsl_compiler 完全独立（自建词法、互不引用）：

| | lv 家族：`.lv` | lv 家族：`.lvz` | dsl_compiler（独立） |
|---|---|---|---|
| 位置 | layer1_parser/ | layer4_reasoning/module + axiom/ | layer4_reasoning/dsl/ |
| 解析器 | lv_loader（7 文件 4534 行） | module_lvz.c（1340 行）+ axiom_pkg_* | dsl_compiler ×5（2052 行） |
| 词法 | 自建 lv_lexer（466 行） | **共享 lexer_shared**（layer2，`/* LvzLexer 是 lvLexer 的别名 */`） | 自建 dsl_lexer（281 行） |
| 输入 | `.lv` 规约文件（`lv_load_file`） | `.lvz` 公理/预设文件（`module_load` / `lvz_load_presets_file`） | 源码字符串（`dsl_compile_and_load`），无专属扩展名 |
| 语法 | `Point A := ...` / `forall` / `Prove` | `axiom "x" "1.0.0" { template "t" N }` | `point A = ...` / `constraint {}` |
| 关键字/能力 | 45+ 关键字：几何+逻辑（forall/exists）+度量+证明+定理 | axiom/template/deps/exports | 22 小写：几何构造+约束+prove |
| 仓库文件数 | **140 个** .lv（bootstrap/src 等） | **149 个** .lvz（axiom_packages 93 + presets 56） | 无用户文件 |
| 用户视角 | ✅ **用户书写**（规约/证明） | ✅ **用户书写**（公理设置/预设） | ❌ **引擎内部通道**（lv_prove / L7 编排器 / L9 应用层调用） |

**核心结论**：
1. **用户书写的语言是 lv 家族**（`.lv` + `.lvz` 两类文件），共享同一词法
   基础（`lvLexer` 结构体 + `lexer_shared` 的空白/注释/字符串处理），
   是同一套语言家族的两个文件格式。
2. **dsl_compiler 不是用户书写语言**——它是引擎内部的"字符串 → 约束图"
   便捷编译通道：`lv_prove(ctx, goal)`、L7 编排器 GEOMETRY 阶段、
   L9 应用层 EXPORT/VISUALIZE（`build_graph_from_file` 读任意文本文件后
   当作 dsl 源码编译，`input_format` 默认 `"dsl"`）。
3. **问题**：语法糖若加给 dsl_compiler，等于给"用户不写的语言"加糖，收益
   归零；真正该加糖的是 `.lv`（规约/证明）与 `.lvz`（公理）——但两者已
   有较丰富的语法（45+/axiom/template），"太空"的是 dsl_compiler 的观感
   只影响引擎内部通道。

---

## 2. dsl_compiler（独立通道）语法能力（22 关键字）

> 记录其语法形态仅作为内部通道契约；语法糖设计**不以它为主**（v1.2 决策）。

### 2.1 关键字

```
bisector centroid circle circumcenter constraint fix free
incenter intersect let line load midpoint orthocenter parallel
perpendicular point polygon prove ray segment triangle
```

### 2.2 单字符 token

```
= ( ) { } [ ] , ; :
```

### 2.3 AST 类型（24 种）

声明：POINT/LINE/CIRCLE/SEGMENT/RAY/POLYGON/TRIANGLE（`x = ...`）
构造：INTERSECT/PARALLEL/PERPENDICULAR/MIDPOINT/CIRCUMCENTER/
      ORTHOCENTER/CENTROID/INCENTER/BISECTOR
语句：CONSTRAINT/PROVE/LOAD/FIX_POINT/FREE_POINT/BLOCK/LET
基础：PROGRAM/IDENT/NUMBER

### 2.4 IR 操作码（30 种）

创建：POINT/POINT_FIXED/LINE/CIRCLE/SEGMENT/RAY/POLYGON/TRIANGLE
构造：INTERSECT/PARALLEL_THROUGH/PERPENDICULAR_THROUGH/MIDPOINT/
      CIRCUMCENTER/ORTHOCENTER/CENTROID/INCENTER/BISECTOR/ANGLE_BISECTOR
约束：ADD_CONSTRAINT/REMOVE_CONSTRAINT/EQUAL/PARALLEL/PERPENDICULAR/
      COLLINEAR/CONCYCLIC
其他：LOAD_AXIOM/PROVE/CHECK_SAT/LABEL/NOOP

### 2.5 语法形态（现状）

```
// 声明（仅 = 赋值式）
point A = free;
point B = fix 10 20;
line l = segment(A, B);
circle k = circle(A, B);
point M = midpoint(A, B);

// 约束块
constraint {
  parallel(l, m);
}

// 证明
prove { collinear(A, B, C); }

// 加载公理
load "euclidean";
```

### 2.6 M5 遗留注记

dsl_compiler.c 头注释自称"实现 .lv 源文件的完整编译流水线"——**与实现脱节**：
它从不按 `.lv` 语法解析（`.lv` 归 lv_loader），实现的实为自身 GCLC 风格
小写关键字语法。该注释为 M5 遗留，已记录待修正。

---

## 3. 扩展点（若需扩展 dsl_compiler 的三处）

| 扩展点 | 位置 | 加什么 |
|---|---|---|
| token 表 | dsl_lexer.c 单字符表 + 关键字表 | 新运算符（`->` `?` `@` `..`）、新关键字 |
| AST 类型 | dsl_compiler.h DslASTType | 新语法节点 |
| IR 操作 + handler | dsl_compiler.h DslIROp + dsl_compiler_load.c VTable | 新操作码 + handler |

**现状约束**：token 是单字符表（无多字符运算符如 `->` `=>` `?.`）；
需要扩展为多字符 token 支持（或保留单字符 + 关键字组合）。
（注：`->` ARROW 已以特例形式存在于 dsl_lexer.c:212-219。）

---

## 4. lv 家族可借鉴的现成语法（语法糖的落点）

### 4.1 `.lv`（lv_loader）已有能力

```
// 逻辑量词
forall x. P(x)
exists x. Q(x)

// 度量
distance(A, B) = 5;
area(triangle ABC) = 10;
length(segment AB)

// 定理/命题
Theorem thm := ...
Proposition p := ...

// 模块/导入
import "module.lv";
module M := ...

// 强类型声明
Point Lexer := lex(input: SourceText) -> TokenStream;
```

### 4.2 `.lvz`（module_lvz / axiom_pkg）已有能力

```
// 公理包声明
axiom "euclidean_plane" "1.0.0" { ... }

// 约束模板（Legal Constructors & Metrics）
template "line_through_two_points" 2
template "pasch_axiom" 4

// 依赖/导出
deps { ... }
exports { ... }
```

**设计提示**：语法糖应加在用户书写的 lv 家族（`.lv` / `.lvz`），并吸收
DSL-A 已证明有用的几何构造简写（`midpoint(A,B)`、`constraint{}` 等），
避免"引擎内部通道语法反而比用户语言顺口"的反常。

---

## 5. 语法糖吸收的约束（v1.2 决策后更新）

1. **手写解析器**：语法糖实现须可落地于递归下降/查表解析。
2. **不破坏现有 IR**：优先"desugar 到现有 IR 操作"，避免新 IR 爆炸
   （如 `midpoint(A,B)` 已是 IR，`A → B` 可 desugar 为 segment(A,B)）。
3. **领域契合**：语法糖须服务于"描述几何构造"（坐标、路径、组合、
   约束、证明），非通用编程糖。
4. **语法糖落点（v1.2 决策）**：以**用户书写的 lv 家族**为主（`.lv` /
   `.lvz`）；dsl_compiler 作为引擎内部通道不优先加糖（其 22 关键字
   简写可反向移植到 lv 家族）；是否统一三套另行决策。
