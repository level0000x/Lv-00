/**
 * @file bootstrap_test.h
 * @brief Lv-00 自举差分测试框架公共接口
 *
 * @details 提供自举验证的核心功能：
 *   1. 差分测试：C API 与几何层结果比较
 *   2. 随机用例生成器：自动生成测试输入
 *   3. 图同构比较器：验证图结构等价
 *   4. 原语包装器：13 个最小原语的差分测试
 *   5. 测试预言机：验证执行结果正确性
 *
 * @author Lv-00 Project
 * @version 1.1.0
 * @date 2026-05-29
 */
#ifndef lv_BOOTSTRAP_TEST_H
#define lv_BOOTSTRAP_TEST_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* ============== 前向声明 ============== */
typedef struct BootstrapDiffTest BootstrapDiffTest;
typedef struct BootstrapDiffTestResult BootstrapDiffTestResult;
typedef struct RandomGenerator RandomGenerator;
typedef struct RandomGeneratorConfig RandomGeneratorConfig;
typedef struct GraphIsomorphismComparator GraphIsomorphismComparator;
typedef struct PrimitiveWrapper PrimitiveWrapper;
typedef struct PrimitiveTestResult PrimitiveTestResult;
typedef struct TestOracle TestOracle;
/* ============== 差分测试结果 ============== */
/**
 * @brief 差分比较结果类型
 */
typedef enum {
    DIFF_RESULT_EQUAL,          /**< 完全相等 */
    DIFF_RESULT_ISO_EQUAL,      /**< 同构等价 */
    DIFF_RESULT_SEMANTIC_EQUAL, /**< 语义等价 */
    DIFF_RESULT_DIFFERENT,      /**< 不同 */
    DIFF_RESULT_ERROR,          /**< 执行错误 */
    DIFF_RESULT_TIMEOUT         /**< 执行超时 */
} DiffComparisonResult;
/**
 * @brief 差分测试结果结构
 */
struct BootstrapDiffTestResult {
    DiffComparisonResult comparison; /**< 比较结果 */
    bool passed;                     /**< 是否通过 */

    /* 详细信息 */
    char *c_api_output;     /**< C API 输出 */
    char *geo_layer_output; /**< 几何层输出 */
    char *diff_description; /**< 差异描述 */

    /* 性能数据 */
    int64_t c_api_time_ns;     /**< C API 执行时间 */
    int64_t geo_layer_time_ns; /**< 几何层执行时间 */

    /* 错误信息 */
    char *error_message; /**< 错误消息 */
};
/* ============== 随机生成器配置 ============== */
/**
 * @brief 随机生成器配置结构
 */
struct RandomGeneratorConfig {
    /* 几何实体数量范围 */
    uint32_t min_points;  /**< 最小点数 */
    uint32_t max_points;  /**< 最大点数 */
    uint32_t min_lines;   /**< 最小线数 */
    uint32_t max_lines;   /**< 最大线数 */
    uint32_t min_circles; /**< 最小圆数 */
    uint32_t max_circles; /**< 最大圆数 */

    /* 约束密度 */
    double constraint_density; /**< 约束密度 (0.0 - 1.0) */

    /* 数值范围 */
    double coord_min; /**< 坐标最小值 */
    double coord_max; /**< 坐标最大值 */

    /* 特殊配置 */
    bool allow_degenerate;      /**< 允许退化情况 */
    bool allow_overconstrained; /**< 允许过约束 */
    bool use_symbolic_coords;   /**< 使用符号坐标 */

    /* 种子 */
    uint64_t seed; /**< 随机种子 */
};
/* ============== 原语测试结果 ============== */
/**
 * @brief 原语测试结果结构
 */
struct PrimitiveTestResult {
    const char *primitive_name;      /**< 原语名称 */
    DiffComparisonResult comparison; /**< 比较结果 */
    bool passed;                     /**< 是否通过 */

    char *input_description; /**< 输入描述 */
    char *c_api_result;      /**< C API 结果 */
    char *geo_layer_result;  /**< 几何层结果 */
    char *error_message;     /**< 错误消息 */

    int64_t execution_time_ns; /**< 执行时间 */
};
/* ============== 框架初始化 ============== */
/**
 * @brief 初始化自举测试框架
 * @return 是否成功
 */
