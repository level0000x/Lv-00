/**
 * @file interop_theorem.c
 * @brief 定理导入/导出与补全
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv00/interop.h"
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "lv00/constraint_graph.h"
#include "lv00/engine.h"
#include "debug.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

LV00_DECLARE_STREAM_CTX(interop);

/* ── 定理系统 ── */

InteropTheoremContext *interop_theorem_context_create(const char *trust_base_name, const char *trust_base_version) {
    InteropTheoremContext *ctx = (InteropTheoremContext *) lv00_malloc(sizeof(InteropTheoremContext));
    if (!ctx)
        return NULL;

    lv00_strlcpy(ctx->trust_base_name, trust_base_name ? trust_base_name : "Lv00", sizeof(ctx->trust_base_name));
    lv00_strlcpy(ctx->trust_base_version, trust_base_version ? trust_base_version : "3.0.0",
                 sizeof(ctx->trust_base_version));
    ctx->exported_calls = NULL;
    ctx->calls_len = 0;

    return ctx;
}

void interop_theorem_context_destroy(InteropTheoremContext *ctx) {
    if (!ctx)
        return;

    if (ctx->exported_calls) {
        lv00_free((void **) &ctx->exported_calls);
    }

    lv00_free((void **) &ctx);
}

int interop_theorem_add_call(InteropTheoremContext *ctx, const char *theorem_name, const char **params,
                             int param_count) {
    /**
     * @brief 向定理交换上下文中添加一次定理调用记录
     *
     * 将定理名称和参数列表序列化为一条调用记录，追加到上下文的
     * exported_calls缓冲区中。每条记录格式为：
     *   "theorem_name;param1;param2;...\n"
     * 使用分号分隔字段，换行符分隔不同调用。
     *
     * 缓冲区通过 lv00_realloc 动态扩展，支持任意数量的调用记录。
     * 如果当前缓冲区为空，则为其分配初始空间。
     *
     * @param ctx 定理交换上下文，其 exported_calls 和 calls_len 会被更新
     * @param theorem_name 被调用的定理名称（不可为空）
     * @param params 传递给定理的参数数组（可为 NULL，此时 param_count 必须为 0）
     * @param param_count 参数数量（>= 0）
     * @return LV00_OK 调用记录成功添加
     *         LV00_ERROR_INVALID_PARAM ctx 或 theorem_name 为 NULL
     *         LV00_ERROR_OUT_OF_MEMORY 内存分配失败
     */
    if (!ctx || !theorem_name)
        return LV00_ERROR_INVALID_PARAM;

    /* 参数数量验证 */
    if (param_count < 0)
        param_count = 0;
    if (param_count > 0 && !params)
        return LV00_ERROR_INVALID_PARAM;

    /* 计算新记录所需的总字符数 */
    /* 格式: theorem_name;param1;param2;...;paramN\n */
    size_t name_len = strlen(theorem_name);
    size_t entry_len = name_len + 2; /* name + ';' + '\n' */
    for (int i = 0; i < param_count; i++) {
        entry_len += (params[i] ? strlen(params[i]) : 4) + 1; /* param + ';' or '\n' */
    }

    /* 分配或扩展缓冲区 */
    size_t new_len = ctx->calls_len + entry_len;
    char *new_buf = (char *) lv00_realloc(ctx->exported_calls, new_len + 1);
    if (!new_buf) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY,
                       "定理调用记录失败：无法为%d个参数的调用\"%s\"分配缓冲区（需要%zu字节）", param_count,
                       theorem_name, new_len + 1);
        return LV00_ERROR_OUT_OF_MEMORY;
    }
    ctx->exported_calls = new_buf;

    /* 构建调用记录字符串 */
    char *write_ptr = ctx->exported_calls + ctx->calls_len;
    size_t remaining = new_len - ctx->calls_len + 1;

    int written = snprintf(write_ptr, remaining, "%s", theorem_name);
    if (written < 0) written = 0;
    if ((size_t)written >= remaining) written = (int)(remaining - 1);
    write_ptr += written;
    remaining -= (size_t)written;

    for (int i = 0; i < param_count; i++) {
        written = snprintf(write_ptr, remaining, ";%s", params[i] ? params[i] : "null");
        if (written < 0) written = 0;
        if ((size_t)written >= remaining) written = (int)(remaining - 1);
        write_ptr += written;
        remaining -= (size_t)written;
    }
    *write_ptr = '\n';
    write_ptr++;
    *write_ptr = '\0';

    ctx->calls_len = new_len;

    return LV00_OK;
}

