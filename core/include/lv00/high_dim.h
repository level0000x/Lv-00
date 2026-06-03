/**
 * @file high_dim.h
 * @brief 高维结构表示与交互模块头文件
 *
 * 本模块实现四维及以上数学对象的表示和投影机制。
 * 高维对象通过端口抽象块承载，二维矩形编码仅为投影视图之一。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#ifndef LV00_HIGH_DIM_H
#define LV00_HIGH_DIM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "constraint_graph.h"
#include "symbolic_coord.h"

/* 前向声明 —— 避免引入 lv00.h 的 16+ 传递依赖 */
struct LV00Engine;
typedef struct LV00Engine LV00Engine;

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量定义 ==================== */

/** 最大支持的维度数 */
#define HIGH_DIM_MAX_DIMENSIONS 16
#define HIGH_DIM_INITIAL_CAPACITY 16

/** 最大投影预设数量 */
#define HIGH_DIM_MAX_PROJECTION_PRESETS 8

/** 投影名称最大长度 */
#define HIGH_DIM_PROJECTION_NAME_MAX 64

/** 默认保真度阈值 */
#define HIGH_DIM_DEFAULT_FIDELITY_THRESHOLD 0.5

/** 语义缩放最大透视深度 */
#define HIGH_DIM_MAX_DEPTH 32

/* ==================== 类型定义 ==================== */

/**
 * @brief 维度映射类型
 *
 * 定义高维坐标轴如何映射到二维画布
 */
typedef enum {
    HIGH_DIM_MAP_TO_X = 0, /**< 映射到X轴 */
    HIGH_DIM_MAP_TO_Y,     /**< 映射到Y轴 */
    HIGH_DIM_MAP_FOLD,     /**< 折叠（不显示） */
    HIGH_DIM_MAP_DISCARD   /**< 丢弃 */
} HighDimMappingType;

/**
 * @brief 高维坐标轴映射配置
 */
typedef struct {
    int axis_index;                  /**< 高维坐标轴索引 */
    HighDimMappingType mapping_type; /**< 映射类型 */
    double scale;                    /**< 缩放因子 */
    double offset;                   /**< 偏移量 */
} HighDimAxisMapping;

/**
 * @brief 线性变换矩阵（2x2，用于投影旋转和缩放）
 */
typedef struct {
    double m[2][2]; /**< 2x2矩阵元素 */
} HighDimTransform2D;

/**
 * @brief 投影预设配置
 */
typedef struct {
    char name[HIGH_DIM_PROJECTION_NAME_MAX];              /**< 投影名称 */
    int dimension_count;                                  /**< 高维对象的维度数 */
    int mapping_count;                                    /**< 映射配置数量 */
    HighDimAxisMapping mappings[HIGH_DIM_MAX_DIMENSIONS]; /**< 轴映射配置 */
    HighDimTransform2D transform;                         /**< 2D变换矩阵 */
    bool is_default;                                      /**< 是否为默认投影 */
} HighDimProjectionPreset;

/**
 * @brief 高维抽象端口块
 *
 * 四维及以上数学对象的抽象表示
 */
typedef struct {
    int block_id;                                                     /**< 关联的函数块ID */
    int dimension_count;                                              /**< 维度数 */
    int preset_count;                                                 /**< 投影预设数量 */
    HighDimProjectionPreset presets[HIGH_DIM_MAX_PROJECTION_PRESETS]; /**< 投影预设数组 */
    int current_preset_index;                                         /**< 当前使用的预设索引 */
    double fidelity_ratio;                                            /**< 当前保真度比例 */
} HighDimAbstractBlock;

/**
 * @brief 高维块管理器
 */
typedef struct {
    HighDimAbstractBlock *blocks;              /**< 高维块数组 */
    int block_count;                           /**< 块数量 */
    int block_capacity;                        /**< 数组容量 */
    int perspective_depth;                     /**< 当前语义缩放透视深度 */
    int perspective_stack[HIGH_DIM_MAX_DEPTH]; /**< 语义缩放深度栈，记录各级进入的block_id */
} HighDimManager;

/**
 * @brief 可见关系统计
 */
typedef struct {
    int total_relations;   /**< 总关系数 */
    int visible_relations; /**< 可见关系数 */
    double fidelity_ratio; /**< 保真度比例 */
} HighDimVisibilityStats;

