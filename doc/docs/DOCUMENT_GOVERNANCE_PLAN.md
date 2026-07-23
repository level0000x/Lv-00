# Lv-00 文档统一治理方案

> **版本**: 1.0.0
> **日期**: 2026-06-06
> **状态**: 待执行
> **基于**: 全量文档与源码审查（实际文件验证）

---

## 1. 治理目标与原则

### 1.1 目标
消除项目文档与源码之间的所有不一致，建立单一权威参考体系。

### 1.2 核心原则
| 原则 | 说明 |
|------|------|
| 代码优先 | 以实际源码目录结构和头文件定义为正典 |
| 单一权威 | 每个概念只在一个文档中定义，其他文档引用 |
| 最小破坏 | 优先修正文档而非重构代码 |
| 标记过渡 | 旧文档添加状态标记，不直接删除 |
| 版本分离 | 文档版本(DOC_VERSION)与代码版本(CODE_VERSION)独立管理 |

---

## 2. 正典定义：十层架构

### 2.1 权威来源
以 **README.md** 和 **VERSION_5.0.0.md** 中定义的十层架构为正典，因为其与实际代码目录结构（core/src/layer1_parser/ ~ layer10_interop/）完全一致。

### 2.2 正典层表
| 层级 | 英文名 | 中文名 | 目录 | 职责 |
|:----:|--------|--------|------|------|
| Shared | Shared | 公共基础层 | core/src/shared/ | 错误码、基础类型、内存管理、日志、诊断 |
| L1 | Parser | 输入解析层 | core/src/layer1_parser/ | 词法分析、公式解析、DSL编译 |
| L2 | Resource | 资源管理层 | core/src/layer2_resource/ | 内存池、缓存、上下文、调试、运行时监控 |
| L3 | Geometry | 几何拓扑层 | core/src/layer3_geometry/ | 约束图、符号坐标、几何原语、WFC范式 |
| L4 | Reasoning | 公理推理层 | core/src/layer4_reasoning/ | 证明引擎、Groebner、SMT/SAT、55+预设 |
| L5 | Output | 结果输出层 | core/src/layer5_output/ | 流事件、TikZ/Lean导出、可视化 |
| L6 | Visual | 图形化编程层 | core/src/layer6_visual/ | 节点图编辑器、几何画布、块调度 |
| L7 | Orchestration | 编排调度层 | core/src/layer7_orchestration/ | 六阶段流水线编排 |
| L8 | Meta-Verify | 元验证层 | core/src/layer8_meta_verify/ | 类型一致性、完备性、可靠性检查 |
| L9 | Application | 应用入口层 | core/src/layer9_application/ | 批处理、交互式REPL |
| L10 | Interop | 外部集成层 | core/src/layer10_interop/ | Lean4/Coq/OPML桥接 |

### 2.3 依赖关系
| 层级 | 可依赖项 |
|:----:|----------|
| L1 | L2 |
| L2 | 无（基础层） |
| L3 | L2 |
| L4 | L2, L3 |
| L5 | L2, L3, L4 |
| L6 | L2, L3, L5 |
| L7 | L2-L6 |
| L8 | L2, L3, L4 |
| L9 | 所有层 |
| L10 | L2, L4, L5 |

### 2.4 与其他层定义的关系
| 来源 | 状态 | 处理方式 |
|------|------|----------|
| ctx.h (5层枚举) | 代码实际 | 保留（向后兼容），添加 L6-L10 枚举 |
| layer_validation.h (6层枚举) | 代码实际 | 保留，扩展到10层 |
| TEN_LAYER_INTEGRATION_PLAN.md (10层，L2/L3名称不同) | 文档 | 标记为[已被取代]，L2/L3名称以正典为准 |
| TEN_LAYER_OPTIMIZED_PLAN.md (L6-L10为独立目录) | 迁移提案 | 标记为[迁移提案]，降级为v6.0规划参考 |

---

## 3. 冲突总表

### 3.1 关键冲突（P0 - 必须立即修复）

