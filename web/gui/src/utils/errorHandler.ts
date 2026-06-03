/**
 * @module utils/errorHandler
 * @description 统一的错误处理与日志工具库
 *
 *              提供标准化的错误处理流程、日志记录和用户通知机制。
 *
 *              功能特性：
 *              - 错误分类：系统错误、验证错误、业务逻辑错误、用户操作错误
 *              - 错误处理：防御性编程、统一错误格式、错误恢复策略
 *              - 日志级别：debug、info、warn、error、fatal
 *              - 用户通知：Toast、Console、UI 反馈
 *
 *              【优化说明】v3.6.0
 *              - 完整的错误分类体系，便于问题追踪和统计
 *              - 标准化的错误格式，包含堆栈、上下文和解决方案
 *              - 灵活的日志输出配置，支持不同环境的日志级别控制
 *
 * @since 3.6.0
 */

// ================================================================
// 错误类型定义
// ================================================================

/**
 * 错误分类枚举
 * 用于区分不同类型的错误，便于日志统计和问题追踪
 */
export enum ErrorCategory {
  /** 系统级错误：内存溢出、网络断开、权限不足 */
  SYSTEM = 'SYSTEM',
  /** 验证错误：输入格式不正确、类型不匹配 */
  VALIDATION = 'VALIDATION',
  /** 业务逻辑错误：状态不一致、约束违反 */
  BUSINESS = 'BUSINESS',
  /** 用户操作错误：操作失败、无效动作 */
  USER_ACTION = 'USER_ACTION',
  /** 渲染错误：Canvas 绘制失败、图形处理异常 */
  RENDERING = 'RENDERING',
  /** 计算错误：数值溢出、除零、几何计算异常 */
  COMPUTATION = 'COMPUTATION',
  /** 未知错误 */
  UNKNOWN = 'UNKNOWN',
}

/**
 * 错误严重程度枚举
 */
export enum ErrorSeverity {
  /** 调试信息，不影响功能 */
  DEBUG = 0,
  /** 一般信息 */
  INFO = 1,
  /** 警告，可能存在问题 */
  WARNING = 2,
  /** 错误，功能受损 */
  ERROR = 3,
  /** 致命错误，应用崩溃 */
  FATAL = 4,
}

/**
 * 错误信息结构
 */
export interface AppError {
  /** 错误标识（用于追踪） */
  id: string;
  /** 错误分类 */
  category: ErrorCategory;
  /** 错误严重程度 */
  severity: ErrorSeverity;
  /** 错误消息 */
  message: string;
  /** 原始错误对象 */
  originalError?: Error;
  /** 错误上下文信息 */
  context?: Record<string, unknown>;
  /** 建议的解决方案 */
  solution?: string;
  /** 错误发生时间 */
  timestamp: number;
  /** 调用堆栈 */
  stack?: string;
}

/**
 * 错误处理器配置
 */
export interface ErrorHandlerConfig {
  /** 是否在控制台输出 */
  consoleOutput?: boolean;
  /** 是否发送到服务器 */
  serverReport?: boolean;
  /** 日志保留数量 */
  maxLogEntries?: number;
  /** 是否显示 Toast 通知 */
  showToast?: boolean;
  /** Toast 自动消失时间（毫秒） */
  toastDuration?: number;
}

// ================================================================
// 错误类
// ================================================================

/**
 * 应用错误基类
 * 扩展 Error 对象，包含分类、严重程度和上下文信息
 */
export class ApplicationError extends Error {
  /** 错误分类 */
  public readonly category: ErrorCategory;
  /** 错误严重程度 */
  public readonly severity: ErrorSeverity;
  /** 错误上下文 */
  public readonly context?: Record<string, unknown>;
  /** 建议的解决方案 */
  public readonly solution?: string;
  /** 错误 ID */
  public readonly id: string;

  constructor(
    message: string,
    category: ErrorCategory = ErrorCategory.UNKNOWN,
    severity: ErrorSeverity = ErrorSeverity.ERROR,
    options?: {
      context?: Record<string, unknown>;
      solution?: string;
      originalError?: Error;
    },
  ) {
    super(message);
    this.name = 'ApplicationError';
    this.category = category;
    this.severity = severity;
    this.context = options?.context;
    this.solution = options?.solution;
    this.id = this.generateId();

    // 保留原始错误堆栈
    if (options?.originalError?.stack) {
      this.stack = options.originalError.stack;
    }
  }

  /**
   * 生成唯一错误 ID
   */
  private generateId(): string {
    return `ERR_${Date.now().toString(36)}_${Math.random().toString(36).substring(2, 8)}`;
  }

  /**
   * 转换为标准错误信息结构
   */
  toAppError(): AppError {
    return {
      id: this.id,
      category: this.category,
      severity: this.severity,
      message: this.message,
      originalError: this,
      context: this.context,
      solution: this.solution,
      timestamp: Date.now(),
      stack: this.stack,
    };
  }

