"""
LLM编程辅助系统 - AI引擎模块
整合多个AI服务提供商的统一接口
"""
import os
import json
import logging
import asyncio
import threading
from typing import Optional, Dict, Any, List, AsyncGenerator, Union
from enum import Enum

# 模块级日志
logger = logging.getLogger(__name__)

# 全局锁：保护 dashscope 全局 API key 的修改
# 注意：dashscope 库使用全局配置，多实例场景需要加锁保护
_dashscope_lock = threading.Lock()


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

    def _get_history(self, session_id: Optional[str] = None) -> List[Dict[str, str]]:
        """
        获取指定会话的对话历史

        Args:
            session_id: 会话ID，为None时返回默认的对话历史

        Returns:
            该会话的对话历史列表
        """
        if session_id is not None:
            return self.session_histories.setdefault(session_id, [])
        return self.conversation_history

    def _build_messages(self, system_prompt: str, message: str,
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
        messages.extend(self._get_history(session_id))
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
        except Exception as e:
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
            messages = self._build_messages(system, message, session_id)

            # 使用 asyncio.to_thread 避免同步SDK阻塞事件循环
            response = await asyncio.to_thread(self._dashscope_call_sync, messages, stream=False)

            if response.status_code == 200:
                content = response.output.choices[0].message.content
                self._update_history(message, content, session_id=session_id)
                return content
            else:
                return f"API错误: {response.code} - {response.message}"
        except ImportError:
            return "请安装dashscope: pip install dashscope"
        except Exception as e:
            logger.error("通义千问调用失败: %s", e, exc_info=True)
            return "AI服务暂时不可用，请稍后重试"

    async def _chat_dashscope_stream(self, message: str, system: str,
                                     session_id: Optional[str] = None) -> AsyncGenerator[str, None]:
        """
        阿里云通义千问 - 流式输出

        Args:
            message: 用户消息
            system: 系统提示词
            session_id: 会话ID，用于隔离对话历史

        Yields:
            流式文本片段
        """
        try:
            import dashscope
            # 不再修改 dashscope 全局 api_key，改为在每次调用时通过参数传递

            messages = self._build_messages(system, message, session_id)

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

            full_content = ""
            for chunk in response:
                if chunk.status_code == 200:
                    content = chunk.output.choices[0].message.content
                    full_content += content
                    yield content

            self._update_history(message, full_content, session_id=session_id)
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
        client = OpenAI(
            api_key=self.config["openai_api_key"],
            timeout=timeout
        )
        return client.chat.completions.create(
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
            messages = self._build_messages(system, message, session_id)

            # 使用 asyncio.to_thread 避免同步SDK阻塞事件循环
            response = await asyncio.to_thread(self._openai_call_sync, messages, stream=False)

            content = response.choices[0].message.content
            self._update_history(message, content, session_id=session_id)
            return content
        except ImportError:
            return "请安装openai: pip install openai"
        except Exception as e:
            logger.error("OpenAI调用失败: %s", e, exc_info=True)
            return "AI服务暂时不可用，请稍后重试"

    async def _chat_openai_stream(self, message: str, system: str,
                                  session_id: Optional[str] = None) -> AsyncGenerator[str, None]:
        """
        OpenAI GPT - 流式输出

        Args:
            message: 用户消息
            system: 系统提示词
            session_id: 会话ID，用于隔离对话历史

        Yields:
            流式文本片段
        """
        try:
            from openai import OpenAI
            timeout = self.config.get("request_timeout", 60.0)
            client = OpenAI(
                api_key=self.config["openai_api_key"],
                timeout=timeout
            )

            messages = self._build_messages(system, message, session_id)

            response = client.chat.completions.create(
                model="gpt-4",
                messages=messages,
                max_tokens=self.config.get("max_tokens", 4000),
                temperature=self.config.get("temperature", 0.3),
                stream=True
            )

            full_content = ""
            for chunk in response:
                if chunk.choices[0].delta.content:
                    content = chunk.choices[0].delta.content
                    full_content += content
                    yield content

            self._update_history(message, full_content, session_id=session_id)
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

            messages = self._build_messages(system, message, session_id)

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
                    self._update_history(message, content, session_id=session_id)
                    return content
                else:
                    return f"API错误: {response.status_code}"
        except ImportError:
            return "请安装httpx: pip install httpx"
        except Exception as e:
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

            messages = self._build_messages(system, message, session_id)

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
                                logger.debug(f"解析 SSE 数据块失败: {data} - {e}")

                    self._update_history(message, full_content, session_id=session_id)
        except Exception as e:
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

            history = self._get_history(session_id)

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
                    self._update_history(message, content, session_id=session_id)
                    return content
                else:
                    return f"API错误: {response.status_code}"
        except Exception as e:
            logger.error("Claude调用失败: %s", e, exc_info=True)
            return "AI服务暂时不可用，请稍后重试"

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

            messages = self._build_messages(system, message, session_id)

            # 将 OpenAI 格式消息转换为 Gemini 格式
            contents = []
            system_instruction = None
            for msg in messages:
                role = msg["role"]
                content = msg["content"]
                if role == "system":
                    system_instruction = content
                elif role == "user":
                    contents.append({
                        "role": "user",
                        "parts": [{"text": content}]
                    })
                elif role == "assistant":
                    contents.append({
                        "role": "model",
                        "parts": [{"text": content}]
                    })

            model = self.config.get("gemini_model", "gemini-pro")
            api_key = self.config["gemini_api_key"]
            url = (
                f"https://generativelanguage.googleapis.com/v1beta/models/"
                f"{model}:generateContent?key={api_key}"
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
                    headers={"Content-Type": "application/json"},
                    json=request_body,
                    timeout=self.config.get("request_timeout", 60.0)
                )

                if response.status_code == 200:
                    data = response.json()
                    candidates = data.get("candidates", [])
                    if candidates:
                        parts = candidates[0].get("content", {}).get("parts", [])
                        content = "".join(part.get("text", "") for part in parts)
                        self._update_history(message, content, session_id=session_id)
                        return content
                    else:
                        return "Gemini 返回空响应"
                else:
                    error_detail = response.text
                    logger.error("Gemini API错误 [%d]: %s", response.status_code, error_detail)
                    return f"Gemini API错误: {response.status_code}"
        except ImportError:
            return "请安装httpx: pip install httpx"
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
            import json as _json

            messages = self._build_messages(system, message, session_id)

            # 将 OpenAI 格式消息转换为 Gemini 格式
            contents = []
            system_instruction = None
            for msg in messages:
                role = msg["role"]
                content = msg["content"]
                if role == "system":
                    system_instruction = content
                elif role == "user":
                    contents.append({
                        "role": "user",
                        "parts": [{"text": content}]
                    })
                elif role == "assistant":
                    contents.append({
                        "role": "model",
                        "parts": [{"text": content}]
                    })

            model = self.config.get("gemini_model", "gemini-pro")
            api_key = self.config["gemini_api_key"]
            url = (
                f"https://generativelanguage.googleapis.com/v1beta/models/"
                f"{model}:streamGenerateContent?alt=sse&key={api_key}"
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
                    headers={"Content-Type": "application/json"},
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
                                chunk = _json.loads(data_str)
                                candidates = chunk.get("candidates", [])
                                if candidates:
                                    parts = candidates[0].get("content", {}).get("parts", [])
                                    for part in parts:
                                        text = part.get("text", "")
                                        if text:
                                            full_content += text
                                            yield text
                            except (_json.JSONDecodeError, KeyError, IndexError) as e:
                                logger.debug("解析 Gemini SSE 数据块失败: %s - %s", data_str[:100], e)

                    self._update_history(message, full_content, session_id=session_id)
        except Exception as e:
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

    def _update_history(self, user_msg: str, assistant_msg: str,
                        session_id: Optional[str] = None):
        """
        更新对话历史

        Args:
            user_msg: 用户消息
            assistant_msg: AI回复
            session_id: 会话ID，为None时更新默认的对话历史
        """
        history = self._get_history(session_id)
        history.append({"role": "user", "content": user_msg})
        history.append({"role": "assistant", "content": assistant_msg})

        # 保持历史记录在限制内
        if len(history) > self.max_history * 2:
            del history[:len(history) - self.max_history * 2]

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
