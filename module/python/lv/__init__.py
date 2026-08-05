"""
Lv-00 Python Bindings (v1.1.0 — GMP unified)
"""
__version__ = "1.1.0"

# Try to load C bindings, fall back to pure Python
try:
    from lv._ctypes_binding import *  # noqa

    # re-export core 模块的核心类、异常、工具函数与常量，
    # 修复 `from lv import Graph` 等顶层导出（v3.3.0 起核心类定义于 lv.core）。
    from lv.core import (  # noqa: F401
        SymbolicCoord, Point, LineSegment, Graph, GeomNode, NormalizationResult,
        lvBaseError, lvError, lvLibraryError, lvArgumentError,
        lvConstraintError, lvSolverError,
        init, cleanup, get_version, get_last_error, set_debug_mode,
        set_log_level, reset_counters, get_counter_report,
        LOG_LEVEL_DEBUG, LOG_LEVEL_INFO, LOG_LEVEL_WARN, LOG_LEVEL_ERROR,
        GEOM_POINT, GEOM_LINE_SEGMENT, GEOM_PORT, GEOM_FUNCTION_BLOCK,
        CONSTRAINT_INCIDENCE, CONSTRAINT_BETWEENNESS, CONSTRAINT_INTERSECTION,
        CONSTRAINT_CONTAINMENT, CONSTRAINT_CONNECTION,
    )

    # re-export engine / async_stream（示例与旧代码使用 from lv import Engine 等）
    from lv.engine import (  # noqa: F401
        Engine, EngineError, EngineMemoryError, EngineStateError,
        EngineConflictError, EngineModuleError,
    )
    from lv.async_stream import (  # noqa: F401
        AsyncStreamIterator, BufferedStreamCollector,
    )
except ImportError:
    from lv.fallback import enable_fallback
    enable_fallback()
