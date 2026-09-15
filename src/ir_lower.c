/*
 * RCC - AST to target-independent typed SSA IR lowering
 */

#include "rcc.h"
#include "ir_lower.h"
#include "ir_pass.h"
#include "mir.h"
#include "mir_alloc.h"
#include "mir_phi.h"
#include "x86_select.h"
#include "x86_legalize.h"
#include "cxx_exception_type.h"

typedef struct RccIrLowerLocal {
    const Decl* declaration;
    RccIrValue address;
    RccIrType type;
    struct RccIrLowerLocal* next;
} RccIrLowerLocal;

typedef struct {
    RccIrValue value;
    RccIrType type;
    bool is_unsigned;
    bool valid;
} RccIrLowerValue;

typedef struct RccIrLowerSwitchLabel {
    const Stmt* statement;
    RccIrBlock* block;
    uint64_t case_bits;
    struct RccIrLowerSwitchLabel* next;
} RccIrLowerSwitchLabel;

typedef struct RccIrLowerSwitch {
    const Type* control_type;
    RccIrLowerSwitchLabel* labels;
    RccIrLowerSwitchLabel* labels_tail;
    RccIrLowerSwitchLabel* default_label;
    struct RccIrLowerSwitch* previous;
} RccIrLowerSwitch;

typedef struct {
    RccIrModule* module;
    RccIrFunction* function;
    const Type* ast_return_type;
    RccIrBlock* current;
    RccIrLowerLocal* locals;
    RccIrLowerSwitch* current_switch;
    RccIrBlockId break_target;
    RccIrBlockId continue_target;
    RccIrValue aggregate_return_address;
    RccIrValue active_exception_frame;
    int aggregate_return_kind;
    bool terminated;
    bool unsupported;
} RccIrLowerContext;

static bool lower_statement(RccIrLowerContext* context,
                            const Stmt* statement);
static RccIrLowerValue lower_expression(RccIrLowerContext* context,
                                        const Expr* expression);
static bool lower_branch(RccIrLowerContext* context,
                         RccIrBlockId target);
static bool lower_conditional_branch(RccIrLowerContext* context,
                                     RccIrLowerValue condition,
                                     RccIrBlockId then_target,
                                     RccIrBlockId else_target);
static RccIrLowerValue lower_conditional_expression(
    RccIrLowerContext* context, const Expr* expression);
static RccIrLowerValue lower_logical_expression(
    RccIrLowerContext* context, const Expr* expression);
static bool lower_switch(RccIrLowerContext* context,
                         const Stmt* statement);
static bool lower_switch_case(RccIrLowerContext* context,
                              const Stmt* statement);
static bool lower_struct_type_supported(const Type* type);
static bool lower_copy_struct_storage(
    RccIrLowerContext* context, RccIrValue destination,
    RccIrValue source, const Type* type);
static bool lower_zero_array_storage(
    RccIrLowerContext* context, RccIrValue base,
    const Type* array_type);
static bool lower_array_initializer(
    RccIrLowerContext* context, RccIrValue base,
    const Type* array_type, const Expr* initializer);
static bool lower_zero_struct_storage(
    RccIrLowerContext* context, RccIrValue base,
    const Type* type);
static bool lower_initialize_struct_storage(
    RccIrLowerContext* context, RccIrValue base,
    const Type* type, const Expr* initializer);
static bool lower_union_type_supported(const Type* type);
static bool lower_copy_union_storage(
    RccIrLowerContext* context, RccIrValue destination,
    RccIrValue source, const Type* type);
static bool lower_zero_union_storage(
    RccIrLowerContext* context, RccIrValue base,
    const Type* type);
static bool lower_initialize_union_storage(
    RccIrLowerContext* context, RccIrValue base,
    const Type* type, const Expr* initializer);
static RccIrLowerValue lower_compound_literal_address(
    RccIrLowerContext* context, const Expr* expression);
static bool lower_abi_is_aggregate(const Type* type);
static bool lower_abi_aggregate_supported(const Type* type);
static int lower_abi_return_layout(const Type* type,
                                   RccIrType* return_type);
static size_t lower_abi_chunk_size(void);
static bool lower_abi_parameter_layout(
    const TypeParam* parameters, const RccIrType* prefix_types,
    size_t prefix_count, RccIrType** types_out, size_t* count_out);
static RccIrLowerValue lower_load_aggregate_chunk(
    RccIrLowerContext* context, RccIrLowerValue base,
    const Type* aggregate_type, size_t offset);

enum {
    LOWER_ABI_RETURN_SCALAR = 0,
    LOWER_ABI_RETURN_REGISTER_AGGREGATE,
    LOWER_ABI_RETURN_REGISTER_PAIR,
    LOWER_ABI_RETURN_SRET,
    LOWER_ABI_RETURN_UNSUPPORTED,
};

static RccIrLowerValue lower_invalid_value(void) {
    RccIrLowerValue value;
    value.value = RCC_IR_VALUE_NONE;
    value.type = rcc_ir_type_void();
    value.is_unsigned = false;
    value.valid = false;
    return value;
}

static RccIrLowerValue lower_value(RccIrValue id, RccIrType type,
                                   bool is_unsigned) {
    RccIrLowerValue value;
    value.value = id;
    value.type = type;
    value.is_unsigned = is_unsigned;
    value.valid = id != RCC_IR_VALUE_NONE;
    return value;
}

static bool lower_type(const Type* type, RccIrType* result) {
    if (!type || !result || type->is_reference || type->cleanup_function ||
        type->is_volatile) {
        return false;
    }
    switch (type->kind) {
        case TYPE_VOID:
            *result = rcc_ir_type_void();
            return true;
        case TYPE_BOOL:
            *result = rcc_ir_type_integer(1u);
            return true;
        case TYPE_CHAR:
        case TYPE_SHORT:
        case TYPE_INT:
        case TYPE_LONG:
        case TYPE_LLONG:
        case TYPE_ENUM:
            if (type->size <= 0 ||
                type->size > (g_opts.target_arch == ARCH_X64 ? 8 : 4)) {
                return false;
            }
            *result = rcc_ir_type_integer((uint16_t)(type->size * 8));
            return true;
        case TYPE_PTR:
        case TYPE_NULLPTR:
            *result = rcc_ir_type_pointer(0u);
            return true;
        case TYPE_FLOAT:
        case TYPE_DOUBLE:
        case TYPE_ARRAY:
        case TYPE_FUNC:
        case TYPE_STRUCT:
        case TYPE_UNION:
            return false;
    }
    return false;
}

static RccIrLowerLocal* lower_find_local(RccIrLowerContext* context,
                                         const Decl* declaration) {
    RccIrLowerLocal* local = context ? context->locals : NULL;
    while (local && local->declaration != declaration) local = local->next;
    return local;
}

static bool lower_add_local(RccIrLowerContext* context,
                            const Decl* declaration, RccIrValue address,
                            RccIrType type) {
    RccIrLowerLocal* local;
    if (!context || !declaration || address == RCC_IR_VALUE_NONE ||
        lower_find_local(context, declaration)) {
        return false;
    }
    local = rcc_alloc(sizeof(*local));
    local->declaration = declaration;
    local->address = address;
    local->type = type;
    local->next = context->locals;
    context->locals = local;
    return true;
}

static void lower_release_locals(RccIrLowerLocal* local) {
    while (local) {
        RccIrLowerLocal* next = local->next;
        rcc_free(local);
        local = next;
    }
}

static RccIrInstruction* lower_append(
    RccIrLowerContext* context, RccIrOpcode opcode, RccIrType type,
    const RccIrValue* operands, size_t operand_count,
    const RccIrBlockId* targets, size_t target_count) {
    RccIrInstruction* instruction;
    if (!context || !context->current || context->terminated) return NULL;
    instruction = rcc_ir_append(context->current, opcode, type, operands,
                                operand_count, targets, target_count);
    if (!instruction) context->unsupported = true;
    return instruction;
}

static RccIrLowerValue lower_integer_constant(RccIrLowerContext* context,
                                              RccIrType type,
                                              bool is_unsigned,
                                              uint64_t immediate) {
    RccIrInstruction* instruction = lower_append(
        context, RCC_IR_CONST_INT, type, NULL, 0u, NULL, 0u);
    if (!instruction) return lower_invalid_value();
    rcc_ir_set_immediate(instruction, immediate);
    return lower_value(instruction->result, type, is_unsigned);
}

static RccIrLowerValue lower_cast(RccIrLowerContext* context,
                                  RccIrLowerValue source,
                                  const Type* target_type) {
    RccIrType target;
    RccIrInstruction* instruction;
    RccIrValue operand;
    if (!source.valid || !lower_type(target_type, &target)) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    if (rcc_ir_type_equal(source.type, target)) {
        source.is_unsigned = target_type->is_unsigned;
        return source;
    }
    operand = source.value;
    if (source.type.kind == RCC_IR_TYPE_INTEGER &&
        target.kind == RCC_IR_TYPE_INTEGER) {
        if (target.bit_width == 1u) {
            RccIrLowerValue zero = lower_integer_constant(
                context, source.type, true, 0u);
            RccIrValue operands[2];
            if (!zero.valid) return lower_invalid_value();
            operands[0] = source.value;
            operands[1] = zero.value;
            instruction = lower_append(context, RCC_IR_ICMP, target,
                                       operands, 2u, NULL, 0u);
            if (!instruction) return lower_invalid_value();
            rcc_ir_set_predicate(instruction, RCC_IR_ICMP_NE);
        } else if (target.bit_width < source.type.bit_width) {
            instruction = lower_append(context, RCC_IR_TRUNC, target,
                                       &operand, 1u, NULL, 0u);
        } else if (target.bit_width > source.type.bit_width) {
            instruction = lower_append(
                context, source.is_unsigned ? RCC_IR_ZEXT : RCC_IR_SEXT,
                target, &operand, 1u, NULL, 0u);
        } else {
            return lower_value(source.value, target,
                               target_type->is_unsigned);
        }
    } else if (source.type.kind == RCC_IR_TYPE_POINTER &&
               target.kind == RCC_IR_TYPE_INTEGER) {
        instruction = lower_append(context, RCC_IR_PTR_TO_INT, target,
                                   &operand, 1u, NULL, 0u);
    } else if (source.type.kind == RCC_IR_TYPE_INTEGER &&
               target.kind == RCC_IR_TYPE_POINTER) {
        instruction = lower_append(context, RCC_IR_INT_TO_PTR, target,
                                   &operand, 1u, NULL, 0u);
    } else if (source.type.kind == RCC_IR_TYPE_POINTER &&
               target.kind == RCC_IR_TYPE_POINTER) {
        return lower_value(source.value, target, true);
    } else {
        context->unsupported = true;
        return lower_invalid_value();
    }
    if (!instruction) return lower_invalid_value();
    return lower_value(instruction->result, target,
                       target_type->is_unsigned);
}

static RccIrLowerValue lower_truth(RccIrLowerContext* context,
                                   RccIrLowerValue source) {
    RccIrType i1 = rcc_ir_type_integer(1u);
    RccIrInstruction* compare;
    RccIrValue operands[2];
    RccIrLowerValue zero;
    if (!source.valid) return lower_invalid_value();
    if (rcc_ir_type_equal(source.type, i1)) return source;
    if (source.type.kind == RCC_IR_TYPE_INTEGER) {
        zero = lower_integer_constant(context, source.type, true, 0u);
    } else if (source.type.kind == RCC_IR_TYPE_POINTER) {
        RccIrType pointer_integer = rcc_ir_type_integer(
            g_opts.target_arch == ARCH_X64 ? 64u : 32u);
        RccIrLowerValue integer_zero;
        RccIrInstruction* cast;
        integer_zero = lower_integer_constant(context, pointer_integer,
                                              true, 0u);
        if (!integer_zero.valid) return lower_invalid_value();
        cast = lower_append(context, RCC_IR_INT_TO_PTR, source.type,
                            &integer_zero.value, 1u, NULL, 0u);
        if (!cast) return lower_invalid_value();
        zero = lower_value(cast->result, source.type, true);
    } else {
        context->unsupported = true;
        return lower_invalid_value();
    }
    if (!zero.valid) return lower_invalid_value();
    operands[0] = source.value;
    operands[1] = zero.value;
    compare = lower_append(context, RCC_IR_ICMP, i1, operands, 2u,
                           NULL, 0u);
    if (!compare) return lower_invalid_value();
    rcc_ir_set_predicate(compare, RCC_IR_ICMP_NE);
    return lower_value(compare->result, i1, true);
}

static RccIrLowerValue lower_pointer_offset(
    RccIrLowerContext* context, RccIrLowerValue pointer,
    RccIrLowerValue index, const Type* pointer_type,
    bool subtract_index) {
    const Type* index_type;
    RccIrLowerValue zero;
    RccIrValue operands[2];
    RccIrInstruction* instruction;
    uint64_t scale;
    if (!pointer_type ||
        (pointer_type->kind != TYPE_PTR &&
         pointer_type->kind != TYPE_ARRAY) ||
        !pointer_type->base || pointer_type->base->size <= 0 ||
        (uint64_t)pointer_type->base->size > (uint64_t)INT32_MAX) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    if (!pointer.valid || pointer.type.kind != RCC_IR_TYPE_POINTER ||
        !index.valid || index.type.kind != RCC_IR_TYPE_INTEGER) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    index_type = index.is_unsigned ? type_ulong : type_long;
    index = lower_cast(context, index, index_type);
    if (!index.valid) return lower_invalid_value();
    if (subtract_index) {
        zero = lower_integer_constant(context, index.type,
                                      index.is_unsigned, 0u);
        if (!zero.valid) return lower_invalid_value();
        operands[0] = zero.value;
        operands[1] = index.value;
        instruction = lower_append(context, RCC_IR_SUB, index.type,
                                   operands, 2u, NULL, 0u);
        if (!instruction) return lower_invalid_value();
        index = lower_value(instruction->result, index.type,
                            index.is_unsigned);
    }
    operands[0] = pointer.value;
    operands[1] = index.value;
    instruction = lower_append(context, RCC_IR_GEP,
                               rcc_ir_type_pointer(0u), operands, 2u,
                               NULL, 0u);
    if (!instruction) return lower_invalid_value();
    scale = (uint64_t)pointer_type->base->size;
    rcc_ir_set_immediate(instruction, scale);
    return lower_value(instruction->result, rcc_ir_type_pointer(0u), true);
}

static RccIrLowerValue lower_pointer_gep(
    RccIrLowerContext* context, const Expr* pointer_expression,
    const Expr* index_expression, bool subtract_index) {
    RccIrLowerValue pointer;
    RccIrLowerValue index;
    if (!pointer_expression || !index_expression) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    pointer = lower_expression(context, pointer_expression);
    index = lower_expression(context, index_expression);
    return lower_pointer_offset(context, pointer, index,
                                pointer_expression->type,
                                subtract_index);
}

static RccIrLowerValue lower_pointer_binary(
    RccIrLowerContext* context, const Expr* expression) {
    const Expr* pointer_expression = NULL;
    const Expr* index_expression = NULL;
    bool subtract_index = expression->kind == EXPR_SUB;
    if (expression->binary_lhs && expression->binary_lhs->type &&
        (expression->binary_lhs->type->kind == TYPE_PTR ||
         expression->binary_lhs->type->kind == TYPE_ARRAY) &&
        expression->binary_rhs && expression->binary_rhs->type &&
        expression->binary_rhs->type->kind != TYPE_PTR &&
        expression->binary_rhs->type->kind != TYPE_ARRAY) {
        pointer_expression = expression->binary_lhs;
        index_expression = expression->binary_rhs;
    } else if (expression->kind == EXPR_ADD && expression->binary_rhs &&
               expression->binary_rhs->type &&
               (expression->binary_rhs->type->kind == TYPE_PTR ||
                expression->binary_rhs->type->kind == TYPE_ARRAY) &&
               expression->binary_lhs && expression->binary_lhs->type &&
               expression->binary_lhs->type->kind != TYPE_PTR &&
               expression->binary_lhs->type->kind != TYPE_ARRAY) {
        pointer_expression = expression->binary_rhs;
        index_expression = expression->binary_lhs;
    }
    if (!pointer_expression || !index_expression) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    return lower_pointer_gep(context, pointer_expression, index_expression,
                             subtract_index);
}

static RccIrLowerValue lower_pointer_difference(
    RccIrLowerContext* context, const Expr* expression) {
    const Type* pointer_type;
    RccIrLowerValue left;
    RccIrLowerValue right;
    RccIrLowerValue scale;
    RccIrType result_type;
    RccIrValue operands[2];
    RccIrInstruction* difference;
    RccIrInstruction* quotient;
    if (!expression || !expression->binary_lhs ||
        !expression->binary_rhs || !expression->binary_lhs->type ||
        !expression->binary_rhs->type ||
        expression->binary_lhs->type->kind != TYPE_PTR ||
        expression->binary_rhs->type->kind != TYPE_PTR ||
        !expression->type || !lower_type(expression->type, &result_type) ||
        result_type.kind != RCC_IR_TYPE_INTEGER ||
        expression->type->is_unsigned) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    pointer_type = expression->binary_lhs->type;
    if (!pointer_type->base || pointer_type->base->size <= 0) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    left = lower_expression(context, expression->binary_lhs);
    right = lower_expression(context, expression->binary_rhs);
    left = lower_cast(context, left, expression->type);
    right = lower_cast(context, right, expression->type);
    if (!left.valid || !right.valid) return lower_invalid_value();
    operands[0] = left.value;
    operands[1] = right.value;
    difference = lower_append(context, RCC_IR_SUB, result_type,
                              operands, 2u, NULL, 0u);
    if (!difference) return lower_invalid_value();
    scale = lower_integer_constant(
        context, result_type, false, (uint64_t)pointer_type->base->size);
    if (!scale.valid) return lower_invalid_value();
    operands[0] = difference->result;
    operands[1] = scale.value;
    quotient = lower_append(context, RCC_IR_SDIV, result_type,
                            operands, 2u, NULL, 0u);
    if (!quotient) return lower_invalid_value();
    return lower_value(quotient->result, result_type, false);
}

