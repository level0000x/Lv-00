# Lv00 元语言学术规范

> **适用范围**：本文定义 Lv00 几何元语言的词法、语法、类型系统、符号系统、操作语义和指称语义。该规范是词法语法解析层的权威文档，也是后续基础几何公理层、约束拓扑规约层、多策略自动推理层和输出证明编译层的输入契约。

---

## 0. 执行评估量规

| 维度 | 合格标准 | 验收方式 |
|---|---|---|
| 词法完整性 | 标识符、数值、注释、关键字、符号均有定义 | lexer/parser 测试覆盖 |
| 语法完整性 | 覆盖几何声明、约束语句、逻辑判断、运算表达式、证明命令 | BNF 审查与样例解析 |
| 类型严谨性 | 几何实体、度量值、命题、证明对象不可隐式混用 | 类型检查测试 |
| 语义明确性 | 每类语句均有操作语义与指称语义 | 文档审查与 IR 映射测试 |
| 层间契约 | 解析层只输出 AST/Typed IR，不调用推理与输出层 | 静态依赖检查 |

---

## 1. 语言定位

Lv00 是一种**强类型形式化几何专用元语言**，用于统一表达几何构造、约束建模、逻辑命题、代数演算和形式证明。

### 1.1 核心属性

1. **强类型形式化专用元语言**
   - 所有标识符必须绑定到确定类型。
   - 几何对象、度量值、逻辑命题、证明对象不可隐式混用。
   - 类型错误必须在进入约束拓扑规约层前被拒绝。

2. **声明式建模 + 命令式演算双重特性**
   - 声明式部分用于描述几何实体、约束、公理、目标命题。
   - 命令式部分用于触发计算、归一化、证明、导出等过程。

3. **几何实体为一等公民对象**
   - 点、线、圆、线段、射线、角、三角形、多边形等几何实体可以被声明、绑定、传参、作为命题对象引用。
   - 几何实体不是普通数值的别名，而是具有独立语义域的对象。

### 1.2 设计边界

词法语法解析层仅负责：

```text
Source Text → Token Stream → AST → Typed AST → Geometric IR
```

解析层不得直接执行：

- 约束图归一化；
- Groebner 基求解；
- SMT/ATP 校验；
- 证明链输出；
- 图形可视化或跨语言导出。

---

## 2. 词法规范

### 2.1 字符集

Lv00 源文件使用 UTF-8 编码。关键字使用 ASCII；数学符号可使用 Unicode 或 ASCII 等价写法。

### 2.2 空白与注释

```bnf
Whitespace      ::= " " | "\t" | "\r" | "\n"
LineComment     ::= "//" Character* Newline
BlockComment    ::= "/*" Character* "*/"
```

空白与注释不参与语义，但保留源码位置用于诊断、错误报告和证明溯源。

### 2.3 标识符

```bnf
Identifier      ::= Letter (Letter | Digit | "_")*
Letter          ::= "A".."Z" | "a".."z" | UnicodeLetter
Digit           ::= "0".."9"
```

约定：

- 几何点通常使用大写单字母：`A`、`B`、`C`。
- 线、圆、变量可使用小写或描述性名称：`l`、`c1`、`radius`。
- 系统保留以 `lv00_` 和 `__` 开头的标识符。

### 2.4 数值字面量

```bnf
Number          ::= Integer | Rational | Decimal
Integer         ::= Digit+
Rational        ::= Integer "/" Integer
Decimal         ::= Digit+ "." Digit+
```

数值默认不作为浮点证明事实。解析层保留原始表示，代数计算内核再决定其所属数域。

### 2.5 字符串字面量

```bnf
String          ::= '"' StringChar* '"'
StringChar      ::= Character - '"' | EscapeSequence
EscapeSequence  ::= "\\n" | "\\t" | "\\r" | "\\\"" | "\\\\"
```

字符串仅用于标签、导出路径、说明文本，不得作为数学事实参与推理。

---

## 3. 关键字

```text
Point Line Circle Segment Ray Angle Triangle Polygon Scalar Bool Proposition Proof
Let Constraint Assume Assert Prove Compute Normalize Export Import Theorem Axiom
forall exists not and or implies iff true false bottom
collinear parallel perpendicular equal congruent tangent incident between on inside outside
length distance angle area midpoint center radius diameter intersect
```

关键字大小写敏感。

---

## 4. 符号系统

### 4.1 逻辑符号

