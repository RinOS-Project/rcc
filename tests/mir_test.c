#include "mir.h"
#include "mir_alloc.h"
#include "mir_phi.h"
#include "x86_select.h"
#include "x86_legalize.h"
#include "x86_encode.h"
#include "x86_object.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static RccIrValue append_const(RccIrBlock* block, RccIrType type,
                               uint64_t value)
{
    RccIrInstruction* instruction = rcc_ir_append(
        block, RCC_IR_CONST_INT, type, NULL, 0u, NULL, 0u);
    assert(instruction != NULL);
    rcc_ir_set_immediate(instruction, value);
    return instruction->result;
}

static void append_branch(RccIrBlock* block, RccIrBlockId target)
{
    assert(rcc_ir_append(block, RCC_IR_BRANCH, rcc_ir_type_void(),
                         NULL, 0u, &target, 1u) != NULL);
}

static void verify_x86_abi_mapping(void)
{
    RccX86Abi abi;
    RccMirRegisterPolicy policy;
    RccX86HardwareGpr hardware;
    char error[256];
    rcc_x86_abi_i686(&abi);
    rcc_mir_register_policy_i686(&policy);
    assert(rcc_x86_abi_verify_policy(
        &abi, &policy, error, sizeof(error)));
    assert(abi.integer_argument_count == 0u);
    assert(abi.return_low == RCC_X86_GPR_AX);
    assert(abi.return_high == RCC_X86_GPR_DX);
    assert(rcc_x86_abi_hardware_gpr(&abi, 4u, &hardware));
    assert(hardware == RCC_X86_GPR_SI);
    rcc_x86_abi_x86_64(&abi);
    rcc_mir_register_policy_x86_64(&policy);
    assert(rcc_x86_abi_verify_policy(
        &abi, &policy, error, sizeof(error)));
    assert(abi.integer_argument_count == 6u);
    assert(abi.integer_arguments[0] == RCC_X86_GPR_DI);
    assert(abi.integer_arguments[5] == RCC_X86_GPR_R9);
    assert(rcc_x86_abi_hardware_gpr(&abi, 10u, &hardware));
    assert(hardware == RCC_X86_GPR_R12);
    policy.caller_saved_gpr_mask ^= UINT64_C(1);
    assert(!rcc_x86_abi_verify_policy(
        &abi, &policy, error, sizeof(error)));
    assert(strstr(error, "disagrees") != NULL);
}

static void verify_ir_to_mir_diamond(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType i1 = rcc_ir_type_integer(1u);
    RccIrType parameter_types[] = {i1};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* ir = rcc_ir_function_add(
        module, "diamond", i32, parameter_types, 1u);
    RccIrBlock* entry = rcc_ir_block_add(ir, "entry");
    RccIrBlock* left = rcc_ir_block_add(ir, "left");
    RccIrBlock* right = rcc_ir_block_add(ir, "right");
    RccIrBlock* merge = rcc_ir_block_add(ir, "merge");
    RccIrBlockId branch_targets[] = {left->id, right->id};
    RccIrValue left_value;
    RccIrValue right_value;
    RccIrValue phi_operands[2];
    RccIrBlockId phi_targets[] = {left->id, right->id};
    RccIrInstruction* phi;
    RccMirFunction* mir = NULL;
    RccMirInstruction* mir_phi;
    RccMirRegisterPolicy policy;
    RccMirAllocation allocation;
    char error[256];
    assert(rcc_ir_append(entry, RCC_IR_COND_BRANCH, rcc_ir_type_void(),
                         &ir->parameters[0], 1u, branch_targets, 2u) != NULL);
    left_value = append_const(left, i32, 11u);
    append_branch(left, merge->id);
    right_value = append_const(right, i32, 29u);
    append_branch(right, merge->id);
    phi_operands[0] = left_value;
    phi_operands[1] = right_value;
    phi = rcc_ir_append(merge, RCC_IR_PHI, i32, phi_operands, 2u,
                        phi_targets, 2u);
    assert(phi != NULL);
    assert(rcc_ir_append(merge, RCC_IR_RETURN, rcc_ir_type_void(),
                         &phi->result, 1u, NULL, 0u) != NULL);
    assert(rcc_mir_lower_ir(ir, &mir, error, sizeof(error)));
    assert(error[0] == '\0');
    assert(mir != NULL);
    assert(strcmp(mir->name, "diamond") == 0);
    assert(mir->block_count == 4u);
    assert(mir->register_count == ir->value_count);
    mir_phi = mir->last_block->first;
    assert(mir_phi != NULL && mir_phi->opcode == RCC_MIR_PHI);
    assert(mir_phi->target_count == 2u);
    assert(rcc_mir_verify_function(mir, error, sizeof(error)));
    rcc_mir_register_policy_i686(&policy);
    assert(rcc_mir_linear_scan_allocate(
        mir, &policy, &allocation, error, sizeof(error)));
    assert(allocation.intervals[left_value].start == 2u);
    assert(allocation.intervals[left_value].end == 3u);
    assert(allocation.intervals[right_value].start == 4u);
    assert(allocation.intervals[right_value].end == 5u);
    assert(allocation.spill_count == 0u);
    assert(rcc_mir_verify_allocation(
        mir, &policy, &allocation, error, sizeof(error)));
    rcc_mir_allocation_release(&allocation);
    mir_phi->targets[0] = RCC_MIR_BLOCK_NONE;
    assert(!rcc_mir_verify_function(mir, error, sizeof(error)));
    assert(strstr(error, "phi") != NULL || strstr(error, "target") != NULL);
    rcc_mir_function_destroy(mir);
    rcc_ir_module_destroy(module);
}

