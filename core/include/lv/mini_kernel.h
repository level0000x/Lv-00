/**
 * @file mini_kernel.h
 * @brief 极简验证内核 —— 借鉴 mm0/Metamath 的超小型可信计算基（TCB）
 *
 * 设计借鉴：
 * - mm0 (github.com/digama0/mm0)
 *   · 验证器不到 2000 行 C 代码
 *   · 只做替换检查（substitution check），不内建任何逻辑
 *   · 四类语句：$f（变量声明）/ $e（前提）/ $a（公理）/ $p（定理）
 *   · 独立验证器理念：验证器不需理解"数学"，只理解"替换"
 * - Metamath (github.com/metamath/metamath-exe)
 *   · 所有数学概念都用公理化符号表达，验证器极简
 *   · 区分"元语言"和"对象语言"层次
 *
 * 设计目标：Lv-00 的 TCB 应被压缩到最低限度——
 *          只验证约束图替换的一致性，其余全部委托给上层引擎。
 *
 * @version 1.1.0
 * @date 2026-05-24
 */
#ifndef lv_MINI_KERNEL_H
#define lv_MINI_KERNEL_H
#include <stdbool.h>
#include <stddef.h>
#include "constraint_graph.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ============== 前向声明 ============== */
typedef struct MiniKernel MiniKernel;
typedef struct MiniStatement MiniStatement;
typedef struct Substitution Substitution;
typedef struct MiniProofVerifier MiniProofVerifier;
typedef struct MiniKernelConfig MiniKernelConfig;
typedef struct ConstraintGraph ConstraintGraph;
/* ============== 语句类型枚举 ============== */
/**
 * @brief 极简验证内核的语句类型
 *
 * 借鉴 Metamath/mm0 的四类核心语句体系：
 * - $f：浮动假设（变量类型声明），声明一个数学变量
 * - $e：前提（必要假设），定理成立的前置条件
 * - $a：公理，无需证明的基础真理
 * - $p：定理，需要从前驱语句推导
 *
 * 验证器不需要理解这些语句的"数学意义"，
 * 只需要验证替换的一致性。
 */
typedef enum {
    MINI_STMT_VAR = 0,     /**< $f —— 浮动假设/变量声明（Floating Hypothesis） */
    MINI_STMT_HYP = 1,     /**< $e —— 前提/必要假设（Essential Hypothesis） */
    MINI_STMT_AXIOM = 2,   /**< $a —— 公理（Axiom），无需证明 */
    MINI_STMT_THEOREM = 3, /**< $p —— 定理（Provable），需从前驱推导 */
    MINI_STMT_COMMENT = 4  /**< $= —— 注释/元数据 */
} MiniStmtType;
/* ============== 极简验证器配置（必须在 MiniKernel 之前完整定义）============== */
/**
 * @brief 极简验证内核的配置参数
 *
 * 控制验证行为的资源限制和功能开关。
 */
struct MiniKernelConfig {
    int max_statements;          /**< 最大语句数量（0 = 无限制） */
    int max_proof_depth;         /**< 最大证明深度（防止无限递归，0 = 无限制） */
    bool trust_colors_enabled;   /**< 是否启用信任颜色跟踪（借鉴 Lv-00 颜色系统） */
    int substitution_cache_size; /**< 替换缓存大小（条目数，0 = 不缓存） */
    bool strict_mode;            /**< 严格模式：额外检查变量作用域和类型一致性 */
    int verification_timeout_ms; /**< 验证超时（毫秒），0 = 无超时 */
};
/* ============== 验证结果枚举 ============== */
/**
 * @brief 验证结果状态码
 *
 * 设计哲学（借鉴 mm0）：验证器只返回"通过"或"失败原因"，
 * 失败原因也仅与替换检查的机制相关，而非与数学语义相关。
 */
