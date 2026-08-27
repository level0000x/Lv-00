# Lv-00 设计级优化机会（能力规划）

> 状态：侦察报告（2026-08-27）
> 范围：**设计/能力级**规划（像 L9 调度层那样的新设计），不含代码级收敛
> （代码级另见 architecture-opportunities.md）
> 前置：十层架构已冻结（用户明确），L9 调度层已出草案（v0.2）

---

## 0. 十层现状（设计级视角）

| 层 | 职责 | 现状 | 设计级缺口 |
|---|---|---|---|
| L0 | 核心便利层 | lv_impl_upper_* 系列（~1 万行） | 已完备，无需动 |
| L1 | 解析 | lv_parser 等（5393 行） | 已完备 |
| L2 | 资源 | 67 文件（2.2 万行） | 已完备 |
| L3 | 几何 | 153 文件（6.5 万行） | 已完备 |
| L4 | 推理 | 333 文件（12.9 万行） | 子域依赖待审计（代码级） |
| L5 | 输出 | interop/plugin/证明导出（1.7 万行） | 已完备 |
| L6 | 可视化 | block/canvas/converter（5220 行） | 已完备 |
| **L7** | **编排** | impl_upper 便捷层（无独立目录） | **缺独立设计** |
| **L8** | **元验证** | layer8_meta_verify/（独立目录） | **验证模型可深化** |
| **L9** | **应用** | 五命令（LOAD/VERIFY/BATCH/EXPORT/VISUALIZE） | 调度层已规划，衔接待设计 |
| L10 | 互操作 | lean4/coq/opml 桥（1427 行） | 已完备 |

**核心洞察**：L7/L8/L9 是"信任链"三层（编排 → 元验证 → 应用），
当前有实现但**无统一设计**。L9 调度层草案引入了"证书"概念，
恰好需要 L7 编排 + L8 验证升级来消费它。

---

## 1. 机会 A：L7 编排层独立设计（流水线编排）

### 现状
- `lv_orchestrator_*` 实现在 `lv_impl_upper_orchestrator.c`（451 行），
  是便捷层封装，无独立 `layer7_orchestration/` 目录。
- 已有 `lvSessionStage` 阶段模型 + `lvSessionConfig` + 超时/深度配置。

### 设计缺口
1. **阶段化流水线不完整**：当前只有固定的 dsl→graph→engine 顺序，
   缺少可组合、可扩展的阶段图（DAG）。
2. **无阶段产物契约**：每阶段输出（tokens/graph/engine 结果）无统一
   接口，难插入验证/缓存/断点续跑。
3. **无故障注入点**：编排层是测试故障恢复的天然位置（阶段失败重跑），
   当前无此能力。

### 设计方向
```
L7 编排层 = 阶段 DAG + 产物契约 + 策略注入

Pipeline = [Parse] → [Build Graph] → [Reason] → [Verify] → [Export]
                     ↓              ↓          ↓
              产物: AST     产物: Graph  产物: Proof
                                        ↓ (可选)
                              [Fault Injection] / [Cache]
```

- **产物契约**：`PipelineArtifact`（type + opaque + destroy），每阶段
  输入/输出统一。
- **阶段可组合**：内置阶段 + 注册式扩展（复用 plugin 机制）。
- **断点续跑**：产物落盘（复用证书格式信封），失败从最近成功阶段续跑。

### 工作量：800-1200 行（编排核心）+ 测试

---

## 2. 机会 B：L8 元验证层深化（六项检查 → 可扩展验证器）

### 现状
- `layer8_meta_verify/meta_verify.c` 独立目录，已有 `lv_meta_verify_session`
  遍历六项检查（lvVerifyCheck 枚举）。
- 已有 BFS 遍历 + 证明对象级验证（`lv_meta_verify_proof`）。

### 设计缺口
1. **六项检查是硬编码表**：新增检查需改枚举 + 表，无注册机制。
2. **无验证器接口**：元验证与 L9 证书验证（编译层验证）未打通——
   L9 需要"编译层独立验证"（lean/coq 编译），L8 当前是图内检查。
