/**
 * @file lv_path.c
 * @brief 统一路径工具族实现
 *
 * @details 由 debug_state.c 的 create_directory（逐级建目录）与
 *          proof_version.c 的 build_path（安全拼接）等手写实现收敛而来。
 */
#include "lv/lv_path.h"

#include "lv/config.h"      /* lv_PATH_SEPARATOR（唯一权威来源）、lv_PATH_BUF_SIZE */
#include "lv/lv_platform.h" /* lv_mkdir */
#include "lv/lv_utils.h"    /* lv_strlcpy */
#include "lv/cross_platform.h" /* lv_THREAD_LOCAL */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

bool lv_path_join(const char *dir, const char *file, char *out, size_t out_size) {
    if (!dir || !file || !out || out_size == 0) {
        return false;
    }
    int written = snprintf(out, out_size, "%s%c%s", dir, lv_PATH_SEPARATOR, file);
    return (written >= 0 && (size_t) written < out_size);
}

const char *lv_path_basename(const char *path) {
    if (!path) {
        return NULL;
    }
    const char *slash = strrchr(path, '/');
    const char *bslash = strrchr(path, '\\');
    const char *last = NULL;
    if (bslash && (!slash || bslash > slash)) {
        last = bslash;
    } else {
        last = slash;
    }
    return last ? last + 1 : path;
}

const char *lv_path_dirname(const char *path, size_t *out_len) {
    if (out_len) {
        *out_len = 0;
    }
    if (!path) {
        return NULL;
    }
    const char *slash = strrchr(path, '/');
    const char *bslash = strrchr(path, '\\');
    const char *last = NULL;
    if (bslash && (!slash || bslash > slash)) {
        last = bslash;
    } else {
        last = slash;
    }
    if (!last) {
        return NULL;
    }
    if (out_len) {
        *out_len = (size_t) (last - path);
    }
    return path;
}

char *lv_path_strip_ext(char *path) {
    if (!path) {
        return NULL;
    }
    char *dot = strrchr(path, '.');
    if (dot) {
        *dot = '\0';
    }
    return path;
}

int lv_path_mkdirs(const char *path) {
    if (!path) {
        return -1;
    }
    char tmp[lv_PATH_BUF_SIZE];
    char *p = NULL;
    size_t len;

    /* 使用 lv_strlcpy 确保零终止后再计算 strlen */
    lv_strlcpy(tmp, path, sizeof(tmp));
    len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == lv_PATH_SEPARATOR) {
        tmp[len - 1] = '\0';
    }

    /* 逐级创建：直接尝试 mkdir，处理 EEXIST（目录已存在）而非预先检查，
     * 避免 TOCTOU（检查时间-使用时间）竞争条件 */
    for (p = tmp + 1; *p; p++) {
        if (*p == lv_PATH_SEPARATOR) {
            *p = '\0';
            if (lv_mkdir(tmp) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = lv_PATH_SEPARATOR;
        }
    }

    /* 创建路径最后一个组件 */
    if (lv_mkdir(tmp) != 0 && errno != EEXIST) {
        return -1;
    }

    return 0;
}

#ifdef _WIN32
/* Windows：递归删除子项的回调（删除目录前先清空内容） */
static bool lv_path_remove_child_cb(void *ctx, const char *name) {
    char child[lv_PATH_BUF_SIZE];
    if (!lv_path_join((const char *) ctx, name, child, sizeof(child))) {
        return false;
    }
    (void) lv_path_remove(child);
    return true;
}
#endif

int lv_path_remove(const char *path) {
    if (!path || !path[0]) {
        return -1;
    }
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        DWORD err = GetLastError();
        return (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) ? 0 : -1;
    }
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        lv_dir_foreach(path, lv_path_remove_child_cb, (void *) path);
        return RemoveDirectoryA(path) ? 0 : -1;
    }
    return DeleteFileA(path) ? 0 : -1;
