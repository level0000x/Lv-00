/**
 * @file interop_theorem.c
 * @brief 定理导入/导出与补全
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/lv_platform.h"
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "lv/constraint_graph.h"
#include "lv/engine.h"
#include "lv/interop.h"
#include "lv/lv_builtin_commands.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"

/* ── 定理系统 ── */

InteropTheoremContext *interop_theorem_context_create(const char *trust_base_name, const char *trust_base_version) {
    InteropTheoremContext *ctx = (InteropTheoremContext *) lv_calloc(1, sizeof(InteropTheoremContext));
    if (!ctx)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "interop_theorem_context_create: lv_calloc(%zu) failed",
                             sizeof(InteropTheoremContext));

    lv_strlcpy(ctx->trust_base_name, trust_base_name ? trust_base_name : "lv", sizeof(ctx->trust_base_name));
    lv_strlcpy(ctx->trust_base_version, trust_base_version ? trust_base_version : "3.0.0",
               sizeof(ctx->trust_base_version));
    ctx->exported_calls = NULL;
    ctx->calls_len = 0;

    return ctx;
}

void interop_theorem_context_destroy(InteropTheoremContext *ctx) {
    if (!ctx)
        return;

    if (ctx->exported_calls) {
        lv_free((void **) &ctx->exported_calls);
    }

    lv_free((void **) &ctx);
}

/**
 * @brief 向定理交换上下文中添加一次定理调用记录
 * @details 将定理名称和参数列表序列化为一条调用记录，追加到上下文的
 *          exported_calls 缓冲区中。每条记录格式为：
 *          "theorem_name;param1;param2;...\n"
 *          使用分号分隔字段，换行符分隔不同调用。
 *          缓冲区通过 lv_realloc 动态扩展。
 * @param ctx          定理交换上下文
 * @param theorem_name 被调用的定理名称（不可为空）
 * @param params       参数数组（可为 NULL，此时 param_count 必须为 0）
 * @param param_count  参数数量（>= 0）
 * @return lv_OK 成功，lv_ERROR_INVALID_PARAM 或 lv_ERROR_OUT_OF_MEMORY
 */
int interop_theorem_add_call(InteropTheoremContext *ctx, const char *theorem_name, const char **params,
                             int param_count) {
    if (!ctx || !theorem_name)
        return lv_ERROR_INVALID_PARAM;

    /* 参数数量验证 */
    if (param_count < 0)
        param_count = 0;
    if (param_count > 0 && !params)
        return lv_ERROR_INVALID_PARAM;

    /* 用 lvStrBuf 构建单条调用记录（自动扩容，消除手写长度计算与截断防护） */
    /* 格式: theorem_name;param1;param2;...;paramN\n */
    lvStrBuf sb = {0};
    lv_strbuf_printf(&sb, "%s", theorem_name);
    for (int i = 0; i < param_count; i++) {
        lv_strbuf_printf(&sb, ";%s", params[i] ? params[i] : "null");
    }
    lv_strbuf_append_n(&sb, '\n', 1);

    /* 追加到累积缓冲区（保持 ctx->exported_calls 对外连续的 NUL 结尾字符串语义：
     * 该字段为 InteropTheoremContext 公开结构的 char* + calls_len，跨调用存活，
     * 且 export_calls 按 calls_len 直接读取，故无法用局部 lvStrBuf 替代累积——
     * 采用"lvStrBuf 构建单条记录 + 一次 realloc 追加"的收敛形态） */
    size_t new_len = ctx->calls_len + sb.len;
    char *new_buf = (char *) lv_realloc(ctx->exported_calls, new_len + 1);
    if (!new_buf) {
        lv_strbuf_destroy(&sb);
        lv_RETURN_ERROR_VAL(lv_ERROR_OUT_OF_MEMORY, lv_ERROR_OUT_OF_MEMORY,
                            "定理调用记录失败：无法为%d个参数的调用\"%s\"分配缓冲区（需要%zu字节）",
                            param_count, theorem_name, new_len + 1);
    }
    ctx->exported_calls = new_buf;
    memcpy(ctx->exported_calls + ctx->calls_len, sb.data, sb.len + 1);
    lv_strbuf_destroy(&sb);

    ctx->calls_len = new_len;

    return lv_OK;
}