static RccIrLowerValue lower_byte_offset_address(
    RccIrLowerContext* context, RccIrLowerValue base,
    uint64_t byte_offset) {
    RccIrType index_type;
    RccIrLowerValue index;
    RccIrValue operands[2];
    RccIrInstruction* address;
    if (!base.valid || base.type.kind != RCC_IR_TYPE_POINTER ||
        !lower_type(type_long, &index_type) ||
        index_type.kind != RCC_IR_TYPE_INTEGER) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    if (byte_offset == 0u) return base;
    index = lower_integer_constant(
        context, index_type, true, byte_offset);
    if (!index.valid) return lower_invalid_value();
    operands[0] = base.value;
    operands[1] = index.value;
    address = lower_append(context, RCC_IR_GEP,
                           rcc_ir_type_pointer(0u), operands, 2u,
                           NULL, 0u);
    if (!address) return lower_invalid_value();
    rcc_ir_set_immediate(address, 1u);
    return lower_value(
        address->result, rcc_ir_type_pointer(0u), true);
}

static RccIrLowerValue lower_lvalue_address(
    RccIrLowerContext* context, const Expr* expression) {
    RccIrLowerLocal* local;
    if (!expression) return lower_invalid_value();
    if (expression->kind == EXPR_IDENT) {
        local = lower_find_local(context, expression->ident_decl);
        if (!local) {
            const Decl* declaration = expression->ident_decl;
            RccIrInstruction* address;
            if (!declaration || declaration->kind != DECL_VAR ||
                !declaration->var_is_global ||
                declaration->var_is_thread_local ||
                !declaration->type || declaration->type->size <= 0 ||
                declaration->type->is_reference ||
                declaration->type->is_volatile ||
                declaration->type->cleanup_function) {
                context->unsupported = true;
                return lower_invalid_value();
            }
            address = lower_append(
                context, RCC_IR_SYMBOL_ADDRESS,
                rcc_ir_type_pointer(0u), NULL, 0u, NULL, 0u);
            if (!address) return lower_invalid_value();
            rcc_ir_set_callee(address, decl_link_name(declaration));
            return lower_value(
                address->result, rcc_ir_type_pointer(0u), true);
        }
        return lower_value(local->address, rcc_ir_type_pointer(0u), true);
    }
    if (expression->kind == EXPR_DEREF) {
        RccIrLowerValue pointer = lower_expression(
            context, expression->unary_operand);
        if (!pointer.valid || pointer.type.kind != RCC_IR_TYPE_POINTER) {
            context->unsupported = true;
            return lower_invalid_value();
        }
        return pointer;
    }
    if (expression->kind == EXPR_INDEX) {
        const Expr* pointer_expression = expression->index_base;
        const Expr* index_expression = expression->index_expr;
        if (pointer_expression && pointer_expression->type &&
            pointer_expression->type->kind != TYPE_PTR && index_expression &&
            index_expression->type && index_expression->type->kind == TYPE_PTR) {
            pointer_expression = expression->index_expr;
            index_expression = expression->index_base;
        }
        return lower_pointer_gep(context, pointer_expression,
                                 index_expression, false);
    }
    if (expression->kind == EXPR_MEMBER ||
        expression->kind == EXPR_PTR_MEMBER) {
        const Type* aggregate_type;
        const TypeField* field = expression->member_field;
        RccIrLowerValue base;
        if (expression->kind == EXPR_PTR_MEMBER) {
            base = lower_expression(context, expression->member_base);
            aggregate_type = expression->member_base &&
                expression->member_base->type &&
                expression->member_base->type->kind == TYPE_PTR
                    ? expression->member_base->type->base : NULL;
        } else {
            base = lower_lvalue_address(context, expression->member_base);
            aggregate_type = expression->member_base
                ? expression->member_base->type : NULL;
        }
        if (!aggregate_type ||
            (aggregate_type->kind != TYPE_STRUCT &&
             aggregate_type->kind != TYPE_UNION) ||
            aggregate_type->size <= 0 || !field || !field->type ||
            field->offset < 0 || field->type->size <= 0 ||
            field->offset > aggregate_type->size ||
            field->type->size > aggregate_type->size - field->offset) {
            context->unsupported = true;
            return lower_invalid_value();
        }
        return lower_byte_offset_address(
            context, base, (uint64_t)field->offset);
    }
    if (expression->kind == EXPR_COMPOUND) {
        return lower_compound_literal_address(context, expression);
    }
    context->unsupported = true;
    return lower_invalid_value();
}

static RccIrLowerValue lower_load_address(RccIrLowerContext* context,
                                          RccIrLowerValue address,
                                          const Type* ast_type) {
    RccIrType type;
    RccIrInstruction* load;
    if (!address.valid || !lower_type(ast_type, &type) ||
        type.kind == RCC_IR_TYPE_VOID) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    load = lower_append(context, RCC_IR_LOAD, type, &address.value, 1u,
                        NULL, 0u);
    if (!load) return lower_invalid_value();
    return lower_value(load->result, type, ast_type->is_unsigned);
}

static RccIrLowerValue lower_load_lvalue(RccIrLowerContext* context,
                                         const Expr* expression) {
    RccIrLowerValue address = lower_lvalue_address(context, expression);
    return lower_load_address(context, address,
                              expression ? expression->type : NULL);
}

static bool lower_store_address(RccIrLowerContext* context,
                                RccIrLowerValue address,
                                RccIrLowerValue value) {
    RccIrValue operands[2];
    if (!address.valid || !value.valid) return false;
    operands[0] = value.value;
    operands[1] = address.value;
    return lower_append(context, RCC_IR_STORE, rcc_ir_type_void(),
                        operands, 2u, NULL, 0u) != NULL;
}

static bool lower_store_lvalue(RccIrLowerContext* context,
                               const Expr* expression,
                               RccIrLowerValue value) {
    RccIrLowerValue address = lower_lvalue_address(context, expression);
    return lower_store_address(context, address, value);
}

static RccIrOpcode lower_binary_opcode(ExprKind kind, bool is_unsigned) {
    switch (kind) {
        case EXPR_ADD: return RCC_IR_ADD;
        case EXPR_SUB: return RCC_IR_SUB;
        case EXPR_MUL: return RCC_IR_MUL;
        case EXPR_DIV: return is_unsigned ? RCC_IR_UDIV : RCC_IR_SDIV;
        case EXPR_MOD: return is_unsigned ? RCC_IR_UREM : RCC_IR_SREM;
        case EXPR_BITAND: return RCC_IR_AND;
        case EXPR_BITOR: return RCC_IR_OR;
        case EXPR_BITXOR: return RCC_IR_XOR;
        case EXPR_LSHIFT: return RCC_IR_SHL;
        case EXPR_RSHIFT: return is_unsigned ? RCC_IR_LSHR : RCC_IR_ASHR;
        default: return RCC_IR_UNREACHABLE;
    }
}

static RccIrLowerValue lower_integer_binary(
    RccIrLowerContext* context, const Expr* expression) {
    RccIrLowerValue left = lower_expression(context,
                                            expression->binary_lhs);
    RccIrLowerValue right = lower_expression(context,
                                             expression->binary_rhs);
    RccIrType result_type;
    RccIrValue operands[2];
    RccIrInstruction* instruction;
    RccIrOpcode opcode;
    if (!left.valid || !right.valid ||
        left.type.kind == RCC_IR_TYPE_POINTER ||
        right.type.kind == RCC_IR_TYPE_POINTER ||
        !lower_type(expression->type, &result_type) ||
        result_type.kind != RCC_IR_TYPE_INTEGER) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    left = lower_cast(context, left, expression->type);
    right = lower_cast(context, right, expression->type);
    if (!left.valid || !right.valid) return lower_invalid_value();
    opcode = lower_binary_opcode(expression->kind,
                                 expression->type->is_unsigned);
    if (opcode == RCC_IR_UNREACHABLE) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    operands[0] = left.value;
    operands[1] = right.value;
    instruction = lower_append(context, opcode, result_type, operands, 2u,
                               NULL, 0u);
    if (!instruction) return lower_invalid_value();
    return lower_value(instruction->result, result_type,
                       expression->type->is_unsigned);
}

static RccIrIntPredicate lower_comparison_predicate(ExprKind kind,
                                                     bool is_unsigned) {
    switch (kind) {
        case EXPR_EQ: return RCC_IR_ICMP_EQ;
        case EXPR_NE: return RCC_IR_ICMP_NE;
        case EXPR_LT:
            return is_unsigned ? RCC_IR_ICMP_ULT : RCC_IR_ICMP_SLT;
        case EXPR_LE:
            return is_unsigned ? RCC_IR_ICMP_ULE : RCC_IR_ICMP_SLE;
        case EXPR_GT:
            return is_unsigned ? RCC_IR_ICMP_UGT : RCC_IR_ICMP_SGT;
        case EXPR_GE:
            return is_unsigned ? RCC_IR_ICMP_UGE : RCC_IR_ICMP_SGE;
        default: return RCC_IR_ICMP_EQ;
    }
}

static RccIrLowerValue lower_comparison(RccIrLowerContext* context,
                                        const Expr* expression) {
    Type* comparison_type = type_common(expression->binary_lhs->type,
                                        expression->binary_rhs->type);
    RccIrLowerValue left = lower_expression(context,
                                            expression->binary_lhs);
    RccIrLowerValue right = lower_expression(context,
                                             expression->binary_rhs);
    RccIrValue operands[2];
    RccIrInstruction* compare;
    RccIrLowerValue result;
    if (!comparison_type || !left.valid || !right.valid) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    left = lower_cast(context, left, comparison_type);
    right = lower_cast(context, right, comparison_type);
    if (!left.valid || !right.valid) return lower_invalid_value();
    operands[0] = left.value;
    operands[1] = right.value;
    compare = lower_append(context, RCC_IR_ICMP,
                           rcc_ir_type_integer(1u), operands, 2u,
                           NULL, 0u);
    if (!compare) return lower_invalid_value();
    rcc_ir_set_predicate(compare, lower_comparison_predicate(
        expression->kind, comparison_type->is_unsigned ||
                              comparison_type->kind == TYPE_PTR));
    result = lower_value(compare->result, rcc_ir_type_integer(1u), true);
    return lower_cast(context, result, expression->type);
}

static RccIrLowerValue lower_assignment(RccIrLowerContext* context,
                                        const Expr* expression) {
    RccIrLowerValue value;
    if (expression->cxx_move_assignment) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    if (expression->binary_lhs && expression->binary_lhs->type &&
        (expression->binary_lhs->type->kind == TYPE_STRUCT ||
         expression->binary_lhs->type->kind == TYPE_UNION)) {
        RccIrLowerValue destination;
        if (!expression->binary_rhs || !expression->binary_rhs->type ||
            expression->binary_rhs->type->kind !=
                expression->binary_lhs->type->kind ||
            !type_is_compatible(expression->binary_lhs->type,
                                expression->binary_rhs->type)) {
            context->unsupported = true;
            return lower_invalid_value();
        }
        destination = lower_lvalue_address(
            context, expression->binary_lhs);
        value = lower_expression(context, expression->binary_rhs);
        if (!destination.valid || !value.valid ||
            value.type.kind != RCC_IR_TYPE_POINTER) {
            return lower_invalid_value();
        }
        if (expression->binary_lhs->type->kind == TYPE_STRUCT) {
            if (!lower_copy_struct_storage(
                    context, destination.value, value.value,
                    expression->binary_lhs->type)) {
                return lower_invalid_value();
            }
        } else if (!lower_copy_union_storage(
                       context, destination.value, value.value,
                       expression->binary_lhs->type)) {
            return lower_invalid_value();
        }
        return destination;
    }
    value = lower_expression(context, expression->binary_rhs);
    if (!value.valid || !expression->binary_lhs ||
        !expression->binary_lhs->type) {
        return lower_invalid_value();
    }
    value = lower_cast(context, value, expression->binary_lhs->type);
    if (!value.valid || !lower_store_lvalue(context,
                                            expression->binary_lhs, value)) {
        return lower_invalid_value();
    }
    return value;
}

static ExprKind lower_compound_binary_kind(ExprKind kind) {
    switch (kind) {
        case EXPR_ADD_ASSIGN: return EXPR_ADD;
        case EXPR_SUB_ASSIGN: return EXPR_SUB;
        case EXPR_MUL_ASSIGN: return EXPR_MUL;
        case EXPR_DIV_ASSIGN: return EXPR_DIV;
        case EXPR_MOD_ASSIGN: return EXPR_MOD;
        case EXPR_AND_ASSIGN: return EXPR_BITAND;
        case EXPR_OR_ASSIGN: return EXPR_BITOR;
        case EXPR_XOR_ASSIGN: return EXPR_BITXOR;
        case EXPR_LSHIFT_ASSIGN: return EXPR_LSHIFT;
        case EXPR_RSHIFT_ASSIGN: return EXPR_RSHIFT;
        default: return kind;
    }
}

static RccIrLowerValue lower_compound_assignment(
    RccIrLowerContext* context, const Expr* expression) {
    RccIrLowerValue address;
    RccIrLowerValue left;
    RccIrLowerValue right;
    Type* operation_type;
    RccIrType ir_operation_type;
    RccIrInstruction* operation;
    RccIrValue operands[2];
    RccIrLowerValue result;
    ExprKind binary_kind = lower_compound_binary_kind(expression->kind);
    if (!expression->binary_lhs || !expression->binary_lhs->type) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    address = lower_lvalue_address(context, expression->binary_lhs);
    left = lower_load_address(context, address,
                              expression->binary_lhs->type);
    right = lower_expression(context, expression->binary_rhs);
    if (!address.valid || !left.valid || !right.valid) {
        return lower_invalid_value();
    }
    if (expression->binary_lhs->type->kind == TYPE_PTR) {
        if ((expression->kind != EXPR_ADD_ASSIGN &&
             expression->kind != EXPR_SUB_ASSIGN) ||
            right.type.kind != RCC_IR_TYPE_INTEGER) {
            context->unsupported = true;
            return lower_invalid_value();
        }
        result = lower_pointer_offset(
            context, left, right, expression->binary_lhs->type,
            expression->kind == EXPR_SUB_ASSIGN);
        if (!result.valid ||
            !lower_store_address(context, address, result)) {
            return lower_invalid_value();
        }
        return result;
    }
    operation_type = binary_kind == EXPR_LSHIFT ||
            binary_kind == EXPR_RSHIFT
        ? type_common(expression->binary_lhs->type, type_int)
        : type_common(expression->binary_lhs->type,
                      expression->binary_rhs->type);
    if (!operation_type || !lower_type(operation_type, &ir_operation_type)) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    left = lower_cast(context, left, operation_type);
    right = lower_cast(context, right, operation_type);
    if (!left.valid || !right.valid) return lower_invalid_value();
    operands[0] = left.value;
    operands[1] = right.value;
    operation = lower_append(
        context, lower_binary_opcode(binary_kind,
                                     operation_type->is_unsigned),
        ir_operation_type, operands, 2u, NULL, 0u);
    if (!operation) return lower_invalid_value();
    result = lower_value(operation->result, ir_operation_type,
                         operation_type->is_unsigned);
    result = lower_cast(context, result, expression->binary_lhs->type);
    if (!result.valid || !lower_store_address(context, address, result)) {
        return lower_invalid_value();
    }
    return result;
}

static RccIrLowerValue lower_increment(RccIrLowerContext* context,
                                       const Expr* expression) {
    RccIrLowerValue address;
    RccIrLowerValue old_value;
    RccIrLowerValue one;
    RccIrValue operands[2];
    RccIrInstruction* operation;
    RccIrLowerValue new_value;
    bool increment = expression->kind == EXPR_PREINC ||
        expression->kind == EXPR_POSTINC;
    bool postfix = expression->kind == EXPR_POSTINC ||
        expression->kind == EXPR_POSTDEC;
    if (!expression->unary_operand || !expression->unary_operand->type) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    address = lower_lvalue_address(context, expression->unary_operand);
    old_value = lower_load_address(context, address,
                                   expression->unary_operand->type);
    if (!address.valid || !old_value.valid) return lower_invalid_value();
    if (expression->unary_operand->type->kind == TYPE_PTR) {
        RccIrType pointer_index_type;
        if (!lower_type(type_long, &pointer_index_type)) {
            context->unsupported = true;
            return lower_invalid_value();
        }
        one = lower_integer_constant(context, pointer_index_type,
                                     false, 1u);
        if (!one.valid) return lower_invalid_value();
        new_value = lower_pointer_offset(
            context, old_value, one, expression->unary_operand->type,
            !increment);
        if (!new_value.valid ||
            !lower_store_address(context, address, new_value)) {
            return lower_invalid_value();
        }
        if (postfix) return old_value;
        return new_value;
    }
    one = lower_integer_constant(context, old_value.type,
                                 old_value.is_unsigned, 1u);
    if (!one.valid) return lower_invalid_value();
    operands[0] = old_value.value;
    operands[1] = one.value;
    operation = lower_append(context, increment ? RCC_IR_ADD : RCC_IR_SUB,
                             old_value.type, operands, 2u, NULL, 0u);
    if (!operation) return lower_invalid_value();
    new_value = lower_value(operation->result, old_value.type,
                            old_value.is_unsigned);
    if (!lower_store_address(context, address, new_value)) {
        return lower_invalid_value();
    }
    if (postfix) return old_value;
    return new_value;
}

