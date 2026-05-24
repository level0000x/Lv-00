"""
LLM编程辅助系统 - AI引擎模块
整合多个AI服务提供商的统一接口

支持的AI提供商:
  - DashScope (阿里云通义千问) - 默认提供商，适合中文编程场景
  - OpenAI (GPT-4) - 通用编程辅助
  - DeepSeek - 专为代码优化设计
  - Anthropic Claude - 长上下文推理
  - Google Gemini - 多模态能力
  - Local (本地兜底) - 无API密钥时的预设回复

会话管理机制:
  - 支持按 session_id 隔离的对话历史，防止跨用户泄漏
  - LRU (Least Recently Used) 策略自动清理最久未访问的会话
  - 默认最大会话数 max_sessions=100，超出时淘汰最旧会话
  - 每个会话最大历史条数 max_history=50

流式输出:
  - DashScope、OpenAI、DeepSeek、Gemini 均支持流式输出
  - 流式调用通过 asyncio.to_thread() 包装同步SDK，避免阻塞事件循环
  - 流式方法返回 AsyncGenerator[str, None]，调用方可逐块消费

配置要求:
  - DASHSCOPE_API_KEY: 通义千问API密钥（推荐，默认提供商）
  - OPENAI_API_KEY: OpenAI API密钥
  - DEEPSEEK_API_KEY: DeepSeek API密钥
  - ANTHROPIC_API_KEY: Anthropic Claude API密钥
  - GEMINI_API_KEY: Google Gemini API密钥
  - 至少配置一个API密钥即可使用，未配置时回退到本地预设回复

核心类:
  - AIProvider: AI服务提供商枚举
  - AIEngine: 统一AI引擎，支持多提供商切换和会话管理
"""
import os
import json
import logging
import asyncio
import time
from typing import Optional, Dict, Any, List, AsyncGenerator, Union
from enum import Enum

# 模块级日志
logger = logging.getLogger(__name__)


class AIProvider(Enum):
    """AI服务提供商"""
    OPENAI = "openai"
    CLAUDE = "claude"
    GEMINI = "gemini"
    DEEPSEEK = "deepseek"
    DASH_SCOPE = "dashscope"
    LOCAL = "local"


