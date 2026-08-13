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
#include "lv/lv_str_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"
#include "lv_utils.h"

/* ==================== 命名常量 ==================== */

/* ============== 确定性状态持久化 ============== */

/**
 * @brief 辅助函数：将端口依赖类型转为字符串
 *
 * @param type 端口依赖类型枚举值
 * @return 对应的字符串表示
 */
/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief port_dep_type_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_port_dep_type_to_string_entries[] = {
    {"INCIDENCE", PORT_DEP_INCIDENCE},
    {"BETWEENNESS", PORT_DEP_BETWEENNESS},
    {"CONTAINMENT", PORT_DEP_CONTAINMENT},
    {"INTERSECTION", PORT_DEP_INTERSECTION},
};

static const char *port_dep_type_to_string(PortDependencyType type) {
    return lv_enum_to_str(s_port_dep_type_to_string_entries, lv_ARRAY_SIZE(s_port_dep_type_to_string_entries), (int) type, "UNKNOWN");
}

/** @brief view_state 序列化名称表（按枚举值升序：EXPANDED=0, COLLAPSED=1, PINNED=2；
 *  未命中回退 "EXPANDED" 与旧 switch default 分支语义一致） */
static const lvStrToEnumEntry s_view_state_to_string_entries[] = {
    {"EXPANDED", FB_VIEW_STATE_EXPANDED},
    {"COLLAPSED", FB_VIEW_STATE_COLLAPSED},
    {"PINNED", FB_VIEW_STATE_PINNED},
};

/**
 * @brief 辅助函数：从字符串解析端口依赖类型
 *
 * @param s 字符串
 * @return 对应的端口依赖类型枚举值
 */