static RccIrLowerValue lower_call(RccIrLowerContext* context,
                                  const Expr* expression) {
    const Decl* callee;
    const ExprList* argument;
    TypeParam* parameter;
    Type* function_type;
    size_t argument_count = 0u;
    size_t index = 0u;
    size_t fixed_count = 0u;
    size_t chunk_size = lower_abi_chunk_size();
    RccIrValue* operands = NULL;
    RccIrType* fixed_types = NULL;
    RccIrType return_type;
    RccIrType call_type;
    RccIrType hidden_type = rcc_ir_type_pointer(0u);
    RccIrLowerValue aggregate_address = lower_invalid_value();
    int return_kind = LOWER_ABI_RETURN_SCALAR;
    RccIrInstruction* call;
    if (expression->cxx_close_call || !expression->call_func ||
        expression->call_func->kind != EXPR_IDENT) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    callee = expression->call_func->ident_decl;
    function_type = callee ? callee->type : NULL;
    if (!callee || callee->kind != DECL_FUNC || !function_type ||
        function_type->kind != TYPE_FUNC || !expression->type ||
        !function_type->ret_type ||
        !type_is_compatible(function_type->ret_type, expression->type)) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    if (lower_abi_is_aggregate(expression->type)) {
        RccIrInstruction* allocation;
        size_t allocation_size;
        return_kind = lower_abi_return_layout(
            expression->type, &return_type);
        if (return_kind == LOWER_ABI_RETURN_UNSUPPORTED) {
            context->unsupported = true;
            return lower_invalid_value();
        }
        allocation_size = ((size_t)expression->type->size +
                           chunk_size - 1u) / chunk_size * chunk_size;
        allocation = lower_append(
            context, RCC_IR_ALLOCA, rcc_ir_type_pointer(0u),
            NULL, 0u, NULL, 0u);
        if (!allocation) return lower_invalid_value();
        rcc_ir_set_immediate(allocation, (uint64_t)allocation_size);
        aggregate_address = lower_value(
            allocation->result, rcc_ir_type_pointer(0u), true);
        if (return_kind == LOWER_ABI_RETURN_SRET) argument_count = 1u;
    } else if (!lower_type(expression->type, &return_type)) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    if (!lower_abi_parameter_layout(
            function_type->params,
            return_kind == LOWER_ABI_RETURN_SRET ? &hidden_type : NULL,
            return_kind == LOWER_ABI_RETURN_SRET ? 1u : 0u,
            &fixed_types, &fixed_count)) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    rcc_free(fixed_types);
    parameter = function_type->params;
    for (argument = expression->call_args; argument;
         argument = argument->next) {
        size_t units = 1u;
        if (parameter && lower_abi_is_aggregate(parameter->type)) {
            if (!argument->expr || !argument->expr->type ||
                !lower_abi_aggregate_supported(parameter->type) ||
                !type_is_compatible(
                    parameter->type, argument->expr->type)) {
                context->unsupported = true;
                return lower_invalid_value();
            }
            units = ((size_t)parameter->type->size +
                     chunk_size - 1u) / chunk_size;
        } else if (!parameter && argument->expr &&
                   lower_abi_is_aggregate(argument->expr->type)) {
            context->unsupported = true;
            return lower_invalid_value();
        }
        if (units > SIZE_MAX - argument_count) {
            context->unsupported = true;
            return lower_invalid_value();
        }
        argument_count += units;
        if (parameter) parameter = parameter->next;
    }
    if (parameter || (!function_type->variadic &&
                      argument_count != fixed_count)) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    if (argument_count != 0u) {
        if (argument_count > SIZE_MAX / sizeof(*operands)) {
            context->unsupported = true;
            return lower_invalid_value();
        }
        operands = rcc_alloc(argument_count * sizeof(*operands));
    }
    if (return_kind == LOWER_ABI_RETURN_SRET) {
        operands[index++] = aggregate_address.value;
    }
    parameter = function_type->params;
    for (argument = expression->call_args; argument;
         argument = argument->next) {
        RccIrLowerValue value = lower_expression(context, argument->expr);
        if (parameter && lower_abi_is_aggregate(parameter->type)) {
            size_t units = ((size_t)parameter->type->size +
                            chunk_size - 1u) / chunk_size;
            if (!value.valid || value.type.kind != RCC_IR_TYPE_POINTER) {
                rcc_free(operands);
                context->unsupported = true;
                return lower_invalid_value();
            }
            for (size_t unit = 0u; unit < units; ++unit) {
                RccIrLowerValue chunk = lower_load_aggregate_chunk(
                    context, value, parameter->type,
                    unit * chunk_size);
                if (!chunk.valid) {
                    rcc_free(operands);
                    return lower_invalid_value();
                }
                operands[index++] = chunk.value;
            }
        } else if (parameter) {
            value = lower_cast(context, value, parameter->type);
            if (!value.valid) {
                rcc_free(operands);
                return lower_invalid_value();
            }
            operands[index++] = value.value;
        } else {
            if (!value.valid ||
                (value.type.kind != RCC_IR_TYPE_POINTER &&
                 (value.type.kind != RCC_IR_TYPE_INTEGER ||
                  value.type.bit_width > chunk_size * 8u))) {
                rcc_free(operands);
                context->unsupported = true;
                return lower_invalid_value();
            }
            operands[index++] = value.value;
        }
        if (parameter) parameter = parameter->next;
        if (!value.valid) {
            rcc_free(operands);
            return lower_invalid_value();
        }
    }
    if (index != argument_count) {
        rcc_free(operands);
        context->unsupported = true;
        return lower_invalid_value();
    }
    call_type = return_type;
    if (return_kind == LOWER_ABI_RETURN_REGISTER_PAIR) {
        call_type = rcc_ir_type_void();
    }
    call = lower_append(context, RCC_IR_CALL, call_type, operands,
                        argument_count, NULL, 0u);
    rcc_free(operands);
    if (!call) return lower_invalid_value();
    rcc_ir_set_callee(call, decl_link_name(callee));
    if (return_kind == LOWER_ABI_RETURN_SRET &&
        g_opts.target_arch != ARCH_X64) {
        rcc_ir_set_immediate(call, 4u);
    }
    if (return_kind == LOWER_ABI_RETURN_REGISTER_AGGREGATE) {
        RccIrLowerValue value = lower_value(
            call->result, return_type, true);
        if (!lower_store_address(context, aggregate_address, value)) {
            return lower_invalid_value();
        }
        return aggregate_address;
    }
    if (return_kind == LOWER_ABI_RETURN_REGISTER_PAIR) {
        RccIrInstruction* capture = lower_append(
            context, RCC_IR_CAPTURE_RETURN_PAIR,
            rcc_ir_type_void(), &aggregate_address.value,
            1u, NULL, 0u);
        if (!capture) return lower_invalid_value();
        rcc_ir_set_immediate(capture,
                             (uint64_t)expression->type->size);
        return aggregate_address;
    }
    if (return_kind == LOWER_ABI_RETURN_SRET) {
        return aggregate_address;
    }
    if (return_type.kind == RCC_IR_TYPE_VOID) {
        RccIrLowerValue value = lower_invalid_value();
        value.type = return_type;
        value.valid = true;
        return value;
    }
    return lower_value(call->result, return_type,
                       expression->type->is_unsigned);
}

static RccIrLowerValue lower_expression(RccIrLowerContext* context,
                                        const Expr* expression) {
    RccIrType type;
    RccIrLowerValue operand;
    RccIrLowerValue zero;
    RccIrValue operands[2];
    RccIrInstruction* instruction;
    int64_t constant;
    if (!context || context->unsupported || !expression ||
        expression->cxx_move_assignment || expression->cxx_close_call ||
        (expression->type && expression->type->cleanup_function)) {
        if (context) context->unsupported = true;
        return lower_invalid_value();
    }
    switch (expression->kind) {
        case EXPR_INT_LIT:
            if (!lower_type(expression->type, &type) ||
                type.kind != RCC_IR_TYPE_INTEGER) {
                context->unsupported = true;
                return lower_invalid_value();
            }
            return lower_integer_constant(context, type,
                                          expression->type->is_unsigned,
                                          (uint64_t)expression->int_val);
        case EXPR_CHAR_LIT:
            if (!lower_type(expression->type, &type)) {
                context->unsupported = true;
                return lower_invalid_value();
            }
            return lower_integer_constant(
                context, type, expression->type->is_unsigned,
                (unsigned char)expression->char_val);
        case EXPR_IDENT:
            if (expression->type &&
                (expression->type->kind == TYPE_ARRAY ||
                 expression->type->kind == TYPE_STRUCT ||
                 expression->type->kind == TYPE_UNION)) {
                return lower_lvalue_address(context, expression);
            }
            return lower_load_lvalue(context, expression);
        case EXPR_CXX_THIS:
            context->unsupported = true;
            return lower_invalid_value();
        case EXPR_NEG:
            operand = lower_expression(context, expression->unary_operand);
            if (!operand.valid || operand.type.kind != RCC_IR_TYPE_INTEGER) {
                context->unsupported = true;
                return lower_invalid_value();
            }
            operand = lower_cast(context, operand, expression->type);
            zero = lower_integer_constant(context, operand.type,
                                          operand.is_unsigned, 0u);
            if (!operand.valid || !zero.valid) return lower_invalid_value();
            operands[0] = zero.value;
            operands[1] = operand.value;
            instruction = lower_append(context, RCC_IR_SUB, operand.type,
                                       operands, 2u, NULL, 0u);
            if (!instruction) return lower_invalid_value();
            return lower_value(instruction->result, operand.type,
                               operand.is_unsigned);
        case EXPR_NOT:
            operand = lower_expression(context, expression->unary_operand);
            operand = lower_truth(context, operand);
            if (!operand.valid) return lower_invalid_value();
            zero = lower_integer_constant(context, operand.type, true, 0u);
            operands[0] = operand.value;
            operands[1] = zero.value;
            instruction = lower_append(context, RCC_IR_ICMP,
                                       rcc_ir_type_integer(1u), operands,
                                       2u, NULL, 0u);
            if (!instruction) return lower_invalid_value();
            rcc_ir_set_predicate(instruction, RCC_IR_ICMP_EQ);
            return lower_cast(
                context,
                lower_value(instruction->result,
                            rcc_ir_type_integer(1u), true),
                expression->type);
        case EXPR_BITNOT:
            operand = lower_expression(context, expression->unary_operand);
            operand = lower_cast(context, operand, expression->type);
            if (!operand.valid || operand.type.kind != RCC_IR_TYPE_INTEGER) {
                context->unsupported = true;
                return lower_invalid_value();
            }
            zero = lower_integer_constant(context, operand.type, true,
                                          UINT64_MAX);
            operands[0] = operand.value;
            operands[1] = zero.value;
            instruction = lower_append(context, RCC_IR_XOR, operand.type,
                                       operands, 2u, NULL, 0u);
            if (!instruction) return lower_invalid_value();
            return lower_value(instruction->result, operand.type,
                               operand.is_unsigned);
        case EXPR_ADDR:
            return lower_lvalue_address(context, expression->unary_operand);
        case EXPR_DEREF:
            if (expression->type &&
                (expression->type->kind == TYPE_STRUCT ||
                 expression->type->kind == TYPE_UNION)) {
                return lower_lvalue_address(context, expression);
            }
            return lower_load_lvalue(context, expression);
        case EXPR_PREINC:
        case EXPR_PREDEC:
        case EXPR_POSTINC:
        case EXPR_POSTDEC:
            return lower_increment(context, expression);
        case EXPR_CAST:
            operand = lower_expression(context, expression->cast_expr);
            return lower_cast(context, operand, expression->type);
        case EXPR_ADD:
        case EXPR_SUB:
            if (expression->type && expression->type->kind == TYPE_PTR) {
                return lower_pointer_binary(context, expression);
            }
            if (expression->kind == EXPR_SUB && expression->binary_lhs &&
                expression->binary_lhs->type &&
                expression->binary_lhs->type->kind == TYPE_PTR &&
                expression->binary_rhs && expression->binary_rhs->type &&
                expression->binary_rhs->type->kind == TYPE_PTR) {
                return lower_pointer_difference(context, expression);
            }
            return lower_integer_binary(context, expression);
        case EXPR_MUL:
        case EXPR_DIV:
        case EXPR_MOD:
        case EXPR_BITAND:
        case EXPR_BITOR:
        case EXPR_BITXOR:
        case EXPR_LSHIFT:
        case EXPR_RSHIFT:
            return lower_integer_binary(context, expression);
        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_GT:
        case EXPR_LE:
        case EXPR_GE:
            return lower_comparison(context, expression);
        case EXPR_ASSIGN:
            return lower_assignment(context, expression);
        case EXPR_ADD_ASSIGN:
        case EXPR_SUB_ASSIGN:
        case EXPR_MUL_ASSIGN:
        case EXPR_DIV_ASSIGN:
        case EXPR_MOD_ASSIGN:
        case EXPR_AND_ASSIGN:
        case EXPR_OR_ASSIGN:
        case EXPR_XOR_ASSIGN:
        case EXPR_LSHIFT_ASSIGN:
        case EXPR_RSHIFT_ASSIGN:
            return lower_compound_assignment(context, expression);
        case EXPR_COMMA:
            (void)lower_expression(context, expression->binary_lhs);
            return lower_expression(context, expression->binary_rhs);
        case EXPR_CALL:
            return lower_call(context, expression);
        case EXPR_SIZEOF:
        case EXPR_ALIGNOF:
            if (!expr_eval_integer_constant((Expr*)expression, &constant) ||
                !lower_type(expression->type, &type)) {
                context->unsupported = true;
                return lower_invalid_value();
            }
            return lower_integer_constant(context, type,
                                          expression->type->is_unsigned,
                                          (uint64_t)constant);
        case EXPR_STRING_LIT: {
            const RccIrConstant* literal;
            RccIrInstruction* address;
            size_t size;
            if (!expression->str_val) {
                context->unsupported = true;
                return lower_invalid_value();
            }
            size = strlen(expression->str_val);
            if (size == SIZE_MAX) {
                context->unsupported = true;
                return lower_invalid_value();
            }
            ++size;
            literal = rcc_ir_module_intern_constant(
                context->module, expression->str_val, size, 1u);
            address = lower_append(
                context, RCC_IR_SYMBOL_ADDRESS,
                rcc_ir_type_pointer(0u), NULL, 0u, NULL, 0u);
            if (!literal || !address) return lower_invalid_value();
            rcc_ir_set_callee(address, literal->name);
            return lower_value(
                address->result, rcc_ir_type_pointer(0u), true);
        }
        case EXPR_FLOAT_LIT:
        case EXPR_GENERIC:
        case EXPR_VA_START:
        case EXPR_VA_END:
        case EXPR_VA_COPY:
        case EXPR_VA_ARG:
            context->unsupported = true;
            return lower_invalid_value();
        case EXPR_COMPOUND: {
            RccIrLowerValue address = lower_compound_literal_address(
                context, expression);
            if (!address.valid || !expression->type) {
                return lower_invalid_value();
            }
            if (expression->type->kind == TYPE_ARRAY ||
                expression->type->kind == TYPE_STRUCT ||
                expression->type->kind == TYPE_UNION) return address;
            return lower_load_address(
                context, address, expression->type);
        }
        case EXPR_COND:
            return lower_conditional_expression(context, expression);
        case EXPR_INDEX:
            if (expression->type &&
                (expression->type->kind == TYPE_ARRAY ||
                 expression->type->kind == TYPE_STRUCT ||
                 expression->type->kind == TYPE_UNION)) {
                return lower_lvalue_address(context, expression);
            }
            return lower_load_lvalue(context, expression);
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            if (expression->type &&
                (expression->type->kind == TYPE_ARRAY ||
                 expression->type->kind == TYPE_STRUCT ||
                 expression->type->kind == TYPE_UNION)) {
                return lower_lvalue_address(context, expression);
            }
            return lower_load_lvalue(context, expression);
        case EXPR_AND:
        case EXPR_OR:
            return lower_logical_expression(context, expression);
    }
    context->unsupported = true;
    return lower_invalid_value();
}

static bool lower_branch(RccIrLowerContext* context,
                         RccIrBlockId target) {
    if (!context || target == RCC_IR_BLOCK_NONE || context->terminated) {
        return false;
    }
    if (!lower_append(context, RCC_IR_BRANCH, rcc_ir_type_void(), NULL, 0u,
                      &target, 1u)) {
        return false;
    }
    context->terminated = true;
    return true;
}

static bool lower_conditional_branch(RccIrLowerContext* context,
                                     RccIrLowerValue condition,
                                     RccIrBlockId then_target,
                                     RccIrBlockId else_target) {
    RccIrBlockId targets[2];
    condition = lower_truth(context, condition);
    if (!condition.valid) return false;
    targets[0] = then_target;
    targets[1] = else_target;
    if (!lower_append(context, RCC_IR_COND_BRANCH, rcc_ir_type_void(),
                      &condition.value, 1u, targets, 2u)) {
        return false;
    }
    context->terminated = true;
    return true;
}

static RccIrLowerValue lower_conditional_expression(
    RccIrLowerContext* context, const Expr* expression) {
    RccIrLowerValue condition;
    RccIrLowerValue then_value;
    RccIrLowerValue else_value;
    RccIrType result_type;
    RccIrBlock* then_block;
    RccIrBlock* else_block;
    RccIrBlock* merge_block;
    RccIrBlock* then_end;
    RccIrBlock* else_end;
    RccIrValue operands[2];
    RccIrBlockId targets[2];
    RccIrInstruction* phi;
    if (!expression || !expression->type ||
        !lower_type(expression->type, &result_type) ||
        result_type.kind == RCC_IR_TYPE_VOID) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    condition = lower_expression(context, expression->cond_test);
    if (!condition.valid) return lower_invalid_value();
    then_block = rcc_ir_block_add(context->function, "cond.then");
    else_block = rcc_ir_block_add(context->function, "cond.else");
    merge_block = rcc_ir_block_add(context->function, "cond.end");
    if (!then_block || !else_block || !merge_block ||
        !lower_conditional_branch(context, condition, then_block->id,
                                  else_block->id)) {
        return lower_invalid_value();
    }

    context->current = then_block;
    context->terminated = false;
    then_value = lower_expression(context, expression->cond_then);
    then_value = lower_cast(context, then_value, expression->type);
    then_end = context->current;
    if (!then_value.valid || context->terminated ||
        !lower_branch(context, merge_block->id)) {
        return lower_invalid_value();
    }

    context->current = else_block;
    context->terminated = false;
    else_value = lower_expression(context, expression->cond_else);
    else_value = lower_cast(context, else_value, expression->type);
    else_end = context->current;
    if (!else_value.valid || context->terminated ||
        !lower_branch(context, merge_block->id)) {
        return lower_invalid_value();
    }

    context->current = merge_block;
    context->terminated = false;
    operands[0] = then_value.value;
    operands[1] = else_value.value;
    targets[0] = then_end->id;
    targets[1] = else_end->id;
    phi = lower_append(context, RCC_IR_PHI, result_type, operands, 2u,
                       targets, 2u);
    if (!phi) return lower_invalid_value();
    return lower_value(phi->result, result_type,
                       expression->type->is_unsigned);
}

