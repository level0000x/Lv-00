# -*- coding: utf-8 -*-
"""临时修复脚本：_ctypes_binding.py 结构体不透明化 + B1-1 符号名；core.py NormalizationResult 属性对齐。"""
import io
import sys

BINDING = r"module/python/lv/_ctypes_binding.py"
CORE = r"module/python/lv/core.py"

# ---------------------------------------------------------------
# _ctypes_binding.py：结构体类整块替换（_NormalizationResult 除外）
# ---------------------------------------------------------------
new_classes = {
"_SymbolicCoord": '''class _SymbolicCoord(ctypes.Structure):
    """
    符号坐标的不透明句柄。

    对应 C 层 SymbolicCoord 类型。Python 侧不直接解引用内部字段，
    统一通过 _lib.symbolic_coord_* 系列 C 函数读写。
    """
    _fields_ = [
        ("_opaque", c_void_p),
    ]
''',
"_ConstraintGraph": '''class _ConstraintGraph(ctypes.Structure):
    """
    约束图的不透明句柄。

    对应 C 层 ConstraintGraph 类型。Python 侧不直接解引用内部字段，
    统一通过 _lib.graph_* 系列 C 函数访问。
    """
    _fields_ = [
        ("_opaque", c_void_p),
    ]
''',
"_GeomNode": '''class _GeomNode(ctypes.Structure):
    """
    几何节点的 C 结构体（仅保留被 Python 侧解引用的前两字段）。

    对应 C 层 GeomNode 类型（constraint_graph.h）。Python 侧仅解引用
    id/type 两个字段，恰好对应 C 布局的前两字段（int id / GeomType type），
    其余内部字段统一通过 C 函数访问。
    """
    _fields_ = [
        ("id", c_int),
        ("type", c_int),
    ]
''',
"_Constraint": '''class _Constraint(ctypes.Structure):
    """
    约束的不透明句柄。

    对应 C 层 Constraint 类型。Python 侧不直接解引用内部字段，
    统一通过 _lib.graph_* 系列 C 函数访问。
    """
    _fields_ = [
        ("_opaque", c_void_p),
    ]
''',
"_FuncBlock": '''class _FuncBlock(ctypes.Structure):
    """
    函数块的不透明句柄。

    对应 C 层 FuncBlock 类型。Python 侧不直接解引用内部字段，
    统一通过 _lib.func_block_* 系列 C 函数访问。
    """
    _fields_ = [
        ("_opaque", c_void_p),
    ]
''',
"_ProofNavigator": '''class _ProofNavigator(ctypes.Structure):
    """
    证明导航器的不透明句柄。

    对应 C 层 ProofNavigator 类型。Python 侧不直接解引用内部字段，
    统一通过 _lib.proof_* 系列 C 函数访问。
    """
    _fields_ = [
        ("_opaque", c_void_p),
    ]
''',
"_Proposition": '''class _Proposition(ctypes.Structure):
    """
    命题的不透明句柄。

    对应 C 层 Proposition 类型。Python 侧不直接解引用内部字段，
    统一通过 _lib.proposition_* / _lib.proof_* 系列 C 函数访问。
    """
    _fields_ = [
        ("_opaque", c_void_p),
    ]
''',
"_lvEngine": '''class _lvEngine(ctypes.Structure):
    """
    引擎的不透明句柄。

    对应 C 层 lvEngine 类型。Python 侧不直接解引用内部字段，
    统一通过 _lib.engine_* 系列 C 函数访问。
    """
    _fields_ = [
        ("_opaque", c_void_p),
    ]
''',
"_MeasureSystem": '''class _MeasureSystem(ctypes.Structure):
    """
    测度系统的不透明句柄。

    对应 C 层 MeasureSystem 类型。Python 侧不直接解引用内部字段，
    统一通过 _lib.measure_* 系列 C 函数访问。
    """
    _fields_ = [
        ("_opaque", c_void_p),
    ]
''',
"_RecursionContext": '''class _RecursionContext(ctypes.Structure):
    """
    递归上下文的不透明句柄。

    对应 C 层 RecursionContext 类型。Python 侧不直接解引用内部字段，
    统一通过 _lib.recursion_* 系列 C 函数访问。
    """
    _fields_ = [
        ("_opaque", c_void_p),
    ]
''',
"_Port": '''class _Port(ctypes.Structure):
    """
    端口的不透明句柄。

    对应 C 层 Port 类型。Python 侧不直接解引用内部字段，
    统一通过 _lib.graph_* 系列 C 函数访问。
    """
    _fields_ = [
        ("_opaque", c_void_p),
    ]
''',
}

recursion_test_result_cls = '''class _RecursionTestResult(ctypes.Structure):
    """
    递归内置测试结果结构体（recursion.h:649-653）。

    C 层预期布局：
        char name[64];       // 测试名称
        bool passed;         // 是否通过
        char error_msg[128]; // 错误信息（passed 为 false 时有效）
    """
    _fields_ = [
        ("name", c_char * 64),
        ("passed", c_bool),
        ("error_msg", c_char * 128),
    ]
'''

