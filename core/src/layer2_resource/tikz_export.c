#include "lv00/lv00.h"
#include "lv00/tikz_export.h"
#include <string.h>

int lv00_tikz_export(void *graph, char *out, size_t buf_size)
{
    (void)graph;
    if (!out || buf_size == 0) return -1;
    out[0] = '\0';
    return 0;
}

int lv00_tikz_export_file(void *graph, const char *filename)
{
    (void)graph; (void)filename;
    return 0;
}
