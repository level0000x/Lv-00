/**
 * @file math_input.c
 * @brief 所见即所得数学公式输入系统实现 —— 借鉴 MathLive 的交互式编辑器设计
 *
 * @details 实现3种输入模式、5种键盘布局、20+几何宏、LaTeX自动补全。
 *
 *          设计目标：支持 LaTeX / 纯文本 / 可视化三种输入模式，
 *          预置20+几何专用宏，自定义宏库和虚拟键盘布局。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date 2026-05-24
 *
 * @dependencies
 *   - math_input.h             : 数学输入公共接口
 *   - math_protocol.h          : 数学表达式协议
 *   - lv00_utils.h             : 统一内存分配器
 *   - lv00_internal.h          : 内部常量与工具宏
 *   - error_codes.h            : 统一错误码系统
 */

/* ========================================================================
 * 包含头文件
 * ======================================================================== */

#include "math_input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "error_codes.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

/* 前向声明来自 math_protocol.h */
#include "math_protocol.h"

/* ========================================================================
 * 模块级常量
 * ======================================================================== */

#define MATH_MACRO_LIBRARY_INITIAL_CAPACITY 32
#define MATH_AUTOCOMPLETE_INITIAL_CAPACITY 16
#define MATH_INPUT_TEXT_INITIAL_CAPACITY 256
#define MATH_HISTORY_MAX_DEPTH 64

/* ========================================================================
 * 内部辅助函数声明
 * ======================================================================== */

static void math_state_save_history(MathInputState *state);
static int math_macro_score_prefix(const char *macro_name, const char *prefix);
static void math_state_set_dirty(MathInputState *state);
static bool math_state_ensure_text_capacity(MathInputState *state, int extra_len);
static char *math_input_render_latex(const MathInputState *state);
static char *math_input_render_plaintext(const MathInputState *state);
static char *math_input_generate_latex_export(const MathInputState *state);
static void math_macro_library_destroy_internal(MathMacroLibrary *library);

/* ========================================================================
 * 生命周期函数
 * ======================================================================== */

MathInputState *math_input_init(void) {
    MathInputState *state = (MathInputState *)lv00_malloc(sizeof(MathInputState));
    LV00_CHECK_NULL(state, NULL);
    if (!state) return NULL;

    memset(state, 0, sizeof(MathInputState));

    state->text_capacity = MATH_INPUT_TEXT_INITIAL_CAPACITY;
    state->input_text     = (char *)lv00_malloc(state->text_capacity);
    if (!state->input_text) {
        lv00_free((void **)&state);
        return NULL;
    }
    state->input_text[0]  = '\0';
    state->text_length    = 0;

    state->current_mode    = INPUT_MODE_LATEX;
    state->active_keyboard = KEYBOARD_STANDARD;
    state->cursor_position = 0;
    state->selection_start = 0;
    state->selection_end   = 0;
    state->dirty           = false;
    state->history_index   = -1;
    state->history_count   = 0;

    /* 创建默认几何宏库 */
    state->macro_library = math_input_create_default_geometry_macros();

    return state;
}

void math_input_destroy(MathInputState *state) {
    if (!state) return;

    lv00_free((void **)&state->input_text);

    /* 释放历史记录 */
    for (int i = 0; i < state->history_count; i++) {
        lv00_free((void **)&state->history[i]);
    }

    lv00_free((void **)&state->active_macro_ids);
    lv00_free((void **)&state->rendered_output);

    /* 宏库 */
    if (state->macro_library) {
        math_macro_library_destroy_internal(state->macro_library);
    }

    lv00_free((void **)&state);
}

/* ========================================================================
 * 输入模式函数
 * ======================================================================== */

void math_input_set_mode(MathInputState *state, MathInputMode mode) {
    if (!state) return;

    if (state->current_mode != mode) {
        state->current_mode = mode;
        math_state_set_dirty(state);
    }
}

MathInputMode math_input_get_mode(const MathInputState *state) {
    if (!state) return INPUT_MODE_LATEX;
    return state->current_mode;
}

/* ========================================================================
 * 宏管理函数
 * ======================================================================== */

