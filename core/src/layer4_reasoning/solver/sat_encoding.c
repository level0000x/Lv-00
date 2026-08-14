/**
 * @file sat_encoding.c
 * @brief SAT 编码管线实现 —— 借鉴 Alloy Kodkod 的关系逻辑到 SAT 编码管道
 *
 * 实现几何约束图中约束到 CNF 子句的编码规则、SAT 变量映射表管理、
 * DIMACS CNF 格式导出以及 SAT 模型的解码。
 *
 * 编码管线流程：
 *   1. 约束图节点注册为 SAT 变量（SatVarMap）
 *   2. 几何约束按规则编码为 CNF 子句
 *   3. 调用内部 SAT 求解器求解
 *   4. 将 SAT 赋值解码回约束图/关系实例
 *
 * @version v3.3.0
 * @date 2026-05-24
 */

#include "lv/sat_encoding.h"

#include <stdio.h>
#include <string.h>

#include "lv/constraint_graph.h"

#include "lv/error_codes.h"
#include "lv/lv_internal.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"
#include "lv/solver_core.h"
#include "sat_encoding_internal.h"

/* ========================================================================
 * 内部常量
 * ======================================================================== */

/** 变元映射表初始容量 */
#define VAR_MAP_INITIAL_CAP 128
/** CNF 子句缓冲区初始容量 */
#define CLAUSE_INITIAL_CAP 256
/** 比特编码用的整数位宽（默认 8 位） */
#define DEFAULT_BITWIDTH 8

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

/**
 * @brief 比较两个元组是否相等
 *
 * @param arity    元数
 * @param a        元组 A
 * @param b        元组 B
 * @return true 相等
 */