| 符号 | ASCII | 含义 |
|---|---|---|
| `∀` | `forall` | 全称量词 |
| `∃` | `exists` | 存在量词 |
| `¬` | `not` | 否定 |
| `∧` | `and` | 合取 |
| `∨` | `or` | 析取 |
| `→` | `implies` 或 `->` | 蕴含 |
| `↔` | `iff` 或 `<->` | 等价 |
| `⊥` | `bottom` | 矛盾 |
| `⊢` | `|-` | 可推导 |
| `⊨` | `|=` | 语义满足 |

### 4.2 度量符号

| 符号/函数 | ASCII | 类型 | 含义 |
|---|---|---|---|
| `|AB|` | `length(A,B)` | `Scalar` | 线段长度 |
| `d(A,B)` | `distance(A,B)` | `Scalar` | 两点距离 |
| `∠ABC` | `angle(A,B,C)` | `Angle` | 角 |
| `m(∠ABC)` | `measure(angle(A,B,C))` | `Scalar` | 角度量 |
| `Area(ABC)` | `area(triangle(A,B,C))` | `Scalar` | 面积 |
| `r(c)` | `radius(c)` | `Scalar` | 圆半径 |

### 4.3 约束算子

| 算子 | ASCII | 含义 |
|---|---|---|
| `=` | `==` | 相等约束 |
| `≠` | `!=` | 不等约束 |
| `∥` | `parallel` | 平行 |
| `⊥` | `perpendicular` | 垂直 |
| `≅` | `congruent` | 全等/同余 |
| `∈` | `on` / `in` | 隶属/在其上 |
| `~` | `similar` | 相似 |

### 4.4 推导符号

| 符号 | ASCII | 含义 |
|---|---|---|
| `⊢` | `derive` | 语法可推导 |
| `⊨` | `models` | 语义满足 |
| `⇒` | `=>` | 计算/归一化推出 |
| `⇝` | `rewrite` | 重写 |
| `∴` | `therefore` | 因此 |

---

## 5. 完整 BNF 上下文无关文法

### 5.1 程序结构

```bnf
Program             ::= ModuleDecl? ImportDecl* Statement*
ModuleDecl          ::= "module" QualifiedName ";"
ImportDecl          ::= "import" QualifiedName ("as" Identifier)? ";"
QualifiedName       ::= Identifier ("." Identifier)*
Statement           ::= DeclarationStmt
                    |   ConstraintStmt
                    |   AssumeStmt
                    |   AssertStmt
                    |   ProveStmt
                    |   LetStmt
                    |   ComputeStmt
                    |   NormalizeStmt
                    |   ExportStmt
                    |   AxiomStmt
                    |   TheoremStmt
```

### 5.2 几何声明

```bnf
DeclarationStmt     ::= EntityType IdentifierList ";"
IdentifierList      ::= Identifier ("," Identifier)*
EntityType          ::= "Point" | "Line" | "Circle" | "Segment" | "Ray"
                    |   "Angle" | "Triangle" | "Polygon" | "Scalar"
                    |   "Bool" | "Proposition" | "Proof"
```

示例：

```lv00
Point A, B, C;
Line l;
Circle c;
Scalar r;
```

### 5.3 绑定与构造语句

```bnf
LetStmt             ::= "Let" Identifier ":" Type "=" Expr ";"
Type                ::= EntityType | FunctionType | SetType
FunctionType        ::= "(" TypeList? ")" "->" Type
TypeList            ::= Type ("," Type)*
SetType             ::= "Set" "<" Type ">"
```

示例：

```lv00
Let s : Segment = segment(A, B);
Let t : Triangle = triangle(A, B, C);
```

### 5.4 约束语句

```bnf
ConstraintStmt      ::= "Constraint" ConstraintExpr ";"
ConstraintExpr      ::= RelationExpr
                    |   MetricExpr CompareOp MetricExpr
                    |   ConstraintExpr LogicalAnd ConstraintExpr
RelationExpr        ::= RelationName "(" ArgList? ")"
RelationName        ::= "collinear" | "parallel" | "perpendicular"
                    |   "congruent" | "tangent" | "incident"
                    |   "between" | "on" | "inside" | "outside"
CompareOp           ::= "=" | "==" | "!=" | "≠" | "<" | "<=" | ">" | ">="
LogicalAnd          ::= "and" | "∧"
ArgList             ::= Expr ("," Expr)*
```

示例：

```lv00
Constraint collinear(A, B, C);
Constraint parallel(line(A,B), line(C,D));
Constraint length(A,B) = length(A,C);
```