static void verify_call_crossing_pressure(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType parameters[] = {i32, i32, i32};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* ir = rcc_ir_function_add(
        module, "pressure", i32, parameters, 3u);
    RccIrBlock* entry = rcc_ir_block_add(ir, "entry");
    RccIrInstruction* call = rcc_ir_append(
        entry, RCC_IR_CALL, rcc_ir_type_void(), NULL, 0u, NULL, 0u);
    RccIrValue first_operands[] = {
        ir->parameters[0], ir->parameters[1]
    };
    RccIrInstruction* first;
    RccIrValue second_operands[2];
    RccIrInstruction* second;
    RccMirFunction* mir = NULL;
    RccMirRegisterPolicy policy;
    RccMirAllocation allocation;
    size_t crossing_parameters = 0u;
    char error[256];
    assert(call != NULL);
    rcc_ir_set_callee(call, "barrier");
    first = rcc_ir_append(entry, RCC_IR_ADD, i32, first_operands, 2u,
                          NULL, 0u);
    assert(first != NULL);
    second_operands[0] = first->result;
    second_operands[1] = ir->parameters[2];
    second = rcc_ir_append(entry, RCC_IR_ADD, i32, second_operands, 2u,
                           NULL, 0u);
    assert(second != NULL);
    assert(rcc_ir_append(entry, RCC_IR_RETURN, rcc_ir_type_void(),
                         &second->result, 1u, NULL, 0u) != NULL);
    assert(rcc_mir_lower_ir(ir, &mir, error, sizeof(error)));
    memset(&policy, 0, sizeof(policy));
    policy.allocatable_gpr_mask = UINT64_C(0x7);
    policy.caller_saved_gpr_mask = UINT64_C(0x3);
    policy.pointer_size = 4u;
    policy.stack_alignment = 16u;
    assert(rcc_mir_linear_scan_allocate(
        mir, &policy, &allocation, error, sizeof(error)));
    for (size_t index = 0u; index < 3u; ++index) {
        RccMirVReg reg = mir->parameters[index];
        assert(allocation.intervals[reg].crosses_call);
        if (allocation.locations[reg].kind ==
            RCC_MIR_LOCATION_PHYSICAL) {
            assert(allocation.locations[reg].physical_register == 2u);
        } else {
            assert(allocation.locations[reg].kind ==
                   RCC_MIR_LOCATION_SPILL);
        }
        ++crossing_parameters;
    }
    assert(crossing_parameters == 3u);
    assert(allocation.spill_count >= 2u);
    assert(allocation.spill_area_size % 16u == 0u);
    assert(rcc_mir_verify_allocation(
        mir, &policy, &allocation, error, sizeof(error)));
    rcc_mir_allocation_release(&allocation);
    rcc_mir_function_destroy(mir);
    rcc_ir_module_destroy(module);
}

