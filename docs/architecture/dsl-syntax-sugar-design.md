# Lv-00 DSL 语法糖设计（四路研究汇总 v1.0）

> 状态：设计深化（2026-08-27），**仅设计不执行**
> 前置文档：dsl-syntax-baseline.md（两套 DSL 基线 + 三扩展点）
> 输入：四路子代理研究（命令式 / 函数式 / 领域 DSL / 证明语言与宏系统）

---

## 1. 研究输入汇总（四路）

| 路 | 覆盖语言/工具 | 核心候选 |
|---|---|---|
| 命令式/脚本 | Python, JS/TS, Ruby, Go, Kotlin, Swift | 关键字参数、解构赋值、列表推导、spread、命名元组、`?:`/`?.`、默认参数、guard、with 上下文、管道 |
| 函数式/声明式 | Elixir, F#, Haskell, Clojure, Prolog, CLP, SQL | `\|>` 管道、let 链式、数据解构、`obj.x` 字段访问、case/match、CLP 约束、中缀运算符、prove 的"结论:-前提"、WITH/视图、数据字面量 |
| 领域 DSL | GeoGebra, TikZ/Asymptote, Solvespace/FreeCAD, Manim, CadQuery, OpenSCAD, Lean/Isabelle | 坐标字面量 `A=(1,2)`、自动命名、路径 `A--B--C`、Sequence 序列、自定义中缀 notation、具名约束原语、选择器 DSL、VGroup、`$fn` 精度变量、have/show |
| 证明语言与宏 | Lean, Coq, Isabelle, Rust macro_rules | 声明式模板宏、宏展开时机、by-tactic 块、have/show、calc 链式推理、证明项导出、Section 前提区、命名空间+选择性导入、Unicode 中缀+优先级表、类型注解 |

**四路交集**（都被反复提及、领域价值最高）：
1. **管道 `|>`**（函数式 + 命令式都首推）——几何"从已知推导未知"天然流水线
2. **解构/多返回值**（函数式 + 命令式）——`intersect` 双交点高频
3. **坐标/数据字面量**（领域 + 函数式）——`A = (1,2)` 砍样板
4. **中缀约束 notation**（领域 + 函数式 + 证明语言）——`a ⟂ b` 像数学
5. **批量构造**（列表推导 / Sequence / map / 路径）——点集处理刚需

---

## 2. 汇总后候选池（去重，24 项）

### 2.1 表达式级糖（desugar 到现有 IR，第一批）

| # | 糖 | 来源 | 价值 | token 增量 | AST 增量 | IR 增量 |
|---|---|---|---|---|---|---|
| S1 | 坐标字面量 `A = (1, 2)` | GeoGebra/Clojure | 高 | 0（复用 `(` `,` `)`） | 声明右侧解析 | **0**（→ IR_CREATE_POINT_FIXED） |
| S2 | 管道 `A \|> midpoint(B)` | Elixir/F# | 高 | +1（`\|>` 多字符） | 0（复用调用节点） | **0**（desugar 嵌套调用） |
| S3 | 解构赋值 `let (P,Q) = intersect(l,m)` | JS/TS/Clojure | 高 | 0 | +1（LET 扩展多目标） | +1（多结果操作数） |
| S4 | 字段访问 `A.x` / `k.radius` | OCaml/Rust | 中高 | +1（`.`） | +1（FIELD_ACCESS） | 0（解释期查字段表） |
| S5 | 关键字参数 `circle(center=A, through=B)` | Python/Swift | 高 | 0 | 实参节点扩展 | **0**（按名绑形参） |
| S6 | 默认参数 | Python | 高 | 0 | 构造形参表 | **0**（编译期补默认实参） |
| S7 | 自动命名（匿名构造） | GeoGebra | 高 | 0 | 0（解析器生成名） | **0**（隐式符号表项） |
| S8 | 数据字面量 `[A, B, C]`（列表） | Clojure | 中高 | 0（`[` 已有） | +1（LIST_LITERAL） | **0**（polygon/onCircle 操作数展开） |

### 2.2 约束/证明级糖（复用现有约束 IR，第二批）

