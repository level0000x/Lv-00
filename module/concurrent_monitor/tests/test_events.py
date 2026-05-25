"""
测试事件总线模块
===============
"""
import pytest
from concurrent_monitor.core.events import (
    EventType,
    Event,
    EventBus,
    get_event_bus
)


class TestEvent:
    """测试 Event 类"""

    def test_creation(self):
        """测试创建事件"""
        event = Event(
            type=EventType.OUTPUT,
            process_id="test_1",
            data="Test data"
        )
        assert event.type == EventType.OUTPUT
        assert event.process_id == "test_1"
        assert event.data == "Test data"
        assert isinstance(event.timestamp, float)

    def test_repr(self):
        """测试字符串表示"""
        event = Event(EventType.OUTPUT, "test", "data")
        repr_str = repr(event)
        assert "OUTPUT" in repr_str
        assert "test" in repr_str


class TestEventBus:
    """测试 EventBus 类"""

    def test_subscribe_and_publish(self):
        """测试订阅和发布"""
        bus = EventBus()
        events = []

        def handler(event):
            events.append(event)

        bus.subscribe(EventType.OUTPUT, handler)

        test_event = Event(EventType.OUTPUT, "test", "hello")
        bus.publish(test_event)

        assert len(events) == 1
        assert events[0] == test_event

    def test_unsubscribe(self):
        """测试取消订阅"""
        bus = EventBus()
        events = []

        def handler(event):
            events.append(event)

        bus.subscribe(EventType.OUTPUT, handler)
        assert bus.unsubscribe(EventType.OUTPUT, handler) is True
        assert bus.unsubscribe(EventType.OUTPUT, handler) is False

        test_event = Event(EventType.OUTPUT, "test", "hello")
        bus.publish(test_event)
        assert len(events) == 0

    def test_multiple_handlers(self):
        """测试多个处理器"""
        bus = EventBus()
        count1 = 0
        count2 = 0

        def handler1(event):
            nonlocal count1
            count1 += 1

        def handler2(event):
            nonlocal count2
            count2 += 1

        bus.subscribe(EventType.OUTPUT, handler1)
        bus.subscribe(EventType.OUTPUT, handler2)

        test_event = Event(EventType.OUTPUT, "test", "hello")
        bus.publish(test_event)

        assert count1 == 1
        assert count2 == 1

    def test_separate_event_types(self):
        """测试不同事件类型独立"""
        bus = EventBus()
        output_events = []
        error_events = []

        def output_handler(event):
            output_events.append(event)

        def error_handler(event):
            error_events.append(event)

        bus.subscribe(EventType.OUTPUT, output_handler)
        bus.subscribe(EventType.ERROR, error_handler)

        bus.publish(Event(EventType.OUTPUT, "test", "output"))
        bus.publish(Event(EventType.ERROR, "test", "error"))

        assert len(output_events) == 1
        assert len(error_events) == 1

    def test_handler_exception(self):
        """测试处理器异常不影响其他处理器"""
        bus = EventBus()
        count = 0

        def bad_handler(event):
            raise ValueError("Bad handler")

        def good_handler(event):
            nonlocal count
            count += 1

        bus.subscribe(EventType.OUTPUT, bad_handler)
        bus.subscribe(EventType.OUTPUT, good_handler)

        test_event = Event(EventType.OUTPUT, "test", "hello")
        bus.publish(test_event)

        assert count == 1

    def test_weakref_cleanup(self):
        """测试弱引用清理"""
        bus = EventBus()

        def create_handler():
            def handler(event):
                pass
            return handler

        handler = create_handler()
        bus.subscribe(EventType.OUTPUT, handler)

        del handler

        # 再次发布，触发清理
        bus.publish(Event(EventType.OUTPUT, "test", "hello"))

    def test_clear(self):
        """测试清空所有处理器"""
        bus = EventBus()
        count = 0

        def handler(event):
            nonlocal count
            count += 1

        bus.subscribe(EventType.OUTPUT, handler)
        bus.clear()

        bus.publish(Event(EventType.OUTPUT, "test", "hello"))
        assert count == 0

    def test_get_stats(self):
        """测试获取统计信息"""
        bus = EventBus()

        def handler1(event):
            pass

        def handler2(event):
            pass

        bus.subscribe(EventType.OUTPUT, handler1)
        bus.subscribe(EventType.ERROR, handler2)

        stats = bus.get_stats()
        assert stats["sync_handlers"] == 2
        assert stats["total"] == 2

    def test_publish_sync(self):
        """测试只发布到同步处理器"""
        bus = EventBus()
        sync_count = 0

        def sync_handler(event):
            nonlocal sync_count
            sync_count += 1

        bus.subscribe(EventType.OUTPUT, sync_handler)

        test_event = Event(EventType.OUTPUT, "test", "hello")
        bus.publish_sync(test_event)
        assert sync_count == 1


class TestGlobalEventBus:
    """测试全局事件总线"""

    def test_get_event_bus(self):
        """测试获取全局总线"""
        bus1 = get_event_bus()
        bus2 = get_event_bus()
        assert bus1 is bus2

    def test_global_bus_usage(self):
        """测试全局总线使用"""
        bus = get_event_bus()
        events = []

        def handler(event):
            events.append(event)

        bus.subscribe(EventType.SYSTEM, handler)
        bus.publish(Event(EventType.SYSTEM, "test", "system message"))

        assert len(events) == 1

        # 清理
        bus.unsubscribe(EventType.SYSTEM, handler)
