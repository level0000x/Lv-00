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
#include "lv/module.h"
#include "lv/module_internal.h"
#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "module_helpers.h"


Module *module_create(const char *name, const char *version) {
    Module *mod = lv_calloc(1, sizeof(Module));
    if (!mod) return NULL;
    mod->name = lv_strdup_safe(name ? name : "unnamed_module");
    mod->version = lv_strdup_safe(version ? version : "0.0.0");
    if (!mod->name || !mod->version) {
        lv_free((void**)&mod->name);
        lv_free((void**)&mod->version);
        lv_free((void**)&mod);
        return NULL;
    }
    mod->dependencies = NULL;
    mod->dependency_count = 0;
    mod->exports = lv_calloc(1, sizeof(ModuleExport));
    if (!mod->exports) {
        lv_free((void**)&mod->name);
        lv_free((void**)&mod->version);
        lv_free((void**)&mod);
        return NULL;
    }
    mod->exports->function_block_ids = NULL;
    mod->exports->type_region_ids = NULL;
    mod->exports->function_count = 0;
    mod->exports->type_count = 0;
    mod->axiom_packages = NULL;
    mod->axiom_package_count = 0;
    mod->graph = NULL;
    if (module_stream_ctx) {
        stream_emit_simple(module_stream_ctx, STREAM_EVENT_INFO, "模块创建成功", 0);
    }
    return mod;
}

void module_destroy(Module *mod) {
    if (mod) {
        lv_free((void**)&mod->name);
        lv_free((void**)&mod->version);
        for (int i = 0; i < mod->dependency_count; i++) {
            lv_free((void**)&mod->dependencies[i].name);
            lv_free((void**)&mod->dependencies[i].version_constraint);
        }
        lv_free((void**)&mod->dependencies);
        lv_free((void**)&mod->exports->function_block_ids);
        lv_free((void**)&mod->exports->type_region_ids);
        lv_free((void**)&mod->exports);
        for (int i = 0; i < mod->axiom_package_count; i++) {
            axiom_package_destroy(mod->axiom_packages[i]);
        }
        lv_free((void**)&mod->axiom_packages);
        if (mod->graph) graph_destroy(mod->graph);
        lv_free((void**)&mod);
    }
}

bool module_add_dependency(Module *mod, const char *dep_name, const char *version_constraint) {
    if (!mod) return false;
    if (mod->dependency_count == 0) {
        mod->dependencies = lv_calloc(1, sizeof(ModuleDependency));
    } else {
        void *tmp = lv_realloc(mod->dependencies, (mod->dependency_count + 1) * sizeof(ModuleDependency));
        if (!tmp) return false;
        mod->dependencies = tmp;
    }
    if (!mod->dependencies) return false;

    /* 安全复制依赖名称，检查 strdup 是否成功 */
    char *name_copy = lv_strdup_safe(dep_name);
    if (!name_copy) {
        return false;
    }
    char *version_copy = lv_strdup_safe(version_constraint ? version_constraint : "");
    if (!version_copy) {
        lv_free((void**)&name_copy);
        return false;
    }

    mod->dependencies[mod->dependency_count].name = name_copy;
    mod->dependencies[mod->dependency_count].version_constraint = version_copy;
    mod->dependencies[mod->dependency_count].module = NULL;
    mod->dependency_count++;
    return true;
}

bool module_add_axiom_package(Module *mod, AxiomPackage *pkg) {
    if (!mod) return false;
    if (mod->axiom_package_count == 0) {
        mod->axiom_packages = lv_calloc(1, sizeof(AxiomPackage*));
    } else {
        void *tmp = lv_realloc(mod->axiom_packages, (mod->axiom_package_count + 1) * sizeof(AxiomPackage*));
        if (!tmp) return false;
        mod->axiom_packages = tmp;
    }
    if (!mod->axiom_packages) return false;
    mod->axiom_packages[mod->axiom_package_count++] = pkg;
    return true;
}

bool module_export_function_block(Module *mod, int func_block_id) {
    if (!mod) return false;
    if (!mod->exports) return false;
    if (mod->exports->function_count == 0) {
        mod->exports->function_block_ids = lv_calloc(1, sizeof(int));
    } else {
        void *tmp = lv_realloc(mod->exports->function_block_ids,
            (mod->exports->function_count + 1) * sizeof(int));
        if (!tmp) return false;
        mod->exports->function_block_ids = tmp;
    }
    if (!mod->exports->function_block_ids) return false;
    mod->exports->function_block_ids[mod->exports->function_count++] = func_block_id;
    return true;
}

bool module_export_type_region(Module *mod, int type_region_id) {
    if (!mod) return false;
    if (mod->exports->type_count == 0) {
        mod->exports->type_region_ids = lv_calloc(1, sizeof(int));
    } else {
        void *tmp = lv_realloc(mod->exports->type_region_ids,
            (mod->exports->type_count + 1) * sizeof(int));
        if (!tmp) return false;
        mod->exports->type_region_ids = tmp;
    }
    if (!mod->exports->type_region_ids) return false;
    mod->exports->type_region_ids[mod->exports->type_count++] = type_region_id;
    return true;
}

static bool load_recursive(Module *mod, const char *filepath, Module **loaded, int *count, int depth, ModuleLoadStatus *status) {
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
    FILE *f = fopen(filepath, "r");
    if (!f) {
        lv_set_error(lv_ERROR_IO, "无法打开文件: %s", filepath);
        *status = MODULE_LOAD_FILE_NOT_FOUND;
        return false;
    }
    
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (len <= 0) {
        fclose(f);
        lv_set_error(lv_ERROR_IO, "文件为空: %s", filepath);
        *status = MODULE_LOAD_PARSE_ERROR;
        return false;
    }
    
    char *buf = lv_calloc(len + 1, 1);
    if (!buf) {
        fclose(f);
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "内存分配失败");
        *status = MODULE_LOAD_PARSE_ERROR;
        return false;
    }
    
    size_t read_len = fread(buf, 1, len, f);
    fclose(f);
    /* 检查 fread 是否完整读取了文件内容 */
    if (read_len != (size_t)len) {
        lv_free((void**)&buf);
        lv_set_error(lv_ERROR_IO, "文件读取不完整: 期望 %ld 字节, 实际读取 %zu 字节 (%s)",
                  len, read_len, filepath);
        *status = MODULE_LOAD_PARSE_ERROR;
        return false;
    }
    buf[read_len] = '\0';
    
    /* 初始化解析器并解析文件 */
    LvzParser parser;
    lvz_parser_init(&parser, buf);
    
    bool parse_result = lvz_parse(&parser, mod);
    
    lvz_parser_cleanup(&parser);
    lv_free((void**)&buf);
    
    if (!parse_result) {
        *status = MODULE_LOAD_PARSE_ERROR;
        return false;
    }
    
    /* 递归加载依赖模块 */
    for (int i = 0; i < mod->dependency_count; i++) {
        ModuleDependency *dep = &mod->dependencies[i];
        
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
        lv_free((void **)&cycle_path);
        return MODULE_LOAD_CIRCULAR_DEPENDENCY;
    }
    
    if (module_stream_ctx) {
        stream_emit_simple(module_stream_ctx, STREAM_EVENT_INFO, "模块加载成功", 0);
    }
    
    return MODULE_LOAD_OK;
}

/* ============== 图数据序列化辅助函数 ============== */

/* 将几何类型转换为字符串 */
static const char *geom_type_to_string(GeomType type) {
    switch (type) {
        case GEOM_POINT: return "POINT";
        case GEOM_LINE_SEGMENT: return "LINE_SEGMENT";
        case GEOM_REGION: return "REGION";
        case GEOM_PORT: return "PORT";
        case GEOM_FUNCTION_BLOCK: return "FUNCTION_BLOCK";
        default: return "UNKNOWN";
    }
}

/* 将约束类型转换为字符串 */
static const char *constraint_type_to_string(ConstraintType type) {
    switch (type) {
        case INCIDENCE: return "INCIDENCE";
        case BETWEENNESS: return "BETWEENNESS";
        case INTERSECTION: return "INTERSECTION";
        case CONTAINMENT: return "CONTAINMENT";
        case CONNECTION: return "CONNECTION";
        default: return "UNKNOWN";
    }
}

/* 将符号坐标序列化为字符串（调用者需释放返回的字符串） */
static char *serialize_symbolic_coord(const SymbolicCoord *coord) {
    if (!coord) return NULL;
    
    char *result = NULL;
    switch (coord->type) {
        case RATIONAL: {
            /* 有理数格式: "rational 分子/分母" */
            char *str = symbolic_coord_serialize(coord);
            if (str) {
                /* 安全：使用 snprintf 并分配足够大的缓冲区 */
                result = lv_calloc(strlen(str) + 16, 1);
                if (result) snprintf(result, strlen(str) + 16, "rational %s", str);
                lv_free((void**)&str);
            }
            break;
        }
        case QUADRATIC: {
            /* 二次根式格式: "quadratic a,b,n" */
            char *str = symbolic_coord_serialize(coord);
            if (str) {
                result = lv_calloc(strlen(str) + 16, 1);
                if (result) snprintf(result, strlen(str) + 16, "quadratic %s", str);
                lv_free((void**)&str);
            }
            break;
        }
        case ALGEBRAIC: {
            /* 代数数格式: "algebraic 多项式系数... 左边界 右边界" */
            char *str = symbolic_coord_serialize(coord);
            if (str) {
                result = lv_calloc(strlen(str) + 16, 1);
                if (result) snprintf(result, strlen(str) + 16, "algebraic %s", str);
                lv_free((void**)&str);
            }
            break;
        }
        case TRANSCENDENTAL: {
            /* 超越常数格式: "transcendental pi" 或 "transcendental e" */
            char *str = symbolic_coord_serialize(coord);
            if (str) {
                result = lv_calloc(strlen(str) + 20, 1);
                if (result) snprintf(result, strlen(str) + 20, "transcendental %s", str);
                lv_free((void**)&str);
            }
            break;
        }
        default:
            result = lv_strdup_safe("unknown");
            break;
    }
    return result ? result : lv_strdup_safe("unknown");
}

