/**
 * @module hooks/useDualBlockOperation
 * @description 通用双函数块操作 hook / Generic dual-block operation hook
 *
 * 将组合（Compose）和乘积（Product）等需要依次选择两个函数块的操作
 * 提取为通用逻辑。两者结构完全相同，仅调用的工具函数不同。
 *
 * Extracts the common pattern of selecting two function blocks sequentially
 * (used by Compose, Product, etc.) into a reusable hook.
 * The only difference between operations is the tool function called.
 */

import { useState, useCallback } from 'react';
import type { UserFuncBlock } from '@/utils/funcBlockPresets';

/**
 * 双函数块操作执行函数的类型定义
 * 接受两个函数块，返回操作结果描述和可选的新函数块。
 *
 * Type definition for the dual-block operation executor function.
 * Accepts two function blocks and returns the operation result description
 * and an optional new function block.
 *
 * @param block1 - 第一个函数块 / First function block
 * @param block2 - 第二个函数块 / Second function block
 * @returns 操作结果，包含描述文本和可选的生成函数块
 */
export interface DualBlockResult {
  /** 操作结果描述 / Operation result description */
  description: string;
  /** 操作生成的函数块（可选，null 表示失败）/ Generated function block (optional, null means failure) */
  result?: UserFuncBlock | null;
}

export type DualBlockExecutor = (
  block1: UserFuncBlock,
  block2: UserFuncBlock,
) => DualBlockResult;

/**
 * useDualBlockOperation hook 的参数接口
 * Parameters interface for the useDualBlockOperation hook
 *
 * @property operationName - 操作的中文名称（用于提示消息）/ Chinese name of the operation (for toast messages)
 * @property operationNameEn - 操作的英文名称（用于日志）/ English name of the operation (for logs)
 * @property executor - 执行操作的函数 / Function that executes the operation
 * @param userBlocks - 当前用户函数块列表 / Current user function block list
 * @param addToast - 添加 Toast 消息的函数 / Function to add toast messages
 * @param appendLog - 追加全局日志的函数 / Function to append global log
 * @param log - 追加面板日志的函数 / Function to append panel log
 * @param setUserBlocks - 更新用户函数块列表的函数 / Function to update user block list
 */
interface UseDualBlockOperationParams {
  /** 操作的中文名称（用于提示消息）/ Chinese name for toast messages */
  operationName: string;
  /** 操作的英文名称（用于日志）/ English name for logs */
  operationNameEn: string;
  /** 执行操作的函数 / Function that executes the operation */
  executor: DualBlockExecutor;
  /** 当前用户函数块列表 / Current user function block list */
  userBlocks: UserFuncBlock[];
  /** 添加 Toast 消息的函数 / Function to add toast messages */
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  addToast: (type: any, message: string) => void;
  /** 追加全局日志的函数 / Function to append global log */
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  appendLog: (message: string, level: any) => void;
  /** 追加面板日志的函数 / Function to append panel log */
  log: (message: string) => void;
  /** 更新用户函数块列表的函数 / Function to update user block list */
  setUserBlocks: React.Dispatch<React.SetStateAction<UserFuncBlock[]>>;
}

/**
 * useDualBlockOperation hook 的返回值接口
 * Return value interface for the useDualBlockOperation hook
 */
interface UseDualBlockOperationReturn {
  /** 当前选中的第一个函数块 / Currently selected first block */
  select1: UserFuncBlock | null;
  /** 当前选中的第二个函数块 / Currently selected second block */
  select2: UserFuncBlock | null;
  /** 开始选择操作（点击操作按钮时调用）/ Start the selection operation */
  handleStart: () => void;
  /** 选择一个函数块（点击用户块列表中的块时调用）/ Select a function block */
  handleSelect: (block: UserFuncBlock) => void;
  /** 取消当前选择操作 / Cancel the current selection operation */
  handleCancel: () => void;
}

