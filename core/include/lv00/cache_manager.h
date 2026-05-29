/**
 * @file cache_manager.h
 * @brief Lv-00/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/**/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 */**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**</**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOL/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**</**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**</**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* =========================================================/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**</**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**</**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    L/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /*/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /*/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /*/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**</**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext */**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /*/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /*/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /*/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * =================================================/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**</**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**</**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00Cache/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    L/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
    
    /* 全局统计 */
    Lv00CacheStats global_stats;
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
    
    /* 全局统计 */
    Lv00CacheStats global_stats;
    
    /* 线程安全 */
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
    
    /* 全局统计 */
    Lv00CacheStats global_stats;
    
    /* 线程安全 */
    void *mutex;                 /**< 互斥锁 */
    
    /* 运行/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
    
    /* 全局统计 */
    Lv00CacheStats global_stats;
    
    /* 线程安全 */
    void *mutex;                 /**< 互斥锁 */
    
    /* 运行状态 */
    bool is_running;
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
    
    /* 全局统计 */
    Lv00CacheStats global_stats;
    
    /* 线程安全 */
    void *mutex;                 /**< 互斥锁 */
    
    /* 运行状态 */
    bool is_running;
    uint64_t start_time;
};
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
    
    /* 全局统计 */
    Lv00CacheStats global_stats;
    
    /* 线程安全 */
    void *mutex;                 /**< 互斥锁 */
    
    /* 运行状态 */
    bool is_running;
    uint64_t start_time;
};

/* ============================================================
 * 生命周期管理 API
 * ============================================================ */

/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
    
    /* 全局统计 */
    Lv00CacheStats global_stats;
    
    /* 线程安全 */
    void *mutex;                 /**< 互斥锁 */
    
    /* 运行状态 */
    bool is_running;
    uint64_t start_time;
};

/* ============================================================
 * 生命周期管理 API
 * ============================================================ */

/**
 * @brief 创建缓存管理器
 *
 * @param config 配置（可为 NULL，使用默认配置）
 */**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
    
    /* 全局统计 */
    Lv00CacheStats global_stats;
    
    /* 线程安全 */
    void *mutex;                 /**< 互斥锁 */
    
    /* 运行状态 */
    bool is_running;
    uint64_t start_time;
};

/* ============================================================
 * 生命周期管理 API
 * ============================================================ */

/**
 * @brief 创建缓存管理器
 *
 * @param config 配置（可为 NULL，使用默认配置）
 * @return 新创建的缓存管理器，失败返回 NULL
 */
LV00_PUBLIC_API L/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
    
    /* 全局统计 */
    Lv00CacheStats global_stats;
    
    /* 线程安全 */
    void *mutex;                 /**< 互斥锁 */
    
    /* 运行状态 */
    bool is_running;
    uint64_t start_time;
};

/* ============================================================
 * 生命周期管理 API
 * ============================================================ */

/**
 * @brief 创建缓存管理器
 *
 * @param config 配置（可为 NULL，使用默认配置）
 * @return 新创建的缓存管理器，失败返回 NULL
 */
LV00_PUBLIC_API Lv00CacheManager *lv00_cache_manager_create/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
    
    /* 全局统计 */
    Lv00CacheStats global_stats;
    
    /* 线程安全 */
    void *mutex;                 /**< 互斥锁 */
    
    /* 运行状态 */
    bool is_running;
    uint64_t start_time;
};

/* ============================================================
 * 生命周期管理 API
 * ============================================================ */

/**
 * @brief 创建缓存管理器
 *
 * @param config 配置（可为 NULL，使用默认配置）
 * @return 新创建的缓存管理器，失败返回 NULL
 */
LV00_PUBLIC_API Lv00CacheManager *lv00_cache_manager_create(const Lv00CacheConfig *config);

/**
 * @brief 销毁缓存管理/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
    
    /* 全局统计 */
    Lv00CacheStats global_stats;
    
    /* 线程安全 */
    void *mutex;                 /**< 互斥锁 */
    
    /* 运行状态 */
    bool is_running;
    uint64_t start_time;
};

