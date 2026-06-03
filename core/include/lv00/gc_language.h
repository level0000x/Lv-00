/**
 * @file gc_language.h
 * @brief 几何构造语言绑定 —— 借鉴 GCLC GC Language 语法 + WASM 移植管道
 *
 * 设计借鉴：
 * - GCLC (github.com/janicicpredrag/gclc)
 *   · GC Language 声明式语法：point A 10 20 / line a A B / circle k A B
 *   · 三种证明方法热切换：面积法(-a) / 吴方法(-w) / Gröbner基法(-g)
 *   · WASM 移植管道：C++ → Emscripten → WASM → TypeScript Web GUI
 *   · LaTeX 证明输出（\begin{proof} 环境）
 *   · 30年长期维护的向后兼容设计
 *
 * 核心映射：
 * - GCLC point A 10 20 → Lv-00 point A(10,20)
 * - GCLC line a A B → Lv-00 line a(A, B)
 * - GCLC circle k A B → Lv-00 circle k(A, B)
 * - GCLC intersection C a b → Lv-00 intersect C = a ∩ b
 *
 * @version v3.3.0
 * @date 2026-05-24
 */

#ifndef LV00_GC_LANGUAGE_H
#define LV00_GC_LANGUAGE_H

#include <stdbool.h>
#include <stddef.h>

#include "constraint_graph.h"
#include "symbolic_coord.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============== 前向声明 ============== */
typedef struct GCLContext GCLContext;
typedef struct GCLCommand GCLCommand;
typedef struct WasmExportConfig WasmExportConfig;
typedef struct ConstraintGraph ConstraintGraph;

/* ============== 证明方法枚举 ============== */

/**
 * @brief GCL 证明方法类型
 *
 * 借鉴 GCLC 的多证明方法热切换机制：
 * - 面积法(-a)：利用面积关系和消点法，可读性强
 * - 吴方法(-w)：代数消元法，适用面广
 * - Gröbner基法(-g)：代数方程系统求解，完备性好
 * - 全角法：利用全角关系进行角度推理
 * - 向量法：矢量代数推导，直观性强
 */
typedef enum {
    GCL_PROOF_AREA = 0,       /**< 面积法 —— 利用面积关系和消点法进行几何证明 */
    GCL_PROOF_WU = 1,         /**< 吴方法 —— 基于代数消元的机器证明方法 */
    GCL_PROOF_GROEBNER = 2,   /**< Gröbner基法 —— 代数方程系统的完备求解 */
    GCL_PROOF_FULL_ANGLE = 3, /**< 全角法 —— 利用全角关系进行角度推理 */
    GCL_PROOF_VECTOR = 4      /**< 向量法 —— 基于矢量代数的几何推导 */
} GCLProofMethod;

/* ============== GCL 命令类型枚举 ============== */

/**
 * @brief GCL 命令类型
 *
 * 涵盖 GCLC GC Language 的主要声明式和构造式命令。
 * 每条命令对应一种几何构造操作，保持向后兼容
 * （通过版本号标记和弃用而非删除的方式）。
 */
