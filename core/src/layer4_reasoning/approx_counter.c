/**
 * @file approx_counter.c
 * @brief ApproxMC 近似模型计数 —— 桩实现
 *
 * 提供约束图 -> DIMACS CNF 编码和近似模型计数的基本框架。
 * 当前为桩实现，后续可对接 ApproxMC 的 C API 或调用外部 ApproxMC 可执行文件。
 *
 * @version v3.3.0
 * @date 2026-05-24
 */

#include "approx_counter.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00.h"

/* ---- 内部辅助 ---- */

/* 修复：定义错误检查宏，用于 cnf_builder_add_lit / cnf_builder_end_clause 的返回值检查 */
#define CHECK_ADD_LIT(b, lit)   do { if (cnf_builder_add_lit((b), (lit)) != 0) goto build_failed; } while (0)
#define CHECK_END_CLAUSE(b)     do { if (cnf_builder_end_clause((b)) != 0) goto build_failed; } while (0)

/** 内部 CNF 子句构建器 */
typedef struct {
    int var_count;       /**< 当前变量数 */
    int *clause_buffer;  /**< 子句缓冲区（0 结尾） */
    int clause_buf_size; /**< 缓冲区容量 */
    int clause_buf_len;  /**< 已使用长度 */
    int clause_count;    /**< 子句总数 */
    int next_var_id;     /**< 下一个可用的临时变量 ID */
} CNFBuilder;

/** 初始化 CNF 构建器 */
static CNFBuilder *cnf_builder_create(void) {
    CNFBuilder *b = (CNFBuilder *) lv00_malloc(sizeof(CNFBuilder));
    if (!b)
        return NULL;
    b->var_count = 0;
    b->clause_buf_size = 4096;
    b->clause_buf_len = 0;
    b->clause_count = 0;
    b->next_var_id = 1;
    b->clause_buffer = (int *) lv00_malloc((size_t) b->clause_buf_size * sizeof(int));
    if (!b->clause_buffer) {
        lv00_free((void **)&b);
        return NULL;
    }
    return b;
}

/** 在 CNF 构建器中创建新变量 */
static int cnf_builder_new_var(CNFBuilder *b) {
    return b->next_var_id++;
}

/** 添加单个文字到当前子句（正数 = 正文字，负数 = 负文字）
 * @return 成功返回 0，内存不足返回 -1
 */
static int cnf_builder_add_lit(CNFBuilder *b, int lit) {
    if (b->clause_buf_len + 1 >= b->clause_buf_size) {
        /* 修复：添加整数溢出保护，防止 clause_buf_size 翻倍后超出 INT_MAX */
        if (b->clause_buf_size > INT_MAX / 2) {
            return -1; /* 容量已接近上限，无法再翻倍 */
        }
        b->clause_buf_size *= 2;
        int *new_buf = (int *) lv00_realloc(b->clause_buffer, (size_t) b->clause_buf_size * sizeof(int));
        /* 修复：检查 realloc 返回值，失败时返回错误而非使用悬垂指针 */
        if (!new_buf) {
            return -1;
        }
        b->clause_buffer = new_buf;
    }
    b->clause_buffer[b->clause_buf_len++] = lit;
    return 0;
}

/** 结束当前子句（在文字序列末尾加 0）
 * @return 成功返回 0，内存不足返回 -1
 */
static int cnf_builder_end_clause(CNFBuilder *b) {
    int ret = cnf_builder_add_lit(b, 0);
    if (ret != 0)
        return ret;
    b->clause_count++;
    return 0;
}

/** 销毁 CNF 构建器 */
static void cnf_builder_destroy(CNFBuilder *b) {
    if (b) {
        lv00_free((void **)&b->clause_buffer);
        lv00_free((void **)&b);
    }
}

/** 将 CNF 构建器输出为 DIMACS 格式字符串 */
static char *cnf_builder_to_dimacs(CNFBuilder *b) {
    if (!b)
        return NULL;
    /* 计算所需缓冲区大小 */
    size_t header_size = 256;
    /* 修复：添加整数溢出检查，防止 clause_buf_len * 12 溢出 size_t */
    if ((size_t) b->clause_buf_len > SIZE_MAX / 12) {
        return NULL; /* 乘法溢出，无法分配 */
    }
    size_t lit_part = (size_t) b->clause_buf_len * 12;
    if (header_size > SIZE_MAX - lit_part) {
        return NULL; /* 加法溢出 */
    }
    size_t buf_size = header_size + lit_part;
    char *buf = (char *) lv00_malloc(buf_size);
    if (!buf)
        return NULL;

    /* 写入头部 */
    int offset = snprintf(buf, header_size, "p cnf %d %d\n", b->next_var_id - 1, b->clause_count);

    /* 写入子句 */
    int lit_idx = 0;
    for (int ci = 0; ci < b->clause_count; ci++) {
        while (b->clause_buffer[lit_idx] != 0) {
            offset += snprintf(buf + offset, buf_size - offset, "%d ", b->clause_buffer[lit_idx]);
            lit_idx++;
        }
        lit_idx++; /* 跳过终止符 0 */
        offset += snprintf(buf + offset, buf_size - offset, "0\n");
    }
    return buf;
}