/**
 * @brief 投影坐标结果
 */
typedef struct {
    double x;              /**< 投影后的X坐标 */
    double y;              /**< 投影后的Y坐标 */
    bool is_valid;         /**< 投影是否有效 */
    char folded_info[256]; /**< 被折叠维度的信息标注 */
} HighDimProjectedCoord;

/* ==================== 生命周期管理 ==================== */

/**
 * @brief 创建高维管理器
 * @return 高维管理器指针，失败返回NULL
 */
HighDimManager *high_dim_manager_create(void);

/**
 * @brief 销毁高维管理器
 * @param manager 高维管理器指针
 */
void high_dim_manager_destroy(HighDimManager *manager);

/**
 * @brief 初始化高维管理器
 * @param manager 高维管理器指针
 * @return 成功返回0，失败返回错误码
 */
int high_dim_manager_init(HighDimManager *manager);

/* ==================== 高维块操作 ==================== */

/**
 * @brief 注册高维抽象块
 *
 * 将一个函数块注册为高维抽象块
 *
 * @param manager 高维管理器
 * @param block_id 函数块ID
 * @param dimension_count 维度数
 * @return 成功返回0，失败返回错误码
 */
int high_dim_register_block(HighDimManager *manager, int block_id, int dimension_count);

/**
 * @brief 注销高维抽象块
 * @param manager 高维管理器
 * @param block_id 函数块ID
 * @return 成功返回0，失败返回错误码
 */
int high_dim_unregister_block(HighDimManager *manager, int block_id);

/**
 * @brief 获取高维块
 * @param manager 高维管理器
 * @param block_id 函数块ID
 * @return 高维块指针，未找到返回NULL
 */
HighDimAbstractBlock *high_dim_get_block(HighDimManager *manager, int block_id);

/* ==================== 投影预设管理 ==================== */

/**
 * @brief 添加投影预设
 *
 * 为高维块添加一个新的投影预设配置
 *
 * @param manager 高维管理器
 * @param block_id 函数块ID
 * @param preset 投影预设配置
 * @return 成功返回预设索引，失败返回错误码
 */
int high_dim_add_projection_preset(HighDimManager *manager, int block_id, const HighDimProjectionPreset *preset);

/**
 * @brief 删除投影预设
 * @param manager 高维管理器
 * @param block_id 函数块ID
 * @param preset_index 预设索引
 * @return 成功返回0，失败返回错误码
 */
int high_dim_remove_projection_preset(HighDimManager *manager, int block_id, int preset_index);

/**
 * @brief 设置当前投影预设
 * @param manager 高维管理器
 * @param block_id 函数块ID
 * @param preset_index 预设索引
 * @return 成功返回0，失败返回错误码
 */
int high_dim_set_current_preset(HighDimManager *manager, int block_id, int preset_index);

/**
 * @brief 获取当前投影预设
 * @param manager 高维管理器
 * @param block_id 函数块ID
 * @return 当前预设指针，失败返回NULL
 */
const HighDimProjectionPreset *high_dim_get_current_preset(const HighDimManager *manager, int block_id);

/**
 * @brief 创建默认投影预设
 *
 * 根据维度数创建默认的投影预设配置
 *
 * @param dimension_count 维度数
 * @param preset 输出预设配置
 * @return 成功返回0，失败返回错误码
 */
int high_dim_create_default_preset(int dimension_count, HighDimProjectionPreset *preset);

/* ==================== 坐标投影 ==================== */

/**
 * @brief 投影高维坐标到二维
 *
 * 将高维坐标根据当前投影预设映射到二维画布
 *
 * @param manager 高维管理器
 * @param block_id 函数块ID
 * @param high_dim_coords 高维坐标数组
 * @param coord_count 坐标数量
 * @param projected 输出投影坐标
 * @return 成功返回0，失败返回错误码
 */
int high_dim_project_coordinates(HighDimManager *manager, int block_id, const SymbolicCoord **high_dim_coords,
                                 int coord_count, HighDimProjectedCoord *projected);

/**
 * @brief 应用2D变换
 *
 * 对投影后的坐标应用旋转、缩放等变换
 *
 * @param coord 输入坐标
 * @param transform 变换矩阵
 * @param result 输出变换后的坐标
 * @return 成功返回0，失败返回错误码
 */