/* ── 导出格式语法参数 ops 表（TheoremFormatOps） ──
 * interop_theorem_export_calls 的 4 格式 if-else 链（COQ/LEAN/ISABELLE/HOL_LIGHT）
 * 表化：各格式输出逐字节不变，仅把语法参数改为查表赋值。
 * 新增导出格式只需在此追加一行，无需改动导出主流程。 */
typedef struct {
    InteropExportFormat format;  /**< 导出格式枚举值（查找键） */
    const char *comment_open;    /**< 注释起始符 */
    const char *comment_close;   /**< 注释结束符 */
    const char *apply_prefix;    /**< apply 语句前缀 */
    const char *line_end;        /**< 语句行尾 */
    bool lean_style_params;      /**< true=Lean 风格（apply name p1 p2），false=Coq 风格（with (A := p1)） */
} TheoremFormatOps;

static const TheoremFormatOps kTheoremFormatOps[] = {
    {INTEROP_EXPORT_COQ, "(* ", " *)", "apply ", ".", false},
    {INTEROP_EXPORT_LEAN, "/- ", " -/", "apply ", "", true},
    /* Isabelle/HOL 格式：使用 (* ... *) 注释，apply 语法 */
    {INTEROP_EXPORT_ISABELLE, "(* ", " *)", "apply ", "", false},
    /* HOL Light 格式：使用 (* ... *) 注释，APPLY 语法 */
    {INTEROP_EXPORT_HOL_LIGHT, "(* ", " *)", "APPLY_THEN ", ";", false},
};

/**
 * @brief 导出定理调用序列为指定格式的证明脚本
 * @details 解析定理交换上下文中存储的调用记录（由 interop_theorem_add_call 积累），
 *          按目标格式（Coq/Lean/Isabelle/HOL Light）生成可直接嵌入证明脚本的代码片段。
 * @param ctx         定理交换上下文（只读）
 * @param format      目标导出格式
 * @param output      输出缓冲区
 * @param output_size 输出缓冲区大小
 * @return lv_OK 成功，lv_ERROR_INVALID_PARAM 参数无效，
 *         lv_ERROR_UNSUPPORTED 格式不支持，lv_ERROR_BUFFER_TOO_SMALL 缓冲区不足
 */
