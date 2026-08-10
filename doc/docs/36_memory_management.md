# 36. 内存管理（Memory Management）

## 模块概述

本文档描述 Lv-00 几何元语言系统的内存管理设施，覆盖分配策略、内存池、泄漏检测与 arena 生命周期。系统采用**分层分配策略**：通用分配走可替换分配器（默认调试分配器），高频小对象走固定大小对象池，临时批量对象走竞技场（arena）批量分配一次性释放。该组头文件主要由 `core/src/layer2_resource/` 下的 `lv_arena.c`、`lv_heap.c` 与 `debug.c`（`lvMemPool` 实现）承载。

**覆盖头文件**：
- `lv_arena.h` —— 竞技场分配器：批量分配一次性释放、checkpoint/rollback、临时竞技场
- `memory_pool.h` —— 固定大小对象池、全局内存统计、通用内存分配包装（`lv_malloc`/`lv_free` 等）、预定义对象池
- `lv_mempool.h` —— 基于 `debug.h` 中 `lvMemPool` 的轻量内存池公共 API
- `lv_mempool_utils.h` —— 内存池静态单例工具（延迟初始化 + 自动置空清理）
- `allocator.h` —— 可替换内存分配器策略接口（`AllocatorOps` vtable）
- `lv_heap.h` —— 泛型二叉堆（优先级队列，基于数组、动态扩容，供调度器复用）

---

## 核心设计原则

1. **分层分配**：按对象生命周期与大小选择分配设施——短生命周期批量对象用 arena，固定大小高频对象用对象池，其余走通用分配器，避免单一 malloc 造成的碎片与锁竞争。
2. **批量分配、一次性释放**：arena 以块为单位批量获取内存，销毁或回滚时整块归还，彻底消除逐对象 free 的开销与碎片。
3. **对象复用**：固定大小对象池预分配并复用槽位，`lv_pool_alloc`/`lv_pool_free` 为常数时间操作，支持 `auto_grow` 自动扩容与可选的线程安全。
4. **可替换分配策略**：通过 `AllocatorOps` 虚表在运行时切换原始分配器、调试分配器或用户自定义分配器；切换线程安全，且要求所有已分配内存必须由原分配器释放。
5. **安全默认**：默认启用调试分配器，具备魔数头/尾（缓冲区溢出检测）、分配追踪链表（泄漏检测）、毒模式填充（use-after-free 检测）、内存统计与内存限制检查。
6. **可观测性**：全局内存统计按类型（`type_id`）登记分配/释放次数与字节峰值，支持打印统计报告，供运行时监控与排障。
7. **所有权明确**：`lv_free(void **ptr)` 释放后自动置空，杜绝悬垂指针；`lv_strdup` 由 `lv.h` 映射到平台 `strdup`/`_strdup`，调用者持有返回指针所有权。

---

## 关键数据结构（C 代码块）

```c
/* 竞技场（lv_arena.h） */
typedef struct lvArenaBlock {
    struct lvArenaBlock *next;   /* 下一个块 */
    size_t capacity;             /* 块总容量 */
    size_t used;                 /* 已用字节数 */
} lvArenaBlock;                  /* 数据区域紧随其后 */

typedef struct lvArenaMark {
    lvArenaBlock *block;         /* 当前块 */
    size_t offset;               /* 当前块中的偏移 */
} lvArenaMark;

typedef struct lvArena {
    lvArenaBlock *head;          /* 当前活跃块 */
    lvArenaBlock *blocks;        /* 所有块链表 */
    size_t block_size;           /* 默认块大小（0 = 64KB） */
    size_t total_allocated;      /* 总分配字节数 */
    size_t total_used;           /* 总使用字节数 */
    bool thread_safe;            /* 是否线程安全 */
    lvMutex mutex;               /* 互斥锁（仅 thread_safe=true） */
} lvArena;

/* 固定大小对象池（memory_pool.h） */
typedef struct {
    size_t object_size;          /* 单个对象大小（字节） */
    size_t capacity;             /* 初始容量 */
    bool thread_safe;            /* 是否线程安全 */
    bool auto_grow;              /* 容量不足时自动扩展 */
    const char *name;            /* 池名称（用于调试） */
} lvPoolConfig;

/* 全局内存统计（memory_pool.h） */
struct lvMemoryStats {
    lvMemTypeStat types[lv_MEM_STAT_MAX_TYPES]; /* 各类型统计，64 上限 */
    int type_count;                             /* 已注册类型数（遍历上界） */
    uint64_t total_bytes;                       /* 总使用字节数 */
    uint64_t peak_bytes;                        /* 总峰值字节数 */
};

/* 可替换分配器虚表（allocator.h） */
typedef struct {
    void *(*alloc)(size_t size);
    void *(*calloc)(size_t count, size_t size);
    void *(*realloc)(void *ptr, size_t new_size);
    void  (*free)(void *ptr);
    const char *name;
    AllocatorSizeQuery size_query;  /* 查询底层分配实际可用大小，可为 NULL */
} AllocatorOps;
```

