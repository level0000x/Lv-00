/**
 * @file math_protocol.h
 * @brief 结构化数学中间表示协议 —— 借鉴 CortexJS / MathJSON 的语义化表达式
 *
 * 借鉴来源：
 *   - CortexJS / MathJSON（cortexjs.io）
 *     语义化 JSON 表达式：["Add", "x", 2] 而非 "x+2"
 *   - Compute Engine 支持数值/符号混合计算
 *   - 可扩展字典（Dictionary）机制——注册自定义函数/符号
 *
 * 设计目标：
 *   - 树形 AST 表达任意数学表达式，支持嵌套到任意深度
 *   - 序列化为 MathJSON 兼容的 JSON 字符串，便于网络传输
 *   - 混合数值/符号计算，支持延迟求值（lazy evaluation）
 *   - 可扩展符号字典，注册几何专用函数（distance/midpoint/collinear 等）
 *   - 支持 LaTeX 导出，用于渲染和文档生成
 *
 * @version v3.3.0
 * @date 2026-05-24
 */

#ifndef LV00_MATH_PROTOCOL_H
#define LV00_MATH_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 第一部分：数学表达式类型枚举（28+ 类型）
 *
 * 借鉴 MathJSON 的语义化表达式设计：
 *   每个节点用类型标签标识语义，参数通过子节点数组传入。
 *   这与传统字符串表示（"x+2"）不同，结构明确、无歧义。
 *
 * 类型覆盖：
 *   - 基本算术（Add/Subtract/Multiply/Divide/Power/Negate/Sqrt）
 *   - 三角函数（Sin/Cos/Tan/Arcsin/Arccos/Arctan）
 *   - 关系运算（Equal/Less/Greater/LessEqual/GreaterEqual/NotEqual）
 *   - 通用函数调用（Function）
 *   - 几何专用（GeomPoint/GeomLine/GeomCircle/Distance/Midpoint/Collinear/Parallel/Perpendicular）
 * ======================================================================== */

/**
 * @brief 数学表达式节点类型枚举
 *
 * 覆盖基本算术、三角函数、关系运算、几何专用等类型。
 * 借鉴 MathJSON 的语义标签设计，每种类型对应一个明确的数学语义。
 */
typedef enum {
    /* ---- 原子类型 ---- */
    MATH_EXPR_NUMBER, /**< 数值字面量：整数或浮点数 */
    MATH_EXPR_SYMBOL, /**< 符号变量：如 x, y, a */
    MATH_EXPR_STRING, /**< 字符串字面量 */

    /* ---- 基本算术 ---- */
    MATH_EXPR_ADD,      /**< 加法：["Add", a, b, ...] */
    MATH_EXPR_SUBTRACT, /**< 减法：["Subtract", a, b] */
    MATH_EXPR_MULTIPLY, /**< 乘法：["Multiply", a, b, ...] */
    MATH_EXPR_DIVIDE,   /**< 除法：["Divide", a, b] */
    MATH_EXPR_POWER,    /**< 幂：["Power", base, exponent] */
    MATH_EXPR_NEGATE,   /**< 取反：["Negate", x] */
    MATH_EXPR_SQRT,     /**< 平方根：["Sqrt", x] */
    MATH_EXPR_ABS,      /**< 绝对值：["Abs", x] */

    /* ---- 三角函数 ---- */
    MATH_EXPR_SIN,    /**< 正弦：["Sin", x] */
    MATH_EXPR_COS,    /**< 余弦：["Cos", x] */
    MATH_EXPR_TAN,    /**< 正切：["Tan", x] */
    MATH_EXPR_ARCSIN, /**< 反正弦：["Arcsin", x] */
    MATH_EXPR_ARCCOS, /**< 反余弦：["Arccos", x] */
    MATH_EXPR_ARCTAN, /**< 反正切：["Arctan", x] */

    /* ---- 关系运算 ---- */
    MATH_EXPR_EQUAL,         /**< 等于：["Equal", a, b] */
    MATH_EXPR_LESS,          /**< 小于：["Less", a, b] */
    MATH_EXPR_GREATER,       /**< 大于：["Greater", a, b] */
    MATH_EXPR_LESS_EQUAL,    /**< 小于等于：["LessEqual", a, b] */
    MATH_EXPR_GREATER_EQUAL, /**< 大于等于：["GreaterEqual", a, b] */
    MATH_EXPR_NOT_EQUAL,     /**< 不等于：["NotEqual", a, b] */

    /* ---- 通用函数 ---- */
    MATH_EXPR_FUNCTION, /**< 通用函数调用：["Function", name, arg1, arg2, ...] */

    /* ---- 几何专用 ---- */
    MATH_EXPR_GEOM_POINT,    /**< 几何点：["GeomPoint", x_coord, y_coord] */
    MATH_EXPR_GEOM_LINE,     /**< 几何线：["GeomLine", pointA, pointB] */
    MATH_EXPR_GEOM_CIRCLE,   /**< 几何圆：["GeomCircle", center, radius] */
    MATH_EXPR_DISTANCE,      /**< 距离：["Distance", pointA, pointB] */
    MATH_EXPR_MIDPOINT,      /**< 中点：["Midpoint", pointA, pointB] */
    MATH_EXPR_COLLINEAR,     /**< 共线：["Collinear", p1, p2, p3] */
    MATH_EXPR_PARALLEL,      /**< 平行：["Parallel", lineA, lineB] */
    MATH_EXPR_PERPENDICULAR, /**< 垂直：["Perpendicular", lineA, lineB] */
    MATH_EXPR_UNKNOWN        /**< 未知/待解析表达式 */
} MathExprType;