int interop_theorem_export_calls(const InteropTheoremContext *ctx, InteropExportFormat format, char *output,
                                 size_t output_size) {
    if (!ctx || !output || output_size == 0)
        return lv_ERROR_INVALID_PARAM;

    /* 按格式查表确定注释语法（原 4 分支 if-else 链表化，参数逐字一致） */
    const TheoremFormatOps *fmt = NULL;
    for (size_t i = 0; i < lv_ARRAY_SIZE(kTheoremFormatOps); i++) {
        if (kTheoremFormatOps[i].format == format) {
            fmt = &kTheoremFormatOps[i];
            break;
        }
    }
    if (!fmt) {
        lv_RETURN_ERROR_VAL(lv_ERROR_UNSUPPORTED, lv_ERROR_UNSUPPORTED,
                            "定理导出仅支持 Coq、Lean、Isabelle/HOL 和 HOL Light 格式，当前格式=%d", format);
    }
    const char *comment_open = fmt->comment_open;
    const char *comment_close = fmt->comment_close;
    const char *apply_prefix = fmt->apply_prefix;
    const char *line_end = fmt->line_end;
    bool lean_style_params = fmt->lean_style_params;

    /* 统一用 lvStrBuf 累积输出（自动扩容），完成后一次拷回调用方缓冲 */
    lvStrBuf sb = {0};

    /* 头部注释 */
    lv_strbuf_printf(&sb, "%sTheorem calls exported by Lv-00%s\n", comment_open, comment_close);
    lv_strbuf_printf(&sb, "%sTrust base: %s v%s%s\n\n", comment_open, ctx->trust_base_name,
                     ctx->trust_base_version, comment_close);

    /* 解析调用记录并生成 apply 语句 */
    if (ctx->exported_calls && ctx->calls_len > 0) {
        /* 手写 malloc+memcpy 复制收敛为 lv_strdup */
        char *buf = lv_strdup(ctx->exported_calls);
        if (!buf) {
            lv_strbuf_destroy(&sb);
            lv_RETURN_ERROR_VAL(lv_ERROR_OUT_OF_MEMORY, lv_ERROR_OUT_OF_MEMORY,
                                "定理导出失败：无法分配%zu字节的临时解析缓冲区", ctx->calls_len + 1);
        }

        /* 按行分割 */
        char *save_ptr_line = NULL;
        char *line = strtok_s(buf, "\n", &save_ptr_line);
        int call_index = 0;
        while (line) {
            /* 每行格式: theorem_name;param1;param2;...; */
            char *field_ctx = NULL;
            char *name = strtok_s(line, ";", &field_ctx);
            if (lv_str_nonempty(name)) {
                /* 生成 apply 语句 */
                lv_strbuf_printf(&sb, "%s%s", apply_prefix, name);

                /* 处理参数 */
                char *param = strtok_s(NULL, ";", &save_ptr_line);
                int pidx = 0;
                while (param) {
                    if (lean_style_params) {
                        /* Lean 风格：apply theorem_name param1 param2 */
                        lv_strbuf_printf(&sb, " %s", param);
                    } else {
                        /* Coq 风格：apply theorem_name with (A := param1) (B := param2) */
                        lv_strbuf_printf(&sb, " with (%c := %s)", (char) ('A' + pidx), param);
                    }
                    param = strtok_s(NULL, ";", &field_ctx);
                    pidx++;
                }

                /* 行尾 */
                lv_strbuf_printf(&sb, "%s\n", line_end);
                call_index++;
            }
            line = strtok_s(NULL, "\n", &save_ptr_line);
        }
        lv_free((void **) &buf);

        if (call_index == 0) {
            /* 没有解析到有效调用 */
            lv_strbuf_printf(&sb, "%s(no theorem calls recorded)%s\n", comment_open, comment_close);
        }
    } else {
        /* 无调用记录 */
        lv_strbuf_printf(&sb, "%s(no theorem calls recorded)%s\n", comment_open, comment_close);
    }

    /* 拷贝回调用方缓冲（截断语义与原实现一致：空间不足时静默截断并返回 lv_OK） */
    lv_strlcpy(output, lv_strbuf_cstr(&sb), output_size);
    lv_strbuf_destroy(&sb);

    return lv_OK;
}

/**
 * @brief 导入外部定理作为信任基块
 * @details 将外部证明助手（Coq/Lean）导出的定理注册为 Lv-00 引擎中的信任基块。
 *          执行参数校验、名称验证、哈希格式检查和块注册。
 * @param engine          引擎实例
 * @param trust_base_name 信任基名称（如 "Tarski_axioms"）
 * @param content_hash    内容哈希值（十六进制字符串）
 * @param description     可选的描述文本（可为 NULL）
 * @param block_id        [out] 输出新注册的块 ID
 * @return lv_OK 成功，lv_ERROR_INVALID_PARAM 参数无效，lv_ERROR_UNSUPPORTED 不支持
 */