static RccIrLowerValue lower_logical_expression(
    RccIrLowerContext* context, const Expr* expression) {
    RccIrLowerValue left;
    RccIrLowerValue right;
    RccIrLowerValue short_value;
    RccIrType result_type;
    RccIrBlock* right_block;
    RccIrBlock* short_block;
    RccIrBlock* merge_block;
    RccIrBlock* right_end;
    RccIrBlock* short_end;
    RccIrValue operands[2];
    RccIrBlockId targets[2];
    RccIrInstruction* phi;
    bool is_and = expression && expression->kind == EXPR_AND;
    if (!expression || !expression->type ||
        !lower_type(expression->type, &result_type) ||
        result_type.kind != RCC_IR_TYPE_INTEGER) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    left = lower_expression(context, expression->binary_lhs);
    if (!left.valid) return lower_invalid_value();
    right_block = rcc_ir_block_add(context->function, "logic.rhs");
    short_block = rcc_ir_block_add(context->function, "logic.short");
    merge_block = rcc_ir_block_add(context->function, "logic.end");
    if (!right_block || !short_block || !merge_block ||
        !lower_conditional_branch(
            context, left,
            is_and ? right_block->id : short_block->id,
            is_and ? short_block->id : right_block->id)) {
        return lower_invalid_value();
    }

    context->current = right_block;
    context->terminated = false;
    right = lower_expression(context, expression->binary_rhs);
    right = lower_truth(context, right);
    right = lower_cast(context, right, expression->type);
    right_end = context->current;
    if (!right.valid || context->terminated ||
        !lower_branch(context, merge_block->id)) {
        return lower_invalid_value();
    }

    context->current = short_block;
    context->terminated = false;
    short_value = lower_integer_constant(
        context, rcc_ir_type_integer(1u), true, is_and ? 0u : 1u);
    short_value = lower_cast(context, short_value, expression->type);
    short_end = context->current;
    if (!short_value.valid || context->terminated ||
        !lower_branch(context, merge_block->id)) {
        return lower_invalid_value();
    }

    context->current = merge_block;
    context->terminated = false;
    operands[0] = right.value;
    operands[1] = short_value.value;
    targets[0] = right_end->id;
    targets[1] = short_end->id;
    phi = lower_append(context, RCC_IR_PHI, result_type, operands, 2u,
                       targets, 2u);
    if (!phi) return lower_invalid_value();
    return lower_value(phi->result, result_type,
                       expression->type->is_unsigned);
}

static bool lower_statement_has_switch_label(const Stmt* statement) {
    const StmtList* item;
    if (!statement) return false;
    switch (statement->kind) {
        case STMT_SWITCH:
            return false;
        case STMT_CASE:
        case STMT_DEFAULT:
            return true;
        case STMT_BLOCK:
            for (item = statement->block_stmts; item; item = item->next) {
                if (lower_statement_has_switch_label(item->stmt)) {
                    return true;
                }
            }
            return false;
        case STMT_IF:
            return lower_statement_has_switch_label(statement->if_then) ||
                lower_statement_has_switch_label(statement->if_else);
        case STMT_WHILE:
        case STMT_DO:
            return lower_statement_has_switch_label(statement->while_body);
        case STMT_FOR:
            return lower_statement_has_switch_label(statement->for_body);
        case STMT_LABEL:
            return lower_statement_has_switch_label(statement->label_stmt);
        default:
            return false;
    }
}

static bool lower_block(RccIrLowerContext* context, const Stmt* block) {
    if (!block || block->kind != STMT_BLOCK) {
        return lower_statement(context, block);
    }
    for (const StmtList* item = block->block_stmts; item;
         item = item->next) {
        if (context->terminated &&
            (!context->current_switch ||
             !lower_statement_has_switch_label(item->stmt))) {
            continue;
        }
        if (!lower_statement(context, item->stmt)) return false;
    }
    return !context->unsupported;
}

static bool lower_if(RccIrLowerContext* context, const Stmt* statement) {
    RccIrLowerValue condition = lower_expression(context,
                                                  statement->if_cond);
    RccIrBlock* then_block;
    RccIrBlock* else_block;
    RccIrBlock* merge_block;
    RccIrBlock* then_end;
    RccIrBlock* else_end;
    bool then_falls;
    bool else_falls;
    if (!condition.valid) return false;
    then_block = rcc_ir_block_add(context->function, "if.then");
    if (!statement->if_else) {
        merge_block = rcc_ir_block_add(context->function, "if.end");
        if (!then_block || !merge_block ||
            !lower_conditional_branch(context, condition, then_block->id,
                                      merge_block->id)) {
            return false;
        }
        context->current = then_block;
        context->terminated = false;
        if (!lower_statement(context, statement->if_then)) return false;
        if (!context->terminated && !lower_branch(context, merge_block->id)) {
            return false;
        }
        context->current = merge_block;
        context->terminated = false;
        return true;
    }
    else_block = rcc_ir_block_add(context->function, "if.else");
    if (!then_block || !else_block ||
        !lower_conditional_branch(context, condition, then_block->id,
                                  else_block->id)) {
        return false;
    }
    context->current = then_block;
    context->terminated = false;
    if (!lower_statement(context, statement->if_then)) return false;
    then_end = context->current;
    then_falls = !context->terminated;
    context->current = else_block;
    context->terminated = false;
    if (!lower_statement(context, statement->if_else)) return false;
    else_end = context->current;
    else_falls = !context->terminated;
    if (!then_falls && !else_falls) {
        context->terminated = true;
        return true;
    }
    merge_block = rcc_ir_block_add(context->function, "if.end");
    if (!merge_block) return false;
    if (then_falls) {
        context->current = then_end;
        context->terminated = false;
        if (!lower_branch(context, merge_block->id)) return false;
    }
    if (else_falls) {
        context->current = else_end;
        context->terminated = false;
        if (!lower_branch(context, merge_block->id)) return false;
    }
    context->current = merge_block;
    context->terminated = false;
    return true;
}

static bool lower_while(RccIrLowerContext* context,
                        const Stmt* statement) {
    RccIrBlock* condition_block = rcc_ir_block_add(context->function,
                                                    "while.cond");
    RccIrBlock* body_block = rcc_ir_block_add(context->function,
                                               "while.body");
    RccIrBlock* exit_block = rcc_ir_block_add(context->function,
                                               "while.end");
    RccIrBlockId old_break = context->break_target;
    RccIrBlockId old_continue = context->continue_target;
    RccIrLowerValue condition;
    if (!condition_block || !body_block || !exit_block ||
        !lower_branch(context, condition_block->id)) {
        return false;
    }
    context->current = condition_block;
    context->terminated = false;
    condition = lower_expression(context, statement->while_cond);
    if (!condition.valid || !lower_conditional_branch(
            context, condition, body_block->id, exit_block->id)) {
        return false;
    }
    context->break_target = exit_block->id;
    context->continue_target = condition_block->id;
    context->current = body_block;
    context->terminated = false;
    if (!lower_statement(context, statement->while_body)) return false;
    if (!context->terminated && !lower_branch(context, condition_block->id)) {
        return false;
    }
    context->break_target = old_break;
    context->continue_target = old_continue;
    context->current = exit_block;
    context->terminated = false;
    return true;
}

static bool lower_do(RccIrLowerContext* context, const Stmt* statement) {
    RccIrBlock* body_block = rcc_ir_block_add(context->function, "do.body");
    RccIrBlock* condition_block = rcc_ir_block_add(context->function,
                                                    "do.cond");
    RccIrBlock* exit_block = rcc_ir_block_add(context->function, "do.end");
    RccIrBlockId old_break = context->break_target;
    RccIrBlockId old_continue = context->continue_target;
    RccIrLowerValue condition;
    if (!body_block || !condition_block || !exit_block ||
        !lower_branch(context, body_block->id)) {
        return false;
    }
    context->break_target = exit_block->id;
    context->continue_target = condition_block->id;
    context->current = body_block;
    context->terminated = false;
    if (!lower_statement(context, statement->while_body)) return false;
    if (context->terminated) {
        context->unsupported = true;
        return false;
    }
    if (!context->terminated && !lower_branch(context, condition_block->id)) {
        return false;
    }
    context->current = condition_block;
    context->terminated = false;
    condition = lower_expression(context, statement->while_cond);
    if (!condition.valid || !lower_conditional_branch(
            context, condition, body_block->id, exit_block->id)) {
        return false;
    }
    context->break_target = old_break;
    context->continue_target = old_continue;
    context->current = exit_block;
    context->terminated = false;
    return true;
}

static bool lower_for(RccIrLowerContext* context, const Stmt* statement) {
    RccIrBlock* condition_block;
    RccIrBlock* body_block;
    RccIrBlock* increment_block;
    RccIrBlock* exit_block;
    RccIrBlockId old_break = context->break_target;
    RccIrBlockId old_continue = context->continue_target;
    RccIrLowerValue condition;
    if (statement->for_init &&
        !lower_statement(context, statement->for_init)) {
        return false;
    }
    condition_block = rcc_ir_block_add(context->function, "for.cond");
    body_block = rcc_ir_block_add(context->function, "for.body");
    increment_block = rcc_ir_block_add(context->function, "for.inc");
    exit_block = rcc_ir_block_add(context->function, "for.end");
    if (!condition_block || !body_block || !increment_block || !exit_block ||
        !lower_branch(context, condition_block->id)) {
        return false;
    }
    context->current = condition_block;
    context->terminated = false;
    if (statement->for_cond) {
        condition = lower_expression(context, statement->for_cond);
    } else {
        condition = lower_integer_constant(
            context, rcc_ir_type_integer(1u), true, 1u);
    }
    if (!condition.valid || !lower_conditional_branch(
            context, condition, body_block->id, exit_block->id)) {
        return false;
    }
    context->break_target = exit_block->id;
    context->continue_target = increment_block->id;
    context->current = body_block;
    context->terminated = false;
    if (!lower_statement(context, statement->for_body)) return false;
    if (context->terminated) {
        context->unsupported = true;
        return false;
    }
    if (!context->terminated && !lower_branch(context, increment_block->id)) {
        return false;
    }
    context->current = increment_block;
    context->terminated = false;
    if (statement->for_inc) {
        (void)lower_expression(context, statement->for_inc);
        if (context->unsupported) return false;
    }
    if (!lower_branch(context, condition_block->id)) return false;
    context->break_target = old_break;
    context->continue_target = old_continue;
    context->current = exit_block;
    context->terminated = false;
    return true;
}

static const Type* lower_switch_control_type(const Type* type) {
    if (!type || type->kind == TYPE_ENUM || type->kind < TYPE_INT) {
        return type_int;
    }
    return type;
}

static uint64_t lower_switch_case_bits(const Expr* expression,
                                       const Type* control_type,
                                       bool* valid) {
    int64_t value = 0;
    unsigned width;
    uint64_t bits;
    if (valid) *valid = false;
    if (!expression || !control_type || control_type->size <= 0 ||
        control_type->size > 8 ||
        !expr_eval_integer_constant((Expr*)expression, &value)) {
        return 0u;
    }
    width = (unsigned)control_type->size * 8u;
    bits = (uint64_t)value;
    if (width < 64u) bits &= (UINT64_C(1) << width) - 1u;
    if (valid) *valid = true;
    return bits;
}

static bool lower_switch_add_label(RccIrLowerContext* context,
                                   RccIrLowerSwitch* switch_context,
                                   const Stmt* statement,
                                   bool is_default) {
    RccIrLowerSwitchLabel* label;
    bool valid = true;
    if (!context || !switch_context || !statement ||
        (is_default && switch_context->default_label)) {
        if (context) context->unsupported = true;
        return false;
    }
    label = rcc_alloc(sizeof(*label));
    label->statement = statement;
    label->block = rcc_ir_block_add(
        context->function, is_default ? "switch.default" : "switch.case");
    label->case_bits = is_default ? 0u : lower_switch_case_bits(
        statement->case_val, switch_context->control_type, &valid);
    label->next = NULL;
    if (!label->block || !valid) {
        rcc_free(label);
        context->unsupported = true;
        return false;
    }
    if (is_default) {
        switch_context->default_label = label;
    } else {
        if (switch_context->labels_tail) {
            switch_context->labels_tail->next = label;
        } else {
            switch_context->labels = label;
        }
        switch_context->labels_tail = label;
    }
    return true;
}

static bool lower_collect_switch_labels(
    RccIrLowerContext* context, RccIrLowerSwitch* switch_context,
    const Stmt* statement) {
    const StmtList* item;
    if (!statement) return true;
    switch (statement->kind) {
        case STMT_SWITCH:
            return true;
        case STMT_CASE:
            return lower_switch_add_label(context, switch_context,
                                          statement, false) &&
                lower_collect_switch_labels(context, switch_context,
                                            statement->case_stmt);
        case STMT_DEFAULT:
            return lower_switch_add_label(context, switch_context,
                                          statement, true) &&
                lower_collect_switch_labels(context, switch_context,
                                            statement->default_stmt);
        case STMT_BLOCK:
            for (item = statement->block_stmts; item; item = item->next) {
                if (!lower_collect_switch_labels(
                        context, switch_context, item->stmt)) {
                    return false;
                }
            }
            return true;
        case STMT_IF:
        case STMT_WHILE:
        case STMT_DO:
        case STMT_FOR:
        case STMT_LABEL:
            if (lower_statement_has_switch_label(statement)) {
                context->unsupported = true;
                return false;
            }
            return true;
        default:
            return true;
    }
}

static void lower_release_switch_labels(RccIrLowerSwitch* switch_context) {
    RccIrLowerSwitchLabel* label;
    if (!switch_context) return;
    label = switch_context->labels;
    while (label) {
        RccIrLowerSwitchLabel* next = label->next;
        rcc_free(label);
        label = next;
    }
    rcc_free(switch_context->default_label);
    switch_context->labels = NULL;
    switch_context->labels_tail = NULL;
    switch_context->default_label = NULL;
}

static RccIrLowerSwitchLabel* lower_find_switch_label(
    RccIrLowerSwitch* switch_context, const Stmt* statement) {
    RccIrLowerSwitchLabel* label;
    if (!switch_context || !statement) return NULL;
    if (statement->kind == STMT_DEFAULT) {
        label = switch_context->default_label;
        return label && label->statement == statement ? label : NULL;
    }
    for (label = switch_context->labels; label; label = label->next) {
        if (label->statement == statement) return label;
    }
    return NULL;
}

static bool lower_switch_case(RccIrLowerContext* context,
                              const Stmt* statement) {
    RccIrLowerSwitchLabel* label = lower_find_switch_label(
        context ? context->current_switch : NULL, statement);
    const Stmt* child;
    if (!context || !label) {
        if (context) context->unsupported = true;
        return false;
    }
    if (!context->terminated && context->current != label->block &&
        !lower_branch(context, label->block->id)) {
        return false;
    }
    context->current = label->block;
    context->terminated = false;
    child = statement->kind == STMT_CASE
        ? statement->case_stmt : statement->default_stmt;
    return lower_statement(context, child);
}

