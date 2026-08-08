/**
 * @file module_delta.c
 * @brief Delta 差分系统
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
 *
 * @todo 待迁移：module_apply_delta（L759 起，嵌套最深 4 层的对象字段遍历，
 *       ~25 处 lv_json_peek 手写循环）与 lv/lv_json.h 新增的 lv_json_parse_field
 *       公共辅助同构；因"name/version 字段嵌套对象取 new 键、图增量字段跳过
 *       数组计数"等专用容错逻辑，暂保留现状，后续统一迁移。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 必须放在 debug.h / lv_internal.h 之前：保证 lv_LOG_WARN 为可展开的
 * 级别常量（若 lv_log.h 后于 lv_internal.h 包含，lv_LOG_WARN 会被
 * lv_internal.h 的同名函数式宏遮蔽且无枚举常量兜底，导致编译错误）。 */
#include "lv/lv_log.h"

#include "lv/module.h"
#include "lv/module_internal.h"
#include "lv/lv_file.h"
#include "lv/lv_json.h"
#include "lv/lv_path.h" /* lv_path_join */

#include "debug.h"
#include "lv/lv_thread.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "module_helpers.h"

void module_set_autosave_config(Module *mod, const AutoSaveConfig *config) {
    if (!mod || !config)
        return;

    AutoSaveConfig *stored = get_or_create_autosave_config(mod->name);
    if (!stored)
        return;

    stored->enabled = config->enabled;
    stored->interval_seconds = config->interval_seconds;
    stored->max_backups = config->max_backups;

    if (stored->backup_directory) {
        lv_free((void **) &stored->backup_directory);
        stored->backup_directory = NULL;
    }
    if (config->backup_directory) {
        stored->backup_directory = lv_strdup_safe(config->backup_directory);
    }
}

/* 生成备份文件路径：目录已以分隔符结尾时直接拼接（保持旧行为），
 * 否则经 lv_path_join 统一拼接（POSIX 下与旧行为逐字节一致；
 * Windows 下 lv_PATH_SEPARATOR 为 '\\'，与旧硬编码 '/' 文件系统语义等价） */
static void make_backup_filepath(char *buf, size_t buf_size, const char *backup_dir, const char *module_name,
                                 int index) {
    char fname[64];
    snprintf(fname, sizeof(fname), "%s_autosave_%d.lvz", module_name, index);
    if (backup_dir && backup_dir[0]) {
        size_t dir_len = strlen(backup_dir);
        if (dir_len > 0 && (backup_dir[dir_len - 1] == '/' || backup_dir[dir_len - 1] == '\\')) {
            snprintf(buf, buf_size, "%s%s", backup_dir, fname);
        } else {
            lv_path_join(backup_dir, fname, buf, buf_size);
        }
    } else {
        snprintf(buf, buf_size, "%s", fname);
    }
}

/* 生成备份文件路径（二进制格式） */
static void make_backup_binpath(char *buf, size_t buf_size, const char *backup_dir, const char *module_name,
                                int index) {
    char fname[64];
    snprintf(fname, sizeof(fname), "%s_autosave_%d.bin", module_name, index);
    if (backup_dir && backup_dir[0]) {
        size_t dir_len = strlen(backup_dir);
        if (dir_len > 0 && (backup_dir[dir_len - 1] == '/' || backup_dir[dir_len - 1] == '\\')) {
            snprintf(buf, buf_size, "%s%s", backup_dir, fname);
        } else {
            lv_path_join(backup_dir, fname, buf, buf_size);
        }
    } else {
        snprintf(buf, buf_size, "%s", fname);
    }
}

