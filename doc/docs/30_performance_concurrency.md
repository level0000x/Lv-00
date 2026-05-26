# 30. 性能优化与并发系统

## 30.1 模块概述

本文档描述 Lv-00 几何元语言系统中的性能优化、并发调度、测试与索引基础设施。该组模块不直接定义几何公理或推理规则，但决定系统在大规模约束图、多策略推理、批量几何计算和持续测试中的可扩展性。

**覆盖头文件**：
- `thread_pool.h` —— 高性能并发任务调度与工作窃取线程池
- `simd_ops.h` —— 跨平台 SIMD 向量运算抽象层
- `benchmark.h` —— 性能基准测试框架
- `test_framework.h` —— 增强单元测试框架
- `fast_index.h` —— 高效索引与检索系统
- `performance_profiler.h` —— 性能剖析器、热点统计与运行期性能采样

---

## 30.2 理论定位

Lv-00 的理论核心是几何构造与证明，但工程实现必须支持：

1. **并行推理**：多后端、多策略、多约束子图可并行处理。
2. **向量化几何计算**：批量坐标、矩阵、区间边界可利用 SIMD 加速。
3. **可重复性能评估**：通过基准测试防止算法退化。
4. **持续正确性验证**：通过测试框架约束模块演化。
5. **低延迟索引检索**：对节点、约束、证明步骤、缓存项进行高效访问。

因此，本模块是 Lv-00 从理论原型扩展为可维护、高性能系统的工程支撑层。

---

## 30.3 thread_pool.h —— 并发任务调度

### 30.3.1 设计目标

线程池模块提供轻量级并发基础设施，支持：
- 工作窃取调度；
- 任务优先级；
- 任务依赖与屏障；
- 任务组批量等待；
- 优雅关闭与即时取消。

### 30.3.2 任务优先级与状态

```c
typedef enum {
    LV00_TASK_PRIORITY_LOW = 0,
    LV00_TASK_PRIORITY_NORMAL = 1,
    LV00_TASK_PRIORITY_HIGH = 2,
    LV00_TASK_PRIORITY_URGENT = 3
} Lv00TaskPriority;
```

```c
typedef enum {
    LV00_TASK_STATUS_PENDING = 0,
    LV00_TASK_STATUS_RUNNING = 1,
    LV00_TASK_STATUS_COMPLETED = 2,
    LV00_TASK_STATUS_FAILED = 3,
    LV00_TASK_STATUS_CANCELLED = 4
} Lv00TaskStatus;
```

优先级用于调度策略，状态用于任务生命周期追踪。

### 30.3.3 任务结构

```c
struct Lv00Task {
    uint64_t id;
    char name[LV00_POOL_TASK_NAME_LEN];
    Lv00TaskPriority priority;
    Lv00TaskStatus status;

    Lv00TaskFunc func;
    void *user_data;
    Lv00TaskCallback callback;
    void *callback_data;

    uint64_t *depends_on;
    int depends_count;
    int depends_remaining;

    Lv00TaskGroup *group;

    int result;
    uint64_t submit_time;
    uint64_t start_time;
    uint64_t end_time;

    struct Lv00Task *next;
};
```

任务依赖通过 `depends_on` 与 `depends_remaining` 记录。只有依赖完成后，任务才进入可执行队列。

### 30.3.4 任务组与统计

```c
struct Lv00TaskGroup {
    uint64_t id;
    char name[64];
    int total_tasks;
    int completed_tasks;
    int failed_tasks;
    bool all_submitted;
    bool wait_cancelled;
    void *completion_event;
};
```

```c
struct Lv00ThreadPoolStats {
    uint64_t tasks_submitted;
    uint64_t tasks_completed;
    uint64_t tasks_failed;
    uint64_t tasks_cancelled;
    uint64_t total_execute_time_us;
    uint64_t max_task_time_us;
    uint64_t queue_max_size;
    uint64_t steal_count;
    int active_threads;
    int idle_threads;
};
```

这些统计项可用于运行时监控与性能回归分析。

### 30.3.5 线程池配置与生命周期

```c
typedef struct {
    int thread_count;
    int queue_capacity;
    bool enable_stealing;
    bool enable_affinity;
    const char *name;
} Lv00ThreadPoolConfig;
```

