/-
Lv-00 Theory: 形式化验证系统统一纲领 (v1.2 R2)
=================================================

本文件是整个 lvFormal 形式化系统的顶层导入纲领，
按依赖层次组织全部 93 个理论模块。

层次结构（Layer 0 最先导入）：
  Layer 0:  核心语义定义 (lvLang, IR, Cv00Lang, Cv00Memory, LogicalFramework)
  Layer 1:  编译器与代码生成 (Compiler, Codegen, Correctness)
  Layer 2:  自举正确性 (Bootstrap)
  Layer 3:  几何与代数 (Geometry, GeometricAlgebra, Differential)
  Layer 4:  约束求解、重写与模型论 (Constraint, Solver, Groebner, Rewrite, ModelTheory)
  Layer 5:  数值方法 (Numeric, ODE)
  Layer 6:  证据、策略自动化与元验证 (Evidence, ProofStrategy, AxiomDiscovery, MetaVerification)
  Layer 7:  互操作与导出 (Interop, Formula)
  Layer 8:  引擎、流式与端到端正确性 (Engine, Streaming, EndToEnd)
  Layer 9:  其他资产与架构 (DSL, Visual, Math, RemovedModule, TheoryHierarchy)

使用方式：
  在需要导入整个形式化体系的文件中，使用：
    import lvFormal.Theory.Theory
-/

-- =============================================================
-- Layer 0: 核心语义定义与逻辑框架
-- =============================================================
import lvFormal.Theory.lvLang           -- 源语言语法与操作语义
import lvFormal.Theory.LvDSL            -- Lv 语言形式化语法定义
import lvFormal.Theory.LvSemantics      -- Lv 语言语义与 IR 连接
import lvFormal.Theory.IR               -- 中间表示与几何语义
import lvFormal.Theory.Cv00Lang         -- Cv00 (C11 子集) 语法与表达式语义
import lvFormal.Theory.Cv00Memory       -- Cv00 内存模型与语句大步语义
import lvFormal.Theory.LogicalFramework -- 逻辑框架基础 (签名/公式/理论/模型/可靠性/完备性)

-- =============================================================
-- Layer 1: 编译器与代码生成
-- =============================================================
import lvFormal.Theory.Compiler         -- lvLang → IR 编译器
import lvFormal.Theory.Codegen          -- IR → Cv00 代码生成器
import lvFormal.Theory.CompilerCorrectness  -- 编译正确性 (lvLang ↔ IR 语义保持)
import lvFormal.Theory.CodegenCorrectness   -- 代码生成正确性 (IR → Cv00 安全性/语义保持)
import lvFormal.Theory.UndefinedBehavior    -- C11 未定义行为分类与 UB-free 证明
import lvFormal.Theory.CompilerTooling      -- 编译器工具链：链接器、调试器、追踪器、剖析器、IR优化器

-- =============================================================
-- Layer 2: 自举正确性
-- =============================================================
import lvFormal.Theory.BootstrapDefs       -- 自举编译器补充定义
import lvFormal.Theory.BootstrapCorrectness -- 自举流程正确性
import lvFormal.Theory.ResourceManagement    -- 资源管理：熔断器、调试工具、错误码系统、测试框架
import lvFormal.Theory.MemorySafetyTheory    -- 内存安全：内存池不变量、无双重释放/泄漏/释放后使用
import lvFormal.Theory.GlobalStateInvariantsTheory -- 全局状态：配置一致性、线程安全、初始化保证、会话隔离
import lvFormal.Theory.ContextInvariantsTheory -- 上下文不变量：嵌套深度、作用域遮蔽、深/浅拷贝、转换往返
import lvFormal.Theory.CacheCoherenceTheory  -- 缓存一致性：LRU/FIFO/LFU驱逐、脏页写回、FastIndex O(1)查找
import lvFormal.Theory.CryptoHashTheory      -- 密码哈希：SHA-256压缩函数、Merkle-Damgård、碰撞阻力
import lvFormal.Theory.EntryPointTheory     -- 系统入口：初始化序列/阶段排序/主循环终止/资源生命周期

