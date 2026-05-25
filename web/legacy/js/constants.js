/**
 * constants.js - Lv-00 全局常量配置文件
 *
 * @description 集中管理所有 Web 前端 JS 文件中分散的硬编码常量。
 *              按模块划分命名空间，挂载到全局对象 window.Lv00Const 上。
 *              所有模块通过引用 Lv00Const.XXX 获取常量值，便于统一管理和调试。
 *
 * 模块划分：
 *   - app:        应用级常量（缩放范围、性能监控、递归深度等）
 *   - render:     渲染模块常量（网格、点半径、线宽等）
 *   - interaction:交互模块常量（缩放步进、命中阈值、偏移量等）
 *   - ui:         UI 模块常量（日志条数、通知时长、搜索结果数等）
 *   - undo:       撤销/重做模块常量（历史栈深度）
 *   - streaming:  流式输出模块常量（SSE 超时、重连参数等）
 *   - c_backend:  C 后端常量（JSON 缓冲区大小，仅供参考，C 文件无法直接导入）
 *
 * 使用方式：
 *   在 HTML 中确保 constants.js 在其他 JS 文件之前加载：
 *   <script src="js/constants.js"></script>
 *   <script src="js/app.js"></script>
 *   ...
 *
 *   各 JS 模块中引用：
 *   var scaleMin = Lv00Const.app.SCALE_MIN;
 *
 * @module constants
 * @author Lv-00 Team
 * @version 1.0.0
 * @since 3.1.0
 */
