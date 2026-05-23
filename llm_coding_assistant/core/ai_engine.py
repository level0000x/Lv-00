"""
LLM编程辅助系统 - AI引擎模块
整合多个AI服务提供商的统一接口
"""
import os
import json
import logging
import asyncio
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

    def __init__(self):
        self.config = self._load_config()
        self.providers = self._init_providers()
        self.default_provider = self._get_default_provider()
        self.conversation_history: List[Dict[str, str]] = []
        self.max_history = 50
    
    def _load_config(self) -> Dict[str, Any]:
        """加载配置，包含API密钥和模型参数"""
        return {
            "dashscope_api_key": os.getenv("DASHSCOPE_API_KEY", ""),
            "openai_api_key": os.getenv("OPENAI_API_KEY", ""),
            "deepseek_api_key": os.getenv("DEEPSEEK_API_KEY", ""),
            "claude_api_key": os.getenv("ANTHROPIC_API_KEY", ""),
            "default_model": "qwen-coder-plus",
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
            AIProvider.GEMINI: False,
            AIProvider.LOCAL: True
        }
    
    def _get_default_provider(self) -> AIProvider:
        """获取默认提供商（优先使用有API密钥的）"""
        priority = [AIProvider.DASH_SCOPE, AIProvider.DEEPSEEK, AIProvider.OPENAI, AIProvider.CLAUDE]
        for provider in priority:
            if self.providers.get(provider, False):
                return provider
        return AIProvider.LOCAL
    
    async def chat(self, message: str, system: str = "",
                   stream: bool = False,
                   provider: Optional[AIProvider] = None) -> Union[str, AsyncGenerator[str, None]]:
        """
        聊天接口

        Args:
            message: 用户消息
            system: 系统提示词（默认使用编程助手提示词）
            stream: 是否流式输出（流式时返回 AsyncGenerator[str, None]，否则返回 str）
            provider: 指定提供商

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
                    return await self._chat_dashscope_stream(message, system)
                return await self._chat_dashscope(message, system)
            elif provider == AIProvider.OPENAI:
                if stream:
                    return await self._chat_openai_stream(message, system)
                return await self._chat_openai(message, system)
            elif provider == AIProvider.DEEPSEEK:
                if stream:
                    return await self._chat_deepseek_stream(message, system)
                return await self._chat_deepseek(message, system)
            elif provider == AIProvider.CLAUDE:
                return await self._chat_claude(message, system)
        except Exception as e:
            return await self._chat_fallback(message, system)
        
        return "暂不支持该提供商"
    
    async def _chat_dashscope(self, message: str, system: str) -> str:
        """阿里云通义千问 - 适合中文编程场景"""
        try:
            import dashscope
            # 使用实例属性存储 API key，避免修改全局状态
            if not hasattr(self, '_dashscope_client') or self._dashscope_api_key != self.config["dashscope_api_key"]:
                dashscope.api_key = self.config["dashscope_api_key"]
                self._dashscope_api_key = self.config["dashscope_api_key"]

            messages = []
            if system:
                messages.append({"role": "system", "content": system})
            # 添加历史记录
            messages.extend(self.conversation_history)
            messages.append({"role": "user", "content": message})

            timeout = self.config.get("request_timeout", 60.0)
            response = dashscope.Generation.call(
                model=self.config.get("default_model", "qwen-coder-plus"),
                messages=messages,
                max_tokens=self.config.get("max_tokens", 4000),
                temperature=self.config.get("temperature", 0.3),
                request_timeout=timeout
            )
            
            if response.status_code == 200:
                content = response.output.choices[0].message.content
                self._update_history(message, content)
                return content
            else:
                return f"API错误: {response.code} - {response.message}"
        except ImportError:
            return "请安装dashscope: pip install dashscope"
        except Exception as e:
            logger.error("通义千问调用失败: %s", e, exc_info=True)
            return "AI服务暂时不可用，请稍后重试"
    
    async def _chat_dashscope_stream(self, message: str, system: str) -> AsyncGenerator[str, None]:
        """阿里云通义千问 - 流式输出"""
        try:
            import dashscope
            # 使用实例属性存储 API key，避免修改全局状态
            if not hasattr(self, '_dashscope_api_key') or self._dashscope_api_key != self.config["dashscope_api_key"]:
                dashscope.api_key = self.config["dashscope_api_key"]
                self._dashscope_api_key = self.config["dashscope_api_key"]

            messages = []
            if system:
                messages.append({"role": "system", "content": system})
            messages.extend(self.conversation_history)
            messages.append({"role": "user", "content": message})

            timeout = self.config.get("request_timeout", 60.0)
            response = dashscope.Generation.call(
                model=self.config.get("default_model", "qwen-coder-plus"),
                messages=messages,
                max_tokens=self.config.get("max_tokens", 4000),
                temperature=self.config.get("temperature", 0.3),
                stream=True,
                request_timeout=timeout
            )
            
            full_content = ""
            for chunk in response:
                if chunk.status_code == 200:
                    content = chunk.output.choices[0].message.content
                    full_content += content
                    yield content
            
            self._update_history(message, full_content)
        except Exception as e:
            logger.error("通义千问流式调用失败: %s", e, exc_info=True)
            yield "AI服务暂时不可用，请稍后重试"
    
    async def _chat_openai(self, message: str, system: str) -> str:
        """OpenAI GPT"""
        try:
            from openai import OpenAI
            timeout = self.config.get("request_timeout", 60.0)
            client = OpenAI(
                api_key=self.config["openai_api_key"],
                timeout=timeout
            )
            
            messages = []
            if system:
                messages.append({"role": "system", "content": system})
            messages.extend(self.conversation_history)
            messages.append({"role": "user", "content": message})
            
            response = client.chat.completions.create(
                model="gpt-4",
                messages=messages,
                max_tokens=self.config.get("max_tokens", 4000),
                temperature=self.config.get("temperature", 0.3)
            )
            
            content = response.choices[0].message.content
            self._update_history(message, content)
            return content
        except ImportError:
            return "请安装openai: pip install openai"
        except Exception as e:
            logger.error("OpenAI调用失败: %s", e, exc_info=True)
            return "AI服务暂时不可用，请稍后重试"
    
    async def _chat_openai_stream(self, message: str, system: str) -> AsyncGenerator[str, None]:
        """OpenAI GPT - 流式输出"""
        try:
            from openai import OpenAI
            timeout = self.config.get("request_timeout", 60.0)
            client = OpenAI(
                api_key=self.config["openai_api_key"],
                timeout=timeout
            )
            
            messages = []
            if system:
                messages.append({"role": "system", "content": system})
            messages.extend(self.conversation_history)
            messages.append({"role": "user", "content": message})
            
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
            
            self._update_history(message, full_content)
        except Exception as e:
            logger.error("OpenAI流式调用失败: %s", e, exc_info=True)
            yield "AI服务暂时不可用，请稍后重试"
    
    async def _chat_deepseek(self, message: str, system: str) -> str:
        """DeepSeek - 专为代码优化"""
        try:
            import httpx
            
            messages = []
            if system:
                messages.append({"role": "system", "content": system})
            messages.extend(self.conversation_history)
            messages.append({"role": "user", "content": message})
            
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
                    self._update_history(message, content)
                    return content
                else:
                    return f"API错误: {response.status_code}"
        except ImportError:
            return "请安装httpx: pip install httpx"
        except Exception as e:
            logger.error("DeepSeek调用失败: %s", e, exc_info=True)
            return "AI服务暂时不可用，请稍后重试"
    
    async def _chat_deepseek_stream(self, message: str, system: str) -> AsyncGenerator[str, None]:
        """DeepSeek - 流式输出"""
        try:
            import httpx
            
            messages = []
            if system:
                messages.append({"role": "system", "content": system})
            messages.extend(self.conversation_history)
            messages.append({"role": "user", "content": message})
            
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
                    
                    self._update_history(message, full_content)
        except Exception as e:
            logger.error("DeepSeek流式调用失败: %s", e, exc_info=True)
            yield "AI服务暂时不可用，请稍后重试"
    
    async def _chat_claude(self, message: str, system: str) -> str:
        """Anthropic Claude"""
        try:
            import httpx
            
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
                        "messages": self.conversation_history + [{"role": "user", "content": message}]
                    },
                    timeout=60.0
                )
                
                if response.status_code == 200:
                    data = response.json()
                    content = data["content"][0]["text"]
                    self._update_history(message, content)
                    return content
                else:
                    return f"API错误: {response.status_code}"
        except Exception as e:
            logger.error("Claude调用失败: %s", e, exc_info=True)
            return "AI服务暂时不可用，请稍后重试"
    
    async def _chat_fallback(self, message: str, system: str) -> str:
        """本地简单处理（无API时）"""
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
    
    def _update_history(self, user_msg: str, assistant_msg: str):
        """更新对话历史"""
        self.conversation_history.append({"role": "user", "content": user_msg})
        self.conversation_history.append({"role": "assistant", "content": assistant_msg})
        
        # 保持历史记录在限制内
        if len(self.conversation_history) > self.max_history * 2:
            self.conversation_history = self.conversation_history[-self.max_history * 2:]
    
    def clear_history(self):
        """清空对话历史"""
        self.conversation_history.clear()
    
    def list_providers(self) -> List[str]:
        """列出可用的提供商"""
        return [p.value for p, available in self.providers.items() if available]
    
    def set_provider(self, provider: str) -> bool:
        """设置默认提供商"""
        try:
            p = AIProvider(provider)
            if self.providers.get(p, False):
                self.default_provider = p
                return True
            return False
        except ValueError:
            return False
    
    def get_status(self) -> Dict[str, Any]:
        """获取引擎状态"""
        return {
            "default_provider": self.default_provider.value,
            "available_providers": self.list_providers(),
            "conversation_length": len(self.conversation_history),
            "config": {
                "model": self.config.get("default_model"),
                "max_tokens": self.config.get("max_tokens"),
                "temperature": self.config.get("temperature")
            }
        }