| # | 糖 | 来源 | 价值 | token | AST | IR |
|---|---|---|---|---|---|---|
| S9 | 中缀约束 `a ⟂ b` / `C on l` | Lean/Isabelle/F# | 高 | +3（`⟂` `∥` `on` + ASCII 别名） | +1（INFIX_CONSTRAINT） | **0**（→ IR_CONSTRAIN_* / ADD_CONSTRAINT） |
| S10 | 具名约束原语 `distance(A,B) = 5` | Solvespace/FreeCAD | 高 | 0 | constraint 块内新谓词 | **0**（→ IR_CONSTRAIN_EQUAL） |
| S11 | have/show 中间结论 | Lean/Coq | 高 | 0 | prove 块内子语句 | **0**（→ IR_PROVE 序列化断言） |
| S12 | 量词/度量补齐 | DSL-B 已有 | 高 | +2（forall/exists 借 DSL-B） | +2 | **0**（→ IR_CHECK_SAT/约束） |
| S13 | 路径语法 `A--B--C` | TikZ/Asymptote | 中高 | +1（`--` 多字符） | +1（PATH） | **0**（→ N×IR_CREATE_SEGMENT） |
| S14 | with 上下文块（单位/精度/公理集） | Python/Kotlin/OpenSCAD `$fn` | 中高 | 0（复用块文法） | +1（WITH） | +1（环境切换指令） |
| S15 | 批量构造 Sequence/map | GeoGebra/Ruby | 高 | 0 | +1（SEQUENCE） | +1（迭代展开指令） |

### 2.3 系统级糖（需要架构决策，第三批）

| # | 糖 | 来源 | 价值 | 说明 |
|---|---|---|---|---|
| S16 | 声明式模板宏（func_block 升级） | Rust macro_rules | 高 | 用户可定义新构造模板；宏展开时机=A→B 之间（AST 后、IR 前） |
| S17 | 命名空间 + 选择性导入 | Coq/Lean/ML | 中高 | `namespace`/`open`/`hiding` 解决 load 全局名冲突 |
| S18 | 证明项导出（Curry-Howard） | Lean/Coq | 架构基石 | 与 L9 证书管线衔接，复用 proof_compiler |
| S19 | case/match 谓词分派 | Haskell/Rust | 中 | constraint/prove 内按谓词头分派 + 未覆盖检查 |
| S20 | 空值兜底 `?:` / 可选链 `?.` | Kotlin/Swift/JS | 高 | 需要 nullable 构造结果贯穿语义层，属语义级 |
| S21 | guard 存在性短路 | Swift | 中高 | 依赖 S20 空值语义 |
| S22 | 选择器 DSL / VGroup | CadQuery/Manim | 中 | 一组对象加同一约束 |
| S23 | 命名记录（构造结果带字段） | Python namedtuple | 中高 | 配合 S3/S4 |
| S24 | 计算链 calc | Lean | 中 | 链式推理证明 |

---

## 3. 设计原则（desugar 优先，防 IR 爆炸）

1. **任何糖必须可 desugar 到 ≤1 个新 IR 操作码**；优先 0 新增（纯语法重写）。
2. **三扩展点各自最小化**：能只改 token 的绝不动 AST；能只改 AST 的绝不动 IR handler。
3. **语法糖不改变现有 22 关键字语义**：所有现有写法保持向后兼容（旧脚本零改动）。
4. **领域优先**：几何痛点（多返回值 / 点集批量 / 构造不存在 / 约束自然化）先做，通用编程糖（unless/defer/海象）不做。
5. **两套 DSL 正视但不强并**：糖默认加 DSL-A；每个糖若 DSL-B 已有等价语义（量词/度量/定理）则直接补齐而非重造。

---

## 4. 逐项设计（S1-S15 详细，S16-S24 概要）

### S1 坐标字面量 `A = (1, 2)`

```
point A = (1, 2);        // == point A = fix 1 2;
point B = free;          // 保留
```

