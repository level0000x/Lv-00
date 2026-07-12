/**
 * @file module_delta.c
 * @brief Delta 差分系统
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lv00/module.h"
#include "lv00/module_internal.h"
#include "debug.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

void module_set_autosave_config(Module *mod, const AutoSaveConfig *config) {
    if (!mod || !config) return;

    AutoSaveConfig *stored = get_or_create_autosave_config(mod->name);
    if (!stored) return;

    stored->enabled = config->enabled;
    stored->interval_seconds = config->interval_seconds;
    stored->max_backups = config->max_backups;

    if (stored->backup_directory) {
        lv00_free((void**)&stored->backup_directory);
        stored->backup_directory = NULL;
    }
    if (config->backup_directory) {
        stored->backup_directory = lv00_strdup_safe(config->backup_directory);
    }
}

/* 生成备份文件路径 */
static void make_backup_filepath(char *buf, size_t buf_size,
                                  const char *backup_dir,
                                  const char *module_name,
                                  int index) {
    if (backup_dir && backup_dir[0]) {
        const char *sep = "";
        size_t dir_len = strlen(backup_dir);
        if (dir_len > 0 && backup_dir[dir_len - 1] != '/' && backup_dir[dir_len - 1] != '\\') {
            sep = "/";
        }
        snprintf(buf, buf_size, "%s%s%s_autosave_%d.lvz",
                 backup_dir, sep, module_name, index);
    } else {
        snprintf(buf, buf_size, "%s_autosave_%d.lvz",
                 module_name, index);
    }
}

/* 生成备份文件路径（二进制格式） */
static void make_backup_binpath(char *buf, size_t buf_size,
                                 const char *backup_dir,
                                 const char *module_name,
                                 int index) {
    if (backup_dir && backup_dir[0]) {
        const char *sep = "";
        size_t dir_len = strlen(backup_dir);
        if (dir_len > 0 && backup_dir[dir_len - 1] != '/' && backup_dir[dir_len - 1] != '\\') {
            sep = "/";
        }
        snprintf(buf, buf_size, "%s%s%s_autosave_%d.bin",
                 backup_dir, sep, module_name, index);
    } else {
        snprintf(buf, buf_size, "%s_autosave_%d.bin",
                 module_name, index);
    }
}

ModuleSaveStatus module_autosave(const Module *mod) {
    if (!mod) {
        lv00_set_error(LV00_ERROR_INVALID_PARAM, "module_autosave: 无效参数");
        return MODULE_SAVE_WRITE_ERROR;
    }

    AutoSaveConfig *config = find_autosave_config(mod->name);
    if (!config || !config->enabled) {
        /* 自动保存未启用，静默成功 */
        return MODULE_SAVE_OK;
    }

    /* 轮转备份：删除最旧的备份 */
    if (config->max_backups > 0) {
        char oldest_path[1024];
        make_backup_filepath(oldest_path, sizeof(oldest_path),
                             config->backup_directory, mod->name,
                             config->max_backups - 1);
        remove(oldest_path);

        char oldest_binpath[1024];
        make_backup_binpath(oldest_binpath, sizeof(oldest_binpath),
                            config->backup_directory, mod->name,
                            config->max_backups - 1);
        remove(oldest_binpath);

        /* 将备份文件向后移动 */
        for (int i = config->max_backups - 2; i >= 0; i--) {
            char old_path[1024], new_path[1024];
            make_backup_filepath(old_path, sizeof(old_path),
                                 config->backup_directory, mod->name, i);
            make_backup_filepath(new_path, sizeof(new_path),
                                 config->backup_directory, mod->name, i + 1);
            rename(old_path, new_path);

            char old_binpath[1024], new_binpath[1024];
            make_backup_binpath(old_binpath, sizeof(old_binpath),
                                config->backup_directory, mod->name, i);
            make_backup_binpath(new_binpath, sizeof(new_binpath),
                                config->backup_directory, mod->name, i + 1);
            rename(old_binpath, new_binpath);
        }
    }

    /* 保存文本格式备份 */
    char backup_path[1024];
    make_backup_filepath(backup_path, sizeof(backup_path),
                         config->backup_directory, mod->name, 0);

    ModuleSaveStatus status = module_save(mod, backup_path);
    if (status != MODULE_SAVE_OK) {
        lv00_set_error(LV00_ERROR_IO, "module_autosave: 文本备份保存失败");
        return status;
    }

    /* 同时保存二进制格式备份 */
    uint8_t *bin_data = NULL;
    size_t bin_size = 0;
    status = module_save_to_binary(mod, &bin_data, &bin_size);
    if (status == MODULE_SAVE_OK && bin_data) {
        char bin_path[1024];
        make_backup_binpath(bin_path, sizeof(bin_path),
                            config->backup_directory, mod->name, 0);

        FILE *f = fopen(bin_path, "wb");
        if (f) {
            fwrite(bin_data, 1, bin_size, f);
            fclose(f);
        }
        lv00_free((void**)&bin_data);
    }

    return MODULE_SAVE_OK;
}