> 说明：`lvMemTypeStat` 包含 `name/total_allocs/total_frees/current_bytes/peak_bytes` 五字段；遍历 `types[]` 时必须以 `type_count` 为上界，而非 `lv_MEM_STAT_MAX_TYPES`。`lvObjectPool` 与 `lvMemPool`（定义于 `debug.h`）为不透明类型，通过 API 操作。

---

## 主要接口（表格）

### 竞技场分配器（`lv_arena.h`）

| 函数 | 功能 |
|------|------|
| `lv_arena_create(block_size, thread_safe)` | 创建竞技场，块大小 0 表示默认 64KB |
| `lv_arena_destroy(arena)` | 销毁并释放所有块 |
| `lv_arena_alloc(arena, size)` | 分配（8 字节对齐），失败返回 NULL |
| `lv_arena_alloc_aligned(arena, size, alignment)` | 自定义对齐分配（2 的幂，0=默认 8 字节） |
| `lv_arena_calloc(arena, size)` | 分配并清零 |
| `lv_arena_strdup(arena, str)` | 复制字符串到竞技场 |
| `lv_arena_reset(arena)` | 重置竞技场（释放所有块，回到初始状态） |
| `lv_arena_mark(arena)` | 获取当前标记（checkpoint） |
| `lv_arena_reset_to_mark(arena, mark)` | 回滚到标记（释放其后分配的内存） |
| `lv_arena_total_allocated/total_used/block_count` | 查询统计 |
| `lv_arena_lock/unlock(arena)` | 线程安全包装 |
| `lv_arena_tmp()` | 获取线程局部临时竞技场 |

### 固定大小对象池（`memory_pool.h`）

| 函数 | 功能 |
|------|------|
| `lv_pool_create(config)` / `lv_pool_destroy(pool)` | 创建/销毁对象池 |
| `lv_pool_alloc(pool)` / `lv_pool_free(pool, obj)` | 分配/归还对象 |
| `lv_pool_get_stats(pool, ...)` | 输出总分配/释放次数与当前使用数量 |
| `lv_pool_clear(pool)` | 清空对象池（不销毁池本身） |
| `lv_init_preset_pools()` / `lv_cleanup_preset_pools()` | 初始化/清理预定义对象池 |
| `lv_get_node_pool()` / `lv_get_constraint_pool()` / `lv_get_symbolic_coord_pool()` / `lv_get_proof_step_pool()` | 获取 ConstraintNode、Constraint、SymbolicCoord、ProofStep 预定义池 |

### 全局内存统计（`memory_pool.h`）

| 函数 | 功能 |
|------|------|
| `lv_mem_register_type(name)` | 注册内存类型，返回 type_id（超限返回 -1） |
| `lv_mem_record_alloc(type_id, size)` / `lv_mem_record_free(type_id, size)` | 记录分配/释放 |
| `lv_mem_get_global_stats(stats)` | 获取全局统计快照 |
| `lv_mem_reset_stats()` | 重置全局统计 |
| `lv_mem_print_stats(stream)` | 打印统计报告（`stream` 为 `void*`，如 `stdout`） |

### 通用内存分配与可替换分配器

| 函数 | 功能 |
|------|------|
| `lv_malloc/calloc/realloc(size)` | 安全分配包装，失败设置错误码 |
| `lv_free(void **ptr)` | 释放并置空指针 |
| `lv_strdup(str)` | 跨平台字符串复制（lv.h 宏映射） |
| `lv_allocator_set(ops)` / `lv_allocator_get()` | 设置/获取全局分配器（切换线程安全） |
| `lv_allocator_reset()` | 重置为默认调试分配器 |
| `lv_allocator_raw()` / `lv_allocator_debug()` | 获取原始分配器 / 调试分配器实例 |

### 轻量内存池与静态单例（`lv_mempool.h` / `lv_mempool_utils.h`）

