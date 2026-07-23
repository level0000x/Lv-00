#include "lv/lv.h"
#include "lv/fast_index.h"
#include <stdlib.h>

struct lvFastIndex {
    int capacity;
    int count;
};

lvFastIndex *lv_fast_index_create(int capacity)
{
    lvFastIndex *idx = (lvFastIndex *)malloc(sizeof(lvFastIndex));
    if (!idx) return NULL;
    idx->capacity = capacity > 0 ? capacity : 64;
    idx->count = 0;
    return idx;
}

void lv_fast_index_destroy(lvFastIndex *idx)
{
    free(idx);
}

int lv_fast_index_insert(lvFastIndex *idx, int node_id, double x, double y, double w, double h)
{
    (void)node_id; (void)x; (void)y; (void)w; (void)h;
    if (!idx) return -1;
    return 0;
}

int lv_fast_index_query(lvFastIndex *idx, double x, double y, int *out_ids, int max_out)
{
    (void)x; (void)y; (void)out_ids; (void)max_out;
    if (!idx) return -1;
    return 0;
}
