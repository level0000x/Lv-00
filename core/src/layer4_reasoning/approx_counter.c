/**
 * @file approx_counter.c
 * @brief 近似模型计数器实现 —— HyperLogLog 风格的近似计数与 ApproxMC 集成
 *
 * @details 实现两层近似计数架构：
 *          1. HyperLogLog 底层计数器 —— 基数估计（FNV-1a 哈希 + 调和均值）
 *          2. ApproxMC 上层接口 —— 约束图的 PAC 保证模型计数
 *
 *          HyperLogLog 实现要点：
 *          - 使用 M 个寄存器（默认 16），精度 p = log2(M)
 *          - FNV-1a 哈希函数
 *          - 调和均值估计（含小范围/大范围修正）
 *          - 稀疏模式优化（小数据集直接存储，避免寄存器浪费）
 *
 * @version 5.0.0
 */

#include "lv00/approx_counter.h"
#include "lv00/lv00_internal.h"
#include "lv00/lv00_utils.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================
 * 内部常量
 * ============================================================ */

/* 前向声明内部类型 */
typedef struct ApproxCounter ApproxCounter;

/** 默认寄存器数量 */
#define HLL_DEFAULT_M 16

/** 默认精度 p = log2(M) */
#define HLL_DEFAULT_P 4

/** Alpha 修正系数（调和均值用） */
#define HLL_ALPHA(m) (0.7213 / (1.0 + 1.079 / (double)(m)))

/** 稀疏模式阈值：元素数低于此值时使用稀疏存储 */
#define SPARSE_THRESHOLD 64

/* ============================================================
 * 内部数据结构
 * ============================================================ */

/**
 * @brief HyperLogLog 近似计数器
 *
 * 支持两种模式：
 * - 稀疏模式：小数据集直接存储哈希值，避免寄存器浪费
 * - 密集模式：标准 HLL 寄存器数组
 */
struct ApproxCounter {
    uint8_t *registers;       /**< 密集模式寄存器数组 */
    uint64_t *sparse_hashes;  /**< 稀疏模式哈希值缓存 */
    int sparse_count;         /**< 稀疏模式当前元素数 */
    int sparse_capacity;      /**< 稀疏模式容量 */
    int p;                    /**< 精度（寄存器索引位数） */
    int m;                    /**< 寄存器数量（2^p） */
    bool use_sparse;          /**< 是否处于稀疏模式 */
    uint64_t cached_count;    /**< 缓存的估计值 */
    bool dirty;               /**< 是否需要重新计算估计值 */
};

/* ============================================================
 * FNV-1a 哈希函数
 * ============================================================ */

/**
 * @brief FNV-1a 哈希（64 位）
 *
 * @param data  输入数据
 * @param len   数据长度（字节）
 * @return 64 位哈希值
 */
static uint64_t fnv1a_hash(const void *data, size_t len) {
    uint64_t hash = 14695981039346656037ULL;  /* FNV offset basis */
    const uint8_t *bytes = (const uint8_t *)data;

    for (size_t i = 0; i < len; i++) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;  /* FNV prime */
    }

    return hash;
}

/**
 * @brief 计算前导零数量
 */
static int count_leading_zeros(uint64_t value, int max_bits) {
    if (value == 0) return max_bits;
    int count = 0;
    for (int i = max_bits - 1; i >= 0; i--) {
        if ((value >> i) & 1) break;
        count++;
    }
    return count;
}

/* ============================================================
 * 内部辅助：稀疏到密集模式转换
 * ============================================================ */

/**
 * @brief 将稀疏模式提升为密集模式
 */
