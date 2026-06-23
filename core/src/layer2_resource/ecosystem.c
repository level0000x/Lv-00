#include "lv00/lv00.h"
#include "lv00/ecosystem.h"

#include <stdlib.h>
#include <string.h>

#define LV00_ECOSYSTEM_MAX_MODULES 64

static int lv00_ecosystem_initialized = 0;
static int lv00_ecosystem_count = 0;
static char lv00_ecosystem_names[LV00_ECOSYSTEM_MAX_MODULES][64];

int lv00_ecosystem_init(void)
{
    if (lv00_ecosystem_initialized) return 0;
    lv00_ecosystem_initialized = 1;
    lv00_ecosystem_count = 0;
    return 0;
}

void lv00_ecosystem_shutdown(void)
{
    lv00_ecosystem_initialized = 0;
    lv00_ecosystem_count = 0;
}

int lv00_ecosystem_register_module(const char *name, int layer)
{
    (void)layer;
    if (!name || !lv00_ecosystem_initialized) return -1;
    if (lv00_ecosystem_count >= LV00_ECOSYSTEM_MAX_MODULES) return -1;
    strncpy(lv00_ecosystem_names[lv00_ecosystem_count], name, 63);
    lv00_ecosystem_names[lv00_ecosystem_count][63] = '\0';
    lv00_ecosystem_count++;
    return 0;
}

int lv00_ecosystem_module_count(void)
{
    return lv00_ecosystem_count;
}

const char *lv00_ecosystem_module_name(int idx)
{
    if (idx < 0 || idx >= lv00_ecosystem_count) return NULL;
    return lv00_ecosystem_names[idx];
}
