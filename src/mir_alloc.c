/*
 * RCC - MIR liveness and linear-scan register allocation
 */

#include "rcc.h"
#include "mir_alloc.h"

#include <stdarg.h>

#define MIR_ALLOC_GRAPH_MAX_BYTES ((size_t)64u * 1024u * 1024u)

typedef struct {
    RccMirVReg reg;
    size_t start;
    size_t end;
} RccMirIntervalOrder;

static bool mir_alloc_error(char* error, size_t error_size,
                            const char* format, ...) {
    va_list arguments;
    if (error && error_size != 0u) {
        va_start(arguments, format);
        vsnprintf(error, error_size, format, arguments);
        va_end(arguments);
    }
    return false;
}

/* Compare nonnegative fractions without a cross-product (which could
 * overflow size_t) or a floating-point ABI dependency. Continued-fraction
 * quotients reverse ordering each time the remainder is reciprocated. */
static int mir_alloc_compare_fractions(size_t left_numerator,
                                       size_t left_denominator,
                                       size_t right_numerator,
                                       size_t right_denominator) {
    bool reversed = false;
    for (;;) {
        size_t left_quotient = left_numerator / left_denominator;
        size_t right_quotient = right_numerator / right_denominator;
        size_t left_remainder;
        size_t right_remainder;
        if (left_quotient != right_quotient) {
            int comparison = left_quotient < right_quotient ? -1 : 1;
            return reversed ? -comparison : comparison;
        }
        left_remainder = left_numerator % left_denominator;
        right_remainder = right_numerator % right_denominator;
        if (left_remainder == 0u || right_remainder == 0u) {
            int comparison;
            if (left_remainder == 0u && right_remainder == 0u) return 0;
            comparison = left_remainder == 0u ? -1 : 1;
            return reversed ? -comparison : comparison;
        }
        left_numerator = left_denominator;
        left_denominator = left_remainder;
        right_numerator = right_denominator;
        right_denominator = right_remainder;
        reversed = !reversed;
    }
}

void rcc_mir_register_policy_i686(RccMirRegisterPolicy* policy) {
    if (!policy) return;
    memset(policy, 0, sizeof(*policy));
    policy->allocatable_gpr_mask = UINT64_C(0x3f);
    policy->allocatable_fpr_mask = UINT64_C(0xff);
    policy->caller_saved_gpr_mask = UINT64_C(0x07);
    policy->caller_saved_fpr_mask = UINT64_C(0xff);
    policy->division_fixed_gpr_mask = UINT64_C(0x05);
    policy->shift_count_fixed_gpr_mask = UINT64_C(0x02);
    policy->pointer_size = 4u;
    policy->stack_alignment = 16u;
}

void rcc_mir_register_policy_x86_64(RccMirRegisterPolicy* policy) {
    if (!policy) return;
    memset(policy, 0, sizeof(*policy));
    policy->allocatable_gpr_mask = UINT64_C(0x3fff);
    policy->allocatable_fpr_mask = UINT64_C(0xffff);
    policy->caller_saved_gpr_mask = UINT64_C(0x01ff);
    policy->caller_saved_fpr_mask = UINT64_C(0xffff);
    policy->division_fixed_gpr_mask = UINT64_C(0x05);
    policy->shift_count_fixed_gpr_mask = UINT64_C(0x02);
    policy->pointer_size = 8u;
    policy->stack_alignment = 16u;
}

static bool mir_alloc_policy_valid(const RccMirRegisterPolicy* policy) {
    if (!policy || policy->pointer_size == 0u ||
        policy->stack_alignment == 0u ||
        (policy->stack_alignment & (policy->stack_alignment - 1u)) != 0u) {
        return false;
    }
    if ((policy->caller_saved_gpr_mask &
         ~policy->allocatable_gpr_mask) != 0u ||
        (policy->caller_saved_fpr_mask &
         ~policy->allocatable_fpr_mask) != 0u ||
        (policy->division_fixed_gpr_mask &
         ~policy->allocatable_gpr_mask) != 0u ||
        (policy->shift_count_fixed_gpr_mask &
         ~policy->allocatable_gpr_mask) != 0u) {
        return false;
    }
    return true;
}

static uint64_t mir_alloc_fixed_mask(
    RccMirOpcode opcode, const RccMirRegisterPolicy* policy) {
    switch (opcode) {
        case RCC_MIR_UDIV:
        case RCC_MIR_SDIV:
        case RCC_MIR_UREM:
        case RCC_MIR_SREM:
            return policy->division_fixed_gpr_mask;
        case RCC_MIR_SHL:
        case RCC_MIR_LSHR:
        case RCC_MIR_ASHR:
            return policy->shift_count_fixed_gpr_mask;
        default:
            return 0u;
    }
}

