#include "ir.h"

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

static RccIrValue append_binary(RccIrBlock* block, RccIrOpcode opcode,
                                RccIrType type, RccIrValue left,
                                RccIrValue right)
{
    RccIrValue operands[] = {left, right};
    RccIrInstruction* instruction = rcc_ir_append(
        block, opcode, type, operands, 2u, NULL, 0u);
    assert(instruction != NULL);
    return instruction->result;
}

static void append_branch(RccIrBlock* block, RccIrBlockId target)
{
    assert(rcc_ir_append(block, RCC_IR_BRANCH, rcc_ir_type_void(), NULL, 0u,
                         &target, 1u) != NULL);
}

static void append_return(RccIrBlock* block, RccIrValue value)
{
    assert(rcc_ir_append(block, RCC_IR_RETURN, rcc_ir_type_void(), &value,
                         1u, NULL, 0u) != NULL);
}

static void expect_valid(RccIrModule* module)
{
    char error[256];
    assert(rcc_ir_verify_module(module, error, sizeof(error)));
    assert(error[0] == '\0');
    rcc_ir_module_destroy(module);
}

static void expect_invalid(RccIrModule* module, const char* expected)
{
    char error[256];
    assert(!rcc_ir_verify_module(module, error, sizeof(error)));
    assert(strstr(error, expected) != NULL);
    rcc_ir_module_destroy(module);
}

static void verify_diamond_phi(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType i1 = rcc_ir_type_integer(1u);
    RccIrType parameters[] = {i32};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "diamond", i32, parameters, 1u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrBlock* positive = rcc_ir_block_add(function, "positive");
    RccIrBlock* negative = rcc_ir_block_add(function, "negative");
    RccIrBlock* merge = rcc_ir_block_add(function, "merge");
    RccIrValue zero = append_const(entry, i32, 0u);
    RccIrValue one = append_const(entry, i32, 1u);
    RccIrValue compare_operands[] = {function->parameters[0], zero};
    RccIrInstruction* compare = rcc_ir_append(
        entry, RCC_IR_ICMP, i1, compare_operands, 2u, NULL, 0u);
    RccIrValue condition[] = {compare->result};
    RccIrBlockId choices[] = {positive->id, negative->id};
    RccIrValue positive_value;
    RccIrValue negative_value;
    RccIrValue incoming[2];
    RccIrBlockId predecessors[2];
    RccIrInstruction* phi;
    assert(compare != NULL);
    rcc_ir_set_predicate(compare, RCC_IR_ICMP_SGT);
    assert(rcc_ir_append(entry, RCC_IR_COND_BRANCH, rcc_ir_type_void(),
                         condition, 1u, choices, 2u) != NULL);

    positive_value = append_binary(positive, RCC_IR_ADD, i32,
                                   function->parameters[0], one);
    append_branch(positive, merge->id);
    negative_value = append_binary(negative, RCC_IR_SUB, i32,
                                   function->parameters[0], one);
    append_branch(negative, merge->id);

    incoming[0] = positive_value;
    incoming[1] = negative_value;
    predecessors[0] = positive->id;
    predecessors[1] = negative->id;
    phi = rcc_ir_append(merge, RCC_IR_PHI, i32, incoming, 2u,
                        predecessors, 2u);
    assert(phi != NULL);
    append_return(merge, phi->result);
    expect_valid(module);
}

static void verify_memory_ir(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType pointer = rcc_ir_type_pointer(0u);
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "memory", i32, NULL, 0u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrInstruction* allocation = rcc_ir_append(
        entry, RCC_IR_ALLOCA, pointer, NULL, 0u, NULL, 0u);
    RccIrValue value = append_const(entry, i32, 7u);
    RccIrValue store_operands[2];
    RccIrValue load_operand[1];
    RccIrInstruction* load;
    assert(allocation != NULL);
    rcc_ir_set_immediate(allocation, 4u);
    store_operands[0] = value;
    store_operands[1] = allocation->result;
    assert(rcc_ir_append(entry, RCC_IR_STORE, rcc_ir_type_void(),
                         store_operands, 2u, NULL, 0u) != NULL);
    load_operand[0] = allocation->result;
    load = rcc_ir_append(entry, RCC_IR_LOAD, i32, load_operand, 1u,
                         NULL, 0u);
    assert(load != NULL);
    append_return(entry, load->result);
    expect_valid(module);
}

static void verify_symbol_address_ir(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType pointer = rcc_ir_type_pointer(0u);
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "global_load", i32, NULL, 0u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrInstruction* address = rcc_ir_append(
        entry, RCC_IR_SYMBOL_ADDRESS, pointer, NULL, 0u, NULL, 0u);
    RccIrInstruction* load;
    assert(address != NULL);
    rcc_ir_set_callee(address, "global_value");
    load = rcc_ir_append(entry, RCC_IR_LOAD, i32, &address->result, 1u,
                         NULL, 0u);
    assert(load != NULL);
    append_return(entry, load->result);
    expect_valid(module);
}

static void reject_symbol_address_without_symbol(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "bad_global", i32, NULL, 0u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrInstruction* address = rcc_ir_append(
        entry, RCC_IR_SYMBOL_ADDRESS, rcc_ir_type_pointer(0u),
        NULL, 0u, NULL, 0u);
    RccIrInstruction* load;
    assert(address != NULL);
    load = rcc_ir_append(entry, RCC_IR_LOAD, i32, &address->result, 1u,
                         NULL, 0u);
    assert(load != NULL);
    append_return(entry, load->result);
    expect_invalid(module, "symbol_address requires");
}

