/*
 * RCC - i686/x86-64 MIR instruction selection
 */

#include "rcc.h"
#include "x86_select.h"

#include <stdarg.h>

static bool x86_select_error(char* error, size_t error_size,
                             const char* format, ...) {
    va_list arguments;
    if (error && error_size != 0u) {
        va_start(arguments, format);
        vsnprintf(error, error_size, format, arguments);
        va_end(arguments);
    }
    return false;
}

static bool x86_is_terminator(RccX86Opcode opcode) {
    return opcode == RCC_X86_JUMP || opcode == RCC_X86_JUMP_IF ||
        opcode == RCC_X86_RETURN || opcode == RCC_X86_TRAP;
}

static bool x86_type_valid(RccMirType type) {
    switch (type.kind) {
        case RCC_MIR_TYPE_VOID:
            return type.bit_width == 0u;
        case RCC_MIR_TYPE_INTEGER:
            return type.bit_width == 1u || type.bit_width == 8u ||
                type.bit_width == 16u || type.bit_width == 32u ||
                type.bit_width == 64u;
        case RCC_MIR_TYPE_FLOAT:
            return type.bit_width == 32u || type.bit_width == 64u;
        case RCC_MIR_TYPE_POINTER:
            return type.bit_width == 0u;
    }
    return false;
}