/**
 * useDualBlockOperation - 通用双函数块操作 hook
 *
 * 管理需要依次选择两个函数块的操作流程：
 * 1. handleStart: 开始选择，提示用户选择第一个块
 * 2. handleSelect: 选择块，第一个块选中后等待第二个块，第二个块选中后自动执行
 * 3. handleCancel: 取消选择，重置状态
 *
 * Manages the workflow of selecting two function blocks sequentially:
 * 1. handleStart: Begin selection, prompt user to select the first block
 * 2. handleSelect: Select a block; after the first block is selected, wait for
 *    the second block; after the second block is selected, auto-execute
 * 3. handleCancel: Cancel selection, reset state
 *
 * @param params - hook 参数 / Hook parameters
 * @returns 选择状态和操作处理函数 / Selection state and operation handlers
 */
export function useDualBlockOperation(
  params: UseDualBlockOperationParams,
): UseDualBlockOperationReturn {
  const {
    operationName,
    operationNameEn,
    executor,
    userBlocks,
    addToast,
    appendLog,
    log,
    setUserBlocks,
  } = params;

  /** 当前选中的第一个函数块 / Currently selected first block */
  const [select1, setSelect1] = useState<UserFuncBlock | null>(null);
  /** 当前选中的第二个函数块 / Currently selected second block */
  const [select2, setSelect2] = useState<UserFuncBlock | null>(null);

  /**
   * 开始选择操作
   * 检查是否有足够的函数块，然后提示用户选择第一个块。
   *
   * Start the selection operation.
   * Checks if there are enough blocks, then prompts the user to select the first one.
   */
  const handleStart = useCallback(() => {
    if (userBlocks.length < 2) {
      addToast(
        'info',
        `${operationName}需要至少 2 个已打包的函数块 / ${operationNameEn} requires at least 2 packed blocks`,
      );
      return;
    }

    // 如果已选择第一个块，提示继续选择第二个
    if (select1) {
      if (!select2) {
        addToast('info', `已选择 "${select1.name}"，请继续选择第二个函数块...`);
        return;
      }
    } else {
      // 开始选择第一个块
      addToast(
        'info',
        `${operationName}功能: 当前有 ${userBlocks.length} 个函数块。请选择一个...`,
      );
      appendLog(`${operationNameEn}操作: 选择第一个函数块`, 'info');
    }
  }, [userBlocks, select1, select2, operationName, operationNameEn, addToast, appendLog]);

  /**
   * 选择一个函数块
   * 如果是第一个块则记录，如果是第二个块则验证后自动执行操作。
   *
   * Select a function block.
   * If it's the first block, record it. If it's the second block,
   * validate and auto-execute the operation.
   */
  const handleSelect = useCallback(
    (block: UserFuncBlock) => {
      if (!select1) {
        // 选择第一个块 / Select the first block
        setSelect1(block);
        addToast('info', `已选择 "${block.name}"，请选择第二个函数块...`);
      } else if (!select2) {
        // 选择第二个块 / Select the second block
        if (block.id === select1.id) {
          addToast('warning', '不能选择同一个函数块');
          return;
        }
        setSelect2(block);

        // 执行操作 / Execute the operation
        const result = executor(select1, block);
        if (result.result) {
          setUserBlocks((prev) => [...prev, result.result!]);
          addToast('success', result.description);
          appendLog(result.description, 'info');
          log(result.description);
        }

        // 重置选择状态 / Reset selection state
        setSelect1(null);
        setSelect2(null);
      }
    },
    [select1, select2, executor, addToast, appendLog, log, setUserBlocks],
  );

  /**
   * 取消当前选择操作，重置所有选择状态。
   * Cancel the current selection operation, reset all selection state.
   */
  const handleCancel = useCallback(() => {
    setSelect1(null);
    setSelect2(null);
    addToast('info', `已取消${operationName}操作`);
  }, [operationName, addToast]);

  return { select1, select2, handleStart, handleSelect, handleCancel };
}

export default useDualBlockOperation;
