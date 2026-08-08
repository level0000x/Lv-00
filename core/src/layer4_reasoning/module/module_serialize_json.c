/**
 * @file module_serialize_json.c
 * @brief JSON 序列化与反序列化
 *
 * @details 从 module_serialize.c 拆分的子模块（Lv-00 项目 v3.3.0+）。
 *
 * @todo 待迁移：module_deserialize_from_json（L471-542）、json_parse_dependencies
 *       （L251-280）、json_parse_exports（L302-363）仍为手写"parse_string →
 *       expect(':') → 值 → 逗号"对象字段遍历循环，与 lv/lv_json.h 新增的
 *       lv_json_parse_field 公共辅助同构；因各字段值解析容错（未知键跳过/
 *       嵌套对象逐字符扫描）互有差异，暂保留现状，后续统一迁移。
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
#include "lv/lv_json.h"

#include "lv/module.h"
#include "lv/module_internal.h"
#include "lv/sha256.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_str_utils.h"
#include "module_helpers.h"

/* ================================================================== */
/*  JSON 序列化 / 反序列化                                             */
/* ================================================================== */

/* 写入器统一使用公共库 lv/lv_json.h 的 lvJsonBuf（原 JsonWriter 已删除） */

/* 字段写处理器表由 X-macro 单一事实源 LV_MODULE_FIELD_X 生成：
 * 与 module_serialize.c（版本哈希）、module_serialize_msgpack.c 共享同一
 * 字段枚举。新增字段时补齐 json_write_<field> 即可（未补会编译期报错）。
 * 各 handler 使用 lvJsonBuf 对象级 API（begin_object/append_key/...），
 * 输出与历史手写版逐字节一致（紧凑模式无空白）。 */

typedef void (*ModuleJsonWriteFn)(lvJsonBuf *w, const Module *mod);

/* 前向声明：kModuleJsonWriters 表初始化器引用这些函数（静态初始化器须先声明） */
static void json_write_name(lvJsonBuf *w, const Module *mod);
static void json_write_version(lvJsonBuf *w, const Module *mod);
static void json_write_dependencies(lvJsonBuf *w, const Module *mod);
static void json_write_exports(lvJsonBuf *w, const Module *mod);
static void json_write_axiom_packages(lvJsonBuf *w, const Module *mod);
static void json_write_graph(lvJsonBuf *w, const Module *mod);

#define LV_MODULE_JSON_WRITE_ENTRY(field) json_write_##field,
static const ModuleJsonWriteFn kModuleJsonWriters[] = {
    LV_MODULE_FIELD_X(LV_MODULE_JSON_WRITE_ENTRY)
};

static void json_write_name(lvJsonBuf *w, const Module *mod) {
    lv_json_buf_append_key(w, "name");
    lv_json_buf_append_string(w, mod->name);
}

static void json_write_version(lvJsonBuf *w, const Module *mod) {
    lv_json_buf_append_key(w, "version");
    lv_json_buf_append_string(w, mod->version);
}

static void json_write_dependencies(lvJsonBuf *w, const Module *mod) {
    lv_json_buf_append_key(w, "dependencies");
    lv_json_buf_begin_array(w);
    for (int i = 0; i < mod->dependencies.count; i++) {
        ModuleDependency *dep = ((ModuleDependency *) mod->dependencies.data) + i;
        lv_json_buf_begin_object(w);
        lv_json_buf_append_key(w, "name");
        lv_json_buf_append_string(w, dep->name);
        lv_json_buf_append_key(w, "version_constraint");
        lv_json_buf_append_string(w, dep->version_constraint);
        lv_json_buf_end_object(w);
    }
    lv_json_buf_end_array(w);
}

