/*
 * RCC - Target-independent machine intermediate representation
 */

#include "rcc.h"
#include "mir.h"

#include <stdarg.h>

typedef struct {
    const RccMirFunction* function;
    RccMirBlock** blocks;
    bool* predecessors;
    bool* dominators;
    bool* parameters;
    const RccMirInstruction** definitions;
    size_t* definition_blocks;
    size_t* definition_orders;
    size_t* terminator_orders;
    char* error;
    size_t error_size;
} RccMirVerifier;

static RccMirType mir_type(RccMirTypeKind kind, uint16_t bit_width) {
    RccMirType type;
    type.kind = kind;
    type.bit_width = bit_width;
    return type;
}

RccMirType rcc_mir_type_void(void) {
    return mir_type(RCC_MIR_TYPE_VOID, 0u);
}

RccMirType rcc_mir_type_integer(uint16_t bit_width) {
    return mir_type(RCC_MIR_TYPE_INTEGER, bit_width);
}

RccMirType rcc_mir_type_float(uint16_t bit_width) {
    return mir_type(RCC_MIR_TYPE_FLOAT, bit_width);
}

RccMirType rcc_mir_type_pointer(void) {
    return mir_type(RCC_MIR_TYPE_POINTER, 0u);
}

bool rcc_mir_type_equal(RccMirType left, RccMirType right) {
    return left.kind == right.kind && left.bit_width == right.bit_width;
}

