/*
 * RCC - RinOS C Compiler
 * x86-32 Code Generator
 */

#include "rcc.h"
#include "ast.h"
#include "symtab.h"
#include "codegen.h"
#include <limits.h>

/* Label management */
static int label_counter = 0;
static int current_stack_offset = 0;

#define INIT_CAPACITY 4096

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
    rel->symbol_name = symbol_name ? rcc_strdup(symbol_name) : NULL;
}

void module_add_tls_relocation(Module* mod,
                               ModuleSymbolSection source_section,
                               uint32_t offset, const char* symbol_name) {
    module_add_relocation(mod, source_section, offset, 0u, false, false,
                          symbol_name);
    mod->relocs_arr[mod->reloc_count - 1].is_tls = true;
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

    if (expression->kind == EXPR_STRING_LIT) {
        *addend = emit_string(mod, expression->str_val);
        module_ensure_rodata_base_symbol(mod);
        *symbol_name = "__rcc_rodata_base";
        return true;
    }
    if (expression->kind == EXPR_ADDR && expression->unary_operand) {
        Expr* addressed = expression->unary_operand;
        Decl* target = NULL;
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
    if (codegen_static_integer(initializer, &integer) && integer == 0) {
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
                    !codegen_emit_static_initializer(
                        mod, field->type, item->expr,
                        (uint32_t)field_offset)) {
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
                    !codegen_emit_tls_initializer(
                        mod, field->type, item->expr,
                        (uint32_t)field_offset)) return false;
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
        return codegen_static_integer(initializer, &constant) && constant == 0;
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
            rcc_error(declaration->loc,
                      "unsupported static initializer for '%s'",
                      declaration->name);
        }
        module_add_symbol(mod, decl_link_name(declaration), offset, true,
                          MODULE_SYMBOL_DATA,
                          declaration->storage != STORAGE_STATIC);
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
                break;
            }
        }
    }
}