typedef enum {
    MINI_VERIFY_OK = 0,                /**< 验证通过 */
    MINI_VERIFY_FAIL_SUBSTITUTION = 1, /**< 验证失败 —— 替换不一致 */
    MINI_VERIFY_FAIL_STACK = 2,        /**< 验证失败 —— 假设栈不匹配 */
    MINI_VERIFY_FAIL_UNBOUND_VAR = 3,  /**< 验证失败 —— 存在未绑定变量 */
    MINI_VERIFY_FAIL_CYCLE = 4,        /**< 验证失败 —— 检测到循环引用 */
    MINI_VERIFY_FAIL_TIMEOUT = 5,      /**< 验证失败 —— 超时 */
    MINI_VERIFY_FAIL_MEMORY = 6        /**< 验证失败 —— 内存不足 */
} MiniVerifyResult;
/* ============== 替换结构体 ============== */
/**
 * @brief 变量替换条目（Metamath 替换检查的核心）
 *
 * 借鉴 Metamath 的替换检查机制：
 * 一条替换表示在证明步骤中，将某变量替换为某表达式。
 * 验证器检查所有替换是否一致——
 * 同一变量在同一替换上下文中必须映射到相同的表达式。
 */
struct Substitution {
    char variable_name[128];    /**< 被替换的变量名 */
    char replacement_term[512]; /**< 替换目标表达式（符号形式） */
    int replacement_node_id;    /**< 替换目标在约束图中的节点 ID（-1 表示纯符号） */
};
/* ============== 极简语句结构体 ============== */
/**
 * @brief 极简验证语句
 *
 * 对应 Metamath/mm0 中的一条语句记录。
 * 语句可引用其他语句作为证明的依据（仅 $p 类型）。
 */
struct MiniStatement {
    int id;                  /**< 语句唯一 ID */
    MiniStmtType type;       /**< 语句类型（$f / $e / $a / $p） */
    char label[256];         /**< 语句标签（如 "ax-mp", "pm2.21"） */
    char formula_text[1024]; /**< 语句公式的文本表示 */
    /* 证明引用（仅 $p 类型有效） */
    int proof_refs[64]; /**< 证明所引用的前驱语句 ID 列表 */
    int ref_count;      /**< 实际引用数量 */
    bool verified; /**< 是否已通过验证 */
    /* 约束图关联 */
    int constraint_node_id; /**< 关联的约束图节点 ID（-1 表示无关联） */
};
/* ============== 极简验证内核结构体 ============== */
/**
 * @brief 极简验证内核 —— 超小型可信计算基（TCB）
 *
 * 借鉴 mm0 的 TCB 设计哲学：
 * - 内核只做替换检查（substitution check），不内建任何数学逻辑
 * - 所有数学概念通过公理化符号由上层表达
 * - 区分"元语言"（验证器本身）和"对象语言"（被验证的数学）
 *
 * 最多包含约 2000 行 C 代码，覆盖：
 * 1. 语句注册与管理
 * 2. 替换一致性检查（核心）
 * 3. 假设栈管理
 * 4. 定理验证调度
 */
struct MiniKernel {
    MiniStatement **statements; /**< 语句数组（动态扩容） */
    int statement_count;        /**< 当前语句数量 */
    int statement_capacity;     /**< 语句数组容量 */
    /* 符号表（变量名 → 语句 ID 映射） */
    char **symbol_names;  /**< 已注册的符号名称列表 */
    int *symbol_stmt_ids; /**< 对应的语句 ID */
    int symbol_count;     /**< 符号数量 */
    int symbol_capacity;  /**< 符号表容量 */
    /* 约束图映射 */
    int *stmt_to_node_map; /**< 语句 ID → 约束图节点 ID 的映射 */
    int map_count;         /**< 映射条目数 */
    /* 内核配置 */
    MiniKernelConfig config; /**< 内核配置参数 */
    /* 统计信息 */
    int total_verified; /**< 已验证通过的定理数 */
    int total_failed;   /**< 验证失败的定理数 */
    int tcb_line_count; /**< TCB 实现行数（自报告） */
    /* 状态 */
    bool is_sealed; /**< 内核是否已封存（封存后不允许添加新公理） */
};
/* ============== 极简验证结果基类 ============== */
/**
 * @brief 极简验证器的验证结果摘要
 */
