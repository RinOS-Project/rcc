/*
 * RCC - verified i686/AMD64 legal-IR byte encoding
 */

#include "rcc.h"
#include "x86_encode.h"

#include <stdarg.h>

typedef struct {
    uint32_t offset;
    uint32_t target;
} RccX86BranchFixup;

typedef struct {
    const RccX86LegalFunction* function;
    RccX86EncodedFunction output;
    size_t code_capacity;
    RccX86BranchFixup* fixups;
    size_t fixup_count;
    size_t fixup_capacity;
    size_t relocation_capacity;
    char* error;
    size_t error_size;
} RccX86Encoder;

static bool x86_emit_compare_zero(RccX86Encoder* encoder,
                                  RccX86Value value, uint16_t size);

static bool x86_encode_error(RccX86Encoder* encoder,
                             const char* format, ...) {
    if (encoder->error && encoder->error_size != 0u) {
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(encoder->error, encoder->error_size, format, arguments);
        va_end(arguments);
    }
    return false;
}

static bool x86_encode_reserve(RccX86Encoder* encoder, size_t extra) {
    size_t required;
    size_t capacity;
    if (extra > UINT32_MAX || encoder->output.code_size >
            UINT32_MAX - extra) {
        return x86_encode_error(encoder,
                                "x86 encoded function exceeds 32 bits");
    }
    required = encoder->output.code_size + extra;
    if (required <= encoder->code_capacity) return true;
    capacity = encoder->code_capacity == 0u ? 128u
                                            : encoder->code_capacity;
    while (capacity < required) {
        size_t next = capacity * 2u;
        if (next < capacity || next > UINT32_MAX) {
            capacity = required;
            break;
        }
        capacity = next;
    }
    encoder->output.code = rcc_realloc(
        encoder->output.code, capacity);
    encoder->code_capacity = capacity;
    return true;
}

static bool x86_emit_u8(RccX86Encoder* encoder, uint8_t value) {
    if (!x86_encode_reserve(encoder, 1u)) return false;
    encoder->output.code[encoder->output.code_size++] = value;
    return true;
}

static bool x86_emit_u16(RccX86Encoder* encoder, uint16_t value) {
    uint8_t bytes[2];
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8u);
    if (!x86_encode_reserve(encoder, sizeof(bytes))) return false;
    memcpy(encoder->output.code + encoder->output.code_size,
           bytes, sizeof(bytes));
    encoder->output.code_size += sizeof(bytes);
    return true;
}

static bool x86_emit_u32(RccX86Encoder* encoder, uint32_t value) {
    uint8_t bytes[4];
    for (size_t index = 0u; index < sizeof(bytes); ++index) {
        bytes[index] = (uint8_t)(value >> (index * 8u));
    }
    if (!x86_encode_reserve(encoder, sizeof(bytes))) return false;
    memcpy(encoder->output.code + encoder->output.code_size,
           bytes, sizeof(bytes));
    encoder->output.code_size += sizeof(bytes);
    return true;
}

static bool x86_emit_u64(RccX86Encoder* encoder, uint64_t value) {
    uint8_t bytes[8];
    for (size_t index = 0u; index < sizeof(bytes); ++index) {
        bytes[index] = (uint8_t)(value >> (index * 8u));
    }
    if (!x86_encode_reserve(encoder, sizeof(bytes))) return false;
    memcpy(encoder->output.code + encoder->output.code_size,
           bytes, sizeof(bytes));
    encoder->output.code_size += sizeof(bytes);
    return true;
}

static bool x86_emit_rex(RccX86Encoder* encoder, bool wide,
                         RccX86HardwareGpr reg,
                         RccX86HardwareGpr rm, bool force) {
    uint8_t rex;
    if (encoder->function->target != RCC_X86_TARGET_X86_64) {
        return (unsigned)reg < 8u && (unsigned)rm < 8u;
    }
    rex = (uint8_t)(0x40u | (wide ? 0x08u : 0u) |
                    (((unsigned)reg >= 8u) ? 0x04u : 0u) |
                    (((unsigned)rm >= 8u) ? 0x01u : 0u));
    if (rex != 0x40u || force) return x86_emit_u8(encoder, rex);
    return true;
}

static bool x86_emit_prefix(RccX86Encoder* encoder, uint16_t size,
                            RccX86HardwareGpr reg,
                            RccX86HardwareGpr rm,
                            bool byte_register) {
    bool force_rex = byte_register &&
        (((unsigned)reg & 7u) >= 4u || ((unsigned)rm & 7u) >= 4u);
    if (size == 2u && !x86_emit_u8(encoder, 0x66u)) return false;
    return x86_emit_rex(encoder, size == 8u, reg, rm, force_rex);
}

static uint8_t x86_modrm(uint8_t mode, unsigned reg, unsigned rm) {
    return (uint8_t)((mode << 6u) | ((reg & 7u) << 3u) | (rm & 7u));
}

static bool x86_value_equal(RccX86Value left, RccX86Value right) {
    if (left.kind != right.kind) return false;
    if (left.kind == RCC_X86_VALUE_GPR) return left.gpr == right.gpr;
    return left.frame_offset == right.frame_offset &&
        left.size == right.size;
}

static bool x86_value_displacement(RccX86Encoder* encoder,
                                   RccX86Value value,
                                   int32_t* displacement) {
    uint32_t frame_offset;
    uint32_t magnitude;
    if (value.kind == RCC_X86_VALUE_INCOMING_ARGUMENT) {
        uint32_t base = (uint32_t)encoder->function->pointer_size * 2u;
        if (value.frame_offset > (uint32_t)INT32_MAX - base) {
            return x86_encode_error(
                encoder, "x86 incoming argument displacement overflows");
        }
        *displacement = (int32_t)(base + value.frame_offset);
        return true;
    }
    if (value.kind != RCC_X86_VALUE_FRAME &&
        value.kind != RCC_X86_VALUE_OUTGOING_ARGUMENT) {
        return x86_encode_error(encoder,
                                "x86 encoder expected a memory value");
    }
    if (value.kind == RCC_X86_VALUE_OUTGOING_ARGUMENT) {
        if (value.frame_offset <
                encoder->function->outgoing_stack_offset) {
            return x86_encode_error(
                encoder, "x86 outgoing argument precedes its frame");
        }
        frame_offset = value.frame_offset -
            encoder->function->outgoing_stack_offset;
    } else {
        if (value.frame_offset > UINT32_MAX -
                encoder->function->outgoing_stack_size) {
            return x86_encode_error(
                encoder, "x86 shifted frame offset overflows");
        }
        frame_offset = value.frame_offset +
            encoder->function->outgoing_stack_size;
    }
    if (frame_offset > encoder->function->stack_adjustment) {
        return x86_encode_error(encoder,
                                "x86 frame displacement is positive");
    }
    magnitude = encoder->function->stack_adjustment - frame_offset;
    if (magnitude > UINT32_C(0x80000000)) {
        return x86_encode_error(encoder,
                                "x86 frame displacement overflows");
    }
    *displacement = magnitude == UINT32_C(0x80000000)
        ? INT32_MIN : -(int32_t)magnitude;
    return true;
}

static bool x86_emit_memory_modrm(RccX86Encoder* encoder,
                                  unsigned reg, int32_t displacement) {
    if (!x86_emit_u8(encoder, x86_modrm(
            2u, reg, RCC_X86_GPR_BP))) return false;
    return x86_emit_u32(encoder, (uint32_t)displacement);
}

static bool x86_emit_push(RccX86Encoder* encoder,
                          RccX86HardwareGpr reg) {
    if (encoder->function->target == RCC_X86_TARGET_X86_64 &&
        (unsigned)reg >= 8u && !x86_emit_u8(encoder, 0x41u)) return false;
    return x86_emit_u8(encoder, (uint8_t)(0x50u + ((unsigned)reg & 7u)));
}

