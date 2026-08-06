/**
 * @file module_serialize.c
 * @brief 模块序列化（MsgPack/JSON）
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

#include "lv/lv_file.h"

#include "lv/module.h"
#include "lv/module_internal.h"
#include "lv/lv_hash.h"


#include "debug.h"
#include "lv_internal.h"
#include "lv/lv_xmacro.h"
#include "lv_utils.h"
#include "module_helpers.h"



Module *module_create(const char *name, const char *version) {
    Module *mod = lv_calloc(1, sizeof(Module));
    if (!mod)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "module_create: lv_calloc failed");
    mod->name = lv_strdup_safe(name ? name : "unnamed_module");
    mod->version = lv_strdup_safe(version ? version : "0.0.0");
    if (!mod->name || !mod->version) {
        lv_free((void **) &mod->name);
        lv_free((void **) &mod->version);
        lv_free((void **) &mod);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "module_create: strdup failed");
    }
    mod->exports = NULL;
    mod->graph = NULL;

    lv_darray_init(&mod->dependencies, sizeof(ModuleDependency));
    lv_darray_init(&mod->axiom_packages, sizeof(AxiomPackage *));

    mod->exports = lv_calloc(1, sizeof(ModuleExport));
    if (!mod->exports) {
        lv_free((void **) &mod->name);
        lv_free((void **) &mod->version);
        lv_free((void **) &mod);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "module_create: exports calloc failed");
    }
    lv_darray_init(&mod->exports->function_block_ids, sizeof(int));
    lv_darray_init(&mod->exports->type_region_ids, sizeof(int));
    if (module_stream_ctx) {
        stream_emit_simple(module_stream_ctx, STREAM_EVENT_INFO, "模块创建成功", 0);
    }
    return mod;
}

void module_destroy(Module *mod) {
    if (mod) {
        lv_free((void **) &mod->name);
        lv_free((void **) &mod->version);
        for (int i = 0; i < mod->dependencies.count; i++) {
            ModuleDependency *dep = (ModuleDependency *) lv_darray_get(&mod->dependencies, i);
            if (dep) {
                lv_free((void **) &dep->name);
                lv_free((void **) &dep->version_constraint);
            }
        }
        lv_darray_free(&mod->dependencies);
        lv_darray_free(&mod->exports->function_block_ids);
        lv_darray_free(&mod->exports->type_region_ids);
        lv_free((void **) &mod->exports);
        for (int i = 0; i < mod->axiom_packages.count; i++) {
            AxiomPackage **slot = (AxiomPackage **) lv_darray_get(&mod->axiom_packages, i);
            if (slot && *slot) {
                axiom_package_destroy(*slot);
            }
        }
        lv_darray_free(&mod->axiom_packages);
        if (mod->graph)
            graph_destroy(mod->graph);
        lv_free((void **) &mod);
    }
}

bool module_add_dependency(Module *mod, const char *dep_name, const char *version_constraint) {
    if (!mod)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "module_add_dependency: mod is NULL");

    /* 安全复制依赖名称，检查 strdup 是否成功 */
    char *name_copy = lv_strdup_safe(dep_name);
    if (!name_copy) {
        return false;
    }
    char *version_copy = lv_strdup_safe(version_constraint ? version_constraint : "");
    if (!version_copy) {
        lv_free((void **) &name_copy);
        return false;
    }

    ModuleDependency dep;
    dep.name = name_copy;
    dep.version_constraint = version_copy;
    dep.module = NULL;
    if (lv_darray_push(&mod->dependencies, &dep) < 0) {
        lv_free((void **) &name_copy);
        lv_free((void **) &version_copy);
        return false;
    }
    return true;
}

bool module_add_axiom_package(Module *mod, AxiomPackage *pkg) {
    if (!mod)
        return false;
    if (lv_darray_push(&mod->axiom_packages, &pkg) < 0)
        return false;
    return true;
}

bool module_export_function_block(Module *mod, int func_block_id) {
    if (!mod)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "module_export_function_block: mod is NULL");
    if (!mod->exports)
        return false;
    if (lv_darray_push(&mod->exports->function_block_ids, &func_block_id) < 0)
        return false;
    return true;
}

