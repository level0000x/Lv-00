# Lv-00 架构优化机会盘点（2026-08-27）

> 状态：侦察报告（数据支撑，非决策）
> 方法：代码规模统计 + 重复扫描 + 层分布分析 + 巨型文件/头识别
> 原则：只列**有数据支撑、可执行**的方向；L7-L10 预留层 + L9 调度层已单独规划

---

## 0. 现状基线

| 指标 | 值 | 说明 |
|---|---|---|
| C 源码 | 25.1 万行 | 319k（含头文件） |
| Python 绑定 | 4.1 万行 | 38/38 测试通过 |
| L4 占比 | 12.9 万行（41%） | 最大层，已拆 20 子目录 |
| CMakeLists | 2395 行 | 单体文件 |
| ENABLE_LAYER_VALIDATION | 已启用 | 编译期层验证 |
| 无层标注文件 | 14 个 | impl_upper 系列（统一便捷层） |
| 巨型公共头 | 7 个（>1000 行） | preset_name_defs.h 3622 行为最 |
| 测试 | 288 CTest + 38 pytest | 零泄漏零失败 |

---

## 1. 代码收敛（最高价值，低风险）

### 1.1 真重复：`remove_square_factors`（已确认）

- `algebraic.c:1225`（`static unsigned int remove_square_factors(unsigned int)`）
- `quadratic.c:404`（`static int remove_square_factors(int)`）
- **同逻辑、双签名**：平方因子移除。收敛到单一实现（如 `lv_remove_square_factors` 放 L3 symbolics 公共位置），两处调用。

### 1.2 命名风格：无前缀 `ensure_capacity`（3 处）

- `geo_halfedge_mesh.c:61` / `smt_theory_combiner.c:67` / `expr_canon.c:268`
- 非真重复（各作用于自己的结构体），但**同名无前缀**易混淆。建议统一前缀
  （`mesh_ensure_capacity` 等）或注释标注结构归属。低优先级。

### 1.3 parse_expr 族（需甄别）

- `fptaylor_eval.c` 2 处 + `gappa_propagate.c` 2 处——各自解析不同 DSL，
  需人工确认是否有可合并的公共 tokenizer。中优先级。

---

## 2. 巨型文件/头（可读性 + 编译时间）

### 2.1 `preset_name_defs.h`（3622 行，902 个 #define）

- 纯宏定义头，占公共头体积最大。可拆为按域分组：
  `preset_name_geometry.h` / `preset_name_algebra.h` / `preset_name_logic.h` 等，
  保留总头聚合。**影响面小（只动 include），收益明确**。

### 2.2 其他巨型公共头

- `lv_utils.h`（1740）/ `proof.h`（1703）/ `constraint_graph.h`（1213）——
  体量大但为高频核心头，拆分收益低、风险高。**不建议动**（除非按功能拆
  proof.h 的 ProofStep vs ProofNavigator 为两个头）。

---

## 3. 层结构（架构一致性）

### 3.1 14 个无层标注文件（impl_upper 系列 + lv.c + convenience）

- 现状：无 layerN 目录，靠 `lv_CURRENT_LAYER` 编译宏受控。
- **建议**：归入 `layer0_core` 或明确标注为"跨层便捷层"（文档级），
  使其在层验证中显式可见而非隐式豁免。低风险、提升可审计性。

### 3.2 L4 内部依赖方向（需专项审计）

- L4 有 20 子目录（backends/func_block/solver/proof_system/type_logic/
  proof/rewrite/engine/model/module/unify/numeric/preset/axiom/dsl/lambda/
  expr/kernel/stream）。41% 的代码集中于此，**子域间依赖方向未验证**。
- **建议**：对 L4 子域做一次依赖图审计（谁 include 谁），识别环或
  逆向依赖，用编译期校验固化（如 ENABLE_LAYER_VALIDATION 的子域级扩展）。

---

## 4. 构建系统

### 4.1 CMakeLists 单体（2395 行）

- 单一文件管理 300+ target。**建议**：按层拆 `cmake/layer4.cmake` 等
  分片 include（文件组织优化，不改构建语义）。中优先级。

### 4.2 构建变体（build3/build_symcheck）

- 已有静态/共享双构建。建议登记标准构建命令到 Makefile 或
  `scripts/build.sh`，消除"build3 是 Ninja、build_symcheck 是共享库"
  的口头约定。低风险。

---

## 5. 测试基础设施

### 5.1 测试文件规模

- 288 个 CTest 已零泄漏零失败，但**测试文件与断言数**未统计。
  建议建立"每公共头最小契约测试覆盖"清单（历史批次已有零覆盖方法论，
  可复用为持续基线）。

### 5.2 故障注入缺失

- L9 调度层草案依赖故障注入（进程崩溃重调度）验证，当前测试无此设施。
  建议新增 `test_fault_injection` 基础（进程 kill 模拟）。与 L9 实施联动。

---

## 6. 新架构机会（承接 L9 草案）

### 6.1 L9 调度层（已规划，3500-4700 行）

- 见 `docs/architecture/L9-scheduler-design.md`（v0.2，含证书格式）。

### 6.2 证书格式已定（.proof.cert）

- 信封 + 三层证明正文（canonical ProofStep / Lean / Coq-OPML），
  原子写 + 版本化 + 哈希校验。见草案 2.3 节。

### 6.3 进程隔离开关（lv_engine_dispatch）

- 默认单进程 FFI + 可选多进程隔离模式（复用 lv_external_process_run）。
  可独立于 L9 先行实现（200-300 行），为插件/脚本隔离提供选项。

---

## 7. 优先级建议（执行顺序）

| 优先级 | 项 | 工作量 | 风险 | 收益 |
|---|---|---|---|---|
| P0 | remove_square_factors 收敛 | ~50 行 | 低 | 消除真重复 |
| P0 | preset_name_defs.h 拆分 | ~100 行（只动 include） | 低 | 头文件体积 |
| P1 | impl_upper 层标注 | ~14 文件注释 | 极低 | 可审计性 |
| P1 | L4 子域依赖审计 | 审计为主 | 中 | 架构一致性 |
| P1 | CMake 分片 | 组织级 | 低 | 可维护性 |
| P2 | lv_engine_dispatch 隔离开关 | 200-300 | 中 | 进程隔离选项 |
| P2 | L9 调度层 | 3500-4700 | 中 | 集群能力 |
| P3 | parse_expr 收敛（甄别后） | 待定 | 中 | 代码收敛 |
| P3 | 故障注入测试设施 | 300-500 | 低 | 测试能力 |

---

## 8. 不建议做的（避免过度优化）

- **拆分 proof.h / constraint_graph.h**：核心头，收益低风险高。
- **合并数值后端 serial_***：同一文件条件编译变体，非重复。
- **L7-L10 层坍缩**：用户已明确十层为扩展预留。
- **无前缀 ensure_capacity 大规模改名**：仅 3 处，命名改进即可。