static void verify_fixed_register_constraints_target(bool x64)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType parameters[] = {i32, i32, i32};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* ir = rcc_ir_function_add(
        module, "fixed", i32, parameters, 3u);
    RccIrBlock* entry = rcc_ir_block_add(ir, "entry");
    RccIrValue div_operands[] = {
        ir->parameters[0], ir->parameters[1]
    };
    RccIrInstruction* division = rcc_ir_append(
        entry, RCC_IR_UDIV, i32, div_operands, 2u, NULL, 0u);
    RccIrValue shift_operands[2];
    RccIrInstruction* shift;
    RccIrValue sum_operands[2];
    RccIrInstruction* sum;
    RccMirFunction* mir = NULL;
    RccMirRegisterPolicy policy;
    RccMirAllocation allocation;
    RccMirPhiPlan plan;
    RccX86Function* selected = NULL;
    RccX86LegalFunction* legal = NULL;
    RccX86LegalInstruction* legal_divide = NULL;
    RccX86LegalInstruction* legal_shift = NULL;
    RccX86LegalInstruction* legal_binary = NULL;
    RccX86EncodedFunction fixed_encoded;
    uint64_t saved_forbidden;
    char error[256];
    assert(division != NULL);
    shift_operands[0] = division->result;
    shift_operands[1] = ir->parameters[2];
    shift = rcc_ir_append(entry, RCC_IR_SHL, i32,
                          shift_operands, 2u, NULL, 0u);
    assert(shift != NULL);
    sum_operands[0] = shift->result;
    sum_operands[1] = ir->parameters[0];
    sum = rcc_ir_append(entry, RCC_IR_ADD, i32,
                        sum_operands, 2u, NULL, 0u);
    assert(sum != NULL);
    assert(rcc_ir_append(entry, RCC_IR_RETURN, rcc_ir_type_void(),
                         &sum->result, 1u, NULL, 0u) != NULL);
    assert(rcc_mir_lower_ir(ir, &mir, error, sizeof(error)));
    if (x64) rcc_mir_register_policy_x86_64(&policy);
    else rcc_mir_register_policy_i686(&policy);
    assert(rcc_mir_linear_scan_allocate(
        mir, &policy, &allocation, error, sizeof(error)));
    assert((allocation.intervals[division->result]
                .forbidden_physical_mask &
            policy.division_fixed_gpr_mask) ==
           policy.division_fixed_gpr_mask);
    assert((allocation.intervals[division->result]
                .forbidden_physical_mask &
            policy.shift_count_fixed_gpr_mask) ==
           policy.shift_count_fixed_gpr_mask);
    assert((allocation.intervals[shift->result]
                .forbidden_physical_mask &
            policy.shift_count_fixed_gpr_mask) != 0u);
    for (size_t reg = 0u; reg < allocation.register_count; ++reg) {
        RccMirLocation location = allocation.locations[reg];
        if (location.kind == RCC_MIR_LOCATION_PHYSICAL) {
            assert((allocation.intervals[reg].forbidden_physical_mask &
                    (UINT64_C(1) << location.physical_register)) == 0u);
        }
    }
    saved_forbidden = allocation.intervals[division->result]
        .forbidden_physical_mask;
    allocation.intervals[division->result].forbidden_physical_mask &=
        ~policy.division_fixed_gpr_mask;
    assert(!rcc_mir_verify_allocation(
        mir, &policy, &allocation, error, sizeof(error)));
    assert(strstr(error, "fixed-register") != NULL);
    allocation.intervals[division->result].forbidden_physical_mask =
        saved_forbidden;
    assert(rcc_mir_build_phi_plan(
        mir, &policy, &allocation, &plan, error, sizeof(error)));
    assert(rcc_x86_select_function(
        mir,
        x64 ? RCC_X86_TARGET_X86_64 : RCC_X86_TARGET_I686,
        &policy, &allocation, &plan, &selected,
        error, sizeof(error)));
    assert(rcc_x86_legalize_function(
        selected, &policy, &legal, error, sizeof(error)));
    assert(legal->block_count == 1u);
    assert(legal->legal_instruction_count >= 9u);
    for (RccX86LegalInstruction* instruction = legal->first_block->first;
         instruction; instruction = instruction->next) {
        if (instruction->opcode == RCC_X86_LEGAL_DIVIDE) {
            legal_divide = instruction;
        } else if (instruction->opcode == RCC_X86_LEGAL_SHIFT) {
            legal_shift = instruction;
        } else if (instruction->opcode == RCC_X86_LEGAL_BINARY) {
            legal_binary = instruction;
        }
    }
    assert(legal_divide != NULL);
    assert(legal_shift != NULL);
    assert(legal_binary != NULL);
    assert(legal_divide->previous->opcode ==
           RCC_X86_LEGAL_PREPARE_UNSIGNED_DIVIDEND);
    assert(legal_divide->previous->previous->destination.gpr ==
           RCC_X86_GPR_AX);
    assert(legal_divide->next->operands[0].gpr == RCC_X86_GPR_AX);
    assert(legal_shift->previous->destination.gpr == RCC_X86_GPR_CX);
    assert(legal_binary->previous->opcode == RCC_X86_LEGAL_COPY);
    assert(legal_binary->previous->destination.kind ==
           legal_binary->destination.kind);
    assert(rcc_x86_verify_legal_function(
        legal, &policy, error, sizeof(error)));
    assert(rcc_x86_encode_function(
        legal, &policy, &fixed_encoded, error, sizeof(error)));
    assert(rcc_x86_verify_encoded_function(
        &fixed_encoded, error, sizeof(error)));
    assert(fixed_encoded.relocation_count == 0u);
    rcc_x86_encoded_function_release(&fixed_encoded);
    legal_divide->operands[0].kind = RCC_X86_VALUE_GPR;
    legal_divide->operands[0].gpr = RCC_X86_GPR_DX;
    assert(!rcc_x86_verify_legal_function(
        legal, &policy, error, sizeof(error)));
    assert(strstr(error, "division fixed-register") != NULL);
    rcc_x86_legal_function_destroy(legal);
    rcc_x86_function_destroy(selected);
    rcc_mir_phi_plan_release(&plan);
    rcc_mir_allocation_release(&allocation);
    rcc_mir_function_destroy(mir);
    rcc_ir_module_destroy(module);
}

