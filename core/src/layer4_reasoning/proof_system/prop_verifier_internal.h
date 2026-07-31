/**
 * @file prop_verifier_internal.h
 * @brief Internal shared definitions for proposition verifier module.
 */

#ifndef lv_PROP_VERIFIER_INTERNAL_H
#define lv_PROP_VERIFIER_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/prop_verifier.h"
#include "lv/lv_internal.h"
#include "lv/stream.h"
#include "lv/lv_strbuf.h"

/* ---- constants ---- */
#define MAX_PREMISES 64
#define MAX_GOALS 64
#define MAX_MEMO_ENTRIES 1024
#define MAX_FORMULA_STR 2048
#define MAX_COPY_DEPTH 200
#define MAX_DESTROY_DEPTH 200

#define PROP_DESTROY_STACK_INIT_CAP 64
#define PROP_DESTROY_STACK_GROWTH 2

#define PROP_PREC_ATOM 100
#define PROP_PREC_NEGATION 80
#define PROP_PREC_CONJUNCTION 60
#define PROP_PREC_DISJUNCTION 50
#define PROP_PREC_IMPLICATION 40
#define PROP_PREC_DEFAULT 0

#define PROP_HASH_TYPE_MULTIPLIER 2654435761U
#define PROP_HASH_STRING_MULTIPLIER 31
#define PROP_HASH_LEFT_MULTIPLIER 0x9e3779b9U
#define PROP_HASH_RIGHT_MULTIPLIER 0x517cc1b7U
#define PROP_HASH_PTR_MULTIPLIER 0x45d9f3bU
#define PROP_HASH_BIT_SHIFT 16
#define PROP_HASH_PREMISES_MULTIPLIER 31

#define PROP_TIME_MS_PER_SEC 1000

#define PROP_SMOKE_TEST_COUNT 13
#define PROP_SMOKE_MAX_PREM_PTRS 8
#define PROP_SMOKE_CLEANUP_MAX_PTRS 16
#define PROP_ATOM_NAME_MAX_LEN 64
#define PROP_ATOM_COLLECT_MAX 32
#define PROP_PATTERN_DESC_BUFSIZE 256
#define PROP_ANALYSIS_DESC_BUFSIZE 512
#define PROP_MISSING_LIST_BUFSIZE 512
#define PROP_STREAM_EVENT_BUFSIZE 256
#define PROP_JSON_DETAIL_BUFSIZE 192

#define PROP_TRUST_YELLOW_THRESHOLD 2
#define PROP_TRUST_AMBER_MIN 3

/* ---- internal data structures ---- */
typedef struct {
    const PropFormula *goal;
    uint64_t premises_hash;
    bool proven;
    bool searched;
} MemoEntry;

typedef struct {
    const PropFormula **premises;
    int premise_count;
    const VerifierConfig *config;
    int steps;
    bool timed_out;
    uint64_t start_time_ms;
    MemoEntry memo[MAX_MEMO_ENTRIES];
    int memo_count;
    int recursion_depth;
} ProofContext;

/* ---- thread-local stream context (defined in prop_verifier.c) ---- */
extern lv_THREAD_LOCAL StreamContext *prop_verifier_stream_ctx;

/* ---- cross-section helpers ---- */
bool formula_equal(const PropFormula *a, const PropFormula *b);
uint64_t get_time_ms(void);
uint64_t formula_hash(const PropFormula *f);
uint64_t premises_hash(const PropFormula **premises, int count);
int memo_find(ProofContext *ctx, const PropFormula *goal, uint64_t phash);
void memo_add(ProofContext *ctx, const PropFormula *goal, uint64_t phash, bool proven);
bool premise_contains(const PropFormula **premises, int count, const PropFormula *f);
bool prove(ProofContext *ctx, const PropFormula **premises, int premise_count, const PropFormula *goal);
int forward_chain_conjunctions(const PropFormula **input, int input_count, const PropFormula **output,
                             int output_capacity);
int collect_atoms(const PropFormula *f, char atoms[][PROP_ATOM_NAME_MAX_LEN], int max_atoms);
bool has_classical_pattern(const PropFormula *f, char *pattern_desc, size_t desc_size);


#ifdef __cplusplus
}
#endif

#endif /* lv_PROP_VERIFIER_INTERNAL_H */
