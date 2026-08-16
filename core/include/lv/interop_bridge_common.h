#ifndef lv_INTEROP_BRIDGE_COMMON_H
#define lv_INTEROP_BRIDGE_COMMON_H

#include <ctype.h>
#include <string.h>

#include "lv/lv_utils.h"

#include "lv/interop.h"      /* lvPlugin / lvExternalSystem / lv_interop_register_plugin */
#include "lv/lv_check.h"     /* lv_CHECK_NOT_NULL / lv_CHECK_ARG / lv_RETURN_ERROR */
#include "lv/lv_str_utils.h" /* lv_str_ltrim */
#include "lv/lv_strbuf.h"    /* lvStrBuf / lv_strbuf_* */
#include "lv/proof.h"        /* ProofNavigator / ProofStep / ProofStepType */

/* 注意：本头不 include lv/interop_step_type.h——
 * Coq 桥接使用本地步骤枚举（NORMALIZATION 等数值分叉，互操作外部契约豁免），
 * include 共享枚举头会与本地 lv_STEP_* 值符号冲突。步骤类型映射由调用方
 * （coq_map_step_type / lv_proof_type_to_interop）作为 map_type 回调提供。 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 证明步骤结构体（Coq / Lean 4 桥接共用）
 *
 * 表示证明中的单个步骤，包含类型、描述文本和序号。
 */
typedef struct {
    int type;              /**< 步骤类型 */
    char description[512]; /**< 步骤描述（tactic 名称） */
    int id;                /**< 步骤编号（按导入顺序） */
} lvProofStep;

/**
 * @brief 桥接证明结构体（Coq / Lean 4 共用）
 *
 * 用于证明脚本的导入/导出中间表示。
 * 包含定理名称和动态增长的步骤数组。
 */
typedef struct {
    char theorem_name[256]; /**< 定理名称 */
    lvDArray steps_da;      /**< 步骤动态数组 */
} lvBridgeProof;

/* ============================================================
 * 证明器桥接公共 helper（Coq / Lean 4 共用）
 *
 * coq_bridge.c 与 lean4_bridge.c 的 export/register 骨架逐行镜像，
 * 此处收敛为公共 static inline 实现（与 lv_xmacro.h 同风格）。
 * import/validate 因两语言语法差异较大（Lean 4 为递归解析器 +
 * 类型签名校验），各自保留在桥接文件中。
 * ============================================================ */

/** @brief tactic 映射表条目（Lv-00 步骤类型 → 证明器 tactic 名） */
typedef struct {
    int step_type;        /**< Lv-00 步骤类型 */
    const char *tactic;   /**< 证明器 tactic 名 */
} lvBridgeTacticMap;

/** @brief 证明导出骨架参数（抽象 Coq .v 与 Lean .lean 的语法差异） */
typedef struct {
    const char *header;         /**< 输出头（含关键字/import 声明） */
    const char *name_suffix;    /**< 定理名后的声明段 */
    const char *footer;         /**< 输出尾 */
    const char *line_prefix;    /**< 每个 tactic 行的前缀 */
    const char *line_suffix;    /**< 每个 tactic 行的后缀（含换行） */
    const char *default_tactic; /**< 未映射步骤的默认 tactic */
    const lvBridgeTacticMap *tactic_map; /**< 步骤类型 → tactic 映射表 */
    int tactic_count;           /**< 映射表条目数 */
} lvBridgeExportSpec;

/**
 * @brief 公共证明导出骨架
 *
 * 收敛 coq_export_proof / lean4_export_proof 的逐行同构骨架：
 * 头 + 定理名 + 声明段 + 每步一行 tactic + 尾，语法差异经
 * lvBridgeExportSpec 注入，输出与原实现逐字节一致。
 *
 * @param[in]  proof         lvBridgeProof 指针（内部证明结构体）
 * @param[out] output        输出缓冲区（用于写入证明器脚本）
 * @param[in]  output_size   输出缓冲区大小（字节）
 * @param[in]  spec          导出骨架参数（header/footer/tactic 映射等）
 * @return 成功返回 0，参数无效或缓冲区不足返回 -1
 */
static inline int bridge_export_proof(void *proof, char *output, int output_size, const lvBridgeExportSpec *spec) {
    lv_CHECK_NOT_NULL(proof);
    lv_CHECK_NOT_NULL(output);
    lv_CHECK_ARG(output_size > 0, lv_ERROR_INVALID_PARAM, "invalid output_size");
    lv_CHECK_NOT_NULL(spec);

    lvBridgeProof *p = (lvBridgeProof *) proof;

    /* 使用 lvStrBuf 动态构建脚本 */
    lvStrBuf sb = {0};
    lv_strbuf_printf(&sb, "%s", spec->header);
    lv_strbuf_printf(&sb, "%s", p->theorem_name);
    lv_strbuf_printf(&sb, "%s", spec->name_suffix);

    /* 遍历每个步骤，在映射表中查找对应 tactic 并逐行输出 */
    for (int i = 0; i < p->steps_da.count; i++) {
        lvProofStep *step = (lvProofStep *) lv_darray_get(&p->steps_da, i);
        const char *tac = spec->default_tactic;
        for (int j = 0; j < spec->tactic_count; j++) {
            if (step->type == spec->tactic_map[j].step_type) {
                tac = spec->tactic_map[j].tactic;
                break;
            }
        }
        lv_strbuf_printf(&sb, "%s%s%s", spec->line_prefix, tac, spec->line_suffix);
    }

    /* 写入尾部 */
    lv_strbuf_printf(&sb, "%s", spec->footer);

    /* 拷贝到调用方缓冲区（lvStrBuf 保证 NUL 结尾），并清理 */
    if (sb.len >= (size_t) output_size)
        lv_RETURN_ERROR(lv_ERROR_IO, "output buffer too small for export");
    memcpy(output, lv_strbuf_cstr(&sb), sb.len + 1);
    lv_strbuf_destroy(&sb);
    return 0;
}

