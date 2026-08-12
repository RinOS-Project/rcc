/*
 * RCC - Target-independent typed SSA optimization passes
 */

#include "rcc.h"
#include "ir_pass.h"

#include <stdarg.h>

typedef struct {
    RccIrInstruction* allocation;
    RccIrInstruction* initial_store;
    RccIrValue address;
    RccIrType type;
} RccIrPromotedVariable;

typedef struct {
    RccIrFunction* function;
    RccIrBlock** blocks;
    size_t block_count;
    RccIrPromotedVariable* variables;
    size_t variable_count;
    size_t original_value_count;
    size_t* address_variables;
    bool* dominator_children;
    RccIrInstruction** phis;
    RccIrValue* current_values;
    RccIrValue* replacements;
    size_t replacement_count;
    RccIrMem2RegStats* stats;
    bool failed;
} RccIrRenameContext;

static bool ir_pass_error(char* error, size_t error_size,
                          const char* format, ...) {
    va_list arguments;
    if (error && error_size != 0u) {
        va_start(arguments, format);
        vsnprintf(error, error_size, format, arguments);
        va_end(arguments);
    }
    return false;
}

static bool ir_pass_checked_square(size_t count, size_t item_size) {
    if (count != 0u && count > SIZE_MAX / count) return false;
    return count * count <= SIZE_MAX / item_size;
}

static RccIrValue ir_pass_resolve(const RccIrValue* replacements,
                                  size_t replacement_count,
                                  RccIrValue value) {
    size_t steps = 0u;
    while (value != RCC_IR_VALUE_NONE && value < replacement_count &&
           replacements[value] != RCC_IR_VALUE_NONE &&
           replacements[value] != value) {
        value = replacements[value];
        if (++steps > replacement_count) return RCC_IR_VALUE_NONE;
    }
    return value;
}

static void ir_pass_unlink_instruction(RccIrInstruction* instruction) {
    RccIrBlock* block;
    if (!instruction || !instruction->block) return;
    block = instruction->block;
    if (instruction->previous) {
        instruction->previous->next = instruction->next;
    } else {
        block->first = instruction->next;
    }
    if (instruction->next) {
        instruction->next->previous = instruction->previous;
    } else {
        block->last = instruction->previous;
    }
    rcc_free(instruction->operands);
    rcc_free(instruction->targets);
    rcc_free(instruction->callee);
    rcc_free(instruction);
}

static bool ir_pass_collect_blocks(RccIrFunction* function,
                                   RccIrBlock*** blocks_out,
                                   char* error, size_t error_size) {
    RccIrBlock** blocks;
    RccIrBlock* block;
    size_t index = 0u;
    if (!function || function->block_count == 0u) {
        return ir_pass_error(error, error_size,
                             "mem2reg requires a non-empty function");
    }
    blocks = rcc_alloc(function->block_count * sizeof(*blocks));
    for (block = function->first_block; block; block = block->next) {
        if (index >= function->block_count || block->id != index) {
            rcc_free(blocks);
            return ir_pass_error(error, error_size,
                                 "mem2reg requires canonical block IDs");
        }
        blocks[index++] = block;
    }
    if (index != function->block_count) {
        rcc_free(blocks);
        return ir_pass_error(error, error_size,
                             "mem2reg block table is inconsistent");
    }
    *blocks_out = blocks;
    return true;
}