3. **无验证报告持久化**：验证结果不落盘，无法支撑证书链。

### 设计方向
```
L8 元验证层 = 注册式验证器 + 两级验证 + 报告持久化

Verifier (注册式, key=check_name)
├── 图内检查（现有六项，迁移为注册条目）
├── 证明级检查（lv_meta_verify_proof）
└── 编译级检查（新增：调 L10 lean/coq 编译证书正文）
      └── 报告 → VerifyReport（可落盘为证书的一部分）
```

- **注册机制**：复用 `lv_registry`（name → verifier fn），新增检查只加条目。
- **编译级验证器**：`meta_verify_compile(shard_cert_path)` 调 lean4_bridge
  编译证书 → 成功/失败。这是 L9 信任链的消费端。
- **报告持久化**：VerifyReport 序列化（复用 canonical 风格）→ 追加到
  证书文件或独立 `.verify` 文件。

### 工作量：600-900 行（注册化改造 + 编译验证器 + 报告落盘）

---

## 3. 机会 C：L9 应用层衔接调度（BATCH 命令 → 集群入口）

### 现状
- L9 应用层五命令中 `LV_APP_CMD_BATCH` 是"批量验证多个输入文件"——
  当前是**串行**遍历（单进程）。
- L9 调度层草案定义了集群分片 + 证书。

### 设计缺口
`BATCH` 命令天然是调度的入口：批量输入 = 分片，可分发到
L9 调度层并行处理 + 证书收集。

### 设计方向
```
LV_APP_CMD_BATCH
    ├── 单机模式（默认）：串行/并行处理（复用 groebner_parallel 思路）
    └── 集群模式（调度）：每个输入 → 分片任务 → L9 调度 → 证书 → 汇总
```

- `BATCH` 命令增加 `mode` 字段（local/cluster），cluster 时走 L9 调度 API。
- 汇总结果 = 全部证书的编译验证结果（L8 编译级验证器消费）。

### 工作量：300-500 行（BATCH 分发 + 汇总）

---

## 4. 机会 D：增量求解架构（几何约束增量验证）

### 现状
- `solver_feedback_solve` 已有"脏变量增量求解"（Solvespace 风格）。
- `solver_sparse_solve` 委托完整求解（稀疏优化候选）。
- 无**图级增量验证**：约束增删后全量重算 vs 增量更新的决策策略缺失。

### 设计缺口
- 大规模几何图（L9 分片场景）每次约束变更全量重算不可行；
- 无"哪些约束影响哪些节点"的依赖追踪（involving_index 已有点→约束
  反向索引，可扩展为完整依赖图）。

### 设计方向
```
增量求解层（可置于 L4 solver 或独立）：
1. 变更集（dirty constraints）→ involving_index 扩展为依赖图
2. 影响域分析（仅受影响子图重算）
3. 增量收敛判定（残差阈值 / 迭代上限）
4. 与 L9 分片协作：单分片内增量，跨分片边界仅传影响摘要
```

### 工作量：1200-1800 行（依赖图 + 增量重算 + 测试）

---

## 5. 机会 E：Python 高层领域 API（绑定之上的 DSL 层）

### 现状
- `module/python/lv/` 4.1 万行，已有 core（ctypes 绑定）+ engine +
  async_stream + dsl_algebra/dsl_context/dsl_wrappers 雏形。
- 38/38 测试通过，绑定层成熟。

### 设计缺口
- dsl_* 是零散包装，无统一"领域 DSL"设计（如
  `with lv.solve(...) as ctx: ctx.point(...).line(...)`）。
- 无批量/分片 Python API（承接 L9：`lv.schedule(shards)`）。

### 设计方向
```
Python 领域层（lv.dsl 包）：
1. 统一上下文（algebra_create 的 Python 化）——已有雏形，规范接口
2. 批量 API：submit(list_of_tasks) → futures（映射 L9 调度）
3. 证书 API：load_cert(path) → VerifyReport（映射 L8）
```

### 工作量：800-1200 行（Python）+ 测试

---

## 6. 优先级与依赖

