# libgit2 版本控制系统参考文档

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴点](#2-核心借鉴点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)
5. [附录](#5-附录)

---

## 1. 项目概述

### 1.1 项目简介

libgit2 是一个纯 C 语言实现的 Git 核心功能库，由 GitHub 官方维护，旨在提供可移植、可重入的 Git 操作接口。与传统的 Git 命令行工具不同，libgit2 将 Git 的核心功能封装为链接库，允许开发者将版本控制能力直接嵌入到应用程序中，而无需依赖外部 Git 进程调用。

该项目诞生于对嵌入式 Git 功能的需求，特别是在资源受限环境、移动平台以及需要高性能版本控制集成的场景中。libgit2 的设计哲学强调零依赖（除标准 C 库外）、跨平台兼容性和线程安全性，使其成为构建自定义版本控制工具、IDE 集成、持续集成系统以及分布式协作应用的理想选择。

### 1.2 技术栈

| 技术维度 | 具体实现 |
|---------|---------|
| 编程语言 | C99 标准，确保最大兼容性 |
| 构建系统 | CMake，支持多平台编译配置 |
| 依赖管理 | 最小依赖设计，核心库仅依赖标准 C 库 |
| 可选依赖 | OpenSSL/LibreSSL（HTTPS 支持）、libssh2（SSH 支持）、zlib（压缩） |
| 测试框架 | 自定义测试框架，包含单元测试和集成测试套件 |
| 文档生成 | Doxygen 注释规范，支持 API 文档自动生成 |
| 包管理 | vcpkg、Conan、Homebrew 等多平台包管理器支持 |

### 1.3 核心架构特点

libgit2 采用分层架构设计，将 Git 功能划分为多个逻辑模块：

- **对象数据库层**：管理 Git 对象（blob、tree、commit、tag）的存储与检索
- **引用管理层**：处理分支、标签等引用操作
- **索引层**：管理暂存区（stage/index）操作
- **网络层**：支持 HTTP/HTTPS/SSH 协议的远程操作
- **传输层**：实现智能 HTTP 传输、Git 协议传输
- **配置层**：解析和管理 Git 配置文件

### 1.4 社区活跃度

libgit2 拥有活跃的开源社区：

- **GitHub 仓库**：https://github.com/libgit2/libgit2
- **Star 数量**：超过 10,000
- **贡献者**：来自全球的 300+ 活跃贡献者
- **发布周期**：稳定的版本发布节奏，每季度发布次要版本更新
- **Issue 响应**：核心维护团队通常在 48 小时内响应关键问题
- **语言绑定**：官方维护或社区支持的绑定包括：
  - libgit2sharp（C#/.NET）
  - rugged（Ruby）
  - pygit2（Python）
  - git2-rs（Rust）
  - nodegit（Node.js）
  - git2go（Go）

### 1.5 许可证

libgit2 采用 **GPLv2 with Linking Exception** 许可证：

- **核心条款**：允许在专有软件中链接和使用 libgit2，无需开源调用方代码
- **修改条款**：对 libgit2 本身的修改需要遵循 GPLv2 开源
- **商业友好**：链接例外条款使得 libgit2 适合商业软件开发
- **专利保护**：包含明确的专利授权条款，降低法律风险

---

## 2. 核心借鉴点

### 2.1 设计哲学借鉴

libgit2 的设计理念对 Lv-00 的证明历史管理系统具有重要参考价值：

| libgit2 设计原则 | Lv-00 应用场景 |
|----------------|--------------|
| 纯 C 实现，零依赖 | Lv-00 第 1 层基础类需要最小化外部依赖 |
| 可重入设计，线程安全 | 第 3 层算法引擎支持并行约束求解 |
| 跨平台兼容 | 第 7 层应用框架支持 Web/CLI/Python DSL 多平台部署 |
| 对象模型清晰 | 第 2 层约束图、几何节点的层次化管理 |
| 最小化内存占用 | 大规模几何证明场景的内存优化 |
| 增量式操作 | 第 6 层证明导出的增量序列化 |

### 2.2 对象存储模型借鉴

Git 的对象模型（blob、tree、commit、tag）为 Lv-00 的证明数据版本管理提供了范式：

```
Git 对象模型                Lv-00 证明数据模型
    |                              |
    +-- blob (文件内容)    ---->   +-- expr_node (表达式节点)
    +-- tree (目录结构)    ---->   +-- constraint_set (约束集合)
    +-- commit (提交历史)  ---->   +-- proof_state (证明状态快照)
    +-- tag (标签)         ---->   +-- proof_checkpoint (证明检查点)
```

### 2.3 核心特性对照表

| libgit2 特性 | 功能描述 | Lv-00 第 2 层映射 | Lv-00 第 6 层映射 |
|-------------|---------|------------------|------------------|
| `git_repository_open` | 打开/初始化仓库 | 初始化约束图存储上下文 | 加载证明项目仓库 |
| `git_clone` | 克隆远程仓库 | 导入外部几何模型 | 同步远程证明库 |
| `git_commit_create` | 创建提交 | 保存证明状态快照 | 创建证明版本节点 |
| `git_branch_create` | 创建分支 | 创建证明探索分支 | 管理证明变体 |
| `git_merge` | 合并分支 | 合并约束求解路径 | 整合多证明策略结果 |
| `git_diff` | 差异比较 | 比较约束图状态 | 生成证明差异报告 |
| `git_status` | 状态检查 | 检查未保存证明步骤 | 验证证明完整性 |
| `git_index_add` | 暂存变更 | 暂存约束变更 | 暂存证明步骤 |
| `git_revparse` | 版本解析 | 解析证明历史引用 | 定位特定证明版本 |
| `git_object_lookup` | 对象查找 | 检索约束节点 | 加载历史证明对象 |

### 2.4 数据管理借鉴点详解

#### 2.4.1 内容寻址存储（CAS）

libgit2 使用 SHA-1 哈希作为对象的唯一标识符，实现内容寻址存储。Lv-00 可借鉴此机制：

- **应用场景**：第 2 层约束图的节点去重、第 6 层证明数据的增量存储
- **实现方式**：使用 SHA-256 对表达式节点、约束条件进行哈希，构建内容寻址的节点池
- **优势**：自动去重、数据完整性校验、高效差异检测

#### 2.4.2 引用（Reference）系统

Git 的引用系统（分支、标签、HEAD）提供了灵活的历史导航机制：

- **应用场景**：第 4 层证明引擎的多策略并行探索、第 6 层证明版本管理
- **实现方式**：建立符号引用到证明状态哈希的映射，支持快速切换证明路径
- **优势**：轻量级分支管理、支持实验性证明探索、便于回溯

#### 2.4.3 索引（Index）机制

Git 的暂存区（索引）作为工作区与仓库之间的缓冲区：

- **应用场景**：第 3 层约束求解的中间状态管理、第 5 层可视化引擎的交互状态
- **实现方式**：在内存中维护待提交的约束变更集合，支持批量提交或回滚
- **优势**：事务性操作支持、性能优化、用户体验提升

#### 2.4.4 对象数据库抽象

libgit2 的对象数据库（ODB）层支持多种后端存储：

- **应用场景**：第 6 层数据交换的多种序列化格式、第 7 层不同部署环境的存储适配
- **实现方式**：定义存储后端接口，支持内存、文件、数据库等多种实现
- **优势**：存储灵活性、测试便利性、性能可优化性

---

## 3. Lv-00 映射方案

### 3.1 架构映射

将 libgit2 的版本控制模型映射到 Lv-00 的 7 层架构：

```
+-------------------+-------------------------------------------+
|    libgit2 概念    |              Lv-00 映射层                  |
+-------------------+-------------------------------------------+
| Repository        | 第 2 层：约束图存储上下文                   |
|                   | 第 6 层：证明项目仓库                       |
+-------------------+-------------------------------------------+
| Object Database   | 第 2 层：几何节点哈希表                     |
|                   | 第 6 层：证明对象序列化存储                 |
+-------------------+-------------------------------------------+
| Index/Stage       | 第 2 层：约束变更缓冲区                     |
|                   | 第 3 层：求解中间状态                       |
+-------------------+-------------------------------------------+
| Commit            | 第 2 层：约束图快照                         |
|                   | 第 4 层：证明状态节点                       |
+-------------------+-------------------------------------------+
| Branch            | 第 3 层：求解路径分支                       |
|                   | 第 4 层：证明策略分支                       |
+-------------------+-------------------------------------------+
| Tag               | 第 4 层：证明里程碑                         |
|                   | 第 6 层：导出检查点                         |
+-------------------+-------------------------------------------+
| Diff              | 第 3 层：约束差异检测                       |
|                   | 第 6 层：证明差异报告                       |
+-------------------+-------------------------------------------+
| Merge             | 第 3 层：约束图合并                         |
|                   | 第 4 层：多策略结果整合                     |
+-------------------+-------------------------------------------+
```

### 3.2 核心数据结构映射

#### 3.2.1 证明仓库上下文

```c
/**
 * 证明仓库上下文 - 映射 git_repository
 * 对应 Lv-00 第 2 层和第 6 层
 */
typedef struct lv00_proof_repo {
    /* 对象数据库 - 存储证明对象 */
    lv00_odb_t *odb;
    
    /* 引用数据库 - 管理分支和标签 */
    lv00_refdb_t *refdb;
    
    /* 索引 - 暂存区 */
    lv00_index_t *index;
    
    /* 配置 - 仓库设置 */
    lv00_config_t *config;
    
    /* 工作目录路径 */
    char *workdir;
    
    /* 当前 HEAD 引用 */
    char *head;
    
    /* 是否裸仓库（无工作目录） */
    int is_bare;
    
    /* 引用计数 */
    atomic_int refcount;
} lv00_proof_repo_t;
```

#### 3.2.2 证明对象类型

```c
/**
 * 证明对象类型 - 映射 git_object_t
 */
typedef enum {
    LV00_OBJ_ANY = -1,       /* 匹配任何对象类型 */
    LV00_OBJ_BAD = 0,        /* 无效对象 */
    LV00_OBJ_EXPR = 1,       /* 表达式节点（映射 blob） */
    LV00_OBJ_CONSTRAINT = 2, /* 约束集合（映射 tree） */
    LV00_OBJ_PROOF = 3,      /* 证明提交（映射 commit） */
    LV00_OBJ_CHECKPOINT = 4, /* 检查点标签（映射 tag） */
} lv00_object_type_t;

/**
 * 证明对象基础结构 - 映射 git_object
 */
typedef struct lv00_object {
    lv00_object_type_t type;
    lv00_oid_t oid;
    atomic_int refcount;
} lv00_object_t;

/**
 * 表达式节点对象 - 映射 git_blob
 * 对应 Lv-00 第 1 层符号坐标、多项式
 */
typedef struct lv00_expr_node {
    lv00_object_t base;
    
    /* 表达式数据 */
    uint8_t *data;
    size_t size;
    
    /* 表达式类型标记 */
    lv00_expr_type_t expr_type;
    
    /* 缓存的哈希值 */
    lv00_oid_t content_hash;
} lv00_expr_node_t;

/**
 * 约束集合对象 - 映射 git_tree
 * 对应 Lv-00 第 2 层约束图
 */
typedef struct lv00_constraint_set {
    lv00_object_t base;
    
    /* 约束条目数组 */
    lv00_constraint_entry_t *entries;
    size_t entry_count;
    size_t entry_capacity;
    
    /* 父约束集合（用于差异检测） */
    lv00_oid_t parent_oid;
} lv00_constraint_set_t;

/**
 * 约束条目 - 映射 git_tree_entry
 */
typedef struct lv00_constraint_entry {
    lv00_oid_t oid;          /* 指向表达式节点 */
    uint16_t mode;           /* 约束类型和属性 */
    char *name;              /* 约束标识符 */
} lv00_constraint_entry_t;

/**
 * 证明提交对象 - 映射 git_commit
 * 对应 Lv-00 第 4 层证明引擎
 */
typedef struct lv00_proof_commit {
    lv00_object_t base;
    
    /* 父提交数组（支持多父合并） */
    lv00_oid_t *parent_oids;
    size_t parent_count;
    
    /* 根约束集合 */
    lv00_oid_t tree_oid;
    
    /* 作者信息 */
    lv00_signature_t *author;
    lv00_signature_t *committer;
    
    /* 提交消息 */
    char *message;
    
    /* 时间戳 */
    int64_t time;
    int offset;
} lv00_proof_commit_t;

/**
 * 检查点标签 - 映射 git_tag
 * 对应 Lv-00 第 6 层导出检查点
 */
typedef struct lv00_checkpoint {
    lv00_object_t base;
    
    /* 目标对象 */
    lv00_oid_t target_oid;
    lv00_object_type_t target_type;
    
    /* 标签信息 */
    char *name;
    lv00_signature_t *tagger;
    char *message;
} lv00_checkpoint_t;
```

#### 3.2.3 对象 ID 和签名

```c
/**
 * 对象 ID - 映射 git_oid
 * 使用 SHA-256 替代 SHA-1
 */
#define LV00_OID_SHA256_SIZE 32
#define LV00_OID_HEX_SIZE 65

typedef struct {
    uint8_t id[LV00_OID_SHA256_SIZE];
} lv00_oid_t;

/**
 * 签名信息 - 映射 git_signature
 */
typedef struct {
    char *name;
    char *email;
    int64_t when;
    int offset;
} lv00_signature_t;
```

### 3.3 核心 API 实现示例

#### 3.3.1 仓库操作

```c
/**
 * 初始化证明仓库 - 映射 git_repository_init
 * 
 * @param out 输出仓库句柄
 * @param path 仓库路径
 * @param bare 是否创建裸仓库
 * @return 成功返回 0，失败返回错误码
 */
int lv00_proof_repo_init(lv00_proof_repo_t **out, const char *path, int bare);

/**
 * 打开现有证明仓库 - 映射 git_repository_open
 */
int lv00_proof_repo_open(lv00_proof_repo_t **out, const char *path);

/**
 * 释放仓库句柄 - 映射 git_repository_free
 */
void lv00_proof_repo_free(lv00_proof_repo_t *repo);

/* 实现示例 */
int lv00_proof_repo_init(lv00_proof_repo_t **out, const char *path, int bare) {
    lv00_proof_repo_t *repo;
    char *repo_path;
    int err;
    
    /* 分配内存 */
    repo = calloc(1, sizeof(*repo));
    if (!repo) return LV00_ERROR_NOMEM;
    
    /* 构建仓库路径 */
    if (bare) {
        repo_path = strdup(path);
    } else {
        repo_path = malloc(strlen(path) + strlen("/.lv00") + 1);
        sprintf(repo_path, "%s/.lv00", path);
    }
    
    /* 创建目录结构 */
    err = mkdir_recursive(repo_path);
    if (err < 0) goto cleanup;
    
    /* 初始化对象数据库 */
    char *objects_path = malloc(strlen(repo_path) + 20);
    sprintf(objects_path, "%s/objects", repo_path);
    err = lv00_odb_init(&repo->odb, objects_path);
    free(objects_path);
    if (err < 0) goto cleanup;
    
    /* 初始化引用数据库 */
    char *refs_path = malloc(strlen(repo_path) + 20);
    sprintf(refs_path, "%s/refs", repo_path);
    err = lv00_refdb_init(&repo->refdb, refs_path);
    free(refs_path);
    if (err < 0) goto cleanup;
    
    /* 初始化索引 */
    err = lv00_index_init(&repo->index);
    if (err < 0) goto cleanup;
    
    /* 初始化 HEAD */
    repo->head = strdup("ref: refs/heads/main");
    repo->is_bare = bare;
    repo->workdir = bare ? NULL : strdup(path);
    
    atomic_init(&repo->refcount, 1);
    
    *out = repo;
    return 0;
    
cleanup:
    lv00_proof_repo_free(repo);
    return err;
}
```

#### 3.3.2 对象操作

```c
/**
 * 从对象数据库读取对象 - 映射 git_object_lookup
 */
int lv00_object_lookup(
    lv00_proof_repo_t *repo,
    lv00_object_t **out,
    const lv00_oid_t *oid,
    lv00_object_type_t type
);

/**
 * 写入对象到数据库 - 映射 git_odb_write
 */
int lv00_object_write(
    lv00_proof_repo_t *repo,
    lv00_oid_t *out_oid,
    const void *data,
    size_t size,
    lv00_object_type_t type
);

/**
 * 计算对象哈希 - 映射 git_odb_hash
 */
int lv00_object_hash(
    lv00_oid_t *out_oid,
    const void *data,
    size_t size,
    lv00_object_type_t type
);

/* 表达式节点创建示例 */
int lv00_expr_node_create(
    lv00_proof_repo_t *repo,
    lv00_expr_node_t **out,
    const uint8_t *data,
    size_t size,
    lv00_expr_type_t expr_type
) {
    lv00_expr_node_t *node;
    lv00_oid_t oid;
    int err;
    
    /* 计算内容哈希 */
    err = lv00_object_hash(&oid, data, size, LV00_OBJ_EXPR);
    if (err < 0) return err;
    
    /* 检查是否已存在 */
    lv00_object_t *existing;
    err = lv00_object_lookup(repo, &existing, &oid, LV00_OBJ_EXPR);
    if (err == 0) {
        *out = (lv00_expr_node_t *)existing;
        return 0;
    }
    
    /* 创建新节点 */
    node = calloc(1, sizeof(*node));
    if (!node) return LV00_ERROR_NOMEM;
    
    node->base.type = LV00_OBJ_EXPR;
    node->base.oid = oid;
    atomic_init(&node->base.refcount, 1);
    
    node->data = malloc(size);
    if (!node->data) {
        free(node);
        return LV00_ERROR_NOMEM;
    }
    memcpy(node->data, data, size);
    node->size = size;
    node->expr_type = expr_type;
    node->content_hash = oid;
    
    /* 写入对象数据库 */
    err = lv00_object_write(repo, &oid, node, sizeof(*node), LV00_OBJ_EXPR);
    if (err < 0) {
        free(node->data);
        free(node);
        return err;
    }
    
    *out = node;
    return 0;
}
```

#### 3.3.3 约束集合操作

```c
/**
 * 创建约束集合 - 映射 git_treebuilder
 */
int lv00_constraint_set_create(
    lv00_proof_repo_t *repo,
    lv00_constraint_set_t **out
);

/**
 * 向约束集合添加条目 - 映射 git_treebuilder_insert
 */
int lv00_constraint_set_add(
    lv00_constraint_set_t *set,
    const char *name,
    const lv00_oid_t *oid,
    uint16_t mode
);

/**
 * 写入约束集合 - 映射 git_treebuilder_write
 */
int lv00_constraint_set_write(
    lv00_proof_repo_t *repo,
    lv00_oid_t *out_oid,
    lv00_constraint_set_t *set
);

/* 实现示例 */
int lv00_constraint_set_add(
    lv00_constraint_set_t *set,
    const char *name,
    const lv00_oid_t *oid,
    uint16_t mode
) {
    lv00_constraint_entry_t *entry;
    
    /* 检查容量 */
    if (set->entry_count >= set->entry_capacity) {
        size_t new_capacity = set->entry_capacity ? set->entry_capacity * 2 : 8;
        lv00_constraint_entry_t *new_entries = realloc(
            set->entries,
            new_capacity * sizeof(*new_entries)
        );
        if (!new_entries) return LV00_ERROR_NOMEM;
        set->entries = new_entries;
        set->entry_capacity = new_capacity;
    }
    
    /* 添加条目 */
    entry = &set->entries[set->entry_count++];
    entry->oid = *oid;
    entry->mode = mode;
    entry->name = strdup(name);
    
    return 0;
}
```

#### 3.3.4 证明提交操作

```c
/**
 * 创建证明提交 - 映射 git_commit_create
 */
int lv00_proof_commit_create(
    lv00_proof_repo_t *repo,
    lv00_oid_t *out_oid,
    const char *update_ref,
    lv00_signature_t *author,
    lv00_signature_t *committer,
    const char *message,
    const lv00_oid_t *tree_oid,
    size_t parent_count,
    const lv00_oid_t *parent_oids[]
);

/* 实现示例 */
int lv00_proof_commit_create(
    lv00_proof_repo_t *repo,
    lv00_oid_t *out_oid,
    const char *update_ref,
    lv00_signature_t *author,
    lv00_signature_t *committer,
    const char *message,
    const lv00_oid_t *tree_oid,
    size_t parent_count,
    const lv00_oid_t *parent_oids[]
) {
    lv00_proof_commit_t *commit;
    int err;
    
    /* 创建提交对象 */
    commit = calloc(1, sizeof(*commit));
    if (!commit) return LV00_ERROR_NOMEM;
    
    commit->base.type = LV00_OBJ_PROOF;
    atomic_init(&commit->base.refcount, 1);
    
    /* 设置父提交 */
    if (parent_count > 0) {
        commit->parent_oids = malloc(parent_count * sizeof(lv00_oid_t));
        if (!commit->parent_oids) {
            free(commit);
            return LV00_ERROR_NOMEM;
        }
        memcpy(commit->parent_oids, parent_oids, parent_count * sizeof(lv00_oid_t));
        commit->parent_count = parent_count;
    }
    
    /* 设置树 */
    commit->tree_oid = *tree_oid;
    
    /* 设置签名 */
    commit->author = lv00_signature_dup(author);
    commit->committer = lv00_signature_dup(committer);
    
    /* 设置消息 */
    commit->message = strdup(message);
    
    /* 设置时间戳 */
    commit->time = committer->when;
    commit->offset = committer->offset;
    
    /* 序列化并写入 */
    uint8_t *serialized;
    size_t serialized_size;
    err = lv00_commit_serialize(commit, &serialized, &serialized_size);
    if (err < 0) goto cleanup;
    
    err = lv00_object_write(repo, out_oid, serialized, serialized_size, LV00_OBJ_PROOF);
    free(serialized);
    if (err < 0) goto cleanup;
    
    commit->base.oid = *out_oid;
    
    /* 更新引用 */
    if (update_ref) {
        err = lv00_refdb_update(repo->refdb, update_ref, out_oid);
        if (err < 0) goto cleanup;
    }
    
cleanup:
    lv00_proof_commit_free(commit);
    return err;
}
```

#### 3.3.5 差异检测

```c
/**
 * 约束集合差异选项 - 映射 git_diff_options
 */
typedef struct {
    uint32_t flags;
    uint16_t context_lines;
    uint16_t interhunk_lines;
    const char *old_prefix;
    const char *new_prefix;
} lv00_diff_options_t;

/**
 * 差异回调 - 映射 git_diff_cb
 */
typedef int (*lv00_diff_callback)(
    const lv00_diff_delta_t *delta,
    const lv00_diff_hunk_t *hunk,
    const lv00_diff_line_t *line,
    void *payload
);

/**
 * 差异检测 - 映射 git_diff_tree_to_tree
 */
int lv00_diff_constraint_sets(
    lv00_diff_t **out,
    lv00_proof_repo_t *repo,
    lv00_constraint_set_t *old_set,
    lv00_constraint_set_t *new_set,
    const lv00_diff_options_t *opts
);

/* 实现示例 */
int lv00_diff_constraint_sets(
    lv00_diff_t **out,
    lv00_proof_repo_t *repo,
    lv00_constraint_set_t *old_set,
    lv00_constraint_set_t *new_set,
    const lv00_diff_options_t *opts
) {
    lv00_diff_t *diff;
    size_t i, j;
    
    diff = calloc(1, sizeof(*diff));
    if (!diff) return LV00_ERROR_NOMEM;
    
    /* 构建旧集合的哈希表 */
    hash_table_t *old_table = hash_table_create();
    for (i = 0; i < old_set->entry_count; i++) {
        hash_table_insert(old_table, old_set->entries[i].name, &old_set->entries[i]);
    }
    
    /* 遍历新集合，检测变更 */
    for (i = 0; i < new_set->entry_count; i++) {
        lv00_constraint_entry_t *new_entry = &new_set->entries[i];
        lv00_constraint_entry_t *old_entry = hash_table_lookup(old_table, new_entry->name);
        
        if (!old_entry) {
            /* 新增约束 */
            lv00_diff_add_delta(diff, LV00_DELTA_ADDED, NULL, new_entry);
        } else if (memcmp(&old_entry->oid, &new_entry->oid, sizeof(lv00_oid_t)) != 0) {
            /* 修改约束 */
            lv00_diff_add_delta(diff, LV00_DELTA_MODIFIED, old_entry, new_entry);
        }
        /* 标记已处理 */
        if (old_entry) old_entry->processed = 1;
    }
    
    /* 检测删除的约束 */
    for (i = 0; i < old_set->entry_count; i++) {
        if (!old_set->entries[i].processed) {
            lv00_diff_add_delta(diff, LV00_DELTA_DELETED, &old_set->entries[i], NULL);
        }
    }
    
    hash_table_free(old_table);
    
    *out = diff;
    return 0;
}
```

### 3.4 与 Lv-00 现有架构的集成

#### 3.4.1 第 2 层约束图集成

```c
/**
 * 约束图的版本控制扩展
 * 在原有 constraint_graph.h 基础上添加版本控制支持
 */

/* 约束图版本控制上下文 */
typedef struct {
    lv00_proof_repo_t *repo;
    lv00_index_t *index;
    lv00_oid_t current_commit;
} lv00_cgraph_vc_ctx_t;

/**
 * 为约束图创建版本控制上下文
 */
int lv00_cgraph_vc_init(lv00_cgraph_vc_ctx_t **ctx, const char *repo_path);

/**
 * 保存当前约束图状态
 */
int lv00_cgraph_vc_snapshot(
    lv00_cgraph_vc_ctx_t *ctx,
    lv00_constraint_graph_t *graph,
    const char *message
);

/**
 * 恢复到指定版本的约束图
 */
int lv00_cgraph_vc_restore(
    lv00_cgraph_vc_ctx_t *ctx,
    lv00_constraint_graph_t **graph,
    const lv00_oid_t *commit_oid
);

/**
 * 获取约束图历史列表
 */
int lv00_cgraph_vc_history(
    lv00_cgraph_vc_ctx_t *ctx,
    lv00_commit_list_t **history,
    int max_count
);
```

#### 3.4.2 第 6 层数据交换集成

```c
/**
 * 证明导出的版本控制支持
 * 在原有 proof.h 基础上添加版本管理
 */

/**
 * 导出证明到仓库
 */
int lv00_proof_export_to_repo(
    lv00_proof_t *proof,
    lv00_proof_repo_t *repo,
    const char *branch_name,
    const char *message
);

/**
 * 从仓库导入证明
 */
int lv00_proof_import_from_repo(
    lv00_proof_t **proof,
    lv00_proof_repo_t *repo,
    const lv00_oid_t *commit_oid
);

/**
 * 序列化证明为 Git 兼容格式
 */
int lv00_proof_serialize_git(
    lv00_proof_t *proof,
    uint8_t **out_data,
    size_t *out_size
);

/**
 * 反序列化 Git 格式证明
 */
int lv00_proof_deserialize_git(
    lv00_proof_t **proof,
    const uint8_t *data,
    size_t size
);
```

---

## 4. 实现路线图

### 4.1 阶段划分总览

| 阶段 | 时间周期 | 核心目标 | 主要交付物 |
|-----|---------|---------|-----------|
| 短期 | 1-2 个月 | 基础框架搭建 | 对象数据库、引用系统 |
| 中期 | 3-6 个月 | 核心功能实现 | 完整版本控制 API |
| 长期 | 6-12 个月 | 生态完善 | 多后端支持、工具链 |

### 4.2 短期目标（1-2 个月）

#### 4.2.1 第 1 月：对象存储系统

| 任务项 | 优先级 | 工作量 | 验收标准 |
|-------|-------|-------|---------|
| 设计对象 ID 系统（SHA-256） | 高 | 3 天 | 实现 lv00_oid_t 及相关操作 |
| 实现内存对象数据库 | 高 | 5 天 | 支持对象的增删改查 |
| 实现文件系统对象数据库 | 高 | 5 天 | 支持松散对象和打包存储 |
| 对象序列化/反序列化 | 中 | 4 天 | 支持所有证明对象类型 |
| 单元测试 | 高 | 3 天 | 覆盖率 > 80% |

#### 4.2.2 第 2 月：引用管理系统

| 任务项 | 优先级 | 工作量 | 验收标准 |
|-------|-------|-------|---------|
| 设计引用数据结构 | 高 | 3 天 | 支持符号引用和直接引用 |
| 实现引用数据库 | 高 | 5 天 | 支持分支和标签 CRUD |
| 实现 HEAD 管理 | 高 | 3 天 | 支持分离 HEAD 和符号引用 |
| 引用事务支持 | 中 | 4 天 | 支持原子性引用更新 |
| 与第 2 层约束图集成 | 高 | 5 天 | 约束图可保存/恢复 |

### 4.3 中期目标（3-6 个月）

#### 4.3.1 第 3-4 月：核心版本控制功能

| 任务项 | 优先级 | 工作量 | 验收标准 |
|-------|-------|-------|---------|
| 实现索引（暂存区） | 高 | 2 周 | 支持约束变更的暂存 |
| 实现提交创建 | 高 | 2 周 | 支持完整提交历史 |
| 实现分支管理 | 高 | 1 周 | 创建/切换/删除分支 |
| 实现差异检测 | 中 | 2 周 | 约束集合差异比较 |
| 实现合并基础 | 中 | 1 周 | 查找共同祖先 |

#### 4.3.2 第 5-6 月：高级功能与集成

| 任务项 | 优先级 | 工作量 | 验收标准 |
|-------|-------|-------|---------|
| 实现三方合并 | 中 | 2 周 | 支持约束图合并 |
| 实现变基操作 | 低 | 2 周 | 证明历史重写 |
| 第 6 层导出集成 | 高 | 2 周 | 证明可导出为 Git 格式 |
| 压缩和优化 | 中 | 1 周 | 打包文件支持 |
| 性能基准测试 | 中 | 1 周 | 建立性能基线 |

### 4.4 长期目标（6-12 个月）

#### 4.4.1 第 7-9 月：多后端支持

| 任务项 | 优先级 | 工作量 | 验收标准 |
|-------|-------|-------|---------|
| 设计存储后端接口 | 高 | 2 周 | 抽象 ODB 和 RefDB 接口 |
| SQLite 后端实现 | 中 | 3 周 | 支持数据库存储 |
| Redis 后端实现 | 低 | 3 周 | 支持分布式缓存 |
| 网络同步协议 | 中 | 4 周 | 支持远程仓库操作 |

#### 4.4.2 第 10-12 月：工具链与生态

| 任务项 | 优先级 | 工作量 | 验收标准 |
|-------|-------|-------|---------|
| CLI 工具开发 | 中 | 4 周 | 类似 git 的命令行工具 |
| Python 绑定 | 中 | 3 周 | 支持 Python DSL 调用 |
| Web GUI 集成 | 低 | 4 周 | 第 7 层 Web 界面支持 |
| 文档和示例 | 高 | 3 周 | 完整 API 文档和教程 |

### 4.5 风险与缓解策略

| 风险项 | 影响程度 | 可能性 | 缓解策略 |
|-------|---------|-------|---------|
| SHA-256 性能瓶颈 | 中 | 中 | 实现哈希缓存，考虑硬件加速 |
| 存储空间膨胀 | 高 | 高 | 实现增量存储和垃圾回收 |
| 并发访问冲突 | 高 | 中 | 实现文件锁和事务机制 |
| 向后兼容性 | 中 | 低 | 设计版本化的存储格式 |

---

## 5. 附录

### 5.1 关键 API 列表

#### 5.1.1 仓库管理 API

| API 名称 | 功能描述 | 对应 libgit2 API |
|---------|---------|-----------------|
| `lv00_proof_repo_init` | 初始化新仓库 | `git_repository_init` |
| `lv00_proof_repo_open` | 打开现有仓库 | `git_repository_open` |
| `lv00_proof_repo_open_bare` | 打开裸仓库 | `git_repository_open_bare` |
| `lv00_proof_repo_discover` | 自动发现仓库 | `git_repository_discover` |
| `lv00_proof_repo_free` | 释放仓库句柄 | `git_repository_free` |
| `lv00_proof_repo_path` | 获取仓库路径 | `git_repository_path` |
| `lv00_proof_repo_workdir` | 获取工作目录 | `git_repository_workdir` |
| `lv00_proof_repo_is_bare` | 检查是否裸仓库 | `git_repository_is_bare` |
| `lv00_proof_repo_is_empty` | 检查是否空仓库 | `git_repository_is_empty` |

#### 5.1.2 对象数据库 API

| API 名称 | 功能描述 | 对应 libgit2 API |
|---------|---------|-----------------|
| `lv00_odb_init` | 初始化对象数据库 | `git_odb_new` |
| `lv00_odb_free` | 释放对象数据库 | `git_odb_free` |
| `lv00_odb_read` | 读取对象 | `git_odb_read` |
| `lv00_odb_write` | 写入对象 | `git_odb_write` |
| `lv00_odb_exists` | 检查对象存在 | `git_odb_exists` |
| `lv00_odb_hash` | 计算对象哈希 | `git_odb_hash` |
| `lv00_odb_foreach` | 遍历对象 | `git_odb_foreach` |

#### 5.1.3 引用管理 API

| API 名称 | 功能描述 | 对应 libgit2 API |
|---------|---------|-----------------|
| `lv00_refdb_init` | 初始化引用数据库 | `git_refdb_new` |
| `lv00_refdb_free` | 释放引用数据库 | `git_refdb_free` |
| `lv00_refdb_lookup` | 查找引用 | `git_reference_lookup` |
| `lv00_refdb_update` | 更新引用 | `git_reference_create` |
| `lv00_refdb_delete` | 删除引用 | `git_reference_delete` |
| `lv00_refdb_foreach` | 遍历引用 | `git_reference_foreach` |
| `lv00_refdb_compress` | 压缩引用 | `git_refdb_compress` |

#### 5.1.4 索引操作 API

| API 名称 | 功能描述 | 对应 libgit2 API |
|---------|---------|-----------------|
| `lv00_index_init` | 初始化索引 | `git_index_new` |
| `lv00_index_free` | 释放索引 | `git_index_free` |
| `lv00_index_clear` | 清空索引 | `git_index_clear` |
| `lv00_index_add` | 添加条目 | `git_index_add` |
| `lv00_index_remove` | 移除条目 | `git_index_remove` |
| `lv00_index_find` | 查找条目 | `git_index_find` |
| `lv00_index_write` | 写入索引 | `git_index_write` |
| `lv00_index_read` | 读取索引 | `git_index_read` |

#### 5.1.5 对象操作 API

| API 名称 | 功能描述 | 对应 libgit2 API |
|---------|---------|-----------------|
| `lv00_object_lookup` | 查找对象 | `git_object_lookup` |
| `lv00_object_lookup_prefix` | 前缀查找 | `git_object_lookup_prefix` |
| `lv00_object_free` | 释放对象 | `git_object_free` |
| `lv00_object_type` | 获取对象类型 | `git_object_type` |
| `lv00_object_id` | 获取对象 ID | `git_object_id` |
| `lv00_object_owner` | 获取所属仓库 | `git_object_owner` |

#### 5.1.6 表达式节点 API（映射 Blob）

| API 名称 | 功能描述 | 对应 libgit2 API |
|---------|---------|-----------------|
| `lv00_expr_lookup` | 查找表达式 | `git_blob_lookup` |
| `lv00_expr_create` | 创建表达式 | `git_blob_create_from_buffer` |
| `lv00_expr_rawcontent` | 获取原始内容 | `git_blob_rawcontent` |
| `lv00_expr_rawsize` | 获取内容大小 | `git_blob_rawsize` |
| `lv00_expr_free` | 释放表达式 | `git_blob_free` |

#### 5.1.7 约束集合 API（映射 Tree）

| API 名称 | 功能描述 | 对应 libgit2 API |
|---------|---------|-----------------|
| `lv00_cset_lookup` | 查找约束集合 | `git_tree_lookup` |
| `lv00_cset_create` | 创建约束集合 | `git_treebuilder_new` |
| `lv00_cset_add` | 添加约束条目 | `git_treebuilder_insert` |
| `lv00_cset_remove` | 移除约束条目 | `git_treebuilder_remove` |
| `lv00_cset_write` | 写入约束集合 | `git_treebuilder_write` |
| `lv00_cset_entrycount` | 获取条目数量 | `git_tree_entrycount` |
| `lv00_cset_free` | 释放约束集合 | `git_tree_free` |

#### 5.1.8 证明提交 API（映射 Commit）

| API 名称 | 功能描述 | 对应 libgit2 API |
|---------|---------|-----------------|
| `lv00_commit_lookup` | 查找提交 | `git_commit_lookup` |
| `lv00_commit_create` | 创建提交 | `git_commit_create` |
| `lv00_commit_parentcount` | 获取父提交数 | `git_commit_parentcount` |
| `lv00_commit_parent` | 获取父提交 | `git_commit_parent` |
| `lv00_commit_tree` | 获取根约束集合 | `git_commit_tree` |
| `lv00_commit_message` | 获取提交消息 | `git_commit_message` |
| `lv00_commit_author` | 获取作者 | `git_commit_author` |
| `lv00_commit_time` | 获取时间戳 | `git_commit_time` |
| `lv00_commit_free` | 释放提交 | `git_commit_free` |

#### 5.1.9 检查点标签 API（映射 Tag）

| API 名称 | 功能描述 | 对应 libgit2 API |
|---------|---------|-----------------|
| `lv00_checkpoint_lookup` | 查找检查点 | `git_tag_lookup` |
| `lv00_checkpoint_create` | 创建检查点 | `git_tag_create` |
| `lv00_checkpoint_delete` | 删除检查点 | `git_tag_delete` |
| `lv00_checkpoint_target` | 获取目标对象 | `git_tag_target` |
| `lv00_checkpoint_name` | 获取检查点名称 | `git_tag_name` |
| `lv00_checkpoint_free` | 释放检查点 | `git_tag_free` |

#### 5.1.10 差异检测 API

| API 名称 | 功能描述 | 对应 libgit2 API |
|---------|---------|-----------------|
| `lv00_diff_cset_to_cset` | 约束集合差异 | `git_diff_tree_to_tree` |
| `lv00_diff_cset_to_index` | 约束集合与索引差异 | `git_diff_tree_to_index` |
| `lv00_diff_index_to_workdir` | 索引与工作区差异 | `git_diff_index_to_workdir` |
| `lv00_diff_free` | 释放差异 | `git_diff_free` |
| `lv00_diff_foreach` | 遍历差异 | `git_diff_foreach` |
| `lv00_diff_print` | 打印差异 | `git_diff_print` |

#### 5.1.11 分支管理 API

| API 名称 | 功能描述 | 对应 libgit2 API |
|---------|---------|-----------------|
| `lv00_branch_create` | 创建分支 | `git_branch_create` |
| `lv00_branch_delete` | 删除分支 | `git_branch_delete` |
| `lv00_branch_lookup` | 查找分支 | `git_branch_lookup` |
| `lv00_branch_name` | 获取分支名称 | `git_branch_name` |
| `lv00_branch_upstream` | 获取上游分支 | `git_branch_upstream` |
| `lv00_branch_set_upstream` | 设置上游分支 | `git_branch_set_upstream` |
| `lv00_branch_foreach` | 遍历分支 | `git_branch_foreach` |

#### 5.1.12 版本解析 API

| API 名称 | 功能描述 | 对应 libgit2 API |
|---------|---------|-----------------|
| `lv00_revparse` | 解析版本引用 | `git_revparse` |
| `lv00_revparse_single` | 解析单个对象 | `git_revparse_single` |
| `lv00_revparse_ext` | 扩展解析 | `git_revparse_ext` |
| `lv00_revwalk_new` | 创建版本遍历器 | `git_revwalk_new` |
| `lv00_revwalk_push` | 添加起始提交 | `git_revwalk_push` |
| `lv00_revwalk_next` | 获取下一个提交 | `git_revwalk_next` |
| `lv00_revwalk_free` | 释放遍历器 | `git_revwalk_free` |

### 5.2 参考文献

#### 5.2.1 官方文档

1. **libgit2 官方文档**
   - 网址：https://libgit2.org/docs/
   - 内容：API 参考、使用指南、示例代码

2. **libgit2 GitHub 仓库**
   - 网址：https://github.com/libgit2/libgit2
   - 内容：源代码、Issue 跟踪、Pull Request

3. **Git 内部原理**
   - 文档：https://git-scm.com/book/en/v2/Git-Internals-Git-Objects
   - 内容：Git 对象模型、存储机制

#### 5.2.2 学术论文

1. **Chacon, S., & Straub, B. (2014). Pro Git (2nd ed.). Apress.**
   - 内容：Git 版本控制系统全面指南
   - 相关章节：第 10 章 Git 内部原理

2. **Spinellis, D. (2012). Git.**
   - IEEE Software, 29(3), 100-101.
   - 内容：Git 版本控制系统概述

#### 5.2.3 技术规范

1. **Git 对象格式规范**
   - 文档：https://github.com/git/git/blob/master/Documentation/technical/index-format.txt
   - 内容：Git 索引文件格式

2. **Git 包文件格式**
   - 文档：https://github.com/git/git/blob/master/Documentation/technical/pack-format.txt
   - 内容：Git 打包存储格式

#### 5.2.4 相关项目

1. **libgit2sharp**
   - 网址：https://github.com/libgit2/libgit2sharp
   - 内容：C# 语言绑定，参考其面向对象封装设计

2. **rugged**
   - 网址：https://github.com/libgit2/rugged
   - 内容：Ruby 语言绑定，参考其高级 API 设计

3. **git2-rs**
   - 网址：https://github.com/rust-lang/git2-rs
   - 内容：Rust 语言绑定，参考其内存安全封装

#### 5.2.5 Lv-00 内部文档

1. **Lv-00 架构设计文档**
   - 路径：`docs/ARCHITECTURE_v3.3.md`
   - 内容：7 层架构详细说明

2. **Lv-00 约束图设计**
   - 路径：`docs/02_constraint_graph.md`
   - 内容：第 2 层约束图数据结构

3. **Lv-00 证明引擎设计**
   - 路径：`docs/09_proof.md`
   - 内容：第 4 层证明引擎架构

4. **Lv-00 数据交换设计**
   - 路径：`docs/API_USAGE_GUIDE.md`
   - 内容：第 6 层序列化和导出机制

---

## 文档信息

- **文档版本**：1.0
- **创建日期**：2026-05-25
- **作者**：Lv-00 开发团队
- **审核状态**：待审核
- **关联项目**：libgit2 v1.8+

---

*本文档为 Lv-00 项目内部参考文档，仅供开发团队使用。*
