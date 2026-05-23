/**
 * @module stores/aiService
 * @description AI 助手模拟响应服务。
 *              将 sendMessage 中的模拟 AI 响应逻辑从 Store 中分离出来，
 *              便于后续替换为真实 API 调用。
 *
 *              使用方式：aiStore 中的 sendMessage 调用此服务的方法。
 *
 *              本模块禁止使用 `!` 非空断言操作符，所有数组/Record 访问
 *              均通过 `if` 守卫或空值合并提供安全回退，避免运行时崩溃。
 */

import type { StreamingEvent } from '@/types';
import { AI_SIMULATED_DELAY_MIN, AI_SIMULATED_DELAY_MAX } from '@/utils/constants';

// ================================================================
// 模拟响应的辅助函数 / Simulated Response Helpers
// ================================================================

/**
 * 根据用户输入关键词，动态构建上下文感知的结构化响应。
 * 根据语言（中文/英文）生成双语友好的回复内容。
 *
 * @param query - 用户输入的消息
 * @returns 响应文本分块数组
 */
export function buildContextualResponse(query: string): string[] {
  const chunks: string[] = [];
  const isChinese = /[\u4e00-\u9fa5]/.test(query);

  /* 标题与前言 */
  if (isChinese) {
    chunks.push('## Lv-00 符号几何引擎分析\n\n');
  } else {
    chunks.push('## Lv-00 Symbolic Geometry Engine Analysis\n\n');
  }

  /* 约束系统关键词 */
  if (/constraint|约束|incidence|关联|betweenness|介值|intersection|相交|containment|包含/i.test(query)) {
    chunks.push(isChinese
      ? '### 约束系统概述\n\nLv-00 支持 **4 种约束类型**：\n\n1. **关联约束** — 点位于线段上\n2. **介值约束** — 点 B 位于 A 和 C 之间\n3. **相交约束** — 两条线段相交\n4. **包含约束** — 点在区域内\n\n'
      : '### Constraint System Overview\n\nLv-00 supports **4 constraint types**:\n\n1. **Incidence** — point lies on a segment\n2. **Betweenness** — point B is between A and C\n3. **Intersection** — two segments intersect\n4. **Containment** — point in region\n\n');
  }

  /* 求解器关键词 */
  if (/solve|求解|equation|方程|groebner/i.test(query)) {
    chunks.push(isChinese
      ? '### 符号求解器\n\n求解器基于 **Groebner 基算法**，支持：\n- 线性方程组\n- 二次方程\n- 多项式约束系统\n\n自由度数 = 2 × 点数 - 约束数\n\n'
      : '### Symbolic Solver\n\nThe solver uses **Groebner basis algorithm**, supporting:\n- Linear equation systems\n- Quadratic equations\n- Polynomial constraint systems\n\nDOF = 2 × numPoints - numConstraints\n\n');
  }

  /* 规范化关键词 */
  if (/normalize|规范化|merge|合并|归一化/i.test(query)) {
    chunks.push(isChinese
      ? '### 图规范化引擎\n\n规范化流程：\n1. 并查集合并等价节点\n2. 哈希预分组（O(n) 优化）\n3. 幂等性验证\n\n'
      : '### Graph Normalization Engine\n\nNormalization pipeline:\n1. Union-find equivalence merging\n2. Hash-based pre-grouping (O(n) optimization)\n3. Idempotency verification\n\n');
  }

  /* 公式关键词 */
  if (/formula|公式|latex|parse|解析|render|渲染/i.test(query)) {
    chunks.push(isChinese
      ? '### 公式系统\n\nLv-00 内置公式解析器和 KaTeX 渲染器：\n- 支持 LaTeX 数学语法\n- 实时预览渲染\n- 多格式输出（LaTeX/AsciiMath/Plain）\n\n'
      : '### Formula System\n\nBuilt-in formula parser and KaTeX renderer:\n- LaTeX math syntax support\n- Live preview rendering\n- Multi-format output (LaTeX/AsciiMath/Plain)\n\n');
  }

  /* 代码示例关键词 */
  if (/code|代码|example|示例|python|function|函数/i.test(query)) {
    if (isChinese) {
      chunks.push('### 代码示例\n\n');
      chunks.push('```python\n# Lv-00 Python API 示例\n');
      chunks.push('from lv00 import Graph, Point, Constraint\n\n');
      chunks.push('graph = Graph()\n');
      chunks.push('graph.add_point(0, 0, label="A")\n');
      chunks.push('graph.add_point(100, 0, label="B")\n');
      chunks.push('graph.add_segment("A", "B")\n');
      chunks.push('graph.add_incidence("C", "AB")\n');
      chunks.push('print(f"自由度: {graph.dof()}")\n');
      chunks.push('```\n\n');
    } else {
      chunks.push('### Code Example\n\n');
      chunks.push('```python\n# Lv-00 Python API example\n');
      chunks.push('from lv00 import Graph, Point, Constraint\n\n');
      chunks.push('graph = Graph()\n');
      chunks.push('graph.add_point(0, 0, label="A")\n');
      chunks.push('graph.add_point(100, 0, label="B")\n');
      chunks.push('graph.add_segment("A", "B")\n');
      chunks.push('graph.add_incidence("C", "AB")\n');
      chunks.push('print(f"Degrees of freedom: {graph.dof()}")\n');
      chunks.push('```\n\n');
    }
  }

  /* 架构关键词 */
  if (/architecture|架构|design|设计|module|模块/i.test(query)) {
    chunks.push(isChinese
      ? '### 系统架构\n\n```\n[Python API] ↔ [ctypes 绑定] ↔ [C 共享库]\n                                  ↕\n[React GUI]  ↔ [Tauri/Rust]  ↔ [GMP 任意精度库]\n```\n\n核心模块：符号坐标 → 约束图 → 规范化 → 求解/重写 → 合/证明\n\n'
      : '### System Architecture\n\n```\n[Python API] ↔ [ctypes binding] ↔ [C Shared Lib]\n                                  ↕\n[React GUI]  ↔ [Tauri/Rust]   ↔ [GMP Arbitrary Precision]\n```\n\nCore pipeline: Symbolic Coord → Constraint Graph → Normalization → Solve/Rewrite → Unify/Proof\n\n');
  }

  /* 结语（包含提供者信息占位） */
  if (isChinese) {
    chunks.push('---\n\n> 提示：当前为模拟 AI 响应。连接真实 API 后可获得更准确的分析。\n');
  } else {
    chunks.push('---\n\n> Note: This is a simulated AI response. Connect a real API for more accurate analysis.\n');
  }

  return chunks;
}

