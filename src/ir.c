/*
 * RCC - Target-independent typed SSA intermediate representation
 */

#include "rcc.h"
#include "ir.h"

#include <stdarg.h>

typedef struct {
    const RccIrFunction* function;
    char* error;
    size_t error_size;
    RccIrBlock** blocks;
    bool* predecessors;
    bool* dominators;
    bool* parameters;
    const RccIrInstruction** definitions;
    size_t* definition_blocks;
    size_t* definition_orders;
    size_t* terminator_orders;
} RccIrVerifier;

static RccIrType ir_type(RccIrTypeKind kind, uint16_t bit_width,
                         uint16_t lanes, uint32_t address_space,
                         uint32_t aggregate_id) {
    RccIrType type;
    type.kind = kind;
    type.bit_width = bit_width;
    type.lanes = lanes;
    type.address_space = address_space;
    type.aggregate_id = aggregate_id;
    return type;
}

RccIrType rcc_ir_type_void(void) {
    return ir_type(RCC_IR_TYPE_VOID, 0u, 0u, 0u, 0u);
}

RccIrType rcc_ir_type_integer(uint16_t bit_width) {
    return ir_type(RCC_IR_TYPE_INTEGER, bit_width, 1u, 0u, 0u);
}

RccIrType rcc_ir_type_float(uint16_t bit_width) {
    return ir_type(RCC_IR_TYPE_FLOAT, bit_width, 1u, 0u, 0u);
}

RccIrType rcc_ir_type_pointer(uint32_t address_space) {
    return ir_type(RCC_IR_TYPE_POINTER, 0u, 1u, address_space, 0u);
}

RccIrType rcc_ir_type_aggregate(uint32_t aggregate_id) {
    return ir_type(RCC_IR_TYPE_AGGREGATE, 0u, 1u, 0u, aggregate_id);
}

bool rcc_ir_type_equal(RccIrType left, RccIrType right) {
    return left.kind == right.kind && left.bit_width == right.bit_width &&
        left.lanes == right.lanes &&
        left.address_space == right.address_space &&
        left.aggregate_id == right.aggregate_id;
}

static bool ir_type_well_formed(RccIrType type) {
    switch (type.kind) {
        case RCC_IR_TYPE_VOID:
            return type.bit_width == 0u && type.lanes == 0u &&
                type.address_space == 0u && type.aggregate_id == 0u;
        case RCC_IR_TYPE_INTEGER:
            return (type.bit_width == 1u || type.bit_width == 8u ||
                    type.bit_width == 16u || type.bit_width == 32u ||
                    type.bit_width == 64u) &&
                type.lanes == 1u && type.address_space == 0u &&
                type.aggregate_id == 0u;
        case RCC_IR_TYPE_FLOAT:
            return (type.bit_width == 32u || type.bit_width == 64u) &&
                type.lanes == 1u && type.address_space == 0u &&
                type.aggregate_id == 0u;
        case RCC_IR_TYPE_POINTER:
            return type.bit_width == 0u && type.lanes == 1u &&
                type.aggregate_id == 0u;
        case RCC_IR_TYPE_AGGREGATE:
            return type.bit_width == 0u && type.lanes == 1u &&
                type.address_space == 0u;
    }
    return false;
}

RccIrModule* rcc_ir_module_create(void) {
    return rcc_alloc(sizeof(RccIrModule));
}

static void ir_instruction_destroy(RccIrInstruction* instruction) {
    if (!instruction) return;
    rcc_free(instruction->operands);
    rcc_free(instruction->targets);
    rcc_free(instruction->callee);
    rcc_free(instruction);
}

void rcc_ir_module_destroy(RccIrModule* module) {
    RccIrFunction* function;
    if (!module) return;
    function = module->first_function;
    while (function) {
        RccIrFunction* next_function = function->next;
        RccIrBlock* block = function->first_block;
        while (block) {
            RccIrBlock* next_block = block->next;
            RccIrInstruction* instruction = block->first;
            while (instruction) {
                RccIrInstruction* next_instruction = instruction->next;
                ir_instruction_destroy(instruction);
                instruction = next_instruction;
            }
            rcc_free(block->name);
            rcc_free(block);
            block = next_block;
        }
        rcc_free(function->name);
        rcc_free(function->parameter_types);
        rcc_free(function->parameters);
        rcc_free(function->value_types);
        rcc_free(function);
        function = next_function;
    }
    rcc_free(module);
}

static void ir_reserve_values(RccIrFunction* function, size_t required) {
    size_t capacity;
    if (required <= function->value_capacity) return;
    capacity = function->value_capacity ? function->value_capacity : 16u;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2u) {
            rcc_fatal("IR value table is too large");
        }
        capacity *= 2u;
    }
    if (capacity > SIZE_MAX / sizeof(*function->value_types)) {
        rcc_fatal("IR value table is too large");
    }
    function->value_types = rcc_realloc(
        function->value_types, capacity * sizeof(*function->value_types));
    function->value_capacity = capacity;
}

