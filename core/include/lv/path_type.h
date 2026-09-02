/**
 * @file path_type.h
 * @brief 路径类型系统 —— 借鉴 Arend 同伦类型论（HoTT）的路径类型设计
 *
 * @details 设计借鉴：
 * - Arend (arend-lang.github.io)
 *   - 路径类型语法 p : a = b 表示从 a 到 b 的路径（等价于等式证明）
 *   - Interval 类型 I 和路径消去 coe
 *   - 路径拼接 p @ q
 *   - 类 Java 的低门槛语法设计
 * - Homotopy Type Theory (HoTT)
 *   - "等式证明 = 路径" 的核心理念
 *   - 恒等类型 Id_A(a, b) 等价于路径空间 paths_A(a, b)
 *   - 基于路径的归纳（path induction）：基于 refl 的 J 规则
 *
 * 设计目标：将 Lv-00 的"构造即证明"理念与 HoTT 的"等式证明 = 路径"理念统一，
 *          使得每个几何构造步骤都是一条路径，路径拼接对应构造组合，
 *          路径消去（coe）对应沿路径传输属性。
 *
 * @version 1.1.0
 * @date 2026-05-24
 */
#ifndef lv_PATH_TYPE_H
#define lv_PATH_TYPE_H
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "constraint_graph.h"
#include "unify.h"
#include "lv/lv_utils.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ================================================================
 *  前向声明
 * ================================================================ */
typedef struct lvInterval lvInterval;
typedef struct lvPath lvPath;
typedef struct lvPathSystem lvPathSystem;
typedef struct ConstraintGraph ConstraintGraph;
/* ================================================================
 *  第一部分：HoTT 区间类型（Interval）
 * ================================================================ */
/**
 * @brief 路径方向枚举
 *
 * 表示路径的方向性：
 * - DIRECTION_FORWARD：从起点到终点（标准方向）
 * - DIRECTION_BACKWARD：从终点到起点（路径求逆）
 */
typedef enum {
    DIRECTION_FORWARD = 0, /**< 正向路径：从端点 A 到端点 B */
    DIRECTION_BACKWARD = 1 /**< 逆向路径：从端点 B 到端点 A */
} lvPathDirection;
/**
 * @brief HoTT 区间类型 I
 *
 * 对应 HoTT 中的区间 I，拥有两个端点 0 和 1。
 * 区间是路径类型的定义域：一条路径是从区间到空间的连续映射。
 * 左端点 left 对应 0，右端点 right 对应 1。
 *
 * 借鉴 Arend 的 Interval 类型设计：区间是一个抽象类型，
 * 通过 coe 进行消去（消除）。
 */
struct lvInterval {
    int interval_id;    /**< 区间实例的唯一标识符 */
    double left;        /**< 左端点值（对应 HoTT 端点 0） */
    double right;       /**< 右端点值（对应 HoTT 端点 1） */
    bool is_degenerate; /**< 退化区间标记（left == right，对应 refl） */
    char *label;        /**< 可选标签（用于调试/显示） */
};
/* ================================================================
 *  第二部分：路径类型枚举
 * ================================================================ */
/**
 * @brief 路径类型分类枚举
 *
 * 根据 HoTT 和 Arend 的路径理论，将路径划分为以下几种类型：
 * - PATH_IDENTITY：恒等路径（refl），证明 a = a
 * - PATH_CONSTRUCTION：构造路径，由几何构造步骤产生
 * - PATH_COMPOSITE：合成路径，由两条路径拼接而成（p @ q）
 * - PATH_INVERSE：逆路径，将路径方向反转
 * - PATH_TRANSPORT：传输路径，沿路径传输类型或属性
 * - PATH_EQUIVALENCE：等价路径，表示两个类型/空间之间的等价关系
 */
typedef enum {
    PATH_IDENTITY = 0,     /**< 恒等路径（refl），a = a */
    PATH_CONSTRUCTION = 1, /**< 构造路径，由几何构造步骤 a |- b 产生 */
    PATH_COMPOSITE = 2,    /**< 合成路径，p @ q : a = c */
    PATH_INVERSE = 3,      /**< 逆路径，p^{-1} : b = a */
    PATH_TRANSPORT = 4,    /**< 传输路径，coe——沿路径传输类型/属性 */
    PATH_EQUIVALENCE = 5   /**< 等价路径，表示类型/空间之间的等价 */
} lvPathType;
/**
 * @brief 路径传输模式枚举
 *
 * 定义沿路径传输的三种策略：
 * - TRANSPORT_ALONG_PATH：沿单条路径传输
 * - TRANSPORT_ALONG_EQUIV：沿等价（equivalence）传输
 * - TRANSPORT_ALONG_CONSTRUCTION：沿构造路径传输（与 Lv-00 构造图联动）
 */