/**
 * @brief 提取定理名（关键字后的第一个标识符，到空白或 ':' 为止）
 *
 * 收敛 coq_import_proof / lean4_import_proof 中相同的定理名提取逻辑。
 *
 * @param[in]  after_kw     关键字（Theorem/theorem）之后的位置
 * @param[out] out_name     输出：定理名起始指针（可为 NULL）
 * @param[out] out_name_len 输出：定理名长度（可为 NULL）
 * @return 0 成功；-1 输入无效或定理名为空
 */
static inline int bridge_extract_theorem_name(const char *after_kw, const char **out_name, size_t *out_name_len) {
    if (!after_kw)
        return -1;
    const char *name_start = lv_str_ltrim((char *) after_kw); /* lv_str_ltrim 不修改原串 */
    const char *name_end = name_start;
    while (*name_end && !isspace((unsigned char) *name_end) && *name_end != ':')
        name_end++;
    if (name_end == name_start)
        return -1;
    if (out_name)
        *out_name = name_start;
    if (out_name_len)
        *out_name_len = (size_t) (name_end - name_start);
    return 0;
}

/**
 * @brief 公共互操作插件注册骨架
 *
 * 收敛 lv_register_coq_plugin / lv_register_lean4_plugin 的逐行同构
 * 注册逻辑（插件名/版本/外部系统/三个回调），仅参数不同。
 *
 * @param[in] mgr          互操作管理器指针
 * @param[in] name         插件名称（如 "coq" / "lean4"）
 * @param[in] version      插件版本号（如 "8.18" / "4.14.0"）
 * @param[in] system       外部系统类型（lv_EXT_COQ / lv_EXT_LEAN4）
 * @param[in] export_proof 导出证明回调
 * @param[in] import_proof 导入证明回调
 * @param[in] validate     校验回调
 * @return 成功返回 0，mgr 为 NULL 返回 -1
 */
static inline int bridge_register(lvInteropManager *mgr, const char *name, const char *version, lvExternalSystem system,
                                  int (*export_proof)(void *, char *, int), int (*import_proof)(const char *, void **),
                                  int (*validate)(const char *)) {
    lv_CHECK_NOT_NULL(mgr);
    lvPlugin plugin;
    memset(&plugin, 0, sizeof(plugin));
    lv_strlcpy(plugin.name, name, sizeof(plugin.name));
    lv_strlcpy(plugin.version, version, sizeof(plugin.version));
    plugin.system = system;
    plugin.export_proof = export_proof;
    plugin.import_proof = import_proof;
    plugin.validate = validate;
    return lv_interop_register_plugin(mgr, &plugin);
}

/* ============================================================
 * ProofNavigator → 桥接内部表示（插件体系接线，批次 C-⑰-补③）
 *
 * lvPlugin.export_proof 统一接受 ProofNavigator*（跨插件一致语义）；
 * 各插件内部把导航器转换为自己的内部证明结构（lvBridgeProof / lvOpmlProof）。
 * 步骤类型映射（ProofStepType → 目标枚举）由调用方以 map_type 回调提供：
 * - Coq：coq_map_step_type（本地枚举，数值分叉豁免）
 * - Lean4 / OPML：lv_proof_type_to_interop（interop_step_type.h）
 * ============================================================ */

/**
 * @brief 从 ProofNavigator 构造 lvBridgeProof（导出侧共享骨架）
 *
 * 遍历导航器证明步骤填入桥接内部表示：theorem_name 取 strategy_note
 * （无则 "lv_proof"），步骤 id/type/description（node/constraint/rule ID 摘要）。
 * 步骤类型经 map_type 回调映射（NULL 时按 ORACLE 兜底）。
 *
 * @param[in]  nav       证明导航器（NULL 返回 -1）
 * @param[out] out       输出结构（调用方负责 lv_darray_free(&out->steps_da)）
 * @param[in]  map_type  步骤类型映射回调（NULL 时全部映射为 ORACLE）
 * @return 0 成功；-1 参数无效
 */
static inline int bridge_proof_from_navigator(const ProofNavigator *nav, lvBridgeProof *out,
                                              int (*map_type)(ProofStepType)) {
    lv_CHECK_NOT_NULL(nav);
    lv_CHECK_NOT_NULL(out);
    memset(out, 0, sizeof(*out));
    lv_strlcpy(out->theorem_name, nav->strategy_note ? nav->strategy_note : "lv_proof",
               sizeof(out->theorem_name));
    lv_darray_init(&out->steps_da, sizeof(lvProofStep));
    for (int i = 0; i < nav->step_count; i++) {
        const ProofStep *src = nav->steps[i];
        lvProofStep step;
        memset(&step, 0, sizeof(step));
        step.id = src ? src->id : i;
        step.type = map_type ? map_type(src ? src->type : PROOF_STEP_ORACLE) : 0;
        if (src) {
            snprintf(step.description, sizeof(step.description), "step %d: node %d constraint %d rule %d", src->id,
                     src->node_id, src->constraint_id, src->rule_id);
        } else {
            snprintf(step.description, sizeof(step.description), "step %d", i);
        }
        lv_darray_push(&out->steps_da, &step);
    }
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* lv_INTEROP_BRIDGE_COMMON_H */
