/**
 * @file test_proof_version.c
 * @brief Tests for the proof version control system.
 *
 * @details Tests cover:
 *          - Repository initialization
 *          - Repository opening
 *          - Creating commits
 *          - Viewing commit log
 *          - Diffing between commits
 *          - Branch creation
 *          - Branch checkout
 *          - Repository destruction
 *          - NULL safety
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-25
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define TEST_MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define TEST_MKDIR(path) mkdir(path, 0755)
#endif

#include "lv.h"
#include "lv/lv_path.h"
#include "proof_version.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * Helper: ensure a directory exists before repo init
 * ============================================================ */

static void ensure_dir_exists(const char *path) {
    TEST_MKDIR(path);
}

/* ============================================================
 * Test: Repository initialization
 * ============================================================ */

static void test_repo_init(void) {
    ensure_dir_exists("test_repo_init");
    lvProofRepo *repo = proof_repo_init("test_repo_init");
    TEST_ASSERT_NOT_NULL(repo);

    /* HEAD should be set (non-empty after initial commit) */
    TEST_ASSERT(repo->head_commit[0] != '\0', "HEAD should be set after init");
    TEST_ASSERT(repo->branch_count >= 1, "should have at least one branch");

    proof_repo_destroy(repo);

    /* Cleanup test directory */
    lv_path_remove("test_repo_init");
}

/* ============================================================
 * Test: Repository open
 * ============================================================ */

static void test_repo_open(void) {
    /* First create a repo */
    ensure_dir_exists("test_repo_open");
    lvProofRepo *repo = proof_repo_init("test_repo_open");
    TEST_ASSERT_NOT_NULL(repo);
    char original_head[lv_OID_LENGTH];
    memset(original_head, 0, sizeof(original_head));
    lv_strlcpy(original_head, repo->head_commit, lv_OID_LENGTH);
    proof_repo_destroy(repo);

    /* Now open it */
    repo = proof_repo_open("test_repo_open");
    TEST_ASSERT_NOT_NULL(repo);
    TEST_ASSERT_STR_EQ(repo->head_commit, original_head);

    proof_repo_destroy(repo);

    lv_path_remove("test_repo_open");
}

/* ============================================================
 * Test: Create commits
 * ============================================================ */

static void test_repo_commit(void) {
    ensure_dir_exists("test_repo_commit");
    lvProofRepo *repo = proof_repo_init("test_repo_commit");
    TEST_ASSERT_NOT_NULL(repo);

    char old_head[lv_OID_LENGTH];
    memset(old_head, 0, sizeof(old_head));
    lv_strlcpy(old_head, repo->head_commit, lv_OID_LENGTH);

    const char *files[] = {"theorem1.lv"};
    const char *contents[] = {"forall x: P(x) -> Q(x)"};

    bool ok = proof_repo_commit(repo, "Add theorem 1", files, contents, 1);
    TEST_ASSERT(ok, "commit should succeed");

    /* HEAD should have changed */
    TEST_ASSERT(strcmp(repo->head_commit, old_head) != 0, "HEAD should change after commit");

    proof_repo_destroy(repo);

    lv_path_remove("test_repo_commit");
}

/* ============================================================
 * Test: Commit log
 * ============================================================ */

static void test_repo_log(void) {
    ensure_dir_exists("test_repo_log");
    lvProofRepo *repo = proof_repo_init("test_repo_log");
    TEST_ASSERT_NOT_NULL(repo);

    /* Create two more commits (initial + 2 = 3 total) */
    const char *files[] = {"proof1.lv"};
    const char *contents[] = {"proof of theorem 1"};

    proof_repo_commit(repo, "Add proof 1", files, contents, 1);
    proof_repo_commit(repo, "Add proof 2", files, contents, 1);

    lvProofCommit commits[lv_LOG_MAX_ENTRIES];
    size_t count = proof_repo_log(repo, commits, lv_LOG_MAX_ENTRIES);

    TEST_ASSERT_EQ(count, 3); /* initial + 2 commits */

    /* Most recent commit should be "Add proof 2" */
    TEST_ASSERT_STR_EQ(commits[0].message, "Add proof 2");

    /* First commit should be "Initial commit" */
    TEST_ASSERT_STR_EQ(commits[2].message, "Initial commit");

    /* Parent chain should be consistent */
    TEST_ASSERT_STR_EQ(commits[0].parent_oid, commits[1].oid);
    TEST_ASSERT_STR_EQ(commits[1].parent_oid, commits[2].oid);

    proof_repo_destroy(repo);

    lv_path_remove("test_repo_log");
}

