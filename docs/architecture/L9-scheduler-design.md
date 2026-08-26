# L9 调度层架构设计草案（可信分片调度）

> 状态：草案 v1.0（2026-08-27）
> 来源：《进程相关思考.txt》设计思路提炼 + Lv-00 现有设施盘点
> 决策原则：**能隔离的绝不共享，能落盘的绝不传输，能重算的绝不恢复。**
> v1.0 新增：调度器状态机 / 分片协议 / 证书验证流程 / 一致性模型 / 安全模型

---

## 0. 与现有代码的关系（复用优先，不重复造轮子）

L9 调度层**不是从零实现**。盘点确认以下设施已存在且可直接复用或小幅改造：

| 现有设施 | 位置 | 行数 | 复用方式 |
|---|---|---|---|
| `lv_external_process_run` | L2 lv_process.c | 609 | 子进程 spawn + 输出捕获 + 退出码（进程隔离基础） |
| `interop_server` / `interop_server_ws` | L5 interop | 1926 | stdio/WebSocket 通信（进程协议层） |
| `engine_scheduler` | L4 engine | 1229 | 后端选择/路由规则（分片调度内核借鉴其规则分发思想，队列需自建） |
| `groebner_parallel` | L4 backends | 1096 | 并行求解（算毕即终止的现成范式） |
| `proof_compiler` | L5 | 843 | 证明编译验证（证书消费端） |
| `lean4_bridge` / `coq_bridge` / `opml_codec` | L10 | 1427 | 证明导出（证书文件格式） |
| `stream_*` 系列 | L2 stream | 1495 | 事件流（进度/错误上报） |
| `interop_export_canonical` | L5 | 183 | 规范文本格式（分片输入/输出编解码） |

**结论**：L9 调度层增量约 **2500-4000 行**，不是 10 万行级工程。

---

## 1. 三种进程策略（按调用方身份）

### 1.1 调用层（Python/脚本 → C 引擎）

```
┌─────────────┐   FFI 直调（默认）   ┌─────────────┐
│  Python 侧   │ ──────────────────→ │  C 引擎      │
│  (ctypes)    │ ←────────────────── │  (L3-L5)    │
└─────────────┘   零序列化、零延迟    └─────────────┘
```

- **默认：单进程 FFI 直调**。理由：批次 118 已证明 ctypes 绑定成熟（Windows 本地 38/38），绑定层维护成本已支付，无需为"免绑定维护"而默认多进程。
- **可选：多进程隔离模式**（`LV_PROC_MODE_ISOLATED` 环境变量或 API 开关）。用于：
  - 嵌入不可信/易崩的语言插件时隔离故障域；
  - 长时间运行的批处理任务（崩溃不带走宿主）；
  - 通过 `lv_external_process_run` + stdio 协议（复用 interop_server 的 stdio 接口）实现。
- 切换点收敛为**单一入口**：`lv_engine_dispatch()`（新增），内部按模式走 FFI 或进程通道。

### 1.2 集群部署层（主控 → 分片 → 计算进程）

```
                 ┌─────────────┐   分片 A ──→ 计算进程 A ──→ 证书 A（落盘）
主控节点 ───────→ │  L9 调度器   │   分片 B ──→ 计算进程 B ──→ 证书 B（落盘）
  (数据分片)      └─────────────┘   分片 C ──→ 计算进程 C ──→ 证书 C（落盘）
                      │
                      └── 全部证书 → 证明编译器（L5/L10）→ 最终结论
```

- 分片依据：现有空间索引（`fast_index` / `geo_aabb_tree`）的网格单元 → 天然分片键。
- 子进程：无状态、只处理自己分片、算毕即终止（复用 groebner_parallel 的并行求解内核，改为进程级）。
- 故障语义：单点崩溃 → 仅重调度对应分片，不影响已验证部分。

---

## 2. 信任传递机制（证书文件 + 编译层验证）

### 2.1 核心不变量

> 最终结论的正确性**不依赖任何单一进程的执行状态**，只依赖所有证明片段在编译层的独立验证。

### 2.2 证书生命周期

```
计算进程                       文件系统                       证明编译器
─────────                     ────────                       ────────
求解分片 → 生成证明片段 ──原子写──→ shard_{id}.proof.cert ──读取──→ 独立编译验证
                    (tmp + rename)                              │
                                    任一片段验证失败 ←──────────┘
                                    重调度对应分片 → 新证书覆盖（版本化）
```

