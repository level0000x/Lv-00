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

#include <errno.h>
#include <stdio.h>
#include <string.h>

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