/* ============================================================
 * Test: Diff between commits
 * ============================================================ */

static void test_repo_diff(void) {
    ensure_dir_exists("test_repo_diff");
    lvProofRepo *repo = proof_repo_init("test_repo_diff");
    TEST_ASSERT_NOT_NULL(repo);

    /* Get initial commit OID */
    char initial_oid[lv_OID_LENGTH];
    memset(initial_oid, 0, sizeof(initial_oid));
    lv_strlcpy(initial_oid, repo->head_commit, lv_OID_LENGTH);

    const char *files[] = {"lemma.lv"};
    const char *contents[] = {"lemma: A -> B"};

    proof_repo_commit(repo, "Add lemma", files, contents, 1);

    /* Diff from initial to HEAD */
    lvProofDiff diff;
    bool ok = proof_repo_diff(repo, initial_oid, NULL, &diff);
    TEST_ASSERT(ok, "diff should succeed");
    TEST_ASSERT_EQ(diff.count, 1);
    TEST_ASSERT_EQ(diff.entries[0].change_type, 1); /* modified */

    proof_repo_diff_destroy(&diff);

    /* Diff from NULL (empty tree) to HEAD */
    ok = proof_repo_diff(repo, NULL, NULL, &diff);
    TEST_ASSERT(ok, "diff from NULL should succeed");
    TEST_ASSERT_EQ(diff.count, 1);
    TEST_ASSERT_EQ(diff.entries[0].change_type, 0); /* added */

    proof_repo_diff_destroy(&diff);

    proof_repo_destroy(repo);

    lv_path_remove("test_repo_diff");
}

/* ============================================================
 * Test: Branch creation
 * ============================================================ */

static void test_repo_branch(void) {
    ensure_dir_exists("test_repo_branch");
    lvProofRepo *repo = proof_repo_init("test_repo_branch");
    TEST_ASSERT_NOT_NULL(repo);

    bool ok = proof_repo_branch(repo, "feature");
    TEST_ASSERT(ok, "branch creation should succeed");
    TEST_ASSERT_EQ(repo->branch_count, 2);
    TEST_ASSERT_STR_EQ(repo->branches[1], "feature");

    /* Branch should point to HEAD */
    TEST_ASSERT_STR_EQ(repo->branch_heads[1], repo->head_commit);

    /* Duplicate branch should fail */
    ok = proof_repo_branch(repo, "feature");
    TEST_ASSERT(!ok, "duplicate branch should fail");

    proof_repo_destroy(repo);

    lv_path_remove("test_repo_branch");
}

/* ============================================================
 * Test: Branch checkout
 * ============================================================ */

