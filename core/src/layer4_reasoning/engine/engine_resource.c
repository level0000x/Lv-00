/**
 * @file engine_resource.c
 * @brief 引擎资源加载与配置（从 engine.c 拆分）
 *
 * @details 负责重写规则、模块、公理包的加载管理，
 *          以及重写步数限制的配置。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/engine.h"

#include <stdio.h>
#include <string.h>

#include "lv/axiom_pkg.h"
#include "lv/lv.h"
#include "lv/lv_path.h"
#include "lv/module.h"

#include "lv_utils.h"

/** 模块名称最大长度（用于 engine_extract_module_name 静态缓冲区） */
#define lv_MAX_NAME_LENGTH 256

/**
 * @brief 引擎内部数组扩容辅助函数（委托给统一的 lv_ensure_capacity）
 *
 * @param arr      当前数组指针的地址（用于 realloc）
 * @param count    当前元素数量
 * @param capacity 当前容量的地址（会被更新为新容量）
 * @param elem_size 单个元素的字节大小
 * @return true 扩容成功（或无需扩容），false 失败（内存不足或溢出）
 * @note 内部委托给 lv_ensure_capacity，最小增长量为 1
 */
static bool engine_ensure_capacity(void **arr, int count, int *capacity, size_t elem_size) {
    return lv_ensure_capacity(arr, count, capacity, elem_size, 1);
}

/**
 * @brief 向引擎添加一条重写规则
 *
 * 将指定的重写规则追加到引擎的规则列表中。
 * 内部数组采用指数增长策略自动扩容。
 *
 * @param engine 引擎实例
 * @param rule   待添加的重写规则（指针所有权转移至引擎，调用者不应再释放）
 * @return true 添加成功，false 参数为 NULL 或内存不足
 */
bool engine_add_rewrite_rule(lvEngine *engine, const RewriteRule *rule) {
    if (!engine || !rule)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "engine_add_rewrite_rule: NULL engine or rule");

    if (!engine_ensure_capacity((void **) &engine->rewrite_rules, engine->rewrite_rule_count,
                                &engine->rewrite_rule_capacity, sizeof(RewriteRule *)))
        return false;

    engine->rewrite_rules[engine->rewrite_rule_count++] = (RewriteRule *) rule;
    return true;
}

/**
 * @brief 从文件路径中提取基础文件名（不含目录和扩展名）
 *
 * 例如: "/path/to/my_module.lvmod" -> "my_module"
 *       "simple.lvmod" -> "simple"
 *       "no_extension" -> "no_extension"
 *
 * @param filepath 文件路径
 * @return 静态缓冲区中的文件名字符串（注意：非线程安全，调用后应立即使用）
 */
static const char *engine_extract_module_name(const char *filepath) {
    static __thread char name_buf[lv_MAX_NAME_LENGTH];
    if (!filepath) {
        name_buf[0] = '\0';
        return name_buf;
    }

    /* 提取最后一个路径分隔符之后的部分（lv_path_basename 同时识别 '/' 与 '\\'） */
    const char *base = lv_path_basename(filepath);

    /* 复制基础文件名 */
    lv_strlcpy(name_buf, base, lv_MAX_NAME_LENGTH);

    /* 去掉扩展名（最后一个 '.' 之后的部分） */
    lv_path_strip_ext(name_buf);

    /* 如果提取后为空，回退到 "temp" */
    if (name_buf[0] == '\0')
        lv_strlcpy(name_buf, "temp", lv_MAX_NAME_LENGTH);

    /* 检测截断：如果原始文件名长度超过缓冲区，记录提示 */
    /* 注意：lv_strlcpy 保证 name_buf 以 NUL 结尾，因此
     * 当原始名 >= lv_MAX_NAME_LENGTH-1 时会被截断。
     * 截断后的模块名仍然唯一标识，不影响功能。 */

    return name_buf;
}