/* ============================================================
 * 生命周期管理 API
 * ============================================================ */

/**
 * @brief 创建缓存管理器
 *
 * @param config 配置（可为 NULL，使用默认配置）
 * @return 新创建的缓存管理器，失败返回 NULL
 */
LV00_PUBLIC_API Lv00CacheManager *lv00_cache_manager_create(const Lv00CacheConfig *config);

/**
 * @brief 销毁缓存管理器
 *
 * @param manager 缓存/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
    
    /* 全局统计 */
    Lv00CacheStats global_stats;
    
    /* 线程安全 */
    void *mutex;                 /**< 互斥锁 */
    
    /* 运行状态 */
    bool is_running;
    uint64_t start_time;
};

/* ============================================================
 * 生命周期管理 API
 * ============================================================ */

/**
 * @brief 创建缓存管理器
 *
 * @param config 配置（可为 NULL，使用默认配置）
 * @return 新创建的缓存管理器，失败返回 NULL
 */
LV00_PUBLIC_API Lv00CacheManager *lv00_cache_manager_create(const Lv00CacheConfig *config);

/**
 * @brief 销毁缓存管理器
 *
 * @param manager 缓存管理器（可为 NULL）
 */
/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
    
    /* 全局统计 */
    Lv00CacheStats global_stats;
    
    /* 线程安全 */
    void *mutex;                 /**< 互斥锁 */
    
    /* 运行状态 */
    bool is_running;
    uint64_t start_time;
};

/* ============================================================
 * 生命周期管理 API
 * ============================================================ */

/**
 * @brief 创建缓存管理器
 *
 * @param config 配置（可为 NULL，使用默认配置）
 * @return 新创建的缓存管理器，失败返回 NULL
 */
LV00_PUBLIC_API Lv00CacheManager *lv00_cache_manager_create(const Lv00CacheConfig *config);

/**
 * @brief 销毁缓存管理器
 *
 * @param manager 缓存管理器（可为 NULL）
 */
LV00_PUBLIC_API void lv00_cache_manager_destroy(Lv00CacheManager *manager);

/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
    
    /* 全局统计 */
    Lv00CacheStats global_stats;
    
    /* 线程安全 */
    void *mutex;                 /**< 互斥锁 */
    
    /* 运行状态 */
    bool is_running;
    uint64_t start_time;
};

/* ============================================================
 * 生命周期管理 API
 * ============================================================ */

/**
 * @brief 创建缓存管理器
 *
 * @param config 配置（可为 NULL，使用默认配置）
 * @return 新创建的缓存管理器，失败返回 NULL
 */
LV00_PUBLIC_API Lv00CacheManager *lv00_cache_manager_create(const Lv00CacheConfig *config);

/**
 * @brief 销毁缓存管理器
 *
 * @param manager 缓存管理器（可为 NULL）
 */
LV00_PUBLIC_API void lv00_cache_manager_destroy(Lv00CacheManager *manager);

/**
 * @brief 验证缓存管理器有效性
 *
 * @param manager 缓存/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
    
    /* 全局统计 */
    Lv00CacheStats global_stats;
    
    /* 线程安全 */
    void *mutex;                 /**< 互斥锁 */
    
    /* 运行状态 */
    bool is_running;
    uint64_t start_time;
};

/* ============================================================
 * 生命周期管理 API
 * ============================================================ */

/**
 * @brief 创建缓存管理器
 *
 * @param config 配置（可为 NULL，使用默认配置）
 * @return 新创建的缓存管理器，失败返回 NULL
 */
LV00_PUBLIC_API Lv00CacheManager *lv00_cache_manager_create(const Lv00CacheConfig *config);

/**
 * @brief 销毁缓存管理器
 *
 * @param manager 缓存管理器（可为 NULL）
 */
LV00_PUBLIC_API void lv00_cache_manager_destroy(Lv00CacheManager *manager);

/**
 * @brief 验证缓存管理器有效性
 *
 * @param manager 缓存管理器
 * @return true 有效，/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
    
    /* 全局统计 */
    Lv00CacheStats global_stats;
    
    /* 线程安全 */
    void *mutex;                 /**< 互斥锁 */
    
    /* 运行状态 */
    bool is_running;
    uint64_t start_time;
};