static bool mir_alloc_apply_fixed_constraints(
    const RccMirFunction* function, const RccMirRegisterPolicy* policy,
    RccMirLiveInterval* intervals, char* error, size_t error_size) {
    const RccMirBlock* block;
    size_t position = 1u;
    for (block = function->first_block; block; block = block->next) {
        const RccMirInstruction* instruction;
        for (instruction = block->first; instruction;
             instruction = instruction->next) {
            uint64_t fixed_mask = mir_alloc_fixed_mask(
                instruction->opcode, policy);
            if (fixed_mask != 0u) {
                size_t reg;
                for (reg = 0u; reg < function->register_count; ++reg) {
                    if (intervals[reg].register_class ==
                            RCC_MIR_REGCLASS_GPR &&
                        intervals[reg].start <= position &&
                        position <= intervals[reg].end) {
                        intervals[reg].forbidden_physical_mask |=
                            fixed_mask;
                    }
                }
            }
            if (position == SIZE_MAX) {
                return mir_alloc_error(
                    error, error_size,
                    "MIR fixed-register positions overflow");
            }
            ++position;
        }
    }
    return true;
}

static bool mir_alloc_verify_fixed_constraints(
    const RccMirFunction* function, const RccMirRegisterPolicy* policy,
    const RccMirAllocation* allocation, char* error, size_t error_size) {
    const RccMirBlock* block;
    size_t position = 1u;
    for (block = function->first_block; block; block = block->next) {
        const RccMirInstruction* instruction;
        for (instruction = block->first; instruction;
             instruction = instruction->next) {
            uint64_t fixed_mask = mir_alloc_fixed_mask(
                instruction->opcode, policy);
            if (fixed_mask != 0u) {
                size_t reg;
                for (reg = 0u; reg < allocation->register_count; ++reg) {
                    RccMirLiveInterval interval =
                        allocation->intervals[reg];
                    if (interval.register_class == RCC_MIR_REGCLASS_GPR &&
                        interval.start <= position &&
                        position <= interval.end &&
                        (interval.forbidden_physical_mask & fixed_mask) !=
                            fixed_mask) {
                        return mir_alloc_error(
                            error, error_size,
                            "MIR interval misses a fixed-register constraint");
                    }
                }
            }
            if (position == SIZE_MAX) {
                return mir_alloc_error(
                    error, error_size,
                    "MIR fixed-register verification overflow");
            }
            ++position;
        }
    }
    return true;
}

static RccMirRegisterClass mir_alloc_register_class(RccMirType type) {
    if (type.kind == RCC_MIR_TYPE_FLOAT) return RCC_MIR_REGCLASS_FPR;
    return RCC_MIR_REGCLASS_GPR;
}

static uint16_t mir_alloc_type_size(RccMirType type,
                                    const RccMirRegisterPolicy* policy) {
    if (type.kind == RCC_MIR_TYPE_POINTER) return policy->pointer_size;
    if (type.bit_width <= 8u) return 1u;
    return (uint16_t)(type.bit_width / 8u);
}

static uint16_t mir_alloc_type_alignment(
    RccMirType type, const RccMirRegisterPolicy* policy) {
    uint16_t size = mir_alloc_type_size(type, policy);
    if (size > policy->stack_alignment) return policy->stack_alignment;
    return size;
}

static bool mir_alloc_align(uint32_t value, uint16_t alignment,
                            uint32_t* result) {
    uint32_t mask = (uint32_t)alignment - 1u;
    if (value > UINT32_MAX - mask) return false;
    *result = (value + mask) & ~mask;
    return true;
}

static bool mir_alloc_collect_positions(
    const RccMirFunction* function, size_t** block_starts_out,
    size_t** block_ends_out,
    size_t** calls_out, size_t* call_count_out,
    RccMirLiveInterval* intervals, char* error, size_t error_size) {
    size_t* block_starts;
    size_t* block_ends;
    size_t* calls;
    size_t call_capacity = 0u;
    size_t call_count = 0u;
    size_t position = 1u;
    const RccMirBlock* block;
    size_t parameter;
    if (function->block_count > SIZE_MAX / sizeof(*block_ends)) {
        return mir_alloc_error(error, error_size,
                               "MIR block position table is too large");
    }
    block_starts = rcc_alloc(
        function->block_count * sizeof(*block_starts));
    block_ends = rcc_alloc(function->block_count * sizeof(*block_ends));
    calls = NULL;
    for (parameter = 0u; parameter < function->parameter_count; ++parameter) {
        RccMirVReg reg = function->parameters[parameter];
        intervals[reg].start = 0u;
        intervals[reg].end = 0u;
    }
    for (block = function->first_block; block; block = block->next) {
        const RccMirInstruction* instruction;
        block_starts[block->id] = position;
        for (instruction = block->first; instruction;
             instruction = instruction->next) {
            size_t operand;
            if (instruction->definition != RCC_MIR_VREG_NONE) {
                RccMirVReg reg = instruction->definition;
                if (position < intervals[reg].start) {
                    intervals[reg].start = position;
                }
                if (position > intervals[reg].end) {
                    intervals[reg].end = position;
                }
            }
            if (instruction->opcode != RCC_MIR_PHI) {
                for (operand = 0u; operand < instruction->operand_count;
                     ++operand) {
                    RccMirVReg reg = instruction->operands[operand];
                    if (position < intervals[reg].start) {
                        intervals[reg].start = position;
                    }
                    if (position > intervals[reg].end) {
                        intervals[reg].end = position;
                    }
                }
            }
            if (instruction->opcode == RCC_MIR_CALL &&
                instruction->callee_value != RCC_MIR_VREG_NONE) {
                RccMirVReg reg = instruction->callee_value;
                if (position < intervals[reg].start) {
                    intervals[reg].start = position;
                }
                if (position > intervals[reg].end) {
                    intervals[reg].end = position;
                }
            }
            if (instruction->opcode == RCC_MIR_CALL) {
                if (call_count == call_capacity) {
                    size_t next_capacity = call_capacity == 0u
                        ? 8u : call_capacity * 2u;
                    if (next_capacity < call_capacity ||
                        next_capacity > SIZE_MAX / sizeof(*calls)) {
                        rcc_free(block_starts);
                        rcc_free(block_ends);
                        rcc_free(calls);
                        return mir_alloc_error(
                            error, error_size,
                            "MIR call position table is too large");
                    }
                    calls = rcc_realloc(
                        calls, next_capacity * sizeof(*calls));
                    call_capacity = next_capacity;
                }
                calls[call_count++] = position;
            }
            if (position == SIZE_MAX) {
                rcc_free(block_starts);
                rcc_free(block_ends);
                rcc_free(calls);
                return mir_alloc_error(error, error_size,
                                       "MIR instruction positions overflow");
            }
            ++position;
        }
        block_ends[block->id] = position - 1u;
    }
    for (block = function->first_block; block; block = block->next) {
        const RccMirInstruction* instruction;
        for (instruction = block->first;
             instruction && instruction->opcode == RCC_MIR_PHI;
             instruction = instruction->next) {
            size_t incoming;
            for (incoming = 0u; incoming < instruction->operand_count;
                 ++incoming) {
                RccMirVReg reg = instruction->operands[incoming];
                RccMirBlockId predecessor = instruction->targets[incoming];
                size_t use_position = block_ends[predecessor];
                if (use_position < intervals[reg].start) {
                    intervals[reg].start = use_position;
                }
                if (use_position > intervals[reg].end) {
                    intervals[reg].end = use_position;
                }
            }
        }
    }
    *block_starts_out = block_starts;
    *block_ends_out = block_ends;
    *calls_out = calls;
    *call_count_out = call_count;
    return true;
}

