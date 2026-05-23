/**
 * @module stores/aiStore
 * @description AI 助手状态管理。
 *              管理聊天消息、流式事件、过滤器配置以及 AI 模型参数。
 *              模拟 AI 响应逻辑已分离到 aiService.ts 文件中。
 *              从原先 stores/index.ts（~850 行单体 Store）拆分而来，遵循单一职责原则。
 */

import { create } from 'zustand';
import type {
  ChatMessage,
  StreamingEvent,
  StreamFilter,
  StreamingEntry,
} from '@/types';
import { generateUniqueId as generateId } from '@/utils/idGenerator';
import {
  MAX_CHAT_MESSAGES,
  MAX_STREAMING_EVENTS,
  MAX_STREAMING_ENTRIES,
  MODEL_TEMPERATURE_DEFAULT,
  MODEL_MAX_TOKENS_DEFAULT,
  DEFAULT_AI_PROVIDER,
} from '@/utils/constants';
import {
  getResponseChunks,
  simulateStreamResponse,
} from './aiService';

// ================================================================
// AI 状态接口 / AI State Interface
// ================================================================

/**
 * AI 助手相关状态。
 * 消息记录、流式事件、过滤器、模型参数以及流式日志条目。
 */
export interface AIState {
  // ---- 聊天消息 / Chat Messages ----
  /** 聊天对话中的所有消息 */
  chatMessages: ChatMessage[];
  /** 当前活跃的 AI 提供者 ID */
  activeProvider: string;
  /** AI 是否正在流式输出响应 */
  isStreaming: boolean;

  // ---- 流式事件 / Streaming Events ----
  /** 来自 AI 后端的流式事件列表 */
  streamingEvents: StreamingEvent[];
  /** 流式事件类型过滤器配置 */
  streamFilters: StreamFilter[];
  /** 模型温度参数 (0-2) */
  modelTemperature: number;
  /** 模型最大 Token 数 (64-32768) */
  modelMaxTokens: number;

  // ---- 流式日志 / Streaming Entries ----
  /** 流式输出的日志条目列表 */
  streamingEntries: StreamingEntry[];
  /** 流式输出是否处于活跃状态 */
  streamingActive: boolean;

  // ---- Actions: 聊天 / Chat Actions ----
  /** 添加一条聊天消息到对话中 */
  addMessage: (message: ChatMessage) => void;
  /** 更新最后一条 assistant 消息的内容（用于流式输出的增量更新） */
  updateLastAssistantMessage: (content: string) => void;
  /** 清空所有聊天消息 */
  clearMessages: () => void;
  /** 设置当前活跃的 AI 提供者 */
  setActiveProvider: (providerId: string) => void;
  /** 设置流式输出状态 */
  setIsStreaming: (streaming: boolean) => void;

  // ---- Actions: 流式事件 / Streaming Event Actions ----
  /** 添加一个流式事件 */
  addStreamEvent: (event: StreamingEvent) => void;
  /** 清空所有流式事件并重置过滤器计数 */
  clearStreamEvents: () => void;
  /** 切换指定类型的流式事件过滤器的启用/禁用 */
  toggleStreamFilter: (type: string) => void;
  /** 重置所有流式事件过滤器的计数 */
  resetStreamFilterCounts: () => void;
  /** 增加指定类型过滤器的计数 */
  incrementStreamFilterCount: (type: string) => void;
  /** 设置模型温度（自动钳制到 0-2） */
  setModelTemperature: (temp: number) => void;
  /** 设置模型最大 Token 数（自动钳制到 64-32768） */
  setModelMaxTokens: (tokens: number) => void;

  // ---- Actions: 流式日志 / Streaming Entry Actions ----
  /** 添加一条流式日志条目 */
  addStreamingEntry: (entry: StreamingEntry) => void;
  /** 清空所有流式日志条目 */
  clearStreamingEntries: () => void;
  /** 设置流式输出活跃状态 */
  setStreamingActive: (active: boolean) => void;