static bool x86_align_frame(uint32_t value, uint16_t alignment,
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

static const RccMirEdgeCopies* x86_find_edge(
    const RccMirPhiPlan* plan, RccMirBlockId predecessor,
    RccMirBlockId successor) {
    size_t index;
    for (index = 0u; index < plan->edge_count; ++index) {
        const RccMirEdgeCopies* edge = &plan->edges[index];
        if (edge->predecessor == predecessor &&
            edge->successor == successor) {
            return edge;
        }
    }
    return NULL;
}

static RccX86Block* x86_append_block(
    RccX86Function* function, RccMirBlockId source_block,
    bool edge_split, RccMirBlockId predecessor,
    RccMirBlockId successor) {
    RccX86Block* block;
    if (!function || function->block_count >= UINT32_MAX) return NULL;
    block = rcc_alloc(sizeof(*block));
    block->id = (uint32_t)function->block_count++;
    block->source_block = source_block;
    block->edge_split = edge_split;
    block->edge_predecessor = predecessor;
    block->edge_successor = successor;
    if (function->last_block) function->last_block->next = block;
    else function->first_block = block;
    function->last_block = block;
    return block;
}

static RccX86Instruction* x86_append_instruction(
    RccX86Block* block, RccX86Opcode opcode, RccMirType type,
    const RccMirLocation* destination,
    const RccMirLocation* operands, const RccMirType* operand_types,
    size_t operand_count,
    const uint32_t* targets, size_t target_count) {
    RccX86Instruction* instruction;
    if (!block || (operand_count != 0u && (!operands || !operand_types)) ||
        (target_count != 0u && !targets)) {
        return NULL;
    }
    instruction = rcc_alloc(sizeof(*instruction));
    instruction->opcode = opcode;
    instruction->type = type;
    if (destination) {
        instruction->has_destination = true;
        instruction->destination = *destination;
    }
    if (operand_count != 0u) {
        instruction->operands = rcc_alloc(
            operand_count * sizeof(*instruction->operands));
        memcpy(instruction->operands, operands,
               operand_count * sizeof(*instruction->operands));
        instruction->operand_types = rcc_alloc(
            operand_count * sizeof(*instruction->operand_types));
        memcpy(instruction->operand_types, operand_types,
               operand_count * sizeof(*instruction->operand_types));
        instruction->operand_count = operand_count;
    }
    if (target_count != 0u) {
        instruction->targets = rcc_alloc(
            target_count * sizeof(*instruction->targets));
        memcpy(instruction->targets, targets,
               target_count * sizeof(*instruction->targets));
        instruction->target_count = target_count;
    }
    if (block->last) block->last->next = instruction;
    else block->first = instruction;
    block->last = instruction;
    return instruction;
}

static bool x86_append_edge_moves(RccX86Block* block,
                                  const RccMirEdgeCopies* edge) {
    size_t index;
    if (!edge) return true;
    for (index = 0u; index < edge->move_count; ++index) {
        const RccMirScheduledMove* move = &edge->moves[index];
        RccX86Instruction* selected = x86_append_instruction(
            block, RCC_X86_COPY, move->type, &move->destination,
            &move->source, &move->type, 1u, NULL, 0u);
        if (!selected) return false;
        selected->cycle_break = move->cycle_break;
    }
    return true;
}

static RccX86Opcode x86_select_opcode(RccMirOpcode opcode) {
    switch (opcode) {
        case RCC_MIR_CONST_INT: return RCC_X86_MOV_IMMEDIATE;
        case RCC_MIR_ADD: return RCC_X86_ADD;
        case RCC_MIR_SUB: return RCC_X86_SUB;
        case RCC_MIR_MUL: return RCC_X86_MUL;
        case RCC_MIR_UDIV: return RCC_X86_UDIV;
        case RCC_MIR_SDIV: return RCC_X86_SDIV;
        case RCC_MIR_UREM: return RCC_X86_UREM;
        case RCC_MIR_SREM: return RCC_X86_SREM;
        case RCC_MIR_AND: return RCC_X86_AND;
        case RCC_MIR_OR: return RCC_X86_OR;
        case RCC_MIR_XOR: return RCC_X86_XOR;
        case RCC_MIR_SHL: return RCC_X86_SHL;
        case RCC_MIR_LSHR: return RCC_X86_SHR;
        case RCC_MIR_ASHR: return RCC_X86_SAR;
        case RCC_MIR_ICMP: return RCC_X86_COMPARE_SET;
        case RCC_MIR_TRUNC: return RCC_X86_TRUNCATE;
        case RCC_MIR_ZEXT: return RCC_X86_ZERO_EXTEND;
        case RCC_MIR_SEXT: return RCC_X86_SIGN_EXTEND;
        case RCC_MIR_PTR_TO_INT:
        case RCC_MIR_INT_TO_PTR:
        case RCC_MIR_BITCAST: return RCC_X86_REINTERPRET;
        case RCC_MIR_SELECT: return RCC_X86_SELECT;
        case RCC_MIR_ALLOCA: return RCC_X86_STACK_ADDRESS;
        case RCC_MIR_LOAD: return RCC_X86_LOAD;
        case RCC_MIR_STORE: return RCC_X86_STORE;
        case RCC_MIR_GEP: return RCC_X86_GEP;
        case RCC_MIR_SYMBOL_ADDRESS: return RCC_X86_SYMBOL_ADDRESS;
        case RCC_MIR_CALL: return RCC_X86_CALL;
        case RCC_MIR_BRANCH: return RCC_X86_JUMP;
        case RCC_MIR_COND_BRANCH: return RCC_X86_JUMP_IF;
        case RCC_MIR_RETURN: return RCC_X86_RETURN;
        case RCC_MIR_UNREACHABLE: return RCC_X86_TRAP;
        case RCC_MIR_PHI: break;
    }
    return RCC_X86_TRAP;
}

static bool x86_collect_operands(
    const RccMirInstruction* instruction,
    const RccMirFunction* function,
    const RccMirAllocation* allocation,
    RccMirLocation** locations_out, RccMirType** types_out) {
    RccMirLocation* locations = NULL;
    RccMirType* types = NULL;
    size_t index;
    if (instruction->operand_count != 0u) {
        if (instruction->operand_count >
            (size_t)-1 / sizeof(*locations)) return false;
        locations = rcc_alloc(
            instruction->operand_count * sizeof(*locations));
        types = rcc_alloc(
            instruction->operand_count * sizeof(*types));
        for (index = 0u; index < instruction->operand_count; ++index) {
            locations[index] =
                allocation->locations[instruction->operands[index]];
            types[index] =
                function->register_types[instruction->operands[index]];
        }
    }
    *locations_out = locations;
    *types_out = types;
    return true;
}

static uint32_t x86_critical_target(
    const RccMirPhiPlan* plan, const uint32_t* split_blocks,
    RccMirBlockId predecessor, RccMirBlockId successor) {
    size_t index;
    for (index = 0u; index < plan->edge_count; ++index) {
        if (plan->edges[index].requires_edge_block &&
            plan->edges[index].predecessor == predecessor &&
            plan->edges[index].successor == successor) {
            return split_blocks[index];
        }
    }
    return successor;
}

static bool x86_select_instruction(
    RccX86Function* selected, RccX86Block* selected_block,
    const RccMirInstruction* instruction,
    const RccMirRegisterPolicy* policy,
    const RccMirAllocation* allocation,
    const RccMirPhiPlan* phi_plan, const uint32_t* split_blocks,
    char* error, size_t error_size) {
    RccMirLocation* operands = NULL;
    RccMirType* operand_types = NULL;
    RccMirLocation* destination = NULL;
    uint32_t targets[2];
    size_t target;
    RccX86Instruction* machine;
    RccMirType type = instruction->type;
    if (instruction->opcode == RCC_MIR_PHI) return true;
    if (instruction->definition != RCC_MIR_VREG_NONE) {
        destination = &allocation->locations[instruction->definition];
    }
    if (!x86_collect_operands(
            instruction, instruction->block->function, allocation,
            &operands, &operand_types)) {
        return x86_select_error(error, error_size,
                                "x86 operand table is too large");
    }
    if (instruction->opcode == RCC_MIR_STORE &&
        instruction->operand_count != 0u) {
        type = instruction->block->function->register_types[
            instruction->operands[0]];
    }
    if (instruction->opcode == RCC_MIR_ALLOCA) {
        uint32_t offset;
        if (instruction->immediate > UINT32_MAX ||
            !x86_align_frame(selected->frame_size, policy->pointer_size,
                             &offset) ||
            offset > UINT32_MAX - (uint32_t)instruction->immediate) {
            rcc_free(operands);
            rcc_free(operand_types);
            return x86_select_error(error, error_size,
                                    "x86 alloca frame exceeds 32 bits");
        }
        selected->frame_size = offset + (uint32_t)instruction->immediate;
    }
    for (target = 0u; target < instruction->target_count; ++target) {
        targets[target] = x86_critical_target(
            phi_plan, split_blocks, instruction->block->id,
            instruction->targets[target]);
    }
    if (instruction->target_count == 1u) {
        const RccMirEdgeCopies* edge = x86_find_edge(
            phi_plan, instruction->block->id,
            instruction->targets[0]);
        if (edge && !edge->requires_edge_block &&
            !x86_append_edge_moves(selected_block, edge)) {
            rcc_free(operands);
            rcc_free(operand_types);
            return x86_select_error(error, error_size,
                                    "x86 phi move table is too large");
        }
    }
    machine = x86_append_instruction(
        selected_block, x86_select_opcode(instruction->opcode), type,
        destination, operands, operand_types, instruction->operand_count,
        targets, instruction->target_count);
    rcc_free(operands);
    rcc_free(operand_types);
    if (!machine) {
        return x86_select_error(error, error_size,
                                "x86 instruction table is too large");
    }
    machine->immediate = instruction->immediate;
    machine->predicate = instruction->predicate;
    if (instruction->opcode == RCC_MIR_ALLOCA) {
        machine->immediate = selected->frame_size -
            (uint32_t)instruction->immediate;
        machine->auxiliary = instruction->immediate;
    } else if (instruction->opcode == RCC_MIR_GEP) {
        machine->auxiliary = instruction->immediate;
    }
    if (instruction->callee) machine->symbol = rcc_strdup(instruction->callee);
    ++selected->source_instruction_count;
    return true;
}

static bool x86_location_valid(
    RccMirLocation location, const RccMirRegisterPolicy* policy,
    uint32_t frame_size) {
    uint64_t mask;
    if (location.register_class == RCC_MIR_REGCLASS_FPR) {
        mask = policy->allocatable_fpr_mask;
    } else if (location.register_class == RCC_MIR_REGCLASS_GPR) {
        mask = policy->allocatable_gpr_mask;
    } else {
        return false;
    }
    if (location.kind == RCC_MIR_LOCATION_PHYSICAL) {
        return location.physical_register < 64u &&
            (mask & (UINT64_C(1) << location.physical_register)) != 0u;
    }
    return location.kind == RCC_MIR_LOCATION_SPILL &&
        location.spill_size != 0u &&
        location.spill_alignment != 0u &&
        location.spill_offset % location.spill_alignment == 0u &&
        location.spill_offset <= frame_size &&
        location.spill_size <= frame_size - location.spill_offset;
}

static bool x86_instruction_shape(const RccX86Instruction* instruction) {
    if (!instruction ||
        (instruction->operand_count != 0u &&
         (!instruction->operands || !instruction->operand_types)) ||
        (instruction->target_count != 0u && !instruction->targets)) {
        return false;
    }
    switch (instruction->opcode) {
        case RCC_X86_COPY:
            return instruction->has_destination &&
                instruction->operand_count == 1u &&
                instruction->target_count == 0u;
        case RCC_X86_MOV_IMMEDIATE:
        case RCC_X86_STACK_ADDRESS:
        case RCC_X86_SYMBOL_ADDRESS:
            return instruction->has_destination &&
                instruction->operand_count == 0u &&
                instruction->target_count == 0u &&
                (instruction->opcode != RCC_X86_SYMBOL_ADDRESS ||
                 (instruction->symbol && instruction->symbol[0]));
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
    }
    return false;
}

bool rcc_x86_verify_function(
    const RccX86Function* function,
    const RccMirRegisterPolicy* policy,
    char* error, size_t error_size) {
    const RccX86Block* block;
    RccX86Abi abi;
    size_t blocks = 0u;
    if (error && error_size != 0u) error[0] = '\0';
    if (!function || !policy ||
        !rcc_x86_abi_for_target(function->target, &abi) ||
        !rcc_x86_abi_verify_policy(&abi, policy, error, error_size) ||
        (function->target == RCC_X86_TARGET_I686
             ? function->pointer_size != 4u
             : function->pointer_size != 8u) ||
        function->pointer_size != policy->pointer_size ||
        function->stack_alignment != policy->stack_alignment ||
        function->stack_alignment == 0u ||
        function->frame_size % function->stack_alignment != 0u ||
        !x86_type_valid(function->return_type) ||
        (function->parameter_count != 0u &&
         (!function->parameter_types || !function->parameters)) ||
        function->original_block_count == 0u ||
        function->block_count < function->original_block_count) {
        return x86_select_error(error, error_size,
                                "x86 selected-function header is invalid");
    }
    for (size_t parameter = 0u;
         parameter < function->parameter_count; ++parameter) {
        if (!x86_type_valid(function->parameter_types[parameter]) ||
            function->parameter_types[parameter].kind ==
                RCC_MIR_TYPE_VOID ||
            !x86_location_valid(function->parameters[parameter], policy,
                                function->frame_size)) {
            return x86_select_error(error, error_size,
                                    "x86 parameter location is invalid");
        }
    }
    for (block = function->first_block; block; block = block->next) {
        const RccX86Instruction* instruction;
        size_t instruction_count = 0u;
        if (block->id != blocks ||
            (block->edge_split
                 ? block->id < function->original_block_count ||
                   block->source_block != RCC_MIR_BLOCK_NONE
                 : block->id >= function->original_block_count ||
                   block->source_block != block->id)) {
            return x86_select_error(error, error_size,
                                    "x86 block identity is invalid");
        }
        for (instruction = block->first; instruction;
             instruction = instruction->next) {
            size_t operand;
            size_t target;
            ++instruction_count;
            if (!x86_instruction_shape(instruction) ||
                (x86_is_terminator(instruction->opcode) &&
                 instruction->next)) {
                return x86_select_error(error, error_size,
                                        "x86 instruction shape is invalid");
            }
            if (instruction->has_destination &&
                !x86_location_valid(instruction->destination, policy,
                                    function->frame_size)) {
                return x86_select_error(error, error_size,
                                        "x86 destination is invalid");
            }
            for (operand = 0u; operand < instruction->operand_count;
                 ++operand) {
                if (!x86_location_valid(instruction->operands[operand],
                                        policy, function->frame_size)) {
                    return x86_select_error(error, error_size,
                                            "x86 operand is invalid");
                }
                if (instruction->operand_types[operand].kind ==
                        RCC_MIR_TYPE_VOID) {
                    return x86_select_error(error, error_size,
                                            "x86 operand type is invalid");
                }
            }
            for (target = 0u; target < instruction->target_count;
                 ++target) {
                if (instruction->targets[target] >= function->block_count) {
                    return x86_select_error(error, error_size,
                                            "x86 branch target is invalid");
                }
            }
        }
        if (instruction_count == 0u || !block->last ||
            !x86_is_terminator(block->last->opcode)) {
            return x86_select_error(error, error_size,
                                    "x86 block lacks a terminator");
        }
        if (block->edge_split &&
            (block->last->opcode != RCC_X86_JUMP ||
             block->last->targets[0] != block->edge_successor)) {
            return x86_select_error(error, error_size,
                                    "x86 split edge is invalid");
        }
        ++blocks;
    }
    if (blocks != function->block_count) {
        return x86_select_error(error, error_size,
                                "x86 block count is inconsistent");
    }
    return true;
}

void rcc_x86_function_destroy(RccX86Function* function) {
    RccX86Block* block;
    if (!function) return;
    block = function->first_block;
    while (block) {
        RccX86Block* next_block = block->next;
        RccX86Instruction* instruction = block->first;
        while (instruction) {
            RccX86Instruction* next_instruction = instruction->next;
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
    rcc_free(function->parameter_types);
    rcc_free(function->parameters);
    rcc_free(function);
}

bool rcc_x86_select_function(
    const RccMirFunction* function, RccX86Target target,
    const RccMirRegisterPolicy* policy,
    const RccMirAllocation* allocation, const RccMirPhiPlan* phi_plan,
    RccX86Function** selected_out, char* error, size_t error_size) {
    RccX86Function* selected = NULL;
    RccX86Block** blocks = NULL;
    uint32_t* split_blocks = NULL;
    const RccMirBlock* mir_block;
    size_t edge;
    bool result = false;
    if (selected_out) *selected_out = NULL;
    if (error && error_size != 0u) error[0] = '\0';
    if (!function || !policy || !allocation || !phi_plan ||
        !selected_out ||
        (target == RCC_X86_TARGET_I686
             ? policy->pointer_size != 4u
             : target != RCC_X86_TARGET_X86_64 ||
               policy->pointer_size != 8u) ||
        !rcc_mir_verify_function(function, error, error_size) ||
        !rcc_mir_verify_allocation(function, policy, allocation,
                                   error, error_size) ||
        !rcc_mir_verify_phi_plan(function, policy, allocation, phi_plan,
                                 error, error_size)) {
        return false;
    }
    if (function->block_count > (size_t)-1 / sizeof(*blocks) ||
        phi_plan->edge_count > (size_t)-1 / sizeof(*split_blocks)) {
        return x86_select_error(error, error_size,
                                "x86 block table is too large");
    }
    selected = rcc_alloc(sizeof(*selected));
    selected->target = target;
    selected->pointer_size = policy->pointer_size;
    selected->stack_alignment = policy->stack_alignment;
    selected->frame_size = phi_plan->frame_size;
    selected->return_type = function->return_type;
    selected->parameter_count = function->parameter_count;
    if (function->parameter_count != 0u) {
        selected->parameter_types = rcc_alloc(
            function->parameter_count *
                sizeof(*selected->parameter_types));
        selected->parameters = rcc_alloc(
            function->parameter_count * sizeof(*selected->parameters));
        memcpy(selected->parameter_types, function->parameter_types,
               function->parameter_count *
                   sizeof(*selected->parameter_types));
        for (size_t parameter = 0u;
             parameter < function->parameter_count; ++parameter) {
            selected->parameters[parameter] =
                allocation->locations[function->parameters[parameter]];
        }
    }
    selected->original_block_count = function->block_count;
    blocks = rcc_alloc(function->block_count * sizeof(*blocks));
    split_blocks = rcc_alloc(phi_plan->edge_count * sizeof(*split_blocks));
    for (edge = 0u; edge < phi_plan->edge_count; ++edge) {
        split_blocks[edge] = UINT32_MAX;
    }
    for (mir_block = function->first_block; mir_block;
         mir_block = mir_block->next) {
        blocks[mir_block->id] = x86_append_block(
            selected, mir_block->id, false,
            RCC_MIR_BLOCK_NONE, RCC_MIR_BLOCK_NONE);
        if (!blocks[mir_block->id]) goto cleanup;
    }
    for (edge = 0u; edge < phi_plan->edge_count; ++edge) {
        const RccMirEdgeCopies* copies = &phi_plan->edges[edge];
        if (copies->requires_edge_block) {
            RccX86Block* split = x86_append_block(
                selected, RCC_MIR_BLOCK_NONE, true,
                copies->predecessor, copies->successor);
            if (!split) goto cleanup;
            split_blocks[edge] = split->id;
        }
    }
    for (mir_block = function->first_block; mir_block;
         mir_block = mir_block->next) {
        const RccMirInstruction* instruction;
        RccX86Block* block = blocks[mir_block->id];
        for (edge = 0u; edge < phi_plan->edge_count; ++edge) {
            const RccMirEdgeCopies* copies = &phi_plan->edges[edge];
            const RccMirBlock* predecessor;
            if (copies->requires_edge_block ||
                copies->successor != mir_block->id) continue;
            predecessor = function->first_block;
            while (predecessor &&
                   predecessor->id != copies->predecessor) {
                predecessor = predecessor->next;
            }
            if (predecessor && predecessor->last->target_count > 1u &&
                !x86_append_edge_moves(block, copies)) {
                goto cleanup;
            }
        }
        for (instruction = mir_block->first; instruction;
             instruction = instruction->next) {
            if (!x86_select_instruction(
                    selected, block, instruction, policy, allocation,
                    phi_plan, split_blocks, error, error_size)) {
                goto cleanup;
            }
        }
    }
    for (edge = 0u; edge < phi_plan->edge_count; ++edge) {
        const RccMirEdgeCopies* copies = &phi_plan->edges[edge];
        if (copies->requires_edge_block) {
            RccX86Block* split = selected->first_block;
            uint32_t target_block = copies->successor;
            while (split && split->id != split_blocks[edge]) {
                split = split->next;
            }
            if (!split || !x86_append_edge_moves(split, copies) ||
                !x86_append_instruction(
                    split, RCC_X86_JUMP, rcc_mir_type_void(),
                    NULL, NULL, NULL, 0u, &target_block, 1u)) {
                goto cleanup;
            }
        }
    }
    if (!x86_align_frame(selected->frame_size, policy->stack_alignment,
                         &selected->frame_size) ||
        !rcc_x86_verify_function(selected, policy, error, error_size)) {
        if (error && error_size != 0u && error[0] == '\0') {
            x86_select_error(error, error_size,
                             "x86 frame alignment overflow");
        }
        goto cleanup;
    }
    *selected_out = selected;
    selected = NULL;
    result = true;
cleanup:
    rcc_free(blocks);
    rcc_free(split_blocks);
    rcc_x86_function_destroy(selected);
    return result;
}
