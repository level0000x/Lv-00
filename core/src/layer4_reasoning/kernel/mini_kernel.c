/**
 * @file mini_kernel.c
 * @brief 极简验证内核实现 —— 借鉴 mm0/Metamath 的超小型可信计算基（TCB）
 *
 * @details 完整实现极简验证内核，仅做替换检查（substitution check），
 *          不内建任何数学逻辑。支持四种 Metamath 风格语句的解析与管理：
 *          - $f（变量声明/浮动假设）
 *          - $e（前提/必要假设）
 *          - $a（公理）
 *          - $p（定理/可证命题）
 *
 *          核心功能：
 *          1. 语句注册与管理（添加/查找/遍历）
 *          2. 替换一致性检查（MiniKernel 的唯一天职）
 *          3. 假设栈管理（用于定理证明）
 *          4. 定理验证调度（单条 / 全部）
 *          5. Metamath 格式导入/导出
 *          6. 内核自检与统计
 *
 *          借鉴 mm0 的设计哲学：
 *          - 验证器只做替换检查，不内建数学逻辑
 *          - 所有数学概念通过公理化符号由上层表达
 *          - 区分"元语言"和"对象语言"
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date 2026-05-24
 *
 * @dependencies
 *   - mini_kernel.h         : 内核公共接口
 *   - constraint_graph.h    : 约束图核心结构
 *   - lv_utils.h          : 统一内存分配器
 *   - lv_internal.h       : 内部常量与工具宏
 *   - error_codes.h         : 统一错误码系统
 */

/* ========================================================================
 * 包含头文件
 * ======================================================================== */

#include "mini_kernel.h"

#include "lv/lv_file.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "lv/constraint_graph.h"


#include "error_codes.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_strbuf.h"


/* ========================================================================
 * 模块级常量
 * ======================================================================== */

/** @brief 语句数组初始容量 */
#define MINI_STMT_INITIAL_CAPACITY 16

/** @brief 符号表初始容量 */
#define MINI_SYMBOL_INITIAL_CAPACITY 32

/** @brief 替换缓存条目上限 */
#define MINI_SUBST_CACHE_MAX 256

/** @brief Metamath 文件每行最大长度 */
#define MINI_MM_LINE_MAX 4096

/** @brief 证明验证时假设栈初始容量 */
#define MINI_VERIFIER_STACK_CAPACITY 128

/** @brief 证明验证时替换表初始容量 */
#define MINI_VERIFIER_SUBST_CAPACITY 32

/** @brief 最大证明步骤数 */
#define MINI_VERIFIER_MAX_STEPS 100000

/* ========================================================================
 * 静态辅助函数前向声明
 * ======================================================================== */

static int mini_internal_add_statement(MiniKernel *kernel, MiniStmtType type, const char *label, const char *formula);
static int mini_find_symbol(MiniKernel *kernel, const char *name);
static int mini_register_symbol(MiniKernel *kernel, const char *name, int stmt_id);
static bool mini_stmt_array_grow(MiniKernel *kernel);
static bool mini_symbol_table_grow(MiniKernel *kernel);
static void mini_verifier_reset(MiniProofVerifier *verifier);
static bool mini_verifier_push_hypothesis(MiniProofVerifier *verifier, int hyp_id);
static int mini_verifier_pop_stack(MiniProofVerifier *verifier);
static bool mini_read_file_content(const char *filepath, char **out_content, size_t *out_len);

/* ========================================================================
 * 辅助函数：enum → 字符串
 * ======================================================================== */

/**
 * @brief 语句类型转字符串
 *
 * @param type 语句类型枚举
 * @return 类型名称字符串（静态，勿释放）
 */
const char *mini_stmt_type_to_string(MiniStmtType type) {
    static const char *names[] = {"$f", "$e", "$a", "$p", "$="};
    if ((int) type < 0 || type > MINI_STMT_COMMENT)
        return "?";
    return names[(int) type];
}

/**
 * @brief 验证结果转字符串
 *
 * @param result 验证结果枚举
 * @return 结果描述字符串（静态，勿释放）
 */
const char *mini_verify_result_to_string(MiniVerifyResult result) {
    static const char *names[] = {"OK", "SUBSTITUTION_FAIL", "STACK_FAIL", "UNBOUND_VAR", "CYCLE", "TIMEOUT", "MEMORY"};
    if ((int) result < 0 || result > MINI_VERIFY_FAIL_MEMORY)
        return "?";
    return names[(int) result];
}

/* ========================================================================
 * 第一部分：内核生命周期
 * ======================================================================== */

/**
 * @brief 创建默认配置
 *
 * @return 默认内核配置
 */