### 5.5 逻辑判断语句

```bnf
AssumeStmt          ::= "Assume" LogicExpr ";"
AssertStmt          ::= "Assert" LogicExpr ";"
ProveStmt           ::= "Prove" LogicExpr ";"
AxiomStmt           ::= "Axiom" Identifier ":" LogicExpr ";"
TheoremStmt         ::= "Theorem" Identifier ":" LogicExpr ProofBlock?
ProofBlock          ::= "{" ProofCommand* "}"
ProofCommand        ::= "by" Identifier ";"
                    |   "apply" Identifier ";"
                    |   "normalize" ";"
                    |   "contradiction" ";"
                    |   "qed" ";"
```

### 5.6 逻辑表达式

```bnf
LogicExpr           ::= IffExpr
IffExpr             ::= ImpliesExpr (("iff" | "<->" | "↔") ImpliesExpr)*
ImpliesExpr         ::= OrExpr (("implies" | "->" | "→") OrExpr)*
OrExpr              ::= AndExpr (("or" | "∨") AndExpr)*
AndExpr             ::= NotExpr (("and" | "∧") NotExpr)*
NotExpr             ::= ("not" | "¬") NotExpr | QuantifiedExpr | PredicateExpr
QuantifiedExpr      ::= Quantifier BinderList "." LogicExpr
Quantifier          ::= "forall" | "∀" | "exists" | "∃"
BinderList          ::= Binder ("," Binder)*
Binder              ::= Identifier ":" Type
PredicateExpr       ::= RelationExpr | MetricExpr CompareOp MetricExpr | "true" | "false" | "bottom" | "⊥"
```

### 5.7 运算表达式

```bnf
Expr                ::= LogicExpr | MetricExpr | GeometryExpr | Identifier | Literal
MetricExpr          ::= AddExpr
AddExpr             ::= MulExpr (("+" | "-") MulExpr)*
MulExpr             ::= PowExpr (("*" | "/") PowExpr)*
PowExpr             ::= UnaryExpr ("^" UnaryExpr)*
UnaryExpr           ::= ("+" | "-") UnaryExpr | PrimaryExpr
PrimaryExpr         ::= Literal | Identifier | FunctionCall | MeasureExpr | "(" Expr ")"
Literal             ::= Number | String | "true" | "false"
FunctionCall        ::= Identifier "(" ArgList? ")"
MeasureExpr         ::= "length" "(" Expr "," Expr ")"
                    |   "distance" "(" Expr "," Expr ")"
                    |   "angle" "(" Expr "," Expr "," Expr ")"
                    |   "measure" "(" Expr ")"
                    |   "area" "(" Expr ")"
                    |   "radius" "(" Expr ")"
GeometryExpr        ::= "point" "(" MetricExpr "," MetricExpr ")"
                    |   "line" "(" Expr "," Expr ")"
                    |   "circle" "(" Expr "," MetricExpr ")"
                    |   "segment" "(" Expr "," Expr ")"
                    |   "ray" "(" Expr "," Expr ")"
                    |   "triangle" "(" Expr "," Expr "," Expr ")"
```

### 5.8 命令式演算语句

```bnf
ComputeStmt         ::= "Compute" Expr ";"
NormalizeStmt       ::= "Normalize" (Identifier | "all") ";"
ExportStmt          ::= "Export" ExportTarget "as" ExportFormat String? ";"
ExportTarget        ::= Identifier | "proof" | "graph" | "theory"
ExportFormat        ::= "json" | "latex" | "tikz" | "lean" | "coq" | "text"
```

---

## 6. 类型系统

### 6.1 基础类型域

| 类型 | 语义域 | 说明 |
|---|---|---|
| `Point` | 欧氏空间中的点对象 | 一等几何实体 |
| `Line` | 直线对象 | 可由两点构造，需处理重合点退化 |
| `Circle` | 圆对象 | 中心 + 半径或三点定义 |
| `Segment` | 线段对象 | 有端点，长度非负 |
| `Ray` | 射线对象 | 起点 + 方向 |
| `Angle` | 有向或无向角对象 | 需定义度量范围 |
| `Triangle` | 三点构成的三角形对象 | 需处理共线退化 |
| `Scalar` | 数域元素 | 可属于 `Q`、二次代数数域或 `R` |
| `Bool` | 逻辑真假值 | 不等同于命题证明 |
| `Proposition` | 一阶逻辑命题 | 可被证明、假设、组合 |
| `Proof` | 证明对象 | 记录证明链与依赖 |