/* ============================================================
 * 生命周期管理 API
 * ============================================================ */

/**
 * @brief 创建缓存管理器
 *
 * @param config 配置（可为 NULL，使用默认配置）
 * @return 新创建的缓存管理器，失败返回 NULL
 */
LV00_PUBLIC_API Lv00CacheManager *lv00_cache_manager_create(const Lv00CacheConfig *config);

/**
 * @brief 销毁缓存管理器
 *
 * @param manager 缓存管理器（可为 NULL）
 */
LV00_PUBLIC_API void lv00_cache_manager_destroy(Lv00CacheManager *manager);

/**
 * @brief 验证缓存管理器有效性
 *
 * @param manager 缓存管理器
 * @return true 有效，false 无效
 */
LV00_PUBLIC_API/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
    
    /* 全局统计 */
    Lv00CacheStats global_stats;
    
    /* 线程安全 */
    void *mutex;                 /**< 互斥锁 */
    
    /* 运行状态 */
    bool is_running;
    uint64_t start_time;
};

/* ============================================================
 * 生命周期管理 API
 * ============================================================ */

/**
 * @brief 创建缓存管理器
 *
 * @param config 配置（可为 NULL，使用默认配置）
 * @return 新创建的缓存管理器，失败返回 NULL
 */
LV00_PUBLIC_API Lv00CacheManager *lv00_cache_manager_create(const Lv00CacheConfig *config);

/**
 * @brief 销毁缓存管理器
 *
 * @param manager 缓存管理器（可为 NULL）
 */
LV00_PUBLIC_API void lv00_cache_manager_destroy(Lv00CacheManager *manager);

/**
 * @brief 验证缓存管理器有效性
 *
 * @param manager 缓存管理器
 * @return true 有效，false 无效
 */
LV00_PUBLIC_API bool lv00_cache_manager_is_valid(const Lv/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
    
    /* 全局统计 */
    Lv00CacheStats global_stats;
    
    /* 线程安全 */
    void *mutex;                 /**< 互斥锁 */
    
    /* 运行状态 */
    bool is_running;
    uint64_t start_time;
};

/* ============================================================
 * 生命周期管理 API
 * ============================================================ */

/**
 * @brief 创建缓存管理器
 *
 * @param config 配置（可为 NULL，使用默认配置）
 * @return 新创建的缓存管理器，失败返回 NULL
 */
LV00_PUBLIC_API Lv00CacheManager *lv00_cache_manager_create(const Lv00CacheConfig *config);

/**
 * @brief 销毁缓存管理器
 *
 * @param manager 缓存管理器（可为 NULL）
 */
LV00_PUBLIC_API void lv00_cache_manager_destroy(Lv00CacheManager *manager);

/**
 * @brief 验证缓存管理器有效性
 *
 * @param manager 缓存管理器
 * @return true 有效，false 无效
 */
LV00_PUBLIC_API bool lv00_cache_manager_is_valid(const Lv00CacheManager *manager);

/**
 * @brief 重置缓存管理器
 *
 * 清空所有缓存条目和上下文。
 *
 * @param manager 缓存管理/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
    
    /* 全局统计 */
    Lv00CacheStats global_stats;
    
    /* 线程安全 */
    void *mutex;                 /**< 互斥锁 */
    
    /* 运行状态 */
    bool is_running;
    uint64_t start_time;
};

/* ============================================================
 * 生命周期管理 API
 * ============================================================ */

/**
 * @brief 创建缓存管理器
 *
 * @param config 配置（可为 NULL，使用默认配置）
 * @return 新创建的缓存管理器，失败返回 NULL
 */
LV00_PUBLIC_API Lv00CacheManager *lv00_cache_manager_create(const Lv00CacheConfig *config);

/**
 * @brief 销毁缓存管理器
 *
 * @param manager 缓存管理器（可为 NULL）
 */
LV00_PUBLIC_API void lv00_cache_manager_destroy(Lv00CacheManager *manager);

