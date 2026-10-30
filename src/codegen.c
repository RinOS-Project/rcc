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
#include "optimize.h"
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <stdio.h>

/* The C-only executable deliberately does not link the C++ frontend.  The
 * vtable pass is therefore an optional extension boundary, not a second
 * implementation or a fake namespace.  Use a weak function rather than a
 * weak data reference: MinGW does not resolve an undefined weak object at
 * link time, while an undefined weak function is a valid null extension
 * point. */
#if defined(__GNUC__) || defined(__clang__)
extern CxxNamespace* cxx_namespace_global(void) __attribute__((weak));
extern bool cxx_class_virtual_base_offset(
    CxxClass*, CxxClass*, int*) __attribute__((weak));
#endif

static CxxNamespace* codegen_cxx_global_namespace(void) {
#if defined(__GNUC__) || defined(__clang__)
    return cxx_namespace_global ? cxx_namespace_global() : NULL;
#else
    return NULL;
#endif
}

static bool codegen_cxx_virtual_base_offset(CxxClass* owner,
                                             CxxClass* base,
                                             int* offset) {
#if defined(__GNUC__) || defined(__clang__)
    return cxx_class_virtual_base_offset != NULL &&
           cxx_class_virtual_base_offset(owner, base, offset);
#else
    (void)owner;
    (void)base;
    (void)offset;
    return false;
#endif
}

/* Label management */
static int label_counter = 0;
static int current_stack_offset = 0;
static Expr* active_static_reference_result_call32 = NULL;
static const char* active_static_reference_result_symbol32 = NULL;
static Decl* active_static_reference_result_decl32 = NULL;

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

static bool debug_inline_signed_integer_type(const Type* type) {
    return type && !type->is_unsigned && !type->is_volatile &&
        !type->is_reference && !type->is_atomic &&
        type->size > 0 && type->size <= 8 &&
        ((type->kind >= TYPE_BOOL && type->kind <= TYPE_LLONG) ||
         type->kind == TYPE_ENUM);
}

static bool debug_inline_unsigned_integer_type(const Type* type) {
    return type && type->is_unsigned && !type->is_volatile &&
        !type->is_reference && !type->is_atomic &&
        type->size > 0 && type->size <= 8 &&
        ((type->kind >= TYPE_BOOL && type->kind <= TYPE_LLONG) ||
         type->kind == TYPE_ENUM);
}

static bool debug_inline_integer_type(const Type* type) {
    return type && !type->is_volatile && !type->is_reference &&
        !type->is_atomic && type->size > 0 && type->size <= 8 &&
        ((type->kind >= TYPE_BOOL && type->kind <= TYPE_LLONG) ||
         type->kind == TYPE_ENUM);
}

static int64_t debug_inline_sign_extend_integer(uint64_t bits,
                                                unsigned int width) {
    if (width == 64u) {
        if (bits <= (uint64_t)INT64_MAX) return (int64_t)bits;
        if (bits == (UINT64_C(1) << 63)) return INT64_MIN;
        return -(int64_t)((~bits) + 1u);
    }
    {
        uint64_t modulus = UINT64_C(1) << width;
        uint64_t sign_bit = modulus >> 1;
        bits &= modulus - 1u;
        if ((bits & sign_bit) != 0u) {
            return -(int64_t)(modulus - bits);
        }
        return (int64_t)bits;
    }
}

static bool debug_inline_capture_converted_integer(
    const ExprList* argument, const Type* parameter_type,
    ModuleDebugInlineParameterValue* value) {
    const Type* argument_type;
    int64_t signed_value = 0;
    uint64_t unsigned_value = 0u;
    bool argument_is_unsigned;
    unsigned int width;
    if (!argument || !argument->expr || argument->cxx_temporary_owner ||
        !debug_inline_integer_type(parameter_type) ||
        !debug_inline_integer_type(argument->expr->type)) {
        return false;
    }
    argument_type = argument->expr->type;
    argument_is_unsigned = argument_type->is_unsigned;
    if (argument_is_unsigned) {
        if (!expr_eval_integer_constant_u64(
                argument->expr, &unsigned_value) &&
            !rcc_sema_eval_constexpr_integer_value(
                argument->expr, &unsigned_value, &argument_is_unsigned)) {
            return false;
        }
    } else if (!expr_eval_integer_constant(argument->expr, &signed_value)) {
        if (!rcc_sema_eval_constexpr_integer_value(
                argument->expr, &unsigned_value, &argument_is_unsigned)) {
            return false;
        }
        if (argument_is_unsigned) {
            signed_value = 0;
        } else {
            signed_value = debug_inline_sign_extend_integer(
                unsigned_value, type_integer_width(argument_type));
            unsigned_value = (uint64_t)signed_value;
        }
    } else {
        unsigned_value = (uint64_t)signed_value;
    }
    if (parameter_type->kind == TYPE_BOOL) {
        value->value = argument_is_unsigned
            ? (unsigned_value != 0u) : (signed_value != 0);
        value->has_signed_integer_constant = true;
        return true;
    }
    width = type_integer_width(parameter_type);
    if (parameter_type->is_unsigned) {
        if (width < 64u) {
            unsigned_value &= (UINT64_C(1) << width) - 1u;
        }
        value->unsigned_value = unsigned_value;
        value->has_unsigned_integer_constant = true;
        return true;
    }
    /* RinOS's i686 and AMD64 ABIs define integer narrowing and unsigned-to-
     * signed conversion by retaining the low destination-width bits and
     * interpreting them as two's-complement.  Mirror that target rule without
     * relying on a host out-of-range unsigned-to-signed cast. */
    if (!argument_is_unsigned) unsigned_value = (uint64_t)signed_value;
    value->value = debug_inline_sign_extend_integer(unsigned_value, width);
    value->has_signed_integer_constant = true;
    return true;
}

static bool debug_inline_capture_float_literal(
    const ExprList* argument, const Type* parameter_type,
    ModuleDebugInlineParameterValue* value) {
    const Type* argument_type;
    Type* evaluated_type = NULL;
    double converted_value;
    if (!argument || !argument->expr || argument->cxx_temporary_owner ||
        !parameter_type || !argument->expr->type ||
        parameter_type->is_volatile ||
        parameter_type->is_reference || parameter_type->is_atomic) {
        return false;
    }
    argument_type = argument->expr->type;
    if (!rcc_sema_eval_constexpr_floating_constant(
            argument->expr, &converted_value, &evaluated_type) ||
        !evaluated_type || evaluated_type->kind != argument_type->kind ||
        !((argument_type->kind == TYPE_FLOAT && argument_type->size == 4) ||
          (argument_type->kind == TYPE_DOUBLE &&
           argument_type->size == 8)) ||
        !((parameter_type->kind == TYPE_FLOAT && parameter_type->size == 4) ||
          (parameter_type->kind == TYPE_DOUBLE &&
           parameter_type->size == 8))) {
        return false;
    }
    if (argument_type->kind == TYPE_FLOAT) {
        if (!(converted_value <= FLT_MAX &&
              converted_value >= -FLT_MAX)) {
            return false;
        }
        converted_value = (double)(float)converted_value;
    } else if (!(converted_value <= DBL_MAX &&
                 converted_value >= -DBL_MAX)) {
        return false;
    }
    if (parameter_type->kind == TYPE_FLOAT) {
        float converted;
        uint32_t bits;
        if (!(converted_value <= FLT_MAX &&
              converted_value >= -FLT_MAX)) {
            return false;
        }
        converted = (float)converted_value;
        memcpy(&bits, &converted, sizeof(bits));
        value->float_bits = bits;
        value->float_size = (uint8_t)sizeof(bits);
    } else {
        memcpy(&value->float_bits, &converted_value,
               sizeof(converted_value));
        value->float_size = (uint8_t)sizeof(converted_value);
    }
    value->has_float_constant = true;
    return true;
}

static bool debug_inline_is_null_pointer_constant(Expr* expression) {
    Expr* value_expression = expression;
    int64_t value;
    if (!expression) return false;
    if (expression->is_cxx_nullptr ||
        (expression->type && expression->type->kind == TYPE_NULLPTR)) {
        return true;
    }
    while (value_expression && value_expression->kind == EXPR_CAST &&
           value_expression->cast_expr) {
        value_expression = value_expression->cast_expr;
        if (value_expression->is_cxx_nullptr ||
            (value_expression->type &&
             value_expression->type->kind == TYPE_NULLPTR)) {
            return true;
        }
        if (debug_inline_integer_type(value_expression->type) &&
            expr_eval_integer_constant(value_expression, &value)) {
            return value == 0;
        }
    }
    return debug_inline_integer_type(value_expression->type) &&
        expr_eval_integer_constant(value_expression, &value) && value == 0;
}

static bool debug_inline_capture_null_pointer(
    const ExprList* argument, const Type* parameter_type,
    ModuleDebugInlineParameterValue* value) {
    uint8_t address_size = g_opts.target_arch == ARCH_X64 ? 8u : 4u;
    if (!argument || !argument->expr || argument->cxx_temporary_owner ||
        !parameter_type || parameter_type->kind != TYPE_PTR ||
        parameter_type->cxx_is_member_pointer || parameter_type->is_volatile ||
        parameter_type->is_reference || parameter_type->is_atomic ||
        parameter_type->size != address_size ||
        !debug_inline_is_null_pointer_constant(argument->expr)) {
        return false;
    }
    value->has_null_pointer_constant = true;
    return true;
}

static void debug_inline_capture_parameter_values(
    ModuleDebugInlineCall* call, const Decl* callee,
    const ExprList* arguments) {
    const DeclList* parameter;
    const ExprList* argument = arguments;
    size_t count = 0u;
    size_t index = 0u;
    for (parameter = callee ? callee->func_params : NULL;
         parameter; parameter = parameter->next) {
        if (parameter->decl && parameter->decl->kind == DECL_PARAM) ++count;
    }
    if (count > SIZE_MAX / sizeof(*call->parameter_values)) {
        rcc_fatal("DWARF inline-parameter value table is too large");
    }
    call->parameter_values = count == 0u
        ? NULL : rcc_alloc(count * sizeof(*call->parameter_values));
    call->parameter_value_count = count;
    for (parameter = callee ? callee->func_params : NULL;
         parameter; parameter = parameter->next) {
        const Decl* declaration = parameter->decl;
        ModuleDebugInlineParameterValue* value;
        const Type* parameter_type;
        const Type* argument_type;
        int64_t constant;
        if (!declaration || declaration->kind != DECL_PARAM) continue;
        value = &call->parameter_values[index++];
        value->declaration = declaration;
        value->value = 0;
        value->unsigned_value = 0u;
        value->float_bits = 0u;
        value->float_size = 0u;
        value->has_null_pointer_constant = false;
        value->has_signed_integer_constant = false;
        value->has_unsigned_integer_constant = false;
        value->has_float_constant = false;
        parameter_type = declaration->type;
        argument_type = argument && argument->expr
            ? argument->expr->type : NULL;
        if (argument && !argument->cxx_temporary_owner &&
            debug_inline_signed_integer_type(parameter_type) &&
            debug_inline_signed_integer_type(argument_type) &&
            parameter_type->kind == argument_type->kind &&
            parameter_type->size == argument_type->size &&
            expr_eval_integer_constant(argument->expr, &constant)) {
            value->value = constant;
            value->has_signed_integer_constant = true;
        } else if (argument && !argument->cxx_temporary_owner &&
                   debug_inline_unsigned_integer_type(parameter_type) &&
                   debug_inline_unsigned_integer_type(argument_type) &&
                   parameter_type->kind == argument_type->kind &&
                   parameter_type->size == argument_type->size) {
            uint64_t unsigned_constant;
            if (expr_eval_integer_constant_u64(
                    argument->expr, &unsigned_constant)) {
                value->unsigned_value = unsigned_constant;
                value->has_unsigned_integer_constant = true;
            }
        } else {
            if (!debug_inline_capture_converted_integer(
                    argument, parameter_type, value)) {
                if (!debug_inline_capture_float_literal(
                        argument, parameter_type, value)) {
                    (void)debug_inline_capture_null_pointer(
                        argument, parameter_type, value);
                }
            }
        }
        if (argument) argument = argument->next;
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
    mod->debug_frame_epilogues = NULL;
    mod->debug_frame_epilogue_count = 0u;
    mod->debug_frame_epilogue_capacity = 0u;
    mod->debug_frame_saves = NULL;
    mod->debug_frame_save_count = 0u;
    mod->debug_frame_save_capacity = 0u;
    mod->debug_inline_calls = NULL;
    mod->debug_inline_call_count = 0u;
    mod->debug_inline_call_capacity = 0u;
    mod->debug_inline_active_call_plus_one = 0u;
    mod->relocs_arr = NULL;
    mod->reloc_count = 0;
    mod->reloc_capacity = 0;
    mod->got_entries = NULL;
    mod->got_entry_count = 0u;
    mod->tls_index_entries = NULL;
    mod->tls_index_entry_count = 0u;
    mod->global_initializers = NULL;
    mod->global_initializer_count = 0;
    mod->global_finalizers = NULL;
    mod->global_finalizer_count = 0;
    mod->static_local_cleanups = NULL;
    mod->cxx_array_placement_cleanup_thunks = NULL;
    mod->cxx_array_placement_cleanup_thunk_count = 0u;
    mod->compound_literal_count = 0u;
    mod->debug_ast = NULL;
    mod->debug_variable_locations = NULL;
    mod->debug_variable_location_count = 0u;
    mod->debug_verified_backend = false;
    mod->debug_statement_ranges = false;
    mod->debug_legacy_statement_lines = false;

    return mod;
}

CxxArrayPlacementCleanupThunk* module_add_cxx_array_placement_cleanup_thunk(
    Module* mod, const Expr* expression) {
    CxxArrayPlacementCleanupThunk* thunk;
    char name[96];
    if (!mod || !expression ||
        !expression->call_new_placement_deallocation_function ||
        !expression->call_new_placement_deallocation_function->link_name) {
        return NULL;
    }
    for (thunk = mod->cxx_array_placement_cleanup_thunks;
         thunk; thunk = thunk->next) {
        if (thunk->expression == expression) return thunk;
    }
    if (mod->cxx_array_placement_cleanup_thunk_count > UINT32_MAX) {
        rcc_fatal("too many C++ array placement cleanup thunks");
    }
    snprintf(name, sizeof(name), "__rcc_array_placement_delete_%u",
             (unsigned)mod->cxx_array_placement_cleanup_thunk_count);
    thunk = rcc_alloc(sizeof(*thunk));
    memset(thunk, 0, sizeof(*thunk));
    thunk->expression = expression;
    thunk->callback_name = rcc_intern(name);
    thunk->deallocation_function =
        expression->call_new_placement_deallocation_function;
    thunk->next = mod->cxx_array_placement_cleanup_thunks;
    mod->cxx_array_placement_cleanup_thunks = thunk;
    ++mod->cxx_array_placement_cleanup_thunk_count;
    return thunk;
}

size_t module_begin_debug_inline_call(
    Module* mod, const Decl* caller, const Decl* callee,
    const ExprList* arguments, SourceLoc call_location,
    uint32_t code_start,
    size_t parent_index_plus_one) {
    size_t next_capacity;
    size_t call_index;
    if (!mod || !caller || !callee ||
        parent_index_plus_one > mod->debug_inline_call_count) {
        rcc_fatal("invalid DWARF inline-call scope");
    }
    if (mod->debug_inline_call_count == mod->debug_inline_call_capacity) {
        next_capacity = mod->debug_inline_call_capacity < 8u
            ? 8u : mod->debug_inline_call_capacity * 2u;
        if (next_capacity < mod->debug_inline_call_capacity ||
            next_capacity > SIZE_MAX / sizeof(*mod->debug_inline_calls)) {
            rcc_fatal("DWARF inline-call table is too large");
        }
        mod->debug_inline_calls = rcc_realloc(
            mod->debug_inline_calls,
            next_capacity * sizeof(*mod->debug_inline_calls));
        mod->debug_inline_call_capacity = next_capacity;
    }
    call_index = mod->debug_inline_call_count++;
    ModuleDebugInlineCall* call = &mod->debug_inline_calls[call_index];
    call->caller = caller;
    call->callee = callee;
    call->arguments = arguments;
    call->parameter_values = NULL;
    call->parameter_value_count = 0u;
    debug_inline_capture_parameter_values(call, callee, arguments);
    call->call_location = call_location;
    call->code_start = code_start;
    call->code_end = code_start;
    call->parent_index_plus_one = parent_index_plus_one;
    return call_index;
}

void module_end_debug_inline_call(Module* mod, size_t call_index,
                                  uint32_t code_end) {
    if (!mod || call_index >= mod->debug_inline_call_count ||
        code_end < mod->debug_inline_calls[call_index].code_start) {
        rcc_fatal("invalid DWARF inline-call range");
    }
    mod->debug_inline_calls[call_index].code_end = code_end;
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
    for (size_t index = 0u; index < mod->debug_inline_call_count; ++index) {
        rcc_free(mod->debug_inline_calls[index].parameter_values);
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
    while (mod->static_local_cleanups) {
        StaticLocalCleanup* next = mod->static_local_cleanups->next;
        rcc_free((void*)mod->static_local_cleanups->callback_name);
        rcc_free(mod->static_local_cleanups);
        mod->static_local_cleanups = next;
    }
    while (mod->cxx_array_placement_cleanup_thunks) {
        CxxArrayPlacementCleanupThunk* next =
            mod->cxx_array_placement_cleanup_thunks->next;
        rcc_free(mod->cxx_array_placement_cleanup_thunks);
        mod->cxx_array_placement_cleanup_thunks = next;
    }
    while (mod->got_entries) {
        ModuleGotEntry* next = mod->got_entries->next;
        rcc_free((void*)mod->got_entries->target_symbol);
        rcc_free((void*)mod->got_entries->slot_symbol);
        rcc_free(mod->got_entries);
        mod->got_entries = next;
    }
    while (mod->tls_index_entries) {
        ModuleTlsIndexEntry* next = mod->tls_index_entries->next;
        rcc_free((void*)mod->tls_index_entries->target_symbol);
        rcc_free((void*)mod->tls_index_entries->slot_symbol);
        rcc_free(mod->tls_index_entries);
        mod->tls_index_entries = next;
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
    rcc_free(mod->debug_frame_epilogues);
    rcc_free(mod->debug_frame_saves);
    rcc_free(mod->debug_inline_calls);
    rcc_free(mod->relocs_arr);
    rcc_free(mod);
}

/* ═══════════════════════════════════════
 * Emit Helpers
 * ═══════════════════════════════════════ */

static void ensure_code_capacity(Module* mod, size_t needed) {
    size_t required;
    size_t capacity;
    if (needed > SIZE_MAX - mod->code.size) {
        rcc_fatal("code section exceeds host size limits");
    }
    required = mod->code.size + needed;
    if (required <= mod->code.capacity) return;
    capacity = mod->code.capacity;
    if (capacity == 0u) capacity = INIT_CAPACITY;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2u) {
            capacity = required;
            break;
        }
        capacity *= 2u;
    }
    mod->code.data = rcc_realloc(mod->code.data, capacity);
    mod->code.capacity = capacity;
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
    sym->source_file = NULL;
    sym->source_line = 0u;
    sym->source_column = 0u;
}

void module_set_symbol_source(Module* mod, const char* name, SourceLoc loc) {
    if (!mod || !name || !name[0]) return;
    for (int index = 0; index < mod->symbol_count; ++index) {
        ModuleSymbol* symbol = &mod->symbols[index];
        if (strcmp(symbol->name, name) != 0) continue;
        symbol->source_file = loc.filename;
        symbol->source_line = loc.line > 0 ? (uint32_t)loc.line : 0u;
        symbol->source_column = loc.column > 0 ? (uint32_t)loc.column : 0u;
        return;
    }
}

void module_set_symbol_size(Module* mod, const char* name, uint32_t size) {
    if (!mod || !name || !name[0]) return;
    for (int index = 0; index < mod->symbol_count; ++index) {
        ModuleSymbol* symbol = &mod->symbols[index];
        if (strcmp(symbol->name, name) == 0) {
            symbol->size = size;
            return;
        }
    }
}

void module_add_debug_frame_epilogue(Module* mod, uint32_t return_pc,
                                     uint32_t resume_pc) {
    size_t new_capacity;
    if (!mod) return;
    if (resume_pc <= return_pc) {
        rcc_fatal("debug frame epilogue range is invalid");
        return;
    }
    if (mod->debug_frame_epilogue_count >=
        mod->debug_frame_epilogue_capacity) {
        new_capacity = mod->debug_frame_epilogue_capacity == 0u
            ? 8u : mod->debug_frame_epilogue_capacity * 2u;
        if (new_capacity <= mod->debug_frame_epilogue_capacity ||
            new_capacity > SIZE_MAX / sizeof(*mod->debug_frame_epilogues)) {
            rcc_fatal("debug frame epilogue table is too large");
            return;
        }
        mod->debug_frame_epilogues = rcc_realloc(
            mod->debug_frame_epilogues,
            new_capacity * sizeof(*mod->debug_frame_epilogues));
        mod->debug_frame_epilogue_capacity = new_capacity;
    }
    mod->debug_frame_epilogues[mod->debug_frame_epilogue_count].return_pc =
        return_pc;
    mod->debug_frame_epilogues[mod->debug_frame_epilogue_count].resume_pc =
        resume_pc;
    ++mod->debug_frame_epilogue_count;
}

void module_add_debug_frame_save(Module* mod, uint32_t save_pc,
                                 uint8_t dwarf_register,
                                 uint32_t frame_offset) {
    size_t new_capacity;
    if (!mod) return;
    if (save_pc == 0u || frame_offset == 0u) {
        rcc_fatal("debug frame callee-save location is invalid");
        return;
    }
    if (mod->debug_frame_save_count >= mod->debug_frame_save_capacity) {
        new_capacity = mod->debug_frame_save_capacity == 0u
            ? 8u : mod->debug_frame_save_capacity * 2u;
        if (new_capacity <= mod->debug_frame_save_capacity ||
            new_capacity > SIZE_MAX / sizeof(*mod->debug_frame_saves)) {
            rcc_fatal("debug frame callee-save table is too large");
            return;
        }
        mod->debug_frame_saves = rcc_realloc(
            mod->debug_frame_saves,
            new_capacity * sizeof(*mod->debug_frame_saves));
        mod->debug_frame_save_capacity = new_capacity;
    }
    mod->debug_frame_saves[mod->debug_frame_save_count].save_pc = save_pc;
    mod->debug_frame_saves[mod->debug_frame_save_count].frame_offset =
        frame_offset;
    mod->debug_frame_saves[mod->debug_frame_save_count].dwarf_register =
        dwarf_register;
    ++mod->debug_frame_save_count;
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

void module_mark_symbol_weak_any(Module* mod, const char* name) {
    if (!mod || !name) return;
    for (int index = 0; index < mod->symbol_count; ++index) {
        ModuleSymbol* symbol = &mod->symbols[index];
        if (strcmp(symbol->name, name) == 0) {
            if (symbol->is_global) symbol->is_weak = true;
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
    rel->is_tls_index_module = false;
    rel->is_tls_index_offset = false;
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

const char* module_get_tls_index_entry(Module* mod, const char* target_symbol) {
    ModuleTlsIndexEntry* entry;
    uint8_t zero[16] = {0};
    char slot_name[64];
    uint32_t offset;
    if (!mod || !target_symbol || target_symbol[0] == '\0') return NULL;
    for (entry = mod->tls_index_entries; entry; entry = entry->next) {
        if (strcmp(entry->target_symbol, target_symbol) == 0) {
            return entry->slot_symbol;
        }
    }
    while ((mod->data.size & 7u) != 0u) emit_data(mod, zero, 1u);
    offset = emit_data(mod, zero, sizeof(zero));
    snprintf(slot_name, sizeof(slot_name), "__rcc_tls_index_%u",
             (unsigned int)mod->tls_index_entry_count++);
    entry = rcc_alloc(sizeof(*entry));
    entry->target_symbol = rcc_strdup(target_symbol);
    entry->slot_symbol = rcc_strdup(slot_name);
    entry->offset = offset;
    entry->next = mod->tls_index_entries;
    mod->tls_index_entries = entry;
    module_add_symbol(mod, entry->slot_symbol, offset, true,
                      MODULE_SYMBOL_DATA, false);
    module_add_relocation(mod, MODULE_SYMBOL_DATA, offset, 0u, false, true,
                          "__rcc_tls_module_slot");
    mod->relocs_arr[mod->reloc_count - 1].is_tls_index_module = true;
    module_add_relocation(mod, MODULE_SYMBOL_DATA, offset + 8u, 0u,
                          false, true, target_symbol);
    mod->relocs_arr[mod->reloc_count - 1].is_tls_index_offset = true;
    mod->relocs_arr[mod->reloc_count - 1].is_tls = true;
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

bool module_resolve_tls_relocation64(const Module* mod,
                                     ModuleSymbolSection source_section,
                                     uint32_t offset, uint64_t* value) {
    const ModuleReloc* relocation = NULL;
    const ModuleSymbol* symbol = NULL;
    uint64_t result;
    if (!mod || !value) return false;
    for (int index = 0; index < mod->reloc_count; ++index) {
        const ModuleReloc* candidate = &mod->relocs_arr[index];
        if (candidate->source_section == source_section &&
            candidate->offset == offset && candidate->is_tls_index_offset) {
            if (relocation) return false;
            relocation = candidate;
        }
    }
    if (!relocation || !relocation->symbol_name) return false;
    for (int index = 0; index < mod->symbol_count; ++index) {
        if (strcmp(mod->symbols[index].name, relocation->symbol_name) == 0) {
            symbol = &mod->symbols[index];
            break;
        }
    }
    if (!symbol || !symbol->is_defined || symbol->section != MODULE_SYMBOL_TLS) {
        return false;
    }
    result = (uint64_t)symbol->offset + relocation->target;
    if (result >= mod->tls.size) return false;
    *value = result;
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
    int64_t member_pointer_adjustment = 0;
    bool has_member_pointer_adjustment = false;
    for (Expr* cast = expression;
         cast && cast->kind == EXPR_CAST; cast = cast->cast_expr) {
        if (cast->cxx_member_pointer_adjustment_valid) {
            has_member_pointer_adjustment = true;
            break;
        }
    }
    if (!has_member_pointer_adjustment && expression && value &&
        expression->type &&
        (type_is_integer(expression->type) ||
         expression->type->kind == TYPE_ENUM) &&
        expr_eval_integer_constant(expression, value)) {
        return true;
    }
    while (expression && expression->kind == EXPR_CAST) {
        if (expression->cxx_member_pointer_adjustment_valid) {
            int64_t adjustment =
                expression->cxx_member_pointer_adjustment;
            if ((adjustment > 0 &&
                 member_pointer_adjustment > INT64_MAX - adjustment) ||
                (adjustment < 0 &&
                 member_pointer_adjustment < INT64_MIN - adjustment)) {
                return false;
            }
            member_pointer_adjustment += adjustment;
        }
        expression = expression->cast_expr;
    }
    if (!expression || !value) return false;
    if (expression->kind == EXPR_INT_LIT) {
        if ((member_pointer_adjustment > 0 &&
             expression->int_val > INT64_MAX - member_pointer_adjustment) ||
            (member_pointer_adjustment < 0 &&
             expression->int_val < INT64_MIN - member_pointer_adjustment)) {
            return false;
        }
        *value = expression->int_val + member_pointer_adjustment;
        return true;
    }
    if (expression->kind == EXPR_CHAR_LIT) {
        *value = (int64_t)expr_char_code_unit(expression);
        return true;
    }
    if (expression->kind == EXPR_SPACESHIP) {
        return expr_eval_integer_constant(expression, value);
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

static bool codegen_runtime_function_static_scalar(
    const Decl* declaration) {
    const Type* type = declaration ? declaration->type : NULL;
    if (!declaration || !type || !declaration->var_is_static_local ||
        declaration->storage != STORAGE_STATIC ||
        !rcc_parser_is_cxx_mode() || type->size <= 0) {
        return false;
    }
    return codegen_runtime_global_scalar(type) || type->kind == TYPE_PTR;
}

static bool codegen_runtime_global_constructor(const Type* type,
                                               const Expr* initializer) {
    if (!type || !type->cxx_class || !initializer) return false;
    if (rcc_parser_is_cxx_mode() &&
        rcc_parser_cxx_standard_at_least(17) &&
        (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) &&
        initializer->kind == EXPR_CALL && initializer->type &&
        type_is_compatible((Type*)type, initializer->type)) {
        return true;
    }
    return initializer->kind == EXPR_COMPOUND &&
           initializer->compound_constructor &&
           initializer->compound_constructor->method &&
           initializer->compound_constructor->method->decl &&
           initializer->compound_constructor->method->decl->func_body;
}

bool codegen_runtime_global_paren_aggregate(const Type* type,
                                           const Expr* initializer) {
    return type && initializer && rcc_parser_is_cxx_mode() &&
           rcc_parser_cxx_standard_at_least(20) && type->cxx_class &&
           (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) &&
           initializer->kind == EXPR_COMPOUND &&
           initializer->compound_paren_init &&
           !initializer->compound_constructor && type->size > 0 &&
           rcc_sema_cxx_trivially_copyable((Type*)type);
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

static void codegen_defer_global_cleanup_plan(Module* mod,
                                             CxxCleanupPlan* plan) {
    GlobalFinalizer* finalizer;
    if (!mod || !plan) return;
    finalizer = rcc_alloc(sizeof(*finalizer));
    finalizer->plan = plan;
    finalizer->next = mod->global_finalizers;
    mod->global_finalizers = finalizer;
    ++mod->global_finalizer_count;
}

static void codegen_defer_global_variable_cleanup(Module* mod,
                                                  Decl* declaration) {
    if (!mod || !declaration) return;
    if (declaration->var_cleanup) {
        codegen_defer_global_finalizer(mod, declaration->var_cleanup);
    }
    codegen_defer_global_cleanup_plan(mod, declaration->var_cleanups);
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

/* Resolve an address-of expression rooted in a static symbol.  Keeping the
 * path walk recursive allows constexpr aggregate results to rebuild pointers
 * to nested fields and array elements without turning target-layout byte
 * offsets into host addresses. */
static bool codegen_static_lvalue_address(
    Module* mod, Expr* expression, const char** symbol_name,
    uint32_t* addend) {
    Decl* target;
    int64_t index;
    if (!mod || !expression || !symbol_name || !addend) return false;
    if (expression->kind == EXPR_IDENT) {
        target = expression->ident_decl;
        if (!target || (target->kind != DECL_FUNC &&
            (target->kind != DECL_VAR ||
             (!target->var_is_global && !target->var_is_static_local)))) {
            return false;
        }
        *symbol_name = decl_link_name(target);
        return true;
    }
    if (expression->kind == EXPR_INDEX && expression->index_base &&
        expression->index_expr &&
        codegen_static_lvalue_address(
            mod, expression->index_base, symbol_name, addend) &&
        codegen_static_integer(expression->index_expr, &index)) {
        return codegen_add_static_offset(
            addend, index,
            codegen_pointer_element_size(expression->index_base->type));
    }
    if (expression->kind == EXPR_MEMBER && expression->member_base &&
        expression->member_field && expression->member_field->offset >= 0 &&
        codegen_static_lvalue_address(
            mod, expression->member_base, symbol_name, addend)) {
        if ((uint64_t)expression->member_field->offset >
            UINT32_MAX - *addend) return false;
        *addend += (uint32_t)expression->member_field->offset;
        return true;
    }
    return false;
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
        *addend = emit_string_encoded(
            mod, expression->str_val, expression->str_byte_length,
            expression->str_element_width);
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
        if (codegen_static_lvalue_address(
                mod, addressed, symbol_name, addend)) {
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
        expression->ident_decl->kind == DECL_VAR &&
        expression->ident_decl->var_is_constexpr &&
        expression->ident_decl->type &&
        expression->ident_decl->type->kind == TYPE_PTR &&
        expression->ident_decl->var_init) {
        return codegen_static_address(mod, expression->ident_decl->var_init,
                                      symbol_name, addend);
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

static bool codegen_static_member_function_pointer_adjustment(
    Expr* initializer, uint32_t width, int64_t* adjustment) {
    int64_t total = 0;
    if (!initializer || !adjustment || (width != 4u && width != 8u)) {
        return false;
    }
    for (Expr* current = initializer; current;
         current = current->kind == EXPR_CAST ? current->cast_expr : NULL) {
        int64_t step;
        if (!current->cxx_member_pointer_adjustment_valid) continue;
        step = current->cxx_member_pointer_adjustment;
        if ((step > 0 && total > INT64_MAX - step) ||
            (step < 0 && total < INT64_MIN - step)) {
            return false;
        }
        total += step;
    }
    if (width == 4u && (total < INT32_MIN || total > INT32_MAX)) {
        return false;
    }
    *adjustment = total;
    return true;
}

static void codegen_write_member_function_pointer_adjustment(
    uint8_t* storage, uint32_t offset, uint32_t width,
    int64_t adjustment) {
    uint64_t bits = (uint64_t)adjustment;
    for (uint32_t byte = 0u; byte < width; ++byte) {
        storage[offset + byte] = (uint8_t)(bits >> (byte * 8u));
    }
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
    if (type->cxx_is_member_pointer) {
        if (type_is_cxx_member_function_pointer(type)) {
            TypeMethod* method =
                initializer->cxx_member_function_pointer_method;
            Decl* function = method ? method->function_decl : NULL;
            int64_t this_adjustment;
            if (!function && initializer->kind == EXPR_ADDR &&
                initializer->unary_operand) {
                function = initializer->unary_operand->ident_decl;
            }
            if (!method && (initializer->is_cxx_nullptr ||
                (initializer->type &&
                 initializer->type->kind == TYPE_NULLPTR) ||
                (initializer->kind == EXPR_INT_LIT &&
                 initializer->int_val == 0))) {
                memset(mod->data.data + offset, 0, (size_t)type->size);
                return true;
            }
            if (!method || type->size < (int)(width * 2u) ||
                (!method->is_virtual &&
                 (!function || !function->link_name)) ||
                (method->is_virtual && method->vtable_index < 0) ||
                !codegen_static_member_function_pointer_adjustment(
                    initializer, width, &this_adjustment)) {
                return false;
            }
            if (method->is_virtual) {
                integer = (int64_t)method->vtable_index * (int64_t)width + 1;
                for (uint32_t byte = 0u; byte < width; ++byte) {
                    mod->data.data[offset + byte] =
                        (uint8_t)((uint64_t)integer >> (byte * 8u));
                }
            } else {
                module_add_relocation(mod, MODULE_SYMBOL_DATA, offset, 0u,
                                      false, width == 8u,
                                      decl_link_name(function));
                add_reloc(mod, MODULE_SYMBOL_DATA, offset,
                          width == 8u ? RIN_RELOC_ABS64
                                      : RIN_RELOC_ABS32);
            }
            codegen_write_member_function_pointer_adjustment(
                mod->data.data, offset + width, width, this_adjustment);
            return true;
        }
        uint64_t bits;
        if (initializer->is_cxx_nullptr ||
            (initializer->type &&
             initializer->type->kind == TYPE_NULLPTR)) {
            integer = -1;
        } else if (!codegen_static_integer(initializer, &integer)) {
            return false;
        }
        bits = (uint64_t)integer;
        for (uint32_t byte = 0u; byte < width; ++byte) {
            mod->data.data[offset + byte] =
                (uint8_t)(bits >> (byte * 8u));
        }
        return true;
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
    if (!type || type->kind != TYPE_ARRAY || !type->base || !initializer) {
        return NULL;
    }
    Expr* string = initializer->kind == EXPR_STRING_LIT ? initializer : NULL;
    if (initializer->kind == EXPR_COMPOUND && initializer->compound_init &&
        !initializer->compound_init->next &&
        initializer->compound_init->designator_kind == INIT_DESIGNATOR_NONE &&
        initializer->compound_init->expr &&
        initializer->compound_init->expr->kind == EXPR_STRING_LIT) {
        string = initializer->compound_init->expr;
    }
    if (!string) return NULL;
    uint8_t width = string->str_element_width
        ? string->str_element_width : 1u;
    TypeKind expected_kind = width == 1u ? TYPE_CHAR
        : width == 2u ? TYPE_SHORT : width == 4u ? TYPE_INT : TYPE_VOID;
    return type->base->kind == expected_kind &&
           type->base->size == width ? string : NULL;
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
    uint64_t mask;
    uint64_t shifted_mask;
    uint64_t storage = 0u;
    int64_t value;
    size_t width;
    if (!data || !field || !field->is_bitfield || !field->type ||
        !initializer || field->type->size <= 0 || field->type->size > 8) {
        return false;
    }
    width = (size_t)field->type->size;
    if ((size_t)offset > data_size || width > data_size - (size_t)offset ||
        !codegen_static_integer(initializer, &value)) {
        return false;
    }
    mask = field->bit_width >= 64u
        ? UINT64_MAX : (UINT64_C(1) << field->bit_width) - UINT64_C(1);
    shifted_mask = mask << field->bit_offset;
    for (size_t byte = 0u; byte < width; ++byte) {
        storage |= (uint64_t)data[offset + byte] << (byte * 8u);
    }
    storage = (storage & ~shifted_mask) |
              (((uint64_t)value & mask) << field->bit_offset);
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
        (type->kind != TYPE_ARRAY && type->kind != TYPE_VECTOR &&
         type->kind != TYPE_STRUCT && type->kind != TYPE_UNION)) {
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
    /* A C++ constructor is executable semantics, even when its argument
     * layout happens to resemble the class fields.  Defer it to the module
     * initializer so a body such as `field = value + 1` is not silently
     * replaced by a bytewise aggregate copy. */
    if (initializer->kind == EXPR_COMPOUND &&
        initializer->compound_constructor) {
        return false;
    }
    if (type->kind == TYPE_PTR && type->cxx_is_member_pointer &&
        codegen_aggregate_zero_initializer(type, initializer)) {
        memset(mod->data.data + offset, 0xFF, (size_t)type->size);
        return true;
    }
    if (codegen_aggregate_zero_initializer(type, initializer)) return true;
    string = codegen_character_array_string(type, initializer);
    if (string) {
        size_t text_size = string->str_byte_length +
            (string->str_element_width ? string->str_element_width : 1u);
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
            if (ast_cxx_is_aggregate(type)) {
                int element_index = 0;
                for (ExprList* item = initializer->compound_init; item;
                     item = item->next) {
                    Type* element_type = NULL;
                    TypeField* field = NULL;
                    int element_offset = 0;
                    uint64_t field_offset;
                    if (item->designator_kind == INIT_DESIGNATOR_FIELD) {
                        field = ast_cxx_aggregate_member(
                            type, item->designator_field);
                        if (!field) return false;
                        element_type = field->type;
                        element_offset = field->offset;
                    } else if (item->designator_kind !=
                                   INIT_DESIGNATOR_NONE ||
                               !ast_cxx_aggregate_element(
                                   type, element_index++, &element_type,
                                   &element_offset, &field)) {
                        return false;
                    }
                    if (!element_type || element_offset < 0 ||
                        (uint64_t)element_offset > UINT32_MAX - offset) {
                        return false;
                    }
                    field_offset = (uint64_t)offset +
                                   (uint64_t)element_offset;
                    if (field && field->is_bitfield
                            ? !codegen_pack_static_bitfield(
                                  mod->data.data, mod->data.size, field,
                                  item->expr, (uint32_t)field_offset)
                            : !codegen_emit_static_initializer(
                                  mod, element_type, item->expr,
                                  (uint32_t)field_offset)) {
                        return false;
                    }
                }
                return true;
            }
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
    /* C++ aggregate brace elision can pass a scalar initializer directly to
     * a single-element aggregate subobject.  Follow its only aggregate
     * element so inherited bases such as `struct D : B { int tail; }` retain
     * the initializer for B's sole member when the parser flattens braces. */
    if ((type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) &&
        ast_cxx_is_aggregate(type) &&
        ast_cxx_aggregate_element_count(type) == 1) {
        Type* element_type = NULL;
        TypeField* field = NULL;
        int element_offset = 0;
        uint64_t element_location;
        if (!ast_cxx_aggregate_element(
                type, 0, &element_type, &element_offset, &field) ||
            !element_type || element_offset < 0 ||
            (uint64_t)element_offset > UINT32_MAX - offset) {
            return false;
        }
        element_location = (uint64_t)offset + (uint64_t)element_offset;
        if (field && field->is_bitfield) {
            return codegen_pack_static_bitfield(
                mod->data.data, mod->data.size, field, initializer,
                (uint32_t)element_location);
        }
        return codegen_emit_static_initializer(
            mod, element_type, initializer, (uint32_t)element_location);
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
        size_t text_size = string->str_byte_length +
            (string->str_element_width ? string->str_element_width : 1u);
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
            if (ast_cxx_is_aggregate(type)) {
                int element_index = 0;
                for (ExprList* item = initializer->compound_init; item;
                     item = item->next) {
                    Type* element_type = NULL;
                    TypeField* field = NULL;
                    int element_offset = 0;
                    uint64_t field_offset;
                    if (item->designator_kind == INIT_DESIGNATOR_FIELD) {
                        field = ast_cxx_aggregate_member(
                            type, item->designator_field);
                        if (!field) return false;
                        element_type = field->type;
                        element_offset = field->offset;
                    } else if (item->designator_kind !=
                                   INIT_DESIGNATOR_NONE ||
                               !ast_cxx_aggregate_element(
                                   type, element_index++, &element_type,
                                   &element_offset, &field)) {
                        return false;
                    }
                    if (!element_type || element_offset < 0 ||
                        (uint64_t)element_offset > UINT32_MAX - offset) {
                        return false;
                    }
                    field_offset = (uint64_t)offset +
                                   (uint64_t)element_offset;
                    if (field && field->is_bitfield
                            ? !codegen_pack_static_bitfield(
                                  mod->tls.data, mod->tls.size, field,
                                  item->expr, (uint32_t)field_offset)
                            : !codegen_emit_tls_initializer(
                                  mod, element_type, item->expr,
                                  (uint32_t)field_offset)) {
                        return false;
                    }
                }
                return true;
            }
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
        if (type->cxx_is_member_pointer) {
            if (type_is_cxx_member_function_pointer(type)) {
                TypeMethod* method =
                    initializer->cxx_member_function_pointer_method;
                Decl* function = method ? method->function_decl : NULL;
                uint32_t width =
                    g_opts.target_arch == ARCH_X64 ? 8u : 4u;
                int64_t this_adjustment;
                if (!function && initializer->kind == EXPR_ADDR &&
                    initializer->unary_operand) {
                    function = initializer->unary_operand->ident_decl;
                }
                if (!method && (initializer->is_cxx_nullptr ||
                    (initializer->type &&
                     initializer->type->kind == TYPE_NULLPTR) ||
                    (initializer->kind == EXPR_INT_LIT &&
                     initializer->int_val == 0))) {
                    memset(mod->tls.data + offset, 0,
                           (size_t)type->size);
                    return true;
                }
                if (!method || type->size < (int)(width * 2u) ||
                    (!method->is_virtual &&
                     (!function || !function->link_name)) ||
                    (method->is_virtual && method->vtable_index < 0) ||
                    !codegen_static_member_function_pointer_adjustment(
                        initializer, width, &this_adjustment)) {
                    return false;
                }
                if (method->is_virtual) {
                    constant = (int64_t)method->vtable_index * width + 1;
                    for (uint32_t byte = 0u; byte < width; ++byte) {
                        mod->tls.data[offset + byte] =
                            (uint8_t)((uint64_t)constant >> (byte * 8u));
                    }
                } else {
                    module_add_relocation(mod, MODULE_SYMBOL_TLS, offset,
                                          0u, false, width == 8u,
                                          decl_link_name(function));
                    add_reloc(mod, MODULE_SYMBOL_TLS, offset,
                              width == 8u ? RIN_RELOC_ABS64
                                          : RIN_RELOC_ABS32);
                }
                codegen_write_member_function_pointer_adjustment(
                    mod->tls.data, offset + width, width, this_adjustment);
                return true;
            }
            if (initializer->is_cxx_nullptr ||
                (initializer->type &&
                 initializer->type->kind == TYPE_NULLPTR)) {
                constant = -1;
            } else if (!codegen_static_integer(initializer, &constant)) {
                return false;
            }
            for (uint32_t byte = 0u; byte < (uint32_t)type->size; ++byte) {
                mod->tls.data[offset + byte] =
                    (uint8_t)((uint64_t)constant >> (byte * 8u));
            }
            return true;
        }
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
                    (cls && (cls->secondary_vtable_count > 0 ||
                             cls->virtual_base_count > 0)));
}

static bool codegen_emit_static_initializer_guard_storage(
    Module* mod, Decl* declaration) {
    Decl* guard = declaration
        ? declaration->var_reference_temporary_guard : NULL;
    const uint32_t guard_size = 8u;
    const uint32_t guard_alignment = 8u;
    uint64_t aligned;
    uint32_t offset;
    if (!mod || !declaration) return false;
    if (!guard) return true;
    if (declaration->var_is_thread_local) {
        aligned = ((uint64_t)mod->tls.size + guard_alignment - 1u) &
                  ~((uint64_t)guard_alignment - 1u);
        if (aligned > INT_MAX || guard_size > UINT32_MAX - aligned) {
            rcc_error(declaration->loc,
                      "thread-local initialization guard exceeds TLS limits");
            return false;
        }
        codegen_ensure_tls_capacity(mod, (size_t)aligned + guard_size);
        if (mod->tls.size < aligned) {
            memset(mod->tls.data + mod->tls.size, 0,
                   (size_t)aligned - mod->tls.size);
        }
        offset = (uint32_t)aligned;
        memset(mod->tls.data + offset, 0, guard_size);
        mod->tls.size = (size_t)aligned + guard_size;
        if (guard_alignment > mod->tls_align) {
            mod->tls_align = guard_alignment;
        }
        guard->var_offset = (int)offset;
        module_add_symbol(mod, decl_link_name(guard), offset, true,
                          MODULE_SYMBOL_TLS, false);
        return true;
    }
    aligned = ((uint64_t)mod->bss.size + guard_alignment - 1u) &
              ~((uint64_t)guard_alignment - 1u);
    if (aligned > INT_MAX || guard_size > UINT32_MAX - aligned) {
        rcc_error(declaration->loc,
                  "static initialization guard exceeds BSS limits");
        return false;
    }
    offset = (uint32_t)aligned;
    mod->bss.size = (size_t)(aligned + guard_size);
    if (guard_alignment > mod->bss.align) {
        mod->bss.align = guard_alignment;
    }
    guard->var_offset = (int)offset;
    module_add_symbol(mod, decl_link_name(guard), offset, true,
                      MODULE_SYMBOL_BSS, false);
    return true;
}

static bool codegen_emit_static_reference_temporary_storage(
    Module* mod, Decl* declaration) {
    Decl* owner = declaration
        ? declaration->var_reference_temporary_owner : NULL;
    uint8_t zero[32] = {0};
    uint32_t size;
    uint32_t alignment;
    uint64_t aligned;
    uint32_t offset;
    if (!mod || !owner || !owner->type || owner->type->size <= 0) {
        return false;
    }
    size = owner->var_reference_temporary_storage_size > 0
        ? (uint32_t)owner->var_reference_temporary_storage_size
        : (uint32_t)owner->type->size;
    alignment = owner->var_reference_temporary_storage_align > 0
        ? (uint32_t)owner->var_reference_temporary_storage_align
        : owner->type->align > 0 ? (uint32_t)owner->type->align : 1u;
    if (alignment > 16u) alignment = 16u;
    if (owner->var_is_thread_local) {
        aligned = ((uint64_t)mod->tls.size + alignment - 1u) &
                  ~((uint64_t)alignment - 1u);
        if (aligned > UINT32_MAX || size > UINT32_MAX - aligned) {
            rcc_error(declaration->loc,
                      "thread-local reference temporary exceeds TLS limits");
            return false;
        }
        codegen_ensure_tls_capacity(mod, (size_t)aligned + size);
        if (mod->tls.size < aligned) {
            memset(mod->tls.data + mod->tls.size, 0,
                   (size_t)aligned - mod->tls.size);
        }
        offset = (uint32_t)aligned;
        memset(mod->tls.data + offset, 0, size);
        mod->tls.size = (size_t)aligned + size;
        if (alignment > mod->tls_align) mod->tls_align = alignment;
        owner->var_offset = (int)offset;
        codegen_add_vtable_pointer(mod, MODULE_SYMBOL_TLS, offset,
                                   owner->type);
        module_add_symbol(mod, decl_link_name(owner), offset, true,
                          MODULE_SYMBOL_TLS, false);

        return codegen_emit_static_initializer_guard_storage(
            mod, declaration);
    }
    aligned = ((uint64_t)mod->data.size + alignment - 1u) &
              ~((uint64_t)alignment - 1u);
    if (aligned > UINT32_MAX || size > UINT32_MAX - aligned) {
        rcc_error(declaration->loc,
                  "static reference temporary exceeds data limits");
        return false;
    }
    while (mod->data.size < aligned) emit_data(mod, zero, 1u);
    offset = (uint32_t)aligned;
    while (size > sizeof(zero)) {
        emit_data(mod, zero, sizeof(zero));
        size -= sizeof(zero);
    }
    if (size) emit_data(mod, zero, size);
    owner->var_offset = offset;
    codegen_add_vtable_pointer(mod, MODULE_SYMBOL_DATA, offset, owner->type);
    module_add_symbol(mod, decl_link_name(owner), offset, true,
                      MODULE_SYMBOL_DATA, false);

    return codegen_emit_static_initializer_guard_storage(mod, declaration);
}

static void codegen_defer_static_local_cleanup(Module* mod,
                                               Decl* declaration) {
    StaticLocalCleanup* cleanup;
    StaticLocalCleanup** tail;
    Decl* cleanup_object;
    const char* owner_name;
    const char* suffix = "$rcc_static_cleanup";
    size_t owner_length;
    size_t suffix_length = strlen(suffix);
    char* callback_name;
    if (!mod || !declaration || !declaration->var_cleanups ||
        (!declaration->var_reference_temporary_owner &&
         !declaration->var_is_thread_local &&
         !declaration->var_is_static_local)) {
        return;
    }
    for (cleanup = mod->static_local_cleanups; cleanup;
         cleanup = cleanup->next) {
        if (cleanup->declaration == declaration) return;
    }
    cleanup_object = declaration->var_reference_temporary_owner
        ? declaration->var_reference_temporary_owner : declaration;
    owner_name = decl_link_name(cleanup_object);
    owner_length = owner_name ? strlen(owner_name) : 0u;
    if (owner_length > SIZE_MAX - suffix_length - 1u) {
        rcc_error(declaration->loc,
                  "static cleanup callback symbol is too large");
        return;
    }
    callback_name = rcc_alloc(owner_length + suffix_length + 1u);
    if (owner_length) memcpy(callback_name, owner_name, owner_length);
    memcpy(callback_name + owner_length, suffix, suffix_length + 1u);
    cleanup = rcc_alloc(sizeof(*cleanup));
    cleanup->declaration = declaration;
    cleanup->callback_name = callback_name;
    cleanup->next = NULL;
    tail = &mod->static_local_cleanups;
    while (*tail) tail = &(*tail)->next;
    *tail = cleanup;
}

static bool codegen_attach_static_initializer_guard(Decl* declaration) {
    const char* base;
    const char* suffix = "$rcc_static_guard";
    size_t base_length;
    size_t suffix_length = strlen(suffix);
    char* guard_name;
    Decl* guard;
    if (!declaration || declaration->var_reference_temporary_guard) {
        return declaration != NULL;
    }
    base = decl_link_name(declaration);
    base_length = base ? strlen(base) : 0u;
    if (base_length > SIZE_MAX - suffix_length - 1u) {
        rcc_error(declaration->loc,
                  "static initialization guard symbol is too large");
        return false;
    }
    /* The synthetic declaration remains attached to the AST until the
     * compilation context is released.  Keep its generated link name in the
     * same arena instead of leaking a standalone allocation per guarded
     * static local. */
    guard_name = ast_arena_alloc(base_length + suffix_length + 1u);
    if (base_length) memcpy(guard_name, base, base_length);
    memcpy(guard_name + base_length, suffix, suffix_length + 1u);
    guard = decl_var("__rcc_static_guard", type_llong, NULL,
                     declaration->loc);
    guard->name = guard_name;
    guard->link_name = guard_name;
    guard->storage = STORAGE_STATIC;
    guard->var_is_global = true;
    guard->var_is_thread_local = declaration->var_is_thread_local;
    declaration->var_reference_temporary_guard = guard;
    return true;
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
    if (declaration->var_reference_temporary_owner) {
        if (!codegen_emit_static_reference_temporary_storage(
                mod, declaration)) {
            return false;
        }
        codegen_defer_static_local_cleanup(mod, declaration);
        if (!declaration->var_is_thread_local) {
            alignment = declaration->type->align > 0
                ? (uint32_t)declaration->type->align : 1u;
            if (alignment > 16u) alignment = 16u;
            while ((mod->data.size & (alignment - 1u)) != 0u) {
                emit_data(mod, zero, 1u);
            }
            offset = (uint32_t)mod->data.size;
            size = (uint32_t)declaration->type->size;
            while (size > sizeof(zero)) {
                emit_data(mod, zero, sizeof(zero));
                size -= sizeof(zero);
            }
            if (size) emit_data(mod, zero, size);
            declaration->var_offset = offset;
            module_add_symbol(mod, decl_link_name(declaration), offset, true,
                              MODULE_SYMBOL_DATA,
                              declaration->storage != STORAGE_STATIC);
            return true;
        }
    }
    if (!declaration->var_reference_temporary_owner &&
        (declaration->var_is_thread_local ||
         declaration->var_reference_temporary_guard)) {
        if (!codegen_emit_static_initializer_guard_storage(
                mod, declaration)) {
            return false;
        }
        codegen_defer_static_local_cleanup(mod, declaration);
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
        if (!declaration->var_init &&
            declaration->type->kind == TYPE_PTR &&
            declaration->type->cxx_is_member_pointer &&
            !type_is_cxx_member_function_pointer(declaration->type)) {
            memset(mod->tls.data + offset, 0xFF, size);
        }
        mod->tls.size = (size_t)aligned + size;
        if (alignment > mod->tls_align) mod->tls_align = alignment;
        declaration->var_offset = offset;
        declaration->var_dynamic_initializer = false;
        if (declaration->var_init &&
            !declaration->var_reference_temporary_owner &&
            (codegen_runtime_global_constructor(
                 declaration->type, declaration->var_init) ||
             !codegen_emit_tls_initializer(
                 mod, declaration->type, declaration->var_init, offset))) {
            if (!declaration->var_reference_temporary_guard &&
                codegen_runtime_function_static_scalar(declaration)) {
                if (!codegen_attach_static_initializer_guard(declaration) ||
                    !codegen_emit_static_initializer_guard_storage(
                        mod, declaration)) {
                    return false;
                }
            }
            if (declaration->var_reference_temporary_guard &&
                (codegen_runtime_function_static_scalar(declaration) ||
                 codegen_runtime_global_constructor(
                     declaration->type, declaration->var_init))) {
                memset(mod->tls.data + offset, 0, size);
                declaration->var_dynamic_initializer = true;
            } else {
                rcc_error(declaration->loc,
                          "unsupported static local TLS initializer for '%s'",
                          declaration->name);
                return false;
            }
        }
        codegen_add_vtable_pointer(mod, MODULE_SYMBOL_TLS, offset,
                                   declaration->type);
        module_add_symbol(mod, decl_link_name(declaration), offset, true,
                          MODULE_SYMBOL_TLS, false);
        return true;
    }
    if (!declaration->var_init) {
        if (declaration->type->kind == TYPE_PTR &&
            declaration->type->cxx_is_member_pointer &&
            !declaration->type->is_reference &&
            !type_is_cxx_member_function_pointer(declaration->type)) {
            while ((mod->data.size & (alignment - 1u)) != 0u) {
                emit_data(mod, zero, 1u);
            }
            offset = (uint32_t)mod->data.size;
            for (uint32_t byte = 0u; byte < size; ++byte) {
                const uint8_t null_byte = 0xFFu;
                emit_data(mod, &null_byte, sizeof(null_byte));
            }
            declaration->var_offset = offset;
            module_add_symbol(mod, decl_link_name(declaration), offset, true,
                              MODULE_SYMBOL_DATA, false);
            return true;
        }
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
    declaration->var_dynamic_initializer = false;
    if (!codegen_emit_static_initializer(
            mod, declaration->type, declaration->var_init, offset)) {
        if (!declaration->var_reference_temporary_guard &&
            codegen_runtime_function_static_scalar(declaration)) {
            if (!codegen_attach_static_initializer_guard(declaration) ||
                !codegen_emit_static_initializer_guard_storage(
                    mod, declaration)) {
                return false;
            }
        }
        if (declaration->var_reference_temporary_guard &&
            (codegen_runtime_function_static_scalar(declaration) ||
             codegen_runtime_global_constructor(
                 declaration->type, declaration->var_init) ||
             codegen_runtime_global_paren_aggregate(
                 declaration->type, declaration->var_init))) {
            size = (uint32_t)declaration->type->size;
            memset(mod->data.data + offset, 0, size);
            declaration->var_dynamic_initializer = true;
        } else {
            rcc_error(declaration->loc,
                      "static local initializer for '%s' is not a supported constant or guarded runtime initializer",
                      declaration->name);
            return false;
        }
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
                    if (statement->decl->is_weak) {
                        module_mark_symbol_weak_any(
                            mod, decl_link_name(statement->decl));
                    }
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
            if (declaration->var_reference_temporary_owner) {
                if (!codegen_emit_static_reference_temporary_storage(
                        mod, declaration)) {
                    continue;
                }
                codegen_defer_static_local_cleanup(mod, declaration);
            } else if (declaration->var_reference_temporary_guard) {
                if (!codegen_emit_static_initializer_guard_storage(
                        mod, declaration)) {
                    continue;
                }
                codegen_defer_static_local_cleanup(mod, declaration);
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
            if (!declaration->var_init &&
                declaration->type->kind == TYPE_PTR &&
                declaration->type->cxx_is_member_pointer &&
                !type_is_cxx_member_function_pointer(declaration->type)) {
                memset(mod->tls.data + offset, 0xFF, size);
            }
            mod->tls.size = (size_t)aligned + size;
            if (alignment > mod->tls_align) mod->tls_align = alignment;
            declaration->var_offset = offset;
            declaration->var_dynamic_initializer = false;
            if (declaration->var_init &&
                !declaration->var_reference_temporary_owner &&
                (codegen_runtime_global_constructor(
                     declaration->type, declaration->var_init) ||
                 !codegen_emit_tls_initializer(
                     mod, declaration->type, declaration->var_init, offset))) {
                if (declaration->var_reference_temporary_guard &&
                    (codegen_runtime_global_scalar(declaration->type) ||
                     codegen_runtime_global_constructor(
                         declaration->type, declaration->var_init))) {
                    memset(mod->tls.data + offset, 0, size);
                    declaration->var_dynamic_initializer = true;
                } else {
                    rcc_error(declaration->loc,
                              "unsupported thread-local initializer for '%s'",
                              declaration->name);
                }
            }
            codegen_add_vtable_pointer(mod, MODULE_SYMBOL_TLS, offset,
                                       declaration->type);
            if (declaration->var_cleanups &&
                !declaration->var_reference_temporary_owner &&
                !declaration->var_reference_temporary_guard) {
                rcc_error(declaration->loc,
                          "thread-local destructor has no initialization guard");
            }
            module_add_symbol(mod, decl_link_name(declaration), offset, true,
                              MODULE_SYMBOL_TLS,
                              declaration->storage != STORAGE_STATIC);
            if (declaration->var_is_inline || declaration->is_weak) {
                module_mark_symbol_weak(mod, decl_link_name(declaration));
            }
            continue;
        }
        if (declaration->var_reference_temporary_owner) {
            uint64_t aligned = ((uint64_t)mod->data.size + alignment - 1u) &
                               ~((uint64_t)alignment - 1u);
            if (aligned > UINT32_MAX || size > UINT32_MAX - aligned) {
                rcc_error(declaration->loc,
                          "static reference object exceeds data limits");
                continue;
            }
            while (mod->data.size < aligned) emit_data(mod, zero, 1u);
            offset = (uint32_t)aligned;
            while (size > sizeof(zero)) {
                emit_data(mod, zero, sizeof(zero));
                size -= sizeof(zero);
            }
            if (size) emit_data(mod, zero, size);
            declaration->var_offset = offset;
            if (!codegen_emit_static_reference_temporary_storage(
                    mod, declaration)) {
                continue;
            }
            module_add_symbol(mod, decl_link_name(declaration), offset, true,
                              MODULE_SYMBOL_DATA,
                              declaration->storage != STORAGE_STATIC);
            if (declaration->var_is_inline || declaration->is_weak) {
                module_mark_symbol_weak(mod, decl_link_name(declaration));
            }
            codegen_defer_global_initializer(mod, declaration);
            codegen_defer_global_variable_cleanup(mod, declaration);
            continue;
        }
        if (declaration->storage == STORAGE_EXTERN &&
            !declaration->var_init) {
            module_add_symbol(mod, decl_link_name(declaration), 0u, false,
                              MODULE_SYMBOL_DATA, true);
            if (declaration->is_weak) {
                module_mark_symbol_weak_any(mod, decl_link_name(declaration));
            }
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
            if (declaration->var_is_inline || declaration->is_weak) {
                module_mark_symbol_weak(mod, decl_link_name(declaration));
            }
            codegen_defer_global_variable_cleanup(mod, declaration);
            continue;
        }
        if (!declaration->var_init) {
            if (declaration->type->kind == TYPE_PTR &&
                declaration->type->cxx_is_member_pointer &&
                !declaration->type->is_reference &&
                !type_is_cxx_member_function_pointer(declaration->type)) {
                uint64_t aligned = ((uint64_t)mod->data.size + alignment - 1u) &
                                   ~((uint64_t)alignment - 1u);
                if (aligned > UINT32_MAX || size > UINT32_MAX - aligned) {
                    rcc_error(declaration->loc,
                              "member-pointer initializer exceeds data limits");
                    continue;
                }
                while (mod->data.size < aligned) emit_data(mod, zero, 1u);
                offset = (uint32_t)aligned;
                for (uint32_t byte = 0u; byte < size; ++byte) {
                    const uint8_t null_byte = 0xFFu;
                    emit_data(mod, &null_byte, sizeof(null_byte));
                }
                declaration->var_offset = offset;
                module_add_symbol(mod, decl_link_name(declaration), offset,
                                  true, MODULE_SYMBOL_DATA,
                                  declaration->storage != STORAGE_STATIC);
                if (declaration->var_is_inline || declaration->is_weak) {
                    module_mark_symbol_weak(mod, decl_link_name(declaration));
                }
                codegen_defer_global_variable_cleanup(mod, declaration);
                continue;
            }
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
            if (declaration->var_is_inline || declaration->is_weak) {
                module_mark_symbol_weak(mod, decl_link_name(declaration));
            }
            codegen_defer_global_variable_cleanup(mod, declaration);
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
            if (codegen_runtime_global_scalar(declaration->type) ||
                codegen_runtime_global_constructor(declaration->type,
                                                   declaration->var_init) ||
                codegen_runtime_global_paren_aggregate(
                    declaration->type, declaration->var_init)) {
                codegen_defer_global_initializer(mod, declaration);
            } else {
                rcc_error(declaration->loc,
                          "unsupported static initializer for '%s'",
                          declaration->name);
            }
        }
        codegen_defer_global_variable_cleanup(mod, declaration);
        codegen_add_vtable_pointer(mod, MODULE_SYMBOL_DATA, offset,
                                   declaration->type);
        module_add_symbol(mod, decl_link_name(declaration), offset, true,
                          MODULE_SYMBOL_DATA,
                          declaration->storage != STORAGE_STATIC);
        if (declaration->var_is_inline || declaration->is_weak) {
            module_mark_symbol_weak(mod, decl_link_name(declaration));
        }
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

uint32_t emit_string(Module* mod, const char* str, size_t length) {
    return emit_string_encoded(mod, str, length, 1u);
}

uint32_t emit_string_encoded(Module* mod, const char* str,
                             size_t byte_length, uint8_t element_width) {
    size_t padding;
    size_t stored_size;
    if (!mod || !str || (element_width != 1u && element_width != 2u &&
                         element_width != 4u) ||
        byte_length % element_width != 0u ||
        byte_length > SIZE_MAX - element_width) {
        rcc_fatal("invalid encoded string passed to code generator");
    }
    /* Check if already exists */
    for (StringLit* s = mod->strings; s; s = s->next) {
        if (s->length == byte_length &&
            s->terminator_width == element_width &&
            memcmp(s->value, str, byte_length) == 0) {
            return s->offset;
        }
    }

    padding = (element_width - mod->rodata.size % element_width) %
              element_width;
    stored_size = byte_length + element_width;
    if (stored_size > SIZE_MAX - padding) {
        rcc_fatal("encoded string size overflows read-only data");
    }
    /* Add new string */
    ensure_rodata_capacity(mod, padding + stored_size);
    memset(mod->rodata.data + mod->rodata.size, 0, padding);
    mod->rodata.size += padding;
    uint32_t offset = (uint32_t)mod->rodata.size;

    memcpy(mod->rodata.data + mod->rodata.size, str, byte_length);
    memset(mod->rodata.data + mod->rodata.size + byte_length, 0,
           element_width);
    mod->rodata.size += stored_size;

    /* Record in string list */
    StringLit* lit = rcc_alloc(sizeof(StringLit));
    lit->value = str;
    lit->length = byte_length;
    lit->terminator_width = element_width;
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

static void emit_add_reg_reg_width32(Module* mod, int dst, int src,
                                     int width) {
    if (width == 1) {
        emit_byte(mod, 0x00);
    } else {
        if (width == 2) emit_byte(mod, 0x66);
        emit_byte(mod, 0x01);
    }
    emit_byte(mod, modrm(3, src, dst));
}

static void emit_sub_reg_reg_width32(Module* mod, int dst, int src,
                                     int width) {
    if (width == 1) {
        emit_byte(mod, 0x28);
    } else {
        if (width == 2) emit_byte(mod, 0x66);
        emit_byte(mod, 0x29);
    }
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

static void emit_imul_reg(Module* mod, int reg) {
    emit_byte(mod, 0xF7);
    emit_byte(mod, modrm(3, 5, reg));
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

static void emit_bswap_reg32(Module* mod, int reg) {
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0xC8 + (uint8_t)reg);
}

static void emit_bsf_reg_reg(Module* mod, int dst, int src) {
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0xBC);
    emit_byte(mod, modrm(3, dst, src));
}

static void emit_bsr_reg_reg(Module* mod, int dst, int src) {
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0xBD);
    emit_byte(mod, modrm(3, dst, src));
}

static void emit_rol_ax_8(Module* mod) {
    emit_byte(mod, 0x66);
    emit_byte(mod, 0xC1);
    emit_byte(mod, modrm(3, 0, EAX));
    emit_byte(mod, 8);
}

static void emit_movzx_eax_ax(Module* mod) {
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0xB7);
    emit_byte(mod, modrm(3, EAX, EAX));
}

static void emit_xchg_eax_edx(Module* mod) {
    emit_byte(mod, 0x92);
}

static void emit_prefetch32(Module* mod, int write, int hint) {
    emit_byte(mod, 0x0F);
    emit_byte(mod, write ? 0x0D : 0x18);
    emit_memory_operand32(mod, write ? 1 : hint, EAX, 0);
}

static void emit_popcount32(Module* mod) {
    emit_mov_reg_reg(mod, ECX, EAX);
    emit_shr_reg_imm(mod, ECX, 1);
    emit_and_reg_imm(mod, ECX, 0x55555555u);
    emit_sub_reg_reg(mod, EAX, ECX);
    emit_mov_reg_reg(mod, ECX, EAX);
    emit_and_reg_imm(mod, ECX, 0x33333333u);
    emit_mov_reg_reg(mod, EDX, EAX);
    emit_shr_reg_imm(mod, EDX, 2);
    emit_and_reg_imm(mod, EDX, 0x33333333u);
    emit_add_reg_reg(mod, ECX, EDX);
    emit_mov_reg_reg(mod, EAX, ECX);
    emit_mov_reg_reg(mod, ECX, EAX);
    emit_shr_reg_imm(mod, ECX, 4);
    emit_add_reg_reg(mod, EAX, ECX);
    emit_and_reg_imm(mod, EAX, 0x0F0F0F0Fu);
    emit_mov_reg_reg(mod, ECX, EAX);
    emit_shr_reg_imm(mod, ECX, 8);
    emit_add_reg_reg(mod, EAX, ECX);
    emit_mov_reg_reg(mod, ECX, EAX);
    emit_shr_reg_imm(mod, ECX, 16);
    emit_add_reg_reg(mod, EAX, ECX);
    emit_and_reg_imm(mod, EAX, 0x3Fu);
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
    module_add_debug_frame_epilogue(mod, code_offset(mod),
                                    code_offset(mod) + 1u);
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
    if (!type_is_complete(type) || type->kind == TYPE_FUNC ||
        type->size <= 0 || type->align <= 0) {
        rcc_error(expression->loc,
                  "static compound literal requires a complete object type");
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

typedef struct CxxRttiEntry {
    CxxClass* cls;
    int offset;
    bool ambiguous;
} CxxRttiEntry;

#define CXX_RTTI_MAX_ENTRIES 128

static const char* codegen_cxx_rtti_symbol(const char* vtable_symbol) {
    static const char prefix[] = "__rcc_rtti_";
    size_t prefix_length = sizeof(prefix) - 1u;
    size_t symbol_length;
    char* buffer;
    const char* result;
    if (!vtable_symbol) return NULL;
    symbol_length = strlen(vtable_symbol);
    if (symbol_length > SIZE_MAX - prefix_length - 1u) {
        rcc_fatal("C++ RTTI metadata symbol size overflows");
    }
    buffer = rcc_alloc(prefix_length + symbol_length + 1u);
    memcpy(buffer, prefix, prefix_length);
    memcpy(buffer + prefix_length, vtable_symbol, symbol_length + 1u);
    result = rcc_intern(buffer);
    rcc_free(buffer);
    return result;
}

static bool codegen_cxx_rtti_add(CxxRttiEntry* entries, int* count,
                                 CxxClass* cls, int offset) {
    if (!entries || !count || !cls) return false;
    for (int index = 0; index < *count; ++index) {
        if (entries[index].cls != cls) continue;
        if (entries[index].offset != offset) entries[index].ambiguous = true;
        return true;
    }
    if (*count >= CXX_RTTI_MAX_ENTRIES) {
        rcc_error((SourceLoc){"<cxx-rtti>", 0, 0},
                  "public C++ hierarchy has too many RTTI entries");
        return false;
    }
    entries[*count].cls = cls;
    entries[*count].offset = offset;
    entries[*count].ambiguous = false;
    ++*count;
    return true;
}

static bool codegen_cxx_rtti_collect_nonvirtual(
    CxxClass* cls, int base_offset, CxxRttiEntry* entries, int* count,
    int depth) {
    if (!cls || !entries || !count) return false;
    if (depth > 64) {
        rcc_error((SourceLoc){"<cxx-rtti>", 0, 0},
                  "C++ RTTI hierarchy exceeds compiler depth limits");
        return false;
    }
    for (int index = 0; index < cls->base_count; ++index) {
        CxxClass* base = cls->bases[index].base;
        int64_t offset;
        if (!base || cls->bases[index].is_virtual ||
            cls->bases[index].access != ACCESS_PUBLIC ||
            !cls->base_offsets || cls->base_offsets[index] < 0) {
            continue;
        }
        offset = (int64_t)base_offset + cls->base_offsets[index];
        if (offset < INT_MIN || offset > INT_MAX) {
            rcc_error((SourceLoc){"<cxx-rtti>", 0, 0},
                      "C++ RTTI base offset exceeds compiler limits");
            return false;
        }
        if (!codegen_cxx_rtti_add(entries, count, base, (int)offset) ||
            !codegen_cxx_rtti_collect_nonvirtual(
                base, (int)offset, entries, count, depth + 1)) {
            return false;
        }
    }
    return true;
}

static bool codegen_cxx_rtti_collect(CxxClass* cls, CxxRttiEntry* entries,
                                     int* count) {
    if (!cls || !entries || !count ||
        !codegen_cxx_rtti_add(entries, count, cls, 0)) {
        return false;
    }
    if (!codegen_cxx_rtti_collect_nonvirtual(cls, 0, entries, count, 0)) {
        return false;
    }
    /* The layout pass has already deduplicated the complete object's virtual
     * bases.  Their offsets are therefore relative to the same complete
     * object, not to an intermediate base subobject. */
    for (int index = 0; index < cls->virtual_base_count; ++index) {
        CxxVirtualBaseInfo* virtual_base = &cls->virtual_bases[index];
        if (!virtual_base->base || !virtual_base->public_path ||
            virtual_base->offset < 0) {
            continue;
        }
        if (!codegen_cxx_rtti_add(entries, count, virtual_base->base,
                                  virtual_base->offset) ||
            !codegen_cxx_rtti_collect_nonvirtual(
                virtual_base->base, virtual_base->offset,
                entries, count, 0)) {
            return false;
        }
    }
    return true;
}

static void codegen_emit_pointer_rodata(Module* mod, uint32_t value,
                                        uint32_t pointer_size) {
    uint64_t wide = value;
    emit_rodata(mod, &wide, pointer_size);
}

static void codegen_emit_signed_rodata(Module* mod, int64_t value,
                                       uint32_t pointer_size) {
    uint64_t wide = (uint64_t)value;
    emit_rodata(mod, &wide, pointer_size);
}

static const char* codegen_cxx_vbase_variant_symbol(CxxClass* owner,
                                                     bool is_virtual_base,
                                                     int index) {
    char suffix[32];
    char* buffer;
    const char* result;
    const char* base;
    size_t base_length;
    size_t suffix_length;
    int suffix_written;
    if (!owner || !owner->virtual_base_table_symbol || index < 0) {
        return NULL;
    }
    base = owner->virtual_base_table_symbol;
    suffix_written = snprintf(suffix, sizeof(suffix), "_%s%d",
                              is_virtual_base ? "vbase" : "base", index);
    if (suffix_written < 0 || (size_t)suffix_written >= sizeof(suffix)) {
        rcc_fatal("C++ virtual-base variant suffix formatting failed");
    }
    base_length = strlen(base);
    suffix_length = (size_t)suffix_written;
    if (base_length > SIZE_MAX - suffix_length - 1u) {
        rcc_fatal("C++ virtual-base variant symbol size overflows");
    }
    buffer = rcc_alloc(base_length + suffix_length + 1u);
    memcpy(buffer, base, base_length);
    memcpy(buffer + base_length, suffix, suffix_length + 1u);
    result = rcc_intern(buffer);
    rcc_free(buffer);
    return result;
}

static void codegen_emit_cxx_vbase_table(Module* mod, const char* symbol,
                                         CxxClass* owner, CxxClass* source,
                                         int source_offset) {
    static const uint8_t zero[8] = {0};
    uint32_t pointer_size = g_opts.target_arch == ARCH_X64 ? 8u : 4u;
    uint32_t offset;
    if (!mod || !symbol || !owner || !source ||
        source->virtual_base_count <= 0 || source_offset < 0) {
        return;
    }
    while ((mod->rodata.size & (pointer_size - 1u)) != 0u) {
        emit_rodata(mod, zero, 1u);
    }
    offset = (uint32_t)mod->rodata.size;
    for (int index = 0; index < source->virtual_base_count; ++index) {
        CxxVirtualBaseInfo* source_base = &source->virtual_bases[index];
        int owner_offset;
        int64_t relative;
        if (!source_base->base ||
            !codegen_cxx_virtual_base_offset(owner, source_base->base,
                                             &owner_offset) ||
            owner_offset < 0) {
            rcc_error((SourceLoc){"<cxx-vbase>", 0, 0},
                      "virtual-base table for '%s' has no owner offset",
                      owner->name ? owner->name : "<anonymous>");
            codegen_emit_signed_rodata(mod, 0, pointer_size);
            continue;
        }
        relative = (int64_t)owner_offset - source_offset;
        codegen_emit_signed_rodata(mod, relative, pointer_size);
    }
    module_add_symbol(mod, symbol, offset, true, MODULE_SYMBOL_RODATA, true);
    module_mark_symbol_weak(mod, symbol);
}

static void codegen_emit_cxx_vbase_tables_in_namespace(Module* mod,
                                                        CxxNamespace* ns) {
    if (!mod || !ns) return;
    for (int index = 0; index < ns->class_count; ++index) {
        CxxClass* cls = ns->classes[index];
        if (!cls || cls->virtual_base_count <= 0 ||
            !cls->virtual_base_table_symbol) {
            continue;
        }
        codegen_emit_cxx_vbase_table(
            mod, cls->virtual_base_table_symbol, cls, cls, 0);
        for (int base_index = 0; base_index < cls->base_count;
             ++base_index) {
            CxxClass* base = cls->bases[base_index].base;
            if (!base || base->virtual_base_count <= 0 ||
                !cls->base_offsets || cls->base_offsets[base_index] < 0) {
                continue;
            }
            codegen_emit_cxx_vbase_table(
                mod, codegen_cxx_vbase_variant_symbol(cls, false, base_index),
                cls, base, cls->base_offsets[base_index]);
        }
        for (int virtual_index = 0;
             virtual_index < cls->virtual_base_count; ++virtual_index) {
            CxxClass* base = cls->virtual_bases[virtual_index].base;
            if (!base || base->virtual_base_count <= 0 ||
                cls->virtual_bases[virtual_index].offset < 0) {
                continue;
            }
            codegen_emit_cxx_vbase_table(
                mod,
                codegen_cxx_vbase_variant_symbol(cls, true, virtual_index),
                cls, base, cls->virtual_bases[virtual_index].offset);
        }
    }
    for (CxxNamespace* child = ns->children; child; child = child->next) {
        codegen_emit_cxx_vbase_tables_in_namespace(mod, child);
    }
}

void codegen_emit_cxx_typeinfo_symbol(Module* mod, const char* symbol) {
    static const char local_prefix[] = "__rcc_typeinfo_local_type_";
    static const uint8_t zero[8] = {0};
    uint8_t hash_bytes[8];
    const char* name_symbol;
    char* name_buffer;
    size_t symbol_length;
    uint32_t pointer_size = g_opts.target_arch == ARCH_X64 ? 8u : 4u;
    uint32_t offset;
    uint32_t name_offset;
    uint32_t name_pointer_offset;
    uint64_t hash = UINT64_C(1469598103934665603);
    bool is_local;
    const ModuleSymbol* existing;
    if (!mod || !symbol || !symbol[0]) return;
    is_local = strncmp(symbol, local_prefix,
                       sizeof(local_prefix) - 1u) == 0;
    existing = module_lookup_symbol(mod, symbol);
    /* Code generation may have registered an undefined relocation symbol
     * before the namespace/vtable pass reaches this identity.  Only an
     * already-defined object suppresses materialization; an undefined entry
     * must be promoted in place so weak RTTI references cannot escape the TU
     * unresolved. */
    if (existing && existing->is_defined) return;
    symbol_length = strlen(symbol);
    if (symbol_length > SIZE_MAX - sizeof("_name")) {
        rcc_fatal("C++ typeinfo name symbol is too long");
    }
    name_buffer = rcc_alloc(symbol_length + sizeof("_name"));
    memcpy(name_buffer, symbol, symbol_length);
    memcpy(name_buffer + symbol_length, "_name", sizeof("_name"));
    name_symbol = rcc_intern(name_buffer);
    rcc_free(name_buffer);
    name_offset = emit_string(mod, symbol, strlen(symbol));
    module_add_symbol(mod, name_symbol, name_offset, true,
                      MODULE_SYMBOL_RODATA, !is_local);
    if (!is_local) module_mark_symbol_weak(mod, name_symbol);
    for (const unsigned char* p = (const unsigned char*)symbol; *p; ++p) {
        hash ^= (uint64_t)*p;
        hash *= UINT64_C(1099511628211);
    }
    for (unsigned index = 0; index < sizeof(hash_bytes); ++index) {
        hash_bytes[index] = (uint8_t)(hash >> (index * 8u));
    }
    while ((mod->rodata.size & (pointer_size - 1u)) != 0u) {
        emit_rodata(mod, zero, 1u);
    }
    offset = (uint32_t)mod->rodata.size;
    emit_rodata(mod, hash_bytes, pointer_size);
    name_pointer_offset = (uint32_t)mod->rodata.size;
    emit_rodata(mod, zero, pointer_size);
    module_add_relocation(mod, MODULE_SYMBOL_RODATA, name_pointer_offset, 0u,
                          false, pointer_size == 8u, name_symbol);
    add_reloc(mod, MODULE_SYMBOL_RODATA, name_pointer_offset,
              pointer_size == 8u ? RIN_RELOC_ABS64 : RIN_RELOC_ABS32);
    module_add_symbol(mod, symbol, offset, true,
                      MODULE_SYMBOL_RODATA, !is_local);
    if (!is_local) module_mark_symbol_weak(mod, symbol);
}

static void codegen_emit_cxx_typeinfo(Module* mod, CxxClass* cls) {
    /* Forward-declared class types have no complete layout or RTTI identity.
     * They can still be used by compile-time type traits and must not force
     * emission of runtime typeinfo. A later complete definition reuses this
     * CxxClass and receives its canonical identity during class completion. */
    if (!mod || !cls || !cls->type || !cls->type->is_complete) return;
    if (!cls->type->cxx_typeinfo_symbol) {
        rcc_error((SourceLoc){"<cxx-rtti>", 0, 0},
                  "C++ class has no typeinfo identity");
        return;
    }
    /* Named class types share weak RTTI metadata across translation units.
     * Anonymous-namespace classes carry the local-typeinfo prefix, which
     * keeps their identity object private to this object file. */
    codegen_emit_cxx_typeinfo_symbol(
        mod, cls->type->cxx_typeinfo_symbol);
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
    if (!cls) return;
    if (cls->virtual_base_count > 0 &&
        cls->virtual_base_pointer_offset >= 0 &&
        cls->virtual_base_table_symbol) {
        uint64_t pointer_offset = (uint64_t)offset +
                                  (uint32_t)cls->virtual_base_pointer_offset;
        if (pointer_offset > UINT32_MAX) {
            rcc_error((SourceLoc){"<cxx-vbase>", 0, 0},
                      "virtual-base pointer exceeds data limits");
        } else {
            module_add_relocation(
                mod, source_section, (uint32_t)pointer_offset, 0u, false,
                width == 8u, cls->virtual_base_table_symbol);
            add_reloc(mod, source_section, (uint32_t)pointer_offset,
                      width == 8u ? RIN_RELOC_ABS64 : RIN_RELOC_ABS32);
        }
    }
    if (!cls->base_offsets) return;
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

    for (int virtual_index = 0;
         virtual_index < cls->virtual_base_count; ++virtual_index) {
        CxxVirtualBaseInfo* virtual_base = &cls->virtual_bases[virtual_index];
        const char* virtual_vtable_symbol = NULL;
        uint64_t virtual_offset;
        bool direct_virtual = false;
        if (!virtual_base->base || virtual_base->base->vtable_size <= 0 ||
            virtual_base->offset < 0) {
            continue;
        }
        for (int index = 0; index < cls->base_count; ++index) {
            if (cls->bases[index].is_virtual &&
                cls->bases[index].base == virtual_base->base &&
                cls->base_offsets &&
                cls->base_offsets[index] == virtual_base->offset) {
                direct_virtual = true;
                break;
            }
        }
        if (direct_virtual) continue;
        for (int secondary_index = 0;
             secondary_index < cls->secondary_vtable_count;
             ++secondary_index) {
            CxxSecondaryVtable* secondary =
                &cls->secondary_vtables[secondary_index];
            if (secondary->is_virtual_base &&
                secondary->virtual_base_index == virtual_index) {
                virtual_vtable_symbol = secondary->symbol;
                break;
            }
        }
        if (!virtual_vtable_symbol) {
            rcc_error((SourceLoc){"<cxx-vtable>", 0, 0},
                      "virtual base has no most-derived vtable");
            continue;
        }
        virtual_offset = (uint64_t)offset +
                         (uint32_t)virtual_base->offset;
        if (virtual_offset > UINT32_MAX) {
            rcc_error((SourceLoc){"<cxx-vtable>", 0, 0},
                      "virtual base vtable pointer exceeds data limits");
            continue;
        }
        module_add_relocation(mod, source_section, (uint32_t)virtual_offset,
                              0u, false, width == 8u,
                              virtual_vtable_symbol);
        add_reloc(mod, source_section, (uint32_t)virtual_offset,
                  width == 8u ? RIN_RELOC_ABS64 : RIN_RELOC_ABS32);
    }

    /* Initialize the hidden table pointer in every virtual-base-bearing
     * subobject.  A non-virtual base needs an owner-relative table because
     * the shared virtual base can be at a different offset for each path. */
    for (int index = 0; index < cls->base_count; ++index) {
        CxxClass* base = cls->bases[index].base;
        const char* table_symbol;
        uint64_t pointer_offset;
        if (!base || base->virtual_base_count <= 0 ||
            base->virtual_base_pointer_offset < 0 ||
            !cls->base_offsets || cls->base_offsets[index] < 0) {
            continue;
        }
        table_symbol = codegen_cxx_vbase_variant_symbol(cls, false, index);
        /* Direct virtual bases are initialized by the virtual-base loop
         * below; this branch handles only non-virtual base subobjects. */
        if (cls->bases[index].is_virtual || !table_symbol) continue;
        pointer_offset = (uint64_t)offset +
                         (uint32_t)cls->base_offsets[index] +
                         (uint32_t)base->virtual_base_pointer_offset;
        if (pointer_offset > UINT32_MAX) {
            rcc_error((SourceLoc){"<cxx-vbase>", 0, 0},
                      "base virtual-base pointer exceeds data limits");
            continue;
        }
        module_add_relocation(mod, source_section, (uint32_t)pointer_offset,
                              0u, false, width == 8u, table_symbol);
        add_reloc(mod, source_section, (uint32_t)pointer_offset,
                  width == 8u ? RIN_RELOC_ABS64 : RIN_RELOC_ABS32);
    }
    for (int virtual_index = 0;
         virtual_index < cls->virtual_base_count; ++virtual_index) {
        CxxClass* base = cls->virtual_bases[virtual_index].base;
        const char* table_symbol;
        uint64_t pointer_offset;
        if (!base || base->virtual_base_count <= 0 ||
            base->virtual_base_pointer_offset < 0 ||
            cls->virtual_bases[virtual_index].offset < 0) {
            continue;
        }
        table_symbol = codegen_cxx_vbase_variant_symbol(
            cls, true, virtual_index);
        if (!table_symbol) continue;
        pointer_offset = (uint64_t)offset +
                         (uint32_t)cls->virtual_bases[virtual_index].offset +
                         (uint32_t)base->virtual_base_pointer_offset;
        if (pointer_offset > UINT32_MAX) {
            rcc_error((SourceLoc){"<cxx-vbase>", 0, 0},
                      "nested virtual-base pointer exceeds data limits");
            continue;
        }
        module_add_relocation(mod, source_section, (uint32_t)pointer_offset,
                              0u, false, width == 8u, table_symbol);
        add_reloc(mod, source_section, (uint32_t)pointer_offset,
                  width == 8u ? RIN_RELOC_ABS64 : RIN_RELOC_ABS32);
    }
}

static void codegen_emit_cxx_vtable_storage(Module* mod,
                                            const char* symbol,
                                            int size,
                                            CxxVtableEntry* entries,
                                            CxxClass* owner,
                                            int source_offset) {
    static const uint8_t zero[16] = {0};
    uint32_t pointer_size = g_opts.target_arch == ARCH_X64 ? 8u : 4u;
    uint32_t offset;
    uint32_t metadata_offset;
    uint32_t header_offset;
    CxxRttiEntry rtti_entries[CXX_RTTI_MAX_ENTRIES];
    int rtti_count = 0;
    const char* metadata_symbol;
    if (!mod || !symbol || size <= 0 || !entries || !owner ||
        source_offset < 0 ||
        !codegen_cxx_rtti_collect(owner, rtti_entries, &rtti_count)) return;
    if (!owner->type || !owner->type->cxx_typeinfo_symbol) {
        rcc_error((SourceLoc){"<cxx-vtable>", 0, 0},
                  "polymorphic class has no typeinfo identity");
        return;
    }
    /* A class may be lowered through a vtable path before it is visited by
     * the namespace-wide typeinfo pass (notably an implicitly materialized
     * derived class).  The RTTI table owns the complete-object identity, so
     * materialize it here as well instead of leaving an unresolved weak
     * reference in the emitted object. */
    codegen_emit_cxx_typeinfo(mod, owner);
    metadata_symbol = codegen_cxx_rtti_symbol(symbol);
    while ((mod->rodata.size & (pointer_size - 1u)) != 0u) {
        emit_rodata(mod, zero, 1u);
    }
    metadata_offset = (uint32_t)mod->rodata.size;
    emit_rodata(mod, zero, pointer_size);
    module_add_relocation(mod, MODULE_SYMBOL_RODATA, metadata_offset, 0u,
                          false, pointer_size == 8u,
                          owner->type->cxx_typeinfo_symbol);
    add_reloc(mod, MODULE_SYMBOL_RODATA, metadata_offset,
              pointer_size == 8u ? RIN_RELOC_ABS64 : RIN_RELOC_ABS32);
    codegen_emit_signed_rodata(mod, source_offset, pointer_size);
    codegen_emit_pointer_rodata(mod, (uint32_t)rtti_count, pointer_size);
    for (int index = 0; index < rtti_count; ++index) {
        CxxRttiEntry* item = &rtti_entries[index];
        if (item->ambiguous || !item->cls->type ||
            !item->cls->type->cxx_typeinfo_symbol) {
            /* Keep the pair in the table so the count remains exact.  A null
             * identity can never match a valid target, while an ambiguous
             * repeated base is correctly rejected at runtime. */
            if (!item->ambiguous) {
                rcc_error((SourceLoc){"<cxx-rtti>", 0, 0},
                          "C++ RTTI entry has no typeinfo identity");
            }
            emit_rodata(mod, zero, pointer_size);
            codegen_emit_signed_rodata(mod, 0, pointer_size);
            continue;
        }
        codegen_emit_cxx_typeinfo(mod, item->cls);
        offset = (uint32_t)mod->rodata.size;
        emit_rodata(mod, zero, pointer_size);
        module_add_relocation(mod, MODULE_SYMBOL_RODATA, offset, 0u, false,
                              pointer_size == 8u,
                              item->cls->type->cxx_typeinfo_symbol);
        add_reloc(mod, MODULE_SYMBOL_RODATA, offset,
                  pointer_size == 8u ? RIN_RELOC_ABS64 : RIN_RELOC_ABS32);
        codegen_emit_signed_rodata(mod, item->offset, pointer_size);
    }
    module_add_symbol(mod, metadata_symbol, metadata_offset, true,
                      MODULE_SYMBOL_RODATA, true);
    module_mark_symbol_weak(mod, metadata_symbol);
    header_offset = (uint32_t)mod->rodata.size;
    emit_rodata(mod, zero, pointer_size);
    module_add_relocation(mod, MODULE_SYMBOL_RODATA, header_offset, 0u, false,
                          pointer_size == 8u, metadata_symbol);
    add_reloc(mod, MODULE_SYMBOL_RODATA, header_offset,
              pointer_size == 8u ? RIN_RELOC_ABS64 : RIN_RELOC_ABS32);
    offset = (uint32_t)mod->rodata.size;
    for (int slot = 0; slot < size; ++slot) {
        emit_rodata(mod, zero, pointer_size);
    }
    module_add_symbol(mod, symbol, offset, true, MODULE_SYMBOL_RODATA, true);
    module_mark_symbol_weak(mod, symbol);
    for (int slot = 0; slot < size; ++slot) {
        CxxVtableEntry* entry = &entries[slot];
        CxxMethod* method = entry->method;
        const char* entry_symbol = entry->entry_symbol;
        if (!method || !method->decl ||
            (!method->is_pure_virtual && !method->decl->link_name)) {
            rcc_error((SourceLoc){"<cxx-vtable>", 0, 0},
                      "virtual table entry %d of '%s' has no function body",
                      slot, owner->name ? owner->name : "<anonymous>");
            continue;
        }
        module_add_relocation(
            mod, MODULE_SYMBOL_RODATA,
            offset + (uint32_t)slot * pointer_size, 0u, false,
            pointer_size == 8u,
            method->is_pure_virtual
                ? "__rcc_pure_virtual"
                : (entry_symbol ? entry_symbol : decl_link_name(method->decl)));
        add_reloc(mod, MODULE_SYMBOL_RODATA,
                  offset + (uint32_t)slot * pointer_size,
                  pointer_size == 8u ? RIN_RELOC_ABS64 : RIN_RELOC_ABS32);
    }
}

static bool codegen_cxx_namespace_has_pure_virtual(CxxNamespace* ns) {
    if (!ns) return false;
    for (int index = 0; index < ns->class_count; ++index) {
        CxxClass* cls = ns->classes[index];
        if (!cls) continue;
        for (int slot = 0; slot < cls->vtable_size; ++slot) {
            if (cls->vtable && cls->vtable[slot].method &&
                cls->vtable[slot].method->is_pure_virtual) return true;
        }
        for (int table_index = 0; table_index < cls->secondary_vtable_count;
             ++table_index) {
            CxxSecondaryVtable* table = &cls->secondary_vtables[table_index];
            for (int slot = 0; table->entries && slot < table->size; ++slot) {
                if (table->entries[slot].method &&
                    table->entries[slot].method->is_pure_virtual) return true;
            }
        }
    }
    for (CxxNamespace* child = ns->children; child; child = child->next) {
        if (codegen_cxx_namespace_has_pure_virtual(child)) return true;
    }
    return false;
}

static void codegen_emit_cxx_pure_virtual_handler(Module* mod) {
    uint32_t start;
    if (!mod) return;
    start = code_offset(mod);
    /* This is the target-local implementation of the C++ pure virtual call
     * contract.  Reaching it is a violated abstract-class dispatch invariant,
     * so the instruction stream terminates synchronously on both targets. */
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0x0B); /* UD2 on both supported x86 targets. */
    module_add_symbol(mod, "__rcc_pure_virtual", start, true,
                      MODULE_SYMBOL_CODE, true);
    module_mark_symbol_weak(mod, "__rcc_pure_virtual");
}

static void codegen_emit_cxx_vtables_in_namespace(Module* mod,
                                                   CxxNamespace* ns) {
    if (!mod || !ns) return;
    for (int index = 0; index < ns->class_count; ++index) {
        CxxClass* cls = ns->classes[index];
        if (!cls || !cls->type) continue;
        codegen_emit_cxx_typeinfo(mod, cls);
    }
    for (int index = 0; index < ns->class_count; ++index) {
        CxxClass* cls = ns->classes[index];
        if (!cls || !cls->type) continue;
        if (cls->type->cxx_vtable_size > 0 &&
            cls->type->cxx_vtable_symbol && cls->vtable) {
            codegen_emit_cxx_vtable_storage(
                mod, cls->type->cxx_vtable_symbol, cls->vtable_size,
                cls->vtable, cls, 0);
        }
        for (int secondary_index = 0;
             secondary_index < cls->secondary_vtable_count;
             ++secondary_index) {
            CxxSecondaryVtable* secondary =
                &cls->secondary_vtables[secondary_index];
            int source_offset = -1;
            if (secondary->is_virtual_base && cls->virtual_bases &&
                secondary->virtual_base_index >= 0 &&
                secondary->virtual_base_index < cls->virtual_base_count) {
                source_offset =
                    cls->virtual_bases[secondary->virtual_base_index].offset;
            } else if (!secondary->is_virtual_base && cls->base_offsets &&
                       secondary->base_index >= 0 &&
                       secondary->base_index < cls->base_count) {
                source_offset = cls->base_offsets[secondary->base_index];
            }
            codegen_emit_cxx_vtable_storage(
                mod, secondary->symbol, secondary->size, secondary->entries,
                cls, source_offset);
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
            if (!table->entries || table->size <= 0) {
                continue;
            }
            if (table->is_virtual_base) {
                if (table->virtual_base_index < 0 ||
                    table->virtual_base_index >= cls->virtual_base_count ||
                    !cls->virtual_bases ||
                    cls->virtual_bases[table->virtual_base_index].offset < 0) {
                    continue;
                }
                base_offset =
                    cls->virtual_bases[table->virtual_base_index].offset;
            } else {
                if (table->base_index < 0 || !cls->base_offsets ||
                    table->base_index >= cls->base_count) {
                    continue;
                }
                base_offset = cls->base_offsets[table->base_index];
            }
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
        if (codegen_cxx_namespace_has_pure_virtual(global_namespace)) {
            codegen_emit_cxx_pure_virtual_handler(mod);
        }
        codegen_emit_cxx_vbase_tables_in_namespace(mod, global_namespace);
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

static void codegen_release_resolution_lists(void) {
    while (label_refs) {
        LabelRef* next = label_refs->next;
        rcc_free(label_refs);
        label_refs = next;
    }
    while (label_defs) {
        LabelDef* next = label_defs->next;
        rcc_free(label_defs);
        label_defs = next;
    }
    while (func_call_refs) {
        FuncCallRef* next = func_call_refs->next;
        rcc_free(func_call_refs);
        func_call_refs = next;
    }
    while (func_defs) {
        FuncDef* next = func_defs->next;
        rcc_free(func_defs);
        func_defs = next;
    }
}

/* ═══════════════════════════════════════
 * Expression Code Generation
 * ═══════════════════════════════════════ */

/* Forward declaration */
static void gen_expr(Module* mod, Expr* expr);
static void gen_expr_impl(Module* mod, Expr* expr);
static void gen_expr_raw(Module* mod, Expr* expr);
static void gen_cxx_typeid32(Module* mod, Expr* expr);
static void gen_cxx_dynamic_cast_runtime32(Module* mod, Expr* expr);
static void emit_test_scalar_value(Module* mod, const Type* type);
static void gen_expr64_pair(Module* mod, Expr* expr);
static void gen_expr64_pair_impl(Module* mod, Expr* expr);
static void gen_expr_as_integer64(Module* mod, Expr* expr);
static void gen_multiply_integer64_from_stack(Module* mod, Expr* lhs,
                                              int rhs_stack_offset);
static void gen_divmod_integer64_from_stack(Module* mod, Expr* lhs,
                                            const Type* result_type,
                                            bool want_remainder,
                                            int rhs_stack_offset);
static void gen_shift_integer64_from_stack(Module* mod, Expr* lhs,
                                           const Type* result_type,
                                           bool shift_left,
                                           int rhs_stack_offset);
static void gen_expr_as_type(Module* mod, Expr* expr, Type* target_type);
static void gen_call(Module* mod, Expr* expr);
static void gen_transfer_by_value_argument_cleanups32(
    Module* mod, ExprList* arguments);
static void gen_lvalue(Module* mod, Expr* expr);
static bool gen_guarded_static_initializer32(
    Module* mod, Decl* declaration);
static void gen_cxx_reference_adjustment32(Module* mod, Expr* expression);
static void codegen_push_call_temporary_cleanup32(
    Module* mod, Decl* temporary_owner, CxxCleanupPlan* plan);
static void codegen_push_explicit_destructor_cleanup32(
    Module* mod, Decl* temporary_owner, CxxCleanupPlan* plan);
static void emit_normalize_atomic_value(Module* mod, int reg,
                                         const Type* type);
static void emit_atomic_cmpxchg_width(Module* mod, int desired, int address,
                                      const Type* type);
static void emit_atomic_cmpxchg8b(Module* mod, int address);
static void emit_atomic_load_i686(Module* mod, const Type* type);
static void emit_convert_integer_value(Module* mod, int reg,
                                       const Type* source_type,
                                       const Type* target_type);

static bool gen_unsigned_magic_divisor32(
    const Expr* expression, uint32_t* divisor, uint32_t* multiplier,
    uint8_t* high_shift) {
    int64_t constant;
    if (!expression || !divisor || !multiplier || !high_shift ||
        (expression->kind != EXPR_DIV && expression->kind != EXPR_MOD) ||
        !expression->type || !type_is_integer(expression->type) ||
        !expression->type->is_unsigned || expression->type->size != 4 ||
        !expression->binary_lhs || !expression->binary_rhs ||
        !expr_eval_integer_constant(expression->binary_rhs, &constant)) {
        return false;
    }
    if (constant == 3) {
        *divisor = 3u;
        *multiplier = UINT32_C(0xaaaaaaab);
        *high_shift = 1u;
        return true;
    }
    if (constant == 5) {
        *divisor = 5u;
        *multiplier = UINT32_C(0xcccccccd);
        *high_shift = 2u;
        return true;
    }
    if (constant == 7) {
        *divisor = 7u;
        *multiplier = UINT32_C(0x24924925);
        *high_shift = 2u;
        return true;
    }
    return false;
}

static bool gen_unsigned_power_of_two_divisor32(
    Type* operation_type, Expr* divisor_expression,
    uint8_t* shift) {
    int64_t constant;
    uint32_t divisor;
    uint8_t amount = 0u;
    if (!operation_type || !shift || !type_is_integer(operation_type) ||
        !operation_type->is_unsigned || operation_type->size != 4 ||
        !divisor_expression ||
        !expr_eval_integer_constant(divisor_expression, &constant)) {
        return false;
    }
    divisor = (uint32_t)(uint64_t)constant;
    if (divisor == 0u) return false;
    if ((divisor & (divisor - 1u)) != 0u) return false;
    while (divisor > 1u) {
        divisor >>= 1u;
        ++amount;
    }
    *shift = amount;
    return true;
}

/* Lower the integer atomic bitwise compound assignments with the same
 * compare/exchange retry semantics as the standard atomic bitwise builtins.
 * The address and RHS are each evaluated once; a failed exchange reloads the
 * observed value from EAX (or EDX:EAX on the i686 wide path). */
static bool gen_atomic_bitwise_assignment(Module* mod, Expr* expr) {
    Type* type;
    int retry_label;

    if (!expr || !expr->binary_lhs || !expr->binary_lhs->type ||
        !expr->binary_lhs->type->is_atomic) return false;
    if (expr->kind != EXPR_AND_ASSIGN && expr->kind != EXPR_OR_ASSIGN &&
        expr->kind != EXPR_XOR_ASSIGN) return false;

    type = expr->binary_lhs->type;
    if (!type_is_integer(type) && type->kind != TYPE_ENUM) {
        rcc_error(expr->loc,
                  "atomic bitwise assignment requires an integer object");
        return true;
    }

    if (type->size == 8) {
        /* CMPXCHG8B uses EDX:EAX as the expected value and ECX:EBX as the
         * replacement.  Keep the operand on the stack while preserving all
         * callee-saved registers used by the loop. */
        emit_push_reg(mod, EBX);
        emit_push_reg(mod, ESI);
        emit_push_reg(mod, EDI);
        gen_expr_as_integer64(mod, expr->binary_rhs);
        emit_push_reg(mod, EDX);
        emit_push_reg(mod, EAX);
        gen_lvalue(mod, expr->binary_lhs);
        emit_mov_reg_reg(mod, ESI, EAX);
        emit_mov_reg_mem(mod, EAX, ESI, 0);
        emit_mov_reg_mem(mod, EDX, ESI, 4);
        retry_label = new_label();
        emit_label(mod, retry_label);
        emit_mov_reg_reg(mod, EBX, EAX);
        emit_mov_reg_reg(mod, ECX, EDX);
        emit_mov_reg_mem(mod, EDI, ESP, 0);
        if (expr->kind == EXPR_AND_ASSIGN) {
            emit_and_reg_reg(mod, EBX, EDI);
        } else if (expr->kind == EXPR_OR_ASSIGN) {
            emit_or_reg_reg(mod, EBX, EDI);
        } else {
            emit_xor_reg_reg(mod, EBX, EDI);
        }
        emit_mov_reg_mem(mod, EDI, ESP, 4);
        if (expr->kind == EXPR_AND_ASSIGN) {
            emit_and_reg_reg(mod, ECX, EDI);
        } else if (expr->kind == EXPR_OR_ASSIGN) {
            emit_or_reg_reg(mod, ECX, EDI);
        } else {
            emit_xor_reg_reg(mod, ECX, EDI);
        }
        emit_atomic_cmpxchg8b(mod, ESI);
        emit_jcc_label(mod, CC_NE, retry_label);
        emit_mov_reg_reg(mod, EAX, EBX);
        emit_mov_reg_reg(mod, EDX, ECX);
        emit_add_reg_imm(mod, ESP, 8);
        emit_pop_reg(mod, EDI);
        emit_pop_reg(mod, ESI);
        emit_pop_reg(mod, EBX);
        return true;
    }

    if (type->size != 1 && type->size != 2 && type->size != 4) {
        rcc_error(expr->loc,
                  "i686 atomic bitwise assignment has unsupported object width");
        return true;
    }
    emit_push_reg(mod, EBX);
    gen_expr(mod, expr->binary_rhs);
    emit_normalize_atomic_value(mod, EAX, type);
    emit_push_reg(mod, EAX); /* Operand. */
    gen_lvalue(mod, expr->binary_lhs);
    emit_push_reg(mod, EAX); /* Object address. */
    emit_mov_reg_reg(mod, ECX, EAX);
    emit_load_typed32(mod, EAX, ECX, 0, type);
    retry_label = new_label();
    emit_label(mod, retry_label);
    emit_mov_reg_reg(mod, EDX, EAX);
    emit_mov_reg_mem(mod, EBX, ESP, 4);
    if (expr->kind == EXPR_AND_ASSIGN) {
        emit_and_reg_reg(mod, EDX, EBX);
    } else if (expr->kind == EXPR_OR_ASSIGN) {
        emit_or_reg_reg(mod, EDX, EBX);
    } else {
        emit_xor_reg_reg(mod, EDX, EBX);
    }
    emit_normalize_atomic_value(mod, EDX, type);
    emit_atomic_cmpxchg_width(mod, EDX, ECX, type);
    emit_jcc_label(mod, CC_NE, retry_label);
    emit_mov_reg_reg(mod, EAX, EDX);
    emit_add_reg_imm(mod, ESP, 8);
    emit_pop_reg(mod, EBX);
    return true;
}

/* Lower integer compound assignments with one-time RHS evaluation.  The
 * i686-wide path keeps the operand in a stack slot and uses the existing
 * software 64-bit arithmetic helpers for each CAS retry. */
static bool gen_atomic_arithmetic_assignment(Module* mod, Expr* expr) {
    Type* type;
    int retry_label;
    int failed_label;
    int done_label;
    bool division;
    bool remainder;

    if (!expr || !expr->binary_lhs || !expr->binary_lhs->type ||
        !expr->binary_lhs->type->is_atomic) return false;
    if (expr->kind != EXPR_MUL_ASSIGN && expr->kind != EXPR_DIV_ASSIGN &&
        expr->kind != EXPR_MOD_ASSIGN && expr->kind != EXPR_LSHIFT_ASSIGN &&
        expr->kind != EXPR_RSHIFT_ASSIGN) return false;
    type = expr->binary_lhs->type;
    if (!type_is_integer(type) && type->kind != TYPE_ENUM) {
        rcc_error(expr->loc,
                  "atomic arithmetic assignment requires an integer object");
        return true;
    }
    division = expr->kind == EXPR_DIV_ASSIGN || expr->kind == EXPR_MOD_ASSIGN;
    remainder = expr->kind == EXPR_MOD_ASSIGN;
    if (type->size == 8) {
        emit_push_reg(mod, EBX);
        emit_push_reg(mod, ESI);
        emit_push_reg(mod, EDI);
        gen_expr_as_integer64(mod, expr->binary_rhs);
        emit_push_reg(mod, EDX); /* Operand high word. */
        emit_push_reg(mod, EAX); /* Operand low word. */
        gen_lvalue(mod, expr->binary_lhs);
        emit_mov_reg_reg(mod, ESI, EAX);
        emit_atomic_load_i686(mod, type);
        retry_label = new_label();
        failed_label = new_label();
        emit_label(mod, retry_label);
        /* Preserve the expected value while the arithmetic helper computes
         * the candidate from the saved RHS at [ESP+8]/[ESP+12]. */
        emit_push_reg(mod, EDX);
        emit_push_reg(mod, EAX);
        if (expr->kind == EXPR_MUL_ASSIGN) {
            gen_multiply_integer64_from_stack(mod, NULL, 8);
        } else if (division) {
            gen_divmod_integer64_from_stack(mod, NULL, expr->type,
                                             remainder, 8);
        } else {
            gen_shift_integer64_from_stack(
                mod, NULL, expr->type,
                expr->kind == EXPR_LSHIFT_ASSIGN, 8);
        }
        emit_mov_reg_reg(mod, EBX, EAX);
        emit_mov_reg_reg(mod, ECX, EDX);
        emit_mov_reg_mem(mod, EAX, ESP, 0);
        emit_mov_reg_mem(mod, EDX, ESP, 4);
        emit_atomic_cmpxchg8b(mod, ESI);
        emit_jcc_label(mod, CC_NE, failed_label);
        emit_add_reg_imm(mod, ESP, 8); /* Expected value. */
        emit_mov_reg_reg(mod, EAX, EBX);
        emit_mov_reg_reg(mod, EDX, ECX);
        emit_add_reg_imm(mod, ESP, 8); /* Saved RHS. */
        emit_pop_reg(mod, EDI);
        emit_pop_reg(mod, ESI);
        emit_pop_reg(mod, EBX);
        done_label = new_label();
        emit_jmp_label(mod, done_label);
        emit_label(mod, failed_label);
        emit_add_reg_imm(mod, ESP, 8); /* Discard the failed expected value. */
        emit_jmp_label(mod, retry_label);
        emit_label(mod, done_label);
        return true;
    }
    if (type->size != 1 && type->size != 2 && type->size != 4) {
        rcc_error(expr->loc,
                  "i686 atomic arithmetic assignment has unsupported object width");
        return true;
    }

    emit_push_reg(mod, EBX);
    emit_push_reg(mod, ESI);
    gen_expr(mod, expr->binary_rhs);
    emit_normalize_atomic_value(mod, EAX, type);
    emit_push_reg(mod, EAX); /* Operand. */
    gen_lvalue(mod, expr->binary_lhs);
    emit_push_reg(mod, EAX); /* Object address. */
    emit_mov_reg_reg(mod, ESI, EAX);
    emit_load_typed32(mod, EAX, ESI, 0, type);
    retry_label = new_label();
    emit_label(mod, retry_label);
    if (division) {
        /* Save the expected value while the divide consumes EAX/EDX. */
        emit_push_reg(mod, EAX);
        emit_mov_reg_mem(mod, EBX, ESP, 8);
        if (type->is_unsigned) emit_xor_reg_reg(mod, EDX, EDX);
        else emit_cdq(mod);
        if (type->is_unsigned) emit_div_reg(mod, EBX);
        else emit_idiv_reg(mod, EBX);
        if (remainder) {
            /* EDX already contains the remainder. */
        } else {
            emit_mov_reg_reg(mod, EDX, EAX);
        }
        emit_pop_reg(mod, EAX);
    } else {
        emit_mov_reg_reg(mod, EDX, EAX);
        emit_mov_reg_mem(mod, EBX, ESP, 4);
        if (expr->kind == EXPR_MUL_ASSIGN) {
            emit_imul_reg_reg(mod, EDX, EBX);
        } else {
            emit_mov_reg_reg(mod, ECX, EBX);
            if (expr->kind == EXPR_LSHIFT_ASSIGN) {
                emit_shl_reg_cl(mod, EDX);
            } else if (type->is_unsigned) {
                emit_shr_reg_cl(mod, EDX);
            } else {
                emit_sar_reg_cl(mod, EDX);
            }
        }
    }
    emit_normalize_atomic_value(mod, EDX, type);
    emit_atomic_cmpxchg_width(mod, EDX, ESI, type);
    emit_jcc_label(mod, CC_NE, retry_label);
    emit_mov_reg_reg(mod, EAX, EDX);
    emit_add_reg_imm(mod, ESP, 8);
    emit_pop_reg(mod, ESI);
    emit_pop_reg(mod, EBX);
    return true;
}

/* Lower i686 64-bit atomic +=/-= with one CMPXCHG8B retry loop.  The
 * right-hand side is evaluated once and retained as a two-word stack value;
 * every retry starts from the value returned by the failed exchange. */
static bool gen_atomic_addsub_assignment(Module* mod, Expr* expr) {
    Type* type;
    int retry_label;
    int failed_label;
    int done_label;
    bool subtract;

    if (!expr || !expr->binary_lhs || !expr->binary_lhs->type ||
        !expr->binary_lhs->type->is_atomic ||
        (expr->kind != EXPR_ADD_ASSIGN &&
         expr->kind != EXPR_SUB_ASSIGN)) return false;
    type = expr->binary_lhs->type;
    if (!type_is_integer(type) && type->kind != TYPE_ENUM) {
        rcc_error(expr->loc,
                  "atomic +=/-= requires an integer object on i686");
        return true;
    }
    if (type->size != 8) return false;

    subtract = expr->kind == EXPR_SUB_ASSIGN;
    emit_push_reg(mod, EBX);
    emit_push_reg(mod, ESI);
    emit_push_reg(mod, EDI);
    gen_expr_as_integer64(mod, expr->binary_rhs);
    emit_push_reg(mod, EDX); /* Operand high word. */
    emit_push_reg(mod, EAX); /* Operand low word. */
    gen_lvalue(mod, expr->binary_lhs);
    emit_mov_reg_reg(mod, ESI, EAX);
    emit_atomic_load_i686(mod, type);
    retry_label = new_label();
    failed_label = new_label();
    emit_label(mod, retry_label);
    emit_push_reg(mod, EDX); /* Expected high word. */
    emit_push_reg(mod, EAX); /* Expected low word. */
    emit_mov_reg_reg(mod, EBX, EAX);
    emit_mov_reg_reg(mod, ECX, EDX);
    emit_mov_reg_mem(mod, EDI, ESP, 8);
    if (subtract) {
        emit_sub_reg_reg(mod, EBX, EDI);
    } else {
        emit_add_reg_reg(mod, EBX, EDI);
    }
    emit_mov_reg_mem(mod, EDI, ESP, 12);
    if (subtract) {
        emit_sbb_reg_reg(mod, ECX, EDI);
    } else {
        emit_adc_reg_reg(mod, ECX, EDI);
    }
    emit_mov_reg_mem(mod, EAX, ESP, 0);
    emit_mov_reg_mem(mod, EDX, ESP, 4);
    emit_atomic_cmpxchg8b(mod, ESI);
    emit_jcc_label(mod, CC_NE, failed_label);
    emit_add_reg_imm(mod, ESP, 8); /* Expected value. */
    emit_mov_reg_reg(mod, EAX, EBX);
    emit_mov_reg_reg(mod, EDX, ECX);
    emit_add_reg_imm(mod, ESP, 8); /* Saved RHS. */
    emit_pop_reg(mod, EDI);
    emit_pop_reg(mod, ESI);
    emit_pop_reg(mod, EBX);
    done_label = new_label();
    emit_jmp_label(mod, done_label);
    /* A failed CMPXCHG8B leaves the observed value in EDX:EAX.  Remove the
     * per-attempt expected-value pair before retrying so contention cannot
     * grow the function's stack frame. */
    emit_label(mod, failed_label);
    emit_add_reg_imm(mod, ESP, 8);
    emit_jmp_label(mod, retry_label);
    emit_label(mod, done_label);
    return true;
}

/* Lower i686 64-bit atomic pre/post increment and decrement with one
 * CMPXCHG8B retry loop.  The old value is kept only for the postfix result;
 * a failed exchange discards that temporary before retrying with the value
 * returned by CMPXCHG8B. */
static bool gen_atomic_increment_assignment(Module* mod, Expr* expr) {
    Expr* operand;
    Type* type;
    bool increment;
    bool post;
    int retry_label;
    int failed_label;
    int done_label;

    if (!expr || !expr->unary_operand ||
        !expr->unary_operand->type ||
        !expr->unary_operand->type->is_atomic) return false;
    if (expr->kind != EXPR_PREINC && expr->kind != EXPR_PREDEC &&
        expr->kind != EXPR_POSTINC && expr->kind != EXPR_POSTDEC) {
        return false;
    }
    operand = expr->unary_operand;
    type = operand->type;
    if (!type_is_integer(type) && type->kind != TYPE_ENUM) return false;
    if (type->size != 8) return false;

    increment = expr->kind == EXPR_PREINC || expr->kind == EXPR_POSTINC;
    post = expr->kind == EXPR_POSTINC || expr->kind == EXPR_POSTDEC;
    emit_push_reg(mod, EBX);
    emit_push_reg(mod, ESI);
    emit_push_reg(mod, EDI);
    gen_lvalue(mod, operand);
    emit_mov_reg_reg(mod, ESI, EAX);
    emit_atomic_load_i686(mod, type);
    retry_label = new_label();
    failed_label = new_label();
    done_label = new_label();
    emit_label(mod, retry_label);
    emit_push_reg(mod, EDX); /* Old high word for postfix results. */
    emit_mov_reg_reg(mod, EDI, EAX); /* Old low word for postfix results. */
    emit_mov_reg_reg(mod, EBX, EAX);
    emit_mov_reg_reg(mod, ECX, EDX);
    if (increment) {
        emit_add_reg_imm(mod, EBX, 1u);
        emit_adc_reg_imm8(mod, ECX, 0u);
    } else {
        emit_sub_reg_imm(mod, EBX, 1u);
        emit_sbb_reg_imm8(mod, ECX, 0u);
    }
    emit_mov_reg_reg(mod, EAX, EDI);
    emit_mov_reg_mem(mod, EDX, ESP, 0);
    emit_atomic_cmpxchg8b(mod, ESI);
    emit_jcc_label(mod, CC_NE, failed_label);
    if (post) {
        emit_mov_reg_reg(mod, EAX, EDI);
        emit_mov_reg_mem(mod, EDX, ESP, 0);
    } else {
        emit_mov_reg_reg(mod, EAX, EBX);
        emit_mov_reg_reg(mod, EDX, ECX);
    }
    emit_add_reg_imm(mod, ESP, 4);
    emit_pop_reg(mod, EDI);
    emit_pop_reg(mod, ESI);
    emit_pop_reg(mod, EBX);
    emit_jmp_label(mod, done_label);
    emit_label(mod, failed_label);
    emit_add_reg_imm(mod, ESP, 4);
    emit_jmp_label(mod, retry_label);
    emit_label(mod, done_label);
    return true;
}

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

/* typeof(expression) evaluates its operand when the resulting type is
 * variably modified. Nested typeof operands are evaluated from the inner
 * type outward, before the VLA extents that consume that type. */
static void gen_typeof_runtime_evaluations(Module* mod, Type* type) {
    if (!type) return;
    if (type->kind == TYPE_PTR || type->kind == TYPE_ARRAY ||
        type->kind == TYPE_VECTOR) {
        gen_typeof_runtime_evaluations(mod, type->base);
    } else if (type->kind == TYPE_FUNC) {
        gen_typeof_runtime_evaluations(mod, type->ret_type);
        for (TypeParam* parameter = type->params; parameter;
             parameter = parameter->next) {
            gen_typeof_runtime_evaluations(mod, parameter->type);
        }
    }
    if (type->c_typeof_runtime_expression &&
        !type->c_typeof_runtime_expression
             ->c_typeof_runtime_evaluation_emitted) {
        gen_expr(mod, type->c_typeof_runtime_expression);
        type->c_typeof_runtime_expression
            ->c_typeof_runtime_evaluation_emitted = true;
    }
}

/* VLA bounds that belong to a block-scope typedef are captured when that
 * typedef declaration is reached. Later declarations using the typedef must
 * use the captured bound instead of evaluating the source expression again. */
static void gen_vla_typedef_bound_evaluations(Module* mod, Type* type,
                                              Decl* owner,
                                              bool inherited_typeof) {
    if (!type) return;
    inherited_typeof = inherited_typeof ||
                       type->c_typeof_operand_type != NULL;
    if (type->kind == TYPE_PTR || type->kind == TYPE_ARRAY ||
        type->kind == TYPE_VECTOR) {
        gen_vla_typedef_bound_evaluations(mod, type->base, owner,
                                          inherited_typeof);
    }
    if (type->kind == TYPE_ARRAY && type->array_bound &&
        !inherited_typeof &&
        type->array_bound->c_vla_typedef_owner == owner &&
        type->array_bound->c_vla_saved_bound_offset < 0) {
        gen_expr(mod, type->array_bound);
        emit_mov_mem_reg(mod, EBP,
                         type->array_bound->c_vla_saved_bound_offset, EAX);
    }
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
        if (type->array_bound->c_vla_saved_bound_offset < 0) {
            emit_mov_reg_mem(mod, EAX, EBP,
                             type->array_bound->c_vla_saved_bound_offset);
        } else {
            gen_expr(mod, type->array_bound);
        }
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

static bool gen_saved_vla_extent(Module* mod, Type* type, Expr* expression);

static Decl* codegen_vla_owner(Expr* expression) {
    while (expression) {
        if (expression->kind == EXPR_INDEX) {
            expression = expression->index_base;
        } else if (expression->kind == EXPR_DEREF ||
                   expression->kind == EXPR_ADDR ||
                   expression->kind == EXPR_PREINC ||
                   expression->kind == EXPR_PREDEC ||
                   expression->kind == EXPR_POSTINC ||
                   expression->kind == EXPR_POSTDEC) {
            expression = expression->unary_operand;
        } else if (expression->kind == EXPR_CAST) {
            expression = expression->cast_expr;
        } else if (expression->kind == EXPR_COMMA) {
            expression = expression->binary_rhs;
        } else if (expression->kind == EXPR_ADD ||
                   expression->kind == EXPR_SUB) {
            Type* left_type = expression->binary_lhs
                ? expression->binary_lhs->type : NULL;
            Type* right_type = expression->binary_rhs
                ? expression->binary_rhs->type : NULL;
            if (left_type && (left_type->kind == TYPE_PTR ||
                              left_type->kind == TYPE_ARRAY)) {
                expression = expression->binary_lhs;
            } else if (right_type && (right_type->kind == TYPE_PTR ||
                                      right_type->kind == TYPE_ARRAY)) {
                expression = expression->binary_rhs;
            } else {
                break;
            }
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

static void gen_vla_extents(Module* mod, Type* type, Type* source_type,
                            Expr* source_expression, Decl* declaration,
                            int* slot_index) {
    Type* source_base;
    if (type && type->c_typeof_operand_type) {
        source_type = type->c_typeof_operand_type;
        source_expression = type->c_typeof_runtime_expression;
    }
    if (!source_type) source_type = type;
    source_base = source_type &&
        (source_type->kind == TYPE_PTR || source_type->kind == TYPE_ARRAY ||
         source_type->kind == TYPE_VECTOR)
        ? source_type->base : (type ? type->base : NULL);
    if (type && type->kind == TYPE_PTR) {
        gen_vla_extents(mod, type->base, source_base, source_expression,
                        declaration, slot_index);
        return;
    }
    if (!type || type->kind != TYPE_ARRAY) {
        emit_mov_reg_imm(mod, EAX, type && type->size > 0
            ? (uint32_t)type->size : 0u);
        return;
    }
    if (type->base && type->base->kind == TYPE_ARRAY) {
        gen_vla_extents(mod, type->base, source_base, source_expression,
                        declaration, slot_index);
    } else {
        emit_mov_reg_imm(mod, EAX, type->base && type->base->size > 0
            ? (uint32_t)type->base->size : 0u);
    }
    bool reused_typeof_bound = type->array_bound &&
        type->array_bound->c_vla_shared_typeof_bound &&
        type->array_bound->c_vla_shared_bound_owner;
    if (reused_typeof_bound) {
        Expr* bound = type->array_bound;
        Decl* owner = bound->c_vla_shared_bound_owner;
        int index = bound->c_vla_shared_bound_index;
        emit_mov_reg_mem(mod, EAX, EBP,
                         owner->var_vla_extent_offset + index * 4);
    } else if (!gen_saved_vla_extent(mod, source_type, source_expression)) {
        emit_push_reg(mod, EAX);
        if (type->array_bound) {
            if (type->array_bound->c_vla_saved_bound_offset < 0) {
                emit_mov_reg_mem(mod, EAX, EBP,
                                 type->array_bound->c_vla_saved_bound_offset);
            } else {
                gen_expr(mod, type->array_bound);
            }
        } else {
            emit_mov_reg_imm(mod, EAX, type->array_len > 0
                ? (uint32_t)type->array_len : 0u);
        }
        emit_mov_reg_reg(mod, ECX, EAX);
        emit_pop_reg(mod, EAX);
        emit_imul_reg_reg(mod, EAX, ECX);
    }
    if (declaration && declaration->var_vla_extent_count > 0 &&
        slot_index && *slot_index < declaration->var_vla_extent_count) {
        emit_mov_mem_reg(mod, EBP,
                         declaration->var_vla_extent_offset +
                             *slot_index * 4,
                         EAX);
        if (type->array_bound &&
            type->array_bound->c_vla_shared_typeof_bound &&
            !type->array_bound->c_vla_shared_bound_owner) {
            type->array_bound->c_vla_shared_bound_owner = declaration;
            type->array_bound->c_vla_shared_bound_index = *slot_index;
        }
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

static bool pointer_stride_is_dynamic(const Type* type) {
    return type && (type->kind == TYPE_PTR || type->kind == TYPE_ARRAY) &&
           type->base &&
           type->base->kind == TYPE_ARRAY &&
           codegen_type_has_vla(type->base);
}

static void gen_vla_alloc(Module* mod, Decl* decl) {
    int slot_index = 0;
    int probe_loop_label;
    int probe_partial_label;
    int probe_done_label;
    gen_typeof_runtime_evaluations(mod, decl->type);
    gen_vla_extents(mod, decl->type, NULL, NULL, decl, &slot_index);
    emit_mov_mem_reg(mod, EBP, decl->var_vla_size_offset, EAX);

    /* A single ESP adjustment can jump over the stack guard.  Keep the
     * original size in its VLA slot and touch each 4 KiB step while moving
     * the stack pointer.  The final byte store stays inside the allocation,
     * including for one-byte VLAs. */
    probe_loop_label = new_label();
    probe_partial_label = new_label();
    probe_done_label = new_label();
    emit_label(mod, probe_loop_label);
    emit_cmp_reg_imm(mod, EAX, 4096);
    emit_jcc_label(mod, CC_BE, probe_partial_label);
    emit_sub_reg_imm(mod, ESP, 4096);
    emit_mov_mem_reg8(mod, ESP, 0, EAX);
    emit_sub_reg_imm(mod, EAX, 4096);
    emit_jmp_label(mod, probe_loop_label);
    emit_label(mod, probe_partial_label);
    emit_test_reg_reg(mod, EAX, EAX);
    emit_jcc_label(mod, CC_E, probe_done_label);
    emit_sub_reg_reg(mod, ESP, EAX);
    emit_mov_mem_reg8(mod, ESP, 0, EAX);
    emit_label(mod, probe_done_label);
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
        gen_vla_extents(mod, value->param_array_type, NULL, NULL,
                        value, &slot_index);
    }
}

static bool gen_is_integer64(const Type* type) {
    return type && type->size == 8 &&
           (type_is_integer((Type*)type) ||
            (type->kind == TYPE_ENUM && type->enum_underlying_type &&
             type_is_integer(type->enum_underlying_type)));
}

/* A scalar expression wider than the i686 register width is returned in
 * EDX:EAX.  Keep the pair intact when a C++ initializer materializes that
 * value into an object field; the ordinary typed store intentionally handles
 * only the low word for the other scalar widths. */
static void emit_store_scalar32(Module* mod, int base, int32_t displacement,
                                const Type* type) {
    if (gen_is_integer64(type)) {
        emit_mov_mem_reg(mod, base, displacement, EAX);
        emit_mov_mem_reg(mod, base, displacement + 4, EDX);
        return;
    }
    if (type && type->kind == TYPE_DOUBLE) {
        emit_mov_mem_reg(mod, base, displacement, EAX);
        emit_mov_mem_reg(mod, base, displacement + 4, EDX);
        return;
    }
    emit_store_typed32(mod, base, displacement, EAX, type);
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
    if (destination_type && destination_type->is_unsigned &&
        destination_type->size >= 8) {
        int lower_half_label = new_label();
        int done_label = new_label();
        /* FISTP stores a signed 64-bit integer.  Values in the upper half of
         * the unsigned range are converted after subtracting 2^63, then the
         * high bit is restored in the integer result. */
        emit_x87_power_of_two(mod, 63);
        emit_x87_fucomip_st1(mod);
        emit_jcc_label(mod, CC_A, lower_half_label);
        emit_x87_power_of_two(mod, 63);
        emit_x87_fsubp_st1(mod);
        emit_x87_store_integer_pop_memory(mod, 8, ESP, 8);
        emit_mov_reg_mem(mod, EDX, ESP, 12);
        emit_mov_reg_imm(mod, ECX, UINT32_C(0x80000000));
        emit_or_reg_reg(mod, EDX, ECX);
        emit_mov_mem_reg(mod, ESP, 12, EDX);
        emit_jmp_label(mod, done_label);
        emit_label(mod, lower_half_label);
        emit_x87_store_integer_pop_memory(mod, 8, ESP, 8);
        emit_label(mod, done_label);
    } else {
        emit_x87_store_integer_pop_memory(mod, 8, ESP, 8);
    }
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
    if (type_is_complex(source_type)) {
        Type* component_type = source_type->complex_component_type;
        if (target_type->kind == TYPE_BOOL) {
            emit_test_scalar_value(mod, source_type);
        } else if (component_type) {
            emit_load_floating_raw(mod, component_type, EAX, 0);
            if (gen_is_floating(target_type)) {
                emit_x87_convert_float_to_float(
                    mod, component_type, target_type);
            } else if (type_is_integer(target_type) ||
                       target_type->kind == TYPE_ENUM) {
                emit_x87_convert_float_to_integer(
                    mod, component_type, target_type);
            }
        }
    } else if (gen_is_floating(target_type)) {
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

static void gen_complex_component_from_operand32(
    Module* mod, Expr* operand, Type* component_type,
    int32_t destination_offset) {
    if (operand && operand->kind == EXPR_CAST &&
        type_is_complex(operand->type) && operand->cast_expr) {
        gen_complex_component_from_operand32(
            mod, operand->cast_expr, component_type, destination_offset);
        return;
    }
    if (type_is_complex(operand->type)) {
        Type* source_component = operand->type->complex_component_type;
        gen_expr(mod, operand);
        emit_push_reg(mod, EAX);
        emit_mov_reg_mem(mod, ECX, ESP, 0);
        emit_x87_load_memory(mod, gen_float_width(source_component), ECX, 0);
        emit_x87_store_pop_memory(mod, gen_float_width(component_type),
                                  EBP, destination_offset);
        emit_x87_load_memory(mod, gen_float_width(source_component), ECX,
                             source_component->size);
        emit_x87_store_pop_memory(mod, gen_float_width(component_type),
                                  EBP,
                                  destination_offset + component_type->size);
        emit_add_reg_imm(mod, ESP, 4);
        return;
    }

    gen_expr_as_type(mod, operand, component_type);
    emit_store_floating_raw(mod, component_type, EBP, destination_offset);
    emit_x87_fld_zero(mod);
    emit_x87_store_pop_memory(mod, gen_float_width(component_type), EBP,
                              destination_offset + component_type->size);
}

static void gen_complex_negate32(Module* mod, Expr* expression) {
    Type* complex_type = expression ? expression->type : NULL;
    Type* component_type = type_is_complex(complex_type)
        ? complex_type->complex_component_type : NULL;
    int32_t result_offset = expression
        ? expression->call_result_offset : 0;
    int32_t component_offset;

    if (!mod || !expression || !component_type || result_offset >= 0) {
        rcc_error(expression ? expression->loc : (SourceLoc){NULL, 0, 0},
                  "complex negation has no result storage");
        return;
    }
    gen_expr(mod, expression->unary_operand);
    emit_push_reg(mod, EAX);
    emit_mov_reg_mem(mod, ECX, ESP, 0);
    for (component_offset = 0; component_offset < complex_type->size;
         component_offset += component_type->size) {
        emit_x87_load_memory(mod, gen_float_width(component_type), ECX,
                             component_offset);
        emit_byte(mod, 0xD9);
        emit_byte(mod, 0xE0); /* FCHS */
        emit_x87_store_pop_memory(mod, gen_float_width(component_type), EBP,
                                  result_offset + component_offset);
    }
    emit_add_reg_imm(mod, ESP, 4);
    emit_byte(mod, 0x8D); /* LEA EAX, [EBP+disp32] */
    emit_byte(mod, modrm(2, EAX, EBP));
    emit_dword(mod, (uint32_t)result_offset);
}

static void gen_complex_push_component32(Module* mod, int32_t offset,
                                         Type* component_type) {
    if (gen_float_width(component_type) == 8) {
        emit_mov_reg_mem(mod, EAX, EBP, offset + 4);
        emit_push_reg(mod, EAX);
    }
    emit_mov_reg_mem(mod, EAX, EBP, offset);
    emit_push_reg(mod, EAX);
}

static void gen_complex_binary32(Module* mod, Expr* expression) {
    Type* complex_type = expression ? expression->type : NULL;
    Type* component_type = type_is_complex(complex_type)
        ? complex_type->complex_component_type : NULL;
    int32_t result_offset = expression
        ? expression->call_result_offset : 0;
    int32_t right_offset;
    int component_width;

    if (!mod || !expression || !component_type || result_offset >= 0 ||
        complex_type->size <= 0 || complex_type->size > INT32_MAX / 2) {
        rcc_error(expression ? expression->loc : (SourceLoc){NULL, 0, 0},
                  "complex arithmetic has no result storage");
        return;
    }
    right_offset = result_offset + complex_type->size;
    component_width = gen_float_width(component_type);
    gen_complex_component_from_operand32(
        mod, expression->binary_lhs, component_type, result_offset);
    gen_complex_component_from_operand32(
        mod, expression->binary_rhs, component_type, right_offset);

    if (expression->kind == EXPR_ADD || expression->kind == EXPR_SUB) {
        int32_t component_offset;
        for (component_offset = 0;
             component_offset < complex_type->size;
             component_offset += component_type->size) {
            emit_x87_load_memory(mod, component_width, EBP,
                                 result_offset + component_offset);
            emit_x87_binary_memory(mod, expression->kind, component_width,
                                   EBP, right_offset + component_offset);
            emit_x87_store_pop_memory(mod, component_width, EBP,
                                      result_offset + component_offset);
        }
    } else if (expression->kind == EXPR_MUL ||
               expression->kind == EXPR_DIV) {
        int argument_bytes = component_width == 8 ? 40 : 24;
        uint32_t call_offset;
        const char* helper = expression->kind == EXPR_MUL
            ? (component_width == 4 ? "__rin_cmul_f32" : "__rin_cmul_f64")
            : (component_width == 4 ? "__rin_cdiv_f32" : "__rin_cdiv_f64");

        emit_byte(mod, 0x8D); /* LEA EAX, [EBP+disp32] */
        emit_byte(mod, modrm(2, EAX, EBP));
        emit_dword(mod, (uint32_t)(result_offset + component_type->size));
        emit_push_reg(mod, EAX);
        emit_byte(mod, 0x8D); /* LEA EAX, [EBP+disp32] */
        emit_byte(mod, modrm(2, EAX, EBP));
        emit_dword(mod, (uint32_t)result_offset);
        emit_push_reg(mod, EAX);
        gen_complex_push_component32(
            mod, right_offset + component_type->size, component_type);
        gen_complex_push_component32(mod, right_offset, component_type);
        gen_complex_push_component32(
            mod, result_offset + component_type->size, component_type);
        gen_complex_push_component32(mod, result_offset, component_type);
        emit_byte(mod, 0xE8);
        call_offset = code_offset(mod);
        emit_dword(mod, 0u);
        add_func_call_ref(helper, call_offset);
        emit_add_reg_imm(mod, ESP, argument_bytes);
    }

    emit_byte(mod, 0x8D); /* LEA EAX, [EBP+disp32] */
    emit_byte(mod, modrm(2, EAX, EBP));
    emit_dword(mod, (uint32_t)result_offset);
}

static void gen_complex_compound32(Module* mod, Expr* expression) {
    Type* complex_type = expression && expression->binary_lhs
        ? expression->binary_lhs->type : NULL;
    Type* component_type = type_is_complex(complex_type)
        ? complex_type->complex_component_type : NULL;
    int32_t result_offset = expression
        ? expression->call_result_offset : 0;
    int32_t right_offset;
    int32_t component_offset;
    int operation;

    if (!mod || !expression || !component_type || result_offset >= 0 ||
        complex_type->size <= 0 || complex_type->size > INT32_MAX / 2) {
        rcc_error(expression ? expression->loc : (SourceLoc){NULL, 0, 0},
                  "complex compound assignment has no result storage");
        return;
    }
    if (expression->kind == EXPR_ADD_ASSIGN) operation = EXPR_ADD;
    else if (expression->kind == EXPR_SUB_ASSIGN) operation = EXPR_SUB;
    else if (expression->kind == EXPR_MUL_ASSIGN) operation = EXPR_MUL;
    else if (expression->kind == EXPR_DIV_ASSIGN) operation = EXPR_DIV;
    else {
        rcc_error(expression->loc,
                  "invalid complex compound assignment operation");
        return;
    }
    right_offset = result_offset + complex_type->size;

    /* Evaluate the destination once, then use its saved address for both the
     * read and write portions of the compound operation. */
    gen_lvalue(mod, expression->binary_lhs);
    emit_push_reg(mod, EAX);
    emit_mov_reg_mem(mod, EDX, ESP, 0);
    for (component_offset = 0; component_offset < complex_type->size;
         component_offset += 4) {
        emit_mov_reg_mem(mod, EAX, EDX, component_offset);
        emit_mov_mem_reg(mod, EBP, result_offset + component_offset, EAX);
    }
    gen_complex_component_from_operand32(
        mod, expression->binary_rhs, component_type, right_offset);

    if (operation == EXPR_ADD || operation == EXPR_SUB) {
        int component_width = gen_float_width(component_type);
        for (component_offset = 0;
             component_offset < complex_type->size;
             component_offset += component_type->size) {
            emit_x87_load_memory(mod, component_width, EBP,
                                 result_offset + component_offset);
            emit_x87_binary_memory(mod, operation, component_width, EBP,
                                   right_offset + component_offset);
            emit_x87_store_pop_memory(mod, component_width, EBP,
                                      result_offset + component_offset);
        }
    } else {
        int component_width = gen_float_width(component_type);
        int argument_bytes = component_width == 8 ? 40 : 24;
        uint32_t call_offset;
        const char* helper = operation == EXPR_MUL
            ? (component_width == 4 ? "__rin_cmul_f32" : "__rin_cmul_f64")
            : (component_width == 4 ? "__rin_cdiv_f32" : "__rin_cdiv_f64");
        emit_byte(mod, 0x8D); /* LEA EAX, [EBP+disp32] */
        emit_byte(mod, modrm(2, EAX, EBP));
        emit_dword(mod, (uint32_t)(result_offset + component_type->size));
        emit_push_reg(mod, EAX);
        emit_byte(mod, 0x8D); /* LEA EAX, [EBP+disp32] */
        emit_byte(mod, modrm(2, EAX, EBP));
        emit_dword(mod, (uint32_t)result_offset);
        emit_push_reg(mod, EAX);
        gen_complex_push_component32(
            mod, right_offset + component_type->size, component_type);
        gen_complex_push_component32(mod, right_offset, component_type);
        gen_complex_push_component32(
            mod, result_offset + component_type->size, component_type);
        gen_complex_push_component32(mod, result_offset, component_type);
        emit_byte(mod, 0xE8);
        call_offset = code_offset(mod);
        emit_dword(mod, 0u);
        add_func_call_ref(helper, call_offset);
        emit_add_reg_imm(mod, ESP, argument_bytes);
    }

    emit_byte(mod, 0x8D); /* LEA EAX, [EBP+disp32] */
    emit_byte(mod, modrm(2, EAX, EBP));
    emit_dword(mod, (uint32_t)result_offset);
    emit_push_reg(mod, EAX);
    emit_mov_reg_mem(mod, EDX, ESP, 4);
    for (component_offset = 0; component_offset < complex_type->size;
         component_offset += 4) {
        emit_mov_reg_mem(mod, EAX, ESP, 0);
        emit_mov_reg_mem(mod, ECX, EAX, component_offset);
        emit_mov_mem_reg(mod, EDX, component_offset, ECX);
    }
    emit_mov_reg_mem(mod, EAX, ESP, 0);
    emit_add_reg_imm(mod, ESP, 8);
}

static void gen_complex_increment32(Module* mod, Expr* expression) {
    Type* complex_type = expression && expression->unary_operand
        ? expression->unary_operand->type : NULL;
    Type* component_type = type_is_complex(complex_type)
        ? complex_type->complex_component_type : NULL;
    int32_t result_offset = expression
        ? expression->call_result_offset : 0;
    bool postfix = expression &&
        (expression->kind == EXPR_POSTINC ||
         expression->kind == EXPR_POSTDEC);
    bool decrement = expression &&
        (expression->kind == EXPR_PREDEC ||
         expression->kind == EXPR_POSTDEC);
    int32_t component_offset;

    if (!mod || !expression || !component_type || result_offset >= 0 ||
        complex_type->size <= 0 ||
        complex_type->size > INT32_MAX / 2) {
        rcc_error(expression ? expression->loc : (SourceLoc){NULL, 0, 0},
                  "complex increment has no result storage");
        return;
    }

    /* The lvalue is evaluated once.  A postfix expression keeps its old
     * value in the frame slot while the object itself receives the update. */
    gen_lvalue(mod, expression->unary_operand);
    emit_push_reg(mod, EAX);
    emit_mov_reg_mem(mod, EDX, ESP, 0);
    if (postfix) {
        for (component_offset = 0;
             component_offset < complex_type->size;
             component_offset += 4) {
            emit_mov_reg_mem(mod, EAX, EDX, component_offset);
            emit_mov_mem_reg(mod, EBP, result_offset + component_offset,
                             EAX);
        }
    }

    emit_x87_load_memory(mod, gen_float_width(component_type), EDX, 0);
    emit_x87_fld_one(mod);
    if (decrement) emit_x87_fsubp_st1(mod);
    else emit_x87_faddp_st1(mod);
    emit_x87_store_pop_memory(
        mod, gen_float_width(component_type), EDX, 0);

    emit_pop_reg(mod, EDX);
    if (postfix) {
        emit_byte(mod, 0x8D); /* LEA EAX, [EBP+disp32] */
        emit_byte(mod, modrm(2, EAX, EBP));
        emit_dword(mod, (uint32_t)result_offset);
    } else {
        emit_mov_reg_reg(mod, EAX, EDX);
    }
}

static Type* gen_complex_comparison_type32(Expr* expression) {
    Type* left = expression && expression->binary_lhs
        ? expression->binary_lhs->type : NULL;
    Type* right = expression && expression->binary_rhs
        ? expression->binary_rhs->type : NULL;
    Type* comparison_type = left && right ? type_common(left, right) : NULL;
    if (!type_is_complex(comparison_type)) {
        comparison_type = type_is_complex(left) ? left : right;
    }
    return type_is_complex(comparison_type) ? comparison_type : NULL;
}

static void gen_complex_compare32(Module* mod, Expr* expression) {
    Type* comparison_type = gen_complex_comparison_type32(expression);
    Type* component_type = comparison_type
        ? comparison_type->complex_component_type : NULL;
    int32_t left_offset = expression
        ? expression->call_result_offset : 0;
    int32_t right_offset;

    if (!mod || !expression || !component_type || left_offset >= 0 ||
        (expression->kind != EXPR_EQ && expression->kind != EXPR_NE)) {
        rcc_error(expression ? expression->loc : (SourceLoc){NULL, 0, 0},
                  "complex comparison has no valid operand storage");
        return;
    }
    right_offset = left_offset + comparison_type->size;
    gen_complex_component_from_operand32(
        mod, expression->binary_lhs, component_type, left_offset);
    gen_complex_component_from_operand32(
        mod, expression->binary_rhs, component_type, right_offset);

    gen_complex_push_component32(mod, left_offset, component_type);
    gen_complex_push_component32(mod, right_offset, component_type);
    emit_x87_compare_stack(mod, component_type, expression->kind);
    emit_push_reg(mod, EAX);
    gen_complex_push_component32(
        mod, left_offset + component_type->size, component_type);
    gen_complex_push_component32(
        mod, right_offset + component_type->size, component_type);
    emit_x87_compare_stack(mod, component_type, expression->kind);
    emit_pop_reg(mod, ECX);
    if (expression->kind == EXPR_EQ) emit_and_reg_reg(mod, EAX, ECX);
    else emit_or_reg_reg(mod, EAX, ECX);
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

typedef enum {
    RCC_CODEGEN_MMX_NONE,
    RCC_CODEGEN_MMX_BINARY,
    RCC_CODEGEN_MMX_VARIABLE_SHIFT,
    RCC_CODEGEN_MMX_IMMEDIATE_SHIFT,
    RCC_CODEGEN_MMX_STORE,
    RCC_CODEGEN_MMX_INIT_V2SI,
    RCC_CODEGEN_MMX_INIT_V4HI,
    RCC_CODEGEN_MMX_INIT_V8QI
} RccCodegenMmxKind;

static RccCodegenMmxKind codegen_mmx_builtin_kind(const char* name) {
    static const char* const binary_names[] = {
        "__builtin_ia32_packsswb", "__builtin_ia32_packssdw",
        "__builtin_ia32_packuswb", "__builtin_ia32_punpckhbw",
        "__builtin_ia32_punpckhwd", "__builtin_ia32_punpckhdq",
        "__builtin_ia32_punpcklbw", "__builtin_ia32_punpcklwd",
        "__builtin_ia32_punpckldq", "__builtin_ia32_paddb",
        "__builtin_ia32_paddw", "__builtin_ia32_paddd",
        "__builtin_ia32_paddsb", "__builtin_ia32_paddsw",
        "__builtin_ia32_paddusb", "__builtin_ia32_paddusw",
        "__builtin_ia32_psubb", "__builtin_ia32_psubw",
        "__builtin_ia32_psubd", "__builtin_ia32_psubsb",
        "__builtin_ia32_psubsw", "__builtin_ia32_psubusb",
        "__builtin_ia32_psubusw", "__builtin_ia32_pmaddwd",
        "__builtin_ia32_pmulhw", "__builtin_ia32_pmullw",
        "__builtin_ia32_pand", "__builtin_ia32_pandn",
        "__builtin_ia32_por", "__builtin_ia32_pxor",
        "__builtin_ia32_pcmpeqb", "__builtin_ia32_pcmpeqw",
        "__builtin_ia32_pcmpeqd", "__builtin_ia32_pcmpgtb",
        "__builtin_ia32_pcmpgtw", "__builtin_ia32_pcmpgtd"
    };
    static const char* const variable_shift_names[] = {
        "__builtin_ia32_psllw", "__builtin_ia32_pslld",
        "__builtin_ia32_psllq", "__builtin_ia32_psraw",
        "__builtin_ia32_psrad", "__builtin_ia32_psrlw",
        "__builtin_ia32_psrld", "__builtin_ia32_psrlq"
    };
    static const char* const immediate_shift_names[] = {
        "__builtin_ia32_psllwi", "__builtin_ia32_pslldi",
        "__builtin_ia32_psllqi", "__builtin_ia32_psrawi",
        "__builtin_ia32_psradi", "__builtin_ia32_psrlwi",
        "__builtin_ia32_psrldi", "__builtin_ia32_psrlqi"
    };
    size_t index;
    if (!name) return RCC_CODEGEN_MMX_NONE;
    if (strcmp(name, "__builtin_ia32_movntq") == 0) {
        return RCC_CODEGEN_MMX_STORE;
    }
    for (index = 0; index < sizeof(binary_names) / sizeof(binary_names[0]);
         ++index) {
        if (strcmp(name, binary_names[index]) == 0) {
            return RCC_CODEGEN_MMX_BINARY;
        }
    }
    for (index = 0;
         index < sizeof(variable_shift_names) / sizeof(variable_shift_names[0]);
         ++index) {
        if (strcmp(name, variable_shift_names[index]) == 0) {
            return RCC_CODEGEN_MMX_VARIABLE_SHIFT;
        }
    }
    for (index = 0;
         index < sizeof(immediate_shift_names) /
                    sizeof(immediate_shift_names[0]); ++index) {
        if (strcmp(name, immediate_shift_names[index]) == 0) {
            return RCC_CODEGEN_MMX_IMMEDIATE_SHIFT;
        }
    }
    if (strcmp(name, "__builtin_ia32_vec_init_v2si") == 0) {
        return RCC_CODEGEN_MMX_INIT_V2SI;
    }
    if (strcmp(name, "__builtin_ia32_vec_init_v4hi") == 0) {
        return RCC_CODEGEN_MMX_INIT_V4HI;
    }
    if (strcmp(name, "__builtin_ia32_vec_init_v8qi") == 0) {
        return RCC_CODEGEN_MMX_INIT_V8QI;
    }
    return RCC_CODEGEN_MMX_NONE;
}

static int codegen_mmx_binary_opcode(const char* name) {
    static const struct {
        const char* name;
        int opcode;
    } opcodes[] = {
        {"__builtin_ia32_packsswb", 0x63},
        {"__builtin_ia32_packssdw", 0x6B},
        {"__builtin_ia32_packuswb", 0x67},
        {"__builtin_ia32_punpckhbw", 0x6F},
        {"__builtin_ia32_punpckhwd", 0x65},
        {"__builtin_ia32_punpckhdq", 0x6D},
        {"__builtin_ia32_punpcklbw", 0x60},
        {"__builtin_ia32_punpcklwd", 0x61},
        {"__builtin_ia32_punpckldq", 0x62},
        {"__builtin_ia32_paddb", 0xFC}, {"__builtin_ia32_paddw", 0xFD},
        {"__builtin_ia32_paddd", 0xFE}, {"__builtin_ia32_paddsb", 0xEC},
        {"__builtin_ia32_paddsw", 0xED}, {"__builtin_ia32_paddusb", 0xDC},
        {"__builtin_ia32_paddusw", 0xDD}, {"__builtin_ia32_psubb", 0xF8},
        {"__builtin_ia32_psubw", 0xF9}, {"__builtin_ia32_psubd", 0xFA},
        {"__builtin_ia32_psubsb", 0xE8}, {"__builtin_ia32_psubsw", 0xE9},
        {"__builtin_ia32_psubusb", 0xD8}, {"__builtin_ia32_psubusw", 0xD9},
        {"__builtin_ia32_pmaddwd", 0xF5}, {"__builtin_ia32_pmulhw", 0xE5},
        {"__builtin_ia32_pmullw", 0xD5}, {"__builtin_ia32_pand", 0xDB},
        {"__builtin_ia32_pandn", 0xDF}, {"__builtin_ia32_por", 0xEB},
        {"__builtin_ia32_pxor", 0xEF}, {"__builtin_ia32_pcmpeqb", 0x74},
        {"__builtin_ia32_pcmpeqw", 0x75}, {"__builtin_ia32_pcmpeqd", 0x76},
        {"__builtin_ia32_pcmpgtb", 0x64}, {"__builtin_ia32_pcmpgtw", 0x65},
        {"__builtin_ia32_pcmpgtd", 0x66}
    };
    size_t index;
    for (index = 0; index < sizeof(opcodes) / sizeof(opcodes[0]); ++index) {
        if (strcmp(name, opcodes[index].name) == 0) return opcodes[index].opcode;
    }
    return -1;
}

static int codegen_mmx_shift_opcode(const char* name, int* group) {
    static const struct {
        const char* name;
        int opcode;
        int group;
    } shifts[] = {
        {"__builtin_ia32_psllw", 0xF1, 6}, {"__builtin_ia32_pslld", 0xF2, 6},
        {"__builtin_ia32_psllq", 0xF3, 6}, {"__builtin_ia32_psraw", 0xE1, 4},
        {"__builtin_ia32_psrad", 0xE2, 4}, {"__builtin_ia32_psrlw", 0xD1, 2},
        {"__builtin_ia32_psrld", 0xD2, 2}, {"__builtin_ia32_psrlq", 0xD3, 2},
        {"__builtin_ia32_psllwi", 0x71, 6}, {"__builtin_ia32_pslldi", 0x72, 6},
        {"__builtin_ia32_psllqi", 0x73, 6}, {"__builtin_ia32_psrawi", 0x71, 4},
        {"__builtin_ia32_psradi", 0x72, 4}, {"__builtin_ia32_psrlwi", 0x71, 2},
        {"__builtin_ia32_psrldi", 0x72, 2}, {"__builtin_ia32_psrlqi", 0x73, 2}
    };
    size_t index;
    for (index = 0; index < sizeof(shifts) / sizeof(shifts[0]); ++index) {
        if (strcmp(name, shifts[index].name) == 0) {
            if (group) *group = shifts[index].group;
            return shifts[index].opcode;
        }
    }
    return -1;
}

static int codegen_mmx_runtime_shift_opcode(const char* name) {
    if (strcmp(name, "__builtin_ia32_psllwi") == 0) return 0xF1;
    if (strcmp(name, "__builtin_ia32_pslldi") == 0) return 0xF2;
    if (strcmp(name, "__builtin_ia32_psllqi") == 0) return 0xF3;
    if (strcmp(name, "__builtin_ia32_psrawi") == 0) return 0xE1;
    if (strcmp(name, "__builtin_ia32_psradi") == 0) return 0xE2;
    if (strcmp(name, "__builtin_ia32_psrlwi") == 0) return 0xD1;
    if (strcmp(name, "__builtin_ia32_psrldi") == 0) return 0xD2;
    if (strcmp(name, "__builtin_ia32_psrlqi") == 0) return 0xD3;
    return -1;
}

static void emit_mmx_load(Module* mod, int mm, int base, int32_t disp) {
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0x6F);
    emit_memory_operand32(mod, mm, base, disp);
}

static void emit_mmx_store(Module* mod, int base, int32_t disp, int mm) {
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0x7F);
    emit_memory_operand32(mod, mm, base, disp);
}

static void emit_mmx_non_temporal_store(Module* mod, int base, int32_t disp,
                                        int mm) {
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0xE7);
    emit_memory_operand32(mod, mm, base, disp);
}

static void emit_mmx_binary(Module* mod, int opcode) {
    emit_byte(mod, 0x0F);
    emit_byte(mod, (uint8_t)opcode);
    emit_byte(mod, modrm(3, 0, 1));
}

static void emit_mmx_immediate_shift(Module* mod, int opcode, int group,
                                     uint8_t amount) {
    emit_byte(mod, 0x0F);
    emit_byte(mod, (uint8_t)opcode);
    emit_byte(mod, modrm(3, group, 0));
    emit_byte(mod, amount);
}

/* i686 uses the same bounded aggregate representation for vectors as AMD64;
 * these helpers keep XMM registers as private instruction temporaries. */
static void emit_sse128_memory(Module* mod, int xmm, int base, int32_t disp,
                               int prefix, int opcode) {
    if (prefix) emit_byte(mod, (uint8_t)prefix);
    emit_byte(mod, 0x0F);
    emit_byte(mod, (uint8_t)opcode);
    emit_memory_operand32(mod, xmm, base, disp);
}

static void emit_sse128_reg(Module* mod, int dst, int src, int prefix,
                            int opcode) {
    if (prefix) emit_byte(mod, (uint8_t)prefix);
    emit_byte(mod, 0x0F);
    emit_byte(mod, (uint8_t)opcode);
    emit_byte(mod, modrm(3, dst, src));
}

static void emit_sse128_immediate(Module* mod, int dst, int src, int prefix,
                                  int opcode, uint8_t immediate) {
    emit_sse128_reg(mod, dst, src, prefix, opcode);
    emit_byte(mod, immediate);
}

static void emit_sse128_immediate_group(Module* mod, int group, int src,
                                        int prefix, int opcode,
                                        uint8_t immediate) {
    if (prefix) emit_byte(mod, (uint8_t)prefix);
    emit_byte(mod, 0x0F);
    emit_byte(mod, (uint8_t)opcode);
    emit_byte(mod, modrm(3, group, src));
    emit_byte(mod, immediate);
}

static void emit_sse_to_int_opcode32(Module* mod, int gpr, int xmm,
                                     int source_width, int opcode) {
    emit_byte(mod, source_width == 4 ? 0xF3 : 0xF2);
    emit_byte(mod, 0x0F);
    emit_byte(mod, (uint8_t)opcode);
    emit_byte(mod, modrm(3, gpr, xmm));
}

static void emit_int_to_sse32(Module* mod, int xmm, int gpr, int source_width,
                              int destination_width) {
    emit_byte(mod, destination_width == 4 ? 0xF3 : 0xF2);
    if (source_width == 8) emit_byte(mod, 0x66);
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0x2A);
    emit_byte(mod, modrm(3, xmm, gpr));
}

static void emit_sse_movemask32(Module* mod, int gpr, int xmm, int prefix,
                                int opcode) {
    if (prefix) emit_byte(mod, (uint8_t)prefix);
    emit_byte(mod, 0x0F);
    emit_byte(mod, (uint8_t)opcode);
    emit_byte(mod, modrm(3, gpr, xmm));
}

static bool codegen_sse_float_binary(const char* name, int* prefix,
                                    int* opcode, bool* reverse) {
    static const struct { const char* name; int prefix; int opcode; } table[] = {
        {"addps", 0, 0x58}, {"subps", 0, 0x5C}, {"mulps", 0, 0x59},
        {"divps", 0, 0x5E}, {"minps", 0, 0x5D}, {"maxps", 0, 0x5F},
        {"andps", 0, 0x54}, {"orps", 0, 0x56}, {"xorps", 0, 0x57},
        {"andnps", 0, 0x55},
        {"addss", 0xF3, 0x58}, {"subss", 0xF3, 0x5C},
        {"mulss", 0xF3, 0x59}, {"divss", 0xF3, 0x5E},
        {"minss", 0xF3, 0x5D}, {"maxss", 0xF3, 0x5F},
        {"addpd", 0x66, 0x58}, {"subpd", 0x66, 0x5C},
        {"mulpd", 0x66, 0x59}, {"divpd", 0x66, 0x5E},
        {"minpd", 0x66, 0x5D}, {"maxpd", 0x66, 0x5F},
        {"andpd", 0x66, 0x54}, {"orpd", 0x66, 0x56},
        {"xorpd", 0x66, 0x57}, {"andnpd", 0x66, 0x55},
        {"addsd", 0xF2, 0x58}, {"subsd", 0xF2, 0x5C},
        {"mulsd", 0xF2, 0x59}, {"divsd", 0xF2, 0x5E},
        {"minsd", 0xF2, 0x5D}, {"maxsd", 0xF2, 0x5F}
    };
    if (!name || strncmp(name, "__builtin_ia32_", 15) != 0) return false;
    name += 15;
    for (size_t index = 0; index < sizeof(table) / sizeof(table[0]); ++index) {
        if (strcmp(name, table[index].name) == 0) {
            if (prefix) *prefix = table[index].prefix;
            if (opcode) *opcode = table[index].opcode;
            if (reverse) *reverse = strncmp(name, "andn", 4) == 0;
            return true;
        }
    }
    return false;
}

static bool codegen_sse_integer_binary(const char* name, int* prefix,
                                       int* opcode, bool* reverse) {
    static const struct { const char* name; int opcode; } table[] = {
        {"packsswb128", 0x63}, {"packssdw128", 0x6B}, {"packuswb128", 0x67},
        {"punpckhbw128", 0x68}, {"punpckhwd128", 0x69},
        {"punpckhdq128", 0x6A}, {"punpckhqdq128", 0x6D},
        {"punpcklbw128", 0x60}, {"punpcklwd128", 0x61},
        {"punpckldq128", 0x62}, {"punpcklqdq128", 0x6C},
        {"paddb128", 0xFC}, {"paddw128", 0xFD}, {"paddd128", 0xFE},
        {"paddq128", 0xD4}, {"paddsb128", 0xEC}, {"paddsw128", 0xED},
        {"paddusb128", 0xDC}, {"paddusw128", 0xDD},
        {"psubb128", 0xF8}, {"psubw128", 0xF9}, {"psubd128", 0xFA},
        {"psubq128", 0xFB}, {"psubsb128", 0xE8}, {"psubsw128", 0xE9},
        {"psubusb128", 0xD8}, {"psubusw128", 0xD9},
        {"pmullw128", 0xD5}, {"pmulhw128", 0xE5}, {"pmulhuw128", 0xE4},
        {"pmuludq128", 0xF4}, {"pmaddwd128", 0xF5},
        {"pand128", 0xDB}, {"pandn128", 0xDF}, {"por128", 0xEB},
        {"pxor128", 0xEF}, {"pcmpeqb128", 0x74}, {"pcmpeqw128", 0x75},
        {"pcmpeqd128", 0x76}, {"pcmpgtb128", 0x64}, {"pcmpgtw128", 0x65},
        {"pcmpgtd128", 0x66}
    };
    if (!name || strncmp(name, "__builtin_ia32_", 15) != 0) return false;
    name += 15;
    for (size_t index = 0; index < sizeof(table) / sizeof(table[0]); ++index) {
        if (strcmp(name, table[index].name) == 0) {
            if (prefix) *prefix = 0x66;
            if (opcode) *opcode = table[index].opcode;
            if (reverse) *reverse = strcmp(name, "pandn128") == 0;
            return true;
        }
    }
    return false;
}

static int codegen_sse_compare_predicate(const char* name) {
    if (strstr(name, "cmpeq")) return 0;
    if (strstr(name, "cmpneq")) return 4;
    if (strstr(name, "cmplt")) return 1;
    if (strstr(name, "cmple")) return 2;
    if (strstr(name, "cmpnlt")) return 5;
    if (strstr(name, "cmpnle")) return 6;
    if (strstr(name, "cmpord")) return 7;
    if (strstr(name, "cmpunord")) return 3;
    return -1;
}

static void gen_sse_store_result32(Module* mod, Expr* expr, int xmm) {
    if (!expr || !expr->type || !type_is_vector(expr->type) ||
        expr->call_result_offset >= 0) {
        rcc_error(expr ? expr->loc : (SourceLoc){"<sse>", 0, 0},
                  "SSE vector builtin has no automatic result slot");
        return;
    }
    emit_sse128_memory(mod, xmm, EBP, expr->call_result_offset, 0, 0x11);
    emit_byte(mod, 0x8D); /* LEA EAX, [EBP+result] */
    emit_byte(mod, modrm(2, EAX, EBP));
    emit_dword(mod, (uint32_t)expr->call_result_offset);
}

static void gen_sse_vector_argument32(Module* mod, Expr* argument, int xmm) {
    gen_expr(mod, argument);
    emit_sse128_memory(mod, xmm, EAX, 0, 0, 0x10);
}

static bool gen_sse_two_vectors32(Module* mod, Expr* call) {
    Expr* lhs = call_argument(call, 0);
    Expr* rhs = call_argument(call, 1);
    if (!lhs || !rhs) return false;
    emit_sub_reg_imm(mod, ESP, 8);
    gen_expr(mod, lhs); emit_mov_mem_reg(mod, ESP, 0, EAX);
    gen_expr(mod, rhs); emit_mov_mem_reg(mod, ESP, 4, EAX);
    emit_mov_reg_mem(mod, ECX, ESP, 0);
    emit_sse128_memory(mod, 0, ECX, 0, 0, 0x10);
    emit_mov_reg_mem(mod, ECX, ESP, 4);
    emit_sse128_memory(mod, 1, ECX, 0, 0, 0x10);
    emit_add_reg_imm(mod, ESP, 8);
    return true;
}

static bool gen_sse_builtin32(Module* mod, Expr* expr) {
    const char* full_name;
    const char* name;
    int prefix;
    int opcode;
    int64_t immediate;
    bool reverse;
    if (!expr || !expr->call_func || expr->call_func->kind != EXPR_IDENT ||
        !expr->call_func->ident_name) return false;
    full_name = expr->call_func->ident_name;
    if (strncmp(full_name, "__builtin_ia32_", 15) != 0) return false;
    name = full_name + 15;

    if (strcmp(name, "pause") == 0) {
        emit_byte(mod, 0xF3); emit_byte(mod, 0x90); return true;
    }
    if (strcmp(name, "sfence") == 0 || strcmp(name, "lfence") == 0 ||
        strcmp(name, "mfence") == 0) {
        emit_byte(mod, 0x0F); emit_byte(mod, 0xAE);
        emit_byte(mod, strcmp(name, "sfence") == 0 ? 0xF8 :
                  strcmp(name, "lfence") == 0 ? 0xE8 : 0xF0);
        return true;
    }
    if (strcmp(name, "ldmxcsr") == 0) {
        emit_sub_reg_imm(mod, ESP, 4);
        gen_expr(mod, call_argument(expr, 0));
        emit_mov_mem_reg(mod, ESP, 0, EAX);
        emit_byte(mod, 0x0F); emit_byte(mod, 0xAE);
        emit_memory_operand32(mod, 2, ESP, 0);
        emit_add_reg_imm(mod, ESP, 4);
        return true;
    }
    if (strcmp(name, "stmxcsr") == 0) {
        emit_sub_reg_imm(mod, ESP, 4);
        emit_byte(mod, 0x0F); emit_byte(mod, 0xAE);
        emit_memory_operand32(mod, 3, ESP, 0);
        emit_mov_reg_mem(mod, EAX, ESP, 0);
        emit_add_reg_imm(mod, ESP, 4);
        return true;
    }
    if (strcmp(name, "clflush") == 0) {
        gen_expr(mod, call_argument(expr, 0));
        emit_byte(mod, 0x0F); emit_byte(mod, 0xAE);
        emit_byte(mod, modrm(0, 7, EAX));
        return true;
    }
    if (strcmp(name, "movnti") == 0) {
        emit_sub_reg_imm(mod, ESP, 4);
        gen_expr(mod, call_argument(expr, 0));
        emit_mov_mem_reg(mod, ESP, 0, EAX);
        gen_expr(mod, call_argument(expr, 1));
        emit_mov_reg_reg(mod, ECX, EAX);
        emit_mov_reg_mem(mod, EAX, ESP, 0);
        emit_byte(mod, 0x0F); emit_byte(mod, 0xC3);
        emit_memory_operand32(mod, ECX, EAX, 0);
        emit_add_reg_imm(mod, ESP, 4);
        return true;
    }
    if (strstr(name, "store") == name || strstr(name, "movnt") == name) {
        emit_sub_reg_imm(mod, ESP, 4);
        gen_expr(mod, call_argument(expr, 0));
        emit_mov_mem_reg(mod, ESP, 0, EAX);
        gen_expr(mod, call_argument(expr, 1));
        emit_sse128_memory(mod, 0, EAX, 0, 0, 0x10);
        emit_mov_reg_mem(mod, ECX, ESP, 0);
        if (strstr(name, "movnt") == name) {
            prefix = strstr(name, "pd") || strstr(name, "dq") ? 0x66 : 0;
            emit_sse128_memory(mod, 0, ECX, 0, prefix, 0x2B);
        } else if (strstr(name, "sd")) {
            emit_sse128_memory(mod, 0, ECX, 0, 0xF2, 0x11);
        } else if (strstr(name, "ss")) {
            emit_sse128_memory(mod, 0, ECX, 0, 0xF3, 0x11);
        } else {
            emit_sse128_memory(mod, 0, ECX, 0, 0, 0x11);
        }
        emit_add_reg_imm(mod, ESP, 4);
        return true;
    }
    if (strstr(name, "load") == name) {
        gen_expr(mod, call_argument(expr, 0));
        if (strcmp(name, "loadss") == 0 || strcmp(name, "loadsd") == 0) {
            emit_sse128_reg(mod, 0, 0, 0, 0x57);
            emit_sse128_memory(mod, 0, EAX, 0,
                               strcmp(name, "loadsd") == 0 ? 0xF2 : 0xF3,
                               0x10);
        } else {
            prefix = strcmp(name, "loadupd") == 0 ||
                     strcmp(name, "loaddqu") == 0 ? 0x66 : 0;
            opcode = strcmp(name, "loaddqu") == 0 ? 0x6F : 0x10;
            emit_sse128_memory(mod, 0, EAX, 0, prefix, opcode);
        }
        gen_sse_store_result32(mod, expr, 0);
        return true;
    }
    if (strcmp(name, "movss") == 0 || strcmp(name, "movsd") == 0) {
        if (!gen_sse_two_vectors32(mod, expr)) return true;
        emit_sse128_reg(mod, 0, 1, strcmp(name, "movsd") == 0 ? 0xF2 : 0xF3,
                        0x10);
        gen_sse_store_result32(mod, expr, 0);
        return true;
    }
    if (codegen_sse_float_binary(full_name, &prefix, &opcode, &reverse) ||
        codegen_sse_integer_binary(full_name, &prefix, &opcode, &reverse)) {
        if (!gen_sse_two_vectors32(mod, expr)) return true;
        if (reverse) {
            emit_sse128_reg(mod, 1, 0, prefix, opcode);
            emit_sse128_reg(mod, 0, 1, 0, 0x10);
        } else {
            emit_sse128_reg(mod, 0, 1, prefix, opcode);
        }
        gen_sse_store_result32(mod, expr, 0);
        return true;
    }
    if (strcmp(name, "unpckhps") == 0 || strcmp(name, "unpcklps") == 0 ||
        strcmp(name, "unpckhpd") == 0 || strcmp(name, "unpcklpd") == 0 ||
        strcmp(name, "movhlps") == 0 || strcmp(name, "movlhps") == 0) {
        if (!gen_sse_two_vectors32(mod, expr)) return true;
        if (strcmp(name, "movhlps") == 0 || strcmp(name, "movlhps") == 0) {
            emit_sse128_reg(mod, 0, 1, 0,
                            strcmp(name, "movhlps") == 0 ? 0x12 : 0x16);
        } else {
            emit_sse128_reg(mod, 0, 1, strstr(name, "pd") ? 0x66 : 0,
                            strstr(name, "unpckh") ? 0x15 : 0x14);
        }
        gen_sse_store_result32(mod, expr, 0);
        return true;
    }
    if (strstr(name, "sqrt") == name || strstr(name, "rcp") == name ||
        strstr(name, "rsqrt") == name) {
        gen_sse_vector_argument32(mod, call_argument(expr, 0), 0);
        opcode = strstr(name, "rsqrt") ? 0x52 :
                 strstr(name, "sqrt") ? 0x51 : 0x53;
        prefix = strstr(name, "sd") ? 0xF2 : strstr(name, "ss") ? 0xF3 :
                 strstr(name, "pd") ? 0x66 : 0;
        emit_sse128_reg(mod, 0, 0, prefix, opcode);
        gen_sse_store_result32(mod, expr, 0);
        return true;
    }
    if (strstr(name, "cmp") == name) {
        int predicate = codegen_sse_compare_predicate(name);
        if (predicate < 0 || !gen_sse_two_vectors32(mod, expr)) {
            rcc_error(expr->loc, "SSE compare builtin has invalid operands");
            return true;
        }
        prefix = strstr(name, "ss") ? 0xF3 :
                 strstr(name, "sd") || strstr(name, "pd") ? 0x66 : 0;
        emit_sse128_immediate(mod, 0, 1, prefix, 0xC2, (uint8_t)predicate);
        gen_sse_store_result32(mod, expr, 0);
        return true;
    }
    if (strstr(name, "comi") == name || strstr(name, "ucomi") == name) {
        if (!gen_sse_two_vectors32(mod, expr)) return true;
        prefix = strstr(name, "d") ? 0x66 : 0;
        emit_sse128_reg(mod, 0, 1, prefix, 0x2E);
        if (strstr(name, "eq")) emit_setcc(mod, CC_E, EAX);
        else if (strstr(name, "neq")) emit_setcc(mod, CC_NE, EAX);
        else if (strstr(name, "lt")) emit_setcc(mod, CC_B, EAX);
        else if (strstr(name, "le")) emit_setcc(mod, CC_BE, EAX);
        else if (strstr(name, "gt")) emit_setcc(mod, CC_A, EAX);
        else emit_setcc(mod, CC_AE, EAX);
        emit_byte(mod, 0x0F); emit_byte(mod, 0xB6);
        emit_byte(mod, modrm(3, EAX, EAX));
        return true;
    }
    if (strstr(name, "movmsk") == name || strstr(name, "pmovmskb") == name) {
        gen_sse_vector_argument32(mod, call_argument(expr, 0), 0);
        prefix = strstr(name, "pd") || strstr(name, "pmovmskb") ? 0x66 : 0;
        opcode = strstr(name, "pmovmskb") ? 0xD7 : 0x50;
        emit_sse_movemask32(mod, EAX, 0, prefix, opcode);
        return true;
    }
    if (strcmp(name, "shufps") == 0 || strcmp(name, "shufpd") == 0) {
        if (!gen_sse_two_vectors32(mod, expr)) return true;
        if (!expr_eval_integer_constant(call_argument(expr, 2), &immediate)) {
            rcc_error(expr->loc, "SSE shuffle requires a constant immediate");
            return true;
        }
        emit_sse128_immediate(mod, 0, 1,
                              strcmp(name, "shufpd") == 0 ? 0x66 : 0,
                              0xC6, (uint8_t)immediate);
        gen_sse_store_result32(mod, expr, 0);
        return true;
    }
    if (strcmp(name, "pshufd") == 0 || strcmp(name, "pshufhw") == 0 ||
        strcmp(name, "pshuflw") == 0) {
        gen_sse_vector_argument32(mod, call_argument(expr, 0), 0);
        if (!expr_eval_integer_constant(call_argument(expr, 1), &immediate)) {
            rcc_error(expr->loc, "SSE integer shuffle requires a constant immediate");
            return true;
        }
        prefix = strcmp(name, "pshufhw") == 0 ? 0xF3 :
                 strcmp(name, "pshuflw") == 0 ? 0xF2 : 0x66;
        emit_sse128_immediate(mod, 0, 0, prefix, 0x70, (uint8_t)immediate);
        gen_sse_store_result32(mod, expr, 0);
        return true;
    }
    if (strstr(name, "cvt") == name) {
        if (strcmp(name, "cvtsi2ss") == 0 ||
            strcmp(name, "cvtsi642ss") == 0) {
            gen_expr(mod, call_argument(expr, 0));
            emit_int_to_sse32(mod, 0, EAX,
                              strcmp(name, "cvtsi642ss") == 0 ? 8 : 4, 4);
            gen_sse_store_result32(mod, expr, 0);
            return true;
        }
        if (strcmp(name, "cvtss2si") == 0 || strcmp(name, "cvttss2si") == 0 ||
            strcmp(name, "cvtss2si64") == 0 ||
            strcmp(name, "cvttss2si64") == 0) {
            gen_sse_vector_argument32(mod, call_argument(expr, 0), 0);
            emit_sse_to_int_opcode32(mod, EAX, 0, 4,
                                     strstr(name, "cvtt") ? 0x2C : 0x2D);
            return true;
        }
        gen_sse_vector_argument32(mod, call_argument(expr, 0), 0);
        prefix = strcmp(name, "cvtpd2ps") == 0 ? 0x66 :
                 strcmp(name, "cvtdq2pd") == 0 ? 0xF3 :
                 strcmp(name, "cvttps2dq") == 0 ? 0xF3 :
                 (strcmp(name, "cvtps2dq") == 0 ||
                  strcmp(name, "cvttpd2dq") == 0) ? 0x66 : 0;
        opcode = (strcmp(name, "cvtdq2pd") == 0 ||
                  strcmp(name, "cvttpd2dq") == 0) ? 0xE6 :
                 (strcmp(name, "cvtdq2ps") == 0 ||
                  strcmp(name, "cvtps2dq") == 0 ||
                  strcmp(name, "cvttps2dq") == 0) ? 0x5B : 0x5A;
        emit_sse128_reg(mod, 0, 0, prefix, opcode);
        gen_sse_store_result32(mod, expr, 0);
        return true;
    }
    if (strcmp(name, "vec_ext_v8hi") == 0) {
        gen_expr(mod, call_argument(expr, 0));
        if (!expr_eval_integer_constant(call_argument(expr, 1), &immediate)) {
            rcc_error(expr->loc, "vector extract requires a constant lane");
            return true;
        }
        emit_load_typed32(mod, EAX, EAX, (int32_t)immediate * 2, type_short);
        return true;
    }
    if (strcmp(name, "vec_set_v8hi") == 0) {
        emit_sub_reg_imm(mod, ESP, 4);
        emit_byte(mod, 0x8D); emit_byte(mod, modrm(2, EAX, EBP));
        emit_dword(mod, (uint32_t)expr->call_result_offset);
        emit_mov_mem_reg(mod, ESP, 0, EAX);
        for (int index = 0; index < 8; ++index) {
            gen_expr(mod, call_argument(expr, index));
            emit_mov_reg_mem(mod, ECX, ESP, 0);
            emit_store_typed32(mod, ECX, index * 2, EAX, type_short);
        }
        emit_mov_reg_mem(mod, EAX, ESP, 0);
        emit_add_reg_imm(mod, ESP, 4);
        return true;
    }
    if (strncmp(name, "psll", 4) == 0 || strncmp(name, "psrl", 4) == 0 ||
        strncmp(name, "psra", 4) == 0) {
        gen_sse_vector_argument32(mod, call_argument(expr, 0), 0);
        if (expr_eval_integer_constant(call_argument(expr, 1), &immediate)) {
            int group = strstr(name, "psra") ? 4 : strstr(name, "psrl") ? 2 : 6;
            opcode = strstr(name, "w") ? 0x71 : strstr(name, "d") ? 0x72 : 0x73;
            emit_sse128_immediate_group(mod, group, 0, 0x66, opcode,
                                        (uint8_t)immediate);
        } else {
            gen_expr(mod, call_argument(expr, 1));
            emit_int_to_sse32(mod, 1, EAX, 4, 4);
            opcode = strstr(name, "w") ? 0xF1 : strstr(name, "d") ? 0xF2 : 0xF3;
            emit_sse128_reg(mod, 0, 1, 0x66, opcode);
        }
        gen_sse_store_result32(mod, expr, 0);
        return true;
    }
    rcc_error(expr->loc, "unsupported SSE/SSE2 compiler builtin '%s'", full_name);
    return true;
}

static bool gen_mmx_builtin32(Module* mod, Expr* expr) {
    RccCodegenMmxKind kind;
    Expr* argument;
    int opcode;
    int group = 0;
    int64_t amount = 0;
    bool immediate_constant = false;
    int index;

    if (!expr || !expr->call_func || expr->call_func->kind != EXPR_IDENT) {
        return false;
    }
    kind = codegen_mmx_builtin_kind(expr->call_func->ident_name);
    if (kind == RCC_CODEGEN_MMX_NONE) return false;
    if (kind == RCC_CODEGEN_MMX_STORE) {
        emit_sub_reg_imm(mod, ESP, 16);
        gen_expr(mod, call_argument(expr, 0));
        emit_mov_mem_reg(mod, ESP, 0, EAX);
        gen_expr_as_integer64(mod, call_argument(expr, 1));
        emit_mov_mem_reg(mod, ESP, 8, EAX);
        emit_mov_mem_reg(mod, ESP, 12, EDX);
        emit_mmx_load(mod, 0, ESP, 8);
        emit_mov_reg_mem(mod, ECX, ESP, 0);
        emit_mmx_non_temporal_store(mod, ECX, 0, 0);
        emit_add_reg_imm(mod, ESP, 16);
        return true;
    }
    if (kind == RCC_CODEGEN_MMX_INIT_V2SI ||
        kind == RCC_CODEGEN_MMX_INIT_V4HI ||
        kind == RCC_CODEGEN_MMX_INIT_V8QI) {
        emit_sub_reg_imm(mod, ESP, 32);
        for (index = 0; index <
             (kind == RCC_CODEGEN_MMX_INIT_V2SI ? 2 :
              kind == RCC_CODEGEN_MMX_INIT_V4HI ? 4 : 8); ++index) {
            gen_expr(mod, call_argument(expr, index));
            emit_mov_mem_reg(mod, ESP, 16 + index * 4, EAX);
        }
        if (kind == RCC_CODEGEN_MMX_INIT_V2SI) {
            emit_mov_reg_mem(mod, EAX, ESP, 16);
            emit_mov_reg_mem(mod, EDX, ESP, 20);
        } else {
            emit_mov_reg_imm(mod, EAX, 0);
            emit_mov_reg_imm(mod, EDX, 0);
            for (index = 0; index <
                 (kind == RCC_CODEGEN_MMX_INIT_V4HI ? 4 : 8); ++index) {
                emit_mov_reg_mem(mod, ECX, ESP, 16 + index * 4);
                emit_and_reg_imm(mod, ECX,
                                 kind == RCC_CODEGEN_MMX_INIT_V4HI
                                     ? 0xFFFFu : 0xFFu);
                if (kind == RCC_CODEGEN_MMX_INIT_V4HI) {
                    emit_shl_reg_imm(mod, ECX,
                        (uint8_t)((index & 1) * 16));
                    emit_or_reg_reg(mod, index >= 2 ? EDX : EAX, ECX);
                } else {
                    if (index >= 4) emit_shl_reg_imm(mod, ECX,
                        (uint8_t)((index - 4) * 8));
                    else emit_shl_reg_imm(mod, ECX, (uint8_t)(index * 8));
                    emit_or_reg_reg(mod, index >= 4 ? EDX : EAX, ECX);
                }
            }
        }
        emit_add_reg_imm(mod, ESP, 32);
        return true;
    }

    emit_sub_reg_imm(mod, ESP, 16);
    gen_expr_as_integer64(mod, call_argument(expr, 0));
    emit_mov_mem_reg(mod, ESP, 0, EAX);
    emit_mov_mem_reg(mod, ESP, 4, EDX);
    if (kind == RCC_CODEGEN_MMX_IMMEDIATE_SHIFT) {
        argument = call_argument(expr, 1);
        immediate_constant = expr_eval_integer_constant(argument, &amount);
        if (!immediate_constant) {
            gen_expr_as_integer64(mod, argument);
            emit_mov_mem_reg(mod, ESP, 8, EAX);
            emit_mov_mem_reg(mod, ESP, 12, EDX);
        }
    } else {
        gen_expr_as_integer64(mod, call_argument(expr, 1));
        emit_mov_mem_reg(mod, ESP, 8, EAX);
        emit_mov_mem_reg(mod, ESP, 12, EDX);
    }
    emit_mmx_load(mod, 0, ESP, 0);
    if (kind == RCC_CODEGEN_MMX_BINARY) {
        emit_mmx_load(mod, 1, ESP, 8);
        opcode = codegen_mmx_binary_opcode(expr->call_func->ident_name);
        emit_mmx_binary(mod, opcode);
    } else if (kind == RCC_CODEGEN_MMX_VARIABLE_SHIFT) {
        emit_mmx_load(mod, 1, ESP, 8);
        opcode = codegen_mmx_shift_opcode(expr->call_func->ident_name, &group);
        emit_mmx_binary(mod, opcode);
    } else if (immediate_constant) {
        opcode = codegen_mmx_shift_opcode(expr->call_func->ident_name, &group);
        emit_mmx_immediate_shift(mod, opcode, group, (uint8_t)amount);
    } else {
        emit_mmx_load(mod, 1, ESP, 8);
        opcode = codegen_mmx_runtime_shift_opcode(
            expr->call_func->ident_name);
        emit_mmx_binary(mod, opcode);
    }
    emit_mmx_store(mod, ESP, 0, 0);
    emit_mov_reg_mem(mod, EAX, ESP, 0);
    emit_mov_reg_mem(mod, EDX, ESP, 4);
    emit_add_reg_imm(mod, ESP, 16);
    return true;
}

static const Type* atomic_value_type(Expr* call) {
    Expr* object = call_argument(call, 0);
    return object && object->type && object->type->kind == TYPE_PTR
        ? object->type->base : type_uint;
}

static void emit_normalize_atomic_value(Module* mod, int reg,
                                        const Type* type) {
    int width = gen_type_width32(type);
    if (type && type->is_bit_precise && type->bit_width < 32u) {
        uint32_t value_mask =
            (UINT32_C(1) << type->bit_width) - UINT32_C(1);
        emit_and_reg_imm(mod, reg, value_mask);
        if (!type->is_unsigned) {
            uint8_t shift = (uint8_t)(32u - type->bit_width);
            emit_shl_reg_imm(mod, reg, shift);
            emit_sar_reg_imm(mod, reg, shift);
        }
    } else if (type && type->kind == TYPE_BOOL) {
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

typedef enum {
    ATOMIC_MINMAX_NONE,
    ATOMIC_MINMAX_MIN,
    ATOMIC_MINMAX_MAX
} AtomicMinMaxOp;

static AtomicMinMaxOp atomic_minmax_operation(const char* name) {
    if (strcmp(name, "__atomic_fetch_min") == 0)
        return ATOMIC_MINMAX_MIN;
    if (strcmp(name, "__atomic_fetch_max") == 0)
        return ATOMIC_MINMAX_MAX;
    return ATOMIC_MINMAX_NONE;
}

typedef enum {
    ATOMIC_WRAP_NONE,
    ATOMIC_WRAP_INCREMENT,
    ATOMIC_WRAP_DECREMENT
} AtomicWrapOp;

static AtomicWrapOp atomic_wrap_operation(const char* name) {
    if (strcmp(name, "__atomic_fetch_uinc") == 0)
        return ATOMIC_WRAP_INCREMENT;
    if (strcmp(name, "__atomic_fetch_udec") == 0)
        return ATOMIC_WRAP_DECREMENT;
    return ATOMIC_WRAP_NONE;
}

/* A plain aligned x86 load is atomic for all scalar widths supported by the
 * frontend.  i686 has no single-instruction 64-bit load, so use CMPXCHG8B
 * with a zero desired value; success leaves the compared zero in EDX:EAX and
 * failure returns the observed object value there. */
static void emit_atomic_load_i686(Module* mod, const Type* type) {
    if (!type || type->size != 8) {
        emit_load_typed32(mod, EAX, EAX, 0, type);
        return;
    }
    emit_push_reg(mod, EBX);
    emit_push_reg(mod, ESI);
    emit_mov_reg_reg(mod, ESI, EAX);
    emit_xor_reg_reg(mod, EAX, EAX);
    emit_xor_reg_reg(mod, EDX, EDX);
    emit_mov_reg_reg(mod, EBX, EAX);
    emit_mov_reg_reg(mod, ECX, EDX);
    emit_atomic_cmpxchg8b(mod, ESI);
    emit_pop_reg(mod, ESI);
    emit_pop_reg(mod, EBX);
}

/* Store a 64-bit value whose low/high words are at [ESP]/[ESP+4] into the
 * address currently held in EAX.  The compare/exchange loop provides the
 * same sequentially-consistent store used by the existing builtin path. */
static void emit_atomic_store64_i686(Module* mod) {
    int retry_label = new_label();
    emit_push_reg(mod, EBX);
    emit_push_reg(mod, ESI);
    emit_mov_reg_reg(mod, ESI, EAX);
    emit_mov_reg_mem(mod, EAX, ESI, 0);
    emit_mov_reg_mem(mod, EDX, ESI, 4);
    emit_label(mod, retry_label);
    emit_mov_reg_mem(mod, EBX, ESP, 8);
    emit_mov_reg_mem(mod, ECX, ESP, 12);
    emit_atomic_cmpxchg8b(mod, ESI);
    emit_jcc_label(mod, CC_NE, retry_label);
    emit_pop_reg(mod, ESI);
    emit_pop_reg(mod, EBX);
}

static bool gen_atomic_builtin64_i686(Module* mod, Expr* call,
                                      const char* name) {
    int retry_label;
    const Type* value_type = atomic_value_type(call);
    bool generic_aggregate = value_type && value_type->size == 8 &&
        (value_type->kind == TYPE_STRUCT || value_type->kind == TYPE_UNION) &&
        (strcmp(name, "__atomic_load") == 0 ||
         strcmp(name, "__atomic_store") == 0 ||
         strcmp(name, "__atomic_exchange") == 0 ||
         strcmp(name, "__atomic_compare_exchange") == 0);
    bool floating_double = value_type && value_type->kind == TYPE_DOUBLE &&
                           value_type->size == 8;
    AtomicMinMaxOp minmax_operation = atomic_minmax_operation(name);
    AtomicWrapOp wrap_operation = atomic_wrap_operation(name);

    if ((strncmp(name, "__atomic_", 9) != 0 &&
         strncmp(name, "__sync_", 7) != 0) ||
        g_opts.target_arch != ARCH_X86 ||
        (!gen_is_integer64(value_type) && !floating_double &&
         !generic_aggregate)) {
        return false;
    }

    if (minmax_operation != ATOMIC_MINMAX_NONE) {
        const Type* value_type = atomic_value_type(call);
        int select_old_label = new_label();
        int select_operand_label = new_label();
        int compare_low_label = new_label();
        int compare_exchange_label = new_label();
        int is_min = minmax_operation == ATOMIC_MINMAX_MIN;
        int signed_type = value_type && !value_type->is_unsigned;

        /* The locked CMPXCHG8B loop is a full barrier, which is stronger
         * than every permitted order for this read-modify-write builtin. */
        gen_expr(mod, call_argument(call, 2));
        emit_push_reg(mod, EBX);
        emit_push_reg(mod, ESI);
        gen_expr(mod, call_argument(call, 0));
        emit_push_reg(mod, EAX); /* Object address. */
        gen_expr_as_integer64(mod, call_argument(call, 1));
        emit_push_reg(mod, EDX); /* Operand high word. */
        emit_push_reg(mod, EAX); /* Operand low word. */

        emit_mov_reg_mem(mod, ESI, ESP, 8);
        emit_mov_reg_mem(mod, EAX, ESI, 0);
        emit_mov_reg_mem(mod, EDX, ESI, 4);
        retry_label = new_label();
        emit_label(mod, retry_label);
        emit_mov_reg_mem(mod, ECX, ESP, 4);
        emit_cmp_reg_reg(mod, EDX, ECX);
        if (is_min) {
            emit_jcc_label(mod, signed_type ? CC_L : CC_B,
                           select_old_label);
            emit_jcc_label(mod, signed_type ? CC_G : CC_A,
                           select_operand_label);
        } else {
            emit_jcc_label(mod, signed_type ? CC_G : CC_A,
                           select_old_label);
            emit_jcc_label(mod, signed_type ? CC_L : CC_B,
                           select_operand_label);
        }
        emit_jmp_label(mod, compare_low_label);

        emit_label(mod, compare_low_label);
        emit_mov_reg_mem(mod, ECX, ESP, 0);
        emit_cmp_reg_reg(mod, EAX, ECX);
        if (is_min) {
            emit_jcc_label(mod, CC_B, select_old_label);
            emit_jcc_label(mod, CC_A, select_operand_label);
        } else {
            emit_jcc_label(mod, CC_A, select_old_label);
            emit_jcc_label(mod, CC_B, select_operand_label);
        }
        emit_jmp_label(mod, select_old_label);

        emit_label(mod, select_operand_label);
        emit_mov_reg_mem(mod, EBX, ESP, 0);
        emit_mov_reg_mem(mod, ECX, ESP, 4);
        emit_jmp_label(mod, compare_exchange_label);

        emit_label(mod, select_old_label);
        emit_mov_reg_reg(mod, EBX, EAX);
        emit_mov_reg_reg(mod, ECX, EDX);

        emit_label(mod, compare_exchange_label);
        emit_atomic_cmpxchg8b(mod, ESI);
        emit_jcc_label(mod, CC_NE, retry_label);
        emit_add_reg_imm(mod, ESP, 12);
        emit_pop_reg(mod, ESI);
        emit_pop_reg(mod, EBX);
        return true;
    }

    if (wrap_operation != ATOMIC_WRAP_NONE) {
        int retry_label = new_label();
        int update_label = new_label();
        int wrap_label = new_label();
        int decrement_label = new_label();
        int compare_exchange_label = new_label();

        /* CMPXCHG8B gives a full barrier, which is stronger than each valid
         * order for these unsigned read-modify-write builtins. */
        gen_expr(mod, call_argument(call, 2));
        emit_push_reg(mod, EBX);
        emit_push_reg(mod, ESI);
        gen_expr(mod, call_argument(call, 0));
        emit_mov_reg_reg(mod, ESI, EAX);
        gen_expr_as_integer64(mod, call_argument(call, 1));
        emit_push_reg(mod, EDX); /* Wrap limit high word. */
        emit_push_reg(mod, EAX); /* Wrap limit low word. */
        emit_mov_reg_mem(mod, EAX, ESI, 0);
        emit_mov_reg_mem(mod, EDX, ESI, 4);

        emit_label(mod, retry_label);
        if (wrap_operation == ATOMIC_WRAP_INCREMENT) {
            emit_mov_reg_mem(mod, ECX, ESP, 4);
            emit_cmp_reg_reg(mod, EDX, ECX);
            emit_jcc_label(mod, CC_A, wrap_label);
            emit_jcc_label(mod, CC_B, update_label);
            emit_mov_reg_mem(mod, ECX, ESP, 0);
            emit_cmp_reg_reg(mod, EAX, ECX);
            emit_jcc_label(mod, CC_AE, wrap_label);
            emit_label(mod, update_label);
            emit_mov_reg_reg(mod, EBX, EAX);
            emit_mov_reg_reg(mod, ECX, EDX);
            emit_add_reg_imm(mod, EBX, 1);
            emit_adc_reg_imm8(mod, ECX, 0);
            emit_jmp_label(mod, compare_exchange_label);
            emit_label(mod, wrap_label);
            emit_mov_reg_imm(mod, EBX, 0u);
            emit_mov_reg_imm(mod, ECX, 0u);
        } else {
            emit_cmp_reg_imm(mod, EDX, 0);
            emit_jcc_label(mod, CC_NE, update_label);
            emit_cmp_reg_imm(mod, EAX, 0);
            emit_jcc_label(mod, CC_E, wrap_label);
            emit_label(mod, update_label);
            emit_mov_reg_mem(mod, ECX, ESP, 4);
            emit_cmp_reg_reg(mod, EDX, ECX);
            emit_jcc_label(mod, CC_A, wrap_label);
            emit_jcc_label(mod, CC_B, decrement_label);
            emit_mov_reg_mem(mod, ECX, ESP, 0);
            emit_cmp_reg_reg(mod, EAX, ECX);
            emit_jcc_label(mod, CC_A, wrap_label);
            emit_label(mod, decrement_label);
            emit_mov_reg_reg(mod, EBX, EAX);
            emit_mov_reg_reg(mod, ECX, EDX);
            emit_sub_reg_imm(mod, EBX, 1);
            emit_sbb_reg_imm8(mod, ECX, 0);
            emit_jmp_label(mod, compare_exchange_label);
            emit_label(mod, wrap_label);
            emit_mov_reg_mem(mod, EBX, ESP, 0);
            emit_mov_reg_mem(mod, ECX, ESP, 4);
        }

        emit_label(mod, compare_exchange_label);
        emit_atomic_cmpxchg8b(mod, ESI);
        emit_jcc_label(mod, CC_NE, retry_label);
        emit_add_reg_imm(mod, ESP, 8);
        emit_pop_reg(mod, ESI);
        emit_pop_reg(mod, EBX);
        return true;
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

    if (strcmp(name, "__atomic_load") == 0) {
        /* The generic form writes the loaded pair through its result
         * pointer.  Keep both pointers on the expression stack while the
         * CMPXCHG8B load executes so evaluation remains ordered. */
        gen_expr(mod, call_argument(call, 2));
        emit_push_reg(mod, EBX);
        emit_push_reg(mod, ESI);
        emit_push_reg(mod, EDI);
        gen_expr(mod, call_argument(call, 0));
        emit_push_reg(mod, EAX);
        gen_expr(mod, call_argument(call, 1));
        emit_push_reg(mod, EAX);
        emit_mov_reg_mem(mod, ESI, ESP, 4);
        emit_xor_reg_reg(mod, EAX, EAX);
        emit_xor_reg_reg(mod, EDX, EDX);
        emit_mov_reg_reg(mod, EBX, EAX);
        emit_mov_reg_reg(mod, ECX, EDX);
        emit_atomic_cmpxchg8b(mod, ESI);
        emit_pop_reg(mod, EDI);
        emit_mov_mem_reg(mod, EDI, 0, EAX);
        emit_mov_mem_reg(mod, EDI, 4, EDX);
        emit_add_reg_imm(mod, ESP, 4);
        emit_pop_reg(mod, EDI);
        emit_pop_reg(mod, ESI);
        emit_pop_reg(mod, EBX);
        return true;
    }

    if (strcmp(name, "__atomic_store") == 0) {
        gen_expr(mod, call_argument(call, 2));
        emit_push_reg(mod, EBX);
        emit_push_reg(mod, ESI);
        emit_push_reg(mod, EDI);
        gen_expr(mod, call_argument(call, 0));
        emit_push_reg(mod, EAX);
        gen_expr(mod, call_argument(call, 1));
        emit_push_reg(mod, EAX);
        emit_mov_reg_mem(mod, ESI, ESP, 4);
        emit_mov_reg_mem(mod, EDI, ESP, 0);
        emit_mov_reg_mem(mod, EAX, EDI, 0);
        emit_mov_reg_mem(mod, EDX, EDI, 4);
        emit_mov_reg_reg(mod, EBX, EAX);
        emit_mov_reg_reg(mod, ECX, EDX);
        emit_mov_reg_mem(mod, EAX, ESI, 0);
        emit_mov_reg_mem(mod, EDX, ESI, 4);
        retry_label = new_label();
        emit_label(mod, retry_label);
        emit_atomic_cmpxchg8b(mod, ESI);
        emit_jcc_label(mod, CC_NE, retry_label);
        emit_add_reg_imm(mod, ESP, 8);
        emit_pop_reg(mod, EDI);
        emit_pop_reg(mod, ESI);
        emit_pop_reg(mod, EBX);
        return true;
    }

    if (strcmp(name, "__atomic_exchange") == 0) {
        gen_expr(mod, call_argument(call, 3));
        emit_push_reg(mod, EBX);
        emit_push_reg(mod, ESI);
        emit_push_reg(mod, EDI);
        gen_expr(mod, call_argument(call, 0));
        emit_push_reg(mod, EAX);
        gen_expr(mod, call_argument(call, 1));
        emit_push_reg(mod, EAX);
        gen_expr(mod, call_argument(call, 2));
        emit_push_reg(mod, EAX);
        emit_mov_reg_mem(mod, ESI, ESP, 8);
        emit_mov_reg_mem(mod, EDI, ESP, 4);
        emit_mov_reg_mem(mod, EBX, EDI, 0);
        emit_mov_reg_mem(mod, ECX, EDI, 4);
        emit_mov_reg_mem(mod, EAX, ESI, 0);
        emit_mov_reg_mem(mod, EDX, ESI, 4);
        retry_label = new_label();
        emit_label(mod, retry_label);
        emit_atomic_cmpxchg8b(mod, ESI);
        emit_jcc_label(mod, CC_NE, retry_label);
        emit_mov_reg_mem(mod, EDI, ESP, 0);
        emit_mov_mem_reg(mod, EDI, 0, EAX);
        emit_mov_mem_reg(mod, EDI, 4, EDX);
        emit_add_reg_imm(mod, ESP, 12);
        emit_pop_reg(mod, EDI);
        emit_pop_reg(mod, ESI);
        emit_pop_reg(mod, EBX);
        return true;
    }

    if (strcmp(name, "__atomic_compare_exchange") == 0) {
        gen_expr(mod, call_argument(call, 5));
        gen_expr(mod, call_argument(call, 4));
        gen_expr(mod, call_argument(call, 3));
        emit_push_reg(mod, EBX);
        emit_push_reg(mod, ESI);
        emit_push_reg(mod, EDI);
        gen_expr(mod, call_argument(call, 0));
        emit_push_reg(mod, EAX);
        gen_expr(mod, call_argument(call, 1));
        emit_push_reg(mod, EAX);
        gen_expr(mod, call_argument(call, 2));
        emit_push_reg(mod, EAX);
        emit_mov_reg_mem(mod, ESI, ESP, 8);
        emit_mov_reg_mem(mod, EDI, ESP, 4);
        emit_mov_reg_mem(mod, EAX, EDI, 0);
        emit_mov_reg_mem(mod, EDX, EDI, 4);
        emit_mov_reg_mem(mod, EBX, ESP, 0);
        emit_mov_reg_mem(mod, ECX, EBX, 4);
        emit_mov_reg_mem(mod, EBX, EBX, 0);
        emit_atomic_cmpxchg8b(mod, ESI);
        emit_setcc(mod, CC_E, EDI);
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xB6);
        emit_byte(mod, modrm(3, EDI, EDI));
        emit_mov_reg_mem(mod, ECX, ESP, 4);
        emit_mov_mem_reg(mod, ECX, 0, EAX);
        emit_mov_mem_reg(mod, ECX, 4, EDX);
        emit_mov_reg_reg(mod, EAX, EDI);
        emit_add_reg_imm(mod, ESP, 12);
        emit_pop_reg(mod, EDI);
        emit_pop_reg(mod, ESI);
        emit_pop_reg(mod, EBX);
        return true;
    }

    if (strcmp(name, "__atomic_store_n") == 0 ||
        strcmp(name, "__atomic_exchange_n") == 0 ||
        strcmp(name, "__sync_lock_test_and_set") == 0 ||
        strcmp(name, "__sync_swap") == 0) {
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

static bool atomic_lock_free_size32(uint64_t size) {
    return size == 1u || size == 2u || size == 4u || size == 8u;
}

static bool gen_atomic_lock_free_query32(Module* mod, Expr* call) {
    const char* name = call && call->call_func
        ? call->call_func->ident_name : NULL;
    Expr* size_expression;
    Expr* pointer_expression;
    int64_t size_value = 0;
    int true_label;
    int end_label;
    static const uint32_t lock_free_sizes[] = {1u, 2u, 4u, 8u};
    size_t index;

    if (!name || (strcmp(name, "__atomic_always_lock_free") != 0 &&
                  strcmp(name, "__atomic_is_lock_free") != 0)) {
        return false;
    }
    size_expression = call_argument(call, 0);
    pointer_expression = call_argument(call, 1);
    /* The pointer operand is part of the builtin call expression.  Evaluate
     * it even though this target's lock-free result depends only on size, so
     * side effects are not silently discarded. */
    if (pointer_expression) gen_expr(mod, pointer_expression);
    if (strcmp(name, "__atomic_always_lock_free") == 0) {
        if (!expr_eval_integer_constant(size_expression, &size_value)) {
            emit_mov_reg_imm(mod, EAX, 0u);
        } else {
            emit_mov_reg_imm(mod, EAX,
                             atomic_lock_free_size32((uint64_t)size_value)
                                 ? 1u : 0u);
        }
        return true;
    }
    gen_expr(mod, size_expression);
    true_label = new_label();
    end_label = new_label();
    for (index = 0; index < sizeof(lock_free_sizes) /
                           sizeof(lock_free_sizes[0]); ++index) {
        emit_cmp_reg_imm(mod, EAX, (int32_t)lock_free_sizes[index]);
        emit_jcc_label(mod, CC_E, true_label);
    }
    emit_mov_reg_imm(mod, EAX, 0u);
    emit_jmp_label(mod, end_label);
    emit_label(mod, true_label);
    emit_mov_reg_imm(mod, EAX, 1u);
    emit_label(mod, end_label);
    return true;
}

static bool gen_atomic_builtin(Module* mod, Expr* call) {
    Expr* function = call->call_func;
    const char* name;
    bool is_atomic;
    bool is_subtract;
    bool returns_new;
    AtomicMinMaxOp minmax_operation;
    AtomicWrapOp wrap_operation;
    AtomicBitwiseOp bitwise_operation;
    const Type* value_type;

    if (!function || function->kind != EXPR_IDENT) return false;
    name = function->ident_name;
    value_type = atomic_value_type(call);
    minmax_operation = atomic_minmax_operation(name);
    wrap_operation = atomic_wrap_operation(name);
    if (gen_atomic_lock_free_query32(mod, call)) return true;
    if (gen_atomic_builtin64_i686(mod, call, name)) return true;
    if (wrap_operation != ATOMIC_WRAP_NONE) {
        int retry_label = new_label();
        int wrap_label = new_label();
        int compare_exchange_label = new_label();

        gen_expr(mod, call_argument(call, 2));
        emit_push_reg(mod, EBX);
        emit_push_reg(mod, ESI);
        emit_push_reg(mod, EDI);
        gen_expr(mod, call_argument(call, 0));
        emit_mov_reg_reg(mod, ESI, EAX);
        gen_expr(mod, call_argument(call, 1));
        emit_normalize_atomic_value(mod, EAX, value_type);
        emit_push_reg(mod, EAX); /* Wrap limit. */
        emit_load_typed32(mod, EAX, ESI, 0, value_type);

        emit_label(mod, retry_label);
        if (wrap_operation == ATOMIC_WRAP_INCREMENT) {
            emit_mov_reg_mem(mod, ECX, ESP, 0);
            emit_cmp_reg_reg(mod, EAX, ECX);
            emit_jcc_label(mod, CC_AE, wrap_label);
            emit_mov_reg_reg(mod, EDX, EAX);
            emit_add_reg_imm(mod, EDX, 1);
            emit_normalize_atomic_value(mod, EDX, value_type);
            emit_jmp_label(mod, compare_exchange_label);
        } else {
            emit_cmp_reg_imm(mod, EAX, 0);
            emit_jcc_label(mod, CC_E, wrap_label);
            emit_mov_reg_mem(mod, ECX, ESP, 0);
            emit_cmp_reg_reg(mod, EAX, ECX);
            emit_jcc_label(mod, CC_A, wrap_label);
            emit_mov_reg_reg(mod, EDX, EAX);
            emit_sub_reg_imm(mod, EDX, 1);
            emit_normalize_atomic_value(mod, EDX, value_type);
            emit_jmp_label(mod, compare_exchange_label);
        }
        emit_label(mod, wrap_label);
        if (wrap_operation == ATOMIC_WRAP_INCREMENT)
            emit_mov_reg_imm(mod, EDX, 0u);
        else
            emit_mov_reg_mem(mod, EDX, ESP, 0);
        emit_normalize_atomic_value(mod, EDX, value_type);
        emit_label(mod, compare_exchange_label);
        emit_atomic_cmpxchg_width(mod, EDX, ESI, value_type);
        emit_jcc_label(mod, CC_NE, retry_label);
        emit_normalize_atomic_value(mod, EAX, value_type);
        emit_add_reg_imm(mod, ESP, 4);
        emit_pop_reg(mod, EDI);
        emit_pop_reg(mod, ESI);
        emit_pop_reg(mod, EBX);
        return true;
    }
    if (strcmp(name, "__atomic_load") == 0) {
        gen_expr(mod, call_argument(call, 2));
        gen_expr(mod, call_argument(call, 0));
        emit_push_reg(mod, EAX);
        gen_expr(mod, call_argument(call, 1));
        emit_push_reg(mod, EAX);
        emit_mov_reg_mem(mod, ECX, ESP, 4);
        emit_load_typed32(mod, EAX, ECX, 0, value_type);
        emit_pop_reg(mod, ECX);
        emit_store_typed32(mod, ECX, 0, EAX, value_type);
        emit_add_reg_imm(mod, ESP, 4);
        return true;
    }
    if (strcmp(name, "__atomic_store") == 0) {
        gen_expr(mod, call_argument(call, 2));
        gen_expr(mod, call_argument(call, 0));
        emit_push_reg(mod, EAX);
        gen_expr(mod, call_argument(call, 1));
        emit_push_reg(mod, EAX);
        /* The expression stack contains desired* at [ESP] and object at
         * [ESP+4].  Load the value from desired*, but perform the atomic
         * exchange against the object address. */
        emit_mov_reg_mem(mod, ECX, ESP, 0);
        emit_load_typed32(mod, EAX, ECX, 0, value_type);
        emit_mov_reg_mem(mod, ECX, ESP, 4);
        emit_atomic_exchange_width(mod, EAX, ECX, value_type);
        emit_add_reg_imm(mod, ESP, 8);
        return true;
    }
    if (strcmp(name, "__atomic_exchange") == 0) {
        gen_expr(mod, call_argument(call, 3));
        gen_expr(mod, call_argument(call, 0));
        emit_push_reg(mod, EAX);
        gen_expr(mod, call_argument(call, 1));
        emit_push_reg(mod, EAX);
        gen_expr(mod, call_argument(call, 2));
        emit_push_reg(mod, EAX);
        emit_mov_reg_mem(mod, ECX, ESP, 4);
        emit_load_typed32(mod, EAX, ECX, 0, value_type);
        emit_mov_reg_mem(mod, ECX, ESP, 8);
        emit_atomic_exchange_width(mod, EAX, ECX, value_type);
        emit_mov_reg_mem(mod, ECX, ESP, 0);
        emit_store_typed32(mod, ECX, 0, EAX, value_type);
        emit_add_reg_imm(mod, ESP, 12);
        return true;
    }
    if (strcmp(name, "__atomic_compare_exchange") == 0) {
        gen_expr(mod, call_argument(call, 5));
        gen_expr(mod, call_argument(call, 4));
        gen_expr(mod, call_argument(call, 3));
        gen_expr(mod, call_argument(call, 0));
        emit_push_reg(mod, EAX);
        gen_expr(mod, call_argument(call, 1));
        emit_push_reg(mod, EAX);
        gen_expr(mod, call_argument(call, 2));
        emit_push_reg(mod, EAX);
        emit_mov_reg_mem(mod, ECX, ESP, 4);
        emit_load_typed32(mod, EAX, ECX, 0, value_type);
        emit_mov_reg_mem(mod, EDX, ESP, 0);
        emit_load_typed32(mod, EDX, EDX, 0, value_type);
        emit_mov_reg_mem(mod, ECX, ESP, 8);
        emit_atomic_cmpxchg_width(mod, EDX, ECX, value_type);
        emit_setcc(mod, CC_E, EDX);
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xB6);
        emit_byte(mod, modrm(3, EDX, EDX));
        emit_mov_reg_mem(mod, ECX, ESP, 4);
        emit_store_typed32(mod, ECX, 0, EAX, value_type);
        emit_mov_reg_reg(mod, EAX, EDX);
        emit_add_reg_imm(mod, ESP, 12);
        return true;
    }
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
    if (strcmp(name, "__atomic_test_and_set") == 0) {
        gen_expr(mod, call_argument(call, 1));
        gen_expr(mod, call_argument(call, 0));
        emit_push_reg(mod, EAX);
        emit_mov_reg_imm(mod, EAX, 1u);
        emit_pop_reg(mod, ECX);
        emit_atomic_exchange_width(mod, EAX, ECX, value_type);
        emit_byte(mod, 0x85);
        emit_byte(mod, modrm(3, EAX, EAX));
        emit_setcc(mod, CC_NE, EAX);
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xB6);
        emit_byte(mod, modrm(3, EAX, EAX));
        return true;
    }
    if (strcmp(name, "__atomic_clear") == 0) {
        gen_expr(mod, call_argument(call, 1));
        gen_expr(mod, call_argument(call, 0));
        emit_atomic_clear_width(mod, EAX, value_type);
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
        strcmp(name, "__sync_lock_test_and_set") == 0 ||
        strcmp(name, "__sync_swap") == 0) {
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
    if (minmax_operation != ATOMIC_MINMAX_NONE) {
        int retry_label = new_label();
        int failed_compare_exchange_label = new_label();
        int use_operand_label = new_label();
        int compare_exchange_label = new_label();
        int done_label = new_label();
        int signed_type = value_type && !value_type->is_unsigned;
        int is_min = minmax_operation == ATOMIC_MINMAX_MIN;

        if (is_atomic) gen_expr(mod, call_argument(call, 2));
        gen_expr(mod, call_argument(call, 0));
        emit_push_reg(mod, EAX); /* Object address. */
        gen_expr(mod, call_argument(call, 1));
        emit_normalize_atomic_value(mod, EAX, value_type);
        emit_push_reg(mod, EAX); /* Operand. */
        emit_mov_reg_mem(mod, ECX, ESP, 4);
        emit_load_typed32(mod, EAX, ECX, 0, value_type);

        emit_label(mod, retry_label);
        emit_mov_reg_mem(mod, EDX, ESP, 0);
        emit_cmp_reg_reg(mod, EAX, EDX);
        if (is_min)
            emit_jcc_label(mod, signed_type ? CC_G : CC_A,
                           use_operand_label);
        else
            emit_jcc_label(mod, signed_type ? CC_L : CC_B,
                           use_operand_label);
        emit_mov_reg_reg(mod, EDX, EAX);
        emit_jmp_label(mod, compare_exchange_label);

        emit_label(mod, use_operand_label);
        emit_mov_reg_mem(mod, EDX, ESP, 0);

        emit_label(mod, compare_exchange_label);
        emit_mov_reg_mem(mod, ECX, ESP, 4);
        emit_atomic_cmpxchg_width(mod, EDX, ECX, value_type);
        emit_jcc_label(mod, CC_NE, failed_compare_exchange_label);
        emit_normalize_atomic_value(mod, EAX, value_type);
        emit_add_reg_imm(mod, ESP, 8);
        emit_jmp_label(mod, done_label);

        /* Byte and word CMPXCHG updates only AL/AX on failure. Normalize its
         * observed value before the next full-width comparison. */
        emit_label(mod, failed_compare_exchange_label);
        emit_normalize_atomic_value(mod, EAX, value_type);
        emit_jmp_label(mod, retry_label);
        emit_label(mod, done_label);
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
    if (strcmp(name, "__atomic_signal_fence") == 0) {
        Expr* order = call_argument(call, 0);
        int64_t constant_order;
        if (!expr_eval_integer_constant(order, &constant_order)) {
            gen_expr(mod, order);
        }
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
                                  int base_register,
                                  int32_t displacement) {
    CxxClass* cls;
    if (!mod || !type || !codegen_type_has_vtable_storage(type)) return;
    if (type->cxx_vtable_size > 0 && type->cxx_vtable_symbol) {
        gen_symbol_address(mod, type->cxx_vtable_symbol, 0u);
        emit_mov_mem_reg(mod, base_register, displacement, EAX);
    }

    cls = type->cxx_class;
    if (!cls) return;
    if (cls->virtual_base_count > 0 &&
        cls->virtual_base_pointer_offset >= 0 &&
        cls->virtual_base_table_symbol) {
        int64_t pointer_displacement = (int64_t)displacement +
                                       cls->virtual_base_pointer_offset;
        if (pointer_displacement < INT32_MIN ||
            pointer_displacement > INT32_MAX) {
            rcc_error((SourceLoc){"<cxx-vbase>", 0, 0},
                      "virtual-base pointer exceeds stack limits");
        } else {
            gen_symbol_address(mod, cls->virtual_base_table_symbol, 0u);
            emit_mov_mem_reg(mod, base_register,
                             (int32_t)pointer_displacement, EAX);
        }
    }
    if (!cls->base_offsets) return;
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
        emit_mov_mem_reg(mod, base_register,
                         (int32_t)base_displacement, EAX);
    }
    for (int virtual_index = 0;
         virtual_index < cls->virtual_base_count; ++virtual_index) {
        CxxVirtualBaseInfo* virtual_base = &cls->virtual_bases[virtual_index];
        const char* virtual_vtable_symbol = NULL;
        int64_t virtual_displacement;
        bool direct_virtual = false;
        if (!virtual_base->base || virtual_base->base->vtable_size <= 0 ||
            virtual_base->offset < 0) {
            continue;
        }
        for (int index = 0; index < cls->base_count; ++index) {
            if (cls->bases[index].is_virtual &&
                cls->bases[index].base == virtual_base->base &&
                cls->base_offsets &&
                cls->base_offsets[index] == virtual_base->offset) {
                direct_virtual = true;
                break;
            }
        }
        if (direct_virtual) continue;
        for (int secondary_index = 0;
             secondary_index < cls->secondary_vtable_count;
             ++secondary_index) {
            CxxSecondaryVtable* secondary =
                &cls->secondary_vtables[secondary_index];
            if (secondary->is_virtual_base &&
                secondary->virtual_base_index == virtual_index) {
                virtual_vtable_symbol = secondary->symbol;
                break;
            }
        }
        if (!virtual_vtable_symbol) {
            rcc_error((SourceLoc){"<cxx-vtable>", 0, 0},
                      "virtual base has no most-derived vtable");
            continue;
        }
        virtual_displacement = (int64_t)displacement + virtual_base->offset;
        if (virtual_displacement < INT32_MIN ||
            virtual_displacement > INT32_MAX) {
            rcc_error((SourceLoc){"<cxx-vtable>", 0, 0},
                      "virtual base vtable pointer exceeds stack limits");
            continue;
        }
        gen_symbol_address(mod, virtual_vtable_symbol, 0u);
        emit_mov_mem_reg(mod, base_register,
                         (int32_t)virtual_displacement, EAX);
    }
    for (int index = 0; index < cls->base_count; ++index) {
        CxxClass* base = cls->bases[index].base;
        const char* table_symbol;
        int64_t pointer_displacement;
        if (!base || base->virtual_base_count <= 0 ||
            base->virtual_base_pointer_offset < 0 ||
            cls->bases[index].is_virtual || cls->base_offsets[index] < 0) {
            continue;
        }
        table_symbol = codegen_cxx_vbase_variant_symbol(cls, false, index);
        if (!table_symbol) continue;
        pointer_displacement = (int64_t)displacement +
                               cls->base_offsets[index] +
                               base->virtual_base_pointer_offset;
        if (pointer_displacement < INT32_MIN ||
            pointer_displacement > INT32_MAX) {
            rcc_error((SourceLoc){"<cxx-vbase>", 0, 0},
                      "base virtual-base pointer exceeds stack limits");
            continue;
        }
        gen_symbol_address(mod, table_symbol, 0u);
        emit_mov_mem_reg(mod, base_register,
                         (int32_t)pointer_displacement, EAX);
    }
    for (int virtual_index = 0;
         virtual_index < cls->virtual_base_count; ++virtual_index) {
        CxxClass* base = cls->virtual_bases[virtual_index].base;
        const char* table_symbol;
        int64_t pointer_displacement;
        if (!base || base->virtual_base_count <= 0 ||
            base->virtual_base_pointer_offset < 0 ||
            cls->virtual_bases[virtual_index].offset < 0) {
            continue;
        }
        table_symbol = codegen_cxx_vbase_variant_symbol(
            cls, true, virtual_index);
        if (!table_symbol) continue;
        pointer_displacement = (int64_t)displacement +
                               cls->virtual_bases[virtual_index].offset +
                               base->virtual_base_pointer_offset;
        if (pointer_displacement < INT32_MIN ||
            pointer_displacement > INT32_MAX) {
            rcc_error((SourceLoc){"<cxx-vbase>", 0, 0},
                      "nested virtual-base pointer exceeds stack limits");
            continue;
        }
        gen_symbol_address(mod, table_symbol, 0u);
        emit_mov_mem_reg(mod, base_register,
                         (int32_t)pointer_displacement, EAX);
    }
}

static const Type* codegen_bitfield_storage_type(const TypeField* field) {
    if (!field || !field->type) return type_uint;
    switch (field->type->size) {
        case 1: return type_uchar;
        case 2: return type_ushort;
        case 8: return type_ullong;
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

/* i686 represents a 64-bit bit-field value as EDX:EAX.  The storage address
 * enters in EAX; fields may straddle the two dwords of their allocation unit. */
static void gen_bitfield_load64_pair(Module* mod, const TypeField* field) {
    emit_mov_reg_reg(mod, ECX, EAX);
    emit_mov_reg_mem(mod, EAX, ECX, 0);
    emit_mov_reg_mem(mod, EDX, ECX, 4);
    if (field->bit_offset < 32u && field->bit_offset != 0u) {
        emit_mov_reg_imm(mod, ECX, field->bit_offset);
        emit_shrd_reg_cl(mod, EAX, EDX);
        emit_shr_reg_cl(mod, EDX);
    } else if (field->bit_offset == 32u) {
        emit_mov_reg_reg(mod, EAX, EDX);
        emit_xor_reg_reg(mod, EDX, EDX);
    } else if (field->bit_offset > 32u) {
        emit_mov_reg_imm(mod, ECX, field->bit_offset - 32u);
        emit_shr_reg_cl(mod, EDX);
        emit_mov_reg_reg(mod, EAX, EDX);
        emit_xor_reg_reg(mod, EDX, EDX);
    }

    if (field->bit_width < 32u) {
        uint32_t mask = (UINT32_C(1) << field->bit_width) - 1u;
        emit_mov_reg_imm(mod, ECX, mask);
        emit_and_reg_reg(mod, EAX, ECX);
        if (field->type && !field->type->is_unsigned) {
            uint8_t extension = (uint8_t)(32u - field->bit_width);
            emit_shl_reg_imm(mod, EAX, extension);
            emit_sar_reg_imm(mod, EAX, extension);
            emit_cdq(mod);
        } else {
            emit_xor_reg_reg(mod, EDX, EDX);
        }
    } else if (field->bit_width == 32u) {
        if (field->type && !field->type->is_unsigned) {
            emit_cdq(mod);
        } else {
            emit_xor_reg_reg(mod, EDX, EDX);
        }
    } else if (field->bit_width < 64u) {
        unsigned high_width = field->bit_width - 32u;
        uint32_t mask = (UINT32_C(1) << high_width) - 1u;
        emit_mov_reg_imm(mod, ECX, mask);
        emit_and_reg_reg(mod, EDX, ECX);
        if (field->type && !field->type->is_unsigned) {
            uint8_t extension = (uint8_t)(32u - high_width);
            emit_shl_reg_imm(mod, EDX, extension);
            emit_sar_reg_imm(mod, EDX, extension);
        }
    }
}

/* EDX:EAX is the new value and ECX is the storage address.  Preserve the
 * neighboring bits in the eight-byte allocation unit and return the value
 * after conversion to the declared bit-field width and signedness. */
static void emit_bitfield_store64_pair(Module* mod, const TypeField* field) {
    uint64_t field_mask = field->bit_width >= 64u
        ? UINT64_MAX
        : (UINT64_C(1) << field->bit_width) - UINT64_C(1);
    uint64_t shifted_mask = field_mask << field->bit_offset;
    uint32_t shifted_low = (uint32_t)shifted_mask;
    uint32_t shifted_high = (uint32_t)(shifted_mask >> 32);

    emit_push_reg(mod, EBX);
    emit_mov_reg_reg(mod, EBX, ECX);
    emit_push_reg(mod, EDX);
    emit_push_reg(mod, EAX);
    emit_mov_reg_mem(mod, EAX, EBX, 0);
    emit_mov_reg_mem(mod, EDX, EBX, 4);
    emit_mov_reg_imm(mod, ECX, ~shifted_low);
    emit_and_reg_reg(mod, EAX, ECX);
    emit_mov_reg_imm(mod, ECX, ~shifted_high);
    emit_and_reg_reg(mod, EDX, ECX);
    emit_push_reg(mod, EDX);
    emit_push_reg(mod, EAX);

    emit_mov_reg_mem(mod, EAX, ESP, 8);
    emit_mov_reg_mem(mod, EDX, ESP, 12);
    if (field->bit_width < 32u) {
        uint32_t mask = (UINT32_C(1) << field->bit_width) - 1u;
        emit_mov_reg_imm(mod, ECX, mask);
        emit_and_reg_reg(mod, EAX, ECX);
        emit_xor_reg_reg(mod, EDX, EDX);
    } else if (field->bit_width < 64u) {
        unsigned high_width = field->bit_width - 32u;
        uint32_t mask = high_width == 32u
            ? UINT32_MAX : (UINT32_C(1) << high_width) - 1u;
        if (field->bit_width == 32u) {
            emit_xor_reg_reg(mod, EDX, EDX);
        } else {
            emit_mov_reg_imm(mod, ECX, mask);
            emit_and_reg_reg(mod, EDX, ECX);
        }
    }
    if (field->bit_offset < 32u && field->bit_offset != 0u) {
        emit_mov_reg_imm(mod, ECX, field->bit_offset);
        emit_shld_reg_cl(mod, EDX, EAX);
        emit_shl_reg_cl(mod, EAX);
    } else if (field->bit_offset == 32u) {
        emit_mov_reg_reg(mod, EDX, EAX);
        emit_xor_reg_reg(mod, EAX, EAX);
    } else if (field->bit_offset > 32u) {
        emit_mov_reg_imm(mod, ECX, field->bit_offset - 32u);
        emit_shl_reg_cl(mod, EAX);
        emit_mov_reg_reg(mod, EDX, EAX);
        emit_xor_reg_reg(mod, EAX, EAX);
    }

    emit_push_reg(mod, EDX);
    emit_push_reg(mod, EAX);
    emit_mov_reg_mem(mod, EAX, ESP, 8);
    emit_mov_reg_mem(mod, EDX, ESP, 12);
    emit_mov_reg_mem(mod, ECX, ESP, 0);
    emit_or_reg_reg(mod, EAX, ECX);
    emit_mov_reg_mem(mod, ECX, ESP, 4);
    emit_or_reg_reg(mod, EDX, ECX);
    emit_mov_mem_reg(mod, EBX, 0, EAX);
    emit_mov_mem_reg(mod, EBX, 4, EDX);
    emit_add_reg_imm(mod, ESP, 16);
    emit_mov_reg_mem(mod, EAX, ESP, 0);
    emit_mov_reg_mem(mod, EDX, ESP, 4);
    emit_add_reg_imm(mod, ESP, 8);

    if (field->bit_width < 32u) {
        uint32_t mask = (UINT32_C(1) << field->bit_width) - 1u;
        emit_mov_reg_imm(mod, ECX, mask);
        emit_and_reg_reg(mod, EAX, ECX);
        if (field->type && !field->type->is_unsigned) {
            uint8_t extension = (uint8_t)(32u - field->bit_width);
            emit_shl_reg_imm(mod, EAX, extension);
            emit_sar_reg_imm(mod, EAX, extension);
            emit_cdq(mod);
        } else {
            emit_xor_reg_reg(mod, EDX, EDX);
        }
    } else if (field->bit_width == 32u) {
        if (field->type && !field->type->is_unsigned) {
            emit_cdq(mod);
        } else {
            emit_xor_reg_reg(mod, EDX, EDX);
        }
    } else if (field->bit_width < 64u) {
        unsigned high_width = field->bit_width - 32u;
        uint32_t mask = (UINT32_C(1) << high_width) - 1u;
        emit_mov_reg_imm(mod, ECX, mask);
        emit_and_reg_reg(mod, EDX, ECX);
        if (field->type && !field->type->is_unsigned) {
            uint8_t extension = (uint8_t)(32u - high_width);
            emit_shl_reg_imm(mod, EDX, extension);
            emit_sar_reg_imm(mod, EDX, extension);
        }
    }
    emit_pop_reg(mod, EBX);
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
    if (field->type && field->type->size == 8) {
        gen_expr_as_integer64(mod, initializer);
        emit_push_reg(mod, EDX);
        emit_push_reg(mod, EAX);
        emit_mov_reg_reg(mod, EAX, EBP);
        emit_add_reg_imm(mod, EAX, displacement);
        emit_mov_reg_reg(mod, ECX, EAX);
        emit_mov_reg_mem(mod, EAX, ESP, 0);
        emit_mov_reg_mem(mod, EDX, ESP, 4);
        emit_bitfield_store64_pair(mod, field);
        emit_add_reg_imm(mod, ESP, 8);
        return true;
    }
    gen_expr(mod, initializer);
    emit_mov_reg_reg(mod, ECX, EAX);
    emit_mov_reg_reg(mod, EDX, EBP);
    emit_add_reg_imm(mod, EDX, displacement);
    emit_bitfield_store32(mod, field);
    return true;
}

static void gen_tls_address(Module* mod, const char* symbol) {
    if (g_opts.dynamic_tls) {
        const char* index_symbol = module_get_tls_index_entry(mod, symbol);
        uint32_t call_offset;
        /* The argument is the address of the 16-byte RinTlsIndexV2 record,
         * not the record contents.  Keep this raw address materialization
         * independent of the ordinary PIC/GOT value helper. */
        emit_mov_reg_imm(mod, EAX, 0u);
        module_add_relocation(mod, MODULE_SYMBOL_CODE,
                              code_offset(mod) - 4u, 0u, false, false,
                              index_symbol);
        add_reloc(mod, MODULE_SYMBOL_CODE, code_offset(mod) - 4u,
                  RIN_RELOC_ABS32);
        emit_push_reg(mod, EAX);
        emit_byte(mod, 0xE8);
        call_offset = code_offset(mod);
        emit_dword(mod, 0u);
        add_func_call_ref("__tls_get_addr", call_offset);
        emit_add_reg_imm(mod, ESP, 4);
        return;
    }
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

static void gen_decl_storage_address32(Module* mod, Decl* declaration) {
    if (declaration && declaration->var_is_thread_local) {
        gen_tls_address(mod, decl_link_name(declaration));
    } else if (declaration) {
        gen_symbol_address(mod, decl_link_name(declaration), 0u);
    }
}

static void gen_reference_temporary_owner_address32(
    Module* mod, Decl* owner) {
    if (!owner) return;
    if (owner->var_is_global || owner->var_is_thread_local) {
        gen_decl_storage_address32(mod, owner);
        return;
    }
    emit_byte(mod, 0x8D); /* LEA EAX, [EBP+disp32] */
    emit_byte(mod, modrm(2, EAX, EBP));
    emit_dword(mod, (uint32_t)owner->var_offset);
}

static bool gen_inline_method_address(Module* mod, Expr* expr);
static bool gen_inline_method_integer64(Module* mod, Expr* expr);

static bool gen_aggregate_type32(const Type* type) {
    return type && (type->kind == TYPE_STRUCT ||
                    type->kind == TYPE_UNION ||
                    type->kind == TYPE_VECTOR ||
                    type_is_cxx_member_function_pointer(type));
}

static bool gen_expr_is_lvalue(Expr* expression) {
    if (!expression) return false;
    if (expression->kind == EXPR_CAST && expression->type &&
        expression->type->is_reference &&
        !expression->type->is_rvalue_reference &&
        (expression->cxx_cast_kind == CXX_CAST_NONE ||
         expression->cxx_cast_kind == CXX_CAST_STATIC ||
         expression->cxx_cast_kind == CXX_CAST_CONST ||
         expression->cxx_cast_kind == CXX_CAST_DYNAMIC)) {
        return gen_expr_is_lvalue(expression->cast_expr);
    }
    switch (expression->kind) {
        case EXPR_IDENT:
        case EXPR_DEREF:
        case EXPR_INDEX:
        case EXPR_PTR_MEMBER:
        case EXPR_CXX_TYPEID:
            return true;
        case EXPR_COMPOUND:
            return !rcc_parser_is_cxx_mode();
        case EXPR_STRING_LIT:
            return rcc_parser_is_cxx_mode();
        case EXPR_MEMBER:
            return !expression->cxx_member_xvalue;
        case EXPR_CXX_MEMBER_PTR_DOT:
            return !expression->cxx_member_xvalue;
        case EXPR_CXX_MEMBER_PTR_ARROW:
            return true;
        case EXPR_COND:
            return expression->cxx_conditional_lvalue;
        case EXPR_COMMA:
            return rcc_parser_is_cxx_mode() &&
                   gen_expr_is_lvalue(expression->binary_rhs);
        case EXPR_CALL:
            return expression->type && expression->type->is_reference &&
                   !expression->type->is_rvalue_reference;
        default:
            return false;
    }
}

static bool gen_expr_is_xvalue(Expr* expression) {
    if (expression && expression->kind == EXPR_COMMA) {
        return rcc_parser_is_cxx_mode() &&
               gen_expr_is_xvalue(expression->binary_rhs);
    }
    if (expression && expression->kind == EXPR_COND) {
        return expression->cxx_conditional_xvalue;
    }
    if (expression && expression->kind == EXPR_MEMBER) {
        return expression->cxx_member_xvalue;
    }
    if (expression && expression->kind == EXPR_CXX_MEMBER_PTR_DOT) {
        return expression->cxx_member_xvalue;
    }
    return expression && expression->type &&
           expression->type->is_reference &&
           expression->type->is_rvalue_reference &&
           (expression->kind == EXPR_CAST ||
            expression->kind == EXPR_CALL);
}

/* Return the prvalue whose storage is extended through a direct && cast.
 * Calls or names that already denote xvalues keep their own lifetime. */
static Expr* gen_reference_temporary_source(Expr* expression) {
    Type* cast_type;
    Expr* source;
    if (!expression || expression->kind != EXPR_CAST) return NULL;
    cast_type = expression->cast_type;
    source = expression->cast_expr;
    if (!cast_type || !cast_type->is_reference ||
        !cast_type->is_rvalue_reference || !source) {
        return NULL;
    }
    if (!gen_expr_is_lvalue(source) && !gen_expr_is_xvalue(source)) {
        return source;
    }
    return gen_reference_temporary_source(source);
}

static bool gen_reference_argument_needs_temporary(Type* passed_type,
                                                   Expr* argument) {
    if (!passed_type || !passed_type->is_reference || !argument ||
        gen_expr_is_lvalue(argument) || gen_expr_is_xvalue(argument)) {
        return false;
    }
    return passed_type->is_rvalue_reference ||
           (passed_type->base && passed_type->base->is_const);
}

static void gen_copy_aggregate32(Module* mod, int32_t destination_offset,
                                  int source_register, int size) {
    int offset = 0;
    emit_mov_reg_reg(mod, ECX, source_register);
    while (offset + 4 <= size) {
        emit_mov_reg_mem(mod, EAX, ECX, offset);
        emit_mov_mem_reg(mod, EBP, destination_offset + offset, EAX);
        offset += 4;
    }
    while (offset < size) {
        emit_load_typed32(mod, EAX, ECX, offset, type_uchar);
        emit_mov_mem_reg8(mod, EBP, destination_offset + offset, EAX);
        ++offset;
    }
}

static void gen_copy_aggregate_to_address32(Module* mod, int destination_register,
                                             int source_register, int size) {
    int offset = 0;
    while (offset + 4 <= size) {
        emit_mov_reg_mem(mod, EAX, source_register, offset);
        emit_mov_mem_reg(mod, destination_register, offset, EAX);
        offset += 4;
    }
    while (offset < size) {
        emit_load_typed32(mod, EAX, source_register, offset, type_uchar);
        emit_mov_mem_reg8(mod, destination_register, offset, EAX);
        ++offset;
    }
}

static bool gen_cxx_indirect_parameter32(const Type* type) {
    return type && type->cxx_class && type->cxx_nontrivial &&
           !type->is_reference &&
           (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION);
}

/* Generate lvalue address in EAX */
static void gen_lvalue(Module* mod, Expr* expr) {
    switch (expr->kind) {
        case EXPR_STRING_LIT:
            gen_expr(mod, expr);
            break;
        case EXPR_CXX_THIS:
            if (expr->cxx_this_stack_offset < 0) {
                rcc_fatal("constructor this argument has no saved object");
            }
            emit_mov_reg_mem(mod, EAX, ESP, expr->cxx_this_stack_offset);
            break;
        case EXPR_CXX_TYPEID:
            if (expr->cxx_typeid_dynamic) {
                gen_cxx_typeid32(mod, expr);
                break;
            }
            if (!expr->cxx_typeid_symbol) {
                rcc_error(expr->loc,
                          "typeid has no validated typeinfo identity");
                return;
            }
            codegen_emit_cxx_typeinfo_symbol(mod, expr->cxx_typeid_symbol);
            gen_symbol_address(mod, expr->cxx_typeid_symbol, 0u);
            break;
        case EXPR_IDENT: {
            /* Use decl set during semantic analysis */
            Decl* decl = expr->ident_decl;
            if (!decl) {
                rcc_error(expr->loc,
                          "identifier has no semantic declaration in i686 code generation");
                return;
            }
            if (decl->kind == DECL_VAR &&
                decl->var_reference_temporary_guard &&
                (decl->var_is_thread_local || decl->var_is_static_local) &&
                !gen_guarded_static_initializer32(mod, decl)) {
                rcc_error(expr->loc,
                          "cannot initialize guarded static object '%s'",
                          decl->name);
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
            } else if (decl->kind == DECL_PARAM &&
                       decl->param_is_cxx_indirect) {
                emit_mov_reg_mem(mod, EAX, EBP, decl->var_offset);
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

        case EXPR_CAST:
            if (expr->type && expr->type->is_reference &&
                (expr->cxx_cast_kind == CXX_CAST_NONE ||
                 expr->cxx_cast_kind == CXX_CAST_STATIC ||
                 expr->cxx_cast_kind == CXX_CAST_CONST ||
                 expr->cxx_cast_kind == CXX_CAST_DYNAMIC)) {
                gen_lvalue(mod, expr->cast_expr);
                if (expr->cxx_cast_kind == CXX_CAST_DYNAMIC &&
                    expr->cxx_dynamic_cast_runtime) {
                    gen_cxx_dynamic_cast_runtime32(mod, expr);
                } else if (expr->cxx_cast_kind == CXX_CAST_STATIC &&
                           expr->cxx_virtual_base_adjustment) {
                    int end_label;
                    if (!expr->cxx_virtual_base_source_class ||
                        expr->cxx_virtual_base_pointer_offset < 0 ||
                        expr->cxx_virtual_base_index < 0 ||
                        expr->cxx_virtual_base_index >=
                            expr->cxx_virtual_base_source_class
                                ->virtual_base_count) {
                        rcc_error(expr->loc,
                                  "virtual-base reference cast has incomplete vbtable metadata");
                        return;
                    }
                    end_label = new_label();
                    emit_test_reg_reg(mod, EAX, EAX);
                    emit_jcc_label(mod, CC_E, end_label);
                    emit_mov_reg_mem(
                        mod, EDX, EAX,
                        expr->cxx_virtual_base_pointer_offset);
                    emit_mov_reg_mem(
                        mod, ECX, EDX,
                        expr->cxx_virtual_base_index * 4);
                    emit_add_reg_reg(mod, EAX, ECX);
                    if (expr->cxx_virtual_base_nested_adjustment != 0) {
                        emit_add_reg_imm(
                            mod, EAX,
                            expr->cxx_virtual_base_nested_adjustment);
                    }
                    emit_label(mod, end_label);
                } else if ((expr->cxx_cast_kind == CXX_CAST_STATIC ||
                            expr->cxx_cast_kind == CXX_CAST_DYNAMIC) &&
                    expr->cxx_pointer_adjustment_valid &&
                    expr->cxx_pointer_adjustment != 0) {
                    emit_add_reg_imm(mod, EAX,
                                     expr->cxx_pointer_adjustment);
                }
            } else if (rcc_parser_is_cxx_mode() && expr->type &&
                       expr->type->kind == TYPE_STRUCT &&
                       expr->type->cxx_class && expr->cast_expr &&
                       expr->cast_expr->type &&
                       expr->cast_expr->type->kind == TYPE_STRUCT &&
                       expr->cast_expr->type->cxx_class &&
                       expr->cast_expr->type->cxx_class !=
                           expr->type->cxx_class) {
                gen_lvalue(mod, expr->cast_expr);
                gen_cxx_reference_adjustment32(mod, expr);
            } else {
                rcc_error(expr->loc, "not an lvalue");
            }
            break;

        case EXPR_INDEX:
            if (expr->index_base &&
                ((expr->index_base->type &&
                  expr->index_base->type->kind == TYPE_ARRAY) ||
                 (expr->index_base->kind == EXPR_IDENT &&
                  expr->index_base->ident_decl &&
                  expr->index_base->ident_decl->type &&
                  expr->index_base->ident_decl->type->kind == TYPE_ARRAY))) {
                gen_lvalue(mod, expr->index_base);
            } else {
                gen_expr(mod, expr->index_base);
            }
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
                /* The imm8 form silently wraps scales such as a 1024-byte
                 * row in a multidimensional array to zero.  Use the full
                 * register multiply so every complete C array element size
                 * is represented on i686 as well. */
                emit_scale_reg(mod, EAX, (uint32_t)expr->type->size);
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
            if (expr->cxx_virtual_base_member_access) {
                int end_label;
                if (!expr->cxx_virtual_base_source_class ||
                    expr->cxx_virtual_base_pointer_offset < 0 ||
                    expr->cxx_virtual_base_index < 0 ||
                    expr->cxx_virtual_base_index >=
                        expr->cxx_virtual_base_source_class
                            ->virtual_base_count) {
                    rcc_error(expr->loc,
                              "virtual-base member has incomplete vbtable metadata");
                    return;
                }
                end_label = new_label();
                emit_test_reg_reg(mod, EAX, EAX);
                emit_jcc_label(mod, CC_E, end_label);
                emit_mov_reg_mem(mod, EDX, EAX,
                                 expr->cxx_virtual_base_pointer_offset);
                emit_mov_reg_mem(mod, ECX, EDX,
                                 expr->cxx_virtual_base_index * 4);
                emit_add_reg_reg(mod, EAX, ECX);
                if (expr->cxx_virtual_base_nested_adjustment != 0) {
                    emit_add_reg_imm(
                        mod, EAX,
                        expr->cxx_virtual_base_nested_adjustment);
                }
                emit_label(mod, end_label);
            } else if (expr->member_field &&
                       expr->member_field->offset > 0) {
                emit_add_reg_imm(mod, EAX, expr->member_field->offset);
            }
            if (expr->member_field && expr->member_field->type &&
                expr->member_field->type->is_reference) {
                emit_mov_reg_mem(mod, EAX, EAX, 0);
            }
            break;

        case EXPR_CXX_MEMBER_PTR_DOT:
        case EXPR_CXX_MEMBER_PTR_ARROW:
            if (expr->kind == EXPR_CXX_MEMBER_PTR_ARROW) {
                gen_expr(mod, expr->binary_lhs);
            } else {
                gen_lvalue(mod, expr->binary_lhs);
            }
            if (expr->cxx_virtual_base_member_access) {
                if (!expr->cxx_virtual_base_source_class ||
                    expr->cxx_virtual_base_pointer_offset < 0 ||
                    expr->cxx_virtual_base_index < 0 ||
                    expr->cxx_virtual_base_index >=
                        expr->cxx_virtual_base_source_class
                            ->virtual_base_count) {
                    rcc_error(expr->loc,
                              "member-pointer virtual-base path has incomplete vbtable metadata");
                    return;
                }
                emit_mov_reg_mem(
                    mod, EDX, EAX,
                    expr->cxx_virtual_base_pointer_offset);
                emit_mov_reg_mem(
                    mod, ECX, EDX,
                    expr->cxx_virtual_base_index * 4);
                emit_add_reg_reg(mod, EAX, ECX);
                if (expr->cxx_virtual_base_nested_adjustment != 0) {
                    emit_add_reg_imm(
                        mod, EAX,
                        expr->cxx_virtual_base_nested_adjustment);
                }
            }
            if (expr->cxx_pointer_adjustment_valid &&
                expr->cxx_pointer_adjustment != 0) {
                emit_add_reg_imm(mod, EAX,
                                 expr->cxx_pointer_adjustment);
            }
            emit_push_reg(mod, EAX);
            gen_expr(mod, expr->binary_rhs);
            emit_pop_reg(mod, ECX);
            emit_add_reg_reg(mod, EAX, ECX);
            break;

        case EXPR_COMPOUND:
            if (!expr->compound_type || expr->compound_offset >= 0) {
                rcc_error(expr->loc,
                          "compound literal has no automatic storage slot");
                return;
            }
            if (expr->compound_type->kind == TYPE_ARRAY ||
                expr->compound_type->kind == TYPE_STRUCT ||
                expr->compound_type->kind == TYPE_UNION ||
                expr->compound_type->kind == TYPE_VECTOR) {
                gen_zero_local_storage(mod, expr->compound_offset,
                                       (size_t)expr->compound_type->size);
            }
            if (!gen_local_initializer(mod, expr->compound_type, expr,
                                       expr->compound_offset)) {
                rcc_error(expr->loc,
                          "unsupported compound literal initializer");
            }
            gen_local_vtable_init(mod, expr->compound_type, EBP,
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
            if (expr->type && expr->type->kind == TYPE_PTR &&
                expr->type->is_reference) {
                /* Ordinary reference-returning calls already return the
                 * referred object's address in the pointer ABI. */
                gen_expr(mod, expr);
                break;
            }
            if (!expr->type || !gen_aggregate_type32(expr->type) ||
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

        case EXPR_COND:
        case EXPR_COMMA: {
            Expr* then_expr = expr->kind == EXPR_COND
                ? expr->cond_then : expr->binary_rhs;
            Expr* else_expr = expr->kind == EXPR_COND
                ? expr->cond_else : NULL;
            int else_label = expr->kind == EXPR_COND ? new_label() : -1;
            int end_label = expr->kind == EXPR_COND ? new_label() : -1;
            bool glvalue_result =
                (expr->kind == EXPR_COND || expr->kind == EXPR_COMMA) &&
                (gen_expr_is_lvalue(expr) || gen_expr_is_xvalue(expr));
            bool aggregate_result = gen_aggregate_type32(expr->type);
            if ((!glvalue_result && !aggregate_result) ||
                (!glvalue_result && expr->aggregate_offset >= 0) ||
                !then_expr ||
                (expr->kind == EXPR_COND && !else_expr)) {
                rcc_error(expr->loc, "aggregate expression has no automatic result slot");
                return;
            }
            if (expr->kind == EXPR_COND) {
                gen_expr(mod, expr->cond_test);
                emit_test_scalar_value(mod, expr->cond_test->type);
                emit_jcc_label(mod, CC_E, else_label);
            } else {
                gen_expr(mod, expr->binary_lhs);
            }
            gen_lvalue(mod, then_expr);
            if (!glvalue_result) {
                gen_copy_aggregate32(mod, expr->aggregate_offset, EAX,
                                     expr->type->size);
            }
            if (expr->kind == EXPR_COND) {
                emit_jmp_label(mod, end_label);
                emit_label(mod, else_label);
                gen_lvalue(mod, else_expr);
                if (!glvalue_result) {
                    gen_copy_aggregate32(mod, expr->aggregate_offset, EAX,
                                         expr->type->size);
                }
                emit_label(mod, end_label);
            }
            if (!glvalue_result) {
                emit_byte(mod, 0x8D);  /* LEA EAX, [EBP+disp32] */
                emit_byte(mod, modrm(2, EAX, EBP));
                emit_dword(mod, (uint32_t)expr->aggregate_offset);
            }
            break;
        }

        case EXPR_ASSIGN:
            if (expr->type && gen_aggregate_type32(expr->type)) {
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

static void gen_divmod_integer64_internal(Module* mod, Expr* lhs, Expr* rhs,
                                           const Type* result_type,
                                           bool want_remainder,
                                           int rhs_stack_offset) {
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

    if (rhs_stack_offset >= 0) {
        emit_mov_reg_mem(mod, EAX, ESP,
                         DIVISION_STORAGE + 12 + rhs_stack_offset);
        emit_mov_reg_mem(mod, EDX, ESP,
                         DIVISION_STORAGE + 16 + rhs_stack_offset);
    } else {
        gen_expr_as_integer64(mod, rhs);
    }
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

static void gen_divmod_integer64(Module* mod, Expr* lhs, Expr* rhs,
                                 const Type* result_type,
                                 bool want_remainder) {
    gen_divmod_integer64_internal(mod, lhs, rhs, result_type, want_remainder,
                                  -1);
}

static void gen_divmod_integer64_from_stack(Module* mod, Expr* lhs,
                                            const Type* result_type,
                                            bool want_remainder,
                                            int rhs_stack_offset) {
    gen_divmod_integer64_internal(mod, lhs, NULL, result_type, want_remainder,
                                  rhs_stack_offset);
}

static Type* codegen_promoted_expression_type(const Expr* expression) {
    Type* type = expression ? expression->type : NULL;
    TypeField* field = NULL;
    Type* bitfield_promotion;
    if (expression &&
        (expression->kind == EXPR_MEMBER ||
         expression->kind == EXPR_PTR_MEMBER)) {
        field = expression->member_field;
    }
    bitfield_promotion = type_bitfield_integer_promotion(field);
    if (bitfield_promotion) return bitfield_promotion;
    if (type && type->kind == TYPE_ENUM) {
        if (type->enum_is_scoped) return type;
        type = type->enum_underlying_type
            ? type->enum_underlying_type : type_int;
    }
    if (type && type->kind < TYPE_INT) {
        return type_int;
    }
    return type;
}

static Type* codegen_comparison_type(Expr* expr) {
    Type* left = codegen_promoted_expression_type(
        expr ? expr->binary_lhs : NULL);
    Type* right = codegen_promoted_expression_type(
        expr ? expr->binary_rhs : NULL);
    if ((left && (left->kind == TYPE_PTR || left->kind == TYPE_ARRAY)) ||
        (right && (right->kind == TYPE_PTR || right->kind == TYPE_ARRAY))) {
        return type_ulong;
    }
    if (!left || !right) return type_int;
    return type_common(left, right);
}

static void emit_test_scalar_value(Module* mod, const Type* type) {
    if (type_is_complex(type)) {
        Type* component_type = type->complex_component_type;
        emit_push_reg(mod, EAX); /* complex object address */
        emit_mov_reg_mem(mod, EDX, ESP, 0);
        emit_load_floating_raw(mod, component_type, EDX, 0);
        emit_x87_floating_truth(mod, component_type);
        emit_push_reg(mod, EAX); /* real-component truth */
        emit_mov_reg_mem(mod, EDX, ESP, 4);
        emit_load_floating_raw(mod, component_type, EDX,
                               component_type->size);
        emit_x87_floating_truth(mod, component_type);
        emit_pop_reg(mod, ECX);
        emit_or_reg_reg(mod, EAX, ECX);
        emit_add_reg_imm(mod, ESP, 4);
        emit_test_reg_reg(mod, EAX, EAX);
        return;
    }
    if (gen_is_floating(type)) {
        emit_x87_floating_truth(mod, type);
        return;
    }
    if (gen_is_integer64(type)) emit_or_reg_reg(mod, EAX, EDX);
    emit_test_reg_reg(mod, EAX, EAX);
}

static void gen_multiply_integer64_internal(Module* mod, Expr* lhs, Expr* rhs,
                                            int rhs_stack_offset) {
    /* Low 64 bits of (ahi:alo) * (bhi:blo): alo*blo plus
     * the low words of both cross products.  A NULL lhs means that its
     * EDX:EAX value was already loaded by a compound assignment. */
    emit_push_reg(mod, EBX);
    emit_push_reg(mod, ESI);
    if (lhs) gen_expr_as_integer64(mod, lhs);
    emit_push_reg(mod, EDX);
    emit_push_reg(mod, EAX);
    if (rhs_stack_offset >= 0) {
        emit_mov_reg_mem(mod, EAX, ESP, rhs_stack_offset + 16);
        emit_mov_reg_mem(mod, EDX, ESP, rhs_stack_offset + 20);
    } else {
        gen_expr_as_integer64(mod, rhs);
    }
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

static void gen_multiply_integer64(Module* mod, Expr* lhs, Expr* rhs) {
    gen_multiply_integer64_internal(mod, lhs, rhs, -1);
}

static void gen_multiply_integer64_from_stack(Module* mod, Expr* lhs,
                                              int rhs_stack_offset) {
    gen_multiply_integer64_internal(mod, lhs, NULL, rhs_stack_offset);
}

static void gen_shift_integer64_internal(Module* mod, Expr* lhs, Expr* rhs,
                                          const Type* result_type,
                                          bool shift_left,
                                          int rhs_stack_offset) {
    int wide_count_label = new_label();
    int end_label = new_label();
    bool arithmetic = !shift_left && result_type &&
                      !result_type->is_unsigned;

    if (lhs) gen_expr_as_integer64(mod, lhs);
    emit_push_reg(mod, EDX);
    emit_push_reg(mod, EAX);
    if (rhs_stack_offset >= 0) {
        emit_mov_reg_mem(mod, EAX, ESP, rhs_stack_offset + 8);
    } else {
        gen_expr(mod, rhs);
    }
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

static void gen_shift_integer64(Module* mod, Expr* lhs, Expr* rhs,
                                const Type* result_type, bool shift_left) {
    gen_shift_integer64_internal(mod, lhs, rhs, result_type, shift_left, -1);
}

static void gen_shift_integer64_from_stack(Module* mod, Expr* lhs,
                                           const Type* result_type,
                                           bool shift_left,
                                           int rhs_stack_offset) {
    gen_shift_integer64_internal(mod, lhs, NULL, result_type, shift_left,
                                 rhs_stack_offset);
}

/* i386 SysV returns 64-bit integer scalars in EDX:EAX.  Keep this separate
 * from the ordinary EAX expression path so an unsupported operation cannot
 * silently truncate its high word. */
static void gen_expr64_pair(Module* mod, Expr* expr) {
    const Decl* caller = NULL;
    const Decl* callee = NULL;
    SourceLoc call_location = {NULL, 0, 0};
    const ExprList* arguments = NULL;
    uint32_t start;
    size_t parent_index_plus_one;
    size_t inline_call_index;
    uint32_t end;
    if (!g_opts.debug_info ||
        !rcc_optimize_inline_debug_info(expr, &caller, &callee,
                                        &call_location, &arguments)) {
        gen_expr64_pair_impl(mod, expr);
        if (expr && expr->type && expr->type->is_bit_precise &&
            expr->type->bit_width > 32u &&
            expr->type->bit_width < 64u) {
            unsigned high_width = expr->type->bit_width - 32u;
            uint32_t mask = (UINT32_C(1) << high_width) - UINT32_C(1);
            emit_and_reg_imm(mod, EDX, mask);
            if (!expr->type->is_unsigned) {
                uint8_t shift = (uint8_t)(32u - high_width);
                emit_shl_reg_imm(mod, EDX, shift);
                emit_sar_reg_imm(mod, EDX, shift);
            }
        }
        return;
    }
    start = code_offset(mod);
    parent_index_plus_one = mod->debug_inline_active_call_plus_one;
    inline_call_index = module_begin_debug_inline_call(
        mod, caller, callee, arguments, call_location, start,
        parent_index_plus_one);
    if (inline_call_index == SIZE_MAX) {
        rcc_fatal("DWARF inline-call index exceeds addressable range");
    }
    mod->debug_inline_active_call_plus_one = inline_call_index + 1u;
    gen_expr64_pair_impl(mod, expr);
    if (expr && expr->type && expr->type->is_bit_precise &&
        expr->type->bit_width > 32u &&
        expr->type->bit_width < 64u) {
        unsigned high_width = expr->type->bit_width - 32u;
        uint32_t mask = (UINT32_C(1) << high_width) - UINT32_C(1);
        emit_and_reg_imm(mod, EDX, mask);
        if (!expr->type->is_unsigned) {
            uint8_t shift = (uint8_t)(32u - high_width);
            emit_shl_reg_imm(mod, EDX, shift);
            emit_sar_reg_imm(mod, EDX, shift);
        }
    }
    end = code_offset(mod);
    mod->debug_inline_active_call_plus_one = parent_index_plus_one;
    module_end_debug_inline_call(mod, inline_call_index, end);
}

static void gen_expr64_pair_impl(Module* mod, Expr* expr) {
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
        case EXPR_CXX_MEMBER_PTR_DOT:
        case EXPR_CXX_MEMBER_PTR_ARROW:
            gen_lvalue(mod, expr);
            if (expr->member_field && expr->member_field->is_bitfield) {
                gen_bitfield_load64_pair(mod, expr->member_field);
            } else if (expr->type && expr->type->is_atomic) {
                emit_atomic_load_i686(mod, expr->type);
            } else {
                emit_mov_reg_reg(mod, ECX, EAX);
                emit_mov_reg_mem(mod, EAX, ECX, 0);
                emit_mov_reg_mem(mod, EDX, ECX, 4);
            }
            break;

        case EXPR_POS:
            gen_expr64_pair(mod, expr->unary_operand);
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
            if (expr->unary_operand && expr->unary_operand->member_field &&
                expr->unary_operand->member_field->is_bitfield) {
                const TypeField* field = expr->unary_operand->member_field;
                bool post = expr->kind == EXPR_POSTINC ||
                            expr->kind == EXPR_POSTDEC;
                bool increment = expr->kind == EXPR_PREINC ||
                                 expr->kind == EXPR_POSTINC;
                gen_lvalue(mod, expr->unary_operand);
                emit_push_reg(mod, EAX);
                gen_bitfield_load64_pair(mod, field);
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
                emit_mov_reg_mem(mod, ECX, ESP, post ? 8 : 0);
                emit_bitfield_store64_pair(mod, field);
                if (post) {
                    emit_push_reg(mod, EDX);
                    emit_push_reg(mod, EAX);
                    emit_mov_reg_mem(mod, EAX, ESP, 8);
                    emit_mov_reg_mem(mod, EDX, ESP, 12);
                    emit_add_reg_imm(mod, ESP, 20);
                } else {
                    emit_add_reg_imm(mod, ESP, 4);
                }
                break;
            }
            if (expr->unary_operand && expr->unary_operand->type &&
                expr->unary_operand->type->is_atomic) {
                if (gen_atomic_increment_assignment(mod, expr)) break;
                rcc_error(expr->loc,
                          "i686 atomic 64-bit ++/-- requires a supported RMW lowering");
                break;
            }
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
            if (expr->binary_lhs &&
                (expr->binary_lhs->kind == EXPR_MEMBER ||
                 expr->binary_lhs->kind == EXPR_PTR_MEMBER) &&
                expr->binary_lhs->member_field &&
                expr->binary_lhs->member_field->is_bitfield) {
                gen_expr_as_integer64(mod, expr->binary_rhs);
                emit_push_reg(mod, EDX);
                emit_push_reg(mod, EAX);
                gen_lvalue(mod, expr->binary_lhs);
                emit_mov_reg_reg(mod, ECX, EAX);
                emit_mov_reg_mem(mod, EAX, ESP, 0);
                emit_mov_reg_mem(mod, EDX, ESP, 4);
                emit_bitfield_store64_pair(
                    mod, expr->binary_lhs->member_field);
                emit_add_reg_imm(mod, ESP, 8);
                break;
            }
            gen_expr_as_integer64(mod, expr->binary_rhs);
            emit_push_reg(mod, EDX);
            emit_push_reg(mod, EAX);
            gen_lvalue(mod, expr->binary_lhs);
            if (expr->binary_lhs->type &&
                expr->binary_lhs->type->is_atomic) {
                emit_atomic_store64_i686(mod);
                emit_mov_reg_mem(mod, EAX, ESP, 0);
                emit_mov_reg_mem(mod, EDX, ESP, 4);
                emit_add_reg_imm(mod, ESP, 8);
            } else {
                emit_mov_reg_reg(mod, ECX, EAX);
                emit_pop_reg(mod, EAX);
                emit_pop_reg(mod, EDX);
                emit_mov_mem_reg(mod, ECX, 0, EAX);
                emit_mov_mem_reg(mod, ECX, 4, EDX);
            }
            break;

        case EXPR_ADD_ASSIGN:
        case EXPR_SUB_ASSIGN:
        case EXPR_AND_ASSIGN:
        case EXPR_OR_ASSIGN:
        case EXPR_XOR_ASSIGN:
            if (expr->binary_lhs &&
                (expr->binary_lhs->kind == EXPR_MEMBER ||
                 expr->binary_lhs->kind == EXPR_PTR_MEMBER) &&
                expr->binary_lhs->member_field &&
                expr->binary_lhs->member_field->is_bitfield) {
                const TypeField* field = expr->binary_lhs->member_field;
                gen_lvalue(mod, expr->binary_lhs);
                emit_push_reg(mod, EAX);
                gen_bitfield_load64_pair(mod, field);
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
                emit_mov_reg_mem(mod, ECX, ESP, 0);
                emit_bitfield_store64_pair(mod, field);
                emit_add_reg_imm(mod, ESP, 4);
                break;
            }
            if (expr->binary_lhs && expr->binary_lhs->type &&
                expr->binary_lhs->type->is_atomic) {
                if (gen_atomic_addsub_assignment(mod, expr)) break;
                if (gen_atomic_bitwise_assignment(mod, expr)) break;
                if (gen_atomic_arithmetic_assignment(mod, expr)) break;
                rcc_error(expr->loc,
                          "i686 atomic 64-bit compound assignment requires a supported RMW lowering");
                break;
            }
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
            if (expr->binary_lhs &&
                (expr->binary_lhs->kind == EXPR_MEMBER ||
                 expr->binary_lhs->kind == EXPR_PTR_MEMBER) &&
                expr->binary_lhs->member_field &&
                expr->binary_lhs->member_field->is_bitfield) {
                const TypeField* field = expr->binary_lhs->member_field;
                gen_lvalue(mod, expr->binary_lhs);
                emit_push_reg(mod, EAX);
                gen_bitfield_load64_pair(mod, field);
                if (expr->kind == EXPR_MUL_ASSIGN) {
                    gen_multiply_integer64(mod, NULL, expr->binary_rhs);
                } else if (expr->kind == EXPR_DIV_ASSIGN ||
                           expr->kind == EXPR_MOD_ASSIGN) {
                    gen_divmod_integer64(mod, NULL, expr->binary_rhs,
                                         expr->binary_lhs->type,
                                         expr->kind == EXPR_MOD_ASSIGN);
                } else {
                    gen_shift_integer64(mod, NULL, expr->binary_rhs,
                                        expr->binary_lhs->type,
                                        expr->kind == EXPR_LSHIFT_ASSIGN);
                }
                emit_mov_reg_mem(mod, ECX, ESP, 0);
                emit_bitfield_store64_pair(mod, field);
                emit_add_reg_imm(mod, ESP, 4);
                break;
            }
            if (expr->binary_lhs && expr->binary_lhs->type &&
                expr->binary_lhs->type->is_atomic) {
                if (gen_atomic_arithmetic_assignment(mod, expr)) break;
                rcc_error(expr->loc,
                          "i686 atomic 64-bit compound assignment requires a supported RMW lowering");
                break;
            }
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
            if (gen_aggregate_type32(expr->type)) {
                gen_lvalue(mod, expr);
                break;
            }
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
            if (gen_aggregate_type32(expr->type)) {
                gen_lvalue(mod, expr);
                break;
            }
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
                if (expr->va_arg_type->cxx_nontrivial) {
                    /* Non-trivial C++ ellipsis arguments are pointers to
                     * temporaries under the Itanium ABI. */
                    gen_lvalue(mod, expr->va_list_operand);
                    emit_mov_reg_reg(mod, ECX, EAX);
                    emit_mov_reg_mem(mod, EAX, ECX, 0);
                    emit_mov_reg_reg(mod, EDX, EAX);
                    emit_add_reg_imm(mod, EAX, 4);
                    emit_mov_mem_reg(mod, ECX, 0, EAX);
                    emit_mov_reg_mem(mod, EAX, EDX, 0);
                    break;
                }
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

static void gen_expr_as_scalar_field32(Module* mod, Expr* expr,
                                       Type* field_type) {
    if (field_type && field_type->is_reference) {
        if (gen_expr_is_lvalue(expr) || gen_expr_is_xvalue(expr)) {
            gen_lvalue(mod, expr);
        } else {
            gen_expr(mod, expr);
        }
        gen_cxx_reference_adjustment32(mod, expr);
    } else if (gen_is_integer64(field_type)) {
        gen_expr_as_integer64(mod, expr);
    } else {
        gen_expr_as_type(mod, expr, field_type);
    }
}

static Type* codegen_comparison_type(Expr* expr);

static void gen_spaceship_integer32(Module* mod, Expr* expr) {
    int less_label = new_label();
    int greater_label = new_label();
    int end_label = new_label();
    Type* comparison_type = codegen_comparison_type(expr);
    bool unsigned_compare = comparison_type && comparison_type->is_unsigned;

    gen_expr(mod, expr->binary_lhs);
    emit_push_reg(mod, EAX);
    gen_expr(mod, expr->binary_rhs);
    emit_mov_reg_reg(mod, ECX, EAX);
    emit_pop_reg(mod, EAX);
    emit_cmp_reg_reg(mod, EAX, ECX);
    emit_jcc_label(mod, unsigned_compare ? CC_B : CC_L, less_label);
    emit_jcc_label(mod, unsigned_compare ? CC_A : CC_G, greater_label);
    emit_mov_reg_imm(mod, EAX, 0u);
    emit_jmp_label(mod, end_label);
    emit_label(mod, less_label);
    emit_mov_reg_imm(mod, EAX, UINT32_MAX);
    emit_jmp_label(mod, end_label);
    emit_label(mod, greater_label);
    emit_mov_reg_imm(mod, EAX, 1u);
    emit_label(mod, end_label);
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

static void gen_spaceship_integer64(Module* mod, Expr* expr) {
    int less_label = new_label();
    int greater_label = new_label();
    int end_label = new_label();
    Type* comparison_type = codegen_comparison_type(expr);
    bool unsigned_compare = comparison_type && comparison_type->is_unsigned;

    gen_expr_as_integer64(mod, expr->binary_lhs);
    emit_push_reg(mod, EDX);
    emit_push_reg(mod, EAX);
    gen_expr_as_integer64(mod, expr->binary_rhs);
    emit_mov_reg_mem(mod, ECX, ESP, 4);
    emit_cmp_reg_reg(mod, ECX, EDX);
    emit_jcc_label(mod, unsigned_compare ? CC_B : CC_L, less_label);
    emit_jcc_label(mod, unsigned_compare ? CC_A : CC_G, greater_label);
    emit_mov_reg_mem(mod, ECX, ESP, 0);
    emit_cmp_reg_reg(mod, ECX, EAX);
    emit_jcc_label(mod, CC_B, less_label);
    emit_jcc_label(mod, CC_A, greater_label);
    emit_mov_reg_imm(mod, EAX, 0u);
    emit_jmp_label(mod, end_label);
    emit_label(mod, less_label);
    emit_mov_reg_imm(mod, EAX, UINT32_MAX);
    emit_jmp_label(mod, end_label);
    emit_label(mod, greater_label);
    emit_mov_reg_imm(mod, EAX, 1u);
    emit_label(mod, end_label);
    emit_add_reg_imm(mod, ESP, 8);
}

static Type* codegen_default_argument_type(Type* type) {
    if (!type) return type_int;
    if (type->kind == TYPE_ENUM) {
        if (rcc_parser_is_cxx_mode()) {
            if (type->enum_is_scoped) return type;
            if (type->enum_underlying_type) {
                Type* underlying = type->enum_underlying_type;
                return underlying->size < type_int->size
                    ? type_int : underlying;
            }
        }
        return type_int;
    }
    if (type->kind < TYPE_INT) return type_int;
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
    if (expr->cxx_temporary_cleanups) {
        emit_push_reg(mod, EDX);
        emit_push_reg(mod, EAX);
        codegen_push_call_temporary_cleanup32(
            mod, NULL, expr->cxx_temporary_cleanups);
        emit_pop_reg(mod, EAX);
        emit_pop_reg(mod, EDX);
    }
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
    if (expr->cxx_temporary_cleanups) {
        emit_push_reg(mod, EAX);
        codegen_push_call_temporary_cleanup32(
            mod, NULL, expr->cxx_temporary_cleanups);
        emit_pop_reg(mod, EAX);
    }
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

static const char* gen_cxx_array_allocation_name(const Expr* expr) {
    const Decl* allocation = expr && expr->call_func &&
            expr->call_func->kind == EXPR_IDENT
        ? expr->call_func->ident_decl : NULL;
    return allocation && allocation->kind == DECL_FUNC &&
            allocation->link_name
        ? decl_link_name((Decl*)allocation) : "rin_malloc";
}

static const char* gen_cxx_array_deallocation_name(const Expr* expr) {
    return expr && expr->call_new_deallocation_function &&
            expr->call_new_deallocation_function->link_name
        ? decl_link_name(expr->call_new_deallocation_function) : "rin_free";
}

static const char* gen_cxx_array_element_destructor_name(const Expr* expr) {
    Type* object_type = expr ? expr->call_new_type : NULL;
    Decl* destructor = object_type && object_type->cxx_class &&
            object_type->cxx_class->destructor_method
        ? object_type->cxx_class->destructor_method->decl : NULL;
    return destructor && destructor->link_name
        ? decl_link_name(destructor) : NULL;
}

static void gen_cxx_change_current_cleanup_pointer32(
    Module* mod, const char* callback_name, int object_reg,
    bool unregister) {
    emit_push_reg(mod, EAX);
    emit_push_reg(mod, ECX);
    emit_push_reg(mod, EDX);
    emit_mov_reg_reg(mod, EAX, object_reg);
    emit_push_reg(mod, EAX);
    gen_symbol_address(mod, callback_name, 0u);
    emit_push_reg(mod, EAX);
    emit_byte(mod, 0xE8);
    {
        uint32_t call_offset = code_offset(mod);
        emit_dword(mod, 0u);
        add_func_call_ref(
            unregister
                ? "rin_cpp_exception_unregister_current_cleanup"
                : "rin_cpp_exception_register_current_cleanup",
            call_offset);
    }
    emit_add_reg_imm(mod, ESP, 8);
    emit_pop_reg(mod, EDX);
    emit_pop_reg(mod, ECX);
    emit_pop_reg(mod, EAX);
}

/* Allocate a non-trivial array with a target-width element-count cookie.
 * The returned pointer addresses the first element; the allocator pointer is
 * the cookie address and is therefore the value passed to rin_free. */
static size_t gen_cxx_array_placement_cleanup_argument_count(
    const CxxArrayPlacementCleanupThunk* thunk) {
    const TypeParam* parameter = thunk && thunk->deallocation_function &&
            thunk->deallocation_function->type &&
            thunk->deallocation_function->type->kind == TYPE_FUNC
        ? thunk->deallocation_function->type->params : NULL;
    size_t count = 0u;
    if (parameter) parameter = parameter->next;
    while (parameter) {
        ++count;
        parameter = parameter->next;
    }
    return count;
}

static void gen_cxx_alloc_array_cookie32(
    Module* mod, Expr* expr,
    const CxxArrayPlacementCleanupThunk* cleanup_thunk) {
    Type* object_type = expr ? expr->call_new_type : NULL;
    const ExprList* placement_argument = expr && expr->call_args
        ? expr->call_args->next : NULL;
    int cookie_size = gen_cxx_array_cookie_size();
    int allocated_label;
    if (!object_type || object_type->size <= 0 ||
        !expr || !expr->call_new_count) {
        rcc_fatal("validated C++ array allocation has incomplete metadata");
    }
    size_t cleanup_argument_count =
        gen_cxx_array_placement_cleanup_argument_count(cleanup_thunk);
    size_t cleanup_record_size = cleanup_thunk
        ? (cleanup_argument_count + 1u) * 4u : 0u;
    if (cleanup_thunk &&
        (cleanup_argument_count == 0u ||
         cleanup_argument_count > (size_t)(INT_MAX / 4) - 1u)) {
        rcc_fatal("C++ placement delete has invalid cleanup parameters");
    }
    if (cleanup_thunk) {
        size_t source_argument_count = 0u;
        for (const ExprList* argument = placement_argument; argument;
             argument = argument->next) {
            ++source_argument_count;
        }
        if (source_argument_count != cleanup_argument_count) {
            rcc_fatal("C++ placement delete does not match allocation arguments");
        }
        emit_sub_reg_imm(mod, ESP, (uint32_t)cleanup_record_size);
    }
    if (placement_argument) {
        const ExprList* argument;
        int placement_count = 0;
        int temporary_bytes;
        int index;
        for (argument = placement_argument; argument;
             argument = argument->next) {
            ++placement_count;
        }
        temporary_bytes = (placement_count + 1) * 4;
        gen_expr(mod, expr->call_new_count);
        emit_push_reg(mod, EAX); /* Preserve the evaluated element count. */
        for (argument = placement_argument; argument;
             argument = argument->next) {
            gen_expr(mod, argument->expr);
            emit_push_reg(mod, EAX);
        }
        emit_sub_reg_imm(mod, ESP, temporary_bytes);
        emit_mov_reg_mem(mod, EAX, ESP,
                         temporary_bytes + placement_count * 4);
        emit_scale_reg(mod, EAX, (uint32_t)object_type->size);
        emit_add_reg_imm(mod, EAX, cookie_size);
        emit_mov_mem_reg(mod, ESP, 0, EAX);
        argument = placement_argument;
        for (index = 0; index < placement_count; ++index,
             argument = argument->next) {
            emit_mov_reg_mem(
                mod, EAX, ESP,
                temporary_bytes + (placement_count - index - 1) * 4);
            emit_mov_mem_reg(mod, ESP, (index + 1) * 4, EAX);
        }
        emit_byte(mod, 0xE8);
        {
            uint32_t call_offset = code_offset(mod);
            emit_dword(mod, 0u);
            add_func_call_ref(
                gen_cxx_array_allocation_name(expr), call_offset);
        }
        allocated_label = new_label();
        emit_cmp_reg_imm(mod, EAX, 0);
        emit_jcc_label(mod, CC_E, allocated_label);
        emit_mov_reg_mem(mod, ECX, ESP,
                         temporary_bytes + placement_count * 4);
        emit_mov_mem_reg(mod, EAX, 0, ECX);
        emit_add_reg_imm(mod, EAX, cookie_size);
        emit_label(mod, allocated_label);
        if (cleanup_thunk) {
            size_t record_offset = (size_t)temporary_bytes * 2u;
            emit_mov_reg_reg(mod, EDX, EAX);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_sub_reg_imm(mod, ECX, (uint32_t)cookie_size);
            emit_mov_mem_reg(mod, ESP, (int)record_offset, ECX);
            for (index = 0, argument = placement_argument;
                 argument; ++index, argument = argument->next) {
                emit_mov_reg_mem(
                    mod, EAX, ESP,
                    temporary_bytes +
                        (placement_count - (int)index - 1) * 4);
                emit_mov_mem_reg(
                    mod, ESP,
                    (int)(record_offset + (index + 1u) * 4u), EAX);
            }
            emit_mov_reg_reg(mod, EAX, EDX);
        }
        emit_add_reg_imm(mod, ESP, temporary_bytes * 2);
        return;
    }
    if (cleanup_thunk) {
        rcc_fatal("C++ placement cleanup requires allocation arguments");
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
        add_func_call_ref(gen_cxx_array_allocation_name(expr), call_offset);
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
        gen_cxx_alloc_array_cookie32(mod, expr, NULL);
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
    int64_t element_count = 0;
    size_t initializer_count = 0u;
    int zero_loop;
    int zero_done;

    if (!element_type || element_type->size <= 0 ||
        !expr || !expr->call_new_count || !expr->call_args) {
        SourceLoc location;
        codegen_expr_loc(&location, expr);
        rcc_error(location, "array new initializer has invalid element storage");
        return;
    }
    /* Evaluate a constant element count once and retain the allocation across
     * each initializer expression.  The semantic pass proves that the
     * initializer list fits and that aggregate elements are trivially
     * copyable. */
    gen_expr(mod, expr->call_new_count);
    emit_scale_reg(mod, EAX, (uint32_t)element_type->size);
    emit_push_reg(mod, EAX);
    emit_byte(mod, 0xE8);
    {
        uint32_t call_offset = code_offset(mod);
        emit_dword(mod, 0u);
        add_func_call_ref(gen_cxx_array_allocation_name(expr), call_offset);
    }
    emit_add_reg_imm(mod, ESP, 4);
    emit_push_reg(mod, EAX);
    emit_mov_reg_mem(mod, ECX, ESP, 0);
    gen_cxx_change_current_cleanup_pointer32(
        mod, gen_cxx_array_deallocation_name(expr), ECX, false);
    for (argument = expr->call_new_args; argument;
         argument = argument->next, offset += element_type->size,
         ++initializer_count) {
        if (gen_aggregate_type32(element_type)) {
            gen_expr(mod, argument->expr);
            emit_push_reg(mod, EAX);
            emit_mov_reg_mem(mod, ECX, ESP, 4);
            emit_mov_reg_mem(mod, EDX, ESP, 0);
            gen_copy_aggregate_to_address32(
                mod, ECX, EDX, element_type->size);
            emit_pop_reg(mod, EDX);
            continue;
        }
        emit_mov_reg_mem(mod, ECX, ESP, 0);
        gen_expr_as_type(mod, argument->expr, element_type);
        if (type_is_integer(element_type) ||
            element_type->kind == TYPE_ENUM) {
            emit_convert_integer_value(mod, EAX, argument->expr->type,
                                       element_type);
        }
        emit_store_scalar32(mod, ECX, offset, element_type);
    }
    if (expr_eval_integer_constant(expr->call_new_count, &element_count) &&
        element_count > (int64_t)initializer_count) {
        emit_mov_reg_mem(mod, ECX, ESP, 0);
        emit_add_reg_imm(mod, ECX, (uint32_t)offset);
        emit_mov_reg_imm(mod, EDX,
                         (uint32_t)(element_count -
                                    (int64_t)initializer_count));
        zero_loop = new_label();
        zero_done = new_label();
        emit_cmp_reg_imm(mod, EDX, 0);
        emit_jcc_label(mod, CC_E, zero_done);
        emit_label(mod, zero_loop);
        emit_mov_reg_imm(mod, EAX, 0u);
        for (int byte_offset = 0; byte_offset + 4 <= element_type->size;
             byte_offset += 4) {
            emit_mov_mem_reg(mod, ECX, byte_offset, EAX);
        }
        for (int byte_offset = (element_type->size / 4) * 4;
             byte_offset < element_type->size; ++byte_offset) {
            emit_mov_mem_reg8(mod, ECX, byte_offset, EAX);
        }
        emit_add_reg_imm(mod, ECX, (uint32_t)element_type->size);
        emit_sub_reg_imm(mod, EDX, 1);
        emit_cmp_reg_imm(mod, EDX, 0);
        emit_jcc_label(mod, CC_NE, zero_loop);
        emit_label(mod, zero_done);
    }
    emit_mov_reg_mem(mod, ECX, ESP, 0);
    gen_cxx_change_current_cleanup_pointer32(
        mod, gen_cxx_array_deallocation_name(expr), ECX, true);
    emit_pop_reg(mod, EAX);
}

static void gen_cxx_zero_object32_size(Module* mod, int address_reg, int size) {
    emit_mov_reg_imm(mod, EAX, 0u);
    for (int offset = 0; offset + 4 <= size; offset += 4) {
        emit_mov_mem_reg(mod, address_reg, offset, EAX);
    }
    for (int offset = (size / 4) * 4; offset < size; ++offset) {
        emit_mov_mem_reg8(mod, address_reg, offset, EAX);
    }
}

static void gen_cxx_zero_object32(Module* mod, Type* object_type,
                                   int address_reg) {
    gen_cxx_zero_object32_size(mod, address_reg,
                                object_type ? object_type->size : 0);
}

static void gen_cxx_initialize_default_members32(Module* mod, Type* object_type) {
    if (!mod || !object_type) return;
    for (TypeField* field = object_type->fields; field; field = field->next) {
        if (!field->initializer) continue;
        if (gen_aggregate_type32(field->type)) {
            emit_push_reg(mod, ECX);
            gen_expr(mod, field->initializer);
            emit_push_reg(mod, EAX);
            emit_mov_reg_mem(mod, ECX, ESP, 4);
            emit_mov_reg_mem(mod, EDX, ESP, 0);
            if (field->offset > 0) {
                emit_add_reg_imm(mod, ECX, field->offset);
            }
            gen_copy_aggregate_to_address32(
                mod, ECX, EDX, field->type->size);
            emit_pop_reg(mod, EDX);
            continue;
        }
        emit_push_reg(mod, ECX);
        gen_expr_as_type(mod, field->initializer, field->type);
        emit_pop_reg(mod, ECX);
        if (type_is_integer(field->type) || field->type->kind == TYPE_ENUM) {
            emit_convert_integer_value(mod, EAX, field->initializer->type,
                                       field->type);
        }
        emit_store_scalar32(mod, ECX, field->offset, field->type);
    }
}

static void gen_cxx_init_default_member_array32(Module* mod, Expr* expr) {
    Type* object_type = expr ? expr->call_new_type : NULL;
    int loop;
    int done;
    if (!object_type || object_type->size <= 0 ||
        !expr || !expr->call_new_count) {
        SourceLoc location;
        codegen_expr_loc(&location, expr);
        rcc_error(location,
                  "array new default member initializer has invalid storage");
        return;
    }
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
    emit_push_reg(mod, EAX);              /* base */
    emit_mov_reg_mem(mod, ECX, ESP, 0);   /* current */
    emit_mov_reg_mem(mod, EDX, ESP, 4);   /* remaining count */
    loop = new_label();
    done = new_label();
    emit_cmp_reg_imm(mod, EDX, 0);
    emit_jcc_label(mod, CC_E, done);
    emit_label(mod, loop);
    if (expr->call_new_value_init) {
        gen_cxx_zero_object32(mod, object_type, ECX);
    }
    gen_cxx_initialize_default_members32(mod, object_type);
    emit_add_reg_imm(mod, ECX, (uint32_t)object_type->size);
    emit_sub_reg_imm(mod, EDX, 1);
    emit_cmp_reg_imm(mod, EDX, 0);
    emit_jcc_label(mod, CC_NE, loop);
    emit_label(mod, done);
    emit_mov_reg_mem(mod, EAX, ESP, 0);
    emit_add_reg_imm(mod, ESP, 8);
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

static int gen_cxx_constructor_base32(Type* object_type, const char* name,
                                      Type** base_type) {
    CxxClass* cls = object_type ? object_type->cxx_class : NULL;
    const char* name_tail = name;
    const char* separator;
    if (base_type) *base_type = NULL;
    if (!cls || !name) return -1;
    separator = strrchr(name, ':');
    if (separator && separator > name && separator[-1] == ':') {
        name_tail = separator + 1;
    }
    for (int index = 0; index < cls->base_count; ++index) {
        CxxClass* base = cls->bases[index].base;
        const char* base_tail = base ? base->name : NULL;
        separator = base_tail ? strrchr(base_tail, ':') : NULL;
        if (separator && separator > base_tail && separator[-1] == ':') {
            base_tail = separator + 1;
        }
        if ((cls->bases[index].base_name &&
             strcmp(cls->bases[index].base_name, name) == 0) ||
            (base && base->name && strcmp(base->name, name) == 0) ||
            (base_tail && strcmp(base_tail, name_tail) == 0)) {
            if (base_type) *base_type = base ? base->type : NULL;
            return cls->base_offsets && cls->base_offsets[index] >= 0
                ? cls->base_offsets[index] : -1;
        }
    }
    return -1;
}

static int gen_cxx_constructor_virtual_base32(Type* object_type,
                                              const char* name,
                                              Type** base_type) {
    CxxClass* cls = object_type ? object_type->cxx_class : NULL;
    const char* name_tail = name;
    const char* separator;
    if (base_type) *base_type = NULL;
    if (!cls || !name) return -1;
    separator = strrchr(name, ':');
    if (separator && separator > name && separator[-1] == ':') {
        name_tail = separator + 1;
    }
    for (int index = 0; index < cls->virtual_base_count; ++index) {
        CxxClass* base = cls->virtual_bases[index].base;
        const char* base_tail = base ? base->name : NULL;
        separator = base_tail ? strrchr(base_tail, ':') : NULL;
        if (separator && separator > base_tail && separator[-1] == ':') {
            base_tail = separator + 1;
        }
        if ((base && base->name && strcmp(base->name, name) == 0) ||
            (base_tail && strcmp(base_tail, name_tail) == 0)) {
            if (base_type) *base_type = base ? base->type : NULL;
            return cls->virtual_bases[index].offset;
        }
    }
    return -1;
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

/* A body-less constructor is lowered directly from its member-initializer
 * list.  Its initializer expressions still name the source constructor
 * parameters, but those parameters do not have a normal function frame in
 * this lowering path.  Bind those names to the actual constructor arguments
 * before emitting the nested initialization so heap and automatic objects
 * share the same value flow. */
typedef struct CxxConstructorBindingAllocation32 {
    void* allocation;
    struct CxxConstructorBindingAllocation32* next;
} CxxConstructorBindingAllocation32;

typedef struct CxxConstructorBindingArena32 {
    CxxConstructorBindingAllocation32* allocations;
} CxxConstructorBindingArena32;

static CxxConstructorBindingArena32* active_cxx_constructor_binding_arena32;

static void gen_cxx_constructor_binding_track32(void* allocation) {
    CxxConstructorBindingArena32* arena =
        active_cxx_constructor_binding_arena32;
    CxxConstructorBindingAllocation32* item;
    if (!allocation || !arena) return;
    item = rcc_alloc(sizeof(*item));
    item->allocation = allocation;
    item->next = arena->allocations;
    arena->allocations = item;
}

static void* gen_cxx_constructor_binding_alloc32(size_t size) {
    void* allocation = rcc_alloc(size);
    gen_cxx_constructor_binding_track32(allocation);
    return allocation;
}

static void gen_cxx_constructor_binding_arena_release32(
    CxxConstructorBindingArena32* arena) {
    CxxConstructorBindingAllocation32* item;
    if (!arena) return;
    item = arena->allocations;
    while (item) {
        CxxConstructorBindingAllocation32* next = item->next;
        rcc_free(item->allocation);
        rcc_free(item);
        item = next;
    }
    arena->allocations = NULL;
}

static Expr* gen_cxx_bind_constructor_argument32(
    CxxConstructorInfo* constructor, Expr* expression, ExprList* arguments) {
    TypeParam* parameter = constructor ? constructor->parameters : NULL;
    ExprList* argument = arguments;
    DeclList* parameter_decl = constructor && constructor->method &&
            constructor->method->decl
        ? constructor->method->decl->func_params : NULL;
    if (!expression) return NULL;
    if (expression->kind == EXPR_CXX_THIS) {
        Expr* this_expression = expr_cxx_this(expression->loc);
        this_expression->cxx_this_stack_offset = 0;
        this_expression->type = expression->type;
        return this_expression;
    }
    if (expression->kind != EXPR_IDENT) return expression;
    if (expression->ident_name &&
        strcmp(expression->ident_name, "this") == 0) {
        Expr* this_expression = expr_cxx_this(expression->loc);
        this_expression->cxx_this_stack_offset = 0;
        this_expression->type = expression->type;
        return this_expression;
    }
    while (parameter && argument) {
        if (parameter->name && expression->ident_name &&
            strcmp(parameter->name, expression->ident_name) == 0 &&
            (!expression->ident_decl ||
             (parameter_decl &&
              parameter_decl->decl == expression->ident_decl))) {
            return argument->expr;
        }
        parameter = parameter->next;
        argument = argument->next;
        if (parameter_decl) parameter_decl = parameter_decl->next;
    }
    return expression;
}

static Expr* gen_cxx_bind_constructor_expression32(
    CxxConstructorInfo* constructor, Expr* expression, ExprList* arguments);
static ExprList* gen_cxx_bind_constructor_expression_list32(
    CxxConstructorInfo* constructor, ExprList* expressions,
    ExprList* arguments);
static CxxCleanupPlan* gen_cxx_bind_constructor_cleanup_plan32(
    CxxConstructorInfo* constructor, CxxCleanupPlan* plan,
    ExprList* arguments);
static GenericAssociation* gen_cxx_bind_constructor_generic32(
    CxxConstructorInfo* constructor, GenericAssociation* associations,
    ExprList* arguments);
static CxxCompoundRequirement* gen_cxx_bind_constructor_compound_requirements32(
    CxxConstructorInfo* constructor, CxxCompoundRequirement* requirements,
    ExprList* arguments);
static void gen_cxx_bind_constructor_expression_metadata32(
    CxxConstructorInfo* constructor, Expr* copy, Expr* expression,
    ExprList* arguments);
static Expr* gen_cxx_find_bound_temporary_source32(
    Expr* original, Expr* bound, Expr* target);

static ExprList* gen_cxx_bind_constructor_expression_list32(
    CxxConstructorInfo* constructor, ExprList* expressions,
    ExprList* arguments) {
    ExprList* bound = NULL;
    ExprList** tail = &bound;
    for (ExprList* item = expressions; item; item = item->next) {
        ExprList* copy = gen_cxx_constructor_binding_alloc32(sizeof(*copy));
        *copy = *item;
        copy->expr = gen_cxx_bind_constructor_expression32(
            constructor, item->expr, arguments);
        copy->cxx_temporary_cleanups =
            gen_cxx_bind_constructor_cleanup_plan32(
                constructor, item->cxx_temporary_cleanups, arguments);
        copy->cxx_temporary_next = NULL;
        copy->next = NULL;
        *tail = copy;
        tail = &copy->next;
    }
    return bound;
}

static CxxCleanupPlan* gen_cxx_bind_constructor_cleanup_plan32(
    CxxConstructorInfo* constructor, CxxCleanupPlan* plan,
    ExprList* arguments) {
    CxxCleanupPlan* bound = NULL;
    CxxCleanupPlan** tail = &bound;
    for (CxxCleanupPlan* item = plan; item; item = item->next) {
        CxxCleanupPlan* copy = gen_cxx_constructor_binding_alloc32(sizeof(*copy));
        *copy = *item;
        copy->expression = gen_cxx_bind_constructor_expression32(
            constructor, item->expression, arguments);
        copy->body = gen_cxx_bind_constructor_cleanup_plan32(
            constructor, item->body, arguments);
        copy->alternate_body = gen_cxx_bind_constructor_cleanup_plan32(
            constructor, item->alternate_body, arguments);
        copy->next = NULL;
        *tail = copy;
        tail = &copy->next;
    }
    return bound;
}

static Expr* gen_cxx_find_bound_temporary_source32(
    Expr* original, Expr* bound, Expr* target) {
    if (!original || !bound || !target) return NULL;
    if (original == target) return bound;
    if (original->kind != bound->kind) return NULL;
    switch (original->kind) {
        case EXPR_ADDR:
        case EXPR_CAST:
            return gen_cxx_find_bound_temporary_source32(
                original->kind == EXPR_ADDR ? original->unary_operand
                                            : original->cast_expr,
                original->kind == EXPR_ADDR ? bound->unary_operand
                                            : bound->cast_expr,
                target);
        case EXPR_ADD: {
            Expr* match = gen_cxx_find_bound_temporary_source32(
                original->binary_lhs, bound->binary_lhs, target);
            return match ? match : gen_cxx_find_bound_temporary_source32(
                original->binary_rhs, bound->binary_rhs, target);
        }
        default:
            return NULL;
    }
}

static GenericAssociation* gen_cxx_bind_constructor_generic32(
    CxxConstructorInfo* constructor, GenericAssociation* associations,
    ExprList* arguments) {
    GenericAssociation* bound = NULL;
    GenericAssociation** tail = &bound;
    for (GenericAssociation* item = associations; item; item = item->next) {
        GenericAssociation* copy = gen_cxx_constructor_binding_alloc32(sizeof(*copy));
        *copy = *item;
        copy->expr = gen_cxx_bind_constructor_expression32(
            constructor, item->expr, arguments);
        copy->next = NULL;
        *tail = copy;
        tail = &copy->next;
    }
    return bound;
}

static CxxCompoundRequirement*
gen_cxx_bind_constructor_compound_requirements32(
    CxxConstructorInfo* constructor, CxxCompoundRequirement* requirements,
    ExprList* arguments) {
    CxxCompoundRequirement* bound = NULL;
    CxxCompoundRequirement** tail = &bound;
    for (CxxCompoundRequirement* item = requirements; item;
         item = item->next) {
        CxxCompoundRequirement* copy =
            gen_cxx_constructor_binding_alloc32(sizeof(*copy));
        *copy = *item;
        copy->expr = gen_cxx_bind_constructor_expression32(
            constructor, item->expr, arguments);
        copy->next = NULL;
        *tail = copy;
        tail = &copy->next;
    }
    return bound;
}

static void gen_cxx_bind_constructor_expression_metadata32(
    CxxConstructorInfo* constructor, Expr* copy, Expr* expression,
    ExprList* arguments) {
    copy->cxx_temporary_source = NULL;
    if (expression->cxx_temporary_source) {
        ExprList* original_argument;
        ExprList* bound_argument;
        if (expression->kind == EXPR_CALL) {
            original_argument = expression->call_args;
            bound_argument = copy->call_args;
            for (; original_argument && bound_argument;
                 original_argument = original_argument->next,
                 bound_argument = bound_argument->next) {
                copy->cxx_temporary_source =
                    gen_cxx_find_bound_temporary_source32(
                        original_argument->expr, bound_argument->expr,
                        expression->cxx_temporary_source);
                if (copy->cxx_temporary_source) break;
            }
        }
        if (!copy->cxx_temporary_source) {
            rcc_error(expression->loc,
                      "constructor initializer temporary source was not preserved while rebinding");
        }
    }
    copy->cxx_temporary_cleanups = gen_cxx_bind_constructor_cleanup_plan32(
        constructor, expression->cxx_temporary_cleanups, arguments);
    copy->cxx_move_assignment = NULL;
    if (expression->cxx_move_assignment) {
        CxxMoveAssignment* move =
            gen_cxx_constructor_binding_alloc32(sizeof(*move));
        *move = *expression->cxx_move_assignment;
        move->source = gen_cxx_bind_constructor_expression32(
            constructor, move->source, arguments);
        move->cleanup = gen_cxx_bind_constructor_expression32(
            constructor, move->cleanup, arguments);
        move->release = gen_cxx_bind_constructor_expression32(
            constructor, move->release, arguments);
        copy->cxx_move_assignment = move;
    }
    copy->cxx_close_call = NULL;
    if (expression->cxx_close_call) {
        CxxCloseCall* close =
            gen_cxx_constructor_binding_alloc32(sizeof(*close));
        *close = *expression->cxx_close_call;
        close->object = gen_cxx_bind_constructor_expression32(
            constructor, close->object, arguments);
        close->handle = gen_cxx_bind_constructor_expression32(
            constructor, close->handle, arguments);
        close->cleanup = gen_cxx_bind_constructor_expression32(
            constructor, close->cleanup, arguments);
        copy->cxx_close_call = close;
    }
    copy->cxx_lambda_captures = gen_cxx_bind_constructor_expression_list32(
        constructor, expression->cxx_lambda_captures, arguments);
    copy->cxx_pack_expansion_pattern =
        expression->cxx_pack_expansion_pattern == expression
            ? copy
            : gen_cxx_bind_constructor_expression32(
                  constructor, expression->cxx_pack_expansion_pattern,
                  arguments);
}

static Expr* gen_cxx_bind_constructor_expression32(
    CxxConstructorInfo* constructor, Expr* expression, ExprList* arguments) {
    Expr* copy;
    if (!expression) return NULL;
    if (expression->kind == EXPR_CXX_THIS) {
        return gen_cxx_bind_constructor_argument32(
            constructor, expression, arguments);
    }
    if (expression->kind == EXPR_IDENT) {
        Expr* bound = gen_cxx_bind_constructor_argument32(
            constructor, expression, arguments);
        if (bound != expression) return bound;
        if (!expression->cxx_temporary_source &&
            !expression->cxx_temporary_cleanups &&
            !expression->cxx_lambda_captures &&
            !expression->cxx_pack_expansion_pattern &&
            !expression->cxx_move_assignment &&
            !expression->cxx_close_call) {
            return expression;
        }
        copy = gen_cxx_constructor_binding_alloc32(sizeof(*copy));
        *copy = *expression;
        gen_cxx_bind_constructor_expression_metadata32(
            constructor, copy, expression, arguments);
        return copy;
    }
    if (
        expression->kind == EXPR_INT_LIT ||
        expression->kind == EXPR_CHAR_LIT ||
        expression->kind == EXPR_FLOAT_LIT ||
        expression->kind == EXPR_STRING_LIT) {
        return expression;
    }
    copy = gen_cxx_constructor_binding_alloc32(sizeof(*copy));
    *copy = *expression;
    switch (expression->kind) {
        case EXPR_POS:
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
        case EXPR_ADDR:
        case EXPR_DEREF:
        case EXPR_PREINC:
        case EXPR_PREDEC:
        case EXPR_POSTINC:
        case EXPR_POSTDEC:
        case EXPR_SIZEOF:
        case EXPR_ALIGNOF:
        case EXPR_NOEXCEPT:
            copy->unary_operand = gen_cxx_bind_constructor_expression32(
                constructor, expression->unary_operand, arguments);
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
        case EXPR_SPACESHIP:
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
        case EXPR_CXX_MEMBER_PTR_DOT:
        case EXPR_CXX_MEMBER_PTR_ARROW:
            copy->binary_lhs = gen_cxx_bind_constructor_expression32(
                constructor, expression->binary_lhs, arguments);
            copy->binary_rhs = gen_cxx_bind_constructor_expression32(
                constructor, expression->binary_rhs, arguments);
            break;
        case EXPR_COND:
            copy->cond_test = gen_cxx_bind_constructor_expression32(
                constructor, expression->cond_test, arguments);
            copy->cond_then = gen_cxx_bind_constructor_expression32(
                constructor, expression->cond_then, arguments);
            copy->cond_else = gen_cxx_bind_constructor_expression32(
                constructor, expression->cond_else, arguments);
            break;
        case EXPR_CAST:
            copy->cast_expr = gen_cxx_bind_constructor_expression32(
                constructor, expression->cast_expr, arguments);
            break;
        case EXPR_CALL:
            copy->call_func = gen_cxx_bind_constructor_expression32(
                constructor, expression->call_func, arguments);
            copy->call_args = gen_cxx_bind_constructor_expression_list32(
                constructor, expression->call_args, arguments);
            copy->call_virtual_object =
                gen_cxx_bind_constructor_expression32(
                    constructor, expression->call_virtual_object, arguments);
            copy->call_new_count = gen_cxx_bind_constructor_expression32(
                constructor, expression->call_new_count, arguments);
            copy->call_new_args = gen_cxx_bind_constructor_expression_list32(
                constructor, expression->call_new_args, arguments);
            break;
        case EXPR_INDEX:
            copy->index_base = gen_cxx_bind_constructor_expression32(
                constructor, expression->index_base, arguments);
            copy->index_expr = gen_cxx_bind_constructor_expression32(
                constructor, expression->index_expr, arguments);
            break;
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            copy->member_base = gen_cxx_bind_constructor_expression32(
                constructor, expression->member_base, arguments);
            if (constructor && constructor->method &&
                constructor->method->owner &&
                constructor->method->owner->type && copy->member_name) {
                for (TypeField* field =
                         constructor->method->owner->type->fields;
                     field; field = field->next) {
                    if (field->name &&
                        strcmp(field->name, copy->member_name) == 0) {
                        copy->member_field = field;
                        copy->type = field->type && field->type->is_reference
                            ? field->type->base : field->type;
                        break;
                    }
                }
            }
            break;
        case EXPR_COMPOUND:
            copy->compound_init =
                gen_cxx_bind_constructor_expression_list32(
                    constructor, expression->compound_init, arguments);
            break;
        case EXPR_CXX_TYPEID:
            copy->cxx_typeid_operand =
                gen_cxx_bind_constructor_expression32(
                    constructor, expression->cxx_typeid_operand, arguments);
            break;
        case EXPR_GENERIC:
            copy->generic_control = gen_cxx_bind_constructor_expression32(
                constructor, expression->generic_control, arguments);
            copy->generic_associations = gen_cxx_bind_constructor_generic32(
                constructor, expression->generic_associations, arguments);
            break;
        case EXPR_CXX_FOLD:
            copy->cxx_fold_init = gen_cxx_bind_constructor_expression32(
                constructor, expression->cxx_fold_init, arguments);
            copy->cxx_fold_pattern = gen_cxx_bind_constructor_expression32(
                constructor, expression->cxx_fold_pattern, arguments);
            break;
        case EXPR_CXX_REQUIRES:
            copy->cxx_requires_items =
                gen_cxx_bind_constructor_expression_list32(
                    constructor, expression->cxx_requires_items, arguments);
            copy->cxx_requires_nested =
                gen_cxx_bind_constructor_expression_list32(
                    constructor, expression->cxx_requires_nested, arguments);
            copy->cxx_requires_compound =
                gen_cxx_bind_constructor_compound_requirements32(
                    constructor, expression->cxx_requires_compound, arguments);
            break;
        case EXPR_VA_START:
        case EXPR_VA_END:
        case EXPR_VA_COPY:
        case EXPR_VA_ARG:
            copy->va_list_operand = gen_cxx_bind_constructor_expression32(
                constructor, expression->va_list_operand, arguments);
            copy->va_second_operand = gen_cxx_bind_constructor_expression32(
                constructor, expression->va_second_operand, arguments);
            break;
        case EXPR_STRING_LIT:
            break;
        default:
            rcc_error(expression->loc,
                      "constructor initializer expression kind %d cannot be rebound",
                      (int)expression->kind);
            return expression;
    }
    gen_cxx_bind_constructor_expression_metadata32(
        constructor, copy, expression, arguments);
    return copy;
}

static ExprList* gen_cxx_bind_constructor_arguments32(
    CxxConstructorInfo* constructor, ExprList* member_arguments,
    ExprList* arguments) {
    return gen_cxx_bind_constructor_expression_list32(
        constructor, member_arguments, arguments);
}

static void gen_cxx_initialize_object32_mode(
    Module* mod, Type* object_type, CxxConstructorInfo* constructor,
    ExprList* arguments, bool initialize_virtual_bases);
static void gen_cxx_initialize_object32_mode_impl(
    Module* mod, Type* object_type, CxxConstructorInfo* constructor,
    ExprList* arguments, bool initialize_virtual_bases);
static void gen_cxx_initialize_object32(Module* mod, Type* object_type,
                                         CxxConstructorInfo* constructor,
                                         ExprList* arguments);
static int active_cxx_exception_cleanup_frame_offset;

/* Keep each completed base/member alive in the active catch frame until the
 * complete constructor succeeds.  A later initializer or the constructor
 * body can throw; the runtime then destroys only the subobjects whose
 * constructors already returned. */
static void gen_cxx_exception_change_subobject32(
    Module* mod, Type* object_type, int address_reg, int frame_offset,
    bool unregister) {
    CxxClass* cls = object_type ? object_type->cxx_class : NULL;
    Decl* destructor = cls && cls->destructor_method
        ? cls->destructor_method->decl : NULL;
    if (!mod || !destructor || !decl_link_name(destructor)) {
        return;
    }
    (void)frame_offset;
    emit_push_reg(mod, EAX);
    emit_push_reg(mod, ECX);
    emit_push_reg(mod, EDX);
    emit_mov_reg_reg(mod, EAX, address_reg);
    emit_push_reg(mod, EAX); /* object */
    gen_symbol_address(mod, decl_link_name(destructor), 0u);
    emit_push_reg(mod, EAX); /* destructor */
    emit_byte(mod, 0xE8);
    {
        uint32_t call_offset = code_offset(mod);
        emit_dword(mod, 0u);
        add_func_call_ref(unregister
                              ? "rin_cpp_exception_unregister_current_cleanup"
                              : "rin_cpp_exception_register_current_cleanup",
                          call_offset);
    }
    emit_add_reg_imm(mod, ESP, 8);
    emit_pop_reg(mod, EDX);
    emit_pop_reg(mod, ECX);
    emit_pop_reg(mod, EAX);
}

static void gen_cxx_exception_change_at_offset32(
    Module* mod, Type* object_type, int object_reg, int offset,
    int frame_offset, bool unregister) {
    emit_push_reg(mod, EAX);
    emit_mov_reg_reg(mod, EAX, object_reg);
    if (offset != 0) emit_add_reg_imm(mod, EAX, (uint32_t)offset);
    gen_cxx_exception_change_subobject32(
        mod, object_type, EAX, frame_offset, unregister);
    emit_pop_reg(mod, EAX);
}

static void gen_cxx_exception_change_array32(
    Module* mod, Type* array_type, int object_reg, int offset,
    int frame_offset, bool unregister) {
    int loop_label;
    int done_label;
    Type* element_type = array_type ? array_type->base : NULL;
    if (!mod || !array_type || array_type->kind != TYPE_ARRAY ||
        !element_type || !element_type->cxx_class ||
        array_type->array_len <= 0 || element_type->size <= 0) {
        return;
    }
    loop_label = new_label();
    done_label = new_label();
    emit_push_reg(mod, object_reg);
    if (offset != 0) emit_add_reg_imm(mod, object_reg, (uint32_t)offset);
    emit_mov_reg_imm(mod, EDX, (uint32_t)array_type->array_len);
    emit_label(mod, loop_label);
    emit_cmp_reg_imm(mod, EDX, 0);
    emit_jcc_label(mod, CC_E, done_label);
    emit_push_reg(mod, EDX);
    gen_cxx_exception_change_subobject32(
        mod, element_type, object_reg, frame_offset, unregister);
    emit_pop_reg(mod, EDX);
    emit_add_reg_imm(mod, object_reg, (uint32_t)element_type->size);
    emit_dec_reg(mod, EDX);
    emit_jmp_label(mod, loop_label);
    emit_label(mod, done_label);
    emit_pop_reg(mod, object_reg);
}

static void gen_cxx_exception_change_direct_subobjects32(
    Module* mod, Type* object_type, int object_reg, int frame_offset,
    bool initialize_virtual_bases, bool unregister);

static void gen_cxx_exception_unregister_initializers32(
    Module* mod, Type* object_type, CxxConstructorInfo* constructor,
    bool initialize_virtual_bases, int frame_offset) {
    if (!mod || !object_type || !constructor) {
        return;
    }
    for (CxxConstructorInitializer* initializer = constructor->initializers;
         initializer; initializer = initializer->next) {
        Type* base_type = NULL;
        int base_offset;
        if (initializer->is_delegating_constructor) {
            gen_cxx_exception_change_direct_subobjects32(
                mod, object_type, ECX, frame_offset,
                initialize_virtual_bases, true);
            return;
        }
        if (initializer->is_base_initializer) {
            if (initializer->is_virtual_base_initializer) {
                if (!initialize_virtual_bases) continue;
                base_offset = gen_cxx_constructor_virtual_base32(
                    object_type, initializer->field, &base_type);
            } else if (initializer->has_inherited_base_layout) {
                base_type = initializer->inherited_base_type;
                base_offset = initializer->inherited_base_offset;
            } else {
                base_offset = gen_cxx_constructor_base32(
                    object_type, initializer->field, &base_type);
            }
            if (base_offset >= 0 && base_type) {
                gen_cxx_exception_change_at_offset32(
                    mod, base_type, ECX, base_offset,
                    frame_offset, true);
            }
            continue;
        }
        {
            TypeField* field = gen_cxx_constructor_field32(
                object_type, initializer->field);
            if (!field || !field->type) continue;
            if (field->type->kind == TYPE_ARRAY) {
                gen_cxx_exception_change_array32(
                    mod, field->type, ECX, field->offset,
                    frame_offset, true);
            } else if (field->type->cxx_class) {
                gen_cxx_exception_change_at_offset32(
                    mod, field->type, ECX, field->offset,
                    frame_offset, true);
            }
        }
    }
}

static void gen_cxx_exception_change_direct_subobjects32(
    Module* mod, Type* object_type, int object_reg, int frame_offset,
    bool initialize_virtual_bases, bool unregister) {
    CxxClass* cls = object_type ? object_type->cxx_class : NULL;
    if (!mod || !cls) return;
    if (initialize_virtual_bases) {
        for (int index = 0; index < cls->virtual_base_count; ++index) {
            CxxClass* base = cls->virtual_bases[index].base;
            if (base && base->type) {
                gen_cxx_exception_change_at_offset32(
                    mod, base->type, object_reg,
                    cls->virtual_bases[index].offset,
                    frame_offset, unregister);
            }
        }
    }
    for (int index = 0; index < cls->base_count; ++index) {
        CxxClass* base = cls->bases[index].base;
        if (!base || !base->type || cls->bases[index].is_virtual ||
            !cls->base_offsets || cls->base_offsets[index] < 0) {
            continue;
        }
        gen_cxx_exception_change_at_offset32(
            mod, base->type, object_reg, cls->base_offsets[index],
            frame_offset, unregister);
    }
    for (TypeParam* parameter = cls->fields; parameter;
         parameter = parameter->next) {
        TypeField* field;
        if (parameter->is_static || !parameter->type) continue;
        field = gen_cxx_constructor_field32(object_type, parameter->name);
        if (!field) continue;
        if (field->type && field->type->kind == TYPE_ARRAY) {
            gen_cxx_exception_change_array32(
                mod, field->type, object_reg, field->offset,
                frame_offset, unregister);
        } else if (field->type && field->type->cxx_class) {
            gen_cxx_exception_change_at_offset32(
                mod, field->type, object_reg, field->offset,
                frame_offset, unregister);
        }
    }
}

/* Execute a validated mem-initializer list before a non-empty constructor
 * body.  The ordinary constructor function owns this prologue, so direct
 * objects, new-expressions, and static initialization all share the same
 * order and argument evaluation. */
static const Type* gen_cxx_class_array_element32(const Type* type);
static bool gen_cxx_class_array_count32(const Type* array_type,
                                        const Type* element_type,
                                        uint32_t* count);
static bool gen_cxx_implicit_default_array_element_supported32(
    const Type* type);

static void gen_cxx_initialize_member_initializers32(
    Module* mod, Type* object_type, CxxConstructorInfo* constructor,
    ExprList* arguments, bool initialize_virtual_bases) {
    if (!mod || !object_type || !constructor) return;
    for (CxxConstructorInitializer* initializer = constructor->initializers;
         initializer; initializer = initializer->next) {
        Type* base_type = NULL;
        int base_offset;
        if (!initializer->is_base_initializer ||
            !initializer->is_virtual_base_initializer) {
            continue;
        }
        if (!initialize_virtual_bases) continue;
        base_offset = gen_cxx_constructor_virtual_base32(
            object_type, initializer->field, &base_type);
        if (base_offset < 0 || !base_type) {
            rcc_error((SourceLoc){"<constructor>", 0, 0},
                      "validated C++ virtual base initializer is incomplete");
            return;
        }
        emit_push_reg(mod, ECX);
        if (base_offset != 0) emit_add_reg_imm(mod, ECX, (uint32_t)base_offset);
        if (initializer->constructor) {
            gen_cxx_initialize_object32_mode(
                mod, base_type, initializer->constructor,
                gen_cxx_bind_constructor_arguments32(
                    constructor, initializer->arguments, arguments), false);
        } else if (initializer->arguments) {
            rcc_error((SourceLoc){"<constructor>", 0, 0},
                      "virtual base initializer has no matching constructor");
            emit_pop_reg(mod, ECX);
            return;
        } else {
            gen_cxx_zero_object32(mod, base_type, ECX);
        }
        emit_pop_reg(mod, ECX);
        gen_cxx_exception_change_at_offset32(
            mod, base_type, ECX, base_offset,
            active_cxx_exception_cleanup_frame_offset, false);
    }
    for (CxxConstructorInitializer* initializer = constructor->initializers;
         initializer; initializer = initializer->next) {
        if (initializer->is_delegating_constructor) {
            if (!initializer->constructor ||
                initializer->constructor == constructor || initializer->next) {
                rcc_error((SourceLoc){"<constructor>", 0, 0},
                          "validated C++ delegating constructor is incomplete");
                return;
            }
            gen_cxx_initialize_object32_mode(
                mod, object_type, initializer->constructor,
                gen_cxx_bind_constructor_arguments32(
                    constructor, initializer->arguments, arguments),
                initialize_virtual_bases);
            gen_cxx_exception_change_direct_subobjects32(
                mod, object_type, ECX,
                active_cxx_exception_cleanup_frame_offset,
                initialize_virtual_bases, false);
            continue;
        }
        if (initializer->is_base_initializer) {
            if (initializer->is_virtual_base_initializer) continue;
            Type* base_type = NULL;
            int base_offset;
            if (initializer->has_inherited_base_layout) {
                base_type = initializer->inherited_base_type;
                base_offset = initializer->inherited_base_offset;
            } else {
                base_offset = gen_cxx_constructor_base32(
                    object_type, initializer->field, &base_type);
            }
            if (base_offset < 0 || !base_type) {
                rcc_error((SourceLoc){"<constructor>", 0, 0},
                          "validated C++ base initializer is incomplete");
                return;
            }
            emit_push_reg(mod, ECX);
            if (base_offset != 0) {
                emit_add_reg_imm(mod, ECX, (uint32_t)base_offset);
            }
            if (initializer->constructor) {
                gen_cxx_initialize_object32_mode(
                    mod, base_type, initializer->constructor,
                    gen_cxx_bind_constructor_arguments32(
                        constructor, initializer->arguments, arguments), false);
            } else if (initializer->arguments) {
                rcc_error((SourceLoc){"<constructor>", 0, 0},
                          "base initializer has no matching constructor");
                emit_pop_reg(mod, ECX);
                return;
            } else {
                gen_cxx_zero_object32(mod, base_type, ECX);
            }
            emit_pop_reg(mod, ECX);
            gen_cxx_exception_change_at_offset32(
                mod, base_type, ECX, base_offset,
                active_cxx_exception_cleanup_frame_offset, false);
            continue;
        }
        TypeField* field = gen_cxx_constructor_field32(
            object_type, initializer->field);
        const Type* array_element = field && field->type
            ? gen_cxx_class_array_element32(field->type) : NULL;
        bool class_array_member = array_element != NULL;
        bool implicit_class_array_default = class_array_member &&
            !initializer->constructor &&
            !initializer->is_default_member_initializer &&
            !initializer->value && !initializer->arguments &&
            gen_cxx_implicit_default_array_element_supported32(
                array_element);
        if (!field || (!initializer->value && !initializer->arguments &&
                       !(((field->type && field->type->cxx_class) ||
                          class_array_member) &&
                         (initializer->constructor ||
                          implicit_class_array_default)))) {
            rcc_error((SourceLoc){"<constructor>", 0, 0},
                      "validated C++ member initializer is incomplete");
            return;
        }
        if (class_array_member) {
            Type* element_type = (Type*)array_element;
            uint32_t element_count;
            int loop_label;
            int done_label;
            if (!gen_cxx_class_array_count32(
                    field->type, element_type, &element_count) ||
                (!initializer->constructor &&
                 !implicit_class_array_default) ||
                (initializer->constructor &&
                 (initializer->value || initializer->arguments))) {
                rcc_error((SourceLoc){"<constructor>", 0, 0},
                          "validated C++ class-array member initializer is incomplete");
                return;
            }
            loop_label = new_label();
            done_label = new_label();
            emit_push_reg(mod, ECX);
            if (field->offset != 0) {
                emit_add_reg_imm(mod, ECX, (uint32_t)field->offset);
            }
            emit_mov_reg_imm(mod, EDX, element_count);
            emit_label(mod, loop_label);
            emit_cmp_reg_imm(mod, EDX, 0);
            emit_jcc_label(mod, CC_E, done_label);
            emit_push_reg(mod, EDX);
            emit_push_reg(mod, ECX);
            if (initializer->constructor) {
                gen_cxx_initialize_object32_mode(
                    mod, element_type, initializer->constructor, NULL, true);
            }
            emit_pop_reg(mod, ECX);
            emit_push_reg(mod, ECX);
            gen_cxx_exception_change_subobject32(
                mod, element_type, ECX,
                active_cxx_exception_cleanup_frame_offset, false);
            emit_pop_reg(mod, ECX);
            emit_pop_reg(mod, EDX);
            emit_add_reg_imm(mod, ECX, (uint32_t)element_type->size);
            emit_dec_reg(mod, EDX);
            emit_jmp_label(mod, loop_label);
            emit_label(mod, done_label);
            emit_pop_reg(mod, ECX);
            continue;
        }
        if (field->type &&
            (field->type->cxx_class ||
             ((field->type->kind == TYPE_STRUCT ||
               field->type->kind == TYPE_UNION) &&
              ast_cxx_is_aggregate(field->type) &&
              rcc_sema_cxx_trivially_copyable(field->type)))) {
            if (!initializer->constructor) {
                Expr* copy_expression = NULL;
                if (ast_cxx_is_aggregate(field->type) &&
                    rcc_sema_cxx_trivially_copyable(field->type)) {
                    Type* value_type = initializer->value
                        ? (initializer->value->type
                               ? initializer->value->type
                               : initializer->value->compound_type)
                        : NULL;
                    if (initializer->value &&
                        initializer->value->kind == EXPR_COMPOUND &&
                        value_type &&
                        type_is_compatible(field->type,
                                           value_type)) {
                        copy_expression = initializer->value;
                    } else if (initializer->arguments &&
                               !initializer->arguments->next &&
                               initializer->arguments->expr &&
                               initializer->arguments->expr->type &&
                               type_is_compatible(
                                   field->type,
                                   initializer->arguments->expr->type)) {
                        copy_expression = initializer->arguments->expr;
                    }
                }
                if (copy_expression) {
                    Expr* source = gen_cxx_bind_constructor_expression32(
                        constructor, copy_expression, arguments);
                    emit_push_reg(mod, ECX);
                    gen_lvalue(mod, source);
                    emit_push_reg(mod, EAX);
                    emit_mov_reg_mem(mod, ECX, ESP, 4);
                    if (field->offset != 0) {
                        emit_add_reg_imm(mod, ECX,
                                         (uint32_t)field->offset);
                    }
                    emit_mov_reg_mem(mod, EDX, ESP, 0);
                    gen_copy_aggregate_to_address32(
                        mod, ECX, EDX, field->type->size);
                    emit_add_reg_imm(mod, ESP, 4);
                    emit_pop_reg(mod, ECX);
                    gen_cxx_exception_change_at_offset32(
                        mod, field->type, ECX, field->offset,
                        active_cxx_exception_cleanup_frame_offset, false);
                    continue;
                }
                rcc_error((SourceLoc){"<constructor>", 0, 0},
                          "validated C++ member constructor is missing for member '%s'",
                          field->name ? field->name : "<unnamed>");
                return;
            }
            emit_push_reg(mod, ECX);
            emit_add_reg_imm(mod, ECX, (uint32_t)field->offset);
            gen_cxx_initialize_object32_mode(
                mod, field->type, initializer->constructor,
                gen_cxx_bind_constructor_arguments32(
                    constructor, initializer->arguments, arguments), true);
            emit_pop_reg(mod, ECX);
            gen_cxx_exception_change_at_offset32(
                mod, field->type, ECX, field->offset,
                active_cxx_exception_cleanup_frame_offset, false);
            continue;
        }
        if (field->type && field->type->kind == TYPE_ARRAY &&
            field->type->base && !field->type->base->cxx_class) {
            Expr* value;
            if (!initializer->value || field->type->size <= 0) {
                rcc_error((SourceLoc){"<constructor>", 0, 0},
                          "validated C++ scalar-array initializer is incomplete");
                return;
            }
            value = gen_cxx_bind_constructor_expression32(
                constructor, initializer->value, arguments);
            emit_push_reg(mod, ECX);
            gen_expr(mod, value);
            emit_push_reg(mod, EAX);
            emit_mov_reg_mem(mod, ECX, ESP, 4);
            if (field->offset != 0) {
                emit_add_reg_imm(mod, ECX, (uint32_t)field->offset);
            }
            emit_mov_reg_mem(mod, EDX, ESP, 0);
            gen_copy_aggregate_to_address32(
                mod, ECX, EDX, field->type->size);
            emit_add_reg_imm(mod, ESP, 4);
            emit_pop_reg(mod, ECX);
            continue;
        }
        if (!initializer->value ||
            (initializer->arguments && initializer->arguments->next)) {
            rcc_error((SourceLoc){"<constructor>", 0, 0},
                      "scalar member initializer has unsupported arity");
            return;
        }
        emit_push_reg(mod, ECX);
        gen_expr_as_scalar_field32(
            mod, gen_cxx_bind_constructor_expression32(
                     constructor, initializer->value, arguments),
            field->type);
        emit_pop_reg(mod, ECX);
        if (type_is_integer(field->type) || field->type->kind == TYPE_ENUM) {
            emit_convert_integer_value(
                mod, EAX, initializer->value->type, field->type);
        }
        emit_store_scalar32(mod, ECX, field->offset, field->type);
    }
}

static void gen_cxx_initialize_inherited_members32(
    Module* mod, Type* object_type, CxxConstructorInfo* constructor) {
    if (!constructor || !constructor->is_inherited) return;
    if (constructor->inherited_selected_base_type &&
        constructor->inherited_selected_base_offset >= 0) {
        gen_cxx_exception_change_at_offset32(
            mod, constructor->inherited_selected_base_type, ECX,
            constructor->inherited_selected_base_offset,
            active_cxx_exception_cleanup_frame_offset, false);
    }
    if (constructor->inherited_member_initializers) {
        gen_cxx_initialize_member_initializers32(
            mod, object_type, constructor->inherited_member_initializers,
            NULL, false);
    }
    for (CxxConstructorInitializer* initializer =
             constructor->inherited_base_initializers;
         initializer; initializer = initializer->next) {
        gen_cxx_exception_change_at_offset32(
            mod, initializer->inherited_base_type, ECX,
            initializer->inherited_base_offset,
            active_cxx_exception_cleanup_frame_offset, true);
    }
    if (constructor->inherited_selected_base_type &&
        constructor->inherited_selected_base_offset >= 0) {
        gen_cxx_exception_change_at_offset32(
            mod, constructor->inherited_selected_base_type, ECX,
            constructor->inherited_selected_base_offset,
            active_cxx_exception_cleanup_frame_offset, true);
    }
}

static void gen_cxx_initialize_inherited_bases32(
    Module* mod, CxxConstructorInfo* constructor) {
    if (!constructor || !constructor->is_inherited) return;
    for (CxxConstructorInitializer* initializer =
             constructor->inherited_base_initializers;
         initializer; initializer = initializer->next) {
        Type* base_type = initializer->inherited_base_type;
        int base_offset = initializer->inherited_base_offset;
        if (!initializer->has_inherited_base_layout || base_offset < 0 ||
            !base_type) {
            rcc_error((SourceLoc){"<constructor>", 0, 0},
                      "validated inherited C++ base initializer is incomplete");
            return;
        }
        emit_push_reg(mod, ECX);
        if (base_offset != 0) {
            emit_add_reg_imm(mod, ECX, (uint32_t)base_offset);
        }
        if (initializer->constructor) {
            gen_cxx_initialize_object32_mode(
                mod, base_type, initializer->constructor, NULL, false);
        } else {
            gen_cxx_zero_object32(mod, base_type, ECX);
        }
        emit_pop_reg(mod, ECX);
        gen_cxx_exception_change_at_offset32(
            mod, base_type, ECX, base_offset,
            active_cxx_exception_cleanup_frame_offset, false);
    }
}

static void gen_cxx_initialize_object32_mode_impl(
    Module* mod, Type* object_type, CxxConstructorInfo* constructor,
    ExprList* arguments, bool initialize_virtual_bases) {
    TypeField* field;
    CxxConstructorInitializer* initializer;
    ExprList* argument;
    int address_reg = ECX;

    if (!constructor) {
        rcc_error((SourceLoc){"<constructor>", 0, 0},
                  "C++ constructor lowering metadata is missing");
        return;
    }
    if (!constructor->body_is_empty) {
        gen_cxx_initialize_inherited_bases32(
            mod, constructor);
        emit_push_reg(mod, ECX);
        gen_cxx_initialize_member_initializers32(
            mod, object_type, constructor, arguments,
            initialize_virtual_bases);
        if (constructor->is_inherited &&
            constructor->inherited_selected_base_offset > 0) {
            emit_add_reg_imm(mod, ECX,
                (uint32_t)constructor->inherited_selected_base_offset);
        }
        gen_cxx_call_constructor32(mod, constructor, arguments);
        emit_pop_reg(mod, ECX);
        gen_cxx_initialize_inherited_members32(
            mod, object_type, constructor);
        gen_cxx_exception_unregister_initializers32(
            mod, object_type, constructor, initialize_virtual_bases,
            active_cxx_exception_cleanup_frame_offset);
        return;
    }
    if (constructor->initializers &&
        constructor->initializers->is_delegating_constructor) {
        CxxConstructorInitializer* delegation = constructor->initializers;
        if (!delegation->constructor ||
            delegation->constructor == constructor || delegation->next) {
            rcc_error((SourceLoc){"<constructor>", 0, 0},
                      "validated C++ delegating constructor is incomplete");
            return;
        }
        gen_cxx_initialize_object32_mode(
            mod, object_type, delegation->constructor,
            gen_cxx_bind_constructor_arguments32(
                constructor, delegation->arguments, arguments),
            initialize_virtual_bases);
        gen_cxx_initialize_inherited_members32(
            mod, object_type, constructor);
        return;
    }
    if (constructor->lowerable_parameter_use_mask_valid) {
        unsigned index = 0u;
        for (argument = arguments; argument; argument = argument->next) {
            if (index < 32u &&
                (constructor->lowerable_parameter_use_mask &
                 (UINT32_C(1) << index)) == 0u) {
                emit_push_reg(mod, address_reg);
                gen_expr(mod, argument->expr);
                emit_pop_reg(mod, address_reg);
            }
            ++index;
        }
    }
    if (!initialize_virtual_bases && object_type && object_type->cxx_class &&
        object_type->cxx_class->virtual_base_count > 0 &&
        object_type->cxx_class->nonvirtual_size > 0) {
        gen_cxx_zero_object32_size(
            mod, address_reg,
            object_type->cxx_class->nonvirtual_size);
    } else {
        gen_cxx_zero_object32(mod, object_type, address_reg);
    }
    gen_cxx_initialize_inherited_bases32(mod, constructor);
    if (constructor->initializers) {
        for (initializer = constructor->initializers; initializer;
             initializer = initializer->next) {
            Type* base_type = NULL;
            int base_offset;
            if (!initializer->is_base_initializer ||
                !initializer->is_virtual_base_initializer) continue;
            if (!initialize_virtual_bases) continue;
            base_offset = gen_cxx_constructor_virtual_base32(
                object_type, initializer->field, &base_type);
            if (base_offset < 0 || !base_type) {
                rcc_error((SourceLoc){"<constructor>", 0, 0},
                          "validated C++ virtual base initializer is incomplete");
                return;
            }
            emit_push_reg(mod, address_reg);
            if (base_offset != 0) {
                emit_add_reg_imm(mod, address_reg, (uint32_t)base_offset);
            }
            if (initializer->constructor) {
                ExprList* bound_arguments =
                    gen_cxx_bind_constructor_arguments32(
                        constructor, initializer->arguments, arguments);
                gen_cxx_initialize_object32_mode(
                    mod, base_type, initializer->constructor,
                    bound_arguments, false);
            } else if (initializer->arguments) {
                rcc_error((SourceLoc){"<constructor>", 0, 0},
                          "virtual base initializer has no matching constructor");
                emit_pop_reg(mod, address_reg);
                return;
            } else {
                gen_cxx_zero_object32(mod, base_type, address_reg);
            }
            emit_pop_reg(mod, address_reg);
            gen_cxx_exception_change_at_offset32(
                mod, base_type, address_reg, base_offset,
                active_cxx_exception_cleanup_frame_offset, false);
        }
        for (initializer = constructor->initializers; initializer;
             initializer = initializer->next) {
            if (initializer->is_base_initializer) {
                if (initializer->is_virtual_base_initializer) continue;
                Type* base_type = NULL;
                int base_offset = gen_cxx_constructor_base32(
                    object_type, initializer->field, &base_type);
                if (base_offset < 0 || !base_type) {
                    rcc_error((SourceLoc){"<constructor>", 0, 0},
                              "validated C++ base initializer is incomplete");
                    return;
                }
                emit_push_reg(mod, address_reg);
                if (base_offset != 0) {
                    emit_add_reg_imm(mod, address_reg,
                                     (uint32_t)base_offset);
                }
                if (initializer->constructor) {
                    ExprList* bound_arguments =
                        gen_cxx_bind_constructor_arguments32(
                            constructor, initializer->arguments, arguments);
                    gen_cxx_initialize_object32_mode(
                        mod, base_type, initializer->constructor,
                        bound_arguments, false);
                } else if (initializer->arguments) {
                    rcc_error((SourceLoc){"<constructor>", 0, 0},
                              "base initializer has no matching constructor");
                    emit_pop_reg(mod, address_reg);
                    return;
                } else {
                    gen_cxx_zero_object32(mod, base_type, address_reg);
                }
                emit_pop_reg(mod, address_reg);
                gen_cxx_exception_change_at_offset32(
                    mod, base_type, address_reg, base_offset,
                    active_cxx_exception_cleanup_frame_offset, false);
                continue;
            }
            field = gen_cxx_constructor_field32(
                object_type, initializer->field);
            const Type* array_element = field && field->type
                ? gen_cxx_class_array_element32(field->type) : NULL;
            bool class_array_member = array_element != NULL;
            bool implicit_class_array_default = class_array_member &&
                !initializer->constructor &&
                !initializer->is_default_member_initializer &&
                !initializer->value && !initializer->arguments &&
                gen_cxx_implicit_default_array_element_supported32(
                    array_element);
            if (!field || (!initializer->value && !initializer->arguments &&
                           !(((field->type && field->type->cxx_class) ||
                              class_array_member) &&
                             (initializer->constructor ||
                              implicit_class_array_default)))) {
                rcc_error((SourceLoc){"<constructor>", 0, 0},
                          "validated C++ member initializer is incomplete");
                return;
            }
            if (class_array_member) {
                Type* element_type = (Type*)array_element;
                uint32_t element_count;
                int loop_label;
                int done_label;
                if (!gen_cxx_class_array_count32(
                        field->type, element_type, &element_count) ||
                    (!initializer->constructor &&
                     !implicit_class_array_default) ||
                    (initializer->constructor &&
                     (initializer->value || initializer->arguments)) ||
                    !element_type->cxx_class) {
                    rcc_error((SourceLoc){"<constructor>", 0, 0},
                              "validated C++ class-array member initializer is incomplete");
                    return;
                }
                loop_label = new_label();
                done_label = new_label();
                emit_push_reg(mod, address_reg);
                if (field->offset != 0) {
                    emit_add_reg_imm(mod, address_reg,
                                     (uint32_t)field->offset);
                }
                emit_mov_reg_imm(mod, EDX, element_count);
                emit_label(mod, loop_label);
                emit_cmp_reg_imm(mod, EDX, 0);
                emit_jcc_label(mod, CC_E, done_label);
                emit_push_reg(mod, EDX);
                emit_push_reg(mod, address_reg);
                if (initializer->constructor) {
                    gen_cxx_initialize_object32_mode(
                        mod, element_type, initializer->constructor,
                        NULL, true);
                }
                emit_pop_reg(mod, address_reg);
                emit_push_reg(mod, address_reg);
                gen_cxx_exception_change_subobject32(
                    mod, element_type, address_reg,
                    active_cxx_exception_cleanup_frame_offset, false);
                emit_pop_reg(mod, address_reg);
                emit_pop_reg(mod, EDX);
                emit_add_reg_imm(mod, address_reg,
                                 (uint32_t)element_type->size);
                emit_dec_reg(mod, EDX);
                emit_jmp_label(mod, loop_label);
                emit_label(mod, done_label);
                emit_pop_reg(mod, address_reg);
                continue;
            }
            if (field->type &&
                (field->type->cxx_class ||
                 ((field->type->kind == TYPE_STRUCT ||
                   field->type->kind == TYPE_UNION) &&
                  ast_cxx_is_aggregate(field->type) &&
                  rcc_sema_cxx_trivially_copyable(field->type)))) {
                if (!initializer->constructor) {
                    Expr* copy_expression = NULL;
                    if (ast_cxx_is_aggregate(field->type) &&
                        rcc_sema_cxx_trivially_copyable(field->type)) {
                        Type* value_type = initializer->value
                            ? (initializer->value->type
                                   ? initializer->value->type
                                   : initializer->value->compound_type)
                            : NULL;
                        if (initializer->value &&
                            initializer->value->kind == EXPR_COMPOUND &&
                            value_type &&
                            type_is_compatible(field->type, value_type)) {
                            copy_expression = initializer->value;
                        } else if (initializer->arguments &&
                                   !initializer->arguments->next &&
                                   initializer->arguments->expr &&
                                   initializer->arguments->expr->type &&
                                   type_is_compatible(
                                       field->type,
                                       initializer->arguments->expr->type)) {
                            copy_expression = initializer->arguments->expr;
                        }
                    }
                    if (copy_expression) {
                        Expr* source = gen_cxx_bind_constructor_expression32(
                            constructor, copy_expression, arguments);
                        emit_push_reg(mod, ECX);
                        gen_lvalue(mod, source);
                        emit_push_reg(mod, EAX);
                        emit_mov_reg_mem(mod, ECX, ESP, 4);
                        if (field->offset != 0) {
                            emit_add_reg_imm(mod, ECX,
                                             (uint32_t)field->offset);
                        }
                        emit_mov_reg_mem(mod, EDX, ESP, 0);
                        gen_copy_aggregate_to_address32(
                            mod, ECX, EDX, field->type->size);
                        emit_add_reg_imm(mod, ESP, 4);
                        emit_pop_reg(mod, ECX);
                        gen_cxx_exception_change_at_offset32(
                            mod, field->type, ECX, field->offset,
                            active_cxx_exception_cleanup_frame_offset,
                            false);
                        continue;
                    }
                    rcc_error((SourceLoc){"<constructor>", 0, 0},
                              "validated C++ member constructor is missing for member '%s'",
                              field->name ? field->name : "<unnamed>");
                    return;
                }
                emit_push_reg(mod, address_reg);
                emit_add_reg_imm(mod, address_reg,
                                 (uint32_t)field->offset);
                ExprList* bound_arguments =
                    gen_cxx_bind_constructor_arguments32(
                        constructor, initializer->arguments, arguments);
                gen_cxx_initialize_object32_mode(
                    mod, field->type, initializer->constructor,
                    bound_arguments, true);
                emit_pop_reg(mod, address_reg);
                gen_cxx_exception_change_at_offset32(
                    mod, field->type, address_reg, field->offset,
                    active_cxx_exception_cleanup_frame_offset, false);
            } else if (field->type &&
                       field->type->kind == TYPE_ARRAY &&
                       field->type->base &&
                       !field->type->base->cxx_class) {
                Expr* value = gen_cxx_bind_constructor_expression32(
                    constructor, initializer->value, arguments);
                if (!value || field->type->size <= 0) {
                    rcc_error((SourceLoc){"<constructor>", 0, 0},
                              "validated C++ scalar-array initializer is incomplete");
                    return;
                }
                emit_push_reg(mod, address_reg);
                gen_expr(mod, value);
                emit_push_reg(mod, EAX);
                emit_mov_reg_mem(mod, ECX, ESP, 4);
                if (field->offset != 0) {
                    emit_add_reg_imm(mod, ECX,
                                     (uint32_t)field->offset);
                }
                emit_mov_reg_mem(mod, EDX, ESP, 0);
                gen_copy_aggregate_to_address32(
                    mod, ECX, EDX, field->type->size);
                emit_add_reg_imm(mod, ESP, 4);
                emit_pop_reg(mod, address_reg);
            } else {
                Expr* value = gen_cxx_bind_constructor_expression32(
                    constructor, initializer->value, arguments);
                if (!value || (initializer->arguments &&
                               initializer->arguments->next)) {
                    rcc_error((SourceLoc){"<constructor>", 0, 0},
                              "scalar member initializer has unsupported arity");
                    return;
                }
                emit_push_reg(mod, address_reg);
                gen_expr_as_scalar_field32(mod, value, field->type);
                emit_pop_reg(mod, address_reg);
                if (type_is_integer(field->type) ||
                    field->type->kind == TYPE_ENUM) {
                    emit_convert_integer_value(mod, EAX, value->type,
                                               field->type);
                }
                emit_store_scalar32(mod, address_reg, field->offset,
                                    field->type);
            }
        }
        gen_cxx_initialize_inherited_members32(
            mod, object_type, constructor);
        gen_cxx_exception_unregister_initializers32(
            mod, object_type, constructor, initialize_virtual_bases,
            active_cxx_exception_cleanup_frame_offset);
        return;
    }
    if (constructor->parameter_count == 0) {
        for (initializer = constructor->initializers; initializer;
             initializer = initializer->next) {
            field = gen_cxx_constructor_field32(
                object_type, initializer->field);
            if (!field || !initializer->value) {
                rcc_fatal("validated C++ constructor field is missing");
            }
            emit_push_reg(mod, address_reg);
            gen_expr_as_scalar_field32(
                mod, initializer->value, field->type);
            emit_pop_reg(mod, address_reg);
            if (type_is_integer(field->type) ||
                field->type->kind == TYPE_ENUM) {
                emit_convert_integer_value(mod, EAX,
                                           initializer->value->type,
                                           field->type);
            }
            emit_store_scalar32(mod, address_reg, field->offset, field->type);
        }
        gen_cxx_initialize_inherited_members32(
            mod, object_type, constructor);
        gen_cxx_exception_unregister_initializers32(
            mod, object_type, constructor, initialize_virtual_bases,
            active_cxx_exception_cleanup_frame_offset);
        return;
    }
    field = object_type ? object_type->fields : NULL;
    for (argument = arguments; argument && field;
         argument = argument->next, field = field->next) {
        emit_push_reg(mod, address_reg);
        gen_expr_as_scalar_field32(mod, argument->expr, field->type);
        emit_pop_reg(mod, address_reg);
        if (type_is_integer(field->type) || field->type->kind == TYPE_ENUM) {
            emit_convert_integer_value(mod, EAX, argument->expr->type,
                                       field->type);
        }
        emit_store_scalar32(mod, address_reg, field->offset, field->type);
    }
    gen_cxx_initialize_inherited_members32(
        mod, object_type, constructor);
}

static void gen_cxx_initialize_object32_mode(
    Module* mod, Type* object_type, CxxConstructorInfo* constructor,
    ExprList* arguments, bool initialize_virtual_bases) {
    CxxConstructorBindingArena32 arena = {0};
    CxxConstructorBindingArena32* previous_arena =
        active_cxx_constructor_binding_arena32;
    active_cxx_constructor_binding_arena32 = &arena;
    gen_cxx_initialize_object32_mode_impl(
        mod, object_type, constructor, arguments, initialize_virtual_bases);
    active_cxx_constructor_binding_arena32 = previous_arena;
    gen_cxx_constructor_binding_arena_release32(&arena);
}

static void gen_cxx_initialize_object32(Module* mod, Type* object_type,
                                         CxxConstructorInfo* constructor,
                                         ExprList* arguments) {
    gen_cxx_initialize_object32_mode(
        mod, object_type, constructor, arguments, true);
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
    size_t initialized_count = 0u;
    CxxArrayPlacementCleanupThunk* cleanup_thunk;
    size_t cleanup_record_size = 0u;
    const char* destructor_name =
        gen_cxx_array_element_destructor_name(expr);
    const char* deallocation_name = gen_cxx_array_deallocation_name(expr);
    bool register_deallocation = expr && expr->call_new_array_cookie &&
        !(expr->call_args && expr->call_args->next);

    if (!object_type || !field || object_type->size <= 0 ||
        !expr || !expr->call_new_count || !expr->call_new_constructor) {
        SourceLoc location;
        codegen_expr_loc(&location, expr);
        rcc_error(location,
                  "array new constructor has invalid element storage");
        return;
    }
    element_size = object_type->size;
    cleanup_thunk = module_add_cxx_array_placement_cleanup_thunk(mod, expr);
    if (cleanup_thunk) {
        size_t cleanup_argument_count =
            gen_cxx_array_placement_cleanup_argument_count(cleanup_thunk);
        cleanup_record_size = (cleanup_argument_count + 1u) * 4u;
    }
    if (expr->call_new_array_cookie) {
        gen_cxx_alloc_array_cookie32(mod, expr, cleanup_thunk);
        if (register_deallocation) {
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_sub_reg_imm(mod, ECX,
                             (uint32_t)gen_cxx_array_cookie_size());
            gen_cxx_change_current_cleanup_pointer32(
                mod, deallocation_name, ECX, false);
        } else if (cleanup_thunk) {
            emit_mov_reg_reg(mod, ECX, ESP);
            gen_cxx_change_current_cleanup_pointer32(
                mod, cleanup_thunk->callback_name, ECX, false);
        }
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
        if (expr->call_new_array_cookie && destructor_name) {
            gen_cxx_change_current_cleanup_pointer32(
                mod, destructor_name, ECX, false);
        }
        emit_add_reg_imm(mod, ECX, (uint32_t)element_size);
        emit_mov_mem_reg(mod, ESP,
                         expr->call_new_array_cookie ? 0 : 4, ECX);
        ++initialized_count;
    }
    if (expr->call_new_array_cookie) {
        if (destructor_name) {
            emit_mov_reg_mem(mod, ECX, ESP, 0);
            while (initialized_count > 0u) {
                emit_sub_reg_imm(mod, ECX, (uint32_t)element_size);
                gen_cxx_change_current_cleanup_pointer32(
                    mod, destructor_name, ECX, true);
                --initialized_count;
            }
        }
        if (register_deallocation) {
            emit_mov_reg_mem(mod, ECX, ESP, 4);
            emit_sub_reg_imm(mod, ECX,
                             (uint32_t)gen_cxx_array_cookie_size());
            gen_cxx_change_current_cleanup_pointer32(
                mod, deallocation_name, ECX, true);
        } else if (cleanup_thunk) {
            emit_mov_reg_reg(mod, ECX, ESP);
            emit_add_reg_imm(mod, ECX, 8);
            gen_cxx_change_current_cleanup_pointer32(
                mod, cleanup_thunk->callback_name, ECX, true);
        }
    }
    emit_mov_reg_mem(mod, EAX, ESP,
                     expr->call_new_array_cookie ? 4 : 8);
    emit_add_reg_imm(mod, ESP,
                     expr->call_new_array_cookie ? 8 : 12);
    if (cleanup_thunk && expr->call_new_array_cookie) {
        emit_add_reg_imm(mod, ESP, (uint32_t)cleanup_record_size);
    }
}

static void gen_cxx_init_default_class_array32(Module* mod, Expr* expr) {
    Type* object_type = expr ? expr->call_new_type : NULL;
    const char* destructor_name =
        gen_cxx_array_element_destructor_name(expr);
    const char* deallocation_name = gen_cxx_array_deallocation_name(expr);
    CxxArrayPlacementCleanupThunk* cleanup_thunk;
    size_t cleanup_record_size = 0u;
    bool register_deallocation = expr && expr->call_new_array_cookie &&
        !(expr->call_args && expr->call_args->next);
    int loop;
    int done;
    int cleanup_loop;
    int cleanup_done;

    if (!object_type || object_type->size <= 0 ||
        !expr || !expr->call_new_count || !expr->call_new_constructor) {
        SourceLoc location;
        codegen_expr_loc(&location, expr);
        rcc_error(location,
                  "array new default constructor has invalid element storage");
        return;
    }
    if (expr->call_new_array_cookie) {
        cleanup_thunk = module_add_cxx_array_placement_cleanup_thunk(
            mod, expr);
        if (cleanup_thunk) {
            cleanup_record_size =
                (gen_cxx_array_placement_cleanup_argument_count(
                    cleanup_thunk) + 1u) * 4u;
        }
        gen_cxx_alloc_array_cookie32(mod, expr, cleanup_thunk);
        if (register_deallocation) {
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_sub_reg_imm(mod, ECX,
                             (uint32_t)gen_cxx_array_cookie_size());
            gen_cxx_change_current_cleanup_pointer32(
                mod, deallocation_name, ECX, false);
        } else if (cleanup_thunk) {
            emit_mov_reg_reg(mod, ECX, ESP);
            gen_cxx_change_current_cleanup_pointer32(
                mod, cleanup_thunk->callback_name, ECX, false);
        }
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
        if (destructor_name) {
            emit_mov_reg_mem(mod, ECX, ESP, 4);
            gen_cxx_change_current_cleanup_pointer32(
                mod, destructor_name, ECX, false);
        }
        emit_mov_reg_mem(mod, ECX, ESP, 4);
        emit_add_reg_imm(mod, ECX, (uint32_t)object_type->size);
        emit_mov_mem_reg(mod, ESP, 4, ECX);
        emit_mov_reg_mem(mod, EDX, ESP, 0);
        emit_sub_reg_imm(mod, EDX, 1);
        emit_mov_mem_reg(mod, ESP, 0, EDX);
        emit_cmp_reg_imm(mod, EDX, 0);
        emit_jcc_label(mod, CC_NE, loop);
        emit_label(mod, done);
        if (destructor_name) {
            emit_mov_reg_mem(mod, ECX, ESP, 8);
            emit_sub_reg_imm(
                mod, ECX, (uint32_t)gen_cxx_array_cookie_size());
            emit_mov_reg_mem(mod, EDX, ECX, 0);
            emit_mov_mem_reg(mod, ESP, 0, EDX);
            cleanup_loop = new_label();
            cleanup_done = new_label();
            emit_label(mod, cleanup_loop);
            emit_mov_reg_mem(mod, EDX, ESP, 0);
            emit_cmp_reg_imm(mod, EDX, 0);
            emit_jcc_label(mod, CC_E, cleanup_done);
            emit_mov_reg_mem(mod, ECX, ESP, 4);
            emit_sub_reg_imm(mod, ECX, (uint32_t)object_type->size);
            emit_mov_mem_reg(mod, ESP, 4, ECX);
            gen_cxx_change_current_cleanup_pointer32(
                mod, destructor_name, ECX, true);
            emit_mov_reg_mem(mod, EDX, ESP, 0);
            emit_sub_reg_imm(mod, EDX, 1);
            emit_mov_mem_reg(mod, ESP, 0, EDX);
            emit_jmp_label(mod, cleanup_loop);
            emit_label(mod, cleanup_done);
        }
        if (register_deallocation) {
            emit_mov_reg_mem(mod, ECX, ESP, 8);
            emit_sub_reg_imm(mod, ECX,
                             (uint32_t)gen_cxx_array_cookie_size());
            gen_cxx_change_current_cleanup_pointer32(
                mod, deallocation_name, ECX, true);
        } else if (cleanup_thunk) {
            emit_mov_reg_reg(mod, ECX, ESP);
            emit_add_reg_imm(mod, ECX, 12);
            gen_cxx_change_current_cleanup_pointer32(
                mod, cleanup_thunk->callback_name, ECX, true);
        }
        emit_mov_reg_mem(mod, EAX, ESP, 8);
        emit_add_reg_imm(mod, ESP, 12);
        if (cleanup_thunk) {
            emit_add_reg_imm(mod, ESP, (uint32_t)cleanup_record_size);
        }
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

static void gen_cxx_destroy_complete32(Module* mod, Type* object_type);

static const Type* gen_cxx_class_array_element32(const Type* type) {
    bool is_array = false;
    while (type && type->kind == TYPE_ARRAY) {
        if (!type->base || type->array_len <= 0 || type->size <= 0) {
            return NULL;
        }
        is_array = true;
        type = type->base;
    }
    return is_array && type && type->cxx_class ? type : NULL;
}

static bool gen_cxx_implicit_default_array_element_supported32(
    const Type* type) {
    const CxxClass* cls = type ? type->cxx_class : NULL;
    if (!cls || !type->is_complete || cls->constructors ||
        cls->base_count != 0 || cls->virtual_base_count != 0 ||
        cls->vtable_size != 0 || cls->has_field_initializer ||
        type->cleanup_function || type->cleanup_field) {
        return false;
    }
    if (cls->destructor_method) {
        const Decl* destructor = cls->destructor_method->decl;
        if (!destructor || !destructor->func_body ||
            !destructor->func_is_noexcept || destructor->func_params) {
            return false;
        }
    }
    for (const TypeParam* field = cls->fields; field; field = field->next) {
        if (field->is_static) continue;
        if (!field->type || field->initializer ||
            !type_is_scalar(field->type) || field->type->is_const ||
            field->type->is_reference || field->type->is_atomic) {
            return false;
        }
    }
    return true;
}

static bool gen_cxx_class_array_count32(const Type* array_type,
                                        const Type* element_type,
                                        uint32_t* count) {
    uint64_t element_count;
    if (!array_type || array_type->kind != TYPE_ARRAY ||
        !element_type || !element_type->cxx_class ||
        element_type->size <= 0 || array_type->size <= 0 ||
        (uint64_t)array_type->size > UINT32_MAX ||
        (uint64_t)element_type->size > (uint64_t)INT32_MAX ||
        array_type->size % element_type->size != 0) {
        return false;
    }
    element_count = (uint64_t)array_type->size /
        (uint64_t)element_type->size;
    if (element_count == 0u || element_count > UINT32_MAX) return false;
    if (count) *count = (uint32_t)element_count;
    return true;
}

static bool gen_cxx_destructor_cleanup_hook_supported32(const Type* type) {
    if (!type) return false;
    if (!type->cleanup_function && !type->cleanup_field) return true;
    if (!type->cleanup_function || !type->cleanup_function[0] ||
        !type->cleanup_field || !type->cxx_class ||
        !type->cxx_class->destructor_method) {
        return false;
    }
    for (const TypeField* field = type->fields; field; field = field->next) {
        if (field != type->cleanup_field) continue;
        return field->type && field->type->size > 0 &&
            field->type->size <= 8 &&
            (type_is_integer(field->type) ||
             field->type->kind == TYPE_ENUM ||
             field->type->kind == TYPE_PTR);
    }
    return false;
}

static bool gen_cxx_destructor_subobjects_supported32(
    const CxxClass* cls, unsigned depth) {
    const TypeParam* field;
    if (!cls || !cls->type || depth >= 64u ||
        cls->virtual_base_count != 0 ||
        !gen_cxx_destructor_cleanup_hook_supported32(cls->type)) {
        return false;
    }
    if (cls->destructor_method) {
        const Decl* destructor = cls->destructor_method->decl;
        if (!destructor || !destructor->func_body ||
            !destructor->func_is_cxx_destructor ||
            !destructor->func_is_noexcept || !destructor->func_this_param ||
            destructor->func_params) {
            return false;
        }
    }
    for (int index = 0; index < cls->base_count; ++index) {
        if (!cls->bases[index].base || cls->bases[index].is_virtual ||
            !cls->base_offsets || cls->base_offsets[index] < 0 ||
            !gen_cxx_destructor_subobjects_supported32(
                cls->bases[index].base, depth + 1u)) {
            return false;
        }
    }
    for (field = cls->fields; field; field = field->next) {
        const Type* type = field->type;
        if (field->is_static || !type) continue;
        if (!gen_cxx_destructor_cleanup_hook_supported32(type)) {
            return false;
        }
        if (type->kind == TYPE_ARRAY) {
            const Type* element_type = gen_cxx_class_array_element32(type);
            if (element_type) {
                uint32_t element_count;
                if (!gen_cxx_class_array_count32(
                        type, element_type, &element_count) ||
                    !gen_cxx_destructor_subobjects_supported32(
                        element_type->cxx_class, depth + 1u)) {
                    return false;
                }
            }
            continue;
        }
        if (type->cxx_class &&
            !gen_cxx_destructor_subobjects_supported32(
                type->cxx_class, depth + 1u)) {
            return false;
        }
    }
    return true;
}
/* A deleting destructor can transfer control through the active C++
 * exception frame before the delete expression reaches its member-destruction
 * code.  Register only the direct member subobjects here; the complete
 * object's destructor is already the operation in flight and must not be
 * registered a second time.  The runtime consumes these callbacks on an
 * exceptional transfer and the normal path unregisters them before the
 * compiler emits the ordinary reverse-order member walk. */
static void gen_cxx_register_delete_object32(Module* mod, Type* object_type,
                                             int frame_offset);

static void gen_cxx_register_delete_members32(Module* mod, Type* object_type,
                                              int frame_offset) {
    CxxClass* cls = object_type ? object_type->cxx_class : NULL;
    if (!cls) return;
    emit_push_reg(mod, EAX);
    for (TypeParam* parameter = cls->fields; parameter;
         parameter = parameter->next) {
        TypeField* field;
        if (parameter->is_static || !parameter->type ||
            !parameter->type->cxx_class) {
            continue;
        }
        field = gen_cxx_constructor_field32(object_type, parameter->name);
        if (!field) continue;
        emit_mov_reg_mem(mod, EAX, ESP, 0);
        if (field->offset) {
            emit_add_reg_imm(mod, EAX, (uint32_t)field->offset);
        }
        gen_cxx_register_delete_object32(mod, field->type, frame_offset);
    }
    emit_pop_reg(mod, EAX);
}

static void gen_cxx_register_delete_object32(Module* mod, Type* object_type,
                                             int frame_offset) {
    CxxClass* cls = object_type ? object_type->cxx_class : NULL;
    Decl* destructor = cls && cls->destructor_method
        ? cls->destructor_method->decl : NULL;
    if (!cls) return;
    gen_cxx_register_delete_members32(mod, object_type, frame_offset);
    if (!destructor || !destructor->func_body || !destructor->link_name) {
        return;
    }
    emit_push_reg(mod, EAX); /* preserve object across symbol lookup/call */
    emit_push_reg(mod, EAX); /* third cdecl argument */
    gen_symbol_address(mod, decl_link_name(destructor), 0u);
    emit_push_reg(mod, EAX);
    emit_mov_reg_reg(mod, EAX, EBP);
    emit_add_reg_imm(mod, EAX, frame_offset);
    emit_push_reg(mod, EAX);
    emit_byte(mod, 0xE8);
    {
        uint32_t call_offset = code_offset(mod);
        emit_dword(mod, 0u);
        add_func_call_ref("rin_cpp_exception_register_cleanup", call_offset);
    }
    emit_add_reg_imm(mod, ESP, 12);
    emit_pop_reg(mod, EAX);
}

static void gen_cxx_unregister_delete_object32(Module* mod, Type* object_type,
                                               int frame_offset);

static void gen_cxx_unregister_delete_members32(Module* mod, Type* object_type,
                                                int frame_offset) {
    CxxClass* cls = object_type ? object_type->cxx_class : NULL;
    if (!cls) return;
    emit_push_reg(mod, EAX);
    for (TypeParam* parameter = cls->fields; parameter;
         parameter = parameter->next) {
        TypeField* field;
        if (parameter->is_static || !parameter->type ||
            !parameter->type->cxx_class) {
            continue;
        }
        field = gen_cxx_constructor_field32(object_type, parameter->name);
        if (!field) continue;
        emit_mov_reg_mem(mod, EAX, ESP, 0);
        if (field->offset) {
            emit_add_reg_imm(mod, EAX, (uint32_t)field->offset);
        }
        gen_cxx_unregister_delete_object32(mod, field->type, frame_offset);
    }
    emit_pop_reg(mod, EAX);
}

static void gen_cxx_unregister_delete_object32(Module* mod, Type* object_type,
                                               int frame_offset) {
    CxxClass* cls = object_type ? object_type->cxx_class : NULL;
    Decl* destructor = cls && cls->destructor_method
        ? cls->destructor_method->decl : NULL;
    if (!cls) return;
    gen_cxx_unregister_delete_members32(mod, object_type, frame_offset);
    if (!destructor || !destructor->func_body || !destructor->link_name) {
        return;
    }
    emit_push_reg(mod, EAX); /* preserve object across symbol lookup/call */
    emit_push_reg(mod, EAX); /* third cdecl argument */
    gen_symbol_address(mod, decl_link_name(destructor), 0u);
    emit_push_reg(mod, EAX);
    emit_mov_reg_reg(mod, EAX, EBP);
    emit_add_reg_imm(mod, EAX, frame_offset);
    emit_push_reg(mod, EAX);
    emit_byte(mod, 0xE8);
    {
        uint32_t call_offset = code_offset(mod);
        emit_dword(mod, 0u);
        add_func_call_ref("rin_cpp_exception_unregister_cleanup", call_offset);
    }
    emit_add_reg_imm(mod, ESP, 12);
    emit_pop_reg(mod, EAX);
}

/* During delete[] the current element's destructor is already in flight when
 * it can transfer through the active exception frame.  Register the earlier
 * elements first, then the current element's direct members last, so the
 * runtime LIFO cleanup order is current-members, previous-element destructor
 * and members, ... .  The caller removes this registration set after a
 * normal destructor return before performing the ordinary member walk. */
static void gen_cxx_register_delete_array32(Module* mod, Type* object_type,
                                            Decl* destructor,
                                            int frame_offset) {
    int previous_done;
    int previous_loop;
    if (!object_type || !object_type->cxx_class) return;
    if (destructor) {
        previous_done = new_label();
        previous_loop = new_label();
        emit_mov_reg_mem(mod, ECX, ESP, 12); /* first element */
        emit_label(mod, previous_loop);
        emit_mov_reg_mem(mod, EDX, ESP, 0);
        emit_cmp_reg_reg(mod, ECX, EDX);
        emit_jcc_label(mod, CC_GE, previous_done);
        emit_mov_reg_reg(mod, EAX, ECX);
        emit_push_reg(mod, ECX);
        gen_cxx_register_delete_object32(mod, object_type, frame_offset);
        emit_pop_reg(mod, ECX);
        emit_add_reg_imm(mod, ECX, (uint32_t)object_type->size);
        emit_jmp_label(mod, previous_loop);
        emit_label(mod, previous_done);
    }
    emit_mov_reg_mem(mod, EAX, ESP, 0);
    gen_cxx_register_delete_members32(mod, object_type, frame_offset);
}

static void gen_cxx_unregister_delete_array32(Module* mod, Type* object_type,
                                              Decl* destructor,
                                              int frame_offset) {
    int previous_done;
    int previous_loop;
    if (!object_type || !object_type->cxx_class) return;
    emit_mov_reg_mem(mod, EAX, ESP, 0);
    gen_cxx_unregister_delete_members32(mod, object_type, frame_offset);
    if (!destructor) return;
    previous_done = new_label();
    previous_loop = new_label();
    emit_mov_reg_mem(mod, ECX, ESP, 12);
    emit_label(mod, previous_loop);
    emit_mov_reg_mem(mod, EDX, ESP, 0);
    emit_cmp_reg_reg(mod, ECX, EDX);
    emit_jcc_label(mod, CC_GE, previous_done);
    emit_mov_reg_reg(mod, EAX, ECX);
    emit_push_reg(mod, ECX);
    gen_cxx_unregister_delete_object32(mod, object_type, frame_offset);
    emit_pop_reg(mod, ECX);
    emit_add_reg_imm(mod, ECX, (uint32_t)object_type->size);
    emit_jmp_label(mod, previous_loop);
    emit_label(mod, previous_done);
}

static void gen_cxx_destroy_class_array32(Module* mod, Type* array_type) {
    const Type* element_type = gen_cxx_class_array_element32(array_type);
    uint32_t element_count;
    int loop;
    int done;
    if (!gen_cxx_class_array_count32(
            array_type, element_type, &element_count)) {
        rcc_fatal("validated C++ class-array destructor has invalid layout");
    }
    loop = new_label();
    done = new_label();
    emit_push_reg(mod, EAX); /* array base */
    emit_add_reg_imm(mod, EAX, (int32_t)(uint32_t)array_type->size);
    emit_push_reg(mod, EAX); /* one-past-end cursor */
    emit_label(mod, loop);
    emit_mov_reg_mem(mod, ECX, ESP, 0);
    emit_sub_reg_imm(mod, ECX, (int32_t)element_type->size);
    emit_mov_reg_mem(mod, EDX, ESP, 4);
    emit_cmp_reg_reg(mod, ECX, EDX);
    emit_jcc_label(mod, CC_B, done);
    emit_mov_mem_reg(mod, ESP, 0, ECX);
    emit_mov_reg_reg(mod, EAX, ECX);
    gen_cxx_destroy_complete32(mod, (Type*)element_type);
    emit_jmp_label(mod, loop);
    emit_label(mod, done);
    emit_add_reg_imm(mod, ESP, 8);
}

/* Destroy direct class members after the enclosing destructor body. */
static void gen_cxx_destroy_members32(Module* mod, Type* object_type) {
    CxxClass* cls = object_type ? object_type->cxx_class : NULL;
    TypeField** fields = NULL;
    int field_count = 0;
    int field_index = 0;
    if (!cls) return;
    for (TypeParam* parameter = cls->fields; parameter;
         parameter = parameter->next) {
        if (!parameter->is_static && parameter->type &&
            (parameter->type->cxx_class ||
             gen_cxx_class_array_element32(parameter->type))) {
            ++field_count;
        }
    }
    if (field_count) fields = rcc_alloc(
        sizeof(*fields) * (size_t)field_count);
    for (TypeParam* parameter = cls->fields; parameter;
         parameter = parameter->next) {
        if (!parameter->is_static && parameter->type &&
            (parameter->type->cxx_class ||
             gen_cxx_class_array_element32(parameter->type))) {
            TypeField* field = gen_cxx_constructor_field32(
                object_type, parameter->name);
            if (field) fields[field_index++] = field;
        }
    }
    emit_push_reg(mod, EAX);
    for (int index = field_index - 1; index >= 0; --index) {
        emit_mov_reg_mem(mod, ECX, ESP, 0);
        if (fields[index]->offset) {
            emit_add_reg_imm(mod, ECX, (uint32_t)fields[index]->offset);
        }
        emit_mov_reg_reg(mod, EAX, ECX);
        if (fields[index]->type->kind == TYPE_ARRAY) {
            gen_cxx_destroy_class_array32(mod, fields[index]->type);
        } else {
            gen_cxx_destroy_complete32(mod, fields[index]->type);
        }
    }
    emit_pop_reg(mod, EAX);
    if (fields) rcc_free(fields);
}

static void gen_cxx_destroy_bases32(Module* mod, Type* object_type) {
    CxxClass* cls = object_type ? object_type->cxx_class : NULL;
    if (!cls || cls->base_count <= 0 || !cls->base_offsets) return;
    emit_push_reg(mod, EAX);
    for (int index = cls->base_count - 1; index >= 0; --index) {
        CxxClass* base = cls->bases[index].base;
        if (!base || cls->bases[index].is_virtual ||
            cls->base_offsets[index] < 0) {
            continue;
        }
        emit_mov_reg_mem(mod, ECX, ESP, 0);
        if (cls->base_offsets[index]) {
            emit_add_reg_imm(mod, ECX,
                             (uint32_t)cls->base_offsets[index]);
        }
        emit_mov_reg_reg(mod, EAX, ECX);
        gen_cxx_destroy_complete32(mod, base->type);
    }
    emit_pop_reg(mod, EAX);
}

static void gen_cxx_destroy_subobjects32(Module* mod, Type* object_type) {
    gen_cxx_destroy_members32(mod, object_type);
    gen_cxx_destroy_bases32(mod, object_type);
}

static Decl* current_function_decl32 = NULL;

static void gen_cxx_destroy_current_function_subobjects32(Module* mod) {
    Decl* declaration = current_function_decl32;
    CxxClass* cls = declaration && declaration->func_method_owner
        ? declaration->func_method_owner->cxx_class : NULL;
    if (!declaration || !declaration->func_is_cxx_destructor || !cls ||
        !declaration->func_this_param ||
        !gen_cxx_destructor_subobjects_supported32(cls, 0u)) {
        return;
    }
    emit_mov_reg_mem(mod, EAX, EBP,
                     declaration->func_this_param->var_offset);
    gen_cxx_destroy_subobjects32(mod, cls->type);
}

static void gen_cxx_destroy_complete32(Module* mod, Type* object_type) {
    CxxClass* cls = object_type ? object_type->cxx_class : NULL;
    Decl* destructor = cls && cls->destructor_method
        ? cls->destructor_method->decl : NULL;
    if (!object_type || !cls) return;
    emit_push_reg(mod, EAX);
    if (destructor && destructor->func_body && destructor->link_name) {
        emit_mov_reg_mem(mod, ECX, ESP, 0);
        emit_push_reg(mod, ECX);
        emit_byte(mod, 0xE8);
        {
            uint32_t call_offset = code_offset(mod);
            emit_dword(mod, 0u);
            add_func_call_ref(decl_link_name(destructor), call_offset);
        }
        emit_add_reg_imm(mod, ESP, 4);
    } else if (object_type->cleanup_function &&
               object_type->cleanup_field) {
        int skip_cleanup = new_label();
        TypeField* field = object_type->cleanup_field;
        emit_mov_reg_mem(mod, ECX, ESP, 0);
        if (field->type && field->type->size == 8) {
            uint64_t invalid = (uint64_t)object_type->cleanup_invalid;
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
                add_func_call_ref(object_type->cleanup_function, call_offset);
            }
            emit_add_reg_imm(mod, ESP, 8);
        } else {
            emit_load_typed32(mod, EAX, ECX, field->offset, field->type);
            emit_cmp_reg_imm(mod, EAX,
                             (int32_t)object_type->cleanup_invalid);
            emit_jcc_label(mod, CC_E, skip_cleanup);
            emit_push_reg(mod, EAX);
            emit_byte(mod, 0xE8);
            {
                uint32_t call_offset = code_offset(mod);
                emit_dword(mod, 0u);
                add_func_call_ref(object_type->cleanup_function,
                                  call_offset);
            }
            emit_add_reg_imm(mod, ESP, 4);
        }
        emit_label(mod, skip_cleanup);
    }
    if (!destructor || !destructor->func_body ||
        !gen_cxx_destructor_subobjects_supported32(cls, 0u)) {
        emit_mov_reg_mem(mod, EAX, ESP, 0);
        gen_cxx_destroy_subobjects32(mod, object_type);
    }
    emit_pop_reg(mod, EAX);
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
    if (expr->call_new_is_array &&
        expr->call_new_default_member_initializers) {
        gen_cxx_init_default_member_array32(mod, expr);
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
        gen_cxx_alloc_array_cookie32(mod, expr, NULL);
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
                 expr->call_new_default_member_initializers ||
                 expr->call_new_constructor != NULL ||
                 argument != NULL;
    if (!initialize) return;

    emit_push_reg(mod, EAX); /* retain the allocation across initializers */
    emit_mov_reg_mem(mod, ECX, ESP, 0);
    if (expr->call_new_default_member_initializers) {
        if (expr->call_new_value_init) {
            gen_cxx_zero_object32(mod, object_type, ECX);
        }
        gen_cxx_initialize_default_members32(mod, object_type);
        emit_pop_reg(mod, EAX);
        return;
    }
    if (expr->call_new_constructor) {
        gen_cxx_initialize_object32(mod, object_type,
                                     expr->call_new_constructor,
                                     argument);
        emit_pop_reg(mod, EAX);
        return;
    }
    if (expr->call_new_copy_init) {
        if (!argument || argument->next ||
            (object_type->kind != TYPE_STRUCT &&
             object_type->kind != TYPE_UNION)) {
            rcc_fatal("validated C++ new copy initializer metadata is incomplete");
        }
        gen_lvalue(mod, argument->expr);
        emit_mov_reg_reg(mod, EDX, EAX);
        emit_mov_reg_mem(mod, ECX, ESP, 0);
        for (int offset = 0; offset + 4 <= object_type->size; offset += 4) {
            emit_mov_reg_mem(mod, EAX, EDX, offset);
            emit_mov_mem_reg(mod, ECX, offset, EAX);
        }
        for (int offset = (object_type->size / 4) * 4;
             offset < object_type->size; ++offset) {
            emit_load_typed32(mod, EAX, EDX, offset, type_uchar);
            emit_mov_mem_reg8(mod, ECX, offset, EAX);
        }
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
            emit_store_scalar32(mod, ECX, field->offset, field->type);
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
    const char* deallocation_name;
    int cookie_size;
    int loop;
    int free_label;
    int done;
    if (!expr || !expr->call_args || !expr->call_args->expr ||
        !expr->call_args->expr->type ||
        expr->call_args->expr->type->kind != TYPE_PTR) {
        rcc_fatal("validated C++ array delete has incomplete operand metadata");
    }
    object_type = expr->call_delete_object_type
        ? expr->call_delete_object_type : expr->call_args->expr->type->base;
    destructor = expr->call_delete_array_destructor;
    cleanup = expr->call_delete_array_cleanup;
    field = expr->call_delete_array_cleanup_field;
    deallocation_name = expr->call_delete_deallocation_function
        ? decl_link_name(expr->call_delete_deallocation_function)
        : "rin_free";
    cookie_size = gen_cxx_array_cookie_size();
    if (!object_type || object_type->size <= 0 ||
        (!destructor && (!cleanup || !field) &&
         !object_type->cxx_class)) {
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

    if (active_cxx_exception_cleanup_frame_offset != INT_MAX) {
        if (!destructor) {
            rcc_error(expr->loc,
                      "exception-aware delete[] requires an explicit element destructor");
        }
        gen_cxx_register_delete_array32(
            mod, object_type, destructor,
            active_cxx_exception_cleanup_frame_offset);
    }
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
        if (active_cxx_exception_cleanup_frame_offset != INT_MAX) {
            gen_cxx_unregister_delete_array32(
                mod, object_type, destructor,
                active_cxx_exception_cleanup_frame_offset);
        }
        gen_cxx_destroy_members32(mod, object_type);
    } else if (!cleanup || !field) {
        emit_mov_reg_mem(mod, EAX, ESP, 0);
        gen_cxx_destroy_complete32(mod, object_type);
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
        emit_mov_reg_mem(mod, EAX, ESP, 0);
        if (active_cxx_exception_cleanup_frame_offset != INT_MAX) {
            gen_cxx_unregister_delete_array32(
                mod, object_type, destructor,
                active_cxx_exception_cleanup_frame_offset);
        }
        gen_cxx_destroy_members32(mod, object_type);
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
        add_func_call_ref(deallocation_name, free_offset);
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
    const char* deallocation_name = expr &&
            expr->call_delete_deallocation_function
        ? decl_link_name(expr->call_delete_deallocation_function)
        : "rin_free";
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
        if (active_cxx_exception_cleanup_frame_offset != INT_MAX) {
            gen_cxx_register_delete_members32(
                mod, expr->call_delete_object_type,
                active_cxx_exception_cleanup_frame_offset);
        }
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
        if (active_cxx_exception_cleanup_frame_offset != INT_MAX) {
            gen_cxx_unregister_delete_members32(
                mod, expr->call_delete_object_type,
                active_cxx_exception_cleanup_frame_offset);
        }
        gen_cxx_destroy_members32(mod,
                                  expr->call_delete_object_type);
        emit_push_reg(mod, EAX);
        emit_byte(mod, 0xE8);
        {
            uint32_t free_offset = code_offset(mod);
            emit_dword(mod, 0);
            add_func_call_ref(deallocation_name, free_offset);
        }
        emit_add_reg_imm(mod, ESP, 4);
        emit_mov_reg_imm(mod, EAX, 0);
        emit_label(mod, done);
        return;
    }
    if (!cleanup || !field || !expr->call_args ||
        !expr->call_args->expr) {
        if (expr->call_delete_object_type && expr->call_args &&
            expr->call_args->expr) {
            done = new_label();
            gen_expr(mod, expr->call_args->expr);
            emit_cmp_reg_imm(mod, EAX, 0);
            emit_jcc_label(mod, CC_E, done);
            if (active_cxx_exception_cleanup_frame_offset != INT_MAX) {
                gen_cxx_register_delete_members32(
                    mod, expr->call_delete_object_type,
                    active_cxx_exception_cleanup_frame_offset);
            }
            emit_push_reg(mod, EAX);
            emit_mov_reg_mem(mod, EAX, ESP, 0);
            gen_cxx_destroy_complete32(mod, expr->call_delete_object_type);
            if (active_cxx_exception_cleanup_frame_offset != INT_MAX) {
                gen_cxx_unregister_delete_members32(
                    mod, expr->call_delete_object_type,
                    active_cxx_exception_cleanup_frame_offset);
            }
            emit_mov_reg_mem(mod, EAX, ESP, 0);
            emit_push_reg(mod, EAX);
            emit_byte(mod, 0xE8);
            {
                uint32_t free_offset = code_offset(mod);
                emit_dword(mod, 0u);
                add_func_call_ref(deallocation_name, free_offset);
            }
            emit_add_reg_imm(mod, ESP, 4);
            emit_add_reg_imm(mod, ESP, 4);
            emit_mov_reg_imm(mod, EAX, 0);
            emit_label(mod, done);
            return;
        }
        rcc_error(expr ? expr->loc : (SourceLoc){"<delete>", 0, 0},
                  "C++ delete cleanup metadata is incomplete");
        return;
    }
    skip_cleanup = new_label();
    done = new_label();
    gen_expr(mod, expr->call_args->expr);
    emit_cmp_reg_imm(mod, EAX, 0);
    emit_jcc_label(mod, CC_E, done);
    if (active_cxx_exception_cleanup_frame_offset != INT_MAX) {
        gen_cxx_register_delete_members32(
            mod, expr->call_delete_object_type,
            active_cxx_exception_cleanup_frame_offset);
    }
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
    if (active_cxx_exception_cleanup_frame_offset != INT_MAX) {
        gen_cxx_unregister_delete_members32(
            mod, expr->call_delete_object_type,
            active_cxx_exception_cleanup_frame_offset);
    }
    emit_mov_reg_mem(mod, EAX, ESP, 0);
    gen_cxx_destroy_members32(mod, expr->call_delete_object_type);
    emit_mov_reg_mem(mod, EAX, ESP, 0);
    emit_push_reg(mod, EAX);
    emit_byte(mod, 0xE8);
    uint32_t free_offset = code_offset(mod);
    emit_dword(mod, 0);
    add_func_call_ref(deallocation_name, free_offset);
    emit_add_reg_imm(mod, ESP, 4);
    emit_add_reg_imm(mod, ESP, 4);
    emit_mov_reg_imm(mod, EAX, 0);
    emit_label(mod, done);
}

static bool codegen_builtin_object_size(Expr* expression, uint64_t* size) {
    Type* type;
    Decl* declaration;
    int64_t index;
    uint32_t element_size;
    uint64_t offset;

    while (expression && expression->kind == EXPR_CAST) {
        expression = expression->cast_expr;
    }
    if (!expression || !size) return false;
    if (expression->kind == EXPR_STRING_LIT) {
        *size = (uint64_t)expression->str_byte_length +
                (expression->str_element_width
                     ? expression->str_element_width : 1u);
        return true;
    }
    if (expression->kind == EXPR_ADDR && expression->unary_operand) {
        return codegen_builtin_object_size(expression->unary_operand, size);
    }
    if (expression->kind == EXPR_INDEX && expression->index_base &&
        expression->index_expr &&
        codegen_builtin_object_size(expression->index_base, size) &&
        codegen_static_integer(expression->index_expr, &index) &&
        index >= 0) {
        element_size = codegen_pointer_element_size(
            expression->index_base->type);
        if (element_size == 0u ||
            (uint64_t)index > UINT64_MAX / element_size) {
            return false;
        }
        offset = (uint64_t)index * element_size;
        if (offset > *size) return false;
        *size -= offset;
        return true;
    }
    if (expression->kind != EXPR_IDENT || !expression->ident_decl) {
        return false;
    }
    declaration = expression->ident_decl;
    if (declaration->kind != DECL_VAR || !declaration->type) return false;
    type = declaration->type;
    if (type->kind == TYPE_ARRAY && type->array_len >= 0 &&
        !type->array_bound && type->size > 0) {
        *size = (uint64_t)type->size;
        return true;
    }
    if (type->kind != TYPE_PTR && type->kind != TYPE_ARRAY &&
        type->kind != TYPE_FUNC && type->size > 0) {
        *size = (uint64_t)type->size;
        return true;
    }
    return false;
}

static bool gen_compiler_overflow_builtin32(Module* mod, Expr* expr) {
    const char* name;
    Expr* result_address;
    Expr* lhs;
    Expr* rhs;
    Type* result_type;
    int width;
    bool is_multiply;
    bool is_subtract;

    if (!expr || expr->kind != EXPR_CALL || !expr->call_func ||
        expr->call_func->kind != EXPR_IDENT) return false;
    name = expr->call_func->ident_name;
    is_multiply = strcmp(name, "__builtin_mul_overflow") == 0;
    is_subtract = strcmp(name, "__builtin_sub_overflow") == 0;
    if (!is_multiply && !is_subtract &&
        strcmp(name, "__builtin_add_overflow") != 0) return false;
    result_address = call_argument(expr, 2);
    lhs = call_argument(expr, 0);
    rhs = call_argument(expr, 1);
    result_type = result_address && result_address->type &&
        result_address->type->kind == TYPE_PTR
        ? result_address->type->base : NULL;
    width = result_type ? result_type->size : 0;
    if (width == 8) {
        int stack_size = 0;

        /* The i686 ABI returns the boolean in EAX while the result object is
         * written through the third argument.  Keep the two operands as
         * explicit low/high words so the result remains independent of the
         * expression evaluation registers. */
        gen_expr(mod, result_address);
        emit_push_reg(mod, EAX); /* Result address. */
        gen_expr_as_integer64(mod, lhs);
        emit_push_reg(mod, EDX);
        emit_push_reg(mod, EAX);
        gen_expr_as_integer64(mod, rhs);
        emit_push_reg(mod, EDX);
        emit_push_reg(mod, EAX);

        if (!is_multiply) {
            /* Stack layout: rhs.low, rhs.high, lhs.low, lhs.high,
             * result_address. */
            emit_mov_reg_mem(mod, EAX, ESP, 8);
            emit_mov_reg_mem(mod, EDX, ESP, 12);
            emit_mov_reg_mem(mod, ECX, ESP, 0);
            emit_mov_reg_mem(mod, EDI, ESP, 4);
            if (is_subtract) {
                emit_sub_reg_reg(mod, EAX, ECX);
                emit_sbb_reg_reg(mod, EDX, EDI);
            } else {
                emit_add_reg_reg(mod, EAX, ECX);
                emit_adc_reg_reg(mod, EDX, EDI);
            }
            emit_setcc(mod, result_type && result_type->is_unsigned
                ? CC_B : CC_O, EBX);
            emit_byte(mod, 0x0F);
            emit_byte(mod, 0xB6);
            emit_byte(mod, modrm(3, EBX, EBX));
            emit_mov_reg_mem(mod, ESI, ESP, 16);
            emit_mov_mem_reg(mod, ESI, 0, EAX);
            emit_mov_mem_reg(mod, ESI, 4, EDX);
            emit_add_reg_imm(mod, ESP, 20);
            emit_mov_reg_reg(mod, EAX, EBX);
            return true;
        }

        /* Compute the complete unsigned 128-bit product from four 32x32
         * partial products.  The low 64 bits are the result object; the
         * upper 64 bits are retained for an exact overflow test. */
        enum {
            MUL_ALO = 0,
            MUL_AHI = 4,
            MUL_BLO = 8,
            MUL_BHI = 12,
            MUL_P0HI = 16,
            MUL_P1LO = 20,
            MUL_P1HI = 24,
            MUL_P2LO = 28,
            MUL_P2HI = 32,
            MUL_P3LO = 36,
            MUL_P3HI = 40,
            MUL_RESULTLO = 44,
            MUL_RESULTHI = 48,
            MUL_LOWCARRY = 52,
            MUL_UPPERLO = 56,
            MUL_UPPERHI = 60,
            MUL_STORAGE = 64
        };

        emit_sub_reg_imm(mod, ESP, MUL_STORAGE);
        emit_mov_reg_mem(mod, EAX, ESP, MUL_STORAGE + 8);
        emit_mov_mem_reg(mod, ESP, MUL_ALO, EAX);
        emit_mov_reg_mem(mod, EAX, ESP, MUL_STORAGE + 12);
        emit_mov_mem_reg(mod, ESP, MUL_AHI, EAX);
        emit_mov_reg_mem(mod, EAX, ESP, MUL_STORAGE + 0);
        emit_mov_mem_reg(mod, ESP, MUL_BLO, EAX);
        emit_mov_reg_mem(mod, EAX, ESP, MUL_STORAGE + 4);
        emit_mov_mem_reg(mod, ESP, MUL_BHI, EAX);

        emit_mov_reg_mem(mod, EAX, ESP, MUL_ALO);
        emit_mov_reg_mem(mod, EBX, ESP, MUL_BLO);
        emit_mul_reg(mod, EBX);
        emit_mov_mem_reg(mod, ESP, MUL_RESULTLO, EAX);
        emit_mov_mem_reg(mod, ESP, MUL_P0HI, EDX);

        emit_mov_reg_mem(mod, EAX, ESP, MUL_ALO);
        emit_mov_reg_mem(mod, EBX, ESP, MUL_BHI);
        emit_mul_reg(mod, EBX);
        emit_mov_mem_reg(mod, ESP, MUL_P1LO, EAX);
        emit_mov_mem_reg(mod, ESP, MUL_P1HI, EDX);

        emit_mov_reg_mem(mod, EAX, ESP, MUL_AHI);
        emit_mov_reg_mem(mod, EBX, ESP, MUL_BLO);
        emit_mul_reg(mod, EBX);
        emit_mov_mem_reg(mod, ESP, MUL_P2LO, EAX);
        emit_mov_mem_reg(mod, ESP, MUL_P2HI, EDX);

        emit_mov_reg_mem(mod, EAX, ESP, MUL_AHI);
        emit_mov_reg_mem(mod, EBX, ESP, MUL_BHI);
        emit_mul_reg(mod, EBX);
        emit_mov_mem_reg(mod, ESP, MUL_P3LO, EAX);
        emit_mov_mem_reg(mod, ESP, MUL_P3HI, EDX);

        /* result.high = p0.high + p1.low + p2.low. */
        emit_mov_reg_mem(mod, EAX, ESP, MUL_P0HI);
        emit_mov_reg_mem(mod, EBX, ESP, MUL_P1LO);
        emit_add_reg_reg(mod, EAX, EBX);
        emit_mov_reg_mem(mod, ECX, ESP, MUL_P2LO);
        emit_adc_reg_reg(mod, EAX, ECX);
        emit_setcc(mod, CC_B, EBX);
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xB6);
        emit_byte(mod, modrm(3, EBX, EBX));
        emit_mov_mem_reg(mod, ESP, MUL_LOWCARRY, EBX);
        emit_mov_mem_reg(mod, ESP, MUL_RESULTHI, EAX);

        /* upper.low = p3.low + p1.high + p2.high + carry(result.high). */
        emit_mov_reg_mem(mod, EAX, ESP, MUL_P3LO);
        emit_mov_reg_mem(mod, ECX, ESP, MUL_P1HI);
        emit_add_reg_reg(mod, EAX, ECX);
        emit_mov_reg_mem(mod, ECX, ESP, MUL_P2HI);
        emit_adc_reg_reg(mod, EAX, ECX);
        emit_mov_reg_mem(mod, ECX, ESP, MUL_LOWCARRY);
        emit_add_reg_reg(mod, EAX, ECX);
        emit_mov_mem_reg(mod, ESP, MUL_UPPERLO, EAX);
        emit_mov_reg_mem(mod, EDI, ESP, MUL_P3HI);
        emit_adc_reg_imm8(mod, EDI, 0u);
        emit_mov_mem_reg(mod, ESP, MUL_UPPERHI, EDI);

        emit_mov_reg_mem(mod, EAX, ESP, MUL_RESULTLO);
        emit_mov_reg_mem(mod, EDX, ESP, MUL_RESULTHI);
        emit_mov_reg_mem(mod, EBX, ESP, MUL_STORAGE + 16);
        emit_mov_mem_reg(mod, EBX, 0, EAX);
        emit_mov_mem_reg(mod, EBX, 4, EDX);

        if (result_type && result_type->is_unsigned) {
            emit_mov_reg_mem(mod, EAX, ESP, MUL_UPPERLO);
            emit_mov_reg_mem(mod, ECX, ESP, MUL_UPPERHI);
            emit_or_reg_reg(mod, EAX, ECX);
            emit_test_reg_reg(mod, EAX, EAX);
            emit_setcc(mod, CC_NE, EAX);
        } else {
            int signed_ok_label = new_label();
            emit_mov_reg_mem(mod, EDX, ESP, MUL_RESULTHI);
            emit_mov_reg_reg(mod, ECX, EDX);
            emit_sar_reg_imm(mod, ECX, 31);
            emit_mov_reg_mem(mod, EAX, ESP, MUL_UPPERLO);
            emit_cmp_reg_reg(mod, EAX, ECX);
            emit_jcc_label(mod, CC_NE, signed_ok_label);
            emit_mov_reg_mem(mod, EAX, ESP, MUL_UPPERHI);
            emit_cmp_reg_reg(mod, EAX, ECX);
            emit_label(mod, signed_ok_label);
            emit_setcc(mod, CC_NE, EAX);
        }
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xB6);
        emit_byte(mod, modrm(3, EAX, EAX));
        stack_size = MUL_STORAGE + 20;
        emit_add_reg_imm(mod, ESP, stack_size);
        return true;
    }
    if (!is_multiply && (width < 1 || width > 4)) {
        rcc_error(expr->loc,
                  "i686 checked addition/subtraction supports 1-, 2-, and 4-byte result types");
        return true;
    }

    gen_expr(mod, result_address);
    emit_push_reg(mod, EAX);
    gen_expr(mod, lhs);
    emit_push_reg(mod, EAX);
    gen_expr(mod, rhs);
    emit_mov_reg_reg(mod, ECX, EAX);
    emit_pop_reg(mod, EAX);
    if (is_multiply) {
        if (result_type && result_type->is_unsigned) {
            emit_mul_reg(mod, ECX);
        } else {
            emit_imul_reg(mod, ECX);
        }
    } else if (is_subtract) {
        emit_sub_reg_reg_width32(mod, EAX, ECX, width);
    } else {
        emit_add_reg_reg_width32(mod, EAX, ECX, width);
    }
    emit_pop_reg(mod, ECX);
    emit_store_typed32(mod, ECX, 0, EAX, result_type);
    if (is_multiply) {
        if (result_type && result_type->is_unsigned) {
            if (width == 4) {
                emit_test_reg_reg(mod, EDX, EDX);
                emit_setcc(mod, CC_NE, EAX);
            } else {
                emit_test_reg_reg(mod, EDX, EDX);
                emit_setcc(mod, CC_NE, EBX);
                emit_byte(mod, 0x0F);
                emit_byte(mod, 0xB6);
                emit_byte(mod, modrm(3, EBX, EBX));
                emit_cmp_reg_imm(mod, EAX,
                                 width == 1 ? 0xff : 0xffff);
                emit_setcc(mod, CC_A, EDX);
                emit_byte(mod, 0x0F);
                emit_byte(mod, 0xB6);
                emit_byte(mod, modrm(3, EDX, EDX));
                emit_or_reg_reg(mod, EBX, EDX);
                emit_mov_reg_reg(mod, EAX, EBX);
            }
        } else {
            if (width < 4) {
                emit_mov_reg_reg(mod, ECX, EAX);
                emit_shl_reg_imm(mod, ECX,
                                 width == 1 ? 24u : 16u);
                emit_sar_reg_imm(mod, ECX,
                                 width == 1 ? 24u : 16u);
                emit_cmp_reg_reg(mod, EAX, ECX);
                emit_setcc(mod, CC_NE, EAX);
            } else {
                emit_mov_reg_reg(mod, ECX, EAX);
                emit_sar_reg_imm(mod, ECX, 31);
                emit_cmp_reg_reg(mod, EDX, ECX);
                emit_setcc(mod, CC_NE, EAX);
            }
        }
    } else {
        emit_setcc(mod, result_type && result_type->is_unsigned
            ? CC_B : CC_O, EAX);
    }
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0xB6);
    emit_byte(mod, modrm(3, EAX, EAX));
    return true;
}

static int gen_float_math_builtin_kind(const char* name) {
    if (!name) return 0;
    if (strcmp(name, "__builtin_fpclassify") == 0) return 12;
    if (strcmp(name, "__builtin_isnan") == 0) return 1;
    if (strcmp(name, "__builtin_isinf") == 0) return 2;
    if (strcmp(name, "__builtin_isfinite") == 0) return 3;
    if (strcmp(name, "__builtin_isnormal") == 0) return 4;
    if (strcmp(name, "__builtin_signbit") == 0) return 5;
    if (strcmp(name, "__builtin_copysign") == 0 ||
        strcmp(name, "__builtin_copysignf") == 0 ||
        strcmp(name, "__builtin_copysignl") == 0) return 6;
    if (strcmp(name, "__builtin_floor") == 0 ||
        strcmp(name, "__builtin_floorf") == 0 ||
        strcmp(name, "__builtin_floorl") == 0) return 7;
    if (strcmp(name, "__builtin_ceil") == 0 ||
        strcmp(name, "__builtin_ceilf") == 0 ||
        strcmp(name, "__builtin_ceill") == 0) return 8;
    if (strcmp(name, "__builtin_round") == 0 ||
        strcmp(name, "__builtin_roundf") == 0 ||
        strcmp(name, "__builtin_roundl") == 0) return 9;
    if (strcmp(name, "__builtin_trunc") == 0 ||
        strcmp(name, "__builtin_truncf") == 0 ||
        strcmp(name, "__builtin_truncl") == 0) return 10;
    if (strcmp(name, "__builtin_rint") == 0 ||
        strcmp(name, "__builtin_rintf") == 0 ||
        strcmp(name, "__builtin_rintl") == 0) return 11;
    return 0;
}

static bool gen_float_math_builtin32(Module* mod, Expr* expr) {
    Expr* first;
    Expr* second;
    const char* name;
    int kind;
    int width;
    if (!expr || expr->kind != EXPR_CALL || !expr->call_func ||
        expr->call_func->kind != EXPR_IDENT ||
        !expr->call_func->ident_name) return false;
    name = expr->call_func->ident_name;
    kind = gen_float_math_builtin_kind(name);
    if (!kind) return false;
    first = call_argument(expr, 0);
    second = call_argument(expr, 1);
    width = first && first->type && first->type->size == 4 ? 4 : 8;

    if (kind == 12) {
        int64_t class_value[5] = {0, 0, 0, 0, 0};
        int index;
        int nan_label = new_label();
        int inf_label = new_label();
        int subnormal_label = new_label();
        int zero_label = new_label();
        int done_label = new_label();
        Expr* floating = call_argument(expr, 5);
        for (index = 0; index < 5; ++index) {
            if (!expr_eval_integer_constant(
                    call_argument(expr, index), &class_value[index])) {
                rcc_error(expr->loc,
                          "__builtin_fpclassify class values must be integer constants");
                class_value[index] = 0;
            }
        }
        width = floating && floating->type && floating->type->size == 4
            ? 4 : 8;
        if (width == 4) {
            gen_expr(mod, floating);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_shr_reg_imm(mod, ECX, 23);
            emit_and_reg_imm(mod, ECX, 0xffu);
            emit_cmp_reg_imm(mod, ECX, 0xff);
            emit_jcc_label(mod, CC_E, inf_label);
            emit_cmp_reg_imm(mod, ECX, 0);
            emit_jcc_label(mod, CC_E, zero_label);
            emit_mov_reg_imm(mod, EAX, (uint32_t)class_value[2]);
            emit_jmp_label(mod, done_label);
            emit_label(mod, inf_label);
            emit_test_reg_imm(mod, EAX, UINT32_C(0x007fffff));
            emit_jcc_label(mod, CC_NE, nan_label);
            emit_mov_reg_imm(mod, EAX, (uint32_t)class_value[1]);
            emit_jmp_label(mod, done_label);
            emit_label(mod, nan_label);
            emit_mov_reg_imm(mod, EAX, (uint32_t)class_value[0]);
            emit_jmp_label(mod, done_label);
            emit_label(mod, zero_label);
            emit_test_reg_imm(mod, EAX, UINT32_C(0x007fffff));
            emit_jcc_label(mod, CC_NE, subnormal_label);
            emit_mov_reg_imm(mod, EAX, (uint32_t)class_value[4]);
            emit_jmp_label(mod, done_label);
            emit_label(mod, subnormal_label);
            emit_mov_reg_imm(mod, EAX, (uint32_t)class_value[3]);
            emit_label(mod, done_label);
        } else {
            /* Floating expressions already return their raw scalar bits in
             * EDX:EAX on i686; the integer conversion helper would zero the
             * high word because the source type is floating-point. */
            gen_expr(mod, floating);
            emit_mov_reg_reg(mod, ECX, EDX);
            emit_shr_reg_imm(mod, ECX, 20);
            emit_and_reg_imm(mod, ECX, 0x7ffu);
            emit_cmp_reg_imm(mod, ECX, 0x7ff);
            emit_jcc_label(mod, CC_E, inf_label);
            emit_cmp_reg_imm(mod, ECX, 0);
            emit_jcc_label(mod, CC_E, zero_label);
            emit_mov_reg_imm(mod, EAX, (uint32_t)class_value[2]);
            emit_jmp_label(mod, done_label);
            emit_label(mod, inf_label);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_and_reg_imm(mod, EDX, UINT32_C(0x000fffff));
            emit_or_reg_reg(mod, ECX, EDX);
            emit_test_reg_reg(mod, ECX, ECX);
            emit_jcc_label(mod, CC_NE, nan_label);
            emit_mov_reg_imm(mod, EAX, (uint32_t)class_value[1]);
            emit_jmp_label(mod, done_label);
            emit_label(mod, nan_label);
            emit_mov_reg_imm(mod, EAX, (uint32_t)class_value[0]);
            emit_jmp_label(mod, done_label);
            emit_label(mod, zero_label);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_and_reg_imm(mod, EDX, UINT32_C(0x000fffff));
            emit_or_reg_reg(mod, ECX, EDX);
            emit_test_reg_reg(mod, ECX, ECX);
            emit_jcc_label(mod, CC_NE, subnormal_label);
            emit_mov_reg_imm(mod, EAX, (uint32_t)class_value[4]);
            emit_jmp_label(mod, done_label);
            emit_label(mod, subnormal_label);
            emit_mov_reg_imm(mod, EAX, (uint32_t)class_value[3]);
            emit_label(mod, done_label);
        }
        return true;
    }

    if (kind == 1 || kind == 2 || kind == 3 || kind == 4 || kind == 5) {
        uint32_t exponent;
        if (width == 4) {
            gen_expr(mod, first);
            if (kind == 5) {
                emit_mov_reg_reg(mod, ECX, EAX);
                emit_and_reg_imm(mod, ECX, UINT32_C(0x80000000));
                emit_cmp_reg_imm(mod, ECX, 0);
                emit_setcc(mod, CC_NE, EAX);
                emit_byte(mod, 0x0F); emit_byte(mod, 0xB6);
                emit_byte(mod, modrm(3, EAX, EAX));
                return true;
            }
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_shr_reg_imm(mod, ECX, 23);
            emit_and_reg_imm(mod, ECX, 0xffu);
            exponent = 0xffu;
        } else {
            gen_expr(mod, first);
            if (kind == 5) {
                emit_mov_reg_reg(mod, ECX, EDX);
                emit_and_reg_imm(mod, ECX, UINT32_C(0x80000000));
                emit_cmp_reg_imm(mod, ECX, 0);
                emit_setcc(mod, CC_NE, EAX);
                emit_byte(mod, 0x0F); emit_byte(mod, 0xB6);
                emit_byte(mod, modrm(3, EAX, EAX));
                return true;
            }
            emit_mov_reg_reg(mod, ECX, EDX);
            emit_shr_reg_imm(mod, ECX, 20);
            emit_and_reg_imm(mod, ECX, 0x7ffu);
            exponent = 0x7ffu;
        }
        if (kind == 3) {
            emit_cmp_reg_imm(mod, ECX, (int32_t)exponent);
            emit_setcc(mod, CC_NE, EAX);
        } else if (kind == 4) {
            emit_mov_reg_reg(mod, EAX, ECX);
            emit_cmp_reg_imm(mod, ECX, 0);
            emit_setcc(mod, CC_NE, EAX);
            emit_cmp_reg_imm(mod, ECX, (int32_t)exponent);
            emit_setcc(mod, CC_NE, ECX);
            emit_and_reg_reg(mod, EAX, ECX);
        } else {
            int done = new_label();
            int not_special = new_label();
            emit_cmp_reg_imm(mod, ECX, (int32_t)exponent);
            emit_jcc_label(mod, CC_NE, not_special);
            if (width == 4) {
                emit_test_reg_imm(mod, EAX, UINT32_C(0x007fffff));
            } else {
                emit_mov_reg_reg(mod, ECX, EAX);
                emit_and_reg_imm(mod, EDX, UINT32_C(0x000fffff));
                emit_or_reg_reg(mod, ECX, EDX);
                emit_test_reg_reg(mod, ECX, ECX);
            }
            emit_setcc(mod, kind == 1 ? CC_NE : CC_E, EAX);
            emit_byte(mod, 0x0F); emit_byte(mod, 0xB6);
            emit_byte(mod, modrm(3, EAX, EAX));
            emit_jmp_label(mod, done);
            emit_label(mod, not_special);
            emit_mov_reg_imm(mod, EAX, 0u);
            emit_label(mod, done);
        }
        return true;
    }

    if (kind == 6) {
        if (width == 4) {
            gen_expr(mod, first);
            emit_sub_reg_imm(mod, ESP, 4);
            emit_mov_mem_reg(mod, ESP, 0, EAX);
            gen_expr(mod, second);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_and_reg_imm(mod, ECX, UINT32_C(0x80000000));
            emit_mov_reg_mem(mod, EAX, ESP, 0);
            emit_and_reg_imm(mod, EAX, UINT32_C(0x7fffffff));
            emit_or_reg_reg(mod, EAX, ECX);
            emit_add_reg_imm(mod, ESP, 4);
        } else {
            gen_expr(mod, first);
            emit_sub_reg_imm(mod, ESP, 8);
            emit_mov_mem_reg(mod, ESP, 0, EAX);
            emit_mov_mem_reg(mod, ESP, 4, EDX);
            gen_expr(mod, second);
            emit_mov_reg_reg(mod, ECX, EDX);
            emit_and_reg_imm(mod, ECX, UINT32_C(0x80000000));
            emit_mov_reg_mem(mod, EAX, ESP, 0);
            emit_mov_reg_mem(mod, EDX, ESP, 4);
            emit_and_reg_imm(mod, EDX, UINT32_C(0x7fffffff));
            emit_or_reg_reg(mod, EDX, ECX);
            emit_add_reg_imm(mod, ESP, 8);
        }
        return true;
    }

    /* FRNDINT follows the x87 control-word rounding mode. Save that word,
     * install the builtin's specified direction, then restore it verbatim. */
    gen_expr(mod, first);
    emit_sub_reg_imm(mod, ESP, 20);
    emit_x87_memory(mod, 0xD9, 7, ESP, 0); /* FNSTCW */
    if (width == 4) emit_mov_mem_reg(mod, ESP, 4, EAX);
    else {
        emit_mov_mem_reg(mod, ESP, 4, EAX);
        emit_mov_mem_reg(mod, ESP, 8, EDX);
    }
    if (kind != 11) {
        emit_load_typed32(mod, EAX, ESP, 0, type_ushort);
        emit_and_reg_imm(mod, EAX, UINT32_C(0x0000f3ff));
        emit_byte(mod, 0x81);
        emit_byte(mod, modrm(3, 1, EAX));
        emit_dword(mod, kind == 7 ? UINT32_C(0x0400) :
            kind == 8 ? UINT32_C(0x0800) : UINT32_C(0x0c00));
        emit_store_typed32(mod, ESP, 2, EAX, type_ushort);
        emit_x87_memory(mod, 0xD9, 5, ESP, 2); /* FLDCW */
    }
    if (kind == 9) {
        if (width == 4) {
            emit_mov_reg_imm(mod, EAX, UINT32_C(0x3f000000));
            emit_mov_mem_reg(mod, ESP, 12, EAX);
        } else {
            emit_mov_reg_imm(mod, EAX, UINT32_C(0x00000000));
            emit_mov_reg_imm(mod, EDX, UINT32_C(0x3fe00000));
            emit_mov_mem_reg(mod, ESP, 12, EAX);
            emit_mov_mem_reg(mod, ESP, 16, EDX);
        }
    }
    emit_x87_load_memory(mod, width, ESP, 4);
    if (kind == 9) {
        emit_byte(mod, 0xD9); emit_byte(mod, 0xE1); /* FABS */
        emit_x87_memory(mod, width == 4 ? 0xD8 : 0xDC, 0, ESP, 12);
    }
    emit_byte(mod, 0xD9); emit_byte(mod, 0xFC); /* FRNDINT */
    if (kind == 9) {
        emit_mov_reg_mem(mod, ECX, ESP, width == 4 ? 4 : 8);
        emit_test_reg_imm(mod, ECX, UINT32_C(0x80000000));
        {
            int positive = new_label();
            emit_jcc_label(mod, CC_NS, positive);
            emit_byte(mod, 0xD9); emit_byte(mod, 0xE0); /* FCHS */
            emit_label(mod, positive);
        }
    }
    emit_x87_store_pop_memory(mod, width, ESP, 4);
    emit_x87_memory(mod, 0xD9, 5, ESP, 0); /* Restore caller's control word. */
    emit_mov_reg_mem(mod, EAX, ESP, 4);
    if (width == 8) emit_mov_reg_mem(mod, EDX, ESP, 8);
    emit_add_reg_imm(mod, ESP, 20);
    return true;
}

static bool gen_compiler_builtin(Module* mod, Expr* expr) {
    Expr* function;
    Expr* argument;
    int64_t value;
    int64_t rw;
    int64_t locality;
    int bswap_width;

    if (!expr || expr->kind != EXPR_CALL ||
        !expr->call_func || expr->call_func->kind != EXPR_IDENT) {
        return false;
    }
    function = expr->call_func;
    if (gen_float_math_builtin32(mod, expr)) return true;
    if (strcmp(function->ident_name, "__builtin_choose_expr") == 0) {
        int64_t condition = 0;
        Expr* condition_expression = call_argument(expr, 0);
        (void)expr_eval_integer_constant(condition_expression, &condition);
        gen_expr(mod, call_argument(expr, condition != 0 ? 1 : 2));
        return true;
    }
    if (gen_compiler_overflow_builtin32(mod, expr)) return true;
    if (gen_mmx_builtin32(mod, expr)) return true;
    if (gen_sse_builtin32(mod, expr)) return true;
    bswap_width = strcmp(function->ident_name, "__builtin_bswap16") == 0 ? 2 :
        strcmp(function->ident_name, "__builtin_bswap32") == 0 ? 4 :
        strcmp(function->ident_name, "__builtin_bswap64") == 0 ? 8 : 0;
    if (bswap_width != 0) {
        argument = call_argument(expr, 0);
        if (bswap_width == 8) {
            if (argument && argument->type && argument->type->size == 8) {
                gen_expr_as_integer64(mod, argument);
            } else {
                gen_expr(mod, argument);
                emit_xor_reg_reg(mod, EDX, EDX);
            }
            emit_bswap_reg32(mod, EAX);
            emit_bswap_reg32(mod, EDX);
            emit_xchg_eax_edx(mod);
        } else {
            gen_expr(mod, argument);
            if (bswap_width == 2) {
                emit_rol_ax_8(mod);
                emit_movzx_eax_ax(mod);
            } else {
                emit_bswap_reg32(mod, EAX);
            }
        }
        return true;
    }
    if (strcmp(function->ident_name, "__builtin_clrsb") == 0 ||
        strcmp(function->ident_name, "__builtin_clrsbl") == 0) {
        int zero_label = new_label();
        int end_label = new_label();
        argument = call_argument(expr, 0);
        gen_expr(mod, argument);
        emit_mov_reg_reg(mod, ECX, EAX);
        emit_sar_reg_imm(mod, ECX, 31);
        emit_xor_reg_reg(mod, EAX, ECX);
        emit_test_reg_reg(mod, EAX, EAX);
        emit_jcc_label(mod, CC_E, zero_label);
        emit_bsr_reg_reg(mod, ECX, EAX);
        emit_mov_reg_imm(mod, EAX, 30u);
        emit_sub_reg_reg(mod, EAX, ECX);
        emit_jmp_label(mod, end_label);
        emit_label(mod, zero_label);
        emit_mov_reg_imm(mod, EAX, 31u);
        emit_label(mod, end_label);
        return true;
    }
    if (strcmp(function->ident_name, "__builtin_clrsbll") == 0) {
        int high_label = new_label();
        int low_label = new_label();
        int zero_label = new_label();
        int end_label = new_label();
        argument = call_argument(expr, 0);
        gen_expr_as_integer64(mod, argument);
        emit_mov_reg_reg(mod, ECX, EDX);
        emit_sar_reg_imm(mod, ECX, 31);
        emit_xor_reg_reg(mod, EAX, ECX);
        emit_xor_reg_reg(mod, EDX, ECX);
        emit_test_reg_reg(mod, EDX, EDX);
        emit_jcc_label(mod, CC_NE, high_label);
        emit_test_reg_reg(mod, EAX, EAX);
        emit_jcc_label(mod, CC_NE, low_label);
        emit_jmp_label(mod, zero_label);
        emit_label(mod, high_label);
        emit_bsr_reg_reg(mod, ECX, EDX);
        emit_mov_reg_imm(mod, EAX, 30u);
        emit_sub_reg_reg(mod, EAX, ECX);
        emit_jmp_label(mod, end_label);
        emit_label(mod, low_label);
        emit_bsr_reg_reg(mod, ECX, EAX);
        emit_mov_reg_imm(mod, EAX, 62u);
        emit_sub_reg_reg(mod, EAX, ECX);
        emit_jmp_label(mod, end_label);
        emit_label(mod, zero_label);
        emit_mov_reg_imm(mod, EAX, 63u);
        emit_label(mod, end_label);
        return true;
    }
    if (strcmp(function->ident_name, "__builtin_clz") == 0 ||
        strcmp(function->ident_name, "__builtin_ctz") == 0 ||
        strcmp(function->ident_name, "__builtin_popcount") == 0 ||
        strcmp(function->ident_name, "__builtin_parity") == 0 ||
        strcmp(function->ident_name, "__builtin_clzl") == 0 ||
        strcmp(function->ident_name, "__builtin_ctzl") == 0 ||
        strcmp(function->ident_name, "__builtin_popcountl") == 0 ||
        strcmp(function->ident_name, "__builtin_parityl") == 0 ||
        strcmp(function->ident_name, "__builtin_ffs") == 0 ||
        strcmp(function->ident_name, "__builtin_ffsl") == 0) {
        argument = call_argument(expr, 0);
        gen_expr(mod, argument);
        if (strcmp(function->ident_name, "__builtin_ffs") == 0 ||
            strcmp(function->ident_name, "__builtin_ffsl") == 0) {
            int zero_label = new_label();
            int end_label = new_label();
            emit_test_reg_reg(mod, EAX, EAX);
            emit_jcc_label(mod, CC_E, zero_label);
            emit_bsf_reg_reg(mod, ECX, EAX);
            emit_mov_reg_reg(mod, EAX, ECX);
            emit_add_reg_imm(mod, EAX, 1);
            emit_jmp_label(mod, end_label);
            emit_label(mod, zero_label);
            emit_xor_reg_reg(mod, EAX, EAX);
            emit_label(mod, end_label);
        } else if (strcmp(function->ident_name, "__builtin_popcount") == 0 ||
            strcmp(function->ident_name, "__builtin_parity") == 0 ||
            strcmp(function->ident_name, "__builtin_popcountl") == 0 ||
            strcmp(function->ident_name, "__builtin_parityl") == 0) {
            emit_popcount32(mod);
            if (strcmp(function->ident_name, "__builtin_parity") == 0 ||
                strcmp(function->ident_name, "__builtin_parityl") == 0) {
                emit_and_reg_imm(mod, EAX, 1u);
            }
        } else if (strcmp(function->ident_name, "__builtin_clz") == 0) {
            emit_bsr_reg_reg(mod, ECX, EAX);
            emit_mov_reg_imm(mod, EAX, 31u);
            emit_sub_reg_reg(mod, EAX, ECX);
        } else {
            emit_bsf_reg_reg(mod, ECX, EAX);
            emit_mov_reg_reg(mod, EAX, ECX);
        }
        return true;
    }
    if (strcmp(function->ident_name, "__builtin_clzll") == 0 ||
        strcmp(function->ident_name, "__builtin_ctzll") == 0 ||
        strcmp(function->ident_name, "__builtin_popcountll") == 0 ||
        strcmp(function->ident_name, "__builtin_parityll") == 0 ||
        strcmp(function->ident_name, "__builtin_ffsll") == 0) {
        argument = call_argument(expr, 0);
        if (argument && argument->type && argument->type->size == 8) {
            gen_expr_as_integer64(mod, argument);
        } else {
            gen_expr(mod, argument);
            emit_xor_reg_reg(mod, EDX, EDX);
        }
        if (strcmp(function->ident_name, "__builtin_ffsll") == 0) {
            int low_label = new_label();
            int high_label = new_label();
            int zero_label = new_label();
            int end_label = new_label();
            emit_test_reg_reg(mod, EAX, EAX);
            emit_jcc_label(mod, CC_NE, low_label);
            emit_test_reg_reg(mod, EDX, EDX);
            emit_jcc_label(mod, CC_NE, high_label);
            emit_jmp_label(mod, zero_label);
            emit_label(mod, low_label);
            emit_bsf_reg_reg(mod, ECX, EAX);
            emit_mov_reg_reg(mod, EAX, ECX);
            emit_add_reg_imm(mod, EAX, 1);
            emit_jmp_label(mod, end_label);
            emit_label(mod, high_label);
            emit_bsf_reg_reg(mod, ECX, EDX);
            emit_mov_reg_reg(mod, EAX, ECX);
            emit_add_reg_imm(mod, EAX, 33);
            emit_jmp_label(mod, end_label);
            emit_label(mod, zero_label);
            emit_xor_reg_reg(mod, EAX, EAX);
            emit_label(mod, end_label);
        } else if (strcmp(function->ident_name, "__builtin_popcountll") == 0 ||
            strcmp(function->ident_name, "__builtin_parityll") == 0) {
            emit_push_reg(mod, EDX);
            emit_popcount32(mod);
            emit_push_reg(mod, EAX);
            emit_mov_reg_mem(mod, EAX, ESP, 4);
            emit_popcount32(mod);
            emit_pop_reg(mod, ECX);
            emit_add_reg_reg(mod, EAX, ECX);
            if (strcmp(function->ident_name, "__builtin_parityll") == 0) {
                emit_and_reg_imm(mod, EAX, 1u);
            }
        } else if (strcmp(function->ident_name, "__builtin_clzll") == 0) {
            int high_label = new_label();
            int end_label = new_label();
            emit_test_reg_reg(mod, EDX, EDX);
            emit_jcc_label(mod, CC_NE, high_label);
            emit_bsr_reg_reg(mod, ECX, EAX);
            emit_mov_reg_imm(mod, EAX, 31u);
            emit_sub_reg_reg(mod, EAX, ECX);
            emit_add_reg_imm(mod, EAX, 32);
            emit_jmp_label(mod, end_label);
            emit_label(mod, high_label);
            emit_bsr_reg_reg(mod, ECX, EDX);
            emit_mov_reg_imm(mod, EAX, 31u);
            emit_sub_reg_reg(mod, EAX, ECX);
            emit_label(mod, end_label);
        } else {
            int low_label = new_label();
            int end_label = new_label();
            emit_test_reg_reg(mod, EAX, EAX);
            emit_jcc_label(mod, CC_NE, low_label);
            emit_bsf_reg_reg(mod, ECX, EDX);
            emit_mov_reg_reg(mod, EAX, ECX);
            emit_add_reg_imm(mod, EAX, 32);
            emit_jmp_label(mod, end_label);
            emit_label(mod, low_label);
            emit_bsf_reg_reg(mod, ECX, EAX);
            emit_mov_reg_reg(mod, EAX, ECX);
            emit_label(mod, end_label);
        }
        return true;
    }
    if (strcmp(function->ident_name, "__builtin_prefetch") == 0) {
        rw = 0;
        locality = 3;
        argument = call_argument(expr, 1);
        if (argument) (void)expr_eval_integer_constant(argument, &rw);
        argument = call_argument(expr, 2);
        if (argument) (void)expr_eval_integer_constant(argument, &locality);
        gen_expr(mod, call_argument(expr, 0));
        value = locality == 0 ? 0 : 4 - locality;
        emit_prefetch32(mod, rw != 0, (int)value);
        return true;
    }
    if (strcmp(function->ident_name, "__builtin_constant_p") == 0) {
        Expr* argument = call_argument(expr, 0);
        int64_t constant = 0;
        emit_mov_reg_imm(mod, EAX,
                         argument && expr_eval_integer_constant(
                             argument, &constant) ? 1u : 0u);
        return true;
    }
    if (strcmp(function->ident_name,
               "__builtin_is_constant_evaluated") == 0) {
        emit_mov_reg_imm(mod, EAX, 0u);
        return true;
    }
    if (strcmp(function->ident_name, "__builtin_object_size") == 0) {
        Expr* object = call_argument(expr, 0);
        int64_t mode = 0;
        uint64_t object_size = 0;
        bool known = codegen_builtin_object_size(object, &object_size);
        (void)codegen_static_integer(call_argument(expr, 1), &mode);
        if (!known) object_size = (mode & 2) != 0 ? 0u : UINT64_MAX;
        emit_mov_reg_imm(mod, EAX, (uint32_t)object_size);
        return true;
    }
    if (strcmp(function->ident_name, "__builtin_strlen") == 0) {
        Expr* string = call_argument(expr, 0);
        Expr* literal = string;
        while (literal && literal->kind == EXPR_CAST) literal = literal->cast_expr;
        if (literal && literal->kind == EXPR_STRING_LIT &&
            literal->str_length <= UINT32_MAX) {
            emit_mov_reg_imm(mod, EAX, (uint32_t)literal->str_length);
            return true;
        }
        if (!string) {
            rcc_error(expr->loc,
                      "__builtin_strlen has no operand");
            return true;
        }
        {
            int loop = new_label();
            int done = new_label();
            gen_expr(mod, string);
            emit_mov_reg_reg(mod, EDX, EAX);
            emit_xor_reg_reg(mod, ECX, ECX);
            emit_label(mod, loop);
            emit_load_typed32(mod, EAX, EDX, 0, type_uchar);
            emit_test_reg_reg(mod, EAX, EAX);
            emit_jcc_label(mod, CC_E, done);
            emit_add_reg_imm(mod, EDX, 1);
            emit_add_reg_imm(mod, ECX, 1);
            emit_jmp_label(mod, loop);
            emit_label(mod, done);
            emit_mov_reg_reg(mod, EAX, ECX);
        }
        return true;
    }
    if (strcmp(function->ident_name, "__builtin_expect") == 0 ||
        strcmp(function->ident_name,
               "__builtin_expect_with_probability") == 0) {
        /* The prediction operand has no runtime value.  Evaluate it for its
         * language-level side effects, then leave the first operand in the
         * normal scalar return registers.  The probability is a validated
         * compile-time constant and has no runtime evaluation. */
        gen_expr(mod, call_argument(expr, 1));
        gen_expr(mod, call_argument(expr, 0));
        return true;
    }
    if (strcmp(function->ident_name, "__builtin_assume_aligned") == 0) {
        /* Alignment and optional offset are compile-time contracts.  Sema
         * has already required them to be integer constants, so the runtime
         * value is exactly the original pointer expression. */
        gen_expr(mod, call_argument(expr, 0));
        return true;
    }
    if (strcmp(function->ident_name, "__builtin_unreachable") == 0 ||
        strcmp(function->ident_name, "__builtin_trap") == 0) {
        /* These are real terminating/undefined-path intrinsics, not
         * unresolved calls or empty stubs. */
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0x0B); /* UD2 */
        return true;
    }
    return false;
}

static void gen_cxx_reference_adjustment32_impl(Module* mod, Expr* expression,
                                                bool skip_static_cast) {
    if (!expression) return;
    /* Explicit reference casts apply their adjustment in gen_lvalue() (or in
     * gen_expr() after lowering the cast).  Initializer and call lowering also
     * invoke this helper for implicit bindings, so do not adjust that cast a
     * second time. */
    if (skip_static_cast && expression->kind == EXPR_CAST && expression->type &&
        expression->type->is_reference &&
        expression->cxx_cast_kind == CXX_CAST_STATIC) {
        return;
    }
    if (expression->cxx_virtual_base_adjustment) {
        int end_label;
        if (!expression->cxx_virtual_base_source_class ||
            expression->cxx_virtual_base_pointer_offset < 0 ||
            expression->cxx_virtual_base_index < 0 ||
            expression->cxx_virtual_base_index >=
                expression->cxx_virtual_base_source_class->virtual_base_count) {
            rcc_error(expression->loc,
                      "reference virtual-base conversion has incomplete vbtable metadata");
            return;
        }
        end_label = new_label();
        emit_test_reg_reg(mod, EAX, EAX);
        emit_jcc_label(mod, CC_E, end_label);
        emit_mov_reg_mem(mod, EDX, EAX,
                         expression->cxx_virtual_base_pointer_offset);
        emit_mov_reg_mem(mod, ECX, EDX,
                         expression->cxx_virtual_base_index * 4);
        emit_add_reg_reg(mod, EAX, ECX);
        if (expression->cxx_virtual_base_nested_adjustment != 0) {
            emit_add_reg_imm(mod, EAX,
                             expression->cxx_virtual_base_nested_adjustment);
        }
        emit_label(mod, end_label);
    } else if (expression->cxx_pointer_adjustment_valid &&
               !(expression->kind == EXPR_CALL && expression->type &&
                 expression->type->kind == TYPE_PTR &&
                 expression->type->is_reference &&
                 (!expression->call_method ||
                  !expression->call_method->field)) &&
               expression->cxx_pointer_adjustment != 0) {
        /* Ordinary reference-returning calls pass through gen_expr(), whose
         * common result path already applies this fixed adjustment.  The
         * field-backed inline-address path still needs it here. */
        emit_add_reg_imm(mod, EAX, expression->cxx_pointer_adjustment);
    }
}

static void gen_cxx_reference_adjustment32(Module* mod, Expr* expression) {
    gen_cxx_reference_adjustment32_impl(mod, expression, true);
}

static void gen_cxx_reference_adjustment32_force(Module* mod,
                                                 Expr* expression) {
    gen_cxx_reference_adjustment32_impl(mod, expression, false);
}

static bool gen_prepare_member_function_pointer_call32(Module* mod,
                                                       Expr* call) {
    Expr* application = call ? call->call_member_pointer_application : NULL;
    Expr* object;
    Expr* pointer;
    int done_label;
    if (!application) return false;
    if (call->call_member_pointer_value_offset >= 0 ||
        call->call_member_pointer_this_offset >= 0 ||
        !application->binary_lhs || !application->binary_rhs) {
        rcc_error(call->loc,
                  "member-function pointer call has incomplete ABI frame metadata");
        return true;
    }
    object = application->binary_lhs;
    pointer = application->binary_rhs;
    gen_expr(mod, object);
    if (application->cxx_virtual_base_member_access) {
        if (!application->cxx_virtual_base_source_class ||
            application->cxx_virtual_base_pointer_offset < 0 ||
            application->cxx_virtual_base_index < 0 ||
            application->cxx_virtual_base_index >=
                application->cxx_virtual_base_source_class
                    ->virtual_base_count) {
            rcc_error(application->loc,
                      "member-function pointer virtual-base receiver metadata is incomplete");
            return true;
        }
        emit_mov_reg_mem(mod, EDX, EAX,
                         application->cxx_virtual_base_pointer_offset);
        emit_mov_reg_mem(mod, ECX, EDX,
                         application->cxx_virtual_base_index * 4);
        emit_add_reg_reg(mod, EAX, ECX);
        if (application->cxx_virtual_base_nested_adjustment != 0) {
            emit_add_reg_imm(
                mod, EAX,
                application->cxx_virtual_base_nested_adjustment);
        }
    } else if (application->cxx_pointer_adjustment_valid &&
               application->cxx_pointer_adjustment != 0) {
        emit_add_reg_imm(mod, EAX,
                         application->cxx_pointer_adjustment);
    }
    emit_mov_mem_reg(mod, EBP,
                     call->call_member_pointer_this_offset, EAX);
    gen_expr(mod, pointer);
    emit_mov_reg_reg(mod, ECX, EAX);
    emit_mov_reg_mem(mod, EAX, ECX, 0);
    emit_mov_mem_reg(mod, EBP,
                     call->call_member_pointer_value_offset, EAX);
    emit_mov_reg_mem(mod, ECX, ECX, 4);
    emit_mov_mem_reg(mod, EBP,
                     call->call_member_pointer_value_offset + 4, ECX);
    emit_mov_reg_mem(mod, ECX, EBP,
                     call->call_member_pointer_value_offset);
    emit_mov_reg_mem(mod, EDX, EBP,
                     call->call_member_pointer_this_offset);
    emit_test_reg_reg(mod, ECX, ECX);
    done_label = new_label();
    emit_jcc_label(mod, CC_E, done_label);
    emit_mov_reg_mem(mod, ECX, EBP,
                     call->call_member_pointer_value_offset + 4);
    emit_add_reg_reg(mod, EDX, ECX);
    emit_label(mod, done_label);
    emit_mov_mem_reg(mod, EBP,
                     call->call_member_pointer_this_offset, EDX);
    return true;
}

static void gen_call(Module* mod, Expr* expr) {
    int argument_bytes = 0;
    int temporary_bytes = 0;
    int argc;
    ExprList** args;
    Type** argument_types;
    int* reference_temp_offsets;
    Type* function_type;
    TypeParam* parameter;
    int i;
    Expr* func_expr;
    bool complex_float_register_result =
        expr && type_is_complex(expr->type) &&
        expr->type->complex_component_type->kind == TYPE_FLOAT;

    if (expr->call_is_new) {
        gen_cxx_new32(mod, expr);
        return;
    }
    if (expr->call_is_delete && expr->call_delete_is_array &&
        (expr->call_delete_array_cleanup ||
         expr->call_delete_array_destructor ||
         expr->call_delete_object_type)) {
        gen_cxx_array_destructor32(mod, expr);
        return;
    }
    if (expr->call_is_delete &&
        (expr->call_delete_cleanup || expr->call_delete_destructor ||
         expr->call_delete_object_type)) {
        gen_cxx_delete32(mod, expr);
        return;
    }
    if (gen_inline_method_call(mod, expr)) return;
    if (gen_compiler_builtin(mod, expr)) return;
    if (gen_atomic_builtin(mod, expr)) return;
    if (expr->call_member_pointer_application) {
        (void)gen_prepare_member_function_pointer_call32(mod, expr);
    }

    argc = exprlist_len(expr->call_args);
    args = rcc_alloc((size_t)argc * sizeof(ExprList*));
    argument_types = rcc_alloc((size_t)argc * sizeof(Type*));
    reference_temp_offsets = rcc_alloc((size_t)argc * sizeof(int));
    function_type = expr->call_abi_function_type
        ? expr->call_abi_function_type
        : (expr->call_func ? expr->call_func->type : NULL);
    if (function_type && function_type->kind == TYPE_PTR) {
        function_type = function_type->base;
    }
    parameter = function_type && function_type->kind == TYPE_FUNC
        ? function_type->params : NULL;
    i = 0;
    for (ExprList* argument = expr->call_args; argument;
         argument = argument->next) {
        Type* argument_type = argument->expr ? argument->expr->type : NULL;
        bool variadic_nontrivial_class = !parameter && function_type &&
            function_type->variadic && argument_type &&
            argument_type->cxx_nontrivial &&
            (argument_type->kind == TYPE_STRUCT ||
             argument_type->kind == TYPE_UNION);
        args[i] = argument;
        argument_types[i] = parameter ? parameter->type
            : variadic_nontrivial_class ? type_ptr(argument_type)
            : codegen_default_argument_type(argument->expr->type);
        if (parameter) parameter = parameter->next;
        ++i;
    }
    for (i = 0; i < argc; ++i) {
        Expr* argument = args[i]->expr;
        Type* passed_type = argument_types[i];
        Type* value_type;
        int value_bytes;
        reference_temp_offsets[i] = -1;
        if (!gen_reference_argument_needs_temporary(passed_type, argument)) {
            continue;
        }
        value_type = passed_type->base;
        if (!value_type || gen_aggregate_type32(value_type)) continue;
        if (gen_is_floating(value_type)) {
            value_bytes = gen_float_width(value_type);
        } else if (gen_is_integer64(value_type)) {
            value_bytes = 8;
        } else {
            value_bytes = 4;
        }
        reference_temp_offsets[i] = temporary_bytes;
        temporary_bytes += value_bytes;
    }
    if (temporary_bytes > 0) {
        emit_sub_reg_imm(mod, ESP, temporary_bytes);
    }
    for (i = argc - 1; i >= 0; --i) {
        Expr* argument = args[i]->expr;
        Type* passed_type = argument_types[i];
        if (passed_type && passed_type->is_reference) {
            if (gen_reference_argument_needs_temporary(passed_type,
                                                       argument)) {
                Type* value_type = passed_type->base;
                int value_offset = reference_temp_offsets[i];
                if (!value_type) {
                    rcc_error(argument->loc,
                              "reference temporary has no referred-to type");
                    continue;
                }
                if (gen_aggregate_type32(value_type)) {
                    if (args[i]->cxx_temporary_owner) {
                        gen_expr(mod, argument);
                        emit_mov_mem_reg(
                            mod, EBP,
                            args[i]->cxx_temporary_owner->var_offset, EAX);
                    } else {
                        gen_lvalue(mod, argument);
                    }
                    gen_cxx_reference_adjustment32(mod, argument);
                    emit_push_reg(mod, EAX);
                    argument_bytes += 4;
                    codegen_push_call_temporary_cleanup32(
                        mod, args[i]->cxx_temporary_owner,
                        args[i]->cxx_temporary_cleanups);
                    continue;
                }
                if (gen_is_floating(value_type)) {
                    gen_expr_as_type(mod, argument, value_type);
                    if (gen_float_width(value_type) == 4) {
                        emit_mov_mem_reg(mod, ESP,
                                         argument_bytes + value_offset, EAX);
                    } else {
                        emit_mov_mem_reg(mod, ESP,
                                         argument_bytes + value_offset, EAX);
                        emit_mov_mem_reg(mod, ESP,
                                         argument_bytes + value_offset + 4,
                                         EDX);
                    }
                } else {
                    gen_expr_as_type(mod, argument, value_type);
                    if (gen_is_integer64(value_type)) {
                        if (!gen_is_integer64(argument->type)) {
                            emit_extend_eax_to_integer64(mod, argument->type);
                        }
                        emit_mov_mem_reg(mod, ESP,
                                         argument_bytes + value_offset, EAX);
                        emit_mov_mem_reg(mod, ESP,
                                         argument_bytes + value_offset + 4,
                                         EDX);
                    } else {
                        if (type_is_integer(value_type) ||
                            value_type->kind == TYPE_ENUM) {
                            emit_convert_integer_value(mod, EAX,
                                                       argument->type,
                                                       value_type);
                        }
                        emit_mov_mem_reg(mod, ESP,
                                         argument_bytes + value_offset, EAX);
                    }
                }
                /* Keep lifetime-extended values below the actual argument
                 * area.  A temporary inserted between two stack arguments
                 * would shift every following parameter by its size. */
                emit_mov_reg_reg(mod, EAX, ESP);
                if (argument_bytes + value_offset != 0) {
                    emit_add_reg_imm(mod, EAX,
                                     argument_bytes + value_offset);
                }
                emit_push_reg(mod, EAX);
                argument_bytes += 4;
                codegen_push_call_temporary_cleanup32(
                    mod, args[i]->cxx_temporary_owner,
                    args[i]->cxx_temporary_cleanups);
                continue;
            }
            gen_lvalue(mod, argument);
            gen_cxx_reference_adjustment32(mod, argument);
            emit_push_reg(mod, EAX);
            argument_bytes += 4;
            continue;
        }
        if (gen_cxx_indirect_parameter32(passed_type)) {
            gen_expr(mod, argument);
            if (args[i]->cxx_temporary_owner) {
                emit_mov_mem_reg(
                    mod, EBP,
                    args[i]->cxx_temporary_owner->var_offset, EAX);
            }
            emit_push_reg(mod, EAX);
            argument_bytes += 4;
            codegen_push_call_temporary_cleanup32(
                mod, args[i]->cxx_temporary_owner,
                args[i]->cxx_temporary_cleanups);
            continue;
        }
        if (passed_type && gen_aggregate_type32(passed_type)) {
            int units = (passed_type->size + 3) / 4;
            if (type_is_cxx_member_function_pointer(passed_type) &&
                (argument->is_cxx_nullptr ||
                 (argument->type &&
                  argument->type->kind == TYPE_NULLPTR))) {
                emit_mov_reg_imm(mod, EAX, 0u);
                for (int unit = 0; unit < units; ++unit) {
                    emit_push_reg(mod, EAX);
                }
                argument_bytes += units * 4;
                continue;
            }
            if (args[i]->cxx_temporary_owner &&
                argument->kind == EXPR_CAST &&
                argument->cxx_virtual_base_adjustment) {
                gen_expr(mod, argument->cast_expr);
                emit_mov_mem_reg(
                    mod, EBP,
                    args[i]->cxx_temporary_owner->var_offset, EAX);
                gen_cxx_reference_adjustment32(mod, argument);
            } else if (argument->kind == EXPR_VA_ARG ||
                       !gen_expr_is_lvalue(argument)) {
                gen_expr(mod, argument);
            } else {
                gen_lvalue(mod, argument);
            }
            if (args[i]->cxx_temporary_owner &&
                !(argument->kind == EXPR_CAST &&
                  argument->cxx_virtual_base_adjustment)) {
                int adjustment = argument->kind == EXPR_CAST &&
                        argument->cxx_pointer_adjustment_valid
                    ? argument->cxx_pointer_adjustment : 0;
                emit_mov_reg_reg(mod, ECX, EAX);
                if (adjustment > 0) {
                    emit_sub_reg_imm(mod, EAX, adjustment);
                } else if (adjustment < 0) {
                    emit_add_reg_imm(mod, EAX, -adjustment);
                }
                emit_mov_mem_reg(
                    mod, EBP,
                    args[i]->cxx_temporary_owner->var_offset, EAX);
                emit_mov_reg_reg(mod, EAX, ECX);
            }
            emit_mov_reg_reg(mod, ECX, EAX);
            for (int unit = units - 1; unit >= 0; --unit) {
                emit_mov_reg_mem(mod, EAX, ECX, unit * 4);
                emit_push_reg(mod, EAX);
            }
            argument_bytes += units * 4;
            codegen_push_call_temporary_cleanup32(
                mod, args[i]->cxx_temporary_owner,
                args[i]->cxx_temporary_cleanups);
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
        if (expr->call_member_pointer_application && i == 0) {
            emit_mov_reg_mem(
                mod, EAX, EBP,
                expr->call_member_pointer_this_offset);
        } else {
            gen_expr(mod, argument);
        }
        if (args[i]->cxx_temporary_owner && passed_type &&
            passed_type->kind == TYPE_PTR &&
            (passed_type->base == argument->type ||
             (expr->call_is_explicit_destructor && i == 0))) {
            emit_mov_mem_reg(
                mod, EBP,
                args[i]->cxx_temporary_owner->var_offset, EAX);
            if (expr->call_is_explicit_destructor && i == 0) {
                codegen_push_explicit_destructor_cleanup32(
                    mod, args[i]->cxx_temporary_owner,
                    args[i]->cxx_temporary_cleanups);
            } else {
                codegen_push_call_temporary_cleanup32(
                    mod, args[i]->cxx_temporary_owner,
                    args[i]->cxx_temporary_cleanups);
            }
            /* Cleanup registration may call into the exception runtime,
             * which is free to clobber EAX.  The pointer argument itself
             * must still be passed after that call. */
            emit_mov_reg_mem(
                mod, EAX, EBP,
                args[i]->cxx_temporary_owner->var_offset);
        }
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
    codegen_push_call_temporary_cleanup32(
        mod, NULL, expr->cxx_temporary_cleanups);
    if (expr->type && gen_aggregate_type32(expr->type) &&
        !complex_float_register_result) {
        if (expr == active_static_reference_result_call32 &&
            active_static_reference_result_symbol32) {
            if (active_static_reference_result_decl32) {
                gen_decl_storage_address32(
                    mod, active_static_reference_result_decl32);
            } else {
                gen_symbol_address(
                    mod, active_static_reference_result_symbol32, 0u);
            }
        } else {
            if (expr->call_result_offset >= 0) {
                rcc_error(expr->loc,
                          "aggregate call has no automatic result slot");
            }
            emit_byte(mod, 0x8D);  /* LEA EAX, [EBP+disp32] */
            emit_byte(mod, modrm(2, EAX, EBP));
            emit_dword(mod, (uint32_t)expr->call_result_offset);
        }
        emit_push_reg(mod, EAX);
        argument_bytes += 4;
    }
    gen_transfer_by_value_argument_cleanups32(mod, expr->call_args);
    rcc_free(argument_types);
    rcc_free(args);
    rcc_free(reference_temp_offsets);

    func_expr = expr->call_func;
    if (expr->call_member_pointer_application) {
        int direct_label = new_label();
        int done_label = new_label();
        emit_mov_reg_mem(mod, EAX, EBP,
                         expr->call_member_pointer_value_offset);
        emit_test_reg_imm(mod, EAX, 1u);
        emit_jcc_label(mod, CC_E, direct_label);
        emit_mov_reg_mem(mod, ECX, EBP,
                         expr->call_member_pointer_this_offset);
        emit_mov_reg_mem(mod, ECX, ECX, 0);
        emit_sub_reg_imm(mod, EAX, 1u);
        emit_add_reg_reg(mod, EAX, ECX);
        emit_mov_reg_mem(mod, EAX, EAX, 0);
        emit_byte(mod, 0xFF);
        emit_byte(mod, modrm(3, 2, EAX));
        emit_jmp_label(mod, done_label);
        emit_label(mod, direct_label);
        emit_byte(mod, 0xFF);
        emit_byte(mod, modrm(3, 2, EAX));
        emit_label(mod, done_label);
    } else if (expr->call_is_virtual && expr->call_virtual_index >= 0) {
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

    if (argument_bytes + temporary_bytes > 0) {
        emit_add_reg_imm(mod, ESP, argument_bytes + temporary_bytes);
    }
    if (complex_float_register_result) {
        if (expr->call_result_offset >= 0) {
            rcc_error(expr->loc,
                      "complex-float call has no automatic result slot");
            return;
        }
        emit_push_reg(mod, EAX);
        emit_push_reg(mod, EDX);
        if (expr == active_static_reference_result_call32 &&
            active_static_reference_result_symbol32) {
            if (active_static_reference_result_decl32) {
                gen_decl_storage_address32(
                    mod, active_static_reference_result_decl32);
            } else {
                gen_symbol_address(
                    mod, active_static_reference_result_symbol32, 0u);
            }
        } else {
            emit_byte(mod, 0x8D); /* LEA EAX, [EBP+disp32] */
            emit_byte(mod, modrm(2, EAX, EBP));
            emit_dword(mod, (uint32_t)expr->call_result_offset);
        }
        emit_mov_reg_reg(mod, ECX, EAX);
        emit_pop_reg(mod, EDX);
        emit_pop_reg(mod, EAX);
        emit_mov_mem_reg(mod, ECX, 0, EAX);
        emit_mov_mem_reg(mod, ECX, 4, EDX);
        emit_mov_reg_reg(mod, EAX, ECX);
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
            emit_mov_reg_imm(mod, EAX,
                expr->is_cxx_nullptr && expr->type &&
                expr->type->kind == TYPE_PTR &&
                expr->type->cxx_is_member_pointer
                    ? UINT32_MAX : (uint32_t)expr->int_val);
            break;

        case EXPR_NOEXCEPT:
            if (!expr->cxx_noexcept_value_valid) {
                rcc_error(expr->loc,
                          "noexcept expression has no semantic value");
                return;
            }
            emit_mov_reg_imm(mod, EAX,
                             expr->cxx_noexcept_value ? 1u : 0u);
            break;

        case EXPR_CXX_TYPEID:
            if (expr->cxx_typeid_dynamic) {
                gen_cxx_typeid32(mod, expr);
                break;
            }
            if (!expr->cxx_typeid_symbol) {
                rcc_error(expr->loc,
                          "typeid has no validated typeinfo identity");
                return;
            }
            codegen_emit_cxx_typeinfo_symbol(mod, expr->cxx_typeid_symbol);
            gen_symbol_address(mod, expr->cxx_typeid_symbol, 0u);
            break;

        case EXPR_CHAR_LIT:
            emit_mov_reg_imm(mod, EAX, expr_char_code_unit(expr));
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
            uint32_t offset = emit_string_encoded(
                mod, expr->str_val, expr->str_byte_length,
                expr->str_element_width);
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
            } else if (decl->type && decl->type->is_atomic) {
                gen_lvalue(mod, expr);
                emit_atomic_load_i686(mod, decl->type);
            } else if (decl->type && decl->type->is_reference) {
                gen_lvalue(mod, expr);
                if (expr->type && expr->type->kind != TYPE_ARRAY &&
                    expr->type->kind != TYPE_FUNC &&
                    expr->type->kind != TYPE_STRUCT &&
                    expr->type->kind != TYPE_UNION &&
                    expr->type->kind != TYPE_VECTOR &&
                    !type_is_cxx_member_function_pointer(expr->type)) {
                    if (gen_is_floating(expr->type)) {
                        emit_load_floating_raw(mod, expr->type, EAX, 0);
                    } else {
                        emit_load_typed32(mod, EAX, EAX, 0, expr->type);
                    }
                }
            } else if (decl->var_is_thread_local) {
                gen_lvalue(mod, expr);
                if (decl->type && decl->type->kind == TYPE_ARRAY) {
                    /* Array expressions decay to the address of element 0. */
                } else if (gen_aggregate_type32(decl->type)) {
                    /* Two-word member-function pointers are returned as an
                     * address by the expression path, like other aggregates. */
                } else if (gen_is_floating(decl->type)) {
                    emit_load_floating_raw(mod, decl->type, EAX, 0);
                } else {
                    emit_load_typed32(mod, EAX, EAX, 0, decl->type);
                }
            } else if (decl->type && decl->type->kind == TYPE_ARRAY) {
                gen_lvalue(mod, expr);
            } else if (decl->type && gen_aggregate_type32(decl->type)) {
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

        case EXPR_POS:
            gen_expr(mod, expr->unary_operand);
            break;

        case EXPR_NEG:
            if (type_is_complex(expr->type)) {
                gen_complex_negate32(mod, expr);
                break;
            }
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
            if (type_is_cxx_member_function_pointer(expr->type)) {
                TypeMethod* method =
                    expr->cxx_member_function_pointer_method;
                Decl* function = expr->unary_operand
                    ? expr->unary_operand->ident_decl : NULL;
                if (!method && expr->type->cxx_member_pointer_owner) {
                    for (TypeMethod* candidate =
                             expr->type->cxx_member_pointer_owner->methods;
                         candidate; candidate = candidate->next) {
                        if (candidate->function_decl == function) {
                            method = candidate;
                            break;
                        }
                    }
                }
                if (expr->aggregate_offset >= 0 || !method ||
                    (!method->is_virtual &&
                     (!function || !function->link_name)) ||
                    (method->is_virtual && method->vtable_index < 0)) {
                    rcc_error(expr->loc,
                              "member-function pointer address has no ABI result storage or selected method");
                    break;
                }
                if (method->is_virtual) {
                    emit_mov_reg_imm(
                        mod, EAX,
                        (uint32_t)method->vtable_index * 4u + 1u);
                } else {
                    gen_symbol_address(mod, decl_link_name(function), 0u);
                }
                emit_mov_mem_reg(mod, EBP, expr->aggregate_offset, EAX);
                emit_mov_reg_imm(mod, EDX, 0u);
                emit_mov_mem_reg(mod, EBP,
                                 expr->aggregate_offset + 4, EDX);
                emit_byte(mod, 0x8D); /* LEA EAX, [EBP+disp32] */
                emit_byte(mod, modrm(2, EAX, EBP));
                emit_dword(mod, (uint32_t)expr->aggregate_offset);
            } else {
                gen_lvalue(mod, expr->unary_operand);
            }
            break;

        case EXPR_DEREF:
            gen_expr(mod, expr->unary_operand);
            /* An aggregate lvalue evaluates to its address.  This is
             * especially important for a pointer-to-array dereference:
             * `(*row)[index]` must index the array object, not load its first
             * element before applying the index. */
            if (expr->type && (expr->type->kind == TYPE_ARRAY ||
                               expr->type->kind == TYPE_FUNC ||
                               expr->type->kind == TYPE_STRUCT ||
                               expr->type->kind == TYPE_UNION ||
                               expr->type->kind == TYPE_VECTOR ||
                               type_is_cxx_member_function_pointer(expr->type))) {
                break;
            }
            if (expr->type && expr->type->is_atomic) {
                emit_atomic_load_i686(mod, expr->type);
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
            if (expr->unary_operand && expr->unary_operand->type &&
                type_is_complex(expr->unary_operand->type) &&
                !expr->unary_operand->type->is_atomic) {
                gen_complex_increment32(mod, expr);
                break;
            }
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
            if (expr->unary_operand && expr->unary_operand->type &&
                expr->unary_operand->type->is_atomic) {
                Type* type = expr->unary_operand->type;
                uint32_t increment = codegen_increment_size(type);
                bool dynamic_increment = pointer_stride_is_dynamic(type);
                if (type->size == 8) {
                    rcc_error(expr->loc,
                              "i686 atomic 64-bit ++/-- requires a supported RMW lowering");
                    break;
                }
                if (!type_is_integer(type) && type->kind != TYPE_ENUM &&
                    type->kind != TYPE_PTR) {
                    rcc_error(expr->loc,
                              "atomic ++/-- requires an integer or pointer object");
                    break;
                }
                if (dynamic_increment) {
                    gen_vla_extent_for_expr(
                        mod, type->base, expr->unary_operand);
                    emit_push_reg(mod, EAX);
                    emit_push_reg(mod, EAX);
                    emit_push_reg(mod, EAX);
                    emit_push_reg(mod, EAX);
                }
                gen_lvalue(mod, expr->unary_operand);
                emit_mov_reg_reg(mod, ECX, EAX);
                if (dynamic_increment) {
                    emit_add_reg_imm(mod, ESP, 12);
                    emit_pop_reg(mod, EAX);
                } else {
                    emit_mov_reg_imm(mod, EAX, increment);
                }
                if (expr->kind == EXPR_PREDEC) emit_neg_reg(mod, EAX);
                emit_mov_reg_reg(mod, EDX, EAX);
                emit_atomic_xadd_width(mod, EAX, ECX, type);
                if (type_is_integer(type) || type->kind == TYPE_ENUM) {
                    emit_normalize_atomic_value(mod, EAX, type);
                }
                emit_add_reg_reg(mod, EAX, EDX);
                if (type_is_integer(type) || type->kind == TYPE_ENUM) {
                    emit_normalize_atomic_value(mod, EAX, type);
                }
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
            {
                bool dynamic_increment = pointer_stride_is_dynamic(expr->type);
                if (dynamic_increment) {
                    gen_vla_extent_for_expr(
                        mod, expr->type->base, expr->unary_operand);
                    emit_push_reg(mod, EAX);
                    emit_push_reg(mod, EAX);
                    emit_push_reg(mod, EAX);
                    emit_push_reg(mod, EAX);
                }
                gen_lvalue(mod, expr->unary_operand);
                emit_push_reg(mod, EAX);
                emit_load_typed32(mod, EAX, EAX, 0, expr->type);
                if (dynamic_increment) {
                    emit_mov_reg_mem(mod, ECX, ESP, 16);
                    if (expr->kind == EXPR_PREINC) {
                        emit_add_reg_reg(mod, EAX, ECX);
                    } else {
                        emit_sub_reg_reg(mod, EAX, ECX);
                    }
                } else if (expr->kind == EXPR_PREINC) {
                    emit_add_reg_imm(mod, EAX,
                                     (int)codegen_increment_size(expr->type));
                } else {
                    emit_sub_reg_imm(mod, EAX,
                                     (int)codegen_increment_size(expr->type));
                }
                emit_pop_reg(mod, ECX);
                emit_store_typed32(mod, ECX, 0, EAX, expr->type);
                if (dynamic_increment) emit_add_reg_imm(mod, ESP, 16);
            }
            break;

        case EXPR_POSTINC:
        case EXPR_POSTDEC:
            if (expr->unary_operand && expr->unary_operand->type &&
                type_is_complex(expr->unary_operand->type) &&
                !expr->unary_operand->type->is_atomic) {
                gen_complex_increment32(mod, expr);
                break;
            }
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
            if (expr->unary_operand && expr->unary_operand->type &&
                expr->unary_operand->type->is_atomic) {
                Type* type = expr->unary_operand->type;
                uint32_t increment = codegen_increment_size(type);
                bool dynamic_increment = pointer_stride_is_dynamic(type);
                if (type->size == 8) {
                    rcc_error(expr->loc,
                              "i686 atomic 64-bit ++/-- requires a supported RMW lowering");
                    break;
                }
                if (!type_is_integer(type) && type->kind != TYPE_ENUM &&
                    type->kind != TYPE_PTR) {
                    rcc_error(expr->loc,
                              "atomic ++/-- requires an integer or pointer object");
                    break;
                }
                if (dynamic_increment) {
                    gen_vla_extent_for_expr(
                        mod, type->base, expr->unary_operand);
                    emit_push_reg(mod, EAX);
                    emit_push_reg(mod, EAX);
                    emit_push_reg(mod, EAX);
                    emit_push_reg(mod, EAX);
                }
                gen_lvalue(mod, expr->unary_operand);
                emit_mov_reg_reg(mod, ECX, EAX);
                if (dynamic_increment) {
                    emit_add_reg_imm(mod, ESP, 12);
                    emit_pop_reg(mod, EAX);
                } else {
                    emit_mov_reg_imm(mod, EAX, increment);
                }
                if (expr->kind == EXPR_POSTDEC) emit_neg_reg(mod, EAX);
                emit_mov_reg_reg(mod, EDX, EAX);
                emit_atomic_xadd_width(mod, EAX, ECX, type);
                if (type_is_integer(type) || type->kind == TYPE_ENUM) {
                    emit_normalize_atomic_value(mod, EAX, type);
                }
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
            {
                bool dynamic_increment = pointer_stride_is_dynamic(expr->type);
                if (dynamic_increment) {
                    gen_vla_extent_for_expr(
                        mod, expr->type->base, expr->unary_operand);
                    emit_push_reg(mod, EAX);
                    emit_push_reg(mod, EAX);
                    emit_push_reg(mod, EAX);
                    emit_push_reg(mod, EAX);
                }
                gen_lvalue(mod, expr->unary_operand);
                emit_push_reg(mod, EAX);
                emit_load_typed32(mod, EAX, EAX, 0, expr->type);
                emit_mov_reg_reg(mod, EDX, EAX);
                if (dynamic_increment) {
                    emit_mov_reg_mem(mod, ECX, ESP, 16);
                    if (expr->kind == EXPR_POSTINC) {
                        emit_add_reg_reg(mod, EDX, ECX);
                    } else {
                        emit_sub_reg_reg(mod, EDX, ECX);
                    }
                } else if (expr->kind == EXPR_POSTINC) {
                    emit_add_reg_imm(mod, EDX,
                                     (int)codegen_increment_size(expr->type));
                } else {
                    emit_sub_reg_imm(mod, EDX,
                                     (int)codegen_increment_size(expr->type));
                }
                emit_pop_reg(mod, ECX);
                emit_store_typed32(mod, ECX, 0, EDX, expr->type);
                if (dynamic_increment) emit_add_reg_imm(mod, ESP, 16);
            }
            break;

        case EXPR_ADD:
            if (type_is_complex(expr->type)) {
                gen_complex_binary32(mod, expr);
                break;
            }
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
            {
                Expr* pointer_expr = NULL;
                Expr* index_expr = NULL;
                Type* pointer_type = NULL;
                bool pointer_on_left =
                    pointer_stride_is_dynamic(expr->binary_lhs->type) &&
                    type_is_integer(expr->binary_rhs->type);
                if (pointer_on_left) {
                    pointer_expr = expr->binary_lhs;
                    index_expr = expr->binary_rhs;
                    pointer_type = pointer_expr->type;
                } else if (type_is_integer(expr->binary_lhs->type) &&
                           pointer_stride_is_dynamic(
                               expr->binary_rhs->type)) {
                    pointer_expr = expr->binary_rhs;
                    index_expr = expr->binary_lhs;
                    pointer_type = pointer_expr->type;
                }
                if (pointer_expr) {
                    if (pointer_on_left) {
                        gen_expr(mod, pointer_expr);
                        emit_push_reg(mod, EAX);
                        emit_push_reg(mod, EAX);
                        gen_expr(mod, index_expr);
                        emit_push_reg(mod, EAX);
                        emit_push_reg(mod, EAX);
                    } else {
                        gen_expr(mod, index_expr);
                        emit_push_reg(mod, EAX);
                        emit_push_reg(mod, EAX);
                        gen_expr(mod, pointer_expr);
                        emit_push_reg(mod, EAX);
                        emit_push_reg(mod, EAX);
                    }
                    gen_vla_extent_for_expr(
                        mod, pointer_type->base, pointer_expr);
                    emit_mov_reg_reg(mod, EDX, EAX);
                    emit_mov_reg_mem(mod, ECX, ESP,
                                     pointer_on_left ? 4 : 12);
                    emit_imul_reg_reg(mod, ECX, EDX);
                    emit_mov_reg_mem(mod, EAX, ESP,
                                     pointer_on_left ? 12 : 4);
                    emit_add_reg_reg(mod, EAX, ECX);
                    emit_add_reg_imm(mod, ESP, 16);
                    break;
                }
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
            if (type_is_complex(expr->type)) {
                gen_complex_binary32(mod, expr);
                break;
            }
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
            if (pointer_stride_is_dynamic(expr->binary_lhs->type) &&
                pointer_stride_is_dynamic(expr->binary_rhs->type)) {
                gen_expr(mod, expr->binary_lhs);
                emit_push_reg(mod, EAX);
                gen_expr(mod, expr->binary_rhs);
                emit_mov_reg_reg(mod, ECX, EAX);
                emit_pop_reg(mod, EAX);
                emit_sub_reg_reg(mod, EAX, ECX);
                emit_push_reg(mod, EAX);
                gen_vla_extent_for_expr(
                    mod, expr->binary_lhs->type->base,
                    expr->binary_lhs);
                emit_mov_reg_reg(mod, ECX, EAX);
                emit_pop_reg(mod, EAX);
                emit_cdq(mod);
                emit_idiv_reg(mod, ECX);
                break;
            }
            if (pointer_stride_is_dynamic(expr->binary_lhs->type) &&
                type_is_integer(expr->binary_rhs->type)) {
                gen_expr(mod, expr->binary_lhs);
                emit_push_reg(mod, EAX);
                emit_push_reg(mod, EAX);
                gen_expr(mod, expr->binary_rhs);
                emit_push_reg(mod, EAX);
                emit_push_reg(mod, EAX);
                gen_vla_extent_for_expr(
                    mod, expr->binary_lhs->type->base,
                    expr->binary_lhs);
                emit_mov_reg_reg(mod, EDX, EAX);
                emit_mov_reg_mem(mod, ECX, ESP, 4);
                emit_imul_reg_reg(mod, ECX, EDX);
                emit_mov_reg_mem(mod, EAX, ESP, 12);
                emit_sub_reg_reg(mod, EAX, ECX);
                emit_add_reg_imm(mod, ESP, 16);
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
            if (type_is_complex(expr->type)) {
                gen_complex_binary32(mod, expr);
                break;
            }
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
            if (expr->kind == EXPR_DIV && type_is_complex(expr->type)) {
                gen_complex_binary32(mod, expr);
                break;
            }
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
            {
                uint8_t shift;
                if (gen_unsigned_power_of_two_divisor32(
                        expr->type, expr->binary_rhs, &shift)) {
                    gen_expr(mod, expr->binary_lhs);
                    if (expr->kind == EXPR_DIV) {
                        if (shift != 0u) emit_shr_reg_imm(mod, EAX, shift);
                    } else {
                        uint32_t mask = shift == 0u
                            ? 0u : (UINT32_C(1) << shift) - 1u;
                        emit_and_reg_imm(mod, EAX, mask);
                    }
                    break;
                }
            }
            {
                uint32_t divisor;
                uint32_t multiplier;
                uint8_t high_shift;
                if (gen_unsigned_magic_divisor32(
                        expr, &divisor, &multiplier, &high_shift)) {
                    gen_expr(mod, expr->binary_lhs);
                    if (divisor == 7u || expr->kind == EXPR_MOD) {
                        emit_push_reg(mod, EAX);
                    }
                    if (divisor == 7u && expr->kind == EXPR_MOD) {
                        emit_push_reg(mod, EAX);
                    }
                    emit_mov_reg_imm(mod, ECX, multiplier);
                    emit_mul_reg(mod, ECX);
                    if (divisor == 7u) {
                        /* For d=7, ceil(2^32/7) gives a high product one
                         * step below the exact quotient for some inputs.
                         * Correct it with ((x-q0)>>1)+q0, then shift twice. */
                        emit_pop_reg(mod, EAX);
                        emit_sub_reg_reg(mod, EAX, EDX);
                        emit_shr_reg_imm(mod, EAX, 1u);
                        emit_add_reg_reg(mod, EAX, EDX);
                        emit_shr_reg_imm(mod, EAX, 2u);
                        if (expr->kind == EXPR_MOD) {
                            emit_pop_reg(mod, ECX);
                            emit_mov_reg_reg(mod, EDX, EAX);
                            emit_shl_reg_imm(mod, EAX, 3u);
                            emit_sub_reg_reg(mod, EAX, EDX);
                            emit_sub_reg_reg(mod, ECX, EAX);
                            emit_mov_reg_reg(mod, EAX, ECX);
                        }
                    } else {
                        emit_shr_reg_imm(mod, EDX, high_shift);
                        if (expr->kind == EXPR_DIV) {
                            emit_mov_reg_reg(mod, EAX, EDX);
                        } else {
                            emit_mov_reg_reg(mod, ECX, EDX);
                            emit_shl_reg_imm(
                                mod, ECX, divisor == 3u ? 1u : 2u);
                            emit_add_reg_reg(mod, ECX, EDX);
                            emit_pop_reg(mod, EAX);
                            emit_sub_reg_reg(mod, EAX, ECX);
                        }
                    }
                    break;
                }
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
            {
                int64_t shift_amount = 0;
                int width = gen_type_width32(expr->type) * 8;
                if (expr_eval_integer_constant(expr->binary_rhs,
                                                &shift_amount) &&
                    shift_amount >= 0 && shift_amount < width) {
                    gen_expr(mod, expr->binary_lhs);
                    emit_shl_reg_imm(mod, EAX, (uint8_t)shift_amount);
                    break;
                }
            }
            gen_expr(mod, expr->binary_lhs);
            emit_push_reg(mod, EAX);
            gen_expr(mod, expr->binary_rhs);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_pop_reg(mod, EAX);
            emit_shl_reg_cl(mod, EAX);
            break;

        case EXPR_RSHIFT:
            {
                int64_t shift_amount = 0;
                int width = gen_type_width32(expr->type) * 8;
                if (expr_eval_integer_constant(expr->binary_rhs,
                                                &shift_amount) &&
                    shift_amount >= 0 && shift_amount < width) {
                    gen_expr(mod, expr->binary_lhs);
                    if (expr->type && expr->type->is_unsigned) {
                        emit_shr_reg_imm(mod, EAX, (uint8_t)shift_amount);
                    } else {
                        emit_sar_reg_imm(mod, EAX, (uint8_t)shift_amount);
                    }
                    break;
                }
            }
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

        case EXPR_SPACESHIP:
            if (gen_is_integer64(expr->binary_lhs->type) ||
                gen_is_integer64(expr->binary_rhs->type)) {
                gen_spaceship_integer64(mod, expr);
            } else {
                gen_spaceship_integer32(mod, expr);
            }
            break;

        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_GT:
        case EXPR_LE:
        case EXPR_GE: {
            if (type_is_cxx_member_function_pointer(
                    expr->binary_lhs->type) ||
                type_is_cxx_member_function_pointer(
                    expr->binary_rhs->type)) {
                emit_push_reg(mod, ESI);
                emit_push_reg(mod, EDI);
                emit_sub_reg_imm(mod, ESP, 8);
                emit_mov_reg_imm(mod, EAX, 0u);
                emit_mov_mem_reg(mod, ESP, 0, EAX);
                emit_mov_mem_reg(mod, ESP, 4, EAX);
                if (expr->binary_lhs->is_cxx_nullptr ||
                    (expr->binary_lhs->type &&
                     expr->binary_lhs->type->kind == TYPE_NULLPTR)) {
                    emit_mov_reg_reg(mod, EAX, ESP);
                } else if (expr->binary_lhs->kind == EXPR_CALL) {
                    gen_lvalue(mod, expr->binary_lhs);
                } else {
                    gen_expr(mod, expr->binary_lhs);
                }
                emit_mov_reg_reg(mod, ESI, EAX);
                if (expr->binary_rhs->is_cxx_nullptr ||
                    (expr->binary_rhs->type &&
                     expr->binary_rhs->type->kind == TYPE_NULLPTR)) {
                    emit_mov_reg_reg(mod, EAX, ESP);
                } else if (expr->binary_rhs->kind == EXPR_CALL) {
                    gen_lvalue(mod, expr->binary_rhs);
                } else {
                    gen_expr(mod, expr->binary_rhs);
                }
                emit_mov_reg_reg(mod, EDI, EAX);
                emit_mov_reg_mem(mod, EAX, ESI, 0);
                emit_mov_reg_mem(mod, ECX, EDI, 0);
                emit_xor_reg_reg(mod, EAX, ECX);
                emit_mov_reg_mem(mod, EDX, ESI, 4);
                emit_mov_reg_mem(mod, ECX, EDI, 4);
                emit_xor_reg_reg(mod, EDX, ECX);
                emit_or_reg_reg(mod, EAX, EDX);
                emit_test_reg_reg(mod, EAX, EAX);
                emit_setcc(mod, expr->kind == EXPR_EQ ? CC_E : CC_NE, EAX);
                emit_byte(mod, 0x0F);
                emit_byte(mod, 0xB6);
                emit_byte(mod, modrm(3, EAX, EAX));
                emit_add_reg_imm(mod, ESP, 8);
                emit_pop_reg(mod, EDI);
                emit_pop_reg(mod, ESI);
                break;
            }
            if (type_is_complex(expr->binary_lhs->type) ||
                type_is_complex(expr->binary_rhs->type)) {
                gen_complex_compare32(mod, expr);
                break;
            }
            if (gen_is_floating(expr->binary_lhs->type) ||
                gen_is_floating(expr->binary_rhs->type)) {
                Type* comparison_type = codegen_comparison_type(expr);
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

        case EXPR_ASSIGN: {
            Type* lhs_type = expr->binary_lhs
                ? expr->binary_lhs->type : NULL;
            if (lhs_type && lhs_type->is_reference) {
                lhs_type = lhs_type->base;
            }
            if (gen_cxx_move_assignment(mod, expr)) break;
            if (expr->binary_lhs &&
                (expr->binary_lhs->kind == EXPR_MEMBER ||
                 expr->binary_lhs->kind == EXPR_PTR_MEMBER) &&
                expr->binary_lhs->member_field &&
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
            if (lhs_type && lhs_type->is_atomic) {
                Type* type = lhs_type;
                if (type->size == 8) {
                    gen_expr_as_type(mod, expr->binary_rhs, type);
                    emit_push_reg(mod, EDX);
                    emit_push_reg(mod, EAX);
                    gen_lvalue(mod, expr->binary_lhs);
                    emit_atomic_store64_i686(mod);
                    emit_mov_reg_mem(mod, EAX, ESP, 0);
                    emit_mov_reg_mem(mod, EDX, ESP, 4);
                    emit_add_reg_imm(mod, ESP, 8);
                } else {
                    gen_expr_as_type(mod, expr->binary_rhs, type);
                    if (type_is_integer(type) || type->kind == TYPE_ENUM) {
                        emit_normalize_atomic_value(mod, EAX, type);
                    }
                    emit_push_reg(mod, EAX);
                    gen_lvalue(mod, expr->binary_lhs);
                    emit_mov_reg_reg(mod, EDX, EAX);
                    emit_pop_reg(mod, ECX);
                    if (type_is_integer(type) || type->kind == TYPE_ENUM) {
                        emit_normalize_atomic_value(mod, ECX, type);
                    }
                    emit_mov_reg_reg(mod, EAX, ECX);
                    emit_atomic_exchange_width(mod, EAX, EDX, type);
                    emit_mov_reg_reg(mod, EAX, ECX);
                }
                break;
            }
            if (gen_is_floating(lhs_type)) {
                Type* type = lhs_type;
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
            {
                Type* aggregate_type = lhs_type;
                if (aggregate_type &&
                    (aggregate_type->kind == TYPE_STRUCT ||
                     aggregate_type->kind == TYPE_UNION)) {
                    int offset = 0;
                    if (expr->binary_rhs->kind == EXPR_ASSIGN ||
                        expr->binary_rhs->kind == EXPR_VA_ARG ||
                        (!gen_expr_is_lvalue(expr->binary_rhs) &&
                         !gen_expr_is_xvalue(expr->binary_rhs))) {
                        gen_expr(mod, expr->binary_rhs);
                    } else {
                        gen_lvalue(mod, expr->binary_rhs);
                    }
                    emit_push_reg(mod, EAX);
                    gen_lvalue(mod, expr->binary_lhs);
                    emit_mov_reg_reg(mod, EDX, EAX);
                    emit_pop_reg(mod, ECX);
                    while (offset + 4 <= aggregate_type->size) {
                        emit_mov_reg_mem(mod, EAX, ECX, offset);
                        emit_mov_mem_reg(mod, EDX, offset, EAX);
                        offset += 4;
                    }
                    while (offset < aggregate_type->size) {
                        emit_load_typed32(mod, EAX, ECX, offset,
                                          type_uchar);
                        emit_store_typed32(mod, EDX, offset, EAX,
                                           type_uchar);
                        ++offset;
                    }
                    emit_mov_reg_reg(mod, EAX, EDX);
                    break;
                }
            }
            gen_expr(mod, expr->binary_rhs);
            if (type_is_integer(lhs_type) ||
                (lhs_type && lhs_type->kind == TYPE_ENUM)) {
                emit_convert_integer_value(mod, EAX,
                                           expr->binary_rhs->type,
                                           lhs_type);
            }
            emit_push_reg(mod, EAX);
            gen_lvalue(mod, expr->binary_lhs);
            emit_pop_reg(mod, ECX);
            if (type_is_integer(lhs_type) ||
                (lhs_type && lhs_type->kind == TYPE_ENUM)) {
                emit_normalize_atomic_value(mod, ECX, lhs_type);
            }
            emit_store_typed32(mod, EAX, 0, ECX,
                               lhs_type);
            emit_mov_reg_reg(mod, EAX, ECX);
            break;
        }

        case EXPR_ADD_ASSIGN:
        case EXPR_SUB_ASSIGN: {
            if (expr->binary_lhs && expr->binary_lhs->type &&
                expr->binary_lhs->type->is_atomic) {
                Type* type = expr->binary_lhs->type;
                uint32_t scale = codegen_pointer_element_size(type);
                if (type->size == 8) {
                    rcc_error(expr->loc,
                              "i686 atomic 64-bit +=/-= requires a supported RMW lowering");
                    break;
                }
                if (!type_is_integer(type) && type->kind != TYPE_ENUM &&
                    type->kind != TYPE_PTR) {
                    rcc_error(expr->loc,
                              "atomic +=/-= requires an integer or pointer object");
                    break;
                }
                gen_expr(mod, expr->binary_rhs);
                if (pointer_stride_is_dynamic(type)) {
                    emit_push_reg(mod, EAX);
                    emit_push_reg(mod, EAX);
                    gen_vla_extent_for_expr(
                        mod, type->base, expr->binary_lhs);
                    emit_mov_reg_reg(mod, ECX, EAX);
                    emit_mov_reg_mem(mod, EAX, ESP, 4);
                    emit_imul_reg_reg(mod, EAX, ECX);
                    emit_add_reg_imm(mod, ESP, 8);
                } else {
                    emit_scale_reg(mod, EAX, scale);
                }
                if (expr->kind == EXPR_SUB_ASSIGN) emit_neg_reg(mod, EAX);
                emit_push_reg(mod, EAX);
                gen_lvalue(mod, expr->binary_lhs);
                emit_mov_reg_reg(mod, ECX, EAX);
                emit_pop_reg(mod, EDX);
                emit_mov_reg_reg(mod, EAX, EDX);
                emit_atomic_xadd_width(mod, EAX, ECX, type);
                if (type_is_integer(type) || type->kind == TYPE_ENUM) {
                    emit_normalize_atomic_value(mod, EAX, type);
                }
                emit_add_reg_reg(mod, EAX, EDX);
                if (type_is_integer(type) || type->kind == TYPE_ENUM) {
                    emit_normalize_atomic_value(mod, EAX, type);
                }
                break;
            }
            if (expr->binary_lhs && expr->binary_lhs->type &&
                type_is_complex(expr->binary_lhs->type)) {
                gen_complex_compound32(mod, expr);
                break;
            }
            if (expr->binary_lhs &&
                (expr->binary_lhs->kind == EXPR_MEMBER ||
                 expr->binary_lhs->kind == EXPR_PTR_MEMBER) &&
                expr->binary_lhs->member_field &&
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
            bool dynamic_scale = pointer_stride_is_dynamic(
                expr->binary_lhs->type);
            gen_lvalue(mod, expr->binary_lhs);
            emit_push_reg(mod, EAX);
            emit_load_typed32(mod, EAX, EAX, 0,
                              expr->binary_lhs->type);
            emit_push_reg(mod, EAX);
            gen_expr(mod, expr->binary_rhs);
            if (dynamic_scale) {
                emit_push_reg(mod, EAX);
                emit_push_reg(mod, EAX);
                gen_vla_extent_for_expr(
                    mod, expr->binary_lhs->type->base,
                    expr->binary_lhs);
                emit_mov_reg_reg(mod, ECX, EAX);
                emit_mov_reg_mem(mod, EAX, ESP, 4);
                emit_imul_reg_reg(mod, EAX, ECX);
                emit_add_reg_imm(mod, ESP, 8);
            } else {
                emit_scale_reg(mod, EAX, scale);
            }
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
            Type* left_type = codegen_promoted_expression_type(
                expr->binary_lhs);
            Type* right_type = codegen_promoted_expression_type(
                expr->binary_rhs);
            Type* operation_type =
                expr->kind == EXPR_LSHIFT_ASSIGN ||
                        expr->kind == EXPR_RSHIFT_ASSIGN
                    ? left_type : type_common(left_type, right_type);
            if (expr->binary_lhs && expr->binary_lhs->type &&
                expr->binary_lhs->type->is_atomic) {
                if (gen_atomic_bitwise_assignment(mod, expr) ||
                    gen_atomic_arithmetic_assignment(mod, expr)) break;
                rcc_error(expr->loc,
                          "atomic compound assignment operator is not supported by the target RMW backend");
                break;
            }
            if ((expr->kind == EXPR_MUL_ASSIGN ||
                 expr->kind == EXPR_DIV_ASSIGN) &&
                expr->binary_lhs && expr->binary_lhs->type &&
                type_is_complex(expr->binary_lhs->type)) {
                gen_complex_compound32(mod, expr);
                break;
            }
            if (expr->binary_lhs &&
                (expr->binary_lhs->kind == EXPR_MEMBER ||
                 expr->binary_lhs->kind == EXPR_PTR_MEMBER) &&
                expr->binary_lhs->member_field &&
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
                expr->binary_lhs->type &&
                expr->binary_lhs->type->is_unsigned &&
                expr->binary_lhs->type->size == 4) {
                uint8_t shift;
                if (gen_unsigned_power_of_two_divisor32(
                        operation_type, expr->binary_rhs, &shift)) {
                    gen_lvalue(mod, expr->binary_lhs);
                    emit_push_reg(mod, EAX);
                    emit_load_typed32(mod, EAX, EAX, 0,
                                      expr->binary_lhs->type);
                    if (expr->kind == EXPR_DIV_ASSIGN) {
                        if (shift != 0u) emit_shr_reg_imm(mod, EAX, shift);
                    } else {
                        uint32_t mask = shift == 0u
                            ? 0u : (UINT32_C(1) << shift) - 1u;
                        emit_and_reg_imm(mod, EAX, mask);
                    }
                    emit_normalize_atomic_value(mod, EAX,
                                                expr->binary_lhs->type);
                    emit_pop_reg(mod, ECX);
                    emit_store_typed32(mod, ECX, 0, EAX,
                                       expr->binary_lhs->type);
                    break;
                }
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
            if (type_is_complex(expr->type)) {
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
            if (gen_aggregate_type32(expr->type)) {
                gen_lvalue(mod, expr);
                break;
            }
            int else_label = new_label();
            int end_label = new_label();
            gen_expr(mod, expr->cond_test);
            emit_test_scalar_value(mod, expr->cond_test->type);
            emit_jcc_label(mod, CC_E, else_label);
            gen_expr_as_type(mod, expr->cond_then, expr->type);
            emit_jmp_label(mod, end_label);
            emit_label(mod, else_label);
            gen_expr_as_type(mod, expr->cond_else, expr->type);
            emit_label(mod, end_label);
            break;
        }

        case EXPR_CALL: {
            if (expr->cxx_typeinfo_hash_code) {
                gen_expr(mod, expr->call_func->member_base);
                emit_mov_reg_mem(mod, EAX, EAX, 0);
                break;
            }
            if (expr->cxx_typeinfo_name) {
                gen_expr(mod, expr->call_func->member_base);
                emit_mov_reg_mem(mod, EAX, EAX, 4);
                break;
            }
            if (expr->cxx_typeinfo_before) {
                gen_expr(mod, expr->call_func->member_base);
                emit_push_reg(mod, EAX);
                gen_expr(mod, expr->call_args->expr);
                emit_mov_reg_reg(mod, ECX, EAX);
                emit_pop_reg(mod, EAX);
                emit_cmp_reg_reg(mod, EAX, ECX);
                emit_setcc(mod, CC_B, EAX);
                emit_byte(mod, 0x0F);
                emit_byte(mod, 0xB6);
                emit_byte(mod, modrm(3, EAX, EAX));
                break;
            }
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
                if (expr->va_arg_type->cxx_nontrivial) {
                    /* Non-trivial C++ ellipsis arguments are pointers to
                     * temporaries under the Itanium ABI. */
                    gen_lvalue(mod, expr->va_list_operand);
                    emit_mov_reg_reg(mod, ECX, EAX);
                    emit_mov_reg_mem(mod, EAX, ECX, 0);
                    emit_mov_reg_reg(mod, EDX, EAX);
                    emit_add_reg_imm(mod, EAX, 4);
                    emit_mov_mem_reg(mod, ECX, 0, EAX);
                    emit_mov_reg_mem(mod, EAX, EDX, 0);
                    break;
                }
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
        case EXPR_PTR_MEMBER: {
            Type* value_type = expr->type;
            if (expr->member_field && expr->member_field->type &&
                expr->member_field->type->is_reference) {
                value_type = expr->member_field->type->base;
            }
            gen_lvalue(mod, expr);
            if (expr->member_field && expr->member_field->is_bitfield) {
                gen_bitfield_load32(mod, expr->member_field);
            } else if (!value_type || value_type->kind != TYPE_ARRAY) {
                if (gen_is_floating(value_type)) {
                    emit_load_floating_raw(mod, value_type, EAX, 0);
                } else {
                    emit_load_typed32(mod, EAX, EAX, 0, value_type);
                }
            }
            break;
        }

        case EXPR_CXX_MEMBER_PTR_DOT:
        case EXPR_CXX_MEMBER_PTR_ARROW:
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

        case EXPR_COMPOUND:
            gen_lvalue(mod, expr);
            if (!expr->type || (expr->type->kind != TYPE_ARRAY &&
                                expr->type->kind != TYPE_STRUCT &&
                                expr->type->kind != TYPE_UNION &&
                                expr->type->kind != TYPE_VECTOR &&
                                !type_is_cxx_member_function_pointer(
                                    expr->type))) {
                if (gen_is_floating(expr->type)) {
                    emit_load_floating_raw(mod, expr->type, EAX, 0);
                } else {
                    emit_load_typed32(mod, EAX, EAX, 0, expr->type);
                }
            }
            break;

        case EXPR_CAST:
            /* A cast type-name may contain a variably modified pointer type.
             * Its size expressions are evaluated as part of the cast
             * expression even though the resulting pointer value does not
             * depend on the computed extent. */
            if (expr->type && codegen_type_has_vla_any(expr->type)) {
                int slot_index = 0;
                gen_typeof_runtime_evaluations(mod, expr->type);
                gen_vla_extents(mod, expr->type, NULL, NULL, NULL,
                                &slot_index);
            }
            if (type_is_complex(expr->type)) {
                if (expr->call_result_offset >= 0) {
                    rcc_error(expr->loc,
                              "complex cast has no automatic result storage");
                    break;
                }
                gen_complex_component_from_operand32(
                    mod, expr->cast_expr,
                    expr->type->complex_component_type,
                    expr->call_result_offset);
                emit_byte(mod, 0x8D); /* LEA EAX, [EBP+disp32] */
                emit_byte(mod, modrm(2, EAX, EBP));
                emit_dword(mod, (uint32_t)expr->call_result_offset);
            } else if (expr->type && expr->type->is_reference &&
                (expr->cxx_cast_kind == CXX_CAST_NONE ||
                 (expr->cxx_cast_kind == CXX_CAST_STATIC &&
                  expr->type->base &&
                  (expr->type->base->kind == TYPE_STRUCT ||
                   expr->type->base->kind == TYPE_UNION)) ||
                 expr->cxx_cast_kind == CXX_CAST_CONST ||
                 expr->cxx_cast_kind == CXX_CAST_DYNAMIC)) {
                /* Reference expressions use the pointer ABI.  Loading the
                 * first word of an aggregate here would pass its vptr as an
                 * object address to RTTI instead of the lvalue address. */
                gen_lvalue(mod, expr->cast_expr);
            } else if (gen_is_floating(expr->type) ||
                gen_is_floating(expr->cast_expr->type) ||
                type_is_complex(expr->cast_expr->type)) {
                gen_expr_as_type(mod, expr->cast_expr, expr->type);
            } else {
                gen_expr(mod, expr->cast_expr);
            }
            if (!gen_is_floating(expr->type) &&
                (type_is_integer(expr->type) ||
                 expr->type->kind == TYPE_ENUM) &&
                !gen_is_floating(expr->cast_expr->type) &&
                !type_is_complex(expr->cast_expr->type)) {
                emit_convert_integer_value(mod, EAX,
                                           expr->cast_expr->type,
                                           expr->type);
            }
            break;

        case EXPR_SIZEOF: {
            Type* measured_type = type_remove_reference(
                expr->sizeof_type ? expr->sizeof_type
                    : (expr->unary_operand
                        ? expr->unary_operand->type : NULL));
            if (expr->sizeof_type) {
                gen_typeof_runtime_evaluations(mod, measured_type);
            }
            if (expr->sizeof_type && codegen_type_has_vla(measured_type)) {
                if (measured_type->c_typeof_operand_type &&
                    measured_type->c_typeof_runtime_expression) {
                    gen_vla_extent_for_expr(
                        mod, measured_type->c_typeof_operand_type,
                        measured_type->c_typeof_runtime_expression);
                } else {
                    gen_vla_extent(mod, measured_type);
                }
            } else if (expr->unary_operand &&
                       expr->unary_operand->kind == EXPR_IDENT &&
                       expr->unary_operand->ident_decl &&
                       expr->unary_operand->ident_decl->var_is_vla) {
                emit_mov_reg_mem(mod, EAX, EBP,
                                 expr->unary_operand->ident_decl->var_vla_size_offset);
            } else if (expr->unary_operand &&
                       codegen_type_has_vla(measured_type)) {
                gen_vla_extent_for_expr(mod, measured_type,
                                        expr->unary_operand);
            } else if (expr->sizeof_type) {
                emit_mov_reg_imm(mod, EAX, measured_type->size);
            } else if (expr->unary_operand && expr->unary_operand->type) {
                emit_mov_reg_imm(mod, EAX, measured_type->size);
            } else {
                emit_mov_reg_imm(mod, EAX, 4);
            }
            break;
        }

        case EXPR_ALIGNOF: {
            Type* measured_type = type_remove_reference(expr->sizeof_type);
            emit_mov_reg_imm(mod, EAX,
                             measured_type ? measured_type->align : 1);
            break;
        }

        case EXPR_COMMA:
            if (type_is_complex(expr->type)) {
                gen_expr(mod, expr->binary_lhs);
                gen_expr(mod, expr->binary_rhs);
                break;
            }
            if (gen_aggregate_type32(expr->type)) {
                gen_lvalue(mod, expr);
                break;
            }
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
        case 'r':
        case 'X': return ECX; /* Bounded generic input register */
        case 'g': return ECX; /* Select the register alternative of GCC g. */
        case 'm': return -1;  /* Memory */
        case 'i':
        case 'n': return -3;  /* Integer constant */
        default: return -1;
    }
}

static int asm_named_register(const char* name) {
    if (!name) return -1;
    if (strcmp(name, "eax") == 0 || strcmp(name, "ax") == 0 ||
        strcmp(name, "al") == 0 || strcmp(name, "ah") == 0) return EAX;
    if (strcmp(name, "ebx") == 0 || strcmp(name, "bx") == 0 ||
        strcmp(name, "bl") == 0 || strcmp(name, "bh") == 0) return EBX;
    if (strcmp(name, "ecx") == 0 || strcmp(name, "cx") == 0 ||
        strcmp(name, "cl") == 0 || strcmp(name, "ch") == 0) return ECX;
    if (strcmp(name, "edx") == 0 || strcmp(name, "dx") == 0 ||
        strcmp(name, "dl") == 0 || strcmp(name, "dh") == 0) return EDX;
    if (strcmp(name, "esi") == 0 || strcmp(name, "si") == 0) return ESI;
    if (strcmp(name, "edi") == 0 || strcmp(name, "di") == 0) return EDI;
    return -1;
}

static const char* asm_constraint_name(const char* constraint) {
    if (!constraint) return NULL;
    while (*constraint == '=' || *constraint == '+' || *constraint == '&') {
        ++constraint;
    }
    return constraint;
}

static bool asm_constraint_is_port(const char* constraint) {
    const char* name = asm_constraint_name(constraint);
    return name && strcmp(name, "Nd") == 0;
}

static int constraint_to_reg_for_operand(const char* constraint,
                                         Expr* expression) {
    int64_t value;
    const char* name = asm_constraint_name(constraint);
    if (expression && expression->kind == EXPR_IDENT &&
        expression->ident_decl && expression->ident_decl->kind == DECL_VAR &&
        expression->ident_decl->var_register_name && name &&
        (strcmp(name, "r") == 0 || strcmp(name, "X") == 0 ||
         strcmp(name, "g") == 0)) {
        return asm_named_register(
            expression->ident_decl->var_register_name);
    }
    if (!asm_constraint_is_port(constraint)) {
        return constraint_to_reg(constraint);
    }
    if (expression && expr_eval_integer_constant(expression, &value) &&
        value >= 0 && value <= 255) {
        return -3;  /* The Nd alternative is an 8-bit port immediate. */
    }
    return EDX;      /* Non-constant and out-of-range ports use %dx. */
}

static int asm_operand_register(const AsmOperand* operand) {
    if (!operand) return -1;
    switch (operand->assigned_gpr) {
        case 0: return EAX;
        case 1: return EBX;
        case 2: return ECX;
        case 3: return EDX;
        case 4: return ESI;
        case 5: return EDI;
        case -2: return -1;
        default:
            return constraint_to_reg_for_operand(operand->constraint,
                                                 operand->expr);
    }
}

static bool asm_memory_frame_offset(const AsmOperand* operand,
                                   int* offset_out) {
    Decl* declaration;
    if (!operand || !operand->expr || !offset_out ||
        operand->expr->kind != EXPR_IDENT) return false;
    declaration = operand->expr->ident_decl;
    if (!declaration ||
        (declaration->kind != DECL_PARAM &&
         (declaration->kind != DECL_VAR ||
          declaration->var_is_global || declaration->var_is_static_local ||
          declaration->var_is_thread_local)) ||
        (declaration->type && declaration->type->is_reference) ||
        declaration->var_register_name) return false;
    *offset_out = declaration->var_offset;
    return true;
}

static bool asm_memory_constraint(const AsmOperand* operand) {
    return operand && strcmp(asm_constraint_name(operand->constraint),
                             "m") == 0;
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

static bool asm_parse_immediate_width(const char* text, int width,
                                      uint64_t* value) {
    char* end;
    unsigned long long parsed;
    unsigned long long maximum;
    long long minimum;
    if (!text || text[0] != '$' || !text[1] || !value) return false;
    switch (width) {
        case 1: maximum = UINT8_MAX; minimum = INT8_MIN; break;
        case 2: maximum = UINT16_MAX; minimum = INT16_MIN; break;
        case 4: maximum = UINT32_MAX; minimum = INT32_MIN; break;
        case 8: maximum = ULLONG_MAX; minimum = LLONG_MIN; break;
        default: return false;
    }
    errno = 0;
    if (text[1] == '-') {
        long long signed_value = strtoll(text + 1, &end, 0);
        if (errno == ERANGE || end == text + 1 || *end != '\0' ||
            signed_value < minimum) return false;
        *value = (uint64_t)signed_value;
        return true;
    }
    parsed = strtoull(text + 1, &end, 0);
    if (errno == ERANGE || end == text + 1 || *end != '\0' ||
        parsed > maximum) return false;
    *value = (uint64_t)parsed;
    return true;
}

static bool asm_no_operands(const char* op1, const char* op2) {
    return !op1 && !op2;
}

static bool asm_parse_register_width(const char* text, int* reg, int* width) {
    if (!text || !reg || !width) return false;
    if (*text == '%') ++text;
    if (strcmp(text, "al") == 0) { *reg = EAX; *width = 1; return true; }
    if (strcmp(text, "cl") == 0) { *reg = ECX; *width = 1; return true; }
    if (strcmp(text, "dl") == 0) { *reg = EDX; *width = 1; return true; }
    if (strcmp(text, "bl") == 0) { *reg = EBX; *width = 1; return true; }
    if (strcmp(text, "ax") == 0) { *reg = EAX; *width = 2; return true; }
    if (strcmp(text, "cx") == 0) { *reg = ECX; *width = 2; return true; }
    if (strcmp(text, "dx") == 0) { *reg = EDX; *width = 2; return true; }
    if (strcmp(text, "bx") == 0) { *reg = EBX; *width = 2; return true; }
    if (strcmp(text, "si") == 0) { *reg = ESI; *width = 2; return true; }
    if (strcmp(text, "di") == 0) { *reg = EDI; *width = 2; return true; }
    if (strcmp(text, "eax") == 0) { *reg = EAX; *width = 4; return true; }
    if (strcmp(text, "ecx") == 0) { *reg = ECX; *width = 4; return true; }
    if (strcmp(text, "edx") == 0) { *reg = EDX; *width = 4; return true; }
    if (strcmp(text, "ebx") == 0) { *reg = EBX; *width = 4; return true; }
    if (strcmp(text, "esi") == 0) { *reg = ESI; *width = 4; return true; }
    if (strcmp(text, "edi") == 0) { *reg = EDI; *width = 4; return true; }
    if (strcmp(text, "ebp") == 0) { *reg = EBP; *width = 4; return true; }
    if (strcmp(text, "esp") == 0) { *reg = ESP; *width = 4; return true; }
    return false;
}

static bool asm_parse_x86_memory(const char* text, int* base_register,
                                int32_t* displacement) {
    const char* open;
    const char* close;
    char displacement_text[32];
    char register_text[8];
    size_t displacement_length;
    size_t register_length;
    char* end;
    long parsed;
    int width;
    if (!text || !base_register || !displacement) return false;
    open = strchr(text, '(');
    close = strrchr(text, ')');
    if (!open || !close || close[1] != '\0' || close <= open + 1) {
        return false;
    }
    displacement_length = (size_t)(open - text);
    register_length = (size_t)(close - open - 1);
    if (displacement_length >= sizeof(displacement_text) ||
        register_length >= sizeof(register_text)) return false;
    memcpy(displacement_text, text, displacement_length);
    displacement_text[displacement_length] = '\0';
    memcpy(register_text, open + 1, register_length);
    register_text[register_length] = '\0';
    if (!asm_parse_register_width(register_text, base_register, &width) ||
        width != 4) return false;
    if (!displacement_length) {
        parsed = 0;
    } else {
        errno = 0;
        parsed = strtol(displacement_text, &end, 0);
        if (errno == ERANGE || end == displacement_text || *end != '\0' ||
            parsed < INT32_MIN || parsed > INT32_MAX) return false;
    }
    *displacement = (int32_t)parsed;
    return true;
}

static void asm_emit_x86_memory_modrm(Module* mod, int reg_field, int base,
                                      int32_t displacement) {
    int mode;
    int rm = base;
    if (displacement == 0 && base != EBP) {
        mode = 0;
    } else if (displacement >= INT8_MIN && displacement <= INT8_MAX) {
        mode = 1;
    } else {
        mode = 2;
    }
    if (base == ESP) rm = 4;
    emit_byte(mod, modrm(mode, reg_field, rm));
    if (base == ESP) emit_byte(mod, 0x24); /* no index, ESP base */
    if (mode == 1) emit_byte(mod, (uint8_t)(int8_t)displacement);
    else if (mode == 2) emit_dword(mod, (uint32_t)displacement);
}

static bool asm_emit_x86_memory_group(Module* mod, const char* operand1,
                                      const char* operand2,
                                      uint8_t opcode_first,
                                      int opcode_second,
                                      int group_extension) {
    int base;
    int32_t displacement;
    if (!operand1 || operand2 ||
        !asm_parse_x86_memory(operand1, &base, &displacement)) {
        return false;
    }
    emit_byte(mod, opcode_first);
    if (opcode_second >= 0) emit_byte(mod, (uint8_t)opcode_second);
    asm_emit_x86_memory_modrm(mod, group_extension, base, displacement);
    return true;
}

static const char* asm_register_name(int reg) {
    switch (reg) {
        case EAX: return "%eax";
        case ECX: return "%ecx";
        case EDX: return "%edx";
        case EBX: return "%ebx";
        case ESI: return "%esi";
        case EDI: return "%edi";
        default: return NULL;
    }
}

static const char* asm_register_name_width(int reg, char modifier) {
    if (!modifier) return asm_register_name(reg);
    if (modifier == 'k') return asm_register_name(reg);
    if (modifier == 'w') {
        switch (reg) {
            case EAX: return "%ax";
            case ECX: return "%cx";
            case EDX: return "%dx";
            case EBX: return "%bx";
            case ESI: return "%si";
            case EDI: return "%di";
            default: return NULL;
        }
    }
    if (modifier == 'b') {
        switch (reg) {
            case EAX: return "%al";
            case ECX: return "%cl";
            case EDX: return "%dl";
            case EBX: return "%bl";
            default: return NULL;
        }
    }
    return NULL;
}

static bool asm_parse_io_register(const char* text, int* reg, int* width) {
    if (!text || !reg || !width) return false;
    if (*text == '%') ++text;
    if (strcmp(text, "al") == 0) { *reg = EAX; *width = 1; return true; }
    if (strcmp(text, "ax") == 0) { *reg = EAX; *width = 2; return true; }
    if (strcmp(text, "eax") == 0) { *reg = EAX; *width = 4; return true; }
    if (strcmp(text, "dl") == 0) { *reg = EDX; *width = 1; return true; }
    if (strcmp(text, "dx") == 0) { *reg = EDX; *width = 2; return true; }
    if (strcmp(text, "edx") == 0) { *reg = EDX; *width = 4; return true; }
    return false;
}

static bool asm_parse_port(const char* text, bool* immediate, uint8_t* value) {
    char* end;
    long long parsed;
    int reg;
    int width;
    if (!text || !immediate || !value) return false;
    if (text[0] == '$' && text[1]) {
        parsed = strtoll(text + 1, &end, 0);
        if (end == text + 1 || *end != '\0' || parsed < 0 || parsed > 255) {
            return false;
        }
        *immediate = true;
        *value = (uint8_t)parsed;
        return true;
    }
    if (!asm_parse_io_register(text, &reg, &width) || reg != EDX ||
        width != 2) return false;
    *immediate = false;
    *value = 0;
    return true;
}

static int asm_format_immediate(char* buffer, size_t capacity,
                                int64_t value, char modifier)
{
    if (modifier == 'n') {
        if (value > 0) {
            return snprintf(buffer, capacity, "%lld", (long long)-value);
        }
        if (value < 0) {
            uint64_t magnitude = (uint64_t)(-(value + 1)) + UINT64_C(1);
            return snprintf(buffer, capacity, "%llu",
                            (unsigned long long)magnitude);
        }
        return snprintf(buffer, capacity, "0");
    }
    return snprintf(buffer, capacity, modifier == 'c' ? "%lld" : "$%lld",
                    (long long)value);
}

static char* asm_expand_template(const char* source, const int* registers,
                                  const int* memory_offsets,
                                  const int* memory_address_registers,
                                  const int64_t* immediate_values,
                                  const bool* immediate_flags,
                                  int operand_count, SourceLoc loc) {
    size_t source_length;
    size_t capacity;
    char* expanded;
    size_t read = 0u;
    size_t write = 0u;

    if (!source) source = "";
    source_length = strlen(source);
    if (operand_count < 0 ||
        (size_t)operand_count >
            (SIZE_MAX - source_length - 1u) / 24u) {
        rcc_fatal("i686 inline asm template is too large");
        return NULL;
    }
    capacity = source_length + (size_t)operand_count * 24u + 1u;
    expanded = rcc_alloc(capacity);
    while (source[read]) {
        const char* name;
        if (source[read] != '%') {
            expanded[write++] = source[read++];
            continue;
        }
        if (source[read + 1u] == '%') {
            expanded[write++] = '%';
            read += 2u;
            continue;
        }
        char modifier = 0;
        size_t index_start = read + 1u;
        if (source[index_start] == 'b' || source[index_start] == 'w' ||
            source[index_start] == 'k' || source[index_start] == 'c' ||
            source[index_start] == 'n') {
            modifier = source[index_start++];
        }
        if (source[index_start] < '0' || source[index_start] > '9') {
            rcc_error(loc, "inline asm placeholder must be %% or %%N");
            rcc_free(expanded);
            return NULL;
        }
        {
            uint64_t index = 0u;
            size_t digit = index_start;
            while (source[digit] >= '0' && source[digit] <= '9') {
                if (index > (UINT64_MAX - 9u) / 10u) {
                    index = UINT64_MAX;
                    break;
                }
                index = index * 10u + (uint64_t)(source[digit] - '0');
                ++digit;
            }
            if (index >= (uint64_t)operand_count) {
                rcc_error(loc,
                          "i686 inline asm operand placeholder index is out of range");
                rcc_free(expanded);
                return NULL;
            }
            if ((memory_offsets &&
                 memory_offsets[(size_t)index] != INT_MIN) ||
                (memory_address_registers &&
                 memory_address_registers[(size_t)index] >= 0)) {
                char memory[32];
                int written;
                const char* register_name = NULL;
                if (modifier) {
                    rcc_error(loc,
                              "i686 inline asm memory operands do not accept register-width modifiers");
                    rcc_free(expanded);
                    return NULL;
                }
                if (memory_offsets &&
                    memory_offsets[(size_t)index] != INT_MIN) {
                    written = snprintf(memory, sizeof(memory), "%d(%%ebp)",
                                       memory_offsets[(size_t)index]);
                } else {
                    register_name = asm_register_name_width(
                        memory_address_registers[(size_t)index], 0);
                    if (!register_name) {
                        rcc_error(loc,
                                  "i686 inline asm memory operand has no address register");
                        rcc_free(expanded);
                        return NULL;
                    }
                    written = snprintf(memory, sizeof(memory), "(%s)",
                                       register_name);
                }
                if (written < 0 || (size_t)written >= sizeof(memory)) {
                    rcc_error(loc,
                              "i686 inline asm frame-relative memory operand is too large");
                    rcc_free(expanded);
                    return NULL;
                }
                for (int character = 0; character < written; ++character) {
                    expanded[write++] = memory[character];
                }
                read = digit;
                continue;
            }
            if (immediate_flags && immediate_flags[(size_t)index]) {
                char immediate[32];
                int written = asm_format_immediate(
                    immediate, sizeof(immediate), immediate_values[index],
                    modifier);
                if (written < 0 || (size_t)written >= sizeof(immediate)) {
                    rcc_error(loc,
                              "i686 inline asm immediate operand is too large");
                    rcc_free(expanded);
                    return NULL;
                }
                for (int character = 0; character < written; ++character) {
                    expanded[write++] = immediate[character];
                }
                read = digit;
                continue;
            }
            if (modifier == 'c' || modifier == 'n') {
                rcc_error(loc, modifier == 'c'
                    ? "inline asm %%cN modifier requires a constant operand"
                    : "inline asm %%nN modifier requires a constant operand");
                rcc_free(expanded);
                return NULL;
            }
            if (!registers || !(name = asm_register_name_width(
                    registers[(size_t)index], modifier))) {
                rcc_error(loc,
                          "i686 inline asm placeholder width is not available for this register");
                rcc_free(expanded);
                return NULL;
            }
            while (*name) expanded[write++] = *name++;
            read = digit;
        }
    }
    expanded[write] = '\0';
    return expanded;
}

/* Encode a single x86 instruction from mnemonic and operands */
static bool asm_parse_control_register(const char* text, int* control) {
    if (!text || !control) return false;
    if (*text == '%') ++text;
    if (text[0] != 'c' || text[1] != 'r' ||
        text[2] < '0' || text[2] > '4' || text[3] != '\0') return false;
    *control = text[2] - '0';
    return true;
}

static bool emit_asm_instruction(Module* mod, const char* mnemonic,
                                  const char* op1, const char* op2,
                                  bool lock_prefix, bool repeat_prefix) {
    uint8_t immediate;

    if (lock_prefix) {
        if (strcmp(mnemonic, "orl") != 0 ||
            !op1 || !op2 || strcmp(op1, "$0") != 0 ||
            strcmp(op2, "(%esp)") != 0) return false;
        emit_byte(mod, 0xF0);
        emit_byte(mod, 0x83);
        emit_byte(mod, 0x0C);
        emit_byte(mod, 0x24);
        emit_byte(mod, 0x00);
        return true;
    }

    if (repeat_prefix) {
        if (!asm_no_operands(op1, op2)) return false;
        if (strcmp(mnemonic, "movsl") == 0) {
            emit_byte(mod, 0xF3); /* REP */
            emit_byte(mod, 0xA5); /* MOVS m32 */
            return true;
        }
        if (strcmp(mnemonic, "stosl") == 0) {
            emit_byte(mod, 0xF3); /* REP */
            emit_byte(mod, 0xAB); /* STOS m32 */
            return true;
        }
        return false;
    }

    /* Normalize AT&T's i386 32-bit suffixes to the validated operand
     * encoders below. */
    if (strcmp(mnemonic, "movl") == 0) mnemonic = "mov";
    else if (strcmp(mnemonic, "leal") == 0) mnemonic = "lea";
    else if (strcmp(mnemonic, "addl") == 0) mnemonic = "add";
    else if (strcmp(mnemonic, "subl") == 0) mnemonic = "sub";
    else if (strcmp(mnemonic, "andl") == 0) mnemonic = "and";
    else if (strcmp(mnemonic, "orl") == 0) mnemonic = "or";
    else if (strcmp(mnemonic, "xorl") == 0) mnemonic = "xor";
    else if (strcmp(mnemonic, "testl") == 0) mnemonic = "test";

    if ((strcmp(mnemonic, "fnstenv") == 0 &&
         asm_emit_x86_memory_group(mod, op1, op2, 0xD9u, -1, 6)) ||
        (strcmp(mnemonic, "fldenv") == 0 &&
         asm_emit_x86_memory_group(mod, op1, op2, 0xD9u, -1, 4)) ||
        (strcmp(mnemonic, "fnstcw") == 0 &&
         asm_emit_x86_memory_group(mod, op1, op2, 0xD9u, -1, 7)) ||
        (strcmp(mnemonic, "fldcw") == 0 &&
         asm_emit_x86_memory_group(mod, op1, op2, 0xD9u, -1, 5)) ||
        (strcmp(mnemonic, "fnstsw") == 0 &&
         asm_emit_x86_memory_group(mod, op1, op2, 0xDDu, -1, 7)) ||
        (strcmp(mnemonic, "stmxcsr") == 0 &&
         asm_emit_x86_memory_group(mod, op1, op2, 0x0Fu, 0xAE, 3)) ||
        (strcmp(mnemonic, "ldmxcsr") == 0 &&
         asm_emit_x86_memory_group(mod, op1, op2, 0x0Fu, 0xAE, 2))) {
        return true;
    }

    if (strcmp(mnemonic, "inb") == 0 || strcmp(mnemonic, "inw") == 0 ||
        strcmp(mnemonic, "inl") == 0) {
        bool port_immediate;
        uint8_t port;
        int destination;
        int width;
        int expected_width = strcmp(mnemonic, "inb") == 0 ? 1
            : (strcmp(mnemonic, "inw") == 0 ? 2 : 4);
        if (!op1 || !op2 || !asm_parse_port(op1, &port_immediate, &port) ||
            !asm_parse_io_register(op2, &destination, &width) ||
            destination != EAX || width != expected_width) return false;
        if (width == 2) emit_byte(mod, 0x66);
        emit_byte(mod, port_immediate ? (width == 1 ? 0xE4 : 0xE5)
                                      : (width == 1 ? 0xEC : 0xED));
        if (port_immediate) emit_byte(mod, port);
        return true;
    }
    if (strcmp(mnemonic, "outb") == 0 || strcmp(mnemonic, "outw") == 0 ||
        strcmp(mnemonic, "outl") == 0) {
        bool port_immediate;
        uint8_t port;
        int source;
        int width;
        int expected_width = strcmp(mnemonic, "outb") == 0 ? 1
            : (strcmp(mnemonic, "outw") == 0 ? 2 : 4);
        if (!op1 || !op2 || !asm_parse_io_register(op1, &source, &width) ||
            source != EAX || width != expected_width ||
            !asm_parse_port(op2, &port_immediate, &port)) return false;
        if (width == 2) emit_byte(mod, 0x66);
        emit_byte(mod, port_immediate ? (width == 1 ? 0xE6 : 0xE7)
                                      : (width == 1 ? 0xEE : 0xEF));
        if (port_immediate) emit_byte(mod, port);
        return true;
    }

    if (strcmp(mnemonic, "mov") == 0) {
        int source;
        int destination;
        int source_control;
        int destination_control;
        int source_width;
        int destination_width;
        uint64_t immediate_value;
        if (asm_parse_control_register(op1, &source_control) &&
            asm_parse_register_width(op2, &destination, &destination_width)) {
            if (destination_width != 4) return false;
            emit_byte(mod, 0x0F);
            emit_byte(mod, 0x20);
            emit_byte(mod, modrm(3, source_control, destination));
            return true;
        }
        if (asm_parse_register_width(op1, &source, &source_width) &&
            asm_parse_control_register(op2, &destination_control)) {
            if (source_width != 4) return false;
            emit_byte(mod, 0x0F);
            emit_byte(mod, 0x22);
            emit_byte(mod, modrm(3, destination_control, source));
            return true;
        }
        {
            int base;
            int32_t displacement;
            if (g_opts.target_arch == ARCH_X86 &&
                asm_parse_x86_memory(op1, &base, &displacement) &&
                asm_parse_register_width(op2, &destination,
                                         &destination_width) &&
                destination_width == 4) {
                emit_byte(mod, 0x8B); /* mov r32, r/m32 */
                asm_emit_x86_memory_modrm(mod, destination, base,
                                          displacement);
                return true;
            }
            if (g_opts.target_arch == ARCH_X86 &&
                asm_parse_register_width(op1, &source, &source_width) &&
                source_width == 4 &&
                asm_parse_x86_memory(op2, &base, &displacement)) {
                emit_byte(mod, 0x89); /* mov r/m32, r32 */
                asm_emit_x86_memory_modrm(mod, source, base, displacement);
                return true;
            }
            if (g_opts.target_arch == ARCH_X86 &&
                asm_parse_immediate_width(op1, 4, &immediate_value) &&
                asm_parse_x86_memory(op2, &base, &displacement)) {
                emit_byte(mod, 0xC7); /* mov r/m32, imm32 */
                asm_emit_x86_memory_modrm(mod, 0, base, displacement);
                emit_dword(mod, (uint32_t)immediate_value);
                return true;
            }
        }
        if (asm_parse_register_width(op2, &destination,
                                     &destination_width) &&
            asm_parse_immediate_width(op1, destination_width,
                                      &immediate_value)) {
            if (destination_width == 1) {
                emit_byte(mod, (uint8_t)(0xB0 + destination));
                emit_byte(mod, (uint8_t)immediate_value);
            } else {
                if (destination_width == 2) emit_byte(mod, 0x66);
                emit_byte(mod, (uint8_t)(0xB8 + destination));
                if (destination_width == 2) {
                    emit_word(mod, (uint16_t)immediate_value);
                } else {
                    emit_dword(mod, (uint32_t)immediate_value);
                }
            }
            return true;
        }
        if (!asm_parse_register_width(op1, &source, &source_width) ||
            !asm_parse_register_width(op2, &destination,
                                       &destination_width) ||
            source_width != destination_width) return false;
        if (source_width == 1) {
            emit_byte(mod, 0x88);
            emit_byte(mod, modrm(3, source, destination));
        } else {
            if (source_width == 2) emit_byte(mod, 0x66);
            emit_byte(mod, 0x89);
            emit_byte(mod, modrm(3, source, destination));
        }
        return true;
    }
    if (strcmp(mnemonic, "lea") == 0) {
        int base;
        int destination;
        int destination_width;
        int32_t displacement;
        if (g_opts.target_arch != ARCH_X86 ||
            !asm_parse_x86_memory(op1, &base, &displacement) || !op2 ||
            !asm_parse_register_width(op2, &destination,
                                      &destination_width) ||
            destination_width != 4) return false;
        emit_byte(mod, 0x8D);
        asm_emit_x86_memory_modrm(mod, destination, base, displacement);
        return true;
    }
    if (g_opts.target_arch == ARCH_X86 &&
        (strcmp(mnemonic, "add") == 0 || strcmp(mnemonic, "sub") == 0 ||
         strcmp(mnemonic, "and") == 0 || strcmp(mnemonic, "or") == 0)) {
        int extension = strcmp(mnemonic, "add") == 0 ? 0
            : (strcmp(mnemonic, "or") == 0 ? 1
               : (strcmp(mnemonic, "and") == 0 ? 4 : 5));
        uint8_t register_opcode = strcmp(mnemonic, "add") == 0 ? 0x01
            : (strcmp(mnemonic, "or") == 0 ? 0x09
               : (strcmp(mnemonic, "and") == 0 ? 0x21 : 0x29));
        int destination;
        int destination_width;
        int source;
        int source_width;
        int base;
        int32_t displacement;
        uint64_t immediate_value;
        if (asm_parse_immediate_width(op1, 4, &immediate_value)) {
            if (asm_parse_register_width(op2, &destination,
                                         &destination_width) &&
                destination_width == 4) {
                emit_byte(mod, 0x81); /* ALU r/m32, imm32 */
                emit_byte(mod, modrm(3, extension, destination));
                emit_dword(mod, (uint32_t)immediate_value);
                return true;
            }
            if (asm_parse_x86_memory(op2, &base, &displacement)) {
                emit_byte(mod, 0x81); /* ALU r/m32, imm32 */
                asm_emit_x86_memory_modrm(mod, extension, base,
                                          displacement);
                emit_dword(mod, (uint32_t)immediate_value);
                return true;
            }
            return false;
        }
        if (!asm_parse_register_width(op1, &source, &source_width) ||
            source_width != 4) return false;
        if (asm_parse_register_width(op2, &destination,
                                     &destination_width) &&
            destination_width == 4) {
            emit_byte(mod, register_opcode);
            emit_byte(mod, modrm(3, source, destination));
            return true;
        }
        if (asm_parse_x86_memory(op2, &base, &displacement)) {
            emit_byte(mod, register_opcode);
            asm_emit_x86_memory_modrm(mod, source, base, displacement);
            return true;
        }
        return false;
    }
    if (strcmp(mnemonic, "xor") == 0 || strcmp(mnemonic, "test") == 0) {
        int source;
        int destination;
        int source_width;
        int destination_width;
        if (!asm_parse_register_width(op1, &source, &source_width) ||
            !asm_parse_register_width(op2, &destination,
                                      &destination_width) ||
            source_width != 4 || destination_width != 4) return false;
        emit_byte(mod, strcmp(mnemonic, "xor") == 0 ? 0x31 : 0x85);
        emit_byte(mod, modrm(3, source, destination));
        return true;
    }
    if (g_opts.target_arch == ARCH_X86 &&
        (strcmp(mnemonic, "fxsave") == 0 ||
         strcmp(mnemonic, "fxrstor") == 0)) {
        int base;
        int32_t displacement;
        if (op2 || !asm_parse_x86_memory(op1, &base, &displacement)) {
            return false;
        }
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xAE);
        asm_emit_x86_memory_modrm(mod,
                                  strcmp(mnemonic, "fxsave") == 0 ? 0 : 1,
                                  base, displacement);
        return true;
    }
    if (g_opts.target_arch == ARCH_X86 && strcmp(mnemonic, "call") == 0) {
        int destination;
        int width;
        if (op2 || !op1) return false;
        if (op1[0] == '*' &&
            asm_parse_register_width(op1 + 1, &destination, &width) &&
            width == 4) {
            emit_byte(mod, 0xFF);
            emit_byte(mod, modrm(3, 2, destination)); /* call r/m32 */
            return true;
        }
        if (asm_parse_register_width(op1, &destination, &width)) return false;
        for (const char* cursor = op1; *cursor; ++cursor) {
            if (!((*cursor >= 'a' && *cursor <= 'z') ||
                  (*cursor >= 'A' && *cursor <= 'Z') ||
                  (*cursor >= '0' && *cursor <= '9') ||
                  *cursor == '_' || *cursor == '.' || *cursor == '$')) {
                return false;
            }
        }
        if (!((*op1 >= 'a' && *op1 <= 'z') ||
              (*op1 >= 'A' && *op1 <= 'Z') || *op1 == '_' ||
              *op1 == '.' || *op1 == '$')) return false;
        emit_byte(mod, 0xE8);
        uint32_t displacement_offset = (uint32_t)code_offset(mod);
        emit_dword(mod, 0);
        add_func_call_ref(rcc_intern(op1), displacement_offset);
        return true;
    }
    if (g_opts.target_arch == ARCH_X86 &&
        strcmp(mnemonic, "jmp") == 0 && op1 && op1[0] == '*' && !op2) {
        int destination;
        int destination_width;
        if (!asm_parse_register_width(op1 + 1, &destination,
                                      &destination_width) ||
            destination_width != 4) return false;
        emit_byte(mod, 0xFF);
        emit_byte(mod, modrm(3, 4, destination)); /* jmp r/m32 */
        return true;
    }
    if (g_opts.target_arch == ARCH_X86 &&
        (strcmp(mnemonic, "pushl") == 0 ||
         strcmp(mnemonic, "popl") == 0) && op1 && !op2) {
        int reg;
        int width;
        if (!asm_parse_register_width(op1, &reg, &width) || width != 4) {
            return false;
        }
        emit_byte(mod, (uint8_t)((strcmp(mnemonic, "pushl") == 0
            ? 0x50 : 0x58) + reg));
        return true;
    }

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
    else if (strcmp(mnemonic, "ud2") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0x0B);
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
    else if (strcmp(mnemonic, "rdtscp") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0x01);
        emit_byte(mod, 0xF9);
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
    else if (strcmp(mnemonic, "emms") == 0) {
        if (!asm_no_operands(op1, op2)) return false;
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0x77);
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

typedef struct AsmNumericLabel {
    unsigned number;
    int codegen_label;
    size_t offset;
} AsmNumericLabel;

static bool asm_parse_numeric_label_line(char* line, unsigned* number) {
    char* cursor = line;
    unsigned parsed = 0;
    bool has_digit = false;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r') cursor++;
    while (*cursor >= '0' && *cursor <= '9') {
        unsigned digit = (unsigned)(*cursor - '0');
        if (parsed > (UINT_MAX - digit) / 10u) return false;
        parsed = parsed * 10u + digit;
        has_digit = true;
        cursor++;
    }
    if (!has_digit || *cursor++ != ':') return false;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r') cursor++;
    if (*cursor) return false;
    *number = parsed;
    return true;
}

static bool asm_parse_numeric_label_reference(const char* operand,
                                               unsigned* number,
                                               bool* forward) {
    const char* cursor = operand;
    unsigned parsed = 0;
    bool has_digit = false;
    if (!cursor) return false;
    while (*cursor >= '0' && *cursor <= '9') {
        unsigned digit = (unsigned)(*cursor - '0');
        if (parsed > (UINT_MAX - digit) / 10u) return false;
        parsed = parsed * 10u + digit;
        has_digit = true;
        cursor++;
    }
    if (!has_digit || (cursor[0] != 'f' && cursor[0] != 'b') || cursor[1]) {
        return false;
    }
    *number = parsed;
    *forward = cursor[0] == 'f';
    return true;
}

static size_t asm_count_numeric_labels(char* source) {
    size_t count = 0;
    char* cursor = source;
    while (*cursor) {
        char* line = cursor;
        char saved;
        unsigned number;
        while (*cursor && *cursor != ';' && *cursor != '\n') cursor++;
        saved = *cursor;
        *cursor = '\0';
        if (asm_parse_numeric_label_line(line, &number)) count++;
        *cursor = saved;
        if (saved) cursor++;
    }
    return count;
}

static void asm_collect_numeric_labels(char* source, AsmNumericLabel* labels) {
    char* cursor = source;
    size_t count = 0;
    while (*cursor) {
        char* line = cursor;
        char* trimmed = line;
        char saved;
        unsigned number;
        while (*cursor && *cursor != ';' && *cursor != '\n') cursor++;
        saved = *cursor;
        *cursor = '\0';
        while (*trimmed == ' ' || *trimmed == '\t' || *trimmed == '\r') {
            trimmed++;
        }
        if (asm_parse_numeric_label_line(line, &number)) {
            labels[count].number = number;
            labels[count].codegen_label = new_label();
            labels[count].offset = (size_t)(trimmed - source);
            count++;
        }
        *cursor = saved;
        if (saved) cursor++;
    }
}

static int asm_find_numeric_label_at(const AsmNumericLabel* labels,
                                     size_t count, size_t offset) {
    for (size_t index = 0; index < count; ++index) {
        if (labels[index].offset == offset) return labels[index].codegen_label;
    }
    return -1;
}

static int asm_find_numeric_label_reference(const AsmNumericLabel* labels,
                                            size_t count, unsigned number,
                                            bool forward,
                                            size_t source_offset) {
    int found = -1;
    size_t best_offset = forward ? SIZE_MAX : 0;
    for (size_t index = 0; index < count; ++index) {
        if (labels[index].number != number) continue;
        if (forward) {
            if (labels[index].offset > source_offset &&
                labels[index].offset < best_offset) {
                best_offset = labels[index].offset;
                found = labels[index].codegen_label;
            }
        } else if (labels[index].offset < source_offset &&
                   (found < 0 || labels[index].offset > best_offset)) {
            best_offset = labels[index].offset;
            found = labels[index].codegen_label;
        }
    }
    return found;
}

/* Parse and emit inline assembly */
static void gen_asm_stmt(Module* mod, Stmt* stmt) {
    const char* tmpl = stmt->asm_template;
    AsmOperand* outputs = stmt->asm_outputs;
    AsmOperand* inputs = stmt->asm_inputs;
    AsmClobber* clobbers = stmt->asm_clobbers;
    AsmOperand** memory_address_operands = NULL;
    int memory_address_count = 0;

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
    int* operand_registers = NULL;
    int* memory_offsets = NULL;
    int* memory_address_registers = NULL;
    int64_t* immediate_values = NULL;
    bool* immediate_flags = NULL;
    if (total_operands > 0) {
        operands = rcc_alloc(total_operands * sizeof(OperandInfo));

        int idx = 0;
        /* Process outputs first */
        for (AsmOperand* op = outputs; op; op = op->next) {
            operands[idx].reg = asm_operand_register(op);
            operands[idx].op = op;
            operands[idx].stack_offset = 0;
            idx++;
        }
        /* Then inputs */
        for (AsmOperand* op = inputs; op; op = op->next) {
            operands[idx].reg = asm_operand_register(op);
            operands[idx].op = op;
            operands[idx].stack_offset = 0;
            idx++;
        }
        operand_registers = rcc_alloc((size_t)total_operands *
                                      sizeof(*operand_registers));
        memory_offsets = rcc_alloc((size_t)total_operands *
                                   sizeof(*memory_offsets));
        memory_address_registers = rcc_alloc(
            (size_t)total_operands * sizeof(*memory_address_registers));
        memory_address_operands = rcc_alloc(
            (size_t)total_operands * sizeof(*memory_address_operands));
        immediate_values = rcc_alloc((size_t)total_operands *
                                     sizeof(*immediate_values));
        immediate_flags = rcc_alloc((size_t)total_operands *
                                    sizeof(*immediate_flags));
        for (int index = 0; index < total_operands; ++index) {
            operand_registers[index] = operands[index].reg;
            memory_offsets[index] = INT_MIN;
            memory_address_registers[index] = -1;
            immediate_flags[index] = false;
            if (strcmp(asm_constraint_name(
                          operands[index].op->constraint), "m") == 0) {
                if (asm_memory_frame_offset(operands[index].op,
                                            &memory_offsets[index])) {
                    operands[index].stack_offset =
                        memory_offsets[index];
                } else {
                    int address_register = asm_operand_register(
                        operands[index].op);
                    if (address_register < 0) {
                        rcc_error(operands[index].op->expr->loc,
                                  "i686 inline asm memory operand has no address register");
                        rcc_free(immediate_flags);
                        rcc_free(immediate_values);
                        rcc_free(memory_address_operands);
                        rcc_free(memory_address_registers);
                        rcc_free(memory_offsets);
                        rcc_free(operand_registers);
                        rcc_free(operands);
                        return;
                    }
                    memory_address_registers[index] = address_register;
                    memory_address_operands[memory_address_count++] =
                        operands[index].op;
                }
            }
            if (operands[index].reg == -3) {
                if (!expr_eval_integer_constant(operands[index].op->expr,
                                                 &immediate_values[index])) {
                    rcc_error(operands[index].op->expr->loc,
                              "inline asm immediate input must be an integer constant expression");
                    rcc_free(immediate_flags);
                    rcc_free(immediate_values);
                    rcc_free(memory_address_operands);
                    rcc_free(memory_address_registers);
                    rcc_free(memory_offsets);
                    rcc_free(operand_registers);
                    rcc_free(operands);
                    return;
                }
                immediate_flags[index] = true;
            }
        }
    }

    /* Preserve the i386 SysV callee-saved registers that fixed constraints
     * may select.  The generated function prologue does not otherwise touch
     * them. */
    bool preserve_ebx = false;
    bool preserve_esi = false;
    bool preserve_edi = false;
    for (int i = 0; i < output_count; i++) {
        preserve_ebx = preserve_ebx || operands[i].reg == EBX;
        preserve_esi = preserve_esi || operands[i].reg == ESI;
        preserve_edi = preserve_edi || operands[i].reg == EDI;
    }
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
            !asm_memory_constraint(operands[i].op) &&
            operands[i].reg >= 0 && operands[i].op->expr) {
            gen_expr(mod, operands[i].op->expr);
            emit_push_reg(mod, EAX);
        }
    }
    for (int i = output_count; i < total_operands; i++) {
        if (!asm_memory_constraint(operands[i].op) &&
            operands[i].reg >= 0 && operands[i].op->expr) {
            gen_expr(mod, operands[i].op->expr);
            emit_push_reg(mod, EAX);
        }
    }
    for (int i = 0; i < memory_address_count; ++i) {
        gen_lvalue(mod, memory_address_operands[i]->expr);
        emit_push_reg(mod, EAX);
    }
    for (int i = memory_address_count - 1; i >= 0; --i) {
        emit_pop_reg(mod, asm_operand_register(memory_address_operands[i]));
    }
    for (int i = total_operands - 1; i >= output_count; i--) {
        if (!asm_memory_constraint(operands[i].op) &&
            operands[i].reg >= 0 && operands[i].op->expr) {
            emit_pop_reg(mod, operands[i].reg);
        }
    }
    for (int i = output_count - 1; i >= 0; i--) {
        if (operands[i].op->constraint[0] == '+' &&
            !asm_memory_constraint(operands[i].op) &&
            operands[i].reg >= 0 && operands[i].op->expr) {
            emit_pop_reg(mod, operands[i].reg);
        }
    }

    /* Parse and emit assembly template.  Numeric placeholders expand to the
     * assigned fixed register or to the validated immediate spelling; the
     * instruction encoder still validates the resulting instruction. */
    char* tmpl_copy = stmt->asm_basic
        ? rcc_strdup(tmpl ? tmpl : "")
        : asm_expand_template(tmpl, operand_registers, memory_offsets,
                              memory_address_registers,
                              immediate_values, immediate_flags,
                              total_operands, stmt->loc);
    if (!tmpl_copy) {
        if (preserve_edi) emit_pop_reg(mod, EDI);
        if (preserve_esi) emit_pop_reg(mod, ESI);
        if (preserve_ebx) emit_pop_reg(mod, EBX);
        if (operand_registers) rcc_free(operand_registers);
        if (memory_address_registers) rcc_free(memory_address_registers);
        if (memory_address_operands) rcc_free(memory_address_operands);
        if (memory_offsets) rcc_free(memory_offsets);
        if (immediate_values) rcc_free(immediate_values);
        if (immediate_flags) rcc_free(immediate_flags);
        if (operands) rcc_free(operands);
        return;
    }
    size_t local_label_count = asm_count_numeric_labels(tmpl_copy);
    AsmNumericLabel* local_labels = local_label_count
        ? rcc_alloc(local_label_count * sizeof(*local_labels)) : NULL;
    if (local_labels) asm_collect_numeric_labels(tmpl_copy, local_labels);
    char* p = tmpl_copy;

    bool lock_prefix = false;
    bool repeat_prefix = false;
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
        size_t instruction_offset = (size_t)(instr - tmpl_copy);

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

            int local_label = asm_find_numeric_label_at(
                local_labels, local_label_count, instruction_offset);
            unsigned local_number;
            bool local_forward;
            bool is_local_branch =
                (strcmp(mnemonic, "jmp") == 0 ||
                 strcmp(mnemonic, "jnz") == 0 ||
                 strcmp(mnemonic, "jne") == 0 ||
                 strcmp(mnemonic, "jz") == 0 ||
                 strcmp(mnemonic, "je") == 0) &&
                asm_parse_numeric_label_reference(op1, &local_number,
                                                  &local_forward);
            if (local_label >= 0) {
                emit_label(mod, local_label);
                lock_prefix = false;
            } else if (is_local_branch) {
                int target = asm_find_numeric_label_reference(
                    local_labels, local_label_count, local_number,
                    local_forward, instruction_offset);
                if (target < 0) {
                    rcc_error(stmt->loc,
                              "inline asm numeric local-label reference has no matching definition");
                } else if (strcmp(mnemonic, "jmp") == 0) {
                    emit_jmp_label(mod, target);
                } else {
                    int condition = (strcmp(mnemonic, "jz") == 0 ||
                                     strcmp(mnemonic, "je") == 0)
                        ? CC_E : CC_NE;
                    emit_jcc_label(mod, condition, target);
                }
                lock_prefix = false;
            } else if (strcmp(mnemonic, "lock") == 0 && !op1[0] && !op2[0]) {
                lock_prefix = true;
            } else if (strcmp(mnemonic, "rep") == 0 && op1[0] && !op2[0] &&
                       !lock_prefix && !repeat_prefix) {
                if (!emit_asm_instruction(mod, op1, NULL, NULL, false, true)) {
                    rcc_error(stmt->loc,
                              "unsupported i686 inline asm REP instruction '%s'",
                              op1);
                }
            } else if (strcmp(mnemonic, "rep") == 0 && !op1[0] && !op2[0] &&
                       !lock_prefix && !repeat_prefix) {
                repeat_prefix = true;
            } else if (!emit_asm_instruction(mod, mnemonic,
                                             op1[0] ? op1 : NULL,
                                             op2[0] ? op2 : NULL,
                                             lock_prefix, repeat_prefix)) {
                rcc_error(stmt->loc,
                          "unsupported i686 inline asm instruction '%s'",
                          mnemonic);
            } else {
                lock_prefix = false;
                repeat_prefix = false;
            }
        }

        if (saved) {
            *p = saved;
            p++;
        }
    }

    if (repeat_prefix) {
        rcc_error(stmt->loc,
                  "i686 inline asm REP prefix has no following instruction");
    }

    rcc_free(tmpl_copy);
    if (local_labels) rcc_free(local_labels);

    /* Store outputs from registers to lvalues */
    for (int i = 0; i < output_count; i++) {
        if (!asm_memory_constraint(operands[i].op) &&
            operands[i].reg >= 0 && operands[i].op->expr) {
            /* Get lvalue address */
            emit_push_reg(mod, operands[i].reg);  /* Save the result */
            gen_lvalue(mod, operands[i].op->expr);
            emit_mov_reg_reg(mod, ECX, EAX);      /* ECX = address */
            emit_pop_reg(mod, EAX);               /* EAX = result */
            emit_store_typed32(mod, ECX, 0, EAX,
                               operands[i].op->expr->type);
        }
    }

    if (preserve_edi) emit_pop_reg(mod, EDI);
    if (preserve_esi) emit_pop_reg(mod, ESI);
    if (preserve_ebx) emit_pop_reg(mod, EBX);

    if (operands) {
        if (operand_registers) rcc_free(operand_registers);
        if (memory_address_registers) rcc_free(memory_address_registers);
        if (memory_address_operands) rcc_free(memory_address_operands);
        if (memory_offsets) rcc_free(memory_offsets);
        if (immediate_values) rcc_free(immediate_values);
        if (immediate_flags) rcc_free(immediate_flags);
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

static void codegen_assign_vla_typedef_bound_slots(
    Type* type, int* stack_bytes, int word_size, SourceLoc loc,
    Decl* owner, bool inherited_typeof) {
    int64_t aligned_bytes;
    int64_t extent;
    if (!type || !stack_bytes || *stack_bytes == INT_MAX) return;
    inherited_typeof = inherited_typeof ||
                       type->c_typeof_operand_type != NULL;
    if (type->kind == TYPE_PTR || type->kind == TYPE_ARRAY ||
        type->kind == TYPE_VECTOR) {
        codegen_assign_vla_typedef_bound_slots(
            type->base, stack_bytes, word_size, loc, owner,
            inherited_typeof);
    }
    if (type->kind != TYPE_ARRAY || !type->array_bound || inherited_typeof ||
        type->array_bound->c_vla_saved_bound_offset < 0) {
        return;
    }
    aligned_bytes = ((int64_t)*stack_bytes + word_size - 1) /
                    word_size * word_size;
    extent = aligned_bytes + word_size;
    if (extent > INT_MAX) {
        rcc_error(loc, "function stack frame exceeds compiler limits");
        *stack_bytes = INT_MAX;
        return;
    }
    type->array_bound->c_vla_saved_bound_offset = -(int)extent;
    type->array_bound->c_vla_typedef_owner = owner;
    *stack_bytes = (int)extent;
}

static void codegen_assign_local_vla_extent_stmt(Stmt* statement,
                                                 int* stack_bytes,
                                                 int word_size) {
    if (!statement || !stack_bytes || *stack_bytes == INT_MAX) return;
    switch (statement->kind) {
        case STMT_BLOCK:
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                codegen_assign_local_vla_extent_stmt(item->stmt, stack_bytes,
                                                       word_size);
            }
            break;
        case STMT_IF:
            codegen_assign_local_vla_extent_stmt(statement->if_then,
                                                  stack_bytes, word_size);
            codegen_assign_local_vla_extent_stmt(statement->if_else,
                                                  stack_bytes, word_size);
            break;
        case STMT_WHILE:
        case STMT_DO:
            codegen_assign_local_vla_extent_stmt(statement->while_body,
                                                  stack_bytes, word_size);
            break;
        case STMT_FOR:
            codegen_assign_local_vla_extent_stmt(statement->for_init,
                                                  stack_bytes, word_size);
            codegen_assign_local_vla_extent_stmt(statement->for_body,
                                                  stack_bytes, word_size);
            break;
        case STMT_SWITCH:
            codegen_assign_local_vla_extent_stmt(statement->switch_body,
                                                  stack_bytes, word_size);
            break;
        case STMT_CASE:
            codegen_assign_local_vla_extent_stmt(statement->case_stmt,
                                                  stack_bytes, word_size);
            break;
        case STMT_DEFAULT:
            codegen_assign_local_vla_extent_stmt(statement->default_stmt,
                                                  stack_bytes, word_size);
            break;
        case STMT_LABEL:
            codegen_assign_local_vla_extent_stmt(statement->label_stmt,
                                                  stack_bytes, word_size);
            break;
        case STMT_TRY:
            codegen_assign_local_vla_extent_stmt(statement->try_body,
                                                  stack_bytes, word_size);
            for (CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                codegen_assign_local_vla_extent_stmt(handler->body,
                                                      stack_bytes, word_size);
            }
            break;
        case STMT_DECL: {
            Decl* declaration = statement->decl;
            int dimensions;
            int64_t aligned_bytes;
            int64_t extent;
            if (!declaration) break;
            if (declaration->kind == DECL_TYPEDEF &&
                codegen_type_has_vla_any(declaration->typedef_type)) {
                codegen_assign_vla_typedef_bound_slots(
                    declaration->typedef_type, stack_bytes, word_size,
                    declaration->loc, declaration, false);
                break;
            }
            if (declaration->kind != DECL_VAR ||
                declaration->var_is_global || declaration->var_is_vla ||
                declaration->var_vla_extent_count > 0 ||
                !codegen_type_has_vla_any(declaration->type)) {
                break;
            }
            dimensions = codegen_vla_dimension_count(declaration->type);
            aligned_bytes = ((int64_t)*stack_bytes + word_size - 1) /
                            word_size * word_size;
            extent = aligned_bytes + (int64_t)dimensions * word_size;
            if (dimensions <= 0 || extent > INT_MAX) {
                rcc_error(declaration->loc,
                          "function stack frame exceeds compiler limits");
                *stack_bytes = INT_MAX;
                break;
            }
            declaration->var_vla_extent_offset = -(int)extent;
            declaration->var_vla_extent_count = dimensions;
            *stack_bytes = (int)extent;
            break;
        }
        default:
            break;
    }
}

int codegen_assign_local_vla_extent_slots(Stmt* statement, int initial_bytes,
                                          int word_size) {
    int stack_bytes = initial_bytes < 0 ? INT_MAX : initial_bytes;
    if (word_size != 4 && word_size != 8) return stack_bytes;
    codegen_assign_local_vla_extent_stmt(statement, &stack_bytes, word_size);
    return stack_bytes;
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

static int codegen_required_frame_alignment(const Stmt* statement) {
    int alignment = 4;
    if (!statement) return alignment;
    switch (statement->kind) {
        case STMT_DECL:
            if (statement->decl && statement->decl->type &&
                statement->decl->type->has_explicit_alignment &&
                statement->decl->type->align > alignment) {
                alignment = statement->decl->type->align;
            }
            break;
        case STMT_BLOCK:
            for (const StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                int nested = codegen_required_frame_alignment(item->stmt);
                if (nested > alignment) alignment = nested;
            }
            break;
        case STMT_IF:
            alignment = codegen_required_frame_alignment(statement->if_then);
            if (statement->if_else) {
                int nested = codegen_required_frame_alignment(statement->if_else);
                if (nested > alignment) alignment = nested;
            }
            break;
        case STMT_WHILE:
        case STMT_DO:
            alignment = codegen_required_frame_alignment(statement->while_body);
            break;
        case STMT_FOR: {
            alignment = codegen_required_frame_alignment(statement->for_init);
            int nested = codegen_required_frame_alignment(statement->for_body);
            if (nested > alignment) alignment = nested;
            break;
        }
        case STMT_SWITCH:
            alignment = codegen_required_frame_alignment(statement->switch_body);
            break;
        case STMT_CASE:
            alignment = codegen_required_frame_alignment(statement->case_stmt);
            break;
        case STMT_DEFAULT:
            alignment = codegen_required_frame_alignment(statement->default_stmt);
            break;
        case STMT_LABEL:
            alignment = codegen_required_frame_alignment(statement->label_stmt);
            break;
        case STMT_TRY:
            alignment = codegen_required_frame_alignment(statement->try_body);
            for (const CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                int nested = codegen_required_frame_alignment(handler->body);
                if (nested > alignment) alignment = nested;
            }
            break;
        default:
            break;
    }
    return alignment;
}

static int codegen_align_frame_bytes(int bytes, int alignment) {
    int64_t value;
    if (alignment <= 1) return bytes;
    value = (int64_t)bytes + alignment - 1;
    if (value > INT_MAX) return INT_MAX;
    return (int)(value / alignment * alignment);
}

static ExprList* active_call_temporary_owners = NULL;

typedef struct CodegenTLSInitializerPlanFrame {
    Decl* declaration;
    struct CodegenTLSInitializerPlanFrame* previous;
} CodegenTLSInitializerPlanFrame;

static CodegenTLSInitializerPlanFrame* active_tls_initializer_plan = NULL;

ExprList* codegen_call_temporary_owners(void) {
    return active_call_temporary_owners;
}

static void codegen_assign_initializer_temporary_storage(
    ExprList* initializer, int* bytes, int stack_alignment) {
    if (!initializer || !initializer->cxx_temporary_owner || !bytes ||
        *bytes == INT_MAX) {
        return;
    }
    Type* owner_type = initializer->cxx_temporary_owner->type;
    int size = owner_type && owner_type->size > 0
        ? owner_type->size : stack_alignment;
    int alignment = owner_type && owner_type->align > 0
        ? owner_type->align : stack_alignment;
    int64_t extent;
    if (alignment < stack_alignment) alignment = stack_alignment;
    extent = (int64_t)*bytes + size;
    if (extent > INT_MAX) {
        *bytes = INT_MAX;
    } else {
        *bytes = codegen_align_frame_bytes((int)extent, alignment);
        initializer->cxx_temporary_owner->var_offset = -*bytes;
        initializer->cxx_temporary_next = active_call_temporary_owners;
        active_call_temporary_owners = initializer;
    }
    codegen_assign_cleanup_plan(initializer->cxx_temporary_cleanups,
                                bytes, stack_alignment);
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
         expression->type->kind == TYPE_UNION ||
         expression->type->kind == TYPE_VECTOR ||
         type_is_cxx_member_function_pointer(expression->type))) {
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
         expression->va_arg_type->kind == TYPE_UNION ||
         expression->va_arg_type->kind == TYPE_VECTOR ||
         type_is_cxx_member_function_pointer(expression->va_arg_type))) {
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
    if ((expression->kind == EXPR_EQ || expression->kind == EXPR_NE) &&
        expression->binary_lhs && expression->binary_rhs &&
        (type_is_complex(expression->binary_lhs->type) ||
         type_is_complex(expression->binary_rhs->type)) &&
        expression->call_result_offset >= 0) {
        Type* comparison_type = gen_complex_comparison_type32(expression);
        int size = comparison_type ? comparison_type->size : 0;
        int alignment = comparison_type ? comparison_type->align : 0;
        int64_t extent;
        if (alignment < stack_alignment) alignment = stack_alignment;
        if (size <= 0 || size > INT_MAX / 2) {
            *bytes = INT_MAX;
        } else {
            extent = (int64_t)*bytes + (int64_t)size * 2;
            if (extent > INT_MAX) {
                *bytes = INT_MAX;
            } else {
                *bytes = codegen_align_frame_bytes((int)extent, alignment);
                expression->call_result_offset = -*bytes;
            }
        }
    }
    if (type_is_complex(expression->type) &&
        (expression->kind == EXPR_ADD || expression->kind == EXPR_SUB ||
         expression->kind == EXPR_MUL || expression->kind == EXPR_DIV ||
         expression->kind == EXPR_ADD_ASSIGN ||
         expression->kind == EXPR_SUB_ASSIGN ||
         expression->kind == EXPR_MUL_ASSIGN ||
         expression->kind == EXPR_DIV_ASSIGN ||
         expression->kind == EXPR_PREINC ||
         expression->kind == EXPR_PREDEC ||
         expression->kind == EXPR_POSTINC ||
         expression->kind == EXPR_POSTDEC) &&
        expression->call_result_offset >= 0) {
        int size = expression->type->size;
        int alignment = expression->type->align;
        int64_t extent;
        if (alignment < stack_alignment) alignment = stack_alignment;
        if (size <= 0 || size > INT_MAX / 2) {
            *bytes = INT_MAX;
        } else {
            extent = (int64_t)*bytes + (int64_t)size * 2;
            if (extent > INT_MAX) {
                *bytes = INT_MAX;
            } else {
                *bytes = codegen_align_frame_bytes((int)extent, alignment);
                expression->call_result_offset = -*bytes;
            }
        }
    }
    if (type_is_complex(expression->type) &&
        (expression->kind == EXPR_CAST || expression->kind == EXPR_NEG) &&
        expression->call_result_offset >= 0) {
        int size = expression->type->size;
        int alignment = expression->type->align;
        int64_t extent;
        if (alignment < stack_alignment) alignment = stack_alignment;
        if (size <= 0) {
            *bytes = INT_MAX;
        } else {
            extent = (int64_t)*bytes + size;
            if (extent > INT_MAX) {
                *bytes = INT_MAX;
            } else {
                *bytes = codegen_align_frame_bytes((int)extent, alignment);
                expression->call_result_offset = -*bytes;
            }
        }
    }
    if (type_is_cxx_member_function_pointer(expression->type) &&
        expression->aggregate_offset == 0 &&
        (expression->kind == EXPR_ADDR ||
         expression->kind == EXPR_CAST ||
         expression->cxx_member_pointer_adjustment_valid ||
         expression->is_cxx_nullptr)) {
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
            expression->aggregate_offset = -*bytes;
        }
    }
    if (expression->kind == EXPR_CALL &&
        expression->call_member_pointer_application &&
        expression->call_member_pointer_value_offset == 0) {
        Type* member_pointer_type =
            expression->call_member_pointer_application->binary_rhs
                ? expression->call_member_pointer_application
                      ->binary_rhs->type
                : NULL;
        int member_pointer_size = member_pointer_type
            ? member_pointer_type->size : 0;
        if (member_pointer_size < stack_alignment) {
            member_pointer_size = stack_alignment;
        }
        int64_t extent = (int64_t)*bytes + member_pointer_size;
        if (extent > INT_MAX) {
            *bytes = INT_MAX;
        } else {
            *bytes = codegen_align_frame_bytes((int)extent,
                                               stack_alignment);
            expression->call_member_pointer_value_offset = -*bytes;
            extent = (int64_t)*bytes + stack_alignment;
            if (extent > INT_MAX) {
                *bytes = INT_MAX;
            } else {
                *bytes = codegen_align_frame_bytes((int)extent,
                                                   stack_alignment);
                expression->call_member_pointer_this_offset = -*bytes;
            }
        }
    }

    switch (expression->kind) {
        case EXPR_IDENT: {
            Decl* declaration = expression->ident_decl;
            if (declaration && declaration->var_is_thread_local &&
                declaration->var_init &&
                (declaration->var_reference_temporary_guard ||
                 declaration->var_dynamic_initializer)) {
                CodegenTLSInitializerPlanFrame* frame;
                for (frame = active_tls_initializer_plan; frame;
                     frame = frame->previous) {
                    if (frame->declaration == declaration) break;
                }
                if (!frame) {
                    CodegenTLSInitializerPlanFrame current = {
                        declaration, active_tls_initializer_plan};
                    active_tls_initializer_plan = &current;
                    codegen_assign_compound_expr(declaration->var_init,
                                                 bytes, stack_alignment);
                    active_tls_initializer_plan = current.previous;
                }
            }
            break;
        }
        case EXPR_POS:
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
        case EXPR_CXX_MEMBER_PTR_DOT:
        case EXPR_CXX_MEMBER_PTR_ARROW:
            codegen_assign_compound_expr(expression->binary_lhs, bytes,
                                         stack_alignment);
            codegen_assign_compound_expr(expression->binary_rhs, bytes,
                                         stack_alignment);
            break;
        case EXPR_COMMA:
            if (gen_aggregate_type32(expression->type) &&
                expression->aggregate_offset == 0) {
                int size = expression->type->size;
                int alignment = expression->type->align;
                int64_t extent;
                if (alignment < stack_alignment) alignment = stack_alignment;
                if (size <= 0) size = 1;
                extent = (int64_t)*bytes + size;
                if (extent > INT_MAX) {
                    *bytes = INT_MAX;
                } else {
                    *bytes = codegen_align_frame_bytes((int)extent,
                                                       alignment);
                    expression->aggregate_offset = -*bytes;
                }
            }
            codegen_assign_compound_expr(expression->binary_lhs, bytes,
                                         stack_alignment);
            codegen_assign_compound_expr(expression->binary_rhs, bytes,
                                         stack_alignment);
            break;
        case EXPR_COND:
            if (gen_aggregate_type32(expression->type) &&
                expression->aggregate_offset == 0) {
                int size = expression->type->size;
                int alignment = expression->type->align;
                int64_t extent;
                if (alignment < stack_alignment) alignment = stack_alignment;
                if (size <= 0) size = 1;
                extent = (int64_t)*bytes + size;
                if (extent > INT_MAX) {
                    *bytes = INT_MAX;
                } else {
                    *bytes = codegen_align_frame_bytes((int)extent,
                                                       alignment);
                    expression->aggregate_offset = -*bytes;
                }
            }
            codegen_assign_compound_expr(expression->cond_test, bytes,
                                         stack_alignment);
            codegen_assign_compound_expr(expression->cond_then, bytes,
                                         stack_alignment);
            codegen_assign_compound_expr(expression->cond_else, bytes,
                                         stack_alignment);
            break;
        case EXPR_CALL:
            if (expression->call_new_type &&
                (expression->call_new_type->kind == TYPE_STRUCT ||
                 expression->call_new_type->kind == TYPE_UNION)) {
                for (TypeField* field = expression->call_new_type->fields;
                     field; field = field->next) {
                    codegen_assign_compound_expr(field->initializer, bytes,
                                                 stack_alignment);
                }
            }
            codegen_assign_compound_expr(expression->call_func, bytes,
                                         stack_alignment);
            codegen_assign_compound_expr(
                expression->call_member_pointer_application, bytes,
                stack_alignment);
            for (ExprList* argument = expression->call_args; argument;
                 argument = argument->next) {
                codegen_assign_compound_expr(argument->expr, bytes,
                                             stack_alignment);
                if (argument->cxx_temporary_owner) {
                    Type* owner_type = argument->cxx_temporary_owner->type;
                    int size = owner_type && owner_type->size > 0
                        ? owner_type->size : stack_alignment;
                    int alignment = owner_type && owner_type->align > 0
                        ? owner_type->align : stack_alignment;
                    int64_t extent;
                    if (alignment < stack_alignment) {
                        alignment = stack_alignment;
                    }
                    extent = (int64_t)*bytes + size;
                    if (extent > INT_MAX) {
                        *bytes = INT_MAX;
                    } else {
                        *bytes = codegen_align_frame_bytes((int)extent,
                                                           alignment);
                        argument->cxx_temporary_owner->var_offset = -*bytes;
                        argument->cxx_temporary_next =
                            active_call_temporary_owners;
                        active_call_temporary_owners = argument;
                    }
                }
                codegen_assign_cleanup_plan(
                    argument->cxx_temporary_cleanups, bytes,
                    stack_alignment);
            }
            if (expression->cxx_temporary_owner &&
                expression->cxx_temporary_source) {
                Expr* source = expression->cxx_temporary_source;
                int temporary_offset = source->kind == EXPR_CALL
                    ? source->call_result_offset
                    : source->kind == EXPR_COMPOUND
                        ? source->compound_offset
                        : (source->kind == EXPR_COND ||
                           source->kind == EXPR_COMMA)
                            ? source->aggregate_offset : 0;
                /* The implicit object argument recursively assigns its
                 * receiver's result slot while traversing call_args.  Only
                 * assign it here if this member-call form did not expose that
                 * path, so cleanup always refers to the same live object. */
                if (temporary_offset >= 0) {
                    codegen_assign_compound_expr(source, bytes,
                                                 stack_alignment);
                    temporary_offset = source->kind == EXPR_CALL
                        ? source->call_result_offset
                        : source->kind == EXPR_COMPOUND
                            ? source->compound_offset
                            : (source->kind == EXPR_COND ||
                               source->kind == EXPR_COMMA)
                                ? source->aggregate_offset : 0;
                }
                if (temporary_offset < 0) {
                    expression->cxx_temporary_owner->var_offset =
                        temporary_offset;
                } else {
                    rcc_error(source->loc,
                              "class-prvalue member receiver has no assigned result slot");
                }
            }
            for (ExprList* argument = expression->call_new_args; argument;
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
            if (expression->compound_type &&
                (expression->compound_type->kind == TYPE_STRUCT ||
                 expression->compound_type->kind == TYPE_UNION)) {
                for (TypeField* field = expression->compound_type->fields;
                     field; field = field->next) {
                    codegen_assign_compound_expr(field->initializer, bytes,
                                                 stack_alignment);
                }
            }
            for (ExprList* initializer = expression->compound_init;
                 initializer; initializer = initializer->next) {
                codegen_assign_compound_expr(initializer->expr, bytes,
                                             stack_alignment);
                codegen_assign_initializer_temporary_storage(
                    initializer, bytes, stack_alignment);
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

static bool codegen_reference_temporary_scalar(Type* type) {
    return type && (type_is_integer(type) || type->kind == TYPE_ENUM ||
                    type_is_floating(type) || type->kind == TYPE_PTR ||
                    type->kind == TYPE_NULLPTR);
}

static bool codegen_reference_temporary_local(Type* type,
                                              Expr* initializer) {
    if (codegen_reference_temporary_scalar(type)) return true;
    return type && initializer && initializer->type &&
        type_is_complete(type) && type->size > 0 &&
        (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION ||
         type->kind == TYPE_VECTOR) &&
        type_is_compatible(type, initializer->type);
}

void codegen_assign_cleanup_plan(CxxCleanupPlan* plan, int* bytes,
                                int stack_alignment) {
    for (; plan; plan = plan->next) {
        if (plan->kind == CXX_CLEANUP_CONDITIONAL) {
            codegen_assign_cleanup_plan(plan->body, bytes, stack_alignment);
            codegen_assign_cleanup_plan(
                plan->alternate_body, bytes, stack_alignment);
            continue;
        }
        if (plan->kind != CXX_CLEANUP_ARRAY_LOOP) continue;
        if (plan->index_decl && plan->index_decl->var_offset == 0) {
            int size = type_int && type_int->size > 0 ? type_int->size : 4;
            int alignment = type_int && type_int->align > 0
                ? type_int->align : 4;
            int64_t extent;
            if (alignment < stack_alignment) alignment = stack_alignment;
            extent = (int64_t)*bytes + size;
            if (extent > INT_MAX) {
                *bytes = INT_MAX;
            } else {
                *bytes = codegen_align_frame_bytes((int)extent, alignment);
                plan->index_decl->var_offset = -*bytes;
            }
        }
        codegen_assign_cleanup_plan(plan->body, bytes, stack_alignment);
    }
}

int codegen_assign_global_cleanup_storage(GlobalFinalizer* finalizers,
                                          int stack_alignment) {
    int bytes = 0;
    for (; finalizers; finalizers = finalizers->next) {
        codegen_assign_cleanup_plan(finalizers->plan, &bytes,
                                    stack_alignment);
    }
    return bytes;
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
            if (statement->return_reference_result &&
                statement->return_reference_temporary_offset == 0 &&
                statement->return_val && statement->return_val->type &&
                !statement->return_val->type->is_reference &&
                !gen_expr_is_lvalue(statement->return_val) &&
                !gen_expr_is_xvalue(statement->return_val) &&
                codegen_reference_temporary_scalar(
                    statement->return_val->type)) {
                int size = statement->return_val->type->size;
                int alignment = statement->return_val->type->align;
                int64_t extent;
                if (size <= 0) size = 1;
                if (alignment < stack_alignment) {
                    alignment = stack_alignment;
                }
                extent = (int64_t)*bytes + size;
                if (extent > INT_MAX) {
                    *bytes = INT_MAX;
                } else {
                    *bytes = codegen_align_frame_bytes((int)extent,
                                                       alignment);
                    statement->return_reference_temporary_offset = -*bytes;
                }
            }
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
                Decl* declaration = statement->decl;
                Type* type = declaration->type;
                Expr* temporary_source =
                    declaration->var_reference_temporary_source
                        ? declaration->var_reference_temporary_source
                        : gen_reference_temporary_source(
                              declaration->var_init);
                Expr* temporary_initializer = temporary_source
                    ? temporary_source : declaration->var_init;
                Type* temporary_type = type && type->is_reference
                    ? type->base : NULL;
                bool conditional_xvalue_owner =
                    declaration->var_reference_temporary_owner &&
                    declaration->var_reference_temporary_owner
                        ->var_reference_temporary_alternate_type;
                codegen_assign_compound_expr(declaration->var_init, bytes,
                                             stack_alignment);
                codegen_assign_cleanup_plan(declaration->var_cleanups, bytes,
                                            stack_alignment);
                if (temporary_type &&
                    !codegen_reference_temporary_scalar(temporary_type) &&
                    temporary_initializer && temporary_initializer->type &&
                    (temporary_initializer->type->kind == TYPE_STRUCT ||
                     temporary_initializer->type->kind == TYPE_UNION ||
                     temporary_initializer->type->kind == TYPE_VECTOR)) {
                    temporary_type = declaration->var_reference_temporary_owner
                        ? declaration->var_reference_temporary_owner->type
                        : temporary_initializer->type;
                }
                if (declaration->var_reference_temporary_offset == 0 &&
                    !declaration->var_is_global &&
                    !declaration->var_is_static_local &&
                    !declaration->var_is_block_extern &&
                    type && type->kind == TYPE_PTR && type->is_reference &&
                    temporary_type && temporary_initializer &&
                    (temporary_source || conditional_xvalue_owner ||
                     (!gen_expr_is_lvalue(declaration->var_init) &&
                      !gen_expr_is_xvalue(declaration->var_init))) &&
                    (conditional_xvalue_owner ||
                     codegen_reference_temporary_local(
                         temporary_type, temporary_initializer))) {
                    if (!codegen_reference_temporary_scalar(temporary_type) &&
                        temporary_initializer->kind == EXPR_CALL &&
                        temporary_initializer->call_result_offset < 0) {
                        /* The ABI result slot is already the materialized
                         * prvalue object.  Reuse it so extending a reference
                         * does not create a second class object. */
                        declaration->var_reference_temporary_offset =
                            temporary_initializer->call_result_offset;
                        if (declaration->var_reference_temporary_owner) {
                            declaration->var_reference_temporary_owner->var_offset =
                                declaration->var_reference_temporary_offset;
                        }
                        break;
                    }
                    int size = conditional_xvalue_owner &&
                            declaration->var_reference_temporary_owner
                                ->var_reference_temporary_storage_size > 0
                        ? declaration->var_reference_temporary_owner
                              ->var_reference_temporary_storage_size
                        : temporary_type->size;
                    int alignment = conditional_xvalue_owner &&
                            declaration->var_reference_temporary_owner
                                ->var_reference_temporary_storage_align > 0
                        ? declaration->var_reference_temporary_owner
                              ->var_reference_temporary_storage_align
                        : temporary_type->align;
                    int64_t extent;
                    if (size <= 0) size = 1;
                    if (alignment < stack_alignment) {
                        alignment = stack_alignment;
                    }
                    extent = (int64_t)*bytes + size;
                    if (extent > INT_MAX) {
                        *bytes = INT_MAX;
                    } else {
                        *bytes = codegen_align_frame_bytes((int)extent,
                                                           alignment);
                        declaration->var_reference_temporary_offset = -*bytes;
                        if (declaration->var_reference_temporary_owner) {
                            declaration->var_reference_temporary_owner->var_offset =
                                declaration->var_reference_temporary_offset;
                        }
                    }
                }
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
    active_call_temporary_owners = NULL;
    codegen_assign_compound_stmt(statement, &bytes, stack_alignment);
    return bytes;
}

int codegen_assign_cxx_constructor_storage(Decl* declaration,
                                          int initial_bytes,
                                          int stack_alignment) {
    int bytes = initial_bytes < 0 ? INT_MAX : initial_bytes;
    CxxClass* cls = declaration && declaration->func_is_cxx_constructor &&
            declaration->func_method_owner
        ? declaration->func_method_owner->cxx_class : NULL;
    if (stack_alignment < 1) stack_alignment = 1;
    if (!cls || bytes == INT_MAX) return bytes;
    for (CxxConstructorInfo* constructor = cls->constructors; constructor;
         constructor = constructor->next) {
        if (!constructor->method || constructor->method->decl != declaration) {
            continue;
        }
        for (CxxConstructorInitializer* initializer =
                 constructor->initializers;
             initializer; initializer = initializer->next) {
            codegen_assign_compound_expr(initializer->value, &bytes,
                                         stack_alignment);
            for (ExprList* argument = initializer->arguments; argument;
                 argument = argument->next) {
                codegen_assign_compound_expr(argument->expr, &bytes,
                                             stack_alignment);
                codegen_assign_initializer_temporary_storage(
                    argument, &bytes, stack_alignment);
            }
        }
    }
    return bytes;
}

int codegen_assign_global_initializer_storage(
    GlobalInitializer* initializers, int stack_alignment) {
    int bytes = 0;
    if (stack_alignment < 1) stack_alignment = 1;
    active_call_temporary_owners = NULL;
    for (; initializers; initializers = initializers->next) {
        Decl* declaration = initializers->declaration;
        Expr* initializer = declaration ? declaration->var_init : NULL;
        codegen_assign_compound_expr(initializer, &bytes, stack_alignment);
    }
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

/* Aggregate initialization must publish each completed class subobject
 * before the next member can throw, then retire those entries on success. */
static void gen_cxx_exception_change_local_subobject32(
    Module* mod, Type* object_type, int32_t displacement, bool unregister) {
    if (!mod || !object_type || !object_type->cxx_class ||
        active_cxx_exception_cleanup_frame_offset == INT_MAX) {
        return;
    }
    emit_byte(mod, 0x8D);  /* LEA EAX, [EBP+disp32] */
    emit_byte(mod, modrm(2, EAX, EBP));
    emit_dword(mod, (uint32_t)displacement);
    gen_cxx_exception_change_subobject32(
        mod, object_type, EAX, active_cxx_exception_cleanup_frame_offset,
        unregister);
}

static bool gen_local_initializer_with_temporary_cleanup32(
    Module* mod, Type* type, Expr* initializer, int32_t displacement,
    ExprList* item) {
    if (!type || type->kind != TYPE_PTR || !type->is_reference || !item ||
        !item->cxx_temporary_owner || !item->cxx_temporary_cleanups) {
        return gen_local_initializer(mod, type, initializer, displacement);
    }
    gen_lvalue(mod, initializer);
    gen_cxx_reference_adjustment32(mod, initializer);
    emit_store_typed32(mod, EBP,
                       item->cxx_temporary_owner->var_offset,
                       EAX, item->cxx_temporary_owner->type);
    emit_store_typed32(mod, EBP, displacement, EAX, type);
    codegen_push_call_temporary_cleanup32(
        mod, item->cxx_temporary_owner, item->cxx_temporary_cleanups);
    return true;
}

static bool gen_local_initializer(Module* mod, Type* type, Expr* initializer,
                                  int32_t displacement) {
    Expr* string = codegen_character_array_string(type, initializer);
    if (!type || !initializer) return false;
    if (rcc_parser_is_cxx_mode() &&
        rcc_parser_cxx_standard_at_least(17) &&
        (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) &&
        initializer->kind == EXPR_CALL && initializer->type &&
        type_is_compatible(type, initializer->type)) {
        int result_offset = initializer->call_result_offset;
        initializer->call_result_offset = displacement;
        gen_expr(mod, initializer);
        initializer->call_result_offset = result_offset;
        return true;
    }
    if (initializer->kind == EXPR_COMPOUND &&
        initializer->compound_constructor && type->cxx_class) {
        emit_byte(mod, 0x8D);  /* LEA EAX, [EBP+disp32] */
        emit_byte(mod, modrm(2, EAX, EBP));
        emit_dword(mod, (uint32_t)displacement);
        emit_mov_reg_reg(mod, ECX, EAX);
        gen_cxx_initialize_object32(
            mod, type, initializer->compound_constructor,
            initializer->compound_value_init ? NULL : initializer->compound_init);
        return true;
    }
    if (initializer->kind == EXPR_COMPOUND && initializer->compound_copy_init &&
        initializer->compound_init && !initializer->compound_init->next &&
        initializer->compound_init->expr &&
        initializer->compound_init->expr->type &&
        type_is_compatible(type, initializer->compound_init->expr->type) &&
        (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION ||
         type->kind == TYPE_VECTOR)) {
        int offset = 0;
        gen_lvalue(mod, initializer->compound_init->expr);
        emit_mov_reg_reg(mod, ECX, EAX);
        while (offset + 4 <= type->size) {
            emit_mov_reg_mem(mod, EAX, ECX, offset);
            emit_mov_mem_reg(mod, EBP, displacement + offset, EAX);
            offset += 4;
        }
        while (offset < type->size) {
            emit_load_typed32(mod, EAX, ECX, offset, type_uchar);
            emit_mov_mem_reg8(mod, EBP, displacement + offset, EAX);
            ++offset;
        }
        return true;
    }
    if (codegen_aggregate_zero_initializer(type, initializer)) return true;
    if (type_is_complex(type)) {
        gen_complex_component_from_operand32(
            mod, initializer, type->complex_component_type, displacement);
        return true;
    }
    if (type_is_cxx_member_function_pointer(type) &&
        (initializer->is_cxx_nullptr ||
         (initializer->type &&
          initializer->type->kind == TYPE_NULLPTR) ||
         (initializer->kind == EXPR_INT_LIT &&
          initializer->int_val == 0))) {
        gen_zero_local_storage(mod, displacement, (size_t)type->size);
        return true;
    }
    if (type_is_cxx_member_function_pointer(type) && !type->is_reference) {
        gen_expr(mod, initializer);
        emit_mov_reg_reg(mod, EDX, EAX);
        emit_byte(mod, 0x8D); /* LEA ECX, [EBP+disp32] */
        emit_byte(mod, modrm(2, ECX, EBP));
        emit_dword(mod, (uint32_t)displacement);
        gen_copy_aggregate_to_address32(mod, ECX, EDX, type->size);
        return true;
    }
    if (initializer->kind == EXPR_COMPOUND && type->kind == TYPE_VECTOR) {
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
    if (type->kind == TYPE_PTR && type->is_reference) {
        gen_lvalue(mod, initializer);
        gen_cxx_reference_adjustment32(mod, initializer);
        emit_store_typed32(mod, EBP, displacement, EAX, type);
        return true;
    }
    if (type->kind == TYPE_PTR && type->cxx_is_member_pointer &&
        !type->is_reference &&
        (initializer->is_cxx_nullptr ||
         (initializer->type &&
          initializer->type->kind == TYPE_NULLPTR) ||
         codegen_aggregate_zero_initializer(type, initializer))) {
        emit_mov_reg_imm(mod, EAX, UINT32_MAX);
        emit_store_typed32(mod, EBP, displacement, EAX, type);
        return true;
    }
    if (string) {
        size_t storage = (size_t)type->size;
        size_t text_size = string->str_byte_length +
            (string->str_element_width ? string->str_element_width : 1u);
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
                gen_cxx_exception_change_local_subobject32(
                    mod, type->base, (int32_t)item_offset, false);
                ++cursor;
            }
            cursor = 0;
            for (ExprList* item = initializer->compound_init; item;
                 item = item->next) {
                int64_t item_offset;
                if (item->designator_kind == INIT_DESIGNATOR_INDEX) {
                    cursor = item->designator_index;
                }
                item_offset = (int64_t)displacement +
                              cursor * type->base->size;
                if (item_offset >= INT32_MIN && item_offset <= INT32_MAX) {
                    gen_cxx_exception_change_local_subobject32(
                        mod, type->base, (int32_t)item_offset, true);
                }
                ++cursor;
            }
            return true;
        }
        if (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) {
            if (ast_cxx_is_aggregate(type)) {
                int element_index = 0;
                for (ExprList* item = initializer->compound_init; item;
                     item = item->next) {
                    Type* element_type = NULL;
                    TypeField* field = NULL;
                    int element_offset = 0;
                    int64_t field_offset;
                    if (item->designator_kind == INIT_DESIGNATOR_FIELD) {
                        field = ast_cxx_aggregate_member(
                            type, item->designator_field);
                        if (!field) return false;
                        element_type = field->type;
                        element_offset = field->offset;
                    } else if (item->designator_kind !=
                                   INIT_DESIGNATOR_NONE ||
                               !ast_cxx_aggregate_element(
                                   type, element_index++, &element_type,
                                   &element_offset, &field)) {
                        return false;
                    }
                    field_offset = (int64_t)displacement + element_offset;
                    if (!element_type || field_offset < INT32_MIN ||
                        field_offset > INT32_MAX) {
                        return false;
                    }
                    if (field && field->is_bitfield) {
                        if (!gen_bitfield_initializer32(
                                mod, field, item->expr,
                                (int32_t)field_offset)) {
                            return false;
                        }
                    } else if (!gen_local_initializer_with_temporary_cleanup32(
                                   mod, element_type, item->expr,
                                   (int32_t)field_offset, item)) {
                        return false;
                    }
                    gen_cxx_exception_change_local_subobject32(
                        mod, element_type, (int32_t)field_offset, false);
                }
                element_index = 0;
                for (ExprList* item = initializer->compound_init; item;
                     item = item->next) {
                    Type* element_type = NULL;
                    TypeField* field = NULL;
                    int element_offset = 0;
                    int64_t field_offset;
                    if (item->designator_kind == INIT_DESIGNATOR_FIELD) {
                        field = ast_cxx_aggregate_member(
                            type, item->designator_field);
                        if (!field) return false;
                        element_type = field->type;
                        element_offset = field->offset;
                    } else if (item->designator_kind !=
                                   INIT_DESIGNATOR_NONE ||
                               !ast_cxx_aggregate_element(
                                   type, element_index++, &element_type,
                                   &element_offset, &field)) {
                        return false;
                    }
                    field_offset = (int64_t)displacement + element_offset;
                    if (element_type && field_offset >= INT32_MIN &&
                        field_offset <= INT32_MAX &&
                        !(field && field->is_bitfield)) {
                        gen_cxx_exception_change_local_subobject32(
                            mod, element_type, (int32_t)field_offset, true);
                    }
                }
                return true;
            }
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
                         : !gen_local_initializer_with_temporary_cleanup32(
                               mod, field->type, item->expr,
                               (int32_t)field_offset, item))) {
                    return false;
                }
                if (!field->is_bitfield) {
                    gen_cxx_exception_change_local_subobject32(
                        mod, field->type, (int32_t)field_offset, false);
                }
                cursor = field->next;
                ++initialized;
            }
            cursor = type->fields;
            initialized = 0;
            for (ExprList* item = initializer->compound_init; item;
                 item = item->next) {
                TypeField* field = cursor;
                int64_t field_offset;
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
                if (!field->is_bitfield && field_offset >= INT32_MIN &&
                    field_offset <= INT32_MAX) {
                    gen_cxx_exception_change_local_subobject32(
                        mod, field->type, (int32_t)field_offset, true);
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
    if (type->kind == TYPE_VECTOR && initializer->type &&
        type_is_compatible(type, initializer->type)) {
        if (initializer->kind == EXPR_VA_ARG) {
            gen_expr(mod, initializer);
        } else {
            gen_lvalue(mod, initializer);
        }
        gen_copy_aggregate32(mod, displacement, EAX, type->size);
        return true;
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
/* The active handler still owns the transferred payload until its body has
 * completed.  INT_MAX means that code is not being emitted inside a catch. */
static int active_cxx_exception_frame_offset = INT_MAX;

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
    const Type* source_object = source_type;
    if (source_object && source_object->is_reference) {
        source_object = source_object->base;
    }
    if (reg == EAX && target_type && target_type->kind == TYPE_BOOL &&
        source_object && source_object->kind == TYPE_PTR &&
        source_object->cxx_is_member_pointer) {
        if (type_is_cxx_member_function_pointer(source_object)) {
            /* The expression value for the ABI aggregate is its address; its
             * first word is zero exactly for a null member-function pointer. */
            emit_mov_reg_mem(mod, reg, reg, 0);
            emit_test_reg_reg(mod, reg, reg);
        } else {
            /* Itanium data-member pointers use -1 as their null value. */
            emit_cmp_reg_imm(mod, reg, -1);
        }
        emit_setcc(mod, CC_NE, reg);
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xB6);
        emit_byte(mod, modrm(3, reg, reg));
        return;
    }
    if (reg == EAX && target_type && target_type->kind == TYPE_BOOL &&
        gen_is_integer64(source_type)) {
        emit_or_reg_reg(mod, EAX, EDX);
    }
    emit_normalize_atomic_value(mod, reg, target_type);
}

static void gen_expr(Module* mod, Expr* expr) {
    const Decl* caller = NULL;
    const Decl* callee = NULL;
    const ExprList* arguments = NULL;
    SourceLoc call_location = {NULL, 0, 0};
    uint32_t start;
    size_t parent_index_plus_one;
    size_t inline_call_index;
    uint32_t end;
    if (!expr) return;
    if (expr && gen_is_integer64(expr->type)) {
        gen_expr64_pair(mod, expr);
        return;
    }
    if (!g_opts.debug_info ||
        !rcc_optimize_inline_debug_info(expr, &caller, &callee,
                                        &call_location, &arguments)) {
        gen_expr_impl(mod, expr);
        if (expr && expr->type && expr->type->is_bit_precise &&
            expr->type->bit_width < 32u) {
            uint32_t mask =
                (UINT32_C(1) << expr->type->bit_width) - UINT32_C(1);
            emit_and_reg_imm(mod, EAX, mask);
            if (!expr->type->is_unsigned) {
                uint8_t shift = (uint8_t)(32u - expr->type->bit_width);
                emit_shl_reg_imm(mod, EAX, shift);
                emit_sar_reg_imm(mod, EAX, shift);
            }
        }
        return;
    }
    start = code_offset(mod);
    parent_index_plus_one = mod->debug_inline_active_call_plus_one;
    inline_call_index = module_begin_debug_inline_call(
        mod, caller, callee, arguments, call_location, start,
        parent_index_plus_one);
    if (inline_call_index == SIZE_MAX) {
        rcc_fatal("DWARF inline-call index exceeds addressable range");
    }
    mod->debug_inline_active_call_plus_one = inline_call_index + 1u;
    gen_expr_impl(mod, expr);
    if (expr && expr->type && expr->type->is_bit_precise &&
        expr->type->bit_width < 32u) {
        uint32_t mask =
            (UINT32_C(1) << expr->type->bit_width) - UINT32_C(1);
        emit_and_reg_imm(mod, EAX, mask);
        if (!expr->type->is_unsigned) {
            uint8_t shift = (uint8_t)(32u - expr->type->bit_width);
            emit_shl_reg_imm(mod, EAX, shift);
            emit_sar_reg_imm(mod, EAX, shift);
        }
    }
    end = code_offset(mod);
    mod->debug_inline_active_call_plus_one = parent_index_plus_one;
    module_end_debug_inline_call(mod, inline_call_index, end);
}

static void gen_expr_impl(Module* mod, Expr* expr) {
    if (!expr) return;
    if (gen_is_integer64(expr->type)) {
        gen_expr64_pair(mod, expr);
        return;
    }
    if (type_is_cxx_member_function_pointer(expr->type) &&
        (expr->is_cxx_nullptr ||
         (expr->type && expr->type->kind == TYPE_NULLPTR))) {
        if (expr->aggregate_offset >= 0) {
            rcc_error(expr->loc,
                      "null member-function pointer has no automatic result slot");
            return;
        }
        emit_mov_reg_imm(mod, EAX, 0u);
        emit_mov_mem_reg(mod, EBP, expr->aggregate_offset, EAX);
        emit_mov_mem_reg(mod, EBP, expr->aggregate_offset + 4, EAX);
        emit_byte(mod, 0x8D); /* LEA EAX, [EBP+disp32] */
        emit_byte(mod, modrm(2, EAX, EBP));
        emit_dword(mod, (uint32_t)expr->aggregate_offset);
        return;
    }
    gen_expr_raw(mod, expr);
    if (type_is_cxx_member_function_pointer(expr->type) &&
        expr->cxx_member_pointer_adjustment_valid) {
        int null_label = new_label();
        int done_label = new_label();
        if (expr->aggregate_offset >= 0) {
            rcc_error(expr->loc,
                      "converted member-function pointer has no automatic result slot");
            return;
        }
        emit_mov_reg_reg(mod, ECX, EAX);
        emit_mov_reg_mem(mod, EAX, ECX, 0);
        emit_mov_reg_mem(mod, EDX, ECX, 4);
        emit_test_reg_reg(mod, EAX, EAX);
        emit_jcc_label(mod, CC_E, null_label);
        if (expr->cxx_member_pointer_adjustment != 0) {
            emit_add_reg_imm(mod, EDX,
                             expr->cxx_member_pointer_adjustment);
        }
        emit_jmp_label(mod, done_label);
        emit_label(mod, null_label);
        emit_mov_reg_imm(mod, EDX, 0u);
        emit_label(mod, done_label);
        emit_mov_mem_reg(mod, EBP, expr->aggregate_offset, EAX);
        emit_mov_mem_reg(mod, EBP, expr->aggregate_offset + 4, EDX);
        emit_byte(mod, 0x8D); /* LEA EAX, [EBP+disp32] */
        emit_byte(mod, modrm(2, EAX, EBP));
        emit_dword(mod, (uint32_t)expr->aggregate_offset);
    } else if (expr->cxx_member_pointer_adjustment_valid &&
        expr->type && expr->type->kind == TYPE_PTR &&
        expr->type->cxx_is_member_pointer &&
        !type_is_cxx_member_function_pointer(expr->type)) {
        int done_label = new_label();
        emit_cmp_reg_imm(mod, EAX, -1);
        emit_jcc_label(mod, CC_E, done_label);
        if (expr->cxx_member_pointer_adjustment != 0) {
            emit_add_reg_imm(mod, EAX,
                             expr->cxx_member_pointer_adjustment);
        }
        emit_label(mod, done_label);
    }
    if (expr->cxx_virtual_base_adjustment &&
        !expr->cxx_virtual_base_member_access) {
        int end_label;
        if (!expr->cxx_virtual_base_source_class ||
            expr->cxx_virtual_base_pointer_offset < 0 ||
            expr->cxx_virtual_base_index < 0 ||
            expr->cxx_virtual_base_index >=
                expr->cxx_virtual_base_source_class->virtual_base_count) {
            rcc_error(expr->loc,
                      "virtual-base conversion has incomplete vbtable metadata");
            return;
        }
        end_label = new_label();
        emit_test_reg_reg(mod, EAX, EAX);
        emit_jcc_label(mod, CC_E, end_label);
        emit_mov_reg_mem(mod, EDX, EAX,
                         expr->cxx_virtual_base_pointer_offset);
        emit_mov_reg_mem(mod, ECX, EDX,
                         expr->cxx_virtual_base_index * 4);
        emit_add_reg_reg(mod, EAX, ECX);
        if (expr->cxx_virtual_base_nested_adjustment != 0) {
            emit_add_reg_imm(mod, EAX,
                             expr->cxx_virtual_base_nested_adjustment);
        }
        emit_label(mod, end_label);
    } else if (expr->cxx_dynamic_cast_runtime) {
        gen_cxx_dynamic_cast_runtime32(mod, expr);
    } else if (expr->cxx_dynamic_cast_checked) {
        int fail_label;
        int done_label;
        if (!expr->cxx_dynamic_cast_vtable_symbol) {
            rcc_error(expr->loc,
                      "dynamic_cast has no validated vtable identity");
            return;
        }
        fail_label = new_label();
        done_label = new_label();
        emit_test_reg_reg(mod, EAX, EAX);
        emit_jcc_label(mod, CC_E, fail_label);
        emit_push_reg(mod, EAX);
        gen_symbol_address(mod, expr->cxx_dynamic_cast_vtable_symbol, 0u);
        emit_mov_reg_reg(mod, EDX, EAX);
        emit_pop_reg(mod, EAX);
        emit_mov_reg_mem(mod, ECX, EAX, 0);
        emit_cmp_reg_reg(mod, ECX, EDX);
        emit_jcc_label(mod, CC_NE, fail_label);
        if (expr->cxx_pointer_adjustment_valid &&
            expr->cxx_pointer_adjustment != 0) {
            emit_add_reg_imm(mod, EAX, expr->cxx_pointer_adjustment);
        }
        emit_jmp_label(mod, done_label);
        emit_label(mod, fail_label);
        emit_xor_reg_reg(mod, EAX, EAX);
        emit_label(mod, done_label);
    } else if (expr->kind != EXPR_CXX_MEMBER_PTR_DOT &&
        expr->kind != EXPR_CXX_MEMBER_PTR_ARROW &&
        expr->cxx_pointer_adjustment_valid &&
        expr->cxx_pointer_adjustment != 0) {
        if (expr->type && expr->type->kind == TYPE_PTR &&
            !expr->type->is_reference) {
            int end_label = new_label();
            emit_test_reg_reg(mod, EAX, EAX);
            emit_jcc_label(mod, CC_E, end_label);
            emit_add_reg_imm(mod, EAX, expr->cxx_pointer_adjustment);
            emit_label(mod, end_label);
        } else {
            emit_add_reg_imm(mod, EAX, expr->cxx_pointer_adjustment);
        }
    }
    if (type_is_integer(expr->type) || expr->type->kind == TYPE_ENUM) {
        emit_normalize_atomic_value(mod, EAX, expr->type);
    }
}

typedef struct CleanupCodegen {
    Expr* expression;
    CxxCleanupPlan* plan;
    Decl* temporary_owner;
    Decl* guard_abort;
    struct CleanupCodegen* previous;
    Decl* declaration;
    int exception_frame_offset;
    bool exception_registered;
} CleanupCodegen;

/* Cleanups below this marker belong to the protected try/catch region.  A
 * direct same-function throw can run them before longjmp; calls in such a
 * region are rejected by sema because a callee cannot see this compiler-side
 * cleanup stack. */
static CleanupCodegen* active_cxx_exception_cleanup_marker = NULL;
static bool cxx_exception_cleanup_registration_enabled = false;
static int active_cxx_exception_cleanup_frame_offset = INT_MAX;

static void gen_cxx_cleanup_plan32(Module* mod, CxxCleanupPlan* plan,
                                  bool unregister, int frame_offset);
static bool gen_cxx_cleanup_plan_registerable32(
    const CxxCleanupPlan* plan);
static void gen_cxx_exception_register_plan32(
    Module* mod, CxxCleanupPlan* plan, int frame_offset);
static void gen_cxx_exception_unregister_plan32(
    Module* mod, CxxCleanupPlan* plan, int frame_offset);

static void gen_cxx_exception_unregister_cleanup32(
    Module* mod, const CleanupCodegen* cleanup);
static void gen_cxx_exception_call32(Module* mod, const char* name);
static void gen_cxx_guard_exception_unregister32(
    Module* mod, const CleanupCodegen* cleanup);
static void gen_cxx_exception_cleanups_until_throw32(
    Module* mod, CleanupCodegen* marker);

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

static void gen_call_temporary_cleanup32(Module* mod,
                                         CleanupCodegen* cleanup) {
    int done_label = -1;
    if (cleanup->guard_abort) {
        if (cleanup->exception_registered) {
            gen_cxx_guard_exception_unregister32(mod, cleanup);
        }
        gen_decl_storage_address32(mod, cleanup->guard_abort);
        emit_push_reg(mod, EAX);
        gen_cxx_exception_call32(mod, "__cxa_guard_abort");
        emit_add_reg_imm(mod, ESP, 4);
        return;
    }
    if (cleanup->temporary_owner) {
        done_label = new_label();
        emit_mov_reg_mem(mod, EAX, EBP,
                         cleanup->temporary_owner->var_offset);
        emit_test_reg_reg(mod, EAX, EAX);
        emit_jcc_label(mod, CC_E, done_label);
    }
    if (cleanup->plan) {
        gen_cxx_cleanup_plan32(mod, cleanup->plan,
                               cleanup->exception_registered,
                               cleanup->exception_frame_offset);
    } else {
        if (cleanup->exception_registered) {
            gen_cxx_exception_unregister_cleanup32(mod, cleanup);
        }
        gen_expr(mod, cleanup->expression);
    }
    if (cleanup->temporary_owner) {
        emit_mov_reg_imm(mod, EAX, 0u);
        emit_mov_mem_reg(mod, EBP,
                         cleanup->temporary_owner->var_offset, EAX);
        emit_label(mod, done_label);
    }
}

static void gen_clear_call_temporary_owners32(Module* mod) {
    for (ExprList* argument = active_call_temporary_owners; argument;
         argument = argument->cxx_temporary_next) {
        if (!argument->cxx_temporary_owner) continue;
        emit_mov_reg_imm(mod, EAX, 0u);
        emit_mov_mem_reg(mod, EBP,
                         argument->cxx_temporary_owner->var_offset, EAX);
    }
}

static void gen_cleanups_until(Module* mod, CleanupCodegen* marker) {
    for (CleanupCodegen* item = active_cleanups; item && item != marker;
         item = item->previous) {
        gen_call_temporary_cleanup32(mod, item);
    }
}

static bool gen_cleanup_count(Module* mod, unsigned count) {
    CleanupCodegen* item = active_cleanups;
    while (item && count > 0u) {
        gen_call_temporary_cleanup32(mod, item);
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

static void gen_cxx_exception_cleanups_until_throw32(
    Module* mod, CleanupCodegen* marker) {
    for (CleanupCodegen* item = active_cleanups; item && item != marker;
         item = item->previous) {
        /* Registered destructor cleanups are consumed by the runtime before
         * it longjmps.  Inline-only wrapper cleanups still need code here. */
        if (!item->exception_registered) {
            gen_call_temporary_cleanup32(mod, item);
        }
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

static void gen_cxx_exception_payload_address32(
    Module* mod, Stmt* stmt, CxxCatch* handler) {
    int payload_ready;

    gen_cxx_exception_frame_address32(mod, stmt->try_frame_offset);
    emit_mov_reg_mem(mod, EDX, EAX, 28);
    if (!handler->compatible_tag_count) return;
    payload_ready = new_label();
    gen_cxx_exception_frame_address32(mod, stmt->try_frame_offset);
    emit_mov_reg_mem(mod, EAX, EAX, 32);
    for (size_t index = 0u; index < handler->compatible_tag_count; ++index) {
        int next_tag = new_label();
        emit_cmp_reg_imm(
            mod, EAX, (int32_t)(uint32_t)handler->compatible_tags[index]);
        emit_jcc_label(mod, CC_NE, next_tag);
        if (handler->compatible_tag_offsets &&
            handler->compatible_tag_offsets[index] != 0) {
            emit_add_reg_imm(mod, EDX,
                             handler->compatible_tag_offsets[index]);
        }
        emit_jmp_label(mod, payload_ready);
        emit_label(mod, next_tag);
    }
    emit_label(mod, payload_ready);
}

static void gen_cxx_exception_call32(Module* mod, const char* name) {
    uint32_t call_offset;
    emit_call_rel32(mod, 0u);
    call_offset = code_offset(mod) - 4u;
    add_func_call_ref(name, call_offset);
}

static void gen_cxx_guard_exception_register32(
    Module* mod, CleanupCodegen* cleanup) {
    if (!mod || !cleanup || !cleanup->guard_abort) {
        return;
    }
    gen_decl_storage_address32(mod, cleanup->guard_abort);
    emit_push_reg(mod, EAX); /* guard object */
    gen_symbol_address(mod, "__cxa_guard_abort", 0u);
    emit_push_reg(mod, EAX); /* cleanup callback */
    gen_cxx_exception_call32(
        mod, "rin_cpp_exception_register_current_cleanup");
    emit_add_reg_imm(mod, ESP, 8);
    cleanup->exception_registered = true;
}

static void gen_cxx_guard_exception_unregister32(
    Module* mod, const CleanupCodegen* cleanup) {
    if (!mod || !cleanup || !cleanup->guard_abort) {
        return;
    }
    gen_decl_storage_address32(mod, cleanup->guard_abort);
    emit_push_reg(mod, EAX); /* guard object */
    gen_symbol_address(mod, "__cxa_guard_abort", 0u);
    emit_push_reg(mod, EAX); /* cleanup callback */
    gen_cxx_exception_call32(
        mod, "rin_cpp_exception_unregister_current_cleanup");
    emit_add_reg_imm(mod, ESP, 8);
}

static Decl* gen_cxx_cleanup_destructor(const CleanupCodegen* cleanup) {
    Expr* expression = cleanup ? cleanup->expression : NULL;
    Expr* function = expression && expression->kind == EXPR_CALL
        ? expression->call_func : NULL;
    Decl* declaration = function && function->kind == EXPR_IDENT
        ? function->ident_decl : NULL;
    return declaration && declaration->func_is_cxx_destructor
        ? declaration : NULL;
}

static void gen_cxx_exception_unregister_cleanup32(
    Module* mod, const CleanupCodegen* cleanup) {
    Decl* destructor = gen_cxx_cleanup_destructor(cleanup);
    ExprList* arguments = cleanup && cleanup->expression
        ? cleanup->expression->call_args : NULL;
    Expr* address = arguments ? arguments->expr : NULL;
    if (!destructor || !address || address->kind != EXPR_ADDR ||
        !address->unary_operand) {
        rcc_error(cleanup && cleanup->expression
                      ? cleanup->expression->loc
                      : (SourceLoc){"<exception-cleanup>", 0, 0},
                  "registered C++ exception cleanup metadata is incomplete");
        return;
    }
    if (cleanup->exception_frame_offset == INT_MAX) {
        gen_lvalue(mod, address->unary_operand);
        emit_push_reg(mod, EAX); /* object */
        gen_symbol_address(mod, decl_link_name(destructor), 0u);
        emit_push_reg(mod, EAX); /* destructor */
        gen_cxx_exception_call32(
            mod, "rin_cpp_exception_unregister_current_cleanup");
        emit_add_reg_imm(mod, ESP, 8);
        return;
    }
    gen_lvalue(mod, address->unary_operand);
    emit_push_reg(mod, EAX); /* object */
    gen_symbol_address(mod, decl_link_name(destructor), 0u);
    emit_push_reg(mod, EAX); /* destructor */
    gen_cxx_exception_frame_address32(mod, cleanup->exception_frame_offset);
    emit_push_reg(mod, EAX); /* frame */
    gen_cxx_exception_call32(mod, "rin_cpp_exception_unregister_cleanup");
    emit_add_reg_imm(mod, ESP, 12);
}

static void gen_cxx_exception_register_cleanup32(
    Module* mod, CleanupCodegen* cleanup) {
    Decl* destructor = gen_cxx_cleanup_destructor(cleanup);
    ExprList* arguments = cleanup && cleanup->expression
        ? cleanup->expression->call_args : NULL;
    Expr* address = arguments ? arguments->expr : NULL;
    if (!destructor || !address || address->kind != EXPR_ADDR ||
        !address->unary_operand) {
        return;
    }
    if (cleanup->exception_frame_offset == INT_MAX) {
        gen_lvalue(mod, address->unary_operand);
        emit_push_reg(mod, EAX); /* object */
        gen_symbol_address(mod, decl_link_name(destructor), 0u);
        emit_push_reg(mod, EAX); /* destructor */
        gen_cxx_exception_call32(
            mod, "rin_cpp_exception_register_current_cleanup");
        emit_add_reg_imm(mod, ESP, 8);
        cleanup->exception_registered = true;
        return;
    }
    gen_lvalue(mod, address->unary_operand);
    emit_push_reg(mod, EAX); /* object */
    gen_symbol_address(mod, decl_link_name(destructor), 0u);
    emit_push_reg(mod, EAX); /* destructor */
    gen_cxx_exception_frame_address32(mod, cleanup->exception_frame_offset);
    emit_push_reg(mod, EAX); /* frame */
    gen_cxx_exception_call32(mod, "rin_cpp_exception_register_cleanup");
    emit_add_reg_imm(mod, ESP, 12);
    cleanup->exception_registered = true;
}

static bool gen_cxx_cleanup_plan_registerable32(
    const CxxCleanupPlan* plan) {
    for (; plan; plan = plan->next) {
        if (plan->kind == CXX_CLEANUP_ARRAY_LOOP) {
            if (!gen_cxx_cleanup_plan_registerable32(plan->body)) return false;
        } else if (plan->kind == CXX_CLEANUP_CONDITIONAL) {
            if (!plan->expression ||
                !gen_cxx_cleanup_plan_registerable32(plan->body) ||
                !gen_cxx_cleanup_plan_registerable32(
                    plan->alternate_body)) {
                return false;
            }
        } else {
            Expr* function = plan->expression &&
                    plan->expression->kind == EXPR_CALL
                ? plan->expression->call_func : NULL;
            ExprList* arguments = plan->expression
                ? plan->expression->call_args : NULL;
            Expr* address = arguments ? arguments->expr : NULL;
            Decl* destructor = function && function->kind == EXPR_IDENT
                ? function->ident_decl : NULL;
            if (!destructor || !destructor->func_is_cxx_destructor ||
                !address || address->kind != EXPR_ADDR ||
                !address->unary_operand) {
                return false;
            }
        }
    }
    return true;
}

static void gen_cxx_cleanup_plan_execute_reverse32(
    Module* mod, CxxCleanupPlan* plan, bool unregister, int frame_offset) {
    size_t count = 0u;
    size_t index = 0u;
    CxxCleanupPlan** items;
    for (CxxCleanupPlan* item = plan; item; item = item->next) ++count;
    if (!count) return;
    if (count > SIZE_MAX / sizeof(*items)) {
        rcc_fatal("C++ cleanup plan is too large");
    }
    items = rcc_alloc(count * sizeof(*items));
    for (CxxCleanupPlan* item = plan; item; item = item->next) {
        items[index++] = item;
    }
    while (index > 0u) {
        CxxCleanupPlan* item = items[--index];
        if (item->kind == CXX_CLEANUP_ARRAY_LOOP) {
            int loop;
            int done;
            if (!item->index_decl || item->index_decl->var_offset >= 0 ||
                item->element_count < 0) {
                rcc_error(item->expression ? item->expression->loc
                                           : (SourceLoc){"<array-cleanup>", 0, 0},
                          "automatic array cleanup loop has no frame index");
                continue;
            }
            if (item->element_count == 0) continue;
            loop = new_label();
            done = new_label();
            emit_mov_reg_imm(mod, EAX, (uint32_t)item->element_count);
            emit_mov_mem_reg(mod, EBP, item->index_decl->var_offset, EAX);
            emit_label(mod, loop);
            emit_mov_reg_mem(mod, EAX, EBP, item->index_decl->var_offset);
            emit_test_reg_reg(mod, EAX, EAX);
            emit_jcc_label(mod, CC_E, done);
            emit_sub_reg_imm(mod, EAX, 1);
            emit_mov_mem_reg(mod, EBP, item->index_decl->var_offset, EAX);
            gen_cxx_cleanup_plan_execute_reverse32(
                mod, item->body, unregister, frame_offset);
            emit_jmp_label(mod, loop);
            emit_label(mod, done);
        } else if (item->kind == CXX_CLEANUP_CONDITIONAL) {
            int then_label = new_label();
            int done_label = new_label();
            if (!item->expression) {
                rcc_error((SourceLoc){"<conditional-cleanup>", 0, 0},
                          "conditional C++ cleanup has no selector");
                continue;
            }
            gen_expr(mod, item->expression);
            emit_test_scalar_value(mod, item->expression->type);
            emit_jcc_label(mod, CC_E, then_label);
            gen_cxx_cleanup_plan_execute_reverse32(
                mod, item->body, unregister, frame_offset);
            emit_jmp_label(mod, done_label);
            emit_label(mod, then_label);
            gen_cxx_cleanup_plan_execute_reverse32(
                mod, item->alternate_body, unregister, frame_offset);
            emit_label(mod, done_label);
        } else {
            if (unregister) {
                CleanupCodegen cleanup = {0};
                cleanup.expression = item->expression;
                cleanup.exception_frame_offset = frame_offset;
                gen_cxx_exception_unregister_cleanup32(mod, &cleanup);
            }
            gen_expr(mod, item->expression);
        }
    }
    rcc_free(items);
}

static void gen_cxx_cleanup_plan32(Module* mod, CxxCleanupPlan* plan,
                                   bool unregister, int frame_offset) {
    gen_cxx_cleanup_plan_execute_reverse32(
        mod, plan, unregister, frame_offset);
}

static void gen_cxx_cleanup_plan_register_forward32(
    Module* mod, CxxCleanupPlan* plan, int frame_offset) {
    for (; plan; plan = plan->next) {
        if (plan->kind == CXX_CLEANUP_ARRAY_LOOP) {
            int loop;
            int done;
            if (!plan->index_decl || plan->index_decl->var_offset >= 0 ||
                plan->element_count < 0) {
                rcc_error(plan->expression ? plan->expression->loc
                                           : (SourceLoc){"<array-cleanup>", 0, 0},
                          "automatic array cleanup loop has no frame index");
                continue;
            }
            loop = new_label();
            done = new_label();
            emit_mov_reg_imm(mod, EAX, 0u);
            emit_mov_mem_reg(mod, EBP, plan->index_decl->var_offset, EAX);
            emit_label(mod, loop);
            emit_mov_reg_mem(mod, EAX, EBP, plan->index_decl->var_offset);
            emit_cmp_reg_imm(mod, EAX, plan->element_count);
            emit_jcc_label(mod, CC_AE, done);
            gen_cxx_cleanup_plan_register_forward32(
                mod, plan->body, frame_offset);
            emit_mov_reg_mem(mod, EAX, EBP, plan->index_decl->var_offset);
            emit_add_reg_imm(mod, EAX, 1);
            emit_mov_mem_reg(mod, EBP, plan->index_decl->var_offset, EAX);
            emit_jmp_label(mod, loop);
            emit_label(mod, done);
        } else if (plan->kind == CXX_CLEANUP_CONDITIONAL) {
            int then_label = new_label();
            int done_label = new_label();
            if (!plan->expression) {
                rcc_error((SourceLoc){"<conditional-cleanup>", 0, 0},
                          "conditional C++ cleanup has no selector");
                continue;
            }
            gen_expr(mod, plan->expression);
            emit_test_scalar_value(mod, plan->expression->type);
            emit_jcc_label(mod, CC_E, then_label);
            gen_cxx_cleanup_plan_register_forward32(
                mod, plan->body, frame_offset);
            emit_jmp_label(mod, done_label);
            emit_label(mod, then_label);
            gen_cxx_cleanup_plan_register_forward32(
                mod, plan->alternate_body, frame_offset);
            emit_label(mod, done_label);
        } else {
            CleanupCodegen cleanup = {0};
            cleanup.expression = plan->expression;
            cleanup.exception_frame_offset = frame_offset;
            gen_cxx_exception_register_cleanup32(mod, &cleanup);
        }
    }
}

static void gen_cxx_exception_register_plan32(
    Module* mod, CxxCleanupPlan* plan, int frame_offset) {
    gen_cxx_cleanup_plan_register_forward32(mod, plan, frame_offset);
}

static void gen_cxx_exception_unregister_plan_forward32(
    Module* mod, CxxCleanupPlan* plan, int frame_offset) {
    for (; plan; plan = plan->next) {
        if (plan->kind == CXX_CLEANUP_ARRAY_LOOP) {
            int loop;
            int done;
            if (!plan->index_decl || plan->index_decl->var_offset >= 0 ||
                plan->element_count < 0) {
                rcc_error(plan->expression ? plan->expression->loc
                                           : (SourceLoc){"<array-cleanup>", 0, 0},
                          "automatic array cleanup loop has no frame index");
                return;
            }
            if (plan->element_count == 0) continue;
            loop = new_label();
            done = new_label();
            emit_mov_reg_imm(mod, EAX, 0u);
            emit_mov_mem_reg(mod, EBP, plan->index_decl->var_offset, EAX);
            emit_label(mod, loop);
            emit_mov_reg_mem(mod, EAX, EBP,
                             plan->index_decl->var_offset);
            emit_cmp_reg_imm(mod, EAX, plan->element_count);
            emit_jcc_label(mod, CC_AE, done);
            gen_cxx_exception_unregister_plan_forward32(
                mod, plan->body, frame_offset);
            emit_mov_reg_mem(mod, EAX, EBP,
                             plan->index_decl->var_offset);
            emit_add_reg_imm(mod, EAX, 1);
            emit_mov_mem_reg(mod, EBP, plan->index_decl->var_offset, EAX);
            emit_jmp_label(mod, loop);
            emit_label(mod, done);
        } else if (plan->kind == CXX_CLEANUP_CONDITIONAL) {
            int then_label = new_label();
            int done_label = new_label();
            if (!plan->expression) {
                rcc_error((SourceLoc){"<conditional-cleanup>", 0, 0},
                          "conditional C++ cleanup has no selector");
                return;
            }
            gen_expr(mod, plan->expression);
            emit_test_scalar_value(mod, plan->expression->type);
            emit_jcc_label(mod, CC_E, then_label);
            gen_cxx_exception_unregister_plan_forward32(
                mod, plan->body, frame_offset);
            emit_jmp_label(mod, done_label);
            emit_label(mod, then_label);
            gen_cxx_exception_unregister_plan_forward32(
                mod, plan->alternate_body, frame_offset);
            emit_label(mod, done_label);
        } else {
            CleanupCodegen cleanup = {0};
            cleanup.expression = plan->expression;
            cleanup.exception_frame_offset = frame_offset;
            gen_cxx_exception_unregister_cleanup32(mod, &cleanup);
        }
    }
}

static void gen_cxx_exception_unregister_plan32(
    Module* mod, CxxCleanupPlan* plan, int frame_offset) {
    gen_cxx_exception_unregister_plan_forward32(mod, plan, frame_offset);
}

static void codegen_push_call_temporary_cleanup32(
    Module* mod, Decl* temporary_owner, CxxCleanupPlan* plan) {
    CleanupCodegen* cleanup;
    if (!mod || !plan) return;
    cleanup = rcc_alloc(sizeof(*cleanup));
    memset(cleanup, 0, sizeof(*cleanup));
    cleanup->plan = plan;
    cleanup->temporary_owner = temporary_owner;
    cleanup->previous = active_cleanups;
    cleanup->exception_frame_offset =
        active_cxx_exception_cleanup_frame_offset;
    active_cleanups = cleanup;
    if (cxx_exception_cleanup_registration_enabled &&
        active_cxx_exception_cleanup_frame_offset != INT_MAX &&
        gen_cxx_cleanup_plan_registerable32(cleanup->plan)) {
        gen_cxx_exception_register_plan32(
            mod, cleanup->plan, active_cxx_exception_cleanup_frame_offset);
        cleanup->exception_registered = true;
    }
}

static void gen_transfer_by_value_argument_cleanups32(
    Module* mod, ExprList* arguments) {
    for (ExprList* argument = arguments; argument;
         argument = argument->next) {
        CleanupCodegen* cleanup;
        if (!argument->cxx_temporary_is_parameter_object ||
            !argument->cxx_temporary_owner) {
            continue;
        }
        for (cleanup = active_cleanups; cleanup;
             cleanup = cleanup->previous) {
            if (cleanup->temporary_owner !=
                    argument->cxx_temporary_owner ||
                cleanup->plan != argument->cxx_temporary_cleanups) {
                continue;
            }
            if (cleanup->exception_registered) {
                gen_cxx_exception_unregister_plan32(
                    mod, cleanup->plan,
                    cleanup->exception_frame_offset);
                cleanup->exception_registered = false;
            }
            break;
        }
        emit_mov_reg_imm(mod, EAX, 0u);
        emit_mov_mem_reg(mod, EBP,
                         argument->cxx_temporary_owner->var_offset, EAX);
    }
}

static void codegen_push_explicit_destructor_cleanup32(
    Module* mod, Decl* temporary_owner, CxxCleanupPlan* plan) {
    CleanupCodegen* cleanup;
    codegen_push_call_temporary_cleanup32(mod, temporary_owner, plan);
    cleanup = active_cleanups;
    if (!cleanup || cleanup->plan != plan || cleanup->exception_registered ||
        !gen_cxx_cleanup_plan_registerable32(plan)) {
        return;
    }
    /* The explicit destructor may be in a callee while the active handler
     * belongs to its caller.  Register against the runtime's current frame,
     * not only a try-frame known in this function at code-generation time. */
    cleanup->exception_frame_offset = INT_MAX;
    gen_cxx_exception_register_plan32(mod, plan, INT_MAX);
    cleanup->exception_registered = true;
}

static void gen_cxx_exception_unwind_cleanup32(Module* mod) {
    gen_cxx_exception_cleanups_until_throw32(
        mod, active_cxx_exception_cleanup_marker);
}

static void gen_cxx_typeid32(Module* mod, Expr* expr) {
    int bad_typeid_label;
    int done_label;
    if (!mod || !expr || !expr->cxx_typeid_dynamic ||
        !expr->cxx_typeid_operand) {
        rcc_error(expr ? expr->loc : (SourceLoc){"<typeid>", 0, 0},
                  "typeid has no validated dynamic operand");
        return;
    }
    bad_typeid_label = new_label();
    done_label = new_label();
    gen_lvalue(mod, expr->cxx_typeid_operand);
    emit_test_reg_reg(mod, EAX, EAX);
    emit_jcc_label(mod, CC_E, bad_typeid_label);
    emit_mov_reg_mem(mod, EAX, EAX, 0);  /* source subobject vptr */
    emit_mov_reg_mem(mod, EAX, EAX, -4); /* vptr[-1] metadata */
    emit_mov_reg_mem(mod, EAX, EAX, 0);  /* most-derived typeinfo */
    emit_jmp_label(mod, done_label);
    emit_label(mod, bad_typeid_label);
    gen_cxx_exception_unwind_cleanup32(mod);
    emit_mov_reg_imm(mod, EAX, 0u);
    emit_push_reg(mod, EAX);              /* no object payload */
    emit_mov_reg_imm(mod, EAX,
                     (uint32_t)RCC_CXX_BAD_TYPEID_TYPE_TAG);
    emit_push_reg(mod, EAX);
    gen_cxx_exception_call32(mod, "rin_cpp_exception_throw");
    emit_add_reg_imm(mod, ESP, 8);
    emit_label(mod, done_label);
}

/* The expression and lvalue paths both need the same complete-object RTTI
 * search.  Keep it in one lowering routine so a reference result cannot
 * accidentally bypass the runtime relationship check.  A pointer result
 * follows the normal null-on-failure rule; a reference result transfers
 * through the real RinOS exception ABI because C++ references cannot encode
 * a failed cast as null. */
static void gen_cxx_dynamic_cast_runtime32(Module* mod, Expr* expr) {
    int null_label = new_label();
    int not_found_label = new_label();
    int found_label = new_label();
    int bad_cast_label = new_label();
    int done_label = new_label();
    bool reference_result = expr && expr->type && expr->type->is_reference;
    if (!expr || !expr->cxx_dynamic_cast_typeinfo_symbol) {
        rcc_error(expr ? expr->loc : (SourceLoc){"<dynamic-cast>", 0, 0},
                  "dynamic_cast has no validated target typeinfo");
        return;
    }

    emit_test_reg_reg(mod, EAX, EAX);
    emit_jcc_label(mod, CC_E, reference_result ? bad_cast_label : null_label);
    emit_mov_reg_mem(mod, ECX, EAX, 0); /* source subobject vptr */
    emit_mov_reg_mem(mod, EDX, ECX, -4); /* vptr[-1] RTTI metadata */
    emit_mov_reg_mem(mod, ECX, EDX, 4); /* source offset */
    emit_sub_reg_reg(mod, EAX, ECX);   /* complete object address */
    emit_push_reg(mod, EAX);
    emit_mov_reg_mem(mod, ECX, EDX, 8); /* target-entry count */
    emit_push_reg(mod, ECX);
    emit_add_reg_imm(mod, EDX, 12);     /* first type/offset pair */
    emit_push_reg(mod, EDX);            /* preserve table across lookup */
    gen_symbol_address(mod, expr->cxx_dynamic_cast_typeinfo_symbol, 0u);
    emit_mov_reg_reg(mod, ECX, EAX);    /* target typeinfo identity */
    emit_pop_reg(mod, EDX);
    emit_mov_reg_mem(mod, EAX, ESP, 0);
    emit_test_reg_reg(mod, EAX, EAX);
    emit_jcc_label(mod, CC_E, not_found_label);

    {
        int loop_label = new_label();
        emit_label(mod, loop_label);
        emit_mov_reg_mem(mod, EAX, EDX, 0);
        emit_cmp_reg_reg(mod, EAX, ECX);
        emit_jcc_label(mod, CC_E, found_label);
        emit_add_reg_imm(mod, EDX, 8);
        emit_mov_reg_mem(mod, EAX, ESP, 0);
        emit_dec_reg(mod, EAX);
        emit_mov_mem_reg(mod, ESP, 0, EAX);
        emit_test_reg_reg(mod, EAX, EAX);
        emit_jcc_label(mod, CC_NE, loop_label);
    }
    emit_jmp_label(mod, not_found_label);

    emit_label(mod, found_label);
    emit_mov_reg_mem(mod, EAX, EDX, 4); /* target offset */
    emit_mov_reg_mem(mod, ECX, ESP, 4); /* complete object */
    emit_add_reg_reg(mod, EAX, ECX);
    emit_add_reg_imm(mod, ESP, 8);
    emit_jmp_label(mod, done_label);

    emit_label(mod, not_found_label);
    emit_add_reg_imm(mod, ESP, 8);
    if (reference_result) {
        emit_jmp_label(mod, bad_cast_label);
    } else {
        emit_xor_reg_reg(mod, EAX, EAX);
        emit_jmp_label(mod, done_label);
    }

    emit_label(mod, null_label);
    emit_xor_reg_reg(mod, EAX, EAX);
    emit_jmp_label(mod, done_label);

    emit_label(mod, bad_cast_label);
    gen_cxx_exception_unwind_cleanup32(mod);
    emit_mov_reg_imm(mod, EAX, 0u); /* no object payload for bad_cast */
    emit_push_reg(mod, EAX);         /* value */
    emit_mov_reg_imm(mod, EAX, (uint32_t)RCC_CXX_BAD_CAST_TYPE_TAG);
    emit_push_reg(mod, EAX);         /* type */
    gen_cxx_exception_call32(mod, "rin_cpp_exception_throw");
    emit_add_reg_imm(mod, ESP, 8);
    emit_label(mod, done_label);
}

static void gen_cxx_exception_release_frame32(Module* mod, int offset) {
    if (offset == INT_MAX) return;
    gen_cxx_exception_frame_address32(mod, offset);
    emit_push_reg(mod, EAX);
    gen_cxx_exception_call32(mod, "rin_cpp_exception_release_frame");
    emit_add_reg_imm(mod, ESP, 4);
}

static uint32_t gen_cxx_exception_type_tag32(const Type* type) {
    return (uint32_t)rcc_cxx_exception_type_tag(type);
}

static Decl* gen_cxx_exception_object_destructor32(Type* type) {
    CxxClass* cls = type ? type->cxx_class : NULL;
    Decl* destructor = cls && cls->destructor_method
        ? cls->destructor_method->decl : NULL;
    if (!destructor || !destructor->func_body || !destructor->link_name ||
        !destructor->func_this_param) {
        return NULL;
    }
    return destructor;
}

static void gen_cxx_throw32(Module* mod, Stmt* stmt) {
    Type* type;
    if (!stmt) rcc_fatal("validated C++ throw is missing");
    if (!stmt->throw_expr) {
        gen_cxx_exception_unwind_cleanup32(mod);
        if (active_cxx_exception_frame_offset != INT_MAX) {
            gen_cxx_exception_frame_address32(
                mod, active_cxx_exception_frame_offset);
            emit_push_reg(mod, EAX);
            gen_cxx_exception_call32(mod, "rin_cpp_exception_rethrow_frame");
            emit_add_reg_imm(mod, ESP, 4);
        } else {
            gen_cxx_exception_call32(mod, "rin_cpp_exception_rethrow");
        }
        return;
    }
    type = stmt->throw_expr->type;
    if (type && (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION)) {
        Decl* destructor = gen_cxx_exception_object_destructor32(type);
        gen_lvalue(mod, stmt->throw_expr);
        emit_push_reg(mod, EAX); /* preserve source across cleanup/release */
        gen_cxx_exception_unwind_cleanup32(mod);
        gen_cxx_exception_release_frame32(mod, active_cxx_exception_frame_offset);
        emit_pop_reg(mod, ECX); /* source object */
        if (destructor) {
            gen_symbol_address(mod, decl_link_name(destructor), 0u);
            emit_push_reg(mod, EAX); /* payload destructor */
        }
        emit_mov_reg_imm(mod, EAX, gen_cxx_exception_type_tag32(type));
        emit_push_reg(mod, EAX); /* type tag */
        emit_mov_reg_imm(mod, EAX, (uint32_t)type->size);
        emit_push_reg(mod, EAX); /* object size */
        emit_push_reg(mod, ECX); /* source object; cdecl argument 1 */
        gen_cxx_exception_call32(
            mod, destructor ? "rin_cpp_exception_throw_object_with_cleanup"
                            : "rin_cpp_exception_throw_object");
        emit_add_reg_imm(mod, ESP, destructor ? 16 : 12);
    } else {
        gen_expr(mod, stmt->throw_expr);
        emit_push_reg(mod, EAX); /* preserve value across cleanup/release */
        gen_cxx_exception_unwind_cleanup32(mod);
        gen_cxx_exception_release_frame32(mod, active_cxx_exception_frame_offset);
        emit_pop_reg(mod, EDX); /* value */
        emit_mov_reg_imm(mod, EAX, gen_cxx_exception_type_tag32(type));
        emit_push_reg(mod, EAX); /* type tag */
        emit_push_reg(mod, EDX); /* value; cdecl argument 1 is at the top */
        gen_cxx_exception_call32(mod, "rin_cpp_exception_throw");
        emit_add_reg_imm(mod, ESP, 8);
    }
}

static void gen_cxx_try32(Module* mod, Stmt* stmt) {
    const int value_offset = 28;
    const int type_offset = 32;
    int dispatch_label = new_label();
    int end_label = new_label();
    int old_active_frame_offset = active_cxx_exception_frame_offset;
    CleanupCodegen* old_exception_cleanup_marker =
        active_cxx_exception_cleanup_marker;
    CleanupCodegen* try_cleanup_marker = active_cleanups;
    int old_cleanup_frame_offset = active_cxx_exception_cleanup_frame_offset;
    bool old_cleanup_registration_enabled =
        cxx_exception_cleanup_registration_enabled;

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

    active_cxx_exception_cleanup_frame_offset = stmt->try_frame_offset;
    active_cxx_exception_cleanup_marker = try_cleanup_marker;
    cxx_exception_cleanup_registration_enabled = true;
    gen_scoped_stmt(mod, stmt->try_body);
    active_cxx_exception_cleanup_frame_offset = old_cleanup_frame_offset;
    active_cxx_exception_cleanup_marker = old_exception_cleanup_marker;
    cxx_exception_cleanup_registration_enabled =
        old_cleanup_registration_enabled;
    gen_cxx_exception_frame_address32(mod, stmt->try_frame_offset);
    emit_push_reg(mod, EAX);
    gen_cxx_exception_call32(mod, "rin_cpp_exception_leave");
    emit_add_reg_imm(mod, ESP, 4);
    emit_jmp_label(mod, end_label);

    emit_label(mod, dispatch_label);
    gen_clear_call_temporary_owners32(mod);
    for (CxxCatch* handler = stmt->try_catches; handler;
         handler = handler->next) {
        int next_handler = new_label();
        if (!handler->is_ellipsis) {
            int matching_handler = new_label();
            gen_cxx_exception_frame_address32(mod, stmt->try_frame_offset);
            emit_mov_reg_mem(mod, EAX, EAX, type_offset);
            emit_cmp_reg_imm(mod, EAX,
                             (int32_t)gen_cxx_exception_type_tag32(
                                 handler->type));
            emit_jcc_label(mod, CC_E, matching_handler);
            for (size_t tag_index = 0u;
                 tag_index < handler->compatible_tag_count; ++tag_index) {
                emit_cmp_reg_imm(
                    mod, EAX,
                    (int32_t)(uint32_t)handler->compatible_tags[tag_index]);
                emit_jcc_label(mod, CC_E, matching_handler);
            }
            emit_jmp_label(mod, next_handler);
            emit_label(mod, matching_handler);
        }
        if (handler->parameter) {
            if (handler->parameter->type &&
                handler->parameter->type->kind == TYPE_PTR &&
                handler->parameter->type->is_reference) {
                /* A reference catch aliases the live payload.  Keep the
                 * frame storage alive until the handler releases it, so a
                 * scalar reference and an adjusted base reference both use
                 * the same target-width address ABI. */
                Type* match_type = (Type*)rcc_cxx_exception_match_type(
                    handler->type);
                if (match_type &&
                    (match_type->kind == TYPE_STRUCT ||
                     match_type->kind == TYPE_UNION)) {
                    gen_cxx_exception_payload_address32(mod, stmt, handler);
                } else {
                    gen_cxx_exception_frame_address32(
                        mod, stmt->try_frame_offset);
                    emit_mov_reg_reg(mod, EDX, EAX);
                    emit_add_reg_imm(mod, EDX, 28);
                }
                emit_store_typed32(mod, EBP, handler->parameter->var_offset,
                                   EDX, handler->parameter->type);
            } else if (handler->parameter->type &&
                       (handler->parameter->type->kind == TYPE_STRUCT ||
                        handler->parameter->type->kind == TYPE_UNION)) {
                gen_cxx_exception_payload_address32(mod, stmt, handler);
                int offset = 0;
                emit_mov_reg_reg(mod, ECX, EDX);
                emit_mov_reg_reg(mod, EDX, EBP);
                emit_add_reg_imm(mod, EDX, handler->parameter->var_offset);
                while (offset + 4 <= handler->parameter->type->size) {
                    emit_mov_reg_mem(mod, EAX, ECX, offset);
                    emit_mov_mem_reg(mod, EDX, offset, EAX);
                    offset += 4;
                }
                while (offset < handler->parameter->type->size) {
                    emit_load_typed32(mod, EAX, ECX, offset, type_uchar);
                    emit_store_typed32(mod, EDX, offset, EAX, type_uchar);
                    ++offset;
                }
            } else {
                gen_cxx_exception_frame_address32(mod, stmt->try_frame_offset);
                emit_mov_reg_mem(mod, EDX, EAX, value_offset);
                emit_store_typed32(mod, EBP, handler->parameter->var_offset,
                                   EDX, handler->parameter->type);
            }
        }
        /* A handler owns the transfer out of the protected region.  Remove
         * this frame before its body so return and rethrow cannot leave a
         * dead stack frame at the top of the runtime chain. */
        gen_cxx_exception_frame_address32(mod, stmt->try_frame_offset);
        emit_push_reg(mod, EAX);
        gen_cxx_exception_call32(mod, "rin_cpp_exception_leave");
        emit_add_reg_imm(mod, ESP, 4);
        active_cxx_exception_frame_offset = stmt->try_frame_offset;
        active_cxx_exception_cleanup_marker = try_cleanup_marker;
        cxx_exception_cleanup_registration_enabled = false;
        gen_scoped_stmt(mod, handler->body);
        cxx_exception_cleanup_registration_enabled =
            old_cleanup_registration_enabled;
        active_cxx_exception_cleanup_marker = old_exception_cleanup_marker;
        gen_cxx_exception_release_frame32(mod, stmt->try_frame_offset);
        active_cxx_exception_frame_offset = old_active_frame_offset;
        emit_jmp_label(mod, end_label);
        if (!handler->is_ellipsis) emit_label(mod, next_handler);
    }

    /* The runtime has already removed this frame.  Re-submit an unmatched
     * payload so an enclosing try or the process-level handler can receive it. */
    gen_cxx_exception_release_frame32(mod, old_active_frame_offset);
    gen_cxx_exception_frame_address32(mod, stmt->try_frame_offset);
    emit_push_reg(mod, EAX);
    gen_cxx_exception_call32(mod, "rin_cpp_exception_rethrow_frame");
    emit_add_reg_imm(mod, ESP, 4);
    emit_label(mod, end_label);
    active_cxx_exception_frame_offset = old_active_frame_offset;
    active_cxx_exception_cleanup_marker = old_exception_cleanup_marker;
}

static bool gen_static_reference_aggregate_into_owner32(
    Module* mod, Decl* declaration, Expr* expression, Type* type) {
    Decl* owner = declaration
        ? declaration->var_reference_temporary_owner : NULL;
    if (!mod || !declaration || !owner || !expression || !type) return false;
    if (expression->kind == EXPR_COMMA) {
        gen_expr(mod, expression->binary_lhs);
        return gen_static_reference_aggregate_into_owner32(
            mod, declaration, expression->binary_rhs, type);
    }
    if (expression->kind == EXPR_COND) {
        int else_label = new_label();
        int done_label = new_label();
        gen_expr(mod, expression->cond_test);
        emit_test_scalar_value(mod, expression->cond_test->type);
        emit_jcc_label(mod, CC_E, else_label);
        if (!gen_static_reference_aggregate_into_owner32(
                mod, declaration, expression->cond_then, type)) {
            return false;
        }
        emit_jmp_label(mod, done_label);
        emit_label(mod, else_label);
        if (!gen_static_reference_aggregate_into_owner32(
                mod, declaration, expression->cond_else, type)) {
            return false;
        }
        emit_label(mod, done_label);
        return true;
    }
    if (expression->kind == EXPR_COMPOUND) {
        if (expression->compound_constructor && type->cxx_class) {
            gen_reference_temporary_owner_address32(mod, owner);
            emit_mov_reg_reg(mod, ECX, EAX);
            gen_cxx_initialize_object32(
                mod, type, expression->compound_constructor,
                expression->compound_value_init
                    ? NULL : expression->compound_init);
            return true;
        }
        if (!owner->var_is_global && !owner->var_is_thread_local) {
            gen_zero_local_storage(mod, owner->var_offset,
                                   (size_t)type->size);
            if (!gen_local_initializer(mod, type, expression,
                                       owner->var_offset)) {
                return false;
            }
            gen_local_vtable_init(mod, type, EBP, owner->var_offset);
            gen_reference_temporary_owner_address32(mod, owner);
            return true;
        }
        if (expression->compound_offset >= 0) {
            rcc_error(expression->loc,
                      "static reference aggregate has no initializer storage");
            return false;
        }
        gen_zero_local_storage(mod, expression->compound_offset,
                               (size_t)type->size);
        if (!gen_local_initializer(mod, type, expression,
                                   expression->compound_offset)) {
            return false;
        }
        gen_local_vtable_init(mod, type, EBP,
                              expression->compound_offset);
        emit_byte(mod, 0x8D); /* LEA EAX, [EBP+disp32] */
        emit_byte(mod, modrm(2, EAX, EBP));
        emit_dword(mod, (uint32_t)expression->compound_offset);
    } else if (expression->kind == EXPR_CALL &&
               gen_aggregate_type32(type)) {
        if (!owner->var_is_global && !owner->var_is_thread_local) {
            int result_offset = expression->call_result_offset;
            expression->call_result_offset = owner->var_offset;
            gen_expr(mod, expression);
            expression->call_result_offset = result_offset;
            return true;
        }
        Expr* old_call = active_static_reference_result_call32;
        const char* old_symbol = active_static_reference_result_symbol32;
        Decl* old_decl = active_static_reference_result_decl32;
        active_static_reference_result_call32 = expression;
        active_static_reference_result_symbol32 = decl_link_name(owner);
        active_static_reference_result_decl32 = owner;
        gen_expr(mod, expression);
        active_static_reference_result_call32 = old_call;
        active_static_reference_result_symbol32 = old_symbol;
        active_static_reference_result_decl32 = old_decl;
        return true;
    } else if (gen_expr_is_lvalue(expression) ||
               gen_expr_is_xvalue(expression)) {
        gen_lvalue(mod, expression);
    } else {
        gen_expr(mod, expression);
    }
    emit_push_reg(mod, EAX);
    gen_reference_temporary_owner_address32(mod, owner);
    emit_mov_reg_reg(mod, ECX, EAX);
    emit_pop_reg(mod, EDX);
    gen_copy_aggregate_to_address32(mod, ECX, EDX, type->size);
    return true;
}

static Expr* gen_static_reference_subobject_rebase32(
    Expr* expression, Expr* complete_object, Decl* owner, bool* replaced) {
    Expr* copy;
    bool child_replaced = false;
    *replaced = false;
    if (!expression || !complete_object || !owner) return NULL;
    if (expression == complete_object) {
        Expr* replacement = expr_ident(owner->name, expression->loc);
        replacement->ident_decl = owner;
        replacement->type = owner->type;
        *replaced = true;
        return replacement;
    }
    switch (expression->kind) {
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            copy = ast_arena_alloc(sizeof(*copy));
            *copy = *expression;
            copy->member_base = gen_static_reference_subobject_rebase32(
                expression->member_base, complete_object, owner,
                &child_replaced);
            if (!copy->member_base || !child_replaced) return expression;
            *replaced = true;
            return copy;
        case EXPR_CXX_MEMBER_PTR_DOT:
            copy = ast_arena_alloc(sizeof(*copy));
            *copy = *expression;
            copy->binary_lhs = gen_static_reference_subobject_rebase32(
                expression->binary_lhs, complete_object, owner,
                &child_replaced);
            if (!copy->binary_lhs || !child_replaced) return expression;
            *replaced = true;
            return copy;
        case EXPR_CAST:
            copy = ast_arena_alloc(sizeof(*copy));
            *copy = *expression;
            copy->cast_expr = gen_static_reference_subobject_rebase32(
                expression->cast_expr, complete_object, owner,
                &child_replaced);
            if (!copy->cast_expr || !child_replaced) return expression;
            *replaced = true;
            return copy;
        case EXPR_COMMA:
            copy = ast_arena_alloc(sizeof(*copy));
            *copy = *expression;
            copy->binary_lhs = gen_static_reference_subobject_rebase32(
                expression->binary_lhs, complete_object, owner,
                &child_replaced);
            {
                bool rhs_replaced = false;
                copy->binary_rhs = gen_static_reference_subobject_rebase32(
                    expression->binary_rhs, complete_object, owner,
                    &rhs_replaced);
                if (!copy->binary_lhs || !copy->binary_rhs ||
                    (!child_replaced && !rhs_replaced)) return expression;
                *replaced = true;
            }
            return copy;
        case EXPR_COND:
            copy = ast_arena_alloc(sizeof(*copy));
            *copy = *expression;
            {
                bool then_replaced = false;
                bool else_replaced = false;
                copy->cond_then = gen_static_reference_subobject_rebase32(
                    expression->cond_then, complete_object, owner,
                    &then_replaced);
                copy->cond_else = gen_static_reference_subobject_rebase32(
                    expression->cond_else, complete_object, owner,
                    &else_replaced);
                if (!copy->cond_then || !copy->cond_else ||
                    (!then_replaced && !else_replaced)) return expression;
                *replaced = true;
            }
            return copy;
        default:
            return expression;
    }
}

static void gen_static_reference_owner_vtable_init32(
    Module* mod, Type* type, Decl* owner) {
    if (!mod || !type || !owner ||
        !codegen_type_has_vtable_storage(type)) {
        return;
    }
    /* Keep EBP as the live CFA base while initializing this static object. */
    emit_push_reg(mod, ECX);
    gen_reference_temporary_owner_address32(mod, owner);
    emit_mov_reg_reg(mod, ECX, EAX);
    gen_local_vtable_init(mod, type, ECX, 0);
    emit_pop_reg(mod, ECX);
}

static Expr* gen_conditional_xvalue_complete_source32(
    Expr* expression, unsigned depth) {
    Type* type;
    if (!expression || depth >= 64u) return NULL;
    type = expression->type;
    if (type && type->kind == TYPE_PTR && type->is_reference) {
        type = type->base;
    }
    if (type && (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) &&
        (expression->kind == EXPR_CALL ||
         expression->kind == EXPR_COMPOUND ||
         expression->kind == EXPR_VA_ARG)) {
        return expression;
    }
    if (expression->kind == EXPR_CAST && expression->cast_expr) {
        Type* source_type = expression->cast_expr->type;
        if (source_type && source_type->kind == TYPE_PTR &&
            source_type->is_reference) {
            source_type = source_type->base;
        }
        if (source_type &&
            (source_type->kind == TYPE_STRUCT ||
             source_type->kind == TYPE_UNION)) {
            return gen_conditional_xvalue_complete_source32(
                expression->cast_expr, depth + 1u);
        }
        if (expression->cast_type && expression->cast_type->is_reference &&
            expression->cast_type->is_rvalue_reference) {
            return gen_conditional_xvalue_complete_source32(
                expression->cast_expr, depth + 1u);
        }
    }
    if (expression->kind == EXPR_COMMA) {
        return gen_conditional_xvalue_complete_source32(
            expression->binary_rhs, depth + 1u);
    }
    if (expression->kind == EXPR_MEMBER &&
        expression->cxx_member_xvalue) {
        return gen_conditional_xvalue_complete_source32(
            expression->member_base, depth + 1u);
    }
    if (expression->kind == EXPR_CXX_MEMBER_PTR_DOT &&
        expression->cxx_member_xvalue) {
        return gen_conditional_xvalue_complete_source32(
            expression->binary_lhs, depth + 1u);
    }
    return NULL;
}

static void gen_conditional_reference_temporary_store_tag32(
    Module* mod, Decl* owner, int tag) {
    emit_push_reg(mod, EAX);
    gen_reference_temporary_owner_address32(mod, owner);
    emit_add_reg_imm(mod, EAX,
                     owner->var_reference_temporary_variant_offset);
    emit_mov_reg_reg(mod, ECX, EAX);
    emit_mov_reg_imm(mod, EDX, (uint32_t)tag);
    emit_store_typed32(mod, ECX, 0, EDX, type_int);
    emit_pop_reg(mod, EAX);
}

static bool gen_conditional_reference_temporary_initializer32(
    Module* mod, Decl* declaration, Expr* conditional) {
    Decl* owner = declaration
        ? declaration->var_reference_temporary_owner : NULL;
    Expr* arms[2];
    Expr* sources[2];
    Type* types[2];
    int else_label;
    int done_label;
    if (!mod || !declaration || !owner || !conditional ||
        conditional->kind != EXPR_COND ||
        !conditional->cxx_conditional_xvalue ||
        !owner->var_reference_temporary_alternate_type ||
        owner->var_reference_temporary_variant_offset < 0) {
        return false;
    }
    arms[0] = conditional->cond_then;
    arms[1] = conditional->cond_else;
    types[0] = owner->type;
    types[1] = owner->var_reference_temporary_alternate_type;
    sources[0] = gen_conditional_xvalue_complete_source32(arms[0], 0u);
    sources[1] = gen_conditional_xvalue_complete_source32(arms[1], 0u);
    for (size_t index = 0u; index < 2u; ++index) {
        if (!sources[index] || !types[index] || types[index]->size <= 0 ||
            (types[index]->kind != TYPE_STRUCT &&
             types[index]->kind != TYPE_UNION) ||
            !sources[index]->type ||
            (sources[index]->type->kind != TYPE_STRUCT &&
             sources[index]->type->kind != TYPE_UNION) ||
            !type_is_compatible(types[index], sources[index]->type)) {
            rcc_error(declaration->loc,
                      "conditional reference temporary arm has no complete class source");
            return false;
        }
    }
    else_label = new_label();
    done_label = new_label();
    gen_expr(mod, conditional->cond_test);
    emit_test_scalar_value(mod, conditional->cond_test->type);
    emit_jcc_label(mod, CC_E, else_label);
    if (!gen_static_reference_aggregate_into_owner32(
            mod, declaration, sources[0], types[0])) {
        return false;
    }
    gen_static_reference_owner_vtable_init32(mod, types[0], owner);
    gen_reference_temporary_owner_address32(mod, owner);
    gen_cxx_reference_adjustment32_force(mod, arms[0]);
    gen_conditional_reference_temporary_store_tag32(mod, owner, 0);
    emit_jmp_label(mod, done_label);
    emit_label(mod, else_label);
    if (!gen_static_reference_aggregate_into_owner32(
            mod, declaration, sources[1], types[1])) {
        return false;
    }
    gen_static_reference_owner_vtable_init32(mod, types[1], owner);
    gen_reference_temporary_owner_address32(mod, owner);
    gen_cxx_reference_adjustment32_force(mod, arms[1]);
    gen_conditional_reference_temporary_store_tag32(mod, owner, 1);
    emit_label(mod, done_label);
    return true;
}

static bool gen_static_reference_temporary_initializer32(
    Module* mod, Decl* declaration) {
    Decl* owner = declaration
        ? declaration->var_reference_temporary_owner : NULL;
    Expr* initializer = declaration ? declaration->var_init : NULL;
    Expr* source = declaration
        ? declaration->var_reference_temporary_source : NULL;
    Type* type = owner ? owner->type : NULL;
    bool conditional_variant = owner &&
        owner->var_reference_temporary_alternate_type != NULL;
    if (!source) source = gen_reference_temporary_source(initializer);
    if (!source) source = initializer;
    if (!mod || !declaration || !owner || !source || !type) return false;

    if (conditional_variant) {
        if (!gen_conditional_reference_temporary_initializer32(
                mod, declaration, initializer)) {
            return false;
        }
    } else if (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION ||
               type->kind == TYPE_ARRAY || type->kind == TYPE_VECTOR) {
        if (!gen_static_reference_aggregate_into_owner32(
                mod, declaration, source, type)) {
            return false;
        }
    } else if (gen_is_floating(type)) {
        gen_expr_as_type(mod, source, type);
        if (gen_float_width(type) == 8) emit_push_reg(mod, EDX);
        emit_push_reg(mod, EAX);
        gen_decl_storage_address32(mod, owner);
        emit_mov_reg_reg(mod, ECX, EAX);
        emit_pop_reg(mod, EAX);
        if (gen_float_width(type) == 8) emit_pop_reg(mod, EDX);
        emit_store_floating_raw(mod, type, ECX, 0);
    } else {
        gen_expr(mod, source);
        if (type_is_integer(type) || type->kind == TYPE_ENUM) {
            emit_convert_integer_value(mod, EAX, source->type, type);
            emit_normalize_atomic_value(mod, EAX, type);
        }
        emit_push_reg(mod, EAX);
        gen_decl_storage_address32(mod, owner);
        emit_mov_reg_reg(mod, EDX, EAX);
        emit_pop_reg(mod, ECX);
        emit_store_typed32(mod, EDX, 0, ECX, type);
    }

    if (!conditional_variant) {
        gen_static_reference_owner_vtable_init32(mod, type, owner);
    }

    if (declaration->var_reference_temporary_source) {
        bool replaced = false;
        Expr* subobject = gen_static_reference_subobject_rebase32(
            initializer, declaration->var_reference_temporary_source, owner,
            &replaced);
        if (!subobject || !replaced) {
            rcc_error(declaration->loc,
                      "cannot rebase static reference subobject for '%s'",
                      declaration->name);
            return false;
        }
        gen_lvalue(mod, subobject);
    } else if (!conditional_variant) {
        gen_decl_storage_address32(mod, owner);
        gen_cxx_reference_adjustment32_force(mod, initializer);
    }
    emit_push_reg(mod, EAX);
    gen_decl_storage_address32(mod, declaration);
    emit_mov_reg_reg(mod, EDX, EAX);
    emit_pop_reg(mod, ECX);
    emit_store_typed32(mod, EDX, 0, ECX, declaration->type);
    return true;
}

static bool gen_global_initializer32(Module* mod, Decl* declaration) {
    Type* type = declaration ? declaration->type : NULL;
    Expr* initializer = declaration ? declaration->var_init : NULL;
    if (!mod || !declaration || !type || !initializer) return false;
    if (declaration->var_reference_temporary_owner) {
        return gen_static_reference_temporary_initializer32(
            mod, declaration);
    }
    if (codegen_runtime_global_paren_aggregate(type, initializer)) {
        size_t offset = 0u;
        gen_expr(mod, initializer);
        emit_push_reg(mod, EAX);
        gen_decl_storage_address32(mod, declaration);
        emit_mov_reg_reg(mod, ECX, EAX);
        emit_pop_reg(mod, EDX);
        while (offset + 4u <= (size_t)type->size) {
            emit_mov_reg_mem(mod, EAX, EDX, (int32_t)offset);
            emit_mov_mem_reg(mod, ECX, (int32_t)offset, EAX);
            offset += 4u;
        }
        while (offset < (size_t)type->size) {
            emit_load_typed32(mod, EAX, EDX, (int32_t)offset, type_uchar);
            emit_store_typed32(mod, ECX, (int32_t)offset, EAX, type_uchar);
            ++offset;
        }
        return true;
    }
    if (codegen_runtime_global_constructor(type, initializer)) {
        if (initializer->kind == EXPR_CALL && initializer->type &&
            type_is_compatible(type, initializer->type)) {
            Expr* old_call = active_static_reference_result_call32;
            const char* old_symbol = active_static_reference_result_symbol32;
            Decl* old_decl = active_static_reference_result_decl32;
            active_static_reference_result_call32 = initializer;
            active_static_reference_result_symbol32 =
                decl_link_name(declaration);
            active_static_reference_result_decl32 = declaration;
            gen_expr(mod, initializer);
            active_static_reference_result_call32 = old_call;
            active_static_reference_result_symbol32 = old_symbol;
            active_static_reference_result_decl32 = old_decl;
            return true;
        }
        gen_decl_storage_address32(mod, declaration);
        emit_mov_reg_reg(mod, ECX, EAX);
        gen_cxx_initialize_object32(
            mod, type, initializer->compound_constructor,
            initializer->compound_value_init ? NULL : initializer->compound_init);
        return true;
    }
    if (gen_is_floating(type)) {
        gen_expr_as_type(mod, initializer, type);
        if (gen_float_width(type) == 4) {
            emit_push_reg(mod, EAX);
            gen_decl_storage_address32(mod, declaration);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_pop_reg(mod, EAX);
        } else {
            emit_push_reg(mod, EDX);
            emit_push_reg(mod, EAX);
            gen_decl_storage_address32(mod, declaration);
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
        gen_decl_storage_address32(mod, declaration);
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
    gen_decl_storage_address32(mod, declaration);
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
    int stack_size;
    if (!mod || !mod->global_initializers) return;
    stack_size = codegen_assign_global_initializer_storage(
        mod->global_initializers, 4);
    if (stack_size > INT_MAX - 15) {
        rcc_error((SourceLoc){"<global-init>", 0, 0},
                  "global initializer frame exceeds compiler limits");
        return;
    }
    stack_size = (stack_size + 15) & ~15;
    start = code_offset(mod);
    add_func_def(name, start);
    emit_push_reg(mod, EBP);
    emit_mov_reg_reg(mod, EBP, ESP);
    if (stack_size > 0) emit_sub_reg_imm(mod, ESP, stack_size);
    gen_clear_call_temporary_owners32(mod);
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
        CleanupCodegen* full_expression_marker = active_cleanups;
        if (!gen_global_initializer32(mod, initializer->declaration)) {
            rcc_error((SourceLoc){"<global-init>", 0, 0},
                      "cannot lower deferred global initializer");
        }
        gen_cleanups_until(mod, full_expression_marker);
        discard_cleanups_until(full_expression_marker);
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

static StaticLocalCleanup* codegen_find_static_local_cleanup(
    Module* mod, Decl* declaration) {
    for (StaticLocalCleanup* item = mod ? mod->static_local_cleanups : NULL;
         item; item = item->next) {
        if (item->declaration == declaration) return item;
    }
    return NULL;
}

static void codegen_register_static_local_cleanup32(
    Module* mod, Decl* declaration) {
    StaticLocalCleanup* cleanup = codegen_find_static_local_cleanup(
        mod, declaration);
    Decl* cleanup_object = declaration &&
            declaration->var_reference_temporary_owner
        ? declaration->var_reference_temporary_owner : declaration;
    if (!cleanup) return;
    /* cdecl pushes arguments right-to-left: dso, object, then callback. */
    gen_symbol_address(mod, "__dso_handle", 0u);
    emit_push_reg(mod, EAX);
    gen_decl_storage_address32(mod, cleanup_object);
    emit_push_reg(mod, EAX);
    gen_symbol_address(mod, cleanup->callback_name, 0u);
    emit_push_reg(mod, EAX);
    gen_cxx_exception_call32(mod, declaration->var_is_thread_local
        ? "__cxa_thread_atexit" : "__cxa_atexit");
    emit_add_reg_imm(mod, ESP, 12);
    emit_cmp_reg_imm(mod, EAX, 0);
    {
        int registered = new_label();
        emit_jcc_label(mod, CC_E, registered);
        emit_byte(mod, 0xE8);
        {
            uint32_t call_offset = code_offset(mod);
            emit_dword(mod, 0u);
            add_func_call_ref("abort", call_offset);
        }
        emit_label(mod, registered);
    }
}

static bool gen_guarded_static_initializer32(
    Module* mod, Decl* declaration) {
    Decl* guard = declaration
        ? declaration->var_reference_temporary_guard : NULL;
    CleanupCodegen* guard_cleanup;
    int done;
    if (!mod || !declaration || !guard) return false;
    done = new_label();
    gen_decl_storage_address32(mod, guard);
    emit_push_reg(mod, EAX);
    gen_cxx_exception_call32(mod, "__cxa_guard_acquire");
    emit_add_reg_imm(mod, ESP, 4);
    emit_test_reg_reg(mod, EAX, EAX);
    emit_jcc_label(mod, CC_E, done);

    guard_cleanup = rcc_alloc(sizeof(*guard_cleanup));
    memset(guard_cleanup, 0, sizeof(*guard_cleanup));
    guard_cleanup->guard_abort = guard;
    guard_cleanup->previous = active_cleanups;
    guard_cleanup->exception_frame_offset =
        active_cxx_exception_cleanup_frame_offset;
    active_cleanups = guard_cleanup;
    gen_cxx_guard_exception_register32(mod, guard_cleanup);

    if (declaration->var_reference_temporary_owner) {
        if (!gen_static_reference_temporary_initializer32(
                mod, declaration)) {
            rcc_error(declaration->loc,
                      "cannot initialize static reference temporary for '%s'",
                      declaration->name);
            return false;
        }
    } else if (declaration->var_dynamic_initializer &&
               declaration->var_init &&
               !gen_global_initializer32(mod, declaration)) {
        rcc_error(declaration->loc,
                  "cannot initialize guarded static-duration object '%s'",
                  declaration->name);
        return false;
    }
    gen_cleanups_until(mod, guard_cleanup);
    discard_cleanups_until(guard_cleanup);
    codegen_register_static_local_cleanup32(mod, declaration);
    if (guard_cleanup->exception_registered) {
        gen_cxx_guard_exception_unregister32(mod, guard_cleanup);
        guard_cleanup->exception_registered = false;
    }
    gen_decl_storage_address32(mod, guard);
    emit_push_reg(mod, EAX);
    gen_cxx_exception_call32(mod, "__cxa_guard_release");
    emit_add_reg_imm(mod, ESP, 4);
    active_cleanups = guard_cleanup->previous;
    rcc_free(guard_cleanup);
    emit_label(mod, done);
    return true;
}

static void codegen_emit_static_local_cleanups32(Module* mod) {
    for (StaticLocalCleanup* item = mod ? mod->static_local_cleanups : NULL;
         item; item = item->next) {
        Type* old_return_type = current_function_return_type;
        CleanupCodegen* old_cleanups = active_cleanups;
        VLAScopeCodegen* old_vla_scopes = active_vla_scopes;
        VLAScopeCodegen* old_break_vla = break_vla_marker;
        VLAScopeCodegen* old_continue_vla = continue_vla_marker;
        int stack_size = 0;
        uint32_t start;
        codegen_assign_cleanup_plan(item->declaration->var_cleanups,
                                    &stack_size, 4);
        if (stack_size > INT_MAX - 15) {
            rcc_error(item->declaration->loc,
                      "static cleanup frame exceeds compiler limits");
            continue;
        }
        stack_size = (stack_size + 15) & ~15;
        start = code_offset(mod);
        add_func_def(item->callback_name, start);
        emit_push_reg(mod, EBP);
        emit_mov_reg_reg(mod, EBP, ESP);
        if (stack_size > 0) emit_sub_reg_imm(mod, ESP, stack_size);
        current_function_return_type = NULL;
        active_cleanups = NULL;
        active_vla_scopes = NULL;
        break_vla_marker = NULL;
        continue_vla_marker = NULL;
        gen_cxx_cleanup_plan32(mod, item->declaration->var_cleanups,
                               false, INT_MAX);
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
        module_add_symbol(mod, item->callback_name, start, true,
                          MODULE_SYMBOL_CODE, false);
    }
}

static void codegen_emit_cxx_array_placement_cleanup_thunks32(Module* mod) {
    for (CxxArrayPlacementCleanupThunk* thunk =
             mod ? mod->cxx_array_placement_cleanup_thunks : NULL;
         thunk; thunk = thunk->next) {
        const TypeParam* parameter = thunk->deallocation_function &&
                thunk->deallocation_function->type &&
                thunk->deallocation_function->type->kind == TYPE_FUNC
            ? thunk->deallocation_function->type->params : NULL;
        size_t argument_count = 0u;
        uint32_t start;
        while (parameter) {
            ++argument_count;
            parameter = parameter->next;
        }
        if (argument_count == 0u || argument_count > (size_t)(INT_MAX / 4) - 1u) {
            rcc_fatal("C++ placement cleanup thunk has invalid parameters");
        }
        if ((code_offset(mod) & 1u) != 0u) emit_byte(mod, 0x90);
        start = code_offset(mod);
        add_func_def(thunk->callback_name, start);
        emit_push_reg(mod, EBP);
        emit_mov_reg_reg(mod, EBP, ESP);
        for (size_t index = argument_count; index > 0u; --index) {
            emit_mov_reg_mem(mod, EAX, EBP, 8);
            emit_mov_reg_mem(mod, EDX, EAX, (int)(index * 4u));
            emit_push_reg(mod, EDX);
        }
        emit_mov_reg_mem(mod, EAX, EBP, 8);
        emit_mov_reg_mem(mod, EDX, EAX, 0);
        emit_push_reg(mod, EDX);
        emit_byte(mod, 0xE8);
        {
            uint32_t call_offset = code_offset(mod);
            emit_dword(mod, 0u);
            add_func_call_ref(
                decl_link_name((Decl*)thunk->deallocation_function),
                call_offset);
        }
        emit_add_reg_imm(mod, ESP,
                         (uint32_t)((argument_count + 1u) * 4u));
        emit_leave(mod);
        emit_ret(mod);
        module_add_symbol(mod, thunk->callback_name, start, true,
                          MODULE_SYMBOL_CODE, false);
        module_set_symbol_size(mod, thunk->callback_name,
                               code_offset(mod) - start);
    }
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
    int stack_size;
    if (!mod || !mod->global_finalizers) return;
    stack_size = codegen_assign_global_cleanup_storage(
        mod->global_finalizers, 4);
    if (stack_size > INT_MAX - 15) {
        rcc_error((SourceLoc){"<global-fini>", 0, 0},
                  "global cleanup stack frame exceeds compiler limits");
        return;
    }
    stack_size = (stack_size + 15) & ~15;
    start = code_offset(mod);
    add_func_def(name, start);
    emit_push_reg(mod, EBP);
    emit_mov_reg_reg(mod, EBP, ESP);
    if (stack_size > 0) emit_sub_reg_imm(mod, ESP, stack_size);
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
        if (finalizer->plan) {
            gen_cxx_cleanup_plan32(mod, finalizer->plan, false, INT_MAX);
            continue;
        }
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
    unsigned width = type_integer_width(control_type);
    if (width == 0u) width = 32u;
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
    uint32_t debug_line_start;
    if (!stmt) return;
    debug_line_start = code_offset(mod);
    stmt->debug_line_valid = false;
    stmt->debug_code_start = debug_line_start;
    stmt->debug_code_end = debug_line_start;

    switch (stmt->kind) {
        case STMT_EXPR:
            if (stmt->expr) {
                CleanupCodegen* full_expression_marker = active_cleanups;
                gen_expr(mod, stmt->expr);
                gen_cleanups_until(mod, full_expression_marker);
                discard_cleanups_until(full_expression_marker);
            }
            break;

        case STMT_BLOCK: {
            CleanupCodegen* marker = active_cleanups;
            VLAScopeCodegen* vla_marker = active_vla_scopes;
            if (!stmt->block_no_scope) {
                stmt->debug_code_start = code_offset(mod);
            }
            if (!stmt->block_no_scope && stmt->vla_stack_offset < 0) {
                emit_mov_mem_reg(mod, EBP, stmt->vla_stack_offset, ESP);
            }
            for (StmtList* s = stmt->block_stmts; s; s = s->next) {
                gen_stmt(mod, s->stmt);
            }
            if (!stmt->block_no_scope) {
                gen_cleanups_until(mod, marker);
                discard_cleanups_until(marker);
                gen_vla_scopes_until(mod, vla_marker);
                discard_vla_scopes_until(vla_marker);
            }
            if (!stmt->block_no_scope && stmt->vla_stack_offset < 0) {
                emit_mov_reg_mem(mod, ESP, EBP, stmt->vla_stack_offset);
            }
            if (!stmt->block_no_scope) {
                stmt->debug_code_end = code_offset(mod);
            }
            break;
        }

        case STMT_IF: {
            CleanupCodegen* full_expression_marker = active_cleanups;
            int false_cleanup_label = new_label();
            int then_label = new_label();
            int else_label = new_label();
            int end_label = new_label();

            gen_expr(mod, stmt->if_cond);
            emit_test_scalar_value(mod, stmt->if_cond->type);
            emit_jcc_label(mod, CC_E, false_cleanup_label);
            gen_cleanups_until(mod, full_expression_marker);
            emit_jmp_label(mod, then_label);
            emit_label(mod, false_cleanup_label);
            gen_cleanups_until(mod, full_expression_marker);
            emit_jmp_label(mod, else_label);
            discard_cleanups_until(full_expression_marker);

            emit_label(mod, then_label);
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
            int false_cleanup_label = new_label();
            int body_label = new_label();
            int old_break = break_label;
            int old_continue = continue_label;
            break_label = end_label;
            continue_label = start_label;
            break_cleanup_marker = loop_marker;
            continue_cleanup_marker = loop_marker;
            break_vla_marker = vla_loop_marker;
            continue_vla_marker = vla_loop_marker;

            emit_label(mod, start_label);
            CleanupCodegen* full_expression_marker = active_cleanups;
            gen_expr(mod, stmt->while_cond);
            emit_test_scalar_value(mod, stmt->while_cond->type);
            emit_jcc_label(mod, CC_E, false_cleanup_label);
            gen_cleanups_until(mod, full_expression_marker);
            emit_jmp_label(mod, body_label);
            emit_label(mod, false_cleanup_label);
            gen_cleanups_until(mod, full_expression_marker);
            emit_jmp_label(mod, end_label);
            discard_cleanups_until(full_expression_marker);

            emit_label(mod, body_label);
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
            int true_cleanup_label = new_label();
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
            CleanupCodegen* full_expression_marker = active_cleanups;
            gen_expr(mod, stmt->while_cond);
            emit_test_scalar_value(mod, stmt->while_cond->type);
            emit_jcc_label(mod, CC_NE, true_cleanup_label);
            gen_cleanups_until(mod, full_expression_marker);
            emit_jmp_label(mod, end_label);
            emit_label(mod, true_cleanup_label);
            gen_cleanups_until(mod, full_expression_marker);
            emit_jmp_label(mod, start_label);
            discard_cleanups_until(full_expression_marker);

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
                CleanupCodegen* full_expression_marker = active_cleanups;
                int false_cleanup_label = new_label();
                int body_label = new_label();
                gen_expr(mod, stmt->for_cond);
                emit_test_scalar_value(mod, stmt->for_cond->type);
                emit_jcc_label(mod, CC_E, false_cleanup_label);
                gen_cleanups_until(mod, full_expression_marker);
                emit_jmp_label(mod, body_label);
                emit_label(mod, false_cleanup_label);
                gen_cleanups_until(mod, full_expression_marker);
                emit_jmp_label(mod, end_label);
                discard_cleanups_until(full_expression_marker);
                emit_label(mod, body_label);
            }

            gen_scoped_stmt(mod, stmt->for_body);

            emit_label(mod, inc_label);
            if (stmt->for_inc) {
                CleanupCodegen* full_expression_marker = active_cleanups;
                gen_expr(mod, stmt->for_inc);
                gen_cleanups_until(mod, full_expression_marker);
                discard_cleanups_until(full_expression_marker);
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
            CleanupCodegen* full_expression_marker = active_cleanups;
            gen_expr(mod, stmt->switch_expr);
            if (stmt->switch_expr && context.control_type &&
                context.control_type->size == 8) {
                emit_push_reg(mod, EAX);
                emit_push_reg(mod, EDX);
            } else {
                emit_push_reg(mod, EAX);
            }
            gen_cleanups_until(mod, full_expression_marker);
            discard_cleanups_until(full_expression_marker);
            if (stmt->switch_expr && context.control_type &&
                context.control_type->size == 8) {
                emit_pop_reg(mod, EDX);
                emit_pop_reg(mod, EAX);
            } else {
                emit_pop_reg(mod, EAX);
            }
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
            {
            CleanupCodegen* full_expression_marker = active_cleanups;
            if (stmt->return_val) {
                if (current_function_return_type &&
                    current_function_return_type->kind == TYPE_PTR &&
                    current_function_return_type->is_reference) {
                    /* C++ references use the pointer ABI.  A reference
                     * return carries the lvalue address, not its value. */
                    if (stmt->return_reference_temporary_offset < 0) {
                        if (!gen_local_initializer(
                                mod, current_function_return_type->base,
                                stmt->return_val,
                                stmt->return_reference_temporary_offset)) {
                            rcc_error(stmt->loc,
                                      "cannot materialize scalar reference return");
                        }
                        emit_byte(mod, 0x8D);  /* LEA EAX, [EBP+disp32] */
                        emit_byte(mod, modrm(2, EAX, EBP));
                        emit_dword(mod, (uint32_t)
                            stmt->return_reference_temporary_offset);
                    } else {
                        gen_lvalue(mod, stmt->return_val);
                        gen_cxx_reference_adjustment32(
                            mod, stmt->return_val);
                    }
                } else if (gen_is_floating(current_function_return_type) ||
                           gen_is_floating(stmt->return_val->type)) {
                    gen_expr_as_type(mod, stmt->return_val,
                                     current_function_return_type);
                } else if (type_is_complex(
                               current_function_return_type)) {
                    Type* component_type =
                        current_function_return_type->complex_component_type;
                    int offset = 0;
                    if (gen_expr_is_lvalue(stmt->return_val) ||
                        gen_expr_is_xvalue(stmt->return_val)) {
                        gen_lvalue(mod, stmt->return_val);
                    } else {
                        gen_expr(mod, stmt->return_val);
                    }
                    if (component_type &&
                        component_type->kind == TYPE_FLOAT) {
                        emit_mov_reg_reg(mod, ECX, EAX);
                        emit_mov_reg_mem(mod, EAX, ECX, 0);
                        emit_mov_reg_mem(mod, EDX, ECX, 4);
                    } else {
                        emit_mov_reg_reg(mod, ECX, EAX);
                        emit_mov_reg_mem(mod, EDX, EBP, 8);
                        while (offset + 4 <=
                               current_function_return_type->size) {
                            emit_mov_reg_mem(mod, EAX, ECX, offset);
                            emit_mov_mem_reg(mod, EDX, offset, EAX);
                            offset += 4;
                        }
                        while (offset <
                               current_function_return_type->size) {
                            emit_load_typed32(
                                mod, EAX, ECX, offset, type_uchar);
                            emit_store_typed32(
                                mod, EDX, offset, EAX, type_uchar);
                            ++offset;
                        }
                        emit_mov_reg_reg(mod, EAX, EDX);
                    }
                } else if (current_function_return_type &&
                    (current_function_return_type->kind == TYPE_STRUCT ||
                     current_function_return_type->kind == TYPE_UNION ||
                     current_function_return_type->kind == TYPE_VECTOR ||
                     type_is_cxx_member_function_pointer(
                         current_function_return_type))) {
                    int offset = 0;
                    if (current_function_return_type->kind == TYPE_VECTOR ||
                        type_is_cxx_member_function_pointer(
                            current_function_return_type)) {
                        gen_expr(mod, stmt->return_val);
                    } else if (stmt->return_val->kind == EXPR_VA_ARG) {
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
                    !gen_is_integer64(stmt->return_val->type) &&
                    !gen_is_floating(stmt->return_val->type)) {
                    emit_extend_eax_to_integer64(
                        mod, stmt->return_val->type);
                } else if (!gen_is_integer64(current_function_return_type) &&
                           !gen_is_floating(current_function_return_type) &&
                           !gen_is_floating(stmt->return_val->type) &&
                           (type_is_integer(current_function_return_type) ||
                            (current_function_return_type &&
                             current_function_return_type->kind ==
                                 TYPE_ENUM))) {
                    emit_convert_integer_value(
                        mod, EAX, stmt->return_val->type,
                        current_function_return_type);
                }
            }
            if (stmt->return_val && active_cleanups !=
                    full_expression_marker) {
                emit_push_reg(mod, EAX);
                emit_push_reg(mod, EDX);
                gen_cleanups_until(mod, full_expression_marker);
                discard_cleanups_until(full_expression_marker);
                emit_pop_reg(mod, EDX);
                emit_pop_reg(mod, EAX);
            }
            if (active_cxx_exception_frame_offset != INT_MAX) {
                if (stmt->return_val) {
                    emit_push_reg(mod, EAX);
                    emit_push_reg(mod, EDX);
                }
                gen_cxx_exception_release_frame32(
                    mod, active_cxx_exception_frame_offset);
                if (stmt->return_val) {
                    emit_pop_reg(mod, EDX);
                    emit_pop_reg(mod, EAX);
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
            gen_cxx_destroy_current_function_subobjects32(mod);
            if (stmt->return_val &&
                gen_is_floating(current_function_return_type)) {
                emit_x87_double_value_from_raw(
                    mod, current_function_return_type);
            }
            emit_leave(mod);
            emit_ret(mod);
            break;
            }

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
            if (d && d->kind == DECL_TYPEDEF) {
                gen_typeof_runtime_evaluations(mod, d->typedef_type);
                gen_vla_typedef_bound_evaluations(
                    mod, d->typedef_type, d, false);
                break;
            }
            if (d->kind == DECL_VAR && d->var_is_static_local &&
                d->var_reference_temporary_guard) {
                if (!gen_guarded_static_initializer32(mod, d)) {
                    rcc_error(d->loc,
                              "cannot initialize static object for '%s'",
                              d->name);
                }
                break;
            }
            if (d->kind == DECL_VAR &&
                (d->var_is_static_local || d->var_is_block_extern)) break;
            CleanupCodegen* initializer_marker = active_cleanups;
            if (d->kind == DECL_VAR && !d->var_is_vla &&
                d->var_vla_extent_count > 0) {
                int extent_slot = 0;
                gen_typeof_runtime_evaluations(mod, d->type);
                gen_vla_extents(mod, d->type, NULL, NULL, d, &extent_slot);
            }
            if (d->kind == DECL_VAR && d->var_is_vla) {
                gen_vla_alloc(mod, d);
                record_vla_scope(d);
            } else if (d->kind == DECL_VAR &&
                       (d->var_init ||
                        (d->type &&
                         codegen_type_has_vtable_storage(d->type)))) {
                    if (d->type && (d->type->kind == TYPE_ARRAY ||
                                d->type->kind == TYPE_STRUCT ||
                                d->type->kind == TYPE_UNION ||
                                d->type->kind == TYPE_VECTOR)) {
                    gen_zero_local_storage(mod, d->var_offset,
                                           (size_t)d->type->size);
                }
                if (d->var_init &&
                    d->var_reference_temporary_offset < 0 &&
                    d->var_reference_temporary_owner &&
                    d->var_reference_temporary_owner
                        ->var_reference_temporary_alternate_type) {
                    if (!gen_conditional_reference_temporary_initializer32(
                            mod, d, d->var_init)) {
                        rcc_error(
                            d->loc,
                            "cannot initialize conditional reference temporary for '%s'",
                            d->name);
                    } else {
                        emit_store_typed32(
                            mod, EBP, d->var_offset, EAX, d->type);
                    }
                } else if (d->var_init &&
                    d->var_reference_temporary_offset < 0) {
                    Expr* temporary_source =
                        d->var_reference_temporary_source
                            ? d->var_reference_temporary_source
                            : gen_reference_temporary_source(d->var_init);
                    Expr* initializer = temporary_source
                        ? temporary_source : d->var_init;
                    Type* temporary_type = d->var_reference_temporary_owner
                        ? d->var_reference_temporary_owner->type
                        : d->type->base;
                    if (!d->var_reference_temporary_owner &&
                        initializer->type &&
                        (initializer->type->kind == TYPE_STRUCT ||
                         initializer->type->kind == TYPE_UNION ||
                         initializer->type->kind == TYPE_VECTOR)) {
                        temporary_type = initializer->type;
                    }
                    bool call_result_storage = temporary_type &&
                        !codegen_reference_temporary_scalar(temporary_type) &&
                        initializer->kind == EXPR_CALL &&
                        initializer->call_result_offset ==
                            d->var_reference_temporary_offset;
                    if (!call_result_storage && temporary_type &&
                        (temporary_type->kind == TYPE_STRUCT ||
                         temporary_type->kind == TYPE_UNION ||
                         temporary_type->kind == TYPE_VECTOR)) {
                        gen_zero_local_storage(
                            mod, d->var_reference_temporary_offset,
                            (size_t)temporary_type->size);
                    }
                    if (call_result_storage) {
                        gen_expr(mod, initializer);
                    } else if (!gen_local_initializer(
                                   mod, temporary_type, initializer,
                                   d->var_reference_temporary_offset)) {
                        rcc_error(
                            d->loc,
                            "cannot initialize reference temporary for '%s'",
                            d->name);
                    }
                    if (!call_result_storage) {
                        gen_local_vtable_init(
                            mod, temporary_type, EBP,
                            d->var_reference_temporary_offset);
                    }
                    if (d->var_reference_temporary_source) {
                        bool replaced = false;
                        Expr* subobject =
                            gen_static_reference_subobject_rebase32(
                                d->var_init,
                                d->var_reference_temporary_source,
                                d->var_reference_temporary_owner,
                                &replaced);
                        if (!subobject || !replaced) {
                            rcc_error(d->loc,
                                      "cannot rebase local reference subobject for '%s'",
                                      d->name);
                            return;
                        }
                        gen_lvalue(mod, subobject);
                    } else {
                        emit_byte(mod, 0x8D);  /* LEA EAX, [EBP+disp32] */
                        emit_byte(mod, modrm(2, EAX, EBP));
                        emit_dword(mod,
                                   (uint32_t)d->var_reference_temporary_offset);
                        gen_cxx_reference_adjustment32_force(mod, d->var_init);
                    }
                    emit_store_typed32(mod, EBP, d->var_offset, EAX, d->type);
                } else if (d->var_init &&
                           !gen_local_initializer(mod, d->type, d->var_init,
                                                  d->var_offset)) {
                    rcc_error(d->loc, "unsupported local initializer for '%s'",
                              d->name);
                }
                gen_local_vtable_init(mod, d->type, EBP, d->var_offset);
            }
            gen_cleanups_until(mod, initializer_marker);
            discard_cleanups_until(initializer_marker);
            if (d->kind == DECL_VAR && d->var_cleanup) {
                CleanupCodegen* cleanup = rcc_alloc(sizeof(*cleanup));
                cleanup->expression = d->var_cleanup;
                cleanup->previous = active_cleanups;
                cleanup->guard_abort = NULL;
                cleanup->declaration = d;
                cleanup->exception_frame_offset =
                    active_cxx_exception_cleanup_frame_offset;
                cleanup->exception_registered = false;
                active_cleanups = cleanup;
                if (cxx_exception_cleanup_registration_enabled &&
                    active_cxx_exception_cleanup_frame_offset != INT_MAX) {
                    gen_cxx_exception_register_cleanup32(mod, cleanup);
                }
            }
            if (d->kind == DECL_VAR && d->var_cleanups) {
                CleanupCodegen* cleanup = rcc_alloc(sizeof(*cleanup));
                cleanup->plan = d->var_cleanups;
                cleanup->previous = active_cleanups;
                cleanup->guard_abort = NULL;
                cleanup->declaration = d;
                cleanup->exception_frame_offset =
                    active_cxx_exception_cleanup_frame_offset;
                active_cleanups = cleanup;
                if (cxx_exception_cleanup_registration_enabled &&
                    active_cxx_exception_cleanup_frame_offset != INT_MAX &&
                    gen_cxx_cleanup_plan_registerable32(d->var_cleanups)) {
                    gen_cxx_exception_register_plan32(
                        mod, d->var_cleanups,
                        active_cxx_exception_cleanup_frame_offset);
                    cleanup->exception_registered = true;
                }
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
            /* Sema owns the StmtKind invariant.  Reaching this point means
             * an AST extension was added without a corresponding i686
             * lowering; silently emitting no code would produce a valid-
             * looking but semantically corrupt image. */
            rcc_error(stmt ? stmt->loc : (SourceLoc){"<statement>", 0, 0},
                      "unsupported statement kind in i686 code generation");
            break;
    }
    stmt->debug_code_end = code_offset(mod);
    if (mod->debug_legacy_statement_lines &&
        stmt->loc.line > 0 && code_offset(mod) > debug_line_start) {
        stmt->debug_line_offset = debug_line_start;
        stmt->debug_line_valid = true;
    }
}

/* ═══════════════════════════════════════
 * Function Code Generation
 * ═══════════════════════════════════════ */

static void gen_function(Module* mod, Decl* decl) {
    int stack_size;
    int frame_alignment;
    Type* old_return_type;
    int old_active_frame_offset;
    Decl* old_function_decl;
    CleanupCodegen* old_cleanups;
    VLAScopeCodegen* old_vla_scopes;
    VLAScopeCodegen* old_break_vla;
    VLAScopeCodegen* old_continue_vla;
    if (!decl->func_body) return;

    /* Naked functions own their complete entry/exit sequence; do not insert
     * the standard compiler-managed frame around their body. */
    if (decl->func_is_naked) {
        gen_stmt(mod, decl->func_body);
        return;
    }

    for (DeclList* parameter = decl->func_params; parameter;
         parameter = parameter->next) {
        if (parameter->decl && parameter->decl->kind == DECL_PARAM) {
            parameter->decl->param_is_cxx_indirect =
                gen_cxx_indirect_parameter32(parameter->decl->type);
        }
    }

    stack_size = codegen_required_local_bytes(decl->func_body);
    for (DeclList* parameter = decl->func_params; parameter;
         parameter = parameter->next) {
        if (parameter->decl && parameter->decl->var_cleanups) {
            codegen_assign_cleanup_plan(parameter->decl->var_cleanups,
                                        &stack_size, 4);
        }
    }
    stack_size = codegen_assign_local_vla_extent_slots(
        decl->func_body, stack_size, 4);
    stack_size = codegen_assign_compound_storage(decl->func_body, stack_size,
                                                 4);
    stack_size = codegen_assign_cxx_constructor_storage(
        decl, stack_size, 4);
    frame_alignment = codegen_required_frame_alignment(decl->func_body);
    codegen_assign_vla_parameter_slots(decl, &stack_size);
    if (stack_size > INT_MAX - 15) {
        rcc_error(decl->loc, "function stack frame exceeds compiler limits");
        return;
    }
    stack_size = (stack_size + 15) & ~15;

    /* Function prologue.  An explicitly over-aligned local needs an aligned
     * frame base on i686; keep the normal saved-EBP/return-address layout at
     * [EBP] and [EBP+4] after rounding ESP down. */
    emit_push_reg(mod, EBP);
    if (frame_alignment > 4) {
        emit_mov_reg_mem(mod, EAX, ESP, 0);
        emit_mov_reg_mem(mod, EDX, ESP, 4);
        emit_and_reg_imm(mod, ESP, (uint32_t)(-(int)frame_alignment));
        emit_mov_mem_reg(mod, ESP, 0, EAX);
        emit_mov_mem_reg(mod, ESP, 4, EDX);
        emit_mov_reg_reg(mod, EBP, ESP);
    } else {
        emit_mov_reg_reg(mod, EBP, ESP);
    }
    if (stack_size > 0) {
        emit_sub_reg_imm(mod, ESP, stack_size);
    }
    gen_clear_call_temporary_owners32(mod);

    gen_vla_parameter_extents(mod, decl);

    /* Generate body */
    old_return_type = current_function_return_type;
    old_active_frame_offset = active_cxx_exception_frame_offset;
    current_function_return_type = decl->type &&
                                   decl->type->kind == TYPE_FUNC
        ? decl->type->ret_type : NULL;
    active_cxx_exception_frame_offset = INT_MAX;
    active_cxx_exception_cleanup_marker = NULL;
    active_cxx_exception_cleanup_frame_offset = INT_MAX;
    old_cleanups = active_cleanups;
    old_vla_scopes = active_vla_scopes;
    old_break_vla = break_vla_marker;
    old_continue_vla = continue_vla_marker;
    active_cleanups = NULL;
    active_vla_scopes = NULL;
    break_vla_marker = NULL;
    continue_vla_marker = NULL;
    named_codegen_labels = NULL;
    old_function_decl = current_function_decl32;
    current_function_decl32 = decl;
    for (DeclList* parameter = decl->func_params; parameter;
         parameter = parameter->next) {
        if (parameter->decl && parameter->decl->var_cleanups) {
            CleanupCodegen* cleanup = rcc_alloc(sizeof(*cleanup));
            cleanup->plan = parameter->decl->var_cleanups;
            cleanup->previous = active_cleanups;
            cleanup->guard_abort = NULL;
            cleanup->declaration = parameter->decl;
            cleanup->exception_frame_offset = INT_MAX;
            cleanup->exception_registered = false;
            active_cleanups = cleanup;
            if (gen_cxx_cleanup_plan_registerable32(cleanup->plan)) {
                gen_cxx_exception_register_plan32(
                    mod, cleanup->plan, INT_MAX);
                cleanup->exception_registered = true;
            }
        }
    }
    /* Constructor storage initialization is emitted by the complete-object
     * caller.  The constructor symbol itself contains only the user body;
     * this lets a derived constructor invoke a base constructor without
     * reconstructing virtual bases that the most-derived caller already
     * initialized. */
    gen_stmt(mod, decl->func_body);
    gen_cleanups_until(mod, NULL);
    gen_cxx_destroy_current_function_subobjects32(mod);
    discard_cleanups_until(NULL);
    discard_vla_scopes_until(NULL);
    active_cleanups = old_cleanups;
    active_vla_scopes = old_vla_scopes;
    break_vla_marker = old_break_vla;
    continue_vla_marker = old_continue_vla;
    codegen_release_named_labels();
    current_function_return_type = old_return_type;
    current_function_decl32 = old_function_decl;
    active_cxx_exception_frame_offset = old_active_frame_offset;
    active_cxx_exception_cleanup_marker = NULL;
    active_cxx_exception_cleanup_frame_offset = INT_MAX;

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
    mod->debug_ast = ast;
    mod->debug_statement_ranges = true;
    mod->debug_legacy_statement_lines = true;

    /* Reset label counter and function call tracking */
    label_counter = 0;
    active_static_reference_result_call32 = NULL;
    active_static_reference_result_symbol32 = NULL;
    active_static_reference_result_decl32 = NULL;
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
            if (d->decl->is_weak) {
                module_mark_symbol_weak_any(mod, decl_link_name(d->decl));
            }
        }
    }

    /* Second pass: generate code for each defined function */
    for (DeclList* d = ast->decls; d; d = d->next) {
        if (d->decl->kind == DECL_FUNC && d->decl->func_body) {
            /* Itanium member-function pointers reserve the low bit of a
             * direct function address to distinguish virtual slots. */
            if ((code_offset(mod) & 1u) != 0u) {
                emit_byte(mod, 0x90);
            }
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
                             d->decl->storage != STORAGE_STATIC &&
                             !d->decl->func_has_local_linkage);
            module_set_symbol_size(mod, decl_link_name(d->decl),
                                   code_offset(mod) - func_start);
            module_set_symbol_source(mod, decl_link_name(d->decl),
                                     d->decl->loc);
            if (!d->decl->func_has_local_linkage &&
                (d->decl->is_weak || (d->decl->func_is_inline &&
                                      d->decl->func_has_cxx_linkage))) {
                module_mark_symbol_weak(mod, decl_link_name(d->decl));
            }
        }
    }

    codegen_emit_static_local_cleanups32(mod);
    codegen_emit_global_init32(mod);
    codegen_emit_global_fini32(mod);
    codegen_emit_cxx_array_placement_cleanup_thunks32(mod);

    codegen_emit_cxx_vtables(mod);

    /* Resolve internal function calls */
    resolve_func_calls(mod);

    /* Resolve label references */
    resolve_labels(mod);

    /* These temporary lists are needed only while patching this module. */
    codegen_release_resolution_lists();

    return mod;
}