RccIrFunction* rcc_ir_function_add(RccIrModule* module, const char* name,
                                  RccIrType return_type,
                                  const RccIrType* parameter_types,
                                  size_t parameter_count) {
    RccIrFunction* function;
    if (!module || !name || !name[0] ||
        (parameter_count != 0u && !parameter_types)) {
        return NULL;
    }
    function = rcc_alloc(sizeof(*function));
    function->name = rcc_strdup(name);
    function->return_type = return_type;
    function->parameter_count = parameter_count;
    function->module = module;
    if (parameter_count != 0u) {
        if (parameter_count > SIZE_MAX / sizeof(*function->parameter_types) ||
            parameter_count > SIZE_MAX / sizeof(*function->parameters)) {
            rcc_fatal("IR parameter table is too large");
        }
        function->parameter_types = rcc_alloc(
            parameter_count * sizeof(*function->parameter_types));
        function->parameters = rcc_alloc(
            parameter_count * sizeof(*function->parameters));
        memcpy(function->parameter_types, parameter_types,
               parameter_count * sizeof(*function->parameter_types));
        ir_reserve_values(function, parameter_count);
        for (size_t index = 0u; index < parameter_count; ++index) {
            function->parameters[index] = (RccIrValue)index;
            function->value_types[index] = parameter_types[index];
        }
        function->value_count = parameter_count;
    }
    if (module->last_function) {
        module->last_function->next = function;
    } else {
        module->first_function = function;
    }
    module->last_function = function;
    ++module->function_count;
    return function;
}

RccIrBlock* rcc_ir_block_add(RccIrFunction* function, const char* name) {
    RccIrBlock* block;
    if (!function || function->block_count >= UINT32_MAX) return NULL;
    block = rcc_alloc(sizeof(*block));
    block->id = (RccIrBlockId)function->block_count;
    block->name = rcc_strdup(name && name[0] ? name : "block");
    block->function = function;
    if (function->last_block) {
        function->last_block->next = block;
    } else {
        function->first_block = block;
    }
    function->last_block = block;
    ++function->block_count;
    return block;
}

RccIrInstruction* rcc_ir_append(RccIrBlock* block, RccIrOpcode opcode,
                                RccIrType result_type,
                                const RccIrValue* operands,
                                size_t operand_count,
                                const RccIrBlockId* targets,
                                size_t target_count) {
    RccIrInstruction* instruction;
    RccIrFunction* function;
    if (!block || !block->function ||
        (operand_count != 0u && !operands) ||
        (target_count != 0u && !targets)) {
        return NULL;
    }
    function = block->function;
    instruction = rcc_alloc(sizeof(*instruction));
    instruction->opcode = opcode;
    instruction->type = result_type;
    instruction->result = RCC_IR_VALUE_NONE;
    instruction->block = block;
    if (operand_count != 0u) {
        if (operand_count > SIZE_MAX / sizeof(*instruction->operands)) {
            rcc_fatal("IR operand table is too large");
        }
        instruction->operands = rcc_alloc(
            operand_count * sizeof(*instruction->operands));
        memcpy(instruction->operands, operands,
               operand_count * sizeof(*instruction->operands));
        instruction->operand_count = operand_count;
    }
    if (target_count != 0u) {
        if (target_count > SIZE_MAX / sizeof(*instruction->targets)) {
            rcc_fatal("IR target table is too large");
        }
        instruction->targets = rcc_alloc(
            target_count * sizeof(*instruction->targets));
        memcpy(instruction->targets, targets,
               target_count * sizeof(*instruction->targets));
        instruction->target_count = target_count;
    }
    if (result_type.kind != RCC_IR_TYPE_VOID) {
        if (function->value_count >= UINT32_MAX) {
            ir_instruction_destroy(instruction);
            return NULL;
        }
        ir_reserve_values(function, function->value_count + 1u);
        instruction->result = (RccIrValue)function->value_count;
        function->value_types[function->value_count++] = result_type;
    }
    instruction->previous = block->last;
    if (block->last) {
        block->last->next = instruction;
    } else {
        block->first = instruction;
    }
    block->last = instruction;
    return instruction;
}

void rcc_ir_set_immediate(RccIrInstruction* instruction, uint64_t immediate) {
    if (instruction) instruction->immediate = immediate;
}

void rcc_ir_set_predicate(RccIrInstruction* instruction,
                          RccIrIntPredicate predicate) {
    if (instruction) instruction->predicate = predicate;
}

void rcc_ir_set_callee(RccIrInstruction* instruction, const char* callee) {
    if (!instruction) return;
    rcc_free(instruction->callee);
    instruction->callee = callee ? rcc_strdup(callee) : NULL;
}

