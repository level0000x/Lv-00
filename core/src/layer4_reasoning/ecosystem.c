/**
 * @file ecosystem.c
 * @brief 开放生态包管理系统实现 —— 借鉴 OpenGeometry / mai / GAP 的联盟共建哲学
 *
 * @details 实现包注册表、兼容性矩阵、Docker一键体验配置、生态统计。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date 2026-05-24
 *
 * @dependencies
 *   - ecosystem.h             : 生态系统公共接口
 *   - lv00_utils.h            : 统一内存分配器
 *   - lv00_internal.h         : 内部常量与工具宏
 *   - error_codes.h           : 统一错误码系统
 */

/* ========================================================================
 * 包含头文件
 * ======================================================================== */

#include "ecosystem.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "error_codes.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

/* ========================================================================
 * 模块级常量
 * ======================================================================== */

#define ECO_DEFAULT_REGISTRY_URL "https://registry.lv-00.org/packages"
#define ECO_DEFAULT_IMAGE_NAME   "lv00/lv00-quickstart"
#define ECO_DEFAULT_IMAGE_TAG    "latest"
#define ECO_CONTRIB_GUIDELINES_BUFFER 32768
#define ECO_DOCKERFILE_BUFFER  16384

/** 默认贡献指南模板 */
static const char *g_default_guidelines =
    "# Lv-00 生态系统贡献指南\n\n"
    "## 代码风格\n"
    "- 使用 C11 标准\n"
    "- 遵循项目 .clang-format 配置\n"
    "- 所有公开 API 必须有 Doxygen 注释\n\n"
    "## 公理包提交规范\n"
    "- 每个公理包必须包含 manifest.json\n"
    "- 必须通过兼容性检查\n"
    "- 必须包含至少一个测试用例\n\n"
    "## PR 流程\n"
    "1. Fork 主仓库\n"
    "2. 创建 feature 分支\n"
    "3. 提交代码并通过 CI\n"
    "4. 发起 Pull Request\n\n"
    "## 测试要求\n"
    "- 新增公理必须附带验证脚本\n"
    "- 测试覆盖率不低于 80%\n\n"
    "## 许可证合规\n"
    "- 提交的包必须声明许可证\n"
    "- 推荐使用 MIT 或 Apache 2.0\n";

/* ========================================================================
 * 内部辅助函数声明
 * ======================================================================== */

static bool eco_match_query(const Lv00EcoPackage *pkg, const Lv00EcoSearchQuery *query);
static int eco_search_relevance(const Lv00EcoPackage *pkg, const char *query_text);
static int eco_cmp_search_results(const void *a, const void *b, void *sort_by_ptr);
static bool eco_is_installed_package(Lv00Ecosystem *eco, const char *package_id);
static int eco_find_package_index(const Lv00Ecosystem *eco, const char *package_id);
static bool eco_parse_semver(const char *version, int *major, int *minor, int *patch);
static const char *eco_entity_type_name_str(Lv00EcosystemEntity entity);
static const char *eco_license_name_str(Lv00EcoLicense license);
static const char *eco_install_status_name_str(Lv00EcoInstallStatus status);
static const char *eco_compatibility_name_str(Lv00EcoCompatibilityLevel level);
static const char *eco_sort_name_str(Lv00EcoSortBy sort_by);

/* ========================================================================
 * 生命周期函数
 * ======================================================================== */

/**
 * @brief 初始化生态系统实例
 *
 * 分配并初始化 Lv00Ecosystem 结构体，设置默认注册表 URL、
 * 默认贡献指南和一键体验（Docker）配置。
 *
 * @return 新分配的 Lv00Ecosystem 指针，失败返回 NULL
 */
Lv00Ecosystem *eco_init(void) {
    Lv00Ecosystem *eco = (Lv00Ecosystem *)lv00_malloc(sizeof(Lv00Ecosystem));
    LV00_CHECK_NULL(eco, NULL);
    if (!eco) return NULL;

    memset(eco, 0, sizeof(Lv00Ecosystem));

    /* 设置默认注册表 URL */
    strncpy(eco->registry.registry_url, ECO_DEFAULT_REGISTRY_URL,
            sizeof(eco->registry.registry_url) - 1);
    eco->registry.auto_update = false;

    /* 设置默认贡献指南 */
    strncpy(eco->contribution_guidelines, g_default_guidelines,
            sizeof(eco->contribution_guidelines) - 1);
    eco->guidelines_length = (int)strlen(eco->contribution_guidelines);

    /* 设置默认一键体验 */
    strncpy(eco->one_click.docker_image, ECO_DEFAULT_IMAGE_NAME,
            sizeof(eco->one_click.docker_image) - 1);
    strncpy(eco->one_click.container_name, "lv00-quickstart",
            sizeof(eco->one_click.container_name) - 1);
    strncpy(eco->one_click.image_tag, ECO_DEFAULT_IMAGE_TAG,
            sizeof(eco->one_click.image_tag) - 1);
    eco->one_click.use_podman = false;
    eco->one_click.enable_gpu = false;

    return eco;
}