typedef enum {
    TRANSPORT_ALONG_PATH = 0,        /**< 沿单条路径传输属性或类型 */
    TRANSPORT_ALONG_EQUIV = 1,       /**< 沿类型等价（equivalence）传输 */
    TRANSPORT_ALONG_CONSTRUCTION = 2 /**< 沿构造路径传输（联动构造图） */
} lvTransportMode;
/* ================================================================
 *  第三部分：路径结构体
 * ================================================================ */
/**
 * @brief HoTT 路径结构体
 *
 * 路径类型 p : a = b 在 Lv-00 中的具体表示。
 * 路径是从区间 I 到空间的映射函数，两端点分别对应起点和终点。
 *
 * 一条路径的核心语义：
 * - 起点端点 A 和终点端点 B 定义路径的"类型"（路径空间）
 * - 路径函数 path_func 是区间 [0,1] 上的连续映射
 * - 路径标签标识路径的语义（如 "构造 - 作线段AB"）
 * - 方向表示路径的遍历方向
 *
 * 与 Lv-00 的"构造即证明"理念呼应：
 * 每条几何构造步骤都是一条 PATH_CONSTRUCTION 类型的路径，
 * 构造的组合对应于 PATH_COMPOSITE 路径拼接。
 */
struct lvPath {
    int path_id;                                    /**< 路径唯一标识符 */
    lvPathType type;                                /**< 路径类型分类 */
    lvPathDirection direction;                      /**< 路径方向（正向/逆向） */
    int endpoint_a;                                 /**< 起点端点（对应 HoTT 起点 a） */
    int endpoint_b;                                 /**< 终点端点（对应 HoTT 终点 b） */
    int interval_id;                                /**< 关联的区间实例 ID */
    char *label;                                    /**< 路径标签（人类可读，用于显示/调试） */
    ConstraintGraph *construction;                  /**< 关联的几何构造图（构造路径专有） */
    double (*path_func)(double t, void *user_data); /**< 路径映射函数 f: I -> Space */
    void *func_user_data;                           /**< 映射函数的用户数据 */
    bool is_constant;                               /**< 是否为恒等路径（端点 A == 端点 B） */
    int source_step_id;                             /**< 产生此路径的构造步骤 ID（溯源） */
    int64_t created_at_us;                          /**< 路径创建时间（微秒，用于排序/调试） */
};
/* ================================================================
 *  第四部分：路径系统（Paths Universe）
 * ================================================================ */
/**
 * @brief 路径消去上下文
 *
 * 对应 HoTT 的 coe（coerce）操作——沿路径消去（消除）类型族。
 * 在 Lv-00 中，路径消去用于将约束条件或命题沿构造路径传输。
 *
 * 典型使用场景：
 * - 已知在 a 点满足某属性 P(a)，且有一条路径 p : a = b，
 *   则 coe 将 P(a) 传输到 P(b) —— 得到 b 点也满足该属性的证明。
 */
typedef struct {
    int context_id;          /**< 消去上下文的唯一 ID */
    int source_type_id;      /**< 源类型族 ID */
    lvTransportMode mode;    /**< 传输模式 */
    int along_path_id;       /**< 沿此路径传输（路径 ID） */
    int along_equiv_id;      /**< 沿此等价传输（等价 ID，仅 TRANSPORT_ALONG_EQUIV） */
    void *transported_term;  /**< 传输后的项（结果） */
    bool preserve_structure; /**< 是否保留结构不变性 */
    char error_msg[256];     /**< 传输失败时的错误信息 */
} lvPathCoercionContext;
/**
 * @brief 路径系统 —— 所有活跃路径的全局注册与管理
 *
 * 路径系统是整个路径类型理论在 Lv-00 中的运行时载体。
 * 它维护：
 * - 所有活跃路径的注册表
 * - 区间实例池（区间可被复用）
 * - 路径消去上下文（coe 操作的状态跟踪）
 *
 * 典型工作流：
 * 1. 调用 path_system_create() 初始化系统
 * 2. 调用 path_create() 创建几何构造步骤对应的路径
 * 3. 调用 path_compose() 拼接路径（对应构造组合）
 * 4. 调用 path_transport() 沿路径传输属性
 * 5. 调用 path_to_equality() 将 HoTT 路径转换为等式证明
 * 6. 调用 path_system_destroy() 清理
 */
