/**
 * @file axiom_template_test.c
 * @brief 约束模板双层测试框架 —— 测试执行器与正则形式验证
 *
 * 提供：
 * 1. 出厂测试集与用户测试集的双层测试执行
 * 2. 模板正则形式验证（图结构比对）
 * 3. 测试用例的创建与销毁
 *
 * 测试执行器是纯比对函数，不执行任何推理，不使用约束求解器。
 *
 * v3.5.0 新增烟测保护机制：
 * - 独立步数上限：每个用例的约束生成步数上限，超限标记为 TIMEOUT
 * - 总时间上限：整个烟测集执行总时间上限，超限标记后续用例为 SKIPPED
 * - 加载策略：若有 TIMEOUT/SKIPPED，发出警告但仍允许加载
 */

#include "lv/axiom_pkg.h"
#include "lv/constraint_graph.h"
#include "lv/lv.h"
#include "lv_internal.h"
#include "debug.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

/* ============== 辅助：图结构比对 ============== */

/**
 * @brief 比较两个约束图的结构是否一致
 *
 * 纯结构比对：比较节点数量、约束数量以及每个约束的类型和参与者数量。
 * 不进行语义等价性判断，也不调用约束求解器。
 *
 * @return true 结构一致
 */
static bool graph_structure_match(const ConstraintGraph *a, const ConstraintGraph *b) {
    if (!a || !b) return false;

    /* 比较节点数量和约束数量 */
    if (a->node_count != b->node_count) return false;
    if (a->constraint_count != b->constraint_count) return false;

    /* 防御性检查：确保 constraints 数组已分配 */
    if (!a->constraints || !b->constraints) return false;

    /* 比较每个约束的类型和参与者数量 */
    for (int i = 0; i < a->constraint_count; i++) {
        Constraint *ca = a->constraints[i];
        Constraint *cb = b->constraints[i];
        if (!ca || !cb) return false;
        if (ca->type != cb->type) return false;
        if (ca->participant_count != cb->participant_count) return false;
    }

    return true;
}

/* ============== 烟测时间监控 ============== */

/**
 * @brief 获取当前单调时间（毫秒）
 * 用于烟测超时检测。
 */
static uint64_t get_current_time_ms(void) {
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
}

/* ============== 模板测试执行器 ============== */

