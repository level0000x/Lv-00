#include <string.h>

#include "lv/effect_system.h"
#include "lv_utils.h"
#include "lv/lv_xmacro.h"

/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief lv_effect_type_name 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_lv_effect_type_name_entries[] = {
    {"Pure", lv_EFFECT_PURE},
    {"FileRead", lv_EFFECT_FILE_READ},
    {"FileWrite", lv_EFFECT_FILE_WRITE},
    {"Network", lv_EFFECT_NETWORK},
    {"UIRender", lv_EFFECT_UI_RENDER},
    {"UIInput", lv_EFFECT_UI_INPUT},
    {"Random", lv_EFFECT_RANDOM},
    {"Time", lv_EFFECT_TIME},
};

const char *lv_effect_type_name(lvEffectType effect) {
    return lv_enum_to_str(s_lv_effect_type_name_entries, lv_ARRAY_SIZE(s_lv_effect_type_name_entries), (int) effect, "Unknown");
}