```c
Lv00ThreadPool *lv00_thread_pool_create(const Lv00ThreadPoolConfig *config);
void lv00_thread_pool_destroy(Lv00ThreadPool *pool, bool immediate);
void lv00_thread_pool_default_config(Lv00ThreadPoolConfig *config);
```

当 `thread_count == 0` 时，线程数由 CPU 核心数自动决定。`immediate == true` 表示销毁时取消待执行任务。

---

## 30.4 simd_ops.h —— SIMD 向量运算

### 30.4.1 设计目标

SIMD 模块提供跨平台向量运算抽象，支持：
- x86/x64 SSE、AVX、AVX2、AVX512；
- ARM NEON；
- 无 SIMD 支持时自动回退到标量实现。

### 30.4.2 能力检测

```c
typedef enum {
    LV00_SIMD_NONE     = 0,
    LV00_SIMD_SSE2     = 1 << 0,
    LV00_SIMD_SSE41    = 1 << 1,
    LV00_SIMD_AVX      = 1 << 2,
    LV00_SIMD_AVX2     = 1 << 3,
    LV00_SIMD_AVX512F  = 1 << 4,
    LV00_SIMD_NEON     = 1 << 5,
} Lv00SimdCapability;
```

```c
uint32_t lv00_simd_detect_capabilities(void);
bool lv00_simd_has_capability(Lv00SimdCapability cap);
const char *lv00_simd_capability_name(Lv00SimdCapability cap);
```

### 30.4.3 向量类型

```c
typedef struct { double v[4]; } Lv00Vec4d;
typedef struct { float  v[4]; } Lv00Vec4f;
typedef struct { float  v[8]; } Lv00Vec8f;
typedef struct { int32_t v[4]; } Lv00Vec4i;
```

`Lv00Vec4d` 主要用于四个双精度坐标或区间边界的批量处理。

### 30.4.4 向量算术

```c
Lv00Vec4d lv00_vec4d_zero(void);
Lv00Vec4d lv00_vec4d_one(void);
Lv00Vec4d lv00_vec4d_set1(double val);
Lv00Vec4d lv00_vec4d_set(double x, double y, double z, double w);
Lv00Vec4d lv00_vec4d_load(const double *ptr);
Lv00Vec4d lv00_vec4d_loadu(const double *ptr);
void lv00_vec4d_store(double *ptr, Lv00Vec4d vec);
void lv00_vec4d_storeu(double *ptr, Lv00Vec4d vec);
```

```c
Lv00Vec4d lv00_vec4d_add(Lv00Vec4d a, Lv00Vec4d b);
Lv00Vec4d lv00_vec4d_sub(Lv00Vec4d a, Lv00Vec4d b);
Lv00Vec4d lv00_vec4d_mul(Lv00Vec4d a, Lv00Vec4d b);
Lv00Vec4d lv00_vec4d_div(Lv00Vec4d a, Lv00Vec4d b);
Lv00Vec4d lv00_vec4d_sqrt(Lv00Vec4d a);
Lv00Vec4d lv00_vec4d_abs(Lv00Vec4d a);
Lv00Vec4d lv00_vec4d_fmadd(Lv00Vec4d a, Lv00Vec4d x, Lv00Vec4d y);
```

### 30.4.5 向量比较

```c
Lv00Vec4d lv00_vec4d_cmpeq(Lv00Vec4d a, Lv00Vec4d b);
Lv00Vec4d lv00_vec4d_cmplt(Lv00Vec4d a, Lv00Vec4d b);
Lv00Vec4d lv00_vec4d_cmple(Lv00Vec4d a, Lv00Vec4d b);
Lv00Vec4d lv00_vec4d_cmpgt(Lv00Vec4d a, Lv00Vec4d b);
```

比较结果以掩码向量表示，可用于批量筛选几何点、区间或候选约束。

---

## 30.5 benchmark.h —— 性能基准测试框架

### 30.5.1 框架目标

基准测试模块用于测量与防止性能退化，支持：
- 微基准测试；
- 模块级宏基准测试；
- 自动迭代次数调整；
- 均值、标准差、百分位统计；
- 结果比较与回归检测。

### 30.5.2 测试函数与结果结构

```c
typedef uint64_t (*Lv00BenchFunc)(int iterations, void *user_data);
```

