/**
 * @file preset_basic_geometry.c
 * @brief �������ι���Ԥ�躯���� - ʵ��
 *
 * ʵ�ֻ������ι���ģ�������Ԥ�躯���顣
 * ������Ĺ��졢�߶β�����ֱ�ߺ����ߡ�Բ�Ĺ���ȡ�
 *
 * @module BasicGeometry
 * @category PRESET_CATEGORY_CONSTRUCTION
 */

#include "preset_basic_geometry.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"

/* ==================== Ԥ�躯�������� ==================== */

/** 基础几何模块预设函数块总数 */


int preset_basic_geometry_count(void) {
    return BASIC_GEOMETRY_PRESET_COUNT;
}