static void json_write_exports(lvJsonBuf *w, const Module *mod) {
    lv_json_buf_append_key(w, "exports");
    lv_json_buf_begin_object(w);
    /* function_blocks */
    lv_json_buf_append_key(w, "function_blocks");
    lv_json_buf_begin_array(w);
    if (mod->exports) {
        for (int i = 0; i < mod->exports->function_block_ids.count; i++)
            lv_json_buf_append_int(w, (long long) ((int *) mod->exports->function_block_ids.data)[i]);
    }
    lv_json_buf_end_array(w);
    /* type_regions */
    lv_json_buf_append_key(w, "type_regions");
    lv_json_buf_begin_array(w);
    if (mod->exports) {
        for (int i = 0; i < mod->exports->type_region_ids.count; i++)
            lv_json_buf_append_int(w, (long long) ((int *) mod->exports->type_region_ids.data)[i]);
    }
    lv_json_buf_end_array(w);
    lv_json_buf_end_object(w);
}

static void json_write_axiom_packages(lvJsonBuf *w, const Module *mod) {
    lv_json_buf_append_key(w, "axiom_packages");
    lv_json_buf_begin_array(w);
    for (int i = 0; i < mod->axiom_packages.count; i++) {
        AxiomPackage *pkg = ((AxiomPackage **) mod->axiom_packages.data)[i];
        if (pkg)
            lv_json_buf_append_string(w, pkg->name);
        else
            lv_json_buf_append_null(w);
    }
    lv_json_buf_end_array(w);
}

static void json_write_graph(lvJsonBuf *w, const Module *mod) {
    lv_json_buf_append_key(w, "graph");
    if (mod->graph) {
        char *graph_json = graph_serialize_to_json(mod->graph);
        if (graph_json) {
            lv_json_buf_append_raw(w, graph_json);
            lv_free((void **) &graph_json);
        } else {
            lv_json_buf_append_null(w);
        }
    } else {
        lv_json_buf_append_null(w);
    }
}

char *module_serialize_to_json(const Module *mod) {
    if (!mod)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "module_serialize_to_json: mod is NULL");

    lvJsonBuf w;
    if (!lv_json_buf_init(&w, 2048))
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "module_serialize_to_json: lv_json_buf_init failed");

    /* 按 X-macro 字段清单顺序写全部字段（对象级 API 自动管理逗号/转义） */
    lv_json_buf_begin_object(&w);
    for (size_t i = 0; i < lv_ARRAY_SIZE(kModuleJsonWriters); i++)
        kModuleJsonWriters[i](&w, mod);
    lv_json_buf_end_object(&w);

    /* 返回 buffer（调用者负责 free） */
    return lv_json_buf_finalize(&w);
}

/* ---------- 图序列化支持函数 ---------- */

char *module_serialize_graph_to_json(const Module *mod) {
    if (!mod) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "module_serialize_graph_to_json: mod is NULL");
    }
    if (!mod->graph) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "module_serialize_graph_to_json: mod->graph is NULL");
    }
    return graph_serialize_to_json(mod->graph);
}

bool module_deserialize_graph_from_json(Module *mod, const char *json) {
    if (!mod) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "module_deserialize_graph_from_json: mod is NULL");
    }
    if (!json) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "module_deserialize_graph_from_json: json is NULL");
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

/* 字段解析处理器表由 X-macro 单一事实源 LV_MODULE_FIELD_X 生成（键名 = 字段名）。
 * 每个 handler 解析一个顶层键的 JSON 值；name/version 先收集到临时变量、
 * 模块延迟创建（键序不可依赖）；解析失败返回 false（已做内部清理），
 * 主循环停止解析（与历史 break 行为一致）。 */

typedef bool (*ModuleJsonParseFn)(lvJsonParser *p, Module **mod, char **name, char **version);

/* 前向声明：kModuleJsonParsers 表初始化器引用这些函数（静态初始化器须先声明） */
static bool json_parse_name(lvJsonParser *p, Module **mod, char **name, char **version);
static bool json_parse_version(lvJsonParser *p, Module **mod, char **name, char **version);
static bool json_parse_dependencies(lvJsonParser *p, Module **mod, char **name, char **version);
static bool json_parse_exports(lvJsonParser *p, Module **mod, char **name, char **version);
static bool json_parse_axiom_packages(lvJsonParser *p, Module **mod, char **name, char **version);
static bool json_parse_graph(lvJsonParser *p, Module **mod, char **name, char **version);