/**
 * @brief 验证缓存管理器有效性
 *
 * @param manager 缓存管理器
 * @return true 有效，false 无效
 */
LV00_PUBLIC_API bool lv00_cache_manager_is_valid(const Lv00CacheManager *manager);

/**
 * @brief 重置缓存管理器
 *
 * 清空所有缓存条目和上下文。
 *
 * @param manager 缓存管理器
 * @return LV00_OK 成功/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
    
    /* 全局统计 */
    Lv00CacheStats global_stats;
    
    /* 线程安全 */
    void *mutex;                 /**< 互斥锁 */
    
    /* 运行状态 */
    bool is_running;
    uint64_t start_time;
};

/* ============================================================
 * 生命周期管理 API
 * ============================================================ */

/**
 * @brief 创建缓存管理器
 *
 * @param config 配置（可为 NULL，使用默认配置）
 * @return 新创建的缓存管理器，失败返回 NULL
 */
LV00_PUBLIC_API Lv00CacheManager *lv00_cache_manager_create(const Lv00CacheConfig *config);

/**
 * @brief 销毁缓存管理器
 *
 * @param manager 缓存管理器（可为 NULL）
 */
LV00_PUBLIC_API void lv00_cache_manager_destroy(Lv00CacheManager *manager);

/**
 * @brief 验证缓存管理器有效性
 *
 * @param manager 缓存管理器
 * @return true 有效，false 无效
 */
LV00_PUBLIC_API bool lv00_cache_manager_is_valid(const Lv00CacheManager *manager);

/**
 * @brief 重置缓存管理器
 *
 * 清空所有缓存条目和上下文。
 *
 * @param manager 缓存管理器
 * @return LV00_OK 成功，其他错误码表示失败
 */
LV/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
    
    /* 全局统计 */
    Lv00CacheStats global_stats;
    
    /* 线程安全 */
    void *mutex;                 /**< 互斥锁 */
    
    /* 运行状态 */
    bool is_running;
    uint64_t start_time;
};

/* ============================================================
 * 生命周期管理 API
 * ============================================================ */

/**
 * @brief 创建缓存管理器
 *
 * @param config 配置（可为 NULL，使用默认配置）
 * @return 新创建的缓存管理器，失败返回 NULL
 */
LV00_PUBLIC_API Lv00CacheManager *lv00_cache_manager_create(const Lv00CacheConfig *config);

/**
 * @brief 销毁缓存管理器
 *
 * @param manager 缓存管理器（可为 NULL）
 */
LV00_PUBLIC_API void lv00_cache_manager_destroy(Lv00CacheManager *manager);

/**
 * @brief 验证缓存管理器有效性
 *
 * @param manager 缓存管理器
 * @return true 有效，false 无效
 */
LV00_PUBLIC_API bool lv00_cache_manager_is_valid(const Lv00CacheManager *manager);

/**
 * @brief 重置缓存管理器
 *
 * 清空所有缓存条目和上下文。
 *
 * @param manager 缓存管理器
 * @return LV00_OK 成功，其他错误码表示失败
 */
LV00_PUBLIC_API Lv00ErrorCode lv00/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
    
    /* 全局统计 */
    Lv00CacheStats global_stats;
    
    /* 线程安全 */
    void *mutex;                 /**< 互斥锁 */
    
    /* 运行状态 */
    bool is_running;
    uint64_t start_time;
};

/* ============================================================
 * 生命周期管理 API
 * ============================================================ */

/**
 * @brief 创建缓存管理器
 *
 * @param config 配置（可为 NULL，使用默认配置）
 * @return 新创建的缓存管理器，失败返回 NULL
 */
LV00_PUBLIC_API Lv00CacheManager *lv00_cache_manager_create(const Lv00CacheConfig *config);

/**
 * @brief 销毁缓存管理器
 *
 * @param manager 缓存管理器（可为 NULL）
 */
LV00_PUBLIC_API void lv00_cache_manager_destroy(Lv00CacheManager *manager);

/**
 * @brief 验证缓存管理器有效性
 *
 * @param manager 缓存管理器
 * @return true 有效，false 无效
 */
