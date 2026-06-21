# Lv-00 代码质量修复 — 全局任务上下文

**版本**: v1.1.0 → v1.1.1 | **创建**: 2026-06-22 | **审查覆盖**: 850 文件, 114 问题

---

## 总览

```
R0  ✅ 创建规划文档 (本文件)
R1  🔴 P0-Critical C: realloc NULL + mpq_get_str + include .c + pow bug
R2  🔴 P0-Build: CMakeLists 源文件列表 + .gitignore web/
R3  🔴 P0-Lean: 12 自反恒等式假证明 + 2 超大文件拆分
R4  🔴 P0-Python: 循环 import (ws_server ↔ stream_bridge)
R5  🟠 P1-High: stubs 类型不匹配 + expr_destroy 递归 + graph_remove 泄漏
R6  🟠 P1-High: Hilbert 重复 + Basic 重复 + rk4 证明 + CMake 头文件
R7  🟡 P2-Medium: atp 泄漏 + expr_eval 未初始化 + Python broad except + macOS 路径
R8  🟡 P2-Medium: .lv00 命名 + .gitignore .lake + README 版本 + lakefile 版本
R9  🟢 P3-Low: 收尾清理 (graph_clone 初始化, 未使用 import, 格式, 死链接)
```

---

## 规则

1. **每轮结束必须**: git add -A → git commit → git push origin master → git push origin main
2. **每轮结束提供**: 下一轮的完整提示词
3. **修复后验证**: 用 Grep 确认零残留

---

## 远程仓库

```
origin: https://github.com/level0000x/Lv-00.git
分支:   master + main (同步)
```

---

## R1 — P0-Critical C (进行中)

### 问题清单

1. `core/src/lv00_impl_native.c:340,374` — realloc 无 NULL 检查
2. `core/src/lv00_impl_native.c:185,282` — mpq_get_str 返回 NULL 直传 snprintf
3. 16 文件 — `#include ".c"` 反模式 (static 变量多拷贝)
4. `core/src/lv00_impl_native.c:613` — pow 逻辑错误 (平方而非累乘)

### 提示词
```
修复 R1 P0-Critical C 问题:

1. lv00_impl_native.c:340,374 — realloc 先赋值临时变量, NULL 时返回 -1 而不是覆盖原指针
2. lv00_impl_native.c:185,282 — mpq_get_str 结果 NULL 时用 "(null)" 替代
3. 16 个 #include ".c" — 改为创建 lv00_impl_upper.h 头文件, 所有 .c include .h 而不 include .c
   需要: 创建 core/include/lv00/lv00_impl_upper.h, 包含 lv00_impl_upper.c 中所有非 static 函数声明
4. lv00_impl_native.c:613 — pow 循环改为保存原始基数, 每次乘以基数而非自身平方

完成后 commit + push master + push main + 输出 R2 提示词
```

---

## 当前进度: R0 完成 → 开始 R1
