/**
 * @file module_serialize_autosave.c
 * @brief 自动保存与崩溃恢复
 *
 * @details 从 module_serialize.c 拆分的子模块（Lv-00 项目 v3.3.0+）。
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

#include "lv/lv_file.h"

#include "lv/module.h"
#include "lv/module_internal.h"
#include "lv/sha256.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "module_helpers.h"

/* ================================================================== */
/*  自动保存与崩溃恢复                                                 */
/* ================================================================== */

/*
 * 自动保存配置存储在模块的扩展数据中。
 * 由于 Module 结构体不能修改（向后兼容），使用静态存储。
 * 在实际生产环境中，应将 AutoSaveConfig 嵌入 Module 结构体。
 */

/* 自动保存配置的全局静态存储（按模块名索引） */
#define MAX_AUTOSAVE_ENTRIES 64

typedef struct {
    char *module_name;
    AutoSaveConfig config;
} AutoSaveEntry;

/** @brief 自动保存表单例状态 */
typedef struct {
    AutoSaveEntry entries[MAX_AUTOSAVE_ENTRIES]; /**< 自动保存配置表 */
    int count;                                   /**< 已注册自动保存配置数量 */
} AutoSaveState;

/** @brief 自动保存表全局单例 */
static AutoSaveState s_autosave_state = {0};

AutoSaveConfig *find_autosave_config(const char *module_name) {
    for (int i = 0; i < s_autosave_state.count; i++) {
        if (strcmp(s_autosave_state.entries[i].module_name, module_name) == 0) {
            return &s_autosave_state.entries[i].config;
        }
    }
    return NULL;
}

AutoSaveConfig *get_or_create_autosave_config(const char *module_name) {
    AutoSaveConfig *existing = find_autosave_config(module_name);
    if (existing)
        return existing;

    if (s_autosave_state.count >= MAX_AUTOSAVE_ENTRIES)
        return NULL;

    s_autosave_state.entries[s_autosave_state.count].module_name = lv_strdup_safe(module_name);
    s_autosave_state.entries[s_autosave_state.count].config.enabled = false;
    s_autosave_state.entries[s_autosave_state.count].config.interval_seconds = 60;
    s_autosave_state.entries[s_autosave_state.count].config.backup_directory = NULL;
    s_autosave_state.entries[s_autosave_state.count].config.max_backups = 5;
    s_autosave_state.count++;
    return &s_autosave_state.entries[s_autosave_state.count - 1].config;
}

/* module_set_autosave_config 已在 module_delta.c 中实现 */

/* ============== 模块内容哈希（SHA-256） ============== */

char *module_compute_content_hash(const Module *mod) {
    if (!mod)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "module_compute_content_hash: mod is NULL");

    lvSha256Context ctx;
    lv_sha256_init(&ctx);

    /* 哈希模块名称和版本 */
    if (mod->name) {
        lv_sha256_update(&ctx, (const uint8_t *) mod->name, strlen(mod->name));
    } else {
        lv_sha256_update(&ctx, (const uint8_t *) "(null)", 6);
    }
    if (mod->version) {
        lv_sha256_update(&ctx, (const uint8_t *) mod->version, strlen(mod->version));
    } else {
        lv_sha256_update(&ctx, (const uint8_t *) "(null)", 6);
    }

    /* 哈希依赖信息 */
    lv_sha256_update(&ctx, (const uint8_t *) &mod->dependencies.count, sizeof(mod->dependencies.count));
    for (int i = 0; i < mod->dependencies.count; i++) {
        if (((ModuleDependency *) mod->dependencies.data)[i].name) {
            lv_sha256_update(&ctx, (const uint8_t *) ((ModuleDependency *) mod->dependencies.data)[i].name, strlen(((ModuleDependency *) mod->dependencies.data)[i].name));
        } else {
            lv_sha256_update(&ctx, (const uint8_t *) "(null)", 6);
        }
        if (((ModuleDependency *) mod->dependencies.data)[i].version_constraint) {
            lv_sha256_update(&ctx, (const uint8_t *) ((ModuleDependency *) mod->dependencies.data)[i].version_constraint,
                             strlen(((ModuleDependency *) mod->dependencies.data)[i].version_constraint));
        } else {
            lv_sha256_update(&ctx, (const uint8_t *) "(null)", 6);
        }
    }

    /* 哈希导出信息 */
    if (mod->exports) {
        lv_sha256_update(&ctx, (const uint8_t *) &mod->exports->function_block_ids.count, sizeof(mod->exports->function_block_ids.count));
        for (int i = 0; i < mod->exports->function_block_ids.count; i++) {
            lv_sha256_update(&ctx, (const uint8_t *) &((int *) mod->exports->function_block_ids.data)[i], sizeof(int));
        }
        lv_sha256_update(&ctx, (const uint8_t *) &mod->exports->type_region_ids.count, sizeof(mod->exports->type_region_ids.count));
        for (int i = 0; i < mod->exports->type_region_ids.count; i++) {
            lv_sha256_update(&ctx, (const uint8_t *) &((int *) mod->exports->type_region_ids.data)[i], sizeof(int));
        }
    } else {
        int zero = 0;
        lv_sha256_update(&ctx, (const uint8_t *) &zero, sizeof(zero));
        lv_sha256_update(&ctx, (const uint8_t *) &zero, sizeof(zero));
    }

    /* 哈希公理包信息 */
    lv_sha256_update(&ctx, (const uint8_t *) &mod->axiom_packages.count, sizeof(mod->axiom_packages.count));
    for (int i = 0; i < mod->axiom_packages.count; i++) {
        if (((AxiomPackage **) mod->axiom_packages.data)[i]) {
            if (((AxiomPackage **) mod->axiom_packages.data)[i]->name) {
                lv_sha256_update(&ctx, (const uint8_t *) ((AxiomPackage **) mod->axiom_packages.data)[i]->name,
                                 strlen(((AxiomPackage **) mod->axiom_packages.data)[i]->name));
            } else {
                lv_sha256_update(&ctx, (const uint8_t *) "(null)", 6);
            }
            if (((AxiomPackage **) mod->axiom_packages.data)[i]->version) {
                lv_sha256_update(&ctx, (const uint8_t *) ((AxiomPackage **) mod->axiom_packages.data)[i]->version,
                                 strlen(((AxiomPackage **) mod->axiom_packages.data)[i]->version));
            } else {
                lv_sha256_update(&ctx, (const uint8_t *) "(null)", 6);
            }
        } else {
            lv_sha256_update(&ctx, (const uint8_t *) "(null)", 6);
            lv_sha256_update(&ctx, (const uint8_t *) "(null)", 6);
        }
    }

    /* 计算最终哈希并转换为十六进制字符串 */
    uint8_t hash[32];
    lv_sha256_final(&ctx, hash);

    char *result = (char *) lv_calloc(65, 1);
    if (result) {
        for (int i = 0; i < 32; i++) {
            snprintf(result + i * 2, 65 - (size_t) (i * 2), "%02x", hash[i]);
        }
        result[64] = '\0';
    }

    return result;
}