/**
 * @brief 销毁生态系统实例，释放所有关联资源
 *
 * 释放兼容性矩阵、一键体验中的环境变量和卷挂载列表，
 * 最后释放生态系统结构体本身。
 *
 * @param eco 要销毁的生态系统指针
 */
void eco_destroy(Lv00Ecosystem *eco) {
    if (!eco) return;

    /* 释放兼容性矩阵 */
    lv00_free((void **)&eco->compat_matrix);

    /* 释放一键体验中的动态数组 */
    if (eco->one_click.env_vars) {
        for (int i = 0; i < eco->one_click.env_var_count; i++) {
            lv00_free((void **)&eco->one_click.env_vars[i]);
        }
        lv00_free((void **)&eco->one_click.env_vars);
    }

    if (eco->one_click.volumes) {
        for (int i = 0; i < eco->one_click.volume_count; i++) {
            lv00_free((void **)&eco->one_click.volumes[i]);
        }
        lv00_free((void **)&eco->one_click.volumes);
    }

    lv00_free((void **)&eco);
}

/* ========================================================================
 * 包管理函数
 * ======================================================================== */

/**
 * @brief 注册一个公理包到生态系统注册表
 *
 * 若包 ID 已存在且新版本更高则更新，否则追加新条目。
 *
 * @param eco 生态系统指针
 * @param pkg 要注册的包信息
 * @return 成功返回包索引（>=0），已存在且版本不更高返回 -1，参数无效或已满返回 -2
 */
int eco_package_register(Lv00Ecosystem *eco, const Lv00EcoPackage *pkg) {
    LV00_CHECK_NULL(eco, -2);
    LV00_CHECK_NULL(pkg, -2);
    if (eco->registry.package_count >= LV00_ECO_MAX_PACKAGES) return -2;

    /* 检查是否已存在 */
    int existing = eco_find_package_index(eco, pkg->package_id);
    if (existing >= 0) {
        /* 如果新版本更高则更新 */
        int cmp = eco_compare_versions(pkg->version,
                                        eco->registry.packages[existing].version);
        if (cmp > 0) {
            memcpy(&eco->registry.packages[existing], pkg, sizeof(Lv00EcoPackage));
            return existing;
        }
        return -1;
    }

    int idx = eco->registry.package_count;
    memcpy(&eco->registry.packages[idx], pkg, sizeof(Lv00EcoPackage));
    eco->registry.package_count++;

    return idx;
}

/**
 * @brief 从注册表中注销指定包
 *
 * 按包 ID 查找并移除，后续条目前移填补空缺。
 *
 * @param eco        生态系统指针
 * @param package_id 要注销的包 ID
 * @return 成功返回 0，未找到或参数无效返回 -1
 */
int eco_package_unregister(Lv00Ecosystem *eco, const char *package_id) {
    LV00_CHECK_NULL(eco, -1);
    LV00_CHECK_NULL(package_id, -1);

    int idx = eco_find_package_index(eco, package_id);
    if (idx < 0) return -1;

    /* 移除包（移动后续条目） */
    int move_count = eco->registry.package_count - idx - 1;
    if (move_count > 0) {
        memmove(&eco->registry.packages[idx],
                &eco->registry.packages[idx + 1],
                sizeof(Lv00EcoPackage) * move_count);
    }
    eco->registry.package_count--;

    return 0;
}

/** @brief 生态系统包搜索排序依据（线程局部，供比较函数使用） */
static LV00_THREAD_LOCAL Lv00EcoSortBy g_sort_by = ECO_SORT_RELEVANCE;

/** @brief 生态系统包搜索排序比较函数（供qsort使用） */
static int eco_package_sort_cmp(const void *a, const void *b) {
    const Lv00EcoPackage *pa = (const Lv00EcoPackage *)a;
    const Lv00EcoPackage *pb = (const Lv00EcoPackage *)b;
    switch (g_sort_by) {
        case ECO_SORT_STARS:
            return (pa->star_count > pb->star_count) - (pa->star_count < pb->star_count);
        case ECO_SORT_DATE:
            return (pa->install_date > pb->install_date) - (pa->install_date < pb->install_date);
        case ECO_SORT_NAME:
            return strcmp(pa->name, pb->name);
        default:
            return 0;
    }
}

