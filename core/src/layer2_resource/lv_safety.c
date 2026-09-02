/**
 * @file lv_safety.c
 * @brief 蓝图泄漏检测实现（TEN_LAYER_OPTIMIZED_PLAN §16.3.3 落地）
 *
 * 基于调试分配器的全局追踪链表（s_utils_state.tracked_allocs，AllocHeader
 * 含 ptr/size/file/line）收集活跃分配；活跃链表为空即无泄漏。
 */

#include "lv/lv_safety.h"

#include <stdio.h>
#include <string.h>

#include "lv_utils_internal.h" /* s_utils_state, AllocHeader */

/** 快照块容量与头中 records 数组一致 */
#define LV_LEAK_RECORD_CAP 256

lvLeakSnapshot lv_leak_detector_snapshot(void) {
    lvLeakSnapshot snap;
    memset(&snap, 0, sizeof(snap));
    const AllocHeader *hdr = s_utils_state.tracked_allocs;
    while (hdr != NULL) {
        snap.active_bytes += hdr->size;
        snap.active_count++;
        if (snap.active_count <= LV_LEAK_RECORD_CAP) {
            size_t idx = snap.active_count - 1;
            snap.records[idx].ptr = (void *) hdr->data;
            snap.records[idx].size = hdr->size;
            snap.records[idx].file = hdr->file;
            snap.records[idx].line = hdr->line;
        }
        hdr = hdr->track_next;
    }
    return snap;
}

void lv_leak_detector_report(const lvLeakSnapshot *snapshot) {
    lvLeakSnapshot snap;
    if (snapshot == NULL) {
        snap = lv_leak_detector_snapshot();
        snapshot = &snap;
    }
    fprintf(stderr, "[leak-detector] active=%llu bytes (%llu blocks)%s\n",
            (unsigned long long) snapshot->active_bytes, (unsigned long long) snapshot->active_count,
            snapshot->active_bytes > 0 ? "  [LEAK]" : "  [clean]");
    if (snapshot->active_count > 0) {
        size_t shown = snapshot->active_count < LV_LEAK_RECORD_CAP ? (size_t) snapshot->active_count
                                                                   : LV_LEAK_RECORD_CAP;
        for (size_t i = 0; i < shown; i++) {
            const char *file = snapshot->records[i].file ? snapshot->records[i].file : "<未记录>";
            fprintf(stderr, "  %p  %zu bytes  %s:%d\n", snapshot->records[i].ptr, snapshot->records[i].size, file,
                    snapshot->records[i].line);
        }
    }
}

int lv_leak_detector_assert_clean(void) {
    lvLeakSnapshot snap = lv_leak_detector_snapshot();
    return (int) (snap.active_bytes > 0 ? snap.active_bytes : 0);
}
