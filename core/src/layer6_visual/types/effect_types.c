#include <string.h>

#include "lv/effect_system.h"
#include "lv_utils.h"

/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief 枚举值 -> 名称 映射项（表必须按 code 升序排列） */
typedef struct {
    int code;         /**< 枚举值 */
    const char *name; /**< 名称字符串 */
} eff_NameEntry;

/** @brief 二分查找枚举名称（表需按 code 升序） */
static const char *eff_name_lookup(const eff_NameEntry *table, size_t count, int code) {
    size_t lo = 0, hi = count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (table[mid].code == code)
            return table[mid].name;
        if (table[mid].code < code)
            lo = mid + 1;
        else
            hi = mid;
    }
    return NULL;
}

/** @brief lv_effect_type_name 名称表（按枚举值升序） */
static const eff_NameEntry s_lv_effect_type_name_entries[] = {
    {lv_EFFECT_PURE, "Pure"},
    {lv_EFFECT_FILE_READ, "FileRead"},
    {lv_EFFECT_FILE_WRITE, "FileWrite"},
    {lv_EFFECT_NETWORK, "Network"},
    {lv_EFFECT_UI_RENDER, "UIRender"},
    {lv_EFFECT_UI_INPUT, "UIInput"},
    {lv_EFFECT_RANDOM, "Random"},
    {lv_EFFECT_TIME, "Time"},
};

const char *lv_effect_type_name(lvEffectType effect) {
    const char *name = eff_name_lookup(s_lv_effect_type_name_entries, lv_ARRAY_SIZE(s_lv_effect_type_name_entries), (int) effect);
    return name ? name : "Unknown";
}
