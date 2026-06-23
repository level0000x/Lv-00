import React from 'react';
import { ToastItem } from '../L5-core/types';
import { useUIStore } from '../L5-core/store/uiStore';

const icons: Record<string, string> = {
  success: '\u2713',
  error: '\u2717',
  warning: '\u26A0',
  info: '\u24D8',
};

const ToastContainer: React.FC = () => {
  const toasts = useUIStore((s) => s.toasts);
  const removeToast = useUIStore((s) => s.removeToast);

  if (!toasts.length) return null;

  return (
    <div className="toast-container">
      {toasts.map((t: ToastItem) => (
        <div
          key={t.id}
          className={`toast ${t.variant}`}
          onClick={() => removeToast(t.id)}
        >
          <span>{icons[t.variant] ?? ''}</span>
          <span>{t.message}</span>
        </div>
      ))}
    </div>
  );
};

export default ToastContainer;