static bool ir_pass_build_cfg(RccIrBlock** blocks, size_t block_count,
                              bool** predecessors_out,
                              char* error, size_t error_size) {
    bool* predecessors;
    size_t source;
    if (!ir_pass_checked_square(block_count, sizeof(bool))) {
        return ir_pass_error(error, error_size,
                             "mem2reg CFG is too large");
    }
    predecessors = rcc_alloc(block_count * block_count *
                             sizeof(*predecessors));
    for (source = 0u; source < block_count; ++source) {
        RccIrInstruction* terminator = blocks[source]->last;
        size_t target_index;
        if (!terminator) {
            rcc_free(predecessors);
            return ir_pass_error(error, error_size,
                                 "mem2reg found an unterminated block");
        }
        for (target_index = 0u; target_index < terminator->target_count;
             ++target_index) {
            RccIrBlockId target = terminator->targets[target_index];
            if (target >= block_count) {
                rcc_free(predecessors);
                return ir_pass_error(error, error_size,
                                     "mem2reg found an invalid CFG edge");
            }
            predecessors[(size_t)target * block_count + source] = true;
        }
    }
    *predecessors_out = predecessors;
    return true;
}

static bool ir_pass_compute_dominators(const bool* predecessors,
                                       size_t block_count,
                                       bool** dominators_out,
                                       size_t** immediate_out,
                                       bool** children_out,
                                       char* error, size_t error_size) {
    bool* dominators;
    bool* next;
    size_t* immediate;
    bool* children;
    bool changed = true;
    size_t block;
    if (!ir_pass_checked_square(block_count, sizeof(*dominators))) {
        return ir_pass_error(error, error_size,
                             "mem2reg dominator table is too large");
    }
    dominators = rcc_alloc(block_count * block_count *
                           sizeof(*dominators));
    next = rcc_alloc(block_count * sizeof(*next));
    immediate = rcc_alloc(block_count * sizeof(*immediate));
    children = rcc_alloc(block_count * block_count * sizeof(*children));
    dominators[0] = true;
    for (block = 1u; block < block_count; ++block) {
        size_t candidate;
        for (candidate = 0u; candidate < block_count; ++candidate) {
            dominators[block * block_count + candidate] = true;
        }
    }
    while (changed) {
        changed = false;
        for (block = 1u; block < block_count; ++block) {
            bool have_predecessor = false;
            size_t predecessor;
            size_t candidate;
            for (candidate = 0u; candidate < block_count; ++candidate) {
                next[candidate] = true;
            }
            for (predecessor = 0u; predecessor < block_count;
                 ++predecessor) {
                if (!predecessors[block * block_count + predecessor]) {
                    continue;
                }
                if (!have_predecessor) {
                    memcpy(next,
                           &dominators[predecessor * block_count],
                           block_count * sizeof(*next));
                    have_predecessor = true;
                } else {
                    for (candidate = 0u; candidate < block_count;
                         ++candidate) {
                        next[candidate] = next[candidate] &&
                            dominators[predecessor * block_count +
                                       candidate];
                    }
                }
            }
            if (!have_predecessor) {
                rcc_free(dominators);
                rcc_free(next);
                rcc_free(immediate);
                rcc_free(children);
                return ir_pass_error(error, error_size,
                                     "mem2reg found an unreachable block");
            }
            next[block] = true;
            for (candidate = 0u; candidate < block_count; ++candidate) {
                size_t offset = block * block_count + candidate;
                if (dominators[offset] != next[candidate]) {
                    dominators[offset] = next[candidate];
                    changed = true;
                }
            }
        }
    }
    immediate[0] = SIZE_MAX;
    for (block = 1u; block < block_count; ++block) {
        size_t candidate;
        size_t selected = SIZE_MAX;
        for (candidate = 0u; candidate < block_count; ++candidate) {
            size_t other;
            bool closest = true;
            if (candidate == block ||
                !dominators[block * block_count + candidate]) {
                continue;
            }
            for (other = 0u; other < block_count; ++other) {
                if (other == block || other == candidate ||
                    !dominators[block * block_count + other]) {
                    continue;
                }
                if (!dominators[candidate * block_count + other]) {
                    closest = false;
                    break;
                }
            }
            if (closest) {
                selected = candidate;
                break;
            }
        }
        if (selected == SIZE_MAX) {
            rcc_free(dominators);
            rcc_free(next);
            rcc_free(immediate);
            rcc_free(children);
            return ir_pass_error(error, error_size,
                                 "mem2reg could not form a dominator tree");
        }
        immediate[block] = selected;
        children[selected * block_count + block] = true;
    }
    rcc_free(next);
    *dominators_out = dominators;
    *immediate_out = immediate;
    *children_out = children;
    return true;
}

