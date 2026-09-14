/*
 * RCC - RinOS C Compiler
 * x86-64 Code Generator
 */

#include "rcc.h"
#include "ast.h"
#include "ast_cxx.h"
#include "symtab.h"
#include "codegen.h"
#include <limits.h>

/* Only compile if generating 64-bit code */
#if 1

/* Label management */
static int label_counter64 = 0;

static void codegen64_expr_loc(SourceLoc* location, const Expr* expression) {
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

static void emit64_div_reg(Module* mod, int reg) {
    emit_rex_w(mod, 0, reg);
    emit_byte(mod, 0xF7);
    emit_byte(mod, modrm64(3, 6, reg));
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

static void emit64_memory_operand_width(Module* mod, int reg, int base,
                                        int32_t disp, int width) {
    emit_rex(mod, width == 8, reg, 0, base);
    emit_byte(mod, 0x8B);
    emit64_memory_operand(mod, reg, base, disp);
}

static void emit64_store_memory_operand_width(Module* mod, int reg, int base,
                                              int32_t disp, int width) {
    emit_rex(mod, width == 8, reg, 0, base);
    emit_byte(mod, 0x89);
    emit64_memory_operand(mod, reg, base, disp);
}

static void emit64_mov_xmm_from_gpr(Module* mod, int xmm, int gpr,
                                    int width) {
    emit_byte(mod, 0x66);
    emit_rex(mod, width == 8, xmm, 0, gpr);
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0x6E);
    emit_byte(mod, modrm64(3, xmm, gpr));
}

static void emit64_mov_gpr_from_xmm(Module* mod, int gpr, int xmm,
                                    int width) {
    emit_byte(mod, 0x66);
    emit_rex(mod, width == 8, xmm, 0, gpr);
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0x7E);
    emit_byte(mod, modrm64(3, xmm, gpr));
}

static void emit64_mov_xmm_from_memory(Module* mod, int xmm, int base,
                                       int32_t disp, int width) {
    emit_byte(mod, width == 4 ? 0xF3 : 0xF2);
    emit_rex(mod, false, xmm, 0, base);
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0x10);
    emit64_memory_operand(mod, xmm, base, disp);
}

static void emit64_mov_memory_from_xmm(Module* mod, int base, int32_t disp,
                                       int xmm, int width) {
    emit_byte(mod, width == 4 ? 0xF3 : 0xF2);
    emit_rex(mod, false, xmm, 0, base);
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0x11);
    emit64_memory_operand(mod, xmm, base, disp);
}

static void emit64_sse_binary(Module* mod, int opcode, int dst, int src,
                              int width) {
    emit_byte(mod, width == 4 ? 0xF3 : 0xF2);
    emit_rex(mod, false, dst, 0, src);
    emit_byte(mod, 0x0F);
    emit_byte(mod, (uint8_t)opcode);
    emit_byte(mod, modrm64(3, dst, src));
}

static void emit64_sse_compare(Module* mod, int lhs, int rhs, int width) {
    emit_byte(mod, width == 4 ? 0x0F : 0x66);
    if (width == 8) emit_byte(mod, 0x0F);
    emit_rex(mod, false, lhs, 0, rhs);
    emit_byte(mod, 0x2E);
    emit_byte(mod, modrm64(3, lhs, rhs));
}

static void emit64_sse_xor(Module* mod, int dst, int src, int width) {
    emit_byte(mod, width == 4 ? 0x0F : 0x66);
    if (width == 8) emit_byte(mod, 0x0F);
    emit_rex(mod, false, dst, 0, src);
    emit_byte(mod, 0x57);
    emit_byte(mod, modrm64(3, dst, src));
}

static void emit64_sse_convert(Module* mod, int opcode, int dst, int src,
                               int width) {
    emit_byte(mod, width == 4 ? 0xF3 : 0xF2);
    emit_rex(mod, false, dst, 0, src);
    emit_byte(mod, 0x0F);
    emit_byte(mod, (uint8_t)opcode);
    emit_byte(mod, modrm64(3, dst, src));
}

static void emit64_int_to_sse(Module* mod, int dst, int gpr, int source_width,
                              int destination_width) {
    emit_byte(mod, destination_width == 4 ? 0xF3 : 0xF2);
    emit_rex(mod, source_width == 8, dst, 0, gpr);
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0x2A);
    emit_byte(mod, modrm64(3, dst, gpr));
}