#define LV_MODULE_JSON_PARSE_ENTRY(field) { #field, json_parse_##field },
static const struct {
    const char *name;
    ModuleJsonParseFn fn;
} kModuleJsonParsers[] = {
    LV_MODULE_FIELD_X(LV_MODULE_JSON_PARSE_ENTRY)
};

static bool json_parse_name(lvJsonParser *p, Module **mod, char **name, char **version) {
    lv_free((void **) name);
    *name = lv_json_parse_string(p);
    /* 如果模块尚未创建且已有 name，立即创建 */
    if (!*mod && *name) {
        *mod = module_create(*name, *version ? *version : "0.0.0");
    }
    return true;
}

static bool json_parse_version(lvJsonParser *p, Module **mod, char **name, char **version) {
    (void) name;
    lv_free((void **) version);
    *version = lv_json_parse_string(p);
    /* 如果模块已创建，更新版本 */
    if (*mod && *version) {
        lv_free((void **) &(*mod)->version);
        (*mod)->version = lv_strdup_safe(*version);
    }
    return true;
}

static bool json_parse_dependencies(lvJsonParser *p, Module **mod, char **name, char **version) {
    (void) name;
    (void) version;
    /* 解析依赖数组 */
    if (lv_json_peek(p) == '[') {
        lv_json_next(p); /* 跳过 '[' */
        while (lv_json_peek(p) != ']' && lv_json_peek(p) != '\0') {
            if (lv_json_peek(p) == ',') {
                lv_json_next(p);
                continue;
            }
            if (lv_json_peek(p) != '{')
                break;
            lv_json_next(p); /* 跳过 '{' */

            char *dep_name = NULL;
            char *dep_ver = NULL;

            while (lv_json_peek(p) != '}' && lv_json_peek(p) != '\0') {
                char *dk = lv_json_parse_string(p);
                if (!dk)
                    break;
                if (!lv_json_expect(p, ':')) {
                    lv_free((void **) &dk);
                    break;
                }

                if (strcmp(dk, "name") == 0) {
                    lv_free((void **) &dep_name);
                    dep_name = lv_json_parse_string(p);
                } else if (strcmp(dk, "version_constraint") == 0) {
                    lv_free((void **) &dep_ver);
                    dep_ver = lv_json_parse_string(p);
                } else {
                    /* 跳过未知值 */
                    if (lv_json_peek(p) == '"') {
                        char *tmp = lv_json_parse_string(p);
                        lv_free((void **) &tmp);
                    } else {
                        while (lv_json_peek(p) != ',' && lv_json_peek(p) != '}' && lv_json_peek(p) != '\0') {
                            lv_json_next(p);
                        }
                    }
                }
                lv_free((void **) &dk);
                if (lv_json_peek(p) == ',')
                    lv_json_next(p);
            }
            if (lv_json_peek(p) == '}')
                lv_json_next(p);

            if (*mod && dep_name) {
                module_add_dependency(*mod, dep_name, dep_ver ? dep_ver : "");
            }
            lv_free((void **) &dep_name);
            lv_free((void **) &dep_ver);
        }
        if (lv_json_peek(p) == ']')
            lv_json_next(p);
    }
    return true;
}