- **token**：0 新增（`(` `,` `)` 已在单字符表）。
- **AST**：`parse_decl_stmt` 声明右侧增加分支——遇 `(` 解析 `NUMBER , NUMBER`，生成 `DSL_AST_POINT_DECL`（子节点 2×NUMBER）。
- **IR**：**0 新增**。IR 生成器在声明右侧为坐标组时直接发射 `IR_CREATE_POINT_FIXED`（已有）。
- **歧义处理**：`(` 在声明位置不可能出现在表达式首位（现表达式均为 ident 开头），无歧义。
- **工作量**：解析 ~40 行 + IR ~10 行 + 测试 ~60 行。

### S2 管道 `|>`

```
point M = A |> midpoint(B);          // == point M = midpoint(A, B);
line   h = A |> perpendicular(line l); // == perpendicular(l, A) 经形参表重排
```

- **token**：+1 多字符 `|>`。**前置**：dsl_lexer 单字符表需支持多字符 token（现仅 `->` 例外，直接复用该路径）。
- **AST**：**0 新增**。解析期直接重写——`lhs |> f(args)` 等价于 `f(lhs, args)`（f 为构造函数时 lhs 前置首参；f 为约束谓词时 lhs 前置）。在 parse_primary 内做语法重写，AST 仍是现有函数调用节点。
- **IR**：**0 新增**。
- **优先级**：管道是**最便宜的几何糖**——纯 tokenizer + 解析期重写，四路研究一致首推。
- **工作量**：tokenizer ~30 行 + 重写 ~50 行 + 测试 ~80 行。

### S3 解构赋值 `let (P, Q) = intersect(l, m)`

```
let (P, Q) = intersect(l, m);     // 双交点
let (T, U) = tangent(k, A);       // 双切点（若构造已支持）
```

- **token**：0 新增（`(` `,` `)` 已有）。
- **AST**：`parse_let_stmt` 增加左值元组分支：`DSL_AST_LET` 的 name 字段改为 children[0]（名称列表）。新增轻量 `DSL_AST_TUPLE_PATTERN` 或在 LET 内嵌 ident 列表。
- **IR**：+1 关键增量——`DslIROperation` 目前 `result_id` 单值。方案：**不新增操作码，扩展 DslIR 结果表**——`IR_INTERSECT` 的 result_id 后跟随第二结果 id（`result_id2`），或引入 `IR_PAIR_BIND`（把两个已有 id 绑定为对）。优先选前者（改结构不改 op 语义）。
- **工作量**：解析 ~50 行 + IR 结构 ~40 行 + handler ~30 行 + 测试 ~100 行。

### S4 字段访问 `A.x` / `k.radius`

```
distance(A, B);      // 现用法保留
let dx = B.x - A.x;  // 坐标字段
```

- **token**：+1 单字符 `.`。
- **AST**：+1 `DSL_AST_FIELD_ACCESS`（name = 对象名，child[0] = 字段名）。
- **IR**：**0 新增**。几何对象字段表（点 x/y、圆 center/radius、线 a/b/c…）在解释器侧查表；字段访问 desugar 为取值指令（解释期求值，不需新 op）。
- **注意**：字段访问引入"表达式出现在构造参数之外"的新形态（S4/S3 需要表达式语句位）。现 parse_stmt 只认声明/构造/let——需在 let 右侧与约束参数位放开表达式文法。
- **工作量**：token ~10 行 + AST ~20 行 + 求值 ~40 行 + 测试 ~60 行。

### S5 关键字参数

```
circle k = circle(center = A, through = B);   // == circle(A, B)
circle k = circle(center = A, radius = 3);    // 半径形态
line   a = parallel(through = A, to = line l); // == parallel(l, A)
```

- **token**：0 新增（`=` 已有；需在参数列表内允许 `name = expr`）。
- **AST**：实参节点加 flag（named），name 字段存形参名。
- **IR**：**0 新增**。每个构造函数的形参表（如 circle: center/through/radius）在 IR 生成器侧按名绑定；未识别形参名报错。
- **组合**：S5 + S6 默认参数一起做，`radius` 可默认 1。
- **工作量**：解析 ~40 行 + 形参表 ~60 行 + 测试 ~80 行。

### S6 默认参数

```
circle k = circle(center = A);     // radius 默认 1
polygon P = polygon(vertices = [A, B, C], closed = true);
```

