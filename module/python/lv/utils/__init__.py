"""
Lv-00 工具模块
==============

提供 Lv-00 核心模块的公共工具函数和装饰器。

功能：
    - 单例模式装饰器
    - 缓存机制
    - 数组限制工具
    - 类型检查工具
    - 资源管理工具

版本：3.5.0
作者：Lv-00 开发团队
"""

import threading
import time
from typing import Any, Callable, Dict, Generic, List, Optional, TypeVar

# ============================================================
# 单例模式工具
# ============================================================

T = TypeVar('T')


def singleton(cls: type) -> type:
    """
    单例模式装饰器。

    确保被装饰的类只有一个实例，并提供线程安全的延迟初始化。

    参数：
        cls: 要被转换为单例的类

    返回：
        type: 单例类

    示例：
        >>> @singleton
        ... class MyClass:
        ...     def __init__(self):
        ...         self.value = 42
        >>> a = MyClass()
        >>> b = MyClass()
        >>> a is b
        True
    """
    instances: Dict[type, Any] = {}
    lock = threading.Lock()

    def wrapper(*args: Any, **kwargs: Any) -> Any:
        if cls not in instances:
            with lock:
                # 双重检查锁定模式
                if cls not in instances:
                    instances[cls] = cls(*args, **kwargs)
        return instances[cls]

    # 保留原始类的元数据
    wrapper.__name__ = cls.__name__
    wrapper.__doc__ = cls.__doc__
    wrapper.__module__ = cls.__module__

    return wrapper


# ============================================================
# 缓存工具
# ============================================================

class TTLCache(Generic[T]):
    """
    带过期时间的缓存类。

    支持为每个缓存项设置独立的过期时间（TTL）。

    属性：
        default_ttl: 默认过期时间（秒）

    示例：
        >>> cache = TTLCache[str](default_ttl=3600)  # 1小时过期
        >>> cache.set("key", "value")
        >>> cache.get("key")
        'value'
    """

    def __init__(self, default_ttl: int = 3600) -> None:
        """
        创建 TTL 缓存。

        参数：
            default_ttl: 默认过期时间（秒），默认 3600（1小时）
        """
        self.default_ttl = default_ttl
        self._cache: Dict[str, Dict[str, Any]] = {}
        self._lock = threading.RLock()

    def get(self, key: str) -> Optional[T]:
        """
        获取缓存值。

        如果缓存已过期，返回 None 并删除该缓存项。

        参数：
            key: 缓存键

        返回：
            Optional[T]: 缓存值，不存在或已过期返回 None
        """
        with self._lock:
            if key not in self._cache:
                return None

            entry = self._cache[key]
            if time.time() > entry['expires']:
                del self._cache[key]
                return None

            return entry['value']

    def set(self, key: str, value: T, ttl: Optional[int] = None) -> None:
        """
        设置缓存值。

        参数：
            key: 缓存键
            value: 缓存值
            ttl: 过期时间（秒），None 使用默认值
        """
        with self._lock:
            self._cache[key] = {
                'value': value,
                'expires': time.time() + (ttl if ttl is not None else self.default_ttl)
            }

    def delete(self, key: str) -> bool:
        """
        删除缓存项。

        参数：
            key: 缓存键

        返回：
            bool: 删除成功返回 True，不存在返回 False
        """
        with self._lock:
            if key in self._cache:
                del self._cache[key]
                return True
            return False

    def clear(self) -> None:
        """清空所有缓存。"""
        with self._lock:
            self._cache.clear()

    def cleanup_expired(self) -> int:
        """
        清理过期缓存项。

        返回：
            int: 清理的缓存项数量
        """
        with self._lock:
            now = time.time()
            expired_keys = [
                key for key, entry in self._cache.items()
                if now > entry['expires']
            ]
            for key in expired_keys:
                del self._cache[key]
            return len(expired_keys)


# ============================================================
# 数组限制工具
# ============================================================

def limit_array_length(
    array: List[T],
    max_length: int,
    from_end: bool = True
) -> List[T]:
    """
    限制数组长度。

    当数组长度超过限制时，从头部或尾部截断。

    参数：
        array: 原始数组
        max_length: 最大长度
        from_end: True 保留尾部（删除头部），False 保留头部

    返回：
        List[T]: 截断后的数组（可能返回原数组引用）

    示例：
        >>> limit_array_length([1, 2, 3, 4, 5], 3)
        [3, 4, 5]
        >>> limit_array_length([1, 2, 3, 4, 5], 3, from_end=False)
        [1, 2, 3]
    """
    if len(array) <= max_length:
        return array

    if from_end:
        return array[-max_length:]
    else:
        return array[:max_length]


def safe_append_limited(
    array: List[T],
    item: T,
    max_length: int
) -> List[T]:
    """
    安全地添加元素到数组，并保持长度限制。

    参数：
        array: 目标数组
        item: 要添加的元素
        max_length: 最大长度

    返回：
        List[T]: 更新后的数组

    示例：
        >>> arr = [1, 2, 3]
        >>> safe_append_limited(arr, 4, 3)
        [2, 3, 4]
    """
    array.append(item)
    if len(array) > max_length:
        return array[-max_length:]
    return array


# ============================================================
# 类型检查工具
# ============================================================

def has_valid_ptr(obj: Any) -> bool:
    """
    检查对象是否具有有效的 _ptr 属性。

    用于验证 C 绑定对象的指针有效性。

    参数：
        obj: 要检查的对象

    返回：
        bool: 具有有效 _ptr 属性返回 True
    """
    return (
        obj is not None and
        hasattr(obj, '_ptr') and
        obj._ptr is not None
    )


def validate_ptr_or_raise(
    obj: Any,
    param_name: str = "obj"
) -> None:
    """
    验证对象具有有效的 _ptr 属性，否则抛出异常。

    参数：
        obj: 要检查的对象
        param_name: 参数名称（用于错误消息）

    异常：
        TypeError: 对象没有有效的 _ptr 属性
    """
    if not has_valid_ptr(obj):
        raise TypeError(
            f"{param_name} 必须具有有效的 _ptr 属性（C 指针），"
            f"收到类型: {type(obj).__name__}"
        )


# ============================================================
# 资源管理工具
# ============================================================

class ResourceGuard:
    """
    资源保护上下文管理器。

    确保资源在使用后被正确释放，即使发生异常。

    示例：
        >>> with ResourceGuard(create_resource, destroy_resource) as res:
        ...     use_resource(res)
    """

    def __init__(
        self,
        create_fn: Callable[[], T],
        destroy_fn: Callable[[T], None]
    ) -> None:
        """
        创建资源保护器。

        参数：
            create_fn: 创建资源的函数
            destroy_fn: 销毁资源的函数
        """
        self._create_fn = create_fn
        self._destroy_fn = destroy_fn
        self._resource: Optional[T] = None

    def __enter__(self) -> T:
        self._resource = self._create_fn()
        return self._resource

    def __exit__(self, exc_type: Any, exc_val: Any, exc_tb: Any) -> None:
        if self._resource is not None:
            try:
                self._destroy_fn(self._resource)
            except Exception:
                pass


# ============================================================
# 导出
# ============================================================

__all__ = [
    # 单例模式
    'singleton',
    # 缓存
    'TTLCache',
    # 数组限制
    'limit_array_length',
    'safe_append_limited',
    # 类型检查
    'has_valid_ptr',
    'validate_ptr_or_raise',
    # 资源管理
    'ResourceGuard',
]