static bool mir_type_valid(RccMirType type) {
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

static bool mir_error(RccMirVerifier* verifier, const char* format, ...) {
    va_list arguments;
    if (verifier->error && verifier->error_size != 0u) {
        va_start(arguments, format);
        vsnprintf(verifier->error, verifier->error_size, format, arguments);
        va_end(arguments);
    }
    return false;
}

static void mir_instruction_destroy(RccMirInstruction* instruction) {
    if (!instruction) return;
    rcc_free(instruction->operands);
    rcc_free(instruction->targets);
    rcc_free(instruction->callee);
    rcc_free(instruction);
}

void rcc_mir_function_destroy(RccMirFunction* function) {
    RccMirBlock* block;
    if (!function) return;
    block = function->first_block;
    while (block) {
        RccMirBlock* next_block = block->next;
        RccMirInstruction* instruction = block->first;
        while (instruction) {
            RccMirInstruction* next_instruction = instruction->next;
            mir_instruction_destroy(instruction);
            instruction = next_instruction;
        }
        rcc_free(block->name);
        rcc_free(block);
        block = next_block;
    }
    rcc_free(function->name);
    rcc_free(function->parameter_types);
    rcc_free(function->parameters);
    rcc_free(function->register_types);
    rcc_free(function);
}

static RccMirBlock* mir_block_add(RccMirFunction* function,
                                  const char* name) {
    RccMirBlock* block;
    if (!function || function->block_count >= UINT32_MAX) return NULL;
    block = rcc_alloc(sizeof(*block));
    block->id = (RccMirBlockId)function->block_count;
    block->name = rcc_strdup(name && name[0] ? name : "block");
    block->function = function;
    if (function->last_block) function->last_block->next = block;
    else function->first_block = block;
    function->last_block = block;
    ++function->block_count;
    return block;
}

static RccMirInstruction* mir_append(
    RccMirBlock* block, RccMirOpcode opcode, RccMirType type,
    RccMirVReg definition, const RccMirVReg* operands,
    size_t operand_count, const RccMirBlockId* targets,
    size_t target_count) {
    RccMirInstruction* instruction;
    if (!block || !block->function ||
        (operand_count != 0u && !operands) ||
        (target_count != 0u && !targets)) {
        return NULL;
    }
    instruction = rcc_alloc(sizeof(*instruction));
    instruction->opcode = opcode;
    instruction->type = type;
    instruction->definition = definition;
    instruction->block = block;
    if (operand_count != 0u) {
        instruction->operands = rcc_alloc(
            operand_count * sizeof(*instruction->operands));
        memcpy(instruction->operands, operands,
               operand_count * sizeof(*instruction->operands));
        instruction->operand_count = operand_count;
    }
    if (target_count != 0u) {
        instruction->targets = rcc_alloc(
            target_count * sizeof(*instruction->targets));
        memcpy(instruction->targets, targets,
               target_count * sizeof(*instruction->targets));
        instruction->target_count = target_count;
    }
    instruction->previous = block->last;
    if (block->last) block->last->next = instruction;
    else block->first = instruction;
    block->last = instruction;
    return instruction;
}

static bool mir_is_binary(RccMirOpcode opcode) {
    return opcode >= RCC_MIR_ADD && opcode <= RCC_MIR_ASHR;
}

static bool mir_is_terminator(RccMirOpcode opcode) {
    return opcode == RCC_MIR_BRANCH || opcode == RCC_MIR_COND_BRANCH ||
        opcode == RCC_MIR_RETURN || opcode == RCC_MIR_UNREACHABLE;
}

static bool mir_shape(RccMirVerifier* verifier,
                      const RccMirInstruction* instruction,
                      size_t operands, size_t targets) {
    if (instruction->operand_count != operands ||
        instruction->target_count != targets) {
        return mir_error(verifier,
                         "MIR opcode %u in block %u has invalid shape",
                         (unsigned)instruction->opcode,
                         instruction->block->id);
    }
    return true;
}

static bool mir_register_type(const RccMirVerifier* verifier,
                              RccMirVReg reg, RccMirType* type) {
    if (reg >= verifier->function->register_count) return false;
    if (type) *type = verifier->function->register_types[reg];
    return true;
}

static bool mir_operand_type(RccMirVerifier* verifier,
                             const RccMirInstruction* instruction,
                             size_t index, RccMirType expected) {
    RccMirType actual;
    if (index >= instruction->operand_count ||
        !mir_register_type(verifier, instruction->operands[index],
                           &actual) ||
        !rcc_mir_type_equal(actual, expected)) {
        return mir_error(verifier,
                         "MIR operand %zu in block %u has invalid type",
                         index, instruction->block->id);
    }
    return true;
}

static bool mir_verify_instruction_type(
    RccMirVerifier* verifier, const RccMirInstruction* instruction) {
    RccMirType first;
    RccMirType second;
    RccMirType void_type = rcc_mir_type_void();
    RccMirType i1 = rcc_mir_type_integer(1u);
    if (!mir_type_valid(instruction->type)) {
        return mir_error(verifier, "MIR instruction has invalid type");
    }
    if (mir_is_binary(instruction->opcode)) {
        return mir_shape(verifier, instruction, 2u, 0u) &&
            instruction->type.kind == RCC_MIR_TYPE_INTEGER &&
            mir_operand_type(verifier, instruction, 0u,
                             instruction->type) &&
            mir_operand_type(verifier, instruction, 1u,
                             instruction->type);
    }
    switch (instruction->opcode) {
        case RCC_MIR_CONST_INT:
            return mir_shape(verifier, instruction, 0u, 0u) &&
                instruction->type.kind == RCC_MIR_TYPE_INTEGER;
        case RCC_MIR_ICMP:
            if (!mir_shape(verifier, instruction, 2u, 0u) ||
                !rcc_mir_type_equal(instruction->type, i1) ||
                !mir_register_type(verifier, instruction->operands[0],
                                   &first) ||
                !mir_register_type(verifier, instruction->operands[1],
                                   &second)) {
                return false;
            }
            return rcc_mir_type_equal(first, second) &&
                (first.kind == RCC_MIR_TYPE_INTEGER ||
                 first.kind == RCC_MIR_TYPE_POINTER) &&
                instruction->predicate <= RCC_IR_ICMP_SGE;
        case RCC_MIR_TRUNC:
        case RCC_MIR_ZEXT:
        case RCC_MIR_SEXT:
            if (!mir_shape(verifier, instruction, 1u, 0u) ||
                !mir_register_type(verifier, instruction->operands[0],
                                   &first)) {
                return false;
            }
            if (first.kind != RCC_MIR_TYPE_INTEGER ||
                instruction->type.kind != RCC_MIR_TYPE_INTEGER) {
                return false;
            }
            if (instruction->opcode == RCC_MIR_TRUNC) {
                return instruction->type.bit_width < first.bit_width;
            }
            return instruction->type.bit_width > first.bit_width;
        case RCC_MIR_PTR_TO_INT:
            return mir_shape(verifier, instruction, 1u, 0u) &&
                mir_operand_type(verifier, instruction, 0u,
                                 rcc_mir_type_pointer()) &&
                instruction->type.kind == RCC_MIR_TYPE_INTEGER;
        case RCC_MIR_INT_TO_PTR:
            if (!mir_shape(verifier, instruction, 1u, 0u) ||
                instruction->type.kind != RCC_MIR_TYPE_POINTER ||
                !mir_register_type(verifier, instruction->operands[0],
                                   &first)) {
                return false;
            }
            return first.kind == RCC_MIR_TYPE_INTEGER;
        case RCC_MIR_BITCAST:
            if (!mir_shape(verifier, instruction, 1u, 0u) ||
                !mir_register_type(verifier, instruction->operands[0],
                                   &first)) {
                return false;
            }
            return (first.kind == RCC_MIR_TYPE_INTEGER &&
                    instruction->type.kind == RCC_MIR_TYPE_INTEGER &&
                    first.bit_width == instruction->type.bit_width) ||
                (first.kind == RCC_MIR_TYPE_POINTER &&
                 instruction->type.kind == RCC_MIR_TYPE_POINTER);
        case RCC_MIR_PHI:
            if (instruction->operand_count == 0u ||
                instruction->operand_count != instruction->target_count ||
                instruction->type.kind == RCC_MIR_TYPE_VOID) {
                return mir_error(verifier, "MIR phi has invalid shape");
            }
            for (size_t index = 0u; index < instruction->operand_count;
                 ++index) {
                if (!mir_operand_type(verifier, instruction, index,
                                      instruction->type)) {
                    return false;
                }
            }
            return true;
        case RCC_MIR_SELECT:
            return mir_shape(verifier, instruction, 3u, 0u) &&
                mir_operand_type(verifier, instruction, 0u, i1) &&
                mir_operand_type(verifier, instruction, 1u,
                                 instruction->type) &&
                mir_operand_type(verifier, instruction, 2u,
                                 instruction->type);
        case RCC_MIR_ALLOCA:
            return mir_shape(verifier, instruction, 0u, 0u) &&
                instruction->type.kind == RCC_MIR_TYPE_POINTER &&
                instruction->immediate != 0u;
        case RCC_MIR_LOAD:
            return mir_shape(verifier, instruction, 1u, 0u) &&
                instruction->type.kind != RCC_MIR_TYPE_VOID &&
                mir_operand_type(verifier, instruction, 0u,
                                 rcc_mir_type_pointer());
        case RCC_MIR_STORE:
            return mir_shape(verifier, instruction, 2u, 0u) &&
                rcc_mir_type_equal(instruction->type, void_type) &&
                mir_operand_type(verifier, instruction, 1u,
                                 rcc_mir_type_pointer());
        case RCC_MIR_GEP:
            if (!mir_shape(verifier, instruction, 2u, 0u) ||
                instruction->type.kind != RCC_MIR_TYPE_POINTER ||
                !mir_register_type(verifier, instruction->operands[1],
                                   &second)) {
                return false;
            }
            return mir_operand_type(verifier, instruction, 0u,
                                    rcc_mir_type_pointer()) &&
                second.kind == RCC_MIR_TYPE_INTEGER &&
                instruction->immediate != 0u;
        case RCC_MIR_CALL:
            return instruction->target_count == 0u &&
                instruction->callee && instruction->callee[0];
        case RCC_MIR_BRANCH:
            return mir_shape(verifier, instruction, 0u, 1u) &&
                rcc_mir_type_equal(instruction->type, void_type);
        case RCC_MIR_COND_BRANCH:
            return mir_shape(verifier, instruction, 1u, 2u) &&
                rcc_mir_type_equal(instruction->type, void_type) &&
                mir_operand_type(verifier, instruction, 0u, i1);
        case RCC_MIR_RETURN:
            if (!rcc_mir_type_equal(instruction->type, void_type) ||
                instruction->target_count != 0u) {
                return false;
            }
            if (verifier->function->return_type.kind == RCC_MIR_TYPE_VOID) {
                return mir_shape(verifier, instruction, 0u, 0u);
            }
            return mir_shape(verifier, instruction, 1u, 0u) &&
                mir_operand_type(verifier, instruction, 0u,
                                 verifier->function->return_type);
        case RCC_MIR_UNREACHABLE:
            return mir_shape(verifier, instruction, 0u, 0u) &&
                rcc_mir_type_equal(instruction->type, void_type);
        case RCC_MIR_ADD:
        case RCC_MIR_SUB:
        case RCC_MIR_MUL:
        case RCC_MIR_UDIV:
        case RCC_MIR_SDIV:
        case RCC_MIR_UREM:
        case RCC_MIR_SREM:
        case RCC_MIR_AND:
        case RCC_MIR_OR:
        case RCC_MIR_XOR:
        case RCC_MIR_SHL:
        case RCC_MIR_LSHR:
        case RCC_MIR_ASHR:
            return false;
    }
    return false;
}

static bool mir_collect_definitions(RccMirVerifier* verifier) {
    size_t register_count = verifier->function->register_count;
    size_t block_index = 0u;
    RccMirBlock* block;
    if (register_count != 0u) {
        verifier->parameters = rcc_alloc(register_count *
                                         sizeof(*verifier->parameters));
        verifier->definitions = rcc_alloc(register_count *
                                          sizeof(*verifier->definitions));
        verifier->definition_blocks = rcc_alloc(
            register_count * sizeof(*verifier->definition_blocks));
        verifier->definition_orders = rcc_alloc(
            register_count * sizeof(*verifier->definition_orders));
        for (size_t reg = 0u; reg < register_count; ++reg) {
            verifier->definition_blocks[reg] = SIZE_MAX;
            verifier->definition_orders[reg] = SIZE_MAX;
        }
    }
    for (size_t index = 0u;
         index < verifier->function->parameter_count; ++index) {
        RccMirVReg reg = verifier->function->parameters[index];
        if (reg >= register_count || verifier->parameters[reg] ||
            !rcc_mir_type_equal(
                verifier->function->parameter_types[index],
                verifier->function->register_types[reg])) {
            return mir_error(verifier, "MIR parameter table is invalid");
        }
        verifier->parameters[reg] = true;
    }
    for (block = verifier->function->first_block; block;
         block = block->next, ++block_index) {
        RccMirInstruction* previous = NULL;
        size_t order = 0u;
        if (block_index >= verifier->function->block_count ||
            block->id != block_index || block->function !=
                verifier->function) {
            return mir_error(verifier, "MIR block table is invalid");
        }
        verifier->blocks[block_index] = block;
        for (RccMirInstruction* instruction = block->first; instruction;
             instruction = instruction->next, ++order) {
            RccMirVReg definition = instruction->definition;
            if (instruction->block != block ||
                instruction->previous != previous) {
                return mir_error(verifier,
                                 "MIR instruction list is invalid");
            }
            previous = instruction;
            if (instruction->type.kind == RCC_MIR_TYPE_VOID) {
                if (definition != RCC_MIR_VREG_NONE) {
                    return mir_error(verifier,
                                     "void MIR instruction defines a register");
                }
            } else {
                if (definition >= register_count ||
                    verifier->parameters[definition] ||
                    verifier->definitions[definition] ||
                    !rcc_mir_type_equal(
                        instruction->type,
                        verifier->function->register_types[definition])) {
                    return mir_error(verifier,
                                     "MIR register definition is invalid");
                }
                verifier->definitions[definition] = instruction;
                verifier->definition_blocks[definition] = block_index;
                verifier->definition_orders[definition] = order;
            }
        }
        if (previous != block->last) {
            return mir_error(verifier, "MIR instruction tail is invalid");
        }
    }
    if (block_index != verifier->function->block_count) {
        return mir_error(verifier, "MIR block count is invalid");
    }
    for (size_t reg = 0u; reg < register_count; ++reg) {
        if (!mir_type_valid(verifier->function->register_types[reg]) ||
            (!verifier->parameters[reg] &&
             !verifier->definitions[reg])) {
            return mir_error(verifier, "MIR register %zu has no definition",
                             reg);
        }
    }
    return true;
}

static bool mir_build_cfg(RccMirVerifier* verifier) {
    size_t count = verifier->function->block_count;
    if (count != 0u && count > SIZE_MAX / count) {
        return mir_error(verifier, "MIR CFG is too large");
    }
    verifier->predecessors = rcc_alloc(count * count *
                                       sizeof(*verifier->predecessors));
    verifier->terminator_orders = rcc_alloc(
        count * sizeof(*verifier->terminator_orders));
    for (size_t block = 0u; block < count; ++block) {
        RccMirInstruction* instruction;
        size_t order = 0u;
        bool saw_non_phi = false;
        for (instruction = verifier->blocks[block]->first; instruction;
             instruction = instruction->next, ++order) {
            if (instruction->opcode == RCC_MIR_PHI && saw_non_phi) {
                return mir_error(verifier,
                                 "MIR phi follows a non-phi instruction");
            }
            if (instruction->opcode != RCC_MIR_PHI) saw_non_phi = true;
            if (mir_is_terminator(instruction->opcode) &&
                instruction->next) {
                return mir_error(verifier,
                                 "MIR block has instructions after terminator");
            }
        }
        instruction = verifier->blocks[block]->last;
        if (!instruction || !mir_is_terminator(instruction->opcode)) {
            return mir_error(verifier,
                             "MIR block does not end in a terminator");
        }
        verifier->terminator_orders[block] = order - 1u;
        for (size_t index = 0u; index < instruction->target_count; ++index) {
            RccMirBlockId target = instruction->targets[index];
            if (target >= count) {
                return mir_error(verifier, "MIR branch target is invalid");
            }
            verifier->predecessors[(size_t)target * count + block] = true;
        }
    }
    for (size_t source = 0u; source < count; ++source) {
        if (verifier->predecessors[source]) {
            return mir_error(verifier, "MIR entry has a predecessor");
        }
    }
    return true;
}

static bool mir_compute_dominators(RccMirVerifier* verifier) {
    size_t count = verifier->function->block_count;
    bool* next = rcc_alloc(count * sizeof(*next));
    bool changed = true;
    verifier->dominators = rcc_alloc(count * count *
                                     sizeof(*verifier->dominators));
    verifier->dominators[0] = true;
    for (size_t block = 1u; block < count; ++block) {
        for (size_t candidate = 0u; candidate < count; ++candidate) {
            verifier->dominators[block * count + candidate] = true;
        }
    }
    while (changed) {
        changed = false;
        for (size_t block = 1u; block < count; ++block) {
            bool have_predecessor = false;
            for (size_t candidate = 0u; candidate < count; ++candidate) {
                next[candidate] = true;
            }
            for (size_t predecessor = 0u; predecessor < count;
                 ++predecessor) {
                if (!verifier->predecessors[block * count + predecessor]) {
                    continue;
                }
                if (!have_predecessor) {
                    memcpy(next,
                           &verifier->dominators[predecessor * count],
                           count * sizeof(*next));
                    have_predecessor = true;
                } else {
                    for (size_t candidate = 0u; candidate < count;
                         ++candidate) {
                        next[candidate] = next[candidate] &&
                            verifier->dominators[predecessor * count +
                                                 candidate];
                    }
                }
            }
            if (!have_predecessor) {
                rcc_free(next);
                return mir_error(verifier, "MIR block is unreachable");
            }
            next[block] = true;
            for (size_t candidate = 0u; candidate < count; ++candidate) {
                size_t offset = block * count + candidate;
                if (verifier->dominators[offset] != next[candidate]) {
                    verifier->dominators[offset] = next[candidate];
                    changed = true;
                }
            }
        }
    }
    rcc_free(next);
    return true;
}

static bool mir_definition_dominates(
    RccMirVerifier* verifier, RccMirVReg reg, size_t use_block,
    size_t use_order, bool phi_edge, RccMirBlockId predecessor) {
    size_t definition_block;
    size_t definition_order;
    if (reg >= verifier->function->register_count) {
        return mir_error(verifier, "MIR use names an invalid register");
    }
    if (verifier->parameters[reg]) return true;
    definition_block = verifier->definition_blocks[reg];
    definition_order = verifier->definition_orders[reg];
    if (phi_edge) {
        if (predecessor >= verifier->function->block_count) return false;
        if (definition_block == predecessor) {
            return definition_order <
                verifier->terminator_orders[predecessor];
        }
        if (!verifier->dominators[(size_t)predecessor *
                                  verifier->function->block_count +
                                  definition_block]) {
            return mir_error(verifier,
                             "MIR phi input does not dominate its edge");
        }
        return true;
    }
    if (definition_block == use_block) {
        if (definition_order >= use_order) {
            return mir_error(verifier, "MIR register is used before definition");
        }
        return true;
    }
    if (!verifier->dominators[use_block *
                              verifier->function->block_count +
                              definition_block]) {
        return mir_error(verifier,
                         "MIR definition does not dominate its use");
    }
    return true;
}

static bool mir_verify_uses(RccMirVerifier* verifier) {
    size_t count = verifier->function->block_count;
    for (size_t block = 0u; block < count; ++block) {
        size_t order = 0u;
        for (RccMirInstruction* instruction =
                 verifier->blocks[block]->first;
             instruction; instruction = instruction->next, ++order) {
            if (!mir_verify_instruction_type(verifier, instruction)) {
                return false;
            }
            if (instruction->opcode == RCC_MIR_PHI) {
                size_t predecessor_count = 0u;
                for (size_t predecessor = 0u; predecessor < count;
                     ++predecessor) {
                    if (verifier->predecessors[block * count + predecessor]) {
                        ++predecessor_count;
                    }
                }
                if (predecessor_count != instruction->target_count) {
                    return mir_error(verifier,
                                     "MIR phi inputs do not match CFG");
                }
                for (size_t index = 0u;
                     index < instruction->operand_count; ++index) {
                    RccMirBlockId predecessor = instruction->targets[index];
                    if (predecessor >= count ||
                        !verifier->predecessors[block * count + predecessor]) {
                        return mir_error(verifier,
                                         "MIR phi names a non-predecessor block");
                    }
                    if (!mir_definition_dominates(
                            verifier, instruction->operands[index], block,
                            order, true, predecessor)) {
                        return false;
                    }
                    for (size_t earlier = 0u; earlier < index; ++earlier) {
                        if (instruction->targets[earlier] == predecessor) {
                            return mir_error(verifier,
                                             "MIR phi repeats an input edge");
                        }
                    }
                }
            } else {
                for (size_t index = 0u;
                     index < instruction->operand_count; ++index) {
                    if (!mir_definition_dominates(
                            verifier, instruction->operands[index], block,
                            order, false, RCC_MIR_BLOCK_NONE)) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

static void mir_verifier_release(RccMirVerifier* verifier) {
    rcc_free(verifier->blocks);
    rcc_free(verifier->predecessors);
    rcc_free(verifier->dominators);
    rcc_free(verifier->parameters);
    rcc_free(verifier->definitions);
    rcc_free(verifier->definition_blocks);
    rcc_free(verifier->definition_orders);
    rcc_free(verifier->terminator_orders);
}

bool rcc_mir_verify_function(const RccMirFunction* function, char* error,
                             size_t error_size) {
    RccMirVerifier verifier;
    bool valid = false;
    memset(&verifier, 0, sizeof(verifier));
    verifier.function = function;
    verifier.error = error;
    verifier.error_size = error_size;
    if (error && error_size != 0u) error[0] = '\0';
    if (!function || !function->name || !function->name[0] ||
        !mir_type_valid(function->return_type) ||
        function->block_count == 0u || !function->first_block ||
        !function->last_block ||
        (function->register_count != 0u && !function->register_types) ||
        (function->parameter_count != 0u &&
         (!function->parameters || !function->parameter_types))) {
        mir_error(&verifier, "MIR function header is invalid");
        goto cleanup;
    }
    verifier.blocks = rcc_alloc(function->block_count *
                                sizeof(*verifier.blocks));
    if (!mir_collect_definitions(&verifier) ||
        !mir_build_cfg(&verifier) ||
        !mir_compute_dominators(&verifier) ||
        !mir_verify_uses(&verifier)) {
        goto cleanup;
    }
    valid = true;
cleanup:
    mir_verifier_release(&verifier);
    return valid;
}

static bool mir_type_from_ir(RccIrType ir_type, RccMirType* mir_result) {
    switch (ir_type.kind) {
        case RCC_IR_TYPE_VOID:
            *mir_result = rcc_mir_type_void();
            return true;
        case RCC_IR_TYPE_INTEGER:
            *mir_result = rcc_mir_type_integer(ir_type.bit_width);
            return true;
        case RCC_IR_TYPE_FLOAT:
            *mir_result = rcc_mir_type_float(ir_type.bit_width);
            return true;
        case RCC_IR_TYPE_POINTER:
            *mir_result = rcc_mir_type_pointer();
            return true;
        case RCC_IR_TYPE_AGGREGATE:
            return false;
    }
    return false;
}

static bool mir_opcode_from_ir(RccIrOpcode ir_opcode,
                               RccMirOpcode* mir_opcode) {
    if ((unsigned)ir_opcode > (unsigned)RCC_IR_UNREACHABLE) return false;
    *mir_opcode = (RccMirOpcode)ir_opcode;
    return true;
}

bool rcc_mir_lower_ir(const RccIrFunction* ir_function,
                      RccMirFunction** mir_out,
                      char* error, size_t error_size) {
    RccMirFunction* function;
    const RccIrBlock* ir_block;
    RccMirBlock* mir_block;
    size_t index;
    if (mir_out) *mir_out = NULL;
    if (error && error_size != 0u) error[0] = '\0';
    if (!ir_function ||
        !rcc_ir_verify_function(ir_function, error, error_size)) {
        return false;
    }
    function = rcc_alloc(sizeof(*function));
    function->name = rcc_strdup(ir_function->name);
    if (!mir_type_from_ir(ir_function->return_type,
                          &function->return_type)) {
        rcc_mir_function_destroy(function);
        return false;
    }
    function->parameter_count = ir_function->parameter_count;
    if (function->parameter_count != 0u) {
        function->parameter_types = rcc_alloc(
            function->parameter_count * sizeof(*function->parameter_types));
        function->parameters = rcc_alloc(
            function->parameter_count * sizeof(*function->parameters));
    }
    for (index = 0u; index < function->parameter_count; ++index) {
        if (!mir_type_from_ir(ir_function->parameter_types[index],
                              &function->parameter_types[index])) {
            rcc_mir_function_destroy(function);
            return false;
        }
        function->parameters[index] = ir_function->parameters[index];
    }
    function->register_count = ir_function->value_count;
    if (function->register_count != 0u) {
        function->register_types = rcc_alloc(
            function->register_count * sizeof(*function->register_types));
    }
    for (index = 0u; index < function->register_count; ++index) {
        if (!mir_type_from_ir(ir_function->value_types[index],
                              &function->register_types[index])) {
            rcc_mir_function_destroy(function);
            return false;
        }
    }
    for (ir_block = ir_function->first_block; ir_block;
         ir_block = ir_block->next) {
        if (!mir_block_add(function, ir_block->name)) {
            rcc_mir_function_destroy(function);
            return false;
        }
    }
    ir_block = ir_function->first_block;
    mir_block = function->first_block;
    while (ir_block && mir_block) {
        const RccIrInstruction* ir_instruction;
        for (ir_instruction = ir_block->first; ir_instruction;
             ir_instruction = ir_instruction->next) {
            RccMirOpcode opcode;
            RccMirType type;
            RccMirInstruction* instruction;
            if (!mir_opcode_from_ir(ir_instruction->opcode, &opcode) ||
                !mir_type_from_ir(ir_instruction->type, &type)) {
                rcc_mir_function_destroy(function);
                return false;
            }
            instruction = mir_append(
                mir_block, opcode, type, ir_instruction->result,
                ir_instruction->operands, ir_instruction->operand_count,
                ir_instruction->targets, ir_instruction->target_count);
            if (!instruction) {
                rcc_mir_function_destroy(function);
                return false;
            }
            instruction->immediate = ir_instruction->immediate;
            instruction->predicate = ir_instruction->predicate;
            if (ir_instruction->callee) {
                instruction->callee = rcc_strdup(ir_instruction->callee);
            }
        }
        ir_block = ir_block->next;
        mir_block = mir_block->next;
    }
    if (!rcc_mir_verify_function(function, error, error_size)) {
        rcc_mir_function_destroy(function);
        return false;
    }
    if (mir_out) *mir_out = function;
    else rcc_mir_function_destroy(function);
    return true;
}