- **token/AST**：0 新增（复用 S5 的具名实参）。
- **IR**：0 新增——IR 生成器在实参缺失时填入默认值。
- **工作量**：~40 行 + 测试 ~50 行。

### S7 自动命名（匿名构造）

```
intersect(l, m);              // 自动生成 point P1 = ...（P1 可后续引用）
midpoint(A, B);               // P2
```

- **token/AST/IR**：全部 0 新增。解析器在"裸构造语句"（无 `name =` 前缀）时自动生成 `P<n>` 符号（复用现有符号表），IR 照常发射。
- **语义约束**：自动命名只在后续有引用时才真正进符号表（否则 IR 不变，纯惰性命名）。
- **工作量**：~30 行 + 测试 ~50 行。

### S8 数据字面量 `[A, B, C]`

```
polygon P = polygon([A, B, C]);   // == polygon(A, B, C)
```

- **token**：0 新增（`[` `]` 已有）。
- **AST**：+1 `DSL_AST_LIST_LITERAL`。
- **IR**：0 新增——IR 生成器把列表展开为操作数序列（polygon 变参）。
- **组合**：与 S3 解构、S15 批量构造配套。
- **工作量**：~30 行 + 测试 ~40 行。

### S9 中缀约束 notation

```
constraint {
  l ⟂ m;             // == perpendicular(l, m);
  a ∥ b;             // == parallel(a, b);
  C on l;            // == ADD_CONSTRAINT 包含
}
```

- **token**：+3 多字符/Unicode（`⟂` `∥` `on`）+ ASCII 别名（`perp` `par` `on`）。**前置**：lexer 需支持 UTF-8 运算符识别（按字节序列匹配）。
- **AST**：+1 `DSL_AST_INFIX_CONSTRAINT`（lhs, op, rhs）。
- **IR**：**0 新增**——desugar 到 `IR_CONSTRAIN_PERPENDICULAR` / `IR_CONSTRAIN_PARALLEL` / `IR_ADD_CONSTRAINT`（全部已有）。
- **优先级表**：`on` > `⟂`/`∥`（简单二元，无需复杂优先级；但为 S20 空值合并留优先级空位）。
- **工作量**：lexer UTF-8 ~40 行 + AST/desugar ~50 行 + 测试 ~80 行。

### S10 具名约束原语

```
constraint {
  distance(A, B) = 5;        // == IR_CONSTRAIN_EQUAL(dist(A,B), 5)
  length(segment AB) = 10;   // 借 DSL-B 度量语法
  angle(A, B, C) = 90;
}
```

- **token**：0 新增。
- **AST**：constraint 块内谓词表扩展（distance/length/angle 谓词，arg 可为几何对象或数值）。
- **IR**：0 新增（→ IR_CONSTRAIN_EQUAL）。**注意**：度量求值（distance 等）已有基础设施（DSL-B 度量 + 求解器），DSL-A 侧需接同一度量后端。
- **工作量**：谓词解析 ~60 行 + 度量接线 ~80 行 + 测试 ~100 行。

### S11 have/show 中间结论

```
prove {
  have mid_on_AB: midpoint(A, B) on segment AB;
  show collinear(A, B, C);
}
```

- **token**：0 新增（`have`/`show` 作为 prove 块内上下文关键字）。
- **AST**：prove 块内子语句类型（HAVE/SHOW），child 为断言。
- **IR**：0 新增——prove 块的断言序列化，have 断言进入前提集（axiom_rule_engine 消费），show 为最终目标（IR_PROVE 已有）。
- **工作量**：解析 ~50 行 + 前提集接线 ~40 行 + 测试 ~80 行。

### S12 量词/度量补齐（借 DSL-B）

```
constraint { forall x in P: on(x, line l); }   // 全称约束
```

- **token**：+2 关键字（forall/exists，与 DSL-B 同名）。
- **AST**：+2（FORALL/EXISTS）。
- **IR**：0 新增（→ IR_CHECK_SAT / 约束展开）。这是**补能力不是加糖**——两套 DSL 能力差距先收窄再谈统一。
- **工作量**：~80 行 + 测试 ~80 行。

### S13 路径语法 `A--B--C`

```
path p = A--B--C;          // == 两段 segment：AB, BC（或折线）
```

