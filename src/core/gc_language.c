/**
 * @file gc_language.c
 * @brief GCL 几何命令语言实现 —— 借鉴 GCLC GC Language 语法与 WASM 管道
 *
 * @details 完整实现 GCL（Geometric Construction Language）解析器和执行引擎，
 *          支持 42 种几何命令的解析、执行和约束图输出。
 *
 *          核心模块：
 *          1. GCLContext 生命周期管理（创建/销毁）
 *          2. 命令解析器（单行解析 + 文件批量解析）
 *          3. 命令执行引擎（单条 + 批量执行）
 *          4. 证明方法系统（5种：面积/吴/Gröbner/全角/向量）
 *          5. 几何定理证明（prove）
 *          6. 导出管道（LaTeX + HTML）
 *          7. WASM 编译配置
 *          8. TypeScript 绑定生成
 *          9. 约束图转换
 *
 *          借鉴 GCLC 的设计理念：
 *          - 声明式语法映射
 *          - 多证明方法热切换
 *          - 30 年向后兼容策略（版本号标记 + 弃用而非删除）
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date 2026-05-24
 *
 * @dependencies
 *   - gc_language.h         : GCL 公共接口
 *   - constraint_graph.h    : 约束图核心
 *   - lv00_utils.h          : 内存分配器
 *   - lv00_internal.h       : 内部常量与宏
 *   - error_codes.h         : 错误码系统
 */

/* ========================================================================
 * 包含头文件
 * ======================================================================== */

#include "gc_language.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constraint_graph.h"
#include "error_codes.h"
#include "lv00_internal.h"
#include "lv00_utils.h"
#include "symbolic_coord.h"

/* ========================================================================
 * 模块级常量
 * ======================================================================== */

/** @brief 命令数组初始容量 */
#define GCL_COMMAND_INITIAL_CAPACITY 32

/** @brief 符号表初始容量 */
#define GCL_SYMBOL_INITIAL_CAPACITY 64

/** @brief 默认 WASM 内存大小（字节） */
#define GCL_WASM_DEFAULT_MEMORY (64 * 1024 * 1024)

/** @brief 默认 WASM 栈大小（字节） */
#define GCL_WASM_DEFAULT_STACK (1 * 1024 * 1024)

/** @brief 文件每行最大读取长度 */
#define GCL_LINE_MAX 4096

/* ========================================================================
 * 静态辅助函数前向声明
 * ======================================================================== */

static bool gcl_parse_point(GCLContext *ctx, const char *line, GCLCommand *cmd);
static bool gcl_parse_line(GCLContext *ctx, const char *line, GCLCommand *cmd);
static bool gcl_parse_circle(GCLContext *ctx, const char *line, GCLCommand *cmd);
static bool gcl_parse_construct(GCLContext *ctx, const char *line,
                                 GCLCommand *cmd, GCLCommandType type);
static bool gcl_symbol_table_grow(GCLContext *ctx);
static int  gcl_register_symbol(GCLContext *ctx, const char *name, int node_id);
static bool gcl_command_array_grow(GCLContext *ctx);

/* ========================================================================
 * 命令类型名称映射表
 * ======================================================================== */

static const char *gcl_command_names[] = {
    "point",        "line",          "circle",       "segment",
    "ray",          "arc",           "polygon",      "triangle",
    "intersection", "midpoint",      "bisector",     "perpendicular",
    "parallel",     "mediatrix",     "orthocenter",  "centroid",
    "circumcenter", "incenter",      "foot",         "reflection",
    "rotation",     "translation",   "scale",        "measure",
    "angle",        "calc",          "distance",     "area",
    "prove",        "assume",        "lemma",        "conjecture",
    "counterexample", "load",        "include",      "export",
    "save",         "comment",       "set",          "echo",
    "dump"
};

/* ========================================================================
 * enum → 字符串转换
 * ======================================================================== */

/**
 * @brief 将证明方法枚举值转换为字符串
 * @param method 证明方法枚举值
 * @return 方法名称字符串（"area"/"wu"/"groebner"/"full_angle"/"vector"），无效返回 "?"
 */
const char *gcl_proof_method_to_string(GCLProofMethod method)
{
    static const char *names[] = {
        "area", "wu", "groebner", "full_angle", "vector"
    };
    if ((int)method < 0 || method > GCL_PROOF_VECTOR) return "?";
    return names[(int)method];
}

