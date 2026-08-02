/**
 * @file plugin_system.c
 * @brief LV-00 模块化插件系统实现（容器文件）
 *
 * @details 实现插件加载、卸载、接口注册机制和插件间通信。
 *
 *          本文件已按功能域拆分为以下模块：
 *          - plugin_system_core.c       内部数据结构、辅助函数与生命周期管理
 *          - plugin_system_load.c       插件加载与卸载
 *          - plugin_system_state.c      插件激活与停用
 *          - plugin_system_query.c      插件查询
 *          - plugin_system_interface.c  接口注册与查询
 *          - plugin_system_config.c     插件配置
 *          - plugin_system_event.c      事件系统
 *          - plugin_system_deps.c       依赖管理
 *          - plugin_system_autoload.c   搜索路径管理与自动加载
 *          - plugin_system_version.c    版本兼容性、插件信息与错误处理
 *
 *          共享内部数据结构与辅助函数见 plugin_system_internal.h。
 *
 * @author Lv-00 Project
 * @version 1.0
 */

#include "lv/plugin_system.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_check.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_utils.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include "lv/lv_strbuf.h"
#endif

#include "plugin_system_internal.h"
