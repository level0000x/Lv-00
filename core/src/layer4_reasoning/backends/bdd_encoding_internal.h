#ifndef BDD_ENCODING_INTERNAL_H
#define BDD_ENCODING_INTERNAL_H

#include "lv/bdd_encoding.h" /* BDDNode / BDDManager */

/* 定义在 bdd_encoding.c（核心文件）：墓碑标记与唯一表哈希（sifting 子模块复用） */

/** 墓碑标记 —— 用于开放寻址哈希表中标记已删除的槽位，保护探查链 */
extern BDDNode bdd_tombstone_marker;
#define BDD_TOMBSTONE (&bdd_tombstone_marker)

/** 节点三元组哈希 (var_id, low, high) -> 唯一表索引
 *  exempt: 判据「哈希表族收敛」——唯一表为三元组键 (var_id, low, high) +
 *  墓碑 + 引用计数的开放寻址表，语义不同于 lv_hashtable_int_hash 的整型键，保留。
 */
extern int bdd_unique_hash(int var_id, BDDNode *low, BDDNode *high, int table_size);

#endif /* BDD_ENCODING_INTERNAL_H */
