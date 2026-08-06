"""
Lv-00 Python 绑定公共辅助模块（绑定层收敛单点）。

统一资源生命周期管理与 ctypes 数据转换样板，供 module/python/lv/ 下
各绑定模块（core / engine / func_block / type_system / formula / ...）复用，
消除 11 个文件中的重复模式。

包含：
    - _PtrOwner      ：C 指针所有权混入类，统一 __del__ / _release 语义
    - _call_ok       ：C 调用返回码检查（result != ok 时抛异常）
    - _call_truthy   ：C 调用真值检查（falsy 结果时抛异常）
    - _int_arr       ：整数列表 → ctypes.c_int 数组
    - _c_arr_to_list ：C 整数数组 → Python 列表（可选释放）
    - _str_enc       ：统一字符串 → UTF-8 字节串编码

设计原则：
    1. 单一实现：析构、错误检查、数组构造各只写一次
    2. 行为保持：异常类型/消息、释放路径与原实现逐处等价
    3. 解释器关闭安全：遵循 PEP 442，关闭期静默跳过清理
    4. 零耦合：本模块不依赖 _ctypes_binding / _lib，
       销毁函数与释放函数由调用方显式传入
"""

import ctypes
import logging
import sys

_logger = logging.getLogger(__name__)


# ============================================================
# 解释器关闭检测（PEP 442）
# ============================================================

def _is_shutting_down() -> bool:
    """检测 Python 解释器是否正在关闭。

    当模块对象被 None 替换时为关闭状态，此时不应执行清理操作。
    参考 PEP 442 (CPython 3.4+) 的终结机制。
    """
    import sys as _check_sys
    return _check_sys.modules.get(__name__) is None


# ============================================================
# _PtrOwner：C 指针所有权混入类
# ============================================================

class _PtrOwner:
    """C 指针所有权混入类。

    统一 __del__ 语义：拥有指针时调用 destroy_fn 释放底层 C 资源，
    并在 Python 解释器关闭期间安全跳过清理（PEP 442）。

    子类约定：
        - 在 __init__ 中调用 ``super().__init__(ptr, destroy_fn, owns_ptr)``
          （或直接 ``_PtrOwner.__init__(self, ...)``）；
        - 通过 ``cls.__new__`` 创建后，同样显式调用 ``_PtrOwner.__init__``
          以登记销毁函数与所有权；
        - 类属性 ``_PTR_LOG`` 控制析构失败时的记录方式：
            "silent"  静默（func_block/type_system/groebner 等原行为）
            "logging" 用 logging.warning（SymbolicCoord 原行为）
            "stderr"  打印到 stderr（Graph/NormalizationResult 原行为）

    属性：
        _ptr: 底层 C 指针
        _destroy_fn: 销毁函数（接收 _ptr 并释放），可为 None
        _owns_ptr: 是否拥有指针所有权（析构时决定是否释放）
    """

    _PTR_LOG = "silent"  # silent / logging / stderr

    def __init__(self, ptr: 'ctypes.c_void_p' = None,
                 destroy_fn: 'callable' = None,
                 owns_ptr: bool = True) -> None:
        """登记指针、销毁函数与所有权。

        参数：
            ptr: 底层 C 指针（可为 None）
            destroy_fn: 销毁函数，签名为 destroy_fn(ptr)；None 表示不释放
            owns_ptr: 是否拥有所有权（默认 True）
        """
        self._ptr = ptr
        self._destroy_fn = destroy_fn
        self._owns_ptr = owns_ptr

    def _release(self) -> None:
        """释放底层 C 指针（幂等）。

        未登记指针、不拥有所有权或无销毁函数时为空操作；
        销毁失败时按 _PTR_LOG 策略记录，但指针仍会被置空，
        避免解释器关闭期间重复进入导致崩溃。
        """
        ptr = getattr(self, '_ptr', None)
        if ptr is None:
            return
        # 先置空再销毁，确保即使销毁抛出异常也不会二次释放
        self._ptr = None
        destroy_fn = getattr(self, '_destroy_fn', None)
        if not getattr(self, '_owns_ptr', True) or destroy_fn is None:
            return
        try:
            destroy_fn(ptr)
        except Exception as e:  # noqa: BLE001 - 析构路径必须捕获所有异常
            if _is_shutting_down():
                return
            mode = getattr(self, '_PTR_LOG', "silent")
            if mode == "logging":
                _logger.warning("%s 析构失败：%s", type(self).__name__, e)
            elif mode == "stderr":
                print(f"[lv] 警告：{type(self).__name__} 析构失败：{e}",
                      file=sys.stderr)
            # "silent"：不记录

    def __del__(self) -> None:
        """析构函数：释放底层 C 分配的内存。"""
        try:
            self._release()
        except Exception:  # noqa: BLE001 - 终结器路径必须绝对安全
            pass


# ============================================================
# C 调用错误检查
# ============================================================

def _call_ok(func, *args, ok: int = 0, exc_cls: type = RuntimeError,
             msg: str = "", errfmt: 'callable' = None, **kwargs):
    """调用 C 函数并检查返回码。

    参数：
        func: 被调用的 C 函数
        ok: 期望的成功返回码（默认 0）
        exc_cls: 失败时抛出的异常类，须接受单个字符串参数
        msg: 错误消息前缀
        errfmt: 可选的自定义消息格式化函数，签名为 errfmt(result) -> str；
                默认生成 f"{msg}: 错误码 {result}"

    返回：
        func 的返回值（此时必然等于 ok）

    示例：
        _call_ok(_lib.graph_add_incidence, self._ptr, pid, lid,
                 exc_cls=lvConstraintError, msg="添加关联约束失败")
    """
    result = func(*args, **kwargs)
    if result != ok:
        if errfmt is not None:
            raise exc_cls(errfmt(result))
        raise exc_cls(f"{msg}: 错误码 {result}")
    return result


def _call_truthy(func, *args, exc_cls: type = RuntimeError,
                 msg: str = "", **kwargs):
    """调用返回布尔值/指针的 C 函数，结果 falsy 时抛异常。

    参数：
        func: 被调用的 C 函数
        exc_cls: 失败时抛出的异常类，须接受单个字符串参数
        msg: 失败时的错误消息

    返回：
        func 的返回值（此时必然为真值）
    """
    result = func(*args, **kwargs)
    if not result:
        raise exc_cls(msg)
    return result


# ============================================================
# ctypes 数据转换
# ============================================================

def _int_arr(lst):
    """构造 ctypes 的 c_int 数组（整型列表 → 数组）。"""
    return (ctypes.c_int * len(lst))(*lst)


def _c_arr_to_list(ptr, n: int, free_fn: 'callable' = None):
    """读取 C 整数数组为 Python 列表，并（可选）释放内存。

    参数：
        ptr: C 数组指针
        n: 元素数量
        free_fn: 释放函数（如 _lib.lv_free_ptr）；None 表示不释放

    返回：
        List[int]：读取到的元素列表

    说明：
        即使读取过程抛异常，也会在 finally 中释放内存，
        与原 try-finally 模式保持一致。
    """
    if not ptr or n <= 0:
        if free_fn is not None and ptr:
            free_fn(ptr)
        return []
    try:
        return [ptr[i] for i in range(n)]
    finally:
        if free_fn is not None:
            free_fn(ptr)


def _str_enc(s: str) -> bytes:
    """统一字符串 → UTF-8 字节串编码。"""
    return s.encode('utf-8')