int axiom_template_test_run(AxiomPackage *pkg, TemplateTestCase **test_cases, int count,
                             int *out_passed, int *out_failed, char ***out_failures)
{
    if (!pkg || !test_cases || count <= 0 || !out_passed || !out_failed || !out_failures) {
        return -1;
    }

    *out_passed = 0;
    *out_failed = 0;
    *out_failures = NULL;

    /* 读取烟测保护配置 */
    int step_limit = lv_config_get_int("smoke_test_step_limit", 1000);
    int timeout_ms = lv_config_get_int("smoke_test_timeout_ms", 30000);
    uint64_t start_time = get_current_time_ms();

    /* 分配失败消息数组 */
    char **failures = lv_calloc((size_t)count, sizeof(char *));
    if (!failures) return -1;

    int fail_count = 0;
    int timed_out = 0;
    int skipped = 0;

    for (int i = 0; i < count; i++) {
        TemplateTestCase *tc = test_cases[i];

        /* 检查总时间是否超限 */
        uint64_t elapsed = get_current_time_ms() - start_time;
        if (elapsed >= (uint64_t)timeout_ms) {
            /* 当前及剩余用例标记为 SKIPPED */
            for (int j = i; j < count; j++) {
                skipped++;
                if (fail_count < count && test_cases[j]) {
                    char msg[128];
                    int _sn_ret;
                    lv_SAFE_SNPRINTF(_sn_ret, msg, sizeof(msg),
                                     "[SKIPPED] '%s': total time exceeded (%d ms limit)",
                                     test_cases[j]->template_name, timeout_ms);
                    (void)_sn_ret;
                    failures[fail_count] = lv_strdup_safe(msg);
                    if (failures[fail_count]) fail_count++;
                }
            }
            break;
        }

        if (!tc) {
            /* 空测试用例视为 ERROR */
            if (fail_count < count) {
                failures[fail_count] = lv_strdup_safe("[ERROR] (null test case)");
                if (failures[fail_count]) fail_count++;
            }
            continue;
        }

        /* 查找模板 */
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, tc->template_name);
        if (!tmpl) {
            if (fail_count < count) {
                char msg[128];
                int _sn_ret;
                lv_SAFE_SNPRINTF(_sn_ret, msg, sizeof(msg),
                                 "[ERROR] Template '%s' not found", tc->template_name);
                (void)_sn_ret;
                failures[fail_count] = lv_strdup_safe(msg);
                if (failures[fail_count]) fail_count++;
            }
            continue;
        }

        /* 如果模板没有 expand 函数，无法运行测试 */
        if (!tmpl->expand) {
            if (fail_count < count) {
                char msg[128];
                int _sn_ret;
                lv_SAFE_SNPRINTF(_sn_ret, msg, sizeof(msg),
                                 "[ERROR] Template '%s' has no expand function", tc->template_name);
                (void)_sn_ret;
                failures[fail_count] = lv_strdup_safe(msg);
                if (failures[fail_count]) fail_count++;
            }
            continue;
        }

        /* 创建临时图并展开模板 */
        ConstraintGraph *expanded = graph_create();
        if (!expanded) {
            if (fail_count < count) {
                failures[fail_count] = lv_strdup_safe("[ERROR] Out of memory creating temp graph");
                if (failures[fail_count]) fail_count++;
            }
            continue;
        }

        tmpl->expand(tc->params, expanded);

        /* 步数检查：展开后的约束数量作为步数指标 */
        int step_count = expanded->constraint_count;
        if (step_count > step_limit) {
            timed_out++;
            if (fail_count < count) {
                char msg[256];
                int _sn_ret;
                lv_SAFE_SNPRINTF(_sn_ret, msg, sizeof(msg),
                                 "[TIMEOUT] '%s': step count %d exceeds limit %d (nodes=%d)",
                                 tc->template_name, step_count, step_limit,
                                 expanded->node_count);
                (void)_sn_ret;
                failures[fail_count] = lv_strdup_safe(msg);
                if (failures[fail_count]) fail_count++;
            }
            graph_destroy(expanded);
            continue;
        }

        /* 判断展开是否"通过"：产生了有效约束 */
        bool actual_pass = (expanded->constraint_count > 0);

        /* 如果提供了预期图，进行结构比对 */
        if (tc->expected_graph != NULL) {
            actual_pass = graph_structure_match(expanded, tc->expected_graph);
        }

        /* 比对实际结果与预期结果 */
        if (actual_pass == tc->expected_result) {
            (*out_passed)++;
        } else {
            if (fail_count < count) {
                char msg[256];
                int _sn_ret;
                lv_SAFE_SNPRINTF(_sn_ret, msg, sizeof(msg),
                                 "[FAIL] '%s': expected %s, got %s (nodes=%d, constraints=%d)",
                                 tc->template_name,
                                 tc->expected_result ? "pass" : "fail",
                                 actual_pass ? "pass" : "fail",
                                 expanded->node_count,
                                 expanded->constraint_count);
                (void)_sn_ret;
                failures[fail_count] = lv_strdup_safe(msg);
                if (failures[fail_count]) fail_count++;
            }
        }

        graph_destroy(expanded);
    }

    *out_failed = fail_count;
    *out_failures = failures;

    /* 加载策略：若有 TIMEOUT 或 SKIPPED，发出警告但仍允许加载 */
    if (timed_out > 0 || skipped > 0) {
        LOG_WARN("axiom_template_test", "Smoke test completed with %d timed out, %d skipped (of %d total)",
                 timed_out, skipped, count);
    }

    return fail_count;
}

/* ============== 模板正则形式验证 ============== */

