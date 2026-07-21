/**
 * @file interactive_geo.h
 * @brief 交互几何系统 —— 借鉴 Cinderella 与 Dr. Geo 的交互几何 UX 设计
 *
 * @details 设计借鉴来源：
 *          - Cinderella (cinderella.de, 1998-)
 *            · Randomized Theorem Checking（随机化定理验证）
 *              —— 通过大量随机采样验证几何定理，提供概率性真值判定
 *            · 连续运动下保持构造一致性（Continuity Tracking）
 *              —— 在用户拖拽几何对象时，自动检测奇异配置并维护构造语境
 *            · 投影几何核心 —— 基于齐次坐标的投影几何作为底层模型
 *
 *          - Dr. Geo (gnu.org/software/dr-geo, 1996-)
 *            · 几何构造即代码生成 —— 用户画图的同时自动生成 Smalltalk 脚本，
 *              以可执行代码精确记录构造过程
 *            · 构造/脚本双向同步 —— 修改脚本自动更新图形，反之亦然
 *
 *          设计目标：
 *          - 提供与 Cinderella 同等的交互几何体验（拖拽、约束、连续性跟踪）
 *          - 集成 Dr. Geo 的代码生成哲学（几何构造 = 可执行脚本）
 *          - 支持随机化定理验证作为证明辅助手段
 *          - 约束实时维护，确保拖拽操作不破坏几何一致性
 *
 * @author Lv-00 Project
 * @version 1.1.0
 * @date 2026-05-24
 */
#ifndef LV00_INTERACTIVE_GEO_H
#define LV00_INTERACTIVE_GEO_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* 前向声明：LV00Engine 定义在 engine.h 中。
 * 此处仅声明指针类型，避免引入 engine.h 的完整依赖链，
 * 同时提供编译期类型安全检查（优于 void*）。 */
typedef struct LV00Engine LV00Engine;
/* ==================== 常量定义 ==================== */
/** 最大同时活跃几何对象数量 */
#define LV00_GEO_MAX_OBJECTS 1024
/** 最大约束数量 */
#define LV00_GEO_MAX_CONSTRAINTS 2048
/** 最大拖拽影响链深度 */
#define LV00_GEO_MAX_DRAG_CHAIN 64
/** 快照历史最大保留数量 */
#define LV00_GEO_MAX_SNAPSHOTS 32
/** 构造脚本缓冲区大小 */
#ifndef LV00_GEO_SCRIPT_BUFFER_SIZE
#define LV00_GEO_SCRIPT_BUFFER_SIZE 65536
#endif
/** 状态导出 JSON 缓冲区大小 */
#ifndef LV00_GEO_STATE_BUFFER_SIZE
#define LV00_GEO_STATE_BUFFER_SIZE 131072
#endif
/** 随机化验证默认采样次数 */
#define LV00_GEO_DEFAULT_SAMPLE_COUNT 10000
/** 随机化验证默认容差 */
#define LV00_GEO_DEFAULT_TOLERANCE 1e-9
/** 随机化验证高置信度阈值 */
#define LV00_GEO_HIGH_CONFIDENCE 0.9999
/* ==================== 枚举定义 ==================== */
/**
 * @brief 交互几何模式枚举
 *
 * 定义用户当前与几何画布交互的模式，决定鼠标/触摸操作的行为。
 * 借鉴 Cinderella 的模态交互设计——每种工具模式有独立的操作语义。
 */
typedef enum {
    GEO_MODE_POINT = 0,     /**< 点模式：点击画布创建新点 */
    GEO_MODE_LINE = 1,      /**< 线模式：拖拽创建直线 */
    GEO_MODE_CIRCLE = 2,    /**< 圆模式：点击圆心后拖拽确定半径 */
    GEO_MODE_SEGMENT = 3,   /**< 线段模式：点击两端点创建线段 */
    GEO_MODE_SELECT = 4,    /**< 选择模式：点击选中/取消选中几何对象 */
    GEO_MODE_DRAG = 5,      /**< 拖拽模式：自由拖拽移动几何对象 */
    GEO_MODE_CONSTRUCT = 6, /**< 构造模式：通过预设构造规则创建几何体 */
    GEO_MODE_MEASURE = 7,   /**< 测量模式：点击显示距离/角度/面积 */
    GEO_MODE_PROVE = 8      /**< 证明模式：选择几何体并启动自动证明 */
} InteractiveGeoMode;
/**
 * @brief 配置分类 —— 借鉴 Cinderella 连续性跟踪
 *
 * Cinderella 在用户拖拽点时会持续检测几何配置的类型，
 * 防止出现平行线相交、三角形退化等奇异状态。
 */
