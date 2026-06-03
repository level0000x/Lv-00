/**
 * @file thread_pool.h
 * @brief 线程池系统 —— 高性能并发任务调度基础设施
 *
 * @details 提供轻量级线程池实现，支持：
 *   1. 工作窃取调度（Work-Stealing）
 *   2. 任务优先级
 *   3. 任务依赖与屏障
 *   4. 线程局部存储
 *   5. 优雅关闭
 *
 * 设计目标：
 *   - 低延迟任务提交
 *   - 高吞吐量并行执行
 *   - 负载均衡
 *   - 可扩展性
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#ifndef LV00_THREAD_POOL_H
#define LV00_THREAD_POOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============== 配置常量 ============== */

/** 默认工作线程数量（0 = 自动检测CPU核心数） */
#define LV00_POOL_DEFAULT_THREADS 0

/** 最大工作线程数量 */
#define LV00_POOL_MAX_THREADS 256

/** 任务队列初始容量 */
#define LV00_POOL_QUEUE_INIT_CAP 1024

/** 工作窃取队列容量 */
#define LV00_POOL_WORKER_QUEUE_CAP 256

/** 任务名称最大长度 */
#define LV00_POOL_TASK_NAME_LEN 64

/* ============== 前向声明 ============== */

typedef struct Lv00ThreadPool Lv00ThreadPool;
typedef struct Lv00Task Lv00Task;
typedef struct Lv00TaskGroup Lv00TaskGroup;
typedef struct Lv00ThreadPoolStats Lv00ThreadPoolStats;

/* ============== 任务类型 ============== */

/**
 * @brief 任务优先级
 */
typedef enum {
    LV00_TASK_PRIORITY_LOW = 0,     /**< 低优先级 */
    LV00_TASK_PRIORITY_NORMAL = 1,  /**< 普通优先级（默认） */
    LV00_TASK_PRIORITY_HIGH = 2,    /**< 高优先级 */
    LV00_TASK_PRIORITY_URGENT = 3   /**< 紧急优先级 */
} Lv00TaskPriority;

/**
 * @brief 任务状态
 */
typedef enum {
    LV00_TASK_STATUS_PENDING = 0,   /**< 等待执行 */
    LV00_TASK_STATUS_RUNNING = 1,   /**< 正在执行 */
    LV00_TASK_STATUS_COMPLETED = 2, /**< 已完成 */
    LV00_TASK_STATUS_FAILED = 3,    /**< 执行失败 */
    LV00_TASK_STATUS_CANCELLED = 4  /**< 已取消 */
} Lv00TaskStatus;

/**
 * @brief 任务执行函数类型
 *
 * @param user_data 用户数据
 * @return 任务结果码（0 = 成功，非零 = 失败）
 */
typedef int (*Lv00TaskFunc)(void *user_data);

/**
 * @brief 任务完成回调
 *
 * @param task 任务指针
 * @param result 任务结果码
 * @param callback_data 回调数据
 */
typedef void (*Lv00TaskCallback)(Lv00Task *task, int result, void *callback_data);

/* ============== 任务结构 ============== */

/**
 * @brief 任务结构
 */
struct Lv00Task {
    /* 任务标识 */
    uint64_t id;                        /**< 任务唯一ID */
    char name[LV00_POOL_TASK_NAME_LEN]; /**< 任务名称 */
    Lv00TaskPriority priority;          /**< 优先级 */
    Lv00TaskStatus status;              /**< 当前状态 */

    /* 执行函数 */
    Lv00TaskFunc func;      /**< 任务函数 */
    void *user_data;        /**< 用户数据 */
    Lv00TaskCallback callback; /**< 完成回调 */
    void *callback_data;    /**< 回调数据 */

    /* 依赖管理 */
    uint64_t *depends_on;   /**< 依赖的任务ID数组 */
    int depends_count;      /**< 依赖数量 */
    int depends_remaining;  /**< 剩余未完成的依赖数 */