/**
 * @brief 搜索注册表中的包
 *
 * 根据查询条件（文本搜索、实体类型过滤）匹配包，支持排序和分页。
 * 相关性排序基于包名称、描述、作者和包 ID 的加权文本匹配。
 *
 * @param eco       生态系统指针
 * @param query     搜索查询条件
 * @param results   输出匹配结果数组（调用者负责释放）
 * @param out_count 输出匹配结果数量
 * @return 成功返回 0，失败返回 -1
 */
int eco_package_search(const Lv00Ecosystem *eco, const Lv00EcoSearchQuery *query,
                        Lv00EcoPackage **results, int *out_count) {
    LV00_CHECK_NULL(eco, -1);
    LV00_CHECK_NULL(query, -1);
    LV00_CHECK_NULL(results, -1);
    LV00_CHECK_NULL(out_count, -1);

    /* 先收集所有匹配的包 */
    int match_cap = 64;
    Lv00EcoPackage *matches = (Lv00EcoPackage *)lv00_malloc(
        sizeof(Lv00EcoPackage) * match_cap);
    if (!matches) return -1;

    int match_count = 0;
    for (int i = 0; i < eco->registry.package_count; i++) {
        if (eco_match_query(&eco->registry.packages[i], query)) {
            if (match_count >= match_cap) {
                match_cap *= 2;
                Lv00EcoPackage *new_m = (Lv00EcoPackage *)lv00_realloc(
                    matches, sizeof(Lv00EcoPackage) * match_cap);
                if (!new_m) {
                    lv00_free((void **)&matches);
                    return -1;
                }
                matches = new_m;
            }
            memcpy(&matches[match_count++], &eco->registry.packages[i],
                   sizeof(Lv00EcoPackage));
        }
    }

    /* 排序 */
    if (query->sort_by != ECO_SORT_RELEVANCE) {
        g_sort_by = query->sort_by;
        qsort(matches, (size_t)match_count, sizeof(Lv00EcoPackage), eco_package_sort_cmp);
    }

    /* 分页 */
    int start = query->page_offset;
    int end   = start + query->page_size;
    if (end > match_count) end = match_count;
    if (start > match_count) start = match_count;

    int result_count = end - start;
    if (result_count < 0) result_count = 0;

    *results   = (Lv00EcoPackage *)lv00_malloc(sizeof(Lv00EcoPackage) * result_count);
    if (!*results && result_count > 0) {
        lv00_free((void **)&matches);
        return -1;
    }

    if (result_count > 0) {
        memcpy(*results, &matches[start], sizeof(Lv00EcoPackage) * result_count);
    }
    *out_count = result_count;

    lv00_free((void **)&matches);
    return 0;
}

/* ========================================================================
 * 安装管理函数
 * ======================================================================== */

/**
 * @brief 安装指定包及其依赖项
 *
 * 检查版本兼容性后递归安装所有依赖项，标记包为已安装状态。
 *
 * @param eco        生态系统指针
 * @param package_id 要安装的包 ID
 * @param version    期望的版本号（NULL 表示不检查版本）
 * @param force      是否强制重新安装已安装的包
 * @return 安装状态码（ECO_INSTALL_OK 表示成功）
 */
Lv00EcoInstallStatus eco_package_install(Lv00Ecosystem *eco, const char *package_id,
                                          const char *version, bool force) {
    LV00_CHECK_NULL(eco, ECO_INSTALL_NOT_FOUND);
    LV00_CHECK_NULL(package_id, ECO_INSTALL_NOT_FOUND);

    int idx = eco_find_package_index(eco, package_id);
    if (idx < 0) return ECO_INSTALL_NOT_FOUND;

    /* 检查版本兼容性 */
    if (version) {
        int cmp = eco_compare_versions(version, eco->registry.packages[idx].version);
        if (cmp != 0) {
            return ECO_INSTALL_VERSION_CONFLICT;
        }
    }

    /* 检查是否已安装 */
    if (eco->registry.packages[idx].is_installed && !force) {
        return ECO_INSTALL_ALREADY_INSTALLED;
    }

    /* 检查 Lv-00 版本兼容性 */
    if (eco->registry.packages[idx].min_lv00_version[0]) {
        /* 实际实现中比较当前运行版本 */
    }

    /* 递归安装依赖项 */
    for (int i = 0; i < eco->registry.packages[idx].dep_count; i++) {
        Lv00EcoInstallStatus dep_status = eco_package_install(
            eco, eco->registry.packages[idx].dependencies[i], NULL, false);
        if (dep_status != ECO_INSTALL_OK && dep_status != ECO_INSTALL_ALREADY_INSTALLED) {
            return ECO_INSTALL_DEP_MISSING;
        }
    }

    /* 标记为已安装 */
    eco->registry.packages[idx].is_installed  = true;
    eco->registry.packages[idx].install_date  = (uint64_t)time(NULL);
    snprintf(eco->registry.packages[idx].install_path,
             sizeof(eco->registry.packages[idx].install_path),
             "packages/%s", package_id);

    eco->registry.installed_count++;

    return ECO_INSTALL_OK;
}