-- =============================================================
-- Layer 3: 几何与代数
-- =============================================================
import lvFormal.Theory.GeometryPresetDefs     -- 几何预设核心代数定义
import lvFormal.Theory.PresetGeometryDefs     -- 预设几何定义 (点/线/圆等)
import lvFormal.Theory.GeometryPresets        -- 几何预设定理 (shoelace 等)
import lvFormal.Theory.PresetGeometry         -- 预设几何定理 (欧拉线等)
import lvFormal.Theory.GeometricAlgebraDefs   -- 几何代数多向量 (带维度参数)
import lvFormal.Theory.GeometricAlgebra       -- 几何代数 (无维度参数)
import lvFormal.Theory.DifferentialGeometry  -- 微分几何 (绝妙定理)
import lvFormal.Theory.NDimGeometry          -- N 维几何
import lvFormal.Theory.CSGGeometryTheory     -- 构造实体几何：BSP树、布尔运算、包围盒层级、网格评估
import lvFormal.Theory.GeometrySymbolicsTheory -- 符号代数：有理数/二次根式/代数数/超越数、类型提升格、精确算术闭包
import lvFormal.Theory.GeometryTopologyTheory -- 几何拓扑：半边网格、Euler示性数、几何谓词、参数曲线、拓扑不变量
import lvFormal.Theory.SparseLinearAlgebraTheory -- 稀疏线性代数：CSR/CSC、共轭梯度、Sylvester结式、Lanczos、GMRES
import lvFormal.Theory.IntervalArithmeticTheory -- 区间算术：外延舍入、Taylor误差、自适应验证、Gappa/Herbie
import lvFormal.Theory.GeometryCompressionTheory -- 几何压缩：量化误差、无损压缩/解压、变换组合、图哈希
import lvFormal.Theory.GeometryInfrastructure-- 几何基础设施：块调度器、缓存管理、跨平台抽象、确定性状态机

-- =============================================================
-- Layer 4: 约束求解与重写
-- =============================================================
import lvFormal.Theory.ConstraintPropagation  -- AC-3 约束传播
import lvFormal.Theory.ConstraintSoundness    -- 约束求解可靠性与等价类管理
import lvFormal.Theory.SolverCorrectness      -- 求解器正确性
import lvFormal.Theory.GroebnerTheory         -- Groebner 基 (Buchberger 算法)
import lvFormal.Theory.RewriteProperties      -- λ-项重写 (强规范化/Church-Rosser)
import lvFormal.Theory.NormalizationProperties-- 表达式规范化
import lvFormal.Theory.ConstraintModelTheory  -- 约束模型论 (约束图的一阶逻辑嵌入/几何模型/无量词完备性)
import lvFormal.Theory.SMTTheory              -- SMT理论：位向量、Nelson-Oppen理论组合、E-matching量词实例化
import lvFormal.Theory.SolverInfrastructure   -- 求解器基础设施：BDD编码、冲突检测、自适应剪枝、稀疏线性代数
import lvFormal.Theory.SATEncoding            -- SAT符号编码：变元映射、CNF子句、Tseitin变换、可靠性/完备性
import lvFormal.Theory.BDDEncoding            -- BDD编码：唯一表哈希、ITE算法、Shannon展开、Sifting优化、ADD
import lvFormal.Theory.FuncBlockTheory        -- 函数块理论：端口、组合结合律、例化语义保持、确定性验证
import lvFormal.Theory.TypeLogicTheory        -- 类型逻辑：Kleene强三值逻辑、Kripke模态逻辑K、一阶量词
import lvFormal.Theory.ATPBackendTheory       -- ATP后端：TPTP编码、SZS解析、ATP→Lv证明映射、后端选择策略
import lvFormal.Theory.ModuleSystemTheory     -- 模块系统：加载/卸载、增量更新、LVZ打包、依赖管理、生命周期
import lvFormal.Theory.UnificationTheory      -- 统一化算法：Martelli-Montanari、MGU、Occurs Check、三角形式
import lvFormal.Theory.ExpressionCanonicalizationTheory -- 表达式规范化：单项式、合并同类项、规范排序、符号归一化
import lvFormal.Theory.SolverModelTheory      -- 求解器模型：代数模式、推理缓存、递归管理、关系模型、多域封闭性
import lvFormal.Theory.AutoDiffTheory         -- 自动微分：双数代数前向模式、计算图反向模式、链式法则正确性
import lvFormal.Theory.ExactArithmeticTheory  -- 精确算术：安全乘法溢出检测、安全加法/减法、快速幂溢出防护
import lvFormal.Theory.GroupTheoryFoundation -- 群论基础：群/环/域/模/Lagrange/同构/Cayley/中国剩余定理
import lvFormal.Theory.NumberTheoryFoundation -- 数论基础：素数/GCD/Euclidean/费马小定理/二次互反律
import lvFormal.Theory.LinearAlgebraFoundation -- 线性代数基础：矩阵分解/Cramer/Gram-Schmidt/Cholesky/SVD