/* ============== 证明验证器结构体 ============== */
/**
 * @brief 证明验证器上下文
 *
 * 维护单次验证的运行时状态，包括假设栈、替换映射、
 * 已验证步骤和历史信息。借鉴 Metamath 的证明验证算法。
 *
 * 工作流程：
 * 1. 压入所有必要假设（$e）到假设栈
 * 2. 逐步骤执行证明引用，每一步做替换检查
 * 3. 检查最终栈顶是否与目标公式一致
 */
struct MiniProofVerifier {
    /* 假设栈 */
    int *hypothesis_stack; /**< 假设栈（存储语句 ID） */
    int stack_top;         /**< 栈顶索引（-1 = 空栈） */
    int stack_capacity;    /**< 栈容量 */
    /* 目标公式 */
    char target_formula[1024]; /**< 当前验证的目标公式文本 */
    /* 替换上下文 */
    Substitution *active_substitutions; /**< 当前步的活跃替换列表 */
    int subst_count;                    /**< 替换数量 */
    int subst_capacity;                 /**< 替换列表容量 */
    /* 验证进度 */
    int verified_step_count; /**< 已验证的步骤计数 */
    int max_steps;           /**< 最大步骤限制 */
    int current_depth;       /**< 当前证明深度 */
    /* 验证状态 */
    MiniVerifyResult last_result; /**< 最近一次验证的结果 */
    char error_detail[1024];      /**< 最近验证失败的详细描述 */
    /* 关联的内核和语句 */
    MiniKernel *kernel; /**< 所属的极简内核 */
    int target_stmt_id; /**< 当前验证的目标语句 ID */
};
/* ============== 内核生命周期 ============== */
/**
 * @brief 创建极简验证内核
 *
 * 初始化一个空的验证内核，包含空的语句表和符号表。
 * 创建后的内核处于"未封存"状态，可以添加语句。
 *
 * @param config 内核配置参数
 * @return 新分配的极简验证内核，失败返回 NULL
 */
MiniKernel *mini_kernel_create(const MiniKernelConfig *config);
/**
 * @brief 销毁极简验证内核
 *
 * 释放内核及其持有的所有语句和符号表内存。
 * 如果验证器还在运行中，会先终止验证。
 *
 * @param kernel 极简验证内核（可为 NULL，什么也不做）
 */
void mini_kernel_destroy(MiniKernel *kernel);
/* ============== 语句管理 ============== */
/**
 * @brief 添加变量声明（$f 语句）
 *
 * 向内核注册一个数学变量及其类型。
 * 借鉴 Metamath 的浮动假设机制。
 *
 * @param kernel       极简验证内核
 * @param label        变量标签（如 "x", "ph"）
 * @param type_formula 类型声明公式（如 "set", "wff"）
 * @return 新语句的 ID，失败返回 -1
 */
int mini_kernel_add_var(MiniKernel *kernel, const char *label, const char *type_formula);
/**
 * @brief 添加前提（$e 语句）
 *
 * 注册一个必要条件假设。
 * 在定理证明中自动压入假设栈。
 *
 * @param kernel  极简验证内核
 * @param label   前提标签
 * @param formula 前提公式文本
 * @return 新语句的 ID，失败返回 -1
 */
int mini_kernel_add_hyp(MiniKernel *kernel, const char *label, const char *formula);
/**
 * @brief 添加公理（$a 语句）
 *
 * 注册一条无需证明的基础真理。
 * 添加公理后内核将保留在"未封存"状态，
 * 直到显式调用封存。
 *
 * @param kernel  极简验证内核
 * @param label   公理标签（如 "ax-1", "ax-mp"）
 * @param formula 公理公式文本
 * @return 新语句的 ID，失败返回 -1
 */