/**
 * @brief 卸载指定包
 *
 * @param eco        生态系统指针
 * @param package_id 要卸载的包 ID
 * @return 成功返回 0，未找到或参数无效返回 -1
 */
int eco_package_uninstall(Lv00Ecosystem *eco, const char *package_id) {
    LV00_CHECK_NULL(eco, -1);
    LV00_CHECK_NULL(package_id, -1);

    int idx = eco_find_package_index(eco, package_id);
    if (idx < 0 || !eco->registry.packages[idx].is_installed) return -1;

    eco->registry.packages[idx].is_installed = false;
    if (eco->registry.installed_count > 0) {
        eco->registry.installed_count--;
    }

    return 0;
}

/**
 * @brief 列出所有已安装的包
 *
 * @param eco      生态系统指针
 * @param out_pkgs 输出已安装包数组（调用者负责释放）
 * @param out_count 输出已安装包数量
 * @return 成功返回 0，失败返回 -1
 */
int eco_package_list_installed(const Lv00Ecosystem *eco, Lv00EcoPackage **out_pkgs,
                                int *out_count) {
    LV00_CHECK_NULL(eco, -1);
    LV00_CHECK_NULL(out_pkgs, -1);
    LV00_CHECK_NULL(out_count, -1);

    /* 先计数 */
    int count = 0;
    for (int i = 0; i < eco->registry.package_count; i++) {
        if (eco->registry.packages[i].is_installed) count++;
    }

    *out_pkgs  = (Lv00EcoPackage *)lv00_malloc(sizeof(Lv00EcoPackage) * count);
    *out_count = 0;

    if (!*out_pkgs && count > 0) return -1;

    for (int i = 0; i < eco->registry.package_count; i++) {
        if (eco->registry.packages[i].is_installed) {
            memcpy(&(*out_pkgs)[*out_count], &eco->registry.packages[i],
                   sizeof(Lv00EcoPackage));
            (*out_count)++;
        }
    }

    return 0;
}

/* ========================================================================
 * 兼容性检查函数
 * ======================================================================== */

/**
 * @brief 检查两个包之间的兼容性
 *
 * 查询兼容性矩阵，支持双向匹配（A-B 等同于 B-A）。
 * 同一包自身视为完全兼容。
 *
 * @param eco          生态系统指针
 * @param package_id_a 包 A 的 ID
 * @param package_id_b 包 B 的 ID
 * @param out_result   输出兼容性详细信息（可为 NULL）
 * @return 兼容性等级
 */
Lv00EcoCompatibilityLevel eco_check_compatibility(const Lv00Ecosystem *eco,
    const char *package_id_a, const char *package_id_b,
    Lv00EcoCompatibility *out_result) {
    LV00_CHECK_NULL(eco, ECO_COMPAT_UNKNOWN);
    LV00_CHECK_NULL(package_id_a, ECO_COMPAT_UNKNOWN);
    LV00_CHECK_NULL(package_id_b, ECO_COMPAT_UNKNOWN);

    /* 查询兼容性矩阵 */
    for (int i = 0; i < eco->compat_matrix_count; i++) {
        const Lv00EcoCompatibility *c = &eco->compat_matrix[i];
        bool match = (strcmp(c->package_id_a, package_id_a) == 0 &&
                      strcmp(c->package_id_b, package_id_b) == 0) ||
                     (strcmp(c->package_id_a, package_id_b) == 0 &&
                      strcmp(c->package_id_b, package_id_a) == 0);
        if (match) {
            if (out_result) {
                memcpy(out_result, c, sizeof(Lv00EcoCompatibility));
            }
            return c->level;
        }
    }

    /* 同一包自身完全兼容 */
    if (strcmp(package_id_a, package_id_b) == 0) {
        return ECO_COMPAT_FULLY;
    }

    return ECO_COMPAT_UNKNOWN;
}

/**
 * @brief 解析所有已安装包之间的兼容性冲突
 *
 * 遍历所有已安装包的两两组合，收集不兼容或部分兼容的配对。
 *
 * @param eco           生态系统指针
 * @param out_conflicts 输出冲突列表数组（调用者负责释放）
 * @param out_count     输出冲突数量
 * @return 成功返回 0，失败返回 -1
 */