static bool ir_pass_value_type(const RccIrFunction* function,
                               RccIrValue value, RccIrType* type) {
    if (!function || value >= function->value_count || !type) return false;
    *type = function->value_types[value];
    return true;
}

static bool ir_pass_find_variable_type(
    RccIrFunction* function, RccIrInstruction* allocation,
    RccIrPromotedVariable* variable) {
    RccIrBlock* block;
    bool have_type = false;
    bool found_first_use = false;
    if (!function || !allocation || !variable) return false;
    variable->allocation = allocation;
    variable->address = allocation->result;
    for (block = function->first_block; block; block = block->next) {
        RccIrInstruction* instruction;
        for (instruction = block->first; instruction;
             instruction = instruction->next) {
            size_t operand_index;
            for (operand_index = 0u;
                 operand_index < instruction->operand_count;
                 ++operand_index) {
                RccIrType use_type;
                if (instruction->operands[operand_index] !=
                    variable->address) {
                    continue;
                }
                if (instruction->opcode == RCC_IR_LOAD &&
                    operand_index == 0u) {
                    use_type = instruction->type;
                } else if (instruction->opcode == RCC_IR_STORE &&
                           operand_index == 1u &&
                           ir_pass_value_type(function,
                                              instruction->operands[0],
                                              &use_type)) {
                } else {
                    return false;
                }
                if (!found_first_use) {
                    if (block != function->first_block ||
                        instruction->opcode != RCC_IR_STORE ||
                        operand_index != 1u) {
                        return false;
                    }
                    found_first_use = true;
                    variable->initial_store = instruction;
                }
                if (!have_type) {
                    variable->type = use_type;
                    have_type = true;
                } else if (!rcc_ir_type_equal(variable->type, use_type)) {
                    return false;
                }
            }
        }
    }
    return have_type && variable->initial_store != NULL;
}

static bool ir_pass_collect_variables(
    RccIrFunction* function, RccIrPromotedVariable** variables_out,
    size_t* variable_count_out, size_t** address_variables_out) {
    RccIrPromotedVariable* variables;
    size_t* address_variables;
    size_t count = 0u;
    RccIrInstruction* instruction;
    size_t index;
    if (function->value_count == 0u) {
        *variables_out = NULL;
        *variable_count_out = 0u;
        *address_variables_out = NULL;
        return true;
    }
    variables = rcc_alloc(function->value_count * sizeof(*variables));
    address_variables = rcc_alloc(function->value_count *
                                  sizeof(*address_variables));
    for (index = 0u; index < function->value_count; ++index) {
        address_variables[index] = SIZE_MAX;
    }
    for (instruction = function->first_block->first; instruction;
         instruction = instruction->next) {
        RccIrPromotedVariable variable;
        if (instruction->opcode != RCC_IR_ALLOCA) continue;
        memset(&variable, 0, sizeof(variable));
        if (!ir_pass_find_variable_type(function, instruction, &variable)) {
            continue;
        }
        variables[count] = variable;
        address_variables[variable.address] = count;
        ++count;
    }
    *variables_out = variables;
    *variable_count_out = count;
    *address_variables_out = address_variables;
    return true;
}

