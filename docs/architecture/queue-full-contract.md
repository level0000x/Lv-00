# 队列满行为契约登记（K75/D4）

> 单源登记文档：Lv-00 各队列/缓冲的「满时行为」现状契约。
> 立项：K75 D4「队列满行为 4 约定（P0）——丢弃/阻塞/覆盖/返回错误未文档化」。
> 处置（用户 2026-09-01 决策）：行为差异本身合理（不同语义域），不统一行为、
> 统一文档登记 + 后续按需补测试。本文档只做现状契约登记说明；代码改动暂缓。

## 1. 契约总表

| 队列/缓冲 | 位置 | 满时行为 | 语义域 |
| --- | --- | --- | --- |
| `command_log`（命令日志） | layer4 command_log.c `command_log_append` | **自动扩容**（lv_darray_push，无固定上限；仅初始容量 1024 提示） | 撤销/重做历史，需完整保留 |
| `stream_buffer`（流式事件缓冲） | layer2 stream/stream_buffer.c `stream_buffer_push` | **丢弃 + 计数**（达 `STREAM_MAX_BUFFER` 后丢弃并 `dropped_count++`） | 实时事件流，允许丢旧保新防内存膨胀 |
| `thread_pool` 任务队列 | layer2 thread_pool.c `lv_thread_pool_submit` | **返回错误**（队列满 `MAX_TASK_QUEUE` 返回 `lv_ERROR_OVERFLOW`，不阻塞不排队） | 异步任务提交，超限拒绝由调用方处理 |
| 错误帧栈（lv_error） | layer2 lv_error.c | **覆盖最旧**（栈满丢最旧帧保最新，栈顶恒最新） | 错误诊断，保留最新上下文 |

## 2. 与设计文档审计结论的差异核对

设计文档（standard-unification-design.md K75 行）审计时描述的 4 队列为
「command_log 环形覆盖 / stream 丢弃带计数 / 消息队列阻塞 / progress 队列丢弃」。
逐项核对现状：

- **command_log「环形覆盖」→ 实为自动扩容**：`command_log_append` 走
  `lv_darray_push` 倍增扩容，无覆盖语义（历史为撤销/重做完整性服务）。
  登记修正。
- **stream「丢弃带计数」→ 相符**：`stream_buffer_push` 满时丢弃并
  `dropped_count++`（stream_buffer.c:53-54 注释已登记 K75/D4）。
- **「消息队列阻塞」→ 无对应独立实现**：最接近的是 thread_pool 任务队列，
  但实际行为是**返回错误**（`lv_ERROR_OVERFLOW`）而非阻塞。登记修正。
- **「progress 队列丢弃」→ 无独立 progress 队列**：progress 是 StreamEvent
  的一种类型（`STREAM_EVENT_PROGRESS`，stream_emit.c），随 stream_buffer
  统一丢弃计数，无独立队列。登记修正。

## 3. 判定结论

4 种满时行为（扩容/丢弃计数/返回错误/覆盖最旧）分属不同语义域，行为差异
合理，**不统一**。登记契约后仍开放的收尾项：

1. （低优先）为 command_log 与错误帧栈补满行为契约测试钉住现状；
2. thread_pool 满队列的调用方错误处理已按 `lv_ERROR_OVERFLOW` 走
   `lv_RETURN_ERROR_NULL`，无需改动。

## 4. 关联登记

- K75 D2 预设上限单源（批次 180）：`lv_PRESET_MAX_COUNT` 10000 已收敛。
- K75 插件双注册表 256 vs 32：plugin_system 容量已改读配置 A（批次 226，
  `integration.max_plugins` / `max_interfaces` 单源）。
- K75 D5 lv_malloc_bounded 死设施：零调用登记见批次 180 遗留；本会话评估
  后认为解析闸门已覆盖输入上限，无明确接线点，维持登记（不接线）。