/* ========================================================================
 * 第二部分：数学表达式 AST 节点
 *
 * 树形结构，每个节点含类型标签和子节点数组。
 * 支持最多 16 个子节点（arity 限制），通过动态数组管理。
 * ======================================================================== */

/** @brief MathExpr 子节点数组最大初始容量 */
#define MATH_EXPR_MAX_ARITY 16

/**
 * @brief 数学表达式树节点
 *
 * 借鉴 MathJSON 的语义化表达式：
 *   - type:   语义标签（如 MATH_EXPR_ADD）
 *   - args:   子表达式数组（树的孩子节点）
 *   - 叶子节点（NUMBER/SYMBOL）使用 number_value/symbol_name
 *
 * 示例：
 *   表达式 (x + 2) * sin(y) 的树形式：
 *     MathExpr { type=MATH_EXPR_MULTIPLY, args=[
 *       MathExpr { type=MATH_EXPR_ADD, args=[
 *         MathExpr { type=MATH_EXPR_SYMBOL, symbol_name="x" },
 *         MathExpr { type=MATH_EXPR_NUMBER, number_value=2.0 } ] },
 *       MathExpr { type=MATH_EXPR_SIN, args=[
 *         MathExpr { type=MATH_EXPR_SYMBOL, symbol_name="y" } ] }
 *     ] }
 */
typedef struct MathExpr {
    MathExprType type;      /**< 节点类型 */
    struct MathExpr **args; /**< 子表达式数组（动态分配） */
    int arg_count;          /**< 当前子表达式数量 */
    int arg_capacity;       /**< 子表达式数组容量 */

    /* 叶子节点数据（仅当 arg_count == 0 时有效） */
    double number_value; /**< 数值（MATH_EXPR_NUMBER） */
    char *symbol_name;   /**< 符号名（MATH_EXPR_SYMBOL） */
    char *string_value;  /**< 字符串值（MATH_EXPR_STRING） */
    char *function_name; /**< 函数名（MATH_EXPR_FUNCTION） */

    /* 元数据 */
    int id;                 /**< 节点唯一 ID（用于缓存和引用） */
    bool is_simplified;     /**< 是否已化简 */
    bool is_evaluated;      /**< 是否已求值 */
    double evaluated_value; /**< 求值结果（数值模式下有效） */
} MathExpr;

