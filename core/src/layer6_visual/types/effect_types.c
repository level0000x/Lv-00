#include <string.h>

#include "lv/effect_system.h"

const char *lv_effect_type_name(lvEffectType effect) {
    switch (effect) {
        case lv_EFFECT_PURE:
            return "Pure";
        case lv_EFFECT_FILE_READ:
            return "FileRead";
        case lv_EFFECT_FILE_WRITE:
            return "FileWrite";
        case lv_EFFECT_NETWORK:
            return "Network";
        case lv_EFFECT_UI_RENDER:
            return "UIRender";
        case lv_EFFECT_UI_INPUT:
            return "UIInput";
        case lv_EFFECT_RANDOM:
            return "Random";
        case lv_EFFECT_TIME:
            return "Time";
        default:
            return "Unknown";
    }
}
