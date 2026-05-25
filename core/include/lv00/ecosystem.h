/**
 * @file ecosystem.h
 * @brief 开放生态包管理系统 —— 借鉴 OpenGeometry / mai / GAP 的联盟共建哲学
 *
 * @details 设计借鉴来源：
 *          - OpenGeometry Group (opengeometry.org, 2023-)
 *            · 联盟共建生态 —— 多家机构/个人协作构建几何公理包生态
 *            · 开放注册与发现机制 —— 任何人均可提交公理包到社区注册表
 *            · 社区贡献指南 —— 包质量审核标准与兼容性声明
 *
 *          - mai (github.com/Xpitfire/mai, 2024-)
 *            · 极简哲学 —— "只需一个 Docker 命令即可体验"
 *            · 预配置镜像分发 —— 免去复杂安装配置，一键运行
 *            · Docker / Podman 优先的用户体验
 *
 *          - GAP (gap-system.org, 1986-)
 *            · 包管理系统（PackageManager）—— 成熟的数学软件包生态
 *            · 包依赖解析与兼容性矩阵
 *            · 远程注册表同步机制
 *
 *          设计目标：
 *          - 提供公理包/预设块/证明策略/导出格式的开放注册与发现
 *          - 社区贡献的数据结构与指南
 *          - 包兼容性矩阵与自动冲突解决
 *          - Docker 一键体验分发（mai 风格）
 *          - 与 GAP 包管理兼容的注册表同步
 *
 * @author Lv-00 Project
 * @version v3.3.0
 * @date 2026-05-24
 */

#ifndef LV00_ECOSYSTEM_H
#define LV00_ECOSYSTEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量定义 ==================== */

/** 最大注册包数量 */
#define LV00_ECO_MAX_PACKAGES 1024

/** 包名称最大长度 */
#define LV00_ECO_NAME_MAX 128

/** 包版本字符串最大长度 */
#define LV00_ECO_VERSION_MAX 32

/** 作者名称最大长度 */
#define LV00_ECO_AUTHOR_MAX 128

/** 包描述最大长度 */
#define LV00_ECO_DESC_MAX 1024

/** 包来源 URL 最大长度 */
#define LV00_ECO_URL_MAX 512

/** 包依赖项最大数量 */
#define LV00_ECO_MAX_DEPENDENCIES 8

/** 最低 Lv-00 版本字符串最大长度 */
#define LV00_ECO_MIN_VERSION_MAX 16

/** 注册表 URL 最大长度 */
#define LV00_ECO_REGISTRY_URL_MAX 512

/** 搜索查询字符串最大长度 */
#define LV00_ECO_QUERY_MAX 256

/** Docker/OCI 镜像名称最大长度 */
#define LV00_ECO_IMAGE_NAME_MAX 256

/** 端口转发规则最大数量 */
#define LV00_ECO_MAX_PORT_FORWARDS 16

/** 贡献指南文本最大长度（Markdown） */
#define LV00_ECO_GUIDELINES_MAX 32768

/** 预配置示例证明最大数量 */
#define LV00_ECO_MAX_SAMPLE_PROOFS 16

/* ==================== 枚举定义 ==================== */

/**
 * @brief 生态实体类型 —— 注册表中可注册的所有内容类型
 *
 * 借鉴 OpenGeometry Group 的开放注册理念，
 * 不仅是公理包，所有可共享的生态组件都可以注册和发现。
 */
typedef enum {
    ECO_ENTITY_AXIOM_PACKAGE = 0,  /**< 公理包：一组公理/定理的集合 */
    ECO_ENTITY_PRESET_BLOCK = 1,   /**< 预设函数块：可复用的几何构造单元 */
    ECO_ENTITY_PROOF_STRATEGY = 2, /**< 证明策略：证明搜索启发式方法 */
    ECO_ENTITY_EXPORT_FORMAT = 3,  /**< 导出格式插件：新增输出格式 */
    ECO_ENTITY_WEB_COMPONENT = 4,  /**< Web 组件：前端可视化组件 */
    ECO_ENTITY_DSL_EXTENSION = 5,  /**< DSL 扩展：语法/语义扩展模块 */
} Lv00EcosystemEntity;

