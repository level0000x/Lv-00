#ifndef LV00_EFFECT_SYSTEM_H
#define LV00_EFFECT_SYSTEM_H

#include "lv00/io_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Effect annotation */
typedef struct Lv00EffectAnnotation {
    Lv00EffectType *effects;
    int effect_count;
} Lv00EffectAnnotation;

/* Effect log entry */
typedef struct Lv00EffectLogEntry {
    Lv00EffectType effect;
    int block_id;
    char description[256];
    double timestamp;
} Lv00EffectLogEntry;

/* Effect tracker */
typedef struct Lv00EffectTracker {
    Lv00EffectLogEntry *entries;
    int entry_count;
    int entry_capacity;

    /* Current combined effect */
    Lv00EffectAnnotation *current_effect;
} Lv00EffectTracker;

/* Lifecycle */
Lv00EffectTracker *lv00_effect_tracker_create(void);
void lv00_effect_tracker_destroy(Lv00EffectTracker *tracker);
void lv00_effect_tracker_reset(Lv00EffectTracker *tracker);

/* Recording */
void lv00_effect_tracker_record(Lv00EffectTracker *tracker, Lv00EffectType effect,
                                 int block_id, const char *desc);

/* Query */
int lv00_effect_tracker_has_effect(const Lv00EffectTracker *tracker, Lv00EffectType effect);
int lv00_effect_tracker_is_pure(const Lv00EffectTracker *tracker);
const Lv00EffectAnnotation *lv00_effect_tracker_current(const Lv00EffectTracker *tracker);

/* Effect composition */
Lv00EffectAnnotation *lv00_effect_compose(const Lv00EffectAnnotation *a,
                                           const Lv00EffectAnnotation *b);
void lv00_effect_annotation_destroy(Lv00EffectAnnotation *ann);

/* Effect checking rules */
int lv00_effect_check_geometry_pure(const Lv00EffectTracker *tracker);

#ifdef __cplusplus
}
#endif

#endif /* LV00_EFFECT_SYSTEM_H */
