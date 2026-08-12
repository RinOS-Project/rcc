#include "ir_pass.h"

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

static RccIrValue append_alloca(RccIrBlock* block, uint64_t size)
{
    RccIrInstruction* instruction = rcc_ir_append(
        block, RCC_IR_ALLOCA, rcc_ir_type_pointer(0u), NULL, 0u,
        NULL, 0u);
    assert(instruction != NULL);
    rcc_ir_set_immediate(instruction, size);
    return instruction->result;
}

static void append_store(RccIrBlock* block, RccIrValue value,
                         RccIrValue address)
{
    RccIrValue operands[] = {value, address};
    assert(rcc_ir_append(block, RCC_IR_STORE, rcc_ir_type_void(),
                         operands, 2u, NULL, 0u) != NULL);
}

static RccIrValue append_load(RccIrBlock* block, RccIrType type,
                              RccIrValue address)
{
    RccIrInstruction* instruction = rcc_ir_append(
        block, RCC_IR_LOAD, type, &address, 1u, NULL, 0u);
    assert(instruction != NULL);
    return instruction->result;
}

static void append_branch(RccIrBlock* block, RccIrBlockId target)
{
    assert(rcc_ir_append(block, RCC_IR_BRANCH, rcc_ir_type_void(),
                         NULL, 0u, &target, 1u) != NULL);
}

static void append_cond_branch(RccIrBlock* block, RccIrValue condition,
                               RccIrBlockId then_target,
                               RccIrBlockId else_target)
{
    RccIrBlockId targets[] = {then_target, else_target};
    assert(rcc_ir_append(block, RCC_IR_COND_BRANCH, rcc_ir_type_void(),
                         &condition, 1u, targets, 2u) != NULL);
}

static void append_return(RccIrBlock* block, RccIrValue value)
{
    assert(rcc_ir_append(block, RCC_IR_RETURN, rcc_ir_type_void(),
                         &value, 1u, NULL, 0u) != NULL);
}

static size_t count_opcode(const RccIrFunction* function,
                           RccIrOpcode opcode)
{
    size_t count = 0u;
    const RccIrBlock* block;
    for (block = function->first_block; block; block = block->next) {
        const RccIrInstruction* instruction;
        for (instruction = block->first; instruction;
             instruction = instruction->next) {
            if (instruction->opcode == opcode) ++count;
        }
    }
    return count;
}

static void verify_diamond_promotion(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType i1 = rcc_ir_type_integer(1u);
    RccIrType parameters[] = {i1};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "diamond", i32, parameters, 1u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrBlock* left = rcc_ir_block_add(function, "left");
    RccIrBlock* right = rcc_ir_block_add(function, "right");
    RccIrBlock* merge = rcc_ir_block_add(function, "merge");
    RccIrValue address = append_alloca(entry, 4u);
    RccIrValue zero = append_const(entry, i32, 0u);
    RccIrValue one;
    RccIrValue two;
    RccIrValue result;
    RccIrMem2RegStats stats;
    char error[256];
    append_store(entry, zero, address);
    append_cond_branch(entry, function->parameters[0], left->id, right->id);
    one = append_const(left, i32, 1u);
    append_store(left, one, address);
    append_branch(left, merge->id);
    two = append_const(right, i32, 2u);
    append_store(right, two, address);
    append_branch(right, merge->id);
    result = append_load(merge, i32, address);
    append_return(merge, result);
    if (!rcc_ir_mem2reg(function, &stats, error, sizeof(error))) {
        fprintf(stderr, "diamond mem2reg failed: %s\n", error);
        assert(0);
    }
    assert(error[0] == '\0');
    assert(stats.promoted_allocas == 1u);
    assert(stats.removed_loads == 1u);
    assert(stats.removed_stores == 3u);
    assert(stats.inserted_phis == 1u);
    assert(count_opcode(function, RCC_IR_ALLOCA) == 0u);
    assert(count_opcode(function, RCC_IR_LOAD) == 0u);
    assert(count_opcode(function, RCC_IR_STORE) == 0u);
    assert(count_opcode(function, RCC_IR_PHI) == 1u);
    assert(rcc_ir_verify_function(function, error, sizeof(error)));
    rcc_ir_module_destroy(module);
}