static bool lower_switch(RccIrLowerContext* context,
                         const Stmt* statement) {
    RccIrLowerSwitch switch_context;
    RccIrLowerSwitchLabel* label;
    const Type* control_type;
    RccIrLowerValue control;
    RccIrLowerValue guard;
    RccIrType ir_control_type;
    RccIrBlock* dispatch_block;
    RccIrBlock* shadow_block;
    RccIrBlock* scan_block;
    RccIrBlock* exit_block;
    RccIrBlockId old_break;
    RccIrValue compare_operands[2];
    bool body_ok;
    memset(&switch_context, 0, sizeof(switch_context));
    if (!context || !statement || !statement->switch_expr ||
        !statement->switch_expr->type) {
        if (context) context->unsupported = true;
        return false;
    }
    control_type = lower_switch_control_type(statement->switch_expr->type);
    if (!lower_type(control_type, &ir_control_type) ||
        ir_control_type.kind != RCC_IR_TYPE_INTEGER) {
        context->unsupported = true;
        return false;
    }
    switch_context.control_type = control_type;
    switch_context.previous = context->current_switch;
    if (!lower_collect_switch_labels(
            context, &switch_context, statement->switch_body)) {
        lower_release_switch_labels(&switch_context);
        return false;
    }
    control = lower_expression(context, statement->switch_expr);
    control = lower_cast(context, control, control_type);
    dispatch_block = rcc_ir_block_add(context->function, "switch.dispatch");
    shadow_block = rcc_ir_block_add(context->function, "switch.shadow");
    scan_block = rcc_ir_block_add(context->function, "switch.scan");
    exit_block = rcc_ir_block_add(context->function, "switch.end");
    guard = lower_integer_constant(
        context, rcc_ir_type_integer(1u), true, 1u);
    if (!control.valid || !dispatch_block || !shadow_block || !scan_block ||
        !exit_block || !guard.valid || !lower_conditional_branch(
            context, guard, dispatch_block->id, shadow_block->id)) {
        lower_release_switch_labels(&switch_context);
        return false;
    }
    context->current = shadow_block;
    context->terminated = false;
    if (!lower_conditional_branch(context, guard, scan_block->id,
                                  exit_block->id)) {
        lower_release_switch_labels(&switch_context);
        return false;
    }

    context->current = dispatch_block;
    context->terminated = false;
    for (label = switch_context.labels; label; label = label->next) {
        RccIrLowerValue case_value = lower_integer_constant(
            context, ir_control_type, control_type->is_unsigned,
            label->case_bits);
        RccIrInstruction* compare;
        RccIrLowerValue condition;
        RccIrBlock* next_dispatch = label->next
            ? rcc_ir_block_add(context->function, "switch.dispatch") : NULL;
        RccIrBlockId miss_target = next_dispatch
            ? next_dispatch->id
            : (switch_context.default_label
                   ? switch_context.default_label->block->id
                   : exit_block->id);
        if (!case_value.valid || (label->next && !next_dispatch)) {
            lower_release_switch_labels(&switch_context);
            return false;
        }
        compare_operands[0] = control.value;
        compare_operands[1] = case_value.value;
        compare = lower_append(context, RCC_IR_ICMP,
                               rcc_ir_type_integer(1u), compare_operands,
                               2u, NULL, 0u);
        if (!compare) {
            lower_release_switch_labels(&switch_context);
            return false;
        }
        rcc_ir_set_predicate(compare, RCC_IR_ICMP_EQ);
        condition = lower_value(compare->result,
                                rcc_ir_type_integer(1u), true);
        if (!lower_conditional_branch(context, condition, label->block->id,
                                      miss_target)) {
            lower_release_switch_labels(&switch_context);
            return false;
        }
        if (next_dispatch) {
            context->current = next_dispatch;
            context->terminated = false;
        }
    }
    if (!switch_context.labels) {
        RccIrBlockId target = switch_context.default_label
            ? switch_context.default_label->block->id : exit_block->id;
        if (!lower_branch(context, target)) {
            lower_release_switch_labels(&switch_context);
            return false;
        }
    }

    old_break = context->break_target;
    context->break_target = exit_block->id;
    context->current_switch = &switch_context;
    context->current = scan_block;
    context->terminated = false;
    body_ok = lower_statement(context, statement->switch_body);
    if (body_ok && !context->terminated) {
        body_ok = lower_branch(context, exit_block->id);
    }
    context->current_switch = switch_context.previous;
    context->break_target = old_break;
    lower_release_switch_labels(&switch_context);
    if (!body_ok) return false;
    context->current = exit_block;
    context->terminated = false;
    return true;
}

static RccIrLowerValue lower_array_element_address(
    RccIrLowerContext* context, RccIrValue base,
    const Type* element_type, uint64_t index) {
    RccIrType index_type;
    RccIrLowerValue index_value;
    RccIrValue operands[2];
    RccIrInstruction* address;
    if (!element_type || element_type->size <= 0 ||
        (uint64_t)element_type->size > (uint64_t)INT32_MAX ||
        !lower_type(type_long, &index_type) ||
        index_type.kind != RCC_IR_TYPE_INTEGER) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    index_value = lower_integer_constant(
        context, index_type, true, index);
    if (!index_value.valid) return lower_invalid_value();
    operands[0] = base;
    operands[1] = index_value.value;
    address = lower_append(context, RCC_IR_GEP,
                           rcc_ir_type_pointer(0u), operands, 2u,
                           NULL, 0u);
    if (!address) return lower_invalid_value();
    rcc_ir_set_immediate(address, (uint64_t)element_type->size);
    return lower_value(
        address->result, rcc_ir_type_pointer(0u), true);
}

static bool lower_copy_array_storage(
    RccIrLowerContext* context, RccIrValue destination,
    RccIrValue source, const Type* array_type) {
    const Type* element_type;
    if (!array_type || array_type->kind != TYPE_ARRAY ||
        array_type->array_len <= 0 || array_type->array_len > 4096 ||
        array_type->size <= 0 || array_type->size > 65536 ||
        !array_type->base) {
        context->unsupported = true;
        return false;
    }
    element_type = array_type->base;
    for (int index = 0; index < array_type->array_len; ++index) {
        RccIrLowerValue destination_element = lower_array_element_address(
            context, destination, element_type, (uint64_t)index);
        RccIrLowerValue source_element = lower_array_element_address(
            context, source, element_type, (uint64_t)index);
        if (!destination_element.valid || !source_element.valid) return false;
        if (element_type->kind == TYPE_ARRAY) {
            if (!lower_copy_array_storage(
                    context, destination_element.value,
                    source_element.value, element_type)) return false;
        } else if (element_type->kind == TYPE_STRUCT) {
            if (!lower_copy_struct_storage(
                    context, destination_element.value,
                    source_element.value, element_type)) return false;
        } else if (element_type->kind == TYPE_UNION) {
            if (!lower_copy_union_storage(
                    context, destination_element.value,
                    source_element.value, element_type)) return false;
        } else {
            RccIrLowerValue value = lower_load_address(
                context, source_element, element_type);
            if (!value.valid || !lower_store_address(
                    context, destination_element, value)) return false;
        }
    }
    return true;
}

static RccIrLowerValue lower_zero_initializer(
    RccIrLowerContext* context, const Type* type) {
    RccIrType ir_type;
    RccIrLowerValue zero;
    if (!lower_type(type, &ir_type) || ir_type.kind == RCC_IR_TYPE_VOID) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    if (ir_type.kind == RCC_IR_TYPE_INTEGER) {
        return lower_integer_constant(
            context, ir_type, type->is_unsigned, 0u);
    }
    if (ir_type.kind == RCC_IR_TYPE_POINTER) {
        RccIrType integer_type;
        if (!lower_type(type_long, &integer_type)) {
            context->unsupported = true;
            return lower_invalid_value();
        }
        zero = lower_integer_constant(
            context, integer_type, true, 0u);
        return lower_cast(context, zero, type);
    }
    context->unsupported = true;
    return lower_invalid_value();
}

static const Expr* lower_scalar_initializer_expression(
    const Expr* initializer) {
    while (initializer && initializer->kind == EXPR_COMPOUND) {
        const ExprList* item = initializer->compound_init;
        if (!item || item->next ||
            item->designator_kind != INIT_DESIGNATOR_NONE) return NULL;
        initializer = item->expr;
    }
    return initializer;
}

static const Expr* lower_character_array_string(
    const Type* array_type, const Expr* initializer) {
    if (!array_type || array_type->kind != TYPE_ARRAY ||
        !array_type->base || array_type->base->kind != TYPE_CHAR ||
        !initializer) return NULL;
    if (initializer->kind == EXPR_STRING_LIT) return initializer;
    if (initializer->kind == EXPR_COMPOUND &&
        initializer->compound_init &&
        !initializer->compound_init->next &&
        initializer->compound_init->designator_kind ==
            INIT_DESIGNATOR_NONE &&
        initializer->compound_init->expr &&
        initializer->compound_init->expr->kind == EXPR_STRING_LIT) {
        return initializer->compound_init->expr;
    }
    return NULL;
}

static bool lower_storage_type_supported_internal(
    const Type* type, unsigned depth) {
    RccIrType ir_type;
    const TypeField* field;
    if (!type || depth >= 32u || type->size <= 0 ||
        type->size > 65536 || type->is_reference || type->is_volatile ||
        type->cleanup_function) return false;
    if (type->kind == TYPE_ARRAY) {
        return type->array_len > 0 && type->array_len <= 4096 &&
            type->base && lower_storage_type_supported_internal(
                type->base, depth + 1u);
    }
    if (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) {
        if (!type->is_complete) return false;
        for (field = type->fields; field; field = field->next) {
            if (!field->type || field->offset < 0 ||
                field->offset > type->size ||
                field->type->size > type->size - field->offset ||
                !lower_storage_type_supported_internal(
                    field->type, depth + 1u)) return false;
        }
        return true;
    }
    return lower_type(type, &ir_type) &&
        ir_type.kind != RCC_IR_TYPE_VOID;
}

static bool lower_abi_is_aggregate(const Type* type) {
    return type && (type->kind == TYPE_STRUCT ||
                    type->kind == TYPE_UNION);
}

static bool lower_abi_naturally_aligned_internal(
    const Type* type, uint64_t offset, unsigned depth) {
    const TypeField* field;
    if (!type || depth >= 32u || type->size <= 0) return false;
    if (type->kind == TYPE_ARRAY) {
        if (!type->base || type->array_len <= 0) return false;
        if (type->base->align > 1 &&
            (offset % (uint64_t)type->base->align != 0u ||
             type->base->size % type->base->align != 0)) return false;
        return lower_abi_naturally_aligned_internal(
            type->base, offset, depth + 1u);
    }
    if (lower_abi_is_aggregate(type)) {
        for (field = type->fields; field; field = field->next) {
            uint64_t field_offset;
            if (!field->type || field->offset < 0 ||
                (uint64_t)field->offset > UINT64_MAX - offset) {
                return false;
            }
            field_offset = offset + (uint64_t)field->offset;
            if (field->type->align > 1 &&
                field_offset % (uint64_t)field->type->align != 0u) {
                return false;
            }
            if (!lower_abi_naturally_aligned_internal(
                    field->type, field_offset, depth + 1u)) return false;
        }
    }
    return true;
}

static bool lower_abi_integer_class_internal(const Type* type,
                                             unsigned depth) {
    const TypeField* field;
    if (!type || depth >= 32u) return false;
    if (type->kind == TYPE_ARRAY) {
        return type->base && type->array_len > 0 &&
            lower_abi_integer_class_internal(type->base, depth + 1u);
    }
    if (lower_abi_is_aggregate(type)) {
        for (field = type->fields; field; field = field->next) {
            if (!lower_abi_integer_class_internal(
                    field->type, depth + 1u)) return false;
        }
        return true;
    }
    if (type->kind == TYPE_PTR || type->kind == TYPE_ENUM) return true;
    return type_is_integer((Type*)type);
}

static bool lower_abi_aggregate_supported(const Type* type) {
    if (!lower_abi_is_aggregate(type) ||
        !lower_storage_type_supported_internal(type, 0u)) return false;
    if (g_opts.target_arch != ARCH_X64) return true;
    return type->size <= 16 &&
        lower_abi_naturally_aligned_internal(type, 0u, 0u) &&
        lower_abi_integer_class_internal(type, 0u);
}

static int lower_abi_return_layout(const Type* type,
                                   RccIrType* return_type) {
    if (!lower_abi_is_aggregate(type) || !return_type ||
        !lower_storage_type_supported_internal(type, 0u)) {
        return LOWER_ABI_RETURN_UNSUPPORTED;
    }
    if (g_opts.target_arch != ARCH_X64) {
        *return_type = rcc_ir_type_pointer(0u);
        return LOWER_ABI_RETURN_SRET;
    }
    if (type->size <= 8 &&
        lower_abi_naturally_aligned_internal(type, 0u, 0u) &&
        lower_abi_integer_class_internal(type, 0u)) {
        *return_type = rcc_ir_type_integer(64u);
        return LOWER_ABI_RETURN_REGISTER_AGGREGATE;
    }
    if (type->size <= 16 &&
        lower_abi_naturally_aligned_internal(type, 0u, 0u) &&
        lower_abi_integer_class_internal(type, 0u)) {
        *return_type = rcc_ir_type_integer(64u);
        return LOWER_ABI_RETURN_REGISTER_PAIR;
    }
    if (type->size > 16 ||
        !lower_abi_naturally_aligned_internal(type, 0u, 0u)) {
        *return_type = rcc_ir_type_pointer(0u);
        return LOWER_ABI_RETURN_SRET;
    }
    return LOWER_ABI_RETURN_UNSUPPORTED;
}

static bool lower_abi_native_scalar_type(
    const Type* type, RccIrType* ir_type) {
    if (!lower_type(type, ir_type) ||
        ir_type->kind == RCC_IR_TYPE_VOID) return false;
    if (ir_type->kind == RCC_IR_TYPE_POINTER) return true;
    return ir_type->kind == RCC_IR_TYPE_INTEGER &&
        ir_type->bit_width <=
            (uint16_t)(g_opts.target_arch == ARCH_X64 ? 64u : 32u);
}

static size_t lower_abi_chunk_size(void) {
    return g_opts.target_arch == ARCH_X64 ? 8u : 4u;
}

static bool lower_abi_parameter_layout(
    const TypeParam* parameters, const RccIrType* prefix_types,
    size_t prefix_count, RccIrType** types_out, size_t* count_out) {
    const TypeParam* parameter;
    RccIrType* types = NULL;
    size_t count = prefix_count;
    size_t cursor = 0u;
    size_t chunk_size = lower_abi_chunk_size();
    if (types_out) *types_out = NULL;
    if (count_out) *count_out = 0u;
    if (!count_out || (prefix_count != 0u && !prefix_types)) return false;
    for (parameter = parameters; parameter; parameter = parameter->next) {
        size_t units = 1u;
        RccIrType scalar_type;
        if (lower_abi_is_aggregate(parameter->type)) {
            if (!lower_abi_aggregate_supported(parameter->type)) return false;
            units = ((size_t)parameter->type->size + chunk_size - 1u) /
                chunk_size;
            if (g_opts.target_arch == ARCH_X64 && count < 6u &&
                units > 6u - count) return false;
        } else if (!lower_abi_native_scalar_type(
                       parameter->type, &scalar_type)) {
            return false;
        }
        if (units > SIZE_MAX - count) return false;
        count += units;
    }
    if (types_out && count != 0u) {
        if (count > SIZE_MAX / sizeof(*types)) return false;
        types = rcc_alloc(count * sizeof(*types));
        if (prefix_count != 0u) {
            memcpy(types, prefix_types,
                   prefix_count * sizeof(*types));
        }
    }
    cursor = prefix_count;
    for (parameter = parameters; parameter; parameter = parameter->next) {
        if (lower_abi_is_aggregate(parameter->type)) {
            size_t units = ((size_t)parameter->type->size +
                            chunk_size - 1u) / chunk_size;
            RccIrType chunk_type = rcc_ir_type_integer(
                (uint16_t)(chunk_size * 8u));
            for (size_t unit = 0u; unit < units; ++unit) {
                if (types) types[cursor] = chunk_type;
                ++cursor;
            }
        } else {
            RccIrType scalar_type;
            if (!lower_abi_native_scalar_type(
                    parameter->type, &scalar_type)) {
                rcc_free(types);
                return false;
            }
            if (types) types[cursor] = scalar_type;
            ++cursor;
        }
    }
    if (cursor != count) {
        rcc_free(types);
        return false;
    }
    if (types_out) *types_out = types;
    *count_out = count;
    return true;
}

static RccIrLowerValue lower_load_aggregate_chunk(
    RccIrLowerContext* context, RccIrLowerValue base,
    const Type* aggregate_type, size_t offset) {
    size_t chunk_size = lower_abi_chunk_size();
    size_t remaining;
    const Type* chunk_ast_type = g_opts.target_arch == ARCH_X64
        ? type_ulong : type_uint;
    RccIrType chunk_type;
    RccIrLowerValue address;
    RccIrLowerValue result;
    if (!base.valid || base.type.kind != RCC_IR_TYPE_POINTER ||
        !aggregate_type || aggregate_type->size <= 0 ||
        offset >= (size_t)aggregate_type->size ||
        !lower_type(chunk_ast_type, &chunk_type)) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    address = lower_byte_offset_address(context, base, (uint64_t)offset);
    remaining = (size_t)aggregate_type->size - offset;
    if (remaining >= chunk_size) {
        return lower_load_address(context, address, chunk_ast_type);
    }
    result = lower_integer_constant(context, chunk_type, true, 0u);
    for (size_t byte = 0u; byte < remaining; ++byte) {
        RccIrLowerValue byte_address = lower_byte_offset_address(
            context, address, (uint64_t)byte);
        RccIrLowerValue byte_value = lower_load_address(
            context, byte_address, type_uchar);
        RccIrValue operands[2];
        RccIrInstruction* operation;
        byte_value = lower_cast(context, byte_value, chunk_ast_type);
        if (!byte_value.valid || !result.valid) {
            return lower_invalid_value();
        }
        if (byte != 0u) {
            RccIrLowerValue shift = lower_integer_constant(
                context, chunk_type, true, (uint64_t)(byte * 8u));
            operands[0] = byte_value.value;
            operands[1] = shift.value;
            operation = lower_append(
                context, RCC_IR_SHL, chunk_type,
                operands, 2u, NULL, 0u);
            if (!operation) return lower_invalid_value();
            byte_value = lower_value(
                operation->result, chunk_type, true);
        }
        operands[0] = result.value;
        operands[1] = byte_value.value;
        operation = lower_append(
            context, RCC_IR_OR, chunk_type,
            operands, 2u, NULL, 0u);
        if (!operation) return lower_invalid_value();
        result = lower_value(operation->result, chunk_type, true);
    }
    return result;
}

static bool lower_zero_array_storage(
    RccIrLowerContext* context, RccIrValue base,
    const Type* array_type) {
    if (!array_type || array_type->kind != TYPE_ARRAY ||
        array_type->array_len <= 0 || array_type->array_len > 4096 ||
        array_type->size <= 0 || array_type->size > 65536 ||
        !array_type->base) {
        context->unsupported = true;
        return false;
    }
    for (int index = 0; index < array_type->array_len; ++index) {
        RccIrLowerValue address = lower_array_element_address(
            context, base, array_type->base, (uint64_t)index);
        if (!address.valid) return false;
        if (array_type->base->kind == TYPE_ARRAY) {
            if (!lower_zero_array_storage(
                    context, address.value, array_type->base)) return false;
        } else if (array_type->base->kind == TYPE_STRUCT) {
            if (!lower_zero_struct_storage(
                    context, address.value, array_type->base)) return false;
        } else if (array_type->base->kind == TYPE_UNION) {
            if (!lower_zero_union_storage(
                    context, address.value, array_type->base)) return false;
        } else {
            RccIrLowerValue zero = lower_zero_initializer(
                context, array_type->base);
            if (!zero.valid ||
                !lower_store_address(context, address, zero)) return false;
        }
    }
    return true;
}