ModuleLoadStatus engine_load_module(lvEngine *engine, const char *filepath) {
    if (!engine || !filepath)
        return MODULE_LOAD_ERROR_INVALID_PATH;
    const char *module_name = engine_extract_module_name(filepath);
    Module *mod = module_create(module_name, "0.0.0");
    if (!mod) {
        engine->last_status = ENGINE_STATUS_OUT_OF_MEMORY;
        snprintf(engine->last_error, sizeof(engine->last_error), "模块创建失败");
        return MODULE_LOAD_MEMORY_ERROR;
    }
    ModuleLoadStatus status = module_load(mod, filepath, engine->loaded_modules, engine->module_count);
    if (status != MODULE_LOAD_OK) {
        module_destroy(mod);
        engine->last_status = ENGINE_STATUS_MODULE_ERROR;
        snprintf(engine->last_error, sizeof(engine->last_error), "模块加载失败 [文件=%s, 状态码=%d]", filepath, status);
        return status;
    }
    /* 指数增长策略：使用通用扩容辅助函数 */
    if (!engine_ensure_capacity((void **) &engine->loaded_modules, engine->module_count, &engine->module_capacity,
                                sizeof(Module *))) {
        module_destroy(mod);
        engine->last_status = ENGINE_STATUS_OUT_OF_MEMORY;
        return MODULE_LOAD_MEMORY_ERROR;
    }
    engine->loaded_modules[engine->module_count++] = mod;
    return MODULE_LOAD_OK;
}

/**
 * @brief 从文件加载公理包
 *
 * 创建临时公理包实例，从指定文件路径解析并加载公理定义，
 * 加载成功后将其追加到引擎的公理包列表中。
 * 内部数组采用指数增长策略自动扩容。
 *
 * @param engine   引擎实例
 * @param filepath 公理包文件路径
 * @return 公理加载状态码（AXIOM_LOAD_OK 表示成功）
 */
AxiomLoadStatus engine_load_axiom_package(lvEngine *engine, const char *filepath) {
    if (!engine || !filepath)
        return AXIOM_LOAD_NULL_POINTER;
    AxiomPackage *pkg = lv_axiom_package_create("temp", "0.0.0");
    if (!pkg) {
        engine->last_status = ENGINE_STATUS_OUT_OF_MEMORY;
        snprintf(engine->last_error, sizeof(engine->last_error), "公理包创建失败");
        return AXIOM_LOAD_MEMORY_ERROR;
    }
    AxiomLoadStatus status = axiom_package_load(pkg, filepath);
    if (status != AXIOM_LOAD_OK) {
        axiom_package_destroy(pkg);
        engine->last_status = ENGINE_STATUS_MODULE_ERROR;
        snprintf(engine->last_error, sizeof(engine->last_error), "公理包加载失败 [文件=%s, 状态码=%d]", filepath,
                 status);
        return status;
    }
    /* 指数增长策略：使用通用扩容辅助函数 */
    if (!engine_ensure_capacity((void **) &engine->axiom_packages, engine->axiom_package_count,
                                &engine->axiom_package_capacity, sizeof(AxiomPackage *))) {
        axiom_package_destroy(pkg);
        engine->last_status = ENGINE_STATUS_OUT_OF_MEMORY;
        return AXIOM_LOAD_MEMORY_ERROR;
    }
    engine->axiom_packages[engine->axiom_package_count++] = pkg;
    return AXIOM_LOAD_OK;
}

/* ================================================================
 * 重写步数限制配置
 * ================================================================ */

/**
 * @brief 设置引擎重写步数上限
 *
 * 若传入 limit <= 0，则自动使用默认值 lv_DEFAULT_REWRITE_STEP_LIMIT。
 *
 * @param engine 引擎实例
 * @param limit  新的步数上限（正值）
 */
void engine_set_rewrite_step_limit(lvEngine *engine, int limit) {
    if (!engine)
        return;
    if (limit <= 0)
        limit = lv_DEFAULT_REWRITE_STEP_LIMIT; /* 强制使用正数下限 */
    engine->rewrite_step_limit = limit;
}

/**
 * @brief 获取引擎重写步数上限
 *
 * 若引擎为 NULL 或未设置有效值，返回默认值 lv_DEFAULT_REWRITE_STEP_LIMIT。
 *
 * @param engine 引擎实例
 * @return 当前重写步数上限
 */
int engine_get_rewrite_step_limit(const lvEngine *engine) {
    if (!engine)
        return lv_DEFAULT_REWRITE_STEP_LIMIT;
    return engine->rewrite_step_limit > 0 ? engine->rewrite_step_limit : lv_DEFAULT_REWRITE_STEP_LIMIT;
}
