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
    if (value.frame_offset > encoder->function->stack_adjustment) {
        return x86_encode_error(encoder,
                                "x86 frame displacement is positive");
    }
    magnitude = encoder->function->stack_adjustment - value.frame_offset;
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
    int32_t displacement;
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

static bool x86_add_relocation(RccX86Encoder* encoder,
                               const char* symbol) {
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
    relocation->type = RCC_X86_CODE_RELOC_REL32;
    relocation->symbol = rcc_strdup(symbol);
    return x86_emit_u32(encoder, 0u);
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

static bool x86_emit_epilogue(RccX86Encoder* encoder) {
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
    return x86_emit_u8(encoder, 0xc9u) &&
        x86_emit_u8(encoder, 0xc3u);
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
        case RCC_X86_LEGAL_CALL:
            return x86_emit_u8(encoder, 0xe8u) &&
                x86_add_relocation(encoder, instruction->symbol);
        case RCC_X86_LEGAL_RETURN:
            return x86_emit_epilogue(encoder);
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
        if (relocation->type != RCC_X86_CODE_RELOC_REL32 ||
            relocation->addend != 0 ||
            !relocation->symbol || !relocation->symbol[0] ||
            relocation->offset > encoded->code_size ||
            4u > encoded->code_size - relocation->offset ||
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