static RccIrInstruction* ir_pass_insert_phi(
    RccIrBlock* block, RccIrType type, const bool* predecessors,
    size_t block_count) {
    RccIrValue* operands;
    RccIrBlockId* targets;
    RccIrInstruction* phi;
    RccIrInstruction* insertion;
    size_t count = 0u;
    size_t predecessor;
    for (predecessor = 0u; predecessor < block_count; ++predecessor) {
        if (predecessors[(size_t)block->id * block_count + predecessor]) {
            ++count;
        }
    }
    if (count == 0u) return NULL;
    operands = rcc_alloc(count * sizeof(*operands));
    targets = rcc_alloc(count * sizeof(*targets));
    count = 0u;
    for (predecessor = 0u; predecessor < block_count; ++predecessor) {
        if (predecessors[(size_t)block->id * block_count + predecessor]) {
            operands[count] = RCC_IR_VALUE_NONE;
            targets[count] = (RccIrBlockId)predecessor;
            ++count;
        }
    }
    phi = rcc_ir_append(block, RCC_IR_PHI, type, operands, count,
                        targets, count);
    rcc_free(operands);
    rcc_free(targets);
    if (!phi) return NULL;
    if (phi->previous) {
        phi->previous->next = NULL;
        block->last = phi->previous;
    } else {
        block->first = NULL;
        block->last = NULL;
    }
    insertion = block->first;
    while (insertion && insertion->opcode == RCC_IR_PHI) {
        insertion = insertion->next;
    }
    if (!insertion) {
        phi->previous = block->last;
        phi->next = NULL;
        if (block->last) block->last->next = phi;
        else block->first = phi;
        block->last = phi;
    } else {
        phi->previous = insertion->previous;
        phi->next = insertion;
        if (insertion->previous) insertion->previous->next = phi;
        else block->first = phi;
        insertion->previous = phi;
    }
    return phi;
}

static bool ir_pass_place_phis(
    RccIrFunction* function, RccIrBlock** blocks,
    RccIrPromotedVariable* variables,
    size_t variable_count, const bool* predecessors,
    const size_t* immediate, RccIrInstruction*** phis_out,
    RccIrMem2RegStats* stats, char* error, size_t error_size) {
    size_t block_count = function->block_count;
    bool* frontier;
    bool* definitions;
    bool* queued;
    size_t* queue;
    RccIrInstruction** phis;
    size_t block;
    size_t variable;
    if (!ir_pass_checked_square(block_count, sizeof(*frontier)) ||
        variable_count > SIZE_MAX / block_count) {
        return ir_pass_error(error, error_size,
                             "mem2reg phi tables are too large");
    }
    frontier = rcc_alloc(block_count * block_count * sizeof(*frontier));
    definitions = rcc_alloc(variable_count * block_count *
                            sizeof(*definitions));
    queued = rcc_alloc(block_count * sizeof(*queued));
    queue = rcc_alloc(block_count * sizeof(*queue));
    phis = rcc_alloc(variable_count * block_count * sizeof(*phis));
    for (block = 1u; block < block_count; ++block) {
        size_t predecessor_count = 0u;
        size_t predecessor;
        for (predecessor = 0u; predecessor < block_count; ++predecessor) {
            if (predecessors[block * block_count + predecessor]) {
                ++predecessor_count;
            }
        }
        if (predecessor_count < 2u) continue;
        for (predecessor = 0u; predecessor < block_count; ++predecessor) {
            size_t runner;
            if (!predecessors[block * block_count + predecessor]) continue;
            runner = predecessor;
            while (runner != immediate[block]) {
                if (runner == SIZE_MAX) break;
                frontier[runner * block_count + block] = true;
                runner = immediate[runner];
            }
        }
    }
    for (block = 0u; block < block_count; ++block) {
        RccIrInstruction* instruction;
        for (instruction = blocks[block]->first; instruction;
             instruction = instruction->next) {
            if (instruction->opcode == RCC_IR_STORE &&
                instruction->operand_count == 2u &&
                instruction->operands[1] < function->value_count) {
                for (variable = 0u; variable < variable_count; ++variable) {
                    if (variables[variable].address ==
                        instruction->operands[1]) {
                        definitions[variable * block_count + block] = true;
                        break;
                    }
                }
            }
        }
    }
    for (variable = 0u; variable < variable_count; ++variable) {
        size_t head = 0u;
        size_t tail = 0u;
        memset(queued, 0, block_count * sizeof(*queued));
        for (block = 0u; block < block_count; ++block) {
            if (definitions[variable * block_count + block]) {
                queue[tail++] = block;
                queued[block] = true;
            }
        }
        while (head < tail) {
            size_t source = queue[head++];
            size_t target;
            for (target = 0u; target < block_count; ++target) {
                size_t phi_index = variable * block_count + target;
                if (!frontier[source * block_count + target] ||
                    phis[phi_index]) {
                    continue;
                }
                phis[phi_index] = ir_pass_insert_phi(
                    blocks[target], variables[variable].type,
                    predecessors, block_count);
                if (!phis[phi_index]) {
                    rcc_free(frontier);
                    rcc_free(definitions);
                    rcc_free(queued);
                    rcc_free(queue);
                    rcc_free(phis);
                    return ir_pass_error(error, error_size,
                                         "mem2reg could not insert phi");
                }
                ++stats->inserted_phis;
                if (!queued[target]) {
                    queue[tail++] = target;
                    queued[target] = true;
                }
            }
        }
    }
    rcc_free(frontier);
    rcc_free(definitions);
    rcc_free(queued);
    rcc_free(queue);
    *phis_out = phis;
    return true;
}

