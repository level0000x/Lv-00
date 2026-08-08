/**
 * @file axiom_test_common.h
 * @brief 公理包测试骨架头 —— 收敛 56 个 test_axiom_*.c 中的整文件模板复制
 *
 * 各 test_axiom_*.c 文件曾经逐字复制了 test_load_from_file / test_templates /
 * test_unconstructible_problems / test_logical_framework / test_content_hash /
 * test_round_trip / test_dependency_validation / test_negative_lookups /
 * test_external_refs 等 static 函数（仅断言具体值不同）。
 *
 * 本头把这些重复函数提取为共享函数（统一加 axiom_test_ 前缀），
 * 各文件的差异数据（包路径、期望模板数、模板名表、不可构造项表、
 * 外部引用表、逻辑框架字符串等）由调用方作为参数传入。
 *
 * 使用约定：
 *   1. 断言宏：统一使用 test_helpers.h 的 TEST_ASSERT（失败即 return 并递增
 *      g_fail_count；成功递增 g_pass_count），不再提供私有兜底实现。
 *      调用方需在 include 本头之前 #include "test_helpers.h"。
 *   2. 全局计数器：调用方需自行定义 g_fail_count / g_pass_count
 *      （test_helpers.h 声明为 extern，文件内定义；非返回式风格为 static）。
 *   3. 共享函数全部为 static inline，直接包含 "lv.h"（其中已含 axiom_pkg.h、
 *      lv_utils.h），无需额外 include。
 *   4. 头文件位于 test/c 目录（已在 CMake include 路径中），无需注册。
 *
 * 保留原则：共享函数体与各文件原函数逐字一致（printf 格式、断言消息、
 * 计数器增减、释放方式均保持一致）；各文件仅保留数据表与文件特有测试。
 */

#ifndef lv_AXIOM_TEST_COMMON_H
#define lv_AXIOM_TEST_COMMON_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "lv.h"

/* ============================================================
 * 差异数据结构（各文件数据表的统一形态）
 * ============================================================ */

/** 模板期望：名称 + 期望参数个数（test_templates 表驱动形态） */
typedef struct {
    const char *name;
    int params;
} AxiomTestTemplateExpectation;

/** 不可构造项期望（test_unconstructible_problems 形态） */
typedef struct {
    const char *name;
    const char *reduces_to;
    int dep_count;
    bool green_verified;
} AxiomTestUcExpectation;

/** 不可构造项期望（test_unconstructibles 最小依赖形态） */
typedef struct {
    const char *name;
    const char *reduces_to;
    int min_deps;
    bool has_ref;
} AxiomTestUcMinDepsExpectation;

/** 外部引用期望：名称 + URL 前缀（test_external_refs 表驱动形态） */
typedef struct {
    const char *name;
    const char *expected_url_prefix;
} AxiomTestExtRefExpectation;

/** 哈希释放方式：lv_free((void**)&ptr) 或 lv_free_ptr(ptr) */
typedef enum {
    AXIOM_TEST_FREE_LV_FREE = 0,     /**< lv_free((void **) &ptr) */
    AXIOM_TEST_FREE_LV_FREE_PTR = 1  /**< lv_free_ptr(ptr) */
} AxiomTestFreeMode;

/** 负向查找风格 */
typedef enum {
    AXIOM_TEST_NEG_BASIC = 0, /**< "nonexistent_template" + "  Negative lookups: correct" */
    AXIOM_TEST_NEG_XYZ = 1,   /**< "nonexistent_template_xyz" 无收尾 printf */
    AXIOM_TEST_NEG_EMPTY = 2  /**< 额外检查空字符串查找 */
} AxiomTestNegStyle;

/* ============================================================
 * 内部辅助
 * ============================================================ */

/** 按模式释放 content_hash 返回的指针 */
static inline void axiom_test_free_hash(char *ptr, AxiomTestFreeMode mode) {
    if (mode == AXIOM_TEST_FREE_LV_FREE_PTR) {
        lv_free_ptr(ptr);
    } else {
        lv_free((void **) &ptr);
    }
}

/* ============================================================
 * Test 1: 从文件加载（universal，全 57 文件同一形态）
 * ============================================================ */