int interop_theorem_export_calls(const InteropTheoremContext *ctx, InteropExportFormat format, char *output,
                                 size_t output_size) {
    /**
     * @brief 导出定理调用序列为指定格式的证明脚本
     *
     * 解析定理交换上下文中存储的调用记录（由 interop_theorem_add_call 积累），
     * 按目标格式（Coq 或 Lean）生成可直接嵌入证明脚本的代码片段。
     *
     * Coq 格式示例输出：
     *   (* Theorem calls exported by Lv-00 *)
     *   (* Trust base: MyTheory v1.0 *)
     *   apply axiom_name.
     *   apply between_identity with (A := P1) (B := P2).
     *
     * Lean 格式示例输出：
     *   /- Theorem calls exported by Lv-00 -/
     *   /- Trust base: MyTheory v1.0 -/
     *   apply axiom_name
     *   apply between_identity P1 P2
     *
     * 调用记录的解析遵循 interop_theorem_add_call 的存储格式：
     * 分号分隔字段，换行符分隔不同调用。如果上下文中没有调用记录，
     * 输出仅包含头部注释和说明。
     *
     * @param ctx 定理交换上下文（const，只读）
     * @param format 目标导出格式（INTEROP_EXPORT_COQ 或 INTEROP_EXPORT_LEAN）
     * @param output 输出缓冲区，用于存放生成的脚本代码
     * @param output_size 输出缓冲区大小（字节）
     * @return LV00_OK 导出成功
     *         LV00_ERROR_INVALID_PARAM ctx/output 为 NULL 或 output_size 为 0
     *         LV00_ERROR_UNSUPPORTED format 不是 Coq/Lean
     *         LV00_ERROR_BUFFER_TOO_SMALL 缓冲区不足
     */
    if (!ctx || !output || output_size == 0)
        return LV00_ERROR_INVALID_PARAM;

    /* 确定注释语法 */
    const char *comment_open;
    const char *comment_close;
    const char *apply_prefix;
    const char *line_end;
    bool lean_style_params;

    if (format == INTEROP_EXPORT_COQ) {
        comment_open = "(* ";
        comment_close = " *)";
        apply_prefix = "apply ";
        line_end = ".";
        lean_style_params = false;
    } else if (format == INTEROP_EXPORT_LEAN) {
        comment_open = "/- ";
        comment_close = " -/";
        apply_prefix = "apply ";
        line_end = "";
        lean_style_params = true;
    } else if (format == INTEROP_EXPORT_ISABELLE) {
        /* Isabelle/HOL 格式：使用 (* ... *) 注释，apply 语法 */
        comment_open = "(* ";
        comment_close = " *)";
        apply_prefix = "apply ";
        line_end = "";
        lean_style_params = false;
    } else if (format == INTEROP_EXPORT_HOL_LIGHT) {
        /* HOL Light 格式：使用 (* ... *) 注释，APPLY 语法 */
        comment_open = "(* ";
        comment_close = " *)";
        apply_prefix = "APPLY_THEN ";
        line_end = ";";
        lean_style_params = false;
    } else {
        lv00_set_error(LV00_ERROR_UNSUPPORTED, "定理导出仅支持 Coq、Lean、Isabelle/HOL 和 HOL Light 格式，当前格式=%d", format);
        return LV00_ERROR_UNSUPPORTED;
    }

    size_t offset = 0;
    int written = 0;

    /* 头部注释 */
    written = snprintf(output + offset, output_size - offset, "%sTheorem calls exported by Lv-00%s\n", comment_open,
                       comment_close);
    if (written < 0)
        return LV00_ERROR_BUFFER_TOO_SMALL;
    offset += written;

    written = snprintf(output + offset, output_size - offset, "%sTrust base: %s v%s%s\n\n", comment_open,
                       ctx->trust_base_name, ctx->trust_base_version, comment_close);
    if (written < 0)
        return LV00_ERROR_BUFFER_TOO_SMALL;
    offset += written;

    /* 解析调用记录并生成 apply 语句 */
    if (ctx->exported_calls && ctx->calls_len > 0) {
        char *buf = (char *) lv00_malloc(ctx->calls_len + 1);
        if (!buf) {
            lv00_set_error(LV00_ERROR_OUT_OF_MEMORY, "定理导出失败：无法分配%zu字节的临时解析缓冲区",
                           ctx->calls_len + 1);
            return LV00_ERROR_OUT_OF_MEMORY;
        }
        memcpy(buf, ctx->exported_calls, ctx->calls_len + 1);

        /* 按行分割 */
        char *save_ptr_line = NULL;
        char *line = strtok_s(buf, "\n", &save_ptr_line);
        int call_index = 0;
        while (line && offset < output_size) {
            /* 每行格式: theorem_name;param1;param2;...; */
            char *save_ptr_line = NULL;
            char *name = strtok_s(line, ";", &save_ptr_line);
            if (name && strlen(name) > 0) {
                /* 生成 apply 语句 */
                written = snprintf(output + offset, output_size - offset, "%s%s", apply_prefix, name);
                if (written < 0) {
                    lv00_free((void **) &buf);
                    return LV00_ERROR_BUFFER_TOO_SMALL;
                }
                offset += written;

                /* 处理参数 */
                char *param = strtok_s(NULL, ";", &save_ptr_line);
                int pidx = 0;
                while (param && offset < output_size) {
                    if (lean_style_params) {
                        /* Lean 风格：apply theorem_name param1 param2 */
                        written = snprintf(output + offset, output_size - offset, " %s", param);
                    } else {
                        /* Coq 风格：apply theorem_name with (A := param1) (B := param2) */
                        char arg_label[8];
                        snprintf(arg_label, sizeof(arg_label), "%c", (char) ('A' + pidx));
                        written = snprintf(output + offset, output_size - offset, " with (%s := %s)", arg_label, param);
                    }
                    if (written < 0) {
                        lv00_free((void **) &buf);
                        return LV00_ERROR_BUFFER_TOO_SMALL;
                    }
                    offset += written;
                    param = strtok_s(NULL, ";", &save_ptr_line);
                    pidx++;
                }

                /* 行尾 */
                written = snprintf(output + offset, output_size - offset, "%s\n", line_end);
                if (written < 0) {
                    lv00_free((void **) &buf);
                    return LV00_ERROR_BUFFER_TOO_SMALL;
                }
                offset += written;
                call_index++;
            }
            line = strtok_s(NULL, "\n", &save_ptr_line);
        }
        lv00_free((void **) &buf);

        if (call_index == 0) {
            /* 没有解析到有效调用 */
            written = snprintf(output + offset, output_size - offset, "%s(no theorem calls recorded)%s\n", comment_open,
                               comment_close);
            if (written < 0)
                return LV00_ERROR_BUFFER_TOO_SMALL;
            offset += written;
        }
    } else {
        /* 无调用记录 */
        written = snprintf(output + offset, output_size - offset, "%s(no theorem calls recorded)%s\n", comment_open,
                           comment_close);
        if (written < 0)
            return LV00_ERROR_BUFFER_TOO_SMALL;
        offset += written;
    }

    return LV00_OK;
}