static bool x86_emit_pop(RccX86Encoder* encoder,
                         RccX86HardwareGpr reg) {
    if (encoder->function->target == RCC_X86_TARGET_X86_64 &&
        (unsigned)reg >= 8u && !x86_emit_u8(encoder, 0x41u)) return false;
    return x86_emit_u8(encoder, (uint8_t)(0x58u + ((unsigned)reg & 7u)));
}

static bool x86_emit_move_register_register(
    RccX86Encoder* encoder, RccX86HardwareGpr destination,
    RccX86HardwareGpr source, uint16_t size) {
    if (!x86_emit_prefix(encoder, size, source, destination,
                         size == 1u) ||
        !x86_emit_u8(encoder, size == 1u ? 0x88u : 0x89u)) return false;
    return x86_emit_u8(encoder, x86_modrm(
        3u, source, destination));
}

static bool x86_emit_load(RccX86Encoder* encoder,
                          RccX86HardwareGpr destination,
                          RccX86Value source, uint16_t size) {
    int32_t displacement = 0;
    if (!x86_value_displacement(encoder, source, &displacement) ||
        !x86_emit_prefix(encoder, size, destination, RCC_X86_GPR_BP,
                         size == 1u) ||
        !x86_emit_u8(encoder, size == 1u ? 0x8au : 0x8bu)) return false;
    return x86_emit_memory_modrm(encoder, destination, displacement);
}

static bool x86_emit_store(RccX86Encoder* encoder,
                           RccX86Value destination,
                           RccX86HardwareGpr source, uint16_t size) {
    int32_t displacement;
    if (!x86_value_displacement(encoder, destination, &displacement) ||
        !x86_emit_prefix(encoder, size, source, RCC_X86_GPR_BP,
                         size == 1u) ||
        !x86_emit_u8(encoder, size == 1u ? 0x88u : 0x89u)) return false;
    return x86_emit_memory_modrm(encoder, source, displacement);
}

static bool x86_emit_copy(RccX86Encoder* encoder,
                          RccX86Value source,
                          RccX86Value destination, uint16_t size) {
    if (size == 0u || size > encoder->function->pointer_size) {
        return x86_encode_error(encoder,
                                "x86 copy width is not native");
    }
    if (x86_value_equal(source, destination)) return true;
    if (source.kind == RCC_X86_VALUE_GPR &&
        destination.kind == RCC_X86_VALUE_GPR) {
        return x86_emit_move_register_register(
            encoder, destination.gpr, source.gpr, size);
    }
    if (source.kind != RCC_X86_VALUE_GPR &&
        destination.kind == RCC_X86_VALUE_GPR) {
        return x86_emit_load(encoder, destination.gpr, source, size);
    }
    if (source.kind == RCC_X86_VALUE_GPR) {
        return x86_emit_store(encoder, destination, source.gpr, size);
    }
    if (!x86_emit_push(encoder, RCC_X86_GPR_AX) ||
        !x86_emit_load(encoder, RCC_X86_GPR_AX, source, size) ||
        !x86_emit_store(encoder, destination, RCC_X86_GPR_AX, size) ||
        !x86_emit_pop(encoder, RCC_X86_GPR_AX)) return false;
    return true;
}

static bool x86_emit_immediate_register(
    RccX86Encoder* encoder, RccX86HardwareGpr destination,
    uint16_t size, uint64_t immediate) {
    if (size == 1u) {
        if (!x86_emit_rex(encoder, false, RCC_X86_GPR_AX,
                          destination,
                          ((unsigned)destination & 7u) >= 4u) ||
            !x86_emit_u8(encoder, (uint8_t)(
                0xb0u + ((unsigned)destination & 7u)))) return false;
        return x86_emit_u8(encoder, (uint8_t)immediate);
    }
    if (size == 2u && !x86_emit_u8(encoder, 0x66u)) return false;
    if (!x86_emit_rex(encoder, size == 8u, RCC_X86_GPR_AX,
                      destination, false) ||
        !x86_emit_u8(encoder, (uint8_t)(
            0xb8u + ((unsigned)destination & 7u)))) return false;
    if (size == 2u) return x86_emit_u16(encoder, (uint16_t)immediate);
    if (size == 4u) return x86_emit_u32(encoder, (uint32_t)immediate);
    if (size == 8u) return x86_emit_u64(encoder, immediate);
    return x86_encode_error(encoder,
                            "x86 immediate width is not native");
}

static bool x86_emit_immediate(RccX86Encoder* encoder,
                               RccX86Value destination,
                               uint16_t size, uint64_t immediate) {
    if (destination.kind == RCC_X86_VALUE_GPR) {
        return x86_emit_immediate_register(
            encoder, destination.gpr, size, immediate);
    }
    return x86_emit_push(encoder, RCC_X86_GPR_AX) &&
        x86_emit_immediate_register(
            encoder, RCC_X86_GPR_AX, size, immediate) &&
        x86_emit_store(
            encoder, destination, RCC_X86_GPR_AX, size) &&
        x86_emit_pop(encoder, RCC_X86_GPR_AX);
}

static uint8_t x86_binary_rm_reg_opcode(RccX86Opcode opcode) {
    switch (opcode) {
        case RCC_X86_ADD: return 0x01u;
        case RCC_X86_SUB: return 0x29u;
        case RCC_X86_AND: return 0x21u;
        case RCC_X86_OR: return 0x09u;
        case RCC_X86_XOR: return 0x31u;
        default: return 0u;
    }
}

static uint8_t x86_binary_reg_rm_opcode(RccX86Opcode opcode) {
    switch (opcode) {
        case RCC_X86_ADD: return 0x03u;
        case RCC_X86_SUB: return 0x2bu;
        case RCC_X86_AND: return 0x23u;
        case RCC_X86_OR: return 0x0bu;
        case RCC_X86_XOR: return 0x33u;
        default: return 0u;
    }
}

static bool x86_emit_binary_register(
    RccX86Encoder* encoder, RccX86Opcode opcode,
    RccX86HardwareGpr destination, RccX86Value source,
    uint16_t size) {
    bool multiply = opcode == RCC_X86_MUL;
    uint8_t operation;
    if (multiply && size == 1u) {
        return x86_encode_error(encoder,
                                "x86 byte multiply is not encoded yet");
    }
    if (source.kind == RCC_X86_VALUE_GPR) {
        if (multiply) {
            if (!x86_emit_prefix(encoder, size, destination,
                                 source.gpr, false) ||
                !x86_emit_u8(encoder, 0x0fu) ||
                !x86_emit_u8(encoder, 0xafu)) return false;
            return x86_emit_u8(encoder, x86_modrm(
                3u, destination, source.gpr));
        }
        operation = x86_binary_rm_reg_opcode(opcode);
        if (size == 1u) --operation;
        if (!x86_emit_prefix(encoder, size, source.gpr, destination,
                             size == 1u) ||
            !x86_emit_u8(encoder, operation)) return false;
        return x86_emit_u8(encoder, x86_modrm(
            3u, source.gpr, destination));
    }
    {
        int32_t displacement;
        if (!x86_value_displacement(encoder, source, &displacement)) {
            return false;
        }
        if (multiply) {
            if (!x86_emit_prefix(encoder, size, destination,
                                 RCC_X86_GPR_BP, false) ||
                !x86_emit_u8(encoder, 0x0fu) ||
                !x86_emit_u8(encoder, 0xafu)) return false;
        } else {
            operation = x86_binary_reg_rm_opcode(opcode);
            if (size == 1u) --operation;
            if (!x86_emit_prefix(encoder, size, destination,
                                 RCC_X86_GPR_BP, size == 1u) ||
                !x86_emit_u8(encoder, operation)) return false;
        }
        return x86_emit_memory_modrm(
            encoder, destination, displacement);
    }
}