static PortDependencyType port_dep_type_from_string(const char *s) {
    /* 查表反序列化（复用上方 s_port_dep_type_to_string_entries 表，
     * 替代 4 分支 strcmp 链；未命中回退 PORT_DEP_INCIDENCE 语义不变） */
    return (PortDependencyType) lv_str_to_enum(s_port_dep_type_to_string_entries,
                                               lv_ARRAY_SIZE(s_port_dep_type_to_string_entries), s,
                                               PORT_DEP_INCIDENCE);
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

    /* 使用 lvStrBuf 动态构建（自动扩容，替代手写 WRITE_FMT 宏 + 固定估算缓冲区） */
    lvStrBuf sb = {0};

    /* 头部：函数块ID和名称 */
    lv_strbuf_printf(&sb, "func_block {\n  id = %d\n", fb->id);

    if (fb->name) {
        lv_strbuf_printf(&sb, "  name = \"%s\"\n", fb->name);
    }
    if (fb->description) {
        lv_strbuf_printf(&sb, "  description = \"%s\"\n", fb->description);
    }

    /* 确定性状态 */
    lv_strbuf_printf(&sb, "  determinism = %s\n", determinism_state_to_string(fb->determinism));

    /* 视图状态（复用 s_view_state_to_string_entries 表，未命中回退 "EXPANDED" 与旧 switch default 一致） */
    lv_strbuf_printf(&sb, "  view_state = %s\n",
                     lv_enum_to_str(s_view_state_to_string_entries, lv_ARRAY_SIZE(s_view_state_to_string_entries),
                                    (int) fb->view_state, "EXPANDED"));

    /* 内部节点 */
    lv_strbuf_printf(&sb, "  internal_nodes = [");
    for (int i = 0; i < fb->internal_node_count; i++) {
        lv_strbuf_printf(&sb, "%s%d", (i > 0) ? ", " : "", fb->internal_node_ids[i]);
    }
    lv_strbuf_printf(&sb, "]\n");

    /* 输入端口 */
    lv_strbuf_printf(&sb, "  input_ports = [");
    for (int i = 0; i < fb->input_count; i++) {
        lv_strbuf_printf(&sb, "%s%d", (i > 0) ? ", " : "", fb->input_port_ids[i]);
    }
    lv_strbuf_printf(&sb, "]\n");

    /* 输出端口 */
    lv_strbuf_printf(&sb, "  output_ports = [");
    for (int i = 0; i < fb->output_count; i++) {
        lv_strbuf_printf(&sb, "%s%d", (i > 0) ? ", " : "", fb->output_port_ids[i]);
    }
    lv_strbuf_printf(&sb, "]\n");

    /* 选择器配置 */
    if (fb->selector) {
        lv_strbuf_printf(&sb, "  selector {\n    type = %d\n    reference_node_id = %d\n  }\n", (int) fb->selector->type,
                         fb->selector->reference_node_id);
    }

    /* 端口依赖 */
    for (int i = 0; i < fb->port_dep_count; i++) {
        PortDependency *dep = &fb->port_deps[i];
        lv_strbuf_printf(&sb,
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
        lv_strbuf_printf(&sb, "  preconditions = [");
        for (int i = 0; i < fb->precondition_count; i++) {
            lv_strbuf_printf(&sb, "%s%d", (i > 0) ? ", " : "", fb->precondition_region_ids[i]);
        }
        lv_strbuf_printf(&sb, "]\n");
    }

    /* 测度 */
    if (fb->has_measure) {
        lv_strbuf_printf(&sb, "  measure = { node_id = %d }\n", fb->measure_node_id);
    }

    lv_strbuf_printf(&sb, "}\n");

    return lv_strbuf_to_string(&sb);
}

/**
 * @brief 辅助函数：跳过空白字符
 *
 * @param p 当前解析指针
 * @return 指向第一个非空白字符的指针
 */
static const char *skip_whitespace(const char *p) {
    return lv_str_skip_ws(p);
}

/**
 * @brief 辅助函数：解析整数值
 *
 * @param p    当前解析指针
 * @param out  输出解析后的整数值
 * @return 指向解析结束位置的指针
 */
static const char *parse_int(const char *p, int *out) {
    /* 转发公共原语 lv_str_read_int（int64 溢出钳位，替代原无钳位手写累加） */
    int64_t v = 0;
    const char *next = p;
    (void) lv_str_read_int(&next, &v);
    *out = (int) v;
    return next;
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
    /* 转发公共原语 lv_str_read_quoted（失败时 out=NULL、指针停在空白后） */
    (void) lv_str_read_quoted(&p, out);
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

    /* 第一遍：用 lv_str_read_int 计数元素（支持负号，替代原仅数字的扫描） */
    int count = 0;
    const char *scan = p;
    while (*scan && *scan != ']') {
        const char *next = scan;
        int64_t v;
        if (lv_str_read_int(&next, &v)) {
            count++;
            scan = next;
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
            int64_t v = 0;
            const char *next = p;
            if (lv_str_read_int(&next, &v)) {
                p = next;
                arr[idx++] = (int) v;
            } else {
                /* '-' 后无数字：与旧 parse_int 一致按 0 计并前进（畸形输入兜底） */
                if (*p == '-')
                    p++;
                while (*p >= '0' && *p <= '9')
                    p++;
                arr[idx++] = 0;
            }
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

/* ── func_block 反序列化字段分发（查找表 + 复用 lv_str_match_delimited，替代手写 strcmp 链） ── */

/** @brief determinism 值→枚举 查找表（前缀匹配，替代 4 分支 strncmp 链） */
static const struct {
    const char *name;
    size_t len;
    DeterminismState state;
} kDeterminismValueTable[] = {
    {"VERIFIED", 8, DETERMINISM_VERIFIED},
    {"NON_DETERMINISTIC", 17, DETERMINISM_NON_DETERMINISTIC},
    {"PARTIALLY_VERIFIED", 19, DETERMINISM_PARTIALLY_VERIFIED},
    {"UNVERIFIED", 10, DETERMINISM_UNVERIFIED},
};

/** @brief view_state 值→枚举 查找表（前缀匹配，替代 2 分支 strncmp 链，未命中回退 EXPANDED） */
static const struct {
    const char *name;
    size_t len;
    FuncBlockViewState state;
} kViewStateValueTable[] = {
    {"COLLAPSED", 9, FB_VIEW_COLLAPSED},
    {"PINNED", 6, FB_VIEW_PINNED},
};

typedef void (*FuncBlockFieldHandler)(FuncBlock *fb, const char **pp);

static void fb_field_id(FuncBlock *fb, const char **pp) {
    const char *p = *pp;
    p = skip_whitespace(p);
    if (*p == '=') {
        p++;
        int val;
        p = parse_int(p, &val);
        fb->id = val;
    }
    *pp = p;
}

static void fb_field_name(FuncBlock *fb, const char **pp) {
    const char *p = *pp;
    p = skip_whitespace(p);
    if (*p == '=') {
        p++;
        char *name;
        p = parse_quoted_string(p, &name);
        lv_free((void **) &fb->name);
        fb->name = name;
    }
    *pp = p;
}

static void fb_field_description(FuncBlock *fb, const char **pp) {
    const char *p = *pp;
    p = skip_whitespace(p);
    if (*p == '=') {
        p++;
        char *desc;
        p = parse_quoted_string(p, &desc);
        lv_free((void **) &fb->description);
        fb->description = desc;
    }
    *pp = p;
}

static void fb_field_determinism(FuncBlock *fb, const char **pp) {
    const char *p = *pp;
    p = skip_whitespace(p);
    if (*p == '=') {
        p++;
        p = skip_whitespace(p);
        /* 值查表（替代 4 分支 strncmp 链） */
        bool matched = false;
        for (size_t i = 0; i < lv_ARRAY_SIZE(kDeterminismValueTable); i++) {
            if (strncmp(p, kDeterminismValueTable[i].name, kDeterminismValueTable[i].len) == 0) {
                fb->determinism = kDeterminismValueTable[i].state;
                p += kDeterminismValueTable[i].len;
                matched = true;
                break;
            }
        }
        if (!matched) {
            /* 跳过未知值到行尾 */
            p = lv_str_skip_until(p, "\n");
        }
    }
    *pp = p;
}

static void fb_field_view_state(FuncBlock *fb, const char **pp) {
    const char *p = *pp;
    p = skip_whitespace(p);
    if (*p == '=') {
        p++;
        p = skip_whitespace(p);
        /* 值查表（替代 2 分支 strncmp 链，未命中回退 EXPANDED） */
        bool matched = false;
        for (size_t i = 0; i < lv_ARRAY_SIZE(kViewStateValueTable); i++) {
            if (strncmp(p, kViewStateValueTable[i].name, kViewStateValueTable[i].len) == 0) {
                fb->view_state = kViewStateValueTable[i].state;
                p += kViewStateValueTable[i].len;
                matched = true;
                break;
            }
        }
        if (!matched) {
            fb->view_state = FB_VIEW_EXPANDED;
            p = lv_str_skip_until(p, "\n");
        }
    }
    *pp = p;
}

static void fb_field_internal_nodes(FuncBlock *fb, const char **pp) {
    const char *p = *pp;
    p = skip_whitespace(p);
    if (*p == '=') {
        p++;
        int *arr = NULL;
        int count = 0;
        p = parse_int_array(p, &arr, &count);
        func_block_set_internal_nodes(fb, arr, count);
        lv_free((void **) &arr);
    }
    *pp = p;
}

static void fb_field_input_ports(FuncBlock *fb, const char **pp) {
    const char *p = *pp;
    p = skip_whitespace(p);
    if (*p == '=') {
        p++;
        int *arr = NULL;
        int count = 0;
        p = parse_int_array(p, &arr, &count);
        func_block_set_input_ports(fb, arr, count);
        lv_free((void **) &arr);
    }
    *pp = p;
}

static void fb_field_output_ports(FuncBlock *fb, const char **pp) {
    const char *p = *pp;
    p = skip_whitespace(p);
    if (*p == '=') {
        p++;
        int *arr = NULL;
        int count = 0;
        p = parse_int_array(p, &arr, &count);
        func_block_set_output_ports(fb, arr, count);
        lv_free((void **) &arr);
    }
    *pp = p;
}

/* selector 嵌套字段关键字表 */
static const char *const kSelectorKeys[] = {"type", "reference_node_id", NULL};

static void fb_field_selector(FuncBlock *fb, const char **pp) {
    const char *p = *pp;
    p = skip_whitespace(p);
    if (*p == '{') {
        p++;
        int sel_type = 0;
        int ref_id = -1;
        while (*p && *p != '}') {
            p = skip_whitespace(p);
            /* 嵌套字段查表（复用 lv_str_match_delimited，替代 2 分支手写链） */
            int idx = lv_str_match_delimited(p, kSelectorKeys);
            if (idx == 0) { /* type */
                p += strlen("type");
                p = skip_whitespace(p);
                if (*p == '=') {
                    p++;
                    p = parse_int(p, &sel_type);
                }
            } else if (idx == 1) { /* reference_node_id */
                p += strlen("reference_node_id");
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
    *pp = p;
}

/* port_dep 嵌套字段关键字表 */
static const char *const kPortDepKeys[] = {"type", "port_id", "external_node_id", "internal_node_id", NULL};

static void fb_field_port_dep(FuncBlock *fb, const char **pp) {
    const char *p = *pp;
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
            /* 嵌套字段查表（复用 lv_str_match_delimited，替代 4 分支手写链） */
            int idx = lv_str_match_delimited(p, kPortDepKeys);
            if (idx == 0) { /* type */
                p += strlen("type");
                p = skip_whitespace(p);
                if (*p == '=') {
                    p++;
                    p = skip_whitespace(p);
                    /* 读取类型字符串 */
                    const char *start = p;
                    p = lv_str_skip_until(p, "\n }");
                    size_t len = (size_t) (p - start);
                    char *type_str = lv_malloc(len + 1);
                    if (type_str) {
                        lv_strlcpy_n(type_str, len + 1, start, (size_t) len);
                        dep.type = port_dep_type_from_string(type_str);
                        lv_free((void **) &type_str);
                    }
                }
            } else if (idx == 1) { /* port_id */
                p += strlen("port_id");
                p = skip_whitespace(p);
                if (*p == '=') {
                    p++;
                    p = parse_int(p, &dep.port_id);
                }
            } else if (idx == 2) { /* external_node_id */
                p += strlen("external_node_id");
                p = skip_whitespace(p);
                if (*p == '=') {
                    p++;
                    p = parse_int(p, &dep.external_node_id);
                }
            } else if (idx == 3) { /* internal_node_id */
                p += strlen("internal_node_id");
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
    *pp = p;
}

static void fb_field_preconditions(FuncBlock *fb, const char **pp) {
    const char *p = *pp;
    p = skip_whitespace(p);
    if (*p == '=') {
        p++;
        int *arr = NULL;
        int count = 0;
        p = parse_int_array(p, &arr, &count);
        func_block_set_preconditions(fb, arr, count);
        lv_free((void **) &arr);
    }
    *pp = p;
}

/* measure 嵌套字段关键字表 */
static const char *const kMeasureKeys[] = {"node_id", NULL};

static void fb_field_measure(FuncBlock *fb, const char **pp) {
    const char *p = *pp;
    p = skip_whitespace(p);
    if (*p == '{') {
        p++;
        int node_id = -1;
        while (*p && *p != '}') {
            p = skip_whitespace(p);
            /* 嵌套字段查表（复用 lv_str_match_delimited，替代单分支手写链） */
            if (lv_str_match_delimited(p, kMeasureKeys) == 0) { /* node_id */
                p += strlen("node_id");
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
    *pp = p;
}

/** @brief 函数块顶层字段关键字表（顺序与 kFuncBlockFieldTable 一致） */
static const char *const kFuncBlockKeys[] = {
    "id", "name", "description", "determinism", "view_state", "internal_nodes",
    "input_ports", "output_ports", "selector", "port_dep", "preconditions", "measure", NULL
};

/** @brief 函数块字段名→处理函数 查找表（替代 12 分支手写关键字匹配链） */
static const struct {
    const char *name;
    FuncBlockFieldHandler handler;
} kFuncBlockFieldTable[] = {
    {"id", fb_field_id},
    {"name", fb_field_name},
    {"description", fb_field_description},
    {"determinism", fb_field_determinism},
    {"view_state", fb_field_view_state},
    {"internal_nodes", fb_field_internal_nodes},
    {"input_ports", fb_field_input_ports},
    {"output_ports", fb_field_output_ports},
    {"selector", fb_field_selector},
    {"port_dep", fb_field_port_dep},
    {"preconditions", fb_field_preconditions},
    {"measure", fb_field_measure},
};

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

        /* 字段名→handler 查表（复用 lv_str_match_delimited 公共关键字匹配，替代 12 分支手写链） */
        int idx = lv_str_match_delimited(p, kFuncBlockKeys);
        if (idx >= 0) {
            p += strlen(kFuncBlockKeys[idx]);
            kFuncBlockFieldTable[idx].handler(fb, &p);
        } else {
            /* 跳过未知键到行尾 */
            p = lv_str_skip_until(p, "\n");
        }
    }

    return true;
}
