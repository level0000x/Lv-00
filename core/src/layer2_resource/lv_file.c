#include "lv/lv_file.h"
#include "lv/lv_log.h"
#include "lv/lv_utils.h"
#include "lv/cross_platform.h"

#include <string.h>
#include <errno.h>

/* lv_platform.h 定义了 lv_file_exists(path) 宏（基于 lv_access），
 * 与本文件实现的 lv_file_exists 函数重名；此处取消宏定义以保留函数实现。 */
#ifdef lv_file_exists
#undef lv_file_exists
#endif

FILE *lv_file_open(const char *path, const char *mode) {
    if (!path || !mode) return NULL;
    FILE *fp = fopen(path, mode);
    if (!fp) {
        lv_ERROR("打开文件失败: %s (mode=%s, errno=%d)", path, mode, errno);
    }
    return fp;
}

int lv_file_close(FILE *fp) {
    if (!fp) return 0;
    if (fclose(fp) != 0) {
        lv_ERROR("关闭文件失败 (errno=%d)", errno);
        return -1;
    }
    return 0;
}

uint8_t *lv_file_read_all(const char *path, size_t *out_len) {
    if (out_len) *out_len = 0;
    FILE *fp = lv_file_open(path, "rb");
    if (!fp) return NULL;
    size_t sz = lv_file_size(fp);
    if (sz == 0) {
        lv_file_close(fp);
        return NULL;
    }
    uint8_t *buf = (uint8_t *)lv_malloc(sz + 1);
    if (!buf) {
        lv_file_close(fp);
        return NULL;
    }
    size_t nread = fread(buf, 1, sz, fp);
    if (nread != sz) {
        lv_ERROR("读取文件不完整: %s (%zu/%zu)", path, nread, sz);
        lv_free((void **)&buf);
        lv_file_close(fp);
        return NULL;
    }
    buf[sz] = '\0';
    lv_file_close(fp);
    if (out_len) *out_len = sz;
    return buf;
}

int lv_file_write_all(const char *path, const void *data, size_t len) {
    FILE *fp = lv_file_open(path, "wb");
    if (!fp) return -1;
    size_t nwritten = fwrite(data, 1, len, fp);
    if (nwritten != len) {
        lv_ERROR("写入文件不完整: %s (%zu/%zu)", path, nwritten, len);
        lv_file_close(fp);
        return -1;
    }
    return lv_file_close(fp);
}

bool lv_file_exists(const char *path) {
    if (!path) return false;
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES);
#else
    struct stat st;
    return stat(path, &st) == 0;
#endif
}

size_t lv_file_size(FILE *fp) {
    if (!fp) return 0;
    long pos = ftell(fp);
    if (pos < 0) return 0;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, pos, SEEK_SET);
    return (sz > 0) ? (size_t)sz : 0;
}
