#!/usr/bin/env python3
"""
Lv-00 性能基准测试脚本模块
============================

本模块用于运行 Lv-00 项目的性能基准测试，收集性能数据并生成报告。
支持与历史数据比较以检测性能回归。

核心功能：
  - 运行基准测试并收集性能数据（执行时间、操作数、吞吐量）
  - 生成性能报告（JSON 格式和 Markdown 格式）
  - 与历史数据比较，检测性能回归（超过阈值时发出警告）
  - 输出适合 CI/CD 流水线的格式

核心类：
  - BenchmarkRunner: 基准测试运行器（执行测试、解析结果、生成报告）
  - PerformanceComparator: 性能比较器（对比当前结果与基线数据）

用法：
    python benchmark.py [--compare-with FILE] [--output-dir DIR] [--fail-on-regression]

环境变量：
    LV00_DEBUG=1: 启用调试输出
"""

import argparse
import json
import os
import re
import subprocess
import sys
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

# 调试模式开关：通过环境变量 LV00_DEBUG=1 或 --debug 参数启用，默认关闭
_DEBUG = os.environ.get('LV00_DEBUG', '0') == '1'

# 默认配置常量
_DEFAULT_BUILD_DIR = "build"
_DEFAULT_OUTPUT_DIR = "benchmark_results"
_DEFAULT_TIMEOUT = 300  # 5分钟超时（秒）

# 性能回归阈值：超过此比例视为回归
_REGRESSION_THRESHOLD = 1.20