class AIEngine:
    """
    统一AI引擎 - 支持多提供商
    专为编程辅助场景优化
    """

    # 编程辅助的系统提示词
    CODING_SYSTEM_PROMPT = """你是一个专业的编程助手，擅长：
1. 代码分析与审查 - 发现潜在bug、性能问题、安全漏洞
2. 代码优化建议 - 提供重构方案、性能优化、最佳实践
3. 代码生成 - 根据需求生成高质量、可维护的代码
4. 调试辅助 - 帮助分析错误日志、定位问题根源
5. 技术咨询 - 解答编程语言、框架、算法相关问题

回答时请：
- 提供清晰、实用的解决方案
- 包含代码示例时使用markdown代码块
- 解释"为什么"而不仅是"怎么做"
- 考虑代码的可读性、可维护性和性能"""

    def __init__(self) -> None:
        """
        初始化 AI 引擎。
        
        加载配置、初始化提供商状态，并设置默认提供商。
        """
        self.config = self._load_config()
        self.providers = self._init_providers()
        self.default_provider = self._get_default_provider()
        self.conversation_history: List[Dict[str, str]] = []
        self.max_history = 50
        # 按会话ID隔离的对话历史，防止跨用户泄漏
        self.session_histories: Dict[str, List[Dict[str, str]]] = {}
        # 会话最大数量限制，防止内存泄漏
        self.max_sessions = 100
        # 会话访问时间戳，用于 LRU 清理
        self._session_access_times: Dict[str, float] = {}
        # 上次定期清理会话的时间戳
        self._last_session_cleanup: float = time.time()
        # 定期清理会话的间隔（秒），默认 10 分钟
        self._session_cleanup_interval: float = 600.0
        # 不活跃会话的超时时间（秒），超过此时间未访问的会话将被清理，默认 30 分钟
        self._session_inactive_timeout: float = 1800.0
        # 保护共享会话状态的异步锁
        self._history_lock = asyncio.Lock()
        # OpenAI 客户端实例（延迟初始化，复用连接池）
        self._openai_client: Optional[Any] = None

    def _load_config(self) -> Dict[str, Any]:
        """加载配置，包含API密钥和模型参数"""
        return {
            "dashscope_api_key": os.getenv("DASHSCOPE_API_KEY", ""),
            "openai_api_key": os.getenv("OPENAI_API_KEY", ""),
            "deepseek_api_key": os.getenv("DEEPSEEK_API_KEY", ""),
            "claude_api_key": os.getenv("ANTHROPIC_API_KEY", ""),
            "gemini_api_key": os.getenv("GEMINI_API_KEY", ""),
            "default_model": "qwen-coder-plus",
            "gemini_model": os.getenv("GEMINI_MODEL", "gemini-pro"),
            "max_tokens": 4000,
            "temperature": 0.3,  # 编程场景使用较低温度
            "request_timeout": 60.0  # 请求超时时间（秒）
        }

    def _init_providers(self) -> Dict[AIProvider, bool]:
        """初始化提供商状态"""
        return {
            AIProvider.DASH_SCOPE: bool(self.config.get("dashscope_api_key")),
            AIProvider.OPENAI: bool(self.config.get("openai_api_key")),
            AIProvider.DEEPSEEK: bool(self.config.get("deepseek_api_key")),
            AIProvider.CLAUDE: bool(self.config.get("claude_api_key")),
            AIProvider.GEMINI: bool(self.config.get("gemini_api_key")),
            AIProvider.LOCAL: True
        }

    def _get_default_provider(self) -> AIProvider:
        """获取默认提供商（优先使用有API密钥的）"""
        priority = [AIProvider.DASH_SCOPE, AIProvider.DEEPSEEK, AIProvider.OPENAI, AIProvider.CLAUDE]
        for provider in priority:
            if self.providers.get(provider, False):
                return provider
        return AIProvider.LOCAL

    async def _get_history(self, session_id: Optional[str] = None) -> List[Dict[str, str]]:
        """
        获取指定会话的对话历史

        当会话数量超过 max_sessions 时，按 LRU 策略清理最久未访问的会话。

        Args:
            session_id: 会话ID，为None时返回默认的对话历史

        Returns:
            该会话的对话历史列表
        """
        async with self._history_lock:
            if session_id is not None:
                # 更新会话访问时间
                self._session_access_times[session_id] = time.time()
                # LRU 清理：当会话数超过上限时，移除最久未访问的会话
                if len(self.session_histories) > self.max_sessions:
                    self._evict_oldest_sessions()
                return self.session_histories.setdefault(session_id, [])
            return self.conversation_history

    def _evict_oldest_sessions(self) -> None:
        """
        按 LRU 策略清理最久未访问的会话，直到会话数不超过 max_sessions。

        同时执行定期清理：每隔 _session_cleanup_interval 秒，
        清理所有超过 _session_inactive_timeout 未访问的不活跃会话。

        每次清理最多移除 20% 的会话，避免频繁触发清理。
        """
        now = time.time()

        # 定期清理不活跃会话（基于超时时间）
        if now - self._last_session_cleanup >= self._session_cleanup_interval:
            inactive_cutoff = now - self._session_inactive_timeout
            inactive_sessions = [
                sid for sid, ts in self._session_access_times.items()
                if ts < inactive_cutoff
            ]
            if inactive_sessions:
                for sid in inactive_sessions:
                    self.session_histories.pop(sid, None)
                    self._session_access_times.pop(sid, None)
                logger.info(
                    "定期清理不活跃会话完成，移除 %d 个超时会话（超时阈值: %.0f 秒），剩余 %d 个",
                    len(inactive_sessions),
                    self._session_inactive_timeout,
                    len(self.session_histories),
                )
            self._last_session_cleanup = now

        if len(self.session_histories) <= self.max_sessions:
            return

        # 按访问时间排序，移除最旧的会话（一次最多清理 20%）
        evict_count = max(1, len(self.session_histories) // 5)
        sorted_sessions = sorted(
            self._session_access_times.items(),
            key=lambda item: item[1],
        )
        for session_id, _ in sorted_sessions[:evict_count]:
            self.session_histories.pop(session_id, None)
            self._session_access_times.pop(session_id, None)

        logger.info(
            "LRU 会话清理完成，移除 %d 个最久未访问的会话，剩余 %d 个",
            evict_count,
            len(self.session_histories),
        )

    async def _build_messages(self, system_prompt: str, message: str,
                        session_id: Optional[str] = None) -> List[Dict[str, str]]:
        """
        构建发送给AI提供商的消息列表（系统提示词 + 历史记录 + 用户消息）

        Args:
            system_prompt: 系统提示词
            message: 当前用户消息
            session_id: 会话ID，用于获取对应的对话历史

        Returns:
            完整的消息列表
        """
        messages = []
        if system_prompt:
            messages.append({"role": "system", "content": system_prompt})
        messages.extend(await self._get_history(session_id))
        messages.append({"role": "user", "content": message})
        return messages

    async def chat(self, message: str, system: str = "",
                   stream: bool = False,
                   provider: Optional[AIProvider] = None,
                   session_id: Optional[str] = None) -> Union[str, AsyncGenerator[str, None]]:
        """
        聊天接口

        Args:
            message: 用户消息
            system: 系统提示词（默认使用编程助手提示词）
            stream: 是否流式输出（流式时返回 AsyncGenerator[str, None]，否则返回 str）
            provider: 指定提供商
            session_id: 会话ID，用于隔离不同用户的对话历史。
                        为None时使用默认的 conversation_history（向后兼容）

        Returns:
            非流式模式返回 str，流式模式返回 AsyncGenerator[str, None]
        """
        provider = provider or self.default_provider
        system = system or self.CODING_SYSTEM_PROMPT

        if not self.providers.get(provider, False):
            return await self._chat_fallback(message, system)

        try:
            if provider == AIProvider.DASH_SCOPE:
                if stream:
                    return await self._chat_dashscope_stream(message, system, session_id=session_id)
                return await self._chat_dashscope(message, system, session_id=session_id)
            elif provider == AIProvider.OPENAI:
                if stream:
                    return await self._chat_openai_stream(message, system, session_id=session_id)
                return await self._chat_openai(message, system, session_id=session_id)
            elif provider == AIProvider.DEEPSEEK:
                if stream:
                    return await self._chat_deepseek_stream(message, system, session_id=session_id)
                return await self._chat_deepseek(message, system, session_id=session_id)
            elif provider == AIProvider.CLAUDE:
                return await self._chat_claude(message, system, session_id=session_id)
            elif provider == AIProvider.GEMINI:
                if stream:
                    return await self._chat_gemini_stream(message, system, session_id=session_id)
                return await self._chat_gemini(message, system, session_id=session_id)
        except (RuntimeError, ConnectionError, ValueError, KeyError, ImportError) as e:
            logger.error("AI提供商调用失败 [%s]: %s", provider, e, exc_info=True)
            return await self._chat_fallback(message, system)

        return "暂不支持该提供商"

    def _dashscope_call_sync(self, messages: List[Dict[str, str]], stream: bool = False):
        """
        同步调用通义千问API（用于在 asyncio.to_thread 中执行，避免阻塞事件循环）

        Args:
            messages: 消息列表
            stream: 是否流式输出

        Returns:
            API响应对象
        """
        import dashscope
        timeout = self.config.get("request_timeout", 60.0)
        # 通过 api_key 参数传递，避免修改 dashscope 全局状态
        return dashscope.Generation.call(
            model=self.config.get("default_model", "qwen-coder-plus"),
            messages=messages,
            max_tokens=self.config.get("max_tokens", 4000),
            temperature=self.config.get("temperature", 0.3),
            stream=stream,
            request_timeout=timeout,
            api_key=self.config["dashscope_api_key"]
        )

    async def _chat_dashscope(self, message: str, system: str,
                              session_id: Optional[str] = None) -> str:
        """
        阿里云通义千问 - 适合中文编程场景（非流式）

        Args:
            message: 用户消息
            system: 系统提示词
            session_id: 会话ID，用于隔离对话历史

        Returns:
            AI回复文本
        """
        try:
            import dashscope
            # 不再修改 dashscope 全局 api_key，改为在每次调用时通过参数传递
            messages = await self._build_messages(system, message, session_id)

            # 使用 asyncio.to_thread 避免同步SDK阻塞事件循环
            response = await asyncio.to_thread(self._dashscope_call_sync, messages, stream=False)

            if response.status_code == 200:
                content = response.output.choices[0].message.content
                await self._update_history(message, content, session_id=session_id)
                return content
            else:
                return f"API 请求失败（错误码 {response.code}）：{response.message}"
        except ImportError:
            raise ImportError("请安装dashscope: pip install dashscope")
        except (ConnectionError, TimeoutError, RuntimeError, KeyError, OSError) as e:
            logger.error("通义千问调用失败: %s", e, exc_info=True)
            return "AI服务暂时不可用，请稍后重试"

    def _dashscope_stream_collect_sync(self, messages: List[Dict[str, str]]) -> List[str]:
        """
        同步执行 DashScope 流式调用并收集所有文本片段（在线程中运行，避免阻塞事件循环）

        将整个同步迭代过程封装在线程内完成，包括网络 I/O 和响应解析，
        确保异步事件循环不会被阻塞。

        Args:
            messages: 消息列表

        Returns:
            文本片段列表（每个元素是一个流式 chunk 的内容）
        """
        import dashscope
        timeout = self.config.get("request_timeout", 60.0)
        response = dashscope.Generation.call(
            model=self.config.get("default_model", "qwen-coder-plus"),
            messages=messages,
            max_tokens=self.config.get("max_tokens", 4000),
            temperature=self.config.get("temperature", 0.3),
            stream=True,
            request_timeout=timeout,
            api_key=self.config["dashscope_api_key"]
        )
        chunks: List[str] = []
        for chunk in response:
            if chunk.status_code == 200:
                content = chunk.output.choices[0].message.content
                if content:
                    chunks.append(content)
        return chunks

    async def _chat_dashscope_stream(self, message: str, system: str,
                                     session_id: Optional[str] = None) -> AsyncGenerator[str, None]:
        """
        阿里云通义千问 - 流式输出

        使用 asyncio.to_thread() 将整个同步流式调用（包括迭代器遍历）
        包装在线程中执行，避免阻塞事件循环。线程完成收集后，
        在异步循环中逐个 yield 结果给调用方。

        Args:
            message: 用户消息
            system: 系统提示词
            session_id: 会话ID，用于隔离对话历史

        Yields:
            流式文本片段
        """
        try:
            messages = await self._build_messages(system, message, session_id)

            # 在线程中完成整个同步流式调用和迭代，避免阻塞事件循环
            chunks = await asyncio.to_thread(self._dashscope_stream_collect_sync, messages)

            full_content = ""
            for content in chunks:
                full_content += content
                yield content

            await self._update_history(message, full_content, session_id=session_id)
        except Exception as e:
            logger.error("通义千问流式调用失败: %s", e, exc_info=True)
            yield "AI服务暂时不可用，请稍后重试"

    def _openai_call_sync(self, messages: List[Dict[str, str]], stream: bool = False):
        """
        同步调用OpenAI API（用于在 asyncio.to_thread 中执行，避免阻塞事件循环）

        Args:
            messages: 消息列表
            stream: 是否流式输出

        Returns:
            API响应对象
        """
        from openai import OpenAI
        timeout = self.config.get("request_timeout", 60.0)
        # 复用客户端实例以利用连接池
        if self._openai_client is None:
            self._openai_client = OpenAI(
                api_key=self.config["openai_api_key"],
                timeout=timeout
            )
        return self._openai_client.chat.completions.create(
            model="gpt-4",
            messages=messages,
            max_tokens=self.config.get("max_tokens", 4000),
            temperature=self.config.get("temperature", 0.3),
            stream=stream
        )

    async def _chat_openai(self, message: str, system: str,
                           session_id: Optional[str] = None) -> str:
        """
        OpenAI GPT（非流式）

        Args:
            message: 用户消息
            system: 系统提示词
            session_id: 会话ID，用于隔离对话历史

        Returns:
            AI回复文本
        """
        try:
            messages = await self._build_messages(system, message, session_id)

            # 使用 asyncio.to_thread 避免同步SDK阻塞事件循环
            response = await asyncio.to_thread(self._openai_call_sync, messages, stream=False)

            content = response.choices[0].message.content
            await self._update_history(message, content, session_id=session_id)
            return content
        except ImportError:
            raise ImportError("请安装openai: pip install openai")
        except Exception as e:
            logger.error("OpenAI调用失败: %s", e, exc_info=True)
            return "AI服务暂时不可用，请稍后重试"

    def _openai_stream_collect_sync(self, messages: List[Dict[str, str]]) -> List[str]:
        """
        同步执行 OpenAI 流式调用并收集所有文本片段（在线程中运行，避免阻塞事件循环）

        将整个同步迭代过程封装在线程内完成，包括网络 I/O 和响应解析，
        确保异步事件循环不会被阻塞。

        Args:
            messages: 消息列表

        Returns:
            文本片段列表（每个元素是一个流式 chunk 的内容）
        """
        from openai import OpenAI
        timeout = self.config.get("request_timeout", 60.0)
        # 复用客户端实例以利用连接池
        if self._openai_client is None:
            self._openai_client = OpenAI(
                api_key=self.config["openai_api_key"],
                timeout=timeout
            )

        response = self._openai_client.chat.completions.create(
            model="gpt-4",
            messages=messages,
            max_tokens=self.config.get("max_tokens", 4000),
            temperature=self.config.get("temperature", 0.3),
            stream=True
        )

        chunks: List[str] = []
        for chunk in response:
            if chunk.choices[0].delta.content:
                content = chunk.choices[0].delta.content
                chunks.append(content)
        return chunks

    async def _chat_openai_stream(self, message: str, system: str,
                                  session_id: Optional[str] = None) -> AsyncGenerator[str, None]:
        """
        OpenAI GPT - 流式输出

        使用 asyncio.to_thread() 将整个同步流式调用（包括迭代器遍历）
        包装在线程中执行，避免阻塞事件循环。线程完成收集后，
        在异步循环中逐个 yield 结果给调用方。

        Args:
            message: 用户消息
            system: 系统提示词
            session_id: 会话ID，用于隔离对话历史

        Yields:
            流式文本片段
        """
        try:
            messages = await self._build_messages(system, message, session_id)

            # 在线程中完成整个同步流式调用和迭代，避免阻塞事件循环
            chunks = await asyncio.to_thread(self._openai_stream_collect_sync, messages)

            full_content = ""
            for content in chunks:
                full_content += content
                yield content

            await self._update_history(message, full_content, session_id=session_id)
        except Exception as e:
            logger.error("OpenAI流式调用失败: %s", e, exc_info=True)
            yield "AI服务暂时不可用，请稍后重试"

    async def _chat_deepseek(self, message: str, system: str,
                             session_id: Optional[str] = None) -> str:
        """
        DeepSeek - 专为代码优化（非流式，基于httpx异步客户端）

        Args:
            message: 用户消息
            system: 系统提示词
            session_id: 会话ID，用于隔离对话历史

        Returns:
            AI回复文本
        """
        try:
            import httpx

            messages = await self._build_messages(system, message, session_id)

            async with httpx.AsyncClient() as client:
                response = await client.post(
                    "https://api.deepseek.com/chat/completions",
                    headers={
                        "Authorization": f"Bearer {self.config['deepseek_api_key']}",
                        "Content-Type": "application/json"
                    },
                    json={
                        "model": "deepseek-coder",
                        "messages": messages,
                        "max_tokens": self.config.get("max_tokens", 4000),
                        "temperature": self.config.get("temperature", 0.3)
                    },
                    timeout=60.0
                )

                if response.status_code == 200:
                    data = response.json()
                    content = data["choices"][0]["message"]["content"]
                    await self._update_history(message, content, session_id=session_id)
                    return content
                else:
                    return f"API 请求失败（HTTP 状态码 {response.status_code}）"
        except ImportError:
            raise ImportError("请安装httpx: pip install httpx")
        except (ConnectionError, TimeoutError, RuntimeError, KeyError, OSError) as e:
            logger.error("DeepSeek调用失败: %s", e, exc_info=True)
            return "AI服务暂时不可用，请稍后重试"

    async def _chat_deepseek_stream(self, message: str, system: str,
                                    session_id: Optional[str] = None) -> AsyncGenerator[str, None]:
        """
        DeepSeek - 流式输出

        Args:
            message: 用户消息
            system: 系统提示词
            session_id: 会话ID，用于隔离对话历史

        Yields:
            流式文本片段
        """
        try:
            import httpx

            messages = await self._build_messages(system, message, session_id)

            async with httpx.AsyncClient() as client:
                async with client.stream(
                    "POST",
                    "https://api.deepseek.com/chat/completions",
                    headers={
                        "Authorization": f"Bearer {self.config['deepseek_api_key']}",
                        "Content-Type": "application/json"
                    },
                    json={
                        "model": "deepseek-coder",
                        "messages": messages,
                        "max_tokens": self.config.get("max_tokens", 4000),
                        "temperature": self.config.get("temperature", 0.3),
                        "stream": True
                    },
                    timeout=60.0
                ) as response:
                    full_content = ""
                    async for line in response.aiter_lines():
                        if line.startswith("data: "):
                            data = line[6:]
                            if data == "[DONE]":
                                break
                            try:
                                chunk = json.loads(data)
                                if chunk["choices"][0]["delta"].get("content"):
                                    content = chunk["choices"][0]["delta"]["content"]
                                    full_content += content
                                    yield content
                            except (json.JSONDecodeError, KeyError, IndexError) as e:
                                # SSE 流中可能出现不完整的 JSON 行，记录后跳过
                                logger.debug("解析 SSE 数据块失败: %s - %s", data, e)

                    await self._update_history(message, full_content, session_id=session_id)
        except (ConnectionError, TimeoutError, RuntimeError, KeyError, OSError) as e:
            logger.error("DeepSeek流式调用失败: %s", e, exc_info=True)
            yield "AI服务暂时不可用，请稍后重试"

    async def _chat_claude(self, message: str, system: str,
                           session_id: Optional[str] = None) -> str:
        """
        Anthropic Claude（基于httpx异步客户端）

        Args:
            message: 用户消息
            system: 系统提示词
            session_id: 会话ID，用于隔离对话历史

        Returns:
            AI回复文本
        """
        try:
            import httpx

            history = await self._get_history(session_id)

            async with httpx.AsyncClient() as client:
                response = await client.post(
                    "https://api.anthropic.com/v1/messages",
                    headers={
                        "x-api-key": self.config["claude_api_key"],
                        "Content-Type": "application/json",
                        "anthropic-version": "2023-06-01"
                    },
                    json={
                        "model": "claude-3-sonnet-20240229",
                        "max_tokens": self.config.get("max_tokens", 4000),
                        "temperature": self.config.get("temperature", 0.3),
                        "system": system,
                        "messages": history + [{"role": "user", "content": message}]
                    },
                    timeout=60.0
                )

                if response.status_code == 200:
                    data = response.json()
                    content = data["content"][0]["text"]
                    await self._update_history(message, content, session_id=session_id)
                    return content
                else:
                    return f"API 请求失败（HTTP 状态码 {response.status_code}）"
        except (ConnectionError, TimeoutError, RuntimeError, KeyError, OSError) as e:
            logger.error("Claude调用失败: %s", e, exc_info=True)
            return "AI服务暂时不可用，请稍后重试"

    def _convert_messages_to_gemini_format(self, messages: List[Dict[str, str]]) -> tuple:
        """将 OpenAI 格式的消息列表转换为 Gemini 格式

        Args:
            messages: OpenAI 格式的消息列表，每条消息包含 role 和 content 字段

        Returns:
            tuple: (contents, system_instruction)
                - contents: Gemini 格式的消息内容列表
                - system_instruction: 系统指令文本（如果有）
        """
        contents = []
        system_instruction = None
        for msg in messages:
            role = msg["role"]
            content = msg["content"]
            if role == "system":
                system_instruction = content
            elif role == "user":
                contents.append({"role": "user", "parts": [{"text": content}]})
            elif role == "assistant":
                contents.append({"role": "model", "parts": [{"text": content}]})
        return contents, system_instruction

    async def _chat_gemini(self, message: str, system: str,
                           session_id: Optional[str] = None) -> str:
        """
        Google Gemini - 同步调用（基于httpx异步客户端）

        Args:
            message: 用户消息
            system: 系统提示词
            session_id: 会话ID，用于隔离对话历史

        Returns:
            AI回复文本
        """
        try:
            import httpx

            messages = await self._build_messages(system, message, session_id)

            # 将 OpenAI 格式消息转换为 Gemini 格式
            contents, system_instruction = self._convert_messages_to_gemini_format(messages)

            model = self.config.get("gemini_model", "gemini-pro")
            api_key = self.config["gemini_api_key"]
            url = (
                f"https://generativelanguage.googleapis.com/v1beta/models/"
                f"{model}:generateContent"
            )

            request_body = {
                "contents": contents,
                "generationConfig": {
                    "maxOutputTokens": self.config.get("max_tokens", 4000),
                    "temperature": self.config.get("temperature", 0.3),
                },
            }
            if system_instruction:
                request_body["systemInstruction"] = {
                    "parts": [{"text": system_instruction}]
                }

            async with httpx.AsyncClient() as client:
                response = await client.post(
                    url,
                    headers={"Content-Type": "application/json", "x-goog-api-key": api_key},
                    json=request_body,
                    timeout=self.config.get("request_timeout", 60.0)
                )

                if response.status_code == 200:
                    data = response.json()
                    candidates = data.get("candidates", [])
                    if candidates:
                        parts = candidates[0].get("content", {}).get("parts", [])
                        content = "".join(part.get("text", "") for part in parts)
                        await self._update_history(message, content, session_id=session_id)
                        return content
                    else:
                        return "Gemini 返回空响应"
                else:
                    error_detail = response.text
                    logger.error("Gemini API错误 [%d]: %s", response.status_code, error_detail)
                    return f"Gemini API 请求失败（HTTP 状态码 {response.status_code}）"
        except ImportError:
            raise ImportError("请安装httpx: pip install httpx")
        except Exception as e:
            logger.error("Gemini调用失败: %s", e, exc_info=True)
            return "AI服务暂时不可用，请稍后重试"

    async def _chat_gemini_stream(self, message: str, system: str,
                                  session_id: Optional[str] = None) -> AsyncGenerator[str, None]:
        """
        Google Gemini - 流式输出

        Args:
            message: 用户消息
            system: 系统提示词
            session_id: 会话ID，用于隔离对话历史

        Yields:
            流式文本片段
        """
        try:
            import httpx

            messages = await self._build_messages(system, message, session_id)

            # 将 OpenAI 格式消息转换为 Gemini 格式
            contents, system_instruction = self._convert_messages_to_gemini_format(messages)

            model = self.config.get("gemini_model", "gemini-pro")
            api_key = self.config["gemini_api_key"]
            # 使用 streamGenerateContent 端点，API 密钥通过 HTTP Header 传递，避免暴露在 URL 中
            url = (
                f"https://generativelanguage.googleapis.com/v1beta/models/"
                f"{model}:streamGenerateContent?alt=sse"
            )

            request_body = {
                "contents": contents,
                "generationConfig": {
                    "maxOutputTokens": self.config.get("max_tokens", 4000),
                    "temperature": self.config.get("temperature", 0.3),
                },
            }
            if system_instruction:
                request_body["systemInstruction"] = {
                    "parts": [{"text": system_instruction}]
                }

            async with httpx.AsyncClient() as client:
                async with client.stream(
                    "POST",
                    url,
                    headers={"Content-Type": "application/json", "x-goog-api-key": api_key},
                    json=request_body,
                    timeout=self.config.get("request_timeout", 60.0)
                ) as response:
                    full_content = ""
                    async for line in response.aiter_lines():
                        if line.startswith("data: "):
                            data_str = line[6:]
                            if not data_str.strip():
                                continue
                            try:
                                chunk = json.loads(data_str)
                                candidates = chunk.get("candidates", [])
                                if candidates:
                                    parts = candidates[0].get("content", {}).get("parts", [])
                                    for part in parts:
                                        text = part.get("text", "")
                                        if text:
                                            full_content += text
                                            yield text
                            except (json.JSONDecodeError, KeyError, IndexError) as e:
                                logger.debug("解析 Gemini SSE 数据块失败: %s - %s", data_str[:100], e)

                    await self._update_history(message, full_content, session_id=session_id)
        except (ConnectionError, TimeoutError, RuntimeError, KeyError, OSError) as e:
            logger.error("Gemini流式调用失败: %s", e, exc_info=True)
            yield "AI服务暂时不可用，请稍后重试"

    async def _chat_fallback(self, message: str, system: str) -> str:
        """
        本地简单处理（无API时的兜底方案）

        Args:
            message: 用户消息
            system: 系统提示词（此兜底方案中未使用）

        Returns:
            本地预设的回复文本
        """
        message_lower = message.lower()

        responses = {
            "你好": "你好！我是编程助手。请描述你的编程问题，我会尽力帮助你。",
            "帮助": "我可以帮助你：\n1. 分析和优化代码\n2. 解答编程问题\n3. 生成代码示例\n4. 调试程序错误\n\n请直接输入你的问题或代码。",
            "代码": "请粘贴你需要帮助的代码，我会为你分析和提供建议。",
        }

        for keyword, response in responses.items():
            if keyword in message_lower:
                return response

        return f"收到你的消息。由于未配置API密钥，我的功能有限。\n\n请配置以下环境变量之一：\n- DASHSCOPE_API_KEY（推荐）\n- OPENAI_API_KEY\n- DEEPSEEK_API_KEY\n\n你的消息：{message[:100]}..."

    async def _update_history(self, user_msg: str, assistant_msg: str,
                        session_id: Optional[str] = None):
        """
        更新对话历史

        Args:
            user_msg: 用户消息
            assistant_msg: AI回复
            session_id: 会话ID，为None时更新默认的对话历史
        """
        async with self._history_lock:
            if session_id is not None:
                history = self.session_histories.setdefault(session_id, [])
            else:
                history = self.conversation_history
            history.append({"role": "user", "content": user_msg})
            history.append({"role": "assistant", "content": assistant_msg})

            # 保持历史记录在限制内
            if len(history) > self.max_history * 2:
                del history[:len(history) - self.max_history * 2]

            # LRU 会话清理：超过最大会话数时，统一调用 _evict_oldest_sessions() 清理
            if session_id is not None:
                self._session_access_times[session_id] = time.time()
                self._evict_oldest_sessions()

    def clear_history(self) -> None:
        """
        清空默认对话历史。
        
        删除所有已存储的对话消息，重置对话上下文。
        """
        self.conversation_history.clear()

    def clear_session_history(self, session_id: str) -> None:
        """
        清除指定会话的对话历史。

        Args:
            session_id: 要清除的会话ID
        """
        self.session_histories.pop(session_id, None)

    def clear_all_sessions(self) -> None:
        """
        清除所有会话历史。
        
        包括默认对话历史和所有 session 历史记录。
        """
        self.conversation_history.clear()
        self.session_histories.clear()

    def list_providers(self) -> List[str]:
        """列出可用的提供商"""
        return [p.value for p, available in self.providers.items() if available]

    def set_provider(self, provider: str) -> bool:
        """
        设置默认提供商

        Args:
            provider: 提供商名称字符串

        Returns:
            设置成功返回True，失败返回False
        """
        try:
            p = AIProvider(provider)
            if self.providers.get(p, False):
                self.default_provider = p
                return True
            return False
        except ValueError:
            return False

    def get_status(self) -> Dict[str, Any]:
        """
        获取引擎状态

        Returns:
            包含引擎状态信息的字典
        """
        return {
            "default_provider": self.default_provider.value,
            "available_providers": self.list_providers(),
            "conversation_length": len(self.conversation_history),
            "active_sessions": len(self.session_histories),
            "config": {
                "model": self.config.get("default_model"),
                "max_tokens": self.config.get("max_tokens"),
                "temperature": self.config.get("temperature")
            }
        }