typedef enum {
    /* 基本声明 */
    GCL_CMD_POINT = 0,    /**< 点声明：point A 10 20 */
    GCL_CMD_LINE = 1,     /**< 线声明：line a A B */
    GCL_CMD_CIRCLE = 2,   /**< 圆声明：circle k A B */
    GCL_CMD_SEGMENT = 3,  /**< 线段声明：segment s A B */
    GCL_CMD_RAY = 4,      /**< 射线声明：ray r A B */
    GCL_CMD_ARC = 5,      /**< 弧声明：arc a A B C */
    GCL_CMD_POLYGON = 6,  /**< 多边形声明：polygon P A B C D ... */
    GCL_CMD_TRIANGLE = 7, /**< 三角形声明：triangle T A B C */

    /* 构造命令 */
    GCL_CMD_INTERSECT = 8,      /**< 交点：intersection C a b */
    GCL_CMD_MIDPOINT = 9,       /**< 中点：midpoint M A B */
    GCL_CMD_BISECTOR = 10,      /**< 角平分线：bisector b A B C */
    GCL_CMD_PERPENDICULAR = 11, /**< 垂线：perpendicular p l A */
    GCL_CMD_PARALLEL = 12,      /**< 平行线：parallel p l A */
    GCL_CMD_MEDIATRIX = 13,     /**< 垂直平分线：mediatrix m A B */
    GCL_CMD_ORTHOCENTER = 14,   /**< 垂心：orthocenter H A B C */
    GCL_CMD_CENTROID = 15,      /**< 重心：centroid G A B C */
    GCL_CMD_CIRCUMCENTER = 16,  /**< 外心：circumcenter O A B C */
    GCL_CMD_INCENTER = 17,      /**< 内心：incenter I A B C */
    GCL_CMD_FOOT = 18,          /**< 垂足：foot F A l */
    GCL_CMD_REFLECTION = 19,    /**< 对称点：reflection A' A l */
    GCL_CMD_ROTATION = 20,      /**< 旋转：rotation B' B A angle */
    GCL_CMD_TRANSLATION = 21,   /**< 平移：translation C' C v */
    GCL_CMD_SCALE = 22,         /**< 缩放：scale S P k */

    /* 测量命令 */
    GCL_CMD_MEASURE = 23,  /**< 测量：measure d A B (距离/角度/面积) */
    GCL_CMD_ANGLE = 24,    /**< 角度计算：angle a A B C */
    GCL_CMD_CALC = 25,     /**< 表达式计算：calc val = expr */
    GCL_CMD_DISTANCE = 26, /**< 距离计算：distance d A B */
    GCL_CMD_AREA = 27,     /**< 面积计算：area s P */

    /* 证明命令 */
    GCL_CMD_PROVE = 28,          /**< 证明命题：prove theorem_name */
    GCL_CMD_ASSUME = 29,         /**< 假设声明：assume condition */
    GCL_CMD_LEMMA = 30,          /**< 引理引用：lemma lemma_name */
    GCL_CMD_CONJECTURE = 31,     /**< 猜想：conjecture statement (待证明) */
    GCL_CMD_COUNTEREXAMPLE = 32, /**< 反例构造：counterexample condition */

    /* 模块/文件命令 */
    GCL_CMD_LOAD = 33,    /**< 加载文件：load "filename.geo" */
    GCL_CMD_INCLUDE = 34, /**< 包含头文件：include "defs.geo" */
    GCL_CMD_EXPORT = 35,  /**< 导出：export "output.tex" */
    GCL_CMD_SAVE = 36,    /**< 保存状态：save "checkpoint.geo" */

    /* 元命令 */
    GCL_CMD_COMMENT = 37, /**< 注释：% 或 // 开头 */
    GCL_CMD_SET = 38,     /**< 设置参数：set param value */
    GCL_CMD_ECHO = 39,    /**< 输出文本：echo message */
    GCL_CMD_DUMP = 40,    /**< 调试输出：dump context */

    /* 计数 */
    GCL_CMD_COUNT = 41 /**< 命令类型总数（用于数组大小） */
} GCLCommandType;

/* ============== WASM 导出格式枚举 ============== */

/**
 * @brief WASM 导出配置格式
 *
 * 借鉴 GCLC 的 WASM 移植管道设计，
 * 支持三种导出级别以适应不同的 Web 部署场景。
 */
typedef enum {
    WASM_GCL_DEFAULT = 0, /**< 默认导出 —— 包含解析+执行，不含可视化 */
    WASM_GCL_MINIMAL = 1, /**< 最小导出 —— 仅解析，最小包体 */
    WASM_GCL_FULL = 2     /**< 完整导出 —— 解析+执行+可视化+证明输出 */
} WasmExportFormat;

