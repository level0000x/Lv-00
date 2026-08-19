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
#include "lv/lv_lifecycle.h"
#include "lv/lv_path.h"

#include "lv/module.h"
#include "lv/module_internal.h"
#include "lv/lv_hash.h"
#include "lv/lv_strbuf.h"


#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"
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

/* ── module_destroy 子资源销毁适配 ── */

/* ModuleDependency 元素：释放内部 name/version_constraint 字符串 */
static void destroy_module_dependency_elem(void *elem) {
    ModuleDependency *dep = (ModuleDependency *) elem;
    if (!dep)
        return;
    lv_free((void **) &dep->name);
    lv_free((void **) &dep->version_constraint);
}

/* ModuleExport 对象：释放两个 id 数组后释放外壳 */
static void destroy_module_exports(void *obj) {
    ModuleExport *exp = (ModuleExport *) obj;
    if (!exp)
        return;
    lv_darray_free(&exp->function_block_ids);
    lv_darray_free(&exp->type_region_ids);
    lv_free((void **) &exp);
}

/* axiom_packages 为指针数组：元素槽解引用后调用 axiom_package_destroy */
static void destroy_module_axiom_pkg_elem(void *elem) {
    AxiomPackage **slot = (AxiomPackage **) elem;
    if (slot && *slot)
        axiom_package_destroy(*slot);
}

LV_DESTROY_SHIM(destroy_module_graph, ConstraintGraph, graph_destroy)

/* module_destroy 字段描述表：释放顺序与原实现一致
 * （name → version → dependencies（逐元素） → exports（内部数组+外壳） →
 *   axiom_packages（逐元素） → graph → 外壳），全部置 NULL 安全 */
static const lvFieldDesc s_module_destroy_fields[] = {
    lv_FIELD_PLAIN(Module, name),
    lv_FIELD_PLAIN(Module, version),
    lv_FIELD_DARRAY_ELEMS(Module, dependencies, destroy_module_dependency_elem),
    lv_FIELD_OBJECT(Module, exports, destroy_module_exports),
    lv_FIELD_DARRAY_ELEMS(Module, axiom_packages, destroy_module_axiom_pkg_elem),
    lv_FIELD_OBJECT(Module, graph, destroy_module_graph),
};

