/**
 * @module components/common/Toast
 * @description Toast 通知容器组件 / Toast notification container component.
 *              从 Zustand store 中读取活跃的 Toast 通知并渲染。
 *              Reads active Toast notifications from the Zustand store and renders them.
 */

import React from 'react';
import { useAppStore } from '@/stores';

/**
 * Toast - Toast 通知容器 / Toast notification container
 *
 * 从 Zustand store 渲染所有活跃的 Toast 通知。
 * Toast 定位在屏幕右下角，并在配置的持续时间后自动消失。
 * 点击 Toast 可手动关闭。
 *
 * Renders all active Toast notifications from the Zustand store.
 * Toasts are positioned at the bottom-right of the screen and
 * automatically disappear after the configured duration.
 * Clicking a Toast dismisses it manually.
 */
const Toast = React.memo(function Toast() {
  const toasts = useAppStore((s) => s.toasts);
  const removeToast = useAppStore((s) => s.removeToast);

  if (toasts.length === 0) return null;

  return (
    <div className="toast-container" id="toastContainer">
      {toasts.map((toast) => (
        <div
          key={toast.id}
          className={`toast toast-${toast.variant}`}
          onClick={() => removeToast(toast.id)}
          role="alert"
        >
          {toast.message}
        </div>
      ))}
    </div>
  );
});

Toast.displayName = 'Toast';

export default Toast;