  // ---- Actions: 发送消息 / Send Message ----
  /**
   * 发送用户消息并获取 AI 响应。
   *
   * 当前使用 aiService 中的模拟响应逻辑生成回复。
   * 后续接入真实 API 时，只需修改 aiService 的实现即可。
   */
  sendMessage: (content: string) => Promise<void>;
}

// ================================================================
// 默认流式过滤器 / Default Stream Filters
// ================================================================

const DEFAULT_STREAM_FILTERS: StreamFilter[] = [
  { type: 'info', label: 'Info', labelZh: '信息', enabled: true, count: 0 },
  { type: 'step', label: 'Step', labelZh: '步骤', enabled: true, count: 0 },
  { type: 'result', label: 'Result', labelZh: '结果', enabled: true, count: 0 },
  { type: 'error', label: 'Error', labelZh: '错误', enabled: true, count: 0 },
  { type: 'warning', label: 'Warning', labelZh: '警告', enabled: true, count: 0 },
  { type: 'debug', label: 'Debug', labelZh: '调试', enabled: false, count: 0 },
];

// ================================================================
// Store 实现 / Store Implementation
// ================================================================

/**
 * AI 助手 Store。
 * 管理聊天对话、流式响应事件和模型配置。
 *
 * @example
 * ```typescript
 * const { chatMessages, sendMessage } = useAIStore();
 * await sendMessage('解释约束系统');
 * ```
 */