bool tuple_equals(int arity, const int *a, const int *b) {
    for (int i = 0; i < arity; i++) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

/**
 * @brief 在变量映射表中查找元组
 *
 * @param enc       编码上下文
 * @param arity     元数
 * @param atom_ids  原子ID数组
 * @return 变量 ID（>= 1），未找到返回 -1
 */
static int find_var_entry(const SatEncoding *enc, int arity, const int *atom_ids) {
    for (int i = 0; i < enc->var_map.count; i++) {
        const SatVarEntry *entry = (const SatVarEntry *)lv_darray_get(&enc->var_map, i);
        if (entry && entry->arity == arity && tuple_equals(arity, entry->atom_ids, atom_ids)) {
            return entry->var_id;
        }
    }
    return -1;
}

/**
 * @brief 确保 CNF 子句缓冲区有足够容量
 */
static bool ensure_clause_capacity(SatEncoding *enc) {
    if (enc->clause_count >= enc->clause_capacity) {
        int old_cap = enc->clause_capacity;
        /* 第一次：扩容 clauses（溢出检查由 lv_ensure_capacity 内部完成） */
        if (!lv_ensure_capacity((void **) &enc->clauses, old_cap,
                                &enc->clause_capacity, sizeof(int *), 1))
            return false;
        /* 第二次：扩容 clause_sizes。临时回退容量指针使扩容真实执行，保持双数组容量一致 */
        enc->clause_capacity = old_cap;
        if (!lv_ensure_capacity((void **) &enc->clause_sizes, old_cap,
                                &enc->clause_capacity, sizeof(int), 1)) {
            enc->clause_capacity = old_cap;
            return false;
        }
    }
    return true;
}

/**
 * @brief 确保变量映射表有足够容量
 */
static bool ensure_var_capacity(SatEncoding *enc) {
    return lv_darray_reserve(&enc->var_map, enc->var_map.count + 1);
}

/* ========================================================================
 * SAT 编码上下文生命周期
 * ======================================================================== */

SatEncoding *sat_encoding_create(int initial_var_capacity, int initial_clause_capacity) {
    if (initial_var_capacity <= 0)
        initial_var_capacity = VAR_MAP_INITIAL_CAP;
    if (initial_clause_capacity <= 0)
        initial_clause_capacity = CLAUSE_INITIAL_CAP;

    SatEncoding *enc = (SatEncoding *) lv_calloc(1, sizeof(SatEncoding));
    lv_CHECK_ALLOC(enc, NULL);

    lv_darray_init(&enc->var_map, sizeof(SatVarEntry));
    if (!lv_darray_reserve(&enc->var_map, initial_var_capacity)) {
        lv_free((void **) &enc);
        return NULL;
    }
    enc->next_var_id = 1;

    enc->clauses = (int **) lv_malloc((size_t) initial_clause_capacity * sizeof(int *));
    enc->clause_sizes = (int *) lv_malloc((size_t) initial_clause_capacity * sizeof(int));
    if (!enc->clauses || !enc->clause_sizes) {
        lv_free((void **) &enc->var_map);
        if (enc->clauses)
            lv_free((void **) &enc->clauses);
        if (enc->clause_sizes)
            lv_free((void **) &enc->clause_sizes);
        lv_free((void **) &enc);
        return NULL;
    }
    enc->clause_capacity = initial_clause_capacity;
    enc->clause_count = 0;

    enc->total_vars = 0;
    enc->total_clauses = 0;
    enc->encode_time_ms = 0.0;
    enc->graph = NULL;
    enc->rel_model = NULL;

    return enc;
}

void sat_encoding_destroy(SatEncoding *enc) {
    if (!enc)
        return;

    lv_darray_free(&enc->var_map);
    lv_free_ptr_array((void ***) &enc->clauses, (size_t) enc->clause_count);
    lv_free((void **) &enc->clause_sizes);
    lv_free((void **) &enc);
}

/* ========================================================================
 * 变量注册与查找
 * ======================================================================== */

int sat_encoding_register_var(SatEncoding *enc, int arity, const int *atom_ids) {
    lv_CHECK_NULL(enc, -1);
    lv_CHECK_NULL(atom_ids, -1);
    if (arity <= 0 || arity > 8) {
        lv_set_error_ctx(lv_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__, "无效元数: %d, 有效范围 [1, 8]", arity);
        return -1;
    }

    /* 查找是否已有映射 */
    int existing = find_var_entry(enc, arity, atom_ids);
    if (existing >= 1)
        return existing;

    /* 创建新映射，使用 lvDArray 就地构造 */
    if (!lv_darray_reserve(&enc->var_map, enc->var_map.count + 1))
        return -1;
    SatVarEntry *entry = (SatVarEntry *)((char *)enc->var_map.data + (size_t)enc->var_map.count * sizeof(SatVarEntry));
    entry->var_id = enc->next_var_id++;
    entry->arity = arity;
    memset(entry->atom_ids, 0, sizeof(entry->atom_ids));
    for (int i = 0; i < arity; i++) {
        entry->atom_ids[i] = atom_ids[i];
    }
    enc->var_map.count++;
    enc->total_vars++;

    return entry->var_id;
}

int sat_encoding_lookup_var(const SatEncoding *enc, int arity, const int *atom_ids) {
    lv_CHECK_NULL(enc, -1);
    lv_CHECK_NULL(atom_ids, -1);
    if (arity <= 0 || arity > 8)
        return -1;
    return find_var_entry(enc, arity, atom_ids);
}

/* ========================================================================
 * CNF 子句管理
 * ======================================================================== */

int sat_encoding_add_clause(SatEncoding *enc, const SatLiteral *literals, int count) {
    lv_CHECK_NULL(enc, -1);
    lv_CHECK_NULL(literals, -1);
    if (count <= 0) {
        /* 空白子句表示矛盾 */
        lv_set_error_ctx(lv_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__, "子句文字数量必须 >= 1");
        return -1;
    }

    if (!ensure_clause_capacity(enc))
        return -1;

    int *clause = (int *) lv_malloc((size_t) (count + 1) * sizeof(int));
    lv_CHECK_ALLOC(clause, -1);
    for (int i = 0; i < count; i++) {
        clause[i] = literals[i];
    }
    clause[count] = 0; /* 0 终止 */

    int idx = enc->clause_count;
    enc->clauses[idx] = clause;
    enc->clause_sizes[idx] = count;
    enc->clause_count++;
    enc->total_clauses++;

    return idx;
}

int sat_encoding_add_assumption(SatEncoding *enc, SatLiteral literal) {
    /* 单元子句即假设 */
    SatLiteral clause[1] = {literal};
    return sat_encoding_add_clause(enc, clause, 1);
}

/* ========================================================================
 * 诊断与调试
 * ======================================================================== */

int *sat_get_unsat_core(const SatEncoding *enc, int *out_count) {
    lv_CHECK_NULL(enc, NULL);
    lv_CHECK_NULL(out_count, NULL);

    /* 提取不可满足核心 */
    /* 策略：优先使用约束图中的约束 ID 作为核心标识。
     * 如果约束图存在，返回所有活跃约束的 ID（fallback 策略，
     * 因为编码上下文不直接持有求解器实例，无法追踪冲突子句）。
     * 如果没有约束图，返回所有子句索引。 */
    if (enc->graph && enc->graph->constraint_count > 0) {
        /* 收集所有活跃约束的 ID 作为 UNSAT core */
        int core_count = 0;
        for (int i = 0; i < enc->graph->constraint_count; i++) {
            if (enc->graph->constraints[i] && enc->graph->constraints[i]->is_active) {
                core_count++;
            }
        }

        if (core_count == 0) {
            /* 没有活跃约束，返回空核心 */
            *out_count = 0;
            int *core = (int *) lv_malloc(sizeof(int));
            if (core)
                core[0] = -1;
            return core;
        }

        int *core = (int *) lv_malloc((size_t) core_count * sizeof(int));
        if (!core) {
            *out_count = 0;
            return NULL;
        }

        int idx = 0;
        for (int i = 0; i < enc->graph->constraint_count; i++) {
            if (enc->graph->constraints[i] && enc->graph->constraints[i]->is_active) {
                core[idx++] = enc->graph->constraints[i]->id;
            }
        }
        *out_count = core_count;
        return core;
    }

    /* Fallback：没有约束图时，返回所有子句索引作为核心 */
    if (enc->clause_count > 0) {
        int *core = (int *) lv_malloc((size_t) enc->clause_count * sizeof(int));
        if (!core) {
            *out_count = 0;
            return NULL;
        }
        for (int i = 0; i < enc->clause_count; i++) {
            core[i] = i;
        }
        *out_count = enc->clause_count;
        return core;
    }

    /* 完全无信息，返回空核心 */
    *out_count = 0;
    int *core = (int *) lv_malloc(sizeof(int));
    if (core)
        core[0] = -1;
    return core;
}

bool sat_encoding_export_dimacs(const SatEncoding *enc, const char *filepath) {
    lv_CHECK_NULL(enc, false);
    lv_CHECK_NULL(filepath, false);

    FILE *fp = fopen(filepath, "w");
    if (!fp) {
        lv_set_error_ctx(lv_ERROR_IO, __FILE__, __LINE__, __func__, "无法打开文件: %s", filepath);
        return false;
    }

    /* 写入 DIMACS CNF 头 */
    fprintf(fp, "c DIMACS CNF -- generated by Lv-00 SAT encoding pipeline\n");
    fprintf(fp, "c Variables: %d\n", enc->next_var_id - 1);
    fprintf(fp, "c Clauses: %d\n", enc->clause_count);
    fprintf(fp, "p cnf %d %d\n", enc->next_var_id - 1, enc->clause_count);

    /* 写入每个子句 */
    for (int i = 0; i < enc->clause_count; i++) {
        const int *clause = enc->clauses[i];
        int size = enc->clause_sizes[i];
        for (int j = 0; j < size; j++) {
            fprintf(fp, "%d ", clause[j]);
        }
        fprintf(fp, "0\n");
    }

    fclose(fp);
    return true;
}

void sat_encoding_get_stats(const SatEncoding *enc, int *out_vars, int *out_clauses) {
    if (!enc) {
        if (out_vars)
            *out_vars = 0;
        if (out_clauses)
            *out_clauses = 0;
        return;
    }
    if (out_vars)
        *out_vars = enc->total_vars;
    if (out_clauses)
        *out_clauses = enc->total_clauses;
}