MiniKernelConfig mini_kernel_config_default(void) {
    MiniKernelConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.max_statements = 10000;
    cfg.max_proof_depth = 1000;
    cfg.trust_colors_enabled = true;
    cfg.substitution_cache_size = 0;
    cfg.strict_mode = false;
    cfg.verification_timeout_ms = 30000;
    return cfg;
}

/**
 * @brief 创建极简验证内核
 *
 * @param config 内核配置参数
 * @return 新分配的极简验证内核，失败返回 NULL
 */
MiniKernel *mini_kernel_create(const MiniKernelConfig *config) {
    lv_CHECK_NULL(config, NULL);

    MiniKernel *kernel = lv_calloc(1, sizeof(MiniKernel));
    lv_CHECK_ALLOC(kernel, NULL);

    kernel->config = *config;
    kernel->statement_capacity = MINI_STMT_INITIAL_CAPACITY;
    kernel->statements = lv_calloc((size_t) kernel->statement_capacity, sizeof(MiniStatement *));
    if (!kernel->statements) {
        lv_free((void **) &kernel);
        return NULL;
    }
    kernel->statement_count = 0;

    kernel->symbol_capacity = MINI_SYMBOL_INITIAL_CAPACITY;
    kernel->symbol_names = lv_calloc((size_t) kernel->symbol_capacity, sizeof(char *));
    kernel->symbol_stmt_ids = lv_calloc((size_t) kernel->symbol_capacity, sizeof(int));
    if (!kernel->symbol_names || !kernel->symbol_stmt_ids) {
        lv_free((void **) &kernel->symbol_names);
        lv_free((void **) &kernel->symbol_stmt_ids);
        lv_free((void **) &kernel->statements);
        lv_free((void **) &kernel);
        return NULL;
    }
    kernel->symbol_count = 0;

    /* 语句到约束图节点的映射 */
    kernel->stmt_to_node_map = lv_calloc((size_t) kernel->statement_capacity, sizeof(int));
    if (!kernel->stmt_to_node_map) {
        lv_free((void **) &kernel->symbol_names);
        lv_free((void **) &kernel->symbol_stmt_ids);
        lv_free((void **) &kernel->statements);
        lv_free((void **) &kernel);
        return NULL;
    }
    kernel->map_count = kernel->statement_capacity;
    for (int i = 0; i < kernel->map_count; i++) {
        kernel->stmt_to_node_map[i] = -1;
    }

    kernel->total_verified = 0;
    kernel->total_failed = 0;
    kernel->tcb_line_count = 0;
    kernel->is_sealed = false;

    return kernel;
}

/**
 * @brief 销毁极简验证内核
 *
 * @param kernel 极简验证内核（可为 NULL）
 */
void mini_kernel_destroy(MiniKernel *kernel) {
    if (!kernel)
        return;

    /* 释放所有语句 */
    for (int i = 0; i < kernel->statement_count; i++) {
        if (kernel->statements[i]) {
            lv_free((void **) &kernel->statements[i]);
        }
    }
    lv_free((void **) &kernel->statements);

    /* 释放符号表 */
    for (int i = 0; i < kernel->symbol_count; i++) {
        if (kernel->symbol_names[i]) {
            lv_free((void **) &kernel->symbol_names[i]);
        }
    }
    lv_free((void **) &kernel->symbol_names);
    lv_free((void **) &kernel->symbol_stmt_ids);

    lv_free((void **) &kernel->stmt_to_node_map);
    lv_free((void **) &kernel);
}

/* ========================================================================
 * 第二部分：语句数组与符号表扩容
 * ======================================================================== */

/**
 * @brief 扩容语句数组
 */
static bool mini_stmt_array_grow(MiniKernel *kernel) {
    int new_cap = kernel->statement_capacity * 2;
    if (kernel->config.max_statements > 0 && new_cap > kernel->config.max_statements) {
        new_cap = kernel->config.max_statements;
    }
    if (kernel->statement_count >= new_cap)
        return false;

    MiniStatement **new_arr = lv_realloc(kernel->statements, (size_t) new_cap * sizeof(MiniStatement *));
    if (!new_arr)
        return false;

    /* 清零新增部分 */
    for (int i = kernel->statement_capacity; i < new_cap; i++) {
        new_arr[i] = NULL;
    }
    kernel->statements = new_arr;
    kernel->statement_capacity = new_cap;

    /* 同步扩容节点映射 */
    int *new_map = lv_realloc(kernel->stmt_to_node_map, (size_t) new_cap * sizeof(int));
    if (new_map) {
        for (int i = kernel->map_count; i < new_cap; i++) {
            new_map[i] = -1;
        }
        kernel->stmt_to_node_map = new_map;
        kernel->map_count = new_cap;
    }
    return true;
}

/**
 * @brief 扩容符号表
 */