  /**
   * 获取用户友好的错误消息
   */
  getUserMessage(): string {
    let message = this.message;
    if (this.solution) {
      message += `\n解决方案：${this.solution}`;
    }
    return message;
  }
}

// ================================================================
// 专用错误类
// ================================================================

/**
 * 验证错误：输入格式不正确、类型不匹配等
 */
export class ValidationError extends ApplicationError {
  constructor(message: string, options?: { field?: string; value?: unknown; solution?: string }) {
    super(
      options?.field ? `${options.field}: ${message}` : message,
      ErrorCategory.VALIDATION,
      ErrorSeverity.WARNING,
      {
        context: { field: options?.field, value: options?.value },
        solution: options?.solution || '请检查输入格式是否正确',
      },
    );
    this.name = 'ValidationError';
  }
}

/**
 * 业务逻辑错误：状态不一致、约束违反等
 */
export class BusinessError extends ApplicationError {
  constructor(message: string, options?: { state?: string; constraint?: string; solution?: string }) {
    super(
      message,
      ErrorCategory.BUSINESS,
      ErrorSeverity.ERROR,
      {
        context: { state: options?.state, constraint: options?.constraint },
        solution: options?.solution || '请检查当前操作是否符合业务规则',
      },
    );
    this.name = 'BusinessError';
  }
}

/**
 * 计算错误：数值溢出、除零、几何计算异常等
 */
export class ComputationError extends ApplicationError {
  constructor(message: string, options?: { operation?: string; operands?: unknown[]; solution?: string }) {
    super(
      message,
      ErrorCategory.COMPUTATION,
      ErrorSeverity.ERROR,
      {
        context: { operation: options?.operation, operands: options?.operands },
        solution: options?.solution || '请检查计算参数是否合理',
      },
    );
    this.name = 'ComputationError';
  }
}

/**
 * 渲染错误：Canvas 绘制失败、图形处理异常等
 */
export class RenderingError extends ApplicationError {
  constructor(message: string, options?: { element?: string; operation?: string; solution?: string }) {
    super(
      message,
      ErrorCategory.RENDERING,
      ErrorSeverity.ERROR,
      {
        context: { element: options?.element, operation: options?.operation },
        solution: options?.solution || '请尝试刷新页面或调整窗口大小',
      },
    );
    this.name = 'RenderingError';
  }
}

// ================================================================
// 错误处理器
// ================================================================

/**
 * 全局错误处理器
 * 统一管理错误记录、日志输出和用户通知
 */
class GlobalErrorHandler {
  /** 单例实例 */
  private static instance: GlobalErrorHandler;
  /** 错误日志 */
  private errorLog: AppError[] = [];
  /** 配置 */
  private config: Required<ErrorHandlerConfig>;

  private constructor() {
    this.config = {
      consoleOutput: true,
      serverReport: false,
      maxLogEntries: 100,
      showToast: true,
      toastDuration: 5000,
    };
  }

  /**
   * 获取单例实例
   */
  static getInstance(): GlobalErrorHandler {
    if (!GlobalErrorHandler.instance) {
      GlobalErrorHandler.instance = new GlobalErrorHandler();
    }
    return GlobalErrorHandler.instance;
  }

  /**
   * 更新配置
   */
  configure(config: Partial<ErrorHandlerConfig>): void {
    this.config = { ...this.config, ...config };
  }

  /**
   * 处理错误
   */
  handle(error: Error | ApplicationError | string, context?: Record<string, unknown>): AppError {
    let appError: AppError;

    if (error instanceof ApplicationError) {
      appError = error.toAppError();
      if (context) {
        appError.context = { ...appError.context, ...context };
      }
    } else if (error instanceof Error) {
      appError = {
        id: `ERR_${Date.now().toString(36)}_${Math.random().toString(36).substring(2, 8)}`,
        category: ErrorCategory.UNKNOWN,
        severity: ErrorSeverity.ERROR,
        message: error.message,
        originalError: error,
        context,
        timestamp: Date.now(),
        stack: error.stack,
      };
    } else {
      appError = {
        id: `ERR_${Date.now().toString(36)}_${Math.random().toString(36).substring(2, 8)}`,
        category: ErrorCategory.UNKNOWN,
        severity: ErrorSeverity.ERROR,
        message: error,
        context,
        timestamp: Date.now(),
      };
    }

    // 记录到日志
    this.errorLog.push(appError);

    // 保持日志数量限制
    if (this.errorLog.length > this.config.maxLogEntries) {
      this.errorLog = this.errorLog.slice(-this.config.maxLogEntries);
    }

    // 控制台输出
    if (this.config.consoleOutput) {
      this.logToConsole(appError);
    }

    // 发送服务器
    if (this.config.serverReport) {
      this.reportToServer(appError);
    }

    return appError;
  }

  /**
   * 输出到控制台
   */
  private logToConsole(error: AppError): void {
    const prefix = `[Lv-00 ${error.category}]`;
    const message = `${prefix} ${error.message}`;

    switch (error.severity) {
      case ErrorSeverity.DEBUG:
        console.debug(message, error);
        break;
      case ErrorSeverity.INFO:
        console.info(message, error);
        break;
      case ErrorSeverity.WARNING:
        console.warn(message, error);
        break;
      case ErrorSeverity.ERROR:
      case ErrorSeverity.FATAL:
        console.error(message, error);
        break;
    }
  }

