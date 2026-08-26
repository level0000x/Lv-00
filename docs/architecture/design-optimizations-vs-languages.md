# Lv-00 已实现代码设计优化点（对照其他编程语言范式）

> 状态：研究分析（2026-08-27）
> 方法：对照主流语言（Rust 所有权/错误、Zig 编译期/分配器、
>       Go 并发、函数式不可变、Smalltalk 消息）审视 Lv-00 已实现代码
> 原则：只列**已实现代码中可落地的设计改进**，不做推翻式重构

---

## 0. Lv-00 已具备的现代设计（对照确认，不重复）

| 设计面 | Lv-00 现状 | 对标语言 |
|---|---|---|
| 错误链 | `lvErrorFrame` 栈 + `lv_ERROR_CTX_WRAP`（cause 链） | Rust `std::error::Error` source |
| 所有权（Python 侧） | `_PtrOwner` 混入 + PEP 442 关闭安全 | Rust 所有权 + Drop |
| 注册表模式 | `lv_registry`（通用 name→fn/value） | Go 插件、Java ServiceLoader |
| 结果返回 | `lv_RETURN_ERROR` 宏族 + 错误码 | Rust `Result`、Zig 错误联合 |
| 一次性初始化 | `lv_once` + `lv_once_reset` | Go sync.Once、Rust OnceLock |
| 类型表（X-macro） | `LV_GEOM_TYPE_X` 等单源宏 | Rust enum + match 穷尽 |
| 池化 | `lv_pool`（预设对象池） | Rust 对象池 crate、C++ pool |
| 契约测试 | test_framework + 零覆盖方法论 | 语言无关（方法论成熟） |

**结论**：Lv-00 的核心基础设施已吸收多数现代语言范式。
以下为**仍有差距、可优化**的设计面。

---

## 1. 内存所有权（对照 Rust）

### 1.1 现状

- C 侧：`graph_add_point` 深拷贝 / `module_set_graph` 接管 / `_PtrOwner` 释放
  ——**约定散落在各头文件注释**，无统一文档。
- 批次 117 修复了 9 处测试泄漏，说明"谁拥有"易错。

### 1.2 设计优化：所有权约定文档 + 注释前缀

对标 Rust 的命名约定，为 C API 建立**所有权三态标注**：

| 后缀/前缀 | 含义 | 示例 |
|---|---|---|
| `_copy` | 调用方保留所有权，返回新对象 | `symbolic_coord_copy` |
| `_take` / `_move` | 接管所有权 | `module_set_graph`（接管） |
| `_borrow` / 无后缀 | 借用，调用方不得释放 | `graph_get_node`（借用） |

**落地**：
1. 写 `docs/architecture/memory-ownership.md` 约定文档（三态 + 示例 + 反例）。
2. 公共头文件关键 API 注释加 `[copy]` / `[take]` / `[borrow]` 标注
   （grep 可审计）。
3. 新增静态检查（可选）：脚本扫描 `graph_add_*` / `*_copy` / `*_destroy`
   的配对，作为 CI 测试外的补充（低优先）。

**工作量**：文档 200-400 行 + 注释标注（跨 50+ 头，机械）。

---

## 2. 错误处理（对照 Rust Result / Zig 错误联合）

### 2.1 现状

- `lv_RETURN_ERROR` 宏族：返回 `-1/NULL/false` + 设置全局/上下文错误。
- **问题**：错误码与返回值分离，调用方常忽略错误码（只看返回值）。
- `lv_error_code(ctx)` 可查，但**无编译期强制**。

### 2.2 设计优化：错误传播分层

对照 Rust 的 `?` 操作符（自动传播），C 侧无法实现编译期传播，
但可建立**分层约定**：

```
层 1（基础）：错误码 + lv_ERROR_CTX_RETURN（现状，保留）
层 2（强制检查）：新增 lv_TRY 宏（goto cleanup + 检查返回值），
                 对标 Rust ?——要求调用方显式处理或传播
层 3（审计）：脚本扫描 `lv_RETURN_ERROR` 调用点，
             统计"返回值被检查 vs 被忽略"的比例，逐步收敛
```

**落地**：优先做层 3（审计，零改动），识别错误被忽略的热点
（如 `graph_add_point` 返回值被丢弃处），再决定是否引入 lv_TRY。

**工作量**：审计脚本 100-200 行；lv_TRY 若做则 200-300 行 + 迁移。

---

## 3. 不可变数据结构（对照函数式 / Rust）

### 3.1 现状

- `ConstraintGraph` / `GeomNode` / `ProofStep` 均为**可变结构体**，
  修改就地发生（`graph_add_point` 改图）。
- 有 `graph_copy` / `func_block_copy`（深拷贝），但**无版本化/持久化
  数据结构**（copy-on-write）。

### 3.2 设计优化：图快照（对照持久化数据结构）

Lv-00 已有 `graph_snapshot_*`（rewrite_snapshot.c，事务回滚用），
但为**全量深拷贝**。对标 Clojure 持久化向量 / Rust `im` crate，可做：

- **增量快照**：快照只记录变更（dirty 节点/约束 diff），
  restore 时应用 diff。复用 `graph_snapshot` 接口，内部改 diff 存储。
- **适用场景**：undo/redo 链（L6 可视化）、增量求解（design-level
  opportunities 的机会 D）、L9 分片输入、rewrite 事务（现存，可减开销）。

**落地**：先做 `graph_diff`（两图差异，可复用 `meta_repr_graph_equivalent`
 思路），再在 snapshot 层用 diff 替代全量拷贝。

**工作量**：graph_diff 400-600 行；snapshot 改造 300-500 行。

---

## 4. 编译期生成（对照 Zig comptime / Rust macro）