static inline void axiom_test_load_from_file(const char *pkg_path, const char *pkg_name) {
    printf("Test 1: Load %s.lvz from file...\n", pkg_name);

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    TEST_ASSERT(pkg != NULL, "package creation should succeed");

    AxiomLoadStatus status = axiom_package_load(pkg, pkg_path);
    TEST_ASSERT(status == AXIOM_LOAD_OK, "axiom_package_load should return AXIOM_LOAD_OK");

    if (status != AXIOM_LOAD_OK) {
        const char *err = axiom_package_get_last_error();
        printf("  Error: %s\n", err ? err : "(unknown)");
    }

    {
        char msg[160];
        snprintf(msg, sizeof(msg), "package name should be '%s'", pkg_name);
        TEST_ASSERT(pkg->name != NULL && strcmp(pkg->name, pkg_name) == 0, msg);
    }
    TEST_ASSERT(pkg->version != NULL && strcmp(pkg->version, "1.0.0") == 0, "package version should be '1.0.0'");

    printf("  Package: '%s' v%s\n", pkg->name, pkg->version);

    axiom_package_destroy(pkg);
}

/* ============================================================
 * Test 2: 约束模板校验（表驱动形态：{name, params}）
 * ============================================================ */
static inline void axiom_test_templates_with_params(const char *pkg_path, int expected_count, const char *count_msg,
                                                    const AxiomTestTemplateExpectation *expectations, int n) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, pkg_path);

    TEST_ASSERT(axiom_package_get_template_count(pkg) == expected_count, count_msg);
    printf("  Template count: %d (expected %d)\n", axiom_package_get_template_count(pkg), expected_count);

    int total = n;
    TEST_ASSERT(total == expected_count, "expected array size should match EXPECTED_TEMPLATE_COUNT");

    int found_count = 0;
    for (int i = 0; i < total; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, expectations[i].name);
        if (tmpl) {
            found_count++;
            if (tmpl->param_count != expectations[i].params) {
                printf("  FAIL: '%s' has %d params, expected %d\n", expectations[i].name, tmpl->param_count,
                       expectations[i].params);
                g_fail_count++;
            } else {
                g_pass_count++;
            }
        } else {
            printf("  MISSING template: '%s'\n", expectations[i].name);
            g_fail_count++;
        }
    }
    TEST_ASSERT(found_count == expected_count, "all expected templates should be found");
    printf("  Found %d / %d templates\n", found_count, expected_count);

    axiom_package_destroy(pkg);
}

/* ============================================================
 * Test 2: 约束模板校验（B2 形态：FAIL-not-found + "Local expected count"，
 * 无 found_count 汇总打印；数据仍为 {name, params}）
 * ============================================================ */
static inline void axiom_test_templates_with_params_min(const char *pkg_path, int expected_count, const char *count_msg,
                                                        const AxiomTestTemplateExpectation *expectations, int n) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, pkg_path);

    TEST_ASSERT(axiom_package_get_template_count(pkg) == expected_count, count_msg);
    printf("  Template count: %d (expected %d)\n", axiom_package_get_template_count(pkg), expected_count);

    int expected_count_local = n;
    TEST_ASSERT(expected_count_local == expected_count,
                "local expected array count should match EXPECTED_TEMPLATE_COUNT");
    printf("  Local expected count: %d\n", expected_count_local);

    for (int i = 0; i < expected_count_local; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, expectations[i].name);
        if (!tmpl) {
            printf("  FAIL: template '%s' not found\n", expectations[i].name);
            g_fail_count++;
            continue;
        }
        TEST_ASSERT(tmpl->param_count == expectations[i].params, "template parameter count mismatch");
    }

    axiom_package_destroy(pkg);
}

/* ============================================================
 * Test 2: 约束模板校验（仅名称形态；各文件特有的参数个数
 * 校验留在调用方，紧接本函数之后执行）
 * ============================================================ */
