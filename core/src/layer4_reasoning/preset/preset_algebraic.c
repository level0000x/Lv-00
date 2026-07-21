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

#include "preset_algebraic.h"
#include "preset_blocks.h"
#include "lv00_internal.h"

#include <string.h>

/* ==================== Ԥ�躯�������� ==================== */

/** 代数模块预设函数块总数 */

/* ==================== ģ��ע��ʵ�� ==================== */

int preset_algebraic_register(void)
{
    int success_count = 0;

    /* -------------------- �������� -------------------- */

    /* �����ӷ� */
    if (preset_blocks_register_by_category(
            "vector_add",
            "�����ӷ���OA + OB = OC",
            PRESET_EXT_ALGEBRA_BASIC,
            3, 1) == 0) {
        success_count++;
    }

    /* �������� */
    if (preset_blocks_register_by_category(
            "vector_sub",
            "����������OA - OB = OC",
            PRESET_EXT_ALGEBRA_BASIC,
            3, 1) == 0) {
        success_count++;
    }

    /* �������� */
    if (preset_blocks_register_by_category(
            "vector_scale",
            "�������ˣ�k * OA = OB",
            PRESET_EXT_ALGEBRA_BASIC,
            3, 1) == 0) {
        success_count++;
    }

    /* ����������� */
    if (preset_blocks_register_by_category(
            "vector_linear_combination",
            "����������ϣ�k1*OA1 + k2*OA2 = OB",
            PRESET_EXT_ALGEBRA_ADVANCED,
            5, 1) == 0) {
        success_count++;
    }

    /* ������һ�� */
    if (preset_blocks_register_by_category(
            "vector_normalize",
            "���쵥λ����",
            PRESET_EXT_ALGEBRA_BASIC,
            2, 1) == 0) {
        success_count++;
    }

    /* ����ͶӰ */
    if (preset_blocks_register_by_category(
            "vector_project",
            "�����ڷ����ϵ�ͶӰ",
            PRESET_EXT_ALGEBRA_ADVANCED,
            3, 1) == 0) {
        success_count++;
    }

    /* -------------------- ����ϵ����� -------------------- */

    /* ��׼������ */
    if (preset_blocks_register_by_category(
            "standard_basis",
            "�����׼������",
            PRESET_EXT_COORDINATE,
            2, 1) == 0) {
        success_count++;
    }

    /* ����任 */
    if (preset_blocks_register_by_category(
            "coordinate_transform",
            "�����һ����任����һ���",
            PRESET_EXT_COORDINATE,
            5, 1) == 0) {
        success_count++;
    }

    /* ������תֱ������ */
    if (preset_blocks_register_by_category(
            "polar_to_cartesian",
            "������ת��Ϊֱ������",
            PRESET_EXT_COORDINATE,
            3, 1) == 0) {
        success_count++;
    }

    /* -------------------- �������� -------------------- */

    /* �����˷� */
    if (preset_blocks_register_by_category(
            "complex_multiply",
            "�����˷��ļ��α�ʾ",
            PRESET_EXT_ALGEBRA_ADVANCED,
            3, 1) == 0) {
        success_count++;
    }

    /* �������� */
    if (preset_blocks_register_by_category(
            "complex_divide",
            "���������ļ��α�ʾ",
            PRESET_EXT_ALGEBRA_ADVANCED,
            3, 1) == 0) {
        success_count++;
    }

    /* ���������� */
    if (preset_blocks_register_by_category(
            "complex_power",
            "������ n ����",
            PRESET_EXT_ALGEBRA_ADVANCED,
            3, 1) == 0) {
        success_count++;
    }

    /* �������� */
    if (preset_blocks_register_by_category(
            "complex_root",
            "������ n �η���",
            PRESET_EXT_ALGEBRA_ADVANCED,
            3, 1) == 0) {
        success_count++;
    }

    /* -------------------- Բ׶���� -------------------- */

    /* �������ϵĵ� */
    if (preset_blocks_register_by_category(
            "parabola_point",
            "�����������ϵĵ�",
            PRESET_EXT_ALGEBRA_ADVANCED,
            4, 1) == 0) {
        success_count++;
    }

    /* ��Բ�ϵĵ� */
    if (preset_blocks_register_by_category(
            "ellipse_point",
            "������Բ�ϵĵ�",
            PRESET_EXT_ALGEBRA_ADVANCED,
            4, 1) == 0) {
        success_count++;
    }

    /* ����Ƿ�����Ԥ�趼ע��ɹ� */
    return success_count == ALGEBRAIC_PRESET_COUNT;
}

int preset_algebraic_count(void)
{
    return ALGEBRAIC_PRESET_COUNT;
}