int high_dim_apply_transform(const HighDimProjectedCoord *coord, const HighDimTransform2D *transform,
                             HighDimProjectedCoord *result);

/**
 * @brief 创建旋转变换
 * @param angle_rad 旋转角度（弧度）
 * @param transform 输出变换矩阵
 * @return 成功返回0，失败返回错误码
 */
int high_dim_create_rotation_transform(double angle_rad, HighDimTransform2D *transform);

/**
 * @brief 创建缩放变换
 * @param scale_x X轴缩放因子
 * @param scale_y Y轴缩放因子
 * @param transform 输出变换矩阵
 * @return 成功返回0，失败返回错误码
 */
int high_dim_create_scale_transform(double scale_x, double scale_y, HighDimTransform2D *transform);

/* ==================== 保真度计算 ==================== */

/**
 * @brief 计算投影保真度
 *
 * 计算当前投影能显示的关系比例
 *
 * @param manager 高维管理器
 * @param block_id 函数块ID
 * @param constraint_graph 约束图
 * @param stats 输出统计信息
 * @return 成功返回0，失败返回错误码
 */
int high_dim_calculate_fidelity(HighDimManager *manager, int block_id, const ConstraintGraph *constraint_graph,
                                HighDimVisibilityStats *stats);

/**
 * @brief 检查保真度是否低于阈值
 * @param manager 高维管理器
 * @param block_id 函数块ID
 * @param threshold 阈值（0.0-1.0）
 * @return 低于阈值返回1，否则返回0，错误返回-1
 */
int high_dim_is_fidelity_below_threshold(const HighDimManager *manager, int block_id, double threshold);

/**
 * @brief 获取保真度提示信息
 *
 * 当保真度低于阈值时，生成提示用户切换投影的消息
 *
 * @param manager 高维管理器
 * @param block_id 函数块ID
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 成功返回0，失败返回错误码
 */
int high_dim_get_fidelity_warning(const HighDimManager *manager, int block_id, char *buffer, size_t buffer_size);

/* ==================== 语义缩放 ==================== */

/**
 * @brief 进入高维块内部透视
 *
 * 切换画布上下文到高维块的局部坐标系
 *
 * @param manager 高维管理器
 * @param block_id 函数块ID
 * @return 成功返回0，失败返回错误码
 */
int high_dim_enter_block_perspective(HighDimManager *manager, int block_id);

/**
 * @brief 退出高维块内部透视
 * @param manager 高维管理器
 * @return 成功返回0，失败返回错误码
 */
int high_dim_exit_block_perspective(HighDimManager *manager);

/**
 * @brief 获取当前透视深度
 * @param manager 高维管理器
 * @return 当前透视深度，错误返回-1
 */
int high_dim_get_current_depth(const HighDimManager *manager);

/**
 * @brief 直接跳转到指定缩放层级
 *
 * 通过反复进入或退出透视来达到目标深度。
 * 如果目标深度大于当前深度，将进入最外层block的透视（需要block_id参数）。
 * 如果目标深度小于当前深度，将退出到目标深度。
 *
 * @param manager 高维管理器
 * @param target_depth 目标深度
 * @param block_id 进入时使用的block_id（退出时忽略）
 * @return LV00_OK 成功，错误码表示失败原因
 */
int high_dim_zoom_to_level(HighDimManager *manager, int target_depth, int block_id);

/**
 * @brief 获取当前缩放级别信息
 *
 * 返回当前缩放级别的详细信息，包括深度和栈顶block_id。
 *
 * @param manager 高维管理器
 * @param out_depth 输出当前深度（可为NULL）
 * @param out_top_block_id 输出栈顶block_id（可为NULL，深度为0时无意义）
 * @return LV00_OK 成功，错误码表示失败原因
 */
int high_dim_get_zoom_level(const HighDimManager *manager, int *out_depth, int *out_top_block_id);

/**
 * @brief 设置缩放焦点
 *
 * 记录焦点block_id，供UI层在缩放时使用。
 *
 * @param manager 高维管理器
 * @param focus_block_id 焦点block_id
 * @return LV00_OK 成功，错误码表示失败原因
 */
int high_dim_set_focus_point(HighDimManager *manager, int focus_block_id);

/* ==================== 多投影视图 ==================== */

