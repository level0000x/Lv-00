/**
 * @file lv_storage.c
 * @brief 统一存储抽象层实现 —— 文件系统后端 + 内存缓冲区后端 + 后端注册表
 *
 * @details 实现 lv_storage.h 中声明的所有接口，包括：
 *          - 文件系统后端（file:// URI 或普通路径）
 *          - 内存缓冲区后端（mem:// URI）
 *          - 后端注册表（URI scheme → 后端映射）
 *          - 序列化注册表扩展
 *
 * @author Lv-00 Project
 * @version 1.0.0
 * @date   2026-08-04
 */

#include "lv/lv_storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "lv/lv_utils.h"   /* lv_malloc, lv_free, lv_calloc, lv_realloc */
#include "lv/lv_log.h"     /* lv_ERROR, lv_WARN, lv_DEBUG */
#include "lv/lv_thread.h"  /* lv_lazy_lock / lv_mutex_* */

/* ═══════════════════════════════════════════════════════════════════
 * 内部常量
 * ═══════════════════════════════════════════════════════════════════ */

/** @brief 最大注册后端数量 */
#define lv_STORAGE_MAX_BACKENDS 16

/** @brief 内存缓冲区初始容量 */
#define lv_STORAGE_MEM_INIT_CAPACITY 4096

/** @brief 内存缓冲区扩容因子 */
#define lv_STORAGE_MEM_GROW_FACTOR 2

/** @brief URI 最大长度 */
#define lv_STORAGE_MAX_URI 512

/* ═══════════════════════════════════════════════════════════════════
 * 第 1 节：内存缓冲区后端
 * ═══════════════════════════════════════════════════════════════════ */

/** @brief 内存缓冲区后端上下文 */
typedef struct {
    char    *data;       /**< 动态缓冲区 */
    int64_t  size;       /**< 数据大小 */
    int64_t  capacity;   /**< 缓冲区容量 */
    int64_t  pos;        /**< 当前读写位置 */
    bool     own_data;   /**< 是否拥有 data 所有权（由 open 传入） */
} MemCtx;

/** @brief 创建内存缓冲区后端上下文 */
static void *mem_create_ctx(void) {
    MemCtx *ctx = (MemCtx *)lv_calloc(1, sizeof(MemCtx));
    if (!ctx) return NULL;
    ctx->data     = NULL;
    ctx->size     = 0;
    ctx->capacity = 0;
    ctx->pos      = 0;
    ctx->own_data = true;
    return ctx;
}

/** @brief 销毁内存缓冲区后端上下文 */
static void mem_destroy_ctx(void *vctx) {
    if (!vctx) return;
    MemCtx *ctx = (MemCtx *)vctx;
    if (ctx->own_data && ctx->data) {
        lv_free((void **)&ctx->data);
    }
    ctx->data     = NULL;
    ctx->size     = 0;
    ctx->capacity = 0;
    ctx->pos      = 0;
    lv_free((void **)&ctx);
}

/** @brief 确保内存缓冲区至少有 min_cap 容量 */
static bool mem_ensure_capacity(MemCtx *ctx, int64_t min_cap) {
    if (min_cap <= ctx->capacity) return true;
    int64_t new_cap = ctx->capacity > 0 ? ctx->capacity : lv_STORAGE_MEM_INIT_CAPACITY;
    while (new_cap < min_cap) {
        new_cap *= lv_STORAGE_MEM_GROW_FACTOR;
    }
    char *new_data = (char *)lv_realloc(ctx->data, (size_t)new_cap);
    if (!new_data) return false;
    ctx->data = new_data;
    ctx->capacity = new_cap;
    return true;
}

/** @brief 打开内存缓冲区后端 */
static bool mem_open(void *vctx, const char *uri, int mode) {
    (void)mode;
    MemCtx *ctx = (MemCtx *)vctx;
    if (!ctx) return false;

    /* mem:// URI 格式：mem://<name> 或 mem://  */
    const char *name = uri + 6; /* 跳过 "mem://" */
    if (*name == '\0') name = "unnamed";

    /* 初始分配 */
    if (!mem_ensure_capacity(ctx, lv_STORAGE_MEM_INIT_CAPACITY)) {
        return false;
    }
    ctx->size = 0;
    ctx->pos  = 0;
    return true;
}