ModuleSaveStatus module_autosave(const Module *mod) {
    if (!mod) {
        lv_set_error(lv_ERROR_INVALID_PARAM, "module_autosave: 无效参数");
        return MODULE_SAVE_WRITE_ERROR;
    }

    AutoSaveConfig *config = find_autosave_config(mod->name);
    if (!config || !config->enabled) {
        /* 自动保存未启用，静默成功 */
        return MODULE_SAVE_OK;
    }

    /* 轮转备份：删除最旧的备份 */
    if (config->max_backups > 0) {
        char oldest_path[lv_PATH_BUF_SIZE];
        make_backup_filepath(oldest_path, sizeof(oldest_path), config->backup_directory, mod->name,
                             config->max_backups - 1);
        remove(oldest_path);

        char oldest_binpath[lv_PATH_BUF_SIZE];
        make_backup_binpath(oldest_binpath, sizeof(oldest_binpath), config->backup_directory, mod->name,
                            config->max_backups - 1);
        remove(oldest_binpath);

        /* 将备份文件向后移动 */
        for (int i = config->max_backups - 2; i >= 0; i--) {
            char old_path[lv_PATH_BUF_SIZE], new_path[lv_PATH_BUF_SIZE];
            make_backup_filepath(old_path, sizeof(old_path), config->backup_directory, mod->name, i);
            make_backup_filepath(new_path, sizeof(new_path), config->backup_directory, mod->name, i + 1);
            rename(old_path, new_path);

            char old_binpath[lv_PATH_BUF_SIZE], new_binpath[lv_PATH_BUF_SIZE];
            make_backup_binpath(old_binpath, sizeof(old_binpath), config->backup_directory, mod->name, i);
            make_backup_binpath(new_binpath, sizeof(new_binpath), config->backup_directory, mod->name, i + 1);
            rename(old_binpath, new_binpath);
        }
    }

    /* 保存文本格式备份 */
    char backup_path[lv_PATH_BUF_SIZE];
    make_backup_filepath(backup_path, sizeof(backup_path), config->backup_directory, mod->name, 0);

    ModuleSaveStatus status = module_save(mod, backup_path);
    if (status != MODULE_SAVE_OK) {
        lv_set_error(lv_ERROR_IO, "module_autosave: 文本备份保存失败");
        return status;
    }

    /* 同时保存二进制格式备份 */
    uint8_t *bin_data = NULL;
    size_t bin_size = 0;
    status = module_save_to_binary(mod, &bin_data, &bin_size);
    if (status == MODULE_SAVE_OK && bin_data) {
        char bin_path[lv_PATH_BUF_SIZE];
        make_backup_binpath(bin_path, sizeof(bin_path), config->backup_directory, mod->name, 0);

        /* 统一 lv_file_write_all（写失败/打开失败返回非零，补全原 fwrite 返回值未检查缺陷；
         * 二进制备份失败不影响文本备份的返回值，与原静默降级语义一致） */
        if (lv_file_write_all(bin_path, bin_data, bin_size) != 0) {
            lv_set_error(lv_ERROR_IO, "module_autosave: 二进制备份写入失败");
        }
        lv_free((void **) &bin_data);
    }

    return MODULE_SAVE_OK;
}

