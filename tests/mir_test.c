#include "mir.h"
#include "mir_alloc.h"
#include "mir_phi.h"
#include "x86_select.h"

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
    verify_ir_to_mir_diamond();
    verify_ir_to_mir_call();
    verify_call_crossing_pressure();
    verify_phi_parallel_copy_cycle();
    verify_x86_critical_edge_selection();
    puts("MIR lowering, allocation, phi-copy, and x86 selection tests passed");
    return 0;
}
