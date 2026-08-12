/*
 * RCC - MIR liveness and linear-scan register allocation
 */

#include "rcc.h"
#include "mir_alloc.h"

#include <stdarg.h>

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
    const RccMirFunction* function, size_t** block_ends_out,
    size_t** calls_out, size_t* call_count_out,
    RccMirLiveInterval* intervals, char* error, size_t error_size) {
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
    block_ends = rcc_alloc(function->block_count * sizeof(*block_ends));
    calls = NULL;
    for (parameter = 0u; parameter < function->parameter_count; ++parameter) {
        RccMirVReg reg = function->parameters[parameter];
        intervals[reg].start = 0u;
        intervals[reg].end = 0u;
    }
    for (block = function->first_block; block; block = block->next) {
        const RccMirInstruction* instruction;
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
            if (instruction->opcode == RCC_MIR_CALL) {
                if (call_count == call_capacity) {
                    size_t next_capacity = call_capacity == 0u
                        ? 8u : call_capacity * 2u;
                    if (next_capacity < call_capacity ||
                        next_capacity > SIZE_MAX / sizeof(*calls)) {
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
    *block_ends_out = block_ends;
    *calls_out = calls;
    *call_count_out = call_count;
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
            function, &block_ends, &calls, &call_count,
            allocation->intervals, error, error_size)) {
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
    rcc_free(block_ends);
    rcc_free(calls);
    if (!result) rcc_mir_allocation_release(allocation);
    return result;
}