/**
 * @brief 创建多投影并排视图
 *
 * 为同一个高维块创建多个不同的投影视图
 *
 * @param manager 高维管理器
 * @param block_id 函数块ID
 * @param preset_indices 预设索引数组
 * @param preset_count 预设数量
 * @param view_ids 输出视图ID数组
 * @return 成功返回0，失败返回错误码
 */
int high_dim_create_multi_projection_view(HighDimManager *manager, int block_id, const int *preset_indices,
                                          int preset_count, int *view_ids);

/**
 * @brief 销毁多投影视图
 * @param manager 高维管理器
 * @param view_id 视图ID
 * @return 成功返回0，失败返回错误码
 */
int high_dim_destroy_multi_projection_view(HighDimManager *manager, int view_id);

/**
 * @brief 联动高亮元素
 *
 * 在一个视图中高亮元素时，其他视图联动高亮对应元素
 *
 * @param manager 高维管理器
 * @param view_ids 视图ID数组
 * @param view_count 视图数量
 * @param element_id 元素ID
 * @return 成功返回0，失败返回错误码
 */
int high_dim_link_highlight(HighDimManager *manager, const int *view_ids, int view_count, int element_id);

/* ==================== 序列化 ==================== */

/**
 * @brief 序列化投影预设到JSON
 * @param preset 投影预设
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 成功返回写入字节数，失败返回错误码
 */
int high_dim_preset_serialize_json(const HighDimProjectionPreset *preset, char *buffer, size_t buffer_size);

/**
 * @brief 从JSON反序列化投影预设
 * @param json JSON字符串
 * @param preset 输出投影预设
 * @return 成功返回0，失败返回错误码
 */
int high_dim_preset_deserialize_json(const char *json, HighDimProjectionPreset *preset);

/* ==================== 4D到3D投影 ==================== */

/**
 * @brief 将4D及以上坐标投影到3D空间
 *
 * 支持四种投影模式：
 *   - 透视投影（projection_mode=0）：以第4维作为深度，产生远小近大的效果
 *   - 正交投影（projection_mode=1）：保留选定维度，高维加权折叠
 *   - 旋转投影（projection_mode=2）：SO(4)旋转后正交投影到3D
 *   - 立体投影（projection_mode=3）：4D球面S^3到3D空间R^3的球极投影
 *
 * 正交投影增强：通过全局变量 g_ortho_selected_axes[3] 选择保留轴，
 * dim_count>4 时支持级联加权折叠（衰减权重公式：w_i = 1/(i-2)）。
 *
 * 投影质量指标：每次调用后在全局变量 g_project_to_3d_projection_trace
 * 中存储投影矩阵迹的近似值（3.0=完美保留，<3.0=信息损失）。
 *
 * @param coord_4d 高维坐标数组（x, y, z, w, ...）
 * @param dim_count 维度数（>= 1，dim_count=4为完整的4D投影）
 * @param camera_distance 摄像机距离（透视投影使用，> 0）
 * @param projection_mode 投影模式（0=透视, 1=正交, 2=旋转, 3=立体）
 * @param coord_3d 输出的3D坐标数组（长度>=3，调用者分配）
 * @return LV00_OK 成功，LV00_ERROR_INVALID_PARAM 参数无效
 */
int high_dim_project_to_3d(const double *coord_4d, int dim_count, double camera_distance, int projection_mode,
                           double *coord_3d);

/* ==================== 保真度计算（增强版） ==================== */

/**
 * @brief 计算投影保真度（增强版，基于约束图的五层度量）
 *
 * 综合五层度量计算加权保真度得分：
 *   - 第一层：维度可见性比例（基础）
 *   - 第二层：约束类型敏感度加权保留率（核心，按INCIDENCE/BETWEENNESS等类型差异加权）
 *   - 第三层：几何失真度量（角度失真 + 面积失真）
 *   - 第四层：拓扑保持度量（节点邻接关系变化）
 *   - 第五层：MDS Stress值（仅dim_count>=5，Frobenius范数近似）
 *
 * 5D以下权重：0.20*dim + 0.45*constraint + 0.20*distortion + 0.15*topology
 * 5D+权重：0.15*dim + 0.35*constraint + 0.20*distortion + 0.15*topology + 0.15*mds
 *
 * @param manager 高维管理器
 * @param block_id 高维块ID
 * @param constraint_graph 关联的约束图（可为NULL，此时仅用维度比）
 * @param stats 输出统计信息
 * @return LV00_OK 成功，错误码见返回值
 */