static bool ir_verify_error(RccIrVerifier* verifier, const char* format, ...) {
    va_list args;
    if (verifier && verifier->error && verifier->error_size != 0u) {
        va_start(args, format);
        vsnprintf(verifier->error, verifier->error_size, format, args);
        va_end(args);
        verifier->error[verifier->error_size - 1u] = '\0';
    }
    return false;
}

static const char* ir_opcode_name(RccIrOpcode opcode) {
    switch (opcode) {
        case RCC_IR_CONST_INT: return "const";
        case RCC_IR_ADD: return "add";
        case RCC_IR_SUB: return "sub";
        case RCC_IR_MUL: return "mul";
        case RCC_IR_UDIV: return "udiv";
        case RCC_IR_SDIV: return "sdiv";
        case RCC_IR_UREM: return "urem";
        case RCC_IR_SREM: return "srem";
        case RCC_IR_AND: return "and";
        case RCC_IR_OR: return "or";
        case RCC_IR_XOR: return "xor";
        case RCC_IR_SHL: return "shl";
        case RCC_IR_LSHR: return "lshr";
        case RCC_IR_ASHR: return "ashr";
        case RCC_IR_ICMP: return "icmp";
        case RCC_IR_TRUNC: return "trunc";
        case RCC_IR_ZEXT: return "zext";
        case RCC_IR_SEXT: return "sext";
        case RCC_IR_PTR_TO_INT: return "ptrtoint";
        case RCC_IR_INT_TO_PTR: return "inttoptr";
        case RCC_IR_BITCAST: return "bitcast";
        case RCC_IR_PHI: return "phi";
        case RCC_IR_SELECT: return "select";
        case RCC_IR_ALLOCA: return "alloca";
        case RCC_IR_LOAD: return "load";
        case RCC_IR_STORE: return "store";
        case RCC_IR_GEP: return "gep";
        case RCC_IR_SYMBOL_ADDRESS: return "symbol_address";
        case RCC_IR_CALL: return "call";
        case RCC_IR_BRANCH: return "branch";
        case RCC_IR_COND_BRANCH: return "cond_branch";
        case RCC_IR_RETURN: return "return";
        case RCC_IR_UNREACHABLE: return "unreachable";
    }
    return "invalid";
}

static bool ir_is_terminator(RccIrOpcode opcode) {
    return opcode == RCC_IR_BRANCH || opcode == RCC_IR_COND_BRANCH ||
        opcode == RCC_IR_RETURN || opcode == RCC_IR_UNREACHABLE;
}

static bool ir_is_binary_integer(RccIrOpcode opcode) {
    return opcode >= RCC_IR_ADD && opcode <= RCC_IR_ASHR;
}

static bool ir_require_shape(RccIrVerifier* verifier,
                             const RccIrInstruction* instruction,
                             size_t operands, size_t targets) {
    if (instruction->operand_count != operands ||
        instruction->target_count != targets) {
        return ir_verify_error(
            verifier, "%s in block %u has invalid operand/target count",
            ir_opcode_name(instruction->opcode), instruction->block->id);
    }
    return true;
}

static bool ir_value_type(RccIrVerifier* verifier, RccIrValue value,
                          RccIrType* type) {
    if (value == RCC_IR_VALUE_NONE ||
        value >= verifier->function->value_count) {
        return ir_verify_error(verifier, "use of invalid SSA value %u",
                               value);
    }
    *type = verifier->function->value_types[value];
    return true;
}

static bool ir_operand_has_type(RccIrVerifier* verifier,
                                const RccIrInstruction* instruction,
                                size_t index, RccIrType expected) {
    RccIrType actual = {0};
    if (index >= instruction->operand_count ||
        !ir_value_type(verifier, instruction->operands[index], &actual)) {
        return false;
    }
    if (!rcc_ir_type_equal(actual, expected)) {
        return ir_verify_error(
            verifier, "%s operand %zu in block %u has mismatched type",
            ir_opcode_name(instruction->opcode), index,
            instruction->block->id);
    }
    return true;
}