- **证书格式**：直接复用 L10 的导出器输出（lean4_bridge/coq_bridge 生成的证明），或 `interop_export_canonical` 的规范格式包装。
- **原子性与幂等**：
  - 写入用 `tmp + rename`（避免半写文件被消费）；
  - 证书带 `(shard_id, version, content_hash)` 头，编译器按版本读取；
  - 重算产生的同分片证书仅覆盖旧版本，不产生冲突（版本化）。
- **消费端解耦**：编译器（L5 proof_compiler + L10 桥）只读文件，不参与分布式状态维护。

### 2.3 证书文件格式（.proof.cert）

证书 = **信封头 + 证明正文**（文本格式，UTF-8，`\n` 行尾，单文件）。

```
; Lv-00 Proof Certificate
; format-version: 1
; shard-id: 3
; shard-count: 8
; task-id: 6f2c9a1e
; version: 2
; content-hash: sha256:9f7c...（正文的 SHA-256，供编译器快速校验完整性）
; created: 2026-08-27T03:00:00Z
; status: PROVED
; target: shard_3_cert
---BEGIN PROOF---
<证明正文，见下>
---END PROOF---
```

#### 2.3.1 证明正文（复用 ProofStep 序列，三层可选）

**层级 A —— 规范证明步骤（默认，ProofNavigator 直出）**
逐行序列化 ProofStep（`proof.h` 的字段），与 `interop_export_canonical` 同风格：

```
STEP <id> <type> color=<ProofColor> node=<node_id> constraint=<constraint_id> rule=<rule_id> [merged=<a,b,c>] [retained=<id>] deps=<d1,d2,...> [note="..."]
```

示例：
```
STEP 0 add_point color=green node=5 constraint=-1 rule=-1 deps=
STEP 1 add_segment color=green node=7 constraint=-1 rule=-1 deps=0
STEP 2 add_incidence color=green node=-1 constraint=3 rule=-1 deps=0,1
```

**层级 B —— Lean 可编译证明（`---BEGIN PROOF---` 内直接为 Lean 代码）**
复用 `interop_export_lean` 的输出（`have h_node_5 : True := by ...`），编译器直接调 `lean4_bridge` 编译验证。这是"编译检查"的最强形态。

**层级 C —— Coq/OPML 证明**
复用 `interop_export_coq` / `opml_codec` 输出，供 Coq/OPML 消费端。

#### 2.3.2 选择规则

| 消费端 | 证明正文层 | 验证方式 |
|---|---|---|
| Lean 编译器 | B（Lean 代码） | `lean4_bridge` 编译 |
| Coq 编译器 | C（Coq 代码） | coq_bridge 编译 |
| 内部 ProofNavigator | A（规范步骤） | proof_compiler 重放验证 |

> 证书格式与证明正文分层：**信封（元数据）固定，正文（证明）按消费端选择**。
> 同一分片可产出 A/B/C 三层并存（`---BEGIN PROOF-LEAN---` 等多段），
> 或按 `target` 字段只产一层。默认 Lean 层（最强编译检查）。

#### 2.3.3 原子写与版本化

```
shard_3.proof.cert.tmp   ← 计算进程写入（含 content-hash 头）
shard_3.proof.cert       ← rename（原子替换，编译器只读此名）
shard_3.proof.cert.v1    ← 历史版本（可选保留，调试用）
```

- 编译器读取：先校 `content-hash`（与正文 SHA-256 比对）→ 再按层编译验证。
- 哈希不匹配 → 判定证书损坏 → 触发重调度（不信任该进程执行状态）。

---

## 3. 序列化策略

| 阶段 | 格式 | 理由 |
|---|---|---|
| 调试 | 人类可读文本（JSON/canonical） | 便于定位数据问题 |
| 生产 | 紧凑二进制（复用 opml_codec / msgpack 式编码） | 降带宽与解析开销 |
| 证书 | **不走 IPC，直接落盘** | 避免跨进程传输的序列化/反序列化不一致 |

- 分片输入/输出经 `interop_export_canonical` 统一编解码（已有）。
- 证书**只落盘、不传输**：这是"能落盘的绝不传输"的落地。