static bool x86_emit_multiply_immediate_register(
    RccX86Encoder* encoder, RccX86HardwareGpr destination,
    uint16_t size, uint64_t immediate) {
    if (size != 4u && size != 8u) {
        return x86_encode_error(
            encoder, "x86 GEP pointer width is unsupported");
    }
    if (immediate == 1u) return true;
    if (immediate == 0u || immediate > (uint64_t)INT32_MAX) {
        return x86_encode_error(
            encoder, "x86 GEP scale does not fit signed imm32");
    }
    if (!x86_emit_prefix(
            encoder, size, destination, destination, false) ||
        !x86_emit_u8(encoder, 0x69u) ||
        !x86_emit_u8(encoder, x86_modrm(
            3u, destination, destination))) return false;
    return x86_emit_u32(encoder, (uint32_t)immediate);
}

static bool x86_emit_binary(RccX86Encoder* encoder,
                            const RccX86LegalInstruction* instruction) {
    RccX86Value destination = instruction->destination;
    RccX86Value source = instruction->operands[0];
    uint16_t size = destination.size;
    uint8_t operation;
    int32_t displacement;
    if (size == 0u || size > encoder->function->pointer_size) {
        return x86_encode_error(encoder,
                                "x86 binary width is not native");
    }
    if (destination.kind == RCC_X86_VALUE_GPR) {
        return x86_emit_binary_register(
            encoder, instruction->selected_opcode,
            destination.gpr, source, size);
    }
    if (instruction->selected_opcode != RCC_X86_MUL &&
        source.kind == RCC_X86_VALUE_GPR) {
        operation = x86_binary_rm_reg_opcode(
            instruction->selected_opcode);
        if (size == 1u) --operation;
        if (!x86_value_displacement(
                encoder, destination, &displacement) ||
            !x86_emit_prefix(encoder, size, source.gpr,
                             RCC_X86_GPR_BP, size == 1u) ||
            !x86_emit_u8(encoder, operation)) return false;
        return x86_emit_memory_modrm(
            encoder, source.gpr, displacement);
    }
    {
        RccX86HardwareGpr scratch =
            source.kind == RCC_X86_VALUE_GPR &&
            source.gpr == RCC_X86_GPR_AX
                ? RCC_X86_GPR_CX : RCC_X86_GPR_AX;
        return x86_emit_push(encoder, scratch) &&
            x86_emit_load(encoder, scratch, destination, size) &&
            x86_emit_binary_register(
                encoder, instruction->selected_opcode,
                scratch, source, size) &&
            x86_emit_store(encoder, destination, scratch, size) &&
            x86_emit_pop(encoder, scratch);
    }
}

static bool x86_emit_prepare_dividend(
    RccX86Encoder* encoder,
    const RccX86LegalInstruction* instruction) {
    uint16_t size = instruction->type.bit_width <= 8u
        ? 1u : (uint16_t)(instruction->type.bit_width / 8u);
    if (size == 1u) {
        return x86_encode_error(
            encoder, "x86 byte division is not encoded yet");
    }
    if (instruction->opcode ==
        RCC_X86_LEGAL_PREPARE_UNSIGNED_DIVIDEND) {
        RccX86Value high;
        memset(&high, 0, sizeof(high));
        high.kind = RCC_X86_VALUE_GPR;
        high.gpr = RCC_X86_GPR_DX;
        high.size = size;
        high.alignment = size;
        return x86_emit_binary_register(
            encoder, RCC_X86_XOR, RCC_X86_GPR_DX, high, size);
    }
    if (size == 2u && !x86_emit_u8(encoder, 0x66u)) return false;
    if (!x86_emit_rex(encoder, size == 8u, RCC_X86_GPR_AX,
                      RCC_X86_GPR_AX, false)) return false;
    if (size != 2u && size != 4u && size != 8u) {
        return x86_encode_error(
            encoder, "x86 signed dividend width is invalid");
    }
    return x86_emit_u8(encoder, 0x99u);
}

static bool x86_emit_divide(
    RccX86Encoder* encoder,
    const RccX86LegalInstruction* instruction) {
    RccX86Value divisor = instruction->operands[0];
    uint16_t size = instruction->type.bit_width <= 8u
        ? 1u : (uint16_t)(instruction->type.bit_width / 8u);
    unsigned extension =
        instruction->selected_opcode == RCC_X86_SDIV ||
        instruction->selected_opcode == RCC_X86_SREM ? 7u : 6u;
    if (size == 1u) {
        return x86_encode_error(
            encoder, "x86 byte division is not encoded yet");
    }
    if (divisor.kind == RCC_X86_VALUE_GPR) {
        if (!x86_emit_prefix(
                encoder, size, (RccX86HardwareGpr)extension,
                divisor.gpr, false) ||
            !x86_emit_u8(encoder, 0xf7u)) return false;
        return x86_emit_u8(encoder, x86_modrm(
            3u, extension, divisor.gpr));
    }
    {
        int32_t displacement;
        if (!x86_value_displacement(
                encoder, divisor, &displacement) ||
            !x86_emit_prefix(
                encoder, size, (RccX86HardwareGpr)extension,
                RCC_X86_GPR_BP, false) ||
            !x86_emit_u8(encoder, 0xf7u)) return false;
        return x86_emit_memory_modrm(
            encoder, extension, displacement);
    }
}

static bool x86_emit_shift(
    RccX86Encoder* encoder,
    const RccX86LegalInstruction* instruction) {
    RccX86Value destination = instruction->destination;
    uint16_t size = instruction->type.bit_width <= 8u
        ? 1u : (uint16_t)(instruction->type.bit_width / 8u);
    unsigned extension;
    switch (instruction->selected_opcode) {
        case RCC_X86_SHL: extension = 4u; break;
        case RCC_X86_SHR: extension = 5u; break;
        case RCC_X86_SAR: extension = 7u; break;
        default:
            return x86_encode_error(
                encoder, "x86 shift opcode is invalid");
    }
    if (destination.kind == RCC_X86_VALUE_GPR) {
        if (!x86_emit_prefix(
                encoder, size, (RccX86HardwareGpr)extension,
                destination.gpr, size == 1u) ||
            !x86_emit_u8(encoder, size == 1u ? 0xd2u : 0xd3u)) {
            return false;
        }
        return x86_emit_u8(encoder, x86_modrm(
            3u, extension, destination.gpr));
    }
    {
        int32_t displacement;
        if (!x86_value_displacement(
                encoder, destination, &displacement) ||
            !x86_emit_prefix(
                encoder, size, (RccX86HardwareGpr)extension,
                RCC_X86_GPR_BP, false) ||
            !x86_emit_u8(encoder, size == 1u ? 0xd2u : 0xd3u)) {
            return false;
        }
        return x86_emit_memory_modrm(
            encoder, extension, displacement);
    }
}

static RccX86HardwareGpr x86_choose_scratch(
    RccX86Value first, RccX86Value second) {
    static const RccX86HardwareGpr candidates[] = {
        RCC_X86_GPR_AX, RCC_X86_GPR_CX, RCC_X86_GPR_DX,
    };
    for (size_t index = 0u;
         index < sizeof(candidates) / sizeof(candidates[0]); ++index) {
        RccX86HardwareGpr candidate = candidates[index];
        if ((first.kind != RCC_X86_VALUE_GPR || first.gpr != candidate) &&
            (second.kind != RCC_X86_VALUE_GPR || second.gpr != candidate)) {
            return candidate;
        }
    }
    return RCC_X86_GPR_AX;
}

static uint8_t x86_setcc_opcode(RccIrIntPredicate predicate) {
    switch (predicate) {
        case RCC_IR_ICMP_EQ: return 0x94u;
        case RCC_IR_ICMP_NE: return 0x95u;
        case RCC_IR_ICMP_ULT: return 0x92u;
        case RCC_IR_ICMP_ULE: return 0x96u;
        case RCC_IR_ICMP_UGT: return 0x97u;
        case RCC_IR_ICMP_UGE: return 0x93u;
        case RCC_IR_ICMP_SLT: return 0x9cu;
        case RCC_IR_ICMP_SLE: return 0x9eu;
        case RCC_IR_ICMP_SGT: return 0x9fu;
        case RCC_IR_ICMP_SGE: return 0x9du;
    }
    return 0u;
}