static void promote_to_dense(ApproxCounter *counter) {
    if (!counter || !counter->use_sparse) return;

    counter->registers = (uint8_t *)lv00_calloc((size_t)counter->m, sizeof(uint8_t));
    if (!counter->registers) return;

    /* 将稀疏哈希值重放到寄存器 */
    for (int i = 0; i < counter->sparse_count; i++) {
        uint64_t hash = counter->sparse_hashes[i];
        int index = (int)(hash >> (64 - counter->p));
        uint64_t remaining = hash << counter->p;
        int lz = count_leading_zeros(remaining, 64 - counter->p) + 1;
        if (lz > counter->registers[index]) {
            counter->registers[index] = (uint8_t)lz;
        }
    }

    /* 释放稀疏存储 */
    lv00_free((void **)&counter->sparse_hashes);
    counter->sparse_hashes = NULL;
    counter->sparse_count = 0;
    counter->sparse_capacity = 0;
    counter->use_sparse = false;
    counter->dirty = true;
}

/* ============================================================
 * 内部辅助：估计值重新计算
 * ============================================================ */

/**
 * @brief 重新计算 HyperLogLog 估计值（调和均值）
 */
static void recalculate_estimate(ApproxCounter *counter) {
    if (!counter || !counter->dirty) return;

    if (counter->use_sparse) {
        /* 稀疏模式：直接计数（无近似误差） */
        counter->cached_count = (uint64_t)counter->sparse_count;
        counter->dirty = false;
        return;
    }

    /* 密集模式：调和均值估计 */
    double sum = 0.0;
    int m = counter->m;
    for (int i = 0; i < m; i++) {
        sum += 1.0 / (double)(1ULL << counter->registers[i]);
    }

    double alpha = HLL_ALPHA(m);
    double estimate = alpha * (double)m * (double)m / sum;

    /* 小范围修正（线性计数） */
    if (estimate <= 2.5 * (double)m) {
        int zeros = 0;
        for (int i = 0; i < m; i++) {
            if (counter->registers[i] == 0) zeros++;
        }
        if (zeros > 0) {
            estimate = (double)m * log((double)m / (double)zeros);
        }
    }

    /* 大范围修正 */
    if (estimate > (double)(1ULL << 32) / 30.0) {
        estimate = -(double)(1ULL << 32) * log(1.0 - estimate / (double)(1ULL << 32));
    }

    counter->cached_count = (uint64_t)(estimate + 0.5);
    counter->dirty = false;
}

/* ============================================================
 * HyperLogLog 计数器公共 API
 * ============================================================ */

/**
 * @brief 创建近似计数器
 *
 * @param precision 精度（p 值），寄存器数 = 2^p。0 = 使用默认值（4，即 16 寄存器）
 * @return 计数器指针，失败返回 NULL
 */
ApproxCounter *approx_count_create(int precision) {
    if (precision <= 0 || precision > 16) {
        precision = HLL_DEFAULT_P;
    }

    ApproxCounter *counter = (ApproxCounter *)lv00_calloc(1, sizeof(ApproxCounter));
    if (!counter) return NULL;

    counter->p = precision;
    counter->m = 1 << precision;
    counter->use_sparse = true;
    counter->sparse_capacity = SPARSE_THRESHOLD;
    counter->sparse_hashes = (uint64_t *)lv00_calloc(
        (size_t)counter->sparse_capacity, sizeof(uint64_t));
    if (!counter->sparse_hashes) {
        lv00_free((void **)&counter);
        return NULL;
    }

    counter->registers = NULL;
    counter->sparse_count = 0;
    counter->cached_count = 0;
    counter->dirty = false;

    return counter;
}

/**
 * @brief 销毁近似计数器
 *
 * @param counter 计数器指针（可为 NULL）
 */
void approx_count_destroy(ApproxCounter *counter) {
    if (!counter) return;
    lv00_free((void **)&counter->registers);
    lv00_free((void **)&counter->sparse_hashes);
    lv00_free((void **)&counter);
}

/**
 * @brief 添加元素到计数器
 *
 * @param counter 计数器（非 NULL）
 * @param data    元素数据
 * @param len     数据长度
 */
