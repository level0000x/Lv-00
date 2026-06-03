/**
 * @file proof_version.h
 * @brief Proof version control system
 *
 * @details Provides a simple file-based version control system for proof
 *          artifacts. Uses SHA-256 hashing for content addressing and commit
 *          identification. Inspired by libgit2's API design, but implemented
 *          without external dependencies.
 *
 *          Features:
 *          - Initialize and open proof repositories
 *          - Create commits with SHA-256 content hashing
 *          - View commit log (history)
 *          - Diff between commits
 *          - Branch management (create, list, checkout)
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-25
 */

#ifndef LV00_PROOF_VERSION_H
#define LV00_PROOF_VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv00.h"

/* ============================================================
 * Constants
 * ============================================================ */

/** Maximum length of a commit OID (SHA-256 hex string) */
#define LV00_OID_LENGTH 65

/** Maximum length of a commit message */
#define LV00_COMMIT_MSG_MAX 512

/** Maximum number of branches per repository */
#define LV00_MAX_BRANCHES 64

/** Maximum length of a branch name */
#define LV00_BRANCH_NAME_MAX 128

/** Maximum number of commits in a log query */
#define LV00_LOG_MAX_ENTRIES 256

/* ============================================================
 * Proof commit
 * ============================================================ */

/**
 * @brief A single commit in the proof version control system.
 *
 * Each commit has a unique SHA-256 OID, a message, a parent OID,
 * and a timestamp.
 */
typedef struct Lv00ProofCommit {
    char     oid[LV00_OID_LENGTH];          /**< SHA-256 hash (hex, null-terminated) */
    char     message[LV00_COMMIT_MSG_MAX];  /**< Commit message */
    char     parent_oid[LV00_OID_LENGTH];   /**< Parent commit OID (empty string for root) */
    int64_t  timestamp;                     /**< Unix timestamp (seconds since epoch) */
} Lv00ProofCommit;

/* ============================================================
 * Proof diff entry
 * ============================================================ */

/**
 * @brief A single diff entry between two commits.
 */
typedef struct Lv00ProofDiffEntry {
    char path[256];           /**< File path that changed */
    char old_hash[LV00_OID_LENGTH]; /**< SHA-256 of the old content */
    char new_hash[LV00_OID_LENGTH]; /**< SHA-256 of the new content */
    int  change_type;         /**< 0 = added, 1 = modified, 2 = deleted */
} Lv00ProofDiffEntry;

/* ============================================================
 * Proof diff result
 * ============================================================ */

/**
 * @brief Result of a diff operation between two commits.
 */
typedef struct Lv00ProofDiff {
    Lv00ProofDiffEntry *entries;   /**< Array of diff entries */
    size_t              count;     /**< Number of entries */
} Lv00ProofDiff;

/* ============================================================
 * Proof repository
 * ============================================================ */

/**
 * @brief A proof version control repository.
 *
 * Stores commits, branches, and tracked files in a simple directory
 * structure under the repository path.
 */
typedef struct Lv00ProofRepo {
    char     path[512];                  /**< Repository root path */
    char     head_commit[LV00_OID_LENGTH]; /**< OID of the current HEAD commit */
    int      branch_count;               /**< Number of branches */
    char     branches[LV00_MAX_BRANCHES][LV00_BRANCH_NAME_MAX]; /**< Branch names */
    char     branch_heads[LV00_MAX_BRANCHES][LV00_OID_LENGTH];  /**< Branch head OIDs */
} Lv00ProofRepo;

/* ============================================================
 * API: Repository lifecycle
 * ============================================================ */

/**
 * @brief Initialize a new proof repository at the given path.
 *
 * Creates the repository directory structure and an initial empty commit.
 *
 * @param path  Directory path for the new repository
 * @return Pointer to the new repository, or NULL on failure
 */
LV00_PUBLIC_API Lv00ProofRepo *proof_repo_init(const char *path);

/**
 * @brief Open an existing proof repository.
 *
 * @param path  Directory path of the existing repository
 * @return Pointer to the opened repository, or NULL on failure
 */
LV00_PUBLIC_API Lv00ProofRepo *proof_repo_open(const char *path);

/**
 * @brief Destroy a proof repository object and free all resources.
 *
 * Does not delete the repository on disk.
 *
 * @param repo  The repository to destroy (may be NULL)
 */
LV00_PUBLIC_API void proof_repo_destroy(Lv00ProofRepo *repo);

/* ============================================================
 * API: Commits
 * ============================================================ */

/**
 * @brief Create a new commit in the repository.
 *
 * Computes SHA-256 hashes of all tracked files and creates a commit
 * object with the given message.
 *
 * @param repo     The repository
 * @param message  Commit message
 * @param files    Array of file paths to track
 * @param contents Array of file contents (parallel to files)
 * @param file_count Number of files
 * @return true on success, false on failure
 */
LV00_PUBLIC_API bool proof_repo_commit(Lv00ProofRepo *repo,
    const char *message,
    const char **files, const char **contents, size_t file_count);

/* ============================================================
 * API: History
 * ============================================================ */

/**
 * @brief Get the commit log (history) from HEAD.
 *
 * Returns an array of commits from newest to oldest.
 *
 * @param repo    The repository
 * @param commits Output array of commits (caller-allocated, at least LV00_LOG_MAX_ENTRIES)
 * @param max_count Maximum number of commits to return
 * @return Number of commits returned, 0 on failure
 */
LV00_PUBLIC_API size_t proof_repo_log(Lv00ProofRepo *repo,
    Lv00ProofCommit *commits, size_t max_count);

/* ============================================================
 * API: Diff
 * ============================================================ */

/**
 * @brief Compute the diff between two commits.
 *
 * @param repo       The repository
 * @param oid_a      OID of the first commit (NULL for empty tree)
 * @param oid_b      OID of the second commit (NULL for HEAD)
 * @param diff       Output diff structure (caller must free via proof_repo_diff_destroy)
 * @return true on success, false on failure
 */
LV00_PUBLIC_API bool proof_repo_diff(Lv00ProofRepo *repo,
    const char *oid_a, const char *oid_b, Lv00ProofDiff *diff);

/**
 * @brief Free resources held by a diff result.
 *
 * @param diff  The diff to free (may be NULL)
 */
LV00_PUBLIC_API void proof_repo_diff_destroy(Lv00ProofDiff *diff);

/* ============================================================
 * API: Branches
 * ============================================================ */

/**
 * @brief Create a new branch pointing at the current HEAD.
 *
 * @param repo      The repository
 * @param name      Branch name
 * @return true on success, false on failure
 */
LV00_PUBLIC_API bool proof_repo_branch(Lv00ProofRepo *repo, const char *name);

/**
 * @brief Checkout a branch, updating HEAD to the branch's commit.
 *
 * @param repo      The repository
 * @param name      Branch name
 * @return true on success, false on failure
 */
LV00_PUBLIC_API bool proof_repo_checkout(Lv00ProofRepo *repo, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PROOF_VERSION_H */