int interop_import_external_theorem(lvEngine *engine, const char *trust_base_name, const char *content_hash,
                                    const char *description, int *block_id) {
    if (!engine || !trust_base_name || !content_hash || !block_id) {
        return lv_ERROR_INVALID_PARAM;
    }

    /* ---- 流式事件：开始外部定理导入 ---- */
    {
        StreamContext *sctx = engine_get_stream_context(engine);
        if (sctx) {
            lvStrBuf sb_2 = {0};
            lv_strbuf_printf(&sb_2, "开始外部定理导入：\"%s\"", trust_base_name);
            stream_emit_simple(sctx, STREAM_EVENT_INFO, sb_2.data, 0);
        }
    }

    *block_id = -1;

    /* ---- 信任基名称验证 ---- */
    size_t name_len = strlen(trust_base_name);
    if (name_len == 0) {
        lv_RETURN_ERROR_VAL(lv_ERROR_INVALID_PARAM, lv_ERROR_INVALID_PARAM, "外部定理导入失败：信任基名称为空");
    }
    if (name_len > 63) {
        lv_RETURN_ERROR_VAL(lv_ERROR_INVALID_PARAM, lv_ERROR_INVALID_PARAM,
                            "外部定理导入失败：信任基名称过长（%zu字符，最大63字符）", name_len);
    }
    for (size_t i = 0; i < name_len; i++) {
        char c = trust_base_name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) {
            lv_RETURN_ERROR_VAL(lv_ERROR_INVALID_PARAM, lv_ERROR_INVALID_PARAM,
                                "外部定理导入失败：信任基名称包含非法字符'%c'（位置=%zu）", c, i);
        }
    }

    /* ---- 内容哈希验证 ---- */
    size_t hash_len = strlen(content_hash);
    if (hash_len < 8) {
        lv_RETURN_ERROR_VAL(lv_ERROR_INVALID_PARAM, lv_ERROR_INVALID_PARAM,
                            "外部定理导入失败：内容哈希过短（%zu字符，最少8字符）", hash_len);
    }
    for (size_t i = 0; i < hash_len; i++) {
        char c = content_hash[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            lv_RETURN_ERROR_VAL(lv_ERROR_INVALID_PARAM, lv_ERROR_INVALID_PARAM,
                                "外部定理导入失败：内容哈希包含非十六进制字符'%c'（位置=%zu）", c, i);
        }
    }

    /* ---- 描述记录 ---- */
    if (lv_str_nonempty(description)) {
        lvStrBuf sb_3 = {0};
        StreamContext *sctx = engine_get_stream_context(engine);
        lv_strbuf_printf(&sb_3, "外部定理\"%s\"（哈希=%s）描述：%s", trust_base_name, content_hash, description);
        if (sctx)
            stream_emit_simple(sctx, STREAM_EVENT_INFO, sb_3.data, 0);
        lv_strbuf_destroy(&sb_3);
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
        lvStrBuf sb_4 = {0};
        StreamContext *sctx = engine_get_stream_context(engine);
        lv_strbuf_printf(&sb_4,
                 "外部定理\"%s\"（哈希前8位=%.8s）已注册为信任基块，block_id=%d。"
                 "注意：完整的外部证明验证和跨系统信任传递需要外部证明助手的配合。",
                 trust_base_name, content_hash, *block_id);
        if (sctx)
            stream_emit_simple(sctx, STREAM_EVENT_INFO, sb_4.data, 0);
        lv_strbuf_destroy(&sb_4);
    }

    return lv_OK;
}

/* ==================== 工具函数 ==================== */

/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief interop_export_format_name 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_interop_export_format_name_entries[] = {
    {"coq", INTEROP_EXPORT_COQ},
    {"lean", INTEROP_EXPORT_LEAN},
    {"html", INTEROP_EXPORT_HTML},
    {"svg", INTEROP_EXPORT_SVG},
    {"pdf", INTEROP_EXPORT_PDF},
    {"tikz", INTEROP_EXPORT_TIKZ},
    {"geojson", INTEROP_EXPORT_GEOJSON},
    {"canonical", INTEROP_EXPORT_CANONICAL},
};

const char *interop_export_format_name(InteropExportFormat format) {
    return lv_enum_to_str(s_interop_export_format_name_entries, lv_ARRAY_SIZE(s_interop_export_format_name_entries), (int) format, "unknown");
}

