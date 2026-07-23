#ifndef lv_PROOF_VERSION_INTERNAL_H
#define lv_PROOF_VERSION_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============== 常量定义 ============== */

#ifndef lv_OID_LENGTH
#define lv_OID_LENGTH 65 /* 64 hex chars + null terminator */
#endif

#ifndef lv_COMMIT_MSG_MAX
#define lv_COMMIT_MSG_MAX 256
#endif

#ifndef lv_BRANCH_NAME_MAX
#define lv_BRANCH_NAME_MAX 64
#endif

#ifndef lv_MAX_BRANCHES
#define lv_MAX_BRANCHES 16
#endif

#ifndef lv_LOG_MAX_ENTRIES
#define lv_LOG_MAX_ENTRIES 256
#endif

/* ============== 提交结构体 ============== */

typedef struct {
    char oid[lv_OID_LENGTH];
    char message[lv_COMMIT_MSG_MAX];
    char parent_oid[lv_OID_LENGTH];
    int64_t timestamp;
} lvProofCommit;

/* ============== 仓库结构体 ============== */

typedef struct {
    char path[256];
    char head_commit[lv_OID_LENGTH];
    char branches[lv_MAX_BRANCHES][lv_BRANCH_NAME_MAX];
    char branch_heads[lv_MAX_BRANCHES][lv_OID_LENGTH];
    int branch_count;
} lvProofRepo;

/* ============== Diff 条目结构体 ============== */

typedef struct {
    char path[256];
    char old_hash[lv_OID_LENGTH];
    char new_hash[lv_OID_LENGTH];
    int change_type;
} lvProofDiffEntry;

/* ============== Diff 结构体 ============== */

typedef struct {
    lvProofDiffEntry *entries;
    int count;
} lvProofDiff;

/* ============== API 函数声明 ============== */

/**
 * @brief 初始化证明版本仓库，在指定路径创建新的仓库结构
 *
 * @param path 仓库存储路径
 * @return 成功返回 lvProofRepo 指针，失败返回 NULL
 */
lvProofRepo *proof_repo_init(const char *path);
/**
 * @brief 打开已存在的证明版本仓库
 *
 * @param path 仓库存储路径
 * @return 成功返回 lvProofRepo 指针，失败返回 NULL
 */
lvProofRepo *proof_repo_open(const char *path);
/**
 * @brief 销毁证明版本仓库，释放所有占用的资源
 *
 * @param repo 仓库指针
 */
void proof_repo_destroy(lvProofRepo *repo);
/**
 * @brief 提交证明快照到仓库，记录当前文件状态并生成提交哈希
 *
 * @param repo 仓库指针
 * @param message 提交信息
 * @param files 文件路径数组
 * @param contents 文件内容数组，与 files 一一对应
 * @param file_count 文件数量
 * @return 提交成功返回 true，失败返回 false
 */
bool proof_repo_commit(lvProofRepo *repo, const char *message, const char **files, const char **contents,
                       size_t file_count);
/**
 * @brief 获取提交历史日志，按时间倒序返回提交记录
 *
 * @param repo 仓库指针
 * @param commits 输出缓冲区，用于存储提交记录数组
 * @param max_count 最多返回的提交记录数量
 * @return 实际返回的提交记录数量
 */
size_t proof_repo_log(lvProofRepo *repo, lvProofCommit *commits, size_t max_count);
/**
 * @brief 比较两个提交之间的差异，生成差异条目列表
 *
 * @param repo 仓库指针
 * @param oid_a 基准提交的 OID
 * @param oid_b 比较目标的 OID
 * @param diff 输出参数，存储差异结果
 * @return 比较成功返回 true，失败返回 false
 */
bool proof_repo_diff(lvProofRepo *repo, const char *oid_a, const char *oid_b, lvProofDiff *diff);
/**
 * @brief 销毁差异对象，释放差异条目占用的内存
 *
 * @param diff 差异对象指针
 */
void proof_repo_diff_destroy(lvProofDiff *diff);
/**
 * @brief 在仓库中创建新分支
 *
 * @param repo 仓库指针
 * @param name 分支名称
 * @return 创建成功返回 true，失败（如重名或容量已满）返回 false
 */
bool proof_repo_branch(lvProofRepo *repo, const char *name);
/**
 * @brief 切换到指定分支，更新 HEAD 到该分支的最新提交
 *
 * @param repo 仓库指针
 * @param name 目标分支名称
 * @return 切换成功返回 true，失败返回 false
 */
bool proof_repo_checkout(lvProofRepo *repo, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* lv_PROOF_VERSION_INTERNAL_H */
