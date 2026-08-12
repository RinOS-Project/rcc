/*
 * RCC - x86 fixed-register legalization
 */

#include "rcc.h"
#include "x86_legalize.h"

#include <stdarg.h>

static bool x86_legal_error(char* error, size_t error_size,
                            const char* format, ...) {
    va_list arguments;
    if (error && error_size != 0u) {
        va_start(arguments, format);
        vsnprintf(error, error_size, format, arguments);
        va_end(arguments);
    }
    return false;
}

static uint16_t x86_legal_type_size(
    RccMirType type, const RccX86Abi* abi) {
    if (type.kind == RCC_MIR_TYPE_POINTER) return abi->pointer_size;
    if (type.bit_width <= 8u) return 1u;
    return (uint16_t)(type.bit_width / 8u);
}

static bool x86_legal_value_equal(RccX86Value left, RccX86Value right) {
    if (left.kind != right.kind) return false;
    if (left.kind == RCC_X86_VALUE_GPR) return left.gpr == right.gpr;
    return left.frame_offset == right.frame_offset &&
        left.size == right.size;
}

static RccX86Value x86_legal_fixed_gpr(
    RccX86HardwareGpr gpr, RccMirType type, const RccX86Abi* abi) {
    RccX86Value value;
    memset(&value, 0, sizeof(value));
    value.kind = RCC_X86_VALUE_GPR;
    value.gpr = gpr;
    value.size = x86_legal_type_size(type, abi);
    value.alignment = value.size;
    return value;
}

static bool x86_legal_resolve_location(
    RccMirLocation location, RccMirType type, const RccX86Abi* abi,
    RccX86Value* value, char* error, size_t error_size) {
    memset(value, 0, sizeof(*value));
    if (location.kind == RCC_MIR_LOCATION_PHYSICAL) {
        if (location.register_class != RCC_MIR_REGCLASS_GPR ||
            !rcc_x86_abi_hardware_gpr(
                abi, location.physical_register, &value->gpr)) {
            return x86_legal_error(
                error, error_size,
                "x86 legalizer cannot resolve a physical register");
        }
        value->kind = RCC_X86_VALUE_GPR;
        value->size = x86_legal_type_size(type, abi);
        value->alignment = value->size;
        return true;
    }
    if (location.kind != RCC_MIR_LOCATION_SPILL) {
        return x86_legal_error(error, error_size,
                               "x86 legalizer saw an invalid location");
    }
    value->kind = RCC_X86_VALUE_FRAME;
    value->frame_offset = location.spill_offset;
    value->size = location.spill_size;
    value->alignment = location.spill_alignment;
    return true;
}

static RccX86LegalBlock* x86_legal_append_block(
    RccX86LegalFunction* function, const RccX86Block* source) {
    RccX86LegalBlock* block;
    if (function->block_count >= UINT32_MAX) return NULL;
    block = rcc_alloc(sizeof(*block));
    block->id = source->id;
    block->source_block = source->source_block;
    block->edge_split = source->edge_split;
    block->edge_predecessor = source->edge_predecessor;
    block->edge_successor = source->edge_successor;
    if (function->last_block) function->last_block->next = block;
    else function->first_block = block;
    function->last_block = block;
    ++function->block_count;
    return block;
}

static RccX86LegalInstruction* x86_legal_append(
    RccX86LegalFunction* function, RccX86LegalBlock* block,
    RccX86LegalOpcode opcode, RccX86Opcode selected_opcode,
    RccMirType type, const RccX86Value* destination,
    const RccX86Value* operands, const RccMirType* operand_types,
    size_t operand_count) {
    RccX86LegalInstruction* instruction;
    if (!function || !block ||
        (operand_count != 0u && (!operands || !operand_types))) {
        return NULL;
    }
    instruction = rcc_alloc(sizeof(*instruction));
    instruction->opcode = opcode;
    instruction->selected_opcode = selected_opcode;
    instruction->type = type;
    if (destination) {
        instruction->has_destination = true;
        instruction->destination = *destination;
    }
    if (operand_count != 0u) {
        instruction->operands = rcc_alloc(
            operand_count * sizeof(*instruction->operands));
        instruction->operand_types = rcc_alloc(
            operand_count * sizeof(*instruction->operand_types));
        memcpy(instruction->operands, operands,
               operand_count * sizeof(*instruction->operands));
        memcpy(instruction->operand_types, operand_types,
               operand_count * sizeof(*instruction->operand_types));
        instruction->operand_count = operand_count;
    }
    instruction->previous = block->last;
    if (block->last) block->last->next = instruction;
    else block->first = instruction;
    block->last = instruction;
    ++function->legal_instruction_count;
    return instruction;
}