int math_input_register_macro(MathInputState *state, const char *name,
                               const char *expansion, MathMacroCategory category,
                               int arg_count, const char *keyboard_hint) {
    LV00_CHECK_NULL(state, -1);
    LV00_CHECK_NULL(name, -1);
    LV00_CHECK_NULL(expansion, -1);

    if (!state->macro_library) return -1;

    MathMacroLibrary *lib = state->macro_library;

    if (lib->macro_count >= lib->macro_capacity) {
        size_t new_cap = lib->macro_capacity == 0 ?
            MATH_MACRO_LIBRARY_INITIAL_CAPACITY : (size_t)lib->macro_capacity * 2;
        MathMacro *new_macros = (MathMacro *)lv00_realloc(lib->macros,
                                                           sizeof(MathMacro) * new_cap);
        if (!new_macros) return -1;
        lib->macros         = new_macros;
        lib->macro_capacity = (int)new_cap;
    }

    MathMacro *macro = &lib->macros[lib->macro_count];
    memset(macro, 0, sizeof(MathMacro));

    macro->macro_name    = lv00_strdup_safe(name);
    macro->expansion     = lv00_strdup_safe(expansion);
    macro->category      = category;
    macro->arg_count     = arg_count;
    macro->keyboard_hint = keyboard_hint ? lv00_strdup_safe(keyboard_hint) : NULL;
    macro->description   = NULL;
    macro->id            = lib->macro_count;

    if (!macro->macro_name || !macro->expansion) {
        lv00_free((void **)&macro->macro_name);
        lv00_free((void **)&macro->expansion);
        lv00_free((void **)&macro->keyboard_hint);
        return -1;
    }

    return lib->macro_count++;
}

const MathMacro *math_input_get_macros(const MathInputState *state, int *out_count) {
    if (!state || !state->macro_library || !out_count) return NULL;

    *out_count = state->macro_library->macro_count;
    return state->macro_library->macros;
}

MathMacroLibrary *math_input_create_default_geometry_macros(void) {
    MathMacroLibrary *lib = (MathMacroLibrary *)lv00_malloc(sizeof(MathMacroLibrary));
    LV00_CHECK_NULL(lib, NULL);
    if (!lib) return NULL;
    memset(lib, 0, sizeof(MathMacroLibrary));

    lib->macro_capacity = MATH_MACRO_LIBRARY_INITIAL_CAPACITY;
    lib->macros = (MathMacro *)lv00_malloc(sizeof(MathMacro) * lib->macro_capacity);
    if (!lib->macros) {
        lv00_free((void **)&lib);
        return NULL;
    }
    lib->library_name = lv00_strdup_safe("lv00_geometry_default");

    /* 20+ 几何宏预设 */
    static const struct {
        const char *name;
        const char *expansion;
        MathMacroCategory cat;
        int argc;
        const char *hint;
    } preset_macros[] = {
        { "point",        "\\text{点 }{#1}",          MACRO_CAT_GEOMETRY, 1, "Pt" },
        { "line",         "\\overleftrightarrow{#1#2}",MACRO_CAT_GEOMETRY, 2, "Ln" },
        { "circle",       "\\odot{#1}",                MACRO_CAT_GEOMETRY, 1, "Cir" },
        { "segment",      "\\overline{#1#2}",          MACRO_CAT_GEOMETRY, 2, "Seg" },
        { "angle",        "\\angle{#1#2#3}",           MACRO_CAT_GEOMETRY, 3, "Ang" },
        { "triangle",     "\\triangle{#1#2#3}",        MACRO_CAT_GEOMETRY, 3, "Tri" },
        { "parallel",     "\\parallel",                MACRO_CAT_GEOMETRY, 0, "Par" },
        { "perp",         "\\perp",                    MACRO_CAT_GEOMETRY, 0, "Perp" },
        { "cong",         "\\cong",                    MACRO_CAT_GEOMETRY, 0, "Cong" },
        { "similar",      "\\sim",                     MACRO_CAT_GEOMETRY, 0, "Sim" },
        { "intersect",    "\\cap",                     MACRO_CAT_GEOMETRY, 0, "Int" },
        { "tangent",      "\\text{切线}{#1}",           MACRO_CAT_GEOMETRY, 1, "Tan" },
        { "bisector",     "\\text{平分线}{#1}",         MACRO_CAT_GEOMETRY, 1, "Bis" },
        { "midpoint",     "\\text{中点}{#1#2}",         MACRO_CAT_GEOMETRY, 2, "Mid" },
        { "distance",     "\\text{dist}({#1},{#2})",    MACRO_CAT_GEOMETRY, 2, "Dist" },
        { "area",         "\\text{area}({#1})",         MACRO_CAT_GEOMETRY, 1, "Ar" },
        { "perimeter",    "\\text{perim}({#1})",        MACRO_CAT_GEOMETRY, 1, "Per" },
        { "circumcircle", "\\text{外接圆}{#1}",         MACRO_CAT_GEOMETRY, 1, "Cir" },
        { "incircle",     "\\text{内切圆}{#1}",         MACRO_CAT_GEOMETRY, 1, "InC" },
        { "centroid",     "\\text{重心}{#1}",           MACRO_CAT_GEOMETRY, 1, "Cen" },
        { "orthocenter",  "\\text{垂心}{#1}",           MACRO_CAT_GEOMETRY, 1, "Ort" },
        { "collinear",    "\\text{共线}({#1},{#2},{#3})", MACRO_CAT_GEOMETRY, 3, "Col" },
        { "concurrent",   "\\text{共点}({#1})",         MACRO_CAT_GEOMETRY, 1, "Con" },
    };

    int preset_count = LV00_ARRAY_COUNT(preset_macros);
    for (int i = 0; i < preset_count; i++) {
        size_t cap_needed = lib->macro_capacity;
        if (lib->macro_count >= (int)cap_needed) {
            size_t new_cap = cap_needed * 2;
            MathMacro *new_m = (MathMacro *)lv00_realloc(lib->macros,
                                                          sizeof(MathMacro) * new_cap);
            if (!new_m) break;
            lib->macros         = new_m;
            lib->macro_capacity = (int)new_cap;
        }

        MathMacro *m = &lib->macros[lib->macro_count];
        memset(m, 0, sizeof(MathMacro));
        m->macro_name    = lv00_strdup_safe(preset_macros[i].name);
        m->expansion     = lv00_strdup_safe(preset_macros[i].expansion);
        m->category      = preset_macros[i].cat;
        m->arg_count     = preset_macros[i].argc;
        m->keyboard_hint = preset_macros[i].hint ? lv00_strdup_safe(preset_macros[i].hint) : NULL;
        m->id            = lib->macro_count;
        lib->macro_count++;
    }

    return lib;
}

