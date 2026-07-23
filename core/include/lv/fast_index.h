#ifndef lv_FAST_INDEX_H
#define lv_FAST_INDEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/lv.h"

typedef struct lvFastIndex lvFastIndex;

lvFastIndex *lv_fast_index_create(int capacity);
void lv_fast_index_destroy(lvFastIndex *idx);
int lv_fast_index_insert(lvFastIndex *idx, int node_id, double x, double y, double w, double h);
int lv_fast_index_query(lvFastIndex *idx, double x, double y, int *out_ids, int max_out);

#ifdef __cplusplus
}
#endif

#endif /* lv_FAST_INDEX_H */
