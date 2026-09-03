/**
 * @file lv_number.h
 * @brief 数值统一抽象层 —— 唯一数值抽象面（0b：不透明句柄 + 强制池化）
 *
 * @details 本头是 Lv-00 唯一数值抽象面（design: number-abstraction-layer-design.md，
 *          ND-1/ND-2）：公共 API 不再暴露 GMP/MPFR 类型；lvNumber 为不透明句柄，
 *          实例唯一来源为数值池（帧池 + 常驻池），禁止逐数系统分配。
 *
 *          表示（kind）：
 *            - lv_NUMBER_INTEGER  : 64 位整数（小值 inline，无堆）
 *            - lv_NUMBER_RATIONAL : 精确有理数（mpq 语义，实现细节不外泄）
 *            - lv_NUMBER_FLOAT    : double（近似；供显示/数值后端）
 *            - ALGEBRAIC / INTERVAL / REAL_MPFR 为预留位（批次接入）
 *
 *          跨类型语义（0b 起精确提升，替代旧 double 降级）：
 *            - int ±/×/÷ rational → rational（精确）；
 *            - 任一操作数为 float → float（近似语义，与旧行为一致）；
 *            - int ÷ int 保持整数截断除法（契约钉住，与旧一致）。
 *
 *          生命周期（强制池化，ND-2）：
 *            - 无活动帧时创建的对象属「常驻池」：用后必须 lv_number_destroy()
 *              归还 free-list（NULL 安全、幂等）；
 *            - lv_number_frame_begin()/end() 之间创建的对象属「帧池」：随
 *              frame_end 整体回收，**不得**逐对象 destroy（destroy 为无害空操作）；
 *            - 帧对象指针在 frame_end 后悬空（arena 语义，与 lv_arena 一致）。
 *
 *          注意：mpq 内部 limb 存储由 GMP 分配器管理（同 coeff_pool/mpz 现状）；
 *          GMP 全局 allocator 接线已于批次 235 完成（lv_gmp_memory_wire，lv_init 首行，
 *          SECURITY.md 盲区关闭）；全部 mpz/mpq_get_str(NULL) 调用点已迁移为调用方缓冲。
 *
 * @version 2.0.0-dev（0b）
 * @see docs/architecture/number-abstraction-layer-design.md
 */

#ifndef lv_NUMBER_H
#define lv_NUMBER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

/* 数值类型枚举（序号/名称契约钉住：test_lv_number_*_ext，勿改值） */
typedef enum {
    lv_NUMBER_RATIONAL = 0,    // 有理数
    lv_NUMBER_ALGEBRAIC,       // 代数数（预留）
    lv_NUMBER_INTERVAL,        // 区间（预留）
    lv_NUMBER_FLOAT,           // 浮点（double）
    lv_NUMBER_INTEGER,         // 整数（int64）
    lv_NUMBER_REAL_MPFR,       // MPFR 表示（预留，dependency-policy 批次）
    lv_NUMBER_COUNT
} lvNumberType;

/* 不透明句柄：布局只存在于 lv_number.c（强制池化节点），禁止直接访问 */
typedef struct lvNumber lvNumber;

struct lvRational; /* 前向声明：域迁移工厂参数（rational.h 中定义为 struct lvRational） */

/* 域迁移试点工厂：从 lvRational（有理数封装）构造 RATIONAL 节点。 */
lv_PUBLIC_API lvNumber *lv_number_from_lvRational(const struct lvRational *r);

/* ============================================================
 * 强制池化 —— 帧池 API（ND-2）
 * ============================================================ */

/** 帧句柄（不透明；嵌套帧经 TLS 栈管理） */
typedef struct lvNumberFrame lvNumberFrame;

/**
 * @brief 开启一个数值帧：帧内创建的 lvNumber 归帧池，随 frame_end 整体回收
 * @return 帧句柄（NULL = 池初始化失败）；可嵌套（先进后出）
 */
lv_PUBLIC_API lvNumberFrame *lv_number_frame_begin(void);

