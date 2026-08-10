/*
 * RCC - RinOS C Compiler
 * x86-64 Code Generator
 */

#include "rcc.h"
#include "ast.h"
#include "symtab.h"
#include "codegen.h"
#include <limits.h>

/* Only compile if generating 64-bit code */
#if 1

/* Label management */
static int label_counter64 = 0;

/* ═══════════════════════════════════════
 * x86-64 Registers
 * ═══════════════════════════════════════ */

/* 64-bit registers */
#define RAX 0
#define RCX 1
#define RDX 2
#define RBX 3
#define RSP 4
#define RBP 5
#define RSI 6
#define RDI 7
#define R8  8
#define R9  9
#define R10 10
#define R11 11
#define R12 12
#define R13 13
#define R14 14
#define R15 15

/* REX prefix bits */
#define REX_W 0x48  /* 64-bit operand size */
#define REX_R 0x44  /* Extension of ModR/M reg field */
#define REX_X 0x42  /* Extension of SIB index field */
#define REX_B 0x41  /* Extension of ModR/M r/m, SIB base, or opcode reg */

/* ═══════════════════════════════════════
 * x86-64 Instruction Encoding
 * ═══════════════════════════════════════ */

static uint8_t modrm64(int mod, int reg, int rm) {
    return (uint8_t)((mod << 6) | ((reg & 7) << 3) | (rm & 7));
}

/* REX prefix for 64-bit operations */
static void emit_rex(Module* mod, bool w, int reg, int index, int rm) {
    uint8_t rex = 0x40;
    if (w) rex |= 0x08;           /* REX.W - 64-bit operand */
    if (reg >= 8) rex |= 0x04;    /* REX.R - extend reg */
    if (index >= 8) rex |= 0x02;  /* REX.X - extend index */
    if (rm >= 8) rex |= 0x01;     /* REX.B - extend rm */
    if (rex != 0x40) {
        emit_byte(mod, rex);
    }
}

static void emit_rex_w(Module* mod, int reg, int rm) {
    uint8_t rex = REX_W;
    if (reg >= 8) rex |= 0x04;
    if (rm >= 8) rex |= 0x01;
    emit_byte(mod, rex);
}

/* Push 64-bit register */
static void emit64_push_reg(Module* mod, int reg) {
    if (reg >= 8) {
        emit_byte(mod, 0x41);  /* REX.B */
    }
    emit_byte(mod, 0x50 + (reg & 7));
}

/* Pop 64-bit register */
static void emit64_pop_reg(Module* mod, int reg) {
    if (reg >= 8) {
        emit_byte(mod, 0x41);
    }
    emit_byte(mod, 0x58 + (reg & 7));
}

/* MOV r64, r64 */
static void emit64_mov_reg_reg(Module* mod, int dst, int src) {
    emit_rex_w(mod, src, dst);
    emit_byte(mod, 0x89);
    emit_byte(mod, modrm64(3, src, dst));
}

/* MOV r64, imm64 */
static void emit64_mov_reg_imm64(Module* mod, int reg, uint64_t imm) {
    emit_rex_w(mod, 0, reg);
    emit_byte(mod, 0xB8 + (reg & 7));
    /* Emit 64-bit immediate */
    emit_dword(mod, (uint32_t)(imm & 0xFFFFFFFF));
    emit_dword(mod, (uint32_t)(imm >> 32));
}

/* MOV r64, imm32 (sign-extended) */
static void emit64_mov_reg_imm32(Module* mod, int reg, uint32_t imm) {
    emit_rex_w(mod, 0, reg);
    emit_byte(mod, 0xC7);
    emit_byte(mod, modrm64(3, 0, reg));
    emit_dword(mod, imm);
}

/* MOV r64, [base + disp] */
static void emit64_mov_reg_mem(Module* mod, int reg, int base, int32_t disp) {
    emit_rex_w(mod, reg, base);
    emit_byte(mod, 0x8B);
    if (disp == 0 && (base & 7) != RBP) {
        emit_byte(mod, modrm64(0, reg, base));
        if ((base & 7) == RSP) {
            emit_byte(mod, 0x24);  /* SIB byte for RSP */
        }
    } else if (disp >= -128 && disp <= 127) {
        emit_byte(mod, modrm64(1, reg, base));
        if ((base & 7) == RSP) {
            emit_byte(mod, 0x24);
        }
        emit_byte(mod, (uint8_t)disp);
    } else {
        emit_byte(mod, modrm64(2, reg, base));
        if ((base & 7) == RSP) {
            emit_byte(mod, 0x24);
        }
        emit_dword(mod, (uint32_t)disp);
    }
}

/* MOV [base + disp], r64 */
static void emit64_mov_mem_reg(Module* mod, int base, int32_t disp, int src) {
    emit_rex_w(mod, src, base);
    emit_byte(mod, 0x89);
    if (disp == 0 && (base & 7) != RBP) {
        emit_byte(mod, modrm64(0, src, base));
        if ((base & 7) == RSP) {
            emit_byte(mod, 0x24);
        }
    } else if (disp >= -128 && disp <= 127) {
        emit_byte(mod, modrm64(1, src, base));
        if ((base & 7) == RSP) {
            emit_byte(mod, 0x24);
        }
        emit_byte(mod, (uint8_t)disp);
    } else {
        emit_byte(mod, modrm64(2, src, base));
        if ((base & 7) == RSP) {
            emit_byte(mod, 0x24);
        }
        emit_dword(mod, (uint32_t)disp);
    }
}

/* ADD r64, imm32 */
static void emit64_add_reg_imm(Module* mod, int reg, int32_t imm) {
    emit_rex_w(mod, 0, reg);
    if (imm >= -128 && imm <= 127) {
        emit_byte(mod, 0x83);
        emit_byte(mod, modrm64(3, 0, reg));
        emit_byte(mod, (uint8_t)imm);
    } else {
        emit_byte(mod, 0x81);
        emit_byte(mod, modrm64(3, 0, reg));
        emit_dword(mod, (uint32_t)imm);
    }
}

/* SUB r64, imm32 */
static void emit64_sub_reg_imm(Module* mod, int reg, int32_t imm) {
    emit_rex_w(mod, 0, reg);
    if (imm >= -128 && imm <= 127) {
        emit_byte(mod, 0x83);
        emit_byte(mod, modrm64(3, 5, reg));
        emit_byte(mod, (uint8_t)imm);
    } else {
        emit_byte(mod, 0x81);
        emit_byte(mod, modrm64(3, 5, reg));
        emit_dword(mod, (uint32_t)imm);
    }
}

/* ADD r64, r64 */
static void emit64_add_reg_reg(Module* mod, int dst, int src) {
    emit_rex_w(mod, src, dst);
    emit_byte(mod, 0x01);
    emit_byte(mod, modrm64(3, src, dst));
}

