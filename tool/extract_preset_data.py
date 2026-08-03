#!/usr/bin/env python3
"""
提取所有 preset_xxx.c 文件中的注册数据，生成 preset_registry.yaml。
支持多种注册模式：
  1. LV_PRESET_REGISTER(success_count, NAME, desc, in_cnt, out_type, math_def, comp, cons, rev, inputs...)
  2. PRESET_REGISTER_CAT_COUNTED(cnt, name, desc, cat, inputs, in_cnt, out, math_def, comp, cons, rev)
  3. 自定义 helper 函数 + register_xxx_preset(name, desc, inputs, in_cnt, out, math_def, comp, cons, rev)
  4. preset_blocks_register_by_category(name, desc, cat, in_cnt, out_cnt)
  5. preset_blocks_register_simple(name, desc, cat, inputs, in_cnt, out, math_def, comp, cons, rev)
"""

import os
import re
import sys

PROJECT_ROOT = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00"
PRESET_DIR = os.path.join(PROJECT_ROOT, "core", "src", "layer4_reasoning", "preset")
FUNC_BLOCK_DIR = os.path.join(PROJECT_ROOT, "core", "src", "layer4_reasoning", "func_block")
OUTPUT_YAML = os.path.join(PROJECT_ROOT, "core", "src", "layer4_reasoning", "preset", "preset_registry.yaml")

# PresetType 枚举映射
PRESET_TYPE_MAP = {
    "PRESET_TYPE_POINT": "POINT",
    "PRESET_TYPE_LINE": "LINE",
    "PRESET_TYPE_LINE_SEGMENT": "LINE_SEGMENT",
    "PRESET_TYPE_RAY": "RAY",
    "PRESET_TYPE_CIRCLE": "CIRCLE",
    "PRESET_TYPE_POLYGON": "POLYGON",
    "PRESET_TYPE_ANGLE": "ANGLE",
    "PRESET_TYPE_SCALAR": "SCALAR",
    "PRESET_TYPE_VECTOR": "VECTOR",
    "PRESET_TYPE_MATRIX": "MATRIX",
    "PRESET_TYPE_BOOLEAN": "BOOLEAN",
    "PRESET_TYPE_INTEGER": "INTEGER",
    "PRESET_TYPE_SET": "SET",
    "PRESET_TYPE_FUNCTION": "FUNCTION",
    "PRESET_TYPE_TUPLE": "TUPLE",
    "PRESET_TYPE_LIST": "LIST",
    "PRESET_TYPE_SEQUENCE": "SEQUENCE",
    "PRESET_TYPE_REGION": "REGION",
    "PRESET_TYPE_PATH": "PATH",
    "PRESET_TYPE_SURFACE": "SURFACE",
    "PRESET_TYPE_SPACE": "SPACE",
    "PRESET_TYPE_GROUP": "GROUP",
    "PRESET_TYPE_GROUP_ELEMENT": "GROUP_ELEMENT",
    "PRESET_TYPE_SUBGROUP": "SUBGROUP",
    "PRESET_TYPE_HO