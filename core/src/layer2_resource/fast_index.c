#include "lv00/lv00.h"
#include "lv00/fast_index.h"
#include <stdlib.h>

struct Lv00FastIndex {
    int capacity;
    int count;
};

Lv00FastIndex *lv00_fast_index_create(int capacity)
{
    Lv00FastIndex *idx = (Lv00FastIndex *)malloc(sizeof(Lv00FastIndex));
    if (!idx) return NULL;
    idx->capacity = capacity > 0 ? capacity : 64;
    idx->count = 0;
    return idx;
}

void lv00_fast_index_destroy(Lv00FastIndex *idx)
{
    free(idx);
}

int lv00_fast_index_insert(Lv00FastIndex *idx, int node_id, double x, double y, double w, double h)
{
    (void)node_id; (void)x; (void)y; (void)w; (void)h;
    if (!idx) return -1;
    return 0;
}

int lv00_fast_index_query(Lv00FastIndex *idx, double x, double y, int *out_ids, int max_out)
{
    (void)x; (void)y; (void)out_ids; (void)max_out;
    if (!idx) return -1;
    return 0;
}