- **token**：+1 多字符 `--`。
- **AST**：+1 `DSL_AST_PATH`。
- **IR**：0 新增——desugar 为 N 个 `IR_CREATE_SEGMENT`（或聚合为 `IR_CREATE_POLYGON` 当闭合）。
- **工作量**：lexer ~20 行 + 解析 ~40 行 + desugar ~30 行 + 测试 ~60 行。

### S14 with 上下文块

```
with unit=degrees, precision=0.01, axioms="euclidean" {
  point A = (1, 0);
  constraint { distance(A, B) = 3; }
}
```

- **token**：0 新增（复用块文法；`=` 已有）。
- **AST**：+1 `DSL_AST_WITH`（配置子句 + 块）。
- **IR**：+1 环境切换指令（PUSH_ENV/POP_ENV 或 WITH_BEGIN/WITH_END），解释器侧压栈恢复。
- **替代**：也可 desugar 为现有 IR 的 LABEL + 元数据，但环境语义需要显式栈，建议 +1 指令（唯一允许 S14 破例）。
- **工作量**：~80 行 + 测试 ~80 行。

### S15 批量构造 Sequence

```
points = sequence(i in 0..8) { div(A, B, i) };   // 等分点族
midpoints = sequence(p in [A, B, C]) { midpoint(A, p) };
```

- **token**：0 新增（`..` 需 +1 多字符，或复用 `:` 区间）。
- **AST**：+1 `DSL_AST_SEQUENCE`（迭代变量 + 区间 + 体）。
- **IR**：+1 迭代展开指令（SEQUENCE_BEGIN/END），解释期逐元素发射体内构造。**备选**：编译期静态展开（区间常数时直接展开为 N 条 IR，0 新增 op）——推荐先做静态展开，动态区间二期。
- **工作量**：解析 ~60 行 + 展开 ~50 行 + 测试 ~100 行。

### S16-S24 概要（第三批，架构决策后）

| # | 设计要点 | 依赖 |
|---|---|---|
| S16 模板宏 | func_block 组合预设升级为可写声明式模板：`template trisect(A, B) { ... }`；占位符 + 卫生性 gensym；**宏展开时机 = AST 后 IR 前**（架构决策） | func_block 现设施 |
| S17 命名空间 | `namespace geo {}` / `open geo` / `import ... hiding X`；符号表加前缀作用域，解决 load 全局名冲突 | 符号表改造 |
| S18 证明项导出 | prove 成功后导出证明项（Lean/Coq 源码），与 L9 证书管线衔接 | proof_compiler |
| S19 case/match | constraint/prove 内按谓词头分派 + 未覆盖谓词静态检查报错 | prove 扩展 |
| S20 空值兜底 `?:` | "构造可能不存在"语义：`let r = k.radius ?: 1;` 需 nullable 贯穿表达式层 | 语义层改造（大） |
| S21 guard | `guard P = intersect(l, m) else { ... }` 存在性短路 | S20 |
| S22 选择器/VGroup | `constraint { parallel(l, >Z); }` 一组对象加同一约束 | 对象集合 |
| S23 命名记录 | 构造结果带字段（intersect→{first,second}）自解释，配 S3/S4 | S3/S4 |
| S24 calc 链 | `calc { a = b; b = c; show a = c; }` 链式推理 | S11 |

---

## 5. 明确不做（防价值陷阱）

| 候选 | 来源 | 否决理由 |
|---|---|---|
| 空格并置应用 `midpoint A B` | Haskell | 需类型消歧，手写解析器歧义爆炸 |
| 全量惰性求值 | Haskell | 需重写逐条解释式求值器 |
| 完整所有权/借用系统 | Rust | 已用"copy/take/borrow 三态 + 审计"替代，解释器语境过重 |
| Go 零值 | Go | 掩盖"自由点 vs 原点 (0,0)"区分，几何语义必须显式 |
| Go defer | Go | 几何构造无资源句柄/IO 清理语义 |
| Promise 链 `.then` | JS | 构造是纯同步，无异步；管道 S2 已覆盖流水线形态 |
| unless | Ruby | 收益过低，纯否定糖 |
| 海象 `:=` | Python | 偏命令式紧凑，与命名式风格相悖 |
| Prolog 完整回溯/unification | Prolog | 只做"结论:-前提"确定性推导（S11 已覆盖），完整回溯超出解释器成本 |

