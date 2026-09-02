#ifndef lv_FAST_INDEX_H
#define lv_FAST_INDEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include "lv/lv.h"

typedef struct lvFastIndex lvFastIndex;

lvFastIndex *lv_fast_index_create(int capacity);
lv_PUBLIC_API void lv_fast_index_destroy(lvFastIndex *idx);
lv_PUBLIC_API int lv_fast_index_insert(lvFastIndex *idx, int node_id, double x, double y, double w, double h);
lv_PUBLIC_API int lv_fast_index_query(lvFastIndex *idx, double x, double y, int *out_ids, int max_out);

#ifdef __cplusplus
}
#endif

#endif /* lv_FAST_INDEX_H */