static bool x86_legal_copy_selected_metadata(
    RccX86LegalInstruction* destination,
    const RccX86Instruction* source) {
    if (source->target_count != 0u) {
        if (source->target_count >
            (size_t)-1 / sizeof(*destination->targets)) return false;
        destination->targets = rcc_alloc(
            source->target_count * sizeof(*destination->targets));
        memcpy(destination->targets, source->targets,
               source->target_count * sizeof(*destination->targets));
        destination->target_count = source->target_count;
    }
    destination->immediate = source->immediate;
    destination->auxiliary = source->auxiliary;
    destination->predicate = source->predicate;
    destination->cycle_break = source->cycle_break;
    if (source->symbol) destination->symbol = rcc_strdup(source->symbol);
    return true;
}

static bool x86_legal_resolve_instruction(
    const RccX86Instruction* source, const RccX86Abi* abi,
    RccX86Value* destination, RccX86Value** operands_out,
    char* error, size_t error_size) {
    RccX86Value* operands = NULL;
    size_t index;
    if (source->has_destination &&
        !x86_legal_resolve_location(
            source->destination, source->type, abi, destination,
            error, error_size)) {
        return false;
    }
    if (source->operand_count != 0u) {
        if (source->operand_count > (size_t)-1 / sizeof(*operands)) {
            return x86_legal_error(error, error_size,
                                   "x86 legal operand table is too large");
        }
        operands = rcc_alloc(
            source->operand_count * sizeof(*operands));
        for (index = 0u; index < source->operand_count; ++index) {
            if (!x86_legal_resolve_location(
                    source->operands[index], source->operand_types[index],
                    abi, &operands[index], error, error_size)) {
                rcc_free(operands);
                return false;
            }
        }
    }
    *operands_out = operands;
    return true;
}

static bool x86_legal_is_division(RccX86Opcode opcode) {
    return opcode == RCC_X86_UDIV || opcode == RCC_X86_SDIV ||
        opcode == RCC_X86_UREM || opcode == RCC_X86_SREM;
}

static bool x86_legal_is_shift(RccX86Opcode opcode) {
    return opcode == RCC_X86_SHL || opcode == RCC_X86_SHR ||
        opcode == RCC_X86_SAR;
}

static bool x86_legal_native_type(
    RccMirType type, const RccX86Abi* abi) {
    return type.kind == RCC_MIR_TYPE_INTEGER &&
        type.bit_width != 0u &&
        type.bit_width <= (uint16_t)(abi->pointer_size * 8u);
}

static bool x86_legal_clone_selected(
    RccX86LegalFunction* function, RccX86LegalBlock* block,
    const RccX86Instruction* source, const RccX86Abi* abi,
    char* error, size_t error_size) {
    RccX86Value destination;
    RccX86Value* operands = NULL;
    RccX86LegalInstruction* legal;
    if (!x86_legal_resolve_instruction(
            source, abi, &destination, &operands, error, error_size)) {
        return false;
    }
    legal = x86_legal_append(
        function, block,
        source->opcode == RCC_X86_COPY
            ? RCC_X86_LEGAL_COPY : RCC_X86_LEGAL_SELECTED,
        source->opcode,
        source->type, source->has_destination ? &destination : NULL,
        operands, source->operand_types, source->operand_count);
    rcc_free(operands);
    if (!legal || !x86_legal_copy_selected_metadata(legal, source)) {
        return x86_legal_error(error, error_size,
                               "x86 legal instruction is too large");
    }
    return true;
}