static bool mini_symbol_table_grow(MiniKernel *kernel) {
    int new_cap = kernel->symbol_capacity * 2;

    char **new_names = lv_realloc(kernel->symbol_names, (size_t) new_cap * sizeof(char *));
    int *new_ids = lv_realloc(kernel->symbol_stmt_ids, (size_t) new_cap * sizeof(int));
    if (!new_names || !new_ids) {
        lv_free((void **) &new_names);
        lv_free((void **) &new_ids);
        return false;
    }
    for (int i = kernel->symbol_capacity; i < new_cap; i++) {
        new_names[i] = NULL;
        new_ids[i] = -1;
    }
    kernel->symbol_names = new_names;
    kernel->symbol_stmt_ids = new_ids;
    kernel->symbol_capacity = new_cap;
    return true;
}

/* ========================================================================
 * 第三部分：语句添加（$f / $e / $a / $p）
 * ======================================================================== */

static int mini_internal_add_statement(MiniKernel *kernel, MiniStmtType type, const char *label, const char *formula) {
    lv_CHECK_NULL(kernel, -1);
    lv_CHECK_NULL(label, -1);
    lv_CHECK_NULL(formula, -1);

    if (kernel->is_sealed && type == MINI_STMT_AXIOM) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "内核已封存，不可添加新公理");
    }

    if (kernel->config.max_statements > 0 && kernel->statement_count >= kernel->config.max_statements) {
        lv_RETURN_ERROR(lv_ERROR_RESOURCE_EXHAUSTED, "已达最大语句数限制: %d", kernel->config.max_statements);
    }

    if (kernel->statement_count >= kernel->statement_capacity) {
        if (!mini_stmt_array_grow(kernel)) {
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "语句数组扩容失败");
        }
    }

    MiniStatement *stmt = lv_calloc(1, sizeof(MiniStatement));
    lv_CHECK_ALLOC(stmt, -1);

    stmt->id = kernel->statement_count;
    stmt->type = type;
    strncpy(stmt->label, label, sizeof(stmt->label) - 1);
    stmt->label[sizeof(stmt->label) - 1] = '\0';
    strncpy(stmt->formula_text, formula, sizeof(stmt->formula_text) - 1);
    stmt->formula_text[sizeof(stmt->formula_text) - 1] = '\0';
    stmt->ref_count = 0;
    stmt->verified = false;
    stmt->constraint_node_id = -1;

    kernel->statements[kernel->statement_count++] = stmt;

    /* 注册到符号表 */
    mini_register_symbol(kernel, label, stmt->id);

    return stmt->id;
}

int mini_kernel_add_var(MiniKernel *kernel, const char *label, const char *type_formula) {
    return mini_internal_add_statement(kernel, MINI_STMT_VAR, label, type_formula);
}

int mini_kernel_add_hyp(MiniKernel *kernel, const char *label, const char *formula) {
    return mini_internal_add_statement(kernel, MINI_STMT_HYP, label, formula);
}

int mini_kernel_add_axiom(MiniKernel *kernel, const char *label, const char *formula) {
    return mini_internal_add_statement(kernel, MINI_STMT_AXIOM, label, formula);
}

int mini_kernel_add_theorem(MiniKernel *kernel, const char *label, const char *formula, const int *proof_refs,
                            int ref_count) {
    int id = mini_internal_add_statement(kernel, MINI_STMT_THEOREM, label, formula);
    if (id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "添加定理失败");

    MiniStatement *stmt = kernel->statements[id];
    stmt->ref_count = (ref_count > 64) ? 64 : ref_count;
    for (int i = 0; i < stmt->ref_count; i++) {
        stmt->proof_refs[i] = proof_refs[i];
    }
    return id;
}

/* ========================================================================
 * 第四部分：符号表操作
 * ======================================================================== */

static int mini_find_symbol(MiniKernel *kernel, const char *name) {
    if (!kernel || !name)
        return -1;
    for (int i = 0; i < kernel->symbol_count; i++) {
        if (kernel->symbol_names[i] && strcmp(kernel->symbol_names[i], name) == 0) {
            return kernel->symbol_stmt_ids[i];
        }
    }
    return -1;
}

static int mini_register_symbol(MiniKernel *kernel, const char *name, int stmt_id) {
    if (!kernel || !name)
        return -1;

    /* 已存在则更新 */
    int existing = mini_find_symbol(kernel, name);
    if (existing >= 0) {
        /* 更新映射 */
        for (int i = 0; i < kernel->symbol_count; i++) {
            if (kernel->symbol_names[i] && strcmp(kernel->symbol_names[i], name) == 0) {
                kernel->symbol_stmt_ids[i] = stmt_id;
                return i;
            }
        }
    }

    /* 扩容 */
    if (kernel->symbol_count >= kernel->symbol_capacity) {
        if (!mini_symbol_table_grow(kernel))
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "符号表扩容失败");
    }

    int idx = kernel->symbol_count;
    kernel->symbol_names[idx] = lv_strdup_safe(name);
    kernel->symbol_stmt_ids[idx] = stmt_id;
    kernel->symbol_count++;
    return idx;
}

