# Lv-00 分层架构设计 v3.2.0

> 借鉴 OpenCASCADE (OCCT) 的 7 模块分层架构模型，
> 将 Lv-00 的模块按职责分层，形成清晰的依赖关系。

---

## 一、分层总览（OCCT 风格 7 层模型）

```
┌──────────────────────────────────────────────────────────────────┐
│  第 7 层  应用框架（Application Framework）                        │
│  ├─ Web GUI（React 组件：ProofPanel / ConstraintGraphPanel）       │
│  ├─ CLI 工具（concurrent_monitor / stream-monitor）               │
│  ├─ Jupyter 集成（Python DSL + notebook 可视化）                   │
│  └─ LLM 编码助手（llm_coding_assistant / API Server）              │
├──────────────────────────────────────────────────────────────────┤
│  第 6 层  数据交换（Data Exchange）                                │
│  ├─ 证明导出（HTML / LaTeX / Coq / 自然语言）                       │
│  ├─ 约束图序列化（JSON / 二进制）                                   │
│  ├─ 公理包格式（.lvz / INDEX.json / manifest.json）                │
│  ├─ 公式渲染（formula_renderer → LaTeX / MathML / 文本）           │
│  └─ 流式事件（StreamEvent → EventQueue → WebSocket）             │
├──────────────────────────────────────────────────────────────────┤
│  第 5 层  可视化引擎（Visualization Engine）                       │
│  ├─ 几何约束图可视化（ConstraintGraphPanel.tsx）                    │
│  ├─ 证明搜索树可视化（ProofSearchTree + DOT / JSON 导出）           │
│  ├─ 证明步骤导航器（ProofPanel + 时间线 SVG）                       │
│  └─ 几何叙事导出（NarrativeExport.tsx / Penrose 风格）              │
├──────────────────────────────────────────────────────────────────┤
│  第 4 层  证明引擎（Proof Engine）                                 │
│  ├─ 命题管理（Proposition：原子/合取/析取/蕴含/否定/量词/矛盾）      │
│  ├─ 证明导航器（ProofNavigator：步骤/断点/依赖/引理折叠）            │
│  ├─ 多策略引擎（ProofMultiStrategy：JGEX 8 种证明方法）              │
│  ├─ 合一检查（Unify：构造图 vs 命题模式）                           │
│  ├─ 爆炸原理（Ex Falso：从矛盾推出任意命题）                         │
│  ├─ 不可构造性分析（三等分角 / 倍立方 / 化圆为方）                    │
│  └─ 命题验证器（prop_verifier：BHK 解释 / 信任颜色桥接）            │
├──────────────────────────────────────────────────────────────────┤
│  第 3 层  算法引擎（Algorithm Engine）                              │
│  ├─ 约束求解器（solver：Groebner基 / 自由度计算 / 冲突检测）          │
│  ├─ 图规范化（normalization：幂等合并 / 作用域感知）                 │
│  ├─ 图重写（rewrite：模式匹配 / 规则应用 / 路径探索）               │
│  ├─ 合一引擎（unify：端口类型 → 约束类型 → 坐标等价 三层合一）        │
│  ├─ 类型系统（type_system：宇宙层级 / 等价检查 / 推断 / 依赖类型）   │
│  ├─ 递归系统（recursion：测度 / 深度监控 / 终止检查）                │
│  └─ 公式引擎（formula_parser / formula_converter / formula_renderer）│
├──────────────────────────────────────────────────────────────────┤
│  第 2 层  建模数据（Modeling Data）                                 │
│  ├─ 约束图（ConstraintGraph：节点/约束/哈希索引/O(1)查找）           │
│  ├─ 几何节点（GeomNode：点/线段/区域/端口/函数块）                   │
│  ├─ 函数块系统（FuncBlock：打包/实例化/部分应用/组合子/确定性检查）    │
│  ├─ 预设函数块（preset_*.h：42 个模块 / 200+ 函数块）               │
│  ├─ 公理包系统（axiom_packages：公理体系加载/升级/依赖管理）         │
│  └─ 模块系统（module：公理模块化 / 互操作）                          │
├──────────────────────────────────────────────────────────────────┤
│  第 1 层  基础类（Foundation Classes）                              │
│  ├─ 符号坐标（SymbolicCoord：有理数/代数数/二次扩域/超越数）          │
│  ├─ 多项式算术（mpz_poly：GMP 多精度 + 结式/因式分解）               │
│  ├─ 位电路系统（溢出熔断 / A/B 计划切换 / 压力测试）                  │
│  ├─ 信任颜色（TrustColor：绿/蓝/黄/橙/红 6 级）                     │
│  ├─ 错误码系统（error_codes：统一错误管理）                          │
│  ├─ 流式上下文（StreamContext：事件注册/分发/订阅）                   │
│  ├─ 图哈希（graph_hash：约束图快速比较）                             │
│  ├─ 深拷贝（node_deep_copy：节点递归复制）                           │
│  └─ 引擎核心（LV00Engine：生命周期 + 配置 + 健康检查 + 计数器）      │
└──────────────────────────────────────────────────────────────────┘
```

