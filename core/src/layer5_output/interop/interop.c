/**
 * @file interop.c
 * @brief 外部互操作模块实现
 *
 * @details 本模块实现与外部系统的互操作功能，包括：
 *          - 服务器管理（WebSocket/STDIO）
 *          - 命令解析与执行
 *          - 多格式导出（Coq/Lean/HTML/SVG/TikZ/GeoJSON）
 *          - 多格式导入（GeoGebra/GeoJSON/SVG）
 *          - 定理交换
 *
 * @note   部分高级功能（如WebSocket服务器、复杂格式导入）当前仅提供API框架，
 *         实际完整实现在UI层或需要额外依赖库支持。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/interop.h"

#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/constraint_graph.h"
#include "lv/engine.h"
#include "lv/proof.h"

#include "lv/error_codes.h"
#include "lv/lv_internal.h" /* lv_SAFE_SNPRINTF, M_PI 等内部宏 */
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream_context_util.h"
#include "lv/symbolic_coord.h"

#define MAX_RELATED_CONSTRAINTS 256

/* ==================== 安全整数解析辅助 ==================== */

/**
 * @brief 安全地将字符串解析为整数，替代不安全的 atoi()。
 *
 * atoi() 无法检测无效输入（如空指针、非数字字符、溢出），
 * 在解析外部 IPC 命令参数时可能导致未定义行为。
 * 本函数使用 strtol() 进行带错误检测的解析。
 *
 * @param str          待解析的字符串（可为 NULL）
 * @param default_val  解析失败时返回的默认值
 * @return 解析成功的整数值，或 default_val
 */

/* ── 子模块已拆分至 interop_server.c / interop_command.c / interop_export.c / interop_import.c / interop_theorem.c ── */
