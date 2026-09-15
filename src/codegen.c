/*
 * RCC - RinOS C Compiler
 * x86-32 Code Generator
 */

#include "rcc.h"
#include "ast.h"
#include "ast_cxx.h"
#include "symtab.h"
#include "codegen.h"
#include "cxx_exception_type.h"
#include <limits.h>

/* The C-only executable deliberately does not link the C++ frontend.  The
 * vtable pass is therefore an optional extension boundary, not a second
 * implementation or a fake namespace.  Use a weak function rather than a
 * weak data reference: MinGW does not resolve an undefined weak object at
 * link time, while an undefined weak function is a valid null extension
 * point. */
#if defined(__GNUC__) || defined(__clang__)
extern CxxNamespace* cxx_namespace_global(void) __attribute__((weak));
#endif

static CxxNamespace* codegen_cxx_global_namespace(void) {
#if defined(__GNUC__) || defined(__clang__)
    return cxx_namespace_global ? cxx_namespace_global() : NULL;
#else
    return NULL;
#endif
}

/* Label management */
static int label_counter = 0;
static int current_stack_offset = 0;

#define INIT_CAPACITY 4096

static void codegen_expr_loc(SourceLoc* location, const Expr* expression) {
    if (!location) return;
    location->filename = NULL;
    location->line = 0;
    location->column = 0;
    if (expression) {
        location->filename = expression->loc.filename;
        location->line = expression->loc.line;
        location->column = expression->loc.column;
    }
}

/* ═══════════════════════════════════════
 * Module Management
 * ═══════════════════════════════════════ */

Module* codegen_new(void) {
    Module* mod = rcc_alloc(sizeof(Module));

    mod->code.data = rcc_alloc(INIT_CAPACITY);
    mod->code.size = 0;
    mod->code.capacity = INIT_CAPACITY;

    mod->rodata.data = rcc_alloc(INIT_CAPACITY);
    mod->rodata.size = 0;
    mod->rodata.capacity = INIT_CAPACITY;

    mod->init_array.data = rcc_alloc(INIT_CAPACITY);
    mod->init_array.size = 0;
    mod->init_array.capacity = INIT_CAPACITY;

    mod->fini_array.data = rcc_alloc(INIT_CAPACITY);
    mod->fini_array.size = 0;
    mod->fini_array.capacity = INIT_CAPACITY;

    mod->data.data = rcc_alloc(INIT_CAPACITY);
    mod->data.size = 0;
    mod->data.capacity = INIT_CAPACITY;

    mod->bss.size = 0;
    mod->bss.align = 1u;

    mod->tls.data = rcc_alloc(INIT_CAPACITY);
    mod->tls.size = 0;
    mod->tls.capacity = INIT_CAPACITY;
    mod->tls_align = 1u;

    mod->relocs = NULL;
    mod->strings = NULL;
    mod->entry_point = 0;
    mod->stack_size = 0;

    /* Symbol and relocation tables for object files */
    mod->symbols = NULL;
    mod->symbol_count = 0;
    mod->symbol_capacity = 0;
    mod->relocs_arr = NULL;
    mod->reloc_count = 0;
    mod->reloc_capacity = 0;
    mod->got_entries = NULL;
    mod->got_entry_count = 0u;
    mod->global_initializers = NULL;
    mod->global_initializer_count = 0;
    mod->global_finalizers = NULL;
    mod->global_finalizer_count = 0;
    mod->compound_literal_count = 0u;

    return mod;
}

void codegen_free(Module* mod) {
    Reloc* relocation;
    StringLit* string_literal;
    if (!mod) return;
    for (int index = 0; index < mod->symbol_count; ++index) {
        rcc_free((void*)mod->symbols[index].name);
    }
    for (int index = 0; index < mod->reloc_count; ++index) {
        rcc_free((void*)mod->relocs_arr[index].symbol_name);
    }
    while (mod->global_initializers) {
        GlobalInitializer* next = mod->global_initializers->next;
        rcc_free(mod->global_initializers);
        mod->global_initializers = next;
    }
    while (mod->global_finalizers) {
        GlobalFinalizer* next = mod->global_finalizers->next;
        rcc_free(mod->global_finalizers);
        mod->global_finalizers = next;
    }
    while (mod->got_entries) {
        ModuleGotEntry* next = mod->got_entries->next;
        rcc_free((void*)mod->got_entries->target_symbol);
        rcc_free((void*)mod->got_entries->slot_symbol);
        rcc_free(mod->got_entries);
        mod->got_entries = next;
    }
    relocation = mod->relocs;
    while (relocation) {
        Reloc* next = relocation->next;
        rcc_free(relocation);
        relocation = next;
    }
    string_literal = mod->strings;
    while (string_literal) {
        StringLit* next = string_literal->next;
        rcc_free(string_literal);
        string_literal = next;
    }
    rcc_free(mod->code.data);
    rcc_free(mod->rodata.data);
    rcc_free(mod->init_array.data);
    rcc_free(mod->fini_array.data);
    rcc_free(mod->data.data);
    rcc_free(mod->tls.data);
    rcc_free(mod->symbols);
    rcc_free(mod->relocs_arr);
    rcc_free(mod);
}

/* ═══════════════════════════════════════
 * Emit Helpers
 * ═══════════════════════════════════════ */

static void ensure_code_capacity(Module* mod, size_t needed) {
    if (mod->code.size + needed > mod->code.capacity) {
        mod->code.capacity *= 2;
        mod->code.data = rcc_realloc(mod->code.data, mod->code.capacity);
    }
}

void emit_byte(Module* mod, uint8_t b) {
    ensure_code_capacity(mod, 1);
    mod->code.data[mod->code.size++] = b;
}

void emit_word(Module* mod, uint16_t w) {
    ensure_code_capacity(mod, 2);
    mod->code.data[mod->code.size++] = w & 0xFF;
    mod->code.data[mod->code.size++] = (w >> 8) & 0xFF;
}

void emit_dword(Module* mod, uint32_t d) {
    ensure_code_capacity(mod, 4);
    mod->code.data[mod->code.size++] = d & 0xFF;
    mod->code.data[mod->code.size++] = (d >> 8) & 0xFF;
    mod->code.data[mod->code.size++] = (d >> 16) & 0xFF;
    mod->code.data[mod->code.size++] = (d >> 24) & 0xFF;
}

void emit_bytes(Module* mod, const uint8_t* data, size_t len) {
    ensure_code_capacity(mod, len);
    memcpy(mod->code.data + mod->code.size, data, len);
    mod->code.size += len;
}

uint32_t code_offset(Module* mod) {
    return (uint32_t)mod->code.size;
}

/* ═══════════════════════════════════════
 * Symbol Table (for object files)
 * ═══════════════════════════════════════ */

void module_add_symbol(Module* mod, const char* name, uint32_t offset,
                       bool is_defined, ModuleSymbolSection section,
                       bool is_global) {
    for (int index = 0; index < mod->symbol_count; ++index) {
        ModuleSymbol* existing = &mod->symbols[index];
        if (strcmp(existing->name, name) != 0) continue;
        if (is_defined && !existing->is_defined) {
            existing->offset = offset;
            existing->is_defined = true;
            existing->section = section;
            existing->is_global = is_global;
        }
        return;
    }

    /* Expand if needed */
    if (mod->symbol_count >= mod->symbol_capacity) {
        int new_cap = mod->symbol_capacity == 0 ? 16 : mod->symbol_capacity * 2;
        mod->symbols = rcc_realloc(mod->symbols, new_cap * sizeof(ModuleSymbol));
        mod->symbol_capacity = new_cap;
    }

    ModuleSymbol* sym = &mod->symbols[mod->symbol_count++];
    sym->name = rcc_strdup(name);
    sym->offset = offset;
    sym->size = 0;
    sym->is_defined = is_defined;
    sym->section = section;
    sym->is_global = is_global;
    sym->is_weak = false;
}

void module_mark_symbol_weak(Module* mod, const char* name) {
    for (int index = 0; index < mod->symbol_count; ++index) {
        ModuleSymbol* symbol = &mod->symbols[index];
        if (strcmp(symbol->name, name) == 0) {
            if (symbol->is_defined && symbol->is_global) {
                symbol->is_weak = true;
            }
            return;
        }
    }
}

void module_add_relocation(Module* mod, ModuleSymbolSection source_section,
                           uint32_t offset, uint32_t target,
                           bool is_relative, bool is_64bit,
                           const char* symbol_name) {
    bool known_symbol = false;
    if (symbol_name && symbol_name[0] != '\0') {
        for (int index = 0; index < mod->symbol_count; ++index) {
            if (strcmp(mod->symbols[index].name, symbol_name) == 0) {
                known_symbol = true;
                break;
            }
        }
        /* Every relocation must have a corresponding object-file symbol.
         * This also covers compiler-synthesized C++ runtime calls such as
         * rin_free, which are not source declarations in the AST. */
        if (!known_symbol) {
            module_add_symbol(mod, symbol_name, 0u, false,
                              MODULE_SYMBOL_CODE, true);
        }
    }
    /* Expand if needed */
    if (mod->reloc_count >= mod->reloc_capacity) {
        int new_cap = mod->reloc_capacity == 0 ? 16 : mod->reloc_capacity * 2;
        mod->relocs_arr = rcc_realloc(mod->relocs_arr, new_cap * sizeof(ModuleReloc));
        mod->reloc_capacity = new_cap;
    }

    ModuleReloc* rel = &mod->relocs_arr[mod->reloc_count++];
    rel->source_section = source_section;
    rel->offset = offset;
    rel->target = target;
    rel->is_relative = is_relative;
    rel->is_64bit = is_64bit;
    rel->is_tls = false;
    rel->is_got = false;
    rel->symbol_name = symbol_name ? rcc_strdup(symbol_name) : NULL;
}

void module_add_got_relocation(Module* mod, ModuleSymbolSection source_section,
                               uint32_t offset, const char* symbol_name) {
    module_add_relocation(mod, source_section, offset, 0u, true, false,
                          symbol_name);
    mod->relocs_arr[mod->reloc_count - 1].is_got = true;
}

void module_add_tls_relocation(Module* mod,
                               ModuleSymbolSection source_section,
                               uint32_t offset, const char* symbol_name) {
    module_add_relocation(mod, source_section, offset, 0u, false, false,
                          symbol_name);
    mod->relocs_arr[mod->reloc_count - 1].is_tls = true;
}

const char* module_get_got_entry(Module* mod, const char* target_symbol) {
    ModuleGotEntry* entry;
    uint32_t pointer_size;
    uint32_t offset;
    uint8_t zero[8] = {0};
    char slot_name[64];

    if (!mod || !target_symbol || target_symbol[0] == '\0') return NULL;
    for (entry = mod->got_entries; entry; entry = entry->next) {
        if (strcmp(entry->target_symbol, target_symbol) == 0) {
            return entry->slot_symbol;
        }
    }

    pointer_size = g_opts.target_arch == ARCH_X64 ? 8u : 4u;
    while ((mod->data.size & (pointer_size - 1u)) != 0u) {
        emit_data(mod, zero, 1u);
    }
    offset = emit_data(mod, zero, pointer_size);
    snprintf(slot_name, sizeof(slot_name), "__rcc_got_%u",
             (unsigned int)mod->got_entry_count++);
    entry = rcc_alloc(sizeof(*entry));
    entry->target_symbol = rcc_strdup(target_symbol);
    entry->slot_symbol = rcc_strdup(slot_name);
    entry->offset = offset;
    entry->next = mod->got_entries;
    mod->got_entries = entry;
    module_add_symbol(mod, entry->slot_symbol, offset, true,
                      MODULE_SYMBOL_DATA, false);
    module_add_relocation(mod, MODULE_SYMBOL_DATA, offset, 0u, false,
                          pointer_size == 8u, target_symbol);
    return entry->slot_symbol;
}

const ModuleSymbol* module_lookup_symbol(const Module* mod,
                                         const char* symbol_name) {
    if (!mod || !symbol_name) return NULL;
    for (int index = 0; index < mod->symbol_count; ++index) {
        if (mod->symbols[index].name &&
            strcmp(mod->symbols[index].name, symbol_name) == 0) {
            return &mod->symbols[index];
        }
    }
    return NULL;
}

bool module_resolve_image_relocation(const Module* mod,
                                     ModuleSymbolSection source_section,
                                     uint32_t offset,
                                     bool is_64bit, uint64_t rodata_rva,
                                     uint64_t data_rva, uint64_t bss_rva,
                                     uint64_t* value) {
    const ModuleReloc* relocation = NULL;
    const ModuleSymbol* symbol = NULL;
    uint64_t base;

    if (!mod || !value) return false;
    for (int index = 0; index < mod->reloc_count; ++index) {
        const ModuleReloc* candidate = &mod->relocs_arr[index];
        if (candidate->source_section == source_section &&
            candidate->offset == offset && !candidate->is_relative &&
            !candidate->is_tls && candidate->is_64bit == is_64bit) {
            if (relocation) return false;
            relocation = candidate;
        }
    }
    if (!relocation || !relocation->symbol_name) return false;
    for (int index = 0; index < mod->symbol_count; ++index) {
        if (strcmp(mod->symbols[index].name,
                   relocation->symbol_name) == 0) {
            symbol = &mod->symbols[index];
            break;
        }
    }
    if (!symbol || !symbol->is_defined) return false;
    switch (symbol->section) {
        case MODULE_SYMBOL_CODE: base = 0u; break;
        case MODULE_SYMBOL_RODATA: base = rodata_rva; break;
        case MODULE_SYMBOL_DATA: base = data_rva; break;
        case MODULE_SYMBOL_BSS: base = bss_rva; break;
        default: return false;
    }
    if (symbol->offset > UINT64_MAX - base ||
        relocation->target > UINT64_MAX - base - symbol->offset) {
        return false;
    }
    *value = base + symbol->offset + relocation->target;
    return true;
}

bool module_resolve_tls_relocation(const Module* mod,
                                   ModuleSymbolSection source_section,
                                   uint32_t offset, uint32_t* value) {
    const ModuleReloc* relocation = NULL;
    const ModuleSymbol* symbol = NULL;
    uint64_t result;
    if (!mod || !value) return false;
    for (int index = 0; index < mod->reloc_count; ++index) {
        const ModuleReloc* candidate = &mod->relocs_arr[index];
        if (candidate->source_section == source_section &&
            candidate->offset == offset && candidate->is_tls) {
            if (relocation) return false;
            relocation = candidate;
        }
    }
    if (!relocation || !relocation->symbol_name) return false;
    for (int index = 0; index < mod->symbol_count; ++index) {
        if (strcmp(mod->symbols[index].name,
                   relocation->symbol_name) == 0) {
            symbol = &mod->symbols[index];
            break;
        }
    }
    if (!symbol || !symbol->is_defined ||
        symbol->section != MODULE_SYMBOL_TLS) return false;
    result = (uint64_t)symbol->offset + relocation->target;
    if (result >= mod->tls.size || result > UINT32_MAX) return false;
    *value = (uint32_t)result;
    return true;
}

void module_ensure_rodata_base_symbol(Module* mod) {
    for (int index = 0; index < mod->symbol_count; ++index) {
        if (strcmp(mod->symbols[index].name, "__rcc_rodata_base") == 0) return;
    }
    module_add_symbol(mod, "__rcc_rodata_base", 0u, true,
                      MODULE_SYMBOL_RODATA, false);
}

static Decl* codegen_global_variable(AST* ast, const char* name) {
    Decl* tentative = NULL;
    Decl* external = NULL;
    for (DeclList* item = ast->decls; item; item = item->next) {
        Decl* candidate = item->decl;
        if (candidate->kind != DECL_VAR || !candidate->var_is_global ||
            strcmp(candidate->name, name) != 0) {
            continue;
        }
        if (candidate->var_init) return candidate;
        if (candidate->storage != STORAGE_EXTERN) {
            if (!tentative) tentative = candidate;
        } else if (!external) {
            external = candidate;
        }
    }
    return tentative ? tentative : external;
}

static bool codegen_static_integer(Expr* expression, int64_t* value) {
    int64_t left;
    int64_t right;
    while (expression && expression->kind == EXPR_CAST) {
        expression = expression->cast_expr;
    }
    if (!expression || !value) return false;
    if (expression->kind == EXPR_INT_LIT) {
        *value = expression->int_val;
        return true;
    }
    if (expression->kind == EXPR_CHAR_LIT) {
        *value = (uint8_t)expression->char_val;
        return true;
    }
    if (expression->kind == EXPR_NEG && expression->unary_operand &&
        codegen_static_integer(expression->unary_operand, value) &&
        *value != INT64_MIN) {
        *value = -*value;
        return true;
    }
    if (expression->kind == EXPR_BITNOT && expression->unary_operand &&
        codegen_static_integer(expression->unary_operand, value)) {
        *value = ~*value;
        return true;
    }
    if (expression->kind == EXPR_NOT && expression->unary_operand &&
        codegen_static_integer(expression->unary_operand, value)) {
        *value = *value == 0;
        return true;
    }
    if (expression->kind == EXPR_SIZEOF) {
        Type* type = expression->sizeof_type ? expression->sizeof_type :
            expression->unary_operand ? expression->unary_operand->type : NULL;
        if (!type || type->size <= 0) return false;
        *value = type->size;
        return true;
    }
    if (expression->kind == EXPR_ALIGNOF) {
        Type* type = expression->sizeof_type;
        if (!type || type->align <= 0) return false;
        *value = type->align;
        return true;
    }
    if (expression->kind == EXPR_COND) {
        if (!codegen_static_integer(expression->cond_test, &left)) return false;
        return codegen_static_integer(left ? expression->cond_then
                                           : expression->cond_else,
                                      value);
    }
    if (expression->kind == EXPR_AND || expression->kind == EXPR_OR) {
        if (!codegen_static_integer(expression->binary_lhs, &left)) {
            return false;
        }
        if ((expression->kind == EXPR_AND && left == 0) ||
            (expression->kind == EXPR_OR && left != 0)) {
            *value = expression->kind == EXPR_OR;
            return true;
        }
        if (!codegen_static_integer(expression->binary_rhs, &right)) {
            return false;
        }
        *value = right != 0;
        return true;
    }
    switch (expression->kind) {
        case EXPR_ADD: case EXPR_SUB: case EXPR_MUL: case EXPR_DIV:
        case EXPR_MOD: case EXPR_BITAND: case EXPR_BITOR: case EXPR_BITXOR:
        case EXPR_LSHIFT: case EXPR_RSHIFT: case EXPR_EQ: case EXPR_NE:
        case EXPR_LT: case EXPR_GT: case EXPR_LE: case EXPR_GE:
            break;
        default:
            return false;
    }
    if (!expression->binary_lhs || !expression->binary_rhs ||
        !codegen_static_integer(expression->binary_lhs, &left) ||
        !codegen_static_integer(expression->binary_rhs, &right)) {
        return false;
    }
    switch (expression->kind) {
        case EXPR_ADD:
            if ((right > 0 && left > INT64_MAX - right) ||
                (right < 0 && left < INT64_MIN - right)) return false;
            *value = left + right;
            return true;
        case EXPR_SUB:
            if ((right < 0 && left > INT64_MAX + right) ||
                (right > 0 && left < INT64_MIN + right)) return false;
            *value = left - right;
            return true;
        case EXPR_MUL:
            if (left == 0 || right == 0) {
                *value = 0;
                return true;
            }
            if ((left == -1 && right == INT64_MIN) ||
                (right == -1 && left == INT64_MIN)) return false;
            if (left > 0 ? (right > 0 ? left > INT64_MAX / right
                                     : right < INT64_MIN / left)
                         : (right > 0 ? left < INT64_MIN / right
                                      : left < INT64_MAX / right)) {
                return false;
            }
            *value = left * right;
            return true;
        case EXPR_DIV:
        case EXPR_MOD:
            if (right == 0 || (left == INT64_MIN && right == -1)) return false;
            *value = expression->kind == EXPR_DIV ? left / right : left % right;
            return true;
        case EXPR_BITAND: *value = left & right; return true;
        case EXPR_BITOR: *value = left | right; return true;
        case EXPR_BITXOR: *value = left ^ right; return true;
        case EXPR_LSHIFT:
            if (left < 0 || right < 0 || right >= 64 ||
                left > (INT64_MAX >> right)) return false;
            *value = left << right;
            return true;
        case EXPR_RSHIFT:
            if (right < 0 || right >= 64) return false;
            *value = left >> right;
            return true;
        case EXPR_EQ: *value = left == right; return true;
        case EXPR_NE: *value = left != right; return true;
        case EXPR_LT: *value = left < right; return true;
        case EXPR_GT: *value = left > right; return true;
        case EXPR_LE: *value = left <= right; return true;
        case EXPR_GE: *value = left >= right; return true;
        default: return false;
    }
}

static bool codegen_runtime_global_scalar(const Type* type) {
    if (!type || type->size <= 0) return false;
    if (type->kind == TYPE_ARRAY || type->kind == TYPE_STRUCT ||
        type->kind == TYPE_UNION || type->kind == TYPE_FUNC) return false;
    /* A static-storage pointer must be an address constant (or a null
     * pointer constant).  Treating an unsupported integer-to-pointer
     * initializer as a runtime scalar would publish a non-conforming image
     * instead of reporting the invalid initializer. */
    if (type->kind == TYPE_PTR || type->kind == TYPE_NULLPTR) return false;
    return type_is_integer((Type*)type) || type->kind == TYPE_ENUM ||
           type_is_floating((Type*)type);
}

static void codegen_defer_global_initializer(Module* mod, Decl* declaration) {
    GlobalInitializer* initializer;
    GlobalInitializer** tail;
    if (!mod || !declaration) return;
    initializer = rcc_alloc(sizeof(*initializer));
    initializer->declaration = declaration;
    initializer->next = NULL;
    tail = &mod->global_initializers;
    while (*tail) tail = &(*tail)->next;
    *tail = initializer;
    ++mod->global_initializer_count;
}

static void codegen_defer_global_finalizer(Module* mod, Expr* expression) {
    GlobalFinalizer* finalizer;
    if (!mod || !expression) return;
    finalizer = rcc_alloc(sizeof(*finalizer));
    finalizer->expression = expression;
    finalizer->next = mod->global_finalizers;
    mod->global_finalizers = finalizer;
    ++mod->global_finalizer_count;
}

/* Evaluate the floating subset permitted in a static initializer.  Keeping
 * this separate from the integer evaluator avoids converting through a
 * machine integer and preserves the IEEE bit pattern that the data emitter
 * must publish. */
static bool codegen_static_floating(Expr* expression, double* value) {
    double left;
    double right;
    int64_t integer;
    if (!expression || !value) return false;
    if (expression->kind == EXPR_FLOAT_LIT) {
        *value = expression->float_val;
        return true;
    }
    if (expression->kind == EXPR_INT_LIT ||
        expression->kind == EXPR_CHAR_LIT) {
        if (!codegen_static_integer(expression, &integer)) return false;
        *value = (double)integer;
        return true;
    }
    if (expression->kind == EXPR_CAST) {
        return codegen_static_floating(expression->cast_expr, value);
    }
    if (expression->kind == EXPR_NEG) {
        if (!codegen_static_floating(expression->unary_operand, &left)) {
            return false;
        }
        *value = -left;
        return true;
    }
    switch (expression->kind) {
        case EXPR_ADD:
        case EXPR_SUB:
        case EXPR_MUL:
        case EXPR_DIV:
            if (!codegen_static_floating(expression->binary_lhs, &left) ||
                !codegen_static_floating(expression->binary_rhs, &right)) {
                return false;
            }
            if (expression->kind == EXPR_DIV && right == 0.0) return false;
            if (expression->kind == EXPR_ADD) *value = left + right;
            else if (expression->kind == EXPR_SUB) *value = left - right;
            else if (expression->kind == EXPR_MUL) *value = left * right;
            else *value = left / right;
            return true;
        case EXPR_COND:
            if (!codegen_static_integer(expression->cond_test, &integer)) {
                return false;
            }
            return codegen_static_floating(
                integer ? expression->cond_then : expression->cond_else,
                value);
        default:
            return false;
    }
}

static uint32_t codegen_pointer_element_size(const Type* type) {
    if (type && (type->kind == TYPE_PTR || type->kind == TYPE_ARRAY) &&
        type->base && type->base->size > 0) {
        return (uint32_t)type->base->size;
    }
    return 0u;
}

static uint32_t codegen_increment_size(const Type* type) {
    uint32_t size = codegen_pointer_element_size(type);
    return size == 0u ? 1u : size;
}

static bool codegen_materialize_static_compound(Module* mod,
                                                Expr* expression);

static bool codegen_add_static_offset(uint32_t* addend, int64_t index,
                                      uint32_t element_size) {
    int64_t delta;
    if (!addend || element_size == 0u ||
        (index > 0 && index > INT64_MAX / (int64_t)element_size) ||
        (index < 0 && index < INT64_MIN / (int64_t)element_size)) {
        return false;
    }
    delta = index * (int64_t)element_size;
    if (delta >= 0) {
        if ((uint64_t)delta > UINT32_MAX - *addend) return false;
        *addend += (uint32_t)delta;
    } else {
        uint64_t magnitude = UINT64_C(0) - (uint64_t)delta;
        if (magnitude > *addend) return false;
        *addend -= (uint32_t)magnitude;
    }
    return true;
}

static bool codegen_static_address(Module* mod, Expr* expression,
                                   const char** symbol_name,
                                   uint32_t* addend) {
    while (expression && expression->kind == EXPR_CAST) {
        expression = expression->cast_expr;
    }
    if (!expression || !symbol_name || !addend) return false;

    if (expression->kind == EXPR_COMPOUND) {
        if (!expression->compound_static_symbol &&
            !codegen_materialize_static_compound(mod, expression)) {
            return false;
        }
        *symbol_name = expression->compound_static_symbol;
        return true;
    }

    if (expression->kind == EXPR_STRING_LIT) {
        *addend = emit_string(mod, expression->str_val);
        module_ensure_rodata_base_symbol(mod);
        *symbol_name = "__rcc_rodata_base";
        return true;
    }
    if (expression->kind == EXPR_ADDR && expression->unary_operand) {
        Expr* addressed = expression->unary_operand;
        Decl* target = NULL;
        if (addressed->kind == EXPR_COMPOUND) {
            if (!codegen_materialize_static_compound(mod, addressed)) {
                return false;
            }
            *symbol_name = addressed->compound_static_symbol;
            return true;
        }
        if (addressed->kind == EXPR_INDEX && addressed->index_base &&
            addressed->index_expr &&
            addressed->index_base->kind == EXPR_COMPOUND) {
            int64_t index;
            if (!codegen_materialize_static_compound(
                    mod, addressed->index_base) ||
                !codegen_static_integer(addressed->index_expr, &index) ||
                !codegen_add_static_offset(
                    addend, index,
                    codegen_pointer_element_size(
                        addressed->index_base->compound_type))) {
                return false;
            }
            *symbol_name = addressed->index_base->compound_static_symbol;
            return true;
        }
        if (addressed->kind == EXPR_MEMBER && addressed->member_base &&
            addressed->member_base->kind == EXPR_COMPOUND &&
            addressed->member_field && addressed->member_field->offset >= 0) {
            if (!codegen_materialize_static_compound(
                    mod, addressed->member_base) ||
                (uint64_t)addressed->member_field->offset >
                    UINT32_MAX - *addend) {
                return false;
            }
            *addend += (uint32_t)addressed->member_field->offset;
            *symbol_name = addressed->member_base->compound_static_symbol;
            return true;
        }
        if (addressed->kind == EXPR_IDENT) {
            target = addressed->ident_decl;
        } else if (addressed->kind == EXPR_INDEX &&
                   addressed->index_base &&
                   addressed->index_base->kind == EXPR_IDENT &&
                   addressed->index_expr) {
            int64_t index;
            target = addressed->index_base->ident_decl;
            if (!target || target->kind != DECL_VAR ||
                !target->var_is_global || !target->type ||
                target->type->kind != TYPE_ARRAY ||
                !codegen_static_integer(addressed->index_expr, &index) ||
                !codegen_add_static_offset(
                    addend, index,
                    codegen_pointer_element_size(target->type))) {
                return false;
            }
        }
        if (!target || (target->kind != DECL_FUNC &&
            (target->kind != DECL_VAR || !target->var_is_global))) {
            return false;
        }
        *symbol_name = decl_link_name(target);
        return true;
    }
    if (expression->kind == EXPR_IDENT && expression->ident_decl &&
        (expression->ident_decl->kind == DECL_FUNC ||
         (expression->ident_decl->kind == DECL_VAR &&
          expression->ident_decl->var_is_global &&
          expression->ident_decl->type &&
          expression->ident_decl->type->kind == TYPE_ARRAY))) {
        *symbol_name = decl_link_name(expression->ident_decl);
        return true;
    }
    if (expression->kind == EXPR_ADD || expression->kind == EXPR_SUB) {
        Expr* address = expression->binary_lhs;
        Expr* integer = expression->binary_rhs;
        int64_t index;
        if (expression->kind == EXPR_ADD &&
            !codegen_static_integer(integer, &index)) {
            address = expression->binary_rhs;
            integer = expression->binary_lhs;
        }
        if (!codegen_static_integer(integer, &index) ||
            !codegen_static_address(mod, address, symbol_name, addend)) {
            return false;
        }
        if (expression->kind == EXPR_SUB) {
            if (index == INT64_MIN) return false;
            index = -index;
        }
        return codegen_add_static_offset(
            addend, index, codegen_pointer_element_size(address->type));
    }
    return false;
}

static bool codegen_emit_static_pointer(Module* mod, Type* type,
                                        Expr* initializer, uint32_t offset) {
    const char* symbol_name = NULL;
    uint32_t addend = 0u;
    uint32_t width = g_opts.target_arch == ARCH_X64 ? 8u : 4u;
    int64_t integer;

    if (!initializer || !type || type->size < (int)width) {
        return false;
    }
    if ((initializer->type &&
         initializer->type->kind == TYPE_NULLPTR) ||
        (codegen_static_integer(initializer, &integer) && integer == 0)) {
        return true;
    }
    if (!codegen_static_address(mod, initializer, &symbol_name, &addend)) {
        return false;
    }

    module_add_relocation(mod, MODULE_SYMBOL_DATA, offset, addend, false,
                          width == 8u, symbol_name);
    add_reloc(mod, MODULE_SYMBOL_DATA, offset,
              width == 8u ? RIN_RELOC_ABS64 : RIN_RELOC_ABS32);
    return true;
}

static Expr* codegen_character_array_string(Type* type, Expr* initializer) {
    if (!type || type->kind != TYPE_ARRAY || !type->base ||
        type->base->kind != TYPE_CHAR || !initializer) {
        return NULL;
    }
    if (initializer->kind == EXPR_STRING_LIT) return initializer;
    if (initializer->kind == EXPR_COMPOUND && initializer->compound_init &&
        !initializer->compound_init->next &&
        initializer->compound_init->designator_kind == INIT_DESIGNATOR_NONE &&
        initializer->compound_init->expr &&
        initializer->compound_init->expr->kind == EXPR_STRING_LIT) {
        return initializer->compound_init->expr;
    }
    return NULL;
}

static TypeField* codegen_initializer_field(Type* type, const char* name) {
    if (!type || !name) return NULL;
    for (TypeField* field = type->fields; field; field = field->next) {
        if (strcmp(field->name, name) == 0) return field;
    }
    return NULL;
}

static bool codegen_pack_static_bitfield(uint8_t* data, size_t data_size,
                                         const TypeField* field,
                                         Expr* initializer,
                                         uint32_t offset) {
    uint32_t mask;
    uint32_t shifted_mask;
    uint32_t storage = 0u;
    int64_t value;
    size_t width;
    if (!data || !field || !field->is_bitfield || !field->type ||
        !initializer || field->type->size <= 0 || field->type->size > 4) {
        return false;
    }
    width = (size_t)field->type->size;
    if ((size_t)offset > data_size || width > data_size - (size_t)offset ||
        !codegen_static_integer(initializer, &value)) {
        return false;
    }
    mask = field->bit_width >= 32u
        ? UINT32_MAX : (UINT32_C(1) << field->bit_width) - 1u;
    shifted_mask = mask << field->bit_offset;
    for (size_t byte = 0u; byte < width; ++byte) {
        storage |= (uint32_t)data[offset + byte] << (byte * 8u);
    }
    storage = (storage & ~shifted_mask) |
              (((uint32_t)value & mask) << field->bit_offset);
    for (size_t byte = 0u; byte < width; ++byte) {
        data[offset + byte] = (uint8_t)(storage >> (byte * 8u));
    }
    return true;
}

static bool codegen_aggregate_zero_initializer(Type* type,
                                               Expr* initializer) {
    ExprList* item;
    int64_t value;
    if (!type || !initializer || initializer->kind != EXPR_COMPOUND ||
        (type->kind != TYPE_ARRAY && type->kind != TYPE_STRUCT &&
         type->kind != TYPE_UNION)) {
        return false;
    }
    item = initializer->compound_init;
    return item && !item->next &&
           item->designator_kind == INIT_DESIGNATOR_NONE && item->expr &&
           codegen_static_integer(item->expr, &value) && value == 0;
}

static bool codegen_emit_static_initializer(Module* mod, Type* type,
                                            Expr* initializer,
                                            uint32_t offset) {
    Expr* string;
    if (!mod || !type || !initializer || offset > mod->data.size ||
        (uint64_t)type->size > mod->data.size - offset) {
        return false;
    }
    if (codegen_aggregate_zero_initializer(type, initializer)) return true;
    string = codegen_character_array_string(type, initializer);
    if (string) {
        size_t text_size = strlen(string->str_val) + 1u;
        size_t copy_size = (size_t)type->size < text_size
            ? (size_t)type->size : text_size;
        memcpy(mod->data.data + offset, string->str_val, copy_size);
        return true;
    }
    if (initializer->kind == EXPR_COMPOUND) {
        if (type->kind == TYPE_ARRAY) {
            int64_t cursor = 0;
            for (ExprList* item = initializer->compound_init; item;
                 item = item->next) {
                uint64_t item_offset;
                if (item->designator_kind == INIT_DESIGNATOR_FIELD) {
                    return false;
                }
                if (item->designator_kind == INIT_DESIGNATOR_INDEX) {
                    cursor = item->designator_index;
                }
                if (cursor < 0 || cursor >= type->array_len || !type->base) {
                    return false;
                }
                item_offset = (uint64_t)offset +
                              (uint64_t)cursor * (uint64_t)type->base->size;
                if (item_offset > UINT32_MAX ||
                    !codegen_emit_static_initializer(
                        mod, type->base, item->expr,
                        (uint32_t)item_offset)) {
                    return false;
                }
                ++cursor;
            }
            return true;
        }
        if (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) {
            TypeField* cursor = type->fields;
            int initialized = 0;
            for (ExprList* item = initializer->compound_init; item;
                 item = item->next) {
                TypeField* field = cursor;
                uint64_t field_offset;
                if (item->designator_kind == INIT_DESIGNATOR_INDEX) {
                    return false;
                }
                if (item->designator_kind == INIT_DESIGNATOR_FIELD) {
                    field = codegen_initializer_field(
                        type, item->designator_field);
                }
                if (!field || (type->kind == TYPE_UNION && initialized != 0 &&
                               item->designator_kind ==
                                   INIT_DESIGNATOR_NONE)) {
                    return false;
                }
                field_offset = (uint64_t)offset + (uint64_t)field->offset;
                if (field_offset > UINT32_MAX ||
                    (field->is_bitfield
                         ? !codegen_pack_static_bitfield(
                               mod->data.data, mod->data.size, field,
                               item->expr, (uint32_t)field_offset)
                         : !codegen_emit_static_initializer(
                               mod, field->type, item->expr,
                               (uint32_t)field_offset))) {
                    return false;
                }
                cursor = field->next;
                ++initialized;
            }
            return true;
        }
        if (!initializer->compound_init || initializer->compound_init->next ||
            initializer->compound_init->designator_kind !=
                INIT_DESIGNATOR_NONE) {
            return false;
        }
        return codegen_emit_static_initializer(
            mod, type, initializer->compound_init->expr, offset);
    }
    if (type->kind == TYPE_PTR) {
        return codegen_emit_static_pointer(mod, type, initializer, offset);
    }
    if (type->kind == TYPE_NULLPTR) {
        return initializer->type &&
               initializer->type->kind == TYPE_NULLPTR;
    }
    if (type_is_floating(type)) {
        double value;
        if (!codegen_static_floating(initializer, &value)) return false;
        if (type->kind == TYPE_FLOAT) {
            float narrowed = (float)value;
            uint32_t bits;
            memcpy(&bits, &narrowed, sizeof(bits));
            for (uint32_t byte = 0u; byte < sizeof(bits); ++byte) {
                mod->data.data[offset + byte] =
                    (uint8_t)(bits >> (byte * 8u));
            }
        } else {
            uint64_t bits;
            memcpy(&bits, &value, sizeof(bits));
            for (uint32_t byte = 0u; byte < sizeof(bits); ++byte) {
                mod->data.data[offset + byte] =
                    (uint8_t)(bits >> (byte * 8u));
            }
        }
        return true;
    }
    if (type_is_integer(type) || type->kind == TYPE_ENUM) {
        int64_t constant;
        uint32_t width = (uint32_t)type->size;
        if (!codegen_static_integer(initializer, &constant)) return false;
        if (type->kind == TYPE_BOOL) constant = constant != 0;
        if (width > 8u) width = 8u;
        for (uint32_t byte = 0u; byte < width; ++byte) {
            mod->data.data[offset + byte] =
                (uint8_t)((uint64_t)constant >> (byte * 8u));
        }
        return true;
    }
    return false;
}

static void codegen_ensure_tls_capacity(Module* mod, size_t needed) {
    if (needed <= mod->tls.capacity) return;
    while (mod->tls.capacity < needed) mod->tls.capacity *= 2u;
    mod->tls.data = rcc_realloc(mod->tls.data, mod->tls.capacity);
}

static bool codegen_emit_tls_initializer(Module* mod, Type* type,
                                         Expr* initializer,
                                         uint32_t offset) {
    Expr* string;
    if (!mod || !type || !initializer || offset > mod->tls.size ||
        (uint64_t)type->size > mod->tls.size - offset) return false;
    if (codegen_aggregate_zero_initializer(type, initializer)) return true;
    string = codegen_character_array_string(type, initializer);
    if (string) {
        size_t text_size = strlen(string->str_val) + 1u;
        size_t copy_size = (size_t)type->size < text_size
            ? (size_t)type->size : text_size;
        memcpy(mod->tls.data + offset, string->str_val, copy_size);
        return true;
    }
    if (initializer->kind == EXPR_COMPOUND) {
        if (type->kind == TYPE_ARRAY) {
            int64_t cursor = 0;
            for (ExprList* item = initializer->compound_init; item;
                 item = item->next) {
                uint64_t item_offset;
                if (item->designator_kind == INIT_DESIGNATOR_FIELD) return false;
                if (item->designator_kind == INIT_DESIGNATOR_INDEX) {
                    cursor = item->designator_index;
                }
                if (cursor < 0 || cursor >= type->array_len || !type->base) {
                    return false;
                }
                item_offset = (uint64_t)offset +
                              (uint64_t)cursor * (uint64_t)type->base->size;
                if (item_offset > UINT32_MAX ||
                    !codegen_emit_tls_initializer(
                        mod, type->base, item->expr,
                        (uint32_t)item_offset)) return false;
                ++cursor;
            }
            return true;
        }
        if (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) {
            TypeField* cursor = type->fields;
            int initialized = 0;
            for (ExprList* item = initializer->compound_init; item;
                 item = item->next) {
                TypeField* field = cursor;
                uint64_t field_offset;
                if (item->designator_kind == INIT_DESIGNATOR_INDEX) return false;
                if (item->designator_kind == INIT_DESIGNATOR_FIELD) {
                    field = codegen_initializer_field(
                        type, item->designator_field);
                }
                if (!field || (type->kind == TYPE_UNION && initialized != 0 &&
                               item->designator_kind ==
                                   INIT_DESIGNATOR_NONE)) return false;
                field_offset = (uint64_t)offset + (uint64_t)field->offset;
                if (field_offset > UINT32_MAX ||
                    (field->is_bitfield
                         ? !codegen_pack_static_bitfield(
                               mod->tls.data, mod->tls.size, field,
                               item->expr, (uint32_t)field_offset)
                         : !codegen_emit_tls_initializer(
                               mod, field->type, item->expr,
                               (uint32_t)field_offset))) return false;
                cursor = field->next;
                ++initialized;
            }
            return true;
        }
        if (!initializer->compound_init || initializer->compound_init->next ||
            initializer->compound_init->designator_kind !=
                INIT_DESIGNATOR_NONE) return false;
        return codegen_emit_tls_initializer(
            mod, type, initializer->compound_init->expr, offset);
    }
    if (type->kind == TYPE_PTR) {
        int64_t constant;
        return (initializer->type &&
                initializer->type->kind == TYPE_NULLPTR) ||
               (codegen_static_integer(initializer, &constant) &&
                constant == 0);
    }
    if (type->kind == TYPE_NULLPTR) {
        return initializer->type &&
               initializer->type->kind == TYPE_NULLPTR;
    }
    if (type_is_floating(type)) {
        double value;
        if (!codegen_static_floating(initializer, &value)) return false;
        if (type->kind == TYPE_FLOAT) {
            float narrowed = (float)value;
            memcpy(mod->tls.data + offset, &narrowed, sizeof(narrowed));
        } else {
            memcpy(mod->tls.data + offset, &value, sizeof(value));
        }
        return true;
    }
    if (type_is_integer(type) || type->kind == TYPE_ENUM) {
        int64_t constant;
        uint32_t width = (uint32_t)type->size;
        if (!codegen_static_integer(initializer, &constant)) return false;
        if (type->kind == TYPE_BOOL) constant = constant != 0;
        if (width > 8u) width = 8u;
        for (uint32_t byte = 0u; byte < width; ++byte) {
            mod->tls.data[offset + byte] =
                (uint8_t)((uint64_t)constant >> (byte * 8u));
        }
        return true;
    }
    return false;
}

static void codegen_add_vtable_pointer(Module* mod,
                                       ModuleSymbolSection source_section,
                                       uint32_t offset, Type* type);

static bool codegen_type_has_vtable_storage(Type* type) {
    CxxClass* cls = type ? type->cxx_class : NULL;
    return type && (type->cxx_vtable_size > 0 ||
                    (cls && cls->secondary_vtable_count > 0));
}

static bool codegen_emit_static_local(Module* mod, Decl* declaration) {
    uint8_t zero[32] = {0};
    uint32_t size;
    uint32_t alignment;
    uint32_t offset;
    if (!mod || !declaration || !declaration->var_is_static_local ||
        !declaration->type || declaration->type->size <= 0) {
        return false;
    }
    size = (uint32_t)declaration->type->size;
    alignment = declaration->type->align > 0
        ? (uint32_t)declaration->type->align : 1u;
    if (alignment > 16u) alignment = 16u;
    if (declaration->var_is_thread_local) {
        uint64_t aligned = ((uint64_t)mod->tls.size + alignment - 1u) &
                           ~((uint64_t)alignment - 1u);
        if (aligned > UINT32_MAX || size > UINT32_MAX - aligned) {
            rcc_error(declaration->loc,
                      "static local TLS exceeds compiler limits");
            return false;
        }
        offset = (uint32_t)aligned;
        codegen_ensure_tls_capacity(mod, (size_t)aligned + size);
        if (mod->tls.size < aligned) {
            memset(mod->tls.data + mod->tls.size, 0,
                   (size_t)aligned - mod->tls.size);
        }
        memset(mod->tls.data + offset, 0, size);
        mod->tls.size = (size_t)aligned + size;
        if (alignment > mod->tls_align) mod->tls_align = alignment;
        declaration->var_offset = offset;
        if (declaration->var_init &&
            !codegen_emit_tls_initializer(
                mod, declaration->type, declaration->var_init, offset)) {
            rcc_error(declaration->loc,
                      "unsupported static local TLS initializer for '%s'",
                      declaration->name);
            return false;
        }
        module_add_symbol(mod, decl_link_name(declaration), offset, true,
                          MODULE_SYMBOL_TLS, false);
        return true;
    }
    if (!declaration->var_init) {
        uint64_t aligned = ((uint64_t)mod->bss.size + alignment - 1u) &
                           ~((uint64_t)alignment - 1u);
        if (aligned > UINT32_MAX || size > UINT32_MAX - aligned) {
            rcc_error(declaration->loc,
                      "static local BSS exceeds compiler limits");
            return false;
        }
        offset = (uint32_t)aligned;
        mod->bss.size = (size_t)(aligned + size);
        if (alignment > mod->bss.align) mod->bss.align = alignment;
        declaration->var_offset = offset;
        module_add_symbol(mod, decl_link_name(declaration), offset, true,
                          MODULE_SYMBOL_BSS, false);
        return true;
    }
    while ((mod->data.size & (alignment - 1u)) != 0u) {
        emit_data(mod, zero, 1u);
    }
    offset = (uint32_t)mod->data.size;
    while (size > sizeof(zero)) {
        emit_data(mod, zero, sizeof(zero));
        size -= sizeof(zero);
    }
    if (size) emit_data(mod, zero, size);
    declaration->var_offset = offset;
    if (!codegen_emit_static_initializer(
            mod, declaration->type, declaration->var_init, offset)) {
        rcc_error(declaration->loc,
                  "static local initializer for '%s' is not a constant address or expression",
                  declaration->name);
        return false;
    }
    codegen_add_vtable_pointer(mod, MODULE_SYMBOL_DATA, offset,
                               declaration->type);
    module_add_symbol(mod, decl_link_name(declaration), offset, true,
                      MODULE_SYMBOL_DATA, false);
    return true;
}

static void codegen_emit_static_locals(Module* mod, Stmt* statement) {
    if (!statement) return;
    switch (statement->kind) {
        case STMT_BLOCK:
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                codegen_emit_static_locals(mod, item->stmt);
            }
            break;
        case STMT_IF:
            codegen_emit_static_locals(mod, statement->if_then);
            codegen_emit_static_locals(mod, statement->if_else);
            break;
        case STMT_WHILE:
        case STMT_DO:
            codegen_emit_static_locals(mod, statement->while_body);
            break;
        case STMT_FOR:
            codegen_emit_static_locals(mod, statement->for_init);
            codegen_emit_static_locals(mod, statement->for_body);
            break;
        case STMT_SWITCH:
            codegen_emit_static_locals(mod, statement->switch_body);
            break;
        case STMT_CASE:
            codegen_emit_static_locals(mod, statement->case_stmt);
            break;
        case STMT_DEFAULT:
            codegen_emit_static_locals(mod, statement->default_stmt);
            break;
        case STMT_LABEL:
            codegen_emit_static_locals(mod, statement->label_stmt);
            break;
        case STMT_TRY:
            codegen_emit_static_locals(mod, statement->try_body);
            for (CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                codegen_emit_static_locals(mod, handler->body);
            }
            break;
        case STMT_DECL:
            if (statement->decl) {
                if (statement->decl->var_is_static_local) {
                    (void)codegen_emit_static_local(mod, statement->decl);
                } else if (statement->decl->var_is_block_extern) {
                    module_add_symbol(mod, decl_link_name(statement->decl),
                                      0u, false,
                                      statement->decl->var_is_thread_local
                                          ? MODULE_SYMBOL_TLS
                                          : MODULE_SYMBOL_DATA,
                                      true);
                }
            }
            break;
        default:
            break;
    }
}

void codegen_emit_global_data(Module* mod, AST* ast) {
    for (DeclList* item = ast->decls; item; item = item->next) {
        Decl* declaration = item->decl;
        uint8_t zero[32] = {0};
        uint32_t size;
        uint32_t alignment;
        uint32_t offset;
        if (declaration->kind != DECL_VAR || !declaration->var_is_global ||
            !declaration->type || declaration->type->size <= 0) continue;
        if (codegen_global_variable(ast, declaration->name) != declaration) {
            continue;
        }
        size = (uint32_t)declaration->type->size;
        alignment = declaration->type->align > 0
            ? (uint32_t)declaration->type->align : 1u;
        if (alignment > 16u) alignment = 16u;
        if (declaration->var_is_thread_local) {
            uint64_t aligned;
            if (declaration->storage == STORAGE_EXTERN &&
                !declaration->var_init) {
                module_add_symbol(mod, decl_link_name(declaration), 0u, false,
                                  MODULE_SYMBOL_TLS, true);
                continue;
            }
            aligned = ((uint64_t)mod->tls.size + alignment - 1u) &
                      ~((uint64_t)alignment - 1u);
            if (aligned > UINT32_MAX || size > UINT32_MAX - aligned) {
                rcc_error(declaration->loc, "TLS template exceeds compiler limits");
                continue;
            }
            offset = (uint32_t)aligned;
            codegen_ensure_tls_capacity(mod, (size_t)aligned + size);
            if (mod->tls.size < aligned) {
                memset(mod->tls.data + mod->tls.size, 0,
                       (size_t)aligned - mod->tls.size);
            }
            memset(mod->tls.data + offset, 0, size);
            mod->tls.size = (size_t)aligned + size;
            if (alignment > mod->tls_align) mod->tls_align = alignment;
            declaration->var_offset = offset;
            if (declaration->var_init &&
                !codegen_emit_tls_initializer(
                    mod, declaration->type, declaration->var_init, offset)) {
                rcc_error(declaration->loc,
                          "unsupported thread-local initializer for '%s'",
                          declaration->name);
            }
            module_add_symbol(mod, decl_link_name(declaration), offset, true,
                              MODULE_SYMBOL_TLS,
                              declaration->storage != STORAGE_STATIC);
            continue;
        }
        if (declaration->storage == STORAGE_EXTERN &&
            !declaration->var_init) {
            module_add_symbol(mod, decl_link_name(declaration), 0u, false,
                              MODULE_SYMBOL_DATA, true);
            continue;
        }
        if (!declaration->var_init &&
            codegen_type_has_vtable_storage(declaration->type)) {
            uint64_t aligned = ((uint64_t)mod->data.size + alignment - 1u) &
                               ~((uint64_t)alignment - 1u);
            if (aligned > UINT32_MAX || size > UINT32_MAX - aligned) {
                rcc_error(declaration->loc, "C++ vtable object exceeds data limits");
                continue;
            }
            offset = (uint32_t)aligned;
            while (mod->data.size < aligned) emit_data(mod, zero, 1u);
            while (size > sizeof(zero)) {
                emit_data(mod, zero, sizeof(zero));
                size -= sizeof(zero);
            }
            if (size) emit_data(mod, zero, size);
            declaration->var_offset = offset;
            codegen_add_vtable_pointer(mod, MODULE_SYMBOL_DATA,
                                       offset, declaration->type);
            module_add_symbol(mod, decl_link_name(declaration), offset, true,
                              MODULE_SYMBOL_DATA,
                              declaration->storage != STORAGE_STATIC);
            continue;
        }
        if (!declaration->var_init) {
            uint64_t aligned = ((uint64_t)mod->bss.size + alignment - 1u) &
                               ~((uint64_t)alignment - 1u);
            if (aligned > UINT32_MAX || size > UINT32_MAX - aligned) {
                rcc_error(declaration->loc, "BSS exceeds compiler limits");
                continue;
            }
            offset = (uint32_t)aligned;
            mod->bss.size = (size_t)(aligned + size);
            if (alignment > mod->bss.align) mod->bss.align = alignment;
            declaration->var_offset = offset;
            module_add_symbol(mod, decl_link_name(declaration), offset, true,
                              MODULE_SYMBOL_BSS,
                              declaration->storage != STORAGE_STATIC);
            continue;
        }
        while ((mod->data.size & (alignment - 1u)) != 0u) {
            emit_data(mod, zero, 1u);
        }
        offset = (uint32_t)mod->data.size;
        while (size > sizeof(zero)) {
            emit_data(mod, zero, sizeof(zero));
            size -= sizeof(zero);
        }
        if (size) emit_data(mod, zero, size);
        declaration->var_offset = offset;
        if (!codegen_emit_static_initializer(
                mod, declaration->type, declaration->var_init, offset)) {
            if (codegen_runtime_global_scalar(declaration->type)) {
                codegen_defer_global_initializer(mod, declaration);
            } else {
                rcc_error(declaration->loc,
                          "unsupported static initializer for '%s'",
                          declaration->name);
            }
        }
        if (declaration->var_cleanup) {
            codegen_defer_global_finalizer(mod, declaration->var_cleanup);
        }
        codegen_add_vtable_pointer(mod, MODULE_SYMBOL_DATA, offset,
                                   declaration->type);
        module_add_symbol(mod, decl_link_name(declaration), offset, true,
                          MODULE_SYMBOL_DATA,
                          declaration->storage != STORAGE_STATIC);
    }
    for (DeclList* item = ast->decls; item; item = item->next) {
        if (item->decl && item->decl->kind == DECL_FUNC &&
            item->decl->func_body) {
            codegen_emit_static_locals(mod, item->decl->func_body);
        }
    }
}

/* ═══════════════════════════════════════
 * Data Section
 * ═══════════════════════════════════════ */

static void ensure_data_capacity(Module* mod, size_t needed) {
    if (mod->data.size + needed > mod->data.capacity) {
        while (mod->data.size + needed > mod->data.capacity) {
            mod->data.capacity *= 2;
        }
        mod->data.data = rcc_realloc(mod->data.data, mod->data.capacity);
    }
}

static void ensure_rodata_capacity(Module* mod, size_t needed) {
    if (mod->rodata.size + needed > mod->rodata.capacity) {
        while (mod->rodata.size + needed > mod->rodata.capacity) {
            mod->rodata.capacity *= 2;
        }
        mod->rodata.data = rcc_realloc(mod->rodata.data,
                                       mod->rodata.capacity);
    }
}

uint32_t emit_string(Module* mod, const char* str) {
    /* Check if already exists */
    for (StringLit* s = mod->strings; s; s = s->next) {
        if (strcmp(s->value, str) == 0) {
            return s->offset;
        }
    }

    /* Add new string */
    size_t len = strlen(str) + 1;
    uint32_t offset = (uint32_t)mod->rodata.size;

    ensure_rodata_capacity(mod, len);
    memcpy(mod->rodata.data + mod->rodata.size, str, len);
    mod->rodata.size += len;

    /* Record in string list */
    StringLit* lit = rcc_alloc(sizeof(StringLit));
    lit->value = str;
    lit->offset = offset;
    lit->next = mod->strings;
    mod->strings = lit;

    return offset;
}

uint32_t emit_data(Module* mod, const void* data, size_t len) {
    uint32_t offset = (uint32_t)mod->data.size;
    ensure_data_capacity(mod, len);
    memcpy(mod->data.data + mod->data.size, data, len);
    mod->data.size += len;
    return offset;
}

/* ═══════════════════════════════════════
 * Relocations
 * ═══════════════════════════════════════ */

void add_reloc(Module* mod, ModuleSymbolSection source_section,
               uint32_t offset, uint32_t type) {
    Reloc* r = rcc_alloc(sizeof(Reloc));
    r->source_section = source_section;
    r->offset = offset;
    r->type = type;
    r->symbol = NULL;
    r->next = mod->relocs;
    mod->relocs = r;
}

/* ═══════════════════════════════════════
 * x86 Instruction Encoding
 * ═══════════════════════════════════════ */

/* Registers */
#define EAX 0
#define ECX 1
#define EDX 2
#define EBX 3
#define ESP 4
#define EBP 5
#define ESI 6
#define EDI 7

/* ModR/M byte */
static uint8_t modrm(int mod, int reg, int rm) {
    return (uint8_t)((mod << 6) | (reg << 3) | rm);
}

/* Common instructions */
static void emit_push_reg(Module* mod, int reg) {
    emit_byte(mod, 0x50 + reg);
}

static void emit_pop_reg(Module* mod, int reg) {
    emit_byte(mod, 0x58 + reg);
}

static void emit_mov_reg_reg(Module* mod, int dst, int src) {
    emit_byte(mod, 0x89);
    emit_byte(mod, modrm(3, src, dst));
}

static void emit_mov_reg_imm(Module* mod, int reg, uint32_t imm) {
    emit_byte(mod, 0xB8 + reg);
    emit_dword(mod, imm);
}

static void emit_memory_operand32(Module* mod, int reg, int base,
                                  int32_t disp) {
    if (disp == 0 && base != EBP) {
        emit_byte(mod, modrm(0, reg, base));
        if (base == ESP) emit_byte(mod, 0x24);
    } else if (disp >= -128 && disp <= 127) {
        emit_byte(mod, modrm(1, reg, base));
        if (base == ESP) emit_byte(mod, 0x24);
        emit_byte(mod, (uint8_t)disp);
    } else {
        emit_byte(mod, modrm(2, reg, base));
        if (base == ESP) emit_byte(mod, 0x24);
        emit_dword(mod, (uint32_t)disp);
    }
}

static void emit_mov_reg_mem(Module* mod, int reg, int base, int32_t disp) {
    emit_byte(mod, 0x8B);
    emit_memory_operand32(mod, reg, base, disp);
}

static void emit_mov_mem_reg(Module* mod, int base, int32_t disp, int src) {
    emit_byte(mod, 0x89);
    emit_memory_operand32(mod, src, base, disp);
}

static void emit_mov_mem_reg8(Module* mod, int base, int32_t disp, int src) {
    emit_byte(mod, 0x88);
    emit_memory_operand32(mod, src, base, disp);
}

static int gen_type_width32(const Type* type) {
    if (type && (type->size == 1 || type->size == 2)) return type->size;
    return 4;
}

static void emit_load_typed32(Module* mod, int reg, int base, int32_t disp,
                              const Type* type) {
    int width = gen_type_width32(type);
    if (width == 4) {
        emit_mov_reg_mem(mod, reg, base, disp);
        return;
    }
    emit_byte(mod, 0x0F);
    emit_byte(mod, type && !type->is_unsigned
        ? (width == 1 ? 0xBE : 0xBF)
        : (width == 1 ? 0xB6 : 0xB7));
    emit_memory_operand32(mod, reg, base, disp);
}

static void emit_store_typed32(Module* mod, int base, int32_t disp, int src,
                               const Type* type) {
    int width = gen_type_width32(type);
    if (width == 4) {
        emit_mov_mem_reg(mod, base, disp, src);
        return;
    }
    if (width == 2) emit_byte(mod, 0x66);
    emit_byte(mod, width == 1 ? 0x88 : 0x89);
    emit_memory_operand32(mod, src, base, disp);
}

static void emit_add_reg_imm(Module* mod, int reg, int32_t imm) {
    if (imm >= -128 && imm <= 127) {
        emit_byte(mod, 0x83);
        emit_byte(mod, modrm(3, 0, reg));
        emit_byte(mod, (uint8_t)imm);
    } else {
        emit_byte(mod, 0x81);
        emit_byte(mod, modrm(3, 0, reg));
        emit_dword(mod, (uint32_t)imm);
    }
}

static void emit_sub_reg_imm(Module* mod, int reg, int32_t imm) {
    if (imm >= -128 && imm <= 127) {
        emit_byte(mod, 0x83);
        emit_byte(mod, modrm(3, 5, reg));
        emit_byte(mod, (uint8_t)imm);
    } else {
        emit_byte(mod, 0x81);
        emit_byte(mod, modrm(3, 5, reg));
        emit_dword(mod, (uint32_t)imm);
    }
}

static void emit_add_reg_reg(Module* mod, int dst, int src) {
    emit_byte(mod, 0x01);
    emit_byte(mod, modrm(3, src, dst));
}

static void emit_sub_reg_reg(Module* mod, int dst, int src) {
    emit_byte(mod, 0x29);
    emit_byte(mod, modrm(3, src, dst));
}

static void emit_adc_reg_reg(Module* mod, int dst, int src) {
    emit_byte(mod, 0x11);
    emit_byte(mod, modrm(3, src, dst));
}

static void emit_sbb_reg_reg(Module* mod, int dst, int src) {
    emit_byte(mod, 0x19);
    emit_byte(mod, modrm(3, src, dst));
}

static void emit_xor_reg_imm8(Module* mod, int reg, uint8_t imm) {
    emit_byte(mod, 0x83);
    emit_byte(mod, modrm(3, 6, reg));
    emit_byte(mod, imm);
}

static void emit_dec_reg(Module* mod, int reg) {
    emit_byte(mod, 0x48 + reg);
}

static void emit_adc_reg_imm8(Module* mod, int reg, uint8_t imm) {
    emit_byte(mod, 0x83);
    emit_byte(mod, modrm(3, 2, reg));
    emit_byte(mod, imm);
}

static void emit_sbb_reg_imm8(Module* mod, int reg, uint8_t imm) {
    emit_byte(mod, 0x83);
    emit_byte(mod, modrm(3, 3, reg));
    emit_byte(mod, imm);
}

static void emit_imul_reg_reg(Module* mod, int dst, int src) {
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0xAF);
    emit_byte(mod, modrm(3, dst, src));
}

static void emit_mul_reg(Module* mod, int reg) {
    emit_byte(mod, 0xF7);
    emit_byte(mod, modrm(3, 4, reg));
}

static void emit_scale_reg(Module* mod, int reg, uint32_t scale) {
    if (scale <= 1u) return;
    emit_mov_reg_imm(mod, EDX, scale);
    emit_imul_reg_reg(mod, reg, EDX);
}

static void emit_idiv_reg(Module* mod, int reg) {
    emit_byte(mod, 0xF7);
    emit_byte(mod, modrm(3, 7, reg));
}

static void emit_div_reg(Module* mod, int reg) {
    emit_byte(mod, 0xF7);
    emit_byte(mod, modrm(3, 6, reg));
}

static void emit_cdq(Module* mod) {
    emit_byte(mod, 0x99);
}

static void emit_neg_reg(Module* mod, int reg) {
    emit_byte(mod, 0xF7);
    emit_byte(mod, modrm(3, 3, reg));
}

static void emit_not_reg(Module* mod, int reg) {
    emit_byte(mod, 0xF7);
    emit_byte(mod, modrm(3, 2, reg));
}

static void emit_and_reg_reg(Module* mod, int dst, int src) {
    emit_byte(mod, 0x21);
    emit_byte(mod, modrm(3, src, dst));
}

static void emit_or_reg_reg(Module* mod, int dst, int src) {
    emit_byte(mod, 0x09);
    emit_byte(mod, modrm(3, src, dst));
}

static void emit_xor_reg_reg(Module* mod, int dst, int src) {
    emit_byte(mod, 0x31);
    emit_byte(mod, modrm(3, src, dst));
}

static void emit_shl_reg_cl(Module* mod, int reg) {
    emit_byte(mod, 0xD3);
    emit_byte(mod, modrm(3, 4, reg));
}

static void emit_shr_reg_cl(Module* mod, int reg) {
    emit_byte(mod, 0xD3);
    emit_byte(mod, modrm(3, 5, reg));
}

static void emit_sar_reg_cl(Module* mod, int reg) {
    emit_byte(mod, 0xD3);
    emit_byte(mod, modrm(3, 7, reg));
}

static void emit_shl_reg_imm(Module* mod, int reg, uint8_t amount) {
    emit_byte(mod, 0xC1);
    emit_byte(mod, modrm(3, 4, reg));
    emit_byte(mod, amount);
}

static void emit_shr_reg_imm(Module* mod, int reg, uint8_t amount) {
    emit_byte(mod, 0xC1);
    emit_byte(mod, modrm(3, 5, reg));
    emit_byte(mod, amount);
}

static void emit_sar_reg_imm(Module* mod, int reg, uint8_t amount) {
    emit_byte(mod, 0xC1);
    emit_byte(mod, modrm(3, 7, reg));
    emit_byte(mod, amount);
}

static void emit_and_reg_imm(Module* mod, int reg, uint32_t immediate) {
    emit_byte(mod, 0x81);
    emit_byte(mod, modrm(3, 4, reg));
    emit_dword(mod, immediate);
}

static void emit_cmp_reg_reg(Module* mod, int r1, int r2) {
    emit_byte(mod, 0x39);
    emit_byte(mod, modrm(3, r2, r1));
}

static void emit_cmp_reg_imm(Module* mod, int reg, int32_t imm) {
    if (imm >= -128 && imm <= 127) {
        emit_byte(mod, 0x83);
        emit_byte(mod, modrm(3, 7, reg));
        emit_byte(mod, (uint8_t)imm);
    } else {
        emit_byte(mod, 0x81);
        emit_byte(mod, modrm(3, 7, reg));
        emit_dword(mod, (uint32_t)imm);
    }
}

static void emit_test_reg_reg(Module* mod, int r1, int r2) {
    emit_byte(mod, 0x85);
    emit_byte(mod, modrm(3, r2, r1));
}

/* Jumps */
static void emit_jmp(Module* mod, int32_t offset) {
    if (offset >= -128 && offset <= 127) {
        emit_byte(mod, 0xEB);
        emit_byte(mod, (uint8_t)offset);
    } else {
        emit_byte(mod, 0xE9);
        emit_dword(mod, (uint32_t)offset);
    }
}

static void emit_jmp_rel32(Module* mod, uint32_t target) {
    emit_byte(mod, 0xE9);
    emit_dword(mod, target);
}

static void emit_jcc_rel32(Module* mod, int cc, uint32_t target) {
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0x80 + cc);
    emit_dword(mod, target);
}

/* Condition codes */
#define CC_O   0   /* Overflow */
#define CC_NO  1
#define CC_B   2   /* Below (unsigned <) */
#define CC_AE  3   /* Above or equal */
#define CC_E   4   /* Equal */
#define CC_NE  5   /* Not equal */
#define CC_BE  6   /* Below or equal */
#define CC_A   7   /* Above */
#define CC_S   8   /* Sign */
#define CC_NS  9
#define CC_P   10  /* Parity */
#define CC_NP  11
#define CC_L   12  /* Less (signed) */
#define CC_GE  13  /* Greater or equal */
#define CC_LE  14  /* Less or equal */
#define CC_G   15  /* Greater */

static void emit_setcc(Module* mod, int cc, int reg) {
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0x90 + cc);
    emit_byte(mod, modrm(3, 0, reg));
}

static void emit_call_rel32(Module* mod, uint32_t target) {
    emit_byte(mod, 0xE8);
    emit_dword(mod, target);
}

static void emit_ret(Module* mod) {
    emit_byte(mod, 0xC3);
}

static void emit_leave(Module* mod) {
    emit_byte(mod, 0xC9);
}

/* ═══════════════════════════════════════
 * Label Management
 * ═══════════════════════════════════════ */

static int new_label(void) {
    return label_counter++;
}

typedef struct LabelRef {
    int label;
    uint32_t offset;
    struct LabelRef* next;
} LabelRef;

typedef struct LabelDef {
    int label;
    uint32_t offset;
    struct LabelDef* next;
} LabelDef;

static LabelRef* label_refs = NULL;
static LabelDef* label_defs = NULL;

static const char* codegen_label_symbol(int label) {
    char name[64];
    int written = snprintf(name, sizeof(name), "__rcc_label_%d", label);
    if (written < 0 || (size_t)written >= sizeof(name)) {
        rcc_fatal("internal code label name exceeds compiler limits");
    }
    return rcc_intern(name);
}

/* Function call references for internal function patching */
typedef struct FuncCallRef {
    const char* func_name;
    uint32_t call_offset;  /* Offset where the rel32 starts */
    struct FuncCallRef* next;
} FuncCallRef;

typedef struct FuncDef {
    const char* name;
    uint32_t offset;
    struct FuncDef* next;
} FuncDef;

static FuncCallRef* func_call_refs = NULL;
static FuncDef* func_defs = NULL;

static void add_func_def(const char* name, uint32_t offset) {
    FuncDef* def = rcc_alloc(sizeof(FuncDef));
    def->name = name;
    def->offset = offset;
    def->next = func_defs;
    func_defs = def;
}

static void add_func_call_ref(const char* name, uint32_t call_offset) {
    FuncCallRef* ref = rcc_alloc(sizeof(FuncCallRef));
    ref->func_name = name;
    ref->call_offset = call_offset;
    ref->next = func_call_refs;
    func_call_refs = ref;
}

static void resolve_func_calls(Module* mod) {
    for (FuncCallRef* ref = func_call_refs; ref; ref = ref->next) {
        bool resolved = false;
        /* Find function definition */
        for (FuncDef* def = func_defs; def; def = def->next) {
            if (strcmp(def->name, ref->func_name) == 0) {
                /* Calculate relative offset */
                int32_t rel = def->offset - (ref->call_offset + 4);
                /* Patch */
                mod->code.data[ref->call_offset] = rel & 0xFF;
                mod->code.data[ref->call_offset + 1] = (rel >> 8) & 0xFF;
                mod->code.data[ref->call_offset + 2] = (rel >> 16) & 0xFF;
                mod->code.data[ref->call_offset + 3] = (rel >> 24) & 0xFF;
                resolved = true;
                break;
            }
        }
        if (!resolved) {
            module_add_relocation(mod, MODULE_SYMBOL_CODE,
                                  ref->call_offset, 0, true, false,
                                  ref->func_name);
        }
    }
}

static bool codegen_materialize_static_compound(Module* mod,
                                                Expr* expression) {
    Type* type;
    size_t alignment;
    size_t aligned;
    size_t size;
    char symbol[64];

    if (!mod || !expression || expression->kind != EXPR_COMPOUND ||
        !expression->compound_type) {
        return false;
    }
    type = expression->compound_type;
    if (!type_is_complete(type) || type->size <= 0 || type->align <= 0 ||
        (type->kind != TYPE_ARRAY && type->kind != TYPE_STRUCT &&
         type->kind != TYPE_UNION)) {
        rcc_error(expression->loc,
                  "static compound literal requires a complete aggregate type");
        return false;
    }
    if (expression->compound_static_symbol) return true;
    alignment = (size_t)type->align;
    aligned = (mod->data.size + alignment - 1u) & ~(alignment - 1u);
    size = (size_t)type->size;
    if (aligned > UINT32_MAX || size > UINT32_MAX - aligned) {
        rcc_error(expression->loc,
                  "static compound literal exceeds data limits");
        return false;
    }
    ensure_data_capacity(mod, aligned - mod->data.size + size);
    while (mod->data.size < aligned) {
        mod->data.data[mod->data.size++] = 0u;
    }
    memset(mod->data.data + mod->data.size, 0, size);
    mod->data.size += size;
    snprintf(symbol, sizeof(symbol), "__rcc_compound_%u",
             mod->compound_literal_count++);
    expression->compound_static_symbol = rcc_intern(symbol);
    module_add_symbol(mod, expression->compound_static_symbol,
                      (uint32_t)aligned, true, MODULE_SYMBOL_DATA, false);
    if (!codegen_emit_static_initializer(mod, type, expression,
                                         (uint32_t)aligned)) {
        rcc_error(expression->loc,
                  "unsupported static compound literal initializer");
        return false;
    }
    return true;
}

static void ensure_init_array_capacity(Module* mod, size_t needed) {
    if (mod->init_array.size + needed > mod->init_array.capacity) {
        while (mod->init_array.size + needed > mod->init_array.capacity) {
            mod->init_array.capacity *= 2;
        }
        mod->init_array.data = rcc_realloc(mod->init_array.data,
                                           mod->init_array.capacity);
    }
}

void codegen_add_init_array_entry(Module* mod, const char* symbol) {
    uint32_t pointer_size = g_opts.target_arch == ARCH_X64 ? 8u : 4u;
    uint32_t offset;
    if (!mod || !symbol) return;
    while ((mod->init_array.size & (pointer_size - 1u)) != 0u) {
        ensure_init_array_capacity(mod, 1u);
        mod->init_array.data[mod->init_array.size++] = 0u;
    }
    offset = (uint32_t)mod->init_array.size;
    ensure_init_array_capacity(mod, pointer_size);
    memset(mod->init_array.data + mod->init_array.size, 0, pointer_size);
    mod->init_array.size += pointer_size;
    module_add_relocation(mod, MODULE_SYMBOL_INIT_ARRAY, offset, 0u,
                          false, pointer_size == 8u, symbol);
    add_reloc(mod, MODULE_SYMBOL_INIT_ARRAY, offset,
              pointer_size == 8u ? RIN_RELOC_ABS64 : RIN_RELOC_ABS32);
}

static void ensure_fini_array_capacity(Module* mod, size_t needed) {
    if (mod->fini_array.size + needed > mod->fini_array.capacity) {
        while (mod->fini_array.size + needed > mod->fini_array.capacity) {
            mod->fini_array.capacity *= 2;
        }
        mod->fini_array.data = rcc_realloc(mod->fini_array.data,
                                           mod->fini_array.capacity);
    }
}

void codegen_add_fini_array_entry(Module* mod, const char* symbol) {
    uint32_t pointer_size = g_opts.target_arch == ARCH_X64 ? 8u : 4u;
    uint32_t offset;
    if (!mod || !symbol) return;
    while ((mod->fini_array.size & (pointer_size - 1u)) != 0u) {
        ensure_fini_array_capacity(mod, 1u);
        mod->fini_array.data[mod->fini_array.size++] = 0u;
    }
    offset = (uint32_t)mod->fini_array.size;
    ensure_fini_array_capacity(mod, pointer_size);
    memset(mod->fini_array.data + mod->fini_array.size, 0, pointer_size);
    mod->fini_array.size += pointer_size;
    module_add_relocation(mod, MODULE_SYMBOL_FINI_ARRAY, offset, 0u,
                          false, pointer_size == 8u, symbol);
    add_reloc(mod, MODULE_SYMBOL_FINI_ARRAY, offset,
              pointer_size == 8u ? RIN_RELOC_ABS64 : RIN_RELOC_ABS32);
}

static uint32_t emit_rodata(Module* mod, const void* data, size_t len) {
    uint32_t offset = (uint32_t)mod->rodata.size;
    ensure_rodata_capacity(mod, len);
    memcpy(mod->rodata.data + mod->rodata.size, data, len);
    mod->rodata.size += len;
    return offset;
}

static void codegen_add_vtable_pointer(Module* mod,
                                       ModuleSymbolSection source_section,
                                       uint32_t offset, Type* type) {
    CxxClass* cls;
    uint32_t width;
    if (!mod || !type || !codegen_type_has_vtable_storage(type)) return;
    width = g_opts.target_arch == ARCH_X64 ? 8u : 4u;
    if (type->cxx_vtable_size > 0 && type->cxx_vtable_symbol) {
        module_add_relocation(mod, source_section, offset, 0u, false,
                              width == 8u, type->cxx_vtable_symbol);
        add_reloc(mod, source_section, offset,
                  width == 8u ? RIN_RELOC_ABS64 : RIN_RELOC_ABS32);
    }

    /* Each non-virtual polymorphic base is a distinct subobject and owns a
     * vptr.  The primary base reuses the object's first word; each other
     * fixed-layout polymorphic base receives its derived table.  Never leave
     * those words zero-initialized: a virtual call must observe a real,
     * ABI-compatible table. */
    cls = type->cxx_class;
    if (!cls || !cls->base_offsets) return;
    for (int index = 0; index < cls->base_count; ++index) {
        CxxClass* base = cls->bases[index].base;
        const char* base_vtable_symbol;
        uint64_t base_offset;
        if (!base || base->vtable_size <= 0 || !base->type ||
            !base->type->cxx_vtable_symbol ||
            (!cls->bases[index].is_virtual &&
             cls->base_offsets[index] <= 0)) {
            continue;
        }
        base_vtable_symbol = base->type->cxx_vtable_symbol;
        for (int secondary_index = 0;
             secondary_index < cls->secondary_vtable_count;
             ++secondary_index) {
            CxxSecondaryVtable* secondary =
                &cls->secondary_vtables[secondary_index];
            if (secondary->base_index == index && secondary->symbol) {
                base_vtable_symbol = secondary->symbol;
                break;
            }
        }
        base_offset = (uint64_t)offset +
                      (uint32_t)cls->base_offsets[index];
        if (base_offset > UINT32_MAX) {
            rcc_error((SourceLoc){"<cxx-vtable>", 0, 0},
                      "secondary vtable pointer exceeds data limits");
            continue;
        }
        module_add_relocation(mod, source_section, (uint32_t)base_offset,
                              0u, false, width == 8u,
                              base_vtable_symbol);
        add_reloc(mod, source_section, (uint32_t)base_offset,
                  width == 8u ? RIN_RELOC_ABS64 : RIN_RELOC_ABS32);
    }
}

static void codegen_emit_cxx_vtable_storage(Module* mod,
                                            const char* symbol,
                                            int size,
                                            CxxVtableEntry* entries,
                                            const char* owner) {
    static const uint8_t zero[16] = {0};
    uint32_t pointer_size = g_opts.target_arch == ARCH_X64 ? 8u : 4u;
    uint32_t offset;
    if (!mod || !symbol || size <= 0 || !entries) return;
    while ((mod->rodata.size & (pointer_size - 1u)) != 0u) {
        emit_rodata(mod, zero, 1u);
    }
    offset = (uint32_t)mod->rodata.size;
    for (int slot = 0; slot < size; ++slot) {
        emit_rodata(mod, zero, pointer_size);
    }
    module_add_symbol(mod, symbol, offset, true, MODULE_SYMBOL_RODATA, true);
    for (int slot = 0; slot < size; ++slot) {
        CxxVtableEntry* entry = &entries[slot];
        CxxMethod* method = entry->method;
        const char* entry_symbol = entry->entry_symbol;
        if (!method || !method->decl || !method->decl->link_name) {
            rcc_error((SourceLoc){"<cxx-vtable>", 0, 0},
                      "virtual table entry %d of '%s' has no function body",
                      slot, owner ? owner : "<anonymous>");
            continue;
        }
        module_add_relocation(
            mod, MODULE_SYMBOL_RODATA,
            offset + (uint32_t)slot * pointer_size, 0u, false,
            pointer_size == 8u,
            entry_symbol ? entry_symbol : decl_link_name(method->decl));
        add_reloc(mod, MODULE_SYMBOL_RODATA,
                  offset + (uint32_t)slot * pointer_size,
                  pointer_size == 8u ? RIN_RELOC_ABS64 : RIN_RELOC_ABS32);
    }
}

static void codegen_emit_cxx_vtables_in_namespace(Module* mod,
                                                   CxxNamespace* ns) {
    if (!mod || !ns) return;
    for (int index = 0; index < ns->class_count; ++index) {
        CxxClass* cls = ns->classes[index];
        if (!cls || !cls->type) continue;
        if (cls->type->cxx_vtable_size > 0 &&
            cls->type->cxx_vtable_symbol && cls->vtable) {
            codegen_emit_cxx_vtable_storage(
                mod, cls->type->cxx_vtable_symbol, cls->vtable_size,
                cls->vtable, cls->name);
        }
        for (int secondary_index = 0;
             secondary_index < cls->secondary_vtable_count;
             ++secondary_index) {
            CxxSecondaryVtable* secondary =
                &cls->secondary_vtables[secondary_index];
            codegen_emit_cxx_vtable_storage(
                mod, secondary->symbol, secondary->size, secondary->entries,
                cls->name);
        }
    }
    for (CxxNamespace* child = ns->children; child; child = child->next) {
        codegen_emit_cxx_vtables_in_namespace(mod, child);
    }
}

static void codegen_emit_cxx_vtable_thunks32_in_namespace(
    Module* mod, CxxNamespace* ns) {
    if (!mod || !ns) return;
    for (int class_index = 0; class_index < ns->class_count; ++class_index) {
        CxxClass* cls = ns->classes[class_index];
        if (!cls) continue;
        for (int table_index = 0;
             table_index < cls->secondary_vtable_count; ++table_index) {
            CxxSecondaryVtable* table = &cls->secondary_vtables[table_index];
            int base_offset;
            if (!table->entries || table->size <= 0 ||
                table->base_index < 0 ||
                !cls->base_offsets ||
                table->base_index >= cls->base_count) {
                continue;
            }
            base_offset = cls->base_offsets[table->base_index];
            for (int slot = 0; slot < table->size; ++slot) {
                CxxVtableEntry* entry = &table->entries[slot];
                uint32_t jump_offset;
                uint32_t start;
                if (!entry->entry_symbol || !entry->method ||
                    !entry->method->decl ||
                    !entry->method->decl->link_name) {
                    continue;
                }
                start = code_offset(mod);
                add_func_def(entry->entry_symbol, start);
                module_add_symbol(mod, entry->entry_symbol, start, true,
                                  MODULE_SYMBOL_CODE, true);
                /* The ABI supplies the secondary-base pointer at [ESP+4].
                 * Mutate that argument in place and tail-jump so every
                 * remaining argument and the caller's return address retain
                 * their original ABI positions. */
                if (base_offset >= -128 && base_offset <= 127) {
                    emit_byte(mod, 0x83);
                    emit_byte(mod, 0x6C);
                    emit_byte(mod, 0x24);
                    emit_byte(mod, 0x04);
                    emit_byte(mod, (uint8_t)base_offset);
                } else {
                    emit_byte(mod, 0x81);
                    emit_byte(mod, 0x6C);
                    emit_byte(mod, 0x24);
                    emit_byte(mod, 0x04);
                    emit_dword(mod, (uint32_t)base_offset);
                }
                emit_byte(mod, 0xE9);
                jump_offset = code_offset(mod);
                emit_dword(mod, 0u);
                add_func_call_ref(decl_link_name(entry->method->decl),
                                  jump_offset);
            }
        }
    }
    for (CxxNamespace* child = ns->children; child; child = child->next) {
        codegen_emit_cxx_vtable_thunks32_in_namespace(mod, child);
    }
}

void codegen_emit_cxx_vtable_thunks32(Module* mod, CxxNamespace* ns) {
    codegen_emit_cxx_vtable_thunks32_in_namespace(mod, ns);
}

void codegen_emit_cxx_vtables(Module* mod) {
    CxxNamespace* global_namespace = codegen_cxx_global_namespace();
    if (mod && global_namespace) {
        if (g_opts.target_arch == ARCH_X64) {
            codegen_emit_cxx_vtable_thunks64(mod, global_namespace);
        } else {
            codegen_emit_cxx_vtable_thunks32(mod, global_namespace);
        }
        codegen_emit_cxx_vtables_in_namespace(mod, global_namespace);
    }
}

static void emit_label(Module* mod, int label) {
    LabelDef* def = rcc_alloc(sizeof(LabelDef));
    def->label = label;
    def->offset = code_offset(mod);
    def->next = label_defs;
    label_defs = def;
    module_add_symbol(mod, codegen_label_symbol(label), def->offset, true,
                      MODULE_SYMBOL_CODE, false);
}

static void emit_jmp_label(Module* mod, int label) {
    emit_byte(mod, 0xE9);
    LabelRef* ref = rcc_alloc(sizeof(LabelRef));
    ref->label = label;
    ref->offset = code_offset(mod);
    ref->next = label_refs;
    label_refs = ref;
    emit_dword(mod, 0);
}

static void emit_jcc_label(Module* mod, int cc, int label) {
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0x80 + cc);
    LabelRef* ref = rcc_alloc(sizeof(LabelRef));
    ref->label = label;
    ref->offset = code_offset(mod);
    ref->next = label_refs;
    label_refs = ref;
    emit_dword(mod, 0);
}

static void resolve_labels(Module* mod) {
    for (LabelRef* ref = label_refs; ref; ref = ref->next) {
        /* Find label definition */
        for (LabelDef* def = label_defs; def; def = def->next) {
            if (def->label == ref->label) {
                /* Calculate relative offset */
                int32_t rel = def->offset - (ref->offset + 4);
                /* Patch */
                mod->code.data[ref->offset] = rel & 0xFF;
                mod->code.data[ref->offset + 1] = (rel >> 8) & 0xFF;
                mod->code.data[ref->offset + 2] = (rel >> 16) & 0xFF;
                mod->code.data[ref->offset + 3] = (rel >> 24) & 0xFF;
                break;
            }
        }
    }
}

/* ═══════════════════════════════════════
 * Expression Code Generation
 * ═══════════════════════════════════════ */

/* Forward declaration */
static void gen_expr(Module* mod, Expr* expr);
static void gen_expr_raw(Module* mod, Expr* expr);
static void gen_expr64_pair(Module* mod, Expr* expr);
static void gen_expr_as_integer64(Module* mod, Expr* expr);
static void gen_expr_as_type(Module* mod, Expr* expr, Type* target_type);
static void gen_call(Module* mod, Expr* expr);
static void emit_convert_integer_value(Module* mod, int reg,
                                       const Type* source_type,
                                       const Type* target_type);

static bool codegen_type_has_vla(const Type* type) {
    return type && type->kind == TYPE_ARRAY &&
           (type->array_bound != NULL || codegen_type_has_vla(type->base));
}

/* Pointer objects have fixed storage, but a pointer-to-VLA parameter still
 * carries runtime array bounds that must be captured at function entry. */
static bool codegen_type_has_vla_any(const Type* type) {
    if (!type) return false;
    if (type->kind == TYPE_ARRAY) {
        return type->array_bound != NULL || codegen_type_has_vla_any(type->base);
    }
    return type->kind == TYPE_PTR && codegen_type_has_vla_any(type->base);
}

/* Leave the runtime byte extent of an array type in EAX.  VLA dimensions are
 * evaluated at the point where the expression is emitted; local declarations
 * save the resulting extent so later sizeof expressions observe the declared
 * object rather than a subsequently changed bound variable. */
static void gen_vla_extent(Module* mod, Type* type) {
    if (!type || type->kind != TYPE_ARRAY) {
        emit_mov_reg_imm(mod, EAX, type && type->size > 0
            ? (uint32_t)type->size : 0u);
        return;
    }
    if (type->base && type->base->kind == TYPE_ARRAY) {
        gen_vla_extent(mod, type->base);
    } else {
        emit_mov_reg_imm(mod, EAX, type->base && type->base->size > 0
            ? (uint32_t)type->base->size : 0u);
    }
    emit_push_reg(mod, EAX);
    if (type->array_bound) {
        gen_expr(mod, type->array_bound);
    } else {
        emit_mov_reg_imm(mod, EAX, type->array_len > 0
            ? (uint32_t)type->array_len : 0u);
    }
    emit_mov_reg_reg(mod, ECX, EAX);
    emit_pop_reg(mod, EAX);
    emit_imul_reg_reg(mod, EAX, ECX);
}

static int codegen_vla_dimension_count(const Type* type) {
    if (!type) return 0;
    if (type->kind == TYPE_PTR) {
        return codegen_vla_dimension_count(type->base);
    }
    if (type->kind != TYPE_ARRAY) return 0;
    return 1 + codegen_vla_dimension_count(type->base);
}

static int codegen_vla_extent_index(const Type* owner, const Type* target) {
    const Type* cursor = owner;
    int below;
    if (cursor && cursor->kind == TYPE_PTR) cursor = cursor->base;
    for (; cursor && cursor->kind == TYPE_ARRAY; cursor = cursor->base) {
        if (cursor == target) {
            below = 0;
            for (cursor = target->base;
                 cursor && cursor->kind == TYPE_ARRAY;
                 cursor = cursor->base) {
                ++below;
            }
            return below;
        }
    }
    return -1;
}

static Decl* codegen_vla_owner(Expr* expression) {
    while (expression) {
        if (expression->kind == EXPR_INDEX) {
            expression = expression->index_base;
        } else if (expression->kind == EXPR_DEREF) {
            expression = expression->unary_operand;
        } else {
            break;
        }
    }
    if (expression && expression->kind == EXPR_IDENT &&
        expression->ident_decl &&
        (expression->ident_decl->kind == DECL_VAR ||
         expression->ident_decl->kind == DECL_PARAM)) {
        return expression->ident_decl;
    }
    return NULL;
}

static void gen_vla_extents(Module* mod, Type* type, Decl* declaration,
                            int* slot_index) {
    if (type && type->kind == TYPE_PTR) {
        gen_vla_extents(mod, type->base, declaration, slot_index);
        return;
    }
    if (!type || type->kind != TYPE_ARRAY) {
        emit_mov_reg_imm(mod, EAX, type && type->size > 0
            ? (uint32_t)type->size : 0u);
        return;
    }
    if (type->base && type->base->kind == TYPE_ARRAY) {
        gen_vla_extents(mod, type->base, declaration, slot_index);
    } else {
        emit_mov_reg_imm(mod, EAX, type->base && type->base->size > 0
            ? (uint32_t)type->base->size : 0u);
    }
    emit_push_reg(mod, EAX);
    if (type->array_bound) {
        gen_expr(mod, type->array_bound);
    } else {
        emit_mov_reg_imm(mod, EAX, type->array_len > 0
            ? (uint32_t)type->array_len : 0u);
    }
    emit_mov_reg_reg(mod, ECX, EAX);
    emit_pop_reg(mod, EAX);
    emit_imul_reg_reg(mod, EAX, ECX);
    if (declaration && declaration->var_vla_extent_count > 0 &&
        slot_index && *slot_index < declaration->var_vla_extent_count) {
        emit_mov_mem_reg(mod, EBP,
                         declaration->var_vla_extent_offset +
                             *slot_index * 4,
                         EAX);
    }
    if (slot_index) ++*slot_index;
}

static bool gen_saved_vla_extent(Module* mod, Type* type, Expr* expression) {
    Decl* owner = codegen_vla_owner(expression);
    Type* owner_type;
    int index;
    if (!owner || owner->var_vla_extent_count <= 0) return false;
    owner_type = owner->param_array_type ? owner->param_array_type : owner->type;
    index = codegen_vla_extent_index(owner_type, type);
    if (index < 0 || index >= owner->var_vla_extent_count) return false;
    emit_mov_reg_mem(mod, EAX, EBP,
                     owner->var_vla_extent_offset + index * 4);
    return true;
}

static void gen_vla_extent_for_expr(Module* mod, Type* type,
                                    Expr* expression) {
    if (!gen_saved_vla_extent(mod, type, expression)) {
        gen_vla_extent(mod, type);
    }
}

static void gen_vla_alloc(Module* mod, Decl* decl) {
    int slot_index = 0;
    gen_vla_extents(mod, decl->type, decl, &slot_index);
    emit_sub_reg_reg(mod, ESP, EAX);
    emit_mov_mem_reg(mod, EBP, decl->var_vla_size_offset, EAX);
    emit_mov_reg_reg(mod, EAX, ESP);
    emit_mov_mem_reg(mod, EBP, decl->var_offset, EAX);
}

static void codegen_assign_vla_parameter_slots(Decl* decl, int* stack_size) {
    if (!decl || !stack_size) return;
    for (DeclList* parameter = decl->func_params; parameter;
         parameter = parameter->next) {
        Decl* value = parameter->decl;
        int dimensions;
        int64_t bytes;
        if (!value || !value->param_array_type ||
            !codegen_type_has_vla_any(value->param_array_type)) {
            continue;
        }
        dimensions = codegen_vla_dimension_count(value->param_array_type);
        bytes = (int64_t)dimensions * 4;
        if (dimensions <= 0 || bytes > INT_MAX - *stack_size) {
            rcc_error(value->loc,
                      "function stack frame exceeds compiler limits");
            continue;
        }
        *stack_size += (int)bytes;
        value->var_vla_extent_offset = -*stack_size;
        value->var_vla_extent_count = dimensions;
    }
}

static void gen_vla_parameter_extents(Module* mod, Decl* decl) {
    if (!decl) return;
    for (DeclList* parameter = decl->func_params; parameter;
         parameter = parameter->next) {
        Decl* value = parameter->decl;
        int slot_index = 0;
        if (!value || value->var_vla_extent_count <= 0 ||
            !value->param_array_type) {
            continue;
        }
        gen_vla_extents(mod, value->param_array_type, value, &slot_index);
    }
}

static bool gen_is_integer64(const Type* type) {
    return type && type->size == 8 &&
           type_is_integer((Type*)type);
}

static bool gen_is_floating(const Type* type) {
    return type && (type->kind == TYPE_FLOAT || type->kind == TYPE_DOUBLE);
}

static int gen_float_width(const Type* type) {
    return type && type->kind == TYPE_FLOAT ? 4 : 8;
}

static void emit_test_reg_imm(Module* mod, int reg, uint32_t imm);

/* x87 memory encodings used by the i686 scalar floating path.  The ordinary
 * expression representation stays in integer registers so that existing
 * lvalue, call, and control-flow lowering remains composable. */
static void emit_x87_memory(Module* mod, int opcode, int group, int base,
                            int32_t displacement) {
    emit_byte(mod, (uint8_t)opcode);
    emit_memory_operand32(mod, group, base, displacement);
}

static void emit_x87_load_memory(Module* mod, int width, int base,
                                 int32_t displacement) {
    emit_x87_memory(mod, width == 4 ? 0xD9 : 0xDD, 0, base, displacement);
}

static void emit_x87_store_pop_memory(Module* mod, int width, int base,
                                      int32_t displacement) {
    emit_x87_memory(mod, width == 4 ? 0xD9 : 0xDD, 3, base, displacement);
}

static void emit_x87_load_integer_memory(Module* mod, int width, int base,
                                         int32_t displacement) {
    if (width >= 8) {
        emit_x87_memory(mod, 0xDF, 5, base, displacement); /* FILD m64 */
    } else {
        emit_x87_memory(mod, 0xDB, 0, base, displacement); /* FILD m32 */
    }
}

static void emit_x87_store_integer_pop_memory(Module* mod, int width,
                                              int base, int32_t displacement) {
    if (width >= 8) {
        emit_x87_memory(mod, 0xDF, 7, base, displacement); /* FISTP m64 */
    } else {
        emit_x87_memory(mod, 0xDB, 3, base, displacement); /* FISTP m32 */
    }
}

static void emit_x87_fld_one(Module* mod) {
    emit_byte(mod, 0xD9);
    emit_byte(mod, 0xE8); /* FLD1 */
}

static void emit_x87_fld_zero(Module* mod) {
    emit_byte(mod, 0xD9);
    emit_byte(mod, 0xEE); /* FLDZ */
}

static void emit_x87_faddp_st1(Module* mod) {
    emit_byte(mod, 0xDE);
    emit_byte(mod, 0xC1); /* FADDP ST(1), ST(0) */
}

static void emit_x87_fsubp_st1(Module* mod) {
    emit_byte(mod, 0xDE);
    emit_byte(mod, 0xE9); /* FSUBP ST(1), ST(0) */
}

static void emit_x87_fucomip_st1(Module* mod) {
    emit_byte(mod, 0xDF);
    emit_byte(mod, 0xE9); /* FUCOMIP ST(1), ST(0), then pop ST(0) */
}

static void emit_x87_fstp_st0(Module* mod) {
    emit_byte(mod, 0xDD);
    emit_byte(mod, 0xD8); /* FSTP ST(0) */
}

static void emit_x87_double_value_from_raw(Module* mod, const Type* type) {
    if (gen_float_width(type) == 4) {
        emit_push_reg(mod, EAX);
        emit_x87_load_memory(mod, 4, ESP, 0);
        emit_add_reg_imm(mod, ESP, 4);
    } else {
        emit_push_reg(mod, EDX);
        emit_push_reg(mod, EAX);
        emit_x87_load_memory(mod, 8, ESP, 0);
        emit_add_reg_imm(mod, ESP, 8);
    }
}

static void emit_raw_from_x87_value(Module* mod, const Type* type) {
    int width = gen_float_width(type);
    emit_sub_reg_imm(mod, ESP, width == 4 ? 4 : 8);
    emit_x87_store_pop_memory(mod, width, ESP, 0);
    if (width == 4) {
        emit_mov_reg_mem(mod, EAX, ESP, 0);
        emit_add_reg_imm(mod, ESP, 4);
    } else {
        emit_mov_reg_mem(mod, EAX, ESP, 0);
        emit_mov_reg_mem(mod, EDX, ESP, 4);
        emit_add_reg_imm(mod, ESP, 8);
    }
}

static void emit_load_floating_raw(Module* mod, const Type* type, int base,
                                   int32_t displacement) {
    if (gen_float_width(type) == 8) {
        if (base == EAX) {
            emit_mov_reg_mem(mod, EDX, base, displacement + 4);
            emit_mov_reg_mem(mod, EAX, base, displacement);
        } else {
            emit_mov_reg_mem(mod, EAX, base, displacement);
            emit_mov_reg_mem(mod, EDX, base, displacement + 4);
        }
    } else {
        emit_mov_reg_mem(mod, EAX, base, displacement);
    }
}

static void emit_store_floating_raw(Module* mod, const Type* type, int base,
                                    int32_t displacement) {
    emit_mov_mem_reg(mod, base, displacement, EAX);
    if (gen_float_width(type) == 8) {
        emit_mov_mem_reg(mod, base, displacement + 4, EDX);
    }
}

static void emit_x87_compare_result(Module* mod, int expression_kind);

static void emit_x87_binary_memory(Module* mod, int operation, int width,
                                    int base, int32_t displacement) {
    int opcode = width == 4 ? 0xD8 : 0xDC;
    int group;
    switch (operation) {
        case EXPR_ADD: group = 0; break;
        case EXPR_SUB: group = 4; break;
        case EXPR_MUL: group = 1; break;
        case EXPR_DIV: group = 6; break;
        default: group = 0; break;
    }
    emit_x87_memory(mod, opcode, group, base, displacement);
}

static void emit_x87_binary_stack(Module* mod, const Type* type,
                                   int operation) {
    int width = gen_float_width(type);
    if (width == 4) {
        emit_x87_load_memory(mod, 4, ESP, 4); /* lhs */
        emit_x87_binary_memory(mod, operation, 4, ESP, 0); /* rhs */
        emit_x87_store_pop_memory(mod, 4, ESP, 4);
        emit_add_reg_imm(mod, ESP, 4);
        emit_pop_reg(mod, EAX);
    } else {
        emit_x87_load_memory(mod, 8, ESP, 8); /* lhs */
        emit_x87_binary_memory(mod, operation, 8, ESP, 0); /* rhs */
        emit_x87_store_pop_memory(mod, 8, ESP, 8);
        emit_add_reg_imm(mod, ESP, 8);
        emit_pop_reg(mod, EAX);
        emit_pop_reg(mod, EDX);
    }
}

static void emit_x87_compare_stack(Module* mod, const Type* type,
                                   int expression_kind) {
    int width = gen_float_width(type);
    int lhs_offset = width == 4 ? 4 : 8;
    /* DF E9 compares ST(1) with ST(0) and pops ST(0).  Load rhs first so
     * lhs is ST(0), preserving the source-order comparison in EFLAGS. */
    emit_x87_load_memory(mod, width, ESP, 0);
    emit_x87_load_memory(mod, width, ESP, lhs_offset);
    emit_x87_fucomip_st1(mod);
    emit_x87_fstp_st0(mod);
    emit_x87_compare_result(mod, expression_kind);
    /* FUCOMIP writes EFLAGS.  Do not adjust ESP until the result has been
     * materialized because ADD would overwrite those flags. */
    emit_add_reg_imm(mod, ESP, width == 4 ? 8 : 16);
}

static void emit_x87_power_of_two(Module* mod, int exponent) {
    int count;
    emit_x87_fld_one(mod);
    for (count = 0; count < exponent; ++count) {
        emit_byte(mod, 0xD8);
        emit_byte(mod, 0xC0); /* FADD ST(0), ST(0) */
    }
}

static void emit_x87_convert_integer_to_float(Module* mod,
                                               const Type* source_type,
                                               const Type* destination_type) {
    int source_width = source_type && source_type->size >= 8 ? 8 : 4;
    int exponent = source_width == 8 ? 64 : 32;
    int unsigned_high_label = new_label();
    int converted_label = new_label();

    emit_sub_reg_imm(mod, ESP, 8);
    emit_mov_mem_reg(mod, ESP, 0, EAX);
    if (source_width == 8) {
        emit_mov_mem_reg(mod, ESP, 4, EDX);
    } else {
        emit_mov_mem_reg(mod, ESP, 4, EDX);
    }

    if (source_type && source_type->is_unsigned && source_width == 4) {
        emit_test_reg_imm(mod, EAX, UINT32_C(0x80000000));
        emit_jcc_label(mod, CC_S, unsigned_high_label);
    } else if (source_type && source_type->is_unsigned && source_width == 8) {
        emit_test_reg_imm(mod, EDX, UINT32_C(0x80000000));
        emit_jcc_label(mod, CC_S, unsigned_high_label);
    }

    emit_x87_load_integer_memory(mod, source_width, ESP, 0);
    emit_jmp_label(mod, converted_label);

    if (source_type && source_type->is_unsigned) {
        emit_label(mod, unsigned_high_label);
        /* FILD is signed.  For the high unsigned half, add 2^32/2^64
         * exactly, built with x87 doubling so no data relocation is needed. */
        emit_x87_load_integer_memory(mod, source_width, ESP, 0);
        emit_x87_power_of_two(mod, exponent);
        emit_x87_faddp_st1(mod);
    }
    emit_label(mod, converted_label);
    emit_x87_store_pop_memory(mod, gen_float_width(destination_type), ESP, 0);
    if (gen_float_width(destination_type) == 4) {
        emit_mov_reg_mem(mod, EAX, ESP, 0);
    } else {
        emit_mov_reg_mem(mod, EAX, ESP, 0);
        emit_mov_reg_mem(mod, EDX, ESP, 4);
    }
    emit_add_reg_imm(mod, ESP, 8);
}

static void emit_x87_convert_float_to_float(Module* mod,
                                             const Type* source_type,
                                             const Type* destination_type) {
    int source_width = gen_float_width(source_type);
    int destination_width = gen_float_width(destination_type);
    if (source_width == destination_width) return;
    emit_sub_reg_imm(mod, ESP, 8);
    if (source_width == 4) {
        emit_mov_mem_reg(mod, ESP, 0, EAX);
        emit_x87_load_memory(mod, 4, ESP, 0);
    } else {
        emit_mov_mem_reg(mod, ESP, 0, EAX);
        emit_mov_mem_reg(mod, ESP, 4, EDX);
        emit_x87_load_memory(mod, 8, ESP, 0);
    }
    emit_x87_store_pop_memory(mod, destination_width, ESP, 0);
    if (destination_width == 4) {
        emit_mov_reg_mem(mod, EAX, ESP, 0);
    } else {
        emit_mov_reg_mem(mod, EAX, ESP, 0);
        emit_mov_reg_mem(mod, EDX, ESP, 4);
    }
    emit_add_reg_imm(mod, ESP, 8);
}

static void emit_x87_convert_float_to_integer(Module* mod,
                                               const Type* source_type,
                                               const Type* destination_type) {
    /* Save and restore the x87 control word so C casts truncate toward zero
     * without changing the caller's floating-point environment.  A 64-bit
     * temporary also covers every 32-bit signed/unsigned result range. */
    int source_width = gen_float_width(source_type);
    emit_sub_reg_imm(mod, ESP, 16);
    /* Preserve the raw source before FNSTCW uses EAX as a scratch register. */
    emit_mov_mem_reg(mod, ESP, 4, EAX);
    if (source_width == 8) {
        emit_mov_mem_reg(mod, ESP, 8, EDX);
    }
    emit_x87_memory(mod, 0xD9, 7, ESP, 0); /* FNSTCW */
    emit_load_typed32(mod, EAX, ESP, 0, type_ushort);
    emit_mov_reg_imm(mod, ECX, UINT32_C(0x0C00));
    emit_or_reg_reg(mod, EAX, ECX);
    emit_store_typed32(mod, ESP, 2, EAX, type_ushort);
    emit_x87_memory(mod, 0xD9, 5, ESP, 2); /* FLDCW */
    emit_x87_load_memory(mod, source_width, ESP, 4);
    emit_x87_store_integer_pop_memory(mod, 8, ESP, 8);
    emit_x87_memory(mod, 0xD9, 5, ESP, 0); /* FLDCW */
    emit_mov_reg_mem(mod, EAX, ESP, 8);
    if (destination_type && destination_type->size >= 8) {
        emit_mov_reg_mem(mod, EDX, ESP, 12);
    }
    emit_add_reg_imm(mod, ESP, 16);
}

static void emit_x87_floating_truth(Module* mod, const Type* type) {
    int width = gen_float_width(type);
    if (width == 4) {
        emit_push_reg(mod, EAX);
    } else {
        emit_push_reg(mod, EDX);
        emit_push_reg(mod, EAX);
    }
    emit_x87_fld_zero(mod);
    emit_x87_load_memory(mod, width, ESP, 0);
    emit_x87_fucomip_st1(mod);
    emit_x87_fstp_st0(mod);
    emit_setcc(mod, CC_NE, EAX);
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0xB6);
    emit_byte(mod, modrm(3, EAX, EAX));
    emit_setcc(mod, CC_P, ECX);
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0xB6);
    emit_byte(mod, modrm(3, ECX, ECX));
    emit_or_reg_reg(mod, EAX, ECX);
    /* The x87 comparison flags must be consumed before ADD changes them. */
    emit_add_reg_imm(mod, ESP, width == 4 ? 4 : 8);
}

static void emit_x87_compare_result(Module* mod, int expression_kind) {
    int first_cc;
    int second_cc = CC_NP;
    bool disjunction = false;
    switch (expression_kind) {
        case EXPR_EQ: first_cc = CC_E; break;
        case EXPR_NE: first_cc = CC_NE; second_cc = CC_P;
                      disjunction = true; break;
        case EXPR_LT: first_cc = CC_B; break;
        case EXPR_GT: first_cc = CC_A; break;
        case EXPR_LE: first_cc = CC_BE; break;
        case EXPR_GE: first_cc = CC_AE; break;
        default: first_cc = CC_E; break;
    }
    emit_setcc(mod, first_cc, EAX);
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0xB6);
    emit_byte(mod, modrm(3, EAX, EAX));
    emit_setcc(mod, second_cc, ECX);
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0xB6);
    emit_byte(mod, modrm(3, ECX, ECX));
    if (disjunction) emit_or_reg_reg(mod, EAX, ECX);
    else emit_and_reg_reg(mod, EAX, ECX);
}

static void emit_x87_add_one_raw(Module* mod, const Type* type,
                                 bool subtract) {
    int width = gen_float_width(type);
    emit_sub_reg_imm(mod, ESP, width == 4 ? 4 : 8);
    if (width == 4) {
        emit_mov_mem_reg(mod, ESP, 0, EAX);
    } else {
        emit_mov_mem_reg(mod, ESP, 0, EAX);
        emit_mov_mem_reg(mod, ESP, 4, EDX);
    }
    emit_x87_load_memory(mod, width, ESP, 0);
    emit_x87_fld_one(mod);
    if (subtract) emit_x87_fsubp_st1(mod);
    else emit_x87_faddp_st1(mod);
    emit_x87_store_pop_memory(mod, width, ESP, 0);
    if (width == 4) emit_mov_reg_mem(mod, EAX, ESP, 0);
    else {
        emit_mov_reg_mem(mod, EAX, ESP, 0);
        emit_mov_reg_mem(mod, EDX, ESP, 4);
    }
    emit_add_reg_imm(mod, ESP, width == 4 ? 4 : 8);
}

static void gen_expr_as_type(Module* mod, Expr* expr, Type* target_type) {
    Type* source_type = expr ? expr->type : NULL;
    if (!expr || !target_type) {
        gen_expr(mod, expr);
        return;
    }
    gen_expr(mod, expr);
    if (gen_is_floating(target_type)) {
        if (gen_is_floating(source_type)) {
            emit_x87_convert_float_to_float(mod, source_type, target_type);
        } else if (type_is_integer(source_type)) {
            emit_x87_convert_integer_to_float(mod, source_type, target_type);
        }
    } else if (gen_is_floating(source_type)) {
        if (target_type->kind == TYPE_BOOL) {
            emit_x87_floating_truth(mod, source_type);
        } else if (type_is_integer(target_type)) {
            emit_x87_convert_float_to_integer(mod, source_type, target_type);
        }
    }
}

static void emit_test_reg_imm(Module* mod, int reg, uint32_t imm) {
    emit_byte(mod, 0xF7);
    emit_byte(mod, modrm(3, 0, reg));
    emit_dword(mod, imm);
}

static void emit_shld_reg_cl(Module* mod, int dst, int src) {
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0xA5);
    emit_byte(mod, modrm(3, src, dst));
}

static void emit_shrd_reg_cl(Module* mod, int dst, int src) {
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0xAD);
    emit_byte(mod, modrm(3, src, dst));
}

static void emit_extend_eax_to_integer64(Module* mod,
                                         const Type* source_type) {
    if (source_type && type_is_integer((Type*)source_type) &&
        !source_type->is_unsigned) {
        emit_cdq(mod);
    } else {
        emit_mov_reg_imm(mod, EDX, 0u);
    }
}

static Expr* call_argument(Expr* call, int index) {
    ExprList* argument = call->call_args;
    while (argument && index-- > 0) argument = argument->next;
    return argument ? argument->expr : NULL;
}

static const Type* atomic_value_type(Expr* call) {
    Expr* object = call_argument(call, 0);
    return object && object->type && object->type->kind == TYPE_PTR
        ? object->type->base : type_uint;
}

static void emit_normalize_atomic_value(Module* mod, int reg,
                                        const Type* type) {
    int width = gen_type_width32(type);
    if (type && type->kind == TYPE_BOOL) {
        emit_byte(mod, 0x85); /* test reg, reg */
        emit_byte(mod, modrm(3, reg, reg));
        emit_setcc(mod, CC_NE, reg);
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xB6);
        emit_byte(mod, modrm(3, reg, reg));
    } else if (width < 4) {
        emit_byte(mod, 0x0F);
        emit_byte(mod, type && !type->is_unsigned
            ? (width == 1 ? 0xBE : 0xBF)
            : (width == 1 ? 0xB6 : 0xB7));
        emit_byte(mod, modrm(3, reg, reg));
    }
}

static void emit_atomic_exchange_width(Module* mod, int value, int address,
                                       const Type* type) {
    int width = gen_type_width32(type);
    if (width == 2) emit_byte(mod, 0x66);
    emit_byte(mod, width == 1 ? 0x86 : 0x87);
    emit_memory_operand32(mod, value, address, 0);
}

static void emit_atomic_xadd_width(Module* mod, int value, int address,
                                   const Type* type) {
    int width = gen_type_width32(type);
    if (width == 2) emit_byte(mod, 0x66);
    emit_byte(mod, 0xF0);
    emit_byte(mod, 0x0F);
    emit_byte(mod, width == 1 ? 0xC0 : 0xC1);
    emit_memory_operand32(mod, value, address, 0);
}

static void emit_atomic_cmpxchg_width(Module* mod, int desired, int address,
                                      const Type* type) {
    int width = gen_type_width32(type);
    if (width == 2) emit_byte(mod, 0x66);
    emit_byte(mod, 0xF0);
    emit_byte(mod, 0x0F);
    emit_byte(mod, width == 1 ? 0xB0 : 0xB1);
    emit_memory_operand32(mod, desired, address, 0);
}

static void emit_atomic_clear_width(Module* mod, int address,
                                    const Type* type) {
    int width = gen_type_width32(type);
    if (width == 2) emit_byte(mod, 0x66);
    emit_byte(mod, width == 1 ? 0xC6 : 0xC7);
    emit_memory_operand32(mod, 0, address, 0);
    if (width == 1) emit_byte(mod, 0u);
    else if (width == 2) {
        emit_byte(mod, 0u);
        emit_byte(mod, 0u);
    } else {
        emit_dword(mod, 0u);
    }
}

static void emit_atomic_cmpxchg8b(Module* mod, int address) {
    emit_byte(mod, 0xF0);
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0xC7);
    emit_memory_operand32(mod, 1, address, 0);
}

static bool gen_atomic_builtin64_i686(Module* mod, Expr* call,
                                      const char* name) {
    int retry_label;

    if ((strncmp(name, "__atomic_", 9) != 0 &&
         strncmp(name, "__sync_", 7) != 0) ||
        g_opts.target_arch != ARCH_X86 ||
        !gen_is_integer64(atomic_value_type(call))) {
        return false;
    }

    if (strcmp(name, "__atomic_load_n") == 0) {
        gen_expr(mod, call_argument(call, 1));
        emit_push_reg(mod, EBX);
        emit_push_reg(mod, ESI);
        gen_expr(mod, call_argument(call, 0));
        emit_mov_reg_reg(mod, ESI, EAX);
        emit_xor_reg_reg(mod, EAX, EAX);
        emit_xor_reg_reg(mod, EDX, EDX);
        emit_mov_reg_reg(mod, EBX, EAX);
        emit_mov_reg_reg(mod, ECX, EDX);
        /* A zero compare either observes zero on success or receives the
         * complete memory value in EDX:EAX on failure. */
        emit_atomic_cmpxchg8b(mod, ESI);
        emit_pop_reg(mod, ESI);
        emit_pop_reg(mod, EBX);
        return true;
    }

    if (strcmp(name, "__atomic_store_n") == 0 ||
        strcmp(name, "__atomic_exchange_n") == 0 ||
        strcmp(name, "__sync_lock_test_and_set") == 0) {
        bool is_atomic_store = strcmp(name, "__atomic_store_n") == 0;
        bool has_order = strncmp(name, "__atomic_", 9) == 0;
        if (has_order) {
            gen_expr(mod, call_argument(call, 2));
        }
        emit_push_reg(mod, EBX);
        emit_push_reg(mod, ESI);
        gen_expr(mod, call_argument(call, 0));
        emit_mov_reg_reg(mod, ESI, EAX);
        gen_expr_as_integer64(mod, call_argument(call, 1));
        emit_mov_reg_reg(mod, EBX, EAX);
        emit_mov_reg_reg(mod, ECX, EDX);
        emit_mov_reg_mem(mod, EAX, ESI, 0);
        emit_mov_reg_mem(mod, EDX, ESI, 4);
        retry_label = new_label();
        emit_label(mod, retry_label);
        emit_atomic_cmpxchg8b(mod, ESI);
        emit_jcc_label(mod, CC_NE, retry_label);
        if (is_atomic_store) {
            emit_mov_reg_imm(mod, EAX, 0u);
            emit_mov_reg_imm(mod, EDX, 0u);
        }
        emit_pop_reg(mod, ESI);
        emit_pop_reg(mod, EBX);
        return true;
    }

    if (strcmp(name, "__atomic_compare_exchange_n") == 0) {
        gen_expr(mod, call_argument(call, 5));
        gen_expr(mod, call_argument(call, 4));
        gen_expr(mod, call_argument(call, 3));
        emit_push_reg(mod, EBX);
        emit_push_reg(mod, ESI);
        emit_push_reg(mod, EDI);
        gen_expr(mod, call_argument(call, 0));
        emit_mov_reg_reg(mod, ESI, EAX);
        gen_expr(mod, call_argument(call, 1));
        emit_mov_reg_reg(mod, EDI, EAX);
        gen_expr_as_integer64(mod, call_argument(call, 2));
        emit_mov_reg_reg(mod, EBX, EAX);
        emit_mov_reg_reg(mod, ECX, EDX);
        emit_mov_reg_mem(mod, EAX, EDI, 0);
        emit_mov_reg_mem(mod, EDX, EDI, 4);
        emit_atomic_cmpxchg8b(mod, ESI);
        emit_setcc(mod, CC_E, ECX);
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xB6);
        emit_byte(mod, modrm(3, ECX, ECX));
        emit_mov_mem_reg(mod, EDI, 0, EAX);
        emit_mov_mem_reg(mod, EDI, 4, EDX);
        emit_mov_reg_reg(mod, EAX, ECX);
        emit_pop_reg(mod, EDI);
        emit_pop_reg(mod, ESI);
        emit_pop_reg(mod, EBX);
        return true;
    }

    if (strcmp(name, "__sync_bool_compare_and_swap") == 0 ||
        strcmp(name, "__sync_val_compare_and_swap") == 0) {
        bool returns_bool =
            strcmp(name, "__sync_bool_compare_and_swap") == 0;
        emit_push_reg(mod, EBX);
        emit_push_reg(mod, ESI);
        gen_expr(mod, call_argument(call, 0));
        emit_mov_reg_reg(mod, ESI, EAX);
        gen_expr_as_integer64(mod, call_argument(call, 1));
        emit_push_reg(mod, EDX);
        emit_push_reg(mod, EAX);
        gen_expr_as_integer64(mod, call_argument(call, 2));
        emit_mov_reg_reg(mod, EBX, EAX);
        emit_mov_reg_reg(mod, ECX, EDX);
        emit_pop_reg(mod, EAX);
        emit_pop_reg(mod, EDX);
        emit_atomic_cmpxchg8b(mod, ESI);
        if (returns_bool) {
            emit_setcc(mod, CC_E, ECX);
            emit_byte(mod, 0x0F);
            emit_byte(mod, 0xB6);
            emit_byte(mod, modrm(3, ECX, ECX));
            emit_mov_reg_reg(mod, EAX, ECX);
        }
        emit_pop_reg(mod, ESI);
        emit_pop_reg(mod, EBX);
        return true;
    }

    if (strcmp(name, "__sync_lock_release") == 0) {
        emit_push_reg(mod, EBX);
        emit_push_reg(mod, ESI);
        gen_expr(mod, call_argument(call, 0));
        emit_mov_reg_reg(mod, ESI, EAX);
        emit_xor_reg_reg(mod, EBX, EBX);
        emit_xor_reg_reg(mod, ECX, ECX);
        emit_mov_reg_mem(mod, EAX, ESI, 0);
        emit_mov_reg_mem(mod, EDX, ESI, 4);
        retry_label = new_label();
        emit_label(mod, retry_label);
        emit_atomic_cmpxchg8b(mod, ESI);
        emit_jcc_label(mod, CC_NE, retry_label);
        emit_pop_reg(mod, ESI);
        emit_pop_reg(mod, EBX);
        return true;
    }

    {
        bool is_atomic = strncmp(name, "__atomic_", 9) == 0;
        bool is_subtract = strcmp(name, "__atomic_fetch_sub") == 0 ||
            strcmp(name, "__atomic_sub_fetch") == 0 ||
            strcmp(name, "__sync_fetch_and_sub") == 0 ||
            strcmp(name, "__sync_sub_and_fetch") == 0;
        bool is_add = strcmp(name, "__atomic_fetch_add") == 0 ||
            strcmp(name, "__atomic_add_fetch") == 0 ||
            strcmp(name, "__sync_fetch_and_add") == 0 ||
            strcmp(name, "__sync_add_and_fetch") == 0;
        bool is_and = strcmp(name, "__atomic_fetch_and") == 0 ||
            strcmp(name, "__atomic_and_fetch") == 0 ||
            strcmp(name, "__sync_fetch_and_and") == 0 ||
            strcmp(name, "__sync_and_and_fetch") == 0;
        bool is_or = strcmp(name, "__atomic_fetch_or") == 0 ||
            strcmp(name, "__atomic_or_fetch") == 0 ||
            strcmp(name, "__sync_fetch_and_or") == 0 ||
            strcmp(name, "__sync_or_and_fetch") == 0;
        bool is_xor = strcmp(name, "__atomic_fetch_xor") == 0 ||
            strcmp(name, "__atomic_xor_fetch") == 0 ||
            strcmp(name, "__sync_fetch_and_xor") == 0 ||
            strcmp(name, "__sync_xor_and_fetch") == 0;
        bool is_nand = strcmp(name, "__atomic_fetch_nand") == 0 ||
            strcmp(name, "__atomic_nand_fetch") == 0 ||
            strcmp(name, "__sync_fetch_and_nand") == 0 ||
            strcmp(name, "__sync_nand_and_fetch") == 0;
        bool returns_new = strcmp(name, "__atomic_add_fetch") == 0 ||
            strcmp(name, "__atomic_sub_fetch") == 0 ||
            strcmp(name, "__atomic_and_fetch") == 0 ||
            strcmp(name, "__atomic_or_fetch") == 0 ||
            strcmp(name, "__atomic_xor_fetch") == 0 ||
            strcmp(name, "__atomic_nand_fetch") == 0 ||
            strcmp(name, "__sync_add_and_fetch") == 0 ||
            strcmp(name, "__sync_sub_and_fetch") == 0 ||
            strcmp(name, "__sync_and_and_fetch") == 0 ||
            strcmp(name, "__sync_or_and_fetch") == 0 ||
            strcmp(name, "__sync_xor_and_fetch") == 0 ||
            strcmp(name, "__sync_nand_and_fetch") == 0;

        if (is_add || is_subtract || is_and || is_or || is_xor || is_nand) {
            if (is_atomic) gen_expr(mod, call_argument(call, 2));
            emit_push_reg(mod, EBX);
            emit_push_reg(mod, ESI);
            emit_push_reg(mod, EDI);
            gen_expr(mod, call_argument(call, 0));
            emit_mov_reg_reg(mod, ESI, EAX);
            gen_expr_as_integer64(mod, call_argument(call, 1));
            emit_push_reg(mod, EDX);
            emit_push_reg(mod, EAX);
            emit_mov_reg_mem(mod, EAX, ESI, 0);
            emit_mov_reg_mem(mod, EDX, ESI, 4);
            retry_label = new_label();
            emit_label(mod, retry_label);
            emit_mov_reg_reg(mod, EBX, EAX);
            emit_mov_reg_reg(mod, ECX, EDX);
            emit_mov_reg_mem(mod, EDI, ESP, 0);
            if (is_add) {
                emit_add_reg_reg(mod, EBX, EDI);
            } else if (is_subtract) {
                emit_sub_reg_reg(mod, EBX, EDI);
            } else if (is_and || is_nand) {
                emit_and_reg_reg(mod, EBX, EDI);
            } else if (is_or) {
                emit_or_reg_reg(mod, EBX, EDI);
            } else {
                emit_xor_reg_reg(mod, EBX, EDI);
            }
            emit_mov_reg_mem(mod, EDI, ESP, 4);
            if (is_add) {
                emit_adc_reg_reg(mod, ECX, EDI);
            } else if (is_subtract) {
                emit_sbb_reg_reg(mod, ECX, EDI);
            } else if (is_and || is_nand) {
                emit_and_reg_reg(mod, ECX, EDI);
            } else if (is_or) {
                emit_or_reg_reg(mod, ECX, EDI);
            } else {
                emit_xor_reg_reg(mod, ECX, EDI);
            }
            if (is_nand) {
                emit_not_reg(mod, EBX);
                emit_not_reg(mod, ECX);
            }
            emit_atomic_cmpxchg8b(mod, ESI);
            emit_jcc_label(mod, CC_NE, retry_label);
            if (returns_new) {
                emit_mov_reg_reg(mod, EAX, EBX);
                emit_mov_reg_reg(mod, EDX, ECX);
            }
            emit_add_reg_imm(mod, ESP, 8);
            emit_pop_reg(mod, EDI);
            emit_pop_reg(mod, ESI);
            emit_pop_reg(mod, EBX);
            return true;
        }
    }

    rcc_error(call->loc,
              "%s i686 64-bit lowering is not implemented yet", name);
    return false;
}

typedef enum {
    ATOMIC_BITWISE_NONE,
    ATOMIC_BITWISE_AND,
    ATOMIC_BITWISE_OR,
    ATOMIC_BITWISE_XOR,
    ATOMIC_BITWISE_NAND
} AtomicBitwiseOp;

static AtomicBitwiseOp atomic_bitwise_operation(const char* name) {
    if (strcmp(name, "__atomic_fetch_and") == 0 ||
        strcmp(name, "__atomic_and_fetch") == 0 ||
        strcmp(name, "__sync_fetch_and_and") == 0 ||
        strcmp(name, "__sync_and_and_fetch") == 0) {
        return ATOMIC_BITWISE_AND;
    }
    if (strcmp(name, "__atomic_fetch_or") == 0 ||
        strcmp(name, "__atomic_or_fetch") == 0 ||
        strcmp(name, "__sync_fetch_and_or") == 0 ||
        strcmp(name, "__sync_or_and_fetch") == 0) {
        return ATOMIC_BITWISE_OR;
    }
    if (strcmp(name, "__atomic_fetch_xor") == 0 ||
        strcmp(name, "__atomic_xor_fetch") == 0 ||
        strcmp(name, "__sync_fetch_and_xor") == 0 ||
        strcmp(name, "__sync_xor_and_fetch") == 0) {
        return ATOMIC_BITWISE_XOR;
    }
    if (strcmp(name, "__atomic_fetch_nand") == 0 ||
        strcmp(name, "__atomic_nand_fetch") == 0 ||
        strcmp(name, "__sync_fetch_and_nand") == 0 ||
        strcmp(name, "__sync_nand_and_fetch") == 0) {
        return ATOMIC_BITWISE_NAND;
    }
    return ATOMIC_BITWISE_NONE;
}

static bool atomic_bitwise_returns_new(const char* name) {
    return strcmp(name, "__atomic_and_fetch") == 0 ||
           strcmp(name, "__atomic_or_fetch") == 0 ||
           strcmp(name, "__atomic_xor_fetch") == 0 ||
           strcmp(name, "__atomic_nand_fetch") == 0 ||
           strcmp(name, "__sync_and_and_fetch") == 0 ||
           strcmp(name, "__sync_or_and_fetch") == 0 ||
           strcmp(name, "__sync_xor_and_fetch") == 0 ||
           strcmp(name, "__sync_nand_and_fetch") == 0;
}

static bool gen_atomic_builtin(Module* mod, Expr* call) {
    Expr* function = call->call_func;
    const char* name;
    bool is_atomic;
    bool is_subtract;
    bool returns_new;
    AtomicBitwiseOp bitwise_operation;
    const Type* value_type;

    if (!function || function->kind != EXPR_IDENT) return false;
    name = function->ident_name;
    value_type = atomic_value_type(call);
    if (gen_atomic_builtin64_i686(mod, call, name)) return true;
    if (strcmp(name, "__atomic_load_n") == 0) {
        gen_expr(mod, call_argument(call, 1));
        gen_expr(mod, call_argument(call, 0));
        emit_load_typed32(mod, EAX, EAX, 0, value_type);
        return true;
    }
    if (strcmp(name, "__atomic_store_n") == 0) {
        gen_expr(mod, call_argument(call, 2));
        gen_expr(mod, call_argument(call, 0));
        emit_push_reg(mod, EAX);
        gen_expr(mod, call_argument(call, 1));
        emit_normalize_atomic_value(mod, EAX, value_type);
        emit_pop_reg(mod, ECX);
        /* XCHG with memory is implicitly locked. */
        emit_atomic_exchange_width(mod, EAX, ECX, value_type);
        return true;
    }
    if (strcmp(name, "__atomic_compare_exchange_n") == 0) {
        /* Evaluate the non-address control operands before reserving address
         * values on the expression stack. */
        gen_expr(mod, call_argument(call, 5));
        gen_expr(mod, call_argument(call, 4));
        gen_expr(mod, call_argument(call, 3));
        gen_expr(mod, call_argument(call, 0));
        emit_push_reg(mod, EAX);
        gen_expr(mod, call_argument(call, 1));
        emit_push_reg(mod, EAX);
        gen_expr(mod, call_argument(call, 2));
        emit_normalize_atomic_value(mod, EAX, value_type);
        emit_mov_reg_reg(mod, EDX, EAX);
        emit_pop_reg(mod, ECX); /* Expected-value address. */
        emit_load_typed32(mod, EAX, ECX, 0, value_type);
        emit_push_reg(mod, ECX);
        emit_mov_reg_mem(mod, ECX, ESP, 4); /* Object address. */
        emit_atomic_cmpxchg_width(mod, EDX, ECX, value_type);
        emit_setcc(mod, CC_E, EDX);
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xB6);
        emit_byte(mod, modrm(3, EDX, EDX));
        emit_pop_reg(mod, ECX);
        emit_add_reg_imm(mod, ESP, 4);
        /* On success EAX still contains *expected; on failure CMPXCHG has
         * replaced it with the observed object value. */
        emit_store_typed32(mod, ECX, 0, EAX, value_type);
        emit_mov_reg_reg(mod, EAX, EDX);
        return true;
    }
    is_atomic = strncmp(name, "__atomic_", 9) == 0;
    if (strcmp(name, "__atomic_exchange_n") == 0 ||
        strcmp(name, "__sync_lock_test_and_set") == 0) {
        if (is_atomic) gen_expr(mod, call_argument(call, 2));
        gen_expr(mod, call_argument(call, 0));
        emit_push_reg(mod, EAX);
        gen_expr(mod, call_argument(call, 1));
        emit_normalize_atomic_value(mod, EAX, value_type);
        emit_pop_reg(mod, ECX);
        /* XCHG with memory returns the previous value in EAX. */
        emit_atomic_exchange_width(mod, EAX, ECX, value_type);
        emit_normalize_atomic_value(mod, EAX, value_type);
        return true;
    }
    is_subtract = strcmp(name, "__atomic_fetch_sub") == 0 ||
                  strcmp(name, "__atomic_sub_fetch") == 0 ||
                  strcmp(name, "__sync_fetch_and_sub") == 0 ||
                  strcmp(name, "__sync_sub_and_fetch") == 0;
    returns_new = strcmp(name, "__atomic_add_fetch") == 0 ||
                  strcmp(name, "__atomic_sub_fetch") == 0 ||
                  strcmp(name, "__sync_add_and_fetch") == 0 ||
                  strcmp(name, "__sync_sub_and_fetch") == 0;
    if (strcmp(name, "__atomic_fetch_add") == 0 ||
        strcmp(name, "__atomic_fetch_sub") == 0 ||
        strcmp(name, "__atomic_add_fetch") == 0 ||
        strcmp(name, "__atomic_sub_fetch") == 0 ||
        strcmp(name, "__sync_fetch_and_add") == 0 ||
        strcmp(name, "__sync_fetch_and_sub") == 0 ||
        strcmp(name, "__sync_add_and_fetch") == 0 ||
        strcmp(name, "__sync_sub_and_fetch") == 0) {
        if (is_atomic) gen_expr(mod, call_argument(call, 2));
        gen_expr(mod, call_argument(call, 0));
        emit_push_reg(mod, EAX);
        gen_expr(mod, call_argument(call, 1));
        emit_normalize_atomic_value(mod, EAX, value_type);
        emit_mov_reg_reg(mod, EDX, EAX);
        emit_pop_reg(mod, ECX);
        emit_mov_reg_reg(mod, EAX, EDX);
        if (is_subtract) emit_neg_reg(mod, EAX);
        emit_atomic_xadd_width(mod, EAX, ECX, value_type);
        emit_normalize_atomic_value(mod, EAX, value_type);
        if (returns_new) {
            if (is_subtract) {
                emit_sub_reg_reg(mod, EAX, EDX);
            } else {
                emit_add_reg_reg(mod, EAX, EDX);
            }
            emit_normalize_atomic_value(mod, EAX, value_type);
        }
        return true;
    }
    bitwise_operation = atomic_bitwise_operation(name);
    if (bitwise_operation != ATOMIC_BITWISE_NONE) {
        int retry_label = new_label();
        if (is_atomic) gen_expr(mod, call_argument(call, 2));
        gen_expr(mod, call_argument(call, 0));
        emit_push_reg(mod, EAX); /* Object address. */
        gen_expr(mod, call_argument(call, 1));
        emit_normalize_atomic_value(mod, EAX, value_type);
        emit_push_reg(mod, EAX); /* Operand. */
        emit_mov_reg_mem(mod, ECX, ESP, 4);
        emit_load_typed32(mod, EAX, ECX, 0, value_type);
        emit_label(mod, retry_label);
        emit_mov_reg_reg(mod, EDX, EAX);
        emit_mov_reg_mem(mod, ECX, ESP, 0);
        if (bitwise_operation == ATOMIC_BITWISE_AND ||
            bitwise_operation == ATOMIC_BITWISE_NAND) {
            emit_and_reg_reg(mod, EDX, ECX);
        } else if (bitwise_operation == ATOMIC_BITWISE_OR) {
            emit_or_reg_reg(mod, EDX, ECX);
        } else {
            emit_xor_reg_reg(mod, EDX, ECX);
        }
        if (bitwise_operation == ATOMIC_BITWISE_NAND) {
            emit_not_reg(mod, EDX);
        }
        emit_normalize_atomic_value(mod, EDX, value_type);
        emit_mov_reg_mem(mod, ECX, ESP, 4);
        emit_atomic_cmpxchg_width(mod, EDX, ECX, value_type);
        emit_jcc_label(mod, CC_NE, retry_label);
        if (atomic_bitwise_returns_new(name)) {
            emit_mov_reg_reg(mod, EAX, EDX);
        }
        emit_normalize_atomic_value(mod, EAX, value_type);
        emit_add_reg_imm(mod, ESP, 8);
        return true;
    }
    if (strcmp(name, "__sync_bool_compare_and_swap") == 0 ||
        strcmp(name, "__sync_val_compare_and_swap") == 0) {
        gen_expr(mod, call_argument(call, 0));
        emit_push_reg(mod, EAX);
        gen_expr(mod, call_argument(call, 1));
        emit_normalize_atomic_value(mod, EAX, value_type);
        emit_push_reg(mod, EAX);
        gen_expr(mod, call_argument(call, 2));
        emit_normalize_atomic_value(mod, EAX, value_type);
        emit_mov_reg_reg(mod, EDX, EAX);
        emit_pop_reg(mod, EAX);
        emit_pop_reg(mod, ECX);
        emit_atomic_cmpxchg_width(mod, EDX, ECX, value_type);
        if (strcmp(name, "__sync_bool_compare_and_swap") == 0) {
            emit_setcc(mod, CC_E, EAX);
            emit_byte(mod, 0x0F);
            emit_byte(mod, 0xB6);
            emit_byte(mod, modrm(3, EAX, EAX));
        } else {
            emit_normalize_atomic_value(mod, EAX, value_type);
        }
        return true;
    }
    if (strcmp(name, "__sync_lock_release") == 0) {
        gen_expr(mod, call_argument(call, 0));
        emit_atomic_clear_width(mod, EAX, value_type);
        return true;
    }
    if (strcmp(name, "__atomic_thread_fence") == 0 ||
        strcmp(name, "__sync_synchronize") == 0) {
        if (strcmp(name, "__atomic_thread_fence") == 0) {
            gen_expr(mod, call_argument(call, 0));
        }
        /* A locked operation is a full barrier on every supported i686 CPU. */
        emit_byte(mod, 0xF0);
        emit_byte(mod, 0x83);
        emit_byte(mod, 0x0C);
        emit_byte(mod, 0x24);
        emit_byte(mod, 0x00); /* lock or dword ptr [esp], 0 */
        return true;
    }
    return false;
}
static void gen_stmt(Module* mod, Stmt* stmt);
static void gen_zero_local_storage(Module* mod, int32_t displacement,
                                   size_t storage);
static bool gen_local_initializer(Module* mod, Type* type, Expr* initializer,
                                  int32_t displacement);
static void gen_cxx_call_constructor32(Module* mod,
                                        CxxConstructorInfo* constructor,
                                        ExprList* arguments);

static bool gen_cxx_initializer_calls_body(const Expr* initializer) {
    return initializer && initializer->kind == EXPR_COMPOUND &&
           initializer->compound_constructor &&
           !initializer->compound_constructor->body_is_empty;
}

static void gen_symbol_address(Module* mod, const char* symbol,
                               uint32_t addend) {
    if (g_opts.pic || g_opts.pie) {
        const char* slot = module_get_got_entry(mod, symbol);
        uint32_t offset;
        emit_byte(mod, 0xE8); /* call next instruction */
        emit_dword(mod, 0u);
        emit_pop_reg(mod, EAX);
        emit_byte(mod, 0x05); /* add EAX, disp32 */
        offset = code_offset(mod);
        emit_dword(mod, 0u);
        module_add_got_relocation(mod, MODULE_SYMBOL_CODE, offset, slot);
        emit_mov_reg_mem(mod, EAX, EAX, 0);
        if (addend != 0u) emit_add_reg_imm(mod, EAX, (int32_t)addend);
        return;
    }
    emit_mov_reg_imm(mod, EAX, 0u);
    module_add_relocation(mod, MODULE_SYMBOL_CODE,
                          code_offset(mod) - 4u, addend,
                          false, false, symbol);
    add_reloc(mod, MODULE_SYMBOL_CODE, code_offset(mod) - 4u,
              RIN_RELOC_ABS32);
}

static void gen_local_vtable_init(Module* mod, Type* type,
                                  int32_t displacement) {
    CxxClass* cls;
    if (!mod || !type || !codegen_type_has_vtable_storage(type)) return;
    if (type->cxx_vtable_size > 0 && type->cxx_vtable_symbol) {
        gen_symbol_address(mod, type->cxx_vtable_symbol, 0u);
        emit_mov_mem_reg(mod, EBP, displacement, EAX);
    }

    cls = type->cxx_class;
    if (!cls || !cls->base_offsets) return;
    for (int index = 0; index < cls->base_count; ++index) {
        CxxClass* base = cls->bases[index].base;
        const char* base_vtable_symbol;
        int64_t base_displacement;
        if (!base || base->vtable_size <= 0 || !base->type ||
            !base->type->cxx_vtable_symbol ||
            (!cls->bases[index].is_virtual &&
             cls->base_offsets[index] <= 0)) {
            continue;
        }
        base_vtable_symbol = base->type->cxx_vtable_symbol;
        for (int secondary_index = 0;
             secondary_index < cls->secondary_vtable_count;
             ++secondary_index) {
            CxxSecondaryVtable* secondary =
                &cls->secondary_vtables[secondary_index];
            if (secondary->base_index == index && secondary->symbol) {
                base_vtable_symbol = secondary->symbol;
                break;
            }
        }
        base_displacement = (int64_t)displacement +
                            cls->base_offsets[index];
        if (base_displacement < INT32_MIN ||
            base_displacement > INT32_MAX) {
            rcc_error((SourceLoc){"<cxx-vtable>", 0, 0},
                      "secondary vtable pointer exceeds stack limits");
            continue;
        }
        gen_symbol_address(mod, base_vtable_symbol, 0u);
        emit_mov_mem_reg(mod, EBP, (int32_t)base_displacement, EAX);
    }
}

static const Type* codegen_bitfield_storage_type(const TypeField* field) {
    if (!field || !field->type) return type_uint;
    switch (field->type->size) {
        case 1: return type_uchar;
        case 2: return type_ushort;
        default: return type_uint;
    }
}

static uint32_t codegen_bitfield_mask(const TypeField* field) {
    if (!field || field->bit_width >= 32u) return UINT32_MAX;
    return (UINT32_C(1) << field->bit_width) - 1u;
}

/* EAX contains the unsigned storage unit loaded from a bit-field address. */
static void emit_bitfield_extract32(Module* mod, const TypeField* field) {
    uint32_t mask = codegen_bitfield_mask(field);
    if (field->bit_offset != 0u) {
        emit_shr_reg_imm(mod, EAX, (uint8_t)field->bit_offset);
    }
    if (field->bit_width < 32u) emit_and_reg_imm(mod, EAX, mask);
    if (field->type && !field->type->is_unsigned && field->bit_width < 32u) {
        uint8_t extension = (uint8_t)(32u - field->bit_width);
        emit_shl_reg_imm(mod, EAX, extension);
        emit_sar_reg_imm(mod, EAX, extension);
    }
}

/* EAX contains the address, and leaves the converted field value in EAX. */
static void gen_bitfield_load32(Module* mod, const TypeField* field) {
    const Type* storage_type = codegen_bitfield_storage_type(field);
    emit_mov_reg_reg(mod, ECX, EAX);
    emit_load_typed32(mod, EAX, ECX, 0, storage_type);
    emit_bitfield_extract32(mod, field);
}

/* EDX contains the storage address and ECX contains the source value.  The
 * read-modify-write preserves neighboring fields in the same allocation
 * unit, while the final extraction gives assignment its C value. */
static void emit_bitfield_store32(Module* mod, const TypeField* field) {
    const Type* storage_type = codegen_bitfield_storage_type(field);
    uint32_t field_mask = codegen_bitfield_mask(field);
    uint32_t shifted_mask = field_mask << field->bit_offset;

    emit_normalize_atomic_value(mod, ECX, field->type);
    emit_mov_reg_imm(mod, EAX, field_mask);
    emit_and_reg_reg(mod, ECX, EAX);
    if (field->bit_offset != 0u) {
        emit_shl_reg_imm(mod, ECX, (uint8_t)field->bit_offset);
    }
    emit_push_reg(mod, ECX);
    emit_load_typed32(mod, EAX, EDX, 0, storage_type);
    emit_mov_reg_imm(mod, ECX, ~shifted_mask);
    emit_and_reg_reg(mod, EAX, ECX);
    emit_pop_reg(mod, ECX);
    emit_or_reg_reg(mod, EAX, ECX);
    emit_store_typed32(mod, EDX, 0, EAX, storage_type);
    if (field->bit_offset != 0u) {
        emit_shr_reg_imm(mod, ECX, (uint8_t)field->bit_offset);
    }
    emit_mov_reg_reg(mod, EAX, ECX);
    if (field->type && !field->type->is_unsigned && field->bit_width < 32u) {
        uint8_t extension = (uint8_t)(32u - field->bit_width);
        emit_shl_reg_imm(mod, EAX, extension);
        emit_sar_reg_imm(mod, EAX, extension);
    }
}

static bool gen_bitfield_initializer32(Module* mod, const TypeField* field,
                                       Expr* initializer,
                                       int32_t displacement) {
    if (!field || !field->is_bitfield || !initializer) return false;
    gen_expr(mod, initializer);
    emit_mov_reg_reg(mod, ECX, EAX);
    emit_mov_reg_reg(mod, EDX, EBP);
    emit_add_reg_imm(mod, EDX, displacement);
    emit_bitfield_store32(mod, field);
    return true;
}

static void gen_tls_address(Module* mod, const char* symbol) {
    /* Variant II x86 TLS: GS:0 contains the thread pointer. */
    emit_byte(mod, 0x65);
    emit_byte(mod, 0xA1);
    emit_dword(mod, 0u);
    emit_byte(mod, 0x05);  /* ADD EAX, imm32 */
    {
        uint32_t offset = code_offset(mod);
        emit_dword(mod, 0u);
        module_add_tls_relocation(mod, MODULE_SYMBOL_CODE, offset, symbol);
        add_reloc(mod, MODULE_SYMBOL_CODE, offset, RIN_RELOC_TLSOFF32S);
    }
}

static bool gen_inline_method_address(Module* mod, Expr* expr);
static bool gen_inline_method_integer64(Module* mod, Expr* expr);

/* Generate lvalue address in EAX */
static void gen_lvalue(Module* mod, Expr* expr) {
    switch (expr->kind) {
        case EXPR_CXX_THIS:
            if (expr->cxx_this_stack_offset < 0) {
                rcc_fatal("constructor this argument has no saved object");
            }
            emit_mov_reg_mem(mod, EAX, ESP, expr->cxx_this_stack_offset);
            break;
        case EXPR_IDENT: {
            /* Use decl set during semantic analysis */
            Decl* decl = expr->ident_decl;
            if (!decl) {
                rcc_error(expr->loc,
                          "identifier has no semantic declaration in i686 code generation");
                return;
            }
            if (decl->kind == DECL_VAR && decl->var_is_thread_local) {
                gen_tls_address(mod, decl_link_name(decl));
                if (decl->type && decl->type->is_reference) {
                    emit_mov_reg_mem(mod, EAX, EAX, 0);
                }
            } else if (decl->kind == DECL_FUNC) {
                gen_symbol_address(mod, decl_link_name(decl), 0u);
            } else if (decl->var_is_vla) {
                emit_mov_reg_mem(mod, EAX, EBP, decl->var_offset);
            } else if (decl->type && decl->type->is_reference) {
                if (decl->var_is_global) {
                    gen_symbol_address(mod, decl_link_name(decl), 0u);
                    emit_mov_reg_mem(mod, EAX, EAX, 0);
                } else {
                    emit_mov_reg_mem(mod, EAX, EBP, decl->var_offset);
                }
            } else if (decl->var_is_global) {
                gen_symbol_address(mod, decl_link_name(decl), 0u);
            } else {
                /* Local: EBP + offset */
                emit_byte(mod, 0x8D);  /* LEA EAX, [EBP+disp] */
                emit_byte(mod, modrm(2, EAX, EBP));
                emit_dword(mod, (uint32_t)decl->var_offset);
            }
            break;
        }

        case EXPR_DEREF:
            gen_expr(mod, expr->unary_operand);
            break;

        case EXPR_INDEX:
            gen_expr(mod, expr->index_base);
            emit_push_reg(mod, EAX);
            gen_expr(mod, expr->index_expr);
            /* Multiply by element size */
            if (codegen_type_has_vla(expr->type)) {
                emit_push_reg(mod, EAX);
                gen_vla_extent_for_expr(mod, expr->type,
                                        expr->index_base);
                emit_mov_reg_reg(mod, ECX, EAX);
                emit_pop_reg(mod, EAX);
                emit_imul_reg_reg(mod, EAX, ECX);
            } else if (expr->type && expr->type->size > 1) {
                emit_byte(mod, 0x6B);  /* IMUL EAX, EAX, imm8 */
                emit_byte(mod, modrm(3, EAX, EAX));
                emit_byte(mod, (uint8_t)expr->type->size);
            }
            emit_pop_reg(mod, ECX);
            emit_add_reg_reg(mod, EAX, ECX);
            break;

        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            if (expr->kind == EXPR_PTR_MEMBER) {
                gen_expr(mod, expr->member_base);
            } else {
                gen_lvalue(mod, expr->member_base);
            }
            if (expr->member_field && expr->member_field->offset > 0) {
                emit_add_reg_imm(mod, EAX, expr->member_field->offset);
            }
            break;

        case EXPR_COMPOUND:
            if (!expr->compound_type || expr->compound_offset >= 0) {
                rcc_error(expr->loc,
                          "compound literal has no automatic storage slot");
                return;
            }
            if ((expr->compound_type->kind == TYPE_ARRAY ||
                 expr->compound_type->kind == TYPE_STRUCT ||
                 expr->compound_type->kind == TYPE_UNION) &&
                !gen_cxx_initializer_calls_body(expr)) {
                gen_zero_local_storage(mod, expr->compound_offset,
                                       (size_t)expr->compound_type->size);
            }
            if (!gen_local_initializer(mod, expr->compound_type, expr,
                                       expr->compound_offset)) {
                rcc_error(expr->loc,
                          "unsupported compound literal initializer");
            }
            gen_local_vtable_init(mod, expr->compound_type,
                                  expr->compound_offset);
            emit_byte(mod, 0x8D);  /* LEA EAX, [EBP+disp32] */
            emit_byte(mod, modrm(2, EAX, EBP));
            emit_dword(mod, (uint32_t)expr->compound_offset);
            break;

        case EXPR_CALL:
            if (expr->call_method && expr->call_method->return_type &&
                expr->call_method->return_type->is_reference &&
                gen_inline_method_address(mod, expr)) {
                break;
            }
            if (!expr->type ||
                (expr->type->kind != TYPE_STRUCT &&
                 expr->type->kind != TYPE_UNION) ||
                expr->call_result_offset >= 0) {
                rcc_error(expr->loc,
                          "aggregate call has no automatic result slot");
                return;
            }
            gen_expr(mod, expr);
            emit_byte(mod, 0x8D);  /* LEA EAX, [EBP+disp32] */
            emit_byte(mod, modrm(2, EAX, EBP));
            emit_dword(mod, (uint32_t)expr->call_result_offset);
            break;

        case EXPR_VA_ARG:
            if (!expr->va_arg_type ||
                (expr->va_arg_type->kind != TYPE_STRUCT &&
                 expr->va_arg_type->kind != TYPE_UNION)) {
                rcc_error(expr->loc, "va_arg aggregate address requested for scalar");
                return;
            }
            if (expr->va_arg_result_offset >= 0) {
                rcc_error(expr->loc,
                          "aggregate va_arg has no automatic result slot");
                return;
            }
            gen_expr(mod, expr);
            break;

        case EXPR_ASSIGN:
            if (expr->type &&
                (expr->type->kind == TYPE_STRUCT ||
                 expr->type->kind == TYPE_UNION)) {
                gen_expr(mod, expr);
            } else {
                rcc_error(expr->loc, "assignment expression is not an lvalue");
            }
            break;

        default:
            rcc_error(expr->loc, "not an lvalue");
            break;
    }
}

static void gen_divmod_integer64(Module* mod, Expr* lhs, Expr* rhs,
                                 const Type* result_type,
                                 bool want_remainder) {
    enum {
        DIVISOR_LOW = 0,
        DIVISOR_HIGH = 4,
        DIVIDEND_LOW = 8,
        DIVIDEND_HIGH = 12,
        QUOTIENT_LOW = 16,
        QUOTIENT_HIGH = 20,
        REMAINDER_LOW = 24,
        REMAINDER_HIGH = 28,
        QUOTIENT_NEGATIVE = 32,
        REMAINDER_NEGATIVE = 36,
        DIVISION_STORAGE = 40
    };
    bool is_signed = result_type && !result_type->is_unsigned;
    int lhs_positive_label = new_label();
    int rhs_positive_label = new_label();
    int loop_label = new_label();
    int subtract_label = new_label();
    int keep_label = new_label();
    int sign_done_label = new_label();

    emit_push_reg(mod, EBX);
    emit_push_reg(mod, ESI);
    emit_push_reg(mod, EDI);
    emit_sub_reg_imm(mod, ESP, DIVISION_STORAGE);

    if (lhs) gen_expr_as_integer64(mod, lhs);
    emit_mov_reg_imm(mod, EBX, 0u);
    if (is_signed) {
        emit_test_reg_reg(mod, EDX, EDX);
        emit_jcc_label(mod, CC_NS, lhs_positive_label);
        emit_mov_reg_imm(mod, EBX, 1u);
        emit_neg_reg(mod, EAX);
        emit_adc_reg_imm8(mod, EDX, 0u);
        emit_neg_reg(mod, EDX);
        emit_label(mod, lhs_positive_label);
    }
    emit_mov_mem_reg(mod, ESP, DIVIDEND_LOW, EAX);
    emit_mov_mem_reg(mod, ESP, DIVIDEND_HIGH, EDX);
    emit_mov_mem_reg(mod, ESP, QUOTIENT_NEGATIVE, EBX);
    emit_mov_mem_reg(mod, ESP, REMAINDER_NEGATIVE, EBX);

    gen_expr_as_integer64(mod, rhs);
    if (is_signed) {
        emit_test_reg_reg(mod, EDX, EDX);
        emit_jcc_label(mod, CC_NS, rhs_positive_label);
        emit_neg_reg(mod, EAX);
        emit_adc_reg_imm8(mod, EDX, 0u);
        emit_neg_reg(mod, EDX);
        emit_mov_reg_mem(mod, EBX, ESP, QUOTIENT_NEGATIVE);
        emit_xor_reg_imm8(mod, EBX, 1u);
        emit_mov_mem_reg(mod, ESP, QUOTIENT_NEGATIVE, EBX);
        emit_label(mod, rhs_positive_label);
    }
    emit_mov_mem_reg(mod, ESP, DIVISOR_LOW, EAX);
    emit_mov_mem_reg(mod, ESP, DIVISOR_HIGH, EDX);
    emit_mov_reg_imm(mod, EAX, 0u);
    emit_mov_mem_reg(mod, ESP, QUOTIENT_LOW, EAX);
    emit_mov_mem_reg(mod, ESP, QUOTIENT_HIGH, EAX);
    emit_mov_mem_reg(mod, ESP, REMAINDER_LOW, EAX);
    emit_mov_mem_reg(mod, ESP, REMAINDER_HIGH, EAX);
    emit_mov_reg_imm(mod, ECX, 64u);

    emit_label(mod, loop_label);
    emit_mov_reg_mem(mod, EAX, ESP, DIVIDEND_LOW);
    emit_mov_reg_mem(mod, EDX, ESP, DIVIDEND_HIGH);
    emit_add_reg_reg(mod, EAX, EAX);
    emit_adc_reg_reg(mod, EDX, EDX);
    emit_setcc(mod, CC_B, EBX);
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0xB6);
    emit_byte(mod, modrm(3, EBX, EBX));
    emit_mov_mem_reg(mod, ESP, DIVIDEND_LOW, EAX);
    emit_mov_mem_reg(mod, ESP, DIVIDEND_HIGH, EDX);

    emit_mov_reg_mem(mod, EAX, ESP, REMAINDER_LOW);
    emit_mov_reg_mem(mod, EDX, ESP, REMAINDER_HIGH);
    emit_add_reg_reg(mod, EAX, EAX);
    emit_adc_reg_reg(mod, EDX, EDX);
    emit_or_reg_reg(mod, EAX, EBX);
    emit_mov_reg_imm(mod, EBX, 0u);
    emit_mov_reg_mem(mod, ESI, ESP, DIVISOR_HIGH);
    emit_cmp_reg_reg(mod, EDX, ESI);
    emit_jcc_label(mod, CC_A, subtract_label);
    emit_jcc_label(mod, CC_B, keep_label);
    emit_mov_reg_mem(mod, EDI, ESP, DIVISOR_LOW);
    emit_cmp_reg_reg(mod, EAX, EDI);
    emit_jcc_label(mod, CC_B, keep_label);

    emit_label(mod, subtract_label);
    emit_mov_reg_mem(mod, EDI, ESP, DIVISOR_LOW);
    emit_sub_reg_reg(mod, EAX, EDI);
    emit_sbb_reg_reg(mod, EDX, ESI);
    emit_mov_reg_imm(mod, EBX, 1u);
    emit_label(mod, keep_label);
    emit_mov_mem_reg(mod, ESP, REMAINDER_LOW, EAX);
    emit_mov_mem_reg(mod, ESP, REMAINDER_HIGH, EDX);
    emit_mov_reg_mem(mod, EAX, ESP, QUOTIENT_LOW);
    emit_mov_reg_mem(mod, EDX, ESP, QUOTIENT_HIGH);
    emit_add_reg_reg(mod, EAX, EAX);
    emit_adc_reg_reg(mod, EDX, EDX);
    emit_or_reg_reg(mod, EAX, EBX);
    emit_mov_mem_reg(mod, ESP, QUOTIENT_LOW, EAX);
    emit_mov_mem_reg(mod, ESP, QUOTIENT_HIGH, EDX);
    emit_dec_reg(mod, ECX);
    emit_jcc_label(mod, CC_NE, loop_label);

    if (want_remainder) {
        emit_mov_reg_mem(mod, EAX, ESP, REMAINDER_LOW);
        emit_mov_reg_mem(mod, EDX, ESP, REMAINDER_HIGH);
        emit_mov_reg_mem(mod, ECX, ESP, REMAINDER_NEGATIVE);
    } else {
        emit_mov_reg_mem(mod, EAX, ESP, QUOTIENT_LOW);
        emit_mov_reg_mem(mod, EDX, ESP, QUOTIENT_HIGH);
        emit_mov_reg_mem(mod, ECX, ESP, QUOTIENT_NEGATIVE);
    }
    if (is_signed) {
        emit_test_reg_reg(mod, ECX, ECX);
        emit_jcc_label(mod, CC_E, sign_done_label);
        emit_neg_reg(mod, EAX);
        emit_adc_reg_imm8(mod, EDX, 0u);
        emit_neg_reg(mod, EDX);
        emit_label(mod, sign_done_label);
    }
    emit_add_reg_imm(mod, ESP, DIVISION_STORAGE);
    emit_pop_reg(mod, EDI);
    emit_pop_reg(mod, ESI);
    emit_pop_reg(mod, EBX);
}

static Type* codegen_comparison_type(Expr* expr) {
    Type* left = expr && expr->binary_lhs ? expr->binary_lhs->type : NULL;
    Type* right = expr && expr->binary_rhs ? expr->binary_rhs->type : NULL;
    if ((left && (left->kind == TYPE_PTR || left->kind == TYPE_ARRAY)) ||
        (right && (right->kind == TYPE_PTR || right->kind == TYPE_ARRAY))) {
        return type_ulong;
    }
    if (!left || !right) return type_int;
    if (left->kind == TYPE_ENUM || left->kind < TYPE_INT) left = type_int;
    if (right->kind == TYPE_ENUM || right->kind < TYPE_INT) right = type_int;
    return type_common(left, right);
}

static void emit_test_scalar_value(Module* mod, const Type* type) {
    if (gen_is_floating(type)) {
        emit_x87_floating_truth(mod, type);
        return;
    }
    if (gen_is_integer64(type)) emit_or_reg_reg(mod, EAX, EDX);
    emit_test_reg_reg(mod, EAX, EAX);
}

static void gen_multiply_integer64(Module* mod, Expr* lhs, Expr* rhs) {
    /* Low 64 bits of (ahi:alo) * (bhi:blo): alo*blo plus
     * the low words of both cross products.  A NULL lhs means that its
     * EDX:EAX value was already loaded by a compound assignment. */
    emit_push_reg(mod, EBX);
    emit_push_reg(mod, ESI);
    if (lhs) gen_expr_as_integer64(mod, lhs);
    emit_push_reg(mod, EDX);
    emit_push_reg(mod, EAX);
    gen_expr_as_integer64(mod, rhs);
    emit_mov_reg_reg(mod, EBX, EAX);
    emit_mov_reg_reg(mod, ESI, EDX);
    emit_mov_reg_mem(mod, EAX, ESP, 0);
    emit_mul_reg(mod, EBX);
    emit_push_reg(mod, EAX);
    emit_mov_reg_reg(mod, ECX, EDX);
    emit_mov_reg_mem(mod, EAX, ESP, 8);
    emit_imul_reg_reg(mod, EAX, EBX);
    emit_add_reg_reg(mod, ECX, EAX);
    emit_mov_reg_mem(mod, EAX, ESP, 4);
    emit_imul_reg_reg(mod, EAX, ESI);
    emit_add_reg_reg(mod, ECX, EAX);
    emit_mov_reg_reg(mod, EDX, ECX);
    emit_pop_reg(mod, EAX);
    emit_add_reg_imm(mod, ESP, 8);
    emit_pop_reg(mod, ESI);
    emit_pop_reg(mod, EBX);
}

static void gen_shift_integer64(Module* mod, Expr* lhs, Expr* rhs,
                                const Type* result_type, bool shift_left) {
    int wide_count_label = new_label();
    int end_label = new_label();
    bool arithmetic = !shift_left && result_type &&
                      !result_type->is_unsigned;

    if (lhs) gen_expr_as_integer64(mod, lhs);
    emit_push_reg(mod, EDX);
    emit_push_reg(mod, EAX);
    gen_expr(mod, rhs);
    emit_mov_reg_reg(mod, ECX, EAX);
    emit_pop_reg(mod, EAX);
    emit_pop_reg(mod, EDX);
    emit_test_reg_imm(mod, ECX, 32u);
    emit_jcc_label(mod, CC_NE, wide_count_label);
    if (shift_left) {
        emit_shld_reg_cl(mod, EDX, EAX);
        emit_shl_reg_cl(mod, EAX);
    } else {
        emit_shrd_reg_cl(mod, EAX, EDX);
        if (arithmetic) emit_sar_reg_cl(mod, EDX);
        else emit_shr_reg_cl(mod, EDX);
    }
    emit_jmp_label(mod, end_label);
    emit_label(mod, wide_count_label);
    if (shift_left) {
        emit_shl_reg_cl(mod, EAX);
        emit_mov_reg_reg(mod, EDX, EAX);
        emit_mov_reg_imm(mod, EAX, 0u);
    } else {
        emit_mov_reg_reg(mod, EAX, EDX);
        if (arithmetic) {
            emit_sar_reg_cl(mod, EAX);
            emit_mov_reg_imm(mod, ECX, 31u);
            emit_sar_reg_cl(mod, EDX);
        } else {
            emit_shr_reg_cl(mod, EAX);
            emit_mov_reg_imm(mod, EDX, 0u);
        }
    }
    emit_label(mod, end_label);
}

/* i386 SysV returns 64-bit integer scalars in EDX:EAX.  Keep this separate
 * from the ordinary EAX expression path so an unsupported operation cannot
 * silently truncate its high word. */
static void gen_expr64_pair(Module* mod, Expr* expr) {
    if (!expr) {
        rcc_error((SourceLoc){"<expr>", 0, 0},
                  "missing expression in i686 64-bit code generation");
        return;
    }

    switch (expr->kind) {
        case EXPR_INT_LIT: {
            uint64_t value = (uint64_t)expr->int_val;
            emit_mov_reg_imm(mod, EAX, (uint32_t)value);
            emit_mov_reg_imm(mod, EDX, (uint32_t)(value >> 32));
            break;
        }

        case EXPR_IDENT:
        case EXPR_DEREF:
        case EXPR_INDEX:
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            gen_lvalue(mod, expr);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_mov_reg_mem(mod, EAX, ECX, 0);
            emit_mov_reg_mem(mod, EDX, ECX, 4);
            break;

        case EXPR_NEG:
            gen_expr64_pair(mod, expr->unary_operand);
            emit_neg_reg(mod, EAX);
            emit_adc_reg_imm8(mod, EDX, 0u);
            emit_neg_reg(mod, EDX);
            break;

        case EXPR_BITNOT:
            gen_expr64_pair(mod, expr->unary_operand);
            emit_not_reg(mod, EAX);
            emit_not_reg(mod, EDX);
            break;

        case EXPR_PREINC:
        case EXPR_PREDEC:
        case EXPR_POSTINC:
        case EXPR_POSTDEC: {
            bool increment = expr->kind == EXPR_PREINC ||
                             expr->kind == EXPR_POSTINC;
            bool post = expr->kind == EXPR_POSTINC ||
                        expr->kind == EXPR_POSTDEC;
            gen_lvalue(mod, expr->unary_operand);
            emit_push_reg(mod, EAX);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_mov_reg_mem(mod, EAX, ECX, 0);
            emit_mov_reg_mem(mod, EDX, ECX, 4);
            if (post) {
                emit_push_reg(mod, EDX);
                emit_push_reg(mod, EAX);
            }
            if (increment) {
                emit_add_reg_imm(mod, EAX, 1);
                emit_adc_reg_imm8(mod, EDX, 0u);
            } else {
                emit_sub_reg_imm(mod, EAX, 1);
                emit_sbb_reg_imm8(mod, EDX, 0u);
            }
            if (post) {
                emit_mov_reg_mem(mod, ECX, ESP, 8);
            } else {
                emit_mov_reg_mem(mod, ECX, ESP, 0);
            }
            emit_mov_mem_reg(mod, ECX, 0, EAX);
            emit_mov_mem_reg(mod, ECX, 4, EDX);
            if (post) {
                emit_pop_reg(mod, EAX);
                emit_pop_reg(mod, EDX);
            }
            emit_add_reg_imm(mod, ESP, 4);
            break;
        }

        case EXPR_ADD:
        case EXPR_SUB:
        case EXPR_BITAND:
        case EXPR_BITOR:
        case EXPR_BITXOR:
            gen_expr_as_integer64(mod, expr->binary_lhs);
            emit_push_reg(mod, EDX);
            emit_push_reg(mod, EAX);
            gen_expr_as_integer64(mod, expr->binary_rhs);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_mov_reg_mem(mod, EAX, ESP, 0);
            if (expr->kind == EXPR_ADD) {
                emit_add_reg_reg(mod, EAX, ECX);
            } else if (expr->kind == EXPR_SUB) {
                emit_sub_reg_reg(mod, EAX, ECX);
            } else if (expr->kind == EXPR_BITAND) {
                emit_and_reg_reg(mod, EAX, ECX);
            } else if (expr->kind == EXPR_BITOR) {
                emit_or_reg_reg(mod, EAX, ECX);
            } else {
                emit_xor_reg_reg(mod, EAX, ECX);
            }
            emit_mov_reg_reg(mod, ECX, EDX);
            emit_mov_reg_mem(mod, EDX, ESP, 4);
            if (expr->kind == EXPR_ADD) {
                emit_adc_reg_reg(mod, EDX, ECX);
            } else if (expr->kind == EXPR_SUB) {
                emit_sbb_reg_reg(mod, EDX, ECX);
            } else if (expr->kind == EXPR_BITAND) {
                emit_and_reg_reg(mod, EDX, ECX);
            } else if (expr->kind == EXPR_BITOR) {
                emit_or_reg_reg(mod, EDX, ECX);
            } else {
                emit_xor_reg_reg(mod, EDX, ECX);
            }
            emit_add_reg_imm(mod, ESP, 8);
            break;

        case EXPR_LSHIFT:
        case EXPR_RSHIFT:
            gen_shift_integer64(mod, expr->binary_lhs, expr->binary_rhs,
                                expr->type, expr->kind == EXPR_LSHIFT);
            break;

        case EXPR_MUL:
            gen_multiply_integer64(mod, expr->binary_lhs,
                                   expr->binary_rhs);
            break;

        case EXPR_DIV:
        case EXPR_MOD:
            gen_divmod_integer64(mod, expr->binary_lhs,
                                 expr->binary_rhs, expr->type,
                                 expr->kind == EXPR_MOD);
            break;

        case EXPR_ASSIGN:
            gen_expr_as_integer64(mod, expr->binary_rhs);
            emit_push_reg(mod, EDX);
            emit_push_reg(mod, EAX);
            gen_lvalue(mod, expr->binary_lhs);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_pop_reg(mod, EAX);
            emit_pop_reg(mod, EDX);
            emit_mov_mem_reg(mod, ECX, 0, EAX);
            emit_mov_mem_reg(mod, ECX, 4, EDX);
            break;

        case EXPR_ADD_ASSIGN:
        case EXPR_SUB_ASSIGN:
        case EXPR_AND_ASSIGN:
        case EXPR_OR_ASSIGN:
        case EXPR_XOR_ASSIGN:
            gen_lvalue(mod, expr->binary_lhs);
            emit_push_reg(mod, EAX);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_mov_reg_mem(mod, EAX, ECX, 0);
            emit_mov_reg_mem(mod, EDX, ECX, 4);
            emit_push_reg(mod, EDX);
            emit_push_reg(mod, EAX);
            gen_expr_as_integer64(mod, expr->binary_rhs);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_mov_reg_mem(mod, EAX, ESP, 0);
            if (expr->kind == EXPR_ADD_ASSIGN) {
                emit_add_reg_reg(mod, EAX, ECX);
            } else if (expr->kind == EXPR_SUB_ASSIGN) {
                emit_sub_reg_reg(mod, EAX, ECX);
            } else if (expr->kind == EXPR_AND_ASSIGN) {
                emit_and_reg_reg(mod, EAX, ECX);
            } else if (expr->kind == EXPR_OR_ASSIGN) {
                emit_or_reg_reg(mod, EAX, ECX);
            } else {
                emit_xor_reg_reg(mod, EAX, ECX);
            }
            emit_mov_reg_reg(mod, ECX, EDX);
            emit_mov_reg_mem(mod, EDX, ESP, 4);
            if (expr->kind == EXPR_ADD_ASSIGN) {
                emit_adc_reg_reg(mod, EDX, ECX);
            } else if (expr->kind == EXPR_SUB_ASSIGN) {
                emit_sbb_reg_reg(mod, EDX, ECX);
            } else if (expr->kind == EXPR_AND_ASSIGN) {
                emit_and_reg_reg(mod, EDX, ECX);
            } else if (expr->kind == EXPR_OR_ASSIGN) {
                emit_or_reg_reg(mod, EDX, ECX);
            } else {
                emit_xor_reg_reg(mod, EDX, ECX);
            }
            emit_add_reg_imm(mod, ESP, 8);
            emit_pop_reg(mod, ECX);
            emit_mov_mem_reg(mod, ECX, 0, EAX);
            emit_mov_mem_reg(mod, ECX, 4, EDX);
            break;

        case EXPR_MUL_ASSIGN:
        case EXPR_DIV_ASSIGN:
        case EXPR_MOD_ASSIGN:
        case EXPR_LSHIFT_ASSIGN:
        case EXPR_RSHIFT_ASSIGN:
            gen_lvalue(mod, expr->binary_lhs);
            emit_push_reg(mod, EAX);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_mov_reg_mem(mod, EAX, ECX, 0);
            emit_mov_reg_mem(mod, EDX, ECX, 4);
            if (expr->kind == EXPR_MUL_ASSIGN) {
                gen_multiply_integer64(mod, NULL, expr->binary_rhs);
            } else if (expr->kind == EXPR_DIV_ASSIGN ||
                       expr->kind == EXPR_MOD_ASSIGN) {
                gen_divmod_integer64(mod, NULL, expr->binary_rhs,
                                     expr->type,
                                     expr->kind == EXPR_MOD_ASSIGN);
            } else {
                gen_shift_integer64(mod, NULL, expr->binary_rhs, expr->type,
                                    expr->kind == EXPR_LSHIFT_ASSIGN);
            }
            emit_pop_reg(mod, ECX);
            emit_mov_mem_reg(mod, ECX, 0, EAX);
            emit_mov_mem_reg(mod, ECX, 4, EDX);
            break;

        case EXPR_CAST:
            if (gen_is_integer64(expr->cast_expr->type)) {
                gen_expr64_pair(mod, expr->cast_expr);
            } else if (gen_is_floating(expr->cast_expr->type)) {
                gen_expr_as_type(mod, expr->cast_expr, expr->type);
            } else {
                gen_expr(mod, expr->cast_expr);
                emit_extend_eax_to_integer64(mod,
                                             expr->cast_expr->type);
            }
            break;

        case EXPR_COND: {
            int else_label = new_label();
            int end_label = new_label();
            gen_expr(mod, expr->cond_test);
            if (gen_is_floating(expr->cond_test->type)) {
                emit_x87_floating_truth(mod, expr->cond_test->type);
            } else if (gen_is_integer64(expr->cond_test->type)) {
                emit_or_reg_reg(mod, EAX, EDX);
            }
            emit_test_reg_reg(mod, EAX, EAX);
            emit_jcc_label(mod, CC_E, else_label);
            gen_expr_as_integer64(mod, expr->cond_then);
            emit_jmp_label(mod, end_label);
            emit_label(mod, else_label);
            gen_expr_as_integer64(mod, expr->cond_else);
            emit_label(mod, end_label);
            break;
        }

        case EXPR_COMMA:
            gen_expr(mod, expr->binary_lhs);
            gen_expr64_pair(mod, expr->binary_rhs);
            break;

        case EXPR_CALL:
            if (!gen_inline_method_integer64(mod, expr)) {
                gen_call(mod, expr);
            }
            break;

        case EXPR_VA_ARG: {
            if (expr->va_arg_type &&
                (expr->va_arg_type->kind == TYPE_STRUCT ||
                 expr->va_arg_type->kind == TYPE_UNION)) {
                int size = expr->va_arg_type->size;
                int step = (size + 3) & ~3;
                gen_lvalue(mod, expr->va_list_operand);
                emit_mov_reg_reg(mod, ECX, EAX);
                emit_mov_reg_mem(mod, EAX, ECX, 0);
                emit_mov_reg_reg(mod, EDX, EAX);
                emit_add_reg_imm(mod, EAX, step);
                emit_mov_mem_reg(mod, ECX, 0, EAX);
                emit_byte(mod, 0x8D);  /* LEA EAX, [EBP+disp32] */
                emit_byte(mod, modrm(2, EAX, EBP));
                emit_dword(mod, (uint32_t)expr->va_arg_result_offset);
                emit_mov_reg_reg(mod, ECX, EAX);
                for (int offset = 0; offset < size; ++offset) {
                    emit_load_typed32(mod, EAX, EDX, offset, type_uchar);
                    emit_store_typed32(mod, ECX, offset, EAX, type_uchar);
                }
                emit_mov_reg_reg(mod, EAX, ECX);
                break;
            }
            int step = (expr->va_arg_type->size + 3) & ~3;
            gen_lvalue(mod, expr->va_list_operand);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_mov_reg_mem(mod, EAX, ECX, 0);
            emit_mov_reg_reg(mod, EDX, EAX);
            emit_add_reg_imm(mod, EAX, step);
            emit_mov_mem_reg(mod, ECX, 0, EAX);
            emit_mov_reg_mem(mod, EAX, EDX, 0);
            emit_mov_reg_mem(mod, EDX, EDX, 4);
            break;
        }

        default:
            rcc_error(expr->loc,
                      "unsupported i686 64-bit integer operation");
            return;
    }
}

static void gen_expr_as_integer64(Module* mod, Expr* expr) {
    if (expr && gen_is_integer64(expr->type)) {
        gen_expr64_pair(mod, expr);
    } else {
        gen_expr(mod, expr);
        emit_extend_eax_to_integer64(mod, expr ? expr->type : NULL);
    }
}

static void gen_compare_integer64(Module* mod, Expr* expr) {
    int high_diff_label = new_label();
    int end_label = new_label();
    bool equality = expr->kind == EXPR_EQ || expr->kind == EXPR_NE;
    Type* comparison_type = codegen_comparison_type(expr);
    bool unsigned_compare = comparison_type && comparison_type->is_unsigned;
    int low_cc;
    int high_cc;

    gen_expr_as_integer64(mod, expr->binary_lhs);
    emit_push_reg(mod, EDX);
    emit_push_reg(mod, EAX);
    gen_expr_as_integer64(mod, expr->binary_rhs);
    emit_mov_reg_mem(mod, ECX, ESP, 4);
    emit_cmp_reg_reg(mod, ECX, EDX);
    emit_jcc_label(mod, CC_NE, high_diff_label);
    emit_mov_reg_mem(mod, ECX, ESP, 0);
    emit_cmp_reg_reg(mod, ECX, EAX);
    switch (expr->kind) {
        case EXPR_EQ: low_cc = CC_E; break;
        case EXPR_NE: low_cc = CC_NE; break;
        case EXPR_LT: low_cc = CC_B; break;
        case EXPR_GT: low_cc = CC_A; break;
        case EXPR_LE: low_cc = CC_BE; break;
        case EXPR_GE: low_cc = CC_AE; break;
        default: low_cc = CC_E; break;
    }
    emit_setcc(mod, low_cc, EAX);
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0xB6);
    emit_byte(mod, modrm(3, EAX, EAX));
    emit_jmp_label(mod, end_label);

    emit_label(mod, high_diff_label);
    if (equality) {
        emit_mov_reg_imm(mod, EAX, expr->kind == EXPR_NE ? 1u : 0u);
    } else {
        switch (expr->kind) {
            case EXPR_LT: high_cc = unsigned_compare ? CC_B : CC_L; break;
            case EXPR_GT: high_cc = unsigned_compare ? CC_A : CC_G; break;
            case EXPR_LE: high_cc = unsigned_compare ? CC_B : CC_L; break;
            case EXPR_GE: high_cc = unsigned_compare ? CC_A : CC_G; break;
            default: high_cc = CC_E; break;
        }
        emit_setcc(mod, high_cc, EAX);
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xB6);
        emit_byte(mod, modrm(3, EAX, EAX));
    }
    emit_label(mod, end_label);
    emit_add_reg_imm(mod, ESP, 8);
}

static Type* codegen_default_argument_type(Type* type) {
    if (!type) return type_int;
    if (type->kind == TYPE_ENUM || type->kind < TYPE_INT) return type_int;
    if (type->kind == TYPE_FLOAT) return type_double;
    if (type->kind == TYPE_ARRAY) return type_ptr(type->base);
    if (type->kind == TYPE_FUNC) return type_ptr(type);
    return type;
}

static bool gen_inline_method_address(Module* mod, Expr* expr) {
    TypeMethod* method = expr ? expr->call_method : NULL;
    Expr* member = expr ? expr->call_func : NULL;
    if (!method || !member || !method->field) return false;
    if (member->kind == EXPR_PTR_MEMBER) {
        gen_expr(mod, member->member_base);
    } else {
        gen_lvalue(mod, member->member_base);
    }
    if (method->field->offset > 0) {
        emit_add_reg_imm(mod, EAX, method->field->offset);
    }
    return true;
}

static bool gen_inline_method_integer64(Module* mod, Expr* expr) {
    TypeMethod* method = expr ? expr->call_method : NULL;
    if (!method ||
        (method->kind != TYPE_METHOD_FIELD &&
         method->kind != TYPE_METHOD_FIELD_RELEASE) || !expr->type ||
        expr->type->size != 8 ||
        !(type_is_integer(expr->type) || expr->type->kind == TYPE_ENUM) ||
        !gen_inline_method_address(mod, expr)) {
        return false;
    }
    emit_mov_reg_reg(mod, ECX, EAX);
    emit_mov_reg_mem(mod, EAX, ECX, 0);
    emit_mov_reg_mem(mod, EDX, ECX, 4);
    if (method->kind == TYPE_METHOD_FIELD_RELEASE) {
        uint64_t invalid = (uint64_t)method->constant;
        emit_push_reg(mod, EDX);
        emit_push_reg(mod, EAX);
        emit_mov_reg_imm(mod, EAX, (int32_t)(uint32_t)invalid);
        emit_mov_mem_reg(mod, ECX, 0, EAX);
        emit_mov_reg_imm(mod, EAX,
                         (int32_t)(uint32_t)(invalid >> 32));
        emit_mov_mem_reg(mod, ECX, 4, EAX);
        emit_pop_reg(mod, EAX);
        emit_pop_reg(mod, EDX);
    }
    return true;
}

static bool gen_inline_close_call(Module* mod, Expr* expr) {
    CxxCloseCall* lowering = expr ? expr->cxx_close_call : NULL;
    TypeMethod* method = expr ? expr->call_method : NULL;
    int done_label;
    if (!lowering || !method || method->kind != TYPE_METHOD_FIELD_CLOSE ||
        !lowering->object || !lowering->handle || !lowering->cleanup ||
        !method->field || !method->result_field ||
        expr->call_result_offset >= 0) {
        return false;
    }

    done_label = new_label();
    gen_zero_local_storage(mod, expr->call_result_offset,
                           (size_t)expr->type->size);
    gen_expr(mod, lowering->handle);
    if (method->field->type->size == 8) {
        uint64_t invalid = (uint64_t)method->constant;
        int active_label = new_label();
        emit_cmp_reg_imm(mod, EDX,
                         (int32_t)(uint32_t)(invalid >> 32));
        emit_jcc_label(mod, CC_NE, active_label);
        emit_cmp_reg_imm(mod, EAX, (int32_t)(uint32_t)invalid);
        emit_jcc_label(mod, CC_E, done_label);
        emit_label(mod, active_label);
    } else {
        emit_cmp_reg_imm(mod, EAX, (int32_t)method->constant);
        emit_jcc_label(mod, CC_E, done_label);
    }

    gen_expr(mod, lowering->cleanup);
    emit_store_typed32(mod, EBP,
                       expr->call_result_offset + method->result_field->offset,
                       EAX, method->result_field->type);
    emit_cmp_reg_imm(mod, EAX, (int32_t)method->success_constant);
    emit_jcc_label(mod, CC_NE, done_label);

    gen_lvalue(mod, lowering->object);
    if (method->field->offset > 0) {
        emit_add_reg_imm(mod, EAX, method->field->offset);
    }
    if (method->field->type->size == 8) {
        uint64_t invalid = (uint64_t)method->constant;
        emit_mov_reg_reg(mod, ECX, EAX);
        emit_mov_reg_imm(mod, EAX, (uint32_t)invalid);
        emit_mov_mem_reg(mod, ECX, 0, EAX);
        emit_mov_reg_imm(mod, EAX, (uint32_t)(invalid >> 32));
        emit_mov_mem_reg(mod, ECX, 4, EAX);
    } else {
        emit_mov_reg_imm(mod, ECX, (uint32_t)method->constant);
        emit_store_typed32(mod, EAX, 0, ECX, method->field->type);
    }
    emit_label(mod, done_label);
    return true;
}

static bool gen_inline_method_call(Module* mod, Expr* expr) {
    TypeMethod* method = expr ? expr->call_method : NULL;
    if (method && method->kind == TYPE_METHOD_FIELD_CLOSE) {
        return gen_inline_close_call(mod, expr);
    }
    if (!method || !gen_inline_method_address(mod, expr)) return false;
    if (expr->type &&
        (expr->type->kind == TYPE_STRUCT ||
         expr->type->kind == TYPE_UNION ||
         expr->type->kind == TYPE_ARRAY)) {
        return true;
    }
    if ((method->kind == TYPE_METHOD_FIELD_EQ_CONSTANT ||
         method->kind == TYPE_METHOD_FIELD_NE_CONSTANT) &&
        method->field->type->size == 8) {
        uint64_t constant = (uint64_t)method->constant;
        int decisive_label = new_label();
        int end_label = new_label();
        emit_mov_reg_reg(mod, ECX, EAX);
        emit_mov_reg_mem(mod, EAX, ECX, 0);
        emit_mov_reg_mem(mod, EDX, ECX, 4);
        emit_cmp_reg_imm(mod, EDX,
                         (int32_t)(uint32_t)(constant >> 32));
        emit_jcc_label(mod, CC_NE, decisive_label);
        emit_cmp_reg_imm(mod, EAX, (int32_t)(uint32_t)constant);
        emit_setcc(mod,
                   method->kind == TYPE_METHOD_FIELD_EQ_CONSTANT
                       ? CC_E : CC_NE,
                   EAX);
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xB6);
        emit_byte(mod, modrm(3, EAX, EAX));
        emit_jmp_label(mod, end_label);
        emit_label(mod, decisive_label);
        emit_mov_reg_imm(mod, EAX,
                         method->kind == TYPE_METHOD_FIELD_NE_CONSTANT
                             ? 1u : 0u);
        emit_label(mod, end_label);
        return true;
    }
    if (method->kind == TYPE_METHOD_FIELD_RELEASE) {
        emit_mov_reg_reg(mod, ECX, EAX);
        emit_load_typed32(mod, EAX, ECX, 0, method->field->type);
        emit_push_reg(mod, EAX);
        emit_mov_reg_imm(mod, EAX, (int32_t)method->constant);
        emit_store_typed32(mod, ECX, 0, EAX, method->field->type);
        emit_pop_reg(mod, EAX);
        return true;
    }
    emit_load_typed32(mod, EAX, EAX, 0, method->field->type);
    if (method->kind == TYPE_METHOD_FIELD_EQ_CONSTANT ||
        method->kind == TYPE_METHOD_FIELD_NE_CONSTANT) {
        emit_cmp_reg_imm(mod, EAX, (int32_t)method->constant);
        emit_setcc(mod,
                   method->kind == TYPE_METHOD_FIELD_EQ_CONSTANT
                       ? CC_E : CC_NE,
                   EAX);
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xB6);
        emit_byte(mod, modrm(3, EAX, EAX));
    }
    return true;
}

static int gen_cxx_array_cookie_size(void) {
    return g_opts.target_arch == ARCH_X64 ? 8 : 4;
}

/* Allocate a non-trivial array with a target-width element-count cookie.
 * The returned pointer addresses the first element; the allocator pointer is
 * the cookie address and is therefore the value passed to rin_free. */
static void gen_cxx_alloc_array_cookie32(Module* mod, Expr* expr) {
    Type* object_type = expr ? expr->call_new_type : NULL;
    int cookie_size = gen_cxx_array_cookie_size();
    int allocated_label;
    if (!object_type || object_type->size <= 0 ||
        !expr || !expr->call_new_count) {
        rcc_fatal("validated C++ array allocation has incomplete metadata");
    }
    gen_expr(mod, expr->call_new_count);
    emit_push_reg(mod, EAX); /* Preserve the evaluated element count. */
    emit_scale_reg(mod, EAX, (uint32_t)object_type->size);
    emit_add_reg_imm(mod, EAX, cookie_size);
    emit_push_reg(mod, EAX);
    emit_byte(mod, 0xE8);
    {
        uint32_t call_offset = code_offset(mod);
        emit_dword(mod, 0u);
        add_func_call_ref("rin_malloc", call_offset);
    }
    emit_add_reg_imm(mod, ESP, 4);
    allocated_label = new_label();
    emit_cmp_reg_imm(mod, EAX, 0);
    emit_jcc_label(mod, CC_E, allocated_label);
    emit_mov_reg_mem(mod, ECX, ESP, 0);
    emit_mov_mem_reg(mod, EAX, 0, ECX);
    emit_add_reg_imm(mod, EAX, cookie_size);
    emit_label(mod, allocated_label);
    emit_add_reg_imm(mod, ESP, 4);
}

static bool gen_cxx_move_assignment(Module* mod, Expr* expr) {
    CxxMoveAssignment* lowering = expr ? expr->cxx_move_assignment : NULL;
    TypeMethod* release;
    TypeField* field;
    int done_label;
    if (!lowering || !lowering->source || !lowering->cleanup ||
        !lowering->release || !lowering->release->call_method) {
        return false;
    }
    release = lowering->release->call_method;
    field = release->field;
    if (!field) return false;

    done_label = new_label();
    gen_lvalue(mod, expr->binary_lhs);
    emit_push_reg(mod, EAX);
    gen_lvalue(mod, lowering->source);
    emit_pop_reg(mod, ECX);
    emit_cmp_reg_reg(mod, EAX, ECX);
    emit_jcc_label(mod, CC_E, done_label);

    gen_expr(mod, lowering->cleanup);
    if (gen_is_integer64(lowering->release->type)) {
        gen_expr64_pair(mod, lowering->release);
        emit_push_reg(mod, EDX);
        emit_push_reg(mod, EAX);
        gen_lvalue(mod, expr->binary_lhs);
        if (field->offset > 0) {
            emit_add_reg_imm(mod, EAX, field->offset);
        }
        emit_mov_reg_reg(mod, ECX, EAX);
        emit_pop_reg(mod, EAX);
        emit_pop_reg(mod, EDX);
        emit_mov_mem_reg(mod, ECX, 0, EAX);
        emit_mov_mem_reg(mod, ECX, 4, EDX);
    } else {
        gen_expr(mod, lowering->release);
        emit_push_reg(mod, EAX);
        gen_lvalue(mod, expr->binary_lhs);
        if (field->offset > 0) {
            emit_add_reg_imm(mod, EAX, field->offset);
        }
        emit_pop_reg(mod, ECX);
        emit_store_typed32(mod, EAX, 0, ECX, field->type);
    }
    emit_label(mod, done_label);
    gen_lvalue(mod, expr->binary_lhs);
    return true;
}

static void gen_cxx_zero_array32(Module* mod, Expr* expr) {
    Type* object_type = expr ? expr->call_new_type : NULL;
    int loop;
    int done;

    if (expr && expr->call_new_array_cookie) {
        int cookie_size = gen_cxx_array_cookie_size();
        int loop = new_label();
        int done = new_label();
        gen_cxx_alloc_array_cookie32(mod, expr);
        emit_push_reg(mod, EAX); /* Preserve the user pointer. */
        emit_mov_reg_reg(mod, ECX, EAX);
        emit_sub_reg_imm(mod, ECX, cookie_size);
        emit_mov_reg_mem(mod, EDX, ECX, 0);
        emit_cmp_reg_imm(mod, EDX, 0);
        emit_jcc_label(mod, CC_E, done);
        emit_mov_reg_reg(mod, ECX, EAX);
        emit_label(mod, loop);
        emit_mov_reg_imm(mod, EAX, 0u);
        for (int offset = 0; offset + 4 <= object_type->size; offset += 4) {
            emit_mov_mem_reg(mod, ECX, offset, EAX);
        }
        for (int offset = (object_type->size / 4) * 4;
             offset < object_type->size; ++offset) {
            emit_mov_mem_reg8(mod, ECX, offset, EAX);
        }
        emit_add_reg_imm(mod, ECX, object_type->size);
        emit_sub_reg_imm(mod, EDX, 1);
        emit_cmp_reg_imm(mod, EDX, 0);
        emit_jcc_label(mod, CC_NE, loop);
        emit_label(mod, done);
        emit_mov_reg_mem(mod, EAX, ESP, 0);
        emit_add_reg_imm(mod, ESP, 4);
        return;
    }
    if (!object_type || !expr->call_new_count) {
        SourceLoc location;
        codegen_expr_loc(&location, expr);
        rcc_error(location, "array new value-initialization has no element count");
        return;
    }
    /* Keep the bound alive across allocation so a runtime bound is evaluated
     * exactly once.  The allocator receives the same size expression as the
     * ordinary array-new path, but this path owns the bound evaluation. */
    gen_expr(mod, expr->call_new_count);
    emit_push_reg(mod, EAX);
    emit_scale_reg(mod, EAX, (uint32_t)object_type->size);
    emit_push_reg(mod, EAX);
    emit_byte(mod, 0xE8);
    {
        uint32_t call_offset = code_offset(mod);
        emit_dword(mod, 0u);
        add_func_call_ref("rin_malloc", call_offset);
    }
    emit_add_reg_imm(mod, ESP, 4);
    emit_push_reg(mod, EAX);
    emit_mov_reg_mem(mod, ECX, ESP, 0);
    emit_mov_reg_mem(mod, EDX, ESP, 4);
    loop = new_label();
    done = new_label();
    emit_cmp_reg_imm(mod, EDX, 0);
    emit_jcc_label(mod, CC_E, done);
    emit_label(mod, loop);
    emit_mov_reg_imm(mod, EAX, 0u);
    {
        int offset = 0;
        for (; offset + 4 <= object_type->size; offset += 4) {
            emit_mov_mem_reg(mod, ECX, offset, EAX);
        }
        for (; offset < object_type->size; ++offset) {
            emit_mov_mem_reg8(mod, ECX, offset, EAX);
        }
    }
    emit_add_reg_imm(mod, ECX, object_type->size);
    emit_sub_reg_imm(mod, EDX, 1);
    emit_cmp_reg_imm(mod, EDX, 0);
    emit_jcc_label(mod, CC_NE, loop);
    emit_label(mod, done);
    emit_mov_reg_mem(mod, EAX, ESP, 0);
    emit_add_reg_imm(mod, ESP, 8);
}

static void gen_cxx_init_array32(Module* mod, Expr* expr) {
    Type* element_type = expr ? expr->call_new_type : NULL;
    ExprList* argument;
    int offset = 0;

    if (!element_type || element_type->size <= 0 ||
        !expr || !expr->call_new_count || !expr->call_args) {
        SourceLoc location;
        codegen_expr_loc(&location, expr);
        rcc_error(location, "array new initializer has invalid element storage");
        return;
    }
    /* Evaluate a constant element count once and retain the allocation across
     * each initializer expression.  The semantic pass limits this path to
     * scalar elements and proves that the initializer list fits. */
    gen_expr(mod, expr->call_new_count);
    emit_scale_reg(mod, EAX, (uint32_t)element_type->size);
    emit_push_reg(mod, EAX);
    emit_byte(mod, 0xE8);
    {
        uint32_t call_offset = code_offset(mod);
        emit_dword(mod, 0u);
        add_func_call_ref("rin_malloc", call_offset);
    }
    emit_add_reg_imm(mod, ESP, 4);
    emit_push_reg(mod, EAX);
    for (argument = expr->call_new_args; argument;
         argument = argument->next, offset += element_type->size) {
        emit_mov_reg_mem(mod, ECX, ESP, 0);
        gen_expr_as_type(mod, argument->expr, element_type);
        if (type_is_integer(element_type) ||
            element_type->kind == TYPE_ENUM) {
            emit_convert_integer_value(mod, EAX, argument->expr->type,
                                       element_type);
        }
        emit_store_typed32(mod, ECX, offset, EAX, element_type);
    }
    emit_pop_reg(mod, EAX);
}

static void gen_cxx_zero_object32(Module* mod, Type* object_type,
                                   int address_reg) {
    emit_mov_reg_imm(mod, EAX, 0u);
    for (int offset = 0; offset + 4 <= object_type->size; offset += 4) {
        emit_mov_mem_reg(mod, address_reg, offset, EAX);
    }
    for (int offset = (object_type->size / 4) * 4;
         offset < object_type->size; ++offset) {
        emit_mov_mem_reg8(mod, address_reg, offset, EAX);
    }
}

static TypeField* gen_cxx_constructor_field32(Type* object_type,
                                               const char* name) {
    for (TypeField* field = object_type ? object_type->fields : NULL;
         field; field = field->next) {
        if (field->name && name && strcmp(field->name, name) == 0) {
            return field;
        }
    }
    return NULL;
}

static void gen_cxx_call_constructor32(Module* mod,
                                        CxxConstructorInfo* constructor,
                                        ExprList* arguments) {
    Decl* declaration = constructor && constructor->method
        ? constructor->method->decl : NULL;
    ExprList* call_arguments = NULL;
    Expr* this_argument;
    Expr* function;
    Expr* call;
    TypeParam* parameter;
    int source_argument_bytes = 0;
    if (!declaration || !declaration->func_is_cxx_method ||
        !declaration->func_is_cxx_constructor || !declaration->func_body) {
        rcc_fatal("validated C++ constructor has no callable body");
    }
    parameter = declaration->type && declaration->type->kind == TYPE_FUNC
        ? declaration->type->params : NULL;
    if (parameter) parameter = parameter->next;
    for (ExprList* argument = arguments; argument;
         argument = argument->next) {
        Type* passed_type = parameter ? parameter->type : argument->expr->type;
        int bytes;
        if (passed_type && passed_type->is_reference) {
            bytes = 4;
        } else if (passed_type &&
                   (passed_type->kind == TYPE_STRUCT ||
                    passed_type->kind == TYPE_UNION ||
                    passed_type->kind == TYPE_ARRAY)) {
            bytes = (passed_type->size + 3) & ~3;
        } else if (gen_is_floating(passed_type)) {
            bytes = gen_float_width(passed_type);
        } else if (gen_is_integer64(passed_type)) {
            bytes = 8;
        } else {
            bytes = 4;
        }
        if (bytes < 0 || source_argument_bytes > INT_MAX - bytes) {
            rcc_fatal("constructor argument area exceeds compiler limits");
        }
        source_argument_bytes += bytes;
        if (parameter) parameter = parameter->next;
    }
    this_argument = expr_cxx_this(declaration->loc);
    this_argument->cxx_this_stack_offset = source_argument_bytes;
    if (!declaration->type || declaration->type->kind != TYPE_FUNC ||
        !declaration->type->params) {
        rcc_fatal("constructor has no implicit object parameter");
    }
    this_argument->type = declaration->type->params->type;
    exprlist_append(&call_arguments, this_argument);
    for (ExprList* argument = arguments; argument;
         argument = argument->next) {
        exprlist_append(&call_arguments, argument->expr);
    }
    function = expr_ident(declaration->name, declaration->loc);
    function->ident_decl = declaration;
    function->type = declaration->type;
    call = expr_call(function, call_arguments, declaration->loc);
    call->type = type_void;
    emit_push_reg(mod, ECX); /* Preserve the object while source args run. */
    gen_expr(mod, call);
    emit_add_reg_imm(mod, ESP, 4);
}

static void gen_cxx_initialize_object32(Module* mod, Type* object_type,
                                         CxxConstructorInfo* constructor,
                                         ExprList* arguments) {
    TypeField* field;
    CxxConstructorInitializer* initializer;
    ExprList* argument;
    int address_reg = ECX;

    if (!constructor) return;
    if (!constructor->body_is_empty) {
        gen_cxx_call_constructor32(mod, constructor, arguments);
        return;
    }
    gen_cxx_zero_object32(mod, object_type, address_reg);
    if (constructor->parameter_count == 0) {
        for (initializer = constructor->initializers; initializer;
             initializer = initializer->next) {
            field = gen_cxx_constructor_field32(
                object_type, initializer->field);
            if (!field || !initializer->value) {
                rcc_fatal("validated C++ constructor field is missing");
            }
            emit_push_reg(mod, address_reg);
            gen_expr_as_type(mod, initializer->value, field->type);
            emit_pop_reg(mod, address_reg);
            if (type_is_integer(field->type) ||
                field->type->kind == TYPE_ENUM) {
                emit_convert_integer_value(mod, EAX,
                                           initializer->value->type,
                                           field->type);
            }
            emit_store_typed32(mod, address_reg, field->offset,
                               EAX, field->type);
        }
        return;
    }
    field = object_type ? object_type->fields : NULL;
    for (argument = arguments; argument && field;
         argument = argument->next, field = field->next) {
        emit_push_reg(mod, address_reg);
        gen_expr_as_type(mod, argument->expr, field->type);
        emit_pop_reg(mod, address_reg);
        if (type_is_integer(field->type) || field->type->kind == TYPE_ENUM) {
            emit_convert_integer_value(mod, EAX, argument->expr->type,
                                       field->type);
        }
        emit_store_typed32(mod, address_reg, field->offset,
                           EAX, field->type);
    }
}

/* Lower the validated one-parameter constructor for each explicitly
 * initialized class-array element.  The semantic pass proves that the
 * constructor initializes the complete object from one scalar parameter and
 * that the destructor is trivial, so no hidden cookie or cleanup ABI is
 * needed for this allocation. */
static void gen_cxx_init_class_array32(Module* mod, Expr* expr) {
    Type* object_type = expr ? expr->call_new_type : NULL;
    TypeField* field = object_type ? object_type->fields : NULL;
    ExprList* argument;
    int32_t element_size;

    if (!object_type || !field || object_type->size <= 0 ||
        !expr || !expr->call_new_count || !expr->call_new_constructor) {
        SourceLoc location;
        codegen_expr_loc(&location, expr);
        rcc_error(location,
                  "array new constructor has invalid element storage");
        return;
    }
    element_size = object_type->size;
    if (expr->call_new_array_cookie) {
        gen_cxx_alloc_array_cookie32(mod, expr);
        emit_push_reg(mod, EAX);              /* user base */
        emit_push_reg(mod, EAX);              /* current, user base */
    } else {
        gen_expr(mod, expr->call_new_count);
        emit_push_reg(mod, EAX);              /* count */
        emit_scale_reg(mod, EAX, (uint32_t)element_size);
        emit_push_reg(mod, EAX);
        emit_byte(mod, 0xE8);
        {
            uint32_t call_offset = code_offset(mod);
            emit_dword(mod, 0u);
            add_func_call_ref("rin_malloc", call_offset);
        }
        emit_add_reg_imm(mod, ESP, 4);
        emit_push_reg(mod, EAX);              /* base, count */
        emit_push_reg(mod, EAX);              /* current, base, count */
    }

    for (argument = expr->call_new_args; argument;
         argument = argument->next) {
        emit_mov_reg_mem(mod, ECX, ESP,
                         expr->call_new_array_cookie ? 0 : 4);
        gen_cxx_initialize_object32(mod, object_type,
                                     expr->call_new_constructor,
                                     argument);
        emit_mov_reg_mem(mod, ECX, ESP,
                         expr->call_new_array_cookie ? 0 : 4);
        emit_add_reg_imm(mod, ECX, (uint32_t)element_size);
        emit_mov_mem_reg(mod, ESP,
                         expr->call_new_array_cookie ? 0 : 4, ECX);
    }
    emit_mov_reg_mem(mod, EAX, ESP,
                     expr->call_new_array_cookie ? 4 : 8);
    emit_add_reg_imm(mod, ESP,
                     expr->call_new_array_cookie ? 8 : 12);
}

static void gen_cxx_init_default_class_array32(Module* mod, Expr* expr) {
    Type* object_type = expr ? expr->call_new_type : NULL;
    int loop;
    int done;

    if (!object_type || object_type->size <= 0 ||
        !expr || !expr->call_new_count || !expr->call_new_constructor) {
        SourceLoc location;
        codegen_expr_loc(&location, expr);
        rcc_error(location,
                  "array new default constructor has invalid element storage");
        return;
    }
    if (expr->call_new_array_cookie) {
        gen_cxx_alloc_array_cookie32(mod, expr);
        emit_push_reg(mod, EAX);              /* user base */
        emit_push_reg(mod, EAX);              /* current, user base */
        emit_mov_reg_reg(mod, ECX, EAX);
        emit_sub_reg_imm(mod, ECX, gen_cxx_array_cookie_size());
        emit_mov_reg_mem(mod, EDX, ECX, 0);
        emit_push_reg(mod, EDX);              /* count, current, user */
        loop = new_label();
        done = new_label();
        emit_cmp_reg_imm(mod, EDX, 0);
        emit_jcc_label(mod, CC_E, done);
        emit_label(mod, loop);
        emit_mov_reg_mem(mod, ECX, ESP, 4);
        gen_cxx_initialize_object32(mod, object_type,
                                     expr->call_new_constructor, NULL);
        emit_mov_reg_mem(mod, ECX, ESP, 4);
        emit_add_reg_imm(mod, ECX, (uint32_t)object_type->size);
        emit_mov_mem_reg(mod, ESP, 4, ECX);
        emit_mov_reg_mem(mod, EDX, ESP, 0);
        emit_sub_reg_imm(mod, EDX, 1);
        emit_mov_mem_reg(mod, ESP, 0, EDX);
        emit_cmp_reg_imm(mod, EDX, 0);
        emit_jcc_label(mod, CC_NE, loop);
        emit_label(mod, done);
        emit_mov_reg_mem(mod, EAX, ESP, 8);
        emit_add_reg_imm(mod, ESP, 12);
        return;
    } else {
        gen_expr(mod, expr->call_new_count);
        emit_push_reg(mod, EAX);              /* count */
        emit_scale_reg(mod, EAX, (uint32_t)object_type->size);
        emit_push_reg(mod, EAX);
        emit_byte(mod, 0xE8);
        {
            uint32_t call_offset = code_offset(mod);
            emit_dword(mod, 0u);
            add_func_call_ref("rin_malloc", call_offset);
        }
        emit_add_reg_imm(mod, ESP, 4);
        emit_push_reg(mod, EAX);              /* base, count */
        emit_push_reg(mod, EAX);              /* current, base, count */
        emit_mov_reg_mem(mod, ECX, ESP, 4);
        emit_mov_reg_mem(mod, EDX, ESP, 8);
    }
    loop = new_label();
    done = new_label();
    emit_cmp_reg_imm(mod, EDX, 0);
    emit_jcc_label(mod, CC_E, done);
    emit_label(mod, loop);
    gen_cxx_initialize_object32(mod, object_type,
                                 expr->call_new_constructor, NULL);
    emit_add_reg_imm(mod, ECX, (uint32_t)object_type->size);
    emit_sub_reg_imm(mod, EDX, 1);
    emit_cmp_reg_imm(mod, EDX, 0);
    emit_jcc_label(mod, CC_NE, loop);
    emit_label(mod, done);
    emit_mov_reg_mem(mod, EAX, ESP, 8);
    emit_add_reg_imm(mod, ESP, 12);
}

static void gen_cxx_new32(Module* mod, Expr* expr) {
    Type* object_type = expr ? expr->call_new_type : NULL;
    TypeField* field;
    ExprList* argument;
    bool saved_is_new;
    bool initialize;

    if (!object_type || object_type->size <= 0) {
        SourceLoc location;
        codegen_expr_loc(&location, expr);
        rcc_error(location, "C++ new expression has no complete storage type");
        return;
    }
    if (expr->call_new_is_array && expr->call_new_constructor &&
        expr->call_new_args) {
        gen_cxx_init_class_array32(mod, expr);
        return;
    }
    if (expr->call_new_is_array && expr->call_new_constructor &&
        !expr->call_new_args) {
        gen_cxx_init_default_class_array32(mod, expr);
        return;
    }
    if (expr->call_new_is_array && expr->call_new_value_init) {
        gen_cxx_zero_array32(mod, expr);
        return;
    }
    if (expr->call_new_is_array && expr->call_new_args) {
        gen_cxx_init_array32(mod, expr);
        return;
    }
    if (expr->call_new_is_array && expr->call_new_array_cookie) {
        gen_cxx_alloc_array_cookie32(mod, expr);
        return;
    }
    saved_is_new = expr->call_is_new;
    expr->call_is_new = false;
    gen_call(mod, expr);
    expr->call_is_new = saved_is_new;

    if (expr->call_new_is_array) {
        return;
    }
    argument = expr->call_new_args;
    initialize = expr->call_new_value_init ||
                 expr->call_new_constructor != NULL ||
                 argument != NULL;
    if (!initialize) return;

    emit_push_reg(mod, EAX); /* retain the allocation across initializers */
    emit_mov_reg_mem(mod, ECX, ESP, 0);
    if (expr->call_new_constructor) {
        gen_cxx_initialize_object32(mod, object_type,
                                     expr->call_new_constructor,
                                     argument);
        emit_pop_reg(mod, EAX);
        return;
    }
    emit_mov_reg_imm(mod, EAX, 0u);
    if (object_type->kind == TYPE_STRUCT ||
        object_type->kind == TYPE_UNION) {
        for (int offset = 0; offset + 4 <= object_type->size; offset += 4) {
            emit_mov_mem_reg(mod, ECX, offset, EAX);
        }
        for (int offset = (object_type->size / 4) * 4;
             offset < object_type->size; ++offset) {
            emit_mov_mem_reg8(mod, ECX, offset, EAX);
        }
        field = object_type->fields;
        while (field && argument) {
            emit_mov_reg_mem(mod, ECX, ESP, 0);
            gen_expr(mod, argument->expr);
            if (type_is_integer(field->type) ||
                field->type->kind == TYPE_ENUM) {
                emit_convert_integer_value(mod, EAX, argument->expr->type,
                                           field->type);
            }
            emit_store_typed32(mod, ECX, field->offset, EAX, field->type);
            field = field->next;
            argument = argument->next;
        }
    } else if (argument) {
        emit_mov_reg_mem(mod, ECX, ESP, 0);
        gen_expr(mod, argument->expr);
        if (type_is_integer(object_type) ||
            object_type->kind == TYPE_ENUM) {
            emit_convert_integer_value(mod, EAX, argument->expr->type,
                                       object_type);
        }
        emit_store_typed32(mod, ECX, 0, EAX, object_type);
    } else {
        emit_mov_reg_mem(mod, ECX, ESP, 0);
        emit_store_typed32(mod, ECX, 0, EAX, object_type);
    }
    emit_pop_reg(mod, EAX);
}

static void gen_cxx_array_destructor32(Module* mod, Expr* expr) {
    Type* object_type;
    Decl* destructor;
    Decl* cleanup;
    TypeField* field;
    int cookie_size;
    int loop;
    int free_label;
    int done;
    if (!expr || !expr->call_args || !expr->call_args->expr ||
        !expr->call_args->expr->type ||
        expr->call_args->expr->type->kind != TYPE_PTR) {
        rcc_fatal("validated C++ array delete has incomplete operand metadata");
    }
    object_type = expr->call_args->expr->type->base;
    destructor = expr->call_delete_array_destructor;
    cleanup = expr->call_delete_array_cleanup;
    field = expr->call_delete_array_cleanup_field;
    cookie_size = gen_cxx_array_cookie_size();
    if (!object_type || object_type->size <= 0 ||
        (!destructor && (!cleanup || !field))) {
        rcc_fatal("validated C++ array delete has incomplete destructor metadata");
    }

    done = new_label();
    gen_expr(mod, expr->call_args->expr);
    emit_cmp_reg_imm(mod, EAX, 0);
    emit_jcc_label(mod, CC_E, done);
    emit_push_reg(mod, EAX);                  /* user pointer */
    emit_mov_reg_reg(mod, ECX, EAX);
    emit_sub_reg_imm(mod, ECX, cookie_size);
    emit_mov_reg_mem(mod, EDX, ECX, 0);       /* element count */
    emit_push_reg(mod, ECX);                  /* allocator pointer */
    emit_push_reg(mod, EDX);                  /* remaining count */
    emit_push_reg(mod, EAX);                  /* current element */
    free_label = new_label();
    loop = new_label();
    emit_cmp_reg_imm(mod, EDX, 0);
    emit_jcc_label(mod, CC_E, free_label);

    /* Start with the last constructed element; delete[] destroys elements in
     * reverse order.  The current pointer and count live on the stack so a
     * user destructor may freely clobber caller-saved registers. */
    emit_mov_reg_mem(mod, EAX, ESP, 0);
    emit_mov_reg_mem(mod, ECX, ESP, 4);
    emit_sub_reg_imm(mod, ECX, 1);
    emit_scale_reg(mod, ECX, (uint32_t)object_type->size);
    emit_add_reg_reg(mod, EAX, ECX);
    emit_mov_mem_reg(mod, ESP, 0, EAX);
    emit_label(mod, loop);
    if (destructor) {
        emit_mov_reg_mem(mod, EAX, ESP, 0);
        emit_push_reg(mod, EAX);              /* preserve current */
        emit_push_reg(mod, EAX);              /* destructor argument */
        emit_byte(mod, 0xE8);
        {
            uint32_t call_offset = code_offset(mod);
            emit_dword(mod, 0u);
            add_func_call_ref(decl_link_name(destructor), call_offset);
        }
        emit_add_reg_imm(mod, ESP, 4);
        emit_pop_reg(mod, EAX);
    } else {
        int skip_cleanup = new_label();
        emit_mov_reg_mem(mod, ECX, ESP, 0);
        if (field->type && field->type->size == 8) {
            uint64_t invalid = (uint64_t)
                expr->call_delete_array_cleanup_invalid;
            emit_mov_reg_mem(mod, EAX, ECX, field->offset);
            emit_mov_reg_mem(mod, EDX, ECX, field->offset + 4);
            emit_cmp_reg_imm(mod, EDX,
                             (int32_t)(uint32_t)(invalid >> 32));
            emit_jcc_label(mod, CC_NE, skip_cleanup);
            emit_cmp_reg_imm(mod, EAX, (int32_t)(uint32_t)invalid);
            emit_jcc_label(mod, CC_E, skip_cleanup);
            emit_push_reg(mod, EDX);
            emit_push_reg(mod, EAX);
            emit_byte(mod, 0xE8);
            {
                uint32_t call_offset = code_offset(mod);
                emit_dword(mod, 0u);
                add_func_call_ref(decl_link_name(cleanup), call_offset);
            }
            emit_add_reg_imm(mod, ESP, 8);
        } else {
            emit_load_typed32(mod, EAX, ECX, field->offset, field->type);
            emit_cmp_reg_imm(mod, EAX,
                             (int32_t)expr->call_delete_array_cleanup_invalid);
            emit_jcc_label(mod, CC_E, skip_cleanup);
            emit_push_reg(mod, EAX);
            emit_byte(mod, 0xE8);
            {
                uint32_t call_offset = code_offset(mod);
                emit_dword(mod, 0u);
                add_func_call_ref(decl_link_name(cleanup), call_offset);
            }
            emit_add_reg_imm(mod, ESP, 4);
        }
        emit_label(mod, skip_cleanup);
    }
    emit_mov_reg_mem(mod, ECX, ESP, 0);
    emit_sub_reg_imm(mod, ECX, (uint32_t)object_type->size);
    emit_mov_mem_reg(mod, ESP, 0, ECX);
    emit_mov_reg_mem(mod, EDX, ESP, 4);
    emit_sub_reg_imm(mod, EDX, 1);
    emit_mov_mem_reg(mod, ESP, 4, EDX);
    emit_cmp_reg_imm(mod, EDX, 0);
    emit_jcc_label(mod, CC_NE, loop);

    emit_label(mod, free_label);
    emit_mov_reg_mem(mod, EAX, ESP, 8);       /* allocator pointer */
    emit_push_reg(mod, EAX);
    emit_byte(mod, 0xE8);
    {
        uint32_t free_offset = code_offset(mod);
        emit_dword(mod, 0u);
        add_func_call_ref("rin_free", free_offset);
    }
    emit_add_reg_imm(mod, ESP, 4);
    emit_add_reg_imm(mod, ESP, 16);
    emit_mov_reg_imm(mod, EAX, 0u);
    emit_label(mod, done);
}

static void gen_cxx_delete32(Module* mod, Expr* expr) {
    Decl* destructor = expr ? expr->call_delete_destructor : NULL;
    Decl* cleanup = expr ? expr->call_delete_cleanup : NULL;
    TypeField* field = expr ? expr->call_delete_cleanup_field : NULL;
    int skip_cleanup;
    int done;

    if (destructor) {
        if (!expr->call_args || !expr->call_args->expr ||
            !destructor->link_name) {
            rcc_error(expr ? expr->loc : (SourceLoc){"<delete>", 0, 0},
                      "C++ delete destructor metadata is incomplete");
            return;
        }
        done = new_label();
        gen_expr(mod, expr->call_args->expr);
        emit_cmp_reg_imm(mod, EAX, 0);
        emit_jcc_label(mod, CC_E, done);
        emit_push_reg(mod, EAX);
        emit_push_reg(mod, EAX);
        emit_byte(mod, 0xE8);
        {
            uint32_t call_offset = code_offset(mod);
            emit_dword(mod, 0);
            add_func_call_ref(decl_link_name(destructor), call_offset);
        }
        emit_add_reg_imm(mod, ESP, 4);
        emit_pop_reg(mod, EAX);
        emit_push_reg(mod, EAX);
        emit_byte(mod, 0xE8);
        {
            uint32_t free_offset = code_offset(mod);
            emit_dword(mod, 0);
            add_func_call_ref("rin_free", free_offset);
        }
        emit_add_reg_imm(mod, ESP, 4);
        emit_mov_reg_imm(mod, EAX, 0);
        emit_label(mod, done);
        return;
    }
    if (!cleanup || !field || !expr->call_args ||
        !expr->call_args->expr) {
        return;
    }
    skip_cleanup = new_label();
    done = new_label();
    gen_expr(mod, expr->call_args->expr);
    emit_cmp_reg_imm(mod, EAX, 0);
    emit_jcc_label(mod, CC_E, done);
    emit_push_reg(mod, EAX); /* Keep the object address across destructor. */
    emit_mov_reg_mem(mod, ECX, ESP, 0);
    if (field->type && field->type->size == 8) {
        uint64_t invalid = (uint64_t)expr->call_delete_cleanup_invalid;
        emit_mov_reg_mem(mod, EAX, ECX, field->offset);
        emit_mov_reg_mem(mod, EDX, ECX, field->offset + 4);
        emit_cmp_reg_imm(mod, EDX, (int32_t)(uint32_t)(invalid >> 32));
        emit_jcc_label(mod, CC_NE, skip_cleanup);
        emit_cmp_reg_imm(mod, EAX, (int32_t)(uint32_t)invalid);
        emit_jcc_label(mod, CC_E, skip_cleanup);
        emit_push_reg(mod, EDX);
        emit_push_reg(mod, EAX);
        emit_byte(mod, 0xE8);
        uint32_t call_offset = code_offset(mod);
        emit_dword(mod, 0);
        add_func_call_ref(decl_link_name(cleanup), call_offset);
        emit_add_reg_imm(mod, ESP, 8);
    } else {
        emit_load_typed32(mod, EAX, ECX, field->offset, field->type);
        emit_cmp_reg_imm(mod, EAX,
                         (int32_t)expr->call_delete_cleanup_invalid);
        emit_jcc_label(mod, CC_E, skip_cleanup);
        emit_push_reg(mod, EAX);
        emit_byte(mod, 0xE8);
        uint32_t call_offset = code_offset(mod);
        emit_dword(mod, 0);
        add_func_call_ref(decl_link_name(cleanup), call_offset);
        emit_add_reg_imm(mod, ESP, 4);
    }
    emit_label(mod, skip_cleanup);
    emit_mov_reg_mem(mod, EAX, ESP, 0);
    emit_push_reg(mod, EAX);
    emit_byte(mod, 0xE8);
    uint32_t free_offset = code_offset(mod);
    emit_dword(mod, 0);
    add_func_call_ref("rin_free", free_offset);
    emit_add_reg_imm(mod, ESP, 4);
    emit_add_reg_imm(mod, ESP, 4);
    emit_mov_reg_imm(mod, EAX, 0);
    emit_label(mod, done);
}

static void gen_call(Module* mod, Expr* expr) {
    int argument_bytes = 0;
    int argc;
    ExprList** args;
    Type** argument_types;
    Type* function_type;
    TypeParam* parameter;
    int i;
    Expr* func_expr;

    if (expr->call_is_new) {
        gen_cxx_new32(mod, expr);
        return;
    }
    if (expr->call_is_delete && expr->call_delete_is_array &&
        (expr->call_delete_array_cleanup ||
         expr->call_delete_array_destructor)) {
        gen_cxx_array_destructor32(mod, expr);
        return;
    }
    if (expr->call_is_delete &&
        (expr->call_delete_cleanup || expr->call_delete_destructor)) {
        gen_cxx_delete32(mod, expr);
        return;
    }
    if (gen_inline_method_call(mod, expr)) return;
    if (gen_atomic_builtin(mod, expr)) return;

    argc = exprlist_len(expr->call_args);
    args = rcc_alloc((size_t)argc * sizeof(ExprList*));
    argument_types = rcc_alloc((size_t)argc * sizeof(Type*));
    function_type = expr->call_func ? expr->call_func->type : NULL;
    if (function_type && function_type->kind == TYPE_PTR) {
        function_type = function_type->base;
    }
    parameter = function_type && function_type->kind == TYPE_FUNC
        ? function_type->params : NULL;
    i = 0;
    for (ExprList* argument = expr->call_args; argument;
         argument = argument->next) {
        args[i] = argument;
        argument_types[i] = parameter ? parameter->type
            : codegen_default_argument_type(argument->expr->type);
        if (parameter) parameter = parameter->next;
        ++i;
    }
    for (i = argc - 1; i >= 0; --i) {
        Expr* argument = args[i]->expr;
        Type* passed_type = argument_types[i];
        if (passed_type && passed_type->is_reference) {
            gen_lvalue(mod, argument);
            emit_push_reg(mod, EAX);
            argument_bytes += 4;
            continue;
        }
        if (passed_type && (passed_type->kind == TYPE_STRUCT ||
                            passed_type->kind == TYPE_UNION)) {
            int units = (passed_type->size + 3) / 4;
            if (argument->kind == EXPR_VA_ARG) {
                gen_expr(mod, argument);
            } else {
                gen_lvalue(mod, argument);
            }
            emit_mov_reg_reg(mod, ECX, EAX);
            for (int unit = units - 1; unit >= 0; --unit) {
                emit_mov_reg_mem(mod, EAX, ECX, unit * 4);
                emit_push_reg(mod, EAX);
            }
            argument_bytes += units * 4;
            continue;
        }
        if (gen_is_floating(passed_type)) {
            gen_expr_as_type(mod, argument, passed_type);
            if (gen_float_width(passed_type) == 4) {
                emit_push_reg(mod, EAX);
                argument_bytes += 4;
            } else {
                emit_push_reg(mod, EDX);
                emit_push_reg(mod, EAX);
                argument_bytes += 8;
            }
            continue;
        }
        gen_expr(mod, argument);
        if (gen_is_integer64(passed_type)) {
            if (!gen_is_integer64(argument->type)) {
                emit_extend_eax_to_integer64(mod, argument->type);
            }
            emit_push_reg(mod, EDX);
            emit_push_reg(mod, EAX);
            argument_bytes += 8;
        } else {
            if (type_is_integer(passed_type) ||
                (passed_type && passed_type->kind == TYPE_ENUM)) {
                emit_convert_integer_value(mod, EAX, argument->type,
                                           passed_type);
            }
            emit_push_reg(mod, EAX);
            argument_bytes += 4;
        }
    }
    if (expr->type && (expr->type->kind == TYPE_STRUCT ||
                       expr->type->kind == TYPE_UNION)) {
        if (expr->call_result_offset >= 0) {
            rcc_error(expr->loc,
                      "aggregate call has no automatic result slot");
        }
        emit_byte(mod, 0x8D);  /* LEA EAX, [EBP+disp32] */
        emit_byte(mod, modrm(2, EAX, EBP));
        emit_dword(mod, (uint32_t)expr->call_result_offset);
        emit_push_reg(mod, EAX);
        argument_bytes += 4;
    }
    rcc_free(argument_types);
    rcc_free(args);

    func_expr = expr->call_func;
    if (expr->call_is_virtual && expr->call_virtual_index >= 0) {
        int this_offset = argument_bytes - 4;
        emit_mov_reg_mem(mod, EAX, ESP, this_offset);
        emit_mov_reg_mem(mod, EAX, EAX, 0);
        emit_mov_reg_mem(mod, EAX, EAX,
                         expr->call_virtual_index * 4);
        emit_byte(mod, 0xFF);
        emit_byte(mod, modrm(3, 2, EAX));
    } else if (func_expr->kind == EXPR_IDENT && func_expr->ident_decl &&
        func_expr->ident_decl->kind == DECL_FUNC) {
        Decl* func_decl = func_expr->ident_decl;
        uint32_t call_offset;

        emit_byte(mod, 0xE8);
        call_offset = code_offset(mod);
        emit_dword(mod, 0);
        /* Resolve after all definitions are emitted.  This also handles a
         * prototype-before-definition without trusting the stale Decl body
         * pointer; unresolved names become normal external REL32 relocations
         * in resolve_func_calls(). */
        add_func_call_ref(decl_link_name(func_decl), call_offset);
    } else {
        gen_expr(mod, func_expr);
        emit_byte(mod, 0xFF);
        emit_byte(mod, modrm(3, 2, EAX));
    }

    if (gen_is_floating(expr->type)) {
        emit_raw_from_x87_value(mod, expr->type);
    }

    if (argument_bytes > 0) {
        emit_add_reg_imm(mod, ESP, argument_bytes);
    }
}

static void gen_floating_compound_assignment(Module* mod, Expr* expr,
                                             int operation) {
    Type* type = expr->binary_lhs->type;
    int width = gen_float_width(type);

    gen_lvalue(mod, expr->binary_lhs);
    emit_push_reg(mod, EAX); /* keep the destination address below operands */
    emit_mov_reg_mem(mod, ECX, ESP, 0);
    emit_load_floating_raw(mod, type, ECX, 0);
    if (width == 4) emit_push_reg(mod, EAX);
    else {
        emit_push_reg(mod, EDX);
        emit_push_reg(mod, EAX);
    }
    gen_expr_as_type(mod, expr->binary_rhs, type);
    if (width == 4) emit_push_reg(mod, EAX);
    else {
        emit_push_reg(mod, EDX);
        emit_push_reg(mod, EAX);
    }
    emit_x87_binary_stack(mod, type, operation);
    emit_pop_reg(mod, ECX);
    emit_store_floating_raw(mod, type, ECX, 0);
}

static void gen_expr_raw(Module* mod, Expr* expr) {
    if (!expr) return;

    switch (expr->kind) {
        case EXPR_INT_LIT:
            emit_mov_reg_imm(mod, EAX, (uint32_t)expr->int_val);
            break;

        case EXPR_CHAR_LIT:
            emit_mov_reg_imm(mod, EAX, (uint32_t)(uint8_t)expr->char_val);
            break;

        case EXPR_FLOAT_LIT: {
            uint64_t bits = 0u;
            if (expr->type && expr->type->kind == TYPE_FLOAT) {
                float value = (float)expr->float_val;
                memcpy(&bits, &value, sizeof(value));
            } else {
                double value = expr->float_val;
                memcpy(&bits, &value, sizeof(value));
            }
            emit_mov_reg_imm(mod, EAX, (uint32_t)bits);
            if (gen_float_width(expr->type) == 8) {
                emit_mov_reg_imm(mod, EDX, (uint32_t)(bits >> 32));
            }
            break;
        }

        case EXPR_STRING_LIT: {
            uint32_t offset = emit_string(mod, expr->str_val);
            module_ensure_rodata_base_symbol(mod);
            gen_symbol_address(mod, "__rcc_rodata_base", offset);
            break;
        }

        case EXPR_CXX_THIS:
            if (expr->cxx_this_stack_offset < 0) {
                rcc_fatal("constructor this argument has no saved object");
            }
            emit_mov_reg_mem(mod, EAX, ESP, expr->cxx_this_stack_offset);
            break;

        case EXPR_IDENT: {
            Decl* decl = expr->ident_decl;
            if (!decl) {
                emit_mov_reg_imm(mod, EAX, 0);
                break;
            }
            if (decl->kind == DECL_FUNC) {
                gen_symbol_address(mod, decl_link_name(decl), 0u);
            } else if (decl->type && decl->type->is_reference) {
                gen_lvalue(mod, expr);
                if (expr->type && expr->type->kind != TYPE_ARRAY &&
                    expr->type->kind != TYPE_STRUCT &&
                    expr->type->kind != TYPE_UNION) {
                    if (gen_is_floating(expr->type)) {
                        emit_load_floating_raw(mod, expr->type, EAX, 0);
                    } else {
                        emit_load_typed32(mod, EAX, EAX, 0, expr->type);
                    }
                }
            } else if (decl->var_is_thread_local) {
                gen_lvalue(mod, expr);
                if (gen_is_floating(decl->type)) {
                    emit_load_floating_raw(mod, decl->type, EAX, 0);
                } else {
                    emit_load_typed32(mod, EAX, EAX, 0, decl->type);
                }
            } else if (decl->type && decl->type->kind == TYPE_ARRAY) {
                gen_lvalue(mod, expr);
            } else if (decl->var_is_global) {
                gen_symbol_address(mod, decl_link_name(decl), 0u);
                if (gen_is_floating(decl->type)) {
                    emit_load_floating_raw(mod, decl->type, EAX, 0);
                } else {
                    emit_load_typed32(mod, EAX, EAX, 0, decl->type);
                }
            } else {
                if (gen_is_floating(decl->type)) {
                    emit_load_floating_raw(mod, decl->type, EBP,
                                           decl->var_offset);
                } else {
                    emit_load_typed32(mod, EAX, EBP, decl->var_offset,
                                      decl->type);
                }
            }
            break;
        }

        case EXPR_NEG:
            gen_expr(mod, expr->unary_operand);
            if (gen_is_floating(expr->type)) {
                emit_x87_double_value_from_raw(mod, expr->type);
                emit_byte(mod, 0xD9);
                emit_byte(mod, 0xE0); /* FCHS */
                emit_raw_from_x87_value(mod, expr->type);
            } else {
                emit_neg_reg(mod, EAX);
            }
            break;

        case EXPR_BITNOT:
            gen_expr(mod, expr->unary_operand);
            emit_not_reg(mod, EAX);
            break;

        case EXPR_NOT:
            gen_expr(mod, expr->unary_operand);
            emit_test_scalar_value(mod, expr->unary_operand->type);
            if (gen_is_floating(expr->unary_operand->type)) {
                emit_xor_reg_imm8(mod, EAX, 1u);
            } else {
                emit_setcc(mod, CC_E, EAX);
                emit_byte(mod, 0x0F);  /* MOVZX EAX, AL */
                emit_byte(mod, 0xB6);
                emit_byte(mod, modrm(3, EAX, EAX));
            }
            break;

        case EXPR_ADDR:
            gen_lvalue(mod, expr->unary_operand);
            break;

        case EXPR_DEREF:
            gen_expr(mod, expr->unary_operand);
            /* An aggregate lvalue evaluates to its address.  This is
             * especially important for a pointer-to-array dereference:
             * `(*row)[index]` must index the array object, not load its first
             * element before applying the index. */
            if (expr->type && (expr->type->kind == TYPE_ARRAY ||
                               expr->type->kind == TYPE_STRUCT ||
                               expr->type->kind == TYPE_UNION)) {
                break;
            }
            if (gen_is_floating(expr->type)) {
                emit_load_floating_raw(mod, expr->type, EAX, 0);
            } else {
                emit_load_typed32(mod, EAX, EAX, 0, expr->type);
            }
            break;

        case EXPR_PREINC:
        case EXPR_PREDEC:
            if (expr->unary_operand && expr->unary_operand->member_field &&
                expr->unary_operand->member_field->is_bitfield) {
                gen_lvalue(mod, expr->unary_operand);
                emit_push_reg(mod, EAX);
                gen_bitfield_load32(mod, expr->unary_operand->member_field);
                if (expr->kind == EXPR_PREINC) {
                    emit_add_reg_imm(mod, EAX, 1);
                } else {
                    emit_sub_reg_imm(mod, EAX, 1);
                }
                emit_pop_reg(mod, EDX);
                emit_mov_reg_reg(mod, ECX, EAX);
                emit_bitfield_store32(mod, expr->unary_operand->member_field);
                break;
            }
            if (gen_is_floating(expr->type)) {
                gen_lvalue(mod, expr->unary_operand);
                emit_push_reg(mod, EAX); /* address */
                emit_load_floating_raw(mod, expr->type, EAX, EAX);
                emit_x87_add_one_raw(mod, expr->type,
                                     expr->kind == EXPR_PREDEC);
                emit_pop_reg(mod, ECX);
                emit_store_floating_raw(mod, expr->type, ECX, 0);
                break;
            }
            gen_lvalue(mod, expr->unary_operand);
            emit_push_reg(mod, EAX);
            emit_load_typed32(mod, EAX, EAX, 0, expr->type);
            if (expr->kind == EXPR_PREINC) {
                emit_add_reg_imm(mod, EAX,
                                 (int)codegen_increment_size(expr->type));
            } else {
                emit_sub_reg_imm(mod, EAX,
                                 (int)codegen_increment_size(expr->type));
            }
            emit_pop_reg(mod, ECX);
            emit_store_typed32(mod, ECX, 0, EAX, expr->type);
            break;

        case EXPR_POSTINC:
        case EXPR_POSTDEC:
            if (expr->unary_operand && expr->unary_operand->member_field &&
                expr->unary_operand->member_field->is_bitfield) {
                gen_lvalue(mod, expr->unary_operand);
                emit_push_reg(mod, EAX);
                gen_bitfield_load32(mod, expr->unary_operand->member_field);
                emit_push_reg(mod, EAX); /* post-expression value */
                if (expr->kind == EXPR_POSTINC) {
                    emit_add_reg_imm(mod, EAX, 1);
                } else {
                    emit_sub_reg_imm(mod, EAX, 1);
                }
                emit_mov_reg_reg(mod, ECX, EAX);
                emit_pop_reg(mod, EAX);
                emit_pop_reg(mod, EDX);
                emit_push_reg(mod, EAX);
                emit_bitfield_store32(mod, expr->unary_operand->member_field);
                emit_pop_reg(mod, EAX);
                break;
            }
            if (gen_is_floating(expr->type)) {
                gen_lvalue(mod, expr->unary_operand);
                emit_push_reg(mod, EAX); /* address */
                emit_load_floating_raw(mod, expr->type, EAX, EAX);
                if (gen_float_width(expr->type) == 4) {
                    emit_push_reg(mod, EAX); /* old value */
                } else {
                    emit_push_reg(mod, EDX);
                    emit_push_reg(mod, EAX);
                }
                emit_x87_add_one_raw(mod, expr->type,
                                     expr->kind == EXPR_POSTDEC);
                emit_mov_reg_mem(mod, ECX, ESP,
                                 gen_float_width(expr->type) == 4 ? 4 : 8);
                emit_store_floating_raw(mod, expr->type, ECX, 0);
                if (gen_float_width(expr->type) == 4) {
                    emit_pop_reg(mod, EAX);
                    emit_add_reg_imm(mod, ESP, 4);
                } else {
                    emit_pop_reg(mod, EAX);
                    emit_pop_reg(mod, EDX);
                    emit_add_reg_imm(mod, ESP, 4);
                }
                break;
            }
            gen_lvalue(mod, expr->unary_operand);
            emit_push_reg(mod, EAX);
            emit_load_typed32(mod, EAX, EAX, 0, expr->type);
            emit_mov_reg_reg(mod, EDX, EAX);
            if (expr->kind == EXPR_POSTINC) {
                emit_add_reg_imm(mod, EDX,
                                 (int)codegen_increment_size(expr->type));
            } else {
                emit_sub_reg_imm(mod, EDX,
                                 (int)codegen_increment_size(expr->type));
            }
            emit_pop_reg(mod, ECX);
            emit_store_typed32(mod, ECX, 0, EDX, expr->type);
            break;

        case EXPR_ADD:
            if (gen_is_floating(expr->type)) {
                Type* arithmetic_type = expr->type;
                gen_expr_as_type(mod, expr->binary_lhs, arithmetic_type);
                if (gen_float_width(arithmetic_type) == 4) {
                    emit_push_reg(mod, EAX);
                } else {
                    emit_push_reg(mod, EDX);
                    emit_push_reg(mod, EAX);
                }
                gen_expr_as_type(mod, expr->binary_rhs, arithmetic_type);
                if (gen_float_width(arithmetic_type) == 4) {
                    emit_push_reg(mod, EAX);
                } else {
                    emit_push_reg(mod, EDX);
                    emit_push_reg(mod, EAX);
                }
                emit_x87_binary_stack(mod, arithmetic_type, EXPR_ADD);
                break;
            }
            gen_expr(mod, expr->binary_lhs);
            emit_push_reg(mod, EAX);
            gen_expr(mod, expr->binary_rhs);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_pop_reg(mod, EAX);
            if (codegen_pointer_element_size(expr->binary_lhs->type) != 0u &&
                type_is_integer(expr->binary_rhs->type)) {
                emit_scale_reg(
                    mod, ECX,
                    codegen_pointer_element_size(expr->binary_lhs->type));
            } else if (type_is_integer(expr->binary_lhs->type) &&
                       codegen_pointer_element_size(
                           expr->binary_rhs->type) != 0u) {
                emit_scale_reg(
                    mod, EAX,
                    codegen_pointer_element_size(expr->binary_rhs->type));
            }
            emit_add_reg_reg(mod, EAX, ECX);
            break;

        case EXPR_SUB:
            if (gen_is_floating(expr->type)) {
                Type* arithmetic_type = expr->type;
                gen_expr_as_type(mod, expr->binary_lhs, arithmetic_type);
                if (gen_float_width(arithmetic_type) == 4) {
                    emit_push_reg(mod, EAX);
                } else {
                    emit_push_reg(mod, EDX);
                    emit_push_reg(mod, EAX);
                }
                gen_expr_as_type(mod, expr->binary_rhs, arithmetic_type);
                if (gen_float_width(arithmetic_type) == 4) {
                    emit_push_reg(mod, EAX);
                } else {
                    emit_push_reg(mod, EDX);
                    emit_push_reg(mod, EAX);
                }
                emit_x87_binary_stack(mod, arithmetic_type, EXPR_SUB);
                break;
            }
            gen_expr(mod, expr->binary_lhs);
            emit_push_reg(mod, EAX);
            gen_expr(mod, expr->binary_rhs);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_pop_reg(mod, EAX);
            if (codegen_pointer_element_size(expr->binary_lhs->type) != 0u &&
                type_is_integer(expr->binary_rhs->type)) {
                emit_scale_reg(
                    mod, ECX,
                    codegen_pointer_element_size(expr->binary_lhs->type));
            }
            emit_sub_reg_reg(mod, EAX, ECX);
            if (codegen_pointer_element_size(expr->binary_lhs->type) != 0u &&
                codegen_pointer_element_size(expr->binary_rhs->type) != 0u) {
                uint32_t element_size = codegen_pointer_element_size(
                    expr->binary_lhs->type);
                if (element_size > 1u) {
                    emit_mov_reg_imm(mod, ECX, element_size);
                    emit_cdq(mod);
                    emit_idiv_reg(mod, ECX);
                }
            }
            break;

        case EXPR_MUL:
            if (gen_is_floating(expr->type)) {
                Type* arithmetic_type = expr->type;
                gen_expr_as_type(mod, expr->binary_lhs, arithmetic_type);
                if (gen_float_width(arithmetic_type) == 4) {
                    emit_push_reg(mod, EAX);
                } else {
                    emit_push_reg(mod, EDX);
                    emit_push_reg(mod, EAX);
                }
                gen_expr_as_type(mod, expr->binary_rhs, arithmetic_type);
                if (gen_float_width(arithmetic_type) == 4) {
                    emit_push_reg(mod, EAX);
                } else {
                    emit_push_reg(mod, EDX);
                    emit_push_reg(mod, EAX);
                }
                emit_x87_binary_stack(mod, arithmetic_type, EXPR_MUL);
                break;
            }
            gen_expr(mod, expr->binary_lhs);
            emit_push_reg(mod, EAX);
            gen_expr(mod, expr->binary_rhs);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_pop_reg(mod, EAX);
            emit_imul_reg_reg(mod, EAX, ECX);
            break;

        case EXPR_DIV:
        case EXPR_MOD:
            if (expr->kind == EXPR_DIV && gen_is_floating(expr->type)) {
                Type* arithmetic_type = expr->type;
                gen_expr_as_type(mod, expr->binary_lhs, arithmetic_type);
                if (gen_float_width(arithmetic_type) == 4) {
                    emit_push_reg(mod, EAX);
                } else {
                    emit_push_reg(mod, EDX);
                    emit_push_reg(mod, EAX);
                }
                gen_expr_as_type(mod, expr->binary_rhs, arithmetic_type);
                if (gen_float_width(arithmetic_type) == 4) {
                    emit_push_reg(mod, EAX);
                } else {
                    emit_push_reg(mod, EDX);
                    emit_push_reg(mod, EAX);
                }
                emit_x87_binary_stack(mod, arithmetic_type, EXPR_DIV);
                break;
            }
            gen_expr(mod, expr->binary_lhs);
            emit_push_reg(mod, EAX);
            gen_expr(mod, expr->binary_rhs);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_pop_reg(mod, EAX);
            if (expr->type && expr->type->is_unsigned) {
                emit_xor_reg_reg(mod, EDX, EDX);
                emit_div_reg(mod, ECX);
            } else {
                emit_cdq(mod);
                emit_idiv_reg(mod, ECX);
            }
            if (expr->kind == EXPR_MOD) {
                emit_mov_reg_reg(mod, EAX, EDX);
            }
            break;

        case EXPR_BITAND:
            gen_expr(mod, expr->binary_lhs);
            emit_push_reg(mod, EAX);
            gen_expr(mod, expr->binary_rhs);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_pop_reg(mod, EAX);
            emit_and_reg_reg(mod, EAX, ECX);
            break;

        case EXPR_BITOR:
            gen_expr(mod, expr->binary_lhs);
            emit_push_reg(mod, EAX);
            gen_expr(mod, expr->binary_rhs);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_pop_reg(mod, EAX);
            emit_or_reg_reg(mod, EAX, ECX);
            break;

        case EXPR_BITXOR:
            gen_expr(mod, expr->binary_lhs);
            emit_push_reg(mod, EAX);
            gen_expr(mod, expr->binary_rhs);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_pop_reg(mod, EAX);
            emit_xor_reg_reg(mod, EAX, ECX);
            break;

        case EXPR_LSHIFT:
            gen_expr(mod, expr->binary_lhs);
            emit_push_reg(mod, EAX);
            gen_expr(mod, expr->binary_rhs);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_pop_reg(mod, EAX);
            emit_shl_reg_cl(mod, EAX);
            break;

        case EXPR_RSHIFT:
            gen_expr(mod, expr->binary_lhs);
            emit_push_reg(mod, EAX);
            gen_expr(mod, expr->binary_rhs);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_pop_reg(mod, EAX);
            if (expr->type && expr->type->is_unsigned) {
                emit_shr_reg_cl(mod, EAX);
            } else {
                emit_sar_reg_cl(mod, EAX);
            }
            break;

        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_GT:
        case EXPR_LE:
        case EXPR_GE: {
            if (gen_is_floating(expr->binary_lhs->type) ||
                gen_is_floating(expr->binary_rhs->type)) {
                Type* comparison_type = type_common(
                    expr->binary_lhs->type, expr->binary_rhs->type);
                gen_expr_as_type(mod, expr->binary_lhs, comparison_type);
                if (gen_float_width(comparison_type) == 4) {
                    emit_push_reg(mod, EAX);
                } else {
                    emit_push_reg(mod, EDX);
                    emit_push_reg(mod, EAX);
                }
                gen_expr_as_type(mod, expr->binary_rhs, comparison_type);
                if (gen_float_width(comparison_type) == 4) {
                    emit_push_reg(mod, EAX);
                } else {
                    emit_push_reg(mod, EDX);
                    emit_push_reg(mod, EAX);
                }
                emit_x87_compare_stack(mod, comparison_type, expr->kind);
                break;
            }
            if (gen_is_integer64(expr->binary_lhs->type) ||
                gen_is_integer64(expr->binary_rhs->type)) {
                gen_compare_integer64(mod, expr);
                break;
            }
            gen_expr(mod, expr->binary_lhs);
            emit_push_reg(mod, EAX);
            gen_expr(mod, expr->binary_rhs);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_pop_reg(mod, EAX);
            emit_cmp_reg_reg(mod, EAX, ECX);

            int cc;
            Type* comparison_type = codegen_comparison_type(expr);
            bool unsigned_compare = comparison_type &&
                                    comparison_type->is_unsigned;
            switch (expr->kind) {
                case EXPR_EQ: cc = CC_E; break;
                case EXPR_NE: cc = CC_NE; break;
                case EXPR_LT: cc = unsigned_compare ? CC_B : CC_L; break;
                case EXPR_GT: cc = unsigned_compare ? CC_A : CC_G; break;
                case EXPR_LE: cc = unsigned_compare ? CC_BE : CC_LE; break;
                case EXPR_GE: cc = unsigned_compare ? CC_AE : CC_GE; break;
                default: cc = CC_E; break;
            }

            emit_setcc(mod, cc, EAX);
            emit_byte(mod, 0x0F);  /* MOVZX EAX, AL */
            emit_byte(mod, 0xB6);
            emit_byte(mod, modrm(3, EAX, EAX));
            break;
        }

        case EXPR_AND: {
            int end_label = new_label();
            gen_expr(mod, expr->binary_lhs);
            emit_test_scalar_value(mod, expr->binary_lhs->type);
            emit_jcc_label(mod, CC_E, end_label);
            gen_expr(mod, expr->binary_rhs);
            emit_test_scalar_value(mod, expr->binary_rhs->type);
            emit_setcc(mod, CC_NE, EAX);
            emit_byte(mod, 0x0F);
            emit_byte(mod, 0xB6);
            emit_byte(mod, modrm(3, EAX, EAX));
            emit_label(mod, end_label);
            break;
        }

        case EXPR_OR: {
            int true_label = new_label();
            int end_label = new_label();
            gen_expr(mod, expr->binary_lhs);
            emit_test_scalar_value(mod, expr->binary_lhs->type);
            emit_jcc_label(mod, CC_NE, true_label);
            gen_expr(mod, expr->binary_rhs);
            emit_test_scalar_value(mod, expr->binary_rhs->type);
            emit_setcc(mod, CC_NE, EAX);
            emit_byte(mod, 0x0F);
            emit_byte(mod, 0xB6);
            emit_byte(mod, modrm(3, EAX, EAX));
            emit_jmp_label(mod, end_label);
            emit_label(mod, true_label);
            emit_mov_reg_imm(mod, EAX, 1u);
            emit_label(mod, end_label);
            break;
        }

        case EXPR_ASSIGN:
            if (gen_cxx_move_assignment(mod, expr)) break;
            if (expr->binary_lhs && expr->binary_lhs->member_field &&
                expr->binary_lhs->member_field->is_bitfield) {
                gen_expr(mod, expr->binary_rhs);
                if (type_is_integer(expr->binary_lhs->type) ||
                    expr->binary_lhs->type->kind == TYPE_ENUM) {
                    emit_convert_integer_value(mod, EAX,
                                               expr->binary_rhs->type,
                                               expr->binary_lhs->type);
                }
                emit_mov_reg_reg(mod, ECX, EAX);
                gen_lvalue(mod, expr->binary_lhs);
                emit_mov_reg_reg(mod, EDX, EAX);
                emit_bitfield_store32(mod, expr->binary_lhs->member_field);
                break;
            }
            if (gen_is_floating(expr->binary_lhs->type)) {
                Type* type = expr->binary_lhs->type;
                gen_expr_as_type(mod, expr->binary_rhs, type);
                if (gen_float_width(type) == 4) {
                    emit_push_reg(mod, EAX);
                    gen_lvalue(mod, expr->binary_lhs);
                    emit_pop_reg(mod, ECX);
                    emit_mov_mem_reg(mod, EAX, 0, ECX);
                    emit_mov_reg_reg(mod, EAX, ECX);
                } else {
                    emit_push_reg(mod, EDX);
                    emit_push_reg(mod, EAX);
                    gen_lvalue(mod, expr->binary_lhs);
                    emit_mov_reg_reg(mod, ECX, EAX);
                    emit_pop_reg(mod, EAX);
                    emit_pop_reg(mod, EDX);
                    emit_store_floating_raw(mod, type, ECX, 0);
                }
                break;
            }
            if (expr->binary_lhs->type &&
                (expr->binary_lhs->type->kind == TYPE_STRUCT ||
                 expr->binary_lhs->type->kind == TYPE_UNION)) {
                int offset = 0;
                if (expr->binary_rhs->kind == EXPR_ASSIGN ||
                    expr->binary_rhs->kind == EXPR_VA_ARG) {
                    gen_expr(mod, expr->binary_rhs);
                } else {
                    gen_lvalue(mod, expr->binary_rhs);
                }
                emit_push_reg(mod, EAX);
                gen_lvalue(mod, expr->binary_lhs);
                emit_mov_reg_reg(mod, EDX, EAX);
                emit_pop_reg(mod, ECX);
                while (offset + 4 <= expr->binary_lhs->type->size) {
                    emit_mov_reg_mem(mod, EAX, ECX, offset);
                    emit_mov_mem_reg(mod, EDX, offset, EAX);
                    offset += 4;
                }
                while (offset < expr->binary_lhs->type->size) {
                    emit_load_typed32(mod, EAX, ECX, offset, type_uchar);
                    emit_store_typed32(mod, EDX, offset, EAX, type_uchar);
                    ++offset;
                }
                emit_mov_reg_reg(mod, EAX, EDX);
                break;
            }
            gen_expr(mod, expr->binary_rhs);
            if (type_is_integer(expr->binary_lhs->type) ||
                expr->binary_lhs->type->kind == TYPE_ENUM) {
                emit_convert_integer_value(mod, EAX,
                                           expr->binary_rhs->type,
                                           expr->binary_lhs->type);
            }
            emit_push_reg(mod, EAX);
            gen_lvalue(mod, expr->binary_lhs);
            emit_pop_reg(mod, ECX);
            if (type_is_integer(expr->binary_lhs->type) ||
                expr->binary_lhs->type->kind == TYPE_ENUM) {
                emit_normalize_atomic_value(mod, ECX,
                                            expr->binary_lhs->type);
            }
            emit_store_typed32(mod, EAX, 0, ECX,
                               expr->binary_lhs->type);
            emit_mov_reg_reg(mod, EAX, ECX);
            break;

        case EXPR_ADD_ASSIGN:
        case EXPR_SUB_ASSIGN: {
            if (expr->binary_lhs && expr->binary_lhs->member_field &&
                expr->binary_lhs->member_field->is_bitfield) {
                gen_lvalue(mod, expr->binary_lhs);
                emit_push_reg(mod, EAX);
                gen_bitfield_load32(mod, expr->binary_lhs->member_field);
                emit_push_reg(mod, EAX);
                gen_expr(mod, expr->binary_rhs);
                emit_mov_reg_reg(mod, EDX, EAX);
                emit_pop_reg(mod, EAX);
                if (expr->kind == EXPR_ADD_ASSIGN) {
                    emit_add_reg_reg(mod, EAX, EDX);
                } else {
                    emit_sub_reg_reg(mod, EAX, EDX);
                }
                emit_pop_reg(mod, EDX);
                emit_mov_reg_reg(mod, ECX, EAX);
                emit_bitfield_store32(mod, expr->binary_lhs->member_field);
                break;
            }
            if (gen_is_floating(expr->binary_lhs->type)) {
                gen_floating_compound_assignment(
                    mod, expr,
                    expr->kind == EXPR_ADD_ASSIGN ? EXPR_ADD : EXPR_SUB);
                break;
            }
            uint32_t scale = codegen_pointer_element_size(
                expr->binary_lhs->type);
            gen_lvalue(mod, expr->binary_lhs);
            emit_push_reg(mod, EAX);
            emit_load_typed32(mod, EAX, EAX, 0,
                              expr->binary_lhs->type);
            emit_push_reg(mod, EAX);
            gen_expr(mod, expr->binary_rhs);
            emit_scale_reg(mod, EAX, scale);
            emit_mov_reg_reg(mod, EDX, EAX);
            emit_pop_reg(mod, EAX);
            if (expr->kind == EXPR_ADD_ASSIGN) {
                emit_add_reg_reg(mod, EAX, EDX);
            } else {
                emit_sub_reg_reg(mod, EAX, EDX);
            }
            emit_normalize_atomic_value(mod, EAX,
                                        expr->binary_lhs->type);
            emit_pop_reg(mod, ECX);
            emit_store_typed32(mod, ECX, 0, EAX,
                               expr->binary_lhs->type);
            break;
        }

        case EXPR_MUL_ASSIGN:
        case EXPR_DIV_ASSIGN:
        case EXPR_MOD_ASSIGN:
        case EXPR_AND_ASSIGN:
        case EXPR_OR_ASSIGN:
        case EXPR_XOR_ASSIGN:
        case EXPR_LSHIFT_ASSIGN:
        case EXPR_RSHIFT_ASSIGN: {
            Type* operation_type = type_common(expr->binary_lhs->type,
                                               expr->binary_rhs->type);
            if (expr->binary_lhs && expr->binary_lhs->member_field &&
                expr->binary_lhs->member_field->is_bitfield) {
                if (gen_is_integer64(operation_type)) {
                    rcc_error(expr->loc,
                              "64-bit compound operation on a bit-field is not supported");
                    break;
                }
                gen_lvalue(mod, expr->binary_lhs);
                emit_push_reg(mod, EAX);
                gen_bitfield_load32(mod, expr->binary_lhs->member_field);
                emit_push_reg(mod, EAX);
                gen_expr(mod, expr->binary_rhs);
                emit_mov_reg_reg(mod, ECX, EAX);
                emit_pop_reg(mod, EAX);
                if (expr->kind == EXPR_MUL_ASSIGN) {
                    emit_imul_reg_reg(mod, EAX, ECX);
                } else if (expr->kind == EXPR_DIV_ASSIGN ||
                           expr->kind == EXPR_MOD_ASSIGN) {
                    if (operation_type && operation_type->is_unsigned) {
                        emit_xor_reg_reg(mod, EDX, EDX);
                        emit_div_reg(mod, ECX);
                    } else {
                        emit_cdq(mod);
                        emit_idiv_reg(mod, ECX);
                    }
                    if (expr->kind == EXPR_MOD_ASSIGN) {
                        emit_mov_reg_reg(mod, EAX, EDX);
                    }
                } else if (expr->kind == EXPR_AND_ASSIGN) {
                    emit_and_reg_reg(mod, EAX, ECX);
                } else if (expr->kind == EXPR_OR_ASSIGN) {
                    emit_or_reg_reg(mod, EAX, ECX);
                } else if (expr->kind == EXPR_XOR_ASSIGN) {
                    emit_xor_reg_reg(mod, EAX, ECX);
                } else if (expr->kind == EXPR_LSHIFT_ASSIGN) {
                    emit_shl_reg_cl(mod, EAX);
                } else if (expr->binary_lhs->type &&
                           expr->binary_lhs->type->is_unsigned) {
                    emit_shr_reg_cl(mod, EAX);
                } else {
                    emit_sar_reg_cl(mod, EAX);
                }
                emit_pop_reg(mod, EDX);
                emit_mov_reg_reg(mod, ECX, EAX);
                emit_bitfield_store32(mod, expr->binary_lhs->member_field);
                break;
            }
            if (gen_is_floating(expr->binary_lhs->type)) {
                if (expr->kind == EXPR_MUL_ASSIGN ||
                    expr->kind == EXPR_DIV_ASSIGN) {
                    gen_floating_compound_assignment(
                        mod, expr,
                        expr->kind == EXPR_MUL_ASSIGN ? EXPR_MUL : EXPR_DIV);
                } else {
                    rcc_error(expr->loc,
                              "invalid floating compound assignment");
                }
                break;
            }
            if ((expr->kind == EXPR_DIV_ASSIGN ||
                 expr->kind == EXPR_MOD_ASSIGN) &&
                gen_is_integer64(operation_type)) {
                gen_lvalue(mod, expr->binary_lhs);
                emit_push_reg(mod, EAX);
                emit_mov_reg_reg(mod, ECX, EAX);
                emit_load_typed32(mod, EAX, ECX, 0,
                                  expr->binary_lhs->type);
                emit_extend_eax_to_integer64(mod,
                                             expr->binary_lhs->type);
                gen_divmod_integer64(mod, NULL, expr->binary_rhs,
                                     operation_type,
                                     expr->kind == EXPR_MOD_ASSIGN);
                emit_normalize_atomic_value(mod, EAX,
                                            expr->binary_lhs->type);
                emit_pop_reg(mod, ECX);
                emit_store_typed32(mod, ECX, 0, EAX,
                                   expr->binary_lhs->type);
                break;
            }
            gen_lvalue(mod, expr->binary_lhs);
            emit_push_reg(mod, EAX);
            emit_load_typed32(mod, EAX, EAX, 0,
                              expr->binary_lhs->type);
            emit_push_reg(mod, EAX);
            gen_expr(mod, expr->binary_rhs);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_pop_reg(mod, EAX);
            if (expr->kind == EXPR_MUL_ASSIGN) {
                emit_imul_reg_reg(mod, EAX, ECX);
            } else if (expr->kind == EXPR_DIV_ASSIGN ||
                       expr->kind == EXPR_MOD_ASSIGN) {
                if (operation_type && operation_type->is_unsigned) {
                    emit_xor_reg_reg(mod, EDX, EDX);
                    emit_div_reg(mod, ECX);
                } else {
                    emit_cdq(mod);
                    emit_idiv_reg(mod, ECX);
                }
                if (expr->kind == EXPR_MOD_ASSIGN) {
                    emit_mov_reg_reg(mod, EAX, EDX);
                }
            } else if (expr->kind == EXPR_AND_ASSIGN) {
                emit_and_reg_reg(mod, EAX, ECX);
            } else if (expr->kind == EXPR_OR_ASSIGN) {
                emit_or_reg_reg(mod, EAX, ECX);
            } else if (expr->kind == EXPR_XOR_ASSIGN) {
                emit_xor_reg_reg(mod, EAX, ECX);
            } else if (expr->kind == EXPR_LSHIFT_ASSIGN) {
                emit_shl_reg_cl(mod, EAX);
            } else if (expr->binary_lhs->type &&
                       expr->binary_lhs->type->is_unsigned) {
                emit_shr_reg_cl(mod, EAX);
            } else {
                emit_sar_reg_cl(mod, EAX);
            }
            emit_normalize_atomic_value(mod, EAX,
                                        expr->binary_lhs->type);
            emit_pop_reg(mod, ECX);
            emit_store_typed32(mod, ECX, 0, EAX,
                               expr->binary_lhs->type);
            break;
        }

        case EXPR_COND: {
            int else_label = new_label();
            int end_label = new_label();
            gen_expr(mod, expr->cond_test);
            emit_test_scalar_value(mod, expr->cond_test->type);
            emit_jcc_label(mod, CC_E, else_label);
            gen_expr(mod, expr->cond_then);
            emit_jmp_label(mod, end_label);
            emit_label(mod, else_label);
            gen_expr(mod, expr->cond_else);
            emit_label(mod, end_label);
            break;
        }

        case EXPR_CALL: {
            gen_call(mod, expr);
            break;
        }

        case EXPR_VA_START: {
            Decl* last = expr->va_second_operand
                ? expr->va_second_operand->ident_decl : NULL;
            int size = last && last->type && last->type->size > 0
                ? last->type->size : 4;
            int offset = last ? last->var_offset + ((size + 3) & ~3) : 0;
            gen_lvalue(mod, expr->va_list_operand);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_mov_reg_reg(mod, EAX, EBP);
            emit_add_reg_imm(mod, EAX, offset);
            emit_mov_mem_reg(mod, ECX, 0, EAX);
            emit_mov_reg_imm(mod, EAX, 0u);
            break;
        }

        case EXPR_VA_END:
            emit_mov_reg_imm(mod, EAX, 0u);
            break;

        case EXPR_VA_COPY:
            gen_expr(mod, expr->va_second_operand);
            emit_push_reg(mod, EAX);
            gen_lvalue(mod, expr->va_list_operand);
            emit_pop_reg(mod, ECX);
            emit_mov_mem_reg(mod, EAX, 0, ECX);
            emit_mov_reg_imm(mod, EAX, 0u);
            break;

        case EXPR_VA_ARG: {
            if (expr->va_arg_type &&
                (expr->va_arg_type->kind == TYPE_STRUCT ||
                 expr->va_arg_type->kind == TYPE_UNION)) {
                int size = expr->va_arg_type->size;
                int step = (size + 3) & ~3;
                gen_lvalue(mod, expr->va_list_operand);
                emit_mov_reg_reg(mod, ECX, EAX);
                emit_mov_reg_mem(mod, EAX, ECX, 0);
                emit_mov_reg_reg(mod, EDX, EAX);
                emit_add_reg_imm(mod, EAX, step);
                emit_mov_mem_reg(mod, ECX, 0, EAX);
                emit_byte(mod, 0x8D);  /* LEA EAX, [EBP+disp32] */
                emit_byte(mod, modrm(2, EAX, EBP));
                emit_dword(mod, (uint32_t)expr->va_arg_result_offset);
                emit_mov_reg_reg(mod, ECX, EAX);
                for (int offset = 0; offset < size; ++offset) {
                    emit_load_typed32(mod, EAX, EDX, offset, type_uchar);
                    emit_store_typed32(mod, ECX, offset, EAX, type_uchar);
                }
                emit_mov_reg_reg(mod, EAX, ECX);
                break;
            }
            int step = (expr->va_arg_type->size + 3) & ~3;
            gen_lvalue(mod, expr->va_list_operand);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_mov_reg_mem(mod, EAX, ECX, 0);
            emit_mov_reg_reg(mod, EDX, EAX);
            emit_add_reg_imm(mod, EAX, step);
            emit_mov_mem_reg(mod, ECX, 0, EAX);
            if (gen_is_floating(expr->va_arg_type)) {
                emit_load_floating_raw(mod, expr->va_arg_type, EDX, 0);
            } else {
                emit_load_typed32(mod, EAX, EDX, 0, expr->va_arg_type);
            }
            break;
        }

        case EXPR_INDEX:
            gen_lvalue(mod, expr);
            if (!expr->type || expr->type->kind != TYPE_ARRAY) {
                if (gen_is_floating(expr->type)) {
                    emit_load_floating_raw(mod, expr->type, EAX, 0);
                } else {
                    emit_load_typed32(mod, EAX, EAX, 0, expr->type);
                }
            }
            break;

        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            gen_lvalue(mod, expr);
            if (expr->member_field && expr->member_field->is_bitfield) {
                gen_bitfield_load32(mod, expr->member_field);
            } else if (!expr->type || expr->type->kind != TYPE_ARRAY) {
                if (gen_is_floating(expr->type)) {
                    emit_load_floating_raw(mod, expr->type, EAX, 0);
                } else {
                    emit_load_typed32(mod, EAX, EAX, 0, expr->type);
                }
            }
            break;

        case EXPR_COMPOUND:
            gen_lvalue(mod, expr);
            if (!expr->type || (expr->type->kind != TYPE_ARRAY &&
                                expr->type->kind != TYPE_STRUCT &&
                                expr->type->kind != TYPE_UNION)) {
                if (gen_is_floating(expr->type)) {
                    emit_load_floating_raw(mod, expr->type, EAX, 0);
                } else {
                    emit_load_typed32(mod, EAX, EAX, 0, expr->type);
                }
            }
            break;

        case EXPR_CAST:
            if (gen_is_floating(expr->type) ||
                gen_is_floating(expr->cast_expr->type)) {
                gen_expr_as_type(mod, expr->cast_expr, expr->type);
            } else {
                gen_expr(mod, expr->cast_expr);
            }
            if (!gen_is_floating(expr->type) &&
                (type_is_integer(expr->type) ||
                 expr->type->kind == TYPE_ENUM) &&
                !gen_is_floating(expr->cast_expr->type)) {
                emit_convert_integer_value(mod, EAX,
                                           expr->cast_expr->type,
                                           expr->type);
            }
            break;

        case EXPR_SIZEOF:
            if (expr->sizeof_type && codegen_type_has_vla(expr->sizeof_type)) {
                gen_vla_extent(mod, expr->sizeof_type);
            } else if (expr->unary_operand &&
                       expr->unary_operand->kind == EXPR_IDENT &&
                       expr->unary_operand->ident_decl &&
                       expr->unary_operand->ident_decl->var_is_vla) {
                emit_mov_reg_mem(mod, EAX, EBP,
                                 expr->unary_operand->ident_decl->var_vla_size_offset);
            } else if (expr->unary_operand &&
                       codegen_type_has_vla(expr->unary_operand->type)) {
                gen_vla_extent_for_expr(mod, expr->unary_operand->type,
                                        expr->unary_operand);
            } else if (expr->sizeof_type) {
                emit_mov_reg_imm(mod, EAX, expr->sizeof_type->size);
            } else if (expr->unary_operand && expr->unary_operand->type) {
                emit_mov_reg_imm(mod, EAX, expr->unary_operand->type->size);
            } else {
                emit_mov_reg_imm(mod, EAX, 4);
            }
            break;

        case EXPR_ALIGNOF:
            emit_mov_reg_imm(mod, EAX,
                             expr->sizeof_type ? expr->sizeof_type->align : 1);
            break;

        case EXPR_COMMA:
            gen_expr(mod, expr->binary_lhs);
            gen_expr(mod, expr->binary_rhs);
            break;

        default:
            rcc_error(expr ? expr->loc : (SourceLoc){"<expr>", 0, 0},
                      "unsupported expression kind in i686 code generation");
            return;
    }
}

/* ═══════════════════════════════════════
 * Inline Assembly Code Generation
 * ═══════════════════════════════════════ */

/* Parse constraint to get register number, -1 if memory/unknown */
static int constraint_to_reg(const char* constraint) {
    /* Skip modifier characters */
    while (*constraint == '=' || *constraint == '+' || *constraint == '&') {
        constraint++;
    }

    switch (*constraint) {
        case 'a': return EAX;
        case 'b': return EBX;
        case 'c': return ECX;
        case 'd': return EDX;
        case 'S': return ESI;
        case 'D': return EDI;
        case 'r': return -2;  /* Any register */
        case 'm': return -1;  /* Memory */
        default: return -1;
    }
}

static bool asm_parse_immediate8(const char* text, uint8_t* value) {
    char* end;
    long long parsed;

    if (!text || text[0] != '$' || !text[1] || !value) return false;
    parsed = strtoll(text + 1, &end, 0);
    if (end == text + 1 || *end != '\0' || parsed < -128 || parsed > 255) {
        return false;
    }
    *value = (uint8_t)parsed;
    return true;
}

static bool asm_no_operands(const char* op1, const char* op2) {
    return !op1 && !op2;
}

/* Encode a single x86 instruction from mnemonic and operands */
static bool emit_asm_instruction(Module* mod, const char* mnemonic,
                                  const char* op1, const char* op2) {
    uint8_t immediate;

    /* Common instructions used in syscall/interrupt context */
    if (strcmp(mnemonic, "int") == 0) {
        if (op2 || !asm_parse_immediate8(op1, &immediate)) return false;
        emit_byte(mod, 0xCD);
        emit_byte(mod, immediate);
        return true;
    }
    else if (strcmp(mnemonic, "int3") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0xCC);
        return true;
    }
    else if (strcmp(mnemonic, "syscall") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0x05);
        return true;
    }
    else if (strcmp(mnemonic, "sysenter") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0x34);
        return true;
    }
    else if (strcmp(mnemonic, "nop") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0x90);
        return true;
    }
    else if (strcmp(mnemonic, "hlt") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0xF4);
        return true;
    }
    else if (strcmp(mnemonic, "cli") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0xFA);
        return true;
    }
    else if (strcmp(mnemonic, "sti") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0xFB);
        return true;
    }
    else if (strcmp(mnemonic, "cld") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0xFC);
        return true;
    }
    else if (strcmp(mnemonic, "std") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0xFD);
        return true;
    }
    else if (strcmp(mnemonic, "pushf") == 0 || strcmp(mnemonic, "pushfl") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0x9C);
        return true;
    }
    else if (strcmp(mnemonic, "popf") == 0 || strcmp(mnemonic, "popfl") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0x9D);
        return true;
    }
    else if (strcmp(mnemonic, "ret") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0xC3);
        return true;
    }
    else if (strcmp(mnemonic, "leave") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0xC9);
        return true;
    }
    else if (strcmp(mnemonic, "cpuid") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xA2);
        return true;
    }
    else if (strcmp(mnemonic, "rdtsc") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0x31);
        return true;
    }
    else if (strcmp(mnemonic, "rdmsr") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0x32);
        return true;
    }
    else if (strcmp(mnemonic, "wrmsr") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0x30);
        return true;
    }
    else if (strcmp(mnemonic, "invlpg") == 0) {
        if (op2 || !op1 ||
            (strcmp(op1, "(%eax)") != 0 && strcmp(op1, "[eax]") != 0)) {
            return false;
        }
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0x01);
        emit_byte(mod, 0x38);
        return true;
    }
    else if (strcmp(mnemonic, "wbinvd") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0x09);
        return true;
    }
    else if (strcmp(mnemonic, "pause") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0xF3);
        emit_byte(mod, 0x90);
        return true;
    }
    else if (strcmp(mnemonic, "mfence") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xAE);
        emit_byte(mod, 0xF0);
        return true;
    }
    else if (strcmp(mnemonic, "lfence") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xAE);
        emit_byte(mod, 0xE8);
        return true;
    }
    else if (strcmp(mnemonic, "sfence") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xAE);
        emit_byte(mod, 0xF8);
        return true;
    }
    return false;
}

/* Parse and emit inline assembly */
static void gen_asm_stmt(Module* mod, Stmt* stmt) {
    const char* tmpl = stmt->asm_template;
    AsmOperand* outputs = stmt->asm_outputs;
    AsmOperand* inputs = stmt->asm_inputs;
    AsmClobber* clobbers = stmt->asm_clobbers;

    /* Count operands for %0, %1, etc. references */
    int output_count = 0;
    int input_count = 0;
    for (AsmOperand* op = outputs; op; op = op->next) output_count++;
    for (AsmOperand* op = inputs; op; op = op->next) input_count++;
    int total_operands = output_count + input_count;

    /* Build operand info arrays */
    typedef struct {
        int reg;           /* Register number or -1 for memory */
        int stack_offset;  /* Stack offset for memory operands */
        AsmOperand* op;
    } OperandInfo;

    OperandInfo* operands = NULL;
    if (total_operands > 0) {
        operands = rcc_alloc(total_operands * sizeof(OperandInfo));

        int idx = 0;
        /* Process outputs first */
        for (AsmOperand* op = outputs; op; op = op->next) {
            operands[idx].reg = constraint_to_reg(op->constraint);
            operands[idx].op = op;
            operands[idx].stack_offset = 0;
            idx++;
        }
        /* Then inputs */
        for (AsmOperand* op = inputs; op; op = op->next) {
            operands[idx].reg = constraint_to_reg(op->constraint);
            operands[idx].op = op;
            operands[idx].stack_offset = 0;
            idx++;
        }
    }

    /* Preserve the i386 SysV callee-saved registers that fixed constraints
     * may select.  The generated function prologue does not otherwise touch
     * them. */
    bool preserve_ebx = false;
    bool preserve_esi = false;
    bool preserve_edi = false;
    for (int i = output_count; i < total_operands; i++) {
        preserve_ebx = preserve_ebx || operands[i].reg == EBX;
        preserve_esi = preserve_esi || operands[i].reg == ESI;
        preserve_edi = preserve_edi || operands[i].reg == EDI;
    }
    for (AsmClobber* clobber = clobbers; clobber;
         clobber = clobber->next) {
        preserve_ebx = preserve_ebx ||
            strcmp(clobber->reg, "ebx") == 0 ||
            strcmp(clobber->reg, "bx") == 0;
        preserve_esi = preserve_esi ||
            strcmp(clobber->reg, "esi") == 0 ||
            strcmp(clobber->reg, "si") == 0;
        preserve_edi = preserve_edi ||
            strcmp(clobber->reg, "edi") == 0 ||
            strcmp(clobber->reg, "di") == 0;
    }
    if (preserve_ebx) emit_push_reg(mod, EBX);
    if (preserve_esi) emit_push_reg(mod, ESI);
    if (preserve_edi) emit_push_reg(mod, EDI);

    /* Evaluate every input before assigning fixed registers.  gen_expr uses
     * EAX as its result, so assigning "a" eagerly would let a later operand
     * silently overwrite the syscall number. */
    for (int i = 0; i < output_count; i++) {
        if (operands[i].op->constraint[0] == '+' &&
            operands[i].reg >= 0 && operands[i].op->expr) {
            gen_expr(mod, operands[i].op->expr);
            emit_push_reg(mod, EAX);
        }
    }
    for (int i = output_count; i < total_operands; i++) {
        if (operands[i].reg >= 0 && operands[i].op->expr) {
            gen_expr(mod, operands[i].op->expr);
            emit_push_reg(mod, EAX);
        }
    }
    for (int i = total_operands - 1; i >= output_count; i--) {
        if (operands[i].reg >= 0 && operands[i].op->expr) {
            emit_pop_reg(mod, operands[i].reg);
        }
    }
    for (int i = output_count - 1; i >= 0; i--) {
        if (operands[i].op->constraint[0] == '+' &&
            operands[i].reg >= 0 && operands[i].op->expr) {
            emit_pop_reg(mod, operands[i].reg);
        }
    }

    /* Parse and emit assembly template */
    /* Simple parser: split by ';' or '\n', then parse each instruction */
    char* tmpl_copy = rcc_strdup(tmpl);
    char* p = tmpl_copy;

    while (*p) {
        /* Skip whitespace */
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        if (!*p) break;

        /* Find end of instruction (semicolon, newline, or end) */
        char* instr_start = p;
        while (*p && *p != ';' && *p != '\n') p++;

        char saved = *p;
        *p = '\0';

        /* Parse instruction: mnemonic [op1 [, op2]] */
        char* instr = instr_start;

        /* Skip leading whitespace */
        while (*instr == ' ' || *instr == '\t') instr++;

        if (*instr) {
            char mnemonic[32] = {0};
            char op1[64] = {0};
            char op2[64] = {0};

            /* Extract mnemonic */
            int mi = 0;
            while (*instr && *instr != ' ' && *instr != '\t' && mi < 31) {
                mnemonic[mi++] = *instr++;
            }
            mnemonic[mi] = '\0';

            /* Skip whitespace */
            while (*instr == ' ' || *instr == '\t') instr++;

            /* Extract first operand */
            if (*instr) {
                int oi = 0;
                while (*instr && *instr != ',' && oi < 63) {
                    if (*instr != ' ' && *instr != '\t') {
                        op1[oi++] = *instr;
                    }
                    instr++;
                }
                op1[oi] = '\0';

                if (*instr == ',') {
                    instr++;
                    while (*instr == ' ' || *instr == '\t') instr++;

                    /* Extract second operand */
                    oi = 0;
                    while (*instr && oi < 63) {
                        if (*instr != ' ' && *instr != '\t') {
                            op2[oi++] = *instr;
                        }
                        instr++;
                    }
                    op2[oi] = '\0';
                }
            }

            /* Emit the instruction */
            if (!emit_asm_instruction(mod, mnemonic,
                                       op1[0] ? op1 : NULL,
                                       op2[0] ? op2 : NULL)) {
                rcc_error(stmt->loc,
                          "unsupported i686 inline asm instruction '%s'",
                          mnemonic);
            }
        }

        if (saved) {
            *p = saved;
            p++;
        }
    }

    rcc_free(tmpl_copy);

    if (preserve_edi) emit_pop_reg(mod, EDI);
    if (preserve_esi) emit_pop_reg(mod, ESI);
    if (preserve_ebx) emit_pop_reg(mod, EBX);

    /* Store outputs from registers to lvalues */
    for (int i = 0; i < output_count; i++) {
        if (operands[i].reg >= 0 && operands[i].op->expr) {
            /* Get lvalue address */
            emit_push_reg(mod, operands[i].reg);  /* Save the result */
            gen_lvalue(mod, operands[i].op->expr);
            emit_mov_reg_reg(mod, ECX, EAX);      /* ECX = address */
            emit_pop_reg(mod, EAX);               /* EAX = result */
            if (operands[i].reg != EAX) {
                emit_mov_reg_reg(mod, EAX, operands[i].reg);
            }
            emit_store_typed32(mod, ECX, 0, EAX,
                               operands[i].op->expr->type);
        }
    }

    if (operands) {
        rcc_free(operands);
    }
}

/* ═══════════════════════════════════════
 * Statement Code Generation
 * ═══════════════════════════════════════ */

static int codegen_max_local_bytes(int first, int second) {
    return first > second ? first : second;
}

int codegen_required_local_bytes(Stmt* statement) {
    int required = 0;
    if (!statement) return 0;
    switch (statement->kind) {
        case STMT_BLOCK:
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                required = codegen_max_local_bytes(
                    required, codegen_required_local_bytes(item->stmt));
            }
            return required;
        case STMT_IF:
            required = codegen_required_local_bytes(statement->if_then);
            return codegen_max_local_bytes(
                required, codegen_required_local_bytes(statement->if_else));
        case STMT_WHILE:
        case STMT_DO:
            return codegen_required_local_bytes(statement->while_body);
        case STMT_FOR:
            required = codegen_required_local_bytes(statement->for_init);
            return codegen_max_local_bytes(
                required, codegen_required_local_bytes(statement->for_body));
        case STMT_SWITCH:
            return codegen_required_local_bytes(statement->switch_body);
        case STMT_CASE:
            return codegen_required_local_bytes(statement->case_stmt);
        case STMT_DEFAULT:
            return codegen_required_local_bytes(statement->default_stmt);
        case STMT_LABEL:
            return codegen_required_local_bytes(statement->label_stmt);
        case STMT_TRY: {
            int required = statement->try_frame_offset < 0
                ? -(statement->try_frame_offset) : statement->try_frame_size;
            required = codegen_max_local_bytes(
                required, codegen_required_local_bytes(statement->try_body));
            for (CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                required = codegen_max_local_bytes(
                    required, codegen_required_local_bytes(handler->body));
            }
            return required;
        }
        case STMT_DECL:
            if (statement->decl && statement->decl->kind == DECL_VAR &&
                !statement->decl->var_is_global &&
                statement->decl->var_offset < 0) {
                int64_t extent = -(int64_t)statement->decl->var_offset;
                return extent > INT_MAX ? INT_MAX : (int)extent;
            }
            return 0;
        default:
            return 0;
    }
}

static bool codegen_stmt_owns_vla(Stmt* statement) {
    if (!statement) return false;
    if (statement->kind == STMT_BLOCK) {
        for (StmtList* item = statement->block_stmts; item;
             item = item->next) {
            if (item->stmt && item->stmt->kind == STMT_DECL &&
                item->stmt->decl && item->stmt->decl->kind == DECL_VAR &&
                item->stmt->decl->var_is_vla) {
                return true;
            }
        }
    } else if (statement->kind == STMT_FOR) {
        Stmt* init = statement->for_init;
        return init && init->kind == STMT_DECL && init->decl &&
               init->decl->kind == DECL_VAR && init->decl->var_is_vla;
    } else if (statement->kind == STMT_TRY) {
        if (codegen_stmt_owns_vla(statement->try_body)) return true;
        for (CxxCatch* handler = statement->try_catches; handler;
             handler = handler->next) {
            if (codegen_stmt_owns_vla(handler->body)) return true;
        }
    }
    return false;
}

static int codegen_align_frame_bytes(int bytes, int alignment) {
    int64_t value;
    if (alignment <= 1) return bytes;
    value = (int64_t)bytes + alignment - 1;
    if (value > INT_MAX) return INT_MAX;
    return (int)(value / alignment * alignment);
}

static void codegen_assign_compound_expr(Expr* expression, int* bytes,
                                         int stack_alignment) {
    if (!expression || !bytes || *bytes == INT_MAX) return;
    if (expression->kind == EXPR_COMPOUND && expression->compound_type) {
        int size = expression->compound_type->size;
        int alignment = expression->compound_type->align;
        int64_t extent;
        if (alignment < stack_alignment) alignment = stack_alignment;
        if (size <= 0) size = 1;
        extent = (int64_t)*bytes + size;
        if (extent > INT_MAX) {
            *bytes = INT_MAX;
        } else {
            *bytes = codegen_align_frame_bytes((int)extent, alignment);
            expression->compound_offset = -*bytes;
        }
    }
    if (expression->kind == EXPR_CALL && expression->type &&
        (expression->type->kind == TYPE_STRUCT ||
         expression->type->kind == TYPE_UNION)) {
        int size = expression->type->size;
        int alignment = expression->type->align;
        int64_t extent;
        if (alignment < stack_alignment) alignment = stack_alignment;
        if (size <= 0) size = 1;
        extent = (int64_t)*bytes + size;
        if (extent > INT_MAX) {
            *bytes = INT_MAX;
        } else {
            *bytes = codegen_align_frame_bytes((int)extent, alignment);
            expression->call_result_offset = -*bytes;
        }
    }
    if (expression->kind == EXPR_VA_ARG && expression->va_arg_type &&
        (expression->va_arg_type->kind == TYPE_STRUCT ||
         expression->va_arg_type->kind == TYPE_UNION)) {
        int size = expression->va_arg_type->size;
        int alignment = expression->va_arg_type->align;
        int64_t extent;
        if (alignment < stack_alignment) alignment = stack_alignment;
        if (size <= 0) size = 1;
        extent = (int64_t)*bytes + size;
        if (extent > INT_MAX) {
            *bytes = INT_MAX;
        } else {
            *bytes = codegen_align_frame_bytes((int)extent, alignment);
            expression->va_arg_result_offset = -*bytes;
        }
    }

    switch (expression->kind) {
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
        case EXPR_ADDR:
        case EXPR_DEREF:
        case EXPR_PREINC:
        case EXPR_PREDEC:
        case EXPR_POSTINC:
        case EXPR_POSTDEC:
            codegen_assign_compound_expr(expression->unary_operand, bytes,
                                         stack_alignment);
            break;
        case EXPR_SIZEOF:
            if (!expression->sizeof_type) {
                codegen_assign_compound_expr(expression->unary_operand, bytes,
                                             stack_alignment);
            }
            break;
        case EXPR_CAST:
            codegen_assign_compound_expr(expression->cast_expr, bytes,
                                         stack_alignment);
            break;
        case EXPR_ADD:
        case EXPR_SUB:
        case EXPR_MUL:
        case EXPR_DIV:
        case EXPR_MOD:
        case EXPR_BITAND:
        case EXPR_BITOR:
        case EXPR_BITXOR:
        case EXPR_LSHIFT:
        case EXPR_RSHIFT:
        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_GT:
        case EXPR_LE:
        case EXPR_GE:
        case EXPR_AND:
        case EXPR_OR:
        case EXPR_ASSIGN:
        case EXPR_ADD_ASSIGN:
        case EXPR_SUB_ASSIGN:
        case EXPR_MUL_ASSIGN:
        case EXPR_DIV_ASSIGN:
        case EXPR_MOD_ASSIGN:
        case EXPR_AND_ASSIGN:
        case EXPR_OR_ASSIGN:
        case EXPR_XOR_ASSIGN:
        case EXPR_LSHIFT_ASSIGN:
        case EXPR_RSHIFT_ASSIGN:
        case EXPR_COMMA:
            codegen_assign_compound_expr(expression->binary_lhs, bytes,
                                         stack_alignment);
            codegen_assign_compound_expr(expression->binary_rhs, bytes,
                                         stack_alignment);
            break;
        case EXPR_COND:
            codegen_assign_compound_expr(expression->cond_test, bytes,
                                         stack_alignment);
            codegen_assign_compound_expr(expression->cond_then, bytes,
                                         stack_alignment);
            codegen_assign_compound_expr(expression->cond_else, bytes,
                                         stack_alignment);
            break;
        case EXPR_CALL:
            codegen_assign_compound_expr(expression->call_func, bytes,
                                         stack_alignment);
            for (ExprList* argument = expression->call_args; argument;
                 argument = argument->next) {
                codegen_assign_compound_expr(argument->expr, bytes,
                                             stack_alignment);
            }
            break;
        case EXPR_INDEX:
            codegen_assign_compound_expr(expression->index_base, bytes,
                                         stack_alignment);
            codegen_assign_compound_expr(expression->index_expr, bytes,
                                         stack_alignment);
            break;
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            codegen_assign_compound_expr(expression->member_base, bytes,
                                         stack_alignment);
            break;
        case EXPR_COMPOUND:
            for (ExprList* initializer = expression->compound_init;
                 initializer; initializer = initializer->next) {
                codegen_assign_compound_expr(initializer->expr, bytes,
                                             stack_alignment);
            }
            break;
        case EXPR_GENERIC:
            codegen_assign_compound_expr(expression->generic_control, bytes,
                                         stack_alignment);
            for (GenericAssociation* association =
                     expression->generic_associations;
                 association; association = association->next) {
                codegen_assign_compound_expr(association->expr, bytes,
                                             stack_alignment);
            }
            break;
        default:
            break;
    }
}

static void codegen_assign_compound_stmt(Stmt* statement, int* bytes,
                                         int stack_alignment) {
    if (!statement) return;
    switch (statement->kind) {
        case STMT_EXPR:
            codegen_assign_compound_expr(statement->expr, bytes,
                                         stack_alignment);
            break;
        case STMT_BLOCK:
            if (codegen_stmt_owns_vla(statement) &&
                statement->vla_stack_offset == 0) {
                int64_t extent = (int64_t)*bytes + stack_alignment;
                if (extent > INT_MAX) {
                    *bytes = INT_MAX;
                } else {
                    *bytes = codegen_align_frame_bytes((int)extent,
                                                       stack_alignment);
                    statement->vla_stack_offset = -*bytes;
                }
            }
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                if (item->stmt && item->stmt->kind == STMT_DECL &&
                    item->stmt->decl &&
                    item->stmt->decl->kind == DECL_VAR &&
                    item->stmt->decl->var_is_vla) {
                    item->stmt->decl->var_vla_scope_offset =
                        statement->vla_stack_offset;
                }
                codegen_assign_compound_stmt(item->stmt, bytes,
                                             stack_alignment);
            }
            break;
        case STMT_IF:
            codegen_assign_compound_expr(statement->if_cond, bytes,
                                         stack_alignment);
            codegen_assign_compound_stmt(statement->if_then, bytes,
                                         stack_alignment);
            codegen_assign_compound_stmt(statement->if_else, bytes,
                                         stack_alignment);
            break;
        case STMT_WHILE:
        case STMT_DO:
            codegen_assign_compound_expr(statement->while_cond, bytes,
                                         stack_alignment);
            codegen_assign_compound_stmt(statement->while_body, bytes,
                                         stack_alignment);
            break;
        case STMT_FOR:
            if (codegen_stmt_owns_vla(statement) &&
                statement->vla_stack_offset == 0) {
                int64_t extent = (int64_t)*bytes + stack_alignment;
                if (extent > INT_MAX) {
                    *bytes = INT_MAX;
                } else {
                    *bytes = codegen_align_frame_bytes((int)extent,
                                                       stack_alignment);
                    statement->vla_stack_offset = -*bytes;
                }
            }
            if (statement->for_init &&
                statement->for_init->kind == STMT_DECL &&
                statement->for_init->decl &&
                statement->for_init->decl->kind == DECL_VAR &&
                statement->for_init->decl->var_is_vla) {
                statement->for_init->decl->var_vla_scope_offset =
                    statement->vla_stack_offset;
            }
            codegen_assign_compound_stmt(statement->for_init, bytes,
                                         stack_alignment);
            codegen_assign_compound_expr(statement->for_cond, bytes,
                                         stack_alignment);
            codegen_assign_compound_expr(statement->for_inc, bytes,
                                         stack_alignment);
            codegen_assign_compound_stmt(statement->for_body, bytes,
                                         stack_alignment);
            break;
        case STMT_SWITCH:
            codegen_assign_compound_expr(statement->switch_expr, bytes,
                                         stack_alignment);
            codegen_assign_compound_stmt(statement->switch_body, bytes,
                                         stack_alignment);
            break;
        case STMT_CASE:
            codegen_assign_compound_expr(statement->case_val, bytes,
                                         stack_alignment);
            codegen_assign_compound_stmt(statement->case_stmt, bytes,
                                         stack_alignment);
            break;
        case STMT_DEFAULT:
            codegen_assign_compound_stmt(statement->default_stmt, bytes,
                                         stack_alignment);
            break;
        case STMT_RETURN:
            codegen_assign_compound_expr(statement->return_val, bytes,
                                         stack_alignment);
            break;
        case STMT_LABEL:
            codegen_assign_compound_stmt(statement->label_stmt, bytes,
                                         stack_alignment);
            break;
        case STMT_TRY:
            codegen_assign_compound_stmt(statement->try_body, bytes,
                                         stack_alignment);
            for (CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                codegen_assign_compound_stmt(handler->body, bytes,
                                             stack_alignment);
            }
            break;
        case STMT_DECL:
            if (statement->decl && statement->decl->kind == DECL_VAR) {
                codegen_assign_compound_expr(statement->decl->var_init, bytes,
                                             stack_alignment);
            }
            break;
        case STMT_ASM:
            for (AsmOperand* operand = statement->asm_outputs; operand;
                 operand = operand->next) {
                codegen_assign_compound_expr(operand->expr, bytes,
                                             stack_alignment);
            }
            for (AsmOperand* operand = statement->asm_inputs; operand;
                 operand = operand->next) {
                codegen_assign_compound_expr(operand->expr, bytes,
                                             stack_alignment);
            }
            break;
        default:
            break;
    }
}

int codegen_assign_compound_storage(Stmt* statement, int initial_bytes,
                                    int stack_alignment) {
    int bytes = initial_bytes < 0 ? INT_MAX : initial_bytes;
    if (stack_alignment < 1) stack_alignment = 1;
    codegen_assign_compound_stmt(statement, &bytes, stack_alignment);
    return bytes;
}

static void gen_zero_local_storage(Module* mod, int32_t displacement,
                                   size_t storage) {
    size_t offset = 0u;
    emit_mov_reg_imm(mod, EAX, 0u);
    while (offset + 4u <= storage) {
        emit_mov_mem_reg(mod, EBP, displacement + (int32_t)offset, EAX);
        offset += 4u;
    }
    while (offset < storage) {
        emit_mov_mem_reg8(mod, EBP, displacement + (int32_t)offset, EAX);
        ++offset;
    }
}

static bool gen_local_initializer(Module* mod, Type* type, Expr* initializer,
                                  int32_t displacement) {
    Expr* string = codegen_character_array_string(type, initializer);
    if (!type || !initializer) return false;
    if (gen_cxx_initializer_calls_body(initializer)) {
        emit_byte(mod, 0x8D);  /* LEA EAX, [EBP+disp32] */
        emit_byte(mod, modrm(2, EAX, EBP));
        emit_dword(mod, (uint32_t)displacement);
        emit_mov_reg_reg(mod, ECX, EAX);
        gen_cxx_call_constructor32(
            mod, initializer->compound_constructor,
            initializer->compound_init);
        return true;
    }
    if (codegen_aggregate_zero_initializer(type, initializer)) return true;
    if (string) {
        size_t storage = (size_t)type->size;
        size_t text_size = strlen(string->str_val) + 1u;
        size_t offset = 0u;
        while (offset + 4u <= storage) {
            uint32_t packed = 0u;
            for (size_t byte = 0u; byte < 4u; ++byte) {
                if (offset + byte < text_size) {
                    packed |= (uint32_t)(uint8_t)string->str_val[offset + byte]
                              << (byte * 8u);
                }
            }
            emit_mov_reg_imm(mod, EAX, packed);
            emit_mov_mem_reg(mod, EBP, displacement + (int32_t)offset, EAX);
            offset += 4u;
        }
        while (offset < storage) {
            uint8_t byte = offset < text_size
                ? (uint8_t)string->str_val[offset] : 0u;
            emit_mov_reg_imm(mod, EAX, byte);
            emit_mov_mem_reg8(mod, EBP,
                              displacement + (int32_t)offset, EAX);
            ++offset;
        }
        return true;
    }
    if (initializer->kind == EXPR_COMPOUND) {
        if (type->kind == TYPE_ARRAY) {
            int64_t cursor = 0;
            for (ExprList* item = initializer->compound_init; item;
                 item = item->next) {
                int64_t item_offset;
                if (item->designator_kind == INIT_DESIGNATOR_FIELD) {
                    return false;
                }
                if (item->designator_kind == INIT_DESIGNATOR_INDEX) {
                    cursor = item->designator_index;
                }
                if (cursor < 0 || cursor >= type->array_len || !type->base) {
                    return false;
                }
                item_offset = (int64_t)displacement +
                              cursor * type->base->size;
                if (item_offset < INT32_MIN || item_offset > INT32_MAX ||
                    !gen_local_initializer(mod, type->base, item->expr,
                                           (int32_t)item_offset)) {
                    return false;
                }
                ++cursor;
            }
            return true;
        }
        if (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) {
            TypeField* cursor = type->fields;
            int initialized = 0;
            for (ExprList* item = initializer->compound_init; item;
                 item = item->next) {
                TypeField* field = cursor;
                int64_t field_offset;
                if (item->designator_kind == INIT_DESIGNATOR_INDEX) {
                    return false;
                }
                if (item->designator_kind == INIT_DESIGNATOR_FIELD) {
                    field = codegen_initializer_field(
                        type, item->designator_field);
                }
                if (!field || (type->kind == TYPE_UNION && initialized != 0 &&
                               item->designator_kind ==
                                   INIT_DESIGNATOR_NONE)) {
                    return false;
                }
                field_offset = (int64_t)displacement + field->offset;
                if (field_offset < INT32_MIN || field_offset > INT32_MAX ||
                    (field->is_bitfield
                         ? !gen_bitfield_initializer32(
                               mod, field, item->expr,
                               (int32_t)field_offset)
                         : !gen_local_initializer(
                               mod, field->type, item->expr,
                               (int32_t)field_offset))) {
                    return false;
                }
                cursor = field->next;
                ++initialized;
            }
            return true;
        }
        if (!initializer->compound_init || initializer->compound_init->next ||
            initializer->compound_init->designator_kind !=
                INIT_DESIGNATOR_NONE) {
            return false;
        }
        return gen_local_initializer(mod, type,
                                     initializer->compound_init->expr,
                                     displacement);
    }
    if ((type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) &&
        initializer->type && type_is_compatible(type, initializer->type)) {
        int offset = 0;
        if (initializer->kind == EXPR_VA_ARG) {
            gen_expr(mod, initializer);
        } else {
            gen_lvalue(mod, initializer);
        }
        emit_mov_reg_reg(mod, ECX, EAX);
        while (offset + 4 <= type->size) {
            emit_mov_reg_mem(mod, EAX, ECX, offset);
            emit_mov_mem_reg(mod, EBP, displacement + offset, EAX);
            offset += 4;
        }
        while (offset < type->size) {
            emit_load_typed32(mod, EAX, ECX, offset, type_uchar);
            emit_store_typed32(mod, EBP, displacement + offset, EAX,
                               type_uchar);
            ++offset;
        }
        return true;
    }
    if (type_is_floating(type)) {
        gen_expr_as_type(mod, initializer, type);
        emit_store_floating_raw(mod, type, EBP, displacement);
        return true;
    }
    if (!type_is_integer(type) && type->kind != TYPE_ENUM &&
        type->kind != TYPE_PTR && type->kind != TYPE_NULLPTR) {
        return false;
    }
    gen_expr(mod, initializer);
    if (gen_is_integer64(type)) {
        if (!gen_is_integer64(initializer->type)) {
            emit_extend_eax_to_integer64(mod, initializer->type);
        }
        emit_mov_mem_reg(mod, EBP, displacement, EAX);
        emit_mov_mem_reg(mod, EBP, displacement + 4, EDX);
    } else {
        if (type_is_integer(type) || type->kind == TYPE_ENUM) {
            emit_convert_integer_value(mod, EAX, initializer->type, type);
        }
        emit_store_typed32(mod, EBP, displacement, EAX, type);
    }
    return true;
}

static int break_label = -1;
static int continue_label = -1;
static Type* current_function_return_type = NULL;

typedef struct SwitchCaseCodegen {
    Stmt* statement;
    uint64_t bits;
    int label;
    struct SwitchCaseCodegen* next;
} SwitchCaseCodegen;

typedef struct SwitchCodegenContext {
    Type* control_type;
    SwitchCaseCodegen* cases;
    Stmt* default_statement;
    int default_label;
    struct SwitchCodegenContext* previous;
} SwitchCodegenContext;

static SwitchCodegenContext* current_switch_codegen = NULL;

typedef struct NamedCodegenLabel {
    const char* name;
    int label;
    struct NamedCodegenLabel* next;
} NamedCodegenLabel;

static NamedCodegenLabel* named_codegen_labels = NULL;

static int codegen_named_label(const char* name) {
    NamedCodegenLabel* item = named_codegen_labels;
    while (item) {
        if (strcmp(item->name, name) == 0) return item->label;
        item = item->next;
    }
    item = rcc_alloc(sizeof(*item));
    item->name = name;
    item->label = new_label();
    item->next = named_codegen_labels;
    named_codegen_labels = item;
    return item->label;
}

static void codegen_release_named_labels(void) {
    while (named_codegen_labels) {
        NamedCodegenLabel* next = named_codegen_labels->next;
        rcc_free(named_codegen_labels);
        named_codegen_labels = next;
    }
}

static void emit_convert_integer_value(Module* mod, int reg,
                                       const Type* source_type,
                                       const Type* target_type) {
    if (reg == EAX && target_type && target_type->kind == TYPE_BOOL &&
        gen_is_integer64(source_type)) {
        emit_or_reg_reg(mod, EAX, EDX);
    }
    emit_normalize_atomic_value(mod, reg, target_type);
}

static void gen_expr(Module* mod, Expr* expr) {
    if (!expr) return;
    if (gen_is_integer64(expr->type)) {
        gen_expr64_pair(mod, expr);
        return;
    }
    gen_expr_raw(mod, expr);
    if (expr->cxx_pointer_adjustment_valid &&
        expr->cxx_pointer_adjustment != 0) {
        emit_add_reg_imm(mod, EAX, expr->cxx_pointer_adjustment);
    }
    if (type_is_integer(expr->type) || expr->type->kind == TYPE_ENUM) {
        emit_normalize_atomic_value(mod, EAX, expr->type);
    }
}

typedef struct CleanupCodegen {
    Expr* expression;
    struct CleanupCodegen* previous;
} CleanupCodegen;

typedef struct VLAScopeCodegen {
    int stack_offset;
    struct VLAScopeCodegen* previous;
} VLAScopeCodegen;

static CleanupCodegen* active_cleanups = NULL;
static CleanupCodegen* break_cleanup_marker = NULL;
static CleanupCodegen* continue_cleanup_marker = NULL;
static VLAScopeCodegen* active_vla_scopes = NULL;
static VLAScopeCodegen* break_vla_marker = NULL;
static VLAScopeCodegen* continue_vla_marker = NULL;

static void gen_cleanups_until(Module* mod, CleanupCodegen* marker) {
    for (CleanupCodegen* item = active_cleanups; item && item != marker;
         item = item->previous) {
        gen_expr(mod, item->expression);
    }
}

static bool gen_cleanup_count(Module* mod, unsigned count) {
    CleanupCodegen* item = active_cleanups;
    while (item && count > 0u) {
        gen_expr(mod, item->expression);
        item = item->previous;
        --count;
    }
    return count == 0u;
}

static void discard_cleanups_until(CleanupCodegen* marker) {
    while (active_cleanups && active_cleanups != marker) {
        CleanupCodegen* previous = active_cleanups->previous;
        rcc_free(active_cleanups);
        active_cleanups = previous;
    }
}

static void gen_vla_scopes_until(Module* mod, VLAScopeCodegen* marker) {
    for (VLAScopeCodegen* item = active_vla_scopes;
         item && item != marker; item = item->previous) {
        emit_mov_reg_mem(mod, ESP, EBP, item->stack_offset);
    }
}

static bool gen_vla_count(Module* mod, unsigned count) {
    VLAScopeCodegen* item = active_vla_scopes;
    while (item && count > 0u) {
        emit_mov_reg_mem(mod, ESP, EBP, item->stack_offset);
        item = item->previous;
        --count;
    }
    return count == 0u;
}

static void discard_vla_scopes_until(VLAScopeCodegen* marker) {
    while (active_vla_scopes && active_vla_scopes != marker) {
        VLAScopeCodegen* previous = active_vla_scopes->previous;
        rcc_free(active_vla_scopes);
        active_vla_scopes = previous;
    }
}

static void record_vla_scope(Decl* declaration) {
    VLAScopeCodegen* scope = rcc_alloc(sizeof(*scope));
    scope->stack_offset = declaration->var_vla_scope_offset;
    scope->previous = active_vla_scopes;
    active_vla_scopes = scope;
}

static void gen_scoped_stmt(Module* mod, Stmt* statement) {
    CleanupCodegen* marker = active_cleanups;
    gen_stmt(mod, statement);
    gen_cleanups_until(mod, marker);
    discard_cleanups_until(marker);
}

static void gen_cxx_exception_frame_address32(Module* mod, int offset) {
    emit_mov_reg_reg(mod, EAX, EBP);
    emit_add_reg_imm(mod, EAX, offset);
}

static void gen_cxx_exception_call32(Module* mod, const char* name) {
    uint32_t call_offset;
    emit_call_rel32(mod, 0u);
    call_offset = code_offset(mod) - 4u;
    add_func_call_ref(name, call_offset);
}

static uint32_t gen_cxx_exception_type_tag32(const Type* type) {
    return (uint32_t)rcc_cxx_exception_type_tag(type);
}

static void gen_cxx_throw32(Module* mod, Stmt* stmt) {
    Type* type;
    if (!stmt) rcc_fatal("validated C++ throw is missing");
    if (!stmt->throw_expr) {
        gen_cxx_exception_call32(mod, "rin_cpp_exception_rethrow");
        return;
    }
    type = stmt->throw_expr->type;
    gen_expr(mod, stmt->throw_expr);
    emit_mov_reg_reg(mod, EDX, EAX); /* preserve value while loading tag */
    emit_mov_reg_imm(mod, EAX, gen_cxx_exception_type_tag32(type));
    emit_push_reg(mod, EAX); /* type tag */
    emit_push_reg(mod, EDX); /* value; cdecl argument 1 is at the top */
    gen_cxx_exception_call32(mod, "rin_cpp_exception_throw");
    emit_add_reg_imm(mod, ESP, 8);
}

static void gen_cxx_try32(Module* mod, Stmt* stmt) {
    const int value_offset = 28;
    const int type_offset = 32;
    int dispatch_label = new_label();
    int end_label = new_label();

    gen_cxx_exception_frame_address32(mod, stmt->try_frame_offset);
    emit_push_reg(mod, EAX);
    gen_cxx_exception_call32(mod, "rin_cpp_exception_install");
    emit_add_reg_imm(mod, ESP, 4);

    gen_cxx_exception_frame_address32(mod, stmt->try_frame_offset);
    emit_push_reg(mod, EAX);
    gen_cxx_exception_call32(mod, "setjmp");
    emit_add_reg_imm(mod, ESP, 4);
    emit_test_reg_reg(mod, EAX, EAX);
    emit_jcc_label(mod, CC_NE, dispatch_label);

    gen_scoped_stmt(mod, stmt->try_body);
    gen_cxx_exception_frame_address32(mod, stmt->try_frame_offset);
    emit_push_reg(mod, EAX);
    gen_cxx_exception_call32(mod, "rin_cpp_exception_leave");
    emit_add_reg_imm(mod, ESP, 4);
    emit_jmp_label(mod, end_label);

    emit_label(mod, dispatch_label);
    for (CxxCatch* handler = stmt->try_catches; handler;
         handler = handler->next) {
        int next_handler = new_label();
        if (!handler->is_ellipsis) {
            gen_cxx_exception_frame_address32(mod, stmt->try_frame_offset);
            emit_mov_reg_mem(mod, EAX, EAX, type_offset);
            emit_cmp_reg_imm(mod, EAX,
                             (int32_t)gen_cxx_exception_type_tag32(
                                 handler->type));
            emit_jcc_label(mod, CC_NE, next_handler);
        }
        if (handler->parameter) {
            gen_cxx_exception_frame_address32(mod, stmt->try_frame_offset);
            emit_mov_reg_mem(mod, EDX, EAX, value_offset);
            emit_store_typed32(mod, EBP, handler->parameter->var_offset,
                               EDX, handler->parameter->type);
        }
        /* A handler owns the transfer out of the protected region.  Remove
         * this frame before its body so return and rethrow cannot leave a
         * dead stack frame at the top of the runtime chain. */
        gen_cxx_exception_frame_address32(mod, stmt->try_frame_offset);
        emit_push_reg(mod, EAX);
        gen_cxx_exception_call32(mod, "rin_cpp_exception_leave");
        emit_add_reg_imm(mod, ESP, 4);
        gen_scoped_stmt(mod, handler->body);
        emit_jmp_label(mod, end_label);
        if (!handler->is_ellipsis) emit_label(mod, next_handler);
    }

    /* The runtime has already removed this frame.  Re-submit an unmatched
     * payload so an enclosing try or the process-level handler can receive it. */
    gen_cxx_exception_frame_address32(mod, stmt->try_frame_offset);
    emit_mov_reg_mem(mod, EAX, EAX, value_offset);
    emit_mov_reg_reg(mod, EDX, EAX); /* preserve value while loading tag */
    gen_cxx_exception_frame_address32(mod, stmt->try_frame_offset);
    emit_mov_reg_mem(mod, EAX, EAX, type_offset);
    emit_push_reg(mod, EAX); /* type tag */
    emit_push_reg(mod, EDX); /* value; cdecl argument 1 is at the top */
    gen_cxx_exception_call32(mod, "rin_cpp_exception_throw");
    emit_add_reg_imm(mod, ESP, 8);
    emit_label(mod, end_label);
}

static bool gen_global_initializer32(Module* mod, Decl* declaration) {
    Type* type = declaration ? declaration->type : NULL;
    Expr* initializer = declaration ? declaration->var_init : NULL;
    if (!mod || !declaration || !type || !initializer) return false;
    if (gen_is_floating(type)) {
        gen_expr_as_type(mod, initializer, type);
        if (gen_float_width(type) == 4) {
            emit_push_reg(mod, EAX);
            gen_symbol_address(mod, decl_link_name(declaration), 0u);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_pop_reg(mod, EAX);
        } else {
            emit_push_reg(mod, EDX);
            emit_push_reg(mod, EAX);
            gen_symbol_address(mod, decl_link_name(declaration), 0u);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_pop_reg(mod, EAX);
            emit_pop_reg(mod, EDX);
        }
        emit_store_floating_raw(mod, type, ECX, 0);
        return true;
    }
    if (gen_is_integer64(type)) {
        gen_expr_as_integer64(mod, initializer);
        emit_push_reg(mod, EDX);
        emit_push_reg(mod, EAX);
        gen_symbol_address(mod, decl_link_name(declaration), 0u);
        emit_mov_reg_reg(mod, ECX, EAX);
        emit_pop_reg(mod, EAX);
        emit_pop_reg(mod, EDX);
        emit_mov_mem_reg(mod, ECX, 0, EAX);
        emit_mov_mem_reg(mod, ECX, 4, EDX);
        return true;
    }
    gen_expr(mod, initializer);
    if (type_is_integer(type) || type->kind == TYPE_ENUM) {
        emit_convert_integer_value(mod, EAX, initializer->type, type);
        emit_normalize_atomic_value(mod, EAX, type);
    }
    emit_push_reg(mod, EAX);
    gen_symbol_address(mod, decl_link_name(declaration), 0u);
    emit_mov_reg_reg(mod, EDX, EAX);
    emit_pop_reg(mod, ECX);
    emit_store_typed32(mod, EDX, 0, ECX, type);
    return true;
}

static void codegen_emit_global_init32(Module* mod) {
    const char* name = "__rcc_global_init";
    GlobalInitializer* initializer;
    Type* old_return_type;
    CleanupCodegen* old_cleanups;
    VLAScopeCodegen* old_vla_scopes;
    VLAScopeCodegen* old_break_vla;
    VLAScopeCodegen* old_continue_vla;
    uint32_t start;
    if (!mod || !mod->global_initializers) return;
    start = code_offset(mod);
    add_func_def(name, start);
    emit_push_reg(mod, EBP);
    emit_mov_reg_reg(mod, EBP, ESP);
    old_return_type = current_function_return_type;
    current_function_return_type = NULL;
    old_cleanups = active_cleanups;
    old_vla_scopes = active_vla_scopes;
    old_break_vla = break_vla_marker;
    old_continue_vla = continue_vla_marker;
    active_cleanups = NULL;
    active_vla_scopes = NULL;
    break_vla_marker = NULL;
    continue_vla_marker = NULL;
    for (initializer = mod->global_initializers; initializer;
         initializer = initializer->next) {
        if (!gen_global_initializer32(mod, initializer->declaration)) {
            rcc_error((SourceLoc){"<global-init>", 0, 0},
                      "cannot lower deferred global initializer");
        }
    }
    discard_cleanups_until(NULL);
    discard_vla_scopes_until(NULL);
    active_cleanups = old_cleanups;
    active_vla_scopes = old_vla_scopes;
    break_vla_marker = old_break_vla;
    continue_vla_marker = old_continue_vla;
    current_function_return_type = old_return_type;
    emit_mov_reg_imm(mod, EAX, 0u);
    emit_leave(mod);
    emit_ret(mod);
    module_add_symbol(mod, name, start, true, MODULE_SYMBOL_CODE, false);
    codegen_add_init_array_entry(mod, name);
}

static void codegen_emit_global_fini32(Module* mod) {
    const char* name = "__rcc_global_fini";
    GlobalFinalizer* finalizer;
    Type* old_return_type;
    CleanupCodegen* old_cleanups;
    VLAScopeCodegen* old_vla_scopes;
    VLAScopeCodegen* old_break_vla;
    VLAScopeCodegen* old_continue_vla;
    uint32_t start;
    if (!mod || !mod->global_finalizers) return;
    start = code_offset(mod);
    add_func_def(name, start);
    emit_push_reg(mod, EBP);
    emit_mov_reg_reg(mod, EBP, ESP);
    old_return_type = current_function_return_type;
    current_function_return_type = NULL;
    old_cleanups = active_cleanups;
    old_vla_scopes = active_vla_scopes;
    old_break_vla = break_vla_marker;
    old_continue_vla = continue_vla_marker;
    active_cleanups = NULL;
    active_vla_scopes = NULL;
    break_vla_marker = NULL;
    continue_vla_marker = NULL;
    for (finalizer = mod->global_finalizers; finalizer;
         finalizer = finalizer->next) {
        if (!finalizer->expression) {
            rcc_error((SourceLoc){"<global-fini>", 0, 0},
                      "cannot lower deferred global finalizer");
            continue;
        }
        gen_expr(mod, finalizer->expression);
    }
    discard_cleanups_until(NULL);
    discard_vla_scopes_until(NULL);
    active_cleanups = old_cleanups;
    active_vla_scopes = old_vla_scopes;
    break_vla_marker = old_break_vla;
    continue_vla_marker = old_continue_vla;
    current_function_return_type = old_return_type;
    emit_mov_reg_imm(mod, EAX, 0u);
    emit_leave(mod);
    emit_ret(mod);
    module_add_symbol(mod, name, start, true, MODULE_SYMBOL_CODE, false);
    codegen_add_fini_array_entry(mod, name);
}

static Type* codegen_switch_control_type(Type* type) {
    if (!type || type->kind == TYPE_ENUM || type->kind < TYPE_INT) {
        return type_int;
    }
    return type;
}

static uint64_t codegen_switch_case_bits(Expr* expression,
                                         Type* control_type) {
    int64_t value = 0;
    unsigned width = control_type && control_type->size > 0
        ? (unsigned)control_type->size * 8u : 32u;
    uint64_t bits;
    (void)expr_eval_integer_constant(expression, &value);
    bits = (uint64_t)value;
    if (width < 64u) bits &= (UINT64_C(1) << width) - 1u;
    return bits;
}

static void codegen_collect_switch_cases(Stmt* statement,
                                         SwitchCodegenContext* context) {
    if (!statement) return;
    switch (statement->kind) {
        case STMT_SWITCH:
            /* Labels in a nested switch belong to that switch. */
            return;
        case STMT_CASE: {
            SwitchCaseCodegen* item = rcc_alloc(sizeof(*item));
            item->statement = statement;
            item->bits = codegen_switch_case_bits(statement->case_val,
                                                   context->control_type);
            item->label = new_label();
            item->next = context->cases;
            context->cases = item;
            codegen_collect_switch_cases(statement->case_stmt, context);
            return;
        }
        case STMT_DEFAULT:
            context->default_statement = statement;
            context->default_label = new_label();
            codegen_collect_switch_cases(statement->default_stmt, context);
            return;
        case STMT_BLOCK:
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                codegen_collect_switch_cases(item->stmt, context);
            }
            return;
        case STMT_IF:
            codegen_collect_switch_cases(statement->if_then, context);
            codegen_collect_switch_cases(statement->if_else, context);
            return;
        case STMT_WHILE:
        case STMT_DO:
            codegen_collect_switch_cases(statement->while_body, context);
            return;
        case STMT_FOR:
            codegen_collect_switch_cases(statement->for_body, context);
            return;
        case STMT_LABEL:
            codegen_collect_switch_cases(statement->label_stmt, context);
            return;
        case STMT_TRY:
            codegen_collect_switch_cases(statement->try_body, context);
            for (CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                codegen_collect_switch_cases(handler->body, context);
            }
            return;
        default:
            return;
    }
}

static SwitchCaseCodegen* codegen_find_switch_case(
    SwitchCodegenContext* context, Stmt* statement) {
    SwitchCaseCodegen* item = context ? context->cases : NULL;
    while (item && item->statement != statement) item = item->next;
    return item;
}

static void codegen_release_switch_cases(SwitchCaseCodegen* item) {
    while (item) {
        SwitchCaseCodegen* next = item->next;
        rcc_free(item);
        item = next;
    }
}

static void gen_stmt(Module* mod, Stmt* stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case STMT_EXPR:
            if (stmt->expr) {
                gen_expr(mod, stmt->expr);
            }
            break;

        case STMT_BLOCK: {
            CleanupCodegen* marker = active_cleanups;
            VLAScopeCodegen* vla_marker = active_vla_scopes;
            if (stmt->vla_stack_offset < 0) {
                emit_mov_mem_reg(mod, EBP, stmt->vla_stack_offset, ESP);
            }
            for (StmtList* s = stmt->block_stmts; s; s = s->next) {
                gen_stmt(mod, s->stmt);
            }
            gen_cleanups_until(mod, marker);
            discard_cleanups_until(marker);
            gen_vla_scopes_until(mod, vla_marker);
            discard_vla_scopes_until(vla_marker);
            if (stmt->vla_stack_offset < 0) {
                emit_mov_reg_mem(mod, ESP, EBP, stmt->vla_stack_offset);
            }
            break;
        }

        case STMT_IF: {
            int else_label = new_label();
            int end_label = new_label();

            gen_expr(mod, stmt->if_cond);
            emit_test_scalar_value(mod, stmt->if_cond->type);
            emit_jcc_label(mod, CC_E, else_label);

            gen_scoped_stmt(mod, stmt->if_then);

            if (stmt->if_else) {
                emit_jmp_label(mod, end_label);
            }

            emit_label(mod, else_label);

            if (stmt->if_else) {
                gen_scoped_stmt(mod, stmt->if_else);
                emit_label(mod, end_label);
            }
            break;
        }

        case STMT_WHILE: {
            CleanupCodegen* loop_marker = active_cleanups;
            VLAScopeCodegen* vla_loop_marker = active_vla_scopes;
            CleanupCodegen* old_break_cleanup = break_cleanup_marker;
            CleanupCodegen* old_continue_cleanup = continue_cleanup_marker;
            VLAScopeCodegen* old_break_vla = break_vla_marker;
            VLAScopeCodegen* old_continue_vla = continue_vla_marker;
            int start_label = new_label();
            int end_label = new_label();
            int old_break = break_label;
            int old_continue = continue_label;
            break_label = end_label;
            continue_label = start_label;
            break_cleanup_marker = loop_marker;
            continue_cleanup_marker = loop_marker;
            break_vla_marker = vla_loop_marker;
            continue_vla_marker = vla_loop_marker;

            emit_label(mod, start_label);
            gen_expr(mod, stmt->while_cond);
            emit_test_scalar_value(mod, stmt->while_cond->type);
            emit_jcc_label(mod, CC_E, end_label);

            gen_scoped_stmt(mod, stmt->while_body);

            emit_jmp_label(mod, start_label);
            emit_label(mod, end_label);

            break_label = old_break;
            continue_label = old_continue;
            break_cleanup_marker = old_break_cleanup;
            continue_cleanup_marker = old_continue_cleanup;
            break_vla_marker = old_break_vla;
            continue_vla_marker = old_continue_vla;
            break;
        }

        case STMT_DO: {
            CleanupCodegen* loop_marker = active_cleanups;
            VLAScopeCodegen* vla_loop_marker = active_vla_scopes;
            CleanupCodegen* old_break_cleanup = break_cleanup_marker;
            CleanupCodegen* old_continue_cleanup = continue_cleanup_marker;
            VLAScopeCodegen* old_break_vla = break_vla_marker;
            VLAScopeCodegen* old_continue_vla = continue_vla_marker;
            int start_label = new_label();
            int end_label = new_label();
            int cond_label = new_label();
            int old_break = break_label;
            int old_continue = continue_label;
            break_label = end_label;
            continue_label = cond_label;
            break_cleanup_marker = loop_marker;
            continue_cleanup_marker = loop_marker;
            break_vla_marker = vla_loop_marker;
            continue_vla_marker = vla_loop_marker;

            emit_label(mod, start_label);
            gen_scoped_stmt(mod, stmt->while_body);

            emit_label(mod, cond_label);
            gen_expr(mod, stmt->while_cond);
            emit_test_scalar_value(mod, stmt->while_cond->type);
            emit_jcc_label(mod, CC_NE, start_label);

            emit_label(mod, end_label);

            break_label = old_break;
            continue_label = old_continue;
            break_cleanup_marker = old_break_cleanup;
            continue_cleanup_marker = old_continue_cleanup;
            break_vla_marker = old_break_vla;
            continue_vla_marker = old_continue_vla;
            break;
        }

        case STMT_FOR: {
            CleanupCodegen* marker = active_cleanups;
            VLAScopeCodegen* vla_marker = active_vla_scopes;
            CleanupCodegen* old_break_cleanup = break_cleanup_marker;
            CleanupCodegen* old_continue_cleanup = continue_cleanup_marker;
            VLAScopeCodegen* old_break_vla = break_vla_marker;
            VLAScopeCodegen* old_continue_vla = continue_vla_marker;
            int start_label = new_label();
            int end_label = new_label();
            int inc_label = new_label();
            int old_break = break_label;
            int old_continue = continue_label;
            break_label = end_label;
            continue_label = inc_label;

            if (stmt->vla_stack_offset < 0) {
                emit_mov_mem_reg(mod, EBP, stmt->vla_stack_offset, ESP);
            }
            if (stmt->for_init) {
                gen_stmt(mod, stmt->for_init);
            }
            break_cleanup_marker = active_cleanups;
            continue_cleanup_marker = active_cleanups;
            break_vla_marker = vla_marker;
            continue_vla_marker = active_vla_scopes;

            emit_label(mod, start_label);

            if (stmt->for_cond) {
                gen_expr(mod, stmt->for_cond);
                emit_test_scalar_value(mod, stmt->for_cond->type);
                emit_jcc_label(mod, CC_E, end_label);
            }

            gen_scoped_stmt(mod, stmt->for_body);

            emit_label(mod, inc_label);
            if (stmt->for_inc) {
                gen_expr(mod, stmt->for_inc);
            }

            emit_jmp_label(mod, start_label);
            emit_label(mod, end_label);
            gen_cleanups_until(mod, marker);
            discard_cleanups_until(marker);
            gen_vla_scopes_until(mod, vla_marker);
            discard_vla_scopes_until(vla_marker);
            if (stmt->vla_stack_offset < 0) {
                emit_mov_reg_mem(mod, ESP, EBP, stmt->vla_stack_offset);
            }

            break_label = old_break;
            continue_label = old_continue;
            break_cleanup_marker = old_break_cleanup;
            continue_cleanup_marker = old_continue_cleanup;
            break_vla_marker = old_break_vla;
            continue_vla_marker = old_continue_vla;
            break;
        }

        case STMT_SWITCH: {
            SwitchCodegenContext context = {0};
            SwitchCodegenContext* old_switch = current_switch_codegen;
            CleanupCodegen* switch_marker = active_cleanups;
            VLAScopeCodegen* vla_switch_marker = active_vla_scopes;
            CleanupCodegen* old_break_cleanup = break_cleanup_marker;
            VLAScopeCodegen* old_break_vla = break_vla_marker;
            int old_break = break_label;
            int end_label = new_label();
            context.control_type = codegen_switch_control_type(
                stmt->switch_expr ? stmt->switch_expr->type : NULL);
            context.default_label = -1;
            context.previous = old_switch;
            codegen_collect_switch_cases(stmt->switch_body, &context);

            /* Evaluate the controlling expression exactly once.  Wide i686
             * values use the established EDX:EAX scalar ABI. */
            gen_expr(mod, stmt->switch_expr);
            for (SwitchCaseCodegen* item = context.cases; item;
                 item = item->next) {
                if (context.control_type && context.control_type->size == 8) {
                    int next_test = new_label();
                    emit_cmp_reg_imm(mod, EDX,
                                     (int32_t)(uint32_t)(item->bits >> 32));
                    emit_jcc_label(mod, CC_NE, next_test);
                    emit_cmp_reg_imm(mod, EAX,
                                     (int32_t)(uint32_t)item->bits);
                    emit_jcc_label(mod, CC_E, item->label);
                    emit_label(mod, next_test);
                } else {
                    emit_cmp_reg_imm(mod, EAX,
                                     (int32_t)(uint32_t)item->bits);
                    emit_jcc_label(mod, CC_E, item->label);
                }
            }
            emit_jmp_label(mod, context.default_label >= 0
                ? context.default_label : end_label);

            break_label = end_label;
            break_cleanup_marker = switch_marker;
            break_vla_marker = vla_switch_marker;
            current_switch_codegen = &context;
            gen_stmt(mod, stmt->switch_body);
            current_switch_codegen = old_switch;
            break_label = old_break;
            break_cleanup_marker = old_break_cleanup;
            break_vla_marker = old_break_vla;
            emit_label(mod, end_label);
            codegen_release_switch_cases(context.cases);
            break;
        }

        case STMT_CASE: {
            SwitchCaseCodegen* item = codegen_find_switch_case(
                current_switch_codegen, stmt);
            if (item) emit_label(mod, item->label);
            gen_stmt(mod, stmt->case_stmt);
            break;
        }

        case STMT_DEFAULT:
            if (current_switch_codegen &&
                current_switch_codegen->default_statement == stmt) {
                emit_label(mod, current_switch_codegen->default_label);
            }
            gen_stmt(mod, stmt->default_stmt);
            break;

        case STMT_GOTO:
            if (!gen_cleanup_count(mod, stmt->goto_cleanup_count)) {
                rcc_error(stmt->loc, "invalid C++ goto cleanup path");
                break;
            }
            if (!gen_vla_count(mod, stmt->goto_vla_count)) {
                rcc_error(stmt->loc, "invalid VLA goto path");
                break;
            }
            emit_jmp_label(mod, codegen_named_label(stmt->goto_label));
            break;

        case STMT_LABEL:
            emit_label(mod, codegen_named_label(stmt->label_name));
            gen_stmt(mod, stmt->label_stmt);
            break;

        case STMT_RETURN:
            if (stmt->return_val) {
                if (gen_is_floating(current_function_return_type)) {
                    gen_expr_as_type(mod, stmt->return_val,
                                     current_function_return_type);
                } else if (current_function_return_type &&
                    (current_function_return_type->kind == TYPE_STRUCT ||
                     current_function_return_type->kind == TYPE_UNION)) {
                    int offset = 0;
                    if (stmt->return_val->kind == EXPR_VA_ARG) {
                        gen_expr(mod, stmt->return_val);
                    } else {
                        gen_lvalue(mod, stmt->return_val);
                    }
                    emit_mov_reg_reg(mod, ECX, EAX);
                    emit_mov_reg_mem(mod, EDX, EBP, 8);
                    while (offset + 4 <= current_function_return_type->size) {
                        emit_mov_reg_mem(mod, EAX, ECX, offset);
                        emit_mov_mem_reg(mod, EDX, offset, EAX);
                        offset += 4;
                    }
                    while (offset < current_function_return_type->size) {
                        emit_load_typed32(mod, EAX, ECX, offset, type_uchar);
                        emit_store_typed32(mod, EDX, offset, EAX, type_uchar);
                        ++offset;
                    }
                    emit_mov_reg_reg(mod, EAX, EDX);
                } else {
                    gen_expr(mod, stmt->return_val);
                }
                if (gen_is_integer64(current_function_return_type) &&
                    !gen_is_integer64(stmt->return_val->type)) {
                    emit_extend_eax_to_integer64(
                        mod, stmt->return_val->type);
                } else if (!gen_is_integer64(current_function_return_type) &&
                           !gen_is_floating(current_function_return_type) &&
                           (type_is_integer(current_function_return_type) ||
                            (current_function_return_type &&
                             current_function_return_type->kind ==
                                 TYPE_ENUM))) {
                    emit_convert_integer_value(
                        mod, EAX, stmt->return_val->type,
                        current_function_return_type);
                }
            }
            if (active_cleanups) {
                if (stmt->return_val) {
                    emit_push_reg(mod, EAX);
                    emit_push_reg(mod, EDX);
                }
                gen_cleanups_until(mod, NULL);
                if (stmt->return_val) {
                    emit_pop_reg(mod, EDX);
                    emit_pop_reg(mod, EAX);
                }
            }
            gen_vla_scopes_until(mod, NULL);
            if (stmt->return_val &&
                gen_is_floating(current_function_return_type)) {
                emit_x87_double_value_from_raw(
                    mod, current_function_return_type);
            }
            emit_leave(mod);
            emit_ret(mod);
            break;

        case STMT_BREAK:
            if (break_label >= 0) {
                gen_cleanups_until(mod, break_cleanup_marker);
                gen_vla_scopes_until(mod, break_vla_marker);
                emit_jmp_label(mod, break_label);
            }
            break;

        case STMT_CONTINUE:
            if (continue_label >= 0) {
                gen_cleanups_until(mod, continue_cleanup_marker);
                gen_vla_scopes_until(mod, continue_vla_marker);
                emit_jmp_label(mod, continue_label);
            }
            break;

        case STMT_DECL: {
            Decl* d = stmt->decl;
            if (d->kind == DECL_VAR &&
                (d->var_is_static_local || d->var_is_block_extern)) break;
            if (d->kind == DECL_VAR && d->var_is_vla) {
                gen_vla_alloc(mod, d);
                record_vla_scope(d);
            } else if (d->kind == DECL_VAR &&
                       (d->var_init ||
                        (d->type &&
                         codegen_type_has_vtable_storage(d->type)))) {
                if (d->type && (d->type->kind == TYPE_ARRAY ||
                                d->type->kind == TYPE_STRUCT ||
                                d->type->kind == TYPE_UNION) &&
                    !gen_cxx_initializer_calls_body(d->var_init)) {
                    gen_zero_local_storage(mod, d->var_offset,
                                           (size_t)d->type->size);
                }
                if (d->var_init &&
                    !gen_local_initializer(mod, d->type, d->var_init,
                                           d->var_offset)) {
                    rcc_error(d->loc, "unsupported local initializer for '%s'",
                              d->name);
                }
                gen_local_vtable_init(mod, d->type, d->var_offset);
            }
            if (d->kind == DECL_VAR && d->var_cleanup) {
                CleanupCodegen* cleanup = rcc_alloc(sizeof(*cleanup));
                cleanup->expression = d->var_cleanup;
                cleanup->previous = active_cleanups;
                active_cleanups = cleanup;
            }
            break;
        }

        case STMT_NULL:
            break;

        case STMT_ASM:
            gen_asm_stmt(mod, stmt);
            break;

        case STMT_TRY:
            gen_cxx_try32(mod, stmt);
            break;

        case STMT_THROW:
            gen_cxx_throw32(mod, stmt);
            break;

        default:
            break;
    }
}

/* ═══════════════════════════════════════
 * Function Code Generation
 * ═══════════════════════════════════════ */

static void gen_function(Module* mod, Decl* decl) {
    int stack_size;
    Type* old_return_type;
    CleanupCodegen* old_cleanups;
    VLAScopeCodegen* old_vla_scopes;
    VLAScopeCodegen* old_break_vla;
    VLAScopeCodegen* old_continue_vla;
    if (!decl->func_body) return;

    stack_size = codegen_required_local_bytes(decl->func_body);
    stack_size = codegen_assign_compound_storage(decl->func_body, stack_size,
                                                 4);
    codegen_assign_vla_parameter_slots(decl, &stack_size);
    if (stack_size > INT_MAX - 15) {
        rcc_error(decl->loc, "function stack frame exceeds compiler limits");
        return;
    }
    stack_size = (stack_size + 15) & ~15;

    /* Function prologue */
    emit_push_reg(mod, EBP);
    emit_mov_reg_reg(mod, EBP, ESP);
    if (stack_size > 0) {
        emit_sub_reg_imm(mod, ESP, stack_size);
    }

    gen_vla_parameter_extents(mod, decl);

    /* Generate body */
    old_return_type = current_function_return_type;
    current_function_return_type = decl->type &&
                                   decl->type->kind == TYPE_FUNC
        ? decl->type->ret_type : NULL;
    old_cleanups = active_cleanups;
    old_vla_scopes = active_vla_scopes;
    old_break_vla = break_vla_marker;
    old_continue_vla = continue_vla_marker;
    active_cleanups = NULL;
    active_vla_scopes = NULL;
    break_vla_marker = NULL;
    continue_vla_marker = NULL;
    named_codegen_labels = NULL;
    gen_stmt(mod, decl->func_body);
    discard_cleanups_until(NULL);
    discard_vla_scopes_until(NULL);
    active_cleanups = old_cleanups;
    active_vla_scopes = old_vla_scopes;
    break_vla_marker = old_break_vla;
    continue_vla_marker = old_continue_vla;
    codegen_release_named_labels();
    current_function_return_type = old_return_type;

    /* Function epilogue (fallthrough return) */
    Type* return_type = decl->type && decl->type->kind == TYPE_FUNC
        ? decl->type->ret_type : NULL;
    if (gen_is_floating(return_type)) {
        emit_x87_fld_zero(mod);
    } else {
        emit_mov_reg_imm(mod, EAX, 0);
    }
    if (gen_is_integer64(return_type)) {
        emit_mov_reg_imm(mod, EDX, 0);
    }
    emit_leave(mod);
    emit_ret(mod);
}

/* ═══════════════════════════════════════
 * Main Code Generation Entry
 * ═══════════════════════════════════════ */

Module* rcc_codegen(AST* ast) {
    Module* mod = codegen_new();

    /* Reset label counter and function call tracking */
    label_counter = 0;
    label_refs = NULL;
    label_defs = NULL;
    func_call_refs = NULL;
    func_defs = NULL;

    codegen_emit_global_data(mod, ast);

    /* First pass: add undefined symbols for external declarations */
    for (DeclList* d = ast->decls; d; d = d->next) {
        if (d->decl->kind == DECL_FUNC && !d->decl->func_body) {
            /* External function declaration */
            module_add_symbol(mod, decl_link_name(d->decl), 0, false,
                              MODULE_SYMBOL_CODE, true);
        }
    }

    /* Second pass: generate code for each defined function */
    for (DeclList* d = ast->decls; d; d = d->next) {
        if (d->decl->kind == DECL_FUNC && d->decl->func_body) {
            /* Record function start offset */
            uint32_t func_start = code_offset(mod);

            /* Record function definition for call resolution */
            add_func_def(decl_link_name(d->decl), func_start);

            /* Record entry point for main */
            if (strcmp(d->decl->name, "main") == 0) {
                mod->entry_point = func_start;
            }

            gen_function(mod, d->decl);

            /* Add symbol for function */
            module_add_symbol(mod, decl_link_name(d->decl), func_start, true,
                              MODULE_SYMBOL_CODE,
                             d->decl->storage != STORAGE_STATIC);
            if (d->decl->func_is_inline &&
                d->decl->func_has_cxx_linkage) {
                module_mark_symbol_weak(mod, decl_link_name(d->decl));
            }
        }
    }

    codegen_emit_global_init32(mod);
    codegen_emit_global_fini32(mod);

    codegen_emit_cxx_vtables(mod);

    /* Resolve internal function calls */
    resolve_func_calls(mod);

    /* Resolve label references */
    resolve_labels(mod);

    return mod;
}