| ID | 冲突 | 涉及文件 | 严重度 | 解决动作 |
|----|------|----------|--------|----------|
| C01 | 大量文档缺失但被引用 | README.md, CONTRIBUTING.md, SECURITY.md | 🔴关键 | 创建缺失文档或移除引用（见§8逐文档清单） |
| C02 | 层命名三重不一致 | ctx.h, layer_validation.h, TEN_LAYER_INTEGRATION_PLAN.md | 🔴关键 | 以ctx.h的L2=Resource/L3=Geometry为正典 |
| C03 | 版本号断裂 | README(v5.0.0) vs CHANGELOG(v3.5.0) | 🔴关键 | 补全CHANGELOG v4.0.0和v5.0.0条目 |
| C04 | TEN_LAYER_OPTIMIZED_PLAN定义与正典冲突 | TEN_LAYER_OPTIMIZED_PLAN.md | 🔴关键 | 标记为[迁移提案]，明确其L6-L10为未来方案 |

### 3.2 高优先级冲突（P1）

| ID | 冲突 | 涉及文件 | 严重度 | 解决动作 |
|----|------|----------|--------|----------|
| C05 | 分支策略不一致：develop vs dev | BRANCHING_STRATEGY.md, COMMIT_CONVENTION.md | 🟠高 | 以BRANCHING_STRATEGY的develop为准 |
| C06 | COMMIT_CONVENTION scope仅覆盖L1-L4 | COMMIT_CONVENTION.md | 🟠高 | 补充L5-L10的scope定义 |
| C05b | 分支命名：dev vs develop | COMMIT_CONVENTION, BRANCHING_STRATEGY | 🟠高 | 统一为develop |
| C07 | Engine/Context状态机重复 | ctx.h, context.h | 🟠高 | 保留双状态机但添加迁移注释，标记为过渡期 |
| C08 | OPML未纳入主导出枚举 | interop.h, opml_codec.c | 🟠高 | 将OPML添加到InteropExportFormat枚举 |
| C09 | Magic模块层级放置不当 | magic.h | 🟠高 | 保留当前位置，添加注释说明其应用层属性 |
| C10 | meta_repr.h无对应源文件 | meta_repr.h | 🟠高 | 确认实现状态，如未实现则标记为[设计阶段] |
| C11 | SECURITY.md版本支持表过时 | SECURITY.md | 🟠高 | 更新到v5.0.0 |

### 3.3 中优先级冲突（P2）

| ID | 冲突 | 涉及文件 | 严重度 | 解决动作 |
|----|------|----------|--------|----------|
| C12 | CONTRIBUTING.md仅列5层 | CONTRIBUTING.md | 🟡中 | 更新为10层 |
| C13 | SECURITY.md引用不存在文件 | SECURITY.md | 🟡中 | 更新引用到实际文件 |
| C14 | CONTRIBUTING.md引用不存在文件 | CONTRIBUTING.md | 🟡中 | 更新引用到实际文件 |
| C15 | README.md引用不存在文件 | README.md | 🟡中 | 创建缺失文档或更新引用 |
| C16 | Python版本不一致(3.8 vs 3.10) | CONTRIBUTING.md | 🟡中 | 统一为>=3.10 |
| C17 | TEN_LAYER_INTEGRATION_PLAN无取代标记 | TEN_LAYER_INTEGRATION_PLAN.md | 🟡中 | 添加[已被取代]标记 |
| C18 | 13个最小原语和差分测试框架未文档化 | bootstrap_test.c/h | 🟡中 | 创建设计文档或在TEN_LAYER_OPTIMIZED_PLAN中补充 |

### 3.4 低优先级冲突（P3）

| ID | 冲突 | 涉及文件 | 严重度 | 解决动作 |
|----|------|----------|--------|----------|
| C19 | WFC范式无独立设计文档 | propagation.h, meta_proof.h | 🟢低 | 代码定义一致，可选择性创建文档 |
| C20 | 编译标志lv_NO_FLOAT等不存在 | （之前文档描述） | 🟢低 | 从文档中移除不存在的标志引用 |
| C21 | README文档链接列表需更新 | README.md | 🟢低 | 移除失效链接或创建对应文档 |

---

## 4. 缺失文档清单

以下文档被多处引用但实际不存在，需要决策处理：

