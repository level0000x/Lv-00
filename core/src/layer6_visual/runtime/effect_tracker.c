#include <string.h>

#include "lv/effect_system.h"
#include "lv/lv_utils.h"

lvEffectTracker *lv_effect_tracker_create(void) {
    lvEffectTracker *tracker = lv_calloc(1, sizeof(lvEffectTracker));
    if (!tracker)
        return NULL;
    tracker->entry_capacity = 64;
    tracker->entries = lv_calloc(tracker->entry_capacity, sizeof(lvEffectLogEntry));
    if (!tracker->entries) {
        lv_free((void **) &tracker);
        return NULL;
    }
    return tracker;
}

void lv_effect_tracker_destroy(lvEffectTracker *tracker) {
    if (!tracker)
        return;
    lv_free((void **) &tracker->entries);
    if (tracker->current_effect)
        lv_effect_annotation_destroy(tracker->current_effect);
    lv_free((void **) &tracker);
}

void lv_effect_tracker_reset(lvEffectTracker *tracker) {
    if (!tracker)
        return;
    tracker->entry_count = 0;
    if (tracker->current_effect) {
        lv_effect_annotation_destroy(tracker->current_effect);
        tracker->current_effect = NULL;
    }
}

void lv_effect_tracker_record(lvEffectTracker *tracker, lvEffectType effect, int block_id, const char *desc) {
    if (!tracker)
        return;
    if (tracker->entry_count >= tracker->entry_capacity) {
        int new_cap = tracker->entry_capacity * 2;
        if (new_cap <= 0)
            new_cap = 16;
        lvEffectLogEntry *new_entries =
            (lvEffectLogEntry *) lv_realloc(tracker->entries, (size_t) new_cap * sizeof(lvEffectLogEntry));
        if (!new_entries)
            return;
        tracker->entries = new_entries;
        tracker->entry_capacity = new_cap;
    }
    lvEffectLogEntry *e = &tracker->entries[tracker->entry_count++];
    e->effect = effect;
    e->block_id = block_id;
    if (desc)
        strncpy(e->description, desc, sizeof(e->description) - 1);
}

int lv_effect_tracker_has_effect(const lvEffectTracker *tracker, lvEffectType effect) {
    if (!tracker)
        return 0;
    for (int i = 0; i < tracker->entry_count; i++) {
        if (tracker->entries[i].effect == effect)
            return 1;
    }
    return 0;
}

int lv_effect_tracker_is_pure(const lvEffectTracker *tracker) {
    if (!tracker)
        return 1;
    return tracker->entry_count == 0;
}

const lvEffectAnnotation *lv_effect_tracker_current(const lvEffectTracker *tracker) {
    return tracker ? tracker->current_effect : NULL;
}

lvEffectAnnotation *lv_effect_compose(const lvEffectAnnotation *a, const lvEffectAnnotation *b) {
    if (!a && !b)
        return NULL;
    lvEffectAnnotation *result = lv_calloc(1, sizeof(lvEffectAnnotation));
    int count = 0;
    if (a)
        count += a->effect_count;
    if (b)
        count += b->effect_count;
    if (count == 0) {
        lv_free((void **) &result);
        return NULL;
    }
    result->effects = lv_calloc(count, sizeof(lvEffectType));
    if (!result->effects) {
        lv_free((void **) &result);
        return NULL;
    }
    result->effect_count = 0;
    if (a) {
        memcpy(result->effects, a->effects, a->effect_count * sizeof(lvEffectType));
        result->effect_count += a->effect_count;
    }
    if (b) {
        memcpy(result->effects + result->effect_count, b->effects, b->effect_count * sizeof(lvEffectType));
        result->effect_count += b->effect_count;
    }
    return result;
}

void lv_effect_annotation_destroy(lvEffectAnnotation *ann) {
    if (!ann)
        return;
    lv_free((void **) &ann->effects);
    lv_free((void **) &ann);
}

int lv_effect_check_geometry_pure(const lvEffectTracker *tracker) {
    if (!tracker)
        return 1;
    for (int i = 0; i < tracker->entry_count; i++) {
        lvEffectType e = tracker->entries[i].effect;
        if (e != lv_EFFECT_PURE)
            return 0;
    }
    return 1;
}