static void verify_fixed_register_constraints(void)
{
    verify_fixed_register_constraints_target(false);
    verify_fixed_register_constraints_target(true);
}

static void verify_phi_parallel_copy_cycle(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType i1 = rcc_ir_type_integer(1u);
    RccIrType parameters[] = {i1};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* ir = rcc_ir_function_add(
        module, "phi_cycle", i32, parameters, 1u);
    RccIrBlock* entry = rcc_ir_block_add(ir, "entry");
    RccIrBlock* left = rcc_ir_block_add(ir, "left");
    RccIrBlock* right = rcc_ir_block_add(ir, "right");
    RccIrBlock* merge = rcc_ir_block_add(ir, "merge");
    RccIrBlockId branch_targets[] = {left->id, right->id};
    RccIrValue left_first;
    RccIrValue left_second;
    RccIrValue right_first;
    RccIrValue right_second;
    RccIrValue first_inputs[2];
    RccIrValue second_inputs[2];
    RccIrBlockId input_blocks[] = {left->id, right->id};
    RccIrInstruction* first_phi;
    RccIrInstruction* second_phi;
    RccIrValue sum_inputs[2];
    RccIrInstruction* sum;
    RccMirFunction* mir = NULL;
    RccMirRegisterPolicy policy;
    RccMirAllocation allocation;
    RccMirPhiPlan plan;
    RccMirLocation swapped;
    char error[256];
    assert(rcc_ir_append(entry, RCC_IR_COND_BRANCH, rcc_ir_type_void(),
                         &ir->parameters[0], 1u, branch_targets, 2u) != NULL);
    left_first = append_const(left, i32, 1u);
    left_second = append_const(left, i32, 2u);
    append_branch(left, merge->id);
    right_first = append_const(right, i32, 3u);
    right_second = append_const(right, i32, 4u);
    append_branch(right, merge->id);
    first_inputs[0] = left_first;
    first_inputs[1] = right_first;
    first_phi = rcc_ir_append(merge, RCC_IR_PHI, i32, first_inputs, 2u,
                              input_blocks, 2u);
    assert(first_phi != NULL);
    second_inputs[0] = left_second;
    second_inputs[1] = right_second;
    second_phi = rcc_ir_append(merge, RCC_IR_PHI, i32, second_inputs, 2u,
                               input_blocks, 2u);
    assert(second_phi != NULL);
    sum_inputs[0] = first_phi->result;
    sum_inputs[1] = second_phi->result;
    sum = rcc_ir_append(merge, RCC_IR_ADD, i32, sum_inputs, 2u,
                        NULL, 0u);
    assert(sum != NULL);
    assert(rcc_ir_append(merge, RCC_IR_RETURN, rcc_ir_type_void(),
                         &sum->result, 1u, NULL, 0u) != NULL);
    assert(rcc_mir_lower_ir(ir, &mir, error, sizeof(error)));
    memset(&policy, 0, sizeof(policy));
    policy.allocatable_gpr_mask = UINT64_C(0x3);
    policy.pointer_size = 4u;
    policy.stack_alignment = 16u;
    assert(rcc_mir_linear_scan_allocate(
        mir, &policy, &allocation, error, sizeof(error)));
    assert(allocation.locations[first_phi->result].kind ==
           RCC_MIR_LOCATION_PHYSICAL);
    assert(allocation.locations[second_phi->result].kind ==
           RCC_MIR_LOCATION_PHYSICAL);
    swapped = allocation.locations[first_phi->result];
    allocation.locations[first_phi->result] =
        allocation.locations[second_phi->result];
    allocation.locations[second_phi->result] = swapped;
    assert(rcc_mir_verify_allocation(
        mir, &policy, &allocation, error, sizeof(error)));
    assert(rcc_mir_build_phi_plan(
        mir, &policy, &allocation, &plan, error, sizeof(error)));
    assert(plan.edge_count == 2u);
    assert(plan.has_cycle_temporary);
    assert(plan.cycle_temporary_size >= 4u);
    assert(plan.frame_size % 16u == 0u);
    for (size_t edge = 0u; edge < plan.edge_count; ++edge) {
        assert(plan.edges[edge].copy_count == 2u);
        assert(plan.edges[edge].move_count == 3u);
        assert(!plan.edges[edge].requires_edge_block);
        assert(plan.edges[edge].moves[0].cycle_break);
    }
    assert(rcc_mir_verify_phi_plan(
        mir, &policy, &allocation, &plan, error, sizeof(error)));
    plan.edges[0].moves[1].source =
        plan.edges[0].moves[1].destination;
    assert(!rcc_mir_verify_phi_plan(
        mir, &policy, &allocation, &plan, error, sizeof(error)));
    assert(strstr(error, "parallel-copy semantics") != NULL);
    rcc_mir_phi_plan_release(&plan);
    rcc_mir_allocation_release(&allocation);
    rcc_mir_function_destroy(mir);
    rcc_ir_module_destroy(module);
}

