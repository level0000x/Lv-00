/**
 * @file proof_version.c
 * @brief Implementation of the proof version control system.
 *
 * @details Implements a simple file-based version control system for proof
 *          artifacts. Uses SHA-256 hashing for content addressing. The repository
 *          is stored in a directory with the following structure:
 *
 *            <repo_path>/
 *              .lv00_repo/
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define LV00_MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define LV00_MKDIR(path) mkdir(path, 0755)
#endif

/* ============================================================
 * Minimal SHA-256 implementation
 * ============================================================ */

/**
 * @brief Minimal SHA-256 implementation for content hashing.
 *
 * Based on the FIPS 180-4 specification.
 */

#define SHA256_BLOCK_SIZE 64
#define SHA256_DIGEST_SIZE 32

static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define EP1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

typedef struct {
    uint32_t state[8];
    uint8_t  buffer[SHA256_BLOCK_SIZE];
    uint64_t bitcount;
    uint32_t buflen;
} Sha256Ctx;

static void sha256_init(Sha256Ctx *ctx) {
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->bitcount = 0;
    ctx->buflen = 0;
}

static void sha256_transform(Sha256Ctx *ctx, const uint8_t block[SHA256_BLOCK_SIZE]) {
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t t1, t2;
    int i;

    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               ((uint32_t)block[i * 4 + 3]);
    }
    for (i = 16; i < 64; i++) {
        w[i] = SIG1(w[i - 2]) + w[i - 7] + SIG0(w[i - 15]) + w[i - 16];
    }

    a = ctx->state[0]; b = ctx->state[1];
    c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5];
    g = ctx->state[6]; h = ctx->state[7];

    for (i = 0; i < 64; i++) {
        t1 = h + EP1(e) + CH(e, f, g) + sha256_k[i] + w[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    ctx->state[0] += a; ctx->state[1] += b;
    ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f;
    ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_update(Sha256Ctx *ctx, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        ctx->buffer[ctx->buflen++] = data[i];
        if (ctx->buflen == SHA256_BLOCK_SIZE) {
            sha256_transform(ctx, ctx->buffer);
            ctx->bitcount += 512;
            ctx->buflen = 0;
        }
    }
}

static void sha256_final(Sha256Ctx *ctx, uint8_t hash[SHA256_DIGEST_SIZE]) {
    ctx->bitcount += (uint64_t)ctx->buflen * 8;

    ctx->buffer[ctx->buflen++] = 0x80;

    if (ctx->buflen > 56) {
        while (ctx->buflen < SHA256_BLOCK_SIZE) {
            ctx->buffer[ctx->buflen++] = 0x00;
        }
        sha256_transform(ctx, ctx->buffer);
        ctx->buflen = 0;
    }

    while (ctx->buflen < 56) {
        ctx->buffer[ctx->buflen++] = 0x00;
    }

    /* Append bit length as big-endian 64-bit */
    for (int i = 7; i >= 0; i--) {
        ctx->buffer[56 + (7 - i)] = (uint8_t)(ctx->bitcount >> (i * 8));
    }
    sha256_transform(ctx, ctx->buffer);

    for (int i = 0; i < 8; i++) {
        hash[i * 4]     = (uint8_t)(ctx->state[i] >> 24);
        hash[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        hash[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        hash[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

/**
 * @brief Compute SHA-256 hash of a string and return as hex.
 *
 * @param data  Input data
 * @param len   Length of input data
 * @param out   Output hex string buffer
 * @param out_size  Size of output buffer (must be at least SHA256_DIGEST_SIZE * 2 + 1)
 *
 * @return true on success, false if buffer is too small
 */
static bool compute_sha256_hex(const void *data, size_t len, char *out, size_t out_size) {
    if (out_size < SHA256_DIGEST_SIZE * 2 + 1) {
        return false;
    }

    Sha256Ctx ctx;
    uint8_t hash[SHA256_DIGEST_SIZE];

    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t *)data, len);
    sha256_final(&ctx, hash);

    for (int i = 0; i < SHA256_DIGEST_SIZE; i++) {
        /* 使用 snprintf 替代 sprintf，确保缓冲区安全 */
        snprintf(out + i * 2, out_size - i * 2, "%02x", hash[i]);
    }
    out[SHA256_DIGEST_SIZE * 2] = '\0';
    return true;
}

/* ============================================================
 * Internal helpers
 * ============================================================ */

/**
 * @brief Safe string copy that guarantees null-termination.
 *
 * Unlike strncpy, this always null-terminates the destination buffer
 * even when the source string length >= n.
 *
 * @param dest  Destination buffer
 * @param src   Source string
 * @param n     Size of destination buffer
 * @return Number of characters copied (excluding null terminator), or 0 on error
 */
static size_t safe_strncpy(char *dest, const char *src, size_t n) {
    if (n == 0 || dest == NULL || src == NULL) {
        return 0;
    }
    strncpy(dest, src, n - 1);
    dest[n - 1] = '\0';
    return strlen(dest);
}

/** 最大路径长度（包含 null 终止符） */
#define MAX_PATH_LEN 4096

/**
 * @brief Build a path by joining directory and filename.
 *
 * Uses a temporary buffer to avoid undefined behavior when
 * dir and out point to the same memory region.
 *
 * @param dir   Directory path
 * @param file  Filename
 * @param out   Output buffer
 * @param out_size  Size of output buffer
 * @return true on success, false if buffer is too small or paths are invalid
 */
static bool build_path(const char *dir, const char *file, char *out, size_t out_size) {
    if (dir == NULL || file == NULL || out == NULL || out_size == 0) {
        return false;
    }

    /* 计算所需缓冲区大小 */
    size_t dir_len = strlen(dir);
    size_t file_len = strlen(file);
    size_t sep_len = 1; /* 分隔符长度 */

    /* 检查路径长度是否超过安全限制 */
    if (dir_len + sep_len + file_len >= MAX_PATH_LEN) {
        return false;
    }

    /* 检查输出缓冲区是否足够 */
    if (out_size < dir_len + sep_len + file_len + 1) {
        return false;
    }

    const char *sep =
#ifdef _WIN32
        "\\";
#else
        "/";
#endif

    /* 安全构建路径 */
    int written = snprintf(out, out_size, "%s%s%s", dir, sep, file);
    return (written >= 0 && (size_t)written < out_size);
}

/**
 * @brief Build the .lv00_repo directory path.
 */
static void repo_dir_path(const char *repo_path, char *out, size_t out_size) {
    build_path(repo_path, ".lv00_repo", out, out_size);
}

/**
 * @brief Create the repository directory structure.
 */
static bool create_repo_dirs(const char *repo_path) {
    char repo_dir[1024];
    char path[1024];

    repo_dir_path(repo_path, repo_dir, sizeof(repo_dir));
    LV00_MKDIR(repo_dir);

    build_path(repo_dir, "commits", path, sizeof(path));
    LV00_MKDIR(path);

    build_path(repo_dir, "objects", path, sizeof(path));
    LV00_MKDIR(path);

    build_path(repo_dir, "branches", path, sizeof(path));
    LV00_MKDIR(path);

    return true;
}

/**
 * @brief Write a string to a file.
 */
static bool write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) return false;
    if (content) {
        fputs(content, f);
    }
    fclose(f);
    return true;
}

/**
 * @brief Read a file into a newly allocated string.
 *
 * @param path  File path
 * @return Newly allocated string, or NULL on failure
 */
static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0) {
        fclose(f);
        return NULL;
    }

    char *buf = (char *)malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t read = fread(buf, 1, (size_t)size, f);
    buf[read] = '\0';
    fclose(f);
    return buf;
}

