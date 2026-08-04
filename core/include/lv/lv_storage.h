/**
 * @file lv_storage.h
 * @brief 统一存储抽象层 —— 文件系统/内存缓冲区/可扩展后端
 *
 * @details 提供统一的存储 I/O 接口，支持 file:// URI、mem:// URI 和普通路径，
 *          以及通过后端注册表扩展自定义 URI scheme。
 *
 *          解决的问题：
 *          - 7+ 个独立的序列化模块各有各的 I/O 方式，缺乏统一的存储接口
 *          - fopen/fread/fwrite 与内存缓冲区两种模式需要不同的 API
 *          - 缺乏 URI scheme 驱动的后端自动选择机制
 *          - 缺乏便利的 read_all/write_all 工具函数
 *
 * 使用方式：
 *   @code
 *   lvStorage *s = lv_storage_open("file:///tmp/data.bin", lv_STORAGE_READ | lv_STORAGE_BINARY);
 *   if (s) {
 *       char buf[256];
 *       lv_storage_read(s, buf, sizeof(buf));
 *       lv_storage_close(s);
 *   }
 *   @endcode
 *
 * @version 1.0.0
 * @date   2026-08-04
 */

#ifndef lv_STORAGE_H
#define lv_STORAGE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "lv_platform.h"

#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════
 * 第 1 节：存储打开模式与定位起点
 * ═══════════════════════════════════════════════════════════════════ */

/** @brief 存储打开模式 */
typedef enum {
    lv_STORAGE_READ     = 1 << 0,  /**< 读模式 */
    lv_STORAGE_WRITE    = 1 << 1,  /**< 写模式 */
    lv_STORAGE_CREATE   = 1 << 2,  /**< 不存在时创建 */
    lv_STORAGE_TRUNCATE = 1 << 3,  /**< 打开时截断 */
    lv_STORAGE_APPEND   = 1 << 4,  /**< 追加模式 */
    lv_STORAGE_BINARY   = 1 << 5,  /**< 二进制模式 */
    lv_STORAGE_TEXT     = 1 << 6,  /**< 文本模式 */
} lvStorageMode;

/** @brief 存储定位起点 */
typedef enum {
    lv_STORAGE_SEEK_SET = 0,  /**< 文件开头 */
    lv_STORAGE_SEEK_CUR,      /**< 当前位置 */
    lv_STORAGE_SEEK_END,      /**< 文件末尾 */
} lvStorageSeekOrigin;

/* ═══════════════════════════════════════════════════════════════════
 * 第 2 节：存储后端虚表与句柄
 * ═══════════════════════════════════════════════════════════════════ */

/** @brief 存储后端操作虚表 */
typedef struct lvStorageOps {
    bool     (*open)(void *ctx, const char *uri, int mode);
    void     (*close)(void *ctx);
    int64_t  (*read)(void *ctx, void *buf, int64_t size);
    int64_t  (*write)(void *ctx, const void *buf, int64_t size);
    int64_t  (*seek)(void *ctx, int64_t offset, lvStorageSeekOrigin origin);
    int64_t  (*tell)(void *ctx);
    int64_t  (*size)(void *ctx);
    bool     (*flush)(void *ctx);
    bool     (*eof)(void *ctx);
} lvStorageOps;

/** @brief 存储句柄 */
typedef struct lvStorage {
    const lvStorageOps *ops;        /**< 后端操作虚表 */
    void               *ctx;        /**< 后端特定上下文 */
    char                uri[512];   /**< 当前 URI */
    int                 mode;       /**< 打开模式 */
    bool                is_open;    /**< 是否已打开 */
} lvStorage;

/** @brief 存储后端注册信息 */
typedef struct lvStorageBackendInfo {
    const char        *scheme;      /**< URI scheme（如 "file", "mem", "http"） */
    const lvStorageOps *ops;        /**< 后端操作虚表 */
    void *(*create_ctx)(void);      /**< 创建后端上下文 */
    void  (*destroy_ctx)(void *ctx);/**< 销毁后端上下文 */
} lvStorageBackendInfo;

/* ═══════════════════════════════════════════════════════════════════
 * 第 3 节：公共 API
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * @brief 打开存储（自动根据 URI scheme 选择后端）
 * @param uri  统一资源标识符（如 "file:///tmp/data.bin", "mem://tmp", 或普通路径）
 * @param mode 打开模式（lvStorageMode 的位组合）
 * @return 存储句柄，失败返回 NULL
 */
lv_PUBLIC_API lvStorage *lv_storage_open(const char *uri, int mode);

/**
 * @brief 关闭存储并释放资源
 * @param storage 存储句柄（可 NULL）
 */
lv_PUBLIC_API void lv_storage_close(lvStorage *storage);

/**
 * @brief 从存储读取数据
 * @param storage 存储句柄
 * @param buf     读取缓冲区
 * @param size    读取字节数
 * @return 实际读取的字节数，失败返回 -1
 */
lv_PUBLIC_API int64_t lv_storage_read(lvStorage *storage, void *buf, int64_t size);

/**
 * @brief 向存储写入数据
 * @param storage 存储句柄
 * @param buf     写入数据
 * @param size    写入字节数
 * @return 实际写入的字节数，失败返回 -1
 */
