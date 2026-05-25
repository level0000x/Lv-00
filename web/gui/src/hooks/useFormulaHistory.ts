/**
 * @module hooks/useFormulaHistory
 * @description 公式历史记录管理 Hook。
 *              提供公式输入的撤销/重做功能，维护一个最多保留 10 条记录的历史栈。
 *
 * 功能特性：
 * - 公式历史栈管理（最多 10 条）
 * - 撤销 / 重做操作
 * - 历史入栈并设置新值
 */

import { useState, useCallback } from 'react';

/** 历史记录最大条数 */
const MAX_HISTORY_SIZE = 10;

/**
 * useFormulaHistory - 公式历史记录管理 Hook
 *
 * @param setFormulaInput - 设置公式输入值的回调（来自 store）
 * @param formulaInput - 当前公式输入值（用于撤销时获取当前值）
 * @param addToast - 添加 Toast 提示的回调
 * @returns 历史记录相关的状态和操作方法
 */
export function useFormulaHistory(
  setFormulaInput: (value: string) => void,
  formulaInput: string,
  addToast: (type: 'success' | 'error' | 'warning' | 'info', message: string) => void,
) {
  /** 历史记录栈 */
  const [formulaHistory, setFormulaHistory] = useState<string[]>([]);
  /** 当前回溯索引，-1 表示在最新位置 */
  const [historyIndex, setHistoryIndex] = useState<number>(-1);

  /**
   * 将当前公式入栈并设置新值。
   * 如果新值与栈顶相同则不入栈。
   *
   * @param newValue - 要设置的新公式值
   * @returns 设置后的新值
   */
  const pushToHistoryAndSet = useCallback(
    (newValue: string): string => {
      setFormulaHistory((prev) => {
        const trimmed = prev.slice(0, MAX_HISTORY_SIZE);
        if (trimmed.length > 0 && trimmed[trimmed.length - 1] === newValue) return trimmed;
        return [...trimmed, newValue].slice(-MAX_HISTORY_SIZE);
      });
      setHistoryIndex(-1);
      setFormulaInput(newValue);
      return newValue;
    },
    [setFormulaInput],
  );

  /**
   * 撤销公式 —— 回退到上一个历史版本
   */
  const handleUndoFormula = useCallback(() => {
    setFormulaHistory((prev) => {
      if (prev.length === 0) {
        addToast('warning', '没有可撤销的公式版本 / No formula versions to undo');
        return prev;
      }
      // 如果当前不在历史中回溯，先把当前值入栈
      let stack = prev;
      const currentFormula = formulaInput;
      if (historyIndex === -1 && currentFormula !== '') {
        stack = [...prev, currentFormula].slice(-MAX_HISTORY_SIZE);
      }
      const newIdx = historyIndex === -1 ? stack.length - 2 : historyIndex - 1;
      if (newIdx < 0) {
        addToast('warning', '已到达最早版本 / Reached oldest version');
        return stack;
      }
      setHistoryIndex(newIdx);
      setFormulaInput(stack[newIdx]!);
      addToast('info', `已撤销至 v${newIdx + 1}/${stack.length} / Undone to v${newIdx + 1}/${stack.length}`);
      return stack;
    });
  }, [formulaInput, historyIndex, setFormulaInput, addToast]);

  /**
   * 重做公式 —— 前进到下一个历史版本
   */
  const handleRedoFormula = useCallback(() => {
    setFormulaHistory((prev) => {
      if (prev.length === 0 || historyIndex >= prev.length - 1) {
        addToast('warning', '没有可重做的公式版本 / No formula versions to redo');
        return prev;
      }
      const newIdx = historyIndex + 1;
      setHistoryIndex(newIdx);
      setFormulaInput(prev[newIdx]!);
      addToast('info', `已重做至 v${newIdx + 1}/${prev.length} / Redone to v${newIdx + 1}/${prev.length}`);
      return prev;
    });
  }, [historyIndex, setFormulaInput, addToast]);

  return {
    formulaHistory,
    historyIndex,
    pushToHistoryAndSet,
    handleUndoFormula,
    handleRedoFormula,
  };
}

export default useFormulaHistory;