/* ========================================================================
 * approx_count_to_sat —— 约束图 -> DIMACS CNF
 *
 * 编码策略（Tseitin 变换）：
 * 1. 为每个约束关系创建一个布尔变量（命名变量）
 * 2. 为每个坐标的每一位创建一个布尔变量（bit-blast）
 * 3. 对每个节点约束关系生成 Tseitin 子句
 * 4. 特殊处理：INCIDENCE -> 点在线上，BETWEENNESS -> A-B-C 共线有序
 * ======================================================================== */

char *approx_count_to_sat(const ConstraintGraph *graph, int *out_cnf_vars) {
    if (!graph)
        return NULL;

    CNFBuilder *b = cnf_builder_create();
    if (!b)
        return NULL;

    /* Phase 1: 变量分配
     * 为每个节点的标识分配布尔变量空间。
     * 简化处理：每个节点一个布尔变量（表示该节点"活跃"）。
     * 完整实现应对坐标做 bit-blasting。
     */
    int *node_vars = (int *) lv00_malloc((size_t) graph->node_count * sizeof(int));
    if (!node_vars) {
        cnf_builder_destroy(b);
        return NULL;
    }

    for (int i = 0; i < graph->node_count; i++) {
        node_vars[i] = cnf_builder_new_var(b);
    }

    /* Phase 2: 约束编码（Tseitin 变换）
     * 遍历每个约束，生成合取范式子句。
     */
    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *c = graph->constraints[ci];
        /* 为约束创建辅助变量 */
        int aux = cnf_builder_new_var(b);

        /* 获取参与约束的节点 ID */
        int n0 = c->participants ? c->participants[0] : 0;
        int n1 = (c->participant_count > 1) ? c->participants[1] : 0;
        int n2 = (c->participant_count > 2) ? c->participants[2] : 0;

        int v0 = (n0 >= 0 && n0 < graph->node_count) ? node_vars[n0] : 0;
        int v1 = (n1 >= 0 && n1 < graph->node_count) ? node_vars[n1] : 0;
        int v2 = (n2 >= 0 && n2 < graph->node_count) ? node_vars[n2] : 0;

        switch (c->type) {
            case INCIDENCE:
                /* 关联约束：点在线段上
             * Tseitin: aux -> v0 & v1
             * 子句：(~aux | v0), (~aux | v1), (aux | ~v0 | ~v1) */
                CHECK_ADD_LIT(b, -aux);
                CHECK_ADD_LIT(b, v0);
                CHECK_END_CLAUSE(b);
                CHECK_ADD_LIT(b, -aux);
                CHECK_ADD_LIT(b, v1);
                CHECK_END_CLAUSE(b);
                CHECK_ADD_LIT(b, aux);
                CHECK_ADD_LIT(b, -v0);
                CHECK_ADD_LIT(b, -v1);
                CHECK_END_CLAUSE(b);
                break;

            case BETWEENNESS:
                /* 之间的约束：B 在 A 和 C 之间
             * Tseitin: aux -> v0 & v1 & v2
             * 4 条子句 */
                CHECK_ADD_LIT(b, -aux);
                CHECK_ADD_LIT(b, v0);
                CHECK_END_CLAUSE(b);
                CHECK_ADD_LIT(b, -aux);
                CHECK_ADD_LIT(b, v1);
                CHECK_END_CLAUSE(b);
                CHECK_ADD_LIT(b, -aux);
                CHECK_ADD_LIT(b, v2);
                CHECK_END_CLAUSE(b);
                CHECK_ADD_LIT(b, aux);
                CHECK_ADD_LIT(b, -v0);
                CHECK_ADD_LIT(b, -v1);
                CHECK_ADD_LIT(b, -v2);
                CHECK_END_CLAUSE(b);
                break;

            case INTERSECTION:
                /* 真交约束：两个对象在某点真交
             * 简化：双变量 AND */
                CHECK_ADD_LIT(b, -aux);
                CHECK_ADD_LIT(b, v0);
                CHECK_END_CLAUSE(b);
                CHECK_ADD_LIT(b, -aux);
                CHECK_ADD_LIT(b, v1);
                CHECK_END_CLAUSE(b);
                CHECK_ADD_LIT(b, aux);
                CHECK_ADD_LIT(b, -v0);
                CHECK_ADD_LIT(b, -v1);
                CHECK_END_CLAUSE(b);
                break;

            case CONTAINMENT:
            case CONNECTION:
                /* 包含/连接约束：与 INCIDENCE 类似处理 */
                CHECK_ADD_LIT(b, -aux);
                CHECK_ADD_LIT(b, v0);
                CHECK_END_CLAUSE(b);
                CHECK_ADD_LIT(b, -aux);
                CHECK_ADD_LIT(b, v1);
                CHECK_END_CLAUSE(b);
                CHECK_ADD_LIT(b, aux);
                CHECK_ADD_LIT(b, -v0);
                CHECK_ADD_LIT(b, -v1);
                CHECK_END_CLAUSE(b);
                break;

            default:
                break;
        }
    }

    /* 添加单元子句：所有节点变量为真（激活状态） */
    for (int i = 0; i < graph->node_count; i++) {
        CHECK_ADD_LIT(b, node_vars[i]);
        CHECK_END_CLAUSE(b);
    }

    /* Phase 3: 输出 DIMACS */
    char *result = cnf_builder_to_dimacs(b);

    if (out_cnf_vars) {
        *out_cnf_vars = b->next_var_id - 1;
    }

    lv00_free((void **)&node_vars);
    cnf_builder_destroy(b);
    return result;

