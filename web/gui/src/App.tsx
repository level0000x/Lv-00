/**
 * App.tsx - Lv-00 GUI 应用根组件（优化版）
 *
 * @description Lv-00 符号几何引擎 GUI 应用的根组件。
 *              使用 ErrorBoundary 包裹主布局，确保未处理异常
 *              不会导致整个应用白屏崩溃。
 *
 * 组件层级结构：
 *   ErrorBoundary（错误边界保护）
 *   └── Layout（应用外壳：Header + 侧边栏 + Canvas + StatusBar）
 *
 * ErrorBoundary 捕获 Layout 及其所有子组件的渲染异常，
 * 展示友好的错误界面并允许用户重试。
 *
 * @module App
 * @since 3.0.0
 */

import Layout from '@/components/layout/Layout';
import ErrorBoundary from '@/components/common/ErrorBoundary';

/**
 * App - Lv-00 GUI 应用根组件
 *
 * @description 应用入口组件，负责渲染整体应用布局。
 *              包含以下职责：
 *   - 提供错误边界保护，防止单个组件错误导致整个应用崩溃
 *   - 渲染应用主布局组件（Layout）
 *
 * @component
 * @example
 * // 应用启动时会自动渲染此组件
 * <App />
 *
 * @returns {JSX.Element} 应用根组件的 React 元素
 */
export default function App(): JSX.Element {
  return (
    <ErrorBoundary>
      <Layout />
    </ErrorBoundary>
  );
}