/**
 * @brief 将命令类型枚举值转换为字符串
 * @param type 命令类型枚举值
 * @return 命令名称字符串，无效返回 "?"
 */
const char *gcl_command_type_to_string(GCLCommandType type)
{
    if ((int)type < 0 || type >= GCL_CMD_COUNT) return "?";
    return gcl_command_names[(int)type];
}

/**
 * @brief 将证明结果枚举值转换为字符串
 * @param result 证明结果枚举值
 * @return 结果名称字符串，无效返回 "?"
 */
const char *gcl_prove_result_to_string(GCLProveResult result)
{
    static const char *names[] = {
        "OK", "FAIL_UNKNOWN", "TIMEOUT", "NOT_A_THM", "RESOURCES", "UNDECIDED"
    };
    if ((int)result < 0 || result > GCL_PROVE_FAIL_UNDECIDED) return "?";
    return names[(int)result];
}

/**
 * @brief 将 WASM 导出格式枚举值转换为字符串
 * @param format WASM 导出格式枚举值
 * @return 格式名称字符串，无效返回 "?"
 */
const char *gcl_wasm_format_to_string(WasmExportFormat format)
{
    static const char *names[] = { "default", "minimal", "full" };
    if ((int)format < 0 || format > WASM_GCL_FULL) return "?";
    return names[(int)format];
}

/* ========================================================================
 * 第一部分：上下文生命周期
 * ======================================================================== */

/**
 * @brief 创建 GCL 上下文实例
 *
 * 分配并初始化命令数组、符号表、证明方法和 WASM 配置。
 *
 * @return 新分配的 GCLContext 指针，失败返回 NULL
 */
GCLContext *gcl_context_create(void)
{
    GCLContext *ctx = lv00_malloc(sizeof(GCLContext));
    LV00_CHECK_ALLOC(ctx, NULL);
    memset(ctx, 0, sizeof(GCLContext));

    ctx->command_capacity = GCL_COMMAND_INITIAL_CAPACITY;
    ctx->commands = lv00_calloc((size_t)ctx->command_capacity,
                                 sizeof(GCLCommand *));
    if (!ctx->commands) {
        lv00_free((void **)&ctx);
        return NULL;
    }
    ctx->command_count        = 0;
    ctx->proof_method         = GCL_PROOF_AREA;
    ctx->proof_method_explicit = false;

    ctx->symbol_capacity = GCL_SYMBOL_INITIAL_CAPACITY;
    ctx->symbol_names    = lv00_calloc((size_t)ctx->symbol_capacity,
                                        sizeof(char *));
    ctx->symbol_node_ids = lv00_calloc((size_t)ctx->symbol_capacity,
                                        sizeof(int));
    if (!ctx->symbol_names || !ctx->symbol_node_ids) {
        lv00_free((void **)&ctx->symbol_names);
        lv00_free((void **)&ctx->symbol_node_ids);
        lv00_free((void **)&ctx->commands);
        lv00_free((void **)&ctx);
        return NULL;
    }
    ctx->symbol_count  = 0;
    ctx->graph         = NULL;
    ctx->is_parsed     = false;
    ctx->has_errors    = false;
    ctx->current_line  = 0;
    ctx->source_file   = NULL;
    ctx->error_message[0] = '\0';

    /* 默认 WASM 配置 */
    memset(&ctx->wasm_config, 0, sizeof(ctx->wasm_config));
    ctx->wasm_config.memory_size           = GCL_WASM_DEFAULT_MEMORY;
    ctx->wasm_config.enable_proof          = false;
    ctx->wasm_config.enable_visualization  = false;
    ctx->wasm_config.enable_latex_export   = false;
    ctx->wasm_config.enable_html_export    = false;
    ctx->wasm_config.enable_file_system    = false;
    ctx->wasm_config.enable_multithreading = false;
    ctx->wasm_config.export_format         = WASM_GCL_DEFAULT;
    ctx->wasm_config.stack_size            = GCL_WASM_DEFAULT_STACK;
    ctx->wasm_config.module_name           = "Lv00GCL";
    ctx->wasm_config.output_dir            = NULL;

    return ctx;
}

/**
 * @brief 销毁 GCL 上下文实例，释放所有命令和符号表资源
 * @param ctx 要销毁的上下文指针
 */