static void emit_label(Module* mod, int label) {
    LabelDef* def = rcc_alloc(sizeof(LabelDef));
    def->label = label;
    def->offset = code_offset(mod);
    def->next = label_defs;
    label_defs = def;
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
static void gen_call(Module* mod, Expr* expr);
static void emit_convert_integer_value(Module* mod, int reg,
                                       const Type* source_type,
                                       const Type* target_type);

static bool gen_is_integer64(const Type* type) {
    return type && type->size == 8 &&
           type_is_integer((Type*)type);
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
    emit_mov_reg_imm(mod, EAX, 0u);
    emit_mov_reg_imm(mod, EDX, 0u);
    return true;
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

static void gen_symbol_address(Module* mod, const char* symbol,
                               uint32_t addend) {
    emit_mov_reg_imm(mod, EAX, 0u);
    module_add_relocation(mod, MODULE_SYMBOL_CODE,
                          code_offset(mod) - 4u, addend,
                          false, false, symbol);
    add_reloc(mod, MODULE_SYMBOL_CODE, code_offset(mod) - 4u,
              RIN_RELOC_ABS32);
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
        case EXPR_IDENT: {
            /* Use decl set during semantic analysis */
            Decl* decl = expr->ident_decl;
            if (!decl) {
                emit_mov_reg_imm(mod, EAX, 0);
                break;
            }
            if (decl->kind == DECL_VAR && decl->var_is_thread_local) {
                gen_tls_address(mod, decl_link_name(decl));
                if (decl->type && decl->type->is_reference) {
                    emit_mov_reg_mem(mod, EAX, EAX, 0);
                }
            } else if (decl->kind == DECL_FUNC) {
                gen_symbol_address(mod, decl_link_name(decl), 0u);
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
            if (expr->type && expr->type->size > 1) {
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
                emit_mov_reg_imm(mod, EAX, 0u);
                break;
            }
            if (expr->compound_type->kind == TYPE_ARRAY ||
                expr->compound_type->kind == TYPE_STRUCT ||
                expr->compound_type->kind == TYPE_UNION) {
                gen_zero_local_storage(mod, expr->compound_offset,
                                       (size_t)expr->compound_type->size);
            }
            if (!gen_local_initializer(mod, expr->compound_type, expr,
                                       expr->compound_offset)) {
                rcc_error(expr->loc,
                          "unsupported compound literal initializer");
            }
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
                emit_mov_reg_imm(mod, EAX, 0u);
                break;
            }
            gen_expr(mod, expr);
            emit_byte(mod, 0x8D);  /* LEA EAX, [EBP+disp32] */
            emit_byte(mod, modrm(2, EAX, EBP));
            emit_dword(mod, (uint32_t)expr->call_result_offset);
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
        emit_mov_reg_imm(mod, EAX, 0u);
        emit_mov_reg_imm(mod, EDX, 0u);
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
            if (gen_is_integer64(expr->cond_test->type)) {
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
            emit_mov_reg_imm(mod, EAX, 0u);
            emit_mov_reg_imm(mod, EDX, 0u);
            break;
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

static bool gen_inline_method_call(Module* mod, Expr* expr) {
    TypeMethod* method = expr ? expr->call_method : NULL;
    if (!method || !gen_inline_method_address(mod, expr)) return false;
    if (expr->type &&
        (expr->type->kind == TYPE_STRUCT ||
         expr->type->kind == TYPE_UNION ||
         expr->type->kind == TYPE_ARRAY)) {
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

static void gen_call(Module* mod, Expr* expr) {
    int argument_bytes = 0;
    int argc;
    ExprList** args;
    Type** argument_types;
    Type* function_type;
    TypeParam* parameter;
    int i;
    Expr* func_expr;

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
            gen_lvalue(mod, argument);
            emit_mov_reg_reg(mod, ECX, EAX);
            for (int unit = units - 1; unit >= 0; --unit) {
                emit_mov_reg_mem(mod, EAX, ECX, unit * 4);
                emit_push_reg(mod, EAX);
            }
            argument_bytes += units * 4;
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
    if (func_expr->kind == EXPR_IDENT && func_expr->ident_decl &&
        func_expr->ident_decl->kind == DECL_FUNC) {
        Decl* func_decl = func_expr->ident_decl;
        uint32_t call_offset;

        emit_byte(mod, 0xE8);
        call_offset = code_offset(mod);
        emit_dword(mod, 0);
        if (func_decl->func_body) {
            add_func_call_ref(decl_link_name(func_decl), call_offset);
        } else {
            /* All external direct calls use the same rel32 contract.  RLD
             * resolves linked definitions directly and materializes a code
             * thunk when the symbol is a dynamic function import. */
            module_add_relocation(mod, MODULE_SYMBOL_CODE,
                                  call_offset, 0, true, false,
                                  decl_link_name(func_decl));
        }
    } else {
        gen_expr(mod, func_expr);
        emit_byte(mod, 0xFF);
        emit_byte(mod, modrm(3, 2, EAX));
    }

    if (argument_bytes > 0) {
        emit_add_reg_imm(mod, ESP, argument_bytes);
    }
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

        case EXPR_STRING_LIT: {
            uint32_t offset = emit_string(mod, expr->str_val);
            module_ensure_rodata_base_symbol(mod);
            gen_symbol_address(mod, "__rcc_rodata_base", offset);
            break;
        }

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
                    emit_load_typed32(mod, EAX, EAX, 0, expr->type);
                }
            } else if (decl->var_is_thread_local) {
                gen_lvalue(mod, expr);
                emit_load_typed32(mod, EAX, EAX, 0, decl->type);
            } else if (decl->type && decl->type->kind == TYPE_ARRAY) {
                gen_lvalue(mod, expr);
            } else if (decl->var_is_global) {
                gen_symbol_address(mod, decl_link_name(decl), 0u);
                emit_load_typed32(mod, EAX, EAX, 0, decl->type);
            } else {
                emit_load_typed32(mod, EAX, EBP, decl->var_offset,
                                  decl->type);
            }
            break;
        }

        case EXPR_NEG:
            gen_expr(mod, expr->unary_operand);
            emit_neg_reg(mod, EAX);
            break;

        case EXPR_BITNOT:
            gen_expr(mod, expr->unary_operand);
            emit_not_reg(mod, EAX);
            break;

        case EXPR_NOT:
            gen_expr(mod, expr->unary_operand);
            emit_test_scalar_value(mod, expr->unary_operand->type);
            emit_setcc(mod, CC_E, EAX);
            emit_byte(mod, 0x0F);  /* MOVZX EAX, AL */
            emit_byte(mod, 0xB6);
            emit_byte(mod, modrm(3, EAX, EAX));
            break;

        case EXPR_ADDR:
            gen_lvalue(mod, expr->unary_operand);
            break;

        case EXPR_DEREF:
            gen_expr(mod, expr->unary_operand);
            emit_load_typed32(mod, EAX, EAX, 0, expr->type);
            break;

        case EXPR_PREINC:
        case EXPR_PREDEC:
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
            gen_expr(mod, expr->binary_lhs);
            emit_push_reg(mod, EAX);
            gen_expr(mod, expr->binary_rhs);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_pop_reg(mod, EAX);
            emit_imul_reg_reg(mod, EAX, ECX);
            break;

        case EXPR_DIV:
        case EXPR_MOD:
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
            if (expr->binary_lhs->type &&
                (expr->binary_lhs->type->kind == TYPE_STRUCT ||
                 expr->binary_lhs->type->kind == TYPE_UNION)) {
                int offset = 0;
                if (expr->binary_rhs->kind == EXPR_ASSIGN) {
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
            int step = (expr->va_arg_type->size + 3) & ~3;
            gen_lvalue(mod, expr->va_list_operand);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_mov_reg_mem(mod, EAX, ECX, 0);
            emit_mov_reg_reg(mod, EDX, EAX);
            emit_add_reg_imm(mod, EAX, step);
            emit_mov_mem_reg(mod, ECX, 0, EAX);
            emit_load_typed32(mod, EAX, EDX, 0, expr->va_arg_type);
            break;
        }

        case EXPR_INDEX:
            gen_lvalue(mod, expr);
            if (!expr->type || expr->type->kind != TYPE_ARRAY) {
                emit_load_typed32(mod, EAX, EAX, 0, expr->type);
            }
            break;

        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            gen_lvalue(mod, expr);
            if (!expr->type || expr->type->kind != TYPE_ARRAY) {
                emit_load_typed32(mod, EAX, EAX, 0, expr->type);
            }
            break;

        case EXPR_COMPOUND:
            gen_lvalue(mod, expr);
            if (!expr->type || (expr->type->kind != TYPE_ARRAY &&
                                expr->type->kind != TYPE_STRUCT &&
                                expr->type->kind != TYPE_UNION)) {
                emit_load_typed32(mod, EAX, EAX, 0, expr->type);
            }
            break;

        case EXPR_CAST:
            gen_expr(mod, expr->cast_expr);
            if (type_is_integer(expr->type) ||
                expr->type->kind == TYPE_ENUM) {
                emit_convert_integer_value(mod, EAX,
                                           expr->cast_expr->type,
                                           expr->type);
            }
            break;

        case EXPR_SIZEOF:
            if (expr->sizeof_type) {
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
            emit_mov_reg_imm(mod, EAX, 0);
            break;
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

static uint8_t asm_immediate8(const char* text) {
    if (text && text[0] == '$') ++text;
    return (uint8_t)strtoull(text ? text : "0", NULL, 0);
}

/* Encode a single x86 instruction from mnemonic and operands */
static void emit_asm_instruction(Module* mod, const char* mnemonic,
                                  const char* op1, const char* op2) {
    /* Common instructions used in syscall/interrupt context */
    if (strcmp(mnemonic, "int") == 0) {
        emit_byte(mod, 0xCD);
        emit_byte(mod, asm_immediate8(op1));
    }
    else if (strcmp(mnemonic, "int3") == 0) {
        emit_byte(mod, 0xCC);
    }
    else if (strcmp(mnemonic, "syscall") == 0) {
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0x05);
    }
    else if (strcmp(mnemonic, "sysenter") == 0) {
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0x34);
    }
    else if (strcmp(mnemonic, "nop") == 0) {
        emit_byte(mod, 0x90);
    }
    else if (strcmp(mnemonic, "hlt") == 0) {
        emit_byte(mod, 0xF4);
    }
    else if (strcmp(mnemonic, "cli") == 0) {
        emit_byte(mod, 0xFA);
    }
    else if (strcmp(mnemonic, "sti") == 0) {
        emit_byte(mod, 0xFB);
    }
    else if (strcmp(mnemonic, "cld") == 0) {
        emit_byte(mod, 0xFC);
    }
    else if (strcmp(mnemonic, "std") == 0) {
        emit_byte(mod, 0xFD);
    }
    else if (strcmp(mnemonic, "pushf") == 0 || strcmp(mnemonic, "pushfl") == 0) {
        emit_byte(mod, 0x9C);
    }
    else if (strcmp(mnemonic, "popf") == 0 || strcmp(mnemonic, "popfl") == 0) {
        emit_byte(mod, 0x9D);
    }
    else if (strcmp(mnemonic, "ret") == 0) {
        emit_byte(mod, 0xC3);
    }
    else if (strcmp(mnemonic, "leave") == 0) {
        emit_byte(mod, 0xC9);
    }
    else if (strcmp(mnemonic, "cpuid") == 0) {
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xA2);
    }
    else if (strcmp(mnemonic, "rdtsc") == 0) {
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0x31);
    }
    else if (strcmp(mnemonic, "rdmsr") == 0) {
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0x32);
    }
    else if (strcmp(mnemonic, "wrmsr") == 0) {
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0x30);
    }
    else if (strcmp(mnemonic, "invlpg") == 0) {
        /* invlpg [eax] - assume address in EAX */
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0x01);
        emit_byte(mod, 0x38);  /* /7 [EAX] */
    }
    else if (strcmp(mnemonic, "wbinvd") == 0) {
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0x09);
    }
    else if (strcmp(mnemonic, "pause") == 0) {
        emit_byte(mod, 0xF3);
        emit_byte(mod, 0x90);
    }
    else if (strcmp(mnemonic, "mfence") == 0) {
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xAE);
        emit_byte(mod, 0xF0);
    }
    else if (strcmp(mnemonic, "lfence") == 0) {
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xAE);
        emit_byte(mod, 0xE8);
    }
    else if (strcmp(mnemonic, "sfence") == 0) {
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xAE);
        emit_byte(mod, 0xF8);
    }
    else if (strcmp(mnemonic, "xchg") == 0) {
        /* xchg eax, eax is nop */
        emit_byte(mod, 0x90);
    }
    /* Memory barrier represented as lock prefix with nop-like op */
    else if (strcmp(mnemonic, "lock") == 0) {
        emit_byte(mod, 0xF0);  /* LOCK prefix */
    }
    /* I/O instructions */
    else if (strcmp(mnemonic, "inb") == 0) {
        if (op2 && strcmp(op2, "%dx") == 0) {
            emit_byte(mod, 0xEC);  /* in al, dx */
        } else {
            emit_byte(mod, 0xE4);  /* in al, imm8 */
            emit_byte(mod, asm_immediate8(op2));
        }
    }
    else if (strcmp(mnemonic, "inw") == 0) {
        emit_byte(mod, 0x66);
        if (op2 && strcmp(op2, "%dx") == 0) {
            emit_byte(mod, 0xED);
        } else {
            emit_byte(mod, 0xE5);
            emit_byte(mod, asm_immediate8(op2));
        }
    }
    else if (strcmp(mnemonic, "inl") == 0) {
        if (op2 && strcmp(op2, "%dx") == 0) {
            emit_byte(mod, 0xED);
        } else {
            emit_byte(mod, 0xE5);
            emit_byte(mod, asm_immediate8(op2));
        }
    }
    else if (strcmp(mnemonic, "outb") == 0) {
        if (op1 && strcmp(op1, "%dx") == 0) {
            emit_byte(mod, 0xEE);  /* out dx, al */
        } else {
            emit_byte(mod, 0xE6);  /* out imm8, al */
            emit_byte(mod, asm_immediate8(op1));
        }
    }
    else if (strcmp(mnemonic, "outw") == 0) {
        emit_byte(mod, 0x66);
        if (op1 && strcmp(op1, "%dx") == 0) {
            emit_byte(mod, 0xEF);
        } else {
            emit_byte(mod, 0xE7);
            emit_byte(mod, asm_immediate8(op1));
        }
    }
    else if (strcmp(mnemonic, "outl") == 0) {
        if (op1 && strcmp(op1, "%dx") == 0) {
            emit_byte(mod, 0xEF);
        } else {
            emit_byte(mod, 0xE7);
            emit_byte(mod, asm_immediate8(op1));
        }
    }
    /* Default: skip unknown instructions with a warning */
    /* In a real compiler, we'd report an error */
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
            emit_asm_instruction(mod, mnemonic,
                                 op1[0] ? op1 : NULL,
                                 op2[0] ? op2 : NULL);
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
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
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
                    !gen_local_initializer(mod, field->type, item->expr,
                                           (int32_t)field_offset)) {
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
        gen_lvalue(mod, initializer);
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
    if (!type_is_integer(type) && type->kind != TYPE_ENUM &&
        type->kind != TYPE_PTR) {
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
    if (type_is_integer(expr->type) || expr->type->kind == TYPE_ENUM) {
        emit_normalize_atomic_value(mod, EAX, expr->type);
    }
}

typedef struct CleanupCodegen {
    Expr* expression;
    struct CleanupCodegen* previous;
} CleanupCodegen;

static CleanupCodegen* active_cleanups = NULL;

static void gen_cleanups_until(Module* mod, CleanupCodegen* marker) {
    for (CleanupCodegen* item = active_cleanups; item && item != marker;
         item = item->previous) {
        gen_expr(mod, item->expression);
    }
}

static void discard_cleanups_until(CleanupCodegen* marker) {
    while (active_cleanups && active_cleanups != marker) {
        CleanupCodegen* previous = active_cleanups->previous;
        rcc_free(active_cleanups);
        active_cleanups = previous;
    }
}

static void gen_scoped_stmt(Module* mod, Stmt* statement) {
    CleanupCodegen* marker = active_cleanups;
    gen_stmt(mod, statement);
    gen_cleanups_until(mod, marker);
    discard_cleanups_until(marker);
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
            for (StmtList* s = stmt->block_stmts; s; s = s->next) {
                gen_stmt(mod, s->stmt);
            }
            gen_cleanups_until(mod, marker);
            discard_cleanups_until(marker);
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
            int start_label = new_label();
            int end_label = new_label();
            int old_break = break_label;
            int old_continue = continue_label;
            break_label = end_label;
            continue_label = start_label;

            emit_label(mod, start_label);
            gen_expr(mod, stmt->while_cond);
            emit_test_scalar_value(mod, stmt->while_cond->type);
            emit_jcc_label(mod, CC_E, end_label);

            gen_scoped_stmt(mod, stmt->while_body);

            emit_jmp_label(mod, start_label);
            emit_label(mod, end_label);

            break_label = old_break;
            continue_label = old_continue;
            break;
        }

        case STMT_DO: {
            int start_label = new_label();
            int end_label = new_label();
            int cond_label = new_label();
            int old_break = break_label;
            int old_continue = continue_label;
            break_label = end_label;
            continue_label = cond_label;

            emit_label(mod, start_label);
            gen_scoped_stmt(mod, stmt->while_body);

            emit_label(mod, cond_label);
            gen_expr(mod, stmt->while_cond);
            emit_test_scalar_value(mod, stmt->while_cond->type);
            emit_jcc_label(mod, CC_NE, start_label);

            emit_label(mod, end_label);

            break_label = old_break;
            continue_label = old_continue;
            break;
        }

        case STMT_FOR: {
            CleanupCodegen* marker = active_cleanups;
            int start_label = new_label();
            int end_label = new_label();
            int inc_label = new_label();
            int old_break = break_label;
            int old_continue = continue_label;
            break_label = end_label;
            continue_label = inc_label;

            if (stmt->for_init) {
                gen_stmt(mod, stmt->for_init);
            }

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

            break_label = old_break;
            continue_label = old_continue;
            break;
        }

        case STMT_SWITCH: {
            SwitchCodegenContext context = {0};
            SwitchCodegenContext* old_switch = current_switch_codegen;
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
            current_switch_codegen = &context;
            gen_stmt(mod, stmt->switch_body);
            current_switch_codegen = old_switch;
            break_label = old_break;
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
            emit_jmp_label(mod, codegen_named_label(stmt->goto_label));
            break;

        case STMT_LABEL:
            emit_label(mod, codegen_named_label(stmt->label_name));
            gen_stmt(mod, stmt->label_stmt);
            break;

        case STMT_RETURN:
            if (stmt->return_val) {
                if (current_function_return_type &&
                    (current_function_return_type->kind == TYPE_STRUCT ||
                     current_function_return_type->kind == TYPE_UNION)) {
                    int offset = 0;
                    gen_lvalue(mod, stmt->return_val);
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
            emit_leave(mod);
            emit_ret(mod);
            break;

        case STMT_BREAK:
            if (break_label >= 0) {
                emit_jmp_label(mod, break_label);
            }
            break;

        case STMT_CONTINUE:
            if (continue_label >= 0) {
                emit_jmp_label(mod, continue_label);
            }
            break;

        case STMT_DECL: {
            Decl* d = stmt->decl;
            if (d->kind == DECL_VAR && d->var_init) {
                if (d->type && (d->type->kind == TYPE_ARRAY ||
                                d->type->kind == TYPE_STRUCT ||
                                d->type->kind == TYPE_UNION)) {
                    gen_zero_local_storage(mod, d->var_offset,
                                           (size_t)d->type->size);
                }
                if (!gen_local_initializer(mod, d->type, d->var_init,
                                           d->var_offset)) {
                    rcc_error(d->loc, "unsupported local initializer for '%s'",
                              d->name);
                }
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
    if (!decl->func_body) return;

    stack_size = codegen_required_local_bytes(decl->func_body);
    stack_size = codegen_assign_compound_storage(decl->func_body, stack_size,
                                                 4);
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

    /* Generate body */
    old_return_type = current_function_return_type;
    current_function_return_type = decl->type &&
                                   decl->type->kind == TYPE_FUNC
        ? decl->type->ret_type : NULL;
    old_cleanups = active_cleanups;
    active_cleanups = NULL;
    named_codegen_labels = NULL;
    gen_stmt(mod, decl->func_body);
    discard_cleanups_until(NULL);
    active_cleanups = old_cleanups;
    codegen_release_named_labels();
    current_function_return_type = old_return_type;

    /* Function epilogue (fallthrough return) */
    emit_mov_reg_imm(mod, EAX, 0);
    if (gen_is_integer64(decl->type && decl->type->kind == TYPE_FUNC
                         ? decl->type->ret_type : NULL)) {
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

    /* Resolve internal function calls */
    resolve_func_calls(mod);

    /* Resolve label references */
    resolve_labels(mod);

    return mod;
}