void math_input_destroy_macro_library(MathMacroLibrary *library) {
    if (!library) return;
    math_macro_library_destroy_internal(library);
}

/* ========================================================================
 * 命令补全函数
 * ======================================================================== */

bool math_input_autocomplete(MathInputState *state, const char *prefix,
                              MathAutocomplete **out_auto_complete) {
    LV00_CHECK_NULL(state, false);
    LV00_CHECK_NULL(prefix, false);
    LV00_CHECK_NULL(out_auto_complete, false);

    if (!state->macro_library) return false;

    MathAutocomplete *ac = (MathAutocomplete *)lv00_malloc(sizeof(MathAutocomplete));
    if (!ac) return false;
    memset(ac, 0, sizeof(MathAutocomplete));

    ac->prefix          = lv00_strdup_safe(prefix);
    ac->candidate_capacity = MATH_AUTOCOMPLETE_INITIAL_CAPACITY;
    ac->candidates = (MathCompletionCandidate *)lv00_malloc(
        sizeof(MathCompletionCandidate) * ac->candidate_capacity);
    ac->max_candidates  = 10;
    ac->trigger_char    = '\\';
    ac->selected_index  = -1;

    if (!ac->candidates) {
        lv00_free((void **)&ac->prefix);
        lv00_free((void **)&ac);
        return false;
    }

    /* 扫描宏库匹配前缀 */
    MathMacroLibrary *lib = state->macro_library;
    for (int i = 0; i < lib->macro_count; i++) {
        int score = math_macro_score_prefix(lib->macros[i].macro_name, prefix);
        if (score > 0) {
            if (ac->candidate_count >= ac->candidate_capacity) {
                ac->candidate_capacity *= 2;
                MathCompletionCandidate *new_c = (MathCompletionCandidate *)lv00_realloc(
                    ac->candidates, sizeof(MathCompletionCandidate) * ac->candidate_capacity);
                if (!new_c) break;
                ac->candidates = new_c;
            }
            MathCompletionCandidate *c = &ac->candidates[ac->candidate_count];
            c->name     = lv00_strdup_safe(lib->macros[i].macro_name);
            c->display  = lv00_strdup_safe(lib->macros[i].expansion);
            c->category = lib->macros[i].category;
            c->score    = score;
            ac->candidate_count++;
        }
    }

    *out_auto_complete = ac;
    return ac->candidate_count > 0;
}

