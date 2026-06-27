#ifndef LV00_PROOF_VERSION_INTERNAL_H
#define LV00_PROOF_VERSION_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* ============== 常量定义 ============== */

#ifndef LV00_OID_LENGTH
#define LV00_OID_LENGTH 64
#endif

#ifndef LV00_COMMIT_MSG_MAX
#define LV00_COMMIT_MSG_MAX 256
#endif

#ifndef LV00_BRANCH_NAME_MAX
#define LV00_BRANCH_NAME_MAX 64
#endif

#ifndef LV00_MAX_BRANCHES
#define LV00_MAX_BRANCHES 16
#endif

/* ============== 提交结构体 ============== */

typedef struct {
    char oid[LV00_OID_LENGTH];
    char message[LV00_COMMIT_MSG_MAX];
    char parent_oid[LV00_OID_LENGTH];
    int64_t timestamp;
} Lv00ProofCommit;

/* ============== 仓库结构体 ============== */

typedef struct {
    char path[256];
    char head_commit[LV00_OID_LENGTH];
    char branches[LV00_MAX_BRANCHES][LV00_BRANCH_NAME_MAX];
    char branch_heads[LV00_MAX_BRANCHES][LV00_OID_LENGTH];
    int branch_count;
} Lv00ProofRepo;

/* ============== Diff 条目结构体 ============== */

typedef struct {
    char path[256];
    char old_hash[LV00_OID_LENGTH];
    char new_hash[LV00_OID_LENGTH];
    int change_type;
} Lv00ProofDiffEntry;

/* ============== Diff 结构体 ============== */

typedef struct {
    Lv00ProofDiffEntry *entries;
    int count;
} Lv00ProofDiff;

/* ============== API 函数声明 ============== */

Lv00ProofRepo *proof_repo_init(const char *path);
Lv00ProofRepo *proof_repo_open(const char *path);
void proof_repo_destroy(Lv00ProofRepo *repo);
bool proof_repo_commit(Lv00ProofRepo *repo, const char *message,
    const char **files, const char **contents, size_t file_count);
bool proof_repo_diff(Lv00ProofRepo *repo, const char *oid_a, const char *oid_b,
    Lv00ProofDiff *diff);
void proof_repo_diff_destroy(Lv00ProofDiff *diff);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PROOF_VERSION_INTERNAL_H */
