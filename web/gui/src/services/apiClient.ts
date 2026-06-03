/**
 * @module services/apiClient
 * @description AI API 客户端抽象层。
 *              为 Lv-00 GUI 提供统一的 AI API 调用接口，
 *              支持 OpenAI 兼容格式的多个提供者（OpenAI / Claude / Gemini / DeepSeek / 本地模型），
 *              并内置指数退避重试、超时控制、流式响应解析和错误分类功能。
 *
 *              当 aiStore 中的 sendMessage 调用真实 API 时，
 *              通过此模块替代 aiService.ts 中的模拟响应。
 *
 *              设计原则：
 *              - 提供者特定差异封装在请求格式化和响应解析中
 *              - 所有提供者统一遵循 OpenAI Chat Completions API 格式
 *              - 支持流式 (SSE) 和非流式两种调用模式
 *              - 错误信息支持中英文双语
 */

// ================================================================
// 类型定义
// ================================================================

/**
 * AI 提供商标识符 —— 统一使用 types/index.ts 中的定义
 * 类型成员：'openai' | 'deepseek' | 'dashscope' | 'anthropic' | 'gemini' | 'ollama' | 'custom'
 */
import type { AIProviderId } from '@/types';
export type { AIProviderId } from '@/types';

/** API 调用选项 */
export interface ApiCallOptions {
  /** AI 提供者标识符 */
  provider: AIProviderId;
  /** API 端点 URL（如果未指定，则根据 provider 推断默认端点） */
  endpoint?: string;
  /** API 密钥 */
  apiKey: string;
  /** 模型名称 */
  model?: string;
  /** 温度参数 (0-2) */
  temperature?: number;
  /** 最大输出 Token 数 */
  maxTokens?: number;
  /** 请求超时时间（毫秒），默认 30000 */
  timeoutMs?: number;
  /** 最大重试次数，默认 3 */
  maxRetries?: number;
}

/** 流式响应回调集合 */
export interface StreamCallbacks {
  /** 收到新的文本分块时调用 */
  onChunk: (text: string, fullContent: string) => void;
  /** 流式输出完成时调用 */
  onDone: (fullContent: string) => void;
  /** 发生错误时调用 */
  onError: (error: ApiClientError) => void;
}

/** API 客户端错误分类 */
export class ApiClientError extends Error {
  /** 错误类别：网络 / 认证 / 速率限制 / 服务器 / 超时 / 客户端 */
  readonly category: 'network' | 'auth' | 'rate_limit' | 'server' | 'timeout' | 'client';
  /** HTTP 状态码（如果有） */
  readonly statusCode?: number;
  /** 原始错误的详细信息 */
  readonly detail?: string;

  constructor(
    message: string,
    category: ApiClientError['category'],
    statusCode?: number,
    detail?: string,
  ) {
    super(message);
    this.name = 'ApiClientError';
    this.category = category;
    this.statusCode = statusCode;
    this.detail = detail;
  }

  /** 获取面向用户的中文错误提示 */
  getUserMessageZh(): string {
    switch (this.category) {
      case 'network': return `网络连接失败：${this.message}`;
      case 'auth': return `API 认证失败：请检查 API 密钥配置`;
      case 'rate_limit': return `请求频率超限：请稍后重试`;
      case 'server': return `AI 服务暂时不可用（${this.statusCode ?? '未知'}）：请稍后重试`;
      case 'timeout': return `AI 响应超时：请求已超过 ${this.detail ?? '设定'} 毫秒`;
      case 'client': return `请求参数错误：${this.message}`;
      default: return this.message;
    }
  }
}

// ================================================================
// 默认配置
// ================================================================

/** 各提供者的默认 API 端点 */
const DEFAULT_ENDPOINTS: Record<AIProviderId, string> = {
  openai: 'https://api.openai.com/v1/chat/completions',
  anthropic: 'https://api.anthropic.com/v1/messages',
  gemini: 'https://generativelanguage.googleapis.com/v1beta/models/gemini-pro:generateContent',
  deepseek: 'https://api.deepseek.com/v1/chat/completions',
  ollama: 'http://localhost:11434/v1/chat/completions', // 默认本地 Ollama 端点
  dashscope: 'https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions',
  custom: '',
};

/** 各提供者的默认模型名称 */
const DEFAULT_MODELS: Record<AIProviderId, string> = {
  openai: 'gpt-3.5-turbo',
  anthropic: 'claude-3-haiku-20240307',
  gemini: 'gemini-pro',
  deepseek: 'deepseek-chat',
  ollama: 'llama3',
  dashscope: 'qwen-turbo',
  custom: '',
};