| 文档路径 | 引用者 | 建议处理 |
|----------|--------|----------|
| doc/DOCUMENTATION.md | README.md | 创建精简版（从VERSION_5.0.0+README整合） |
| doc/docs/API_QUICKSTART.md | README.md | 创建（基于interop.h和ctx.h） |
| doc/docs/TUTORIAL.md | README.md | 创建（基于README示例扩展） |
| doc/docs/ARCHITECTURE_MANUAL.md | README.md | 创建（基于VERSION_5.0.0+正典层表） |
| doc/docs/CODING_STANDARD.md | CONTRIBUTING.md | 创建或移除引用 |
| doc/docs/CODING_STANDARD_v3.4.2.md | README.md | 移除引用（统一为CODING_STANDARD.md） |
| doc/docs/NAMING_CONVENTION.md | CONTRIBUTING.md | 创建或移除引用 |
| doc/docs/ARCHITECTURE_v3.3.md | SECURITY.md, CONTRIBUTING.md | 移除引用（已过时） |
| doc/docs/lv_LANGUAGE_SPEC.md | README.md | 创建（基于lexer.h/parser.h） |
| doc/docs/INFERENCE_STRATEGIES_SPEC.md | （之前文档引用） | 创建（基于reasoning引擎代码） |
| doc/docs/PERFORMANCE_OPTIMIZATION.md | （之前文档引用） | 创建（基于config.h+性能相关代码） |
| doc/docs/OPML_SPECIFICATION.md | （之前文档引用） | 创建（基于opml_codec.c） |
| doc/docs/competitive_analysis.md | README.md | 创建或移除引用 |

---

## 5. 命名统一规范

### 5.1 层目录命名
以 **ctx.h** 中的枚举名称为准：
- L2: `layer2_resource/`（资源管理层）
- L3: `layer3_geometry/`（几何拓扑层）

> 注：用户之前选择混合名称(layer2_axiom_resource + layer3_geometry_constraint)，
> 但经验证实际代码使用 layer2_resource/ 和 layer3_geometry/。
> 建议：代码目录保持不变，文档中可注明别名。

### 5.2 API命名规范
以 **README.md** 示例代码为准：
- 上下文创建：`lv_context_create()`
- 上下文销毁：`lv_context_destroy()`
- 点构造：`lv_point(ctx, x, y)`
- 距离计算：`lv_distance(ctx, A, B)`

### 5.3 版本号规范
| 类型 | 当前值 | 格式 |
|------|--------|------|
| DOC_VERSION | 5.0.0 | 文档头部标注 |
| CODE_VERSION | 0.5.0 | CMakeLists.txt + version.h |
| PYTHON_BINDING_VERSION | 0.5.0 | pyproject.toml |

---

## 6. 版本号治理

### 6.1 CHANGELOG.md 补全方案
需要添加的条目：

#### v5.0.0 (2026-06-04)
- 十层架构正式确立（L1-L10）
- 新增 Layer 6-10 实现（Visual/Orchestration/Meta-Verify/Application/Interop）
- Lean 4 形式化验证框架
- OPML 开放证明交换格式
- 55+ 数学理论预设模块
- 可视化编程（四视图同步）
- 元验证五维检查

#### v4.0.0 (需确认日期)
- 从五层架构扩展到六层（新增Visual层）
- layer_validation.h 引入编译时层级检查
- WFC范式约束传播引擎
- meta_proof.h 元证明框架

### 6.2 VERSION_LOG.md 同步更新
与CHANGELOG保持一致。

---

## 7. 文档状态标记规范

| 标记 | 含义 | 使用场景 |
|------|------|----------|
| `[当前]` | 正典文档，活跃维护 | README.md, VERSION_5.0.0.md |
| `[已被取代]` | 已有更新版本替代 | TEN_LAYER_INTEGRATION_PLAN.md |
| `[迁移提案]` | 未来规划，非当前架构 | TEN_LAYER_OPTIMIZED_PLAN.md |
| `[历史文档]` | 保留供参考 | 早期设计文档 |
| `[待更新]` | 内容需要更新以匹配正典 | CONTRIBUTING.md, SECURITY.md, COMMIT_CONVENTION.md |
| `[待创建]` | 引用存在但文件缺失 | 见§4缺失文档清单 |

---

## 8. 逐文档修改清单

### 8.1 README.md [待更新]
- [ ] 移除对不存在文档的引用：CODING_STANDARD_v3.4.2.md
- [ ] 更新文档链接列表，标注[待创建]的文档
- [ ] 确认十层定义与正典一致（已一致）

