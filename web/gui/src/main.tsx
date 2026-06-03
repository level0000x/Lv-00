/**
 * main.tsx - Lv-00 GUI 应用入口点（优化版）
 *
 * @description React 18 应用启动入口文件。
 *              负责初始化 React 根节点、加载样式文件并渲染 App 组件。
 *
 * 初始化流程：
 *   1. 导入 React 18 的 StrictMode 组件
 *   2. 创建 React 根节点（createRoot）
 *   3. 验证 DOM 中是否存在 #root 元素
 *   4. 加载全局样式文件
 *   5. 渲染 App 组件到根节点
 *
 * StrictMode 说明：
 *   - React 18 新增的开发环境辅助工具
 *   - 启用额外的检查和警告以帮助发现潜在问题
 *   - 在开发环境下会双重渲染组件以检测不纯的副作用
 *
 * @module main
 * @since 3.0.0
 */

import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import App from './App';
import './styles/variables.css';
import './styles/global.css';
import './styles/components.css';
import './styles/animations.css';
import './styles/panel-forms.css';
import './styles/shortcut-help.css';
import './services/logger';

/**
 * 应用入口函数
 *
 * @description 初始化 React 18 应用并挂载到 DOM。
 *
 * 初始化步骤：
 *   1. 获取 DOM 中的 #root 元素
 *   2. 如果元素不存在，抛出错误提示
 *   3. 使用 createRoot 创建 React 18 根节点
 *   4. 调用 render 方法渲染 App 组件
 *
 * 错误处理：
 *   - 如果找不到 #root 元素，应用无法启动
 *   - 会在控制台输出清晰的错误信息
 *
 * @throws {Error} 如果 DOM 中不存在 #root 元素
 */
const rootElement = document.getElementById('root');
if (!rootElement) {
  throw new Error('[Lv-00] 启动失败：未找到 #root DOM 元素。请检查 index.html 中是否包含 <div id="root"></div>。');
}

/**
 * 创建 React 应用根节点并渲染
 *
 * 使用 React 18 的 createRoot API：
 *   - createRoot 支持并发渲染特性
 *   - StrictMode 启用额外的开发检查
 *   - 自动处理 React 18 的 Suspense 和流式 SSR
 */
createRoot(rootElement).render(
  <StrictMode>
    <App />
  </StrictMode>,
);