typedef enum {
    CONFIG_NORMAL = 0,    /**< 正常配置：所有对象位置与约束一致 */
    CONFIG_SINGULAR = 1,  /**< 奇异配置：触发退化条件（如平行线相交） */
    CONFIG_DEGENERATE = 2 /**< 退化配置：维度降低（如三角形三点共线） */
} ConfigClassification;
/**
 * @brief 脚本语言类型 —— 借鉴 Dr. Geo 代码生成
 *
 * Dr. Geo 在用户画图时同步生成 Smalltalk 脚本。
 * Lv-00 扩展为支持多种目标脚本语言。
 */
typedef enum {
    SCRIPT_LANG_LV00_DSL = 0, /**< Lv-00 原生 DSL */
    SCRIPT_LANG_PYTHON = 1,   /**< Python 脚本 */
    SCRIPT_LANG_LUA = 2       /**< Lua 脚本 */
} ScriptLanguage;
/**
 * @brief 随机化验证结果
 */
typedef enum {
    RAND_CHECK_PASSED = 0,                /**< 所有样本通过，定理在该容差下成立 */
    RAND_CHECK_FAILED = 1,                /**< 至少一个样本失败 */
    RAND_CHECK_INCONCLUSIVE = 2,          /**< 无法判定（样本数不足或容差过严格） */
    RAND_CHECK_PROBABILISTICALLY_TRUE = 3 /**< 高概率成立（但非严格证明） */
} RandomizedCheckResult;
/**
 * @brief 约束维护状态
 */
typedef enum {
    CONSTRAINT_OK = 0,                /**< 约束维护成功 */
    CONSTRAINT_OVER_CONSTRAINED = 1,  /**< 过度约束，存在冗余或冲突约束 */
    CONSTRAINT_UNDER_CONSTRAINED = 2, /**< 约束不足，存在自由度 */
    CONSTRAINT_SINGULAR_AVOIDED = 3,  /**< 检测到奇异配置并自动避开 */
    CONSTRAINT_FAILED = 4,            /**< 约束维护失败 */
} ConstraintMaintainStatus;
/* ==================== 结构体定义 ==================== */
/**
 * @brief 几何画布状态 —— 借鉴 Cinderella 视图管理
 *
 * 封装当前交互会话中所有活跃的几何对象、选中状态、拖拽状态
 * 和视图变换信息。这是交互几何系统的核心运行时状态。
 */
typedef struct Lv00GeoCanvasState {
    /* ── 几何对象 ── */
    int *active_object_ids;  /**< 所有活跃几何对象 ID 列表 */
    int active_object_count; /**< 活跃对象数量 */
    /* ── 拖拽状态 ── */
    int drag_target_id;    /**< 当前被拖拽的对象 ID（-1 = 无拖拽） */
    double drag_start_x;   /**< 拖拽起始 X 坐标 */
    double drag_start_y;   /**< 拖拽起始 Y 坐标 */
    double drag_current_x; /**< 拖拽当前 X 坐标 */
    double drag_current_y; /**< 拖拽当前 Y 坐标 */
    /* ── 选中状态 ── */
    int *selected_ids;       /**< 当前选中对象 ID 列表 */
    int selected_count;      /**< 选中对象数量 */
    int primary_selected_id; /**< 主选中对象 ID（菜单/属性面板目标） */
    /* ── 当前构造模式 ── */
    InteractiveGeoMode current_mode; /**< 当前交互模式 */
    int construction_partials[4];    /**< 构造模式部分结果（如圆：先点圆心再拖半径） */
    int construction_partial_count;  /**< 部分构造步骤计数 */
    /* ── 视口变换 ── */
    double viewport_matrix[3][3]; /**< 视口变换矩阵（平移+缩放） */
    double zoom_level;            /**< 当前缩放级别（1.0 = 100%） */
    double viewport_offset_x;     /**< 视口 X 偏移 */
    double viewport_offset_y;     /**< 视口 Y 偏移 */
    double canvas_width;          /**< 画布像素宽度（由 UI 设置） */
    double canvas_height;         /**< 画布像素高度（由 UI 设置） */
    /* ── 元数据 ── */
    bool grid_visible;   /**< 是否显示网格 */
    bool snap_to_grid;   /**< 是否吸附到网格 */
    double grid_spacing; /**< 网格间距 */
    bool modified;       /**< 自上次保存后是否有修改 */
} Lv00GeoCanvasState;
/**
 * @brief 随机化定理验证 —— 借鉴 Cinderella Randomized Theorem Checking
 *
 * Cinderella 通过以下方式验证几何定理：
 * 1. 生成大量随机配置（采样）
 * 2. 在每种配置下验证定理条件
 * 3. 统计通过率，给出概率真值判定
 *
 * 这不是严格的形式化证明，但在实践中对于排除错误猜想非常高效。
 */
