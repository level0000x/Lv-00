/**
 * @file high_dim.c
 * @brief 高维结构表示与交互模块实现
 *
 * @details 本模块实现四维及以上数学对象的表示和投影机制。
 *
 *          核心功能：
 *          - 高维块管理：注册/注销/查询高维抽象块（>= 4维）
 *          - 投影预设：管理多套投影映射方案，支持默认和自定义预设
 *          - 轴映射：定义各维度到二维平面的映射方式
 *            HIGH_DIM_MAP_TO_X / HIGH_DIM_MAP_TO_Y / HIGH_DIM_MAP_FOLD / HIGH_DIM_MAP_DISCARD
 *          - 坐标投影：将高维 SymbolicCoord 投影为 HighDimProjectedCoord
 *          - 二维变换：旋转和缩放变换矩阵组合到投影结果
 *          - 保真度计算：评估投影的信息保留程度
 *            （维度可见性 40% + 约束可见性 60% 加权平均）
 *          - 语义缩放：实现嵌套高维块内部的视角切换（深度栈管理）
 *
 *          关键数据结构：
 *          - HighDimAbstractBlock：高维抽象块（维度数、预设、保真度）
 *          - HighDimAxisMapping：单维度到二维的映射定义
 *          - HighDimProjectionPreset：完整的投影方案（映射列表+2D变换）
 *          - HighDimProjectedCoord：投影后的二维坐标结果
 *          - HighDimVisibilityStats：保真度统计结果
 *
 * @author Lv-00 Project
 * @version 3.0.1
 *
 * @dependencies
 *   - high_dim.h           : 高维模块公共接口定义
 *   - error_codes.h        : 错误码定义（lv_OK / lv_ERROR_*）
 *   - lv_utils.h         : 统一内存分配器和工具函数
 *   - lv_internal.h      : 内部数据结构与常量（M_PI 等）
 *   - stream.h             : 流式事件输出
 *   - constraint_graph.h   : 约束图接口（保真度计算依赖）
 */

#include "high_dim.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lv/config.h"
#include "lv/lv_json.h"
#include "lv/lv_parse_utils.h"

#include "debug.h" /* LOG_DEBUG, LOG_WARN, LOG_ERROR 等日志宏 */
#include "error_codes.h"
#include "lv_internal.h" /* M_PI, lv_SAFE_SNPRINTF 等内部宏 */
#include "lv/lv_str_utils.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"

/* ==================== 内部常量 ==================== */

/* 注：high_dim 模块无 setter 函数（变量被 high_dim_preset.c 等直接 extern 引用），
 * 不适用 LV_STREAM_CTX_DEFINE 宏，保留手写。 */
lv_THREAD_LOCAL StreamContext *high_dim_stream_ctx = NULL;

/**
 * 圆周率常量 π
 *
 * 改用 lv_internal.h 中统一定义的 M_PI，
 * 避免常量重复定义，确保全项目精度一致。
 * 值: 3.14159265358979323846
 */