static inline void axiom_test_templates_names_only(const char *pkg_path, int expected_count, const char *count_msg,
                                                   const char *const *names, int n) {
    printf("Test 2: Verify constraint templates...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, pkg_path);

    TEST_ASSERT(axiom_package_get_template_count(pkg) == expected_count, count_msg);
    printf("  Template count: %d (expected %d)\n", axiom_package_get_template_count(pkg), expected_count);

    int found_count = 0;
    for (int i = 0; i < n; i++) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, names[i]);
        if (tmpl) {
            found_count++;
        } else {
            printf("  MISSING template: '%s'\n", names[i]);
            g_fail_count++;
        }
    }
    TEST_ASSERT(found_count == expected_count, "all expected templates should be found");
    printf("  Found %d / %d templates\n", found_count, expected_count);

    axiom_package_destroy(pkg);
}

/* ============================================================
 * Test 3: 不可构造项校验（A 形态：TEST_ASSERT(uc != NULL) + if(uc)）
 * ============================================================ */
static inline void axiom_test_unconstructible_problems(const char *pkg_path, int expected_count, const char *count_msg,
                                                       const AxiomTestUcExpectation *expectations, int n) {
    printf("Test 3: Verify known unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, pkg_path);

    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg) == expected_count, count_msg);
    printf("  Unconstructible count: %d (expected %d)\n", axiom_package_get_unconstructible_count(pkg),
           expected_count);

    for (int i = 0; i < n; i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expectations[i].name);
        TEST_ASSERT(uc != NULL, expectations[i].name);

        if (uc) {
            TEST_ASSERT(uc->reduces_to != NULL && strcmp(uc->reduces_to, expectations[i].reduces_to) == 0,
                        expectations[i].name);
            TEST_ASSERT(uc->dependency_chain.count == expectations[i].dep_count, expectations[i].name);
            TEST_ASSERT(uc->green_verified == expectations[i].green_verified, expectations[i].name);
            TEST_ASSERT(uc->external_ref != NULL && strlen(uc->external_ref) > 0, "should have external_ref URL");
            printf("  [%d] %s -> %s (deps=%d, verified=%s)\n", i, uc->name, uc->reduces_to,
                   uc->dependency_chain.count, uc->green_verified ? "true" : "false");
        }
    }

    axiom_package_destroy(pkg);
}

/* ============================================================
 * Test 3: 不可构造项校验（C 形态：min_deps + if(!uc) FAIL/continue）
 * ============================================================ */
static inline void axiom_test_unconstructibles_min_deps(const char *pkg_path, int expected_count, const char *count_msg,
                                                        const AxiomTestUcMinDepsExpectation *expectations, int n) {
    printf("Test 3: Verify unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, pkg_path);

    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg) == expected_count, count_msg);
    printf("  Unconstructible count: %d (expected %d)\n", axiom_package_get_unconstructible_count(pkg),
           expected_count);

    int uc_count = n;
    TEST_ASSERT(uc_count == expected_count, "local expected UC count should match");

    for (int i = 0; i < uc_count; i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expectations[i].name);
        if (!uc) {
            printf("  FAIL: unconstructible '%s' not found\n", expectations[i].name);
            g_fail_count++;
            continue;
        }
        TEST_ASSERT(uc->reduces_to && strcmp(uc->reduces_to, expectations[i].reduces_to) == 0,
                    "unconstructible reduces_to mismatch");
        TEST_ASSERT(uc->dependency_chain.count >= expectations[i].min_deps,
                    "unconstructible should have minimum dependency count");
        TEST_ASSERT(expectations[i].has_ref ? (uc->external_ref != NULL) : 1,
                    "unconstructible should have external_ref");
        TEST_ASSERT(uc->green_verified == true, "unconstructible should be green_verified");
    }

    axiom_package_destroy(pkg);
}

/* ============================================================
 * Test 3: 不可构造项校验（B 形态：if(uc)/else MISSING + HTTPS 检查）
 * ============================================================ */
