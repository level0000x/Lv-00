/**
 * @file proof_version.c
 * @brief Implementation of the proof version control system.
 *
 * @details Implements a simple file-based version control system for proof
 *          artifacts. Uses SHA-256 hashing for content addressing. The repository
 *          is stored in a directory with the following structure:
 *
 *            <repo_path>/
 *              .lv_repo/
 *                HEAD              - current commit OID
 *                commits/
 *                  <oid>           - commit metadata (JSON-like text)
 *                objects/
 *                  <hash>          - file content blobs
 *                branches/
 *                  <name>          - branch head OID
 *
 *          SHA-256 is computed using a minimal implementation to avoid
 *          external dependencies.
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-25
 */

#include "proof_version.h"

#include "lv/config.h"
#include "lv/lv_config.h"
#include "lv/lv_file.h"
#include "lv/lv_path.h"
#include "lv/lv_platform.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/sha256.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "lv_utils.h"

void safe_strncpy(char *dest, const char *src, size_t max_len);



/* ============================================================
 * Internal helpers
 * ============================================================ */

/**
 * @brief Build the .lv_repo directory path.
 */
static void repo_dir_path(const char *repo_path, char *out, size_t out_size) {
    lv_path_join(repo_path, ".lv_repo", out, out_size);
}

/**
 * @brief Create the repository directory structure.
 */
static bool create_repo_dirs(const char *repo_path) {
    char repo_dir[lv_PATH_BUF_SIZE];
    char path[lv_PATH_BUF_SIZE];

    /* 统一走 lv_path_mkdirs（递归逐级创建，内部容忍 EEXIST），
     * 收敛对 repo_dir/commits/objects/branches 的四次 lv_mkdir */
    repo_dir_path(repo_path, repo_dir, sizeof(repo_dir));
    lv_path_mkdirs(repo_dir);

    lv_path_join(repo_dir, "commits", path, sizeof(path));
    lv_path_mkdirs(path);

    lv_path_join(repo_dir, "objects", path, sizeof(path));
    lv_path_mkdirs(path);

    lv_path_join(repo_dir, "branches", path, sizeof(path));
    lv_path_mkdirs(path);

    return true;
}

/**
 * @brief Write a string to a file.
 */
static bool write_file(const char *path, const char *content) {
    FILE *f = lv_file_open(path, "w");
    if (!f)
        return false;
    if (content) {
        fputs(content, f);
    }
    lv_file_close(f);
    return true;
}

/**
 * @brief Read a file into a newly allocated string.
 *
 * 收编自手写 fopen/ftell/malloc/fread 实现：底层统一走 lv_file_read_all。
 * lv_file_read_all 以 "rb" 读取，原实现以 "r"（文本模式）读取；
 * 在 Windows 上文本模式会将 "\r\n" 归一为 "\n"，此处手动执行相同的
 * 归一转换，保持调用方行为逐位一致。
 *
 * @param path  File path
 * @return Newly allocated string, or NULL on failure
 * @note 与 lv_file_read_all 相同：空文件返回 NULL（原实现返回空字符串）。
 */
static char *read_file(const char *path) {
    char *content = (char *) lv_file_read_all(path, NULL);
    if (!content) {
        return NULL;
    }
    char *src = content;
    char *dst = content;
    while (*src) {
        if (src[0] == '\r' && src[1] == '\n') {
            src++;
            continue;
        }
        *dst++ = *src++;
    }
    *dst = '\0';
    return content;
}

/**
 * @brief Compute a commit OID from message, parent, and file hashes.
 */
