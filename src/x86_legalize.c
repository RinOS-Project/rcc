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

static size_t x86_legal_stack_argument_slots(
    RccMirType type, const RccX86Abi* abi) {
    size_t size = x86_legal_type_size(type, abi);
    return (size + abi->pointer_size - 1u) / abi->pointer_size;
}

static bool x86_legal_align(uint32_t value, uint16_t alignment,
                            uint32_t* result) {
    uint32_t mask;
    if (alignment == 0u || (alignment & (alignment - 1u)) != 0u) {
        return false;
    }
    mask = (uint32_t)alignment - 1u;
    if (value > UINT32_MAX - mask) return false;
    *result = (value + mask) & ~mask;
    return true;
}

static bool x86_legal_native_scalar(
    RccMirType type, const RccX86Abi* abi) {
    if (type.kind == RCC_MIR_TYPE_POINTER) return true;
    return type.kind == RCC_MIR_TYPE_INTEGER && type.bit_width != 0u &&
        type.bit_width <= (uint16_t)(abi->pointer_size * 8u);
}

static bool x86_legal_abi_scalar_parameter(
    RccMirType type, const RccX86Abi* abi) {
    return x86_legal_native_scalar(type, abi) ||
        ((abi->target == RCC_X86_TARGET_I686 ||
          abi->target == RCC_X86_TARGET_X86_64) &&
         type.kind == RCC_MIR_TYPE_FLOAT &&
         (type.bit_width == 32u || type.bit_width == 64u));
}

static bool x86_legal_type_supported(
    RccMirType type, const RccX86Abi* abi) {
    if (type.kind == RCC_MIR_TYPE_VOID) return type.bit_width == 0u;
    if (type.kind == RCC_MIR_TYPE_POINTER) return type.bit_width == 0u;
    if (type.kind == RCC_MIR_TYPE_FLOAT) {
        return (abi->target == RCC_X86_TARGET_I686 ||
                abi->target == RCC_X86_TARGET_X86_64) &&
            (type.bit_width == 32u || type.bit_width == 64u);
    }
    if (!x86_legal_native_scalar(type, abi)) return false;
    return type.bit_width == 1u || type.bit_width == 8u ||
        type.bit_width == 16u || type.bit_width == 32u ||
        type.bit_width == 64u;
}

static bool x86_legal_return_type_supported(
    RccMirType type, const RccX86Abi* abi) {
    if (type.kind == RCC_MIR_TYPE_FLOAT) {
        return x86_legal_type_supported(type, abi);
    }
    return (abi->target == RCC_X86_TARGET_I686 &&
            type.kind == RCC_MIR_TYPE_INTEGER &&
            type.bit_width == 64u) ||
        x86_legal_type_supported(type, abi);
}

static bool x86_legal_pair_return_supported(
    const RccX86Instruction* instruction, const RccX86Abi* abi) {
    uint16_t word_width = abi->target == RCC_X86_TARGET_I686 ? 32u : 64u;
    uint64_t metadata_low = abi->target == RCC_X86_TARGET_I686 ? 8u : 9u;
    uint64_t metadata_high = abi->target == RCC_X86_TARGET_I686 ? 8u : 16u;
    return instruction && instruction->operand_count == 2u &&
        instruction->immediate >= metadata_low &&
        instruction->immediate <= metadata_high &&
        instruction->operand_types[0].kind == RCC_MIR_TYPE_INTEGER &&
        instruction->operand_types[0].bit_width == word_width &&
        instruction->operand_types[1].kind == RCC_MIR_TYPE_INTEGER &&
        instruction->operand_types[1].bit_width == word_width;
}

static uint32_t x86_legal_hardware_callee_mask(const RccX86Abi* abi) {
    uint32_t mask = 0u;
    size_t index;
    for (index = 0u; index < abi->gpr_count; ++index) {
        if ((abi->callee_saved_abstract_mask &
             (UINT64_C(1) << index)) != 0u) {
            mask |= UINT32_C(1) << abi->gpr_map[index];
        }
    }
    return mask;
}

static bool x86_legal_note_selected_location(
    RccMirLocation location, const RccX86Abi* abi,
    uint32_t* used_mask, char* error, size_t error_size) {
    RccX86HardwareGpr hardware;
    if (location.kind != RCC_MIR_LOCATION_PHYSICAL ||
        location.register_class != RCC_MIR_REGCLASS_GPR) {
        return true;
    }
    if (!rcc_x86_abi_hardware_gpr(
            abi, location.physical_register, &hardware)) {
        return x86_legal_error(
            error, error_size,
            "x86 frame planner cannot resolve a physical register");
    }
    *used_mask |= UINT32_C(1) << hardware;
    return true;
}

static bool x86_legal_plan_callee_saves(
    RccX86LegalFunction* legal, const RccX86Function* selected,
    const RccX86Abi* abi, char* error, size_t error_size) {
    const RccX86Block* block;
    uint32_t selected_used = 0u;
    uint32_t offset;
    uint32_t end;
    size_t count = 0u;
    size_t index;
    for (index = 0u; index < selected->parameter_count; ++index) {
        if (!x86_legal_note_selected_location(
                selected->parameters[index], abi, &selected_used,
                error, error_size)) return false;
    }
    for (block = selected->first_block; block; block = block->next) {
        const RccX86Instruction* instruction;
        for (instruction = block->first; instruction;
             instruction = instruction->next) {
            size_t operand;
            if (instruction->has_destination &&
                !x86_legal_note_selected_location(
                    instruction->destination, abi, &selected_used,
                    error, error_size)) return false;
            for (operand = 0u; operand < instruction->operand_count;
                 ++operand) {
                if (!x86_legal_note_selected_location(
                        instruction->operands[operand], abi,
                        &selected_used, error, error_size)) return false;
            }
        }
    }
    legal->callee_saved_gpr_mask = selected_used &
        x86_legal_hardware_callee_mask(abi);
    for (index = 0u; index < 16u; ++index) {
        if ((legal->callee_saved_gpr_mask &
             (UINT32_C(1) << index)) != 0u) ++count;
    }
    legal->callee_save_area_offset = legal->frame_size;
    if (count != 0u) {
        if (count > SIZE_MAX / sizeof(*legal->callee_saves)) {
            return x86_legal_error(error, error_size,
                                   "x86 callee-save table is too large");
        }
        legal->callee_saves = rcc_alloc(
            count * sizeof(*legal->callee_saves));
    }
    offset = legal->callee_save_area_offset;
    for (index = 0u; index < 16u; ++index) {
        RccX86CalleeSave* save;
        if ((legal->callee_saved_gpr_mask &
             (UINT32_C(1) << index)) == 0u) continue;
        if (offset > UINT32_MAX - abi->pointer_size) {
            return x86_legal_error(error, error_size,
                                   "x86 callee-save area exceeds 32 bits");
        }
        save = &legal->callee_saves[legal->callee_save_count++];
        save->gpr = (RccX86HardwareGpr)index;
        save->frame_offset = offset;
        offset += abi->pointer_size;
    }
    legal->callee_save_area_size =
        offset - legal->callee_save_area_offset;
    if (!x86_legal_align(offset, abi->stack_alignment, &end)) {
        return x86_legal_error(error, error_size,
                               "x86 callee-save frame exceeds 32 bits");
    }
    legal->frame_size = end;
    return true;
}

