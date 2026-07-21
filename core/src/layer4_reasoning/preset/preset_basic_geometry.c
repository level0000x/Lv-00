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
#include "preset_blocks.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== Ԥ�躯�������� ==================== */

/** 基础几何模块预设函数块总数 */

/* ==================== ģ��ע��ʵ�� ==================== */

bool preset_basic_geometry_register(void)
{
    int success_count = 0;

    /* -------------------- ��Ĺ��� -------------------- */

    /* ͨ�����깹��� */
    if (preset_blocks_register_by_category(
            "point_from_coords",
            "ͨ���ѿ������깹��� P(x, y)",
            PRESET_EXT_BASIC_CONSTRUCTION,
            2, 1) == 0) {
        success_count++;
    }

    /* �е㹹�� */
    if (preset_blocks_register_by_category(
            "midpoint",
            "��������֮����е� M = (A+B)/2",
            PRESET_EXT_BASIC_CONSTRUCTION,
            2, 1) == 0) {
        success_count++;
    }

    /* ���Ĺ��� */
    if (preset_blocks_register_by_category(
            "centroid",
            "���������ε����� G = (A+B+C)/3",
            PRESET_EXT_BASIC_CONSTRUCTION,
            3, 1) == 0) {
        success_count++;
    }

    /* ���Ĺ��� */
    if (preset_blocks_register_by_category(
            "circumcenter",
            "���������ε����ģ����ԲԲ�ģ�",
            PRESET_EXT_BASIC_CONSTRUCTION,
            3, 1) == 0) {
        success_count++;
    }

    /* ���Ĺ��� */
    if (preset_blocks_register_by_category(
            "incenter",
            "���������ε����ģ�����ԲԲ�ģ�",
            PRESET_EXT_BASIC_CONSTRUCTION,
            3, 1) == 0) {
        success_count++;
    }

    /* ���Ĺ��� */
    if (preset_blocks_register_by_category(
            "orthocenter",
            "���������εĴ��ģ������ߵĽ��㣩",
            PRESET_EXT_BASIC_CONSTRUCTION,
            3, 1) == 0) {
        success_count++;
    }

    /* -------------------- �߶β��� -------------------- */

    /* ͨ�����㹹���߶� */
    if (preset_blocks_register_by_category(
            "segment_from_points",
            "ͨ�����㹹���߶� AB",
            PRESET_EXT_BASIC_CONSTRUCTION,
            2, 1) == 0) {
        success_count++;
    }

    /* ��ֱƽ���� */
    if (preset_blocks_register_by_category(
            "perpendicular_bisector",
            "�����߶εĴ�ֱƽ����",
            PRESET_EXT_BASIC_CONSTRUCTION,
            2, 1) == 0) {
        success_count++;
    }

    /* �д����ϵĵ� */
    if (preset_blocks_register_by_category(
            "point_on_perp_bisector",
            "���д����Ϲ�������е�Ϊ d �ĵ�",
            PRESET_EXT_BASIC_CONSTRUCTION,
            3, 1) == 0) {
        success_count++;
    }

    /* -------------------- ֱ�ߺ����� -------------------- */

    /* ͨ�����㹹��ֱ�� */
    if (preset_blocks_register_by_category(
            "line_from_points",
            "ͨ�����㹹������ֱ��",
            PRESET_EXT_BASIC_CONSTRUCTION,
            2, 1) == 0) {
        success_count++;
    }

    /* ƽ���� */
    if (preset_blocks_register_by_category(
            "parallel_line",
            "������ƽ���ڸ����߶ε�ֱ��",
            PRESET_EXT_BASIC_CONSTRUCTION,
            3, 1) == 0) {
        success_count++;
    }

    /* ���� */
    if (preset_blocks_register_by_category(
            "perpendicular_line",
            "��������ֱ�ڸ����߶ε�ֱ��",
            PRESET_EXT_BASIC_CONSTRUCTION,
            3, 1) == 0) {
        success_count++;
    }

    /* ���� */
    if (preset_blocks_register_by_category(
            "ray_from_points",
            "ͨ�����ͷ���㹹������",
            PRESET_EXT_BASIC_CONSTRUCTION,
            2, 1) == 0) {
        success_count++;
    }

    /* -------------------- Բ�Ĺ��� -------------------- */

    /* Բ�ĺͰ뾶�� */
    if (preset_blocks_register_by_category(
            "circle_center_radius",
            "ͨ��Բ�ĺͰ뾶�㹹��Բ",
            PRESET_EXT_CIRCLE,
            2, 1) == 0) {
        success_count++;
    }

    /* ���㶨Բ */
    if (preset_blocks_register_by_category(
            "circle_three_points",
            "ͨ�����㹹�����Բ",
            PRESET_EXT_CIRCLE,
            3, 1) == 0) {
        success_count++;
    }

    /* ���� */
    if (preset_blocks_register_by_category(
            "tangent_from_point",
            "���ⲿ����Բ������",
            PRESET_EXT_CIRCLE,
            3, 1) == 0) {
        success_count++;
    }

    /* -------------------- ������� -------------------- */

    /* ��ֱ�߽��� */
    if (preset_blocks_register_by_category(
            "line_intersection",
            "������ֱ�ߵĽ���",
            PRESET_EXT_BASIC_CONSTRUCTION,
            4, 1) == 0) {
        success_count++;
    }

    /* ֱ����Բ���� */
    if (preset_blocks_register_by_category(
            "line_circle_intersection",
            "����ֱ����Բ�Ľ���",
            PRESET_EXT_BASIC_CONSTRUCTION,
            4, 1) == 0) {
        success_count++;
    }

    /* ��Բ���� */
    if (preset_blocks_register_by_category(
            "circle_circle_intersection",
            "������Բ�Ľ���",
            PRESET_EXT_CIRCLE,
            4, 1) == 0) {
        success_count++;
    }

    /* -------------------- ������Գ� -------------------- */

    /* �����ֱ�߷��� */
    if (preset_blocks_register_by_category(
            "reflect_point_over_line",
            "�����ֱ�ߵķ��䣨�ԳƵ㣩",
            PRESET_EXT_BASIC_CONSTRUCTION,
            3, 1) == 0) {
        success_count++;
    }

    /* ����ڵ㷴�� */
    if (preset_blocks_register_by_category(
            "reflect_point_over_point",
            "����ڵ�����ĶԳ�",
            PRESET_EXT_BASIC_CONSTRUCTION,
            2, 1) == 0) {
        success_count++;
    }

    /* -------------------- ����㹹�� -------------------- */

    /* �������ָ� */
    if (preset_blocks_register_by_category(
            "point_divide_segment",
            "�������ָ��߶εĵ�",
            PRESET_EXT_BASIC_CONSTRUCTION,
            3, 1) == 0) {
        success_count++;
    }

    /* ���͹��� */
    if (preset_blocks_register_by_category(
            "harmonic_conjugate",
            "������͹����",
            PRESET_EXT_ADVANCED_CONSTRUCTION,
            3, 1) == 0) {
        success_count++;
    }

    /* ����Ƿ�����Ԥ�趼ע��ɹ� */
    return success_count == BASIC_GEOMETRY_PRESET_COUNT;
}

int preset_basic_geometry_count(void)
{
    return BASIC_GEOMETRY_PRESET_COUNT;
}