### 4.1 现状

- 已有 X-macro（`LV_GEOM_TYPE_X` 等）生成枚举↔字符串映射。
- `preset_name_defs.h`（3622 行）是 902 个 `#define`——**纯手工维护**，
  无生成器。

### 4.2 设计优化：代码生成脚本

对照 Zig 的 `comptime` 与 Rust 的 `macro_rules`，将**数据驱动的
重复声明**改为脚本生成：

- `preset_name_defs.h` ← 由 `tools/gen_preset_names.py` 从单一数据源
  （JSON/表格）生成。
- 好处：新增预设只改数据源；生成物提交进仓库（确定性）；
  与现有 X-macro 哲学一致。

**落地**：写生成器（Python 200-300 行）+ 数据源 + CI 校验生成物一致
  （防手改漂移）。

**工作量**：300-500 行（生成器 + CI 步骤）。

---

## 5. 并发模型（对照 Go goroutine / Erlang actor）

### 5.1 现状

- `thread_pool.c`（L2）+ `groebner_parallel`（L4）+ 流式回调。
- **无任务级并发抽象**：并行求解是特定实现，无统一"提交任务 →
  取结果"的 API。
- 调度器（engine_scheduler）是**单线程**路由，非并发队列。

### 5.2 设计优化：任务并发抽象（对标 Go worker pool）

```
lv_task_pool（新增，L2）：
  submit(fn, arg, on_done_cb) → task_id
  wait(task_id) / wait_all()
  内部复用 thread_pool，但暴露"任务"级 API（可取消/超时/依赖）
```

- **与 L9 调度层关系**：L9 是**进程级**调度；lv_task_pool 是
  **线程级**任务抽象。两者分层：L9 worker 内部可用 lv_task_pool
  并行处理多个证书验证。
- **复用**：现有 thread_pool + stream 事件，封装为任务 API。

**工作量**：500-800 行（任务池 API + 测试）。

---

## 6. 消息传递（对照 Erlang actor / Go channel）

### 6.1 现状

- `interop_server` 是 stdio/WS 消息（进程间）。
- 流式事件（stream_*）是发布-订阅（回调）。
- **无进程内 actor/消息队列**：模块间通信靠直接函数调用 +
  全局状态（graph_stream_ctx 等）。

### 6.2 设计优化：模块事件总线深化

- 已有 `lv_event_bus`（L2，event_bus.c）——检查覆盖度。
- 对标 actor：为 L4 子域（solver/proof/rewrite）定义**域事件**，
  经 event_bus 发布，解耦模块直接调用。
- 收益：L4 子域依赖审计（架构机会）可借事件总线消除部分逆向依赖。

**落地**：先审计 event_bus 现有订阅者，再为高频跨域点
  （solver→proof 结果通知）接入事件。

**工作量**：审计 100 行 + 接入 200-400 行。

---

## 7. 元编程 / DSL（对照 Lisp 宏 / Rust 过程宏）

### 7.1 现状

- L1 有完整 DSL（lv_parser / dsl_compiler）。
- **无宏系统**：DSL 用户不能定义新语法。

### 7.2 设计优化：DSL 宏（模板展开）

- `func_block` / `axiom` 已有模板/预设机制——可作为宏基础。
- 对标 Lisp 宏：允许用户 `define_macro name(args) = <template>`，
  解析期展开。
- 复用 `preset_manager_compose`（组合预设）思想。

**落地**：DSL 宏 = 预设组合的用户界面（用现有模板引擎），
  新增语法解析 + 展开器。

**工作量**：600-1000 行（解析 + 展开 + 测试）。

---

## 8. 可观察性（对照 OpenTelemetry / Erlang 可观测）

### 8.1 现状

- `runtime_monitor` / `performance_profiler` / `stream_*` 已有。
- **无分布式追踪**：L9 分片任务的跨进程链路无 trace-id。

### 8.2 设计优化：追踪上下文

- 信封头加 `trace-id`（L9 证书已有 task-id，扩展为 trace-id）。
- 主控/worker 日志带 trace-id，跨进程关联（对照 OpenTelemetry W3C）。
- 复用 stream 事件携带 trace-id。

**落地**：L9 协议消息加 trace-id 字段 + 日志前缀。

**工作量**：100-200 行（与 L9 实施合并）。

---

## 9. 优先级汇总

| 优先级 | 项 | 工作量 | 风险 | 收益 |
|---|---|---|---|---|
| P0 | 内存所有权文档（三态标注） | 200-400 | 极低 | 防泄漏（对照 Rust） |
| P0 | 错误忽略审计脚本 | 100-200 | 极低 | 发现隐患（对照 Rust ?） |
| P1 | 图 diff + 增量快照 | 700-1100 | 中 | undo/增量/L9（对照 Clojure） |
| P1 | 任务并发抽象 lv_task_pool | 500-800 | 中 | L9 证书并行验证（对照 Go） |
| P2 | preset_name_defs 生成器 | 300-500 | 低 | 消除手工宏（对照 Zig） |
| P2 | DSL 宏 | 600-1000 | 中 | 用户扩展（对照 Lisp） |
| P2 | 事件总线深化 | 300-500 | 中 | 解耦 L4（对照 actor） |
| P3 | 追踪上下文 | 100-200 | 低 | 跨进程可观测（对照 OTel） |

---

## 10. 不建议做的

- **C 侧引入完整所有权编译器检查**：C 无此能力，文档+审计足够。
- **全量改错误返回为 lv_TRY**：风险高，先审计再定。
- **持久化数据结构全面替换**：只在快照/增量场景引入，不推翻
  现有可变图。
- **actor 全面改造**：只做事件总线深化，不引入 actor 运行时。