typedef struct Lv00RandomizedCheck {
    int sample_count; /**< 随机采样次数 */
    double tolerance; /**< 数值容差 */
    int passed_samples; /**< 通过的样本数 */
    int failed_samples; /**< 失败的样本数 */
    bool is_probabilistically_true; /**< 概率真值判定 */
    double confidence_level;        /**< 置信水平 [0.0, 1.0] */
    /* ── 调试信息 ── */
    double *failed_sample_params;  /**< 失败样本的参数数组 */
    int failed_sample_param_count; /**< 失败样本参数数量 */
    double elapsed_time_ms;        /**< 验证耗时（毫秒） */
} Lv00RandomizedCheck;
/**
 * @brief 几何脚本绑定 —— 借鉴 Dr. Geo Smalltalk 代码生成
 *
 * Dr. Geo 的核心理念：每次几何构造操作不仅创建图形，
 * 同时生成对应的 Smalltalk 脚本代码。这样：
 * - 用户可以通过修改脚本来精确调整构造
 * - 构造过程可重复、可审计、可分享
 *
 * Lv-00 扩展为支持多种脚本语言，并提供脚本代码映射表。
 */
typedef struct Lv00GeoScriptBinding {
    /* ── 对象→脚本映射表 ── */
    int *object_ids;        /**< 几何对象 ID 列表 */
    char **script_snippets; /**< 对应脚本代码片段（字符串数组） */
    int binding_count;      /**< 绑定数量 */
    /* ── 脚本语言配置 ── */
    ScriptLanguage current_language; /**< 当前目标脚本语言 */
    /* ── 自动生成 ── */
    bool auto_generate; /**< 自动生成脚本标志（true = 每次构造自动追加） */
    /* ── 脚本缓冲区 ── */
    char full_script[LV00_GEO_SCRIPT_BUFFER_SIZE]; /**< 完整构造脚本 */
    int script_length;                             /**< 脚本当前长度 */
} Lv00GeoScriptBinding;
/**
 * @brief 连续性跟踪器 —— 借鉴 Cinderella Continuity 机制
 *
 * Cinderella 使用投影几何（齐次坐标）作为底层数学模型，
 * 确保在连续运动中几何构造不会发生"跳跃"。
 *
 * 连续性跟踪器在每一步操作前后检测配置变化，防止：
 * - 平行线"相交于无穷远点"时的数值奇异
 * - 三角形退化为线段
 * - 分母为零的代数奇异
 */
