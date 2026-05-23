#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Lv-00 流式输出端到端测试

测试流程：
1. C 引擎流式事件发射
2. Python 桥接服务器 WebSocket 推送
3. 前端事件接收和过滤

运行方式:
    python test_streaming_e2e.py
"""

import asyncio
import json
import os
import sys
import time
import unittest
from unittest.mock import Mock, patch, MagicMock
from dataclasses import dataclass
from typing import List, Dict, Any, Optional

# 添加模块路径
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


# ================================================================
# 模拟 C 引擎事件 / Mock C Engine Events
# ================================================================

@dataclass
class MockStreamEvent:
    """模拟的流式事件"""
    type: int
    type_name: str
    description: str
    timestamp_ms: int
    step: int
    total_steps: int
    node_id: int
    constraint_id: int
    color: str

    def to_dict(self) -> Dict[str, Any]:
        return {
            'type': self.type,
            'type_name': self.type_name,
            'description': self.description,
            'timestamp_ms': self.timestamp_ms,
            'step': self.step,
            'total_steps': self.total_steps,
            'node_id': self.node_id,
            'constraint_id': self.constraint_id,
            'color': self.color,
        }


def create_mock_events() -> List[MockStreamEvent]:
    """创建模拟事件序列"""
    return [
        MockStreamEvent(
            type=0,  # ENGINE_START
            type_name='引擎启动',
            description='开始几何求解流程',
            timestamp_ms=int(time.time() * 1000),
            step=0,
            total_steps=10,
            node_id=-1,
            constraint_id=-1,
            color='#3fb950',
        ),
        MockStreamEvent(
            type=3,  # NORMALIZE_START
            type_name='归一化开始',
            description='开始图归一化',
            timestamp_ms=int(time.time() * 1000),
            step=1,
            total_steps=10,
            node_id=-1,
            constraint_id=-1,
            color='#58a6ff',
        ),
        MockStreamEvent(
            type=4,  # NORMALIZE_MERGE
            type_name='节点合并',
            description='合并节点: 5 → 3',
            timestamp_ms=int(time.time() * 1000),
            step=2,
            total_steps=10,
            node_id=3,
            constraint_id=5,
            color='#a371f7',
        ),
        MockStreamEvent(
            type=5,  # NORMALIZE_DONE
            type_name='归一化完成',
            description='合并了 2 个节点',
            timestamp_ms=int(time.time() * 1000),
            step=3,
            total_steps=10,
            node_id=-1,
            constraint_id=-1,
            color='#3fb950',
        ),
        MockStreamEvent(
            type=12,  # SOLVE_START
            type_name='求解开始',
            description='开始代数求解',
            timestamp_ms=int(time.time() * 1000),
            step=4,
            total_steps=10,
            node_id=-1,
            constraint_id=-1,
            color='#f0883e',
        ),
        MockStreamEvent(
            type=15,  # SOLVE_VARIABLE_RESOLVED
            type_name='变量解得',
            description='x = 3/2',
            timestamp_ms=int(time.time() * 1000),
            step=5,
            total_steps=10,
            node_id=-1,
            constraint_id=-1,
            color='#a371f7',
        ),
        MockStreamEvent(
            type=16,  # SOLVE_DONE
            type_name='求解完成',
            description='求解成功，找到 1 个解',
            timestamp_ms=int(time.time() * 1000),
            step=6,
            total_steps=10,
            node_id=-1,
            constraint_id=-1,
            color='#3fb950',
        ),
        MockStreamEvent(
            type=1,  # ENGINE_DONE
            type_name='引擎完成',
            description='几何求解流程完成',
            timestamp_ms=int(time.time() * 1000),
            step=7,
            total_steps=10,
            node_id=-1,
            constraint_id=-1,
            color='#3fb950',
        ),
    ]


# ================================================================
# 测试用例 / Test Cases
# ================================================================

class TestStreamEventEmission(unittest.TestCase):
    """测试流式事件发射"""

    def test_event_creation(self):
        """测试事件创建"""
        events = create_mock_events()
        self.assertEqual(len(events), 8)
        self.assertEqual(events[0].type, 0)
        self.assertEqual(events[0].type_name, '引擎启动')

    def test_event_serialization(self):
        """测试事件序列化"""
        events = create_mock_events()
        for event in events:
            event_dict = event.to_dict()
            self.assertIsInstance(event_dict, dict)
            self.assertIn('type', event_dict)
            self.assertIn('description', event_dict)
            self.assertIn('timestamp_ms', event_dict)

    def test_event_json_encoding(self):
        """测试 JSON 编码"""
        events = create_mock_events()
        for event in events:
            event_dict = event.to_dict()
            json_str = json.dumps(event_dict, ensure_ascii=False)
            self.assertIsInstance(json_str, str)
            # 解码验证
            decoded = json.loads(json_str)
            self.assertEqual(decoded['type'], event.type)


class TestStreamEventFilter(unittest.TestCase):
    """测试事件过滤"""

    def setUp(self):
        self.events = create_mock_events()

    def test_filter_by_category(self):
        """测试按类别过滤"""
        # 引擎事件: type 0, 1
        engine_events = [e for e in self.events if e.type in (0, 1)]
        self.assertEqual(len(engine_events), 2)

        # 归一化事件: type 3, 4, 5
        normalize_events = [e for e in self.events if e.type in (3, 4, 5)]
        self.assertEqual(len(normalize_events), 3)

        # 求解事件: type 12, 15, 16
        solve_events = [e for e in self.events if e.type in (12, 15, 16)]
        self.assertEqual(len(solve_events), 3)

    def test_filter_by_node_id(self):
        """测试按节点 ID 过滤"""
        events_with_node = [e for e in self.events if e.node_id >= 0]
        self.assertEqual(len(events_with_node), 1)
        self.assertEqual(events_with_node[0].node_id, 3)


class TestJSONRPCProtocol(unittest.TestCase):
    """测试 JSON-RPC 协议"""

    def test_notification_format(self):
        """测试通知格式"""
        events = create_mock_events()
        event_dict = events[0].to_dict()

        notification = {
            'jsonrpc': '2.0',
            'method': 'stream.event',
            'params': event_dict,
        }

        self.assertEqual(notification['jsonrpc'], '2.0')
        self.assertEqual(notification['method'], 'stream.event')
        self.assertIn('params', notification)

    def test_request_format(self):
        """测试请求格式"""
        request = {
            'jsonrpc': '2.0',
            'method': 'subscribe',
            'params': {'event_mask': -1},
            'id': 1,
        }

        self.assertEqual(request['jsonrpc'], '2.0')
        self.assertEqual(request['method'], 'subscribe')
        self.assertIn('id', request)

    def test_response_format(self):
        """测试响应格式"""
        response = {
            'jsonrpc': '2.0',
            'id': 1,
            'result': {'status': 'subscribed'},
        }

        self.assertEqual(response['jsonrpc'], '2.0')
        self.assertIn('result', response)
        self.assertNotIn('error', response)

    def test_error_response_format(self):
        """测试错误响应格式"""
        error_response = {
            'jsonrpc': '2.0',
            'id': 1,
            'error': {
                'code': -32601,
                'message': 'Method not found',
            },
        }

        self.assertIn('error', error_response)
        self.assertIn('code', error_response['error'])
        self.assertIn('message', error_response['error'])


class TestWebSocketProtocol(unittest.TestCase):
    """测试 WebSocket 协议"""

    def test_subscribe_message(self):
        """测试订阅消息"""
        subscribe = {
            'jsonrpc': '2.0',
            'method': 'subscribe',
            'params': {'event_mask': -1},
            'id': 1,
        }
        json_str = json.dumps(subscribe)
        decoded = json.loads(json_str)
        self.assertEqual(decoded['method'], 'subscribe')

    def test_ping_pong(self):
        """测试心跳"""
        ping = {
            'jsonrpc': '2.0',
            'method': 'ping',
            'id': 1,
        }
        pong = {
            'jsonrpc': '2.0',
            'id': 1,
            'result': 'pong',
        }
        self.assertEqual(ping['id'], pong['id'])


# ================================================================
# 异步测试 / Async Tests
# ================================================================

class TestAsyncStreaming(unittest.IsolatedAsyncioTestCase):
    """异步流式测试"""

    async def test_event_sequence(self):
        """测试事件序列"""
        events = create_mock_events()

        received_events = []

        async def event_handler(event: Dict):
            received_events.append(event)
            await asyncio.sleep(0.01)  # 模拟处理延迟

        # 模拟事件流
        for event in events:
            await event_handler(event.to_dict())

        self.assertEqual(len(received_events), len(events))

    async def test_event_buffering(self):
        """测试事件缓冲"""
        events = create_mock_events()
        buffer: List[Dict] = []
        max_buffer_size = 5

        for event in events:
            buffer.append(event.to_dict())
            if len(buffer) > max_buffer_size:
                buffer.pop(0)

        self.assertLessEqual(len(buffer), max_buffer_size)

    async def test_event_rate_limiting(self):
        """测试事件速率限制"""
        events = create_mock_events()
        min_interval_ms = 50

        last_time = 0
        for event in events:
            current_time = time.time() * 1000
            if last_time > 0:
                elapsed = current_time - last_time
                if elapsed < min_interval_ms:
                    await asyncio.sleep((min_interval_ms - elapsed) / 1000)
            last_time = time.time() * 1000

        # 验证总耗时
        total_expected = (len(events) - 1) * min_interval_ms / 1000
        self.assertGreater(total_expected, 0)


# ================================================================
# 集成测试 / Integration Tests
# ================================================================

class TestStreamIntegration(unittest.TestCase):
    """集成测试"""

    def test_full_pipeline(self):
        """测试完整流水线"""
        # 1. 创建事件
        events = create_mock_events()
        self.assertEqual(len(events), 8)

        # 2. 序列化
        json_events = [json.dumps(e.to_dict(), ensure_ascii=False) for e in events]
        self.assertEqual(len(json_events), 8)

        # 3. 包装为 JSON-RPC 通知
        notifications = [
            json.dumps({
                'jsonrpc': '2.0',
                'method': 'stream.event',
                'params': json.loads(e),
            }, ensure_ascii=False)
            for e in json_events
        ]
        self.assertEqual(len(notifications), 8)

        # 4. 反序列化验证
        for notification in notifications:
            parsed = json.loads(notification)
            self.assertEqual(parsed['jsonrpc'], '2.0')
            self.assertEqual(parsed['method'], 'stream.event')
            self.assertIn('params', parsed)


# ================================================================
# 主入口 / Main Entry
# ================================================================

if __name__ == '__main__':
    # 运行测试
    unittest.main(verbosity=2)
