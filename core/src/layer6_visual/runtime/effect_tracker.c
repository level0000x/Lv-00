#include <string.h>

#include "lv/effect_system.h"
#include "lv/lv_utils.h"
#include "lv/lv_internal.h"

lvEffectTracker *lv_effect_tracker_create(void) {
    lvEffectTracker *tracker = lv_calloc(1, sizeof(lvEffectTracker));
    if (!tracker)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate effect tracker");
    lv_darray_init(&tracker->entries, sizeof(lvEffectLogEntry));
    if (!lv_darray_reserve(&tracker->entries, 64)) {
        lv_free((void **) &tracker);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to reserve effect tracker darray");
    }
    return tracker;
}

void lv_effect_tracker_destroy(lvEffectTracker *tracker) {
    if (!tracker)
        return;
    lv_darray_free(&tracker->entries);
    if (tracker->current_effect)
        lv_effect_annotation_destroy(tracker->current_effect);
    lv_free((void **) &tracker);
}

void lv_effect_tracker_reset(lvEffectTracker *tracker) {
    if (!tracker)
        return;
    lv_darray_clear(&tracker->entries);
    if (tracker->current_effect) {
        lv_effect_annotation_destroy(tracker->current_effect);
        tracker->current_effect = NULL;
    }
}

void lv_effect_tracker_record(lvEffectTracker *tracker, lvEffectType effect, int block_id, const char *desc) {
    if (!tracker)
        return;
    lvEffectLogEntry e;
    memset(&e, 0, sizeof(e));
    e.effect = effect;
    e.block_id = block_id;
    if (desc)
        lv_strlcpy(e.description, desc, sizeof(e.description));
    lv_darray_push(&tracker->entries, &e);
}

int lv_effect_tracker_has_effect(const lvEffectTracker *tracker, lvEffectType effect) {
    if (!tracker)
        return 0;
    for (int i = 0; i < tracker->entries.count; i++) {
        lvEffectLogEntry *e = (lvEffectLogEntry *) lv_darray_get(&tracker->entries, i);
        if (e && e->effect == effect)
            return 1;
    }
    return 0;
}

int lv_effect_tracker_is_pure(const lvEffectTracker *tracker) {
    if (!tracker)
        return 1;
    return tracker->entries.count == 0;
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
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate composed effects");
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
    for (int i = 0; i < tracker->entries.count; i++) {
        lvEffectLogEntry *entry = (lvEffectLogEntry *) lv_darray_get(&tracker->entries, i);
        if (entry && entry->effect != lv_EFFECT_PURE)
            return 0;
    }
    return 1;
}
