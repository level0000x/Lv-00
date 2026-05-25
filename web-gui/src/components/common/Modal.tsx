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
 * @property id - 模态框唯一标识符，用于与 Zustand store 中的 modal 状态匹配 / Unique modal identifier, matched against Zustand store's modal state
 * @property title - 对话框标题（双语）/ Dialog title (bilingual)
 * @property children - 对话框主体内容 / Dialog body content
 * @property onConfirm - 确认操作回调 / Confirm action callback
 * @property onCancel - 取消操作回调 / Cancel action callback
 * @property confirmLabel - 确认按钮文本（默认 'OK'）/ Confirm button text (default: 'OK')
 * @property cancelLabel - 取消按钮文本（默认 'CANCEL / 取消'）/ Cancel button text (default: 'CANCEL / 取消')
 * @property danger - 确认操作是否为破坏性操作（红色确认按钮）/ Whether the confirm action is destructive (red confirm button)
 */
interface ModalProps {
  /** 模态框唯一标识符 / Unique modal identifier */
  id: string;
  /** 对话框标题 / Dialog title */
  title: string;
  /** 对话框主体内容 / Dialog body content */
  children: React.ReactNode;
  /** 确认操作回调（在关闭模态框之前调用）/ Confirm action callback (called before closing modal) */
  onConfirm?: () => void;
  /** 取消操作回调（在关闭模态框之前调用）/ Cancel action callback (called before closing modal) */
  onCancel?: () => void;
  /** 确认按钮文本 / Confirm button label */
  confirmLabel?: string;
  /** 取消按钮文本 / Cancel button label */
  cancelLabel?: string;
  /** 确认操作是否为破坏性操作 / Whether the confirm action is destructive */
  danger?: boolean;
}

/**
 * Modal - 模态框对话框组件 / Modal dialog component
 *
 * 功能特性 / Features:
 * - 带背景模糊的居中覆盖层 / Centered overlay with background blur
 * - 键盘支持（Escape 关闭）/ Keyboard support (Escape to close)
 * - Tab 键焦点陷阱（防止焦点离开对话框）/ Tab focus trap (prevents focus from leaving the dialog)
 * - 危险操作变体（红色确认按钮）/ Destructive action variant (red confirm button)
 * - 通过 Zustand store 管理显示/隐藏状态 / Visibility managed via Zustand store
 * - 点击遮罩层关闭 / Click overlay to close
 * - 打开时自动聚焦到确认按钮 / Auto-focus confirm button on open
 * - 完整的 ARIA 属性支持 / Full ARIA attribute support
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
  const cancelBtnRef = useRef<HTMLButtonElement>(null);

  /** 当前模态框是否可见 / Whether this modal is currently visible */
  const isVisible = modal?.id === id;

  /**
   * 确认操作处理
   * 先调用外部 onConfirm 回调，然后关闭模态框。
   *
   * Confirm action handler.
   * Calls external onConfirm callback first, then closes the modal.
   */
  const handleConfirm = useCallback(() => {
    onConfirm?.();
    hideModal();
  }, [onConfirm, hideModal]);

  /**
   * 取消操作处理
   * 先调用外部 onCancel 回调，然后关闭模态框。
   *
   * Cancel action handler.
   * Calls external onCancel callback first, then closes the modal.
   */
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
      if (e.key === 'Escape') {
        e.preventDefault();
        handleCancel();
      }
    };
    document.addEventListener('keydown', handler);
    return () => document.removeEventListener('keydown', handler);
  }, [isVisible, handleCancel]);

  // 焦点陷阱：Tab 键在对话框内循环 / Focus trap: Tab key cycles within dialog
  useEffect(() => {
    if (!isVisible) return;
    const dialog = document.querySelector(`#modal-${id} .modal-dialog`) as HTMLElement | null;
    if (!dialog) return;

    const handleTabTrap = (e: KeyboardEvent) => {
      if (e.key !== 'Tab') return;

      const focusableElements = dialog.querySelectorAll<HTMLElement>(
        'button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])',
      );
      if (focusableElements.length === 0) return;

      const firstEl = focusableElements[0]!;
      const lastEl = focusableElements[focusableElements.length - 1]!;

      if (e.shiftKey && document.activeElement === firstEl) {
        e.preventDefault();
        lastEl.focus();
      } else if (!e.shiftKey && document.activeElement === lastEl) {
        e.preventDefault();
        firstEl.focus();
      }
    };

    document.addEventListener('keydown', handleTabTrap);
    return () => document.removeEventListener('keydown', handleTabTrap);
  }, [isVisible, id]);

  // 打开时自动聚焦到确认按钮 / Auto-focus confirm button on open
  useEffect(() => {
    if (isVisible) {
      // 使用 requestAnimationFrame 确保对话框已渲染到 DOM
      // Use requestAnimationFrame to ensure the dialog has been rendered to the DOM
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
      role="presentation"
    >
      <div
        className="modal-dialog"
        role="dialog"
        aria-modal="true"
        aria-label={title}
        aria-describedby={`modal-body-${id}`}
      >
        {/* 对话框标题 / Dialog title */}
        <div className="modal-title">{title}</div>

        {/* 对话框主体内容 / Dialog body content */}
        <div className="modal-body" id={`modal-body-${id}`}>
          {children}
        </div>

        {/* 操作按钮区域 / Action buttons area */}
        <div className="modal-actions">
          <button
            ref={confirmBtnRef}
            className={`modal-btn ${danger ? 'modal-btn-danger' : 'modal-btn-primary'}`}
            onClick={handleConfirm}
            aria-label={confirmLabel}
          >
            {confirmLabel}
          </button>
          <button
            ref={cancelBtnRef}
            className="modal-btn"
            onClick={handleCancel}
            aria-label={cancelLabel}
          >
            {cancelLabel}
          </button>
        </div>
      </div>
    </div>
  );
};

export default Modal;
