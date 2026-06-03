/**
 * @module components/panels/formula/FormulaHistory
 * @description 历史记录管理子组件。
 *              提供公式输入的撤销/重做按钮、清空和导出功能。
 *
 * 功能特性：
 * - 撤销 / 重做公式版本
 * - 清空输入框和输出
 * - 导出输出内容到剪贴板
 */

import React from 'react';
import Panel from '../Panel';

/** 安全复制到剪贴板（兼容非 HTTPS 环境） / Safe clipboard copy with fallback */
function copyToClipboard(text: string, addToast: (v: string, m: string) => void): void {
  if (navigator.clipboard && window.isSecureContext) {
    navigator.clipboard.writeText(text).then(
      () => addToast('success', '已复制到剪贴板 / Copied to clipboard'),
      () => fallbackCopy(text, addToast),
    );
  } else {
    fallbackCopy(text, addToast);
  }
}

/** textarea 回退复制方案 */
function fallbackCopy(text: string, addToast: (v: string, m: string) => void): void {
  try {
    const ta = document.createElement('textarea');
    ta.value = text;
    ta.style.position = 'fixed';
    ta.style.opacity = '0';
    document.body.appendChild(ta);
    ta.select();
    document.execCommand('copy');
    document.body.removeChild(ta);
    addToast('success', '已复制到剪贴板 / Copied to clipboard');
  } catch {
    addToast('error', '复制失败 / Copy failed');
  }
}

/**
 * FormulaHistory 组件属性
 */
interface FormulaHistoryProps {
  /** 历史记录栈 */
  formulaHistory: string[];
  /** 当前回溯索引 */
  historyIndex: number;
  /** 撤销回调 */
  onUndo: () => void;
  /** 重做回调 */
  onRedo: () => void;
  /** 清空回调 */
  onClear: () => void;
  /** 输出文本（用于导出） */
  outputText: string;
  /** 添加 Toast 提示的回调 */
  addToast: (type: 'success' | 'error' | 'warning' | 'info', message: string) => void;
}

/**
 * FormulaHistory - 历史记录管理子组件
 *
 * 包含 ACTIONS 面板：撤销/重做、清空、导出按钮。
 */
const FormulaHistory: React.FC<FormulaHistoryProps> = ({
  formulaHistory,
  historyIndex,
  onUndo,
  onRedo,
  onClear,
  outputText,
  addToast,
}) => {
  return (
    <Panel title="ACTIONS / 操作" panelId="formula-actions">
      {/* 公式历史：撤销/重做 */}
      <div className="formula-btn-row">
        <button
          className="btn"
          onClick={onUndo}
          disabled={formulaHistory.length === 0 || (historyIndex === -1 && formulaHistory.length === 0)}
          style={{ fontSize: '11px' }}
        >
          UNDO FORMULA / 撤销公式
        </button>
        <button
          className="btn"
          onClick={onRedo}
          disabled={historyIndex >= formulaHistory.length - 1 || historyIndex === -1}
          style={{ fontSize: '11px' }}
        >
          REDO FORMULA / 重做公式
        </button>
      </div>
      {formulaHistory.length > 0 && (
        <div className="info-box" style={{ marginTop: 4, fontSize: '10px', textAlign: 'center' }}>
          v{historyIndex === -1 ? formulaHistory.length : historyIndex + 1}/{formulaHistory.length}
        </div>
      )}
      <div className="panel-separator" />
      <button className="btn" onClick={onClear}>
        CLEAR / 清空
      </button>
      <button
        className="btn"
        onClick={() => {
          if (!outputText) {
            addToast('warning', '没有可导出的内容 / Nothing to export');
            return;
          }
          // 复制到剪贴板
          copyToClipboard(outputText, (v, m) => addToast(v as 'success' | 'error' | 'warning' | 'info', m));
        }}
      >
        EXPORT / 导出
      </button>
    </Panel>
  );
};

export default FormulaHistory;
