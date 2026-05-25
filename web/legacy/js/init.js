/**
 * @file init.js
 * @brief 页面初始化脚本（全局清理与事件委托）
 * @description 从 index.html 内联脚本提取而来，作为独立文件使用 defer 加载。
 *              包含 beforeunload 清理、数值假设模态框事件委托、全局键盘快捷键。
 */
'use strict';

/**
 * 页面卸载前清理所有事件监听器，防止内存泄漏
 * Cleanup all event listeners before page unload to prevent memory leaks
 */
window.addEventListener('beforeunload', function() {
    if (window.lv00App && typeof window.lv00App.cleanup === 'function') {
        // 调用 app.cleanup() 执行完整的资源清理：
        // - 移除所有 DOM 事件监听器（Canvas 交互、键盘快捷键、resize 等）
        // - 取消 requestAnimationFrame 渲染循环
        // - 清理 WebAssembly 内存引用（若已加载 WASM）
        // - 断开 SSE/WebSocket 连接
        // - 释放临时对象引用，帮助 GC 回收
        window.lv00App.cleanup();
    }
});

/**
 * 数值假设模态框关闭事件委托
 * 绑定到 document.body 以捕获 data-action="close-numeric-assumption" 按钮点击
 */
document.addEventListener('DOMContentLoaded', function() {
    document.body.addEventListener('click', function(e) {
        var target = e.target;
        if (target && target.getAttribute('data-action') === 'close-numeric-assumption') {
            var modal = document.getElementById('modalNumericAssumption');
            if (modal) {
                modal.classList.remove('active');
            }
        }
    });

    /**
     * 全局键盘快捷键处理
     * Ctrl+1~9: 切换到对应序号的模块标签
     * Ctrl+Z:   撤销操作（委托给 app.undo）
     * Ctrl+Y:   重做操作（委托给 app.redo）
     *
     * 注意：仅在非输入框（input/textarea/select）聚焦时生效，
     *       避免与文本编辑冲突。
     */
    document.addEventListener('keydown', function(e) {
        // 如果焦点在输入框中，不处理快捷键（避免干扰文本输入）
        var tag = (e.target && e.target.tagName) ? e.target.tagName.toLowerCase() : '';
        if (tag === 'input' || tag === 'textarea' || tag === 'select') {
            return;
        }

        var ctrl = e.ctrlKey || e.metaKey;

        // Ctrl+1~9: 切换模块标签
        if (ctrl && e.key >= '1' && e.key <= '9') {
            e.preventDefault();
            var tabs = document.querySelectorAll('.module-tab');
            var index = parseInt(e.key, 10) - 1;
            if (index >= 0 && index < tabs.length) {
                tabs[index].click();
            }
            return;
        }

        // Ctrl+Z: 撤销
        if (ctrl && e.key === 'z' && !e.shiftKey) {
            e.preventDefault();
            var app = window.lv00App;
            if (app && typeof app.undo === 'function') {
                app.undo();
            }
            return;
        }

        // Ctrl+Y: 重做
        if (ctrl && e.key === 'y') {
            e.preventDefault();
            var app2 = window.lv00App;
            if (app2 && typeof app2.redo === 'function') {
                app2.redo();
            }
            return;
        }
    });
});
