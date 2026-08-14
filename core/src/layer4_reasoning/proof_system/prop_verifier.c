/**
 * @file prop_verifier.c
 * @brief 命题逻辑验证器实现 —— 自然演绎证明引擎
 *
 * @details 实现基于自然演绎的命题逻辑验证算法。
 *          支持直觉主义逻辑和经典逻辑模式。
 *
 *          核心推理规则：
 *            - 合取消除：从合取推导其分项
 *            - 析取引入：从分项推导析取
 *            - 蕴含消除（modus ponens）：从蕴含和前件推导后件
 *            - 否定消除：从否定和原命题推导矛盾
 *            - 爆炸原理（可选）：从矛盾推导任意命题
 *
 * @author Lv-00 Project
 * @version 1.1.0
 */

#include "lv/prop_verifier.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream.h"
#include "prop_verifier_internal.h"

/* 注：setter prop_verifier_set_stream_context 定义在 prop_verifier_equivalence.c（异文件），
 * 不适用 LV_STREAM_CTX_DEFINE 宏（宏要求 setter 与变量同文件），保留手写。 */
lv_THREAD_LOCAL StreamContext *prop_verifier_stream_ctx = NULL;

/* ============================================================
 * 内部常量
 * ============================================================ */

#define MAX_PREMISES 64       /**< 最大前提数 */
#define MAX_GOALS 64          /**< 最大目标数 */
#define MAX_MEMO_ENTRIES 1024 /**< 记忆化条目数 */
#define MAX_COPY_DEPTH 200    /**< 公式深拷贝递归深度（防止栈溢出） */
#define MAX_DESTROY_DEPTH 200 /**< 公式销毁递归深度（防止栈溢出） */

/* ---- 销毁栈配置 ---- */
#define PROP_DESTROY_STACK_INIT_CAP 64 /* 销毁时栈的初始容量 */
#define PROP_DESTROY_STACK_GROWTH 2    /* 销毁栈扩容倍数 */

/* ---- 运算符优先级 ---- */
#define PROP_PREC_ATOM 100       /* 原子命题优先级 */
#define PROP_PREC_NEGATION 80    /* 否定运算符优先级 */
#define PROP_PREC_CONJUNCTION 60 /* 合取运算符优先级 */
#define PROP_PREC_DISJUNCTION 50 /* 析取运算符优先级 */
#define PROP_PREC_IMPLICATION 40 /* 蕴含运算符优先级 */
#define PROP_PREC_DEFAULT 0      /* 默认运算符优先级 */

/* ---- 哈希常量定义 ---- */
#define PROP_HASH_TYPE_MULTIPLIER 2654435761U  /* 黄金比例乘数（Knuth推荐） */
#define PROP_HASH_STRING_MULTIPLIER 31         /* 字符串哈希乘数 */
#define PROP_HASH_LEFT_MULTIPLIER 0x9e3779b9U  /* 左子公式哈希乘数 */
#define PROP_HASH_RIGHT_MULTIPLIER 0x517cc1b7U /* 右子公式哈希乘数 */
#define PROP_HASH_PTR_MULTIPLIER 0x45d9f3bU    /* 指针哈希乘数 */
#define PROP_HASH_BIT_SHIFT 16                 /* 哈希位移偏移量 */
#define PROP_HASH_PREMISES_MULTIPLIER 31       /* 前提集合哈希乘数 */

/* ---- 时间转换 ---- */
#define PROP_TIME_MS_PER_SEC 1000 /* 秒到毫秒的转换常数 */

/* ---- 冒烟测试与缓冲区常量 ---- */
#define PROP_SMOKE_TEST_COUNT 13       /* 内置冒烟测试数 */
#define PROP_SMOKE_MAX_PREM_PTRS 8     /* 冒烟测试前提指针临时数组大小 */
#define PROP_SMOKE_CLEANUP_MAX_PTRS 16 /* 冒烟测试清理时临时收集指针数 */
#define PROP_ATOM_NAME_MAX_LEN 64      /* 原子命题名称最大长度 */
#define PROP_ATOM_COLLECT_MAX 32       /* 收集原子命题最大数量 */
#define PROP_PATTERN_DESC_BUFSIZE 256  /* 模式描述缓冲区大小 */
#define PROP_ANALYSIS_DESC_BUFSIZE 512 /* 分析描述缓冲区大小 */
#define PROP_MISSING_LIST_BUFSIZE 512  /* 缺失列表缓冲区大小 */
#define PROP_STREAM_EVENT_BUFSIZE 256  /* 公式事件描述缓冲区大小 */
#define PROP_JSON_DETAIL_BUFSIZE 192   /* JSON详情缓冲区大小 */

/* ---- 信任颜色判定阈值 ---- */
#define PROP_TRUST_YELLOW_THRESHOLD 2 /* 黄色信任的缺失构造上限 */
#define PROP_TRUST_AMBER_MIN 3        /* 琥珀色信任的缺失构造下限 */

