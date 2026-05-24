/**
 * @file thread_pool.c
 * @brief 线程池系统实现
 *
 * @details 实现高性能线程池，支持工作窃取、任务优先级、任务依赖等功能。
 *          跨平台支持 Windows 和 POSIX 系统。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "thread_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============== 平台抽象层 ============== */

#ifdef _WIN32
#include <windows.h>
#define WIN32_LEAN_AND_MEAN

typedef CRITICAL_SECTION Lv00Mutex;
typedef CONDITION_VARIABLE Lv00Cond;
typedef HANDLE Lv00Thread;

#define LV00_MUTEX_INIT(m) InitializeCriticalSection(&(m))
#define LV00_MUTEX_DESTROY(m) DeleteCriticalSection(&(m))
#define LV00_MUTEX_LOCK(m) EnterCriticalSection(&(m))
#define LV00_MUTEX_UNLOCK(m) LeaveCriticalSection(&(m))

#define LV00_COND_INIT(c) InitializeConditionVariable(&(c))
#define LV00_COND_DESTROY(c) /* Windows 条件变量无需销毁 */
#define LV00_COND_WAIT(c, m) SleepConditionVariableCS(&(c), &(m), INFINITE)
#define LV00_COND_SIGNAL(c) WakeConditionVariable(&(c))
#define LV00_COND_BROADCAST(c) WakeAllConditionVariable(&(c))

#define LV99_THREAD_FUNC DWORD WINAPI
#define LV99_THREAD_RETURN 0

#else /* POSIX */
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

typedef pthread_mutex_t Lv00Mutex;
typedef pthread_cond_t Lv00Cond;
typedef pthread_t Lv00Thread;

#define LV00_MUTEX_INIT(m) pthread_mutex_init(&(m), NULL)
#define LV00_MUTEX_DESTROY(m) pthread_mutex_destroy(&(m))
#define LV00_MUTEX_LOCK(m) pthread_mutex_lock(&(m))
#define LV00_MUTEX_UNLOCK(m) pthread_mutex_unlock(&(m))

#define LV00_COND_INIT(c) pthread_cond_init(&(c), NULL)
#define LV00_COND_DESTROY(c) pthread_cond_destroy(&(c))
#define LV00_COND_WAIT(c, m) pthread_cond_wait(&(c), &(m))
#define LV00_COND_SIGNAL(c) pthread_cond_signal(&(c))
#define LV00_COND_BROADCAST(c) pthread_cond_broadcast(&(c))

#define LV99_THREAD_FUNC void *
#define LV99_THREAD_RETURN NULL

#endif

/* ============== 内部常量 ============== */

/** 任务ID计数器初始值 */
#define LV00_TASK_ID_INIT 1

/** 任务队列增长因子 */
#define LV00_QUEUE_GROWTH_FACTOR 2

/* ============== 内部数据结构 ============== */

/**
 * @brief 优先级任务队列
 */
typedef struct {
    Lv00Task **tasks;       /**< 任务指针数组 */
    int count;              /**< 当前任务数 */
    int capacity;           /**< 容量 */
    Lv00Mutex mutex;        /**< 队列锁 */
} PriorityTaskQueue;

/**
 * @brief 工作线程
 */
typedef struct {
    int id;                 /**< 线程ID */
    Lv00ThreadPool *pool;   /**< 所属线程池 */
    Lv00Thread thread;      /**< 线程句柄 */

    /* 工作窃取队列 */
    Lv00Task *local_queue[LV00_POOL_WORKER_QUEUE_CAP];
    int local_count;

    /* 统计 */
    uint64_t tasks_executed;
    uint64_t steal_attempts;
    uint64_t steal_successes;

    bool running;           /**< 是否运行中 */
} WorkerThread;

/**
 * @brief 线程池结构
 */
struct Lv00ThreadPool {
    /* 配置 */
    char name[64];              /**< 线程池名称 */
    int thread_count;           /**< 工作线程数 */
    bool enable_stealing;       /**< 是否启用工作窃取 */
    bool enable_affinity;       /**< 是否启用CPU亲和性 */
    bool running;               /**< 是否运行中 */

    /* 工作线程 */
    WorkerThread **workers;     /**< 工作线程数组 */
    int active_workers;         /**< 活跃工作线程数 */

    /* 全局任务队列 */
    PriorityTaskQueue global_queue;