static bool ir_verify_instruction_types(
    RccIrVerifier* verifier, const RccIrInstruction* instruction) {
    RccIrType first = {0};
    RccIrType second = {0};
    RccIrType void_type = rcc_ir_type_void();
    RccIrType i1_type = rcc_ir_type_integer(1u);
    if (!ir_type_well_formed(instruction->type)) {
        return ir_verify_error(verifier, "%s in block %u has invalid type",
                               ir_opcode_name(instruction->opcode),
                               instruction->block->id);
    }
    if (ir_is_binary_integer(instruction->opcode)) {
        if (!ir_require_shape(verifier, instruction, 2u, 0u)) return false;
        if (instruction->type.kind != RCC_IR_TYPE_INTEGER) {
            return ir_verify_error(verifier,
                                   "%s requires an integer result type",
                                   ir_opcode_name(instruction->opcode));
        }
        return ir_operand_has_type(verifier, instruction, 0u,
                                   instruction->type) &&
            ir_operand_has_type(verifier, instruction, 1u,
                                instruction->type);
    }
    switch (instruction->opcode) {
        case RCC_IR_CONST_INT:
            if (!ir_require_shape(verifier, instruction, 0u, 0u)) {
                return false;
            }
            if (instruction->type.kind != RCC_IR_TYPE_INTEGER) {
                return ir_verify_error(verifier,
                                       "const requires integer result type");
            }
            return true;
        case RCC_IR_ICMP:
            if (!ir_require_shape(verifier, instruction, 2u, 0u) ||
                !rcc_ir_type_equal(instruction->type, i1_type) ||
                !ir_value_type(verifier, instruction->operands[0], &first) ||
                !ir_value_type(verifier, instruction->operands[1], &second)) {
                return false;
            }
            if (!rcc_ir_type_equal(first, second) ||
                (first.kind != RCC_IR_TYPE_INTEGER &&
                 first.kind != RCC_IR_TYPE_POINTER) ||
                instruction->predicate > RCC_IR_ICMP_SGE) {
                return ir_verify_error(verifier,
                                       "icmp has incompatible operands");
            }
            return true;
        case RCC_IR_TRUNC:
        case RCC_IR_ZEXT:
        case RCC_IR_SEXT:
            if (!ir_require_shape(verifier, instruction, 1u, 0u) ||
                !ir_value_type(verifier, instruction->operands[0], &first)) {
                return false;
            }
            if (first.kind != RCC_IR_TYPE_INTEGER ||
                instruction->type.kind != RCC_IR_TYPE_INTEGER ||
                (instruction->opcode == RCC_IR_TRUNC &&
                 instruction->type.bit_width >= first.bit_width) ||
                (instruction->opcode != RCC_IR_TRUNC &&
                 instruction->type.bit_width <= first.bit_width)) {
                return ir_verify_error(verifier,
                                       "%s has invalid integer widths",
                                       ir_opcode_name(instruction->opcode));
            }
            return true;
        case RCC_IR_PTR_TO_INT:
        case RCC_IR_INT_TO_PTR:
            if (!ir_require_shape(verifier, instruction, 1u, 0u) ||
                !ir_value_type(verifier, instruction->operands[0], &first)) {
                return false;
            }
            if ((instruction->opcode == RCC_IR_PTR_TO_INT &&
                 (first.kind != RCC_IR_TYPE_POINTER ||
                  instruction->type.kind != RCC_IR_TYPE_INTEGER)) ||
                (instruction->opcode == RCC_IR_INT_TO_PTR &&
                 (first.kind != RCC_IR_TYPE_INTEGER ||
                  instruction->type.kind != RCC_IR_TYPE_POINTER))) {
                return ir_verify_error(verifier,
                                       "%s has incompatible types",
                                       ir_opcode_name(instruction->opcode));
            }
            return true;
        case RCC_IR_BITCAST:
            if (!ir_require_shape(verifier, instruction, 1u, 0u) ||
                !ir_value_type(verifier, instruction->operands[0], &first)) {
                return false;
            }
            if ((first.kind == RCC_IR_TYPE_INTEGER &&
                 instruction->type.kind == RCC_IR_TYPE_INTEGER &&
                 first.bit_width == instruction->type.bit_width) ||
                (first.kind == RCC_IR_TYPE_POINTER &&
                 instruction->type.kind == RCC_IR_TYPE_POINTER)) {
                return true;
            }
            return ir_verify_error(verifier,
                                   "bitcast has incompatible types");
        case RCC_IR_PHI:
            if (instruction->type.kind == RCC_IR_TYPE_VOID ||
                instruction->operand_count == 0u ||
                instruction->operand_count != instruction->target_count) {
                return ir_verify_error(verifier,
                                       "phi has invalid incoming edges");
            }
            for (size_t index = 0u; index < instruction->operand_count;
                 ++index) {
                if (!ir_operand_has_type(verifier, instruction, index,
                                         instruction->type)) {
                    return false;
                }
            }
            return true;
        case RCC_IR_SELECT:
            if (!ir_require_shape(verifier, instruction, 3u, 0u)) {
                return false;
            }
            if (instruction->type.kind == RCC_IR_TYPE_VOID) {
                return ir_verify_error(verifier,
                                       "select requires a result type");
            }
            return ir_operand_has_type(verifier, instruction, 0u, i1_type) &&
                ir_operand_has_type(verifier, instruction, 1u,
                                    instruction->type) &&
                ir_operand_has_type(verifier, instruction, 2u,
                                    instruction->type);
        case RCC_IR_ALLOCA:
            if (!ir_require_shape(verifier, instruction, 0u, 0u)) {
                return false;
            }
            if (instruction->type.kind != RCC_IR_TYPE_POINTER ||
                instruction->immediate == 0u) {
                return ir_verify_error(verifier,
                                       "alloca requires pointer type and size");
            }
            return true;
        case RCC_IR_LOAD:
            if (!ir_require_shape(verifier, instruction, 1u, 0u) ||
                instruction->type.kind == RCC_IR_TYPE_VOID ||
                !ir_value_type(verifier, instruction->operands[0], &first)) {
                return false;
            }
            if (first.kind != RCC_IR_TYPE_POINTER) {
                return ir_verify_error(verifier,
                                       "load address is not a pointer");
            }
            return true;
        case RCC_IR_STORE:
            if (!ir_require_shape(verifier, instruction, 2u, 0u) ||
                !rcc_ir_type_equal(instruction->type, void_type) ||
                !ir_value_type(verifier, instruction->operands[0], &first) ||
                !ir_value_type(verifier, instruction->operands[1], &second)) {
                return false;
            }
            if (first.kind == RCC_IR_TYPE_VOID ||
                second.kind != RCC_IR_TYPE_POINTER) {
                return ir_verify_error(verifier,
                                       "store has incompatible operands");
            }
            return true;
        case RCC_IR_GEP:
            if (!ir_require_shape(verifier, instruction, 2u, 0u) ||
                instruction->type.kind != RCC_IR_TYPE_POINTER ||
                !ir_value_type(verifier, instruction->operands[0], &first) ||
                !ir_value_type(verifier, instruction->operands[1], &second)) {
                return false;
            }
            if (first.kind != RCC_IR_TYPE_POINTER ||
                second.kind != RCC_IR_TYPE_INTEGER ||
                instruction->immediate == 0u) {
                return ir_verify_error(verifier,
                                       "gep has incompatible operands");
            }
            return true;
        case RCC_IR_SYMBOL_ADDRESS:
            if (!ir_require_shape(verifier, instruction, 0u, 0u) ||
                instruction->type.kind != RCC_IR_TYPE_POINTER ||
                !instruction->callee || !instruction->callee[0]) {
                return ir_verify_error(
                    verifier,
                    "symbol_address requires pointer type and symbol");
            }
            return true;
        case RCC_IR_CALL:
            if (instruction->target_count != 0u || !instruction->callee ||
                !instruction->callee[0]) {
                return ir_verify_error(verifier,
                                       "call requires a callee symbol");
            }
            for (size_t index = 0u; index < instruction->operand_count;
                 ++index) {
                if (!ir_value_type(verifier, instruction->operands[index],
                                   &first)) {
                    return false;
                }
            }
            return true;
        case RCC_IR_BRANCH:
            if (!ir_require_shape(verifier, instruction, 0u, 1u) ||
                !rcc_ir_type_equal(instruction->type, void_type)) {
                return ir_verify_error(verifier,
                                       "branch has invalid type or shape");
            }
            return true;
        case RCC_IR_COND_BRANCH:
            if (!ir_require_shape(verifier, instruction, 1u, 2u) ||
                !rcc_ir_type_equal(instruction->type, void_type)) {
                return ir_verify_error(
                    verifier, "conditional branch has invalid type or shape");
            }
            return ir_operand_has_type(verifier, instruction, 0u, i1_type);
        case RCC_IR_RETURN:
            if (!rcc_ir_type_equal(instruction->type, void_type) ||
                instruction->target_count != 0u) {
                return ir_verify_error(verifier,
                                       "return has invalid result or target");
            }
            if (verifier->function->return_type.kind == RCC_IR_TYPE_VOID) {
                return ir_require_shape(verifier, instruction, 0u, 0u);
            }
            return ir_require_shape(verifier, instruction, 1u, 0u) &&
                ir_operand_has_type(verifier, instruction, 0u,
                                    verifier->function->return_type);
        case RCC_IR_UNREACHABLE:
            if (!ir_require_shape(verifier, instruction, 0u, 0u) ||
                !rcc_ir_type_equal(instruction->type, void_type)) {
                return ir_verify_error(verifier,
                                       "unreachable has invalid type or shape");
            }
            return true;
        case RCC_IR_ADD:
        case RCC_IR_SUB:
        case RCC_IR_MUL:
        case RCC_IR_UDIV:
        case RCC_IR_SDIV:
        case RCC_IR_UREM:
        case RCC_IR_SREM:
        case RCC_IR_AND:
        case RCC_IR_OR:
        case RCC_IR_XOR:
        case RCC_IR_SHL:
        case RCC_IR_LSHR:
        case RCC_IR_ASHR:
            return false;
    }
    return ir_verify_error(verifier, "unknown IR opcode");
}