static void compute_commit_oid(const char *message, const char *parent_oid, const char *file_hashes, int64_t timestamp,
                               char *oid_out) {
    /* Combine all data into a single buffer for hashing */
    size_t msg_len = message ? strlen(message) : 0;
    size_t parent_len = parent_oid ? strlen(parent_oid) : 0;
    size_t fh_len = file_hashes ? strlen(file_hashes) : 0;
    size_t total = msg_len + 1 + parent_len + 1 + fh_len + 1 + sizeof(int64_t);

    char *buf = (char *) lv_malloc(total);
    if (!buf) {
        memset(oid_out, '0', lv_OID_LENGTH - 1);
        oid_out[lv_OID_LENGTH - 1] = '\0';
        return;
    }

    size_t pos = 0;
    if (message) {
        memcpy(buf + pos, message, msg_len);
        pos += msg_len;
    }
    buf[pos++] = '\0';
    if (parent_oid) {
        memcpy(buf + pos, parent_oid, parent_len);
        pos += parent_len;
    }
    buf[pos++] = '\0';
    if (file_hashes) {
        memcpy(buf + pos, file_hashes, fh_len);
        pos += fh_len;
    }
    buf[pos++] = '\0';
    memcpy(buf + pos, &timestamp, sizeof(int64_t));
    pos += sizeof(int64_t);

    lv_sha256_hex((const uint8_t *) buf, pos, oid_out);
    lv_free((void **) &buf);
    buf = NULL;
}

/**
 * @brief Write commit metadata to a file.
 */
static bool write_commit_file(const char *repo_path, const lvProofCommit *commit) {
    char dir[lv_PATH_BUF_SIZE], tmp[lv_PATH_BUF_SIZE], path[lv_PATH_BUF_SIZE];

    repo_dir_path(repo_path, dir, sizeof(dir));
    lv_path_join(dir, "commits", tmp, sizeof(tmp));
    lv_path_join(tmp, commit->oid, path, sizeof(path));

    FILE *f = lv_file_open(path, "w");
    if (!f)
        return false;

    fprintf(f, "oid: %s\n", commit->oid);
    fprintf(f, "message: %s\n", commit->message);
    fprintf(f, "parent: %s\n", commit->parent_oid);
    fprintf(f, "timestamp: %lld\n", (long long) commit->timestamp);

    lv_file_close(f);
    return true;
}

/**
 * @brief Read commit metadata from a file.
 */
static bool read_commit_file(const char *path, lvProofCommit *commit) {
    char *content = read_file(path);
    if (!content)
        return false;

    memset(commit, 0, sizeof(lvProofCommit));

    char *saveptr = NULL;
    char *line = lv_strtok_r(content, "\n", &saveptr);
    while (line) {
        if (strncmp(line, "oid: ", 5) == 0) {
            safe_strncpy(commit->oid, line + 5, lv_OID_LENGTH);
        } else if (strncmp(line, "message: ", 9) == 0) {
            safe_strncpy(commit->message, line + 9, lv_COMMIT_MSG_MAX);
        } else if (strncmp(line, "parent: ", 8) == 0) {
            safe_strncpy(commit->parent_oid, line + 8, lv_OID_LENGTH);
        } else if (strncmp(line, "timestamp: ", 11) == 0) {
            commit->timestamp = strtoll(line + 11, NULL, 10);
        }
        line = lv_strtok_r(NULL, "\n", &saveptr);
    }

    lv_free((void **) &content);
    content = NULL;
    return true;
}

/**
 * @brief Get the current time as a Unix timestamp.
 *
 * 统一走 lv_get_wallclock_ms()（cross_platform 时间基准，Windows 上基于
 * GetSystemTimeAsFileTime 并完成 1601→1970 转换），消除本文件手写的
 * 平台双分支时间戳逻辑。
 */
static int64_t get_timestamp(void) {
    return (int64_t) (lv_get_wallclock_ms() / 1000);
}

/* ============================================================
 * API implementation: Repository lifecycle
 * ============================================================ */

