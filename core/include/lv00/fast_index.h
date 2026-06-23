#ifndef LV00_FAST_INDEX_H
#define LV00_FAST_INDEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv00/lv00.h"

typedef struct Lv00FastIndex Lv00FastIndex;

Lv00FastIndex *lv00_fast_index_create(int capacity);
void lv00_fast_index_destroy(Lv00FastIndex *idx);
int lv00_fast_index_insert(Lv00FastIndex *idx, int node_id, double x, double y, double w, double h);
int lv00_fast_index_query(Lv00FastIndex *idx, double x, double y, int *out_ids, int max_out);

#ifdef __cplusplus
}
#endif

#endif /* LV00_FAST_INDEX_H */