static bool x86_legalize_division(
    RccX86LegalFunction* function, RccX86LegalBlock* block,
    const RccX86Instruction* source, const RccX86Abi* abi,
    char* error, size_t error_size) {
    RccX86Value destination;
    RccX86Value* operands = NULL;
    RccX86Value accumulator;
    RccX86Value result;
    RccX86LegalOpcode prepare;
    RccX86LegalInstruction* divide;
    bool is_signed = source->opcode == RCC_X86_SDIV ||
        source->opcode == RCC_X86_SREM;
    bool is_remainder = source->opcode == RCC_X86_UREM ||
        source->opcode == RCC_X86_SREM;
    if (!x86_legal_resolve_instruction(
            source, abi, &destination, &operands, error, error_size)) {
        return false;
    }
    accumulator = x86_legal_fixed_gpr(abi->return_low,
                                       source->type, abi);
    result = x86_legal_fixed_gpr(
        is_remainder ? abi->return_high : abi->return_low,
        source->type, abi);
    prepare = is_signed ? RCC_X86_LEGAL_PREPARE_SIGNED_DIVIDEND
                        : RCC_X86_LEGAL_PREPARE_UNSIGNED_DIVIDEND;
    if (!x86_legal_append(
            function, block, RCC_X86_LEGAL_COPY, RCC_X86_COPY,
            source->type, &accumulator, &operands[0],
            &source->operand_types[0], 1u) ||
        !x86_legal_append(
            function, block, prepare, source->opcode,
            source->type, NULL, NULL, NULL, 0u)) {
        rcc_free(operands);
        return x86_legal_error(error, error_size,
                               "x86 division preparation is too large");
    }
    divide = x86_legal_append(
        function, block, RCC_X86_LEGAL_DIVIDE, source->opcode,
        source->type, NULL, &operands[1],
        &source->operand_types[1], 1u);
    if (!divide || !x86_legal_append(
            function, block, RCC_X86_LEGAL_COPY, RCC_X86_COPY,
            source->type, &destination, &result, &source->type, 1u)) {
        rcc_free(operands);
        return x86_legal_error(error, error_size,
                               "x86 division result is too large");
    }
    rcc_free(operands);
    return true;
}

static bool x86_legalize_shift(
    RccX86LegalFunction* function, RccX86LegalBlock* block,
    const RccX86Instruction* source, const RccX86Abi* abi,
    char* error, size_t error_size) {
    RccX86Value destination;
    RccX86Value* operands = NULL;
    RccX86Value count;
    if (!x86_legal_resolve_instruction(
            source, abi, &destination, &operands, error, error_size)) {
        return false;
    }
    count = x86_legal_fixed_gpr(abi->shift_count,
                                 source->operand_types[1], abi);
    if (!x86_legal_append(
            function, block, RCC_X86_LEGAL_COPY, RCC_X86_COPY,
            source->type, &destination, &operands[0],
            &source->operand_types[0], 1u) ||
        !x86_legal_append(
            function, block, RCC_X86_LEGAL_COPY, RCC_X86_COPY,
            source->operand_types[1], &count, &operands[1],
            &source->operand_types[1], 1u) ||
        !x86_legal_append(
            function, block, RCC_X86_LEGAL_SHIFT, source->opcode,
            source->type, &destination, NULL, NULL, 0u)) {
        rcc_free(operands);
        return x86_legal_error(error, error_size,
                               "x86 shift legalization is too large");
    }
    rcc_free(operands);
    return true;
}

static bool x86_legal_gpr_allowed(RccX86HardwareGpr gpr,
                                  const RccX86Abi* abi) {
    size_t index;
    for (index = 0u; index < abi->gpr_count; ++index) {
        if (abi->gpr_map[index] == gpr) return true;
    }
    return false;
}

static bool x86_legal_value_valid(
    RccX86Value value, const RccX86LegalFunction* function,
    const RccX86Abi* abi) {
    if (value.size == 0u || value.alignment == 0u) return false;
    if (value.kind == RCC_X86_VALUE_GPR) {
        return x86_legal_gpr_allowed(value.gpr, abi);
    }
    return value.kind == RCC_X86_VALUE_FRAME &&
        value.frame_offset % value.alignment == 0u &&
        value.frame_offset <= function->frame_size &&
        value.size <= function->frame_size - value.frame_offset;
}

