/*
 * RCC++ - Stable exception type identity for the scalar exception ABI
 */

#ifndef RCC_CXX_EXCEPTION_TYPE_H
#define RCC_CXX_EXCEPTION_TYPE_H

#include "ast.h"

/* Keep one bit of the target-width type word for the ownership convention
 * used by trivially-copyable aggregate exceptions.  The remaining 31 bits
 * carry the deterministic structural type identity on both supported x86
 * targets, so the same exception can cross an i686/AMD64 library boundary
 * without depending on a host pointer. */
#define RCC_CXX_EXCEPTION_OBJECT_FLAG UINT64_C(0x80000000)

static inline const Type* rcc_cxx_exception_match_type(const Type* type) {
    while (type && type->kind == TYPE_PTR && type->is_reference) {
        type = type->base;
    }
    return type;
}

/* The current RinOS exception frame carries one target-width type word.  A
 * TypeKind alone is not an exception type identity: signedness, enum identity,
 * and pointer pointee type all participate in C++ catch matching.  Keep this
 * hash deterministic so a throw in one translation unit can be matched by a
 * handler in another one without embedding a host pointer in the image. */
static inline void rcc_cxx_exception_hash_byte(uint64_t* hash,
                                               unsigned char byte) {
    *hash ^= byte;
    *hash *= UINT64_C(1099511628211);
}

static inline void rcc_cxx_exception_hash_text(uint64_t* hash,
                                               const char* text) {
    const unsigned char* cursor = (const unsigned char*)(text ? text : "");
    while (*cursor) rcc_cxx_exception_hash_byte(hash, *cursor++);
    rcc_cxx_exception_hash_byte(hash, 0u);
}

static inline void rcc_cxx_exception_hash_type(uint64_t* hash,
                                               const Type* type,
                                               bool top_level,
                                               unsigned depth) {
    if (!type || depth >= 32u) {
        rcc_cxx_exception_hash_byte(hash, 0xffu);
        return;
    }
    rcc_cxx_exception_hash_byte(hash, (unsigned char)type->kind);
    if (type->kind == TYPE_PTR) {
        /* Top-level cv-qualification on the pointer object is discarded by
         * throw/catch value semantics; pointee qualification is retained. */
        rcc_cxx_exception_hash_type(hash, type->base, false, depth + 1u);
        return;
    }
    if (type->kind == TYPE_ENUM) {
        rcc_cxx_exception_hash_text(hash, type->enum_tag);
        rcc_cxx_exception_hash_byte(hash, type->enum_is_scoped ? 1u : 0u);
    } else if (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) {
        rcc_cxx_exception_hash_text(hash, type->cxx_namespace);
        rcc_cxx_exception_hash_text(hash, type->tag);
    } else {
        rcc_cxx_exception_hash_byte(hash, type->is_unsigned ? 1u : 0u);
        rcc_cxx_exception_hash_byte(hash, (unsigned char)type->size);
    }
    if (!top_level) {
        rcc_cxx_exception_hash_byte(hash, type->is_const ? 1u : 0u);
        rcc_cxx_exception_hash_byte(hash, type->is_volatile ? 1u : 0u);
    }
}

static inline uint64_t rcc_cxx_exception_type_tag(const Type* type) {
    uint64_t hash = UINT64_C(1469598103934665603);
    /* A catch reference binds to the thrown object; the reference declarator
     * is not part of the exception's dynamic type identity. */
    type = rcc_cxx_exception_match_type(type);
    rcc_cxx_exception_hash_type(&hash, type, true, 0u);
    hash &= UINT64_C(0x7fffffff);
    /* Zero is reserved for an absent runtime type. */
    if (hash == 0u) hash = UINT64_C(1);
    if (type && (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION)) {
        hash |= RCC_CXX_EXCEPTION_OBJECT_FLAG;
    }
    return hash;
}

#endif
