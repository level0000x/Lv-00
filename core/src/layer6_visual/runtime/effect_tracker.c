#include "lv00/effect_system.h"
#include "lv00/lv00_utils.h"
#include <string.h>

Lv00EffectTracker *lv00_effect_tracker_create(void) {
    Lv00EffectTracker *tracker = lv00_calloc(1, sizeof(Lv00EffectTracker));
    if (!tracker) return NULL;
    tracker->entry_capacity = 64;
    tracker->entries = lv00_calloc(tracker->entry_capacity, sizeof(Lv00EffectLogEntry));
    if (!tracker->entries) {
        lv00_free((void **)&tracker);
        return NULL;
    }
    return tracker;
}

void lv00_effect_tracker_destroy(Lv00EffectTracker *tracker) {
    if (!tracker) return;
    lv00_free((void **)&tracker->entries);
    if (tracker->current_effect) lv00_effect_annotation_destroy(tracker->current_effect);
    lv00_free((void **)&tracker);
}

void lv00_effect_tracker_reset(Lv00EffectTracker *tracker) {
    if (!tracker) return;
    tracker->entry_count = 0;
    if (tracker->current_effect) {
        lv00_effect_annotation_destroy(tracker->current_effect);
        tracker->current_effect = NULL;
    }
}

void lv00_effect_tracker_record(Lv00EffectTracker *tracker, Lv00EffectType effect,
                                 int block_id, const char *desc) {
    if (!tracker) return;
    if (tracker->entry_count >= tracker->entry_capacity) {
        int new_cap = tracker->entry_capacity * 2;
        if (new_cap <= 0) new_cap = 16;
        Lv00EffectLogEntry *new_entries = (Lv00EffectLogEntry *)lv00_realloc(tracker->entries, (size_t)new_cap * sizeof(Lv00EffectLogEntry));
        if (!new_entries) return;
        tracker->entries = new_entries;
        tracker->entry_capacity = new_cap;
    }
    Lv00EffectLogEntry *e = &tracker->entries[tracker->entry_count++];
    e->effect = effect;
    e->block_id = block_id;
    if (desc) strncpy(e->description, desc, sizeof(e->description) - 1);
}

int lv00_effect_tracker_has_effect(const Lv00EffectTracker *tracker, Lv00EffectType effect) {
    if (!tracker) return 0;
    for (int i = 0; i < tracker->entry_count; i++) {
        if (tracker->entries[i].effect == effect) return 1;
    }
    return 0;
}

int lv00_effect_tracker_is_pure(const Lv00EffectTracker *tracker) {
    if (!tracker) return 1;
    return tracker->entry_count == 0;
}

const Lv00EffectAnnotation *lv00_effect_tracker_current(const Lv00EffectTracker *tracker) {
    return tracker ? tracker->current_effect : NULL;
}

Lv00EffectAnnotation *lv00_effect_compose(const Lv00EffectAnnotation *a,
                                           const Lv00EffectAnnotation *b) {
    if (!a && !b) return NULL;
    Lv00EffectAnnotation *result = lv00_calloc(1, sizeof(Lv00EffectAnnotation));
    int count = 0;
    if (a) count += a->effect_count;
    if (b) count += b->effect_count;
    if (count == 0) { lv00_free((void **)&result); return NULL; }
    result->effects = lv00_calloc(count, sizeof(Lv00EffectType));
    if (!result->effects) {
        lv00_free((void **)&result);
        return NULL;
    }
    result->effect_count = 0;
    if (a) {
        memcpy(result->effects, a->effects, a->effect_count * sizeof(Lv00EffectType));
        result->effect_count += a->effect_count;
    }
    if (b) {
        memcpy(result->effects + result->effect_count, b->effects, b->effect_count * sizeof(Lv00EffectType));
        result->effect_count += b->effect_count;
    }
    return result;
}

void lv00_effect_annotation_destroy(Lv00EffectAnnotation *ann) {
    if (!ann) return;
    lv00_free((void **)&ann->effects);
    lv00_free((void **)&ann);
}

int lv00_effect_check_geometry_pure(const Lv00EffectTracker *tracker) {
    if (!tracker) return 1;
    for (int i = 0; i < tracker->entry_count; i++) {
        Lv00EffectType e = tracker->entries[i].effect;
        if (e != LV00_EFFECT_PURE) return 0;
    }
    return 1;
}
