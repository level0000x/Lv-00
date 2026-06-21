#ifndef LV00_EQUIV_CLASS_H
#define LV00_EQUIV_CLASS_H
/* TODO: Equiv class module stub */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/** Equivalence class manager. */
typedef struct Lv00EquivClass Lv00EquivClass;
/** Compatibility typedef for test code. */
typedef Lv00EquivClass EquivClassManager;
#define equiv_manager_create(n) lv00_equiv_class_create((n))

/** Create equiv class manager. */
Lv00EquivClass *lv00_equiv_class_create(size_t n_elements);
/** Union two elements. */
int lv00_equiv_class_union(Lv00EquivClass *ec, int a, int b);
/** Find representative. */
int lv00_equiv_class_find(Lv00EquivClass *ec, int a);

#ifdef __cplusplus
}
#endif

#endif