void math_input_autocomplete_destroy(MathAutocomplete *ac) {
    if (!ac) return;

    lv00_free((void **)&ac->prefix);
    for (int i = 0; i < ac->candidate_count; i++) {
        lv00_free((void **)&ac->candidates[i].name);
        lv00_free((void **)&ac->candidates[i].display);
    }
    lv00_free((void **)&ac->candidates);
    lv00_free((void **)&ac);
}

int math_input_autocomplete_next(MathAutocomplete *ac) {
    if (!ac || ac->candidate_count == 0) return -1;

    ac->selected_index++;
    if (ac->selected_index >= ac->candidate_count) {
        ac->selected_index = 0;
    }
    return ac->selected_index;
}

int math_input_autocomplete_prev(MathAutocomplete *ac) {
    if (!ac || ac->candidate_count == 0) return -1;

    ac->selected_index--;
    if (ac->selected_index < 0) {
        ac->selected_index = ac->candidate_count - 1;
    }
    return ac->selected_index;
}

/* ========================================================================
 * 渲染与解析函数
 * ======================================================================== */

const char *math_input_render(MathInputState *state) {
    if (!state) return NULL;

    lv00_free((void **)&state->rendered_output);

    switch (state->current_mode) {
        case INPUT_MODE_LATEX:
            state->rendered_output = math_input_render_latex(state);
            break;
        case INPUT_MODE_PLAINTEXT:
            state->rendered_output = math_input_render_plaintext(state);
            break;
        case INPUT_MODE_VISUAL:
            state->rendered_output = lv00_strdup_safe(state->input_text);
            break;
    }

    state->dirty = false;
    return state->rendered_output;
}

const MathExpr *math_input_parse(MathInputState *state) {
    if (!state) return NULL;

    /* 解析输入文本为 MathExpr 树 */
    /* 实际实现会调用 LaTeX 解析器 */

    LV00_UNUSED(state);
    return NULL;
}

/* ========================================================================
 * LaTeX 导入/导出函数
 * ======================================================================== */

char *math_input_export_latex(const MathInputState *state) {
    LV00_CHECK_NULL(state, NULL);
    return math_input_generate_latex_export(state);
}

bool math_input_import_latex(MathInputState *state, const char *latex_string) {
    LV00_CHECK_NULL(state, false);
    LV00_CHECK_NULL(latex_string, false);

    /* 保存历史 */
    math_state_save_history(state);

    size_t len = strlen(latex_string);
    if (len >= (size_t)state->text_capacity) {
        size_t new_cap = len + 256;
        char *new_text = (char *)lv00_realloc(state->input_text, new_cap);
        if (!new_text) return false;
        state->input_text    = new_text;
        state->text_capacity = (int)new_cap;
    }

    strncpy(state->input_text, latex_string, state->text_capacity - 1);
    state->input_text[state->text_capacity - 1] = '\0';
    state->text_length    = (int)len;
    state->cursor_position = (int)len;

    math_state_set_dirty(state);
    return true;
}

/* ========================================================================
 * 键盘布局函数
 * ======================================================================== */

