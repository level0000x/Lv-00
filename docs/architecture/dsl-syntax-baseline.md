# Lv-00 DSL 现状基线（语法糖吸收前）

> 状态：基线盘点（2026-08-27）
> 用途：作为语法糖设计的对照基准；标注两套 DSL 并存问题

---

## 1. 两套 DSL 并存（关键设计问题）

Lv-00 有**两套独立 DSL 实现，互不引用**：

| | DSL-A（dsl_compiler） | DSL-B（lv_loader / .lv） |
|---|---|---|
| 位置 | layer4_reasoning/dsl/ | layer1_parser/ |
| 风格 | 小写关键字 | 大写实体名 + 小写运算符 |
| 关键字数 | 22 | 45+ |
| 输入形式 | 源码字符串（`dsl_compile_and_load`） | .lv 文件（`lv_load_file`） |
| 语法 | `point A = ...` | `Point A := ...` / `Let x = ...` |
| 能力 | 几何构造 + 约束 + prove | 几何 + 逻辑（forall/exists）+ 度量 + 证明 + 定理 |
| 用户视角 | "太空"（待加语法糖） | 较丰富 |

**问题**：语法糖加在 DSL-A 而不统一两套，会加剧分裂。
设计决策点：语法糖设计**以 DSL-A 为主**（用户指的"太空"那套），
但借鉴 DSL-B 已有关键字（量词/度量/定理）。

---

## 2. DSL-A 语法能力（22 关键字）

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

```c
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

---

## 3. 扩展点（加语法糖的三处）

| 扩展点 | 位置 | 加什么 |
|---|---|---|
| token 表 | dsl_lexer.c 单字符表 + 关键字表 | 新运算符（`->` `?` `@` `..`）、新关键字 |
| AST 类型 | dsl_compiler.h DslASTType | 新语法节点 |
| IR 操作 + handler | dsl_compiler.h DslIROp + dsl_compiler_load.c VTable | 新操作码 + handler |

**现状约束**：token 是单字符表（无多字符运算符如 `->` `=>` `?.`）；
需要扩展为多字符 token 支持（或保留单字符 + 关键字组合）。

---

## 4. DSL-B（.lv）可借鉴的现成语法（勿重复发明）

DSL-B 已有但 DSL-A 缺的语法能力：

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
```

**设计提示**：语法糖不应只加"糖"，还应补齐 DSL-B 已证明有用的语义
（量词、度量、定理声明），避免两套语法能力差距扩大。

---

## 5. 语法糖吸收的约束

1. **手写解析器**：语法糖实现须可落地于递归下降/查表解析。
2. **不破坏现有 IR**：优先"desugar 到现有 IR 操作"，避免新 IR 爆炸
   （如 `midpoint(A,B)` 已是 IR，`A → B` 可 desugar 为 segment(A,B)）。
3. **领域契合**：语法糖须服务于"描述几何构造"（坐标、路径、组合、
   约束、证明），非通用编程糖。
4. **两套 DSL**：语法糖默认加 DSL-A；是否统一两套另行决策。
