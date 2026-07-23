/**
 * @file func_block_serialize.c
 * @brief 函数块序列化/反序列化模块
 * @details 实现函数块状态的序列化（保存为文本格式）和反序列化（从文本格式恢复）。
 *          包含端口依赖类型转换、辅助解析函数等。
 *
 * @author Lv-00 Project
 * @version 3.0.1
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "func_block.h"
#include "lv_internal.h"
#include "lv_utils.h"

/* ==================== 命名常量 ==================== */

/** 序列化缓冲区的初始大小 */
#define SERIALIZE_BUFFER_INITIAL_SIZE 1024

/* ============== 确定性状态持久化 ============== */

/**
 * @brief 辅助函数：将端口依赖类型转为字符串
 *
 * @param type 端口依赖类型枚举值
 * @return 对应的字符串表示
 */
static const char *port_dep_type_to_string(PortDependencyType type) {
    switch (type) {
        case PORT_DEP_INCIDENCE:
            return "INCIDENCE";
        case PORT_DEP_BETWEENNESS:
            return "BETWEENNESS";
        case PORT_DEP_CONTAINMENT:
            return "CONTAINMENT";
        case PORT_DEP_INTERSECTION:
            return "INTERSECTION";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief 辅助函数：从字符串解析端口依赖类型
 *
 * @param s 字符串
 * @return 对应的端口依赖类型枚举值
 */
static PortDependencyType port_dep_type_from_string(const char *s) {
    if (strcmp(s, "INCIDENCE") == 0)
        return PORT_DEP_INCIDENCE;
    if (strcmp(s, "BETWEENNESS") == 0)
        return PORT_DEP_BETWEENNESS;
    if (strcmp(s, "CONTAINMENT") == 0)
        return PORT_DEP_CONTAINMENT;
    if (strcmp(s, "INTERSECTION") == 0)
        return PORT_DEP_INTERSECTION;
    return PORT_DEP_INCIDENCE;
}

/**
 * @brief 序列化函数块状态
 *
 * @param fb 函数块
 * @return 序列化后的字符串，调用方负责释放，失败返回 NULL
 */
char *func_block_serialize_state(const FuncBlock *fb) {
    if (!fb)
        return NULL;

    /* 估算缓冲区大小 */
    size_t buf_size = SERIALIZE_BUFFER_INITIAL_SIZE;
    if (fb->name)
        buf_size += strlen(fb->name);
    if (fb->description)
        buf_size += strlen(fb->description);
    buf_size += (size_t) (fb->internal_node_count + fb->input_count + fb->output_count + fb->port_dep_count +
                          fb->precondition_count) *
                32;

    char *buf = lv_malloc(buf_size);
    if (!buf)
        return NULL;
    buf[0] = '\0';
    size_t pos = 0;

/* 辅助宏：安全写入格式化字符串，防止缓冲区溢出 */
#define WRITE_FMT(fmt, ...)                                              \
    do {                                                                 \
        int w = snprintf(buf + pos, buf_size - pos, fmt, ##__VA_ARGS__); \
        if (w < 0)                                                       \
            goto done;                                                   \
        if ((size_t) w >= buf_size - pos) {                              \
            pos = buf_size - 1;                                          \
            goto done;                                                   \
        }                                                                \
        pos += (size_t) w;                                               \
    } while (0)

    /* 头部：函数块ID和名称 */
    WRITE_FMT("func_block {\n  id = %d\n", fb->id);

    if (fb->name) {
        WRITE_FMT("  name = \"%s\"\n", fb->name);
    }
    if (fb->description) {
        WRITE_FMT("  description = \"%s\"\n", fb->description);
    }

    /* 确定性状态 */
    WRITE_FMT("  determinism = %s\n", determinism_state_to_string(fb->determinism));

    /* 视图状态 */
    {
        const char *vs = "EXPANDED";
        switch (fb->view_state) {
            case FB_VIEW_COLLAPSED:
                vs = "COLLAPSED";
                break;
            case FB_VIEW_PINNED:
                vs = "PINNED";
                break;
            default:
                vs = "EXPANDED";
                break;
        }
        WRITE_FMT("  view_state = %s\n", vs);
    }

    /* 内部节点 */
    WRITE_FMT("  internal_nodes = [");
    for (int i = 0; i < fb->internal_node_count; i++) {
        WRITE_FMT("%s%d", i > 0 ? ", " : "", fb->internal_node_ids[i]);
    }
    WRITE_FMT("]\n");

    /* 输入端口 */
    WRITE_FMT("  input_ports = [");
    for (int i = 0; i < fb->input_count; i++) {
        WRITE_FMT("%s%d", i > 0 ? ", " : "", fb->input_port_ids[i]);
    }
    WRITE_FMT("]\n");

    /* 输出端口 */
    WRITE_FMT("  output_ports = [");
    for (int i = 0; i < fb->output_count; i++) {
        WRITE_FMT("%s%d", i > 0 ? ", " : "", fb->output_port_ids[i]);
    }
    WRITE_FMT("]\n");

    /* 选择器配置 */
    if (fb->selector) {
        WRITE_FMT("  selector {\n    type = %d\n    reference_node_id = %d\n  }\n", (int) fb->selector->type,
                  fb->selector->reference_node_id);
    }

    /* 端口依赖 */
    for (int i = 0; i < fb->port_dep_count; i++) {
        PortDependency *dep = &fb->port_deps[i];
        WRITE_FMT(
            "  port_dep {\n"
            "    type = %s\n"
            "    port_id = %d\n"
            "    external_node_id = %d\n"
            "    internal_node_id = %d\n"
            "  }\n",
            port_dep_type_to_string(dep->type), dep->port_id, dep->external_node_id, dep->internal_node_id);
    }

    /* 前置条件 */
    if (fb->precondition_count > 0) {
        WRITE_FMT("  preconditions = [");
        for (int i = 0; i < fb->precondition_count; i++) {
            WRITE_FMT("%s%d", i > 0 ? ", " : "", fb->precondition_region_ids[i]);
        }
        WRITE_FMT("]\n");
    }

    /* 测度 */
    if (fb->has_measure) {
        WRITE_FMT("  measure = { node_id = %d }\n", fb->measure_node_id);
    }

    WRITE_FMT("}\n");

#undef WRITE_FMT
done:
    buf[pos] = '\0';
    return buf;
}

/**
 * @brief 辅助函数：跳过空白字符
 *
 * @param p 当前解析指针
 * @return 指向第一个非空白字符的指针
 */
static const char *skip_whitespace(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    return p;
}

/**
 * @brief 辅助函数：解析整数值
 *
 * @param p    当前解析指针
 * @param out  输出解析后的整数值
 * @return 指向解析结束位置的指针
 */
static const char *parse_int(const char *p, int *out) {
    p = skip_whitespace(p);
    *out = 0;
    int sign = 1;
    if (*p == '-') {
        sign = -1;
        p++;
    }
    while (*p >= '0' && *p <= '9') {
        *out = *out * 10 + (*p - '0');
        p++;
    }
    *out *= sign;
    return p;
}

/**
 * @brief 辅助函数：解析带引号的字符串
 *
 * 从当前位置解析一个 "..." 格式的字符串。
 *
 * @param p   当前解析指针（应指向 opening quote）
 * @param out 输出解析后的字符串（调用者负责 free）
 * @return 指向结束引号后的指针
 */
static const char *parse_quoted_string(const char *p, char **out) {
    p = skip_whitespace(p);
    if (*p != '"') {
        *out = NULL;
        return p;
    }
    p++; /* 跳过开始引号 */
    const char *start = p;
    while (*p && *p != '"')
        p++;
    size_t len = (size_t) (p - start);
    *out = lv_malloc(len + 1);
    if (*out) {
        memcpy(*out, start, len);
        (*out)[len] = '\0';
    }
    if (*p == '"')
        p++; /* 跳过结束引号 */
    return p;
}

/**
 * @brief 辅助函数：解析整数数组 "[1, 2, 3]"
 *
 * @param p         当前解析指针（应指向 '['）
 * @param out       输出整数数组（调用者负责 free）
 * @param out_count 输出元素数量
 * @return 指向 ']' 之后的指针
 */
static const char *parse_int_array(const char *p, int **out, int *out_count) {
    p = skip_whitespace(p);
    if (*p != '[') {
        *out = NULL;
        *out_count = 0;
        return p;
    }
    p++; /* 跳过 '[' */

    /* 先计算元素数量 */
    int count = 0;
    const char *scan = p;
    while (*scan && *scan != ']') {
        if (*scan >= '0' && *scan <= '9') {
            count++;
            while (*scan >= '0' && *scan <= '9')
                scan++;
        } else {
            scan++;
        }
    }

    if (count == 0) {
        *out = NULL;
        *out_count = 0;
        if (*p == ']')
            p++;
        return p;
    }

    int *arr = lv_malloc((size_t) count * sizeof(int));
    if (!arr) {
        *out = NULL;
        *out_count = 0;
        return p;
    }

    int idx = 0;
    while (*p && *p != ']' && idx < count) {
        p = skip_whitespace(p);
        if (*p >= '0' || *p == '-') {
            int val = 0;
            int sign = 1;
            if (*p == '-') {
                sign = -1;
                p++;
            }
            while (*p >= '0' && *p <= '9') {
                val = val * 10 + (*p - '0');
                p++;
            }
            arr[idx++] = val * sign;
        } else {
            p++;
        }
    }
    if (*p == ']')
        p++;

    *out = arr;
    *out_count = idx;
    return p;
}

/**
 * @brief 反序列化函数块状态
 *
 * @param fb   函数块
 * @param data 序列化字符串
 * @return true  成功，false 失败
 */
bool func_block_deserialize_state(FuncBlock *fb, const char *data) {
    if (!fb || !data)
        return false;

    const char *p = data;

    /* 查找 "func_block {" */
    const char *header = strstr(p, "func_block");
    if (!header)
        return false;
    p = header + strlen("func_block");
    p = skip_whitespace(p);
    if (*p != '{')
        return false;
    p++; /* 跳过 '{' */

    while (*p && *p != '}') {
        p = skip_whitespace(p);
        if (*p == '}')
            break;

/* 辅助宏：检查字符位置是否为键值分隔符或结束符 */
#define IS_KEY_BOUNDARY(s, off) \
    ((s)[off] == ' ' || (s)[off] == '=' || (s)[off] == '\t' || (s)[off] == '\0' || (s)[off] == '\n' || (s)[off] == '{')

        /* 解析键名 */
        if (strncmp(p, "id", 2) == 0 && IS_KEY_BOUNDARY(p, 2)) {
            p += 2;
            p = skip_whitespace(p);
            if (*p == '=') {
                p++;
                int val;
                p = parse_int(p, &val);
                fb->id = val;
            }
        } else if (strncmp(p, "name", 4) == 0 && IS_KEY_BOUNDARY(p, 4)) {
            p += 4;
            p = skip_whitespace(p);
            if (*p == '=') {
                p++;
                char *name;
                p = parse_quoted_string(p, &name);
                lv_free((void **) &fb->name);
                fb->name = name;
            }
        } else if (strncmp(p, "description", 11) == 0 && IS_KEY_BOUNDARY(p, 11)) {
            p += 11;
            p = skip_whitespace(p);
            if (*p == '=') {
                p++;
                char *desc;
                p = parse_quoted_string(p, &desc);
                lv_free((void **) &fb->description);
                fb->description = desc;
            }
        } else if (strncmp(p, "determinism", 11) == 0 && IS_KEY_BOUNDARY(p, 11)) {
            p += 11;
            p = skip_whitespace(p);
            if (*p == '=') {
                p++;
                p = skip_whitespace(p);
                if (strncmp(p, "VERIFIED", 8) == 0) {
                    fb->determinism = DETERMINISM_VERIFIED;
                    p += 8;
                } else if (strncmp(p, "NON_DETERMINISTIC", 17) == 0) {
                    fb->determinism = DETERMINISM_NON_DETERMINISTIC;
                    p += 17;
                } else if (strncmp(p, "PARTIALLY_VERIFIED", 19) == 0) {
                    fb->determinism = DETERMINISM_PARTIALLY_VERIFIED;
                    p += 19;
                } else if (strncmp(p, "UNVERIFIED", 10) == 0) {
                    fb->determinism = DETERMINISM_UNVERIFIED;
                    p += 10;
                } else {
                    /* 跳过未知值到行尾 */
                    while (*p && *p != '\n')
                        p++;
                }
            }
        } else if (strncmp(p, "view_state", 10) == 0 && IS_KEY_BOUNDARY(p, 10)) {
            p += 10;
            p = skip_whitespace(p);
            if (*p == '=') {
                p++;
                p = skip_whitespace(p);
                if (strncmp(p, "COLLAPSED", 9) == 0) {
                    fb->view_state = FB_VIEW_COLLAPSED;
                    p += 9;
                } else if (strncmp(p, "PINNED", 6) == 0) {
                    fb->view_state = FB_VIEW_PINNED;
                    p += 6;
                } else {
                    fb->view_state = FB_VIEW_EXPANDED;
                    while (*p && *p != '\n')
                        p++;
                }
            }
        } else if (strncmp(p, "internal_nodes", 14) == 0 && IS_KEY_BOUNDARY(p, 14)) {
            p += 14;
            p = skip_whitespace(p);
            if (*p == '=') {
                p++;
                int *arr = NULL;
                int count = 0;
                p = parse_int_array(p, &arr, &count);
                func_block_set_internal_nodes(fb, arr, count);
                lv_free((void **) &arr);
            }
        } else if (strncmp(p, "input_ports", 11) == 0 && IS_KEY_BOUNDARY(p, 11)) {
            p += 11;
            p = skip_whitespace(p);
            if (*p == '=') {
                p++;
                int *arr = NULL;
                int count = 0;
                p = parse_int_array(p, &arr, &count);
                func_block_set_input_ports(fb, arr, count);
                lv_free((void **) &arr);
            }
        } else if (strncmp(p, "output_ports", 12) == 0 && IS_KEY_BOUNDARY(p, 12)) {
            p += 12;
            p = skip_whitespace(p);
            if (*p == '=') {
                p++;
                int *arr = NULL;
                int count = 0;
                p = parse_int_array(p, &arr, &count);
                func_block_set_output_ports(fb, arr, count);
                lv_free((void **) &arr);
            }
        } else if (strncmp(p, "selector", 8) == 0 && IS_KEY_BOUNDARY(p, 8)) {
            p += 8;
            p = skip_whitespace(p);
            if (*p == '{') {
                p++;
                int sel_type = 0;
                int ref_id = -1;
                while (*p && *p != '}') {
                    p = skip_whitespace(p);
                    if (strncmp(p, "type", 4) == 0 && IS_KEY_BOUNDARY(p, 4)) {
                        p += 4;
                        p = skip_whitespace(p);
                        if (*p == '=') {
                            p++;
                            p = parse_int(p, &sel_type);
                        }
                    } else if (strncmp(p, "reference_node_id", 18) == 0 && IS_KEY_BOUNDARY(p, 18)) {
                        p += 18;
                        p = skip_whitespace(p);
                        if (*p == '=') {
                            p++;
                            p = parse_int(p, &ref_id);
                        }
                    } else {
                        p++;
                    }
                }
                if (*p == '}')
                    p++;
                SolutionSelector *sel = selector_create_with_reference((SelectorType) sel_type, ref_id);
                if (sel)
                    func_block_set_selector(fb, sel);
            }
        } else if (strncmp(p, "port_dep", 8) == 0 && IS_KEY_BOUNDARY(p, 8)) {
            p += 8;
            p = skip_whitespace(p);
            if (*p == '{') {
                p++;
                PortDependency dep;
                memset(&dep, 0, sizeof(dep));
                dep.port_id = -1;
                dep.external_node_id = -1;
                dep.internal_node_id = -1;
                while (*p && *p != '}') {
                    p = skip_whitespace(p);
                    if (strncmp(p, "type", 4) == 0 && IS_KEY_BOUNDARY(p, 4)) {
                        p += 4;
                        p = skip_whitespace(p);
                        if (*p == '=') {
                            p++;
                            p = skip_whitespace(p);
                            /* 读取类型字符串 */
                            const char *start = p;
                            while (*p && *p != '\n' && *p != ' ' && *p != '}')
                                p++;
                            size_t len = (size_t) (p - start);
                            char *type_str = lv_malloc(len + 1);
                            if (type_str) {
                                memcpy(type_str, start, len);
                                type_str[len] = '\0';
                                dep.type = port_dep_type_from_string(type_str);
                                lv_free((void **) &type_str);
                            }
                        }
                    } else if (strncmp(p, "port_id", 7) == 0 && IS_KEY_BOUNDARY(p, 7)) {
                        p += 7;
                        p = skip_whitespace(p);
                        if (*p == '=') {
                            p++;
                            p = parse_int(p, &dep.port_id);
                        }
                    } else if (strncmp(p, "external_node_id", 17) == 0 && IS_KEY_BOUNDARY(p, 17)) {
                        p += 17;
                        p = skip_whitespace(p);
                        if (*p == '=') {
                            p++;
                            p = parse_int(p, &dep.external_node_id);
                        }
                    } else if (strncmp(p, "internal_node_id", 17) == 0 && IS_KEY_BOUNDARY(p, 17)) {
                        p += 17;
                        p = skip_whitespace(p);
                        if (*p == '=') {
                            p++;
                            p = parse_int(p, &dep.internal_node_id);
                        }
                    } else {
                        p++;
                    }
                }
                if (*p == '}')
                    p++;
                func_block_add_port_dependency(fb, &dep);
            }
        } else if (strncmp(p, "preconditions", 13) == 0 && IS_KEY_BOUNDARY(p, 13)) {
            p += 13;
            p = skip_whitespace(p);
            if (*p == '=') {
                p++;
                int *arr = NULL;
                int count = 0;
                p = parse_int_array(p, &arr, &count);
                func_block_set_preconditions(fb, arr, count);
                lv_free((void **) &arr);
            }
        } else if (strncmp(p, "measure", 7) == 0 && IS_KEY_BOUNDARY(p, 7)) {
            p += 7;
            p = skip_whitespace(p);
            if (*p == '{') {
                p++;
                int node_id = -1;
                while (*p && *p != '}') {
                    p = skip_whitespace(p);
                    if (strncmp(p, "node_id", 7) == 0 && IS_KEY_BOUNDARY(p, 7)) {
                        p += 7;
                        p = skip_whitespace(p);
                        if (*p == '=') {
                            p++;
                            p = parse_int(p, &node_id);
                        }
                    } else {
                        p++;
                    }
                }
                if (*p == '}')
                    p++;
                if (node_id >= 0) {
                    fb->has_measure = true;
                    fb->measure_node_id = node_id;
                }
            }
        } else {
            /* 跳过未知键到行尾 */
            while (*p && *p != '\n')
                p++;
        }
    }

#undef IS_KEY_BOUNDARY

    return true;
}
