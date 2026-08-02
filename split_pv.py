import re

SRC = 'core/src/layer4_reasoning/proof/proof_version.c'
DIR = 'core/src/layer4_reasoning/proof'

with open(SRC, encoding='utf-8-sig') as f:
    text = f.read()
lines = text.split('\n')
if lines and lines[-1] == '':
    lines.pop()
TOTAL = len(lines)
print(f'SRC total lines: {TOTAL}')

INCLUDES = '''#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv.h"
#include "lv/proof.h"
#include "lv/smt_backend.h"
#include "lv/thread_pool.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_str_utils.h"

#include "lv/lv_strbuf.h"'''

def module_header(name, brief):
    return f'''/**
 * @file {name}
 * @brief 证明版本管理与序列化 —— {brief}
 *
 * @details 由 proof_version.c 按功能域拆分而来。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

'''

SLICES = [
    ('proof_version_nl.c',      37,   221, '自然语言导出与策略注释'),
    ('proof_version_ghost.c',  224,   431, '幽灵标记与引导填充'),
    ('proof_version_sledge.c', 433,   700, 'Sledgehammer 自动证明策略调度'),
    ('proof_version_task.c',   702,   763, '任务系统与备选占位'),
    ('proof_version_isar.c',   763,  1495, 'Isar 导出与 HOL Light 微内核验证'),
    ('proof_version_refine.c',1497,  1618, 'F* 精化类型与 SMT 混合验证'),
]

for name, start, end, brief in SLICES:
    if start < 1 or end > TOTAL:
        print(f'!! BOUND OUT OF RANGE {name}: {start}-{end} (total {TOTAL})')
        continue
    body = lines[start - 1:end]
    out = module_header(name, brief) + INCLUDES + '\n\n' + '\n'.join(body) + '\n'
    path = f'{DIR}/{name}'
    with open(path, 'w', encoding='utf-8') as f:
        f.write(out)
    print(f'wrote {path}: {end - start + 1} body lines')

print('done')