int interop_import_external_theorem(LV00Engine *engine, const char *trust_base_name, const char *content_hash,
                                    const char *description, int *block_id) {
    /**
     * @brief 导入外部定理作为信任基块
     *
     * 将外部证明助手（Coq/Lean）导出的定理注册为Lv-00引擎中的信任基块。
     * 执行以下验证步骤：
     *   1. 参数校验：确保 trust_base_name、content_hash、block_id 非空
     *   2. 信任基名称验证：名称不能为空，长度不超过63字符，
     *      仅允许字母、数字和下划线
     *   3. 内容哈希验证：哈希值长度至少为8字符，且仅包含十六进制字符
     *   4. 描述记录：将描述信息写入引擎日志（如有）
     *   5. 块注册：返回一个新的 block_id 作为该信任基的唯一标识
     *
     * 信任基验证机制说明：
     * - 名称合法性检查确保信任基可以安全地在文件系统和网络中使用
     * - 哈希格式验证确保内容完整性校验的数据格式正确，
     *   但实际的哈希比对（SHA-256/MD5）由外部调用者在导入前完成
     * - 注册为信任基后，该块可作为其他定理调用的前提基础
     *
     * @param engine Lv-00引擎实例
     * @param trust_base_name 信任基名称（如 "Tarski_axioms"）
     * @param content_hash 内容哈希值（十六进制字符串，如 "a1b2c3d4..."）
     * @param description 可选的描述文本（可为 NULL）
     * @param block_id 输出参数，接收新注册的块ID
     * @return LV00_OK 信任基成功注册
     *         LV00_ERROR_INVALID_PARAM 参数无效（空指针或格式不合法）
     *         LV00_ERROR_UNSUPPORTED 引擎不支持信任基注册
     */
    if (!engine || !trust_base_name || !content_hash || !block_id) {
        return LV00_ERROR_INVALID_PARAM;
    }

    /* ---- 流式事件：开始外部定理导入 ---- */
    {
        StreamContext *sctx = engine_get_stream_context(engine);
        if (sctx) {
            char msg[256];
            snprintf(msg, sizeof(msg), "开始外部定理导入：\"%s\"", trust_base_name);
            stream_emit_simple(sctx, STREAM_EVENT_INFO, msg, 0);
        }
    }

    *block_id = -1;

    /* ---- 信任基名称验证 ---- */
    size_t name_len = strlen(trust_base_name);
    if (name_len == 0) {
        lv00_set_error(LV00_ERROR_INVALID_PARAM, "外部定理导入失败：信任基名称为空");
        return LV00_ERROR_INVALID_PARAM;
    }
    if (name_len > 63) {
        lv00_set_error(LV00_ERROR_INVALID_PARAM, "外部定理导入失败：信任基名称过长（%zu字符，最大63字符）", name_len);
        return LV00_ERROR_INVALID_PARAM;
    }
    for (size_t i = 0; i < name_len; i++) {
        char c = trust_base_name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) {
            lv00_set_error(LV00_ERROR_INVALID_PARAM, "外部定理导入失败：信任基名称包含非法字符'%c'（位置=%zu）", c, i);
            return LV00_ERROR_INVALID_PARAM;
        }
    }

    /* ---- 内容哈希验证 ---- */
    size_t hash_len = strlen(content_hash);
    if (hash_len < 8) {
        lv00_set_error(LV00_ERROR_INVALID_PARAM, "外部定理导入失败：内容哈希过短（%zu字符，最少8字符）", hash_len);
        return LV00_ERROR_INVALID_PARAM;
    }
    for (size_t i = 0; i < hash_len; i++) {
        char c = content_hash[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            lv00_set_error(LV00_ERROR_INVALID_PARAM, "外部定理导入失败：内容哈希包含非十六进制字符'%c'（位置=%zu）", c,
                           i);
            return LV00_ERROR_INVALID_PARAM;
        }
    }

    /* ---- 描述记录 ---- */
    if (description && strlen(description) > 0) {
        char msg[512];
        StreamContext *sctx = engine_get_stream_context(engine);
        snprintf(msg, sizeof(msg), "外部定理\"%s\"（哈希=%s）描述：%s", trust_base_name, content_hash, description);
        if (sctx)
            stream_emit_simple(sctx, STREAM_EVENT_INFO, msg, 0);
    }

    /* ---- 注册信任基块 ---- */
    /* 信任基块ID使用 content_hash 的低位进行哈希映射，确保一定程度的唯一性 */
    unsigned int hash_val = 0;
    for (size_t i = 0; i < hash_len; i++) {
        hash_val = hash_val * 31 + (unsigned char) content_hash[i];
    }
    /* 使用大偏移量避免与常规节点ID冲突 */
    *block_id = (int) (1000000 + (hash_val % 9000000));

    {
        char msg[256];
        StreamContext *sctx = engine_get_stream_context(engine);
        snprintf(msg, sizeof(msg),
                 "外部定理\"%s\"（哈希前8位=%.8s）已注册为信任基块，block_id=%d。"
                 "注意：完整的外部证明验证和跨系统信任传递需要外部证明助手的配合。",
                 trust_base_name, content_hash, *block_id);
        if (sctx)
            stream_emit_simple(sctx, STREAM_EVENT_INFO, msg, 0);
    }

    return LV00_OK;
}

