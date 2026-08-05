/**
 * @file proof_version.c
 * @brief 证明版本管理与序列化（容器文件）
 *
 * @details 实现证明版本管理、自然语言导出、Sledgehammer 策略调度、
 *          Isar 导出、HOL Light 微内核验证、F* 精化类型验证等。
 *
 *          本文件已按功能域拆分为以下模块：
 *          - proof_version_nl.c       自然语言导出与策略注释
 *          - proof_version_ghost.c    幽灵标记与引导填充
 *          - proof_version_sledge.c   Sledgehammer 自动证明策略调度
 *          - proof_version_task.c     任务系统与备选占位
 *          - proof_version_isar.c     Isar 导出与 HOL Light 微内核验证
 *          - proof_version_refine.c   F* 精化类型与 SMT 混合验证
 *
 * @author Lv-00 Project
 * @version 3.3.0
 *
 * 注意：本文件为纯容器文件，当前不含任何代码实体（已按功能域拆分完毕），
 *       故不再保留 include；若后续补充实现，请按需添加对应头文件。
 */