static inline void axiom_test_unconstructibles_with_https(const char *pkg_path, int expected_count,
                                                          const char *count_msg,
                                                          const AxiomTestUcExpectation *expectations, int n) {
    printf("Test 3: Verify known unconstructible problems...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, pkg_path);

    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg) == expected_count, count_msg);
    printf("  Unconstructible count: %d (expected %d)\n", axiom_package_get_unconstructible_count(pkg),
           expected_count);

    for (int i = 0; i < n; i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, expectations[i].name);
        if (uc) {
            TEST_ASSERT(uc->reduces_to != NULL && strcmp(uc->reduces_to, expectations[i].reduces_to) == 0,
                        "reduces_to should match");
            TEST_ASSERT(uc->dependency_chain.count == expectations[i].dep_count, "dependency_count should match");
            TEST_ASSERT(uc->green_verified == expectations[i].green_verified, "green_verified should be true");
            TEST_ASSERT(uc->external_ref != NULL && strlen(uc->external_ref) > 0, "should have external_ref URL");
            TEST_ASSERT(strncmp(uc->external_ref, "https://", 8) == 0, "external_ref should be an HTTPS URL");
            printf("  [%d] %s -> %s (deps=%d, verified=%s)\n", i, uc->name, uc->reduces_to,
                   uc->dependency_chain.count, uc->green_verified ? "true" : "false");
        } else {
            printf("  MISSING unconstructible: '%s'\n", expectations[i].name);
            g_fail_count++;
        }
    }

    axiom_package_destroy(pkg);
}

/* ============================================================
 * Test 4: 逻辑框架（S 形态：单条 strcmp 断言 + 无引号输出）
 * ============================================================ */