typedef struct Lv00ContinuityTracker {
    /* ── 上次配置快照 ── */
    double *last_config; /**< 上次配置的参数向量 */
    int last_config_dim; /**< 参数向量维度 */
    /* ── 奇异检测 ── */
    bool parallel_lines_detected; /**< 平行线异常相交检测 */
    bool degenerate_triangle;     /**< 三角形退化（三点共线）检测 */
    bool zero_denominator;        /**< 分母为零检测 */
    bool near_singular;           /**< 接近奇异（数值不稳定）的预警 */
    /* ── 配置分类 ── */
    ConfigClassification current_config;  /**< 当前配置分类 */
    ConfigClassification previous_config; /**< 上次配置分类 */
    /* ── 容差配置 ── */
    double singular_threshold;   /**< 奇异判定阈值 */
    double degenerate_threshold; /**< 退化判定阈值 */
    /* ── 统计 ── */
    int singular_encounters; /**< 奇异配置累计遭遇次数 */
    int singular_avoidances; /**< 奇异配置成功避开次数 */
} Lv00ContinuityTracker;
/**
 * @brief 约束保持器 —— 借鉴 Cinderella 实时约束求解
 *
 * 当用户拖拽点 A 时，Cinderella 实时计算所有受影响的约束并更新位置。
 *
 * 典型场景：
 * - 拖拽三角形顶点 B，则 AB、BC 线段随之更新
 * - 拖拽圆上的点，该点被约束保持在圆周上
 * - 拖拽满足"中点"约束的点，另一个点自动对称调整
 *
 * 约束保持器维护约束图并在每次拖拽事件中执行增量更新。
 */
typedef struct Lv00ConstraintMaintainer {
    /* ── 约束条目 ── */
    int *constraint_ids;      /**< 约束 ID 列表 */
    int *constraint_subjects; /**< 约束主体对象 ID 列表 */
    int constraint_count;     /**< 约束数量 */
    /* ── 影响链 ── */
    int *affected_objects; /**< 当前受影响的对象（拖拽时填充） */
    int affected_count;    /**< 受影响对象数量 */
    /* ── 求解器引用 ── */
    void *solver_handle; /**< 约束求解器句柄（内部使用，类型为 Solver*，
                              此处保留 void* 因为 Solver 定义在 solver.h 中，
                              而 solver.h 依赖 constraint_graph.h 等重型头文件，
                              为避免循环依赖和编译开销，不在此处前向声明 Solver） */
    /* ── 维护策略 ── */
    bool use_projective_method; /**< 使用投影几何方法（Cinderella 风格） */
    double convergence_epsilon; /**< 收敛判定阈值 */
    int max_iterations;         /**< 约束求解最大迭代次数 */
} Lv00ConstraintMaintainer;
/**
 * @brief 交互几何主上下文
 *
 * 聚合所有交互几何子系统的顶层结构，是交互几何模块的入口。
 */
typedef struct Lv00InteractiveGeo {
    Lv00GeoCanvasState canvas_state;           /**< 画布状态 */
    Lv00RandomizedCheck rand_check;            /**< 随机化验证 */
    Lv00GeoScriptBinding script_binding;       /**< 脚本绑定 */
    Lv00ContinuityTracker continuity;          /**< 连续性跟踪 */
    Lv00ConstraintMaintainer constraint_maint; /**< 约束保持 */
    /* ── 快照系统 ── */
    char *snapshots[LV00_GEO_MAX_SNAPSHOTS]; /**< 状态快照 JSON 数组 */
    int snapshot_count;                      /**< 快照数量 */
    int current_snapshot_index;              /**< 当前快照索引（用于撤销/重做） */
    /* ── 引擎引用 ── */
    LV00Engine *engine_handle; /**< 关联的 LV00Engine 句柄 */
    /* ── 回调 ── */
    void (*on_mode_changed)(InteractiveGeoMode new_mode);       /**< 模式变更回调 */
    void (*on_selection_changed)(int selected_id);              /**< 选中变更回调 */
    void (*on_drag_updated)(int object_id, double x, double y); /**< 拖拽更新回调 */
} Lv00InteractiveGeo;
/* ==================== 生命周期 ==================== */
/**
 * @brief 初始化交互几何系统
 *
 * 分配并初始化 Lv00InteractiveGeo 上下文，设置默认模式为 GEO_MODE_SELECT，
 * 缩放级别 1.0，视口为单位矩阵。
 *
 * @param[in] engine_handle 关联的 LV00Engine 句柄（可为 NULL 延迟绑定）
 * @return 新分配的交互几何上下文，失败返回 NULL
 */
Lv00InteractiveGeo *interactive_geo_init(LV00Engine *engine_handle);
/**
 * @brief 销毁交互几何系统并释放所有关联资源
 *
 * 包括快照历史、脚本绑定、选中列表、约束影响链等。
 * 传入 NULL 是安全的。
 *
 * @param[in,out] geo 交互几何上下文（设为 NULL 是安全的）
 */