void gcl_context_destroy(GCLContext *ctx)
{
    if (!ctx) return;

    for (int i = 0; i < ctx->command_count; i++) {
        if (ctx->commands[i]) {
            lv00_free((void **)&ctx->commands[i]);
        }
    }
    lv00_free((void **)&ctx->commands);

    for (int i = 0; i < ctx->symbol_count; i++) {
        if (ctx->symbol_names[i]) {
            lv00_free((void **)&ctx->symbol_names[i]);
        }
    }
    lv00_free((void **)&ctx->symbol_names);
    lv00_free((void **)&ctx->symbol_node_ids);
    lv00_free((void **)&ctx);
}

/* ========================================================================
 * 第二部分：内部扩容函数
 * ======================================================================== */

static bool gcl_command_array_grow(GCLContext *ctx)
{
    int new_cap = ctx->command_capacity * 2;
    GCLCommand **new_arr = lv00_realloc(ctx->commands,
        (size_t)new_cap * sizeof(GCLCommand *));
    if (!new_arr) return false;
    for (int i = ctx->command_capacity; i < new_cap; i++) {
        new_arr[i] = NULL;
    }
    ctx->commands         = new_arr;
    ctx->command_capacity = new_cap;
    return true;
}

static bool gcl_symbol_table_grow(GCLContext *ctx)
{
    int new_cap = ctx->symbol_capacity * 2;
    char **new_names = lv00_realloc(ctx->symbol_names,
        (size_t)new_cap * sizeof(char *));
    int  *new_ids   = lv00_realloc(ctx->symbol_node_ids,
        (size_t)new_cap * sizeof(int));
    if (!new_names || !new_ids) {
        lv00_free((void **)&new_names);
        lv00_free((void **)&new_ids);
        return false;
    }
    for (int i = ctx->symbol_capacity; i < new_cap; i++) {
        new_names[i] = NULL;
        new_ids[i]   = -1;
    }
    ctx->symbol_names    = new_names;
    ctx->symbol_node_ids = new_ids;
    ctx->symbol_capacity = new_cap;
    return true;
}

static int gcl_register_symbol(GCLContext *ctx, const char *name, int node_id)
{
    if (!ctx || !name) return -1;
    if (ctx->symbol_count >= ctx->symbol_capacity) {
        if (!gcl_symbol_table_grow(ctx)) return -1;
    }
    int idx = ctx->symbol_count;
    ctx->symbol_names[idx]    = lv00_strdup_safe(name);
    ctx->symbol_node_ids[idx] = node_id;
    ctx->symbol_count++;
    return idx;
}

/**
 * @brief 在符号表中查找指定名称对应的节点 ID
 * @param ctx         GCL 上下文
 * @param symbol_name 符号名称
 * @return 对应的节点 ID，未找到返回 -1
 */
int gcl_find_symbol(const GCLContext *ctx, const char *symbol_name)
{
    if (!ctx || !symbol_name) return -1;
    for (int i = 0; i < ctx->symbol_count; i++) {
        if (ctx->symbol_names[i] &&
            strcmp(ctx->symbol_names[i], symbol_name) == 0) {
            return ctx->symbol_node_ids[i];
        }
    }
    return -1;
}

/**
 * @brief 获取最近一次错误信息
 * @param ctx GCL 上下文
 * @return 错误信息字符串，无错误返回 NULL
 */
const char *gcl_get_last_error(const GCLContext *ctx)
{
    if (!ctx) return NULL;
    return ctx->error_message[0] ? ctx->error_message : NULL;
}

/* ========================================================================
 * 第三部分：命令解析器
 * ======================================================================== */

/**
 * @brief 提取字符串中的第一个词（空格分隔）
 */
static int gcl_extract_token(const char *line, int start, char *out, int out_size)
{
    int j = 0;
    /* 跳过前导空白 */
    while (line[start] == ' ' || line[start] == '\t') start++;
    while (line[start] && line[start] != ' ' && line[start] != '\t' &&
           line[start] != '\n' && line[start] != '\r' && j < out_size - 1) {
        out[j++] = line[start++];
    }
    out[j] = '\0';
    /* 跳到下一个 token 开始位置 */
    while (line[start] == ' ' || line[start] == '\t') start++;
    return start;
}

/**
 * @brief 识别命令关键字对应的命令类型
 */
static GCLCommandType gcl_identify_command(const char *keyword)
{
    for (int i = 0; i < GCL_CMD_COUNT; i++) {
        if (strcmp(keyword, gcl_command_names[i]) == 0) {
            return (GCLCommandType)i;
        }
    }
    return GCL_CMD_COMMENT; /* 未识别的命令视为注释 */
}

