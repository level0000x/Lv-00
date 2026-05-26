#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Lv-00 流式输出示例

演示如何使用 Lv-00 的流式输出功能，包括：
1. 异步迭代器模式
2. 事件过滤
3. 批量处理
4. 错误处理
5. 与引擎集成

运行方式：
    python streaming_example.py

依赖：
    pip install aiohttp websockets

作者：Lv-00 开发团队
版本：3.5.0
"""

import asyncio
import logging

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(name)s: %(message)s',
    datefmt='%H:%M:%S'
)
logger = logging.getLogger('streaming_example')

# ================================================================
# 示例 1: 基本异步迭代器
# ================================================================

async def example_basic_iterator():
    """
    示例 1: 基本异步迭代器模式

    演示如何使用 AsyncStreamIterator 消费引擎事件。
    """
    print("\n" + "=" * 60)
    print("示例 1: 基本异步迭代器")
    print("=" * 60)

    try:
        from lv00 import Engine, AsyncStreamIterator
    except ImportError:
        print("警告: lv00 模块未安装，使用模拟模式")
        await _simulate_basic_iterator()
        return

    engine = Engine()

    # 创建异步迭代器
    iterator = AsyncStreamIterator(engine, timeout=5.0)

    try:
        async for event in iterator:
            print(f"  事件: {event.type}")
            print(f"    描述: {event.description}")
            print(f"    步骤: {event.step}/{event.total_steps}")

            if event.is_complete():
                print("  ✓ 完成!")
                break
    finally:
        iterator.close()


async def _simulate_basic_iterator():
    """模拟基本迭代器（当 lv00 未安装时使用）。"""
    print("  [模拟] 开始迭代事件...")

    for i in range(5):
        await asyncio.sleep(0.5)
        print(f"  [模拟] 事件 {i+1}: 步骤 {i+1}/5")

    print("  [模拟] ✓ 迭代完成!")


# ================================================================
# 示例 2: 事件过滤
# ================================================================

async def example_event_filtering():
    """
    示例 2: 事件类型过滤

    演示如何只接收特定类型的事件。
    """
    print("\n" + "=" * 60)
    print("示例 2: 事件类型过滤")
    print("=" * 60)

    try:
        from lv00 import Engine, AsyncStreamIterator
    except ImportError:
        print("警告: lv00 模块未安装，使用模拟模式")
        await _simulate_event_filtering()
        return

    engine = Engine()

    # 只接收归一化和求解事件
    event_types = ['normalization', 'solving']

    async with AsyncStreamIterator(
        engine,
        event_types=event_types,
        timeout=10.0
    ) as stream:
        print(f"  已订阅事件类型: {event_types}")

        async for event in stream:
            print(f"  [{event.type}] {event.description}")

            if event.is_complete():
                break


async def _simulate_event_filtering():
    """模拟事件过滤。"""
    print("  [模拟] 已订阅事件类型: ['normalization', 'solving']")

    events = [
        ('normalization', '开始归一化约束图'),
        ('normalization_merge', '合并节点 5 和 7'),
        ('solving', '开始求解方程组'),
        ('solving_progress', '已解出变量 x'),
        ('solving_complete', '求解完成'),
    ]

    for event_type, desc in events:
        await asyncio.sleep(0.3)
        print(f"  [{event_type}] {desc}")


# ================================================================
# 示例 3: 批量处理
# ================================================================

async def example_batch_processing():
    """
    示例 3: 批量事件处理

    演示如何使用 BufferedStreamCollector 按批次处理事件。
    """
    print("\n" + "=" * 60)
    print("示例 3: 批量事件处理")
    print("=" * 60)

    try:
        from lv00 import Engine, BufferedStreamCollector
    except ImportError:
        print("警告: lv00 模块未安装，使用模拟模式")
        await _simulate_batch_processing()
        return

    engine = Engine()

    # 配置: 每 10 个事件或每 1 秒触发一次批次
    async with BufferedStreamCollector(
        engine,
        buffer_size=10,
        time_window=1.0
    ) as collector:
        batch_count = 0

        async for batch in collector.batches():
            batch_count += 1
            print(f"  批次 {batch_count}: 收到 {len(batch)} 个事件")

            for event in batch:
                print(f"    - {event.type}: {event.description[:50]}...")

            # 示例: 处理 3 个批次后停止
            if batch_count >= 3:
                print("  已处理 3 个批次，停止")
                break


async def _simulate_batch_processing():
    """模拟批量处理。"""
    print("  [模拟] 配置: buffer_size=10, time_window=1.0s")

    for batch_num in range(1, 4):
        await asyncio.sleep(0.8)
        event_count = 8 + batch_num  # 模拟不同大小
        print(f"  [模拟] 批次 {batch_num}: 收到 {event_count} 个事件")

        for i in range(min(3, event_count)):  # 只显示前 3 个
            print(f"    - 事件 {i+1}: 描述...")


# ================================================================
# 示例 4: 便捷函数
# ================================================================

async def example_convenience_functions():
    """
    示例 4: 使用便捷函数

    演示 stream_events, collect_events, wait_for_event 的用法。
    """
    print("\n" + "=" * 60)
    print("示例 4: 便捷函数")
    print("=" * 60)

    try:
        from lv00 import stream_events, collect_events, wait_for_event
        from lv00.stream_bridge import EngineBridge
    except ImportError:
        print("警告: lv00 模块未安装，使用模拟模式")
        await _simulate_convenience_functions()
        return

    # 创建引擎桥接器
    engine = EngineBridge()

    # 示例 4.1: stream_events
    print("\n  4.1 stream_events - 流式迭代:")
    count = 0
    async for event in stream_events(engine, timeout=3.0):
        print(f"    {event.type}")
        count += 1
        if count >= 5:
            break

    # 示例 4.2: collect_events
    print("\n  4.2 collect_events - 收集指定数量:")
    events = await collect_events(engine, count=3, timeout=5.0)
    print(f"    收集到 {len(events)} 个事件")

    # 示例 4.3: wait_for_event
    print("\n  4.3 wait_for_event - 等待特定事件:")
    event = await wait_for_event(
        engine,
        'normalization_complete',
        timeout=10.0
    )
    if event:
        print(f"    收到目标事件: {event.type}")
    else:
        print("    超时未收到目标事件")


async def _simulate_convenience_functions():
    """模拟便捷函数。"""
    print("  [模拟] 4.1 stream_events:")
    for i in range(5):
        await asyncio.sleep(0.2)
        print(f"    事件_{i+1}")

    print("\n  [模拟] 4.2 collect_events:")
    await asyncio.sleep(0.5)
    print("    收集到 3 个事件")

    print("\n  [模拟] 4.3 wait_for_event:")
    await asyncio.sleep(0.5)
    print("    收到目标事件: normalization_complete")


# ================================================================
# 示例 5: 完整工作流
# ================================================================

async def example_complete_workflow():
    """
    示例 5: 完整工作流

    演示如何将流式输出集成到完整的几何求解工作流中。
    """
    print("\n" + "=" * 60)
    print("示例 5: 完整工作流")
    print("=" * 60)

    try:
        from lv00 import Engine, Graph, AsyncStreamContext
    except ImportError:
        print("警告: lv00 模块未安装，使用模拟模式")
        await _simulate_complete_workflow()
        return

    # 创建引擎和约束图
    engine = Engine()
    graph = Graph()

    # 添加几何元素（三角形）
    print("  创建几何图形: 三角形")
    a = graph.add_point(0, 0)
    b = graph.add_point(4, 0)
    c = graph.add_point(2, 3)

    print(f"    点 A: ({a.x}, {a.y})")
    print(f"    点 B: ({b.x}, {b.y})")
    print(f"    点 C: ({c.x}, {c.y})")

    # 使用流式上下文进行求解
    print("\n  开始求解（流式输出）:")

    async with AsyncStreamContext(engine) as stream:
        # 在后台执行求解
        solve_task = asyncio.create_task(
            asyncio.to_thread(engine.solve, graph)
        )

        # 实时处理事件
        event_count = 0
        async for event in stream:
            event_count += 1

            # 显示进度
            if event.total_steps > 0:
                progress = event.step / event.total_steps * 100
                bar_len = int(progress / 5)
                bar = '█' * bar_len + '░' * (20 - bar_len)
                print(f"\r    [{bar}] {progress:.0f}% - {event.type}", end='', flush=True)

            if event.is_complete():
                print()  # 换行
                break

        # 等待求解完成
        await solve_task

    print(f"\n  ✓ 求解完成，共处理 {event_count} 个事件")


async def _simulate_complete_workflow():
    """模拟完整工作流。"""
    print("  [模拟] 创建几何图形: 三角形")
    print("    点 A: (0, 0)")
    print("    点 B: (4, 0)")
    print("    点 C: (2, 3)")

    print("\n  [模拟] 开始求解（流式输出）:")

    for i in range(21):
        await asyncio.sleep(0.1)
        progress = i * 5
        bar_len = i
        bar = '█' * bar_len + '░' * (20 - bar_len)
        print(f"\r    [{bar}] {progress}% - 求解中", end='', flush=True)

    print()
    print(f"\n  [模拟] ✓ 求解完成，共处理 20 个事件")


# ================================================================
# 示例 6: 错误处理
# ================================================================

async def example_error_handling():
    """
    示例 6: 错误处理

    演示如何正确处理流中的错误事件。
    """
    print("\n" + "=" * 60)
    print("示例 6: 错误处理")
    print("=" * 60)

    try:
        from lv00 import Engine, AsyncStreamIterator
    except ImportError:
        print("警告: lv00 模块未安装，使用模拟模式")
        await _simulate_error_handling()
        return

    engine = Engine()

    async with AsyncStreamIterator(engine, timeout=10.0) as stream:
        async for event in stream:
            # 检查错误事件
            if event.is_error():
                print(f"  ⚠ 错误: {event.type}")
                print(f"    描述: {event.description}")

                # 根据错误类型决定处理方式
                if 'fatal' in event.type.lower():
                    print("    致命错误，停止处理")
                    break
                else:
                    print("    非致命错误，继续处理")
            else:
                print(f"  ✓ 正常: {event.type}")

            if event.is_complete():
                break


async def _simulate_error_handling():
    """模拟错误处理。"""
    events = [
        ('normalization_start', False),
        ('normalization_warning', False),
        ('solving_error', True),
        ('solving_retry', False),
        ('solving_complete', False),
    ]

    for event_type, is_error in events:
        await asyncio.sleep(0.3)
        if is_error:
            print(f"  [模拟] ⚠ 错误: {event_type}")
            print(f"    描述: 求解过程中发生错误")
            print("    非致命错误，继续处理")
        else:
            print(f"  [模拟] ✓ 正常: {event_type}")


# ================================================================
# 主函数
# ================================================================

async def main():
    """运行所有示例。"""
    print("\n" + "=" * 60)
    print("Lv-00 流式输出示例")
    print("=" * 60)

    examples = [
        ("基本异步迭代器", example_basic_iterator),
        ("事件类型过滤", example_event_filtering),
        ("批量事件处理", example_batch_processing),
        ("便捷函数", example_convenience_functions),
        ("完整工作流", example_complete_workflow),
        ("错误处理", example_error_handling),
    ]

    for name, func in examples:
        try:
            await func()
        except Exception as e:
            logger.error(f"示例 '{name}' 执行失败: {e}")

        # 示例间隔
        await asyncio.sleep(0.5)

    print("\n" + "=" * 60)
    print("所有示例执行完成！")
    print("=" * 60 + "\n")


if __name__ == '__main__':
    asyncio.run(main())
