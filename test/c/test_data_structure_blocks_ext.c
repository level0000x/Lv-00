/**
 * @file test_data_structure_blocks_ext.c
 * @brief 数据结构块契约测试（批次 C-㊺续28：data_structure_blocks.h 7 个零覆盖 API）
 *
 * 覆盖：list_block_create/destroy、map_block_create/destroy、
 *   record_block_create/destroy、record_block_set_field
 * 契约：操作类型存储、record 字段名深拷贝、越界 index 拒绝。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/data_structure_blocks.h"

int g_pass_count = 0;
int g_fail_count = 0;

static void test_list_map_block_api(void) {
    /* list block */
    lvListBlock *lb = lv_list_block_create(lv_LIST_MAP);
    TEST_ASSERT_NOT_NULL(lb);
    TEST_ASSERT_EQ((int) lb->operation, (int) lv_LIST_MAP);
    lv_list_block_destroy(lb);
    lv_list_block_destroy(NULL);

    lvListBlock *lb2 = lv_list_block_create(lv_LIST_FILTER);
    TEST_ASSERT_EQ((int) lb2->operation, (int) lv_LIST_FILTER);
    lv_list_block_destroy(lb2);

    /* map block */
    lvMapBlock *mb = lv_map_block_create(lv_MAP_INSERT);
    TEST_ASSERT_NOT_NULL(mb);
    TEST_ASSERT_EQ((int) mb->operation, (int) lv_MAP_INSERT);
    lv_map_block_destroy(mb);
    lv_map_block_destroy(NULL);

    lvMapBlock *mb2 = lv_map_block_create(lv_MAP_KEYS);
    TEST_ASSERT_EQ((int) mb2->operation, (int) lv_MAP_KEYS);
    lv_map_block_destroy(mb2);

    printf("  test_list_map_block_api: PASSED\n");
}

static void test_record_block_api(void) {
    /* create(2) */
    lvRecordBlock *rb = lv_record_block_create(2);
    TEST_ASSERT_NOT_NULL(rb);
    TEST_ASSERT_EQ(rb->field_count, 2);
    TEST_ASSERT_NOT_NULL(rb->fields);

    /* set_field：正常 */
    TEST_ASSERT_EQ(lv_record_block_set_field(rb, 0, "name", (void *) 0x11), 0);
    TEST_ASSERT_NOT_NULL(rb->fields[0].field_name);
    TEST_ASSERT(strcmp(rb->fields[0].field_name, "name") == 0, "字段名");
    TEST_ASSERT_EQ(rb->fields[0].field_type, (void *) 0x11);
    TEST_ASSERT_EQ(lv_record_block_set_field(rb, 1, "age", NULL), 0);
    TEST_ASSERT_NOT_NULL(rb->fields[1].field_name);

    /* set_field：覆盖旧名（防泄漏） */
    TEST_ASSERT_EQ(lv_record_block_set_field(rb, 0, "full_name", (void *) 0x22), 0);
    TEST_ASSERT(strcmp(rb->fields[0].field_name, "full_name") == 0, "覆盖字段名");

    /* 越界/NULL */
    TEST_ASSERT_EQ(lv_record_block_set_field(rb, 2, "x", NULL), -1);
    TEST_ASSERT_EQ(lv_record_block_set_field(rb, -1, "x", NULL), -1);
    TEST_ASSERT_EQ(lv_record_block_set_field(NULL, 0, "x", NULL), -1);

    lv_record_block_destroy(rb);
    lv_record_block_destroy(NULL);

    /* create(0)：延迟分配 */
    lvRecordBlock *zero = lv_record_block_create(0);
    TEST_ASSERT_NOT_NULL(zero);
    TEST_ASSERT_EQ(zero->field_count, 0);
    TEST_ASSERT_NULL(zero->fields);
    TEST_ASSERT_EQ(lv_record_block_set_field(zero, 0, "x", NULL), -1);
    lv_record_block_destroy(zero);

    printf("  test_record_block_api: PASSED\n");
}

TEST_MAIN_BEGIN("Lv-00 Data Structure Blocks Ext Test Suite")
    printf("=== Lv-00 Data Structure Blocks Ext Test Suite (batch C-㊺续28) ===\n\n");
    lv_init();
    TEST_MAIN_RUN(test_list_map_block_api);
    TEST_MAIN_RUN(test_record_block_api);
    lv_cleanup();
TEST_MAIN_END()
