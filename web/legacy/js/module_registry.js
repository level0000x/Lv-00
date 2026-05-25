/**
 * @file module_registry.js
 * @brief 模块注册表（已废弃）
 *
 * 【状态：废弃】当前未被 index.html 加载，保留仅供参考。
 * 如需启用，请在 index.html 中添加对应的 <script> 标签。
 * @deprecated 此模块当前未使用，将在未来版本中移除。
 */

var Lv00ModuleRegistry = (function() {
    'use strict';

    /**
     * 内部模块存储对象
     * 键为模块名称（字符串），值为模块配置对象
     * @type {Object<string, Object>}
     */
    var _modules = {};

    // ================================================================
    //  注册模块
    //  将模块信息添加到注册表中。如果模块名已存在，会覆盖旧配置
    //  并输出警告日志。
    //
    //  @param {string} name - 模块名称（唯一标识符，如 "formula", "debug"）
    //  @param {Object} config - 模块配置对象
    //  @param {string} config.label - 模块显示标签
    //  @param {string} [config.icon] - 模块图标字符
    //  @param {string} [config.panelId] - 对应面板的 DOM ID
    //  @param {string} [config.tabId] - 对应标签按钮的 data-module 值
    //  @param {boolean} [config.enabled] - 是否启用（默认 true）
    //  @param {Object} [config.extra] - 额外的自定义数据
    //  @returns {boolean} 注册是否成功
    // ================================================================
    function register(name, config) {
        // 守卫：模块名无效
        if (typeof name !== 'string' || name.length === 0) {
            console.warn('[Lv00ModuleRegistry] 注册失败：模块名无效');
            return false;
        }

        // 守卫：配置对象无效
        if (!config || typeof config !== 'object') {
            console.warn('[Lv00ModuleRegistry] 注册失败：配置对象无效');
            return false;
        }

        // 检测重复注册
        if (_modules.hasOwnProperty(name)) {
            console.warn('[Lv00ModuleRegistry] 模块 "' + name + '" 已存在，将被覆盖');
        }

        // 构建标准配置对象（使用默认值填充缺失字段）
        var normalized = {
            name: name,
            label: config.label || name,
            icon: config.icon || '',
            panelId: config.panelId || ('panel' + name.charAt(0).toUpperCase() + name.slice(1)),
            tabId: config.tabId || name,
            enabled: config.hasOwnProperty('enabled') ? !!config.enabled : true,
            extra: config.extra || {}
        };

        _modules[name] = normalized;
        return true;
    }

    // ================================================================
    //  获取模块
    //  根据模块名获取已注册的模块配置。未注册时返回 null。
    //
    //  @param {string} name - 模块名称
    //  @returns {Object|null} 模块配置对象，未找到返回 null
    // ================================================================
    function getModule(name) {
        // 守卫：模块名无效
        if (typeof name !== 'string' || name.length === 0) {
            return null;
        }
        return _modules.hasOwnProperty(name) ? _modules[name] : null;
    }

    // ================================================================
    //  获取所有模块
    //  返回注册表中所有已注册模块配置的浅拷贝数组。
    //
    //  @returns {Array<Object>} 所有模块配置对象的数组
    // ================================================================
    function getAll() {
        var result = [];
        for (var key in _modules) {
            if (_modules.hasOwnProperty(key)) {
                result.push(_modules[key]);
            }
        }
        return result;
    }

    // ================================================================
    //  获取所有启用的模块
    //  返回所有 enabled 为 true 的模块配置的浅拷贝数组。
    //
    //  @returns {Array<Object>} 所有已启用模块配置对象的数组
    // ================================================================
    function getEnabled() {
        var result = [];
        for (var key in _modules) {
            if (_modules.hasOwnProperty(key) && _modules[key].enabled) {
                result.push(_modules[key]);
            }
        }
        return result;
    }

    // ================================================================
    //  检查模块是否已注册
    //
    //  @param {string} name - 模块名称
    //  @returns {boolean} 模块是否已注册
    // ================================================================
    function has(name) {
        if (typeof name !== 'string' || name.length === 0) {
            return false;
        }
        return _modules.hasOwnProperty(name);
    }

    // ================================================================
    //  设置模块启用/禁用状态
    //
    //  @param {string} name - 模块名称
    //  @param {boolean} enabled - 是否启用
    //  @returns {boolean} 设置是否成功
    // ================================================================
    function setEnabled(name, enabled) {
        if (typeof name !== 'string' || !_modules.hasOwnProperty(name)) {
            return false;
        }
        _modules[name].enabled = !!enabled;
        return true;
    }

    // ================================================================
    //  注销模块
    //  从注册表中移除指定模块。如果模块不存在，静默返回 false。
    //
    //  @param {string} name - 模块名称
    //  @returns {boolean} 是否成功注销
    // ================================================================
    function unregister(name) {
        if (typeof name !== 'string' || !_modules.hasOwnProperty(name)) {
            return false;
        }
        delete _modules[name];
        return true;
    }

    // ================================================================
    //  获取已注册模块的数量
    //
    //  @returns {number} 已注册模块总数
    // ================================================================
    function count() {
        var n = 0;
        for (var key in _modules) {
            if (_modules.hasOwnProperty(key)) {
                n++;
            }
        }
        return n;
    }

    // ================================================================
    //  批量注册模块
    //  接收一个模块配置数组，依次调用 register 注册每个模块。
    //
    //  @param {Array<Object>} modules - 模块配置数组
    //  @returns {Object} 结果汇总 { success: number, failed: number }
    // ================================================================
    function registerAll(modules) {
        if (!modules || typeof modules.length !== 'number') {
            return { success: 0, failed: 0 };
        }

        var successCount = 0;
        var failedCount = 0;

        for (var i = 0; i < modules.length; i++) {
            var mod = modules[i];
            if (mod && mod.name && register(mod.name, mod)) {
                successCount++;
            } else {
                failedCount++;
            }
        }

        return {
            success: successCount,
            failed: failedCount
        };
    }

    // ================================================================
    //  返回公共 API
    // ================================================================
    return {
        register: register,
        get: getModule,
        getAll: getAll,
        getEnabled: getEnabled,
        has: has,
        setEnabled: setEnabled,
        unregister: unregister,
        count: count,
        registerAll: registerAll
    };

})();