/**
 * 模拟响应的默认后备提供者 key。
 * 当传入的 provider 不在预定义列表中时使用此值。
 */
const FALLBACK_PROVIDER = 'openai' as const;

/**
 * 获取固定模拟响应（按提供者回退）。
 * 当上下文感知响应无法匹配合适内容时，使用此回退响应。
 *
 * @param provider - AI 提供者 ID（如 'openai', 'claude', 'gemini'）
 * @returns 响应文本分块数组，保证非空（至少返回默认 openai 响应）
 */
export function getFallbackResponse(provider: string): string[] {
  const simulatedResponses: Record<string, string[]> = {
    openai: [
      '## Analysis / 分析\n\n',
      'Based on your query, here is a detailed analysis:\n\n',
      '1. **First point**: The geometric constraint system uses symbolic computation.\n\n',
      '2. **Second point**: Points are represented as symbolic variables with exact arithmetic.\n\n',
      '```python\n# Example code\ndef solve_constraint(points, constraints):\n    for c in constraints:\n        apply(c, points)\n    return points\n```\n\n',
      '3. **Conclusion**: The Lv-00 engine provides exact geometric reasoning capabilities.\n\n',
      '> This is a simulated response for demonstration purposes.\n',
    ],
    claude: [
      'Let me think about this step by step.\n\n',
      '**Step 1**: Understanding the problem context...\n\n',
      '**Step 2**: Analyzing the geometric constraints...\n\n',
      'The key insight here is that Lv-00 uses a symbolic approach rather than numerical approximation.\n\n',
      '```\nConstraint: incidence(P1, Segment(A, B))\nResult: P1 lies on the line through A and B\n```\n\n',
      '**Step 3**: Formulating the solution...\n\n',
      'This approach ensures mathematical exactness in all geometric computations.\n',
    ],
    gemini: [
      'Here\'s a comprehensive overview:\n\n',
      '### Geometric Engine Architecture\n\n',
      'The Lv-00 symbolic geometry engine consists of several key components:\n\n',
      '- **Constraint Solver**: Handles incidence, betweenness, intersection, and containment\n',
      '- **Symbolic Arithmetic**: Exact computation without floating-point errors\n',
      '- **Region System**: Polygon definition and area computation\n\n',
      '```typescript\ninterface Point { id: number; x: number; y: number; }\ninterface Segment { p1: number; p2: number; id: number; }\n```\n\n',
      'This modular design allows for extensible geometric reasoning.\n',
    ],
    deepseek: [
      '好的，让我来分析一下您的问题。\n\n',
      '### Lv-00 符号几何引擎\n\n',
      'Lv-00 是一个基于符号计算的几何引擎，具有以下特点：\n\n',
      '1. **精确计算**：使用符号运算而非浮点近似\n',
      '2. **约束系统**：支持关联、介值、相交、包含四种约束类型\n',
      '3. **区域管理**：支持多边形区域的定义和面积计算\n\n',
      '```python\n# 约束示例\nadd_constraint(incidence, [point_id, segment_id])\nadd_constraint(betweenness, [p_a, p_b, p_c])\n```\n\n',
      '如需更多帮助，请随时提问！\n',
    ],
    local: [
      '[Local Model / 本地模型]\n\n',
      'Processing query locally...\n\n',
      'The local model provides fast inference without network dependency.\n\n',
      'Note: Local models may have limited knowledge compared to cloud-based providers.\n',
    ],
  };

  // 安全回退：优先使用传入 provider 的响应，不存在则使用默认 openai 响应
  const matched = simulatedResponses[provider];
  if (matched) {
    return matched;
  }
  // simulatedResponses 包含 FALLBACK_PROVIDER 的键，此处必定非空
  const fallback = simulatedResponses[FALLBACK_PROVIDER];
  return fallback ?? [];
}

