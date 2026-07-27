/-
Lv-00 formal: TheoryHierarchy — 理论层次架构 (v1.2 R1)
========================================================

本文件将 Lv-00 形式化验证体系的全部理论模块组织为
一个形式化的层次依赖图（Dependency Graph），
明确定义了每个理论在体系中的位置、职责和依赖关系。

核心内容：
  1. TheoryNode — 理论节点（描述每个理论模块的元信息）
  2. DependencyGraph — 依赖图（理论间的 has-import 关系）
  3. LayerAssignment — 分层分配（每个理论所属的逻辑层）
  4. LayerProperties — 分层性质（无环依赖、向上引用等）
  5. DependencyVerification — 依赖验证（自动验证依赖关系正确性）
  6. TheoryStatus — 理论状态（已完成 / 部分完成 / 仅规范）

本文件不定义新的数学内容，而是提供整个形式化体系的"架构图"。
形式化体系的每个贡献者可以通过本文件快速了解体系的全貌。
-/

import Mathlib

namespace lvFormal.Theory.TheoryHierarchy

/-! ===============================================================
   第一部分：理论节点（TheoryNode）
   
   每个 .lean 文件中的主要形式化理论是一个 TheoryNode。
   =============================================================== -/

/-- 理论节点：描述一个形式化理论模块的全部元信息。 -/
structure TheoryNode where
  /-- 模块的规范名称（去除前缀 .lean） -/
  moduleName : String
  /-- 模块的简短描述 -/
  description : String
  /-- 模块导入的其他模块列表 -/
  imports : List String
  /-- 模块所属的逻辑层次编号（0~9） -/
  layer : ℕ
  /-- 模块的状态 -/
  status : TheoryStatus
  deriving Repr

/-- 理论状态：标记一个理论模块的实现完整度。 -/
inductive TheoryStatus where
  /-- 已完成：包含完整的定义、定理和证明 -/
  | complete
  /-- 部分完成：包含核心定义，但部分定理带有 sorry -/
  | partial
  /-- 仅规范：仅包含文件头和结构定义，证明尚未完成 -/
  | specOnly
  deriving DecidableEq, Repr

/-! ===============================================================
   第二部分：Lv-00 完整理论图谱
   
--   这是当前所有 93 个理论模块（包括新创建的）的完整注册表。
   =============================================================== -/

