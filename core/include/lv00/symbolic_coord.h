#ifndef LV00_SYMBOLIC_COORD_H
#define LV00_SYMBOLIC_COORD_H
/* TODO: Symbolic coord module stub */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <gmp.h>

/** Trust color enum — from symbolic_coord.h per float_error.h doc. */
typedef enum {
    TRUST_GREEN  = 0,
    TRUST_BLUE   = 0,  /**< alias for green */
    TRUST_YELLOW = 1,
    TRUST_ORANGE = 2,
    TRUST_AMBER  = 2,  /**< alias for orange */
    TRUST_RED    = 3
} TrustColor;

/** Light orange subtype. */
typedef enum {
    LO_NONE = 0, LO_MEMORY = 1, LO_PERFORMANCE = 2, LO_NUMERIC = 3
} LightOrangeSubtype;

/** Symbolic coordinate type. */
typedef struct Lv00SymbolicCoord {
    mpq_t x, y;
    int dim;
    TrustColor trust;
} Lv00SymbolicCoord;

/** Compatibility typedef for constraint_graph.h */
typedef Lv00SymbolicCoord SymbolicCoord;

/* === Lifecycle === */
SymbolicCoord *symbolic_coord_create_rational(int num, int den);
SymbolicCoord *symbolic_coord_copy(const SymbolicCoord *c);
void symbolic_coord_destroy(SymbolicCoord *c);

/* === Arithmetic === */
SymbolicCoord *symbolic_coord_add(const SymbolicCoord *a, const SymbolicCoord *b);
SymbolicCoord *symbolic_coord_subtract(const SymbolicCoord *a, const SymbolicCoord *b);
SymbolicCoord *symbolic_coord_multiply(const SymbolicCoord *a, const SymbolicCoord *b);
SymbolicCoord *symbolic_coord_divide(const SymbolicCoord *a, const SymbolicCoord *b);
SymbolicCoord *symbolic_coord_negate(const SymbolicCoord *c);
SymbolicCoord *symbolic_coord_sqrt(const SymbolicCoord *c);
SymbolicCoord *symbolic_coord_pow(const SymbolicCoord *c, int exp);

/* === Comparison & queries === */
int symbolic_coord_compare(const SymbolicCoord *a, const SymbolicCoord *b);
bool symbolic_coord_is_zero(const SymbolicCoord *c);
bool symbolic_coord_is_positive(const SymbolicCoord *c);
bool symbolic_coord_is_negative(const SymbolicCoord *c);
bool symbolic_coord_is_amber(const SymbolicCoord *c);

/* === Conversion === */
double symbolic_coord_to_double(const SymbolicCoord *c);

/* === Serialization === */
char *symbolic_coord_serialize(const SymbolicCoord *c);

/* === Trust color === */
TrustColor symbolic_coord_get_trust(const SymbolicCoord *c);
void symbolic_coord_set_trust(SymbolicCoord *c, TrustColor t);

/* === Hashing === */
uint64_t symbolic_coord_hash(const SymbolicCoord *c);

#ifdef __cplusplus
}
#endif
#endif