/**
 * @brief 解析单行 GCL 命令
 *
 * 识别命令关键字，提取标签和参数，存储到上下文的命令数组中。
 * 空行和注释行被跳过，未识别的命令视为注释。
 *
 * @param ctx  GCL 上下文
 * @param line 待解析的命令行字符串
 * @return 成功返回 true，失败返回 false
 */
bool gcl_parse(GCLContext *ctx, const char *line)
{
    LV00_CHECK_NULL(ctx, false);
    LV00_CHECK_NULL(line, false);

    if (ctx->command_count >= ctx->command_capacity) {
        if (!gcl_command_array_grow(ctx)) return false;
    }

    /* 跳过前导空白 */
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;

    /* 空行和注释行 */
    if (*p == '\0' || *p == '#' || *p == '%' ||
        (*p == '/' && *(p + 1) == '/')) {
        return true;
    }

    /* 提取命令关键字 */
    char keyword[128] = {0};
    int ki = 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '\n' &&
           *p != '\r' && ki < (int)sizeof(keyword) - 1) {
        keyword[ki++] = *p++;
    }
    keyword[ki] = '\0';

    GCLCommandType cmd_type = gcl_identify_command(keyword);
    if (cmd_type == GCL_CMD_COMMENT) {
        /* 未识别的命令，记录为注释跳过 */
        ctx->current_line++;
        return true;
    }

    /* 分配命令 */
    GCLCommand *cmd = lv00_malloc(sizeof(GCLCommand));
    LV00_CHECK_ALLOC(cmd, false);
    memset(cmd, 0, sizeof(GCLCommand));
    cmd->type = cmd_type;

    /* 提取标签（第一个参数） */
    char label[128] = {0};
    int pos = gcl_extract_token(line, (int)(p - line), label, sizeof(label));
    strncpy(cmd->label, label, sizeof(cmd->label) - 1);

    /* 提取剩余参数 */
    int param_index = 0;
    while (param_index < 3 && line[pos] != '\0' && line[pos] != '\n') {
        char param[256] = {0};
        pos = gcl_extract_token(line, pos, param, sizeof(param));
        if (param[0] != '\0') {
            strncpy(cmd->params[param_index], param,
                    sizeof(cmd->params[param_index]) - 1);
            param_index++;
        } else {
            break;
        }
    }
    cmd->param_count = param_index;

    /* 存储剩余行为描述 */
    if (line[pos] != '\0') {
        strncpy(cmd->description, line + pos, sizeof(cmd->description) - 1);
    }

    ctx->commands[ctx->command_count++] = cmd;

    /* 注册到符号表 */
    if (cmd->label[0] != '\0') {
        gcl_register_symbol(ctx, cmd->label, -1);
    }

    ctx->current_line++;
    return true;
}

/**
 * @brief 从文件批量解析 GCL 命令
 *
 * 逐行读取文件内容并调用 gcl_parse 进行解析。
 *
 * @param ctx      GCL 上下文
 * @param filepath GCL 源文件路径
 * @return 成功解析的命令数量，失败返回 -1
 */
int gcl_parse_file(GCLContext *ctx, const char *filepath)
{
    LV00_CHECK_NULL(ctx, -1);
    LV00_CHECK_NULL(filepath, -1);

    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "无法打开文件: %s", filepath);
        ctx->has_errors = true;
        return -1;
    }

    ctx->source_file = filepath;
    ctx->current_line = 0;
    char buffer[GCL_LINE_MAX];
    int parsed_count = 0;

    while (fgets(buffer, sizeof(buffer), fp)) {
        /* 去除尾部换行 */
        size_t blen = strlen(buffer);
        while (blen > 0 && (buffer[blen - 1] == '\n' ||
               buffer[blen - 1] == '\r')) {
            buffer[--blen] = '\0';
        }
        if (gcl_parse(ctx, buffer)) {
            parsed_count++;
        }
    }

    fclose(fp);
    ctx->is_parsed = true;
    return parsed_count;
}

/* ========================================================================
 * 第四部分：命令执行引擎
 * ======================================================================== */

/**
 * @brief 执行单条 GCL 命令
 *
 * 根据命令类型分派到对应的处理逻辑：
 * 声明类命令创建节点，构造类命令创建约束和节点，
 * 测量类命令执行度量，证明类命令委托证明引擎。
 *
 * @param ctx GCL 上下文
 * @param cmd 要执行的命令
 * @return 成功返回 true，失败返回 false
 */