  /**
   * 发送到服务器（预留接口）
   */
  private reportToServer(error: AppError): void {
    // TODO: 实现服务器上报
    console.log('[Lv-00] Error report to server:', error);
  }

  /**
   * 获取错误日志
   */
  getErrorLog(): AppError[] {
    return [...this.errorLog];
  }

  /**
   * 清空错误日志
   */
  clearLog(): void {
    this.errorLog = [];
  }

  /**
   * 获取最近的 N 条错误
   */
  getRecentErrors(count: number = 10): AppError[] {
    return this.errorLog.slice(-count);
  }
}

// 导出单例访问函数
export const errorHandler = GlobalErrorHandler.getInstance();

// ================================================================
// 便捷错误创建函数
// ================================================================

/**
 * 创建验证错误
 */
export function createValidationError(
  message: string,
  options?: { field?: string; value?: unknown; solution?: string },
): ValidationError {
  return new ValidationError(message, options);
}

/**
 * 创建业务错误
 */
export function createBusinessError(
  message: string,
  options?: { state?: string; constraint?: string; solution?: string },
): BusinessError {
  return new BusinessError(message, options);
}

/**
 * 创建计算错误
 */
export function createComputationError(
  message: string,
  options?: { operation?: string; operands?: unknown[]; solution?: string },
): ComputationError {
  return new ComputationError(message, options);
}

/**
 * 创建渲染错误
 */
export function createRenderingError(
  message: string,
  options?: { element?: string; operation?: string; solution?: string },
): RenderingError {
  return new RenderingError(message, options);
}

// ================================================================
// 错误处理装饰器
// ================================================================

/**
 * 错误处理装饰器工厂
 * 为异步函数添加统一的错误处理逻辑
 *
 * @param handler - 自定义错误处理器
 * @returns 装饰器函数
 *
 * @example
 * class MyService {
 *   @handleError((err) => logger.error(err))
 *   async fetchData() {
 *     // 可能会抛出错误
 *   }
 * }
 */
export function handleError(
  handler: (error: Error) => void,
) {
  return function <T>(
    _target: unknown,
    _propertyKey: string,
    descriptor: TypedPropertyDescriptor<(...args: unknown[]) => Promise<T>>,
  ): TypedPropertyDescriptor<(...args: unknown[]) => Promise<T>> {
    const originalMethod = descriptor.value;

    if (!originalMethod) {
      return descriptor;
    }

    descriptor.value = async function (...args: unknown[]): Promise<T> {
      try {
        return await originalMethod.apply(this, args);
      } catch (error) {
        handler(error as Error);
        throw error;
      }
    };

    return descriptor;
  };
}

// ================================================================
// try-catch 包装函数
// ================================================================

/**
 * 安全的异步函数执行包装器
 * 自动捕获错误并调用错误处理器
 *
 * @param asyncFn - 异步函数
 * @param fallback - 失败时的默认值
 * @param onError - 错误回调
 * @returns 执行结果或默认值
 *
 * @example
 * const data = await tryCatchAsync(
 *   () => fetch('/api/data').then(r => r.json()),
 *   null,
 *   (err) => logger.error(err),
 * );
 */
export async function tryCatchAsync<T>(
  asyncFn: () => Promise<T>,
  fallback: T,
  onError?: (error: Error) => void,
): Promise<T> {
  try {
    return await asyncFn();
  } catch (error) {
    const appError = errorHandler.handle(error as Error);
    onError?.(appError.originalError || (error as Error));
    return fallback;
  }
}

/**
 * 安全的同步函数执行包装器
 * 自动捕获错误并调用错误处理器
 *
 * @param fn - 同步函数
 * @param fallback - 失败时的默认值
 * @param onError - 错误回调
 * @returns 执行结果或默认值
 *
 * @example
 * const result = tryCatch(
 *   () => JSON.parse(userInput),
 *   null,
 *   (err) => logger.error(err),
 * );
 */
export function tryCatch<T>(
  fn: () => T,
  fallback: T,
  onError?: (error: Error) => void,
): T {
  try {
    return fn();
  } catch (error) {
    const appError = errorHandler.handle(error as Error);
    onError?.(appError.originalError || (error as Error));
    return fallback;
  }
}

// ================================================================
// 导出所有
// ================================================================

export default {
  // 错误类型
  ErrorCategory,
  ErrorSeverity,

  // 错误类
  ApplicationError,
  ValidationError,
  BusinessError,
  ComputationError,
  RenderingError,

  // 错误处理器
  errorHandler,

  // 便捷创建函数
  createValidationError,
  createBusinessError,
  createComputationError,
  createRenderingError,

  // 装饰器
  handleError,

  // 包装函数
  tryCatchAsync,
  tryCatch,
};
