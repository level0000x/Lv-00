"""
Lv-00 Python Bindings (v1.1.0 — GMP unified)
"""
__version__ = "1.1.0"

# Try to load C bindings, fall back to pure Python
try:
    from lv00._ctypes_binding import *  # noqa
except ImportError:
    from lv00.fallback import enable_fallback
    enable_fallback()
