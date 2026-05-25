/**
 * @module services/logger
 * @description Lv-00 统一日志服务。
 *              包装 console.log / console.warn / console.error，
 *              提供统一的日志级别管理、前缀标记和环境自适应输出。
 *
 *              核心功能：
 *              - 四个日志级别：debug / info / warn / error
 *              - 级别过滤：低于当前阈值的日志不会输出
 *              - 生产环境自动禁用 debug 级别
 *              - 所有输出带 [Lv-00] 前缀，便于跨系统日志检索
 *              - 开发环境：彩色控制台输出，方便肉眼识别级别
 *              - 生产环境：结构化 JSON 日志，便于日志采集与分析
 *
 *              使用示例：
 *              ```ts
 *              import { logger, LogLevel } from './services/logger';
 *              logger.info('应用启动成功');
 *              logger.debug('调试信息仅在开发环境输出');
 *              logger.setLevel(LogLevel.WARN); // 仅输出警告及以上级别
 *              ```
 *
 * @since 3.0.0
 */

// ================================================================
// 日志级别定义
// ================================================================

/**
 * 日志级别枚举
 *
 * 级别从低到高依次为：DEBUG < INFO < WARN < ERROR
 * 设置当前级别后，仅输出级别 >= 当前级别的日志。
 *
 * @example
 *   logger.setLevel(LogLevel.WARN);  // 仅输出 WARN 和 ERROR
 */
export enum LogLevel {
  /** 调试信息，生产环境默认禁用 */
  DEBUG = 0,
  /** 常规信息，应用运行状态 */
  INFO = 1,
  /** 警告信息，需要关注但不影响运行 */
  WARN = 2,
  /** 错误信息，需要立即处理 */
  ERROR = 3,
}

// ================================================================
// 内部映射表
// ================================================================

/** 日志级别 -> 中文标签映射 */
const LEVEL_LABELS: Record<LogLevel, string> = {
  [LogLevel.DEBUG]: 'DEBUG',
  [LogLevel.INFO]: 'INFO ',
  [LogLevel.WARN]: 'WARN ',
  [LogLevel.ERROR]: 'ERROR',
};

/** 日志级别 -> console 方法映射 */
const LEVEL_CONSOLE_METHODS: Record<LogLevel, 'log' | 'info' | 'warn' | 'error'> = {
  [LogLevel.DEBUG]: 'log',
  [LogLevel.INFO]: 'info',
  [LogLevel.WARN]: 'warn',
  [LogLevel.ERROR]: 'error',
};

/**
 * 开发环境彩色输出所用的 CSS 样式
 *
 * 颜色灵感来源于 Lv-00 设计系统的调色板：
 * - DEBUG: 灰色，低调不显眼
 * - INFO:  蓝色，中性信息
 * - WARN:  琥珀色加粗，引起注意
 * - ERROR: 红色加粗，强烈警告
 */
const LEVEL_STYLES: Record<LogLevel, string> = {
  [LogLevel.DEBUG]: 'color: #8b949e',
  [LogLevel.INFO]: 'color: #58a6ff',
  [LogLevel.WARN]: 'color: #d29922; font-weight: bold',
  [LogLevel.ERROR]: 'color: #f85149; font-weight: bold',
};

// ================================================================
// 工具函数
// ================================================================

/**
 * 检测当前是否运行在生产环境
 *
 * 优先使用 Vite 提供的 import.meta.env.PROD，
 * 如果不可用（如测试环境），则默认视为开发环境。
 *
 * @returns {boolean} 是否为生产环境
 */
function isProduction(): boolean {
  try {
    // Vite 构建时会将 import.meta.env.PROD 替换为布尔字面量
    if (
      typeof import.meta !== 'undefined' &&
      import.meta.env &&
      import.meta.env.PROD === true
    ) {
      return true;
    }
  } catch {
    // import.meta 不可用，默认为开发环境
  }
  return false;
}

/**
 * 格式化时间戳供开发环境日志使用
 *
 * 仅取 ISO 时间串的时间部分（HH:mm:ss.SSS），
 * 减少控制台噪音，保持日志紧凑。
 *
 * @returns {string} 格式为 "HH:mm:ss.SSS" 的时间戳
 */
function devTimestamp(): string {
  return new Date().toISOString().slice(11, 23);
}

/**
 * 将日志参数序列化为字符串（生产环境使用）
 *
 * 支持字符串、数字、对象等多种类型的安全序列化。
 *
 * @param args - 日志参数列表
 * @returns {string} 序列化后的消息字符串
 */
function serializeArgs(args: unknown[]): string {
  return args
    .map((a) => {
      if (typeof a === 'string') return a;
      if (a instanceof Error) return `${a.name}: ${a.message}`;
      try {
        return JSON.stringify(a);
      } catch {
        return String(a);
      }
    })
    .join(' ');
}

// ================================================================
// Logger 类
// ================================================================

/**
 * Lv-00 统一日志服务类
 *
 * 包装浏览器原生 console API，提供：
 * 1. 级别过滤 — 低于阈值的日志静默忽略
 * 2. 前缀注入 — 所有输出带 `[Lv-00]` 前缀
 * 3. 环境自适应 — 开发环境彩色，生产环境 JSON
 * 4. 运行时切换 — 可通过 setLevel() 动态调整输出粒度
 *
 * @example
 *   import { logger } from './services/logger';
 *   logger.info('GUI 初始化完成');
 *   logger.warn('API 响应延迟偏高', { latencyMs: 2500 });
 *   logger.error('渲染失败', new Error('Canvas 上下文丢失'));
 */
