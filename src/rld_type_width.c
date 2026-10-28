/* SPDX-License-Identifier: MIT */
/* Minimal Type width support for RLD's shared RO emitter object. */
#include "ast.h"

unsigned type_integer_width(const Type* type)
{
    if (!type) return 0u;
    if (type->is_bit_precise) return type->bit_width;
    if (type->kind == TYPE_ENUM) {
        if (type->enum_underlying_type)
            return type_integer_width(type->enum_underlying_type);
        return type->size > 0 ? (unsigned)type->size * 8u : 0u;
    }
    if (type->kind < TYPE_BOOL || type->kind > TYPE_LLONG) return 0u;
    if (type->kind == TYPE_BOOL) return 1u;
    return type->size > 0 ? (unsigned)type->size * 8u : 0u;
}
