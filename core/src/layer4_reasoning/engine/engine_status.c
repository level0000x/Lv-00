/**
 * @file engine_status.c
 * @brief 引擎状态码映射函数（纯函数，无引擎内部状态依赖）
 *
 * @details 从 engine.c 中提取，减少主引擎文件的耦合度。
 *          提供 EngineStatus 枚举值的字符串映射，包括：
 *          - engine_status_to_string()       : 中文描述
 *          - engine_status_to_identifier()   : 英文标识符
 *          - engine_status_get_description() : 详细描述（含建议操作）
 *
 * @author Lv-00 Project
 */

#include "lv/engine.h"
#include "lv/lv_xmacro.h"

/* ============================================================
 *  引擎状态码 → 字符串映射
 * ============================================================ */

/* ================================================================
 * 引擎状态码名称表（数据表化，替代自建 struct + 线性扫描）
 *
 * 中文短名 / 英文标识符 / 详细描述 三张 lvStrToEnumEntry 表分别
 * 经 lv_enum_to_str()（按枚举值升序二分查找）查询，与
 * algebraic_number_rational.c / smt_backend_impl.c 既有用法一致；
 * 三张表均按 EngineStatus 枚举值升序排列，供二分查找使用。
 * ================================================================ */

/** @brief 中文短名表（engine_status_to_string，按枚举值升序） */
static const lvStrToEnumEntry kEngineStatusZhEntries[] = {
    {"成功", ENGINE_STATUS_OK},
    {"内存不足", ENGINE_STATUS_OUT_OF_MEMORY},
    {"无效状态", ENGINE_STATUS_INVALID_STATE},
    {"无效参数", ENGINE_STATUS_INVALID_ARGUMENT},
    {"约束冲突", ENGINE_STATUS_CONSTRAINT_CONFLICT},
    {"模块错误", ENGINE_STATUS_MODULE_ERROR},
    {"内部错误", ENGINE_STATUS_ERROR_INTERNAL},
};

/** @brief 英文标识符表（engine_status_to_identifier，按枚举值升序） */
static const lvStrToEnumEntry kEngineStatusEnEntries[] = {
    {"ENGINE_STATUS_OK", ENGINE_STATUS_OK},
    {"ENGINE_STATUS_OUT_OF_MEMORY", ENGINE_STATUS_OUT_OF_MEMORY},
    {"ENGINE_STATUS_INVALID_STATE", ENGINE_STATUS_INVALID_STATE},
    {"ENGINE_STATUS_INVALID_ARGUMENT", ENGINE_STATUS_INVALID_ARGUMENT},
    {"ENGINE_STATUS_CONSTRAINT_CONFLICT", ENGINE_STATUS_CONSTRAINT_CONFLICT},
    {"ENGINE_STATUS_MODULE_ERROR", ENGINE_STATUS_MODULE_ERROR},
    {"ENGINE_STATUS_ERROR_INTERNAL", ENGINE_STATUS_ERROR_INTERNAL},
};

/** @brief 详细描述表（engine_status_get_description，按枚举值升序） */
static const lvStrToEnumEntry kEngineStatusDescEntries[] = {
    {"操作成功完成。系统处于正常状态，可以继续后续操作。", ENGINE_STATUS_OK},
    {"内存分配失败。系统无法分配所需的内存资源。建议：检查系统内存使用情况，尝试释放不必要的资源，或减小"
     "问题规模。", ENGINE_STATUS_OUT_OF_MEMORY},
    {"引擎处于无效状态。当前操作与引擎状态不兼容。建议：检查引擎当前状态，必要时通过 lv_engine_transition_state() 转回 IDLE "
     "重置。", ENGINE_STATUS_INVALID_STATE},
    {"传入参数无效。可能是空指针、越界值或格式错误的参数。建议：检查函数调用的参数是否符合文档要求。",
     ENGINE_STATUS_INVALID_ARGUMENT},
    {"约束冲突。几何约束之间存在矛盾，无法满足所有约束条件。建议：检查约束定义，移除或修改冲突的约束。",
     ENGINE_STATUS_CONSTRAINT_CONFLICT},
    {"模块错误。加载或执行模块/公理包时发生错误。建议：检查模块文件路径和格式是否正确。", ENGINE_STATUS_MODULE_ERROR},
    {"内部错误。系统内部出现意外情况。建议：检查日志获取详细信息，如果问题持续请报告给开发团队。",
     ENGINE_STATUS_ERROR_INTERNAL},
};

const char *engine_status_to_string(EngineStatus status) {
    return lv_enum_to_str(kEngineStatusZhEntries, lv_ARRAY_SIZE(kEngineStatusZhEntries), (int) status, "未知错误");
}

const char *engine_status_to_identifier(EngineStatus status) {
    return lv_enum_to_str(kEngineStatusEnEntries, lv_ARRAY_SIZE(kEngineStatusEnEntries), (int) status,
                          "ENGINE_STATUS_UNKNOWN");
}

const char *engine_status_get_description(EngineStatus status) {
    return lv_enum_to_str(kEngineStatusDescEntries, lv_ARRAY_SIZE(kEngineStatusDescEntries), (int) status,
                          "未知错误。系统遇到未识别的错误状态。建议：检查日志并报告问题。");
}