---

## 二、分层依赖规则

```
应用框架 (7) ──→ 数据交换 (6) ──→ 可视化 (5) ──→ 证明引擎 (4)
                                                      │
                                                      ↓
基础类 (1) ←──── 建模数据 (2) ←──── 算法引擎 (3) ←────┘
```

**依赖规则：**
- 上层可依赖下层，下层不可依赖上层
- 第 1 层（基础类）无外部依赖（仅依赖 GMP）
- 第 2-3 层依赖第 1 层
- 第 4 层依赖第 2-3 层 + 第 1 层
- 第 5-7 层可有额外外部依赖（Web 框架、Python 包等）

---

## 三、各层详细说明

### 第 1 层：基础类（Foundation Classes） — 核心数据结构与基础设施

| 模块 | 文件 | 职责 |
|:---|:---|:---|
| 符号坐标 | `symbolic_coord.h/c` | 4 种精确坐标类型 + 四则运算 + 比较 + 序列化 |
| 多项式算术 | `mpz_poly.h/c` | 基于 GMP 的多项式表示和运算 |
| 信任颜色 | `symbolic_coord.h` (TrustColor) | 6 级信任颜色，贯穿整个系统 |
| 位电路 | `symbolic_coord.h` (电路系统) | 溢出熔断 + A/B 计划切换 |
| 错误码 | `error_codes.h/c` | 统一错误码定义和管理 |
| 流式上下文 | `stream.h/c` | 事件系统基础设施 |
| 图哈希 | `graph_hash.h/c` | 约束图快速指纹 |
| 深拷贝 | `node_deep_copy.h/c` | 节点递归复制工具 |
| 引擎核心 | `engine.h/c` | 生命周期 + 配置 + 健康检查 |

**借鉴参考：** OCCT `TCollection` / `Standard` 包 — 基础类型和工具

---

### 第 2 层：建模数据（Modeling Data） — 几何数据模型

| 模块 | 文件 | 职责 |
|:---|:---|:---|
| 约束图 | `constraint_graph.h/c` | 核心数据结构：节点 + 约束 + 哈希索引 |
| 函数块 | `func_block*.h/c` | 打包/实例化/组合子/确定性检查 |
| 预设函数块 | `preset_*.h` (42 个) | 42 个数学领域的 200+ 预设函数块 |
| 公理包 | `axiom_packages/` | 公理体系加载/升级/依赖管理 |
| 模块系统 | `module.h/c` | 公理模块化 + 互操作 |

**借鉴参考：**
- OCCT `TopoDS` 包 — 拓扑数据结构（对应 GeomNode）
- OCCT `Geom` 包 — 几何表示（对应 SymbolicCoord 坐标系统）
- CGAL Kernel 概念 — 几何内核设计
- GeoCoq 公理分层 — 公理模块化组织

---

### 第 3 层：算法引擎（Algorithm Engine） — 计算与推理算法

| 模块 | 文件 | 职责 |
|:---|:---|:---|
| 求解器 | `solver.h/c` | Groebner基 + 自由度计算 + 冲突检测 + 交互式反馈 |
| 规范化 | `normalization.h/c` | 幂等合并 + 作用域感知 |
| 重写 | `rewrite.h/c` | 规则模式匹配 + 路径探索 |
| 合一 | `unify.h/c` | 三层合一（端口/约束/坐标） |
| 类型系统 | `type_system.h/c` | 宇宙层级 + 等价检查 + 推断 + 路径探索 |
| 递归 | `recursion.h/c` | 测度系统 + 终止检查 |
| 公式 | `formula_*.h/c` | 公式解析/转换/渲染 |