static void verify_x86_critical_edge_selection_target(
    RccX86Target target)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType i1 = rcc_ir_type_integer(1u);
    RccIrType parameters[] = {i1};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* ir = rcc_ir_function_add(
        module, "critical", i32, parameters, 1u);
    RccIrBlock* entry = rcc_ir_block_add(ir, "entry");
    RccIrBlock* pivot = rcc_ir_block_add(ir, "pivot");
    RccIrBlock* side = rcc_ir_block_add(ir, "side");
    RccIrBlock* other = rcc_ir_block_add(ir, "other");
    RccIrBlock* merge = rcc_ir_block_add(ir, "merge");
    RccIrBlockId entry_targets[] = {pivot->id, side->id};
    RccIrBlockId pivot_targets[] = {merge->id, other->id};
    RccIrBlockId phi_targets[] = {pivot->id, side->id};
    RccIrValue pivot_value;
    RccIrValue side_value;
    RccIrValue other_value;
    RccIrValue phi_values[2];
    RccIrInstruction* phi;
    RccMirFunction* mir = NULL;
    RccMirRegisterPolicy policy;
    RccMirAllocation allocation;
    RccMirPhiPlan plan;
    RccX86Function* selected = NULL;
    RccX86Block* pivot_machine;
    RccX86Block* split;
    char error[256];
    assert(rcc_ir_append(entry, RCC_IR_COND_BRANCH, rcc_ir_type_void(),
                         &ir->parameters[0], 1u,
                         entry_targets, 2u) != NULL);
    pivot_value = append_const(pivot, i32, 1u);
    assert(rcc_ir_append(pivot, RCC_IR_COND_BRANCH, rcc_ir_type_void(),
                         &ir->parameters[0], 1u,
                         pivot_targets, 2u) != NULL);
    side_value = append_const(side, i32, 3u);
    append_branch(side, merge->id);
    other_value = append_const(other, i32, 9u);
    assert(rcc_ir_append(other, RCC_IR_RETURN, rcc_ir_type_void(),
                         &other_value, 1u, NULL, 0u) != NULL);
    phi_values[0] = pivot_value;
    phi_values[1] = side_value;
    phi = rcc_ir_append(merge, RCC_IR_PHI, i32, phi_values, 2u,
                        phi_targets, 2u);
    assert(phi != NULL);
    assert(rcc_ir_append(merge, RCC_IR_RETURN, rcc_ir_type_void(),
                         &phi->result, 1u, NULL, 0u) != NULL);
    assert(rcc_mir_lower_ir(ir, &mir, error, sizeof(error)));
    if (target == RCC_X86_TARGET_X86_64) {
        rcc_mir_register_policy_x86_64(&policy);
    } else {
        rcc_mir_register_policy_i686(&policy);
    }
    assert(rcc_mir_linear_scan_allocate(
        mir, &policy, &allocation, error, sizeof(error)));
    assert(rcc_mir_build_phi_plan(
        mir, &policy, &allocation, &plan, error, sizeof(error)));
    assert(plan.edge_count == 2u);
    assert(plan.edges[0].requires_edge_block ||
           plan.edges[1].requires_edge_block);
    assert(rcc_x86_select_function(
        mir, target, &policy, &allocation, &plan,
        &selected, error, sizeof(error)));
    assert(selected->original_block_count == 5u);
    assert(selected->block_count == 6u);
    assert(selected->pointer_size ==
           (target == RCC_X86_TARGET_X86_64 ? 8u : 4u));
    pivot_machine = selected->first_block->next;
    assert(pivot_machine->id == pivot->id);
    assert(pivot_machine->last->opcode == RCC_X86_JUMP_IF);
    assert(pivot_machine->last->targets[0] == 5u);
    split = selected->last_block;
    assert(split->edge_split);
    assert(split->edge_predecessor == pivot->id);
    assert(split->edge_successor == merge->id);
    assert(split->first->opcode == RCC_X86_COPY);
    assert(split->last->opcode == RCC_X86_JUMP);
    assert(split->last->targets[0] == merge->id);
    assert(rcc_x86_verify_function(
        selected, &policy, error, sizeof(error)));
    split->last->targets[0] = (uint32_t)selected->block_count;
    assert(!rcc_x86_verify_function(
        selected, &policy, error, sizeof(error)));
    assert(strstr(error, "target") != NULL);
    rcc_x86_function_destroy(selected);
    rcc_mir_phi_plan_release(&plan);
    rcc_mir_allocation_release(&allocation);
    rcc_mir_function_destroy(mir);
    rcc_ir_module_destroy(module);
}