int mini_kernel_add_axiom(MiniKernel *kernel, const char *label, const char *formula);
/**
 * @brief 添加定理（$p 语句）
 *
 * 注册一个待证明的定理及其证明引用。
 * 定理需要通过 mini_kernel_prove_theorem() 单独验证。
 *
 * @param kernel     极简验证内核
 * @param label      定理标签
 * @param formula    定理公式文本
 * @param proof_refs 证明引用语句 ID 列表
 * @param ref_count  引用数量
 * @return 新语句的 ID，失败返回 -1
 */
int mini_kernel_add_theorem(MiniKernel *kernel, const char *label, const char *formula, const int *proof_refs,
                            int ref_count);
/* ============== 替换检查（核心功能） ============== */
/**
 * @brief 检查变量替换的一致性 —— 内核的唯一天职
 *
 * 这是极简验证内核最核心的函数。
 * 借鉴 mm0 的设计理念：验证器不内建任何数学逻辑，
 * 只检查"替换"这一种元操作的一致性。
 *
 * 检查逻辑：
 * 1. 验证每个替换条目中 variable_name 在上下文中有定义
 * 2. 验证同一变量在同一个替换上下文中未映射到不同表达式
 * 3. 检查替换后公式的语法完整性
 *
 * @param kernel        极简验证内核
 * @param substitutions 替换条目列表
 * @param subst_count   替换条目数量
 * @param base_formula  被替换的基础公式（如公理公式）
 * @param out_result    输出：替换后的公式文本（调用者需释放）
 * @return 验证结果状态码
 */
MiniVerifyResult mini_kernel_check_substitution(MiniKernel *kernel, const Substitution *substitutions, int subst_count,
                                                const char *base_formula, char **out_result);
/* ============== 定理证明与验证 ============== */
/**
 * @brief 验证单个定理
 *
 * 对指定的定理执行完整验证流程：
 * 1. 创建验证器上下文
 * 2. 压入所有必要假设
 * 3. 逐步骤应用证明引用，进行替换检查
 * 4. 检验最终结果是否与定理公式一致
 *
 * @param kernel   极简验证内核
 * @param stmt_id  要验证的定理语句 ID
 * @return 验证结果状态码
 */
MiniVerifyResult mini_kernel_prove_theorem(MiniKernel *kernel, int stmt_id);
/**
 * @brief 从零开始验证所有定理
 *
 * 遍历内核中的所有 $p 类型语句，
 * 按依赖顺序依次验证。
 * 任何定理的依赖未通过验证，该定理也会标记为失败。
 *
 * @param kernel  极简验证内核
 * @param out_passed 输出：通过验证的数量
 * @param out_failed 输出：验证失败的数量
 * @return MINI_VERIFY_OK 表示全部通过，否则返回首个错误的类型
 */
MiniVerifyResult mini_kernel_verify_all(MiniKernel *kernel, int *out_passed, int *out_failed);
/* ============== 导入与导出 ============== */
/**
 * @brief 从 Metamath 格式文件导入语句
 *
 * 解析 .mm 文件，将语句批量添加到内核中。
 * 支持 $f、$e、$a、$p 四类语句以及 $= 注释。
 * 引用标签将在导入完成后自动解析为语句 ID。
 *
 * @param kernel   极简验证内核
 * @param filepath Metamath 文件路径
 * @return 成功导入的语句数，负数表示错误
 */
int mini_kernel_import_mm(MiniKernel *kernel, const char *filepath);
/**
 * @brief 导出内核为 Metamath 格式
 *
 * 将内核中的所有语句按 Metamath 语法导出到文件。
 * 已验证的定理附加完整证明步骤，
 * 未验证的定理生成证明骨架。
 *
 * @param kernel   极简验证内核
 * @param filepath 输出文件路径
 * @return 导出成功返回 true
 */