bool axiom_template_verify_normal_form(AxiomPackage *pkg, const char *template_name)
{
    if (!pkg || !template_name) return false;

    /* 查找模板 */
    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, template_name);
    if (!tmpl) {
        LOG_ERROR("axiom_template_test", "Template '%s' not found for normal form verification",
                  template_name);
        return false;
    }

    /* 如果模板没有 expand 函数，无法验证 */
    if (!tmpl->expand) {
        LOG_ERROR("axiom_template_test", "Template '%s' has no expand function", template_name);
        return false;
    }

    /* 如果没有定义正则形式，跳过验证（视为通过） */
    if (tmpl->normal_form.constraint_type_count <= 0 &&
        tmpl->normal_form.node_type_count <= 0) {
        LOG_INFO("axiom_template_test", "Template '%s' has no normal form defined, skipping",
                 template_name);
        return true;
    }

    /* 创建临时图并展开模板 */
    ConstraintGraph *expanded = graph_create();
    if (!expanded) {
        LOG_ERROR("axiom_template_test", "Out of memory creating temp graph");
        return false;
    }

    tmpl->expand(tmpl->params ? (SymbolicCoord **)tmpl->params : NULL, expanded);

    /* 使用现有的正则形式验证函数 */
    /* 构建规范形式字符串："CONSTRAINT_TYPE(NODE_TYPE,...)+" */
    char canonical_buf[256] = {0};
    int pos = 0;

    for (int i = 0; i < tmpl->normal_form.constraint_type_count && pos < 240; i++) {
        /* 追加约束类型编号 */
        int _n;
        lv_SAFE_SNPRINTF(_n, &canonical_buf[pos], (size_t)(256 - pos),
                         "%d(", tmpl->normal_form.expected_constraint_types[i]);
        pos += _n;

        /* 追加节点类型 */
        for (int j = 0; j < tmpl->normal_form.node_type_count && pos < 240; j++) {
            if (j > 0 && pos < 255) {
                canonical_buf[pos++] = ',';
            }
            lv_SAFE_SNPRINTF(_n, &canonical_buf[pos], (size_t)(256 - pos),
                             "%d", tmpl->normal_form.expected_node_types[j]);
            pos += _n;
        }
        if (pos < 255) {
            canonical_buf[pos++] = ')';
            if (i < tmpl->normal_form.constraint_type_count - 1 && pos < 254) {
                canonical_buf[pos++] = '+';
            }
        }
    }
    canonical_buf[pos] = '\0';

    bool result = axiom_template_validate_normal_form(tmpl, expanded, canonical_buf);

    graph_destroy(expanded);
    return result;
}

/* ============== 测试用例生命周期管理 ============== */

TemplateTestCase *axiom_template_test_case_create(const char *name, TestCaseType type,
                                                    int param_count, bool expected)
{
    if (!name || param_count < 0) return NULL;

    TemplateTestCase *tc = lv_calloc(1, sizeof(TemplateTestCase));
    if (!tc) return NULL;

    tc->template_name = lv_strdup_safe(name);
    if (!tc->template_name) {
        lv_free((void **)&tc);
        return NULL;
    }

    tc->type = type;
    tc->param_count = param_count;
    tc->params = NULL;          /* 调用者可后续设置 */
    tc->expected_graph = NULL;  /* 调用者可后续设置 */
    tc->expected_result = expected;
    tc->description = NULL;

    return tc;
}

TemplateTestCase *axiom_template_test_case_copy(const TemplateTestCase *src)
{
    if (!src) return NULL;

    TemplateTestCase *dst = lv_calloc(1, sizeof(TemplateTestCase));
    if (!dst) return NULL;

    /* 深拷贝基本字段 */
    dst->template_name = lv_strdup_safe(src->template_name);
    if (src->template_name && !dst->template_name) {
        lv_free((void **)&dst);
        return NULL;
    }

    dst->type = src->type;
    dst->param_count = src->param_count;
    dst->expected_result = src->expected_result;

    dst->description = lv_strdup_safe(src->description);
    if (src->description && !dst->description) {
        lv_free((void **)&dst->template_name);
        lv_free((void **)&dst);
        return NULL;
    }

    /* params 浅拷贝（单个元素由调用者管理） */
    dst->params = NULL;
    if (src->params && src->param_count > 0) {
        dst->params = lv_calloc((size_t)src->param_count, sizeof(SymbolicCoord *));
        if (!dst->params) {
            lv_free((void **)&dst->template_name);
            lv_free((void **)&dst->description);
            lv_free((void **)&dst);
            return NULL;
        }
        memcpy(dst->params, src->params, (size_t)src->param_count * sizeof(SymbolicCoord *));
    }

    /* expected_graph 浅拷贝（由调用者管理生命周期） */
    dst->expected_graph = src->expected_graph;

    return dst;
}

void axiom_template_test_case_destroy(TemplateTestCase *tc)
{
    if (!tc) return;

    lv_free((void **)&tc->template_name);
    lv_free((void **)&tc->description);

    /* params 数组中的每个 SymbolicCoord 由调用者管理，
     * 此处只释放数组本身（浅释放） */
    if (tc->params) {
        lv_free((void **)&tc->params);
    }

    /* expected_graph 由调用者管理，此处不释放 */
    lv_free((void **)&tc);
}