    /* 任务组 */
    Lv00TaskGroup *group;   /**< 所属任务组（可为NULL） */

    /* 执行结果 */
    int result;             /**< 执行结果码 */
    uint64_t submit_time;   /**< 提交时间（微秒） */
    uint64_t start_time;    /**< 开始时间（微秒） */
    uint64_t end_time;      /**< 结束时间（微秒） */

    /* 内部链接 */
    struct Lv00Task *next;  /**< 链表下一个节点 */
};

/* ============== 任务组 ============== */

/**
 * @brief 任务组 —— 用于批量管理和等待任务
 */
struct Lv00TaskGroup {
    uint64_t id;            /**< 组ID */
    char name[64];          /**< 组名称 */

    /* 任务统计 */
    int total_tasks;        /**< 总任务数 */
    int completed_tasks;    /**< 已完成任务数 */
    int failed_tasks;       /**< 失败任务数 */

    /* 状态 */
    bool all_submitted;     /**< 是否所有任务已提交 */
    bool wait_cancelled;    /**< 等待是否被取消 */

    /* 同步 */
    void *completion_event; /**< 完成事件（平台相关） */
};

/* ============== 线程池统计 ============== */

/**
 * @brief 线程池统计信息
 */
struct Lv00ThreadPoolStats {
    /* 任务统计 */
    uint64_t tasks_submitted;    /**< 已提交任务数 */
    uint64_t tasks_completed;    /**< 已完成任务数 */
    uint64_t tasks_failed;       /**< 失败任务数 */
    uint64_t tasks_cancelled;    /**< 取消任务数 */

    /* 时间统计 */
    uint64_t total_execute_time_us; /**< 总执行时间（微秒） */
    uint64_t max_task_time_us;      /**< 单任务最大执行时间 */

    /* 队列统计 */
    uint64_t queue_max_size;    /**< 队列历史最大长度 */
    uint64_t steal_count;       /**< 工作窃取次数 */

    /* 线程统计 */
    int active_threads;         /**< 活跃线程数 */
    int idle_threads;           /**< 空闲线程数 */
};

/* ============== 线程池配置 ============== */

/**
 * @brief 线程池配置
 */
typedef struct {
    int thread_count;       /**< 工作线程数量（0 = 自动） */
    int queue_capacity;     /**< 任务队列容量 */
    bool enable_stealing;   /**< 是否启用工作窃取 */
    bool enable_affinity;   /**< 是否启用CPU亲和性 */
    const char *name;       /**< 线程池名称（调试用） */
} Lv00ThreadPoolConfig;

/* ============== 线程池生命周期 ============== */

/**
 * @brief 创建线程池
 *
 * @param config 配置参数（可为NULL使用默认值）
 * @return 新创建的线程池，失败返回 NULL
 */
Lv00ThreadPool *lv00_thread_pool_create(const Lv00ThreadPoolConfig *config);

/**
 * @brief 销毁线程池
 *
 * 等待所有任务完成后销毁。如果 immediate 为 true，
 * 则立即取消所有待执行任务。
 *
 * @param pool 线程池
 * @param immediate 是否立即销毁
 */
void lv00_thread_pool_destroy(Lv00ThreadPool *pool, bool immediate);

/**
 * @brief 获取默认配置
 *
 * @param config 输出配置结构
 */
void lv00_thread_pool_default_config(Lv00ThreadPoolConfig *config);

/* ============== 任务提交 ============== */

/**
 * @brief 创建任务
 *
 * @param func 任务函数
 * @param user_data 用户数据
 * @param name 任务名称（可为NULL）
 * @return 新创建的任务，失败返回 NULL
 */
Lv00Task *lv00_task_create(Lv00TaskFunc func, void *user_data, const char *name);

/**
 * @brief 销毁任务
 *
 * @param task 任务指针
 */