bool bootstrap_test_framework_init(void);
/**
 * @brief 清理自举测试框架
 */
void lv_bootstrap_test_framework_cleanup(void);
/**
 * @brief 检查框架是否已初始化
 * @return 是否已初始化
 */
bool bootstrap_test_framework_is_initialized(void);
/* ============== 差分测试 API ============== */
/**
 * @brief 创建差分测试
 * @param test_name 测试名称
 * @param dsl_source DSL 源码（可选）
 * @return 差分测试对象
 */
BootstrapDiffTest *bootstrap_diff_test_create(const char *test_name, const char *dsl_source);
/**
 * @brief 销毁差分测试
 * @param test 差分测试对象
 */
void bootstrap_diff_test_destroy(BootstrapDiffTest *test);
/**
 * @brief 执行差分测试
 * @param test 差分测试对象
 * @return 测试结果
 */
BootstrapDiffTestResult *bootstrap_diff_test_run(BootstrapDiffTest *test);
/**
 * @brief 销毁差分测试结果
 * @param result 测试结果
 */
void bootstrap_diff_test_result_destroy(BootstrapDiffTestResult *result);
/**
 * @brief 执行批量差分测试
 * @param tests 测试数组
 * @param count 测试数量
 * @param out_results 输出结果数组
 * @return 成功执行的测试数量
 */
uint32_t bootstrap_diff_test_run_batch(BootstrapDiffTest **tests, uint32_t count,
                                       BootstrapDiffTestResult **out_results);
/* ============== 随机生成器 API ============== */
/**
 * @brief 创建随机生成器
 * @param config 配置参数
 * @return 生成器对象
 */
RandomGenerator *random_generator_create(const RandomGeneratorConfig *config);
/**
 * @brief 销毁随机生成器
 * @param gen 生成器对象
 */
void random_generator_destroy(RandomGenerator *gen);
/**
 * @brief 获取默认配置
 * @return 默认配置结构
 */
RandomGeneratorConfig random_generator_default_config(void);
/**
 * @brief 生成随机约束图
 * @param gen 生成器对象
 * @return 约束图（需由调用者销毁）
 */
void *random_generator_generate_graph(RandomGenerator *gen);
/**
 * @brief 生成随机 DSL 源码
 * @param gen 生成器对象
 * @return DSL 源码字符串（需由调用者释放）
 */
char *random_generator_generate_dsl(RandomGenerator *gen);
/**
 * @brief 批量生成约束图
 * @param gen 生成器对象
 * @param out_graphs 输出图数组
 * @param count 生成数量
 * @return 实际生成数量
 */
uint32_t random_generator_generate_batch(RandomGenerator *gen, void **out_graphs, uint32_t count);
/**
 * @brief 重置生成器种子
 * @param gen 生成器对象
 * @param seed 新种子
 */
void random_generator_reset_seed(RandomGenerator *gen, uint64_t seed);
/* ============== 图同构比较器 API ============== */
/**
 * @brief 创建图同构比较器
 * @return 比较器对象
 */
GraphIsomorphismComparator *graph_isomorphism_create(void);
/**
 * @brief 销毁图同构比较器
 * @param comp 比较器对象
 */
void graph_isomorphism_destroy(GraphIsomorphismComparator *comp);
/**
 * @brief 配置比较器
 * @param comp 比较器对象
 * @param ignore_ids 是否忽略 ID
 * @param compare_coords 是否比较坐标
 * @param coord_tolerance 坐标容差
 */
void graph_isomorphism_configure(GraphIsomorphismComparator *comp, bool ignore_ids, bool compare_coords,
                                 double coord_tolerance);
/**
 * @brief 比较两个图是否同构
 * @param comp 比较器对象
 * @param graph_a 图 A
 * @param graph_b 图 B
 * @return 是否同构
 */
bool graph_isomorphism_compare(GraphIsomorphismComparator *comp, const void *graph_a, const void *graph_b);
/**
 * @brief 计算图哈希
 * @param graph 图对象
 * @return 哈希值
 */
uint64_t graph_isomorphism_hash(const void *graph);
/**
 * @brief 查找同构映射
 * @param comp 比较器对象
 * @param graph_a 图 A
 * @param graph_b 图 B
 * @param out_node_mapping 输出节点映射
 * @param out_constraint_mapping 输出约束映射
 * @return 是否找到映射
 */