/**
 * @brief 开源许可证类型
 */
typedef enum {
    ECO_LICENSE_MIT = 0,     /**< MIT License */
    ECO_LICENSE_APACHE2 = 1, /**< Apache License 2.0 */
    ECO_LICENSE_GPL2 = 2,    /**< GNU General Public License v2 */
    ECO_LICENSE_GPL3 = 3,    /**< GNU General Public License v3 */
    ECO_LICENSE_BSD3 = 4,    /**< BSD 3-Clause License */
    ECO_LICENSE_CUSTOM = 99, /**< 自定义许可证 */
} Lv00EcoLicense;

/**
 * @brief 排序方式 —— 用于包搜索结果的排序
 */
typedef enum {
    ECO_SORT_RELEVANCE = 0, /**< 按相关性排序（默认） */
    ECO_SORT_STARS = 1,     /**< 按星标数量排序（最受欢迎） */
    ECO_SORT_DATE = 2,      /**< 按发布日期排序（最新优先） */
    ECO_SORT_NAME = 3,      /**< 按名称字母序排序 */
} Lv00EcoSortBy;

/**
 * @brief 包安装状态
 */
typedef enum {
    ECO_INSTALL_OK = 0,                /**< 安装成功 */
    ECO_INSTALL_NOT_FOUND = 1,         /**< 包未在注册表中找到 */
    ECO_INSTALL_VERSION_CONFLICT = 2,  /**< 版本不兼容 */
    ECO_INSTALL_DEP_MISSING = 3,       /**< 依赖项缺失 */
    ECO_INSTALL_DISK_FULL = 4,         /**< 磁盘空间不足 */
    ECO_INSTALL_NETWORK_ERROR = 5,     /**< 网络错误 */
    ECO_INSTALL_ALREADY_INSTALLED = 6, /**< 已安装相同版本 */
    ECO_INSTALL_CHECKSUM_MISMATCH = 7, /**< 校验和不匹配 */
} Lv00EcoInstallStatus;

/**
 * @brief 兼容性判定结果
 */
typedef enum {
    ECO_COMPAT_FULLY = 0,        /**< 完全兼容 */
    ECO_COMPAT_PARTIAL = 1,      /**< 部分兼容（需指定版本范围） */
    ECO_COMPAT_INCOMPATIBLE = 2, /**< 不兼容 */
    ECO_COMPAT_UNKNOWN = 3,      /**< 未知（缺乏兼容性数据） */
} Lv00EcoCompatibilityLevel;

/* ==================== 结构体定义 ==================== */

/**
 * @brief 生态包 —— 注册表中的单个可分发单元
 *
 * 封装一个生态实体的所有元数据，包括身份信息、许可证、依赖项、
 * 来源 URL 和社区评分。是包管理的核心数据结构。
 */
typedef struct Lv00EcoPackage {
    /* ── 身份信息 ── */
    char package_id[LV00_ECO_NAME_MAX]; /**< 包唯一标识符 */
    char name[LV00_ECO_NAME_MAX];       /**< 包名称（人类可读） */
    char version[LV00_ECO_VERSION_MAX]; /**< 语义化版本号 */
    char author[LV00_ECO_AUTHOR_MAX];   /**< 作者/组织名称 */

    /* ── 许可证 ── */
    Lv00EcoLicense license_type;                 /**< 许可证类型 */
    char custom_license_text[LV00_ECO_DESC_MAX]; /**< 自定义许可证全文（CUSTOM 时使用） */

    /* ── 描述 ── */
    char description[LV00_ECO_DESC_MAX]; /**< 包的功能描述 */
    char source_url[LV00_ECO_URL_MAX];   /**< 源代码仓库 URL */

    /* ── 依赖项 ── */
    char dependencies[LV00_ECO_MAX_DEPENDENCIES][LV00_ECO_NAME_MAX]; /**< 依赖包 ID 列表 */
    int dep_count;                                                   /**< 依赖项数量 */

    /* ── 实体类型 ── */
    Lv00EcosystemEntity entity_type; /**< 实体类型 */

    /* ── 安装信息 ── */
    uint64_t install_date;                           /**< 安装时间戳（Unix epoch） */
    int star_count;                                  /**< 星标数量 */
    bool verified;                                   /**< 官方认证标志 */
    char min_lv00_version[LV00_ECO_MIN_VERSION_MAX]; /**< 最低兼容 Lv-00 版本 */

    /* ── 安装后路径 ── */
    char install_path[LV00_ECO_URL_MAX]; /**< 本地安装路径 */
    bool is_installed;                   /**< 是否已安装 */
} Lv00EcoPackage;

