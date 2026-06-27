#ifndef LV00_EXTENDED_TYPES_H
#define LV00_EXTENDED_TYPES_H

#include "lv00/type_system.h"
#include "lv00/effect_system.h"

#ifdef __cplusplus
extern "C" {
#endif

/* List type region */
typedef struct Lv00ListTypeRegion {
    void *base;  /* TypeRegion base */
    void *elem_type;
} Lv00ListTypeRegion;

/* Map type region */
typedef struct Lv00MapTypeRegion {
    void *base;
    void *key_type;
    void *value_type;
} Lv00MapTypeRegion;

/* Function type region (dependent) */
typedef struct Lv00FunctionTypeRegion {
    void *base;
    void *param_type;
    void *return_type;
    int is_dependent;
} Lv00FunctionTypeRegion;

/* Effect type region */
typedef struct Lv00EffectTypeRegion {
    void *base;
    Lv00EffectType *effects;
    int effect_count;
    void *result_type;
} Lv00EffectTypeRegion;

/* Factory */
Lv00ListTypeRegion *lv00_list_type_create(void *elem_type);
void lv00_list_type_destroy(Lv00ListTypeRegion *t);

Lv00MapTypeRegion *lv00_map_type_create(void *key_type, void *value_type);
void lv00_map_type_destroy(Lv00MapTypeRegion *t);

Lv00FunctionTypeRegion *lv00_function_type_create(void *param, void *ret, int dependent);
void lv00_function_type_destroy(Lv00FunctionTypeRegion *t);

Lv00EffectTypeRegion *lv00_effect_type_create(Lv00EffectType *effects, int count, void *result);
void lv00_effect_type_destroy(Lv00EffectTypeRegion *t);

/* Type compatibility check */
int lv00_extended_type_compatible(void *a, void *b);

#ifdef __cplusplus
}
#endif

#endif /* LV00_EXTENDED_TYPES_H */