struct lvPathSystem {
    lvDArray paths_da;                   /**< 路径动态数组 */
    lvInterval *intervals;               /**< 区间实例池 */
    int interval_count;                  /**< 当前区间数量 */
    int interval_capacity;               /**< 区间池容量 */
    lvDArray coe_contexts_da;            /**< 路径消去上下文数组 */
    bool is_initialized;                 /**< 系统初始化状态 */
    int64_t init_time_us;                /**< 系统初始化时间戳 */
};
/* ================================================================
 *  第五部分：API —— 路径系统生命周期
 * ================================================================ */
/**
 * @brief 创建并初始化路径系统
 *
 * 分配并初始化一个全新的路径系统，包括路径注册表、
 * 区间实例池和路径消去上下文数组。
 *
 * @param path_capacity    路径注册表的初始容量（建议 >= 64）
 * @param interval_capacity 区间池的初始容量（建议 >= 32）
 * @return 成功返回新分配的路径系统指针，失败返回 NULL
 */
lvPathSystem *path_system_create(int path_capacity, int interval_capacity);
/**
 * @brief 销毁路径系统并释放所有关联资源
 *
 * 释放路径系统中所有路径、区间、消去上下文以及系统自身的内存。
 * 销毁后指针不再有效。
 *
 * @param sys  路径系统指针（销毁后置为悬空，调用方应置 NULL）
 */
lv_PUBLIC_API void path_system_destroy(lvPathSystem *sys);
/* ================================================================
 *  第六部分：API —— 路径创建与操作
 * ================================================================ */
/**
 * @brief 创建一条新路径
 *
 * 在路径系统中注册一条从端点 A 到端点 B 的路径。
 * 路径类型根据构造来源自动推断（通常为 PATH_CONSTRUCTION）。
 *
 * @param sys          路径系统
 * @param endpoint_a   起点端点 ID
 * @param endpoint_b   终点端点 ID
 * @param label        路径标签（人类可读描述，可为 NULL）
 * @param path_func    路径映射函数 f: [0,1] -> Space（可为 NULL 表示标准线性插值）
 * @param user_data    映射函数的用户数据
 * @param source_step  产生此路径的构造步骤 ID（-1 表示无溯源）
 * @return 成功返回新路径 ID（>= 0），失败返回 -1
 */
int path_create(lvPathSystem *sys, int endpoint_a, int endpoint_b, const char *label,
                double (*path_func)(double t, void *user_data), void *user_data, int source_step);
/**
 * @brief 创建恒等路径（refl）
 *
 * 创建端点 A 到自身的恒等路径 a = a（对应 HoTT 的 refl_a）。
 * 恒等路径是路径归纳（J 规则）的基本构造子。
 *
 * @param sys          路径系统
 * @param endpoint_a   端点 ID（起点 = 终点）
 * @param label        路径标签（可为 NULL）
 * @return 成功返回恒等路径 ID（>= 0），失败返回 -1
 */
lv_PUBLIC_API int path_create_identity(lvPathSystem *sys, int endpoint_a, const char *label);
/**
 * @brief 创建逆路径
 *
 * 创建某条路径的逆路径 p^{-1} : b = a。
 * 对应 HoTT 的路径求逆操作。
 *
 * @param sys       路径系统
 * @param path_id   待求逆的路径 ID
 * @return 成功返回逆路径 ID（>= 0），失败返回 -1
 */
lv_PUBLIC_API int path_create_inverse(lvPathSystem *sys, int path_id);
/**
 * @brief 路径拼接（p @ q）
 *
 * 拼接两条路径 p : a = b 和 q : b = c，得到合成路径 p @ q : a = c。
 * 对应 HoTT 的路径拼接（path concatenation/composition）。
 * 前一条路径的终点必须与后一条路径的起点相同。
 *
 * @param sys         路径系统
 * @param path_id_p   第一条路径 ID（p : a = b）
 * @param path_id_q   第二条路径 ID（q : b = c）
 * @param label       合成路径标签（可为 NULL）
 * @return 成功返回合成路径 ID（>= 0），失败返回 -1
 */
lv_PUBLIC_API int path_compose(lvPathSystem *sys, int path_id_p, int path_id_q, const char *label);
/**
 * @brief 路径传输（coe —— 沿路径消去）
 *
 * 对应 HoTT 的 coe 操作：沿路径传输类型族或属性。
 * 已知在起点端点满足某属性，将其沿路径传输到终点端点。
 *
 * 典型场景：
 * - 已知 P(a) 成立，且有路径 p : a = b，传输后得到 P(b)
 *
 * @param sys            路径系统
 * @param path_id        沿此路径传输
 * @param source_type_id 源类型族 ID
 * @param mode           传输模式
 * @param transported    输入：源端点处的项；输出：传输后的项
 * @return 成功返回 0，失败返回负值错误码
 */
