/**
 * @file preset_measurements.c
 * @brief ���ζ�������Ԥ�躯���� - ʵ��
 *
 * ʵ�ּ��ζ�������ģ�������Ԥ�躯���顣
 * �������롢�Ƕȡ����������ȶ������㡣
 *
 * @module Measurements
 * @category PRESET_CATEGORY_MEASUREMENT
 */

#include "preset_measurements.h"
#include "preset_blocks.h"
#include "lv00_internal.h"

#include <string.h>

/* ==================== Ԥ�躯�������� ==================== */

/** ��������ģ��Ԥ�躯�������� */
#define MEASUREMENTS_PRESET_COUNT 20

/* ==================== ģ��ע��ʵ�� ==================== */

bool preset_measurements_register(void)
{
    int success_count = 0;

    /* -------------------- ������� -------------------- */

    /* ŷ����þ��� */
    if (preset_blocks_register_by_category(
            "distance_euclidean",
            "����������ŷ����þ���",
            PRESET_EXT_MEASUREMENT,
            2, 1)) {
        success_count++;
    }

    /* ŷ����þ���ƽ�� */
    if (preset_blocks_register_by_category(
            "distance_squared",
            "�������������ƽ�������⿪����",
            PRESET_EXT_MEASUREMENT,
            2, 1)) {
        success_count++;
    }

    /* �����پ��� */
    if (preset_blocks_register_by_category(
            "distance_manhattan",
            "���������������پ��루L1������",
            PRESET_EXT_MEASUREMENT,
            2, 1)) {
        success_count++;
    }

    /* �б�ѩ����� */
    if (preset_blocks_register_by_category(
            "distance_chebyshev",
            "�����������б�ѩ����루L�޷�����",
            PRESET_EXT_MEASUREMENT,
            2, 1)) {
        success_count++;
    }

    /* �㵽ֱ�߾��� */
    if (preset_blocks_register_by_category(
            "distance_point_to_line",
            "����㵽ֱ�ߵ���̾���",
            PRESET_EXT_MEASUREMENT,
            3, 1)) {
        success_count++;
    }

    /* �㵽�߶ξ��� */
    if (preset_blocks_register_by_category(
            "distance_point_to_segment",
            "����㵽�߶ε���̾���",
            PRESET_EXT_MEASUREMENT,
            3, 1)) {
        success_count++;
    }

    /* -------------------- �Ƕȶ��� -------------------- */

    /* ����Ƕ� */
    if (preset_blocks_register_by_category(
            "angle_three_points",
            "���������γɵĽǶ�",
            PRESET_EXT_MEASUREMENT,
            3, 1)) {
        success_count++;
    }

    /* ��ֱ�߼н� */
    if (preset_blocks_register_by_category(
            "angle_two_lines",
            "������ֱ�ߵļн�",
            PRESET_EXT_MEASUREMENT,
            4, 1)) {
        success_count++;
    }

    /* ����� */
    if (preset_blocks_register_by_category(
            "directed_angle",
            "��������ǣ������ţ�",
            PRESET_EXT_MEASUREMENT,
            3, 1)) {
        success_count++;
    }

    /* -------------------- ������� -------------------- */

    /* ��������������깫ʽ�� */
    if (preset_blocks_register_by_category(
            "triangle_area",
            "ʹ�����깫ʽ�������������",
            PRESET_EXT_MEASUREMENT,
            3, 1)) {
        success_count++;
    }

    /* ���׹�ʽ */
    if (preset_blocks_register_by_category(
            "triangle_area_heron",
            "ʹ�ú��׹�ʽ�������������",
            PRESET_EXT_MEASUREMENT,
            3, 1)) {
        success_count++;
    }

    /* Բ��� */
    if (preset_blocks_register_by_category(
            "circle_area",
            "����Բ�����",
            PRESET_EXT_MEASUREMENT,
            2, 1)) {
        success_count++;
    }

    /* ������� */
    if (preset_blocks_register_by_category(
            "sector_area",
            "�����������",
            PRESET_EXT_MEASUREMENT,
            3, 1)) {
        success_count++;
    }

    /* -------------------- ���ȼ��� -------------------- */

    /* �߶γ��� */
    if (preset_blocks_register_by_category(
            "segment_length",
            "�����߶γ���",
            PRESET_EXT_MEASUREMENT,
            2, 1)) {
        success_count++;
    }

    /* Բ�ܳ� */
    if (preset_blocks_register_by_category(
            "circle_circumference",
            "����Բ���ܳ�",
            PRESET_EXT_MEASUREMENT,
            2, 1)) {
        success_count++;
    }

    /* -------------------- �������� -------------------- */

    /* ����ģ�� */
    if (preset_blocks_register_by_category(
            "vector_magnitude",
            "��������ģ��",
            PRESET_EXT_MEASUREMENT,
            2, 1)) {
        success_count++;
    }

    /* ������� */
    if (preset_blocks_register_by_category(
            "vector_dot_product",
            "�������������ĵ��",
            PRESET_EXT_MEASUREMENT,
            4, 1)) {
        success_count++;
    }

    /* ������� */
    if (preset_blocks_register_by_category(
            "vector_cross_product",
            "�������������Ĳ������ά��",
            PRESET_EXT_MEASUREMENT,
            4, 1)) {
        success_count++;
    }

    /* �����н� */
    if (preset_blocks_register_by_category(
            "vector_angle",
            "�������������ļн�",
            PRESET_EXT_MEASUREMENT,
            4, 1)) {
        success_count++;
    }

    /* -------------------- ���ʼ��� -------------------- */

    /* Բ������ */
    if (preset_blocks_register_by_category(
            "circle_curvature",
            "����Բ������",
            PRESET_EXT_MEASUREMENT,
            2, 1)) {
        success_count++;
    }

    /* ����Ƿ�����Ԥ�趼ע��ɹ� */
    return success_count == MEASUREMENTS_PRESET_COUNT;
}

int preset_measurements_count(void)
{
    return MEASUREMENTS_PRESET_COUNT;
}
