/**
 * @file lv_ringbuf.h
 * @brief 泛型环形缓冲区
 *
 * 固定容量的环形缓冲区（循环队列），支持任意元素类型。
 * 满时新数据覆盖最旧数据。线程不安全，调用者需自行加锁。
 *
 * 从 debug.c 中的 lvLogRingBuffer 私有实现提取并泛化。
 * 日志专用环形缓冲区是此泛型模块的薄封装。
 */
#ifndef lv_RINGBUF_H
#define lv_RINGBUF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 泛型环形缓冲区 */
typedef struct {
    uint8_t *buffer;      /**< 数据缓冲区 */
    size_t elem_size;     /**< 每个元素的大小 */
    int capacity;         /**< 最大元素数 */
    int head;             /**< 写入位置 */
    int count;            /**< 当前元素数 */
} lvRingBuf;

/**
 * @brief 初始化环形缓冲区
 * @param rb         环形缓冲区指针
 * @param elem_size  元素大小
 * @param capacity   最大元素数（>= 1）
 * @return true 成功
 */
bool lv_ringbuf_init(lvRingBuf *rb, size_t elem_size, int capacity);

/** @brief 销毁环形缓冲区（释放内部缓冲区，不释放 rb 本身） */
void lv_ringbuf_destroy(lvRingBuf *rb);

/**
 * @brief 写入一个元素（覆盖最旧数据）
 * @param rb    环形缓冲区指针
 * @param elem  元素数据
 */
void lv_ringbuf_write(lvRingBuf *rb, const void *elem);

/**
 * @brief 读取第 i 个元素（0 = 最旧）
 * @param rb    环形缓冲区指针
 * @param index 索引
 * @param out   输出缓冲区
 * @return true 成功
 */
bool lv_ringbuf_read(const lvRingBuf *rb, int index, void *out);

/**
 * @brief 获取元素指针（直接访问）
 * @param rb    环形缓冲区指针
 * @param index 索引
 * @return 元素指针，越界返回 NULL
 */
void *lv_ringbuf_get(const lvRingBuf *rb, int index);

/** @brief 清空环形缓冲区 */
void lv_ringbuf_clear(lvRingBuf *rb);

/** @brief 调整容量（丢失最旧数据） */
bool lv_ringbuf_resize(lvRingBuf *rb, int new_capacity);

/** @brief 获取元素数量 */
static inline int lv_ringbuf_count(const lvRingBuf *rb) { return rb ? rb->count : 0; }

/** @brief 获取容量 */
static inline int lv_ringbuf_capacity(const lvRingBuf *rb) { return rb ? rb->capacity : 0; }

#ifdef __cplusplus
}
#endif

#endif /* lv_RINGBUF_H */