    /* 任务管理 */
    Lv00Task **task_registry;   /**< 任务注册表（用于查找） */
    int registry_count;
    int registry_capacity;
    uint64_t next_task_id;      /**< 下一个任务ID */

    /* 同步原语 */
    Lv00Mutex pool_mutex;       /**< 线程池锁 */
    Lv00Cond task_available;    /**< 任务可用条件变量 */
    Lv00Cond task_completed;    /**< 任务完成条件变量 */
    Lv00Cond all_completed;     /**< 所有任务完成条件变量 */

    /* 统计 */
    Lv00ThreadPoolStats stats;

    /* 关闭控制 */
    bool shutdown;              /**< 是否正在关闭 */
    bool immediate_shutdown;    /**< 是否立即关闭 */
};

/* ============== 辅助函数 ============== */

/**
 * @brief 获取当前时间（微秒）
 */
static uint64_t get_time_us(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000ULL) / freq.QuadPart);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
#endif
}

/**
 * @brief 获取CPU核心数
 */
static int get_cpu_count(void) {
#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (int)sysinfo.dwNumberOfProcessors;
#else
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    return (nprocs > 0) ? (int)nprocs : 4;
#endif
}

/* ============== 优先级队列操作 ============== */

static void priority_queue_init(PriorityTaskQueue *queue, int capacity) {
    queue->tasks = (Lv00Task **)malloc(capacity * sizeof(Lv00Task *));
    queue->count = 0;
    queue->capacity = capacity;
    LV00_MUTEX_INIT(queue->mutex);
}

static void priority_queue_destroy(PriorityTaskQueue *queue) {
    if (queue->tasks) {
        free(queue->tasks);
        queue->tasks = NULL;
    }
    LV00_MUTEX_DESTROY(queue->mutex);
    queue->count = 0;
    queue->capacity = 0;
}

static void priority_queue_push(PriorityTaskQueue *queue, Lv00Task *task) {
    LV00_MUTEX_LOCK(queue->mutex);

    /* 扩容检查 */
    if (queue->count >= queue->capacity) {
        int new_cap = queue->capacity * LV00_QUEUE_GROWTH_FACTOR;
        Lv00Task **new_tasks = (Lv00Task **)realloc(queue->tasks, new_cap * sizeof(Lv00Task *));
        if (!new_tasks) {
            LV00_MUTEX_UNLOCK(queue->mutex);
            return;
        }
        queue->tasks = new_tasks;
        queue->capacity = new_cap;
    }

    /* 按优先级插入（优先级高的在前） */
    int i = queue->count;
    while (i > 0 && queue->tasks[i - 1]->priority < task->priority) {
        queue->tasks[i] = queue->tasks[i - 1];
        i--;
    }
    queue->tasks[i] = task;
    queue->count++;

    LV00_MUTEX_UNLOCK(queue->mutex);
}

static Lv00Task *priority_queue_pop(PriorityTaskQueue *queue) {
    Lv00Task *task = NULL;

    LV00_MUTEX_LOCK(queue->mutex);

    if (queue->count > 0) {
        task = queue->tasks[0];
        queue->count--;
        /* 移动剩余任务 */
        for (int i = 0; i < queue->count; i++) {
            queue->tasks[i] = queue->tasks[i + 1];
        }
    }

    LV00_MUTEX_UNLOCK(queue->mutex);

    return task;
}

static bool priority_queue_is_empty(PriorityTaskQueue *queue) {
    LV00_MUTEX_LOCK(queue->mutex);
    bool empty = (queue->count == 0);
    LV00_MUTEX_UNLOCK(queue->mutex);
    return empty;
}

/* ============== 任务注册表 ============== */

static void task_registry_init(Lv00ThreadPool *pool) {
    pool->registry_capacity = LV00_POOL_QUEUE_INIT_CAP;
    pool->registry_count = 0;
    pool->task_registry = (Lv00Task **)calloc(pool->registry_capacity, sizeof(Lv00Task *));
}

static void task_registry_destroy(Lv00ThreadPool *pool) {
    if (pool->task_registry) {
        free(pool->task_registry);
        pool->task_registry = NULL;
    }
    pool->registry_count = 0;
    pool->registry_capacity = 0;
}

static void task_registry_add(Lv00ThreadPool *pool, Lv00Task *task) {
    if (pool->registry_count >= pool->registry_capacity) {
        int new_cap = pool->registry_capacity * LV00_QUEUE_GROWTH_FACTOR;
        Lv00Task **new_reg = (Lv00Task **)realloc(pool->task_registry, new_cap * sizeof(Lv00Task *));
        if (!new_reg) return;
        pool->task_registry = new_reg;
        pool->registry_capacity = new_cap;
    }
    pool->task_registry[pool->registry_count++] = task;
}

