/**
 * @file unify_simplify.c
 * @brief simple proposition and proof
 * @details Split from unify.c
 */

#include "lv/unify.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/geometric_primitives.h"
#include "lv/proof.h"

#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/normalization.h"
#include "lv/stream.h"
#include "lv/stream.h"
#include "lv/type_system.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_lifecycle.h"
#include "unify_internal.h"

/* ---------------------------------------------------------------------------
 * 简化命题与证明
 * ------------------------------------------------------------------------- */

SimpleProposition *simple_proposition_create(const char *name, int *input_port_ids, int input_count,
                                             int *output_port_ids, int output_count) {
    SimpleProposition *prop = lv_calloc(1, sizeof(SimpleProposition));
    if (!prop)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "simple_proposition_create: calloc prop failed");

    /* 使用 lv_strdup_safe 替代裸 strdup，统一内存管理，
     * 确保内存统计正确且避免混用标准 free 与 lv_free。
     * 当 name 为 NULL 时，使用空字符串作为默认值。 */
    prop->name = name ? lv_strdup_safe(name) : lv_strdup_safe("");
    if (!prop->name) {
        lv_free((void **) &prop);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "simple_proposition_create: strdup name failed");
    }

    prop->pattern = graph_create();
    if (!prop->pattern) {
        lv_free((void **) &prop->name);
        lv_free((void **) &prop);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "simple_proposition_create: graph_create failed");
    }

    /* 检查 input_port_ids 和 output_port_ids 是否为 NULL。
     * 当 count > 0 但对应数组为 NULL 时，视为参数错误，返回 NULL。 */
    if ((input_count > 0 && !input_port_ids) || (output_count > 0 && !output_port_ids)) {
        graph_destroy(prop->pattern);
        lv_free((void **) &prop->name);
        lv_free((void **) &prop);
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "simple_proposition_create: NULL port_ids with non-zero count");
    }

    prop->input_port_ids = input_count > 0 ? lv_calloc((size_t) input_count, sizeof(int)) : NULL;
    if (input_count > 0 && prop->input_port_ids) {
        memcpy(prop->input_port_ids, input_port_ids, (size_t) input_count * sizeof(int));
    }
    prop->output_port_ids = output_count > 0 ? lv_calloc((size_t) output_count, sizeof(int)) : NULL;
    if (output_count > 0 && prop->output_port_ids) {
        memcpy(prop->output_port_ids, output_port_ids, (size_t) output_count * sizeof(int));
    }
    prop->input_count = input_count;
    prop->output_count = output_count;
    return prop;
}

LV_DESTROY_SHIM(destroy_simple_prop_pattern, ConstraintGraph, graph_destroy);

/* 简单命题字段销毁表（判据 G：pattern 归 object 语义，其余指针字段 PLAIN；name 释放后由表置 NULL） */
static const lvFieldDesc s_simple_prop_destroy_fields[] = {
    lv_FIELD_PLAIN(SimpleProposition, name),
    lv_FIELD_PLAIN(SimpleProposition, input_port_ids),
    lv_FIELD_PLAIN(SimpleProposition, output_port_ids),
    lv_FIELD_OBJECT(SimpleProposition, pattern, destroy_simple_prop_pattern),
};

void simple_proposition_destroy(SimpleProposition *prop) {
    if (prop) {
        lv_obj_destroy_fields(prop, s_simple_prop_destroy_fields, lv_ARRAY_SIZE(s_simple_prop_destroy_fields));
        lv_free((void **) &prop);
    }
}

SimpleProof *simple_proof_create(SimpleProposition *prop, ConstraintGraph *construction) {
    SimpleProof *proof = lv_calloc(1, sizeof(SimpleProof));
    if (!proof)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "simple_proof_create: calloc proof failed");
    proof->proposition = prop;
    proof->construction = construction;
    proof->normalized = false;
    proof->passed = false;
    return proof;
}

void simple_proof_destroy(SimpleProof *proof) {
    if (proof) {
        graph_destroy(proof->construction);
        lv_free((void **) &proof);
    }
}

bool simple_proof_check(SimpleProof *proof) {
    if (!proof->normalized) {
        simple_proof_normalize(proof);
    }

    /* 层级1：基本构造-命题合一 */
    UnifyStatus status = unify_construction_with_proposition(proof->construction, proof->proposition->pattern);

    if (status != UNIFY_STATUS_OK) {
        proof->passed = false;
        return false;
    }

    /* 层级2：坐标级别的等价验证。
     * 根据 design_v2.9.md Section 10.2：基本匹配通过后，
     * 验证坐标是否代数等价。 */
    UnifyStatus coord_status =
        unify_construction_with_proposition_coord(proof->construction, proof->proposition->pattern);

    proof->passed = (coord_status == UNIFY_STATUS_OK);
    return proof->passed;
}

void simple_proof_normalize(SimpleProof *proof) {
    NormalizationResult *nr = graph_normalize(proof->construction, true);
    if (nr) {
        proof->normalized = true;
        normalization_result_destroy(nr);
    }
}
