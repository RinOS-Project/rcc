/*
 * RCC - RinOS C Compiler
 * x86-32 Code Generator
 */

#include "rcc.h"
#include "ast.h"
#include "symtab.h"
#include "codegen.h"

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

    mod->data.data = rcc_alloc(INIT_CAPACITY);
    mod->data.size = 0;
    mod->data.capacity = INIT_CAPACITY;

    mod->bss.size = 0;
    mod->bss.align = 1u;

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
    if (!mod) return;
    rcc_free(mod->code.data);
    rcc_free(mod->data.data);
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
}

void module_add_relocation(Module* mod, uint32_t offset, uint32_t target,
                           bool is_relative, bool is_64bit,
                           const char* symbol_name) {
    /* Expand if needed */
    if (mod->reloc_count >= mod->reloc_capacity) {
        int new_cap = mod->reloc_capacity == 0 ? 16 : mod->reloc_capacity * 2;
        mod->relocs_arr = rcc_realloc(mod->relocs_arr, new_cap * sizeof(ModuleReloc));
        mod->reloc_capacity = new_cap;
    }

    ModuleReloc* rel = &mod->relocs_arr[mod->reloc_count++];
    rel->offset = offset;
    rel->target = target;
    rel->is_relative = is_relative;
    rel->is_64bit = is_64bit;
    rel->symbol_name = symbol_name ? rcc_strdup(symbol_name) : NULL;
}