/** @brief 关闭内存缓冲区后端 */
static void mem_close(void *vctx) {
    MemCtx *ctx = (MemCtx *)vctx;
    if (!ctx) return;
    if (ctx->own_data && ctx->data) {
        lv_free((void **)&ctx->data);
    }
    ctx->data     = NULL;
    ctx->size     = 0;
    ctx->capacity = 0;
    ctx->pos      = 0;
}

/** @brief 从内存缓冲区读取数据 */
static int64_t mem_read(void *vctx, void *buf, int64_t size) {
    MemCtx *ctx = (MemCtx *)vctx;
    if (!ctx || !buf || size <= 0) return -1;

    int64_t avail = ctx->size - ctx->pos;
    if (avail <= 0) return 0;
    if (size > avail) size = avail;

    memcpy(buf, ctx->data + ctx->pos, (size_t)size);
    ctx->pos += size;
    return size;
}

/** @brief 向内存缓冲区写入数据 */
static int64_t mem_write(void *vctx, const void *buf, int64_t size) {
    MemCtx *ctx = (MemCtx *)vctx;
    if (!ctx || !buf || size <= 0) return -1;

    int64_t needed = ctx->pos + size;
    if (needed > ctx->capacity) {
        if (!mem_ensure_capacity(ctx, needed)) return -1;
    }

    memcpy(ctx->data + ctx->pos, buf, (size_t)size);
    ctx->pos += size;
    if (ctx->pos > ctx->size) {
        ctx->size = ctx->pos;
    }
    return size;
}

/** @brief 定位内存缓冲区读写位置 */
static int64_t mem_seek(void *vctx, int64_t offset, lvStorageSeekOrigin origin) {
    MemCtx *ctx = (MemCtx *)vctx;
    if (!ctx) return -1;

    int64_t new_pos;
    switch (origin) {
        case lv_STORAGE_SEEK_SET:
            new_pos = offset;
            break;
        case lv_STORAGE_SEEK_CUR:
            new_pos = ctx->pos + offset;
            break;
        case lv_STORAGE_SEEK_END:
            new_pos = ctx->size + offset;
            break;
        default:
            return -1;
    }

    if (new_pos < 0) new_pos = 0;
    ctx->pos = new_pos;
    /* 如果定位到当前 size 之后，需要扩展缓冲区但不改变 size */
    if (new_pos > ctx->capacity) {
        if (!mem_ensure_capacity(ctx, new_pos)) return -1;
    }
    return ctx->pos;
}

/** @brief 获取内存缓冲区当前位置 */
static int64_t mem_tell(void *vctx) {
    MemCtx *ctx = (MemCtx *)vctx;
    if (!ctx) return -1;
    return ctx->pos;
}

/** @brief 获取内存缓冲区数据大小 */
static int64_t mem_size(void *vctx) {
    MemCtx *ctx = (MemCtx *)vctx;
    if (!ctx) return -1;
    return ctx->size;
}

/** @brief 刷新内存缓冲区（无操作） */
static bool mem_flush(void *vctx) {
    (void)vctx;
    return true;
}

/** @brief 检查内存缓冲区是否到达末尾 */
static bool mem_eof(void *vctx) {
    MemCtx *ctx = (MemCtx *)vctx;
    if (!ctx) return true;
    return ctx->pos >= ctx->size;
}

/** @brief 内存缓冲区后端操作虚表 */
static const lvStorageOps g_mem_ops = {
    .open   = mem_open,
    .close  = mem_close,
    .read   = mem_read,
    .write  = mem_write,
    .seek   = mem_seek,
    .tell   = mem_tell,
    .size   = mem_size,
    .flush  = mem_flush,
    .eof    = mem_eof,
};

/* ═══════════════════════════════════════════════════════════════════
 * 第 2 节：文件系统后端
 * ═══════════════════════════════════════════════════════════════════ */

/** @brief 文件系统后端上下文 */
typedef struct {
    FILE *fp;        /**< 文件指针 */
    char  mode_str[8]; /**< fopen 模式字符串 */
} FileCtx;