/**
 * 获取响应文本分块。
 * 优先使用上下文感知响应，回退到固定模拟响应。
 *
 * @param provider - AI 提供者 ID
 * @param content - 用户消息内容
 * @returns 响应文本分块数组
 */
export function getResponseChunks(provider: string, content: string): string[] {
  const contextualChunks = buildContextualResponse(content);
  // 如果上下文响应有效（多于开头标题），使用它；否则用回退
  return contextualChunks.length > 1 ? contextualChunks : getFallbackResponse(provider);
}

// ================================================================
// 真实 API 客户端配置 / Real API Client Configuration
// ================================================================

/**
 * AI 客户端配置接口。
 * 存储连接到 OpenAI 兼容 API 端点所需的全部参数。
 *
 * AI client configuration interface.
 * Stores all parameters needed to connect to an OpenAI-compatible API endpoint.
 */
export interface AIClientConfig {
  /** API 端点 URL / API endpoint URL (e.g., "https://api.openai.com") */
  endpoint: string;
  /** API 密钥 / API key */
  apiKey: string;
  /** 模型名称 / Model name (e.g., "gpt-4o", "gpt-3.5-turbo") */
  modelName: string;
  /** 最大生成 token 数 / Maximum tokens to generate */
  maxTokens: number;
  /** 采样温度 (0-2) / Sampling temperature (0-2) */
  temperature: number;
}

/**
 * 真实 API 客户端接口。
 * 定义与远程 AI 服务交互的标准方法签名。
 *
 * Real API client interface.
 * Defines the standard method signature for interacting with remote AI services.
 */
export interface RealAPIClient {
  /**
   * 发送消息到 AI 服务并获取流式响应。
   * Send a message to the AI service and receive streaming response.
   *
   * @param content - 用户消息内容 / user message content
   * @param onChunk - 每收到一个文本分块时调用的回调 / callback invoked for each text chunk received
   * @returns 完整的响应文本 / complete response text
   */
  sendMessage(content: string, onChunk: (chunk: string) => void): Promise<string>;
}

// ---- 全局配置状态 / Global Configuration State ----

/** 当前激活的真实 API 配置，为 null 时使用模拟响应 */
let realAPIConfig: AIClientConfig | null = null;

/**
 * 检查当前是否使用真实 API。
 * Check whether the real API is currently active.
 *
 * @returns 如果已配置真实 API 则返回 true / true if real API config is set
 */
export function isUsingRealAPI(): boolean {
  return realAPIConfig !== null;
}

/**
 * 设置或清除真实 API 配置。
 * Set or clear the real API configuration.
 *
 * 传入 null 可切换回模拟响应模式。
 * Pass null to switch back to simulated response mode.
 *
 * @param config - API 配置对象或 null / API configuration object or null
 */
export function setRealAPIConfig(config: AIClientConfig | null): void {
  realAPIConfig = config;
}