void module_destroy(Module *mod) {
    if (!mod)
        return;
    lv_obj_destroy_fields(mod, s_module_destroy_fields,
                          sizeof(s_module_destroy_fields) / sizeof(s_module_destroy_fields[0]));
    lv_free((void **) &mod);
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
        if (lv_str_eq(loaded[i]->name, mod->name)) {
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

    /* 读取文件内容（统一走 lv_file_read_all；buf 已保证以 '\0' 结尾） */
    size_t len = 0;
    char *buf = (char *) lv_file_read_all(filepath, &len);
    if (!buf) {
        lv_set_error(lv_ERROR_IO, "无法读取文件: %s", filepath);
        /* 与原实现一致：打开失败 → FILE_NOT_FOUND，空文件/读取异常 → PARSE_ERROR */
        *status = lv_file_exists(filepath) ? MODULE_LOAD_PARSE_ERROR : MODULE_LOAD_FILE_NOT_FOUND;
        return false;
    }

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
            if (lv_str_eq(loaded[j]->name, dep->name)) {
                dep->module = loaded[j];
                dep_loaded = true;
                break;
            }
        }

        if (!dep_loaded) {
            /* 构建依赖文件路径：假设依赖文件在相同目录下，名称为 <dep_name>.lvz
             * 统一走 lv_path_dirname + lv_path_join（替换手写 strrchr/memcpy 样板） */
            char dep_path[1024];
            char dep_name_buf[520];
            lv_snprintf(dep_name_buf, sizeof(dep_name_buf), "%s.lvz", dep->name);

            size_t dir_len = 0;
            const char *dir_start = lv_path_dirname(filepath, &dir_len);
            if (dir_start) {
                char dir_buf[1024];
                size_t dlen = (dir_len < sizeof(dir_buf)) ? dir_len : (sizeof(dir_buf) - 1);
                lv_strlcpy_n(dir_buf, sizeof(dir_buf), dir_start, dlen);
                lv_path_join(dir_buf, dep_name_buf, dep_path, sizeof(dep_path));
            } else {
                lv_snprintf(dep_path, sizeof(dep_path), "%s.lvz", dep->name);
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
        /* 构建错误消息：报告循环路径（lvStrBuf 动态构建，消除固定 512 缓冲截断） */
        lvStrBuf msg;
        lv_strbuf_init(&msg);
        lv_strbuf_printf(&msg, "模块循环依赖: ");
        for (int i = 0; i < cycle_path_len; i++) {
            Module *m = loaded[cycle_path[i]];
            if (m) {
                lv_strbuf_printf(&msg, "%s", m->name);
                if (i < cycle_path_len - 1) {
                    lv_strbuf_printf(&msg, " → ");
                }
            }
        }
        char *msg_str = lv_strbuf_to_string(&msg);
        lv_set_error(lv_ERROR_INVALID_PARAM, "%s", msg_str ? msg_str : "");
        lv_free((void **) &msg_str);
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

/** @brief CoordType → 序列化前缀 查找表 */
static const struct {
    const char *prefix;
} kCoordPrefix[] = {
    [RATIONAL]       = {"rational "},
    [QUADRATIC]      = {"quadratic "},
    [ALGEBRAIC]      = {"algebraic "},
    [TRANSCENDENTAL] = {"transcendental "},
};

/* 将符号坐标序列化为字符串（调用者需释放返回的字符串） */
static char *serialize_symbolic_coord(const SymbolicCoord *coord) {
    if (!coord)
        return NULL;

    char *result = NULL;
    if ((unsigned) coord->type < lv_ARRAY_SIZE(kCoordPrefix)) {
        /* 使用 lv_asprintf 精确分配（消除固定余量估算与截断风险） */
        char *str = symbolic_coord_serialize(coord);
        if (str) {
            result = lv_asprintf("%s%s", kCoordPrefix[coord->type].prefix, str);
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

/* LVZ 文本格式写端。字段清单从 X-macro 单一事实源 LV_MODULE_FIELD_X 派生：
 * module/deps/exports/axioms 四节对应 name+version / dependencies / exports /
 * axiom_packages，graph 节由 serialize_constraint_graph 写出；本函数输出字节
 * 为既有 .lvz 文件格式，保持不变（module_lvz.c lvz_parse 为对应读端）。
 * 节式语法（每节关键字 + 计数）与 X-macro 的字段序不同，故保持手写而不
 * 以宏生成 handler 表；新增字段时在此补一节即可（字段枚举仍以 X-macro 为准）。 */
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

/* 版本哈希的字段处理表由 X-macro 单一事实源 LV_MODULE_FIELD_X 生成：
 * 字段清单（name/version/dependencies/exports/axiom_packages）与
 * module_serialize_json.c / module_serialize_msgpack.c 共享同一枚举，
 * 新增字段时补齐 hash_field_<field> 即可（未补会编译期报错）。 */

typedef void (*ModuleHashFieldFn)(lvHashCtx *ctx, const Module *mod);

/* 前向声明：kModuleHashFields 表初始化器引用这些函数（静态初始化器须先声明） */
static void hash_field_name(lvHashCtx *ctx, const Module *mod);
static void hash_field_version(lvHashCtx *ctx, const Module *mod);
static void hash_field_dependencies(lvHashCtx *ctx, const Module *mod);
static void hash_field_exports(lvHashCtx *ctx, const Module *mod);
static void hash_field_axiom_packages(lvHashCtx *ctx, const Module *mod);
static void hash_field_graph(lvHashCtx *ctx, const Module *mod);

#define LV_MODULE_HASH_ENTRY(field) hash_field_##field,
static const ModuleHashFieldFn kModuleHashFields[] = {
    LV_MODULE_FIELD_X(LV_MODULE_HASH_ENTRY)
};

static void hash_field_name(lvHashCtx *ctx, const Module *mod) {
    lv_hash_str(ctx, mod->name);
}

static void hash_field_version(lvHashCtx *ctx, const Module *mod) {
    lv_hash_str(ctx, mod->version);
}

static void hash_field_dependencies(lvHashCtx *ctx, const Module *mod) {
    lv_hash_int32(ctx, mod->dependencies.count);
    for (int i = 0; i < mod->dependencies.count; i++) {
        lv_hash_str(ctx, ((ModuleDependency *) mod->dependencies.data)[i].name);
        lv_hash_str(ctx, ((ModuleDependency *) mod->dependencies.data)[i].version_constraint);
    }
}

static void hash_field_exports(lvHashCtx *ctx, const Module *mod) {
    lv_hash_int32(ctx, mod->exports->function_block_ids.count);
    lv_hash_int32(ctx, mod->exports->type_region_ids.count);

    for (int i = 0; i < mod->exports->function_block_ids.count; i++) {
        lv_hash_int32(ctx, ((int *) mod->exports->function_block_ids.data)[i]);
    }

    for (int i = 0; i < mod->exports->type_region_ids.count; i++) {
        lv_hash_int32(ctx, ((int *) mod->exports->type_region_ids.data)[i]);
    }
}

static void hash_field_axiom_packages(lvHashCtx *ctx, const Module *mod) {
    lv_hash_int32(ctx, mod->axiom_packages.count);
    for (int i = 0; i < mod->axiom_packages.count; i++) {
        if (((AxiomPackage **) mod->axiom_packages.data)[i]) {
            lv_hash_str(ctx, ((AxiomPackage **) mod->axiom_packages.data)[i]->name);
            lv_hash_str(ctx, ((AxiomPackage **) mod->axiom_packages.data)[i]->version);
        }
    }
}

/* 版本哈希不含约束图（历史行为：graph 变化不参与 module_compute_version_hash） */
static void hash_field_graph(lvHashCtx *ctx, const Module *mod) {
    (void) ctx;
    (void) mod;
}

char *module_compute_version_hash(const Module *mod) {
    if (!mod)
        return NULL;

    lvHashCtx ctx;
    lv_hash_init(&ctx, LV_HASH_FNV1A);

    /* 按字段清单（X-macro 表序）逐一哈希，输入序列与历史实现完全一致 */
    for (size_t i = 0; i < lv_ARRAY_SIZE(kModuleHashFields); i++) {
        kModuleHashFields[i](&ctx, mod);
    }

    /* 转换为十六进制字符串 (64位 = 16个十六进制字符) */
    return lv_hash_to_hex_alloc(&ctx);
}

bool module_validate_dependency_chain(Module *mod, Module **all_modules, int module_count) {
    if (!mod) return false;
    for (int i = 0; i < mod->dependencies.count; i++) {
        bool found = false;
        for (int j = 0; j < module_count; j++) {
            if (lv_str_eq(all_modules[j]->name, ((ModuleDependency *) mod->dependencies.data)[i].name)) {
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
 *
 * 【lv_cycle_detect 收敛评估结论（不收敛，保留本实现）】
 *   lv_cycle_detect（lv_graph_traversal.h）仅通过 on_cycle 回调报告
 *   from_id → to_id 反向边，不提供当前 DFS 路径栈内容，无法重建
 *   module_load 错误消息所需的完整环路径（"A → B → C → A"）：
 *     1. 路径重建缺失：lvCycleFoundFunc 签名只含 from_id/to_id/edge_info，
 *        不含当前 DFS 路径；CONTINUE 逐环报告也无法拼接路径（回调间无
 *        共享路径上下文，且批次式邻居枚举与模块依赖的单批次语义不同）。
 *     2. 公开 API 承诺：module.h 中 module_full_cycle_detect 的
 *        out_path/out_path_len 参数显式要求输出环路径，迁移将破坏该语义。
 *     3. 三色标记逻辑等价：lv_cycle_detect 的 WHITE/GRAY/BLACK 语义与
 *        本实现一致，但本实现额外维护 path_stack 记录 DFS 路径。
 *   故保留手写三色 DFS；本次仅将固定 path_stack[MAX_MODULE_DEPTH] 改为
 *   按 count 动态分配，消除 MAX_MODULE_DEPTH=32 的模块数上限（路径栈
 *   深度不超过模块数 count）。若未来 lv_cycle_detect 增加路径输出回调
 *   （on_cycle 携带路径栈），可再收敛。
 */
bool module_full_cycle_detect(Module **modules, int count, int **out_path, int *out_path_len) {
    if (!modules)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "module_full_cycle_detect: modules is NULL");
    if (count <= 0)
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "module_full_cycle_detect: count <= 0");

    DFSColor *color_map = (DFSColor *) lv_calloc((size_t) count, sizeof(DFSColor));
    if (!color_map)
        return false;

    /* 路径栈：跟踪当前 DFS 路径上的模块索引（动态分配，按模块数定长，
     * 消除历史固定栈 path_stack[MAX_MODULE_DEPTH] 的 32 模块上限） */
    int *path_stack = (int *) lv_malloc((size_t) count * sizeof(int));
    if (!path_stack) {
        lv_free((void **) &color_map);
        return false;
    }
    int path_stack_len = 0;

    bool has_cycle = false;
    for (int i = 0; i < count && !has_cycle; i++) {
        if (color_map[i] == DFS_WHITE) {
            has_cycle = dfs_detect_cycle(modules[i], modules, count, color_map, path_stack, &path_stack_len, out_path,
                                         out_path_len);
        }
    }

    lv_free((void **) &path_stack);
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
    if (lv_str_startswith(constraint, ">=")) {
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
        lv_strlcpy_n(lower, lower_len + 1, constraint, lower_len);
        const char *upper = dash + 3;

        bool result = (module_compare_versions(version, lower) >= 0 && module_compare_versions(version, upper) <= 0);
        lv_free((void **) &lower);
        return result;
    }

    /* Default: exact match */
    return module_compare_versions(constraint, version) == 0;
}