---

## 6. 实施路线（三批，全部待用户确认后执行）

### 第一批：表达式级（S1-S8）——约 900-1100 行
纯语法重写为主，IR 仅 S3 扩展结果表。**前置**：lexer 多字符 token 支持（S2/S13 依赖，~80 行）。
覆盖痛点：砍样板（S1/S7）、流水线（S2）、多返回值（S3）、命名可读（S4/S5/S6）、批量传参（S8）。

### 第二批：约束/证明级（S9-S15）——约 1200-1600 行
复用现有约束 IR（0 新 op，S14 例外 +1）。覆盖痛点：约束自然化（S9/S10）、分步证明（S11）、量词能力补齐（S12）、批量构造（S15）。

### 第三批：系统级（S16-S24）——约 2500-4000 行
依赖架构决策（宏展开时机、命名空间、nullable 语义、证明项导出接 L9）。逐个立项，不做则留档。

---

## 7. 两套 DSL 分裂的处置建议

现状：DSL-A（22 关键字）与 DSL-B（45+ 关键字）互不引用，语法糖默认加 DSL-A 会加剧分裂。

**决策选项**（供用户选择，本次不执行）：
- **选项 A（推荐）**：语法糖只加 DSL-A，但 S12 量词/度量补齐从 DSL-B 语义反向移植，先收窄能力差距；统一留到 DSL-A 语法糖稳定后（糖本身是"两套语法如何统一"的试金石）。
- **选项 B**：同步为 DSL-B 定义等价映射表（每个 DSL-A 糖给出 .lv 等价写法），双轨演进。
- **选项 C**：立即合并两套 DSL（重写 loader 为 compiler 前端）——大重构，与"十层架构不可动"冲突，**不建议本次做**。

---

## 8. 工作量汇总

| 批次 | 新增行数（估） | 新 IR op | 新 AST 类型 | 新 token |
|---|---|---|---|---|
| 第一批 S1-S8 | 900-1100 | 0（S3 扩展结构） | 4（FIELD_ACCESS/LIST_LITERAL/TUPLE_PATTERN/具名实参） | 2（`\|>` `.`） |
| 第二批 S9-S15 | 1200-1600 | 1（S14 环境指令） | 5（INFIX/WITH/SEQUENCE/PATH/HAVE-SHOW） | 5（`⟂` `∥` `on` `--` `..`） |
| 第三批 S16-S24 | 2500-4000 | 视立项 | 视立项 | 视立项 |
| **合计** | **4600-6700** | 1-2 | 9+ | 7+ |

对比基线：DSL-A 现实现约 3000 行（lexer+parse+IR+load），语法糖全量落地后 DSL-A 将翻倍至 ~1 万行——规模仍在 L4 单模块可控范围（最大 .c 现 <2000 行，语法糖按模块拆分后单文件仍 <1500 行）。

---

## 9. 风险与缓解

| 风险 | 缓解 |
|---|---|
| lexer 多字符/UTF-8 支持改动波及现有 22 关键字解析 | 多字符匹配放单字符表**之后**（先试单字符，不中再试多字符），现有 token 零回归 |
| S3 结果表扩展改 DslIR 结构，影响 IR 消费方（load/导出） | 结果表扩展向后兼容：result_id2 默认 -1，现有消费方按单结果路径不变 |
| S14 环境指令破坏"逐条解释"简单性 | PUSH/POP 成对压栈，解释器只加一个环境栈，不改变单条指令语义 |
| 语法糖全量后 DSL-A 变大，回归面扩大 | 每个糖独立契约测试（build3 + ctest + Python），按批次提交 |
| 糖之间交互（S2 管道 × S5 关键字参数 × S9 中缀） | 设计规则：管道只作用于构造/谓词调用；中缀只在 constraint 块内；具名实参只在调用内——三者语法位置互斥，无歧义 |

---

*附：本设计全部为"深化不执行"，落地顺序与选项 A/B/C 待用户确认。*
