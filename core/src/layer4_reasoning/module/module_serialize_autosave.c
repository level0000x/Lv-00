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
#include "lv/lv_hash.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv/lv_str_utils.h"
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
        if (lv_str_eq(s_autosave_state.entries[i].module_name, module_name)) {
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

/**
 * @brief 释放全部自动保存配置（供程序退出或模块卸载时调用）
 *
 * 释放 module_name 与 backup_directory 的 strdup 副本并清空计数。
 */
void module_autosave_cleanup(void) {
    for (int i = 0; i < s_autosave_state.count; i++) {
        lv_free((void **) &s_autosave_state.entries[i].module_name);
        if (s_autosave_state.entries[i].config.backup_directory) {
            lv_free((void **) &s_autosave_state.entries[i].config.backup_directory);
        }
    }
    s_autosave_state.count = 0;
}

/* module_set_autosave_config 已在 module_delta.c 中实现 */

/* ============== 模块内容哈希（SHA-256，统一委托 lv_hash 模块） ============== */

char *module_compute_content_hash(const Module *mod) {
    if (!mod)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "module_compute_content_hash: mod is NULL");

    lvHashCtx ctx;
    lv_hash_init(&ctx, LV_HASH_SHA256);

    /* 哈希模块名称和版本 */
    lv_hash_str(&ctx, mod->name);
    lv_hash_str(&ctx, mod->version);

    /* 哈希依赖信息 */
    lv_hash_int32(&ctx, mod->dependencies.count);
    for (int i = 0; i < mod->dependencies.count; i++) {
        lv_hash_str(&ctx, ((ModuleDependency *) mod->dependencies.data)[i].name);
        lv_hash_str(&ctx, ((ModuleDependency *) mod->dependencies.data)[i].version_constraint);
    }

    /* 哈希导出信息 */
    if (mod->exports) {
        lv_hash_int32(&ctx, mod->exports->function_block_ids.count);
        for (int i = 0; i < mod->exports->function_block_ids.count; i++) {
            lv_hash_int32(&ctx, ((int *) mod->exports->function_block_ids.data)[i]);
        }
        lv_hash_int32(&ctx, mod->exports->type_region_ids.count);
        for (int i = 0; i < mod->exports->type_region_ids.count; i++) {
            lv_hash_int32(&ctx, ((int *) mod->exports->type_region_ids.data)[i]);
        }
    } else {
        lv_hash_int32(&ctx, 0);
        lv_hash_int32(&ctx, 0);
    }

    /* 哈希公理包信息 */
    lv_hash_int32(&ctx, mod->axiom_packages.count);
    for (int i = 0; i < mod->axiom_packages.count; i++) {
        AxiomPackage *pkg = ((AxiomPackage **) mod->axiom_packages.data)[i];
        if (pkg) {
            lv_hash_str(&ctx, pkg->name);
            lv_hash_str(&ctx, pkg->version);
        } else {
            lv_hash_str(&ctx, NULL);
            lv_hash_str(&ctx, NULL);
        }
    }

    /* 计算最终哈希并转换为十六进制字符串 */
    return lv_hash_to_hex_alloc(&ctx);
}