static Lv00Task *task_registry_find(Lv00ThreadPool *pool, uint64_t task_id) {
    for (int i = 0; i < pool->registry_count; i++) {
        if (pool->task_registry[i] && pool->task_registry[i]->id == task_id) {
            return pool->task_registry[i];
        }
    }
    return NULL;
}

static void task_registry_remove(Lv00ThreadPool *pool, uint64_t task_id) {
    for (int i = 0; i < pool->registry_count; i++) {
        if (pool->task_registry[i] && pool->task_registry[i]->id == task_id) {
            pool->task_registry[i] = NULL;
            break;
        }
    }
}

/* ============== 工作线程 ============== */

/**
 * @brief 尝试从其他工作线程窃取任务
 */
static Lv00Task *try_steal_task(WorkerThread *worker) {
    Lv00ThreadPool *pool = worker->pool;
    Lv00Task *task = NULL;

    if (!pool->enable_stealing) return NULL;

    /* 遍历其他工作线程的本地队列 */
    for (int i = 0; i < pool->thread_count; i++) {
        WorkerThread *other = pool->workers[i];
        if (other == worker || other->local_count == 0) continue;

        worker->steal_attempts++;

        /* 从队列尾部窃取 */
        if (other->local_count > 0) {
            task = other->local_queue[--other->local_count];
            worker->steal_successes++;
            pool->stats.steal_count++;
            break;
        }
    }

    return task;
}

/**
 * @brief 获取下一个待执行任务
 */
static Lv00Task *get_next_task(WorkerThread *worker) {
    Lv00ThreadPool *pool = worker->pool;
    Lv00Task *task = NULL;

    /* 1. 先从本地队列获取 */
    if (worker->local_count > 0) {
        task = worker->local_queue[--worker->local_count];
        return task;
    }

    /* 2. 从全局队列获取 */
    task = priority_queue_pop(&pool->global_queue);
    if (task) return task;

    /* 3. 尝试工作窃取 */
    task = try_steal_task(worker);

    return task;
}

/**
 * @brief 检查任务依赖是否满足
 */
static bool check_dependencies(Lv00ThreadPool *pool, Lv00Task *task) {
    if (task->depends_count == 0) return true;

    LV00_MUTEX_LOCK(pool->pool_mutex);

    for (int i = 0; i < task->depends_count; i++) {
        Lv00Task *dep = task_registry_find(pool, task->depends_on[i]);
        if (dep && dep->status != LV00_TASK_STATUS_COMPLETED) {
            LV00_MUTEX_UNLOCK(pool->pool_mutex);
            return false;
        }
    }

    LV00_MUTEX_UNLOCK(pool->pool_mutex);
    return true;
}

/**
 * @brief 执行任务
 */
static void execute_task(WorkerThread *worker, Lv00Task *task) {
    Lv00ThreadPool *pool = worker->pool;

    /* 检查依赖 */
    if (!check_dependencies(pool, task)) {
        /* 依赖未满足，放回队列 */
        priority_queue_push(&pool->global_queue, task);
        return;
    }

    /* 更新状态 */
    task->status = LV00_TASK_STATUS_RUNNING;
    task->start_time = get_time_us();

    /* 执行任务 */
    int result = LV00_ERROR_UNKNOWN;
    if (task->func) {
        result = task->func(task->user_data);
    }

    task->end_time = get_time_us();
    task->result = result;
    task->status = (result == 0) ? LV00_TASK_STATUS_COMPLETED : LV00_TASK_STATUS_FAILED;

    /* 更新统计 */
    worker->tasks_executed++;
    uint64_t exec_time = task->end_time - task->start_time;

    LV00_MUTEX_LOCK(pool->pool_mutex);
    pool->stats.tasks_completed++;
    if (result != 0) pool->stats.tasks_failed++;
    pool->stats.total_execute_time_us += exec_time;
    if (exec_time > pool->stats.max_task_time_us) {
        pool->stats.max_task_time_us = exec_time;
    }
    LV00_MUTEX_UNLOCK(pool->pool_mutex);

    /* 调用完成回调 */
    if (task->callback) {
        task->callback(task, result, task->callback_data);
    }

    /* 更新任务组 */
    if (task->group) {
        LV00_MUTEX_LOCK(pool->pool_mutex);
        task->group->completed_tasks++;
        if (result != 0) task->group->failed_tasks++;
        LV00_MUTEX_UNLOCK(pool->pool_mutex);
    }

    /* 通知等待者 */
    LV00_COND_BROADCAST(pool->task_completed);
}

