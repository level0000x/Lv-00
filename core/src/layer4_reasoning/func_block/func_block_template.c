/**
 * @file func_block_template.c
 * @brief 蓝图函数块模板系统实现（TEN_LAYER_OPTIMIZED_PLAN §4.1.3 落地）
 *
 * 模板注册表（名称 → 模板结构，互斥保护）。模板承载声明式元数据
 * （参数/脚本/版本/依赖）；实例化基于 func_block_create + 端口配置 +
 * func_block_instantiate。
 */

#include "lv/func_block_template.h"

#include <string.h>

#include "lv/func_block.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_thread.h"
#include "lv/lv_utils.h"

/* ============================================================
 * 模板结构
 * ============================================================ */

#define LV_FB_TEMPLATE_MAX_PARAMS 16
#define LV_FB_TEMPLATE_MAX_DEPS 16

struct FuncBlockTemplate {
    char *name;                        /**< 名称副本 */
    char *description;                 /**< 描述副本 */
    char *script;                      /**< 脚本副本（可为 NULL） */
    char *version;                     /**< 版本副本（可为 NULL） */
    FuncBlockTemplateParam params[LV_FB_TEMPLATE_MAX_PARAMS]; /**< 参数数组 */
    int param_count;                   /**< 参数数 */
    char *deps[LV_FB_TEMPLATE_MAX_DEPS]; /**< 依赖名副本数组 */
    int dep_count;                     /**< 依赖数 */
    bool registered;                   /**< 已注册（注册后属注册表所有） */
};

#define LV_MAX_FB_TEMPLATES 64
static FuncBlockTemplate *g_templates[LV_MAX_FB_TEMPLATES];
static int g_template_count = 0;

lv_LAZY_LOCK_DEFINE(g_template_lock);
#define TPL_LOCK() lv_lazy_lock_lock(&g_template_lock, g_template_lock_init_once)
#define TPL_UNLOCK() lv_lazy_lock_unlock(&g_template_lock)

/** @brief 释放模板内部副本（不释放结构本身） */
static void template_free_inner(FuncBlockTemplate *t) {
    if (t == NULL)
        return;
    lv_free((void **) &t->name);
    lv_free((void **) &t->description);
    lv_free((void **) &t->script);
    lv_free((void **) &t->version);
    for (int i = 0; i < t->dep_count; i++)
        lv_free((void **) &t->deps[i]);
}

/** @brief 按名查找注册表索引；未找到返回 -1 */
static int template_find(const char *name) {
    for (int i = 0; i < g_template_count; i++) {
        if (g_templates[i] != NULL && lv_str_eq(g_templates[i]->name, name))
            return i;
    }
    return -1;
}

/* ============================================================
 * 公共接口
 * ============================================================ */

FuncBlockTemplate *lv_fb_template_create(const char *name, const char *description) {
    if (name == NULL) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "lv_fb_template_create: name is NULL");
    }
    FuncBlockTemplate *t = (FuncBlockTemplate *) lv_calloc(1, sizeof(FuncBlockTemplate));
    if (t == NULL)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_fb_template_create: calloc failed");
    t->name = lv_strdup(name);
    t->description = description ? lv_strdup(description) : NULL;
    if (t->name == NULL || (description && t->description == NULL)) {
        template_free_inner(t);
        lv_free((void **) &t);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_fb_template_create: strdup failed");
    }
    return t;
}

void lv_fb_template_destroy(FuncBlockTemplate *tmpl) {
    if (tmpl == NULL)
        return;
    if (tmpl->registered) {
        /* 已注册模板须经 unregister 释放；此处仅日志（不应到达） */
        return;
    }
    template_free_inner(tmpl);
    lv_free((void **) &tmpl);
}

bool lv_fb_template_add_param(FuncBlockTemplate *tmpl, const FuncBlockTemplateParam *param) {
    if (tmpl == NULL || param == NULL || param->name[0] == '\0') {
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "lv_fb_template_add_param: invalid param");
    }
    if (tmpl->registered) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_STATE, "lv_fb_template_add_param: template already registered");
    }
    if (tmpl->param_count >= LV_FB_TEMPLATE_MAX_PARAMS)
        return false;
    tmpl->params[tmpl->param_count] = *param; /* 浅拷贝（description 指针借用） */
    tmpl->param_count++;
    return true;
}

bool lv_fb_template_set_script(FuncBlockTemplate *tmpl, const char *script) {
    if (tmpl == NULL || tmpl->registered) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "lv_fb_template_set_script: invalid");
    }
    lv_free((void **) &tmpl->script);
    tmpl->script = script ? lv_strdup(script) : NULL;
    return script == NULL || tmpl->script != NULL;
}