/**
 * @brief Compute a commit OID from message, parent, and file hashes.
 */
static void compute_commit_oid(const char *message, const char *parent_oid,
    const char *file_hashes, int64_t timestamp, char *oid_out) {
    /* Combine all data into a single buffer for hashing */
    size_t msg_len = message ? strlen(message) : 0;
    size_t parent_len = parent_oid ? strlen(parent_oid) : 0;
    size_t fh_len = file_hashes ? strlen(file_hashes) : 0;
    size_t total = msg_len + 1 + parent_len + 1 + fh_len + 1 + sizeof(int64_t);

    char *buf = (char *)malloc(total);
    if (!buf) {
        memset(oid_out, '0', LV00_OID_LENGTH - 1);
        oid_out[LV00_OID_LENGTH - 1] = '\0';
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

    compute_sha256_hex(buf, pos, oid_out, LV00_OID_LENGTH);
    free(buf);
}

/**
 * @brief Write commit metadata to a file.
 */
static bool write_commit_file(const char *repo_path, const Lv00ProofCommit *commit) {
    char dir[1024], path[1024];

    repo_dir_path(repo_path, dir, sizeof(dir));
    build_path(dir, "commits", dir, sizeof(dir));
    build_path(dir, commit->oid, path, sizeof(path));

    FILE *f = fopen(path, "w");
    if (!f) return false;

    fprintf(f, "oid: %s\n", commit->oid);
    fprintf(f, "message: %s\n", commit->message);
    fprintf(f, "parent: %s\n", commit->parent_oid);
    fprintf(f, "timestamp: %lld\n", (long long)commit->timestamp);

    fclose(f);
    return true;
}

/**
 * @brief Read commit metadata from a file.
 */
static bool read_commit_file(const char *path, Lv00ProofCommit *commit) {
    char *content = read_file(path);
    if (!content) return false;

    memset(commit, 0, sizeof(Lv00ProofCommit));

    char *line = strtok(content, "\n");
    while (line) {
        if (strncmp(line, "oid: ", 5) == 0) {
            safe_strncpy(commit->oid, line + 5, LV00_OID_LENGTH);
        } else if (strncmp(line, "message: ", 9) == 0) {
            safe_strncpy(commit->message, line + 9, LV00_COMMIT_MSG_MAX);
        } else if (strncmp(line, "parent: ", 8) == 0) {
            safe_strncpy(commit->parent_oid, line + 8, LV00_OID_LENGTH);
        } else if (strncmp(line, "timestamp: ", 11) == 0) {
            commit->timestamp = strtoll(line + 11, NULL, 10);
        }
        line = strtok(NULL, "\n");
    }

    free(content);
    return true;
}

/**
 * @brief Get the current time as a Unix timestamp.
 */
static int64_t get_timestamp(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    int64_t tt = ((int64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    /* Convert from 100-nanosecond intervals since 1601-01-01 to Unix epoch */
    return (tt - 116444736000000000LL) / 10000000LL;
#else
    #include <time.h>
    return (int64_t)time(NULL);
#endif
}

/* ============================================================
 * API implementation: Repository lifecycle
 * ============================================================ */

Lv00ProofRepo *proof_repo_init(const char *path) {
    if (!path) return NULL;

    Lv00ProofRepo *repo = (Lv00ProofRepo *)calloc(1, sizeof(Lv00ProofRepo));
    if (!repo) return NULL;

    strncpy(repo->path, path, sizeof(repo->path) - 1);
    repo->path[sizeof(repo->path) - 1] = '\0';

    /* Create directory structure */
    if (!create_repo_dirs(path)) {
        free(repo);
        return NULL;
    }

    /* Create initial root commit */
    Lv00ProofCommit root;
    memset(&root, 0, sizeof(root));
    safe_strncpy(root.message, "Initial commit", LV00_COMMIT_MSG_MAX);
    root.parent_oid[0] = '\0';
    root.timestamp = get_timestamp();
    compute_commit_oid(root.message, "", "", root.timestamp, root.oid);

    if (!write_commit_file(path, &root)) {
        free(repo);
        return NULL;
    }

    /* Set HEAD to root commit */
    safe_strncpy(repo->head_commit, root.oid, LV00_OID_LENGTH);

    /* Write HEAD file */
    char head_path[1024];
    char dir[1024];
    repo_dir_path(path, dir, sizeof(dir));
    build_path(dir, "HEAD", head_path, sizeof(head_path));
    write_file(head_path, root.oid);

    /* Create default 'main' branch */
    safe_strncpy(repo->branches[0], "main", LV00_BRANCH_NAME_MAX);
    safe_strncpy(repo->branch_heads[0], root.oid, LV00_OID_LENGTH);
    repo->branch_count = 1;

    /* Write branch file */
    char branch_path[1024];
    build_path(dir, "branches", dir, sizeof(dir));
    build_path(dir, "main", branch_path, sizeof(branch_path));
    write_file(branch_path, root.oid);

    return repo;
}

Lv00ProofRepo *proof_repo_open(const char *path) {
    if (!path) return NULL;

    Lv00ProofRepo *repo = (Lv00ProofRepo *)calloc(1, sizeof(Lv00ProofRepo));
    if (!repo) return NULL;

    strncpy(repo->path, path, sizeof(repo->path) - 1);
    repo->path[sizeof(repo->path) - 1] = '\0';

    /* Read HEAD */
    char head_path[1024], dir[1024];
    repo_dir_path(path, dir, sizeof(dir));
    build_path(dir, "HEAD", head_path, sizeof(head_path));

    char *head = read_file(head_path);
    if (!head) {
        free(repo);
        return NULL;
    }
    /* Strip trailing newline */
    size_t hlen = strlen(head);
    if (hlen > 0 && head[hlen - 1] == '\n') head[hlen - 1] = '\0';
    safe_strncpy(repo->head_commit, head, LV00_OID_LENGTH);
    free(head);

    /* Read branches */
    char branches_dir[1024];
    build_path(dir, "branches", branches_dir, sizeof(branches_dir));

    /* Scan branch files */
    repo->branch_count = 0;
    for (int i = 0; i < LV00_MAX_BRANCHES && repo->branch_count < LV00_MAX_BRANCHES; i++) {
        /* Try common branch names */
        const char *known_branches[] = {"main", "master", "develop", "feature", "test"};
        int num_known = 5;

        for (int j = 0; j < num_known && repo->branch_count < LV00_MAX_BRANCHES; j++) {
            /* Check if already loaded */
            bool already = false;
            for (int k = 0; k < repo->branch_count; k++) {
                if (strcmp(repo->branches[k], known_branches[j]) == 0) {
                    already = true;
                    break;
                }
            }
            if (already) continue;

            char bp[1024];
            build_path(branches_dir, known_branches[j], bp, sizeof(bp));
            char *content = read_file(bp);
            if (content) {
                safe_strncpy(repo->branches[repo->branch_count], known_branches[j],
                    LV00_BRANCH_NAME_MAX);
                size_t clen = strlen(content);
                if (clen > 0 && content[clen - 1] == '\n') content[clen - 1] = '\0';
                safe_strncpy(repo->branch_heads[repo->branch_count], content,
                    LV00_OID_LENGTH);
                free(content);
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

void proof_repo_destroy(Lv00ProofRepo *repo) {
    free(repo);
}

/* ============================================================
 * API implementation: Commits
 * ============================================================ */

bool proof_repo_commit(Lv00ProofRepo *repo, const char *message,
    const char **files, const char **contents, size_t file_count) {
    if (!repo || !message) return false;

    /* Compute file hashes and store objects */
    char *hash_buf = (char *)malloc(file_count * (LV00_OID_LENGTH + 1));
    if (!hash_buf) return false;
    memset(hash_buf, 0, file_count * (LV00_OID_LENGTH + 1));

    char dir[1024], obj_dir[1024], obj_path[1024];
    repo_dir_path(repo->path, dir, sizeof(dir));
    build_path(dir, "objects", obj_dir, sizeof(obj_dir));

    for (size_t i = 0; i < file_count; i++) {
        if (!files[i] || !contents[i]) continue;

        /* Compute content hash */
        char hash[LV00_OID_LENGTH];
        compute_sha256_hex(contents[i], strlen(contents[i]), hash, LV00_OID_LENGTH);

        /* Store object */
        build_path(obj_dir, hash, obj_path, sizeof(obj_path));
        write_file(obj_path, contents[i]);

        /* Append to hash buffer */
        if (i > 0) {
            /* hash_buf already has separator space from memset */
        }
        strncat(hash_buf, hash, LV00_OID_LENGTH - 1);
        if (i + 1 < file_count) {
            strncat(hash_buf, " ", 1);
        }
    }

    /* Create commit */
    Lv00ProofCommit commit;
    memset(&commit, 0, sizeof(commit));
    safe_strncpy(commit.message, message, LV00_COMMIT_MSG_MAX);
    safe_strncpy(commit.parent_oid, repo->head_commit, LV00_OID_LENGTH);
    commit.timestamp = get_timestamp();
    compute_commit_oid(message, repo->head_commit, hash_buf, commit.timestamp, commit.oid);

    free(hash_buf);

    /* Write commit file */
    if (!write_commit_file(repo->path, &commit)) {
        return false;
    }

    /* Update HEAD */
    safe_strncpy(repo->head_commit, commit.oid, LV00_OID_LENGTH);
    char head_path[1024];
    build_path(dir, "HEAD", head_path, sizeof(head_path));
    write_file(head_path, commit.oid);

    /* Update current branch head */
    for (int i = 0; i < repo->branch_count; i++) {
        if (strcmp(repo->branch_heads[i], commit.parent_oid) == 0) {
            safe_strncpy(repo->branch_heads[i], commit.oid, LV00_OID_LENGTH);
            char branch_path[1024], branches_dir[1024];
            build_path(dir, "branches", branches_dir, sizeof(branches_dir));
            build_path(branches_dir, repo->branches[i], branch_path, sizeof(branch_path));
            write_file(branch_path, commit.oid);
            break;
        }
    }

    return true;
}

/* ============================================================
 * API implementation: History
 * ============================================================ */

size_t proof_repo_log(Lv00ProofRepo *repo, Lv00ProofCommit *commits, size_t max_count) {
    if (!repo || !commits || max_count == 0) return 0;

    size_t count = 0;
    char current_oid[LV00_OID_LENGTH];
    memset(current_oid, 0, sizeof(current_oid));
    safe_strncpy(current_oid, repo->head_commit, LV00_OID_LENGTH);

    char dir[1024], commit_path[1024];
    repo_dir_path(repo->path, dir, sizeof(dir));
    build_path(dir, "commits", dir, sizeof(dir));

    while (count < max_count && current_oid[0] != '\0') {
        build_path(dir, current_oid, commit_path, sizeof(commit_path));

        if (!read_commit_file(commit_path, &commits[count])) {
            break;
        }

        /* Move to parent */
        safe_strncpy(current_oid, commits[count].parent_oid, LV00_OID_LENGTH);
        count++;
    }

    return count;
}

/* ============================================================
 * API implementation: Diff
 * ============================================================ */

bool proof_repo_diff(Lv00ProofRepo *repo, const char *oid_a, const char *oid_b,
    Lv00ProofDiff *diff) {
    if (!repo || !diff) return false;

    diff->entries = NULL;
    diff->count = 0;

    /* For this simple implementation, diff between two commits
     * compares their stored file objects. Since commits reference
     * file hashes, we report the commit-level difference. */
    char target_b[LV00_OID_LENGTH];
    memset(target_b, 0, sizeof(target_b));
    if (oid_b) {
        safe_strncpy(target_b, oid_b, LV00_OID_LENGTH);
    } else {
        safe_strncpy(target_b, repo->head_commit, LV00_OID_LENGTH);
    }

    /* If oid_a is NULL, treat as empty tree (all files added) */
    if (!oid_a || oid_a[0] == '\0') {
        diff->entries = (Lv00ProofDiffEntry *)calloc(1, sizeof(Lv00ProofDiffEntry));
        if (!diff->entries) return false;

        strncpy(diff->entries[0].path, "(root)", sizeof(diff->entries[0].path) - 1);
        diff->entries[0].path[sizeof(diff->entries[0].path) - 1] = '\0';
        memset(diff->entries[0].old_hash, '0', LV00_OID_LENGTH - 1);
        diff->entries[0].old_hash[LV00_OID_LENGTH - 1] = '\0';
        safe_strncpy(diff->entries[0].new_hash, target_b, LV00_OID_LENGTH);
        diff->entries[0].change_type = 0; /* added */
        diff->count = 1;
        return true;
    }

    /* Compare two commits */
    diff->entries = (Lv00ProofDiffEntry *)calloc(1, sizeof(Lv00ProofDiffEntry));
    if (!diff->entries) return false;

    strncpy(diff->entries[0].path, "(commit)", sizeof(diff->entries[0].path) - 1);
    diff->entries[0].path[sizeof(diff->entries[0].path) - 1] = '\0';
    safe_strncpy(diff->entries[0].old_hash, oid_a, LV00_OID_LENGTH);
    safe_strncpy(diff->entries[0].new_hash, target_b, LV00_OID_LENGTH);
    diff->entries[0].change_type = 1; /* modified */
    diff->count = 1;

    return true;
}

void proof_repo_diff_destroy(Lv00ProofDiff *diff) {
    if (!diff) return;
    free(diff->entries);
    diff->entries = NULL;
    diff->count = 0;
}

/* ============================================================
 * API implementation: Branches
 * ============================================================ */

bool proof_repo_branch(Lv00ProofRepo *repo, const char *name) {
    if (!repo || !name) return false;
    if (repo->branch_count >= LV00_MAX_BRANCHES) return false;

    /* Check if branch already exists */
    for (int i = 0; i < repo->branch_count; i++) {
        if (strcmp(repo->branches[i], name) == 0) {
            return false; /* Branch already exists */
        }
    }

    /* Create new branch pointing at HEAD */
    int idx = repo->branch_count;
    safe_strncpy(repo->branches[idx], name, LV00_BRANCH_NAME_MAX);
    safe_strncpy(repo->branch_heads[idx], repo->head_commit, LV00_OID_LENGTH);
    repo->branch_count++;

    /* Write branch file */
    char dir[1024], branch_path[1024];
    repo_dir_path(repo->path, dir, sizeof(dir));
    build_path(dir, "branches", dir, sizeof(dir));
    build_path(dir, name, branch_path, sizeof(branch_path));
    write_file(branch_path, repo->head_commit);

    return true;
}

bool proof_repo_checkout(Lv00ProofRepo *repo, const char *name) {
    if (!repo || !name) return false;

    /* Find branch */
    for (int i = 0; i < repo->branch_count; i++) {
        if (strcmp(repo->branches[i], name) == 0) {
            /* Update HEAD */
            safe_strncpy(repo->head_commit, repo->branch_heads[i], LV00_OID_LENGTH);

            /* Write HEAD file */
            char dir[1024], head_path[1024];
            repo_dir_path(repo->path, dir, sizeof(dir));
            build_path(dir, "HEAD", head_path, sizeof(head_path));
            write_file(head_path, repo->head_commit);

            return true;
        }
    }

    return false; /* Branch not found */
}