static bool mir_alloc_extend_cfg_liveness(
    const RccMirFunction* function, const size_t* block_starts,
    const size_t* block_ends, RccMirLiveInterval* intervals,
    char* error, size_t error_size) {
    bool* definitions = NULL;
    bool* uses = NULL;
    bool* edge_uses = NULL;
    bool* live_in = NULL;
    bool* live_out = NULL;
    size_t cells;
    const RccMirBlock* block;
    bool changed = true;
    if (function->register_count != 0u &&
        function->block_count > SIZE_MAX / function->register_count) {
        return mir_alloc_error(error, error_size,
                               "MIR liveness matrix is too large");
    }
    cells = function->block_count * function->register_count;
    if (cells > SIZE_MAX / sizeof(*definitions)) {
        return mir_alloc_error(error, error_size,
                               "MIR liveness matrix is too large");
    }
    definitions = rcc_alloc(cells * sizeof(*definitions));
    uses = rcc_alloc(cells * sizeof(*uses));
    edge_uses = rcc_alloc(cells * sizeof(*edge_uses));
    live_in = rcc_alloc(cells * sizeof(*live_in));
    live_out = rcc_alloc(cells * sizeof(*live_out));
    for (block = function->first_block; block; block = block->next) {
        const RccMirInstruction* instruction;
        size_t base = (size_t)block->id * function->register_count;
        for (instruction = block->first; instruction;
             instruction = instruction->next) {
            size_t operand;
            if (instruction->opcode != RCC_MIR_PHI) {
                for (operand = 0u; operand < instruction->operand_count;
                     ++operand) {
                    RccMirVReg reg = instruction->operands[operand];
                    if (!definitions[base + reg]) uses[base + reg] = true;
                }
            }
            if (instruction->opcode == RCC_MIR_CALL &&
                instruction->callee_value != RCC_MIR_VREG_NONE &&
                !definitions[base + instruction->callee_value]) {
                uses[base + instruction->callee_value] = true;
            }
            if (instruction->definition != RCC_MIR_VREG_NONE) {
                definitions[base + instruction->definition] = true;
            }
        }
    }
    for (block = function->first_block; block; block = block->next) {
        const RccMirInstruction* instruction;
        for (instruction = block->first;
             instruction && instruction->opcode == RCC_MIR_PHI;
             instruction = instruction->next) {
            size_t incoming;
            for (incoming = 0u; incoming < instruction->operand_count;
                 ++incoming) {
                size_t predecessor = instruction->targets[incoming];
                RccMirVReg reg = instruction->operands[incoming];
                edge_uses[predecessor * function->register_count + reg] =
                    true;
            }
        }
    }
    while (changed) {
        changed = false;
        for (block = function->first_block; block; block = block->next) {
            const RccMirInstruction* terminator = block->last;
            size_t base = (size_t)block->id * function->register_count;
            size_t reg;
            for (reg = 0u; reg < function->register_count; ++reg) {
                bool out = edge_uses[base + reg];
                size_t successor;
                for (successor = 0u;
                     !out && successor < terminator->target_count;
                     ++successor) {
                    size_t target = terminator->targets[successor];
                    out = live_in[target * function->register_count + reg];
                }
                if (out && !live_out[base + reg]) {
                    live_out[base + reg] = true;
                    changed = true;
                }
                if ((uses[base + reg] ||
                     (live_out[base + reg] && !definitions[base + reg])) &&
                    !live_in[base + reg]) {
                    live_in[base + reg] = true;
                    changed = true;
                }
            }
        }
    }
    for (block = function->first_block; block; block = block->next) {
        size_t base = (size_t)block->id * function->register_count;
        size_t reg;
        for (reg = 0u; reg < function->register_count; ++reg) {
            if (live_in[base + reg]) {
                if (block_starts[block->id] < intervals[reg].start) {
                    intervals[reg].start = block_starts[block->id];
                }
                if (block_starts[block->id] > intervals[reg].end) {
                    intervals[reg].end = block_starts[block->id];
                }
            }
            if (live_out[base + reg] &&
                block_ends[block->id] > intervals[reg].end) {
                intervals[reg].end = block_ends[block->id];
            }
        }
    }
    rcc_free(definitions);
    rcc_free(uses);
    rcc_free(edge_uses);
    rcc_free(live_in);
    rcc_free(live_out);
    return true;
}