old_b1_1 = (
    "getattr(_lib, 'recursion_run_builtin_test/c').argtypes = "
    "[POINTER(_MeasureSystem), POINTER(POINTER(c_void_p)), POINTER(c_int)]\n"
    "getattr(_lib, 'recursion_run_builtin_test/c').restype = c_int"
)
new_b1_1 = (
    "# 运行递归模块内置测试套件（recursion.h:715）\n"
    "_lib.recursion_run_builtin_tests.argtypes = "
    "[POINTER(_MeasureSystem), POINTER(POINTER(_RecursionTestResult)), POINTER(c_int)]\n"
    "_lib.recursion_run_builtin_tests.restype = c_int"
)


def replace_class(text, clsname, new_body):
    marker = "class %s(ctypes.Structure):" % clsname
    start = text.index(marker)
    end = text.index("\nclass ", start + 1)
    return text[:start] + new_body + text[end:]


def must_replace(text, old, new, what):
    cnt = text.count(old)
    assert cnt == 1, "expected exactly 1 occurrence of %s, found %d" % (what, cnt)
    return text.replace(old, new)


def main():
    with io.open(BINDING, "r", encoding="utf-8", newline="") as f:
        text = f.read()

    # 1) 替换 12 个结构体类（_NormalizationResult 保持精确布局，不动）
    for name, body in new_classes.items():
        text = replace_class(text, name, body)

    # 2) 在 _Port 之后、SymbolicCoord 函数签名段之前插入 _RecursionTestResult
    section_marker = "\n# ============================================================\n# SymbolicCoord 函数签名"
    assert text.count(section_marker) == 1
    text = text.replace(section_marker, "\n" + recursion_test_result_cls + section_marker, 1)

    # 3) B1-1 符号名与 argtypes 修复
    text = must_replace(text, old_b1_1, new_b1_1, "recursion_run_builtin_test 绑定段")

    with io.open(BINDING, "w", encoding="utf-8", newline="") as f:
        f.write(text)
    print("binding ok, lines:", text.count("\n"))

    # ---------------------------------------------------------------
    # core.py：NormalizationResult 属性层对齐
    # ---------------------------------------------------------------
    with io.open(CORE, "r", encoding="utf-8", newline="") as f:
        ctext = f.read()

    old_props_start = "    @property\n    def merged_count(self) -> int:"
    idx_start = ctext.index(old_props_start)
    # 属性块结束于 success 属性（其后是空行 + Graph 类注释段）
    tail_marker = "            return False\n\n\n# ============================================================\n# Graph 类"
    idx_end = ctext.index(tail_marker, idx_start) + len("            return False\n")
    old_block = ctext[idx_start:idx_end]

    new_block = '''    @property
    def merged_count(self) -> int:
        """被合并的等价节点数量（对应 C 结构体 merged_count 字段）。"""
        if self._ptr is None:
            return 0
        try:
            return int(self._ptr.contents.merged_count)
        except Exception:
            logger.debug("Property access failed", exc_info=True)
            return 0

    @property
    def original_ids(self) -> list:
        """
        被合并的原始节点 ID 列表（对应 C 结构体 original_ids 数组，
        有效长度由 merged_count 字段决定）。
        """
        if self._ptr is None:
            return []
        try:
            count = int(self._ptr.contents.merged_count)
            raw = self._ptr.contents.original_ids
            if not raw or count <= 0:
                return []
            arr = ctypes.cast(raw, ctypes.POINTER(ctypes.c_int))
            return [int(arr[i]) for i in range(count)]
        except Exception:
            logger.debug("Property access failed", exc_info=True)
            return []

    @property
    def representative_ids(self) -> list:
        """
        代表节点 ID 列表（对应 C 结构体 representative_ids 数组，
        有效长度由 merged_count 字段决定）。
        """
        if self._ptr is None:
            return []
        try:
            count = int(self._ptr.contents.merged_count)
            raw = self._ptr.contents.representative_ids
            if not raw or count <= 0:
                return []
            arr = ctypes.cast(raw, ctypes.POINTER(ctypes.c_int))
            return [int(arr[i]) for i in range(count)]
        except Exception:
            logger.debug("Property access failed", exc_info=True)
            return []

    @property
    def user_confirmed(self) -> bool:
        """归一化是否经过用户确认（对应 C 结构体 user_confirmed 字段）。"""
        if self._ptr is None:
            return False
        try:
            return bool(self._ptr.contents.user_confirmed)
        except Exception:
            logger.debug("Property access failed", exc_info=True)
            return False

    @property
    def success(self) -> bool:
        """归一化是否成功（C 结构体无 success 字段，映射到 user_confirmed）。"""
        return self.user_confirmed
'''

    assert old_block.count("@property") == 7, "unexpected property block shape: %d" % old_block.count("@property")
    ctext = ctext[:idx_start] + new_block + ctext[idx_end:]
    assert "simplified_constraints" not in ctext
    assert "removed_nodes" not in ctext
    assert "iterations" not in ctext
    assert "contents.merged_nodes" not in ctext
    assert "contents.success" not in ctext

    with io.open(CORE, "w", encoding="utf-8", newline="") as f:
        f.write(ctext)
    print("core ok, lines:", ctext.count("\n"))


if __name__ == "__main__":
    main()
