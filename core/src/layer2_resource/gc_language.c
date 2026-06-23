#include "lv00/lv00.h"
#include "lv00/gc_language.h"

int lv00_gc_parse(const char *source, void *engine)
{
    (void)source; (void)engine;
    return 0;
}

const char *lv00_gc_error(void)
{
    return NULL;
}

int lv00_gc_command_count(void)
{
    return 0;
}
