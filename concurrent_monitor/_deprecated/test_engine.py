"""
[已废弃] 此文件为旧版独立脚本，已被 concurrent_monitor/ 包结构替代。
保留仅供参考和向后兼容，新代码请使用包结构导入。
@deprecated 请使用 `from concurrent_monitor.core.engine import MonitorEngine` 等替代。
"""

"""
核心引擎功能验证 - 不依赖外部库
"""
import sys
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from monitor_engine import MonitorEngine, OutputLine, ProcessStatus


def test_basic():
    """基本功能测试"""
    print("=" * 50)
    print("测试1: 单进程执行")
    print("=" * 50)

    engine = MonitorEngine(max_concurrency=5)
    engine.register_process("test1", "echo Hello World")

    outputs = []
    engine.subscribe(lambda line: outputs.append(line))

    engine.run_all_sync(timeout=10)

    proc = engine.get_process("test1")
    assert proc is not None
    assert proc.status == ProcessStatus.COMPLETED
    assert proc.exit_code == 0
    assert any("Hello World" in o.content for o in outputs)
    print("  ✅ 单进程执行通过")


def test_multi_concurrent():
    """并发多进程测试"""
    print()
    print("=" * 50)
    print("测试2: 多进程并发执行")
    print("=" * 50)

    engine = MonitorEngine(max_concurrency=10)
    commands = [
        ("cmd1", 'powershell -Command "Write-Host Output1; Start-Sleep 0.5; Write-Host Output2"'),
        ("cmd2", 'powershell -Command "Write-Host Hello_from_2; Start-Sleep 0.3"'),
        ("cmd3", 'powershell -Command "1..3 | ForEach-Object { Write-Host Line_$_ }"'),
    ]
    for pid, cmd in commands:
        engine.register_process(pid, cmd)

    outputs_by_proc = {}
    def collect(line):
        if line.process_id not in outputs_by_proc:
            outputs_by_proc[line.process_id] = []
        outputs_by_proc[line.process_id].append(line.content)

    engine.subscribe(collect)
    engine.run_all_sync(timeout=15)

    # 验证所有进程都完成了
    for pid, _ in commands:
        proc = engine.get_process(pid)
        assert proc is not None, f"进程 {pid} 不存在"
        assert proc.status == ProcessStatus.COMPLETED, f"进程 {pid} 状态异常: {proc.status}"
        assert proc.exit_code == 0, f"进程 {pid} 退出码异常: {proc.exit_code}"

    # 验证输出
    assert "Output1" in outputs_by_proc.get("cmd1", [])
    assert "Output2" in outputs_by_proc.get("cmd1", [])
    assert "Hello_from_2" in outputs_by_proc.get("cmd2", [])
    assert "Line_1" in outputs_by_proc.get("cmd3", [])

    print("  ✅ 多进程并发执行通过")


def test_summary():
    """摘要功能测试"""
    print()
    print("=" * 50)
    print("测试3: 摘要统计")
    print("=" * 50)

    engine = MonitorEngine()
    engine.register_process("ok", "echo ok")
    engine.register_process("fail", "exit 1")
    engine.run_all_sync()

    summary = engine.get_summary()
    assert summary["total"] == 2
    assert summary["status_counts"].get("completed", 0) >= 1
    assert len(summary["processes"]) == 2

    for p in summary["processes"]:
        assert "id" in p
        assert "command" in p
        assert "status" in p
        assert "lines" in p
        assert "duration" in p

    print("  ✅ 摘要统计通过")


def test_timeout():
    """超时测试"""
    print()
    print("=" * 50)
    print("测试4: 超时处理")
    print("=" * 50)

    engine = MonitorEngine()
    engine.register_process("slow", 'powershell -Command "Start-Sleep 10"')
    engine.run_all_sync(timeout=2)

    proc = engine.get_process("slow")
    assert proc is not None
    assert proc.status == ProcessStatus.TIMEOUT, f"期望 TIMEOUT，实际: {proc.status}"

    print("  ✅ 超时处理通过")


def test_stop():
    """停止进程测试"""
    print()
    print("=" * 50)
    print("测试5: 强制停止")
    print("=" * 50)

    engine = MonitorEngine()
    engine.register_process("long", 'powershell -Command "Start-Sleep 30"')

    import threading
    t = engine.run_in_background(timeout=None)

    import time
    time.sleep(1)
    engine.stop_process("long")

    proc = engine.get_process("long")
    assert proc is not None
    assert proc.status == ProcessStatus.FAILED

    print("  ✅ 强制停止通过")


if __name__ == "__main__":
    print()
    print("╔════════════════════════════════════════════════╗")
    print("║   并发输出实时监控 - 核心引擎验证             ║")
    print("╚════════════════════════════════════════════════╝")
    print()

    try:
        test_basic()
        test_multi_concurrent()
        test_summary()
        test_timeout()
        test_stop()

        print()
        print("═" * 50)
        print("  🎉 全部测试通过！")
        print("═" * 50)
    except Exception as e:
        print(f"\n  ❌ 测试失败: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