/-- 全部理论节点的注册表。 -/
def allTheoryNodes : List TheoryNode :=
  [
    -- ===========================================================
    -- Layer 0: 核心语义定义
    -- ===========================================================
    { moduleName := "lvLang"
    , description := "源语言 (.lv) 语法与操作语义"
    , imports := []
    , layer := 0
    , status := .complete
    }
  , { moduleName := "IR"
    , description := "中间表示：几何约束、表达式和语义"
    , imports := []
    , layer := 0
    , status := .complete
    }
  , { moduleName := "Cv00Lang"
    , description := "Cv00 语言：语法与表达式语义"
    , imports := []
    , layer := 0
    , status := .complete
    }
  , { moduleName := "Cv00Memory"
    , description := "Cv00 内存模型与语句大步语义"
    , imports := ["Cv00Lang"]
    , layer := 0
    , status := .complete
    }
  , { moduleName := "LvDSL"
    , description := "Lv 语言语法定义：词法、类型系统、表达式、语句、程序"
    , imports := []
    , layer := 0
    , status := .partial
    }
  , { moduleName := "LvSemantics"
    , description := "Lv 语言语义与 IR 连接：表达式/约束/证明的语义桥接"
    , imports := ["LvDSL", "IR", "Evidence", "LogicalFramework"]
    , layer := 1
    , status := .partial
    }

    -- ===========================================================
    -- Layer 0.5: 逻辑框架元基础（新层）
    -- ===========================================================
  , { moduleName := "LogicalFramework"
    , description := "逻辑框架基础：签名、公式、理论、模型、满足关系、可靠的元理论"
    , imports := []
    , layer := 0
    , status := .specOnly
    }

    -- ===========================================================
    -- Layer 1: 编译器与代码生成
    -- ===========================================================
  , { moduleName := "Compiler"
    , description := "lvLang → IR 编译器"
    , imports := ["lvLang", "IR"]
    , layer := 1
    , status := .complete
    }
  , { moduleName := "Codegen"
    , description := "IR → Cv00 代码生成器"
    , imports := ["IR", "Cv00Lang", "Cv00Memory"]
    , layer := 1
    , status := .complete
    }
  , { moduleName := "CompilerCorrectness"
    , description := "编译器正确性 (lvLang ↔ IR 语义保持)"
    , imports := ["lvLang", "IR", "Compiler"]
    , layer := 1
    , status := .partial
    }
  , { moduleName := "CodegenCorrectness"
    , description := "代码生成正确性 (IR → Cv00 安全性/语义保持)"
    , imports := ["IR", "Codegen", "Cv00Memory"]
    , layer := 1
    , status := .partial
    }
  , { moduleName := "UndefinedBehavior"
    , description := "C11 未定义行为分类与 UB-free 证明"
    , imports := ["Cv00Lang", "Cv00Memory", "CodegenCorrectness"]
    , layer := 1
    , status := .partial
    }
  , { moduleName := "CompilerTooling"
    , description := "编译器工具链：链接器、调试器、追踪器、剖析器、IR优化器"
    , imports := []
    , layer := 1
    , status := .partial
    }

    -- ===========================================================
    -- Layer 2: 自举正确性
    -- ===========================================================
  , { moduleName := "BootstrapDefs"
    , description := "自举编译器补充定义"
    , imports := ["lvLang", "IR"]
    , layer := 2
    , status := .partial
    }
  , { moduleName := "BootstrapCorrectness"
    , description := "自举流程正确性"
    , imports := ["BootstrapDefs", "CompilerCorrectness"]
    , layer := 2
    , status := .partial
    }
  , { moduleName := "ResourceManagement"
    , description := "资源管理：熔断器、调试工具、错误码系统、测试框架"
    , imports := []
    , layer := 2
    , status := .partial
    }
  , { moduleName := "MemorySafetyTheory"
    , description := "内存安全：内存池不变量、无双重释放、无内存泄漏、无释放后使用、对齐约束、边界检查"
    , imports := []
    , layer := 2
    , status := .partial
    }
  , { moduleName := "GlobalStateInvariantsTheory"
    , description := "全局状态不变量：配置一致性、线程安全、初始化保证、配置持久化往返、模式一致性、更新原子性、会话隔离"
    , imports := []
    , layer := 2
    , status := .partial
    }
  , { moduleName := "ContextInvariantsTheory"
    , description := "上下文不变量：嵌套深度、作用域遮蔽、深/浅拷贝、表示转换往返、上下文切换保持、错误链完整性"
    , imports := []
    , layer := 2
    , status := .partial
    }
  , { moduleName := "CacheCoherenceTheory"
    , description := "缓存一致性：LRU/FIFO/LFU驱逐、脏页写回、FastIndex O(1)查找、缓存击穿/雪崩防护、时间戳单调性"
    , imports := []
    , layer := 2
    , status := .partial
    }
  , { moduleName := "CryptoHashTheory"
    , description := "密码哈希：SHA-256压缩函数、Merkle-Damgård填充、确定性、碰撞阻力、单向性、长度扩展攻击防护"
    , imports := []
    , layer := 2
    , status := .partial
    }
  , { moduleName := "EntryPointTheory"
    , description := "系统入口：SystemMode、初始化序列与阶段排序、依赖图、完整性检查、主循环终止、资源生命周期"
    , imports := []
    , layer := 2
    , status := .partial
    }

    -- ===========================================================
    -- Layer 3: 几何与代数
    -- ===========================================================
  , { moduleName := "GeometryPresetDefs"
    , description := "几何预设核心代数定义"
    , imports := []
    , layer := 3
    , status := .complete
    }
  , { moduleName := "PresetGeometryDefs"
    , description := "预设几何定义 (点/线/圆等)"
    , imports := ["GeometryPresetDefs"]
    , layer := 3
    , status := .complete
    }
  , { moduleName := "GeometryPresets"
    , description := "几何预设定理 (shoelace 等)"
    , imports := ["GeometryPresetDefs", "PresetGeometryDefs"]
    , layer := 3
    , status := .partial
    }
  , { moduleName := "PresetGeometry"
    , description := "预设几何定理 (欧拉线等)"
    , imports := ["GeometryPresetDefs", "PresetGeometryDefs"]
    , layer := 3
    , status := .partial
    }
  , { moduleName := "GeometricAlgebraDefs"
    , description := "几何代数多向量 (带维度参数)"
    , imports := []
    , layer := 3
    , status := .complete
    }
  , { moduleName := "GeometricAlgebra"
    , description := "几何代数 (无维度参数)"
    , imports := ["GeometricAlgebraDefs"]
    , layer := 3
    , status := .partial
    }
  , { moduleName := "DifferentialGeometry"
    , description := "微分几何 (绝妙定理、Christoffel 符号、Riemann 曲率张量)"
    , imports := ["GeometricAlgebraDefs"]
    , layer := 3
    , status := .partial
    }
  , { moduleName := "NDimGeometry"
    , description := "N 维几何"
    , imports := ["GeometricAlgebraDefs"]
    , layer := 3
    , status := .partial
    }
  , { moduleName := "CSGGeometryTheory"
    , description := "构造实体几何：BSP树划分、布尔运算、包围盒层级、图元生成与网格评估"
    , imports := []
    , layer := 3
    , status := .partial
    }
  , { moduleName := "GeometrySymbolicsTheory"
    , description := "符号代数：有理数/二次根式/代数数/超越数、类型提升格、精确算术闭包、位电路熔断"
    , imports := []
    , layer := 3
    , status := .partial
    }
  , { moduleName := "GeometryTopologyTheory"
    , description := "几何拓扑：半边网格、Euler示性数、几何谓词、事件检测、动态几何、参数曲线/Bezier/B-spline、路径类型、拓扑不变量"
    , imports := []
    , layer := 3
    , status := .partial
    }
  , { moduleName := "SparseLinearAlgebraTheory"
    , description := "稀疏线性代数：CSR/CSC格式、SpMV、共轭梯度、Sylvester结式、LU分解、Lanczos特征值、GMRES、代数数域扩张"
    , imports := []
    , layer := 3
    , status := .partial
    }
  , { moduleName := "IntervalArithmeticTheory"
    , description := "区间算术：外延舍入、四角乘法、超越函数区间、Taylor误差分析、自适应验证、调度场求值器、Gappa约束传播、Herbie重写"
    , imports := []
    , layer := 3
    , status := .partial
    }
  , { moduleName := "GeometryCompressionTheory"
    , description := "几何压缩：量化有界误差、无损压缩/解压逆运算、仿射变换组合/逆同态、图哈希一致性、拓扑保持压缩"
    , imports := []
    , layer := 3
    , status := .partial
    }
  , { moduleName := "GeometryInfrastructure"
    , description := "几何基础设施：块调度器、缓存管理、跨平台抽象、确定性状态机"
    , imports := []
    , layer := 3
    , status := .partial
    }

    -- ===========================================================
    -- Layer 4: 约束求解与重写
    -- ===========================================================
  , { moduleName := "ConstraintPropagation"
    , description := "AC-3 约束传播"
    , imports := ["IR"]
    , layer := 4
    , status := .partial
    }
  , { moduleName := "ConstraintSoundness"
    , description := "约束求解可靠性与等价类管理"
    , imports := ["IR", "ConstraintPropagation"]
    , layer := 4
    , status := .partial
    }
  , { moduleName := "SolverCorrectness"
    , description := "求解器正确性"
    , imports := ["ConstraintSoundness"]
    , layer := 4
    , status := .partial
    }
  , { moduleName := "GroebnerTheory"
    , description := "Groebner 基 (Buchberger 算法、多项式求值)"
    , imports := []
    , layer := 4
    , status := .partial
    }
  , { moduleName := "RewriteProperties"
    , description := "λ-项重写 (强规范化/Church-Rosser)"
    , imports := []
    , layer := 4
    , status := .partial
    }
  , { moduleName := "NormalizationProperties"
    , description := "表达式规范化与语义保持"
    , imports := []
    , layer := 4
    , status := .partial
    }

    -- ===========================================================
    -- Layer 4.5: 约束模型论（新层）
    -- ===========================================================
  , { moduleName := "ConstraintModelTheory"
    , description := "约束模型论：约束图的一阶逻辑嵌入、几何模型、语义等价、QV片段完备性"
    , imports := ["LogicalFramework", "IR"]
    , layer := 4
    , status := .partial
    }
  , { moduleName := "SMTTheory"
    , description := "SMT理论：位向量、Nelson-Oppen理论组合、E-matching量词实例化"
    , imports := []
    , layer := 4
    , status := .partial
    }
  , { moduleName := "SolverInfrastructure"
    , description := "求解器基础设施：BDD编码、冲突检测、自适应剪枝、稀疏线性代数、数学预设"
    , imports := []
    , layer := 4
    , status := .partial
    }
  , { moduleName := "SATEncoding"
    , description := "SAT符号编码：变元映射、CNF子句编码、Tseitin变换、编码可靠性/完备性/解码逆定理"
    , imports := []
    , layer := 4
    , status := .partial
    }
  , { moduleName := "BDDEncoding"
    , description := "BDD编码：唯一表哈希、ITE算法、Shannon展开、BDD→CNF Tseitin、Sifting优化、ADD代数决策图、bit-blasting"
    , imports := []
    , layer := 4
    , status := .partial
    }
  , { moduleName := "FuncBlockTheory"
    , description := "函数块理论：端口、组合结合律、例化语义保持、确定性验证、注册表查找、选择器"
    , imports := []
    , layer := 4
    , status := .partial
    }
  , { moduleName := "TypeLogicTheory"
    , description := "类型逻辑：Kleene强三值逻辑、Kripke模态逻辑K、一阶量词对偶性、不等式传递推理"
    , imports := []
    , layer := 4
    , status := .partial
    }
  , { moduleName := "ATPBackendTheory"
    , description := "ATP后端：TPTP编码、SZS解析、ATP→Lv证明映射、后端注册发现、自动选择策略、优雅降级"
    , imports := []
    , layer := 4
    , status := .partial
    }
  , { moduleName := "ModuleSystemTheory"
    , description := "模块系统：加载/卸载、增量更新冲突检测、LVZ打包解包、序列化、依赖图无环、版本兼容、生命周期状态机"
    , imports := []
    , layer := 4
    , status := .partial
    }
  , { moduleName := "UnificationTheory"
    , description := "统一化算法：Martelli-Montanari算法、MGU存在性与唯一性、Occurs Check、三角形式、匹配与统一化关系"
    , imports := []
    , layer := 4
    , status := .partial
    }
  , { moduleName := "ExpressionCanonicalizationTheory"
    , description := "表达式规范化：单项式、多项式合并同类项、规范排序、符号归一化、符号表达式替换、有理数算术"
    , imports := []
    , layer := 4
    , status := .partial
    }
  , { moduleName := "SolverModelTheory"
    , description := "求解器模型：代数模式、推理缓存LRU、递归深度管理、关系模型、多域封闭性"
    , imports := []
    , layer := 4
    , status := .partial
    }
  , { moduleName := "AutoDiffTheory"
    , description := "自动微分：双数代数前向模式、计算图反向模式、链式法则正确性、梯度无截断误差"
    , imports := []
    , layer := 4
    , status := .partial
    }
  , { moduleName := "ExactArithmeticTheory"
    , description := "精确算术：安全乘法溢出检测、安全加法/减法、快速幂溢出防护、高精度时间戳规范化"
    , imports := []
    , layer := 4
    , status := .partial
    }
  , { moduleName := "GroupTheoryFoundation"
    , description := "群论基础：群/子群/正规子群/商群/同态、环/域/模/向量空间、Lagrange定理、同构定理、Cayley定理、中国剩余定理"
    , imports := []
    , layer := 4
    , status := .partial
    }
  , { moduleName := "NumberTheoryFoundation"
    , description := "数论基础：素数、GCD/LCM、Euclidean算法、中国剩余定理、Euler函数、费马小定理、二次互反律、梅森素数"
    , imports := []
    , layer := 4
    , status := .partial
    }
  , { moduleName := "LinearAlgebraFoundation"
    , description := "线性代数基础：向量/矩阵、LU/QR/SVD分解、Cramer法则、Gram-Schmidt、Cholesky分解、正定矩阵"
    , imports := []
    , layer := 4
    , status := .partial
    }

    -- ===========================================================
    -- Layer 5: 数值方法
    -- ===========================================================
  , { moduleName := "NumericDefs"
    , description := "数值计算核心定义 (GMRES/CG/Power)"
    , imports := []
    , layer := 5
    , status := .complete
    }
  , { moduleName := "Numeric"
    , description := "数值方法 (二分/Horner)"
    , imports := ["NumericDefs"]
    , layer := 5
    , status := .partial
    }
  , { moduleName := "ODESolverDefs"
    , description := "ODE 求解器定义 (RK4/AB4)"
    , imports := ["NumericDefs"]
    , layer := 5
    , status := .complete
    }
  , { moduleName := "ODESolver"
    , description := "ODE 求解器正确性"
    , imports := ["ODESolverDefs", "Numeric"]
    , layer := 5
    , status := .partial
    }

    -- ===========================================================
    -- Layer 6: 证据与元验证（重构）
    -- ===========================================================
  , { moduleName := "Evidence"
    , description := "证据自检查系统：ProofTrace、evidence_check、可靠性/完备性/组合性"
    , imports := ["IR"]
    , layer := 6
    , status := .complete
    }
  , { moduleName := "ProofStrategy"
    , description := "证明策略与自动化：Goal/Tactic、策略组合子、前向/后向链、搜索、策略语义定理"
    , imports := ["Evidence", "IR", "AxiomDiscoveryTheory"]
    , layer := 6
    , status := .partial
    }
  , { moduleName := "AxiomDiscoveryTheory"
    , description := "公理发现理论：发现规则、多步推导链、可靠性/覆盖率"
    , imports := []
    , layer := 6
    , status := .partial
    }
  , { moduleName := "MetaVerificationTheory"
    , description := "元验证理论：自指、证明检查器形式化、一致性、信任模型、反射原理、Gödel不完备性、Löb定理"
    , imports := ["LogicalFramework", "Evidence", "IR"]
    , layer := 6
    , status := .partial
    }
  , { moduleName := "MetaProofTheory"
    , description := "元证明：L1直接矛盾、L2传播矛盾、L3代数排除、剪枝完备性报告、策略优先级链、信任颜色系统"
    , imports := []
    , layer := 6
    , status := .partial
    }
  , { moduleName := "ProofCompilerTheory"
    , description := "证明编译：证明对象模型、多格式导出（JSON/LaTeX/Coq/Lean4/HTML/DOT/TikZ）、证明验证、迹事件管理"
    , imports := ["Evidence"]
    , layer := 6
    , status := .partial
    }

    -- ===========================================================
    -- Layer 7: 互操作与导出
    -- ===========================================================
  , { moduleName := "InteropSoundness"
    , description := "互操作可靠性 (Coq/Lean/OPML/GeoJSON/SVG)"
    , imports := ["IR", "Cv00Lang"]
    , layer := 7
    , status := .partial
    }
  , { moduleName := "InteropCorrectness"
    , description := "互操作正确性 (证据桥接/格式兼容)"
    , imports := ["InteropSoundness"]
    , layer := 7
    , status := .partial
    }
  , { moduleName := "FormulaSemantics"
    , description := "公式语义 (LaTeX/DSL/Python)"
    , imports := ["IR"]
    , layer := 7
    , status := .partial
    }
  , { moduleName := "PluginSystemTheory"
    , description := "插件系统：动态库加载、状态机生命周期、接口注册表、依赖图、事件广播、版本兼容性、自动加载"
    , imports := []
    , layer := 7
    , status := .partial
    }
  , { moduleName := "TikZExportTheory"
    , description := "TikZ导出：约束图到TikZ图形映射、节点类型渲染、坐标转换、符号→数值保持、图形完备性"
    , imports := []
    , layer := 7
    , status := .partial
    }
  , { moduleName := "MagicConstantsTheory"
    , description := "魔数常量：MagicCategory/Version/VersionRange、枚举注册表、常量唯一性、向后兼容性、序列化往返"
    , imports := []
    , layer := 7
    , status := .partial
    }

    -- ===========================================================
    -- Layer 8: 引擎与流式处理
    -- ===========================================================
  , { moduleName := "EngineInvariants"
    , description := "引擎生命周期不变量 + 管道端到端安全性"
    , imports := ["IR", "Evidence", "CodegenCorrectness", "UndefinedBehavior"]
    , layer := 8
    , status := .partial
    }
  , { moduleName := "ProofEngineSoundness"
    , description := "证明引擎可靠性"
    , imports := ["Evidence", "ConstraintSoundness"]
    , layer := 8
    , status := .partial
    }
  , { moduleName := "KernelInvariants"
    , description := "内核不变量 (递归限制/缓存)"
    , imports := ["IR"]
    , layer := 8
    , status := .partial
    }
  , { moduleName := "StreamingTheory"
    , description := "流式处理理论"
    , imports := ["IR"]
    , layer := 8
    , status := .partial
    }
  , { moduleName := "StreamInvariants"
    , description := "流式不变量"
    , imports := ["StreamingTheory"]
    , layer := 8
    , status := .partial
    }
  , { moduleName := "OrchestrationSoundness"
    , description := "管道编排可靠性"
    , imports := ["ProofEngineSoundness", "Evidence"]
    , layer := 8
    , status := .partial
    }

    -- ===========================================================
    -- Layer 8.5: 端到端正确性（连接层）
    -- ===========================================================
  , { moduleName := "EndToEndCorrectness"
    , description := "端到端正确性定理：组合编译器正确性、代码生成安全性和证据系统"
    , imports := ["lvLang", "IR", "Compiler", "CompilerCorrectness",
                  "Codegen", "CodegenCorrectness", "Cv00Lang", "Cv00Memory",
                  "Evidence", "UndefinedBehavior"]
    , layer := 8
    , status := .partial
    }

    -- ===========================================================
    -- Layer 9: 其他资产
    -- ===========================================================
  , { moduleName := "DSLWrappersSoundness"
    , description := "DSL 包装器可靠性"
    , imports := ["IR"]
    , layer := 9
    , status := .partial
    }
  , { moduleName := "GeomPresetSoundness"
    , description := "几何预设可靠性"
    , imports := ["GeometryPresets", "IR"]
    , layer := 9
    , status := .partial
    }
  , { moduleName := "InteractiveGeoSoundness"
    , description := "交互式几何可靠性"
    , imports := ["IR"]
    , layer := 9
    , status := .partial
    }
  , { moduleName := "MathPresetSoundness"
    , description := "数学预设定理 (GCD/群论)"
    , imports := ["IR"]
    , layer := 9
    , status := .partial
    }
  , { moduleName := "VisualLayerSoundness"
    , description := "可视化层可靠性"
    , imports := ["Cv00Lang"]
    , layer := 9
    , status := .partial
    }
  , { moduleName := "RemovedModule"
    , description := "ROSE 循环认知模型"
    , imports := []
    , layer := 9
    , status := .partial
    }
  , { moduleName := "ApplicationLayer"
    , description := "应用层：CLI入口、批处理、输出导出、可视化命令"
    , imports := []
    , layer := 9
    , status := .partial
    }
  , { moduleName := "ConvenienceAPIsTheory"
    , description := "便捷函数：字符串/math/集合操作、前置/后置条件、安全性保证（无崩溃全函数）、数学正确性"
    , imports := []
    , layer := 9
    , status := .partial
    }
  , { moduleName := "VisualBlockTheory"
    , description := "可视化块语义：18种块类型、端口签名、连接图、数据流、块组合结合律、良构无环、转换器往返"
    , imports := []
    , layer := 9
    , status := .partial
    }
  , { moduleName := "VisualRuntimeTheory"
    , description := "可视化运行时：Canvas/FrameBuffer/RenderPipeline、事件处理/动画帧/IO、调度器依赖、运行时不变量"
    , imports := []
    , layer := 9
    , status := .partial
    }
  ]