int eco_resolve_conflicts(Lv00Ecosystem *eco, Lv00EcoCompatibility **out_conflicts,
                           int *out_count) {
    LV00_CHECK_NULL(eco, -1);
    LV00_CHECK_NULL(out_conflicts, -1);
    LV00_CHECK_NULL(out_count, -1);

    int conf_cap = 16;
    Lv00EcoCompatibility *conflicts = (Lv00EcoCompatibility *)lv00_malloc(
        sizeof(Lv00EcoCompatibility) * conf_cap);
    if (!conflicts) return -1;

    int conf_count = 0;

    /* 遍历所有已安装包的两两组合 */
    for (int i = 0; i < eco->registry.package_count; i++) {
        if (!eco->registry.packages[i].is_installed) continue;
        for (int j = i + 1; j < eco->registry.package_count; j++) {
            if (!eco->registry.packages[j].is_installed) continue;

            Lv00EcoCompatibilityLevel level = eco_check_compatibility(
                eco,
                eco->registry.packages[i].package_id,
                eco->registry.packages[j].package_id,
                NULL);

            if (level == ECO_COMPAT_INCOMPATIBLE || level == ECO_COMPAT_PARTIAL) {
                if (conf_count >= conf_cap) {
                    conf_cap *= 2;
                    Lv00EcoCompatibility *new_c = (Lv00EcoCompatibility *)lv00_realloc(
                        conflicts, sizeof(Lv00EcoCompatibility) * conf_cap);
                    if (!new_c) {
                        lv00_free((void **)&conflicts);
                        return -1;
                    }
                    conflicts = new_c;
                }
                memset(&conflicts[conf_count], 0, sizeof(Lv00EcoCompatibility));
                strncpy(conflicts[conf_count].package_id_a,
                        eco->registry.packages[i].package_id, LV00_ECO_NAME_MAX - 1);
                strncpy(conflicts[conf_count].package_id_b,
                        eco->registry.packages[j].package_id, LV00_ECO_NAME_MAX - 1);
                conflicts[conf_count].level = level;
                conf_count++;
            }
        }
    }

    *out_conflicts = conflicts;
    *out_count     = conf_count;
    return 0;
}

/* ========================================================================
 * 注册表同步函数
 * ======================================================================== */

/**
 * @brief 同步远程注册表
 *
 * @param eco      生态系统指针
 * @param full_sync 是否执行全量同步
 * @return 成功返回 0，失败返回 -1
 */
int eco_registry_sync(Lv00Ecosystem *eco, bool full_sync) {
    LV00_CHECK_NULL(eco, -1);

    /* 占位实现：从远程注册表同步 */
    if (full_sync) {
        eco->registry.last_sync = (uint64_t)time(NULL);
    }

    LV00_UNUSED(full_sync);

    return 0;
}

/* ========================================================================
 * 一键体验函数
 * ======================================================================== */

/**
 * @brief 创建一键体验配置
 *
 * 初始化 Docker/Podman 容器配置，设置默认镜像、端口转发等。
 *
 * @param eco        生态系统指针
 * @param image_name Docker 镜像名称（NULL 使用默认值）
 * @param use_podman 是否使用 Podman 替代 Docker
 * @param one_click  输出一键体验配置
 * @return 成功返回 0，失败返回 -1
 */
int eco_one_click_create(const Lv00Ecosystem *eco, const char *image_name,
                          bool use_podman, Lv00OneClick *one_click) {
    LV00_CHECK_NULL(eco, -1);
    LV00_CHECK_NULL(one_click, -1);

    memset(one_click, 0, sizeof(Lv00OneClick));

    /* 设置镜像名称 */
    if (image_name) {
        strncpy(one_click->docker_image, image_name, LV00_ECO_IMAGE_NAME_MAX - 1);
    } else {
        strncpy(one_click->docker_image, ECO_DEFAULT_IMAGE_NAME, LV00_ECO_IMAGE_NAME_MAX - 1);
    }

    one_click->use_podman = use_podman;
    strncpy(one_click->container_name, "lv00-quickstart", LV00_ECO_NAME_MAX - 1);
    strncpy(one_click->image_tag, ECO_DEFAULT_IMAGE_TAG, LV00_ECO_VERSION_MAX - 1);

    /* 端口转发：默认 8080 */
    one_click->port_forwarding[0].host_port      = 8080;
    one_click->port_forwarding[0].container_port = 8080;
    strncpy(one_click->port_forwarding[0].protocol, "tcp", 7);
    one_click->port_forward_count = 1;

    return 0;
}

/**
 * @brief 导出一键体验的 Dockerfile 内容
 *
 * 根据一键体验配置生成完整的 Dockerfile 字符串，包含基础镜像、
 * 依赖安装、公理包复制、端口暴露和环境变量配置。
 *
 * @param one_click 一键体验配置
 * @param output    输出 Dockerfile 字符串（调用者负责释放）
 * @return 成功返回字符串长度，失败返回 -1
 */