LV00_PUBLIC_API bool lv00_cache_manager_is_valid(const Lv00CacheManager *manager);

/**
 * @brief 重置缓存管理器
 *
 * 清空所有缓存条目和上下文。
 *
 * @param manager 缓存管理器
 * @return LV00_OK 成功，其他错误码表示失败
 */
LV00_PUBLIC_API Lv00ErrorCode lv00_cache_manager_reset(Lv00CacheManager *manager);

/* ============================================================
 */**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
    
    /* 全局统计 */
    Lv00CacheStats global_stats;
    
    /* 线程安全 */
    void *mutex;                 /**< 互斥锁 */
    
    /* 运行状态 */
    bool is_running;
    uint64_t start_time;
};

/* ============================================================
 * 生命周期管理 API
 * ============================================================ */

/**
 * @brief 创建缓存管理器
 *
 * @param config 配置（可为 NULL，使用默认配置）
 * @return 新创建的缓存管理器，失败返回 NULL
 */
LV00_PUBLIC_API Lv00CacheManager *lv00_cache_manager_create(const Lv00CacheConfig *config);

/**
 * @brief 销毁缓存管理器
 *
 * @param manager 缓存管理器（可为 NULL）
 */
LV00_PUBLIC_API void lv00_cache_manager_destroy(Lv00CacheManager *manager);

/**
 * @brief 验证缓存管理器有效性
 *
 * @param manager 缓存管理器
 * @return true 有效，false 无效
 */
LV00_PUBLIC_API bool lv00_cache_manager_is_valid(const Lv00CacheManager *manager);

/**
 * @brief 重置缓存管理器
 *
 * 清空所有缓存条目和上下文。
 *
 * @param manager 缓存管理器
 * @return LV00_OK 成功，其他错误码表示失败
 */
LV00_PUBLIC_API Lv00ErrorCode lv00_cache_manager_reset(Lv00CacheManager *manager);

/* ============================================================
 * 基本缓存操作 API
 * ============================================================/**
 * @file cache_manager.h
 * @brief Lv-00 缓存隔离层管理器
 * @details 实现上下文状态隔离，避免多次连续运算造成上下文状态污染。
 *          提供缓存LRU淘汰机制，支持大容量数据存储和复杂拓扑数据收纳。
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 设计原则
 * - 上下文隔离：每个运算上下文拥有独立的缓存空间
 * - LRU淘汰：当缓存达到上限时，自动淘汰最久未使用的条目
 * - 数据分块：支持大容量数据的分块存储，避免冗余数据堆积
 * - 线程安全：通过上下文隔离实现线程安全
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)，被 Layer 3+ 依赖。
 */

#ifndef LV00_CACHE_MANAGER_H
#define LV00_CACHE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"
#include "cross_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** @brief 缓存管理器版本号 */
#define LV00_CACHE_MANAGER_VERSION 1

/** @brief 缓存魔法数 */
#define LV00_CACHE_MAGIC 0x4C5630304348ULL /* "LV00CH" */

/** @brief 默认最大缓存条目数 */
#define LV00_CACHE_DEFAULT_MAX_ENTRIES 1024

/** @brief 默认缓存条目最大大小（字节） */
#define LV00_CACHE_DEFAULT_MAX_ENTRY_SIZE (1024 * 1024) /* 1MB */

/** @brief 默认缓存总大小限制（字节） */
#define LV00_CACHE_DEFAULT_TOTAL_SIZE (64 * 1024 * 1024) /* 64MB */

/** @brief 数据块大小 */
#define LV00_CACHE_BLOCK_SIZE (64 * 1024) /* 64KB */

/** @brief 最大数据块数 */
#define LV00_CACHE_MAX_BLOCKS 1024

/** @brief 缓存键最大长度 */
#define LV00_CACHE_MAX_KEY_LEN 256

/** @brief 缓存标签最大数量 */
#define LV00_CACHE_MAX_TAGS 16

/* ============================================================
 * 缓存条目类型枚举
 * ============================================================ */