/* ============== WASM 导出配置结构体 ============== */

/**
 * @brief WASM 导出配置
 *
 * 控制 C++ → Emscripten → WASM → TypeScript Web GUI
 * 编译管道的输出参数。
 */
struct WasmExportConfig {
    int memory_size;                /**< WASM 线性内存大小（字节），默认 64MB */
    bool enable_proof;              /**< 是否编译证明引擎到 WASM */
    bool enable_visualization;      /**< 是否编译可视化模块到 WASM */
    bool enable_latex_export;       /**< 是否编译 LaTeX 导出模块 */
    bool enable_html_export;        /**< 是否编译 HTML 导出模块 */
    bool enable_file_system;        /**< 是否启用 WASI 文件系统支持 */
    bool enable_multithreading;     /**< 是否启用 Web Workers 多线程 */
    WasmExportFormat export_format; /**< 导出格式级别 */
    int stack_size;                 /**< WASM 栈大小（字节），默认 1MB */
    const char *module_name;        /**< WASM 模块名称（用于 TypeScript 绑定） */
    const char *output_dir;         /**< 编译输出目录 */
};

/* ============== GCL 命令结构体 ============== */

/**
 * @brief GCL 单条命令
 *
 * 表示 GC Language 中的一条声明式或构造式命令。
 * 借鉴 GCLC 的命令解析和执行的命令行参数风格。
 */
struct GCLCommand {
    GCLCommandType type;   /**< 命令类型 */
    char label[128];       /**< 命令标签（如点名 A、线名 a） */
    char params[4][256];   /**< 参数列表（最多4个参数，每个最长255字符） */
    int param_count;       /**< 实际参数数量 */
    char description[512]; /**< 命令描述/注释文本 */
};

/* ============== GCL 上下文结构体 ============== */

/**
 * @brief GCL 执行上下文
 *
 * 保存 GC Language 解析和执行过程中的全部状态，
 * 包括命令列表、证明方法、符号表及 WASM 导出配置。
 * 一个上下文对应于一个 .geo 文件的完整解析结果。
 */
struct GCLContext {
    GCLCommand **commands; /**< 命令列表（动态数组） */
    int command_count;     /**< 已解析的命令数量 */
    int command_capacity;  /**< 命令数组容量 */

    GCLProofMethod proof_method; /**< 当前激活的证明方法 */
    bool proof_method_explicit;  /**< 证明方法是否由用户显式设定 */

    /* 符号表 */
    char **symbol_names;  /**< 已注册的符号名称列表 */
    int *symbol_node_ids; /**< 对应的约束图节点 ID */
    int symbol_count;     /**< 符号数量 */
    int symbol_capacity;  /**< 符号表容量 */

    /* 关联的约束图（命令执行的目标） */
    ConstraintGraph *graph; /**< 执行结果写入的约束图 */

    /* WASM 导出配置 */
    WasmExportConfig wasm_config; /**< WASM 编译管道配置 */

    /* 状态 */
    bool is_parsed;           /**< 是否已完成解析 */
    bool has_errors;          /**< 是否存在解析/执行错误 */
    char error_message[1024]; /**< 最近的错误信息 */
    int current_line;         /**< 当前解析行号（1-based） */
    const char *source_file;  /**< 源文件名 */
};

/* ============== GCL 证明结果 ============== */

/**
 * @brief GCL 证明结果状态
 */
typedef enum {
    GCL_PROVE_OK = 0,             /**< 证明成功 */
    GCL_PROVE_FAIL_UNKNOWN = 1,   /**< 证明失败 —— 原因未知 */
    GCL_PROVE_FAIL_TIMEOUT = 2,   /**< 证明超时 */
    GCL_PROVE_FAIL_NOT_A_THM = 3, /**< 非定理 —— 找到了反例 */
    GCL_PROVE_FAIL_RESOURCES = 4, /**< 资源耗尽（内存/步骤限制） */
    GCL_PROVE_FAIL_UNDECIDED = 5  /**< 不可判定 —— 超出证明方法能力 */
} GCLProveResult;