void lv00_task_destroy(Lv00Task *task);

/**
 * @brief 设置任务优先级
 *
 * @param task 任务
 * @param priority 优先级
 */
void lv00_task_set_priority(Lv00Task *task, Lv00TaskPriority priority);

/**
 * @brief 设置任务完成回调
 *
 * @param task 任务
 * @param callback 回调函数
 * @param callback_data 回调数据
 */
void lv00_task_set_callback(Lv00Task *task, Lv00TaskCallback callback, void *callback_data);

/**
 * @brief 添加任务依赖
 *
 * 当前任务将在依赖的任务完成后才开始执行。
 *
 * @param task 当前任务
 * @param depends_on_task_id 依赖的任务ID
 * @return 成功返回 0，失败返回 -1
 */
int lv00_task_add_dependency(Lv00Task *task, uint64_t depends_on_task_id);

/**
 * @brief 提交任务到线程池
 *
 * @param pool 线程池
 * @param task 任务（线程池取得所有权）
 * @return 任务ID，失败返回 0
 */
uint64_t lv00_thread_pool_submit(Lv00ThreadPool *pool, Lv00Task *task);

/**
 * @brief 提交简单任务
 *
 * 便捷函数，自动创建任务并提交。
 *
 * @param pool 线程池
 * @param func 任务函数
 * @param user_data 用户数据
 * @param priority 优先级
 * @return 任务ID，失败返回 0
 */
uint64_t lv00_thread_pool_submit_simple(Lv00ThreadPool *pool,
                                         Lv00TaskFunc func,
                                         void *user_data,
                                         Lv00TaskPriority priority);

/* ============== 任务等待 ============== */

/**
 * @brief 等待单个任务完成
 *
 * @param pool 线程池
 * @param task_id 任务ID
 * @param timeout_ms 超时时间（毫秒，0 = 无限等待）
 * @return 任务结果码，超时返回 LV00_ERROR_TIMEOUT
 */
int lv00_thread_pool_wait_task(Lv00ThreadPool *pool, uint64_t task_id, uint32_t timeout_ms);

/**
 * @brief 等待所有任务完成
 *
 * @param pool 线程池
 * @param timeout_ms 超时时间（毫秒，0 = 无限等待）
 * @return 成功返回 0，超时返回 LV00_ERROR_TIMEOUT
 */
int lv00_thread_pool_wait_all(Lv00ThreadPool *pool, uint32_t timeout_ms);

/**
 * @brief 取消任务
 *
 * 只能取消尚未开始执行的任务。
 *
 * @param pool 线程池
 * @param task_id 任务ID
 * @return 成功返回 0，任务已执行或不存在返回 -1
 */
int lv00_thread_pool_cancel_task(Lv00ThreadPool *pool, uint64_t task_id);

/* ============== 任务组 ============== */

/**
 * @brief 创建任务组
 *
 * @param name 组名称（可为NULL）
 * @return 新创建的任务组，失败返回 NULL
 */
Lv00TaskGroup *lv00_task_group_create(const char *name);

/**
 * @brief 销毁任务组
 *
 * @param group 任务组
 */
void lv00_task_group_destroy(Lv00TaskGroup *group);

/**
 * @brief 将任务添加到任务组
 *
 * @param group 任务组
 * @param task 任务
 * @return 成功返回 0
 */
int lv00_task_group_add(Lv00TaskGroup *group, Lv00Task *task);

/**
 * @brief 提交任务组中的所有任务
 *
 * @param pool 线程池
 * @param group 任务组
 * @return 成功提交的任务数量
 */
int lv00_thread_pool_submit_group(Lv00ThreadPool *pool, Lv00TaskGroup *group);

/**
 * @brief 等待任务组中所有任务完成
 *
 * @param pool 线程池
 * @param group 任务组
 * @param timeout_ms 超时时间（毫秒，0 = 无限等待）
 * @return 成功返回 0，超时返回 LV00_ERROR_TIMEOUT
 */