class Logger {
  /** 统一日志前缀，便于 grep / 采集 */
  private readonly prefix: string;

  /** 当前日志级别阈值 */
  private currentLevel: LogLevel;

  /** 是否为生产环境（决定输出格式） */
  private readonly prod: boolean;

  /**
   * 创建 Logger 实例
   *
   * @param prefix - 日志前缀，默认为 '[Lv-00]'
   */
  constructor(prefix: string = '[Lv-00]') {
    this.prefix = prefix;
    this.prod = isProduction();
    // 生产环境默认将级别设为 INFO，静默忽略所有 DEBUG 日志
    this.currentLevel = this.prod ? LogLevel.INFO : LogLevel.DEBUG;
  }

  // ------------------------------------------------------------------
  // 公共 API
  // ------------------------------------------------------------------

  /**
   * 输出 DEBUG 级别日志
   *
   * 仅在当前级别 <= DEBUG 时输出。
   * 生产环境下默认被过滤。
   *
   * @param args - 任意日志参数
   */
  debug(...args: unknown[]): void {
    this.emit(LogLevel.DEBUG, args);
  }

  /**
   * 输出 INFO 级别日志
   *
   * 用于记录应用正常运行状态的关键节点。
   *
   * @param args - 任意日志参数
   */
  info(...args: unknown[]): void {
    this.emit(LogLevel.INFO, args);
  }

  /**
   * 输出 WARN 级别日志
   *
   * 用于记录需要关注但不阻断流程的异常情况。
   *
   * @param args - 任意日志参数
   */
  warn(...args: unknown[]): void {
    this.emit(LogLevel.WARN, args);
  }

  /**
   * 输出 ERROR 级别日志
   *
   * 用于记录需要立即处理的错误。
   * ERROR 级别始终输出，不受阈值限制。
   *
   * @param args - 任意日志参数
   */
  error(...args: unknown[]): void {
    this.emit(LogLevel.ERROR, args);
  }

  /**
   * 获取当前日志级别
   *
   * @returns {LogLevel} 当前级别枚举值
   */
  getLevel(): LogLevel {
    return this.currentLevel;
  }

  /**
   * 动态设置日志级别
   *
   * 运行时调整输出粒度，例如在排查问题时临时降低级别以获取更详细的日志。
   *
   * @param level - 新的日志级别阈值
   *
   * @example
   *   logger.setLevel(LogLevel.DEBUG); // 开启所有日志
   *   logger.setLevel(LogLevel.WARN);  // 仅输出警告和错误
   */
  setLevel(level: LogLevel): void {
    const old = this.currentLevel;
    this.currentLevel = level;
    // 仅当级别真正发生变化时才输出提示
    if (old !== level) {
      this.emit(LogLevel.INFO, [`日志级别已切换: ${LEVEL_LABELS[old].trim()} -> ${LEVEL_LABELS[level].trim()}`]);
    }
  }

  // ------------------------------------------------------------------
  // 内部实现
  // ------------------------------------------------------------------

  /**
   * 日志输出的核心方法
   *
   * 根据运行环境选择不同的输出策略：
   * - 开发环境：使用 console 的 %c 占位符实现彩色分级输出
   * - 生产环境：将日志序列化为单行 JSON，便于日志采集管道处理
   *
   * @param level - 日志级别
   * @param args - 用户传入的日志参数列表
   */
  private emit(level: LogLevel, args: unknown[]): void {
    // 级别过滤：低于当前阈值的日志不输出
    if (level < this.currentLevel) return;

    const method = LEVEL_CONSOLE_METHODS[level];
    const label = LEVEL_LABELS[level];

    if (this.prod) {
      // ============================================================
      // 生产环境：结构化 JSON 日志
      // 单行 JSON 格式，字段固定，便于 ELK / Splunk / DataDog 等采集
      // ============================================================
      const logEntry: Record<string, unknown> = {
        app: this.prefix,
        level: label.trim(),
        time: new Date().toISOString(),
        msg: serializeArgs(args),
      };
      console[method](JSON.stringify(logEntry));
    } else {
      // ============================================================
      // 开发环境：彩色控制台输出
      // 利用 console 的 %c 占位符为不同部分赋予不同颜色
      //
      // 输出格式：[Lv-00] [INFO ] 14:25:03.123 日志内容
      //
      // 颜色策略：
      //   - 前缀 [Lv-00]：青色加粗（品牌色）
      //   - 级别标签：各级别独立颜色
      //   - 时间戳：灰色调
      //   - 日志内容：继承浏览器默认样式
      // ============================================================
      const style = LEVEL_STYLES[level];
      const ts = devTimestamp();

      console[method](
        // 格式字符串：三部分各用 %c 占位
        `%c${this.prefix} %c[${label}] %c${ts}`,
        // 前缀样式：使用 Lv-00 品牌 accent 色
        'color: #00bcd4; font-weight: bold',
        // 级别标签样式：各级别独立
        style,
        // 时间戳样式：低调灰色
        'color: #484f58',
        // 展开用户的日志参数
        ...args,
      );
    }
  }
}

// ================================================================
// 单例导出
// ================================================================

/**
 * Lv-00 日志服务单例
 *
 * 全局唯一实例，所有模块共享同一个 Logger。
 *
 * @example
 *   import { logger } from './services/logger';
 *   logger.info('模块已加载');
 */
export const logger = new Logger('[Lv-00]');