---

## 4. Lean 层与进程的关系

- **Lean 层（L10）不做任何多进程逻辑**：与 C 引擎单进程 FFI 直调，最低延迟、最大确定性。
- 集群调度、分片管理、进程生命周期 → 全部归 **L9 调度层**（本设计），与证明编译器解耦。
- Lean 层仅作为**最终证明的消费端**：读取证书文件 → 编译验证 → 输出结论。

---

## 5. 总体设计哲学落地

> **能隔离的绝不共享，能落盘的绝不传输，能重算的绝不恢复。**

1. **隔离性**：非核心语言调用（插件/脚本）进程隔离，限制故障域（复用 lv_external_process_run）。
2. **持久化**：关键证明结果以证书文件落地（原子写 + 版本化），利用文件系统稳定性 + 编译器增量机制，取代不可靠的 IPC。
3. **无状态与重算优先**：所有计算进程无状态，失败即重调度，无状态回滚/恢复逻辑。

---

## 6. L9 调度层模块划分（增量实现清单）

```
core/src/layer9_scheduler/
├── scheduler_core.c      # 分片任务队列 + 调度内核（复用 engine_scheduler 思想）
├── scheduler_shard.c     # 空间索引 → 分片键 → 分片输入序列化（复用 fast_index/canonical）
├── scheduler_worker.c    # 子进程生命周期管理（复用 lv_external_process_run）
├── scheduler_cert.c      # 证书文件原子写 + 版本化 + 读取（新增，~400 行）
├── scheduler_verify.c    # 证书编译验证编排（复用 proof_compiler + L10 桥）
├── scheduler_proto.c     # 进程协议编解码（复用 interop_server stdio + canonical）
└── scheduler_public.h    # 公共 API：lv_sched_dispatch / lv_sched_submit / lv_sched_wait
```

---

## 7. 工作量预估

### 7.1 复用现状（不计入新增）

| 现有设施 | 行数 | 状态 |
|---|---|---|
| lv_process.c | 609 | 可直接复用 |
| interop_server + ws | 1926 | 可直接复用（stdio 协议） |
| engine_scheduler | 1229 | 改造复用（分片版） |
| groebner_parallel | 1096 | 并行内核复用 |
| proof_compiler + L10 桥 | 2270 | 证书消费端复用 |
| stream 系列 | 1495 | 事件流复用 |
| canonical 编解码 | 183 | 分片 I/O 复用 |
| **小计复用** | **~8800** | 不重写 |

### 7.2 新增/改造工作量

| 模块 | 预估行数 | 说明 |
|---|---|---|
| scheduler_core（分片调度内核） | 800-1200 | 队列 + 规则分发借鉴 engine_scheduler 思想，任务队列自建 |
| scheduler_shard（分片划分） | 300-500 | 复用 fast_index 网格 |
| scheduler_worker（进程管理） | 300-400 | lv_external_process_run 封装 |
| scheduler_cert（证书原子写/版本化/格式编解码） | 400-600 | **全新**（信封 + 三层证明正文） |
| scheduler_verify（验证编排） | 200-300 | proof_compiler 封装 |
| scheduler_proto（进程协议） | 300-500 | interop_server 复用 + 证书旁路 |
| 公共头文件 + API | 200-300 | 新增 |
| 单进程隔离模式开关（lv_engine_dispatch） | 200-300 | 现有引擎入口改造 |
| 测试（调度/证书/故障注入） | 800-1200 | 新增契约测试 |
| **新增小计** | **~3400-4700** | |

### 7.3 结论

- **新增约 3500-4700 行**（调度层本体 + 测试）。
- **重写约 0 行**（现有 8800 行设施全部复用，无废弃）。
- 对 31.9 万行 C 代码库，L9 调度层增量约 **1.1%-1.5%**。

### 7.4 风险与缓解

| 风险 | 缓解 |
|---|---|
| 多进程序列化开销 | 默认单进程 FFI，隔离模式按需开启 |
| 证书文件并发写冲突 | 原子写（tmp+rename）+ 分片版本化 |
| 分片负载不均 | 空间索引网格 + 动态重分片（按 fast_index 中位数启发） |
| 证明片段语义一致性 | 编译层独立验证 + 版本化覆盖 |

---

## 8. 与现有十层架构的关系

