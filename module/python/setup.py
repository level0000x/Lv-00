"""
Lv-00 Python 绑定安装配置脚本（最小化版本）。

注意：大部分元数据（name, version, description, classifiers, requires-python 等）
已在 python/pyproject.toml 的 [project] 段中声明和维护，此处仅保留 setup() 特有的配置。
如需修改版本号或依赖，请优先编辑 pyproject.toml。

版本：3.3.0
作者：Lv-00 开发团队
"""

from setuptools import setup

setup(
    # --- 以下为 setup() 特有配置，不与 pyproject.toml 重复 ---
    zip_safe=False,
    # install_requires 已在 pyproject.toml 中声明（当前无外部依赖）
    # extras_require 已在 pyproject.toml [project.optional-dependencies] 中声明，此处不再重复
)