/**
 * @brief 结束当前数值帧：回收该帧内创建的全部对象（含 mpq 内部存储）
 * @param frame 帧句柄（NULL = 结束最内层当前帧；不匹配时安全拒绝）
 */
lv_PUBLIC_API void lv_number_frame_end(lvNumberFrame *frame);

/** @brief 当前是否处于数值帧内 */
lv_PUBLIC_API bool lv_number_in_frame(void);

/* ============================================================
 * 工厂函数（无帧时产出常驻对象，须 destroy；帧内产出帧对象）
 * ============================================================ */

lv_PUBLIC_API lvNumber *lv_number_from_rational(int64_t num, uint64_t den);
lv_PUBLIC_API lvNumber *lv_number_from_double(double val);
lv_PUBLIC_API lvNumber *lv_number_from_int(int64_t val);
lv_PUBLIC_API lvNumber *lv_number_from_string(const char *str);

/* ============================================================
 * 算术运算（跨类型精确提升，见文件头注释）
 * ============================================================ */

lv_PUBLIC_API lvNumber *lv_number_add(const lvNumber *a, const lvNumber *b);
lv_PUBLIC_API lvNumber *lv_number_sub(const lvNumber *a, const lvNumber *b);
lv_PUBLIC_API lvNumber *lv_number_mul(const lvNumber *a, const lvNumber *b);
lv_PUBLIC_API lvNumber *lv_number_div(const lvNumber *a, const lvNumber *b);
lv_PUBLIC_API lvNumber *lv_number_neg(const lvNumber *n);
lv_PUBLIC_API lvNumber *lv_number_abs(const lvNumber *n);
lv_PUBLIC_API lvNumber *lv_number_pow(const lvNumber *base, int exp);

/* ============================================================
 * 比较（NULL 安全：compare(NULL,x)→0、谓词(NULL)→false）
 * ============================================================ */

lv_PUBLIC_API int lv_number_compare(const lvNumber *a, const lvNumber *b);
lv_PUBLIC_API bool lv_number_eq(const lvNumber *a, const lvNumber *b);
lv_PUBLIC_API bool lv_number_lt(const lvNumber *a, const lvNumber *b);
lv_PUBLIC_API bool lv_number_gt(const lvNumber *a, const lvNumber *b);
lv_PUBLIC_API bool lv_number_lte(const lvNumber *a, const lvNumber *b);
lv_PUBLIC_API bool lv_number_gte(const lvNumber *a, const lvNumber *b);

/* ============================================================
 * 转换 / 查询 / 克隆 / 销毁
 * ============================================================ */

lv_PUBLIC_API double lv_number_to_double(const lvNumber *n);
lv_PUBLIC_API int64_t lv_number_to_int(const lvNumber *n);
lv_PUBLIC_API char *lv_number_to_string(const lvNumber *n);   /* [take] 调用者 lv_free */

lv_PUBLIC_API bool lv_number_is_zero(const lvNumber *n);
lv_PUBLIC_API bool lv_number_is_one(const lvNumber *n);
lv_PUBLIC_API bool lv_number_is_negative(const lvNumber *n);
lv_PUBLIC_API bool lv_number_is_positive(const lvNumber *n);
lv_PUBLIC_API bool lv_number_is_integer(const lvNumber *n);
lv_PUBLIC_API lvNumberType lv_number_type(const lvNumber *n);
lv_PUBLIC_API uint64_t lv_number_hash(const lvNumber *n);

lv_PUBLIC_API lvNumber *lv_number_clone(const lvNumber *n);   /* 独立深拷贝 */
lv_PUBLIC_API void lv_number_destroy(lvNumber *n);            /* 常驻归还 free-list；帧对象空操作 */

/* ---- 类型信息（名称契约钉住，勿改字符串）---- */
lv_PUBLIC_API const char *lv_number_type_name(lvNumberType type);

#ifdef __cplusplus
}
#endif

#endif /* lv_NUMBER_H */
