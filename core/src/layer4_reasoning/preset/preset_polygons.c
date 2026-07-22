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

#include "preset_polygons.h"
#include "preset_blocks.h"
#include "lv00_internal.h"

#include <string.h>

/* ==================== Ԥ�躯�������� ==================== */

/** �����ģ��Ԥ�躯�������� */

/* ==================== ģ��ע��ʵ�� ==================== */

bool preset_polygons_register(void)
{
    int success_count = 0;

    /* -------------------- ������� -------------------- */

    /* �������� */
    if (preset_blocks_register_by_category(
            "equilateral_triangle",
            "�����߹����������εĵ���������",
            PRESET_EXT_POLYGON,
            2, 1)) {
        success_count++;
    }

    /* ������ */
    if (preset_blocks_register_by_category(
            "square",
            "�����߹��������ε�������������",
            PRESET_EXT_POLYGON,
            2, 2)) {
        success_count++;
    }

    /* ��n���� */
    if (preset_blocks_register_by_category(
            "regular_polygon",
            "�����߹�����n����",
            PRESET_EXT_POLYGON,
            3, 1)) {
        success_count++;
    }

    /* ������� */
    if (preset_blocks_register_by_category(
            "regular_pentagon",
            "�����߹����������",
            PRESET_EXT_POLYGON,
            2, 5)) {
        success_count++;
    }

    /* �������� */
    if (preset_blocks_register_by_category(
            "regular_hexagon",
            "�����߹�����������",
            PRESET_EXT_POLYGON,
            2, 6)) {
        success_count++;
    }

    /* -------------------- ���������⹹�� -------------------- */

    /* ���������� */
    if (preset_blocks_register_by_category(
            "isosceles_triangle",
            "�����ױߺ͸߹������������",
            PRESET_EXT_POLYGON,
            3, 1)) {
        success_count++;
    }

    /* ֱ�������� */
    if (preset_blocks_register_by_category(
            "right_triangle",
            "����ֱ�Ƕ����һֱ�Ǳ߹���ֱ��������",
            PRESET_EXT_POLYGON,
            3, 1)) {
        success_count++;
    }

    /* SSS���� */
    if (preset_blocks_register_by_category(
            "triangle_sss",
            "�������߳�����������",
            PRESET_EXT_POLYGON,
            4, 1)) {
        success_count++;
    }

    /* SAS���� */
    if (preset_blocks_register_by_category(
            "triangle_sas",
            "�������߼��нǹ���������",
            PRESET_EXT_POLYGON,
            4, 1)) {
        success_count++;
    }

    /* ASA���� */
    if (preset_blocks_register_by_category(
            "triangle_asa",
            "�������Ǽ��б߹���������",
            PRESET_EXT_POLYGON,
            4, 1)) {
        success_count++;
    }

    /* -------------------- �ı��ι��� -------------------- */

    /* ���� */
    if (preset_blocks_register_by_category(
            "rectangle",
            "���������㹹�����",
            PRESET_EXT_POLYGON,
            3, 1)) {
        success_count++;
    }

    /* ƽ���ı��� */
    if (preset_blocks_register_by_category(
            "parallelogram",
            "���������㹹��ƽ���ı���",
            PRESET_EXT_POLYGON,
            3, 1)) {
        success_count++;
    }

    /* ���� */
    if (preset_blocks_register_by_category(
            "rhombus",
            "�����Խ��߹�������",
            PRESET_EXT_POLYGON,
            3, 1)) {
        success_count++;
    }

    /* ���� */
    if (preset_blocks_register_by_category(
            "trapezoid",
            "�����ױߺ͵׽ǹ�������",
            PRESET_EXT_POLYGON,
            4, 1)) {
        success_count++;
    }

    /* ���� */
    if (preset_blocks_register_by_category(
            "kite",
            "�����Խ��ߺͶ��㹹������",
            PRESET_EXT_POLYGON,
            3, 1)) {
        success_count++;
    }

    /* -------------------- �������� -------------------- */

    /* ���ζ���� */
    if (preset_blocks_register_by_category(
            "star_polygon",
            "�������ζ���� {n/k}",
            PRESET_EXT_POLYGON,
            4, 1)) {
        success_count++;
    }

    /* ����� */
    if (preset_blocks_register_by_category(
            "pentagram",
            "�����������",
            PRESET_EXT_POLYGON,
            2, 5)) {
        success_count++;
    }

    /* ����Ƿ�����Ԥ�趼ע��ɹ� */
    return success_count == POLYGONS_PRESET_COUNT;
}

int preset_polygons_count(void)
{
    return POLYGONS_PRESET_COUNT;
}
