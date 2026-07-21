/**
 * @file meta_verify.h
 * @brief Layer 8 元验证器 —— 证明质量的 6 项检查
 *
 * @details 本模块提供 Lv-00 系统的元验证接口，对会话或证明执行
 *          6 项质量检查：结构完整性、类型一致性、完备性、可靠性、
 *          非平凡性、往返验证。
 *
 * @author Lv-00 Project
 * @version 1.1.0
 */

#ifndef LV00_META_VERIFY_H
#define LV00_META_VERIFY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv00/orchestrator.h"

/* ============================================================
 * 枚举定义
 * ============================================================ */

/**
 * @brief 元验证检查项枚举
 */
typedef enum {
    LV00_CHECK_STRUCTURAL = 0,  /**< 检查 1: 结构完整性 */
    LV00_CHECK_TYPE,             /**< 检查 2: 类型一致性 */
    LV00_CHECK_COMPLETE,         /**< 检查 3: 完备性 */
    LV00_CHECK_SOUND,            /**< 检查 4: 可靠性 */
    LV00_CHECK_NONTRIVIAL,       /**< 检查 5: 非平凡性 */
    LV00_CHECK_ROUNDTRIP,        /**< 检查 6: 往返验证 */
    LV00_CHECK_COUNT             /**< 检查项总数 */
} Lv00VerifyCheck;

/* ============================================================
 * 结构体定义
 * ============================================================ */

/**
 * @brief 单项检查结果（元验证专用）
 */
typedef struct {
    Lv00VerifyCheck check;       /**< 检查项标识 */
    int passed;                   /**< 1=通过, 0=失败, -1=跳过 */
    char description[512];       /**< 检查描述/诊断信息 */
} Lv00MetaVerifyResult;

/**
 * @brief 元验证报告
 */
typedef struct {
    int total_checks;             /**< 总检查数 */
    int passed_checks;            /**< 通过数 */
    int failed_checks;            /**< 失败数 */
    int skipped_checks;           /**< 跳过数 */
    Lv00MetaVerifyResult results[LV00_CHECK_COUNT]; /**< 各项结果 */
    char summary[1024];           /**< 摘要 */
} Lv00VerifyReport;

/**
 * @brief 元验证器
 */
typedef struct Lv00MetaVerifier {
    unsigned int check_mask;      /**< 启用的检查位掩码 */
    int strict_mode;              /**< 严格模式 */
} Lv00MetaVerifier;

/* ============================================================
 * 公共接口
 * ============================================================ */

/**
 * @brief 创建元验证器
 * @return 成功返回元验证器指针，失败返回 NULL
 */
Lv00MetaVerifier *lv00_meta_verifier_create(void);

/**
 * @brief 销毁元验证器
 * @param verifier 元验证器指针
 */
void lv00_meta_verifier_destroy(Lv00MetaVerifier *verifier);

/**
 * @brief 启用指定检查项
 * @param verifier 元验证器指针
 * @param check 要启用的检查项
 */
void lv00_meta_verifier_enable_check(Lv00MetaVerifier *verifier, Lv00VerifyCheck check);

/**
 * @brief 禁用指定检查项
 * @param verifier 元验证器指针
 * @param check 要禁用的检查项
 */
void lv00_meta_verifier_disable_check(Lv00MetaVerifier *verifier, Lv00VerifyCheck check);

/**
 * @brief 设置严格模式
 * @param verifier 元验证器指针
 * @param strict 非零启用严格模式，零关闭
 */
void lv00_meta_verifier_set_strict(Lv00MetaVerifier *verifier, int strict);

/**
 * @brief 对会话执行元验证
 * @param verifier 元验证器指针
 * @param session 会话指针
 * @return 返回元验证报告
 */
Lv00VerifyReport lv00_meta_verify_session(Lv00MetaVerifier *verifier, const Lv00Session *session);

/**
 * @brief 对证明执行元验证
 * @param verifier 元验证器指针
 * @param proof 证明对象指针
 * @return 返回元验证报告
 */
Lv00VerifyReport lv00_meta_verify_proof(Lv00MetaVerifier *verifier, void *proof);

/**
 * @brief 查询报告是否全部通过
 * @param report 验证报告指针
 * @return 全部通过返回 1，否则返回 0
 */
int lv00_verify_report_passed(const Lv00VerifyReport *report);

/**
 * @brief 获取报告摘要
 * @param report 验证报告指针
 * @return 返回报告摘要字符串
 */
const char *lv00_verify_report_summary(const Lv00VerifyReport *report);

/**
 * @brief 获取指定检查项的结果
 * @param report 验证报告指针
 * @param check 检查项标识
 * @return 返回对应检查结果指针
 */
const Lv00MetaVerifyResult *lv00_verify_report_result(const Lv00VerifyReport *report, Lv00VerifyCheck check);

#ifdef __cplusplus
}
#endif

#endif /* LV00_META_VERIFY_H */