bool module_export_type_region(Module *mod, int type_region_id) {
    if (!mod)
        return false;
    if (lv_darray_push(&mod->exports->type_region_ids, &type_region_id) < 0)
        return false;
    return true;
}

static bool load_recursive(Module *mod, const char *filepath, Module **loaded, int *count, int depth,
                           ModuleLoadStatus *status) {
    /* 检查递归深度 */
    if (depth > MAX_MODULE_DEPTH) {
        lv_set_error(lv_ERROR_RESOURCE_EXHAUSTED, "模块加载深度超过最大限制 (%d)", MAX_MODULE_DEPTH);
        *status = MODULE_LOAD_DEPTH_EXCEEDED;
        return false;
    }

    /* 检查是否已经加载过此模块 (避免循环依赖) */
    for (int i = 0; i < *count; i++) {
        if (strcmp(loaded[i]->name, mod->name) == 0) {
            /* 模块已加载，设置引用 */
            return true;
        }
    }

    /* 将当前模块添加到已加载列表 */
    /* 边界检查：确保不超过 loaded 数组的最大容量 MAX_MODULE_DEPTH */
    if (*count >= MAX_MODULE_DEPTH) {
        lv_set_error(lv_ERROR_RESOURCE_EXHAUSTED, "已加载模块数量超过最大限制 (%d)，无法继续加载模块 '%s'",
                     MAX_MODULE_DEPTH, mod->name);
        *status = MODULE_LOAD_DEPTH_EXCEEDED;
        return false;
    }
    loaded[*count] = mod;
    (*count)++;

    /* 读取文件内容 */
    FILE *f = lv_file_open(filepath, "r");
    if (!f) {
        lv_set_error(lv_ERROR_IO, "无法打开文件: %s", filepath);
        *status = MODULE_LOAD_FILE_NOT_FOUND;
        return false;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0) {
        lv_file_close(f);
        lv_set_error(lv_ERROR_IO, "文件为空: %s", filepath);
        *status = MODULE_LOAD_PARSE_ERROR;
        return false;
    }

    char *buf = lv_calloc(len + 1, 1);
    if (!buf) {
        lv_file_close(f);
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "内存分配失败");
        *status = MODULE_LOAD_PARSE_ERROR;
        return false;
    }

    size_t read_len = fread(buf, 1, len, f);
    lv_file_close(f);
    /* 检查 fread 是否完整读取了文件内容 */
    if (read_len != (size_t) len) {
        lv_free((void **) &buf);
        lv_set_error(lv_ERROR_IO, "文件读取不完整: 期望 %ld 字节, 实际读取 %zu 字节 (%s)", len, read_len, filepath);
        *status = MODULE_LOAD_PARSE_ERROR;
        return false;
    }
    buf[read_len] = '\0';

    /* 初始化解析器并解析文件 */
    LvzParser parser;
    lvz_parser_init(&parser, buf);

    bool parse_result = lvz_parse(&parser, mod);

    lvz_parser_cleanup(&parser);
    lv_free((void **) &buf);

    if (!parse_result) {
        *status = MODULE_LOAD_PARSE_ERROR;
        return false;
    }

    /* 递归加载依赖模块 */
    for (int i = 0; i < mod->dependencies.count; i++) {
        ModuleDependency *dep = (ModuleDependency *) lv_darray_get(&mod->dependencies, i);

        /* 检查依赖是否已经在已加载列表中 */
        bool dep_loaded = false;
        for (int j = 0; j < *count; j++) {
            if (strcmp(loaded[j]->name, dep->name) == 0) {
                dep->module = loaded[j];
                dep_loaded = true;
                break;
            }
        }

        if (!dep_loaded) {
            /* 构建依赖文件路径 */
            /* 假设依赖文件在相同目录下，名称为 <dep_name>.lvz */
            char dep_path[1024];
            const char *last_slash = strrchr(filepath, '/');
            const char *last_backslash = strrchr(filepath, '\\');
            const char *dir_end = (last_slash > last_backslash) ? last_slash : last_backslash;

            if (dir_end) {
                /* 使用 memcpy 进行精确长度复制（已分配 dir_len+1，手动零终止更安全） */
                size_t dir_len = dir_end - filepath + 1;
                memcpy(dep_path, filepath, dir_len);
                dep_path[dir_len] = '\0';
                snprintf(dep_path + dir_len, sizeof(dep_path) - dir_len, "%s.lvz", dep->name);
            } else {
                snprintf(dep_path, sizeof(dep_path), "%s.lvz", dep->name);
            }

            /* 创建依赖模块 */
            Module *dep_mod = module_create(dep->name, dep->version_constraint);
            if (!dep_mod) {
                lv_set_error(lv_ERROR_OUT_OF_MEMORY, "无法创建依赖模块: %s", dep->name);
                *status = MODULE_LOAD_PARSE_ERROR;
                return false;
            }

            /* 递归加载依赖 */
            ModuleLoadStatus dep_status = MODULE_LOAD_OK;
            if (!load_recursive(dep_mod, dep_path, loaded, count, depth + 1, &dep_status)) {
                module_destroy(dep_mod);
                *status = dep_status;
                return false;
            }

            dep->module = dep_mod;
        }
    }

    *status = MODULE_LOAD_OK;
    return true;
}

