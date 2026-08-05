/**
 * @file module_serialize_json.c
 * @brief JSON 序列化与反序列化
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

char *module_serialize_to_json(const Module *mod) {
    if (!mod)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "module_serialize_to_json: mod is NULL");

    lvJsonBuf w;
    if (!lv_json_buf_init(&w, 2048))
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "module_serialize_to_json: lv_json_buf_init failed");

    lv_json_buf_append_char(&w, '{');

    /* name */
    lv_json_buf_append_raw(&w, "\"name\":");
    lv_json_buf_append_string(&w, mod->name);
    lv_json_buf_append_char(&w, ',');

    /* version */
    lv_json_buf_append_raw(&w, "\"version\":");
    lv_json_buf_append_string(&w, mod->version);
    lv_json_buf_append_char(&w, ',');

    /* dependencies */
    lv_json_buf_append_raw(&w, "\"dependencies\":[");
    for (int i = 0; i < mod->dependencies.count; i++) {
        if (i > 0)
            lv_json_buf_append_char(&w, ',');
        lv_json_buf_append_char(&w, '{');
        lv_json_buf_append_raw(&w, "\"name\":");
        lv_json_buf_append_string(&w, ((ModuleDependency *) mod->dependencies.data)[i].name);
        lv_json_buf_append_char(&w, ',');
        lv_json_buf_append_raw(&w, "\"version_constraint\":");
        lv_json_buf_append_string(&w, ((ModuleDependency *) mod->dependencies.data)[i].version_constraint);
        lv_json_buf_append_char(&w, '}');
    }
    lv_json_buf_append_raw(&w, "],");

    /* exports */
    lv_json_buf_append_raw(&w, "\"exports\":{");

    /* function_blocks */
    lv_json_buf_append_raw(&w, "\"function_blocks\":[");
    if (mod->exports) {
        for (int i = 0; i < mod->exports->function_block_ids.count; i++) {
            if (i > 0)
                lv_json_buf_append_char(&w, ',');
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", ((int *) mod->exports->function_block_ids.data)[i]);
            lv_json_buf_append_raw(&w, buf);
        }
    }
    lv_json_buf_append_raw(&w, "],");

    /* type_regions */
    lv_json_buf_append_raw(&w, "\"type_regions\":[");
    if (mod->exports) {
        for (int i = 0; i < mod->exports->type_region_ids.count; i++) {
            if (i > 0)
                lv_json_buf_append_char(&w, ',');
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", ((int *) mod->exports->type_region_ids.data)[i]);
            lv_json_buf_append_raw(&w, buf);
        }
    }
    lv_json_buf_append_raw(&w, "]");

    lv_json_buf_append_char(&w, '}');

    /* axiom_packages */
    lv_json_buf_append_raw(&w, ",\"axiom_packages\":[");
    for (int i = 0; i < mod->axiom_packages.count; i++) {
        if (i > 0)
            lv_json_buf_append_char(&w, ',');
        if (((AxiomPackage **) mod->axiom_packages.data)[i]) {
            lv_json_buf_append_string(&w, ((AxiomPackage **) mod->axiom_packages.data)[i]->name);
        } else {
            lv_json_buf_append_raw(&w, "null");
        }
    }
    lv_json_buf_append_raw(&w, "]");

    /* graph - 序列化约束图 */
    lv_json_buf_append_raw(&w, ",\"graph\":");
    if (mod->graph) {
        char *graph_json = graph_serialize_to_json(mod->graph);
        if (graph_json) {
            lv_json_buf_append_raw(&w, graph_json);
            lv_free((void **) &graph_json);
        } else {
            lv_json_buf_append_raw(&w, "null");
        }
    } else {
        lv_json_buf_append_raw(&w, "null");
    }

    lv_json_buf_append_char(&w, '}');

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
    if (!json_reader_expect_char(r, '"'))
        return NULL;

    size_t start = r->pos;
    size_t raw_len = 0;

    /* 第一遍：定位结束引号，统计原始字节数（转义序列按原始长度计入） */
    while (r->pos < r->size && r->data[r->pos] != '"') {
        if (r->data[r->pos] == '\\' && r->pos + 1 < r->size) {
            r->pos += 2;
            raw_len += 2;
        } else {
            r->pos++;
            raw_len++;
        }
    }

    if (r->pos >= r->size)
        return NULL;
    r->pos++; /* 跳过结束引号 */

    /* 第二遍：公共反转义 API 计算解码后长度并分配 */
    size_t len = lv_str_json_unescape(r->data + start, raw_len, NULL, 0);
    char *result = (char *) lv_calloc(len + 1, 1);
    if (!result)
        return NULL;

    lv_str_json_unescape(r->data + start, raw_len, result, len + 1);
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

    if (r->pos == start || (r->pos == start + 1 && negative))
        return false;

    int64_t val = 0;
    for (size_t i = start + (negative ? 1 : 0); i < r->pos; i++) {
        val = val * 10 + (r->data[i] - '0');
    }
    *out = negative ? -val : val;
    return true;
}