bool graph_isomorphism_find_mapping(GraphIsomorphismComparator *comp, const void *graph_a, const void *graph_b,
                                    int **out_node_mapping, int **out_constraint_mapping);
/* ============== 原语包装器 API ============== */
/**
 * @brief 初始化原语包装器
 * @return 是否成功
 */
bool primitive_wrapper_init(void);
/**
 * @brief 清理原语包装器
 */
void lv_primitive_wrapper_cleanup(void);
/**
 * @brief 注册原语
 * @param name 原语名称
 * @param c_api_func C API 函数指针
 * @param param_types 参数类型数组
 * @param param_count 参数数量
 * @param return_type 返回类型
 * @return 是否成功
 */
bool primitive_wrapper_register(const char *name, void *c_api_func, const char **param_types, uint32_t param_count,
                                const char *return_type);
/**
 * @brief 执行单个原语差分测试
 * @param name 原语名称
 * @param params 参数数组
 * @return 测试结果
 */
PrimitiveTestResult *primitive_wrapper_test(const char *name, void **params);
/**
 * @brief 销毁原语测试结果
 * @param result 测试结果
 */
void primitive_test_result_destroy(PrimitiveTestResult *result);
/**
 * @brief 执行所有原语的差分测试
 * @param out_results 输出结果数组
 * @param max_count 最大数量
 * @return 实际测试数量
 */
uint32_t primitive_wrapper_test_all(PrimitiveTestResult **out_results, uint32_t max_count);
/**
 * @brief 获取原语统计信息
 * @param name 原语名称
 * @param out_total 输出总测试数
 * @param out_passed 输出通过数
 * @param out_failed 输出失败数
 */
void primitive_wrapper_get_stats(const char *name, uint32_t *out_total, uint32_t *out_passed, uint32_t *out_failed);
/* ============== 测试预言机 API ============== */
/**
 * @brief 创建测试预言机
 * @return 预言机对象
 */
TestOracle *test_oracle_create(void);
/**
 * @brief 销毁测试预言机
 * @param oracle 预言机对象
 */
void test_oracle_destroy(TestOracle *oracle);
/**
 * @brief 验证归一化幂等性
 * @param oracle 预言机对象
 * @param graph 约束图
 * @return 是否幂等
 */
bool test_oracle_verify_normalization_idempotent(TestOracle *oracle, void *graph);
/**
 * @brief 验证求解正确性
 * @param oracle 预言机对象
 * @param graph 约束图
 * @param solution 求解结果
 * @return 是否正确
 */
bool test_oracle_verify_solution_correct(TestOracle *oracle, const void *graph, const void *solution);
/**
 * @brief 验证证明有效性
 * @param oracle 预言机对象
 * @param trace 证明追踪
 * @return 是否有效
 */
bool test_oracle_verify_proof_valid(TestOracle *oracle, const void *trace);
/**
 * @brief 验证序列化往返正确性
 * @param oracle 预言机对象
 * @param graph 原始图
 * @param serialized 序列化数据
 * @param deserialized 反序列化图
 * @return 是否正确
 */
bool test_oracle_verify_serialize_roundtrip(TestOracle *oracle, const void *graph, const char *serialized,
                                            const void *deserialized);
/* ============== 报告生成 ============== */
/**
 * @brief 生成差分测试报告
 * @param results 结果数组
 * @param count 结果数量
 * @param format 格式（"json", "html", "text"）
 * @return 报告字符串
 */
char *bootstrap_test_generate_report(BootstrapDiffTestResult **results, uint32_t count, const char *format);
/**
 * @brief 将报告写入文件
 * @param results 结果数组
 * @param count 结果数量
 * @param filepath 文件路径
 * @param format 格式
 * @return 是否成功
 */
bool bootstrap_test_write_report(BootstrapDiffTestResult **results, uint32_t count, const char *filepath,
                                 const char *format);
#ifdef __cplusplus
}
#endif
/* ============================================================
 * 向后兼容别名（旧名称 → lv_ 前缀新名称）
 * ============================================================ */
#define bootstrap_test_framework_cleanup lv_bootstrap_test_framework_cleanup
#define primitive_wrapper_cleanup lv_primitive_wrapper_cleanup
#endif /* lv_BOOTSTRAP_TEST_H */