ModuleLoadStatus module_recover_from_backup(const char *module_name, Module **out_module) {
    if (!module_name || !out_module) {
        lv00_set_error(LV00_ERROR_INVALID_PARAM, "module_recover_from_backup: 无效参数");
        return MODULE_LOAD_PARSE_ERROR;
    }

    AutoSaveConfig *config = find_autosave_config(module_name);

    /* 尝试从备份文件恢复（从最新到最旧） */
    int max_attempts = config ? config->max_backups : 5;
    if (max_attempts <= 0) max_attempts = 5;

    for (int i = 0; i < max_attempts; i++) {
        /* 优先尝试二进制格式 */
        char bin_path[1024];
        make_backup_binpath(bin_path, sizeof(bin_path),
                            config ? config->backup_directory : NULL,
                            module_name, i);

        FILE *f = fopen(bin_path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long fsize = ftell(f);
            fseek(f, 0, SEEK_SET);

            if (fsize > 0) {
                uint8_t *data = (uint8_t *)lv00_malloc((size_t)fsize);
                if (data) {
                    size_t read_len = fread(data, 1, (size_t)fsize, f);
                    fclose(f);

                    ModuleLoadStatus status = module_load_from_binary(data, read_len, out_module);
                    lv00_free((void**)&data);
                    if (status == MODULE_LOAD_OK) {
                        return MODULE_LOAD_OK;
                    }
                } else {
                    fclose(f);
                }
            } else {
                fclose(f);
            }
        }

        /* 尝试文本格式 */
        char txt_path[1024];
        make_backup_filepath(txt_path, sizeof(txt_path),
                             config ? config->backup_directory : NULL,
                             module_name, i);

        f = fopen(txt_path, "r");
        if (f) {
            fseek(f, 0, SEEK_END);
            long fsize = ftell(f);
            fseek(f, 0, SEEK_SET);

            if (fsize > 0) {
                char *data = (char *)lv00_malloc((size_t)fsize + 1);
                if (data) {
                    size_t read_len = fread(data, 1, (size_t)fsize, f);
                    data[read_len] = '\0';
                    fclose(f);

                    /* 尝试作为 LVZ 格式加载 */
                    Module *mod = module_create(module_name, "0.0.0");
                    if (mod) {
                        LvzParser parser;
                        lvz_parser_init(&parser, data);
                        bool parse_ok = lvz_parse(&parser, mod);
                        lvz_parser_cleanup(&parser);

                        if (parse_ok) {
                            lv00_free((void**)&data);
                            *out_module = mod;
                            return MODULE_LOAD_OK;
                        }
                        module_destroy(mod);
                    }

                    /* 尝试作为 JSON 格式加载 */
                    ModuleLoadStatus json_status = module_deserialize_from_json(data, out_module);
                    lv00_free((void**)&data);
                    if (json_status == MODULE_LOAD_OK) {
                        return MODULE_LOAD_OK;
                    }
                } else {
                    fclose(f);
                }
            } else {
                fclose(f);
            }
        }
    }

    lv00_set_error(LV00_ERROR_NOT_FOUND, "module_recover_from_backup: 未找到可恢复的备份文件");
    return MODULE_LOAD_FILE_NOT_FOUND;
}

/* ================================================================== */
/*  增量存储                                                           */
/* ================================================================== */

/*
 * 增量快照记录自 base_hash 以来的模块变化。
 * 实现策略：将当前模块序列化为 JSON，并与基线进行比较，
 * 生成仅包含差异的 delta 数据。
 *
 * delta_data 格式 (JSON):
 * {
 *     "base_hash": "0123456789abcdef",
 *     "changes": {
 *         "name": {"old": "old_name", "new": "new_name"},
 *         "version": {"old": "1.0.0", "new": "2.0.0"},
 *         "dependencies_added": [...],
 *         "dependencies_removed": [...],
 *         "exports_function_blocks_added": [...],
 *         "exports_function_blocks_removed": [...],
 *         "exports_type_regions_added": [...],
 *         "exports_type_regions_removed": [...]
 *     }
 * }
 */