/* ============== 上下文管理 API ============== */

/**
 * @brief 创建 GCL 上下文
 *
 * 初始化一个空的 GCL 执行环境，包含空的命令列表、符号表、
 * 默认证明方法（面积法）和默认 WASM 配置。
 *
 * @return 新分配的 GCL 上下文，失败返回 NULL
 */
GCLContext *gcl_context_create(void);

/**
 * @brief 销毁 GCL 上下文
 *
 * 释放上下文及其所持有的全部内存，包括命令列表和符号表。
 * 不会释放关联的约束图（由调用者管理）。
 *
 * @param ctx GCL 上下文（可为 NULL，什么也不做）
 */
void gcl_context_destroy(GCLContext *ctx);

/* ============== 解析 API ============== */

/**
 * @brief 解析单行 GCL 命令
 *
 * 将一行 GC Language 文本解析为 GCLCommand，
 * 追加到上下文的命令列表中并注册符号表条目。
 *
 * @param ctx   GCL 上下文
 * @param line  待解析的单行命令文本（以 '\0' 结尾）
 * @return 解析成功返回 true，语法错误返回 false
 *         （错误信息可通过 ctx->error_message 获取）
 */
bool gcl_parse(GCLContext *ctx, const char *line);

/**
 * @brief 解析整个 .geo 文件
 *
 * 逐行读取并解析文件内容到 GCL 上下文中。
 * 支持 GCLC 风格的注释（% 和 //）。
 *
 * @param ctx       GCL 上下文
 * @param filepath  文件路径
 * @return 成功解析的行数，负数表示错误
 */
int gcl_parse_file(GCLContext *ctx, const char *filepath);

/* ============== 执行 API ============== */

/**
 * @brief 执行上下文中所有已解析的命令
 *
 * 按顺序执行命令列表，将每次构造操作的结果写入关联的约束图。
 * 执行期间会根据当前证明方法自动选择适当的求解策略。
 *
 * @param ctx GCL 上下文（必须已关联约束图）
 * @return 成功执行的命令数量，负数表示错误
 */
int gcl_execute(GCLContext *ctx);

/**
 * @brief 执行单条命令
 *
 * 执行给定的 GCL 命令，将结果写入上下文的约束图。
 *
 * @param ctx   GCL 上下文
 * @param cmd  待执行的命令
 * @return 执行成功返回 true，失败返回 false
 */
bool gcl_execute_command(GCLContext *ctx, const GCLCommand *cmd);

/* ============== 证明方法管理 ============== */

/**
 * @brief 设置当前证明方法
 *
 * 借鉴 GCLC 的运行时证明方法切换（-a/-w/-g 命令行选项）。
 * 切换后，后续 gcl_prove() 调用将使用新方法。
 *
 * @param ctx    GCL 上下文
 * @param method 要激活的证明方法
 */
void gcl_set_proof_method(GCLContext *ctx, GCLProofMethod method);

/**
 * @brief 获取当前证明方法
 *
 * @param ctx GCL 上下文
 * @return 当前激活的证明方法
 */
GCLProofMethod gcl_get_proof_method(const GCLContext *ctx);

/* ============== 证明执行 ============== */

/**
 * @brief 执行几何定理证明
 *
 * 使用当前激活的证明方法，验证约束图中是否满足指定命题。
 * 借鉴 GCLC 的 prove 命令和执行管道。
 *
 * @param ctx          GCL 上下文
 * @param proposition 要证明的命题名称（已注册在符号表中）
 * @param timeout_ms   超时限制（毫秒），0 表示无限制
 * @return 证明结果状态码
 */
GCLProveResult gcl_prove(GCLContext *ctx, const char *proposition, int timeout_ms);

/* ============== 导出 API ============== */