| 优先级 | 机会 | 工作量 | 依赖 | 价值 |
|---|---|---|---|---|
| P0 | **L8 编译级验证器** | 600-900 | 无（lean4_bridge 已有） | 信任链消费端，L9 前置 |
| P0 | **L7 阶段产物契约** | 800-1200 | 无 | 编排可组合，L9 前置 |
| P1 | L9 应用层 BATCH 衔接 | 300-500 | L8 + L9 调度 | 集群入口 |
| P1 | 增量求解架构 | 1200-1800 | 无 | 大规模性能 |
| P2 | Python 领域层 | 800-1200 | L8/L9 API | 用户面 |

**信任链全景（A+B+C+L9 衔接）**：
```
输入 → L7 编排（阶段 DAG）→ 求解 → 证明 → L8 验证（注册式+编译级）
                                          ↓
                                 证书（.proof.cert）
                                          ↓
                    L9 调度（分片/并行）→ L9 应用（BATCH 汇总）
```

---

## 7. 不建议设计级优化的

- **L0-L6 各层**：已完备，设计级改动风险高收益低。
- **L10**：三个桥已有明确消费方（证明导出），不动。
- **十层坍缩**：用户已定。
- **增量求解替代 L9**：两者互补（增量是单分片内优化，调度是跨分片），
  非替代。

---

## 8. 执行模型定位与双模式（新增，2026-08-27 追加）

### 8.1 现状定位（已核实）

Lv-00 执行模型是**混合型、主体解释**：

```
DSL 源码 → Tokenizer → AST → [dsl_compile: AST→IR] → [dsl_ir_to_constraint_graph: IR→图]
                                     ↑ 真正的编译        ↑ 解释执行（VTable 分发 30 操作码）
→ 约束求解 → 证明 → [interop_export_lean/coq: 证明→源码]
                    ↑ 源码级编译（到外部证明助手的源语言）
```

- `dsl_compile` 是真实编译（AST→IR，30 操作码 + 符号表 + 源码映射）；
- `dsl_ir_to_constraint_graph` 是**逐条 VTable 分发解释**（非字节码/机器码）；
- `lv_loader.c`（另一套 .lv 路径）AST 遍历直接驱动引擎，更纯解释；
- 证明导出（lean/coq）是**源码级编译**（到外部证明助手），
  是系统最接近"真正编译"的部分。

### 8.2 可优化点（记录，详见 dual-mode-execution-design.md）

1. **IR 落盘/缓存**：现在每次 `dsl_compile_and_load` 都重新
   tokenize→parse→compile。IR 可序列化则编译结果可缓存（对照 .class）。
2. **IR 静态验证**：`validate_ir` 配置已存在，可扩展为编译期检查。
3. **证明导出 = 编译后端**：lean/coq 是 IR 的编译后端，可加更多
   （SMT-LIB / Python sympy / C 代码生成）。
4. **双模式执行**（用户提议）：默认解释 + 可选编译为机器码，
   复用 ga_codegen（6 目标）、lv_dlopen、lv_external_process_run。
   详见 `dual-mode-execution-design.md`。

### 8.3 双模式设计摘要（完整版见 dual-mode-execution-design.md）

- **模式 A 解释**（默认）：现状 VTable 分发，交互/调试/小规模。
- **模式 B 编译**（可选）：IR→C 生成 → 编译 .so/.dll → lv_dlopen 加载；
  收益在**部署/复用**（消除重复解析、分片 worker 预编译），
  非图构造微秒级分发（文档明确边界，避免过度承诺）。
- **AUTO 模式**：缓存命中则编译，否则解释（类比 Java JIT 预热）。
- 工作量：1750-2650 行（含等价性测试），增量 0.5%-0.8%。

### 8.4 与 L9 的关系

- 分片 worker 用模式 B 预编译（免运行时解析）；
- IR→C（图构造机器码）与 Lean/Coq（证明可验证源码）是**两条互补编译路径**：
  - IR→C：怎么构造图 → 性能/部署；
  - Lean/Coq：为什么正确 → 可信。
