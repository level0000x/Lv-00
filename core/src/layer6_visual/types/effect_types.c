#include "lv00/effect_system.h"
#include <string.h>

const char *lv00_effect_type_name(Lv00EffectType effect) {
    switch (effect) {
        case LV00_EFFECT_PURE: return "Pure";
        case LV00_EFFECT_FILE_READ: return "FileRead";
        case LV00_EFFECT_FILE_WRITE: return "FileWrite";
        case LV00_EFFECT_NETWORK: return "Network";
        case LV00_EFFECT_UI_RENDER: return "UIRender";
        case LV00_EFFECT_UI_INPUT: return "UIInput";
        case LV00_EFFECT_RANDOM: return "Random";
        case LV00_EFFECT_TIME: return "Time";
        default: return "Unknown";
    }
}