/* SUB r64, r64 */
static void emit64_sub_reg_reg(Module* mod, int dst, int src) {
    emit_rex_w(mod, src, dst);
    emit_byte(mod, 0x29);
    emit_byte(mod, modrm64(3, src, dst));
}

/* IMUL r64, r64 */
static void emit64_imul_reg_reg(Module* mod, int dst, int src) {
    emit_rex_w(mod, dst, src);
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0xAF);
    emit_byte(mod, modrm64(3, dst, src));
}

static uint32_t gen64_pointer_element_size(const Type* type) {
    if (type && (type->kind == TYPE_PTR || type->kind == TYPE_ARRAY) &&
        type->base && type->base->size > 0) {
        return (uint32_t)type->base->size;
    }
    return 0u;
}

static uint32_t gen64_increment_size(const Type* type) {
    uint32_t size = gen64_pointer_element_size(type);
    return size == 0u ? 1u : size;
}

static void emit64_scale_reg(Module* mod, int reg, uint32_t scale) {
    if (scale <= 1u) return;
    emit64_mov_reg_imm32(mod, RDX, scale);
    emit64_imul_reg_reg(mod, reg, RDX);
}

/* IDIV r64 */
static void emit64_idiv_reg(Module* mod, int reg) {
    emit_rex_w(mod, 0, reg);
    emit_byte(mod, 0xF7);
    emit_byte(mod, modrm64(3, 7, reg));
}

/* CQO - sign extend RAX to RDX:RAX */
static void emit64_cqo(Module* mod) {
    emit_byte(mod, REX_W);
    emit_byte(mod, 0x99);
}

/* NEG r64 */
static void emit64_neg_reg(Module* mod, int reg) {
    emit_rex_w(mod, 0, reg);
    emit_byte(mod, 0xF7);
    emit_byte(mod, modrm64(3, 3, reg));
}

/* NOT r64 */
static void emit64_not_reg(Module* mod, int reg) {
    emit_rex_w(mod, 0, reg);
    emit_byte(mod, 0xF7);
    emit_byte(mod, modrm64(3, 2, reg));
}

/* AND r64, r64 */
static void emit64_and_reg_reg(Module* mod, int dst, int src) {
    emit_rex_w(mod, src, dst);
    emit_byte(mod, 0x21);
    emit_byte(mod, modrm64(3, src, dst));
}

/* OR r64, r64 */
static void emit64_or_reg_reg(Module* mod, int dst, int src) {
    emit_rex_w(mod, src, dst);
    emit_byte(mod, 0x09);
    emit_byte(mod, modrm64(3, src, dst));
}

/* XOR r64, r64 */
static void emit64_xor_reg_reg(Module* mod, int dst, int src) {
    emit_rex_w(mod, src, dst);
    emit_byte(mod, 0x31);
    emit_byte(mod, modrm64(3, src, dst));
}

/* SHL r64, CL */
static void emit64_shl_reg_cl(Module* mod, int reg) {
    emit_rex_w(mod, 0, reg);
    emit_byte(mod, 0xD3);
    emit_byte(mod, modrm64(3, 4, reg));
}

/* SHR r64, CL */
static void emit64_shr_reg_cl(Module* mod, int reg) {
    emit_rex_w(mod, 0, reg);
    emit_byte(mod, 0xD3);
    emit_byte(mod, modrm64(3, 5, reg));
}

/* SAR r64, CL */
static void emit64_sar_reg_cl(Module* mod, int reg) {
    emit_rex_w(mod, 0, reg);
    emit_byte(mod, 0xD3);
    emit_byte(mod, modrm64(3, 7, reg));
}

/* CMP r64, r64 */
static void emit64_cmp_reg_reg(Module* mod, int r1, int r2) {
    emit_rex_w(mod, r2, r1);
    emit_byte(mod, 0x39);
    emit_byte(mod, modrm64(3, r2, r1));
}

/* CMP r64, imm32 */
static void emit64_cmp_reg_imm(Module* mod, int reg, int32_t imm) {
    emit_rex_w(mod, 0, reg);
    if (imm >= -128 && imm <= 127) {
        emit_byte(mod, 0x83);
        emit_byte(mod, modrm64(3, 7, reg));
        emit_byte(mod, (uint8_t)imm);
    } else {
        emit_byte(mod, 0x81);
        emit_byte(mod, modrm64(3, 7, reg));
        emit_dword(mod, (uint32_t)imm);
    }
}

/* TEST r64, r64 */
static void emit64_test_reg_reg(Module* mod, int r1, int r2) {
    emit_rex_w(mod, r2, r1);
    emit_byte(mod, 0x85);
    emit_byte(mod, modrm64(3, r2, r1));
}

/* Condition codes */
#define CC64_O   0
#define CC64_NO  1
#define CC64_B   2
#define CC64_AE  3
#define CC64_E   4
#define CC64_NE  5
#define CC64_BE  6
#define CC64_A   7
#define CC64_S   8
#define CC64_NS  9
#define CC64_P   10
#define CC64_NP  11
#define CC64_L   12
#define CC64_GE  13
#define CC64_LE  14
#define CC64_G   15

/* SETCC r8 */
static void emit64_setcc(Module* mod, int cc, int reg) {
    if (reg >= 8) {
        emit_byte(mod, 0x41);
    }
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0x90 + cc);
    emit_byte(mod, modrm64(3, 0, reg));
}

/* MOVZX r64, r8 */
static void emit64_movzx_r64_r8(Module* mod, int dst, int src) {
    emit_rex_w(mod, dst, src);
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0xB6);
    emit_byte(mod, modrm64(3, dst, src));
}

/* LEA r64, [base + disp] */
static void emit64_lea(Module* mod, int dst, int base, int32_t disp) {
    emit_rex_w(mod, dst, base);
    emit_byte(mod, 0x8D);
    if (disp == 0 && (base & 7) != RBP) {
        emit_byte(mod, modrm64(0, dst, base));
        if ((base & 7) == RSP) {
            emit_byte(mod, 0x24);
        }
    } else if (disp >= -128 && disp <= 127) {
        emit_byte(mod, modrm64(1, dst, base));
        if ((base & 7) == RSP) {
            emit_byte(mod, 0x24);
        }
        emit_byte(mod, (uint8_t)disp);
    } else {
        emit_byte(mod, modrm64(2, dst, base));
        if ((base & 7) == RSP) {
            emit_byte(mod, 0x24);
        }
        emit_dword(mod, (uint32_t)disp);
    }
}

/* RET */
static void emit64_ret(Module* mod) {
    emit_byte(mod, 0xC3);
}

/* LEAVE */
static void emit64_leave(Module* mod) {
    emit_byte(mod, 0xC9);
}

/* ═══════════════════════════════════════
 * Label Management (64-bit)
 * ═══════════════════════════════════════ */

static int new_label64(void) {
    return label_counter64++;
}

typedef struct LabelRef64 {
    int label;
    uint32_t offset;
    struct LabelRef64* next;
} LabelRef64;