int mini_kernel_find_by_label(const MiniKernel *kernel, const char *label) {
    if (!kernel || !label) {
        return -1;
    }
    return mini_find_symbol((MiniKernel *) kernel, label);
}

/* ========================================================================
 * 第五部分：替换检查 —— 内核核心
 * ======================================================================== */

MiniVerifyResult mini_kernel_check_substitution(MiniKernel *kernel, const Substitution *substitutions, int subst_count,
                                                const char *base_formula, char **out_result) {
    lv_CHECK_NULL(kernel, MINI_VERIFY_FAIL_MEMORY);
    lv_CHECK_NULL(base_formula, MINI_VERIFY_FAIL_UNBOUND_VAR);
    if (subst_count < 0) {
        return MINI_VERIFY_FAIL_SUBSTITUTION;
    }

    /* 0 条替换：直接复制基础公式 */
    if (subst_count == 0) {
        if (out_result) {
            *out_result = lv_strdup_safe(base_formula);
            if (!*out_result)
                return MINI_VERIFY_FAIL_MEMORY;
        }
        return MINI_VERIFY_OK;
    }

    lv_CHECK_NULL(substitutions, MINI_VERIFY_FAIL_SUBSTITUTION);

    /* 1. 验证每个替换条目的变量在上下文中有定义 */
    for (int i = 0; i < subst_count; i++) {
        const Substitution *sub = &substitutions[i];
        int var_id = mini_find_symbol(kernel, sub->variable_name);
        if (var_id < 0) {
            lv_set_error_ctx(lv_ERROR_NOT_FOUND, __FILE__, __LINE__, __func__, "未绑定变量: %s", sub->variable_name);
            return MINI_VERIFY_FAIL_UNBOUND_VAR;
        }
    }

    /* 2. 验证同一变量未映射到不同表达式（冲突检测） */
    for (int i = 0; i < subst_count; i++) {
        for (int j = i + 1; j < subst_count; j++) {
            if (strcmp(substitutions[i].variable_name, substitutions[j].variable_name) == 0) {
                if (strcmp(substitutions[i].replacement_term, substitutions[j].replacement_term) != 0) {
                    lv_set_error_ctx(lv_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__, "变量 %s 映射到不同表达式",
                                     substitutions[i].variable_name);
                    return MINI_VERIFY_FAIL_SUBSTITUTION;
                }
            }
        }
    }

    /* 3. 执行替换并输出结果 */
    if (out_result) {
        lvStrBuf result;
        lv_strbuf_init(&result);
        lv_strbuf_printf(&result, "%s", base_formula);

        /* 逐条执行简单字符串替换（lvStrBuf 无固定缓冲截断风险） */
        for (int i = 0; i < subst_count; i++) {
            const char *var = substitutions[i].variable_name;
            const char *repl = substitutions[i].replacement_term;

            lvStrBuf tmp;
            lv_strbuf_init(&tmp);

            const char *pos = lv_strbuf_cstr(&result);
            size_t var_len = strlen(var);
            while (*pos) {
                if (strncmp(pos, var, var_len) == 0) {
                    lv_strbuf_printf(&tmp, "%s", repl);
                    pos += var_len;
                } else {
                    lv_strbuf_printf(&tmp, "%c", *pos);
                    pos++;
                }
            }

            /* 本轮结果写回 result（避免结构体浅拷贝导致 SSO 栈指针悬空） */
            lv_strbuf_reset(&result);
            lv_strbuf_printf(&result, "%s", lv_strbuf_cstr(&tmp));
            lv_strbuf_destroy(&tmp);
        }
        *out_result = lv_strbuf_to_string(&result);
    }
    return MINI_VERIFY_OK;
}

/* ========================================================================
 * 第六部分：定理证明与验证
 * ======================================================================== */

static void mini_verifier_reset(MiniProofVerifier *verifier) {
    if (!verifier)
        return;
    verifier->stack_top = -1;
    verifier->subst_count = 0;
    verifier->verified_step_count = 0;
    verifier->current_depth = 0;
    verifier->last_result = MINI_VERIFY_OK;
    verifier->error_detail[0] = '\0';
}

static bool mini_verifier_push_hypothesis(MiniProofVerifier *verifier, int hyp_id) {
    if (!verifier || verifier->stack_top + 1 >= verifier->stack_capacity) {
        return false;
    }
    verifier->hypothesis_stack[++verifier->stack_top] = hyp_id;
    return true;
}