static bool x86_emit_compare_values(
    RccX86Encoder* encoder, RccX86Value left,
    RccX86Value right, uint16_t size) {
    uint8_t opcode;
    if (left.kind == RCC_X86_VALUE_GPR) {
        if (right.kind == RCC_X86_VALUE_GPR) {
            opcode = size == 1u ? 0x38u : 0x39u;
            if (!x86_emit_prefix(encoder, size, right.gpr, left.gpr,
                                 size == 1u) ||
                !x86_emit_u8(encoder, opcode)) return false;
            return x86_emit_u8(encoder, x86_modrm(
                3u, right.gpr, left.gpr));
        }
        {
            int32_t displacement;
            opcode = size == 1u ? 0x3au : 0x3bu;
            if (!x86_value_displacement(encoder, right, &displacement) ||
                !x86_emit_prefix(encoder, size, left.gpr,
                                 RCC_X86_GPR_BP, size == 1u) ||
                !x86_emit_u8(encoder, opcode)) return false;
            return x86_emit_memory_modrm(
                encoder, left.gpr, displacement);
        }
    }
    if (right.kind == RCC_X86_VALUE_GPR) {
        int32_t displacement;
        opcode = size == 1u ? 0x38u : 0x39u;
        if (!x86_value_displacement(encoder, left, &displacement) ||
            !x86_emit_prefix(encoder, size, right.gpr,
                             RCC_X86_GPR_BP, size == 1u) ||
            !x86_emit_u8(encoder, opcode)) return false;
        return x86_emit_memory_modrm(
            encoder, right.gpr, displacement);
    }
    {
        RccX86HardwareGpr scratch = x86_choose_scratch(left, right);
        return x86_emit_push(encoder, scratch) &&
            x86_emit_load(encoder, scratch, left, size) &&
            x86_emit_compare_values(
                encoder,
                (RccX86Value){RCC_X86_VALUE_GPR, scratch, 0u,
                              size, size},
                right, size) &&
            x86_emit_pop(encoder, scratch);
    }
}

static bool x86_emit_setcc(RccX86Encoder* encoder,
                           RccX86Value destination,
                           RccIrIntPredicate predicate) {
    uint8_t opcode = x86_setcc_opcode(predicate);
    if (opcode == 0u) {
        return x86_encode_error(encoder,
                                "x86 integer predicate is invalid");
    }
    if (destination.kind == RCC_X86_VALUE_GPR) {
        if (!x86_emit_rex(
                encoder, false, RCC_X86_GPR_AX, destination.gpr,
                ((unsigned)destination.gpr & 7u) >= 4u) ||
            !x86_emit_u8(encoder, 0x0fu) ||
            !x86_emit_u8(encoder, opcode)) return false;
        return x86_emit_u8(encoder, x86_modrm(
            3u, 0u, destination.gpr));
    }
    {
        int32_t displacement;
        if (!x86_value_displacement(
                encoder, destination, &displacement) ||
            !x86_emit_u8(encoder, 0x0fu) ||
            !x86_emit_u8(encoder, opcode)) return false;
        return x86_emit_memory_modrm(encoder, 0u, displacement);
    }
}

static bool x86_emit_compare_set(
    RccX86Encoder* encoder,
    const RccX86LegalInstruction* instruction) {
    uint16_t size = instruction->operand_types[0].kind ==
                        RCC_MIR_TYPE_POINTER
        ? encoder->function->pointer_size
        : (instruction->operand_types[0].bit_width <= 8u
               ? 1u
               : (uint16_t)(instruction->operand_types[0].bit_width / 8u));
    return x86_emit_compare_values(
               encoder, instruction->operands[0],
               instruction->operands[1], size) &&
        x86_emit_setcc(
            encoder, instruction->destination,
            instruction->predicate);
}

static bool x86_emit_extend_register(
    RccX86Encoder* encoder, RccX86HardwareGpr destination,
    RccX86Value source, uint16_t source_size,
    uint16_t destination_size, bool sign_extend) {
    uint8_t opcode;
    int32_t displacement = 0;
    if (source_size == 4u && destination_size == 8u) {
        if (!sign_extend) {
            return source.kind == RCC_X86_VALUE_GPR
                ? x86_emit_move_register_register(
                    encoder, destination, source.gpr, 4u)
                : x86_emit_load(encoder, destination, source, 4u);
        }
        if (!x86_emit_rex(
                encoder, true, destination,
                source.kind == RCC_X86_VALUE_GPR
                    ? source.gpr : RCC_X86_GPR_BP,
                false) || !x86_emit_u8(encoder, 0x63u)) return false;
    } else if (source_size == 1u || source_size == 2u) {
        opcode = sign_extend
            ? (source_size == 1u ? 0xbeu : 0xbfu)
            : (source_size == 1u ? 0xb6u : 0xb7u);
        if (!x86_emit_rex(
                encoder, destination_size == 8u, destination,
                source.kind == RCC_X86_VALUE_GPR
                    ? source.gpr : RCC_X86_GPR_BP,
                source_size == 1u && source.kind == RCC_X86_VALUE_GPR &&
                    ((unsigned)source.gpr & 7u) >= 4u) ||
            !x86_emit_u8(encoder, 0x0fu) ||
            !x86_emit_u8(encoder, opcode)) return false;
    } else {
        return x86_encode_error(encoder,
                                "x86 extension width is unsupported");
    }
    if (source.kind == RCC_X86_VALUE_GPR) {
        return x86_emit_u8(encoder, x86_modrm(
            3u, destination, source.gpr));
    }
    if (!x86_value_displacement(encoder, source, &displacement)) {
        return false;
    }
    return x86_emit_memory_modrm(
        encoder, destination, displacement);
}

static bool x86_emit_conversion(
    RccX86Encoder* encoder,
    const RccX86LegalInstruction* instruction) {
    RccX86Value destination = instruction->destination;
    RccX86Value source = instruction->operands[0];
    uint16_t destination_size = destination.size;
    uint16_t source_size = source.size;
    bool sign_extend =
        instruction->selected_opcode == RCC_X86_SIGN_EXTEND;
    if (instruction->selected_opcode == RCC_X86_TRUNCATE ||
        instruction->selected_opcode == RCC_X86_REINTERPRET ||
        source_size == destination_size) {
        return x86_emit_copy(
            encoder, source, destination, destination_size);
    }
    if (destination_size <= source_size) {
        return x86_encode_error(encoder,
                                "x86 extension does not widen");
    }
    if (destination.kind == RCC_X86_VALUE_GPR) {
        return x86_emit_extend_register(
            encoder, destination.gpr, source, source_size,
            destination_size, sign_extend);
    }
    {
        RccX86HardwareGpr scratch = x86_choose_scratch(source, destination);
        return x86_emit_push(encoder, scratch) &&
            x86_emit_extend_register(
                encoder, scratch, source, source_size,
                destination_size, sign_extend) &&
            x86_emit_store(
                encoder, destination, scratch, destination_size) &&
            x86_emit_pop(encoder, scratch);
    }
}

static RccX86HardwareGpr x86_choose_scratch_three(
    RccX86Value first, RccX86Value second, RccX86HardwareGpr excluded) {
    static const RccX86HardwareGpr candidates[] = {
        RCC_X86_GPR_AX, RCC_X86_GPR_CX, RCC_X86_GPR_DX,
    };
    for (size_t index = 0u;
         index < sizeof(candidates) / sizeof(candidates[0]); ++index) {
        RccX86HardwareGpr candidate = candidates[index];
        if (candidate != excluded &&
            (first.kind != RCC_X86_VALUE_GPR || first.gpr != candidate) &&
            (second.kind != RCC_X86_VALUE_GPR || second.gpr != candidate)) {
            return candidate;
        }
    }
    return excluded == RCC_X86_GPR_AX ? RCC_X86_GPR_CX
                                      : RCC_X86_GPR_AX;
}