/* 序列化单个节点 */
static void serialize_node(FILE *f, const GeomNode *node) {
    if (!f || !node) return;
    
    /* 对于PORT类型的节点，获取is_formal_param */
    int is_formal = 0;
    if (node->type == GEOM_PORT && node->data.port) {
        is_formal = node->data.port->is_formal_param ? 1 : 0;
    }
    
    fprintf(f, "    node %d %s %d %d %d %d\n",
            node->id,
            geom_type_to_string(node->type),
            node->coord_count,
            node->namespace_depth,
            node->parent_block_id,
            is_formal);
    
    /* 序列化符号坐标 */
    for (int i = 0; i < node->coord_count; i++) {
        char *coord_str = serialize_symbolic_coord(node->symbolic_coords[i]);
        if (coord_str) {
            fprintf(f, "      coord %s\n", coord_str);
            lv_free((void**)&coord_str);
        }
    }
}

/* 序列化单个约束 */
static void serialize_constraint(FILE *f, const Constraint *constraint) {
    if (!f || !constraint) return;
    
    fprintf(f, "    constraint %d %s %d",
            constraint->id,
            constraint_type_to_string(constraint->type),
            constraint->participant_count);
    
    /* 序列化参与者ID */
    for (int i = 0; i < constraint->participant_count; i++) {
        fprintf(f, " %d", constraint->participants[i]);
    }
    fprintf(f, "\n");
}

/* 序列化整个约束图 */
static void serialize_constraint_graph(FILE *f, const ConstraintGraph *graph) {
    if (!f || !graph) return;
    
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
    FILE *f = fopen(filepath, "w");
    if (!f) return MODULE_SAVE_FILE_ERROR;
    
    /* 写入文件头注释 */
    fprintf(f, "# LVZ Module File\n");
    fprintf(f, "# Generated by module_save\n\n");
    
    fprintf(f, "lvz 1.0\n");
    fprintf(f, "module \"%s\" \"%s\"\n", mod->name, mod->version);
    fprintf(f, "deps %d\n", mod->dependency_count);
    for (int i = 0; i < mod->dependency_count; i++) {
        fprintf(f, "  dep \"%s\" \"%s\"\n", mod->dependencies[i].name, mod->dependencies[i].version_constraint);
    }
    fprintf(f, "exports %d %d\n", mod->exports->function_count, mod->exports->type_count);
    for (int i = 0; i < mod->exports->function_count; i++) {
        fprintf(f, "  func_block %d\n", mod->exports->function_block_ids[i]);
    }
    for (int i = 0; i < mod->exports->type_count; i++) {
        fprintf(f, "  type_region %d\n", mod->exports->type_region_ids[i]);
    }
    fprintf(f, "axioms %d\n", mod->axiom_package_count);
    for (int i = 0; i < mod->axiom_package_count; i++) {
        fprintf(f, "  axiom \"%s\"\n", mod->axiom_packages[i]->name);
    }
    
    /* 序列化约束图数据 */
    if (mod->graph) {
        serialize_constraint_graph(f, mod->graph);
    }
    
    fprintf(f, "end\n");
    fclose(f);
    if (module_stream_ctx) {
        stream_emit_simple(module_stream_ctx, STREAM_EVENT_INFO, "模块保存成功", 0);
    }
    return MODULE_SAVE_OK;
}

/* ============== FNV-1a 哈希实现 ============== */

/* FNV-1a constants for 64-bit hash */
#define FNV_PRIME        0x100000001b3ULL
#define FNV_OFFSET_BASIS 0xcbf29ce484222325ULL

static void fnv1a_hash_update(uint64_t *hash, const void *data, size_t len) {
    const unsigned char *bytes = (const unsigned char *)data;
    for (size_t i = 0; i < len; i++) {
        *hash ^= bytes[i];
        *hash *= FNV_PRIME;
    }
}

static void fnv1a_hash_string(uint64_t *hash, const char *str) {
    if (str) {
        fnv1a_hash_update(hash, str, strlen(str));
    } else {
        fnv1a_hash_update(hash, "(null)", 6);
    }
}

static void fnv1a_hash_int(uint64_t *hash, int value) {
    fnv1a_hash_update(hash, &value, sizeof(int));
}

char *module_compute_version_hash(const Module *mod) {
    if (!mod) return NULL;
    
    uint64_t hash = FNV_OFFSET_BASIS;
    
    /* 哈希模块名和版本 */
    fnv1a_hash_string(&hash, mod->name);
    fnv1a_hash_string(&hash, mod->version);
    
    /* 哈希依赖信息 */
    fnv1a_hash_int(&hash, mod->dependency_count);
    for (int i = 0; i < mod->dependency_count; i++) {
        fnv1a_hash_string(&hash, mod->dependencies[i].name);
        fnv1a_hash_string(&hash, mod->dependencies[i].version_constraint);
    }
    
    /* 哈希导出信息 */
    fnv1a_hash_int(&hash, mod->exports->function_count);
    fnv1a_hash_int(&hash, mod->exports->type_count);
    
    for (int i = 0; i < mod->exports->function_count; i++) {
        fnv1a_hash_int(&hash, mod->exports->function_block_ids[i]);
    }
    
    for (int i = 0; i < mod->exports->type_count; i++) {
        fnv1a_hash_int(&hash, mod->exports->type_region_ids[i]);
    }
    
    /* 哈希公理包信息 */
    fnv1a_hash_int(&hash, mod->axiom_package_count);
    for (int i = 0; i < mod->axiom_package_count; i++) {
        if (mod->axiom_packages[i]) {
            fnv1a_hash_string(&hash, mod->axiom_packages[i]->name);
            fnv1a_hash_string(&hash, mod->axiom_packages[i]->version);
        }
    }
    
    /* 转换为十六进制字符串 (64位 = 16个十六进制字符) */
    char *result = lv_calloc(17, 1);
    if (result) {
        snprintf(result, 17, "%016llx", (unsigned long long)hash);
    }
    
    return result;
}

