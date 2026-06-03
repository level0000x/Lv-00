#include "lv00/interop.h"
#include <stdlib.h>
#include <string.h>

const char *lv00_interop_system_name(Lv00ExternalSystem system) {
    switch (system) {
        case LV00_EXT_LEAN4:      return "Lean 4";
        case LV00_EXT_COQ:        return "Coq";
        case LV00_EXT_ISABELLE:   return "Isabelle/HOL";
        case LV00_EXT_HOL4:       return "HOL4";
        case LV00_EXT_AGDA:       return "Agda";
        case LV00_EXT_MATLAB:     return "MATLAB";
        case LV00_EXT_MAPLE:      return "Maple";
        case LV00_EXT_MATHEMATICA: return "Mathematica";
        case LV00_EXT_JSON:       return "JSON";
        case LV00_EXT_LVZ:        return "LVZ";
        default:                  return "Unknown";
    }
}

Lv00InteropManager *lv00_interop_create(void) {
    Lv00InteropManager *mgr = calloc(1, sizeof(Lv00InteropManager));
    if (!mgr) return NULL;
    mgr->plugin_capacity = 16;
    mgr->plugins = calloc(mgr->plugin_capacity, sizeof(Lv00Plugin *));
    return mgr;
}

void lv00_interop_destroy(Lv00InteropManager *mgr) {
    if (!mgr) return;
    for (int i = 0; i < mgr->plugin_count; i++) {
        free(mgr->plugins[i]);
    }
    free(mgr->plugins);
    free(mgr);
}

int lv00_interop_register_plugin(Lv00InteropManager *mgr, const Lv00Plugin *plugin) {
    if (!mgr || !plugin) return -1;
    if (mgr->plugin_count >= mgr->plugin_capacity) return -1;
    Lv00Plugin *copy = calloc(1, sizeof(Lv00Plugin));
    if (!copy) return -1;
    *copy = *plugin;
    mgr->plugins[mgr->plugin_count++] = copy;
    return 0;
}

int lv00_interop_unregister_plugin(Lv00InteropManager *mgr, const char *name) {
    if (!mgr || !name) return -1;
    for (int i = 0; i < mgr->plugin_count; i++) {
        if (strcmp(mgr->plugins[i]->name, name) == 0) {
            free(mgr->plugins[i]);
            mgr->plugins[i] = mgr->plugins[--mgr->plugin_count];
            return 0;
        }
    }
    return -1;
}

const Lv00Plugin *lv00_interop_find_plugin(Lv00InteropManager *mgr, Lv00ExternalSystem system) {
    if (!mgr) return NULL;
    for (int i = 0; i < mgr->plugin_count; i++) {
        if (mgr->plugins[i]->system == system) return mgr->plugins[i];
    }
    return NULL;
}

Lv00InteropResult lv00_interop_export(Lv00InteropManager *mgr, Lv00ExternalSystem target, void *proof) {
    Lv00InteropResult result = {0};
    result.target = target;
    result.direction = LV00_INTEROP_EXPORT;
    if (!mgr || !proof) {
        result.success = 0;
        strncpy(result.error_msg, "Invalid arguments", sizeof(result.error_msg) - 1);
        return result;
    }
    const Lv00Plugin *plugin = lv00_interop_find_plugin(mgr, target);
    if (!plugin || !plugin->export_proof) {
        result.success = 0;
        snprintf(result.error_msg, sizeof(result.error_msg), "No plugin for %s", lv00_interop_system_name(target));
        return result;
    }
    int rc = plugin->export_proof(proof, result.output, sizeof(result.output));
    result.success = (rc == 0);
    return result;
}

Lv00InteropResult lv00_interop_import(Lv00InteropManager *mgr, Lv00ExternalSystem source, const char *input) {
    Lv00InteropResult result = {0};
    result.target = source;
    result.direction = LV00_INTEROP_IMPORT;
    if (!mgr || !input) {
        result.success = 0;
        strncpy(result.error_msg, "Invalid arguments", sizeof(result.error_msg) - 1);
        return result;
    }
    const Lv00Plugin *plugin = lv00_interop_find_plugin(mgr, source);
    if (!plugin || !plugin->import_proof) {
        result.success = 0;
        snprintf(result.error_msg, sizeof(result.error_msg), "No plugin for %s", lv00_interop_system_name(source));
        return result;
    }
    int rc = plugin->import_proof(input, &result.output);
    result.success = (rc == 0);
    return result;
}