static int mir_alloc_compare_intervals(const void* left_pointer,
                                       const void* right_pointer) {
    const RccMirIntervalOrder* left = left_pointer;
    const RccMirIntervalOrder* right = right_pointer;
    if (left->start < right->start) return -1;
    if (left->start > right->start) return 1;
    if (left->end < right->end) return -1;
    if (left->end > right->end) return 1;
    if (left->reg < right->reg) return -1;
    if (left->reg > right->reg) return 1;
    return 0;
}

static uint64_t mir_alloc_class_mask(
    const RccMirRegisterPolicy* policy, RccMirRegisterClass register_class) {
    if (register_class == RCC_MIR_REGCLASS_FPR) {
        return policy->allocatable_fpr_mask;
    }
    return policy->allocatable_gpr_mask;
}

static uint64_t mir_alloc_caller_saved_mask(
    const RccMirRegisterPolicy* policy, RccMirRegisterClass register_class) {
    if (register_class == RCC_MIR_REGCLASS_FPR) {
        return policy->caller_saved_fpr_mask;
    }
    return policy->caller_saved_gpr_mask;
}

static uint16_t mir_alloc_first_register(uint64_t mask) {
    uint16_t index = 0u;
    while (index < 64u && (mask & (UINT64_C(1) << index)) == 0u) ++index;
    return index;
}

static size_t mir_alloc_popcount(uint64_t value) {
    size_t count = 0u;
    while (value != 0u) {
        value &= value - 1u;
        ++count;
    }
    return count;
}

static bool mir_alloc_graph_has_edge(const uint64_t* graph,
                                    size_t words_per_row,
                                    size_t left, size_t right) {
    return (graph[left * words_per_row + right / 64u] &
            (UINT64_C(1) << (right % 64u))) != 0u;
}

static void mir_alloc_graph_add_edge(uint64_t* graph,
                                     size_t words_per_row,
                                     size_t left, size_t right) {
    graph[left * words_per_row + right / 64u] |=
        UINT64_C(1) << (right % 64u);
    graph[right * words_per_row + left / 64u] |=
        UINT64_C(1) << (left % 64u);
}

static bool mir_alloc_assign_spill(
    const RccMirFunction* function, const RccMirRegisterPolicy* policy,
    RccMirAllocation* allocation, RccMirVReg reg,
    char* error, size_t error_size) {
    RccMirLocation* location = &allocation->locations[reg];
    RccMirType type = function->register_types[reg];
    uint16_t size = mir_alloc_type_size(type, policy);
    uint16_t alignment = mir_alloc_type_alignment(type, policy);
    uint32_t offset;
    if (!mir_alloc_align(allocation->spill_area_size, alignment, &offset) ||
        offset > UINT32_MAX - size) {
        return mir_alloc_error(error, error_size,
                               "MIR spill area exceeds 32-bit offsets");
    }
    location->kind = RCC_MIR_LOCATION_SPILL;
    location->register_class = mir_alloc_register_class(type);
    location->physical_register = UINT16_MAX;
    location->spill_offset = offset;
    location->spill_size = size;
    location->spill_alignment = alignment;
    allocation->spill_area_size = offset + size;
    ++allocation->spill_count;
    return true;
}

static bool mir_alloc_intervals_overlap(RccMirLiveInterval left,
                                        RccMirLiveInterval right) {
    return left.start <= right.end && right.start <= left.end;
}