static inline void axiom_test_logical_framework(const char *pkg_path, const char *bottom_geometry,
                                                const char *negation_encoding, int contradiction_behavior,
                                                const char *contradiction_name) {
    printf("Test 4: Verify bottom geometry and logical framework...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, pkg_path);

    {
        char msg[192];
        snprintf(msg, sizeof(msg), "bottom_geometry should be '%s'", bottom_geometry);
        TEST_ASSERT(pkg->bottom_geometry != NULL && strcmp(pkg->bottom_geometry, bottom_geometry) == 0, msg);
    }
    printf("  bottom_geometry: %s\n", pkg->bottom_geometry);

    {
        char msg[192];
        snprintf(msg, sizeof(msg), "negation_encoding should be '%s'", negation_encoding);
        TEST_ASSERT(pkg->negation_encoding != NULL && strcmp(pkg->negation_encoding, negation_encoding) == 0, msg);
    }
    printf("  negation_encoding: %s\n", pkg->negation_encoding);

    {
        char msg[192];
        snprintf(msg, sizeof(msg), "contradiction_behavior should be %s", contradiction_name);
        TEST_ASSERT(pkg->contradiction_behavior == contradiction_behavior, msg);
    }
    printf("  contradiction_behavior: %s\n", contradiction_name);

    axiom_package_destroy(pkg);
}

/* ============================================================
 * Test 4: 逻辑框架（M 形态：presence + strcmp 双断言 + 引号输出；
 * header_text 为 "Test 4: ..." 首行打印文本，各文件可能不同）
 * ============================================================ */
static inline void axiom_test_logical_framework_checked(const char *pkg_path, const char *header_text,
                                                        const char *bottom_geometry, const char *negation_encoding,
                                                        int contradiction_behavior, const char *contradiction_name) {
    printf("%s\n", header_text);

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, pkg_path);

    TEST_ASSERT(pkg->bottom_geometry != NULL, "bottom_geometry should be set");
    {
        char msg[192];
        snprintf(msg, sizeof(msg), "bottom_geometry should be '%s'", bottom_geometry);
        TEST_ASSERT(strcmp(pkg->bottom_geometry, bottom_geometry) == 0, msg);
    }
    printf("  bottom_geometry: '%s'\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL, "negation_encoding should be set");
    {
        char msg[192];
        snprintf(msg, sizeof(msg), "negation_encoding should be '%s'", negation_encoding);
        TEST_ASSERT(strcmp(pkg->negation_encoding, negation_encoding) == 0, msg);
    }
    printf("  negation_encoding: '%s'\n", pkg->negation_encoding);

    {
        char msg[192];
        snprintf(msg, sizeof(msg), "contradiction_behavior should be %s", contradiction_name);
        TEST_ASSERT(pkg->contradiction_behavior == contradiction_behavior, msg);
    }
    printf("  contradiction_behavior: %s\n", contradiction_name);

    axiom_package_destroy(pkg);
}

/* ============================================================
 * Test 4: 逻辑框架（P 形态：仅非空断言 + 引号输出）
 * ============================================================ */
static inline void axiom_test_logical_framework_presence(const char *pkg_path, int contradiction_behavior,
                                                         const char *contradiction_name) {
    printf("Test 4: Verify logical framework settings...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, pkg_path);

    TEST_ASSERT(pkg->bottom_geometry != NULL, "bottom_geometry should be set");
    printf("  bottom_geometry: '%s'\n", pkg->bottom_geometry);

    TEST_ASSERT(pkg->negation_encoding != NULL, "negation_encoding should be set");
    printf("  negation_encoding: '%s'\n", pkg->negation_encoding);

    {
        char msg[192];
        snprintf(msg, sizeof(msg), "contradiction_behavior should be %s", contradiction_name);
        TEST_ASSERT(pkg->contradiction_behavior == contradiction_behavior, msg);
    }
    printf("  contradiction_behavior: %s\n", contradiction_name);

    axiom_package_destroy(pkg);
}

/* ============================================================
 * Test 5: 内容哈希（H1 形态：单次计算 + "SHA-256: %s"）
 * ============================================================ */
static inline void axiom_test_content_hash(const char *pkg_path, AxiomTestFreeMode mode) {
    printf("Test 5: Content hash computation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, pkg_path);

    char *hash = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash != NULL, "content hash should not be NULL");
    TEST_ASSERT(strlen(hash) == 64, "SHA-256 hash should be 64 hex chars");

    if (hash) {
        printf("  SHA-256: %s\n", hash);
        axiom_test_free_hash(hash, mode);
    }

    axiom_package_destroy(pkg);
}

/* ============================================================
 * Test 5: 内容哈希（H2 形态：两次计算验证确定性 + "Hash: %.8s"）
 * ============================================================ */
static inline void axiom_test_content_hash_deterministic(const char *pkg_path, AxiomTestFreeMode mode) {
    printf("Test 5: Content hash computation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, pkg_path);

    char *hash1 = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash1 != NULL, "content hash should not be NULL");
    TEST_ASSERT(strlen(hash1) == 64, "SHA-256 hash should be 64 hex chars");
    printf("  Hash: %.8s...%.8s (len=%zu)\n", hash1, hash1 + 56, strlen(hash1));

    /* Hash should be deterministic */
    char *hash2 = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash2 != NULL, "second hash should not be NULL");
    TEST_ASSERT(strcmp(hash1, hash2) == 0, "content hash should be deterministic");

    axiom_test_free_hash(hash1, mode);
    axiom_test_free_hash(hash2, mode);
    axiom_package_destroy(pkg);
}

/* ============================================================
 * Test 6: 往返保存/加载（R1 形态：pkg1/pkg2 + hash 对比 + 计数 printf）
 * ============================================================ */
static inline void axiom_test_round_trip(const char *pkg_path, const char *save_path, AxiomTestFreeMode mode) {
    printf("Test 6: Round-trip save/load...\n");

    AxiomPackage *pkg1 = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg1, pkg_path);

    AxiomSaveStatus save_status = axiom_package_save(pkg1, save_path);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK, "save should succeed");

    AxiomPackage *pkg2 = axiom_package_create("placeholder", "0.0.0");
    AxiomLoadStatus load_status = axiom_package_load(pkg2, save_path);
    TEST_ASSERT(load_status == AXIOM_LOAD_OK, "re-load from saved file should succeed");

    TEST_ASSERT(axiom_package_get_template_count(pkg2) == axiom_package_get_template_count(pkg1),
                "template count should match after round-trip");
    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg2) == axiom_package_get_unconstructible_count(pkg1),
                "unconstructible count should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->name, pkg1->name) == 0, "name should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->version, pkg1->version) == 0, "version should match after round-trip");
    TEST_ASSERT(strcmp(pkg2->bottom_geometry, pkg1->bottom_geometry) == 0,
                "bottom_geometry should match after round-trip");
    TEST_ASSERT(pkg2->contradiction_behavior == pkg1->contradiction_behavior,
                "contradiction_behavior should match after round-trip");

    printf("  Round-trip: templates=%d, unconstructibles=%d\n", axiom_package_get_template_count(pkg2),
           axiom_package_get_unconstructible_count(pkg2));

    char *hash1 = axiom_package_compute_content_hash(pkg1);
    char *hash2 = axiom_package_compute_content_hash(pkg2);
    TEST_ASSERT(hash1 && hash2 && strcmp(hash1, hash2) == 0, "content hashes should match after round-trip");
    printf("  Hash match: %s\n", (hash1 && hash2 && strcmp(hash1, hash2) == 0) ? "YES" : "NO");

    axiom_test_free_hash(hash1, mode);
    axiom_test_free_hash(hash2, mode);
    axiom_package_destroy(pkg1);
    axiom_package_destroy(pkg2);
}