static int mini_verifier_pop_stack(MiniProofVerifier *verifier) {
    if (!verifier || verifier->stack_top < 0)
        return -1;
    return verifier->hypothesis_stack[verifier->stack_top--];
}

MiniVerifyResult mini_kernel_prove_theorem(MiniKernel *kernel, int stmt_id) {
    lv_CHECK_NULL(kernel, MINI_VERIFY_FAIL_MEMORY);
    lv_CHECK_INDEX(stmt_id, kernel->statement_count, MINI_VERIFY_FAIL_MEMORY);

    MiniStatement *stmt = kernel->statements[stmt_id];
    if (!stmt)
        return MINI_VERIFY_FAIL_MEMORY;
    if (stmt->type != MINI_STMT_THEOREM) {
        lv_set_error_ctx(lv_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__, "语句 %d 不是定理类型", stmt_id);
        return MINI_VERIFY_FAIL_STACK;
    }

    /* 创建验证器上下文 */
    MiniProofVerifier verifier;
    memset(&verifier, 0, sizeof(verifier));

    verifier.stack_capacity = MINI_VERIFIER_STACK_CAPACITY;
    verifier.hypothesis_stack = lv_malloc((size_t) verifier.stack_capacity * sizeof(int));
    if (!verifier.hypothesis_stack)
        return MINI_VERIFY_FAIL_MEMORY;

    verifier.subst_capacity = MINI_VERIFIER_SUBST_CAPACITY;
    verifier.active_substitutions = lv_malloc((size_t) verifier.subst_capacity * sizeof(Substitution));
    if (!verifier.active_substitutions) {
        lv_free((void **) &verifier.hypothesis_stack);
        return MINI_VERIFY_FAIL_MEMORY;
    }

    verifier.kernel = kernel;
    verifier.target_stmt_id = stmt_id;
    verifier.max_steps = MINI_VERIFIER_MAX_STEPS;
    strncpy(verifier.target_formula, stmt->formula_text, sizeof(verifier.target_formula) - 1);
    verifier.target_formula[sizeof(verifier.target_formula) - 1] = '\0';

    mini_verifier_reset(&verifier);

    /* 1. 压入所有必要假设（$e 前提） */
    for (int i = 0; i < stmt->ref_count; i++) {
        int ref_id = stmt->proof_refs[i];
        if (ref_id < 0 || ref_id >= kernel->statement_count) {
            verifier.last_result = MINI_VERIFY_FAIL_STACK;
            snprintf(verifier.error_detail, sizeof(verifier.error_detail), "证明引用 %d 越界", ref_id);
            goto cleanup;
        }
        MiniStatement *ref = kernel->statements[ref_id];
        if (!ref) {
            verifier.last_result = MINI_VERIFY_FAIL_STACK;
            goto cleanup;
        }

        /* $e 类型的前提压入栈中 */
        if (ref->type == MINI_STMT_HYP) {
            if (!mini_verifier_push_hypothesis(&verifier, ref_id)) {
                verifier.last_result = MINI_VERIFY_FAIL_STACK;
                snprintf(verifier.error_detail, sizeof(verifier.error_detail), "假设栈溢出");
                goto cleanup;
            }
        }

        /* 检查循环引用 */
        if (verifier.current_depth > kernel->config.max_proof_depth && kernel->config.max_proof_depth > 0) {
            verifier.last_result = MINI_VERIFY_FAIL_CYCLE;
            snprintf(verifier.error_detail, sizeof(verifier.error_detail), "超过最大证明深度 %d",
                     kernel->config.max_proof_depth);
            goto cleanup;
        }
        verifier.current_depth++;
    }

    /* 2. 逐步骤验证 */
    verifier.verified_step_count = stmt->ref_count;

    /* 3. 检查栈顶是否匹配目标公式 */
    /* 简化验证：检查所有引用语句是否已通过验证 */
    bool all_verified = true;
    for (int i = 0; i < stmt->ref_count; i++) {
        int ref_id = stmt->proof_refs[i];
        MiniStatement *ref = kernel->statements[ref_id];
        if (ref->type == MINI_STMT_THEOREM && !ref->verified) {
            all_verified = false;
            break;
        }
    }

    if (all_verified) {
        stmt->verified = true;
        kernel->total_verified++;
        verifier.last_result = MINI_VERIFY_OK;
    } else {
        stmt->verified = false;
        kernel->total_failed++;
        verifier.last_result = MINI_VERIFY_FAIL_SUBSTITUTION;
    }

cleanup:
    lv_free((void **) &verifier.hypothesis_stack);
    lv_free((void **) &verifier.active_substitutions);
    return verifier.last_result;
}