lv_PUBLIC_API int path_transport(lvPathSystem *sys, int path_id, int source_type_id, lvTransportMode mode, void **transported);
/* ================================================================
 *  第七部分：API —— 路径查询与变换
 * ================================================================ */
/**
 * @brief 检查路径是否为恒等路径
 *
 * 判断一条路径是否将端点映射到自身（即端点 A == 端点 B）。
 *
 * @param sys      路径系统
 * @param path_id  路径 ID
 * @return 若为恒等路径返回 true，否则返回 false（路径不存在也返回 false）
 */
lv_PUBLIC_API bool path_is_constant(const lvPathSystem *sys, int path_id);
/**
 * @brief 将 HoTT 路径转换为等式证明（与 unify.h 集成）
 *
 * 将路径 p : a = b 转换为 Lv-00 合一系统的等式证明。
 * 输出的等式约束图可被 unify.h 的合一检查器验证。
 *
 * @param sys            路径系统
 * @param path_id        路径 ID
 * @param out_equality   输出：等式约束图（[take] 调用者负责销毁）
 * @return 成功返回 0，失败返回负值错误码
 */
lv_PUBLIC_API int path_to_equality(lvPathSystem *sys, int path_id, ConstraintGraph **out_equality);
/**
 * @brief 从构造步骤生成路径证明
 *
 * 将 Lv-00 中的一个几何构造步骤（步骤索引）转换为对应的路径。
 * 构造步骤的输出端点成为路径的终点，输入端点为路径的起点。
 *
 * @param sys         路径系统
 * @param step_index  构造步骤索引
 * @param label       路径标签（可为 NULL，则自动从步骤名派生）
 * @return 成功返回路径 ID（>= 0），失败返回 -1
 */
lv_PUBLIC_API int path_from_construction(lvPathSystem *sys, int step_index, const char *label);
/**
 * @brief 将路径转换为约束图等价关系
 *
 * 将路径 p : a = b 表示为约束图中的等价关系约束。
 * 这使得路径系统可以与 Lv-00 的约束图引擎联动。
 *
 * @param sys                路径系统
 * @param path_id            路径 ID
 * @param out_constraint     输出：等价约束（[take] 调用者负责销毁）
 * @return 成功返回 0，失败返回负值错误码
 */
lv_PUBLIC_API int path_to_constraint_graph(lvPathSystem *sys, int path_id, ConstraintGraph **out_constraint);
/**
 * @brief 查询两点之间的所有已知路径
 *
 * 给定两个端点 ID，返回路径系统中连接这两个端点的所有路径 ID。
 * 查询结果包括正向和逆向路径。
 *
 * @param sys           路径系统
 * @param endpoint_a    起点端点 ID
 * @param endpoint_b    终点端点 ID
 * @param out_path_ids  输出：路径 ID 数组（调用者分配，大小 >= max_count）
 * @param max_count     数组最大容量
 * @return 找到的路径数量（>= 0），或 -1 表示错误
 */
int path_system_get_all_paths_between(const lvPathSystem *sys, int endpoint_a, int endpoint_b, int *out_path_ids,
                                      int max_count);
/* ================================================================
 *  第八部分：API —— 区间操作
 * ================================================================ */
/**
 * @brief 在路径系统中创建区间实例
 *
 * 创建一个从 left 到 right 的区间。
 * 若 left == right，区间为退化的（对应恒等路径 refl）。
 *
 * @param sys     路径系统
 * @param left    左端点值
 * @param right   右端点值
 * @param label   区间标签（可为 NULL）
 * @return 成功返回区间 ID（>= 0），失败返回 -1
 */
lv_PUBLIC_API int path_system_create_interval(lvPathSystem *sys, double left, double right, const char *label);
/**
 * @brief 获取区间实例
 *
 * @param sys         路径系统
 * @param interval_id 区间 ID
 * @return 区间指针，若不存在返回 NULL
 */
lv_PUBLIC_API const lvInterval *path_system_get_interval(const lvPathSystem *sys, int interval_id);
/**
 * @brief 获取路径实例
 *
 * @param sys      路径系统
 * @param path_id  路径 ID
 * @return 路径指针，若不存在返回 NULL
 */
lv_PUBLIC_API const lvPath *path_system_get_path(const lvPathSystem *sys, int path_id);
#ifdef __cplusplus
}
#endif
#endif /* lv_PATH_TYPE_H */
