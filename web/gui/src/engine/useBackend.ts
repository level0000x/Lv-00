/**
 * @module engine/useBackend
 * @description React context and hook for accessing the Lv-00 backend instance.
 *              Provides the IBackend interface to all components through React's
 *              context mechanism, enabling dependency injection and testability.
 *
 *              React 上下文和钩子，用于访问 Lv-00 后端实例。
 *              通过 React 上下文机制为所有组件提供 IBackend 接口，
 *              支持依赖注入和可测试性。
 */

import { useContext, createContext } from 'react';
import type { IBackend } from './backend';

// ================================================================
// Backend Context / 后端上下文
// ================================================================

/**
 * React context that holds the current backend instance.
 * Must be provided by a parent component (typically the App root)
 * via `<BackendContext.Provider value={backend}>`.
 *
 * 存储当前后端实例的 React 上下文。
 * 必须由父组件（通常是 App 根组件）通过
 * `<BackendContext.Provider value={backend}>` 提供。
 */
export const BackendContext = createContext<IBackend | null>(null);

// ================================================================
// useBackend Hook / useBackend 钩子
// ================================================================

/**
 * React hook to access the current backend instance from context.
 *
 * Throws an error if used outside of a BackendContext.Provider,
 * which indicates a setup issue in the component tree.
 *
 * 从上下文中获取当前后端实例的 React 钩子。
 *
 * 如果在 BackendContext.Provider 之外使用，将抛出错误，
 * 这表明组件树中存在配置问题。
 *
 * @example
 * ```tsx
 * function MyComponent() {
 *   const backend = useBackend();
 *   const handleAddPoint = () => {
 *     const id = backend.graphAddPoint(graphHandle, 1.0, 2.0);
 *     console.log('Added point:', id);
 *   };
 *   return <button onClick={handleAddPoint}>Add Point</button>;
 * }
 * ```
 *
 * @returns The current IBackend instance / 当前后端实例
 * @throws Error if backend context is not available / 如果后端上下文不可用则抛出错误
 */
export function useBackend(): IBackend {
  const backend = useContext(BackendContext);
  if (backend === null) {
    throw new Error(
      '[Lv-00] useBackend() must be used within a <BackendContext.Provider>. ' +
      'Ensure that the backend context is set up in the App root component.\n' +
      '[Lv-00] useBackend() 必须在 <BackendContext.Provider> 内使用。' +
      '请确保在 App 根组件中设置了后端上下文。'
    );
  }
  return backend;
}
