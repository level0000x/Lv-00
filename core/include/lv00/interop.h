#ifndef LV00_INTEROP_H
#define LV00_INTEROP_H

#include "lv00/application.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LV00_LAYER_INTEROP 10

/* External system identifiers */
typedef enum {
    LV00_EXT_LEAN4,
    LV00_EXT_COQ,
    LV00_EXT_ISABELLE,
    LV00_EXT_HOL4,
    LV00_EXT_AGDA,
    LV00_EXT_MATLAB,
    LV00_EXT_MAPLE,
    LV00_EXT_MATHEMATICA,
    LV00_EXT_JSON,
    LV00_EXT_LVZ,
    LV00_EXT_COUNT
} Lv00ExternalSystem;

/* Interop direction */
typedef enum {
    LV00_INTEROP_EXPORT,    /* Lv-00 → External */
    LV00_INTEROP_IMPORT     /* External → Lv-00 */
} Lv00InteropDirection;

/* Interop result */
typedef struct Lv00InteropResult {
    int success;
    Lv00ExternalSystem target;
    Lv00InteropDirection direction;
    char output[4096];
    char error_msg[512];
} Lv00InteropResult;

/* Plugin interface */
typedef struct Lv00Plugin {
    char name[128];
    char version[64];
    Lv00ExternalSystem system;
    int (*export_proof)(void *proof, char *output, int output_size);
    int (*import_proof)(const char *input, void **proof);
    int (*validate)(const char *input);
    void *userdata;
} Lv00Plugin;

/* Interop manager */
typedef struct Lv00InteropManager {
    Lv00Plugin **plugins;
    int plugin_count;
    int plugin_capacity;
} Lv00InteropManager;

/* Lifecycle */
Lv00InteropManager *lv00_interop_create(void);
void lv00_interop_destroy(Lv00InteropManager *mgr);

/* Plugin management */
int lv00_interop_register_plugin(Lv00InteropManager *mgr, const Lv00Plugin *plugin);
int lv00_interop_unregister_plugin(Lv00InteropManager *mgr, const char *name);
const Lv00Plugin *lv00_interop_find_plugin(Lv00InteropManager *mgr, Lv00ExternalSystem system);

/* Export/Import */
Lv00InteropResult lv00_interop_export(Lv00InteropManager *mgr, Lv00ExternalSystem target,
                                        void *proof);
Lv00InteropResult lv00_interop_import(Lv00InteropManager *mgr, Lv00ExternalSystem source,
                                        const char *input);

/* System name query */
const char *lv00_interop_system_name(Lv00ExternalSystem system);

#ifdef __cplusplus
}
#endif

#endif /* LV00_INTEROP_H */