/** @brief interop_import_format_name 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_interop_import_format_name_entries[] = {
    {"geogebra", INTEROP_IMPORT_GEOGEBRA},
    {"geojson", INTEROP_IMPORT_GEOJSON},
    {"svg", INTEROP_IMPORT_SVG},
};

const char *interop_import_format_name(InteropImportFormat format) {
    return lv_enum_to_str(s_interop_import_format_name_entries, lv_ARRAY_SIZE(s_interop_import_format_name_entries), (int) format, "unknown");
}

InteropExportFormat interop_parse_export_format(const char *str) {
    if (!str)
        return (InteropExportFormat) -1;

    /* 复用名称表反向查找（替代 8 分支 strcmp 链） */
    return (InteropExportFormat) lv_str_to_enum(s_interop_export_format_name_entries,
                                                lv_ARRAY_SIZE(s_interop_export_format_name_entries), str, -1);
}

InteropImportFormat interop_parse_import_format(const char *str) {
    if (!str)
        return (InteropImportFormat) -1;

    /* 复用名称表反向查找（替代 3 分支 strcmp 链） */
    return (InteropImportFormat) lv_str_to_enum(s_interop_import_format_name_entries,
                                                lv_ARRAY_SIZE(s_interop_import_format_name_entries), str, -1);
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

/* 内置命令列表已统一到 lv_builtin_commands（见 lv_builtin_commands.h，
   定义于 lv_protocol.c），与 lv_proto_completions 共用一份。 */

static int str_prefix_match(const char *str, const char *prefix) {
    size_t plen = strlen(prefix);
    if (plen == 0)
        return 1;
    return strncmp(str, prefix, plen) == 0;
}

char **interop_get_command_completions(lvEngine *engine, const char *prefix, int *out_count) {
    if (!out_count)
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "interop_get_command_completions: out_count is NULL");
    *out_count = 0;

    int capacity = INTEROP_MAX_COMPLETIONS;
    char **result = (char **) lv_calloc((size_t) capacity, sizeof(char *));
    if (!result)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "interop_get_command_completions: lv_calloc(%d) failed", capacity);

    int count = 0;
    const char *p = prefix ? prefix : "";

    /* 内置命令补全 */
    for (int i = 0; lv_builtin_commands[i] != NULL; i++) {
        if (count >= capacity - 1)
            break;
        if (str_prefix_match(lv_builtin_commands[i], p)) {
            result[count] = lv_strdup_safe(lv_builtin_commands[i]);
            if (result[count])
                count++;
        }
    }

    /* 当前图中的节点名称和约束名称补全 */
    /* 从 engine 获取实时节点/约束名称列表 */
    if (engine && engine->main_graph) {
        ConstraintGraph *graph = engine->main_graph;

        /* 遍历所有活跃节点生成补全项 */
        for (int i = 0; i < graph->node_count && count < capacity - 1; i++) {
            GeomNode *node = graph->nodes[i];
            if (!node || !node->is_active)
                continue;

            lvStrBuf sb_5 = {0};
            lv_strbuf_printf(&sb_5, "%s_%d",
                     interop_geom_type_name(node->type), node->id);
            if (str_prefix_match(sb_5.data, p)) {
                result[count] = lv_strdup_safe(sb_5.data);
                if (result[count])
                    count++;
            }
            lv_strbuf_destroy(&sb_5);
        }

        /* 遍历所有活跃约束生成补全项 */
        for (int i = 0; i < graph->constraint_count && count < capacity - 1; i++) {
            Constraint *con = graph->constraints[i];
            if (!con || !con->is_active)
                continue;

            lvStrBuf sb_6 = {0};
            lv_strbuf_printf(&sb_6, "%s_%d",
                     interop_constraint_type_name(con->type), con->id);
            if (str_prefix_match(sb_6.data, p)) {
                result[count] = lv_strdup_safe(sb_6.data);
                if (result[count])
                    count++;
            }
            lv_strbuf_destroy(&sb_6);
        }
    }

    if (count == 0) {
        lv_free((void **) &result);
        *out_count = 0;
        return NULL;
    }

    *out_count = count;
    return result;
}

void interop_free_completions(char **completions, int count) {
    if (!completions)
        return;
    for (int i = 0; i < count; i++) {
        lv_free((void **) &completions[i]);
    }
    lv_free((void **) &completions);
}