lv_PUBLIC_API int64_t lv_storage_write(lvStorage *storage, const void *buf, int64_t size);

/**
 * @brief 定位存储读写位置
 * @param storage 存储句柄
 * @param offset  偏移量（字节）
 * @param origin  定位起点
 * @return 定位后的绝对位置，失败返回 -1
 */
lv_PUBLIC_API int64_t lv_storage_seek(lvStorage *storage, int64_t offset, lvStorageSeekOrigin origin);

/**
 * @brief 获取当前读写位置
 * @param storage 存储句柄
 * @return 当前读写位置，失败返回 -1
 */
lv_PUBLIC_API int64_t lv_storage_tell(lvStorage *storage);

/**
 * @brief 获取存储大小
 * @param storage 存储句柄
 * @return 总大小（字节），失败返回 -1
 */
lv_PUBLIC_API int64_t lv_storage_size(lvStorage *storage);

/**
 * @brief 刷新缓冲区
 * @param storage 存储句柄
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_storage_flush(lvStorage *storage);

/**
 * @brief 检查是否到达存储末尾
 * @param storage 存储句柄
 * @return true 已到达末尾，false 未到达
 */
lv_PUBLIC_API bool lv_storage_eof(lvStorage *storage);

/**
 * @brief 检查存储是否已打开
 * @param storage 存储句柄
 * @return true 已打开，false 未打开
 */
lv_PUBLIC_API bool lv_storage_is_open(const lvStorage *storage);

/**
 * @brief 获取存储 URI
 * @param storage 存储句柄
 * @return URI 字符串
 */
lv_PUBLIC_API const char *lv_storage_get_uri(const lvStorage *storage);

/**
 * @brief 读取整个存储内容到内存缓冲区（便利函数）
 * @param storage 存储句柄
 * @param out_size 输出：读取的字节数（可 NULL）
 * @return 堆分配的缓冲区（调用者需 lv_free），失败返回 NULL
 */
lv_PUBLIC_API char *lv_storage_read_all(lvStorage *storage, int64_t *out_size);

/**
 * @brief 将数据全部写入存储（便利函数）
 * @param storage 存储句柄
 * @param data    数据指针
 * @param size    数据大小（字节）
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_storage_write_all(lvStorage *storage, const void *data, int64_t size);

/**
 * @brief 注册自定义存储后端
 * @param info 后端注册信息（scheme 和 ops 必须非 NULL）
 * @return true 注册成功，false 失败（scheme 重复或参数无效）
 *
 * @note 注册的后端在 lv_storage_system_cleanup 时自动注销。
 *       如果 scheme 已存在，注册失败并返回 false。
 */
lv_PUBLIC_API bool lv_storage_register_backend(const lvStorageBackendInfo *info);

/**
 * @brief 初始化存储系统（注册内置后端）
 *
 * 自动注册 file:// 和 mem:// 两个内置后端。
 * 可在程序启动时调用一次，或由 lv_storage_open 延迟初始化。
 */
lv_PUBLIC_API void lv_storage_system_init(void);

/**
 * @brief 清理存储系统（释放所有注册的后端）
 */
lv_PUBLIC_API void lv_storage_system_cleanup(void);

/* ═══════════════════════════════════════════════════════════════════
 * 第 4 节：序列化注册表扩展
 *
 * 提供类型驱动的序列化/反序列化接口，将类型名映射到对应的
 * 序列化/反序列化函数。
 * ═══════════════════════════════════════════════════════════════════ */

/** @brief 序列化函数类型 */
typedef bool (*lvSerializeFunc)(const void *obj, lvStorage *storage);

/** @brief 反序列化函数类型 */
typedef bool (*lvDeserializeFunc)(void *obj, lvStorage *storage);

/**
 * @brief 注册类型序列化器
 * @param type_name 类型名称（如 "ConstraintGraph", "Module"）
 * @param ser       序列化函数（可 NULL，表示不支持序列化）
 * @param deser     反序列化函数（可 NULL，表示不支持反序列化）
 * @return true 注册成功，false 失败
 */
lv_PUBLIC_API bool lv_serialize_register(const char *type_name,
                                          lvSerializeFunc ser,
                                          lvDeserializeFunc deser);

/**
 * @brief 通用序列化：将对象序列化到存储
 * @param type_name 类型名称
 * @param obj       对象指针
 * @param storage   存储句柄
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_serialize_to_storage(const char *type_name,
                                            const void *obj,
                                            lvStorage *storage);

/**
 * @brief 通用反序列化：从存储反序列化对象
 * @param type_name 类型名称
 * @param obj       对象指针（输出）
 * @param storage   存储句柄
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_deserialize_from_storage(const char *type_name,
                                                void *obj,
                                                lvStorage *storage);

/**
 * @brief 便利函数：序列化到文件
 * @param type_name 类型名称
 * @param obj       对象指针
 * @param filepath  文件路径
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_serialize_to_file(const char *type_name,
                                         const void *obj,
                                         const char *filepath);

/**
 * @brief 便利函数：从文件反序列化
 * @param type_name 类型名称
 * @param obj       对象指针（输出）
 * @param filepath  文件路径
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_deserialize_from_file(const char *type_name,
                                             void *obj,
                                             const char *filepath);

#ifdef __cplusplus
}
#endif

#endif /* lv_STORAGE_H */