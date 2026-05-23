"""
Lv-00 UI编程辅助系统 - 主程序模块
====================================

本模块是 Lv-00 编程辅助系统的交互式命令行入口。
整合知识库、代码模板和 AI 接口，提供统一的编程辅助界面。

核心类：
  - Lv00CodingAssistant: 主交互类，管理命令解析和功能调度

功能模块：
  - API 参考：查看图操作、约束、证明等模块的 API 文档
  - 代码模板：获取 WASM 绑定、Canvas 渲染器等代码模板
  - 代码片段：获取常用代码片段（C 内存管理、错误处理等）
  - 代码生成：根据任务描述生成绑定、渲染器、交互处理器等代码
  - 概念解释：解释归一化、合一、证明系统等核心概念
"""

import os
import sys
import json
import logging
from typing import Dict, List, Any, Optional
from pathlib import Path

# 导入本地模块
from .lv00_knowledge import (
    Lv00KnowledgeBase,
    Lv00PromptEngine,
    Lv00Module,
    get_lv00_helper
)
from .templates import (
    CODE_TEMPLATES,
    CODE_SNIPPETS,
    API_QUICKREF,
    get_template,
    get_snippet,
    get_api_reference
)


class Lv00CodingAssistant:
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
    """

    def __init__(self) -> None:
        """初始化编程辅助系统"""
        self.kb: Lv00KnowledgeBase = Lv00KnowledgeBase()
        self.pe: Lv00PromptEngine = Lv00PromptEngine(self.kb)
        self.current_context: Dict[str, Any] = {}
        self.history: List[str] = []

        # 加载用户配置
        self.config: Dict[str, Any] = self._load_config()

    def _load_config(self) -> Dict[str, Any]:
        """
        从用户目录加载配置文件

        配置文件路径：~/.lv00_coding_assistant/config.json
        如果文件不存在或解析失败，返回默认配置。

        Returns:
            Dict[str, Any]: 配置字典
        """
        config_path: Path = Path.home() / ".lv00_coding_assistant" / "config.json"
        if config_path.exists():
            try:
                with open(config_path, 'r', encoding='utf-8') as f:
                    return json.load(f)
            except json.JSONDecodeError as e:
                logging.warning(f"配置文件 JSON 解析失败: {e}，使用默认配置")
            except OSError as e:
                logging.warning(f"配置文件读取失败: {e}，使用默认配置")
        return {
            "ai_provider": "dashscope",
            "model": "qwen-coder-plus",
            "theme": "dark"
        }

    def _save_config(self) -> None:
        """保存当前配置到用户目录的配置文件"""
        config_path: Path = Path.home() / ".lv00_coding_assistant"
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
        """打印帮助信息，列出所有可用命令"""
        help_text = """
可用命令:
───────────────────────────────────────────────────────────────────────
  help              显示帮助信息
  api <module>      查看API参考 (graph/constraint/proof/type/coord)
  template <name>   获取代码模板 (wasm_basic/wasm_string/canvas/js_wrapper)
  snippet <name>    获取代码片段 (c_memory/c_error/js_promise/css/theme)
  task <description> 生成编程任务指南

  code <task>       生成代码: binding/renderer/interaction/panel
  explain <concept> 解释概念: normalization/unification/proof

  context <file>    设置当前文件上下文
  history          查看命令历史
  clear            清屏
  exit             退出
───────────────────────────────────────────────────────────────────────

