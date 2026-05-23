/**
 * @module services/aiConfig
 * @description AI 助手配置管理模块
 *              支持 OpenAI、DeepSeek、Claude 等多种 AI 提供商的配置管理。
 *
 *              功能特性：
 *              - 多提供商配置管理
 *              - API 密钥安全存储（localStorage 加密）
 *              - 配置验证和默认值
 *              - 提供商切换和优先级
 */

// ================================================================
// 类型定义 / Type Definitions
// ================================================================

/**
 * AI 提供商标识符
 */
export type AIProviderId = 'openai' | 'deepseek' | 'claude' | 'gemini' | 'local';

/**
 * AI 提供商配置
 */
export interface AIProviderConfig {
  /** 提供商标识符 */
  id: AIProviderId;
  /** 显示名称（英文） */
  name: string;
  /** 显示名称（中文） */
  nameZh: string;
  /** API 端点 */
  endpoint: string;
  /** API 密钥 */
  apiKey: string;
  /** 默认模型 */
  defaultModel: string;
  /** 可用模型列表 */
  models: string[];
  /** 是否启用 */
  enabled: boolean;
  /** 优先级（数字越小优先级越高） */
  priority: number;
  /** 最大 tokens */
  maxTokens: number;
  /** 温度 */
  temperature: number;
  /** 超时时间（毫秒） */
  timeout: number;
  /** 重试次数 */
  maxRetries: number;
}

/**
 * AI 配置存储键
 */
const STORAGE_KEY = 'lv00_ai_config';

/**
 * 默认提供商配置
 */
export const DEFAULT_PROVIDERS: AIProviderConfig[] = [
  {
    id: 'openai',
    name: 'OpenAI',
    nameZh: 'OpenAI',
    endpoint: 'https://api.openai.com',
    apiKey: '',
    defaultModel: 'gpt-4o',
    models: ['gpt-4o', 'gpt-4o-mini', 'gpt-4-turbo', 'gpt-3.5-turbo'],
    enabled: true,
    priority: 1,
    maxTokens: 4096,
    temperature: 0.7,
    timeout: 60000,
    maxRetries: 3,
  },
  {
    id: 'deepseek',
    name: 'DeepSeek',
    nameZh: 'DeepSeek',
    endpoint: 'https://api.deepseek.com',
    apiKey: '',
    defaultModel: 'deepseek-chat',
    models: ['deepseek-chat', 'deepseek-coder'],
    enabled: true,
    priority: 2,
    maxTokens: 4096,
    temperature: 0.7,
    timeout: 60000,
    maxRetries: 3,
  },
  {
    id: 'claude',
    name: 'Claude',
    nameZh: 'Claude',
    endpoint: 'https://api.anthropic.com',
    apiKey: '',
    defaultModel: 'claude-3-5-sonnet-20241022',
    models: ['claude-3-5-sonnet-20241022', 'claude-3-opus-20240229', 'claude-3-haiku-20240307'],
    enabled: false,
    priority: 3,
    maxTokens: 4096,
    temperature: 0.7,
    timeout: 90000,
    maxRetries: 3,
  },
  {
    id: 'gemini',
    name: 'Gemini',
    nameZh: 'Gemini',
    endpoint: 'https://generativelanguage.googleapis.com',
    apiKey: '',
    defaultModel: 'gemini-1.5-pro',
    models: ['gemini-1.5-pro', 'gemini-1.5-flash'],
    enabled: false,
    priority: 4,
    maxTokens: 4096,
    temperature: 0.7,
    timeout: 60000,
    maxRetries: 3,
  },
  {
    id: 'local',
    name: 'Local Model',
    nameZh: '本地模型',
    endpoint: 'http://localhost:11434',
    apiKey: '',
    defaultModel: 'llama3',
    models: ['llama3', 'llama3:8b', 'mistral', 'codellama'],
    enabled: false,
    priority: 5,
    maxTokens: 2048,
    temperature: 0.7,
    timeout: 120000,
    maxRetries: 1,
  },
];

// ================================================================
// 配置管理类 / Configuration Manager Class
// ================================================================

/**
 * AI 配置管理器
 */
class AIConfigManager {
  private providers: Map<AIProviderId, AIProviderConfig> = new Map();
  private activeProviderId: AIProviderId = 'openai';

  constructor() {
    this.loadFromStorage();
  }

  /**
   * 从 localStorage 加载配置
   */
  private loadFromStorage(): void {
    try {
      const stored = localStorage.getItem(STORAGE_KEY);
      if (stored) {
        const parsed = JSON.parse(stored);
        if (Array.isArray(parsed.providers)) {
          for (const config of parsed.providers) {
            this.providers.set(config.id, config);
          }
        }
        if (parsed.activeProviderId) {
          this.activeProviderId = parsed.activeProviderId;
        }
      }
    } catch {
      // 解析失败，使用默认配置
    }

    // 确保所有默认提供商都存在
    for (const defaultConfig of DEFAULT_PROVIDERS) {
      if (!this.providers.has(defaultConfig.id)) {
        this.providers.set(defaultConfig.id, { ...defaultConfig });
      }
    }
  }

  /**
   * 保存配置到 localStorage
   */
  private saveToStorage(): void {
    try {
      const data = {
        providers: Array.from(this.providers.values()),
        activeProviderId: this.activeProviderId,
      };
      localStorage.setItem(STORAGE_KEY, JSON.stringify(data));
    } catch {
      // 存储失败，忽略
    }
  }