/** 默认请求超时时间（毫秒） */
const DEFAULT_TIMEOUT_MS = 30000;

/** 默认最大重试次数 */
const DEFAULT_MAX_RETRIES = 3;

/** 指数退避的初始等待时间（毫秒） */
const INITIAL_BACKOFF_MS = 1000;

/** 不需要重试的 HTTP 状态码 */
const NON_RETRYABLE_STATUS_CODES = new Set([400, 401, 403, 404, 422]);

// ================================================================
// 辅助函数
// ================================================================

/**
 * 根据 HTTP 状态码对错误进行分类。
 *
 * @param statusCode - HTTP 响应状态码
 * @param message - 错误消息（用于自定义分类回退）
 * @returns 错误类别
 */
function classifyError(statusCode: number, message: string): ApiClientError['category'] {
  if (statusCode === 401 || statusCode === 403) return 'auth';
  if (statusCode === 429) return 'rate_limit';
  if (statusCode >= 500) return 'server';
  if (statusCode >= 400) return 'client';
  // 非 HTTP 错误根据消息内容判断
  if (message.includes('timeout') || message.includes('abort')) return 'timeout';
  if (message.includes('fetch') || message.includes('network') || message.includes('ECONNREFUSED')) return 'network';
  return 'client';
}

/**
 * 构建 OpenAI 兼容的请求体。
 *
 * @param messages - 消息列表
 * @param options - API 调用选项
 * @param stream - 是否使用流式模式
 * @returns 请求体对象和自定义请求头
 */
function buildRequestBody(
  messages: Array<{ role: string; content: string }>,
  options: ApiCallOptions,
  stream: boolean,
): { body: string; headers: Record<string, string> } {
  const headers: Record<string, string> = {
    'Content-Type': 'application/json',
  };

  // Anthropic (Claude) 使用不同的 API 格式和认证头
  if (options.provider === 'anthropic') {
    headers['x-api-key'] = options.apiKey;
    headers['anthropic-version'] = '2023-06-01';
    const body = {
      model: options.model ?? DEFAULT_MODELS.anthropic,
      messages: messages.map((m) => ({ role: m.role, content: m.content })),
      max_tokens: options.maxTokens ?? 2048,
      temperature: options.temperature ?? 0.7,
      stream,
    };
    return { body: JSON.stringify(body), headers };
  }

  // Google Gemini 使用不同的 API 格式和认证方式
  if (options.provider === 'gemini') {
    // Gemini 使用 URL 参数传递密钥，不需要 Authorization 头
    const body = {
      contents: messages.map((m) => ({
        role: m.role === 'assistant' ? 'model' : 'user',
        parts: [{ text: m.content }],
      })),
      generationConfig: {
        temperature: options.temperature ?? 0.7,
        maxOutputTokens: options.maxTokens ?? 2048,
      },
    };
    return { body: JSON.stringify(body), headers: { 'Content-Type': 'application/json' } };
  }

  // OpenAI 兼容格式（适用于 OpenAI、DeepSeek、本地 Ollama 等）
  headers['Authorization'] = `Bearer ${options.apiKey}`;
  const body = {
    model: options.model ?? DEFAULT_MODELS[options.provider] ?? 'gpt-3.5-turbo',
    messages,
    temperature: options.temperature ?? 0.7,
    max_tokens: options.maxTokens ?? 2048,
    stream,
  };
  return { body: JSON.stringify(body), headers };
}

/**
 * 从 SSE 流中解析文本分块。
 * 支持标准 OpenAI SSE 格式和 Anthropic 流式格式。
 *
 * @param reader - ReadableStream 默认读取器
 * @param callbacks - 流式回调集合
 * @param signal - 用于取消请求的 AbortSignal
 */
async function parseSSEStream(
  reader: ReadableStreamDefaultReader<Uint8Array>,
  callbacks: StreamCallbacks,
  signal: AbortSignal,
): Promise<void> {
  const decoder = new TextDecoder('utf-8');
  let fullContent = '';
  let buffer = '';

  try {
    while (true) {
      if (signal.aborted) {
        throw new ApiClientError('请求已被取消 / Request cancelled', 'client');
      }

      const { done, value } = await reader.read();
      if (done) break;

      buffer += decoder.decode(value, { stream: true });
      const lines = buffer.split('\n');
      // 保留最后一个可能不完整的行
      buffer = lines.pop() ?? '';

      for (const line of lines) {
        const trimmed = line.trim();
        if (!trimmed || trimmed === 'data: [DONE]') continue;

        // 标准 OpenAI SSE 格式: "data: {...}"
        if (trimmed.startsWith('data: ')) {
          try {
            const json = JSON.parse(trimmed.slice(6));
            const delta = json.choices?.[0]?.delta?.content;
            if (delta && typeof delta === 'string') {
              fullContent += delta;
              callbacks.onChunk(delta, fullContent);
            }
          } catch {
            // 忽略无效的 JSON 行（流式传输中的非标准行）
          }
        }
      }
    }
  } catch (err) {
    if (err instanceof ApiClientError) throw err;
    if (signal.aborted) {
      throw new ApiClientError('请求已取消 / Request cancelled', 'client');
    }
    throw new ApiClientError(
      `流式解析失败: ${(err as Error).message}`,
      'network',
    );
  } finally {
    try { reader.releaseLock(); } catch { /* 已释放 */ }
  }

  callbacks.onDone(fullContent);
}

