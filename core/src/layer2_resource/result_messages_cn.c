/**
 * @file result_messages_cn.c
 * @brief Lv-00 中文结果信息转换系统实现
 *
 * @details 实现枚举值到中文字符串的映射转换函数。
 *          所有中文描述均以静态字符串形式存储，确保无需释放内存。
 *
 * @author Lv-00 Project
 * @version 3.5.0
 */

#include "result_messages_cn.h"

#include <stdio.h>
#include <string.h>

/* ============================================================
 * 函数块系统结果转中文实现
 * ============================================================ */

/**
 * @brief 确定性状态中文描述表
 */
static const char *g_determinism_state_cn[] = {
    "未验证",        /* DETERMINISM_STATE_UNVERIFIED */
    "已验证",        /* DETERMINISM_STATE_VERIFIED */
    "非确定性",      /* DETERMINISM_STATE_NON_DETERMINISTIC */
    "部分验证",      /* DETERMINISM_STATE_PARTIALLY_VERIFIED */
};

/**
 * @brief 确定性状态英文缩写表
 */
static const char *g_determinism_state_abbr[] = {
    "UNVERIFIED",        /* DETERMINISM_STATE_UNVERIFIED */
    "VERIFIED",          /* DETERMINISM_STATE_VERIFIED */
    "NON_DET",           /* DETERMINISM_STATE_NON_DETERMINISTIC */
    "PARTIAL",           /* DETERMINISM_STATE_PARTIALLY_VERIFIED */
};

/**
 * @brief 打包结果中文描述表
 */
static const char *g_pack_result_cn[] = {
    "打包成功",                        /* PACK_RESULT_OK */
    "存在跨边界约束",                 /* PACK_RESULT_CROSS_BOUNDARY_CONFLICT */
    "无效节点",                       /* PACK_RESULT_INVALID_NODES */
    "无效端口",                       /* PACK_RESULT_INVALID_PORTS */
    "无效图",                         /* PACK_RESULT_INVALID_GRAPH */
    "内存不足",                       /* PACK_RESULT_OUT_OF_MEMORY */
    "用户取消",                       /* PACK_RESULT_CANCELLED */
};

/**
 * @brief 实例化结果中文描述表
 */
static const char *g_instantiate_result_cn[] = {
    "实例化成功",                     /* LV00_INSTANTIATE_OK */
    "无解",                           /* LV00_INSTANTIATE_NO_SOLUTION */
    "多解",                           /* LV00_INSTANTIATE_MULTIPLE_SOLUTIONS */
    "需要选择器",                     /* LV00_INSTANTIATE_SELECTOR_NEEDED */
    "前置条件不满足",                 /* LV00_INSTANTIATE_PRECONDITION_FAILED */
    "内存不足",                       /* LV00_INSTANTIATE_OUT_OF_MEMORY */
};

/* ============================================================
 * 求解器结果转中文实现
 * ============================================================ */

/**
 * @brief 求解器结果中文描述表
 */
static const char *g_solver_result_cn[] = {
    "求解成功",                       /* ENGINE_SOLVE_SUCCESS */
    "无解",                           /* ENGINE_SOLVE_NO_SOLUTION */
    "多解",                           /* ENGINE_SOLVE_MULTIPLE_SOLUTIONS */
    "超时",                           /* ENGINE_SOLVE_TIMEOUT */
    "错误",                           /* ENGINE_SOLVE_ERROR */
    "未初始化",                       /* ENGINE_SOLVE_UNINITIALIZED */
    "冲突",                           /* ENGINE_SOLVE_CONFLICT */
    "约束不足",                       /* ENGINE_SOLVE_UNDERCONSTRAINED */
    "约束过度",                       /* ENGINE_SOLVE_OVERCONSTRAINED */
    "收敛",                           /* ENGINE_SOLVE_CONVERGED */
    "未收敛",                         /* ENGINE_SOLVE_NOT_CONVERGED */
};

/**
 * @brief 求解器状态中文描述表
 */
static const char *g_solver_status_cn[] = {
    "空闲",                           /* SOLVER_STATUS_IDLE */
    "运行中",                         /* SOLVER_STATUS_RUNNING */
    "已完成",                         /* SOLVER_STATUS_COMPLETED */
    "失败",                           /* SOLVER_STATUS_FAILED */
    "已停止",                         /* SOLVER_STATUS_STOPPED */
};

/* ============================================================
 * 归一化结果转中文实现
 * ============================================================ */

/**
 * @brief 归一化结果中文描述表
 */
static const char *g_normalize_result_cn[] = {
    "归一化成功",                     /* NORMALIZE_SUCCESS */
    "无需归一化",                     /* NORMALIZE_NO_NEED */
    "归一化失败",                     /* NORMALIZE_FAILED */
    "归一化超时",                     /* NORMALIZE_TIMEOUT */
    "达到最大迭代次数",               /* NORMALIZE_MAX_ITERATIONS */
};

/* ============================================================
 * 几何类型转中文实现
 * ============================================================ */

/**
 * @brief 几何节点类型中文描述表
 */
