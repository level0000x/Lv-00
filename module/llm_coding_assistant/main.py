"""
Lv-00 UI编程辅助系统 - 主程序模块
====================================

本模块是 Lv-00 编程辅助系统的交互式命令行入口。
整合知识库、代码模板和 AI 接口，提供统一的编程辅助界面。

核心类：
  - lvCodingAssistant: 主交互类，管理命令解析和功能调度

功能模块：
  - API 参考：查看图操作、约束、证明等模块的 API 文档
  - 代码模板：获取 WASM 绑定、Canvas 渲染器等代码模板
  - 代码片段：获取常用代码片段（C 内存管理、错误处理等）
  - 代码生成：根据任务描述生成绑定、渲染器、交互处理器等代码
  - 概念解释：解释归一化、合一、证明系统等核心概念
"""

import os
import json
import logging
from collections import deque
from typing import Dict, List, Any, Optional, Deque
from pathlib import Path

# 导入本地模块
from .lv_knowledge import (
    lvKnowledgeBase,
    lvPromptEngine,
    lvModule,
    get_lv_helper,
    CONCEPT_EXPLANATIONS,
    CODE_GUIDANCE,
)
from .templates import (
    CODE_TEMPLATES,
    CODE_SNIPPETS,
    API_QUICKREF,
    get_template,
    get_snippet,
    get_api_reference
)