static bool lower_array_initializer(
    RccIrLowerContext* context, RccIrValue base,
    const Type* array_type, const Expr* initializer) {
    const ExprList* item;
    const Expr* string;
    int64_t cursor = 0;
    if (!array_type || array_type->kind != TYPE_ARRAY ||
        array_type->array_len <= 0 || array_type->array_len > 4096 ||
        array_type->size <= 0 || array_type->size > 65536 ||
        !array_type->base || !initializer) {
        context->unsupported = true;
        return false;
    }
    string = lower_character_array_string(array_type, initializer);
    if (!string && initializer->kind != EXPR_COMPOUND) {
        context->unsupported = true;
        return false;
    }
    if (!lower_zero_array_storage(context, base, array_type)) return false;
    if (string) {
        size_t text_size = strlen(string->str_val);
        size_t copy_size;
        if (text_size == SIZE_MAX) {
            context->unsupported = true;
            return false;
        }
        ++text_size;
        copy_size = text_size < (size_t)array_type->array_len
            ? text_size : (size_t)array_type->array_len;
        for (size_t index = 0u; index < copy_size; ++index) {
            RccIrLowerValue address = lower_array_element_address(
                context, base, array_type->base, index);
            RccIrType element_type;
            RccIrLowerValue value;
            if (!lower_type(array_type->base, &element_type)) {
                context->unsupported = true;
                return false;
            }
            value = lower_integer_constant(
                context, element_type, array_type->base->is_unsigned,
                (uint8_t)string->str_val[index]);
            if (!address.valid || !value.valid ||
                !lower_store_address(context, address, value)) return false;
        }
        return true;
    }
    for (item = initializer->compound_init; item; item = item->next) {
        const Expr* expression;
        RccIrLowerValue address;
        RccIrLowerValue value;
        if (item->designator_kind == INIT_DESIGNATOR_FIELD) {
            context->unsupported = true;
            return false;
        }
        if (item->designator_kind == INIT_DESIGNATOR_INDEX) {
            cursor = item->designator_index;
        }
        if (cursor < 0 || cursor >= array_type->array_len) {
            context->unsupported = true;
            return false;
        }
        address = lower_array_element_address(
            context, base, array_type->base, (uint64_t)cursor);
        if (array_type->base->kind == TYPE_ARRAY) {
            if (!address.valid || !lower_array_initializer(
                    context, address.value, array_type->base,
                    item->expr)) return false;
            ++cursor;
            continue;
        }
        if (array_type->base->kind == TYPE_STRUCT) {
            if (!address.valid || !lower_initialize_struct_storage(
                    context, address.value, array_type->base,
                    item->expr)) return false;
            ++cursor;
            continue;
        }
        if (array_type->base->kind == TYPE_UNION) {
            if (!address.valid || !lower_initialize_union_storage(
                    context, address.value, array_type->base,
                    item->expr)) return false;
            ++cursor;
            continue;
        }
        expression = lower_scalar_initializer_expression(item->expr);
        if (!expression) {
            context->unsupported = true;
            return false;
        }
        value = lower_expression(context, expression);
        value = lower_cast(context, value, array_type->base);
        if (!address.valid || !value.valid ||
            !lower_store_address(context, address, value)) return false;
        ++cursor;
    }
    return true;
}

static bool lower_struct_type_supported(const Type* type) {
    return type && type->kind == TYPE_STRUCT &&
        lower_storage_type_supported_internal(type, 0u);
}

static bool lower_union_type_supported(const Type* type) {
    return type && type->kind == TYPE_UNION &&
        lower_storage_type_supported_internal(type, 0u);
}

static bool lower_copy_union_storage(
    RccIrLowerContext* context, RccIrValue destination,
    RccIrValue source, const Type* type) {
    if (!lower_union_type_supported(type)) {
        context->unsupported = true;
        return false;
    }
    for (int offset = 0; offset < type->size; ++offset) {
        RccIrLowerValue destination_base = lower_value(
            destination, rcc_ir_type_pointer(0u), true);
        RccIrLowerValue source_base = lower_value(
            source, rcc_ir_type_pointer(0u), true);
        RccIrLowerValue destination_address = lower_byte_offset_address(
            context, destination_base, (uint64_t)offset);
        RccIrLowerValue source_address = lower_byte_offset_address(
            context, source_base, (uint64_t)offset);
        RccIrLowerValue value = lower_load_address(
            context, source_address, type_uchar);
        if (!destination_address.valid || !source_address.valid ||
            !value.valid || !lower_store_address(
                context, destination_address, value)) return false;
    }
    return true;
}

static bool lower_copy_struct_storage(
    RccIrLowerContext* context, RccIrValue destination,
    RccIrValue source, const Type* type) {
    const TypeField* field;
    if (!lower_struct_type_supported(type)) {
        context->unsupported = true;
        return false;
    }
    for (field = type->fields; field; field = field->next) {
        RccIrLowerValue destination_base = lower_value(
            destination, rcc_ir_type_pointer(0u), true);
        RccIrLowerValue source_base = lower_value(
            source, rcc_ir_type_pointer(0u), true);
        RccIrLowerValue destination_address = lower_byte_offset_address(
            context, destination_base, (uint64_t)field->offset);
        RccIrLowerValue source_address = lower_byte_offset_address(
            context, source_base, (uint64_t)field->offset);
        if (!destination_address.valid || !source_address.valid) return false;
        if (field->type->kind == TYPE_ARRAY) {
            if (!lower_copy_array_storage(
                    context, destination_address.value,
                    source_address.value, field->type)) return false;
        } else if (field->type->kind == TYPE_STRUCT) {
            if (!lower_copy_struct_storage(
                    context, destination_address.value,
                    source_address.value, field->type)) return false;
        } else if (field->type->kind == TYPE_UNION) {
            if (!lower_copy_union_storage(
                    context, destination_address.value,
                    source_address.value, field->type)) return false;
        } else {
            RccIrLowerValue value = lower_load_address(
                context, source_address, field->type);
            if (!value.valid || !lower_store_address(
                    context, destination_address, value)) return false;
        }
    }
    return true;
}

static const TypeField* lower_struct_field(
    const Type* type, const char* name) {
    const TypeField* field;
    if (!type || !name) return NULL;
    for (field = type->fields; field; field = field->next) {
        if (field->name && strcmp(field->name, name) == 0) return field;
    }
    return NULL;
}

static bool lower_zero_struct_storage(
    RccIrLowerContext* context, RccIrValue base,
    const Type* type) {
    const TypeField* field;
    if (!lower_struct_type_supported(type)) {
        context->unsupported = true;
        return false;
    }
    for (field = type->fields; field; field = field->next) {
        RccIrLowerValue base_value = lower_value(
            base, rcc_ir_type_pointer(0u), true);
        RccIrLowerValue address = lower_byte_offset_address(
            context, base_value, (uint64_t)field->offset);
        if (!address.valid) return false;
        if (field->type->kind == TYPE_ARRAY) {
            if (!lower_zero_array_storage(
                    context, address.value, field->type)) return false;
        } else if (field->type->kind == TYPE_STRUCT) {
            if (!lower_zero_struct_storage(
                    context, address.value, field->type)) return false;
        } else if (field->type->kind == TYPE_UNION) {
            if (!lower_zero_union_storage(
                    context, address.value, field->type)) return false;
        } else {
            RccIrLowerValue zero = lower_zero_initializer(
                context, field->type);
            if (!zero.valid ||
                !lower_store_address(context, address, zero)) return false;
        }
    }
    return true;
}

static bool lower_struct_initializer(
    RccIrLowerContext* context, RccIrValue base,
    const Type* type, const Expr* initializer) {
    const TypeField* cursor;
    const ExprList* item;
    if (!lower_struct_type_supported(type) || !initializer ||
        initializer->kind != EXPR_COMPOUND ||
        !lower_zero_struct_storage(context, base, type)) {
        context->unsupported = true;
        return false;
    }
    cursor = type->fields;
    for (item = initializer->compound_init; item; item = item->next) {
        const TypeField* field = cursor;
        const Expr* expression;
        RccIrLowerValue base_value;
        RccIrLowerValue address;
        RccIrLowerValue value;
        if (item->designator_kind == INIT_DESIGNATOR_INDEX) {
            context->unsupported = true;
            return false;
        }
        if (item->designator_kind == INIT_DESIGNATOR_FIELD) {
            field = lower_struct_field(type, item->designator_field);
        }
        if (!field) {
            context->unsupported = true;
            return false;
        }
        base_value = lower_value(base, rcc_ir_type_pointer(0u), true);
        address = lower_byte_offset_address(
            context, base_value, (uint64_t)field->offset);
        if (field->type->kind == TYPE_ARRAY) {
            if (!address.valid || !lower_array_initializer(
                    context, address.value, field->type,
                    item->expr)) return false;
            cursor = field->next;
            continue;
        }
        if (field->type->kind == TYPE_STRUCT) {
            if (!address.valid || !lower_initialize_struct_storage(
                    context, address.value, field->type,
                    item->expr)) return false;
            cursor = field->next;
            continue;
        }
        if (field->type->kind == TYPE_UNION) {
            if (!address.valid || !lower_initialize_union_storage(
                    context, address.value, field->type,
                    item->expr)) return false;
            cursor = field->next;
            continue;
        }
        expression = lower_scalar_initializer_expression(item->expr);
        if (!expression) {
            context->unsupported = true;
            return false;
        }
        value = lower_expression(context, expression);
        value = lower_cast(context, value, field->type);
        if (!address.valid || !value.valid ||
            !lower_store_address(context, address, value)) return false;
        cursor = field->next;
    }
    return true;
}

static bool lower_initialize_struct_storage(
    RccIrLowerContext* context, RccIrValue base,
    const Type* type, const Expr* initializer) {
    RccIrLowerValue source;
    if (!lower_struct_type_supported(type) || !initializer) {
        context->unsupported = true;
        return false;
    }
    if (initializer->kind == EXPR_COMPOUND) {
        return lower_struct_initializer(
            context, base, type, initializer);
    }
    if (!initializer->type || initializer->type->kind != TYPE_STRUCT ||
        !type_is_compatible((Type*)type, initializer->type)) {
        context->unsupported = true;
        return false;
    }
    source = lower_expression(context, initializer);
    if (!source.valid || source.type.kind != RCC_IR_TYPE_POINTER) {
        context->unsupported = true;
        return false;
    }
    return lower_copy_struct_storage(
        context, base, source.value, type);
}

static bool lower_zero_union_storage(
    RccIrLowerContext* context, RccIrValue base,
    const Type* type) {
    if (!lower_union_type_supported(type)) {
        context->unsupported = true;
        return false;
    }
    for (int offset = 0; offset < type->size; ++offset) {
        RccIrLowerValue base_value = lower_value(
            base, rcc_ir_type_pointer(0u), true);
        RccIrLowerValue address = lower_byte_offset_address(
            context, base_value, (uint64_t)offset);
        RccIrLowerValue zero = lower_zero_initializer(
            context, type_uchar);
        if (!address.valid || !zero.valid ||
            !lower_store_address(context, address, zero)) return false;
    }
    return true;
}

static bool lower_union_initializer(
    RccIrLowerContext* context, RccIrValue base,
    const Type* type, const Expr* initializer) {
    const ExprList* item;
    const TypeField* field;
    RccIrLowerValue base_value;
    RccIrLowerValue address;
    const Expr* expression;
    RccIrLowerValue value;
    if (!lower_union_type_supported(type) || !initializer ||
        initializer->kind != EXPR_COMPOUND ||
        !lower_zero_union_storage(context, base, type)) {
        context->unsupported = true;
        return false;
    }
    item = initializer->compound_init;
    if (!item) return true;
    if (item->next || item->designator_kind == INIT_DESIGNATOR_INDEX) {
        context->unsupported = true;
        return false;
    }
    field = type->fields;
    if (item->designator_kind == INIT_DESIGNATOR_FIELD) {
        field = lower_struct_field(type, item->designator_field);
    }
    if (!field) {
        context->unsupported = true;
        return false;
    }
    base_value = lower_value(base, rcc_ir_type_pointer(0u), true);
    address = lower_byte_offset_address(
        context, base_value, (uint64_t)field->offset);
    if (!address.valid) return false;
    if (field->type->kind == TYPE_ARRAY) {
        return lower_array_initializer(
            context, address.value, field->type, item->expr);
    }
    if (field->type->kind == TYPE_STRUCT) {
        return lower_initialize_struct_storage(
            context, address.value, field->type, item->expr);
    }
    if (field->type->kind == TYPE_UNION) {
        return lower_initialize_union_storage(
            context, address.value, field->type, item->expr);
    }
    expression = lower_scalar_initializer_expression(item->expr);
    if (!expression) {
        context->unsupported = true;
        return false;
    }
    value = lower_expression(context, expression);
    value = lower_cast(context, value, field->type);
    return value.valid && lower_store_address(context, address, value);
}

static bool lower_initialize_union_storage(
    RccIrLowerContext* context, RccIrValue base,
    const Type* type, const Expr* initializer) {
    RccIrLowerValue source;
    if (!lower_union_type_supported(type) || !initializer) {
        context->unsupported = true;
        return false;
    }
    if (initializer->kind == EXPR_COMPOUND) {
        return lower_union_initializer(context, base, type, initializer);
    }
    if (!initializer->type || initializer->type->kind != TYPE_UNION ||
        !type_is_compatible((Type*)type, initializer->type)) {
        context->unsupported = true;
        return false;
    }
    source = lower_expression(context, initializer);
    if (!source.valid || source.type.kind != RCC_IR_TYPE_POINTER) {
        context->unsupported = true;
        return false;
    }
    return lower_copy_union_storage(
        context, base, source.value, type);
}

static RccIrLowerValue lower_compound_literal_address(
    RccIrLowerContext* context, const Expr* expression) {
    const Type* type;
    RccIrInstruction* allocation;
    if (!context || !expression || expression->kind != EXPR_COMPOUND) {
        if (context) context->unsupported = true;
        return lower_invalid_value();
    }
    type = expression->compound_type
        ? expression->compound_type : expression->type;
    if (!type || type->size <= 0 || type->size > 65536 ||
        type->is_reference || type->is_volatile ||
        type->cleanup_function) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    allocation = lower_append(
        context, RCC_IR_ALLOCA, rcc_ir_type_pointer(0u),
        NULL, 0u, NULL, 0u);
    if (!allocation) return lower_invalid_value();
    rcc_ir_set_immediate(allocation, (uint64_t)type->size);
    if (type->kind == TYPE_ARRAY) {
        if (!lower_array_initializer(
                context, allocation->result, type, expression)) {
            return lower_invalid_value();
        }
    } else if (type->kind == TYPE_STRUCT) {
        if (!lower_initialize_struct_storage(
                context, allocation->result, type, expression)) {
            return lower_invalid_value();
        }
    } else if (type->kind == TYPE_UNION) {
        if (!lower_initialize_union_storage(
                context, allocation->result, type, expression)) {
            return lower_invalid_value();
        }
    } else {
        const Expr* initializer = lower_scalar_initializer_expression(
            expression);
        RccIrLowerValue value;
        RccIrLowerValue address;
        if (!initializer) {
            context->unsupported = true;
            return lower_invalid_value();
        }
        value = lower_expression(context, initializer);
        value = lower_cast(context, value, type);
        address = lower_value(
            allocation->result, rcc_ir_type_pointer(0u), true);
        if (!value.valid ||
            !lower_store_address(context, address, value)) {
            return lower_invalid_value();
        }
    }
    return lower_value(
        allocation->result, rcc_ir_type_pointer(0u), true);
}

static bool lower_declaration(RccIrLowerContext* context,
                              const Decl* declaration) {
    RccIrType type = rcc_ir_type_void();
    RccIrInstruction* allocation;
    bool is_array = declaration && declaration->type &&
        declaration->type->kind == TYPE_ARRAY;
    bool is_struct = declaration && declaration->type &&
        declaration->type->kind == TYPE_STRUCT;
    bool is_union = declaration && declaration->type &&
        declaration->type->kind == TYPE_UNION;
    if (!declaration || declaration->kind != DECL_VAR ||
        declaration->var_is_global || declaration->var_is_thread_local ||
        declaration->storage == STORAGE_EXTERN ||
        declaration->storage == STORAGE_STATIC || declaration->var_cleanup ||
        declaration->var_cleanups ||
        ((is_array || is_struct || is_union)
             ? (declaration->type->size <= 0 ||
                declaration->type->is_reference ||
                declaration->type->is_volatile ||
                declaration->type->cleanup_function ||
                (is_struct &&
                 !lower_struct_type_supported(declaration->type)) ||
                (is_union &&
                 !lower_union_type_supported(declaration->type)))
             : !lower_type(declaration->type, &type)) ||
        (!is_array && !is_struct && !is_union &&
         type.kind == RCC_IR_TYPE_VOID)) {
        context->unsupported = true;
        return false;
    }
    allocation = lower_append(context, RCC_IR_ALLOCA,
                              rcc_ir_type_pointer(0u), NULL, 0u, NULL, 0u);
    if (!allocation) return false;
    rcc_ir_set_immediate(allocation,
                         declaration->type->size > 0
                             ? (uint64_t)declaration->type->size : 1u);
    if (is_array || is_struct || is_union) {
        type = rcc_ir_type_pointer(0u);
    }
    if (!lower_add_local(context, declaration, allocation->result, type)) {
        context->unsupported = true;
        return false;
    }
    if (is_array && declaration->var_init) {
        return lower_array_initializer(
            context, allocation->result,
            declaration->type, declaration->var_init);
    }
    if (is_struct && declaration->var_init) {
        return lower_initialize_struct_storage(
            context, allocation->result,
            declaration->type, declaration->var_init);
    }
    if (is_union && declaration->var_init) {
        return lower_initialize_union_storage(
            context, allocation->result,
            declaration->type, declaration->var_init);
    }
    if (declaration->var_init) {
        RccIrLowerValue initializer = lower_expression(
            context, declaration->var_init);
        Expr target;
        if (!initializer.valid) return false;
        initializer = lower_cast(context, initializer, declaration->type);
        memset(&target, 0, sizeof(target));
        target.kind = EXPR_IDENT;
        target.type = declaration->type;
        target.ident_decl = (Decl*)declaration;
        if (!initializer.valid ||
            !lower_store_lvalue(context, &target, initializer)) {
            return false;
        }
    }
    return true;
}

