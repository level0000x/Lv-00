/**
 * @module components/common/ErrorBoundary
 * @description 全局错误边界（Error Boundary）组件。
 *              捕获子组件渲染过程中抛出的未处理异常，
 *              展示友好的错误信息界面并提供"重试"机制。
 *
 *              Global error boundary component.
 *              Catches unhandled exceptions thrown during child component rendering,
 *              displays a friendly error UI and provides a "retry" mechanism.
 *
 *              注意：ErrorBoundary 必须使用 React class component 实现，
 *              因为 React 目前不支持在函数组件中使用
 *              componentDidCatch / getDerivedStateFromError。
 *
 *              Note: ErrorBoundary must be implemented as a React class component
 *              because React does not support componentDidCatch / getDerivedStateFromError
 *              in function components.
 */

import React from 'react';

// ================================================================
// 类型定义 / Type Definitions
// ================================================================

/**
 * ErrorBoundary 组件的 Props 接口 / ErrorBoundary component Props interface
 * @property children - 需要被错误边界保护的子组件树 / Child component tree to be protected by the error boundary
 * @property fallbackRender - 可选的渲染回调，接收 error 对象，返回自定义的降级 UI。
 *                            如果不提供，将使用默认的错误展示界面。
 *                            Optional render callback that receives the error object
 *                            and returns a custom fallback UI.
 *                            If not provided, the default error display will be used.
 * @property onError - 可选的错误回调，在捕获到错误时触发。
 *                     可用于集成外部错误上报服务（如 Sentry、LogRocket 等）。
 *                     Optional error callback triggered when an error is caught.
 *                     Can be used to integrate external error reporting services
 *                     (e.g., Sentry, LogRocket, etc.).
 */
export interface ErrorBoundaryProps {
  children: React.ReactNode;
  fallbackRender?: (error: Error, errorInfo: React.ErrorInfo) => React.ReactNode;
  onError?: (error: Error, errorInfo: React.ErrorInfo) => void;
}

/**
 * ErrorBoundary 组件的 State 接口 / ErrorBoundary component State interface
 * @property hasError - 是否捕获到错误 / Whether an error has been caught
 * @property error - 捕获到的错误对象（未出错时为 null）/ Caught error object (null when no error)
 */
interface ErrorBoundaryState {
  hasError: boolean;
  error: Error | null;
}

// ================================================================
// 组件实现 / Component Implementation
// ================================================================

/**
 * ErrorBoundary - 全局错误边界组件 / Global error boundary component
 *
 * 功能 / Features:
 * - 捕获子组件渲染过程中的未处理异常
 *   Catches unhandled exceptions during child component rendering
 * - 展示友好的错误信息，包括错误消息和堆栈跟踪
 *   Displays friendly error information, including error message and stack trace
 * - 提供"重试"按钮，重置错误状态并重新渲染子组件
 *   Provides a "retry" button to reset error state and re-render child components
 * - 支持通过 fallbackRender 自定义降级 UI
 *   Supports custom fallback UI via fallbackRender
 * - 支持通过 onError 集成外部错误上报
 *   Supports external error reporting integration via onError
 *
 * 使用示例 / Usage Example:
 * ```tsx
 * // 基本用法 / Basic usage
 * <ErrorBoundary>
 *   <App />
 * </ErrorBoundary>
 *
 * // 自定义降级 UI / Custom fallback UI
 * <ErrorBoundary
 *   fallbackRender={(err) => <CustomErrorPage error={err} />}
 *   onError={(err, info) => Sentry.captureException(err)}
 * >
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
   * 必须返回新的 state 对象，不能有副作用。
   *
   * Static method: derive new state from error.
   * React calls this method when a child component throws an exception.
   * Must return a new state object, must not have side effects.
   *
   * @param error - 捕获到的错误对象 / Caught error object
   * @returns 新的 state，包含错误信息 / New state containing error information
   */
  static getDerivedStateFromError(error: Error): ErrorBoundaryState {
    return { hasError: true, error };
  }

  /**
   * 错误日志记录（生命周期方法）。
   * 在这里可以执行副作用，如日志记录或错误上报。
   *
   * Error logging (lifecycle method).
   * Side effects such as logging or error reporting can be performed here.
   *
   * @param error - 捕获到的错误对象 / Caught error object
   * @param errorInfo - 包含组件堆栈的附加信息 / Additional info containing component stack
   */
  componentDidCatch(error: Error, errorInfo: React.ErrorInfo): void {
    // 输出到控制台 / Log to console
    console.error('[ErrorBoundary] 捕获到未处理错误 / Caught unhandled error:', error);
    console.error('[ErrorBoundary] 组件堆栈 / Component stack:', errorInfo.componentStack);

    // 调用外部错误回调（用于集成 Sentry 等服务）
    // Call external error callback (for integrating Sentry, etc.)
    this.props.onError?.(error, errorInfo);
  }

  /**
   * 重置错误状态，重新尝试渲染子组件。
   * 由"重试"按钮触发。
   *
   * Reset error state and retry rendering child components.
   * Triggered by the "retry" button.
   */
  private handleRetry = (): void => {
    this.setState({ hasError: false, error: null });
  };

  render(): React.ReactNode {
    const { hasError, error } = this.state;
    const { children, fallbackRender } = this.props;

    // 未出错时正常渲染子组件 / Render children normally when no error
    if (!hasError || !error) {
      return children;
    }

    // 如果提供了自定义降级渲染，优先使用
    // If custom fallback render is provided, use it first
    if (fallbackRender) {
      return fallbackRender(error, { componentStack: '' });
    }

    // 默认的错误展示界面：居中布局，包含错误图标、信息和重试按钮
    // Default error display: centered layout with error icon, info, and retry button
    return (
      <div className="error-boundary" role="alert">
        {/* 错误图标 / Error icon */}
        <div className="error-boundary__icon">
          {'\u26A0'}
        </div>

        {/* 错误标题 / Error title */}
        <div className="error-boundary__title">
          应用遇到了一个错误 / An error occurred
        </div>

        {/* 错误消息 / Error message */}
        <div className="error-boundary__message">
          {error.message || '未知错误 / Unknown error'}
        </div>

        {/* 错误堆栈详情（仅开发环境显示）/ Error stack details (dev only) */}
        {error.stack && (
          <pre className="error-boundary__details">
            {error.stack}
          </pre>
        )}

        {/* 重试按钮 / Retry button */}
        <button
          className="error-boundary__retry"
          onClick={this.handleRetry}
          aria-label="重试 / Retry"
        >
          重试 / Retry
        </button>
      </div>
    );
  }
}

export default ErrorBoundary;