void interactive_geo_destroy(Lv00InteractiveGeo *geo);
/* ==================== 模式管理 ==================== */
/**
 * @brief 设置当前交互模式
 *
 * 切换工具模式（点/线/圆/选择/拖拽/构造/测量/证明）。
 * 会触发 on_mode_changed 回调。
 *
 * @param[in,out] geo  交互几何上下文
 * @param[in]     mode 目标交互模式
 */
void interactive_geo_set_mode(Lv00InteractiveGeo *geo, InteractiveGeoMode mode);
/**
 * @brief 获取当前交互模式
 *
 * @param[in] geo 交互几何上下文
 * @return 当前交互模式
 */
InteractiveGeoMode interactive_geo_get_mode(const Lv00InteractiveGeo *geo);
/* ==================== 选择管理 ==================== */
/**
 * @brief 选中一个几何对象
 *
 * 如果已存在多选，添加到选择列表。如果按下修饰键（Ctrl/Cmd），
 * 追加选择；否则替换选择。
 *
 * @param[in,out] geo       交互几何上下文
 * @param[in]     object_id 要选中的对象 ID
 * @return 成功返回 0，无效 ID 返回 -1
 */
int interactive_geo_select(Lv00InteractiveGeo *geo, int object_id);
/**
 * @brief 取消选中几何对象
 *
 * @param[in,out] geo       交互几何上下文
 * @param[in]     object_id 要取消选中的对象 ID（-1 表示取消全部选中）
 */
void interactive_geo_deselect(Lv00InteractiveGeo *geo, int object_id);
/* ==================== 拖拽交互 ==================== */
/**
 * @brief 开始拖拽操作
 *
 * 记录拖拽起始位置和目标对象。如果附近没有可拖拽对象，
 * object_id 设为 -1（画布拖拽/平移）。
 *
 * @param[in,out] geo       交互几何上下文
 * @param[in]     object_id 被拖拽的对象 ID（-1 = 画布平移）
 * @param[in]     x         拖拽起始 X 坐标（世界坐标）
 * @param[in]     y         拖拽起始 Y 坐标（世界坐标）
 * @return 成功返回 0，对象不存在返回 -1
 */
int interactive_geo_drag_start(Lv00InteractiveGeo *geo, int object_id, double x, double y);
/**
 * @brief 拖拽移动
 *
 * 更新拖拽位置并触发约束维护。
 * 借鉴 Cinderella 的实时约束求解——拖拽点 A 时自动更新所有受约束影响的点。
 *
 * @param[in,out] geo  交互几何上下文
 * @param[in]     x    当前拖拽 X 坐标（世界坐标）
 * @param[in]     y    当前拖拽 Y 坐标（世界坐标）
 * @return 约束维护状态码
 */
ConstraintMaintainStatus interactive_geo_drag_move(Lv00InteractiveGeo *geo, double x, double y);
/**
 * @brief 结束拖拽操作
 *
 * 释放拖拽状态，将最终位置写入对象坐标。
 *
 * @param[in,out] geo  交互几何上下文
 * @param[in]     x    最终 X 坐标（世界坐标）
 * @param[in]     y    最终 Y 坐标（世界坐标）
 * @return 约束维护状态码
 */
ConstraintMaintainStatus interactive_geo_drag_end(Lv00InteractiveGeo *geo, double x, double y);
/* ==================== 随机化定理验证 ==================== */
/**
 * @brief 执行随机化定理验证 —— 借鉴 Cinderella Randomized Theorem Checking
 *
 * 对给定的几何定理或命题执行随机采样验证：
 * 1. 生成 sample_count 组随机配置（遵守当前约束）
 * 2. 在每组配置下评估定理条件
 * 3. 统计通过率，返回概率真值判定
 *
 * @param[in,out] geo          交互几何上下文
 * @param[in]     sample_count 采样次数（0 = 使用默认值 10000）
 * @param[in]     tolerance    数值容差（0 = 使用默认值 1e-9）
 * @param[in]     theorem_expr 定理表达式（Lv-00 DSL 格式，NULL = 使用当前选中构造）
 * @param[out]    result       输出：验证结果详情
 * @return 随机化验证结果状态码
 */
int interactive_geo_randomized_check(Lv00InteractiveGeo *geo, int sample_count,
                                     double tolerance, const char *theorem_expr,
                                     Lv00RandomizedCheck *result);