/**
 * 创建真实 API 客户端。
 * Create a real API client.
 *
 * 支持 OpenAI 兼容的 chat completions 端点，使用 SSE 流式传输。
 * 当 fetch 或流处理过程中发生错误时，抛出异常供上层回退处理。
 *
 * Supports OpenAI-compatible chat completions endpoint with SSE streaming.
 * Throws exceptions on fetch or stream processing errors for upper-level fallback handling.
 *
 * 实现细节 / Implementation details:
 * - POST /v1/chat/completions with stream: true
 * - 解析 SSE (Server-Sent Events) 数据流 / Parse SSE data stream
 * - 安全访问嵌套 JSON 属性，禁止 ! 非空断言 / Safe nested access, no ! assertions
 * - 保留不完整的行缓冲区以处理分片 / Preserve incomplete line buffer for chunked data
 *
 * @param config - API 客户端配置 / API client configuration
 * @returns 具有 sendMessage 方法的客户端对象 / client object with sendMessage method
 */
export function createRealAPIClient(config: AIClientConfig): RealAPIClient {
  return {
    async sendMessage(content: string, onChunk: (chunk: string) => void): Promise<string> {
      // 构建请求 URL / Build request URL
      const url = `${config.endpoint}/v1/chat/completions`;

      // 发起 fetch 请求 / Initiate fetch request
      const response = await fetch(url, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${config.apiKey}`,
        },
        body: JSON.stringify({
          model: config.modelName,
          messages: [{ role: 'user', content }],
          max_tokens: config.maxTokens,
          temperature: config.temperature,
          stream: true,
        }),
      });

      // 检查 HTTP 状态 / Check HTTP status
      if (!response.ok) {
        let errorDetail = '';
        try {
          errorDetail = await response.text();
        } catch {
          errorDetail = '(unable to read error body)';
        }
        throw new Error(
          `API request failed with status ${response.status}: ${errorDetail}`,
        );
      }

      // 获取可读流 / Get readable stream
      const reader = response.body?.getReader();
      if (reader === null || reader === undefined) {
        throw new Error(
          'Response body is not readable (no ReadableStream available)',
        );
      }

      const decoder = new TextDecoder('utf-8');
      let fullContent = '';
      let buffer = '';

      // 循环读取流数据 / Loop reading stream data
      while (true) {
        const readResult = await reader.read();
        const { done, value } = readResult;
        if (done) {
          break;
        }

        // 解码二进制数据并追加到缓冲区 / Decode binary data and append to buffer
        if (value) {
          buffer += decoder.decode(value, { stream: true });
        }

        // 按行分割 SSE 数据 / Split SSE data by line
        const lines = buffer.split('\n');
        // 保留最后一个不完整的行 / Keep the last incomplete line
        const lastLine = lines.pop();
        buffer = lastLine !== undefined ? lastLine : '';

        for (const line of lines) {
          const trimmed = line.trim();
          if (trimmed === '') {
            continue;
          }
          if (!trimmed.startsWith('data: ')) {
            continue;
          }

          const dataStr = trimmed.slice(6);
          // SSE 流结束标记 / SSE stream end marker
          if (dataStr === '[DONE]') {
            continue;
          }

          try {
            const parsed = JSON.parse(dataStr);
            // 安全访问嵌套属性，禁止 ! 非空断言 / Safe nested access, no ! assertions
            const choices = parsed.choices;
            if (!choices || !Array.isArray(choices) || choices.length === 0) {
              continue;
            }
            const firstChoice = choices[0];
            if (firstChoice === null || firstChoice === undefined) {
              continue;
            }
            const delta = firstChoice.delta;
            if (delta === null || delta === undefined) {
              continue;
            }
            const deltaContent = delta.content;
            if (deltaContent && typeof deltaContent === 'string') {
              fullContent += deltaContent;
              onChunk(deltaContent);
            }
          } catch {
            // 忽略无法解析的 JSON 行 / Ignore unparseable JSON lines
            continue;
          }
        }
      }

      return fullContent;
    },
  };
}

/**
 * 模拟流式事件的类型定义（用于模拟 SSE 事件流）。
 * 使用 `as const` + `readonly` 确保编译期类型安全和不可变性。
 */
export const SIMULATED_EVENT_TYPES = [
  { type: 0, desc: 'Stream started / 流开始', filterType: 'info' as const },
  { type: 1, desc: 'Tokenizing input / 输入分词', filterType: 'step' as const },
  { type: 2, desc: 'Processing context / 处理上下文', filterType: 'step' as const },
  { type: 3, desc: 'Generating response / 生成回复', filterType: 'info' as const },
  { type: 5, desc: 'Context window: 4096 tokens', filterType: 'debug' as const },
  { type: 8, desc: 'Model loaded / 模型已加载', filterType: 'info' as const },
] as const;

/** 推导 SIMULATED_EVENT_TYPES 的只读类型 */
type SimulatedEventEntry = (typeof SIMULATED_EVENT_TYPES)[number];

/**
 * 模拟流式输出。
 * 依次发送预处理事件 → 文本分块事件 → 完成事件。
 *
 * 内部使用 `if` 守卫安全地访问数组元素，替代不安全的 `!` 非空断言。
 *
 * --- 真实 API 流处理已就绪 ---
 *
 * SSE 流处理已在 createRealAPIClient() (本文件第 ~285 行) 中完整实现：
 * - 使用 fetch + ReadableStream 读取 OpenAI 兼容的 SSE (data: 行) 流
 * - 解析 choices[0].delta.content 增量内容
 * - 通过 onChunk 回调逐块传递
 * - 保留不完整行缓冲区以处理 TCP 分片
 *
 * sendMessage() (第 ~509 行) 统一入口已集成切换逻辑：
 * - 若 realAPIConfig 已配置 → 使用 createRealAPIClient 的真实 SSE 流
 * - 若真实 API 连接失败 → 自动回退到本函数 (simulateStreamResponse)
 * - 若 realAPIConfig 为 null → 直接使用本函数的模拟响应
 *
 * --- WebSocket 备选方案 ---
 *
 * 若后端需要 WebSocket 流式传输（而非 SSE + fetch），可按以下步骤扩展：
 *
 *   1. 定义 WebSocketRealAPIClient 类，实现 RealAPIClient 接口
 *   2. 在 connectWebSocket() 中建立 ws:// 或 wss:// 连接
 *   3. ws.onmessage 中解析 JSON 帧，调用 onChunk 回调
 *   4. 在 sendMessage() 入口通过 realAPIConfig 的字段选择传输方式
 *
 *   示例 stub（保留以作参考，实际启用时移入独立模块）：
 *
 *   ```
 *   function createWebSocketClient(config: AIClientConfig): RealAPIClient {
 *     return {
 *       async sendMessage(content, onChunk): Promise<string> {
 *         const wsUrl = config.endpoint.replace(/^http/, 'ws') + '/v1/stream';
 *         const ws = new WebSocket(wsUrl);
 *         let fullContent = '';
 *         return new Promise((resolve, reject) => {
 *           ws.onopen = () => ws.send(JSON.stringify({ model: config.modelName, messages: [{ role: 'user', content }] }));
 *           ws.onmessage = (event) => {
 *             const parsed = JSON.parse(event.data);
 *             if (parsed.done) { ws.close(); resolve(fullContent); return; }
 *             const text = parsed.content ?? '';
 *             fullContent += text;
 *             onChunk(text);
 *           };
 *           ws.onerror = () => reject(new Error('WebSocket connection error'));
 *           ws.onclose = () => resolve(fullContent);
 *         });
 *       },
 *     };
 *   }
 *   ```
 *
 * 当前 mock 实现（下方）用于开发/演示阶段，连接真实 API 后自动跳过。
 *
 * @param chunks - 响应文本分块
 * @param callbacks - 事件回调集合
 *   - addStreamEvent: 添加流式事件到事件列表
 *   - incrementFilterCount: 增加对应过滤类型的计数
 *   - updateContent: 更新累积的响应内容
 */
export async function simulateStreamResponse(
  chunks: string[],
  callbacks: {
    addStreamEvent: (event: StreamingEvent) => void;
    incrementFilterCount: (type: string) => void;
    updateContent: (fullContent: string) => void;
  },
): Promise<void> {
  // 发送预处理事件（使用安全的按索引访问，避免 ! 非空断言）
  for (let i = 0; i < SIMULATED_EVENT_TYPES.length; i++) {
    const evt: SimulatedEventEntry | undefined = SIMULATED_EVENT_TYPES[i];
    if (!evt) {
      continue; // 安全跳过 null/undefined 条目
    }
    await delay(AI_SIMULATED_DELAY_MIN + Math.random() * (AI_SIMULATED_DELAY_MAX - AI_SIMULATED_DELAY_MIN));
    callbacks.addStreamEvent({
      type: evt.type,
      description: evt.desc,
      stepNumber: i + 1,
    });
    callbacks.incrementFilterCount(evt.filterType);
  }

  // 流式输出响应分块（使用安全的按索引访问，避免 ! 非空断言）
  let fullContent = '';
  for (let i = 0; i < chunks.length; i++) {
    const chunk = chunks[i];
    if (chunk === undefined || chunk === null) {
      continue; // 安全跳过 null/undefined 分块
    }
    await delay(AI_SIMULATED_DELAY_MIN + Math.random() * (AI_SIMULATED_DELAY_MAX - AI_SIMULATED_DELAY_MIN));
    fullContent += chunk;
    callbacks.updateContent(fullContent);

    callbacks.addStreamEvent({
      type: 10,
      description: `Chunk ${i + 1}/${chunks.length} / 块 ${i + 1}/${chunks.length}`,
      stepNumber: SIMULATED_EVENT_TYPES.length + i + 1,
    });
    callbacks.incrementFilterCount('result');
  }

  // 完成事件
  callbacks.addStreamEvent({
    type: 15,
    description: 'Response complete / 回复完成',
    stepNumber: SIMULATED_EVENT_TYPES.length + chunks.length + 1,
  });
  callbacks.incrementFilterCount('result');
}

/** 异步延迟辅助函数 */
function delay(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

// ================================================================
// 统一消息发送入口 / Unified Message Sending Entry Point
// ================================================================

/**
 * 统一的消息发送入口。
 * 优先使用真实 API（如果已配置），失败时自动回退到模拟响应。
 *
 * Unified message sending entry point.
 * Prefers real API if configured, automatically falls back to simulated response on failure.
 *
 * 执行流程 / Execution flow:
 * 1. 检查 realAPIConfig 是否已配置 / Check if realAPIConfig is set
 * 2. 若已配置：创建客户端并尝试流式调用 / If set: create client and attempt streaming call
 * 3. 若真实 API 失败：自动回退到模拟响应 / If real API fails: auto-fallback to simulated response
 * 4. 若未配置：直接使用模拟响应 / If not set: use simulated response directly
 *
 * @param content - 用户消息内容 / user message content
 * @param provider - AI 提供者 ID（用于模拟回退）/ AI provider ID (for simulated fallback)
 * @param callbacks - 回调集合 / callback collection
 *   - addStreamEvent: 添加流式事件到事件列表 / add streaming event to event list
 *   - incrementFilterCount: 增加对应过滤类型的计数 / increment count for filter type
 *   - updateContent: 更新累积的响应内容 / update accumulated response content
 * @returns 完整的响应文本 / complete response text
 */
export async function sendMessage(
  content: string,
  provider: string,
  callbacks: {
    addStreamEvent: (event: StreamingEvent) => void;
    incrementFilterCount: (type: string) => void;
    updateContent: (fullContent: string) => void;
  },
): Promise<string> {
  // 优先尝试真实 API / Try real API first
  if (realAPIConfig !== null) {
    try {
      const client = createRealAPIClient(realAPIConfig);
      let accumulatedContent = '';

      // 发送真实 API 流开始事件 / Send real API stream start event
      callbacks.addStreamEvent({
        type: 0,
        description: 'Real API stream started / 真实 API 流开始',
        stepNumber: 1,
      });
      callbacks.incrementFilterCount('info');

      // 使用真实 API 客户端发送消息 / Send message using real API client
      const result = await client.sendMessage(content, (chunk: string) => {
        accumulatedContent += chunk;
        callbacks.updateContent(accumulatedContent);
      });

      // 发送真实 API 完成事件 / Send real API completion event
      callbacks.addStreamEvent({
        type: 15,
        description: 'Real API response complete / 真实 API 回复完成',
        stepNumber: 2,
      });
      callbacks.incrementFilterCount('result');

      return result;
    } catch {
      // 真实 API 失败，发送回退通知并继续使用模拟 / Real API failed, notify and use simulated
      callbacks.addStreamEvent({
        type: 3,
        description:
          'Real API unavailable, falling back to simulated response / 真实 API 不可用，回退到模拟响应',
        stepNumber: 1,
      });
      callbacks.incrementFilterCount('info');
      // 继续执行下方模拟响应逻辑 / Continue to simulated response below
    }
  }

  // 使用模拟响应 / Use simulated response
  const chunks = getResponseChunks(provider, content);
  await simulateStreamResponse(chunks, callbacks);
  return chunks.join('');
}