### 8.2 CONTRIBUTING.md [待更新]
- [ ] 将5层列表扩展为10层
- [ ] 移除对不存在文件的引用：CODING_STANDARD.md, NAMING_CONVENTION.md, ARCHITECTURE_v3.3.md
- [ ] Python版本从>=3.8更新为>=3.10
- [ ] 添加文档状态标记

### 8.3 SECURITY.md [待更新]
- [ ] 版本支持表从v3.3.x更新到v5.0.0
- [ ] 移除对ARCHITECTURE_v3.3.md的引用，改为README.md或VERSION_5.0.0.md
- [ ] 添加文档状态标记

### 8.4 COMMIT_CONVENTION.md [待更新]
- [ ] Scope列表补充L5(output)-L10(interop)
- [ ] 分支策略从main/dev/exp更新为与BRANCHING_STRATEGY.md一致（main/develop/stable+功能分支）
- [ ] 添加文档状态标记

### 8.5 CHANGELOG.md [待更新]
- [ ] 添加v4.0.0条目
- [ ] 添加v5.0.0条目

### 8.6 VERSION_LOG.md [待更新]
- [ ] 与CHANGELOG同步更新

### 8.7 .github/BRANCHING_STRATEGY.md [当前]
- [ ] 确认内容完整（已较完善）
- [ ] 无需重大修改

### 8.8 TEN_LAYER_INTEGRATION_PLAN.md [已被取代]
- [ ] 添加[已被取代]标记和说明
- [ ] 指向正典定义（README.md + VERSION_5.0.0.md）
- [ ] 保留其5条层间规则作为参考

### 8.9 TEN_LAYER_OPTIMIZED_PLAN.md [迁移提案]
- [ ] 添加[迁移提案]标记
- [ ] 明确说明其L6-L10定义是v6.0规划，非当前架构
- [ ] 修正L2/L3命名以匹配正典（或添加说明）

### 8.10 VERSION_5.0.0.md [当前]
- [ ] 确认十层定义与正典一致（已一致）
- [ ] 无需重大修改

---

## 9. 模块归属补全

### 9.1 未明确归属的模块

| 模块 | 当前位置 | 建议归属 | 理由 |
|------|----------|----------|------|
| meta_repr (MetaReprEncoder/Decoder) | core/include/lv/meta_repr.h | L3 (Geometry) | 操作ConstraintGraph和GeomNode |
| magic (Magic系统) | core/include/lv/magic.h | L6 (Visual) | 教育性可视化映射，应用层功能 |
| bootstrap_test (差分测试) | core/src/layer2_resource/bootstrap_test.c | L2 (Resource) | 已在L2目录中，测试基础设施 |
| opml_codec (OPML编解码) | core/src/layer10_interop/opml_codec.c | L10 (Interop) | 已在L10目录中，外部集成 |
| 13个最小原语 | bootstrap_test.c中定义 | L3 (Geometry) | 几何基础操作 |

### 9.2 需确认的模块
以下模块在之前文档中提及但无法验证其存在：
- quantifier_logic, number_theory, inequality_approximation
- gappa_verification, meta_proof_cache, runtime_monitoring
- 如这些模块的代码存在于core/src/某层目录中，以其物理位置确定归属
- 如不存在，则标记为[设计阶段]

---

## 10. 状态机统一

### 10.1 正典状态机（5态）
以 ctx.h 的 lvEngineState 为正典：

```
IDLE → PARSING → REASONING → COMPLETE
                      ↘ ERROR
COMPLETE → IDLE (reset)
ERROR → IDLE (reset)
```

| 状态 | 说明 |
|------|------|
| ENGINE_STATE_IDLE | 空闲，等待输入 |
| ENGINE_STATE_PARSING | 正在解析输入 |
| ENGINE_STATE_REASONING | 正在推理/求解 |
| ENGINE_STATE_COMPLETE | 求解完成 |
| ENGINE_STATE_ERROR | 出错 |

### 10.2 lvContext状态机
与Engine状态机语义对齐（context.h注释已说明）。
标记为"过渡期共存设计"，长期计划合并到Context中。

---

## 11. 执行路线图

### Phase 1: 关键修复（3天）
| 任务 | 文件 | 优先级 |
|------|------|--------|
| 补全CHANGELOG v4.0.0/v5.0.0 | CHANGELOG.md, VERSION_LOG.md | P0 |
| 标记TEN_LAYER_INTEGRATION_PLAN为[已被取代] | TEN_LAYER_INTEGRATION_PLAN.md | P0 |
| 标记TEN_LAYER_OPTIMIZED_PLAN为[迁移提案] | TEN_LAYER_OPTIMIZED_PLAN.md | P0 |
| 移除死链接引用 | CONTRIBUTING.md, SECURITY.md, README.md | P0 |