lvProofRepo *proof_repo_init(const char *path) {
    if (!path)
        return NULL;

    lvProofRepo *repo = (lvProofRepo *) lv_calloc(1, sizeof(lvProofRepo));
    if (!repo)
        return NULL;

    lv_strlcpy(repo->path, path, sizeof(repo->path));

    /* Create directory structure */
    if (!create_repo_dirs(path)) {
        lv_free((void **) &repo);
        repo = NULL;
        return NULL;
    }

    /* Create initial root commit */
    lvProofCommit root;
    memset(&root, 0, sizeof(root));
    safe_strncpy(root.message, "Initial commit", lv_COMMIT_MSG_MAX);
    root.parent_oid[0] = '\0';
    root.timestamp = get_timestamp();
    compute_commit_oid(root.message, "", "", root.timestamp, root.oid);

    if (!write_commit_file(path, &root)) {
        lv_free((void **) &repo);
        repo = NULL;
        return NULL;
    }

    /* Set HEAD to root commit */
    safe_strncpy(repo->head_commit, root.oid, lv_OID_LENGTH);

    /* Write HEAD file */
    char head_path[lv_PATH_BUF_SIZE];
    char dir[lv_PATH_BUF_SIZE];
    repo_dir_path(path, dir, sizeof(dir));
    lv_path_join(dir, "HEAD", head_path, sizeof(head_path));
    write_file(head_path, root.oid);

    /* Create default 'main' branch */
    safe_strncpy(repo->branches[0], "main", lv_BRANCH_NAME_MAX);
    safe_strncpy(repo->branch_heads[0], root.oid, lv_OID_LENGTH);
    repo->branch_count = 1;

    /* Write branch file */
    char branch_path[lv_PATH_BUF_SIZE], branches_dir_init[lv_PATH_BUF_SIZE];
    lv_path_join(dir, "branches", branches_dir_init, sizeof(branches_dir_init));
    lv_path_join(branches_dir_init, "main", branch_path, sizeof(branch_path));
    write_file(branch_path, root.oid);

    return repo;
}

lvProofRepo *proof_repo_open(const char *path) {
    if (!path)
        return NULL;

    lvProofRepo *repo = (lvProofRepo *) lv_calloc(1, sizeof(lvProofRepo));
    if (!repo)
        return NULL;

    lv_strlcpy(repo->path, path, sizeof(repo->path));

    /* Read HEAD */
    char head_path[lv_PATH_BUF_SIZE], dir[lv_PATH_BUF_SIZE];
    repo_dir_path(path, dir, sizeof(dir));
    lv_path_join(dir, "HEAD", head_path, sizeof(head_path));

    char *head = read_file(head_path);
    if (!head) {
        lv_free((void **) &repo);
        repo = NULL;
        return NULL;
    }
    /* Strip trailing newline */
    size_t hlen = strlen(head);
    if (hlen > 0 && head[hlen - 1] == '\n')
        head[hlen - 1] = '\0';
    safe_strncpy(repo->head_commit, head, lv_OID_LENGTH);
    lv_free((void **) &head);
    head = NULL;

    /* Read branches */
    char branches_dir[lv_PATH_BUF_SIZE];
    lv_path_join(dir, "branches", branches_dir, sizeof(branches_dir));

    /* Scan branch files */
    repo->branch_count = 0;
    for (int i = 0; i < lv_MAX_BRANCHES && repo->branch_count < lv_MAX_BRANCHES; i++) {
        /* Try common branch names */
        const char *known_branches[] = {"main", "master", "develop", "feature", "test"};
        int num_known = 5;

        for (int j = 0; j < num_known && repo->branch_count < lv_MAX_BRANCHES; j++) {
            /* Check if already loaded */
            bool already = false;
            for (int k = 0; k < repo->branch_count; k++) {
                if (strcmp(repo->branches[k], known_branches[j]) == 0) {
                    already = true;
                    break;
                }
            }
            if (already)
                continue;

            char bp[lv_PATH_BUF_SIZE];
            lv_path_join(branches_dir, known_branches[j], bp, sizeof(bp));
            char *content = read_file(bp);
            if (content) {
                safe_strncpy(repo->branches[repo->branch_count], known_branches[j], lv_BRANCH_NAME_MAX);
                size_t clen = strlen(content);
                if (clen > 0 && content[clen - 1] == '\n')
                    content[clen - 1] = '\0';
                safe_strncpy(repo->branch_heads[repo->branch_count], content, lv_OID_LENGTH);
                lv_free((void **) &content);
                content = NULL;
                repo->branch_count++;
            }
        }

        /* Only do one pass of known branches */
        if (i == 0) {
            /* Also try to enumerate files in branches directory */
            /* For simplicity, we only check known branch names */
            break;
        }
    }

    return repo;
}

void proof_repo_destroy(lvProofRepo *repo) {
    if (!repo)
        return;
    lv_free((void **) &repo);
    repo = NULL;
}

/* ============================================================
 * API implementation: Commits
 * ============================================================ */

