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
 *
 *              支持三种 AI 响应模式：
 *              1. 模拟响应（默认）：使用 buildContextualResponse 生成上下文感知的回复
 *              2. 真实 API（SSE）：通过 createRealAPIClient 连接 OpenAI 兼容端点
 *              3. WebSocket：通过 WebSocketAIClient 连接 WebSocket 服务端
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
 * 匹配规则：
 * - 约束系统：constraint/约束/incidence/关联/betweenness/介值/intersection/相交/containment/包含
 * - 求解器：solve/求解/equation/方程/groebner
 * - 规范化：normalize/规范化/merge/合并/归一化
 * - 公式：formula/公式/latex/parse/解析/render/渲染
 * - 代码：code/代码/example/示例/python/function/函数
 * - 架构：architecture/架构/design/设计/module/模块
 *
 * @param query - 用户输入的消息
 * @returns 响应文本分块数组（至少包含标题和结语，保证非空）
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
      ? '### 约束系统概述\n\nLv-00 支持 **5 种约束类型**：\n\n1. **关联约束 (incidence)** — 点位于线段上\n2. **介值约束 (betweenness)** — 点 B 位于 A 和 C 之间\n3. **相交约束 (intersection)** — 两条线段相交\n4. **包含约束 (containment)** — 点在区域内\n5. **连接约束 (connection)** — 一般连接关系\n\n'
      : '### Constraint System Overview\n\nLv-00 supports **5 constraint types**:\n\n1. **Incidence** — point lies on a segment\n2. **Betweenness** — point B is between A and C\n3. **Intersection** — two segments intersect\n4. **Containment** — point in region\n5. **Connection** — general connection between elements\n\n');
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
      ? '### 系统架构\n\n```\n[Python API] ↔ [ctypes 绑定] ↔ [C 共享库]\n                                  ↕\n[React GUI]  ↔ [Tauri/Rust]  ↔ [GMP 任意精度库]\n```\n\n核心模块：符号坐标 → 约束图 → 规范化 → 求解/重写 → 合一/证明\n\n'
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
 * @param provider - AI 提供者 ID（如 'openai', 'claude', 'gemini', 'deepseek', 'local'）
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
      '2. **约束系统**：支持关联、介值、相交、包含、连接五种约束类型\n',
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
 * @returns 响应文本分块数组（保证非空）
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
 */
export interface AIClientConfig {
  /** API 端点 URL（如 "https://api.openai.com"） */
  endpoint: string;
  /** API 密钥（Bearer Token） */
  apiKey: string;
  /** 模型名称（如 "gpt-4o", "gpt-3.5-turbo"） */
  modelName: string;
  /** 最大生成 token 数 */
  maxTokens: number;
  /** 采样温度 (0-2)，值越高输出越随机 */
  temperature: number;
}

/**
 * 真实 API 客户端接口。
 * 定义与远程 AI 服务交互的标准方法签名。
 */
export interface RealAPIClient {
  /**
   * 发送消息到 AI 服务并获取流式响应。
   *
   * @param content - 用户消息内容
   * @param onChunk - 每收到一个文本分块时调用的回调
   * @returns 完整的响应文本
   * @throws 当 HTTP 请求失败或流处理出错时抛出异常
   */
  sendMessage(content: string, onChunk: (chunk: string) => void): Promise<string>;
}

// ---- 全局配置状态 / Global Configuration State ----

/** 当前激活的真实 API 配置，为 null 时使用模拟响应 */
let realAPIConfig: AIClientConfig | null = null;

/**
 * 检查当前是否使用真实 API。
 *
 * @returns 如果已配置真实 API 则返回 true
 */
export function isUsingRealAPI(): boolean {
  return realAPIConfig !== null;
}

/**
 * 设置或清除真实 API 配置。
 * 传入 null 可切换回模拟响应模式。
 *
 * @param config - API 配置对象或 null
 */
export function setRealAPIConfig(config: AIClientConfig | null): void {
  realAPIConfig = config;
}

/**
 * 创建真实 API 客户端。
 *
 * 支持 OpenAI 兼容的 chat completions 端点，使用 SSE 流式传输。
 * 当 fetch 或流处理过程中发生错误时，抛出异常供上层回退处理。
 *
 * 实现细节：
 * - POST /v1/chat/completions with stream: true
 * - 解析 SSE (Server-Sent Events) 数据流
 * - 安全访问嵌套 JSON 属性，禁止 ! 非空断言
 * - 保留不完整的行缓冲区以处理 TCP 分片
 * - 在异常路径上显式取消 reader，防止资源泄漏
 *
 * @param config - API 客户端配置
 * @returns 具有 sendMessage 方法的客户端对象
 */
