/**
 * @file cuda_backend.h
 * @brief CUDA GPU 数值后端 API
 *
 * 提供 CUDA 后端的向量/矩阵/线性求解器操作表注册。
 * 需要 CUDA Toolkit (NVCC) 编译。
 */

#ifndef lv_CUDA_BACKEND_H
#define lv_CUDA_BACKEND_H

#include "lv/numerical_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 注册 CUDA 后端操作表到后端注册表 */
int lv_cuda_register_backend(void);

/** @brief 检查 CUDA 是否可用（GPU 设备数和运行时版本） */
int lv_cuda_available(void);

/** @brief 获取 CUDA 设备数 */
int lv_cuda_device_count(void);

/** @brief 获取 CUDA 工具箱版本字符串 */
const char *lv_cuda_backend_version(void);

#ifdef __cplusplus
}
#endif

#endif /* lv_CUDA_BACKEND_H */