/**
 * @brief 将证明导出为 LaTeX 格式
 *
 * 生成 GCLC 风格的 \begin{proof} 环境，
 * 包含完整的推理步骤和辅助构造。
 *
 * @param ctx       GCL 上下文
 * @param filepath  输出文件路径（.tex）
 * @return 导出成功返回 true
 */
bool gcl_export_latex(const GCLContext *ctx, const char *filepath);

/**
 * @brief 将证明导出为 HTML 格式
 *
 * 生成可交互的 HTML 页面，包含 SVG 几何图形和逐步证明。
 * 借鉴 GCLC 的 Web GUI 和 WASM 图形的呈现方式。
 *
 * @param ctx       GCL 上下文
 * @param filepath  输出文件路径（.html）
 * @return 导出成功返回 true
 */
bool gcl_export_html(const GCLContext *ctx, const char *filepath);

/* ============== WASM 编译管道 ============== */

/**
 * @brief 配置 WASM 编译参数
 *
 * 设置 C++ → Emscripten → WASM 管道的编译选项。
 * 借鉴 GCLC 的 WASM 移植经验。
 *
 * @param ctx            GCL 上下文
 * @param export_format  导出格式级别
 * @param memory_size    WASM 内存大小（字节），0 表示使用默认值
 * @return 配置成功返回 true
 */
bool gcl_compile_wasm(GCLContext *ctx, WasmExportFormat export_format, int memory_size);

/**
 * @brief 生成 TypeScript 绑定文件
 *
 * 根据当前 WASM 导出配置，生成对应的 TypeScript 类型声明
 * 和胶水代码，供 Web GUI 集成使用。
 *
 * @param ctx       GCL 上下文
 * @param filepath  输出文件路径（.ts）
 * @return 导出成功返回 true
 */
bool gcl_export_typescript_bindings(const GCLContext *ctx, const char *filepath);

/* ============== 约束图转换 ============== */

/**
 * @brief 将 GCL 命令列表转换为约束图
 *
 * 执行命令列表中的所有构造命令，将结果填充到
 * 指定的约束图中。此函数不修改 ctx->graph，
 * 而是写入外部提供的图。
 *
 * @param ctx     GCL 上下文（已完成解析）
 * @param graph   目标约束图（由调用者创建和管理）
 * @return 成功转换的命令数，负数表示错误
 */
int gcl_to_constraint_graph(const GCLContext *ctx, ConstraintGraph *graph);

/* ============== 辅助函数 ============== */

/**
 * @brief 证明方法枚举转字符串
 *
 * @param method 证明方法
 * @return 方法名称字符串（静态，勿释放）
 */
const char *gcl_proof_method_to_string(GCLProofMethod method);

/**
 * @brief 命令类型枚举转字符串
 *
 * @param type 命令类型
 * @return 命令名称字符串（静态，勿释放）
 */
const char *gcl_command_type_to_string(GCLCommandType type);

/**
 * @brief 证明结果枚举转字符串
 *
 * @param result 证明结果
 * @return 结果描述字符串（静态，勿释放）
 */
const char *gcl_prove_result_to_string(GCLProveResult result);

/**
 * @brief WASM 导出格式枚举转字符串
 *
 * @param format 导出格式
 * @return 格式名称字符串（静态，勿释放）
 */
const char *gcl_wasm_format_to_string(WasmExportFormat format);

/**
 * @brief 查找符号表中名称对应的节点ID
 *
 * @param ctx        GCL 上下文
 * @param symbol_name 符号名称
 * @return 节点 ID，未找到返回 -1
 */
int gcl_find_symbol(const GCLContext *ctx, const char *symbol_name);

/**
 * @brief 获取上一条错误信息
 *
 * @param ctx GCL 上下文
 * @return 错误信息字符串（内部存储，勿释放），无错误返回 NULL
 */
const char *gcl_get_last_error(const GCLContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* LV00_GC_LANGUAGE_H */