### Phase 2: 高优先级（3天）
| 任务 | 文件 | 优先级 |
|------|------|--------|
| 更新SECURITY.md版本表 | SECURITY.md | P1 |
| 扩展CONTRIBUTING.md为10层 | CONTRIBUTING.md | P1 |
| 补充COMMIT_CONVENTION scope L5-L10 | COMMIT_CONVENTION.md | P1 |
| 统一分支命名(dev→develop) | COMMIT_CONVENTION.md | P1 |
| OPML添加到导出枚举 | interop.h | P1 |

### Phase 3: 中优先级（2天）
| 任务 | 文件 | 优先级 |
|------|------|--------|
| 创建ARCHITECTURE_MANUAL.md | doc/docs/ | P2 |
| 创建API_QUICKSTART.md | doc/docs/ | P2 |
| 创建TUTORIAL.md | doc/docs/ | P2 |
| 创建lv_LANGUAGE_SPEC.md | doc/docs/ | P2 |
| Python版本统一为3.10 | CONTRIBUTING.md | P2 |

### Phase 4: 低优先级（2天）
| 任务 | 文件 | 优先级 |
|------|------|--------|
| 创建PERFORMANCE_OPTIMIZATION.md | doc/docs/ | P3 |
| 创建OPML_SPECIFICATION.md | doc/docs/ | P3 |
| 创建CODING_STANDARD.md | doc/docs/ | P3 |
| 文档化13个最小原语 | doc/docs/ 或嵌入现有文档 | P3 |
| 最终审查验证 | 全部文档 | P3 |

---

## 12. 验证检查表

### 12.1 自动化验证
- [ ] 所有README.md中的文档链接指向实际存在的文件
- [ ] CHANGELOG.md包含v4.0.0和v5.0.0条目
- [ ] 所有文档引用的文件路径有效
- [ ] ctx.h和layer_validation.h的层枚举一致（或明确标注差异）

### 12.2 人工验证
- [ ] 十层定义在所有文档中一致
- [ ] L2/L3命名在所有文档中一致
- [ ] 分支策略在COMMIT_CONVENTION和BRANCHING_STRATEGY中一致
- [ ] 版本号在所有文档中一致
- [ ] Python版本要求在所有文档中一致

---

## 附录A：实际存在的文档清单

### 项目根目录
- README.md [当前]
- CHANGELOG.md [待更新]
- VERSION_LOG.md [待更新]
- CONTRIBUTING.md [待更新]
- COMMIT_CONVENTION.md [待更新]
- SECURITY.md [待更新]
- LICENSE

### doc/目录
- doc/VERSION_5.0.0.md [当前]

### doc/docs/目录
- doc/docs/TEN_LAYER_INTEGRATION_PLAN.md [已被取代]
- doc/docs/TEN_LAYER_OPTIMIZED_PLAN.md [迁移提案]

### .github/目录
- .github/BRANCHING_STRATEGY.md [当前]

### core/include/lv/ (关键头文件)
- ctx.h (5层枚举 + Engine状态机)
- context.h (Context状态机)
- layer_validation.h (6层枚举)
- interop.h (导出格式枚举，缺OPML)
- propagation.h (WFC数据结构)
- meta_proof.h (元证明框架)
- magic.h (Magic模块)
- meta_repr.h (元表示编码)
- bootstrap_test.h (差分测试框架)

## 附录B：正典引用速查

| 概念 | 正典来源 | 备注 |
|------|----------|------|
| 十层架构定义 | README.md §系统架构 | 与VERSION_5.0.0.md一致 |
| 层目录命名 | core/src/layer*_*/ | ctx.h枚举名 |
| API风格 | README.md §使用示例 | lv_context_create() |
| 状态机 | ctx.h | 5态：IDLE/PARSING/REASONING/COMPLETE/ERROR |
| 分支策略 | .github/BRANCHING_STRATEGY.md | 9种分支类型 |
| 版本号 | README.md(v5.0.0) + CHANGELOG | DOC_VERSION=5.0.0, CODE_VERSION=0.5.0 |