static bool x86_emit_indirect_modrm(
    RccX86Encoder* encoder, unsigned reg, RccX86HardwareGpr base) {
    unsigned low = (unsigned)base & 7u;
    if (low == 4u) {
        return x86_emit_u8(encoder, x86_modrm(0u, reg, 4u)) &&
            x86_emit_u8(encoder, 0x24u);
    }
    if (low == 5u) {
        return x86_emit_u8(encoder, x86_modrm(1u, reg, low)) &&
            x86_emit_u8(encoder, 0u);
    }
    return x86_emit_u8(encoder, x86_modrm(0u, reg, low));
}

static bool x86_emit_indirect_load(
    RccX86Encoder* encoder, RccX86HardwareGpr destination,
    RccX86HardwareGpr address, uint16_t size) {
    if (!x86_emit_prefix(encoder, size, destination, address,
                         size == 1u) ||
        !x86_emit_u8(encoder, size == 1u ? 0x8au : 0x8bu)) return false;
    return x86_emit_indirect_modrm(encoder, destination, address);
}

static bool x86_emit_indirect_store(
    RccX86Encoder* encoder, RccX86HardwareGpr address,
    RccX86HardwareGpr source, uint16_t size) {
    if (!x86_emit_prefix(encoder, size, source, address,
                         size == 1u) ||
        !x86_emit_u8(encoder, size == 1u ? 0x88u : 0x89u)) return false;
    return x86_emit_indirect_modrm(encoder, source, address);
}

static bool x86_emit_indirect_store_displacement(
    RccX86Encoder* encoder, RccX86HardwareGpr address,
    int32_t displacement, RccX86HardwareGpr source,
    uint16_t size) {
    if (!x86_emit_prefix(encoder, size, source, address,
                         size == 1u) ||
        !x86_emit_u8(encoder, size == 1u ? 0x88u : 0x89u) ||
        !x86_emit_u8(encoder, x86_modrm(
            2u, source, address))) return false;
    if (((unsigned)address & 7u) == 4u &&
        !x86_emit_u8(encoder, 0x24u)) return false;
    return x86_emit_u32(encoder, (uint32_t)displacement);
}

static bool x86_emit_capture_return_pair(
    RccX86Encoder* encoder,
    const RccX86LegalInstruction* instruction) {
    RccX86Value address = instruction->operands[0];
    RccX86HardwareGpr address_register = address.kind == RCC_X86_VALUE_GPR
        ? address.gpr : RCC_X86_GPR_R11;
    if (encoder->function->target != RCC_X86_TARGET_X86_64 ||
        instruction->immediate < 9u || instruction->immediate > 16u) {
        return x86_encode_error(
            encoder, "x86 return-pair capture is invalid");
    }
    if (address.kind != RCC_X86_VALUE_GPR &&
        !x86_emit_load(encoder, address_register, address, 8u)) {
        return false;
    }
    return x86_emit_indirect_store_displacement(
               encoder, address_register, 0, RCC_X86_GPR_AX, 8u) &&
        x86_emit_indirect_store_displacement(
               encoder, address_register, 8, RCC_X86_GPR_DX, 8u);
}

static bool x86_emit_stack_address(
    RccX86Encoder* encoder,
    const RccX86LegalInstruction* instruction) {
    RccX86Value slot;
    RccX86Value destination = instruction->destination;
    RccX86HardwareGpr result = destination.kind == RCC_X86_VALUE_GPR
        ? destination.gpr
        : x86_choose_scratch(destination, destination);
    int32_t displacement = 0;
    bool preserve = destination.kind != RCC_X86_VALUE_GPR;
    memset(&slot, 0, sizeof(slot));
    slot.kind = RCC_X86_VALUE_FRAME;
    slot.frame_offset = (uint32_t)instruction->immediate;
    slot.size = encoder->function->pointer_size;
    slot.alignment = encoder->function->pointer_size;
    if (!x86_value_displacement(encoder, slot, &displacement) ||
        (preserve && !x86_emit_push(encoder, result)) ||
        !x86_emit_prefix(
            encoder, encoder->function->pointer_size,
            result, RCC_X86_GPR_BP, false) ||
        !x86_emit_u8(encoder, 0x8du) ||
        !x86_emit_memory_modrm(encoder, result, displacement) ||
        (preserve && !x86_emit_store(
            encoder, destination, result,
            encoder->function->pointer_size)) ||
        (preserve && !x86_emit_pop(encoder, result))) return false;
    return true;
}

static bool x86_emit_pointer_load(
    RccX86Encoder* encoder,
    const RccX86LegalInstruction* instruction) {
    RccX86Value address = instruction->operands[0];
    RccX86Value destination = instruction->destination;
    RccX86HardwareGpr address_register;
    RccX86HardwareGpr result_register;
    bool preserve_address = address.kind != RCC_X86_VALUE_GPR;
    bool preserve_result = destination.kind != RCC_X86_VALUE_GPR;
    address_register = preserve_address
        ? x86_choose_scratch(address, destination) : address.gpr;
    result_register = preserve_result
        ? x86_choose_scratch_three(
            address, destination, address_register)
        : destination.gpr;
    if ((preserve_address && !x86_emit_push(
             encoder, address_register)) ||
        (preserve_result && !x86_emit_push(
             encoder, result_register)) ||
        (preserve_address && !x86_emit_load(
             encoder, address_register, address,
             encoder->function->pointer_size)) ||
        !x86_emit_indirect_load(
            encoder, result_register, address_register,
            destination.size) ||
        (preserve_result && !x86_emit_store(
             encoder, destination, result_register,
             destination.size)) ||
        (preserve_result && !x86_emit_pop(
             encoder, result_register)) ||
        (preserve_address && !x86_emit_pop(
             encoder, address_register))) return false;
    return true;
}

static bool x86_emit_pointer_store(
    RccX86Encoder* encoder,
    const RccX86LegalInstruction* instruction) {
    RccX86Value value = instruction->operands[0];
    RccX86Value address = instruction->operands[1];
    RccX86HardwareGpr address_register;
    RccX86HardwareGpr value_register;
    bool preserve_address = address.kind != RCC_X86_VALUE_GPR;
    bool preserve_value = value.kind != RCC_X86_VALUE_GPR;
    address_register = preserve_address
        ? x86_choose_scratch(address, value) : address.gpr;
    value_register = preserve_value
        ? x86_choose_scratch_three(
            address, value, address_register)
        : value.gpr;
    if ((preserve_address && !x86_emit_push(
             encoder, address_register)) ||
        (preserve_value && !x86_emit_push(
             encoder, value_register)) ||
        (preserve_address && !x86_emit_load(
             encoder, address_register, address,
             encoder->function->pointer_size)) ||
        (preserve_value && !x86_emit_load(
             encoder, value_register, value, value.size)) ||
        !x86_emit_indirect_store(
            encoder, address_register, value_register, value.size) ||
        (preserve_value && !x86_emit_pop(
             encoder, value_register)) ||
        (preserve_address && !x86_emit_pop(
             encoder, address_register))) return false;
    return true;
}