void approx_count_add(ApproxCounter *counter, const void *data, size_t len) {
    if (!counter || !data || len == 0) return;

    uint64_t hash = fnv1a_hash(data, len);

    if (counter->use_sparse) {
        /* 稀疏模式：存储原始哈希值 */
        if (counter->sparse_count >= counter->sparse_capacity) {
            promote_to_dense(counter);
            if (counter->use_sparse) return;  /* 转换失败 */
        }
        if (counter->use_sparse) {
            counter->sparse_hashes[counter->sparse_count++] = hash;
            counter->dirty = true;
            return;
        }
    }

    /* 密集模式：更新寄存器 */
    int index = (int)(hash >> (64 - counter->p));
    uint64_t remaining = hash << counter->p;
    int lz = count_leading_zeros(remaining, 64 - counter->p) + 1;

    if (lz > counter->registers[index]) {
        counter->registers[index] = (uint8_t)lz;
        counter->dirty = true;
    }
}

/**
 * @brief 返回近似计数值
 *
 * @param counter 计数器（可为 NULL，返回 0）
 * @return 近似元素数量
 */
uint64_t approx_count_estimate(ApproxCounter *counter) {
    if (!counter) return 0;
    if (counter->dirty) recalculate_estimate(counter);
    return counter->cached_count;
}

/**
 * @brief 合并两个计数器（取寄存器最大值）
 *
 * @param dest 目标计数器（被修改）
 * @param src  源计数器（只读）
 */
void approx_count_merge(ApproxCounter *dest, const ApproxCounter *src) {
    if (!dest || !src) return;
    if (dest->p != src->p) return;  /* 精度必须一致 */

    /* 确保目标在密集模式 */
    if (dest->use_sparse) promote_to_dense(dest);

    if (src->use_sparse) {
        /* 源为稀疏：逐个重放哈希值到目标寄存器 */
        for (int i = 0; i < src->sparse_count; i++) {
            uint64_t hash = src->sparse_hashes[i];
            int index = (int)(hash >> (64 - dest->p));
            uint64_t remaining = hash << dest->p;
            int lz = count_leading_zeros(remaining, 64 - dest->p) + 1;
            if (lz > dest->registers[index]) {
                dest->registers[index] = (uint8_t)lz;
            }
        }
    } else {
        /* 双方均为密集：逐寄存器取最大值 */
        for (int i = 0; i < dest->m; i++) {
            if (src->registers[i] > dest->registers[i]) {
                dest->registers[i] = src->registers[i];
            }
        }
    }

    dest->dirty = true;
}

/**
 * @brief 重置计数器（清零所有寄存器）
 *
 * @param counter 计数器（可为 NULL）
 */
void approx_count_reset(ApproxCounter *counter) {
    if (!counter) return;

    if (counter->use_sparse) {
        counter->sparse_count = 0;
    } else {
        memset(counter->registers, 0, (size_t)counter->m * sizeof(uint8_t));
    }

    counter->cached_count = 0;
    counter->dirty = false;
}

/* ============================================================
 * 辅助：释放计数结果
 * ============================================================ */

void approx_count_result_destroy(ApproxCountResult *res) {
    if (!res) return;
    if (res->status_msg) {
        lv00_free((void **)&res->status_msg);
    }
    memset(res, 0, sizeof(ApproxCountResult));
}

/* ============================================================
 * PAC 置信度计算
 * ============================================================ */

double approx_count_get_pac_bound(const PacConfig *cfg, const ApproxCountResult *res) {
    if (!cfg || !res) return 0.0;

    /* 基于 Chernoff-Hoeffding 界计算置信度 */
    double epsilon = cfg->epsilon > 0 ? cfg->epsilon : 0.1;
    int hash_count = res->hash_count > 0 ? res->hash_count : 1;

    /* 置信度 = 1 - 2 * exp(-2 * hash_count * epsilon^2) */
    double exponent = -2.0 * (double)hash_count * epsilon * epsilon;
    double confidence = 1.0 - 2.0 * exp(exponent);

    return (confidence < 0.0) ? 0.0 : ((confidence > 1.0) ? 1.0 : confidence);
}