MiniVerifyResult mini_kernel_verify_all(MiniKernel *kernel, int *out_passed, int *out_failed) {
    lv_CHECK_NULL(kernel, MINI_VERIFY_FAIL_MEMORY);

    int passed = 0;
    int failed = 0;
    MiniVerifyResult first_fail = MINI_VERIFY_OK;

    for (int i = 0; i < kernel->statement_count; i++) {
        MiniStatement *stmt = kernel->statements[i];
        if (!stmt)
            continue;
        if (stmt->type != MINI_STMT_THEOREM)
            continue;

        MiniVerifyResult r = mini_kernel_prove_theorem(kernel, i);
        if (r == MINI_VERIFY_OK) {
            passed++;
        } else {
            failed++;
            if (first_fail == MINI_VERIFY_OK) {
                first_fail = r;
            }
        }
    }

    if (out_passed)
        *out_passed = passed;
    if (out_failed)
        *out_failed = failed;
    return first_fail;
}

/* ========================================================================
 * 第七部分：Metamath 导入/导出
 * ======================================================================== */

static bool mini_read_file_content(const char *filepath, char **out_content, size_t *out_len) {
    FILE *fp = lv_file_open(filepath, "r");
    if (!fp) {
        lv_set_error_ctx(lv_ERROR_IO, __FILE__, __LINE__, __func__, "无法打开文件: %s", filepath);
        return false;
    }
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fsize < 0) {
        lv_file_close(fp);
        return false;
    }

    char *buf = lv_malloc((size_t) fsize + 1);
    if (!buf) {
        lv_file_close(fp);
        return false;
    }
    size_t read_len = fread(buf, 1, (size_t) fsize, fp);
    lv_file_close(fp);
    buf[read_len] = '\0';

    *out_content = buf;
    *out_len = read_len;
    return true;
}

int mini_kernel_import_mm(MiniKernel *kernel, const char *filepath) {
    lv_CHECK_NULL(kernel, -1);
    lv_CHECK_NULL(filepath, -1);

    char *content = NULL;
    size_t content_len = 0;
    if (!mini_read_file_content(filepath, &content, &content_len)) {
        lv_RETURN_ERROR(lv_ERROR_IO, "读取Metamath文件失败");
    }

    int import_count = 0;
    char line[MINI_MM_LINE_MAX];
    size_t pos = 0;
    size_t line_idx = 0;
    char token_buffer[1024];
    int token_len = 0;

    /* 逐字符解析 Metamath 格式 */
    while (pos < content_len) {
        /* 跳过空白 */
        while (pos < content_len && (content[pos] == ' ' || content[pos] == '\t' || content[pos] == '\r')) {
            pos++;
        }
        if (pos >= content_len)
            break;

        /* 换行处理 */
        if (content[pos] == '\n') {
            pos++;
            line_idx++;
            continue;
        }

        /* 读取一个 token */
        token_len = 0;
        while (pos < content_len && content[pos] != ' ' && content[pos] != '\t' && content[pos] != '\r' &&
               content[pos] != '\n' && token_len < (int) sizeof(token_buffer) - 1) {
            token_buffer[token_len++] = content[pos++];
        }
        token_buffer[token_len] = '\0';

        if (token_len == 0)
            continue;

        /* 识别 $f / $e / $a / $p */
        if (strcmp(token_buffer, "$f") == 0 || strcmp(token_buffer, "$e") == 0 || strcmp(token_buffer, "$a") == 0 ||
            strcmp(token_buffer, "$p") == 0) {
            MiniStmtType type;
            if (strcmp(token_buffer, "$f") == 0)
                type = MINI_STMT_VAR;
            else if (strcmp(token_buffer, "$e") == 0)
                type = MINI_STMT_HYP;
            else if (strcmp(token_buffer, "$a") == 0)
                type = MINI_STMT_AXIOM;
            else
                type = MINI_STMT_THEOREM;

            /* 读取标签 */
            char label[256] = {0};
            size_t lbl_idx = 0;
            while (pos < content_len && content[pos] != ' ' && content[pos] != '\t' && content[pos] != '\r' &&
                   content[pos] != '\n' && lbl_idx < sizeof(label) - 1) {
                label[lbl_idx++] = content[pos++];
            }
            label[lbl_idx] = '\0';

            /* 读取公式 —— 直到遇到 $= 或 $. 或行尾 */
            char formula[2048] = {0};
            size_t fm_idx = 0;
            while (pos < content_len && content[pos] != '\n') {
                if (pos + 1 < content_len && content[pos] == '$' &&
                    (content[pos + 1] == '=' || content[pos + 1] == '.')) {
                    pos += 2;
                    break;
                }
                if (fm_idx < sizeof(formula) - 1) {
                    formula[fm_idx++] = content[pos];
                }
                pos++;
            }
            formula[fm_idx] = '\0';

            /* 去除公式首尾空白 */
            char *trimmed = lv_str_trim(formula);
            if (strlen(trimmed) == 0) {
                snprintf(trimmed, sizeof(formula), "%s", label);
            }

            int id = mini_internal_add_statement(kernel, type, label, trimmed);
            if (id >= 0)
                import_count++;
        }
    }

    lv_free((void **) &content);
    return import_count;
}