static size_t ir_pass_address_variable(const RccIrRenameContext* context,
                                       RccIrValue address) {
    if (address >= context->original_value_count) return SIZE_MAX;
    return context->address_variables[address];
}

static void ir_pass_rename_block(RccIrRenameContext* context,
                                 size_t block_index) {
    RccIrValue* saved;
    RccIrBlock* block;
    RccIrInstruction* instruction;
    size_t variable;
    size_t successor;
    if (context->failed) return;
    saved = rcc_alloc(context->variable_count * sizeof(*saved));
    memcpy(saved, context->current_values,
           context->variable_count * sizeof(*saved));
    block = context->blocks[block_index];
    for (variable = 0u; variable < context->variable_count; ++variable) {
        RccIrInstruction* phi = context->phis[
            variable * context->block_count + block_index];
        if (phi) context->current_values[variable] = phi->result;
    }
    instruction = block->first;
    while (instruction) {
        RccIrInstruction* next = instruction->next;
        size_t mapped = SIZE_MAX;
        size_t operand;
        if (instruction->opcode == RCC_IR_PHI) {
            instruction = next;
            continue;
        }
        if (instruction->opcode == RCC_IR_ALLOCA) {
            mapped = ir_pass_address_variable(context,
                                              instruction->result);
            if (mapped != SIZE_MAX) {
                ir_pass_unlink_instruction(instruction);
                instruction = next;
                continue;
            }
        }
        if (instruction->opcode == RCC_IR_STORE &&
            instruction->operand_count == 2u) {
            mapped = ir_pass_address_variable(
                context, instruction->operands[1]);
            if (mapped != SIZE_MAX) {
                RccIrValue value = ir_pass_resolve(
                    context->replacements, context->replacement_count,
                    instruction->operands[0]);
                if (value == RCC_IR_VALUE_NONE) {
                    context->failed = true;
                    break;
                }
                context->current_values[mapped] = value;
                ++context->stats->removed_stores;
                ir_pass_unlink_instruction(instruction);
                instruction = next;
                continue;
            }
        }
        if (instruction->opcode == RCC_IR_LOAD &&
            instruction->operand_count == 1u) {
            mapped = ir_pass_address_variable(
                context, instruction->operands[0]);
            if (mapped != SIZE_MAX) {
                RccIrValue value = context->current_values[mapped];
                value = ir_pass_resolve(context->replacements,
                                        context->replacement_count, value);
                if (value == RCC_IR_VALUE_NONE ||
                    instruction->result >= context->replacement_count) {
                    context->failed = true;
                    break;
                }
                context->replacements[instruction->result] = value;
                ++context->stats->removed_loads;
                ir_pass_unlink_instruction(instruction);
                instruction = next;
                continue;
            }
        }
        for (operand = 0u; operand < instruction->operand_count; ++operand) {
            RccIrValue value = ir_pass_resolve(
                context->replacements, context->replacement_count,
                instruction->operands[operand]);
            if (value == RCC_IR_VALUE_NONE) {
                context->failed = true;
                break;
            }
            instruction->operands[operand] = value;
        }
        if (context->failed) break;
        instruction = next;
    }
    if (!context->failed) {
        RccIrInstruction* terminator = block->last;
        for (successor = 0u; successor < terminator->target_count;
             ++successor) {
            size_t target = terminator->targets[successor];
            for (variable = 0u; variable < context->variable_count;
                 ++variable) {
                RccIrInstruction* phi = context->phis[
                    variable * context->block_count + target];
                size_t incoming;
                RccIrValue value;
                if (!phi) continue;
                value = ir_pass_resolve(context->replacements,
                                        context->replacement_count,
                                        context->current_values[variable]);
                if (value == RCC_IR_VALUE_NONE) {
                    context->failed = true;
                    break;
                }
                for (incoming = 0u; incoming < phi->target_count;
                     ++incoming) {
                    if (phi->targets[incoming] == block_index) {
                        phi->operands[incoming] = value;
                        break;
                    }
                }
                if (incoming == phi->target_count) {
                    context->failed = true;
                    break;
                }
            }
            if (context->failed) break;
        }
    }
    if (!context->failed) {
        size_t child;
        for (child = 0u; child < context->block_count; ++child) {
            if (context->dominator_children[
                    block_index * context->block_count + child]) {
                ir_pass_rename_block(context, child);
                if (context->failed) break;
            }
        }
    }
    memcpy(context->current_values, saved,
           context->variable_count * sizeof(*saved));
    rcc_free(saved);
}

