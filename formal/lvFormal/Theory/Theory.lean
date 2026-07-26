/-
Lv-00 Theory: 形式化验证系统统一纲领 (v1.2 R2)
=================================================

本文件是整个 lvFormal 形式化系统的顶层导入纲领，
按依赖层次组织全部 55 个理论模块。

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

-- =============================================================
-- Layer 2: 自举正确性
-- =============================================================
import lvFormal.Theory.BootstrapDefs       -- 自举编译器补充定义
import lvFormal.Theory.BootstrapCorrectness -- 自举流程正确性

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

-- =============================================================
-- Layer 7: 互操作与导出
-- =============================================================
import lvFormal.Theory.InteropSoundness    -- 互操作可靠性 (Coq/Lean/OPML/GeoJSON/SVG)
import lvFormal.Theory.InteropCorrectness  -- 互操作正确性 (证据桥接/格式兼容)
import lvFormal.Theory.FormulaSemantics    -- 公式语义 (LaTeX/DSL/Python)

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
import lvFormal.Theory.TheoryHierarchy         -- 理论层次架构 (依赖图/层结构/状态追踪)
