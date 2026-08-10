# 30. 性能与并发：线程池、环形缓冲与基准测试

## 模块概述

本文档描述 Lv-00 几何元语言系统的性能与并发基础层。该层提供跨平台线程抽象、线程池任务调度、泛型环形缓冲区、网格空间索引、哈希历史去重，以及基准测试/性能追踪工具，是约束求解、量词枚举、证明搜索等重计算模块的并行化地基。

**覆盖头文件**：
- `thread_pool.h` —— 线程池与等待组、并行 for 抽象
- `lv_thread.h` —— 跨平台线程原语（互斥锁/条件变量/线程/一次性初始化/惰性锁）
- `lv_ringbuf.h` —— 泛型环形缓冲区
- `fast_index.h` —— 网格哈希 2D 空间索引
- `hash_history.h` —— uint64 环形哈希历史（含 32 位轻量预筛）
- `performance_profiler.h` —— 基准测试与性能会话（另见 `test_framework.h` 的 `lvBenchmark`）

---

## 核心设计原则

1. **跨平台零开销抽象**：`lv_thread.h` 全部为 `static inline` 实现，在 Windows（CRITICAL_SECTION/CONDITION_VARIABLE/_beginthreadex/InitOnceExecuteOnce）与 POSIX（pthread）之间提供一致 C 接口，无函数调用间接开销。
2. **正确性优先、性能次之**：线程池未链接真实现（未定义 `lv_THREAD_POOL_IMPL`）时，`lv_parallel_for` 退化为顺序执行占位实现；`pool == NULL` 时同样顺序执行，保证任何配置下语义正确。
3. **调用者负责同步**：`lvRingBuf` 明确线程不安全，由调用方加锁；线程池内部队列则由自带的 `mutex + not_empty` 条件变量保护，不向外暴露。
4. **跨线程分配器安全**：任务节点由 worker 线程释放。`lvThreadTask.uses_std_free == 1` 表示节点由标准 `calloc` 分配、worker 用 `free` 释放，避免跨线程 `lv_free` 破坏 lv 分配器的 TLS 追踪链表。
5. **全局单例一次性初始化**：全局线程池通过 `lv_once` 保证恰好初始化一次，销毁前先置空全局指针，保证 NULL 安全与重复销毁安全。
6. **环形容量语义统一**：`lvRingBuf` 与 `HashHistory` 均为"满容量覆盖最旧"的环形语义；`HashHistory.contains` 采用两阶段检查（32 位轻量预筛 → 64 位精确确认），以空间换时间。
7. **数据局部性加速**：`fast_index.h` 采用均匀网格 + Cantor 配对哈希，插入 O(1) 期望、查询 O(1+k) 期望，用于几何点命中预筛。

---

## 关键数据结构

```c
/* 线程池（内部结构，见 thread_pool.c） */
struct lvThreadPool {
    lv_thread_t *threads;   /* 工作线程句柄数组 */
    int thread_count;       /* 工作线程数 */
    lvThreadTask *queue_head; /* 任务队列头（链表） */
    lvThreadTask *queue_tail; /* 任务队列尾 */
    int queue_size;           /* 当前队列长度 */
    lv_mutex_t mutex;         /* 队列保护互斥锁 */
    lv_cond_t not_empty;      /* 队列非空条件变量 */
    int shutdown;             /* 关闭标志 */
};

/* 任务节点（thread_pool.h） */
struct lvThreadTask {
    void (*func)(void *arg);   /* 任务函数 */
    void *arg;                 /* 任务参数 */
    struct lvWaitGroup *group; /* 所属等待组（可为 NULL） */
    struct lvThreadTask *next; /* 下一个任务 */
    int uses_std_free;         /* 1 = 标准分配，worker 用 free 释放 */
};

/* 等待组（thread_pool.h） */
struct lvWaitGroup {
    int pending;         /* 待完成任务数 */
    int completed_count; /* 已完成任务数（支持超时查询） */
    lv_mutex_t mutex;
    lv_cond_t cond;
};

/* 泛型环形缓冲区（lv_ringbuf.h） */
typedef struct {
    uint8_t *buffer;  /* 数据缓冲区 */
    size_t elem_size; /* 每个元素的大小 */
    int capacity;     /* 最大元素数 */
    int head;         /* 写入位置 */
    int count;        /* 当前元素数 */
} lvRingBuf;

/* 哈希历史（hash_history.h，布局与 WLHashHistory 兼容） */
typedef struct HashHistory {
    uint64_t *full_history;  /* 完整 64 位哈希环形缓冲 */
    int full_count;          /* 完整缓冲元素数 */
    int full_pos;            /* 环形写入位置 */
    uint32_t *light_history; /* 32 位轻量预筛缓冲（NULL 禁用） */
    int light_count;
    int light_pos;
} HashHistory;
```

