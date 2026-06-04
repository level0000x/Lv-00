# Lv-00 自举差分测试框架设计规范

> **版本**: 1.0.0-draft  
> **日期**: 2026-05-29  
> **状态**: 设计阶段  
> **依赖**: Lv-00 v5.0.0 测试框架 + 13 个最小原语

---

## 一、概述

自举差分测试框架用于验证几何层与 C 核的语义一致性。核心思想：

```
同一个输入 → C API 执行 → 结果 A
同一个输入 → 几何层执行 → 结果 B
比较 A 与 B 是否语义等价
```

---

## 二、核心组件

### 2.1 组件架构

```
bootstrap_test_framework/
├── include/
│   ├── bootstrap_test.h          # 公共接口
│   ├── differential_test.h       # 差分测试核心
│   ├── random_generator.h        # 随机用例生成器
│   ├── graph_isomorphism.h       # 图同构比较器
│   ├── primitive_wrapper.h       # 13 原语包装器
│   └── test_oracle.h             # 测试预言机
├── src/
│   ├── bootstrap_test.c          # 框架初始化
│   ├── differential_test.c       # 差分测试实现
│   ├── random_generator.c        # 随机生成器实现
│   ├── graph_isomorphism.c       # 图同构算法
│   ├── primitive_wrapper.c       # 原语包装实现
│   └── test_oracle.c             # 预言机实现
└── tests/
    ├── test_differential.c       # 差分测试自测
    ├── test_generator.c          # 生成器自测
    ├── test_isomorphism.c        # 同构比较自测
    └── test_primitives.c         # 原语包装自测
```

---

## 三、差分测试核心

### 3.1 差分测试流程

```c
typedef struct BootstrapDiffTest {
    /* 输入 */
    char *dsl_source;              /* Lv-00 DSL 源码 */
    ConstraintGraph *input_graph;  /* 输入约束图 */
    
    /* C API 执行路径 */
    struct {
        ConstraintGraph *graph;
        NormalizationResult *norm_result;
        SolverResult *solve_result;
        ProofTrace *proof_trace;
    } c_api_result;
    
    /* 几何层执行路径 */
    struct {
        GeometricIR *ir;
        GeometricResult *result;
    } geo_layer_result;
    
    /* 比较结果 */
    DiffComparisonResult comparison;
    
    /* 元数据 */
    char *test_name;
    uint64_t timestamp;
    bool passed;
} BootstrapDiffTest;
```

### 3.2 差分测试执行

```c
/* 执行差分测试 */
BootstrapDiffTestResult bootstrap_diff_test_run(BootstrapDiffTest *test);

/* 比较结果 */
typedef enum {
    DIFF_RESULT_EQUAL,             /* 完全相等 */
    DIFF_RESULT_ISO_EQUAL,         /* 同构等价 */
    DIFF_RESULT_SEMANTIC_EQUAL,    /* 语义等价 */
    DIFF_RESULT_DIFFERENT,         /* 不同 */
    DIFF_RESULT_ERROR              /* 执行错误 */
} DiffComparisonResult;
```

---

## 四、随机用例生成器

### 4.1 生成策略

```c
typedef struct RandomGeneratorConfig {
    /* 几何实体数量范围 */
    uint32_t min_points;
    uint32_t max_points;
    uint32_t min_lines;
    uint32_t max_lines;
    uint32_t min_circles;
    uint32_t max_circles;
    
    /* 约束密度 */
    double constraint_density;     /* 0.0 - 1.0 */
    
    /* 数值范围 */
    double coord_min;
    double coord_max;
    
    /* 特殊配置 */
    bool allow_degenerate;         /* 允许退化情况 */
    bool allow_overconstrained;    /* 允许过约束 */
    bool use_symbolic_coords;      /* 使用符号坐标 */
    
    /* 种子 */
    uint64_t seed;
} RandomGeneratorConfig;
```

### 4.2 生成器 API

```c
/* 创建随机生成器 */
RandomGenerator *random_generator_create(const RandomGeneratorConfig *config);

/* 生成随机约束图 */
ConstraintGraph *random_generator_generate_graph(RandomGenerator *gen);

/* 生成随机 DSL 源码 */
char *random_generator_generate_dsl(RandomGenerator *gen);

/* 生成随机证明目标 */
Proposition *random_generator_generate_proposition(RandomGenerator *gen, 
                                                    const ConstraintGraph *graph);

/* 批量生成 */
int random_generator_generate_batch(RandomGenerator *gen,
                                     ConstraintGraph **out_graphs,
                                     uint32_t count);
```