static bool ir_pass_compact_values(RccIrFunction* function,
                                   const RccIrValue* replacements,
                                   size_t replacement_count,
                                   char* error, size_t error_size) {
    size_t old_count = function->value_count;
    size_t* mapping = rcc_alloc(old_count * sizeof(*mapping));
    RccIrType* types = rcc_alloc(old_count * sizeof(*types));
    size_t count = 0u;
    size_t index;
    RccIrBlock* block;
    for (index = 0u; index < old_count; ++index) mapping[index] = SIZE_MAX;
    for (index = 0u; index < function->parameter_count; ++index) {
        RccIrValue old = function->parameters[index];
        mapping[old] = count;
        function->parameters[index] = (RccIrValue)count;
        types[count++] = function->parameter_types[index];
    }
    for (block = function->first_block; block; block = block->next) {
        RccIrInstruction* instruction;
        for (instruction = block->first; instruction;
             instruction = instruction->next) {
            if (instruction->result == RCC_IR_VALUE_NONE) continue;
            if (instruction->result >= old_count ||
                mapping[instruction->result] != SIZE_MAX) {
                rcc_free(mapping);
                rcc_free(types);
                return ir_pass_error(error, error_size,
                                     "mem2reg found duplicate SSA values");
            }
            mapping[instruction->result] = count;
            instruction->result = (RccIrValue)count;
            types[count++] = instruction->type;
        }
    }
    for (block = function->first_block; block; block = block->next) {
        RccIrInstruction* instruction;
        for (instruction = block->first; instruction;
             instruction = instruction->next) {
            size_t operand;
            for (operand = 0u; operand < instruction->operand_count;
                 ++operand) {
                RccIrValue old = ir_pass_resolve(
                    replacements, replacement_count,
                    instruction->operands[operand]);
                if (old >= old_count || mapping[old] == SIZE_MAX) {
                    rcc_free(mapping);
                    rcc_free(types);
                    return ir_pass_error(error, error_size,
                                         "mem2reg left a dangling SSA use");
                }
                instruction->operands[operand] =
                    (RccIrValue)mapping[old];
            }
        }
    }
    rcc_free(function->value_types);
    function->value_types = types;
    function->value_count = count;
    function->value_capacity = old_count;
    rcc_free(mapping);
    return true;
}

