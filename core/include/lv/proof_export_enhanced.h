#ifndef lv_PROOF_EXPORT_ENHANCED_H
#define lv_PROOF_EXPORT_ENHANCED_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 导出目标格式枚举 */
typedef enum {
    EXPORT_HTML  = 0,
    EXPORT_LATEX = 1,
    EXPORT_COQ   = 2,
    EXPORT_LEAN4 = 3,
    EXPORT_JSON  = 4,
    EXPORT_DOT   = 5
} lvExportFormat;

/** 单个证明步骤 */
typedef struct {
    int         step_id;
    const char *rule;
    const char *premise;
    const char *conclusion;
    int         depth;
} lvProofStep;

/** 完整证明 */
typedef struct {
    lvProofStep *steps;
    int            n_steps;
    const char    *theorem;
} lvProof;

/** 导出配置 */
typedef struct {
    lvExportFormat format;
    bool             include_proof_trace;
    bool             include_geometry;
    bool             pretty_print;
} lvExportConfig;

/** 导出结果 */
typedef struct {
    bool        success;
    char       *output;
    size_t      output_size;
} lvExportResult;

/**
 * 主导出函数：将证明按指定格式导出。
 * @param proof  证明对象（NULL → 失败）
 * @param config 导出配置（NULL → 失败）
 * @return 堆上分配的 lvExportResult，调用者须通过 proof_export_result_destroy 释放
 */
lvExportResult *proof_export_enhanced(const lvProof *proof,
                                        const lvExportConfig *config);

/**
 * 便捷函数：从定理名创建单步证明并导出。
 * @param theorem_name 定理名称（NULL → 失败）
 * @param format       导出格式
 * @return 堆上分配的 lvExportResult
 */
lvExportResult *proof_export_from_navigator(const char *theorem_name,
                                              lvExportFormat format);

/**
 * 释放导出结果。
 * @param result 导出结果（NULL 安全）
 */
void proof_export_result_destroy(lvExportResult *result);

#ifdef __cplusplus
}
#endif

#endif /* lv_PROOF_EXPORT_ENHANCED_H */