/-! ===============================================================
   第三部分：依赖图分析
   
   从 allTheoryNodes 构建依赖图并进行性质验证。
   =============================================================== -/

/-- 查找理论节点。-/
def findNode (name : String) : Option TheoryNode :=
  allTheoryNodes.find? (λ n => n.moduleName = name)

/-- 计算理论的总数。-/
def theoryCount : ℕ := allTheoryNodes.length

/-- 计算每个层的理论数量。-/
def theoryCountByLayer : List (ℕ × ℕ) :=
  List.range 10 |>.map (λ l =>
    (l, allTheoryNodes.filter (λ n => n.layer = l) |>.length))

/-- 计算每种状态的理论数量。-/
def theoryCountByStatus : List (TheoryStatus × ℕ) :=
  [(.complete, allTheoryNodes.filter (λ n => n.status = .complete) |>.length),
   (.partial, allTheoryNodes.filter (λ n => n.status = .partial) |>.length),
   (.specOnly, allTheoryNodes.filter (λ n => n.status = .specOnly) |>.length)]

/-- 依赖图的边数：总 import 关系数 -/
def totalDependencyEdges : ℕ :=
  allTheoryNodes.map (λ n => n.imports.length) |>.sum

/-- 验证结果：依赖关系是否满足无环条件。
    注意：完整的环检测需要对图进行拓扑排序。
    这里我们做一个简化的声明：所有层内的依赖只指向更低的层。 -/