/* ========================================================================
 * 第三部分：可扩展数学字典
 *
 * 借鉴 CortexJS Compute Engine 的可扩展字典机制。
 * 字典条目分三类：FUNCTION / SYMBOL / PREDICATE。
 * 几何模块可注册 distance/midpoint/collinear 等几何专用函数。
 * ======================================================================== */

/**
 * @brief 字典条目类型枚举
 */
typedef enum {
    MATH_DICT_FUNCTION, /**< 函数：接收参数返回表达式 */
    MATH_DICT_SYMBOL,   /**< 符号：命名常量或变量 */
    MATH_DICT_PREDICATE /**< 谓词：返回布尔值的函数（几何关系等） */
} MathDictEntryType;

/**
 * @brief 数学字典条目
 *
 * 每条目包含名称、类型和元数（arity）。
 * 扩展函数（如同构函子 Hom、函子组合 ∘）通过此机制注册。
 */
typedef struct MathDictEntry {
    char *name;                   /**< 条目名称（如 "distance", "midpoint", "collinear"） */
    MathDictEntryType entry_type; /**< 条目类型 */
    int arity;                    /**< 元数（-1 表示可变参数） */
    char *description;            /**< 描述文本 */
    int id;                       /**< 条目在字典内的唯一 ID */
} MathDictEntry;

/**
 * @brief 可扩展数学字典
 *
 * 维护已注册的函数/符号/谓词表。
 * Compute Engine 遇到 MATH_EXPR_FUNCTION 节点时，
 * 在字典中查找对应的函数定义以确定求值策略。
 */
typedef struct MathDictionary {
    MathDictEntry *entries; /**< 字典条目数组 */
    int entry_count;        /**< 当前条目数 */
    int entry_capacity;     /**< 条目数组容量 */
    char *name;             /**< 字典名称（如 "lv00_geometry"） */
    bool is_readonly;       /**< 是否只读（预定义字典不可修改） */
} MathDictionary;

/* ========================================================================
 * 第四部分：序列化与通信协议
 * ======================================================================== */

/**
 * @brief 数学表达式序列化格式枚举
 */
typedef enum {
    MATH_FORMAT_JSON,   /**< MathJSON 兼容格式：["Add", "x", 2] */
    MATH_FORMAT_BINARY, /**< 二进制紧凑格式：类型标签 + 长度前缀 */
    MATH_FORMAT_S_EXPR  /**< S-表达式格式：(Add x 2) */
} MathSerializationFormat;

/**
 * @brief 协议消息类型
 */
typedef enum {
    MATH_MSG_EXPRESSION,       /**< 数学表达式 */
    MATH_MSG_DICTIONARY_ENTRY, /**< 字典条目注册 */
    MATH_MSG_EVALUATE_REQUEST, /**< 求值请求 */
    MATH_MSG_EVALUATE_RESULT,  /**< 求值结果 */
    MATH_MSG_ERROR             /**< 错误消息 */
} MathProtocolMessageType;

/**
 * @brief 协议消息帧
 *
 * 用于前后端通信的消息帧格式。
 * 每条消息包含类型标签、长度和负载（序列化的表达式）。
 */
typedef struct MathProtocolMessage {
    MathProtocolMessageType msg_type; /**< 消息类型 */
    MathSerializationFormat format;   /**< 序列化格式 */
    uint8_t *payload;                 /**< 消息负载 */
    size_t payload_length;            /**< 负载长度（字节） */
    int message_id;                   /**< 消息 ID（用于请求-响应匹配） */
} MathProtocolMessage;

/* ========================================================================
 * 第五部分：核心 API —— 表达式生命周期
 * ======================================================================== */

/**
 * @brief 创建一个叶子数值表达式
 *
 * @param value 数值
 * @return 新分配的 MathExpr（调用者负责释放），失败返回 NULL
 */
MathExpr *math_expr_create_number(double value);