/** @brief 将 lvStorageMode 位组合转换为 fopen 模式字符串 */
static void file_mode_to_fopen(int mode, char *out, size_t out_size) {
    int idx = 0;
    (void)out_size;

    if ((mode & lv_STORAGE_READ) && (mode & lv_STORAGE_WRITE)) {
        out[idx++] = 'a';
        /* 优先使用 "a+"（追加+读），但如有 TRUNCATE 则用 "w+" */
        if (mode & lv_STORAGE_TRUNCATE) {
            out[0] = 'w';
        }
        out[idx++] = '+';
    } else if (mode & lv_STORAGE_WRITE) {
        if (mode & lv_STORAGE_APPEND) {
            out[idx++] = 'a';
        } else if (mode & lv_STORAGE_TRUNCATE) {
            out[idx++] = 'w';
        } else {
            out[idx++] = 'r';
            out[idx++] = '+'; /* 没有 TRUNCATE 且没有 APPEND 的写模式需要 r+ */
        }
    } else {
        out[idx++] = 'r';
    }

    if (mode & lv_STORAGE_BINARY) {
        out[idx++] = 'b';
    }

    out[idx] = '\0';
}

/** @brief 创建文件系统后端上下文 */
static void *file_create_ctx(void) {
    FileCtx *ctx = (FileCtx *)lv_calloc(1, sizeof(FileCtx));
    if (!ctx) return NULL;
    ctx->fp = NULL;
    ctx->mode_str[0] = '\0';
    return ctx;
}

/** @brief 销毁文件系统后端上下文 */
static void file_destroy_ctx(void *vctx) {
    if (!vctx) return;
    FileCtx *ctx = (FileCtx *)vctx;
    if (ctx->fp) {
        fclose(ctx->fp);
        ctx->fp = NULL;
    }
    lv_free((void **)&ctx);
}

/**
 * @brief 从 URI 或普通路径提取文件路径
 *
 * "file:///tmp/foo"  → "/tmp/foo"
 * "/tmp/foo"         → "/tmp/foo"
 * "C:\\tmp\\foo"     → "C:\\tmp\\foo"
 * "relative/path"    → "relative/path"
 */
static const char *file_extract_path(const char *uri) {
    if (!uri) return NULL;
    if (strncmp(uri, "file://", 7) == 0) {
        return uri + 7;
    }
    return uri;
}

/** @brief 打开文件系统后端 */
static bool file_open(void *vctx, const char *uri, int mode) {
    FileCtx *ctx = (FileCtx *)vctx;
    if (!ctx || !uri) return false;

    const char *path = file_extract_path(uri);
    if (!path || *path == '\0') return false;

    file_mode_to_fopen(mode, ctx->mode_str, sizeof(ctx->mode_str));

    /* 如果 mode 包含 CREATE 且文件不存在，需要特殊处理 */
    FILE *fp = NULL;
    if ((mode & lv_STORAGE_CREATE) && !(mode & lv_STORAGE_WRITE)) {
        /* CREATE 但不 WRITE 没有意义，尝试 r+ 模式 */
        fp = fopen(path, "r+b");
        if (!fp) {
            /* 文件不存在，创建它 */
            fp = fopen(path, "wb");
            if (fp) {
                fclose(fp);
                fp = fopen(path, "rb");
            }
        }
    } else {
        fp = fopen(path, ctx->mode_str);
    }

    if (!fp) {
        lv_ERROR("打开文件失败: %s (mode=%s, errno=%d)", path, ctx->mode_str, errno);
        return false;
    }

    ctx->fp = fp;
    return true;
}

/** @brief 关闭文件系统后端 */
static void file_close(void *vctx) {
    FileCtx *ctx = (FileCtx *)vctx;
    if (!ctx || !ctx->fp) return;
    fclose(ctx->fp);
    ctx->fp = NULL;
}

/** @brief 从文件读取数据 */
static int64_t file_read(void *vctx, void *buf, int64_t size) {
    FileCtx *ctx = (FileCtx *)vctx;
    if (!ctx || !ctx->fp || !buf || size <= 0) return -1;
    size_t nread = fread(buf, 1, (size_t)size, ctx->fp);
    if (nread == 0 && ferror(ctx->fp)) return -1;
    return (int64_t)nread;
}