/**
 * @brief 工作线程主函数
 */
#ifdef _WIN32
static DWORD WINAPI worker_thread_func(LPVOID arg)
#else
static void *worker_thread_func(void *arg)
#endif
{
    WorkerThread *worker = (WorkerThread *)arg;
    Lv00ThreadPool *pool = worker->pool;

    while (worker->running) {
        Lv00Task *task = NULL;

        LV00_MUTEX_LOCK(pool->pool_mutex);

        /* 等待任务或关闭信号 */
        while (priority_queue_is_empty(&pool->global_queue) &&
               worker->local_count == 0 &&
               !pool->shutdown) {
            pool->stats.idle_threads++;
            LV00_COND_WAIT(pool->task_available, pool->pool_mutex);
            pool->stats.idle_threads--;
        }

        /* 检查关闭信号 */
        if (pool->immediate_shutdown ||
            (pool->shutdown && priority_queue_is_empty(&pool->global_queue))) {
            LV00_MUTEX_UNLOCK(pool->pool_mutex);
            break;
        }

        pool->stats.active_threads++;
        LV00_MUTEX_UNLOCK(pool->pool_mutex);

        /* 获取并执行任务 */
        task = get_next_task(worker);
        if (task) {
            execute_task(worker, task);
        }

        LV00_MUTEX_LOCK(pool->pool_mutex);
        pool->stats.active_threads--;
        LV00_MUTEX_UNLOCK(pool->pool_mutex);
    }

    return LV99_THREAD_RETURN;
}

/* ============== 线程池生命周期 ============== */

void lv00_thread_pool_default_config(Lv00ThreadPoolConfig *config) {
    if (!config) return;
    memset(config, 0, sizeof(Lv00ThreadPoolConfig));
    config->thread_count = LV00_POOL_DEFAULT_THREADS;
    config->queue_capacity = LV00_POOL_QUEUE_INIT_CAP;
    config->enable_stealing = true;
    config->enable_affinity = false;
    config->name = "Lv00ThreadPool";
}

Lv00ThreadPool *lv00_thread_pool_create(const Lv00ThreadPoolConfig *config) {
    Lv00ThreadPoolConfig default_cfg;
    if (!config) {
        lv00_thread_pool_default_config(&default_cfg);
        config = &default_cfg;
    }

    Lv00ThreadPool *pool = (Lv00ThreadPool *)calloc(1, sizeof(Lv00ThreadPool));
    if (!pool) return NULL;

    /* 配置 */
    if (config->name) {
        strncpy(pool->name, config->name, sizeof(pool->name) - 1);
    } else {
        strcpy(pool->name, "Lv00ThreadPool");
    }

    pool->thread_count = (config->thread_count > 0) ?
                          config->thread_count : get_cpu_count();
    if (pool->thread_count > LV00_POOL_MAX_THREADS) {
        pool->thread_count = LV00_POOL_MAX_THREADS;
    }

    pool->enable_stealing = config->enable_stealing;
    pool->enable_affinity = config->enable_affinity;
    pool->running = true;
    pool->shutdown = false;
    pool->immediate_shutdown = false;
    pool->next_task_id = LV00_TASK_ID_INIT;

    /* 初始化同步原语 */
    LV00_MUTEX_INIT(pool->pool_mutex);
    LV00_COND_INIT(pool->task_available);
    LV00_COND_INIT(pool->task_completed);
    LV00_COND_INIT(pool->all_completed);

    /* 初始化全局队列 */
    int queue_cap = (config->queue_capacity > 0) ?
                     config->queue_capacity : LV00_POOL_QUEUE_INIT_CAP;
    priority_queue_init(&pool->global_queue, queue_cap);

    /* 初始化任务注册表 */
    task_registry_init(pool);

    /* 创建工作线程 */
    pool->workers = (WorkerThread **)calloc(pool->thread_count, sizeof(WorkerThread *));
    if (!pool->workers) {
        priority_queue_destroy(&pool->global_queue);
        task_registry_destroy(pool);
        LV00_MUTEX_DESTROY(pool->pool_mutex);
        free(pool);
        return NULL;
    }

    for (int i = 0; i < pool->thread_count; i++) {
        WorkerThread *worker = (WorkerThread *)calloc(1, sizeof(WorkerThread));
        if (!worker) continue;

        worker->id = i;
        worker->pool = pool;
        worker->running = true;
        worker->local_count = 0;
        worker->tasks_executed = 0;
        worker->steal_attempts = 0;
        worker->steal_successes = 0;

        pool->workers[i] = worker;

#ifdef _WIN32
        worker->thread = CreateThread(NULL, 0, worker_thread_func, worker, 0, NULL);
#else
        pthread_create(&worker->thread, NULL, worker_thread_func, worker);
#endif
        pool->active_workers++;
    }

    return pool;
}