static bool x86_legal_selected_shape(
    const RccX86LegalInstruction* instruction) {
    switch (instruction->selected_opcode) {
        case RCC_X86_MOV_IMMEDIATE:
        case RCC_X86_STACK_ADDRESS:
            return instruction->has_destination &&
                instruction->operand_count == 0u &&
                instruction->target_count == 0u;
        case RCC_X86_ADD: case RCC_X86_SUB: case RCC_X86_MUL:
        case RCC_X86_UDIV: case RCC_X86_SDIV: case RCC_X86_UREM:
        case RCC_X86_SREM: case RCC_X86_AND: case RCC_X86_OR:
        case RCC_X86_XOR: case RCC_X86_SHL: case RCC_X86_SHR:
        case RCC_X86_SAR: case RCC_X86_COMPARE_SET:
        case RCC_X86_GEP:
            return instruction->has_destination &&
                instruction->operand_count == 2u &&
                instruction->target_count == 0u;
        case RCC_X86_TRUNCATE: case RCC_X86_ZERO_EXTEND:
        case RCC_X86_SIGN_EXTEND: case RCC_X86_REINTERPRET:
        case RCC_X86_LOAD:
            return instruction->has_destination &&
                instruction->operand_count == 1u &&
                instruction->target_count == 0u;
        case RCC_X86_SELECT:
            return instruction->has_destination &&
                instruction->operand_count == 3u &&
                instruction->target_count == 0u;
        case RCC_X86_STORE:
            return !instruction->has_destination &&
                instruction->operand_count == 2u &&
                instruction->target_count == 0u;
        case RCC_X86_CALL:
            return instruction->target_count == 0u &&
                instruction->symbol && instruction->symbol[0];
        case RCC_X86_JUMP:
            return !instruction->has_destination &&
                instruction->operand_count == 0u &&
                instruction->target_count == 1u;
        case RCC_X86_JUMP_IF:
            return !instruction->has_destination &&
                instruction->operand_count == 1u &&
                instruction->target_count == 2u;
        case RCC_X86_RETURN:
            return !instruction->has_destination &&
                instruction->operand_count <= 1u &&
                instruction->target_count == 0u;
        case RCC_X86_TRAP:
            return !instruction->has_destination &&
                instruction->operand_count == 0u &&
                instruction->target_count == 0u;
        case RCC_X86_COPY:
            return false;
    }
    return false;
}

static bool x86_legal_instruction_shape(
    const RccX86LegalInstruction* instruction) {
    switch (instruction->opcode) {
        case RCC_X86_LEGAL_SELECTED:
            return x86_legal_selected_shape(instruction);
        case RCC_X86_LEGAL_COPY:
            return instruction->selected_opcode == RCC_X86_COPY &&
                instruction->has_destination &&
                instruction->operand_count == 1u &&
                instruction->target_count == 0u;
        case RCC_X86_LEGAL_PREPARE_UNSIGNED_DIVIDEND:
            return !instruction->has_destination &&
                instruction->operand_count == 0u &&
                instruction->target_count == 0u &&
                (instruction->selected_opcode == RCC_X86_UDIV ||
                 instruction->selected_opcode == RCC_X86_UREM);
        case RCC_X86_LEGAL_PREPARE_SIGNED_DIVIDEND:
            return !instruction->has_destination &&
                instruction->operand_count == 0u &&
                instruction->target_count == 0u &&
                (instruction->selected_opcode == RCC_X86_SDIV ||
                 instruction->selected_opcode == RCC_X86_SREM);
        case RCC_X86_LEGAL_DIVIDE:
            return x86_legal_is_division(instruction->selected_opcode) &&
                !instruction->has_destination &&
                instruction->operand_count == 1u &&
                instruction->target_count == 0u;
        case RCC_X86_LEGAL_SHIFT:
            return x86_legal_is_shift(instruction->selected_opcode) &&
                instruction->has_destination &&
                instruction->operand_count == 0u &&
                instruction->target_count == 0u;
    }
    return false;
}

static bool x86_legal_is_terminator(const RccX86LegalInstruction* item) {
    return item->opcode == RCC_X86_LEGAL_SELECTED &&
        (item->selected_opcode == RCC_X86_JUMP ||
         item->selected_opcode == RCC_X86_JUMP_IF ||
         item->selected_opcode == RCC_X86_RETURN ||
         item->selected_opcode == RCC_X86_TRAP);
}

