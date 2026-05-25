# Lv-00 流式输出使用指南

本文档介绍 Lv-00 几何元编程库的流式输出功能，包括 C API、Python 绑定和 Web 前端集成。

## 目录

1. [概述](#概述)
2. [C API 使用](#c-api-使用)
3. [Python 绑定使用](#python-绑定使用)
4. [Web 前端集成](#web-前端集成)
5. [最佳实践](#最佳实践)

---

## 概述

Lv-00 的流式输出系统允许实时接收引擎事件，包括：

- **归一化事件**：节点合并、约束简化
- **求解事件**：方程求解进度、变量绑定
- **证明事件**：推理步骤、定理应用
- **错误事件**：冲突检测、异常报告

### 事件类型

| 类型标识 | 描述 | 颜色 |
|---------|------|------|
| `normalization_start` | 归一化开始 | 蓝色 |
| `normalization_merge` | 节点合并 | 青色 |
| `normalization_complete` | 归一化完成 | 绿色 |
| `solving_start` | 求解开始 | 蓝色 |
| `solving_progress` | 求解进度 | 黄色 |
| `solving_complete` | 求解完成 | 绿色 |
| `proof_step` | 证明步骤 | 紫色 |
| `error` | 错误 | 红色 |

---

## C API 使用

### 基本用法

```c
#include <lv00/stream.h>

// 1. 创建流上下文
StreamContext* ctx = stream_context_create();

// 2. 定义回调函数
void on_event(const StreamEvent* event, void* user_data) {
    printf("事件: %s - %s\n",
           stream_event_type_id(event->type),
           event->description);
}

// 3. 注册回调
int callback_id = stream_register_callback_ex(
    ctx,
    on_event,
    NULL,  // user_data
    STREAM_FILTER_ALL
);

// 4. 使用引擎（事件会自动触发回调）
Engine* engine = engine_create();
engine_set_stream_context(engine, ctx);
engine_solve(engine);

// 5. 清理
stream_unregister_callback_by_id(ctx, callback_id);
stream_context_destroy(ctx);
```

### 惰性求值模式

```c
// 启用惰性发射模式
stream_set_emit_mode(ctx, STREAM_EMIT_LAZY);

// 执行操作（事件被缓存）
engine_solve(engine);

// 惰性拉取事件
StreamEvent event;
while (stream_lazy_next(ctx, &event)) {
    process_event(&event);
}

// 或批量拉取
int count = stream_lazy_drain(ctx, events, 100);
```

### 事件过滤

```c
// 只接收归一化和求解事件
uint64_t mask = STREAM_EVENT_NORMALIZATION | STREAM_EVENT_SOLVING;
stream_register_callback_ex(ctx, on_event, NULL, mask);
```

---

## Python 绑定使用

### 异步迭代器模式

```python
import asyncio
from lv00 import Engine, AsyncStreamIterator

async def main():
    engine = Engine()

    # 方式1: 异步 for 循环
    async for event in AsyncStreamIterator(engine):
        print(f"事件: {event.type} - {event.description}")
        if event.is_complete():
            break

    # 方式2: 上下文管理器
    async with AsyncStreamContext(engine) as stream:
        async for event in stream:
            process(event)

    # 方式3: 过滤特定事件类型
    async for event in AsyncStreamIterator(
        engine,
        event_types=['normalization', 'solving']
    ):
        print(f"[{event.step}/{event.total_steps}] {event.description}")

asyncio.run(main())
```

### 便捷函数

```python
import asyncio
from lv00 import stream_events, collect_events, wait_for_event

async def example():
    engine = Engine()

    # 流式迭代
    async for event in stream_events(engine, ['normalization']):
        print(event)

    # 收集指定数量的事件
    events = await collect_events(engine, count=10, timeout=5.0)
    print(f"收集到 {len(events)} 个事件")

    # 等待特定事件
    event = await wait_for_event(engine, 'normalization_complete', timeout=10.0)
    if event:
        print("归一化完成！")
```

### 缓冲收集器

```python
import asyncio
from lv00 import BufferedStreamCollector

async def batch_processing():
    engine = Engine()

    # 按批次处理事件
    async with BufferedStreamCollector(
        engine,
        buffer_size=50,    # 每 50 个事件触发一次
        time_window=1.0    # 或每 1 秒触发一次
    ) as collector:
        async for batch in collector.batches():
            print(f"处理批次: {len(batch)} 个事件")
            for event in batch:
                process(event)
```

### 流式桥接服务器

```python
from lv00.stream_bridge import StreamBridgeServer, EngineBridge

# 启动 WebSocket 服务器
engine = EngineBridge('./build/liblv00.so')
server = StreamBridgeServer(engine, 'localhost', 3456)

# 前端可通过 ws://localhost:3456 连接
asyncio.run(server.start())
```

---

## Web 前端集成

### WebSocket 客户端

```typescript
import { StreamClient } from './services/streamClient';

// 创建客户端
const client = new StreamClient('ws://localhost:3456');

// 连接并订阅
await client.connect();
await client.subscribe({ event_mask: -1 });

// 监听事件
client.on('stream.event', (event) => {
    console.log(`事件: ${event.type}`, event);
});

// 断开连接
client.disconnect();
```

### React Hook

```tsx
import { useEngineStream } from '../hooks/useEngineStream';

function StreamPanel() {
    const { events, isConnected, error } = useEngineStream('ws://localhost:3456');

    return (
        <div>
            <div>状态: {isConnected ? '已连接' : '未连接'}</div>
            <ul>
                {events.map((event, i) => (
                    <li key={i}>
                        <span className={`badge badge-${event.color}`}>
                            {event.type_name}
                        </span>
                        {event.description}
                    </li>
                ))}
            </ul>
        </div>
    );
}
```

### SSE 备选通道

```typescript
// 使用 Server-Sent Events 作为备选
const eventSource = new EventSource('http://localhost:3457/events');

eventSource.onmessage = (e) => {
    const event = JSON.parse(e.data);
    console.log('SSE 事件:', event);
};

eventSource.onerror = (e) => {
    console.error('SSE 连接错误');
};
```

---

## 最佳实践

### 1. 事件过滤

只订阅需要的事件类型，减少不必要的处理开销：

```python
# 推荐：只订阅需要的事件
async for event in AsyncStreamIterator(engine, event_types=['normalization']):
    process(event)

# 不推荐：订阅所有事件再过滤
async for event in AsyncStreamIterator(engine):
    if event.type == 'normalization':
        process(event)
```

### 2. 超时控制

设置合理的超时，避免无限等待：

```python
async for event in AsyncStreamIterator(engine, timeout=30.0):
    if event.is_error():
        break
```

### 3. 资源清理

使用上下文管理器确保资源正确释放：

```python
# 推荐：自动清理
async with AsyncStreamContext(engine) as stream:
    async for event in stream:
        process(event)

# 手动清理时记得调用 close
iterator = AsyncStreamIterator(engine)
try:
    async for event in iterator:
        process(event)
finally:
    iterator.close()
```

### 4. 批量处理

对于高频事件流，使用批量处理提高效率：

```python
async with BufferedStreamCollector(engine, buffer_size=100) as collector:
    async for batch in collector.batches():
        # 批量处理
        await process_batch(batch)
```

### 5. 错误处理

正确处理流中的错误事件：

```python
async for event in AsyncStreamIterator(engine):
    if event.is_error():
        logger.error(f"引擎错误: {event.description}")
        # 根据错误类型决定是否继续
        if event.type == 'fatal_error':
            break
```

---

## 完整示例

### Python 端到端示例

```python
import asyncio
import logging
from lv00 import Engine, Graph, Point
from lv00 import AsyncStreamContext, StreamEvent

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

async def solve_with_streaming():
    """使用流式输出求解几何问题。"""

    # 创建引擎和约束图
    engine = Engine()
    graph = Graph()

    # 添加几何元素
    a = graph.add_point(0, 0)
    b = graph.add_point(2, 0)
    c = graph.add_point(1, 1)

    # 使用流式上下文
    async with AsyncStreamContext(engine) as stream:
        # 启动求解（在后台）
        solve_task = asyncio.create_task(
            asyncio.to_thread(engine.solve, graph)
        )

        # 实时处理事件
        async for event in stream:
            logger.info(f"[{event.step}/{event.total_steps}] {event.type}: {event.description}")

            # 显示进度
            if event.progress > 0:
                print(f"进度: {event.progress:.1f}%")

            # 检查完成
            if event.is_complete():
                print("求解完成！")
                break

            # 检查错误
            if event.is_error():
                logger.error(f"求解错误: {event.description}")
                break

        # 等待求解完成
        await solve_task

if __name__ == '__main__':
    asyncio.run(solve_with_streaming())
```

---

## 参考文档

- [C API 参考](../include/lv00/stream.h)
- [Python API 参考](../python/lv00/async_stream.py)
- [Web 前端 API](../web-gui/src/services/streamClient.ts)
- [实现状态报告](./implementation_status.md)