`lvFastIndex` 与 `lvThreadPool` 同为不透明类型：前者内部为"桶数组 + 多值链表"的均匀网格索引（单元大小取包围盒尺寸中位数，`MIN_CELL_SIZE=1e-6`，桶数 64 起倍增，上限 4096）。

---

## 主要接口

### 线程池与任务调度（thread_pool.h）

| 接口 | 签名 | 说明 |
|------|------|------|
| 创建线程池 | `lvThreadPool *lv_thread_pool_create(int num_threads)` | `<=0` 时取默认 4 线程，失败返回 NULL |
| 销毁线程池 | `void lv_thread_pool_destroy(lvThreadPool *pool)` | 广播退出信号、join 全部 worker、回收残留任务 |
| 全局单例 | `lvThreadPool *lv_get_global_thread_pool(void)` | `lv_once` 保证一次性初始化 |
| 销毁单例 | `void lv_global_thread_pool_destroy(void)` | 先置空再销毁，重复调用安全 |
| 提交任务 | `lvWaitGroup *lv_thread_pool_submit(lvThreadPool *pool, lvThreadTask *task)` | 新建等待组并入队；队列满（>4096）返回 NULL |
| 等待组 | `void lv_thread_pool_wait_group(lvThreadPool *pool, lvWaitGroup *group, int timeout_ms)` | `<0` 无限等待并自动释放；`==0` 非阻塞；`>0` 超时保留组供查询 |
| 并行 for | `void lv_parallel_for(lvThreadPool *pool, int n_iters, int chunk_size, lvParallelForFn fn, void *ctx)` | 按 `chunk_size` 分片提交，全部提交后统一阻塞等待；`pool==NULL` 顺序执行 |

### 线程原语（lv_thread.h）

| 接口 | 签名 | 说明 |
|------|------|------|
| 互斥锁 | `lv_mutex_init/destroy/lock/unlock(lv_mutex_t *)` | static inline，零开销 |
| 锁守卫 | `lv_lock_guard_init/destroy(lvLockGuard *g, lv_mutex_t *m)`；`LV_SCOPE_LOCK(m)`；`LV_SCOPE_LOCK_MAYBE(m, cond)` | RAII/goto-cleanup 风格；cleanup 属性在 MSVC 下退化为手动配对 |
| 条件变量 | `lv_cond_init/destroy/signal/broadcast/wait/timedwait` | `timedwait` 返回 0 为被唤醒，非 0 为超时 |
| 线程 | `lv_thread_create/join/detach`；`lv_thread_id(void)`；`lv_thread_sleep(ms)` | 入口统一为 `void *(*)(void *)` |
| 一次性初始化 | `lv_once(lv_once_t *once, void (*init)(void))`；`lv_ONCE_INIT` | 线程安全，替代手写 static+mutex |
| 惰性锁 | `lv_lazy_lock_init/lock/unlock/destroy`；`lv_LAZY_LOCK_DEFINE(name)` | once 守卫 + 互斥锁捆绑，首次 lock 自动初始化 |

### 环形缓冲与索引

| 接口 | 签名 | 说明 |
|------|------|------|
| 环形缓冲 | `lv_ringbuf_init(rb, elem_size, capacity)` / `lv_ringbuf_destroy` / `lv_ringbuf_write` / `lv_ringbuf_read` / `lv_ringbuf_get` / `lv_ringbuf_clear` / `lv_ringbuf_resize` | 满时覆盖最旧；`get` 越界返回 NULL；`count/capacity` 为 inline 查询 |
| 空间索引 | `lv_fast_index_create(int capacity)` / `lv_fast_index_destroy` / `lv_fast_index_insert(idx, node_id, x, y, w, h)` / `lv_fast_index_query(idx, x, y, out_ids, max_out)` | 插入 O(1) 期望；查询返回候选 ID 超集 |
| 哈希历史 | `hash_history_init(hh, capacity, use_light)` / `hash_history_destroy` / `hash_history_add(hh, capacity, hash)` / `hash_history_contains(hh, hash)` / `hash_history_count(hh)` | `use_light` 启用 32 位预筛；两阶段包含检查 |