ModuleLoadStatus module_load(Module *mod, const char *filepath, Module **loaded_modules, int module_count) {
    /* 清除之前的错误 */
    lv_clear_error();

    if (!mod || !filepath) {
        lv_set_error(lv_ERROR_INVALID_PARAM, "无效参数");
        return MODULE_LOAD_PARSE_ERROR;
    }

    /* 初始化已加载模块列表 */
    Module *loaded[MAX_MODULE_DEPTH];
    int count = 0;

    /* 复制已有的已加载模块 */
    for (int i = 0; i < module_count && i < MAX_MODULE_DEPTH; i++) {
        loaded[count++] = loaded_modules[i];
    }

    /* 递归加载模块 */
    ModuleLoadStatus status = MODULE_LOAD_OK;
    if (!load_recursive(mod, filepath, loaded, &count, 0, &status)) {
        return status;
    }

    /* 加载完成后使用三色 DFS 进行完整循环依赖检测 */
    int *cycle_path = NULL;
    int cycle_path_len = 0;
    if (module_full_cycle_detect(loaded, count, &cycle_path, &cycle_path_len)) {
        /* 构建错误消息：报告循环路径 */
        char buf[512] = {0};
        int pos = snprintf(buf, sizeof(buf), "模块循环依赖: ");
        for (int i = 0; i < cycle_path_len; i++) {
            Module *m = loaded[cycle_path[i]];
            if (m) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", m->name);
                if (i < cycle_path_len - 1) {
                    pos += snprintf(buf + pos, sizeof(buf) - pos, " → ");
                }
            }
        }
        lv_set_error(lv_ERROR_INVALID_PARAM, "%s", buf);
        lv_free((void **) &cycle_path);
        return MODULE_LOAD_CIRCULAR_DEPENDENCY;
    }

    if (module_stream_ctx) {
        stream_emit_simple(module_stream_ctx, STREAM_EVENT_INFO, "模块加载成功", 0);
    }

    return MODULE_LOAD_OK;
}

/* ============== 图数据序列化辅助函数 ============== */

/* 将几何类型转换为字符串 */
/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief geom_type_to_string 名称表（由共享 X-macro 单一事实来源生成） */
static const lvStrToEnumEntry s_geom_type_to_string_entries[] = {
    lv_XMACRO_TO_ENUM_TABLE(LV_GEOM_TYPE_X)
};

static const char *geom_type_to_string(GeomType type) {
    return lv_enum_to_str(s_geom_type_to_string_entries, lv_ARRAY_SIZE(s_geom_type_to_string_entries), (int) type, "UNKNOWN");
}

/* 将约束类型转换为字符串 */
/** @brief constraint_type_to_string 名称表（由共享 X-macro 单一事实来源生成） */
static const lvStrToEnumEntry s_constraint_type_to_string_entries[] = {
    lv_XMACRO_TO_ENUM_TABLE(LV_CONSTRAINT_TYPE_X)
};

static const char *constraint_type_to_string(ConstraintType type) {
    return lv_enum_to_str(s_constraint_type_to_string_entries, lv_ARRAY_SIZE(s_constraint_type_to_string_entries), (int) type, "UNKNOWN");
}