- L9 为**预留层**（用户明确十层为扩展预留，不做坍缩）——本设计恰好填充 L9 的职责。
- L10（lean4/coq/opml 桥）保持纯消费端，不引入分布式逻辑（符合第四节约束）。
- L5 interop 的 server 设施上移到 L9 复用，L5 自身不动。

---

## 9. 下一步建议

1. 先实现 `scheduler_cert`（证书原子写 + 版本化）——最小可信闭环，可独立测试。
2. 再实现 `scheduler_core`（分片调度）——队列 + 规则分发借鉴 engine_scheduler，单机验证。
3. 最后接 `scheduler_worker`（进程隔离）+ 单进程/多进程双模式开关。
4. 每步加契约测试（调度正确性 / 证书原子性 / 故障注入重调度）。

---

## 10. 调度器状态机（v1.0 深化）

### 10.1 分片任务生命周期

```
                    ┌──────────────────────────────────────────┐
                    │            ShardTask 状态机                │
                    └──────────────────────────────────────────┘
  SUBMITTED ──→ SCHEDULED ──→ DISPATCHED ──→ RUNNING ──→ VERIFYING ──→ DONE
      │             │             │             │           │          │
      │             │             │             │           │          │
      │             └──失败───────┘             │           │          │
      │             (调度资源不足)               │           │          │
      │             └───────────────────────────┼──崩溃──────┘          │
      │                                         │      (退出码非0/超时)  │
      │                                         ↓                       │
      │                                    RESCHEDULING ──→ SCHEDULED    │
      │                                         │                        │
      │                                         └──(重试次数超限)──→ FAILED
      └──────────────────────────────────────────────────────────────┘
                     (取消)──→ CANCELLED
```

| 状态 | 含义 | 进入条件 | 出口条件 |
|---|---|---|---|
| SUBMITTED | 已提交，未调度 | `lv_sched_submit` | 资源可用 → SCHEDULED |
| SCHEDULED | 已入队 | 资源确认 | worker 领取 → DISPATCHED |
| DISPATCHED | 已派发 | worker 进程 spawn 成功 | 进程存活 → RUNNING |
| RUNNING | 计算中 | 收到 worker 心跳/开始信号 | 证书落盘 → VERIFYING；崩溃/超时 → RESCHEDULING |
| VERIFYING | 证书编译验证中 | RUNNING 完成 | 验证通过 → DONE；失败 → RESCHEDULING |
| RESCHEDULING | 重试中 | 崩溃/验证失败 | 重试次数 < max → SCHEDULED；≥ max → FAILED |
| DONE / FAILED / CANCELLED | 终态 | — | — |

### 10.2 重试策略

- **重试上限**：`max_retries`（默认 2，可配）。
- **退避**：指数退避 `delay = base * 2^attempt`（base=100ms，封顶 5s），
  避免故障风暴下同时重试。
- **只重试该分片**：已验证通过的分片证书不动，仅未通过的重算
  （与设计哲学一致：能重算的绝不恢复）。
- **毒丸任务**：连续失败超过 max_retries → FAILED，记录错误，不阻塞其他分片。

---

## 11. 分片进程协议（v1.0 深化）

### 11.1 消息格式（stdio，行分隔 JSON，复用 interop_server stdio）

```
主控 → worker:   {"type":"shard.task","shard_id":3,"input":"<canonical>","timeout_ms":5000}
worker → 主控:  {"type":"shard.ack","shard_id":3}
worker → 主控:  {"type":"shard.progress","shard_id":3,"pct":42}
worker → 主控:  {"type":"shard.cert","shard_id":3,"cert_path":"shard_3.proof.cert","version":2}
worker → 主控:  {"type":"shard.error","shard_id":3,"code":"OUT_OF_MEMORY","msg":"..."}
```

### 11.2 协议不变量

1. **证书不随消息传**：`shard.cert` 只带路径，正文走文件系统（能落盘的绝不传输）。
2. **ack 后超时判定**：主控发出 task 后启动计时器；ack 前超时 → 视 worker 未启动，
  直接重调度（无需等崩溃信号）。
3. **cert 消息是提交点**：主控收到 `shard.cert` 即转 VERIFYING（读文件验证），
  不依赖 worker 后续存活。