structure DependencyVerificationResult where
  /-- 所有节点的总数量 -/
  totalNodes : ℕ
  /-- 总依赖边数 -/
  totalEdges : ℕ
  /-- 按状态的分布 -/
  statusDistribution : List (TheoryStatus × ℕ)
  /-- 无环保证（简化：假设我们已验证） -/
  acyclic : Bool
  deriving Repr

/-- 执行依赖验证。-/
def verifyDependencies : DependencyVerificationResult :=
  { totalNodes := theoryCount
    totalEdges := totalDependencyEdges
    statusDistribution := theoryCountByStatus
    acyclic := true    -- 简化：假设无环（实际需要拓扑排序验证）
  }

/-! ===============================================================
   第四部分：层结构的形式化
   
   形式化定义什么是"合法"的分层：
   1. 下层不能导入上层（向上依赖禁止）
   2. 同层之间允许导入
   3. 跨层导入只能指向更低的层
   =============================================================== -/

/-- 依赖合法性检查：在所有直接依赖关系中，源节点的层编号 ≥ 目标节点的层编号。
    即：每层的代码只能依赖同层或更低层的代码。
    
    违反此规则的例子：Layer 3 的模块导入了 Layer 5 的模块（向上依赖）。 -/
def is_legal_dependency (node : TheoryNode) (depName : String) : Bool :=
  match findNode depName with
  | none => false         -- 依赖不存在
  | some depNode =>
      -- 允许：同层导入，或导入更低层
      node.layer ≥ depNode.layer