/* ============================================================
 * Test 6: 往返保存/加载（R2 形态：hash_orig/hash_reload + remove 清理）
 * ============================================================ */
static inline void axiom_test_round_trip_save_load(const char *pkg_path, const char *save_path, const char *pkg_name,
                                                   int expected_templates, int expected_uc, AxiomTestFreeMode mode) {
    printf("Test 6: Round-trip save/load...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, pkg_path);

    /* Save to test file */
    AxiomSaveStatus save_status = axiom_package_save(pkg, save_path);
    TEST_ASSERT(save_status == AXIOM_SAVE_OK, "axiom_package_save should return AXIOM_SAVE_OK");

    /* Compute hash before destroying */
    char *hash_orig = axiom_package_compute_content_hash(pkg);
    TEST_ASSERT(hash_orig != NULL, "original hash should be computable");

    axiom_package_destroy(pkg);

    /* Load from saved file */
    AxiomPackage *pkg2 = axiom_package_create("placeholder", "0.0.0");
    AxiomLoadStatus load_status = axiom_package_load(pkg2, save_path);
    TEST_ASSERT(load_status == AXIOM_LOAD_OK, "reloading saved file should succeed");

    TEST_ASSERT(strcmp(pkg2->name, pkg_name) == 0, "reloaded package should have same name");
    TEST_ASSERT(strcmp(pkg2->version, "1.0.0") == 0, "reloaded package should have same version");
    TEST_ASSERT(axiom_package_get_template_count(pkg2) == expected_templates,
                "reloaded package should have same template count");
    TEST_ASSERT(axiom_package_get_unconstructible_count(pkg2) == expected_uc,
                "reloaded package should have same unconstructible count");

    char *hash_reload = axiom_package_compute_content_hash(pkg2);
    TEST_ASSERT(hash_reload != NULL, "reloaded hash should be computable");
    TEST_ASSERT(strcmp(hash_orig, hash_reload) == 0, "content hash should survive round-trip");

    axiom_test_free_hash(hash_orig, mode);
    axiom_test_free_hash(hash_reload, mode);
    axiom_package_destroy(pkg2);

    /* Clean up test file */
    remove(save_path);
}

/* ============================================================
 * Test 7: 依赖验证（V1 形态：仅自我验证 + "Self-validation: %s"）
 * ============================================================ */
static inline void axiom_test_dependency_validation(const char *pkg_path, const char *fail_msg, const char *suffix) {
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, pkg_path);

    bool valid = axiom_package_validate_dependencies(pkg, &pkg, 1);
    printf("  Self-validation: %s%s\n", valid ? "PASS" : fail_msg, suffix);

    axiom_package_destroy(pkg);
}

/* ============================================================
 * Test 7: 依赖验证（V2 形态：loaded_packages 数组 + Validation note）
 * extra_line 为 NULL 时不打印附加行
 * ============================================================ */