bool module_validate_dependency_chain(Module *mod, Module **all_modules, int module_count) {
    for (int i = 0; i < mod->dependency_count; i++) {
        bool found = false;
        for (int j = 0; j < module_count; j++) {
            if (strcmp(all_modules[j]->name, mod->dependencies[i].name) == 0) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

/**
 * @brief 三色 DFS 标记（白/灰/黑）
 *
 * WHITE = 未访问，GRAY = 访问中（当前 DFS 路径上），BLACK = 已访问完成
 */
typedef enum {
    DFS_WHITE = 0,
    DFS_GRAY  = 1,
    DFS_BLACK = 2
} DFSColor;

/**
 * @brief 在模块数组中按指针查找模块索引
 * @return 模块索引，未找到返回 -1
 */
static int module_index_in_array(Module **modules, int count, Module *mod) {
    for (int i = 0; i < count; i++) {
        if (modules[i] == mod) return i;
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
 * @param out_path 输出：循环路径（暂未实现，保留参数供扩展）
 * @param out_path_len 输出：路径长度（暂未实现，保留参数供扩展）
 * @return true 检测到循环，false 无循环
 */
static bool dfs_detect_cycle(Module *mod, Module **modules, int count,
                              DFSColor *color_map,
                              int *path_stack, int *path_stack_len,
                              int **out_path, int *out_path_len) {
    int idx = module_index_in_array(modules, count, mod);
    if (idx < 0) return false;

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
                *out_path = (int *)lv_calloc((size_t)cycle_len, sizeof(int));
                if (*out_path) {
                    memcpy(*out_path, path_stack + cycle_start, (size_t)cycle_len * sizeof(int));
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
    for (int i = 0; i < mod->dependency_count; i++) {
        Module *dep = mod->dependencies[i].module;
        if (dep) {
            if (dfs_detect_cycle(dep, modules, count, color_map,
                                  path_stack, path_stack_len,
                                  out_path, out_path_len)) {
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
 * @param out_path 输出：循环路径（调用者需 free，当前暂未实现）
 * @param out_path_len 输出：路径长度（当前暂未实现）
 * @return true 检测到循环依赖，false 无循环
 */
bool module_full_cycle_detect(Module **modules, int count, int **out_path, int *out_path_len) {
    if (!modules || count <= 0) return false;

    DFSColor *color_map = (DFSColor *)lv_calloc((size_t)count, sizeof(DFSColor));
    if (!color_map) return false;

    /* 路径栈：跟踪当前 DFS 路径上的模块索引 */
    int path_stack[MAX_MODULE_DEPTH];
    int path_stack_len = 0;

    bool has_cycle = false;
    for (int i = 0; i < count && !has_cycle; i++) {
        if (color_map[i] == DFS_WHITE) {
            has_cycle = dfs_detect_cycle(modules[i], modules, count, color_map,
                                          path_stack, &path_stack_len,
                                          out_path, out_path_len);
        }
    }

    lv_free((void **)&color_map);
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
    if (!mod) return false;

    /* 构建完整模块数组：visited + mod */
    int total = visited_count + 1;
    Module **all_modules = (Module **)lv_calloc((size_t)total, sizeof(Module *));
    if (!all_modules) return false;

    for (int i = 0; i < visited_count; i++) {
        all_modules[i] = visited[i];
    }
    all_modules[visited_count] = mod;

    bool result = module_full_cycle_detect(all_modules, total, NULL, NULL);

    lv_free((void **)&all_modules);
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
    if (!v1 || !v2) return 0;

    int major1 = 0, minor1 = 0, patch1 = 0;
    int major2 = 0, minor2 = 0, patch2 = 0;

    if (sscanf(v1, "%d.%d.%d", &major1, &minor1, &patch1) < 1) {
        major1 = minor1 = patch1 = 0;
    }
    if (sscanf(v2, "%d.%d.%d", &major2, &minor2, &patch2) < 1) {
        major2 = minor2 = patch2 = 0;
    }

    if (major1 != major2) return major1 - major2;
    if (minor1 != minor2) return minor1 - minor2;
    return patch1 - patch2;
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
    if (!constraint || !version) return false;

    /* Exact match: "1.0.0" */
    if (strncmp(constraint, ">=", 2) == 0) {
        return module_compare_versions(version, constraint + 2) >= 0;
    }

    /* 脱字号： "^1.0.0" 表示 >=1.0.0 且 <2.0.0 */
    if (constraint[0] == '^') {
        const char *base = constraint + 1;
        if (module_compare_versions(version, base) < 0) return false;
        /* Check major version match: extract major from base */
        int base_major = 0;
        if (sscanf(base, "%d", &base_major) < 1) base_major = 0;
        int ver_major = 0;
        if (sscanf(version, "%d", &ver_major) < 1) return false;
        return ver_major == base_major;
    }

    /* 波浪号："~1.0.0" 表示 >=1.0.0 且 <1.1.0 */
    if (constraint[0] == '~') {
        const char *base = constraint + 1;
        if (module_compare_versions(version, base) < 0) return false;
        /* Check major.minor match */
        int base_major = 0, base_minor = 0;
        if (sscanf(base, "%d.%d", &base_major, &base_minor) < 1) {
            base_major = base_minor = 0;
        }
        int ver_major = 0, ver_minor = 0;
        if (sscanf(version, "%d.%d", &ver_major, &ver_minor) < 1) return false;
        return ver_major == base_major && ver_minor == base_minor;
    }

    /* Range: "1.0.0 - 2.0.0" */
    const char *dash = strstr(constraint, " - ");
    if (dash) {
        size_t lower_len = (size_t)(dash - constraint);
        char *lower = lv_calloc(lower_len + 1, 1);
        memcpy(lower, constraint, lower_len);
        lower[lower_len] = '\0';
        const char *upper = dash + 3;

        bool result = (module_compare_versions(version, lower) >= 0 &&
                       module_compare_versions(version, upper) <= 0);
        lv_free((void**)&lower);
        return result;
    }

    /* Default: exact match */
    return module_compare_versions(constraint, version) == 0;
}

/* ================================================================== */
/*  最小化 MessagePack 编码/解码器                                     */
/* ================================================================== */

typedef enum {
    MSGPACK_NIL     = 0xc0,
    MSGPACK_FALSE   = 0xc2,
    MSGPACK_TRUE    = 0xc3,
    MSGPACK_FIXSTR  = 0xa0,     /* fixstr: 101xxxxx, up to 31 bytes */
    MSGPACK_STR8    = 0xd9,
    MSGPACK_STR16   = 0xda,
    MSGPACK_STR32   = 0xdb,
    MSGPACK_BIN8    = 0xc4,
    MSGPACK_BIN16   = 0xc5,
    MSGPACK_BIN32   = 0xc6,
    MSGPACK_ARRAY16 = 0xdc,
    MSGPACK_MAP16   = 0xde,
    MSGPACK_INT8    = 0xd0,
    MSGPACK_INT16   = 0xd1,
    MSGPACK_INT32   = 0xd2,
    MSGPACK_INT64   = 0xd3,
    MSGPACK_UINT8   = 0xcc,
    MSGPACK_UINT16  = 0xcd,
    MSGPACK_UINT32  = 0xce,
    MSGPACK_UINT64  = 0xcf,
    MSGPACK_FIXINT  = 0x00      /* fixint: 0xxxxxxx, 0~127 */
} MsgPackType;

/* 编码器 */
typedef struct {
    uint8_t *buffer;
    size_t capacity;
    size_t pos;
    bool error;  /* 编码错误标志：ensure 失败时设置 */
} MsgPackEncoder;

/* 解码器 */
typedef struct {
    const uint8_t *data;
    size_t size;
    size_t pos;
} MsgPackDecoder;

/* ---------- 编码器辅助函数 ---------- */

static bool mp_encoder_init(MsgPackEncoder *enc, size_t initial_capacity) {
    enc->buffer = (uint8_t *)lv_calloc(initial_capacity, 1);
    if (!enc->buffer) return false;
    enc->capacity = initial_capacity;
    enc->pos = 0;
    enc->error = false;
    return true;
}

static bool mp_encoder_ensure(MsgPackEncoder *enc, size_t extra) {
    while (enc->pos + extra > enc->capacity) {
        size_t new_cap = enc->capacity * 2;
        uint8_t *new_buf = (uint8_t *)lv_realloc(enc->buffer, new_cap);
        if (!new_buf) return false;
        enc->buffer = new_buf;
        enc->capacity = new_cap;
    }
    return true;
}

static void mp_encoder_write_byte(MsgPackEncoder *enc, uint8_t b) {
    if (enc->error) return;
    if (!mp_encoder_ensure(enc, 1)) {
        enc->error = true;
        return;
    }
    enc->buffer[enc->pos++] = b;
}

static void mp_encoder_write_u16(MsgPackEncoder *enc, uint16_t v) {
    if (enc->error) return;
    if (!mp_encoder_ensure(enc, 2)) {
        enc->error = true;
        return;
    }
    enc->buffer[enc->pos++] = (uint8_t)(v >> 8);
    enc->buffer[enc->pos++] = (uint8_t)(v & 0xff);
}

static void mp_encoder_write_u32(MsgPackEncoder *enc, uint32_t v) {
    if (enc->error) return;
    if (!mp_encoder_ensure(enc, 4)) {
        enc->error = true;
        return;
    }
    enc->buffer[enc->pos++] = (uint8_t)(v >> 24);
    enc->buffer[enc->pos++] = (uint8_t)(v >> 16);
    enc->buffer[enc->pos++] = (uint8_t)(v >> 8);
    enc->buffer[enc->pos++] = (uint8_t)(v & 0xff);
}

static void mp_encoder_write_u64(MsgPackEncoder *enc, uint64_t v) {
    if (enc->error) return;
    if (!mp_encoder_ensure(enc, 8)) {
        enc->error = true;
        return;
    }
    for (int i = 7; i >= 0; i--) {
        enc->buffer[enc->pos++] = (uint8_t)((v >> (i * 8)) & 0xff);
    }
}

static void mp_encoder_write_i16(MsgPackEncoder *enc, int16_t v) {
    mp_encoder_write_u16(enc, (uint16_t)v);
}

static void mp_encoder_write_i32(MsgPackEncoder *enc, int32_t v) {
    mp_encoder_write_u32(enc, (uint32_t)v);
}

static void mp_encoder_write_i64(MsgPackEncoder *enc, int64_t v) {
    mp_encoder_write_u64(enc, (uint64_t)v);
}

/* 编码 fixint (0~127) */
static void mp_encoder_write_fixint(MsgPackEncoder *enc, int8_t v) {
    mp_encoder_write_byte(enc, (uint8_t)v);
}

/* 编码正整数 */
static void mp_encoder_write_uint(MsgPackEncoder *enc, uint64_t v) {
    if (v <= 127) {
        mp_encoder_write_byte(enc, (uint8_t)v);
    } else if (v <= 0xff) {
        mp_encoder_write_byte(enc, MSGPACK_UINT8);
        mp_encoder_write_byte(enc, (uint8_t)v);
    } else if (v <= 0xffff) {
        mp_encoder_write_byte(enc, MSGPACK_UINT16);
        mp_encoder_write_u16(enc, (uint16_t)v);
    } else if (v <= 0xffffffffUL) {
        mp_encoder_write_byte(enc, MSGPACK_UINT32);
        mp_encoder_write_u32(enc, (uint32_t)v);
    } else {
        mp_encoder_write_byte(enc, MSGPACK_UINT64);
        mp_encoder_write_u64(enc, v);
    }
}

/* 编码负整数 */
static void mp_encoder_write_int(MsgPackEncoder *enc, int64_t v) {
    if (v >= 0) {
        mp_encoder_write_uint(enc, (uint64_t)v);
    } else if (v >= -32) {
        mp_encoder_write_byte(enc, (uint8_t)(0xe0 | (int8_t)(-1 - v)));
    } else if (v >= -128) {
        mp_encoder_write_byte(enc, MSGPACK_INT8);
        mp_encoder_write_byte(enc, (uint8_t)v);
    } else if (v >= -32768) {
        mp_encoder_write_byte(enc, MSGPACK_INT16);
        mp_encoder_write_i16(enc, (int16_t)v);
    } else if (v >= -2147483648LL) {
        mp_encoder_write_byte(enc, MSGPACK_INT32);
        mp_encoder_write_i32(enc, (int32_t)v);
    } else {
        mp_encoder_write_byte(enc, MSGPACK_INT64);
        mp_encoder_write_i64(enc, v);
    }
}

/* 编写字符串 */
static void mp_encoder_write_str(MsgPackEncoder *enc, const char *str) {
    if (!str) {
        mp_encoder_write_byte(enc, MSGPACK_NIL);
        return;
    }
    size_t len = strlen(str);
    if (len <= 31) {
        mp_encoder_write_byte(enc, (uint8_t)(MSGPACK_FIXSTR | len));
    } else if (len <= 0xff) {
        mp_encoder_write_byte(enc, MSGPACK_STR8);
        mp_encoder_write_byte(enc, (uint8_t)len);
    } else if (len <= 0xffff) {
        mp_encoder_write_byte(enc, MSGPACK_STR16);
        mp_encoder_write_u16(enc, (uint16_t)len);
    } else {
        mp_encoder_write_byte(enc, MSGPACK_STR32);
        mp_encoder_write_u32(enc, (uint32_t)len);
    }
    mp_encoder_ensure(enc, len);
    if (enc->error) return;
    memcpy(enc->buffer + enc->pos, str, len);
    enc->pos += len;
}

/* 编码二进制数据 */
static void mp_encoder_write_bin(MsgPackEncoder *enc, const uint8_t *data, size_t len) {
    if (enc->error) return;
    if (len <= 0xff) {
        mp_encoder_write_byte(enc, MSGPACK_BIN8);
        mp_encoder_write_byte(enc, (uint8_t)len);
    } else if (len <= 0xffff) {
        mp_encoder_write_byte(enc, MSGPACK_BIN16);
        mp_encoder_write_u16(enc, (uint16_t)len);
    } else {
        mp_encoder_write_byte(enc, MSGPACK_BIN32);
        mp_encoder_write_u32(enc, (uint32_t)len);
    }
    mp_encoder_ensure(enc, len);
    if (enc->error) return;
    memcpy(enc->buffer + enc->pos, data, len);
    enc->pos += len;
}

/* 编码数组头 */
static void mp_encoder_write_array_header(MsgPackEncoder *enc, uint16_t count) {
    if (count <= 15) {
        mp_encoder_write_byte(enc, (uint8_t)(0x90 | count));
    } else {
        mp_encoder_write_byte(enc, MSGPACK_ARRAY16);
        mp_encoder_write_u16(enc, count);
    }
}

/* 编码 map 头 */
static void mp_encoder_write_map_header(MsgPackEncoder *enc, uint16_t count) {
    if (count <= 15) {
        mp_encoder_write_byte(enc, (uint8_t)(0x80 | count));
    } else {
        mp_encoder_write_byte(enc, MSGPACK_MAP16);
        mp_encoder_write_u16(enc, count);
    }
}

static void mp_encoder_destroy(MsgPackEncoder *enc) {
    lv_free((void**)&enc->buffer);
    enc->buffer = NULL;
    enc->capacity = 0;
    enc->pos = 0;
    enc->error = false;
}

/* ---------- 解码器辅助函数 ---------- */

static bool mp_decoder_init(MsgPackDecoder *dec, const uint8_t *data, size_t size) {
    dec->data = data;
    dec->size = size;
    dec->pos = 0;
    return data != NULL && size > 0;
}

static bool mp_decoder_has_data(MsgPackDecoder *dec) {
    return dec->pos < dec->size;
}

static uint8_t mp_decoder_peek(MsgPackDecoder *dec) {
    return dec->pos < dec->size ? dec->data[dec->pos] : 0;
}

static uint8_t mp_decoder_read_byte(MsgPackDecoder *dec) {
    return dec->pos < dec->size ? dec->data[dec->pos++] : 0;
}

static uint16_t mp_decoder_read_u16(MsgPackDecoder *dec) {
    uint16_t hi = mp_decoder_read_byte(dec);
    uint16_t lo = mp_decoder_read_byte(dec);
    return (uint16_t)((hi << 8) | lo);
}

static uint32_t mp_decoder_read_u32(MsgPackDecoder *dec) {
    uint32_t b0 = mp_decoder_read_byte(dec);
    uint32_t b1 = mp_decoder_read_byte(dec);
    uint32_t b2 = mp_decoder_read_byte(dec);
    uint32_t b3 = mp_decoder_read_byte(dec);
    return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
}

static uint64_t mp_decoder_read_u64(MsgPackDecoder *dec) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) {
        v = (v << 8) | mp_decoder_read_byte(dec);
    }
    return v;
}

static int64_t mp_decoder_read_i64(MsgPackDecoder *dec) {
    return (int64_t)mp_decoder_read_u64(dec);
}

/* 解码整数 */
static bool mp_decoder_read_int(MsgPackDecoder *dec, int64_t *out) {
    if (!mp_decoder_has_data(dec)) return false;
    uint8_t type = mp_decoder_peek(dec);
    mp_decoder_read_byte(dec);

    if (type <= 0x7f) {
        /* fixint positive */
        *out = (int64_t)type;
    } else if (type >= 0xe0) {
        /* fixint negative */
        *out = (int64_t)(int8_t)type;
    } else if (type == MSGPACK_INT8) {
        *out = (int64_t)(int8_t)mp_decoder_read_byte(dec);
    } else if (type == MSGPACK_INT16) {
        int16_t v = (int16_t)mp_decoder_read_u16(dec);
        *out = (int64_t)v;
    } else if (type == MSGPACK_INT32) {
        int32_t v = (int32_t)mp_decoder_read_u32(dec);
        *out = (int64_t)v;
    } else if (type == MSGPACK_INT64) {
        *out = mp_decoder_read_i64(dec);
    } else if (type == MSGPACK_UINT8) {
        *out = (int64_t)mp_decoder_read_byte(dec);
    } else if (type == MSGPACK_UINT16) {
        *out = (int64_t)mp_decoder_read_u16(dec);
    } else if (type == MSGPACK_UINT32) {
        *out = (int64_t)mp_decoder_read_u32(dec);
    } else if (type == MSGPACK_UINT64) {
        *out = (int64_t)mp_decoder_read_u64(dec);
    } else {
        return false;
    }
    return true;
}

/* 解码字符串（返回 malloc 分配的字符串，调用者负责 free） */
static bool mp_decoder_read_str(MsgPackDecoder *dec, char **out) {
    if (!mp_decoder_has_data(dec)) return false;
    uint8_t type = mp_decoder_peek(dec);
    mp_decoder_read_byte(dec);

    size_t len = 0;
    if (type >= 0xa0 && type <= 0xbf) {
        len = type & 0x1f;
    } else if (type == MSGPACK_STR8) {
        len = mp_decoder_read_byte(dec);
    } else if (type == MSGPACK_STR16) {
        len = mp_decoder_read_u16(dec);
    } else if (type == MSGPACK_STR32) {
        len = mp_decoder_read_u32(dec);
    } else {
        return false;
    }

    if (dec->pos + len > dec->size) return false;

    char *str = (char *)lv_calloc(len + 1, 1);
    if (!str) return false;
    memcpy(str, dec->data + dec->pos, len);
    str[len] = '\0';
    dec->pos += len;
    *out = str;
    return true;
}

/* 解码二进制数据 */
static bool mp_decoder_read_bin(MsgPackDecoder *dec, uint8_t **out, size_t *out_len) {
    if (!mp_decoder_has_data(dec)) return false;
    uint8_t type = mp_decoder_peek(dec);
    mp_decoder_read_byte(dec);

    size_t len = 0;
    if (type == MSGPACK_BIN8) {
        len = mp_decoder_read_byte(dec);
    } else if (type == MSGPACK_BIN16) {
        len = mp_decoder_read_u16(dec);
    } else if (type == MSGPACK_BIN32) {
        len = mp_decoder_read_u32(dec);
    } else {
        return false;
    }

    if (dec->pos + len > dec->size) return false;

    uint8_t *buf = (uint8_t *)lv_calloc(len, 1);
    if (!buf) return false;
    memcpy(buf, dec->data + dec->pos, len);
    dec->pos += len;
    *out = buf;
    *out_len = len;
    return true;
}

/* 解码数组头 */
static bool mp_decoder_read_array_header(MsgPackDecoder *dec, uint16_t *count) {
    if (!mp_decoder_has_data(dec)) return false;
    uint8_t type = mp_decoder_peek(dec);
    mp_decoder_read_byte(dec);

    if (type >= 0x90 && type <= 0x9f) {
        *count = type & 0x0f;
    } else if (type == MSGPACK_ARRAY16) {
        *count = mp_decoder_read_u16(dec);
    } else {
        return false;
    }
    return true;
}

/* 解码 map 头 */
static bool mp_decoder_read_map_header(MsgPackDecoder *dec, uint16_t *count) {
    if (!mp_decoder_has_data(dec)) return false;
    uint8_t type = mp_decoder_peek(dec);
    mp_decoder_read_byte(dec);

    if (type >= 0x80 && type <= 0x8f) {
        *count = type & 0x0f;
    } else if (type == MSGPACK_MAP16) {
        *count = mp_decoder_read_u16(dec);
    } else {
        return false;
    }
    return true;
}

/* 跳过一条完整的 MessagePack 值（用于跳过未知的 map 值） */
static bool mp_decoder_skip_value(MsgPackDecoder *dec) {
    if (!mp_decoder_has_data(dec)) return false;
    uint8_t type = mp_decoder_peek(dec);
    mp_decoder_read_byte(dec); /* consume type byte */

    if (type <= 0x7f) return true; /* positive fixint */
    if (type >= 0xe0) return true; /* negative fixint */
    if (type == 0xc0 || type == 0xc2 || type == 0xc3) return true; /* nil, false, true */

    if (type == 0xcc) { /* uint8 */
        return mp_decoder_has_data(dec) && (mp_decoder_read_byte(dec), true);
    }
    if (type == 0xcd) { /* uint16 */
        if (dec->pos + 2 > dec->size) return false;
        dec->pos += 2; return true;
    }
    if (type == 0xce) { /* uint32 */
        if (dec->pos + 4 > dec->size) return false;
        dec->pos += 4; return true;
    }
    if (type == 0xd0) { /* int8 */
        return mp_decoder_has_data(dec) && (mp_decoder_read_byte(dec), true);
    }
    if (type == 0xd1) { /* int16 */
        if (dec->pos + 2 > dec->size) return false;
        dec->pos += 2; return true;
    }
    if (type == 0xd2) { /* int32 */
        if (dec->pos + 4 > dec->size) return false;
        dec->pos += 4; return true;
    }
    if (type == 0xd3) { /* int64 */
        if (dec->pos + 8 > dec->size) return false;
        dec->pos += 8; return true;
    }
    if (type == 0xca || type == 0xcb) { /* float32/64 */
        uint32_t skip = (type == 0xca) ? 4 : 8;
        if (dec->pos + skip > dec->size) return false;
        dec->pos += skip; return true;
    }

    /* fixstr: 0xa0-0xbf */
    if (type >= 0xa0 && type <= 0xbf) {
        uint8_t len = type & 0x1f;
        if (dec->pos + len > dec->size) return false;
        dec->pos += len; return true;
    }
    /* str8 */
    if (type == 0xd9) {
        if (!mp_decoder_has_data(dec)) return false;
        uint8_t len = mp_decoder_read_byte(dec);
        if (dec->pos + len > dec->size) return false;
        dec->pos += len; return true;
    }
    /* str16 */
    if (type == 0xda) {
        if (dec->pos + 2 > dec->size) return false;
        uint16_t len = (uint16_t)(dec->data[dec->pos] << 8 | dec->data[dec->pos+1]);
        dec->pos += 2;
        if (dec->pos + len > dec->size) return false;
        dec->pos += len; return true;
    }
    /* str32 */
    if (type == 0xdb) {
        if (dec->pos + 4 > dec->size) return false;
        uint32_t len = (uint32_t)((uint32_t)dec->data[dec->pos] << 24 | (uint32_t)dec->data[dec->pos+1] << 16 | (uint32_t)dec->data[dec->pos+2] << 8 | (uint32_t)dec->data[dec->pos+3]);
        dec->pos += 4;
        if (dec->pos + len > dec->size) return false;
        dec->pos += len; return true;
    }

    /* bin8/16/32 */
    if (type == 0xc4) {
        if (!mp_decoder_has_data(dec)) return false;
        uint8_t len = mp_decoder_read_byte(dec);
        if (dec->pos + len > dec->size) return false;
        dec->pos += len; return true;
    }
    if (type == 0xc5) {
        if (dec->pos + 2 > dec->size) return false;
        uint16_t len = (uint16_t)(dec->data[dec->pos] << 8 | dec->data[dec->pos+1]);
        dec->pos += 2;
        if (dec->pos + len > dec->size) return false;
        dec->pos += len; return true;
    }
    if (type == 0xc6) {
        if (dec->pos + 4 > dec->size) return false;
        uint32_t len = (uint32_t)((uint32_t)dec->data[dec->pos] << 24 | (uint32_t)dec->data[dec->pos+1] << 16 | (uint32_t)dec->data[dec->pos+2] << 8 | (uint32_t)dec->data[dec->pos+3]);
        dec->pos += 4;
        if (dec->pos + len > dec->size) return false;
        dec->pos += len; return true;
    }

    /* fixarray: 0x90-0x9f */
    if (type >= 0x90 && type <= 0x9f) {
        uint8_t count = type & 0x0f;
        for (uint8_t i = 0; i < count; i++) { if (!mp_decoder_skip_value(dec)) return false; }
        return true;
    }
    /* array16 */
    if (type == 0xdc) {
        if (dec->pos + 2 > dec->size) return false;
        uint16_t count = (uint16_t)(dec->data[dec->pos] << 8 | dec->data[dec->pos+1]);
        dec->pos += 2;
        for (uint16_t i = 0; i < count; i++) { if (!mp_decoder_skip_value(dec)) return false; }
        return true;
    }

    /* fixmap: 0x80-0x8f */
    if (type >= 0x80 && type <= 0x8f) {
        uint8_t count = type & 0x0f;
        for (uint8_t i = 0; i < count; i++) { if (!mp_decoder_skip_value(dec)) return false; if (!mp_decoder_skip_value(dec)) return false; }
        return true;
    }
    /* map16 */
    if (type == 0xde) {
        if (dec->pos + 2 > dec->size) return false;
        uint16_t count = (uint16_t)(dec->data[dec->pos] << 8 | dec->data[dec->pos+1]);
        dec->pos += 2;
        for (uint16_t i = 0; i < count; i++) { if (!mp_decoder_skip_value(dec)) return false; if (!mp_decoder_skip_value(dec)) return false; }
        return true;
    }

    /* 未知类型——无法安全跳过 */
    return false;
}

/* ================================================================== */
/*  module_save_to_binary / module_load_from_binary                    */
/* ================================================================== */

/*
 * 二进制格式结构 (MessagePack map):
 * {
 *     "name": string,
 *     "version": string,
 *     "dependencies": [
 *         {"name": string, "version_constraint": string}
 *     ],
 *     "exports": {
 *         "function_blocks": [int, ...],
 *         "type_regions": [int, ...]
 *     },
 *     "axiom_package": binary   // 嵌套的公理包二进制数据（暂存名称列表）
 * }
 */

ModuleSaveStatus module_save_to_binary(const Module *mod, uint8_t **out_data, size_t *out_size) {
    if (!mod || !out_data || !out_size) {
        lv_set_error(lv_ERROR_INVALID_PARAM, "module_save_to_binary: 无效参数");
        return MODULE_SAVE_WRITE_ERROR;
    }

    MsgPackEncoder enc;
    if (!mp_encoder_init(&enc, 1024)) {
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "module_save_to_binary: 内存分配失败");
        return MODULE_SAVE_WRITE_ERROR;
    }

    /* 顶层 map: 5 个键 */
    mp_encoder_write_map_header(&enc, 5);

    /* "name" */
    mp_encoder_write_str(&enc, "name");
    mp_encoder_write_str(&enc, mod->name ? mod->name : "");

    /* "version" */
    mp_encoder_write_str(&enc, "version");
    mp_encoder_write_str(&enc, mod->version ? mod->version : "");

    /* "dependencies" */
    mp_encoder_write_str(&enc, "dependencies");
    mp_encoder_write_array_header(&enc, (uint16_t)mod->dependency_count);
    for (int i = 0; i < mod->dependency_count; i++) {
        mp_encoder_write_map_header(&enc, 2);
        mp_encoder_write_str(&enc, "name");
        mp_encoder_write_str(&enc, mod->dependencies[i].name ? mod->dependencies[i].name : "");
        mp_encoder_write_str(&enc, "version_constraint");
        mp_encoder_write_str(&enc, mod->dependencies[i].version_constraint ? mod->dependencies[i].version_constraint : "");
    }

    /* "exports" */
    mp_encoder_write_str(&enc, "exports");
    mp_encoder_write_map_header(&enc, 2);
    /* function_blocks */
    mp_encoder_write_str(&enc, "function_blocks");
    mp_encoder_write_array_header(&enc, (uint16_t)(mod->exports ? mod->exports->function_count : 0));
    if (mod->exports) {
        for (int i = 0; i < mod->exports->function_count; i++) {
            mp_encoder_write_int(&enc, (int64_t)mod->exports->function_block_ids[i]);
        }
    }
    /* type_regions */
    mp_encoder_write_str(&enc, "type_regions");
    mp_encoder_write_array_header(&enc, (uint16_t)(mod->exports ? mod->exports->type_count : 0));
    if (mod->exports) {
        for (int i = 0; i < mod->exports->type_count; i++) {
            mp_encoder_write_int(&enc, (int64_t)mod->exports->type_region_ids[i]);
        }
    }

    /* "axiom_packages" - 存储公理包名称列表 */
    mp_encoder_write_str(&enc, "axiom_packages");
    mp_encoder_write_array_header(&enc, (uint16_t)mod->axiom_package_count);
    for (int i = 0; i < mod->axiom_package_count; i++) {
        if (mod->axiom_packages[i]) {
            mp_encoder_write_str(&enc, mod->axiom_packages[i]->name ? mod->axiom_packages[i]->name : "");
        } else {
            mp_encoder_write_str(&enc, "");
        }
    }

    *out_data = enc.buffer;
    *out_size = enc.pos;
    /* 注意：不调用 mp_encoder_destroy，因为 buffer 已转移给调用者 */
    if (enc.error) {
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "module_save_to_binary: 编码过程中内存不足");
        lv_free((void**)&enc.buffer);
        *out_data = NULL;
        *out_size = 0;
        return MODULE_SAVE_WRITE_ERROR;
    }
    return MODULE_SAVE_OK;
}

ModuleLoadStatus module_load_from_binary(const uint8_t *data, size_t size, Module **out_module) {
    if (!data || size == 0 || !out_module) {
        lv_set_error(lv_ERROR_INVALID_PARAM, "module_load_from_binary: 无效参数");
        return MODULE_LOAD_PARSE_ERROR;
    }

    MsgPackDecoder dec;
    if (!mp_decoder_init(&dec, data, size)) {
        lv_set_error(lv_ERROR_INVALID_PARAM, "module_load_from_binary: 无效数据");
        return MODULE_LOAD_PARSE_ERROR;
    }

    uint16_t map_count = 0;
    if (!mp_decoder_read_map_header(&dec, &map_count)) {
        lv_set_error(lv_ERROR_PARSE, "module_load_from_binary: 无法读取顶层 map");
        return MODULE_LOAD_PARSE_ERROR;
    }

    /* 临时变量 */
    char *name = NULL;
    char *version = NULL;
    Module *mod = NULL;

    for (uint16_t i = 0; i < map_count; i++) {
        char *key = NULL;
        if (!mp_decoder_read_str(&dec, &key)) {
            lv_set_error(lv_ERROR_PARSE, "module_load_from_binary: 无法读取 map 键");
            lv_free((void**)&name); lv_free((void**)&version);
            return MODULE_LOAD_PARSE_ERROR;
        }

        if (strcmp(key, "name") == 0) {
            lv_free((void**)&key);
            if (!mp_decoder_read_str(&dec, &name)) {
                lv_set_error(lv_ERROR_PARSE, "module_load_from_binary: 无法读取 name");
                lv_free((void**)&name); lv_free((void**)&version);
                if (mod) module_destroy(mod);
                return MODULE_LOAD_PARSE_ERROR;
            }
            /* 如果模块尚未创建且已有 name，立即创建 */
            if (!mod && name) {
                mod = module_create(name, version ? version : "0.0.0");
                if (!mod) {
                    lv_free((void**)&name); lv_free((void**)&version);
                    return MODULE_LOAD_PARSE_ERROR;
                }
            }
        }
        else if (strcmp(key, "version") == 0) {
            lv_free((void**)&key);
            if (!mp_decoder_read_str(&dec, &version)) {
                lv_set_error(lv_ERROR_PARSE, "module_load_from_binary: 无法读取 version");
                lv_free((void**)&name); lv_free((void**)&version);
                if (mod) module_destroy(mod);
                return MODULE_LOAD_PARSE_ERROR;
            }
            /* 如果模块已创建，更新版本 */
            if (mod && version) {
                lv_free((void**)&mod->version);
                mod->version = lv_strdup_safe(version);
            }
        }
        else if (strcmp(key, "dependencies") == 0) {
            lv_free((void**)&key);
            uint16_t dep_count = 0;
            if (!mp_decoder_read_array_header(&dec, &dep_count)) {
                lv_set_error(lv_ERROR_PARSE, "module_load_from_binary: 无法读取 dependencies 数组");
                lv_free((void**)&name); lv_free((void**)&version);
                if (mod) module_destroy(mod);
                return MODULE_LOAD_PARSE_ERROR;
            }
            for (uint16_t j = 0; j < dep_count; j++) {
                uint16_t dep_map_count = 0;
                if (!mp_decoder_read_map_header(&dec, &dep_map_count)) {
                    lv_free((void**)&name); lv_free((void**)&version);
                    if (mod) module_destroy(mod);
                    return MODULE_LOAD_PARSE_ERROR;
                }
                char *dep_name = NULL;
                char *dep_ver = NULL;
                for (uint16_t k = 0; k < dep_map_count; k++) {
                    char *dk = NULL;
                    if (!mp_decoder_read_str(&dec, &dk)) {
                        lv_free((void**)&name); lv_free((void**)&version); lv_free((void**)&dep_name); lv_free((void**)&dep_ver); lv_free((void**)&dk);
                        return MODULE_LOAD_PARSE_ERROR;
                    }
                    if (strcmp(dk, "name") == 0) {
                        lv_free((void**)&dk);
                        mp_decoder_read_str(&dec, &dep_name);
                    } else if (strcmp(dk, "version_constraint") == 0) {
                        lv_free((void**)&dk);
                        mp_decoder_read_str(&dec, &dep_ver);
                    } else {
                        lv_free((void**)&dk);
                        /* 跳过未知值 */
                        mp_decoder_skip_value(&dec);
                    }
                }
                if (mod && dep_name) {
                    module_add_dependency(mod, dep_name, dep_ver ? dep_ver : "");
                }
                lv_free((void**)&dep_name);
                lv_free((void**)&dep_ver);
            }
        }
        else if (strcmp(key, "exports") == 0) {
            lv_free((void**)&key);
            uint16_t exp_map_count = 0;
            if (!mp_decoder_read_map_header(&dec, &exp_map_count)) {
                lv_free((void**)&name); lv_free((void**)&version);
                if (mod) module_destroy(mod);
                return MODULE_LOAD_PARSE_ERROR;
            }
            for (uint16_t j = 0; j < exp_map_count; j++) {
                char *ek = NULL;
                if (!mp_decoder_read_str(&dec, &ek)) {
                    lv_free((void**)&name); lv_free((void**)&version);
                    if (mod) module_destroy(mod);
                    return MODULE_LOAD_PARSE_ERROR;
                }
                if (strcmp(ek, "function_blocks") == 0) {
                    lv_free((void**)&ek);
                    uint16_t fb_count = 0;
                    if (!mp_decoder_read_array_header(&dec, &fb_count)) {
                        lv_free((void**)&name); lv_free((void**)&version);
                        if (mod) module_destroy(mod);
                        return MODULE_LOAD_PARSE_ERROR;
                    }
                    for (uint16_t k = 0; k < fb_count; k++) {
                        int64_t val = 0;
                        if (mp_decoder_read_int(&dec, &val) && mod) {
                            module_export_function_block(mod, (int)val);
                        }
                    }
                }
                else if (strcmp(ek, "type_regions") == 0) {
                    lv_free((void**)&ek);
                    uint16_t tr_count = 0;
                    if (!mp_decoder_read_array_header(&dec, &tr_count)) {
                        lv_free((void**)&name); lv_free((void**)&version);
                        if (mod) module_destroy(mod);
                        return MODULE_LOAD_PARSE_ERROR;
                    }
                    for (uint16_t k = 0; k < tr_count; k++) {
                        int64_t val = 0;
                        if (mp_decoder_read_int(&dec, &val) && mod) {
                            module_export_type_region(mod, (int)val);
                        }
                    }
                }
                else {
                    lv_free((void**)&ek);
                    /* 跳过未知值 */
                    mp_decoder_skip_value(&dec);
                }
            }
        }
        else if (strcmp(key, "axiom_packages") == 0) {
            lv_free((void**)&key);
            uint16_t pkg_count = 0;
            if (!mp_decoder_read_array_header(&dec, &pkg_count)) {
                lv_free((void**)&name); lv_free((void**)&version);
                return MODULE_LOAD_PARSE_ERROR;
            }
            for (uint16_t j = 0; j < pkg_count; j++) {
                char *pkg_name = NULL;
                if (mp_decoder_read_str(&dec, &pkg_name) && mod && pkg_name) {
                    AxiomPackage *pkg = axiom_package_create(pkg_name, "0.0.0");
                    if (pkg) {
                        module_add_axiom_package(mod, pkg);
                    }
                }
                lv_free((void**)&pkg_name);
            }
        }
        else {
            lv_free((void**)&key);
            /* 跳过未知键的值 */
            mp_decoder_skip_value(&dec);
        }
    }

    /* 创建模块（如果尚未创建） */
    if (!mod && name) {
        mod = module_create(name, version ? version : "0.0.0");
    }

    if (!mod) {
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "module_load_from_binary: 无法创建模块");
        lv_free((void**)&name); lv_free((void**)&version);
        return MODULE_LOAD_PARSE_ERROR;
    }

    /* 确保模块有名称（map 键顺序不可依赖） */
    if (!mod->name) {
        mod->name = lv_strdup_safe("unnamed_module");
    }

    lv_free((void**)&name);
    lv_free((void**)&version);
    *out_module = mod;
    return MODULE_LOAD_OK;
}

/* ================================================================== */
/*  JSON 序列化 / 反序列化                                             */
/* ================================================================== */

/* JSON 写入器类型定义已提取至 module_helpers.h */

bool json_writer_init(JsonWriter *w, size_t initial_capacity) {
    w->buffer = (char *)lv_calloc(initial_capacity, 1);
    if (!w->buffer) return false;
    w->capacity = initial_capacity;
    w->pos = 0;
    w->buffer[0] = '\0';
    return true;
}

void json_writer_ensure(JsonWriter *w, size_t extra) {
    while (w->pos + extra >= w->capacity) {
        size_t new_cap = w->capacity * 2;
        char *new_buf = (char *)lv_realloc(w->buffer, new_cap);
        if (!new_buf) return;
        w->buffer = new_buf;
        w->capacity = new_cap;
    }
}

void json_writer_putc(JsonWriter *w, char c) {
    json_writer_ensure(w, 2);
    w->buffer[w->pos++] = c;
    w->buffer[w->pos] = '\0';
}

void json_writer_puts(JsonWriter *w, const char *s) {
    size_t len = strlen(s);
    json_writer_ensure(w, len + 1);
    memcpy(w->buffer + w->pos, s, len);
    w->pos += len;
    w->buffer[w->pos] = '\0';
}

/* 写入 JSON 转义字符串 */
void json_writer_write_escaped_str(JsonWriter *w, const char *s) {
    if (!s) {
        json_writer_puts(w, "null");
        return;
    }
    json_writer_putc(w, '"');
    for (; *s; s++) {
        switch (*s) {
            case '"':  json_writer_puts(w, "\\\""); break;
            case '\\': json_writer_puts(w, "\\\\"); break;
            case '\n': json_writer_puts(w, "\\n"); break;
            case '\r': json_writer_puts(w, "\\r"); break;
            case '\t': json_writer_puts(w, "\\t"); break;
            default:
                if ((unsigned char)*s < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)*s);
                    json_writer_puts(w, buf);
                } else {
                    json_writer_putc(w, *s);
                }
                break;
        }
    }
    json_writer_putc(w, '"');
}

void json_writer_destroy(JsonWriter *w) {
    lv_free((void**)&w->buffer);
    w->buffer = NULL;
}

char *module_serialize_to_json(const Module *mod) {
    if (!mod) return NULL;

    JsonWriter w;
    if (!json_writer_init(&w, 2048)) return NULL;

    json_writer_putc(&w, '{');

    /* name */
    json_writer_puts(&w, "\"name\":");
    json_writer_write_escaped_str(&w, mod->name);
    json_writer_putc(&w, ',');

    /* version */
    json_writer_puts(&w, "\"version\":");
    json_writer_write_escaped_str(&w, mod->version);
    json_writer_putc(&w, ',');

    /* dependencies */
    json_writer_puts(&w, "\"dependencies\":[");
    for (int i = 0; i < mod->dependency_count; i++) {
        if (i > 0) json_writer_putc(&w, ',');
        json_writer_putc(&w, '{');
        json_writer_puts(&w, "\"name\":");
        json_writer_write_escaped_str(&w, mod->dependencies[i].name);
        json_writer_putc(&w, ',');
        json_writer_puts(&w, "\"version_constraint\":");
        json_writer_write_escaped_str(&w, mod->dependencies[i].version_constraint);
        json_writer_putc(&w, '}');
    }
    json_writer_puts(&w, "],");

    /* exports */
    json_writer_puts(&w, "\"exports\":{");

    /* function_blocks */
    json_writer_puts(&w, "\"function_blocks\":[");
    if (mod->exports) {
        for (int i = 0; i < mod->exports->function_count; i++) {
            if (i > 0) json_writer_putc(&w, ',');
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", mod->exports->function_block_ids[i]);
            json_writer_puts(&w, buf);
        }
    }
    json_writer_puts(&w, "],");

    /* type_regions */
    json_writer_puts(&w, "\"type_regions\":[");
    if (mod->exports) {
        for (int i = 0; i < mod->exports->type_count; i++) {
            if (i > 0) json_writer_putc(&w, ',');
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", mod->exports->type_region_ids[i]);
            json_writer_puts(&w, buf);
        }
    }
    json_writer_puts(&w, "]");

    json_writer_putc(&w, '}');

    /* axiom_packages */
    json_writer_puts(&w, ",\"axiom_packages\":[");
    for (int i = 0; i < mod->axiom_package_count; i++) {
        if (i > 0) json_writer_putc(&w, ',');
        if (mod->axiom_packages[i]) {
            json_writer_write_escaped_str(&w, mod->axiom_packages[i]->name);
        } else {
            json_writer_puts(&w, "null");
        }
    }
    json_writer_puts(&w, "]");

    /* graph - 序列化约束图 */
    json_writer_puts(&w, ",\"graph\":");
    if (mod->graph) {
        char *graph_json = graph_serialize_to_json(mod->graph);
        if (graph_json) {
            json_writer_puts(&w, graph_json);
            lv_free((void**)&graph_json);
        } else {
            json_writer_puts(&w, "null");
        }
    } else {
        json_writer_puts(&w, "null");
    }

    json_writer_putc(&w, '}');

    /* 返回 buffer（调用者负责 free） */
    return w.buffer;
}

/* ---------- 图序列化支持函数 ---------- */

char *module_serialize_graph_to_json(const Module *mod) {
    if (!mod || !mod->graph) {
        lv_set_error(lv_ERROR_NULL_POINTER, "module_serialize_graph_to_json: 模块或图为空");
        return NULL;
    }
    return graph_serialize_to_json(mod->graph);
}

bool module_deserialize_graph_from_json(Module *mod, const char *json) {
    if (!mod || !json) {
        lv_set_error(lv_ERROR_INVALID_PARAM, "module_deserialize_graph_from_json: 无效参数");
        return false;
    }
    
    /* 销毁现有的图 */
    if (mod->graph) {
        graph_destroy(mod->graph);
        mod->graph = NULL;
    }
    
    /* 反序列化图 */
    ConstraintGraph *graph = graph_deserialize_from_json(json);
    if (!graph) {
        lv_set_error(lv_ERROR_PARSE, "module_deserialize_graph_from_json: 图反序列化失败");
        return false;
    }
    
    mod->graph = graph;
    return true;
}

/* ---------- JSON 解析器类型定义已提取至 module_helpers.h ---------- */

void json_reader_init(JsonReader *r, const char *data, size_t size) {
    r->data = data;
    r->size = size;
    r->pos = 0;
}

void json_reader_skip_whitespace(JsonReader *r) {
    while (r->pos < r->size) {
        char c = r->data[r->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            r->pos++;
        } else {
            break;
        }
    }
}

char json_reader_peek(JsonReader *r) {
    json_reader_skip_whitespace(r);
    return r->pos < r->size ? r->data[r->pos] : '\0';
}

char json_reader_next(JsonReader *r) {
    json_reader_skip_whitespace(r);
    return r->pos < r->size ? r->data[r->pos++] : '\0';
}

bool json_reader_expect_char(JsonReader *r, char c) {
    char got = json_reader_next(r);
    return got == c;
}

/* 读取 JSON 字符串（返回 malloc 分配的字符串） */
char *json_reader_read_string(JsonReader *r) {
    if (!json_reader_expect_char(r, '"')) return NULL;

    size_t start = r->pos;
    size_t len = 0;

    while (r->pos < r->size && r->data[r->pos] != '"') {
        if (r->data[r->pos] == '\\' && r->pos + 1 < r->size) {
            r->pos += 2;
            len++;
        } else {
            r->pos++;
            len++;
        }
    }

    if (r->pos >= r->size) return NULL;
    r->pos++; /* 跳过结束引号 */

    /* 解码转义字符 */
    char *result = (char *)lv_calloc(len + 1, 1);
    if (!result) return NULL;

    const char *src = r->data + start;
    char *dst = result;
    const char *end = r->data + r->pos - 1;

    while (src < end) {
        if (*src == '\\' && src + 1 < end) {
            src++;
            switch (*src) {
                case 'n': *dst++ = '\n'; break;
                case 'r': *dst++ = '\r'; break;
                case 't': *dst++ = '\t'; break;
                case '"': *dst++ = '"'; break;
                case '\\': *dst++ = '\\'; break;
                case '/': *dst++ = '/'; break;
                default: *dst++ = *src; break;
            }
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    return result;
}

/* 读取 JSON 整数 */
bool json_reader_read_int(JsonReader *r, int64_t *out) {
    json_reader_skip_whitespace(r);
    size_t start = r->pos;
    bool negative = false;

    if (r->pos < r->size && r->data[r->pos] == '-') {
        negative = true;
        r->pos++;
    }

    while (r->pos < r->size && r->data[r->pos] >= '0' && r->data[r->pos] <= '9') {
        r->pos++;
    }

    if (r->pos == start || (r->pos == start + 1 && negative)) return false;

    int64_t val = 0;
    for (size_t i = start + (negative ? 1 : 0); i < r->pos; i++) {
        val = val * 10 + (r->data[i] - '0');
    }
    *out = negative ? -val : val;
    return true;
}

/* 读取 JSON 数组长度（仅计数，不解析内容） */
int json_reader_count_array_elements(JsonReader *r) {
    if (!json_reader_expect_char(r, '[')) return -1;

    int count = 0;
    json_reader_skip_whitespace(r);
    if (json_reader_peek(r) == ']') {
        r->pos++;
        return 0;
    }

    /* 简单计数：通过跟踪括号/引号层级 */
    int depth = 1;
    while (r->pos < r->size && depth > 0) {
        char c = r->data[r->pos];
        if (c == '"') {
            /* 跳过字符串 */
            r->pos++;
            while (r->pos < r->size && r->data[r->pos] != '"') {
                if (r->data[r->pos] == '\\') r->pos++;
                r->pos++;
            }
            if (r->pos < r->size) r->pos++;
        } else if (c == '[' || c == '{') {
            depth++;
            r->pos++;
        } else if (c == ']' || c == '}') {
            depth--;
            if (c == ']' && depth == 0) {
                r->pos++;
                break;
            }
            r->pos++;
        } else if (c == ',') {
            if (depth == 1) count++;
            r->pos++;
        } else {
            r->pos++;
        }
    }
    return count + 1;
}

ModuleLoadStatus module_deserialize_from_json(const char *json, Module **out_module) {
    if (!json || !out_module) {
        lv_set_error(lv_ERROR_INVALID_PARAM, "module_deserialize_from_json: 无效参数");
        return MODULE_LOAD_PARSE_ERROR;
    }

    size_t json_len = strlen(json);
    JsonReader r;
    json_reader_init(&r, json, json_len);

    if (json_reader_peek(&r) != '{') {
        lv_set_error(lv_ERROR_PARSE, "module_deserialize_from_json: 期望 JSON 对象");
        return MODULE_LOAD_PARSE_ERROR;
    }
    r.pos++; /* 跳过 '{' */

    char *name = NULL;
    char *version = NULL;
    Module *mod = NULL;

    while (json_reader_peek(&r) != '}' && json_reader_peek(&r) != '\0') {
        /* 读取键 */
        char *key = json_reader_read_string(&r);
        if (!key) break;

        if (!json_reader_expect_char(&r, ':')) {
            lv_free((void**)&key);
            break;
        }

        if (strcmp(key, "name") == 0) {
            lv_free((void**)&name);
            name = json_reader_read_string(&r);
            /* 如果模块尚未创建且已有 name，立即创建 */
            if (!mod && name) {
                mod = module_create(name, version ? version : "0.0.0");
            }
        }
        else if (strcmp(key, "version") == 0) {
            lv_free((void**)&version);
            version = json_reader_read_string(&r);
            /* 如果模块已创建，更新版本 */
            if (mod && version) {
                lv_free((void**)&mod->version);
                mod->version = lv_strdup_safe(version);
            }
        }
        else if (strcmp(key, "dependencies") == 0) {
            /* 解析依赖数组 */
            if (json_reader_peek(&r) == '[') {
                r.pos++; /* 跳过 '[' */
                while (json_reader_peek(&r) != ']' && json_reader_peek(&r) != '\0') {
                    if (json_reader_peek(&r) == ',') { r.pos++; continue; }
                    if (json_reader_peek(&r) != '{') break;
                    r.pos++; /* 跳过 '{' */

                    char *dep_name = NULL;
                    char *dep_ver = NULL;

                    while (json_reader_peek(&r) != '}' && json_reader_peek(&r) != '\0') {
                        char *dk = json_reader_read_string(&r);
                        if (!dk) break;
                        if (!json_reader_expect_char(&r, ':')) { lv_free((void**)&dk); break; }

                        if (strcmp(dk, "name") == 0) {
                            lv_free((void**)&dep_name);
                            dep_name = json_reader_read_string(&r);
                        } else if (strcmp(dk, "version_constraint") == 0) {
                            lv_free((void**)&dep_ver);
                            dep_ver = json_reader_read_string(&r);
                        } else {
                            /* 跳过未知值 */
                            if (json_reader_peek(&r) == '"') {
                                char *tmp = json_reader_read_string(&r);
                                lv_free((void**)&tmp);
                            } else {
                                while (r.pos < r.size && json_reader_peek(&r) != ',' && json_reader_peek(&r) != '}') {
                                    r.pos++;
                                }
                            }
                        }
                        lv_free((void**)&dk);
                    }
                    if (json_reader_peek(&r) == '}') r.pos++;

                    if (mod && dep_name) {
                        module_add_dependency(mod, dep_name, dep_ver ? dep_ver : "");
                    }
                    lv_free((void**)&dep_name);
                    lv_free((void**)&dep_ver);
                }
                if (json_reader_peek(&r) == ']') r.pos++;
            }
        }
        else if (strcmp(key, "exports") == 0) {
            if (json_reader_peek(&r) == '{') {
                r.pos++; /* 跳过 '{' */

                while (json_reader_peek(&r) != '}' && json_reader_peek(&r) != '\0') {
                    char *ek = json_reader_read_string(&r);
                    if (!ek) break;
                    if (!json_reader_expect_char(&r, ':')) { lv_free((void**)&ek); break; }

                    if (strcmp(ek, "function_blocks") == 0 && mod) {
                        if (json_reader_peek(&r) == '[') {
                            r.pos++;
                            while (json_reader_peek(&r) != ']' && json_reader_peek(&r) != '\0') {
                                if (json_reader_peek(&r) == ',') { r.pos++; continue; }
                                int64_t val = 0;
                                if (json_reader_read_int(&r, &val)) {
                                    module_export_function_block(mod, (int)val);
                                } else {
                                    r.pos++;
                                }
                            }
                            if (json_reader_peek(&r) == ']') r.pos++;
                        }
                    }
                    else if (strcmp(ek, "type_regions") == 0 && mod) {
                        if (json_reader_peek(&r) == '[') {
                            r.pos++;
                            while (json_reader_peek(&r) != ']' && json_reader_peek(&r) != '\0') {
                                if (json_reader_peek(&r) == ',') { r.pos++; continue; }
                                int64_t val = 0;
                                if (json_reader_read_int(&r, &val)) {
                                    module_export_type_region(mod, (int)val);
                                } else {
                                    r.pos++;
                                }
                            }
                            if (json_reader_peek(&r) == ']') r.pos++;
                        }
                    }
                    else {
                        /* 跳过未知值 */
                        if (json_reader_peek(&r) == '"') {
                            char *tmp = json_reader_read_string(&r);
                            lv_free((void**)&tmp);
                        } else if (json_reader_peek(&r) == '[') {
                            int count = json_reader_count_array_elements(&r);
                            (void)count;
                        } else {
                            while (r.pos < r.size && json_reader_peek(&r) != ',' && json_reader_peek(&r) != '}') {
                                r.pos++;
                            }
                        }
                    }
                    lv_free((void**)&ek);
                }
                if (json_reader_peek(&r) == '}') r.pos++;
            }
        }
        else if (strcmp(key, "axiom_packages") == 0) {
            if (json_reader_peek(&r) == '[') {
                r.pos++;
                while (json_reader_peek(&r) != ']' && json_reader_peek(&r) != '\0') {
                    if (json_reader_peek(&r) == ',') { r.pos++; continue; }
                    char *pkg_name = json_reader_read_string(&r);
                    if (mod && pkg_name) {
                        AxiomPackage *pkg = axiom_package_create(pkg_name, "0.0.0");
                        if (pkg) {
                            module_add_axiom_package(mod, pkg);
                        }
                    }
                    lv_free((void**)&pkg_name);
                }
                if (json_reader_peek(&r) == ']') r.pos++;
            }
        }
        else if (strcmp(key, "graph") == 0) {
            /* 反序列化约束图 */
            if (json_reader_peek(&r) == '{') {
                /* 提取 graph 对象的字符串 */
                r.pos++; /* 跳过 '{' */
                size_t graph_start = r.pos;
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
                
                /* 创建 graph JSON 字符串的副本 */
                size_t graph_len = r.pos - graph_start - 1;
                char *graph_json = lv_calloc(graph_len + 1, 1);
                if (graph_json) {
                    /* 使用 memcpy 进行精确长度复制（已分配 graph_len+1，手动零终止更安全） */
                    memcpy(graph_json, r.data + graph_start, graph_len);
                    graph_json[graph_len] = '\0';
                    
                    /* 反序列化图 */
                    if (mod) {
                        ConstraintGraph *graph = graph_deserialize_from_json(graph_json);
                        if (graph) {
                            mod->graph = graph;
                        }
                    }
                    lv_free((void**)&graph_json);
                }
            } else if (json_reader_peek(&r) == 'n') {
                /* null - 跳过 "null" */
                r.pos += 4;
            }
        }
        else {
            /* 跳过未知键的值 */
            if (json_reader_peek(&r) == '"') {
                char *tmp = json_reader_read_string(&r);
                lv_free((void**)&tmp);
            } else if (json_reader_peek(&r) == '[') {
                int count = json_reader_count_array_elements(&r);
                (void)count;
            } else if (json_reader_peek(&r) == '{') {
                /* 跳过嵌套对象 */
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
                /* 跳过数字/布尔/null */
                while (r.pos < r.size && json_reader_peek(&r) != ',' && json_reader_peek(&r) != '}') {
                    r.pos++;
                }
            }
        }

        lv_free((void**)&key);

        if (json_reader_peek(&r) == ',') r.pos++;
    }

    /* 创建模块 */
    if (!mod && name) {
        mod = module_create(name, version ? version : "0.0.0");
    }

    if (!mod) {
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "module_deserialize_from_json: 无法创建模块");
        lv_free((void**)&name); lv_free((void**)&version);
        return MODULE_LOAD_PARSE_ERROR;
    }

    lv_free((void**)&name);
    lv_free((void**)&version);
    *out_module = mod;
    return MODULE_LOAD_OK;
}

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

static AutoSaveEntry g_autosave_entries[MAX_AUTOSAVE_ENTRIES];
static int g_autosave_entry_count = 0;

AutoSaveConfig *find_autosave_config(const char *module_name) {
    for (int i = 0; i < g_autosave_entry_count; i++) {
        if (strcmp(g_autosave_entries[i].module_name, module_name) == 0) {
            return &g_autosave_entries[i].config;
        }
    }
    return NULL;
}

AutoSaveConfig *get_or_create_autosave_config(const char *module_name) {
    AutoSaveConfig *existing = find_autosave_config(module_name);
    if (existing) return existing;

    if (g_autosave_entry_count >= MAX_AUTOSAVE_ENTRIES) return NULL;

    g_autosave_entries[g_autosave_entry_count].module_name = lv_strdup_safe(module_name);
    g_autosave_entries[g_autosave_entry_count].config.enabled = false;
    g_autosave_entries[g_autosave_entry_count].config.interval_seconds = 60;
    g_autosave_entries[g_autosave_entry_count].config.backup_directory = NULL;
    g_autosave_entries[g_autosave_entry_count].config.max_backups = 5;
    g_autosave_entry_count++;
    return &g_autosave_entries[g_autosave_entry_count - 1].config;
}

/* module_set_autosave_config 已在 module_delta.c 中实现 */
