/**
 * @file axiom_pkg_internal.h
 * @brief Internal shared definitions for axiom package system.
 */

#ifndef lv_AXIOM_PKG_INTERNAL_H
#define lv_AXIOM_PKG_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "axiom_pkg.h"
#include "lv_internal.h"

/* Compatibility macro: set_error -> lv_set_error */
#define set_error(fmt, ...) lv_set_error(lv_ERROR_INVALID_PARAM, (fmt), ##__VA_ARGS__)

/* Compatibility macros: short names for PropositionKind */
#ifndef CONSTRUCTIVE
#define CONSTRUCTIVE PROPOSITION_KIND_CONSTRUCTIVE
#endif
#ifndef NON_CONSTRUCTIVE_ORACLE
#define NON_CONSTRUCTIVE_ORACLE PROPOSITION_KIND_NON_CONSTRUCTIVE_ORACLE
#endif
#ifndef EXPLOSION_PRINCIPLE
#define EXPLOSION_PRINCIPLE PROPOSITION_KIND_EXPLOSION_PRINCIPLE
#endif

/* Constants */
#define AXIOM_SHA256_OUTPUT_SIZE 32
#define AXIOM_SHA256_HEX_SIZE 65
#define AXIOM_EXPANSION_CACHE_CAP 16
#define AXIOM_DEP_REF_CACHE_CAP 16
#define AXIOM_MAX_EXPANSION_DEPTH 8
#define AXIOM_MAX_FILE_SIZE (64 * 1024 * 1024)
#define AXIOM_MAX_PARTICIPANT_TYPES 8
#define AXIOM_PARTICIPANT_TYPE_LEN 32
#define AXIOM_TEST_MSG_BUF_SIZE 256
#define AXIOM_PARAM_DESC_MAX_LEN 64

/* Thread-local stream context (defined in axiom_pkg.c) */
extern lv_THREAD_LOCAL StreamContext *axiom_stream_ctx;
void axiom_package_set_stream_context(StreamContext *ctx);

/* Internal helper (defined in axiom_pkg.c) */
char *safe_lv_strdup_safe(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* lv_AXIOM_PKG_INTERNAL_H */