typedef enum {
    CACHE_ENTRY_TYPE_INVALID = 0,    /**< 无效类型 */
    CACHE_ENTRY_TYPE_RAW,            /**< 原始二进制数据 */
    CACHE_ENTRY_TYPE_GEOM_NODE,      /**< 几何节点 */
    CACHE_ENTRY_TYPE_CONSTRAINT,     /**< 约束 */
    CACHE_ENTRY_TYPE_SYMBOLIC_COORD, /**< 符号坐标 */
    CACHE_ENTRY_TYPE_PROOF_STEP,     /**< 证明步骤 */
    CACHE_ENTRY_TYPE_SOLVER_STATE,   /**< 求解器状态 */
    CACHE_ENTRY_TYPE_TOPOLOGY,       /**< 拓扑数据 */
    CACHE_ENTRY_TYPE_CUSTOM          /**< 自定义类型 */
} Lv00CacheEntryType;

/* ============================================================
 * 缓存策略枚举
 * ============================================================ */

typedef enum {
    CACHE_POLICY_LRU = 0,        /**< 最近最少使用 */
    CACHE_POLICY_LFU,            /**< 最少使用频率 */
    CACHE_POLICY_FIFO,           /**< 先进先出 */
    CACHE_POLICY_TTL,            /**< 生存时间 */
    CACHE_POLICY_ADAPTIVE        /**< 自适应策略 */
} Lv00CachePolicy;

/* ============================================================
 * 缓存统计结构
 * ============================================================ */

typedef struct {
    uint64_t total_hits;         /**< 总命中次数 */
    uint64_t total_misses;       /**< 总未命中次数 */
    uint64_t total_evictions;    /**< 总淘汰次数 */
    uint64_t total_insertions;   /**< 总插入次数 */
    uint64_t total_removals;     /**< 总删除次数 */
    uint64_t current_size;       /**< 当前缓存大小（字节） */
    uint32_t current_entries;    /**< 当前条目数 */
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    double hit_rate;             /**< 命中率 */
    double avg_access_time_ms;   /**< 平均访问时间（毫秒） */
} Lv00CacheStats;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct Lv00CacheManager Lv00CacheManager;
typedef struct Lv00CacheEntry Lv00CacheEntry;
typedef struct Lv00CacheContext Lv00CacheContext;
typedef struct Lv00CacheBlock Lv00CacheBlock;

/* ============================================================
 * 数据块结构（用于大容量数据分块存储）
 * ============================================================ */

struct Lv00CacheBlock {
    uint32_t block_id;           /**< 块ID */
    uint32_t ref_count;          /**< 引用计数 */
    size_t used_size;            /**< 已使用大小 */
    uint8_t *data;               /**< 数据指针 */
    Lv00CacheBlock *next;        /**< 下一块 */
    Lv00CacheBlock *prev;        /**< 前一块 */
    bool is_dirty;               /**< 脏标记 */
};

/* ============================================================
 * 缓存条目结构
 * ============================================================ */

struct Lv00CacheEntry {
    char key[LV00_CACHE_MAX_KEY_LEN]; /**< 缓存键 */
    Lv00CacheEntryType type;          /**< 条目类型 */
    
    /* 数据存储 */
    union {
        /* 小数据直接存储 */
        struct {
            void *data;
            size_t size;
        } raw;
        
        /* 大数据分块存储 */
        struct {
            Lv00CacheBlock *first_block;  /**< 第一块 */
            Lv00CacheBlock *last_block;   /**< 最后一块 */
            uint32_t block_count;         /**< 块数量 */
        } chunked;
    } storage;
    
    bool is_chunked;             /**< 是否分块存储 */
    
    /* LRU链表指针 */
    Lv00CacheEntry *lru_next;
    Lv00CacheEntry *lru_prev;
    
    /* 哈希表指针 */
    Lv00CacheEntry *hash_next;
    
    /* 元数据 */
    uint64_t access_count;       /**< 访问计数 */
    uint64_t last_access_time;   /**< 最后访问时间 */
    uint64_t creation_time;      /**< 创建时间 */
    uint32_t ttl_seconds;        /**< 生存时间（秒，0表示永久） */
    uint32_t priority;           /**< 优先级 */
    
