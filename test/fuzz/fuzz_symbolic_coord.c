/**
 * @file fuzz_symbolic_coord.c
 * @brief 符号坐标模糊测试 - 使用 libFuzzer
 *
 * 测试目标：
 * - 各种坐标类型的创建和销毁
 * - 坐标运算的鲁棒性
 * - 序列化/反序列化
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lv.h"
#include "lv_utils.h"

/**
 * @brief libFuzzer 入口点：符号坐标模糊测试
 *
 * 使用 libFuzzer 提供的随机输入数据测试符号坐标模块的鲁棒性。
 * 输入数据被解析为有理数坐标参数，用于测试坐标创建、销毁、
 * 序列化/反序列化以及四则运算的正确性和安全性。
 *
 * @param data 模糊测试输入数据指针
 * @param size 输入数据的字节长度，至少需要 16 字节
 * @return 固定返回 0（libFuzzer 约定）
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 16)
        return 0;

    /* 测试有理数坐标 */
    {
        /* [P1 修复] 使用位移操作代替直接内存操作，避免字节序假设 */
        int64_t numer = (int64_t) ((uint64_t) data[0] | ((uint64_t) data[1] << 8) | ((uint64_t) data[2] << 16) |
                                   ((uint64_t) data[3] << 24));
        uint64_t denom =
            (uint64_t) data[4] | ((uint64_t) data[5] << 8) | ((uint64_t) data[6] << 16) | ((uint64_t) data[7] << 24);
        if (denom == 0)
            denom = 1; /* 避免除零 */

        SymbolicCoord *c = symbolic_coord_create_rational(numer, denom);
        if (c) {
            /* 测试序列化 */
            char *str = symbolic_coord_serialize(c);
            if (str) {
                /* 测试反序列化 */
                SymbolicCoord *c2 = symbolic_coord_deserialize(str);
                if (c2) {
                    symbolic_coord_destroy(c2);
                }
                lv_free_ptr(str);
            }
            symbolic_coord_destroy(c);
        }
    }

    /* 测试坐标运算 */
    {
        int64_t n1 = (int64_t) data[8];
        int64_t n2 = (int64_t) data[9];

        SymbolicCoord *a = symbolic_coord_create_rational(n1, 1);
        if (!a)
            goto skip_coord_ops;
        SymbolicCoord *b = symbolic_coord_create_rational(n2, 1);
        if (!b) {
            symbolic_coord_destroy(a);
            goto skip_coord_ops;
        }

        /* 测试加法 */
        SymbolicCoord *sum = symbolic_coord_add(a, b);
        if (sum)
            symbolic_coord_destroy(sum);

        /* 测试减法 */
        SymbolicCoord *diff = symbolic_coord_subtract(a, b);
        if (diff)
            symbolic_coord_destroy(diff);

        /* 测试乘法 */
        SymbolicCoord *prod = symbolic_coord_multiply(a, b);
        if (prod)
            symbolic_coord_destroy(prod);

        /* 测试除法（避免除零） */
        if (n2 != 0) {
            SymbolicCoord *quot = symbolic_coord_divide(a, b);
            if (quot)
                symbolic_coord_destroy(quot);
        }

        /* 测试比较 */
        symbolic_coord_compare(a, b);

        symbolic_coord_destroy(a);
        symbolic_coord_destroy(b);

    skip_coord_ops:
    }

    return 0;
}