build_failed:
    /* 修复：构建失败时的统一错误处理路径 */
    lv00_free((void **)&node_vars);
    cnf_builder_destroy(b);
    return NULL;
}

/* ========================================================================
 * approx_count_solutions —— 近似模型计数
 *
 * 当前桩实现：调用 approx_count_to_sat 后使用模拟估算。
 * 完整实现应调用 ApproxMC 的 C API 或外部进程。
 * ======================================================================== */

/**
 * @brief 近似模型计数
 *
 * 将约束图编码为 CNF，使用模拟 ApproxMC 估算模型数量，
 * 并计算 PAC（Probably Approximately Correct）置信度。
 *
 * @param graph 约束图
 * @param cfg   PAC 配置参数
 * @param out   输出计数结果
 * @return 成功返回 true，失败返回 false
 */
bool approx_count_solutions(const ConstraintGraph *graph, const PacConfig *cfg, ApproxCountResult *out) {
    if (!graph || !cfg || !out)
        return false;

    /* 将约束图编码为 CNF */
    int cnf_vars = 0;
    char *cnf_str = approx_count_to_sat(graph, &cnf_vars);
    if (!cnf_str) {
        out->cell_sol_count = 0;
        out->hash_count = 0;
        out->total_count = 0;
        out->confidence = 0.0;
        out->status_msg = lv00_strdup("Error: CNF encoding failed");
        return false;
    }

    /* 模拟 ApproxMC 估算：
     * 使用哈希层数计算（当前使用配置中的 num_hashes 或默认值）
     */
    int h = cfg->num_hashes > 0 ? cfg->num_hashes : 4;

    /* 简单估算：假设每个单元平均有 n 个解 */
    uint64_t estimated_cell = 1;

    /* total_count = cell_sol_count * 2^hash_count */
    uint64_t estimated_total = estimated_cell * ((uint64_t) 1 << (unsigned) h);

    /* 计算 PAC 置信度（Chernoff-Hoeffding 界简化） */
    double pac_bound = approx_count_get_pac_bound(cfg, out);

    out->cell_sol_count = estimated_cell;
    out->hash_count = h;
    out->total_count = estimated_total;
    out->confidence = pac_bound;

    /* 生成状态消息 */
    size_t msg_size = 256;
    out->status_msg = (char *) lv00_malloc(msg_size);
    if (out->status_msg) {
        snprintf(out->status_msg, msg_size,
                 "Model count: ~%llu with %.0f%% confidence "
                 "(epsilon=%.3f, delta=%.3f, %d CNF vars)",
                 (unsigned long long) estimated_total, pac_bound * 100.0, cfg->epsilon, cfg->delta, cnf_vars);
    }

    lv00_free((void **)&cnf_str);
    return true;
}