    /* 标签 */
    char tags[LV00_CACHE_MAX_TAGS][32];
    uint32_t tag_count;
    
    /* 上下文关联 */
    Lv00CacheContext *context;   /**< 所属上下文 */
    
    /* 自定义析构函数 */
    void (*destructor)(void *data);
};

/* ============================================================
 * 缓存上下文结构（实现上下文隔离）
 * ============================================================ */

struct Lv00CacheContext {
    uint32_t context_id;         /**< 上下文ID */
    char name[64];               /**< 上下文名称 */
    
    /* 统计信息 */
    Lv00CacheStats stats;
    
    /* 父上下文（用于上下文继承） */
    Lv00CacheContext *parent;
    
    /* 子上下文链表 */
    Lv00CacheContext *first_child;
    Lv00CacheContext *next_sibling;
    
    /* 隔离标记 */
    bool is_isolated;            /**< 是否完全隔离 */
    bool inherit_from_parent;    /**< 是否继承父上下文缓存 */
    
    /* 私有数据 */
    void *user_data;
};

/* ============================================================
 * 缓存管理器配置
 * ============================================================ */

typedef struct {
    uint32_t max_entries;        /**< 最大条目数 */
    uint64_t max_size;           /**< 最大缓存大小（字节） */
    uint64_t max_entry_size;     /**< 单个条目最大大小 */
    Lv00CachePolicy policy;      /**< 缓存策略 */
    uint32_t default_ttl;        /**< 默认生存时间（秒） */
    bool enable_compression;     /**< 是否启用压缩 */
    bool enable_persistence;     /**< 是否启用持久化 */
    float eviction_threshold;    /**< 淘汰阈值（0.0-1.0） */
} Lv00CacheConfig;

/* ============================================================
 * 缓存管理器结构
 * ============================================================ */

struct Lv00CacheManager {
    uint64_t magic;              /**< 魔法数 */
    uint32_t version;            /**< 版本号 */
    
    /* 配置 */
    Lv00CacheConfig config;
    
    /* 哈希表（按key查找） */
    Lv00CacheEntry **hash_table;
    uint32_t hash_table_size;
    
    /* LRU链表头尾 */
    Lv00CacheEntry *lru_head;    /**< 最近使用 */
    Lv00CacheEntry *lru_tail;    /**< 最久未使用 */
    
    /* 数据块池 */
    Lv00CacheBlock *block_pool;
    uint32_t block_pool_size;
    
    /* 上下文管理 */
    Lv00CacheContext *default_context;
    Lv00CacheContext *current_context;
    uint32_t next_context_id;
    
    /* 全局统计 */
    Lv00CacheStats global_stats;
    
    /* 线程安全 */
    void *mutex;                 /**< 互斥锁 */
    
    /* 运行状态 */
    bool is_running;
    uint64_t start_time;
};

/* ============================================================
 * 生命周期管理 API
 * ============================================================ */

/**
 * @brief 创建缓存管理器
 *
 * @param config 配置（可为 NULL，使用默认配置）
 * @return 新创建的缓存管理器，失败返回 NULL
 */
LV00_PUBLIC_API Lv00CacheManager *lv00_cache_manager_create(const Lv00CacheConfig *config);

/**
 * @brief 销毁缓存管理器
 *
 * @param manager 缓存管理器（可为 NULL）
 */
LV00_PUBLIC_API void lv00_cache_manager_destroy(Lv00CacheManager *manager);

/**
 * @brief 验证缓存管理器有效性
 *
 * @param manager 缓存管理器
 * @return true 有效，false 无效
 */
LV00_PUBLIC_API bool lv00_cache_manager_is_valid(const Lv00CacheManager *manager);

/**
 * @brief 重置缓存管理器
 *
 * 清空所有缓存条目和上下文。
 *
 * @param manager 缓存管理器
 * @return LV00_OK 成功，其他错误码表示失败
 */
LV00_PUBLIC_API Lv00ErrorCode lv00_cache_manager_reset(Lv00CacheManager *manager);

/* ============================================================
 * 基本缓存操作 API
 * ============================================================ */

/**
 * @brief 插入