class BenchmarkRunner:
    """
    基准测试运行器

    负责执行基准测试可执行文件、解析输出、计算统计数据和生成报告。

    Attributes:
        build_dir: 构建目录路径（存放可执行文件）
        results: 测试结果字典 {测试名称: 结果数据}
    """

    def __init__(self, build_dir: str = _DEFAULT_BUILD_DIR, debug: bool = _DEBUG) -> None:
        """
        初始化基准测试运行器

        Args:
            build_dir: 构建目录路径，默认为 "build"
            debug: 是否启用调试输出，默认读取环境变量
        """
        self.build_dir: str = build_dir
        self.results: Dict[str, Dict[str, Any]] = {}
        self._debug: bool = debug
        
    def run_benchmark(self, test_name: str, executable: str) -> Optional[Dict[str, Any]]:
        """
        运行单个基准测试并解析结果

        Args:
            test_name: 测试名称
            executable: 可执行文件名（相对于 build_dir）

        Returns:
            解析后的测试结果字典，失败时返回 None
        """
        executable_path = os.path.join(self.build_dir, executable)
        if not os.path.exists(executable_path):
            if self._debug:
                print(f"警告: 找不到可执行文件 {executable_path}")
            return None

        exec_path = Path(executable_path).resolve()
        build_path = Path(self.build_dir).resolve()
        if not str(exec_path).startswith(str(build_path)):
            raise ValueError(f"Executable path {exec_path} is outside build directory {build_path}")

        try:
            result = subprocess.run(
                [executable_path],
                capture_output=True,
                text=True,
                timeout=_DEFAULT_TIMEOUT
            )
            
            if result.returncode != 0:
                if self._debug:
                    print(f"错误: {test_name} 返回非零退出码")
                    print(result.stderr)
                return None
                
            return self._parse_output(test_name, result.stdout)
            
        except subprocess.TimeoutExpired:
            if self._debug:
                print(f"错误: {test_name} 超时")
            return None
        except Exception as e:
            if self._debug:
                print(f"错误: 运行 {test_name} 时发生异常: {e}")
            return None
    
    def _parse_output(self, test_name: str, output: str) -> Dict[str, Any]:
        """
        解析基准测试的标准输出

        从输出中提取每个测试用例的名称、耗时、操作数和单位，
        并计算汇总统计信息。

        Args:
            test_name: 测试名称
            output: 可执行文件的标准输出文本

        Returns:
            包含测试用例列表和汇总统计的字典
        """
        results = {
            "name": test_name,
            "timestamp": datetime.now().isoformat(),
            "tests": []
        }
        
        # 解析测试输出中的性能数据
        # 格式: "  Test Name                  123.45 ms (1000 ops)"
        pattern = r"\s+(.+?)\s{2,}([\d.]+)\s+ms\s+\((\d+)\s+(\w+)\)"
        
        for line in output.split('\n'):
            match = re.search(pattern, line)
            if match:
                test_case = match.group(1).strip()
                time_ms = float(match.group(2))
                count = int(match.group(3))
                unit = match.group(4)
                
                results["tests"].append({
                    "name": test_case,
                    "time_ms": time_ms,
                    "count": count,
                    "unit": unit,
                    "ops_per_sec": count / (time_ms / 1000) if time_ms > 0 else 0
                })
        
        # 计算汇总统计
        if results["tests"]:
            times = [t["time_ms"] for t in results["tests"]]
            results["summary"] = {
                "total_tests": len(results["tests"]),
                "total_time_ms": sum(times),
                "avg_time_ms": sum(times) / len(times),
                "min_time_ms": min(times),
                "max_time_ms": max(times)
            }
        
        return results
    
    def run_all_benchmarks(self) -> Dict[str, Dict[str, Any]]:
        """
        运行所有已注册的基准测试

        测试覆盖 Lv-00 核心模块的性能表现：
          - 有理数算术（benchmark）
          - 约束图操作（benchmark_constraint）
          - 图归一化（benchmark_normalize）
          - 符号坐标操作（benchmark_symbolic_coord）
          - 公理包模板匹配（benchmark_axiom）
          - 函数块打包/实例化（benchmark_func_block）

        Returns:
            所有测试结果的字典 {测试名称: 结果数据}
        """
        benchmarks = [
            ("benchmark", "test_benchmark.exe" if sys.platform == "win32" else "test_benchmark"),
            ("rational_arithmetic", "test_benchmark_rational.exe" if sys.platform == "win32" else "test_benchmark_rational"),
            ("constraint_graph", "test_benchmark_constraint.exe" if sys.platform == "win32" else "test_benchmark_constraint"),
            ("normalization", "test_benchmark_normalize.exe" if sys.platform == "win32" else "test_benchmark_normalize"),
            ("symbolic_coord", "test_benchmark_symbolic_coord.exe" if sys.platform == "win32" else "test_benchmark_symbolic_coord"),
            ("axiom_matching", "test_benchmark_axiom.exe" if sys.platform == "win32" else "test_benchmark_axiom"),
            ("func_block", "test_benchmark_func_block.exe" if sys.platform == "win32" else "test_benchmark_func_block"),
        ]
        
        if self._debug:
            print("=" * 60)
            print("Lv-00 性能基准测试")
            print("=" * 60)
            print()
        
        for name, executable in benchmarks:
            if self._debug:
                print(f"运行 {name}...")
            result = self.run_benchmark(name, executable)
            if result:
                self.results[name] = result
                if self._debug:
                    print(f"  v 完成 - {len(result.get('tests', []))} 个测试用例")
            else:
                if self._debug:
                    print(f"  x 失败")
            if self._debug:
                print()
        
        return self.results
    
    def generate_report(self, output_dir: str = _DEFAULT_OUTPUT_DIR) -> str:
        """
        生成性能报告（JSON + Markdown 格式）

        在指定目录下创建带时间戳的报告文件。

        Args:
            output_dir: 输出目录路径

        Returns:
            str: 生成的 JSON 报告文件路径
        """
        os.makedirs(output_dir, exist_ok=True)
        
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        json_file = os.path.join(output_dir, f"benchmark_{timestamp}.json")
        md_file = os.path.join(output_dir, f"benchmark_{timestamp}.md")
        
        # 保存 JSON 数据
        with open(json_file, 'w', encoding='utf-8') as f:
            json.dump(self.results, f, indent=2)
        
        # 生成 Markdown 报告
        self._generate_markdown(md_file)
        
        if self._debug:
            print(f"报告已保存:")
            print(f"  JSON: {json_file}")
            print(f"  Markdown: {md_file}")
        
        return json_file
    
    def _generate_markdown(self, filename: str) -> None:
        """
        生成 Markdown 格式的性能报告

        Args:
            filename: 输出文件路径
        """
        with open(filename, 'w', encoding='utf-8') as f:
            f.write("# Lv-00 性能基准测试报告\n\n")
            f.write(f"生成时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
            
            for benchmark_name, data in self.results.items():
                f.write(f"## {benchmark_name}\n\n")
                
                if "summary" in data:
                    summary = data["summary"]
                    f.write("### 汇总\n\n")
                    f.write(f"- 总测试数: {summary['total_tests']}\n")
                    f.write(f"- 总耗时: {summary['total_time_ms']:.2f} ms\n")
                    f.write(f"- 平均耗时: {summary['avg_time_ms']:.2f} ms\n")
                    f.write(f"- 最小耗时: {summary['min_time_ms']:.2f} ms\n")
                    f.write(f"- 最大耗时: {summary['max_time_ms']:.2f} ms\n")
                    f.write("\n")
                
                if "tests" in data and data["tests"]:
                    f.write("### 详细结果\n\n")
                    f.write("| 测试名称 | 耗时 (ms) | 操作数 | 单位 | 每秒操作数 |\n")
                    f.write("|---------|----------|--------|------|-----------|\n")
                    
                    for test in data["tests"]:
                        ops_str = f"{test['ops_per_sec']:,.0f}" if test['ops_per_sec'] > 0 else "N/A"
                        f.write(f"| {test['name']} | {test['time_ms']:.2f} | "
                               f"{test['count']} | {test['unit']} | {ops_str} |\n")
                    
                    f.write("\n")


class PerformanceComparator:
    """
    性能比较器

    将当前基准测试结果与历史基线数据比较，
    检测性能回归（当前耗时超过基线的指定比例）。

    Attributes:
        baseline: 基线数据（历史测试结果）
    """

    def __init__(self, baseline_file: str) -> None:
        """
        初始化性能比较器

        Args:
            baseline_file: 基线 JSON 结果文件路径
        """
        self.baseline: Dict[str, Any] = self._load_results(baseline_file)
    
    @staticmethod
    def _load_results(filename: str) -> Dict[str, Any]:
        """
        加载历史结果文件

        Args:
            filename: JSON 结果文件路径

        Returns:
            Dict[str, Any]: 加载的历史结果数据

        Raises:
            FileNotFoundError: 文件不存在
            json.JSONDecodeError: JSON 解析失败
        """
        if not os.path.exists(filename):
            raise FileNotFoundError(f"基线文件不存在: {filename}")
        with open(filename, 'r', encoding='utf-8') as f:
            return json.load(f)
    
    def compare(self, current_results: Dict[str, Any]) -> Tuple[bool, List[str]]:
        """
        比较当前结果与基线数据

        对比每个测试用例和汇总时间的性能变化，
        超过回归阈值（_REGRESSION_THRESHOLD）的记录为回归。

        Args:
            current_results: 当前测试结果字典

        Returns:
            Tuple[bool, List[str]]: (是否有回归, 回归详情列表)
        """
        regressions = []
        has_regression = False
        
        for benchmark_name, current_data in current_results.items():
            if benchmark_name not in self.baseline:
                continue
                
            baseline_data = self.baseline[benchmark_name]
            
            # 比较汇总时间
            if "summary" in current_data and "summary" in baseline_data:
                current_total = current_data["summary"]["total_time_ms"]
                baseline_total = baseline_data["summary"]["total_time_ms"]
                
                ratio = current_total / baseline_total if baseline_total > 0 else 0
                
                if ratio > _REGRESSION_THRESHOLD:
                    has_regression = True
                    regressions.append(
                        f"{benchmark_name}: 总耗时从 {baseline_total:.2f}ms "
                        f"增加到 {current_total:.2f}ms (慢 {ratio:.2f}x)"
                    )
            
            # 比较单个测试
            current_tests = {t["name"]: t for t in current_data.get("tests", [])}
            baseline_tests = {t["name"]: t for t in baseline_data.get("tests", [])}
            
            for test_name, current_test in current_tests.items():
                if test_name not in baseline_tests:
                    continue
                    
                baseline_test = baseline_tests[test_name]
                current_time = current_test["time_ms"]
                baseline_time = baseline_test["time_ms"]
                
                ratio = current_time / baseline_time if baseline_time > 0 else 0
                
                if ratio > _REGRESSION_THRESHOLD:
                    has_regression = True
                    regressions.append(
                        f"{benchmark_name}/{test_name}: 从 {baseline_time:.2f}ms "
                        f"增加到 {current_time:.2f}ms (慢 {ratio:.2f}x)"
                    )
        
        return has_regression, regressions


def main() -> None:
    """
    主函数入口

    解析命令行参数，运行基准测试，生成报告，
    可选地与历史数据进行性能回归比较。
    """
    parser = argparse.ArgumentParser(
        description="Lv-00 性能基准测试工具 —— 运行测试、生成报告、检测性能回归"
    )
    parser.add_argument("--build-dir", default=_DEFAULT_BUILD_DIR, help="构建目录")
    parser.add_argument("--output-dir", default=_DEFAULT_OUTPUT_DIR, help="输出目录")
    parser.add_argument("--compare-with", help="与历史数据比较")
    parser.add_argument("--fail-on-regression", action="store_true",
                       help="检测到性能回归时返回非零退出码")
    parser.add_argument("--debug", action="store_true",
                       help="启用调试输出（也可通过环境变量 LV00_DEBUG=1 启用）")

    args = parser.parse_args()

    # 命令行参数优先级高于环境变量
    debug = _DEBUG
    if args.debug:
        debug = True
    
    # 运行基准测试
    runner = BenchmarkRunner(args.build_dir, debug=debug)
    results = runner.run_all_benchmarks()
    
    if not results:
        # 错误消息始终输出
        sys.stderr.write("错误: 没有基准测试结果\n")
        sys.exit(1)
    
    # 生成报告
    json_file = runner.generate_report(args.output_dir)
    
    # 性能比较
    if args.compare_with:
        if debug:
            print("\n" + "=" * 60)
            print("性能比较")
            print("=" * 60)
            print()
        
        try:
            comparator = PerformanceComparator(args.compare_with)
        except (FileNotFoundError, json.JSONDecodeError) as e:
            sys.stderr.write(f"错误: 无法加载基线文件: {e}\n")
            sys.exit(1)
        has_regression, regressions = comparator.compare(results)
        
        if regressions:
            if debug:
                print("检测到的性能变化:\n")
            for reg in regressions:
                if debug:
                    print(f"  . {reg}")
            if debug:
                print()
        else:
            if debug:
                print(". 未检测到显著性能回归\n")
        
        if has_regression and args.fail_on_regression:
            sys.stderr.write("错误: 检测到性能回归，退出\n")
            sys.exit(2)
    
    if debug:
        print("=" * 60)
        print("基准测试完成")
        print("=" * 60)


if __name__ == "__main__":
    main()