bool rcc_ir_mem2reg(RccIrFunction* function, RccIrMem2RegStats* stats,
                    char* error, size_t error_size) {
    RccIrMem2RegStats local_stats;
    RccIrBlock** blocks = NULL;
    bool* predecessors = NULL;
    bool* dominators = NULL;
    size_t* immediate = NULL;
    bool* children = NULL;
    RccIrPromotedVariable* variables = NULL;
    size_t variable_count = 0u;
    size_t* address_variables = NULL;
    RccIrInstruction** phis = NULL;
    RccIrRenameContext rename;
    bool result = false;
    size_t index;
    memset(&local_stats, 0, sizeof(local_stats));
    if (stats) memset(stats, 0, sizeof(*stats));
    if (error && error_size != 0u) error[0] = '\0';
    if (!function || !rcc_ir_verify_function(function, error, error_size)) {
        return false;
    }
    if (!ir_pass_collect_blocks(function, &blocks, error, error_size) ||
        !ir_pass_build_cfg(blocks, function->block_count, &predecessors,
                           error, error_size) ||
        !ir_pass_compute_dominators(predecessors, function->block_count,
                                    &dominators, &immediate, &children,
                                    error, error_size) ||
        !ir_pass_collect_variables(function, &variables, &variable_count,
                                   &address_variables)) {
        goto cleanup;
    }
    if (variable_count == 0u) {
        result = true;
        goto cleanup;
    }
    if (!ir_pass_place_phis(function, blocks, variables, variable_count,
                            predecessors, immediate, &phis, &local_stats,
                            error, error_size)) {
        goto cleanup;
    }
    memset(&rename, 0, sizeof(rename));
    rename.function = function;
    rename.blocks = blocks;
    rename.block_count = function->block_count;
    rename.variables = variables;
    rename.variable_count = variable_count;
    rename.original_value_count = function->value_count -
        local_stats.inserted_phis;
    rename.address_variables = address_variables;
    rename.dominator_children = children;
    rename.phis = phis;
    rename.replacement_count = function->value_count;
    rename.replacements = rcc_alloc(rename.replacement_count *
                                    sizeof(*rename.replacements));
    rename.current_values = rcc_alloc(variable_count *
                                      sizeof(*rename.current_values));
    rename.stats = &local_stats;
    for (index = 0u; index < rename.replacement_count; ++index) {
        rename.replacements[index] = RCC_IR_VALUE_NONE;
    }
    for (index = 0u; index < variable_count; ++index) {
        rename.current_values[index] = RCC_IR_VALUE_NONE;
    }
    ir_pass_rename_block(&rename, 0u);
    if (rename.failed ||
        !ir_pass_compact_values(function, rename.replacements,
                                rename.replacement_count,
                                error, error_size)) {
        rcc_free(rename.replacements);
        rcc_free(rename.current_values);
        if (!rename.failed) goto cleanup;
        ir_pass_error(error, error_size,
                      "mem2reg encountered an undefined promoted value");
        goto cleanup;
    }
    rcc_free(rename.replacements);
    rcc_free(rename.current_values);
    local_stats.promoted_allocas = variable_count;
    if (!rcc_ir_verify_function(function, error, error_size)) goto cleanup;
    result = true;
cleanup:
    rcc_free(blocks);
    rcc_free(predecessors);
    rcc_free(dominators);
    rcc_free(immediate);
    rcc_free(children);
    rcc_free(variables);
    rcc_free(address_variables);
    rcc_free(phis);
    if (result && stats) *stats = local_stats;
    return result;
}
