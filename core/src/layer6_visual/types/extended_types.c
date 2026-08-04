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
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate list type region");
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
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate map type region");
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
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate function type region");
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
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate effect type region");
    if (count > 0 && effects) {
        t->effects = lv_calloc(count, sizeof(lvEffectType));
        if (!t->effects) {
            lv_free((void **) &t);
            lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate effects array");
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

/* 类型兼容性检查处理器函数指针类型 */
typedef int (*TypeCompatibleHandler)(const TypeRegion *ta, const TypeRegion *tb);

/* 函数类型兼容性检查：输入逆变，输出协变 */
static int compat_function(const TypeRegion *ta, const TypeRegion *tb) {
    if (ta->input_type && tb->input_type) {
        if (!lv_extended_type_compatible(tb->input_type, ta->input_type)) {
            return 0;
        }
    }
    if (ta->output_type && tb->output_type) {
        if (!lv_extended_type_compatible(ta->output_type, tb->output_type)) {
            return 0;
        }
    }
    return 1;
}

/* 乘积类型兼容性检查：每个分量协变 */
static int compat_product(const TypeRegion *ta, const TypeRegion *tb) {
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
}

/* 和类型兼容性检查：两个分支都需要兼容 */
static int compat_sum(const TypeRegion *ta, const TypeRegion *tb) {
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
}

/* 依赖类型兼容性检查：参数节点ID相同且体类型兼容 */
static int compat_dependent(const TypeRegion *ta, const TypeRegion *tb) {
    if (ta->param_node_id != tb->param_node_id) {
        return 0;
    }
    if (ta->body_type && tb->body_type) {
        return lv_extended_type_compatible(ta->body_type, tb->body_type);
    }
    return 1;
}

/* 类型变量兼容性检查：同名或同ID则兼容 */
static int compat_variable(const TypeRegion *ta, const TypeRegion *tb) {
    if (ta->variable_id == tb->variable_id)
        return 1;
    if (ta->variable_name && tb->variable_name && strcmp(ta->variable_name, tb->variable_name) == 0) {
        return 1;
    }
    return 0;
}

/* 基本几何类型兼容性检查：同种类即兼容 */
static int compat_geom_primitive(const TypeRegion *ta, const TypeRegion *tb) {
    (void)ta;
    (void)tb;
    return 1;
}

/* BOTTOM 类型兼容性检查：兼容所有类型 */
static int compat_bottom(const TypeRegion *ta, const TypeRegion *tb) {
    (void)ta;
    (void)tb;
    return 1;
}

/* 谓词子类型兼容性检查：基类型兼容即可 */
static int compat_predicate_subtype(const TypeRegion *ta, const TypeRegion *tb) {
    if (ta->base_type && tb->base_type) {
        return lv_extended_type_compatible(ta->base_type, tb->base_type);
    }
    return 1;
}

/* 默认兼容性检查：不兼容 */
static int compat_default(const TypeRegion *ta, const TypeRegion *tb) {
    (void)ta;
    (void)tb;
    return 0;
}

/* 扩展类型兼容性检查 */
/* 检查两个扩展类型是否兼容，支持协变/逆变规则 */
int lv_extended_type_compatible(void *a, void *b) {
    /* 任一为 NULL 则不兼容 */
    if (!a || !b)
        return 0;

    /* 指针相同则必然兼容 */
    if (a == b)
        return 1;

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

    /* 同种类，使用 VTable 查找表进行类型兼容性检查 */
    static const TypeCompatibleHandler kTypeCompatibleHandlers[] = {
        [TYPE_KIND_FUNCTION]         = compat_function,
        [TYPE_KIND_PRODUCT]          = compat_product,
        [TYPE_KIND_SUM]              = compat_sum,
        [TYPE_KIND_DEPENDENT]        = compat_dependent,
        [TYPE_KIND_VARIABLE]         = compat_variable,
        [TYPE_KIND_POINT]            = compat_geom_primitive,
        [TYPE_KIND_LINE_SEGMENT]     = compat_geom_primitive,
        [TYPE_KIND_REGION]           = compat_geom_primitive,
        [TYPE_KIND_BOTTOM]           = compat_bottom,
        [TYPE_KIND_PREDICATE_SUBTYPE]= compat_predicate_subtype,
    };

    if (ta->kind >= 0 && ta->kind < (int)(sizeof(kTypeCompatibleHandlers)/sizeof(kTypeCompatibleHandlers[0])) && kTypeCompatibleHandlers[ta->kind]) {
        return kTypeCompatibleHandlers[ta->kind](ta, tb);
    }
    return 0;
}