static bool ir_collect_blocks(RccIrVerifier* verifier) {
    size_t index = 0u;
    const RccIrFunction* function = verifier->function;
    if (function->block_count == 0u || !function->first_block) {
        return ir_verify_error(verifier, "function has no entry block");
    }
    if (function->block_count > SIZE_MAX / sizeof(*verifier->blocks)) {
        return ir_verify_error(verifier, "function has too many blocks");
    }
    verifier->blocks = rcc_alloc(function->block_count *
                                 sizeof(*verifier->blocks));
    for (RccIrBlock* block = function->first_block; block;
         block = block->next) {
        if (index >= function->block_count || block->id != index ||
            block->function != function || !block->name || !block->name[0]) {
            return ir_verify_error(verifier,
                                   "block identifiers are not canonical");
        }
        verifier->blocks[index++] = block;
    }
    if (index != function->block_count ||
        function->last_block != verifier->blocks[index - 1u]) {
        return ir_verify_error(verifier, "block list is inconsistent");
    }
    return true;
}

static bool ir_collect_values(RccIrVerifier* verifier) {
    const RccIrFunction* function = verifier->function;
    size_t value_count = function->value_count;
    if (value_count > function->value_capacity ||
        (value_count != 0u && !function->value_types)) {
        return ir_verify_error(verifier, "SSA value table is inconsistent");
    }
    if (value_count != 0u) {
        verifier->parameters = rcc_alloc(value_count *
                                         sizeof(*verifier->parameters));
        verifier->definitions = rcc_alloc(value_count *
                                          sizeof(*verifier->definitions));
        verifier->definition_blocks = rcc_alloc(
            value_count * sizeof(*verifier->definition_blocks));
        verifier->definition_orders = rcc_alloc(
            value_count * sizeof(*verifier->definition_orders));
        for (size_t value = 0u; value < value_count; ++value) {
            verifier->definition_blocks[value] = SIZE_MAX;
            verifier->definition_orders[value] = SIZE_MAX;
        }
    }
    if (function->parameter_count > value_count ||
        (function->parameter_count != 0u &&
         (!function->parameters || !function->parameter_types))) {
        return ir_verify_error(verifier, "parameter table is inconsistent");
    }
    for (size_t index = 0u; index < function->parameter_count; ++index) {
        RccIrValue value = function->parameters[index];
        if (value >= value_count || verifier->parameters[value] ||
            !ir_type_well_formed(function->parameter_types[index]) ||
            function->parameter_types[index].kind == RCC_IR_TYPE_VOID ||
            !rcc_ir_type_equal(function->parameter_types[index],
                               function->value_types[value])) {
            return ir_verify_error(verifier,
                                   "parameter %zu has invalid SSA type",
                                   index);
        }
        verifier->parameters[value] = true;
    }
    for (size_t block_index = 0u; block_index < function->block_count;
         ++block_index) {
        size_t order = 0u;
        RccIrBlock* block = verifier->blocks[block_index];
        RccIrInstruction* previous = NULL;
        for (RccIrInstruction* instruction = block->first; instruction;
             instruction = instruction->next, ++order) {
            if (instruction->block != block ||
                instruction->previous != previous ||
                (instruction->operand_count != 0u &&
                 !instruction->operands) ||
                (instruction->target_count != 0u &&
                 !instruction->targets)) {
                return ir_verify_error(verifier,
                                       "instruction list is inconsistent");
            }
            previous = instruction;
            if (instruction->type.kind == RCC_IR_TYPE_VOID) {
                if (instruction->result != RCC_IR_VALUE_NONE) {
                    return ir_verify_error(verifier,
                                           "void instruction defines a value");
                }
            } else {
                RccIrValue value = instruction->result;
                if (value >= value_count || verifier->parameters[value] ||
                    verifier->definitions[value] ||
                    !rcc_ir_type_equal(function->value_types[value],
                                       instruction->type)) {
                    return ir_verify_error(verifier,
                                           "invalid or duplicate SSA value");
                }
                verifier->definitions[value] = instruction;
                verifier->definition_blocks[value] = block_index;
                verifier->definition_orders[value] = order;
            }
        }
        if (previous != block->last) {
            return ir_verify_error(verifier,
                                   "instruction tail is inconsistent");
        }
    }
    for (size_t value = 0u; value < value_count; ++value) {
        if (!ir_type_well_formed(function->value_types[value]) ||
            (!verifier->parameters[value] &&
             !verifier->definitions[value])) {
            return ir_verify_error(verifier,
                                   "SSA value %zu has no definition", value);
        }
    }
    return true;
}