/**
 * @brief 创建一个符号变量表达式
 *
 * @param name 符号名称（内部会复制）
 * @return 新分配的 MathExpr（调用者负责释放），失败返回 NULL
 */
MathExpr *math_expr_create_symbol(const char *name);

/**
 * @brief 创建一个带子表达式的复合表达式
 *
 * 分配新的 MathExpr 节点并设置类型，子表达式通过 math_expr_add_arg() 添加。
 *
 * @param type 表达式类型（如 MATH_EXPR_ADD）
 * @return 新分配的 MathExpr（调用者负责释放），失败返回 NULL
 */
MathExpr *math_expr_create(MathExprType type);

/**
 * @brief 向复合表达式添加子表达式
 *
 * 子表达式被移动（move）到父节点中，调用者不应再访问。
 *
 * @param parent 父表达式（必须是非叶子类型）
 * @param child  子表达式（所有权转移到父节点）
 * @return true 添加成功，false 参数无效或容量已满
 */
bool math_expr_add_arg(MathExpr *parent, MathExpr *child);

/**
 * @brief 销毁表达式树
 *
 * 递归释放整个表达式树的所有节点及其子表达式。
 *
 * @param expr 要销毁的表达式（可为 NULL）
 */
void math_expr_destroy(MathExpr *expr);

/**
 * @brief 深拷贝表达式树
 *
 * @param expr 源表达式
 * @return 深拷贝的新表达式树（调用者负责释放），失败返回 NULL
 */
MathExpr *math_expr_clone(const MathExpr *expr);

/* ========================================================================
 * 第六部分：核心 API —— 序列化/反序列化
 * ======================================================================== */

/**
 * @brief 将表达式树序列化为 MathJSON 字符串
 *
 * 生成 CortexJS MathJSON 兼容的 JSON 表示：
 *   ["Multiply", ["Add", "x", 2], ["Sin", "y"]]
 *
 * @param expr   要序列化的表达式
 * @param format 序列化格式
 * @return 新分配的字符串（调用者负责 free），失败返回 NULL
 */
char *math_expr_serialize(const MathExpr *expr, MathSerializationFormat format);

/**
 * @brief 从 MathJSON 字符串反序列化为表达式树
 *
 * 解析 MathJSON 格式的字符串并重构表达式树。
 *
 * @param json_string JSON 格式的序列化字符串
 * @param format      输入格式
 * @return 新分配的 MathExpr 树（调用者负责释放），解析失败返回 NULL
 */
MathExpr *math_expr_deserialize(const char *json_string, MathSerializationFormat format);

/* ========================================================================
 * 第七部分：核心 API —— 化简与求值
 * ======================================================================== */

/**
 * @brief 对表达式进行符号化简
 *
 * 应用代数化简规则：常数折叠、零元/幺元消除、幂等化简等。
 * 支持混合数值/符号表达式的部分化简。
 *
 * @param expr 要化简的表达式（原地修改）
 * @return true 化简成功，false 化简失败（部分化简可能已完成）
 */
bool math_expr_simplify(MathExpr *expr);

/**
 * @brief 对表达式进行求值
 *
 * 在给定变量绑定的条件下计算表达式的数值（或符号）结果。
 * 纯数值表达式直接计算，含符号变量的表达式需要 bindings。
 *
 * @param expr       要计算的表达式
 * @param bindings   变量绑定表（symbol_name → MathExpr*，NULL 表示空绑定）
 * @param binding_count 绑定数量
 * @return 求值结果表达式（调用者负责释放），求值失败返回 NULL
 */
MathExpr *math_expr_evaluate(MathExpr *expr, const char **binding_names, MathExpr *const *binding_values,
                             int binding_count);

/* ========================================================================
 * 第八部分：核心 API —— 字典管理
 * ======================================================================== */

/**
 * @brief 创建数学字典
 *
 * @param name       字典名称
 * @param readonly   true 创建只读字典（预定义），false 创建可修改字典
 * @return 新分配的 MathDictionary，失败返回 NULL
 */
