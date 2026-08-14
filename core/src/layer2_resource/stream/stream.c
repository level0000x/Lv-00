/**
 * @file stream.c
 * @brief 流式输出系统实现 —— 引擎事件回调与实时状态推送
 *
 * @details 提供流式事件发射、回调注册、事件过滤、JSON 序列化、事件统计等核心功能。
 *          支撑 Web 前端实时可视化和证明步骤动画渲染。
 *
 *          功能模块:
 *            - 生命周期管理: 创建/销毁流式上下文
 *            - 回调管理: 注册/注销回调，支持事件类型过滤掩码
 *            - 事件发射: 立即/缓冲/节流/惰性四种模式
 *            - 惰性求值: 消费者主动拉取模式，阈值自动刷新
 *            - 异步模式: 基于环形缓冲区的多线程消费者模式（互斥锁+条件变量）
 *            - JSON 序列化: 手工拼接 JSON / JSON-RPC 字符串
 *            - 事件统计: 按类型计数、总数、丢弃数
 *            - 工具函数: 时间戳、事件类型名称/颜色/标识符、过滤掩码解析
 *
 *          该文件为全量重构版本：原文件因编码损坏导致部分注释和逻辑丢失，
 *          于 2026-05-20 基于头文件声明和功能规格重新实现，并通过回归测试验证。
 *
 * @author Lv-00 Project
 * @version 3.3.0  (惰性求值完整实现 2026-05-23)
 *
 * @dependencies
 *   - stream.h              : 流式输出系统公共接口定义
 *   - lv_utils.h          : 统一内存分配器
 *
 * @note 本模块无外部依赖（除 lv_utils），仅依赖标准 C 库。
 *       所有平台相关代码通过 #ifdef 隔离（Windows: windows.h/timeGetTime/process.h，
 *       类 Unix: sys/time.h/strings.h/pthread.h）。异步模式使用平台原生线程原语。
 */

#include "lv/lv_platform.h"
#include "lv/lv_thread.h"
#include "lv/stream.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "lv/lv.h"
#include "lv/lv_utils.h"
#include "lv/lv_str_utils.h"

/* ── 平台线程支持 ── */
/* 使用统一的 lv/lv_thread.h 抽象 */

#include "stream_internal.h"