static void verify_loop_phi(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType i1 = rcc_ir_type_integer(1u);
    RccIrType parameters[] = {i32};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "loop", i32, parameters, 1u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrBlock* header = rcc_ir_block_add(function, "header");
    RccIrBlock* body = rcc_ir_block_add(function, "body");
    RccIrBlock* exit = rcc_ir_block_add(function, "exit");
    RccIrValue zero = append_const(entry, i32, 0u);
    RccIrValue one = append_const(entry, i32, 1u);
    RccIrValue incoming[] = {zero, RCC_IR_VALUE_NONE};
    RccIrBlockId predecessors[] = {entry->id, body->id};
    RccIrInstruction* phi;
    RccIrValue compare_operands[2];
    RccIrInstruction* compare;
    RccIrValue condition[1];
    RccIrBlockId choices[] = {body->id, exit->id};
    RccIrValue next;
    append_branch(entry, header->id);
    phi = rcc_ir_append(header, RCC_IR_PHI, i32, incoming, 2u,
                        predecessors, 2u);
    assert(phi != NULL);
    compare_operands[0] = phi->result;
    compare_operands[1] = function->parameters[0];
    compare = rcc_ir_append(header, RCC_IR_ICMP, i1, compare_operands, 2u,
                            NULL, 0u);
    assert(compare != NULL);
    rcc_ir_set_predicate(compare, RCC_IR_ICMP_SLT);
    condition[0] = compare->result;
    assert(rcc_ir_append(header, RCC_IR_COND_BRANCH, rcc_ir_type_void(),
                         condition, 1u, choices, 2u) != NULL);
    next = append_binary(body, RCC_IR_ADD, i32, phi->result, one);
    append_branch(body, header->id);
    phi->operands[1] = next;
    append_return(exit, phi->result);
    expect_valid(module);
}

static void reject_type_mismatch(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType parameter_types[] = {i32, rcc_ir_type_integer(64u)};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "bad_types", i32, parameter_types, 2u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrValue value = append_binary(entry, RCC_IR_ADD, i32,
                                     function->parameters[0],
                                     function->parameters[1]);
    append_return(entry, value);
    expect_invalid(module, "mismatched type");
}

static void reject_missing_terminator(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "unterminated", i32, NULL, 0u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    (void)append_const(entry, i32, 1u);
    expect_invalid(module, "no terminator");
}

static void reject_use_before_definition(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "forward_use", i32, NULL, 0u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrValue future[] = {1u, 1u};
    RccIrInstruction* add = rcc_ir_append(
        entry, RCC_IR_ADD, i32, future, 2u, NULL, 0u);
    (void)append_const(entry, i32, 2u);
    append_return(entry, add->result);
    expect_invalid(module, "used before definition");
}

static void reject_non_dominating_use(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType i1 = rcc_ir_type_integer(1u);
    RccIrType parameters[] = {i1};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "bad_dominance", i32, parameters, 1u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrBlock* left = rcc_ir_block_add(function, "left");
    RccIrBlock* right = rcc_ir_block_add(function, "right");
    RccIrBlock* merge = rcc_ir_block_add(function, "merge");
    RccIrBlockId choices[] = {left->id, right->id};
    RccIrValue condition[] = {function->parameters[0]};
    RccIrValue only_left;
    assert(rcc_ir_append(entry, RCC_IR_COND_BRANCH, rcc_ir_type_void(),
                         condition, 1u, choices, 2u) != NULL);
    only_left = append_const(left, i32, 9u);
    append_branch(left, merge->id);
    append_branch(right, merge->id);
    append_return(merge, only_left);
    expect_invalid(module, "does not dominate");
}

static void reject_bad_phi_predecessors(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType i1 = rcc_ir_type_integer(1u);
    RccIrType parameters[] = {i1};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "bad_phi", i32, parameters, 1u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrBlock* left = rcc_ir_block_add(function, "left");
    RccIrBlock* right = rcc_ir_block_add(function, "right");
    RccIrBlock* merge = rcc_ir_block_add(function, "merge");
    RccIrBlockId choices[] = {left->id, right->id};
    RccIrValue condition[] = {function->parameters[0]};
    RccIrValue left_value;
    RccIrValue right_value;
    RccIrValue incoming[2];
    RccIrBlockId repeated[2];
    RccIrInstruction* phi;
    assert(rcc_ir_append(entry, RCC_IR_COND_BRANCH, rcc_ir_type_void(),
                         condition, 1u, choices, 2u) != NULL);
    left_value = append_const(left, i32, 1u);
    append_branch(left, merge->id);
    right_value = append_const(right, i32, 2u);
    append_branch(right, merge->id);
    incoming[0] = left_value;
    incoming[1] = right_value;
    repeated[0] = left->id;
    repeated[1] = left->id;
    phi = rcc_ir_append(merge, RCC_IR_PHI, i32, incoming, 2u,
                        repeated, 2u);
    append_return(merge, phi->result);
    expect_invalid(module, "repeats an incoming block");
}

static void reject_unreachable_block(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "unreachable_block", i32, NULL, 0u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrBlock* dead = rcc_ir_block_add(function, "dead");
    RccIrValue one = append_const(entry, i32, 1u);
    RccIrValue two = append_const(dead, i32, 2u);
    append_return(entry, one);
    append_return(dead, two);
    expect_invalid(module, "is unreachable");
}

int main(void)
{
    verify_diamond_phi();
    verify_memory_ir();
    verify_symbol_address_ir();
    verify_loop_phi();
    reject_type_mismatch();
    reject_missing_terminator();
    reject_use_before_definition();
    reject_non_dominating_use();
    reject_bad_phi_predecessors();
    reject_unreachable_block();
    reject_symbol_address_without_symbol();
    puts("Typed SSA IR and CFG verifier tests passed");
    return 0;
}