/**
 * @brief 生态注册表 —— 借鉴 OpenGeometry Group 联盟注册中心
 *
 * 维护所有可发现生态包的索引。
 * 支持从远程注册表同步，以及本地离线包管理。
 */
typedef struct Lv00EcoRegistry {
    /* ── 包列表 ── */
    Lv00EcoPackage packages[LV00_ECO_MAX_PACKAGES]; /**< 已注册的包数组 */
    int package_count;                              /**< 已注册包数量 */

    /* ── 注册表元数据 ── */
    char registry_url[LV00_ECO_REGISTRY_URL_MAX]; /**< 远程注册表 URL */
    uint64_t last_sync;                           /**< 上次同步时间戳（0 = 从未同步） */

    /* ── 更新策略 ── */
    bool auto_update; /**< 是否自动同步注册表索引 */

    /* ── 统计缓存 ── */
    int total_packages_remote; /**< 远程包总数 */
    int verified_packages;     /**< 已验证包数量 */
    int recently_updated;      /**< 最近 30 天更新的包数量 */
    int installed_count;       /**< 已安装包数量 */
} Lv00EcoRegistry;

/**
 * @brief 包兼容性矩阵条目
 *
 * 记录两个包之间的兼容性关系。借鉴 GAP PackageManager 的
 * 兼容性声明机制——每个包可以在 manifest 中声明与其他包的兼容性。
 */
typedef struct Lv00EcoCompatibility {
    /* ── 涉及的包 ── */
    char package_id_a[LV00_ECO_NAME_MAX]; /**< 包 A 的标识符 */
    char package_id_b[LV00_ECO_NAME_MAX]; /**< 包 B 的标识符 */

    /* ── 兼容性 ── */
    Lv00EcoCompatibilityLevel level; /**< 兼容性级别 */

    /* ── 冲突详情 ── */
    char conflict_reason[LV00_ECO_DESC_MAX]; /**< 不兼容原因（人类可读） */
    char resolution[LV00_ECO_DESC_MAX];      /**< 建议的解决方案 */
} Lv00EcoCompatibility;

/**
 * @brief 包搜索查询
 *
 * 封装搜索请求的所有参数，支持全文搜索、类型过滤、
 * 排序和分页。用于注册表中的包发现。
 */
typedef struct Lv00EcoSearchQuery {
    /* ── 查询条件 ── */
    char query_text[LV00_ECO_QUERY_MAX];    /**< 搜索关键词 */
    Lv00EcosystemEntity entity_type_filter; /**< 实体类型过滤（ECO_ENTITY_AXIOM_PACKAGE = 不过滤） */

    /* ── 排序 ── */
    Lv00EcoSortBy sort_by; /**< 排序方式 */

    /* ── 分页 ── */
    int page_size;   /**< 每页结果数 */
    int page_offset; /**< 偏移量 */
} Lv00EcoSearchQuery;

/**
 * @brief 一键体验配置 —— 借鉴 mai Docker 一键体验
 *
 * mai 的核心理念："你只需要一个 Docker 命令。"
 * 将复杂的 Lv-00 安装/配置/公理包加载过程封装为
 * 预构建的 OCI 容器镜像，用户无需手动安装任何依赖。
 */