static bool x86_emit_gep_index(
    RccX86Encoder* encoder, RccX86HardwareGpr destination,
    RccX86Value source, RccMirType type) {
    uint16_t source_size;
    uint16_t pointer_size = encoder->function->pointer_size;
    if (type.kind != RCC_MIR_TYPE_INTEGER || type.bit_width == 0u) {
        return x86_encode_error(encoder,
                                "x86 GEP index is not an integer");
    }
    switch (type.bit_width) {
        case 1u: case 8u: source_size = 1u; break;
        case 16u: source_size = 2u; break;
        case 32u: source_size = 4u; break;
        case 64u: source_size = 8u; break;
        default: source_size = 0u; break;
    }
    if (source_size == 0u || source_size > pointer_size ||
        source.size != source_size) {
        return x86_encode_error(
            encoder, "x86 GEP index width is unsupported");
    }
    if (source_size == pointer_size) {
        return x86_emit_copy(
            encoder, source,
            (RccX86Value){RCC_X86_VALUE_GPR, destination, 0u,
                          pointer_size, pointer_size},
            pointer_size);
    }
    return x86_emit_extend_register(
        encoder, destination, source, source_size,
        pointer_size, true);
}

static bool x86_emit_gep(
    RccX86Encoder* encoder,
    const RccX86LegalInstruction* instruction) {
    RccX86Value base = instruction->operands[0];
    RccX86Value index = instruction->operands[1];
    RccX86Value destination = instruction->destination;
    RccX86HardwareGpr result;
    bool destination_is_base =
        destination.kind == RCC_X86_VALUE_GPR &&
        base.kind == RCC_X86_VALUE_GPR &&
        destination.gpr == base.gpr;
    bool preserve_result;
    if (base.size != encoder->function->pointer_size ||
        destination.size != encoder->function->pointer_size) {
        return x86_encode_error(
            encoder, "x86 GEP pointer operand width is invalid");
    }
    if (instruction->auxiliary == 0u) {
        return x86_encode_error(encoder, "x86 GEP scale is zero");
    }
    result = destination.kind == RCC_X86_VALUE_GPR &&
             !destination_is_base
        ? destination.gpr : x86_choose_scratch(base, index);
    preserve_result = destination.kind != RCC_X86_VALUE_GPR ||
        result != destination.gpr;
    if ((preserve_result && !x86_emit_push(encoder, result)) ||
        !x86_emit_gep_index(
            encoder, result, index, instruction->operand_types[1]) ||
        !x86_emit_multiply_immediate_register(
            encoder, result, encoder->function->pointer_size,
            instruction->auxiliary)) return false;
    if (destination_is_base) {
        if (!x86_emit_binary_register(
                encoder, RCC_X86_ADD, destination.gpr,
                (RccX86Value){RCC_X86_VALUE_GPR, result, 0u,
                              encoder->function->pointer_size,
                              encoder->function->pointer_size},
                encoder->function->pointer_size)) return false;
    } else {
        if (!x86_emit_binary_register(
                encoder, RCC_X86_ADD, result, base,
                encoder->function->pointer_size) ||
            (destination.kind != RCC_X86_VALUE_GPR &&
             !x86_emit_store(
                 encoder, destination, result,
                 encoder->function->pointer_size))) return false;
    }
    return !preserve_result || x86_emit_pop(encoder, result);
}

static bool x86_patch_rel32_to_here(
    RccX86Encoder* encoder, uint32_t offset) {
    int64_t delta;
    int32_t encoded;
    if (offset > encoder->output.code_size ||
        4u > encoder->output.code_size - offset) {
        return x86_encode_error(encoder,
                                "x86 local branch fixup is invalid");
    }
    delta = (int64_t)encoder->output.code_size -
        ((int64_t)offset + 4);
    if (delta < INT32_MIN || delta > INT32_MAX) {
        return x86_encode_error(
            encoder, "x86 local branch displacement overflows");
    }
    encoded = (int32_t)delta;
    for (size_t byte = 0u; byte < sizeof(encoded); ++byte) {
        encoder->output.code[offset + byte] =
            (uint8_t)((uint32_t)encoded >> (byte * 8u));
    }
    return true;
}

static bool x86_emit_select(
    RccX86Encoder* encoder,
    const RccX86LegalInstruction* instruction) {
    RccX86Value condition = instruction->operands[0];
    RccX86Value when_true = instruction->operands[1];
    RccX86Value when_false = instruction->operands[2];
    RccX86Value destination = instruction->destination;
    uint16_t size = 0u;
    uint32_t branch_offset;
    if (instruction->type.kind == RCC_MIR_TYPE_POINTER) {
        size = encoder->function->pointer_size;
    } else if (instruction->type.kind == RCC_MIR_TYPE_INTEGER) {
        switch (instruction->type.bit_width) {
            case 1u: case 8u: size = 1u; break;
            case 16u: size = 2u; break;
            case 32u: size = 4u; break;
            case 64u: size = 8u; break;
            default: break;
        }
    }
    if (size == 0u || size > encoder->function->pointer_size ||
        condition.size != 1u || destination.size != size ||
        when_true.size != size || when_false.size != size) {
        return x86_encode_error(encoder,
                                "x86 select width is not native");
    }
    if (x86_value_equal(destination, when_true) &&
        x86_value_equal(destination, when_false)) return true;
    if (!x86_emit_compare_zero(encoder, condition, 1u)) return false;
    if (x86_value_equal(destination, when_true)) {
        if (!x86_emit_u8(encoder, 0x0fu) ||
            !x86_emit_u8(encoder, 0x85u)) return false;
        branch_offset = (uint32_t)encoder->output.code_size;
        return x86_emit_u32(encoder, 0u) &&
            x86_emit_copy(encoder, when_false, destination, size) &&
            x86_patch_rel32_to_here(encoder, branch_offset);
    }
    if (!x86_emit_copy(encoder, when_false, destination, size) ||
        !x86_emit_u8(encoder, 0x0fu) ||
        !x86_emit_u8(encoder, 0x84u)) return false;
    branch_offset = (uint32_t)encoder->output.code_size;
    return x86_emit_u32(encoder, 0u) &&
        x86_emit_copy(encoder, when_true, destination, size) &&
        x86_patch_rel32_to_here(encoder, branch_offset);
}

static bool x86_add_fixup(RccX86Encoder* encoder, uint32_t target) {
    RccX86BranchFixup* fixup;
    if (encoder->fixup_count == encoder->fixup_capacity) {
        size_t capacity = encoder->fixup_capacity == 0u
            ? 8u : encoder->fixup_capacity * 2u;
        if (capacity < encoder->fixup_capacity ||
            capacity > SIZE_MAX / sizeof(*encoder->fixups)) {
            return x86_encode_error(encoder,
                                    "x86 branch table is too large");
        }
        encoder->fixups = rcc_realloc(
            encoder->fixups, capacity * sizeof(*encoder->fixups));
        encoder->fixup_capacity = capacity;
    }
    fixup = &encoder->fixups[encoder->fixup_count++];
    fixup->offset = (uint32_t)encoder->output.code_size;
    fixup->target = target;
    return x86_emit_u32(encoder, 0u);
}

static bool x86_add_relocation(
    RccX86Encoder* encoder, const char* symbol,
    RccX86CodeRelocationType type) {
    RccX86CodeRelocation* relocation;
    if (encoder->output.relocation_count ==
        encoder->relocation_capacity) {
        size_t capacity = encoder->relocation_capacity == 0u
            ? 8u : encoder->relocation_capacity * 2u;
        if (capacity < encoder->relocation_capacity ||
            capacity > SIZE_MAX / sizeof(*encoder->output.relocations)) {
            return x86_encode_error(encoder,
                                    "x86 relocation table is too large");
        }
        encoder->output.relocations = rcc_realloc(
            encoder->output.relocations,
            capacity * sizeof(*encoder->output.relocations));
        encoder->relocation_capacity = capacity;
    }
    relocation = &encoder->output.relocations[
        encoder->output.relocation_count++];
    memset(relocation, 0, sizeof(*relocation));
    relocation->offset = (uint32_t)encoder->output.code_size;
    relocation->type = type;
    relocation->symbol = rcc_strdup(symbol);
    return type == RCC_X86_CODE_RELOC_ABS64
        ? x86_emit_u64(encoder, 0u) : x86_emit_u32(encoder, 0u);
}

