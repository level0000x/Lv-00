/**
 * @file hip_backend.h
 * @brief AMD HIP GPU 数值后端 API
 *
 * 提供 HIP 后端的向量/矩阵/线性求解器操作表注册。
 * 需要 ROCm 平台 (HIP) 编译。
 */

#ifndef lv_HIP_BACKEND_H
#define lv_HIP_BACKEND_H

#include "lv/numerical_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 注册 HIP 后端操作表到后端注册表 */
int lv_hip_register_backend(void);

/** @brief 检查 HIP 是否可用 */
int lv_hip_available(void);

/** @brief 获取 HIP 设备数 */
int lv_hip_device_count(void);

/** @brief 获取 HIP 运行时版本字符串 */
const char *lv_hip_backend_version(void);

#ifdef __cplusplus
}
#endif

#endif /* lv_HIP_BACKEND_H */
