/**
 * utils.js - 通用工具函数模块
 *
 * 从 app.js 中提取的通用工具方法，挂载到 Lv00WebApp.prototype 上。
 * 包含防抖、节流、输入值获取、ID 解析等常用辅助函数。
 *
 * @description 该模块提供了一系列前端通用工具函数，供其他模块通过 prototype 调用。
 *              本模块不包含模块级硬编码常量，所有工具函数参数均由调用方传入。
 *              全局常量通过 js/constants.js 统一管理，由 Lv00Const 命名空间提供。
 * @module utils
 * @requires Lv00WebApp 构造函数已定义
 * @requires js/constants.js（全局常量配置）
 * @since 3.0.0
 */

(function() {
    'use strict';

    // ================================================================
    /**
     * 防抖函数 (Debounce)
     *
     * @description 在指定等待时间内，如果函数被再次调用则重新计时。
     *              适用于窗口 resize 等高频事件场景。
     *
     * @param {Function} func - 需要防抖的目标函数
     * @param {number} wait - 等待时间（毫秒），在此时间内重复调用会重置计时器
     * @returns {Function} 返回经过防抖包装后的新函数
     *
     * @example
     *   window.addEventListener('resize', app._debounce(resizeHandler, 100));
     */
    // ================================================================
    Lv00WebApp.prototype._debounce = function(func, wait) {
        if (typeof func !== 'function') {
            console.error('[Lv-00] _debounce: 参数 func 不是函数');
            return function() {};
        }
        var timeout;
        return function() {
            var context = this;
            var args = arguments;
            clearTimeout(timeout);
            timeout = setTimeout(function() {
                func.apply(context, args);
            }, wait);
        };
    };

    // ================================================================
    /**
     * 节流函数 (Throttle)
     *
     * @description 在指定时间间隔内最多执行一次函数。
     *              适用于鼠标移动、滚动等需要限制频率的事件。
     *
     * @param {Function} func - 需要节流的目标函数
     * @param {number} limit - 节流间隔时间（毫秒）
     * @returns {Function} 返回经过节流包装后的新函数
     *
     * @example
     *   canvas.addEventListener('mousemove', app._throttle(moveHandler, 16));
     */
    // ================================================================
    Lv00WebApp.prototype._throttle = function(func, limit) {
        if (typeof func !== 'function') {
            console.error('[Lv-00] _throttle: 参数 func 不是函数');
            return function() {};
        }
        var inThrottle;
        return function() {
            var context = this;
            var args = arguments;
            if (!inThrottle) {
                func.apply(context, args);
                inThrottle = true;
                setTimeout(function() { inThrottle = false; }, limit);
            }
        };
    };

    // ================================================================
    /**
     * 获取输入框的字符串值
     *
     * @description 安全获取指定 DOM 元素的输入值，当元素不存在或出错时返回默认值。
     *
     * @param {string} id - 输入框的 DOM 元素 ID
     * @param {string} [fallback=''] - 获取失败时的默认返回值
     * @returns {string} 输入框的值，获取失败返回 fallback
     */
    // ================================================================
    Lv00WebApp.prototype._getInputValue = function(id, fallback) {
        if (!id || typeof id !== 'string') {
            console.warn('[Lv-00] _getInputValue: 无效的 ID 参数:', id);
            return fallback || '';
        }
        try {
            var el = document.getElementById(id);
            if (el) return el.value;
        } catch (e) {
            console.warn('[Lv-00] _getInputValue: 获取元素 ' + id + ' 失败:', e.message);
        }
        return fallback || '';
    };

    // ================================================================
    /**
     * 获取输入框的整数值
     *
     * @description 安全获取指定 DOM 元素的输入值并解析为整数。
     *
     * @param {string} id - 输入框的 DOM 元素 ID
     * @param {number} [fallback=0] - 获取失败时的默认返回值
     * @returns {number} 解析后的整数值，获取失败返回 fallback
     */
    // ================================================================
    Lv00WebApp.prototype._getInputInt = function(id, fallback) {
        if (!id || typeof id !== 'string') {
            console.warn('[Lv-00] _getInputInt: 无效的 ID 参数:', id);
            return (fallback !== undefined) ? fallback : 0;
        }
        try {
            var el = document.getElementById(id);
            if (el) {
                var v = parseInt(el.value, 10);
                if (!isNaN(v)) return v;
            }
        } catch (e) {
            console.warn('[Lv-00] _getInputInt: 获取元素 ' + id + ' 失败:', e.message);
        }
        return (fallback !== undefined) ? fallback : 0;
    };

    // ================================================================
    /**
     * 获取输入框的浮点数值
     *
     * @description 安全获取指定 DOM 元素的输入值并解析为浮点数。
     *
     * @param {string} id - 输入框的 DOM 元素 ID
     * @param {number} [fallback=0] - 获取失败时的默认返回值
     * @returns {number} 解析后的浮点数值，获取失败返回 fallback
     */
    // ================================================================
    Lv00WebApp.prototype._getInputFloat = function(id, fallback) {
        if (!id || typeof id !== 'string') {
            console.warn('[Lv-00] _getInputFloat: 无效的 ID 参数:', id);
            return (fallback !== undefined) ? fallback : 0;
        }
        try {
            var el = document.getElementById(id);
            if (el) {
                var v = parseFloat(el.value);
                if (!isNaN(v)) return v;
            }
        } catch (e) {
            console.warn('[Lv-00] _getInputFloat: 获取元素 ' + id + ' 失败:', e.message);
        }
        return (fallback !== undefined) ? fallback : 0;
    };

    // ================================================================
    /**
     * 解析逗号分隔的 ID 列表字符串
     *
     * @description 将逗号分隔的字符串解析为数字 ID 数组，自动过滤空值和无效值。
     *              例如: "1, 2, 3" => [1, 2, 3]
     *
     * @param {string} str - 逗号分隔的 ID 字符串
     * @returns {number[]} 解析后的 ID 数组，失败返回空数组
     */
    // ================================================================
    Lv00WebApp.prototype._parseIdList = function(str) {
        if (!str || typeof str !== 'string') {
            return [];
        }
        try {
            var parts = str.split(',');
            var ids = [];
            for (var i = 0; i < parts.length; i++) {
                var trimmed = parts[i].replace(/\s/g, '');
                if (trimmed.length > 0) {
                    var id = parseInt(trimmed, 10);
                    if (!isNaN(id)) ids.push(id);
                }
            }
            return ids;
        } catch (e) {
            console.warn('[Lv-00] _parseIdList: 解析失败:', e.message);
            return [];
        }
    };

    // ================================================================
    /**
     * 解析节点 ID 字符串
     *
     * @description 支持 "n5" 或 "5" 格式，返回纯数字 ID。
     *              自动去除空白字符和前缀 "n"/"N"。
     *
     * @param {string} str - 节点 ID 字符串（如 "n5" 或 "5"）
     * @returns {number} 解析后的数字 ID，失败返回 -1
     *
     * @example
     *   app._parseNodeId('n5')  // => 5
     *   app._parseNodeId('5')   // => 5
     *   app._parseNodeId('N10') // => 10
     */
    // ================================================================
    Lv00WebApp.prototype._parseNodeId = function(str) {
        if (!str) return -1;
        try {
            str = String(str).replace(/\s/g, '');
            // 去除前缀 "n" 或 "N"
            if (str.charAt(0) === 'n' || str.charAt(0) === 'N') {
                str = str.substring(1);
            }
            var id = parseInt(str, 10);
            return isNaN(id) ? -1 : id;
        } catch (e) {
            console.warn('[Lv-00] _parseNodeId: 解析失败:', e.message);
            return -1;
        }
    };

})();