static RccIrType lower_cxx_exception_word_type(void) {
    return rcc_ir_type_integer(
        (uint16_t)(g_opts.target_arch == ARCH_X64 ? 64u : 32u));
}

static uint64_t lower_cxx_exception_type_tag(const Type* type) {
    return rcc_cxx_exception_type_tag(type);
}

static RccIrLowerValue lower_cxx_runtime_call(
    RccIrLowerContext* context, const char* callee, RccIrType result_type,
    const RccIrValue* operands, size_t operand_count) {
    RccIrInstruction* call;
    if (!context || !callee || !callee[0]) {
        if (context) context->unsupported = true;
        return lower_invalid_value();
    }
    call = lower_append(context, RCC_IR_CALL, result_type, operands,
                        operand_count, NULL, 0u);
    if (!call) return lower_invalid_value();
    rcc_ir_set_callee(call, callee);
    return lower_value(call->result, result_type,
                       result_type.kind == RCC_IR_TYPE_INTEGER);
}

static bool lower_cxx_exception_leave(RccIrLowerContext* context,
                                      RccIrLowerValue frame) {
    RccIrValue operand;
    if (!frame.valid) return false;
    operand = frame.value;
    (void)lower_cxx_runtime_call(context, "rin_cpp_exception_leave",
                                 rcc_ir_type_void(), &operand, 1u);
    return !context->unsupported;
}

static bool lower_cxx_exception_release_frame(RccIrLowerContext* context,
                                               RccIrValue frame) {
    if (!context || frame == RCC_IR_VALUE_NONE) return true;
    (void)lower_cxx_runtime_call(
        context, "rin_cpp_exception_release_frame", rcc_ir_type_void(),
        &frame, 1u);
    return !context->unsupported;
}

static RccIrLowerValue lower_cxx_exception_type_compare(
    RccIrLowerContext* context, RccIrLowerValue actual, uint64_t expected) {
    RccIrLowerValue constant;
    RccIrValue operands[2];
    RccIrInstruction* compare;
    if (!actual.valid || actual.type.kind != RCC_IR_TYPE_INTEGER) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    constant = lower_integer_constant(context, actual.type, true, expected);
    if (!constant.valid) return lower_invalid_value();
    operands[0] = actual.value;
    operands[1] = constant.value;
    compare = lower_append(context, RCC_IR_ICMP,
                           rcc_ir_type_integer(1u), operands, 2u,
                           NULL, 0u);
    if (!compare) return lower_invalid_value();
    rcc_ir_set_predicate(compare, RCC_IR_ICMP_EQ);
    return lower_value(compare->result, rcc_ir_type_integer(1u), true);
}

static RccIrLowerValue lower_cxx_exception_type_match(
    RccIrLowerContext* context, RccIrLowerValue actual,
    const CxxCatch* handler) {
    RccIrLowerValue condition;
    if (!handler || !handler->type) {
        if (context) context->unsupported = true;
        return lower_invalid_value();
    }
    condition = lower_cxx_exception_type_compare(
        context, actual, lower_cxx_exception_type_tag(handler->type));
    if (!condition.valid) return lower_invalid_value();
    for (size_t index = 0u; index < handler->compatible_tag_count; ++index) {
        RccIrLowerValue candidate = lower_cxx_exception_type_compare(
            context, actual, handler->compatible_tags[index]);
        RccIrInstruction* combined;
        RccIrValue operands[2];
        if (!candidate.valid) return lower_invalid_value();
        operands[0] = condition.value;
        operands[1] = candidate.value;
        combined = lower_append(context, RCC_IR_OR,
                                rcc_ir_type_integer(1u), operands, 2u,
                                NULL, 0u);
        if (!combined) return lower_invalid_value();
        condition = lower_value(combined->result,
                                rcc_ir_type_integer(1u), true);
    }
    return condition;
}

static RccIrLowerValue lower_cxx_exception_payload_address(
    RccIrLowerContext* context, RccIrLowerValue frame,
    const CxxCatch* handler) {
    const uint64_t value_offset =
        (uint64_t)(g_opts.target_arch == ARCH_X64 ? 72u : 28u);
    const uint64_t type_offset =
        (uint64_t)(g_opts.target_arch == ARCH_X64 ? 80u : 32u);
    RccIrLowerValue payload_word;
    RccIrLowerValue payload;
    RccIrLowerValue actual;

    if (!context || !handler || !frame.valid) {
        if (context) context->unsupported = true;
        return lower_invalid_value();
    }
    payload_word = lower_load_address(
        context, lower_byte_offset_address(context, frame, value_offset),
        type_ulong);
    payload = lower_cast(context, payload_word, type_ptr(type_void));
    if (!payload.valid) return lower_invalid_value();
    for (size_t index = 0u; index < handler->compatible_tag_count; ++index) {
        RccIrLowerValue condition;
        RccIrLowerValue candidate;
        RccIrInstruction* select;
        RccIrValue operands[3];
        actual = lower_load_address(
            context, lower_byte_offset_address(context, frame, type_offset),
            type_ulong);
        condition = lower_cxx_exception_type_compare(
            context, actual, handler->compatible_tags[index]);
        candidate = lower_byte_offset_address(
            context, payload,
            (uint64_t)(handler->compatible_tag_offsets
                           ? handler->compatible_tag_offsets[index] : 0));
        if (!condition.valid || !candidate.valid) return lower_invalid_value();
        operands[0] = condition.value;
        operands[1] = candidate.value;
        operands[2] = payload.value;
        select = lower_append(context, RCC_IR_SELECT, candidate.type,
                              operands, 3u, NULL, 0u);
        if (!select) return lower_invalid_value();
        payload = lower_value(select->result, candidate.type, true);
    }
    return payload;
}

static bool lower_cxx_catch_body(RccIrLowerContext* context,
                                 const CxxCatch* handler,
                                 RccIrLowerValue frame) {
    const StmtList* item;
    RccIrLowerLocal* local;
    RccIrLowerValue value;
    if (!context || !handler || !handler->body) return false;
    if (handler->parameter) {
        if (handler->body->kind != STMT_BLOCK ||
            !handler->body->block_stmts ||
            handler->body->block_stmts->stmt->kind != STMT_DECL ||
            handler->body->block_stmts->stmt->decl != handler->parameter ||
            !lower_statement(context, handler->body->block_stmts->stmt)) {
            context->unsupported = true;
            return false;
        }
        local = lower_find_local(context, handler->parameter);
        value = lower_cxx_exception_payload_address(context, frame, handler);
        if (!local || !value.valid) {
            context->unsupported = true;
            return false;
        }
        if (handler->parameter->type &&
            (handler->parameter->type->kind == TYPE_STRUCT ||
             handler->parameter->type->kind == TYPE_UNION)) {
            RccIrLowerValue source = lower_cast(
                context, value, type_ptr(type_void));
            bool copied = source.valid &&
                (handler->parameter->type->kind == TYPE_STRUCT
                    ? lower_copy_struct_storage(
                          context, local->address, source.value,
                          handler->parameter->type)
                    : lower_copy_union_storage(
                          context, local->address, source.value,
                          handler->parameter->type));
            if (!copied) return false;
        } else {
            value = lower_cast(context, value, handler->parameter->type);
            if (!value.valid || !lower_store_address(
                    context, lower_value(local->address, local->type, true),
                    value)) {
                context->unsupported = true;
                return false;
            }
        }
        item = handler->body->block_stmts->next;
        for (; item; item = item->next) {
            if (!lower_statement(context, item->stmt)) return false;
        }
        return true;
    }
    return lower_statement(context, handler->body);
}

static bool lower_cxx_throw(RccIrLowerContext* context,
                            const Stmt* statement) {
    RccIrLowerValue value;
    RccIrLowerValue word;
    RccIrLowerValue tag;
    RccIrLowerValue size;
    RccIrValue operands[3];
    if (!statement) {
        if (context) context->unsupported = true;
        return false;
    }
    if (!statement->throw_expr) {
        if (context->active_exception_frame != RCC_IR_VALUE_NONE) {
            RccIrValue operand = context->active_exception_frame;
            (void)lower_cxx_runtime_call(
                context, "rin_cpp_exception_rethrow_frame",
                rcc_ir_type_void(), &operand, 1u);
        } else {
            (void)lower_cxx_runtime_call(
                context, "rin_cpp_exception_rethrow", rcc_ir_type_void(),
                NULL, 0u);
        }
        if (context->unsupported ||
            !lower_append(context, RCC_IR_UNREACHABLE, rcc_ir_type_void(),
                          NULL, 0u, NULL, 0u)) return false;
        context->terminated = true;
        return true;
    }
    value = lower_expression(context, statement->throw_expr);
    tag = lower_integer_constant(context, lower_cxx_exception_word_type(),
                                 true, lower_cxx_exception_type_tag(
                                     statement->throw_expr->type));
    if (!value.valid || !tag.valid) return false;
    if (statement->throw_expr->type &&
        (statement->throw_expr->type->kind == TYPE_STRUCT ||
         statement->throw_expr->type->kind == TYPE_UNION)) {
        size = lower_integer_constant(
            context, lower_cxx_exception_word_type(), true,
            (uint64_t)statement->throw_expr->type->size);
        if (!size.valid || value.type.kind != RCC_IR_TYPE_POINTER) {
            context->unsupported = true;
            return false;
        }
        if (!lower_cxx_exception_release_frame(
                context, context->active_exception_frame)) return false;
        operands[0] = value.value;
        operands[1] = size.value;
        operands[2] = tag.value;
        (void)lower_cxx_runtime_call(
            context, "rin_cpp_exception_throw_object", rcc_ir_type_void(),
            operands, 3u);
    } else {
        word = lower_cast(context, value, type_ulong);
        if (!word.valid) return false;
        if (!lower_cxx_exception_release_frame(
                context, context->active_exception_frame)) return false;
        operands[0] = word.value;
        operands[1] = tag.value;
        (void)lower_cxx_runtime_call(context, "rin_cpp_exception_throw",
                                     rcc_ir_type_void(), operands, 2u);
    }
    if (context->unsupported ||
        !lower_append(context, RCC_IR_UNREACHABLE, rcc_ir_type_void(),
                      NULL, 0u, NULL, 0u)) return false;
    context->terminated = true;
    return true;
}

static bool lower_cxx_statement_falls_through(const Stmt* statement) {
    const StmtList* item;
    if (!statement) return true;
    switch (statement->kind) {
        case STMT_RETURN:
        case STMT_BREAK:
        case STMT_CONTINUE:
        case STMT_GOTO:
        case STMT_THROW:
            return false;
        case STMT_BLOCK:
            for (item = statement->block_stmts; item; item = item->next) {
                if (!lower_cxx_statement_falls_through(item->stmt)) {
                    return false;
                }
            }
            return true;
        case STMT_IF:
            return !statement->if_else ||
                lower_cxx_statement_falls_through(statement->if_then) ||
                lower_cxx_statement_falls_through(statement->if_else);
        case STMT_TRY:
            if (lower_cxx_statement_falls_through(statement->try_body)) {
                return true;
            }
            for (const CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                if (lower_cxx_statement_falls_through(handler->body)) {
                    return true;
                }
            }
            return false;
        default:
            return true;
    }
}

static bool lower_cxx_try(RccIrLowerContext* context,
                          const Stmt* statement) {
    RccIrLowerValue frame;
    RccIrLowerValue setjmp_result;
    RccIrLowerValue condition;
    RccIrInstruction* allocation;
    RccIrBlock* body_block;
    RccIrBlock* dispatch_block;
    RccIrBlock* end_block;
    RccIrBlock* unmatched_block;
    RccIrBlock** handler_blocks;
    RccIrValue old_active_exception_frame;
    bool end_reachable;
    bool has_ellipsis = false;
    size_t typed_handler_count;
    size_t handler_count = 0u;
    size_t index;
    if (!context || !statement || !statement->try_body ||
        !statement->try_catches || statement->try_frame_size <= 0) {
        if (context) context->unsupported = true;
        return false;
    }
    old_active_exception_frame = context->active_exception_frame;
    for (const CxxCatch* handler = statement->try_catches; handler;
         handler = handler->next) ++handler_count;
    for (const CxxCatch* handler = statement->try_catches; handler;
         handler = handler->next) {
        if (handler->is_ellipsis) has_ellipsis = true;
    }
    typed_handler_count = has_ellipsis ? handler_count - 1u : handler_count;
    end_reachable = lower_cxx_statement_falls_through(statement->try_body);
    for (const CxxCatch* handler = statement->try_catches; handler;
         handler = handler->next) {
        end_reachable = end_reachable ||
            lower_cxx_statement_falls_through(handler->body);
    }
    handler_blocks = rcc_alloc(handler_count * sizeof(*handler_blocks));
    allocation = lower_append(context, RCC_IR_ALLOCA,
                              rcc_ir_type_pointer(0u), NULL, 0u, NULL, 0u);
    if (!allocation || !handler_blocks) {
        rcc_free(handler_blocks);
        context->unsupported = true;
        return false;
    }
    rcc_ir_set_immediate(allocation, (uint64_t)statement->try_frame_size);
    frame = lower_value(allocation->result, rcc_ir_type_pointer(0u), true);
    {
        RccIrValue operand = frame.value;
        (void)lower_cxx_runtime_call(context, "rin_cpp_exception_install",
                                     rcc_ir_type_void(), &operand, 1u);
    }
    {
        RccIrValue operand = frame.value;
        setjmp_result = lower_cxx_runtime_call(
            context, "setjmp", lower_cxx_exception_word_type(), &operand, 1u);
    }
    body_block = rcc_ir_block_add(context->function, "try.body");
    dispatch_block = rcc_ir_block_add(context->function, "try.dispatch");
    end_block = end_reachable
        ? rcc_ir_block_add(context->function, "try.end") : NULL;
    unmatched_block = has_ellipsis
        ? NULL : rcc_ir_block_add(context->function, "try.unmatched");
    if (!setjmp_result.valid || !body_block || !dispatch_block ||
        (end_reachable && !end_block) || (!has_ellipsis && !unmatched_block)) {
        rcc_free(handler_blocks);
        context->unsupported = true;
        return false;
    }
    for (index = 0u; index < handler_count; ++index) {
        handler_blocks[index] = rcc_ir_block_add(context->function,
                                                 "try.handler");
        if (!handler_blocks[index]) {
            rcc_free(handler_blocks);
            context->unsupported = true;
            return false;
        }
    }
    condition = lower_truth(context, setjmp_result);
    {
        RccIrBlockId targets[2] = {body_block->id, dispatch_block->id};
        if (!condition.valid ||
            !lower_append(context, RCC_IR_COND_BRANCH, rcc_ir_type_void(),
                          &condition.value, 1u, targets, 2u)) {
            rcc_free(handler_blocks);
            return false;
        }
    }
    context->terminated = true;

    context->current = body_block;
    context->terminated = false;
    if (!lower_statement(context, statement->try_body)) {
        rcc_free(handler_blocks);
        return false;
    }
    if (!context->terminated) {
        if (!end_reachable || !lower_cxx_exception_leave(context, frame) ||
            !lower_branch(context, end_block->id)) {
            rcc_free(handler_blocks);
            return false;
        }
    }

    context->current = dispatch_block;
    context->terminated = false;
    index = 0u;
    {
        const CxxCatch* handler = statement->try_catches;
        for (; index < typed_handler_count;
             ++index, handler = handler->next) {
            RccIrBlock* next_block;
            bool final_typed = index + 1u >= typed_handler_count;
            if (final_typed) {
                next_block = has_ellipsis
                    ? handler_blocks[typed_handler_count] : unmatched_block;
            } else {
                next_block = rcc_ir_block_add(context->function, "try.next");
            }
            if (!next_block) {
                rcc_free(handler_blocks);
                context->unsupported = true;
                return false;
            }
            {
                RccIrLowerValue actual = lower_load_address(
                    context,
                    lower_byte_offset_address(
                        context, frame,
                        (uint64_t)(g_opts.target_arch == ARCH_X64 ? 80u : 32u)),
                    type_ulong);
                condition = lower_cxx_exception_type_match(
                    context, actual, handler);
                if (!condition.valid ||
                    !lower_conditional_branch(context, condition,
                                              handler_blocks[index]->id,
                                              next_block->id)) {
                    rcc_free(handler_blocks);
                    return false;
                }
            }
            if (!final_typed) {
                context->current = next_block;
                context->terminated = false;
            }
        }
        if (has_ellipsis && typed_handler_count == 0u) {
            if (!lower_branch(context, handler_blocks[0]->id)) {
                rcc_free(handler_blocks);
                return false;
            }
        }
    }

    if (!has_ellipsis) {
        context->current = unmatched_block;
        context->terminated = false;
        {
            RccIrValue operand = frame.value;
            if (!lower_cxx_exception_release_frame(
                    context, old_active_exception_frame)) {
                rcc_free(handler_blocks);
                return false;
            }
            (void)lower_cxx_runtime_call(
                context, "rin_cpp_exception_rethrow_frame",
                rcc_ir_type_void(), &operand, 1u);
            if (context->unsupported ||
                !lower_append(context, RCC_IR_UNREACHABLE, rcc_ir_type_void(),
                              NULL, 0u, NULL, 0u)) {
                rcc_free(handler_blocks);
                return false;
            }
            context->terminated = true;
        }
    }

    /* The parser requires a catch-all handler to be last, so every handler
     * block is lowered independently after the dispatch chain above. */
    index = 0u;
    for (const CxxCatch* handler = statement->try_catches; handler;
         handler = handler->next, ++index) {
        RccIrValue handler_previous_frame = context->active_exception_frame;
        context->current = handler_blocks[index];
        context->terminated = false;
        if (!lower_cxx_exception_leave(context, frame)) {
            rcc_free(handler_blocks);
            return false;
        }
        context->active_exception_frame = frame.value;
        if (!lower_cxx_catch_body(context, handler, frame)) {
            context->active_exception_frame = handler_previous_frame;
            rcc_free(handler_blocks);
            return false;
        }
        if (!context->terminated) {
            if (!lower_cxx_exception_release_frame(context, frame.value) ||
                !end_reachable || !lower_branch(context, end_block->id)) {
                context->active_exception_frame = handler_previous_frame;
                rcc_free(handler_blocks);
                return false;
            }
        }
        context->active_exception_frame = handler_previous_frame;
    }
    rcc_free(handler_blocks);
    if (!end_reachable) {
        context->terminated = true;
        return true;
    }
    context->current = end_block;
    context->terminated = false;
    return true;
}