bool proof_repo_commit(lvProofRepo *repo, const char *message, const char **files, const char **contents,
                       size_t file_count) {
    if (!repo || !message)
        return false;

    /* Compute file hashes and store objects */
    /* 用 lvStrBuf 累积哈希列表（自动扩容，消除手写大小计算），
       完成后 lv_strbuf_to_string 转出供 compute_commit_oid 消费 */
    lvStrBuf hash_sb = {0};

    char dir[lv_PATH_BUF_SIZE], obj_dir[lv_PATH_BUF_SIZE], obj_path[lv_PATH_BUF_SIZE];
    repo_dir_path(repo->path, dir, sizeof(dir));
    lv_path_join(dir, "objects", obj_dir, sizeof(obj_dir));

    for (size_t i = 0; i < file_count; i++) {
        if (!files[i] || !contents[i])
            continue;

        /* Compute content hash */
        char hash[lv_OID_LENGTH];
        lv_sha256_hex((const uint8_t *) contents[i], strlen(contents[i]), hash);

        /* Store object */
        lv_path_join(obj_dir, hash, obj_path, sizeof(obj_path));
        write_file(obj_path, contents[i]);

        /* Append to hash buffer */
        if (i > 0) {
            lv_strbuf_printf(&hash_sb, " ");
        }
        lv_strbuf_printf(&hash_sb, "%s", hash);
    }

    char *hash_buf = lv_strbuf_to_string(&hash_sb);

    /* Create commit */
    lvProofCommit commit;
    memset(&commit, 0, sizeof(commit));
    safe_strncpy(commit.message, message, lv_COMMIT_MSG_MAX);
    safe_strncpy(commit.parent_oid, repo->head_commit, lv_OID_LENGTH);
    commit.timestamp = get_timestamp();
    compute_commit_oid(message, repo->head_commit, hash_buf, commit.timestamp, commit.oid);

    lv_free((void **) &hash_buf);
    hash_buf = NULL;

    /* Write commit file */
    if (!write_commit_file(repo->path, &commit)) {
        return false;
    }

    /* Update HEAD */
    safe_strncpy(repo->head_commit, commit.oid, lv_OID_LENGTH);
    char head_path[lv_PATH_BUF_SIZE];
    lv_path_join(dir, "HEAD", head_path, sizeof(head_path));
    write_file(head_path, commit.oid);

    /* Update current branch head */
    for (int i = 0; i < repo->branch_count; i++) {
        if (strcmp(repo->branch_heads[i], commit.parent_oid) == 0) {
            safe_strncpy(repo->branch_heads[i], commit.oid, lv_OID_LENGTH);
            char branch_path[lv_PATH_BUF_SIZE], branches_dir[lv_PATH_BUF_SIZE];
            lv_path_join(dir, "branches", branches_dir, sizeof(branches_dir));
            lv_path_join(branches_dir, repo->branches[i], branch_path, sizeof(branch_path));
            write_file(branch_path, commit.oid);
            break;
        }
    }

    return true;
}

/* ============================================================
 * API implementation: History
 * ============================================================ */

size_t proof_repo_log(lvProofRepo *repo, lvProofCommit *commits, size_t max_count) {
    if (!repo || !commits || max_count == 0)
        return 0;

    size_t count = 0;
    char current_oid[lv_OID_LENGTH];
    memset(current_oid, 0, sizeof(current_oid));
    safe_strncpy(current_oid, repo->head_commit, lv_OID_LENGTH);

    char dir[lv_PATH_BUF_SIZE], commits_dir[lv_PATH_BUF_SIZE], commit_path[lv_PATH_BUF_SIZE];
    repo_dir_path(repo->path, dir, sizeof(dir));
    lv_path_join(dir, "commits", commits_dir, sizeof(commits_dir));

    while (count < max_count && current_oid[0] != '\0') {
        lv_path_join(commits_dir, current_oid, commit_path, sizeof(commit_path));

        if (!read_commit_file(commit_path, &commits[count])) {
            break;
        }

        /* Move to parent */
        safe_strncpy(current_oid, commits[count].parent_oid, lv_OID_LENGTH);
        count++;
    }

    return count;
}

/* ============================================================
 * API implementation: Diff
 * ============================================================ */