typedef struct Lv00OneClick {
    /* ── 镜像配置 ── */
    char docker_image[LV00_ECO_IMAGE_NAME_MAX]; /**< Docker/Podman 镜像名 */
    char container_name[LV00_ECO_NAME_MAX];     /**< 容器名称 */
    bool use_podman;                            /**< 是否使用 Podman（替代 Docker） */

    /* ── 预配置内容 ── */
    char required_axiom_packages[LV00_ECO_MAX_DEPENDENCIES][LV00_ECO_NAME_MAX]; /**< 预装公理包 */
    int axiom_package_count;                                                    /**< 预装公理包数量 */
    char sample_proofs[LV00_ECO_MAX_SAMPLE_PROOFS][LV00_ECO_DESC_MAX];          /**< 样例证明 */
    int sample_proof_count;                                                     /**< 样例证明数量 */

    /* ── 端口转发 ── */
    struct {
        int host_port;                             /**< 主机端口 */
        int container_port;                        /**< 容器端口 */
        char protocol[8];                          /**< 协议：tcp/udp */
    } port_forwarding[LV00_ECO_MAX_PORT_FORWARDS]; /**< 端口转发规则 */
    int port_forward_count;                        /**< 转发规则数量 */

    /* ── 环境变量 ── */
    char **env_vars;   /**< 环境变量（KEY=VALUE 格式） */
    int env_var_count; /**< 环境变量数量 */

    /* ── 挂载卷 ── */
    char **volumes;   /**< 卷挂载（host_path:container_path 格式） */
    int volume_count; /**< 卷数量 */

    /* ── GPU 支持 ── */
    bool enable_gpu; /**< 启用 GPU 加速 */

    /* ── 镜像标签 ── */
    char image_tag[LV00_ECO_VERSION_MAX]; /**< 镜像标签 */
} Lv00OneClick;

/**
 * @brief 生态统计
 *
 * 汇总注册表的关键统计指标。
 */
typedef struct Lv00EcoStats {
    int total_packages;                           /**< 总包数量 */
    int verified_count;                           /**< 已验证包数量 */
    int recently_updated_count;                   /**< 最近 30 天更新数 */
    int installed_count;                          /**< 已安装包数量 */
    int total_stars;                              /**< 总星标数 */
    int unique_authors;                           /**< 不同作者数量 */
    int avg_deps_per_package;                     /**< 每个包的依赖项平均数 */
    char most_popular_package[LV00_ECO_NAME_MAX]; /**< 最受欢迎包名称 */
    int most_popular_stars;                       /**< 最受欢迎包星标数 */
    uint64_t last_full_sync;                      /**< 上次全量同步时间戳 */
} Lv00EcoStats;

/**
 * @brief 生态主上下文
 *
 * 聚合注册表、兼容性矩阵和贡献指南的顶层结构。
 */
typedef struct Lv00Ecosystem {
    Lv00EcoRegistry registry;            /**< 注册表 */
    Lv00EcoCompatibility *compat_matrix; /**< 兼容性矩阵数组 */
    int compat_matrix_count;             /**< 兼容性条目数量 */
    Lv00OneClick one_click;              /**< 一键体验配置 */

    /* ── 贡献指南 ── */
    char contribution_guidelines[LV00_ECO_GUIDELINES_MAX]; /**< 贡献指南 Markdown 文本 */
    int guidelines_length;                                 /**< 指南文本长度 */
} Lv00Ecosystem;

/* ==================== 生命周期 ==================== */

/**
 * @brief 初始化生态系统
 *
 * 分配并初始化 Lv00Ecosystem 上下文，
 * 设置默认注册表 URL 和贡献指南模板。
 *
 * @return 新分配的生态系统上下文，失败返回 NULL
 */
Lv00Ecosystem *eco_init(void);

/**
 * @brief 销毁生态系统并释放所有关联资源
 *
 * 包括兼容性矩阵、一键体验配置中的字符串数组等。
 * 传入 NULL 是安全的。
 *
 * @param[in,out] eco 生态系统上下文（设为 NULL 是安全的）
 */
void eco_destroy(Lv00Ecosystem *eco);

/* ==================== 包管理 ==================== */

/**
 * @brief 向注册表注册一个包
 *
 * 将包元数据添加到本地注册表。如果包 ID 已存在，
 * 检查版本号——更高版本则更新，相同版本则忽略。
 *
 * @param[in,out] eco 生态系统上下文
 * @param[in]     pkg 包信息
 * @return 成功返回包索引，已存在返回 -1，内存满返回 -2
 */
int eco_package_register(Lv00Ecosystem *eco, const Lv00EcoPackage *pkg);

/**
 * @brief 从注册表移除一个包
 *
 * @param[in,out] eco        生态系统上下文
 * @param[in]     package_id 要移除的包标识符
 * @return 成功返回 0，未找到返回 -1
 */