bool mini_kernel_export_mm(const MiniKernel *kernel, const char *filepath);
/* ============== 内核自检 ============== */
/**
 * @brief 内核自检 —— 验证验证器自身的逻辑一致性
 *
 * 借鉴 mm0 的"信任但验证"理念：
 * 运行一系列内置测试用例，验证内核的替换检查逻辑是否正确。
 * 包括：
 * - 有效替换应通过检查
 * - 冲突替换应被检测
 * - 边界条件（空替换、相同替换、循环引用）应正确处理
 *
 * @param kernel  极简验证内核
 * @return 自检结果状态码
 */
MiniVerifyResult mini_kernel_self_check(MiniKernel *kernel);
/* ============== 统计与元信息 ============== */
/**
 * @brief 获取内核统计信息
 *
 * 返回内核的完整统计摘要，包括语句数、
 * 已验证数和 TCB 行数等元信息。
 *
 * @param kernel           极简验证内核
 * @param out_total_stmts  输出：总语句数
 * @param out_vars         输出：$f 语句数
 * @param out_hyps         输出：$e 语句数
 * @param out_axioms       输出：$a 语句数
 * @param out_theorems     输出：$p 语句数
 * @param out_verified     输出：已验证通过的定理数
 * @param out_tcb_lines    输出：TCB 实现行数
 */
void mini_kernel_stats(const MiniKernel *kernel, int *out_total_stmts, int *out_vars, int *out_hyps, int *out_axioms,
                       int *out_theorems, int *out_verified, int *out_tcb_lines);
/* ============== 约束图集成 ============== */
/**
 * @brief 将验证内核的语句映射到约束图节点
 *
 * 建立语句 ID 和约束图节点 ID 的双向关联，
 * 使 Lv-00 的图形化约束系统能够直接操作
 * 内核中的已验证定理。
 *
 * @param kernel  极简验证内核
 * @param stmt_id 语句 ID
 * @param node_id 约束图节点 ID
 * @return 映射成功返回 true
 */
bool mini_kernel_bind_to_graph(MiniKernel *kernel, int stmt_id, int node_id);
/**
 * @brief 根据约束图节点查找对应的验证语句
 *
 * @param kernel  极简验证内核
 * @param node_id 约束图节点 ID
 * @return 对应的语句 ID，未找到返回 -1
 */
int mini_kernel_find_by_node(const MiniKernel *kernel, int node_id);
/* ============== 辅助函数 ============== */
/**
 * @brief 语句类型转字符串
 *
 * @param type 语句类型枚举
 * @return 类型名称字符串（静态，勿释放）
 */
const char *mini_stmt_type_to_string(MiniStmtType type);
/**
 * @brief 验证结果转字符串
 *
 * @param result 验证结果枚举
 * @return 结果描述字符串（静态，勿释放）
 */
const char *mini_verify_result_to_string(MiniVerifyResult result);
/**
 * @brief 根据标签查找语句 ID
 *
 * @param kernel 极简验证内核
 * @param label  语句标签
 * @return 语句 ID，未找到返回 -1
 */
int mini_kernel_find_by_label(const MiniKernel *kernel, const char *label);
/**
 * @brief 封存内核（禁止再添加新公理）
 *
 * 借鉴 Metamath 的"封闭世界"假设：
 * 封存后不允许添加新的公理，但可以继续添加和验证定理。
 *
 * @param kernel 极简验证内核
 */
void mini_kernel_seal(MiniKernel *kernel);
/**
 * @brief 创建默认配置
 *
 * @return 默认的内核配置（max_statements=10000, max_proof_depth=1000,
 *         trust_colors_enabled=true, strict_mode=false, verification_timeout_ms=30000）
 */
MiniKernelConfig mini_kernel_config_default(void);
/**
 * @brief 重置内核（清除所有语句和符号表，恢复到初始状态）
 *
 * @param kernel 极简验证内核
 */
void mini_kernel_reset(MiniKernel *kernel);
#ifdef __cplusplus
}
#endif
#endif /* lv_MINI_KERNEL_H */