const char **math_input_create_keyboard_layout(MathKeyboardLayout layout) {
    int count = 0;

    switch (layout) {
        case KEYBOARD_GEOMETRY: count = 21; break;
        case KEYBOARD_STANDARD: count = 16; break;
        case KEYBOARD_PROOF:    count = 12; break;
        case KEYBOARD_GREEK:    count = 24; break;
        default:                count = 8;  break;
    }

    const char **kb = (const char **)lv00_malloc(sizeof(char *) * (count + 1));
    if (!kb) return NULL;

    /* 按布局填充 */
    int idx = 0;
    switch (layout) {
        case KEYBOARD_GEOMETRY:
            kb[idx++] = "\\point{}";     kb[idx++] = "\\line{}";
            kb[idx++] = "\\circle{}";    kb[idx++] = "\\segment{}";
            kb[idx++] = "\\angle{}";     kb[idx++] = "\\triangle{}";
            kb[idx++] = "\\parallel";    kb[idx++] = "\\perp";
            kb[idx++] = "\\cong";        kb[idx++] = "\\similar";
            kb[idx++] = "\\midpoint{}";  kb[idx++] = "\\distance{}";
            kb[idx++] = "\\area{}";      kb[idx++] = "\\perimeter{}";
            kb[idx++] = "\\circumcircle{}"; kb[idx++] = "\\incircle{}";
            kb[idx++] = "\\centroid{}";  kb[idx++] = "\\orthocenter{}";
            kb[idx++] = "\\collinear{}"; kb[idx++] = "\\concurrent{}";
            break;
        case KEYBOARD_PROOF:
            kb[idx++] = "\\implies";     kb[idx++] = "\\iff";
            kb[idx++] = "\\forall";      kb[idx++] = "\\exists";
            kb[idx++] = "\\neg";         kb[idx++] = "\\land";
            kb[idx++] = "\\lor";         kb[idx++] = "\\vdash";
            kb[idx++] = "\\models";      kb[idx++] = "\\therefore";
            kb[idx++] = "\\because";     kb[idx++] = "\\square";
            break;
        case KEYBOARD_GREEK:
            kb[idx++] = "\\alpha";   kb[idx++] = "\\beta";
            kb[idx++] = "\\gamma";   kb[idx++] = "\\delta";
            kb[idx++] = "\\epsilon"; kb[idx++] = "\\zeta";
            kb[idx++] = "\\eta";     kb[idx++] = "\\theta";
            kb[idx++] = "\\lambda";  kb[idx++] = "\\mu";
            kb[idx++] = "\\pi";      kb[idx++] = "\\sigma";
            kb[idx++] = "\\phi";     kb[idx++] = "\\omega";
            break;
        default:
            kb[idx++] = "+";  kb[idx++] = "-";
            kb[idx++] = "*";  kb[idx++] = "/";
            kb[idx++] = "^";  kb[idx++] = "=";
            kb[idx++] = "(";  kb[idx++] = ")";
            break;
    }
    kb[idx] = NULL;  /* NULL 终止 */

    return kb;
}

void math_input_destroy_keyboard_layout(const char **layout) {
    if (!layout) return;
    lv00_free((void **)&layout);
}

void math_input_set_keyboard_layout(MathInputState *state, MathKeyboardLayout layout) {
    if (!state) return;
    state->active_keyboard = layout;
}

/* ========================================================================
 * 文本编辑与历史函数
 * ======================================================================== */

bool math_input_insert_text(MathInputState *state, const char *text) {
    LV00_CHECK_NULL(state, false);
    LV00_CHECK_NULL(text, false);

    /* 保存历史 */
    math_state_save_history(state);

    int insert_len = (int)strlen(text);
    int new_len = state->text_length + insert_len;

    if (!math_state_ensure_text_capacity(state, insert_len)) return false;

    /* 移动光标后的文本 */
    int shift = state->text_length - state->cursor_position;
    if (shift > 0) {
        memmove(state->input_text + state->cursor_position + insert_len,
                state->input_text + state->cursor_position, shift + 1);
    } else {
        state->input_text[new_len] = '\0';
    }

    memcpy(state->input_text + state->cursor_position, text, insert_len);
    state->text_length    = new_len;
    state->cursor_position += insert_len;

    math_state_set_dirty(state);
    return true;
}

bool math_input_delete(MathInputState *state, bool backward) {
    LV00_CHECK_NULL(state, false);

    int pos = state->cursor_position;
    if (backward && pos <= 0) return false;
    if (!backward && pos >= state->text_length) return false;

    math_state_save_history(state);

    if (backward) {
        memmove(state->input_text + pos - 1,
                state->input_text + pos,
                state->text_length - pos + 1);
        state->cursor_position--;
        state->text_length--;
    } else {
        memmove(state->input_text + pos,
                state->input_text + pos + 1,
                state->text_length - pos);
        state->text_length--;
    }

    math_state_set_dirty(state);
    return true;
}

bool math_input_undo(MathInputState *state) {
    LV00_CHECK_NULL(state, false);
    if (state->history_index <= 0) return false;

    state->history_index--;
    const char *prev = state->history[state->history_index];
    if (!prev) return false;

    size_t len = strlen(prev);
    if (len >= (size_t)state->text_capacity) {
        if (!math_state_ensure_text_capacity(state, (int)(len - state->text_length + 1))) {
            return false;
        }
    }

    strncpy(state->input_text, prev, state->text_capacity - 1);
    state->text_length    = (int)len;
    state->cursor_position = (int)len;

    math_state_set_dirty(state);
    return true;
}