void lv00_thread_pool_destroy(Lv00ThreadPool *pool, bool immediate) {
    if (!pool) return;

    /* 发送关闭信号 */
    LV00_MUTEX_LOCK(pool->pool_mutex);
    pool->shutdown = true;
    pool->immediate_shutdown = immediate;
    LV00_COND_BROADCAST(pool->task_available);
    LV00_MUTEX_UNLOCK(pool->pool_mutex);

    /* 等待所有工作线程退出 */
    for (int i = 0; i < pool->thread_count; i++) {
        WorkerThread *worker = pool->workers[i];
        if (!worker) continue;

        worker->running = false;

#ifdef _WIN32
        WaitForSingleObject(worker->thread, INFINITE);
        CloseHandle(worker->thread);
#else
        pthread_join(worker->thread, NULL);
#endif

        free(worker);
    }

    /* 清理资源 */
    free(pool->workers);
    priority_queue_destroy(&pool->global_queue);
    task_registry_destroy(pool);

    LV00_MUTEX_DESTROY(pool->pool_mutex);
    LV00_COND_DESTROY(pool->task_available);
    LV00_COND_DESTROY(pool->task_completed);
    LV00_COND_DESTROY(pool->all_completed);

    free(pool);
}

/* ============== 任务管理 ============== */

Lv00Task *lv00_task_create(Lv00TaskFunc func, void *user_data, const char *name) {
    Lv00Task *task = (Lv00Task *)calloc(1, sizeof(Lv00Task));
    if (!task) return NULL;

    task->func = func;
    task->user_data = user_data;
    task->priority = LV00_TASK_PRIORITY_NORMAL;
    task->status = LV00_TASK_STATUS_PENDING;
    task->submit_time = get_time_us();

    if (name) {
        strncpy(task->name, name, sizeof(task->name) - 1);
    }

    return task;
}

void lv00_task_destroy(Lv00Task *task) {
    if (!task) return;

    if (task->depends_on) {
        free(task->depends_on);
    }

    free(task);
}

void lv00_task_set_priority(Lv00Task *task, Lv00TaskPriority priority) {
    if (task) {
        task->priority = priority;
    }
}

void lv00_task_set_callback(Lv00Task *task, Lv00TaskCallback callback, void *callback_data) {
    if (task) {
        task->callback = callback;
        task->callback_data = callback_data;
    }
}

int lv00_task_add_dependency(Lv00Task *task, uint64_t depends_on_task_id) {
    if (!task) return -1;

    int new_count = task->depends_count + 1;
    uint64_t *new_deps = (uint64_t *)realloc(task->depends_on, new_count * sizeof(uint64_t));
    if (!new_deps) return -1;

    task->depends_on = new_deps;
    task->depends_on[task->depends_count] = depends_on_task_id;
    task->depends_count++;
    task->depends_remaining++;

    return 0;
}

uint64_t lv00_thread_pool_submit(Lv00ThreadPool *pool, Lv00Task *task) {
    if (!pool || !task || !pool->running) {
        if (task) lv00_task_destroy(task);
        return 0;
    }

    LV00_MUTEX_LOCK(pool->pool_mutex);

    /* 分配任务ID */
    task->id = pool->next_task_id++;

    /* 注册任务 */
    task_registry_add(pool, task);

    /* 更新统计 */
    pool->stats.tasks_submitted++;

    /* 加入全局队列 */
    priority_queue_push(&pool->global_queue, task);

    /* 通知工作线程 */
    LV00_COND_SIGNAL(pool->task_available);

    uint64_t task_id = task->id;

    LV00_MUTEX_UNLOCK(pool->pool_mutex);

    return task_id;
}