export const useAIStore = create<AIState>((set, get) => ({
  // ---- 初始状态 / Initial State ----
  chatMessages: [],
  activeProvider: DEFAULT_AI_PROVIDER,
  isStreaming: false,

  streamingEvents: [],
  streamFilters: DEFAULT_STREAM_FILTERS,
  modelTemperature: MODEL_TEMPERATURE_DEFAULT,
  modelMaxTokens: MODEL_MAX_TOKENS_DEFAULT,

  streamingEntries: [],
  streamingActive: false,

  // ================================================================
  // Actions: 聊天 / Chat Actions
  // ================================================================

  /**
   * 添加一条聊天消息。
   * 自动限制消息总数不超过 MAX_CHAT_MESSAGES 条，防止内存溢出。
   */
  addMessage: (message) =>
    set((state) => ({
      chatMessages: [...state.chatMessages.slice(-(MAX_CHAT_MESSAGES - 1)), message],
    })),

  /**
   * 更新最后一条 assistant 消息的内容。
   * 用于流式输出时逐块更新显示内容。
   * 从后向前查找第一条 role === 'assistant' 的消息并替换其内容。
   *
   * 使用安全的索引访问模式，避免 ! 非空断言：
   * 先通过变量引用保存数组中元素，进行 null/undefined 检查后再操作。
   */
  updateLastAssistantMessage: (content) =>
    set((state) => {
      const msgs = [...state.chatMessages];
      for (let i = msgs.length - 1; i >= 0; i--) {
        const msg = msgs[i];
        if (msg && msg.role === 'assistant') {
          msgs[i] = { ...msg, content };
          break;
        }
      }
      return { chatMessages: msgs };
    }),

  /** 清空所有聊天消息 */
  clearMessages: () => set({ chatMessages: [] }),

  /** 设置当前活跃的 AI 提供者 */
  setActiveProvider: (providerId) => set({ activeProvider: providerId }),

  /** 设置流式输出状态 */
  setIsStreaming: (streaming) => set({ isStreaming: streaming }),

  // ================================================================
  // Actions: 流式事件 / Streaming Event Actions
  // ================================================================

  /**
   * 添加一个流式事件。
   * 自动限制事件总数不超过 MAX_STREAMING_EVENTS 条。
   */
  addStreamEvent: (event) =>
    set((state) => ({
      streamingEvents: [...state.streamingEvents.slice(-(MAX_STREAMING_EVENTS - 1)), event],
    })),

  /** 清空所有流式事件并重置所有过滤器计数 */
  clearStreamEvents: () =>
    set((state) => ({
      streamingEvents: [],
      streamFilters: state.streamFilters.map((f) => ({ ...f, count: 0 })),
    })),

  /** 切换指定类型过滤器的启用/禁用状态 */
  toggleStreamFilter: (type) =>
    set((state) => ({
      streamFilters: state.streamFilters.map((f) =>
        f.type === type ? { ...f, enabled: !f.enabled } : f,
      ),
    })),

  /** 重置所有过滤器的计数 */
  resetStreamFilterCounts: () =>
    set((state) => ({
      streamFilters: state.streamFilters.map((f) => ({ ...f, count: 0 })),
    })),

  /** 增加指定类型过滤器的计数 */
  incrementStreamFilterCount: (type) =>
    set((state) => ({
      streamFilters: state.streamFilters.map((f) =>
        f.type === type ? { ...f, count: f.count + 1 } : f,
      ),
    })),

  /** 设置模型温度（自动钳制到 0-2） */
  setModelTemperature: (temp) =>
    set({ modelTemperature: Math.max(0, Math.min(2, temp)) }),

  /** 设置模型最大 Token 数（自动钳制到 64-32768） */
  setModelMaxTokens: (tokens) =>
    set({ modelMaxTokens: Math.max(64, Math.min(32768, tokens)) }),

  // ================================================================
  // Actions: 流式日志 / Streaming Entry Actions
  // ================================================================

  /**
   * 添加一条流式日志条目。
   * 自动限制条目总数不超过 MAX_STREAMING_ENTRIES 条。
   */
  addStreamingEntry: (entry) =>
    set((state) => ({
      streamingEntries: [...state.streamingEntries.slice(-(MAX_STREAMING_ENTRIES - 1)), entry],
    })),

  /** 清空所有流式日志条目 */
  clearStreamingEntries: () => set({ streamingEntries: [] }),

  /** 设置流式输出活跃状态 */
  setStreamingActive: (active) => set({ streamingActive: active }),

  // ================================================================
  // Actions: 发送消息 / Send Message
  // ================================================================

  /**
   * 发送用户消息并获取 AI 的流式模拟响应。
   *
   * 流程：
   * 1. 添加用户消息到聊天记录
   * 2. 创建空的 assistant 消息并标记为流式
   * 3. 通过 aiService 生成模拟响应文本分块
   * 4. 逐块更新 assistant 消息内容，模拟打字机效果
   * 5. 完成时清除流式标记
   *
   * 模拟响应逻辑已提取到 aiService.ts 中，便于后续替换为真实 API 调用。
   */
  sendMessage: async (content) => {
    const {
      addMessage,
      setIsStreaming,
      addStreamEvent,
      updateLastAssistantMessage,
      incrementStreamFilterCount,
      activeProvider,
    } = get();

    try {
    // 1. 添加用户消息
    const userMsg: ChatMessage = {
      id: `user-${generateId()}`,
      role: 'user',
      content,
      timestamp: Date.now(),
    };
    addMessage(userMsg);

    // 2. 创建空的 assistant 消息（流式占位）
    const assistantMsg: ChatMessage = {
      id: `assistant-${generateId()}`,
      role: 'assistant',
      content: '',
      timestamp: Date.now(),
      isStreaming: true,
    };
    addMessage(assistantMsg);
    setIsStreaming(true);

    // 3. 获取响应文本分块（优先上下文感知，回退到固定模拟）
    const chunks = getResponseChunks(activeProvider, content);

    // 4. 模拟流式输出
    await simulateStreamResponse(chunks, {
      addStreamEvent,
      incrementFilterCount: incrementStreamFilterCount,
      updateContent: (fullContent) => {
        updateLastAssistantMessage(fullContent);
      },
    });

    // 5. 完成流式输出
    updateLastAssistantMessage(chunks.join(''));

    // 清除流式标记（将 assistant 消息的 isStreaming 设为 false）
    // 使用安全的索引访问模式，避免 ! 非空断言
    set((state) => {
      const msgs = [...state.chatMessages];
      for (let i = msgs.length - 1; i >= 0; i--) {
        const msg = msgs[i];
        if (msg && msg.role === 'assistant') {
          msgs[i] = { ...msg, isStreaming: false };
          break;
        }
      }
      return { chatMessages: msgs };
    });
    } finally {
      setIsStreaming(false);
    }
  },
}));