/* ==================== 坐标变换 ==================== */

/**
 * @brief 将世界坐标转换为屏幕坐标
 *
 * 根据当前视口的缩放级别和偏移量，将几何世界坐标映射到屏幕像素坐标。
 * 使用 3x3 视口变换矩阵完成仿射变换。
 *
 * @param[in]  geo      交互几何上下文
 * @param[in]  world_x  世界坐标 X
 * @param[in]  world_y  世界坐标 Y
 * @param[out] screen_x 输出的屏幕坐标 X
 * @param[out] screen_y 输出的屏幕坐标 Y
 */
void interactive_geo_world_to_screen(const Lv00InteractiveGeo *geo,
                                     double world_x, double world_y,
                                     double *screen_x, double *screen_y);

/**
 * @brief 将屏幕坐标转换为世界坐标
 *
 * 逆变换：屏幕像素坐标 → 世界几何坐标。
 *
 * @param[in]  geo      交互几何上下文
 * @param[in]  screen_x 屏幕坐标 X
 * @param[in]  screen_y 屏幕坐标 Y
 * @param[out] world_x  输出的世界坐标 X
 * @param[out] world_y  输出的世界坐标 Y
 */
void interactive_geo_screen_to_world(const Lv00InteractiveGeo *geo,
                                     double screen_x, double screen_y,
                                     double *world_x, double *world_y);

/* ==================== 命中检测 ==================== */

/**
 * @brief 对屏幕坐标执行命中检测
 *
 * 将屏幕坐标转换为世界坐标后，在所有活跃几何对象中查找
 * 距离最近的可交互对象。返回最近对象的 ID，如果无对象
 * 在命中阈值范围内则返回 -1。
 *
 * 命中阈值（hit_radius）为屏幕像素距离，默认 12px。
 *
 * @param[in] geo         交互几何上下文
 * @param[in] screen_x    屏幕坐标 X
 * @param[in] screen_y    屏幕坐标 Y
 * @param[in] hit_radius  命中半径（屏幕像素，0 = 默认 12px）
 * @param[out] out_distance 可选：输出最近对象的距离
 * @return 命中对象 ID，无命中返回 -1
 */
int interactive_geo_hit_test(const Lv00InteractiveGeo *geo,
                             double screen_x, double screen_y,
                             double hit_radius, double *out_distance);

/**
 * @brief 获取指定对象的当前世界坐标
 *
 * 从引擎图数据中查询对象的世界坐标位置。
 *
 * @param[in]  geo       交互几何上下文
 * @param[in]  object_id 对象 ID
 * @param[out] world_x   输出 X 坐标
 * @param[out] world_y   输出 Y 坐标
 * @return 成功返回 0，对象不存在返回 -1
 */
int interactive_geo_get_object_position(const Lv00InteractiveGeo *geo,
                                        int object_id,
                                        double *world_x, double *world_y);

/**
 * @brief 更新视口缩放级别
 *
 * 以指定世界坐标点为中心进行缩放。
 *
 * @param[in,out] geo       交互几何上下文
 * @param[in]     zoom_delta 缩放增量（>0 放大，<0 缩小）
 * @param[in]     center_x   缩放中心 X（屏幕坐标）
 * @param[in]     center_y   缩放中心 Y（屏幕坐标）
 */
void interactive_geo_zoom(Lv00InteractiveGeo *geo, double zoom_delta,
                          double center_x, double center_y);

/**
 * @brief 重置视口到默认状态
 *
 * @param[in,out] geo 交互几何上下文
 */
void interactive_geo_reset_viewport(Lv00InteractiveGeo *geo);

/**
 * @brief 设置画布像素尺寸
 *
 * UI 端在创建画布或窗口 resize 时调用此函数，
 * 更新 w2s/s2w 坐标变换中使用的画布宽高。
 *
 * @param[in,out] geo   交互几何上下文
 * @param[in]     width  画布宽度（像素）
 * @param[in]     height 画布高度（像素）
 */
void interactive_geo_set_canvas_size(Lv00InteractiveGeo *geo,
                                     double width, double height);

#ifdef __cplusplus
}
#endif
#endif /* LV00_INTERACTIVE_GEO_H */