typedef struct LabelDef64 {
    int label;
    uint32_t offset;
    struct LabelDef64* next;
} LabelDef64;

typedef struct FuncCallRef64 {
    const char* function_name;
    uint32_t call_offset;
    struct FuncCallRef64* next;
} FuncCallRef64;

typedef struct FuncDef64 {
    const char* name;
    uint32_t offset;
    struct FuncDef64* next;
} FuncDef64;

static LabelRef64* label_refs64 = NULL;
static LabelDef64* label_defs64 = NULL;
static FuncCallRef64* func_call_refs64 = NULL;
static FuncDef64* func_defs64 = NULL;

static void add_func_def64(const char* name, uint32_t offset) {
    FuncDef64* definition = rcc_alloc(sizeof(*definition));
    definition->name = name;
    definition->offset = offset;
    definition->next = func_defs64;
    func_defs64 = definition;
}

static void emit64_memory_operand(Module* mod, int reg, int base, int32_t disp) {
    if (disp == 0 && (base & 7) != RBP) {
        emit_byte(mod, modrm64(0, reg, base));
        if ((base & 7) == RSP) emit_byte(mod, 0x24);
    } else if (disp >= -128 && disp <= 127) {
        emit_byte(mod, modrm64(1, reg, base));
        if ((base & 7) == RSP) emit_byte(mod, 0x24);
        emit_byte(mod, (uint8_t)disp);
    } else {
        emit_byte(mod, modrm64(2, reg, base));
        if ((base & 7) == RSP) emit_byte(mod, 0x24);
        emit_dword(mod, (uint32_t)disp);
    }
}

static int gen64_type_width(const Type* type) {
    if (!type || type->size <= 0) return 8;
    if (type->size == 1 || type->size == 2 || type->size == 4 ||
        type->size == 8) return type->size;
    return 8;
}

static void emit64_load_typed(Module* mod, int reg, int base, int32_t disp,
                              const Type* type) {
    int width = gen64_type_width(type);
    if (width == 8) {
        emit64_mov_reg_mem(mod, reg, base, disp);
        return;
    }
    if (width == 4 && type && !type->is_unsigned && type->kind != TYPE_PTR) {
        emit_rex_w(mod, reg, base);
        emit_byte(mod, 0x63); /* MOVSXD r64, r/m32 */
        emit64_memory_operand(mod, reg, base, disp);
        return;
    }
    {
        uint8_t rex = 0x40;
        if (width < 4) rex |= 0x08; /* r64 destination */
        if (reg >= 8) rex |= 0x04;
        if (base >= 8) rex |= 0x01;
        if (rex != 0x40) emit_byte(mod, rex);
    }
    if (width == 4) {
        emit_byte(mod, 0x8B); /* MOV r32, r/m32 zero-extends */
    } else {
        emit_byte(mod, 0x0F);
        emit_byte(mod, type && !type->is_unsigned
            ? (width == 1 ? 0xBE : 0xBF)
            : (width == 1 ? 0xB6 : 0xB7));
    }
    emit64_memory_operand(mod, reg, base, disp);
}

static void emit64_store_typed(Module* mod, int base, int32_t disp, int src,
                               const Type* type) {
    int width = gen64_type_width(type);
    if (width == 8) {
        emit64_mov_mem_reg(mod, base, disp, src);
        return;
    }
    if (width == 2) emit_byte(mod, 0x66);
    {
        uint8_t rex = 0x40;
        if (src >= 8) rex |= 0x04;
        if (base >= 8) rex |= 0x01;
        if (width == 1 && ((src & 7) >= 4)) rex |= 0x40;
        if (rex != 0x40 || width == 1) emit_byte(mod, rex);
    }
    emit_byte(mod, width == 1 ? 0x88 : 0x89);
    emit64_memory_operand(mod, src, base, disp);
}

static void gen64_expr(Module* mod, Expr* expr);