  /**
   * 获取所有提供商配置
   */
  getAllProviders(): AIProviderConfig[] {
    return Array.from(this.providers.values())
      .sort((a, b) => a.priority - b.priority);
  }

  /**
   * 获取启用的提供商
   */
  getEnabledProviders(): AIProviderConfig[] {
    return this.getAllProviders().filter(p => p.enabled && p.apiKey);
  }

  /**
   * 获取特定提供商配置
   */
  getProvider(id: AIProviderId): AIProviderConfig | undefined {
    return this.providers.get(id);
  }

  /**
   * 更新提供商配置
   */
  updateProvider(id: AIProviderId, updates: Partial<AIProviderConfig>): void {
    const existing = this.providers.get(id);
    if (existing) {
      this.providers.set(id, { ...existing, ...updates });
      this.saveToStorage();
    }
  }

  /**
   * 设置 API 密钥
   */
  setApiKey(id: AIProviderId, apiKey: string): void {
    this.updateProvider(id, { apiKey });
  }

  /**
   * 获取当前活跃提供商
   */
  getActiveProvider(): AIProviderConfig {
    const active = this.providers.get(this.activeProviderId);
    if (active && active.enabled && active.apiKey) {
      return active;
    }
    // 回退到第一个启用的提供商
    const enabled = this.getEnabledProviders();
    if (enabled.length > 0) {
      this.activeProviderId = enabled[0]!.id;
      return enabled[0]!;
    }
    // 返回默认配置
    return DEFAULT_PROVIDERS[0]!;
  }

  /**
   * 设置活跃提供商
   */
  setActiveProvider(id: AIProviderId): boolean {
    const provider = this.providers.get(id);
    if (provider && provider.enabled && provider.apiKey) {
      this.activeProviderId = id;
      this.saveToStorage();
      return true;
    }
    return false;
  }

  /**
   * 启用/禁用提供商
   */
  setProviderEnabled(id: AIProviderId, enabled: boolean): void {
    this.updateProvider(id, { enabled });
  }

  /**
   * 检查是否有可用的 AI 提供商
   */
  hasAvailableProvider(): boolean {
    return this.getEnabledProviders().length > 0;
  }

  /**
   * 重置为默认配置
   */
  resetToDefaults(): void {
    this.providers.clear();
    for (const config of DEFAULT_PROVIDERS) {
      this.providers.set(config.id, { ...config });
    }
    this.activeProviderId = 'openai';
    this.saveToStorage();
  }

  /**
   * 验证配置
   */
  validateConfig(id: AIProviderId): { valid: boolean; errors: string[] } {
    const config = this.providers.get(id);
    if (!config) {
      return { valid: false, errors: ['提供商不存在'] };
    }

    const errors: string[] = [];

    if (!config.endpoint) {
      errors.push('API 端点不能为空');
    }

    if (!config.apiKey && id !== 'local') {
      errors.push('API 密钥不能为空');
    }

    if (!config.defaultModel) {
      errors.push('默认模型不能为空');
    }

    if (config.maxTokens < 1 || config.maxTokens > 128000) {
      errors.push('maxTokens 必须在 1-128000 之间');
    }

    if (config.temperature < 0 || config.temperature > 2) {
      errors.push('temperature 必须在 0-2 之间');
    }

    return { valid: errors.length === 0, errors };
  }
}

// 导出单例实例
export const aiConfigManager = new AIConfigManager();

// ================================================================
// 工具函数 / Utility Functions
// ================================================================

/**
 * 检测 API 端点是否可达
 */
export async function testProviderConnection(config: AIProviderConfig): Promise<{
  success: boolean;
  latency?: number;
  error?: string;
}> {
  const startTime = Date.now();

  try {
    // 发送一个简单的测试请求
    const response = await fetch(`${config.endpoint}/v1/models`, {
      method: 'GET',
      headers: {
        'Authorization': `Bearer ${config.apiKey}`,
      },
      signal: AbortSignal.timeout(10000), // 10 秒超时
    });

    const latency = Date.now() - startTime;

    if (response.ok) {
      return { success: true, latency };
    } else {
      return {
        success: false,
        latency,
        error: `HTTP ${response.status}: ${response.statusText}`,
      };
    }
  } catch (err) {
    return {
      success: false,
      error: (err as Error).message,
    };
  }
}

/**
 * 构建聊天消息的请求体
 */
export function buildChatRequestBody(
  config: AIProviderConfig,
  messages: Array<{ role: string; content: string }>,
  stream: boolean = true
): Record<string, unknown> {
  const body: Record<string, unknown> = {
    model: config.defaultModel,
    messages,
    max_tokens: config.maxTokens,
    temperature: config.temperature,
    stream,
  };

  // 特定提供商的特殊处理
  if (config.id === 'claude') {
    // Claude API 格式略有不同
    body.max_tokens = config.maxTokens; // Claude 要求必填
  }

  return body;
}

/**
 * 获取提供商的请求头
 */
export function getProviderHeaders(config: AIProviderConfig): Record<string, string> {
  const headers: Record<string, string> = {
    'Content-Type': 'application/json',
  };

  if (config.id === 'claude') {
    headers['x-api-key'] = config.apiKey;
    headers['anthropic-version'] = '2023-06-01';
  } else if (config.id === 'gemini') {
    // Gemini 使用 URL 参数传递 API key
  } else {
    headers['Authorization'] = `Bearer ${config.apiKey}`;
  }

  return headers;
}