static bool x86_legal_value_equal(RccX86Value left, RccX86Value right) {
    if (left.kind != right.kind) return false;
    if (left.kind == RCC_X86_VALUE_GPR) return left.gpr == right.gpr;
    if (left.kind == RCC_X86_VALUE_FPR) return left.fpr == right.fpr;
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

static RccX86Value x86_legal_fixed_fpr(
    uint16_t fpr, RccMirType type, const RccX86Abi* abi) {
    RccX86Value value;
    memset(&value, 0, sizeof(value));
    value.kind = RCC_X86_VALUE_FPR;
    value.fpr = fpr;
    value.size = x86_legal_type_size(type, abi);
    value.alignment = value.size;
    return value;
}

static RccX86Value x86_legal_argument_value(
    RccX86ValueKind kind, uint32_t offset, RccMirType type,
    const RccX86Abi* abi) {
    RccX86Value value;
    memset(&value, 0, sizeof(value));
    value.kind = kind;
    value.frame_offset = offset;
    value.size = x86_legal_type_size(type, abi);
    value.alignment = abi->pointer_size;
    return value;
}

static bool x86_legal_resolve_location(
    RccMirLocation location, RccMirType type, const RccX86Abi* abi,
    RccX86Value* value, char* error, size_t error_size) {
    memset(value, 0, sizeof(*value));
    if (location.kind == RCC_MIR_LOCATION_PHYSICAL) {
        if (location.register_class == RCC_MIR_REGCLASS_FPR &&
            type.kind == RCC_MIR_TYPE_FLOAT &&
            ((abi->target == RCC_X86_TARGET_X86_64 &&
              location.physical_register < abi->fpr_count) ||
             (abi->target == RCC_X86_TARGET_I686 &&
              location.physical_register == 0u))) {
            value->kind = RCC_X86_VALUE_FPR;
            value->fpr = location.physical_register;
            value->size = x86_legal_type_size(type, abi);
            value->alignment = value->size;
            return true;
        }
        if (location.register_class != RCC_MIR_REGCLASS_GPR ||
            type.kind == RCC_MIR_TYPE_FLOAT ||
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

static void x86_legal_tag_source(
    RccX86LegalBlock* block, RccX86LegalInstruction* previous,
    const RccX86Instruction* source) {
    RccX86LegalInstruction* instruction;
    if (!block || !source ||
        (!source->source_statement && !source->source_declaration)) return;
    instruction = previous ? previous->next : block->first;
    while (instruction) {
        instruction->source_statement = source->source_statement;
        instruction->source_declaration = source->source_declaration;
        instruction = instruction->next;
    }
}

static bool x86_legal_append_copy(
    RccX86LegalFunction* function, RccX86LegalBlock* block,
    RccMirType type, RccX86Value source, RccX86Value destination,
    char* error, size_t error_size) {
    if (!x86_legal_append(
            function, block, RCC_X86_LEGAL_COPY, RCC_X86_COPY,
            type, &destination, &source, &type, 1u)) {
        return x86_legal_error(error, error_size,
                               "x86 legal copy table is too large");
    }
    return true;
}

static bool x86_legal_reserve_parallel_temporary(
    RccX86LegalFunction* function, const RccX86Abi* abi,
    RccX86Value* temporary, char* error, size_t error_size) {
    uint32_t offset;
    uint32_t end;
    if (!function->has_parallel_copy_temporary) {
        if (function->frame_plan_complete) {
            return x86_legal_error(
                error, error_size,
                "x86 parallel-copy temporary was not reserved");
        }
        if (!x86_legal_align(function->frame_size, abi->pointer_size,
                             &offset) ||
            offset > UINT32_MAX - abi->pointer_size ||
            !x86_legal_align(offset + abi->pointer_size,
                             abi->stack_alignment, &end)) {
            return x86_legal_error(
                error, error_size,
                "x86 parallel-copy temporary exceeds the frame");
        }
        function->has_parallel_copy_temporary = true;
        function->parallel_copy_temporary_offset = offset;
        function->frame_size = end;
    }
    memset(temporary, 0, sizeof(*temporary));
    temporary->kind = RCC_X86_VALUE_FRAME;
    temporary->frame_offset = function->parallel_copy_temporary_offset;
    temporary->size = abi->pointer_size;
    temporary->alignment = abi->pointer_size;
    return true;
}

static bool x86_legal_schedule_parallel_copies(
    RccX86LegalFunction* function, RccX86LegalBlock* block,
    const RccX86Abi* abi, const RccX86Value* input_sources,
    const RccX86Value* input_destinations,
    const RccMirType* types, size_t count,
    char* error, size_t error_size) {
    RccX86Value* sources;
    RccX86Value* destinations;
    bool* pending;
    size_t remaining = 0u;
    size_t index;
    if (count == 0u) return true;
    if (!input_sources || !input_destinations || !types ||
        count > (size_t)-1 / sizeof(*sources) ||
        count > (size_t)-1 / sizeof(*pending)) {
        return x86_legal_error(error, error_size,
                               "x86 parallel-copy table is too large");
    }
    sources = rcc_alloc(count * sizeof(*sources));
    destinations = rcc_alloc(count * sizeof(*destinations));
    pending = rcc_alloc(count * sizeof(*pending));
    memcpy(sources, input_sources, count * sizeof(*sources));
    memcpy(destinations, input_destinations,
           count * sizeof(*destinations));
    for (index = 0u; index < count; ++index) {
        if (!x86_legal_value_equal(sources[index], destinations[index])) {
            pending[index] = true;
            ++remaining;
        }
    }
    while (remaining != 0u) {
        bool progress = false;
        for (index = 0u; index < count; ++index) {
            bool destination_is_source = false;
            size_t other;
            if (!pending[index]) continue;
            for (other = 0u; other < count; ++other) {
                if (pending[other] && other != index &&
                    x86_legal_value_equal(destinations[index],
                                          sources[other])) {
                    destination_is_source = true;
                    break;
                }
            }
            if (destination_is_source) continue;
            if (!x86_legal_append_copy(
                    function, block, types[index], sources[index],
                    destinations[index], error, error_size)) goto fail;
            pending[index] = false;
            --remaining;
            progress = true;
        }
        if (!progress) {
            RccX86Value temporary;
            RccMirType temporary_type;
            uint16_t temporary_size;
            size_t other;
            for (index = 0u; index < count; ++index) {
                if (pending[index]) break;
            }
            if (index == count) goto fail;
            temporary_type = types[index];
            temporary_size = x86_legal_type_size(temporary_type, abi);
            /* Save the value being overwritten, not the width of the copy
             * which happened to expose the cycle.  A narrow argument can
             * target a register whose pending source is a full-width pointer
             * (or scalar); spilling with the narrow type would truncate that
             * incoming value before its dependent copy executes. */
            for (other = 0u; other < count; ++other) {
                uint16_t source_size;
                if (!pending[other] ||
                    !x86_legal_value_equal(
                        sources[other], destinations[index])) {
                    continue;
                }
                source_size = x86_legal_type_size(types[other], abi);
                if (source_size > temporary_size) {
                    temporary_type = types[other];
                    temporary_size = source_size;
                }
            }
            if (!x86_legal_reserve_parallel_temporary(
                    function, abi, &temporary,
                    error, error_size) ||
                !x86_legal_append_copy(
                    function, block, temporary_type, destinations[index],
                    temporary, error, error_size)) goto fail;
            for (other = 0u; other < count; ++other) {
                if (pending[other] &&
                    x86_legal_value_equal(sources[other],
                                          destinations[index])) {
                    sources[other] = temporary;
                }
            }
        }
    }
    rcc_free(sources);
    rcc_free(destinations);
    rcc_free(pending);
    return true;
fail:
    rcc_free(sources);
    rcc_free(destinations);
    rcc_free(pending);
    return false;
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
    destination->symbol_is_code = source->symbol_is_code;
    destination->symbol_is_tls = source->symbol_is_tls;
    destination->has_callee = source->has_callee;
    destination->sysv_variadic_call = source->sysv_variadic_call;
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

static bool x86_legal_is_binary(RccX86Opcode opcode) {
    return opcode == RCC_X86_ADD || opcode == RCC_X86_SUB ||
        opcode == RCC_X86_MUL || opcode == RCC_X86_AND ||
        opcode == RCC_X86_OR || opcode == RCC_X86_XOR ||
        opcode == RCC_X86_FADD || opcode == RCC_X86_FSUB ||
        opcode == RCC_X86_FMUL || opcode == RCC_X86_FDIV;
}

static bool x86_legal_is_float_binary(RccX86Opcode opcode) {
    return opcode >= RCC_X86_FADD && opcode <= RCC_X86_FDIV;
}

static bool x86_legal_native_type(
    RccMirType type, const RccX86Abi* abi) {
    return type.kind == RCC_MIR_TYPE_INTEGER &&
        type.bit_width != 0u &&
        type.bit_width <= (uint16_t)(abi->pointer_size * 8u);
}

static bool x86_legal_float_type(
    RccMirType type, const RccX86Abi* abi) {
    return (abi->target == RCC_X86_TARGET_I686 ||
            abi->target == RCC_X86_TARGET_X86_64) &&
        type.kind == RCC_MIR_TYPE_FLOAT &&
        (type.bit_width == 32u || type.bit_width == 64u);
}

static const RccSysvMemoryArgument* x86_memory_argument_for_operand(
    const RccX86Instruction* instruction, size_t operand_index) {
    for (size_t index = 0u;
         index < instruction->sysv_memory_argument_count; ++index) {
        const RccSysvMemoryArgument* argument =
            &instruction->sysv_memory_arguments[index];
        if (operand_index >= argument->first_operand &&
            operand_index - argument->first_operand <
                argument->operand_count) {
            return argument;
        }
    }
    return NULL;
}

static const RccSysvMemoryArgument* x86_memory_parameter_for_operand(
    const RccX86Function* function, size_t parameter_index) {
    for (size_t index = 0u;
         index < function->sysv_memory_parameter_count; ++index) {
        const RccSysvMemoryArgument* parameter =
            &function->sysv_memory_parameters[index];
        if (parameter_index >= parameter->first_operand &&
            parameter_index - parameter->first_operand <
                parameter->operand_count) {
            return parameter;
        }
    }
    return NULL;
}

static bool x86_legal_call_supported(
    const RccX86Instruction* instruction, const RccX86Abi* abi) {
    size_t index;
    if (instruction->opcode != RCC_X86_CALL ||
        (instruction->sysv_variadic_call &&
         abi->target != RCC_X86_TARGET_X86_64) ||
        (instruction->type.kind != RCC_MIR_TYPE_VOID &&
         !x86_legal_abi_scalar_parameter(instruction->type, abi)) ||
        (instruction->immediate != 0u &&
         (abi->target != RCC_X86_TARGET_I686 ||
          instruction->immediate != abi->pointer_size)) ||
        !rcc_sysv_memory_arguments_valid(
            instruction->sysv_memory_arguments,
            instruction->sysv_memory_argument_count,
            instruction->operand_count) ||
        (instruction->sysv_memory_argument_count != 0u &&
         abi->target != RCC_X86_TARGET_X86_64)) {
        return false;
    }
    for (index = 0u; index < instruction->operand_count; ++index) {
        if (x86_memory_argument_for_operand(instruction, index)) {
            if (instruction->operand_types[index].kind !=
                    RCC_MIR_TYPE_INTEGER ||
                instruction->operand_types[index].bit_width != 64u) {
                return false;
            }
            continue;
        }
        if (!x86_legal_abi_scalar_parameter(
                instruction->operand_types[index], abi)) return false;
    }
    return true;
}

static bool x86_legal_call_stack_bytes(
    const RccX86Instruction* instruction, const RccX86Abi* abi,
    uint32_t* bytes_out, uint32_t* alignment_out) {
    size_t stack_count;
    size_t integer_index = 0u;
    size_t floating_index = 0u;
    if (!alignment_out ||
        !x86_legal_call_supported(instruction, abi)) return false;
    *alignment_out = abi->stack_alignment;
    stack_count = 0u;
    for (size_t index = 0u; index < instruction->operand_count;) {
        const RccSysvMemoryArgument* memory_argument =
            x86_memory_argument_for_operand(instruction, index);
        if (memory_argument) {
            if (index == memory_argument->first_operand) {
                if (memory_argument->alignment > *alignment_out) {
                    *alignment_out = memory_argument->alignment;
                }
                size_t alignment_slots =
                    memory_argument->alignment / abi->pointer_size;
                size_t remainder;
                if (alignment_slots == 0u) return false;
                remainder = stack_count % alignment_slots;
                if (remainder != 0u) {
                    size_t padding = alignment_slots - remainder;
                    if (padding > SIZE_MAX - stack_count) return false;
                    stack_count += padding;
                }
            }
            ++stack_count;
            ++index;
            continue;
        }
        if (instruction->operand_types[index].kind == RCC_MIR_TYPE_FLOAT) {
            if (floating_index >= abi->floating_argument_count) {
                size_t slots = x86_legal_stack_argument_slots(
                    instruction->operand_types[index], abi);
                if (slots > SIZE_MAX - stack_count) return false;
                stack_count += slots;
            }
            ++floating_index;
        } else {
            if (integer_index >= abi->integer_argument_count) {
                ++stack_count;
            }
            ++integer_index;
        }
        ++index;
    }
    if (stack_count > UINT32_MAX / abi->pointer_size) return false;
    *bytes_out = (uint32_t)stack_count * abi->pointer_size;
    return true;
}

static bool x86_legal_selected_location_equal(
    RccMirLocation left, RccMirLocation right) {
    if (left.kind != right.kind ||
        left.register_class != right.register_class) return false;
    if (left.kind == RCC_MIR_LOCATION_PHYSICAL) {
        return left.physical_register == right.physical_register;
    }
    return left.spill_offset == right.spill_offset &&
        left.spill_size == right.spill_size;
}

static bool x86_legal_prepare_outgoing_frame(
    RccX86LegalFunction* legal, const RccX86Function* selected,
    const RccX86Abi* abi, char* error, size_t error_size) {
    const RccX86Block* block;
    uint32_t maximum = 0u;
    uint32_t aligned;
    bool may_need_temporary =
        abi->integer_argument_count != 0u &&
        selected->parameter_count > 1u;
    for (block = selected->first_block; block; block = block->next) {
        const RccX86Instruction* instruction;
        for (instruction = block->first; instruction;
             instruction = instruction->next) {
            uint32_t bytes;
            uint32_t stack_alignment;
            if (instruction->opcode == RCC_X86_CALL &&
                x86_legal_call_supported(instruction, abi)) {
                if (instruction->has_callee) may_need_temporary = true;
                if ((abi->integer_argument_count > 1u ||
                     abi->floating_argument_count > 1u) &&
                    instruction->operand_count > 1u) {
                    may_need_temporary = true;
                }
                if (!x86_legal_call_stack_bytes(
                        instruction, abi, &bytes, &stack_alignment) ||
                    (stack_alignment > abi->stack_alignment &&
                     bytes > (uint32_t)INT32_MAX -
                         (stack_alignment - 1u)) ||
                    !x86_legal_align(
                        bytes, abi->stack_alignment, &bytes)) {
                    return x86_legal_error(
                        error, error_size,
                        "x86 outgoing argument area exceeds 32 bits");
                }
                if (bytes > maximum) maximum = bytes;
            }
            if (instruction->opcode == RCC_X86_RETURN &&
                instruction->operand_count == 2u) {
                may_need_temporary = true;
            }
            if (instruction->has_destination &&
                instruction->operand_count == 2u &&
                x86_legal_is_binary(instruction->opcode) &&
                (x86_legal_native_type(instruction->type, abi) ||
                 (x86_legal_is_float_binary(instruction->opcode) &&
                  x86_legal_float_type(instruction->type, abi))) &&
                x86_legal_selected_location_equal(
                    instruction->destination,
                    instruction->operands[1]) &&
                !x86_legal_selected_location_equal(
                    instruction->destination,
                    instruction->operands[0])) {
                may_need_temporary = true;
            }
        }
    }
    if (may_need_temporary) {
        RccX86Value temporary;
        if (!x86_legal_reserve_parallel_temporary(
                legal, abi, &temporary, error, error_size)) return false;
    }
    legal->outgoing_stack_size = maximum;
    legal->outgoing_stack_offset = legal->frame_size;
    if (legal->frame_size > UINT32_MAX - maximum ||
        !x86_legal_align(legal->frame_size + maximum,
                         abi->stack_alignment, &aligned)) {
        return x86_legal_error(error, error_size,
                               "x86 call frame exceeds 32 bits");
    }
    legal->frame_size = aligned;
    return true;
}

static bool x86_legal_complete_frame_plan(
    RccX86LegalFunction* legal, const RccX86Abi* abi,
    char* error, size_t error_size) {
    uint32_t consumed;
    uint32_t remainder;
    uint32_t padding;
    consumed = (uint32_t)abi->pointer_size * 2u;
    remainder = ((legal->frame_size % abi->stack_alignment) +
                 (consumed % abi->stack_alignment)) %
        abi->stack_alignment;
    padding = remainder == 0u
        ? 0u : (uint32_t)abi->stack_alignment - remainder;
    if (legal->frame_size > UINT32_MAX - padding) {
        return x86_legal_error(error, error_size,
                               "x86 stack adjustment exceeds 32 bits");
    }
    legal->stack_alignment_padding = padding;
    legal->stack_adjustment = legal->frame_size + padding;
    legal->frame_plan_complete = true;
    return true;
}

static uint32_t x86_legal_collect_used_gprs(
    const RccX86LegalFunction* function) {
    const RccX86LegalBlock* block;
    uint32_t mask = 0u;
    for (block = function->first_block; block; block = block->next) {
        const RccX86LegalInstruction* instruction;
        for (instruction = block->first; instruction;
             instruction = instruction->next) {
            size_t operand;
            if (instruction->has_destination &&
                instruction->destination.kind == RCC_X86_VALUE_GPR) {
                mask |= UINT32_C(1) << instruction->destination.gpr;
            }
            for (operand = 0u; operand < instruction->operand_count;
                 ++operand) {
                if (instruction->operands[operand].kind ==
                    RCC_X86_VALUE_GPR) {
                    mask |= UINT32_C(1) <<
                        instruction->operands[operand].gpr;
                }
            }
        }
    }
    return mask;
}

static bool x86_legalize_parameters(
    RccX86LegalFunction* legal, RccX86LegalBlock* block,
    const RccX86Function* selected, const RccX86Abi* abi,
    char* error, size_t error_size) {
    RccX86Value* sources = NULL;
    RccX86Value* destinations = NULL;
    uint32_t incoming_offset = 0u;
    size_t integer_index = 0u;
    size_t floating_index = 0u;
    size_t index;
    if (selected->parameter_count == 0u) {
        legal->parameter_ingress_complete = true;
        return true;
    }
    for (index = 0u; index < selected->parameter_count; ++index) {
        if (!x86_legal_abi_scalar_parameter(
                selected->parameter_types[index], abi)) {
            return true;
        }
    }
    if (selected->parameter_count >
        (size_t)-1 / sizeof(*sources)) {
        return x86_legal_error(error, error_size,
                               "x86 parameter copy table is too large");
    }
    sources = rcc_alloc(selected->parameter_count * sizeof(*sources));
    destinations = rcc_alloc(
        selected->parameter_count * sizeof(*destinations));
    for (index = 0u; index < selected->parameter_count; ++index) {
        RccMirType type = selected->parameter_types[index];
        const RccSysvMemoryArgument* memory_parameter =
            x86_memory_parameter_for_operand(selected, index);
        bool floating = type.kind == RCC_MIR_TYPE_FLOAT;
        bool in_register = floating
            ? floating_index < abi->floating_argument_count
            : integer_index < abi->integer_argument_count;
        if (memory_parameter) {
            if (index == memory_parameter->first_operand) {
                uint32_t alignment = memory_parameter->alignment;
                uint32_t remainder;
                if (alignment < abi->pointer_size ||
                    alignment % abi->pointer_size != 0u) {
                    rcc_free(sources);
                    rcc_free(destinations);
                    return x86_legal_error(
                        error, error_size,
                        "x86 incoming aggregate alignment is invalid");
                }
                remainder = incoming_offset % alignment;
                if (remainder != 0u) {
                    uint32_t padding = alignment - remainder;
                    if (padding > UINT32_MAX - incoming_offset) {
                        rcc_free(sources);
                        rcc_free(destinations);
                        return x86_legal_error(
                            error, error_size,
                            "x86 incoming aggregate area exceeds 32 bits");
                    }
                    incoming_offset += padding;
                }
            }
            if (incoming_offset > UINT32_MAX - abi->pointer_size) {
                rcc_free(sources);
                rcc_free(destinations);
                return x86_legal_error(
                    error, error_size,
                    "x86 incoming argument area exceeds 32 bits");
            }
            sources[index] = x86_legal_argument_value(
                RCC_X86_VALUE_INCOMING_ARGUMENT,
                incoming_offset, type, abi);
            incoming_offset += abi->pointer_size;
        } else if (floating) {
            if (in_register) {
                sources[index] = x86_legal_fixed_fpr(
                    abi->floating_arguments[floating_index], type, abi);
            }
            ++floating_index;
        } else {
            if (in_register) {
                sources[index] = x86_legal_fixed_gpr(
                    abi->integer_arguments[integer_index], type, abi);
            }
            ++integer_index;
        }
        if (!in_register && !memory_parameter) {
            {
                uint32_t argument_size = (uint32_t)(
                    x86_legal_stack_argument_slots(type, abi) *
                    abi->pointer_size);
                if (incoming_offset > UINT32_MAX - argument_size) {
                    rcc_free(sources);
                    rcc_free(destinations);
                    return x86_legal_error(
                        error, error_size,
                        "x86 incoming argument area exceeds 32 bits");
                }
                sources[index] = x86_legal_argument_value(
                    RCC_X86_VALUE_INCOMING_ARGUMENT,
                    incoming_offset, type, abi);
                incoming_offset += argument_size;
            }
        }
        if (!x86_legal_resolve_location(
                selected->parameters[index], type, abi,
                &destinations[index], error, error_size)) {
            rcc_free(sources);
            rcc_free(destinations);
            return false;
        }
    }
    if (!x86_legal_schedule_parallel_copies(
            legal, block, abi, sources, destinations,
            selected->parameter_types, selected->parameter_count,
            error, error_size)) {
        rcc_free(sources);
        rcc_free(destinations);
        return false;
    }
    rcc_free(sources);
    rcc_free(destinations);
    legal->parameter_ingress_complete = true;
    return true;
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

static bool x86_legalize_binary(
    RccX86LegalFunction* function, RccX86LegalBlock* block,
    const RccX86Instruction* source, const RccX86Abi* abi,
    char* error, size_t error_size) {
    RccX86Value destination;
    RccX86Value* operands = NULL;
    RccX86Value right;
    RccX86LegalInstruction* binary;
    if (!x86_legal_resolve_instruction(
            source, abi, &destination, &operands,
            error, error_size)) return false;
    right = operands[1];
    if (x86_legal_value_equal(destination, right) &&
        !x86_legal_value_equal(destination, operands[0])) {
        RccX86Value temporary;
        if (!x86_legal_reserve_parallel_temporary(
                function, abi, &temporary, error, error_size) ||
            !x86_legal_append_copy(
                function, block, source->operand_types[1], right,
                temporary, error, error_size)) {
            rcc_free(operands);
            return false;
        }
        right = temporary;
    }
    if (!x86_legal_append_copy(
            function, block, source->operand_types[0], operands[0],
            destination, error, error_size)) {
        rcc_free(operands);
        return false;
    }
    binary = x86_legal_append(
        function, block, RCC_X86_LEGAL_BINARY, source->opcode,
        source->type, &destination, &right,
        &source->operand_types[1], 1u);
    rcc_free(operands);
    if (!binary) {
        return x86_legal_error(error, error_size,
                               "x86 binary legalization is too large");
    }
    return true;
}

static bool x86_legalize_call(
    RccX86LegalFunction* function, RccX86LegalBlock* block,
    const RccX86Instruction* source, const RccX86Abi* abi,
    char* error, size_t error_size) {
    RccX86Value destination;
    RccX86Value* operands = NULL;
    RccX86Value* argument_destinations = NULL;
    uint32_t stack_bytes;
    uint32_t stack_index = 0u;
    size_t integer_index = 0u;
    size_t floating_index = 0u;
    uint8_t vector_argument_count = 0u;
    size_t index;
    RccX86LegalInstruction* call;
    RccX86Value callee;
    RccX86Value callee_temporary;
    uint32_t stack_alignment;
    if (!x86_legal_call_stack_bytes(
            source, abi, &stack_bytes, &stack_alignment) ||
        !x86_legal_resolve_instruction(
            source, abi, &destination, &operands,
            error, error_size)) {
        return false;
    }
    if (source->has_callee &&
        !x86_legal_resolve_location(
            source->callee, rcc_mir_type_pointer(), abi, &callee,
            error, error_size)) {
        rcc_free(operands);
        return false;
    }
    if (source->has_callee &&
        (!x86_legal_reserve_parallel_temporary(
             function, abi, &callee_temporary, error, error_size) ||
         !x86_legal_append_copy(
             function, block, rcc_mir_type_pointer(), callee,
             callee_temporary, error, error_size))) {
        rcc_free(operands);
        return false;
    }
    if (source->operand_count != 0u) {
        argument_destinations = rcc_alloc(
            source->operand_count * sizeof(*argument_destinations));
        for (index = 0u; index < source->operand_count; ++index) {
            RccMirType type = source->operand_types[index];
            const RccSysvMemoryArgument* memory_argument =
                x86_memory_argument_for_operand(source, index);
            bool floating = type.kind == RCC_MIR_TYPE_FLOAT;
            bool in_register = !memory_argument && (floating
                ? floating_index < abi->floating_argument_count
                : integer_index < abi->integer_argument_count);
            if (memory_argument &&
                index == memory_argument->first_operand) {
                size_t alignment_slots =
                    memory_argument->alignment / abi->pointer_size;
                size_t remainder = stack_index % alignment_slots;
                if (remainder != 0u) {
                    size_t padding = alignment_slots - remainder;
                    if (padding > UINT32_MAX - stack_index) {
                        rcc_free(argument_destinations);
                        rcc_free(operands);
                        return x86_legal_error(
                            error, error_size,
                            "x86 outgoing argument alignment exceeds 32 bits");
                    }
                    stack_index += (uint32_t)padding;
                }
            }
            if (memory_argument) {
                /* MEMORY-class aggregate words are copied contiguously to
                 * the caller's stack area and consume no argument registers. */
            } else if (floating) {
                if (in_register) {
                    argument_destinations[index] = x86_legal_fixed_fpr(
                        abi->floating_arguments[floating_index], type, abi);
                    if (source->sysv_variadic_call) {
                        ++vector_argument_count;
                    }
                }
                ++floating_index;
            } else {
                if (in_register) {
                    argument_destinations[index] = x86_legal_fixed_gpr(
                        abi->integer_arguments[integer_index], type, abi);
                }
                ++integer_index;
            }
            if (!in_register) {
                size_t stack_slots = x86_legal_stack_argument_slots(
                    type, abi);
                if (stack_index > UINT32_MAX / abi->pointer_size ||
                    stack_slots > UINT32_MAX / abi->pointer_size -
                        stack_index) {
                    rcc_free(argument_destinations);
                    rcc_free(operands);
                    return x86_legal_error(
                        error, error_size,
                        "x86 outgoing argument offset exceeds 32 bits");
                }
                argument_destinations[index] = x86_legal_argument_value(
                    RCC_X86_VALUE_OUTGOING_ARGUMENT,
                    function->outgoing_stack_offset +
                        stack_index * abi->pointer_size,
                    type, abi);
                stack_index += (uint32_t)stack_slots;
            }
        }
        if (!x86_legal_schedule_parallel_copies(
                function, block, abi, operands, argument_destinations,
                source->operand_types, source->operand_count,
                error, error_size)) {
            rcc_free(argument_destinations);
            rcc_free(operands);
            return false;
        }
        rcc_free(argument_destinations);
    }
    call = x86_legal_append(
        function, block, RCC_X86_LEGAL_CALL, RCC_X86_CALL,
        source->type, NULL, NULL, NULL, 0u);
    if (!call || !x86_legal_copy_selected_metadata(call, source)) {
        rcc_free(operands);
        return x86_legal_error(error, error_size,
                               "x86 legal call is too large");
    }
    if (source->has_callee) call->callee = callee_temporary;
    call->auxiliary = stack_bytes;
    call->sysv_variadic_call = source->sysv_variadic_call;
    call->sysv_vector_argument_count = vector_argument_count;
    call->sysv_stack_alignment = stack_alignment;
    if (source->has_destination) {
        RccX86Value result = source->type.kind == RCC_MIR_TYPE_FLOAT
            ? x86_legal_fixed_fpr(0u, source->type, abi)
            : x86_legal_fixed_gpr(abi->return_low, source->type, abi);
        if (!x86_legal_append_copy(
                function, block, source->type, result, destination,
                error, error_size)) {
            rcc_free(operands);
            return false;
        }
    }
    rcc_free(operands);
    return true;
}

static bool x86_legalize_return(
    RccX86LegalFunction* function, RccX86LegalBlock* block,
    const RccX86Instruction* source, const RccX86Abi* abi,
    char* error, size_t error_size) {
    RccX86Value destination;
    RccX86Value* operands = NULL;
    RccX86LegalInstruction* result;
    bool pair_return = source->operand_count == 2u;
    if ((pair_return && !x86_legal_pair_return_supported(source, abi)) ||
        (!pair_return && source->immediate != 0u &&
         (abi->target != RCC_X86_TARGET_I686 ||
          source->immediate != abi->pointer_size))) {
        return x86_legal_error(error, error_size,
                               "x86 return stack pop is invalid");
    }
    if (!x86_legal_resolve_instruction(
            source, abi, &destination, &operands,
            error, error_size)) return false;
    if (pair_return) {
        RccX86Value destinations[2];
        destinations[0] = x86_legal_fixed_gpr(
            abi->return_low, source->operand_types[0], abi);
        destinations[1] = x86_legal_fixed_gpr(
            abi->return_high, source->operand_types[1], abi);
        if (!x86_legal_schedule_parallel_copies(
                function, block, abi, operands, destinations,
                source->operand_types, 2u, error, error_size)) {
            rcc_free(operands);
            return false;
        }
    } else if (source->operand_count == 1u) {
        RccMirType type = source->operand_types[0];
        RccX86Value return_register = type.kind == RCC_MIR_TYPE_FLOAT
            ? x86_legal_fixed_fpr(0u, type, abi)
            : x86_legal_fixed_gpr(abi->return_low, type, abi);
        if (!x86_legal_append_copy(
                function, block, type, operands[0], return_register,
                error, error_size)) {
            rcc_free(operands);
            return false;
        }
    }
    result = x86_legal_append(
        function, block, RCC_X86_LEGAL_RETURN, RCC_X86_RETURN,
        function->return_type, NULL, NULL, NULL, 0u);
    if (result) {
        result->immediate = pair_return &&
                abi->target == RCC_X86_TARGET_I686
            ? 0u : source->immediate;
        result->auxiliary = pair_return ? 2u :
            (source->operand_count == 1u ? 1u : 0u);
    }
    rcc_free(operands);
    if (!result) {
        return x86_legal_error(error, error_size,
                               "x86 legal return is too large");
    }
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
    if (value.kind == RCC_X86_VALUE_FPR) {
        return ((abi->target == RCC_X86_TARGET_X86_64 &&
                 value.fpr < abi->fpr_count) ||
                (abi->target == RCC_X86_TARGET_I686 &&
                 value.fpr == 0u)) &&
            (value.size == 4u || value.size == 8u) &&
            value.alignment == value.size;
    }
    if (value.frame_offset % value.alignment != 0u) return false;
    if (value.kind == RCC_X86_VALUE_FRAME) {
        if (value.frame_offset <= function->source_frame_size &&
            value.size <= function->source_frame_size -
                value.frame_offset) return true;
        return function->has_parallel_copy_temporary &&
            value.frame_offset ==
                function->parallel_copy_temporary_offset &&
            value.size == abi->pointer_size &&
            value.alignment == abi->pointer_size;
    }
    if (value.kind == RCC_X86_VALUE_OUTGOING_ARGUMENT) {
        uint32_t relative;
        if (value.frame_offset < function->outgoing_stack_offset) {
            return false;
        }
        relative = value.frame_offset - function->outgoing_stack_offset;
        return relative <= function->outgoing_stack_size &&
            value.size <= function->outgoing_stack_size - relative;
    }
    return value.kind == RCC_X86_VALUE_INCOMING_ARGUMENT &&
        value.alignment == abi->pointer_size &&
        value.size <= (abi->pointer_size < 8u
            ? 8u : abi->pointer_size) &&
        value.frame_offset <= UINT32_MAX - value.size;
}

static bool x86_legal_selected_shape(
    const RccX86LegalInstruction* instruction) {
    switch (instruction->selected_opcode) {
        case RCC_X86_MOV_IMMEDIATE:
        case RCC_X86_STACK_ADDRESS:
        case RCC_X86_FRAME_ADDRESS:
        case RCC_X86_SYMBOL_ADDRESS:
            return instruction->has_destination &&
                instruction->operand_count == 0u &&
                instruction->target_count == 0u &&
                (instruction->selected_opcode != RCC_X86_STACK_ADDRESS ||
                 (instruction->immediate <= INT32_MAX &&
                  instruction->auxiliary != 0u &&
                  instruction->auxiliary <= 4096u &&
                  (instruction->auxiliary &
                   (instruction->auxiliary - 1u)) == 0u)) &&
                (instruction->selected_opcode != RCC_X86_FRAME_ADDRESS ||
                 instruction->immediate <= INT32_MAX) &&
                (instruction->selected_opcode != RCC_X86_SYMBOL_ADDRESS ||
                 (instruction->symbol && instruction->symbol[0]));
        case RCC_X86_ADD: case RCC_X86_SUB: case RCC_X86_MUL:
        case RCC_X86_UDIV: case RCC_X86_SDIV: case RCC_X86_UREM:
        case RCC_X86_SREM: case RCC_X86_AND: case RCC_X86_OR:
        case RCC_X86_XOR: case RCC_X86_SHL: case RCC_X86_SHR:
        case RCC_X86_SAR: case RCC_X86_COMPARE_SET:
        case RCC_X86_FADD: case RCC_X86_FSUB:
        case RCC_X86_FMUL: case RCC_X86_FDIV:
        case RCC_X86_GEP:
            return instruction->has_destination &&
                instruction->operand_count == 2u &&
                instruction->target_count == 0u;
        case RCC_X86_TRUNCATE: case RCC_X86_ZERO_EXTEND:
        case RCC_X86_SIGN_EXTEND: case RCC_X86_FLOAT_EXTEND:
        case RCC_X86_SITOFP: case RCC_X86_FPTOSI:
        case RCC_X86_FLOAT_TRUNCATE:
        case RCC_X86_REINTERPRET:
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
                ((instruction->symbol && instruction->symbol[0] &&
                  !instruction->has_callee) ||
                 (!instruction->symbol && instruction->has_callee));
        case RCC_X86_CAPTURE_RETURN_PAIR:
            return !instruction->has_destination &&
                instruction->operand_count == 1u &&
                instruction->target_count == 0u &&
                instruction->type.kind == RCC_MIR_TYPE_VOID &&
                (instruction->immediate == 8u ||
                 (instruction->immediate >= 9u &&
                  instruction->immediate <= 16u));
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
                instruction->operand_count <= 2u &&
                instruction->target_count == 0u;
        case RCC_X86_PREFETCH:
            return !instruction->has_destination &&
                instruction->operand_count == 1u &&
                instruction->target_count == 0u &&
                instruction->type.kind == RCC_MIR_TYPE_VOID &&
                instruction->operand_types[0].kind == RCC_MIR_TYPE_POINTER &&
                instruction->immediate <= 7u;
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
        case RCC_X86_LEGAL_BINARY:
            return x86_legal_is_binary(instruction->selected_opcode) &&
                instruction->has_destination &&
                instruction->operand_count == 1u &&
                instruction->target_count == 0u;
        case RCC_X86_LEGAL_CALL:
            return instruction->selected_opcode == RCC_X86_CALL &&
                !instruction->has_destination &&
                instruction->operand_count == 0u &&
                instruction->target_count == 0u &&
                ((instruction->symbol && instruction->symbol[0] &&
                  !instruction->has_callee) ||
                 (!instruction->symbol && instruction->has_callee));
        case RCC_X86_LEGAL_RETURN:
            return instruction->selected_opcode == RCC_X86_RETURN &&
                !instruction->has_destination &&
                instruction->operand_count == 0u &&
                instruction->target_count == 0u;
    }
    return false;
}

static bool x86_legal_is_terminator(const RccX86LegalInstruction* item) {
    return item->opcode == RCC_X86_LEGAL_RETURN ||
        (item->opcode == RCC_X86_LEGAL_SELECTED &&
         (item->selected_opcode == RCC_X86_JUMP ||
          item->selected_opcode == RCC_X86_JUMP_IF ||
          item->selected_opcode == RCC_X86_RETURN ||
          item->selected_opcode == RCC_X86_TRAP));
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

static bool x86_legal_verify_binary(
    const RccX86LegalInstruction* instruction,
    const RccX86Abi* abi, char* error, size_t error_size) {
    const RccX86LegalInstruction* input = instruction->previous;
    if (!(x86_legal_is_float_binary(instruction->selected_opcode)
              ? x86_legal_float_type(instruction->type, abi)
              : x86_legal_native_type(instruction->type, abi)) ||
        !instruction->has_destination ||
        instruction->operand_count != 1u || !input ||
        input->opcode != RCC_X86_LEGAL_COPY ||
        !input->has_destination || input->operand_count != 1u ||
        !x86_legal_value_equal(
            input->destination, instruction->destination)) {
        return x86_legal_error(error, error_size,
                               "x86 two-address binary sequence is invalid");
    }
    return true;
}

static bool x86_legal_selected_call_supported(
    const RccX86LegalInstruction* instruction, const RccX86Abi* abi) {
    size_t index;
    if (instruction->selected_opcode != RCC_X86_CALL ||
        (instruction->type.kind != RCC_MIR_TYPE_VOID &&
         !x86_legal_abi_scalar_parameter(instruction->type, abi))) {
        return false;
    }
    for (index = 0u; index < instruction->operand_count; ++index) {
        if (!x86_legal_abi_scalar_parameter(
                instruction->operand_types[index], abi)) return false;
    }
    return true;
}

static bool x86_legal_verify_call(
    const RccX86LegalInstruction* instruction,
    const RccX86LegalFunction* function, const RccX86Abi* abi,
    char* error, size_t error_size) {
    if ((instruction->sysv_variadic_call
             ? function->target != RCC_X86_TARGET_X86_64 ||
                   instruction->sysv_vector_argument_count >
                       abi->floating_argument_count
             : instruction->sysv_vector_argument_count != 0u) ||
        instruction->auxiliary > function->outgoing_stack_size ||
        instruction->auxiliary % abi->pointer_size != 0u) {
        return x86_legal_error(error, error_size,
                               "x86 call stack area is invalid");
    }
    if (instruction->sysv_stack_alignment < abi->stack_alignment ||
        instruction->sysv_stack_alignment > 4096u ||
        (instruction->sysv_stack_alignment &
         (instruction->sysv_stack_alignment - 1u)) != 0u ||
        (instruction->sysv_stack_alignment > abi->stack_alignment &&
         (function->target != RCC_X86_TARGET_X86_64 ||
          !instruction->sysv_variadic_call ||
          instruction->auxiliary == 0u))) {
        return x86_legal_error(error, error_size,
                               "x86 call stack alignment is invalid");
    }
    if (instruction->type.kind != RCC_MIR_TYPE_VOID) {
        const RccX86LegalInstruction* output = instruction->next;
        bool floating_result = instruction->type.kind == RCC_MIR_TYPE_FLOAT;
        if (!output || output->opcode != RCC_X86_LEGAL_COPY ||
            output->operand_count != 1u ||
            (floating_result
                 ? output->operands[0].kind != RCC_X86_VALUE_FPR ||
                       output->operands[0].fpr != 0u
                 : output->operands[0].kind != RCC_X86_VALUE_GPR ||
                       output->operands[0].gpr != abi->return_low)) {
            return x86_legal_error(error, error_size,
                                   "x86 call result sequence is invalid");
        }
    }
    return true;
}

static bool x86_legal_verify_return(
    const RccX86LegalInstruction* instruction,
    const RccX86LegalFunction* function, const RccX86Abi* abi,
    char* error, size_t error_size) {
    if (!rcc_mir_type_equal(instruction->type, function->return_type)) {
        return x86_legal_error(error, error_size,
                               "x86 return type is invalid");
    }
    if (function->return_type.kind != RCC_MIR_TYPE_VOID) {
        const RccX86LegalInstruction* input = instruction->previous;
        bool pair_return = instruction->auxiliary == 2u;
        bool found_low = false;
        bool found_high = false;
        while (input && input->opcode == RCC_X86_LEGAL_COPY) {
            if (function->return_type.kind == RCC_MIR_TYPE_FLOAT &&
                input->has_destination &&
                input->destination.kind == RCC_X86_VALUE_FPR &&
                input->destination.fpr == 0u) {
                found_low = true;
            } else if (input->has_destination &&
                input->destination.kind == RCC_X86_VALUE_GPR) {
                if (input->destination.gpr == abi->return_low) {
                    found_low = true;
                }
                if (input->destination.gpr == abi->return_high) {
                    found_high = true;
                }
            }
            input = input->previous;
        }
        if (pair_return &&
            ((function->target == RCC_X86_TARGET_I686 &&
              instruction->immediate == 0u) ||
             (function->target == RCC_X86_TARGET_X86_64 &&
              instruction->immediate >= 9u &&
              instruction->immediate <= 16u))) return true;
        if (!pair_return && instruction->auxiliary != 1u) {
            return x86_legal_error(error, error_size,
                                   "x86 return metadata is invalid");
        }
        if (!found_low || (pair_return && !found_high)) {
            return x86_legal_error(error, error_size,
                                   "x86 return-register sequence is invalid");
        }
    }
    return true;
}

static bool x86_legal_verify_frame_plan(
    const RccX86LegalFunction* function, const RccX86Abi* abi,
    char* error, size_t error_size) {
    uint32_t hardware_callee = x86_legal_hardware_callee_mask(abi);
    uint32_t save_end;
    uint32_t next_reserved;
    uint32_t expected_mask = 0u;
    size_t expected_count = 0u;
    size_t index;
    if (!function->frame_plan_complete ||
        function->source_frame_size % function->stack_alignment != 0u ||
        function->source_frame_size > function->frame_size ||
        (function->has_sysv_varargs_gpr_save_area
             ? function->target != RCC_X86_TARGET_X86_64 ||
                   function->sysv_varargs_gpr_save_area_offset % 16u != 0u ||
                   function->sysv_varargs_gpr_save_area_offset >
                       function->source_frame_size ||
                   RCC_IR_SYSV_VA_SAVE_AREA_SIZE >
                       function->source_frame_size -
                       function->sysv_varargs_gpr_save_area_offset
             : function->sysv_varargs_gpr_save_area_offset != 0u) ||
        function->callee_save_area_offset !=
            function->source_frame_size ||
        function->callee_save_count >
            UINT32_MAX / abi->pointer_size ||
        function->callee_save_area_size !=
            function->callee_save_count * abi->pointer_size ||
        (function->callee_save_count != 0u) !=
            (function->callee_saves != NULL) ||
        function->callee_save_area_offset > UINT32_MAX -
            function->callee_save_area_size ||
        function->stack_alignment_padding >=
            function->stack_alignment ||
        function->stack_adjustment < function->frame_size ||
        function->stack_alignment_padding !=
            function->stack_adjustment - function->frame_size ||
        ((function->stack_adjustment % function->stack_alignment) +
         (((uint32_t)abi->pointer_size * 2u) %
          function->stack_alignment)) % function->stack_alignment != 0u ||
        function->callee_saved_gpr_mask !=
            (function->used_gpr_mask & hardware_callee)) {
        return x86_legal_error(error, error_size,
                               "x86 frame plan is invalid");
    }
    save_end = function->callee_save_area_offset +
        function->callee_save_area_size;
    next_reserved = function->has_parallel_copy_temporary
        ? function->parallel_copy_temporary_offset
        : function->outgoing_stack_offset;
    if (save_end > next_reserved) {
        return x86_legal_error(error, error_size,
                               "x86 callee-save area overlaps the frame");
    }
    for (index = 0u; index < function->callee_save_count; ++index) {
        const RccX86CalleeSave* save = &function->callee_saves[index];
        uint32_t bit;
        if ((unsigned)save->gpr >= 16u) {
            return x86_legal_error(error, error_size,
                                   "x86 callee-save register is invalid");
        }
        bit = UINT32_C(1) << save->gpr;
        if ((hardware_callee & bit) == 0u ||
            (expected_mask & bit) != 0u ||
            save->frame_offset != function->callee_save_area_offset +
                (uint32_t)index * abi->pointer_size ||
            (index != 0u &&
             function->callee_saves[index - 1u].gpr >= save->gpr)) {
            return x86_legal_error(error, error_size,
                                   "x86 callee-save entry is invalid");
        }
        expected_mask |= bit;
        ++expected_count;
    }
    if (expected_count != function->callee_save_count ||
        expected_mask != function->callee_saved_gpr_mask) {
        return x86_legal_error(error, error_size,
                               "x86 callee-save mask is inconsistent");
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
        function->outgoing_stack_size % function->stack_alignment != 0u ||
        function->outgoing_stack_offset > function->frame_size ||
        function->outgoing_stack_size > function->frame_size -
            function->outgoing_stack_offset ||
        function->outgoing_stack_size != function->frame_size -
            function->outgoing_stack_offset ||
        !x86_legal_return_type_supported(function->return_type, &abi) ||
        (function->has_parallel_copy_temporary &&
         (function->parallel_copy_temporary_offset >
              function->frame_size ||
          abi.pointer_size > function->frame_size -
              function->parallel_copy_temporary_offset ||
          function->parallel_copy_temporary_offset >
              function->outgoing_stack_offset ||
          abi.pointer_size > function->outgoing_stack_offset -
              function->parallel_copy_temporary_offset)) ||
        function->block_count < function->original_block_count) {
        return x86_legal_error(error, error_size,
                               "x86 legal-function header is invalid");
    }
    if (!x86_legal_verify_frame_plan(
            function, &abi, error, error_size)) return false;
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
                (instruction->opcode == RCC_X86_LEGAL_SELECTED &&
                 instruction->selected_opcode == RCC_X86_FLOAT_EXTEND &&
                 ((function->target != RCC_X86_TARGET_I686 &&
                   function->target != RCC_X86_TARGET_X86_64) ||
                  instruction->type.kind != RCC_MIR_TYPE_FLOAT ||
                  instruction->type.bit_width != 64u ||
                  instruction->operand_count != 1u ||
                  instruction->operand_types[0].kind !=
                      RCC_MIR_TYPE_FLOAT ||
                  instruction->operand_types[0].bit_width != 32u)) ||
                (instruction->opcode == RCC_X86_LEGAL_SELECTED &&
                 instruction->selected_opcode == RCC_X86_SITOFP &&
                 ((function->target != RCC_X86_TARGET_I686 &&
                   function->target != RCC_X86_TARGET_X86_64) ||
                  instruction->type.kind != RCC_MIR_TYPE_FLOAT ||
                  (instruction->type.bit_width != 32u &&
                   instruction->type.bit_width != 64u) ||
                  instruction->operand_count != 1u ||
                  instruction->operand_types[0].kind !=
                      RCC_MIR_TYPE_INTEGER ||
                  (function->target == RCC_X86_TARGET_I686
                       ? instruction->operand_types[0].bit_width != 32u
                       : (instruction->operand_types[0].bit_width != 32u &&
                          instruction->operand_types[0].bit_width != 64u)))) ||
                (instruction->opcode == RCC_X86_LEGAL_SELECTED &&
                 instruction->selected_opcode == RCC_X86_FPTOSI &&
                 ((function->target != RCC_X86_TARGET_I686 &&
                   function->target != RCC_X86_TARGET_X86_64) ||
                  instruction->type.kind != RCC_MIR_TYPE_INTEGER ||
                  (function->target == RCC_X86_TARGET_I686
                       ? instruction->type.bit_width != 32u
                       : (instruction->type.bit_width != 32u &&
                          instruction->type.bit_width != 64u)) ||
                  instruction->operand_count != 1u ||
                  instruction->operand_types[0].kind !=
                      RCC_MIR_TYPE_FLOAT ||
                  (instruction->operand_types[0].bit_width != 32u &&
                   instruction->operand_types[0].bit_width != 64u))) ||
                (instruction->opcode == RCC_X86_LEGAL_SELECTED &&
                 instruction->selected_opcode == RCC_X86_FLOAT_TRUNCATE &&
                 ((function->target != RCC_X86_TARGET_I686 &&
                   function->target != RCC_X86_TARGET_X86_64) ||
                  instruction->type.kind != RCC_MIR_TYPE_FLOAT ||
                  instruction->type.bit_width != 32u ||
                  instruction->operand_count != 1u ||
                  instruction->operand_types[0].kind !=
                      RCC_MIR_TYPE_FLOAT ||
                  instruction->operand_types[0].bit_width != 64u)) ||
                !((instruction->opcode == RCC_X86_LEGAL_RETURN
                       ? x86_legal_return_type_supported(
                             instruction->type, &abi)
                       : x86_legal_type_supported(
                             instruction->type, &abi))) ||
                (instruction->opcode == RCC_X86_LEGAL_CALL &&
                 instruction->immediate != 0u &&
                 (function->target != RCC_X86_TARGET_I686 ||
                  instruction->immediate != abi.pointer_size)) ||
                (instruction->opcode == RCC_X86_LEGAL_RETURN &&
                 instruction->immediate != 0u &&
                 !((function->target == RCC_X86_TARGET_I686 &&
                    instruction->immediate == abi.pointer_size) ||
                   (function->target == RCC_X86_TARGET_X86_64 &&
                    instruction->immediate >= 9u &&
                    instruction->immediate <= 16u))) ||
                (instruction->opcode == RCC_X86_LEGAL_SELECTED &&
                 instruction->selected_opcode ==
                     RCC_X86_CAPTURE_RETURN_PAIR &&
                 ((function->target == RCC_X86_TARGET_I686 &&
                   instruction->immediate != 8u) ||
                  (function->target == RCC_X86_TARGET_X86_64 &&
                   (instruction->immediate < 9u ||
                    instruction->immediate > 16u)) ||
                  (function->target != RCC_X86_TARGET_I686 &&
                   function->target != RCC_X86_TARGET_X86_64) ||
                  instruction->operand_types[0].kind !=
                      RCC_MIR_TYPE_POINTER ||
                  !instruction->previous ||
                  instruction->previous->opcode !=
                      RCC_X86_LEGAL_CALL)) ||
                (instruction->has_destination &&
                 !x86_legal_value_valid(
                     instruction->destination, function, &abi)) ||
                (instruction->has_callee &&
                 (!x86_legal_value_valid(
                      instruction->callee, function, &abi) ||
                  instruction->callee.size != abi.pointer_size ||
                  instruction->callee.alignment != abi.pointer_size)) ||
                (x86_legal_is_terminator(instruction) &&
                 instruction->next)) {
                return x86_legal_error(error, error_size,
                                       "x86 legal instruction is invalid");
            }
            for (operand = 0u; operand < instruction->operand_count;
                 ++operand) {
                if (!x86_legal_type_supported(
                        instruction->operand_types[operand], &abi) ||
                    instruction->operand_types[operand].kind ==
                        RCC_MIR_TYPE_VOID ||
                    !x86_legal_value_valid(
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
            if (instruction->opcode == RCC_X86_LEGAL_BINARY &&
                !x86_legal_verify_binary(
                    instruction, &abi, error, error_size)) return false;
            if (instruction->opcode == RCC_X86_LEGAL_CALL &&
                !x86_legal_verify_call(
                    instruction, function, &abi,
                    error, error_size)) return false;
            if (instruction->opcode == RCC_X86_LEGAL_RETURN &&
                !x86_legal_verify_return(
                    instruction, function, &abi,
                    error, error_size)) return false;
            if (instruction->opcode == RCC_X86_LEGAL_SELECTED &&
                x86_legal_selected_call_supported(instruction, &abi)) {
                return x86_legal_error(error, error_size,
                                       "native SysV call was not legalized");
            }
            if (instruction->opcode == RCC_X86_LEGAL_SELECTED &&
                x86_legal_is_binary(instruction->selected_opcode) &&
                (x86_legal_native_type(instruction->type, &abi) ||
                 (x86_legal_is_float_binary(
                      instruction->selected_opcode) &&
                  x86_legal_float_type(instruction->type, &abi)))) {
                return x86_legal_error(error, error_size,
                                       "native binary op was not legalized");
            }
            if (instruction->opcode == RCC_X86_LEGAL_SELECTED &&
                instruction->selected_opcode == RCC_X86_RETURN &&
                (function->return_type.kind == RCC_MIR_TYPE_VOID ||
                 x86_legal_native_scalar(function->return_type, &abi) ||
                 function->return_type.kind == RCC_MIR_TYPE_FLOAT)) {
                return x86_legal_error(error, error_size,
                                       "native SysV return was not legalized");
            }
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
    if (function->used_gpr_mask != x86_legal_collect_used_gprs(function)) {
        return x86_legal_error(error, error_size,
                               "x86 used-register mask is inconsistent");
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
    rcc_free(function->callee_saves);
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
    legal->source_frame_size = selected->frame_size;
    legal->frame_size = selected->frame_size;
    legal->has_sysv_varargs_gpr_save_area =
        selected->has_sysv_varargs_gpr_save_area;
    legal->sysv_varargs_gpr_save_area_offset =
        selected->sysv_varargs_gpr_save_area_offset;
    legal->return_type = selected->return_type;
    legal->original_block_count = selected->original_block_count;
    legal->source_instruction_count = selected->source_instruction_count;
    if (!x86_legal_plan_callee_saves(
            legal, selected, &abi, error, error_size) ||
        !x86_legal_prepare_outgoing_frame(
            legal, selected, &abi, error, error_size) ||
        !x86_legal_complete_frame_plan(
            legal, &abi, error, error_size)) goto cleanup;
    for (source_block = selected->first_block; source_block;
         source_block = source_block->next) {
        const RccX86Instruction* source;
        RccX86LegalBlock* block = x86_legal_append_block(
            legal, source_block);
        if (!block) goto cleanup;
        if (source_block == selected->first_block &&
            !x86_legalize_parameters(
                legal, block, selected, &abi,
                error, error_size)) goto cleanup;
        for (source = source_block->first; source; source = source->next) {
            RccX86LegalInstruction* previous = block->last;
            bool source_ok;
            if (x86_legal_is_division(source->opcode) &&
                x86_legal_native_type(source->type, &abi)) {
                source_ok = x86_legalize_division(
                        legal, block, source, &abi,
                    error, error_size);
            } else if (x86_legal_is_shift(source->opcode) &&
                       x86_legal_native_type(source->type, &abi)) {
                source_ok = x86_legalize_shift(
                        legal, block, source, &abi,
                    error, error_size);
            } else if (x86_legal_is_binary(source->opcode) &&
                       (x86_legal_native_type(source->type, &abi) ||
                        (x86_legal_is_float_binary(source->opcode) &&
                         x86_legal_float_type(source->type, &abi)))) {
                source_ok = x86_legalize_binary(
                        legal, block, source, &abi,
                    error, error_size);
            } else if (source->opcode == RCC_X86_CALL &&
                       x86_legal_call_supported(source, &abi)) {
                source_ok = x86_legalize_call(
                        legal, block, source, &abi,
                    error, error_size);
            } else if (source->opcode == RCC_X86_RETURN &&
                       (selected->return_type.kind == RCC_MIR_TYPE_VOID ||
                        x86_legal_native_scalar(
                            selected->return_type, &abi) ||
                        selected->return_type.kind == RCC_MIR_TYPE_FLOAT ||
                        x86_legal_pair_return_supported(source, &abi))) {
                source_ok = x86_legalize_return(
                        legal, block, source, &abi,
                    error, error_size);
            } else if (!x86_legal_clone_selected(
                           legal, block, source, &abi,
                           error, error_size)) {
                source_ok = false;
            } else {
                source_ok = true;
            }
            if (!source_ok) goto cleanup;
            x86_legal_tag_source(block, previous, source);
        }
    }
    legal->used_gpr_mask = x86_legal_collect_used_gprs(legal);
    if (!rcc_x86_verify_legal_function(
            legal, policy, error, error_size)) goto cleanup;
    *legal_out = legal;
    legal = NULL;
    result = true;
cleanup:
    rcc_x86_legal_function_destroy(legal);
    return result;
}
