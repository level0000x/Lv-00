/**
 * @file func_block_preset.c
 * @brief 预设函数块系统实现
 *
 * @details 实现完整的预设函数块库，包括：
 *          - 50+ 个内置预设函数块定义
 *          - 类型系统和约束验证
 *          - 实例化引擎
 *          - 文档生成
 *
 *          实现已按职责拆分为独立模块：
 *          - func_block_preset_data.c     内置预设元数据大表与全局状态
 *          - func_block_preset_internal.c 库生命周期、实例化与内部辅助函数
 *          - func_block_preset_query.c    预设查询、类型验证与枚举映射
 *          - func_block_preset_advanced.c 高级预设操作（组合、偏应用、逆、注册）
 *          - func_block_preset_doc.c      预设文档生成
 *
 *          各模块通过 func_block_preset_internal.h 共享内部定义。
 *
 * @version 5.0.0
 */

#include "func_block_preset.h"
#include "lv/func_block_internal.h"
#include "lv/lv_xmacro.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error_codes.h"
#include "lv_internal.h"
#include "lv_utils.h"