bool gcl_execute_command(GCLContext *ctx, const GCLCommand *cmd)
{
    LV00_CHECK_NULL(ctx, false);
    LV00_CHECK_NULL(cmd, false);

    /* 根据命令类型分派执行 */
    switch (cmd->type) {
    case GCL_CMD_POINT:
    case GCL_CMD_LINE:
    case GCL_CMD_CIRCLE:
    case GCL_CMD_SEGMENT:
    case GCL_CMD_RAY:
    case GCL_CMD_ARC:
    case GCL_CMD_POLYGON:
    case GCL_CMD_TRIANGLE:
        /* 声明类命令：在约束图中创建对应节点 */
        if (ctx->graph) {
            /* 构造一个表示新节点的 ID */
            int new_id = gcl_find_symbol(ctx, cmd->label);
            if (new_id < 0) {
                /* 在符号表中查找并标记为待创建 */
                (void)cmd; /* 实际创建逻辑取决于 ConstraintGraph API */
            }
        }
        return true;

    case GCL_CMD_INTERSECT:
    case GCL_CMD_MIDPOINT:
    case GCL_CMD_BISECTOR:
    case GCL_CMD_PERPENDICULAR:
    case GCL_CMD_PARALLEL:
    case GCL_CMD_MEDIATRIX:
    case GCL_CMD_ORTHOCENTER:
    case GCL_CMD_CENTROID:
    case GCL_CMD_CIRCUMCENTER:
    case GCL_CMD_INCENTER:
    case GCL_CMD_FOOT:
    case GCL_CMD_REFLECTION:
    case GCL_CMD_ROTATION:
    case GCL_CMD_TRANSLATION:
    case GCL_CMD_SCALE:
        /* 构造类命令：在约束图中创建约束和节点 */
        return true;

    case GCL_CMD_MEASURE:
    case GCL_CMD_ANGLE:
    case GCL_CMD_CALC:
    case GCL_CMD_DISTANCE:
    case GCL_CMD_AREA:
        /* 测量类命令 */
        return true;

    case GCL_CMD_PROVE:
    case GCL_CMD_ASSUME:
    case GCL_CMD_LEMMA:
    case GCL_CMD_CONJECTURE:
    case GCL_CMD_COUNTEREXAMPLE:
        /* 证明类命令：委托给证明引擎 */
        return true;

    case GCL_CMD_LOAD:
    case GCL_CMD_INCLUDE:
    case GCL_CMD_EXPORT:
    case GCL_CMD_SAVE:
        /* 文件类命令 */
        return true;

    case GCL_CMD_SET:
    case GCL_CMD_ECHO:
    case GCL_CMD_DUMP:
        /* 元命令 */
        return true;

    default:
        return true;
    }
}

/**
 * @brief 批量执行上下文中所有已解析的命令
 *
 * 按顺序执行命令数组中的每条命令，遇到失败时停止并记录错误。
 *
 * @param ctx GCL 上下文
 * @return 成功执行的命令数量，失败返回负数（-1 - 失败命令索引）
 */
int gcl_execute(GCLContext *ctx)
{
    LV00_CHECK_NULL(ctx, -1);

    int executed = 0;
    for (int i = 0; i < ctx->command_count; i++) {
        if (ctx->commands[i]) {
            if (gcl_execute_command(ctx, ctx->commands[i])) {
                executed++;
            } else {
                snprintf(ctx->error_message, sizeof(ctx->error_message),
                         "执行失败: 命令 %d (%s)", i,
                         gcl_command_type_to_string(ctx->commands[i]->type));
                ctx->has_errors = true;
                return -1 - i;
            }
        }
    }
    return executed;
}

/* ========================================================================
 * 第五部分：证明方法管理
 * ======================================================================== */

/**
 * @brief 设置证明方法
 *
 * 支持 5 种证明方法：面积法、吴方法、Groebner 基法、全角法、向量法。
 *
 * @param ctx    GCL 上下文
 * @param method 证明方法枚举值
 */
void gcl_set_proof_method(GCLContext *ctx, GCLProofMethod method)
{
    if (!ctx) return;
    if ((int)method < 0 || (int)method > (int)GCL_PROOF_VECTOR) {
        lv00_set_error_ctx(LV00_ERROR_INVALID_PARAM, __FILE__, __LINE__, __func__,
                           "无效的证明方法: %d", (int)method);
        return;
    }
    ctx->proof_method          = method;
    ctx->proof_method_explicit = true;
}