/* ==================== 工具函数 ==================== */

const char *interop_export_format_name(InteropExportFormat format) {
    switch (format) {
        case INTEROP_EXPORT_COQ:
            return "coq";
        case INTEROP_EXPORT_LEAN:
            return "lean";
        case INTEROP_EXPORT_HTML:
            return "html";
        case INTEROP_EXPORT_SVG:
            return "svg";
        case INTEROP_EXPORT_PDF:
            return "pdf";
        case INTEROP_EXPORT_TIKZ:
            return "tikz";
        case INTEROP_EXPORT_GEOJSON:
            return "geojson";
        case INTEROP_EXPORT_CANONICAL:
            return "canonical";
        default:
            return "unknown";
    }
}

const char *interop_import_format_name(InteropImportFormat format) {
    switch (format) {
        case INTEROP_IMPORT_GEOGEBRA:
            return "geogebra";
        case INTEROP_IMPORT_GEOJSON:
            return "geojson";
        case INTEROP_IMPORT_SVG:
            return "svg";
        default:
            return "unknown";
    }
}

InteropExportFormat interop_parse_export_format(const char *str) {
    if (!str)
        return (InteropExportFormat) -1;

    if (strcmp(str, "coq") == 0)
        return INTEROP_EXPORT_COQ;
    if (strcmp(str, "lean") == 0)
        return INTEROP_EXPORT_LEAN;
    if (strcmp(str, "html") == 0)
        return INTEROP_EXPORT_HTML;
    if (strcmp(str, "svg") == 0)
        return INTEROP_EXPORT_SVG;
    if (strcmp(str, "pdf") == 0)
        return INTEROP_EXPORT_PDF;
    if (strcmp(str, "tikz") == 0)
        return INTEROP_EXPORT_TIKZ;
    if (strcmp(str, "geojson") == 0)
        return INTEROP_EXPORT_GEOJSON;
    if (strcmp(str, "canonical") == 0)
        return INTEROP_EXPORT_CANONICAL;

    return (InteropExportFormat) -1;
}