export function createRealAPIClient(config: AIClientConfig): RealAPIClient {
  return {
    async sendMessage(content: string, onChunk: (chunk: string) => void): Promise<string> {
      // 构建请求 URL
      const url = `${config.endpoint}/v1/chat/completions`;

      // 发起 fetch 请求
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

      // 检查 HTTP 状态
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

      // 获取可读流
      const reader = response.body?.getReader();
      if (reader === null || reader === undefined) {
        throw new Error(
          'Response body is not readable (no ReadableStream available)',
        );
      }

      const decoder = new TextDecoder('utf-8');
      let fullContent = '';
      let buffer = '';

      try {
        // 循环读取流数据
        while (true) {
          const readResult = await reader.read();
          const { done, value } = readResult;
          if (done) {
            break;
          }

          // 解码二进制数据并追加到缓冲区
          if (value) {
            buffer += decoder.decode(value, { stream: true });
          }

          // 按行分割 SSE 数据
          const lines = buffer.split('\n');
          // 保留最后一个不完整的行
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
            // SSE 流结束标记
            if (dataStr === '[DONE]') {
              continue;
            }

            try {
              const parsed = JSON.parse(dataStr);
              // 安全访问嵌套属性，禁止 ! 非空断言
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
              // 忽略无法解析的 JSON 行
              continue;
            }
          }
        }
      } finally {
        // 确保在异常路径上释放 reader，防止资源泄漏
        try {
          reader.releaseLock();
        } catch {
          // reader 可能已关闭，忽略释放错误
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

// ================================================================
// WebSocket 客户端 / WebSocket Client
// ================================================================

/**
 * WebSocket 连接状态枚举。
 * - disconnected: 未连接（初始状态或主动断开后）
 * - connecting: 正在建立连接
 * - connected: 已连接，可正常通信
 * - reconnecting: 连接断开后正在自动重连
 */
export type WebSocketState = 'disconnected' | 'connecting' | 'connected' | 'reconnecting';

/**
 * WebSocket 客户端配置接口。
 */
export interface WebSocketClientConfig {
  /** WebSocket 服务器地址（如 "ws://localhost:8080/ws"） */
  url: string;
  /** 最大重连次数（默认 10） */
  maxReconnectAttempts?: number;
  /** 初始重连延迟（毫秒，默认 1000） */
  initialReconnectDelay?: number;
  /** 最大重连延迟（毫秒，默认 30000） */
  maxReconnectDelay?: number;
  /** 连接超时（毫秒，默认 10000） */
  connectionTimeout?: number;
  /** 请求超时（毫秒，默认 30000） */
  requestTimeout?: number;
}

/**
 * WebSocket 消息类型（客户端 -> 服务端）。
 */
export interface WSRequestMessage {
  /** 查询唯一标识符 */
  id: string;
  /** 查询类型：'query'（通用查询）| 'proof'（证明）| 'solve'（求解） */
  type: 'query' | 'proof' | 'solve';
  /** 查询内容文本 */
  content: string;
  /** 发送时间戳（毫秒） */
  timestamp: number;
}

/**
 * WebSocket 响应类型（服务端 -> 客户端）。
 */
export interface WSResponseMessage {
  /** 对应的查询 ID */
  id: string;
  /** 响应类型：'chunk'（文本块）| 'done'（完成）| 'error'（错误） */
  type: 'chunk' | 'done' | 'error';
  /** 文本内容（chunk 类型时有值） */
  content?: string;
  /** 错误信息（error 类型时有值） */
  error?: string;
  /** 响应时间戳（毫秒） */
  timestamp: number;
}

/**
 * WebSocket 客户端回调接口。
 * 用于将 WebSocket 事件通知给上层使用者。
 */
export interface WebSocketClientCallbacks {
  /** 收到文本分块时调用 */
  onChunk: (queryId: string, chunk: string) => void;
  /** 查询完成时调用，包含完整响应文本 */
  onComplete: (queryId: string, fullContent: string) => void;
  /** 查询出错时调用，包含错误描述 */
  onError: (queryId: string, error: string) => void;
  /** 连接状态变化时调用 */
  onStateChange: (state: WebSocketState) => void;
}

/**
 * WebSocket AI 客户端。
 *
 * 特性：
 * - 自动重连（指数退避 + 随机抖动）：连接意外断开后自动尝试重新连接
 * - 流式响应解析：支持 JSON 帧和 SSE-like 两种消息格式
 * - 请求超时处理：每个查询有独立的超时计时器，超时后自动 reject
 * - 连接生命周期管理：支持主动断开和自动重连的区分
 * - 资源清理：disconnect 时清理所有计时器、挂起查询和事件监听器
 *
 * 使用方式：
 * ```ts
 * const client = new WebSocketAIClient({ url: 'ws://localhost:8080/ws' });
 * client.onResponse({
 *   onChunk: (id, chunk) => console.log(chunk),
 *   onComplete: (id, content) => console.log('Done:', content),
 *   onError: (id, err) => console.error(err),
 *   onStateChange: (state) => console.log('State:', state),
 * });
 * await client.connect();
 * await client.sendQuery('Solve the triangle ABC');
 * client.disconnect();
 * ```
 */
export class WebSocketAIClient {
  /** 客户端配置（所有可选字段已填充默认值） */
  private config: Required<WebSocketClientConfig>;
  /** WebSocket 实例引用（null 表示未连接） */
  private ws: WebSocket | null = null;
  /** 当前连接状态 */
  private state: WebSocketState = 'disconnected';
  /** 事件回调注册（null 表示未注册） */
  private callbacks: WebSocketClientCallbacks | null = null;
  /** 当前已尝试的重连次数 */
  private reconnectAttempts = 0;
  /** 重连延迟计时器 ID（null 表示无活跃计时器） */
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  /** 连接超时计时器 ID（null 表示无活跃计时器） */
  private connectionTimer: ReturnType<typeof setTimeout> | null = null;
  /**
   * 挂起的查询映射表。
   * key: 查询 ID, value: Promise 的 resolve/reject、累积内容和超时计时器。
   */
  private pendingQueries = new Map<string, {
    resolve: (content: string) => void;
    reject: (error: Error) => void;
    accumulated: string;
    timeoutTimer: ReturnType<typeof setTimeout>;
  }>();
  /** 查询 ID 自增计数器，确保每个查询有唯一标识 */
  private queryCounter = 0;
  /** 是否为主动关闭（true 时不触发自动重连） */
  private intentionalClose = false;
  /** SSE-like 消息缓冲区，用于处理跨帧分割的不完整行 */
  private messageBuffer = '';

  constructor(config: WebSocketClientConfig) {
    this.config = {
      url: config.url,
      maxReconnectAttempts: config.maxReconnectAttempts ?? 10,
      initialReconnectDelay: config.initialReconnectDelay ?? 1000,
      maxReconnectDelay: config.maxReconnectDelay ?? 30000,
      connectionTimeout: config.connectionTimeout ?? 10000,
      requestTimeout: config.requestTimeout ?? 30000,
    };
  }

  /**
   * 注册响应回调。
   * 必须在 connect() 之前调用，否则将无法接收事件通知。
   *
   * @param callbacks - 回调函数集合
   */
  onResponse(callbacks: WebSocketClientCallbacks): void {
    this.callbacks = callbacks;
  }

  /**
   * 获取当前连接状态。
   *
   * @returns 当前 WebSocketState
   */
  getState(): WebSocketState {
    return this.state;
  }

  /**
   * 建立 WebSocket 连接。
   *
   * 如果已处于 connected 或 connecting 状态，直接返回已 resolve 的 Promise。
   * 连接过程中会启动超时计时器，超时后自动 reject 并清理资源。
   *
   * @returns Promise，连接成功时 resolve
   * @throws 连接超时、WebSocket 构造失败或连接被拒绝时 reject
   */
  connect(): Promise<void> {
    if (this.ws && (this.state === 'connected' || this.state === 'connecting')) {
      return Promise.resolve();
    }

    this.intentionalClose = false;
    this.setState('connecting');

    return new Promise((resolve, reject) => {
      // 连接超时计时器
      this.clearConnectionTimer();
      this.connectionTimer = setTimeout(() => {
        if (this.state === 'connecting') {
          this.cleanupWebSocket();
          this.setState('disconnected');
          reject(new Error(
            `WebSocket connection timeout after ${this.config.connectionTimeout}ms / 连接超时`,
          ));
        }
      }, this.config.connectionTimeout);

      try {
        this.ws = new WebSocket(this.config.url);
      } catch (err) {
        this.clearConnectionTimer();
        const msg = err instanceof Error ? err.message : 'Failed to create WebSocket';
        this.setState('disconnected');
        reject(new Error(msg));
        return;
      }

      this.ws.onopen = () => {
        this.clearConnectionTimer();
        this.reconnectAttempts = 0;
        this.setState('connected');
        resolve();
      };

      this.ws.onmessage = (event: MessageEvent) => {
        this.handleMessage(event.data as string);
      };

      this.ws.onerror = () => {
        // onerror 本身不传递详细信息，错误细节在 onclose 中处理
      };

      this.ws.onclose = (event: CloseEvent) => {
        this.clearConnectionTimer();
        this.cleanupWebSocket();

        // 清理 SSE 消息缓冲区，防止残留数据影响下次连接
        this.messageBuffer = '';

        // 拒绝所有挂起的查询
        for (const [id, pending] of this.pendingQueries) {
          clearTimeout(pending.timeoutTimer);
          pending.reject(new Error(
            `Connection closed (code: ${event.code}) / 连接已关闭`,
          ));
          this.callbacks?.onError(id, `Connection closed (code: ${event.code})`);
        }
        this.pendingQueries.clear();

        if (!this.intentionalClose) {
          this.attemptReconnect();
        } else {
          this.setState('disconnected');
        }
      };
    });
  }

  /**
   * 发送查询并等待流式响应。
   *
   * 每个查询分配唯一 ID 和独立的超时计时器。
   * 服务端通过 chunk/done/error 类型的响应消息驱动 Promise 的完成。
   *
   * @param content - 查询内容
   * @param type - 查询类型（'query' | 'proof' | 'solve'，默认 'query'）
   * @returns Promise，包含完整响应文本
   * @throws WebSocket 未连接、查询超时或发送失败时 reject
   */
  sendQuery(content: string, type: 'query' | 'proof' | 'solve' = 'query'): Promise<string> {
    if (this.state !== 'connected' || !this.ws) {
      return Promise.reject(new Error('WebSocket not connected / WebSocket 未连接'));
    }

    const queryId = `q_${++this.queryCounter}_${Date.now()}`;
    const message: WSRequestMessage = {
      id: queryId,
      type,
      content,
      timestamp: Date.now(),
    };

    return new Promise((resolve, reject) => {
      // 请求超时计时器
      const timeoutTimer = setTimeout(() => {
        this.pendingQueries.delete(queryId);
        const errMsg = `Query timeout after ${this.config.requestTimeout}ms / 查询超时`;
        this.callbacks?.onError(queryId, errMsg);
        reject(new Error(errMsg));
      }, this.config.requestTimeout);

      this.pendingQueries.set(queryId, {
        resolve,
        reject,
        accumulated: '',
        timeoutTimer,
      });

      try {
        // 安全发送：通过局部变量避免 this.ws 被并发修改
        const ws = this.ws;
        if (ws) {
          ws.send(JSON.stringify(message));
        } else {
          // 理论上不会到达（上方已检查 state 和 ws），防御性处理
          clearTimeout(timeoutTimer);
          this.pendingQueries.delete(queryId);
          reject(new Error('WebSocket not connected / WebSocket 未连接'));
        }
      } catch (err) {
        clearTimeout(timeoutTimer);
        this.pendingQueries.delete(queryId);
        const msg = err instanceof Error ? err.message : 'Send failed';
        reject(new Error(`Failed to send query: ${msg}`));
      }
    });
  }

  /**
   * 断开 WebSocket 连接。
   *
   * 执行以下清理操作：
   * 1. 标记为主动关闭（阻止自动重连）
   * 2. 清除所有计时器（重连、连接超时）
   * 3. 拒绝所有挂起查询并通知回调
   * 4. 关闭 WebSocket 连接（code=1000）
   * 5. 清理事件监听器引用
   */
  disconnect(): void {
    this.intentionalClose = true;
    this.clearReconnectTimer();
    this.clearConnectionTimer();

    // 清理所有挂起查询
    for (const [, pending] of this.pendingQueries) {
      clearTimeout(pending.timeoutTimer);
      pending.reject(new Error('Client disconnected / 客户端已断开'));
    }
    this.pendingQueries.clear();

    // 清理 SSE 消息缓冲区
    this.messageBuffer = '';

    if (this.ws) {
      this.ws.close(1000, 'Client disconnect');
      this.cleanupWebSocket();
    }

    this.setState('disconnected');
  }

  /**
   * 处理收到的消息。
   *
   * 支持两种格式：
   * 1. JSON 帧: {"id":"...","type":"chunk","content":"..."}
   * 2. SSE-like 文本: "data: {...}\n\ndata: {...}\n"
   * 3. 纯文本: 作为 chunk 广播给第一个挂起查询
   *
   * @param data - 原始消息字符串
   */
  private handleMessage(data: string): void {
    // 尝试 SSE-like 格式解析
    if (data.startsWith('data: ') || data.includes('\ndata: ')) {
      this.handleSSEMessage(data);
      return;
    }

    // JSON 帧解析
    try {
      const message: WSResponseMessage = JSON.parse(data);
      this.processMessage(message);
    } catch {
      // 非 JSON 文本，作为纯文本 chunk 处理
      this.handlePlainText(data);
    }
  }

  /**
   * 处理 SSE-like 格式消息。
   * 将数据追加到缓冲区，按行分割后逐行解析 JSON。
   * 保留最后一个不完整的行到下次消息到达时处理。
   *
   * @param data - SSE 格式的原始数据
   */
  private handleSSEMessage(data: string): void {
    // 追加到缓冲区处理跨帧分割
    this.messageBuffer += data;

    const lines = this.messageBuffer.split('\n');
    this.messageBuffer = lines.pop() ?? '';

    for (const line of lines) {
      const trimmed = line.trim();
      if (!trimmed || !trimmed.startsWith('data: ')) continue;

      const jsonStr = trimmed.slice(6).trim();
      if (jsonStr === '[DONE]') continue;

      try {
        const message: WSResponseMessage = JSON.parse(jsonStr);
        this.processMessage(message);
      } catch {
        // 忽略无法解析的 SSE 行
      }
    }
  }

  /**
   * 处理纯文本消息（无 ID，广播给第一个挂起查询）。
   *
   * @param text - 纯文本内容
   */
  private handlePlainText(text: string): void {
    // 纯文本广播给第一个挂起查询
    for (const [id, pending] of this.pendingQueries) {
      pending.accumulated += text;
      this.callbacks?.onChunk(id, text);
      break; // 只发给第一个挂起查询
    }
  }

  /**
   * 处理解析后的消息对象。
   * 根据 type 字段分发到不同的处理逻辑：
   * - chunk: 追加文本到累积内容，触发 onChunk 回调
   * - done: 完成查询，触发 onComplete 回调并 resolve Promise
   * - error: 查询出错，触发 onError 回调并 reject Promise
   *
   * @param message - 已解析的响应消息
   */
  private processMessage(message: WSResponseMessage): void {
    const pending = this.pendingQueries.get(message.id);
    if (!pending) return;

    switch (message.type) {
      case 'chunk': {
        const chunk = message.content ?? '';
        pending.accumulated += chunk;
        this.callbacks?.onChunk(message.id, chunk);
        break;
      }
      case 'done': {
        clearTimeout(pending.timeoutTimer);
        this.pendingQueries.delete(message.id);
        this.callbacks?.onComplete(message.id, pending.accumulated);
        pending.resolve(pending.accumulated);
        break;
      }
      case 'error': {
        clearTimeout(pending.timeoutTimer);
        this.pendingQueries.delete(message.id);
        const errMsg = message.error ?? 'Unknown error';
        this.callbacks?.onError(message.id, errMsg);
        pending.reject(new Error(errMsg));
        break;
      }
    }
  }

  /**
   * 指数退避重连。
   *
   * 退避公式：delay = min(initial * 2^(attempt-1), max) + random(0, 500ms)
   * 当重连次数超过 maxReconnectAttempts 时停止重连，切换到 disconnected 状态。
   */
  private attemptReconnect(): void {
    if (this.reconnectAttempts >= this.config.maxReconnectAttempts) {
      this.setState('disconnected');
      return;
    }

    this.setState('reconnecting');
    this.reconnectAttempts += 1;

    // 指数退避: delay = min(initial * 2^(attempt-1), max) + jitter
    const baseDelay = this.config.initialReconnectDelay * Math.pow(2, this.reconnectAttempts - 1);
    const delay = Math.min(baseDelay, this.config.maxReconnectDelay);
    const jitter = Math.random() * 500; // 0-500ms 随机抖动，防止多个客户端同时重连

    this.clearReconnectTimer();
    this.reconnectTimer = setTimeout(() => {
      this.connect().catch(() => {
        // connect 失败会触发 onclose -> attemptReconnect，无需额外处理
      });
    }, delay + jitter);
  }

  /**
   * 更新连接状态并通知回调。
   *
   * @param newState - 新的连接状态
   */
  private setState(newState: WebSocketState): void {
    this.state = newState;
    this.callbacks?.onStateChange(newState);
  }

  /**
   * 清理 WebSocket 实例引用和所有事件监听器。
   * 将 onopen/onmessage/onerror/onclose 置为 null，帮助 GC 回收闭包引用。
   */
  private cleanupWebSocket(): void {
    if (this.ws) {
      this.ws.onopen = null;
      this.ws.onmessage = null;
      this.ws.onerror = null;
      this.ws.onclose = null;
      this.ws = null;
    }
  }

  /**
   * 清除连接超时计时器。
   */
  private clearConnectionTimer(): void {
    if (this.connectionTimer !== null) {
      clearTimeout(this.connectionTimer);
      this.connectionTimer = null;
    }
  }

  /**
   * 清除重连计时器。
   */
  private clearReconnectTimer(): void {
    if (this.reconnectTimer !== null) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
  }
}

/**
 * 创建 WebSocket AI 客户端的便捷工厂函数。
 *
 * @param config - WebSocket 客户端配置
 * @returns WebSocketAIClient 实例
 */
export function createWebSocketClient(config: WebSocketClientConfig): WebSocketAIClient {
  return new WebSocketAIClient(config);
}

// ================================================================
// 模拟流式输出 / Simulated Stream Response
// ================================================================

/**
 * 模拟流式输出。
 * 依次发送预处理事件 -> 文本分块事件 -> 完成事件。
 *
 * 内部使用 `if` 守卫安全地访问数组元素，替代不安全的 `!` 非空断言。
 *
 * 真实 API 流处理已就绪：
 * - SSE 流处理已在 createRealAPIClient() 中完整实现
 * - WebSocket 流处理已在 WebSocketAIClient 类中完整实现
 * - sendMessage() 统一入口已集成切换逻辑
 *
 * 当前 mock 实现用于开发/演示阶段，连接真实 API 后自动跳过。
 *
 * @param chunks - 响应文本分块数组
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
 * 执行流程：
 * 1. 检查 realAPIConfig 是否已配置
 * 2. 若已配置：创建客户端并尝试流式调用
 * 3. 若真实 API 失败：自动回退到模拟响应（发送回退通知事件）
 * 4. 若未配置：直接使用模拟响应
 *
 * @param content - 用户消息内容
 * @param provider - AI 提供者 ID（用于模拟回退时的响应选择）
 * @param callbacks - 回调集合
 *   - addStreamEvent: 添加流式事件到事件列表
 *   - incrementFilterCount: 增加对应过滤类型的计数
 *   - updateContent: 更新累积的响应内容
 * @returns 完整的响应文本
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
  // 优先尝试真实 API
  if (realAPIConfig !== null) {
    try {
      const client = createRealAPIClient(realAPIConfig);
      let accumulatedContent = '';

      // 发送真实 API 流开始事件
      callbacks.addStreamEvent({
        type: 0,
        description: 'Real API stream started / 真实 API 流开始',
        stepNumber: 1,
      });
      callbacks.incrementFilterCount('info');

      // 使用真实 API 客户端发送消息
      const result = await client.sendMessage(content, (chunk: string) => {
        accumulatedContent += chunk;
        callbacks.updateContent(accumulatedContent);
      });

      // 发送真实 API 完成事件
      callbacks.addStreamEvent({
        type: 15,
        description: 'Real API response complete / 真实 API 回复完成',
        stepNumber: 2,
      });
      callbacks.incrementFilterCount('result');

      return result;
    } catch {
      // 真实 API 失败，发送回退通知并继续使用模拟
      callbacks.addStreamEvent({
        type: 3,
        description:
          'Real API unavailable, falling back to simulated response / 真实 API 不可用，回退到模拟响应',
        stepNumber: 1,
      });
      callbacks.incrementFilterCount('info');
      // 继续执行下方模拟响应逻辑
    }
  }

  // 使用模拟响应
  const chunks = getResponseChunks(provider, content);
  await simulateStreamResponse(chunks, callbacks);
  return chunks.join('');
}