int eco_package_unregister(Lv00Ecosystem *eco, const char *package_id);

/**
 * @brief 在注册表中搜索包
 *
 * 支持全文搜索（匹配名称、描述、作者）和实体类型过滤。
 * 排序和分页由查询参数控制。
 *
 * @param[in]  eco       生态系统上下文
 * @param[in]  query     搜索查询
 * @param[out] results   输出：匹配的包数组（调用者负责 free）
 * @param[out] out_count 输出：结果数量
 * @return 成功返回 0，失败返回 -1
 */
int eco_package_search(const Lv00Ecosystem *eco, const Lv00EcoSearchQuery *query, Lv00EcoPackage **results,
                       int *out_count);

/* ==================== 安装管理 ==================== */

/**
 * @brief 从注册表安装一个包到本地
 *
 * 自动解析并安装依赖项（递归）。
 * 借鉴 GAP PackageManager 的依赖解析策略：
 * 1. 查找包元数据
 * 2. 检查 Lv-00 版本兼容性
 * 3. 递归解析依赖项
 * 4. 下载并安装包文件
 * 5. 校验完整性
 *
 * @param[in,out] eco        生态系统上下文
 * @param[in]     package_id 要安装的包标识符
 * @param[in]     version    期望版本（NULL = 安装最新版）
 * @param[in]     force      强制重新安装（即使已存在）
 * @return 安装状态码
 */
Lv00EcoInstallStatus eco_package_install(Lv00Ecosystem *eco, const char *package_id, const char *version, bool force);

/**
 * @brief 卸载已安装的包
 *
 * 如其他已安装的包依赖此包，返回警告但不阻止卸载。
 *
 * @param[in,out] eco        生态系统上下文
 * @param[in]     package_id 要卸载的包标识符
 * @return 成功返回 0，未安装返回 -1
 */
int eco_package_uninstall(Lv00Ecosystem *eco, const char *package_id);

/**
 * @brief 列出所有已安装的包
 *
 * @param[in]  eco       生态系统上下文
 * @param[out] out_pkgs  输出：已安装包数组（调用者负责 free）
 * @param[out] out_count 输出：已安装包数量
 * @return 成功返回 0，失败返回 -1
 */
int eco_package_list_installed(const Lv00Ecosystem *eco, Lv00EcoPackage **out_pkgs, int *out_count);

/* ==================== 兼容性检查 ==================== */

/**
 * @brief 检查两个包之间的兼容性
 *
 * 查询兼容性矩阵，判定两个包能否共存在同一安装中。
 * 如果矩阵中没有相关条目，返回 ECO_COMPAT_UNKNOWN。
 *
 * @param[in]  eco          生态系统上下文
 * @param[in]  package_id_a 包 A 的标识符
 * @param[in]  package_id_b 包 B 的标识符
 * @param[out] out_result   输出：兼容性结果（调用者可选择 NULL）
 * @return 兼容性级别
 */
Lv00EcoCompatibilityLevel eco_check_compatibility(const Lv00Ecosystem *eco, const char *package_id_a,
                                                  const char *package_id_b, Lv00EcoCompatibility *out_result);

/**
 * @brief 解决所有已安装包之间的冲突
 *
 * 遍历已安装包，检查所有两两组合的兼容性。
 * 对不兼容的包尝试降级版本或提供替代方案。
 *
 * @param[in,out] eco          生态系统上下文
 * @param[out]    out_conflicts 输出：冲突列表（调用者负责 free）
 * @param[out]    out_count     输出：冲突数量
 * @return 成功返回 0，失败返回 -1
 */
int eco_resolve_conflicts(Lv00Ecosystem *eco, Lv00EcoCompatibility **out_conflicts, int *out_count);

/* ==================== 注册表同步 ==================== */

/**
 * @brief 从远程注册表同步包索引
 *
 * 从 registry_url 获取最新的包索引 JSON，
 * 合并到本地注册表。支持增量同步（仅更新变化部分）。
 *
 * 借鉴 GAP 的 PackageManager 远程注册表同步机制：
 * - 增量同步：仅下载上次同步后变化的包元数据
 * - 冲突处理：本地包优先，远程包仅当版本更高时覆盖
 * - 网络容错：超时与重试策略
 *
 * @param[in,out] eco      生态系统上下文
 * @param[in]     full_sync true = 全量覆盖同步，false = 增量同步
 * @return 成功返回 0，网络错误返回 -1，格式错误返回 -2
 */