/** @brief CoordType → 序列化前缀与缓冲区余量 查找表 */
static const struct {
    const char *prefix;
    int extra;
} kCoordPrefix[] = {
    [RATIONAL]       = {"rational ", 16},
    [QUADRATIC]      = {"quadratic ", 16},
    [ALGEBRAIC]      = {"algebraic ", 16},
    [TRANSCENDENTAL] = {"transcendental ", 20},
};

/* 将符号坐标序列化为字符串（调用者需释放返回的字符串） */
static char *serialize_symbolic_coord(const SymbolicCoord *coord) {
    if (!coord)
        return NULL;

    char *result = NULL;
    if ((unsigned) coord->type < lv_ARRAY_SIZE(kCoordPrefix)) {
        /* 安全：使用 snprintf 并分配足够大的缓冲区 */
        char *str = symbolic_coord_serialize(coord);
        if (str) {
            result = lv_calloc(strlen(str) + kCoordPrefix[coord->type].extra, 1);
            if (result)
                snprintf(result, strlen(str) + kCoordPrefix[coord->type].extra, "%s%s",
                         kCoordPrefix[coord->type].prefix, str);
            lv_free((void **) &str);
        }
    } else {
        result = lv_strdup_safe("unknown");
    }
    return result ? result : lv_strdup_safe("unknown");
}

/* 序列化单个节点 */
static void serialize_node(FILE *f, const GeomNode *node) {
    if (!f || !node)
        return;

    /* 对于PORT类型的节点，获取is_formal_param */
    int is_formal = 0;
    if (node->type == GEOM_PORT && node->data.port) {
        is_formal = node->data.port->is_formal_param ? 1 : 0;
    }

    fprintf(f, "    node %d %s %d %d %d %d\n", node->id, geom_type_to_string(node->type), node->coord_count,
            node->namespace_depth, node->parent_block_id, is_formal);

    /* 序列化符号坐标 */
    for (int i = 0; i < node->coord_count; i++) {
        char *coord_str = serialize_symbolic_coord(node->symbolic_coords[i]);
        if (coord_str) {
            fprintf(f, "      coord %s\n", coord_str);
            lv_free((void **) &coord_str);
        }
    }
}

/* 序列化单个约束 */
static void serialize_constraint(FILE *f, const Constraint *constraint) {
    if (!f || !constraint)
        return;

    fprintf(f, "    constraint %d %s %d", constraint->id, constraint_type_to_string(constraint->type),
            constraint->participant_count);

    /* 序列化参与者ID */
    for (int i = 0; i < constraint->participant_count; i++) {
        fprintf(f, " %d", constraint->participants[i]);
    }
    fprintf(f, "\n");
}

/* 序列化整个约束图 */
static void serialize_constraint_graph(FILE *f, const ConstraintGraph *graph) {
    if (!f || !graph)
        return;

    fprintf(f, "graph %d %d\n", graph->node_count, graph->constraint_count);

    /* 序列化所有节点 */
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]) {
            serialize_node(f, graph->nodes[i]);
        }
    }

    /* 序列化所有约束 */
    for (int i = 0; i < graph->constraint_count; i++) {
        if (graph->constraints[i]) {
            serialize_constraint(f, graph->constraints[i]);
        }
    }
}

ModuleSaveStatus module_save(const Module *mod, const char *filepath) {
    FILE *f = lv_file_open(filepath, "w");
    if (!f)
        return MODULE_SAVE_FILE_ERROR;

    /* 写入文件头注释 */
    fprintf(f, "# LVZ Module File\n");
    fprintf(f, "# Generated by module_save\n\n");

    fprintf(f, "lvz 1.0\n");
    fprintf(f, "module \"%s\" \"%s\"\n", mod->name, mod->version);
    fprintf(f, "deps %d\n", mod->dependencies.count);
    for (int i = 0; i < mod->dependencies.count; i++) {
        ModuleDependency *dep = (ModuleDependency *) lv_darray_get(&mod->dependencies, i);
        fprintf(f, "  dep \"%s\" \"%s\"\n", dep->name, dep->version_constraint);
    }
    fprintf(f, "exports %d %d\n", mod->exports->function_block_ids.count, mod->exports->type_region_ids.count);
    for (int i = 0; i < mod->exports->function_block_ids.count; i++) {
        fprintf(f, "  func_block %d\n", *(int *) lv_darray_get(&mod->exports->function_block_ids, i));
    }
    for (int i = 0; i < mod->exports->type_region_ids.count; i++) {
        fprintf(f, "  type_region %d\n", *(int *) lv_darray_get(&mod->exports->type_region_ids, i));
    }
    fprintf(f, "axioms %d\n", mod->axiom_packages.count);
    for (int i = 0; i < mod->axiom_packages.count; i++) {
        AxiomPackage **pkg = (AxiomPackage **) lv_darray_get(&mod->axiom_packages, i);
        fprintf(f, "  axiom \"%s\"\n", (*pkg)->name);
    }

    /* 序列化约束图数据 */
    if (mod->graph) {
        serialize_constraint_graph(f, mod->graph);
    }

    fprintf(f, "end\n");
    lv_file_close(f);
    if (module_stream_ctx) {
        stream_emit_simple(module_stream_ctx, STREAM_EVENT_INFO, "模块保存成功", 0);
    }
    return MODULE_SAVE_OK;
}