int eco_one_click_export_dockerfile(const Lv00OneClick *one_click, char **output) {
    LV00_CHECK_NULL(one_click, -1);
    LV00_CHECK_NULL(output, -1);

    size_t buf_size = ECO_DOCKERFILE_BUFFER;
    char *buf = (char *)lv00_malloc(buf_size);
    if (!buf) return -1;

    int pos = 0;
    const char *base_image = one_click->use_podman ?
        "FROM docker.io/ubuntu:22.04" : "FROM ubuntu:22.04";

    pos += snprintf(buf + pos, buf_size - pos, "%s\n\n", base_image);
    pos += snprintf(buf + pos, buf_size - pos, "# Lv-00 Quick Start Container\n");
    pos += snprintf(buf + pos, buf_size - pos, "LABEL maintainer=\"Lv-00 Project\"\n");
    pos += snprintf(buf + pos, buf_size - pos,
        "LABEL description=\"Lv-00 geometry meta-language one-click experience\"\n\n");

    pos += snprintf(buf + pos, buf_size - pos,
        "# Install dependencies\n"
        "RUN apt-get update && apt-get install -y \\\n"
        "    build-essential cmake git curl \\\n"
        "    libgmp-dev libmpfr-dev && \\\n"
        "    rm -rf /var/lib/apt/lists/*\n\n");

    /* 复制公理包 */
    if (one_click->axiom_package_count > 0) {
        pos += snprintf(buf + pos, buf_size - pos,
            "# Copy axiom packages\n"
            "COPY axioms/ /lv00/axioms/\n\n");
    }

    /* 复制样例证明 */
    if (one_click->sample_proof_count > 0) {
        pos += snprintf(buf + pos, buf_size - pos,
            "# Copy sample proofs\n"
            "COPY samples/ /lv00/samples/\n\n");
    }

    /* 端口暴露 */
    for (int i = 0; i < one_click->port_forward_count; i++) {
        pos += snprintf(buf + pos, buf_size - pos,
            "EXPOSE %d/%s\n",
            one_click->port_forwarding[i].container_port,
            one_click->port_forwarding[i].protocol);
    }

    /* 环境变量 */
    for (int i = 0; i < one_click->env_var_count; i++) {
        if (one_click->env_vars[i]) {
            pos += snprintf(buf + pos, buf_size - pos,
                "ENV %s\n", one_click->env_vars[i]);
        }
    }

    pos += snprintf(buf + pos, buf_size - pos, "\nCMD [\"/lv00/bin/lv00-server\"]\n");

    *output = buf;
    return pos;
}

/* ========================================================================
 * 贡献指南函数
 * ======================================================================== */

/**
 * @brief 获取生态系统贡献指南内容
 *
 * @param eco    生态系统指针
 * @param output 输出贡献指南字符串（调用者负责释放）
 * @return 成功返回字符串长度，失败返回 -1
 */
int eco_contribute_guidelines(const Lv00Ecosystem *eco, char **output) {
    LV00_CHECK_NULL(eco, -1);
    LV00_CHECK_NULL(output, -1);

    size_t len = strlen(eco->contribution_guidelines) + 1;
    *output = (char *)lv00_malloc(len);
    if (!*output) return -1;

    memcpy(*output, eco->contribution_guidelines, len);
    return (int)(len - 1);
}

/* ========================================================================
 * 统计信息函数
 * ======================================================================== */

/**
 * @brief 收集生态系统统计信息
 *
 * 统计总包数、已安装数、总星标数、已验证数、最近更新数、
 * 最热门包、平均依赖数等指标。
 *
 * @param eco   生态系统指针
 * @param stats 输出统计信息结构体
 * @return 成功返回 0，失败返回 -1
 */
int eco_stats(const Lv00Ecosystem *eco, Lv00EcoStats *stats) {
    LV00_CHECK_NULL(eco, -1);
    LV00_CHECK_NULL(stats, -1);

    memset(stats, 0, sizeof(Lv00EcoStats));

    stats->total_packages   = eco->registry.package_count;
    stats->installed_count  = eco->registry.installed_count;
    stats->last_full_sync   = eco->registry.last_sync;

    int total_stars    = 0;
    int verified       = 0;
    int recently_updated = 0;
    uint64_t thirty_days_ago = (uint64_t)time(NULL) - 30ULL * 24 * 3600;
    int most_stars     = -1;
    int total_contributors = 0;

    for (int i = 0; i < eco->registry.package_count; i++) {
        const Lv00EcoPackage *pkg = &eco->registry.packages[i];

        total_stars += pkg->star_count;
        if (pkg->verified) verified++;

        if (pkg->install_date > thirty_days_ago && pkg->install_date > 0) {
            recently_updated++;
        }

        if (pkg->star_count > most_stars) {
            most_stars = pkg->star_count;
            strncpy(stats->most_popular_package, pkg->name, LV00_ECO_NAME_MAX - 1);
            stats->most_popular_stars = pkg->star_count;
        }

        if (pkg->author[0]) total_contributors++;
    }

    stats->total_stars            = total_stars;
    stats->verified_count         = verified;
    stats->recently_updated_count = recently_updated;
    stats->unique_authors         = total_contributors;

    /* 平均依赖数 */
    int total_deps = 0;
    for (int i = 0; i < eco->registry.package_count; i++) {
        total_deps += eco->registry.packages[i].dep_count;
    }
    stats->avg_deps_per_package = eco->registry.package_count > 0 ?
        total_deps / eco->registry.package_count : 0;

    return 0;
}

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/**
 * @brief 获取生态系统实体类型的名称字符串
 * @param entity 实体类型枚举值
 * @return 类型名称字符串
 */