/** @brief 向文件写入数据 */
static int64_t file_write(void *vctx, const void *buf, int64_t size) {
    FileCtx *ctx = (FileCtx *)vctx;
    if (!ctx || !ctx->fp || !buf || size <= 0) return -1;
    size_t nwritten = fwrite(buf, 1, (size_t)size, ctx->fp);
    if (nwritten != (size_t)size && ferror(ctx->fp)) return -1;
    return (int64_t)nwritten;
}

/** @brief 定位文件读写位置 */
static int64_t file_seek(void *vctx, int64_t offset, lvStorageSeekOrigin origin) {
    FileCtx *ctx = (FileCtx *)vctx;
    if (!ctx || !ctx->fp) return -1;

    int whence;
    switch (origin) {
        case lv_STORAGE_SEEK_SET: whence = SEEK_SET; break;
        case lv_STORAGE_SEEK_CUR: whence = SEEK_CUR; break;
        case lv_STORAGE_SEEK_END: whence = SEEK_END; break;
        default: return -1;
    }

    if (fseek(ctx->fp, (long)offset, whence) != 0) return -1;
    return (int64_t)ftell(ctx->fp);
}

/** @brief 获取文件当前位置 */
static int64_t file_tell(void *vctx) {
    FileCtx *ctx = (FileCtx *)vctx;
    if (!ctx || !ctx->fp) return -1;
    long pos = ftell(ctx->fp);
    return (pos < 0) ? -1 : (int64_t)pos;
}

/** @brief 获取文件大小 */
static int64_t file_size(void *vctx) {
    FileCtx *ctx = (FileCtx *)vctx;
    if (!ctx || !ctx->fp) return -1;

    long cur = ftell(ctx->fp);
    if (cur < 0) return -1;
    if (fseek(ctx->fp, 0, SEEK_END) != 0) return -1;
    long sz = ftell(ctx->fp);
    if (sz < 0) { fseek(ctx->fp, cur, SEEK_SET); return -1; }
    fseek(ctx->fp, cur, SEEK_SET);
    return (int64_t)sz;
}

/** @brief 刷新文件缓冲区 */
static bool file_flush(void *vctx) {
    FileCtx *ctx = (FileCtx *)vctx;
    if (!ctx || !ctx->fp) return false;
    return fflush(ctx->fp) == 0;
}

/** @brief 检查文件是否到达末尾 */
static bool file_eof(void *vctx) {
    FileCtx *ctx = (FileCtx *)vctx;
    if (!ctx || !ctx->fp) return true;
    return feof(ctx->fp) != 0;
}

/** @brief 文件系统后端操作虚表 */
static const lvStorageOps g_file_ops = {
    .open   = file_open,
    .close  = file_close,
    .read   = file_read,
    .write  = file_write,
    .seek   = file_seek,
    .tell   = file_tell,
    .size   = file_size,
    .flush  = file_flush,
    .eof    = file_eof,
};

/* ═══════════════════════════════════════════════════════════════════
 * 第 3 节：后端注册表
 * ═══════════════════════════════════════════════════════════════════ */

/** @brief 后端注册表条目 */
typedef struct {
    const char               *scheme;
    const lvStorageOps       *ops;
    void *(*create_ctx)(void);
    void  (*destroy_ctx)(void *ctx);
} BackendEntry;

/** @brief 后端注册表状态 */
static struct {
    BackendEntry entries[lv_STORAGE_MAX_BACKENDS];
    int          count;
    lv_lazy_lock lock;   /**< 惰性互斥锁：首次加锁时自动初始化 */
} s_backend_registry = {0};