static Expr* gen64_character_array_string(Type* type, Expr* initializer) {
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

static TypeField* gen64_initializer_field(Type* type, const char* name) {
    if (!type || !name) return NULL;
    for (TypeField* field = type->fields; field; field = field->next) {
        if (strcmp(field->name, name) == 0) return field;
    }
    return NULL;
}

static void gen64_zero_local_storage(Module* mod, int32_t displacement,
                                     size_t storage) {
    size_t offset = 0u;
    emit64_mov_reg_imm32(mod, RAX, 0u);
    while (offset + 4u <= storage) {
        emit64_store_typed(mod, RBP, displacement + (int32_t)offset,
                           RAX, type_int);
        offset += 4u;
    }
    while (offset < storage) {
        emit64_store_typed(mod, RBP, displacement + (int32_t)offset,
                           RAX, type_char);
        ++offset;
    }
}

static bool gen64_local_initializer(Module* mod, Type* type,
                                    Expr* initializer,
                                    int32_t displacement) {
    Expr* string = gen64_character_array_string(type, initializer);
    if (!type || !initializer) return false;
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
            emit64_mov_reg_imm32(mod, RAX, packed);
            emit64_store_typed(mod, RBP, displacement + (int32_t)offset,
                               RAX, type_int);
            offset += 4u;
        }
        while (offset < storage) {
            uint8_t byte = offset < text_size
                ? (uint8_t)string->str_val[offset] : 0u;
            emit64_mov_reg_imm32(mod, RAX, byte);
            emit64_store_typed(mod, RBP, displacement + (int32_t)offset,
                               RAX, type_char);
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
                    !gen64_local_initializer(mod, type->base, item->expr,
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
                    field = gen64_initializer_field(
                        type, item->designator_field);
                }
                if (!field || (type->kind == TYPE_UNION && initialized != 0 &&
                               item->designator_kind ==
                                   INIT_DESIGNATOR_NONE)) {
                    return false;
                }
                field_offset = (int64_t)displacement + field->offset;
                if (field_offset < INT32_MIN || field_offset > INT32_MAX ||
                    !gen64_local_initializer(mod, field->type, item->expr,
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
        return gen64_local_initializer(
            mod, type, initializer->compound_init->expr, displacement);
    }
    if (!type_is_integer(type) && type->kind != TYPE_ENUM &&
        type->kind != TYPE_PTR) {
        return false;
    }
    gen64_expr(mod, initializer);
    emit64_store_typed(mod, RBP, displacement, RAX, type);
    return true;
}

static void add_func_call_ref64(const char* name, uint32_t call_offset) {
    FuncCallRef64* reference = rcc_alloc(sizeof(*reference));
    reference->function_name = name;
    reference->call_offset = call_offset;
    reference->next = func_call_refs64;
    func_call_refs64 = reference;
}

static void resolve_func_calls64(Module* mod) {
    FuncCallRef64* reference;
    for (reference = func_call_refs64; reference; reference = reference->next) {
        FuncDef64* definition;
        for (definition = func_defs64; definition; definition = definition->next) {
            if (strcmp(definition->name, reference->function_name) == 0) {
                int32_t relative = (int32_t)definition->offset -
                                   (int32_t)(reference->call_offset + 4u);
                mod->code.data[reference->call_offset] = (uint8_t)relative;
                mod->code.data[reference->call_offset + 1u] =
                    (uint8_t)(relative >> 8);
                mod->code.data[reference->call_offset + 2u] =
                    (uint8_t)(relative >> 16);
                mod->code.data[reference->call_offset + 3u] =
                    (uint8_t)(relative >> 24);
                break;
            }
        }
    }
}

static void emit64_label(Module* mod, int label) {
    LabelDef64* def = rcc_alloc(sizeof(LabelDef64));
    def->label = label;
    def->offset = code_offset(mod);
    def->next = label_defs64;
    label_defs64 = def;
}

static void emit64_jmp_label(Module* mod, int label) {
    emit_byte(mod, 0xE9);
    LabelRef64* ref = rcc_alloc(sizeof(LabelRef64));
    ref->label = label;
    ref->offset = code_offset(mod);
    ref->next = label_refs64;
    label_refs64 = ref;
    emit_dword(mod, 0);
}

static void emit64_jcc_label(Module* mod, int cc, int label) {
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0x80 + cc);
    LabelRef64* ref = rcc_alloc(sizeof(LabelRef64));
    ref->label = label;
    ref->offset = code_offset(mod);
    ref->next = label_refs64;
    label_refs64 = ref;
    emit_dword(mod, 0);
}

static void resolve_labels64(Module* mod) {
    for (LabelRef64* ref = label_refs64; ref; ref = ref->next) {
        for (LabelDef64* def = label_defs64; def; def = def->next) {
            if (def->label == ref->label) {
                int32_t rel = def->offset - (ref->offset + 4);
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
 * Expression Code Generation (64-bit)
 * ═══════════════════════════════════════ */

static void gen64_stmt(Module* mod, Stmt* stmt);

static void gen64_symbol_address(Module* mod, const char* symbol,
                                 uint32_t addend) {
    emit64_mov_reg_imm64(mod, RAX, 0u);
    module_add_relocation(mod, MODULE_SYMBOL_CODE,
                          code_offset(mod) - 8u, addend,
                          false, true, symbol);
    add_reloc(mod, MODULE_SYMBOL_CODE, code_offset(mod) - 8u,
              RIN_RELOC_ABS64);
}

static void gen64_tls_address(Module* mod, const char* symbol) {
    /* Variant II x86_64 TLS: FS:0 contains the thread pointer. */
    static const uint8_t load_thread_pointer[] = {
        0x64, 0x48, 0x8b, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00
    };
    emit_bytes(mod, load_thread_pointer, sizeof(load_thread_pointer));
    emit_byte(mod, 0x48);
    emit_byte(mod, 0x81);
    emit_byte(mod, 0xc0);  /* ADD RAX, imm32 (sign extended) */
    {
        uint32_t offset = code_offset(mod);
        emit_dword(mod, 0u);
        module_add_tls_relocation(mod, MODULE_SYMBOL_CODE, offset, symbol);
        add_reloc(mod, MODULE_SYMBOL_CODE, offset, RIN_RELOC_TLSOFF32S);
    }
}

/* Generate lvalue address in RAX */
static void gen64_lvalue(Module* mod, Expr* expr) {
    switch (expr->kind) {
        case EXPR_IDENT: {
            Decl* decl = expr->ident_decl;
            if (!decl) {
                emit64_mov_reg_imm32(mod, RAX, 0);
                break;
            }
            if (decl->kind == DECL_VAR && decl->var_is_thread_local) {
                gen64_tls_address(mod, decl->name);
            } else if (decl->kind == DECL_FUNC || decl->var_is_global) {
                gen64_symbol_address(mod, decl->name, 0u);
            } else {
                emit64_lea(mod, RAX, RBP, decl->var_offset);
            }
            break;
        }

        case EXPR_DEREF:
            gen64_expr(mod, expr->unary_operand);
            break;

        case EXPR_INDEX:
            gen64_expr(mod, expr->index_base);
            emit64_push_reg(mod, RAX);
            gen64_expr(mod, expr->index_expr);
            if (expr->type && expr->type->size > 1) {
                emit64_mov_reg_imm32(mod, RCX, expr->type->size);
                emit64_imul_reg_reg(mod, RAX, RCX);
            }
            emit64_pop_reg(mod, RCX);
            emit64_add_reg_reg(mod, RAX, RCX);
            break;

        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            if (expr->kind == EXPR_PTR_MEMBER) {
                gen64_expr(mod, expr->member_base);
            } else {
                gen64_lvalue(mod, expr->member_base);
            }
            if (expr->member_field && expr->member_field->offset > 0) {
                emit64_add_reg_imm(mod, RAX, expr->member_field->offset);
            }
            break;

        default:
            rcc_error(expr->loc, "not an lvalue");
            break;
    }
}

static void gen64_expr(Module* mod, Expr* expr) {
    if (!expr) return;

    switch (expr->kind) {
        case EXPR_INT_LIT:
            emit64_mov_reg_imm32(mod, RAX, (uint32_t)expr->int_val);
            break;

        case EXPR_CHAR_LIT:
            emit64_mov_reg_imm32(mod, RAX, (uint32_t)(uint8_t)expr->char_val);
            break;

        case EXPR_STRING_LIT: {
            uint32_t offset = emit_string(mod, expr->str_val);
            module_ensure_rodata_base_symbol(mod);
            gen64_symbol_address(mod, "__rcc_rodata_base", offset);
            break;
        }

        case EXPR_IDENT: {
            Decl* decl = expr->ident_decl;
            if (!decl) {
                emit64_mov_reg_imm32(mod, RAX, 0);
                break;
            }
            if (decl->kind == DECL_FUNC) {
                gen64_symbol_address(mod, decl->name, 0u);
            } else if (decl->var_is_thread_local) {
                gen64_lvalue(mod, expr);
                emit64_load_typed(mod, RAX, RAX, 0, decl->type);
            } else if (decl->type && decl->type->kind == TYPE_ARRAY) {
                gen64_lvalue(mod, expr);
            } else if (decl->var_is_global) {
                gen64_symbol_address(mod, decl->name, 0u);
                emit64_load_typed(mod, RAX, RAX, 0, decl->type);
            } else {
                emit64_load_typed(mod, RAX, RBP, decl->var_offset,
                                  decl->type);
            }
            break;
        }

        case EXPR_NEG:
            gen64_expr(mod, expr->unary_operand);
            emit64_neg_reg(mod, RAX);
            break;

        case EXPR_BITNOT:
            gen64_expr(mod, expr->unary_operand);
            emit64_not_reg(mod, RAX);
            break;

        case EXPR_NOT:
            gen64_expr(mod, expr->unary_operand);
            emit64_cmp_reg_imm(mod, RAX, 0);
            emit64_setcc(mod, CC64_E, RAX);
            emit64_movzx_r64_r8(mod, RAX, RAX);
            break;

        case EXPR_ADDR:
            gen64_lvalue(mod, expr->unary_operand);
            break;

        case EXPR_DEREF:
            gen64_expr(mod, expr->unary_operand);
            emit64_load_typed(mod, RAX, RAX, 0, expr->type);
            break;

        case EXPR_PREINC:
        case EXPR_PREDEC:
            gen64_lvalue(mod, expr->unary_operand);
            emit64_push_reg(mod, RAX);
            emit64_load_typed(mod, RAX, RAX, 0, expr->type);
            if (expr->kind == EXPR_PREINC) {
                emit64_add_reg_imm(
                    mod, RAX, (int32_t)gen64_increment_size(expr->type));
            } else {
                emit64_sub_reg_imm(
                    mod, RAX, (int32_t)gen64_increment_size(expr->type));
            }
            emit64_pop_reg(mod, RCX);
            emit64_store_typed(mod, RCX, 0, RAX, expr->type);
            break;

        case EXPR_POSTINC:
        case EXPR_POSTDEC:
            gen64_lvalue(mod, expr->unary_operand);
            emit64_push_reg(mod, RAX);
            emit64_load_typed(mod, RAX, RAX, 0, expr->type);
            emit64_mov_reg_reg(mod, RDX, RAX);
            if (expr->kind == EXPR_POSTINC) {
                emit64_add_reg_imm(
                    mod, RDX, (int32_t)gen64_increment_size(expr->type));
            } else {
                emit64_sub_reg_imm(
                    mod, RDX, (int32_t)gen64_increment_size(expr->type));
            }
            emit64_pop_reg(mod, RCX);
            emit64_store_typed(mod, RCX, 0, RDX, expr->type);
            break;

        case EXPR_ADD:
            gen64_expr(mod, expr->binary_lhs);
            emit64_push_reg(mod, RAX);
            gen64_expr(mod, expr->binary_rhs);
            emit64_mov_reg_reg(mod, RCX, RAX);
            emit64_pop_reg(mod, RAX);
            if (gen64_pointer_element_size(expr->binary_lhs->type) != 0u &&
                type_is_integer(expr->binary_rhs->type)) {
                emit64_scale_reg(
                    mod, RCX,
                    gen64_pointer_element_size(expr->binary_lhs->type));
            } else if (type_is_integer(expr->binary_lhs->type) &&
                       gen64_pointer_element_size(
                           expr->binary_rhs->type) != 0u) {
                emit64_scale_reg(
                    mod, RAX,
                    gen64_pointer_element_size(expr->binary_rhs->type));
            }
            emit64_add_reg_reg(mod, RAX, RCX);
            break;

        case EXPR_SUB:
            gen64_expr(mod, expr->binary_lhs);
            emit64_push_reg(mod, RAX);
            gen64_expr(mod, expr->binary_rhs);
            emit64_mov_reg_reg(mod, RCX, RAX);
            emit64_pop_reg(mod, RAX);
            if (gen64_pointer_element_size(expr->binary_lhs->type) != 0u &&
                type_is_integer(expr->binary_rhs->type)) {
                emit64_scale_reg(
                    mod, RCX,
                    gen64_pointer_element_size(expr->binary_lhs->type));
            }
            emit64_sub_reg_reg(mod, RAX, RCX);
            if (gen64_pointer_element_size(expr->binary_lhs->type) != 0u &&
                gen64_pointer_element_size(expr->binary_rhs->type) != 0u) {
                uint32_t element_size = gen64_pointer_element_size(
                    expr->binary_lhs->type);
                if (element_size > 1u) {
                    emit64_mov_reg_imm32(mod, RCX, element_size);
                    emit64_cqo(mod);
                    emit64_idiv_reg(mod, RCX);
                }
            }
            break;

        case EXPR_MUL:
            gen64_expr(mod, expr->binary_lhs);
            emit64_push_reg(mod, RAX);
            gen64_expr(mod, expr->binary_rhs);
            emit64_mov_reg_reg(mod, RCX, RAX);
            emit64_pop_reg(mod, RAX);
            emit64_imul_reg_reg(mod, RAX, RCX);
            break;

        case EXPR_DIV:
        case EXPR_MOD:
            gen64_expr(mod, expr->binary_lhs);
            emit64_push_reg(mod, RAX);
            gen64_expr(mod, expr->binary_rhs);
            emit64_mov_reg_reg(mod, RCX, RAX);
            emit64_pop_reg(mod, RAX);
            emit64_cqo(mod);
            emit64_idiv_reg(mod, RCX);
            if (expr->kind == EXPR_MOD) {
                emit64_mov_reg_reg(mod, RAX, RDX);
            }
            break;

        case EXPR_BITAND:
            gen64_expr(mod, expr->binary_lhs);
            emit64_push_reg(mod, RAX);
            gen64_expr(mod, expr->binary_rhs);
            emit64_mov_reg_reg(mod, RCX, RAX);
            emit64_pop_reg(mod, RAX);
            emit64_and_reg_reg(mod, RAX, RCX);
            break;

        case EXPR_BITOR:
            gen64_expr(mod, expr->binary_lhs);
            emit64_push_reg(mod, RAX);
            gen64_expr(mod, expr->binary_rhs);
            emit64_mov_reg_reg(mod, RCX, RAX);
            emit64_pop_reg(mod, RAX);
            emit64_or_reg_reg(mod, RAX, RCX);
            break;

        case EXPR_BITXOR:
            gen64_expr(mod, expr->binary_lhs);
            emit64_push_reg(mod, RAX);
            gen64_expr(mod, expr->binary_rhs);
            emit64_mov_reg_reg(mod, RCX, RAX);
            emit64_pop_reg(mod, RAX);
            emit64_xor_reg_reg(mod, RAX, RCX);
            break;

        case EXPR_LSHIFT:
            gen64_expr(mod, expr->binary_lhs);
            emit64_push_reg(mod, RAX);
            gen64_expr(mod, expr->binary_rhs);
            emit64_mov_reg_reg(mod, RCX, RAX);
            emit64_pop_reg(mod, RAX);
            emit64_shl_reg_cl(mod, RAX);
            break;

        case EXPR_RSHIFT:
            gen64_expr(mod, expr->binary_lhs);
            emit64_push_reg(mod, RAX);
            gen64_expr(mod, expr->binary_rhs);
            emit64_mov_reg_reg(mod, RCX, RAX);
            emit64_pop_reg(mod, RAX);
            if (expr->type && expr->type->is_unsigned) {
                emit64_shr_reg_cl(mod, RAX);
            } else {
                emit64_sar_reg_cl(mod, RAX);
            }
            break;

        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_GT:
        case EXPR_LE:
        case EXPR_GE: {
            gen64_expr(mod, expr->binary_lhs);
            emit64_push_reg(mod, RAX);
            gen64_expr(mod, expr->binary_rhs);
            emit64_mov_reg_reg(mod, RCX, RAX);
            emit64_pop_reg(mod, RAX);
            emit64_cmp_reg_reg(mod, RAX, RCX);

            int cc;
            switch (expr->kind) {
                case EXPR_EQ: cc = CC64_E; break;
                case EXPR_NE: cc = CC64_NE; break;
                case EXPR_LT: cc = CC64_L; break;
                case EXPR_GT: cc = CC64_G; break;
                case EXPR_LE: cc = CC64_LE; break;
                case EXPR_GE: cc = CC64_GE; break;
                default: cc = CC64_E; break;
            }

            emit64_setcc(mod, cc, RAX);
            emit64_movzx_r64_r8(mod, RAX, RAX);
            break;
        }

        case EXPR_AND: {
            int end_label = new_label64();
            gen64_expr(mod, expr->binary_lhs);
            emit64_test_reg_reg(mod, RAX, RAX);
            emit64_jcc_label(mod, CC64_E, end_label);
            gen64_expr(mod, expr->binary_rhs);
            emit64_test_reg_reg(mod, RAX, RAX);
            emit64_setcc(mod, CC64_NE, RAX);
            emit64_movzx_r64_r8(mod, RAX, RAX);
            emit64_label(mod, end_label);
            break;
        }

        case EXPR_OR: {
            int end_label = new_label64();
            gen64_expr(mod, expr->binary_lhs);
            emit64_test_reg_reg(mod, RAX, RAX);
            emit64_jcc_label(mod, CC64_NE, end_label);
            gen64_expr(mod, expr->binary_rhs);
            emit64_label(mod, end_label);
            emit64_test_reg_reg(mod, RAX, RAX);
            emit64_setcc(mod, CC64_NE, RAX);
            emit64_movzx_r64_r8(mod, RAX, RAX);
            break;
        }

        case EXPR_ASSIGN:
            gen64_expr(mod, expr->binary_rhs);
            emit64_push_reg(mod, RAX);
            gen64_lvalue(mod, expr->binary_lhs);
            emit64_pop_reg(mod, RCX);
            emit64_store_typed(mod, RAX, 0, RCX,
                               expr->binary_lhs->type);
            emit64_mov_reg_reg(mod, RAX, RCX);
            break;

        case EXPR_ADD_ASSIGN:
        case EXPR_SUB_ASSIGN: {
            uint32_t scale = gen64_pointer_element_size(
                expr->binary_lhs->type);
            gen64_lvalue(mod, expr->binary_lhs);
            emit64_push_reg(mod, RAX);
            emit64_load_typed(mod, RAX, RAX, 0,
                              expr->binary_lhs->type);
            emit64_push_reg(mod, RAX);
            gen64_expr(mod, expr->binary_rhs);
            emit64_scale_reg(mod, RAX, scale);
            emit64_mov_reg_reg(mod, RDX, RAX);
            emit64_pop_reg(mod, RAX);
            if (expr->kind == EXPR_ADD_ASSIGN) {
                emit64_add_reg_reg(mod, RAX, RDX);
            } else {
                emit64_sub_reg_reg(mod, RAX, RDX);
            }
            emit64_pop_reg(mod, RCX);
            emit64_store_typed(mod, RCX, 0, RAX,
                               expr->binary_lhs->type);
            break;
        }

        case EXPR_COND: {
            int else_label = new_label64();
            int end_label = new_label64();
            gen64_expr(mod, expr->cond_test);
            emit64_test_reg_reg(mod, RAX, RAX);
            emit64_jcc_label(mod, CC64_E, else_label);
            gen64_expr(mod, expr->cond_then);
            emit64_jmp_label(mod, end_label);
            emit64_label(mod, else_label);
            gen64_expr(mod, expr->cond_else);
            emit64_label(mod, end_label);
            break;
        }

        case EXPR_CALL: {
            /* x86-64 System V ABI: RDI, RSI, RDX, RCX, R8, R9 */
            int arg_regs[] = {RDI, RSI, RDX, RCX, R8, R9};
            int argc = exprlist_len(expr->call_args);
            int abi_argc = 0;
            int stack_argc;
            int stack_padding;

            /* Collect arguments */
            ExprList** args = rcc_alloc(argc * sizeof(ExprList*));
            int i = 0;
            for (ExprList* a = expr->call_args; a; a = a->next) {
                args[i++] = a;
                if (a->expr->type &&
                    (a->expr->type->kind == TYPE_STRUCT ||
                     a->expr->type->kind == TYPE_UNION)) {
                    abi_argc += (a->expr->type->size + 7) / 8;
                } else {
                    ++abi_argc;
                }
            }
            stack_argc = abi_argc > 6 ? abi_argc - 6 : 0;
            stack_padding = (stack_argc & 1) ? 8 : 0;

            /* Keep the call boundary 16-byte aligned. Evaluate every scalar
             * argument to the temporary stack first; loading a later
             * argument is then unable to clobber an earlier argument register. */
            if (stack_padding) emit64_sub_reg_imm(mod, RSP, stack_padding);
            for (i = argc - 1; i >= 0; i--) {
                Expr* argument = args[i]->expr;
                if (argument->type &&
                    (argument->type->kind == TYPE_STRUCT ||
                     argument->type->kind == TYPE_UNION)) {
                    int units = (argument->type->size + 7) / 8;
                    for (int unit = units - 1; unit >= 0; --unit) {
                        gen64_lvalue(mod, argument);
                        if (unit) emit64_add_reg_imm(mod, RAX, unit * 8);
                        emit64_mov_reg_mem(mod, RAX, RAX, 0);
                        emit64_push_reg(mod, RAX);
                    }
                } else {
                    gen64_expr(mod, argument);
                    emit64_push_reg(mod, RAX);
                }
            }
            for (i = 0; i < abi_argc && i < 6; ++i) {
                emit64_pop_reg(mod, arg_regs[i]);
            }
            rcc_free(args);

            /* Direct calls use rel32 and produce a .ro relocation only when
             * the definition is external to this translation unit. */
            if (expr->call_func->kind == EXPR_IDENT &&
                expr->call_func->ident_decl &&
                expr->call_func->ident_decl->kind == DECL_FUNC) {
                Decl* function = expr->call_func->ident_decl;
                uint32_t call_offset;
                emit_byte(mod, 0xE8);
                call_offset = code_offset(mod);
                emit_dword(mod, 0u);
                if (function->func_body) {
                    add_func_call_ref64(function->name, call_offset);
                } else {
                    /* call qword ptr [rip+disp32]; the loader fills the
                     * associated 64-bit import slot before execution. */
                    mod->code.size = call_offset - 1u;
                    emit_byte(mod, 0xFF);
                    emit_byte(mod, 0x15);
                    call_offset = code_offset(mod);
                    emit_dword(mod, 0u);
                    module_add_relocation(mod, MODULE_SYMBOL_CODE,
                                          call_offset, 0u, true, false,
                                          function->name);
                }
            } else {
                gen64_expr(mod, expr->call_func);
                emit_byte(mod, 0xFF);  /* CALL RAX */
                emit_byte(mod, modrm64(3, 2, RAX));
            }

            /* Clean up stack arguments */
            if (stack_argc || stack_padding) {
                emit64_add_reg_imm(mod, RSP,
                                   stack_argc * 8 + stack_padding);
            }
            break;
        }

        case EXPR_INDEX:
            gen64_lvalue(mod, expr);
            if (!expr->type || expr->type->kind != TYPE_ARRAY) {
                emit64_load_typed(mod, RAX, RAX, 0, expr->type);
            }
            break;

        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            gen64_lvalue(mod, expr);
            if (!expr->type || expr->type->kind != TYPE_ARRAY) {
                emit64_load_typed(mod, RAX, RAX, 0, expr->type);
            }
            break;

        case EXPR_CAST:
            gen64_expr(mod, expr->cast_expr);
            break;

        case EXPR_SIZEOF:
            if (expr->sizeof_type) {
                emit64_mov_reg_imm32(mod, RAX, expr->sizeof_type->size);
            } else if (expr->unary_operand && expr->unary_operand->type) {
                emit64_mov_reg_imm32(mod, RAX, expr->unary_operand->type->size);
            } else {
                emit64_mov_reg_imm32(mod, RAX, 8);  /* 64-bit default */
            }
            break;

        case EXPR_ALIGNOF:
            emit64_mov_reg_imm32(
                mod, RAX,
                expr->sizeof_type ? expr->sizeof_type->align : 1);
            break;

        case EXPR_COMMA:
            gen64_expr(mod, expr->binary_lhs);
            gen64_expr(mod, expr->binary_rhs);
            break;

        default:
            emit64_mov_reg_imm32(mod, RAX, 0);
            break;
    }
}

/* ═══════════════════════════════════════
 * Statement Code Generation (64-bit)
 * ═══════════════════════════════════════ */

static int break_label64 = -1;
static int continue_label64 = -1;

static void gen64_stmt(Module* mod, Stmt* stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case STMT_EXPR:
            if (stmt->expr) {
                gen64_expr(mod, stmt->expr);
            }
            break;

        case STMT_BLOCK:
            for (StmtList* s = stmt->block_stmts; s; s = s->next) {
                gen64_stmt(mod, s->stmt);
            }
            break;

        case STMT_IF: {
            int else_label = new_label64();
            int end_label = new_label64();

            gen64_expr(mod, stmt->if_cond);
            emit64_test_reg_reg(mod, RAX, RAX);
            emit64_jcc_label(mod, CC64_E, else_label);

            gen64_stmt(mod, stmt->if_then);

            if (stmt->if_else) {
                emit64_jmp_label(mod, end_label);
            }

            emit64_label(mod, else_label);

            if (stmt->if_else) {
                gen64_stmt(mod, stmt->if_else);
                emit64_label(mod, end_label);
            }
            break;
        }

        case STMT_WHILE: {
            int start_label = new_label64();
            int end_label = new_label64();
            int old_break = break_label64;
            int old_continue = continue_label64;
            break_label64 = end_label;
            continue_label64 = start_label;

            emit64_label(mod, start_label);
            gen64_expr(mod, stmt->while_cond);
            emit64_test_reg_reg(mod, RAX, RAX);
            emit64_jcc_label(mod, CC64_E, end_label);

            gen64_stmt(mod, stmt->while_body);

            emit64_jmp_label(mod, start_label);
            emit64_label(mod, end_label);

            break_label64 = old_break;
            continue_label64 = old_continue;
            break;
        }

        case STMT_DO: {
            int start_label = new_label64();
            int end_label = new_label64();
            int cond_label = new_label64();
            int old_break = break_label64;
            int old_continue = continue_label64;
            break_label64 = end_label;
            continue_label64 = cond_label;

            emit64_label(mod, start_label);
            gen64_stmt(mod, stmt->while_body);

            emit64_label(mod, cond_label);
            gen64_expr(mod, stmt->while_cond);
            emit64_test_reg_reg(mod, RAX, RAX);
            emit64_jcc_label(mod, CC64_NE, start_label);

            emit64_label(mod, end_label);

            break_label64 = old_break;
            continue_label64 = old_continue;
            break;
        }

        case STMT_FOR: {
            int start_label = new_label64();
            int end_label = new_label64();
            int inc_label = new_label64();
            int old_break = break_label64;
            int old_continue = continue_label64;
            break_label64 = end_label;
            continue_label64 = inc_label;

            if (stmt->for_init) {
                gen64_stmt(mod, stmt->for_init);
            }

            emit64_label(mod, start_label);

            if (stmt->for_cond) {
                gen64_expr(mod, stmt->for_cond);
                emit64_test_reg_reg(mod, RAX, RAX);
                emit64_jcc_label(mod, CC64_E, end_label);
            }

            gen64_stmt(mod, stmt->for_body);

            emit64_label(mod, inc_label);
            if (stmt->for_inc) {
                gen64_expr(mod, stmt->for_inc);
            }

            emit64_jmp_label(mod, start_label);
            emit64_label(mod, end_label);

            break_label64 = old_break;
            continue_label64 = old_continue;
            break;
        }

        case STMT_RETURN:
            if (stmt->return_val) {
                gen64_expr(mod, stmt->return_val);
            }
            emit64_leave(mod);
            emit64_ret(mod);
            break;

        case STMT_BREAK:
            if (break_label64 >= 0) {
                emit64_jmp_label(mod, break_label64);
            }
            break;

        case STMT_CONTINUE:
            if (continue_label64 >= 0) {
                emit64_jmp_label(mod, continue_label64);
            }
            break;

        case STMT_DECL: {
            Decl* d = stmt->decl;
            if (d->kind == DECL_VAR && d->var_init) {
                if (d->type && (d->type->kind == TYPE_ARRAY ||
                                d->type->kind == TYPE_STRUCT ||
                                d->type->kind == TYPE_UNION)) {
                    gen64_zero_local_storage(mod, d->var_offset,
                                             (size_t)d->type->size);
                }
                if (!gen64_local_initializer(mod, d->type, d->var_init,
                                             d->var_offset)) {
                    rcc_error(d->loc, "unsupported local initializer for '%s'",
                              d->name);
                }
            }
            break;
        }

        case STMT_NULL:
            break;

        default:
            break;
    }
}

/* ═══════════════════════════════════════
 * Function Code Generation (64-bit)
 * ═══════════════════════════════════════ */

static bool gen64_shift_local_offsets(Stmt* statement, int shift) {
    if (!statement || shift == 0) return true;
    switch (statement->kind) {
        case STMT_BLOCK:
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                if (!gen64_shift_local_offsets(item->stmt, shift)) {
                    return false;
                }
            }
            return true;
        case STMT_IF:
            return gen64_shift_local_offsets(statement->if_then, shift) &&
                   gen64_shift_local_offsets(statement->if_else, shift);
        case STMT_WHILE:
        case STMT_DO:
            return gen64_shift_local_offsets(statement->while_body, shift);
        case STMT_FOR:
            return gen64_shift_local_offsets(statement->for_init, shift) &&
                   gen64_shift_local_offsets(statement->for_body, shift);
        case STMT_SWITCH:
            return gen64_shift_local_offsets(statement->switch_body, shift);
        case STMT_CASE:
            return gen64_shift_local_offsets(statement->case_stmt, shift);
        case STMT_DEFAULT:
            return gen64_shift_local_offsets(statement->default_stmt, shift);
        case STMT_LABEL:
            return gen64_shift_local_offsets(statement->label_stmt, shift);
        case STMT_DECL:
            if (statement->decl && statement->decl->kind == DECL_VAR &&
                !statement->decl->var_is_global &&
                statement->decl->var_offset < 0) {
                int64_t shifted = (int64_t)statement->decl->var_offset - shift;
                if (shifted < INT_MIN) {
                    rcc_error(statement->decl->loc,
                              "function stack frame exceeds compiler limits");
                    return false;
                }
                statement->decl->var_offset = (int)shifted;
            }
            return true;
        default:
            return true;
    }
}

static void gen64_function(Module* mod, Decl* decl) {
    static const int argument_registers[] = {RDI, RSI, RDX, RCX, R8, R9};
    int64_t original_parameter_size = 0;
    int64_t parameter_frame_size = 0;
    int register_cursor = 0;
    int stack_cursor = 16; /* saved RBP + return address */
    int stack_size;
    if (!decl->func_body) return;

    for (DeclList* parameter = decl->func_params; parameter;
         parameter = parameter->next) {
        Decl* value = parameter->decl;
        int size = value->type && value->type->size > 0
            ? value->type->size : 8;
        int aggregate = value->type &&
            (value->type->kind == TYPE_STRUCT ||
             value->type->kind == TYPE_UNION ||
             value->type->kind == TYPE_ARRAY);
        int units = aggregate ? (size + 7) / 8 : 1;
        int64_t storage = aggregate ? (int64_t)units * 8
                                    : ((int64_t)size + 3) & ~INT64_C(3);
        original_parameter_size += size;
        original_parameter_size = (original_parameter_size + 3) &
                                  ~INT64_C(3);
        parameter_frame_size += storage;
        if (parameter_frame_size > INT_MAX) {
            rcc_error(decl->loc, "function stack frame exceeds compiler limits");
            return;
        }
        value->var_offset = -(int)parameter_frame_size;
    }
    if (parameter_frame_size > original_parameter_size &&
        !gen64_shift_local_offsets(
            decl->func_body,
            (int)(parameter_frame_size - original_parameter_size))) {
        return;
    }
    stack_size = codegen_required_local_bytes(decl->func_body);
    if (parameter_frame_size > stack_size) {
        if (parameter_frame_size > INT_MAX) {
            rcc_error(decl->loc, "function stack frame exceeds compiler limits");
            return;
        }
        stack_size = (int)parameter_frame_size;
    }
    if (stack_size > INT_MAX - 15) {
        rcc_error(decl->loc, "function stack frame exceeds compiler limits");
        return;
    }
    stack_size = (stack_size + 15) & ~15;

    /* Function prologue */
    emit64_push_reg(mod, RBP);
    emit64_mov_reg_reg(mod, RBP, RSP);
    if (stack_size > 0) {
        emit64_sub_reg_imm(mod, RSP, stack_size);
    }

    /* Sema reserves parameter storage as part of the function frame. Rebuild
     * those offsets and spill the SysV register arguments before the body so
     * ordinary identifier/member code remains independent of caller-saved
     * registers. Integer-only aggregates up to 16 bytes use one or two
     * eightbyte classes, which covers the stable SDK's slice/string values. */
    for (DeclList* parameter = decl->func_params; parameter;
         parameter = parameter->next) {
        Decl* value = parameter->decl;
        int size = value->type && value->type->size > 0
            ? value->type->size : 8;
        int aggregate = value->type &&
            (value->type->kind == TYPE_STRUCT ||
             value->type->kind == TYPE_UNION ||
             value->type->kind == TYPE_ARRAY);
        int units = aggregate ? (size + 7) / 8 : 1;
        if (units <= 2 && units <= 6 - register_cursor) {
            if (aggregate) {
                for (int unit = 0; unit < units; ++unit) {
                    emit64_mov_mem_reg(mod, RBP,
                        value->var_offset + unit * 8,
                        argument_registers[register_cursor++]);
                }
            } else {
                emit64_store_typed(mod, RBP, value->var_offset,
                                   argument_registers[register_cursor++],
                                   value->type);
            }
        } else {
            if (aggregate) {
                for (int unit = 0; unit < units; ++unit) {
                    emit64_mov_reg_mem(mod, RAX, RBP,
                                       stack_cursor + unit * 8);
                    emit64_mov_mem_reg(mod, RBP,
                                       value->var_offset + unit * 8, RAX);
                }
            } else {
                emit64_load_typed(mod, RAX, RBP, stack_cursor, value->type);
                emit64_store_typed(mod, RBP, value->var_offset, RAX,
                                   value->type);
            }
            stack_cursor += (size + 7) & ~7;
        }
    }

    /* Generate body */
    gen64_stmt(mod, decl->func_body);

    /* Function epilogue */
    emit64_mov_reg_imm32(mod, RAX, 0);
    emit64_leave(mod);
    emit64_ret(mod);
}

/* ═══════════════════════════════════════
 * Main Entry for 64-bit Code Generation
 * ═══════════════════════════════════════ */

Module* rcc_codegen64(AST* ast) {
    Module* mod = codegen_new();

    /* Reset label counter */
    label_counter64 = 0;
    label_refs64 = NULL;
    label_defs64 = NULL;
    func_call_refs64 = NULL;
    func_defs64 = NULL;

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

            add_func_def64(d->decl->name, func_start);

            if (strcmp(d->decl->name, "main") == 0) {
                mod->entry_point = func_start;
            }
            gen64_function(mod, d->decl);

            /* Add symbol for function */
            module_add_symbol(mod, d->decl->name, func_start, true,
                              MODULE_SYMBOL_CODE,
                             d->decl->storage != STORAGE_STATIC);
        }
    }

    resolve_func_calls64(mod);

    /* Resolve label references */
    resolve_labels64(mod);

    return mod;
}

#endif /* 64-bit code */