/* ============== FNV-1a 版本哈希（统一委托 lv_hash 模块） ============== */

char *module_compute_version_hash(const Module *mod) {
    if (!mod)
        return NULL;

    lvHashCtx ctx;
    lv_hash_init(&ctx, LV_HASH_FNV1A);

    /* 哈希模块名和版本 */
    lv_hash_str(&ctx, mod->name);
    lv_hash_str(&ctx, mod->version);

    /* 哈希依赖信息 */
    lv_hash_int32(&ctx, mod->dependencies.count);
    for (int i = 0; i < mod->dependencies.count; i++) {
        lv_hash_str(&ctx, ((ModuleDependency *) mod->dependencies.data)[i].name);
        lv_hash_str(&ctx, ((ModuleDependency *) mod->dependencies.data)[i].version_constraint);
    }

    /* 哈希导出信息 */
    lv_hash_int32(&ctx, mod->exports->function_block_ids.count);
    lv_hash_int32(&ctx, mod->exports->type_region_ids.count);

    for (int i = 0; i < mod->exports->function_block_ids.count; i++) {
        lv_hash_int32(&ctx, ((int *) mod->exports->function_block_ids.data)[i]);
    }

    for (int i = 0; i < mod->exports->type_region_ids.count; i++) {
        lv_hash_int32(&ctx, ((int *) mod->exports->type_region_ids.data)[i]);
    }

    /* 哈希公理包信息 */
    lv_hash_int32(&ctx, mod->axiom_packages.count);
    for (int i = 0; i < mod->axiom_packages.count; i++) {
        if (((AxiomPackage **) mod->axiom_packages.data)[i]) {
            lv_hash_str(&ctx, ((AxiomPackage **) mod->axiom_packages.data)[i]->name);
            lv_hash_str(&ctx, ((AxiomPackage **) mod->axiom_packages.data)[i]->version);
        }
    }

    /* 转换为十六进制字符串 (64位 = 16个十六进制字符) */
    return lv_hash_to_hex_alloc(&ctx);
}