/**
 * 从非流式响应中解析完整内容。
 *
 * @param response - Fetch 响应对象
 * @returns 解析出的文本内容
 */
async function parseNonStreamResponse(response: Response): Promise<string> {
  const data = await response.json();

  // OpenAI 兼容格式
  if (data.choices?.[0]?.message?.content) {
    return data.choices[0].message.content;
  }

  // Anthropic 格式
  if (data.content?.[0]?.text) {
    return data.content[0].text;
  }

  // Gemini 格式
  if (data.candidates?.[0]?.content?.parts?.[0]?.text) {
    return data.candidates[0].content.parts[0].text;
  }

  throw new ApiClientError('无法解析 AI 响应格式', 'client', 200);
}

// ================================================================
// 核心 API 调用函数
// ================================================================

/**
 * 发送流式聊天请求并实时返回响应分块。
 *
 * 内置指数退避重试机制：遇到 429/5xx 错误时自动重试，
 * 最多重试 `options.maxRetries` 次（默认 3 次）。
 *
 * 流程：
 * 1. 构建 OpenAI 兼容的请求体（支持各提供者的格式转换）
 * 2. 发送 fetch 请求，启用流式传输
 * 3. 逐行解析 SSE 数据流
 * 4. 每个文本分块通过 onChunk 回调推送
 * 5. 完成后调用 onDone 回调
 * 6. 遇到错误时调用 onError 回调
 *
 * @param messages - 对话消息列表
 * @param options - API 调用选项
 * @param callbacks - 流式响应回调
 * @param signal - 可选的 AbortSignal 用于取消请求
 */
export async function streamChat(
  messages: Array<{ role: string; content: string }>,
  options: ApiCallOptions,
  callbacks: StreamCallbacks,
  signal?: AbortSignal,
): Promise<void> {
  const endpoint = options.endpoint ?? DEFAULT_ENDPOINTS[options.provider];
  const maxRetries = options.maxRetries ?? DEFAULT_MAX_RETRIES;
  const timeoutMs = options.timeoutMs ?? DEFAULT_TIMEOUT_MS;

  for (let attempt = 0; attempt <= maxRetries; attempt++) {
    try {
      // 创建超时控制器：合并传入的 signal 和超时 signal
      const abortController = new AbortController();
      const timeoutId = setTimeout(() => abortController.abort(), timeoutMs);

      // 如果外部传入了 signal，建立关联
      const mergedSignal = signal
        ? combineAbortSignals(signal, abortController.signal)
        : abortController.signal;

      const { body, headers } = buildRequestBody(messages, options, true);

      const response = await fetch(endpoint, {
        method: 'POST',
        headers,
        body,
        signal: mergedSignal,
      });

      clearTimeout(timeoutId);

      // 检查响应状态
      if (!response.ok) {
        const category = classifyError(response.status, response.statusText);
        const errorText = await response.text().catch(() => '');
        const err = new ApiClientError(
          `API 请求失败 (${response.status}): ${errorText || response.statusText}`,
          category,
          response.status,
          errorText,
        );

        // 不可重试的错误直接抛出
        if (NON_RETRYABLE_STATUS_CODES.has(response.status)) {
          callbacks.onError(err);
          return;
        }

        // 可重试：等待并继续
        if (attempt < maxRetries) {
          await delay(INITIAL_BACKOFF_MS * Math.pow(2, attempt));
        }
        continue;
      }

      if (!response.body) {
        throw new ApiClientError('响应体为空 / Empty response body', 'server');
      }

      const reader = response.body.getReader();
      await parseSSEStream(reader, callbacks, mergedSignal);
      return; // 成功完成
    } catch (err) {
      if (err instanceof ApiClientError) {
        // 客户端错误或超时直接停止，不重试
        if (err.category === 'client' || err.category === 'timeout' || err.category === 'auth') {
          callbacks.onError(err);
          return;
        }
      }

      if (signal?.aborted) {
        callbacks.onError(new ApiClientError('请求已取消 / Request cancelled', 'client'));
        return;
      }

      // 最后一次尝试也失败了
      if (attempt >= maxRetries) {
        const finalErr = err instanceof ApiClientError
          ? err
          : new ApiClientError(
              `请求失败（已重试 ${maxRetries} 次）: ${(err as Error).message}`,
              'network',
            );
        callbacks.onError(finalErr);
        return;
      }

      // 等待后重试
      await delay(INITIAL_BACKOFF_MS * Math.pow(2, attempt));
    }
  }
}