static bool ir_build_cfg(RccIrVerifier* verifier) {
    const RccIrFunction* function = verifier->function;
    size_t count = function->block_count;
    bool* reachable;
    RccIrBlockId* queue;
    size_t head = 0u;
    size_t tail = 0u;
    if (count > SIZE_MAX / count ||
        count * count > SIZE_MAX / sizeof(*verifier->predecessors)) {
        return ir_verify_error(verifier, "CFG is too large");
    }
    verifier->predecessors = rcc_alloc(count * count *
                                       sizeof(*verifier->predecessors));
    verifier->terminator_orders = rcc_alloc(
        count * sizeof(*verifier->terminator_orders));
    for (size_t block_index = 0u; block_index < count; ++block_index) {
        RccIrBlock* block = verifier->blocks[block_index];
        RccIrInstruction* terminator = block->last;
        size_t order = 0u;
        bool saw_non_phi = false;
        if (!terminator || !ir_is_terminator(terminator->opcode)) {
            return ir_verify_error(verifier,
                                   "block %zu ('%s') has no terminator",
                                   block_index,
                                   block->name ? block->name : "");
        }
        for (RccIrInstruction* instruction = block->first; instruction;
             instruction = instruction->next, ++order) {
            if (ir_is_terminator(instruction->opcode) && instruction->next) {
                return ir_verify_error(verifier,
                                       "block %zu has instructions after terminator",
                                       block_index);
            }
            if (instruction->opcode == RCC_IR_PHI) {
                if (saw_non_phi) {
                    return ir_verify_error(verifier,
                                           "phi is not at start of block %zu",
                                           block_index);
                }
            } else {
                saw_non_phi = true;
            }
        }
        verifier->terminator_orders[block_index] = order - 1u;
        for (size_t target_index = 0u;
             target_index < terminator->target_count; ++target_index) {
            RccIrBlockId target = terminator->targets[target_index];
            if (target >= count) {
                return ir_verify_error(verifier,
                                       "block %zu branches to invalid block %u",
                                       block_index, target);
            }
            verifier->predecessors[(size_t)target * count + block_index] =
                true;
        }
    }
    for (size_t source = 0u; source < count; ++source) {
        if (verifier->predecessors[source]) {
            return ir_verify_error(verifier,
                                   "entry block has a predecessor");
        }
    }
    reachable = rcc_alloc(count * sizeof(*reachable));
    queue = rcc_alloc(count * sizeof(*queue));
    reachable[0] = true;
    queue[tail++] = 0u;
    while (head < tail) {
        RccIrBlockId block_id = queue[head++];
        RccIrInstruction* terminator = verifier->blocks[block_id]->last;
        for (size_t index = 0u; index < terminator->target_count; ++index) {
            RccIrBlockId target = terminator->targets[index];
            if (!reachable[target]) {
                reachable[target] = true;
                queue[tail++] = target;
            }
        }
    }
    for (size_t block = 0u; block < count; ++block) {
        if (!reachable[block]) {
            rcc_free(reachable);
            rcc_free(queue);
            return ir_verify_error(verifier, "block %zu is unreachable",
                                   block);
        }
    }
    rcc_free(reachable);
    rcc_free(queue);
    return true;
}

