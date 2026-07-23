# Lv-00 竞品分析与生态定位

> 本文档记录了 Lv-00 项目在几何计算、形式化验证、自动证明等交叉领域的竞品调研结果，
> 明确了 Lv-00 在生态中的独特定位，以及从各标杆项目中可借鉴的设计理念。

---

## 一、值得借鉴的项目清单

按借鉴价值从高到低排列：

### 🥇 第一梯队：直接相关，必看

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **LeanGeo** | [github.com/ahumenberger/LeanGeo](https://github.com/ahumenberger/LeanGeo) | 在 Lean 里做几何的形式化，看它的公理体系怎么组织、证明步骤怎么结构化。**借鉴它证明呈现的清晰度** |
| **GeoCoq** | [github.com/GeoCoq/GeoCoq](https://github.com/GeoCoq/GeoCoq) | Coq 里最成熟的几何证明库，Tarski 公理体系。**借鉴它的公理分层、模块化组织方式** |
| **AlphaGeometry** | [github.com/google-deepmind/alphageometry](https://github.com/google-deepmind/alphageometry) | DeepMind 的奥林匹克几何 AI。**借鉴它如何用自然语言输出可读的证明步骤，这个用户体验设计极好** |
| **Newclid** | [github.com/leomlopes/newclid](https://github.com/leomlopes/newclid) | 学习证明搜索的策略设计。**借鉴它的证明回溯和可视化思路** |

### 🥈 第二梯队：设计理念可参考

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **CGAL** | [cgal.org](https://www.cgal.org) | 计算几何的工业标准。**借鉴它的 API 文档组织方式、模块分类逻辑** |
| **Solvespace** | [solvespace.com](https://solvespace.com) | 几何约束求解器。**借鉴它的交互设计理念——用户怎么拖拽约束、怎么看到实时反馈** |
| **Penrose** | [penrose.cs.cmu.edu](https://penrose.cs.cmu.edu) | 将数学符号自动转为图形。**借鉴它"用代码描述数学关系，自动生成可视化"的叙事方式** |
| **PyEuclid** | [github.com/euclid-ai/pyeuclid](https://github.com/euclid-ai/pyeuclid) | Python 几何证明库。**借鉴它的高层 API 设计，看怎么让用户用最简单语法表达复杂几何关系** |

### 🥉 第三梯队：细分功能可借鉴

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **Kingdon** | [github.com/tBuLi/kingdon](https://github.com/tBuLi/kingdon) | Python 几何代数库。**借鉴它符号计算的用户体验——怎么让符号运算看起来像数值运算一样直观** |
| **FRONTIER** | [github.com/UL-FRI-LG/FRONTIER](https://github.com/UL-FRI-LG/FRONTIER) | 几何约束求解。**借鉴它的约束图可视化表达** |
| **GeoGebra** | [geogebra.org](https://www.geogebra.org) | 教育领域的几何交互标杆。**借鉴它对几何对象的命名和引用体系** |
| **GAP** | [gap-system.org](https://www.gap-system.org) | 计算群论系统。**借鉴它的包管理和生态建设思路** |

### 🆕 第四梯队：新增补充项目（2026-05-24）

以下 10 个为补充调研发现的项目，按类别分组。

#### A. 几何定理自动证明

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **JGEX / GEX** | [github.com/kovzol/Java-Geometry-Expert](https://github.com/kovzol/Java-Geometry-Expert) | 中科院张景中团队的几何证明里程碑系统。**借鉴它的多证明方法并存引擎、可读证明生成算法、C-tree约束分解策略** |

#### B. 几何代数（Geometric Algebra）

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **GAlgebra** | [github.com/pygae/galgebra](https://github.com/pygae/galgebra) | 基于 SymPy 的符号几何代数库。**借鉴它的操作符重载 API（`A*B`=几何积）、度规参数化声明（`Ga('e', g=[1,-1,-1,-1])`）、符号化简管道** |
| **clifford** | [github.com/pygae/clifford](https://github.com/pygae/clifford) | 数值几何代数库。**借鉴它的预定义代数命名空间、flat array 存储策略、Numba JIT 加速** |
| **Ganja.js** | [github.com/enkimute/ganja.js](https://github.com/enkimute/ganja.js) | JS 几何代数代码生成器+可视化引擎。**借鉴它的 inline AST 转译 DSL 技术（Lv-00 DSL 编译器最直接的技术参考）、跨语言 codegen、度规参数化** |
| **Grassmann.jl** | [github.com/chakravala/Grassmann.jl](https://github.com/chakravala/Grassmann.jl) | Julia 多线性微分几何代数系统。**借鉴它的编译期类型级代数（零运行时开销）、TensorAlgebra 类型层级、子代数互操作性** |

#### C. 符号几何计算

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **SymPy Geometry** | [github.com/sympy/sympy](https://github.com/sympy/sympy) (geometry) | Python 符号几何计算模块。**借鉴它的 GeometryEntity 层次体系、符号参数化 API、交点/距离计算方法命名约定** |

#### D. 工业级几何内核与 CAD 框架

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **OpenCASCADE (OCCT)** | [dev.opencascade.org](https://dev.opencascade.org) | 工业级开源几何建模内核。**借鉴它的 7 模块分层架构（Lv-00 软件架构最重要分层参考）、B-Rep 拓扑与几何分离、规范化命名与引用体系** |
| **CadQuery** | [github.com/CadQuery/cadquery](https://github.com/CadQuery/cadquery) | Python 代码驱动参数化 CAD 框架。**借鉴它的 Fluent API 链式调用（Lv-00 DSL 语法直接 UX 参考）、Selector DSL（`faces(">Z")`）、Workplane 抽象** |
| **build123d** | [github.com/gumyr/build123d](https://github.com/gumyr/build123d) | Python 代数模式 3D CAD 框架。**借鉴它的代数模式无状态设计（与 Lv-00"构造即运算"高度一致）、操作符变换语法（`Plane.XZ * Pos(X=5) * Rectangle(1,1)`）、双模式架构** |

#### E. 国产几何内核生态

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **OpenGeometry Group** | [opengeometry.cn](https://opengeometry.cn) | 中国首个开源云几何内核。**借鉴它的联盟共建生态模式、云原生架构、代数路径与数值路径分离的工程实践** |

### 🆕 第五梯队：第二轮补充项目（2026-05-24，第二次调研）

共 15 个项目，按类别分组。

#### F. 几何构造语言与自动证明

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **GCLC** | [github.com/janicicpredrag/gclc](https://github.com/janicicpredrag/gclc) | 几何构造语言（GC Language）+ 三种自动证明方法（面积法/吴方法/Gröbner基法）合一的经典系统。**借鉴它的 GC 语言语法设计（`point A 10 20`、`line a A B`）、证明方法热切换机制、WASM Web 移植经验（C++→Emscripten→TypeScript GUI）**——与 Lv-00 的"几何构造+证明"双料定位最为接近 |
| **Cinderella** | [cinderella.de](https://cinderella.de) | 交互几何先驱（由 Jürgen Richter-Gebert 和 Ulrich Kortenkamp 开发），基于投影几何+复数运算的核心引擎。**借鉴它的随机化定理验证（Randomized Theorem Checking）、连续运动下保持构造一致性的核心算法、从 Java Applet 到 Web 的跨时代移植路径** |
| **Dr. Geo** | [gnu.org/software/dr-geo](https://www.gnu.org/software/dr-geo/) | 交互几何 + Smalltalk 脚本编程。**借鉴它将几何构造与脚本语言深度绑定的设计——用户在 GUI 中画几何体同时生成可编程的 Smalltalk 脚本，与 Lv-00"几何即程序"理念一致** |

#### G. 形式化证明辅助工具

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **mathlib4 EuclideanGeometry** | [github.com/leanprover-community/mathlib4](https://github.com/leanprover-community/mathlib4) | Lean 4 社区统一数学库中的欧氏几何形式化，覆盖 Birkhoff 公理体系和 Tarski 公理体系。**与 LeanGeo 互补——mathlib4 是活跃社区主流路径，借鉴它在 Lean 4 类型系统下如何优雅表达 IncidenceGeometry、介于性（Betweenness）、全等性（Congruence）等基础几何关系** |
| **ProofWidgets4** | [github.com/leanprover-community/ProofWidgets4](https://github.com/leanprover-community/ProofWidgets4) | Lean 4 证明交互式可视化组件框架。**借鉴它的证明目标渲染、前提选择面板、证明步骤高亮的设计——这些是 Lv-00 Web GUI 证明面板可参考的 UX 模式。支持自定义 React 组件直接嵌入 Lean 证明环境** |
| **Arend** | [arend-lang.github.io](https://arend-lang.github.io) | JetBrains 开发的基于同伦类型论（HoTT）的定理证明器。**借鉴它的路径类型（Path Type）语法设计——用 `p : a = b` 表示路径（等价于等式证明），与几何直觉天然对应。它的类 Java 语法降低了形式化入门门槛** |
| **mm0 / Metamath** | [github.com/digama0/mm0](https://github.com/digama0/mm0) | 超小型证明验证器（verifier < 2000 行 C 代码）及其元语言 MM0。**借鉴它的极简验证内核设计——验证器只做替换检查（substitution check），不内建任何逻辑，真正做到了"元语言"的最小化。这是 Lv-00 内核精简的重要参照** |

#### H. 约束求解与 SMT 后端

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **Z3** | [github.com/Z3Prover/z3](https://github.com/Z3Prover/z3) | Microsoft 的工业级 SMT 求解器。**借鉴它作为 Lv-00 约束图求解的后端引擎——将几何约束编码为 SMT 公式（非线性实数算术 NRA 理论），利用 Z3 的 CDCL(T) 引擎求解。已有 Python/C API 可直接集成** |
| **cvc5** | [github.com/cvc5/cvc5](https://github.com/cvc5/cvc5) | 高性能 SMT 求解器（Stanford/CMU/Iowa 联合开发）。**借鉴它比 Z3 更灵活的理论组合能力——可同时处理非线性算术+未解释函数+量词，适合 Lv-00 混合几何计算与逻辑推理的场景** |

#### I. 计算机代数系统（CAS）

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **Singular** | [singular.de](https://www.singular.de) | 专精多项式系统/Gröbner 基的计算机代数系统，代数几何的核心工具。**借鉴它的多项式理想计算——它是 Lv-00 的 Gröbner 基证明方法（对标 JGEX 的 Gröbner 基法）的底层引擎候选。它的脚本语言给 Lv-00 代数操作的 DSL 语法提供参考** |
| **Macaulay2** | [macaulay2.com](https://macaulay2.com) | 交换代数和代数几何研究计算系统。**借鉴它的"先声明环，再在环上做计算"的编程范式——`R = QQ[x,y,z]` 定义多项式环 → `I = ideal(x^2+y^2-z^2)` 定义理想 → 计算理想属性。这与 Lv-00"先声明公理系统，再做几何计算"的范式高度类似** |
| **polymake** | [polymake.org](https://polymake.org) | 多面体几何计算系统（用 Perl 粘合多种 C++ 库）。**借鉴它的"多后端引擎+统一客户端"架构——用户用 Perl 脚本描述多面体，后台动态路由到 cdd、lrs、libnormaliz 等不同引擎。这给 Lv-00"多种求解策略透明切换"的工程架构提供直接参考** |

#### J. Web 数学交互组件

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **MathLive** | [github.com/arnog/mathlive](https://github.com/arnog/mathlive) | 高质量 Web 数学公式输入组件（React/Vue/Svelte 可嵌入）。**借鉴它的所见即所得数学编辑器设计——支持 LaTeX 输入实时渲染、虚拟键盘、命令补全。这是 Lv-00 Web GUI 中 FormulaPanel/公式输入框的 UI 组件直接参考** |
| **CortexJS** | [github.com/cortex-js/compute-engine](https://github.com/cortex-js/compute-engine) | 浏览器端数学计算引擎（基于 MathJSON 格式）。**借鉴它的 MathJSON 标准化中间表示——`["Add", "x", 2]` 之类的语义化 JSON。Lv-00 的 Web 端与前端的通信协议可参考这种结构化的数学表示** |
| **jsTikZ / TikZJax** | [tikzjax.com](https://tikzjax.com) | 在浏览器中将 TikZ 代码编译为 SVG。**借鉴它的纯前端几何渲染管道：TikZ 源码 → WebAssembly LaTeX 引擎 → SVG 输出。Lv-00 Web 端几何可视化可参考"源码→WASM 编译→前端渲染"的架构** |

#### K. 最小化形式化语言

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **mai** | [github.com/amka66/mai](https://github.com/amka66/mai) | 教育用途的数学解释器，用 Prolog 中推理规则直接定义一阶逻辑+ZFC集合论。**借鉴它的极简主义哲学——整个形式化基础用推理规则表定义，不存在"代码"和"逻辑"的分离。这种"逻辑即代码"的设计与 Lv-00"构造即证明"属于同一种极简设计思维** |

### 第五梯队（新增项目）详细借鉴要点

#### F1. GCLC — 几何构造语言与自动证明的合体

- **GC 语言语法可直接映射到 Lv-00 DSL**：`point A 10 20`、`line a A B`、`circle k A B`、`intersection C a b`——每个命令同时是几何构造步骤和证明前提的声明。Lv-00 DSL 可设计为 `point A(10,20); line a(A,B); circle k(A,B); intersect C = a ∩ k` 的链式语法，保持 GC 语言"构造即声明"的精髓
- **三种证明方法的热切换机制**：GCLC 用 `-a`（面积法）、`-w`（吴方法）、`-g`（Gröbner基法）命令行参数切换证明方法。Lv-00 已经实现了 `ProofMultiStrategy` 的多策略框架（`proof.h`），GCLC 的方法选择经验可直接映射——在 `ProofStrategyType` 枚举中新增 `PROOF_STRATEGY_AREA` 和 `PROOF_STRATEGY_WU`，通过配置切换
- **WASM Web 移植的技术路线验证**：GCLC 2024年完成了 `C++ → Emscripten → WASM → TypeScript Web GUI` 的移植，前端用 CodeMirror 做代码编辑 + Lezer 做语法高亮。Lv-00 的 `web/lv_web_bindings.c` + `web/lv_web_bindings_v2.c` 已经为 WASM 绑定做好了准备，GCLC 的移植经验验证了这条技术路线的可行性
- **LaTeX 证明输出的可读性设计**：GCLC 将证明步骤生成为完整的 LaTeX 文档（包含 `\begin{proof}` 环境），每个推理步骤附带引用的公理/定理编号。Lv-00 的 `proof_export_latex()` 可参考这个格式，增强可读性
- **30年项目的生存经验**：GCLC 从 1995 年开发至今，历经多次代码重写（纯 C → C++），仍保持向后兼容。这种长期维护的设计智慧值得 Lv-00 关注

#### F2. Cinderella — 连续几何运动下的构造一致性

- **随机化定理验证（Randomized Theorem Checking）**：Cinderella 不依赖符号计算，而是通过随机实例化坐标点并检查构造的数值一致性来验证定理——这种方法虽不严格，但极快，适合交互场景下的"快速反馈"。Lv-00 可在 `prover.h` 中新增 `proof_randomized_check()` 函数，作为完整符号证明的快速预筛选
- **投影几何+复数运算的核心引擎**：Cinderella 选择投影几何作为底层模型（而非欧氏几何），因为投影几何在连续运动下具有更好的封闭性。Lv-00 的"公理中立"设计天然支持这种替换——可将 `projective_geometry.lvz` 公理包作为默认底层加载
- **从 Java Applet 到 Web 的跨时代移植路径**：Cinderella 经历了 Applet → Web Start → HTML5 的完整移植，这对 Lv-00 的长期技术演进有参考价值
- **定理的连续性保持**：当用户拖拽几何对象时，Cinderella 保证即使跨越奇异配置（如两条平行线突然相交），定理的证明结构仍保持一致。这对 Lv-00 `solver_feedback_solve()` 中的交互求解反馈设计有直接参考价值

#### F3. Dr. Geo — 几何构造与脚本语言的绑定范式

- **几何构造即代码生成**：用户在 GUI 中画一个点/线/圆，系统同时生成对应的 Smalltalk 脚本代码。用户可以在 GUI 和代码编辑器之间自由切换——这与 Lv-00"每个几何操作同时是合法的程序语句"的目标完全一致。Lv-00 Web GUI 可在构造面板中新增"显示生成的 DSL 代码"视图
- **Smalltalk 的纯粹面向对象模型**：Dr. Geo 选择 Smalltalk（而非 Python/JS）作为脚本语言，是因为 Smalltalk"一切皆对象"的哲学天然适合几何——`aPoint` 知道自己怎么画、怎么计算与其他对象的关系。Lv-00 的几何对象设计可借鉴这种"自包含"的封装思想

---

#### G1. mathlib4 EuclideanGeometry — Lean 4 类型系统下的几何形式化

- **IncidenceGeometry 类型类的设计模式**：`class IncidenceGeometry (Ω : Type u) where` 定义了"点"和"线"的关联公理——一个类型满足某些性质就成为"关联几何"。这与 Lv-00 公理包的概念完全对应——加载 `affine_geometry.lvz` 就获得了 IncidenceGeometry 的能力
- **Birkhoff 公理 vs Tarski 公理的双路径验证**：mathlib4 同时实现了两种公理体系并证明了二者的等价性。Lv-00 的公理包体系可以此为参照，在 `axiom_packages/` 下建立公理间等价性证明的元层
- **Betweenness 和 Congruence 的优雅表示**：用 `S.angle ∠ A B C` 和 `S.segment A B` 等类型安全的构造来表示几何关系——Lv-00 的类型系统（`type_system.h`）可借鉴这种类型安全的设计，为几何关系创建专门的类型代理
- **SyntheticGeometry 的免坐标风格**：mathlib4 的几何形式化刻意避免坐标，完全用综合几何的公理和定理进行推导——这种"免坐标"风格与 Lv-00"几何体本身是实体"的理念一致

#### G2. ProofWidgets4 — 证明交互式可视化组件

- **自定义 React 组件嵌入证明环境**：ProofWidgets4 允许开发者在 Lean 证明中嵌入交互式 React 组件（如 `selectionPanel`、`goalDisplay`、`applyButton`）。Lv-00 Web GUI 可参考这种模式，将证明面板设计为可组合的 Web Component
- **证明目标的 HTML 渲染**：将证明目标（Goal）和前提（Hypotheses）渲染为结构化的 HTML 树，支持展开/折叠子目标。Lv-00 `ProofPanel.tsx` 可新增这种目标树可视化
- **步骤高亮与导航**：当前证明步骤高亮显示、已完成的子目标标绿。这与 Lv-00 `ProofNavigator` 的 `current_step` 导航逻辑天然对应
- **前提选择器**：从当前上下文中智能推荐可用的前提和引理——Lv-00 的 `proof_multi_strategy` 框架可集成一个前提推荐组件

#### G3. Arend — 同伦类型论（HoTT）的几何直觉语法

- **路径类型（Path Type）的几何化语法**：`p : a = b` 在 Arend 中表示从 a 到 b 的路径（等价于相等证明），`p @ q` 表示路径拼接。这种"等式证明 = 路径"的直接几何直觉，与 Lv-00"构造即证明"理念产生深层共鸣——Lv-00 中的几何构造本身可以就是一条"路径证明"
- **类 Java 语法的低门槛设计**：Arend 刻意采用接近 Java 的语法（而非 ML 风格），降低了形式化证明的入门门槛。Lv-00 DSL 语法设计也应遵循类似原则——避免过于抽象的函数式风格，优先使用直觉化的命令式语法
- **Interval 类型和 Path 消去**：`I`（区间类型）和 `coe`（沿路径的强制转换）提供了处理同伦类型论核心概念的简洁接口。Lv-00 的类型系统中的宇宙层级（type_system.h）可以从 HoTT 的视角重新审视

#### G4. mm0 / Metamath — 极简验证内核的终极实践

- **验证器不到 2000 行 C 代码**：mm0 的验证内核只做一件事——检查每个证明步骤的替换是否合法。不内建任何逻辑、不内建任何推理规则。Lv-00 可参考这个设计，创建 `proof_minimal_verifier()` 函数：只做约束图节点的替换一致性检查，其余全部委托给上层引擎
- **$f / $e / $a / $p 四类语句的极简语法**：Metamath 数据库仅用四种语句类型就表达了从命题逻辑到 ZFC 的全部数学基础。Lv-00 的 `.lvz` 文件格式可以借鉴这种极简分层——`$f` = 变量声明、`$e` = 前提、`$a` = 公理、`$p` = 定理
- **独立验证器（Independent Verifier）理念**：mm0 刻意保持验证器的独立性——验证器不需要理解"数学"，只需要理解"替换"。Lv-00 如果需要一个高可信度内核，可以设计一个类似的"独立语法检查器"来验证 `.lvz` 文件
- **MM0 元语言**：在 Metamath 之上的一层抽象，允许定义更复杂的语法和推理规则。Lv-00 的"公理中立"设计可以映射为 MM0 元语言层→具体公理层 的两层架构

---

#### H1. Z3 — 工业级 SMT 求解器作为后端引擎

- **非线性实数算术（QF_NRA）理论**：几何约束（点距、线段长度关系）可以直接编码为 QF_NRA 公式——`(= (^ x 2) (+ (^ r 2) (^ y 2)))` 表示点在圆上。Lv-00 的 `solver.h` 可新增 `solver_encode_to_smtlib2(ConstraintGraph *graph, char **out_smt)` 函数
- **CDCL(T) 引擎的性能优势**：Z3 的 CDCL(T) 引擎在非线性算术上的性能远超朴素 Gröbner 基。当前 `groebner_basis_compute()` 仅处理度数≤2，Z3 可突破这个限制
- **C API 直接集成路径**：Z3 提供完整的 C API（`Z3_mk_context()` → `Z3_mk_solver()` → `Z3_solver_assert()` → `Z3_solver_check()`）。Lv-00 可在 `solver.h` 中新增 `SMTSolverBackend` 抽象，初始实现用 Z3 C API
- **Unsat Core 用于证明萃取**：Z3 的 `Z3_solver_get_unsat_core()` 返回导致不可满足的最小约束子集——在几何证明中，这直接对应"需要哪些公理/条件才能推出结论"
- **Model 用于构造生成**：当公式可满足时，Z3 的 `Z3_solver_get_model()` 返回具体的变量赋值——在几何中直接对应可构造的坐标值

#### H2. cvc5 — 灵活理论组合的 SMT 引擎

- **非线性算术 + 未解释函数 + 量词的理论混编**：cvc5 独有的理论组合能力——同时处理 `sin(x) + f(y) = 0`（其中 f 是未解释函数符号）——这对应 Lv-00 中"符号计算+逻辑推理"的混合场景
- **C++ 原生 API**：cvc5 提供 C++ API（`solver.assertFormula()` / `solver.checkSat()`），Lv-00 的 C 内核可以通过封装 C++ 调用或使用 cvc5 的 C API 绑定来集成
- **Proof Production Mode**：cvc5 可输出证明（proof）作为求解结果的附带产物——这直接对应 Lv-00 的证明系统，可将 SMT 证明转换为 Lv-00 的 `ProofNavigator` 步骤

---

#### I1. Singular — 专精多项式系统的 CAS

- **Gröbner 基计算的工业级实现**：Singular 的 Gröbner 基算法（F4/F5 算法）在性能上远超 Lv-00 当前的简化 Buchberger 算法。Lv-00 可选择将 Singular 作为 Gröbner 基计算的外部引擎（通过 `system()` 调用 Singular 脚本或使用 libSingular 库）
- **理想和簇（Variety）的代数几何视角**：Singular 的 `ideal I = x^2+y^2-1, x-y;` → `groebner(I);` → `vdim(I);` 工作流，与 Lv-00"声明约束→求解→分析解空间"的流程完全一致
- **脚本语言作为 DSL 参考**：Singular 的脚本语言风格（`ring r = 0,(x,y),dp;`）与 Lv-00 的公理包加载（`load axiom euclidean_plane`）概念类似

#### I2. Macaulay2 — 环声明的编程范式

- **"先声明环，再在环上做计算"**：`R = QQ[x,y,z]` → `I = ideal(x^2+y^2-z^2)` → `dim I` ——这种范式与 Lv-00"先声明公理系统，再做几何计算"高度类似。Lv-00 DSL 可以设计为：`use euclidean; let I = constraint(x^2+y^2=z^2); solve I`
- **多环共存与对象归属**：Macaulay2 允许同时声明多个环，每个对象知道自己属于哪个环。Lv-00 的 namespace_depth 和公理包系统天然支持这种多环境共存

#### I3. polymake — 多后端引擎的统一客户端

- **"Perl 粘合多种 C++ 库"的架构模式**：polymake 用 Perl 脚本作为统一前端，后台根据问题类型动态路由到 cdd（计算多面体顶点）、lrs（顶点枚举）、libnormaliz（正规化计算）等不同引擎。Lv-00 的 `EngineScheduler` 可参考此设计——在 `engine.h` 中新增 `EngineScheduler` 结构体，根据约束图特征（自由度数、约束类型分布、变量规模）自动选择最优求解后端
- **规则驱动的后端选择**：polymake 的后端选择规则（如"多面体面数>1000 时用 cdd"）以配置文件形式管理。Lv-00 可在 `config` 模块中新增 `solver_routing_rules.json`，定义 Groebner/SMT/Wu 的选择规则
- **统一数据模型**：polymake 内部用统一的多面体数据模型（`Polytope` 对象），后端之间的切换对用户透明。Lv-00 的 `ConstraintGraph` 就是这个统一数据模型

---

#### J1. MathLive — 所见即所得的数学公式输入

- **支持 LaTeX 输入、实时渲染、虚拟键盘**：MathLive 是一个成熟的 Web Component（`<mathlive-mathfield>`），直接在 HTML 中嵌入即可获得完整的数学编辑能力。Lv-00 Web GUI 的 `FormulaPanel` 可以直接集成 MathLive，替换现有的手写公式解析器
- **命令补全与宏定义**：MathLive 支持自定义 LaTeX 宏（如 `\point{A}` → 渲染为点标记），Lv-00 可利用此功能为几何对象创建视觉化的输入提示
- **双向绑定**：编辑器和渲染视图实时同步——用户输入 LaTeX，画布上同步显示几何体。这是 Lv-00 的 `formula_to_graph.js` 和 `graph_to_formula.js` 的 UX 升级方向

#### J2. CortexJS / MathJSON — 数学的结构化中间表示

- **语义化的 JSON 数学表达式**：`["Add", "x", 2]` 而非 `"x+2"` 字符串——MathJSON 使得数学表达式的机器处理（比较、转换、化简）变得直接而无需字符串解析。Lv-00 的 Web 端前后端通信协议可以基于 MathJSON 格式
- **Compute Engine 的计算能力**：CortexJS 内置的 Compute Engine 支持数值计算、符号微分、化简等——可以作为 Lv-00 Web 端 JS 回退模式的数学计算引擎
- **可扩展的字典（Dictionary）机制**：MathJSON 允许注册自定义函数/符号字典——Lv-00 可将几何专用函数（如 `distance`、`midpoint`、`collinear`）注册为自定义字典项

#### J3. jsTikZ / TikZJax — 前端 WASM 几何渲染管道

- **TikZ 源码 → WebAssembly LaTeX → SVG**：jsTikZ 将完整的 LaTeX 编译器编译为 WASM，在浏览器端执行 TikZ 到 SVG 的转换。Lv-00 的几何可视化可以输出为 TikZ 格式，通过此管道实现高质量前端渲染
- **增量编译优化**：TikZJax 通过缓存 LaTeX 格式文件（`.fmt`）大幅加速首次编译——Lv-00 的 WASM 模块加载可以借鉴这种预编译缓存策略

---

#### K. mai — 数学基础的教育级极简实现

- **推理规则即代码的哲学**：mai 用不到 200 行的 Horc 推理规则定义了完整的一阶逻辑+ZFC 集合论。不存在"形式化系统"和"代码实现"之间的鸿沟——推理规则表直接就是可执行的代码。这与 Lv-00"构造即证明"属于同一种极简设计思维：**把元概念本身当作代码来写**
- **Docker 一键体验**：`docker run --rm amka66/mai set-theory.pl` 无安装即可体验完整的数学基础。Lv-00 可参考这种轻量分发方式，为公理包提供 Docker 化的"一键体验"
- **Prolog 作为元语言的元语言**：mai 的推理规则用 Prolog 编写，利用 Prolog 的回溯搜索能力实现自动证明。Lv-00 的约束求解器（`solver.h`）可以参考 Prolog 式回溯的求解策略——在约束图求解失败时，不是直接报错，而是回溯到上一个决策点尝试替代分支

---

## 二、Lv-00 的核心定位

### 一句话定位

> **Lv-00 是唯一将几何构造、计算程序、一阶逻辑证明三者统一于同一语法体系的元语言。**

### 不是什么

Lv-00 必须在生态中找到自己的独特位置。明确说出"我们不是什么"与"我们是什么"同等重要：

| 不是 | 说明 |
|:---|:---|
| **不是几何工具库** | 不是 CGAL、OpenGeometry 那种供人调用的算法包 |
| **不是自动证明器** | 不是 AlphaGeometry、Newclid 那种解题 AI |
| **不是形式化验证库** | 不是 LeanGeo、GeoCoq 那种依附于外部证明器的数学库 |
| **而是：一种语言本身** | 让你同时完成"画图、算数、证明"三件事的语言 |

### 生态位图示

```
┌──────────────────────────────────────────────────┐
│              上层的具体应用                         │
│   CGAL / CAD / AI求解器 / 教育工具 / 可视化        │
│            ↑ 它们需要精确语义                      │
├──────────────────────────────────────────────────┤
│                                                   │
│           Lv-00：几何元语言（提供精确语义）           │
│     几何构造 = 计算程序 = 一阶逻辑证明               │
│     三者统一于同一语法体系                          │
│                                                   │
│            ↑ 它们提供形式化基础                      │
├──────────────────────────────────────────────────┤
│              底层的逻辑框架                         │
│      Lean / Coq / 一阶逻辑 / 约束求解               │
└──────────────────────────────────────────────────┘
```

### 展开说明

Lv-00 不是现有工具的"又一个替代品"，而是一种**新的品类**——几何元语言（Geometric Meta-Language）。它的独特性体现在五个方面：

#### 1. 几何即语法

在 Lv-00 中，几何体本身就是程序的实体。点、线、区域不仅是数学对象，更是语法构造。用户画一个三角形，同时就在写一个程序、构造一个证明。不需要在"几何工具"和"编程语言"之间切换——它们本就是同一个东西。

#### 2. 数形不二

数值在 Lv-00 中只能是几何量。没有游离于几何之外的"裸数值"。坐标是点的一部分，长度是线段的性质，面积是区域的性质。这种约束不是限制，而是保证——它确保一切计算都有几何意义。

#### 3. 构造即证明

一个几何构造是否构成证明，取决于合一检查（unification check）的结果。如果构造与命题模式合一，那么构造本身就是证明。这不同于"先构造再证明"的传统范式，而是将证明内建于构造过程之中。

#### 4. 公理中立

内核不内建距离、角度概念。欧氏几何、双曲几何、椭圆几何、集合论、类型论都是可加载的"公理系统包"。这意味着 Lv-00 不是绑定在某个特定数学体系之上，而是一个可以让多种数学体系和平共存的元框架。

#### 5. 可演进

公理系统可以升级。当一个定理被证明后，它可以固化为新的重写规则或新约束类型，从而扩展语言本身的表达能力。这是 Lv-00 区别于所有静态形式化系统的关键特性。

---

## 三、各竞品具体借鉴要点

### 第三梯队（已有项目）

#### LeanGeo — 证明呈现的清晰度

- **公理命名规范**：遵循 `namespace.axiom_name` 的统一命名
- **证明块注释**：每个关键步骤有清晰的注释说明用了什么规则
- **可视化输出**：证明步骤附带几何图示
- **分层呈现**：先展示总体策略（如"通过作辅助线构造相似三角形"），再展开细节

#### GeoCoq — 公理分层与模块化

- **公理分层**：从最基础的关联公理到高级的连续公理，层层递进
- **独立性追踪**：每个定理标注了依赖哪些公理，便于理解公理间的逻辑关系
- **可替换性**：模块化设计使得替换某个公理组（如平行公理→双曲公理）变得容易

#### AlphaGeometry — 自然语言证明输出

- **人类可读的证明步骤**：每一行都是一句完整的自然语言描述
- **辅助构造的合理解释**：不仅说"作辅助线 XX"，还解释为什么作这条辅助线
- **逐步展开**：从已知条件出发，每一步只应用一条推理规则

#### Newclid — 证明回溯与可视化

- **搜索树可视化**：展示证明搜索过程中尝试了哪些路径
- **回溯点标注**：标注在哪些节点进行了回溯，方便理解证明策略
- **策略可切换**：用户可以在不同搜索策略之间切换观察效果

#### CGAL — API 文档组织

- **按概念分类**：不是按类/函数排列，而是按几何概念（如 Kernel、Arrangement、Triangulation）分类
- **每个概念三件套**：概念定义 → 模型实现 → 使用示例
- **复杂度标注**：每个操作的时空复杂度明确标注

### 第四梯队（新增项目）

#### JGEX / GEX — 多证明方法并存与可读证明生成

- **六种独立证明方法共存**：吴方法、面积法、Gröbner 基法、向量法、全角法、演绎数据库法，用户可在同一系统内切换——对应 Lv-00 公理中立的多策略架构
- **可读证明生成算法**：面积法和全角法生成传统几何风格的"人类可读"证明步骤（如"由三角形面积公式可知…"），而非纯逻辑形式推导——对标 Lv-00 `proof.c` 输出格式化
- **C-tree 约束分解算法**：将复杂几何约束图递归分解为可顺序求解的子图，每步只处理局部约束——对标 Lv-00 `constraint_graph` 的求解策略
- **动态几何交互**：支持拖拽几何对象，约束关系实时保持，证明步骤同步更新

#### GAlgebra — 符号几何代数操作符映射

- **数学操作符→代码操作符的直接映射**：`A*B`=几何积、`A^B`=外积、`A|B`=内积、`A<B`=左缩并、`~A`=反向——Lv-00 Python binding 可直接复用此映射表
- **度规参数化声明语法**：`Ga('e', g=[1,-1,-1,-1], coords=(t,x,y,z))` 单行代码定义完整的时空代数——对标 Lv-00 公理包加载的简洁声明方式
- **符号化简管道**：`simplify` → `trigsimp` → `expand` 多阶段自动化简，同时输出 LaTeX 和控制台格式——对标 Lv-00 符号计算层的输出管道设计
- **Jupyter Notebook 原生支持**：多向量直接渲染为 LaTeX 格式的教学推理过程

#### clifford — 数值几何代数存储与性能优化

- **预定义代数命名空间**：`from clifford.g3 import *` 即获得完整 3D 欧氏 GA（e1,e2,e3,e12,e13,e23,I）——对标 Lv-00 "import 即加载公理系统+预定义基"模式
- **flat array 多向量存储**：使用扁平数组存储多向量所有分量，避免 Python 对象开销——对标 Lv-00 C 内核数值路径的存储方案
- **Numba JIT 加速**：数值 GA 运算的即时编译加速——Lv-00 数值执行路径的性能优化参考
- **多向量系数索引**：`X[i]` 返回第 i 个分量、`X(ei)` 返回以 ei 为伪标量的子代数部分——对标 Lv-00 几何体内部表示的索引方案

#### Ganja.js — inline DSL 的 AST 转译技术

- **内联代数语法的 AST 转译**：`Algebra(0,1).inline(()=>(3+2e1)*(1+4e1))` 中 `e1` 直接在 JS 语法中作为代数常量使用——Ganja 通过重写函数 AST 将操作符表达式翻译为过程化 API 调用。**这是 Lv-00 DSL 编译器最直接的技术参考**
- **度规签名式参数化**：`Algebra(2,0,1)` = 2正+0负+1零维 = PGA2D——与 Lv-00 公理包签名理念完全一致
- **跨语言代码生成管道**：单一定义 → 生成 JS/C++/C#/Rust/Python 五种语言的高性能 flat multivector 代码——Lv-00 如需要多语言后端导出可直接参考
- **内置 2D/3D 可视化引擎**：代数表达式自动渲染为 SVG/WebGL 图形

#### Grassmann.jl — 编译期类型级代数系统

- **编译期完全预计算**：`TensorBundle{n,P,g,ν,μ}` 作为字节编码的类型参数，所有代数结构在编译期确定——零运行时开销。Lv-00 若设计 Rust 内核可借鉴
- **严谨的类型层级**：`TensorAlgebra{V}` → `TensorGraded{V,G}` → `Chain{V,G}` / `Multivector{V}` / `Spinor{V}` / `Simplex{V}`——对标 Lv-00 几何体类型系统（点/线/面/体）的参数化多态设计
- **子代数互操作性**：通过 `AbstractTensors.jl` 统一抽象，不同签名、不同维度的代数可通用互操作——Lv-00 公理中立设计天然需要欧氏几何和双曲几何对象在统一框架下共存
- **宏声明简化**：`@basis S"-++"` 一个宏声明一个带指定签名的完整代数空间——可直接映射到 Lv-00 的公理包加载机制

#### SymPy Geometry — Python 符号几何 API 基准

- **GeometryEntity 继承层次**：`GeometryEntity` → `GeometrySet` → `LinearEntity`(Line/Ray/Segment) / `Ellipse`(Circle/Ellipse) / `Polygon`(Triangle)——对标 Lv-00 几何类型系统层次组织
- **直观的 API 命名约定**：`Point.distance(other)`、`Line.intersection(Circle)`、`Triangle.area`——Lv-00 Python binding 的用户体验基准
- **符号参数化几何**：`Point(x, y)` 支持符号坐标，交点计算返回符号表达式（而非近似数值）——对标 Lv-00 符号路径执行
- **entity 方法完备性**：每个几何实体提供 distance、intersection、contains、reflection、rotation 等全套方法——Lv-00 几何对象 API 覆盖度参考

#### OpenCASCADE (OCCT) — 工业级模块化架构

- **7 大模块分层架构**：Foundation → Modeling Data → Modeling Algorithms → Visualization → Data Exchange → Application Framework → Test Harness——**这是 Lv-00 软件架构设计最重要的分层参考**
- **B-Rep 拓扑与几何的严格分离**：TopoDS_Shape 拓扑层次（Vertex/Edge/Wire/Face/Shell/Solid/CompSolid/Compound）与 Geom 几何表示独立——同一拓扑上可挂载不同几何实现，与 Lv-00"公理中立"的拓扑/度量分离哲学完全一致
- **标准化命名与引用体系**：`TCollection_AsciiString` / `TCollection_HExtendedString` 等规范化字符串处理——对标 Lv-00 `NAMING_CONVENTION.md` 的规范化
- **DRAW Test Harness**：内置 Tcl 脚本测试环境，可交互式执行几何操作并即时可视化

#### CadQuery — Fluent API 的自然语言化几何构造

- **Fluent API 链式调用**：`box.faces(">Z").workplane().circle(2).extrude(1)` 让 3D 建模像自然语言描述一样直观——**这是 Lv-00 DSL 语法设计的直接 UX 参考**
- **Selector DSL**：`faces(">Z")`、`edges("|Z")`、`vertices("<X")` 等选择器语法，用简洁字符串表达几何体上的拓扑选择——对标 Lv-00 几何对象引用语法
- **Workplane 作为一等抽象**：2D 工作平面概念作为一等对象，所有 2D 构造操作在此平面上进行——对标 Lv-00 构造平面概念
- **Jupyter 原生支持**：几何模型直接在 Notebook 中内嵌 3D 可视化

#### build123d — 代数模式的无状态设计

- **代数模式 (Algebra Mode) 的纯粹无状态设计**：所有操作返回新对象，`part += sub_obj` 明确的代数语义，无隐式状态——**与 Lv-00"构造即运算"理念高度一致**
- **操作符驱动的变换语法**：`Plane.XZ * Pos(X=5) * Rectangle(1, 1)` 用乘法链表达变换序列——对标 Lv-00 变换语法设计
- **ShapeList 函数式选择器**：`.edges().filter_by(Axis.Z).sort_by(GeomType.CYLINDER)` 提供函数式的拓扑选择能力——对标 Lv-00 几何体子对象引用 API
- **双模式架构共存**：代数模式（无状态，适合函数式组合）+ 构建器模式（有状态，符合传统 CAD 工作流直觉）——Lv-00 可同时提供声明式和过程式两种使用风格

#### OpenGeometry Group — 国产开源几何内核生态

- **联盟共建的生态运营模式**：多家企业+学术机构共同贡献，而非单组织主导——对标 Lv-00 公理包开放生态的建设路径
- **云原生几何计算**：从设计之初考虑几何计算作为服务的分布式部署——对标 Lv-00 Web GUI / server 模式的部署策略
- **代数引擎与数值引擎的明确分离**：符号几何引擎和数值计算引擎作为独立子系统——对标 Lv-00 符号/数值双路径架构的工程实践验证
- **社区驱动的标准化**：通过社区共识推进几何内核接口标准的统一

---

## 四、后续行动计划

### 已完成落地（2026-05-23）

| 优先级 | 借鉴对象 | 借鉴内容 | 落地模块 | 状态 |
|:---|:---|:---|:---|:---:|
| P0 | LeanGeo + GeoCoq | 公理分层与证明呈现 | `axiom_packages/` 组织方式、`proof.c` 输出格式 | ✅ |
| P1 | AlphaGeometry | 自然语言证明输出 | `proof.c` + Web GUI 证明展示 | ✅ |
| P2 | CGAL | API 文档组织 | `docs/API_USAGE_GUIDE.md` 重构 | ✅ |
| P3 | GeoGebra | 几何对象命名体系 | `docs/NAMING_CONVENTION.md` | ✅ |
| P4 | GAP | 包管理 | `axiom_packages/` 包管理机制 | ✅ |
| — | Newclid | 证明回溯可视化 | `proof.h/c` 搜索树 + `ProofPanel.tsx` | ✅ |
| — | Solvespace | 交互式求解反馈 | `solver.h/c` + 流式输出 | ✅ |
| — | FRONTIER | 约束图可视化 | `ConstraintGraphPanel.tsx` | ✅ |
| — | Kingdon | 符号计算UX | `FormulaPanel.tsx` 实时预览 | ✅ |
| — | PyEuclid | 高层Python API | `python/lv/dsl.py` | ✅ |
| — | Penrose | 几何叙事生成 | `NarrativeExport.tsx` | ✅ |

**总计：11 个竞品全部落地，新增约 11,000 行代码，18 个 C API，5 个 Web GUI 组件。**

### 已全部落地的新增项目（2026-05-24，全部 ✅）

| 优先级 | 借鉴对象 | 借鉴内容 | 建议落地模块 | 状态 |
|:---|:---|:---|:---|:---:|
| P1 | Ganja.js | inline AST 转译 DSL | `include/lv/dsl_compiler.h`（438行：37种Token/25种AST/30种IR操作码/13 API） | ✅ 落地 |
| P1 | build123d | 代数模式无状态设计 | `include/lv/algebra_mode.h`（575行：AlgebraicGeom/12种选择器/25+ API/链式调用） | ✅ 落地 |
| P1 | CadQuery | Fluent API 链式调用 | 融入 `include/lv/algebra_mode.h`（lvSelector/SelectorType/链式API） | ✅ 落地 |
| P1 | **GCLC** | 几何构造语言语法 + WASM 移植 | `include/lv/gc_language.h`（463行：42种命令/5种证明方法/WASM导出/12+ API） | ✅ 落地 |
| P1 | **mm0 / Metamath** | 极简内核验证设计 | `include/lv/mini_kernel.h`（513行：$f/$e/$a/$p四类语句/替换检查/极小TCB/15+ API） | ✅ 落地 |
| P2 | JGEX | 多证明方法并存引擎 | `include/lv/proof.h`（ProofMultiStrategy 已有8种证明方法枚举） | ✅ 已有 |
| P2 | OCCT | 7 模块分层架构 | `docs/architecture_v3.2.md` 已基于此重构 | ✅ 已有 |
| P2 | GAlgebra | 操作符重载 API | Python binding 设计（`python/lv/dsl.py` 已含操作符映射） | ✅ 已有 |
| P2 | **Z3 / cvc5** | SMT 求解器作为后端引擎 | `include/lv/smt_backend.h`（已含 Z3/cvc5 后端抽象） | ✅ 已有 |
| P2 | **polymake** | 多后端引擎+统一客户端架构 | `include/lv/engine_scheduler.h`（已含多后端调度框架） | ✅ 已有 |
| P3 | Grassmann.jl | 编译期类型级代数 | `include/lv/type_system.h`（已含宇宙层级+类型推断） | ✅ 已有 |
| P3 | SymPy Geometry | GeometryEntity 继承层次 | `include/lv/geometry_types.h`（已含完整 GeometryEntity 层次） | ✅ 已有 |
| P3 | clifford | flat array 存储 | `include/lv/geometry_types.h`（已借鉴 flat array 策略） | ✅ 已有 |
| P3 | **mathlib4 EuclideanGeometry** | Lean 4 几何形式化最佳实践 | `include/lv/euclidean_geometry.h`（441行：5公理组/5几何谓词/等价性验证/14 API） | ✅ 落地 |
| P3 | **MathLive** | Web 数学公式输入 UX | `include/lv/math_input.h`（391行：3输入模式/5键盘布局/20+几何宏/自动补全/18 API） | ✅ 落地 |
| P3 | **CortexJS / MathJSON** | 结构性数学中间表示 | `include/lv/math_protocol.h`（402行：32表达式类型/MathJSON序列化/可扩展字典/14 API） | ✅ 落地 |
| P4 | OpenGeometry Group | 联盟共建生态 | `include/lv/ecosystem.h`（527行：包注册表/兼容性矩阵/Docker一键体验/生态统计/16 API） | ✅ 落地 |
| P4 | **Arend** | 路径类型语法 / HoTT 直觉 | `include/lv/path_type.h`（341行：区间I/6路径类型/coe消去/路径拼接/15 API） | ✅ 落地 |
| P4 | **Singular / Macaulay2** | Gröbner 基计算 / 环声明范式 | `include/lv/groebner_engine.h`（461行：多项式环/F4-F5算法/理想/代数簇/24 API） | ✅ 落地 |
| P4 | **Cinderella / Dr. Geo** | 交互几何 UX / 脚本绑定 | `include/lv/interactive_geo.h`（481行：随机化验证/连续性保持/脚本绑定/约束维护/16 API） | ✅ 落地 |
| P4 | **ProofWidgets4** | 证明可视化组件架构 | `include/lv/proof_widget.h`（342行：8组件类型/目标显示/前提面板/策略推荐/16 API） | ✅ 落地 |
| P4 | **mai** | 极简"逻辑即代码"设计哲学 | 理念已融入 `mini_kernel.h` 极简TCB + `ecosystem.h` Docker一键体验 | ✅ 落地 |
| P4 | **jsTikZ / TikZJax** | 前端 WASM 几何渲染管道 | `include/lv/tikz_export.h`（536行：28元素类型/信任颜色映射/WASM渲染/增量编译/18 API） | ✅ 落地 |

---

## 五、三条贯穿设计线索

总结全部 **91 个**参考项目，可提炼为 Lv-00 的三条核心设计线索：

| 线索 | 代表项目 | 核心启示 | Lv-00 对应模块 |
|:---|:---|:---|:---|
| **代数声明即计算** | Ganja.js、GAlgebra、Grassmann.jl、**Macaulay2** | 通过声明度规签名或公理参数，一次性获得完整的代数计算能力——"一句话加载一个几何世界" | 公理包加载 (`axiom_packages/`) |
| **几何即语法** | CadQuery、build123d、Ganja.js、**GCLC** | 几何构造直接成为程序语法——每个几何操作同时是合法的程序语句和合法的几何推理步骤 | DSL 设计、Python binding |
| **多模式共存** | JGEX、build123d、OCCT、**polymake**、**Z3/cvc5** | 在统一框架下让多种计算模式（代数/过程式、符号/数值、多公理系统、多求解后端）和平共存 | 求解器、类型系统、证明引擎 |

### 新增项目带来的四条新线索

| 新线索 | 代表项目 | 核心启示 |
|:---|:---|:---|
| **极简即强壮** | mm0、mai、Metamath、**HOL Light** | 真正的基础设施应该是极简的——mm0 验证器不到 2000 行 C 代码、HOL Light 内核不足 500 行、mai 用纯推理规则定义整个数学基础。Lv-00 内核应向这个方向看齐 |
| **WASM 即通道** | GCLC、jsTikZ、Cinderella、**OpenSCAD** | C/C++ 数值/符号核心 → Emscripten → WASM → 浏览器，这条技术路线已经被多个项目验证。GCLC 和 OpenSCAD 的移植经验双重验证了 Lv-00 Web 端的可行性 |
| **组件即交互** | MathLive、ProofWidgets4、CortexJS | 不要自己从头写数学 UI——已有高质量的开源组件（公式输入、结构渲染、证明面板），集成它们比自研更快更好 |
| **重写即语义** | **Maude**、**K Framework**、**Rascal**、**Rosette** | 用重写规则直接定义系统的操作语义——每条规则既是规约也是可执行代码。Lv-00 的 `rewrite.h` 和公理包的语义定义可以直接映射为重写规则，证明搜索即重写路径搜索 |

### 第七梯队带来的三条新线索

| 新线索 | 代表项目 | 核心启示 |
|:---|:---|:---|
| **非破坏性重写（E-Graphs）** | **egg**、egglog | 重写不应是选择性的——所有可能的重写结果同时保存在 e-graph 等价类中，彻底解决"重写顺序选择"难题。证明即等价类中的路径搜索 |
| **GAT 编译管线** | **Catlab.jl**、GATlab.jl | 范畴论公理声明 → 类型系统 → 可执行代码自动编译。Lv-00 的 `.lvz` 公理包加载机制可以从此大规模受益 |
| **ATP/SMT 混合后端** | **Vampire**、**E Prover**、iProver | SMT（Z3/cvc5）和纯 FOL ATP（Vampire/E）各有所长，同时接入+自动调度是最优策略，几何问题的复杂度可通过后端多样性来应对 |

---

*最后更新：2026-05-24（第七梯队落地完成，累计 75 个项目）*

---

## 六、第六梯队落地总结（2026-05-24 第三次落地）

20 个新增项目已全部落地，详见 [`docs/reports/Lv-00_全部57个参考项目落地汇报_2026-05-24.md`](reports/Lv-00_全部57个参考项目落地汇报_2026-05-24.md)。

| 类别 | 项目数 | 落地产出 | 状态 |
|:---|:---:|:---|:---:|
| L. 证明助手补完 | 5 | 5 篇设计文档 + `proof.h` 新增 5 个函数声明 | ✅ |
| M. 重写逻辑系统 | 5 | 5 篇设计文档 + `rewrite.h` 新增策略枚举/结构体/函数 | ✅ |
| N. 补充 CAS | 5 | 5 篇设计文档 | ✅ |
| O. 程序化几何建模 | 3 | 3 篇设计文档 + `geometry_types.h` 新增 CSG 操作符 | ✅ |
| P. 约束求解与建模 | 3 | 3 篇设计文档 | ✅ |
| Q. 可视化与几何处理 | 2 | 2 篇设计文档 | ✅ |
| **总计** | **20** | **23 篇设计文档 + 3 个头文件修改** | ✅ |
| **57 个项目全部落地** | | | ✅✅✅ |

---

## 七、第七梯队落地总结（2026-05-24 第四次落地）

18 个新增项目已全部落地，详见 [`docs/competitive_analysis_tier7.md`](competitive_analysis_tier7.md) 和 [`docs/reference/`](reference/)。

### 落地详情

| 优先级 | 借鉴对象 | 借鉴内容 | 落地模块 | 状态 |
|:---|:---|:---|:---|:---:|
| P0 | **egg** | e-graph 非破坏性重写范式 | `docs/reference/egg_egraph_rewriting.md`（627行）+ `rewrite.h` 设计方向 | ✅ |
| P0 | **Graphviz DOT** | 声明式图描述语言 | `include/lv/constraint_graph.h` 新增 `DOTLayoutEngine`/`DOTExportConfig`/`graph_export_dot()`/`graph_export_dot_file()`/`graph_export_dot_to_svg()`（~75行API）+ `docs/reference/dot_graphviz_visualization.md`（503行） | ✅ |
| P1 | **Catlab.jl / GATlab.jl** | GAT 编译管线 + wiring diagram | `docs/reference/catlab_gat_compilation.md`（613行） | ✅ |
| P1 | **Vampire / E Prover / iProver** | FOL ATP 后端集成 | `include/lv/atp_backend.h`（388行新头文件：ATPBackendType/ATPConfig/ATPResultInfo/ATPBackendSolver/注册表/引擎调度集成）+ `docs/reference/vampire_eprover_atp_backend.md`（706行） | ✅ |
| P2 | **LeanDojo / Pantograph** | 证明树数据模型 + 证明交互协议 | `docs/reference/leandojo_proof_tree.md`（373行） | ✅ |
| P2 | **Dafny** | ensures 子句 + auto-active verification + calc 证明 | `docs/reference/dafny_ensures_verification.md`（327行） | ✅ |
| P2 | **Mermaid.js** | 文本→实时图表渲染管线 | `docs/reference/mermaidjs_diagram_rendering.md`（388行） | ✅ |
| P2 | **Desmos API / CL** | 声明式数学交互 UX | `docs/reference/desmos_cl_declarative_ux.md`（352行） | ✅ |
| P3 | **SymEngine** | C++ 符号数学引擎 + C ABI 互操作 | `docs/reference/symengine_cpp_symbolic.md`（392行） | ✅ |
| P3 | **Symbolics.jl + ModelingToolkit.jl** | 符号-数值透明切换 + 多目标编译 | `docs/reference/symbolicsjl_hybrid_computing.md`（451行） | ✅ |
| P3 | **Semagrams.jl** | 交互式 wiring diagram 编辑 UX | `docs/reference/semagrams_wiring_diagram_editor.md`（375行） | ✅ |
| P3 | **AbstractAlgebra.jl** | 泛型代数结构声明 | `docs/reference/abstractalgebra_generic_algebra.md`（441行） | ✅ |
| P3 | **egglog** | Datalog+e-graph 混合（在 egg 文档中覆盖） | `docs/reference/egg_egraph_rewriting.md`（已含 egglog 章节） | ✅ |
| P3 | **iProver** | Inst-Gen 量词友好 ATP（在 Vampire 文档中覆盖） | `docs/reference/vampire_eprover_atp_backend.md`（已含 iProver 章节） | ✅ |
| P3 | **Pantograph** | 证明交互协议（在 LeanDojo 文档中覆盖） | `docs/reference/leandojo_proof_tree.md`（已含 Pantograph 章节） | ✅ |

### 第七梯队按类别汇总

| 类别 | 项目数 | 落地产出 | 状态 |
|:---|:---:|:---|:---:|
| R. E-Graphs | 2 | 1 篇设计文档（含 egglog） | ✅ |
| S. 应用范畴论 | 3 | 1 篇设计文档（含 Semagrams） | ✅ |
| T. FOL ATP | 3 | 1 篇设计文档（含 iProver）+ `atp_backend.h`（388行新头文件） | ✅ |
| U. 符号数学核心库 | 3 | 2 篇设计文档 | ✅ |
| V. 图描述语言 | 2 | 1 篇设计文档 + `constraint_graph.h` DOT API 扩展 | ✅ |
| W. ML 辅助证明 | 2 | 1 篇设计文档（含 Pantograph） | ✅ |
| X. 交互式数学 | 1 | 1 篇设计文档 | ✅ |
| Y. 程序验证 | 1 | 1 篇设计文档 | ✅ |
| Z. Julia 建模 | 1 | 1 篇设计文档 | ✅ |
| **总计** | **18** | **12 篇设计文档 + 2 个头文件（新增 `atp_backend.h` 388行，扩展 `constraint_graph.h` +75行）** | ✅ |
| **75 个项目全部落地** | | | ✅✅✅✅ |

---

## 八、第八梯队落地总结（2026-05-24 第五次落地）

8 个新增项目已全部落地，详见 [`docs/reference/`](reference/)。

### 第八梯队新增项目清单

共 8 个项目，按类别分组。

#### AA. Datalog 与逻辑编程引擎

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **Soufflé** | [github.com/souffle-lang/souffle](https://github.com/souffle-lang/souffle) | 高性能 Datalog 引擎，编译为并行 C++。**借鉴它的声明式约束传播范式——Lv-00 `constraint_graph` 中的约束传播天然是 Datalog 规则（已知事实→推导新事实）。Soufflé 支持递归+聚合+ADT+Provenance，约束传播建模为 Datalog 规则后在 Soufflé 引擎上并行执行。Provenance 系统直接对应 Lv-00 证明树的依赖链追踪** |

#### AB. 语言工程框架

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **Langium** | [github.com/eclipse-langium/langium](https://github.com/eclipse-langium/langium) | TypeScript 语言工程框架，Xtext 的精神继承者。**借鉴它的"语法声明→语义模型→代码生成"管线——一份语法文件同时生成解析器、LSP 服务器、类型检查器和代码生成器。它的依赖注入架构与 Lv-00"公理中立"的模块替换思想一致。Lv-00 DSL 的 `formula_parser.h` 可参考 Langium 的错误恢复和 LSP 深度集成机制** |

#### AC. 精确数值与区间算术

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **FLINT / Arb** | [github.com/flintlib/flint](https://github.com/flintlib/flint) | 最快数论/多项式 C 库，Arb 提供任意精度区间算术。**借鉴它的"区间算术（Ball Arithmetic）"——每个实数计算返回 `[mid ± rad]` 的严格区间，保证包含真实值。作为 Lv-00"符号路径"和"数值路径"之间的第三条路径——"区间路径"提供不完全符号但严格正确的中间方案。纯 C 实现，与 Lv-00 技术栈完美兼容** |

#### AD. 计算拓扑

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **GUDHI** | [github.com/GUDHI/gudhi-devel](https://github.com/GUDHI/gudhi-devel) | 拓扑数据分析与高维几何理解库。**借鉴它的单纯复形数据结构——用 0-单纯形(点)、1-单纯形(边)、2-单纯形(三角形)统一表示几何构造，持久同调计算自动检测 Betti 数（连通分支数、孔洞数、空洞数）。Lv-00 的 `algebraic_topology.lvz` 和 `point_set_topology.lvz` 从"纸上公理"升级为"可计算的拓扑分析"** |

#### AE. 形式化 C 程序验证

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **Frama-C** | [frama-c.com](https://frama-c.com) | 工业级 C 程序形式化验证平台，基于 ACSL 规约语言。**借鉴它的"代码+规约一体化"验证——`/*@ requires x >= 0; ensures \result >= 0; */` 嵌入 C 代码，WP 插件自动通过 SMT 验证。Lv-00 内核作为 C 代码库，可用 Frama-C 验证关键不变量（端口一致性、约束图完整性）。ACSL 的规约语法与 Lv-00"构造即证明"哲学一脉相承** |

#### AF. 声明式技术绘图语言

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **Asymptote** | [github.com/vectorgraphics/asymptote](https://github.com/vectorgraphics/asymptote) | C++ 风格语法的矢量图形编程语言。**借鉴它的 C++ 内核 + 类 C++ 语法——计算在 C++ 内核中执行（非 LaTeX 宏展开），3D 原生支持（投影/消隐/光照），path 作为一等对象。与 Lv-00 C 内核天然对应——几何构造后的证明图形可直接导出为 Asymptote 代码，渲染为出版物级质量的 PDF 证明图** |

#### AG. 数学知识管理

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **MMT** | [github.com/UniFormal/MMT](https://github.com/UniFormal/MMT) | 数学知识的形式化表示与互操作元框架。**借鉴它的"理论态射（Theory Morphism）"——不同数学理论间通过态射映射自动导出定理的跨理论翻译。这是 Lv-00"公理中立"设计的形式化理论基础——欧氏几何→双曲几何的平行公理替换就是一次 theory morphism。MathHub 门户聚集数十种形式化数学库的互操作经验** |

#### AH. 约束整数规划

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **SCIP** | [github.com/scipopt/scip](https://github.com/scipopt/scip) | 最快的非商业约束整数规划求解器。**借鉴它的约束处理器插件架构——每种约束类型实现为独立插件，可热加载。MIP+CP 混合范式——同时支持连续变量和离散变量，自动在分枝定界和约束传播间切换。对应 Lv-00 中混合类型约束（连续坐标+离散选择）的统一求解。Apache 2.0 许可的 C 源码可直接链接到 Lv-00 内核** |

### 第八梯队落地详情

| 优先级 | 借鉴对象 | 借鉴内容 | 落地模块 | 状态 |
|:---|:---|:---|:---|:---:|
| P1 | **Soufflé** | Datalog 声明式约束传播 + Provenance | `docs/reference/souffle_datalog_engine.md`（546行） | ✅ |
| P1 | **FLINT/Arb** | 区间算术 + 严格数值验证 | `docs/reference/flint_arb_rigorous_numerics.md`（~325行） | ✅ |
| P2 | **Langium** | 语法声明即全栈 + LSP 深度集成 | `docs/reference/langium_dsl_framework.md` | ✅ |
| P2 | **Frama-C** | ACSL 代码+规约一体化 + WP 验证 | `docs/reference/framac_deductive_verification.md` | ✅ |
| P2 | **GUDHI** | 单纯复形 + 持久同调 + Betti 数 | `docs/reference/gudhi_computational_topology.md`（458行） | ✅ |
| P3 | **Asymptote** | C++ 内核 3D 原生绘图 + path 一等对象 | `docs/reference/asymptote_technical_drawing.md`（430行） | ✅ |
| P3 | **MMT** | 理论态射 + MathHub 联邦式知识库 | `docs/reference/mmt_math_knowledge_management.md`（579行） | ✅ |
| P3 | **SCIP** | 约束处理器插件架构 + MIP+CP 混合 | `docs/reference/scip_mixed_integer_programming.md`（470行） | ✅ |

### 第八梯队按类别汇总

| 类别 | 项目数 | 落地产出 | 状态 |
|:---|:---:|:---|:---:|
| AA. Datalog 引擎 | 1 | 1 篇设计文档 | ✅ |
| AB. DSL 框架 | 1 | 1 篇设计文档 | ✅ |
| AC. 精确数值 | 1 | 1 篇设计文档 | ✅ |
| AD. 计算拓扑 | 1 | 1 篇设计文档 | ✅ |
| AE. 形式化 C 验证 | 1 | 1 篇设计文档 | ✅ |
| AF. 技术绘图 | 1 | 1 篇设计文档 | ✅ |
| AG. 知识管理 | 1 | 1 篇设计文档 | ✅ |
| AH. 混合约束求解 | 1 | 1 篇设计文档 | ✅ |
| **总计** | **8** | **8 篇设计文档** | ✅ |
| **83 个项目全部落地** | | | ✅✅✅✅✅ |

### 第八梯队带来的新设计线索

| 新线索 | 代表项目 | 核心启示 | Lv-00 对应模块 |
|:---|:---|:---|:---|
| **Datalog 约束传播** | Soufflé | 约束传播天然是 Datalog 规则——声明式描述几何约束关系，引擎自动递归推导 | 约束图求解、证明依赖追踪 |
| **第三条数值路径** | FLINT/Arb | 在符号（精确但慢）和浮点（快但不保真）之间，区间算术提供"快且保真"的第三条路 | 符号坐标系统、求解器 |
| **语言工程规模化** | Langium | DSL 不仅是语法设计，更是 LSP/IDE 体验的工程体系——单一声明驱动全栈工具链 | DSL 编译器、Web GUI |
| **计算拓扑** | GUDHI | 拓扑从"纸上公理"变成"可计算属性"——几何构造的 Betti 数可作为证明的不变量检查 | 代数拓扑公理包 |
| **自举安全** | Frama-C | 可信计算基（TCB）的质量可通过形式化自举验证系统性提升 | 内核不变量、CI 流水线 |
| **公理元层** | MMT | "公理中立"的形式化理论基础——theory morphism 解释公理包的互操作和替换 | 公理包系统 |
| **C++ 原生绘图** | Asymptote | 与 TikZ 互补——C++ 内核在 C++ 层做几何计算和渲染，性能更好 | 几何可视化导出 |
| **混合求解** | SCIP | 连续+离散混合约束的统一求解——约束处理器插件架构可规模化 | 求解器多后端调度 |

---

---
---

## 九、第九梯队落地总结（2026-05-24 第六次落地）

8 个新增项目已全部落地，详见 [`docs/reference/`](reference/)。

### 第九梯队新增项目清单

共 8 个项目，按类别分组。

#### AI. 形式化规约与模型检测

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **TLA+** | [github.com/tlaplus/tlaplus](https://github.com/tlaplus/tlaplus) | Leslie Lamport 的形式化规约语言，基于时序逻辑（Temporal Logic of Actions）。**借鉴它的 Init/Next/Spec 三段式规约范式——Lv-00 几何构造可表达为 Init（初始几何体）→ Next（构造步骤）→ Spec（完整构造+不变式）。TLC 模型检查器的穷举状态搜索与 Lv-00 证明搜索天然对应。TLAPS 证明管理器的层次化证明与 ProofNavigator 架构一致。PlusCal→TLA+ 编译管线为 DSL→内核的变换管道提供直接参考** |
| **Alloy** | [github.com/AlloyTools/org.alloytools.alloy](https://github.com/AlloyTools/org.alloytools.alloy) | MIT 开发的轻量级关系模型查找器，基于一阶关系逻辑。**借鉴它的"关系即一切"统一建模范式——Lv-00 约束图可重新解释为关系模型（点、线、区域都是关系元组），约束即为关系上的逻辑公式。有限范围假设允许小规模定理快速验证。Kodkod 的关系逻辑→SAT 编码为 constraint_graph→SAT 转换提供工程参考。交互式反例可视化对 Lv-00 证明反例展示有直接 UX 价值** |

#### AJ. 高性能数值计算基础设施

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **Eigen** | [gitlab.com/libeigen/eigen](https://gitlab.com/libeigen/eigen) | 纯头文件零依赖 C++ 线性代数库，CGAL/ROS/Drake/TensorFlow 等千余项目的数值底层。**借鉴它的纯头文件分发模式——Lv-00 可提供单头文件 `lv.h` 的轻量分发。表达式模板惰性求值对应 Lv-00 符号计算的延迟求值。Geometry 模块（Transform/Rotation/Quaternion）与 Lv-00 几何变换直接对应。固定大小矩阵栈分配为小几何体（2D/3D 点、线段）的零堆分配优化提供参考。SIMD 向量化（SSE/AVX/NEON）为 Lv-00 数值计算加速提供范例** |

#### AK. 非线性优化

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **IPOPT** | [github.com/coin-or/Ipopt](https://github.com/coin-or/Ipopt) | CMU 开发的大规模非线性优化求解器，原对偶内点法+过滤线搜索。**借鉴它的线性求解器抽象层（MA27/MA57/MUMPS/Pardiso/SPRAL）——Lv-00 的 solver 模块可参考此设计，将 Gröbner基/Gauss消元/迭代法统一为可互换的后端。CppAD 自动微分集成为 Lv-00 约束图梯度计算提供技术路径。稀疏雅可比处理为大规模几何约束系统提供可扩展性参考。AMPL 模型格式为 Lv-00 几何优化问题描述提供 DSL 语法参考** |

#### AL. 网格生成与几何离散化

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **Gmsh** | [gitlab.onelab.info/gmsh/gmsh](https://gitlab.onelab.info/gmsh/gmsh) | 三维有限元网格生成器，内置 CAD 引擎。**借鉴它的 `.geo` 脚本语言的声明式几何构造语法——`Point(1) = {0,0,0}; Line(1) = {1,2};` 与 Lv-00 DSL 的声明式构造风格一致。几何→网格的自动离散化管道为 Lv-00 符号几何→数值可视化的转换路径提供参考。OpenCASCADE 内核集成模式为 Lv-00 第三方几何库集成提供工程范式。Physical Group 语义标注对应 Lv-00 约束图节点分组系统** |

#### AM. SAT 纯布尔求解

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **CaDiCaL** | [github.com/arminbiere/cadical](https://github.com/arminbiere/cadical) | Armin Biere 开发的极简 CDCL SAT 求解器，~15K LOC 纯 C++ 无依赖代码。**借鉴它极简内核设计——不到 2000 行核心就能完成完整的 CDCL 循环，Lv-00 求解器内核的精简程度可以此为参照。增量求解+假设接口允许高效添加/撤销约束。LRAT/LKCP 证明追踪使每个 SAT 判定结果附带可独立验证的证明——直接对应 Lv-00 布尔层的可验证性。Watch-Literal 机制为 Lv-00 约束传播提供高效实现参考** |

#### AN. 微分方程与几何演化

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **SUNDIALS** | [github.com/LLNL/sundials](https://github.com/LLNL/sundials) | LLNL 开发的微分方程求解器套件（CVODE/IDA/ARKODE/KINSOL），MPI/CUDA/HIP 并行。**借鉴它的三层后端抽象（N_Vector→SUNMatrix→SUNLinearSolver）——Lv-00 数值路径可参考此设计，将坐标/矩阵/线性求解全部抽象化以支持 CPU/GPU 后端。自适应步长+误差控制为 Lv-00 几何演化（如曲线流形变形）提供精度保证。KINSOL 非线性代数求解为 Lv-00 几何非线性方程组提供数值求解器选择。事件检测（Rootfinding）为 Lv-00 几何交叉/接触事件提供检测框架** |

#### AO. Web 3D 渲染

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **Three.js** | [github.com/mrdoob/three.js](https://github.com/mrdoob/three.js) | 110k+ Stars 的 WebGL/WebGPU 3D 渲染库。**借鉴它的 Scene Graph 场景树架构——Lv-00 Web GUI 的几何可视化可组织为场景树（根节点→GeomNode→子对象）。BufferGeometry 批量顶点存储为几何体坐标数据的 WebGL 批量渲染提供方案。WebGPU 新后端为 Lv-00 计算着色器加速几何算法（并行距离计算、碰撞检测）打开可能。Raycaster 射线拾取为 Lv-00 几何对象点击选择提供交互参考。模块化 ESM 架构为 Lv-00 Web GUI 的组织方式提供范例** |

### 第九梯队落地详情

| 优先级 | 借鉴对象 | 借鉴内容 | 落地模块 | 状态 |
|:---|:---|:---|:---|:---:|
| P1 | **TLA+** | 时序逻辑三段式规约 + TLC 模型检查 | `docs/reference/tlaplus_formal_specification.md` + `include/lv/geo_spec.h`（238行：GeoConstructionSpec/GeoInvariant/StateSpaceExplorer/17 API） | ✅ |
| P1 | **Alloy** | 关系模型统一范式 + SAT 编码管道 | `docs/reference/alloy_relational_model_finder.md` + `include/lv/relation_model.h`（345行：原子/关系/公式/SmScope/18 API）+ `include/lv/sat_encoding.h`（288行：SatEncoding/CNF编码/15 API） | ✅ |
| P1 | **Eigen** | 纯头文件架构 + Geometry 模块 | `docs/reference/eigen_linear_algebra.md` + `include/lv/lv_numeric.h`（761行：Vec2-4/Mat3-4/Quat/Transform/SSE2加速/14个矩阵分解） | ✅ |
| P1 | **IPOPT** | 线性求解器抽象层 + 内点法 | `docs/reference/ipopt_nonlinear_optimization.md` + 求解器抽象架构已在 `solver_core.h`/`numerical_backend.h` 中体现 | ✅ |
| P2 | **Gmsh** | .geo 声明式构造语法 + 几何离散化 | `docs/reference/gmsh_mesh_generation.md` + `geo_spec.h` 中 GeoStepType 覆蓋 Point/Line/Circle等声明式构造 | ✅ |
| P2 | **CaDiCaL** | CDCL 极简内核 + LRAT 证明追踪 | `docs/reference/cadical_sat_solver.md` + `include/lv/solver_core.h`（365行：lvSolver/10状态CDCL/21 API） | ✅ |
| P2 | **SUNDIALS** | 三层后端抽象 + 自适应步长 + 事件检测 | `docs/reference/sundials_differential_equations.md` + `include/lv/numerical_backend.h`（396行：Vector/Matrix/LinSol三层抽象）+ `include/lv/geom_evol.h`（268行：PI步长控制）+ `include/lv/geo_event_detect.h`（305行：Brent求根） | ✅ |
| P3 | **Three.js** | Scene Graph + BufferGeometry + WebGPU | `docs/reference/threejs_web3d_rendering.md` + `lv_numeric.h` 中 Mat4/Quat 支撑 Web 端 3D 数学（Web GUI 组件待后续实现） | ✅ |

### 第九梯队按类别汇总

| 类别 | 项目数 | 落地产出 | 状态 |
|:---|:---:|:---|:---:|
| AI. 形式化规约与模型检测 | 2 | 2 篇设计文档（TLA+ 685行 + Alloy 724行） | ✅ |
| AJ. 高性能数值计算 | 1 | 1 篇设计文档（Eigen 598行） | ✅ |
| AK. 非线性优化 | 1 | 1 篇设计文档（IPOPT 495行） | ✅ |
| AL. 网格生成与离散化 | 1 | 1 篇设计文档（Gmsh 475行） | ✅ |
| AM. SAT 纯布尔求解 | 1 | 1 篇设计文档（CaDiCaL 697行） | ✅ |
| AN. 微分方程与几何演化 | 1 | 1 篇设计文档（SUNDIALS 698行） | ✅ |
| AO. Web 3D 渲染 | 1 | 1 篇设计文档（Three.js 648行） | ✅ |
| **总计** | **8** | **8 篇设计文档（≈5,020 行）** | ✅ |
| **91 个项目全部落地** | | | ✅✅✅✅✅✅ |

### 第九梯队带来的新设计线索

| 新线索 | 代表项目 | 核心启示 | Lv-00 对应模块 |
|:---|:---|:---|:---|
| **时序逻辑规约** | TLA+ | 几何构造可建模为状态变迁系统——Init/Next/Invariant 的规约框架天然适合"一步步构造然后验证不变式"的几何证明场景 | 规约层、证明引擎 |
| **关系即一切** | Alloy | 约束图可重新解释为关系代数——点/线/区域是原子的元组关系，约束是关系上的逻辑公式，SAT 编码实现自动验证 | 约束图、SAT 编码管道 |
| **头文件即分发** | Eigen | 真正的轻量级库不需要运行时链接——纯头文件 + 表达式模板 = 零依赖 + 最佳编译优化。Lv-00 的数值助手层可提供此模式 | 数值路径、分发模式 |
| **求解器抽象层** | IPOPT | 多后端求解器的关键不是实现每种算法，而是设计统一的线性代数抽象——让多种求解器可互换 | 求解器架构 |
| **几何即脚本** | Gmsh | `.geo` 语言的声明式语法与 Lv-00 DSL 目标一致——Point/Line/Surface/Volume 的自然语言化构造 | DSL 语法 |
| **极简即可信** | CaDiCaL | SAT 求解器的证明追踪（LRAT）允许第三方独立验证每一个判定——Lv-00 布尔层可达到同等可信度 | 求解器内核、证明输出 |
| **三层后端抽象** | SUNDIALS | N_Vector→SUNMatrix→SUNLinearSolver 的三层抽象使 C 代码天然支持 CPU/CUDA/HIP——Lv-00 数值路径获得异构计算支持 | 数值后端 |
| **Web 3D 成熟方案** | Three.js | 不需要从头构建 WebGL 渲染管线——Scene Graph + BufferGeometry + Raycaster 已提供完整的 3D 交互框架 | Web GUI |

---

## 十、累计统计

| 梯队 | 项目数 | 累计 | 落地日期 |
|:---|:---:|:---:|:---|
| 第一梯队（直接相关） | 4 | 4 | 2026-05-23 |
| 第二梯队（设计理念） | 4 | 8 | 2026-05-23 |
| 第三梯队（细分功能） | 4 | 12 | 2026-05-23 |
| 第四梯队（补充项目） | 10 | 22 | 2026-05-24 |
| 第五梯队（第二轮补充） | 15 | 37 | 2026-05-24 |
| 第六梯队（证明/重写/CAS） | 20 | 57 | 2026-05-24 |
| 第七梯队（E-Graphs等） | 18 | 75 | 2026-05-24 |
| 第八梯队（Datalog等） | 8 | 83 | 2026-05-24 |
| **第九梯队（规约/数值/可视化）** | **8** | **91** | **2026-05-24** |

### 全部分类汇总（共 91 个项目，25 个类别）

| 类别代码 | 类别名称 | 项目数 |
|:---|:---|:---:|
| 第一~三梯队 | 几何证明/计算/交互 | 12 |
| A | 自动几何证明 | 1 |
| B | 几何代数 | 4 |
| C | 符号几何计算 | 1 |
| D | 工业级几何内核 | 3 |
| E | 国产几何生态 | 1 |
| F | 几何构造语言 | 3 |
| G | 形式化证明辅助 | 4 |
| H | 约束求解与 SMT | 2 |
| I | 计算机代数系统 | 3 |
| J | Web 数学交互 | 3 |
| K | 最小化形式化语言 | 1 |
| L | 证明助手补完 | 5 |
| M | 重写逻辑系统 | 5 |
| N | 补充 CAS | 5 |
| O | 程序化几何建模 | 3 |
| P | 约束求解与建模 | 3 |
| Q | 可视化与几何处理 | 2 |
| R | E-Graphs | 2 |
| S | 应用范畴论 | 3 |
| T | FOL ATP | 3 |
| U | 符号数学核心库 | 3 |
| V | 图描述语言 | 2 |
| W | ML 辅助证明 | 2 |
| X/Y/Z | 交互/程序/建模 | 3 |
| AA~AH | Datalog/语言工程/区间/拓扑等 | 8 |
| **AI~AO** | **规约/数值/优化/网格/SAT/微分/3D** | **8** |
| **总计** | **25 类** | **91** |

---

## 十一、第十梯队落地总结（2026-05-24 第七次落地）

7 个新增项目已全部落地，详见 [`docs/reference/`](reference/)。

### 第十梯队新增项目清单

共 7 个项目，按类别分组。

#### AP. C 原生稀疏线性代数

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **SuiteSparse / GraphBLAS** | [github.com/DrTimothyAldenDavis/SuiteSparse](https://github.com/DrTimothyAldenDavis/SuiteSparse) | Prof. Tim Davis 开发的 C 语言稀疏矩阵算法套件，MATLAB 后端。**借鉴它的多重分解策略（LU/Cholesky/QR）——将 Lv-00 几何约束矩阵映射为稀疏矩阵，加速线性求解。GraphBLAS 的 Semiring 抽象（将矩阵乘法泛化为 (⊕,⊗) 对）为 constraint_graph 上的约束传播提供代数化引擎。纯 C 实现，与 Lv-00 内核技术栈一致。列压缩存储 (CSC) 格式直接对应约束矩阵的稀疏存储** |

#### AQ. 几何数据压缩

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **Draco** | [github.com/google/draco](https://github.com/google/draco) | Google 开发的 3D 几何数据压缩库，obj→draco 可达 20:1 压缩比。**借鉴它的预测编码策略（平行四边形预测、多阶预测）——Lv-00 几何构造序列可增量压缩存储。Edgebreaker 拓扑压缩对应 constraint_graph 拓扑优化。rANS 熵编码后端为 .lvz 文件的高效编码提供方案。属性压缩分离（位置/法线/颜色）对应 GeomNode 属性的分层序列化** |

#### AR. 浮点误差严格验证

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **FPTaylor** | [github.com/soarlab/FPTaylor](https://github.com/soarlab/FPTaylor) | Utah 大学的浮点舍入误差严格估计工具，基于符号泰勒展开+区间算术+全局优化混合架构。**借鉴它的符号泰勒形式——将 Lv-00 数值路径的浮点表达式展开为带一阶误差界的泰勒多项式。区间算术+分支定界（Gelpia/Z3）混合评估为 Lv-00 的数值安全确认提供第三条路径（在符号和浮点之间）。可输出 HOL Light 形式化证明——TrustColor 可用此误差界决定降级触发条件** |

#### AS. 浮点精度自动改进

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **Herbie** | [herbie.uwplse.org](https://herbie.uwplse.org) | Washington 大学的自动浮点精度改进工具，通过随机采样+Pareto 最优重写搜索找到更稳定的等价表达式。**借鉴它的随机采样驱动误差检测——识别数值路径中的"危险输入区域"。Pareto 最优重写搜索（精度-速度权衡）为 Lv-00 数值表达式自动优化提供方案。重写规则库（结合律重组、提前除零避免等）可直接映射到 Lv-00 rewrite 模块中的数值优化规则** |

#### AT. 概率模型检测

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **PRISM** | [prismmodelchecker.org](https://www.prismmodelchecker.org) | Oxford/Birmingham 大学的概率符号模型检测器，支持 DTMC/MDP/CTMC 等模型。**借鉴它的概率变迁系统建模——引入"几何不确定构造"概念（点坐标以概率分布而非精确值给出）。PCTL（概率计算树逻辑）为 Lv-00 扩展概率几何谓词（"以 >0.95 概率三点共线"）。MTBDD 符号化状态压缩为大规模概率约束空间的符号化表示提供方案** |

#### AU. 近似模型计数

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **ApproxMC** | [github.com/meelgroup/approxmc](https://github.com/meelgroup/approxmc) | Meel Group 的近似 #SAT 计数器，基于 CryptoMiniSat + Arjun 独立支持。C++ 实现，提供 PAC（Probably Approximately Correct）保证（默认 ε=0.8, δ=0.2）。**借鉴它的哈希基近似计数——将"约束图有多少有效构造"转化为基于 2-universal hash 的统计估计。Arjun 独立支持自动消除冗余变量——对应 Lv-00 约束图中的冗余节点检测。模型计数比 SAT 的 yes/no 更丰富——引入"近似可构造性"概念** |

#### AV. 二叉决策图（BDD）

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **CUDD** | [github.com/ivmai/cudd](https://github.com/ivmai/cudd) | Fabio Somenzi 开发的纯 C 语言 BDD/ADD/ZDD 操作库，在形式化验证领域有 30+ 年历史。**借鉴它的 BDD 变量序优化（sifting/group sifting）——Lv-00 求解器可通过 BDD 编码几何约束系统的布尔层。ADD（代数决策图）比 BDD 更适合数值信息的符号化——对应 Lv-00 符号坐标的 ADD 编码。ZDD（零压缩 BDD）稀疏表示对应约束集合的压缩存储。引用计数+唯一表机制对应 Lv-00 不变式子图缓存** |

### 第十梯队落地详情

| 优先级 | 借鉴对象 | 借鉴内容 | 落地模块 | 状态 |
|:---|:---|:---|:---|:---:|
| P1 | **SuiteSparse / GraphBLAS** | 稀疏矩阵分解 + Semiring 约束传播 | `docs/reference/suitesparse_graphblas_sparse_algebra.md`（405行） | ✅ |
| P1 | **Draco** | 预测编码 + Edgebreaker 拓扑压缩 | `docs/reference/draco_geometry_compression.md`（705行） | ✅ |
| P2 | **FPTaylor** | 符号泰勒展开 + 区间全局优化 | `docs/reference/fptaylor_rigorous_float_error.md`（839行） | ✅ |
| P2 | **Herbie** | 随机采样误差检测 + Pareto 重写优化 | `docs/reference/herbie_floating_point_improvement.md`（429行） | ✅ |
| P2 | **ApproxMC** | 哈希近似计数 + PAC 保证 | `docs/reference/approxmc_approximate_counting.md`（449行） | ✅ |
| P3 | **CUDD** | BDD/ADD/ZDD 决策图 + 变量序优化 | `docs/reference/cudd_binary_decision_diagrams.md`（523行） | ✅ |
| P3 | **PRISM** | PCTL 概率逻辑 + MTBDD 符号化 | `docs/reference/prism_probabilistic_model_checker.md`（506行） | ✅ |

### 第十梯队按类别汇总

| 类别 | 项目数 | 落地产出 | 状态 |
|:---|:---:|:---|:---:|
| AP. C 稀疏线性代数 | 1 | 1 篇设计文档（405行） | ✅ |
| AQ. 几何数据压缩 | 1 | 1 篇设计文档（705行） | ✅ |
| AR. 浮点误差验证 | 1 | 1 篇设计文档（839行） | ✅ |
| AS. 浮点精度改进 | 1 | 1 篇设计文档（429行） | ✅ |
| AT. 概率模型检测 | 1 | 1 篇设计文档（506行） | ✅ |
| AU. 近似模型计数 | 1 | 1 篇设计文档（449行） | ✅ |
| AV. 二叉决策图 | 1 | 1 篇设计文档（523行） | ✅ |
| **总计** | **7** | **7 篇设计文档（≈3,856 行）** | ✅ |
| **98 个项目全部落地** | | | ✅✅✅✅✅✅✅ |

### 第十梯队带来的新设计线索

| 新线索 | 代表项目 | 核心启示 | Lv-00 对应模块 |
|:---|:---|:---|:---|
| **Semiring 约束传播** | SuiteSparse/GraphBLAS | 约束传播可表达为 (⊕,⊗) 代数操作——矩阵乘法泛化为任意半环运算，稀疏矩阵直接对应约束图 | solver、constraint_graph |
| **预测编码几何存储** | Draco | 几何构造序列具有高度冗余，预测编码+熵编码可实现 20:1 压缩。构造历史天然是预测序列 | 数据交换层、.lvz 格式 |
| **泰勒形式数值安全** | FPTaylor | 浮点计算的安全不需要回到符号——一阶泰勒展开+区间算术可给出严格误差界，兼顾性能和安全 | symbolic_coord、TrustColor |
| **自动精度优化** | Herbie | 数值不稳定的表达式可通过代数等价重写自动修复——精度优化不需要人工审查 | 数值路径、rewrite |
| **PAC 计数保证** | ApproxMC | "有多少种构造方法"可以带有概率保证地近似回答——比精确计数快指数级 | 约束分析、解空间度量 |
| **决策图符号化** | CUDD | BDD/ADD 提供了布尔层和数值层的统一符号化——几何约束→BDD 编码→变量序优化→高效判定 | solver、布尔编码 |
| **概率几何推理** | PRISM | 几何不确定性可建模为概率变迁——"以 95% 概率构造有效"开辟了柔性验证新维度 | 证明引擎、约束类型 |

---

## 十二、更新后累计统计

| 梯队 | 项目数 | 累计 | 落地日期 |
|:---|:---:|:---:|:---|
| 第一梯队（直接相关） | 4 | 4 | 2026-05-23 |
| 第二梯队（设计理念） | 4 | 8 | 2026-05-23 |
| 第三梯队（细分功能） | 4 | 12 | 2026-05-23 |
| 第四梯队（补充项目） | 10 | 22 | 2026-05-24 |
| 第五梯队（第二轮补充） | 15 | 37 | 2026-05-24 |
| 第六梯队（证明/重写/CAS） | 20 | 57 | 2026-05-24 |
| 第七梯队（E-Graphs等） | 18 | 75 | 2026-05-24 |
| 第八梯队（Datalog等） | 8 | 83 | 2026-05-24 |
| 第九梯队（规约/数值/可视化） | 8 | 91 | 2026-05-24 |
| **第十梯队（稀疏/压缩/误差/概率/BDD）** | **7** | **98** | **2026-05-24** |

### 全部分类汇总（共 98 个项目，32 个类别）

| 类别代码 | 类别名称 | 项目数 |
|:---|:---|:---:|
| 第一~三梯队 | 几何证明/计算/交互 | 12 |
| A | 自动几何证明 | 1 |
| B | 几何代数 | 4 |
| C | 符号几何计算 | 1 |
| D | 工业级几何内核 | 3 |
| E | 国产几何生态 | 1 |
| F | 几何构造语言 | 3 |
| G | 形式化证明辅助 | 4 |
| H | 约束求解与 SMT | 2 |
| I | 计算机代数系统 | 3 |
| J | Web 数学交互 | 3 |
| K | 最小化形式化语言 | 1 |
| L | 证明助手补完 | 5 |
| M | 重写逻辑系统 | 5 |
| N | 补充 CAS | 5 |
| O | 程序化几何建模 | 3 |
| P | 约束求解与建模 | 3 |
| Q | 可视化与几何处理 | 2 |
| R | E-Graphs | 2 |
| S | 应用范畴论 | 3 |
| T | FOL ATP | 3 |
| U | 符号数学核心库 | 3 |
| V | 图描述语言 | 2 |
| W | ML 辅助证明 | 2 |
| X/Y/Z | 交互/程序/建模 | 3 |
| AA~AH | Datalog/语言工程/区间/拓扑等 | 8 |
| AI~AO | 规约/数值/优化/网格/SAT/微分/3D | 8 |
| **AP~AV** | **稀疏代数/几何压缩/误差验证/精度改进/概率检测/近似计数/BDD** | **7** |
| **总计** | **32 类** | **98** |

---

---

## 十三、第十一梯队落地总结（2026-05-25 第八次落地）

8 个新增项目已全部落地，详见 [`docs/reference/`](reference/)。

### 第十一梯队新增项目清单

共 8 个项目，按类别分组。

#### AW. 几何代数表示学习

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **GATr (Geometric Algebra Transformer)** | [github.com/Qualcomm-AI-research/geometric-algebra-transformer](https://github.com/Qualcomm-AI-research/geometric-algebra-transformer) | Qualcomm AI Research 的 NeurIPS 2023 论文实现，基于射影几何代数 G(3,0,1) 的 16 维多向量表示，E(3) 等变 Transformer 架构。**借鉴它的统一几何表示——点/向量/平面/旋转/平移全部编码为 16 维多向量分量，等变线性映射保证几何变换后表示一致性。几何积（bilinear）作为核心运算替代传统叉积/点积分离计算。接口函数（embed_point/extract_scalar）提供几何量与代数表示的双向桥接。对 Lv-00 第 1 层符号坐标系统的几何代数扩展有直接参考价值** |

#### AX. 多精度区间算术

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **MPFI** | [mpfr.org/mpfi](https://www.mpfr.org/mpfi/) | INRIA 基于 MPFR 的多精度区间算术 C 库，20+ 年历史，LGPL 许可。**借鉴它的严格数值边界计算——每个运算结果保证包含真实值（向外舍入），支持任意精度。作为 Lv-00 第 1 层符号坐标系统的第 5 种精确类型（区间），在符号（精确但慢）和浮点（快但不保真）之间提供"快且保真"的第三条路径。与 FLINT/Arb 的 Ball Arithmetic 互补——MPFI 提供标准区间语义，Arb 提供球算术语义。纯 C 实现，与 Lv-00 技术栈完美兼容** |

#### AY. AI 几何定理证明

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **Seed-Prover / Seed-Geometry** | [字节跳动 Seed 团队](https://arxiv.org/abs/2501.00693) | 字节跳动 Seed 团队的形式化数学推理专用模型，Seed-Prover 1.5 基于 Agentic RL 训练。在 IMO-AG-50 基准上解决 43/50 道几何题（2000-2024），超越 AlphaGeometry 2；MiniF2F 数据集 100% 正确率。**借鉴它的神经+符号混合架构——LLM 长思维链 + 强化学习 + 自动辅助构造搜索。Agentic RL 训练范式为 Lv-00 第 7 层 LLM 编码助手的"AI 辅助证明搜索"提供前沿参考。形式化证明输出（Lean 4）与 Lv-00 第 6 层的 Coq 导出后端形成互补** |

#### AZ. 交互式几何逻辑

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **GeoLogic** | [github.com/nicowilliams/informal](https://github.com/nicowilliams/informal) | CWI 开发的 Coq 交互式平面几何逻辑系统，将几何直觉与形式化证明结合。**借鉴它的依赖类型编码几何不变量——利用 Coq 的类型系统将"三点共线""四点共圆"等几何性质编码为类型约束。几何变换（旋转/平移/对称）的形式化处理为 Lv-00 第 3 层类型系统的几何不变量追踪提供参考。构造性推理模式与 Lv-00 第 4 层 BHK 解释的构造性语义天然契合** |

#### BA. 浮点数值公式验证

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **Gappa** | [gappa.gitlabpages.inria.fr](https://gappa.gitlabpages.inria.fr/) | INRIA Guillaume Melquiond 开发的浮点数值公式自动验证工具，C++ 实现，最新版 1.8.0。**借鉴它的专用浮点证明 DSL——八种谓词（BND/ABS/REL/LIN/FIX/FLT/NZR/EQL）精确描述浮点性质，区间传播+重写规则双引擎自动证明。可自动生成 Coq 形式化证明脚本实现端到端验证。定点/浮点混合运算支持为 Lv-00 第 1 层位电路系统的浮点交互提供验证方案。与 FPTaylor/Herbie 形成浮点验证三件套** |

#### BB. SMT 自动求解

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **Alt-Ergo** | [github.com/OCamlPro/alt-ergo](https://github.com/OCamlPro/alt-ergo) | OCamlPro 和 Inria 联合开发的 SMT 自动定理证明器，Apache 2.0 许可。**借鉴它的 CDCL(T) 理论组合架构——SAT 核心调度多个理论求解器（等式/算术/数组），多态排序系统支持类型化推理，触发器 E-matching 机制处理量词实例化。作为 Why3/Frama-C/SPARK 的默认后端，其理论组合调度器设计对 Lv-00 第 3 层合一引擎的多后端调度有直接参考价值。CC(X) 框架的模块化扩展方式为 Lv-00 求解器插件架构提供范例** |

#### BC. 几何代数代码生成

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **GAALOP** | [github.com/gaalop/gaalop](https://github.com/gaalop/gaalop) | TU Darmstadt 开发的几何代数代码生成器/预处理器，Java 实现，LGPL 许可。**借鉴它的两阶段编译流水线——符号化简（Maple/Matlab）+ 代码生成（C/C++/CUDA/OpenCL），将高层几何代数表达式自动编译为优化的数值代码。支持自定义代数签名和多种 Clifford 代数（Cl(3,0,1)/Cl(5,0)）。TBA（基于乘法表的代数）方法将几何积分解为查表+累加，为 Lv-00 第 3 层公式引擎的代码生成后端提供参考。与 GATr 的几何代数表示形成互补——GATr 侧重学习，GAALOP 侧重编译** |

#### BD. 量子程序形式化验证

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **QBricks** | [github.com/qbricks/qbricks](https://github.com/qbricks/qbricks) | HASLab/INESC TEC/Universidade do Minho（葡萄牙）开发的量子程序形式化验证环境，基于 Why3 框架。**借鉴它的领域特定操作建模方法——将量子计算操作（量子态/量子门/量子测量）建模为 WhyML 函数，利用 Why3 多证明器分派（Alt-Ergo/Z3/CVC5）自动验证。分层抽象与精化关系为 Lv-00 第 2 层函数块系统的"几何操作→验证条件"建模提供方法论参考。验证条件的多引擎分派策略与 Lv-00 第 4 层多策略引擎的设计理念一致** |

### 第十一梯队落地详情

| 优先级 | 借鉴对象 | 借鉴内容 | 落地模块 | 状态 |
|:---|:---|:---|:---|:---:|
| P1 | **GATr** | PGA 多向量表示 + 等变线性映射 + 几何积接口 | `docs/reference/gatr_geometric_algebra_transformer.md`（441行） | ✅ |
| P1 | **MPFI** | 多精度区间算术 + 严格边界 + 第 5 种精确类型 | `docs/reference/mpfi_interval_arithmetic.md`（453行） | ✅ |
| P1 | **Seed-Prover** | Agentic RL + 神经符号混合 + 辅助构造搜索 | `docs/reference/seed_prover_neural_geometry.md`（454行） | ✅ |
| P2 | **GeoLogic** | 依赖类型几何不变量 + 构造性推理 | `docs/reference/geologic_coq_geometry.md`（530行） | ✅ |
| P2 | **Gappa** | 浮点证明 DSL + 区间传播 + Coq 证明生成 | `docs/reference/gappa_float_proof.md`（393行） | ✅ |
| P2 | **Alt-Ergo** | CDCL(T) 理论组合 + 多态排序 + 触发器 | `docs/reference/alt_ergo_smt_solver.md`（445行） | ✅ |
| P3 | **GAALOP** | 两阶段 GA 编译 + TBA 乘法表 + 代码生成 | `docs/reference/gaalop_geometric_algebra_codegen.md`（483行） | ✅ |
| P3 | **QBricks** | 领域特定建模 + 分层抽象 + 多引擎分派 | `docs/reference/qbricks_quantum_verification.md`（399行） | ✅ |

### 第十一梯队按类别汇总

| 类别 | 项目数 | 落地产出 | 状态 |
|:---|:---:|:---|:---:|
| AW. 几何代数表示学习 | 1 | 1 篇设计文档（441行） | ✅ |
| AX. 多精度区间算术 | 1 | 1 篇设计文档（453行） | ✅ |
| AY. AI 几何定理证明 | 1 | 1 篇设计文档（454行） | ✅ |
| AZ. 交互式几何逻辑 | 1 | 1 篇设计文档（530行） | ✅ |
| BA. 浮点数值验证 | 1 | 1 篇设计文档（393行） | ✅ |
| BB. SMT 自动求解 | 1 | 1 篇设计文档（445行） | ✅ |
| BC. 几何代数代码生成 | 1 | 1 篇设计文档（483行） | ✅ |
| BD. 量子程序验证 | 1 | 1 篇设计文档（399行） | ✅ |
| **总计** | **8** | **8 篇设计文档（≈3,598 行）** | ✅ |
| **106 个项目全部落地** | | | ✅✅✅✅✅✅✅✅ |

### 第十一梯队带来的新设计线索

| 新线索 | 代表项目 | 核心启示 | Lv-00 对应模块 |
|:---|:---|:---|:---|
| **统一几何代数表示** | GATr | 点/向量/平面/旋转/平移统一为 16 维多向量——消除传统几何表示的碎片化，等变映射保证变换一致性 | 第 1 层符号坐标、第 3 层公式引擎 |
| **区间路径** | MPFI | 在符号（精确慢）和浮点（快不保真）之间，区间算术提供"快且保真"的第三条路径——与 FLINT/Arb 的 Ball Arithmetic 互补 | 第 1 层精确坐标类型系统 |
| **神经辅助构造** | Seed-Prover | AI 不替代证明引擎，而是增强辅助构造搜索——Agentic RL 训练的 LLM 提供构造建议，形式化引擎验证正确性 | 第 7 层 LLM 助手、第 4 层证明引擎 |
| **类型即不变量** | GeoLogic | 几何不变量（共线/共圆/平行）可编码为依赖类型——类型检查自动验证几何性质，构造性推理保证证明可执行 | 第 3 层类型系统、第 4 层证明引擎 |
| **浮点证明 DSL** | Gappa | 浮点验证不需要通用 SMT——专用 DSL（8 种谓词）+ 区间传播 + 重写规则即可高效证明数值性质 | 第 1 层数值验证、第 3 层重写引擎 |
| **理论组合调度** | Alt-Ergo | CDCL(T) 架构的核心是 SAT 核心调度多个理论求解器——每个理论独立实现，通过理论组合器统一接口 | 第 3 层合一引擎、求解器架构 |
| **GA 编译器** | GAALOP | 几何代数表达式可通过符号化简+代码生成两阶段编译为高效数值代码——TBA 乘法表方法消除运行时代数开销 | 第 3 层公式引擎、第 6 层数据交换 |
| **领域特定验证** | QBricks | 任何计算领域（量子/几何/数值）都可建模为 WhyML 函数+验证条件——分层抽象+多引擎分派是通用方法论 | 第 2 层函数块、第 4 层验证器 |

---

## 十四、更新后累计统计

| 梯队 | 项目数 | 累计 | 落地日期 |
|:---|:---:|:---:|:---|
| 第一梯队（直接相关） | 4 | 4 | 2026-05-23 |
| 第二梯队（设计理念） | 4 | 8 | 2026-05-23 |
| 第三梯队（细分功能） | 4 | 12 | 2026-05-23 |
| 第四梯队（补充项目） | 10 | 22 | 2026-05-24 |
| 第五梯队（第二轮补充） | 15 | 37 | 2026-05-24 |
| 第六梯队（证明/重写/CAS） | 20 | 57 | 2026-05-24 |
| 第七梯队（E-Graphs等） | 18 | 75 | 2026-05-24 |
| 第八梯队（Datalog等） | 8 | 83 | 2026-05-24 |
| 第九梯队（规约/数值/可视化） | 8 | 91 | 2026-05-24 |
| 第十梯队（稀疏/压缩/误差/概率/BDD） | 7 | 98 | 2026-05-24 |
| **第十一梯队（GA/区间/AI证明/浮点/SMT/量子）** | **8** | **106** | **2026-05-25** |

### 全部分类汇总（共 106 个项目，40 个类别）

| 类别代码 | 类别名称 | 项目数 |
|:---|:---|:---:|
| 第一~三梯队 | 几何证明/计算/交互 | 12 |
| A | 自动几何证明 | 1 |
| B | 几何代数 | 4 |
| C | 符号几何计算 | 1 |
| D | 工业级几何内核 | 3 |
| E | 国产几何生态 | 1 |
| F | 几何构造语言 | 3 |
| G | 形式化证明辅助 | 4 |
| H | 约束求解与 SMT | 2 |
| I | 计算机代数系统 | 3 |
| J | Web 数学交互 | 3 |
| K | 最小化形式化语言 | 1 |
| L | 证明助手补完 | 5 |
| M | 重写逻辑系统 | 5 |
| N | 补充 CAS | 5 |
| O | 程序化几何建模 | 3 |
| P | 约束求解与建模 | 3 |
| Q | 可视化与几何处理 | 2 |
| R | E-Graphs | 2 |
| S | 应用范畴论 | 3 |
| T | FOL ATP | 3 |
| U | 符号数学核心库 | 3 |
| V | 图描述语言 | 2 |
| W | ML 辅助证明 | 2 |
| X/Y/Z | 交互/程序/建模 | 3 |
| AA~AH | Datalog/语言工程/区间/拓扑等 | 8 |
| AI~AO | 规约/数值/优化/网格/SAT/微分/3D | 8 |
| AP~AV | 稀疏代数/几何压缩/误差验证/精度改进/概率检测/近似计数/BDD | 7 |
| **AW~BD** | **GA表示/区间算术/AI证明/几何逻辑/浮点验证/SMT/GA编译/量子验证** | **8** |
| **总计** | **40 类** | **106** |

---

## 十五、第十二梯队落地总结（2026-05-25 第九次落地）

8 个新增项目已全部落地，详见 [`docs/reference/`](reference/)。

### 第十二梯队新增项目清单

共 8 个项目，按类别分组。

#### BE. 版本控制系统

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **libgit2** | [github.com/libgit2/libgit2](https://github.com/libgit2/libgit2) | GitHub 官方维护的纯 C Git 核心库，跨平台（Linux/macOS/iOS/Windows），支持 20+ 语言绑定。**借鉴它的对象存储模型——内容寻址（SHA-256）、引用系统（分支/标签）、索引机制（暂存区）、ODB 抽象（对象数据库后端可插拔）。为 Lv-00 第 6 层数据交换提供"证明版本控制"能力——约束图/证明树的提交历史、分支并行探索、差异检测、回滚恢复。与 libgit2 的 C99 实现和 GPLv2 with Linking Exception 许可证完美兼容** |

#### BF. 自动微分框架

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **Enzyme** | [github.com/EnzymeAD/Enzyme](https://github.com/EnzymeAD/Enzyme) | MIT/LLNL 联合开发的 LLVM 级自动微分框架，NeurIPS 2020 论文。**借鉴它的"优化后微分"策略——在 LLVM IR 优化后的代码上执行 AD，生成比传统"微分后优化"更高效的梯度代码。活性分析（Activity Analysis）确定哪些指令影响导数计算，减少冗余计算。支持前向/反向模式、高阶导数、GPU 内核微分。为 Lv-00 第 3 层公式引擎的微分能力提供 LLVM 级实现参考，与 Enzyme 的 Apache 2.0 许可证兼容** |

#### BG. 位向量 SMT 求解器

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **STP** | [github.com/stp/stp](https://github.com/stp/stp) | MIT Vijay Ganesh 团队开发的位向量与数组约束 SMT 求解器，KLEE 符号执行引擎的默认后端。**借鉴它的分层编码策略——词级优化（代数简化、常量传播）→ 位级编码（位向量转布尔电路）→ SAT 求解。数组的抽象解释机制（读链分析、索引等价检测、懒惰公理实例化）。为 Lv-00 第 3 层约束求解器提供位向量理论和数组理论的完整实现参考，与 STP 的 MIT 许可证兼容** |
| **Boolector** | [github.com/Boolector/boolector](https://github.com/Boolector/boolector) | JKU Linz 开发的位向量/数组 SMT 求解器，多次 SMT-COMP 冠军。**借鉴它的 Lambert 变换优化位向量乘法、动态变量消除、增量求解支持。多种 SAT 后端（MiniSat/CaDiCaL/PicoSAT/Lingeling）可插拔架构。为 Lv-00 第 3 层提供高性能位向量求解实现参考，与 Boolector 的 MIT 许可证兼容** |

#### BH. 数论计算库

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **NTL** | [libntl.org](https://libntl.org/) | Victor Shoup 开发的高性能数论库，始于 1990 年代。**借鉴它的模数上下文管理（ZZ_pContext 实现模运算状态隔离）、算法选择启发式（Karatsuba/FFT/NTT 根据输入大小自动选择）、线程安全设计。核心组件：ZZ（任意精度整数）、ZZ_pX（模 p 多项式）、GF2X（GF(2) 多项式）、LLL（格基约化）。为 Lv-00 第 1 层基础类提供数论算法实现参考，与 NTL 的 LGPL 许可证兼容** |

#### BI. 微分方程求解

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **DifferentialEquations.jl** | [github.com/SciML/DifferentialEquations.jl](https://github.com/SciML/DifferentialEquations.jl) | Julia 社区的统一微分方程求解套件，Chris Rackauckas 主导。**借鉴它的三层架构设计——Problem（问题定义）/ Algorithm（算法选择）/ Solution（解封装）。统一接口支持 ODE/SDE/DAE/DDE/PDE，自适应步长控制，自动算法选择（根据刚性/非刚性自动切换），回调系统（事件检测）。为 Lv-00 第 3 层数值求解模块提供统一接口设计参考，Julia 的 MIT 许可证** |

#### BJ. 自动化证明策略

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **Aesop** | [github.com/leanprover-community/aesop](https://github.com/leanprover-community/aesop) | Jannis Limperg 开发的 Lean 4 白盒自动化证明策略。**借鉴它的规则搜索架构——规则库（Rule Set）+ 证明状态（Proof State）+ 搜索策略（最佳优先/深度优先/广度优先）。可解释的证明搜索过程（每一步可见规则应用），规则优先级和权重系统，与 Lean 4 元编程深度集成。为 Lv-00 第 4 层证明引擎的自动化策略提供规则引擎设计参考，Apache 2.0 许可证** |

#### BK. 证明格式转换

| 项目 | 链接 | 最值得借鉴的地方 |
|:---|:---|:---|
| **Mathport** | [github.com/leanprover-community/mathport](https://github.com/leanprover-community/mathport) | Lean 官方团队的 Lean 3→Lean 4 自动移植工具。**借鉴它的分层转换架构——Synport（语法移植）+ Binport（二进制数据移植）+ Tactic 自动转换。AST 中间表示设计，前后端分离，错误报告和修复建议系统。处理 100 万+ 行 mathlib 的成功经验。为 Lv-00 第 6 层数据交换的"证明格式转换"（Coq↔Lean↔自定义）提供架构参考，Apache 2.0 许可证** |

### 第十二梯队落地详情

| 优先级 | 借鉴对象 | 借鉴内容 | 落地模块 | 状态 |
|:---|:---|:---|:---|:---:|
| P1 | **libgit2** | 对象存储模型 + 引用系统 + 索引机制 | `docs/reference/libgit2_version_control.md`（973行） | ✅ |
| P1 | **Enzyme** | LLVM 级自动微分 + 活性分析 | `docs/reference/enzyme_autodiff_llvm.md`（680行） | ✅ |
| P2 | **STP** | 位向量分层编码 + 数组抽象解释 | `docs/reference/stp_smt_solver.md`（520行） | ✅ |
| P2 | **Boolector** | Lambert 变换 + 增量求解 + SAT 后端 | `docs/reference/boolector_smt_solver.md`（540行） | ✅ |
| P2 | **NTL** | 模数上下文 + 算法选择启发式 + LLL | `docs/reference/ntl_number_theory.md`（780行） | ✅ |
| P3 | **DifferentialEquations.jl** | 统一接口三层架构 + 自适应算法选择 | `docs/reference/differentialequations_jl_diffeq.md`（550行） | ✅ |
| P3 | **Aesop** | 规则搜索架构 + 可解释证明过程 | `docs/reference/aesop_lean4_tactic.md`（537行） | ✅ |
| P3 | **Mathport** | AST 中间表示 + 分层转换架构 | `docs/reference/mathport_lean_migration.md`（599行） | ✅ |

### 第十二梯队按类别汇总

| 类别 | 项目数 | 落地产出 | 状态 |
|:---|:---:|:---|:---:|
| BE. 版本控制系统 | 1 | 1 篇设计文档（973行） | ✅ |
| BF. 自动微分框架 | 1 | 1 篇设计文档（680行） | ✅ |
| BG. 位向量 SMT 求解器 | 2 | 2 篇设计文档（1060行） | ✅ |
| BH. 数论计算库 | 1 | 1 篇设计文档（780行） | ✅ |
| BI. 微分方程求解 | 1 | 1 篇设计文档（550行） | ✅ |
| BJ. 自动化证明策略 | 1 | 1 篇设计文档（537行） | ✅ |
| BK. 证明格式转换 | 1 | 1 篇设计文档（599行） | ✅ |
| **总计** | **8** | **8 篇设计文档（≈5,179 行）** | ✅ |
| **114 个项目全部落地** | | | ✅✅✅✅✅✅✅✅ |

### 第十二梯队带来的新设计线索

| 新线索 | 代表项目 | 核心启示 | Lv-00 对应模块 |
|:---|:---|:---|:---|
| **证明版本控制** | libgit2 | 约束图/证明树可用 Git 模型管理——内容寻址、分支并行探索、差异检测、回滚恢复。"证明即代码"不仅是口号，更需要完整的版本控制基础设施 | 第 6 层数据交换、第 2 层约束图 |
| **优化后微分** | Enzyme | 自动微分应在 LLVM IR 优化后执行，而非源代码层面——优化后的代码结构更简单，生成的梯度代码更高效。活性分析消除冗余导数计算 | 第 3 层公式引擎、数值计算模块 |
| **位向量分层编码** | STP/Boolector | 位向量求解不是直接转 SAT——词级优化（代数简化）→ 位级编码（电路生成）→ SAT 求解的三层架构更高效。数组需要抽象解释而非直接展开 | 第 3 层约束求解器、第 1 层位电路系统 |
| **算法选择启发式** | NTL | 同一运算（乘法/多项式乘法）应根据输入大小自动选择算法——Karatsuba/FFT/NTT 的切换阈值需要精细调优。模数上下文隔离保证线程安全 | 第 1 层基础类、第 3 层算法引擎 |
| **统一问题接口** | DifferentialEquations.jl | 不同数值问题（ODE/DAE/优化）可共享统一的三层接口——Problem/Algorithm/Solution 分离实现关注点分离，算法选择器根据问题特征自动匹配 | 第 3 层数值求解、第 7 层应用框架 |
| **白盒自动化** | Aesop | 自动化证明不应是黑盒——规则搜索过程应可解释（每步可见规则应用），规则优先级/权重可配置，失败时提供诊断信息 | 第 4 层证明引擎、自动化策略 |
| **AST 中间表示** | Mathport | 格式转换需要 AST 中间表示——源格式解析为 AST → AST 转换 → 目标格式生成。前后端分离支持多源多目标组合 | 第 6 层数据交换、证明格式转换 |

---

## 十六、更新后累计统计

| 梯队 | 项目数 | 累计 | 落地日期 |
|:---|:---:|:---:|:---|
| 第一梯队（直接相关） | 4 | 4 | 2026-05-23 |
| 第二梯队（设计理念） | 4 | 8 | 2026-05-23 |
| 第三梯队（细分功能） | 4 | 12 | 2026-05-23 |
| 第四梯队（补充项目） | 10 | 22 | 2026-05-24 |
| 第五梯队（第二轮补充） | 15 | 37 | 2026-05-24 |
| 第六梯队（证明/重写/CAS） | 20 | 57 | 2026-05-24 |
| 第七梯队（E-Graphs等） | 18 | 75 | 2026-05-24 |
| 第八梯队（Datalog等） | 8 | 83 | 2026-05-24 |
| 第九梯队（规约/数值/可视化） | 8 | 91 | 2026-05-24 |
| 第十梯队（稀疏/压缩/误差/概率/BDD） | 7 | 98 | 2026-05-24 |
| 第十一梯队（GA/区间/AI证明/浮点/SMT/量子） | 8 | 106 | 2026-05-25 |
| **第十二梯队（版本控制/自动微分/位向量SMT/数论/微分方程/自动化策略/格式转换）** | **8** | **114** | **2026-05-25** |

### 全部分类汇总（共 114 个项目，47 个类别）

| 类别代码 | 类别名称 | 项目数 |
|:---|:---|:---:|
| 第一~三梯队 | 几何证明/计算/交互 | 12 |
| A | 自动几何证明 | 1 |
| B | 几何代数 | 4 |
| C | 符号几何计算 | 1 |
| D | 工业级几何内核 | 3 |
| E | 国产几何生态 | 1 |
| F | 几何构造语言 | 3 |
| G | 形式化证明辅助 | 4 |
| H | 约束求解与 SMT | 2 |
| I | 计算机代数系统 | 3 |
| J | Web 数学交互 | 3 |
| K | 最小化形式化语言 | 1 |
| L | 证明助手补完 | 5 |
| M | 重写逻辑系统 | 5 |
| N | 补充 CAS | 5 |
| O | 程序化几何建模 | 3 |
| P | 约束求解与建模 | 3 |
| Q | 可视化与几何处理 | 2 |
| R | E-Graphs | 2 |
| S | 应用范畴论 | 3 |
| T | FOL ATP | 3 |
| U | 符号数学核心库 | 3 |
| V | 图描述语言 | 2 |
| W | ML 辅助证明 | 2 |
| X/Y/Z | 交互/程序/建模 | 3 |
| AA~AH | Datalog/语言工程/区间/拓扑等 | 8 |
| AI~AO | 规约/数值/优化/网格/SAT/微分/3D | 8 |
| AP~AV | 稀疏代数/几何压缩/误差验证/精度改进/概率检测/近似计数/BDD | 7 |
| AW~BD | GA表示/区间算术/AI证明/几何逻辑/浮点验证/SMT/GA编译/量子验证 | 8 |
| **BE~BK** | **版本控制/自动微分/位向量SMT/数论/微分方程/自动化策略/格式转换** | **8** |
| **总计** | **47 类** | **114** |

---

*最后更新：2026-05-25（第十二梯队落地完成，累计 114 个项目）*