static bool x86_legal_verify_divide(
    const RccX86LegalInstruction* instruction, const RccX86Abi* abi,
    char* error, size_t error_size) {
    const RccX86LegalInstruction* prepare = instruction->previous;
    const RccX86LegalInstruction* input = prepare ? prepare->previous : NULL;
    const RccX86LegalInstruction* output = instruction->next;
    RccX86HardwareGpr expected_result =
        instruction->selected_opcode == RCC_X86_UREM ||
        instruction->selected_opcode == RCC_X86_SREM
            ? abi->return_high : abi->return_low;
    bool signed_division =
        instruction->selected_opcode == RCC_X86_SDIV ||
        instruction->selected_opcode == RCC_X86_SREM;
    if (instruction->operand_count != 1u ||
        instruction->has_destination || !prepare || !input || !output ||
        input->opcode != RCC_X86_LEGAL_COPY ||
        !input->has_destination ||
        input->destination.kind != RCC_X86_VALUE_GPR ||
        input->destination.gpr != abi->return_low ||
        prepare->opcode !=
            (signed_division
                 ? RCC_X86_LEGAL_PREPARE_SIGNED_DIVIDEND
                 : RCC_X86_LEGAL_PREPARE_UNSIGNED_DIVIDEND) ||
        output->opcode != RCC_X86_LEGAL_COPY ||
        output->operand_count != 1u ||
        output->operands[0].kind != RCC_X86_VALUE_GPR ||
        output->operands[0].gpr != expected_result ||
        (instruction->operands[0].kind == RCC_X86_VALUE_GPR &&
         (instruction->operands[0].gpr == abi->return_low ||
          instruction->operands[0].gpr == abi->return_high))) {
        return x86_legal_error(error, error_size,
                               "x86 division fixed-register sequence is invalid");
    }
    return true;
}

static bool x86_legal_verify_shift(
    const RccX86LegalInstruction* instruction, const RccX86Abi* abi,
    char* error, size_t error_size) {
    const RccX86LegalInstruction* count = instruction->previous;
    const RccX86LegalInstruction* input = count ? count->previous : NULL;
    if (!instruction->has_destination || instruction->operand_count != 0u ||
        !count || !input || count->opcode != RCC_X86_LEGAL_COPY ||
        !count->has_destination ||
        count->destination.kind != RCC_X86_VALUE_GPR ||
        count->destination.gpr != abi->shift_count ||
        input->opcode != RCC_X86_LEGAL_COPY ||
        !input->has_destination || input->operand_count != 1u ||
        !x86_legal_value_equal(input->destination,
                               instruction->destination)) {
        return x86_legal_error(error, error_size,
                               "x86 shift fixed-register sequence is invalid");
    }
    return true;
}

bool rcc_x86_verify_legal_function(
    const RccX86LegalFunction* function,
    const RccMirRegisterPolicy* policy,
    char* error, size_t error_size) {
    RccX86Abi abi;
    const RccX86LegalBlock* block;
    size_t block_count = 0u;
    size_t instruction_count = 0u;
    if (error && error_size != 0u) error[0] = '\0';
    if (!function || !policy ||
        !rcc_x86_abi_for_target(function->target, &abi) ||
        !rcc_x86_abi_verify_policy(&abi, policy, error, error_size) ||
        function->pointer_size != abi.pointer_size ||
        function->stack_alignment != abi.stack_alignment ||
        function->frame_size % function->stack_alignment != 0u ||
        function->block_count < function->original_block_count) {
        return x86_legal_error(error, error_size,
                               "x86 legal-function header is invalid");
    }
    for (block = function->first_block; block; block = block->next) {
        const RccX86LegalInstruction* instruction;
        if (block->id != block_count || !block->first || !block->last) {
            return x86_legal_error(error, error_size,
                                   "x86 legal block is invalid");
        }
        for (instruction = block->first; instruction;
             instruction = instruction->next) {
            size_t operand;
            size_t target;
            ++instruction_count;
            if ((instruction->operand_count != 0u &&
                 (!instruction->operands ||
                  !instruction->operand_types)) ||
                (instruction->target_count != 0u &&
                 !instruction->targets) ||
                !x86_legal_instruction_shape(instruction) ||
                (instruction->has_destination &&
                 !x86_legal_value_valid(
                     instruction->destination, function, &abi)) ||
                (x86_legal_is_terminator(instruction) &&
                 instruction->next)) {
                return x86_legal_error(error, error_size,
                                       "x86 legal instruction is invalid");
            }
            for (operand = 0u; operand < instruction->operand_count;
                 ++operand) {
                if (!x86_legal_value_valid(
                        instruction->operands[operand], function, &abi)) {
                    return x86_legal_error(error, error_size,
                                           "x86 legal operand is invalid");
                }
            }
            for (target = 0u; target < instruction->target_count;
                 ++target) {
                if (instruction->targets[target] >= function->block_count) {
                    return x86_legal_error(error, error_size,
                                           "x86 legal target is invalid");
                }
            }
            if (instruction->opcode == RCC_X86_LEGAL_SELECTED &&
                x86_legal_native_type(instruction->type, &abi) &&
                (x86_legal_is_division(instruction->selected_opcode) ||
                 x86_legal_is_shift(instruction->selected_opcode))) {
                return x86_legal_error(error, error_size,
                                       "native fixed-register op was not legalized");
            }
            if (instruction->opcode == RCC_X86_LEGAL_DIVIDE &&
                !x86_legal_verify_divide(
                    instruction, &abi, error, error_size)) return false;
            if (instruction->opcode == RCC_X86_LEGAL_SHIFT &&
                !x86_legal_verify_shift(
                    instruction, &abi, error, error_size)) return false;
            if ((instruction->opcode ==
                     RCC_X86_LEGAL_PREPARE_UNSIGNED_DIVIDEND ||
                 instruction->opcode ==
                     RCC_X86_LEGAL_PREPARE_SIGNED_DIVIDEND) &&
                (!instruction->next ||
                 instruction->next->opcode != RCC_X86_LEGAL_DIVIDE)) {
                return x86_legal_error(
                    error, error_size,
                    "x86 dividend preparation is not followed by divide");
            }
        }
        if (!x86_legal_is_terminator(block->last)) {
            return x86_legal_error(error, error_size,
                                   "x86 legal block lacks a terminator");
        }
        ++block_count;
    }
    if (block_count != function->block_count ||
        instruction_count != function->legal_instruction_count) {
        return x86_legal_error(error, error_size,
                               "x86 legal counts are inconsistent");
    }
    return true;
}