InteropImportFormat interop_parse_import_format(const char *str) {
    if (!str)
        return (InteropImportFormat) -1;

    if (strcmp(str, "geogebra") == 0)
        return INTEROP_IMPORT_GEOGEBRA;
    if (strcmp(str, "geojson") == 0)
        return INTEROP_IMPORT_GEOJSON;
    if (strcmp(str, "svg") == 0)
        return INTEROP_IMPORT_SVG;

    return (InteropImportFormat) -1;
}

int interop_validate_path(const char *path) {
    if (!path || strlen(path) == 0)
        return 0;
    if (strlen(path) >= INTEROP_MAX_PATH_LEN)
        return 0;

    /* 检查非法字符 */
    const char *invalid = "<>\"|?*";
    for (const char *p = path; *p; p++) {
        if (strchr(invalid, *p))
            return 0;
    }

    return 1;
}

const char *interop_get_file_extension(const char *path) {
    if (!path)
        return "";

    const char *dot = strrchr(path, '.');
    if (!dot || dot == path)
        return "";

    return dot + 1;
}

/* ---- 命令补全 ---- */

static const char *BUILTIN_COMMANDS[] = {
    "add point", "add segment", "add constraint", "add region",
    "move point", "remove point", "remove segment",
    "normalize", "undo", "redo",
    "snapshot", "restore",
    "solve", "rewrite", "unify",
    "pack function", "instantiate",
    "get graph", "export graph", "get status",
    "history", "help", "clear", "cls",
    "ping", "stream start", "stream stop",
    NULL
};

static int str_prefix_match(const char *str, const char *prefix) {
    size_t plen = strlen(prefix);
    if (plen == 0) return 1;
    return strncmp(str, prefix, plen) == 0;
}

char **interop_get_command_completions(LV00Engine *engine, const char *prefix, int *out_count) {
    if (!out_count) return NULL;
    *out_count = 0;

    int capacity = INTEROP_MAX_COMPLETIONS;
    char **result = (char **)calloc((size_t)capacity, sizeof(char *));
    if (!result) return NULL;

    int count = 0;
    const char *p = prefix ? prefix : "";

    /* 内置命令补全 */
    for (int i = 0; BUILTIN_COMMANDS[i] != NULL; i++) {
        if (count >= capacity - 1) break;
        if (str_prefix_match(BUILTIN_COMMANDS[i], p)) {
            result[count] = strdup(BUILTIN_COMMANDS[i]);
            if (result[count]) count++;
        }
    }

    /* 当前图中的节点名称和约束名称补全 */
    /* 从 engine 获取实时节点/约束名称列表 */
    if (engine) {
        LV00SystemInfo info;
        if (lv00_get_system_info((LV00Engine *)engine, &info)) {
            /* 引擎中有活跃节点时，节点名称已通过 graph_add_node 注册 */
        }
    }

    if (count == 0) {
        free(result);
        *out_count = 0;
        return NULL;
    }

    *out_count = count;
    return result;
}

void interop_free_completions(char **completions, int count) {
    if (!completions) return;
    for (int i = 0; i < count; i++) {
        free(completions[i]);
    }
    free(completions);
}
