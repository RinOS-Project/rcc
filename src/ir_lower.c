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

typedef struct {
    RccIrModule* module;
    RccIrFunction* function;
    const Type* ast_return_type;
    RccIrBlock* current;
    RccIrLowerLocal* locals;
    RccIrBlockId break_target;
    RccIrBlockId continue_target;
    bool terminated;
    bool unsupported;
} RccIrLowerContext;

static bool lower_statement(RccIrLowerContext* context,
                            const Stmt* statement);
static RccIrLowerValue lower_expression(RccIrLowerContext* context,
                                        const Expr* expression);

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
            if (type->size <= 0 || type->size > 8) return false;
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

static RccIrLowerValue lower_lvalue_address(
    RccIrLowerContext* context, const Expr* expression) {
    RccIrLowerLocal* local;
    if (!expression) return lower_invalid_value();
    if (expression->kind == EXPR_IDENT) {
        local = lower_find_local(context, expression->ident_decl);
        if (!local) {
            context->unsupported = true;
            return lower_invalid_value();
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
    context->unsupported = true;
    return lower_invalid_value();
}

static RccIrLowerValue lower_load_lvalue(RccIrLowerContext* context,
                                         const Expr* expression) {
    RccIrLowerValue address = lower_lvalue_address(context, expression);
    RccIrType type;
    RccIrInstruction* load;
    if (!address.valid || !lower_type(expression->type, &type) ||
        type.kind == RCC_IR_TYPE_VOID) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    load = lower_append(context, RCC_IR_LOAD, type, &address.value, 1u,
                        NULL, 0u);
    if (!load) return lower_invalid_value();
    return lower_value(load->result, type, expression->type->is_unsigned);
}

static bool lower_store_lvalue(RccIrLowerContext* context,
                               const Expr* expression,
                               RccIrLowerValue value) {
    RccIrLowerValue address = lower_lvalue_address(context, expression);
    RccIrValue operands[2];
    if (!address.valid || !value.valid) return false;
    operands[0] = value.value;
    operands[1] = address.value;
    return lower_append(context, RCC_IR_STORE, rcc_ir_type_void(),
                        operands, 2u, NULL, 0u) != NULL;
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
    RccIrLowerValue left = lower_load_lvalue(context,
                                             expression->binary_lhs);
    RccIrLowerValue right = lower_expression(context,
                                             expression->binary_rhs);
    Type* operation_type;
    RccIrType ir_operation_type;
    RccIrInstruction* operation;
    RccIrValue operands[2];
    RccIrLowerValue result;
    ExprKind binary_kind = lower_compound_binary_kind(expression->kind);
    if (!left.valid || !right.valid || !expression->binary_lhs->type ||
        expression->binary_lhs->type->kind == TYPE_PTR) {
        context->unsupported = true;
        return lower_invalid_value();
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
    if (!result.valid || !lower_store_lvalue(
            context, expression->binary_lhs, result)) {
        return lower_invalid_value();
    }
    return result;
}

static RccIrLowerValue lower_increment(RccIrLowerContext* context,
                                       const Expr* expression) {
    RccIrLowerValue old_value = lower_load_lvalue(
        context, expression->unary_operand);
    RccIrLowerValue one;
    RccIrValue operands[2];
    RccIrInstruction* operation;
    RccIrLowerValue new_value;
    bool increment = expression->kind == EXPR_PREINC ||
        expression->kind == EXPR_POSTINC;
    bool postfix = expression->kind == EXPR_POSTINC ||
        expression->kind == EXPR_POSTDEC;
    if (!old_value.valid || !expression->unary_operand->type ||
        expression->unary_operand->type->kind == TYPE_PTR) {
        context->unsupported = true;
        return lower_invalid_value();
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
    if (!lower_store_lvalue(context, expression->unary_operand, new_value)) {
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
    size_t argument_count = 0u;
    size_t index = 0u;
    RccIrValue* operands = NULL;
    RccIrType return_type;
    RccIrInstruction* call;
    if (expression->cxx_close_call || !expression->call_func ||
        expression->call_func->kind != EXPR_IDENT) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    callee = expression->call_func->ident_decl;
    if (!callee || callee->kind != DECL_FUNC ||
        !lower_type(expression->type, &return_type)) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    for (argument = expression->call_args; argument;
         argument = argument->next) {
        ++argument_count;
    }
    if (argument_count != 0u) {
        operands = rcc_alloc(argument_count * sizeof(*operands));
    }
    parameter = callee->type && callee->type->kind == TYPE_FUNC
        ? callee->type->params : NULL;
    for (argument = expression->call_args; argument;
         argument = argument->next) {
        RccIrLowerValue value = lower_expression(context, argument->expr);
        if (parameter) {
            value = lower_cast(context, value, parameter->type);
            parameter = parameter->next;
        }
        if (!value.valid) {
            rcc_free(operands);
            return lower_invalid_value();
        }
        operands[index++] = value.value;
    }
    call = lower_append(context, RCC_IR_CALL, return_type, operands,
                        argument_count, NULL, 0u);
    rcc_free(operands);
    if (!call) return lower_invalid_value();
    rcc_ir_set_callee(call, decl_link_name(callee));
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
            return lower_load_lvalue(context, expression);
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
        case EXPR_FLOAT_LIT:
        case EXPR_STRING_LIT:
        case EXPR_AND:
        case EXPR_OR:
        case EXPR_COND:
        case EXPR_INDEX:
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
        case EXPR_COMPOUND:
        case EXPR_GENERIC:
        case EXPR_VA_START:
        case EXPR_VA_END:
        case EXPR_VA_COPY:
        case EXPR_VA_ARG:
            context->unsupported = true;
            return lower_invalid_value();
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

static bool lower_block(RccIrLowerContext* context, const Stmt* block) {
    if (!block || block->kind != STMT_BLOCK) {
        return lower_statement(context, block);
    }
    for (const StmtList* item = block->block_stmts; item;
         item = item->next) {
        if (context->terminated) break;
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

static bool lower_declaration(RccIrLowerContext* context,
                              const Decl* declaration) {
    RccIrType type;
    RccIrInstruction* allocation;
    if (!declaration || declaration->kind != DECL_VAR ||
        declaration->var_is_global || declaration->var_is_thread_local ||
        declaration->storage == STORAGE_EXTERN ||
        declaration->storage == STORAGE_STATIC || declaration->var_cleanup ||
        !lower_type(declaration->type, &type) ||
        type.kind == RCC_IR_TYPE_VOID) {
        context->unsupported = true;
        return false;
    }
    allocation = lower_append(context, RCC_IR_ALLOCA,
                              rcc_ir_type_pointer(0u), NULL, 0u, NULL, 0u);
    if (!allocation) return false;
    rcc_ir_set_immediate(allocation,
                         declaration->type->size > 0
                             ? (uint64_t)declaration->type->size : 1u);
    if (!lower_add_local(context, declaration, allocation->result, type)) {
        context->unsupported = true;
        return false;
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

static bool lower_statement(RccIrLowerContext* context,
                            const Stmt* statement) {
    if (!context || context->unsupported || !statement) {
        if (context) context->unsupported = true;
        return false;
    }
    if (context->terminated) return true;
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
            if (context->function->return_type.kind == RCC_IR_TYPE_VOID) {
                if (statement->return_val) {
                    (void)lower_expression(context, statement->return_val);
                    if (context->unsupported) return false;
                }
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
        case STMT_CASE:
        case STMT_DEFAULT:
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
    size_t index = 0u;
    while (parameter) {
        const Decl* item = parameter->decl;
        RccIrType type;
        RccIrInstruction* allocation;
        RccIrValue operands[2];
        if (!item || item->kind != DECL_PARAM ||
            index >= context->function->parameter_count ||
            !lower_type(item->type, &type)) {
            context->unsupported = true;
            return false;
        }
        allocation = lower_append(context, RCC_IR_ALLOCA,
                                  rcc_ir_type_pointer(0u), NULL, 0u,
                                  NULL, 0u);
        if (!allocation) return false;
        rcc_ir_set_immediate(allocation,
                             item->type->size > 0
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
        parameter = parameter->next;
    }
    return index == context->function->parameter_count;
}

RccIrLowerStatus rcc_ir_lower_function(const Decl* declaration,
                                       RccIrModule** module_out,
                                       char* error, size_t error_size) {
    RccIrLowerContext context;
    RccIrType return_type;
    RccIrType* parameter_types = NULL;
    size_t parameter_count = 0u;
    size_t index = 0u;
    RccIrModule* module;
    RccIrFunction* function;
    RccIrBlock* entry;
    const DeclList* parameter;
    if (module_out) *module_out = NULL;
    if (error && error_size != 0u) error[0] = '\0';
    if (!declaration || declaration->kind != DECL_FUNC ||
        !declaration->func_body || !declaration->type ||
        declaration->type->kind != TYPE_FUNC ||
        !lower_type(declaration->type->ret_type, &return_type)) {
        return RCC_IR_LOWER_UNSUPPORTED;
    }
    for (parameter = declaration->func_params; parameter;
         parameter = parameter->next) {
        ++parameter_count;
    }
    if (parameter_count != 0u) {
        parameter_types = rcc_alloc(parameter_count *
                                    sizeof(*parameter_types));
    }
    for (parameter = declaration->func_params; parameter;
         parameter = parameter->next) {
        if (!parameter->decl ||
            !lower_type(parameter->decl->type, &parameter_types[index++])) {
            rcc_free(parameter_types);
            return RCC_IR_LOWER_UNSUPPORTED;
        }
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
        RccIrMem2RegStats stats;
        if (!rcc_ir_mem2reg(function, &stats, error, error_size)) {
            rcc_ir_module_destroy(module);
            return RCC_IR_LOWER_INVALID;
        }
    }
    {
        RccIrSimplifyStats stats;
        if (!rcc_ir_simplify(function, &stats, error, error_size)) {
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
        if (!item->decl || item->decl->kind != DECL_FUNC ||
            !item->decl->func_body) {
            continue;
        }
        status = rcc_ir_lower_function(item->decl, &module, error,
                                       error_size);
        if (status == RCC_IR_LOWER_INVALID) return false;
        if (status == RCC_IR_LOWER_OK) {
            ++count;
            rcc_ir_module_destroy(module);
        }
    }
    if (lowered_functions) *lowered_functions = count;
    return true;
}