快捷提示:
  · 输入 'api graph' 查看图操作API
  · 输入 'template wasm_string' 获取WASM绑定模板
  · 输入 'code binding' 获取绑定代码生成指导
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
            for name: str, apis: Dict[str, str] in API_QUICKREF.items():
                print(f"\n【{name}】")
                for func: str, desc: str in apis.items():
                    print(f"  · {func:35s} - {desc}")
            return

        # 查询指定模块的 API
        apis: Optional[Dict[str, str]] = get_api_reference(module)
        if apis:
            print(f"\n【{module.upper()} API 参考】")
            print("─" * 40)
            for func: str, desc: str in apis.items():
                # 获取函数签名等详细信息
                details: Dict[str, str] = self.kb.api_signatures.get(func, {})
                print(f"\n• {func}")
                print(f"  描述: {desc}")
                if details:
                    print(f"  签名: {details.get('signature', 'N/A')}")
                    print(f"  返回: {details.get('returns', 'N/A')}")
        else:
            print(f"❌ 未找到模块: {module}")
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
            for key: str, tmpl: Dict[str, Any] in CODE_TEMPLATES.items():
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
            print(f"❌ 未找到模板: {name}")
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
            for key: str, snippet: str in CODE_SNIPPETS.items():
                preview: str = snippet.split('\n')[0][:50]
                print(f"  · {key:20s} - {preview}...")
            return

        # 查询指定片段
        snippet: Optional[str] = CODE_SNIPPETS.get(name)
        if snippet:
            print(f"\n【{name}】")
            print("─" * 60)
            print(snippet)
        else:
            print(f"❌ 未找到片段: {name}")

    def handle_code_command(self, task: str) -> None:
        """
        处理代码生成命令

        根据任务类型显示对应的代码生成指导信息。

        Args:
            task: 任务类型（binding/renderer/interaction/panel）
        """
        if task == "binding":
            print("""
【生成WebAssembly绑定代码】

请提供以下信息:
1. C函数名 (例如: graph_add_circle)
2. 函数描述
3. 参数列表

我将生成:
• C绑定代码 (EMSCRIPTEN_KEEPALIVE)
• JavaScript包装器
• 使用示例

提示: 使用 'api graph' 查看可用API
""")
        elif task == "renderer":
            print("""
【生成Canvas渲染器代码】

提供:
1. 渲染元素 (point/segment/region/mixed)
2. 特殊功能 (选中高亮/缩放/拖拽)

我将生成:
• GeometryRenderer类
• 坐标系转换
• 渲染方法
• 事件绑定
""")
        elif task == "interaction":
            print("""
【生成交互处理器代码】

提供:
1. 交互模式 (select/construct/analyze)
2. 工具列表
3. 特殊操作

我将生成:
• CanvasInteraction类
• 事件处理器
• 工具切换逻辑
• 撤销/重做支持
""")
        elif task == "panel":
            print("""
【生成UI面板代码】

提供:
1. 模块名称 (graph/block/proof/type/recurse/engine/debug)
2. 面板功能
3. 按钮列表

我将生成:
• HTML面板结构
• CSS样式
• JavaScript事件绑定
• API调用逻辑
""")
        else:
            print(f"❌ 未知任务: {task}")
            print("可用任务: binding, renderer, interaction, panel")

    def handle_explain_command(self, concept: str) -> None:
        """
        处理概念解释命令

        查找并显示指定概念的详细解释文档。

        Args:
            concept: 概念名称
        """
        # 内置概念解释字典
        explanations: Dict[str, str] = {
            "normalization": """
【图归一化 (Graph Normalization)】

归一化是Lv-00保证幂等性的核心机制。

工作流程:
1. 点合并: 合并坐标相同的点
   - 使用坐标哈希分组
   - 精确coord_equal()判等
   - 处理作用域冲突

2. 线段/区域合并: 合并端点相同的几何体

3. 稳定化: 拓扑排序固定顺序

关键API:
• graph_normalize(g, interactive)
  - interactive=true 时跨作用域合并需确认
  - 返回 NormalizationResult

注意事项:
• 归一化后图再次归一化不会变化(幂等性)
• 合并日志记录用于证明导航器回放
""",

            "unification": """
【合一检查 (Unification)】

合一检查是证明系统的核心。

执行流程:
1. 对构造图和命题图各自归一化
2. 展开命题中的模板为正则形式
3. 三层匹配:
   - 端口类型匹配
   - 约束类型匹配
   - 符号坐标精确匹配

关键API:
• proof_unify(construction, proposition, strict)

返回值:
• UNIFY_OK: 合一成功
• UNIFY_MISMATCH: 匹配失败
• UNIFY_INCOMPLETE: 部分匹配

严格边界:
• 不调用求解器判定语义等价
• 仅比较结构
""",

            "proof": """
【证明系统 (Proof System)】

命题结构:
• 输入/输出端口 (声明期望的证物)
• 虚线框几何模式 (等待填充)
• 前置/后置条件 (可选)

证明步骤:
1. 创建命题 (proposition_create)
2. 设置模式图 (proposition_set_pattern)
3. 执行构造
4. 合一检查 (proof_unify)
5. 成功则命题得证

信任颜色:
• 绿色: 全构造
• 蓝色: 待完成
• 黄色: 条件性不可构造
• 橙色: 非构造性依赖

关键API:
• proof_create_proposition()
• proof_unify()
• proof_step_forward()
• proof_step_backward()
""",

            "func_block": """
【函数块 (Function Block)】

函数块封装内部约束子图为可复用单元。

生命周期:
1. 打包 (Pack): 将子图封装为函数块
2. 例化 (Instantiate): 创建函数块实例
3. β-归约: 应用参数到形式参数

打包要求:
• 必须处理跨边界约束冲突
• 端口标记 (namespace_depth, parent_block_id)
• 变量捕获消解

确定性:
• VERIFIED: 静态分析确认唯一解
• PARTIALLY_VERIFIED: 未发现冲突
• NON_DETERMINISTIC: 出现多解

组合子:
• Compose: f∘g 组合
• Product: f×g 乘积

关键API:
• func_block_pack()
• func_block_instantiate()
• func_block_compose()
""",

            "trust": """
【信任颜色系统】

Lv-00使用颜色编码构造的可靠性:

🟢 TRUST_GREEN (0)
   全构造，无任何非常规依赖

🔵 TRUST_BLUE (1-3)
   • 未探索 (UNEXPLORED)
   • 资源受限 (RESOURCE)
   • 超出范围 (OUT_OF_RANGE)

🟢 TRUST_GREEN_VERIFIED (4)
   已证不可构造

🟡 TRUST_YELLOW (5)
   条件性不可构造

🟠 TRUST_ORANGE (6-7)
   • 非构造性oracle
   • 爆炸原理 (ex falso)

🟡 TRUST_AMBER (8)
   含数值假设

🟠 TRUST_DARK_ORANGE (9)
   非构造性+数值假设
"""
        }

        if concept in explanations:
            print(explanations[concept])
        else:
            print(f"❌ 未找到概念: {concept}")
            print("可用概念:", ", ".join(explanations.keys()))

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

                # 记录命令到历史
                self.history.append(cmd)

                # 解析命令：第一个词为命令名，其余为参数
                parts: List[str] = cmd.split(maxsplit=1)
                command: str = parts[0].lower()
                args: str = parts[1] if len(parts) > 1 else ""

                # 分发命令到对应的处理函数
                if command in ['exit', 'quit', 'q']:
                    print("\n再见! 祝编程愉快! 🎉\n")
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
                    for i: int, h: str in enumerate(self.history[-20:], 1):
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

        Args:
            file_path: 文件路径
        """
        if not file_path:
            print("❌ 请提供文件路径")
            return

        path: Path = Path(file_path)
        if not path.exists():
            print(f"❌ 文件不存在: {file_path}")
            return

        # 读取文件内容作为上下文
        # 安全限制：拒绝超过 10MB 的文件，防止内存耗尽
        MAX_FILE_SIZE = 10 * 1024 * 1024  # 10MB
        try:
            file_size = path.stat().st_size
            if file_size > MAX_FILE_SIZE:
                print(f"❌ 文件过大 ({file_size / (1024*1024):.1f}MB)，最大允许 10MB")
                return

            with open(path, 'r', encoding='utf-8') as f:
                content: str = f.read()

            self.current_context = {
                "file": str(path),
                "name": path.name,
                "size": len(content),
                "preview": content[:500]
            }

            print(f"✓ 已设置上下文: {path.name}")
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
    assistant: Lv00CodingAssistant = Lv00CodingAssistant()
    assistant.run_interactive()


if __name__ == "__main__":
    main()
