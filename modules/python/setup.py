from setuptools import setup, find_packages

setup(
    name="lv00",
    version="1.1.0",
    description="Lv-00 Geometric Metalanguage — Python Bindings",
    author="Lv-00 Contributors",
    packages=find_packages(),
    python_requires=">=3.10",
    install_requires=[],
    extras_require={
        "test": ["pytest>=7.0"],
    },
)
