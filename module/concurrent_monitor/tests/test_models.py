"""
测试数据模型模块
===============
"""
import time
import pytest
from concurrent_monitor.core.models import (
    ProcessStatus,
    StreamType,
    OutputLine,
    ProcessInfo,
    ProcessSummary
)


class TestProcessStatus:
    """测试 ProcessStatus 枚举"""

    def test_is_terminal(self):
        """测试终止状态判断"""
        assert ProcessStatus.COMPLETED.is_terminal is True
        assert ProcessStatus.FAILED.is_terminal is True
        assert ProcessStatus.TIMEOUT.is_terminal is True
        assert ProcessStatus.PENDING.is_terminal is False
        assert ProcessStatus.RUNNING.is_terminal is False

    def test_icon(self):
        """测试状态图标"""
        assert ProcessStatus.PENDING.icon == "○"
        assert ProcessStatus.RUNNING.icon == "◉"
        assert ProcessStatus.COMPLETED.icon == "✔"
        assert ProcessStatus.FAILED.icon == "✘"
        assert ProcessStatus.TIMEOUT.icon == "⏱"

    def test_color(self):
        """测试状态颜色"""
        assert ProcessStatus.PENDING.color == "dim"
        assert ProcessStatus.RUNNING.color == "yellow"
        assert ProcessStatus.COMPLETED.color == "green"
        assert ProcessStatus.FAILED.color == "red"
        assert ProcessStatus.TIMEOUT.color == "magenta"


class TestOutputLine:
    """测试 OutputLine 数据类"""

    def test_creation(self):
        """测试创建输出行"""
        line = OutputLine(
            process_id="test_1",
            stream="stdout",
            content="Hello World"
        )
        assert line.process_id == "test_1"
        assert line.stream == "stdout"
        assert line.content == "Hello World"
        assert isinstance(line.timestamp, float)

    def test_to_dict(self):
        """测试序列化"""
        line = OutputLine("test", "stderr", "Error message")
        d = line.to_dict()
        assert d["process_id"] == "test"
        assert d["stream"] == "stderr"
        assert d["content"] == "Error message"
        assert "timestamp" in d

    def test_from_dict(self):
        """测试反序列化"""
        data = {
            "process_id": "test",
            "stream": "system",
            "content": "System message",
            "timestamp": 123456789.0
        }
        line = OutputLine.from_dict(data)
        assert line.process_id == "test"
        assert line.stream == "system"
        assert line.content == "System message"
        assert line.timestamp == 123456789.0

    def test_frozen(self):
        """测试对象不可变"""
        line = OutputLine("test", "stdout", "Hello")
        with pytest.raises(AttributeError):
            line.content = "Changed"


class TestProcessInfo:
    """测试 ProcessInfo 数据类"""

    def test_creation(self):
        """测试创建进程信息"""
        proc = ProcessInfo(
            process_id="test_proc",
            command="echo hello"
        )
        assert proc.process_id == "test_proc"
        assert proc.command == "echo hello"
        assert proc.status == ProcessStatus.PENDING
        assert proc.exit_code is None
        assert len(proc.output_lines) == 0

    def test_mark_started(self):
        """测试标记开始"""
        proc = ProcessInfo("test", "cmd")
        proc.mark_started()
        assert proc.status == ProcessStatus.RUNNING
        assert proc.start_time is not None
        assert proc.is_running is True
        assert proc.is_finished is False

    def test_mark_completed_success(self):
        """测试标记成功完成"""
        proc = ProcessInfo("test", "cmd")
        proc.mark_started()
        time.sleep(0.01)
        proc.mark_completed(0)
        assert proc.status == ProcessStatus.COMPLETED
        assert proc.exit_code == 0
        assert proc.end_time is not None
        assert proc.is_finished is True
        assert proc.duration is not None

    def test_mark_completed_failed(self):
        """测试标记失败完成"""
        proc = ProcessInfo("test", "cmd")
        proc.mark_started()
        proc.mark_completed(1)
        assert proc.status == ProcessStatus.FAILED
        assert proc.exit_code == 1

    def test_mark_timeout(self):
        """测试标记超时"""
        proc = ProcessInfo("test", "cmd")
        proc.mark_started()
        proc.mark_timeout()
        assert proc.status == ProcessStatus.TIMEOUT
        assert proc.exit_code == -1

    def test_mark_failed(self):
        """测试标记失败"""
        proc = ProcessInfo("test", "cmd")
        proc.mark_started()
        proc.mark_failed("Something went wrong")
        assert proc.status == ProcessStatus.FAILED
        assert proc.metadata["error_message"] == "Something went wrong"

    def test_add_output(self):
        """测试添加输出"""
        proc = ProcessInfo("test", "cmd")
        proc.add_output("Line 1")
        proc.add_output("Line 2", is_error=True)
        assert len(proc.output_lines) == 2
        assert proc.error_count == 1

    def test_duration(self):
        """测试时长计算"""
        proc = ProcessInfo("test", "cmd")
        assert proc.duration is None
        proc.mark_started()
        time.sleep(0.01)
        assert proc.duration is not None
        assert proc.duration > 0

    def test_to_dict(self):
        """测试序列化"""
        proc = ProcessInfo("test", "cmd")
        proc.mark_started()
        proc.add_output("test output")
        d = proc.to_dict()
        assert d["process_id"] == "test"
        assert d["command"] == "cmd"
        assert d["status"] == "running"
        assert d["lines"] == 1


class TestProcessSummary:
    """测试 ProcessSummary 数据类"""

    def test_from_processes_empty(self):
        """测试空进程列表"""
        summary = ProcessSummary.from_processes({})
        assert summary.total == 0
        assert summary.success_rate == 0.0

    def test_from_processes_mixed(self):
        """测试混合状态"""
        proc1 = ProcessInfo("p1", "cmd1")
        proc1.mark_started()
        proc1.mark_completed(0)

        proc2 = ProcessInfo("p2", "cmd2")
        proc2.mark_started()
        proc2.mark_completed(1)

        proc3 = ProcessInfo("p3", "cmd3")
        proc3.mark_started()
        proc3.mark_timeout()

        proc4 = ProcessInfo("p4", "cmd4")

        processes = {"p1": proc1, "p2": proc2, "p3": proc3, "p4": proc4}
        summary = ProcessSummary.from_processes(processes)

        assert summary.total == 4
        assert summary.completed == 1
        assert summary.failed == 1
        assert summary.timeout == 1
        assert summary.pending == 1
        assert summary.success_rate == 25.0

    def test_to_dict(self):
        """测试序列化"""
        summary = ProcessSummary(
            total=10,
            completed=7,
            failed=2,
            timeout=1,
            running=0,
            pending=0
        )
        d = summary.to_dict()
        assert d["total"] == 10
        assert d["completed"] == 7
        assert d["success_rate"] == "70.0%"