### 6.2 类型判断

类型环境记为 `Γ`，判断形式为：

```text
Γ ⊢ e : T
```

示例规则：

```text
Γ(A)=Point, Γ(B)=Point
———————————————
Γ ⊢ length(A,B) : Scalar

Γ(A)=Point, Γ(B)=Point, Γ(C)=Point
———————————————————————————————
Γ ⊢ collinear(A,B,C) : Proposition
```

### 6.3 禁止隐式混用

以下均为类型错误：

```lv00
Constraint A = 1;                  // Point 不可与 Scalar 相等
Constraint length(A,B) = line(A,B); // Scalar 不可与 Line 相等
Prove length(A,B);                 // Scalar 不是 Proposition
```

### 6.4 几何退化类型条件

构造函数必须显式声明退化条件：

| 构造 | 正常条件 | 退化情况 |
|---|---|---|
| `line(A,B)` | `A ≠ B` | 重合点无法唯一确定直线 |
| `triangle(A,B,C)` | 三点不共线 | 共线时退化为线性结构 |
| `circle(O,r)` | `r > 0` | `r = 0` 退化为点圆 |
| `angle(A,B,C)` | `A ≠ B ∧ C ≠ B` | 零向量角未定义 |

---

## 7. 操作语义

操作语义描述程序如何从源文本演化为内部状态。

系统状态定义：

```text
Σ = (Γ, E, C, A, G, P)
```

其中：

- `Γ`：类型环境；
- `E`：实体环境；
- `C`：约束集合；
- `A`：假设集合；
- `G`：目标命题集合；
- `P`：证明对象集合。

### 7.1 声明语句

```text
Σ ⟶ Σ[Γ[x ↦ T], E[x ↦ fresh(T)]]
```

示例：

```lv00
Point A;
```

语义：在类型环境中登记 `A : Point`，并在实体环境中创建未约束点对象。

### 7.2 约束语句

```text
Γ ⊢ φ : Proposition
Σ ⟶ Σ[C := C ∪ {φ}]
```

语义：类型检查通过后，将命题作为约束事实加入约束集合。解析层不执行相容性判定，相容性由约束拓扑规约层处理。

### 7.3 假设语句

```text
Γ ⊢ φ : Proposition
Σ ⟶ Σ[A := A ∪ {φ}]
```

假设具有作用域。反证法中的临时假设不得泄漏到全局公理或约束集合。

### 7.4 证明语句

```text
Γ ⊢ φ : Proposition
Σ ⟶ Σ[G := G ∪ {φ}]
```

证明语句只登记目标，不直接证明。证明过程由多策略自动推理层调度。

### 7.5 计算语句

```text
Γ ⊢ e : T
Σ ⟶ Σ[result := eval(e)]
```

计算语句可触发表达式求值，但计算结果若用于证明，必须经数域与证明规则校验。

### 7.6 归一化语句

```text
Σ ⟶ request_normalize(C)
```

解析层仅产生归一化请求，实际拓扑约束合并由约束拓扑规约层执行。

### 7.7 导出语句

```text
Σ ⟶ request_export(target, format)
```

解析层仅记录导出请求，实际格式化由输出证明编译层执行。

---

## 8. 指称语义

指称语义描述语法对象在数学模型中的意义。

### 8.1 模型结构

一个 Lv00 几何模型定义为：

```text
M = (U_P, U_L, U_C, U_S, I, R)
```

其中：

- `U_P`：点域；
- `U_L`：直线域；
- `U_C`：圆域；
- `U_S`：数值域；
- `I`：函数与构造符解释；
- `R`：关系符解释。

### 8.2 实体解释

```text
⟦Point A⟧_M ∈ U_P
⟦Line l⟧_M ∈ U_L
⟦Circle c⟧_M ∈ U_C
```

### 8.3 度量解释

```text
⟦length(A,B)⟧_M = EuclideanDistance(⟦A⟧_M, ⟦B⟧_M)
⟦angle(A,B,C)⟧_M = Angle(⟦A⟧_M, ⟦B⟧_M, ⟦C⟧_M)
```

### 8.4 关系解释

```text
M ⊨ collinear(A,B,C)
iff ⟦A⟧_M, ⟦B⟧_M, ⟦C⟧_M 位于同一直线

M ⊨ parallel(l1,l2)
iff direction(⟦l1⟧_M) 与 direction(⟦l2⟧_M) 线性相关

M ⊨ perpendicular(l1,l2)
iff direction(⟦l1⟧_M) · direction(⟦l2⟧_M) = 0
```