(function() {
    'use strict';

    /**
     * 全局常量命名空间
     * @namespace Lv00Const
     */
    window.Lv00Const = {

        // ================================================================
        //  app 模块 - 应用级常量
        //  中文说明：控制缩放范围、性能统计间隔、递归深度和 EMA 平滑权重
        // ================================================================
        app: {
            /** @constant {number} 最小缩放比例 - 防止过度缩小导致数值溢出 */
            SCALE_MIN: 0.01,
            /** @constant {number} 最大缩放比例 - 防止过度放大导致性能问题 */
            SCALE_MAX: 10000,
            /** @constant {number} 导出后 DOM 清理延迟（毫秒）- 避免浏览器拦截下载 */
            EXPORT_CLEANUP_MS: 100,
            /** @constant {number} 性能监控统计间隔（毫秒）- FPS 数据更新频率 */
            PERF_MONITOR_MS: 1000,
            /** @constant {number} 默认递归深度 - 用于递归上下文初始化 */
            RECURSION_DEPTH: 10,
            /** @constant {number} EMA 平滑权重（新值占比）- 用于渲染耗时指数移动平均 */
            EMA_ALPHA: 0.1
        },

        // ================================================================
        //  render 模块 - 渲染常量
        //  中文说明：控制 Canvas 渲染中的网格间距、节点大小、线宽和缩放阈值
        // ================================================================
        render: {
            /** @constant {number} 基础网格间距（CSS 像素）*/
            BASE_GRID_SIZE: 50,
            /** @constant {number} 缩放级别阈值 1：scale < 0.1 时网格变疏 10 倍 */
            GRID_SCALE_L1: 0.1,
            /** @constant {number} 缩放级别阈值 2 */
            GRID_SCALE_L2: 0.5,
            /** @constant {number} 缩放级别阈值 3：默认间距 */
            GRID_SCALE_L3: 1,
            /** @constant {number} 缩放级别阈值 4 */
            GRID_SCALE_L4: 2,
            /** @constant {number} 缩放级别阈值 5 */
            GRID_SCALE_L5: 5,
            /** @constant {number} 缩放级别阈值 6：scale >= 10 时网格变密 10 倍 */
            GRID_SCALE_L6: 10,
            /** @constant {number} 极大缩小时网格倍率 */
            GRID_MULT_10: 10,
            /** @constant {number} 网格倍率 5 */
            GRID_MULT_5: 5,
            /** @constant {number} 网格倍率 2 */
            GRID_MULT_2: 2,
            /** @constant {number} 网格除数 2 */
            GRID_DIV_2: 2,
            /** @constant {number} 网格除数 5 */
            GRID_DIV_5: 5,
            /** @constant {number} 极大放大时网格除数 */
            GRID_DIV_10: 10,
            /** @constant {number} 普通节点半径（CSS 像素）*/
            POINT_RADIUS_NORMAL: 4,
            /** @constant {number} 选中/悬停节点半径（CSS 像素）*/
            POINT_RADIUS_ACTIVE: 6,
            /** @constant {number} 外圈边框偏移量（CSS 像素）*/
            POINT_OUTER_OFFSET: 3,
            /** @constant {number} 线段绘制线宽（CSS 像素）*/
            SEGMENT_LINE_WIDTH: 2,
            /** @constant {number} 坐标轴线宽（CSS 像素）*/
            AXIS_LINE_WIDTH: 2,
            /** @constant {number} 指数移动平均（EMA）新值权重 - 用于渲染耗时平滑 */
            PERF_EMA_ALPHA: 0.1
        },

        // ================================================================
        //  interaction 模块 - 交互常量
        //  中文说明：控制缩放步进、命中检测阈值、探测提示偏移和滚轮平滑参数
        // ================================================================
        interaction: {
            /** @constant {number} 键盘缩放步进因子 - 每次按键缩放比例 */
            ZOOM_FACTOR_STEP: 1.2,
            /** @constant {number} 滚轮平滑缩放因子 - 用于 Math.pow 指数计算 */
            ZOOM_FACTOR_SMOOTH: 1.0015,
            /** @constant {number} 命中检测基础阈值（CSS 像素）- 用于点检测 */
            HIT_THRESHOLD_BASE: 10,
            /** @constant {number} 探测提示框相对于鼠标的偏移量（CSS 像素）*/
            PROBE_TOOLTIP_OFFSET: 15
        },

        // ================================================================
        //  ui 模块 - UI 常量
        //  中文说明：控制日志缓冲区、通知显示时长和搜索结果上限
        // ================================================================
        ui: {
            /** @constant {number} 日志最大条目数 - 防止内存泄漏 */
            MAX_LOG_ENTRIES: 500,
            /** @constant {number} Toast 隐藏动画时长（毫秒）*/
            TOAST_HIDE_ANIMATION_MS: 300,
            /** @constant {number} 默认通知显示时长（毫秒）*/
            TOAST_DURATION_DEFAULT: 3000,
            /** @constant {number} 警告通知显示时长（毫秒）*/
            TOAST_DURATION_WARNING: 4000,
            /** @constant {number} 错误通知显示时长（毫秒）*/
            TOAST_DURATION_ERROR: 5000,
            /** @constant {number} 搜索结果最大显示数 */
            MAX_SEARCH_RESULTS: 10
        },

        // ================================================================
        //  undo 模块 - 撤销/重做常量
        //  中文说明：控制撤销历史记录的最大深度
        // ================================================================
        undo: {
            /** @constant {number} 最大撤销历史深度 - 超出时移除最早记录 */
            MAX_UNDO_STACK: 50
        },

        // ================================================================
        //  streaming 模块 - 流式输出常量
        //  中文说明：控制 SSE 连接超时和指数退避重连参数
        // ================================================================
        streaming: {
            /** @constant {number} SSE 默认超时时长（毫秒）- 30 秒无消息则断开 */
            SSE_DEFAULT_TIMEOUT_MS: 30000,
            /** @constant {number} SSE 重连间隔基数（毫秒）- 指数退避起点 */
            SSE_RETRY_BASE_MS: 1000,
            /** @constant {number} SSE 重连最大延迟（毫秒）- 指数退避上限 */
            SSE_RETRY_MAX_DELAY_MS: 30000,
            /** @constant {number} 事件类型最小值（含 SSE 连接事件 -1）*/
            EVENT_TYPE_MIN: -1,
            /** @constant {number} 事件类型最大值 */
            EVENT_TYPE_MAX: 46
        },

        // ================================================================
        //  c_backend - C 后端常量（仅供参考）
        //  中文说明：C 文件 lv00_web_bindings_v2.c 中的常量，
        //            无法直接导入 JS，此处仅为文档记录。
        // ================================================================
        c_backend: {
            /** @constant {number} JSON 输出缓冲区最大大小（64KB）*/
            JSON_BUF_SIZE: 65536
        },

        // ================================================================
        //  integrate 模块 - 集成加载常量
        //  中文说明：控制扩展面板加载超时和流式初始化重试间隔
        // ================================================================
        integrate: {
            /** @constant {number} 模块加载超时时间（毫秒）- 默认 10 秒 */
            LOAD_TIMEOUT: 10000
        }
    };

    // ================================================================
    //  向后兼容别名
    //  中文说明：为保持与旧代码的兼容性，同时将常用顶层常量
    //            以扁平化方式暴露在 Lv00Const 上。
    // ================================================================
    var appConst = window.Lv00Const.app;
    var renderConst = window.Lv00Const.render;
    var interactionConst = window.Lv00Const.interaction;
    var uiConst = window.Lv00Const.ui;
    var undoConst = window.Lv00Const.undo;
    var streamingConst = window.Lv00Const.streaming;

    // 暴露扁平常量别名（部分旧代码直接使用 Lv00Const.SCALE_MIN 形式）
    window.Lv00Const.SCALE_MIN            = appConst.SCALE_MIN;
    window.Lv00Const.SCALE_MAX            = appConst.SCALE_MAX;
    window.Lv00Const.EXPORT_CLEANUP_MS    = appConst.EXPORT_CLEANUP_MS;
    window.Lv00Const.PERF_MONITOR_MS      = appConst.PERF_MONITOR_MS;
    window.Lv00Const.RECURSION_DEPTH      = appConst.RECURSION_DEPTH;
    window.Lv00Const.EMA_ALPHA            = appConst.EMA_ALPHA;

})();