**借鉴参考：**
- OCCT `BRepAlgo` / `GeomAlgo` 包 — 几何算法
- JGEX C-tree 分解 — 约束分解策略（对标 solver）
- CGAL 算法包 — 算法组织方式

---

### 第 4 层：证明引擎（Proof Engine） — 命题与证明

| 模块 | 文件 | 职责 |
|:---|:---|:---|
| 命题管理 | `proof.h/c` (Proposition) | 8 种命题类型 + 模式图 + 前置/后置条件 |
| 证明导航器 | `proof.h/c` (ProofNavigator) | 步骤导航 + 断点 + 依赖链 + 引理折叠 |
| 多策略引擎 | `proof_multi_strategy.h/c` | 8 种证明方法并存（JGEX 架构） |
| 合一检查 | `proof.h/c` (proof_unify) | 构造图 vs 命题模式 |
| 爆炸原理 | `proof.h/c` (proof_ex_falso) | 从矛盾推出任意命题 |
| 不可构造性 | `proof.h/c` (unconstructibility) | 已知不可构造问题检查/归约 |
| 命题验证器 | `prop_verifier.h/c` | BHK 解释 / 信任颜色桥接 |

**借鉴参考：**
- JGEX/GEX — 多证明方法并存架构（8 种策略）
- LeanGeo — 证明呈现的清晰度 + 策略注释
- AlphaGeometry — 自然语言证明输出
- Newclid — 证明回溯与搜索树可视化

---

### 第 5 层：可视化引擎（Visualization Engine）

| 模块 | 文件 | 职责 |
|:---|:---|:---|
| 约束图可视化 | `ConstraintGraphPanel.tsx` (Web GUI) | 几何节点和约束的交互式可视化 |
| 证明搜索树 | `ProofSearchTree` (proof.h) | 搜索树 JSON / DOT 导出 |
| 证明导航 | `ProofPanel.tsx` (Web GUI) | 步骤导航 + 时间线 SVG |
| 几何叙事 | `NarrativeExport.tsx` (Web GUI) | Penrose 风格的几何→可视化叙事 |

**借鉴参考：**
- FRONTIER — 约束图可视化表达
- Penrose — 数学→可视化的叙事方式
- Solvespace — 交互式约束求解反馈

---

### 第 6 层：数据交换（Data Exchange）

| 模块 | 文件 | 职责 |
|:---|:---|:---|
| 证明导出 | `proof.h/c` (export) | HTML / LaTeX / Coq / 自然语言 |
| 约束图序列化 | `constraint_graph.h/c` (JSON) | JSON 格式读写 |
| 公理包格式 | `axiom_packages/` (.lvz) | .lvz 文件格式 + INDEX.json |
| 公式渲染 | `formula_renderer.h/c` | LaTeX / MathML / 纯文本 |
| 流式事件 | `stream.h/c` | 事件序列化 / WebSocket |

**借鉴参考：**
- AlphaGeometry — 自然语言证明输出格式
- Coq — 可验证的证明步骤导出
- OCCT STEP/IGES — 标准数据交换格式设计

---

### 第 7 层：应用框架（Application Framework）

| 模块 | 文件 | 职责 |
|:---|:---|:---|
| Web GUI | Web 前端（React 组件） | 可视化 + 交互式构造 + 证明导航 |
| CLI | `concurrent_monitor/` | 命令行交互和批处理 |
| Python DSL | `python/lv00/` | Python 绑定 + DSL + Workplane + AlgebraMode |
| LLM 助手 | `llm_coding_assistant/` | AI 辅助编码和构造 |
| 流式监控 | `stream-monitor/` | 实时事件监控 |
| Jupyter | Python DSL + 可视化 | Notebook 内嵌交互 |

**借鉴参考：**
- CadQuery — Fluent API / Selector DSL
- build123d — Algebra Mode / 操作符变换链
- GeoGebra — 几何对象的命名和引用体系
- GAP — 包管理和生态建设