bool module_validate_dependency_chain(Module *mod, Module **all_modules, int module_count) {
    if (!mod) return false;
    for (int i = 0; i < mod->dependencies.count; i++) {
        bool found = false;
        for (int j = 0; j < module_count; j++) {
            if (strcmp(all_modules[j]->name, ((ModuleDependency *) mod->dependencies.data)[i].name) == 0) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }
    return true;
}

/**
 * @brief 三色 DFS 标记（白/灰/黑）
 *
 * WHITE = 未访问，GRAY = 访问中（当前 DFS 路径上），BLACK = 已访问完成
 */
typedef enum { DFS_WHITE = 0, DFS_GRAY = 1, DFS_BLACK = 2 } DFSColor;

/**
 * @brief 在模块数组中按指针查找模块索引
 * @return 模块索引，未找到返回 -1
 */
static int module_index_in_array(Module **modules, int count, Module *mod) {
    for (int i = 0; i < count; i++) {
        if (modules[i] == mod)
            return i;
    }
    return -1;
}

/**
 * @brief 递归 DFS 检测循环依赖（三色标记算法）
 *
 * @param mod 当前模块
 * @param modules 所有模块数组
 * @param count 模块数量
 * @param color_map 颜色表
 * @param out_path 输出：循环路径（调用者需 free，可为 NULL）
 * @param out_path_len 输出：路径长度（可为 NULL）
 * @return true 检测到循环，false 无循环
 */
static bool dfs_detect_cycle(Module *mod, Module **modules, int count, DFSColor *color_map, int *path_stack,
                             int *path_stack_len, int **out_path, int *out_path_len) {
    int idx = module_index_in_array(modules, count, mod);
    if (idx < 0)
        return false;

    if (color_map[idx] == DFS_GRAY) {
        /* 检测到循环！当前模块在当前 DFS 路径上 */
        if (out_path && out_path_len && path_stack && path_stack_len) {
            /* 找到当前 idx 在路径栈中的位置 */
            int cycle_start = -1;
            for (int i = 0; i < *path_stack_len; i++) {
                if (path_stack[i] == idx) {
                    cycle_start = i;
                    break;
                }
            }
            if (cycle_start >= 0) {
                int cycle_len = *path_stack_len - cycle_start;
                *out_path = (int *) lv_calloc((size_t) cycle_len, sizeof(int));
                if (*out_path) {
                    memcpy(*out_path, path_stack + cycle_start, (size_t) cycle_len * sizeof(int));
                    *out_path_len = cycle_len;
                }
            }
        }
        return true;
    }
    if (color_map[idx] == DFS_BLACK) {
        /* 已访问完成，无需重复 */
        return false;
    }

    /* 标记为访问中 */
    color_map[idx] = DFS_GRAY;

    /* 将当前模块索引加入路径栈 */
    if (path_stack && path_stack_len) {
        path_stack[*path_stack_len] = idx;
        (*path_stack_len)++;
    }

    /* 遍历依赖 */
    for (int i = 0; i < mod->dependencies.count; i++) {
        Module *dep = ((ModuleDependency *) mod->dependencies.data)[i].module;
        if (dep) {
            if (dfs_detect_cycle(dep, modules, count, color_map, path_stack, path_stack_len, out_path, out_path_len)) {
                return true;
            }
        }
    }

    /* 标记为访问完成 */
    color_map[idx] = DFS_BLACK;
    /* 将当前模块索引移出路径栈 */
    if (path_stack && path_stack_len && *path_stack_len > 0) {
        (*path_stack_len)--;
    }
    return false;
}

/**
 * @brief 模块加载时的完整循环检测
 *
 * 使用三色 DFS 对所有已加载模块执行完整循环检测。
 * 可检测任意复杂循环依赖（如 A→B, A→C, B→D, C→D, D→A）。
 *
 * @param modules 模块数组
 * @param count 模块数量
 * @param out_path 输出：循环路径（调用者需 free，可为 NULL）
 * @param out_path_len 输出：路径长度（可为 NULL）
 * @return true 检测到循环依赖，false 无循环
 */
bool module_full_cycle_detect(Module **modules, int count, int **out_path, int *out_path_len) {
    if (!modules)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "module_full_cycle_detect: modules is NULL");
    if (count <= 0)
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "module_full_cycle_detect: count <= 0");

    DFSColor *color_map = (DFSColor *) lv_calloc((size_t) count, sizeof(DFSColor));
    if (!color_map)
        return false;

    /* 路径栈：跟踪当前 DFS 路径上的模块索引 */
    int path_stack[MAX_MODULE_DEPTH];
    int path_stack_len = 0;

    bool has_cycle = false;
    for (int i = 0; i < count && !has_cycle; i++) {
        if (color_map[i] == DFS_WHITE) {
            has_cycle = dfs_detect_cycle(modules[i], modules, count, color_map, path_stack, &path_stack_len, out_path,
                                         out_path_len);
        }
    }

    lv_free((void **) &color_map);
    return has_cycle;
}

/**
 * @brief 检测模块依赖中是否存在循环依赖（旧版 API，保留向后兼容）
 *
 * 内部调用三色 DFS 算法进行完整检测。
 *
 * @param mod 待检测的模块
 * @param visited 已访问模块列表
 * @param visited_count 已访问模块数量
 * @return true 如果检测到循环依赖，false 否则
 */