4. **心跳可选**：长任务可发 progress 防主控误判超时；无心跳任务按
   `timeout_ms` 硬上限。

### 11.3 幂等重放

- worker 幂等：同一 `shard_id` 的 task 重复投递，worker 只处理一次
  （证书文件 version 相同则跳过重算——先查 `shard_{id}.proof.cert` 是否
  已存在且 hash 匹配）。
- 主控幂等：重调度只递增 version，不改变分片内容（分片输入由
  `shard_id` 确定性派生）。

---

## 12. 证书验证流程（v1.0 深化）

```
主控 VERIFYING 状态:
  1. 读证书 → 校验信封（format-version / shard-id / content-hash）
  2. content-hash vs 正文 SHA-256 比对 → 不匹配 → 证书损坏 → RESCHEDULING
  3. 按 target 层分发验证:
       target=lean  → lean4_bridge 编译证书正文（L10）
       target=coq   → coq_bridge 编译
       target=canon → proof_compiler 重放（L5）
  4. 编译通过 → 证书有效 → 登记 VerifyReport → DONE
  5. 编译失败 → 记录错误 → RESCHEDULING
```

- **验证是并行的**：多个分片证书可同时编译验证（各证书独立）。
- **验证结果落盘**：`VerifyReport`（复用 L8 深化设计的报告格式）
  → `shard_{id}.verify` 文件，供审计与 L9 应用层汇总。
- **最终结论**：全部 DONE 分片的 VerifyReport 汇总 → 整体结论
  （任何 FAILED → 整体失败，但已 DONE 部分可复用）。

---

## 13. 一致性模型（v1.0 深化）

### 13.1 单调性

- **证书版本单调递增**：同一分片新证书 version > 旧证书。
  编译器/主控只接受 `version >= 当前`。
- **结论单调**：一旦 DONE 即终态，重算不推翻已 DONE 分片
  （除非显式全局重验，version 全局 +1）。

### 13.2 最终一致性

- 无分布式锁：证书文件即"提交记录"。主控崩溃重启后，
  扫描目录恢复各分片状态（存在且验证过的证书 → DONE）。
- 恢复无需日志：`shard_{id}.proof.cert` 的存在性 + VerifyReport 即状态
  （能落盘的绝不传输 → 文件系统即状态存储）。

### 13.3 冲突

- 同一分片并发重算（主控崩溃前已派发 + 重启后重派）：
  两 worker 写同一 `shard_{id}.proof.cert.tmp` → rename 原子覆盖，
  后写者胜。主控以 VerifyReport 为准（验证通过者胜）。

---

## 14. 安全模型（v1.0 深化）

### 14.1 证书不可伪造

- 证书正文 = 可编译证明（Lean/Coq）——伪造者需构造合法证明，
  等价于攻破证明系统（超出威胁模型）。
- `content-hash` 防意外损坏（非防恶意篡改；防篡改需签名，见下）。

### 14.2 可选签名（未来）

- 信封增加 `signature` 字段（Ed25519/HMAC，密钥由主控持有）。
- 用于跨信任域部署（多租户）：计算进程需签名才被接受。
- 单机/单信任域：省略签名，依赖"证明本身可编译"的强保证。

### 14.3 资源隔离

- 分片 worker 以 `timeout_ms` 硬限制（lv_external_process_run 已支持）。
- 内存/文件句柄：worker 进程独立（崩溃即回收），主控只读证书文件。
- 证书目录权限：worker 只写自己分片（`shard_{id}.tmp`），主控只读正式名。

---

## 15. 深化后的工作量修正（v1.0）

| 模块 | v0.2 预估 | v1.0 修正 | 新增内容 |
|---|---|---|---|
| scheduler_core（含状态机） | 800-1200 | 1000-1400 | 状态机 + 重试退避 |
| scheduler_proto（含协议） | 300-500 | 400-600 | 消息类型 + 幂等重放 |
| scheduler_verify（含验证流程） | 200-300 | 400-500 | 并行验证 + VerifyReport |
| scheduler_worker（含恢复） | 300-400 | 400-500 | 崩溃恢复扫描 + 幂等 |
| **新增小计** | **3400-4700** | **4000-5400** | |

- 结论修正：新增约 **4000-5400 行**（含测试），仍 < 代码库 2%。
- 复用 8800 行不变，重写 0 行不变。