static bool json_parse_exports(lvJsonParser *p, Module **mod, char **name, char **version) {
    (void) name;
    (void) version;
    if (lv_json_peek(p) == '{') {
        lv_json_next(p); /* 跳过 '{' */

        while (lv_json_peek(p) != '}' && lv_json_peek(p) != '\0') {
            char *ek = lv_json_parse_string(p);
            if (!ek)
                break;
            if (!lv_json_expect(p, ':')) {
                lv_free((void **) &ek);
                break;
            }

            if (strcmp(ek, "function_blocks") == 0 && *mod) {
                if (lv_json_peek(p) == '[') {
                    lv_json_next(p);
                    while (lv_json_peek(p) != ']' && lv_json_peek(p) != '\0') {
                        if (lv_json_peek(p) == ',') {
                            lv_json_next(p);
                            continue;
                        }
                        int64_t val = 0;
                        if (lv_json_parse_int64(p, &val)) {
                            module_export_function_block(*mod, (int) val);
                        } else {
                            lv_json_next(p);
                        }
                    }
                    if (lv_json_peek(p) == ']')
                        lv_json_next(p);
                }
            } else if (strcmp(ek, "type_regions") == 0 && *mod) {
                if (lv_json_peek(p) == '[') {
                    lv_json_next(p);
                    while (lv_json_peek(p) != ']' && lv_json_peek(p) != '\0') {
                        if (lv_json_peek(p) == ',') {
                            lv_json_next(p);
                            continue;
                        }
                        int64_t val = 0;
                        if (lv_json_parse_int64(p, &val)) {
                            module_export_type_region(*mod, (int) val);
                        } else {
                            lv_json_next(p);
                        }
                    }
                    if (lv_json_peek(p) == ']')
                        lv_json_next(p);
                }
            } else {
                /* 跳过未知值 */
                if (lv_json_peek(p) == '"') {
                    char *tmp = lv_json_parse_string(p);
                    lv_free((void **) &tmp);
                } else if (lv_json_peek(p) == '[') {
                    lv_json_skip_value(p);
                } else {
                    while (lv_json_peek(p) != ',' && lv_json_peek(p) != '}' && lv_json_peek(p) != '\0') {
                        lv_json_next(p);
                    }
                }
            }
            lv_free((void **) &ek);
            if (lv_json_peek(p) == ',')
                lv_json_next(p);
        }
        if (lv_json_peek(p) == '}')
            lv_json_next(p);
    }
    return true;
}

static bool json_parse_axiom_packages(lvJsonParser *p, Module **mod, char **name, char **version) {
    (void) name;
    (void) version;
    if (lv_json_peek(p) == '[') {
        lv_json_next(p);
        while (lv_json_peek(p) != ']' && lv_json_peek(p) != '\0') {
            if (lv_json_peek(p) == ',') {
                lv_json_next(p);
                continue;
            }
            char *pkg_name = lv_json_parse_string(p);
            if (*mod && pkg_name) {
                AxiomPackage *pkg = lv_axiom_package_create(pkg_name, "0.0.0");
                if (pkg) {
                    module_add_axiom_package(*mod, pkg);
                }
            }
            lv_free((void **) &pkg_name);
        }
        if (lv_json_peek(p) == ']')
            lv_json_next(p);
    }
    return true;
}

static bool json_parse_graph(lvJsonParser *p, Module **mod, char **name, char **version) {
    (void) name;
    (void) version;
    /* 反序列化约束图 */
    if (lv_json_peek(p) == '{') {
        /* 提取 graph 对象的字符串 */
        lv_json_next(p); /* 跳过 '{' */
        size_t graph_start = p->pos - 1; /* 包含外层 '{'（此前提取缺外层花括号导致图反序列化失败） */
        int depth = 1;
        /* 逐字符扫描（保留空白），直到深度归零；不能用 lv_json_next 替代（会跳过空白） */
        while (p->pos < p->size && depth > 0) {
            char c = p->data[p->pos];
            if (c == '"') {
                p->pos++;
                while (p->pos < p->size && p->data[p->pos] != '"') {
                    if (p->data[p->pos] == '\\')
                        p->pos++;
                    p->pos++;
                }
                if (p->pos < p->size)
                    p->pos++;
            } else if (c == '{') {
                depth++;
                p->pos++;
            } else if (c == '}') {
                depth--;
                p->pos++;
            } else {
                p->pos++;
            }
        }

        /* 创建 graph JSON 字符串的副本（含外层 '{' 与末尾 '}'，构成完整 JSON 对象） */
        size_t graph_len = p->pos - graph_start;
        char *graph_json = lv_calloc(graph_len + 1, 1);
        if (graph_json) {
            /* 使用 memcpy 进行精确长度复制（已分配 graph_len+1，手动零终止更安全） */
            memcpy(graph_json, p->data + graph_start, graph_len);
            graph_json[graph_len] = '\0';

            /* 反序列化图 */
            if (*mod) {
                ConstraintGraph *graph = graph_deserialize_from_json(graph_json);
                if (graph) {
                    (*mod)->graph = graph;
                }
            }
            lv_free((void **) &graph_json);
        }
    } else if (lv_json_peek(p) == 'n') {
        /* null - 跳过 "null" */
        p->pos += 4;
    }
    return true;
}

