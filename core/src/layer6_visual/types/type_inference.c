#include "lv00/extended_types.h"
#include "lv00/lv00_utils.h"
#include <stdlib.h>
#include <string.h>

/* Enhanced type inference for Layer 6 generic types */
/* Infers type parameters for List<T>, Map<K,V>, etc. */

/* 单条推理规则 */
typedef struct {
    char pattern[128];   /* 匹配模式（子串） */
    char type_name[128]; /* 推理出的类型名 */
} TypeInferenceRule;

typedef struct Lv00TypeInference {
    void *type_env;
    int inference_depth;

    /* 自定义规则表 */
    TypeInferenceRule *rules;
    int rule_count;
    int rule_capacity;
} Lv00TypeInference;

Lv00TypeInference *lv00_type_inference_create(void) {
    Lv00TypeInference *inf = lv00_calloc(1, sizeof(Lv00TypeInference));
    if (!inf) return NULL;
    inf->rule_capacity = 8;
    inf->rules = lv00_calloc(inf->rule_capacity, sizeof(TypeInferenceRule));
    if (!inf->rules) {
        lv00_free((void **)&inf);
        return NULL;
    }
    return inf;
}

void lv00_type_inference_destroy(Lv00TypeInference *inf) {
    if (!inf) return;
    lv00_free((void **)&inf->rules);
    lv00_free((void **)&inf);
}

/* 注册一条 pattern->type 推理规则 */
int lv00_type_inference_register_rule(Lv00TypeInference *inf,
                                       const char *pattern, const char *type) {
    if (!inf || !pattern || !type) return -1;
    if (inf->rule_count >= inf->rule_capacity) {
        int new_cap = inf->rule_capacity * 2;
        TypeInferenceRule *new_arr = lv00_realloc(inf->rules,
                                              new_cap * sizeof(TypeInferenceRule));
        if (!new_arr) return -1;
        inf->rules = new_arr;
        inf->rule_capacity = new_cap;
    }
    strncpy(inf->rules[inf->rule_count].pattern, pattern,
            sizeof(inf->rules[0].pattern) - 1);
    inf->rules[inf->rule_count].pattern[sizeof(inf->rules[0].pattern) - 1] = '\0';
    strncpy(inf->rules[inf->rule_count].type_name, type,
            sizeof(inf->rules[0].type_name) - 1);
    inf->rules[inf->rule_count].type_name[sizeof(inf->rules[0].type_name) - 1] = '\0';
    inf->rule_count++;
    return 0;
}

/* 基于规则的简单类型推理 */
int lv00_type_inference_infer(Lv00TypeInference *inf, const char *expr,
                               char *result_type, size_t size) {
    if (!inf || !expr || !result_type || size == 0) return -1;

    /* 先检查自定义规则（后注册的优先） */
    for (int i = inf->rule_count - 1; i >= 0; i--) {
        if (strstr(expr, inf->rules[i].pattern) != NULL) {
            strncpy(result_type, inf->rules[i].type_name, size - 1);
            result_type[size - 1] = '\0';
            return 0;
        }
    }

    /* 内置默认规则 */
    if (strstr(expr, "+") != NULL || strstr(expr, "-") != NULL ||
        strstr(expr, "*") != NULL || strstr(expr, "/") != NULL) {
        strncpy(result_type, "Number", size - 1);
        result_type[size - 1] = '\0';
        return 0;
    }
    if (strstr(expr, "point") != NULL || strstr(expr, "Point") != NULL) {
        strncpy(result_type, "Point", size - 1);
        result_type[size - 1] = '\0';
        return 0;
    }
    if (strstr(expr, "line") != NULL || strstr(expr, "Line") != NULL) {
        strncpy(result_type, "Line", size - 1);
        result_type[size - 1] = '\0';
        return 0;
    }
    if (strstr(expr, "circle") != NULL || strstr(expr, "Circle") != NULL) {
        strncpy(result_type, "Circle", size - 1);
        result_type[size - 1] = '\0';
        return 0;
    }
    if (strstr(expr, "\"") != NULL || strstr(expr, "'") != NULL) {
        strncpy(result_type, "String", size - 1);
        result_type[size - 1] = '\0';
        return 0;
    }
    if (strstr(expr, "true") != NULL || strstr(expr, "false") != NULL) {
        strncpy(result_type, "Boolean", size - 1);
        result_type[size - 1] = '\0';
        return 0;
    }

    /* 无法推理 */
    strncpy(result_type, "Unknown", size - 1);
    result_type[size - 1] = '\0';
    return 1;
}