static bool ir_compute_dominators(RccIrVerifier* verifier) {
    size_t count = verifier->function->block_count;
    bool changed = true;
    bool* next = rcc_alloc(count * sizeof(*next));
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
                return ir_verify_error(verifier,
                                       "block %zu has no predecessor", block);
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

static bool ir_value_dominates_use(RccIrVerifier* verifier,
                                   RccIrValue value, size_t use_block,
                                   size_t use_order) {
    size_t definition_block;
    if (value >= verifier->function->value_count) {
        return ir_verify_error(verifier, "use of invalid SSA value %u",
                               value);
    }
    if (verifier->parameters[value]) return true;
    definition_block = verifier->definition_blocks[value];
    if (definition_block == use_block) {
        if (verifier->definition_orders[value] >= use_order) {
            return ir_verify_error(verifier,
                                   "SSA value %u is used before definition",
                                   value);
        }
        return true;
    }
    if (!verifier->dominators[use_block * verifier->function->block_count +
                              definition_block]) {
        return ir_verify_error(verifier,
                               "SSA value %u does not dominate its use",
                               value);
    }
    return true;
}

static bool ir_value_dominates_edge(RccIrVerifier* verifier,
                                    RccIrValue value,
                                    RccIrBlockId predecessor) {
    size_t definition_block;
    if (value >= verifier->function->value_count ||
        predecessor >= verifier->function->block_count) {
        return ir_verify_error(verifier, "phi has invalid incoming value");
    }
    if (verifier->parameters[value]) return true;
    definition_block = verifier->definition_blocks[value];
    if (definition_block == predecessor) {
        if (verifier->definition_orders[value] >=
            verifier->terminator_orders[predecessor]) {
            return ir_verify_error(verifier,
                                   "phi incoming value is not available on edge");
        }
        return true;
    }
    if (!verifier->dominators[(size_t)predecessor *
                                  verifier->function->block_count +
                              definition_block]) {
        return ir_verify_error(verifier,
                               "phi incoming value does not dominate edge");
    }
    return true;
}

static bool ir_verify_phi_predecessors(
    RccIrVerifier* verifier, const RccIrInstruction* instruction) {
    size_t count = verifier->function->block_count;
    size_t block = instruction->block->id;
    size_t predecessor_count = 0u;
    for (size_t predecessor = 0u; predecessor < count; ++predecessor) {
        if (verifier->predecessors[block * count + predecessor]) {
            ++predecessor_count;
        }
    }
    if (predecessor_count != instruction->target_count) {
        return ir_verify_error(verifier,
                               "phi incoming blocks do not match predecessors");
    }
    for (size_t index = 0u; index < instruction->target_count; ++index) {
        RccIrBlockId incoming = instruction->targets[index];
        if (incoming >= count ||
            !verifier->predecessors[block * count + incoming]) {
            return ir_verify_error(verifier,
                                   "phi names a non-predecessor block");
        }
        for (size_t earlier = 0u; earlier < index; ++earlier) {
            if (instruction->targets[earlier] == incoming) {
                return ir_verify_error(verifier,
                                       "phi repeats an incoming block");
            }
        }
    }
    return true;
}

static bool ir_verify_uses(RccIrVerifier* verifier) {
    for (size_t block_index = 0u;
         block_index < verifier->function->block_count; ++block_index) {
        size_t order = 0u;
        for (RccIrInstruction* instruction =
                 verifier->blocks[block_index]->first;
             instruction; instruction = instruction->next, ++order) {
            if (!ir_verify_instruction_types(verifier, instruction)) {
                return false;
            }
            if (instruction->opcode == RCC_IR_PHI) {
                if (!ir_verify_phi_predecessors(verifier, instruction)) {
                    return false;
                }
                for (size_t index = 0u;
                     index < instruction->operand_count; ++index) {
                    if (!ir_value_dominates_edge(
                            verifier, instruction->operands[index],
                            instruction->targets[index])) {
                        return false;
                    }
                }
            } else {
                for (size_t index = 0u;
                     index < instruction->operand_count; ++index) {
                    if (!ir_value_dominates_use(
                            verifier, instruction->operands[index],
                            block_index, order)) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

static void ir_verifier_release(RccIrVerifier* verifier) {
    rcc_free(verifier->blocks);
    rcc_free(verifier->predecessors);
    rcc_free(verifier->dominators);
    rcc_free(verifier->parameters);
    rcc_free(verifier->definitions);
    rcc_free(verifier->definition_blocks);
    rcc_free(verifier->definition_orders);
    rcc_free(verifier->terminator_orders);
}

bool rcc_ir_verify_function(const RccIrFunction* function, char* error,
                            size_t error_size) {
    RccIrVerifier verifier = {0};
    bool valid = false;
    if (error && error_size != 0u) error[0] = '\0';
    verifier.function = function;
    verifier.error = error;
    verifier.error_size = error_size;
    if (!function || !function->name || !function->name[0] ||
        !ir_type_well_formed(function->return_type)) {
        ir_verify_error(&verifier, "invalid IR function contract");
        return false;
    }
    if (!ir_collect_blocks(&verifier) || !ir_collect_values(&verifier) ||
        !ir_build_cfg(&verifier) || !ir_compute_dominators(&verifier) ||
        !ir_verify_uses(&verifier)) {
        ir_verifier_release(&verifier);
        return false;
    }
    valid = true;
    ir_verifier_release(&verifier);
    return valid;
}

bool rcc_ir_verify_module(const RccIrModule* module, char* error,
                          size_t error_size) {
    size_t count = 0u;
    const RccIrFunction* function;
    const RccIrFunction* last = NULL;
    if (error && error_size != 0u) error[0] = '\0';
    if (!module) {
        if (error && error_size != 0u) {
            snprintf(error, error_size, "invalid IR module");
        }
        return false;
    }
    for (function = module->first_function; function;
         function = function->next) {
        if (function->module != module ||
            !rcc_ir_verify_function(function, error, error_size)) {
            return false;
        }
        for (const RccIrFunction* earlier = module->first_function;
             earlier != function; earlier = earlier->next) {
            if (strcmp(earlier->name, function->name) == 0) {
                if (error && error_size != 0u) {
                    snprintf(error, error_size,
                             "duplicate IR function '%s'", function->name);
                }
                return false;
            }
        }
        last = function;
        ++count;
    }
    if (count != module->function_count ||
        (count == 0u && module->last_function) ||
        (count != 0u && module->last_function != last)) {
        if (error && error_size != 0u) {
            snprintf(error, error_size, "IR function list is inconsistent");
        }
        return false;
    }
    return true;
}
