/**
 * @module components/common/ErrorBoundary
 * @description 全局错误边界（Error Boundary）组件。
 *              捕获子组件渲染过程中抛出的未处理异常，
 *              展示友好的错误信息界面并提供"重试"机制。
 *
 *              注意：ErrorBoundary 必须使用 React class component 实现，
 *              因为 React 目前不支持在函数组件中使用
 *              componentDidCatch / getDerivedStateFromError。
 */

import React from 'react';

// ================================================================
// 类型定义 / Type Definitions
// ================================================================

/**
 * ErrorBoundary 组件的 Props 接口
 * @property children - 需要被错误边界保护的子组件树
 * @property fallbackRender - 可选的渲染回调，接收 error 对象，
 *                            返回自定义的降级 UI。
 *                            如果不提供，将使用默认的错误展示界面。
 */
export interface ErrorBoundaryProps {
  children: React.ReactNode;
  fallbackRender?: (error: Error) => React.ReactNode;
}

/**
 * ErrorBoundary 组件的 State 接口
 * @property hasError - 是否捕获到错误
 * @property error - 捕获到的错误对象（未出错时为 null）
 */
interface ErrorBoundaryState {
  hasError: boolean;
  error: Error | null;
}

// ================================================================
// 组件实现 / Component Implementation
// ================================================================

/**
 * ErrorBoundary - 全局错误边界组件
 *
 * 功能：
 * - 捕获子组件渲染过程中的未处理异常
 * - 展示友好的错误信息，包括错误消息和堆栈跟踪
 * - 提供"重试"按钮，重置错误状态并重新渲染子组件
 * - 支持通过 fallbackRender 自定义降级 UI
 *
 * 使用示例：
 * ```tsx
 * <ErrorBoundary fallbackRender={(err) => <CustomErrorPage error={err} />}>
 *   <App />
 * </ErrorBoundary>
 * ```
 */
class ErrorBoundary extends React.Component<ErrorBoundaryProps, ErrorBoundaryState> {
  constructor(props: ErrorBoundaryProps) {
    super(props);
    this.state = { hasError: false, error: null };
  }

  /**
   * 静态方法：从错误中派生新的 state。
   * 当子组件抛出异常时，React 会调用此方法。
   *
   * @param error - 捕获到的错误对象
   * @returns 新的 state，包含错误信息
   */
  static getDerivedStateFromError(error: Error): ErrorBoundaryState {
    return { hasError: true, error };
  }

  /**
   * 错误日志记录。
   * 在这里可以集成外部错误上报服务（如 Sentry、LogRocket 等）。
   *
   * @param error - 捕获到的错误对象
   * @param errorInfo - 包含组件堆栈的附加信息
   */
  componentDidCatch(error: Error, errorInfo: React.ErrorInfo): void {
    console.error('[ErrorBoundary] 捕获到未处理错误:', error);
    console.error('[ErrorBoundary] 组件堆栈:', errorInfo.componentStack);
  }

  /**
   * 重置错误状态，重新尝试渲染子组件。
   * 由"重试"按钮触发。
   */
  private handleRetry = (): void => {
    this.setState({ hasError: false, error: null });
  };

  render(): React.ReactNode {
    const { hasError, error } = this.state;
    const { children, fallbackRender } = this.props;

    // 未出错时正常渲染子组件
    if (!hasError || !error) {
      return children;
    }

    // 如果提供了自定义降级渲染，优先使用
    if (fallbackRender) {
      return fallbackRender(error);
    }

    // 默认的错误展示界面：居中布局，包含错误图标、信息和重试按钮
    return (
      <div className="error-boundary">
        {/* 错误图标 */}
        <div className="error-boundary__icon">
          {'\u26A0'}
        </div>

        {/* 错误标题 */}
        <div className="error-boundary__title">
          应用遇到了一个错误
        </div>

        {/* 错误消息 */}
        <div className="error-boundary__message">
          {error.message || '未知错误'}
        </div>

        {/* 错误堆栈详情 */}
        {error.stack && (
          <pre className="error-boundary__details">
            {error.stack}
          </pre>
        )}

        {/* 重试按钮 */}
        <button className="error-boundary__retry" onClick={this.handleRetry}>
          重试
        </button>
      </div>
    );
  }
}

export default ErrorBoundary;
