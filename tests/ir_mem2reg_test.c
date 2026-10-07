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

static RccIrValue append_select(RccIrBlock* block, RccIrType type,
                                RccIrValue condition, RccIrValue when_true,
                                RccIrValue when_false)
{
    RccIrValue operands[] = {condition, when_true, when_false};
    RccIrInstruction* instruction = rcc_ir_append(
        block, RCC_IR_SELECT, type, operands, 3u, NULL, 0u);
    assert(instruction != NULL);
    return instruction->result;
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

static RccIrModule* build_optimization_pipeline_fixture(
    RccIrFunction** function_out)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType parameters[] = {i32, i32};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "optimization_pipeline", i32, parameters, 2u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrValue address = append_alloca(entry, 4u);
    RccIrValue loaded;
    RccIrValue operands[2];
    RccIrInstruction* first;
    RccIrInstruction* duplicate;
    RccIrInstruction* product;
    append_store(entry, function->parameters[0], address);
    loaded = append_load(entry, i32, address);
    operands[0] = loaded;
    operands[1] = function->parameters[1];
    first = rcc_ir_append(entry, RCC_IR_ADD, i32,
                          operands, 2u, NULL, 0u);
    operands[0] = function->parameters[1];
    operands[1] = loaded;
    duplicate = rcc_ir_append(entry, RCC_IR_ADD, i32,
                              operands, 2u, NULL, 0u);
    assert(first != NULL && duplicate != NULL);
    operands[0] = first->result;
    operands[1] = duplicate->result;
    product = rcc_ir_append(entry, RCC_IR_MUL, i32,
                            operands, 2u, NULL, 0u);
    assert(product != NULL);
    append_return(entry, product->result);
    *function_out = function;
    return module;
}

static void verify_optimization_level_pipeline(void)
{
    RccIrModule* module;
    RccIrFunction* function;
    RccIrOptimizationStats stats;
    char error[256];

    module = build_optimization_pipeline_fixture(&function);
    assert(rcc_ir_optimize_function(function, 0u, &stats,
                                    error, sizeof(error)));
    assert(stats.level == 0u && stats.simplify_rounds == 0u);
    assert(count_opcode(function, RCC_IR_ALLOCA) == 1u);
    assert(count_opcode(function, RCC_IR_LOAD) == 1u);
    assert(count_opcode(function, RCC_IR_STORE) == 1u);
    assert(count_opcode(function, RCC_IR_ADD) == 2u);
    rcc_ir_module_destroy(module);

    module = build_optimization_pipeline_fixture(&function);
    assert(rcc_ir_optimize_function(function, 1u, &stats,
                                    error, sizeof(error)));
    assert(stats.level == 1u && stats.simplify_rounds == 1u);
    assert(stats.mem2reg.promoted_allocas == 1u);
    assert(stats.simplify.commoned_instructions == 0u);
    assert(count_opcode(function, RCC_IR_ALLOCA) == 0u);
    assert(count_opcode(function, RCC_IR_LOAD) == 0u);
    assert(count_opcode(function, RCC_IR_STORE) == 0u);
    assert(count_opcode(function, RCC_IR_ADD) == 2u);
    rcc_ir_module_destroy(module);

    module = build_optimization_pipeline_fixture(&function);
    assert(rcc_ir_optimize_function(function, 2u, &stats,
                                    error, sizeof(error)));
    assert(stats.level == 2u && stats.simplify_rounds == 1u);
    assert(stats.mem2reg.promoted_allocas == 1u);
    assert(stats.simplify.commoned_instructions == 1u);
    assert(count_opcode(function, RCC_IR_ADD) == 1u);
    rcc_ir_module_destroy(module);

    module = build_optimization_pipeline_fixture(&function);
    assert(rcc_ir_optimize_function(function, 3u, &stats,
                                    error, sizeof(error)));
    assert(stats.level == 3u && stats.simplify_rounds == 2u);
    assert(stats.mem2reg.promoted_allocas == 1u);
    assert(stats.simplify.commoned_instructions == 1u);
    assert(count_opcode(function, RCC_IR_ADD) == 1u);
    assert(!rcc_ir_optimize_function(function, 4u, &stats,
                                     error, sizeof(error)));
    assert(strstr(error, "invalid SSA optimization level 4") != NULL);
    rcc_ir_module_destroy(module);
}

