/**
 * @module components/common/Modal
 * @description 模态框对话框组件 / Modal dialog component.
 *              提供居中覆盖层对话框，包含标题、内容区域和操作按钮（确认/取消）。
 *              Provides a centered overlay dialog with title, content area,
 *              and action buttons (confirm/cancel).
 */

import React, { useEffect, useCallback, useRef } from 'react';
import { useAppStore } from '@/stores';

/**
 * Modal 组件的 props 接口 / Modal component props interface
 * @property id - 模态框唯一标识符 / Unique modal identifier
 * @property title - 对话框标题（双语）/ Dialog title (bilingual)
 * @property children - 对话框主体内容 / Dialog body content
 * @property onConfirm - 确认操作回调 / Confirm action callback
 * @property onCancel - 取消操作回调 / Cancel action callback
 * @property confirmLabel - 确认按钮文本（默认 'OK'）/ Confirm button text (default: 'OK')
 * @property cancelLabel - 取消按钮文本（默认 'CANCEL / 取消'）/ Cancel button text (default: 'CANCEL / 取消')
 * @property danger - 确认操作是否为破坏性操作 / Whether the confirm action is destructive
 */
interface ModalProps {
  id: string;
  title: string;
  children: React.ReactNode;
  onConfirm?: () => void;
  onCancel?: () => void;
  confirmLabel?: string;
  cancelLabel?: string;
  danger?: boolean;
}

/**
 * Modal - 模态框对话框组件 / Modal dialog component
 *
 * 特性 / Features:
 * - 带背景模糊的居中覆盖层 / Centered overlay with background blur
 * - 键盘支持（Escape 关闭）/ Keyboard support (Escape to close)
 * - 危险操作变体（红色确认按钮）/ Destructive action variant (red confirm button)
 * - 通过 Zustand store 管理显示/隐藏状态 / Visibility managed via Zustand store
 * - 点击遮罩层关闭 / Click overlay to close
 * - 打开时自动聚焦到确认按钮 / Auto-focus confirm button on open
 */
const Modal: React.FC<ModalProps> = ({
  id,
  title,
  children,
  onConfirm,
  onCancel,
  confirmLabel = 'OK',
  cancelLabel = 'CANCEL / 取消',
  danger = false,
}) => {
  const modal = useAppStore((s) => s.modal);
  const hideModal = useAppStore((s) => s.hideModal);
  const confirmBtnRef = useRef<HTMLButtonElement>(null);

  const isVisible = modal?.id === id;

  const handleConfirm = useCallback(() => {
    onConfirm?.();
    hideModal();
  }, [onConfirm, hideModal]);

  const handleCancel = useCallback(() => {
    onCancel?.();
    hideModal();
  }, [onCancel, hideModal]);

  /**
   * 点击遮罩层关闭：仅当点击目标是遮罩本身时才关闭
   * Click overlay to close: only close when the click target is the overlay itself
   */
  const handleOverlayClick = useCallback(
    (e: React.MouseEvent<HTMLDivElement>) => {
      if (e.target === e.currentTarget) {
        handleCancel();
      }
    },
    [handleCancel],
  );

  // 按 Escape 键关闭 / Close on Escape key
  useEffect(() => {
    if (!isVisible) return;
    const handler = (e: KeyboardEvent) => {
      if (e.key === 'Escape') handleCancel();
    };
    document.addEventListener('keydown', handler);
    return () => document.removeEventListener('keydown', handler);
  }, [isVisible, handleCancel]);

  // 打开时自动聚焦到确认按钮 / Auto-focus confirm button on open
  useEffect(() => {
    if (isVisible) {
      // 使用 requestAnimationFrame 确保对话框已渲染
      requestAnimationFrame(() => {
        confirmBtnRef.current?.focus();
      });
    }
  }, [isVisible]);

  if (!isVisible) return null;

  return (
    <div
      className="modal-overlay active"
      id={`modal-${id}`}
      onClick={handleOverlayClick}
    >
      <div className="modal-dialog" role="dialog" aria-modal="true" aria-label={title}>
        <div className="modal-title">{title}</div>
        <div className="modal-body">{children}</div>
        <div className="modal-actions">
          <button
            ref={confirmBtnRef}
            className={`modal-btn ${danger ? 'modal-btn-danger' : 'modal-btn-primary'}`}
            onClick={handleConfirm}
          >
            {confirmLabel}
          </button>
          <button className="modal-btn" onClick={handleCancel}>
            {cancelLabel}
          </button>
        </div>
      </div>
    </div>
  );
};

export default Modal;
