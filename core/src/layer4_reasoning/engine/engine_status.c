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
 * 引擎状态码名称表（X-macro 单一键表生成，判据 D：同一枚举三张平行表合流）
 *
 * 中文短名 / 英文标识符 / 详细描述 三张 lvStrToEnumEntry 表均由同一
 * ENGINE_STATUS_X 键表生成，消除三份枚举清单的漂移风险（新增/删除
 * 状态码只改键表一处）。键表项序 = EngineStatus 枚举值升序，供
 * lv_enum_to_str()（按枚举值升序二分查找）查询；与
 * algebraic_number_rational.c / smt_backend_impl.c 既有用法一致。
 * ================================================================ */

/** @brief EngineStatus 状态码键表（升序；desc 为详细描述，含建议操作） */
#define ENGINE_STATUS_X(x)                                                                                             \
    x(ENGINE_STATUS_OK, "ENGINE_STATUS_OK", "成功",                                                                    \
      "操作成功完成。系统处于正常状态，可以继续后续操作。")                                                              \
    x(ENGINE_STATUS_OUT_OF_MEMORY, "ENGINE_STATUS_OUT_OF_MEMORY", "内存不足",                                           \
      "内存分配失败。系统无法分配所需的内存资源。建议：检查系统内存使用情况，尝试释放不必要的资源，或减小问题规模。")       \
    x(ENGINE_STATUS_INVALID_STATE, "ENGINE_STATUS_INVALID_STATE", "无效状态",                                           \
      "引擎处于无效状态。当前操作与引擎状态不兼容。建议：检查引擎当前状态，必要时通过 lv_engine_transition_state() "      \
      "转回 IDLE 重置。")                                                                                               \
    x(ENGINE_STATUS_INVALID_ARGUMENT, "ENGINE_STATUS_INVALID_ARGUMENT", "无效参数",                                     \
      "传入参数无效。可能是空指针、越界值或格式错误的参数。建议：检查函数调用的参数是否符合文档要求。")                   \
    x(ENGINE_STATUS_CONSTRAINT_CONFLICT, "ENGINE_STATUS_CONSTRAINT_CONFLICT", "约束冲突",                               \
      "几何约束之间存在矛盾，无法满足所有约束条件。建议：检查约束定义，移除或修改冲突的约束。")                           \
    x(ENGINE_STATUS_MODULE_ERROR, "ENGINE_STATUS_MODULE_ERROR", "模块错误",                                             \
      "模块错误。加载或执行模块/公理包时发生错误。建议：检查模块文件路径和格式是否正确。")                               \
    x(ENGINE_STATUS_ERROR_INTERNAL, "ENGINE_STATUS_ERROR_INTERNAL", "内部错误",                                         \
      "内部错误。系统内部出现意外情况。建议：检查日志获取详细信息，如果问题持续请报告给开发团队。")

/** @brief 键表生成器：中文短名条目（engine_status_to_string） */
#define LV_ES_ZH_ENTRY(name, en, zh, desc) { zh, name },
/** @brief 键表生成器：英文标识符条目（engine_status_to_identifier） */
#define LV_ES_EN_ENTRY(name, en, zh, desc) { en, name },
/** @brief 键表生成器：详细描述条目（engine_status_get_description） */
#define LV_ES_DESC_ENTRY(name, en, zh, desc) { desc, name },

/** @brief 中文短名表（按枚举值升序） */
static const lvStrToEnumEntry kEngineStatusZhEntries[] = {
    ENGINE_STATUS_X(LV_ES_ZH_ENTRY)
};

/** @brief 英文标识符表（按枚举值升序） */
static const lvStrToEnumEntry kEngineStatusEnEntries[] = {
    ENGINE_STATUS_X(LV_ES_EN_ENTRY)
};

/** @brief 详细描述表（按枚举值升序） */
static const lvStrToEnumEntry kEngineStatusDescEntries[] = {
    ENGINE_STATUS_X(LV_ES_DESC_ENTRY)
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