---

## 五、图同构比较器

### 5.1 同构比较算法

使用 **VF2 算法**（现有 rewrite.h 中已实现）进行子图同构检测。

```c
typedef struct GraphIsomorphismComparator {
    /* 配置 */
    struct {
        bool ignore_node_ids;      /* 忽略节点 ID */
        bool ignore_constraint_ids;/* 忽略约束 ID */
        bool compare_coordinates;  /* 比较坐标值 */
        bool compare_types;        /* 比较类型 */
        double coord_tolerance;    /* 坐标容差 */
    } config;
    
    /* 结果缓存 */
    uint64_t *hash_cache;
    uint32_t cache_size;
} GraphIsomorphismComparator;
```

### 5.2 同构比较 API

```c
/* 创建同构比较器 */
GraphIsomorphismComparator *graph_isomorphism_create(void);

/* 比较两个图是否同构 */
bool graph_isomorphism_compare(GraphIsomorphismComparator *comp,
                                const ConstraintGraph *graph_a,
                                const ConstraintGraph *graph_b);

/* 计算图哈希（用于快速比较） */
uint64_t graph_isomorphism_hash(const ConstraintGraph *graph);

/* 查找同构映射 */
bool graph_isomorphism_find_mapping(GraphIsomorphismComparator *comp,
                                     const ConstraintGraph *graph_a,
                                     const ConstraintGraph *graph_b,
                                     int **out_node_mapping,
                                     int **out_constraint_mapping);
```

---

## 六、原语包装器

### 6.1 原语包装设计

将 13 个最小原语包装为可测试的函数：

```c
typedef struct PrimitiveWrapper {
    /* 原语名称 */
    const char *name;
    
    /* C API 函数指针 */
    void *c_api_func;
    
    /* 几何层函数指针（待实现） */
    void *geo_layer_func;
    
    /* 参数类型描述 */
    struct {
        const char **param_types;
        uint32_t param_count;
        const char *return_type;
    } signature;
    
    /* 测试状态 */
    uint32_t test_count;
    uint32_t pass_count;
    uint32_t fail_count;
} PrimitiveWrapper;
```

### 6.2 原语包装 API

```c
/* 初始化原语包装器 */
bool primitive_wrapper_init(void);

/* 注册原语 */
bool primitive_wrapper_register(const char *name,
                                 void *c_api_func,
                                 const char **param_types,
                                 uint32_t param_count,
                                 const char *return_type);

/* 执行原语差分测试 */
PrimitiveTestResult primitive_wrapper_test(const char *name,
                                            void **params);

/* 批量测试所有原语 */
PrimitiveBatchResult primitive_wrapper_test_all(void);
```

---

## 七、测试预言机

### 7.1 预言机设计

预言机用于验证执行结果的正确性：

```c
typedef struct TestOracle {
    /* 验证规则 */
    struct {
        /* 归一化验证 */
        bool (*verify_normalization)(const ConstraintGraph *before,
                                      const ConstraintGraph *after,
                                      const NormalizationResult *result);
        
        /* 求解验证 */
        bool (*verify_solution)(const ConstraintGraph *graph,
                                 const SolverResult *result);
        
        /* 证明验证 */
        bool (*verify_proof)(const Proposition *goal,
                             const ProofTrace *trace);
        
        /* 合一验证 */
        bool (*verify_unify)(const ConstraintGraph *construction,
                             const ConstraintGraph *proposition,
                             const UnifyResult *result);
    } rules;
} TestOracle;
```

### 7.2 预言机 API

```c
/* 创建预言机 */
TestOracle *test_oracle_create(void);

/* 验证归一化幂等性 */
bool test_oracle_verify_normalization_idempotent(TestOracle *oracle,
                                                  ConstraintGraph *graph);

/* 验证求解正确性 */
bool test_oracle_verify_solution_correct(TestOracle *oracle,
                                          const ConstraintGraph *graph,
                                          const SolverResult *result);

/* 验证证明有效性 */
bool test_oracle_verify_proof_valid(TestOracle *oracle,
                                     const ProofTrace *trace);
```

---

## 八、测试套件

### 8.1 核心测试套件

