#include "mir.h"

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
    mir_phi->targets[0] = RCC_MIR_BLOCK_NONE;
    assert(!rcc_mir_verify_function(mir, error, sizeof(error)));
    assert(strstr(error, "phi") != NULL || strstr(error, "target") != NULL);
    rcc_mir_function_destroy(mir);
    rcc_ir_module_destroy(module);
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
    puts("Target-independent MIR lowering and verifier tests passed");
    return 0;
}