static inline void axiom_test_dependency_validation_note(const char *pkg_path, const char *extra_line) {
    printf("Test 7: Dependency validation...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, pkg_path);

    AxiomPackage *loaded_packages[1] = {pkg};

    bool valid = axiom_package_validate_dependencies(pkg, loaded_packages, 1);
    if (!valid) {
        const char *err = axiom_package_get_last_error();
        printf("  Validation note: %s\n", err ? err : "(unknown)");
        if (extra_line) {
            printf("  %s\n", extra_line);
        }
    }
    TEST_ASSERT(1, "dependency validation executed");

    axiom_package_destroy(pkg);
}

/* ============================================================
 * Test 8: 负向查找（N1 / N2 / N3 三种历史形态）
 * ============================================================ */
static inline void axiom_test_negative_lookups(const char *pkg_path, AxiomTestNegStyle style) {
    printf("Test 8: Negative lookups...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, pkg_path);

    if (style == AXIOM_TEST_NEG_XYZ) {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, "nonexistent_template_xyz");
        TEST_ASSERT(tmpl == NULL, "lookup of non-existent template should return NULL");

        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "nonexistent_problem_xyz");
        TEST_ASSERT(uc == NULL, "lookup of non-existent unconstructible should return NULL");
    } else if (style == AXIOM_TEST_NEG_EMPTY) {
        /* Template that doesn't exist */
        ConstraintTemplate *t = axiom_package_get_template(pkg, "nonexistent_template");
        TEST_ASSERT(t == NULL, "nonexistent template should return NULL");

        /* Unconstructible problem that doesn't exist */
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "nonexistent_problem");
        TEST_ASSERT(uc == NULL, "nonexistent unconstructible should return NULL");

        /* Empty string lookups */
        t = axiom_package_get_template(pkg, "");
        TEST_ASSERT(t == NULL, "empty template name should return NULL");

        uc = axiom_package_lookup_unconstructible(pkg, "");
        TEST_ASSERT(uc == NULL, "empty unconstructible name should return NULL");

        printf("  All negative lookups returned NULL as expected\n");
    } else {
        ConstraintTemplate *tmpl = axiom_package_get_template(pkg, "nonexistent_template");
        TEST_ASSERT(tmpl == NULL, "non-existent template should return NULL");

        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "nonexistent_problem");
        TEST_ASSERT(uc == NULL, "non-existent unconstructible should return NULL");

        printf("  Negative lookups: correct\n");
    }

    axiom_package_destroy(pkg);
}

/* ============================================================
 * Test 9: 外部引用校验（E1 形态：URL 前缀表驱动）
 * ============================================================ */
static inline void axiom_test_external_refs(const char *pkg_path, const AxiomTestExtRefExpectation *ref_checks, int n) {
    printf("Test 9: Verify external reference URLs...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, pkg_path);

    for (int i = 0; i < n; i++) {
        KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, ref_checks[i].name);
        TEST_ASSERT(uc != NULL, ref_checks[i].name);
        if (uc) {
            TEST_ASSERT(uc->external_ref != NULL &&
                            strncmp(uc->external_ref, ref_checks[i].expected_url_prefix,
                                    strlen(ref_checks[i].expected_url_prefix)) == 0,
                        ref_checks[i].name);
            printf("  [%d] %s -> %s\n", i, uc->name, uc->external_ref);
        }
    }

    axiom_package_destroy(pkg);
}

/* ============================================================
 * Test 9: 外部引用校验（E2 形态：遍历全部 + URL 格式检查）
 * ============================================================ */
static inline void axiom_test_external_refs_all(const char *pkg_path) {
    printf("Test 9: External reference URLs...\n");

    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    axiom_package_load(pkg, pkg_path);

    for (int i = 0; i < axiom_package_get_unconstructible_count(pkg); i++) {
        KnownUnconstructible *uc = axiom_package_get_unconstructible(pkg, i);
        TEST_ASSERT(uc->external_ref != NULL, "each unconstructible should have an external_ref");

        /* Verify it's a valid URL */
        int is_url = (strncmp(uc->external_ref, "http://", 7) == 0 || strncmp(uc->external_ref, "https://", 8) == 0);
        TEST_ASSERT(is_url, "external_ref should be a valid URL");

        printf("  '%s' -> %s\n", uc->name, uc->external_ref);
    }

    axiom_package_destroy(pkg);
}

#endif /* lv_AXIOM_TEST_COMMON_H */
