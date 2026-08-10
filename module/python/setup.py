from setuptools import setup, find_packages

_LONG_DESCRIPTION = (
    "Lv-00 几何元语言（Geometric Metalanguage）的 Python 绑定。\n\n"
    "本包通过 ctypes 加载预编译的 C 共享库（liblv.dll / liblv.so / liblv.dylib）"
    "实现高性能几何约束求解；若未找到共享库，则自动回退到纯 Python 模式。\n\n"
    "注意：C 共享库由 CMake 构建（需以 BUILD_SHARED_LIBS=ON 配置，静态库 .a/.lib "
    "无法被 ctypes 加载），本包不负责编译。构建产物可放置在包目录或项目的 "
    "build/build3/build4/bin/lib 目录，也可通过环境变量 lv_LIBRARY_PATH 指定完整路径。"
)

setup(
    name="lv",
    version="1.1.0",
    description="Lv-00 Geometric Metalanguage — Python Bindings",
    long_description=_LONG_DESCRIPTION,
    long_description_content_type="text/plain",
    author="Lv-00 Contributors",
    packages=find_packages(),
    package_data={
        "lv": ["*.py", "py.typed"],
        "lv.preset_blocks": ["*.py"],
        "lv.utils": ["*.py"],
    },
    python_requires=">=3.10",
    install_requires=[],
    extras_require={
        "test": ["pytest>=7.0"],
    },
)