static void emit64_sse_to_int(Module* mod, int gpr, int src,
                              int source_width, int destination_width) {
    emit_byte(mod, source_width == 4 ? 0xF3 : 0xF2);
    emit_rex(mod, destination_width == 8, gpr, 0, src);
    emit_byte(mod, 0x0F);
    emit_byte(mod, 0x2C);
    emit_byte(mod, modrm64(3, gpr, src));
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
    if (type && type_is_floating((Type*)type)) {
        emit64_memory_operand_width(mod, reg, base, disp, width);
        return;
    }
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
    if (type && type_is_floating((Type*)type)) {
        emit64_store_memory_operand_width(mod, src, base, disp, width);
        return;
    }
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
static void gen64_lvalue(Module* mod, Expr* expr);

static bool gen64_type_has_vla(const Type* type) {
    return type && type->kind == TYPE_ARRAY &&
           (type->array_bound != NULL || gen64_type_has_vla(type->base));
}

/* Pointer objects have fixed storage, but a pointer-to-VLA parameter still
 * carries runtime array bounds that must be captured at function entry. */
static bool gen64_type_has_vla_any(const Type* type) {
    if (!type) return false;
    if (type->kind == TYPE_ARRAY) {
        return type->array_bound != NULL || gen64_type_has_vla_any(type->base);
    }
    return type->kind == TYPE_PTR && gen64_type_has_vla_any(type->base);
}

static void gen64_vla_extent(Module* mod, Type* type) {
    if (!type || type->kind != TYPE_ARRAY) {
        emit64_mov_reg_imm32(mod, RAX, type && type->size > 0
            ? (uint32_t)type->size : 0u);
        return;
    }
    if (type->base && type->base->kind == TYPE_ARRAY) {
        gen64_vla_extent(mod, type->base);
    } else {
        emit64_mov_reg_imm32(mod, RAX, type->base && type->base->size > 0
            ? (uint32_t)type->base->size : 0u);
    }
    emit64_push_reg(mod, RAX);
    if (type->array_bound) {
        gen64_expr(mod, type->array_bound);
    } else {
        emit64_mov_reg_imm32(mod, RAX, type->array_len > 0
            ? (uint32_t)type->array_len : 0u);
    }
    emit64_mov_reg_reg(mod, RCX, RAX);
    emit64_pop_reg(mod, RAX);
    emit64_imul_reg_reg(mod, RAX, RCX);
}

static int gen64_vla_dimension_count(const Type* type) {
    if (!type) return 0;
    if (type->kind == TYPE_PTR) {
        return gen64_vla_dimension_count(type->base);
    }
    if (type->kind != TYPE_ARRAY) return 0;
    return 1 + gen64_vla_dimension_count(type->base);
}

static int gen64_vla_extent_index(const Type* owner, const Type* target) {
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

static Decl* gen64_vla_owner(Expr* expression) {
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

static void gen64_vla_extents(Module* mod, Type* type, Decl* declaration,
                              int* slot_index) {
    if (type && type->kind == TYPE_PTR) {
        gen64_vla_extents(mod, type->base, declaration, slot_index);
        return;
    }
    if (!type || type->kind != TYPE_ARRAY) {
        emit64_mov_reg_imm32(mod, RAX, type && type->size > 0
            ? (uint32_t)type->size : 0u);
        return;
    }
    if (type->base && type->base->kind == TYPE_ARRAY) {
        gen64_vla_extents(mod, type->base, declaration, slot_index);
    } else {
        emit64_mov_reg_imm32(mod, RAX, type->base && type->base->size > 0
            ? (uint32_t)type->base->size : 0u);
    }
    emit64_push_reg(mod, RAX);
    if (type->array_bound) {
        gen64_expr(mod, type->array_bound);
    } else {
        emit64_mov_reg_imm32(mod, RAX, type->array_len > 0
            ? (uint32_t)type->array_len : 0u);
    }
    emit64_mov_reg_reg(mod, RCX, RAX);
    emit64_pop_reg(mod, RAX);
    emit64_imul_reg_reg(mod, RAX, RCX);
    if (declaration && declaration->var_vla_extent_count > 0 &&
        slot_index && *slot_index < declaration->var_vla_extent_count) {
        emit64_mov_mem_reg(mod, RBP,
                           declaration->var_vla_extent_offset +
                               *slot_index * 8,
                           RAX);
    }
    if (slot_index) ++*slot_index;
}

static bool gen64_saved_vla_extent(Module* mod, Type* type,
                                   Expr* expression) {
    Decl* owner = gen64_vla_owner(expression);
    Type* owner_type;
    int index;
    if (!owner || owner->var_vla_extent_count <= 0) return false;
    owner_type = owner->param_array_type ? owner->param_array_type : owner->type;
    index = gen64_vla_extent_index(owner_type, type);
    if (index < 0 || index >= owner->var_vla_extent_count) return false;
    emit64_mov_reg_mem(mod, RAX, RBP,
                       owner->var_vla_extent_offset + index * 8);
    return true;
}

static void gen64_vla_extent_for_expr(Module* mod, Type* type,
                                      Expr* expression) {
    if (!gen64_saved_vla_extent(mod, type, expression)) {
        gen64_vla_extent(mod, type);
    }
}

static void gen64_vla_alloc(Module* mod, Decl* decl) {
    int slot_index = 0;
    gen64_vla_extents(mod, decl->type, decl, &slot_index);
    emit64_sub_reg_reg(mod, RSP, RAX);
    emit64_mov_mem_reg(mod, RBP, decl->var_vla_size_offset, RAX);
    emit64_mov_reg_reg(mod, RAX, RSP);
    emit64_mov_mem_reg(mod, RBP, decl->var_offset, RAX);
}

static void gen64_vla_parameter_extents(Module* mod, Decl* decl) {
    if (!decl) return;
    for (DeclList* parameter = decl->func_params; parameter;
         parameter = parameter->next) {
        Decl* value = parameter->decl;
        int slot_index = 0;
        if (!value || value->var_vla_extent_count <= 0 ||
            !value->param_array_type ||
            !gen64_type_has_vla_any(value->param_array_type)) {
            continue;
        }
        gen64_vla_extents(mod, value->param_array_type, value, &slot_index);
    }
}

static bool gen64_is_floating(const Type* type) {
    return type && (type->kind == TYPE_FLOAT || type->kind == TYPE_DOUBLE);
}

static void emit64_jmp_label(Module* mod, int label);

typedef enum {
    GEN64_CLASS_NONE,
    GEN64_CLASS_INTEGER,
    GEN64_CLASS_SSE,
    GEN64_CLASS_MEMORY
} Gen64Class;

typedef struct {
    Gen64Class classes[2];
    int count;
    bool memory;
} Gen64AggregateClass;

static Gen64AggregateClass gen64_empty_aggregate_class(void) {
    Gen64AggregateClass result;
    result.classes[0] = GEN64_CLASS_NONE;
    result.classes[1] = GEN64_CLASS_NONE;
    result.count = 0;
    result.memory = false;
    return result;
}

typedef struct {
    Gen64AggregateClass aggregate;
    bool is_aggregate;
    bool memory;
    int storage;
    int temp_offset;
    int stack_offset;
    int gp_start;
    int fp_start;
} Gen64CallArg;

static Gen64Class gen64_merge_class(Gen64Class left, Gen64Class right) {
    if (left == GEN64_CLASS_MEMORY || right == GEN64_CLASS_MEMORY) {
        return GEN64_CLASS_MEMORY;
    }
    if (left == GEN64_CLASS_NONE) return right;
    if (right == GEN64_CLASS_NONE || left == right) return left;
    /* INTEGER dominates SSE for a mixed eightbyte. */
    return GEN64_CLASS_INTEGER;
}

static bool gen64_classify_type_at(const Type* type, int base_offset,
                                   Gen64AggregateClass* result) {
    if (!type || !result || base_offset < 0 || type->size <= 0 ||
        base_offset > 16 - type->size) {
        return false;
    }
    if (type->align > 1 && base_offset % type->align != 0) return false;
    if (type->kind == TYPE_FLOAT || type->kind == TYPE_DOUBLE) {
        int index = base_offset / 8;
        result->classes[index] = gen64_merge_class(
            result->classes[index], GEN64_CLASS_SSE);
        return true;
    }
    if (type->kind == TYPE_ARRAY) {
        if (type->array_len <= 0 || !type->base) return false;
        for (int index = 0; index < type->array_len; ++index) {
            int64_t offset = (int64_t)base_offset +
                             (int64_t)index * type->base->size;
            if (offset > INT_MAX ||
                !gen64_classify_type_at(type->base, (int)offset, result)) {
                return false;
            }
        }
        return true;
    }
    if (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) {
        if (!type->is_complete || !type->fields || type->cxx_nontrivial) {
            return false;
        }
        for (const TypeField* field = type->fields; field;
             field = field->next) {
            if (field->offset < 0 || field->offset > type->size ||
                !gen64_classify_type_at(field->type,
                                        base_offset + field->offset,
                                        result)) {
                return false;
            }
        }
        return true;
    }
    if (type_is_integer((Type*)type) || type->kind == TYPE_ENUM ||
        type->kind == TYPE_PTR || type->kind == TYPE_NULLPTR) {
        int first = base_offset / 8;
        int last = (base_offset + type->size - 1) / 8;
        for (int index = first; index <= last && index < 2; ++index) {
            result->classes[index] = gen64_merge_class(
                result->classes[index], GEN64_CLASS_INTEGER);
        }
        return last < 2;
    }
    return false;
}

static Gen64AggregateClass gen64_classify_aggregate(const Type* type) {
    Gen64AggregateClass result = {{GEN64_CLASS_NONE, GEN64_CLASS_NONE}, 0,
                                  false};
    if (!type || (type->kind != TYPE_STRUCT && type->kind != TYPE_UNION) ||
        type->size <= 0 || type->size > 16 ||
        !gen64_classify_type_at(type, 0, &result)) {
        result.memory = true;
        result.count = type && type->size > 0
            ? (type->size + 7) / 8 : 0;
        return result;
    }
    result.count = (type->size + 7) / 8;
    for (int index = 0; index < result.count; ++index) {
        if (result.classes[index] == GEN64_CLASS_NONE) {
            result.classes[index] = GEN64_CLASS_INTEGER;
        }
        if (result.classes[index] == GEN64_CLASS_MEMORY) {
            result.memory = true;
        }
    }
    return result;
}

static bool gen64_is_aggregate(const Type* type) {
    return type && (type->kind == TYPE_STRUCT ||
                    type->kind == TYPE_UNION);
}

static int gen64_aggregate_storage(const Type* type) {
    int size = type ? type->size : 0;
    if (size <= 0) size = 1;
    return (size + 7) & ~7;
}

static void gen64_copy_memory(Module* mod, int destination_base,
                              int32_t destination_offset, int source_base,
                              int32_t source_offset, int size) {
    int offset = 0;
    while (offset + 8 <= size) {
        emit64_mov_reg_mem(mod, RAX, source_base, source_offset + offset);
        emit64_mov_mem_reg(mod, destination_base,
                           destination_offset + offset, RAX);
        offset += 8;
    }
    while (offset < size) {
        emit64_load_typed(mod, RAX, source_base, source_offset + offset,
                          type_uchar);
        emit64_store_typed(mod, destination_base,
                           destination_offset + offset, RAX, type_uchar);
        ++offset;
    }
}

static int gen64_float_width(const Type* type) {
    return type && type->kind == TYPE_FLOAT ? 4 : 8;
}

static void gen64_float_binary_raw(Module* mod, int operation, int width) {
    emit64_mov_xmm_from_gpr(mod, 0, RAX, width);
    emit64_mov_xmm_from_gpr(mod, 1, RCX, width);
    emit64_sse_binary(mod, operation, 0, 1, width);
    emit64_mov_gpr_from_xmm(mod, RAX, 0, width);
}

static void gen64_float_literal(Module* mod, const Expr* expression) {
    uint64_t bits = 0u;
    if (expression->type && expression->type->kind == TYPE_FLOAT) {
        float value = (float)expression->float_val;
        memcpy(&bits, &value, sizeof(value));
    } else {
        double value = expression->float_val;
        memcpy(&bits, &value, sizeof(value));
    }
    emit64_mov_reg_imm64(mod, RAX, bits);
}

static void gen64_float_cast(Module* mod, Expr* expression) {
    Type* source_type = expression && expression->cast_expr
        ? expression->cast_expr->type : NULL;
    Type* destination_type = expression ? expression->type : NULL;
    int source_width;
    int destination_width;
    if (!source_type || !destination_type) {
        SourceLoc location;
        codegen64_expr_loc(&location, expression);
        rcc_error(location, "floating cast has no source or destination type");
        emit64_mov_reg_imm64(mod, RAX, 0u);
        return;
    }
    gen64_expr(mod, expression->cast_expr);
    if (gen64_is_floating(destination_type)) {
        destination_width = gen64_float_width(destination_type);
        if (gen64_is_floating(source_type)) {
            source_width = gen64_float_width(source_type);
            if (source_width != destination_width) {
                emit64_mov_xmm_from_gpr(mod, 0, RAX, source_width);
                /* CVTSS2SD uses F3; CVTSD2SS uses F2. */
                emit64_sse_convert(mod, 0x5A, 0, 0,
                                   source_width == 4 ? 4 : 8);
                emit64_mov_gpr_from_xmm(mod, RAX, 0, destination_width);
            }
            return;
        }
        source_width = source_type->size >= 8 ? 8 : 4;
        emit64_int_to_sse(mod, 0, RAX, source_width, destination_width);
        emit64_mov_gpr_from_xmm(mod, RAX, 0, destination_width);
        return;
    }
    if (gen64_is_floating(source_type) && type_is_integer(destination_type)) {
        source_width = gen64_float_width(source_type);
        destination_width = destination_type->size >= 8 ? 8 : 4;
        emit64_mov_xmm_from_gpr(mod, 0, RAX, source_width);
        emit64_sse_to_int(mod, RAX, 0, source_width, destination_width);
        return;
    }
    /* Integer casts retain the existing raw integer representation. */
}

/* Convert the raw value in RAX to the floating representation requested by
 * the usual arithmetic conversions.  Binary expressions keep their original
 * operand nodes, so code generation must perform this conversion explicitly
 * when one operand is an integer and the other is floating. */
static void gen64_convert_to_float(Module* mod, Type* source_type,
                                   Type* destination_type) {
    int source_width;
    int destination_width;
    bool converted = false;
    if (!source_type || !destination_type ||
        !gen64_is_floating(destination_type)) {
        return;
    }
    destination_width = gen64_float_width(destination_type);
    if (gen64_is_floating(source_type)) {
        source_width = gen64_float_width(source_type);
        if (source_width != destination_width) {
            emit64_mov_xmm_from_gpr(mod, 0, RAX, source_width);
            emit64_sse_convert(mod, 0x5A, 0, 0, source_width);
            converted = true;
        }
    } else if (type_is_integer(source_type) ||
               source_type->kind == TYPE_ENUM) {
        source_width = source_type->size >= 8 ? 8 : 4;
        emit64_int_to_sse(mod, 0, RAX, source_width, destination_width);
        converted = true;
    } else {
        return;
    }
    if (converted) {
        emit64_mov_gpr_from_xmm(mod, RAX, 0, destination_width);
    }
}

static void gen64_float_truth(Module* mod, Expr* expression) {
    int width = gen64_float_width(expression ? expression->type : NULL);
    gen64_expr(mod, expression);
    emit64_mov_xmm_from_gpr(mod, 0, RAX, width);
    emit64_sse_xor(mod, 1, 1, width);
    emit64_sse_compare(mod, 0, 1, width);
    emit64_setcc(mod, CC64_NE, RAX);
    emit64_movzx_r64_r8(mod, RAX, RAX);
    emit64_setcc(mod, CC64_P, RCX);
    emit64_movzx_r64_r8(mod, RCX, RCX);
    emit64_or_reg_reg(mod, RAX, RCX);
}

static void gen64_float_compare(Module* mod, Expr* expression) {
    Type* comparison_type = type_common(expression->binary_lhs->type,
                                        expression->binary_rhs->type);
    int width = gen64_float_width(comparison_type);
    int first_cc;
    int second_cc = CC64_NP;
    bool disjunction = false;
    gen64_expr(mod, expression->binary_lhs);
    gen64_convert_to_float(mod, expression->binary_lhs->type,
                           comparison_type);
    emit64_push_reg(mod, RAX);
    gen64_expr(mod, expression->binary_rhs);
    gen64_convert_to_float(mod, expression->binary_rhs->type,
                           comparison_type);
    emit64_mov_reg_reg(mod, RCX, RAX);
    emit64_pop_reg(mod, RAX);
    emit64_mov_xmm_from_gpr(mod, 0, RAX, width);
    emit64_mov_xmm_from_gpr(mod, 1, RCX, width);
    emit64_sse_compare(mod, 0, 1, width);
    switch (expression->kind) {
        case EXPR_EQ: first_cc = CC64_E; break;
        case EXPR_NE: first_cc = CC64_NE; second_cc = CC64_P;
                      disjunction = true; break;
        case EXPR_LT: first_cc = CC64_B; break;
        case EXPR_GT: first_cc = CC64_A; break;
        case EXPR_LE: first_cc = CC64_BE; break;
        case EXPR_GE: first_cc = CC64_AE; break;
        default: first_cc = CC64_E; break;
    }
    emit64_setcc(mod, first_cc, RAX);
    emit64_movzx_r64_r8(mod, RAX, RAX);
    emit64_setcc(mod, second_cc, RCX);
    emit64_movzx_r64_r8(mod, RCX, RCX);
    if (disjunction) emit64_or_reg_reg(mod, RAX, RCX);
    else emit64_and_reg_reg(mod, RAX, RCX);
}

static void gen64_float_add_one(Module* mod, Expr* expression,
                                bool subtract, bool post) {
    Type* type = expression->type;
    int width = gen64_float_width(type);
    uint64_t bits;
    float one_float = 1.0f;
    double one_double = 1.0;
    bits = 0u;
    memcpy(&bits, width == 4 ? (void*)&one_float : (void*)&one_double,
           (size_t)width);
    gen64_lvalue(mod, expression->unary_operand);
    emit64_push_reg(mod, RAX);
    emit64_load_typed(mod, RAX, RAX, 0, type);
    if (post) emit64_push_reg(mod, RAX);
    emit64_mov_reg_reg(mod, RCX, RAX);
    emit64_mov_reg_imm64(mod, RAX, bits);
    gen64_float_binary_raw(mod, subtract ? 0x5C : 0x58, width);
    if (post) {
        emit64_mov_reg_reg(mod, RDX, RAX);
        emit64_pop_reg(mod, RAX);
    } else {
        emit64_mov_reg_reg(mod, RDX, RAX);
    }
    emit64_pop_reg(mod, RCX);
    emit64_store_typed(mod, RCX, 0, RDX, type);
}

static void emit64_label(Module* mod, int label);
static void emit64_jcc_label(Module* mod, int cc, int label);
static Type* current_function_return_type64 = NULL;
static int current_function_sret_offset64 = 0;
static bool current_function_variadic64 = false;
static int current_function_va_gp_offset64 = 0;
static int current_function_va_fp_offset64 = 48;
static int current_function_va_overflow_offset64 = 0;
static int current_function_va_reg_save_offset64 = 0;

static void gen64_va_arg_aggregate(Module* mod, Expr* expression) {
    Gen64AggregateClass classification = gen64_classify_aggregate(
        expression ? expression->va_arg_type : NULL);
    Type* type = expression ? expression->va_arg_type : NULL;
    int overflow_label = new_label64();
    int done_label = new_label64();
    int gp_count = 0;
    int fp_count = 0;
    int rounded_size = gen64_aggregate_storage(type);

    if (!type || expression->va_arg_result_offset >= 0) {
        SourceLoc location;
        codegen64_expr_loc(&location, expression);
        rcc_error(location, "aggregate va_arg has no automatic result slot");
        emit64_mov_reg_imm32(mod, RAX, 0u);
        return;
    }
    if (!classification.memory) {
        for (int index = 0; index < classification.count; ++index) {
            if (classification.classes[index] == GEN64_CLASS_INTEGER) {
                ++gp_count;
            } else if (classification.classes[index] == GEN64_CLASS_SSE) {
                ++fp_count;
            }
        }
    }

    gen64_lvalue(mod, expression->va_list_operand);
    emit64_mov_reg_reg(mod, RCX, RAX);
    emit64_lea(mod, R11, RBP, expression->va_arg_result_offset);
    if (classification.memory) {
        emit64_jmp_label(mod, overflow_label);
    } else {
        /* gp_offset and fp_offset are 32-bit fields in the SysV va_list. */
        emit64_load_typed(mod, R8, RCX, 0, type_uint);
        emit64_cmp_reg_imm(mod, R8, 48 - gp_count * 8);
        emit64_jcc_label(mod, CC64_A, overflow_label);
        emit64_load_typed(mod, R9, RCX, 4, type_uint);
        emit64_cmp_reg_imm(mod, R9, 176 - fp_count * 16);
        emit64_jcc_label(mod, CC64_A, overflow_label);
        emit64_mov_reg_mem(mod, R10, RCX, 16);
        {
            for (int index = 0; index < classification.count; ++index) {
                if (classification.classes[index] == GEN64_CLASS_INTEGER) {
                    emit64_mov_reg_reg(mod, RAX, R8);
                    emit64_add_reg_reg(mod, RAX, R10);
                    emit64_mov_reg_mem(mod, RAX, RAX, 0);
                    emit64_mov_mem_reg(mod, R11, index * 8, RAX);
                    emit64_add_reg_imm(mod, R8, 8);
                } else {
                    emit64_mov_reg_reg(mod, RAX, R9);
                    emit64_add_reg_reg(mod, RAX, R10);
                    emit64_mov_xmm_from_memory(mod, 0, RAX, 0, 8);
                    emit64_mov_gpr_from_xmm(mod, RAX, 0, 8);
                    emit64_mov_mem_reg(mod, R11, index * 8, RAX);
                    emit64_add_reg_imm(mod, R9, 16);
                }
            }
        }
        emit64_mov_mem_reg(mod, RCX, 0, R8);
        emit64_mov_mem_reg(mod, RCX, 4, R9);
        emit64_jmp_label(mod, done_label);
    }
    emit64_label(mod, overflow_label);
    emit64_mov_reg_mem(mod, RDX, RCX, 8);
    emit64_lea(mod, RAX, RDX, rounded_size);
    emit64_mov_mem_reg(mod, RCX, 8, RAX);
    gen64_copy_memory(mod, R11, 0, RDX, 0, type->size);
    emit64_label(mod, done_label);
    emit64_mov_reg_reg(mod, RAX, R11);
}

static Type* codegen64_comparison_type(Expr* expr) {
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

static Expr* call64_argument(Expr* call, int index) {
    ExprList* argument = call->call_args;
    while (argument && index-- > 0) argument = argument->next;
    return argument ? argument->expr : NULL;
}

static const Type* atomic64_value_type(Expr* call) {
    Expr* object = call64_argument(call, 0);
    return object && object->type && object->type->kind == TYPE_PTR
        ? object->type->base : type_uint;
}

static void emit64_normalize_atomic_value(Module* mod, int reg,
                                          const Type* type) {
    int width = gen64_type_width(type);
    if (type && type->kind == TYPE_BOOL) {
        emit_rex_w(mod, reg, reg);
        emit_byte(mod, 0x85); /* test reg64, reg64 */
        emit_byte(mod, modrm64(3, reg, reg));
        emit64_setcc(mod, CC64_NE, reg);
        emit64_movzx_r64_r8(mod, reg, reg);
    } else if (width == 4) {
        if (type && !type->is_unsigned) {
            emit_rex_w(mod, reg, reg);
            emit_byte(mod, 0x63); /* movsxd reg64, reg32 */
        } else {
            emit_rex(mod, false, reg, 0, reg);
            emit_byte(mod, 0x89); /* mov reg32, reg32: zero-extend */
        }
        emit_byte(mod, modrm64(3, reg, reg));
    } else if (width < 4) {
        if (type && !type->is_unsigned) emit_rex_w(mod, reg, reg);
        emit_byte(mod, 0x0F);
        emit_byte(mod, type && !type->is_unsigned
            ? (width == 1 ? 0xBE : 0xBF)
            : (width == 1 ? 0xB6 : 0xB7));
        emit_byte(mod, modrm64(3, reg, reg));
    }
}

static void emit64_atomic_exchange_width(Module* mod, int value, int address,
                                         const Type* type) {
    int width = gen64_type_width(type);
    if (width == 2) emit_byte(mod, 0x66);
    if (width == 8) emit_rex_w(mod, value, address);
    emit_byte(mod, width == 1 ? 0x86 : 0x87);
    emit64_memory_operand(mod, value, address, 0);
}

static void emit64_atomic_xadd_width(Module* mod, int value, int address,
                                     const Type* type) {
    int width = gen64_type_width(type);
    if (width == 2) emit_byte(mod, 0x66);
    emit_byte(mod, 0xF0);
    if (width == 8) emit_rex_w(mod, value, address);
    emit_byte(mod, 0x0F);
    emit_byte(mod, width == 1 ? 0xC0 : 0xC1);
    emit64_memory_operand(mod, value, address, 0);
}

static void emit64_atomic_cmpxchg_width(Module* mod, int desired, int address,
                                        const Type* type) {
    int width = gen64_type_width(type);
    if (width == 2) emit_byte(mod, 0x66);
    emit_byte(mod, 0xF0);
    if (width == 8) emit_rex_w(mod, desired, address);
    emit_byte(mod, 0x0F);
    emit_byte(mod, width == 1 ? 0xB0 : 0xB1);
    emit64_memory_operand(mod, desired, address, 0);
}

static void emit64_atomic_clear_width(Module* mod, int address,
                                      const Type* type) {
    int width = gen64_type_width(type);
    if (width == 2) emit_byte(mod, 0x66);
    if (width == 8) emit_rex_w(mod, 0, address);
    emit_byte(mod, width == 1 ? 0xC6 : 0xC7);
    emit64_memory_operand(mod, 0, address, 0);
    if (width == 1) emit_byte(mod, 0u);
    else if (width == 2) {
        emit_byte(mod, 0u);
        emit_byte(mod, 0u);
    } else {
        emit_dword(mod, 0u);
    }
}

typedef enum {
    ATOMIC64_BITWISE_NONE,
    ATOMIC64_BITWISE_AND,
    ATOMIC64_BITWISE_OR,
    ATOMIC64_BITWISE_XOR,
    ATOMIC64_BITWISE_NAND
} Atomic64BitwiseOp;

static Atomic64BitwiseOp atomic64_bitwise_operation(const char* name) {
    if (strcmp(name, "__atomic_fetch_and") == 0 ||
        strcmp(name, "__atomic_and_fetch") == 0 ||
        strcmp(name, "__sync_fetch_and_and") == 0 ||
        strcmp(name, "__sync_and_and_fetch") == 0) {
        return ATOMIC64_BITWISE_AND;
    }
    if (strcmp(name, "__atomic_fetch_or") == 0 ||
        strcmp(name, "__atomic_or_fetch") == 0 ||
        strcmp(name, "__sync_fetch_and_or") == 0 ||
        strcmp(name, "__sync_or_and_fetch") == 0) {
        return ATOMIC64_BITWISE_OR;
    }
    if (strcmp(name, "__atomic_fetch_xor") == 0 ||
        strcmp(name, "__atomic_xor_fetch") == 0 ||
        strcmp(name, "__sync_fetch_and_xor") == 0 ||
        strcmp(name, "__sync_xor_and_fetch") == 0) {
        return ATOMIC64_BITWISE_XOR;
    }
    if (strcmp(name, "__atomic_fetch_nand") == 0 ||
        strcmp(name, "__atomic_nand_fetch") == 0 ||
        strcmp(name, "__sync_fetch_and_nand") == 0 ||
        strcmp(name, "__sync_nand_and_fetch") == 0) {
        return ATOMIC64_BITWISE_NAND;
    }
    return ATOMIC64_BITWISE_NONE;
}

static bool atomic64_bitwise_returns_new(const char* name) {
    return strcmp(name, "__atomic_and_fetch") == 0 ||
           strcmp(name, "__atomic_or_fetch") == 0 ||
           strcmp(name, "__atomic_xor_fetch") == 0 ||
           strcmp(name, "__atomic_nand_fetch") == 0 ||
           strcmp(name, "__sync_and_and_fetch") == 0 ||
           strcmp(name, "__sync_or_and_fetch") == 0 ||
           strcmp(name, "__sync_xor_and_fetch") == 0 ||
           strcmp(name, "__sync_nand_and_fetch") == 0;
}

static bool gen64_atomic_builtin(Module* mod, Expr* call) {
    Expr* function = call->call_func;
    const char* name;
    bool is_atomic;
    bool is_subtract;
    bool returns_new;
    Atomic64BitwiseOp bitwise_operation;
    const Type* value_type;

    if (!function || function->kind != EXPR_IDENT) return false;
    name = function->ident_name;
    value_type = atomic64_value_type(call);
    if (strcmp(name, "__atomic_load_n") == 0) {
        gen64_expr(mod, call64_argument(call, 1));
        gen64_expr(mod, call64_argument(call, 0));
        emit64_load_typed(mod, RAX, RAX, 0, value_type);
        return true;
    }
    if (strcmp(name, "__atomic_store_n") == 0) {
        gen64_expr(mod, call64_argument(call, 2));
        gen64_expr(mod, call64_argument(call, 0));
        emit64_push_reg(mod, RAX);
        gen64_expr(mod, call64_argument(call, 1));
        emit64_normalize_atomic_value(mod, RAX, value_type);
        emit64_pop_reg(mod, RCX);
        emit64_atomic_exchange_width(mod, RAX, RCX, value_type);
        return true;
    }
    if (strcmp(name, "__atomic_compare_exchange_n") == 0) {
        gen64_expr(mod, call64_argument(call, 5));
        gen64_expr(mod, call64_argument(call, 4));
        gen64_expr(mod, call64_argument(call, 3));
        gen64_expr(mod, call64_argument(call, 0));
        emit64_push_reg(mod, RAX);
        gen64_expr(mod, call64_argument(call, 1));
        emit64_push_reg(mod, RAX);
        gen64_expr(mod, call64_argument(call, 2));
        emit64_normalize_atomic_value(mod, RAX, value_type);
        emit64_mov_reg_reg(mod, RDX, RAX);
        emit64_pop_reg(mod, RCX); /* Expected-value address. */
        emit64_load_typed(mod, RAX, RCX, 0, value_type);
        emit64_push_reg(mod, RCX);
        emit64_mov_reg_mem(mod, RCX, RSP, 8); /* Object address. */
        emit64_atomic_cmpxchg_width(mod, RDX, RCX, value_type);
        emit64_setcc(mod, CC64_E, RDX);
        emit64_movzx_r64_r8(mod, RDX, RDX);
        emit64_pop_reg(mod, RCX);
        emit64_add_reg_imm(mod, RSP, 8);
        emit64_store_typed(mod, RCX, 0, RAX, value_type);
        emit64_mov_reg_reg(mod, RAX, RDX);
        return true;
    }
    is_atomic = strncmp(name, "__atomic_", 9) == 0;
    if (strcmp(name, "__atomic_exchange_n") == 0 ||
        strcmp(name, "__sync_lock_test_and_set") == 0) {
        if (is_atomic) gen64_expr(mod, call64_argument(call, 2));
        gen64_expr(mod, call64_argument(call, 0));
        emit64_push_reg(mod, RAX);
        gen64_expr(mod, call64_argument(call, 1));
        emit64_normalize_atomic_value(mod, RAX, value_type);
        emit64_pop_reg(mod, RCX);
        emit64_atomic_exchange_width(mod, RAX, RCX, value_type);
        emit64_normalize_atomic_value(mod, RAX, value_type);
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
        if (is_atomic) gen64_expr(mod, call64_argument(call, 2));
        gen64_expr(mod, call64_argument(call, 0));
        emit64_push_reg(mod, RAX);
        gen64_expr(mod, call64_argument(call, 1));
        emit64_normalize_atomic_value(mod, RAX, value_type);
        emit64_mov_reg_reg(mod, RDX, RAX);
        emit64_pop_reg(mod, RCX);
        emit64_mov_reg_reg(mod, RAX, RDX);
        if (is_subtract && gen64_type_width(value_type) == 8) {
            emit64_neg_reg(mod, RAX);
        } else if (is_subtract) {
            emit_byte(mod, 0xF7);
            emit_byte(mod, 0xD8); /* neg eax */
        }
        emit64_atomic_xadd_width(mod, RAX, RCX, value_type);
        emit64_normalize_atomic_value(mod, RAX, value_type);
        if (returns_new) {
            if (gen64_type_width(value_type) == 8) {
                if (is_subtract) emit64_sub_reg_reg(mod, RAX, RDX);
                else emit64_add_reg_reg(mod, RAX, RDX);
            } else {
                emit_byte(mod, is_subtract ? 0x29 : 0x01);
                emit_byte(mod, 0xD0); /* sub/add eax, edx */
            }
            emit64_normalize_atomic_value(mod, RAX, value_type);
        }
        return true;
    }
    bitwise_operation = atomic64_bitwise_operation(name);
    if (bitwise_operation != ATOMIC64_BITWISE_NONE) {
        int retry_label = new_label64();
        if (is_atomic) gen64_expr(mod, call64_argument(call, 2));
        gen64_expr(mod, call64_argument(call, 0));
        emit64_push_reg(mod, RAX); /* Object address. */
        gen64_expr(mod, call64_argument(call, 1));
        emit64_normalize_atomic_value(mod, RAX, value_type);
        emit64_push_reg(mod, RAX); /* Operand. */
        emit64_mov_reg_mem(mod, RCX, RSP, 8);
        emit64_load_typed(mod, RAX, RCX, 0, value_type);
        emit64_label(mod, retry_label);
        emit64_mov_reg_reg(mod, RDX, RAX);
        emit64_mov_reg_mem(mod, RCX, RSP, 0);
        if (bitwise_operation == ATOMIC64_BITWISE_AND ||
            bitwise_operation == ATOMIC64_BITWISE_NAND) {
            emit64_and_reg_reg(mod, RDX, RCX);
        } else if (bitwise_operation == ATOMIC64_BITWISE_OR) {
            emit64_or_reg_reg(mod, RDX, RCX);
        } else {
            emit64_xor_reg_reg(mod, RDX, RCX);
        }
        if (bitwise_operation == ATOMIC64_BITWISE_NAND) {
            emit64_not_reg(mod, RDX);
        }
        emit64_normalize_atomic_value(mod, RDX, value_type);
        emit64_mov_reg_mem(mod, RCX, RSP, 8);
        emit64_atomic_cmpxchg_width(mod, RDX, RCX, value_type);
        emit64_jcc_label(mod, CC64_NE, retry_label);
        if (atomic64_bitwise_returns_new(name)) {
            emit64_mov_reg_reg(mod, RAX, RDX);
        }
        emit64_normalize_atomic_value(mod, RAX, value_type);
        emit64_add_reg_imm(mod, RSP, 16);
        return true;
    }
    if (strcmp(name, "__sync_bool_compare_and_swap") == 0 ||
        strcmp(name, "__sync_val_compare_and_swap") == 0) {
        gen64_expr(mod, call64_argument(call, 0));
        emit64_push_reg(mod, RAX);
        gen64_expr(mod, call64_argument(call, 1));
        emit64_normalize_atomic_value(mod, RAX, value_type);
        emit64_push_reg(mod, RAX);
        gen64_expr(mod, call64_argument(call, 2));
        emit64_normalize_atomic_value(mod, RAX, value_type);
        emit64_mov_reg_reg(mod, RDX, RAX);
        emit64_pop_reg(mod, RAX);
        emit64_pop_reg(mod, RCX);
        emit64_atomic_cmpxchg_width(mod, RDX, RCX, value_type);
        if (strcmp(name, "__sync_bool_compare_and_swap") == 0) {
            emit64_setcc(mod, CC64_E, RAX);
            emit64_movzx_r64_r8(mod, RAX, RAX);
        } else {
            emit64_normalize_atomic_value(mod, RAX, value_type);
        }
        return true;
    }
    if (strcmp(name, "__sync_lock_release") == 0) {
        gen64_expr(mod, call64_argument(call, 0));
        emit64_atomic_clear_width(mod, RAX, value_type);
        return true;
    }
    if (strcmp(name, "__atomic_thread_fence") == 0 ||
        strcmp(name, "__sync_synchronize") == 0) {
        if (strcmp(name, "__atomic_thread_fence") == 0) {
            gen64_expr(mod, call64_argument(call, 0));
        }
        emit_byte(mod, 0xF0);
        emit_byte(mod, 0x83);
        emit_byte(mod, 0x0C);
        emit_byte(mod, 0x24);
        emit_byte(mod, 0x00); /* lock or dword ptr [rsp], 0 */
        return true;
    }
    return false;
}

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

static bool gen64_aggregate_zero_initializer(Type* type,
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
           expr_eval_integer_constant(item->expr, &value) && value == 0;
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
    if (gen64_aggregate_zero_initializer(type, initializer)) return true;
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
    if ((type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) &&
        initializer->type && type_is_compatible(type, initializer->type)) {
        int offset = 0;
        if (initializer->kind == EXPR_VA_ARG) {
            gen64_expr(mod, initializer);
        } else {
            gen64_lvalue(mod, initializer);
        }
        emit64_mov_reg_reg(mod, RCX, RAX);
        while (offset + 8 <= type->size) {
            emit64_mov_reg_mem(mod, RAX, RCX, offset);
            emit64_mov_mem_reg(mod, RBP, displacement + offset, RAX);
            offset += 8;
        }
        while (offset < type->size) {
            emit64_load_typed(mod, RAX, RCX, offset, type_uchar);
            emit64_store_typed(mod, RBP, displacement + offset, RAX,
                               type_uchar);
            ++offset;
        }
        return true;
    }
    if (!type_is_integer(type) && type->kind != TYPE_ENUM &&
        type->kind != TYPE_PTR && type->kind != TYPE_NULLPTR &&
        !gen64_is_floating(type)) {
        return false;
    }
    gen64_expr(mod, initializer);
    if (type_is_integer(type) || type->kind == TYPE_ENUM) {
        emit64_normalize_atomic_value(mod, RAX, type);
    }
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
        bool resolved = false;
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
                resolved = true;
                break;
            }
        }
        if (!resolved) {
            module_add_relocation(mod, MODULE_SYMBOL_CODE,
                                  reference->call_offset, 0u,
                                  true, false, reference->function_name);
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

static void gen64_local_vtable_init(Module* mod, Type* type,
                                    int32_t displacement) {
    if (!mod || !type || type->cxx_vtable_size <= 0 ||
        !type->cxx_vtable_symbol) return;
    gen64_symbol_address(mod, type->cxx_vtable_symbol, 0u);
    emit64_mov_mem_reg(mod, RBP, displacement, RAX);
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

static bool gen64_inline_method_address(Module* mod, Expr* expr);

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
                gen64_tls_address(mod, decl_link_name(decl));
                if (decl->type && decl->type->is_reference) {
                    emit64_mov_reg_mem(mod, RAX, RAX, 0);
                }
            } else if (decl->kind == DECL_FUNC) {
                gen64_symbol_address(mod, decl_link_name(decl), 0u);
            } else if (decl->var_is_vla) {
                emit64_mov_reg_mem(mod, RAX, RBP, decl->var_offset);
            } else if (decl->type && decl->type->is_reference) {
                if (decl->var_is_global) {
                    gen64_symbol_address(mod, decl_link_name(decl), 0u);
                    emit64_mov_reg_mem(mod, RAX, RAX, 0);
                } else {
                    emit64_mov_reg_mem(mod, RAX, RBP, decl->var_offset);
                }
            } else if (decl->var_is_global) {
                gen64_symbol_address(mod, decl_link_name(decl), 0u);
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
            if (gen64_type_has_vla(expr->type)) {
                emit64_push_reg(mod, RAX);
                gen64_vla_extent_for_expr(mod, expr->type,
                                          expr->index_base);
                emit64_mov_reg_reg(mod, RCX, RAX);
                emit64_pop_reg(mod, RAX);
                emit64_imul_reg_reg(mod, RAX, RCX);
            } else if (expr->type && expr->type->size > 1) {
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

        case EXPR_COMPOUND:
            if (!expr->compound_type || expr->compound_offset >= 0) {
                rcc_error(expr->loc,
                          "compound literal has no automatic storage slot");
                emit64_mov_reg_imm32(mod, RAX, 0u);
                break;
            }
            if (expr->compound_type->kind == TYPE_ARRAY ||
                expr->compound_type->kind == TYPE_STRUCT ||
                expr->compound_type->kind == TYPE_UNION) {
                gen64_zero_local_storage(mod, expr->compound_offset,
                                         (size_t)expr->compound_type->size);
            }
            if (!gen64_local_initializer(mod, expr->compound_type, expr,
                                         expr->compound_offset)) {
                rcc_error(expr->loc,
                          "unsupported compound literal initializer");
            }
            gen64_local_vtable_init(mod, expr->compound_type,
                                    expr->compound_offset);
            emit64_lea(mod, RAX, RBP, expr->compound_offset);
            break;

        case EXPR_CALL:
            if (expr->call_method && expr->call_method->return_type &&
                expr->call_method->return_type->is_reference &&
                gen64_inline_method_address(mod, expr)) {
                break;
            }
            if (!expr->type ||
                (expr->type->kind != TYPE_STRUCT &&
                 expr->type->kind != TYPE_UNION) ||
                expr->call_result_offset >= 0) {
                rcc_error(expr->loc,
                          "aggregate call has no automatic result slot");
                emit64_mov_reg_imm32(mod, RAX, 0u);
                break;
            }
            gen64_expr(mod, expr);
            emit64_lea(mod, RAX, RBP, expr->call_result_offset);
            break;

        case EXPR_VA_ARG:
            if (!gen64_is_aggregate(expr ? expr->va_arg_type : NULL)) {
                rcc_error(expr->loc,
                          "va_arg aggregate address requested for scalar");
                emit64_mov_reg_imm32(mod, RAX, 0u);
            } else {
                gen64_expr(mod, expr);
            }
            break;

        case EXPR_ASSIGN:
            if (expr->type &&
                (expr->type->kind == TYPE_STRUCT ||
                 expr->type->kind == TYPE_UNION)) {
                gen64_expr(mod, expr);
            } else {
                rcc_error(expr->loc, "assignment expression is not an lvalue");
            }
            break;

        default:
            rcc_error(expr->loc, "not an lvalue");
            break;
    }
}

static bool gen64_inline_method_address(Module* mod, Expr* expr) {
    TypeMethod* method = expr ? expr->call_method : NULL;
    Expr* member = expr ? expr->call_func : NULL;
    if (!method || !member || !method->field) return false;
    if (member->kind == EXPR_PTR_MEMBER) {
        gen64_expr(mod, member->member_base);
    } else {
        gen64_lvalue(mod, member->member_base);
    }
    if (method->field->offset > 0) {
        emit64_add_reg_imm(mod, RAX, method->field->offset);
    }
    return true;
}

static void emit64_compare_constant(Module* mod, int reg, int64_t value) {
    if ((uint64_t)value == (uint64_t)(int64_t)(int32_t)value) {
        emit64_cmp_reg_imm(mod, reg, (int32_t)value);
    } else {
        emit64_mov_reg_imm64(mod, RCX, (uint64_t)value);
        emit64_cmp_reg_reg(mod, reg, RCX);
    }
}

static bool gen64_inline_close_call(Module* mod, Expr* expr) {
    CxxCloseCall* lowering = expr ? expr->cxx_close_call : NULL;
    TypeMethod* method = expr ? expr->call_method : NULL;
    int done_label;
    if (!lowering || !method || method->kind != TYPE_METHOD_FIELD_CLOSE ||
        !lowering->object || !lowering->handle || !lowering->cleanup ||
        !method->field || !method->result_field ||
        expr->call_result_offset >= 0) {
        return false;
    }

    done_label = new_label64();
    gen64_zero_local_storage(mod, expr->call_result_offset,
                             (size_t)expr->type->size);
    gen64_expr(mod, lowering->handle);
    emit64_compare_constant(mod, RAX, method->constant);
    emit64_jcc_label(mod, CC64_E, done_label);

    gen64_expr(mod, lowering->cleanup);
    emit64_store_typed(
        mod, RBP,
        expr->call_result_offset + method->result_field->offset,
        RAX, method->result_field->type);
    emit64_compare_constant(mod, RAX, method->success_constant);
    emit64_jcc_label(mod, CC64_NE, done_label);

    gen64_lvalue(mod, lowering->object);
    if (method->field->offset > 0) {
        emit64_add_reg_imm(mod, RAX, method->field->offset);
    }
    emit64_mov_reg_reg(mod, RCX, RAX);
    emit64_mov_reg_imm64(mod, RAX, (uint64_t)method->constant);
    emit64_store_typed(mod, RCX, 0, RAX, method->field->type);
    emit64_label(mod, done_label);
    return true;
}

static bool gen64_inline_method_call(Module* mod, Expr* expr) {
    TypeMethod* method = expr ? expr->call_method : NULL;
    if (method && method->kind == TYPE_METHOD_FIELD_CLOSE) {
        return gen64_inline_close_call(mod, expr);
    }
    if (!method || !gen64_inline_method_address(mod, expr)) return false;
    if (expr->type &&
        (expr->type->kind == TYPE_STRUCT ||
         expr->type->kind == TYPE_UNION ||
         expr->type->kind == TYPE_ARRAY)) {
        return true;
    }
    if (method->kind == TYPE_METHOD_FIELD_RELEASE) {
        emit64_mov_reg_reg(mod, RCX, RAX);
        emit64_load_typed(mod, RAX, RCX, 0, method->field->type);
        emit64_push_reg(mod, RAX);
        emit64_mov_reg_imm64(mod, RAX, (uint64_t)method->constant);
        emit64_store_typed(mod, RCX, 0, RAX, method->field->type);
        emit64_pop_reg(mod, RAX);
        return true;
    }
    emit64_load_typed(mod, RAX, RAX, 0, method->field->type);
    if (method->kind == TYPE_METHOD_FIELD_EQ_CONSTANT ||
        method->kind == TYPE_METHOD_FIELD_NE_CONSTANT) {
        uint64_t constant = (uint64_t)method->constant;
        if (constant == (uint64_t)(int64_t)(int32_t)constant) {
            emit64_cmp_reg_imm(mod, RAX, (int32_t)constant);
        } else {
            emit64_mov_reg_imm64(mod, RCX, constant);
            emit64_cmp_reg_reg(mod, RAX, RCX);
        }
        emit64_setcc(mod,
                     method->kind == TYPE_METHOD_FIELD_EQ_CONSTANT
                         ? CC64_E : CC64_NE,
                     RAX);
        emit64_movzx_r64_r8(mod, RAX, RAX);
    }
    return true;
}

static bool gen64_cxx_move_assignment(Module* mod, Expr* expr) {
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

    done_label = new_label64();
    gen64_lvalue(mod, expr->binary_lhs);
    emit64_push_reg(mod, RAX);
    gen64_lvalue(mod, lowering->source);
    emit64_pop_reg(mod, RCX);
    emit64_cmp_reg_reg(mod, RAX, RCX);
    emit64_jcc_label(mod, CC64_E, done_label);

    gen64_expr(mod, lowering->cleanup);
    gen64_expr(mod, lowering->release);
    emit64_push_reg(mod, RAX);
    gen64_lvalue(mod, expr->binary_lhs);
    if (field->offset > 0) {
        emit64_add_reg_imm(mod, RAX, field->offset);
    }
    emit64_mov_reg_reg(mod, RCX, RAX);
    emit64_pop_reg(mod, RAX);
    emit64_store_typed(mod, RCX, 0, RAX, field->type);
    emit64_label(mod, done_label);
    gen64_lvalue(mod, expr->binary_lhs);
    return true;
}

static void gen64_cxx_zero_array(Module* mod, Expr* expr) {
    Type* object_type = expr ? expr->call_new_type : NULL;
    int loop;
    int done;

    if (!object_type || !expr->call_new_count) {
        SourceLoc location;
        codegen64_expr_loc(&location, expr);
        rcc_error(location, "array new value-initialization has no element count");
        emit64_mov_reg_imm32(mod, RAX, 0u);
        return;
    }
    /* Store the bound in the private call frame before invoking rin_malloc;
     * this avoids reevaluating a side-effecting bound during zeroing. */
    gen64_expr(mod, expr->call_new_count);
    emit64_sub_reg_imm(mod, RSP, 16);
    emit64_mov_mem_reg(mod, RSP, 0, RAX);
    emit64_mov_reg_reg(mod, RCX, RAX);
    emit64_mov_reg_imm32(mod, RAX, (uint32_t)object_type->size);
    emit64_imul_reg_reg(mod, RAX, RCX);
    emit64_mov_reg_reg(mod, RDI, RAX);
    emit_byte(mod, 0xE8);
    {
        uint32_t call_offset = code_offset(mod);
        emit_dword(mod, 0u);
        add_func_call_ref64("rin_malloc", call_offset);
    }
    emit64_mov_mem_reg(mod, RSP, 8, RAX);
    emit64_mov_reg_mem(mod, RDX, RSP, 8);
    emit64_mov_reg_mem(mod, RCX, RSP, 0);
    loop = new_label64();
    done = new_label64();
    emit64_cmp_reg_imm(mod, RCX, 0);
    emit64_jcc_label(mod, CC64_E, done);
    emit64_label(mod, loop);
    emit64_mov_reg_imm32(mod, RAX, 0u);
    {
        int offset = 0;
        for (; offset + 8 <= object_type->size; offset += 8) {
            emit64_mov_mem_reg(mod, RDX, offset, RAX);
        }
        if (offset + 4 <= object_type->size) {
            emit64_store_typed(mod, RDX, offset, RAX, type_uint);
            offset += 4;
        }
        for (; offset < object_type->size; ++offset) {
            emit64_store_typed(mod, RDX, offset, RAX, type_uchar);
        }
    }
    emit64_add_reg_imm(mod, RDX, object_type->size);
    emit64_sub_reg_imm(mod, RCX, 1);
    emit64_cmp_reg_imm(mod, RCX, 0);
    emit64_jcc_label(mod, CC64_NE, loop);
    emit64_label(mod, done);
    emit64_mov_reg_mem(mod, RAX, RSP, 8);
    emit64_add_reg_imm(mod, RSP, 16);
}

static void gen64_cxx_init_array(Module* mod, Expr* expr) {
    Type* element_type = expr ? expr->call_new_type : NULL;
    ExprList* argument;
    int offset = 0;

    if (!element_type || element_type->size <= 0 ||
        !expr || !expr->call_new_count || !expr->call_new_args) {
        SourceLoc location;
        codegen64_expr_loc(&location, expr);
        rcc_error(location, "array new initializer has invalid element storage");
        emit64_mov_reg_imm32(mod, RAX, 0u);
        return;
    }
    /* Keep the element count in the allocation expression only; the
     * semantic pass has already proved that this constant count covers every
     * initializer, so no second evaluation or unchecked runtime bound is
     * needed here. */
    gen64_expr(mod, expr->call_new_count);
    emit64_mov_reg_reg(mod, RCX, RAX);
    emit64_mov_reg_imm32(mod, RAX, (uint32_t)element_type->size);
    emit64_imul_reg_reg(mod, RAX, RCX);
    emit64_mov_reg_reg(mod, RDI, RAX);
    emit_byte(mod, 0xE8);
    {
        uint32_t call_offset = code_offset(mod);
        emit_dword(mod, 0u);
        add_func_call_ref64("rin_malloc", call_offset);
    }
    emit64_push_reg(mod, RAX);
    for (argument = expr->call_new_args; argument;
         argument = argument->next, offset += element_type->size) {
        emit64_mov_reg_mem(mod, RCX, RSP, 0);
        gen64_expr(mod, argument->expr);
        if (gen64_is_floating(element_type)) {
            gen64_convert_to_float(mod, argument->expr->type, element_type);
        } else if (type_is_integer(element_type) ||
                   element_type->kind == TYPE_ENUM) {
            emit64_normalize_atomic_value(mod, RAX, element_type);
        }
        emit64_store_typed(mod, RCX, offset, RAX, element_type);
    }
    emit64_pop_reg(mod, RAX);
}

static void gen64_cxx_zero_object(Module* mod, Type* object_type,
                                  int address_reg) {
    emit64_mov_reg_imm32(mod, RAX, 0u);
    int offset = 0;
    for (; offset + 8 <= object_type->size; offset += 8) {
        emit64_mov_mem_reg(mod, address_reg, offset, RAX);
    }
    if (offset + 4 <= object_type->size) {
        emit64_store_typed(mod, address_reg, offset, RAX, type_uint);
        offset += 4;
    }
    while (offset < object_type->size) {
        emit64_store_typed(mod, address_reg, offset, RAX, type_uchar);
        ++offset;
    }
}

static TypeField* gen64_cxx_constructor_field(Type* object_type,
                                               const char* name) {
    for (TypeField* field = object_type ? object_type->fields : NULL;
         field; field = field->next) {
        if (field->name && name && strcmp(field->name, name) == 0) {
            return field;
        }
    }
    return NULL;
}

static void gen64_cxx_initialize_object(Module* mod, Type* object_type,
                                         CxxConstructorInfo* constructor,
                                         ExprList* arguments) {
    TypeField* field;
    CxxConstructorInitializer* initializer;
    ExprList* argument;
    int address_reg = RCX;

    gen64_cxx_zero_object(mod, object_type, address_reg);
    if (!constructor) return;
    if (constructor->parameter_count == 0) {
        for (initializer = constructor->initializers; initializer;
             initializer = initializer->next) {
            field = gen64_cxx_constructor_field(
                object_type, initializer->field);
            if (!field || !initializer->value) {
                rcc_fatal("validated C++ constructor field is missing");
            }
            emit64_push_reg(mod, address_reg);
            gen64_expr(mod, initializer->value);
            emit64_pop_reg(mod, address_reg);
            emit64_store_typed(mod, address_reg, field->offset,
                               RAX, field->type);
        }
        return;
    }
    field = object_type ? object_type->fields : NULL;
    for (argument = arguments; argument && field;
         argument = argument->next, field = field->next) {
        emit64_push_reg(mod, address_reg);
        gen64_expr(mod, argument->expr);
        emit64_pop_reg(mod, address_reg);
        emit64_store_typed(mod, address_reg, field->offset,
                           RAX, field->type);
    }
}

static void gen64_cxx_init_class_array(Module* mod, Expr* expr) {
    Type* object_type = expr ? expr->call_new_type : NULL;
    TypeField* field = object_type ? object_type->fields : NULL;
    ExprList* argument;

    if (!object_type || !field || object_type->size <= 0 ||
        !expr || !expr->call_new_count || !expr->call_new_constructor) {
        SourceLoc location;
        codegen64_expr_loc(&location, expr);
        rcc_error(location,
                  "array new constructor has invalid element storage");
        emit64_mov_reg_imm32(mod, RAX, 0u);
        return;
    }
    gen64_expr(mod, expr->call_new_count);
    emit64_sub_reg_imm(mod, RSP, 24);
    emit64_mov_mem_reg(mod, RSP, 0, RAX);  /* count */
    emit64_mov_reg_imm32(mod, RCX, (uint32_t)object_type->size);
    emit64_imul_reg_reg(mod, RAX, RCX);
    emit64_mov_reg_reg(mod, RDI, RAX);
    emit_byte(mod, 0xE8);
    {
        uint32_t call_offset = code_offset(mod);
        emit_dword(mod, 0u);
        add_func_call_ref64("rin_malloc", call_offset);
    }
    emit64_mov_mem_reg(mod, RSP, 8, RAX);  /* base */
    emit64_mov_mem_reg(mod, RSP, 16, RAX); /* current */
    for (argument = expr->call_new_args; argument;
         argument = argument->next) {
        emit64_mov_reg_mem(mod, RCX, RSP, 16);
        gen64_cxx_initialize_object(mod, object_type,
                                     expr->call_new_constructor,
                                     argument);
        emit64_mov_reg_mem(mod, RCX, RSP, 16);
        emit64_add_reg_imm(mod, RCX, (uint32_t)object_type->size);
        emit64_mov_mem_reg(mod, RSP, 16, RCX);
    }
    emit64_mov_reg_mem(mod, RAX, RSP, 8);
    emit64_add_reg_imm(mod, RSP, 24);
}

static void gen64_cxx_init_default_class_array(Module* mod, Expr* expr) {
    Type* object_type = expr ? expr->call_new_type : NULL;
    int loop;
    int done;

    if (!object_type || object_type->size <= 0 ||
        !expr || !expr->call_new_count || !expr->call_new_constructor) {
        SourceLoc location;
        codegen64_expr_loc(&location, expr);
        rcc_error(location,
                  "array new default constructor has invalid element storage");
        emit64_mov_reg_imm32(mod, RAX, 0u);
        return;
    }
    gen64_expr(mod, expr->call_new_count);
    emit64_sub_reg_imm(mod, RSP, 24);
    emit64_mov_mem_reg(mod, RSP, 0, RAX);  /* count */
    emit64_mov_reg_imm32(mod, RCX, (uint32_t)object_type->size);
    emit64_imul_reg_reg(mod, RAX, RCX);
    emit64_mov_reg_reg(mod, RDI, RAX);
    emit_byte(mod, 0xE8);
    {
        uint32_t call_offset = code_offset(mod);
        emit_dword(mod, 0u);
        add_func_call_ref64("rin_malloc", call_offset);
    }
    emit64_mov_mem_reg(mod, RSP, 8, RAX);  /* base */
    emit64_mov_mem_reg(mod, RSP, 16, RAX); /* current */
    emit64_mov_reg_mem(mod, RCX, RSP, 16);
    emit64_mov_reg_mem(mod, RDX, RSP, 0);
    loop = new_label64();
    done = new_label64();
    emit64_cmp_reg_imm(mod, RDX, 0);
    emit64_jcc_label(mod, CC64_E, done);
    emit64_label(mod, loop);
    gen64_cxx_initialize_object(mod, object_type,
                                 expr->call_new_constructor, NULL);
    emit64_add_reg_imm(mod, RCX, (uint32_t)object_type->size);
    emit64_sub_reg_imm(mod, RDX, 1);
    emit64_cmp_reg_imm(mod, RDX, 0);
    emit64_jcc_label(mod, CC64_NE, loop);
    emit64_label(mod, done);
    emit64_mov_reg_mem(mod, RAX, RSP, 8);
    emit64_add_reg_imm(mod, RSP, 24);
}

static void gen64_cxx_new(Module* mod, Expr* expr) {
    Type* object_type = expr ? expr->call_new_type : NULL;
    TypeField* field;
    ExprList* argument;
    bool saved_is_new;
    bool initialize;

    if (!object_type || object_type->size <= 0) {
        SourceLoc location;
        codegen64_expr_loc(&location, expr);
        rcc_error(location, "C++ new expression has no complete storage type");
        emit64_mov_reg_imm32(mod, RAX, 0u);
        return;
    }
    if (expr->call_new_is_array && expr->call_new_constructor &&
        expr->call_new_args) {
        gen64_cxx_init_class_array(mod, expr);
        return;
    }
    if (expr->call_new_is_array && expr->call_new_constructor &&
        !expr->call_new_args) {
        gen64_cxx_init_default_class_array(mod, expr);
        return;
    }
    if (expr->call_new_is_array && expr->call_new_value_init) {
        gen64_cxx_zero_array(mod, expr);
        return;
    }
    if (expr->call_new_is_array && expr->call_new_args) {
        gen64_cxx_init_array(mod, expr);
        return;
    }
    saved_is_new = expr->call_is_new;
    expr->call_is_new = false;
    gen64_expr(mod, expr);
    expr->call_is_new = saved_is_new;

    if (expr->call_new_is_array) {
        return;
    }
    argument = expr->call_new_args;
    initialize = expr->call_new_value_init ||
                 expr->call_new_constructor != NULL ||
                 argument != NULL;
    if (!initialize) return;

    emit64_push_reg(mod, RAX);
    emit64_mov_reg_mem(mod, RCX, RSP, 0);
    if (expr->call_new_constructor) {
        gen64_cxx_initialize_object(mod, object_type,
                                     expr->call_new_constructor,
                                     argument);
        emit64_pop_reg(mod, RAX);
        return;
    }
    emit64_mov_reg_imm32(mod, RAX, 0u);
    if (object_type->kind == TYPE_STRUCT ||
        object_type->kind == TYPE_UNION) {
        int offset = 0;
        for (; offset + 8 <= object_type->size; offset += 8) {
            emit64_mov_mem_reg(mod, RCX, offset, RAX);
        }
        if (offset + 4 <= object_type->size) {
            emit64_store_typed(mod, RCX, offset, RAX, type_uint);
            offset += 4;
        }
        while (offset < object_type->size) {
            emit64_store_typed(mod, RCX, offset, RAX, type_uchar);
            ++offset;
        }
        field = object_type->fields;
        while (field && argument) {
            emit64_mov_reg_mem(mod, RCX, RSP, 0);
            gen64_expr(mod, argument->expr);
            emit64_store_typed(mod, RCX, field->offset, RAX, field->type);
            field = field->next;
            argument = argument->next;
        }
    } else if (argument) {
        emit64_mov_reg_mem(mod, RCX, RSP, 0);
        gen64_expr(mod, argument->expr);
        emit64_store_typed(mod, RCX, 0, RAX, object_type);
    } else {
        emit64_mov_reg_mem(mod, RCX, RSP, 0);
        emit64_store_typed(mod, RCX, 0, RAX, object_type);
    }
    emit64_pop_reg(mod, RAX);
}

static void gen64_cxx_delete(Module* mod, Expr* expr) {
    Decl* cleanup = expr ? expr->call_delete_cleanup : NULL;
    TypeField* field = expr ? expr->call_delete_cleanup_field : NULL;
    int skip_cleanup;
    int done;

    if (!cleanup || !field || !expr->call_args ||
        !expr->call_args->expr) {
        return;
    }
    skip_cleanup = new_label64();
    done = new_label64();
    gen64_expr(mod, expr->call_args->expr);
    emit64_cmp_reg_imm(mod, RAX, 0);
    emit64_jcc_label(mod, CC64_E, done);
    emit64_push_reg(mod, RAX); /* Keep the object address across destructor. */
    emit64_mov_reg_mem(mod, RCX, RSP, 0);
    emit64_load_typed(mod, RAX, RCX, field->offset, field->type);
    emit64_compare_constant(mod, RAX,
                            expr->call_delete_cleanup_invalid);
    emit64_jcc_label(mod, CC64_E, skip_cleanup);
    emit64_mov_reg_reg(mod, RDI, RAX);
    emit_byte(mod, 0xE8);
    uint32_t call_offset = code_offset(mod);
    emit_dword(mod, 0);
    add_func_call_ref64(decl_link_name(cleanup), call_offset);
    emit64_label(mod, skip_cleanup);
    emit64_mov_reg_mem(mod, RDI, RSP, 0);
    emit_byte(mod, 0xE8);
    uint32_t free_offset = code_offset(mod);
    emit_dword(mod, 0);
    add_func_call_ref64("rin_free", free_offset);
    emit64_add_reg_imm(mod, RSP, 8);
    emit64_mov_reg_imm32(mod, RAX, 0u);
    emit64_label(mod, done);
}

static void gen64_expr_raw(Module* mod, Expr* expr) {
    if (!expr) return;

    switch (expr->kind) {
        case EXPR_INT_LIT:
            emit64_mov_reg_imm64(mod, RAX, (uint64_t)expr->int_val);
            break;

        case EXPR_CHAR_LIT:
            emit64_mov_reg_imm32(mod, RAX, (uint32_t)(uint8_t)expr->char_val);
            break;

        case EXPR_FLOAT_LIT:
            gen64_float_literal(mod, expr);
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
                gen64_symbol_address(mod, decl_link_name(decl), 0u);
            } else if (decl->type && decl->type->is_reference) {
                gen64_lvalue(mod, expr);
                if (expr->type && expr->type->kind != TYPE_ARRAY &&
                    expr->type->kind != TYPE_STRUCT &&
                    expr->type->kind != TYPE_UNION) {
                    emit64_load_typed(mod, RAX, RAX, 0, expr->type);
                }
            } else if (decl->var_is_thread_local) {
                gen64_lvalue(mod, expr);
                emit64_load_typed(mod, RAX, RAX, 0, decl->type);
            } else if (decl->type && decl->type->kind == TYPE_ARRAY) {
                gen64_lvalue(mod, expr);
            } else if (decl->var_is_global) {
                gen64_symbol_address(mod, decl_link_name(decl), 0u);
                emit64_load_typed(mod, RAX, RAX, 0, decl->type);
            } else {
                emit64_load_typed(mod, RAX, RBP, decl->var_offset,
                                  decl->type);
            }
            break;
        }

        case EXPR_NEG:
            if (gen64_is_floating(expr->type)) {
                int width = gen64_float_width(expr->type);
                gen64_expr(mod, expr->unary_operand);
                emit64_mov_xmm_from_gpr(mod, 1, RAX, width);
                emit64_sse_xor(mod, 0, 0, width);
                emit64_sse_binary(mod, 0x5C, 0, 1, width);
                emit64_mov_gpr_from_xmm(mod, RAX, 0, width);
            } else {
                gen64_expr(mod, expr->unary_operand);
                emit64_neg_reg(mod, RAX);
            }
            break;

        case EXPR_BITNOT:
            gen64_expr(mod, expr->unary_operand);
            emit64_not_reg(mod, RAX);
            break;

        case EXPR_NOT:
            if (gen64_is_floating(expr->unary_operand->type)) {
                gen64_float_truth(mod, expr->unary_operand);
                emit64_xor_reg_reg(mod, RAX, 1);
            } else {
                gen64_expr(mod, expr->unary_operand);
                emit64_cmp_reg_imm(mod, RAX, 0);
                emit64_setcc(mod, CC64_E, RAX);
                emit64_movzx_r64_r8(mod, RAX, RAX);
            }
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
            if (gen64_is_floating(expr->type)) {
                gen64_float_add_one(mod, expr,
                                    expr->kind == EXPR_PREDEC, false);
                break;
            }
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
            if (gen64_is_floating(expr->type)) {
                gen64_float_add_one(mod, expr,
                                    expr->kind == EXPR_POSTDEC, true);
                break;
            }
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
            if (gen64_is_floating(expr->type)) {
                gen64_expr(mod, expr->binary_lhs);
                gen64_convert_to_float(mod, expr->binary_lhs->type,
                                       expr->type);
                emit64_push_reg(mod, RAX);
                gen64_expr(mod, expr->binary_rhs);
                gen64_convert_to_float(mod, expr->binary_rhs->type,
                                       expr->type);
                emit64_mov_reg_reg(mod, RCX, RAX);
                emit64_pop_reg(mod, RAX);
                gen64_float_binary_raw(mod, 0x58,
                                       gen64_float_width(expr->type));
                break;
            }
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
            if (gen64_is_floating(expr->type)) {
                gen64_expr(mod, expr->binary_lhs);
                gen64_convert_to_float(mod, expr->binary_lhs->type,
                                       expr->type);
                emit64_push_reg(mod, RAX);
                gen64_expr(mod, expr->binary_rhs);
                gen64_convert_to_float(mod, expr->binary_rhs->type,
                                       expr->type);
                emit64_mov_reg_reg(mod, RCX, RAX);
                emit64_pop_reg(mod, RAX);
                gen64_float_binary_raw(mod, 0x5C,
                                       gen64_float_width(expr->type));
                break;
            }
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
            if (gen64_is_floating(expr->type)) {
                gen64_expr(mod, expr->binary_lhs);
                gen64_convert_to_float(mod, expr->binary_lhs->type,
                                       expr->type);
                emit64_push_reg(mod, RAX);
                gen64_expr(mod, expr->binary_rhs);
                gen64_convert_to_float(mod, expr->binary_rhs->type,
                                       expr->type);
                emit64_mov_reg_reg(mod, RCX, RAX);
                emit64_pop_reg(mod, RAX);
                gen64_float_binary_raw(mod, 0x59,
                                       gen64_float_width(expr->type));
                break;
            }
            gen64_expr(mod, expr->binary_lhs);
            emit64_push_reg(mod, RAX);
            gen64_expr(mod, expr->binary_rhs);
            emit64_mov_reg_reg(mod, RCX, RAX);
            emit64_pop_reg(mod, RAX);
            emit64_imul_reg_reg(mod, RAX, RCX);
            break;

        case EXPR_DIV:
        case EXPR_MOD:
            if (expr->kind == EXPR_DIV && gen64_is_floating(expr->type)) {
                gen64_expr(mod, expr->binary_lhs);
                gen64_convert_to_float(mod, expr->binary_lhs->type,
                                       expr->type);
                emit64_push_reg(mod, RAX);
                gen64_expr(mod, expr->binary_rhs);
                gen64_convert_to_float(mod, expr->binary_rhs->type,
                                       expr->type);
                emit64_mov_reg_reg(mod, RCX, RAX);
                emit64_pop_reg(mod, RAX);
                gen64_float_binary_raw(mod, 0x5E,
                                       gen64_float_width(expr->type));
                break;
            }
            gen64_expr(mod, expr->binary_lhs);
            emit64_push_reg(mod, RAX);
            gen64_expr(mod, expr->binary_rhs);
            emit64_mov_reg_reg(mod, RCX, RAX);
            emit64_pop_reg(mod, RAX);
            if (expr->type && expr->type->is_unsigned) {
                emit64_xor_reg_reg(mod, RDX, RDX);
                emit64_div_reg(mod, RCX);
            } else {
                emit64_cqo(mod);
                emit64_idiv_reg(mod, RCX);
            }
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
            emit64_normalize_atomic_value(mod, RAX, expr->type);
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
            emit64_normalize_atomic_value(mod, RAX, expr->type);
            break;

        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_GT:
        case EXPR_LE:
        case EXPR_GE: {
            if (gen64_is_floating(expr->binary_lhs->type) ||
                gen64_is_floating(expr->binary_rhs->type)) {
                gen64_float_compare(mod, expr);
                break;
            }
            gen64_expr(mod, expr->binary_lhs);
            emit64_push_reg(mod, RAX);
            gen64_expr(mod, expr->binary_rhs);
            emit64_mov_reg_reg(mod, RCX, RAX);
            emit64_pop_reg(mod, RAX);
            emit64_cmp_reg_reg(mod, RAX, RCX);

            int cc;
            Type* comparison_type = codegen64_comparison_type(expr);
            bool unsigned_compare = comparison_type &&
                                    comparison_type->is_unsigned;
            switch (expr->kind) {
                case EXPR_EQ: cc = CC64_E; break;
                case EXPR_NE: cc = CC64_NE; break;
                case EXPR_LT: cc = unsigned_compare ? CC64_B : CC64_L; break;
                case EXPR_GT: cc = unsigned_compare ? CC64_A : CC64_G; break;
                case EXPR_LE: cc = unsigned_compare ? CC64_BE : CC64_LE; break;
                case EXPR_GE: cc = unsigned_compare ? CC64_AE : CC64_GE; break;
                default: cc = CC64_E; break;
            }

            emit64_setcc(mod, cc, RAX);
            emit64_movzx_r64_r8(mod, RAX, RAX);
            break;
        }

        case EXPR_AND: {
            int end_label = new_label64();
            if (gen64_is_floating(expr->binary_lhs->type)) {
                gen64_float_truth(mod, expr->binary_lhs);
            } else {
                gen64_expr(mod, expr->binary_lhs);
            }
            emit64_test_reg_reg(mod, RAX, RAX);
            emit64_jcc_label(mod, CC64_E, end_label);
            if (gen64_is_floating(expr->binary_rhs->type)) {
                gen64_float_truth(mod, expr->binary_rhs);
            } else {
                gen64_expr(mod, expr->binary_rhs);
            }
            emit64_test_reg_reg(mod, RAX, RAX);
            emit64_setcc(mod, CC64_NE, RAX);
            emit64_movzx_r64_r8(mod, RAX, RAX);
            emit64_label(mod, end_label);
            break;
        }

        case EXPR_OR: {
            int end_label = new_label64();
            if (gen64_is_floating(expr->binary_lhs->type)) {
                gen64_float_truth(mod, expr->binary_lhs);
            } else {
                gen64_expr(mod, expr->binary_lhs);
            }
            emit64_test_reg_reg(mod, RAX, RAX);
            emit64_jcc_label(mod, CC64_NE, end_label);
            if (gen64_is_floating(expr->binary_rhs->type)) {
                gen64_float_truth(mod, expr->binary_rhs);
            } else {
                gen64_expr(mod, expr->binary_rhs);
            }
            emit64_label(mod, end_label);
            emit64_test_reg_reg(mod, RAX, RAX);
            emit64_setcc(mod, CC64_NE, RAX);
            emit64_movzx_r64_r8(mod, RAX, RAX);
            break;
        }

        case EXPR_ASSIGN:
            if (gen64_cxx_move_assignment(mod, expr)) break;
            if (expr->binary_lhs->type &&
                (expr->binary_lhs->type->kind == TYPE_STRUCT ||
                 expr->binary_lhs->type->kind == TYPE_UNION)) {
                int offset = 0;
                if (expr->binary_rhs->kind == EXPR_ASSIGN ||
                    expr->binary_rhs->kind == EXPR_VA_ARG) {
                    gen64_expr(mod, expr->binary_rhs);
                } else {
                    gen64_lvalue(mod, expr->binary_rhs);
                }
                emit64_push_reg(mod, RAX);
                gen64_lvalue(mod, expr->binary_lhs);
                emit64_mov_reg_reg(mod, RDX, RAX);
                emit64_pop_reg(mod, RCX);
                while (offset + 8 <= expr->binary_lhs->type->size) {
                    emit64_mov_reg_mem(mod, RAX, RCX, offset);
                    emit64_mov_mem_reg(mod, RDX, offset, RAX);
                    offset += 8;
                }
                while (offset < expr->binary_lhs->type->size) {
                    emit64_load_typed(mod, RAX, RCX, offset, type_uchar);
                    emit64_store_typed(mod, RDX, offset, RAX, type_uchar);
                    ++offset;
                }
                emit64_mov_reg_reg(mod, RAX, RDX);
                break;
            }
            gen64_expr(mod, expr->binary_rhs);
            emit64_push_reg(mod, RAX);
            gen64_lvalue(mod, expr->binary_lhs);
            emit64_pop_reg(mod, RCX);
            if (type_is_integer(expr->binary_lhs->type) ||
                expr->binary_lhs->type->kind == TYPE_ENUM) {
                emit64_normalize_atomic_value(mod, RCX,
                                               expr->binary_lhs->type);
            }
            emit64_store_typed(mod, RAX, 0, RCX,
                               expr->binary_lhs->type);
            emit64_mov_reg_reg(mod, RAX, RCX);
            break;

        case EXPR_ADD_ASSIGN:
        case EXPR_SUB_ASSIGN: {
            if (gen64_is_floating(expr->binary_lhs->type)) {
                int width = gen64_float_width(expr->binary_lhs->type);
                gen64_lvalue(mod, expr->binary_lhs);
                emit64_push_reg(mod, RAX);
                emit64_load_typed(mod, RAX, RAX, 0,
                                  expr->binary_lhs->type);
                emit64_push_reg(mod, RAX);
                gen64_expr(mod, expr->binary_rhs);
                gen64_convert_to_float(mod, expr->binary_rhs->type,
                                       expr->binary_lhs->type);
                emit64_mov_reg_reg(mod, RCX, RAX);
                emit64_pop_reg(mod, RAX);
                gen64_float_binary_raw(
                    mod, expr->kind == EXPR_ADD_ASSIGN ? 0x58 : 0x5C,
                    width);
                emit64_pop_reg(mod, RCX);
                emit64_store_typed(mod, RCX, 0, RAX,
                                   expr->binary_lhs->type);
                break;
            }
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
            emit64_normalize_atomic_value(mod, RAX,
                                          expr->binary_lhs->type);
            emit64_pop_reg(mod, RCX);
            emit64_store_typed(mod, RCX, 0, RAX,
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
            if (gen64_is_floating(expr->binary_lhs->type) &&
                (expr->kind == EXPR_MUL_ASSIGN ||
                 expr->kind == EXPR_DIV_ASSIGN)) {
                int width = gen64_float_width(expr->binary_lhs->type);
                gen64_lvalue(mod, expr->binary_lhs);
                emit64_push_reg(mod, RAX);
                emit64_load_typed(mod, RAX, RAX, 0,
                                  expr->binary_lhs->type);
                emit64_push_reg(mod, RAX);
                gen64_expr(mod, expr->binary_rhs);
                gen64_convert_to_float(mod, expr->binary_rhs->type,
                                       expr->binary_lhs->type);
                emit64_mov_reg_reg(mod, RCX, RAX);
                emit64_pop_reg(mod, RAX);
                gen64_float_binary_raw(
                    mod, expr->kind == EXPR_MUL_ASSIGN ? 0x59 : 0x5E,
                    width);
                emit64_pop_reg(mod, RCX);
                emit64_store_typed(mod, RCX, 0, RAX,
                                   expr->binary_lhs->type);
                break;
            }
            gen64_lvalue(mod, expr->binary_lhs);
            emit64_push_reg(mod, RAX);
            emit64_load_typed(mod, RAX, RAX, 0,
                              expr->binary_lhs->type);
            emit64_push_reg(mod, RAX);
            gen64_expr(mod, expr->binary_rhs);
            emit64_mov_reg_reg(mod, RCX, RAX);
            emit64_pop_reg(mod, RAX);
            if (expr->kind == EXPR_MUL_ASSIGN) {
                emit64_imul_reg_reg(mod, RAX, RCX);
            } else if (expr->kind == EXPR_DIV_ASSIGN ||
                       expr->kind == EXPR_MOD_ASSIGN) {
                if (operation_type && operation_type->is_unsigned) {
                    emit64_xor_reg_reg(mod, RDX, RDX);
                    emit64_div_reg(mod, RCX);
                } else {
                    emit64_cqo(mod);
                    emit64_idiv_reg(mod, RCX);
                }
                if (expr->kind == EXPR_MOD_ASSIGN) {
                    emit64_mov_reg_reg(mod, RAX, RDX);
                }
            } else if (expr->kind == EXPR_AND_ASSIGN) {
                emit64_and_reg_reg(mod, RAX, RCX);
            } else if (expr->kind == EXPR_OR_ASSIGN) {
                emit64_or_reg_reg(mod, RAX, RCX);
            } else if (expr->kind == EXPR_XOR_ASSIGN) {
                emit64_xor_reg_reg(mod, RAX, RCX);
            } else if (expr->kind == EXPR_LSHIFT_ASSIGN) {
                emit64_shl_reg_cl(mod, RAX);
            } else if (expr->binary_lhs->type &&
                       expr->binary_lhs->type->is_unsigned) {
                emit64_shr_reg_cl(mod, RAX);
            } else {
                emit64_sar_reg_cl(mod, RAX);
            }
            emit64_normalize_atomic_value(mod, RAX,
                                          expr->binary_lhs->type);
            emit64_pop_reg(mod, RCX);
            emit64_store_typed(mod, RCX, 0, RAX,
                               expr->binary_lhs->type);
            break;
        }

        case EXPR_COND: {
            int else_label = new_label64();
            int end_label = new_label64();
            if (gen64_is_floating(expr->cond_test->type)) {
                gen64_float_truth(mod, expr->cond_test);
            } else {
                gen64_expr(mod, expr->cond_test);
            }
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
            if (expr->call_is_new) {
                gen64_cxx_new(mod, expr);
                break;
            }
            if (expr->call_is_delete && expr->call_delete_cleanup) {
                gen64_cxx_delete(mod, expr);
                break;
            }
            if (gen64_inline_method_call(mod, expr)) break;
            if (gen64_atomic_builtin(mod, expr)) break;
            /* x86-64 System V ABI: RDI, RSI, RDX, RCX, R8, R9 */
            int arg_regs[] = {RDI, RSI, RDX, RCX, R8, R9};
            int argc = exprlist_len(expr->call_args);
            int gp_cursor;
            int fp_cursor;
            int temp_bytes = 0;
            int stack_bytes = 0;
            int stack_padding;
            bool aggregate_result = expr->type &&
                (expr->type->kind == TYPE_STRUCT ||
                 expr->type->kind == TYPE_UNION);
            Gen64AggregateClass result_class;
            if (aggregate_result) {
                result_class = gen64_classify_aggregate(expr->type);
            } else {
                result_class = gen64_empty_aggregate_class();
            }
            bool memory_result = aggregate_result && result_class.memory;
            int register_base = memory_result ? 1 : 0;
            Gen64CallArg* call_arguments = rcc_alloc(
                (size_t)argc * sizeof(*call_arguments));
            Type* function_type;
            TypeParam* parameter;

            /* Collect arguments */
            ExprList** args = rcc_alloc(argc * sizeof(ExprList*));
            Type** argument_types = rcc_alloc(argc * sizeof(Type*));
            function_type = expr->call_func ? expr->call_func->type : NULL;
            if (function_type && function_type->kind == TYPE_PTR) {
                function_type = function_type->base;
            }
            parameter = function_type && function_type->kind == TYPE_FUNC
                ? function_type->params : NULL;
            int i = 0;
            for (ExprList* a = expr->call_args; a; a = a->next) {
                args[i++] = a;
                argument_types[i - 1] = parameter ? parameter->type
                    : (a->expr->type &&
                       (a->expr->type->kind == TYPE_ENUM ||
                        a->expr->type->kind < TYPE_INT)
                        ? type_int
                        : (a->expr->type &&
                           a->expr->type->kind == TYPE_FLOAT
                            ? type_double : a->expr->type));
                if (parameter) parameter = parameter->next;
                call_arguments[i - 1].is_aggregate = gen64_is_aggregate(
                    argument_types[i - 1]);
                if (call_arguments[i - 1].is_aggregate) {
                    call_arguments[i - 1].aggregate =
                        gen64_classify_aggregate(argument_types[i - 1]);
                } else {
                    call_arguments[i - 1].aggregate =
                        gen64_empty_aggregate_class();
                }
                call_arguments[i - 1].memory = false;
                call_arguments[i - 1].storage = call_arguments[i - 1].is_aggregate
                    ? gen64_aggregate_storage(argument_types[i - 1]) : 8;
            }
            gp_cursor = register_base;
            fp_cursor = 0;
            for (i = 0; i < argc; ++i) {
                Gen64CallArg* argument = &call_arguments[i];
                Type* type = argument_types[i];
                if (argument->is_aggregate) {
                    int gp_count = 0;
                    int fp_count = 0;
                    if (!argument->aggregate.memory) {
                        for (int index = 0;
                             index < argument->aggregate.count; ++index) {
                            if (argument->aggregate.classes[index] ==
                                GEN64_CLASS_INTEGER) {
                                ++gp_count;
                            } else if (argument->aggregate.classes[index] ==
                                       GEN64_CLASS_SSE) {
                                ++fp_count;
                            }
                        }
                    }
                    if (argument->aggregate.memory ||
                        gp_cursor + gp_count > 6 || fp_cursor + fp_count > 8) {
                        argument->memory = true;
                    } else {
                        argument->gp_start = gp_cursor;
                        argument->fp_start = fp_cursor;
                        gp_cursor += gp_count;
                        fp_cursor += fp_count;
                    }
                } else if (type && type->is_reference) {
                    if (gp_cursor < 6) {
                        argument->gp_start = gp_cursor++;
                    } else {
                        argument->memory = true;
                    }
                } else if (gen64_is_floating(type)) {
                    if (fp_cursor < 8) {
                        argument->fp_start = fp_cursor++;
                    } else {
                        argument->memory = true;
                    }
                } else if (gp_cursor < 6) {
                    argument->gp_start = gp_cursor++;
                } else {
                    argument->memory = true;
                }
                if (argument->memory) {
                    argument->stack_offset = stack_bytes;
                    stack_bytes += argument->storage;
                }
                call_arguments[i].temp_offset = temp_bytes;
                temp_bytes += argument->storage;
            }
            /* A generated function keeps RSP 16-byte aligned after its
             * prologue.  The SysV ABI requires the same alignment immediately
             * before CALL (the hardware push then gives the callee an RSP
             * value congruent to 8 modulo 16). */
            stack_padding = (16 - ((temp_bytes + stack_bytes) & 15)) & 15;

            /* Evaluate arguments into a private, contiguous temporary area.
             * This keeps source evaluation independent of register assignment
             * and lets mixed INTEGER/SSE aggregates be copied losslessly. */
            if (temp_bytes) emit64_sub_reg_imm(mod, RSP, temp_bytes);
            emit64_mov_reg_reg(mod, R10, RSP);
            for (i = argc - 1; i >= 0; i--) {
                Expr* argument = args[i]->expr;
                Type* passed_type = argument_types[i];
                Gen64CallArg* layout = &call_arguments[i];
                if (layout->is_aggregate) {
                    if (argument->kind == EXPR_VA_ARG) {
                        gen64_expr(mod, argument);
                    } else {
                        gen64_lvalue(mod, argument);
                    }
                    emit64_mov_reg_reg(mod, R11, RAX);
                    gen64_copy_memory(mod, RSP, layout->temp_offset,
                                      R11, 0, passed_type->size);
                } else if (passed_type && passed_type->is_reference) {
                    gen64_lvalue(mod, argument);
                    emit64_mov_mem_reg(mod, RSP, layout->temp_offset, RAX);
                } else {
                    gen64_expr(mod, argument);
                    if (gen64_is_floating(passed_type)) {
                        /* Sema records the source expression type and the
                         * call ABI type separately.  This matters for both
                         * fixed floating parameters and the default
                         * promotion of float variadic arguments: the raw
                         * value must be converted before it is copied to the
                         * temporary area, otherwise a float's 32-bit bits
                         * would be interpreted as a double. */
                        gen64_convert_to_float(mod, argument->type,
                                               passed_type);
                    } else if (type_is_integer(passed_type) ||
                        (passed_type && passed_type->kind == TYPE_ENUM)) {
                        emit64_normalize_atomic_value(mod, RAX,
                                                       passed_type);
                    }
                    emit64_mov_mem_reg(mod, RSP, layout->temp_offset, RAX);
                }
            }

            if (stack_bytes + stack_padding) {
                emit64_sub_reg_imm(mod, RSP, stack_bytes + stack_padding);
                emit64_mov_reg_reg(mod, RAX, RSP);
                emit64_mov_reg_imm32(mod, RDX, 0u);
                for (i = 0; i < stack_bytes; i += 8) {
                    emit64_mov_mem_reg(mod, RSP, i, RDX);
                }
                emit64_mov_reg_reg(mod, R10, RSP);
                emit64_add_reg_imm(mod, R10, stack_bytes + stack_padding);
                for (i = 0; i < argc; ++i) {
                    Gen64CallArg* layout = &call_arguments[i];
                    if (layout->memory) {
                        gen64_copy_memory(mod, RSP, layout->stack_offset,
                                          R10, layout->temp_offset,
                                          layout->storage);
                    }
                }
            } else if (temp_bytes) {
                /* Nested calls may clobber all caller-saved registers while
                 * evaluating the source arguments.  Re-anchor the temporary
                 * area before reloading the final argument registers. */
                emit64_mov_reg_reg(mod, R10, RSP);
            }
            gp_cursor = register_base;
            fp_cursor = 0;
            for (i = 0; i < argc; ++i) {
                Type* passed_type = argument_types[i];
                Gen64CallArg* layout = &call_arguments[i];
                if (layout->memory) continue;
                if (layout->is_aggregate) {
                    for (int index = 0; index < layout->aggregate.count;
                         ++index) {
                        if (layout->aggregate.classes[index] ==
                            GEN64_CLASS_INTEGER) {
                            emit64_mov_reg_mem(
                                mod, arg_regs[gp_cursor++],
                                R10, layout->temp_offset + index * 8);
                        } else {
                            emit64_mov_xmm_from_memory(
                                mod, fp_cursor++, R10,
                                layout->temp_offset + index * 8, 8);
                        }
                    }
                } else if (passed_type && passed_type->is_reference) {
                    emit64_mov_reg_mem(mod, arg_regs[gp_cursor++], R10,
                                       layout->temp_offset);
                } else if (gen64_is_floating(passed_type)) {
                    emit64_mov_xmm_from_memory(mod, fp_cursor++, R10,
                                               layout->temp_offset,
                                               gen64_float_width(passed_type));
                } else {
                    emit64_mov_reg_mem(mod, arg_regs[gp_cursor++], R10,
                                       layout->temp_offset);
                }
            }
            rcc_free(argument_types);
            rcc_free(args);

            if (aggregate_result && expr->call_result_offset >= 0) {
                rcc_error(expr->loc,
                          "aggregate call has no automatic result slot");
            }
            if (memory_result) {
                emit64_lea(mod, RDI, RBP, expr->call_result_offset);
            }

            /* Direct calls use rel32 and produce a .ro relocation only when
             * the definition is external to this translation unit. */
            if (expr->call_is_virtual && expr->call_virtual_index >= 0) {
                /* The implicit this argument is always the first GP
                 * argument.  Load the most-derived function from its vptr
                 * after all source arguments have been evaluated. */
                emit64_mov_reg_mem(mod, RAX, arg_regs[register_base], 0);
                emit64_mov_reg_mem(mod, RAX, RAX,
                                   expr->call_virtual_index * 8);
                emit_byte(mod, 0xFF);  /* CALL RAX */
                emit_byte(mod, modrm64(3, 2, RAX));
            } else if (expr->call_func->kind == EXPR_IDENT &&
                expr->call_func->ident_decl &&
                expr->call_func->ident_decl->kind == DECL_FUNC) {
                Decl* function = expr->call_func->ident_decl;
                uint32_t call_offset;
                if (function_type && function_type->kind == TYPE_FUNC &&
                    function_type->variadic) {
                    emit64_mov_reg_imm32(mod, RAX, (uint32_t)fp_cursor);
                }
                emit_byte(mod, 0xE8);
                call_offset = code_offset(mod);
                emit_dword(mod, 0u);
                /* Resolve local definitions after the complete translation
                 * unit has been emitted; unresolved calls retain a typed
                 * rel32 relocation for rld to resolve. */
                add_func_call_ref64(decl_link_name(function), call_offset);
            } else {
                gen64_expr(mod, expr->call_func);
                emit64_mov_reg_reg(mod, R11, RAX);
                if (function_type && function_type->kind == TYPE_FUNC &&
                    function_type->variadic) {
                    emit64_mov_reg_imm32(mod, RAX, (uint32_t)fp_cursor);
                }
                emit_byte(mod, 0xFF);  /* CALL R11 */
                emit_byte(mod, modrm64(3, 2, R11));
            }

            if (expr->type && gen64_is_floating(expr->type)) {
                emit64_mov_gpr_from_xmm(
                    mod, RAX, 0, gen64_float_width(expr->type));
            }

            if (aggregate_result && !memory_result) {
                int gp_return = 0;
                int fp_return = 0;
                for (int index = 0; index < result_class.count; ++index) {
                    if (result_class.classes[index] == GEN64_CLASS_INTEGER) {
                        emit64_mov_mem_reg(
                            mod, RBP, expr->call_result_offset + index * 8,
                            gp_return++ == 0 ? RAX : RDX);
                    } else {
                        emit64_mov_gpr_from_xmm(mod, RAX, fp_return++, 8);
                        emit64_mov_mem_reg(
                            mod, RBP, expr->call_result_offset + index * 8,
                            RAX);
                    }
                }
            }

            /* Clean up stack arguments */
            if (temp_bytes + stack_bytes + stack_padding) {
                emit64_add_reg_imm(mod, RSP,
                                   temp_bytes + stack_bytes + stack_padding);
            }
            rcc_free(call_arguments);
            break;
        }

        case EXPR_VA_START:
            gen64_expr(mod, expr->va_list_operand);
            emit64_mov_reg_reg(mod, RCX, RAX);
            emit64_mov_reg_imm32(
                mod, RAX, (uint32_t)current_function_va_gp_offset64);
            emit64_store_typed(mod, RCX, 0, RAX, type_uint);
            emit64_mov_reg_imm32(
                mod, RAX, (uint32_t)current_function_va_fp_offset64);
            emit64_store_typed(mod, RCX, 4, RAX, type_uint);
            emit64_lea(mod, RAX, RBP,
                       current_function_va_overflow_offset64);
            emit64_mov_mem_reg(mod, RCX, 8, RAX);
            emit64_lea(mod, RAX, RBP,
                       current_function_va_reg_save_offset64);
            emit64_mov_mem_reg(mod, RCX, 16, RAX);
            emit64_mov_reg_imm32(mod, RAX, 0u);
            break;

        case EXPR_VA_END:
            emit64_mov_reg_imm32(mod, RAX, 0u);
            break;

        case EXPR_VA_COPY:
            gen64_expr(mod, expr->va_second_operand);
            emit64_push_reg(mod, RAX);
            gen64_expr(mod, expr->va_list_operand);
            emit64_pop_reg(mod, RCX);
            emit64_mov_reg_mem(mod, RDX, RCX, 0);
            emit64_mov_mem_reg(mod, RAX, 0, RDX);
            emit64_mov_reg_mem(mod, RDX, RCX, 8);
            emit64_mov_mem_reg(mod, RAX, 8, RDX);
            emit64_mov_reg_mem(mod, RDX, RCX, 16);
            emit64_mov_mem_reg(mod, RAX, 16, RDX);
            emit64_mov_reg_imm32(mod, RAX, 0u);
            break;

        case EXPR_VA_ARG: {
            if (gen64_is_aggregate(expr->va_arg_type)) {
                gen64_va_arg_aggregate(mod, expr);
                break;
            }
            int overflow_label = new_label64();
            int load_label = new_label64();
            gen64_expr(mod, expr->va_list_operand);
            emit64_mov_reg_reg(mod, RCX, RAX);
            if (gen64_is_floating(expr->va_arg_type)) {
                emit64_load_typed(mod, RAX, RCX, 4, type_uint);
                emit64_cmp_reg_imm(mod, RAX, 160);
                emit64_jcc_label(mod, CC64_A, overflow_label);
                emit64_mov_reg_mem(mod, RDX, RCX, 16);
                emit64_add_reg_reg(mod, RDX, RAX);
                emit64_add_reg_imm(mod, RAX, 16);
                emit64_store_typed(mod, RCX, 4, RAX, type_uint);
                emit64_mov_xmm_from_memory(
                    mod, 0, RDX, 0, gen64_float_width(expr->va_arg_type));
                emit64_mov_gpr_from_xmm(
                    mod, RAX, 0, gen64_float_width(expr->va_arg_type));
            } else {
                emit64_load_typed(mod, RAX, RCX, 0, type_uint);
                emit64_cmp_reg_imm(mod, RAX, 40);
                emit64_jcc_label(mod, CC64_A, overflow_label);
                emit64_mov_reg_mem(mod, RDX, RCX, 16);
                emit64_add_reg_reg(mod, RDX, RAX);
                emit64_add_reg_imm(mod, RAX, 8);
                emit64_store_typed(mod, RCX, 0, RAX, type_uint);
            }
            emit64_jmp_label(mod, load_label);
            emit64_label(mod, overflow_label);
            emit64_mov_reg_mem(mod, RDX, RCX, 8);
            emit64_lea(mod, RAX, RDX, 8);
            emit64_mov_mem_reg(mod, RCX, 8, RAX);
            emit64_label(mod, load_label);
            emit64_load_typed(mod, RAX, RDX, 0, expr->va_arg_type);
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

        case EXPR_COMPOUND:
            gen64_lvalue(mod, expr);
            if (!expr->type || (expr->type->kind != TYPE_ARRAY &&
                                expr->type->kind != TYPE_STRUCT &&
                                expr->type->kind != TYPE_UNION)) {
                emit64_load_typed(mod, RAX, RAX, 0, expr->type);
            }
            break;

        case EXPR_CAST:
            if (gen64_is_floating(expr->type) ||
                gen64_is_floating(expr->cast_expr->type)) {
                gen64_float_cast(mod, expr);
            } else {
                gen64_expr(mod, expr->cast_expr);
            }
            break;

        case EXPR_SIZEOF:
            if (expr->sizeof_type && gen64_type_has_vla(expr->sizeof_type)) {
                gen64_vla_extent(mod, expr->sizeof_type);
            } else if (expr->unary_operand &&
                       expr->unary_operand->kind == EXPR_IDENT &&
                       expr->unary_operand->ident_decl &&
                       expr->unary_operand->ident_decl->var_is_vla) {
                emit64_mov_reg_mem(
                    mod, RAX, RBP,
                    expr->unary_operand->ident_decl->var_vla_size_offset);
            } else if (expr->unary_operand &&
                       gen64_type_has_vla(expr->unary_operand->type)) {
                gen64_vla_extent_for_expr(mod, expr->unary_operand->type,
                                          expr->unary_operand);
            } else if (expr->sizeof_type) {
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

typedef struct SwitchCaseCodegen64 {
    Stmt* statement;
    uint64_t bits;
    int label;
    struct SwitchCaseCodegen64* next;
} SwitchCaseCodegen64;

typedef struct SwitchCodegenContext64 {
    Type* control_type;
    SwitchCaseCodegen64* cases;
    Stmt* default_statement;
    int default_label;
    struct SwitchCodegenContext64* previous;
} SwitchCodegenContext64;

static SwitchCodegenContext64* current_switch_codegen64 = NULL;

typedef struct NamedCodegenLabel64 {
    const char* name;
    int label;
    struct NamedCodegenLabel64* next;
} NamedCodegenLabel64;

static NamedCodegenLabel64* named_codegen_labels64 = NULL;

static int codegen64_named_label(const char* name) {
    NamedCodegenLabel64* item = named_codegen_labels64;
    while (item) {
        if (strcmp(item->name, name) == 0) return item->label;
        item = item->next;
    }
    item = rcc_alloc(sizeof(*item));
    item->name = name;
    item->label = new_label64();
    item->next = named_codegen_labels64;
    named_codegen_labels64 = item;
    return item->label;
}

static void codegen64_release_named_labels(void) {
    while (named_codegen_labels64) {
        NamedCodegenLabel64* next = named_codegen_labels64->next;
        rcc_free(named_codegen_labels64);
        named_codegen_labels64 = next;
    }
}

static void gen64_expr(Module* mod, Expr* expr) {
    if (!expr) return;
    gen64_expr_raw(mod, expr);
    if (type_is_integer(expr->type) || expr->type->kind == TYPE_ENUM) {
        emit64_normalize_atomic_value(mod, RAX, expr->type);
    }
}

typedef struct CleanupCodegen64 {
    Expr* expression;
    struct CleanupCodegen64* previous;
} CleanupCodegen64;

typedef struct VLAScopeCodegen64 {
    int stack_offset;
    struct VLAScopeCodegen64* previous;
} VLAScopeCodegen64;

static CleanupCodegen64* active_cleanups64 = NULL;
static CleanupCodegen64* break_cleanup_marker64 = NULL;
static CleanupCodegen64* continue_cleanup_marker64 = NULL;
static VLAScopeCodegen64* active_vla_scopes64 = NULL;
static VLAScopeCodegen64* break_vla_marker64 = NULL;
static VLAScopeCodegen64* continue_vla_marker64 = NULL;

static void gen64_cleanups_until(Module* mod, CleanupCodegen64* marker) {
    for (CleanupCodegen64* item = active_cleanups64;
         item && item != marker; item = item->previous) {
        gen64_expr(mod, item->expression);
    }
}

static bool gen64_cleanup_count(Module* mod, unsigned count) {
    CleanupCodegen64* item = active_cleanups64;
    while (item && count > 0u) {
        gen64_expr(mod, item->expression);
        item = item->previous;
        --count;
    }
    return count == 0u;
}

static void discard64_cleanups_until(CleanupCodegen64* marker) {
    while (active_cleanups64 && active_cleanups64 != marker) {
        CleanupCodegen64* previous = active_cleanups64->previous;
        rcc_free(active_cleanups64);
        active_cleanups64 = previous;
    }
}

static void gen64_vla_scopes_until(Module* mod,
                                   VLAScopeCodegen64* marker) {
    for (VLAScopeCodegen64* item = active_vla_scopes64;
         item && item != marker; item = item->previous) {
        emit64_mov_reg_mem(mod, RSP, RBP, item->stack_offset);
    }
}

static bool gen64_vla_count(Module* mod, unsigned count) {
    VLAScopeCodegen64* item = active_vla_scopes64;
    while (item && count > 0u) {
        emit64_mov_reg_mem(mod, RSP, RBP, item->stack_offset);
        item = item->previous;
        --count;
    }
    return count == 0u;
}

static void discard64_vla_scopes_until(VLAScopeCodegen64* marker) {
    while (active_vla_scopes64 && active_vla_scopes64 != marker) {
        VLAScopeCodegen64* previous = active_vla_scopes64->previous;
        rcc_free(active_vla_scopes64);
        active_vla_scopes64 = previous;
    }
}

static void record64_vla_scope(Decl* declaration) {
    VLAScopeCodegen64* scope = rcc_alloc(sizeof(*scope));
    scope->stack_offset = declaration->var_vla_scope_offset;
    scope->previous = active_vla_scopes64;
    active_vla_scopes64 = scope;
}

static void gen64_scoped_stmt(Module* mod, Stmt* statement) {
    CleanupCodegen64* marker = active_cleanups64;
    gen64_stmt(mod, statement);
    gen64_cleanups_until(mod, marker);
    discard64_cleanups_until(marker);
}

static Type* codegen64_switch_control_type(Type* type) {
    if (!type || type->kind == TYPE_ENUM || type->kind < TYPE_INT) {
        return type_int;
    }
    return type;
}

static uint64_t codegen64_switch_case_bits(Expr* expression,
                                           Type* control_type) {
    int64_t value = 0;
    unsigned width = control_type && control_type->size > 0
        ? (unsigned)control_type->size * 8u : 32u;
    uint64_t bits;
    (void)expr_eval_integer_constant(expression, &value);
    bits = (uint64_t)value;
    if (width < 64u) {
        uint64_t mask = (UINT64_C(1) << width) - 1u;
        bits &= mask;
        if (control_type && !control_type->is_unsigned &&
            (bits & (UINT64_C(1) << (width - 1u))) != 0u) {
            bits |= ~mask;
        }
    }
    return bits;
}

static void codegen64_collect_switch_cases(
    Stmt* statement, SwitchCodegenContext64* context) {
    if (!statement) return;
    switch (statement->kind) {
        case STMT_SWITCH:
            return;
        case STMT_CASE: {
            SwitchCaseCodegen64* item = rcc_alloc(sizeof(*item));
            item->statement = statement;
            item->bits = codegen64_switch_case_bits(statement->case_val,
                                                     context->control_type);
            item->label = new_label64();
            item->next = context->cases;
            context->cases = item;
            codegen64_collect_switch_cases(statement->case_stmt, context);
            return;
        }
        case STMT_DEFAULT:
            context->default_statement = statement;
            context->default_label = new_label64();
            codegen64_collect_switch_cases(statement->default_stmt, context);
            return;
        case STMT_BLOCK:
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                codegen64_collect_switch_cases(item->stmt, context);
            }
            return;
        case STMT_IF:
            codegen64_collect_switch_cases(statement->if_then, context);
            codegen64_collect_switch_cases(statement->if_else, context);
            return;
        case STMT_WHILE:
        case STMT_DO:
            codegen64_collect_switch_cases(statement->while_body, context);
            return;
        case STMT_FOR:
            codegen64_collect_switch_cases(statement->for_body, context);
            return;
        case STMT_LABEL:
            codegen64_collect_switch_cases(statement->label_stmt, context);
            return;
        default:
            return;
    }
}

static SwitchCaseCodegen64* codegen64_find_switch_case(
    SwitchCodegenContext64* context, Stmt* statement) {
    SwitchCaseCodegen64* item = context ? context->cases : NULL;
    while (item && item->statement != statement) item = item->next;
    return item;
}

static void codegen64_release_switch_cases(SwitchCaseCodegen64* item) {
    while (item) {
        SwitchCaseCodegen64* next = item->next;
        rcc_free(item);
        item = next;
    }
}

static int codegen64_asm_register(const char* constraint)
{
    if (!constraint) return -1;
    while (*constraint == '=' || *constraint == '+' || *constraint == '&') {
        ++constraint;
    }
    if (strcmp(constraint, "{eax}") == 0 ||
        strcmp(constraint, "{rax}") == 0) return RAX;
    if (strcmp(constraint, "{ebx}") == 0 ||
        strcmp(constraint, "{rbx}") == 0) return RBX;
    if (strcmp(constraint, "{ecx}") == 0 ||
        strcmp(constraint, "{rcx}") == 0) return RCX;
    if (strcmp(constraint, "{edx}") == 0 ||
        strcmp(constraint, "{rdx}") == 0) return RDX;
    if (strcmp(constraint, "{esi}") == 0 ||
        strcmp(constraint, "{rsi}") == 0) return RSI;
    if (strcmp(constraint, "{edi}") == 0 ||
        strcmp(constraint, "{rdi}") == 0) return RDI;
    switch (*constraint) {
        case 'a': return RAX;
        case 'b': return RBX;
        case 'c': return RCX;
        case 'd': return RDX;
        case 'S': return RSI;
        case 'D': return RDI;
        case 'r':
        case 'X': return R10;
        default: return -1;
    }
}

static bool codegen64_asm_no_operands(const char* text, size_t length,
                                       const char* mnemonic)
{
    size_t mnemonic_length = strlen(mnemonic);
    return length == mnemonic_length &&
           strncmp(text, mnemonic, mnemonic_length) == 0;
}

static bool codegen64_asm_parse_int(const char* text, size_t length,
                                    uint8_t* immediate_out)
{
    char operand[64];
    char* end;
    long long parsed;
    size_t cursor = 3u;
    size_t operand_length;

    if (length < 4u || strncmp(text, "int", 3u) != 0 ||
        (text[3] != ' ' && text[3] != '\t')) return false;
    while (cursor < length && (text[cursor] == ' ' || text[cursor] == '\t')) {
        ++cursor;
    }
    operand_length = length - cursor;
    while (operand_length > 0u &&
           (text[cursor + operand_length - 1u] == ' ' ||
            text[cursor + operand_length - 1u] == '\t')) {
        --operand_length;
    }
    if (operand_length < 2u || operand_length >= sizeof(operand)) return false;
    memcpy(operand, text + cursor, operand_length);
    operand[operand_length] = '\0';
    if (operand[0] != '$' || !operand[1]) return false;
    parsed = strtoll(operand + 1, &end, 0);
    if (end == operand + 1 || *end != '\0' || parsed < -128 ||
        parsed > 255) return false;
    *immediate_out = (uint8_t)parsed;
    return true;
}

static bool codegen64_emit_asm_instruction(Module* mod, const char* text,
                                           size_t length)
{
    uint8_t immediate;

    while (length > 0u && (*text == ' ' || *text == '\t' ||
                           *text == '\r')) {
        ++text;
        --length;
    }
    while (length > 0u && (text[length - 1u] == ' ' ||
                           text[length - 1u] == '\t' ||
                           text[length - 1u] == '\r')) {
        --length;
    }
    if (length == 0u) return true;
    if (codegen64_asm_no_operands(text, length, "syscall")) {
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0x05);
        return true;
    }
    if (codegen64_asm_parse_int(text, length, &immediate)) {
        emit_byte(mod, 0xCD);
        emit_byte(mod, immediate);
        return true;
    }
    if (codegen64_asm_no_operands(text, length, "nop")) {
        emit_byte(mod, 0x90);
        return true;
    }
    if (codegen64_asm_no_operands(text, length, "pause")) {
        emit_byte(mod, 0xF3);
        emit_byte(mod, 0x90);
        return true;
    }
    if (codegen64_asm_no_operands(text, length, "cpuid")) {
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xA2);
        return true;
    }
    if (codegen64_asm_no_operands(text, length, "rdtsc")) {
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0x31);
        return true;
    }
    if (codegen64_asm_no_operands(text, length, "int3")) {
        emit_byte(mod, 0xCC);
        return true;
    }
    if (codegen64_asm_no_operands(text, length, "cli")) {
        emit_byte(mod, 0xFA);
        return true;
    }
    if (codegen64_asm_no_operands(text, length, "sti")) {
        emit_byte(mod, 0xFB);
        return true;
    }
    if (codegen64_asm_no_operands(text, length, "cld")) {
        emit_byte(mod, 0xFC);
        return true;
    }
    if (codegen64_asm_no_operands(text, length, "std")) {
        emit_byte(mod, 0xFD);
        return true;
    }
    if (codegen64_asm_no_operands(text, length, "hlt")) {
        emit_byte(mod, 0xF4);
        return true;
    }
    if (codegen64_asm_no_operands(text, length, "ret")) {
        emit_byte(mod, 0xC3);
        return true;
    }
    if (codegen64_asm_no_operands(text, length, "leave")) {
        emit_byte(mod, 0xC9);
        return true;
    }
    if (codegen64_asm_no_operands(text, length, "mfence")) {
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xAE);
        emit_byte(mod, 0xF0);
        return true;
    }
    if (codegen64_asm_no_operands(text, length, "lfence")) {
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xAE);
        emit_byte(mod, 0xE8);
        return true;
    }
    if (codegen64_asm_no_operands(text, length, "sfence")) {
        emit_byte(mod, 0x0F);
        emit_byte(mod, 0xAE);
        emit_byte(mod, 0xF8);
        return true;
    }
    return false;
}

static void gen64_asm_stmt(Module* mod, Stmt* stmt)
{
    AsmOperand* operand;
    AsmOperand** inputs;
    int* registers;
    int input_count = 0;
    int input_index = 0;
    bool preserve_rbx = false;
    const char* cursor;

    for (operand = stmt->asm_outputs; operand; operand = operand->next) {
        int reg = codegen64_asm_register(operand->constraint);
        if (reg != RAX) {
            rcc_error(stmt->loc,
                      "AMD64 inline asm currently requires '=a' outputs");
            return;
        }
        if (operand->constraint[0] == '+') ++input_count;
    }
    for (operand = stmt->asm_inputs; operand; operand = operand->next) {
        ++input_count;
    }
    inputs = input_count > 0
        ? rcc_alloc((size_t)input_count * sizeof(*inputs)) : NULL;
    registers = input_count > 0
        ? rcc_alloc((size_t)input_count * sizeof(*registers)) : NULL;
    for (operand = stmt->asm_outputs; operand; operand = operand->next) {
        if (operand->constraint[0] == '+') {
            inputs[input_index] = operand;
            registers[input_index++] = codegen64_asm_register(
                operand->constraint);
        }
    }
    for (operand = stmt->asm_inputs; operand; operand = operand->next) {
        int reg = codegen64_asm_register(operand->constraint);
        if (reg < 0) {
            rcc_error(stmt->loc,
                      "unsupported AMD64 inline asm constraint '%s'",
                      operand->constraint);
            rcc_free(inputs);
            rcc_free(registers);
            return;
        }
        inputs[input_index] = operand;
        registers[input_index++] = reg;
        preserve_rbx = preserve_rbx || reg == RBX;
    }
    for (AsmClobber* clobber = stmt->asm_clobbers; clobber;
         clobber = clobber->next) {
        preserve_rbx = preserve_rbx ||
            strcmp(clobber->reg, "rbx") == 0 ||
            strcmp(clobber->reg, "ebx") == 0 ||
            strcmp(clobber->reg, "bx") == 0;
    }

    if (preserve_rbx) emit64_push_reg(mod, RBX);
    for (int index = 0; index < input_count; ++index) {
        gen64_expr(mod, inputs[index]->expr);
        emit64_push_reg(mod, RAX);
    }
    for (int index = input_count - 1; index >= 0; --index) {
        emit64_pop_reg(mod, registers[index]);
    }

    cursor = stmt->asm_template ? stmt->asm_template : "";
    while (*cursor) {
        const char* begin;
        size_t length;
        while (*cursor == ';' || *cursor == '\n') ++cursor;
        begin = cursor;
        while (*cursor && *cursor != ';' && *cursor != '\n') ++cursor;
        length = (size_t)(cursor - begin);
        if (!codegen64_emit_asm_instruction(mod, begin, length)) {
            rcc_error(stmt->loc, "unsupported AMD64 inline asm instruction");
            break;
        }
    }

    if (preserve_rbx) emit64_pop_reg(mod, RBX);
    for (operand = stmt->asm_outputs; operand; operand = operand->next) {
        emit64_push_reg(mod, RAX);
        gen64_lvalue(mod, operand->expr);
        emit64_mov_reg_reg(mod, RCX, RAX);
        emit64_pop_reg(mod, RAX);
        emit64_store_typed(mod, RCX, 0, RAX, operand->expr->type);
    }
    rcc_free(inputs);
    rcc_free(registers);
}

static void gen64_stmt(Module* mod, Stmt* stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case STMT_EXPR:
            if (stmt->expr) {
                gen64_expr(mod, stmt->expr);
            }
            break;

        case STMT_BLOCK: {
            CleanupCodegen64* marker = active_cleanups64;
            VLAScopeCodegen64* vla_marker = active_vla_scopes64;
            if (stmt->vla_stack_offset < 0) {
                emit64_mov_mem_reg(mod, RBP, stmt->vla_stack_offset, RSP);
            }
            for (StmtList* s = stmt->block_stmts; s; s = s->next) {
                gen64_stmt(mod, s->stmt);
            }
            gen64_cleanups_until(mod, marker);
            discard64_cleanups_until(marker);
            gen64_vla_scopes_until(mod, vla_marker);
            discard64_vla_scopes_until(vla_marker);
            if (stmt->vla_stack_offset < 0) {
                emit64_mov_reg_mem(mod, RSP, RBP, stmt->vla_stack_offset);
            }
            break;
        }

        case STMT_IF: {
            int else_label = new_label64();
            int end_label = new_label64();

            gen64_expr(mod, stmt->if_cond);
            emit64_test_reg_reg(mod, RAX, RAX);
            emit64_jcc_label(mod, CC64_E, else_label);

            gen64_scoped_stmt(mod, stmt->if_then);

            if (stmt->if_else) {
                emit64_jmp_label(mod, end_label);
            }

            emit64_label(mod, else_label);

            if (stmt->if_else) {
                gen64_scoped_stmt(mod, stmt->if_else);
                emit64_label(mod, end_label);
            }
            break;
        }

        case STMT_WHILE: {
            CleanupCodegen64* loop_marker = active_cleanups64;
            VLAScopeCodegen64* vla_loop_marker = active_vla_scopes64;
            CleanupCodegen64* old_break_cleanup = break_cleanup_marker64;
            CleanupCodegen64* old_continue_cleanup =
                continue_cleanup_marker64;
            VLAScopeCodegen64* old_break_vla = break_vla_marker64;
            VLAScopeCodegen64* old_continue_vla = continue_vla_marker64;
            int start_label = new_label64();
            int end_label = new_label64();
            int old_break = break_label64;
            int old_continue = continue_label64;
            break_label64 = end_label;
            continue_label64 = start_label;
            break_cleanup_marker64 = loop_marker;
            continue_cleanup_marker64 = loop_marker;
            break_vla_marker64 = vla_loop_marker;
            continue_vla_marker64 = vla_loop_marker;

            emit64_label(mod, start_label);
            gen64_expr(mod, stmt->while_cond);
            emit64_test_reg_reg(mod, RAX, RAX);
            emit64_jcc_label(mod, CC64_E, end_label);

            gen64_scoped_stmt(mod, stmt->while_body);

            emit64_jmp_label(mod, start_label);
            emit64_label(mod, end_label);

            break_label64 = old_break;
            continue_label64 = old_continue;
            break_cleanup_marker64 = old_break_cleanup;
            continue_cleanup_marker64 = old_continue_cleanup;
            break_vla_marker64 = old_break_vla;
            continue_vla_marker64 = old_continue_vla;
            break;
        }

        case STMT_DO: {
            CleanupCodegen64* loop_marker = active_cleanups64;
            VLAScopeCodegen64* vla_loop_marker = active_vla_scopes64;
            CleanupCodegen64* old_break_cleanup = break_cleanup_marker64;
            CleanupCodegen64* old_continue_cleanup =
                continue_cleanup_marker64;
            VLAScopeCodegen64* old_break_vla = break_vla_marker64;
            VLAScopeCodegen64* old_continue_vla = continue_vla_marker64;
            int start_label = new_label64();
            int end_label = new_label64();
            int cond_label = new_label64();
            int old_break = break_label64;
            int old_continue = continue_label64;
            break_label64 = end_label;
            continue_label64 = cond_label;
            break_cleanup_marker64 = loop_marker;
            continue_cleanup_marker64 = loop_marker;
            break_vla_marker64 = vla_loop_marker;
            continue_vla_marker64 = vla_loop_marker;

            emit64_label(mod, start_label);
            gen64_scoped_stmt(mod, stmt->while_body);

            emit64_label(mod, cond_label);
            gen64_expr(mod, stmt->while_cond);
            emit64_test_reg_reg(mod, RAX, RAX);
            emit64_jcc_label(mod, CC64_NE, start_label);

            emit64_label(mod, end_label);

            break_label64 = old_break;
            continue_label64 = old_continue;
            break_cleanup_marker64 = old_break_cleanup;
            continue_cleanup_marker64 = old_continue_cleanup;
            break_vla_marker64 = old_break_vla;
            continue_vla_marker64 = old_continue_vla;
            break;
        }

        case STMT_FOR: {
            CleanupCodegen64* marker = active_cleanups64;
            VLAScopeCodegen64* vla_marker = active_vla_scopes64;
            CleanupCodegen64* old_break_cleanup = break_cleanup_marker64;
            CleanupCodegen64* old_continue_cleanup =
                continue_cleanup_marker64;
            VLAScopeCodegen64* old_break_vla = break_vla_marker64;
            VLAScopeCodegen64* old_continue_vla = continue_vla_marker64;
            int start_label = new_label64();
            int end_label = new_label64();
            int inc_label = new_label64();
            int old_break = break_label64;
            int old_continue = continue_label64;
            break_label64 = end_label;
            continue_label64 = inc_label;

            if (stmt->vla_stack_offset < 0) {
                emit64_mov_mem_reg(mod, RBP, stmt->vla_stack_offset, RSP);
            }
            if (stmt->for_init) {
                gen64_stmt(mod, stmt->for_init);
            }
            break_cleanup_marker64 = active_cleanups64;
            continue_cleanup_marker64 = active_cleanups64;
            break_vla_marker64 = vla_marker;
            continue_vla_marker64 = active_vla_scopes64;

            emit64_label(mod, start_label);

            if (stmt->for_cond) {
                gen64_expr(mod, stmt->for_cond);
                emit64_test_reg_reg(mod, RAX, RAX);
                emit64_jcc_label(mod, CC64_E, end_label);
            }

            gen64_scoped_stmt(mod, stmt->for_body);

            emit64_label(mod, inc_label);
            if (stmt->for_inc) {
                gen64_expr(mod, stmt->for_inc);
            }

            emit64_jmp_label(mod, start_label);
            emit64_label(mod, end_label);
            gen64_cleanups_until(mod, marker);
            discard64_cleanups_until(marker);
            gen64_vla_scopes_until(mod, vla_marker);
            discard64_vla_scopes_until(vla_marker);
            if (stmt->vla_stack_offset < 0) {
                emit64_mov_reg_mem(mod, RSP, RBP, stmt->vla_stack_offset);
            }

            break_label64 = old_break;
            continue_label64 = old_continue;
            break_cleanup_marker64 = old_break_cleanup;
            continue_cleanup_marker64 = old_continue_cleanup;
            break_vla_marker64 = old_break_vla;
            continue_vla_marker64 = old_continue_vla;
            break;
        }

        case STMT_SWITCH: {
            SwitchCodegenContext64 context = {0};
            SwitchCodegenContext64* old_switch = current_switch_codegen64;
            CleanupCodegen64* switch_marker = active_cleanups64;
            VLAScopeCodegen64* vla_switch_marker = active_vla_scopes64;
            CleanupCodegen64* old_break_cleanup = break_cleanup_marker64;
            VLAScopeCodegen64* old_break_vla = break_vla_marker64;
            int old_break = break_label64;
            int end_label = new_label64();
            context.control_type = codegen64_switch_control_type(
                stmt->switch_expr ? stmt->switch_expr->type : NULL);
            context.default_label = -1;
            context.previous = old_switch;
            codegen64_collect_switch_cases(stmt->switch_body, &context);

            gen64_expr(mod, stmt->switch_expr);
            for (SwitchCaseCodegen64* item = context.cases; item;
                 item = item->next) {
                uint64_t signed_imm32 = (uint64_t)(int64_t)(int32_t)item->bits;
                if (item->bits == signed_imm32) {
                    emit64_cmp_reg_imm(mod, RAX, (int32_t)item->bits);
                } else {
                    emit64_mov_reg_imm64(mod, RCX, item->bits);
                    emit64_cmp_reg_reg(mod, RAX, RCX);
                }
                emit64_jcc_label(mod, CC64_E, item->label);
            }
            emit64_jmp_label(mod, context.default_label >= 0
                ? context.default_label : end_label);

            break_label64 = end_label;
            break_cleanup_marker64 = switch_marker;
            break_vla_marker64 = vla_switch_marker;
            current_switch_codegen64 = &context;
            gen64_stmt(mod, stmt->switch_body);
            current_switch_codegen64 = old_switch;
            break_label64 = old_break;
            break_cleanup_marker64 = old_break_cleanup;
            break_vla_marker64 = old_break_vla;
            emit64_label(mod, end_label);
            codegen64_release_switch_cases(context.cases);
            break;
        }

        case STMT_CASE: {
            SwitchCaseCodegen64* item = codegen64_find_switch_case(
                current_switch_codegen64, stmt);
            if (item) emit64_label(mod, item->label);
            gen64_stmt(mod, stmt->case_stmt);
            break;
        }

        case STMT_DEFAULT:
            if (current_switch_codegen64 &&
                current_switch_codegen64->default_statement == stmt) {
                emit64_label(mod, current_switch_codegen64->default_label);
            }
            gen64_stmt(mod, stmt->default_stmt);
            break;

        case STMT_GOTO:
            if (!gen64_cleanup_count(mod, stmt->goto_cleanup_count)) {
                rcc_error(stmt->loc, "invalid C++ goto cleanup path");
                break;
            }
            if (!gen64_vla_count(mod, stmt->goto_vla_count)) {
                rcc_error(stmt->loc, "invalid VLA goto path");
                break;
            }
            emit64_jmp_label(mod, codegen64_named_label(stmt->goto_label));
            break;

        case STMT_LABEL:
            emit64_label(mod, codegen64_named_label(stmt->label_name));
            gen64_stmt(mod, stmt->label_stmt);
            break;

        case STMT_RETURN:
            if (stmt->return_val) {
                if (current_function_return_type64 &&
                    (current_function_return_type64->kind == TYPE_STRUCT ||
                     current_function_return_type64->kind == TYPE_UNION)) {
                    int size = current_function_return_type64->size;
                    Gen64AggregateClass return_class =
                        gen64_classify_aggregate(
                            current_function_return_type64);
                    if (stmt->return_val->kind == EXPR_VA_ARG) {
                        gen64_expr(mod, stmt->return_val);
                    } else {
                        gen64_lvalue(mod, stmt->return_val);
                    }
                    emit64_mov_reg_reg(mod, RCX, RAX);
                    if (return_class.memory) {
                        int offset = 0;
                        emit64_mov_reg_mem(mod, R11, RBP,
                                           current_function_sret_offset64);
                        while (offset + 8 <= size) {
                            emit64_mov_reg_mem(mod, RAX, RCX, offset);
                            emit64_mov_mem_reg(mod, R11, offset, RAX);
                            offset += 8;
                        }
                        while (offset < size) {
                            emit64_load_typed(mod, RAX, RCX, offset,
                                              type_uchar);
                            emit64_store_typed(mod, R11, offset, RAX,
                                               type_uchar);
                            ++offset;
                        }
                        emit64_mov_reg_reg(mod, RAX, R11);
                    } else {
                        int gp_return = 0;
                        int fp_return = 0;
                        for (int index = 0; index < return_class.count;
                             ++index) {
                            if (return_class.classes[index] ==
                                GEN64_CLASS_INTEGER) {
                                emit64_mov_reg_mem(
                                    mod,
                                    gp_return++ == 0 ? RAX : RDX,
                                    RCX, index * 8);
                            } else {
                                emit64_mov_xmm_from_memory(
                                    mod, fp_return++, RCX, index * 8, 8);
                            }
                        }
                    }
                } else {
                    gen64_expr(mod, stmt->return_val);
                }
                if (type_is_integer(current_function_return_type64) ||
                    (current_function_return_type64 &&
                     current_function_return_type64->kind == TYPE_ENUM)) {
                    emit64_normalize_atomic_value(
                        mod, RAX, current_function_return_type64);
                }
            }
            if (active_cleanups64) {
                if (stmt->return_val) {
                    emit64_push_reg(mod, RAX);
                    emit64_push_reg(mod, RDX);
                }
                gen64_cleanups_until(mod, NULL);
                if (stmt->return_val) {
                    emit64_pop_reg(mod, RDX);
                    emit64_pop_reg(mod, RAX);
                }
            }
            gen64_vla_scopes_until(mod, NULL);
            if (stmt->return_val &&
                gen64_is_floating(current_function_return_type64)) {
                emit64_mov_xmm_from_gpr(
                    mod, 0, RAX,
                    gen64_float_width(current_function_return_type64));
            }
            emit64_leave(mod);
            emit64_ret(mod);
            break;

        case STMT_BREAK:
            if (break_label64 >= 0) {
                gen64_cleanups_until(mod, break_cleanup_marker64);
                gen64_vla_scopes_until(mod, break_vla_marker64);
                emit64_jmp_label(mod, break_label64);
            }
            break;

        case STMT_CONTINUE:
            if (continue_label64 >= 0) {
                gen64_cleanups_until(mod, continue_cleanup_marker64);
                gen64_vla_scopes_until(mod, continue_vla_marker64);
                emit64_jmp_label(mod, continue_label64);
            }
            break;

        case STMT_DECL: {
            Decl* d = stmt->decl;
            if (d->kind == DECL_VAR && d->var_is_vla) {
                gen64_vla_alloc(mod, d);
                record64_vla_scope(d);
            } else if (d->kind == DECL_VAR &&
                       (d->var_init ||
                        (d->type && d->type->cxx_vtable_size > 0))) {
                if (d->type && (d->type->kind == TYPE_ARRAY ||
                                d->type->kind == TYPE_STRUCT ||
                                d->type->kind == TYPE_UNION)) {
                    gen64_zero_local_storage(mod, d->var_offset,
                                             (size_t)d->type->size);
                }
                if (d->var_init &&
                    !gen64_local_initializer(mod, d->type, d->var_init,
                                             d->var_offset)) {
                    rcc_error(d->loc, "unsupported local initializer for '%s'",
                              d->name);
                }
                gen64_local_vtable_init(mod, d->type, d->var_offset);
            }
            if (d->kind == DECL_VAR && d->var_cleanup) {
                CleanupCodegen64* cleanup = rcc_alloc(sizeof(*cleanup));
                cleanup->expression = d->var_cleanup;
                cleanup->previous = active_cleanups64;
                active_cleanups64 = cleanup;
            }
            break;
        }

        case STMT_NULL:
            break;

        case STMT_ASM:
            gen64_asm_stmt(mod, stmt);
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
    Type* return_type = decl->type && decl->type->kind == TYPE_FUNC
        ? decl->type->ret_type : NULL;
    bool aggregate_return = return_type && gen64_is_aggregate(return_type);
    Gen64AggregateClass return_class;
    if (aggregate_return) {
        return_class = gen64_classify_aggregate(return_type);
    } else {
        return_class = gen64_empty_aggregate_class();
    }
    bool memory_result = aggregate_return && return_class.memory;
    bool variadic = decl->type && decl->type->kind == TYPE_FUNC &&
                    decl->type->variadic;
    int64_t parameter_frame_size = (memory_result ? 8 : 0) +
                                   (variadic ? 176 : 0);
    int va_reg_save_offset = variadic ? -(int)parameter_frame_size : 0;
    int register_cursor = memory_result ? 1 : 0;
    int float_register_cursor = 0;
    int stack_cursor = 16; /* saved RBP + return address */
    int stack_size;
    Type* old_return_type;
    CleanupCodegen64* old_cleanups;
    VLAScopeCodegen64* old_vla_scopes;
    VLAScopeCodegen64* old_break_vla;
    VLAScopeCodegen64* old_continue_vla;
    int old_sret_offset;
    bool old_variadic;
    int old_va_gp_offset;
    int old_va_fp_offset;
    int old_va_overflow_offset;
    int old_va_reg_save_offset;
    DeclList implicit_this_parameter;
    DeclList* all_parameters = decl->func_params;
    if (!decl->func_body) return;

    if (decl->func_this_param) {
        implicit_this_parameter.decl = decl->func_this_param;
        implicit_this_parameter.next = decl->func_params;
        all_parameters = &implicit_this_parameter;
    }

    for (DeclList* parameter = all_parameters; parameter;
         parameter = parameter->next) {
        Decl* value = parameter->decl;
        int vla_dimensions = value->param_array_type &&
            gen64_type_has_vla(value->param_array_type)
            ? gen64_vla_dimension_count(value->param_array_type) : 0;
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
        if (vla_dimensions > 0) {
            storage += (int64_t)vla_dimensions * 8;
        }
        parameter_frame_size += storage;
        if (parameter_frame_size > INT_MAX) {
            rcc_error(decl->loc, "function stack frame exceeds compiler limits");
            return;
        }
        value->var_offset = -(int)parameter_frame_size;
        if (vla_dimensions > 0) {
            value->var_vla_extent_offset = value->var_offset + 8;
            value->var_vla_extent_count = vla_dimensions;
        }
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
    stack_size = codegen_assign_compound_storage(decl->func_body, stack_size,
                                                 8);
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
    if (memory_result) {
        emit64_mov_mem_reg(mod, RBP, -8, RDI);
    }
    if (variadic) {
        for (int index = 0; index < 6; ++index) {
            emit64_mov_mem_reg(mod, RBP, va_reg_save_offset + index * 8,
                               argument_registers[index]);
        }
        for (int index = 0; index < 8; ++index) {
            /* Scalar C variadic arguments use only the low 32/64 bits of
             * each XMM register. Keep the ABI-mandated 16-byte slot spacing
             * so va_arg can advance fp_offset independently of width. */
            emit64_mov_memory_from_xmm(
                mod, RBP, va_reg_save_offset + 48 + index * 16,
                index, 8);
        }
    }

    /* Sema reserves parameter storage as part of the function frame. Rebuild
     * the full SysV classification and spill every incoming argument before
     * the body so ordinary identifier/member code remains independent of
     * caller-saved registers. */
    register_cursor = memory_result ? 1 : 0;
    float_register_cursor = 0;
    stack_cursor = 16;
    for (DeclList* parameter = all_parameters; parameter;
         parameter = parameter->next) {
        Decl* value = parameter->decl;
        int size = value->type && value->type->size > 0
            ? value->type->size : 8;
        bool aggregate = gen64_is_aggregate(value->type);
        Gen64AggregateClass classification;
        if (aggregate) {
            classification = gen64_classify_aggregate(value->type);
        } else {
            classification = gen64_empty_aggregate_class();
        }
        int gp_count = 0;
        int fp_count = 0;
        bool memory_argument = false;
        if (aggregate && !classification.memory) {
            for (int index = 0; index < classification.count; ++index) {
                if (classification.classes[index] == GEN64_CLASS_INTEGER) {
                    ++gp_count;
                } else if (classification.classes[index] ==
                           GEN64_CLASS_SSE) {
                    ++fp_count;
                }
            }
            memory_argument = register_cursor + gp_count > 6 ||
                              float_register_cursor + fp_count > 8;
        } else if (aggregate) {
            memory_argument = true;
        } else if (value->type && gen64_is_floating(value->type)) {
            memory_argument = float_register_cursor >= 8;
        } else {
            memory_argument = register_cursor >= 6;
        }
        if (!memory_argument && value->type && gen64_is_floating(value->type)) {
            int width = gen64_float_width(value->type);
            emit64_mov_memory_from_xmm(
                mod, RBP, value->var_offset,
                float_register_cursor++, width);
            continue;
        }
        if (!memory_argument && aggregate) {
            for (int index = 0; index < classification.count; ++index) {
                if (classification.classes[index] == GEN64_CLASS_INTEGER) {
                    emit64_mov_mem_reg(
                        mod, RBP, value->var_offset + index * 8,
                        argument_registers[register_cursor++]);
                } else {
                    emit64_mov_gpr_from_xmm(
                        mod, RAX, float_register_cursor++, 8);
                    emit64_mov_mem_reg(
                        mod, RBP, value->var_offset + index * 8, RAX);
                }
            }
        } else if (!memory_argument) {
            emit64_store_typed(mod, RBP, value->var_offset,
                               argument_registers[register_cursor++],
                               value->type);
        } else {
            int storage = aggregate ? gen64_aggregate_storage(value->type)
                                     : (size + 7) & ~7;
            if (aggregate) {
                gen64_copy_memory(mod, RBP, value->var_offset,
                                  RBP, stack_cursor, size);
            } else if (value->type && gen64_is_floating(value->type)) {
                emit64_mov_xmm_from_memory(
                    mod, 0, RBP, stack_cursor,
                    gen64_float_width(value->type));
                emit64_mov_memory_from_xmm(
                    mod, RBP, value->var_offset, 0,
                    gen64_float_width(value->type));
            } else {
                emit64_load_typed(mod, RAX, RBP, stack_cursor, value->type);
                emit64_store_typed(mod, RBP, value->var_offset, RAX,
                                   value->type);
            }
            stack_cursor += storage;
        }
    }

    gen64_vla_parameter_extents(mod, decl);

    /* Generate body */
    old_return_type = current_function_return_type64;
    old_sret_offset = current_function_sret_offset64;
    old_variadic = current_function_variadic64;
    old_va_gp_offset = current_function_va_gp_offset64;
    old_va_fp_offset = current_function_va_fp_offset64;
    old_va_overflow_offset = current_function_va_overflow_offset64;
    old_va_reg_save_offset = current_function_va_reg_save_offset64;
    current_function_sret_offset64 = memory_result ? -8 : 0;
    current_function_return_type64 = return_type;
    current_function_variadic64 = variadic;
    current_function_va_gp_offset64 = register_cursor * 8;
    if (current_function_va_gp_offset64 > 48) {
        current_function_va_gp_offset64 = 48;
    }
    current_function_va_fp_offset64 = 48 + float_register_cursor * 16;
    if (current_function_va_fp_offset64 > 176) {
        current_function_va_fp_offset64 = 176;
    }
    current_function_va_overflow_offset64 = stack_cursor;
    current_function_va_reg_save_offset64 = va_reg_save_offset;
    old_cleanups = active_cleanups64;
    old_vla_scopes = active_vla_scopes64;
    old_break_vla = break_vla_marker64;
    old_continue_vla = continue_vla_marker64;
    active_cleanups64 = NULL;
    active_vla_scopes64 = NULL;
    break_vla_marker64 = NULL;
    continue_vla_marker64 = NULL;
    named_codegen_labels64 = NULL;
    gen64_stmt(mod, decl->func_body);
    discard64_cleanups_until(NULL);
    discard64_vla_scopes_until(NULL);
    active_cleanups64 = old_cleanups;
    active_vla_scopes64 = old_vla_scopes;
    break_vla_marker64 = old_break_vla;
    continue_vla_marker64 = old_continue_vla;
    codegen64_release_named_labels();
    current_function_return_type64 = old_return_type;
    current_function_sret_offset64 = old_sret_offset;
    current_function_variadic64 = old_variadic;
    current_function_va_gp_offset64 = old_va_gp_offset;
    current_function_va_fp_offset64 = old_va_fp_offset;
    current_function_va_overflow_offset64 = old_va_overflow_offset;
    current_function_va_reg_save_offset64 = old_va_reg_save_offset;

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
            module_add_symbol(mod, decl_link_name(d->decl), 0, false,
                              MODULE_SYMBOL_CODE, true);
        }
    }

    /* Second pass: generate code for each defined function */
    for (DeclList* d = ast->decls; d; d = d->next) {
        if (d->decl->kind == DECL_FUNC && d->decl->func_body) {
            /* Record function start offset */
            uint32_t func_start = code_offset(mod);

            add_func_def64(decl_link_name(d->decl), func_start);

            if (strcmp(d->decl->name, "main") == 0) {
                mod->entry_point = func_start;
            }
            gen64_function(mod, d->decl);

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

    codegen_emit_cxx_vtables(mod);

    resolve_func_calls64(mod);

    /* Resolve label references */
    resolve_labels64(mod);

    return mod;
}

#endif /* 64-bit code */
