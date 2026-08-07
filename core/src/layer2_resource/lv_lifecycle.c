/**
 * @file lv_lifecycle.c
 * @brief 复合对象生命周期管理 —— 通用字段销毁实现
 *
 * @details 实现 lv_obj_destroy_fields：按 lvFieldDesc 描述表逐字段销毁
 *          复合对象持有的资源，收敛各模块同构的逐字段销毁样板。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/lv_lifecycle.h"

#include <string.h> /* memcpy（严格别名安全读取指针字段） */

/**
 * @brief 按字段描述表统一销毁复合对象的全部资源
 *
 * 释放顺序 = 描述表声明顺序；所有指针字段释放后置 NULL（lv_free 语义），
 * 全部字段为 NULL 的对象重复调用安全。
 */
void lv_obj_destroy_fields(void *obj, const lvFieldDesc *fields, size_t n) {
    if (!obj || !fields)
        return;

    for (size_t i = 0; i < n; i++) {
        const lvFieldDesc *f = &fields[i];
        void *field_ptr = (char *) obj + f->offset;

        switch (f->kind) {
        case LV_FIELD_PLAIN_FREE:
            /* lv_free 本身 NULL 安全：NULL 直接返回，否则释放并置 NULL */
            lv_free((void **) field_ptr);
            break;

        case LV_FIELD_DARRAY_FREE:
            /* lv_darray_free 对零初始化/已释放的 lvDArray 均安全 */
            lv_darray_free((lvDArray *) field_ptr);
            break;

        case LV_FIELD_DARRAY_ELEMS: {
            /* 先逐元素销毁（元素含内部资源时），再整体释放数组 */
            lvDArray *arr = (lvDArray *) field_ptr;
            if (arr->data && f->u.elem_destroy) {
                for (int k = 0; k < arr->count; k++) {
                    void *elem = lv_darray_get(arr, k);
                    if (elem)
                        f->u.elem_destroy(elem);
                }
            }
            lv_darray_free(arr);
            break;
        }

        case LV_FIELD_OBJECT: {
            /* 调用对象自身的 destroy 回调，随后置 NULL */
            void **pp = (void **) field_ptr;
            if (*pp && f->u.object_destroy)
                f->u.object_destroy(*pp);
            *pp = NULL;
            break;
        }

        case LV_FIELD_ARRAY_ELEMS: {
            /* 指针数组：逐元素回调销毁后，释放数组外壳本身。
             * 元素个数取自 count_offset 指向的 int 字段。 */
            void **arr;
            memcpy(&arr, field_ptr, sizeof(arr));
            if (arr && f->u.elem_destroy) {
                int count = (f->count_offset != 0) ? *(const int *) ((const char *) obj + f->count_offset) : 0;
                for (int k = 0; k < count; k++) {
                    if (arr[k])
                        f->u.elem_destroy(arr[k]);
                }
            }
            lv_free((void **) field_ptr);
            break;
        }

        case LV_FIELD_CUSTOM:
            if (f->u.custom_fn)
                f->u.custom_fn(obj, field_ptr);
            break;
        }
    }
}