void rcc_x86_legal_function_destroy(RccX86LegalFunction* function) {
    RccX86LegalBlock* block;
    if (!function) return;
    block = function->first_block;
    while (block) {
        RccX86LegalBlock* next_block = block->next;
        RccX86LegalInstruction* instruction = block->first;
        while (instruction) {
            RccX86LegalInstruction* next_instruction = instruction->next;
            rcc_free(instruction->operands);
            rcc_free(instruction->operand_types);
            rcc_free(instruction->targets);
            rcc_free(instruction->symbol);
            rcc_free(instruction);
            instruction = next_instruction;
        }
        rcc_free(block);
        block = next_block;
    }
    rcc_free(function);
}

bool rcc_x86_legalize_function(
    const RccX86Function* selected,
    const RccMirRegisterPolicy* policy,
    RccX86LegalFunction** legal_out,
    char* error, size_t error_size) {
    RccX86LegalFunction* legal = NULL;
    RccX86Abi abi;
    const RccX86Block* source_block;
    bool result = false;
    if (legal_out) *legal_out = NULL;
    if (error && error_size != 0u) error[0] = '\0';
    if (!selected || !policy || !legal_out ||
        !rcc_x86_verify_function(selected, policy, error, error_size) ||
        !rcc_x86_abi_for_target(selected->target, &abi)) {
        return false;
    }
    legal = rcc_alloc(sizeof(*legal));
    legal->target = selected->target;
    legal->pointer_size = selected->pointer_size;
    legal->stack_alignment = selected->stack_alignment;
    legal->frame_size = selected->frame_size;
    legal->original_block_count = selected->original_block_count;
    legal->source_instruction_count = selected->source_instruction_count;
    for (source_block = selected->first_block; source_block;
         source_block = source_block->next) {
        const RccX86Instruction* source;
        RccX86LegalBlock* block = x86_legal_append_block(
            legal, source_block);
        if (!block) goto cleanup;
        for (source = source_block->first; source; source = source->next) {
            if (x86_legal_is_division(source->opcode) &&
                x86_legal_native_type(source->type, &abi)) {
                if (!x86_legalize_division(
                        legal, block, source, &abi,
                        error, error_size)) goto cleanup;
            } else if (x86_legal_is_shift(source->opcode) &&
                       x86_legal_native_type(source->type, &abi)) {
                if (!x86_legalize_shift(
                        legal, block, source, &abi,
                        error, error_size)) goto cleanup;
            } else if (!x86_legal_clone_selected(
                           legal, block, source, &abi,
                           error, error_size)) {
                goto cleanup;
            }
        }
    }
    if (!rcc_x86_verify_legal_function(
            legal, policy, error, error_size)) goto cleanup;
    *legal_out = legal;
    legal = NULL;
    result = true;
cleanup:
    rcc_x86_legal_function_destroy(legal);
    return result;
}