#else
    struct stat st;
    if (lstat(path, &st) != 0) {
        return (errno == ENOENT || errno == ENOTDIR) ? 0 : -1;
    }
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        if (!d) {
            return -1;
        }
        struct dirent *entry;
        char *child = NULL;
        size_t child_cap = 0;
        while ((entry = readdir(d)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            size_t need = strlen(path) + strlen(entry->d_name) + 2;
            if (need > child_cap) {
                /* 收敛到 lv_realloc/lv_free 以纳入统一内存追踪，保持"恰好扩到
                 * need"语义不变（不复用 lv_ensure_capacity 的倍增）。lv_realloc
                 * 失败时保留原指针，与 realloc 语义等价；此处 need >= 2 恒成立，
                 * 不会触发 lv_realloc 的 size==0 分支。 */
                char *tmp = lv_realloc(child, need);
                if (!tmp) {
                    lv_free(&child);
                    closedir(d);
                    return -1;
                }
                child = tmp;
                child_cap = need;
            }
            snprintf(child, need, "%s/%s", path, entry->d_name);
            (void) lv_path_remove(child);
        }
        lv_free(&child);
        closedir(d);
        return rmdir(path) == 0 ? 0 : -1;
    }
    return unlink(path) == 0 ? 0 : -1;
#endif
}

int lv_dir_foreach(const char *dir, lv_dir_entry_fn fn, void *ctx) {
    if (!dir || !fn) {
        return -1;
    }
#ifdef _WIN32
    char pattern[lv_PATH_BUF_SIZE];
    if (!lv_path_join(dir, "*", pattern, sizeof(pattern))) {
        return -1;
    }
    WIN32_FIND_DATAA find_data;
    HANDLE h_find = FindFirstFileA(pattern, &find_data);
    if (h_find == INVALID_HANDLE_VALUE) {
        return 0; /* 目录不存在或为空：与历史手写遍历语义一致，不算错误 */
    }
    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }
        if (!fn(ctx, find_data.cFileName)) {
            break;
        }
    } while (FindNextFileA(h_find, &find_data));
    FindClose(h_find);
#else
    DIR *d = opendir(dir);
    if (!d) {
        return 0;
    }
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (!fn(ctx, entry->d_name)) {
            break;
        }
    }
    closedir(d);
#endif
    return 0;
}

bool lv_temp_path(char *out, size_t out_size) {
    if (!out || out_size == 0) {
        return false;
    }
    static unsigned long s_temp_counter = 0;
    s_temp_counter++;
#ifdef _WIN32
    DWORD tmp_len = GetTempPathA(0, NULL);
    if (tmp_len == 0 || (size_t) tmp_len + 40 > out_size) {
        return false;
    }
    if (GetTempPathA((DWORD) out_size, out) == 0) {
        return false;
    }
    size_t base = strlen(out);
    if (base > 0 && out[base - 1] != '\\') {
        if (base + 1 >= out_size) {
            return false;
        }
        out[base] = '\\';
        out[base + 1] = '\0';
        base++;
    }
    snprintf(out + base, out_size - base, "lv_tmp_%08lx_%010llu_%06lu",
             (unsigned long) GetCurrentProcessId(),
             (unsigned long long) lv_get_time_ns(),
             (unsigned long) s_temp_counter);
    return true;
#else
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || !tmpdir[0]) {
        tmpdir = "/tmp";
    }
    int written = snprintf(out, out_size, "%s/lv_tmp_%08d_%010llu_%06lu",
                           tmpdir, (int) getpid(),
                           (unsigned long long) lv_get_time_ns(),
                           (unsigned long) s_temp_counter);
    return (written >= 0 && (size_t) written < out_size);
#endif
}

const char *lv_path_home_dir(void) {
    /* 收敛说明：由 debug_state.c 的 get_home_dir（#ifdef 双平台镜像）收敛而来，
     * 环境变量空值处理统一在此收敛（与 lv_temp_path 的 TMPDIR 空值回落模式一致）。
     * Windows：USERPROFILE → HOMEDRIVE+HOMEPATH → 空串；
     * POSIX：HOME 缺失/为空回落 "/tmp"。 */
#ifdef _WIN32
    static lv_THREAD_LOCAL char home_path[MAX_PATH] = {0};
    if (home_path[0] == '\0') {
        const char *up = getenv("USERPROFILE");
        if (up && up[0]) {
            lv_strlcpy(home_path, up, sizeof(home_path));
        } else {
            const char *hd = getenv("HOMEDRIVE");
            const char *hp = getenv("HOMEPATH");
            if (hd && hp && hd[0] && hp[0]) {
                snprintf(home_path, MAX_PATH, "%s%s", hd, hp);
            }
        }
    }
    return home_path;
#else
    const char *home = getenv("HOME");
    return (home && home[0]) ? home : "/tmp";
#endif
}