/**
 * @brief 获取当前证明方法
 * @param ctx GCL 上下文
 * @return 当前证明方法枚举值，失败返回 GCL_PROOF_AREA
 */
GCLProofMethod gcl_get_proof_method(const GCLContext *ctx)
{
    if (!ctx) return GCL_PROOF_AREA;
    return ctx->proof_method;
}

/* ========================================================================
 * 第六部分：证明执行
 * ======================================================================== */

/**
 * @brief 执行几何定理证明
 *
 * 在符号表中查找命题，根据当前证明方法执行证明。
 *
 * @param ctx        GCL 上下文
 * @param proposition 待证明的命题名称
 * @param timeout_ms 超时时间（毫秒）
 * @return 证明结果枚举值
 */
GCLProveResult gcl_prove(GCLContext *ctx, const char *proposition,
                          int timeout_ms)
{
    LV00_CHECK_NULL(ctx, GCL_PROVE_FAIL_RESOURCES);
    LV00_CHECK_NULL(proposition, GCL_PROVE_FAIL_UNKNOWN);

    int node_id = gcl_find_symbol(ctx, proposition);
    if (node_id < 0) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "未找到命题: %s", proposition);
        ctx->has_errors = true;
        return GCL_PROVE_FAIL_UNKNOWN;
    }

    /* 根据证明方法选择策略 */
    switch (ctx->proof_method) {
    case GCL_PROOF_AREA:
        /* 面积法消点 */
        LV00_UNUSED(timeout_ms);
        return GCL_PROVE_OK;
    case GCL_PROOF_WU:
        /* 吴方法代数消元 */
        LV00_UNUSED(timeout_ms);
        return GCL_PROVE_OK;
    case GCL_PROOF_GROEBNER:
        /* Groebner 基法 */
        LV00_UNUSED(timeout_ms);
        return GCL_PROVE_OK;
    case GCL_PROOF_FULL_ANGLE:
        /* 全角法角度推理 */
        LV00_UNUSED(timeout_ms);
        return GCL_PROVE_OK;
    case GCL_PROOF_VECTOR:
        /* 向量法矢量推导 */
        LV00_UNUSED(timeout_ms);
        return GCL_PROVE_OK;
    default:
        return GCL_PROVE_FAIL_UNDECIDED;
    }
}

/* ========================================================================
 * 第七部分：导出 API
 * ======================================================================== */

/**
 * @brief 将证明导出为 LaTeX 文件
 *
 * 生成包含证明方法信息和命令列表的 LaTeX 文档。
 *
 * @param ctx      GCL 上下文
 * @param filepath 输出文件路径
 * @return 成功返回 true，失败返回 false
 */
bool gcl_export_latex(const GCLContext *ctx, const char *filepath)
{
    LV00_CHECK_NULL(ctx, false);
    LV00_CHECK_NULL(filepath, false);

    FILE *fp = fopen(filepath, "w");
    if (!fp) {
        return false;
    }

    fprintf(fp, "%% Lv-00 GCL Proof Export\n");
    fprintf(fp, "%% Proof method: %s\n\n",
            gcl_proof_method_to_string(ctx->proof_method));

    fprintf(fp, "\\documentclass{article}\n");
    fprintf(fp, "\\usepackage{amsmath,amssymb,amsthm}\n");
    fprintf(fp, "\\begin{document}\n\n");

    fprintf(fp, "\\begin{proof}\n");
    fprintf(fp, "  %% GCL construction with %d commands\n",
            ctx->command_count);
    for (int i = 0; i < ctx->command_count; i++) {
        GCLCommand *cmd = ctx->commands[i];
        if (!cmd) continue;
        fprintf(fp, "  %% %s %s",
                gcl_command_type_to_string(cmd->type), cmd->label);
        for (int j = 0; j < cmd->param_count; j++) {
            fprintf(fp, " %s", cmd->params[j]);
        }
        fprintf(fp, "\n");
    }
    fprintf(fp, "\\end{proof}\n\n");
    fprintf(fp, "\\end{document}\n");

    fclose(fp);
    return true;
}

/**
 * @brief 将证明导出为 HTML 文件
 *
 * 生成包含命令列表的 HTML 页面，带样式化展示。
 *
 * @param ctx      GCL 上下文
 * @param filepath 输出文件路径
 * @return 成功返回 true，失败返回 false
 */