/* 将十六进制哈希字符串转换为 uint64_t */
static uint64_t hash_string_to_u64(const char *hex_str) {
    if (!hex_str) return 0;
    uint64_t val = 0;
    for (int i = 0; hex_str[i] && i < 16; i++) {
        char c = hex_str[i];
        val <<= 4;
        if (c >= '0' && c <= '9') val |= (uint64_t)(c - '0');
        else if (c >= 'a' && c <= 'f') val |= (uint64_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val |= (uint64_t)(c - 'A' + 10);
    }
    return val;
}

/* 将 uint64_t 转换为十六进制字符串 */
static char *u64_to_hash_string(uint64_t val) {
    char *result = (char *)lv00_malloc(17);
    if (result) {
        snprintf(result, 17, "%016llx", (unsigned long long)val);
    }
    return result;
}

/*
 * 增量快照的基线存储（静态，按模块名索引）。
 * 存储上次快照时的模块状态摘要，用于计算差异。
 */
#define MAX_DELTA_BASELINES 64

typedef struct {
    char *module_name;
    uint64_t base_hash;
    char *name;
    char *version;
    /* 依赖快照 */
    char **dep_names;
    char **dep_versions;
    int dep_count;
    /* 导出快照 */
    int *func_block_ids;
    int func_count;
    int *type_region_ids;
    int type_count;
    /* 图快照 */
    int graph_node_count;
    int graph_constraint_count;
    int *graph_node_ids;
    int *graph_constraint_ids;
    uint64_t *graph_node_coord_hashes;  /* 每个节点的坐标哈希，用于检测修改 */
} DeltaBaseline;

static DeltaBaseline g_delta_baselines[MAX_DELTA_BASELINES];
static int g_delta_baseline_count = 0;

static DeltaBaseline *find_delta_baseline(const char *module_name) {
    for (int i = 0; i < g_delta_baseline_count; i++) {
        if (strcmp(g_delta_baselines[i].module_name, module_name) == 0) {
            return &g_delta_baselines[i];
        }
    }
    return NULL;
}

static void free_delta_baseline(DeltaBaseline *bl) {
    if (!bl) return;
    lv00_free((void**)&bl->module_name);
    lv00_free((void**)&bl->name);
    lv00_free((void**)&bl->version);
    for (int i = 0; i < bl->dep_count; i++) {
        lv00_free((void**)&bl->dep_names[i]);
        lv00_free((void**)&bl->dep_versions[i]);
    }
    lv00_free((void**)&bl->dep_names);
    lv00_free((void**)&bl->dep_versions);
    lv00_free((void**)&bl->func_block_ids);
    lv00_free((void**)&bl->type_region_ids);
    lv00_free((void**)&bl->graph_node_ids);
    lv00_free((void**)&bl->graph_constraint_ids);
    lv00_free((void**)&bl->graph_node_coord_hashes);
    memset(bl, 0, sizeof(DeltaBaseline));
}

static void store_baseline(const Module *mod, uint64_t hash) {
    if (!mod) return;

    /* 查找或创建基线条目 */
    DeltaBaseline *bl = find_delta_baseline(mod->name);
    if (!bl) {
        if (g_delta_baseline_count >= MAX_DELTA_BASELINES) {
            /* 回收最旧的条目 */
            free_delta_baseline(&g_delta_baselines[0]);
            /* 移动其他条目 */
            memmove(&g_delta_baselines[0], &g_delta_baselines[1],
                    (g_delta_baseline_count - 1) * sizeof(DeltaBaseline));
            g_delta_baseline_count--;
        }
        bl = &g_delta_baselines[g_delta_baseline_count++];
    } else {
        free_delta_baseline(bl);
    }

    bl->module_name = lv00_strdup_safe(mod->name);
    bl->base_hash = hash;
    bl->name = lv00_strdup_safe(mod->name);
    bl->version = lv00_strdup_safe(mod->version);

    /* 复制依赖 */
    bl->dep_count = mod->dependency_count;
    if (bl->dep_count > 0) {
        bl->dep_names = (char **)lv00_malloc(sizeof(char *) * bl->dep_count);
        bl->dep_versions = (char **)lv00_malloc(sizeof(char *) * bl->dep_count);
        for (int i = 0; i < bl->dep_count; i++) {
            bl->dep_names[i] = lv00_strdup_safe(mod->dependencies[i].name);
            bl->dep_versions[i] = lv00_strdup_safe(mod->dependencies[i].version_constraint);
        }
    }

    /* 复制导出 */
    bl->func_count = mod->exports ? mod->exports->function_count : 0;
    if (bl->func_count > 0) {
        bl->func_block_ids = (int *)lv00_malloc(sizeof(int) * bl->func_count);
        memcpy(bl->func_block_ids, mod->exports->function_block_ids, sizeof(int) * bl->func_count);
    }

    bl->type_count = mod->exports ? mod->exports->type_count : 0;
    if (bl->type_count > 0) {
        bl->type_region_ids = (int *)lv00_malloc(sizeof(int) * bl->type_count);
        memcpy(bl->type_region_ids, mod->exports->type_region_ids, sizeof(int) * bl->type_count);
    }

    /* 复制图快照 */
    if (mod->graph) {
        bl->graph_node_count = mod->graph->node_count;
        bl->graph_constraint_count = mod->graph->constraint_count;

        if (bl->graph_node_count > 0) {
            bl->graph_node_ids = (int *)lv00_malloc(sizeof(int) * bl->graph_node_count);
            bl->graph_node_coord_hashes = (uint64_t *)lv00_malloc(sizeof(uint64_t) * bl->graph_node_count);
            for (int i = 0; i < bl->graph_node_count; i++) {
                GeomNode *n = mod->graph->nodes[i];
                bl->graph_node_ids[i] = n ? n->id : -1;
                /* 计算节点坐标哈希：组合所有坐标的哈希 */
                uint64_t ch = 0;
                if (n && n->symbolic_coords) {
                    for (int c = 0; c < n->coord_count; c++) {
                        if (n->symbolic_coords[c]) {
                            ch ^= symbolic_coord_hash(n->symbolic_coords[c]) + 0x9e3779b9ULL + (ch << 6) + (ch >> 2);
                        }
                    }
                }
                /* 将节点类型和 id 混入哈希 */
                if (n) {
                    ch ^= (uint64_t)n->type * 0x9e3779b9ULL + (ch << 6) + (ch >> 2);
                }
                bl->graph_node_coord_hashes[i] = ch;
            }
        }

        if (bl->graph_constraint_count > 0) {
            bl->graph_constraint_ids = (int *)lv00_malloc(sizeof(int) * bl->graph_constraint_count);
            for (int i = 0; i < bl->graph_constraint_count; i++) {
                Constraint *c = mod->graph->constraints[i];
                bl->graph_constraint_ids[i] = c ? c->id : -1;
            }
        }
    }
}

ModuleDelta *module_compute_delta(const Module *mod, uint64_t base_hash) {
    if (!mod) return NULL;

    /* 获取基线 */
    DeltaBaseline *bl = find_delta_baseline(mod->name);

    /* 如果没有基线或哈希不匹配，存储当前状态并返回完整快照 */
    if (!bl || bl->base_hash != base_hash) {
        store_baseline(mod, base_hash);

        /* 返回完整快照作为 delta */
        char *json = module_serialize_to_json(mod);
        if (!json) return NULL;

        ModuleDelta *delta = (ModuleDelta *)lv00_malloc(sizeof(ModuleDelta));
        if (!delta) { lv00_free((void**)&json); return NULL; }

        delta->base_version_hash = base_hash;
        delta->delta_data = json;
        delta->delta_size = strlen(json);
        return delta;
    }

    /* 计算差异 */
    JsonWriter w;
    if (!json_writer_init(&w, 2048)) return NULL;

    json_writer_putc(&w, '{');

    /* base_hash */
    json_writer_puts(&w, "\"base_hash\":\"");
    char *hash_str = u64_to_hash_string(base_hash);
    if (hash_str) {
        json_writer_puts(&w, hash_str);
        lv00_free((void**)&hash_str);
    }
    json_writer_puts(&w, "\",");

    /* changes */
    json_writer_puts(&w, "\"changes\":{");

    bool has_changes = false;

    /* 检查 name 变化 */
    if ((bl->name && mod->name && strcmp(bl->name, mod->name) != 0) ||
        (bl->name && !mod->name) || (!bl->name && mod->name)) {
        if (has_changes) json_writer_putc(&w, ',');
        json_writer_puts(&w, "\"name\":{\"old\":");
        json_writer_write_escaped_str(&w, bl->name);
        json_writer_puts(&w, ",\"new\":");
        json_writer_write_escaped_str(&w, mod->name);
        json_writer_puts(&w, "}");
        has_changes = true;
    }

    /* 检查 version 变化 */
    if ((bl->version && mod->version && strcmp(bl->version, mod->version) != 0) ||
        (bl->version && !mod->version) || (!bl->version && mod->version)) {
        if (has_changes) json_writer_putc(&w, ',');
        json_writer_puts(&w, "\"version\":{\"old\":");
        json_writer_write_escaped_str(&w, bl->version);
        json_writer_puts(&w, ",\"new\":");
        json_writer_write_escaped_str(&w, mod->version);
        json_writer_puts(&w, "}");
        has_changes = true;
    }

    /* 检查依赖变化 */
    {
        /* 找出被移除的依赖 */
        if (has_changes) json_writer_putc(&w, ',');
        json_writer_puts(&w, "\"dependencies_removed\":[");
        bool first = true;
        for (int i = 0; i < bl->dep_count; i++) {
            bool found = false;
            for (int j = 0; j < mod->dependency_count; j++) {
                if (strcmp(bl->dep_names[i], mod->dependencies[j].name) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (!first) json_writer_putc(&w, ',');
                json_writer_write_escaped_str(&w, bl->dep_names[i]);
                first = false;
                has_changes = true;
            }
        }
        json_writer_puts(&w, "],");

        /* 找出新增的依赖 */
        json_writer_puts(&w, "\"dependencies_added\":[");
        first = true;
        for (int i = 0; i < mod->dependency_count; i++) {
            bool found = false;
            for (int j = 0; j < bl->dep_count; j++) {
                if (strcmp(mod->dependencies[i].name, bl->dep_names[j]) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (!first) json_writer_putc(&w, ',');
                json_writer_putc(&w, '{');
                json_writer_puts(&w, "\"name\":");
                json_writer_write_escaped_str(&w, mod->dependencies[i].name);
                json_writer_puts(&w, ",\"version_constraint\":");
                json_writer_write_escaped_str(&w, mod->dependencies[i].version_constraint);
                json_writer_putc(&w, '}');
                first = false;
                has_changes = true;
            }
        }
        json_writer_puts(&w, "],");

        /* 找出版本约束变化的依赖 */
        json_writer_puts(&w, "\"dependencies_modified\":[");
        first = true;
        for (int i = 0; i < mod->dependency_count; i++) {
            for (int j = 0; j < bl->dep_count; j++) {
                if (strcmp(mod->dependencies[i].name, bl->dep_names[j]) == 0 &&
                    strcmp(mod->dependencies[i].version_constraint, bl->dep_versions[j]) != 0) {
                    if (!first) json_writer_putc(&w, ',');
                    json_writer_putc(&w, '{');
                    json_writer_puts(&w, "\"name\":");
                    json_writer_write_escaped_str(&w, mod->dependencies[i].name);
                    json_writer_puts(&w, ",\"old_version_constraint\":");
                    json_writer_write_escaped_str(&w, bl->dep_versions[j]);
                    json_writer_puts(&w, ",\"new_version_constraint\":");
                    json_writer_write_escaped_str(&w, mod->dependencies[i].version_constraint);
                    json_writer_putc(&w, '}');
                    first = false;
                    has_changes = true;
                    break;
                }
            }
        }
        json_writer_puts(&w, "]");
    }

    /* 检查图变化 */
    {
        if (has_changes) json_writer_putc(&w, ',');
        bool graph_changed = false;

        /* 收集当前图的节点 ID 和坐标哈希 */
        int cur_node_count = (mod->graph) ? mod->graph->node_count : 0;
        int cur_constraint_count = (mod->graph) ? mod->graph->constraint_count : 0;

        int *cur_node_ids = NULL;
        uint64_t *cur_node_hashes = NULL;
        int *cur_constraint_ids = NULL;

        if (cur_node_count > 0) {
            cur_node_ids = (int *)lv00_malloc(sizeof(int) * cur_node_count);
            cur_node_hashes = (uint64_t *)lv00_malloc(sizeof(uint64_t) * cur_node_count);
            for (int i = 0; i < cur_node_count; i++) {
                GeomNode *n = mod->graph->nodes[i];
                cur_node_ids[i] = n ? n->id : -1;
                uint64_t ch = 0;
                if (n && n->symbolic_coords) {
                    for (int c = 0; c < n->coord_count; c++) {
                        if (n->symbolic_coords[c]) {
                            ch ^= symbolic_coord_hash(n->symbolic_coords[c]) + 0x9e3779b9ULL + (ch << 6) + (ch >> 2);
                        }
                    }
                }
                if (n) {
                    ch ^= (uint64_t)n->type * 0x9e3779b9ULL + (ch << 6) + (ch >> 2);
                }
                cur_node_hashes[i] = ch;
            }
        }

        if (cur_constraint_count > 0) {
            cur_constraint_ids = (int *)lv00_malloc(sizeof(int) * cur_constraint_count);
            for (int i = 0; i < cur_constraint_count; i++) {
                Constraint *c = mod->graph->constraints[i];
                cur_constraint_ids[i] = c ? c->id : -1;
            }
        }

        /* nodes_added: 在当前图中但不在基线中 */
        json_writer_puts(&w, "\"nodes_added\":[");
        {
            bool first = true;
            for (int i = 0; i < cur_node_count; i++) {
                bool found = false;
                for (int j = 0; j < bl->graph_node_count; j++) {
                    if (cur_node_ids[i] == bl->graph_node_ids[j]) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    if (!first) json_writer_putc(&w, ',');
                    char tmp[32];
                    snprintf(tmp, sizeof(tmp), "%d", cur_node_ids[i]);
                    json_writer_puts(&w, tmp);
                    first = false;
                    graph_changed = true;
                }
            }
        }
        json_writer_puts(&w, "],");

        /* nodes_removed: 在基线中但不在当前图中 */
        json_writer_puts(&w, "\"nodes_removed\":[");
        {
            bool first = true;
            for (int i = 0; i < bl->graph_node_count; i++) {
                bool found = false;
                for (int j = 0; j < cur_node_count; j++) {
                    if (bl->graph_node_ids[i] == cur_node_ids[j]) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    if (!first) json_writer_putc(&w, ',');
                    char tmp[32];
                    snprintf(tmp, sizeof(tmp), "%d", bl->graph_node_ids[i]);
                    json_writer_puts(&w, tmp);
                    first = false;
                    graph_changed = true;
                }
            }
        }
        json_writer_puts(&w, "],");

        /* nodes_modified: 相同 ID 但坐标哈希不同 */
        json_writer_puts(&w, "\"nodes_modified\":[");
        {
            bool first = true;
            for (int i = 0; i < cur_node_count; i++) {
                for (int j = 0; j < bl->graph_node_count; j++) {
                    if (cur_node_ids[i] == bl->graph_node_ids[j] &&
                        cur_node_hashes[i] != bl->graph_node_coord_hashes[j]) {
                        if (!first) json_writer_putc(&w, ',');
                        char tmp[32];
                        snprintf(tmp, sizeof(tmp), "%d", cur_node_ids[i]);
                        json_writer_puts(&w, tmp);
                        first = false;
                        graph_changed = true;
                        break;
                    }
                }
            }
        }
        json_writer_puts(&w, "],");

        /* constraints_added: 在当前图中但不在基线中 */
        json_writer_puts(&w, "\"constraints_added\":[");
        {
            bool first = true;
            for (int i = 0; i < cur_constraint_count; i++) {
                bool found = false;
                for (int j = 0; j < bl->graph_constraint_count; j++) {
                    if (cur_constraint_ids[i] == bl->graph_constraint_ids[j]) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    if (!first) json_writer_putc(&w, ',');
                    char tmp[32];
                    snprintf(tmp, sizeof(tmp), "%d", cur_constraint_ids[i]);
                    json_writer_puts(&w, tmp);
                    first = false;
                    graph_changed = true;
                }
            }
        }
        json_writer_puts(&w, "],");

        /* constraints_removed: 在基线中但不在当前图中 */
        json_writer_puts(&w, "\"constraints_removed\":[");
        {
            bool first = true;
            for (int i = 0; i < bl->graph_constraint_count; i++) {
                bool found = false;
                for (int j = 0; j < cur_constraint_count; j++) {
                    if (bl->graph_constraint_ids[i] == cur_constraint_ids[j]) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    if (!first) json_writer_putc(&w, ',');
                    char tmp[32];
                    snprintf(tmp, sizeof(tmp), "%d", bl->graph_constraint_ids[i]);
                    json_writer_puts(&w, tmp);
                    first = false;
                    graph_changed = true;
                }
            }
        }
        json_writer_puts(&w, "]");

        if (graph_changed) has_changes = true;

        lv00_free((void**)&cur_node_ids);
        lv00_free((void**)&cur_node_hashes);
        lv00_free((void**)&cur_constraint_ids);
    }

    json_writer_putc(&w, '}'); /* end changes */
    json_writer_putc(&w, '}'); /* end root */

    ModuleDelta *delta = (ModuleDelta *)lv00_malloc(sizeof(ModuleDelta));
    if (!delta) { json_writer_destroy(&w); return NULL; }

    delta->base_version_hash = base_hash;
    delta->delta_data = w.buffer;
    delta->delta_size = w.pos;

    /* 更新基线 */
    store_baseline(mod, base_hash);

    return delta;
}

bool module_apply_delta(Module *mod, const ModuleDelta *delta) {
    if (!mod || !delta || !delta->delta_data) {
        lv00_set_error(LV00_ERROR_INVALID_PARAM, "module_apply_delta: 无效参数");
        return false;
    }

    /* 验证基线哈希 */
    char *current_hash_str = module_compute_version_hash(mod);
    if (current_hash_str) {
        uint64_t current_hash = hash_string_to_u64(current_hash_str);
        lv00_free((void**)&current_hash_str);
        if (current_hash != delta->base_version_hash) {
            lv00_set_error(LV00_ERROR_INVALID_PARAM, "module_apply_delta: 基线版本哈希不匹配");
            return false;
        }
    }

    /* 解析 delta JSON */
    size_t json_len = strlen(delta->delta_data);
    JsonReader r;
    json_reader_init(&r, delta->delta_data, json_len);

    if (json_reader_peek(&r) != '{') {
        lv00_set_error(LV00_ERROR_PARSE, "module_apply_delta: 无效的 delta 数据格式");
        return false;
    }
    r.pos++;

    while (json_reader_peek(&r) != '}' && json_reader_peek(&r) != '\0') {
        char *key = json_reader_read_string(&r);
        if (!key) break;
        if (!json_reader_expect_char(&r, ':')) { lv00_free((void**)&key); break; }

        if (strcmp(key, "changes") == 0) {
            if (json_reader_peek(&r) != '{') { lv00_free((void**)&key); break; }
            r.pos++;

            while (json_reader_peek(&r) != '}' && json_reader_peek(&r) != '\0') {
                char *ck = json_reader_read_string(&r);
                if (!ck) break;
                if (!json_reader_expect_char(&r, ':')) { lv00_free((void**)&ck); break; }

                if (strcmp(ck, "name") == 0) {
                    /* 应用名称变更 */
                    if (json_reader_peek(&r) == '{') {
                        r.pos++;
                        while (json_reader_peek(&r) != '}' && json_reader_peek(&r) != '\0') {
                            char *fk = json_reader_read_string(&r);
                            if (!fk) break;
                            if (!json_reader_expect_char(&r, ':')) { lv00_free((void**)&fk); break; }
                            char *val = json_reader_read_string(&r);
                            if (strcmp(fk, "new") == 0 && val) {
                                lv00_free((void**)&mod->name);
                                mod->name = val;
                                val = NULL;
                            }
                            lv00_free((void**)&val);
                            lv00_free((void**)&fk);
                            if (json_reader_peek(&r) == ',') r.pos++;
                        }
                        if (json_reader_peek(&r) == '}') r.pos++;
                    }
                }
                else if (strcmp(ck, "version") == 0) {
                    if (json_reader_peek(&r) == '{') {
                        r.pos++;
                        while (json_reader_peek(&r) != '}' && json_reader_peek(&r) != '\0') {
                            char *fk = json_reader_read_string(&r);
                            if (!fk) break;
                            if (!json_reader_expect_char(&r, ':')) { lv00_free((void**)&fk); break; }
                            char *val = json_reader_read_string(&r);
                            if (strcmp(fk, "new") == 0 && val) {
                                lv00_free((void**)&mod->version);
                                mod->version = val;
                                val = NULL;
                            }
                            lv00_free((void**)&val);
                            lv00_free((void**)&fk);
                            if (json_reader_peek(&r) == ',') r.pos++;
                        }
                        if (json_reader_peek(&r) == '}') r.pos++;
                    }
                }
                else if (strcmp(ck, "dependencies_added") == 0) {
                    if (json_reader_peek(&r) == '[') {
                        r.pos++;
                        while (json_reader_peek(&r) != ']' && json_reader_peek(&r) != '\0') {
                            if (json_reader_peek(&r) == ',') { r.pos++; continue; }
                            if (json_reader_peek(&r) == '{') {
                                r.pos++;
                                char *dn = NULL, *dv = NULL;
                                while (json_reader_peek(&r) != '}' && json_reader_peek(&r) != '\0') {
                                    char *fk = json_reader_read_string(&r);
                                    if (!fk) break;
                                    if (!json_reader_expect_char(&r, ':')) { lv00_free((void**)&fk); break; }
                                    char *val = json_reader_read_string(&r);
                                    if (strcmp(fk, "name") == 0) { lv00_free((void**)&dn); dn = val; }
                                    else if (strcmp(fk, "version_constraint") == 0) { lv00_free((void**)&dv); dv = val; }
                                    else lv00_free((void**)&val);
                                    lv00_free((void**)&fk);
                                    if (json_reader_peek(&r) == ',') r.pos++;
                                }
                                if (json_reader_peek(&r) == '}') r.pos++;
                                if (dn) {
                                    module_add_dependency(mod, dn, dv ? dv : "");
                                }
                                lv00_free((void**)&dn); lv00_free((void**)&dv);
                            } else {
                                r.pos++;
                            }
                        }
                        if (json_reader_peek(&r) == ']') r.pos++;
                    }
                }
                else if (strcmp(ck, "dependencies_removed") == 0) {
                    if (json_reader_peek(&r) == '[') {
                        r.pos++;
                        while (json_reader_peek(&r) != ']' && json_reader_peek(&r) != '\0') {
                            if (json_reader_peek(&r) == ',') { r.pos++; continue; }
                            char *dep_name = json_reader_read_string(&r);
                            if (dep_name) {
                                /* 查找并移除依赖 */
                                for (int i = 0; i < mod->dependency_count; i++) {
                                    if (strcmp(mod->dependencies[i].name, dep_name) == 0) {
                                        lv00_free((void**)&mod->dependencies[i].name);
                                        lv00_free((void**)&mod->dependencies[i].version_constraint);
                                        /* 将最后一个元素移到当前位置 */
                                        if (i < mod->dependency_count - 1) {
                                            mod->dependencies[i] = mod->dependencies[mod->dependency_count - 1];
                                        }
                                        mod->dependency_count--;
                                        break;
                                    }
                                }
                                lv00_free((void**)&dep_name);
                            }
                        }
                        if (json_reader_peek(&r) == ']') r.pos++;
                    }
                }
                else if (strcmp(ck, "dependencies_modified") == 0) {
                    if (json_reader_peek(&r) == '[') {
                        r.pos++;
                        while (json_reader_peek(&r) != ']' && json_reader_peek(&r) != '\0') {
                            if (json_reader_peek(&r) == ',') { r.pos++; continue; }
                            if (json_reader_peek(&r) == '{') {
                                r.pos++;
                                char *dn = NULL, *nv = NULL;
                                while (json_reader_peek(&r) != '}' && json_reader_peek(&r) != '\0') {
                                    char *fk = json_reader_read_string(&r);
                                    if (!fk) break;
                                    if (!json_reader_expect_char(&r, ':')) { lv00_free((void**)&fk); break; }
                                    char *val = json_reader_read_string(&r);
                                    if (strcmp(fk, "name") == 0) { lv00_free((void**)&dn); dn = val; }
                                    else if (strcmp(fk, "new_version_constraint") == 0) { lv00_free((void**)&nv); nv = val; }
                                    else lv00_free((void**)&val);
                                    lv00_free((void**)&fk);
                                    if (json_reader_peek(&r) == ',') r.pos++;
                                }
                                if (json_reader_peek(&r) == '}') r.pos++;
                                if (dn && nv) {
                                    for (int i = 0; i < mod->dependency_count; i++) {
                                        if (strcmp(mod->dependencies[i].name, dn) == 0) {
                                            lv00_free((void**)&mod->dependencies[i].version_constraint);
                                            mod->dependencies[i].version_constraint = lv00_strdup_safe(nv);
                                            break;
                                        }
                                    }
                                }
                                lv00_free((void**)&dn); lv00_free((void**)&nv);
                            } else {
                                r.pos++;
                            }
                        }
                        if (json_reader_peek(&r) == ']') r.pos++;
                    }
                }
                else if (strcmp(ck, "nodes_added") == 0 ||
                         strcmp(ck, "nodes_removed") == 0 ||
                         strcmp(ck, "nodes_modified") == 0 ||
                         strcmp(ck, "constraints_added") == 0 ||
                         strcmp(ck, "constraints_removed") == 0) {
                    /* 图增量字段：检测到图变化时回退到完整图重序列化 */
                    if (json_reader_peek(&r) == '[') {
                        int cnt = json_reader_count_array_elements(&r);
                        if (cnt > 0 && mod->graph) {
                            /* 有图变化，记录警告并标记需要完整图替换 */
                            fprintf(stderr,
                                "[WARN] module_apply_delta: graph changes detected "
                                "(%s: %d items), falling back to full graph re-serialization\n",
                                ck, cnt);
                        }
                    }
                }
                else {
                    /* 跳过未知字段 */
                    if (json_reader_peek(&r) == '"') {
                        char *tmp = json_reader_read_string(&r);
                        lv00_free((void**)&tmp);
                    } else if (json_reader_peek(&r) == '[') {
                        int cnt = json_reader_count_array_elements(&r);
                        (void)cnt;
                    } else if (json_reader_peek(&r) == '{') {
                        r.pos++;
                        int depth = 1;
                        while (r.pos < r.size && depth > 0) {
                            char c = r.data[r.pos];
                            if (c == '"') {
                                r.pos++;
                                while (r.pos < r.size && r.data[r.pos] != '"') {
                                    if (r.data[r.pos] == '\\') r.pos++;
                                    r.pos++;
                                }
                                if (r.pos < r.size) r.pos++;
                            } else if (c == '{') { depth++; r.pos++; }
                            else if (c == '}') { depth--; r.pos++; }
                            else { r.pos++; }
                        }
                    } else {
                        while (r.pos < r.size && json_reader_peek(&r) != ',' && json_reader_peek(&r) != '}') {
                            r.pos++;
                        }
                    }
                }

                lv00_free((void**)&ck);
                if (json_reader_peek(&r) == ',') r.pos++;
            }
            if (json_reader_peek(&r) == '}') r.pos++;
        }
        else {
            /* 跳过 base_hash 等其他字段 */
            if (json_reader_peek(&r) == '"') {
                char *tmp = json_reader_read_string(&r);
                lv00_free((void**)&tmp);
            } else if (json_reader_peek(&r) == '{') {
                r.pos++;
                int depth = 1;
                while (r.pos < r.size && depth > 0) {
                    char c = r.data[r.pos];
                    if (c == '"') {
                        r.pos++;
                        while (r.pos < r.size && r.data[r.pos] != '"') {
                            if (r.data[r.pos] == '\\') r.pos++;
                            r.pos++;
                        }
                        if (r.pos < r.size) r.pos++;
                    } else if (c == '{') { depth++; r.pos++; }
                    else if (c == '}') { depth--; r.pos++; }
                    else { r.pos++; }
                }
            } else {
                while (r.pos < r.size && json_reader_peek(&r) != ',' && json_reader_peek(&r) != '}') {
                    r.pos++;
                }
            }
        }

        lv00_free((void**)&key);
        if (json_reader_peek(&r) == ',') r.pos++;
    }

    return true;
}

void module_delta_destroy(ModuleDelta *delta) {
    if (delta) {
        lv00_free((void**)&delta->delta_data);
        lv00_free((void**)&delta);
    }
}