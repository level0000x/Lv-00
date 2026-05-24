#!/usr/bin/env python3
"""
Lv-00 UI编程辅助系统 - 快速启动脚本模块
============================================

本模块是 Lv-00 编程辅助系统的快速启动入口。
负责环境检查（Python 版本）并启动主程序交互式会话。

用法：
    python start.py

环境要求：
    - Python >= 3.10
    - 所有依赖模块已正确安装
"""

import os
import sys
from pathlib import Path


def main() -> None:
    """
    主函数入口

    执行流程：
      1. 检查 Python 版本是否 >= 3.8
      2. 导入并启动主程序

    注意：
      本函数不修改全局工作目录（os.chdir），
      所有路径操作均使用绝对路径，避免副作用。
    """
    # 获取脚本所在目录的绝对路径（仅用于导入路径拼接，不改变工作目录）
    script_dir: Path = Path(__file__).parent.resolve()

    print("=" * 60)
    print("Lv-00 UI编程辅助系统")
    print("=" * 60)
    print()

    # 检查 Python 版本是否满足最低要求
    if sys.version_info < (3, 8):
        print("需要Python 3.8或更高版本")
        print(f"   当前版本: {sys.version}")
        sys.exit(1)

    # 将脚本所在目录加入模块搜索路径，确保后续导入能找到本地模块
    if str(script_dir) not in sys.path:
        sys.path.insert(0, str(script_dir))

    # 导入并启动主程序
    # 兼容两种运行方式：
    #   1. 作为包模块运行（python -m llm_coding_assistant.start）→ 使用相对导入
    #   2. 直接运行脚本（python start.py）→ 使用绝对导入
    try:
        from .main import main as run_assistant
        run_assistant()
    except ImportError:
        try:
            from main import main as run_assistant
            run_assistant()
        except ImportError as e:
            print("导入主模块失败")
            print(f"   错误详情: {e}")
            print("   请确保所有文件都已正确安装，且 main.py 位于同一目录下")
            sys.exit(1)
    except KeyboardInterrupt:
        print("\n\n已退出")
        sys.exit(0)


if __name__ == "__main__":
    main()