/** @brief 后端注册表一次性初始化回调：初始化互斥锁并注册默认后端（首次加锁时执行一次） */
static void backend_registry_init_once(void) {
    lv_mutex_init(&s_backend_registry.lock.mutex);
    s_backend_registry.count = 0;

    /* 注册 file:// 后端 */
    s_backend_registry.entries[s_backend_registry.count].scheme      = "file";
    s_backend_registry.entries[s_backend_registry.count].ops         = &g_file_ops;
    s_backend_registry.entries[s_backend_registry.count].create_ctx  = file_create_ctx;
    s_backend_registry.entries[s_backend_registry.count].destroy_ctx = file_destroy_ctx;
    s_backend_registry.count++;

    /* 注册 mem:// 后端 */
    s_backend_registry.entries[s_backend_registry.count].scheme      = "mem";
    s_backend_registry.entries[s_backend_registry.count].ops         = &g_mem_ops;
    s_backend_registry.entries[s_backend_registry.count].create_ctx  = mem_create_ctx;
    s_backend_registry.entries[s_backend_registry.count].destroy_ctx = mem_destroy_ctx;
    s_backend_registry.count++;
}

/** @brief 根据 URI 提取 scheme 部分 */
static const char *extract_scheme(const char *uri, char *out, size_t out_size) {
    if (!uri || !out || out_size == 0) return NULL;

    const char *colon = strstr(uri, "://");
    if (!colon) {
        /* 没有 scheme，视为普通文件路径 */
        return NULL;
    }

    size_t scheme_len = (size_t)(colon - uri);
    if (scheme_len >= out_size) scheme_len = out_size - 1;
    strncpy(out, uri, scheme_len);
    out[scheme_len] = '\0';
    return out;
}