bool module_resolve_image_relocation(const Module* mod, uint32_t offset,
                                     bool is_64bit, uint64_t data_rva,
                                     uint64_t bss_rva, uint64_t* value) {
    const ModuleReloc* relocation = NULL;
    const ModuleSymbol* symbol = NULL;
    uint64_t base;

    if (!mod || !value) return false;
    for (int index = 0; index < mod->reloc_count; ++index) {
        const ModuleReloc* candidate = &mod->relocs_arr[index];
        if (candidate->offset == offset && !candidate->is_relative &&
            candidate->is_64bit == is_64bit) {
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
        if (declaration->storage == STORAGE_EXTERN &&
            !declaration->var_init) {
            module_add_symbol(mod, declaration->name, 0u, false,
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
            module_add_symbol(mod, declaration->name, offset, true,
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
        if (declaration->var_init &&
            (declaration->var_init->kind == EXPR_INT_LIT ||
             declaration->var_init->kind == EXPR_CHAR_LIT)) {
            uint64_t initial = declaration->var_init->kind == EXPR_INT_LIT
                ? (uint64_t)declaration->var_init->int_val
                : (uint64_t)(uint8_t)declaration->var_init->char_val;
            uint32_t initial_size = (uint32_t)declaration->type->size;
            if (initial_size > 8u) initial_size = 8u;
            for (uint32_t byte = 0u; byte < initial_size; ++byte) {
                mod->data.data[offset + byte] =
                    (uint8_t)(initial >> (byte * 8u));
            }
        }
        module_add_symbol(mod, declaration->name, offset, true,
                          MODULE_SYMBOL_DATA,
                          declaration->storage != STORAGE_STATIC);
    }
}

/* ═══════════════════════════════════════
 * Data Section
 * ═══════════════════════════════════════ */

static void ensure_data_capacity(Module* mod, size_t needed) {
    if (mod->data.size + needed > mod->data.capacity) {
        mod->data.capacity *= 2;
        mod->data.data = rcc_realloc(mod->data.data, mod->data.capacity);
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
    uint32_t offset = (uint32_t)mod->data.size;

    ensure_data_capacity(mod, len);
    memcpy(mod->data.data + mod->data.size, str, len);
    mod->data.size += len;

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

void add_reloc(Module* mod, uint32_t offset, uint32_t type) {
    Reloc* r = rcc_alloc(sizeof(Reloc));
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

static void emit_mov_reg_mem(Module* mod, int reg, int base, int32_t disp) {
    emit_byte(mod, 0x8B);
    if (disp == 0 && base != EBP) {
        emit_byte(mod, modrm(0, reg, base));
    } else if (disp >= -128 && disp <= 127) {
        emit_byte(mod, modrm(1, reg, base));
        emit_byte(mod, (uint8_t)disp);
    } else {
        emit_byte(mod, modrm(2, reg, base));
        emit_dword(mod, (uint32_t)disp);
    }
}

static void emit_mov_mem_reg(Module* mod, int base, int32_t disp, int src) {
    emit_byte(mod, 0x89);
    if (disp == 0 && base != EBP) {
        emit_byte(mod, modrm(0, src, base));
    } else if (disp >= -128 && disp <= 127) {
        emit_byte(mod, modrm(1, src, base));
        emit_byte(mod, (uint8_t)disp);
    } else {
        emit_byte(mod, modrm(2, src, base));
        emit_dword(mod, (uint32_t)disp);
    }
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

static void emit_imul_reg_reg(Module* mod, int dst, int src) {
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0xAF);
    emit_byte(mod, modrm(3, dst, src));
}

static void emit_idiv_reg(Module* mod, int reg) {
    emit_byte(mod, 0xF7);
    emit_byte(mod, modrm(3, 7, reg));
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
static void gen_stmt(Module* mod, Stmt* stmt);

static void gen_symbol_address(Module* mod, const char* symbol,
                               uint32_t addend) {
    emit_mov_reg_imm(mod, EAX, 0u);
    module_add_relocation(mod, code_offset(mod) - 4u, addend,
                          false, false, symbol);
    add_reloc(mod, code_offset(mod) - 4u, RIN_RELOC_ABS32);
}

static void gen_ensure_data_base_symbol(Module* mod) {
    for (int index = 0; index < mod->symbol_count; ++index) {
        if (strcmp(mod->symbols[index].name, "__rcc_data_base") == 0) return;
    }
    module_add_symbol(mod, "__rcc_data_base", 0u, true,
                      MODULE_SYMBOL_DATA, false);
}

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
            if (decl->kind == DECL_FUNC || decl->var_is_global) {
                gen_symbol_address(mod, decl->name, 0u);
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

        default:
            rcc_error(expr->loc, "not an lvalue");
            break;
    }
}

static void gen_expr(Module* mod, Expr* expr) {
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
            gen_ensure_data_base_symbol(mod);
            gen_symbol_address(mod, "__rcc_data_base", offset);
            break;
        }

        case EXPR_IDENT: {
            Decl* decl = expr->ident_decl;
            if (!decl) {
                emit_mov_reg_imm(mod, EAX, 0);
                break;
            }
            if (decl->kind == DECL_FUNC) {
                gen_symbol_address(mod, decl->name, 0u);
            } else if (decl->var_is_global) {
                gen_symbol_address(mod, decl->name, 0u);
                emit_mov_reg_mem(mod, EAX, EAX, 0);
            } else {
                emit_mov_reg_mem(mod, EAX, EBP, decl->var_offset);
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
            emit_cmp_reg_imm(mod, EAX, 0);
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
            emit_mov_reg_mem(mod, EAX, EAX, 0);
            break;

        case EXPR_PREINC:
        case EXPR_PREDEC:
            gen_lvalue(mod, expr->unary_operand);
            emit_push_reg(mod, EAX);
            emit_mov_reg_mem(mod, EAX, EAX, 0);
            if (expr->kind == EXPR_PREINC) {
                emit_add_reg_imm(mod, EAX, 1);
            } else {
                emit_sub_reg_imm(mod, EAX, 1);
            }
            emit_pop_reg(mod, ECX);
            emit_mov_mem_reg(mod, ECX, 0, EAX);
            break;

        case EXPR_POSTINC:
        case EXPR_POSTDEC:
            gen_lvalue(mod, expr->unary_operand);
            emit_push_reg(mod, EAX);
            emit_mov_reg_mem(mod, EAX, EAX, 0);
            emit_mov_reg_reg(mod, EDX, EAX);
            if (expr->kind == EXPR_POSTINC) {
                emit_add_reg_imm(mod, EDX, 1);
            } else {
                emit_sub_reg_imm(mod, EDX, 1);
            }
            emit_pop_reg(mod, ECX);
            emit_mov_mem_reg(mod, ECX, 0, EDX);
            break;

        case EXPR_ADD:
            gen_expr(mod, expr->binary_lhs);
            emit_push_reg(mod, EAX);
            gen_expr(mod, expr->binary_rhs);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_pop_reg(mod, EAX);
            emit_add_reg_reg(mod, EAX, ECX);
            break;

        case EXPR_SUB:
            gen_expr(mod, expr->binary_lhs);
            emit_push_reg(mod, EAX);
            gen_expr(mod, expr->binary_rhs);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_pop_reg(mod, EAX);
            emit_sub_reg_reg(mod, EAX, ECX);
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
            emit_cdq(mod);
            emit_idiv_reg(mod, ECX);
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
            gen_expr(mod, expr->binary_lhs);
            emit_push_reg(mod, EAX);
            gen_expr(mod, expr->binary_rhs);
            emit_mov_reg_reg(mod, ECX, EAX);
            emit_pop_reg(mod, EAX);
            emit_cmp_reg_reg(mod, EAX, ECX);

            int cc;
            switch (expr->kind) {
                case EXPR_EQ: cc = CC_E; break;
                case EXPR_NE: cc = CC_NE; break;
                case EXPR_LT: cc = CC_L; break;
                case EXPR_GT: cc = CC_G; break;
                case EXPR_LE: cc = CC_LE; break;
                case EXPR_GE: cc = CC_GE; break;
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
            emit_test_reg_reg(mod, EAX, EAX);
            emit_jcc_label(mod, CC_E, end_label);
            gen_expr(mod, expr->binary_rhs);
            emit_test_reg_reg(mod, EAX, EAX);
            emit_setcc(mod, CC_NE, EAX);
            emit_byte(mod, 0x0F);
            emit_byte(mod, 0xB6);
            emit_byte(mod, modrm(3, EAX, EAX));
            emit_label(mod, end_label);
            break;
        }

        case EXPR_OR: {
            int end_label = new_label();
            gen_expr(mod, expr->binary_lhs);
            emit_test_reg_reg(mod, EAX, EAX);
            emit_jcc_label(mod, CC_NE, end_label);
            gen_expr(mod, expr->binary_rhs);
            emit_label(mod, end_label);
            emit_test_reg_reg(mod, EAX, EAX);
            emit_setcc(mod, CC_NE, EAX);
            emit_byte(mod, 0x0F);
            emit_byte(mod, 0xB6);
            emit_byte(mod, modrm(3, EAX, EAX));
            break;
        }

        case EXPR_ASSIGN:
            gen_expr(mod, expr->binary_rhs);
            emit_push_reg(mod, EAX);
            gen_lvalue(mod, expr->binary_lhs);
            emit_pop_reg(mod, ECX);
            emit_mov_mem_reg(mod, EAX, 0, ECX);
            emit_mov_reg_reg(mod, EAX, ECX);
            break;

        case EXPR_COND: {
            int else_label = new_label();
            int end_label = new_label();
            gen_expr(mod, expr->cond_test);
            emit_test_reg_reg(mod, EAX, EAX);
            emit_jcc_label(mod, CC_E, else_label);
            gen_expr(mod, expr->cond_then);
            emit_jmp_label(mod, end_label);
            emit_label(mod, else_label);
            gen_expr(mod, expr->cond_else);
            emit_label(mod, end_label);
            break;
        }

        case EXPR_CALL: {
            /* Push arguments in reverse order */
            int argc = exprlist_len(expr->call_args);
            ExprList** args = rcc_alloc(argc * sizeof(ExprList*));
            int i = 0;
            for (ExprList* a = expr->call_args; a; a = a->next) {
                args[i++] = a;
            }
            for (i = argc - 1; i >= 0; i--) {
                gen_expr(mod, args[i]->expr);
                emit_push_reg(mod, EAX);
            }
            rcc_free(args);

            /* Check if this is a direct function call */
            Expr* func_expr = expr->call_func;
            if (func_expr->kind == EXPR_IDENT && func_expr->ident_decl &&
                func_expr->ident_decl->kind == DECL_FUNC) {
                Decl* func_decl = func_expr->ident_decl;

                /* Emit CALL rel32 */
                emit_byte(mod, 0xE8);  /* CALL rel32 */
                uint32_t call_offset = code_offset(mod);
                emit_dword(mod, 0);  /* Placeholder */

                if (func_decl->func_body) {
                    /* Internal function - record for later patching */
                    add_func_call_ref(func_decl->name, call_offset);
                } else {
                    /* call dword ptr [absolute slot]; NDRV/RIN v3 uses an
                     * 8-byte import slot even though i686 consumes low32. */
                    mod->code.size = call_offset - 1u;
                    emit_byte(mod, 0xFF);
                    emit_byte(mod, 0x15);
                    call_offset = code_offset(mod);
                    emit_dword(mod, 0u);
                    module_add_relocation(mod, call_offset, 0, false, false,
                                          func_decl->name);
                }
            } else {
                /* Indirect call through function pointer */
                gen_expr(mod, expr->call_func);
                emit_byte(mod, 0xFF);  /* CALL EAX */
                emit_byte(mod, modrm(3, 2, EAX));
            }

            /* Clean up arguments */
            if (argc > 0) {
                emit_add_reg_imm(mod, ESP, argc * 4);
            }
            break;
        }

        case EXPR_INDEX:
            gen_lvalue(mod, expr);
            emit_mov_reg_mem(mod, EAX, EAX, 0);
            break;

        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            gen_lvalue(mod, expr);
            emit_mov_reg_mem(mod, EAX, EAX, 0);
            break;

        case EXPR_CAST:
            gen_expr(mod, expr->cast_expr);
            /* Most casts are no-ops in 32-bit */
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

/* Check if constraint is output (starts with = or +) */
static bool is_output_constraint(const char* constraint) {
    return constraint[0] == '=' || constraint[0] == '+';
}

/* Encode a single x86 instruction from mnemonic and operands */
static void emit_asm_instruction(Module* mod, const char* mnemonic,
                                  const char* op1, const char* op2) {
    /* Common instructions used in syscall/interrupt context */
    if (strcmp(mnemonic, "int") == 0) {
        emit_byte(mod, 0xCD);
        if (op1 && op1[0] == '$') {
            emit_byte(mod, (uint8_t)atoi(op1 + 1));
        } else if (op1) {
            emit_byte(mod, (uint8_t)atoi(op1));
        }
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
            if (op2 && op2[0] == '$') {
                emit_byte(mod, (uint8_t)atoi(op2 + 1));
            }
        }
    }
    else if (strcmp(mnemonic, "inw") == 0) {
        emit_byte(mod, 0x66);
        if (op2 && strcmp(op2, "%dx") == 0) {
            emit_byte(mod, 0xED);
        } else {
            emit_byte(mod, 0xE5);
            if (op2 && op2[0] == '$') {
                emit_byte(mod, (uint8_t)atoi(op2 + 1));
            }
        }
    }
    else if (strcmp(mnemonic, "inl") == 0) {
        if (op2 && strcmp(op2, "%dx") == 0) {
            emit_byte(mod, 0xED);
        } else {
            emit_byte(mod, 0xE5);
            if (op2 && op2[0] == '$') {
                emit_byte(mod, (uint8_t)atoi(op2 + 1));
            }
        }
    }
    else if (strcmp(mnemonic, "outb") == 0) {
        if (op1 && strcmp(op1, "%dx") == 0) {
            emit_byte(mod, 0xEE);  /* out dx, al */
        } else {
            emit_byte(mod, 0xE6);  /* out imm8, al */
            if (op1 && op1[0] == '$') {
                emit_byte(mod, (uint8_t)atoi(op1 + 1));
            }
        }
    }
    else if (strcmp(mnemonic, "outw") == 0) {
        emit_byte(mod, 0x66);
        if (op1 && strcmp(op1, "%dx") == 0) {
            emit_byte(mod, 0xEF);
        } else {
            emit_byte(mod, 0xE7);
            if (op1 && op1[0] == '$') {
                emit_byte(mod, (uint8_t)atoi(op1 + 1));
            }
        }
    }
    else if (strcmp(mnemonic, "outl") == 0) {
        if (op1 && strcmp(op1, "%dx") == 0) {
            emit_byte(mod, 0xEF);
        } else {
            emit_byte(mod, 0xE7);
            if (op1 && op1[0] == '$') {
                emit_byte(mod, (uint8_t)atoi(op1 + 1));
            }
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
    /* AsmClobber* clobbers = stmt->asm_clobbers; */

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

    /* Load inputs into registers */
    for (int i = output_count; i < total_operands; i++) {
        if (operands[i].reg >= 0 && operands[i].op->expr) {
            gen_expr(mod, operands[i].op->expr);
            if (operands[i].reg != EAX) {
                emit_mov_reg_reg(mod, operands[i].reg, EAX);
            }
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
            emit_mov_mem_reg(mod, ECX, 0, EAX);   /* Store */
        }
    }

    if (operands) {
        rcc_free(operands);
    }
}

/* ═══════════════════════════════════════
 * Statement Code Generation
 * ═══════════════════════════════════════ */

static int break_label = -1;
static int continue_label = -1;

static void gen_stmt(Module* mod, Stmt* stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case STMT_EXPR:
            if (stmt->expr) {
                gen_expr(mod, stmt->expr);
            }
            break;

        case STMT_BLOCK:
            for (StmtList* s = stmt->block_stmts; s; s = s->next) {
                gen_stmt(mod, s->stmt);
            }
            break;

        case STMT_IF: {
            int else_label = new_label();
            int end_label = new_label();

            gen_expr(mod, stmt->if_cond);
            emit_test_reg_reg(mod, EAX, EAX);
            emit_jcc_label(mod, CC_E, else_label);

            gen_stmt(mod, stmt->if_then);

            if (stmt->if_else) {
                emit_jmp_label(mod, end_label);
            }

            emit_label(mod, else_label);

            if (stmt->if_else) {
                gen_stmt(mod, stmt->if_else);
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
            emit_test_reg_reg(mod, EAX, EAX);
            emit_jcc_label(mod, CC_E, end_label);

            gen_stmt(mod, stmt->while_body);

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
            gen_stmt(mod, stmt->while_body);

            emit_label(mod, cond_label);
            gen_expr(mod, stmt->while_cond);
            emit_test_reg_reg(mod, EAX, EAX);
            emit_jcc_label(mod, CC_NE, start_label);

            emit_label(mod, end_label);

            break_label = old_break;
            continue_label = old_continue;
            break;
        }

        case STMT_FOR: {
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
                emit_test_reg_reg(mod, EAX, EAX);
                emit_jcc_label(mod, CC_E, end_label);
            }

            gen_stmt(mod, stmt->for_body);

            emit_label(mod, inc_label);
            if (stmt->for_inc) {
                gen_expr(mod, stmt->for_inc);
            }

            emit_jmp_label(mod, start_label);
            emit_label(mod, end_label);

            break_label = old_break;
            continue_label = old_continue;
            break;
        }

        case STMT_RETURN:
            if (stmt->return_val) {
                gen_expr(mod, stmt->return_val);
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
                gen_expr(mod, d->var_init);
                emit_mov_mem_reg(mod, EBP, d->var_offset, EAX);
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
    if (!decl->func_body) return;

    /* Calculate stack size */
    int stack_size = 0;
    /* TODO: Count local variables */
    stack_size = 64;  /* Default */

    /* Function prologue */
    emit_push_reg(mod, EBP);
    emit_mov_reg_reg(mod, EBP, ESP);
    if (stack_size > 0) {
        emit_sub_reg_imm(mod, ESP, stack_size);
    }

    /* Generate body */
    gen_stmt(mod, decl->func_body);

    /* Function epilogue (fallthrough return) */
    emit_mov_reg_imm(mod, EAX, 0);
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
            module_add_symbol(mod, d->decl->name, 0, false,
                              MODULE_SYMBOL_CODE, true);
        }
    }

    /* Second pass: generate code for each defined function */
    for (DeclList* d = ast->decls; d; d = d->next) {
        if (d->decl->kind == DECL_FUNC && d->decl->func_body) {
            /* Record function start offset */
            uint32_t func_start = code_offset(mod);

            /* Record function definition for call resolution */
            add_func_def(d->decl->name, func_start);

            /* Record entry point for main */
            if (strcmp(d->decl->name, "main") == 0) {
                mod->entry_point = func_start;
            }

            gen_function(mod, d->decl);

            /* Add symbol for function */
            module_add_symbol(mod, d->decl->name, func_start, true,
                              MODULE_SYMBOL_CODE,
                             d->decl->storage != STORAGE_STATIC);
        }
    }

    /* Resolve internal function calls */
    resolve_func_calls(mod);

    /* Resolve label references */
    resolve_labels(mod);

    return mod;
}