int eco_registry_sync(Lv00Ecosystem *eco, bool full_sync);

/* ==================== 一键体验 ==================== */

/**
 * @brief 创建一键体验配置
 *
 * 根据当前生态系统配置生成一个预配置的 Docker/Podman 容器定义。
 *
 * @param[in]  eco          生态系统上下文
 * @param[in]  image_name   Docker 镜像名称（NULL = 使用默认 lv00/lv00-quickstart）
 * @param[in]  use_podman   是否使用 Podman
 * @param[out] one_click    输出：一键体验配置
 * @return 成功返回 0，失败返回 -1
 */
int eco_one_click_create(const Lv00Ecosystem *eco, const char *image_name, bool use_podman, Lv00OneClick *one_click);

/**
 * @brief 导出一键体验配置为 Dockerfile
 *
 * 生成一个自包含的 Dockerfile，包含：
 * - 基础镜像选择
 * - Lv-00 核心安装
 * - 预设公理包复制
 * - 样例证明注入
 * - 端口暴露
 *
 * @param[in]  one_click 一键体验配置
 * @param[out] output    输出：Dockerfile 内容（调用者负责 free）
 * @return Dockerfile 字符数，失败返回 -1
 */
int eco_one_click_export_dockerfile(const Lv00OneClick *one_click, char **output);

/* ==================== 贡献指南 ==================== */

/**
 * @brief 获取贡献指南（Markdown 格式）
 *
 * 返回生态系统贡献指南，包括：
 * - 代码风格
 * - 公理包提交规范
 * - PR 流程
 * - 测试要求
 * - 许可证合规
 *
 * @param[in]  eco       生态系统上下文
 * @param[out] output    输出：Markdown 格式贡献指南文本（调用者负责 free）
 * @return 指南文本字符数，失败返回 -1
 */
int eco_contribute_guidelines(const Lv00Ecosystem *eco, char **output);

/* ==================== 统计信息 ==================== */

/**
 * @brief 获取生态系统统计信息
 *
 * @param[in]  eco   生态系统上下文
 * @param[out] stats 输出：统计信息
 * @return 成功返回 0，失败返回 -1
 */
int eco_stats(const Lv00Ecosystem *eco, Lv00EcoStats *stats);

/* ==================== 辅助函数 ==================== */

/**
 * @brief 获取生态实体类型名称
 *
 * @param[in] entity 实体类型
 * @return 类型名称字符串
 */
const char *eco_entity_type_name(Lv00EcosystemEntity entity);

/**
 * @brief 获取许可证名称
 *
 * @param[in] license 许可证类型
 * @return 许可证名称字符串（如 "MIT"）
 */
const char *eco_license_name(Lv00EcoLicense license);

/**
 * @brief 获取安装状态名称
 *
 * @param[in] status 安装状态
 * @return 状态名称字符串
 */
const char *eco_install_status_name(Lv00EcoInstallStatus status);

/**
 * @brief 获取兼容性级别名称
 *
 * @param[in] level 兼容性级别
 * @return 级别名称字符串
 */
const char *eco_compatibility_name(Lv00EcoCompatibilityLevel level);

/**
 * @brief 验证语义化版本号格式
 *
 * @param[in] version 版本号字符串
 * @return 有效返回 true
 */
bool eco_validate_semver(const char *version);

/**
 * @brief 比较两个语义化版本号
 *
 * @param[in] v1 版本号 A
 * @param[in] v2 版本号 B
 * @return 负数（v1 < v2）、0（相等）、正数（v1 > v2）
 */
int eco_compare_versions(const char *v1, const char *v2);

/**
 * @brief 获取排序方式名称
 *
 * @param[in] sort_by 排序方式
 * @return 排序方式名称字符串
 */
const char *eco_sort_name(Lv00EcoSortBy sort_by);

#ifdef __cplusplus
}
#endif

#endif /* LV00_ECOSYSTEM_H */