/** @brief 根据 scheme 查找后端 */
static BackendEntry *find_backend(const char *scheme) {
    if (!scheme) {
        /* 无 scheme 使用默认文件后端 */
        for (int i = 0; i < s_backend_registry.count; i++) {
            if (strcmp(s_backend_registry.entries[i].scheme, "file") == 0) {
                return &s_backend_registry.entries[i];
            }
        }
        return NULL;
    }

    for (int i = 0; i < s_backend_registry.count; i++) {
        if (strcmp(s_backend_registry.entries[i].scheme, scheme) == 0) {
            return &s_backend_registry.entries[i];
        }
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════
 * 第 4 节：公共 API 实现
 * ═══════════════════════════════════════════════════════════════════ */

void lv_storage_system_init(void) {
    /* 线程安全的一次性初始化：互斥锁初始化与默认后端注册仅首次执行（lv_lazy_lock 幂等） */
    lv_lazy_lock_init(&s_backend_registry.lock, backend_registry_init_once);
}

void lv_storage_system_cleanup(void) {
    lv_lazy_lock_lock(&s_backend_registry.lock, backend_registry_init_once);

    /* 清理所有后端上下文 */
    for (int i = 0; i < s_backend_registry.count; i++) {
        /* 后端上下文由各 lvStorage 实例管理，这里只清理注册表条目 */
        s_backend_registry.entries[i].scheme      = NULL;
        s_backend_registry.entries[i].ops          = NULL;
        s_backend_registry.entries[i].create_ctx   = NULL;
        s_backend_registry.entries[i].destroy_ctx  = NULL;
    }
    s_backend_registry.count = 0;

    lv_lazy_lock_unlock(&s_backend_registry.lock);
    /* 注：注册表互斥锁由 lv_lazy_lock 一次性初始化，生命周期与进程一致，
     * 不在此销毁；once 不可重置，清理后系统仍可安全继续使用。 */
}

bool lv_storage_register_backend(const lvStorageBackendInfo *info) {
    if (!info || !info->scheme || !info->ops) return false;

    /* 延迟初始化（lv_lazy_lock 幂等，仅首次生效） */
    lv_storage_system_init();

    lv_lazy_lock_lock(&s_backend_registry.lock, backend_registry_init_once);

    /* 检查 scheme 是否已存在 */
    for (int i = 0; i < s_backend_registry.count; i++) {
        if (strcmp(s_backend_registry.entries[i].scheme, info->scheme) == 0) {
            lv_WARN("存储后端 scheme 已存在: %s", info->scheme);
            lv_lazy_lock_unlock(&s_backend_registry.lock);
            return false;
        }
    }

    if (s_backend_registry.count >= lv_STORAGE_MAX_BACKENDS) {
        lv_ERROR("存储后端注册表已满 (max=%d)", lv_STORAGE_MAX_BACKENDS);
        lv_lazy_lock_unlock(&s_backend_registry.lock);
        return false;
    }

    s_backend_registry.entries[s_backend_registry.count].scheme      = info->scheme;
    s_backend_registry.entries[s_backend_registry.count].ops         = info->ops;
    s_backend_registry.entries[s_backend_registry.count].create_ctx  = info->create_ctx;
    s_backend_registry.entries[s_backend_registry.count].destroy_ctx = info->destroy_ctx;
    s_backend_registry.count++;

    lv_lazy_lock_unlock(&s_backend_registry.lock);
    return true;
}

lvStorage *lv_storage_open(const char *uri, int mode) {
    if (!uri) return NULL;

    /* 延迟初始化（lv_once 幂等，仅首次生效） */
    lv_storage_system_init();

    /* 提取 URI scheme */
    char scheme_buf[64] = {0};
    const char *scheme = extract_scheme(uri, scheme_buf, sizeof(scheme_buf));

    lv_lazy_lock_lock(&s_backend_registry.lock, backend_registry_init_once);
    BackendEntry *entry = find_backend(scheme);
    if (!entry) {
        lv_ERROR("不支持的存储 URI scheme: %s (uri=%s)",
                  scheme ? scheme : "(null)", uri);
        lv_lazy_lock_unlock(&s_backend_registry.lock);
        return NULL;
    }

    /* 创建后端上下文 */
    void *ctx = entry->create_ctx ? entry->create_ctx() : NULL;
    if (!ctx) {
        lv_ERROR("创建存储后端上下文失败: scheme=%s", entry->scheme);
        lv_lazy_lock_unlock(&s_backend_registry.lock);
        return NULL;
    }

    lv_lazy_lock_unlock(&s_backend_registry.lock);

    /* 打开后端 */
    if (!entry->ops->open(ctx, uri, mode)) {
        if (entry->destroy_ctx) entry->destroy_ctx(ctx);
        return NULL;
    }

    /* 分配存储句柄 */
    lvStorage *storage = (lvStorage *)lv_malloc(sizeof(lvStorage));
    if (!storage) {
        entry->ops->close(ctx);
        if (entry->destroy_ctx) entry->destroy_ctx(ctx);
        return NULL;
    }

    storage->ops     = entry->ops;
    storage->ctx     = ctx;
    storage->mode    = mode;
    storage->is_open = true;
    strncpy(storage->uri, uri, lv_STORAGE_MAX_URI - 1);
    storage->uri[lv_STORAGE_MAX_URI - 1] = '\0';

    return storage;
}

void lv_storage_close(lvStorage *storage) {
    if (!storage) return;

    if (storage->is_open && storage->ops && storage->ops->close) {
        storage->ops->close(storage->ctx);
    }

    /* 查找并调用 destroy_ctx */
    char scheme_buf[64] = {0};
    const char *scheme = extract_scheme(storage->uri, scheme_buf, sizeof(scheme_buf));

    lv_lazy_lock_lock(&s_backend_registry.lock, backend_registry_init_once);
    BackendEntry *entry = find_backend(scheme);
    if (entry && entry->destroy_ctx) {
        entry->destroy_ctx(storage->ctx);
    } else {
        /* 兜底释放 */
        lv_free((void **)&storage->ctx);
    }
    lv_lazy_lock_unlock(&s_backend_registry.lock);

    storage->ops     = NULL;
    storage->ctx     = NULL;
    storage->is_open = false;
    lv_free((void **)&storage);
}

int64_t lv_storage_read(lvStorage *storage, void *buf, int64_t size) {
    if (!storage || !storage->is_open || !storage->ops || !storage->ops->read) return -1;
    if (!buf || size <= 0) return -1;
    return storage->ops->read(storage->ctx, buf, size);
}

int64_t lv_storage_write(lvStorage *storage, const void *buf, int64_t size) {
    if (!storage || !storage->is_open || !storage->ops || !storage->ops->write) return -1;
    if (!buf || size <= 0) return -1;
    return storage->ops->write(storage->ctx, buf, size);
}

int64_t lv_storage_seek(lvStorage *storage, int64_t offset, lvStorageSeekOrigin origin) {
    if (!storage || !storage->is_open || !storage->ops || !storage->ops->seek) return -1;
    return storage->ops->seek(storage->ctx, offset, origin);
}

int64_t lv_storage_tell(lvStorage *storage) {
    if (!storage || !storage->is_open || !storage->ops || !storage->ops->tell) return -1;
    return storage->ops->tell(storage->ctx);
}

int64_t lv_storage_size(lvStorage *storage) {
    if (!storage || !storage->is_open || !storage->ops || !storage->ops->size) return -1;
    return storage->ops->size(storage->ctx);
}

bool lv_storage_flush(lvStorage *storage) {
    if (!storage || !storage->is_open || !storage->ops || !storage->ops->flush) return false;
    return storage->ops->flush(storage->ctx);
}

bool lv_storage_eof(lvStorage *storage) {
    if (!storage || !storage->is_open || !storage->ops || !storage->ops->eof) return true;
    return storage->ops->eof(storage->ctx);
}

bool lv_storage_is_open(const lvStorage *storage) {
    return storage && storage->is_open;
}

const char *lv_storage_get_uri(const lvStorage *storage) {
    if (!storage) return NULL;
    return storage->uri;
}

char *lv_storage_read_all(lvStorage *storage, int64_t *out_size) {
    if (out_size) *out_size = 0;
    if (!storage || !storage->is_open) return NULL;

    int64_t sz = lv_storage_size(storage);
    if (sz <= 0) {
        /* 对于无法获取大小的流，尝试逐块读取 */
        #define lv_STORAGE_READALL_CHUNK 4096
        char *buf = NULL;
        int64_t total = 0;
        int64_t cap = 0;
        char chunk[lv_STORAGE_READALL_CHUNK];
        int64_t nread;
        while ((nread = lv_storage_read(storage, chunk, lv_STORAGE_READALL_CHUNK)) > 0) {
            if (total + nread > cap) {
                /* 统一委托 lv_ensure_capacity（count 含 +1 结尾 NUL 语义；cap 为 int64_t，经局部 int 桥接） */
                int cap_i = (int) cap;
                if (!lv_ensure_capacity((void **) &buf, (int) (total + nread + 1), &cap_i, 1, 0)) {
                    lv_free((void **) &buf);
                    return NULL;
                }
                cap = (int64_t) cap_i;
            }
            memcpy(buf + total, chunk, (size_t)nread);
            total += nread;
        }
        if (buf) buf[total] = '\0';
        if (out_size) *out_size = total;
        return buf;
    }

    char *buf = (char *)lv_malloc((size_t)sz + 1);
    if (!buf) return NULL;

    int64_t nread = lv_storage_read(storage, buf, sz);
    if (nread != sz) {
        lv_free((void **)&buf);
        return NULL;
    }
    buf[sz] = '\0';
    if (out_size) *out_size = sz;
    return buf;
}

bool lv_storage_write_all(lvStorage *storage, const void *data, int64_t size) {
    if (!storage || !storage->is_open || !data || size <= 0) return false;
    int64_t nwritten = lv_storage_write(storage, data, size);
    return nwritten == size;
}

/* ═══════════════════════════════════════════════════════════════════
 * 第 5 节：序列化注册表
 * ═══════════════════════════════════════════════════════════════════ */

/** @brief 序列化注册表条目 */
typedef struct {
    const char       *type_name;
    lvSerializeFunc   ser;
    lvDeserializeFunc deser;
} SerializeEntry;

/** @brief 最大序列化器数量 */
#define lv_SERIALIZE_MAX_ENTRIES 64

/** @brief 序列化注册表状态 */
static struct {
    SerializeEntry entries[lv_SERIALIZE_MAX_ENTRIES];
    int            count;
    lv_lazy_lock   lock;   /**< 惰性互斥锁：首次加锁时自动初始化 */
} s_serialize_registry = {0};

/** @brief 序列化注册表一次性初始化回调（首次加锁时执行一次） */
static void serialize_registry_init_once(void) {
    lv_mutex_init(&s_serialize_registry.lock.mutex);
    s_serialize_registry.count = 0;
}

bool lv_serialize_register(const char *type_name,
                            lvSerializeFunc ser,
                            lvDeserializeFunc deser) {
    if (!type_name) return false;

    lv_lazy_lock_lock(&s_serialize_registry.lock, serialize_registry_init_once);

    /* 检查类型是否已注册 */
    for (int i = 0; i < s_serialize_registry.count; i++) {
        if (strcmp(s_serialize_registry.entries[i].type_name, type_name) == 0) {
            lv_WARN("序列化器已注册，覆盖: %s", type_name);
            s_serialize_registry.entries[i].ser   = ser;
            s_serialize_registry.entries[i].deser = deser;
            lv_lazy_lock_unlock(&s_serialize_registry.lock);
            return true;
        }
    }

    if (s_serialize_registry.count >= lv_SERIALIZE_MAX_ENTRIES) {
        lv_ERROR("序列化注册表已满 (max=%d)", lv_SERIALIZE_MAX_ENTRIES);
        lv_lazy_lock_unlock(&s_serialize_registry.lock);
        return false;
    }

    s_serialize_registry.entries[s_serialize_registry.count].type_name = type_name;
    s_serialize_registry.entries[s_serialize_registry.count].ser       = ser;
    s_serialize_registry.entries[s_serialize_registry.count].deser     = deser;
    s_serialize_registry.count++;

    lv_lazy_lock_unlock(&s_serialize_registry.lock);
    return true;
}

/** @brief 根据类型名称查找序列化器 */
static SerializeEntry *find_serialize_entry(const char *type_name) {
    if (!type_name) return NULL;
    for (int i = 0; i < s_serialize_registry.count; i++) {
        if (strcmp(s_serialize_registry.entries[i].type_name, type_name) == 0) {
            return &s_serialize_registry.entries[i];
        }
    }
    return NULL;
}

bool lv_serialize_to_storage(const char *type_name,
                              const void *obj,
                              lvStorage *storage) {
    if (!type_name || !obj || !storage) return false;

    lv_lazy_lock_lock(&s_serialize_registry.lock, serialize_registry_init_once);
    SerializeEntry *entry = find_serialize_entry(type_name);
    lv_lazy_lock_unlock(&s_serialize_registry.lock);

    if (!entry) {
        lv_ERROR("未注册的序列化类型: %s", type_name);
        return false;
    }
    if (!entry->ser) {
        lv_ERROR("类型不支持序列化: %s", type_name);
        return false;
    }

    return entry->ser(obj, storage);
}

bool lv_deserialize_from_storage(const char *type_name,
                                  void *obj,
                                  lvStorage *storage) {
    if (!type_name || !obj || !storage) return false;

    lv_lazy_lock_lock(&s_serialize_registry.lock, serialize_registry_init_once);
    SerializeEntry *entry = find_serialize_entry(type_name);
    lv_lazy_lock_unlock(&s_serialize_registry.lock);

    if (!entry) {
        lv_ERROR("未注册的反序列化类型: %s", type_name);
        return false;
    }
    if (!entry->deser) {
        lv_ERROR("类型不支持反序列化: %s", type_name);
        return false;
    }

    return entry->deser(obj, storage);
}

bool lv_serialize_to_file(const char *type_name,
                           const void *obj,
                           const char *filepath) {
    if (!type_name || !obj || !filepath) return false;

    lvStorage *storage = lv_storage_open(filepath,
        lv_STORAGE_WRITE | lv_STORAGE_CREATE | lv_STORAGE_TRUNCATE | lv_STORAGE_BINARY);
    if (!storage) return false;

    bool ok = lv_serialize_to_storage(type_name, obj, storage);
    lv_storage_close(storage);
    return ok;
}

bool lv_deserialize_from_file(const char *type_name,
                               void *obj,
                               const char *filepath) {
    if (!type_name || !obj || !filepath) return false;

    lvStorage *storage = lv_storage_open(filepath, lv_STORAGE_READ | lv_STORAGE_BINARY);
    if (!storage) return false;

    bool ok = lv_deserialize_from_storage(type_name, obj, storage);
    lv_storage_close(storage);
    return ok;
}