static bool x86_emit_symbol_address_register(
    RccX86Encoder* encoder, RccX86HardwareGpr destination,
    const char* symbol) {
    RccX86CodeRelocationType type =
        encoder->function->target == RCC_X86_TARGET_X86_64
            ? RCC_X86_CODE_RELOC_ABS64
            : RCC_X86_CODE_RELOC_ABS32U;
    if (!x86_emit_rex(
            encoder, encoder->function->pointer_size == 8u,
            RCC_X86_GPR_AX, destination, false) ||
        !x86_emit_u8(encoder, (uint8_t)(
            0xb8u + ((unsigned)destination & 7u)))) return false;
    return x86_add_relocation(encoder, symbol, type);
}

static bool x86_emit_symbol_address(
    RccX86Encoder* encoder,
    const RccX86LegalInstruction* instruction) {
    RccX86Value destination = instruction->destination;
    uint16_t size = encoder->function->pointer_size;
    if (destination.size != size || !instruction->symbol ||
        !instruction->symbol[0]) {
        return x86_encode_error(
            encoder, "x86 symbol address contract is invalid");
    }
    if (destination.kind == RCC_X86_VALUE_GPR) {
        return x86_emit_symbol_address_register(
            encoder, destination.gpr, instruction->symbol);
    }
    return x86_emit_push(encoder, RCC_X86_GPR_AX) &&
        x86_emit_symbol_address_register(
            encoder, RCC_X86_GPR_AX, instruction->symbol) &&
        x86_emit_store(
            encoder, destination, RCC_X86_GPR_AX, size) &&
        x86_emit_pop(encoder, RCC_X86_GPR_AX);
}

static bool x86_emit_compare_zero(RccX86Encoder* encoder,
                                  RccX86Value value, uint16_t size) {
    uint8_t opcode = size == 1u ? 0x80u : 0x83u;
    if (value.kind == RCC_X86_VALUE_GPR) {
        if (!x86_emit_prefix(encoder, size, RCC_X86_GPR_DI,
                             value.gpr, size == 1u) ||
            !x86_emit_u8(encoder, opcode) ||
            !x86_emit_u8(encoder, x86_modrm(
                3u, 7u, value.gpr))) return false;
    } else {
        int32_t displacement;
        if (!x86_value_displacement(encoder, value, &displacement) ||
            !x86_emit_prefix(encoder, size, RCC_X86_GPR_DI,
                             RCC_X86_GPR_BP, false) ||
            !x86_emit_u8(encoder, opcode) ||
            !x86_emit_memory_modrm(
                encoder, 7u, displacement)) return false;
    }
    return x86_emit_u8(encoder, 0u);
}

static bool x86_emit_prologue(RccX86Encoder* encoder) {
    const RccX86LegalFunction* function = encoder->function;
    if (!x86_emit_push(encoder, RCC_X86_GPR_BP) ||
        !x86_emit_prefix(encoder, function->pointer_size,
                         RCC_X86_GPR_SP, RCC_X86_GPR_BP, false) ||
        !x86_emit_u8(encoder, 0x89u) ||
        !x86_emit_u8(encoder, x86_modrm(
            3u, RCC_X86_GPR_SP, RCC_X86_GPR_BP))) return false;
    if (function->stack_adjustment != 0u) {
        if (!x86_emit_prefix(encoder, function->pointer_size,
                             RCC_X86_GPR_BP, RCC_X86_GPR_SP, false) ||
            !x86_emit_u8(encoder, 0x81u) ||
            !x86_emit_u8(encoder, x86_modrm(
                3u, 5u, RCC_X86_GPR_SP)) ||
            !x86_emit_u32(encoder, function->stack_adjustment)) {
            return false;
        }
    }
    for (size_t index = 0u; index < function->callee_save_count; ++index) {
        RccX86Value slot;
        memset(&slot, 0, sizeof(slot));
        slot.kind = RCC_X86_VALUE_FRAME;
        slot.frame_offset = function->callee_saves[index].frame_offset;
        slot.size = function->pointer_size;
        slot.alignment = function->pointer_size;
        if (!x86_emit_store(encoder, slot,
                            function->callee_saves[index].gpr,
                            function->pointer_size)) return false;
    }
    return true;
}

static bool x86_emit_stack_subtract(RccX86Encoder* encoder,
                                    uint32_t bytes) {
    if (bytes == 0u) return true;
    return x86_emit_prefix(encoder, encoder->function->pointer_size,
                           RCC_X86_GPR_BP, RCC_X86_GPR_SP, false) &&
        x86_emit_u8(encoder, 0x81u) &&
        x86_emit_u8(encoder, x86_modrm(
            3u, 5u, RCC_X86_GPR_SP)) &&
        x86_emit_u32(encoder, bytes);
}

static bool x86_emit_epilogue(RccX86Encoder* encoder,
                              uint16_t stack_pop) {
    const RccX86LegalFunction* function = encoder->function;
    size_t index = function->callee_save_count;
    while (index != 0u) {
        RccX86Value slot;
        --index;
        memset(&slot, 0, sizeof(slot));
        slot.kind = RCC_X86_VALUE_FRAME;
        slot.frame_offset = function->callee_saves[index].frame_offset;
        slot.size = function->pointer_size;
        slot.alignment = function->pointer_size;
        if (!x86_emit_load(encoder,
                           function->callee_saves[index].gpr,
                           slot, function->pointer_size)) return false;
    }
    if (!x86_emit_u8(encoder, 0xc9u)) return false;
    if (stack_pop == 0u) return x86_emit_u8(encoder, 0xc3u);
    return x86_emit_u8(encoder, 0xc2u) &&
        x86_emit_u16(encoder, stack_pop);
}