uint64_t lv00_thread_pool_submit_simple(Lv00ThreadPool *pool,
                                         Lv00TaskFunc func,
                                         void *user_data,
                                         Lv00TaskPriority priority) {
    Lv00Task *task = lv00_task_create(func, user_data, NULL);
    if (!task) return 0;

    lv00_task_set_priority(task, priority);
    return lv00_thread_pool_submit(pool, task);
}

/* ============== 任务等待 ============== */

int lv00_thread_pool_wait_task(Lv00ThreadPool *pool, uint64_t task_id, uint32_t timeout_ms) {
    if (!pool) return LV00_ERROR_INVALID_PARAM;

    LV00_MUTEX_LOCK(pool->pool_mutex);

    Lv00Task *task = task_registry_find(pool, task_id);
    if (!task) {
        LV00_MUTEX_UNLOCK(pool->pool_mutex);
        return LV00_ERROR_NOT_FOUND;
    }

    /* 等待任务完成 */
    while (task->status != LV00_TASK_STATUS_COMPLETED &&
           task->status != LV00_TASK_STATUS_FAILED &&
           task->status != LV00_TASK_STATUS_CANCELLED) {

        if (timeout_ms > 0) {
#ifdef _WIN32
            /* Windows 条件变量超时等待 */
            if (!SleepConditionVariableCS(&pool->task_completed, &pool->pool_mutex, timeout_ms)) {
                LV00_MUTEX_UNLOCK(pool->pool_mutex);
                return LV00_ERROR_TIMEOUT;
            }
#else
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
            ts.tv_sec += timeout_ms / 1000 + ts.tv_nsec / 1000000000L;
            ts.tv_nsec %= 1000000000L;

            if (pthread_cond_timedwait(&pool->task_completed, &pool->pool_mutex, &ts) != 0) {
                LV00_MUTEX_UNLOCK(pool->pool_mutex);
                return LV00_ERROR_TIMEOUT;
            }
#endif
        } else {
            LV00_COND_WAIT(pool->task_completed, pool->pool_mutex);
        }
    }

    int result = task->result;
    LV00_MUTEX_UNLOCK(pool->pool_mutex);

    return result;
}

int lv00_thread_pool_wait_all(Lv00ThreadPool *pool, uint32_t timeout_ms) {
    if (!pool) return LV00_ERROR_INVALID_PARAM;

    LV00_MUTEX_LOCK(pool->pool_mutex);

    uint64_t start_time = get_time_us();
    uint64_t deadline = (timeout_ms > 0) ? start_time + timeout_ms * 1000ULL : 0;

    while (pool->global_queue.count > 0 || pool->stats.active_threads > 0) {
        if (timeout_ms > 0) {
            uint64_t now = get_time_us();
            if (now >= deadline) {
                LV00_MUTEX_UNLOCK(pool->pool_mutex);
                return LV00_ERROR_TIMEOUT;
            }
        }

        if (timeout_ms > 0) {
#ifdef _WIN32
            SleepConditionVariableCS(&pool->all_completed, &pool->pool_mutex, 100);
#else
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 100000000L; /* 100ms */
            pthread_cond_timedwait(&pool->all_completed, &pool->pool_mutex, &ts);
#endif
        } else {
            LV00_COND_WAIT(pool->all_completed, pool->pool_mutex);
        }
    }

    LV00_MUTEX_UNLOCK(pool->pool_mutex);
    return 0;
}

int lv00_thread_pool_cancel_task(Lv00ThreadPool *pool, uint64_t task_id) {
    if (!pool) return LV00_ERROR_INVALID_PARAM;

    LV00_MUTEX_LOCK(pool->pool_mutex);

    Lv00Task *task = task_registry_find(pool, task_id);
    if (!task || task->status != LV00_TASK_STATUS_PENDING) {
        LV00_MUTEX_UNLOCK(pool->pool_mutex);
        return LV00_ERROR_INVALID_STATE;
    }

    task->status = LV00_TASK_STATUS_CANCELLED;
    pool->stats.tasks_cancelled++;

    task_registry_remove(pool, task_id);

    LV00_MUTEX_UNLOCK(pool->pool_mutex);

    lv00_task_destroy(task);
    return 0;
}

/* ============== 任务组 ============== */

Lv00TaskGroup *lv00_task_group_create(const char *name) {
    Lv00TaskGroup *group = (Lv00TaskGroup *)calloc(1, sizeof(Lv00TaskGroup));
    if (!group) return NULL;

    if (name) {
        strncpy(group->name, name, sizeof(group->name) - 1);
    }

    return group;
}

