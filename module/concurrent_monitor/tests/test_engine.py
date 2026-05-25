"""
测试监控引擎模块
===============
"""
import asyncio
import platform
import pytest
from concurrent_monitor.core.engine import MonitorEngine
from concurrent_monitor.core.models import ProcessStatus, ProcessInfo
from concurrent_monitor.core.events import EventBus, EventType


@pytest.fixture
def engine():
    """创建测试引擎"""
    bus = EventBus()
    eng = MonitorEngine(max_concurrency=5, event_bus=bus)
    yield eng
    eng.shutdown()


def get_test_command(message="test output", exit_code=0):
    """获取跨平台的测试命令"""
    if platform.system() == "Windows":
        return f'python -c "print(\'{message}\'); import sys; sys.exit({exit_code})"'
    else:
        return f'python3 -c "print(\'{message}\'); import sys; sys.exit({exit_code})"'


class TestMonitorEngineBasic:
    """测试引擎基本功能"""

    def test_create_engine(self, engine):
        """测试创建引擎"""
        assert engine is not None
        assert engine.config.engine.max_concurrency == 5
        assert engine.is_running is False

    def test_register_and_get_process(self, engine):
        """测试注册和获取进程"""
        proc = engine.register_process("test1", "echo hello")
        assert proc.process_id == "test1"
        assert proc.command == "echo hello"
        assert proc.status == ProcessStatus.PENDING

        retrieved = engine.get_process("test1")
        assert retrieved is not None
        assert retrieved.process_id == "test1"

    def test_register_duplicate_id(self, engine):
        """测试重复ID报错"""
        engine.register_process("test1", "echo hello")
        with pytest.raises(ValueError, match="进程ID已存在"):
            engine.register_process("test1", "echo another")

    def test_get_nonexistent_process(self, engine):
        """测试获取不存在的进程"""
        assert engine.get_process("nonexistent") is None

    def test_get_all_processes(self, engine):
        """测试获取所有进程"""
        engine.register_process("test1", "cmd1")
        engine.register_process("test2", "cmd2")

        processes = engine.get_all_processes()
        assert len(processes) == 2
        assert "test1" in processes
        assert "test2" in processes

    def test_unregister_process(self, engine):
        """测试注销进程"""
        engine.register_process("test1", "cmd1")
        assert engine.unregister_process("test1") is True
        assert engine.unregister_process("test1") is False
        assert engine.get_process("test1") is None


class TestMonitorEngineExecution:
    """测试引擎执行功能"""

    @pytest.mark.asyncio
    async def test_run_single_process_success(self):
        """测试运行单个成功进程"""
        bus = EventBus()
        engine = MonitorEngine(max_concurrency=5, event_bus=bus)

        try:
            engine.register_process("test1", get_test_command("hello"))
            await engine.run_all()

            proc = engine.get_process("test1")
            assert proc is not None
            assert proc.status == ProcessStatus.COMPLETED
            assert proc.exit_code == 0
        finally:
            engine.shutdown()

    @pytest.mark.asyncio
    async def test_run_single_process_failure(self):
        """测试运行单个失败进程"""
        bus = EventBus()
        engine = MonitorEngine(max_concurrency=5, event_bus=bus)

        try:
            engine.register_process("fail1", get_test_command("fail", exit_code=1))
            await engine.run_all()

            proc = engine.get_process("fail1")
            assert proc is not None
            assert proc.status == ProcessStatus.FAILED
            assert proc.exit_code == 1
        finally:
            engine.shutdown()

    @pytest.mark.asyncio
    async def test_run_multiple_processes(self):
        """测试运行多个进程"""
        bus = EventBus()
        engine = MonitorEngine(max_concurrency=5, event_bus=bus)

        try:
            engine.register_process("p1", get_test_command("p1"))
            engine.register_process("p2", get_test_command("p2"))
            await engine.run_all()

            p1 = engine.get_process("p1")
            p2 = engine.get_process("p2")

            assert p1 is not None and p1.status == ProcessStatus.COMPLETED
            assert p2 is not None and p2.status == ProcessStatus.COMPLETED
        finally:
            engine.shutdown()

    def test_subscribe_and_unsubscribe(self, engine):
        """测试订阅和取消订阅"""
        received = []

        def callback(output_line):
            received.append(output_line)

        engine.subscribe(callback)

        # 注销
        assert engine.unsubscribe(callback) is True
        assert engine.unsubscribe(callback) is False


class TestMonitorEngineSummary:
    """测试摘要功能"""

    def test_get_summary(self, engine):
        """测试获取摘要"""
        # 注册一些进程
        engine.register_process("p1", "cmd1")
        engine.register_process("p2", "cmd2")

        summary = engine.get_summary()
        assert summary["total"] == 2
        assert summary["status_counts"]["pending"] == 2


class TestMonitorEngineContextManager:
    """测试上下文管理器"""

    def test_context_manager(self):
        """测试with语句"""
        with MonitorEngine(max_concurrency=5) as engine:
            assert engine is not None
            engine.register_process("test", "echo hello")
        # 退出上下文后，引擎应已关闭


def test_engine_shutdown(engine):
    """测试关闭引擎"""
    engine.register_process("test", "echo hello")
    engine.shutdown()
    # 关闭后应无错误