static bool mir_alloc_linear_scan(
    const RccMirFunction* function, const RccMirRegisterPolicy* policy,
    RccMirAllocation* allocation, RccMirIntervalOrder* order,
    char* error, size_t error_size) {
    RccMirVReg* active;
    size_t active_count = 0u;
    size_t index;
    active = rcc_alloc(function->register_count * sizeof(*active));
    for (index = 0u; index < function->register_count; ++index) {
        RccMirVReg current = order[index].reg;
        RccMirLiveInterval interval = allocation->intervals[current];
        RccMirRegisterClass register_class = interval.register_class;
        uint64_t allowed = mir_alloc_class_mask(policy, register_class);
        uint64_t used = 0u;
        size_t active_index = 0u;
        size_t spill_candidate = SIZE_MAX;
        if (interval.crosses_call) {
            allowed &= ~mir_alloc_caller_saved_mask(policy,
                                                     register_class);
        }
        allowed &= ~interval.forbidden_physical_mask;
        while (active_index < active_count) {
            RccMirVReg active_reg = active[active_index];
            RccMirLiveInterval active_interval =
                allocation->intervals[active_reg];
            RccMirLocation active_location =
                allocation->locations[active_reg];
            if (active_interval.end < interval.start) {
                active[active_index] = active[--active_count];
                continue;
            }
            if (active_location.kind == RCC_MIR_LOCATION_PHYSICAL &&
                active_location.register_class == register_class) {
                used |= UINT64_C(1) <<
                    active_location.physical_register;
                if ((allowed & (UINT64_C(1) <<
                                active_location.physical_register)) != 0u &&
                    (spill_candidate == SIZE_MAX ||
                     active_interval.end > allocation->intervals[
                         active[spill_candidate]].end)) {
                    spill_candidate = active_index;
                }
            }
            ++active_index;
        }
        allowed &= ~used;
        if (allowed != 0u) {
            RccMirLocation* location = &allocation->locations[current];
            location->kind = RCC_MIR_LOCATION_PHYSICAL;
            location->register_class = register_class;
            location->physical_register =
                mir_alloc_first_register(allowed);
            location->spill_offset = UINT32_MAX;
            ++allocation->physical_count;
            active[active_count++] = current;
            continue;
        }
        if (spill_candidate != SIZE_MAX) {
            RccMirVReg victim = active[spill_candidate];
            RccMirLocation victim_location = allocation->locations[victim];
            if (allocation->intervals[victim].end > interval.end) {
                if (!mir_alloc_assign_spill(function, policy, allocation,
                                            victim, error, error_size)) {
                    rcc_free(active);
                    return false;
                }
                allocation->locations[current] = victim_location;
                active[spill_candidate] = current;
                continue;
            }
        }
        if (!mir_alloc_assign_spill(function, policy, allocation, current,
                                    error, error_size)) {
            rcc_free(active);
            return false;
        }
    }
    rcc_free(active);
    return true;
}

static bool mir_alloc_graph_within_budget(size_t register_count) {
    size_t words_per_row;
    size_t cells;
    if (register_count > SIZE_MAX - 63u) return false;
    words_per_row = (register_count + 63u) / 64u;
    if (words_per_row != 0u &&
        register_count > SIZE_MAX / words_per_row) {
        return false;
    }
    cells = register_count * words_per_row;
    return cells <= SIZE_MAX / sizeof(uint64_t) &&
        cells * sizeof(uint64_t) <= MIR_ALLOC_GRAPH_MAX_BYTES;
}

static bool mir_alloc_run_linear_scan(
    const RccMirFunction* function, const RccMirRegisterPolicy* policy,
    RccMirAllocation* allocation, char* error, size_t error_size) {
    RccMirIntervalOrder* order;
    size_t reg;
    bool ok;
    if (function->register_count > SIZE_MAX / sizeof(*order)) {
        return mir_alloc_error(error, error_size,
                               "MIR interval order table is too large");
    }
    order = rcc_alloc(function->register_count * sizeof(*order));
    for (reg = 0u; reg < function->register_count; ++reg) {
        order[reg].reg = (RccMirVReg)reg;
        order[reg].start = allocation->intervals[reg].start;
        order[reg].end = allocation->intervals[reg].end;
    }
    qsort(order, function->register_count, sizeof(*order),
          mir_alloc_compare_intervals);
    ok = mir_alloc_linear_scan(
        function, policy, allocation, order, error, error_size);
    rcc_free(order);
    return ok;
}

