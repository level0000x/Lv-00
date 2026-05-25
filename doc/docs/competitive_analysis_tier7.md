# Lv-00 竞品分析 — 第七梯队：补充项目（2026-05-24，第三轮调研）

> 本文档在原有 57 个参考项目基础上，新增 18 个项目，覆盖 E-Graphs、纯 FOL ATP、应用范畴论、符号数学库、图描述语言、ML 辅助证明、Julia 科学计算生态等之前未涉及的领域。

---

## 第七梯队新增项目清单

共 18 个项目，按类别分组。

### R. E-Graphs 与 Equality Saturation（重写系统新范式）

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **egg** | [github.com/egraphs-good/egg](https://github.com/egraphs-good/egg) | Rust 实现的 e-graph（等式图）与 equality saturation 框架。**借鉴它的非破坏性重写范式——所有重写结果同时保存在 e-graph 中，不需要选择"先应用哪条规则"。这与 Lv-00 的 `rewrite.h` 面临的核心问题（重写顺序选择）直接对应。e-graph 的 congruence closure 算法可以替代 Lv-00 当前的顺序重写策略** |
| **egglog** | [github.com/egraphs-good/egglog](https://github.com/egraphs-good/egglog) | egg 的 Datalog 扩展——将 equality saturation 与逻辑编程结合，支持增量执行和可组合分析。**借鉴它的"Datalog + e-graph"混合架构——约束图的求解过程天然是 Datalog 风格的（已知事实 → 推导新事实），Lv-00 可将约束传播建模为 Datalog 规则，在 e-graph 框架下执行** |

### S. 应用范畴论与图式化推理

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **Catlab.jl** | [github.com/AlgebraicJulia/Catlab.jl](https://github.com/AlgebraicJulia/Catlab.jl) | Julia 语言的应用/计算范畴论框架。**借鉴它的 GAT（广义代数理论）类型系统——用范畴论的 doctrine（如幺半范畴、对称幺半范畴）定义可计算结构的类型规则。Lv-00 的 `cartesian_closed_category.lvz` 和 `category_theory.lvz` 可以直接借鉴 Catlab 的 GAT 实现——范畴公理可直接编译为可执行代码。它的 wiring diagram（串图）可视化与 Lv-00 的约束图可视化天然对应** |
| **Semagrams.jl** | [github.com/AlgebraicJulia/Semagrams.jl](https://github.com/AlgebraicJulia/Semagrams.jl) | Catlab 生态中的交互式范畴图编辑器。**借鉴它的 wiring diagram 交互编辑 UX——拖拽盒子、连线表示态射、组合图式。Lv-00 Web GUI 的几何构造面板可参考"盒+线"的可视化构造范式** |
| **GATlab.jl** | [github.com/AlgebraicJulia/GATlab.jl](https://github.com/AlgebraicJulia/GATlab.jl) | 从 Catlab 中提取的广义代数理论（GAT）核心。**借鉴它的 GAT 语法编译器——将范畴论声明（`@theory Category{Ob,Hom} begin ... end`）编译为 Julia 类型系统和可执行代码。Lv-00 的公理包加载机制（`.lvz` → 运行时类型系统）与 GATlab 的"理论声明 → 代码生成"管线具有极深的架构共鸣——GATlab 是目前最成熟的开源实现** |

### T. 高性能一阶逻辑自动定理证明器（FOL ATP）

> Z3 和 cvc5 已在第五梯队覆盖（SMT 方向）。以下为纯 FOL ATP，与 SMT 互补。

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **Vampire** | [github.com/vprover/vampire](https://github.com/vprover/vampire) | CASC（世界 ATP 竞赛）常年冠军的 FOL 定理证明器。**借鉴它的 superposition calculus 实现——这是当前 FOL 最有效的完备推理算法。Lv-00 的证明多策略引擎可新增 `PROOF_STRATEGY_SUPERPOSITION` 策略，将几何公理编码为 FOL 子句交由 Vampire 求解。同时借鉴它的 strategy scheduling——不同问题类型自动选择最优策略组合** |
| **E Prover** | [github.com/eprover/eprover](https://github.com/eprover/eprover) | 另一个顶级 FOL ATP，以高性能和模块化著称。**借鉴它的 clause evaluation heuristics（子句评估启发式）——给定大量生成的子句，如何选择最有希望的子句优先处理。Lv-00 的证明搜索回溯机制可借鉴 E 的子句选择策略来优化搜索效率。E 的 SMT-LIB 兼容模式使 Lv-00 可同时尝试 SMT（Z3/cvc5）和 ATP（E/Vampire）后端** |
| **iProver** | [github.com/iprover/iprover](https://github.com/iprover/iprover) | 基于 Inst-Gen 的 ATP，对含有量词的公式特别有效。**借鉴它的 instantiation-based 方法——几何问题中大量涉及"存在一条线通过两点"这种存在量词，iProver 的实例化方法可能在处理含存在量词的几何问题上比 superposition 更高效** |

### U. 符号数学核心库

> SymPy、Maxima、FriCAS、Singular 等 CAS 已在之前的梯队中覆盖。以下为更底层、更适合嵌入 C/C++ 项目的符号数学库。

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **SymEngine** | [github.com/symengine/symengine](https://github.com/symengine/symengine) | C++ 独立符号数学引擎（SymPy 的 C++ 核心）。**借鉴它的表达式存储设计——基于 Intrusive Reference Counting 的不可变表达式 DAG，与 Lv-00 C 内核的符号坐标存储架构高度兼容（都是 C 生态）。它的多语言绑定策略（C/Python/Ruby/Julia/Haskell wrapper）为 Lv-00 的多语言互操作提供了模板。MIT 许可证与 Lv-00 一致** |
| **Symbolics.jl** | [github.com/JuliaSymbolics/Symbolics.jl](https://github.com/JuliaSymbolics/Symbolics.jl) | Julia 的符号-数值混合计算框架。**借鉴它的"符号表达式即 Julia 代码"的透明设计——`@variables x y; expr = x^2 + y^2` 返回的既是 Julia 表达式又是符号表达式，可无缝在符号计算和数值计算之间切换。Lv-00 的"符号路径+数值路径"双轨架构可参考这种透明切换的设计——同一段几何构造代码既是符号的（用于证明）也是数值的（用于可视化）** |
| **AbstractAlgebra.jl** | [github.com/Nemocas/AbstractAlgebra.jl](https://github.com/Nemocas/AbstractAlgebra.jl) | Julia 的纯 Julia 抽象代数实现。**借鉴它的泛型环/域/群设计——`polynomial_ring(QQ, :x)` 一行声明完整的泛型多项式环，所有操作自动继承正确的代数结构。Lv-00 的环论和群论公理包（`ring_theory.lvz`、`group_theory.lvz`）可借鉴这种"声明代数结构→自动获得全套操作"的设计** |

### V. 图描述语言与可视化

> 当前 Lv-00 的约束图可视化通过自定义 Canvas 渲染。以下项目为标准化图描述格式和自动布局引擎，可极大提升约束图可视化的工程质量和开发效率。

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **Graphviz (DOT)** | [graphviz.org](https://graphviz.org) | AT&T 的图可视化工具包，DOT 是一种图描述语言。**借鉴 DOT 的声明式图描述语法——`A -> B [label="距离=5"]` 直接对应约束图中的 `node A→node B [constraint: distance=5]`。Lv-00 可在 `constraint_graph.h` 中新增 `graph_export_dot()` 函数，将约束图导出为 DOT 格式，利用 Graphviz 的自动布局引擎（dot/neato/fdp）渲染。工程上比自研布局算法大幅省力** |
| **Mermaid.js** | [github.com/mermaid-js/mermaid](https://github.com/mermaid-js/mermaid) | 文本驱动的 JS 图表工具，Markdown 友好。**借鉴它的"文本→实时图表"渲染管线——用户在文本编辑器中写 `graph LR; A-->B`，预览面板实时渲染图表。Lv-00 Web GUI 可在约束编辑面板中新增"Mermaid 模式"——输入约束声明文本，实时渲染约束图。Mermaid 的序列图语法甚至可用来可视化证明步骤的时间线** |

### W. ML 辅助定理证明

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **LeanDojo** | [github.com/lean-dojo/LeanDojo](https://github.com/lean-dojo/LeanDojo) | Lean 4 的 ML 辅助证明平台。**借鉴它的 proof tree 数据结构——将 Lean 证明状态建模为可检索、可回放的树结构，支持基于 LLM 的策略推荐。Lv-00 可借鉴此数据模型来增强 `ProofNavigator`——证明树中的每个节点存储（goal, hypotheses, applied_tactic, result），支持回溯和策略搜索** |
| **Pantograph** | [github.com/lenianiva/Pantograph](https://github.com/lenianiva/Pantograph) | Lean 4 的证明交互协议（类 LSP 的设计）。**借鉴它的 proof interaction protocol——通过结构化消息（JSON）在证明引擎和前端之间通信，支持 `goal`、`tactic`、`search` 等操作。Lv-00 的求解器与 Web GUI 之间的通信协议可参考 Pantograph 的消息格式设计** |

### X. 交互式数学平台

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **Desmos API / CL** | [desmos.com/api](https://www.desmos.com/api/v1.11/docs/index.html) | 在线数学图形计算器的 API 和 Computational Layer。**借鉴它的 Computational Layer（计算层）设计——将数学表达式、交互式控件（滑块、按钮）、图形渲染和约束条件统一在声明式脚本中管理。`number("a"): 5` 声明变量、`graph: line through A and B` 声明几何体——这种声明式风格与 Lv-00 DSL 的设计方向一致。Desmos 是该领域用户量最大的产品，其 UX 模式经过海量用户验证** |

### Y. 程序验证语言

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **Dafny** | [github.com/dafny-lang/dafny](https://github.com/dafny-lang/dafny) | Microsoft 的验证感知编程语言，支持 auto-active verification。**借鉴它的"程序+规约一体化"设计——`method Sort(a: array<int>) ensures sorted(a) && multiset(a[..]) == multiset(old(a[..]))` 在同一段代码中混合了可执行逻辑和形式规约。Lv-00 可以借鉴这种"构造=规约"的模式——每次几何构造自动生成对应的约束规约（ensures 子句），构造完成后自动验证规约是否满足。Dafny 的 `calc` 语句（链式计算）也可作为 Lv-00 证明展示的格式参考** |

### Z. Julia 科学计算生态中的建模框架

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **ModelingToolkit.jl** | [github.com/SciML/ModelingToolkit.jl](https://github.com/SciML/ModelingToolkit.jl) | Julia 的符号-数值混合建模框架。**借鉴它的"符号模型→多种后端"编译管道——同一个 `@variables x(t) y(t); @equations D(x) ~ y` 模型定义可以编译为 ODE 求解代码、符号化简表达式、C 代码或 LaTeX 输出。Lv-00 可借鉴这种"单一定义→多目标输出"的编译架构——同一套几何构造可以输出为约束求解、证明验证、可视化渲染、LaTeX 证明文档** |

---

## 第七梯队新增项目详细借鉴要点

### R1. egg — E-Graphs 的非破坏性重写范式

- **e-graph 与 Lv-00 重写系统的根本性差异**：Lv-00 当前 `rewrite.h` 采用顺序重写（选择一条规则→应用→替换→继续），面临"规则选择顺序影响最终结果"的经典问题。egg 的 e-graph 采用 equality saturation：所有重写结果同时保存在等价类（e-class）中，没有"先应用哪条"的选择问题。Lv-00 如果采用 e-graph 作为重写后端，几何公理重写将不再有"重写顺序歧义"
- **congruence closure 的几何语义**：在 e-graph 中，如果 `f(a)` 和 `f(b)` 分别在它们的 e-class 中，且 `a` 和 `b` 被证明相等，那么 `f(a)` 和 `f(b)` 自动合并到同一个 e-class。这在几何中直接对应：如果两点重合、两线段等长、两区域面积相等，那么以它们为构造输入的进一步构造也自动相等
- **Rule 声明语法**：`rewrite!("commute-add"; "(+ ?a ?b)" => "(+ ?b ?a)")` 用模式匹配语法声明重写规则——比 Lv-00 当前的 C 函数式规则注册更简洁。Lv-00 的公理包文件（`.lvz`）可以直接采用类似的模式语法声明几何公理
- **egglog 的 Datalog+e-graph 混合**：约束传播天然是 Datalog 规则（已知两点坐标 → 可推导线段长度 → 可检查是否满足约束），egglog 恰好是为此设计的。Lv-00 可将 `constraint_graph` 的约束传播逻辑迁移到 egglog 式规则引擎上
- **已在编译器优化中大规模验证**：egg 已被用于 Herbie（浮点精度优化）、Ruler（规则推导）、Cranelift（WASM 编译器优化）等工业项目，工程成熟度高

### S1. Catlab.jl — 应用范畴论的编程实现

- **GAT 的"理论→代码"编译**：`@theory Category{Ob,Hom} begin Ob::TYPE; Hom(dom::Ob, codom::Ob)::TYPE; id(a::Ob)::Hom(a,a); compose(f::Hom(a,b), g::Hom(b,c))::Hom(a,c) end`——这个声明自动生成类型检查代码、wiring diagram 可视化、序列化支持。Lv-00 的 `cartesian_closed_category.lvz` 公理包的设计目标与之完全一致——声明范畴结构、自动获得全套操作
- **wiring diagram（串图）作为一等数据结构**：Catlab 将串图（态射的组合图）建模为有类型标注的超图——每个盒子是态射、每条连线是对象、盒子间的连接是组合关系。Lv-00 的约束图本质上是同样结构（约束=态射、几何体=对象、连接=参数传递）
- **Monoidal category 的并行组合**：`f ⊗ g`（张量积）表示两个态射并行组合——在几何中这对应"两个独立几何构造的并置，不共享参数"。Lv-00 的约束图天然支持这种模式，但缺乏显式的语法糖
- **GraphML 和 JSON 序列化**：Catlab 的 wiring diagram 可以导出为 GraphML 或 JSON，在网络传输和存储之间自由切换——Lv-00 的约束图序列化可以借鉴

### S2. Semagrams.jl — 交互式范畴图编辑器

- **拖拽式 wiring diagram 编辑**：用户拖拽盒子（态射）和连线（对象），编辑器自动进行类型检查——连线只能连接类型匹配的端口。Lv-00 的几何构造面板可参考这种"类型感知的拖拽构造"——拖拽"点"到"线段"的端点上时，自动检查类型兼容性
- **分层的组合粒度**：Semagrams 支持多层级——一个盒子内部可以嵌套另一个完整的 wiring diagram。Lv-00 的 func_block 系统（`func_block.h`）恰好支持这种嵌套——函数块内部可以包含子构造

### T1. Vampire — Superposition Calculus 的工业级实现

- **Superposition + 几何的潜力**：许多几何公理可以直接编码为 superposition 友好的形式。例如"经过两点有且仅有一条直线"可以编码为 Horn 子句。Vampire 在 TPTP 几何问题集上的表现已经被初步验证
- **Strategy scheduling**：Vampire 内置数十种预定义策略（根据子句数量、符号分布、深度等特征自动选择），运行时会依次尝试多种策略直到找到证明。Lv-00 的 `proof_multi_strategy` 目前是手工配置策略——可借鉴 Vampire 的特征驱动自动策略选择
- **AVATAR architecture**：Vampire 的 AVATAR 模式将 SAT 求解器与 superposition 结合——SAT 处理命题结构、superposition 处理一阶逻辑。Lv-00 的证明引擎可考虑类似的混合架构——约束图求解（处理几何结构）+ superposition（处理逻辑推导）

### T2. E Prover — 子句评估启发式的设计智慧

- **子句选择是 ATP 的核心瓶颈**：E Prover 的核心创新在于子句评估函数（clause evaluation functions）——给定 10 万个已推导出的子句，如何选择下一个最有希望的子句进行推理。Lv-00 的证明搜索树中同样面临"先尝试哪条推理路径"的选择问题
- **SINDE 策略**：E Prover 的 SINDE 是一种基于符号计数的子句权重函数——更短、含更少新符号的子句优先。Lv-00 的几何证明搜索可参考类似的启发式——"涉及更少额外构造步骤的证明路径"优先探索
- **Proof output**：E Prover 可以以 TPTP TSTP 格式输出完整证明（包括推导步骤和使用的公理），Lv-00 如果有 ATP 后端集成，可以先用 E/Vampire 找到证明，再将证明转换为 Lv-00 的 `ProofNavigator` 格式

### U1. SymEngine — C++ 符号数学引擎

- **与 Lv-00 C 内核的技术栈完全对齐**：SymEngine 是 C++ 库（提供 C wrapper），Lv-00 是 C 项目——两者可以直接在 C ABI 层面互操作。SymEngine 的表达式 DAG 设计（`Basic` 基类 → `Add/Mul/Symbol/Integer` 等派生类）与 Lv-00 的 `SymbolicCoord` 类型层次高度兼容
- **任意精度 + 快速数值路径**：SymEngine 可选编译为 GMP/FLINT/MPFR 任意精度模式或 LLVM JIT 编译模式（直接生成机器码执行表达式），这对应 Lv-00 的"符号路径（精确证明）+ 数值路径（快速可视化）"双轨设计
- **已集成的数学操作**：SymEngine 已实现代数化简、三角恒等变换、微积分（微分/积分）、级数展开——Lv-00 如果需要更丰富的代数操作，直接复用 SymEngine 远比自己重新实现高效

### U2. Symbolics.jl — 符号-数值混合的透明切换

- **符号和数值是同一个对象**：`@variables x y; expr = x^2 + y^2` 返回一个 `Num` 类型——可以 `substitute(expr, x=>1.0)` 得到数值结果，也可以 `simplify(expr)` 得到符号化简。Lv-00 的"几何体即程序"理念可以升级为"几何体既是符号的也是数值的"
- **`build_function` 的代码生成**：`build_function(expr, [x, y])` 将符号表达式编译为原生 Julia 函数——Lv-00 的 Python binding 可以借鉴类似机制，将符号几何构造编译为可执行的高性能计算函数
- **ModelingToolkit.jl 的"单一源码→多目标"编译**：定义一次模型，编译为 ODE 求解、优化问题、LaTeX 文档——Lv-00 可以类似地"定义一次几何构造，编译为约束求解、证明验证、可视化、LaTeX 证明"

### V1. Graphviz (DOT) — 声明式图描述语言

- **DOT 语法与约束图的直接映射**：`node1 -> node2 [label="距离=5", color=red]` 直接对应 `graph_add_line_segment(g, node1, node2)` 加约束标注。`subgraph cluster` 可以表示嵌套的几何子系统
- **自动布局算法**：Graphviz 提供 dot（层级）、neato（弹簧模型）、fdp（力导向）、sfdp（大规模）、circo（环形）、twopi（径向）六种布局引擎——Lv-00 的约束图可视化可根据图规模自动选择布局算法
- **HTML-like labels**：DOT 支持 `label=< <TABLE>...</TABLE> >` 的 HTML 标签——Lv-00 的约束图节点可以嵌入更丰富的几何属性展示（坐标、约束类型、自由度等）
- **工程成熟度极高**：Graphviz 已稳定维护 30+ 年，有 Python/JS/WASM 等多种绑定，可以直接嵌入 Lv-00 Web GUI 的前端渲染管道

### W1. LeanDojo — ML 辅助证明的数据基础设施

- **proof tree 数据结构是通用概念**：LeanDojo 用 `(goal, hypotheses, tactic, children[])` 的四元组表示证明树节点——这个结构与 Lv-00 的 `ProofNavigator` 中的 `proof_step` 结构天然对应。Lv-00 可以借鉴这个数据模型来增强证明树的可检索性和回放能力
- **检索增强生成（RAG）的前置预处理**：LeanDojo 的核心功能是从 mathlib4 中检索与当前证明目标相关的引理——Lv-00 如果有足够的证明数据积累，可以训练类似的检索模型，在用户做几何证明时自动推荐可用的几何定理
- **证明回放器**：LeanDojo 提供证明回放功能——将之前成功的证明步骤在新的环境下重新执行，验证其有效性。Lv-00 的证明系统可以支持"证明回放"——当公理包升级后，自动回放受影响的证明检查是否仍然成立

### X1. Desmos API / CL — 声明式数学交互

- **Computational Layer（CL）的声明式范式**：`number("a"): 5`、`graph: circle((0,0), a)`、`sketch: polygon(A,B,C)`——用声明式脚本同时管理数据绑定、几何渲染和交互逻辑。Lv-00 DSL 可以直接借鉴 CL 的声明式语法风格
- **双向绑定的数学控件**：Desmos 的滑块（slider）和数学表达式之间是双向绑定的——拖动滑块改变数值→所有引用该数值的几何体实时更新。Lv-00 的几何交互反馈（`solver_feedback_solve()`）可参考这种模式
- **经过海量用户验证的 UX**：Desmos 是全球使用最广泛的数学交互工具之一，其几何操作（拖拽点、选中线段、测量角度）的交互模式经过数亿次用户操作验证——Lv-00 Web GUI 的交互设计不应另起炉灶，而应参考 Desmos 验证过的模式

### Y1. Dafny — 程序与规约的一体化

- **`ensures` 子句与构造验证**：`method ConstructPerpendicularBisector(A: Point, B: Point) returns (l: Line) ensures l.isPerpendicularTo(Line(A,B)) && l.passesThrough(Midpoint(A,B))`——Dafny 的"方法+后置条件"设计直接映射到 Lv-00 的"几何构造+命题"模式。用户声明一个构造，同时声明它应满足的性质，系统自动验证
- **`calc` 语句的证明呈现**：`calc { A == B; == { lemma1; } C; } ` 用链式计算语法展示证明步骤——可读性极高。Lv-00 的证明输出格式可借鉴 `calc` 的"链式等式推导"风格
- **auto-active verification**：Dafny 的核心思想是"验证器自动运行，但用户手工引导"——证明器在后台持续尝试验证，用户提供的 `assert`/`invariant` 作为"提示"引导验证器。Lv-00 的交互式证明可以借鉴这种"自动+手工引导"的模式

---

## 三条新的核心设计线索

总结全部 **75 个**参考项目（57 个原有 + 18 个新增），可提炼出三条此前未被充分认识的新设计线索：

| 新线索 | 代表项目 | 核心启示 | Lv-00 对应模块 |
|:---|:---|:---|:---|
| **非破坏性重写（E-Graphs）** | egg, egglog | 重写不应是选择性和破坏性的——所有可能的重写结果应同时保存在 e-graph 中等价类中，最终通过 extraction 选出最优结果。这彻底解决了 Lv-00 `rewrite.h` 的"重写顺序选择"难题 | 重写引擎 (`rewrite.h`)、约束传播 (`constraint_graph`) |
| **声明式构造=类型检查=可视化** | Catlab.jl, ModelingToolkit.jl, Desmos CL | 一份声明（几何构造/GAT 理论/model 定义）自动获得：类型检查（编译期验证）+ 可视化（wiring diagram/几何图形）+ 代码生成（多后端编译）。Lv-00 的标志性"构造=计算=证明"可以扩展为"构造=计算=证明=可视化=代码生成" | DSL 编译器、公理包、Web GUI |
| **ATP/SMT 混合后端战略** | Vampire, E Prover, iProver (+ 已有的 Z3, cvc5) | 没有一种求解策略在所有几何问题上最优。最佳实践是"多后端自动调度"：将同一几何问题编码为 SMT-LIB2（给 Z3/cvc5）和 TPTP FOF（给 Vampire/E），并自动选择最先返回结果的后端 | 证明多策略引擎 (`proof_multi_strategy`)、求解器调度 (`engine_scheduler`) |

### 新增的独特贡献

| 新增贡献 | 代表项目 | 对 Lv-00 的关键意义 |
|:---|:---|:---|
| **e-graph 重写范式** | egg | 几何重写不再需要"先选哪条规则"——所有等价形式同时存在，证明即等价类中的路径搜索 |
| **GAT 编译管线** | Catlab.jl / GATlab.jl | 范畴论公理 → 类型系统 → 可执行代码的自动编译，Lv-00 公理包机制可从此大规模受益 |
| **FOL ATP 补充 SMT** | Vampire, E | SMT（Z3/cvc5）和 ATP（Vampire/E）在不同类型的几何问题上各有所长，同时接入是最优策略 |
| **符号-数值透明切换** | Symbolics.jl | "同一段构造代码，符号执行给出精确证明，数值执行给出实时可视化" |
| **声明式图描述语言** | Graphviz DOT, Mermaid.js | 约束图可视化直接从自研变为工业标准格式，开发效率提升一个数量级 |
| **领域模型→多目标编译** | ModelingToolkit.jl | 一份几何构造定义为约束求解、证明验证、可视化、代码生成、LaTeX 的五合一产出 |

---

*创建日期：2026-05-24*
