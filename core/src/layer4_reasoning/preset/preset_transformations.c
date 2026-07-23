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

#include "preset_transformations.h"

#include <string.h>

#include "lv_internal.h"
#include "preset_blocks.h"

/* ==================== Ԥ�躯�������� ==================== */

/** 变换模块预设函数块总数 */

/* ==================== ģ��ע��ʵ�� ==================== */

bool preset_transformations_register(void) {
    int success_count = 0;

    /* -------------------- ƽ�Ʊ任 -------------------- */

    /* ƽ�Ʊ任 */
    if (preset_blocks_register_by_category("translation", "����������ƽ��", PRESET_EXT_TRANSFORMATION_BASIC, 3, 1)) {
        success_count++;
    }

    /* -------------------- ��ת�任 -------------------- */

    /* �Ƶ���ת */
    if (preset_blocks_register_by_category("rotation", "������������תָ���Ƕ�", PRESET_EXT_TRANSFORMATION_BASIC, 3, 1)) {
        success_count++;
    }

    /* ͨ���ο�����ת */
    if (preset_blocks_register_by_category("rotation_by_reference", "ͨ���ο���ȷ����ת�Ƕ�", PRESET_EXT_TRANSFORMATION_BASIC,
                                           4, 1)) {
        success_count++;
    }

    /* -------------------- ����任 -------------------- */

    /* ����ֱ�߷��� */
    if (preset_blocks_register_by_category("reflection_line", "�����ֱ�ߵķ���", PRESET_EXT_TRANSFORMATION_BASIC, 3, 1)) {
        success_count++;
    }

    /* ���ڵ㷴�� */
    if (preset_blocks_register_by_category("reflection_point", "����ڵ�����ķ���", PRESET_EXT_TRANSFORMATION_BASIC, 2,
                                           1)) {
        success_count++;
    }

    /* ���Ʒ��� */
    if (preset_blocks_register_by_category("glide_reflection", "������ط����᷽��ƽ��", PRESET_EXT_TRANSFORMATION_ADVANCED,
                                           4, 1)) {
        success_count++;
    }

    /* -------------------- λ��/���ű任 -------------------- */

    /* λ�Ʊ任 */
    if (preset_blocks_register_by_category("homothety", "�������ĵ�λ�Ʊ任�����ţ�", PRESET_EXT_TRANSFORMATION_BASIC, 3,
                                           1)) {
        success_count++;
    }

    /* ͨ���ο���λ�� */
    if (preset_blocks_register_by_category("homothety_by_reference", "ͨ���ο���ȷ������ϵ��",
                                           PRESET_EXT_TRANSFORMATION_BASIC, 4, 1)) {
        success_count++;
    }

    /* �������� */
    if (preset_blocks_register_by_category("scale", "��ԭ��Ϊ���ĵľ�������", PRESET_EXT_TRANSFORMATION_BASIC, 2, 1)) {
        success_count++;
    }

    /* -------------------- ����任 -------------------- */

    /* ���б任 */
    if (preset_blocks_register_by_category("shear", "��ָ������Ĵ��б任", PRESET_EXT_TRANSFORMATION_ADVANCED, 3, 1)) {
        success_count++;
    }

    /* -------------------- �任��� -------------------- */

    /* �任���� */
    if (preset_blocks_register_by_category("transform_compose", "�����任�ĸ��� g �� f",
                                           PRESET_EXT_TRANSFORMATION_ADVANCED, 2, 1)) {
        success_count++;
    }

    /* �任���� */
    if (preset_blocks_register_by_category("transform_inverse", "��任����", PRESET_EXT_TRANSFORMATION_ADVANCED, 1, 1)) {
        success_count++;
    }

    /* ��ȱ任 */
    if (preset_blocks_register_by_category("identity_transform", "��ȱ任", PRESET_EXT_TRANSFORMATION_BASIC, 1, 1)) {
        success_count++;
    }

    /* -------------------- ����任 -------------------- */

    /* ���ݱ任 */
    if (preset_blocks_register_by_category("inversion", "����Բ�ķ��ݱ任", PRESET_EXT_TRANSFORMATION_ADVANCED, 3, 1)) {
        success_count++;
    }

    /* �������� */
    if (preset_blocks_register_by_category("spiral_similarity", "��ת�����ŵĸ���", PRESET_EXT_TRANSFORMATION_ADVANCED, 4,
                                           1)) {
        success_count++;
    }

    /* ����Ƿ�����Ԥ�趼ע��ɹ� */
    return success_count == TRANSFORMATIONS_PRESET_COUNT;
}

int preset_transformations_count(void) {
    return TRANSFORMATIONS_PRESET_COUNT;
}