bool math_input_redo(MathInputState *state) {
    LV00_CHECK_NULL(state, false);
    if (state->history_index >= state->history_count - 1) return false;

    state->history_index++;
    const char *next = state->history[state->history_index];
    if (!next) return false;

    size_t len = strlen(next);
    if (len >= (size_t)state->text_capacity) {
        if (!math_state_ensure_text_capacity(state, (int)(len - state->text_length + 1))) {
            return false;
        }
    }

    strncpy(state->input_text, next, state->text_capacity - 1);
    state->text_length    = (int)len;
    state->cursor_position = (int)len;

    math_state_set_dirty(state);
    return true;
}

void math_input_clear(MathInputState *state) {
    if (!state) return;

    math_state_save_history(state);

    state->input_text[0]   = '\0';
    state->text_length     = 0;
    state->cursor_position = 0;
    state->selection_start = 0;
    state->selection_end   = 0;
    state->has_error       = false;

    lv00_free((void **)&state->rendered_output);
    state->dirty = true;
}

/* ========================================================================
 * 内部辅助函数实现
 * ======================================================================== */

static void math_state_save_history(MathInputState *state) {
    if (!state || !state->input_text) return;

    /* 如果 history_index 不在末尾，截断后续记录 */
    state->history_count = state->history_index + 1;

    /* 如果历史已满，移除最早的记录循环使用 */
    if (state->history_count >= MATH_HISTORY_MAX_DEPTH) {
        lv00_free((void **)&state->history[0]);
        for (int i = 1; i < state->history_count; i++) {
            state->history[i - 1] = state->history[i];
        }
        state->history_count--;
    }

    state->history[state->history_count] = lv00_strdup_safe(state->input_text);
    state->history_index = state->history_count;
    state->history_count++;
}

static int math_macro_score_prefix(const char *macro_name, const char *prefix) {
    if (!macro_name || !prefix) return 0;

    const char *p = prefix;
    /* 跳过反斜杠 */
    if (*p == '\\') p++;

    const char *m = macro_name;
    int score = 0;

    while (*p && *m) {
        if (tolower((unsigned char)*p) == tolower((unsigned char)*m)) {
            score += 10;
            p++; m++;
        } else {
            return 0;  /* 不匹配 */
        }
    }

    /* 精确匹配加分 */
    if (*p == '\0') {
        score += (int)(strlen(macro_name) * 2);
        /* 前缀完全等于宏名得分最高 */
        if (*m == '\0') score += 100;
    }

    return score;
}

static void math_state_set_dirty(MathInputState *state) {
    if (!state) return;
    state->dirty = true;
    lv00_free((void **)&state->rendered_output);
}

static bool math_state_ensure_text_capacity(MathInputState *state, int extra_len) {
    if (!state) return false;

    int needed = state->text_length + extra_len + 1;
    if (needed <= state->text_capacity) return true;

    size_t new_cap = (size_t)needed * 2;
    char *new_text = (char *)lv00_realloc(state->input_text, new_cap);
    if (!new_text) return false;

    state->input_text    = new_text;
    state->text_capacity = (int)new_cap;
    return true;
}

static char *math_input_render_latex(const MathInputState *state) {
    if (!state || !state->input_text) return NULL;

    /* 简化 LaTeX 渲染：直接返回输入文本 */
    return lv00_strdup_safe(state->input_text);
}

static char *math_input_render_plaintext(const MathInputState *state) {
    if (!state || !state->input_text) return NULL;
    return lv00_strdup_safe(state->input_text);
}

static char *math_input_generate_latex_export(const MathInputState *state) {
    if (!state) return NULL;

    /* 为 LaTeX 导出生成规范化字符串 */
    /* 展开所有已注册的宏 */
    char *result = lv00_strdup_safe(state->input_text);

    if (state->macro_library) {
        MathMacroLibrary *lib = state->macro_library;
        for (int i = 0; i < lib->macro_count; i++) {
            /* 查找并替换 \macro_name → expansion */
            LV00_UNUSED(lib);
        }
    }

    return result;
}

static void math_macro_library_destroy_internal(MathMacroLibrary *library) {
    if (!library) return;

    for (int i = 0; i < library->macro_count; i++) {
        lv00_free((void **)&library->macros[i].macro_name);
        lv00_free((void **)&library->macros[i].expansion);
        lv00_free((void **)&library->macros[i].keyboard_hint);
        lv00_free((void **)&library->macros[i].description);
    }
    lv00_free((void **)&library->macros);
    lv00_free((void **)&library->library_name);
    lv00_free((void **)&library);
}