| 函数 | 功能 |
|------|------|
| `lv_mempool_create(block_size, initial_blocks)` | 创建内存池（基于 debug.h 的 lvMemPool） |
| `lv_mempool_destroy(pool)` / `lv_mempool_alloc(pool)` / `lv_mempool_free(pool, block)` | 销毁 / 分配 / 归还（NULL 安全） |
| `lv_mempool_static_init(&pool, block_size, initial_count)` | 静态单例延迟初始化 |
| `lv_mempool_static_destroy(&pool)` | 销毁静态池并置空指针，防悬垂 |

### 泛型二叉堆（`lv_heap.h`）

| 函数 | 功能 |
|------|------|
| `lv_heap_init(heap, elem_size, type, compare, capacity)` | 初始化堆（min/max，0=默认 16） |
| `lv_heap_destroy/push/pop/top/size/empty` | 堆生命周期与操作 |

---

## 工作流程

**arena 生命周期（临时批量对象）**：

```
lv_arena_create -> lv_arena_alloc/calloc/strdup（块满时按翻倍策略新建块并挂入链表）
              -> lv_arena_mark（checkpoint） -> 继续分配
              -> lv_arena_reset_to_mark（回滚，仅释放 mark 之后的分配）
              -> lv_arena_reset / lv_arena_destroy（整块归还，碎片归零）
```

**对象池复用（高频小对象）**：`lv_pool_create` 按 `lvPoolConfig` 预分配容量；`lv_pool_alloc` 从空闲槽取出对象，`lv_pool_free` 归还槽位；容量不足且 `auto_grow` 时自动扩容。解析层、推理层的 ConstraintNode/Constraint 等实体通过 `lv_init_preset_pools` 预定义池获得，降低分配抖动。

**分配与泄漏检测路径**：`lv_malloc` → 当前分配器（默认调试分配器）→ 附加魔数头/尾、登记分配追踪链表、毒模式填充、更新 `lvMemoryStats`；`lv_free` 校验魔数并移除追踪记录。泄漏检测依赖两点：追踪链表在进程退出时仍存在未释放记录，以及 `lv_mem_print_stats` 报告中 `total_allocs > total_frees` 且 `current_bytes > 0` 的类型即存在泄漏。`type_id` 由 `lv_mem_register_type` 于初始化阶段分配。

**分配器切换约束**：`lv_allocator_set` 切换前需确保所有已分配内存均由原分配器释放；`lv_allocator_get` 位于热路径，不做加锁（指针写入仅发生在切换这一罕见操作，读为对齐原子操作）。

---

## 模块关系（表格）

| 上游/调用方 | 关系说明 | 参考文档 |
|-------------|----------|----------|
| 核心基础设施（`lv.h`/`lv_utils.h`/`debug.h`/`config.h`） | 提供 `lv_malloc` 家族、`lvMemPool` 实现、错误码与阈值配置，本模块是其内存能力面 | [23_core_infrastructure.md](23_core_infrastructure.md) |
| 上下文与生命周期 | arena 生命周期与上下文创建/销毁对齐，临时对象挂靠上下文级 arena | [12_context_and_lifecycle.md](12_context_and_lifecycle.md) |
| 约束传播 | ConstraintNode/Constraint 预定义对象池服务约束图节点分配 | [24_constraint_propagation.md](24_constraint_propagation.md) |
| 引擎调度器 | 调度任务使用泛型堆（`lv_heap`）做优先级队列，复用内存统计做配额监控 | [25_engine_scheduler.md](25_engine_scheduler.md) |
| 运行时监控 | 内存统计（分配/释放/峰值）作为健康检查与事件追踪数据源 | [32_runtime_monitoring.md](32_runtime_monitoring.md) |
| 函数块系统 | 函数块对象（`FuncBlock`）分配走固定大小池，降低高频调用分配开销 | [07_func_block.md](07_func_block.md) |
| 元证明缓存 / 图形化编程 | 缓存与可视化构造产物使用 arena 批量分配，随会话一次性释放 | [34_meta_proof_cache.md](34_meta_proof_cache.md)、[35_layer6_visual_programming.md](35_layer6_visual_programming.md) |

---

## 版本历史

| 版本 | 日期 | 变更内容 |
|------|------|----------|
| 1.0.0 | 2026-06 | 引入 `memory_pool.h`（对象池 + 全局统计 + 安全分配包装）与 `lv_arena.h`（竞技场） |
| 1.1.0 | 2026-07 | 移除线性分配器 `lvLinearAllocator`（由 `lv_arena` 自定义对齐承接）与 LRU 对象缓存 `lvObjectCache`；新增 `allocator.h` 可替换分配器 vtable 与 `lv_mempool_utils.h` 静态单例工具 |
| 1.2.0 | 2026-08 | 本次成文：明确分层分配策略、泄漏检测路径与 arena 生命周期规范 |