static void verify_x86_critical_edge_selection(void)
{
    verify_x86_critical_edge_selection_target(RCC_X86_TARGET_I686);
    verify_x86_critical_edge_selection_target(RCC_X86_TARGET_X86_64);
}

static void set_physical_location(RccMirLocation* location,
                                  uint16_t physical_register)
{
    memset(location, 0, sizeof(*location));
    location->kind = RCC_MIR_LOCATION_PHYSICAL;
    location->register_class = RCC_MIR_REGCLASS_GPR;
    location->physical_register = physical_register;
    location->spill_offset = UINT32_MAX;
}

static void verify_sysv_call_legalization_target(bool x64)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType parameters[] = {i32, i32, i32, i32, i32, i32, i32};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* ir = rcc_ir_function_add(
        module, "sysv_call", i32, parameters, 7u);
    RccIrBlock* entry = rcc_ir_block_add(ir, "entry");
    RccIrInstruction* call = rcc_ir_append(
        entry, RCC_IR_CALL, i32, ir->parameters, 7u, NULL, 0u);
    RccMirFunction* mir = NULL;
    RccMirRegisterPolicy policy;
    RccMirAllocation allocation;
    RccMirPhiPlan plan;
    RccX86Function* selected = NULL;
    RccX86LegalFunction* legal = NULL;
    RccX86EncodedFunction encoded;
    ObjectFile* object;
    ObjSection* text;
    ObjSymbol* function_symbol;
    ObjSymbol* call_symbol;
    RccX86LegalInstruction* legal_call = NULL;
    size_t incoming_count = 0u;
    size_t outgoing_count = 0u;
    char error[256];
    assert(call != NULL);
    rcc_ir_set_callee(call, "callee7");
    assert(rcc_ir_append(entry, RCC_IR_RETURN, rcc_ir_type_void(),
                         &call->result, 1u, NULL, 0u) != NULL);
    assert(rcc_mir_lower_ir(ir, &mir, error, sizeof(error)));
    if (x64) rcc_mir_register_policy_x86_64(&policy);
    else rcc_mir_register_policy_i686(&policy);
    assert(rcc_mir_linear_scan_allocate(
        mir, &policy, &allocation, error, sizeof(error)));
    if (x64) {
        static const uint16_t registers[] = {
            3u, 4u, 5u, 6u, 7u, 8u, 9u,
        };
        for (size_t index = 0u; index < 7u; ++index) {
            set_physical_location(
                &allocation.locations[mir->parameters[index]],
                registers[index]);
        }
        set_physical_location(&allocation.locations[call->result], 10u);
        assert(rcc_mir_verify_allocation(
            mir, &policy, &allocation, error, sizeof(error)));
    }
    assert(rcc_mir_build_phi_plan(
        mir, &policy, &allocation, &plan, error, sizeof(error)));
    assert(rcc_x86_select_function(
        mir,
        x64 ? RCC_X86_TARGET_X86_64 : RCC_X86_TARGET_I686,
        &policy, &allocation, &plan, &selected,
        error, sizeof(error)));
    assert(selected->parameter_count == 7u);
    assert(rcc_mir_type_equal(
        selected->return_type, rcc_mir_type_integer(32u)));
    assert(rcc_x86_legalize_function(
        selected, &policy, &legal, error, sizeof(error)));
    assert(legal->parameter_ingress_complete);
    assert(legal->outgoing_stack_size == (x64 ? 16u : 32u));
    assert(legal->outgoing_stack_offset + legal->outgoing_stack_size ==
           legal->frame_size);
    assert(legal->frame_plan_complete);
    assert(legal->source_frame_size <= legal->callee_save_area_offset);
    assert(legal->stack_adjustment == legal->frame_size +
           legal->stack_alignment_padding);
    assert((legal->stack_adjustment + 2u * policy.pointer_size) %
           policy.stack_alignment == 0u);
    assert(legal->stack_alignment_padding == (x64 ? 0u : 8u));
    assert(!x64 || legal->has_parallel_copy_temporary);
    if (x64) {
        assert(legal->callee_save_count == 2u);
        assert(legal->callee_saves[0].gpr == RCC_X86_GPR_BX);
        assert(legal->callee_saves[1].gpr == RCC_X86_GPR_R12);
        assert(legal->callee_saved_gpr_mask ==
               ((UINT32_C(1) << RCC_X86_GPR_BX) |
                (UINT32_C(1) << RCC_X86_GPR_R12)));
    }
    for (RccX86LegalInstruction* instruction = legal->first_block->first;
         instruction; instruction = instruction->next) {
        if (instruction->opcode == RCC_X86_LEGAL_CALL) {
            legal_call = instruction;
        }
        if (instruction->opcode == RCC_X86_LEGAL_COPY) {
            if (instruction->operands[0].kind ==
                RCC_X86_VALUE_INCOMING_ARGUMENT) ++incoming_count;
            if (instruction->destination.kind ==
                RCC_X86_VALUE_OUTGOING_ARGUMENT) ++outgoing_count;
        }
    }
    assert(legal_call != NULL);
    assert(strcmp(legal_call->symbol, "callee7") == 0);
    assert(legal_call->auxiliary == (x64 ? 8u : 28u));
    assert(incoming_count >= (x64 ? 1u : 7u));
    assert(outgoing_count == (x64 ? 1u : 7u));
    assert(legal->first_block->last->opcode == RCC_X86_LEGAL_RETURN);
    assert(legal->first_block->last->previous->destination.gpr ==
           RCC_X86_GPR_AX);
    assert(rcc_x86_verify_legal_function(
        legal, &policy, error, sizeof(error)));
    assert(rcc_x86_encode_function(
        legal, &policy, &encoded, error, sizeof(error)));
    assert(rcc_x86_verify_encoded_function(
        &encoded, error, sizeof(error)));
    assert(encoded.code[0] == 0x55u);
    assert(encoded.code[encoded.code_size - 1u] == 0xc3u);
    assert(encoded.relocation_count == 1u);
    assert(strcmp(encoded.relocations[0].symbol, "callee7") == 0);
    assert(encoded.relocations[0].offset >= 1u);
    assert(encoded.code[encoded.relocations[0].offset - 1u] == 0xe8u);
    object = objfile_new(
        "<x86-encoder-mismatch>", x64 ? ARCH_X86 : ARCH_X64);
    assert(!rcc_x86_object_add_function(
        object, "sysv_call", SYM_GLOBAL, &encoded,
        error, sizeof(error)));
    assert(strstr(error, "architecture") != NULL);
    assert(object->section_count == 0 && object->symbol_count == 0);
    objfile_free(object);
    object = objfile_new(
        "<x86-encoder>", x64 ? ARCH_X64 : ARCH_X86);
    assert(rcc_x86_object_add_function(
        object, "sysv_call", SYM_GLOBAL, &encoded,
        error, sizeof(error)));
    text = objfile_get_section(object, ".text");
    function_symbol = objfile_find_symbol(object, "sysv_call");
    call_symbol = objfile_find_symbol(object, "callee7");
    assert(text != NULL && text->type == SECT_CODE);
    assert((text->flags & SECT_FLAG_WRITE) == 0u);
    assert(text->size == encoded.code_size);
    assert(function_symbol != NULL && function_symbol->section == 0);
    assert(function_symbol->value == 0u);
    assert(function_symbol->size == encoded.code_size);
    assert(call_symbol != NULL && call_symbol->section == -1);
    assert(text->relocs != NULL && text->relocs->next == NULL);
    assert(text->relocs->type == RELOC_REL32);
    assert(text->relocs->offset == encoded.relocations[0].offset);
    assert(strcmp(text->relocs->symbol_name, "callee7") == 0);
    assert(!rcc_x86_object_add_function(
        object, "sysv_call", SYM_GLOBAL, &encoded,
        error, sizeof(error)));
    assert(strstr(error, "duplicate") != NULL);
    objfile_free(object);
    encoded.relocations[0].offset = (uint32_t)encoded.code_size;
    assert(!rcc_x86_verify_encoded_function(
        &encoded, error, sizeof(error)));
    assert(strstr(error, "relocation") != NULL);
    rcc_x86_encoded_function_release(&encoded);
    if (legal->callee_save_count != 0u) {
        uint32_t original_offset = legal->callee_saves[0].frame_offset;
        legal->callee_saves[0].frame_offset += policy.pointer_size;
        assert(!rcc_x86_verify_legal_function(
            legal, &policy, error, sizeof(error)));
        assert(strstr(error, "callee-save") != NULL);
        legal->callee_saves[0].frame_offset = original_offset;
    }
    ++legal->stack_adjustment;
    assert(!rcc_x86_verify_legal_function(
        legal, &policy, error, sizeof(error)));
    assert(strstr(error, "frame plan") != NULL);
    --legal->stack_adjustment;
    legal_call->auxiliary = legal->outgoing_stack_size +
        policy.pointer_size;
    assert(!rcc_x86_verify_legal_function(
        legal, &policy, error, sizeof(error)));
    assert(strstr(error, "call stack area") != NULL);
    rcc_x86_legal_function_destroy(legal);
    rcc_x86_function_destroy(selected);
    rcc_mir_phi_plan_release(&plan);
    rcc_mir_allocation_release(&allocation);
    rcc_mir_function_destroy(mir);
    rcc_ir_module_destroy(module);
}

