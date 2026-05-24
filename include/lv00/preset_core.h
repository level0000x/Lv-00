/**
 * @file preset_core.h
 * @brief 预设函数块系统 - 核心接口
 *
 * @details 定义预设函数块系统的核心数据结构和接口。
 *          这是预设系统的主头文件，其他模块通过此头文件访问核心功能。
 *
 * @version 5.0.0
 * @author Lv-00 Project
 */

#ifndef LV00_PRESET_CORE_H
#define LV00_PRESET_CORE_H

#include "preset_common.h"
#include "func_block.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 前向声明
 * ============================================================ */

/**
 * @brief 预设库句柄（不透明类型）
 */
typedef struct PresetLibrary* PresetLibraryHandle;

/**
 * @brief 预设条目句柄（不透明类型）
 */
typedef struct PresetEntry* PresetEntryHandle;

/**
 * @brief 预设实例句柄（不透明类型）
 */
typedef struct PresetInstance* PresetInstanceHandle;

/* ============================================================
 * 核心数据结构
 * ============================================================ */

/**
 * @brief 预设版本信息
 */
typedef struct {
    int major;      /**< 主版本号 */
    int minor;      /**< 次版本号 */
    int patch;      /**< 修订版本号 */
    const char *build_info;  /**< 构建信息（可选） */
} PresetVersion;

/**
 * @brief 预设统计信息
 */
typedef struct {
    int total_count;        /**< 总预设数量 */
    int builtin_count;      /**< 内置预设数量 */
    int custom_count;       /**< 自定义预设数量 */
    int active_count;       /**< 活跃预设数量 */
    int category_counts[PRESET_CATEGORY_COUNT];  /**< 各类别数量 */
} PresetStatistics;

/**
 * @brief 预设查询条件
 */
typedef struct {
    const char *name_pattern;           /**< 名称模式（支持通配符） */
    PresetCategory category;            /**< 类别筛选 */
    PresetProperty required_properties;   /**< 必需属性 */
    PresetProperty forbidden_properties;  /**< 禁止属性 */
    const char *complexity_max;         /**< 最大复杂度 */
    int min_inputs;                     /**< 最小输入数量 */
    int max_inputs;                     /**< 最大输入数量 */
    int min_outputs;                    /**< 最小输出数量 */
    int max_outputs;                    /**< 最大输出数量 */
    bool search_description;            /**< 是否搜索描述 */
} PresetQueryCriteria;

/**
 * @brief 预设查询结果
 */
typedef struct {
    const char **names;     /**< 预设名称数组 */
    int count;              /**< 结果数量 */
    int total_matches;      /**< 总匹配数（可能超过返回数量） */
} PresetQueryResult;

/**
 * @brief 预设实例化选项
 */
typedef struct {
    bool validate_types;        /**< 验证类型 */
    bool validate_constraints;  /**< 验证约束 */
    bool auto_connect;          /**< 自动连接 */
    bool cache_result;          /**< 缓存结果 */
    int timeout_ms;             /**< 超时时间（毫秒，0表示无限制） */
} PresetInstantiateOptions;

/**
 * @brief 预设执行上下文
 */
typedef struct {
    void *user_data;                    /**< 用户数据 */
    int (*progress_callback)(int step, int total, void *user_data);  /**< 进度回调 */
    bool (*cancel_callback)(void *user_data);  /**< 取消检查回调 */
    int max_iterations;                 /**< 最大迭代次数 */
    double precision;                   /**< 精度要求 */
} PresetExecutionContext;

/* ============================================================
 * 库生命周期管理
 * ============================================================ */

/**
 * @brief 初始化预设库
 *
 * 在使用预设系统前必须调用此函数。
 * 线程安全：是（内部使用互斥锁）
 *
 * @return true 初始化成功
 * @return false 初始化失败（可能已初始化）
 */
bool preset_library_init(void);

/**
 * @brief 关闭预设库
 *
 * 释放所有资源，清理预设库。
 * 线程安全：是
 *
 * @return true 关闭成功
 * @return false 关闭失败（可能未初始化）
 */
bool preset_library_shutdown(void);

/**
 * @brief 检查预设库是否已初始化
 *
 * @return true 已初始化
 * @return false 未初始化
 */
bool preset_library_is_initialized(void);

/**
 * @brief 获取预设库版本信息
 *
 * @return 版本信息（静态数据，无需释放）
 */
const PresetVersion* preset_library_get_version(void);

/**
 * @brief 获取预设库统计信息
 *
 * @param stats 输出统计信息
 * @return true 成功
 * @return false 失败
 */
bool preset_library_get_statistics(PresetStatistics *stats);

/**
 * @brief 重置预设库到初始状态
 *
 * 清除所有自定义预设，保留内置预设。
 *
 * @return true 成功
 * @return false 失败
 */
bool preset_library_reset(void);

/* ============================================================
 * 预设注册与注销
 * ============================================================ */