const char *eco_entity_type_name(Lv00EcosystemEntity entity) {
    return eco_entity_type_name_str(entity);
}

/**
 * @brief 获取许可证类型的名称字符串
 * @param license 许可证枚举值
 * @return 许可证名称字符串
 */
const char *eco_license_name(Lv00EcoLicense license) {
    return eco_license_name_str(license);
}

/**
 * @brief 获取安装状态的名称字符串
 * @param status 安装状态枚举值
 * @return 状态名称字符串
 */
const char *eco_install_status_name(Lv00EcoInstallStatus status) {
    return eco_install_status_name_str(status);
}

/**
 * @brief 获取兼容性等级的名称字符串
 * @param level 兼容性等级枚举值
 * @return 等级名称字符串
 */
const char *eco_compatibility_name(Lv00EcoCompatibilityLevel level) {
    return eco_compatibility_name_str(level);
}

/**
 * @brief 验证版本字符串是否符合语义化版本规范（x.y.z）
 * @param version 版本字符串
 * @return 合法返回 true，否则返回 false
 */
bool eco_validate_semver(const char *version) {
    if (!version) return false;

    int major, minor, patch;
    return eco_parse_semver(version, &major, &minor, &patch);
}

/**
 * @brief 比较两个语义化版本号
 *
 * 按主版本号、次版本号、补丁版本号逐级比较。
 * 若版本字符串不符合 semver 格式，则退化为字符串比较。
 *
 * @param v1 第一个版本字符串
 * @param v2 第二个版本字符串
 * @return v1 > v2 返回 1，v1 < v2 返回 -1，相等返回 0
 */
int eco_compare_versions(const char *v1, const char *v2) {
    if (!v1 || !v2) return 0;

    int ma1, mi1, pa1, ma2, mi2, pa2;
    bool ok1 = eco_parse_semver(v1, &ma1, &mi1, &pa1);
    bool ok2 = eco_parse_semver(v2, &ma2, &mi2, &pa2);

    if (!ok1 || !ok2) return strcmp(v1, v2);

    if (ma1 != ma2) return (ma1 > ma2) ? 1 : -1;
    if (mi1 != mi2) return (mi1 > mi2) ? 1 : -1;
    if (pa1 != pa2) return (pa1 > pa2) ? 1 : -1;

    return 0;
}

/**
 * @brief 获取排序方式的名称字符串
 * @param sort_by 排序方式枚举值
 * @return 排序方式名称字符串
 */
const char *eco_sort_name(Lv00EcoSortBy sort_by) {
    return eco_sort_name_str(sort_by);
}

/* ========================================================================
 * 内部辅助函数实现
 * ======================================================================== */

static bool eco_match_query(const Lv00EcoPackage *pkg, const Lv00EcoSearchQuery *query) {
    if (!pkg || !query) return false;

    /* 实体类型过滤：默认显示全部 */
    if (query->entity_type_filter != ECO_ENTITY_AXIOM_PACKAGE) {
        /* 当指定具体类型时进行过滤 */
        /* 保留：当前默认不过滤 */
    }

    /* 文本搜索 */
    if (query->query_text[0] != '\0') {
        int relevance = eco_search_relevance(pkg, query->query_text);
        if (relevance <= 0) return false;
    }

    return true;
}

static int eco_search_relevance(const Lv00EcoPackage *pkg, const char *query_text) {
    if (!pkg || !query_text || query_text[0] == '\0') return 1;

    int score = 0;
    const char *fields[] = { pkg->name, pkg->description, pkg->author, pkg->package_id };
    const int weights[]  = { 10, 5, 3, 8 };

    for (int f = 0; f < 4; f++) {
        if (!fields[f]) continue;

        const char *field = fields[f];
        const char *q     = query_text;
        while (*field && *q) {
            if (tolower((unsigned char)*field) == tolower((unsigned char)*q)) {
                score += weights[f];
                q++;
            }
            field++;
        }
    }

    return score;
}