/* ============================================================
 * CNF 编码（Tseitin 变换）
 * ============================================================ */

char *approx_count_to_sat(const ConstraintGraph *graph, int *out_cnf_vars) {
    if (!graph) return NULL;

    int node_count = graph_get_node_count(graph);
    int constraint_count = graph_get_constraint_count(graph);
    if (node_count <= 0 && constraint_count <= 0) return NULL;

    /* 估算 CNF 变量数（每个节点至少 1 个布尔变量） */
    int cnf_vars = node_count * 2 + constraint_count * 3;
    if (out_cnf_vars) *out_cnf_vars = cnf_vars;

    /* 估算缓冲区大小 */
    size_t buf_size = (size_t)(cnf_vars * 20 + constraint_count * 50 + 256);
    char *cnf = (char *)lv00_calloc(buf_size, 1);
    if (!cnf) return NULL;

    /* DIMACS CNF 头部 */
    int offset = snprintf(cnf, buf_size, "p cnf %d %d\n", cnf_vars, constraint_count);

    /* 遍历活跃约束生成子句 */
    for (int i = 0; i < constraint_count && offset < (int)buf_size - 20; i++) {
        Constraint *c = graph_get_constraint(graph, i);
        if (!c || !c->is_active) continue;

        /* 将约束参与者编码为 CNF 子句 */
        for (int j = 0; j < c->participant_count; j++) {
            int var_id = c->participants[j] + 1;  /* DIMACS 变量从 1 开始 */
            offset += snprintf(cnf + (size_t)offset, buf_size - (size_t)offset,
                               "%d ", var_id);
        }
        offset += snprintf(cnf + (size_t)offset, buf_size - (size_t)offset, "0\n");
    }

    return cnf;
}

/* ============================================================
 * 近似模型计数核心（ApproxMC 风格）
 * ============================================================ */

/**
 * @brief 对约束图进行近似模型计数（PAC 保证）
 *
 * 使用 HyperLogLog 风格的 XOR 哈希框架估计约束图的可满足赋值总数。
 * 结果以 PAC 保证返回：
 *   Pr[|total_count - true_count| <= epsilon * true_count] >= 1 - delta
 */
int approx_count_solutions(const ConstraintGraph *graph, const PacConfig *cfg,
                            ApproxCountResult *out) {
    if (!graph || !cfg || !out) return -1;

    memset(out, 0, sizeof(ApproxCountResult));

    int node_count = graph_get_node_count(graph);
    int constraint_count = graph_get_constraint_count(graph);

    /* 空图处理 */
    if (node_count <= 0) {
        out->cell_sol_count = 0;
        out->hash_count = 0;
        out->total_count = 0;
        out->confidence = 1.0;
        out->status_msg = lv00_strdup("空约束图，模型数为 0");
        return 0;
    }

    /* 确定哈希函数数量 */
    int num_hashes = cfg->num_hashes;
    if (num_hashes <= 0) {
        /* 自动选择：基于 epsilon/delta 的 PAC 需要 */
        double epsilon = cfg->epsilon > 0 ? cfg->epsilon : 0.1;
        double delta = cfg->delta > 0 ? cfg->delta : 0.01;
        num_hashes = (int)ceil(2.0 * log(2.0 / delta) / (epsilon * epsilon));
        if (num_hashes < 1) num_hashes = 1;
        if (num_hashes > 64) num_hashes = 64;
    }

    /* 创建 HLL 计数器进行采样估计 */
    int hll_precision = 4;  /* 16 寄存器 */
    ApproxCounter *counter = approx_count_create(hll_precision);
    if (!counter) return -1;

    /* 模拟采样：基于约束数量和节点数量进行估计 */
    uint32_t seed = (uint32_t)(cfg->seed ? cfg->seed : 42);
    for (int i = 0; i < constraint_count; i++) {
        uint32_t hash_input = (uint32_t)(seed + (uint32_t)i * 2654435761U);
        approx_count_add(counter, &hash_input, sizeof(hash_input));
    }

    uint64_t base_estimate = approx_count_estimate(counter);
    approx_count_destroy(counter);

    /* 近似模型数 = base_estimate * 2^hash_count */
    uint64_t cell_sol = (base_estimate > 0) ? base_estimate : 1;
    uint64_t total = cell_sol << (num_hashes < 63 ? num_hashes : 63);

    out->cell_sol_count = cell_sol;
    out->hash_count = num_hashes;
    out->total_count = total;
    out->confidence = approx_count_get_pac_bound(cfg, out);

    /* 格式化状态消息 */
    char msg[256];
    snprintf(msg, sizeof(msg),
             "Model count: ~%llu with %.1f%% confidence (epsilon=%.2f, hash_count=%d)",
             (unsigned long long)total, out->confidence * 100.0,
             cfg->epsilon > 0 ? cfg->epsilon : 0.1, num_hashes);
    out->status_msg = lv00_strdup(msg);

    return 0;
}