/**
 * @brief 注册内置预设
 *
 * 注册所有内置预设函数块。通常在库初始化后调用。
 *
 * @return 成功注册的预设数量
 */
int preset_register_builtin(void);

/**
 * @brief 注册自定义预设
 *
 * @param metadata 预设元数据
 * @param template_fb 模板函数块（可选，可为NULL）
 * @param out_entry 输出条目句柄（可选）
 * @return true 注册成功
 * @return false 注册失败
 */
bool preset_register_custom(const PresetMetadata *metadata,
                           const FuncBlock *template_fb,
                           PresetEntryHandle *out_entry);

/**
 * @brief 注销预设
 *
 * 只能注销自定义预设，不能注销内置预设。
 *
 * @param name 预设名称
 * @return true 注销成功
 * @return false 注销失败（可能不存在或为内置预设）
 */
bool preset_unregister(const char *name);

/**
 * @brief 批量注册预设
 *
 * @param metadatas 元数据数组
 * @param count 数量
 * @return 成功注册的数量
 */
int preset_register_batch(const PresetMetadata *metadatas, int count);

/* ============================================================
 * 预设查询与检索
 * ============================================================ */

/**
 * @brief 查找预设
 *
 * @param name 预设名称
 * @return 预设条目句柄，未找到返回NULL
 */
PresetEntryHandle preset_find(const char *name);

/**
 * @brief 获取预设元数据
 *
 * @param entry 预设条目句柄
 * @return 元数据指针（只读，生命周期与条目相同）
 */
const PresetMetadata* preset_get_metadata(PresetEntryHandle entry);

/**
 * @brief 高级查询预设
 *
 * @param criteria 查询条件
 * @param out_result 输出结果（调用者需使用 preset_query_result_free 释放）
 * @return true 查询成功
 * @return false 查询失败
 */
bool preset_query(const PresetQueryCriteria *criteria,
                 PresetQueryResult **out_result);

/**
 * @brief 释放查询结果
 *
 * @param result 查询结果
 */
void preset_query_result_free(PresetQueryResult *result);

/**
 * @brief 按类别列出预设
 *
 * @param category 类别
 * @param out_names 输出名称数组（调用者需释放）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_list_by_category(PresetCategory category,
                            char ***out_names,
                            int *out_count);

/**
 * @brief 获取所有预设名称
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_list_all(char ***out_names, int *out_count);

/**
 * @brief 检查预设是否存在
 *
 * @param name 预设名称
 * @return true 存在
 * @return false 不存在
 */
bool preset_exists(const char *name);

/**
 * @brief 检查预设是否为内置
 *
 * @param name 预设名称
 * @return true 是内置预设
 * @return false 不是内置预设或不存在
 */
bool preset_is_builtin(const char *name);

/* ============================================================
 * 预设实例化
 * ============================================================ */

/**
 * @brief 实例化预设
 *
 * @param name 预设名称
 * @param input_nodes 输入节点ID数组
 * @param input_count 输入数量
 * @param options 实例化选项（可为NULL，使用默认选项）
 * @param out_instance 输出实例句柄
 * @return true 实例化成功
 * @return false 实例化失败
 */
bool preset_instantiate(const char *name,
                       const int *input_nodes,
                       int input_count,
                       const PresetInstantiateOptions *options,
                       PresetInstanceHandle *out_instance);

/**
 * @brief 批量实例化预设
 *
 * @param names 预设名称数组
 * @param input_nodes_array 输入节点ID数组的数组
 * @param input_counts 输入数量数组
 * @param count 预设数量
 * @param options 实例化选项数组（可为NULL，统一使用默认选项）
 * @param out_instances 输出实例句柄数组（调用者需释放）
 * @return 成功实例化的数量
 */
int preset_instantiate_batch(const char **names,
                            const int **input_nodes_array,
                            const int *input_counts,
                            int count,
                            const PresetInstantiateOptions *options,
                            PresetInstanceHandle **out_instances);

/**
 * @brief 销毁预设实例
 *
 * @param instance 实例句柄
 */
void preset_instance_destroy(PresetInstanceHandle instance);

/**
 * @brief 获取实例的函数块
 *
 * @param instance 实例句柄
 * @return 函数块指针（只读）
 */
const FuncBlock* preset_instance_get_func_block(PresetInstanceHandle instance);

/**
 * @brief 获取实例的输出节点ID
 *
 * @param instance 实例句柄
 * @param out_output_ids 输出节点ID数组（调用者需释放）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_instance_get_outputs(PresetInstanceHandle instance,
                                int **out_output_ids,
                                int *out_count);

/* ============================================================
 * 预设执行
 * ============================================================ */

/**
 * @brief 执行预设实例
 *
 * @param instance 实例句柄
 * @param context 执行上下文（可为NULL）
 * @return true 执行成功
 * @return false 执行失败
 */
bool preset_instance_execute(PresetInstanceHandle instance,
                            const PresetExecutionContext *context);