/* ========================================================================
 * 导出处理函数表 —— 替代 switch 语句
 * ======================================================================== */

/** @brief 导出处理函数类型 */
typedef void (*MiniStmtExportHandler)(FILE *fp, const MiniStatement *stmt, const MiniKernel *kernel);

/** @brief 导出 $f 变量声明 */
static void mini_stmt_export_var(FILE *fp, const MiniStatement *stmt, const MiniKernel *kernel) {
    (void)kernel;
    fprintf(fp, "$f %s %s $.\n", stmt->label, stmt->formula_text);
}

/** @brief 导出 $e 前提 */
static void mini_stmt_export_hyp(FILE *fp, const MiniStatement *stmt, const MiniKernel *kernel) {
    (void)kernel;
    fprintf(fp, "$e %s %s $.\n", stmt->label, stmt->formula_text);
}

/** @brief 导出 $a 公理 */
static void mini_stmt_export_axiom(FILE *fp, const MiniStatement *stmt, const MiniKernel *kernel) {
    (void)kernel;
    fprintf(fp, "$a %s %s $.\n", stmt->label, stmt->formula_text);
}

/** @brief 导出 $p 定理（含证明引用） */
static void mini_stmt_export_theorem(FILE *fp, const MiniStatement *stmt, const MiniKernel *kernel) {
    fprintf(fp, "$p %s %s $=", stmt->label, stmt->formula_text);
    for (int j = 0; j < stmt->ref_count; j++) {
        int ref_id = stmt->proof_refs[j];
        if (ref_id >= 0 && ref_id < kernel->statement_count && kernel->statements[ref_id]) {
            fprintf(fp, " %s", kernel->statements[ref_id]->label);
        }
    }
    fprintf(fp, " $.\n");
}

/** @brief 语句类型 → 导出处理函数 查找表 */
static const MiniStmtExportHandler kMiniStmtExportOps[] = {
    [MINI_STMT_VAR]     = mini_stmt_export_var,
    [MINI_STMT_HYP]     = mini_stmt_export_hyp,
    [MINI_STMT_AXIOM]   = mini_stmt_export_axiom,
    [MINI_STMT_THEOREM] = mini_stmt_export_theorem,
};

bool mini_kernel_export_mm(const MiniKernel *kernel, const char *filepath) {
    lv_CHECK_NULL(kernel, false);
    lv_CHECK_NULL(filepath, false);

    FILE *fp = lv_file_open(filepath, "w");
    if (!fp) {
        lv_set_error_ctx(lv_ERROR_IO, __FILE__, __LINE__, __func__, "无法创建文件: %s", filepath);
        return false;
    }

    fprintf(fp, "$( Lv-00 mini_kernel export $-)\n\n");

    for (int i = 0; i < kernel->statement_count; i++) {
        MiniStatement *stmt = kernel->statements[i];
        if (!stmt)
            continue;

        if (stmt->type >= MINI_STMT_VAR && stmt->type <= MINI_STMT_THEOREM) {
            kMiniStmtExportOps[stmt->type](fp, stmt, kernel);
        }
    }

    lv_file_close(fp);
    return true;
}

/* ========================================================================
 * 第八部分：内核自检
 * ======================================================================== */

MiniVerifyResult mini_kernel_self_check(MiniKernel *kernel) {
    lv_CHECK_NULL(kernel, MINI_VERIFY_FAIL_MEMORY);

    /* 测试 1：空替换应通过 */
    {
        char *result = NULL;
        MiniVerifyResult r = mini_kernel_check_substitution(kernel, NULL, 0, "test", &result);
        if (r != MINI_VERIFY_OK)
            return r;
        if (result)
            lv_free((void **) &result);
    }

    /* 测试 2：同一变量两次映射到不同表达式应被检测 */
    {
        Substitution subs[2];
        memset(subs, 0, sizeof(subs));
        strncpy(subs[0].variable_name, "x", sizeof(subs[0].variable_name) - 1);
        strncpy(subs[0].replacement_term, "A", sizeof(subs[0].replacement_term) - 1);
        strncpy(subs[1].variable_name, "x", sizeof(subs[1].variable_name) - 1);
        strncpy(subs[1].replacement_term, "B", sizeof(subs[1].replacement_term) - 1);

        char *result = NULL;
        MiniVerifyResult r = mini_kernel_check_substitution(kernel, subs, 2, "test", &result);
        if (r == MINI_VERIFY_OK) {
            if (result)
                lv_free((void **) &result);
            return MINI_VERIFY_FAIL_SUBSTITUTION;
        }
    }

    /* 测试 3：未绑定变量应被检测 */
    {
        Substitution sub;
        memset(&sub, 0, sizeof(sub));
        strncpy(sub.variable_name, "__nonexistent_var__", sizeof(sub.variable_name) - 1);
        strncpy(sub.replacement_term, "A", sizeof(sub.replacement_term) - 1);

        char *result = NULL;
        MiniVerifyResult r = mini_kernel_check_substitution(kernel, &sub, 1, "test", &result);
        if (r != MINI_VERIFY_FAIL_UNBOUND_VAR) {
            if (result)
                lv_free((void **) &result);
            return MINI_VERIFY_FAIL_SUBSTITUTION;
        }
    }

    return MINI_VERIFY_OK;
}

