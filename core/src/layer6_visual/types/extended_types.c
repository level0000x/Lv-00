#include "lv/extended_types.h"

#include <stdlib.h>
#include <string.h>

#include "lv/lv_utils.h"
#include "lv/type_system.h"
#include "lv/lv_internal.h"

lvListTypeRegion *lv_list_type_create(void *elem_type) {
    lvListTypeRegion *t = lv_calloc(1, sizeof(lvListTypeRegion));
    /* calloc returns NULL on failure; caller must check */
    if (!t)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate list type region");
    t->elem_type = elem_type;
    return t;
}

void lv_list_type_destroy(lvListTypeRegion *t) {
    lv_free((void **) &t);
}

lvMapTypeRegion *lv_map_type_create(void *key_type, void *value_type) {
    lvMapTypeRegion *t = lv_calloc(1, sizeof(lvMapTypeRegion));
    /* calloc returns NULL on failure; caller must check */
    if (!t)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate map type region");
    t->key_type = key_type;
    t->value_type = value_type;
    return t;
}

void lv_map_type_destroy(lvMapTypeRegion *t) {
    lv_free((void **) &t);
}

lvFunctionTypeRegion *lv_function_type_create(void *param, void *ret, int dependent) {
    lvFunctionTypeRegion *t = lv_calloc(1, sizeof(lvFunctionTypeRegion));
    /* calloc returns NULL on failure; caller must check */
    if (!t)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate function type region");
    t->param_type = param;
    t->return_type = ret;
    t->is_dependent = dependent;
    return t;
}

void lv_function_type_destroy(lvFunctionTypeRegion *t) {
    lv_free((void **) &t);
}

lvEffectTypeRegion *lv_effect_type_create(lvEffectType *effects, int count, void *result) {
    lvEffectTypeRegion *t = lv_calloc(1, sizeof(lvEffectTypeRegion));
    /* calloc returns NULL on failure; caller must check */
    if (!t)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate effect type region");
    if (count > 0 && effects) {
        t->effects = lv_calloc(count, sizeof(lvEffectType));
        if (!t->effects) {
            lv_free((void **) &t);
            lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate effects array");
        }
        memcpy(t->effects, effects, count * sizeof(lvEffectType));
        t->effect_count = count;
    }
    t->result_type = result;
    return t;
}

void lv_effect_type_destroy(lvEffectTypeRegion *t) {
    if (!t)
        return;
    lv_free((void **) &t->effects);
    lv_free((void **) &t);
}

/* 扩展类型兼容性检查 */
/* 检查两个扩展类型是否兼容，支持协变/逆变规则 */
int lv_extended_type_compatible(void *a, void *b) {
    /* 指针相同则必然兼容 */
    if (a == b)
        return 1;

    /* 任一为 NULL 则不兼容 */
    if (!a || !b)
        return 0;

    /* 两个指针都指向 TypeRegion（基础类型系统） */
    TypeRegion *ta = (TypeRegion *) a;
    TypeRegion *tb = (TypeRegion *) b;

    /* 检查类型种类兼容性：同种类则兼容 */
    if (ta->kind != tb->kind) {
        /* 不同种类不兼容（BOTTOM 类型除外，BOTTOM 兼容所有类型） */
        if (ta->kind == TYPE_KIND_BOTTOM || tb->kind == TYPE_KIND_BOTTOM) {
            return 1;
        }
        return 0;
    }

    /* 同种类，根据具体种类执行深度兼容性检查 */
    switch (ta->kind) {
        case TYPE_KIND_FUNCTION:
            /* 函数类型：输入逆变，输出协变 */
            /* 即 (A -> B) 兼容 (C -> D) 当且仅当 C 兼容 A 且 B 兼容 D */
            if (ta->input_type && tb->input_type) {
                /* 输入参数逆变：tb->input_type 需兼容 ta->input_type */
                if (!lv_extended_type_compatible(tb->input_type, ta->input_type)) {
                    return 0;
                }
            }
            if (ta->output_type && tb->output_type) {
                /* 输出参数协变：ta->output_type 需兼容 tb->output_type */
                if (!lv_extended_type_compatible(ta->output_type, tb->output_type)) {
                    return 0;
                }
            }
            return 1;

        case TYPE_KIND_PRODUCT:
            /* 乘积类型：每个分量都需要兼容（协变） */
            if (ta->left_type && tb->left_type) {
                if (!lv_extended_type_compatible(ta->left_type, tb->left_type)) {
                    return 0;
                }
            }
            if (ta->right_type && tb->right_type) {
                if (!lv_extended_type_compatible(ta->right_type, tb->right_type)) {
                    return 0;
                }
            }
            return 1;

        case TYPE_KIND_SUM:
            /* 和类型：两个分支都需要兼容 */
            if (ta->first_type && tb->first_type) {
                if (!lv_extended_type_compatible(ta->first_type, tb->first_type)) {
                    return 0;
                }
            }
            if (ta->second_type && tb->second_type) {
                if (!lv_extended_type_compatible(ta->second_type, tb->second_type)) {
                    return 0;
                }
            }
            return 1;

        case TYPE_KIND_DEPENDENT:
            /* 依赖类型：参数节点ID相同且体类型兼容 */
            if (ta->param_node_id != tb->param_node_id) {
                return 0;
            }
            if (ta->body_type && tb->body_type) {
                return lv_extended_type_compatible(ta->body_type, tb->body_type);
            }
            return 1;

        case TYPE_KIND_VARIABLE:
            /* 类型变量：同名或同ID则兼容 */
            if (ta->variable_id == tb->variable_id)
                return 1;
            if (ta->variable_name && tb->variable_name && strcmp(ta->variable_name, tb->variable_name) == 0) {
                return 1;
            }
            return 0;

        case TYPE_KIND_POINT:
        case TYPE_KIND_LINE_SEGMENT:
        case TYPE_KIND_REGION:
            /* 基本几何类型：同种类即兼容 */
            return 1;

        case TYPE_KIND_BOTTOM:
            /* BOTTOM 兼容所有类型 */
            return 1;

        case TYPE_KIND_PREDICATE_SUBTYPE:
            /* 谓词子类型：基类型兼容即可 */
            if (ta->base_type && tb->base_type) {
                return lv_extended_type_compatible(ta->base_type, tb->base_type);
            }
            return 1;

        default:
            return 0;
    }
}