static bool mir_alloc_graph_color(
    const RccMirFunction* function, const RccMirRegisterPolicy* policy,
    RccMirAllocation* allocation, char* error, size_t error_size) {
    size_t words_per_row;
    size_t graph_cells;
    size_t graph_bytes;
    uint64_t* graph = NULL;
    uint64_t* allowed_masks = NULL;
    size_t* degrees = NULL;
    size_t* spill_costs = NULL;
    bool* removed = NULL;
    RccMirVReg* stack = NULL;
    size_t stack_count = 0u;
    size_t reg;
    bool ok = false;

    if (function->register_count > SIZE_MAX - 63u) {
        return mir_alloc_error(error, error_size,
                               "MIR interference graph is too large");
    }
    words_per_row = (function->register_count + 63u) / 64u;
    if (words_per_row != 0u &&
        function->register_count > SIZE_MAX / words_per_row) {
        return mir_alloc_error(error, error_size,
                               "MIR interference graph is too large");
    }
    graph_cells = function->register_count * words_per_row;
    if (graph_cells > SIZE_MAX / sizeof(*graph) ||
        function->register_count > SIZE_MAX / sizeof(*allowed_masks) ||
        function->register_count > SIZE_MAX / sizeof(*degrees) ||
        function->register_count > SIZE_MAX / sizeof(*spill_costs) ||
        function->register_count > SIZE_MAX / sizeof(*stack)) {
        return mir_alloc_error(error, error_size,
                               "MIR interference graph is too large");
    }
    graph_bytes = graph_cells * sizeof(*graph);
    graph = rcc_alloc(graph_bytes);
    allowed_masks = rcc_alloc(
        function->register_count * sizeof(*allowed_masks));
    degrees = rcc_alloc(function->register_count * sizeof(*degrees));
    spill_costs = rcc_alloc(
        function->register_count * sizeof(*spill_costs));
    removed = rcc_alloc(function->register_count * sizeof(*removed));
    stack = rcc_alloc(function->register_count * sizeof(*stack));

    for (reg = 0u; reg < function->register_count; ++reg) {
        RccMirLiveInterval interval = allocation->intervals[reg];
        uint64_t allowed = mir_alloc_class_mask(
            policy, interval.register_class);
        allocation->locations[reg].kind = RCC_MIR_LOCATION_SPILL;
        allocation->locations[reg].register_class = interval.register_class;
        allocation->locations[reg].physical_register = UINT16_MAX;
        if (interval.crosses_call) {
            allowed &= ~mir_alloc_caller_saved_mask(
                policy, interval.register_class);
        }
        allowed &= ~interval.forbidden_physical_mask;
        allowed_masks[reg] = allowed;
        spill_costs[reg] = 1u;
    }

    for (reg = 0u; reg < function->register_count; ++reg) {
        size_t other;
        for (other = reg + 1u; other < function->register_count; ++other) {
            if (allocation->intervals[reg].register_class ==
                    allocation->intervals[other].register_class &&
                mir_alloc_intervals_overlap(
                    allocation->intervals[reg],
                    allocation->intervals[other])) {
                mir_alloc_graph_add_edge(graph, words_per_row, reg, other);
                ++degrees[reg];
                ++degrees[other];
            }
        }
    }

    /* Use and definition counts approximate the cost of spilling each value. */
    {
        const RccMirBlock* block;
        for (block = function->first_block; block; block = block->next) {
            const RccMirInstruction* instruction;
            for (instruction = block->first; instruction;
                 instruction = instruction->next) {
                size_t operand;
                if (instruction->definition != RCC_MIR_VREG_NONE &&
                    spill_costs[instruction->definition] < SIZE_MAX) {
                    ++spill_costs[instruction->definition];
                }
                for (operand = 0u; operand < instruction->operand_count;
                     ++operand) {
                    RccMirVReg value = instruction->operands[operand];
                    if (spill_costs[value] < SIZE_MAX) {
                        ++spill_costs[value];
                    }
                }
                if (instruction->opcode == RCC_MIR_CALL &&
                    instruction->callee_value != RCC_MIR_VREG_NONE &&
                    spill_costs[instruction->callee_value] < SIZE_MAX) {
                    ++spill_costs[instruction->callee_value];
                }
            }
        }
    }

    while (stack_count < function->register_count) {
        size_t selected = SIZE_MAX;
        size_t selected_degree = SIZE_MAX;

        for (reg = 0u; reg < function->register_count; ++reg) {
            size_t palette_size;
            if (removed[reg]) continue;
            palette_size = mir_alloc_popcount(allowed_masks[reg]);
            if (degrees[reg] < palette_size) {
                if (selected == SIZE_MAX || degrees[reg] < selected_degree ||
                    (degrees[reg] == selected_degree && reg < selected)) {
                    selected = reg;
                    selected_degree = degrees[reg];
                }
            }
        }

        if (selected == SIZE_MAX) {
            for (reg = 0u; reg < function->register_count; ++reg) {
                int priority_order;
                if (removed[reg]) continue;
                priority_order = selected == SIZE_MAX ? -1 :
                    mir_alloc_compare_fractions(
                        spill_costs[reg], degrees[reg] + 1u,
                        spill_costs[selected], degrees[selected] + 1u);
                if (priority_order < 0 ||
                    (priority_order == 0 &&
                     (degrees[reg] > selected_degree ||
                      (degrees[reg] == selected_degree && reg < selected)))) {
                    selected = reg;
                    selected_degree = degrees[reg];
                }
            }
        }
        if (selected == SIZE_MAX) {
            mir_alloc_error(error, error_size,
                            "MIR interference graph simplify failed");
            goto cleanup;
        }
        removed[selected] = true;
        stack[stack_count++] = (RccMirVReg)selected;
        for (reg = 0u; reg < function->register_count; ++reg) {
            if (!removed[reg] &&
                mir_alloc_graph_has_edge(
                    graph, words_per_row, selected, reg) &&
                degrees[reg] != 0u) {
                --degrees[reg];
            }
        }
    }

    while (stack_count != 0u) {
        RccMirVReg value = stack[--stack_count];
        RccMirLiveInterval interval = allocation->intervals[value];
        uint64_t unavailable = 0u;
        uint64_t available;
        size_t neighbor;
        for (neighbor = 0u; neighbor < function->register_count; ++neighbor) {
            RccMirLocation location;
            if (!mir_alloc_graph_has_edge(
                    graph, words_per_row, value, neighbor)) {
                continue;
            }
            location = allocation->locations[neighbor];
            if (location.kind == RCC_MIR_LOCATION_PHYSICAL &&
                location.register_class == interval.register_class) {
                unavailable |= UINT64_C(1) << location.physical_register;
            }
        }
        available = allowed_masks[value] & ~unavailable;
        if (available != 0u) {
            RccMirLocation* location = &allocation->locations[value];
            location->kind = RCC_MIR_LOCATION_PHYSICAL;
            location->register_class = interval.register_class;
            location->physical_register = mir_alloc_first_register(available);
            location->spill_offset = UINT32_MAX;
            ++allocation->physical_count;
        } else if (!mir_alloc_assign_spill(
                       function, policy, allocation, value,
                       error, error_size)) {
            goto cleanup;
        }
    }
    ok = true;

cleanup:
    rcc_free(graph);
    rcc_free(allowed_masks);
    rcc_free(degrees);
    rcc_free(spill_costs);
    rcc_free(removed);
    rcc_free(stack);
    return ok;
}

