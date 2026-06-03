#include "lv00/extended_types.h"
#include <stdlib.h>

Lv00ListTypeRegion *lv00_list_type_create(void *elem_type) {
    Lv00ListTypeRegion *t = calloc(1, sizeof(Lv00ListTypeRegion));
    if (!t) return NULL;
    t->elem_type = elem_type;
    return t;
}

void lv00_list_type_destroy(Lv00ListTypeRegion *t) {
    free(t);
}

Lv00MapTypeRegion *lv00_map_type_create(void *key_type, void *value_type) {
    Lv00MapTypeRegion *t = calloc(1, sizeof(Lv00MapTypeRegion));
    if (!t) return NULL;
    t->key_type = key_type;
    t->value_type = value_type;
    return t;
}

void lv00_map_type_destroy(Lv00MapTypeRegion *t) {
    free(t);
}

Lv00FunctionTypeRegion *lv00_function_type_create(void *param, void *ret, int dependent) {
    Lv00FunctionTypeRegion *t = calloc(1, sizeof(Lv00FunctionTypeRegion));
    if (!t) return NULL;
    t->param_type = param;
    t->return_type = ret;
    t->is_dependent = dependent;
    return t;
}

void lv00_function_type_destroy(Lv00FunctionTypeRegion *t) {
    free(t);
}

Lv00EffectTypeRegion *lv00_effect_type_create(Lv00EffectType *effects, int count, void *result) {
    Lv00EffectTypeRegion *t = calloc(1, sizeof(Lv00EffectTypeRegion));
    if (!t) return NULL;
    if (count > 0 && effects) {
        t->effects = calloc(count, sizeof(Lv00EffectType));
        memcpy(t->effects, effects, count * sizeof(Lv00EffectType));
        t->effect_count = count;
    }
    t->result_type = result;
    return t;
}

void lv00_effect_type_destroy(Lv00EffectTypeRegion *t) {
    if (!t) return;
    free(t->effects);
    free(t);
}

int lv00_extended_type_compatible(void *a, void *b) {
    /* TODO: implement type compatibility checking */
    return a == b;
}