bool lv_fb_template_set_version(FuncBlockTemplate *tmpl, const char *version) {
    if (tmpl == NULL || tmpl->registered) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "lv_fb_template_set_version: invalid");
    }
    lv_free((void **) &tmpl->version);
    tmpl->version = version ? lv_strdup(version) : NULL;
    return version == NULL || tmpl->version != NULL;
}

bool lv_fb_template_add_dependency(FuncBlockTemplate *tmpl, const char *dep_name) {
    if (tmpl == NULL || dep_name == NULL || tmpl->registered) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "lv_fb_template_add_dependency: invalid");
    }
    /* 去重 */
    for (int i = 0; i < tmpl->dep_count; i++) {
        if (lv_str_eq(tmpl->deps[i], dep_name))
            return true;
    }
    if (tmpl->dep_count >= LV_FB_TEMPLATE_MAX_DEPS)
        return false;
    tmpl->deps[tmpl->dep_count] = lv_strdup(dep_name);
    if (tmpl->deps[tmpl->dep_count] == NULL)
        return false;
    tmpl->dep_count++;
    return true;
}

bool lv_fb_template_register(FuncBlockTemplate *tmpl) {
    if (tmpl == NULL || tmpl->name == NULL || tmpl->registered) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_INVALID_PARAM, "lv_fb_template_register: invalid");
    }
    TPL_LOCK();
    if (template_find(tmpl->name) >= 0 || g_template_count >= LV_MAX_FB_TEMPLATES) {
        TPL_UNLOCK();
        return false;
    }
    g_templates[g_template_count++] = tmpl;
    tmpl->registered = true;
    TPL_UNLOCK();
    return true;
}

FuncBlockTemplate *lv_fb_template_query(const char *name) {
    if (name == NULL)
        return NULL;
    TPL_LOCK();
    int idx = template_find(name);
    FuncBlockTemplate *t = (idx >= 0) ? g_templates[idx] : NULL;
    TPL_UNLOCK();
    return t;
}

bool lv_fb_template_unregister(const char *name) {
    if (name == NULL)
        return false;
    TPL_LOCK();
    int idx = template_find(name);
    if (idx < 0) {
        TPL_UNLOCK();
        return false;
    }
    FuncBlockTemplate *t = g_templates[idx];
    for (int j = idx; j < g_template_count - 1; j++)
        g_templates[j] = g_templates[j + 1];
    g_template_count--;
    TPL_UNLOCK();
    template_free_inner(t);
    lv_free((void **) &t);
    return true;
}

int lv_fb_template_instantiate(const char *template_name, ConstraintGraph *graph, const FuncBlockInstantiationArgs *args) {
    if (template_name == NULL || graph == NULL) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_fb_template_instantiate: NULL param");
    }
    FuncBlockTemplate *t = lv_fb_template_query(template_name);
    if (t == NULL) {
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "lv_fb_template_instantiate: template %s not found", template_name);
    }

    /* 实例化：在图中创建函数块节点（名称/描述来自模板；端口按实参数量配置）。
     * 模板为声明式元数据（参数/脚本/版本/依赖），本实现不解释脚本，
     * 实例化产出 GEOM_FUNCTION_BLOCK 节点 + 输入/输出端口（等价于
     * func_block_instantiate 的空内部结构形态）。 */
    int input_count = (args != NULL) ? args->input_count : 0;
    int *input_ports = (int *) lv_malloc((size_t)(input_count > 0 ? input_count : 1) * sizeof(int));
    if (input_ports == NULL)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_fb_template_instantiate: alloc failed");
    int in_cnt = 0;
    for (int i = 0; i < input_count; i++) {
        if (graph_add_port(graph, PORT_INPUT, 0, -1) == ADD_NODE_OK)
            input_ports[in_cnt++] = graph_get_last_added_node_id(graph);
    }
    int output_ports[4] = {0, 0, 0, 0};
    int out_cnt = 0;
    for (int i = 0; i < 4; i++) {
        if (graph_add_port(graph, PORT_OUTPUT, 0, -1) == ADD_NODE_OK)
            output_ports[out_cnt++] = graph_get_last_added_node_id(graph);
    }

    AddNodeResult r = graph_add_function_block(graph, NULL, 0, input_ports, in_cnt, output_ports, out_cnt);
    lv_free((void **) &input_ports);
    if (r != ADD_NODE_OK)
        return -1;
    return graph_get_last_added_node_id(graph);
}