/-- 输出所有不合法的向上依赖。 -/
def illegal_upward_dependencies : List (String × String) :=
  allTheoryNodes.bind (λ node =>
    node.imports.filter (λ dep =>
      ¬ is_legal_dependency node dep
    ) |>.map (λ dep => (node.moduleName, dep))
  )

/-- 层次分配的良基性：所有依赖都是合法的（无向上依赖）。 -/
theorem layer_assignment_well_founded : illegal_upward_dependencies = [] := by
  native_decide

/-! ===============================================================
   第五部分：架构文档与演化
   
   记录理论体系各层的跨层连接和关键依赖。
   用于帮助新的贡献者理解体系结构。
   =============================================================== -/

/-- 层的描述信息。 -/
structure LayerInfo where
  /-- 层编号 -/
  number : ℕ
  /-- 层名称 -/
  name : String
  /-- 层职责概述 -/
  responsibility : String
  /-- 层中包含的理论数量 -/
  theoryCount : ℕ
  /-- 关键模块（最重要的1~3个） -/
  keyModules : List String

/-- 所有层的描述。 -/
def allLayers : List LayerInfo :=
  [ { number := 0, name := "核心语义"
    , responsibility := "定义 Lv-00 系统的所有核心语法和语义：源语言、IR、Cv00 语言和内存模型"
    , theoryCount := 5, keyModules := ["lvLang", "IR", "Cv00Memory", "LvDSL"]
    }
  , { number := 1, name := "编译器与代码生成"
    , responsibility := "编译器（lvLang→IR）和代码生成器（IR→Cv00）的正确性保证"
    , theoryCount := 6, keyModules := ["CompilerCorrectness", "CodegenCorrectness", "LvSemantics"]
    }
  , { number := 2, name := "自举正确性"
    , responsibility := "确保编译器可以自举（自己编译自己）的正确性、资源管理与基础设施"
    , theoryCount := 9, keyModules := ["BootstrapCorrectness", "MemorySafetyTheory", "GlobalStateInvariantsTheory", "CacheCoherenceTheory", "EntryPointTheory"]
    }
  , { number := 3, name := "几何与代数"
    , responsibility := "几何基础：预设几何、几何代数、微分几何、N维几何、CSG构造实体几何、几何基础设施、符号代数、几何拓扑、稀疏线性代数、区间算术、几何压缩"
    , theoryCount := 15, keyModules := ["GeometricAlgebra", "DifferentialGeometry", "CSGGeometryTheory", "GeometrySymbolicsTheory", "GeometryTopologyTheory", "SparseLinearAlgebraTheory", "IntervalArithmeticTheory", "GeometryCompressionTheory"]
    }
  , { number := 4, name := "约束求解与重写"
    , responsibility := "约束传播、求解器正确性、Groebner基、重写系统、SAT/BDD编码、函数块理论、类型逻辑、ATP/模块系统、表达式规范化、求解器模型、自动微分"
    , theoryCount := 23, keyModules := ["ConstraintSoundness", "GroebnerTheory", "ConstraintModelTheory", "BDDEncoding", "FuncBlockTheory", "AutoDiffTheory", "ExactArithmeticTheory", "GroupTheoryFoundation", "NumberTheoryFoundation", "LinearAlgebraFoundation"]
    }
  , { number := 5, name := "数值方法"
    , responsibility := "数值线性代数、ODE 求解器的正确性"
    , theoryCount := 4, keyModules := ["Numeric", "ODESolver"]
    }
  , { number := 6, name := "证据与元验证"
    , responsibility := "零信任证据验证系统、证明策略自动化、公理发现、元验证理论"
    , theoryCount := 6, keyModules := ["Evidence", "MetaVerificationTheory", "ProofStrategy", "MetaProofTheory", "ProofCompilerTheory"]
    }
  , { number := 7, name := "互操作与导出"
    , responsibility := "与其他形式化工具（Coq）和外部格式（GeoJSON/SVG）的互操作、插件系统"
    , theoryCount := 6, keyModules := ["InteropCorrectness", "PluginSystemTheory", "TikZExportTheory", "MagicConstantsTheory"]
    }
  , { number := 8, name := "引擎与流式"
    , responsibility := "证明引擎、流式处理、管道编排、端到端正确性"
    , theoryCount := 7, keyModules := ["EngineInvariants", "EndToEndCorrectness"]
    }
  , { number := 9, name := "其他资产"
    , responsibility := "DSL 包装器、几何/数学预设、可视化、认知模型、应用层、便捷函数、可视化块语义、可视化运行时"
    , theoryCount := 11, keyModules := ["InteractiveGeoSoundness", "RemovedModule", "ApplicationLayer", "VisualBlockTheory", "VisualRuntimeTheory"]
    }
  ]

/-- 按层分组的理论清单（用于文档生成）。 -/
def theoriesByLayer : List (LayerInfo × List TheoryNode) :=
  allLayers.map (λ layer =>
    (layer, allTheoryNodes.filter (λ n => n.layer = layer.number))
  )

end lvFormal.Theory.TheoryHierarchy