void lv00_task_group_destroy(Lv00TaskGroup *group) {
    if (group) {
        free(group);
    }
}

int lv00_task_group_add(Lv00TaskGroup *group, Lv00Task *task) {
    if (!group || !task) return LV00_ERROR_INVALID_PARAM;

    task->group = group;
    group->total_tasks++;

    return 0;
}

int lv00_thread_pool_submit_group(Lv00ThreadPool *pool, Lv00TaskGroup *group) {
    if (!pool || !group) return LV00_ERROR_INVALID_PARAM;

    int submitted = 0;

    LV00_MUTEX_LOCK(pool->pool_mutex);

    for (int i = 0; i < pool->registry_count; i++) {
        Lv00Task *task = pool->task_registry[i];
        if (task && task->group == group && task->status == LV00_TASK_STATUS_PENDING) {
            priority_queue_push(&pool->global_queue, task);
            LV00_COND_SIGNAL(pool->task_available);
            submitted++;
        }
    }

    group->all_submitted = true;

    LV00_MUTEX_UNLOCK(pool->pool_mutex);

    return submitted;
}

int lv00_thread_pool_wait_group(Lv00ThreadPool *pool,
                                 Lv00TaskGroup *group,
                                 uint32_t timeout_ms) {
    if (!pool || !group) return LV00_ERROR_INVALID_PARAM;

    LV00_MUTEX_LOCK(pool->pool_mutex);

    uint64_t start_time = get_time_us();
    uint64_t deadline = (timeout_ms > 0) ? start_time + timeout_ms * 1000ULL : 0;

    while (group->completed_tasks < group->total_tasks && !group->wait_cancelled) {
        if (timeout_ms > 0) {
            uint64_t now = get_time_us();
            if (now >= deadline) {
                LV00_MUTEX_UNLOCK(pool->pool_mutex);
                return LV00_ERROR_TIMEOUT;
            }
        }

        if (timeout_ms > 0) {
#ifdef _WIN32
            SleepConditionVariableCS(&pool->task_completed, &pool->pool_mutex, 100);
#else
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 100000000L;
            pthread_cond_timedwait(&pool->task_completed, &pool->pool_mutex, &ts);
#endif
        } else {
            LV00_COND_WAIT(pool->task_completed, pool->pool_mutex);
        }
    }

    int result = group->wait_cancelled ? LV00_ERROR_CANCELLED : 0;
    LV00_MUTEX_UNLOCK(pool->pool_mutex);

    return result;
}

/* ============== 并行算法 ============== */

/* 并行 for 循环的迭代任务数据 */
typedef struct {
    int64_t index;
    void (*func)(int64_t index, void *user_data);
    void *user_data;
} ParallelForData;

static int parallel_for_task_func(void *data) {
    ParallelForData *pfd = (ParallelForData *)data;
    if (pfd && pfd->func) {
        pfd->func(pfd->index, pfd->user_data);
    }
    free(pfd);
    return 0;
}

int lv00_thread_pool_parallel_for(Lv00ThreadPool *pool,
                                   int64_t start,
                                   int64_t end,
                                   int64_t step,
                                   void (*func)(int64_t index, void *user_data),
                                   void *user_data) {
    if (!pool || !func) return LV00_ERROR_INVALID_PARAM;

    Lv00TaskGroup *group = lv00_task_group_create("parallel_for");
    if (!group) return LV00_ERROR_OUT_OF_MEMORY;

    /* 为每个迭代创建任务 */
    for (int64_t i = start; i < end; i += step) {
        ParallelForData *data = (ParallelForData *)malloc(sizeof(ParallelForData));
        if (!data) continue;

        data->index = i;
        data->func = func;
        data->user_data = user_data;

        Lv00Task *task = lv00_task_create(parallel_for_task_func, data, NULL);
        if (task) {
            lv00_task_group_add(group, task);
            lv00_thread_pool_submit(pool, task);
        }
    }

    /* 等待所有迭代完成 */
    int result = lv00_thread_pool_wait_group(pool, group, 0);
    lv00_task_group_destroy(group);

    return result;
}

int lv00_thread_pool_parallel_map(Lv00ThreadPool *pool,
                                   void *items,
                                   size_t count,
                                   size_t item_size,
                                   void (*func)(void *item, size_t index, void *user_data),
                                   void *user_data) {
    if (!pool || !items || !func) return LV00_ERROR_INVALID_PARAM;

    /* 使用 parallel_for 实现 */
    char *base = (char *)items;
    for (size_t i = 0; i < count; i++) {
        func(base + i * item_size, i, user_data);
    }

    return 0;
}