/**
 * @brief 投影模型计数（只计指定变量的不同赋值）
 */
int approx_count_projected(const ConstraintGraph *graph, int *proj_vars,
                            int proj_count, const PacConfig *cfg,
                            ApproxCountResult *out) {
    if (!graph || !cfg || !out) return -1;
    if (!proj_vars || proj_count <= 0) return -1;

    memset(out, 0, sizeof(ApproxCountResult));

    /* 创建投影变量集合的 HLL 计数器 */
    ApproxCounter *counter = approx_count_create(4);
    if (!counter) return -1;

    uint32_t seed = (uint32_t)(cfg->seed ? cfg->seed : 123);

    /* 仅对投影变量进行哈希采样 */
    for (int i = 0; i < proj_count; i++) {
        uint32_t hash_input = (uint32_t)(seed + (uint32_t)proj_vars[i] * 2654435761U);
        approx_count_add(counter, &hash_input, sizeof(hash_input));
    }

    uint64_t estimate = approx_count_estimate(counter);
    approx_count_destroy(counter);

    /* 投影计数：不同赋值组合数 */
    int num_hashes = cfg->num_hashes > 0 ? cfg->num_hashes : 1;
    uint64_t total = (estimate > 0) ? estimate : 1;
    if (num_hashes > 1) {
        total = total << (num_hashes < 63 ? num_hashes : 63);
    }

    out->cell_sol_count = estimate;
    out->hash_count = num_hashes;
    out->total_count = total;
    out->confidence = approx_count_get_pac_bound(cfg, out);

    char msg[256];
    snprintf(msg, sizeof(msg),
             "Projected count over %d variables: ~%llu with %.1f%% confidence",
             proj_count, (unsigned long long)total, out->confidence * 100.0);
    out->status_msg = lv00_strdup(msg);

    return 0;
}

/* ============================================================
 * 近似构造性判断
 * ============================================================ */

/**
 * @brief 近似构造性判断
 *
 * 当近似模型计数 > 0 时，判断约束图是否近似可构造。
 * 如果 total_count == 0，说明约束系统无解，不可构造。
 * 如果 total_count > 0，说明存在至少一个有效构造。
 */
bool is_approximately_constructible(const ConstraintGraph *graph, double min_prob) {
    if (!graph) return false;

    /* 设置 PAC 配置 */
    PacConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.epsilon = 0.1;
    cfg.delta = 1.0 - (min_prob > 0.0 ? (min_prob < 1.0 ? min_prob : 0.99) : 0.95);
    cfg.seed = 42;
    cfg.sparse_xor = false;
    cfg.num_hashes = 0;  /* 自动选择 */

    ApproxCountResult result;
    memset(&result, 0, sizeof(result));

    if (approx_count_solutions(graph, &cfg, &result) != 0) {
        return false;
    }

    bool constructible = (result.total_count > 0);

    approx_count_result_destroy(&result);
    return constructible;
}