static void test_repo_checkout(void) {
    ensure_dir_exists("test_repo_checkout");
    lvProofRepo *repo = proof_repo_init("test_repo_checkout");
    TEST_ASSERT_NOT_NULL(repo);

    /* Create a branch before making new commits */
    proof_repo_branch(repo, "dev");

    /* Save the current HEAD (which dev branch points to) */
    char dev_head[lv_OID_LENGTH];
    memset(dev_head, 0, sizeof(dev_head));
    lv_strlcpy(dev_head, repo->head_commit, lv_OID_LENGTH);

    /* Make a new commit on main */
    const char *files[] = {"new.lv"};
    const char *contents[] = {"new content"};
    proof_repo_commit(repo, "New commit on main", files, contents, 1);

    /* HEAD should have advanced */
    TEST_ASSERT(strcmp(repo->head_commit, dev_head) != 0, "HEAD should have advanced after commit");

    /* Checkout dev branch */
    bool ok = proof_repo_checkout(repo, "dev");
    TEST_ASSERT(ok, "checkout should succeed");
    TEST_ASSERT_STR_EQ(repo->head_commit, dev_head);

    /* Checkout non-existent branch should fail */
    ok = proof_repo_checkout(repo, "nonexistent");
    TEST_ASSERT(!ok, "checkout of nonexistent branch should fail");

    proof_repo_destroy(repo);

    lv_path_remove("test_repo_checkout");
}

/* ============================================================
 * Test: NULL safety
 * ============================================================ */

static void test_null_safety(void) {
    /* NULL init */
    lvProofRepo *repo = proof_repo_init(NULL);
    TEST_ASSERT_NULL(repo);

    /* NULL open */
    repo = proof_repo_open(NULL);
    TEST_ASSERT_NULL(repo);

    /* NULL destroy (should not crash) */
    proof_repo_destroy(NULL);

    /* NULL commit */
    bool ok = proof_repo_commit(NULL, "msg", NULL, NULL, 0);
    TEST_ASSERT(!ok, "commit with NULL repo should fail");

    /* NULL log */
    size_t count = proof_repo_log(NULL, NULL, 0);
    TEST_ASSERT_EQ(count, 0);

    /* NULL diff */
    lvProofDiff diff;
    ok = proof_repo_diff(NULL, NULL, NULL, &diff);
    TEST_ASSERT(!ok, "diff with NULL repo should fail");

    /* NULL diff destroy */
    proof_repo_diff_destroy(NULL);

    /* NULL branch */
    ok = proof_repo_branch(NULL, "name");
    TEST_ASSERT(!ok, "branch with NULL repo should fail");

    /* NULL checkout */
    ok = proof_repo_checkout(NULL, "name");
    TEST_ASSERT(!ok, "checkout with NULL repo should fail");
}

/* ============================================================
 * Test: Multiple commits with different files
 * ============================================================ */

static void test_multiple_commits(void) {
    ensure_dir_exists("test_multi_commit");
    lvProofRepo *repo = proof_repo_init("test_multi_commit");
    TEST_ASSERT_NOT_NULL(repo);

    const char *files1[] = {"axiom1.lv", "axiom2.lv"};
    const char *contents1[] = {"A1: forall x. P(x)", "A2: exists y. Q(y)"};
    proof_repo_commit(repo, "Add axioms", files1, contents1, 2);

    const char *files2[] = {"theorem1.lv"};
    const char *contents2[] = {"T1: P(a) -> Q(a)"};
    proof_repo_commit(repo, "Add theorem", files2, contents2, 1);

    lvProofCommit commits[10];
    size_t count = proof_repo_log(repo, commits, 10);
    TEST_ASSERT_EQ(count, 3); /* initial + 2 */
    TEST_ASSERT_STR_EQ(commits[0].message, "Add theorem");
    TEST_ASSERT_STR_EQ(commits[1].message, "Add axioms");

    proof_repo_destroy(repo);

    lv_path_remove("test_multi_commit");
}

/* ============================================================
 * Main
 * ============================================================ */

TEST_MAIN_BEGIN("Proof Version Control")

    TEST_MAIN_RUN(test_repo_init);
    TEST_MAIN_RUN(test_repo_open);
    TEST_MAIN_RUN(test_repo_commit);
    TEST_MAIN_RUN(test_repo_log);
    TEST_MAIN_RUN(test_repo_diff);
    TEST_MAIN_RUN(test_repo_branch);
    TEST_MAIN_RUN(test_repo_checkout);
    TEST_MAIN_RUN(test_null_safety);
    TEST_MAIN_RUN(test_multiple_commits);


TEST_MAIN_END()