---

## 四、模块依赖图

```
                           ┌─────────────────┐
                           │   应用框架 (7)    │
                           │  Web GUI / CLI   │
                           │  Python DSL / LLM│
                           └────────┬────────┘
                                    │
                           ┌────────▼────────┐
                           │   数据交换 (6)    │
                           │  Export / JSON   │
                           │  Stream / Render │
                           └────────┬────────┘
                                    │
                           ┌────────▼────────┐
                           │   可视化 (5)      │
                           │  GraphPanel      │
                           │  SearchTree      │
                           └────────┬────────┘
                                    │
                           ┌────────▼────────┐
          ┌───────────────│   证明引擎 (4)    │───────────────┐
          │               │  Proposition     │               │
          │               │  ProofNavigator  │               │
          │               │  MultiStrategy   │               │
          │               └────────┬────────┘               │
          │                        │                        │
   ┌──────▼──────┐          ┌──────▼──────┐          ┌──────▼──────┐
   │ 算法引擎(3)  │◄─────────│ 建模数据(2)  │─────────►│ 算法引擎(3)  │
   │  Solver     │          │  Graph       │          │  Rewrite    │
   │  Normalize  │          │  FuncBlock   │          │  Unify      │
   │  TypeSystem │          │  Preset      │          │  Recursion  │
   └──────┬──────┘          │  AxiomPkg    │          └──────┬──────┘
          │                 └──────┬──────┘                │
          │                        │                        │
          │                 ┌──────▼──────┐                │
          └────────────────►│  基础类 (1)  │◄───────────────┘
                            │  Coord       │
                            │  Poly        │
                            │  TrustColor  │
                            │  ErrorCode   │
                            │  Engine      │
                            └─────────────┘
```

---

## 五、借鉴来源汇总

| Lv-00 分层 | 借鉴来源 | 借鉴内容 |
|:---|:---|:---|
| 第 1 层 基础类 | OCCT Foundation | 基础类型和工具包设计 |
| 第 2 层 建模数据 | OCCT TopoDS/Geom + CGAL Kernel + GeoCoq | 拓扑/几何分离 + 公理分层 |
| 第 3 层 算法引擎 | OCCT BRepAlgo + JGEX C-tree + CGAL | 算法模块化 + 约束分解 |
| 第 4 层 证明引擎 | JGEX + LeanGeo + AlphaGeometry + Newclid | 多方法引擎 + 可读证明 + 搜索树 |
| 第 5 层 可视化 | FRONTIER + Penrose + Solvespace | 约束图可视化 + 叙事 + 交互反馈 |
| 第 6 层 数据交换 | AlphaGeometry + Coq + OCCT STEP/IGES | 可读输出 + 可验证导出 + 标准格式 |
| 第 7 层 应用框架 | CadQuery + build123d + GeoGebra + GAP | Fluent API + 代数模式 + 命名体系 + 包管理 |

---

## 六、与 OCCT 分层的对应关系

| OCCT 模块 | Lv-00 对应模块 | 说明 |
|:---|:---|:---|
| Foundation Classes | 第 1 层 基础类 | 基础类型、内存管理、异常处理 |
| Modeling Data | 第 2 层 建模数据 | 几何数据结构（TopoDS → GeomNode, Geom → SymbolicCoord） |
| Modeling Algorithms | 第 3 层 算法引擎 | 几何算法（BRepAlgo → Solver/Rewrite/Normalize） |
| — | 第 4 层 证明引擎 | Lv-00 独有的证明层（OCCT 无对应） |
| Visualization | 第 5 层 可视化引擎 | 图形渲染和交互 |
| Data Exchange | 第 6 层 数据交换 | 导入/导出/序列化 |
| Application Framework | 第 7 层 应用框架 | 应用级基础设施 |

> **注意：** 第 4 层（证明引擎）是 Lv-00 相比传统 CAD 内核（OCCT/CGAL）的独特扩展。
> 它将"构造=计算=证明"三者统一，是 Lv-00 作为几何元语言的核心差异。

---

*最后更新：2026-05-24*
*参考：OpenCASCADE Technology Architecture Overview*