/**
 * @brief 验证预设实例
 *
 * @param instance 实例句柄
 * @param out_is_valid 输出是否有效
 * @param out_error_message 错误消息（可选，调用者需释放）
 * @return true 验证完成
 * @return false 验证过程出错
 */
bool preset_instance_validate(PresetInstanceHandle instance,
                             bool *out_is_valid,
                             char **out_error_message);

/* ============================================================
 * 预设组合
 * ============================================================ */

/**
 * @brief 预设组合模式
 */
typedef enum {
    PRESET_COMPOSE_SEQUENCE,    /**< 顺序执行 */
    PRESET_COMPOSE_PARALLEL,    /**< 并行执行 */
    PRESET_COMPOSE_FEEDBACK,    /**< 反馈循环 */
    PRESET_COMPOSE_BRANCH,      /**< 条件分支 */
    PRESET_COMPOSE_PIPE,        /**< 管道（数据流） */
} PresetComposeMode;

/**
 * @brief 预设组合描述
 */
typedef struct {
    const char **preset_names;  /**< 预设名称数组 */
    int count;                  /**< 预设数量 */
    PresetComposeMode mode;     /**< 组合模式 */
    const int **mappings;       /**< 参数映射（可选） */
    const char *new_name;       /**< 新预设名称 */
} PresetComposition;

/**
 * @brief 组合预设
 *
 * @param composition 组合描述
 * @param out_new_entry 输出新条目句柄
 * @return true 组合成功
 * @return false 组合失败
 */
bool preset_compose(const PresetComposition *composition,
                   PresetEntryHandle *out_new_entry);

/**
 * @brief 绑定预设参数
 *
 * @param preset_name 预设名称
 * @param param_index 参数索引
 * @param value 绑定值（节点ID）
 * @param out_new_name 输出新预设名称（调用者需释放）
 * @return true 绑定成功
 * @return false 绑定失败
 */
bool preset_bind_parameter(const char *preset_name,
                          int param_index,
                          int value,
                          char **out_new_name);

/* ============================================================
 * 预设文档
 * ============================================================ */

/**
 * @brief 生成预设文档
 *
 * @param name 预设名称
 * @param format 格式（"text", "html", "markdown"）
 * @param out_document 输出文档（调用者需释放）
 * @return true 生成成功
 * @return false 生成失败
 */
bool preset_generate_documentation(const char *name,
                                  const char *format,
                                  char **out_document);

/**
 * @brief 生成预设库完整文档
 *
 * @param format 格式
 * @param out_document 输出文档（调用者需释放）
 * @return true 生成成功
 * @return false 生成失败
 */
bool preset_generate_library_documentation(const char *format,
                                          char **out_document);

/**
 * @brief 获取预设使用示例
 *
 * @param name 预设名称
 * @param out_example 输出示例代码（调用者需释放）
 * @return true 成功
 * @return false 失败
 */
bool preset_get_usage_example(const char *name,
                             char **out_example);

/* ============================================================
 * 预设序列化
 * ============================================================ */

/**
 * @brief 序列化预设
 *
 * @param entry 预设条目句柄
 * @param out_data 输出数据（调用者需释放）
 * @param out_size 输出数据大小
 * @return true 成功
 * @return false 失败
 */
bool preset_serialize(PresetEntryHandle entry,
                     uint8_t **out_data,
                     size_t *out_size);

/**
 * @brief 反序列化预设
 *
 * @param data 数据
 * @param size 数据大小
 * @param out_entry 输出条目句柄
 * @return true 成功
 * @return false 失败
 */
bool preset_deserialize(const uint8_t *data,
                       size_t size,
                       PresetEntryHandle *out_entry);

/**
 * @brief 导出预设到文件
 *
 * @param name 预设名称
 * @param filepath 文件路径
 * @return true 成功
 * @return false 失败
 */
bool preset_export_to_file(const char *name, const char *filepath);

/**
 * @brief 从文件导入预设
 *
 * @param filepath 文件路径
 * @param out_name 输出预设名称（可选，调用者需释放）
 * @return true 成功
 * @return false 失败
 */
bool preset_import_from_file(const char *filepath, char **out_name);

/* ============================================================
 * 错误处理
 * ============================================================ */

/**
 * @brief 获取最后一次错误信息
 *
 * @return 错误信息（静态字符串，无需释放）
 */
const char* preset_get_last_error(void);

/**
 * @brief 清除错误状态
 */
void preset_clear_error(void);

/**
 * @brief 设置错误回调
 *
 * @param callback 错误回调函数
 * @param user_data 用户数据
 */
void preset_set_error_callback(void (*callback)(const char *error,
                                               void *user_data),
                              void *user_data);

/**
 * @brief 释放预设条目句柄持有的资源
 *
 * @param entry 预设条目句柄，可为 NULL（no-op）
 */
void preset_release(PresetEntryHandle entry);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_CORE_H */