int lv00_thread_pool_wait_group(Lv00ThreadPool *pool,
                                 Lv00TaskGroup *group,
                                 uint32_t timeout_ms);

/* ============== 并行算法 ============== */

/**
 * @brief 并行 for 循环
 *
 * 将循环迭代分配到多个线程执行。
 *
 * @param pool 线程池
 * @param start 起始索引
 * @param end 结束索引（不包含）
 * @param step 步长
 * @param func 迭代函数 (index, user_data)
 * @param user_data 用户数据
 * @return 成功返回 0
 */
int lv00_thread_pool_parallel_for(Lv00ThreadPool *pool,
                                   int64_t start,
                                   int64_t end,
                                   int64_t step,
                                   void (*func)(int64_t index, void *user_data),
                                   void *user_data);

/**
 * @brief 并行映射
 *
 * 对数组中的每个元素应用函数。
 *
 * @param pool 线程池
 * @param items 元素数组
 * @param count 元素数量
 * @param item_size 单个元素大小
 * @param func 映射函数 (item, index, user_data)
 * @param user_data 用户数据
 * @return 成功返回 0
 */
int lv00_thread_pool_parallel_map(Lv00ThreadPool *pool,
                                   void *items,
                                   size_t count,
                                   size_t item_size,
                                   void (*func)(void *item, size_t index, void *user_data),
                                   void *user_data);

/**
 * @brief 并行归约
 *
 * @param pool 线程池
 * @param items 元素数组
 * @param count 元素数量
 * @param item_size 单个元素大小
 * @param init_value 初始值
 * @param reduce_func 归约函数 (accumulator, item, user_data)
 * @param user_data 用户数据
 * @param result 输出结果
 * @return 成功返回 0
 */
int lv00_thread_pool_parallel_reduce(Lv00ThreadPool *pool,
                                      const void *items,
                                      size_t count,
                                      size_t item_size,
                                      void *init_value,
                                      void (*reduce_func)(void *acc, const void *item, void *user_data),
                                      void *user_data,
                                      void *result);

/* ============== 统计与诊断 ============== */

/**
 * @brief 获取线程池统计信息
 *
 * @param pool 线程池
 * @param stats 输出统计结构
 */
void lv00_thread_pool_get_stats(const Lv00ThreadPool *pool, Lv00ThreadPoolStats *stats);

/**
 * @brief 重置统计信息
 *
 * @param pool 线程池
 */
void lv00_thread_pool_reset_stats(Lv00ThreadPool *pool);

/**
 * @brief 获取线程数量
 *
 * @param pool 线程池
 * @return 工作线程数量
 */
int lv00_thread_pool_get_thread_count(const Lv00ThreadPool *pool);

/**
 * @brief 获取当前队列长度
 *
 * @param pool 线程池
 * @return 待执行任务数量
 */
int lv00_thread_pool_get_queue_size(const Lv00ThreadPool *pool);

/**
 * @brief 打印线程池诊断信息
 *
 * @param pool 线程池
 * @param stream 输出流（如 stdout）
 */
void lv00_thread_pool_print_diag(const Lv00ThreadPool *pool, void *stream);

/* ============== 全局线程池 ============== */

/**
 * @brief 初始化全局线程池
 *
 * 应用程序启动时调用一次。
 *
 * @param config 配置（可为NULL使用默认值）
 * @return 成功返回 0
 */
int lv00_init_global_thread_pool(const Lv00ThreadPoolConfig *config);

/**
 * @brief 清理全局线程池
 *
 * 应用程序退出时调用。
 */
void lv00_cleanup_global_thread_pool(void);

/**
 * @brief 获取全局线程池
 *
 * @return 全局线程池，未初始化返回 NULL
 */
Lv00ThreadPool *lv00_get_global_thread_pool(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_THREAD_POOL_H */