static void verify_sysv_call_legalization(void)
{
    verify_sysv_call_legalization_target(false);
    verify_sysv_call_legalization_target(true);
}

static void verify_ir_to_mir_call(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* ir = rcc_ir_function_add(
        module, "caller", i32, NULL, 0u);
    RccIrBlock* entry = rcc_ir_block_add(ir, "entry");
    RccIrValue argument = append_const(entry, i32, 7u);
    RccIrInstruction* call = rcc_ir_append(
        entry, RCC_IR_CALL, i32, &argument, 1u, NULL, 0u);
    RccMirFunction* mir = NULL;
    char error[256];
    assert(call != NULL);
    rcc_ir_set_callee(call, "callee");
    assert(rcc_ir_append(entry, RCC_IR_RETURN, rcc_ir_type_void(),
                         &call->result, 1u, NULL, 0u) != NULL);
    assert(rcc_mir_lower_ir(ir, &mir, error, sizeof(error)));
    assert(mir->first_block->first->next->opcode == RCC_MIR_CALL);
    assert(strcmp(mir->first_block->first->next->callee, "callee") == 0);
    assert(rcc_mir_verify_function(mir, error, sizeof(error)));
    rcc_mir_function_destroy(mir);
    rcc_ir_module_destroy(module);
}

int main(void)
{
    verify_x86_abi_mapping();
    verify_ir_to_mir_diamond();
    verify_ir_to_mir_call();
    verify_call_crossing_pressure();
    verify_fixed_register_constraints();
    verify_phi_parallel_copy_cycle();
    verify_x86_critical_edge_selection();
    verify_sysv_call_legalization();
    puts("MIR lowering, allocation, phi-copy, and x86 selection tests passed");
    return 0;
}
