#include <stdlib.h>
#include <string.h>

#include "lv/extended_types.h"
#include "lv/lv_utils.h"
#include "lv/lv_internal.h"

/* Enhanced type inference for Layer 6 generic types */
/* Infers type parameters for List<T>, Map<K,V>, etc. */

/* 单条推理规则 */
typedef struct {
    char pattern[128];   /* 匹配模式（子串） */
    char type_name[128]; /* 推理出的类型名 */
} lvTypeInferenceRule;

typedef struct lvTypeInference {
    void *type_env;
    int inference_depth;

    /* 自定义规则表 */
    lvDArray rules;  /**< lvTypeInferenceRule 动态数组 */
} lvTypeInference;

lvTypeInference *lv_type_inference_create(void) {
    lvTypeInference *inf = lv_calloc(1, sizeof(lvTypeInference));
    if (!inf)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate type inference");
    lv_darray_init(&inf->rules, sizeof(lvTypeInferenceRule));
    if (!lv_darray_reserve(&inf->rules, 8)) {
        lv_free((void **) &inf);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate rules array");
    }
    return inf;
}

void lv_type_inference_destroy(lvTypeInference *inf) {
    if (!inf)
        return;
    lv_darray_free(&inf->rules);
    lv_free((void **) &inf);
}

/* 注册一条 pattern->type 推理规则 */
int lv_type_inference_register_rule(lvTypeInference *inf, const char *pattern, const char *type) {
    if (!inf || !pattern || !type)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "NULL inf, pattern, or type");
    lvTypeInferenceRule rule;
    lv_strlcpy(rule.pattern, pattern, sizeof(rule.pattern));
    lv_strlcpy(rule.type_name, type, sizeof(rule.type_name));
    int idx = lv_darray_push(&inf->rules, &rule);
    if (idx < 0)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "failed to push rule");
    return 0;
}

/* 基于规则的简单类型推理 */
int lv_type_inference_infer(lvTypeInference *inf, const char *expr, char *result_type, size_t size) {
    if (!inf || !expr || !result_type || size == 0)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "NULL inf, expr, or result_type");

    /* 先检查自定义规则（后注册的优先） */
    for (int i = inf->rules.count - 1; i >= 0; i--) {
        lvTypeInferenceRule *rule = (lvTypeInferenceRule *) lv_darray_get(&inf->rules, i);
        if (strstr(expr, rule->pattern) != NULL) {
            lv_strlcpy(result_type, rule->type_name, size);
            return 0;
        }
    }

    /* 内置默认规则 */
    if (strstr(expr, "+") != NULL || strstr(expr, "-") != NULL || strstr(expr, "*") != NULL ||
        strstr(expr, "/") != NULL) {
        lv_strlcpy(result_type, "Number", size);
        return 0;
    }
    if (strstr(expr, "point") != NULL || strstr(expr, "Point") != NULL) {
        lv_strlcpy(result_type, "Point", size);
        return 0;
    }
    if (strstr(expr, "line") != NULL || strstr(expr, "Line") != NULL) {
        lv_strlcpy(result_type, "Line", size);
        return 0;
    }
    if (strstr(expr, "circle") != NULL || strstr(expr, "Circle") != NULL) {
        lv_strlcpy(result_type, "Circle", size);
        return 0;
    }
    if (strstr(expr, "\"") != NULL || strstr(expr, "'") != NULL) {
        lv_strlcpy(result_type, "String", size);
        return 0;
    }
    if (strstr(expr, "true") != NULL || strstr(expr, "false") != NULL) {
        lv_strlcpy(result_type, "Boolean", size);
        return 0;
    }

    /* 无法推理 */
    lv_strlcpy(result_type, "Unknown", size);
    return 1;
}