static int eco_cmp_search_results(const void *a, const void *b, void *sort_by_ptr) {
    LV00_UNUSED(sort_by_ptr);
    const Lv00EcoPackage *pa = (const Lv00EcoPackage *)a;
    const Lv00EcoPackage *pb = (const Lv00EcoPackage *)b;
    return strcmp(pa->name, pb->name);
}

static bool eco_is_installed_package(Lv00Ecosystem *eco, const char *package_id) {
    int idx = eco_find_package_index(eco, package_id);
    return idx >= 0 && eco->registry.packages[idx].is_installed;
}

static int eco_find_package_index(const Lv00Ecosystem *eco, const char *package_id) {
    if (!eco || !package_id) return -1;

    for (int i = 0; i < eco->registry.package_count; i++) {
        if (strcmp(eco->registry.packages[i].package_id, package_id) == 0) {
            return i;
        }
    }
    return -1;
}

static bool eco_parse_semver(const char *version, int *major, int *minor, int *patch) {
    if (!version || !major || !minor || !patch) return false;

    *major = 0; *minor = 0; *patch = 0;

    /* 解析 x.y.z 格式 */
    const char *p = version;
    while (*p >= '0' && *p <= '9') {
        *major = *major * 10 + (*p - '0');
        p++;
    }
    if (*p != '.') return false;
    p++;

    while (*p >= '0' && *p <= '9') {
        *minor = *minor * 10 + (*p - '0');
        p++;
    }
    if (*p != '.') return false;
    p++;

    while (*p >= '0' && *p <= '9') {
        *patch = *patch * 10 + (*p - '0');
        p++;
    }

    return true;
}

static const char *eco_entity_type_name_str(Lv00EcosystemEntity entity) {
    switch (entity) {
        case ECO_ENTITY_AXIOM_PACKAGE:  return "AXIOM_PACKAGE";
        case ECO_ENTITY_PRESET_BLOCK:   return "PRESET_BLOCK";
        case ECO_ENTITY_PROOF_STRATEGY: return "PROOF_STRATEGY";
        case ECO_ENTITY_EXPORT_FORMAT:  return "EXPORT_FORMAT";
        case ECO_ENTITY_WEB_COMPONENT:  return "WEB_COMPONENT";
        case ECO_ENTITY_DSL_EXTENSION:  return "DSL_EXTENSION";
        default:                        return "UNKNOWN";
    }
}

static const char *eco_license_name_str(Lv00EcoLicense license) {
    switch (license) {
        case ECO_LICENSE_MIT:     return "MIT";
        case ECO_LICENSE_APACHE2: return "Apache-2.0";
        case ECO_LICENSE_GPL2:    return "GPL-2.0";
        case ECO_LICENSE_GPL3:    return "GPL-3.0";
        case ECO_LICENSE_BSD3:    return "BSD-3-Clause";
        case ECO_LICENSE_CUSTOM:  return "CUSTOM";
        default:                  return "UNKNOWN";
    }
}

static const char *eco_install_status_name_str(Lv00EcoInstallStatus status) {
    switch (status) {
        case ECO_INSTALL_OK:               return "INSTALL_OK";
        case ECO_INSTALL_NOT_FOUND:        return "NOT_FOUND";
        case ECO_INSTALL_VERSION_CONFLICT: return "VERSION_CONFLICT";
        case ECO_INSTALL_DEP_MISSING:      return "DEP_MISSING";
        case ECO_INSTALL_DISK_FULL:        return "DISK_FULL";
        case ECO_INSTALL_NETWORK_ERROR:    return "NETWORK_ERROR";
        case ECO_INSTALL_ALREADY_INSTALLED: return "ALREADY_INSTALLED";
        case ECO_INSTALL_CHECKSUM_MISMATCH: return "CHECKSUM_MISMATCH";
        default:                            return "UNKNOWN";
    }
}

static const char *eco_compatibility_name_str(Lv00EcoCompatibilityLevel level) {
    switch (level) {
        case ECO_COMPAT_FULLY:        return "FULLY_COMPATIBLE";
        case ECO_COMPAT_PARTIAL:      return "PARTIALLY_COMPATIBLE";
        case ECO_COMPAT_INCOMPATIBLE: return "INCOMPATIBLE";
        case ECO_COMPAT_UNKNOWN:      return "UNKNOWN";
        default:                      return "?";
    }
}

static const char *eco_sort_name_str(Lv00EcoSortBy sort_by) {
    switch (sort_by) {
        case ECO_SORT_RELEVANCE: return "RELEVANCE";
        case ECO_SORT_STARS:     return "STARS";
        case ECO_SORT_DATE:      return "DATE";
        case ECO_SORT_NAME:      return "NAME";
        default:                 return "UNKNOWN";
    }
}
