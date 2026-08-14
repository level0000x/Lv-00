#include <string.h>

#include "lv/effect_system.h"
#include "lv/lv_utils.h"
#include "lv/lv_xmacro.h"

/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

#define LV_EFFECT_TYPE_X(x) \
    x(lv_EFFECT_PURE, "Pure") \
    x(lv_EFFECT_FILE_READ, "FileRead") \
    x(lv_EFFECT_FILE_WRITE, "FileWrite") \
    x(lv_EFFECT_NETWORK, "Network") \
    x(lv_EFFECT_UI_RENDER, "UIRender") \
    x(lv_EFFECT_UI_INPUT, "UIInput") \
    x(lv_EFFECT_RANDOM, "Random") \
    x(lv_EFFECT_TIME, "Time")

/** @brief lv_effect_type_name 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_lv_effect_type_name_entries[] = {
    lv_XMACRO_TO_ENUM_TABLE(LV_EFFECT_TYPE_X)
};

const char *lv_effect_type_name(lvEffectType effect) {
    return lv_enum_to_str(s_lv_effect_type_name_entries, lv_ARRAY_SIZE(s_lv_effect_type_name_entries), (int) effect, "Unknown");
}