/* ========================================================================
 * approx_count_projected —— 投影模型计数
 *
 * 桩实现：框架与 approx_count_solutions 相同，投影变量过滤留待后续。
 * ======================================================================== */

/**
 * @brief 投影模型计数
 *
 * 在指定投影变量子集上计算模型数量（当前桩实现与全量计数相同）。
 *
 * @param graph      约束图
 * @param proj_vars  投影变量 ID 数组
 * @param proj_count 投影变量数量
 * @param cfg        PAC 配置参数
 * @param out        输出计数结果
 * @return 成功返回 true，失败返回 false
 */
bool approx_count_projected(const ConstraintGraph *graph, int *proj_vars, int proj_count, const PacConfig *cfg,
                            ApproxCountResult *out) {
    if (!graph || !proj_vars || proj_count <= 0 || !cfg || !out)
        return false;

    /* 当前桩：与全量计数相同，忽略投影变量过滤 */
    bool ok = approx_count_solutions(graph, cfg, out);
    if (!ok)
        return false;

    /* 状态消息附加投影信息 */
    if (out->status_msg) {
        size_t len = strlen(out->status_msg);
        size_t new_size = len + 128;
        char *new_msg = (char *) lv00_realloc(out->status_msg, new_size);
        if (new_msg) {
            out->status_msg = new_msg;
            snprintf(out->status_msg + len, new_size - len, " [projected: %d vars]", proj_count);
        }
    }
    return true;
}

/* ========================================================================
 * approx_count_get_pac_bound —— PAC 置信度计算
 * ======================================================================== */

/**
 * @brief 计算 PAC 置信度
 *
 * 基于 Chernoff-Hoeffding 界计算概率近似正确（PAC）的置信度。
 *
 * @param cfg PAC 配置参数
 * @param res 计数结果
 * @return 置信度值（0.0 ~ 1.0）
 */
double approx_count_get_pac_bound(const PacConfig *cfg, const ApproxCountResult *res) {
    if (!cfg || !res)
        return 0.0;

    /* Chernoff-Hoeffding 界：
     * 所需样本数 m >= (3/epsilon^2) * log(2/delta)
     * 置信度 = 1 - delta * exp(-epsilon^2 * m / 3)
     *
     * 如果 cell_sol_count > 0，使用简化估算
     */
    if (res->total_count == 0)
        return 0.0;

    double m = (double) res->cell_sol_count;
    if (m < 1.0)
        m = 1.0;
    double eps_sq = cfg->epsilon * cfg->epsilon;
    double bound = 1.0 - cfg->delta * exp(-eps_sq * m / 3.0);
    if (bound > 1.0)
        bound = 1.0;
    if (bound < 0.0)
        bound = 0.0;
    return bound;
}

/* ========================================================================
 * approx_count_result_free —— 释放计数结果
 * ======================================================================== */

/**
 * @brief 释放计数结果中的动态资源
 * @param res 计数结果指针
 */
void approx_count_result_free(ApproxCountResult *res) {
    if (res) {
        lv00_free((void **)&res->status_msg);
        res->status_msg = NULL;
        res->cell_sol_count = 0;
        res->hash_count = 0;
        res->total_count = 0;
        res->confidence = 0.0;
    }
}

/* ========================================================================
 * is_approximately_constructible —— 近似可构造性判定
 * ======================================================================== */

/**
 * @brief 近似可构造性判定
 *
 * 使用默认 PAC 配置进行模型计数，判断约束图对应的几何构造
 * 是否以不低于 min_prob 的置信度可构造。
 *
 * @param graph    约束图
 * @param min_prob 最小置信度阈值
 * @return 可构造返回 true，否则返回 false
 */
bool is_approximately_constructible(const ConstraintGraph *graph, double min_prob) {
    if (!graph)
        return false;

    /* 使用默认 PAC 配置进行计数 */
    PacConfig cfg;
    cfg.epsilon = 0.2;
    cfg.delta = 1.0 - min_prob;
    cfg.seed = 42;
    cfg.sparse_xor = true;
    cfg.num_hashes = 0; /* 自动选择 */

    ApproxCountResult res;
    memset(&res, 0, sizeof(res));

    if (!approx_count_solutions(graph, &cfg, &res)) {
        return false;
    }

    bool constructible = (res.total_count > 0) && (res.confidence >= min_prob);

    approx_count_result_free(&res);
    return constructible;
}