### 8.5 逻辑解释

```text
M ⊨ φ ∧ ψ  iff  M ⊨ φ 且 M ⊨ ψ
M ⊨ φ ∨ ψ  iff  M ⊨ φ 或 M ⊨ ψ
M ⊨ φ → ψ  iff  M ⊭ φ 或 M ⊨ ψ
M ⊨ ¬φ     iff  M ⊭ φ
M ⊨ ∀x:T.φ iff  对所有 a ∈ ⟦T⟧_M, M[x↦a] ⊨ φ
M ⊨ ∃x:T.φ iff  存在 a ∈ ⟦T⟧_M, M[x↦a] ⊨ φ
```

---

## 9. AST 与 Typed IR 契约

解析层输出两级结构：

```text
AST：保留语法结构、源码位置、原始字面量
Typed IR：完成名称绑定、类型检查、符号规约后的结构化对象
```

### 9.1 AST 节点类别

```text
ProgramNode
DeclarationNode
ConstraintNode
AssumeNode
AssertNode
ProveNode
LetNode
ComputeNode
NormalizeNode
ExportNode
LogicExprNode
MetricExprNode
GeometryExprNode
```

### 9.2 Typed IR 对象类别

```text
Lv00TypedEntity
Lv00TypedMetricExpr
Lv00TypedRelation
Lv00TypedProposition
Lv00TypedConstraint
Lv00ProofGoal
Lv00CommandRequest
```

### 9.3 层间传递原则

- 下游层只能读取 Typed IR，不应重新解析源文本。
- Typed IR 中必须包含 `source_span`，用于错误定位和证明溯源。
- Typed IR 不包含推理结果，只表示输入事实和命令请求。

---

## 10. 示例

### 10.1 等腰三角形声明与证明目标

```lv00
Point A, B, C;
Constraint length(A,B) = length(A,C);
Prove angle(A,B,C) = angle(B,C,A);
```

语义：声明三点，加入等腰约束，登记底角相等证明目标。

### 10.2 平行线约束

```lv00
Point A, B, C, D;
Let l1 : Line = line(A,B);
Let l2 : Line = line(C,D);
Constraint parallel(l1, l2);
Assert not perpendicular(l1, l2);
```

注意：若 `l1` 或 `l2` 退化，类型检查或约束相容检测必须报告条件不足。

### 10.3 反证法目标

```lv00
Point A, B, C;
Constraint collinear(A,B,C);
Prove not area(triangle(A,B,C)) > 0;
```

推理层可通过局部反设 `area(triangle(A,B,C)) > 0` 导出矛盾，但该矛盾不得污染全局约束集合。

---

## 11. 错误分类

| 错误类别 | 示例 | 处理层 |
|---|---|---|
| 词法错误 | 非法字符、未闭合字符串 | 词法语法解析层 |
| 语法错误 | 缺少分号、括号不匹配 | 词法语法解析层 |
| 名称错误 | 未声明标识符、重复声明 | 词法语法解析层 |
| 类型错误 | `Point = Scalar` | 词法语法解析层 |
| 退化风险 | `line(A,A)` | 基础几何公理层 / 约束拓扑规约层 |
| 约束矛盾 | `A=B` 且 `A≠B` | 约束拓扑规约层 |
| 推理失败 | 欠约束无法证明 | 多策略自动推理层 |
| 导出失败 | 不支持目标格式 | 输出证明编译层 |

---

## 12. 与五层架构的关系

```text
词法语法解析层：本文定义 Token、BNF、AST、Typed IR。
基础几何公理层：解释 Point/Line/Circle 等实体和基础度量关系。
约束拓扑规约层：消费 Typed Constraint，执行约束图构建与归一化。
多策略自动推理层：消费约束系统与证明目标，生成 Proof Object。
输出证明编译层：消费 Proof Object，生成文本、LaTeX、TikZ、JSON 或跨语言导出。
```

---

## 13. 后续实现要求

1. 新增 parser 行为时，必须先补充 BNF 与解析测试。
2. 新增几何实体时，必须同时定义类型规则、操作语义和指称语义。
3. 新增逻辑算子时，必须定义优先级、结合性和语义解释。
4. 任何浮点近似结果不得直接作为证明事实。
5. 反证法、矛盾闭包、SMT 校验均不得在解析层出现实现依赖。
