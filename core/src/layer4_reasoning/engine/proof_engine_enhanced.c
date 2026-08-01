/**
 * @file proof_engine_enhanced.c
 * @brief 增强证明引擎实现 —— 反证法完善与逻辑溯源树
 *
 * @details 实现增强证明引擎的全部功能模块：
 *   1. 溯源树（TraceTree）：记录证明的完整依赖链，支持路径查找和导出
 *   2. 反证法（Contradiction）：完整的矛盾推导路径搜索与验证
 *   3. 证明策略调度（Strategy）：10种策略的注册、适用性评估与自动选择
 *   4. 证明验证（Verify）：独立验证证明正确性，检查步骤合法性
 *   5. 证明优化（Optimize）：冗余步骤消除，复杂度评估
 *   6. 证明导出（Export）：自然语言、LaTeX、Coq、Isar 格式输出
 *   7. 矛盾检测（ContradictionDetect）：6种矛盾类型的自动检测
 *
 * 线程安全设计：
 *   - 所有静态状态使用线程局部存储（lv_THREAD_LOCAL）
 *   - 引擎实例本身不含共享可变状态
 *   - 节点 ID 分配使用原子递增
 *
 * 内存管理：
 *   - 统一使用 lv_malloc/lv_calloc/lv_realloc/lv_free
 *   - 所有分配失败路径均通过 lv_set_error 报告错误
 *   - 树结构的销毁采用递归释放，防止内存泄漏
 *
 * @author Lv-00 Project
 * @version 3.3.0
 *
 * @dependencies
 *   - proof_engine_enhanced.h : 增强证明引擎公共接口
 *   - proof.h                 : 现有证明系统接口
 *   - axiom_rule_engine.h     : 公理规则引擎接口
 *   - constraint_graph.h      : 约束图接口
 *   - lv_utils.h            : 统一内存分配器与错误处理
 *   - error_codes.h           : 错误码定义
 *
 * ============================================================
 * 拆分子模块（Lv-00 v3.3.0+）
 * ============================================================
 *
 * 本文件已拆分为以下子模块：
 *   - proof_engine.c    证明引擎生命周期（create/destroy/注册/统计）
 *   - proof_strategy.c  证明策略执行内核（10 种策略 + 调度）
 *   - proof_verify.c    证明验证（lv_verify_proof / lv_verify_proof_step）
 *   - proof_optimize.c  证明优化（冗余消除 / 复杂度评估）
 *   - proof_export.c    证明导出（自然语言/LaTeX/Coq/Isar）
 *
 * 本文件保留：文件头、公共常量、内部辅助函数（safe_strncpy）。
 * 内部共享声明见 proof_engine_enhanced_internal.h。
 */

#include "proof_engine_enhanced.h"
#include "proof_engine_enhanced_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/proof.h"

#include "axiom_rule_engine.h"
#include "error_codes.h"
#include "lv.h" /* lv_THREAD_LOCAL 宏定义 */
#include "three_valued_logic.h"

#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_strbuf.h"

/* ============== 内部常量与宏 ============== */

/** 溯源树初始节点容量 */

/** 溯源树节点初始子节点容量 */

/** 矛盾路径初始容量 */

/** 导出缓冲区初始大小 */
#define EXPORT_BUFFER_INITIAL_SIZE 4096

/** 导出缓冲区最大大小 */
#define EXPORT_BUFFER_MAX_SIZE (1024 * 1024)

/** 默认证明引擎最大深度（已迁移至 proof_engine.c） */
/** 默认证明引擎最大分支数（已迁移至 proof_engine.c） */
/** 默认超时时间（毫秒，已迁移至 proof_engine.c） */

/** 原子递增节点 ID（线程安全） */

/* ============== 内部辅助函数 ============== */

/**
 * @brief 在访问映射表中检查节点是否已访问（线性探测）
 *
 * 用于 lv_trace_tree_find_path 中的 DFS 路径搜索，
 * 替代 GNU statement-expression 宏以避免 -Wpedantic 警告。
 *
 * @param visited_map  访问映射表（0 = 未占用）
 * @param map_size     映射表大小
 * @param node_id      待检查的节点 ID
 * @return true 已访问，false 未访问
 */

/**
 * @brief 获取当前时间戳（纳秒级）
 * @return 当前时间戳
 */

/**
 * @brief 安全字符串复制，截断超长字符串
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param max_len 缓冲区最大长度
 *
 * 本函数与标准 strncpy 的行为差异：
 *   1. 标准 strncpy 不会保证目标字符串以 '\0' 结尾（当 src 长度 >= n 时），
 *      而本函数始终在 dest[max_len-1] 处写入 '\0'，确保结果始终为合法 C 字符串。
 *   2. 标准 strncpy 在 src 短于 n 时会用 '\0' 填充剩余空间，本函数不做填充，
 *      仅写入有效内容加终止符，性能更优。
 *   3. 本函数对 NULL 指针做了防御性检查，标准 strncpy 对 NULL 调用是未定义行为。
 */
void safe_strncpy(char *dest, const char *src, size_t max_len) {
    if (!dest || !src) {
        if (dest && max_len > 0)
            dest[0] = '\0';
        return;
    }
    strncpy(dest, src, max_len - 1);
    dest[max_len - 1] = '\0';
}

/* StringBuffer 已迁移至 lvStrBuf（lv/lv_strbuf.h） */