bool module_detect_circular_dependency(Module *mod, Module **visited, int visited_count) {
    if (!mod)
        return false;

    /* 构建完整模块数组：visited + mod */
    int total = visited_count + 1;
    Module **all_modules = (Module **) lv_calloc((size_t) total, sizeof(Module *));
    if (!all_modules)
        return false;

    for (int i = 0; i < visited_count; i++) {
        all_modules[i] = visited[i];
    }
    all_modules[visited_count] = mod;

    bool result = module_full_cycle_detect(all_modules, total, NULL, NULL);

    lv_free((void **) &all_modules);
    return result;
}

/* ------------------------------------------------------------------ */
/*  Version constraint parsing                                         */
/* ------------------------------------------------------------------ */

/**
 * @brief Compare two semantic version strings.
 * @return <0 if v1 < v2, 0 if v1 == v2, >0 if v1 > v2
 */
int module_compare_versions(const char *v1, const char *v2) {
    if (!v1 || !v2)
        return 0;

    /* 复用 lv_utils 中的 lvVersion 统一版本比较 */
    lvVersion *ver1 = version_parse(v1);
    lvVersion *ver2 = version_parse(v2);

    if (!ver1 || !ver2) {
        version_destroy(ver1);
        version_destroy(ver2);
        return 0;
    }

    int result = version_compare(ver1, ver2);
    version_destroy(ver1);
    version_destroy(ver2);
    return result;
}

/**
 * @brief Parse and check a semantic version constraint.
 *
 * Supported formats:
 * - "1.0.0" (exact match)
 * - ">=1.0.0" (greater or equal)
 * - "^1.0.0" (compatible major: >=1.0.0 and <2.0.0)
 * - "~1.0.0" (compatible minor: >=1.0.0 and <1.1.0)
 * - "1.0.0 - 2.0.0" (range)
 *
 * @param constraint Version constraint string
 * @param version Version string to check
 * @return true if version satisfies constraint
 */
bool module_parse_version_constraint(const char *constraint, const char *version) {
    if (!constraint)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "module_parse_version_constraint: constraint is NULL");
    if (!version)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "module_parse_version_constraint: version is NULL");

    /* Exact match: "1.0.0" */
    if (strncmp(constraint, ">=", 2) == 0) {
        return module_compare_versions(version, constraint + 2) >= 0;
    }

    /* 脱字号： "^1.0.0" 表示 >=1.0.0 且 <2.0.0 */
    if (constraint[0] == '^') {
        const char *base = constraint + 1;
        if (module_compare_versions(version, base) < 0)
            return false;
        /* 使用 lvVersion 统一比较主版本号是否一致 */
        lvVersion *base_ver = version_parse(base);
        lvVersion *ver = version_parse(version);
        if (!base_ver || !ver) {
            version_destroy(base_ver);
            version_destroy(ver);
            return false;
        }
        bool result = (ver->major == base_ver->major);
        version_destroy(base_ver);
        version_destroy(ver);
        return result;
    }

    /* 波浪号："~1.0.0" 表示 >=1.0.0 且 <1.1.0 */
    if (constraint[0] == '~') {
        const char *base = constraint + 1;
        if (module_compare_versions(version, base) < 0)
            return false;
        /* 使用 lvVersion 统一检查 major.minor 是否一致 */
        lvVersion *base_ver = version_parse(base);
        lvVersion *ver = version_parse(version);
        if (!base_ver || !ver) {
            version_destroy(base_ver);
            version_destroy(ver);
            return false;
        }
        bool result = (ver->major == base_ver->major && ver->minor == base_ver->minor);
        version_destroy(base_ver);
        version_destroy(ver);
        return result;
    }

    /* Range: "1.0.0 - 2.0.0" */
    const char *dash = strstr(constraint, " - ");
    if (dash) {
        size_t lower_len = (size_t) (dash - constraint);
        char *lower = lv_calloc(lower_len + 1, 1);
        memcpy(lower, constraint, lower_len);
        lower[lower_len] = '\0';
        const char *upper = dash + 3;

        bool result = (module_compare_versions(version, lower) >= 0 && module_compare_versions(version, upper) <= 0);
        lv_free((void **) &lower);
        return result;
    }

    /* Default: exact match */
    return module_compare_versions(constraint, version) == 0;
}