static void verify_debug_local_promotion_policy(void)
{
    static const int source_declaration_marker = 0;
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType parameters[] = {i32, i32};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "debug_local_promotion", i32, parameters, 2u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrInstruction* source_allocation = rcc_ir_append(
        entry, RCC_IR_ALLOCA, rcc_ir_type_pointer(0u), NULL, 0u,
        NULL, 0u);
    RccIrValue temporary_address = append_alloca(entry, 4u);
    RccIrValue source_value;
    RccIrValue temporary_value;
    RccIrValue result;
    RccIrOptimizationStats stats;
    RccIrInstruction* instruction;
    size_t retained_source_allocas = 0u;
    char error[256];

    assert(source_allocation != NULL);
    rcc_ir_set_immediate(source_allocation, 4u);
    source_allocation->source_declaration = &source_declaration_marker;
    append_store(entry, function->parameters[0], source_allocation->result);
    append_store(entry, function->parameters[1], temporary_address);
    source_value = append_load(entry, i32, source_allocation->result);
    temporary_value = append_load(entry, i32, temporary_address);
    result = append_binary(entry, RCC_IR_ADD, i32,
                           source_value, temporary_value);
    append_return(entry, result);

    assert(rcc_ir_optimize_function_preserving_source_declarations(
        function, 1u, &stats, error, sizeof(error)));
    assert(error[0] == '\0');
    assert(stats.mem2reg.promoted_allocas == 1u);
    assert(count_opcode(function, RCC_IR_ALLOCA) == 1u);
    assert(count_opcode(function, RCC_IR_LOAD) == 1u);
    assert(count_opcode(function, RCC_IR_STORE) == 1u);
    for (instruction = entry->first; instruction;
         instruction = instruction->next) {
        if (instruction->opcode != RCC_IR_ALLOCA) continue;
        assert(instruction->source_declaration ==
               &source_declaration_marker);
        ++retained_source_allocas;
    }
    assert(retained_source_allocas == 1u);
    rcc_ir_module_destroy(module);
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

static void verify_integer_simplification(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "simplify", i32, NULL, 0u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrValue two = append_const(entry, i32, 2u);
    RccIrValue three = append_const(entry, i32, 3u);
    RccIrValue add_operands[] = {two, three};
    RccIrInstruction* add = rcc_ir_append(
        entry, RCC_IR_ADD, i32, add_operands, 2u, NULL, 0u);
    RccIrValue four = append_const(entry, i32, 4u);
    RccIrValue multiply_operands[2];
    RccIrInstruction* multiply;
    RccIrSimplifyStats stats;
    char error[256];
    assert(add != NULL);
    multiply_operands[0] = add->result;
    multiply_operands[1] = four;
    multiply = rcc_ir_append(entry, RCC_IR_MUL, i32,
                             multiply_operands, 2u, NULL, 0u);
    assert(multiply != NULL);
    append_return(entry, multiply->result);
    assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
    assert(error[0] == '\0');
    assert(stats.folded_instructions == 2u);
    assert(stats.removed_instructions == 4u);
    assert(count_opcode(function, RCC_IR_CONST_INT) == 1u);
    assert(count_opcode(function, RCC_IR_ADD) == 0u);
    assert(count_opcode(function, RCC_IR_MUL) == 0u);
    assert(rcc_ir_verify_function(function, error, sizeof(error)));
    rcc_ir_module_destroy(module);
}

static void verify_self_compare_simplification(void)
{
    static const RccIrIntPredicate predicates[] = {
        RCC_IR_ICMP_EQ, RCC_IR_ICMP_NE,
        RCC_IR_ICMP_ULT, RCC_IR_ICMP_ULE,
        RCC_IR_ICMP_UGT, RCC_IR_ICMP_UGE,
        RCC_IR_ICMP_SLT, RCC_IR_ICMP_SLE,
        RCC_IR_ICMP_SGT, RCC_IR_ICMP_SGE,
    };
    static const uint64_t expected[] = {
        1u, 0u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u,
    };
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType parameters[] = {i32};
    size_t index;

    for (index = 0u; index < sizeof(predicates) / sizeof(predicates[0]);
         ++index) {
        RccIrModule* module = rcc_ir_module_create();
        RccIrFunction* function = rcc_ir_function_add(
            module, "self_compare", rcc_ir_type_integer(1u),
            parameters, 1u);
        RccIrBlock* entry = rcc_ir_block_add(function, "entry");
        RccIrValue operands[] = {
            function->parameters[0], function->parameters[0],
        };
        RccIrInstruction* compare;
        RccIrSimplifyStats stats;
        char error[256];

        assert(function != NULL && entry != NULL);
        compare = rcc_ir_append(entry, RCC_IR_ICMP,
                                rcc_ir_type_integer(1u),
                                operands, 2u, NULL, 0u);
        assert(compare != NULL);
        rcc_ir_set_predicate(compare, predicates[index]);
        append_return(entry, compare->result);
        assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
        assert(error[0] == '\0');
        assert(stats.folded_instructions >= 1u);
        assert(count_opcode(function, RCC_IR_ICMP) == 0u);
        assert(count_opcode(function, RCC_IR_CONST_INT) == 1u);
        assert(entry->first != NULL &&
               entry->first->opcode == RCC_IR_CONST_INT);
        assert(entry->first->immediate == expected[index]);
        assert(rcc_ir_verify_function(function, error, sizeof(error)));
        rcc_ir_module_destroy(module);
    }
}

static void verify_self_binary_simplification(void)
{
    static const RccIrOpcode opcodes[] = {
        RCC_IR_SUB, RCC_IR_AND, RCC_IR_OR, RCC_IR_XOR,
    };
    static const uint16_t widths[] = {8u, 16u, 32u, 64u};
    size_t opcode_index;
    size_t width_index;

    for (opcode_index = 0u;
         opcode_index < sizeof(opcodes) / sizeof(opcodes[0]);
         ++opcode_index) {
        for (width_index = 0u;
             width_index < sizeof(widths) / sizeof(widths[0]);
             ++width_index) {
            RccIrType integer = rcc_ir_type_integer(widths[width_index]);
            RccIrType parameters[] = {integer};
            RccIrModule* module = rcc_ir_module_create();
            RccIrFunction* function = rcc_ir_function_add(
                module, "self_binary", integer, parameters, 1u);
            RccIrBlock* entry = rcc_ir_block_add(function, "entry");
            RccIrValue operands[] = {
                function->parameters[0], function->parameters[0],
            };
            RccIrInstruction* operation;
            RccIrSimplifyStats stats;
            char error[256];

            assert(function != NULL && entry != NULL);
            operation = rcc_ir_append(entry, opcodes[opcode_index],
                                      integer, operands, 2u, NULL, 0u);
            assert(operation != NULL);
            append_return(entry, operation->result);
            assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
            assert(error[0] == '\0');
            assert(stats.folded_instructions >= 1u);
            assert(count_opcode(function, opcodes[opcode_index]) == 0u);
            if (opcodes[opcode_index] == RCC_IR_SUB ||
                opcodes[opcode_index] == RCC_IR_XOR) {
                assert(count_opcode(function, RCC_IR_CONST_INT) == 1u);
                assert(entry->first != NULL &&
                       entry->first->opcode == RCC_IR_CONST_INT);
                assert(entry->first->immediate == 0u);
            }
            assert(rcc_ir_verify_function(function, error, sizeof(error)));
            rcc_ir_module_destroy(module);
        }
    }
}

static void verify_modulo_one_simplification(void)
{
    static const RccIrOpcode opcodes[] = {RCC_IR_UREM, RCC_IR_SREM};
    static const uint16_t widths[] = {8u, 16u, 32u, 64u};
    size_t opcode_index;
    size_t width_index;

    for (opcode_index = 0u;
         opcode_index < sizeof(opcodes) / sizeof(opcodes[0]);
         ++opcode_index) {
        for (width_index = 0u;
             width_index < sizeof(widths) / sizeof(widths[0]);
             ++width_index) {
            RccIrType integer = rcc_ir_type_integer(widths[width_index]);
            RccIrType parameters[] = {integer};
            RccIrModule* module = rcc_ir_module_create();
            RccIrFunction* function = rcc_ir_function_add(
                module, "modulo_one", integer, parameters, 1u);
            RccIrBlock* entry = rcc_ir_block_add(function, "entry");
            RccIrValue one = append_const(entry, integer, 1u);
            RccIrValue operands[] = {function->parameters[0], one};
            RccIrInstruction* operation;
            RccIrSimplifyStats stats;
            char error[256];

            assert(function != NULL && entry != NULL);
            operation = rcc_ir_append(entry, opcodes[opcode_index],
                                      integer, operands, 2u, NULL, 0u);
            assert(operation != NULL);
            append_return(entry, operation->result);
            assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
            assert(error[0] == '\0');
            assert(stats.folded_instructions >= 1u);
            assert(count_opcode(function, opcodes[opcode_index]) == 0u);
            assert(count_opcode(function, RCC_IR_CONST_INT) == 1u);
            assert(entry->first != NULL &&
                   entry->first->opcode == RCC_IR_CONST_INT);
            assert(entry->first->immediate == 0u);
            assert(rcc_ir_verify_function(function, error, sizeof(error)));
            rcc_ir_module_destroy(module);
        }
    }
}

static void verify_constant_branch_pruning(void)
{
    RccIrType i1 = rcc_ir_type_integer(1u);
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "constant_branch", i32, NULL, 0u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrBlock* dead = rcc_ir_block_add(function, "dead");
    RccIrBlock* live = rcc_ir_block_add(function, "live");
    RccIrValue condition = append_const(entry, i1, 0u);
    RccIrValue dead_value = append_const(dead, i32, 99u);
    RccIrValue live_value = append_const(live, i32, 7u);
    RccIrSimplifyStats stats;
    char error[256];

    append_cond_branch(entry, condition, dead->id, live->id);
    append_return(dead, dead_value);
    append_return(live, live_value);
    assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
    assert(error[0] == '\0');
    assert(stats.folded_instructions == 1u);
    assert(stats.removed_blocks == 1u);
    assert(function->block_count == 2u);
    assert(function->first_block->last->opcode == RCC_IR_BRANCH);
    assert(function->first_block->last->targets[0] == 1u);
    assert(count_opcode(function, RCC_IR_COND_BRANCH) == 0u);
    assert(count_opcode(function, RCC_IR_RETURN) == 1u);
    assert(rcc_ir_verify_function(function, error, sizeof(error)));
    rcc_ir_module_destroy(module);
}

static void verify_equal_branch_target_simplification(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType parameters[] = {rcc_ir_type_integer(1u)};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "equal_branch_targets", i32, parameters, 1u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrBlock* target = rcc_ir_block_add(function, "target");
    RccIrValue value;
    RccIrSimplifyStats stats;
    char error[256];

    assert(function != NULL && entry != NULL && target != NULL);
    append_cond_branch(entry, function->parameters[0], target->id,
                       target->id);
    value = append_const(target, i32, 23u);
    append_return(target, value);
    assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
    assert(error[0] == '\0');
    assert(stats.folded_instructions >= 1u);
    assert(count_opcode(function, RCC_IR_COND_BRANCH) == 0u);
    assert(entry->last != NULL && entry->last->opcode == RCC_IR_BRANCH);
    assert(entry->last->target_count == 1u);
    assert(entry->last->targets[0] == target->id);
    assert(function->block_count == 2u);
    assert(rcc_ir_verify_function(function, error, sizeof(error)));
    rcc_ir_module_destroy(module);
}

static void verify_constant_phi_and_select_folding(void)
{
    RccIrType i1 = rcc_ir_type_integer(1u);
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType parameters[] = {i1};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "constant_phi", i32, parameters, 1u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrBlock* left = rcc_ir_block_add(function, "left");
    RccIrBlock* right = rcc_ir_block_add(function, "right");
    RccIrBlock* merge = rcc_ir_block_add(function, "merge");
    RccIrValue left_value;
    RccIrValue right_value;
    RccIrValue incoming[2];
    RccIrBlockId targets[] = {left->id, right->id};
    RccIrInstruction* phi;
    RccIrSimplifyStats stats;
    char error[256];
    assert(function != NULL && entry != NULL && left != NULL &&
           right != NULL && merge != NULL);
    append_cond_branch(entry, function->parameters[0], left->id, right->id);
    left_value = append_const(left, i32, 7u);
    append_branch(left, merge->id);
    right_value = append_const(right, i32, 7u);
    append_branch(right, merge->id);
    incoming[0] = left_value;
    incoming[1] = right_value;
    phi = rcc_ir_append(merge, RCC_IR_PHI, i32, incoming, 2u,
                        targets, 2u);
    assert(phi != NULL);
    append_return(merge, phi->result);
    assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
    assert(stats.folded_instructions >= 1u);
    assert(count_opcode(function, RCC_IR_PHI) == 0u);
    assert(rcc_ir_verify_function(function, error, sizeof(error)));
    rcc_ir_module_destroy(module);

    module = rcc_ir_module_create();
    function = rcc_ir_function_add(module, "constant_select", i32,
                                   NULL, 0u);
    entry = rcc_ir_block_add(function, "entry");
    assert(function != NULL && entry != NULL);
    {
        RccIrValue condition = append_const(entry, i1, 1u);
        RccIrValue when_true = append_const(entry, i32, 9u);
        RccIrValue when_false = append_const(entry, i32, 11u);
        RccIrValue selected = append_select(
            entry, i32, condition, when_true, when_false);
        RccIrSimplifyStats select_stats;
        append_return(entry, selected);
        assert(rcc_ir_simplify(function, &select_stats,
                               error, sizeof(error)));
        assert(select_stats.folded_instructions >= 1u);
        assert(count_opcode(function, RCC_IR_SELECT) == 0u);
        assert(rcc_ir_verify_function(function, error, sizeof(error)));
    }
    rcc_ir_module_destroy(module);
}

static void verify_trivial_phi_simplification(void)
{
    RccIrType i1 = rcc_ir_type_integer(1u);
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType parameters[] = {i32, i1};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "trivial_phi", i32, parameters, 2u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrBlock* left = rcc_ir_block_add(function, "left");
    RccIrBlock* right = rcc_ir_block_add(function, "right");
    RccIrBlock* merge = rcc_ir_block_add(function, "merge");
    RccIrValue incoming[2];
    RccIrBlockId targets[2];
    RccIrInstruction* phi;
    RccIrSimplifyStats stats;
    char error[256];

    assert(function != NULL && entry != NULL && left != NULL &&
           right != NULL && merge != NULL);
    append_cond_branch(entry, function->parameters[1], left->id, right->id);
    append_branch(left, merge->id);
    append_branch(right, merge->id);
    incoming[0] = function->parameters[0];
    incoming[1] = function->parameters[0];
    targets[0] = left->id;
    targets[1] = right->id;
    phi = rcc_ir_append(merge, RCC_IR_PHI, i32, incoming, 2u,
                        targets, 2u);
    assert(phi != NULL);
    append_return(merge, phi->result);
    assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
    assert(error[0] == '\0');
    assert(stats.folded_instructions >= 1u);
    assert(count_opcode(function, RCC_IR_PHI) == 0u);
    assert(merge->first != NULL && merge->first->opcode == RCC_IR_RETURN);
    assert(merge->first->operands[0] == function->parameters[0]);
    assert(rcc_ir_verify_function(function, error, sizeof(error)));
    rcc_ir_module_destroy(module);
}

static void verify_trivial_select_simplification(void)
{
    RccIrType i1 = rcc_ir_type_integer(1u);
    RccIrType pointer = rcc_ir_type_pointer(0u);
    RccIrType parameters[] = {i1, pointer};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "trivial_select", pointer, parameters, 2u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrValue selected;
    RccIrSimplifyStats stats;
    char error[256];

    assert(function != NULL && entry != NULL);
    selected = append_select(entry, pointer, function->parameters[0],
                             function->parameters[1], function->parameters[1]);
    append_return(entry, selected);
    assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
    assert(error[0] == '\0');
    assert(stats.folded_instructions >= 1u);
    assert(count_opcode(function, RCC_IR_SELECT) == 0u);
    assert(entry->first != NULL && entry->first->opcode == RCC_IR_RETURN);
    assert(entry->first->operands[0] == function->parameters[1]);
    assert(rcc_ir_verify_function(function, error, sizeof(error)));
    rcc_ir_module_destroy(module);
}

static void verify_constant_condition_select_simplification(void)
{
    RccIrType i1 = rcc_ir_type_integer(1u);
    RccIrType pointer = rcc_ir_type_pointer(0u);
    RccIrType parameters[] = {pointer, pointer};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "constant_condition_select", pointer, parameters, 2u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrValue condition;
    RccIrValue selected;
    RccIrSimplifyStats stats;
    char error[256];

    assert(function != NULL && entry != NULL);
    condition = append_const(entry, i1, 1u);
    selected = append_select(entry, pointer, condition,
                             function->parameters[0], function->parameters[1]);
    append_return(entry, selected);
    assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
    assert(error[0] == '\0');
    assert(stats.folded_instructions >= 1u);
    assert(count_opcode(function, RCC_IR_SELECT) == 0u);
    assert(entry->last != NULL && entry->last->opcode == RCC_IR_RETURN);
    assert(entry->last->operands[0] == function->parameters[0]);
    assert(rcc_ir_verify_function(function, error, sizeof(error)));
    rcc_ir_module_destroy(module);
}

static void verify_undefined_folds_are_preserved(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "preserve_undefined", i32, NULL, 0u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrValue minimum = append_const(entry, i32, UINT32_C(0x80000000));
    RccIrValue negative_one = append_const(entry, i32, UINT32_MAX);
    RccIrValue operands[] = {minimum, negative_one};
    RccIrInstruction* divide = rcc_ir_append(
        entry, RCC_IR_SDIV, i32, operands, 2u, NULL, 0u);
    RccIrSimplifyStats stats;
    char error[256];
    assert(divide != NULL);
    append_return(entry, divide->result);
    assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
    assert(stats.folded_instructions == 0u);
    assert(stats.removed_instructions == 0u);
    assert(count_opcode(function, RCC_IR_SDIV) == 1u);
    assert(rcc_ir_verify_function(function, error, sizeof(error)));
    rcc_ir_module_destroy(module);
}

static void verify_integer_identities(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType parameters[] = {i32};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "integer_identities", i32, parameters, 1u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrValue zero = append_const(entry, i32, 0u);
    RccIrValue one = append_const(entry, i32, 1u);
    RccIrValue all_bits = append_const(entry, i32, UINT32_MAX);
    RccIrValue operands[2];
    RccIrInstruction* value;
    RccIrSimplifyStats stats;
    char error[256];

    operands[0] = function->parameters[0];
    operands[1] = zero;
    value = rcc_ir_append(entry, RCC_IR_ADD, i32, operands, 2u, NULL, 0u);
    assert(value != NULL);
    operands[0] = one;
    operands[1] = value->result;
    value = rcc_ir_append(entry, RCC_IR_MUL, i32, operands, 2u, NULL, 0u);
    assert(value != NULL);
    operands[0] = value->result;
    operands[1] = all_bits;
    value = rcc_ir_append(entry, RCC_IR_AND, i32, operands, 2u, NULL, 0u);
    assert(value != NULL);
    operands[0] = zero;
    operands[1] = value->result;
    value = rcc_ir_append(entry, RCC_IR_XOR, i32, operands, 2u, NULL, 0u);
    assert(value != NULL);
    operands[0] = value->result;
    operands[1] = zero;
    value = rcc_ir_append(entry, RCC_IR_SHL, i32, operands, 2u, NULL, 0u);
    assert(value != NULL);
    operands[0] = value->result;
    operands[1] = one;
    value = rcc_ir_append(entry, RCC_IR_SDIV, i32, operands, 2u, NULL, 0u);
    assert(value != NULL);
    append_return(entry, value->result);
    assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
    assert(error[0] == '\0');
    assert(stats.folded_instructions >= 6u);
    assert(count_opcode(function, RCC_IR_ADD) == 0u);
    assert(count_opcode(function, RCC_IR_MUL) == 0u);
    assert(count_opcode(function, RCC_IR_AND) == 0u);
    assert(count_opcode(function, RCC_IR_XOR) == 0u);
    assert(count_opcode(function, RCC_IR_SHL) == 0u);
    assert(count_opcode(function, RCC_IR_SDIV) == 0u);
    assert(rcc_ir_verify_function(function, error, sizeof(error)));
    rcc_ir_module_destroy(module);
}

static void verify_block_local_cse(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType parameters[] = {i32, i32};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "block_cse", i32, parameters, 2u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrValue first_operands[] = {
        function->parameters[0], function->parameters[1],
    };
    RccIrValue reversed_operands[] = {
        function->parameters[1], function->parameters[0],
    };
    RccIrInstruction* first = rcc_ir_append(
        entry, RCC_IR_ADD, i32, first_operands, 2u, NULL, 0u);
    RccIrInstruction* duplicate = rcc_ir_append(
        entry, RCC_IR_ADD, i32, reversed_operands, 2u, NULL, 0u);
    RccIrValue product_operands[2];
    RccIrInstruction* product;
    RccIrSimplifyStats stats;
    char error[256];
    assert(first != NULL && duplicate != NULL);
    product_operands[0] = first->result;
    product_operands[1] = duplicate->result;
    product = rcc_ir_append(entry, RCC_IR_MUL, i32,
                            product_operands, 2u, NULL, 0u);
    assert(product != NULL);
    append_return(entry, product->result);
    assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
    assert(error[0] == '\0');
    assert(stats.folded_instructions == 0u);
    assert(stats.commoned_instructions == 1u);
    assert(stats.removed_instructions == 1u);
    assert(count_opcode(function, RCC_IR_ADD) == 1u);
    assert(count_opcode(function, RCC_IR_MUL) == 1u);
    assert(rcc_ir_verify_function(function, error, sizeof(error)));
    rcc_ir_module_destroy(module);
}

static void verify_store_to_load_forwarding_after_write(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "store_to_load_write_barrier", i32, NULL, 0u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrValue address = append_alloca(entry, 4u);
    RccIrValue replacement = append_const(entry, i32, 23u);
    RccIrValue first;
    RccIrValue second;
    RccIrValue operands[2];
    RccIrInstruction* sum;
    RccIrSimplifyStats stats;
    char error[256];
    first = append_load(entry, i32, address);
    append_store(entry, replacement, address);
    second = append_load(entry, i32, address);
    operands[0] = first;
    operands[1] = second;
    sum = rcc_ir_append(entry, RCC_IR_ADD, i32,
                        operands, 2u, NULL, 0u);
    assert(sum != NULL);
    append_return(entry, sum->result);
    assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
    assert(stats.commoned_instructions == 1u);
    assert(count_opcode(function, RCC_IR_LOAD) == 1u);
    assert(count_opcode(function, RCC_IR_STORE) == 1u);
    assert(sum->operands[0] == first);
    assert(sum->operands[1] == replacement);
    assert(rcc_ir_verify_function(function, error, sizeof(error)));
    rcc_ir_module_destroy(module);
}

static void verify_block_local_load_cse(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType pointer = rcc_ir_type_pointer(0u);
    RccIrType one_pointer[] = {pointer};
    RccIrModule* module;
    RccIrFunction* function;
    RccIrBlock* entry;
    RccIrValue first;
    RccIrValue second;
    RccIrValue sum_operands[2];
    RccIrInstruction* sum;
    RccIrSimplifyStats stats;
    char error[256];

    {
        RccIrValue address;
        RccIrValue stored;
        RccIrValue loaded;
        module = rcc_ir_module_create();
        function = rcc_ir_function_add(
            module, "store_to_load_forwarding", i32, NULL, 0u);
        entry = rcc_ir_block_add(function, "entry");
        address = append_alloca(entry, 4u);
        stored = append_const(entry, i32, 37u);
        append_store(entry, stored, address);
        loaded = append_load(entry, i32, address);
        append_return(entry, loaded);
        assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
        assert(stats.commoned_instructions == 1u);
        assert(count_opcode(function, RCC_IR_LOAD) == 0u);
        assert(function->first_block->last->operands[0] == stored);
        assert(rcc_ir_verify_function(function, error, sizeof(error)));
        rcc_ir_module_destroy(module);
    }

    {
        RccIrValue first_address;
        RccIrValue second_address;
        RccIrValue first_value;
        RccIrValue second_value;
        RccIrValue loaded;
        module = rcc_ir_module_create();
        function = rcc_ir_function_add(
            module, "store_forwarding_disjoint_allocas", i32, NULL, 0u);
        entry = rcc_ir_block_add(function, "entry");
        first_address = append_alloca(entry, 4u);
        second_address = append_alloca(entry, 4u);
        first_value = append_const(entry, i32, 13u);
        second_value = append_const(entry, i32, 31u);
        append_store(entry, first_value, first_address);
        append_store(entry, second_value, second_address);
        loaded = append_load(entry, i32, first_address);
        append_return(entry, loaded);
        assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
        assert(stats.commoned_instructions == 1u);
        assert(count_opcode(function, RCC_IR_LOAD) == 0u);
        assert(function->first_block->last->operands[0] == first_value);
        assert(rcc_ir_verify_function(function, error, sizeof(error)));
        rcc_ir_module_destroy(module);
    }

    {
        module = rcc_ir_module_create();
        function = rcc_ir_function_add(
            module, "block_local_load_cse", i32, one_pointer, 1u);
        entry = rcc_ir_block_add(function, "entry");
        first = append_load(entry, i32, function->parameters[0]);
        second = append_load(entry, i32, function->parameters[0]);
        sum_operands[0] = first;
        sum_operands[1] = second;
        sum = rcc_ir_append(entry, RCC_IR_ADD, i32, sum_operands, 2u,
                            NULL, 0u);
        assert(sum != NULL);
        append_return(entry, sum->result);
        assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
        assert(error[0] == '\0');
        assert(stats.commoned_instructions == 1u);
        assert(count_opcode(function, RCC_IR_LOAD) == 1u);
        assert(rcc_ir_verify_function(function, error, sizeof(error)));
        rcc_ir_module_destroy(module);
    }

    {
        RccIrType parameters[] = {pointer, pointer, i32};
        module = rcc_ir_module_create();
        function = rcc_ir_function_add(
            module, "load_cse_store_barrier", i32, parameters, 3u);
        entry = rcc_ir_block_add(function, "entry");
        first = append_load(entry, i32, function->parameters[0]);
        append_store(entry, function->parameters[2], function->parameters[1]);
        second = append_load(entry, i32, function->parameters[0]);
        sum_operands[0] = first;
        sum_operands[1] = second;
        sum = rcc_ir_append(entry, RCC_IR_ADD, i32, sum_operands, 2u,
                            NULL, 0u);
        assert(sum != NULL);
        append_return(entry, sum->result);
        assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
        assert(stats.commoned_instructions == 0u);
        assert(count_opcode(function, RCC_IR_LOAD) == 2u);
        assert(rcc_ir_verify_function(function, error, sizeof(error)));
        rcc_ir_module_destroy(module);
    }

    {
        RccIrValue first_address;
        RccIrValue second_address;
        RccIrValue first_value;
        RccIrValue second_value;
        RccIrInstruction* mutating_call;
        module = rcc_ir_module_create();
        function = rcc_ir_function_add(
            module, "load_cse_disjoint_allocas", i32, NULL, 0u);
        entry = rcc_ir_block_add(function, "entry");
        first_address = append_alloca(entry, 4u);
        second_address = append_alloca(entry, 4u);
        first_value = append_const(entry, i32, 11u);
        second_value = append_const(entry, i32, 29u);
        append_store(entry, first_value, first_address);
        mutating_call = rcc_ir_append(
            entry, RCC_IR_CALL, rcc_ir_type_void(), &first_address, 1u,
            NULL, 0u);
        assert(mutating_call != NULL);
        rcc_ir_set_callee(mutating_call, "mutate_memory");
        first = append_load(entry, i32, first_address);
        append_store(entry, second_value, second_address);
        second = append_load(entry, i32, first_address);
        sum_operands[0] = first;
        sum_operands[1] = second;
        sum = rcc_ir_append(entry, RCC_IR_ADD, i32, sum_operands, 2u,
                            NULL, 0u);
        assert(sum != NULL);
        append_return(entry, sum->result);
        assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
        assert(stats.commoned_instructions == 1u);
        assert(count_opcode(function, RCC_IR_LOAD) == 1u);
        assert(rcc_ir_verify_function(function, error, sizeof(error)));
        rcc_ir_module_destroy(module);
    }

    {
        RccIrType parameters[] = {pointer};
        RccIrValue address;
        RccIrValue stored;
        RccIrValue loaded;
        module = rcc_ir_module_create();
        function = rcc_ir_function_add(
            module, "store_forwarding_unknown_alias", i32, parameters, 1u);
        entry = rcc_ir_block_add(function, "entry");
        address = append_alloca(entry, 4u);
        stored = append_const(entry, i32, 17u);
        append_store(entry, stored, address);
        append_store(entry, stored, function->parameters[0]);
        loaded = append_load(entry, i32, address);
        append_return(entry, loaded);
        assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
        assert(stats.commoned_instructions == 0u);
        assert(count_opcode(function, RCC_IR_LOAD) == 1u);
        assert(rcc_ir_verify_function(function, error, sizeof(error)));
        rcc_ir_module_destroy(module);
    }

    {
        RccIrValue address;
        RccIrValue stored;
        RccIrInstruction* call;
        RccIrValue loaded;
        module = rcc_ir_module_create();
        function = rcc_ir_function_add(
            module, "store_forwarding_call_barrier", i32, NULL, 0u);
        entry = rcc_ir_block_add(function, "entry");
        address = append_alloca(entry, 4u);
        stored = append_const(entry, i32, 19u);
        append_store(entry, stored, address);
        call = rcc_ir_append(entry, RCC_IR_CALL, rcc_ir_type_void(),
                             &address, 1u, NULL, 0u);
        assert(call != NULL);
        rcc_ir_set_callee(call, "mutate_memory");
        loaded = append_load(entry, i32, address);
        append_return(entry, loaded);
        assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
        assert(stats.commoned_instructions == 0u);
        assert(count_opcode(function, RCC_IR_LOAD) == 1u);
        assert(rcc_ir_verify_function(function, error, sizeof(error)));
        rcc_ir_module_destroy(module);
    }

    {
        RccIrValue address;
        RccIrValue stored;
        RccIrInstruction* volatile_load;
        module = rcc_ir_module_create();
        function = rcc_ir_function_add(
            module, "store_forwarding_volatile_load", i32, NULL, 0u);
        entry = rcc_ir_block_add(function, "entry");
        address = append_alloca(entry, 4u);
        stored = append_const(entry, i32, 41u);
        append_store(entry, stored, address);
        volatile_load = rcc_ir_append(
            entry, RCC_IR_LOAD, i32, &address, 1u, NULL, 0u);
        assert(volatile_load != NULL);
        rcc_ir_set_volatile_access(volatile_load, true);
        append_return(entry, volatile_load->result);
        assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
        assert(stats.commoned_instructions == 0u);
        assert(count_opcode(function, RCC_IR_LOAD) == 1u);
        assert(rcc_ir_verify_function(function, error, sizeof(error)));
        rcc_ir_module_destroy(module);
    }

    {
        RccIrValue address;
        RccIrValue stored;
        RccIrInstruction* volatile_store;
        RccIrValue store_operands[2];
        RccIrValue loaded;
        module = rcc_ir_module_create();
        function = rcc_ir_function_add(
            module, "store_forwarding_volatile_store", i32, NULL, 0u);
        entry = rcc_ir_block_add(function, "entry");
        address = append_alloca(entry, 4u);
        stored = append_const(entry, i32, 43u);
        store_operands[0] = stored;
        store_operands[1] = address;
        volatile_store = rcc_ir_append(
            entry, RCC_IR_STORE, rcc_ir_type_void(), store_operands, 2u,
            NULL, 0u);
        assert(volatile_store != NULL);
        rcc_ir_set_volatile_access(volatile_store, true);
        loaded = append_load(entry, i32, address);
        append_return(entry, loaded);
        assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
        assert(stats.commoned_instructions == 0u);
        assert(count_opcode(function, RCC_IR_LOAD) == 1u);
        assert(rcc_ir_verify_function(function, error, sizeof(error)));
        rcc_ir_module_destroy(module);
    }

    {
        RccIrType parameters[] = {pointer, pointer};
        RccIrValue call_argument;
        RccIrInstruction* call;
        module = rcc_ir_module_create();
        function = rcc_ir_function_add(
            module, "load_cse_call_barrier", i32, parameters, 2u);
        entry = rcc_ir_block_add(function, "entry");
        first = append_load(entry, i32, function->parameters[0]);
        call_argument = function->parameters[1];
        call = rcc_ir_append(entry, RCC_IR_CALL, rcc_ir_type_void(),
                             &call_argument, 1u, NULL, 0u);
        assert(call != NULL);
        rcc_ir_set_callee(call, "mutate_memory");
        second = append_load(entry, i32, function->parameters[0]);
        sum_operands[0] = first;
        sum_operands[1] = second;
        sum = rcc_ir_append(entry, RCC_IR_ADD, i32, sum_operands, 2u,
                            NULL, 0u);
        assert(sum != NULL);
        append_return(entry, sum->result);
        assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
        assert(stats.commoned_instructions == 0u);
        assert(count_opcode(function, RCC_IR_LOAD) == 2u);
        assert(rcc_ir_verify_function(function, error, sizeof(error)));
        rcc_ir_module_destroy(module);
    }

    {
        RccIrType parameters[] = {pointer, pointer};
        RccIrInstruction* volatile_load;
        module = rcc_ir_module_create();
        function = rcc_ir_function_add(
            module, "load_cse_volatile_barrier", i32, parameters, 2u);
        entry = rcc_ir_block_add(function, "entry");
        first = append_load(entry, i32, function->parameters[0]);
        volatile_load = rcc_ir_append(
            entry, RCC_IR_LOAD, i32, &function->parameters[1], 1u,
            NULL, 0u);
        assert(volatile_load != NULL);
        rcc_ir_set_volatile_access(volatile_load, true);
        second = append_load(entry, i32, function->parameters[0]);
        sum_operands[0] = first;
        sum_operands[1] = second;
        sum = rcc_ir_append(entry, RCC_IR_ADD, i32, sum_operands, 2u,
                            NULL, 0u);
        assert(sum != NULL);
        append_return(entry, sum->result);
        assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
        assert(stats.commoned_instructions == 0u);
        assert(count_opcode(function, RCC_IR_LOAD) == 3u);
        assert(rcc_ir_verify_function(function, error, sizeof(error)));
        rcc_ir_module_destroy(module);
    }

    {
        RccIrType i8 = rcc_ir_type_integer(8u);
        module = rcc_ir_module_create();
        function = rcc_ir_function_add(
            module, "load_cse_type_barrier", i32, one_pointer, 1u);
        entry = rcc_ir_block_add(function, "entry");
        first = append_load(entry, i32, function->parameters[0]);
        (void)append_load(entry, i8, function->parameters[0]);
        append_return(entry, first);
        assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
        assert(stats.commoned_instructions == 0u);
        assert(count_opcode(function, RCC_IR_LOAD) == 2u);
        assert(rcc_ir_verify_function(function, error, sizeof(error)));
        rcc_ir_module_destroy(module);
    }

    {
        RccIrType parameters[] = {pointer};
        RccIrBlock* successor;
        module = rcc_ir_module_create();
        function = rcc_ir_function_add(
            module, "load_cse_block_barrier", i32, parameters, 1u);
        entry = rcc_ir_block_add(function, "entry");
        successor = rcc_ir_block_add(function, "successor");
        first = append_load(entry, i32, function->parameters[0]);
        append_branch(entry, successor->id);
        second = append_load(successor, i32, function->parameters[0]);
        sum_operands[0] = first;
        sum_operands[1] = second;
        sum = rcc_ir_append(successor, RCC_IR_ADD, i32, sum_operands, 2u,
                            NULL, 0u);
        assert(sum != NULL);
        append_return(successor, sum->result);
        assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
        assert(stats.commoned_instructions == 0u);
        assert(count_opcode(function, RCC_IR_LOAD) == 2u);
        assert(rcc_ir_verify_function(function, error, sizeof(error)));
        rcc_ir_module_destroy(module);
    }
}

static void verify_volatile_access_survives_mem2reg(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType parameters[] = {i32};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "volatile_access_survives_mem2reg", i32, parameters, 1u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrValue address = append_alloca(entry, 4u);
    RccIrValue first;
    RccIrValue second;
    RccIrInstruction* store;
    RccIrInstruction* load;
    RccIrValue sum_operands[2];
    RccIrInstruction* sum;
    RccIrMem2RegStats stats;
    char error[256];

    append_store(entry, function->parameters[0], address);
    load = rcc_ir_append(entry, RCC_IR_LOAD, i32, &address, 1u, NULL, 0u);
    assert(load != NULL);
    rcc_ir_set_volatile_access(load, true);
    first = load->result;
    store = rcc_ir_append(
        entry, RCC_IR_STORE, rcc_ir_type_void(),
        (RccIrValue[]){first, address}, 2u, NULL, 0u);
    assert(store != NULL);
    rcc_ir_set_volatile_access(store, true);
    load = rcc_ir_append(entry, RCC_IR_LOAD, i32, &address, 1u, NULL, 0u);
    assert(load != NULL);
    rcc_ir_set_volatile_access(load, true);
    second = load->result;
    sum_operands[0] = first;
    sum_operands[1] = second;
    sum = rcc_ir_append(entry, RCC_IR_ADD, i32, sum_operands, 2u,
                        NULL, 0u);
    assert(sum != NULL);
    append_return(entry, sum->result);

    assert(rcc_ir_mem2reg(function, &stats, error, sizeof(error)));
    assert(error[0] == '\0');
    assert(stats.promoted_allocas == 0u);
    assert(stats.removed_loads == 0u);
    assert(stats.removed_stores == 0u);
    assert(count_opcode(function, RCC_IR_ALLOCA) == 1u);
    assert(count_opcode(function, RCC_IR_LOAD) == 2u);
    assert(count_opcode(function, RCC_IR_STORE) == 2u);
    assert(rcc_ir_verify_function(function, error, sizeof(error)));
    rcc_ir_module_destroy(module);
}

static void verify_dominator_scoped_gvn(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType i1 = rcc_ir_type_integer(1u);
    RccIrType parameters[] = {i32, i32, i1};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "dominator_gvn", i32, parameters, 3u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrBlock* left = rcc_ir_block_add(function, "left");
    RccIrBlock* right = rcc_ir_block_add(function, "right");
    RccIrValue operands[] = {
        function->parameters[0], function->parameters[1],
    };
    RccIrValue reversed[] = {
        function->parameters[1], function->parameters[0],
    };
    RccIrInstruction* dominating = rcc_ir_append(
        entry, RCC_IR_ADD, i32, operands, 2u, NULL, 0u);
    RccIrInstruction* left_duplicate;
    RccIrInstruction* right_duplicate;
    RccIrSimplifyStats stats;
    char error[256];
    assert(dominating != NULL);
    append_cond_branch(entry, function->parameters[2],
                       left->id, right->id);
    left_duplicate = rcc_ir_append(
        left, RCC_IR_ADD, i32, reversed, 2u, NULL, 0u);
    right_duplicate = rcc_ir_append(
        right, RCC_IR_ADD, i32, operands, 2u, NULL, 0u);
    assert(left_duplicate != NULL && right_duplicate != NULL);
    append_return(left, left_duplicate->result);
    append_return(right, right_duplicate->result);
    assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
    assert(stats.commoned_instructions == 2u);
    assert(count_opcode(function, RCC_IR_ADD) == 1u);
    assert(rcc_ir_verify_function(function, error, sizeof(error)));
    rcc_ir_module_destroy(module);
}

static void verify_sibling_values_are_not_commoned(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType i1 = rcc_ir_type_integer(1u);
    RccIrType parameters[] = {i32, i32, i1};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "sibling_gvn", i32, parameters, 3u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrBlock* left = rcc_ir_block_add(function, "left");
    RccIrBlock* right = rcc_ir_block_add(function, "right");
    RccIrBlock* merge = rcc_ir_block_add(function, "merge");
    RccIrValue operands[] = {
        function->parameters[0], function->parameters[1],
    };
    RccIrInstruction* left_value;
    RccIrInstruction* right_value;
    RccIrValue incoming[2];
    RccIrBlockId targets[] = {left->id, right->id};
    RccIrInstruction* phi;
    RccIrSimplifyStats stats;
    char error[256];
    append_cond_branch(entry, function->parameters[2],
                       left->id, right->id);
    left_value = rcc_ir_append(
        left, RCC_IR_ADD, i32, operands, 2u, NULL, 0u);
    right_value = rcc_ir_append(
        right, RCC_IR_ADD, i32, operands, 2u, NULL, 0u);
    assert(left_value != NULL && right_value != NULL);
    append_branch(left, merge->id);
    append_branch(right, merge->id);
    incoming[0] = left_value->result;
    incoming[1] = right_value->result;
    phi = rcc_ir_append(merge, RCC_IR_PHI, i32,
                        incoming, 2u, targets, 2u);
    assert(phi != NULL);
    append_return(merge, phi->result);
    assert(rcc_ir_simplify(function, &stats, error, sizeof(error)));
    assert(stats.commoned_instructions == 0u);
    assert(count_opcode(function, RCC_IR_ADD) == 2u);
    assert(rcc_ir_verify_function(function, error, sizeof(error)));
    rcc_ir_module_destroy(module);
}

static void verify_loop_invariant_code_motion(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType i1 = rcc_ir_type_integer(1u);
    RccIrType parameters[] = {i32, i32, i32};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* function = rcc_ir_function_add(
        module, "loop_licm", i32, parameters, 3u);
    RccIrBlock* entry = rcc_ir_block_add(function, "entry");
    RccIrBlock* header = rcc_ir_block_add(function, "header");
    RccIrBlock* body = rcc_ir_block_add(function, "body");
    RccIrBlock* exit = rcc_ir_block_add(function, "exit");
    RccIrValue zero = append_const(entry, i32, 0u);
    RccIrValue invariant;
    RccIrValue next;
    RccIrValue compare_operands[2];
    RccIrValue condition[1];
    RccIrValue phi_operands[2];
    RccIrBlockId phi_predecessors[2];
    RccIrBlockId branch_targets[2];
    RccIrInstruction* phi;
    RccIrInstruction* compare;
    RccIrInstruction* unsafe_division;
    RccIrInstruction* unsafe_shift;
    RccIrInstruction* safe_division;
    RccIrInstruction* safe_shift;
    RccIrInstruction* safe_left_shift;
    RccIrInstruction* out_of_range_left_shift;
    RccIrValue unsafe_operands[2];
    RccIrValue safe_operands[2];
    RccIrValue safe_divisor;
    RccIrValue safe_shift_count;
    RccIrValue out_of_range_shift_count;
    RccIrOptimizationStats stats;
    char error[256];

    safe_divisor = append_const(entry, i32, 3u);
    safe_shift_count = append_const(entry, i32, 2u);
    out_of_range_shift_count = append_const(entry, i32, 32u);
    append_branch(entry, header->id);
    phi_operands[0] = zero;
    phi_operands[1] = RCC_IR_VALUE_NONE;
    phi_predecessors[0] = entry->id;
    phi_predecessors[1] = body->id;
    phi = rcc_ir_append(header, RCC_IR_PHI, i32, phi_operands, 2u,
                        phi_predecessors, 2u);
    assert(phi != NULL);
    compare_operands[0] = phi->result;
    compare_operands[1] = function->parameters[2];
    compare = rcc_ir_append(header, RCC_IR_ICMP, i1,
                            compare_operands, 2u, NULL, 0u);
    assert(compare != NULL);
    rcc_ir_set_predicate(compare, RCC_IR_ICMP_SLT);
    condition[0] = compare->result;
    branch_targets[0] = body->id;
    branch_targets[1] = exit->id;
    assert(rcc_ir_append(header, RCC_IR_COND_BRANCH,
                         rcc_ir_type_void(), condition, 1u,
                         branch_targets, 2u) != NULL);

    invariant = append_binary(body, RCC_IR_ADD, i32,
                              function->parameters[0],
                              function->parameters[1]);
    unsafe_operands[0] = function->parameters[0];
    unsafe_operands[1] = function->parameters[1];
    unsafe_division = rcc_ir_append(
        body, RCC_IR_SDIV, i32, unsafe_operands, 2u, NULL, 0u);
    assert(unsafe_division != NULL);
    unsafe_shift = rcc_ir_append(
        body, RCC_IR_SHL, i32, unsafe_operands, 2u, NULL, 0u);
    assert(unsafe_shift != NULL);
    safe_operands[0] = function->parameters[0];
    safe_operands[1] = safe_divisor;
    safe_division = rcc_ir_append(
        body, RCC_IR_SDIV, i32, safe_operands, 2u, NULL, 0u);
    assert(safe_division != NULL);
    safe_operands[1] = safe_shift_count;
    safe_shift = rcc_ir_append(
        body, RCC_IR_LSHR, i32, safe_operands, 2u, NULL, 0u);
    assert(safe_shift != NULL);
    safe_left_shift = rcc_ir_append(
        body, RCC_IR_SHL, i32, safe_operands, 2u, NULL, 0u);
    assert(safe_left_shift != NULL);
    safe_operands[1] = out_of_range_shift_count;
    out_of_range_left_shift = rcc_ir_append(
        body, RCC_IR_SHL, i32, safe_operands, 2u, NULL, 0u);
    assert(out_of_range_left_shift != NULL);
    next = append_binary(body, RCC_IR_ADD, i32,
                         phi->result, invariant);
    next = append_binary(body, RCC_IR_ADD, i32,
                         next, safe_division->result);
    next = append_binary(body, RCC_IR_ADD, i32,
                         next, safe_shift->result);
    next = append_binary(body, RCC_IR_ADD, i32,
                         next, safe_left_shift->result);
    next = append_binary(body, RCC_IR_ADD, i32,
                         next, out_of_range_left_shift->result);
    next = append_binary(body, RCC_IR_ADD, i32,
                         next, unsafe_division->result);
    next = append_binary(body, RCC_IR_ADD, i32,
                         next, unsafe_shift->result);
    append_branch(body, header->id);
    phi->operands[1] = next;
    append_return(exit, phi->result);

    assert(rcc_ir_optimize_function(function, 2u, &stats,
                                    error, sizeof(error)));
    assert(error[0] == '\0');
    assert(stats.hoisted_instructions == 4u);
    {
        RccIrInstruction* entry_instruction;
        size_t entry_adds = 0u;
        for (entry_instruction = entry->first; entry_instruction;
             entry_instruction = entry_instruction->next) {
            if (entry_instruction->opcode == RCC_IR_ADD) ++entry_adds;
        }
        assert(entry_adds == 1u);
    }
    assert(safe_division->block == entry);
    assert(safe_shift->block == entry);
    assert(safe_left_shift->block == entry);
    assert(unsafe_division->block == body);
    assert(unsafe_shift->block == body);
    assert(out_of_range_left_shift->block == body);
    assert(rcc_ir_verify_function(function, error, sizeof(error)));
    rcc_ir_module_destroy(module);
}

int main(void)
{
    verify_optimization_level_pipeline();
    verify_debug_local_promotion_policy();
    verify_diamond_promotion();
    verify_loop_promotion();
    verify_escape_is_not_promoted();
    verify_integer_simplification();
    verify_self_compare_simplification();
    verify_self_binary_simplification();
    verify_modulo_one_simplification();
    verify_constant_branch_pruning();
    verify_equal_branch_target_simplification();
    verify_constant_phi_and_select_folding();
    verify_trivial_phi_simplification();
    verify_trivial_select_simplification();
    verify_constant_condition_select_simplification();
    verify_undefined_folds_are_preserved();
    verify_integer_identities();
    verify_block_local_cse();
    verify_store_to_load_forwarding_after_write();
    verify_block_local_load_cse();
    verify_volatile_access_survives_mem2reg();
    verify_dominator_scoped_gvn();
    verify_sibling_values_are_not_commoned();
    verify_loop_invariant_code_motion();
    puts("Typed SSA mem2reg and simplification tests passed");
    return 0;
}