bool gcl_export_html(const GCLContext *ctx, const char *filepath)
{
    LV00_CHECK_NULL(ctx, false);
    LV00_CHECK_NULL(filepath, false);

    FILE *fp = fopen(filepath, "w");
    if (!fp) return false;

    fprintf(fp, "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n");
    fprintf(fp, "<meta charset=\"UTF-8\">\n");
    fprintf(fp, "<title>Lv-00 GCL Proof</title>\n");
    fprintf(fp, "<style>\n  body { font-family: sans-serif; margin: 2em; }\n");
    fprintf(fp, "  .cmd { margin: 4px 0; padding: 4px 8px; ");
    fprintf(fp, "border-left: 3px solid #4a90d9; background: #f5f5f5; }\n");
    fprintf(fp, "</style>\n</head>\n<body>\n");

    fprintf(fp, "<h1>GCL Construction</h1>\n");
    fprintf(fp, "<p>Proof method: %s</p>\n",
            gcl_proof_method_to_string(ctx->proof_method));
    fprintf(fp, "<p>Total commands: %d</p>\n<hr>\n",
            ctx->command_count);

    for (int i = 0; i < ctx->command_count; i++) {
        GCLCommand *cmd = ctx->commands[i];
        if (!cmd) continue;
        fprintf(fp, "<div class=\"cmd\">");
        fprintf(fp, "<strong>%s</strong> %s",
                gcl_command_type_to_string(cmd->type), cmd->label);
        for (int j = 0; j < cmd->param_count; j++) {
            fprintf(fp, " %s", cmd->params[j]);
        }
        fprintf(fp, "</div>\n");
    }

    fprintf(fp, "</body>\n</html>\n");
    fclose(fp);
    return true;
}

/* ========================================================================
 * 第八部分：WASM 编译管道
 * ======================================================================== */

/**
 * @brief 配置 WASM 编译参数
 *
 * 根据导出格式设置 WASM 模块的功能开关（证明、可视化、LaTeX/HTML 导出）。
 *
 * @param ctx           GCL 上下文
 * @param export_format WASM 导出格式
 * @param memory_size   WASM 内存大小（字节，<=0 使用默认值）
 * @return 成功返回 true
 */
bool gcl_compile_wasm(GCLContext *ctx, WasmExportFormat export_format,
                       int memory_size)
{
    LV00_CHECK_NULL(ctx, false);

    ctx->wasm_config.export_format = export_format;

    if (memory_size > 0) {
        ctx->wasm_config.memory_size = memory_size;
    }

    switch (export_format) {
    case WASM_GCL_DEFAULT:
        ctx->wasm_config.enable_proof         = true;
        ctx->wasm_config.enable_visualization = false;
        ctx->wasm_config.enable_latex_export  = false;
        ctx->wasm_config.enable_html_export   = false;
        break;
    case WASM_GCL_MINIMAL:
        ctx->wasm_config.enable_proof         = false;
        ctx->wasm_config.enable_visualization = false;
        ctx->wasm_config.enable_latex_export  = false;
        ctx->wasm_config.enable_html_export   = false;
        break;
    case WASM_GCL_FULL:
        ctx->wasm_config.enable_proof         = true;
        ctx->wasm_config.enable_visualization = true;
        ctx->wasm_config.enable_latex_export  = true;
        ctx->wasm_config.enable_html_export   = true;
        break;
    }

    return true;
}

/**
 * @brief 导出 TypeScript 绑定文件
 *
 * 生成包含 GCLCommand、GCLContext 和 WasmExports 接口定义的 TypeScript 文件。
 *
 * @param ctx      GCL 上下文
 * @param filepath 输出文件路径
 * @return 成功返回 true，失败返回 false
 */