static bool lower_statement(RccIrLowerContext* context,
                            const Stmt* statement) {
    if (!context || context->unsupported || !statement) {
        if (context) context->unsupported = true;
        return false;
    }
    if (context->terminated &&
        (!context->current_switch ||
         !lower_statement_has_switch_label(statement))) {
        return true;
    }
    switch (statement->kind) {
        case STMT_NULL:
            return true;
        case STMT_EXPR:
            (void)lower_expression(context, statement->expr);
            return !context->unsupported;
        case STMT_BLOCK:
            return lower_block(context, statement);
        case STMT_DECL:
            return lower_declaration(context, statement->decl);
        case STMT_RETURN:
            if (lower_abi_is_aggregate(context->ast_return_type)) {
                RccIrLowerValue source;
                RccIrLowerValue result;
                RccIrLowerValue high = lower_invalid_value();
                RccIrValue return_values[2];
                size_t return_count = 1u;
                RccIrInstruction* return_instruction;
                if (!statement->return_val ||
                    !statement->return_val->type ||
                    !type_is_compatible((Type*)context->ast_return_type,
                                        statement->return_val->type)) {
                    context->unsupported = true;
                    return false;
                }
                source = lower_expression(context, statement->return_val);
                if (!source.valid ||
                    source.type.kind != RCC_IR_TYPE_POINTER) return false;
                if (context->aggregate_return_kind ==
                    LOWER_ABI_RETURN_REGISTER_AGGREGATE) {
                    result = lower_load_aggregate_chunk(
                        context, source, context->ast_return_type, 0u);
                } else if (context->aggregate_return_kind ==
                           LOWER_ABI_RETURN_REGISTER_PAIR) {
                    result = lower_load_aggregate_chunk(
                        context, source, context->ast_return_type, 0u);
                    high = lower_load_aggregate_chunk(
                        context, source, context->ast_return_type, 8u);
                    return_count = 2u;
                } else if (context->aggregate_return_kind ==
                           LOWER_ABI_RETURN_SRET) {
                    bool copied = context->ast_return_type->kind == TYPE_STRUCT
                        ? lower_copy_struct_storage(
                              context, context->aggregate_return_address,
                              source.value, context->ast_return_type)
                        : lower_copy_union_storage(
                              context, context->aggregate_return_address,
                              source.value, context->ast_return_type);
                    if (!copied) return false;
                    result = lower_value(
                        context->aggregate_return_address,
                        rcc_ir_type_pointer(0u), true);
                } else {
                    context->unsupported = true;
                    return false;
                }
                if (!result.valid ||
                    (return_count == 2u && !high.valid)) return false;
                if (!lower_cxx_exception_release_frame(
                        context, context->active_exception_frame)) return false;
                return_values[0] = result.value;
                return_values[1] = high.value;
                return_instruction = lower_append(
                    context, RCC_IR_RETURN, rcc_ir_type_void(),
                    return_values, return_count, NULL, 0u);
                if (!return_instruction) return false;
                if (context->aggregate_return_kind ==
                        LOWER_ABI_RETURN_REGISTER_PAIR) {
                    rcc_ir_set_immediate(
                        return_instruction,
                        (uint64_t)context->ast_return_type->size);
                } else if (context->aggregate_return_kind ==
                        LOWER_ABI_RETURN_SRET &&
                    g_opts.target_arch != ARCH_X64) {
                    rcc_ir_set_immediate(return_instruction, 4u);
                }
            } else if (context->function->return_type.kind ==
                       RCC_IR_TYPE_VOID) {
                if (statement->return_val) {
                    (void)lower_expression(context, statement->return_val);
                    if (context->unsupported) return false;
                }
                if (!lower_cxx_exception_release_frame(
                        context, context->active_exception_frame)) return false;
                if (!lower_append(context, RCC_IR_RETURN,
                                  rcc_ir_type_void(), NULL, 0u, NULL, 0u)) {
                    return false;
                }
            } else {
                RccIrLowerValue result = lower_expression(
                    context, statement->return_val);
                if (!result.valid || !context->ast_return_type) return false;
                result = lower_cast(context, result,
                                    context->ast_return_type);
                if (!result.valid) return false;
                if (!lower_cxx_exception_release_frame(
                        context, context->active_exception_frame)) return false;
                if (!lower_append(context, RCC_IR_RETURN,
                                  rcc_ir_type_void(), &result.value, 1u,
                                  NULL, 0u)) {
                    return false;
                }
            }
            context->terminated = true;
            return true;
        case STMT_IF:
            return lower_if(context, statement);
        case STMT_WHILE:
            return lower_while(context, statement);
        case STMT_DO:
            return lower_do(context, statement);
        case STMT_FOR:
            return lower_for(context, statement);
        case STMT_BREAK:
            return lower_branch(context, context->break_target);
        case STMT_CONTINUE:
            return lower_branch(context, context->continue_target);
        case STMT_SWITCH:
            return lower_switch(context, statement);
        case STMT_CASE:
        case STMT_DEFAULT:
            return lower_switch_case(context, statement);
        case STMT_TRY:
            return lower_cxx_try(context, statement);
        case STMT_THROW:
            return lower_cxx_throw(context, statement);
        case STMT_GOTO:
        case STMT_LABEL:
        case STMT_ASM:
            context->unsupported = true;
            return false;
    }
    context->unsupported = true;
    return false;
}

static bool lower_parameters(RccIrLowerContext* context,
                             const Decl* declaration) {
    const DeclList* parameter = declaration->func_params;
    size_t index = context->aggregate_return_kind ==
                       LOWER_ABI_RETURN_SRET ? 1u : 0u;
    if (declaration->func_this_param) {
        const Decl* item = declaration->func_this_param;
        RccIrType type;
        RccIrInstruction* allocation;
        RccIrValue operands[2];
        if (index >= context->function->parameter_count ||
            !item->type || !lower_abi_native_scalar_type(item->type, &type)) {
            context->unsupported = true;
            return false;
        }
        allocation = lower_append(context, RCC_IR_ALLOCA,
                                  rcc_ir_type_pointer(0u), NULL, 0u,
                                  NULL, 0u);
        if (!allocation) return false;
        rcc_ir_set_immediate(
            allocation, item->type->size > 0
                ? (uint64_t)item->type->size : 1u);
        if (!lower_add_local(context, item, allocation->result, type)) {
            context->unsupported = true;
            return false;
        }
        operands[0] = context->function->parameters[index];
        operands[1] = allocation->result;
        if (!lower_append(context, RCC_IR_STORE, rcc_ir_type_void(),
                          operands, 2u, NULL, 0u)) {
            return false;
        }
        ++index;
    }
    while (parameter) {
        const Decl* item = parameter->decl;
        RccIrType type;
        RccIrInstruction* allocation;
        RccIrValue operands[2];
        bool aggregate = item && lower_abi_is_aggregate(item->type);
        size_t units = aggregate
            ? ((size_t)item->type->size + lower_abi_chunk_size() - 1u) /
                lower_abi_chunk_size()
            : 1u;
        size_t allocation_size = aggregate
            ? units * lower_abi_chunk_size()
            : (item && item->type && item->type->size > 0
                   ? (size_t)item->type->size : 1u);
        if (!item || item->kind != DECL_PARAM ||
            index > context->function->parameter_count ||
            units > context->function->parameter_count - index ||
            (aggregate
                 ? !lower_abi_aggregate_supported(item->type)
                 : !lower_abi_native_scalar_type(item->type, &type))) {
            context->unsupported = true;
            return false;
        }
        allocation = lower_append(context, RCC_IR_ALLOCA,
                                  rcc_ir_type_pointer(0u), NULL, 0u,
                                  NULL, 0u);
        if (!allocation) return false;
        rcc_ir_set_immediate(allocation, (uint64_t)allocation_size);
        if (aggregate) type = rcc_ir_type_pointer(0u);
        if (!lower_add_local(context, item, allocation->result, type)) {
            context->unsupported = true;
            return false;
        }
        if (aggregate) {
            RccIrLowerValue base = lower_value(
                allocation->result, rcc_ir_type_pointer(0u), true);
            for (size_t unit = 0u; unit < units; ++unit) {
                RccIrLowerValue address = lower_byte_offset_address(
                    context, base,
                    (uint64_t)(unit * lower_abi_chunk_size()));
                RccIrLowerValue value = lower_value(
                    context->function->parameters[index],
                    context->function->parameter_types[index], true);
                if (!address.valid ||
                    !lower_store_address(context, address, value)) {
                    return false;
                }
                ++index;
            }
        } else {
            operands[0] = context->function->parameters[index];
            operands[1] = allocation->result;
            if (!lower_append(context, RCC_IR_STORE, rcc_ir_type_void(),
                              operands, 2u, NULL, 0u)) {
                return false;
            }
            ++index;
        }
        parameter = parameter->next;
    }
    return index == context->function->parameter_count;
}

RccIrLowerStatus rcc_ir_lower_function(const Decl* declaration,
                                       RccIrModule** module_out,
                                       char* error, size_t error_size) {
    RccIrLowerContext context;
    RccIrType return_type;
    RccIrType hidden_type = rcc_ir_type_pointer(0u);
    RccIrType* parameter_types = NULL;
    size_t parameter_count = 0u;
    int aggregate_return_kind = LOWER_ABI_RETURN_SCALAR;
    RccIrModule* module;
    RccIrFunction* function;
    RccIrBlock* entry;
    if (module_out) *module_out = NULL;
    if (error && error_size != 0u) error[0] = '\0';
    if (!declaration || declaration->kind != DECL_FUNC ||
        !declaration->func_body || !declaration->type ||
        declaration->type->kind != TYPE_FUNC ||
        !declaration->type->ret_type) {
        return RCC_IR_LOWER_UNSUPPORTED;
    }
    if (lower_abi_is_aggregate(declaration->type->ret_type)) {
        aggregate_return_kind = lower_abi_return_layout(
            declaration->type->ret_type, &return_type);
        if (aggregate_return_kind == LOWER_ABI_RETURN_UNSUPPORTED) {
            return RCC_IR_LOWER_UNSUPPORTED;
        }
    } else if (!lower_type(declaration->type->ret_type, &return_type)) {
        return RCC_IR_LOWER_UNSUPPORTED;
    }
    if (!lower_abi_parameter_layout(
            declaration->type->params,
            aggregate_return_kind == LOWER_ABI_RETURN_SRET
                ? &hidden_type : NULL,
            aggregate_return_kind == LOWER_ABI_RETURN_SRET ? 1u : 0u,
            &parameter_types, &parameter_count)) {
        return RCC_IR_LOWER_UNSUPPORTED;
    }
    module = rcc_ir_module_create();
    function = rcc_ir_function_add(module, decl_link_name(declaration),
                                   return_type, parameter_types,
                                   parameter_count);
    rcc_free(parameter_types);
    entry = function ? rcc_ir_block_add(function, "entry") : NULL;
    if (!function || !entry) {
        rcc_ir_module_destroy(module);
        return RCC_IR_LOWER_INVALID;
    }
    memset(&context, 0, sizeof(context));
    context.module = module;
    context.function = function;
    context.ast_return_type = declaration->type->ret_type;
    context.aggregate_return_kind = aggregate_return_kind;
    context.aggregate_return_address =
        aggregate_return_kind == LOWER_ABI_RETURN_SRET
            ? function->parameters[0] : RCC_IR_VALUE_NONE;
    context.active_exception_frame = RCC_IR_VALUE_NONE;
    context.current = entry;
    context.break_target = RCC_IR_BLOCK_NONE;
    context.continue_target = RCC_IR_BLOCK_NONE;
    if (!lower_parameters(&context, declaration) ||
        !lower_statement(&context, declaration->func_body)) {
        lower_release_locals(context.locals);
        rcc_ir_module_destroy(module);
        return context.unsupported ? RCC_IR_LOWER_UNSUPPORTED
                                   : RCC_IR_LOWER_INVALID;
    }
    if (!context.terminated) {
        if (return_type.kind != RCC_IR_TYPE_VOID ||
            !lower_append(&context, RCC_IR_RETURN, rcc_ir_type_void(),
                          NULL, 0u, NULL, 0u)) {
            lower_release_locals(context.locals);
            rcc_ir_module_destroy(module);
            return RCC_IR_LOWER_UNSUPPORTED;
        }
        context.terminated = true;
    }
    lower_release_locals(context.locals);
    {
        RccIrOptimizationStats stats;
        if (!rcc_ir_optimize_function(
                function, (unsigned)g_opts.opt_level, &stats,
                error, error_size)) {
            rcc_ir_module_destroy(module);
            return RCC_IR_LOWER_INVALID;
        }
    }
    {
        RccMirFunction* mir = NULL;
        RccMirRegisterPolicy policy;
        RccMirAllocation allocation;
        RccMirPhiPlan phi_plan;
        RccX86Function* selected = NULL;
        RccX86LegalFunction* legal = NULL;
        if (!rcc_mir_lower_ir(function, &mir, error, error_size)) {
            rcc_ir_module_destroy(module);
            return RCC_IR_LOWER_INVALID;
        }
        if (g_opts.target_arch == ARCH_X64) {
            rcc_mir_register_policy_x86_64(&policy);
        } else {
            rcc_mir_register_policy_i686(&policy);
        }
        if (!rcc_mir_linear_scan_allocate(
                mir, &policy, &allocation, error, error_size)) {
            rcc_mir_function_destroy(mir);
            rcc_ir_module_destroy(module);
            return RCC_IR_LOWER_INVALID;
        }
        if (!rcc_mir_build_phi_plan(
                mir, &policy, &allocation, &phi_plan,
                error, error_size)) {
            rcc_mir_allocation_release(&allocation);
            rcc_mir_function_destroy(mir);
            rcc_ir_module_destroy(module);
            return RCC_IR_LOWER_INVALID;
        }
        if (!rcc_x86_select_function(
                mir,
                g_opts.target_arch == ARCH_X64
                    ? RCC_X86_TARGET_X86_64 : RCC_X86_TARGET_I686,
                &policy, &allocation, &phi_plan, &selected,
                error, error_size)) {
            rcc_mir_phi_plan_release(&phi_plan);
            rcc_mir_allocation_release(&allocation);
            rcc_mir_function_destroy(mir);
            rcc_ir_module_destroy(module);
            return RCC_IR_LOWER_INVALID;
        }
        if (!rcc_x86_legalize_function(
                selected, &policy, &legal, error, error_size)) {
            rcc_x86_function_destroy(selected);
            rcc_mir_phi_plan_release(&phi_plan);
            rcc_mir_allocation_release(&allocation);
            rcc_mir_function_destroy(mir);
            rcc_ir_module_destroy(module);
            return RCC_IR_LOWER_INVALID;
        }
        rcc_x86_legal_function_destroy(legal);
        rcc_x86_function_destroy(selected);
        rcc_mir_phi_plan_release(&phi_plan);
        rcc_mir_allocation_release(&allocation);
        rcc_mir_function_destroy(mir);
    }
    if (!rcc_ir_verify_module(module, error, error_size)) {
        rcc_ir_module_destroy(module);
        return RCC_IR_LOWER_INVALID;
    }
    if (module_out) {
        *module_out = module;
    } else {
        rcc_ir_module_destroy(module);
    }
    return RCC_IR_LOWER_OK;
}

bool rcc_ir_verify_ast_subset(const AST* ast, size_t* lowered_functions,
                              char* error, size_t error_size) {
    size_t count = 0u;
    if (lowered_functions) *lowered_functions = 0u;
    if (error && error_size != 0u) error[0] = '\0';
    if (!ast) {
        if (error && error_size != 0u) {
            snprintf(error, error_size, "invalid AST for IR lowering");
        }
        return false;
    }
    for (const DeclList* item = ast->decls; item; item = item->next) {
        RccIrModule* module = NULL;
        RccIrLowerStatus status;
        char detail[512];
        if (!item->decl || item->decl->kind != DECL_FUNC ||
            !item->decl->func_body) {
            continue;
        }
        status = rcc_ir_lower_function(item->decl, &module, error,
                                       error_size);
        if (status == RCC_IR_LOWER_INVALID) {
            snprintf(detail, sizeof(detail), "%s",
                     error && error[0] ? error : "typed SSA validation failed");
            if (error && error_size != 0u && error[0] == '\0') {
                snprintf(error, error_size,
                         "function '%s' failed typed SSA validation",
                         item->decl->name ? item->decl->name : "<anonymous>");
            } else if (error && error_size != 0u) {
                snprintf(error, error_size, "function '%s': %s",
                         item->decl->name ? item->decl->name : "<anonymous>",
                         detail);
            }
            return false;
        }
        if (status == RCC_IR_LOWER_OK) {
            ++count;
            rcc_ir_module_destroy(module);
        }
    }
    if (lowered_functions) *lowered_functions = count;
    return true;
}