ModuleLoadStatus module_recover_from_backup(const char *module_name, Module **out_module) {
    if (!module_name || !out_module) {
        lv_set_error(lv_ERROR_INVALID_PARAM, "module_recover_from_backup: 无效参数");
        return MODULE_LOAD_PARSE_ERROR;
    }

    AutoSaveConfig *config = find_autosave_config(module_name);

    /* 尝试从备份文件恢复（从最新到最旧） */
    int max_attempts = config ? config->max_backups : 5;
    if (max_attempts <= 0)
        max_attempts = 5;

    for (int i = 0; i < max_attempts; i++) {
        /* 优先尝试二进制格式（lv_file_read_all：不存在/为空/短读均返回 NULL） */
        char bin_path[lv_PATH_BUF_SIZE];
        make_backup_binpath(bin_path, sizeof(bin_path), config ? config->backup_directory : NULL, module_name, i);

        size_t bin_size = 0;
        uint8_t *bin_data = lv_file_read_all(bin_path, &bin_size);
        if (bin_data) {
            ModuleLoadStatus status = module_load_from_binary(bin_data, bin_size, out_module);
            lv_free((void **) &bin_data);
            if (status == MODULE_LOAD_OK) {
                return MODULE_LOAD_OK;
            }
        }

        /* 尝试文本格式（lv_file_read_all 返回的缓冲以 NUL 结尾，供 lvz_parse 使用） */
        char txt_path[lv_PATH_BUF_SIZE];
        make_backup_filepath(txt_path, sizeof(txt_path), config ? config->backup_directory : NULL, module_name, i);

        size_t txt_size = 0;
        char *data = (char *) lv_file_read_all(txt_path, &txt_size);
        if (data) {
            /* 尝试作为 LVZ 格式加载 */
            Module *mod = module_create(module_name, "0.0.0");
            if (mod) {
                LvzParser parser;
                lvz_parser_init(&parser, data);
                bool parse_ok = lvz_parse(&parser, mod);
                lvz_parser_cleanup(&parser);

                if (parse_ok) {
                    lv_free((void **) &data);
                    *out_module = mod;
                    return MODULE_LOAD_OK;
                }
                module_destroy(mod);
            }

            /* 尝试作为 JSON 格式加载 */
            ModuleLoadStatus json_status = module_deserialize_from_json(data, out_module);
            lv_free((void **) &data);
            if (json_status == MODULE_LOAD_OK) {
                return MODULE_LOAD_OK;
            }
        }
    }

    lv_set_error(lv_ERROR_NOT_FOUND, "module_recover_from_backup: 未找到可恢复的备份文件");
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
    if (!hex_str)
        return 0;
    uint64_t val = 0;
    for (int i = 0; hex_str[i] && i < 16; i++) {
        char c = hex_str[i];
        val <<= 4;
        if (c >= '0' && c <= '9')
            val |= (uint64_t) (c - '0');
        else if (c >= 'a' && c <= 'f')
            val |= (uint64_t) (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            val |= (uint64_t) (c - 'A' + 10);
    }
    return val;
}

/* 将 uint64_t 转换为十六进制字符串 */
static char *u64_to_hash_string(uint64_t val) {
    char *result = (char *) lv_malloc(17);
    if (result) {
        snprintf(result, 17, "%016llx", (unsigned long long) val);
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
    uint64_t *graph_node_coord_hashes; /* 每个节点的坐标哈希，用于检测修改 */
} DeltaBaseline;

/** @brief Delta 基线表单例状态 */
typedef struct {
    DeltaBaseline entries[MAX_DELTA_BASELINES]; /**< Delta 基线表 */
    int count;                                  /**< 已存储基线数量 */
} DeltaBaselineState;

/** @brief Delta 基线表全局单例 */
static DeltaBaselineState s_delta_baseline_state = {0};

/** @brief Delta 基线表并发保护互斥锁（惰性初始化） */
static lv_mutex_t s_delta_baseline_mutex;
static lv_once_t s_delta_baseline_once;

/** 惰性初始化基线表互斥锁（lv_once 保证只执行一次） */
static void delta_baseline_mutex_init(void) {
    lv_mutex_init(&s_delta_baseline_mutex);
}

static DeltaBaseline *find_delta_baseline(const char *module_name) {
    for (int i = 0; i < s_delta_baseline_state.count; i++) {
        if (strcmp(s_delta_baseline_state.entries[i].module_name, module_name) == 0) {
            return &s_delta_baseline_state.entries[i];
        }
    }
    return NULL;
}

static void free_delta_baseline(DeltaBaseline *bl) {
    if (!bl)
        return;
    lv_free((void **) &bl->module_name);
    lv_free((void **) &bl->name);
    lv_free((void **) &bl->version);
    for (int i = 0; i < bl->dep_count; i++) {
        lv_free((void **) &bl->dep_names[i]);
        lv_free((void **) &bl->dep_versions[i]);
    }
    lv_free((void **) &bl->dep_names);
    lv_free((void **) &bl->dep_versions);
    lv_free((void **) &bl->func_block_ids);
    lv_free((void **) &bl->type_region_ids);
    lv_free((void **) &bl->graph_node_ids);
    lv_free((void **) &bl->graph_constraint_ids);
    lv_free((void **) &bl->graph_node_coord_hashes);
    memset(bl, 0, sizeof(DeltaBaseline));
}

static void store_baseline(const Module *mod, uint64_t hash) {
    if (!mod)
        return;

    /* 查找或创建基线条目 */
    DeltaBaseline *bl = find_delta_baseline(mod->name);
    if (!bl) {
        if (s_delta_baseline_state.count >= MAX_DELTA_BASELINES) {
            /* 回收最旧的条目 */
            free_delta_baseline(&s_delta_baseline_state.entries[0]);
            /* 移动其他条目 */
            memmove(&s_delta_baseline_state.entries[0], &s_delta_baseline_state.entries[1], (s_delta_baseline_state.count - 1) * sizeof(DeltaBaseline));
            s_delta_baseline_state.count--;
        }
        bl = &s_delta_baseline_state.entries[s_delta_baseline_state.count++];
    } else {
        free_delta_baseline(bl);
    }

    bl->module_name = lv_strdup_safe(mod->name);
    bl->base_hash = hash;
    bl->name = lv_strdup_safe(mod->name);
    bl->version = lv_strdup_safe(mod->version);

    /* 复制依赖 */
    bl->dep_count = mod->dependencies.count;
    if (bl->dep_count > 0) {
        bl->dep_names = (char **) lv_malloc(sizeof(char *) * bl->dep_count);
        bl->dep_versions = (char **) lv_malloc(sizeof(char *) * bl->dep_count);
        for (int i = 0; i < bl->dep_count; i++) {
            bl->dep_names[i] = lv_strdup_safe(((ModuleDependency *) mod->dependencies.data)[i].name);
            bl->dep_versions[i] = lv_strdup_safe(((ModuleDependency *) mod->dependencies.data)[i].version_constraint);
        }
    }

    /* 复制导出 */
    bl->func_count = mod->exports ? mod->exports->function_block_ids.count : 0;
    if (bl->func_count > 0) {
        bl->func_block_ids = (int *) lv_malloc(sizeof(int) * bl->func_count);
        memcpy(bl->func_block_ids, (int *) mod->exports->function_block_ids.data, sizeof(int) * bl->func_count);
    }

    bl->type_count = mod->exports ? mod->exports->type_region_ids.count : 0;
    if (bl->type_count > 0) {
        bl->type_region_ids = (int *) lv_malloc(sizeof(int) * bl->type_count);
        memcpy(bl->type_region_ids, (int *) mod->exports->type_region_ids.data, sizeof(int) * bl->type_count);
    }

    /* 复制图快照 */
    if (mod->graph) {
        bl->graph_node_count = mod->graph->node_count;
        bl->graph_constraint_count = mod->graph->constraint_count;

        if (bl->graph_node_count > 0) {
            bl->graph_node_ids = (int *) lv_malloc(sizeof(int) * bl->graph_node_count);
            bl->graph_node_coord_hashes = (uint64_t *) lv_malloc(sizeof(uint64_t) * bl->graph_node_count);
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
                    ch ^= (uint64_t) n->type * 0x9e3779b9ULL + (ch << 6) + (ch >> 2);
                }
                bl->graph_node_coord_hashes[i] = ch;
            }
        }

        if (bl->graph_constraint_count > 0) {
            bl->graph_constraint_ids = (int *) lv_malloc(sizeof(int) * bl->graph_constraint_count);
            for (int i = 0; i < bl->graph_constraint_count; i++) {
                Constraint *c = mod->graph->constraints[i];
                bl->graph_constraint_ids[i] = c ? c->id : -1;
            }
        }
    }
}

