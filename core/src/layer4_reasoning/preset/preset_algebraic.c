/**
 * @file preset_algebraic.c
 * @brief ��������Ԥ�躯���� - ʵ��
 *
 * ʵ�ִ�������ģ�������Ԥ�躯���顣
 * ������������������任����������ȡ�
 *
 * @module Algebraic
 * @category PRESET_CATEGORY_ALGEBRAIC
 */

#include "lv/preset_algebraic.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/preset_blocks.h"

/* ==================== Ԥ�躯�������� ==================== */

/** 代数模块预设函数块总数 */


int preset_algebraic_count(void) {
    return ALGEBRAIC_PRESET_COUNT;
}
