/**
 * @file high_dim_demo.c
 * @brief 高维结构表示与交互模块演示
 *
 * 本示例演示如何使用高维模块进行四维及以上几何对象的投影。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include "high_dim.h"
#include "lv00.h"
#include "lv00_utils.h"

/**
 * @brief 高维结构表示与交互模块演示
 *
 * 本示例演示如何使用高维模块进行四维及以上几何对象的投影，
 * 包括注册高维块、创建自定义投影预设、计算保真度、
 * 序列化预设以及创建多投影视图等操作。
 *
 * @return 程序退出码，0 表示成功
 */
int main() {
    printf("=== Lv-00 高维结构表示与交互演示 ===\n\n");

    /* 初始化系统 */
    if (!lv00_init()) {
        fprintf(stderr, "初始化失败\n");
        return 1;
    }

    /* 创建高维管理器 */
    HighDimManager *manager = high_dim_manager_create();
    if (!manager) {
        fprintf(stderr, "创建高维管理器失败\n");
        lv00_cleanup();
        return 1;
    }

    printf("1. 注册6维超立方体块\n");
    int block_id = 1;
    int result = high_dim_register_block(manager, block_id, 6);
    if (result != LV00_OK) {
        fprintf(stderr, "注册高维块失败: %d\n", result);
        high_dim_manager_destroy(manager);
        lv00_cleanup();
        return 1;
    }
    printf("   成功注册6维块，ID=%d\n\n", block_id);

    /* 获取当前预设 */
    const HighDimProjectionPreset *preset = high_dim_get_current_preset(manager, block_id);
    if (preset) {
        printf("2. 当前投影预设: %s\n", preset->name);
        printf("   维度数: %d\n", preset->dimension_count);
        printf("   映射配置:\n");
        for (int i = 0; i < preset->mapping_count; i++) {
            printf("     轴%d -> %s\n", preset->mappings[i].axis_index,
                   high_dim_mapping_type_to_string(preset->mappings[i].mapping_type));
        }
    } else {
        fprintf(stderr, "   警告: 获取当前投影预设失败\n");
    }

    /* 创建自定义投影预设 */
    printf("\n3. 创建自定义投影预设\n");
    HighDimProjectionPreset custom_preset;
    memset(&custom_preset, 0, sizeof(custom_preset));
    lv00_strlcpy(custom_preset.name, "CustomView", HIGH_DIM_PROJECTION_NAME_MAX);
    custom_preset.dimension_count = 6;
    custom_preset.mapping_count = 6;

    /* 配置映射：维度0->X, 维度2->Y, 其余折叠 */
    for (int i = 0; i < 6; i++) {
        custom_preset.mappings[i].axis_index = i;
        custom_preset.mappings[i].scale = 1.0;
        custom_preset.mappings[i].offset = 0.0;

        if (i == 0) {
            custom_preset.mappings[i].mapping_type = HIGH_DIM_MAP_TO_X;
        } else if (i == 2) {
            custom_preset.mappings[i].mapping_type = HIGH_DIM_MAP_TO_Y;
        } else {
            custom_preset.mappings[i].mapping_type = HIGH_DIM_MAP_FOLD;
        }
    }

    /* 设置旋转变换（45度） */
    int rot_result = high_dim_create_rotation_transform(M_PI / 4, &custom_preset.transform);
    if (rot_result != LV00_OK) {
        fprintf(stderr, "   创建旋转变换失败: %d\n", rot_result);
    }

    int preset_idx = high_dim_add_projection_preset(manager, block_id, &custom_preset);
    if (preset_idx >= 0) {
        printf("   成功添加自定义预设，索引=%d\n", preset_idx);

        /* 切换到自定义预设 */
        high_dim_set_current_preset(manager, block_id, preset_idx);
        printf("   已切换到自定义预设\n");
    }

    /* 计算保真度 */
    printf("\n4. 计算投影保真度\n");
    HighDimVisibilityStats stats;
    result = high_dim_calculate_fidelity(manager, block_id, NULL, &stats);
    if (result == LV00_OK) {
        printf("   总维度: %d\n", stats.total_relations);
        printf("   可见维度: %d\n", stats.visible_relations);
        printf("   保真度: %.1f%%\n", stats.fidelity_ratio * 100.0);

        /* 检查是否低于阈值 */
        if (high_dim_is_fidelity_below_threshold(manager, block_id, HIGH_DIM_DEFAULT_FIDELITY_THRESHOLD)) {
            char warning[512];
            high_dim_get_fidelity_warning(manager, block_id, warning, sizeof(warning));
            printf("   警告: %s\n", warning);
        }
    }

    /* 获取折叠维度信息 */
    printf("\n5. 折叠维度信息\n");
    char folded_info[256];
    const HighDimProjectionPreset *current_preset = high_dim_get_current_preset(manager, block_id);
    if (current_preset) {
        high_dim_get_folded_dimensions_info(current_preset, folded_info, sizeof(folded_info));
        printf("   %s\n", folded_info);
    }

    /* 序列化预设到JSON */
    printf("\n6. 序列化投影预设\n");
    char json_buffer[2048];
    if (current_preset) {
        int json_len = high_dim_preset_serialize_json(current_preset, json_buffer, sizeof(json_buffer));
        if (json_len > 0) {
            printf("   JSON长度: %d字节\n", json_len);
            printf("   JSON预览（前200字符）:\n");
            char preview[201];
            lv00_strlcpy(preview, json_buffer, sizeof(preview));
            printf("   %s...\n", preview);
        }
    }

    /* 创建多投影视图 */
    printf("\n7. 创建多投影视图\n");
    int preset_indices[] = {0, 1}; /* 默认预设和自定义预设 */
    int view_ids[2] = {-1, -1};
    result = high_dim_create_multi_projection_view(manager, block_id, preset_indices, 2, view_ids);
    if (result == LV00_OK) {
        printf("   成功创建2个视图: ID=%d, ID=%d\n", view_ids[0], view_ids[1]);
    } else {
        printf("   创建多投影视图失败: 错误码=%d\n", result);
    }

    /* 销毁多投影视图，释放资源 */
    if (view_ids[0] >= 0) {
        high_dim_destroy_multi_projection_view(manager, view_ids[0]);
    }
    if (view_ids[1] >= 0) {
        high_dim_destroy_multi_projection_view(manager, view_ids[1]);
    }

    /* 清理 */
    printf("\n8. 清理资源\n");
    high_dim_manager_destroy(manager);
    lv00_cleanup();

    printf("\n=== 演示完成 ===\n");
    return 0;
}