class lvCodingAssistant:
    """
    Lv-00 UI编程辅助系统主类

    提供交互式的编程辅助界面，支持 API 查询、模板获取、
    代码生成、概念解释等功能。

    Attributes:
        kb: 知识库实例
        pe: 提示词引擎实例
        current_context: 当前文件上下文信息
        history: 命令历史记录列表
        config: 用户配置字典
        max_history: 命令历史记录上限，超出时自动裁剪最早记录
    """

    # ── 类级常量：命令历史记录上限 ──────────────────────────────────────
    DEFAULT_MAX_HISTORY = 1000

    # ── 类级常量：概念解释数据 ──────────────────────────────────────────
    # 从 lv_knowledge 模块导入，不再在 main.py 中内联定义
    CONCEPT_EXPLANATIONS: Dict[str, str] = {}

    # ── 类级常量：代码生成指导文本 ──────────────────────────────────────
    # 从 lv_knowledge 模块导入，不再在 main.py 中内联定义
    CODE_GUIDANCE: Dict[str, str] = {}

    def __init__(self) -> None:
        """初始化编程辅助系统"""
        self.kb: lvKnowledgeBase = lvKnowledgeBase()
        self.pe: lvPromptEngine = lvPromptEngine(self.kb)
        self.current_context: Dict[str, Any] = {}

        # 命令历史记录上限，超出时自动裁剪最早记录
        self.max_history: int = self.DEFAULT_MAX_HISTORY
        # 使用 deque 自动维护历史记录大小上限，避免手动切片
        self.history: Deque[str] = deque(maxlen=self.max_history)

        # 加载用户配置
        self.config: Dict[str, Any] = self._load_config()

    # 【修复 #9】配置文件 schema 定义
    # 定义配置文件中各字段的名称、期望类型和是否必填。
    # 加载配置时会根据此 schema 进行验证，确保配置文件的完整性。
    _CONFIG_SCHEMA: Dict[str, Dict[str, Any]] = {
        "ai_provider": {"type": str, "required": False, "default": "dashscope"},
        "model": {"type": str, "required": False, "default": "qwen-coder-plus"},
        "theme": {"type": str, "required": False, "default": "dark"},
        "max_history": {"type": int, "required": False, "default": 1000},
    }

    def _validate_config(self, config: Dict[str, Any]) -> Dict[str, Any]:
        """验证配置字典是否符合 schema 定义

        【修复 #9】对从 JSON 文件加载的配置进行基本的 schema 验证，
        检查必要字段是否存在、类型是否正确。对于缺失的可选字段，
        使用 schema 中定义的默认值填充。对于类型不匹配的字段，
        记录警告并使用默认值。

        Args:
            config: 从 JSON 文件加载的原始配置字典

        Returns:
            Dict[str, Any]: 验证后的配置字典（缺失字段已填充默认值）
        """
        validated = {}
        for key, schema in self._CONFIG_SCHEMA.items():
            expected_type = schema["type"]
            default_value = schema["default"]

            if key not in config:
                # 字段缺失，使用默认值
                validated[key] = default_value
                continue

            value = config[key]
            if not isinstance(value, expected_type):
                # 类型不匹配，记录警告并使用默认值
                logging.warning(
                    f"配置字段 '{key}' 类型错误: 期望 {expected_type.__name__}, "
                    f"实际 {type(value).__name__}，使用默认值 {default_value!r}"
                )
                validated[key] = default_value
            else:
                validated[key] = value

        # 检查配置中是否存在未定义的字段（可能是拼写错误或版本不匹配）
        unknown_keys = set(config.keys()) - set(self._CONFIG_SCHEMA.keys())
        if unknown_keys:
            logging.warning(f"配置文件中存在未识别的字段: {unknown_keys}，已忽略")

        return validated

    def _load_config(self) -> Dict[str, Any]:
        """
        从用户目录加载配置文件

        配置文件路径：~/.lv_coding_assistant/config.json
        如果文件不存在或解析失败，返回默认配置。

        【修复 #9】加载后通过 _validate_config() 进行 schema 验证，
        确保配置字段的类型正确、缺失字段有默认值。

        Returns:
            Dict[str, Any]: 验证后的配置字典
        """
        config_path: Path = Path.home() / ".lv_coding_assistant" / "config.json"
        if config_path.exists():
            try:
                with open(config_path, 'r', encoding='utf-8') as f:
                    raw_config = json.load(f)
                # 【修复 #9】对加载的配置进行 schema 验证
                return self._validate_config(raw_config)
            except json.JSONDecodeError as e:
                logging.warning(f"配置文件 JSON 解析失败: {e}，使用默认配置")
            except OSError as e:
                logging.warning(f"配置文件读取失败: {e}，使用默认配置")
        # 返回默认配置（经过 schema 验证，确保所有字段都有值）
        return self._validate_config({})

    def _save_config(self) -> None:
        """保存当前配置到用户目录的配置文件"""
        config_path: Path = Path.home() / ".lv_coding_assistant"
        try:
            config_path.mkdir(parents=True, exist_ok=True)
            with open(config_path / "config.json", 'w', encoding='utf-8') as f:
                json.dump(self.config, f, indent=2)
        except OSError as e:
            logging.error(f"配置保存失败: {e}")

    def _update_config(self, key: str, value: Any) -> None:
        """
        更新配置项并自动保存

        Args:
            key: 配置键名
            value: 配置值
        """
        self.config[key] = value
        self._save_config()

    def print_banner(self) -> None:
        """打印系统启动横幅"""
        banner = """
╔═══════════════════════════════════════════════════════════════╗
║                                                                       ║
║     Lv-00 UI编程辅助系统 v1.0                                        ║
║     Symbolic Geometry Engine - Code Assistant                         ║
║                                                                       ║
║     专为Lv-00几何元语言可视化界面优化的智能编程助手                   ║
║                                                                       ║
╚═══════════════════════════════════════════════════════════════╝
"""
        print(banner)

    def print_help(self) -> None:
        """打印帮助信息，列出所有可用命令（命令描述已中文化）"""
        help_text = """
可用命令:
───────────────────────────────────────────────────────────────────────
  help              显示本帮助信息
  api <模块>        查看 API 参考 (graph/constraint/proof/type/coord)
  template <名称>   获取代码模板 (wasm_basic/wasm_string/canvas/js_wrapper)
  snippet <名称>    获取代码片段 (c_memory/c_error/js_promise/css/theme)
  task <描述>       根据描述生成编程任务指南

  code <任务>       生成代码: binding/renderer/interaction/panel
  explain <概念>    解释核心概念: normalization/unification/proof

  context <文件>    设置当前文件上下文（用于代码生成参考）
  history           查看最近 20 条命令历史
  clear             清屏并重新显示横幅
  exit              退出程序
───────────────────────────────────────────────────────────────────────

使用提示:
  · 输入 'api graph' 查看图操作 API 文档
  · 输入 'template wasm_string' 获取 WASM 字符串绑定模板
  · 输入 'code binding' 获取绑定代码生成指导
  · 输入 'explain normalization' 了解归一化概念
"""
        print(help_text)

    def handle_api_command(self, module: str) -> None:
        """
        处理 API 查询命令

        不带参数时列出所有可用模块，带参数时显示指定模块的详细 API。

        Args:
            module: 模块名称（为空时列出所有模块）
        """
        if not module:
            # 列出所有可用模块
            print("\n可用API模块:")
            print("─" * 40)
            for name, apis in API_QUICKREF.items():
                print(f"\n【{name}】")
                for func, desc in apis.items():
                    print(f"  · {func:35s} - {desc}")
            return

        # 查询指定模块的 API
        apis: Optional[Dict[str, str]] = get_api_reference(module)
        if apis:
            print(f"\n【{module.upper()} API 参考】")
            print("─" * 40)
            for func, desc in apis.items():
                # 获取函数签名等详细信息
                details: Dict[str, str] = self.kb.api_signatures.get(func, {})
                print(f"\n• {func}")
                print(f"  描述: {desc}")
                if details:
                    print(f"  签名: {details.get('signature', 'N/A')}")
                    print(f"  返回: {details.get('returns', 'N/A')}")
        else:
            print(f"未找到模块: {module}")
            print("可用模块:", ", ".join(API_QUICKREF.keys()))

    def handle_template_command(self, name: str) -> None:
        """
        处理模板查询命令

        不带参数时列出所有可用模板，带参数时显示指定模板的完整代码。

        Args:
            name: 模板名称（为空时列出所有模板）
        """
        if not name:
            # 列出所有可用模板
            print("\n可用代码模板:")
            print("─" * 40)
            for key, tmpl in CODE_TEMPLATES.items():
                print(f"\n【{tmpl['name']}】({tmpl['language']})")
                print(f"  描述: {tmpl['description']}")
                print(f"  标签: {', '.join(tmpl.get('tags', []))}")
            return

        # 查询指定模板
        template: Optional[Dict[str, Any]] = CODE_TEMPLATES.get(name)
        if template:
            print(f"\n【{template['name']}】")
            print(f"语言: {template['language']}")
            print(f"描述: {template['description']}")
            print("\n代码:")
            print("─" * 60)
            print(template['template'])
        else:
            print(f"未找到模板: {name}")
            print("可用模板:", ", ".join(CODE_TEMPLATES.keys()))

    def handle_snippet_command(self, name: str) -> None:
        """
        处理代码片段查询命令

        不带参数时列出所有可用片段，带参数时显示指定片段的完整代码。

        Args:
            name: 片段名称（为空时列出所有片段）
        """
        if not name:
            # 列出所有可用片段
            print("\n可用代码片段:")
            print("─" * 40)
            for key, snippet in CODE_SNIPPETS.items():
                preview = snippet.split('\n')[0][:50]
                print(f"  · {key:20s} - {preview}...")
            return

        # 查询指定片段
        snippet: Optional[str] = CODE_SNIPPETS.get(name)
        if snippet:
            print(f"\n【{name}】")
            print("─" * 60)
            print(snippet)
        else:
            print(f"未找到片段: {name}")

    def handle_code_command(self, task: str) -> None:
        """
        处理代码生成命令

        根据任务类型显示对应的代码生成指导信息。
        指导文本来自类级常量 CODE_GUIDANCE。

        Args:
            task: 任务类型（binding/renderer/interaction/panel）
        """
        guidance: Optional[str] = self.CODE_GUIDANCE.get(task)
        if guidance:
            print(guidance)
        else:
            print(f"未知任务: {task}")
            print("可用任务:", ", ".join(self.CODE_GUIDANCE.keys()))

    def handle_explain_command(self, concept: str) -> None:
        """
        处理概念解释命令

        查找并显示指定概念的详细解释文档。
        解释内容来自类级常量 CONCEPT_EXPLANATIONS。

        Args:
            concept: 概念名称
        """
        if concept in self.CONCEPT_EXPLANATIONS:
            print(self.CONCEPT_EXPLANATIONS[concept])
        else:
            print(f"未找到概念: {concept}")
            print("可用概念:", ", ".join(self.CONCEPT_EXPLANATIONS.keys()))

    def handle_task_command(self, description: str) -> None:
        """
        处理编程任务命令

        使用提示词引擎根据任务描述生成编程任务指南。

        Args:
            description: 任务描述文本
        """
        prompt: str = self.pe.generate_coding_task_prompt(description)
        print("\n【编程任务指南】")
        print("─" * 60)
        print(prompt)

    def run_interactive(self) -> None:
        """
        运行交互式命令行会话

        持续读取用户输入并分发到对应的处理函数，
        直到用户输入 exit/quit/q 命令。
        """
        self.print_banner()
        self.print_help()

        while True:
            try:
                cmd: str = input("\nLv-00> ").strip()

                if not cmd:
                    continue

                # 记录命令到历史（deque 自动维护大小上限）
                self.history.append(cmd)

                # 解析命令：第一个词为命令名，其余为参数
                parts: List[str] = cmd.split(maxsplit=1)
                command: str = parts[0].lower()
                args: str = parts[1] if len(parts) > 1 else ""

                # 分发命令到对应的处理函数
                if command in ['exit', 'quit', 'q']:
                    print("\n再见! 祝编程愉快!\n")
                    break

                elif command in ['help', 'h', '?']:
                    self.print_help()

                elif command == 'api':
                    self.handle_api_command(args)

                elif command == 'template':
                    self.handle_template_command(args)

                elif command == 'snippet':
                    self.handle_snippet_command(args)

                elif command == 'code':
                    self.handle_code_command(args)

                elif command == 'explain':
                    self.handle_explain_command(args)

                elif command == 'task':
                    self.handle_task_command(args)

                elif command == 'history':
                    # 显示最近20条命令历史
                    print("\n命令历史:")
                    for i, h in enumerate(self.history[-20:], 1):
                        print(f"  {i}. {h}")

                elif command == 'clear':
                    # 清屏并重新显示横幅
                    # 使用 ANSI 转义序列替代 os.system，避免命令注入风险
                    print("\033[2J\033[H", end="", flush=True)
                    self.print_banner()

                elif command == 'context':
                    self.set_context(args)

                else:
                    # 未识别的命令，尝试作为通用查询处理
                    self.handle_task_command(cmd)

            except KeyboardInterrupt:
                print("\n\n使用 'exit' 命令退出")
            except (OSError, ValueError, RuntimeError) as e:
                print(f"\n错误: {e}")
                logging.debug(f"命令执行异常: {cmd} - {e}")

    def set_context(self, file_path: str) -> None:
        """
        设置当前文件上下文

        读取指定文件的内容作为后续操作的上下文参考。
        包含路径安全验证：仅允许读取允许的目录范围内的文件，
        防止路径遍历攻击。

        Args:
            file_path: 文件路径（相对路径或绝对路径）
        """
        if not file_path:
            print("请提供文件路径")
            return

        # 【修复 #8】使用 os.path.realpath() 解析路径，消除符号链接跳转风险
        # os.path.realpath() 会解析所有符号链接、`.` 和 `..`，返回规范化的绝对路径，
        # 比 Path.resolve() 更可靠（在所有 Python 版本中行为一致）。
        resolved_path = os.path.realpath(file_path)
        path = Path(resolved_path)

        # 安全检查：确保解析后的路径在允许的范围内
        # 【修复 #8】扩展允许的根目录列表，不仅限于 CWD，
        # 还包括用户主目录下的 .lv_coding_assistant 目录（配置文件所在位置）
        allowed_roots = [
            Path.cwd().resolve(),
            Path.home().resolve() / ".lv_coding_assistant",
        ]
        is_allowed = False
        for allowed_root in allowed_roots:
            try:
                path.relative_to(allowed_root)
                is_allowed = True
                break
            except ValueError:
                continue

        if not is_allowed:
            print(f"安全限制: 只能读取允许的目录内的文件")
            print(f"  允许的目录: {[str(r) for r in allowed_roots]}")
            print(f"  请求路径: {path}")
            return

        if not path.exists():
            print(f"文件不存在: {file_path}")
            return

        if not path.is_file():
            print(f"指定的路径不是文件: {file_path}")
            return

        # 读取文件内容作为上下文
        # 安全限制：拒绝超过 10MB 的文件，防止内存耗尽
        MAX_FILE_SIZE = 10 * 1024 * 1024  # 10MB
        try:
            file_size = path.stat().st_size
            if file_size > MAX_FILE_SIZE:
                print(f"文件过大 ({file_size / (1024*1024):.1f}MB)，最大允许 10MB")
                return

            with open(path, 'r', encoding='utf-8') as f:
                content: str = f.read()

            self.current_context = {
                "file": str(path),
                "name": path.name,
                "size": len(content),
                "preview": content[:500]
            }

            print(f"已设置上下文: {path.name}")
            print(f"  大小: {len(content)} 字节")
            print(f"  预览: {content[:100]}...")

        except UnicodeDecodeError as e:
            logging.error(f"文件编码错误: {file_path} - {e}")
            print(f"读取文件失败: 不支持的编码 ({e})")
        except OSError as e:
            logging.error(f"文件读取失败: {file_path} - {e}")
            print(f"读取文件失败: {e}")


def main() -> None:
    """主函数入口，创建辅助系统实例并启动交互式会话"""
    assistant: lvCodingAssistant = lvCodingAssistant()
    assistant.run_interactive()


if __name__ == "__main__":
    main()