```c
typedef struct {
    char name[64];
    int iterations;
    uint64_t total_time_us;
    double mean_us;
    double std_dev_us;
    double min_us;
    double max_us;
    double percentiles[LV00_BENCH_PERCENTILE_COUNT];
    double ops_per_sec;
    size_t memory_before;
    size_t memory_after;
    size_t memory_peak;
    bool success;
    char error_msg[256];
} Lv00BenchResult;
```

### 30.5.3 基准套件

```c
typedef struct {
    char name[64];
    Lv00BenchFunc func;
    Lv00BenchSetupFunc setup;
    Lv00BenchTeardownFunc teardown;
    void *user_data;
    int min_iterations;
    int max_iterations;
    double target_time_sec;
} Lv00BenchCase;
```

```c
Lv00BenchSuite *lv00_bench_suite_create(const char *name);
void lv00_bench_suite_destroy(Lv00BenchSuite *suite);
int lv00_bench_suite_add(Lv00BenchSuite *suite, const Lv00BenchCase *case_);
int lv00_bench_suite_run(Lv00BenchSuite *suite);
const Lv00BenchResult *lv00_bench_suite_get_result(const Lv00BenchSuite *suite, int index);
char *lv00_bench_suite_to_json(const Lv00BenchSuite *suite);
char *lv00_bench_suite_to_markdown(const Lv00BenchSuite *suite);
```

### 30.5.4 回归检测

```c
typedef struct {
    double mean_ratio;
    double min_ratio;
    double max_ratio;
    double ops_ratio;
    bool is_regression;
    double regression_threshold;
} Lv00BenchComparison;
```

```c
Lv00BenchComparison lv00_bench_compare(const Lv00BenchResult *baseline,
                                        const Lv00BenchResult *current,
                                        double regression_threshold);
```

---

## 30.6 test_framework.h —— 增强单元测试框架

### 30.6.1 框架目标

测试框架提供：
- 测试注册、发现与执行；
- 断言失败记录；
- setup/teardown 夹具；
- 参数化测试；
- JSON/XML/HTML 多格式报告；
- 超时、跳过、错误状态区分。

### 30.6.2 测试状态与严重性

```c
typedef enum {
    TEST_STATUS_PENDING,
    TEST_STATUS_RUNNING,
    TEST_STATUS_PASSED,
    TEST_STATUS_FAILED,
    TEST_STATUS_SKIPPED,
    TEST_STATUS_ERROR,
    TEST_STATUS_TIMEOUT
} Lv00TestStatus;
```

```c
typedef enum {
    TEST_SEVERITY_INFO,
    TEST_SEVERITY_WARNING,
    TEST_SEVERITY_ERROR,
    TEST_SEVERITY_FATAL
} Lv00TestSeverity;
```

### 30.6.3 测试用例与套件

```c
struct Lv00TestCase {
    char name[LV00_TEST_NAME_MAX_LEN];
    char suite[LV00_TEST_SUITE_MAX_LEN];
    Lv00TestFunc func;
    Lv00TestSetupFunc setup;
    Lv00TestTeardownFunc teardown;
    char **tags;
    uint32_t tag_count;
    Lv00TestStatus status;
    char message[LV00_TEST_MSG_MAX_LEN];
    char file[256];
    int line;
    int64_t elapsed_ns;
    uint64_t memory_used;
    void *test_data;
    uint32_t data_index;
};
```

```c
struct Lv00TestSuite {
    char name[LV00_TEST_SUITE_MAX_LEN];
    Lv00TestCase *cases;
    uint32_t case_count;
    uint32_t case_capacity;
    Lv00TestSetupFunc suite_setup;
    Lv00TestTeardownFunc suite_teardown;
    uint32_t passed_count;
    uint32_t failed_count;
    uint32_t skipped_count;
};
```

### 30.6.4 注册接口

```c
bool lv00_test_register(const char *suite_name,
                        const char *test_name,
                        Lv00TestFunc func);

bool lv00_test_register_with_fixture(const char *suite_name,
                                      const char *test_name,
                                      Lv00TestFunc func,
                                      Lv00TestSetupFunc setup,
                                      Lv00TestTeardownFunc teardown);
```

---

## 30.7 fast_index.h —— 高效索引系统

### 30.7.1 设计目标

`fast_index.h` 提供多种高性能索引结构：

1. Robin Hood 哈希表；
2. 布隆过滤器；
3. 跳表；
4. LRU 缓存；
5. R 树变体空间索引。