MathDictionary *math_dict_create(const char *name, bool readonly);

/**
 * @brief 销毁数学字典
 *
 * @param dict 字典（可为 NULL）
 */
void math_dict_destroy(MathDictionary *dict);

/**
 * @brief 向字典注册一个条目
 *
 * 注册新的函数、符号或谓词定义。
 *
 * @param dict        目标字典
 * @param name        条目名称
 * @param entry_type  条目类型
 * @param arity       元数（-1 表示可变参数）
 * @param description 可选的描述文本（可为 NULL）
 * @return true 注册成功，false 名称冲突或字典只读
 */
bool math_dict_register(MathDictionary *dict, const char *name, MathDictEntryType entry_type, int arity,
                        const char *description);

/**
 * @brief 在字典中查找条目
 *
 * @param dict  字典
 * @param name  要查找的条目名称
 * @return 找到的条目指针（借引用），未找到返回 NULL
 */
const MathDictEntry *math_dict_lookup(const MathDictionary *dict, const char *name);

/**
 * @brief 从字典中移除条目
 *
 * @param dict  字典
 * @param name  要移除的条目名称
 * @return true 移除成功，false 未找到或字典只读
 */
bool math_dict_unregister(MathDictionary *dict, const char *name);

/* ========================================================================
 * 第九部分：核心 API —— 前后端通信协议
 * ======================================================================== */

/**
 * @brief 创建协议消息帧
 *
 * @param msg_type 消息类型
 * @param format   序列化格式
 * @param payload  消息负载（所有权转移至消息帧）
 * @param length   负载长度
 * @return 新分配的消息帧，失败返回 NULL
 */
MathProtocolMessage *math_protocol_create_message(MathProtocolMessageType msg_type, MathSerializationFormat format,
                                                  uint8_t *payload, size_t length);

/**
 * @brief 销毁协议消息帧
 *
 * @param msg 消息帧（可为 NULL）
 */
void math_protocol_destroy_message(MathProtocolMessage *msg);

/**
 * @brief 发送协议消息（占位函数——由具体传输层实现）
 *
 * 将序列化的消息帧发送到后端计算引擎。
 * 具体传输方式（TCP/WebSocket/内存管道）由实现层决定。
 *
 * @param msg     要发送的消息
 * @param address 目标地址（格式取决于传输层）
 * @return true 发送成功，false 发送失败
 */
bool math_protocol_send(const MathProtocolMessage *msg, const char *address);

/**
 * @brief 接收协议消息（占位函数——由具体传输层实现）
 *
 * 从前端或后端接收序列化的消息帧。
 *
 * @param address 源地址（格式取决于传输层）
 * @param timeout_ms 超时毫秒（0 = 阻塞等待，-1 = 不等待）
 * @return 接收到的消息帧（调用者负责释放），超时或失败返回 NULL
 */
MathProtocolMessage *math_protocol_receive(const char *address, int timeout_ms);

/* ========================================================================
 * 第十部分：核心 API —— LaTeX 导出
 * ======================================================================== */

/**
 * @brief 将表达式树转换为 LaTeX 字符串
 *
 * 将 MathExpr 树转换为可渲染的 LaTeX 字符串。
 * 示例：
 *   MATH_EXPR_ADD(x, MATH_EXPR_POWER(y, 2)) → "x + y^{2}"
 *
 * @param expr 表达式树
 * @return 新分配的 LaTeX 字符串（调用者负责 free），失败返回 NULL
 */
char *math_expr_to_latex(const MathExpr *expr);

/**
 * @brief 获取表达式类型的字符串名称
 *
 * 用于调试和日志输出。
 *
 * @param type 表达式类型枚举值
 * @return 类型名称字符串（静态，不可修改），如 "Add", "Sin", "GeomPoint"
 */
const char *math_expr_type_name(MathExprType type);

#ifdef __cplusplus
}
#endif

#endif /* LV00_MATH_PROTOCOL_H */