static bool x86_emit_instruction(
    RccX86Encoder* encoder,
    const RccX86LegalInstruction* instruction) {
    uint16_t size;
    switch (instruction->opcode) {
        case RCC_X86_LEGAL_COPY:
            size = instruction->type.kind == RCC_MIR_TYPE_POINTER
                ? encoder->function->pointer_size
                : (instruction->type.bit_width <= 8u
                       ? 1u : (uint16_t)(instruction->type.bit_width / 8u));
            return x86_emit_copy(
                encoder, instruction->operands[0],
                instruction->destination, size);
        case RCC_X86_LEGAL_BINARY:
            return x86_emit_binary(encoder, instruction);
        case RCC_X86_LEGAL_PREPARE_UNSIGNED_DIVIDEND:
        case RCC_X86_LEGAL_PREPARE_SIGNED_DIVIDEND:
            return x86_emit_prepare_dividend(encoder, instruction);
        case RCC_X86_LEGAL_DIVIDE:
            return x86_emit_divide(encoder, instruction);
        case RCC_X86_LEGAL_SHIFT:
            return x86_emit_shift(encoder, instruction);
        case RCC_X86_LEGAL_CALL:
            if (!x86_emit_u8(encoder, 0xe8u) ||
                !x86_add_relocation(
                    encoder, instruction->symbol,
                    RCC_X86_CODE_RELOC_REL32)) return false;
            return x86_emit_stack_subtract(
                encoder, (uint32_t)instruction->immediate);
        case RCC_X86_LEGAL_RETURN:
            return x86_emit_epilogue(
                encoder,
                encoder->function->target == RCC_X86_TARGET_I686
                    ? (uint16_t)instruction->immediate : 0u);
        case RCC_X86_LEGAL_SELECTED:
            break;
        default:
            return x86_encode_error(
                encoder, "x86 legal opcode is not encoded yet");
    }
    switch (instruction->selected_opcode) {
        case RCC_X86_MOV_IMMEDIATE:
            size = instruction->type.kind == RCC_MIR_TYPE_POINTER
                ? encoder->function->pointer_size
                : (instruction->type.bit_width <= 8u
                       ? 1u : (uint16_t)(instruction->type.bit_width / 8u));
            return x86_emit_immediate(
                encoder, instruction->destination,
                size, instruction->immediate);
        case RCC_X86_COMPARE_SET:
            return x86_emit_compare_set(encoder, instruction);
        case RCC_X86_TRUNCATE:
        case RCC_X86_ZERO_EXTEND:
        case RCC_X86_SIGN_EXTEND:
        case RCC_X86_REINTERPRET:
            return x86_emit_conversion(encoder, instruction);
        case RCC_X86_STACK_ADDRESS:
            return x86_emit_stack_address(encoder, instruction);
        case RCC_X86_SYMBOL_ADDRESS:
            return x86_emit_symbol_address(encoder, instruction);
        case RCC_X86_LOAD:
            return x86_emit_pointer_load(encoder, instruction);
        case RCC_X86_STORE:
            return x86_emit_pointer_store(encoder, instruction);
        case RCC_X86_GEP:
            return x86_emit_gep(encoder, instruction);
        case RCC_X86_CAPTURE_RETURN_PAIR:
            return x86_emit_capture_return_pair(encoder, instruction);
        case RCC_X86_SELECT:
            return x86_emit_select(encoder, instruction);
        case RCC_X86_JUMP:
            return x86_emit_u8(encoder, 0xe9u) &&
                x86_add_fixup(encoder, instruction->targets[0]);
        case RCC_X86_JUMP_IF:
            size = instruction->operand_types[0].kind ==
                       RCC_MIR_TYPE_POINTER
                ? encoder->function->pointer_size
                : (instruction->operand_types[0].bit_width <= 8u
                       ? 1u
                       : (uint16_t)(
                           instruction->operand_types[0].bit_width / 8u));
            return x86_emit_compare_zero(
                       encoder, instruction->operands[0], size) &&
                x86_emit_u8(encoder, 0x0fu) &&
                x86_emit_u8(encoder, 0x85u) &&
                x86_add_fixup(encoder, instruction->targets[0]) &&
                x86_emit_u8(encoder, 0xe9u) &&
                x86_add_fixup(encoder, instruction->targets[1]);
        case RCC_X86_TRAP:
            return x86_emit_u8(encoder, 0x0fu) &&
                x86_emit_u8(encoder, 0x0bu);
        default:
            return x86_encode_error(
                encoder, "x86 selected opcode is not encoded yet");
    }
}

static bool x86_resolve_fixups(RccX86Encoder* encoder) {
    for (size_t index = 0u; index < encoder->fixup_count; ++index) {
        RccX86BranchFixup fixup = encoder->fixups[index];
        int64_t delta;
        int32_t encoded;
        if (fixup.target >= encoder->output.block_count ||
            fixup.offset > encoder->output.code_size ||
            4u > encoder->output.code_size - fixup.offset) {
            return x86_encode_error(encoder,
                                    "x86 branch fixup is invalid");
        }
        delta = (int64_t)encoder->output.block_offsets[fixup.target] -
            ((int64_t)fixup.offset + 4);
        if (delta < INT32_MIN || delta > INT32_MAX) {
            return x86_encode_error(encoder,
                                    "x86 branch displacement overflows");
        }
        encoded = (int32_t)delta;
        for (size_t byte = 0u; byte < sizeof(encoded); ++byte) {
            encoder->output.code[fixup.offset + byte] =
                (uint8_t)((uint32_t)encoded >> (byte * 8u));
        }
    }
    return true;
}

void rcc_x86_encoded_function_release(RccX86EncodedFunction* encoded) {
    if (!encoded) return;
    for (size_t index = 0u; index < encoded->relocation_count; ++index) {
        rcc_free(encoded->relocations[index].symbol);
    }
    rcc_free(encoded->relocations);
    rcc_free(encoded->block_offsets);
    rcc_free(encoded->code);
    memset(encoded, 0, sizeof(*encoded));
}

bool rcc_x86_verify_encoded_function(
    const RccX86EncodedFunction* encoded,
    char* error, size_t error_size) {
    if (error && error_size != 0u) error[0] = '\0';
    if (!encoded ||
        (encoded->target != RCC_X86_TARGET_I686 &&
         encoded->target != RCC_X86_TARGET_X86_64) ||
        !encoded->code || encoded->code_size == 0u ||
        encoded->code_size > UINT32_MAX ||
        !encoded->block_offsets || encoded->block_count == 0u ||
        (encoded->relocation_count != 0u && !encoded->relocations)) {
        if (error && error_size != 0u) {
            snprintf(error, error_size,
                     "x86 encoded-function header is invalid");
        }
        return false;
    }
    for (size_t index = 0u; index < encoded->block_count; ++index) {
        if (encoded->block_offsets[index] >= encoded->code_size ||
            (index != 0u && encoded->block_offsets[index] <=
                encoded->block_offsets[index - 1u])) {
            if (error && error_size != 0u) {
                snprintf(error, error_size,
                         "x86 encoded block table is invalid");
            }
            return false;
        }
    }
    for (size_t index = 0u; index < encoded->relocation_count; ++index) {
        const RccX86CodeRelocation* relocation =
            &encoded->relocations[index];
        uint32_t width = relocation->type == RCC_X86_CODE_RELOC_ABS64
            ? 8u : 4u;
        bool type_valid = relocation->type == RCC_X86_CODE_RELOC_REL32 ||
            (relocation->type == RCC_X86_CODE_RELOC_ABS32U &&
             encoded->target == RCC_X86_TARGET_I686) ||
            (relocation->type == RCC_X86_CODE_RELOC_ABS64 &&
             encoded->target == RCC_X86_TARGET_X86_64);
        if (!type_valid ||
            relocation->addend != 0 ||
            !relocation->symbol || !relocation->symbol[0] ||
            relocation->offset > encoded->code_size ||
            width > encoded->code_size - relocation->offset ||
            (index != 0u && relocation->offset <=
                encoded->relocations[index - 1u].offset)) {
            if (error && error_size != 0u) {
                snprintf(error, error_size,
                         "x86 encoded relocation is invalid");
            }
            return false;
        }
    }
    return true;
}

bool rcc_x86_encode_function(
    const RccX86LegalFunction* function,
    const RccMirRegisterPolicy* policy,
    RccX86EncodedFunction* encoded,
    char* error, size_t error_size) {
    RccX86Encoder encoder;
    const RccX86LegalBlock* block;
    bool result = false;
    if (encoded) memset(encoded, 0, sizeof(*encoded));
    if (error && error_size != 0u) error[0] = '\0';
    if (!function || !policy || !encoded ||
        !rcc_x86_verify_legal_function(
            function, policy, error, error_size)) return false;
    memset(&encoder, 0, sizeof(encoder));
    encoder.function = function;
    encoder.output.target = function->target;
    encoder.output.block_count = function->block_count;
    encoder.output.block_offsets = rcc_alloc(
        function->block_count * sizeof(*encoder.output.block_offsets));
    encoder.error = error;
    encoder.error_size = error_size;
    if (!x86_emit_prologue(&encoder)) goto cleanup;
    for (block = function->first_block; block; block = block->next) {
        const RccX86LegalInstruction* instruction;
        encoder.output.block_offsets[block->id] =
            (uint32_t)encoder.output.code_size;
        for (instruction = block->first; instruction;
             instruction = instruction->next) {
            if (!x86_emit_instruction(&encoder, instruction)) goto cleanup;
        }
    }
    if (!x86_resolve_fixups(&encoder) ||
        !rcc_x86_verify_encoded_function(
            &encoder.output, error, error_size)) goto cleanup;
    *encoded = encoder.output;
    memset(&encoder.output, 0, sizeof(encoder.output));
    result = true;
cleanup:
    rcc_free(encoder.fixups);
    rcc_x86_encoded_function_release(&encoder.output);
    return result;
}