### 基准测试与性能追踪（performance_profiler.h）

| 接口 | 签名 | 说明 |
|------|------|------|
| 基准测试 | `int lv_perf_benchmark_run(const char *name, void (*fn)(void), void *setup_fn, lvPerfBenchResult *result)` | 10 次预热 + 校准约 1 秒迭代数 + 计时统计 |
| 打印结果 | `void lv_perf_benchmark_print_result(const char *name, const lvPerfBenchResult *result, FILE *out)` | 输出均值/最小/最大/标准差 |
| 性能会话 | `lv_perf_session_create/destroy/reset`；`lv_perf_begin/end(session, region_name)` | 命名区域计时 |
| 内存追踪 | `lv_perf_session_record_alloc/free(session, type_name, bytes)` | 分配/释放事件记录 |
| 报告 | `lv_perf_report_print(session, out)`；`int lv_perf_report_to_json(session, buffer, size)` | 文本 / JSON 导出 |

另见 `test_framework.h`：`bool lv_benchmark_register(const char *name, lvBenchmarkFunc func, uint64_t iterations)`、`lvBenchmark *lv_benchmark_run(const char *name)`、`lv_benchmark_destroy(lvBenchmark *)`，与 `lv_perf_benchmark_run` 互为补充。

---

## 工作流程

**任务提交-等待生命周期**：调用方分配 `lvThreadTask` 并填充 `func/arg`，`lv_thread_pool_submit` 创建 `pending=1` 的等待组挂到任务上并入队，随后 `lv_cond_signal` 唤醒一个 worker；worker 循环在 `queue_size==0 && !shutdown` 时睡眠，取出队头任务、解锁执行，完成后按 `group` 递减 `pending` 并在归零时 signal；`lv_thread_pool_wait_group` 等待完成后销毁组。

**并行 for**：`lv_parallel_for` 将 `[0, n_iters)` 切成 `n_tasks = ceil(n_iters / chunk_size)` 片，每个分片一个 `par_for_worker` 任务（参数存于一次性 `args` 数组），先全部提交（保证并行度）再逐个无限等待，最后回收数组。

**关闭流程**：`lv_thread_pool_destroy` 置 `shutdown=1` 并 broadcast，worker 在队列空且 shutdown 时退出，主线程逐个 join 并清理残留任务与锁。

**基准测试**：`lv_perf_benchmark_run` 先执行 10 次预热，再通过校准确定约 1 秒的总迭代次数，正式计时后输出 `lvPerfBenchResult`（mean/min/max/stddev）。`lvPerfSession` 则用于会话内多区域累加计时与内存统计，可导出 JSON 供外部聚合。

**典型组合**：几何点批量命中查询先走 `lv_fast_index_query` 得到候选超集，再做精确谓词；重写/归一化用 `HashHistory` 两阶段判重防环；大规模量词枚举子任务经 `lv_parallel_for` 分片并行。

---

## 模块关系

| 相关模块 | 关系说明 |
|----------|----------|
| [25_engine_scheduler.md](25_engine_scheduler.md) | 引擎调度器负责多引擎动态路由；并发层为调度器提供并行执行后端与任务排队能力 |
| [27_quantifier_logic.md](27_quantifier_logic.md) | 量词有限域枚举与关系 SAT 搜索可经 `lv_parallel_for` 分片并行 |
| [32_runtime_monitoring.md](32_runtime_monitoring.md) | `runtime_guard.h` 提供自旋/耗时守卫约束并行任务；`runtime_monitor.h` 与 `performance_profiler.h` 共同承担性能可观测性 |
| [09_proof.md](09_proof.md) | 证明搜索的并行分支可提交线程池；验证超时/步数预算与 `runtime_guard` 联动 |
| [36_memory_management.md](36_memory_management.md) | lv 分配器 TLS 追踪链表约束跨线程释放（`uses_std_free`）；`lv_arena`/`lv_mempool` 供分片任务无竞争分配 |
| [04_solver.md](04_solver.md) | 约束求解迭代中可用环形缓冲做轨迹窗口、`HashHistory` 做状态去重 |

---

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.0.0 | 2026-08-10 | 初稿：整理 `thread_pool.h`/`lv_thread.h`/`lv_ringbuf.h`/`fast_index.h`/`hash_history.h`/`performance_profiler.h` 设计 |