int high_dim_compute_fidelity(HighDimManager *manager, int block_id, const ConstraintGraph *constraint_graph,
                              HighDimVisibilityStats *stats);

/* ==================== 多视图管理（统一接口） ==================== */

/** 多视图管理操作：列出所有活跃视图 */
#define MULTIVIEW_OP_LIST 0
/** 多视图管理操作：获取活跃视图数量 */
#define MULTIVIEW_OP_COUNT 1
/** 多视图管理操作：清除所有视图 */
#define MULTIVIEW_OP_CLEAR 2
/** 多视图管理操作：按block_id过滤列出活跃视图（count复用为block_id输入） */
#define MULTIVIEW_OP_LIST_BY_BLOCK 3
/** 多视图管理操作：视图状态JSON导出（view_ids作为字符缓冲区） */
#define MULTIVIEW_OP_EXPORT_JSON 4
/** 多视图管理操作：获取指定索引的视图（view_ids[0]=输出view_id，count=输入索引） */
#define MULTIVIEW_OP_GET_VIEW 5
/** 多视图管理操作：设置指定视图为活跃状态（view_ids[0]=view_id） */
#define MULTIVIEW_OP_SET_ACTIVE 6
/** 多视图管理操作：克隆指定视图（view_ids[0]=源view_id，输出新view_id到count） */
#define MULTIVIEW_OP_CLONE_VIEW 7
/** 多视图管理操作：比较两个视图差异（view_ids[0]=view1, view_ids[1]=view2） */
#define MULTIVIEW_OP_COMPARE_VIEWS 8

/**
 * @brief 统一的多投影视图管理接口
 *
 * 通过 operation 参数选择操作：
 *   - MULTIVIEW_OP_LIST (0)：列出所有活跃视图ID到 view_ids 数组
 *   - MULTIVIEW_OP_COUNT (1)：获取活跃视图总数到 count
 *   - MULTIVIEW_OP_CLEAR (2)：清除所有视图（批量标记为inactive）
 *   - MULTIVIEW_OP_LIST_BY_BLOCK (3)：按block_id过滤列出，count复用为block_id输入/实际数输出
 *   - MULTIVIEW_OP_EXPORT_JSON (4)：导出JSON到view_ids字符缓冲区，count=缓冲区大小
 *   - MULTIVIEW_OP_GET_VIEW (5)：获取指定索引的视图，count=输入索引，view_ids[0]=输出view_id
 *   - MULTIVIEW_OP_SET_ACTIVE (6)：设置指定视图为活跃，view_ids[0]=view_id
 *   - MULTIVIEW_OP_CLONE_VIEW (7)：克隆指定视图，view_ids[0]=源view_id，count=输出新view_id
 *   - MULTIVIEW_OP_COMPARE_VIEWS (8)：比较两个视图，view_ids[0]=view1, view_ids[1]=view2
 *
 * @param manager 高维管理器
 * @param operation 操作类型（0/1/2/3/4）
 * @param view_ids 视图ID数组（LIST/LIST_BY_BLOCK）或字符缓冲区（EXPORT_JSON）
 * @param count 输入/输出参数（语义随operation变化）
 * @return LV00_OK 成功，错误码见返回值
 */
int high_dim_manage_multi_views(HighDimManager *manager, int operation, int *view_ids, int *count);

/* ==================== 工具函数 ==================== */

/**
 * @brief 验证维度映射配置
 * @param dimension_count 维度数
 * @param mappings 映射配置数组
 * @param mapping_count 映射数量
 * @return 有效返回1，无效返回0
 */
int high_dim_validate_mapping(int dimension_count, const HighDimAxisMapping *mappings, int mapping_count);

/**
 * @brief 获取映射类型字符串
 * @param mapping_type 映射类型
 * @return 类型字符串
 */
const char *high_dim_mapping_type_to_string(HighDimMappingType mapping_type);

/**
 * @brief 从字符串解析映射类型
 * @param str 类型字符串
 * @return 映射类型，无效返回-1
 */
HighDimMappingType high_dim_mapping_type_from_string(const char *str);

/**
 * @brief 计算被折叠维度的信息标注
 * @param preset 投影预设
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 成功返回0，失败返回错误码
 */
int high_dim_get_folded_dimensions_info(const HighDimProjectionPreset *preset, char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* LV00_HIGH_DIM_H */
