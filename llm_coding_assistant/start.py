#!/usr/bin/env python3
"""
Lv-00 UI编程辅助系统 - 快速启动脚本模块
============================================

本模块是 Lv-00 编程辅助系统的快速启动入口。
负责环境检查（Python 版本）并启动主程序交互式会话。

用法：
    python start.py

环境要求：
    - Python >= 3.8
    - 所有依赖模块已正确安装
"""

import os
import sys
import subprocess

def main() -> None:
    """
    主函数入口

    执行流程：
      1. 切换工作目录到脚本所在目录
      2. 检查 Python 版本是否 >= 3.8
      3. 导入并启动主程序
    """
    # 获取脚本所在目录
    script_dir: str = os.path.dirname(os.path.abspath(__file__))

    # 确保在正确的目录中运行
    os.chdir(script_dir)

    print("=" * 60)
    print("Lv-00 UI编程辅助系统")
    print("=" * 60)
    print()

    # 检查 Python 版本是否满足最低要求
    if sys.version_info < (3, 8):
        print("❌ 需要Python 3.8或更高版本")
        print(f"   当前版本: {sys.version}")
        sys.exit(1)

    # 导入并启动主程序
    try:
        from .main import main as run_assistant
        run_assistant()
    except ImportError:
        print("❌ 导入主模块失败")
        print("   请确保所有文件都已正确安装")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n\n已退出")
        sys.exit(0)

if __name__ == "__main__":
    main()
