/**
 * @module components/common/Toast
 * @description Toast 通知容器组件 / Toast notification container component.
 *              从 Zustand store 中读取活跃的 Toast 通知并渲染。
 *              Reads active Toast notifications from the Zustand store and renders them.
 *
 *              Toast 通知用于向用户反馈操作结果（成功、警告、错误、信息），
 *              定位在屏幕右下角，在配置的持续时间后自动消失。
 *              点击 Toast 可手动关闭。
 *
 *              Toast notifications provide feedback on operation results
 *              (success, warning, error, info). Positioned at the bottom-right
 *              of the screen, they automatically disappear after the configured duration.
 *              Clicking a Toast dismisses it manually.
 */

import React from 'react';
import { useAppStore } from '@/stores';

/**
 * Toast - Toast 通知容器 / Toast notification container
 *
 * 功能特性 / Features:
 * - 从 Zustand store 渲染所有活跃的 Toast 通知
 *   Renders all active Toast notifications from the Zustand store
 * - 支持四种变体：success / warning / error / info
 *   Supports four variants: success / warning / error / info
 * - 点击可手动关闭 / Click to dismiss manually
 * - 使用 role="alert" 提供无障碍支持
 *   Uses role="alert" for accessibility support
 * - 使用 React.memo 优化，避免不必要的重渲染
 *   Optimized with React.memo to avoid unnecessary re-renders
 */
const Toast = React.memo(function Toast() {
  const toasts = useAppStore((s) => s.toasts);
  const removeToast = useAppStore((s) => s.removeToast);

  // 无活跃通知时不渲染 / Don't render when there are no active notifications
  if (toasts.length === 0) return null;

  return (
    <div className="toast-container" id="toastContainer" aria-live="polite" aria-atomic="true">
      {toasts.map((toast) => (
        <div
          key={toast.id}
          className={`toast toast-${toast.variant}`}
          onClick={() => removeToast(toast.id)}
          role="alert"
          tabIndex={0}
          onKeyDown={(e) => {
            // 支持键盘操作关闭 / Support keyboard dismiss
            if (e.key === 'Enter' || e.key === ' ') {
              e.preventDefault();
              removeToast(toast.id);
            }
          }}
        >
          {toast.message}
        </div>
      ))}
    </div>
  );
});

Toast.displayName = 'Toast';

export default Toast;