/* 读取 JSON 数组长度（仅计数，不解析内容） */
int json_reader_count_array_elements(JsonReader *r) {
    if (!json_reader_expect_char(r, '['))
        return -1;

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
                if (r->data[r->pos] == '\\')
                    r->pos++;
                r->pos++;
            }
            if (r->pos < r->size)
                r->pos++;
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
            if (depth == 1)
                count++;
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
        if (!key)
            break;

        if (!json_reader_expect_char(&r, ':')) {
            lv_free((void **) &key);
            break;
        }

        if (strcmp(key, "name") == 0) {
            lv_free((void **) &name);
            name = json_reader_read_string(&r);
            /* 如果模块尚未创建且已有 name，立即创建 */
            if (!mod && name) {
                mod = module_create(name, version ? version : "0.0.0");
            }
        } else if (strcmp(key, "version") == 0) {
            lv_free((void **) &version);
            version = json_reader_read_string(&r);
            /* 如果模块已创建，更新版本 */
            if (mod && version) {
                lv_free((void **) &mod->version);
                mod->version = lv_strdup_safe(version);
            }
        } else if (strcmp(key, "dependencies") == 0) {
            /* 解析依赖数组 */
            if (json_reader_peek(&r) == '[') {
                r.pos++; /* 跳过 '[' */
                while (json_reader_peek(&r) != ']' && json_reader_peek(&r) != '\0') {
                    if (json_reader_peek(&r) == ',') {
                        r.pos++;
                        continue;
                    }
                    if (json_reader_peek(&r) != '{')
                        break;
                    r.pos++; /* 跳过 '{' */

                    char *dep_name = NULL;
                    char *dep_ver = NULL;

                    while (json_reader_peek(&r) != '}' && json_reader_peek(&r) != '\0') {
                        char *dk = json_reader_read_string(&r);
                        if (!dk)
                            break;
                        if (!json_reader_expect_char(&r, ':')) {
                            lv_free((void **) &dk);
                            break;
                        }

                        if (strcmp(dk, "name") == 0) {
                            lv_free((void **) &dep_name);
                            dep_name = json_reader_read_string(&r);
                        } else if (strcmp(dk, "version_constraint") == 0) {
                            lv_free((void **) &dep_ver);
                            dep_ver = json_reader_read_string(&r);
                        } else {
                            /* 跳过未知值 */
                            if (json_reader_peek(&r) == '"') {
                                char *tmp = json_reader_read_string(&r);
                                lv_free((void **) &tmp);
                            } else {
                                while (r.pos < r.size && json_reader_peek(&r) != ',' && json_reader_peek(&r) != '}') {
                                    r.pos++;
                                }
                            }
                        }
                        lv_free((void **) &dk);
                    }
                    if (json_reader_peek(&r) == '}')
                        r.pos++;

                    if (mod && dep_name) {
                        module_add_dependency(mod, dep_name, dep_ver ? dep_ver : "");
                    }
                    lv_free((void **) &dep_name);
                    lv_free((void **) &dep_ver);
                }
                if (json_reader_peek(&r) == ']')
                    r.pos++;
            }
        } else if (strcmp(key, "exports") == 0) {
            if (json_reader_peek(&r) == '{') {
                r.pos++; /* 跳过 '{' */

                while (json_reader_peek(&r) != '}' && json_reader_peek(&r) != '\0') {
                    char *ek = json_reader_read_string(&r);
                    if (!ek)
                        break;
                    if (!json_reader_expect_char(&r, ':')) {
                        lv_free((void **) &ek);
                        break;
                    }

                    if (strcmp(ek, "function_blocks") == 0 && mod) {
                        if (json_reader_peek(&r) == '[') {
                            r.pos++;
                            while (json_reader_peek(&r) != ']' && json_reader_peek(&r) != '\0') {
                                if (json_reader_peek(&r) == ',') {
                                    r.pos++;
                                    continue;
                                }
                                int64_t val = 0;
                                if (json_reader_read_int(&r, &val)) {
                                    module_export_function_block(mod, (int) val);
                                } else {
                                    r.pos++;
                                }
                            }
                            if (json_reader_peek(&r) == ']')
                                r.pos++;
                        }
                    } else if (strcmp(ek, "type_regions") == 0 && mod) {
                        if (json_reader_peek(&r) == '[') {
                            r.pos++;
                            while (json_reader_peek(&r) != ']' && json_reader_peek(&r) != '\0') {
                                if (json_reader_peek(&r) == ',') {
                                    r.pos++;
                                    continue;
                                }
                                int64_t val = 0;
                                if (json_reader_read_int(&r, &val)) {
                                    module_export_type_region(mod, (int) val);
                                } else {
                                    r.pos++;
                                }
                            }
                            if (json_reader_peek(&r) == ']')
                                r.pos++;
                        }
                    } else {
                        /* 跳过未知值 */
                        if (json_reader_peek(&r) == '"') {
                            char *tmp = json_reader_read_string(&r);
                            lv_free((void **) &tmp);
                        } else if (json_reader_peek(&r) == '[') {
                            int count = json_reader_count_array_elements(&r);
                            (void) count;
                        } else {
                            while (r.pos < r.size && json_reader_peek(&r) != ',' && json_reader_peek(&r) != '}') {
                                r.pos++;
                            }
                        }
                    }
                    lv_free((void **) &ek);
                }
                if (json_reader_peek(&r) == '}')
                    r.pos++;
            }
        } else if (strcmp(key, "axiom_packages") == 0) {
            if (json_reader_peek(&r) == '[') {
                r.pos++;
                while (json_reader_peek(&r) != ']' && json_reader_peek(&r) != '\0') {
                    if (json_reader_peek(&r) == ',') {
                        r.pos++;
                        continue;
                    }
                    char *pkg_name = json_reader_read_string(&r);
                    if (mod && pkg_name) {
                        AxiomPackage *pkg = lv_axiom_package_create(pkg_name, "0.0.0");
                        if (pkg) {
                            module_add_axiom_package(mod, pkg);
                        }
                    }
                    lv_free((void **) &pkg_name);
                }
                if (json_reader_peek(&r) == ']')
                    r.pos++;
            }
        } else if (strcmp(key, "graph") == 0) {
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
                            if (r.data[r.pos] == '\\')
                                r.pos++;
                            r.pos++;
                        }
                        if (r.pos < r.size)
                            r.pos++;
                    } else if (c == '{') {
                        depth++;
                        r.pos++;
                    } else if (c == '}') {
                        depth--;
                        r.pos++;
                    } else {
                        r.pos++;
                    }
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
                    lv_free((void **) &graph_json);
                }
            } else if (json_reader_peek(&r) == 'n') {
                /* null - 跳过 "null" */
                r.pos += 4;
            }
        } else {
            /* 跳过未知键的值 */
            if (json_reader_peek(&r) == '"') {
                char *tmp = json_reader_read_string(&r);
                lv_free((void **) &tmp);
            } else if (json_reader_peek(&r) == '[') {
                int count = json_reader_count_array_elements(&r);
                (void) count;
            } else if (json_reader_peek(&r) == '{') {
                /* 跳过嵌套对象 */
                r.pos++;
                int depth = 1;
                while (r.pos < r.size && depth > 0) {
                    char c = r.data[r.pos];
                    if (c == '"') {
                        r.pos++;
                        while (r.pos < r.size && r.data[r.pos] != '"') {
                            if (r.data[r.pos] == '\\')
                                r.pos++;
                            r.pos++;
                        }
                        if (r.pos < r.size)
                            r.pos++;
                    } else if (c == '{') {
                        depth++;
                        r.pos++;
                    } else if (c == '}') {
                        depth--;
                        r.pos++;
                    } else {
                        r.pos++;
                    }
                }
            } else {
                /* 跳过数字/布尔/null */
                while (r.pos < r.size && json_reader_peek(&r) != ',' && json_reader_peek(&r) != '}') {
                    r.pos++;
                }
            }
        }

        lv_free((void **) &key);

        if (json_reader_peek(&r) == ',')
            r.pos++;
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