static void verify_loop_promotion(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "loop", i32, NULL, 0u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrBlock* condition = rcc_ir_block_add(function, "condition");
    RccIrBlock* body = rcc_ir_block_add(function, "body");
    RccIrBlock* exit = rcc_ir_block_add(function, "exit");
    RccIrValue address = append_alloca(entry, 4u);
    RccIrValue zero = append_const(entry, i32, 0u);
    RccIrValue current;
    RccIrValue limit;
    RccIrValue compare_operands[2];
    RccIrInstruction* compare;
    RccIrValue one;
    RccIrValue add_operands[2];
    RccIrInstruction* add;
    RccIrValue result;
    RccIrMem2RegStats stats;
    char error[256];
    append_store(entry, zero, address);
    append_branch(entry, condition->id);
    current = append_load(condition, i32, address);
    limit = append_const(condition, i32, 4u);
    compare_operands[0] = current;
    compare_operands[1] = limit;
    compare = rcc_ir_append(condition, RCC_IR_ICMP,
                            rcc_ir_type_integer(1u), compare_operands, 2u,
                            NULL, 0u);
    assert(compare != NULL);
    rcc_ir_set_predicate(compare, RCC_IR_ICMP_SLT);
    append_cond_branch(condition, compare->result, body->id, exit->id);
    current = append_load(body, i32, address);
    one = append_const(body, i32, 1u);
    add_operands[0] = current;
    add_operands[1] = one;
    add = rcc_ir_append(body, RCC_IR_ADD, i32, add_operands, 2u,
                        NULL, 0u);
    assert(add != NULL);
    append_store(body, add->result, address);
    append_branch(body, condition->id);
    result = append_load(exit, i32, address);
    append_return(exit, result);
    if (!rcc_ir_mem2reg(function, &stats, error, sizeof(error))) {
        fprintf(stderr, "loop mem2reg failed: %s\n", error);
        assert(0);
    }
    assert(error[0] == '\0');
    assert(stats.promoted_allocas == 1u);
    assert(stats.removed_loads == 3u);
    assert(stats.removed_stores == 2u);
    assert(stats.inserted_phis == 1u);
    assert(count_opcode(function, RCC_IR_ALLOCA) == 0u);
    assert(count_opcode(function, RCC_IR_LOAD) == 0u);
    assert(count_opcode(function, RCC_IR_STORE) == 0u);
    assert(count_opcode(function, RCC_IR_PHI) == 1u);
    assert(rcc_ir_verify_function(function, error, sizeof(error)));
    rcc_ir_module_destroy(module);
}

static void verify_escape_is_not_promoted(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "escape", i32, NULL, 0u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrValue address = append_alloca(entry, 4u);
    RccIrValue zero = append_const(entry, i32, 0u);
    RccIrInstruction* call;
    RccIrMem2RegStats stats;
    char error[256];
    append_store(entry, zero, address);
    call = rcc_ir_append(entry, RCC_IR_CALL, rcc_ir_type_void(),
                         &address, 1u, NULL, 0u);
    assert(call != NULL);
    rcc_ir_set_callee(call, "capture");
    append_return(entry, zero);
    assert(rcc_ir_mem2reg(function, &stats, error, sizeof(error)));
    assert(stats.promoted_allocas == 0u);
    assert(count_opcode(function, RCC_IR_ALLOCA) == 1u);
    assert(count_opcode(function, RCC_IR_STORE) == 1u);
    assert(rcc_ir_verify_function(function, error, sizeof(error)));
    rcc_ir_module_destroy(module);
}

int main(void)
{
    verify_diamond_promotion();
    verify_loop_promotion();
    verify_escape_is_not_promoted();
    puts("Typed SSA mem2reg tests passed");
    return 0;
}
