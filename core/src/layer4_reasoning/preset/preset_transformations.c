/**
 * @file preset_transformations.c
 * @brief ���α任Ԥ�躯���� - ʵ��
 *
 * ʵ�ּ��α任ģ�������Ԥ�躯���顣
 * ����ƽ�ơ���ת�����䡢λ�ơ�����任�ȡ�
 *
 * @module Transformations
 * @category PRESET_CATEGORY_TRANSFORMATION
 */

#include "lv/preset_transformations.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/preset_blocks.h"

/* ==================== Ԥ�躯�������� ==================== */

/** 变换模块预设函数块总数 */


int preset_transformations_count(void) {
    return TRANSFORMATIONS_PRESET_COUNT;
}
