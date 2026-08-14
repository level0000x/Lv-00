/**
 * @file preset_polygons.c
 * @brief ����ι���Ԥ�躯���� - ʵ��
 *
 * ʵ�ֶ���ι���ģ�������Ԥ�躯���顣
 * ����������Ρ����������⹹�졢�ı��ι���ȡ�
 *
 * @module Polygons
 * @category PRESET_CATEGORY_CONSTRUCTION
 */

#include "lv/preset_polygons.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/preset_blocks.h"

/* ==================== Ԥ�躯�������� ==================== */

/** �����ģ��Ԥ�躯�������� */


int preset_polygons_count(void) {
    return POLYGONS_PRESET_COUNT;
}