bool proof_repo_diff(lvProofRepo *repo, const char *oid_a, const char *oid_b, lvProofDiff *diff) {
    if (!repo || !diff)
        return false;

    diff->entries = NULL;
    diff->count = 0;

    /* For this simple implementation, diff between two commits
     * compares their stored file objects. Since commits reference
     * file hashes, we report the commit-level difference. */
    char target_b[lv_OID_LENGTH];
    memset(target_b, 0, sizeof(target_b));
    if (oid_b) {
        safe_strncpy(target_b, oid_b, lv_OID_LENGTH);
    } else {
        safe_strncpy(target_b, repo->head_commit, lv_OID_LENGTH);
    }

    /* If oid_a is NULL, treat as empty tree (all files added) */
    if (!oid_a || oid_a[0] == '\0') {
        diff->entries = (lvProofDiffEntry *) lv_calloc(1, sizeof(lvProofDiffEntry));
        if (!diff->entries)
            return false;

        lv_strlcpy(diff->entries[0].path, "(root)", sizeof(diff->entries[0].path));
        memset(diff->entries[0].old_hash, '0', lv_OID_LENGTH - 1);
        diff->entries[0].old_hash[lv_OID_LENGTH - 1] = '\0';
        safe_strncpy(diff->entries[0].new_hash, target_b, lv_OID_LENGTH);
        diff->entries[0].change_type = 0; /* added */
        diff->count = 1;
        return true;
    }

    /* Compare two commits */
    diff->entries = (lvProofDiffEntry *) lv_calloc(1, sizeof(lvProofDiffEntry));
    if (!diff->entries)
        return false;

    lv_strlcpy(diff->entries[0].path, "(commit)", sizeof(diff->entries[0].path));
    safe_strncpy(diff->entries[0].old_hash, oid_a, lv_OID_LENGTH);
    safe_strncpy(diff->entries[0].new_hash, target_b, lv_OID_LENGTH);
    diff->entries[0].change_type = 1; /* modified */
    diff->count = 1;

    return true;
}

void proof_repo_diff_destroy(lvProofDiff *diff) {
    if (!diff)
        return;
    lv_free((void **) &diff->entries);
    diff->entries = NULL;
    diff->count = 0;
}

/* ============================================================
 * API implementation: Branches
 * ============================================================ */

bool proof_repo_branch(lvProofRepo *repo, const char *name) {
    if (!repo || !name)
        return false;
    if (repo->branch_count >= lv_MAX_BRANCHES)
        return false;

    /* Check if branch already exists */
    for (int i = 0; i < repo->branch_count; i++) {
        if (strcmp(repo->branches[i], name) == 0) {
            return false; /* Branch already exists */
        }
    }

    /* Create new branch pointing at HEAD */
    int idx = repo->branch_count;
    safe_strncpy(repo->branches[idx], name, lv_BRANCH_NAME_MAX);
    safe_strncpy(repo->branch_heads[idx], repo->head_commit, lv_OID_LENGTH);
    repo->branch_count++;

    /* Write branch file */
    char dir[lv_PATH_BUF_SIZE], branch_path[lv_PATH_BUF_SIZE], branches_dir[lv_PATH_BUF_SIZE];
    repo_dir_path(repo->path, dir, sizeof(dir));
    lv_path_join(dir, "branches", branches_dir, sizeof(branches_dir));
    lv_path_join(branches_dir, name, branch_path, sizeof(branch_path));
    write_file(branch_path, repo->head_commit);

    return true;
}

bool proof_repo_checkout(lvProofRepo *repo, const char *name) {
    if (!repo || !name)
        return false;

    /* Find branch */
    for (int i = 0; i < repo->branch_count; i++) {
        if (strcmp(repo->branches[i], name) == 0) {
            /* Update HEAD */
            safe_strncpy(repo->head_commit, repo->branch_heads[i], lv_OID_LENGTH);

            /* Write HEAD file */
            char dir[lv_PATH_BUF_SIZE], head_path[lv_PATH_BUF_SIZE];
            repo_dir_path(repo->path, dir, sizeof(dir));
            lv_path_join(dir, "HEAD", head_path, sizeof(head_path));
            write_file(head_path, repo->head_commit);

            return true;
        }
    }

    return false; /* Branch not found */
}