void rcc_mir_allocation_release(RccMirAllocation* allocation) {
    if (!allocation) return;
    rcc_free(allocation->intervals);
    rcc_free(allocation->locations);
    memset(allocation, 0, sizeof(*allocation));
}

bool rcc_mir_verify_allocation(
    const RccMirFunction* function, const RccMirRegisterPolicy* policy,
    const RccMirAllocation* allocation, char* error, size_t error_size) {
    size_t left;
    if (error && error_size != 0u) error[0] = '\0';
    if (!function || !mir_alloc_policy_valid(policy) || !allocation ||
        allocation->register_count != function->register_count ||
        (function->register_count != 0u &&
         (!allocation->intervals || !allocation->locations))) {
        return mir_alloc_error(error, error_size,
                               "MIR allocation header is invalid");
    }
    if (!mir_alloc_verify_fixed_constraints(
            function, policy, allocation, error, error_size)) {
        return false;
    }
    for (left = 0u; left < allocation->register_count; ++left) {
        RccMirLiveInterval interval = allocation->intervals[left];
        RccMirLocation location = allocation->locations[left];
        uint64_t class_mask = mir_alloc_class_mask(
            policy, interval.register_class);
        if (interval.start > interval.end ||
            location.register_class != interval.register_class) {
            return mir_alloc_error(error, error_size,
                                   "MIR interval %zu is invalid", left);
        }
        if (location.kind == RCC_MIR_LOCATION_PHYSICAL) {
            uint64_t bit;
            if (location.physical_register >= 64u) {
                return mir_alloc_error(error, error_size,
                                       "MIR physical register is invalid");
            }
            bit = UINT64_C(1) << location.physical_register;
            if ((class_mask & bit) == 0u ||
                (interval.forbidden_physical_mask & bit) != 0u ||
                (interval.crosses_call &&
                 (mir_alloc_caller_saved_mask(
                      policy, interval.register_class) & bit) != 0u)) {
                return mir_alloc_error(
                    error, error_size,
                    "MIR physical register violates target/fixed policy");
            }
        } else if (location.kind == RCC_MIR_LOCATION_SPILL) {
            if (location.spill_size == 0u ||
                location.spill_alignment == 0u ||
                location.spill_offset % location.spill_alignment != 0u ||
                location.spill_offset > allocation->spill_area_size ||
                location.spill_size > allocation->spill_area_size -
                    location.spill_offset) {
                return mir_alloc_error(error, error_size,
                                       "MIR spill location is invalid");
            }
        } else {
            return mir_alloc_error(error, error_size,
                                   "MIR location kind is invalid");
        }
        for (size_t right = 0u; right < left; ++right) {
            RccMirLocation other = allocation->locations[right];
            if (location.kind == RCC_MIR_LOCATION_PHYSICAL &&
                other.kind == RCC_MIR_LOCATION_PHYSICAL &&
                location.register_class == other.register_class &&
                location.physical_register == other.physical_register &&
                mir_alloc_intervals_overlap(interval,
                    allocation->intervals[right])) {
                return mir_alloc_error(
                    error, error_size,
                    "overlapping MIR intervals share a physical register");
            }
        }
    }
    return true;
}