ModuleLoadStatus module_deserialize_from_json(const char *json, Module **out_module) {
    if (!json || !out_module) {
        lv_set_error(lv_ERROR_INVALID_PARAM, "module_deserialize_from_json: 无效参数");
        return MODULE_LOAD_PARSE_ERROR;
    }

    size_t json_len = strlen(json);
    lvJsonParser p;
    lv_json_parser_init(&p, json, json_len);

    if (lv_json_peek(&p) != '{') {
        lv_set_error(lv_ERROR_PARSE, "module_deserialize_from_json: 期望 JSON 对象");
        return MODULE_LOAD_PARSE_ERROR;
    }
    lv_json_next(&p); /* 跳过 '{' */

    char *name = NULL;
    char *version = NULL;
    Module *mod = NULL;

    while (lv_json_peek(&p) != '}' && lv_json_peek(&p) != '\0') {
        /* 读取键 */
        char *key = lv_json_parse_string(&p);
        if (!key)
            break;

        if (!lv_json_expect(&p, ':')) {
            lv_free((void **) &key);
            break;
        }

        /* 键分发：查 X-macro 生成的字段处理表（替代 6 分支 strcmp 链） */
        bool matched = false;
        bool parse_failed = false;
        for (size_t j = 0; j < lv_ARRAY_SIZE(kModuleJsonParsers); j++) {
            if (strcmp(key, kModuleJsonParsers[j].name) == 0) {
                matched = true;
                if (!kModuleJsonParsers[j].fn(&p, &mod, &name, &version)) {
                    parse_failed = true;
                }
                break;
            }
        }
        if (parse_failed) {
            lv_free((void **) &key);
            break; /* 解析失败：停止解析（与历史 break 行为一致） */
        }
        if (!matched) {
            /* 跳过未知键的值 */
            if (lv_json_peek(&p) == '"') {
                char *tmp = lv_json_parse_string(&p);
                lv_free((void **) &tmp);
            } else if (lv_json_peek(&p) == '[') {
                lv_json_skip_value(&p);
            } else if (lv_json_peek(&p) == '{') {
                /* 跳过嵌套对象（逐字符扫描，保留空白） */
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
                /* 跳过数字/布尔/null */
                while (lv_json_peek(&p) != ',' && lv_json_peek(&p) != '}' && lv_json_peek(&p) != '\0') {
                    lv_json_next(&p);
                }
            }
        }

        lv_free((void **) &key);

        if (lv_json_peek(&p) == ',')
            lv_json_next(&p);
    }

    /* 创建模块 */
    if (!mod && name) {
        mod = module_create(name, version ? version : "0.0.0");
    }

    if (!mod) {
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "module_deserialize_from_json: 无法创建模块");
        lv_free((void **) &name);
        lv_free((void **) &version);
        return MODULE_LOAD_PARSE_ERROR;
    }

    lv_free((void **) &name);
    lv_free((void **) &version);
    *out_module = mod;
    return MODULE_LOAD_OK;
}