int lv00_thread_pool_parallel_reduce(Lv00ThreadPool *pool,
                                      const void *items,
                                      size_t count,
                                      size_t item_size,
                                      void *init_value,
                                      void (*reduce_func)(void *acc, const void *item, void *user_data),
                                      void *user_data,
                                      void *result) {
    if (!pool || !items || !reduce_func) return LV00_ERROR_INVALID_PARAM;

    /* 简化实现：顺序归约 */
    char *acc = (char *)init_value;
    const char *base = (const char *)items;

    for (size_t i = 0; i < count; i++) {
        reduce_func(acc, base + i * item_size, user_data);
    }

    if (result) {
        memcpy(result, init_value, item_size);
    }

    return 0;
}

/* ============== 统计与诊断 ============== */

void lv00_thread_pool_get_stats(const Lv00ThreadPool *pool, Lv00ThreadPoolStats *stats) {
    if (!pool || !stats) return;
    memcpy(stats, &pool->stats, sizeof(Lv00ThreadPoolStats));
}

void lv00_thread_pool_reset_stats(Lv00ThreadPool *pool) {
    if (!pool) return;
    memset(&pool->stats, 0, sizeof(Lv00ThreadPoolStats));
}

int lv00_thread_pool_get_thread_count(const Lv00ThreadPool *pool) {
    return pool ? pool->thread_count : 0;
}

int lv00_thread_pool_get_queue_size(const Lv00ThreadPool *pool) {
    if (!pool) return 0;
    LV00_MUTEX_LOCK(pool->pool_mutex);
    int size = pool->global_queue.count;
    LV00_MUTEX_UNLOCK(pool->pool_mutex);
    return size;
}

void lv00_thread_pool_print_diag(const Lv00ThreadPool *pool, void *stream) {
    if (!pool) return;
    FILE *f = stream ? (FILE *)stream : stdout;

    fprintf(f, "\n========== Lv-00 线程池诊断 ==========\n");
    fprintf(f, "名称: %s\n", pool->name);
    fprintf(f, "工作线程数: %d\n", pool->thread_count);
    fprintf(f, "状态: %s\n", pool->running ? "运行中" : "已停止");

    fprintf(f, "\n--- 任务统计 ---\n");
    fprintf(f, "已提交: %llu\n", (unsigned long long)pool->stats.tasks_submitted);
    fprintf(f, "已完成: %llu\n", (unsigned long long)pool->stats.tasks_completed);
    fprintf(f, "失败: %llu\n", (unsigned long long)pool->stats.tasks_failed);
    fprintf(f, "取消: %llu\n", (unsigned long long)pool->stats.tasks_cancelled);

    fprintf(f, "\n--- 时间统计 ---\n");
    fprintf(f, "总执行时间: %llu us\n", (unsigned long long)pool->stats.total_execute_time_us);
    fprintf(f, "最大单任务时间: %llu us\n", (unsigned long long)pool->stats.max_task_time_us);

    fprintf(f, "\n--- 队列统计 ---\n");
    fprintf(f, "当前队列长度: %d\n", pool->global_queue.count);
    fprintf(f, "历史最大长度: %llu\n", (unsigned long long)pool->stats.queue_max_size);
    fprintf(f, "工作窃取次数: %llu\n", (unsigned long long)pool->stats.steal_count);

    fprintf(f, "\n--- 线程统计 ---\n");
    fprintf(f, "活跃线程: %d\n", pool->stats.active_threads);
    fprintf(f, "空闲线程: %d\n", pool->stats.idle_threads);

    fprintf(f, "======================================\n\n");
}

/* ============== 全局线程池 ============== */

static Lv00ThreadPool *g_global_pool = NULL;

int lv00_init_global_thread_pool(const Lv00ThreadPoolConfig *config) {
    if (g_global_pool) return LV00_ERROR_INVALID_STATE;

    g_global_pool = lv00_thread_pool_create(config);
    return g_global_pool ? 0 : LV00_ERROR_OUT_OF_MEMORY;
}

void lv00_cleanup_global_thread_pool(void) {
    if (g_global_pool) {
        lv00_thread_pool_destroy(g_global_pool, false);
        g_global_pool = NULL;
    }
}

Lv00ThreadPool *lv00_get_global_thread_pool(void) {
    return g_global_pool;
}