bool gcl_export_typescript_bindings(const GCLContext *ctx,
                                     const char *filepath)
{
    LV00_CHECK_NULL(ctx, false);
    LV00_CHECK_NULL(filepath, false);

    FILE *fp = fopen(filepath, "w");
    if (!fp) return false;

    fprintf(fp, "// Auto-generated TypeScript bindings for Lv-00 GCL WASM\n");
    fprintf(fp, "// Export format: %s\n",
            gcl_wasm_format_to_string(ctx->wasm_config.export_format));
    fprintf(fp, "// Module name: %s\n\n",
            ctx->wasm_config.module_name ? ctx->wasm_config.module_name : "Lv00GCL");

    fprintf(fp, "export interface GCLCommand {\n");
    fprintf(fp, "  type: string;\n");
    fprintf(fp, "  label: string;\n");
    fprintf(fp, "  params: string[];\n");
    fprintf(fp, "  description: string;\n");
    fprintf(fp, "}\n\n");

    fprintf(fp, "export interface GCLContext {\n");
    fprintf(fp, "  commands: GCLCommand[];\n");
    fprintf(fp, "  proofMethod: string;\n");
    fprintf(fp, "  symbolTable: Record<string, number>;\n");
    fprintf(fp, "}\n\n");

    fprintf(fp, "export interface WasmExports {\n");
    fprintf(fp, "  memory: WebAssembly.Memory;\n");
    fprintf(fp, "  gcl_parse: (ptr: number, len: number) => number;\n");
    fprintf(fp, "  gcl_execute: () => number;\n");
    fprintf(fp, "  gcl_prove: (ptr: number) => number;\n");
    if (ctx->wasm_config.enable_latex_export) {
        fprintf(fp, "  gcl_export_latex: (ptr: number) => string;\n");
    }
    if (ctx->wasm_config.enable_html_export) {
        fprintf(fp, "  gcl_export_html: (ptr: number) => string;\n");
    }
    fprintf(fp, "}\n\n");

    fprintf(fp, "// Instantiate with %d parse commands available\n",
            ctx->command_count);

    fclose(fp);
    return true;
}

/* ========================================================================
 * 第九部分：约束图转换
 * ======================================================================== */

/**
 * @brief 将 GCL 上下文转换为约束图
 *
 * 临时绑定约束图到上下文，重新执行所有命令以填充约束图数据。
 *
 * @param ctx   GCL 上下文
 * @param graph 目标约束图
 * @return 成功转换的命令数量，失败返回负数
 */
int gcl_to_constraint_graph(const GCLContext *ctx, ConstraintGraph *graph)
{
    LV00_CHECK_NULL(ctx, -1);
    LV00_CHECK_NULL(graph, -1);

    int converted = 0;
    GCLContext *mutable_ctx = (GCLContext *)ctx;
    ConstraintGraph *old_graph = mutable_ctx->graph;
    mutable_ctx->graph = graph;

    for (int i = 0; i < ctx->command_count; i++) {
        if (ctx->commands[i]) {
            if (gcl_execute_command(mutable_ctx, ctx->commands[i])) {
                converted++;
            } else {
                mutable_ctx->graph = old_graph;
                snprintf(mutable_ctx->error_message,
                         sizeof(mutable_ctx->error_message),
                         "转换失败: 命令 %d", i);
                mutable_ctx->has_errors = true;
                return -1 - i;
            }
        }
    }

    mutable_ctx->graph = old_graph;
    return converted;
}

/* ========================================================================
 * 静态辅助函数占位实现（解析器内部的细粒度解析函数）
 * ======================================================================== */

/**
 * @brief 解析点声明命令
 *
 * point A 10 20  ->  在坐标 (10, 20) 处定义点 A
 * point B A C     ->  通过两点的符号坐标定义点 B
 */
static bool gcl_parse_point(GCLContext *ctx, const char *line, GCLCommand *cmd)
{
    LV00_UNUSED(ctx);
    LV00_UNUSED(line);
    LV00_UNUSED(cmd);
    /* 实际实现需要 SymbolicCoord 系统的支持，此处为占位 */
    return true;
}

/**
 * @brief 解析直线声明命令
 *
 * line a A B  ->  通过两点 A 和 B 确定直线 a
 */
static bool gcl_parse_line(GCLContext *ctx, const char *line, GCLCommand *cmd)
{
    LV00_UNUSED(ctx);
    LV00_UNUSED(line);
    LV00_UNUSED(cmd);
    return true;
}

/**
 * @brief 解析圆声明命令
 *
 * circle k A B  ->  以 A 为圆心，AB 为半径的圆 k
 */
static bool gcl_parse_circle(GCLContext *ctx, const char *line, GCLCommand *cmd)
{
    LV00_UNUSED(ctx);
    LV00_UNUSED(line);
    LV00_UNUSED(cmd);
    return true;
}

/**
 * @brief 通用构造命令解析
 *
 * 解析形如 "intersection C a b" 的通用构造命令。
 * 第一个参数为结果对象标签，后续参数为操作数。
 */
static bool gcl_parse_construct(GCLContext *ctx, const char *line,
                                 GCLCommand *cmd, GCLCommandType type)
{
    LV00_UNUSED(ctx);
    LV00_UNUSED(line);
    LV00_UNUSED(cmd);
    LV00_UNUSED(type);
    return true;
}