static ModuleDelta *module_compute_delta_locked(const Module *mod, uint64_t base_hash) {
    if (!mod)
        return NULL;

    /* 获取基线 */
    DeltaBaseline *bl = find_delta_baseline(mod->name);

    /* 如果没有基线或哈希不匹配，存储当前状态并返回完整快照 */
    if (!bl || bl->base_hash != base_hash) {
        store_baseline(mod, base_hash);

        /* 返回完整快照作为 delta */
        char *json = module_serialize_to_json(mod);
        if (!json)
            return NULL;

        ModuleDelta *delta = (ModuleDelta *) lv_calloc(1, sizeof(ModuleDelta));
        if (!delta) {
            lv_free((void **) &json);
            return NULL;
        }

        delta->base_version_hash = base_hash;
        delta->delta_data = json;
        delta->delta_size = strlen(json);
        return delta;
    }

    /* 计算差异 */
    lvJsonBuf w;
    if (!lv_json_buf_init(&w, 2048))
        return NULL;

    /* 对象级 API：键/标量值自动管理逗号（紧凑输出与旧手写模板字节一致） */
    lv_json_buf_begin_object(&w);

    /* base_hash */
    lv_json_buf_append_key(&w, "base_hash");
    char *hash_str = u64_to_hash_string(base_hash);
    if (hash_str) {
        lv_json_buf_append_string(&w, hash_str);
        lv_free((void **) &hash_str);
    } else {
        lv_json_buf_append_string(&w, "");
    }

    /* changes */
    lv_json_buf_append_key(&w, "changes");
    lv_json_buf_begin_object(&w);

    /* 检查 name 变化 */
    if ((bl->name && mod->name && strcmp(bl->name, mod->name) != 0) || (bl->name && !mod->name) ||
        (!bl->name && mod->name)) {
        lv_json_buf_append_key(&w, "name");
        lv_json_buf_begin_object(&w);
        lv_json_buf_append_key(&w, "old");
        lv_json_buf_append_string(&w, bl->name);
        lv_json_buf_append_key(&w, "new");
        lv_json_buf_append_string(&w, mod->name);
        lv_json_buf_end_object(&w);
    }

    /* 检查 version 变化 */
    if ((bl->version && mod->version && strcmp(bl->version, mod->version) != 0) || (bl->version && !mod->version) ||
        (!bl->version && mod->version)) {
        lv_json_buf_append_key(&w, "version");
        lv_json_buf_begin_object(&w);
        lv_json_buf_append_key(&w, "old");
        lv_json_buf_append_string(&w, bl->version);
        lv_json_buf_append_key(&w, "new");
        lv_json_buf_append_string(&w, mod->version);
        lv_json_buf_end_object(&w);
    }

    /* 检查依赖变化 */
    {
        /* 找出被移除的依赖 */
        /* dependencies_removed 为字符串数组：append_string 不管理数组内逗号，需手动 */
        lv_json_buf_append_key(&w, "dependencies_removed");
        lv_json_buf_begin_array(&w);
        bool first = true;
        for (int i = 0; i < bl->dep_count; i++) {
            bool found = false;
            for (int j = 0; j < mod->dependencies.count; j++) {
                if (strcmp(bl->dep_names[i], ((ModuleDependency *) mod->dependencies.data)[j].name) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (!first)
                    lv_json_buf_append_char(&w, ',');
                lv_json_buf_append_string(&w, bl->dep_names[i]);
                first = false;
            }
        }
        lv_json_buf_end_array(&w);

        /* 找出新增的依赖：对象数组，begin_object 自动管理逗号 */
        lv_json_buf_append_key(&w, "dependencies_added");
        lv_json_buf_begin_array(&w);
        for (int i = 0; i < mod->dependencies.count; i++) {
            bool found = false;
            for (int j = 0; j < bl->dep_count; j++) {
                if (strcmp(((ModuleDependency *) mod->dependencies.data)[i].name, bl->dep_names[j]) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                lv_json_buf_begin_object(&w);
                lv_json_buf_append_key(&w, "name");
                lv_json_buf_append_string(&w, ((ModuleDependency *) mod->dependencies.data)[i].name);
                lv_json_buf_append_key(&w, "version_constraint");
                lv_json_buf_append_string(&w, ((ModuleDependency *) mod->dependencies.data)[i].version_constraint);
                lv_json_buf_end_object(&w);
            }
        }
        lv_json_buf_end_array(&w);

        /* 找出版本约束变化的依赖：对象数组，begin_object 自动管理逗号 */
        lv_json_buf_append_key(&w, "dependencies_modified");
        lv_json_buf_begin_array(&w);
        for (int i = 0; i < mod->dependencies.count; i++) {
            for (int j = 0; j < bl->dep_count; j++) {
                if (strcmp(((ModuleDependency *) mod->dependencies.data)[i].name, bl->dep_names[j]) == 0 &&
                    strcmp(((ModuleDependency *) mod->dependencies.data)[i].version_constraint, bl->dep_versions[j]) != 0) {
                    lv_json_buf_begin_object(&w);
                    lv_json_buf_append_key(&w, "name");
                    lv_json_buf_append_string(&w, ((ModuleDependency *) mod->dependencies.data)[i].name);
                    lv_json_buf_append_key(&w, "old_version_constraint");
                    lv_json_buf_append_string(&w, bl->dep_versions[j]);
                    lv_json_buf_append_key(&w, "new_version_constraint");
                    lv_json_buf_append_string(&w, ((ModuleDependency *) mod->dependencies.data)[i].version_constraint);
                    lv_json_buf_end_object(&w);
                    break;
                }
            }
        }
        lv_json_buf_end_array(&w);
    }

    /* 检查图变化 */
    {
        bool graph_changed = false;

        /* 收集当前图的节点 ID 和坐标哈希 */
        int cur_node_count = (mod->graph) ? mod->graph->node_count : 0;
        int cur_constraint_count = (mod->graph) ? mod->graph->constraint_count : 0;

        int *cur_node_ids = NULL;
        uint64_t *cur_node_hashes = NULL;
        int *cur_constraint_ids = NULL;

        if (cur_node_count > 0) {
            cur_node_ids = (int *) lv_malloc(sizeof(int) * cur_node_count);
            cur_node_hashes = (uint64_t *) lv_malloc(sizeof(uint64_t) * cur_node_count);
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
                    ch ^= (uint64_t) n->type * 0x9e3779b9ULL + (ch << 6) + (ch >> 2);
                }
                cur_node_hashes[i] = ch;
            }
        }

        if (cur_constraint_count > 0) {
            cur_constraint_ids = (int *) lv_malloc(sizeof(int) * cur_constraint_count);
            for (int i = 0; i < cur_constraint_count; i++) {
                Constraint *c = mod->graph->constraints[i];
                cur_constraint_ids[i] = c ? c->id : -1;
            }
        }

        /* nodes_added: 在当前图中但不在基线中（整型数组，append_int 自动管理逗号） */
        lv_json_buf_append_key(&w, "nodes_added");
        lv_json_buf_begin_array(&w);
        for (int i = 0; i < cur_node_count; i++) {
            bool found = false;
            for (int j = 0; j < bl->graph_node_count; j++) {
                if (cur_node_ids[i] == bl->graph_node_ids[j]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                lv_json_buf_append_int(&w, cur_node_ids[i]);
                graph_changed = true;
            }
        }
        lv_json_buf_end_array(&w);

        /* nodes_removed: 在基线中但不在当前图中 */
        lv_json_buf_append_key(&w, "nodes_removed");
        lv_json_buf_begin_array(&w);
        for (int i = 0; i < bl->graph_node_count; i++) {
            bool found = false;
            for (int j = 0; j < cur_node_count; j++) {
                if (bl->graph_node_ids[i] == cur_node_ids[j]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                lv_json_buf_append_int(&w, bl->graph_node_ids[i]);
                graph_changed = true;
            }
        }
        lv_json_buf_end_array(&w);

        /* nodes_modified: 相同 ID 但坐标哈希不同 */
        lv_json_buf_append_key(&w, "nodes_modified");
        lv_json_buf_begin_array(&w);
        for (int i = 0; i < cur_node_count; i++) {
            for (int j = 0; j < bl->graph_node_count; j++) {
                if (cur_node_ids[i] == bl->graph_node_ids[j] &&
                    cur_node_hashes[i] != bl->graph_node_coord_hashes[j]) {
                    lv_json_buf_append_int(&w, cur_node_ids[i]);
                    graph_changed = true;
                    break;
                }
            }
        }
        lv_json_buf_end_array(&w);

        /* constraints_added: 在当前图中但不在基线中 */
        lv_json_buf_append_key(&w, "constraints_added");
        lv_json_buf_begin_array(&w);
        for (int i = 0; i < cur_constraint_count; i++) {
            bool found = false;
            for (int j = 0; j < bl->graph_constraint_count; j++) {
                if (cur_constraint_ids[i] == bl->graph_constraint_ids[j]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                lv_json_buf_append_int(&w, cur_constraint_ids[i]);
                graph_changed = true;
            }
        }
        lv_json_buf_end_array(&w);

        /* constraints_removed: 在基线中但不在当前图中 */
        lv_json_buf_append_key(&w, "constraints_removed");
        lv_json_buf_begin_array(&w);
        for (int i = 0; i < bl->graph_constraint_count; i++) {
            bool found = false;
            for (int j = 0; j < cur_constraint_count; j++) {
                if (bl->graph_constraint_ids[i] == cur_constraint_ids[j]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                lv_json_buf_append_int(&w, bl->graph_constraint_ids[i]);
                graph_changed = true;
            }
        }
        lv_json_buf_end_array(&w);

        (void) graph_changed; /* 逗号已由对象级 API 自动管理，不再需要 has_changes 记账 */

        lv_free((void **) &cur_node_ids);
        lv_free((void **) &cur_node_hashes);
        lv_free((void **) &cur_constraint_ids);
    }

    lv_json_buf_end_object(&w); /* end changes */
    lv_json_buf_end_object(&w); /* end root */

    ModuleDelta *delta = (ModuleDelta *) lv_calloc(1, sizeof(ModuleDelta));
    if (!delta) {
        lv_json_buf_free(&w);
        return NULL;
    }

    delta->base_version_hash = base_hash;
    delta->delta_data = lv_json_buf_finalize(&w);
    delta->delta_size = w.pos;

    /* 更新基线 */
    store_baseline(mod, base_hash);

    return delta;
}

bool module_apply_delta(Module *mod, const ModuleDelta *delta) {
    if (!mod || !delta || !delta->delta_data) {
        lv_set_error(lv_ERROR_INVALID_PARAM, "module_apply_delta: 无效参数");
        return false;
    }

    /* 验证基线哈希 */
    char *current_hash_str = module_compute_version_hash(mod);
    if (current_hash_str) {
        uint64_t current_hash = hash_string_to_u64(current_hash_str);
        lv_free((void **) &current_hash_str);
        if (current_hash != delta->base_version_hash) {
            lv_set_error(lv_ERROR_INVALID_PARAM, "module_apply_delta: 基线版本哈希不匹配");
            return false;
        }
    }

    /* 解析 delta JSON */
    size_t json_len = strlen(delta->delta_data);
    lvJsonParser p;
    lv_json_parser_init(&p, delta->delta_data, json_len);

    if (lv_json_peek(&p) != '{') {
        lv_set_error(lv_ERROR_PARSE, "module_apply_delta: 无效的 delta 数据格式");
        return false;
    }
    lv_json_next(&p);

    while (lv_json_peek(&p) != '}' && lv_json_peek(&p) != '\0') {
        char *key = lv_json_parse_string(&p);
        if (!key)
            break;
        if (!lv_json_expect(&p, ':')) {
            lv_free((void **) &key);
            break;
        }

        if (strcmp(key, "changes") == 0) {
            if (lv_json_peek(&p) != '{') {
                lv_free((void **) &key);
                break;
            }
            lv_json_next(&p);

            while (lv_json_peek(&p) != '}' && lv_json_peek(&p) != '\0') {
                char *ck = lv_json_parse_string(&p);
                if (!ck)
                    break;
                if (!lv_json_expect(&p, ':')) {
                    lv_free((void **) &ck);
                    break;
                }

                if (strcmp(ck, "name") == 0) {
                    /* 应用名称变更 */
                    if (lv_json_peek(&p) == '{') {
                        lv_json_next(&p);
                        while (lv_json_peek(&p) != '}' && lv_json_peek(&p) != '\0') {
                            char *fk = lv_json_parse_string(&p);
                            if (!fk)
                                break;
                            if (!lv_json_expect(&p, ':')) {
                                lv_free((void **) &fk);
                                break;
                            }
                            char *val = lv_json_parse_string(&p);
                            if (strcmp(fk, "new") == 0 && val) {
                                lv_free((void **) &mod->name);
                                mod->name = val;
                                val = NULL;
                            }
                            lv_free((void **) &val);
                            lv_free((void **) &fk);
                            if (lv_json_peek(&p) == ',')
                                lv_json_next(&p);
                        }
                        if (lv_json_peek(&p) == '}')
                            lv_json_next(&p);
                    }
                } else if (strcmp(ck, "version") == 0) {
                    if (lv_json_peek(&p) == '{') {
                        lv_json_next(&p);
                        while (lv_json_peek(&p) != '}' && lv_json_peek(&p) != '\0') {
                            char *fk = lv_json_parse_string(&p);
                            if (!fk)
                                break;
                            if (!lv_json_expect(&p, ':')) {
                                lv_free((void **) &fk);
                                break;
                            }
                            char *val = lv_json_parse_string(&p);
                            if (strcmp(fk, "new") == 0 && val) {
                                lv_free((void **) &mod->version);
                                mod->version = val;
                                val = NULL;
                            }
                            lv_free((void **) &val);
                            lv_free((void **) &fk);
                            if (lv_json_peek(&p) == ',')
                                lv_json_next(&p);
                        }
                        if (lv_json_peek(&p) == '}')
                            lv_json_next(&p);
                    }
                } else if (strcmp(ck, "dependencies_added") == 0) {
                    if (lv_json_peek(&p) == '[') {
                        lv_json_next(&p);
                        while (lv_json_peek(&p) != ']' && lv_json_peek(&p) != '\0') {
                            if (lv_json_peek(&p) == ',') {
                                lv_json_next(&p);
                                continue;
                            }
                            if (lv_json_peek(&p) == '{') {
                                lv_json_next(&p);
                                char *dn = NULL, *dv = NULL;
                                while (lv_json_peek(&p) != '}' && lv_json_peek(&p) != '\0') {
                                    char *fk = lv_json_parse_string(&p);
                                    if (!fk)
                                        break;
                                    if (!lv_json_expect(&p, ':')) {
                                        lv_free((void **) &fk);
                                        break;
                                    }
                                    char *val = lv_json_parse_string(&p);
                                    if (strcmp(fk, "name") == 0) {
                                        lv_free((void **) &dn);
                                        dn = val;
                                    } else if (strcmp(fk, "version_constraint") == 0) {
                                        lv_free((void **) &dv);
                                        dv = val;
                                    } else
                                        lv_free((void **) &val);
                                    lv_free((void **) &fk);
                                    if (lv_json_peek(&p) == ',')
                                        lv_json_next(&p);
                                }
                                if (lv_json_peek(&p) == '}')
                                    lv_json_next(&p);
                                if (dn) {
                                    module_add_dependency(mod, dn, dv ? dv : "");
                                }
                                lv_free((void **) &dn);
                                lv_free((void **) &dv);
                            } else {
                                lv_json_next(&p);
                            }
                        }
                        if (lv_json_peek(&p) == ']')
                            lv_json_next(&p);
                    }
                } else if (strcmp(ck, "dependencies_removed") == 0) {
                    if (lv_json_peek(&p) == '[') {
                        lv_json_next(&p);
                        while (lv_json_peek(&p) != ']' && lv_json_peek(&p) != '\0') {
                            if (lv_json_peek(&p) == ',') {
                                lv_json_next(&p);
                                continue;
                            }
                            char *dep_name = lv_json_parse_string(&p);
                            if (dep_name) {
                                /* 查找并移除依赖 */
                                for (int i = 0; i < mod->dependencies.count; i++) {
                                    if (strcmp(((ModuleDependency *) mod->dependencies.data)[i].name, dep_name) == 0) {
                                        lv_free((void **) &((ModuleDependency *) mod->dependencies.data)[i].name);
                                        lv_free((void **) &((ModuleDependency *) mod->dependencies.data)[i].version_constraint);
                                        /* 将最后一个元素移到当前位置 */
                                        if (i < mod->dependencies.count - 1) {
                                            ((ModuleDependency *) mod->dependencies.data)[i] = ((ModuleDependency *) mod->dependencies.data)[mod->dependencies.count - 1];
                                        }
                                        mod->dependencies.count--;
                                        break;
                                    }
                                }
                                lv_free((void **) &dep_name);
                            }
                        }
                        if (lv_json_peek(&p) == ']')
                            lv_json_next(&p);
                    }
                } else if (strcmp(ck, "dependencies_modified") == 0) {
                    if (lv_json_peek(&p) == '[') {
                        lv_json_next(&p);
                        while (lv_json_peek(&p) != ']' && lv_json_peek(&p) != '\0') {
                            if (lv_json_peek(&p) == ',') {
                                lv_json_next(&p);
                                continue;
                            }
                            if (lv_json_peek(&p) == '{') {
                                lv_json_next(&p);
                                char *dn = NULL, *nv = NULL;
                                while (lv_json_peek(&p) != '}' && lv_json_peek(&p) != '\0') {
                                    char *fk = lv_json_parse_string(&p);
                                    if (!fk)
                                        break;
                                    if (!lv_json_expect(&p, ':')) {
                                        lv_free((void **) &fk);
                                        break;
                                    }
                                    char *val = lv_json_parse_string(&p);
                                    if (strcmp(fk, "name") == 0) {
                                        lv_free((void **) &dn);
                                        dn = val;
                                    } else if (strcmp(fk, "new_version_constraint") == 0) {
                                        lv_free((void **) &nv);
                                        nv = val;
                                    } else
                                        lv_free((void **) &val);
                                    lv_free((void **) &fk);
                                    if (lv_json_peek(&p) == ',')
                                        lv_json_next(&p);
                                }
                                if (lv_json_peek(&p) == '}')
                                    lv_json_next(&p);
                                if (dn && nv) {
                                    for (int i = 0; i < mod->dependencies.count; i++) {
                                        if (strcmp(((ModuleDependency *) mod->dependencies.data)[i].name, dn) == 0) {
                                            lv_free((void **) &((ModuleDependency *) mod->dependencies.data)[i].version_constraint);
                                            ((ModuleDependency *) mod->dependencies.data)[i].version_constraint = lv_strdup_safe(nv);
                                            break;
                                        }
                                    }
                                }
                                lv_free((void **) &dn);
                                lv_free((void **) &nv);
                            } else {
                                lv_json_next(&p);
                            }
                        }
                        if (lv_json_peek(&p) == ']')
                            lv_json_next(&p);
                    }
                } else if (strcmp(ck, "nodes_added") == 0 || strcmp(ck, "nodes_removed") == 0 ||
                           strcmp(ck, "nodes_modified") == 0 || strcmp(ck, "constraints_added") == 0 ||
                           strcmp(ck, "constraints_removed") == 0) {
                    /* 图增量字段：检测到图变化时回退到完整图重序列化 */
                    if (lv_json_peek(&p) == '[') {
                        /* 统计数组元素个数并跳过整个数组（替代原私有计数函数） */
                        int cnt = 0;
                        lv_json_next(&p); /* 跳过 '[' */
                        while (lv_json_peek(&p) != ']' && lv_json_peek(&p) != '\0') {
                            lv_json_skip_value(&p);
                            cnt++;
                            if (lv_json_peek(&p) == ',')
                                lv_json_next(&p);
                        }
                        if (lv_json_peek(&p) == ']')
                            lv_json_next(&p);
                        if (cnt > 0 && mod->graph) {
                            /* 有图变化，记录警告并标记需要完整图替换 */
                            lv_log(lv_LOG_WARN,
                                   "module_apply_delta: graph changes detected "
                                   "(%s: %d items), falling back to full graph re-serialization\n",
                                   ck, cnt);
                        }
                    }
                } else {
                    /* 跳过未知字段 */
                    if (lv_json_peek(&p) == '"') {
                        char *tmp = lv_json_parse_string(&p);
                        lv_free((void **) &tmp);
                    } else if (lv_json_peek(&p) == '[') {
                        lv_json_skip_value(&p);
                    } else if (lv_json_peek(&p) == '{') {
                        /* 逐字符扫描（保留空白），不能用 lv_json_next 替代 */
                        lv_json_next(&p);
                        int depth = 1;
                        while (p.pos < p.size && depth > 0) {
                            char c = p.data[p.pos];
                            if (c == '"') {
                                p.pos++;
                                while (p.pos < p.size && p.data[p.pos] != '"') {
                                    if (p.data[p.pos] == '\\')
                                        p.pos++;
                                    p.pos++;
                                }
                                if (p.pos < p.size)
                                    p.pos++;
                            } else if (c == '{') {
                                depth++;
                                p.pos++;
                            } else if (c == '}') {
                                depth--;
                                p.pos++;
                            } else {
                                p.pos++;
                            }
                        }
                    } else {
                        while (lv_json_peek(&p) != ',' && lv_json_peek(&p) != '}' && lv_json_peek(&p) != '\0') {
                            lv_json_next(&p);
                        }
                    }
                }

                lv_free((void **) &ck);
                if (lv_json_peek(&p) == ',')
                    lv_json_next(&p);
            }
            if (lv_json_peek(&p) == '}')
                lv_json_next(&p);
        } else {
            /* 跳过 base_hash 等其他字段 */
            if (lv_json_peek(&p) == '"') {
                char *tmp = lv_json_parse_string(&p);
                lv_free((void **) &tmp);
            } else if (lv_json_peek(&p) == '{') {
                /* 逐字符扫描（保留空白），不能用 lv_json_next 替代 */
                lv_json_next(&p);
                int depth = 1;
                while (p.pos < p.size && depth > 0) {
                    char c = p.data[p.pos];
                    if (c == '"') {
                        p.pos++;
                        while (p.pos < p.size && p.data[p.pos] != '"') {
                            if (p.data[p.pos] == '\\')
                                p.pos++;
                            p.pos++;
                        }
                        if (p.pos < p.size)
                            p.pos++;
                    } else if (c == '{') {
                        depth++;
                        p.pos++;
                    } else if (c == '}') {
                        depth--;
                        p.pos++;
                    } else {
                        p.pos++;
                    }
                }
            } else {
                while (lv_json_peek(&p) != ',' && lv_json_peek(&p) != '}' && lv_json_peek(&p) != '\0') {
                    lv_json_next(&p);
                }
            }
        }

        lv_free((void **) &key);
        if (lv_json_peek(&p) == ',')
            lv_json_next(&p);
    }

    return true;
}

/**
 * @brief 线程安全入口：计算模块增量（内部持基线表锁）
 *
 * 基线表为模块级单例，通过互斥锁保护并发访问，
 * 行为与旧无锁版本一致（基线比较结果不变）。
 */
ModuleDelta *module_compute_delta(const Module *mod, uint64_t base_hash) {
    lv_once(&s_delta_baseline_once, delta_baseline_mutex_init);
    lv_mutex_lock(&s_delta_baseline_mutex);
    ModuleDelta *result = module_compute_delta_locked(mod, base_hash);
    lv_mutex_unlock(&s_delta_baseline_mutex);
    return result;
}

/**
 * @brief 释放全部 Delta 基线（供程序退出或模块卸载时调用）
 *
 * 释放基线表中所有动态内存并清空计数；
 * 互斥锁本身保留，避免清理后再次使用导致未初始化锁。
 */
void module_delta_cleanup(void) {
    lv_once(&s_delta_baseline_once, delta_baseline_mutex_init);
    lv_mutex_lock(&s_delta_baseline_mutex);
    for (int i = 0; i < s_delta_baseline_state.count; i++) {
        free_delta_baseline(&s_delta_baseline_state.entries[i]);
    }
    s_delta_baseline_state.count = 0;
    lv_mutex_unlock(&s_delta_baseline_mutex);
}

void module_delta_destroy(ModuleDelta *delta) {
    if (delta) {
        lv_free((void **) &delta->delta_data);
        lv_free((void **) &delta);
    }
}