```c
/* 注册自举测试套件 */
void bootstrap_test_register_suite(void);

/* 测试套件定义 */
LV00_TEST_SUITE(BootstrapCore)
{
    /* 原语差分测试 */
    LV00_TEST(BootstrapCore, geo_create_node_diff);
    LV00_TEST(BootstrapCore, geo_create_constraint_diff);
    LV00_TEST(BootstrapCore, geo_solve_diff);
    LV00_TEST(BootstrapCore, geo_normalize_diff);
    LV00_TEST(BootstrapCore, geo_rewrite_diff);
    LV00_TEST(BootstrapCore, geo_unify_diff);
    LV00_TEST(BootstrapCore, geo_pack_diff);
    LV00_TEST(BootstrapCore, geo_instantiate_diff);
    LV00_TEST(BootstrapCore, geo_prove_diff);
    LV00_TEST(BootstrapCore, geo_export_diff);
    LV00_TEST(BootstrapCore, geo_serialize_diff);
    LV00_TEST(BootstrapCore, geo_deserialize_diff);
    LV00_TEST(BootstrapCore, geo_query_diff);
    
    /* 归一化幂等性测试 */
    LV00_TEST(BootstrapCore, normalization_idempotent);
    
    /* 序列化往返测试 */
    LV00_TEST(BootstrapCore, serialize_roundtrip);
    
    /* 图同构测试 */
    LV00_TEST(BootstrapCore, graph_isomorphism_basic);
    LV00_TEST(BootstrapCore, graph_isomorphism_complex);
}
```

### 8.2 随机测试套件

```c
LV00_TEST_SUITE(BootstrapRandom)
{
    /* 随机生成测试 */
    LV00_TEST(BootstrapRandom, random_graph_generation);
    LV00_TEST(BootstrapRandom, random_dsl_generation);
    
    /* 大规模差分测试 */
    LV00_TEST_PARAMETERIZED(BootstrapRandom, large_scale_diff, 
                            random_graph_generator, 100);
    
    /* 压力测试 */
    LV00_BENCHMARK(BootstrapRandom, solve_100_nodes, 1000);
    LV00_BENCHMARK(BootstrapRandom, normalize_1000_constraints, 100);
}
```

---

## 九、集成方式

### 9.1 与现有测试框架集成

```c
/* 在 lv00_test_main 中集成 */
int main(int argc, char **argv)
{
    /* 初始化自举测试框架 */
    bootstrap_test_framework_init();
    
    /* 注册自举测试套件 */
    bootstrap_test_register_suite();
    
    /* 运行所有测试 */
    Lv00TestReport *report = lv00_test_run_all();
    
    /* 输出报告 */
    lv00_test_report_print(report, stdout);
    lv00_test_report_write_file(report, "bootstrap_test_report.json", "json");
    
    /* 清理 */
    lv00_test_report_destroy(report);
    bootstrap_test_framework_cleanup();
    
    return 0;
}
```

### 9.2 CMake 集成

```cmake
# bootstrap_test_framework/CMakeLists.txt

add_library(bootstrap_test_framework STATIC
    src/bootstrap_test.c
    src/differential_test.c
    src/random_generator.c
    src/graph_isomorphism.c
    src/primitive_wrapper.c
    src/test_oracle.c
)

target_link_libraries(bootstrap_test_framework
    lv00_static
    lv00_test_framework
)

target_include_directories(bootstrap_test_framework PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 测试可执行文件
add_executable(bootstrap_test_runner
    tests/test_differential.c
    tests/test_generator.c
    tests/test_isomorphism.c
    tests/test_primitives.c
)

target_link_libraries(bootstrap_test_runner
    bootstrap_test_framework
    lv00_static
)

add_test(NAME BootstrapTest COMMAND bootstrap_test_runner)
```

---

## 十、成功标准

| 标准 | 度量方法 | 目标 |
|------|---------|------|
| 原语差分测试覆盖率 | 13 个原语全部测试 | 100% |
| 差分测试通过率 | C API 与几何层结果一致 | ≥ 99% |
| 归一化幂等性 | 连续归一化结果不变 | 100% |
| 序列化往返正确性 | 序列化→反序列化语义不变 | 100% |
| 图同构检测准确率 | 已知同构/非同构图正确识别 | 100% |
| 随机测试稳定性 | 1000 次随机测试无崩溃 | 100% |

---

## 参考文档

- [自举架构设计](self_bootstrapping_design.md)
- [13 个最小原语](geometric_primitives.md)
- [测试框架](test_framework.h)
- [重写系统](rewrite.h) - VF2 同构算法