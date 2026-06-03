/**
 * @file sample_plugin.c
 * @brief LV-00 插件示例
 *
 * 演示如何创建和使用 LV-00 插件
 *
 * @author Lv-00 Project
 * @version 1.0
 */

#include "lv00/plugin_system.h"
#include <stdio.h>
#include <string.h>

/* ============ 插件信息 ============ */

#define PLUGIN_NAME "sample_plugin"
#define PLUGIN_VERSION "1.0.0"
#define PLUGIN_AUTHOR "Lv-00 Project"
#define PLUGIN_DESCRIPTION "A sample plugin demonstrating the LV-00 plugin system"

/* ============ 插件生命周期回调 ============ */

/**
 * @brief 插件加载时调用
 * @param ctx 插件上下文
 * @return 0 成功，非0 失败
 */
int lv00_plugin_on_load(Lv00PluginContext* ctx) {
    printf("[%s] Plugin loaded successfully\n", PLUGIN_NAME);
    
    /* 初始化插件配置 */
    ctx->config = lv00_plugin_config_create();
    if (!ctx->config) {
        return -1;
    }
    
    /* 设置默认配置 */
    lv00_plugin_config_set(ctx->config, "enabled", "true", 3);
    lv00_plugin_config_set(ctx->config, "log_level", "info", 0);
    lv00_plugin_config_set(ctx->config, "max_iterations", "1000", 1);
    
    return 0;
}

/**
 * @brief 插件卸载时调用
 * @param ctx 插件上下文
 * @return 0 成功，非0 失败
 */
int lv00_plugin_on_unload(Lv00PluginContext* ctx) {
    printf("[%s] Plugin unloading...\n", PLUGIN_NAME);
    
    /* 清理配置 */
    if (ctx->config) {
        lv00_plugin_config_destroy(ctx->config);
        ctx->config = NULL;
    }
    
    printf("[%s] Plugin unloaded\n", PLUGIN_NAME);
    return 0;
}

/**
 * @brief 插件激活时调用
 * @param ctx 插件上下文
 * @return 0 成功，非0 失败
 */
int lv00_plugin_on_activate(Lv00PluginContext* ctx) {
    printf("[%s] Plugin activated\n", PLUGIN_NAME);
    
    /* 检查配置 */
    const char* enabled = lv00_plugin_config_get(ctx->config, "enabled", "false");
    if (strcmp(enabled, "true") != 0) {
        printf("[%s] Plugin is disabled in configuration\n", PLUGIN_NAME);
        return -1;
    }
    
    /* 注册接口 */
    Lv00PluginInterface* interface = (Lv00PluginInterface*)malloc(sizeof(Lv00PluginInterface));
    if (interface) {
        strncpy(interface->name, "sample_interface", sizeof(interface->name) - 1);
        interface->version = 1;
        interface->description = "Sample plugin interface";
        
        lv00_plugin_register_interface(ctx->plugin, interface);
    }
    
    return 0;
}

/**
 * @brief 插件停用时调用
 * @param ctx 插件上下文
 * @return 0 成功，非0 失败
 */
int lv00_plugin_on_deactivate(Lv00PluginContext* ctx) {
    printf("[%s] Plugin deactivated\n", PLUGIN_NAME);
    return 0;
}

/**
 * @brief 插件配置变更时调用
 * @param ctx 插件上下文
 * @param config 新配置
 * @return 0 成功，非0 失败
 */
int lv00_plugin_on_configure(Lv00PluginContext* ctx, const Lv00PluginConfig* config) {
    printf("[%s] Configuration updated\n", PLUGIN_NAME);
    
    /* 应用新配置 */
    if (ctx->config) {
        lv00_plugin_config_destroy(ctx->config);
    }
    ctx->config = lv00_plugin_config_create();
    
    /* 复制配置项 */
    for (size_t i = 0; i < config->entry_count; i++) {
        lv00_plugin_config_set(ctx->config, 
            config->entries[i].key,
            config->entries[i].value,
            config->entries[i].type);
    }
    
    return 0;
}

/**
 * @brief 插件事件处理
 * @param ctx 插件上下文
 * @param event 事件
 * @return 0 成功，非0 失败
 */
int lv00_plugin_on_event(Lv00PluginContext* ctx, const Lv00PluginEvent* event) {
    switch (event->type) {
        case LV00_PLUGIN_EVENT_LOAD:
            printf("[%s] Received LOAD event\n", PLUGIN_NAME);
            break;
        case LV00_PLUGIN_EVENT_UNLOAD:
            printf("[%s] Received UNLOAD event\n", PLUGIN_NAME);
            break;
        case LV00_PLUGIN_EVENT_ACTIVATE:
            printf("[%s] Received ACTIVATE event\n", PLUGIN_NAME);
            break;
        case LV00_PLUGIN_EVENT_DEACTIVATE:
            printf("[%s] Received DEACTIVATE event\n", PLUGIN_NAME);
            break;
        case LV00_PLUGIN_EVENT_MESSAGE:
            printf("[%s] Received MESSAGE event\n", PLUGIN_NAME);
            break;
        default:
            printf("[%s] Received unknown event type: %d\n", PLUGIN_NAME, event->type);
            break;
    }
    return 0;
}

/* ============ 插件入口 ============ */

LV00_PLUGIN_DECLARE(PLUGIN_NAME);

LV00_PLUGIN_ENTRY();

/* ============ 插件特定功能 ============ */

/**
 * @brief 示例：自定义几何运算
 */
static double sample_compute_area(double a, double b, double c) {
    /* 海伦公式计算三角形面积 */
    double s = (a + b + c) / 2.0;
    return sqrt(s * (s - a) * (s - b) * (s - c));
}

/**
 * @brief 示例：自定义验证函数
 */
static int sample_validate_triangle(double a, double b, double c) {
    return (a + b > c) && (b + c > a) && (c + a > b);
}

/**
 * @brief 示例：导出接口函数
 */
void* sample_get_function(const char* name) {
    if (strcmp(name, "compute_area") == 0) {
        return (void*)sample_compute_area;
    }
    if (strcmp(name, "validate_triangle") == 0) {
        return (void*)sample_validate_triangle;
    }
    return NULL;
}