/**
 * 发送非流式聊天请求，返回完整响应文本。
 *
 * 用于不需要流式输出的场景（如快捷查询）。
 *
 * @param messages - 对话消息列表
 * @param options - API 调用选项
 * @param signal - 可选的 AbortSignal 用于取消请求
 * @returns AI 返回的完整响应文本
 * @throws ApiClientError 如果请求失败
 */
export async function chat(
  messages: Array<{ role: string; content: string }>,
  options: ApiCallOptions,
  signal?: AbortSignal,
): Promise<string> {
  const endpoint = options.endpoint ?? DEFAULT_ENDPOINTS[options.provider];
  const maxRetries = options.maxRetries ?? DEFAULT_MAX_RETRIES;
  const timeoutMs = options.timeoutMs ?? DEFAULT_TIMEOUT_MS;

  for (let attempt = 0; attempt <= maxRetries; attempt++) {
    try {
      const abortController = new AbortController();
      const timeoutId = setTimeout(() => abortController.abort(), timeoutMs);
      const mergedSignal = signal
        ? combineAbortSignals(signal, abortController.signal)
        : abortController.signal;

      const { body, headers } = buildRequestBody(messages, options, false);

      const response = await fetch(endpoint, {
        method: 'POST',
        headers,
        body,
        signal: mergedSignal,
      });

      clearTimeout(timeoutId);

      if (!response.ok) {
        const category = classifyError(response.status, response.statusText);
        const errorText = await response.text().catch(() => '');
        const err = new ApiClientError(
          `API 请求失败 (${response.status}): ${errorText || response.statusText}`,
          category,
          response.status,
          errorText,
        );

        if (NON_RETRYABLE_STATUS_CODES.has(response.status)) {
          throw err;
        }

        if (attempt < maxRetries) {
          await delay(INITIAL_BACKOFF_MS * Math.pow(2, attempt));
        }
        continue;
      }

      return await parseNonStreamResponse(response);
    } catch (err) {
      if (err instanceof ApiClientError) {
        if (err.category === 'client' || err.category === 'timeout' || err.category === 'auth') {
          throw err;
        }
      }
      if (attempt >= maxRetries) {
        throw err instanceof ApiClientError
          ? err
          : new ApiClientError(
              `请求失败（已重试 ${maxRetries} 次）: ${(err as Error).message}`,
              'network',
            );
      }
      await delay(INITIAL_BACKOFF_MS * Math.pow(2, attempt));
    }
  }

  throw new ApiClientError('未知错误 / Unknown error', 'server');
}

// ================================================================
// 工具函数
// ================================================================

/**
 * 将两个 AbortSignal 合并为一个。
 * 任意一个信号触发时，合并后的信号也会被触发。
 * 【优化】修复内存泄漏：正确移除事件监听器
 *
 * @param a - 第一个 AbortSignal
 * @param b - 第二个 AbortSignal
 * @returns 合并后的 AbortSignal
 */
function combineAbortSignals(a: AbortSignal, b: AbortSignal): AbortSignal {
  if (a.aborted || b.aborted) return AbortSignal.abort();
  const controller = new AbortController();

  // 【安全修复 H-04】修复内存泄漏：正确移除事件监听器
  // 使用 { once: true } 确保监听器在触发后自动移除
  // 同时在 controller.abort() 时主动清理，防止重复触发
  const onAbort = () => {
    controller.abort();
    // 主动清理以防万一（虽然 once:true 应该自动处理）
    a.removeEventListener('abort', onAbort);
    b.removeEventListener('abort', onAbort);
  };

  a.addEventListener('abort', onAbort, { once: true });
  b.addEventListener('abort', onAbort, { once: true });

  return controller.signal;
}

/**
 * 异步延迟工具函数。
 *
 * @param ms - 延迟毫秒数
 * @returns Promise，在指定毫秒后 resolve
 */
function delay(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}