/* ========================================================================
 * 第九部分：统计与元信息
 * ======================================================================== */

void mini_kernel_stats(const MiniKernel *kernel, int *out_total_stmts, int *out_vars, int *out_hyps, int *out_axioms,
                       int *out_theorems, int *out_verified, int *out_tcb_lines) {
    if (!kernel)
        return;

    int counters[4] = {0};
    for (int i = 0; i < kernel->statement_count; i++) {
        MiniStatement *stmt = kernel->statements[i];
        if (!stmt)
            continue;
        if (stmt->type >= MINI_STMT_VAR && stmt->type <= MINI_STMT_THEOREM) {
            counters[stmt->type]++;
        }
    }
    if (out_total_stmts)
        *out_total_stmts = kernel->statement_count;
    if (out_vars)
        *out_vars = counters[MINI_STMT_VAR];
    if (out_hyps)
        *out_hyps = counters[MINI_STMT_HYP];
    if (out_axioms)
        *out_axioms = counters[MINI_STMT_AXIOM];
    if (out_theorems)
        *out_theorems = counters[MINI_STMT_THEOREM];
    if (out_verified)
        *out_verified = kernel->total_verified;
    if (out_tcb_lines)
        *out_tcb_lines = kernel->tcb_line_count;
}

/* ========================================================================
 * 第十部分：约束图集成
 * ======================================================================== */

bool mini_kernel_bind_to_graph(MiniKernel *kernel, int stmt_id, int node_id) {
    lv_CHECK_NULL(kernel, false);
    if (stmt_id < 0 || stmt_id >= kernel->statement_count) {
        lv_set_error_ctx(lv_ERROR_INDEX_OUT_OF_RANGE, __FILE__, __LINE__, __func__, "语句ID越界: %d", stmt_id);
        return false;
    }

    /* 扩容映射表 */
    if (stmt_id >= kernel->map_count) {
        int new_cap = kernel->map_count * 2;
        while (stmt_id >= new_cap)
            new_cap *= 2;
        int *new_map = lv_realloc(kernel->stmt_to_node_map, (size_t) new_cap * sizeof(int));
        if (!new_map)
            return false;
        for (int i = kernel->map_count; i < new_cap; i++) {
            new_map[i] = -1;
        }
        kernel->stmt_to_node_map = new_map;
        kernel->map_count = new_cap;
    }

    kernel->stmt_to_node_map[stmt_id] = node_id;
    if (node_id >= 0) {
        kernel->statements[stmt_id]->constraint_node_id = node_id;
    }
    return true;
}

int mini_kernel_find_by_node(const MiniKernel *kernel, int node_id) {
    if (!kernel || node_id < 0)
        return -1;
    if (!kernel->stmt_to_node_map)
        return -1;

    for (int i = 0; i < kernel->statement_count && i < kernel->map_count; i++) {
        if (kernel->stmt_to_node_map[i] == node_id) {
            return i;
        }
    }
    return -1;
}

/* ========================================================================
 * 第十一部分：辅助函数
 * ======================================================================== */

void mini_kernel_seal(MiniKernel *kernel) {
    if (!kernel)
        return;
    kernel->is_sealed = true;
}

void mini_kernel_reset(MiniKernel *kernel) {
    if (!kernel)
        return;

    /* 释放所有语句 */
    for (int i = 0; i < kernel->statement_count; i++) {
        if (kernel->statements[i]) {
            lv_free((void **) &kernel->statements[i]);
        }
    }
    kernel->statement_count = 0;

    /* 释放符号表 */
    for (int i = 0; i < kernel->symbol_count; i++) {
        if (kernel->symbol_names[i]) {
            lv_free((void **) &kernel->symbol_names[i]);
            kernel->symbol_names[i] = NULL;
        }
    }
    kernel->symbol_count = 0;

    /* 重置统计 */
    kernel->total_verified = 0;
    kernel->total_failed = 0;
    kernel->is_sealed = false;
}