bool rcc_mir_linear_scan_allocate(
    const RccMirFunction* function, const RccMirRegisterPolicy* policy,
    RccMirAllocation* allocation, char* error, size_t error_size) {
    RccMirIntervalOrder* order = NULL;
    size_t* block_starts = NULL;
    size_t* block_ends = NULL;
    size_t* calls = NULL;
    size_t call_count = 0u;
    size_t reg;
    bool result = false;
    if (allocation) memset(allocation, 0, sizeof(*allocation));
    if (error && error_size != 0u) error[0] = '\0';
    if (!function || !allocation || !mir_alloc_policy_valid(policy) ||
        !rcc_mir_verify_function(function, error, error_size)) {
        return false;
    }
    allocation->register_count = function->register_count;
    if (function->register_count == 0u) return true;
    allocation->intervals = rcc_alloc(
        function->register_count * sizeof(*allocation->intervals));
    allocation->locations = rcc_alloc(
        function->register_count * sizeof(*allocation->locations));
    order = rcc_alloc(function->register_count * sizeof(*order));
    for (reg = 0u; reg < function->register_count; ++reg) {
        allocation->intervals[reg].start = SIZE_MAX;
        allocation->intervals[reg].register_class =
            mir_alloc_register_class(function->register_types[reg]);
    }
    if (!mir_alloc_collect_positions(
            function, &block_starts, &block_ends, &calls, &call_count,
            allocation->intervals, error, error_size)) {
        goto cleanup;
    }
    if (!mir_alloc_extend_cfg_liveness(
            function, block_starts, block_ends, allocation->intervals,
            error, error_size)) {
        goto cleanup;
    }
    if (!mir_alloc_apply_fixed_constraints(
            function, policy, allocation->intervals,
            error, error_size)) {
        goto cleanup;
    }
    for (reg = 0u; reg < function->register_count; ++reg) {
        size_t call;
        if (allocation->intervals[reg].start == SIZE_MAX) {
            mir_alloc_error(error, error_size,
                            "MIR register %zu has no live interval", reg);
            goto cleanup;
        }
        for (call = 0u; call < call_count; ++call) {
            if (allocation->intervals[reg].start < calls[call] &&
                calls[call] < allocation->intervals[reg].end) {
                allocation->intervals[reg].crosses_call = true;
                break;
            }
        }
        order[reg].reg = (RccMirVReg)reg;
        order[reg].start = allocation->intervals[reg].start;
        order[reg].end = allocation->intervals[reg].end;
    }
    qsort(order, function->register_count, sizeof(*order),
          mir_alloc_compare_intervals);
    if (!mir_alloc_linear_scan(function, policy, allocation, order,
                               error, error_size) ||
        !mir_alloc_align(allocation->spill_area_size,
                         policy->stack_alignment,
                         &allocation->spill_area_size) ||
        !rcc_mir_verify_allocation(function, policy, allocation,
                                   error, error_size)) {
        if (error && error_size != 0u && error[0] == '\0') {
            mir_alloc_error(error, error_size,
                            "MIR spill area alignment overflow");
        }
        goto cleanup;
    }
    result = true;
cleanup:
    rcc_free(order);
    rcc_free(block_starts);
    rcc_free(block_ends);
    rcc_free(calls);
    if (!result) rcc_mir_allocation_release(allocation);
    return result;
}

bool rcc_mir_graph_color_allocate(
    const RccMirFunction* function, const RccMirRegisterPolicy* policy,
    RccMirAllocation* allocation, char* error, size_t error_size) {
    size_t* block_starts = NULL;
    size_t* block_ends = NULL;
    size_t* calls = NULL;
    size_t call_count = 0u;
    size_t reg;
    bool result = false;
    if (allocation) memset(allocation, 0, sizeof(*allocation));
    if (error && error_size != 0u) error[0] = '\0';
    if (!function || !allocation || !mir_alloc_policy_valid(policy) ||
        !rcc_mir_verify_function(function, error, error_size)) {
        return false;
    }
    allocation->register_count = function->register_count;
    if (function->register_count == 0u) return true;
    if (function->register_count > SIZE_MAX / sizeof(*allocation->intervals) ||
        function->register_count > SIZE_MAX / sizeof(*allocation->locations)) {
        mir_alloc_error(error, error_size, "MIR register table is too large");
        goto cleanup;
    }
    allocation->intervals = rcc_alloc(
        function->register_count * sizeof(*allocation->intervals));
    allocation->locations = rcc_alloc(
        function->register_count * sizeof(*allocation->locations));
    for (reg = 0u; reg < function->register_count; ++reg) {
        allocation->intervals[reg].start = SIZE_MAX;
        allocation->intervals[reg].register_class =
            mir_alloc_register_class(function->register_types[reg]);
    }
    if (!mir_alloc_collect_positions(
            function, &block_starts, &block_ends, &calls, &call_count,
            allocation->intervals, error, error_size) ||
        !mir_alloc_extend_cfg_liveness(
            function, block_starts, block_ends, allocation->intervals,
            error, error_size) ||
        !mir_alloc_apply_fixed_constraints(
            function, policy, allocation->intervals,
            error, error_size)) {
        goto cleanup;
    }
    for (reg = 0u; reg < function->register_count; ++reg) {
        size_t call;
        if (allocation->intervals[reg].start == SIZE_MAX) {
            mir_alloc_error(error, error_size,
                            "MIR register %zu has no live interval", reg);
            goto cleanup;
        }
        for (call = 0u; call < call_count; ++call) {
            if (allocation->intervals[reg].start < calls[call] &&
                calls[call] < allocation->intervals[reg].end) {
                allocation->intervals[reg].crosses_call = true;
                break;
            }
        }
    }
    if (!(mir_alloc_graph_within_budget(function->register_count)
              ? mir_alloc_graph_color(
                    function, policy, allocation, error, error_size)
              : mir_alloc_run_linear_scan(
                    function, policy, allocation, error, error_size)) ||
        !mir_alloc_align(allocation->spill_area_size,
                         policy->stack_alignment,
                         &allocation->spill_area_size) ||
        !rcc_mir_verify_allocation(
            function, policy, allocation, error, error_size)) {
        if (error && error_size != 0u && error[0] == '\0') {
            mir_alloc_error(error, error_size,
                            "MIR spill area alignment overflow");
        }
        goto cleanup;
    }
    result = true;

cleanup:
    rcc_free(block_starts);
    rcc_free(block_ends);
    rcc_free(calls);
    if (!result) rcc_mir_allocation_release(allocation);
    return result;
}