-- =============================================================
-- Layer 5: 数值方法
-- =============================================================
import lvFormal.Theory.NumericDefs    -- 数值计算核心定义 (GMRES/CG/Power)
import lvFormal.Theory.Numeric        -- 数值方法 (二分/Horner)
import lvFormal.Theory.ODESolverDefs  -- ODE 求解器定义 (RK4/AB4)
import lvFormal.Theory.ODESolver      -- ODE 求解器正确性

-- =============================================================
-- Layer 6: 证据、策略自动化与元验证
-- =============================================================
import lvFormal.Theory.Evidence               -- 证据自检查系统 (零信任验证)
import lvFormal.Theory.ProofStrategy          -- 证明策略与自动化 (Goal/Tactic/策略组合子/搜索)
import lvFormal.Theory.MetaVerificationTheory -- 元验证理论 (自指/一致性/信任模型/反射原理)
import lvFormal.Theory.AxiomDiscoveryTheory   -- 公理发现过程 (多步推导/可靠性/覆盖率)
import lvFormal.Theory.MetaProofTheory        -- 元证明：L1/L2/L3剪枝可靠性、信任颜色、完备性报告
import lvFormal.Theory.ProofCompilerTheory    -- 证明编译：多格式导出、证明对象模型、迹事件管理

-- =============================================================
-- Layer 7: 互操作与导出
-- =============================================================
import lvFormal.Theory.InteropSoundness    -- 互操作可靠性 (Coq/Lean/OPML/GeoJSON/SVG)
import lvFormal.Theory.InteropCorrectness  -- 互操作正确性 (证据桥接/格式兼容)
import lvFormal.Theory.FormulaSemantics    -- 公式语义 (LaTeX/DSL/Python)
import lvFormal.Theory.PluginSystemTheory -- 插件系统：动态库加载、状态机生命周期、接口注册、事件广播
import lvFormal.Theory.TikZExportTheory   -- TikZ导出：约束图到TikZ图形映射、节点渲染、坐标转换
import lvFormal.Theory.MagicConstantsTheory -- 魔数常量：版本/特性标志/枚举注册表/常量唯一性/向后兼容

-- =============================================================
-- Layer 8: 引擎与流式处理
-- =============================================================
import lvFormal.Theory.EngineInvariants        -- 引擎生命周期不变量
import lvFormal.Theory.ProofEngineSoundness    -- 证明引擎可靠性
import lvFormal.Theory.KernelInvariants        -- 内核不变量 (递归限制/缓存)
import lvFormal.Theory.StreamingTheory         -- 流式处理理论
import lvFormal.Theory.StreamInvariants        -- 流式不变量
import lvFormal.Theory.OrchestrationSoundness  -- 管道编排可靠性
import lvFormal.Theory.EndToEndCorrectness    -- 端到端正确性（组合所有安全性保证）

-- =============================================================
-- Layer 9: 其他资产与架构
-- =============================================================
import lvFormal.Theory.DSLWrappersSoundness    -- DSL 包装器可靠性
import lvFormal.Theory.GeomPresetSoundness     -- 几何预设可靠性
import lvFormal.Theory.InteractiveGeoSoundness -- 交互式几何可靠性
import lvFormal.Theory.MathPresetSoundness     -- 数学预设定理 (GCD/群论)
import lvFormal.Theory.VisualLayerSoundness    -- 可视化层可靠性
import lvFormal.Theory.RemovedModule           -- ROSE 循环认知模型
import lvFormal.Theory.ApplicationLayer        -- 应用层：CLI入口、批处理、输出导出、可视化命令
import lvFormal.Theory.ConvenienceAPIsTheory   -- 便捷函数：字符串/math/集合操作、安全性/正确性
import lvFormal.Theory.VisualBlockTheory       -- 可视化块语义：18种块类型、块图组合、数据流/转换器
import lvFormal.Theory.VisualRuntimeTheory     -- 可视化运行时：Canvas/RenderPipeline、事件处理、动画帧/调度器
import lvFormal.Theory.TheoryHierarchy         -- 理论层次架构 (依赖图/层结构/状态追踪)