目标是平均 O(1) 查找、低碎片、高缓存命中率，并在需要时支持线程安全。

### 30.7.2 Robin Hood 哈希表

```c
typedef struct Lv00HashEntry {
    uint64_t key;
    void *value;
    uint8_t hash;
    bool occupied;
} Lv00HashEntry;
```

```c
typedef struct {
    Lv00HashEntry *entries;
    size_t capacity;
    size_t count;
    size_t tombstones;
    bool thread_safe;
    void *mutex;
} Lv00HashTable;
```

核心 API：

```c
Lv00HashTable *lv00_hash_create(size_t initial_capacity, bool thread_safe);
void lv00_hash_destroy(Lv00HashTable *ht);
void *lv00_hash_insert(Lv00HashTable *ht, uint64_t key, void *value);
void *lv00_hash_find(const Lv00HashTable *ht, uint64_t key);
void *lv00_hash_remove(Lv00HashTable *ht, uint64_t key);
bool lv00_hash_contains(const Lv00HashTable *ht, uint64_t key);
size_t lv00_hash_size(const Lv00HashTable *ht);
void lv00_hash_clear(Lv00HashTable *ht);
```

### 30.7.3 布隆过滤器

```c
typedef struct {
    uint8_t *bits;
    size_t num_bits;
    size_t num_hashes;
    size_t count;
} Lv00BloomFilter;
```

核心语义：
- 返回 false 表示元素一定不存在；
- 返回 true 表示元素可能存在，可能有假阳性。

```c
Lv00BloomFilter *lv00_bloom_create(size_t expected_items, double error_rate);
void lv00_bloom_destroy(Lv00BloomFilter *bf);
void lv00_bloom_add(Lv00BloomFilter *bf, const void *data, size_t len);
void lv00_bloom_add_str(Lv00BloomFilter *bf, const char *str);
void lv00_bloom_add_int(Lv00BloomFilter *bf, int64_t value);
bool lv00_bloom_might_contain(const Lv00BloomFilter *bf, const void *data, size_t len);
double lv00_bloom_estimate_fp_rate(const Lv00BloomFilter *bf);
```

### 30.7.4 跳表

```c
typedef struct Lv00SkipNode {
    int64_t key;
    void *value;
    int level;
    struct Lv00SkipNode **forward;
} Lv00SkipNode;
```

```c
typedef struct {
    Lv00SkipNode *header;
    int level;
    size_t count;
    bool thread_safe;
    void *mutex;
} Lv00SkipList;
```

跳表适合有序键检索、区间查询和排名近似。

---

## 30.8 理论—代码对应关系

| 代码概念 | 理论/工程对应 | 说明 |
|----------|----------------|------|
| `Lv00ThreadPool` | 并行任务调度器 | 支持工作窃取和任务依赖 |
| `Lv00TaskGroup` | 屏障/批处理单元 | 等待一组任务完成 |
| `Lv00Vec4d` | SIMD 向量空间 | 批量双精度坐标处理 |
| `Lv00BenchResult` | 性能统计样本 | 均值、方差、百分位和吞吐量 |
| `Lv00TestReport` | 测试证明记录 | 保存执行状态和报告输出 |
| `Lv00HashTable` | O(1) 平均索引 | Robin Hood 哈希 |
| `Lv00BloomFilter` | 概率存在性判断 | false 一定不存在，true 可能存在 |
| `Lv00SkipList` | 随机化有序结构 | 支持快速有序查找 |

---

## 30.9 相关模块文档

| 文档 | 关联内容 |
|------|----------|
| [23_core_infrastructure.md](23_core_infrastructure.md) | 基础设施、配置、错误与内存管理 |
| [25_engine_scheduler.md](25_engine_scheduler.md) | 引擎调度与后端选择 |
| [17_numerical_analysis.md](17_numerical_analysis.md) | 数值计算与误差分析 |
| [24_constraint_propagation.md](24_constraint_propagation.md) | 约束传播与图哈希 |
| [32_runtime_monitoring.md](32_runtime_monitoring.md) | 运行时监控和保护机制 |

---

## 30.10 版本历史

- **v3.5.0**
  - 补全文档化：线程池、SIMD、基准测试、测试框架、高效索引。
  - 明确性能基础设施在五层架构中的支撑作用。

- **v3.3.0**
  - 引入并发、SIMD、测试、基准和索引基础模块。