static const char *g_geom_type_cn[] = {
    "点",                             /* GEOM_POINT */
    "线段",                           /* GEOM_LINE_SEGMENT */
    "直线",                           /* GEOM_LINE */
    "射线",                           /* GEOM_RAY */
    "圆",                             /* GEOM_CIRCLE */
    "圆弧",                           /* GEOM_ARC */
    "椭圆",                           /* GEOM_ELLIPSE */
    "多边形",                         /* GEOM_POLYGON */
    "区域",                           /* GEOM_REGION */
    "函数块",                         /* GEOM_FUNCTION_BLOCK */
    "端口",                           /* GEOM_PORT */
    "约束",                           /* GEOM_CONSTRAINT */
};

/**
 * @brief 约束类型中文描述表
 */
static const char *g_constraint_type_cn[] = {
    "关联约束",                        /* CONSTRAINT_INCIDENCE */
    "距离约束",                        /* CONSTRAINT_DISTANCE */
    "角度约束",                        /* CONSTRAINT_ANGLE */
    "垂直约束",                        /* CONSTRAINT_PERPENDICULAR */
    "平行约束",                        /* CONSTRAINT_PARALLEL */
    "相切约束",                        /* CONSTRAINT_TANGENT */
    "相交约束",                        /* CONSTRAINT_INTERSECTION */
    "中点约束",                        /* CONSTRAINT_MIDPOINT */
    "垂直平分约束",                    /* CONSTRAINT_PERPENDICULAR_BISECTOR */
    "角度平分约束",                    /* CONSTRAINT_ANGLE_BISECTOR */
    "包含约束",                        /* CONSTRAINT_CONTAINMENT */
    "同心约束",                        /* CONSTRAINT_CONCENTRIC */
};

/* ============================================================
 * 证明系统结果转中文实现
 * ============================================================ */

/**
 * @brief 证明结果中文描述表
 */
static const char *g_proof_result_cn[] = {
    "证明成功",                        /* PROOF_SUCCESS */
    "证明失败",                        /* PROOF_FAILED */
    "证明超时",                        /* PROOF_TIMEOUT */
    "证明不完整",                      /* PROOF_INCOMPLETE */
    "证明无法验证",                    /* PROOF_UNVERIFIABLE */
    "证明被拒绝",                      /* PROOF_REJECTED */
};

/**
 * @brief 证明状态中文描述表
 */
static const char *g_proof_status_cn[] = {
    "未开始",                          /* PROOF_STATUS_NONE */
    "搜索中",                          /* PROOF_STATUS_SEARCHING */
    "验证中",                          /* PROOF_STATUS_VERIFYING */
    "完成",                            /* PROOF_STATUS_COMPLETED */
    "失败",                            /* PROOF_STATUS_FAILED */
    "超时",                            /* PROOF_STATUS_TIMEOUT */
};

/* ============================================================
 * 公共API实现
 * ============================================================ */

const char *determinism_state_to_string_cn(int state) {
    if (state >= 0 && state < 4) {
        return g_determinism_state_cn[state];
    }
    return "未知状态";
}

const char *determinism_state_to_abbr(int state) {
    if (state >= 0 && state < 4) {
        return g_determinism_state_abbr[state];
    }
    return "UNKNOWN";
}

const char *pack_result_to_string_cn(int result) {
    if (result >= 0 && result < 7) {
        return g_pack_result_cn[result];
    }
    return "未知结果";
}

const char *instantiate_result_to_string_cn(int result) {
    if (result >= 0 && result < 6) {
        return g_instantiate_result_cn[result];
    }
    return "未知结果";
}

const char *solver_result_to_string_cn(int result) {
    if (result >= 0 && result < 11) {
        return g_solver_result_cn[result];
    }
    return "未知结果";
}

const char *solver_status_to_string_cn(int status) {
    if (status >= 0 && status < 5) {
        return g_solver_status_cn[status];
    }
    return "未知状态";
}

const char *normalize_result_to_string_cn(int result) {
    if (result >= 0 && result < 5) {
        return g_normalize_result_cn[result];
    }
    return "未知结果";
}

const char *geom_type_to_string_cn(int type) {
    if (type >= 0 && type < 12) {
        return g_geom_type_cn[type];
    }
    return "未知几何类型";
}

const char *constraint_type_to_string_cn(int type) {
    if (type >= 0 && type < 12) {
        return g_constraint_type_cn[type];
    }
    return "未知约束类型";
}

const char *proof_result_to_string_cn(int result) {
    if (result >= 0 && result < 6) {
        return g_proof_result_cn[result];
    }
    return "未知结果";
}

const char *proof_status_to_string_cn(int status) {
    if (status >= 0 && status < 6) {
        return g_proof_status_cn[status];
    }
    return "未知状态";
}

int format_determinism_state_cn(int state, char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return -1;

    const char *abbr = determinism_state_to_abbr(state);
    const char *cn = determinism_state_to_string_cn(state);

    return snprintf(buf, buf_size, "[%s] %s", abbr, cn);
}

int format_pack_result_cn(int result, char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return -1;

    const char *cn = pack_result_to_string_cn(result);
    return snprintf(buf, buf_size, "%s", cn);
}

int format_solver_result_cn(int result, char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return -1;

    const char *cn = solver_result_to_string_cn(result);
    return snprintf(buf, buf_size, "%s", cn);
}
