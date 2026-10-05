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

typedef struct {
    RccIrValue value;
    RccIrType type;
    bool is_unsigned;
    bool valid;
} RccIrLowerValue;

typedef struct {
    RccIrLowerValue low;
    RccIrLowerValue high;
    bool is_unsigned;
    bool valid;
} RccIrLowerWideValue;

typedef struct RccIrLowerLocal {
    const Decl* declaration;
    RccIrValue address;
    RccIrType type;
    /* Read-only i686 64-bit integers remain as two real i32 SSA values.
     * The address is still materialized for valid address-taking uses, but
     * ordinary reads never need to reload the stack mirror. */
    bool wide_ssa;
    RccIrLowerWideValue wide_value;
    struct RccIrLowerLocal* next;
} RccIrLowerLocal;

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

typedef struct RccIrLowerLabel {
    const char* name;
    const Stmt* statement;
    RccIrBlock* block;
    struct RccIrLowerLabel* next;
} RccIrLowerLabel;

typedef struct {
    RccIrModule* module;
    RccIrFunction* function;
    const Type* ast_return_type;
    RccIrBlock* current;
    RccIrLowerLocal* locals;
    RccIrLowerLabel* labels;
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
static bool lower_collect_labels(RccIrLowerContext* context,
                                 const Stmt* statement);
static RccIrLowerValue lower_expression(RccIrLowerContext* context,
                                        const Expr* expression);
static bool lower_i686_wide_scalar_type(const Type* type);
static ExprKind lower_compound_binary_kind(ExprKind kind);
static bool lower_wide_scalar_expression(
    RccIrLowerContext* context, const Expr* expression,
    RccIrLowerWideValue* result);
static RccIrLowerValue lower_wide_scalar_truth(
    RccIrLowerContext* context, RccIrLowerWideValue value);
static bool lower_wide_scalar_conditional_expression(
    RccIrLowerContext* context, const Expr* expression,
    RccIrLowerWideValue* result);
static bool lower_wide_scalar_call(
    RccIrLowerContext* context, const Expr* expression,
    RccIrLowerWideValue* result);
static bool lower_wide_scalar_compare(
    RccIrLowerContext* context, ExprKind kind,
    RccIrLowerWideValue left, RccIrLowerWideValue right,
    bool is_unsigned, RccIrLowerValue* result);
static RccIrLowerValue lower_wide_scalar_word_select(
    RccIrLowerContext* context, RccIrLowerValue condition,
    RccIrLowerValue then_value, RccIrLowerValue else_value);
static RccIrLowerValue lower_wide_scalar_bool_operation(
    RccIrLowerContext* context, RccIrOpcode opcode,
    RccIrLowerValue left, RccIrLowerValue right);
static RccIrLowerValue lower_wide_scalar_compare_words(
    RccIrLowerContext* context, RccIrLowerValue left,
    RccIrLowerValue right, RccIrIntPredicate predicate);
static bool lower_wide_scalar_shift(
    RccIrLowerContext* context, ExprKind kind,
    RccIrLowerWideValue value, RccIrLowerValue amount,
    bool is_unsigned, RccIrLowerWideValue* result);
static bool lower_wide_scalar_store(
    RccIrLowerContext* context, RccIrLowerValue address,
    RccIrLowerWideValue value);
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
static RccIrLowerValue lower_load_address(
    RccIrLowerContext* context, RccIrLowerValue address,
    const Type* ast_type);
static RccIrLowerValue lower_cxx_typeid_address(
    RccIrLowerContext* context, const Expr* expression);
static RccIrLowerValue lower_typeinfo_field(
    RccIrLowerContext* context, const Expr* expression,
    uint64_t byte_offset, const Type* field_type);
static RccIrLowerValue lower_typeinfo_before(
    RccIrLowerContext* context, const Expr* expression);

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
    if (!type || !result || type->cleanup_function ||
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
        case TYPE_VECTOR:
        case TYPE_FUNC:
        case TYPE_STRUCT:
        case TYPE_UNION:
            /* The verified SSA subset is scalar; the production x86
             * backend owns vector lowering and remains the fallback. */
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
    local->wide_ssa = false;
    memset(&local->wide_value, 0, sizeof(local->wide_value));
    local->next = context->locals;
    context->locals = local;
    return true;
}

static bool lower_set_wide_local_ssa(
    RccIrLowerContext* context, const Decl* declaration,
    RccIrLowerWideValue value) {
    RccIrLowerLocal* local = lower_find_local(context, declaration);
    if (!local || !value.valid || !lower_i686_wide_scalar_type(
            declaration ? declaration->type : NULL)) return false;
    local->wide_ssa = true;
    local->wide_value = value;
    return true;
}

static void lower_release_locals(RccIrLowerLocal* local) {
    while (local) {
        RccIrLowerLocal* next = local->next;
        rcc_free(local);
        local = next;
    }
}

static void lower_release_labels(RccIrLowerLabel* label) {
    while (label) {
        RccIrLowerLabel* next = label->next;
        rcc_free(label);
        label = next;
    }
}

static RccIrLowerLabel* lower_find_label_name(
    RccIrLowerContext* context, const char* name) {
    if (!context || !name) return NULL;
    for (RccIrLowerLabel* label = context->labels; label;
         label = label->next) {
        if (strcmp(label->name, name) == 0) return label;
    }
    return NULL;
}

static RccIrLowerLabel* lower_find_label_statement(
    RccIrLowerContext* context, const Stmt* statement) {
    if (!context || !statement) return NULL;
    for (RccIrLowerLabel* label = context->labels; label;
         label = label->next) {
        if (label->statement == statement) return label;
    }
    return NULL;
}

static bool lower_collect_labels(RccIrLowerContext* context,
                                 const Stmt* statement) {
    const StmtList* item;
    if (!context || !statement) return true;
    switch (statement->kind) {
        case STMT_LABEL: {
            RccIrLowerLabel* label;
            if (!statement->label_name ||
                lower_find_label_name(context, statement->label_name)) {
                context->unsupported = true;
                return false;
            }
            label = rcc_alloc(sizeof(*label));
            label->name = statement->label_name;
            label->statement = statement;
            label->block = rcc_ir_block_add(context->function, "label");
            if (!label->block) {
                rcc_free(label);
                context->unsupported = true;
                return false;
            }
            label->next = context->labels;
            context->labels = label;
            return lower_collect_labels(context, statement->label_stmt);
        }
        case STMT_BLOCK:
            for (item = statement->block_stmts; item; item = item->next) {
                if (!lower_collect_labels(context, item->stmt)) return false;
            }
            return true;
        case STMT_IF:
            return lower_collect_labels(context, statement->if_then) &&
                lower_collect_labels(context, statement->if_else);
        case STMT_WHILE:
        case STMT_DO:
            return lower_collect_labels(context, statement->while_body);
        case STMT_FOR:
            return lower_collect_labels(context, statement->for_init) &&
                lower_collect_labels(context, statement->for_body);
        case STMT_SWITCH:
            /* A goto may enter a switch through an ordinary label.  The
             * switch lowerer owns case/default entry blocks, but it does not
             * yet model arbitrary labels interleaved with those entries.  Do
             * not pre-create unterminated verified blocks for that shape;
             * lower_statement will report the label as outside the verified
             * subset and the production AST backend will handle it. */
            return true;
        case STMT_CASE:
            return lower_collect_labels(context, statement->case_stmt);
        case STMT_DEFAULT:
            return lower_collect_labels(context, statement->default_stmt);
        case STMT_TRY:
            if (!lower_collect_labels(context, statement->try_body)) {
                return false;
            }
            for (const CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                if (!lower_collect_labels(context, handler->body)) {
                    return false;
                }
            }
            return true;
        default:
            return true;
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

static RccIrLowerValue lower_adjusted_pointer(
    RccIrLowerContext* context, RccIrLowerValue pointer,
    int adjustment) {
    RccIrType pointer_type = rcc_ir_type_pointer(0u);
    RccIrType integer_type = rcc_ir_type_integer(
        (uint16_t)(g_opts.target_arch == ARCH_X64 ? 64u : 32u));
    RccIrLowerValue adjusted;
    RccIrLowerValue condition;
    RccIrLowerValue zero_integer;
    RccIrLowerValue zero_pointer;
    RccIrInstruction* zero_cast;
    RccIrInstruction* select;
    RccIrValue operands[3];
    if (!pointer.valid || pointer.type.kind != RCC_IR_TYPE_POINTER) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    if (adjustment == 0) return pointer;
    adjusted = lower_byte_offset_address(
        context, pointer, (uint64_t)(int64_t)adjustment);
    condition = lower_truth(context, pointer);
    zero_integer = lower_integer_constant(
        context, integer_type, true, 0u);
    if (!adjusted.valid || !condition.valid || !zero_integer.valid) {
        return lower_invalid_value();
    }
    zero_cast = lower_append(context, RCC_IR_INT_TO_PTR, pointer_type,
                             &zero_integer.value, 1u, NULL, 0u);
    if (!zero_cast) return lower_invalid_value();
    zero_pointer = lower_value(zero_cast->result, pointer_type, true);
    operands[0] = condition.value;
    operands[1] = adjusted.value;
    operands[2] = zero_pointer.value;
    select = lower_append(context, RCC_IR_SELECT, pointer_type,
                          operands, 3u, NULL, 0u);
    if (!select) return lower_invalid_value();
    return lower_value(select->result, pointer_type, true);
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
        if (expression->ident_decl && expression->ident_decl->type &&
            expression->ident_decl->type->is_reference) {
            return lower_load_address(
                context,
                lower_value(local->address, rcc_ir_type_pointer(0u), true),
                expression->ident_decl->type);
        }
        if (local->wide_ssa && lower_i686_wide_scalar_type(
                expression->ident_decl ? expression->ident_decl->type : NULL)) {
            RccIrLowerValue address = lower_value(
                local->address, rcc_ir_type_pointer(0u), true);
            /* An address escape invalidates the pair as an SSA source.  Keep
             * the stack mirror coherent before returning it so calls and
             * pointer expressions observe the current two-word value. */
            if (!lower_wide_scalar_store(
                    context, address, local->wide_value)) {
                context->unsupported = true;
                return lower_invalid_value();
            }
            local->wide_ssa = false;
        }
        return lower_value(local->address, rcc_ir_type_pointer(0u), true);
    }
    if (expression->kind == EXPR_CAST && expression->type &&
        expression->type->is_reference &&
        (expression->cxx_cast_kind == CXX_CAST_NONE ||
         expression->cxx_cast_kind == CXX_CAST_CONST ||
         expression->cxx_cast_kind == CXX_CAST_DYNAMIC)) {
        RccIrLowerValue address = lower_lvalue_address(
            context, expression->cast_expr);
        if (expression->cxx_pointer_adjustment_valid) {
            return lower_adjusted_pointer(
                context, address, expression->cxx_pointer_adjustment);
        }
        return address;
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

static RccIrLowerValue lower_cxx_typeid_address(
    RccIrLowerContext* context, const Expr* expression) {
    RccIrInstruction* address;
    if (!context || !expression || expression->kind != EXPR_CXX_TYPEID ||
        expression->cxx_typeid_dynamic ||
        !expression->cxx_typeid_symbol ||
        !expression->cxx_typeid_symbol[0]) {
        if (context) context->unsupported = true;
        return lower_invalid_value();
    }
    address = lower_append(
        context, RCC_IR_SYMBOL_ADDRESS, rcc_ir_type_pointer(0u),
        NULL, 0u, NULL, 0u);
    if (!address) return lower_invalid_value();
    rcc_ir_set_callee(address, expression->cxx_typeid_symbol);
    return lower_value(address->result, rcc_ir_type_pointer(0u), true);
}

static RccIrLowerValue lower_typeinfo_field(
    RccIrLowerContext* context, const Expr* expression,
    uint64_t byte_offset, const Type* field_type) {
    RccIrLowerValue address;
    address = lower_cxx_typeid_address(
        context, expression && expression->call_func
            ? expression->call_func->member_base : NULL);
    address = lower_byte_offset_address(context, address, byte_offset);
    return lower_load_address(context, address, field_type);
}

static RccIrLowerValue lower_typeinfo_before(
    RccIrLowerContext* context, const Expr* expression) {
    const Expr* base;
    const Expr* argument;
    RccIrLowerValue left;
    RccIrLowerValue right;
    RccIrValue operands[2];
    RccIrInstruction* compare;
    RccIrLowerValue result;
    if (!context || !expression || !expression->call_func ||
        !expression->call_func->member_base ||
        !expression->call_args || expression->call_args->next ||
        !expression->call_args->expr) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    base = expression->call_func->member_base;
    argument = expression->call_args->expr;
    if (base->kind != EXPR_CXX_TYPEID || base->cxx_typeid_dynamic ||
        argument->kind != EXPR_CXX_TYPEID || argument->cxx_typeid_dynamic) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    left = lower_cxx_typeid_address(context, base);
    right = lower_cxx_typeid_address(context, argument);
    if (!left.valid || !right.valid) return lower_invalid_value();
    operands[0] = left.value;
    operands[1] = right.value;
    compare = lower_append(
        context, RCC_IR_ICMP, rcc_ir_type_integer(1u),
        operands, 2u, NULL, 0u);
    if (!compare) return lower_invalid_value();
    rcc_ir_set_predicate(compare, RCC_IR_ICMP_ULT);
    result = lower_value(compare->result, rcc_ir_type_integer(1u), true);
    return lower_cast(context, result, expression->type);
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

static bool lower_i686_wide_scalar_type(const Type* type) {
    return g_opts.target_arch == ARCH_X86 && type &&
        type_is_integer((Type*)type) && type->size == 8;
}

static bool lower_wide_scalar_constant(
    RccIrLowerContext* context, const Expr* expression,
    RccIrLowerWideValue* result) {
    int64_t constant;
    uint64_t bits;
    RccIrType word_type = rcc_ir_type_integer(32u);
    if (!expression || !result || !expr_eval_integer_constant(
            (Expr*)expression, &constant)) return false;
    bits = (uint64_t)constant;
    if (expression->type && type_is_integer(expression->type) &&
        expression->type->size > 0 && expression->type->size < 8) {
        unsigned width = (unsigned)expression->type->size * 8u;
        uint64_t mask = (UINT64_C(1) << width) - 1u;
        bits &= mask;
        if (!expression->type->is_unsigned &&
            (bits & (UINT64_C(1) << (width - 1u))) != 0u) {
            bits |= ~mask;
        }
    }
    result->low = lower_integer_constant(
        context, word_type, true, (uint32_t)bits);
    result->high = lower_integer_constant(
        context, word_type, true, (uint32_t)(bits >> 32u));
    result->is_unsigned = expression->type && expression->type->is_unsigned;
    result->valid = result->low.valid && result->high.valid;
    return result->valid;
}

static bool lower_wide_scalar_load(
    RccIrLowerContext* context, RccIrLowerValue address,
    bool is_unsigned, RccIrLowerWideValue* result) {
    RccIrLowerValue high_address;
    if (!result || !address.valid ||
        address.type.kind != RCC_IR_TYPE_POINTER) return false;
    high_address = lower_byte_offset_address(context, address, 4u);
    result->low = lower_load_address(context, address, type_uint);
    result->high = lower_load_address(context, high_address, type_uint);
    result->is_unsigned = is_unsigned;
    result->valid = result->low.valid && result->high.valid;
    return result->valid;
}

static bool lower_wide_scalar_store(
    RccIrLowerContext* context, RccIrLowerValue address,
    RccIrLowerWideValue value) {
    RccIrLowerValue high_address;
    if (!address.valid || address.type.kind != RCC_IR_TYPE_POINTER ||
        !value.valid) return false;
    high_address = lower_byte_offset_address(context, address, 4u);
    return lower_store_address(context, address, value.low) &&
        lower_store_address(context, high_address, value.high);
}

static bool lower_wide_scalar_binary(
    RccIrLowerContext* context, ExprKind kind,
    RccIrLowerWideValue left, RccIrLowerWideValue right,
    bool is_unsigned, RccIrLowerWideValue* result) {
    RccIrOpcode opcode;
    RccIrValue operands[2];
    RccIrInstruction* low_operation;
    RccIrInstruction* high_operation;
    RccIrInstruction* carry_operation;
    RccIrInstruction* final_high;
    RccIrLowerValue carry;
    if (!result || !left.valid || !right.valid) return false;
    switch (kind) {
        case EXPR_ADD: opcode = RCC_IR_ADD; break;
        case EXPR_SUB: opcode = RCC_IR_SUB; break;
        case EXPR_BITAND: opcode = RCC_IR_AND; break;
        case EXPR_BITOR: opcode = RCC_IR_OR; break;
        case EXPR_BITXOR: opcode = RCC_IR_XOR; break;
        default: return false;
    }
    operands[0] = left.low.value;
    operands[1] = right.low.value;
    low_operation = lower_append(
        context, opcode, rcc_ir_type_integer(32u), operands, 2u,
        NULL, 0u);
    if (!low_operation) return false;
    operands[0] = left.high.value;
    operands[1] = right.high.value;
    high_operation = lower_append(
        context, opcode, rcc_ir_type_integer(32u), operands, 2u,
        NULL, 0u);
    if (!high_operation) return false;
    if (kind == EXPR_ADD || kind == EXPR_SUB) {
        operands[0] = kind == EXPR_ADD
            ? low_operation->result : left.low.value;
        operands[1] = kind == EXPR_ADD
            ? left.low.value : right.low.value;
        carry_operation = lower_append(
            context, RCC_IR_ICMP, rcc_ir_type_integer(1u), operands, 2u,
            NULL, 0u);
        if (!carry_operation) return false;
        rcc_ir_set_predicate(carry_operation, RCC_IR_ICMP_ULT);
        carry = lower_value(
            carry_operation->result, rcc_ir_type_integer(1u), true);
        carry = lower_cast(context, carry, type_uint);
        if (!carry.valid) return false;
        operands[0] = high_operation->result;
        operands[1] = carry.value;
        final_high = lower_append(
            context, kind == EXPR_ADD ? RCC_IR_ADD : RCC_IR_SUB,
            rcc_ir_type_integer(32u), operands, 2u, NULL, 0u);
    } else {
        final_high = high_operation;
    }
    if (!final_high) return false;
    result->low = lower_value(
        low_operation->result, rcc_ir_type_integer(32u), true);
    result->high = lower_value(
        final_high->result, rcc_ir_type_integer(32u), true);
    result->is_unsigned = is_unsigned;
    result->valid = true;
    return true;
}

static RccIrLowerValue lower_wide_scalar_word_operation(
    RccIrLowerContext* context, RccIrOpcode opcode,
    RccIrLowerValue left, RccIrLowerValue right) {
    RccIrValue operands[2];
    RccIrInstruction* instruction;
    if (!left.valid || !right.valid ||
        left.type.kind != RCC_IR_TYPE_INTEGER ||
        left.type.bit_width != 32u ||
        right.type.kind != RCC_IR_TYPE_INTEGER ||
        right.type.bit_width != 32u) {
        return lower_invalid_value();
    }
    operands[0] = left.value;
    operands[1] = right.value;
    instruction = lower_append(
        context, opcode, rcc_ir_type_integer(32u), operands, 2u,
        NULL, 0u);
    if (!instruction) return lower_invalid_value();
    return lower_value(instruction->result,
                       rcc_ir_type_integer(32u), true);
}

static RccIrLowerValue lower_wide_scalar_bswap_word(
    RccIrLowerContext* context, RccIrLowerValue source) {
    static const uint32_t masks[] = {
        UINT32_C(0x000000ff), UINT32_C(0x0000ff00),
        UINT32_C(0x00ff0000), UINT32_C(0xff000000)
    };
    static const unsigned shifts[] = {24u, 8u, 8u, 24u};
    static const RccIrOpcode opcodes[] = {
        RCC_IR_SHL, RCC_IR_SHL, RCC_IR_LSHR, RCC_IR_LSHR
    };
    RccIrLowerValue result = lower_invalid_value();
    for (size_t index = 0u; index < sizeof(masks) / sizeof(masks[0]);
         ++index) {
        RccIrLowerValue mask = lower_integer_constant(
            context, rcc_ir_type_integer(32u), true, masks[index]);
        RccIrLowerValue shift = lower_integer_constant(
            context, rcc_ir_type_integer(32u), true, shifts[index]);
        RccIrLowerValue masked;
        RccIrLowerValue shifted;
        if (!mask.valid || !shift.valid) return lower_invalid_value();
        masked = lower_wide_scalar_word_operation(
            context, RCC_IR_AND, source, mask);
        shifted = lower_wide_scalar_word_operation(
            context, opcodes[index], masked, shift);
        if (!shifted.valid) return lower_invalid_value();
        if (!result.valid) {
            result = shifted;
        } else {
            result = lower_wide_scalar_word_operation(
                context, RCC_IR_OR, result, shifted);
            if (!result.valid) return lower_invalid_value();
        }
    }
    return result;
}

static RccIrLowerWideValue lower_wide_scalar_word_pair(
    RccIrLowerContext* context, RccIrLowerValue word) {
    RccIrLowerWideValue result;
    result.low = word;
    result.high = lower_integer_constant(
        context, rcc_ir_type_integer(32u), true, 0u);
    result.is_unsigned = true;
    result.valid = result.low.valid && result.high.valid;
    return result;
}

static bool lower_wide_scalar_multiply(
    RccIrLowerContext* context, RccIrLowerWideValue left,
    RccIrLowerWideValue right, bool is_unsigned,
    RccIrLowerWideValue* result) {
    RccIrLowerValue mask16;
    RccIrLowerValue shift16;
    RccIrLowerValue zero;
    RccIrLowerValue left0;
    RccIrLowerValue left1;
    RccIrLowerValue right0;
    RccIrLowerValue right1;
    RccIrLowerValue product0;
    RccIrLowerValue product1;
    RccIrLowerValue product2;
    RccIrLowerValue product3;
    RccIrLowerValue cross_left;
    RccIrLowerValue cross_right;
    RccIrLowerWideValue term0;
    RccIrLowerWideValue term1;
    RccIrLowerWideValue term2;
    RccIrLowerWideValue term3;
    RccIrLowerWideValue term4;
    RccIrLowerWideValue term5;
    RccIrLowerWideValue sum;
    if (!result) return false;
    memset(result, 0, sizeof(*result));
    if (!left.valid || !right.valid) return false;
    mask16 = lower_integer_constant(
        context, rcc_ir_type_integer(32u), true, 0xffffu);
    shift16 = lower_integer_constant(
        context, rcc_ir_type_integer(32u), true, 16u);
    zero = lower_integer_constant(
        context, rcc_ir_type_integer(32u), true, 0u);
    if (!mask16.valid || !shift16.valid || !zero.valid) return false;
    left0 = lower_wide_scalar_word_operation(
        context, RCC_IR_AND, left.low, mask16);
    left1 = lower_wide_scalar_word_operation(
        context, RCC_IR_LSHR, left.low, shift16);
    right0 = lower_wide_scalar_word_operation(
        context, RCC_IR_AND, right.low, mask16);
    right1 = lower_wide_scalar_word_operation(
        context, RCC_IR_LSHR, right.low, shift16);
    if (!left0.valid || !left1.valid || !right0.valid || !right1.valid) {
        return false;
    }
    product0 = lower_wide_scalar_word_operation(
        context, RCC_IR_MUL, left0, right0);
    product1 = lower_wide_scalar_word_operation(
        context, RCC_IR_MUL, left0, right1);
    product2 = lower_wide_scalar_word_operation(
        context, RCC_IR_MUL, left1, right0);
    product3 = lower_wide_scalar_word_operation(
        context, RCC_IR_MUL, left1, right1);
    cross_left = lower_wide_scalar_word_operation(
        context, RCC_IR_MUL, left.low, right.high);
    cross_right = lower_wide_scalar_word_operation(
        context, RCC_IR_MUL, left.high, right.low);
    if (!product0.valid || !product1.valid ||
        !product2.valid || !product3.valid ||
        !cross_left.valid || !cross_right.valid) return false;
    term0 = lower_wide_scalar_word_pair(context, product0);
    term1.low = lower_wide_scalar_word_operation(
        context, RCC_IR_SHL, product1, shift16);
    term1.high = lower_wide_scalar_word_operation(
        context, RCC_IR_LSHR, product1, shift16);
    term1.is_unsigned = true;
    term1.valid = term1.low.valid && term1.high.valid;
    term2.low = lower_wide_scalar_word_operation(
        context, RCC_IR_SHL, product2, shift16);
    term2.high = lower_wide_scalar_word_operation(
        context, RCC_IR_LSHR, product2, shift16);
    term2.is_unsigned = true;
    term2.valid = term2.low.valid && term2.high.valid;
    term3.low = zero;
    term3.high = product3;
    term3.is_unsigned = true;
    term3.valid = true;
    term4.low = zero;
    term4.high = cross_left;
    term4.is_unsigned = true;
    term4.valid = true;
    term5.low = zero;
    term5.high = cross_right;
    term5.is_unsigned = true;
    term5.valid = true;
    if (!term0.valid || !term1.valid || !term2.valid || !term3.valid ||
        !term4.valid || !term5.valid ||
        !lower_wide_scalar_binary(
            context, EXPR_ADD, term0, term1, true, &sum) ||
        !lower_wide_scalar_binary(
            context, EXPR_ADD, sum, term2, true, &sum) ||
        !lower_wide_scalar_binary(
            context, EXPR_ADD, sum, term3, true, &sum) ||
        !lower_wide_scalar_binary(
            context, EXPR_ADD, sum, term4, true, &sum) ||
        !lower_wide_scalar_binary(
            context, EXPR_ADD, sum, term5, is_unsigned, result)) {
        return false;
    }
    return true;
}

static RccIrLowerValue lower_wide_scalar_scratch(
    RccIrLowerContext* context) {
    RccIrInstruction* allocation = lower_append(
        context, RCC_IR_ALLOCA, rcc_ir_type_pointer(0u),
        NULL, 0u, NULL, 0u);
    if (!allocation) return lower_invalid_value();
    rcc_ir_set_immediate(allocation, 8u);
    return lower_value(allocation->result,
                       rcc_ir_type_pointer(0u), true);
}

static bool lower_wide_scalar_unsigned_divmod(
    RccIrLowerContext* context, RccIrLowerWideValue dividend,
    RccIrLowerWideValue divisor, bool want_remainder,
    RccIrLowerWideValue* result) {
    RccIrLowerWideValue remainder;
    RccIrLowerWideValue quotient;
    RccIrLowerWideValue source;
    RccIrLowerWideValue shifted_remainder;
    RccIrLowerWideValue subtracted_remainder;
    RccIrLowerValue zero_word;
    RccIrLowerValue one_word;
    RccIrLowerValue bit_shift;
    RccIrLowerValue bit;
    RccIrLowerValue bit_word;
    RccIrLowerValue selected_bit;
    RccIrLowerValue condition;
    RccIrLowerValue remainder_address;
    RccIrLowerValue subtracted_address;
    RccIrLowerValue quotient_address;
    RccIrLowerValue source_address;
    RccIrLowerValue divisor_address;
    RccIrLowerWideValue saved_remainder;
    RccIrLowerWideValue saved_subtracted;
    RccIrLowerWideValue saved_quotient;
    RccIrLowerWideValue saved_source;
    RccIrLowerWideValue compare_remainder;
    RccIrLowerWideValue compare_divisor;
    RccIrLowerWideValue current_divisor;
    if (!result || !dividend.valid || !divisor.valid) return false;
    memset(result, 0, sizeof(*result));
    zero_word = lower_integer_constant(
        context, rcc_ir_type_integer(32u), true, 0u);
    one_word = lower_integer_constant(
        context, rcc_ir_type_integer(32u), true, 1u);
    bit_shift = lower_integer_constant(
        context, rcc_ir_type_integer(32u), true, 31u);
    if (!zero_word.valid || !one_word.valid || !bit_shift.valid) {
        return false;
    }
    remainder_address = lower_wide_scalar_scratch(context);
    subtracted_address = lower_wide_scalar_scratch(context);
    quotient_address = lower_wide_scalar_scratch(context);
    source_address = lower_wide_scalar_scratch(context);
    divisor_address = lower_wide_scalar_scratch(context);
    if (!remainder_address.valid || !subtracted_address.valid ||
        !quotient_address.valid || !source_address.valid ||
        !divisor_address.valid ||
        !lower_wide_scalar_store(context, divisor_address, divisor)) {
        return false;
    }
    remainder.low = zero_word;
    remainder.high = zero_word;
    remainder.is_unsigned = true;
    remainder.valid = true;
    quotient = remainder;
    if (!lower_wide_scalar_store(
            context, source_address, dividend) ||
        !lower_wide_scalar_load(
            context, source_address, true, &source)) return false;
    for (size_t step = 0u; step < 64u; ++step) {
        if (!lower_wide_scalar_load(
                context, divisor_address, true, &current_divisor)) {
            return false;
        }
        bit = lower_wide_scalar_word_operation(
            context, RCC_IR_LSHR, source.high, bit_shift);
        bit = lower_wide_scalar_word_operation(
            context, RCC_IR_AND, bit, one_word);
        if (!bit.valid) return false;
        bit_word = lower_wide_scalar_word_pair(context, bit).low;
        shifted_remainder = remainder;
        if (!lower_wide_scalar_shift(
                context, EXPR_LSHIFT, shifted_remainder,
                one_word, true, &shifted_remainder)) return false;
        shifted_remainder.low = lower_wide_scalar_word_operation(
            context, RCC_IR_OR, shifted_remainder.low, bit_word);
        if (!shifted_remainder.low.valid ||
            !shifted_remainder.high.valid) {
            return false;
        }
        remainder = shifted_remainder;
        if (!lower_wide_scalar_shift(
                context, EXPR_LSHIFT, quotient,
                one_word, true, &quotient) ||
            !lower_wide_scalar_shift(
                context, EXPR_LSHIFT, source,
                one_word, true, &source)) return false;
        subtracted_remainder = remainder;
        if (!lower_wide_scalar_binary(
                context, EXPR_SUB, remainder, current_divisor,
                true, &subtracted_remainder)) return false;
        if (!lower_wide_scalar_store(
                context, remainder_address, remainder) ||
            !lower_wide_scalar_store(
                context, subtracted_address, subtracted_remainder) ||
            !lower_wide_scalar_store(context, quotient_address, quotient) ||
            !lower_wide_scalar_store(context, source_address, source)) {
            return false;
        }
        compare_remainder = lower_wide_scalar_load(
            context, remainder_address, true, &compare_remainder)
            ? compare_remainder : (RccIrLowerWideValue){0};
        compare_divisor = lower_wide_scalar_load(
            context, divisor_address, true, &compare_divisor)
            ? compare_divisor : (RccIrLowerWideValue){0};
        condition = lower_wide_scalar_compare(
            context, EXPR_GE, compare_remainder, compare_divisor,
            true, &condition)
            ? condition : lower_invalid_value();
        if (!condition.valid) return false;
        saved_remainder = lower_wide_scalar_load(
            context, remainder_address, true, &saved_remainder)
            ? saved_remainder : (RccIrLowerWideValue){0};
        saved_subtracted = lower_wide_scalar_load(
            context, subtracted_address, true, &saved_subtracted)
            ? saved_subtracted : (RccIrLowerWideValue){0};
        saved_quotient = lower_wide_scalar_load(
            context, quotient_address, true, &saved_quotient)
            ? saved_quotient : (RccIrLowerWideValue){0};
        saved_source = lower_wide_scalar_load(
            context, source_address, true, &saved_source)
            ? saved_source : (RccIrLowerWideValue){0};
        if (!saved_remainder.valid || !saved_subtracted.valid ||
            !saved_quotient.valid || !saved_source.valid) return false;
        remainder.low = lower_wide_scalar_word_select(
            context, condition, saved_subtracted.low, saved_remainder.low);
        remainder.high = lower_wide_scalar_word_select(
            context, condition, saved_subtracted.high, saved_remainder.high);
        quotient = saved_quotient;
        source = saved_source;
        if (!remainder.low.valid || !remainder.high.valid) return false;
        selected_bit = lower_wide_scalar_word_select(
            context, condition, one_word, zero_word);
        quotient.low = lower_wide_scalar_word_operation(
            context, RCC_IR_OR, quotient.low, selected_bit);
        if (!selected_bit.valid || !quotient.low.valid) return false;
    }
    *result = want_remainder ? remainder : quotient;
    result->is_unsigned = true;
    result->valid = result->low.valid && result->high.valid;
    return result->valid;
}

static bool lower_wide_scalar_divmod(
    RccIrLowerContext* context, ExprKind kind,
    RccIrLowerWideValue dividend, RccIrLowerWideValue divisor,
    bool is_unsigned, RccIrLowerWideValue* result) {
    RccIrLowerValue zero_word;
    RccIrLowerValue left_sign;
    RccIrLowerValue right_sign;
    RccIrLowerValue result_sign;
    RccIrLowerValue left_address;
    RccIrLowerValue right_address;
    RccIrLowerValue left_sign_address;
    RccIrLowerValue right_sign_address;
    RccIrLowerWideValue left_saved;
    RccIrLowerWideValue right_saved;
    RccIrLowerWideValue left_original;
    RccIrLowerWideValue right_original;
    RccIrLowerWideValue zero_value;
    RccIrLowerWideValue left_magnitude;
    RccIrLowerWideValue right_magnitude;
    RccIrLowerWideValue unsigned_result;
    RccIrLowerWideValue negative_result;
    RccIrLowerWideValue left_sign_value;
    RccIrLowerWideValue right_sign_value;
    RccIrLowerWideValue left_sign_mask;
    RccIrLowerWideValue right_sign_mask;
    RccIrLowerWideValue left_xored;
    RccIrLowerWideValue right_xored;
    bool want_remainder = kind == EXPR_MOD;
    if (!result || !dividend.valid || !divisor.valid ||
        (kind != EXPR_DIV && kind != EXPR_MOD)) return false;
    if (is_unsigned) {
        return lower_wide_scalar_unsigned_divmod(
            context, dividend, divisor, want_remainder, result);
    }
    zero_word = lower_integer_constant(
        context, rcc_ir_type_integer(32u), true, 0u);
    if (!zero_word.valid) return false;
    zero_value.low = zero_word;
    zero_value.high = zero_word;
    zero_value.is_unsigned = true;
    zero_value.valid = true;
    left_address = lower_wide_scalar_scratch(context);
    right_address = lower_wide_scalar_scratch(context);
    left_sign_address = lower_wide_scalar_scratch(context);
    right_sign_address = lower_wide_scalar_scratch(context);
    if (!left_address.valid || !right_address.valid ||
        !left_sign_address.valid || !right_sign_address.valid ||
        !lower_wide_scalar_store(context, left_address, dividend) ||
        !lower_wide_scalar_store(context, right_address, divisor)) return false;
    if (!lower_wide_scalar_load(
            context, left_address, true, &left_saved)) return false;
    left_sign = lower_wide_scalar_compare(
        context, EXPR_LT, left_saved, zero_value, false, &left_sign)
        ? left_sign : lower_invalid_value();
    if (!left_sign.valid || !lower_store_address(
            context, left_sign_address, left_sign) ||
        !lower_wide_scalar_load(
            context, right_address, true, &right_saved)) return false;
    right_sign = lower_wide_scalar_compare(
        context, EXPR_LT, right_saved, zero_value, false, &right_sign)
        ? right_sign : lower_invalid_value();
    if (!right_sign.valid || !lower_store_address(
            context, right_sign_address, right_sign) ||
        !(left_sign = lower_load_address(
            context, left_sign_address, type_bool)).valid ||
        !(right_sign = lower_load_address(
            context, right_sign_address, type_bool)).valid) return false;
    if (!lower_wide_scalar_load(
            context, left_address, true, &left_original) ||
        !lower_wide_scalar_load(
            context, right_address, true, &right_original)) return false;
    left_sign_value.low = lower_cast(context, left_sign, type_uint);
    left_sign_value.high = zero_word;
    left_sign_value.is_unsigned = true;
    left_sign_value.valid = left_sign_value.low.valid;
    right_sign_value.low = lower_cast(context, right_sign, type_uint);
    right_sign_value.high = zero_word;
    right_sign_value.is_unsigned = true;
    right_sign_value.valid = right_sign_value.low.valid;
    if (!left_sign_value.valid || !right_sign_value.valid ||
        !lower_wide_scalar_binary(
            context, EXPR_SUB, zero_value, left_sign_value,
            true, &left_sign_mask) ||
        !lower_wide_scalar_binary(
            context, EXPR_SUB, zero_value, right_sign_value,
            true, &right_sign_mask) ||
        !lower_wide_scalar_binary(
            context, EXPR_BITXOR, left_original, left_sign_mask,
            true, &left_xored) ||
        !lower_wide_scalar_binary(
            context, EXPR_BITXOR, right_original, right_sign_mask,
            true, &right_xored) ||
        !lower_wide_scalar_binary(
            context, EXPR_ADD, left_xored, left_sign_value,
            true, &left_magnitude) ||
        !lower_wide_scalar_binary(
            context, EXPR_ADD, right_xored, right_sign_value,
            true, &right_magnitude)) return false;
    if (!lower_wide_scalar_unsigned_divmod(
            context, left_magnitude, right_magnitude,
            want_remainder, &unsigned_result)) return false;
    if (!lower_wide_scalar_binary(
            context, EXPR_SUB, zero_value, unsigned_result,
            true, &negative_result)) return false;
    if (!lower_wide_scalar_load(
            context, left_address, true, &left_original) ||
        !lower_wide_scalar_load(
            context, right_address, true, &right_original)) return false;
    left_sign = lower_wide_scalar_compare(
        context, EXPR_LT, left_original, zero_value, false, &left_sign)
        ? left_sign : lower_invalid_value();
    right_sign = lower_wide_scalar_compare(
        context, EXPR_LT, right_original, zero_value, false, &right_sign)
        ? right_sign : lower_invalid_value();
    result_sign = want_remainder
        ? left_sign
        : lower_wide_scalar_bool_operation(
              context, RCC_IR_XOR, left_sign, right_sign);
    if (!result_sign.valid) return false;
    result->low = lower_wide_scalar_word_select(
        context, result_sign, negative_result.low, unsigned_result.low);
    if (!result->low.valid) return false;
    if (!lower_wide_scalar_load(
            context, left_address, true, &left_original) ||
        !lower_wide_scalar_load(
            context, right_address, true, &right_original)) return false;
    left_sign = lower_wide_scalar_compare(
        context, EXPR_LT, left_original, zero_value, false, &left_sign)
        ? left_sign : lower_invalid_value();
    right_sign = lower_wide_scalar_compare(
        context, EXPR_LT, right_original, zero_value, false, &right_sign)
        ? right_sign : lower_invalid_value();
    result_sign = want_remainder
        ? left_sign
        : lower_wide_scalar_bool_operation(
              context, RCC_IR_XOR, left_sign, right_sign);
    if (!result_sign.valid) return false;
    result->high = lower_wide_scalar_word_select(
        context, result_sign, negative_result.high, unsigned_result.high);
    result->is_unsigned = false;
    result->valid = result->low.valid && result->high.valid;
    return result->valid;
}

static RccIrLowerValue lower_wide_scalar_compare_words(
    RccIrLowerContext* context, RccIrLowerValue left,
    RccIrLowerValue right, RccIrIntPredicate predicate) {
    RccIrValue operands[2];
    RccIrInstruction* instruction;
    if (!left.valid || !right.valid ||
        left.type.kind != RCC_IR_TYPE_INTEGER ||
        left.type.bit_width != 32u ||
        right.type.kind != RCC_IR_TYPE_INTEGER ||
        right.type.bit_width != 32u) {
        return lower_invalid_value();
    }
    operands[0] = left.value;
    operands[1] = right.value;
    instruction = lower_append(
        context, RCC_IR_ICMP, rcc_ir_type_integer(1u), operands, 2u,
        NULL, 0u);
    if (!instruction) return lower_invalid_value();
    rcc_ir_set_predicate(instruction, predicate);
    return lower_value(instruction->result,
                       rcc_ir_type_integer(1u), true);
}

static RccIrLowerValue lower_wide_scalar_bool_operation(
    RccIrLowerContext* context, RccIrOpcode opcode,
    RccIrLowerValue left, RccIrLowerValue right) {
    RccIrValue operands[2];
    RccIrInstruction* instruction;
    if (!left.valid || !right.valid ||
        left.type.kind != RCC_IR_TYPE_INTEGER ||
        left.type.bit_width != 1u ||
        right.type.kind != RCC_IR_TYPE_INTEGER ||
        right.type.bit_width != 1u) {
        return lower_invalid_value();
    }
    operands[0] = left.value;
    operands[1] = right.value;
    instruction = lower_append(
        context, opcode, rcc_ir_type_integer(1u), operands, 2u,
        NULL, 0u);
    if (!instruction) return lower_invalid_value();
    return lower_value(instruction->result,
                       rcc_ir_type_integer(1u), true);
}

static RccIrLowerValue lower_wide_scalar_word_select(
    RccIrLowerContext* context, RccIrLowerValue condition,
    RccIrLowerValue then_value, RccIrLowerValue else_value) {
    RccIrValue operands[3];
    RccIrInstruction* instruction;
    if (!condition.valid || !then_value.valid || !else_value.valid ||
        condition.type.kind != RCC_IR_TYPE_INTEGER ||
        condition.type.bit_width != 1u ||
        then_value.type.kind != RCC_IR_TYPE_INTEGER ||
        then_value.type.bit_width != 32u ||
        !rcc_ir_type_equal(then_value.type, else_value.type)) {
        return lower_invalid_value();
    }
    operands[0] = condition.value;
    operands[1] = then_value.value;
    operands[2] = else_value.value;
    instruction = lower_append(
        context, RCC_IR_SELECT, rcc_ir_type_integer(32u), operands, 3u,
        NULL, 0u);
    if (!instruction) return lower_invalid_value();
    return lower_value(instruction->result,
                       rcc_ir_type_integer(32u), true);
}

static bool lower_wide_scalar_compare(
    RccIrLowerContext* context, ExprKind kind,
    RccIrLowerWideValue left, RccIrLowerWideValue right,
    bool is_unsigned, RccIrLowerValue* result) {
    RccIrLowerValue high_equal;
    RccIrLowerValue low_equal;
    RccIrLowerValue high_not_equal;
    RccIrLowerValue low_not_equal;
    RccIrLowerValue high_order;
    RccIrLowerValue low_order;
    RccIrLowerValue ordered_low;
    RccIrLowerValue ordered;
    RccIrIntPredicate high_predicate;
    RccIrIntPredicate low_predicate;
    if (!result) return false;
    *result = lower_invalid_value();
    if (!left.valid || !right.valid) return false;
    if (kind == EXPR_NE) {
        high_not_equal = lower_wide_scalar_compare_words(
            context, left.high, right.high, RCC_IR_ICMP_NE);
        low_not_equal = lower_wide_scalar_compare_words(
            context, left.low, right.low, RCC_IR_ICMP_NE);
        if (!high_not_equal.valid || !low_not_equal.valid) return false;
        *result = lower_wide_scalar_bool_operation(
            context, RCC_IR_OR, high_not_equal, low_not_equal);
        return result->valid;
    }
    high_equal = lower_wide_scalar_compare_words(
        context, left.high, right.high, RCC_IR_ICMP_EQ);
    low_equal = lower_wide_scalar_compare_words(
        context, left.low, right.low, RCC_IR_ICMP_EQ);
    if (!high_equal.valid || !low_equal.valid) return false;
    if (kind == EXPR_EQ) {
        *result = lower_wide_scalar_bool_operation(
            context, RCC_IR_AND,
            high_equal, low_equal);
        return result->valid;
    }
    switch (kind) {
        case EXPR_LT:
            high_predicate = is_unsigned
                ? RCC_IR_ICMP_ULT : RCC_IR_ICMP_SLT;
            low_predicate = RCC_IR_ICMP_ULT;
            break;
        case EXPR_LE:
            high_predicate = is_unsigned
                ? RCC_IR_ICMP_ULT : RCC_IR_ICMP_SLT;
            low_predicate = RCC_IR_ICMP_ULE;
            break;
        case EXPR_GT:
            high_predicate = is_unsigned
                ? RCC_IR_ICMP_UGT : RCC_IR_ICMP_SGT;
            low_predicate = RCC_IR_ICMP_UGT;
            break;
        case EXPR_GE:
            high_predicate = is_unsigned
                ? RCC_IR_ICMP_UGT : RCC_IR_ICMP_SGT;
            low_predicate = RCC_IR_ICMP_UGE;
            break;
        default:
            return false;
    }
    high_order = lower_wide_scalar_compare_words(
        context, left.high, right.high, high_predicate);
    low_order = lower_wide_scalar_compare_words(
        context, left.low, right.low, low_predicate);
    ordered_low = lower_wide_scalar_bool_operation(
        context, RCC_IR_AND, high_equal, low_order);
    ordered = lower_wide_scalar_bool_operation(
        context, RCC_IR_OR, high_order, ordered_low);
    if (!ordered.valid) return false;
    *result = ordered;
    return true;
}

static bool lower_wide_scalar_shift(
    RccIrLowerContext* context, ExprKind kind,
    RccIrLowerWideValue value, RccIrLowerValue amount,
    bool is_unsigned, RccIrLowerWideValue* result) {
    RccIrLowerValue count;
    RccIrLowerValue thirty_two;
    RccIrLowerValue thirty_one;
    RccIrLowerValue zero;
    RccIrLowerValue low_count;
    RccIrLowerValue cross_count;
    RccIrLowerValue large_count;
    RccIrLowerValue is_small;
    RccIrLowerValue has_word_shift;
    RccIrLowerValue small_low;
    RccIrLowerValue small_high;
    RccIrLowerValue large_low;
    RccIrLowerValue large_high;
    RccIrLowerValue cross;
    RccIrLowerValue same_high;
    RccIrLowerValue selected_low;
    RccIrLowerValue selected_high;
    RccIrOpcode low_opcode;
    RccIrOpcode high_opcode;
    if (!result) return false;
    memset(result, 0, sizeof(*result));
    if (!value.valid || !amount.valid ||
        amount.type.kind != RCC_IR_TYPE_INTEGER ||
        amount.type.bit_width > 32u) return false;
    count = lower_cast(context, amount, type_uint);
    thirty_two = lower_integer_constant(
        context, rcc_ir_type_integer(32u), true, 32u);
    thirty_one = lower_integer_constant(
        context, rcc_ir_type_integer(32u), true, 31u);
    zero = lower_integer_constant(
        context, rcc_ir_type_integer(32u), true, 0u);
    if (!count.valid || !thirty_two.valid || !thirty_one.valid ||
        !zero.valid) return false;
    low_count = lower_wide_scalar_word_operation(
        context, RCC_IR_AND, count, thirty_one);
    cross_count = lower_wide_scalar_word_operation(
        context, RCC_IR_SUB, thirty_two, low_count);
    cross_count = lower_wide_scalar_word_operation(
        context, RCC_IR_AND, cross_count, thirty_one);
    large_count = lower_wide_scalar_word_operation(
        context, RCC_IR_SUB, count, thirty_two);
    large_count = lower_wide_scalar_word_operation(
        context, RCC_IR_AND, large_count, thirty_one);
    is_small = lower_wide_scalar_compare_words(
        context, count, thirty_two, RCC_IR_ICMP_ULT);
    has_word_shift = lower_wide_scalar_compare_words(
        context, low_count, zero, RCC_IR_ICMP_NE);
    if (!low_count.valid || !cross_count.valid || !large_count.valid ||
        !is_small.valid || !has_word_shift.valid) return false;
    if (kind == EXPR_LSHIFT) {
        low_opcode = RCC_IR_SHL;
        high_opcode = RCC_IR_SHL;
        small_low = lower_wide_scalar_word_operation(
            context, low_opcode, value.low, low_count);
        same_high = lower_wide_scalar_word_operation(
            context, high_opcode, value.high, low_count);
        cross = lower_wide_scalar_word_operation(
            context, RCC_IR_LSHR, value.low, cross_count);
        cross = lower_wide_scalar_word_select(
            context, has_word_shift, cross, zero);
        small_high = lower_wide_scalar_word_operation(
            context, RCC_IR_OR, same_high, cross);
        large_low = zero;
        large_high = lower_wide_scalar_word_operation(
            context, RCC_IR_SHL, value.low, large_count);
    } else if (kind == EXPR_RSHIFT) {
        low_opcode = RCC_IR_LSHR;
        high_opcode = is_unsigned ? RCC_IR_LSHR : RCC_IR_ASHR;
        small_low = lower_wide_scalar_word_operation(
            context, low_opcode, value.low, low_count);
        same_high = lower_wide_scalar_word_operation(
            context, high_opcode, value.high, low_count);
        cross = lower_wide_scalar_word_operation(
            context, RCC_IR_SHL, value.high, cross_count);
        cross = lower_wide_scalar_word_select(
            context, has_word_shift, cross, zero);
        small_low = lower_wide_scalar_word_operation(
            context, RCC_IR_OR, small_low, cross);
        small_high = same_high;
        large_low = lower_wide_scalar_word_operation(
            context, high_opcode, value.high, large_count);
        large_high = is_unsigned
            ? zero
            : lower_wide_scalar_word_operation(
                  context, RCC_IR_ASHR, value.high, thirty_one);
    } else {
        return false;
    }
    if (!small_low.valid || !small_high.valid || !large_low.valid ||
        !large_high.valid) return false;
    selected_low = lower_wide_scalar_word_select(
        context, is_small, small_low, large_low);
    selected_high = lower_wide_scalar_word_select(
        context, is_small, small_high, large_high);
    if (!selected_low.valid || !selected_high.valid) return false;
    result->low = selected_low;
    result->high = selected_high;
    result->is_unsigned = value.is_unsigned;
    result->valid = true;
    return true;
}

static RccIrLowerValue lower_wide_scalar_truth(
    RccIrLowerContext* context, RccIrLowerWideValue value) {
    RccIrLowerValue zero;
    RccIrLowerValue high_nonzero;
    RccIrLowerValue low_nonzero;
    if (!value.valid) return lower_invalid_value();
    zero = lower_integer_constant(
        context, rcc_ir_type_integer(32u), true, 0u);
    if (!zero.valid) return lower_invalid_value();
    high_nonzero = lower_wide_scalar_compare_words(
        context, value.high, zero, RCC_IR_ICMP_NE);
    low_nonzero = lower_wide_scalar_compare_words(
        context, value.low, zero, RCC_IR_ICMP_NE);
    if (!high_nonzero.valid || !low_nonzero.valid) {
        return lower_invalid_value();
    }
    return lower_wide_scalar_bool_operation(
        context, RCC_IR_OR, high_nonzero, low_nonzero);
}

static bool lower_wide_scalar_conditional_expression(
    RccIrLowerContext* context, const Expr* expression,
    RccIrLowerWideValue* result) {
    RccIrLowerValue condition;
    RccIrLowerWideValue condition_wide;
    RccIrLowerWideValue then_value;
    RccIrLowerWideValue else_value;
    RccIrBlock* then_block;
    RccIrBlock* else_block;
    RccIrBlock* merge_block;
    RccIrBlock* then_end;
    RccIrBlock* else_end;
    RccIrValue operands[2];
    RccIrBlockId targets[2];
    RccIrInstruction* low_phi;
    RccIrInstruction* high_phi;
    if (result) memset(result, 0, sizeof(*result));
    if (!context || !expression || !result || !expression->type ||
        !lower_i686_wide_scalar_type(expression->type)) return false;
    if (expression->cond_test && lower_i686_wide_scalar_type(
            expression->cond_test->type)) {
        if (!lower_wide_scalar_expression(
                context, expression->cond_test, &condition_wide)) {
            return false;
        }
        condition = lower_wide_scalar_truth(context, condition_wide);
    } else {
        condition = lower_expression(context, expression->cond_test);
    }
    if (!condition.valid) return false;
    then_block = rcc_ir_block_add(context->function, "wide.cond.then");
    else_block = rcc_ir_block_add(context->function, "wide.cond.else");
    merge_block = rcc_ir_block_add(context->function, "wide.cond.end");
    if (!then_block || !else_block || !merge_block ||
        !lower_conditional_branch(context, condition, then_block->id,
                                  else_block->id)) {
        return false;
    }

    context->current = then_block;
    context->terminated = false;
    if (!lower_wide_scalar_expression(
            context, expression->cond_then, &then_value) ||
        context->terminated || !lower_branch(context, merge_block->id)) {
        return false;
    }
    then_end = context->current;

    context->current = else_block;
    context->terminated = false;
    if (!lower_wide_scalar_expression(
            context, expression->cond_else, &else_value) ||
        context->terminated || !lower_branch(context, merge_block->id)) {
        return false;
    }
    else_end = context->current;

    context->current = merge_block;
    context->terminated = false;
    operands[0] = then_value.low.value;
    operands[1] = else_value.low.value;
    targets[0] = then_end->id;
    targets[1] = else_end->id;
    low_phi = lower_append(
        context, RCC_IR_PHI, rcc_ir_type_integer(32u), operands, 2u,
        targets, 2u);
    if (!low_phi) return false;
    operands[0] = then_value.high.value;
    operands[1] = else_value.high.value;
    high_phi = lower_append(
        context, RCC_IR_PHI, rcc_ir_type_integer(32u), operands, 2u,
        targets, 2u);
    if (!high_phi) return false;
    result->low = lower_value(
        low_phi->result, rcc_ir_type_integer(32u), true);
    result->high = lower_value(
        high_phi->result, rcc_ir_type_integer(32u), true);
    result->is_unsigned = expression->type->is_unsigned;
    result->valid = true;
    return true;
}

static bool lower_wide_scalar_expression(
    RccIrLowerContext* context, const Expr* expression,
    RccIrLowerWideValue* result) {
    RccIrLowerLocal* local;
    RccIrLowerValue address;
    RccIrLowerValue scalar;
    RccIrLowerValue shift;
    RccIrInstruction* high_operation;
    RccIrLowerWideValue left;
    RccIrLowerWideValue right;
    if (result) memset(result, 0, sizeof(*result));
    if (!context || !expression || !result || context->unsupported ||
        !expression->type || !type_is_integer(expression->type) ||
        expression->type->size <= 0) return false;
    if (lower_wide_scalar_constant(context, expression, result)) return true;
    if (expression->kind == EXPR_IDENT && expression->ident_decl) {
        local = lower_find_local(context, expression->ident_decl);
        if (local && local->wide_ssa) {
            *result = local->wide_value;
            result->is_unsigned = expression->type->is_unsigned;
            return result->valid;
        }
    }
    if (expression->kind == EXPR_IDENT || expression->kind == EXPR_DEREF ||
        expression->kind == EXPR_INDEX || expression->kind == EXPR_MEMBER ||
        expression->kind == EXPR_PTR_MEMBER) {
        if (!lower_i686_wide_scalar_type(expression->type)) return false;
        address = lower_lvalue_address(context, expression);
        return lower_wide_scalar_load(
            context, address, expression->type->is_unsigned, result);
    }
    if (expression->kind == EXPR_CAST) {
        if (!expression->cast_expr || !expression->cast_expr->type) {
            return false;
        }
        if (lower_i686_wide_scalar_type(expression->cast_expr->type)) {
            return lower_wide_scalar_expression(
                context, expression->cast_expr, result);
        }
        if (expression->cast_expr->type &&
            type_is_integer(expression->cast_expr->type) &&
            expression->cast_expr->type->size > 0 &&
            expression->cast_expr->type->size <= 4 &&
            lower_i686_wide_scalar_type(expression->type)) {
            scalar = lower_expression(context, expression->cast_expr);
            if (!scalar.valid) return false;
            scalar = lower_cast(
                context, scalar,
                scalar.is_unsigned ? type_uint : type_int);
            if (!scalar.valid) return false;
            result->low = scalar;
            if (scalar.is_unsigned) {
                result->high = lower_integer_constant(
                    context, rcc_ir_type_integer(32u), true, 0u);
            } else {
                shift = lower_integer_constant(
                    context, rcc_ir_type_integer(32u), true, 31u);
                if (!shift.valid) return false;
                {
                    RccIrValue operands[2] = {
                        scalar.value, shift.value
                    };
                    high_operation = lower_append(
                        context, RCC_IR_ASHR, rcc_ir_type_integer(32u),
                        operands, 2u, NULL, 0u);
                }
                if (!high_operation) return false;
                result->high = lower_value(
                    high_operation->result, rcc_ir_type_integer(32u), true);
            }
            result->is_unsigned = expression->type->is_unsigned;
            result->valid = result->low.valid && result->high.valid;
            return result->valid;
        }
        return false;
    }
    if (expression->kind == EXPR_COMMA &&
        lower_i686_wide_scalar_type(expression->type) &&
        expression->binary_lhs && expression->binary_rhs) {
        RccIrLowerWideValue discarded;
        /* Preserve the left operand's sequencing side effect when it is an
         * i686 wide scalar.  The scalar path cannot represent that pair, so
         * lower it before returning the right operand's pair value. */
        if (lower_i686_wide_scalar_type(expression->binary_lhs->type)) {
            if (!lower_wide_scalar_expression(
                    context, expression->binary_lhs, &discarded)) {
                return false;
            }
        } else {
            (void)lower_expression(context, expression->binary_lhs);
            if (context->unsupported) return false;
        }
        return lower_wide_scalar_expression(
            context, expression->binary_rhs, result);
    }
    if (expression->kind == EXPR_ASSIGN &&
        lower_i686_wide_scalar_type(expression->type) &&
        expression->binary_lhs && expression->binary_rhs &&
        lower_i686_wide_scalar_type(expression->binary_lhs->type)) {
        RccIrLowerWideValue value;
        address = lower_lvalue_address(context, expression->binary_lhs);
        if (!lower_wide_scalar_expression(
                context, expression->binary_rhs, &value) ||
            !lower_wide_scalar_store(context, address, value)) {
            return false;
        }
        *result = value;
        return true;
    }
    if ((expression->kind == EXPR_ADD_ASSIGN ||
         expression->kind == EXPR_SUB_ASSIGN ||
         expression->kind == EXPR_MUL_ASSIGN ||
         expression->kind == EXPR_DIV_ASSIGN ||
         expression->kind == EXPR_MOD_ASSIGN ||
         expression->kind == EXPR_AND_ASSIGN ||
         expression->kind == EXPR_OR_ASSIGN ||
         expression->kind == EXPR_XOR_ASSIGN ||
         expression->kind == EXPR_LSHIFT_ASSIGN ||
         expression->kind == EXPR_RSHIFT_ASSIGN) &&
        lower_i686_wide_scalar_type(expression->type) &&
        expression->binary_lhs && expression->binary_rhs &&
        lower_i686_wide_scalar_type(expression->binary_lhs->type)) {
        RccIrLowerWideValue left;
        RccIrLowerWideValue right;
        RccIrLowerWideValue value;
        ExprKind binary_kind = lower_compound_binary_kind(expression->kind);
        address = lower_lvalue_address(context, expression->binary_lhs);
        if (!lower_wide_scalar_load(
                context, address, expression->binary_lhs->type->is_unsigned,
                &left)) {
            return false;
        }
        if (binary_kind == EXPR_LSHIFT || binary_kind == EXPR_RSHIFT) {
            scalar = lower_expression(context, expression->binary_rhs);
            if (!scalar.valid || !lower_wide_scalar_shift(
                    context, binary_kind, left, scalar,
                    expression->type->is_unsigned, &value)) {
                return false;
            }
        } else if (!lower_wide_scalar_expression(
                       context, expression->binary_rhs, &right)) {
            return false;
        } else if (binary_kind == EXPR_MUL &&
                   !lower_wide_scalar_multiply(
                       context, left, right,
                       expression->type->is_unsigned, &value)) {
            return false;
        } else if ((binary_kind == EXPR_DIV ||
                    binary_kind == EXPR_MOD) &&
                   !lower_wide_scalar_divmod(
                       context, binary_kind, left, right,
                       expression->type->is_unsigned, &value)) {
            return false;
        } else if (!lower_wide_scalar_binary(
                       context, binary_kind, left, right,
                       expression->type->is_unsigned, &value)) {
            return false;
        }
        if (!lower_wide_scalar_store(context, address, value)) return false;
        *result = value;
        return true;
    }
    if ((expression->kind == EXPR_PREINC ||
         expression->kind == EXPR_PREDEC ||
         expression->kind == EXPR_POSTINC ||
         expression->kind == EXPR_POSTDEC) &&
        lower_i686_wide_scalar_type(expression->type) &&
        expression->unary_operand &&
        lower_i686_wide_scalar_type(expression->unary_operand->type)) {
        RccIrLowerWideValue old_value;
        RccIrLowerWideValue one;
        RccIrLowerWideValue new_value;
        bool increment = expression->kind == EXPR_PREINC ||
            expression->kind == EXPR_POSTINC;
        bool postfix = expression->kind == EXPR_POSTINC ||
            expression->kind == EXPR_POSTDEC;
        address = lower_lvalue_address(context, expression->unary_operand);
        if (!lower_wide_scalar_load(
                context, address,
                expression->unary_operand->type->is_unsigned,
                &old_value)) {
            return false;
        }
        one.low = lower_integer_constant(
            context, rcc_ir_type_integer(32u), true, 1u);
        one.high = lower_integer_constant(
            context, rcc_ir_type_integer(32u), true, 0u);
        one.is_unsigned = true;
        one.valid = one.low.valid && one.high.valid;
        if (!one.valid || !lower_wide_scalar_binary(
                context, increment ? EXPR_ADD : EXPR_SUB,
                old_value, one, expression->type->is_unsigned,
                &new_value) ||
            !lower_wide_scalar_store(context, address, new_value)) {
            return false;
        }
        *result = postfix ? old_value : new_value;
        return true;
    }
    if (expression->kind == EXPR_COND &&
        lower_i686_wide_scalar_type(expression->type)) {
        return lower_wide_scalar_conditional_expression(
            context, expression, result);
    }
    if (expression->kind == EXPR_CALL &&
        lower_i686_wide_scalar_type(expression->type) &&
        expression->call_func &&
        expression->call_func->kind == EXPR_IDENT &&
        expression->call_func->ident_name &&
        strcmp(expression->call_func->ident_name,
               "__builtin_bswap64") == 0) {
        const ExprList* argument = expression->call_args;
        RccIrLowerWideValue source;
        if (!argument || !argument->expr || argument->next ||
            !lower_i686_wide_scalar_type(argument->expr->type) ||
            !lower_wide_scalar_expression(
                context, argument->expr, &source)) {
            return false;
        }
        result->low = lower_wide_scalar_bswap_word(
            context, source.high);
        result->high = lower_wide_scalar_bswap_word(
            context, source.low);
        result->is_unsigned = expression->type->is_unsigned;
        result->valid = result->low.valid && result->high.valid;
        return result->valid;
    }
    if (expression->kind == EXPR_CALL &&
        lower_i686_wide_scalar_type(expression->type) &&
        expression->call_func &&
        expression->call_func->kind == EXPR_IDENT &&
        expression->call_func->ident_name &&
        (strcmp(expression->call_func->ident_name,
                "__builtin_expect") == 0 ||
         strcmp(expression->call_func->ident_name,
                "__builtin_expect_with_probability") == 0)) {
        const ExprList* value_argument = expression->call_args;
        const ExprList* expected_argument = value_argument ?
            value_argument->next : NULL;
        const ExprList* probability_argument = expected_argument ?
            expected_argument->next : NULL;
        RccIrLowerWideValue value;
        RccIrLowerWideValue expected_wide;
        RccIrLowerValue expected_scalar;
        if (!value_argument || !value_argument->expr ||
            !expected_argument || !expected_argument->expr ||
            ((strcmp(expression->call_func->ident_name,
                     "__builtin_expect") == 0 && probability_argument) ||
             (strcmp(expression->call_func->ident_name,
                     "__builtin_expect_with_probability") == 0 &&
              (!probability_argument || probability_argument->next)))) {
            return false;
        }
        /* __builtin_expect evaluates its prediction operand before the
         * value operand.  The prediction does not affect the returned pair,
         * but it must still be lowered so side effects and diagnostics are
         * preserved on the i686 wide-scalar path. */
        if (lower_i686_wide_scalar_type(expected_argument->expr->type)) {
            if (!lower_wide_scalar_expression(
                    context, expected_argument->expr, &expected_wide)) {
                return false;
            }
        } else {
            expected_scalar = lower_expression(
                context, expected_argument->expr);
            if (!expected_scalar.valid ||
                expected_scalar.type.kind != RCC_IR_TYPE_INTEGER) {
                return false;
            }
        }
        if (!lower_wide_scalar_expression(
                context, value_argument->expr, &value)) {
            return false;
        }
        *result = value;
        return true;
    }
    if (expression->kind == EXPR_CALL &&
        lower_i686_wide_scalar_type(expression->type)) {
        return lower_wide_scalar_call(context, expression, result);
    }
    if (expression->kind == EXPR_MUL &&
        lower_i686_wide_scalar_type(expression->type) &&
        lower_wide_scalar_expression(
            context, expression->binary_lhs, &left) &&
        lower_wide_scalar_expression(
            context, expression->binary_rhs, &right)) {
        return lower_wide_scalar_multiply(
            context, left, right, expression->type->is_unsigned, result);
    }
    if ((expression->kind == EXPR_DIV || expression->kind == EXPR_MOD) &&
        lower_i686_wide_scalar_type(expression->type) &&
        lower_wide_scalar_expression(
            context, expression->binary_lhs, &left) &&
        lower_wide_scalar_expression(
            context, expression->binary_rhs, &right)) {
        return lower_wide_scalar_divmod(
            context, expression->kind, left, right,
            expression->type->is_unsigned, result);
    }
    if ((expression->kind == EXPR_ADD || expression->kind == EXPR_SUB ||
         expression->kind == EXPR_BITAND || expression->kind == EXPR_BITOR ||
         expression->kind == EXPR_BITXOR) &&
        lower_i686_wide_scalar_type(expression->type) &&
        lower_wide_scalar_expression(
            context, expression->binary_lhs, &left) &&
        lower_wide_scalar_expression(
            context, expression->binary_rhs, &right)) {
        return lower_wide_scalar_binary(
            context, expression->kind, left, right,
            expression->type->is_unsigned, result);
    }
    if ((expression->kind == EXPR_LSHIFT ||
         expression->kind == EXPR_RSHIFT) &&
        lower_i686_wide_scalar_type(expression->type) &&
        lower_wide_scalar_expression(
            context, expression->binary_lhs, &left) &&
        (shift = lower_expression(context, expression->binary_rhs)).valid) {
        return lower_wide_scalar_shift(
            context, expression->kind, left, shift,
            expression->type->is_unsigned, result);
    }
    if (expression->kind == EXPR_NEG &&
        lower_i686_wide_scalar_type(expression->type) &&
        lower_wide_scalar_expression(
            context, expression->unary_operand, &right)) {
        left.low = lower_integer_constant(
            context, rcc_ir_type_integer(32u), true, 0u);
        left.high = lower_integer_constant(
            context, rcc_ir_type_integer(32u), true, 0u);
        left.is_unsigned = true;
        left.valid = left.low.valid && left.high.valid;
        return lower_wide_scalar_binary(
            context, EXPR_SUB, left, right,
            expression->type->is_unsigned, result);
    }
    if (expression->kind == EXPR_BITNOT &&
        lower_i686_wide_scalar_type(expression->type) &&
        lower_wide_scalar_expression(
            context, expression->unary_operand, &right)) {
        left.low = lower_integer_constant(
            context, rcc_ir_type_integer(32u), true, UINT32_MAX);
        left.high = lower_integer_constant(
            context, rcc_ir_type_integer(32u), true, UINT32_MAX);
        left.is_unsigned = true;
        left.valid = left.low.valid && left.high.valid;
        return lower_wide_scalar_binary(
            context, EXPR_BITXOR, left, right,
            expression->type->is_unsigned, result);
    }
    return false;
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
    bool typeinfo_equality =
        (expression->kind == EXPR_EQ || expression->kind == EXPR_NE) &&
        expression->binary_lhs && expression->binary_rhs &&
        expression->binary_lhs->type == rcc_cxx_type_info_type() &&
        expression->binary_rhs->type == rcc_cxx_type_info_type();
    RccIrLowerValue left;
    RccIrLowerValue right;
    RccIrValue operands[2];
    RccIrInstruction* compare;
    RccIrLowerValue result;
    if (typeinfo_equality) {
        left = lower_expression(context, expression->binary_lhs);
        right = lower_expression(context, expression->binary_rhs);
        if (!left.valid || !right.valid ||
            left.type.kind != RCC_IR_TYPE_POINTER ||
            right.type.kind != RCC_IR_TYPE_POINTER) {
            context->unsupported = true;
            return lower_invalid_value();
        }
        operands[0] = left.value;
        operands[1] = right.value;
        compare = lower_append(
            context, RCC_IR_ICMP, rcc_ir_type_integer(1u),
            operands, 2u, NULL, 0u);
        if (!compare) return lower_invalid_value();
        rcc_ir_set_predicate(compare, expression->kind == EXPR_EQ
            ? RCC_IR_ICMP_EQ : RCC_IR_ICMP_NE);
        result = lower_value(compare->result, rcc_ir_type_integer(1u), true);
        return lower_cast(context, result, expression->type);
    }
    if (lower_i686_wide_scalar_type(comparison_type)) {
        RccIrLowerWideValue wide_left;
        RccIrLowerWideValue wide_right;
        RccIrLowerValue wide_result;
        if (!lower_wide_scalar_expression(
                context, expression->binary_lhs, &wide_left) ||
            !lower_wide_scalar_expression(
                context, expression->binary_rhs, &wide_right) ||
            !lower_wide_scalar_compare(
                context, expression->kind, wide_left, wide_right,
                comparison_type->is_unsigned, &wide_result) ||
            !wide_result.valid) {
            context->unsupported = true;
            return lower_invalid_value();
        }
        return lower_cast(context, wide_result, expression->type);
    }
    left = lower_expression(context, expression->binary_lhs);
    right = lower_expression(context, expression->binary_rhs);
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

static RccIrLowerValue lower_spaceship(RccIrLowerContext* context,
                                       const Expr* expression) {
    Type* comparison_type = type_common(expression->binary_lhs->type,
                                        expression->binary_rhs->type);
    RccIrType result_type;
    RccIrLowerValue left;
    RccIrLowerValue right;
    RccIrLowerValue less;
    RccIrLowerValue greater;
    RccIrLowerValue negative;
    RccIrLowerValue zero;
    RccIrLowerValue positive;
    RccIrInstruction* less_compare;
    RccIrInstruction* greater_compare;
    RccIrInstruction* greater_select;
    RccIrInstruction* less_select;
    RccIrValue operands[3];
    if (!comparison_type || !lower_type(expression->type, &result_type) ||
        result_type.kind != RCC_IR_TYPE_INTEGER) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    left = lower_expression(context, expression->binary_lhs);
    right = lower_expression(context, expression->binary_rhs);
    if (!left.valid || !right.valid) return lower_invalid_value();
    left = lower_cast(context, left, comparison_type);
    right = lower_cast(context, right, comparison_type);
    if (!left.valid || !right.valid) return lower_invalid_value();
    operands[0] = left.value;
    operands[1] = right.value;
    less_compare = lower_append(context, RCC_IR_ICMP,
                                rcc_ir_type_integer(1u), operands, 2u,
                                NULL, 0u);
    greater_compare = lower_append(context, RCC_IR_ICMP,
                                   rcc_ir_type_integer(1u), operands, 2u,
                                   NULL, 0u);
    if (!less_compare || !greater_compare) return lower_invalid_value();
    rcc_ir_set_predicate(less_compare, lower_comparison_predicate(
        EXPR_LT, comparison_type->is_unsigned ||
                 comparison_type->kind == TYPE_PTR));
    rcc_ir_set_predicate(greater_compare, lower_comparison_predicate(
        EXPR_GT, comparison_type->is_unsigned ||
                 comparison_type->kind == TYPE_PTR));
    less = lower_value(less_compare->result, rcc_ir_type_integer(1u), true);
    greater = lower_value(greater_compare->result,
                          rcc_ir_type_integer(1u), true);
    negative = lower_integer_constant(context, result_type, false,
                                      UINT64_MAX);
    zero = lower_integer_constant(context, result_type, false, 0u);
    positive = lower_integer_constant(context, result_type, false, 1u);
    if (!less.valid || !greater.valid || !negative.valid ||
        !zero.valid || !positive.valid) {
        return lower_invalid_value();
    }
    operands[0] = greater.value;
    operands[1] = positive.value;
    operands[2] = zero.value;
    greater_select = lower_append(context, RCC_IR_SELECT, result_type,
                                  operands, 3u, NULL, 0u);
    if (!greater_select) return lower_invalid_value();
    operands[0] = less.value;
    operands[1] = negative.value;
    operands[2] = greater_select->result;
    less_select = lower_append(context, RCC_IR_SELECT, result_type,
                               operands, 3u, NULL, 0u);
    if (!less_select) return lower_invalid_value();
    return lower_value(less_select->result, result_type,
                       expression->type->is_unsigned);
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
        RccIrLowerValue value;
        if (parameter && parameter->type &&
            parameter->type->kind == TYPE_PTR &&
            parameter->type->is_reference) {
            /* A reference parameter is an address in the target ABI.  Keep
             * the argument as an lvalue address so a reference local is not
             * accidentally loaded as its pointee value. */
            value = lower_lvalue_address(context, argument->expr);
        } else {
            value = lower_expression(context, argument->expr);
        }
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

static RccIrLowerValue lower_builtin_integer_binary(
    RccIrLowerContext* context, RccIrOpcode opcode,
    RccIrLowerValue left, RccIrLowerValue right) {
    RccIrValue operands[2];
    RccIrInstruction* instruction;
    if (!context || !left.valid || !right.valid ||
        !rcc_ir_type_equal(left.type, right.type) ||
        left.type.kind != RCC_IR_TYPE_INTEGER) {
        if (context) context->unsupported = true;
        return lower_invalid_value();
    }
    operands[0] = left.value;
    operands[1] = right.value;
    instruction = lower_append(context, opcode, left.type,
                               operands, 2u, NULL, 0u);
    if (!instruction) return lower_invalid_value();
    return lower_value(instruction->result, left.type, left.is_unsigned);
}

static RccIrLowerValue lower_builtin_integer_compare(
    RccIrLowerContext* context, RccIrLowerValue left,
    RccIrLowerValue right, RccIrIntPredicate predicate) {
    RccIrValue operands[2];
    RccIrInstruction* instruction;
    if (!context || !left.valid || !right.valid ||
        !rcc_ir_type_equal(left.type, right.type) ||
        left.type.kind != RCC_IR_TYPE_INTEGER) {
        if (context) context->unsupported = true;
        return lower_invalid_value();
    }
    operands[0] = left.value;
    operands[1] = right.value;
    instruction = lower_append(
        context, RCC_IR_ICMP, rcc_ir_type_integer(1u), operands, 2u,
        NULL, 0u);
    if (!instruction) return lower_invalid_value();
    rcc_ir_set_predicate(instruction, predicate);
    return lower_value(instruction->result,
                       rcc_ir_type_integer(1u), true);
}

static RccIrLowerValue lower_builtin_integer_select(
    RccIrLowerContext* context, RccIrLowerValue condition,
    RccIrLowerValue when_true, RccIrLowerValue when_false) {
    RccIrValue operands[3];
    RccIrInstruction* instruction;
    if (!context || !condition.valid || !when_true.valid ||
        !when_false.valid ||
        !rcc_ir_type_equal(condition.type, rcc_ir_type_integer(1u)) ||
        !rcc_ir_type_equal(when_true.type, when_false.type)) {
        if (context) context->unsupported = true;
        return lower_invalid_value();
    }
    operands[0] = condition.value;
    operands[1] = when_true.value;
    operands[2] = when_false.value;
    instruction = lower_append(
        context, RCC_IR_SELECT, when_true.type, operands, 3u, NULL, 0u);
    if (!instruction) return lower_invalid_value();
    return lower_value(instruction->result, when_true.type,
                       when_true.is_unsigned);
}

static RccIrLowerValue lower_builtin_checked_add_sub(
    RccIrLowerContext* context, const Expr* expression, bool subtract) {
    const ExprList* first = expression ? expression->call_args : NULL;
    const ExprList* second = first ? first->next : NULL;
    const ExprList* third = second ? second->next : NULL;
    const Type* operand_ast_type;
    const Type* result_ast_type;
    RccIrType operand_type;
    RccIrType result_type;
    RccIrLowerValue left;
    RccIrLowerValue right;
    RccIrLowerValue destination;
    RccIrLowerValue result;
    RccIrLowerValue overflow;
    if (!context || !expression || !expression->type ||
        !first || !first->expr || !second || !second->expr ||
        !third || !third->expr || third->next ||
        !type_is_integer(first->expr->type) ||
        !type_is_integer(second->expr->type) ||
        third->expr->type->kind != TYPE_PTR ||
        !third->expr->type->base ||
        !type_is_integer(third->expr->type->base) ||
        !lower_type(first->expr->type, &operand_type) ||
        !lower_type(third->expr->type->base, &result_type) ||
        operand_type.kind != RCC_IR_TYPE_INTEGER ||
        !rcc_ir_type_equal(operand_type, result_type) ||
        operand_type.bit_width < 8u || operand_type.bit_width > 64u ||
        (operand_type.bit_width == 64u && g_opts.target_arch != ARCH_X64)) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    operand_ast_type = first->expr->type;
    result_ast_type = third->expr->type->base;
    if (operand_ast_type->is_unsigned != result_ast_type->is_unsigned ||
        second->expr->type->is_unsigned != result_ast_type->is_unsigned ||
        second->expr->type->size != result_ast_type->size) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    left = lower_cast(context, lower_expression(context, first->expr),
                      operand_ast_type);
    right = lower_cast(context, lower_expression(context, second->expr),
                       operand_ast_type);
    destination = lower_expression(context, third->expr);
    if (!left.valid || !right.valid || !destination.valid ||
        destination.type.kind != RCC_IR_TYPE_POINTER) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    result = lower_builtin_integer_binary(
        context, subtract ? RCC_IR_SUB : RCC_IR_ADD, left, right);
    if (!result.valid) return lower_invalid_value();
    if (operand_ast_type->is_unsigned) {
        overflow = lower_builtin_integer_compare(
            context, subtract ? left : result,
            subtract ? right : left, RCC_IR_ICMP_ULT);
    } else {
        RccIrLowerValue left_xor;
        RccIrLowerValue right_xor;
        RccIrLowerValue sign_bits;
        RccIrLowerValue sign_shift = lower_integer_constant(
            context, operand_type, true,
            (uint64_t)operand_type.bit_width - 1u);
        RccIrLowerValue zero = lower_integer_constant(
            context, operand_type, true, 0u);
        left_xor = lower_builtin_integer_binary(
            context, RCC_IR_XOR, left, subtract ? right : result);
        right_xor = lower_builtin_integer_binary(
            context, RCC_IR_XOR,
            subtract ? left : right, result);
        sign_bits = lower_builtin_integer_binary(
            context, RCC_IR_AND, left_xor, right_xor);
        sign_bits = lower_builtin_integer_binary(
            context, RCC_IR_LSHR, sign_bits, sign_shift);
        overflow = lower_builtin_integer_compare(
            context, sign_bits, zero, RCC_IR_ICMP_NE);
    }
    if (!overflow.valid || !lower_store_address(context, destination, result)) {
        return lower_invalid_value();
    }
    return lower_cast(context, overflow, expression->type);
}

static RccIrLowerValue lower_builtin_checked_mul(
    RccIrLowerContext* context, const Expr* expression) {
    const ExprList* first = expression ? expression->call_args : NULL;
    const ExprList* second = first ? first->next : NULL;
    const ExprList* third = second ? second->next : NULL;
    const Type* operand_ast_type;
    const Type* result_ast_type;
    RccIrType operand_type;
    RccIrType result_type;
    RccIrLowerValue left;
    RccIrLowerValue right;
    RccIrLowerValue destination;
    RccIrLowerValue result;
    RccIrLowerValue overflow;
    RccIrLowerValue zero;
    RccIrLowerValue one;
    RccIrLowerValue right_nonzero;
    RccIrLowerValue safe_right;
    RccIrLowerValue limit;
    RccIrLowerValue quotient;
    RccIrLowerValue too_large;
    if (!context || !expression || !expression->type ||
        !first || !first->expr || !second || !second->expr ||
        !third || !third->expr || third->next ||
        !type_is_integer(first->expr->type) ||
        !type_is_integer(second->expr->type) ||
        third->expr->type->kind != TYPE_PTR ||
        !third->expr->type->base ||
        !type_is_integer(third->expr->type->base) ||
        !lower_type(first->expr->type, &operand_type) ||
        !lower_type(third->expr->type->base, &result_type) ||
        operand_type.kind != RCC_IR_TYPE_INTEGER ||
        !rcc_ir_type_equal(operand_type, result_type) ||
        operand_type.bit_width < 32u || operand_type.bit_width > 64u ||
        (operand_type.bit_width == 64u && g_opts.target_arch != ARCH_X64)) {
        if (context) context->unsupported = true;
        return lower_invalid_value();
    }
    operand_ast_type = first->expr->type;
    result_ast_type = third->expr->type->base;
    if (operand_ast_type->is_unsigned != result_ast_type->is_unsigned ||
        second->expr->type->is_unsigned != result_ast_type->is_unsigned ||
        second->expr->type->size != result_ast_type->size) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    left = lower_cast(context, lower_expression(context, first->expr),
                      operand_ast_type);
    right = lower_cast(context, lower_expression(context, second->expr),
                       operand_ast_type);
    destination = lower_expression(context, third->expr);
    if (!left.valid || !right.valid || !destination.valid ||
        destination.type.kind != RCC_IR_TYPE_POINTER) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    result = lower_builtin_integer_binary(context, RCC_IR_MUL, left, right);
    if (!result.valid) return lower_invalid_value();
    zero = lower_integer_constant(context, operand_type, true, 0u);
    one = lower_integer_constant(context, operand_type, true, 1u);
    if (!zero.valid || !one.valid) return lower_invalid_value();
    if (operand_ast_type->is_unsigned) {
        uint64_t maximum = operand_type.bit_width == 64u
            ? UINT64_MAX : UINT64_C(0xffffffff);
        limit = lower_integer_constant(
            context, operand_type, true, maximum);
        right_nonzero = lower_builtin_integer_compare(
            context, right, zero, RCC_IR_ICMP_NE);
        safe_right = lower_builtin_integer_select(
            context, right_nonzero, right, one);
    } else {
        RccIrLowerValue left_negative;
        RccIrLowerValue right_negative;
        RccIrLowerValue negative_product;
        RccIrLowerValue negative_left;
        RccIrLowerValue negative_right;
        RccIrLowerValue absolute_left;
        RccIrLowerValue absolute_right;
        RccIrLowerValue minimum_magnitude;
        RccIrLowerValue maximum_positive;
        uint64_t minimum = UINT64_C(1) << (operand_type.bit_width - 1u);
        left_negative = lower_builtin_integer_compare(
            context, left, zero, RCC_IR_ICMP_SLT);
        right_negative = lower_builtin_integer_compare(
            context, right, zero, RCC_IR_ICMP_SLT);
        negative_product = lower_builtin_integer_binary(
            context, RCC_IR_XOR, left_negative, right_negative);
        negative_left = lower_builtin_integer_binary(
            context, RCC_IR_SUB, zero, left);
        negative_right = lower_builtin_integer_binary(
            context, RCC_IR_SUB, zero, right);
        absolute_left = lower_builtin_integer_select(
            context, left_negative, negative_left, left);
        absolute_right = lower_builtin_integer_select(
            context, right_negative, negative_right, right);
        minimum_magnitude = lower_integer_constant(
            context, operand_type, true, minimum);
        maximum_positive = lower_integer_constant(
            context, operand_type, true, minimum - 1u);
        limit = lower_builtin_integer_select(
            context, negative_product, minimum_magnitude, maximum_positive);
        right_nonzero = lower_builtin_integer_compare(
            context, absolute_right, zero, RCC_IR_ICMP_NE);
        safe_right = lower_builtin_integer_select(
            context, right_nonzero, absolute_right, one);
        left = absolute_left;
    }
    if (!limit.valid || !right_nonzero.valid || !safe_right.valid) {
        return lower_invalid_value();
    }
    quotient = lower_builtin_integer_binary(
        context, RCC_IR_UDIV, limit, safe_right);
    too_large = lower_builtin_integer_compare(
        context, left, quotient, RCC_IR_ICMP_UGT);
    overflow = lower_builtin_integer_binary(
        context, RCC_IR_AND, right_nonzero, too_large);
    if (!overflow.valid ||
        !lower_store_address(context, destination, result)) {
        return lower_invalid_value();
    }
    return lower_cast(context, overflow, expression->type);
}

static bool lower_builtin_object_size_known(
    const Expr* expression, uint64_t* size) {
    const Decl* declaration;
    const Type* type;
    int64_t index;
    uint32_t element_size;
    uint64_t offset;
    while (expression && expression->kind == EXPR_CAST) {
        expression = expression->cast_expr;
    }
    if (!expression || !size) return false;
    if (expression->kind == EXPR_STRING_LIT) {
        if (expression->str_length == SIZE_MAX) return false;
        *size = (uint64_t)expression->str_length + 1u;
        return true;
    }
    if (expression->kind == EXPR_ADDR && expression->unary_operand) {
        return lower_builtin_object_size_known(
            expression->unary_operand, size);
    }
    if (expression->kind == EXPR_INDEX && expression->index_base &&
        expression->index_expr &&
        lower_builtin_object_size_known(expression->index_base, size) &&
        expr_eval_integer_constant(expression->index_expr, &index) &&
        index >= 0 && expression->index_base->type &&
        (expression->index_base->type->kind == TYPE_PTR ||
         expression->index_base->type->kind == TYPE_ARRAY) &&
        expression->index_base->type->base &&
        expression->index_base->type->base->size > 0) {
        element_size = (uint32_t)expression->index_base->type->base->size;
        if ((uint64_t)index > UINT64_MAX / element_size) return false;
        offset = (uint64_t)index * element_size;
        if (offset > *size) return false;
        *size -= offset;
        return true;
    }
    if (expression->kind != EXPR_IDENT || !expression->ident_decl) {
        return false;
    }
    declaration = expression->ident_decl;
    if (declaration->kind != DECL_VAR || !declaration->type) return false;
    type = declaration->type;
    if (type->kind == TYPE_ARRAY && type->array_len >= 0 &&
        !type->array_bound && type->size > 0) {
        *size = (uint64_t)type->size;
        return true;
    }
    if (type->kind != TYPE_PTR && type->kind != TYPE_ARRAY &&
        type->kind != TYPE_FUNC && type->size > 0) {
        *size = (uint64_t)type->size;
        return true;
    }
    return false;
}

static RccIrLowerValue lower_builtin_bit_count(
    RccIrLowerContext* context, const Expr* expression, const char* name) {
    const ExprList* argument;
    const Type* argument_type;
    RccIrType source_type;
    RccIrType result_type;
    RccIrLowerValue source;
    RccIrLowerValue result;
    bool leading = false;
    bool population = false;
    bool parity = false;
    bool first_set = false;
    bool clrsb = false;
    unsigned width = 0u;
    unsigned index;

    if (!context || !expression || !name ||
        !expression->type || !lower_type(expression->type, &result_type) ||
        result_type.kind != RCC_IR_TYPE_INTEGER ||
        result_type.bit_width != 32u) {
        if (context) context->unsupported = true;
        return lower_invalid_value();
    }
    if (strcmp(name, "__builtin_clz") == 0) {
        leading = true;
        width = 32u;
    } else if (strcmp(name, "__builtin_clzl") == 0) {
        leading = true;
        width = g_opts.target_arch == ARCH_X64 ? 64u : 32u;
    } else if (strcmp(name, "__builtin_clzll") == 0) {
        leading = true;
        width = 64u;
    } else if (strcmp(name, "__builtin_ctz") == 0) {
        width = 32u;
    } else if (strcmp(name, "__builtin_ctzl") == 0) {
        width = g_opts.target_arch == ARCH_X64 ? 64u : 32u;
    } else if (strcmp(name, "__builtin_ctzll") == 0) {
        width = 64u;
    } else if (strcmp(name, "__builtin_popcount") == 0) {
        population = true;
        width = 32u;
    } else if (strcmp(name, "__builtin_popcountl") == 0) {
        population = true;
        width = g_opts.target_arch == ARCH_X64 ? 64u : 32u;
    } else if (strcmp(name, "__builtin_popcountll") == 0) {
        population = true;
        width = 64u;
    } else if (strcmp(name, "__builtin_parity") == 0) {
        population = true;
        parity = true;
        width = 32u;
    } else if (strcmp(name, "__builtin_parityl") == 0) {
        population = true;
        parity = true;
        width = g_opts.target_arch == ARCH_X64 ? 64u : 32u;
    } else if (strcmp(name, "__builtin_parityll") == 0) {
        population = true;
        parity = true;
        width = 64u;
    } else if (strcmp(name, "__builtin_ffs") == 0) {
        first_set = true;
        width = 32u;
    } else if (strcmp(name, "__builtin_ffsl") == 0) {
        first_set = true;
        width = g_opts.target_arch == ARCH_X64 ? 64u : 32u;
    } else if (strcmp(name, "__builtin_ffsll") == 0) {
        first_set = true;
        width = 64u;
    } else if (strcmp(name, "__builtin_clrsb") == 0) {
        leading = true;
        clrsb = true;
        width = 32u;
    } else if (strcmp(name, "__builtin_clrsbl") == 0) {
        leading = true;
        clrsb = true;
        width = g_opts.target_arch == ARCH_X64 ? 64u : 32u;
    } else if (strcmp(name, "__builtin_clrsbll") == 0) {
        leading = true;
        clrsb = true;
        width = 64u;
    } else {
        context->unsupported = true;
        return lower_invalid_value();
    }
    argument = expression->call_args;
    if (!argument || !argument->expr || argument->next ||
        !argument->expr->type || !type_is_integer(argument->expr->type) ||
        !lower_type(argument->expr->type, &source_type) ||
        source_type.kind != RCC_IR_TYPE_INTEGER ||
        source_type.bit_width != width ||
        (width == 64u && g_opts.target_arch != ARCH_X64)) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    argument_type = argument->expr->type;
    source = lower_expression(context, argument->expr);
    source = lower_cast(context, source, argument_type);
    if (!source.valid || !rcc_ir_type_equal(source.type, source_type)) {
        context->unsupported = true;
        return lower_invalid_value();
    }
    if (clrsb) {
        RccIrLowerValue sign_shift = lower_integer_constant(
            context, source_type, true, (uint64_t)width - 1u);
        RccIrLowerValue sign;
        RccIrLowerValue normalized;
        if (!sign_shift.valid) return lower_invalid_value();
        sign = lower_builtin_integer_binary(
            context, RCC_IR_ASHR, source, sign_shift);
        normalized = lower_builtin_integer_binary(
            context, RCC_IR_XOR, source, sign);
        if (!sign.valid || !normalized.valid) return lower_invalid_value();
        source = normalized;
    }
    if (population) {
        static const uint64_t masks[] = {
            UINT64_C(0x5555555555555555),
            UINT64_C(0x3333333333333333),
            UINT64_C(0x0f0f0f0f0f0f0f0f),
        };
        static const unsigned shifts[] = {1u, 2u, 4u};
        for (index = 0u; index < 3u; ++index) {
            RccIrLowerValue shifted;
            RccIrLowerValue shift = lower_integer_constant(
                context, source_type, true, shifts[index]);
            RccIrLowerValue mask = lower_integer_constant(
                context, source_type, true,
                width == 32u ? (masks[index] & UINT64_C(0xffffffff))
                             : masks[index]);
            if (!shift.valid || !mask.valid) return lower_invalid_value();
            shifted = lower_builtin_integer_binary(
                context, RCC_IR_LSHR, source, shift);
            shifted = lower_builtin_integer_binary(
                context, RCC_IR_AND, shifted, mask);
            if (!shifted.valid) return lower_invalid_value();
            if (index == 0u) {
                source = lower_builtin_integer_binary(
                    context, RCC_IR_SUB, source, shifted);
            } else {
                RccIrLowerValue masked = lower_builtin_integer_binary(
                    context, RCC_IR_AND, source, mask);
                if (!masked.valid) return lower_invalid_value();
                source = lower_builtin_integer_binary(
                    context, RCC_IR_ADD, masked, shifted);
            }
            if (!source.valid) return lower_invalid_value();
        }
        {
            static const unsigned extra_shifts[] = {8u, 16u, 32u};
            unsigned extra_count = width == 64u ? 3u : 2u;
            for (index = 0u; index < extra_count; ++index) {
                RccIrLowerValue shift = lower_integer_constant(
                    context, source_type, true, extra_shifts[index]);
                RccIrLowerValue shifted;
                if (!shift.valid) return lower_invalid_value();
                shifted = lower_builtin_integer_binary(
                    context, RCC_IR_LSHR, source, shift);
                source = lower_builtin_integer_binary(
                    context, RCC_IR_ADD, source, shifted);
                if (!source.valid) return lower_invalid_value();
            }
        }
        {
            RccIrLowerValue mask = lower_integer_constant(
                context, source_type, true,
                parity ? UINT64_C(1) :
                (width == 64u ? UINT64_C(0x7f) : UINT64_C(0x3f)));
            if (!mask.valid) return lower_invalid_value();
            source = lower_builtin_integer_binary(
                context, RCC_IR_AND, source, mask);
        }
        return lower_cast(context, source, expression->type);
    }
    result = lower_integer_constant(
        context, result_type, true, first_set ? 0u : (uint64_t)width);
    if (!result.valid) return lower_invalid_value();
    for (index = 0u; index < width; ++index) {
        unsigned bit_index = leading ? width - 1u - index : index;
        RccIrLowerValue shift = lower_integer_constant(
            context, source_type, true, bit_index);
        RccIrLowerValue one = lower_integer_constant(
            context, source_type, true, 1u);
        RccIrLowerValue zero = lower_integer_constant(
            context, source_type, true, 0u);
        RccIrLowerValue shifted;
        RccIrLowerValue bit;
        RccIrLowerValue set;
        RccIrLowerValue limit = lower_integer_constant(
            context, result_type, true, first_set ? 0u : (uint64_t)width);
        RccIrLowerValue unused;
        RccIrValue operands[3];
        RccIrInstruction* instruction;
        RccIrLowerValue candidate = lower_integer_constant(
            context, result_type, true,
            first_set ? (uint64_t)index + 1u : (uint64_t)index);
        if (!shift.valid || !one.valid || !zero.valid || !limit.valid ||
            !candidate.valid) return lower_invalid_value();
        shifted = lower_builtin_integer_binary(
            context, RCC_IR_LSHR, source, shift);
        bit = lower_builtin_integer_binary(
            context, RCC_IR_AND, shifted, one);
        if (!shifted.valid || !bit.valid) return lower_invalid_value();
        operands[0] = bit.value;
        operands[1] = zero.value;
        instruction = lower_append(context, RCC_IR_ICMP,
                                   rcc_ir_type_integer(1u), operands, 2u,
                                   NULL, 0u);
        if (!instruction) return lower_invalid_value();
        rcc_ir_set_predicate(instruction, RCC_IR_ICMP_NE);
        set = lower_value(instruction->result,
                          rcc_ir_type_integer(1u), true);
        operands[0] = result.value;
        operands[1] = limit.value;
        instruction = lower_append(context, RCC_IR_ICMP,
                                   rcc_ir_type_integer(1u), operands, 2u,
                                   NULL, 0u);
        if (!instruction) return lower_invalid_value();
        rcc_ir_set_predicate(instruction, RCC_IR_ICMP_EQ);
        unused = lower_value(instruction->result,
                             rcc_ir_type_integer(1u), true);
        operands[0] = unused.value;
        operands[1] = set.value;
        instruction = lower_append(context, RCC_IR_AND,
                                   rcc_ir_type_integer(1u), operands, 2u,
                                   NULL, 0u);
        if (!instruction) return lower_invalid_value();
        operands[0] = instruction->result;
        operands[1] = candidate.value;
        operands[2] = result.value;
        instruction = lower_append(context, RCC_IR_SELECT, result_type,
                                   operands, 3u, NULL, 0u);
        if (!instruction) return lower_invalid_value();
        result = lower_value(instruction->result, result_type, true);
    }
    if (clrsb) {
        RccIrLowerValue one = lower_integer_constant(
            context, result_type, true, 1u);
        if (!one.valid) return lower_invalid_value();
        result = lower_builtin_integer_binary(
            context, RCC_IR_SUB, result, one);
        if (!result.valid) return lower_invalid_value();
    }
    return result;
}

static RccIrLowerValue lower_builtin_call(
    RccIrLowerContext* context, const Expr* expression) {
    const ExprList* first;
    const ExprList* second;
    RccIrLowerValue value;
    RccIrLowerValue expected;
    RccIrLowerValue result;
    const char* name;
    if (!context || !expression || !expression->call_func ||
        expression->call_func->kind != EXPR_IDENT ||
        !expression->call_func->ident_name) {
        if (context) context->unsupported = true;
        return lower_invalid_value();
    }
    name = expression->call_func->ident_name;
    if (strcmp(name, "__builtin_add_overflow") == 0 ||
        strcmp(name, "__builtin_sub_overflow") == 0) {
        return lower_builtin_checked_add_sub(
            context, expression, strcmp(name, "__builtin_sub_overflow") == 0);
    }
    if (strcmp(name, "__builtin_mul_overflow") == 0) {
        return lower_builtin_checked_mul(context, expression);
    }
    if (strcmp(name, "__builtin_object_size") == 0) {
        const ExprList* object = expression->call_args;
        const ExprList* mode = object ? object->next : NULL;
        RccIrType result_type;
        int64_t mode_value;
        uint64_t object_size;
        bool known;
        if (!object || !object->expr || !mode || !mode->expr ||
            mode->next || !expr_eval_integer_constant(
                mode->expr, &mode_value) || mode_value < 0 || mode_value > 3 ||
            !expression->type || !lower_type(expression->type, &result_type) ||
            result_type.kind != RCC_IR_TYPE_INTEGER) {
            context->unsupported = true;
            return lower_invalid_value();
        }
        known = lower_builtin_object_size_known(object->expr, &object_size);
        if (!known) object_size = (mode_value & 2) != 0 ? 0u : UINT64_MAX;
        return lower_integer_constant(
            context, result_type, expression->type->is_unsigned,
            object_size);
    }
    if (strcmp(name, "__builtin_strlen") == 0) {
        const ExprList* argument = expression->call_args;
        const Expr* string = argument ? argument->expr : NULL;
        RccIrType result_type;
        while (string && string->kind == EXPR_CAST) string = string->cast_expr;
        if (!argument || argument->next || !string ||
            string->kind != EXPR_STRING_LIT ||
            string->str_length == SIZE_MAX || !expression->type ||
            !lower_type(expression->type, &result_type) ||
            result_type.kind != RCC_IR_TYPE_INTEGER) {
            context->unsupported = true;
            return lower_invalid_value();
        }
        return lower_integer_constant(
            context, result_type, expression->type->is_unsigned,
            (uint64_t)string->str_length);
    }
    if (strcmp(name, "__builtin_prefetch") == 0) {
        const ExprList* first = expression->call_args;
        const ExprList* second = first ? first->next : NULL;
        const ExprList* third = second ? second->next : NULL;
        int64_t rw = 0;
        int64_t locality = 3;
        RccIrLowerValue address;
        RccIrInstruction* instruction;
        RccIrLowerValue result;
        RccIrValue operand;
        if (!expression->type || expression->type->kind != TYPE_VOID ||
            !first || !first->expr || (third && third->next) ||
            (second && (!second->expr ||
                        !expr_eval_integer_constant(second->expr, &rw))) ||
            (third && (!third->expr ||
                       !expr_eval_integer_constant(third->expr, &locality))) ||
            rw < 0 || rw > 1 || locality < 0 || locality > 3) {
            context->unsupported = true;
            return lower_invalid_value();
        }
        address = lower_expression(context, first->expr);
        if (!address.valid || address.type.kind != RCC_IR_TYPE_POINTER) {
            context->unsupported = true;
            return lower_invalid_value();
        }
        operand = address.value;
        instruction = lower_append(context, RCC_IR_PREFETCH,
                                   rcc_ir_type_void(), &operand, 1u,
                                   NULL, 0u);
        if (!instruction) return lower_invalid_value();
        instruction->immediate = (uint64_t)(rw != 0 ? 4 : 0) |
            (uint64_t)(locality == 0 ? 0 : 4 - locality);
        result.type = rcc_ir_type_void();
        result.is_unsigned = false;
        result.valid = true;
        return result;
    }
    if (strcmp(name, "__builtin_clz") == 0 ||
        strcmp(name, "__builtin_clzl") == 0 ||
        strcmp(name, "__builtin_clzll") == 0 ||
        strcmp(name, "__builtin_ctz") == 0 ||
        strcmp(name, "__builtin_ctzl") == 0 ||
        strcmp(name, "__builtin_ctzll") == 0 ||
        strcmp(name, "__builtin_popcount") == 0 ||
        strcmp(name, "__builtin_popcountl") == 0 ||
        strcmp(name, "__builtin_popcountll") == 0 ||
        strcmp(name, "__builtin_parity") == 0 ||
        strcmp(name, "__builtin_parityl") == 0 ||
        strcmp(name, "__builtin_parityll") == 0 ||
        strcmp(name, "__builtin_ffs") == 0 ||
        strcmp(name, "__builtin_ffsl") == 0 ||
        strcmp(name, "__builtin_ffsll") == 0 ||
        strcmp(name, "__builtin_clrsb") == 0 ||
        strcmp(name, "__builtin_clrsbl") == 0 ||
        strcmp(name, "__builtin_clrsbll") == 0) {
        return lower_builtin_bit_count(context, expression, name);
    }
    if (strcmp(name, "__builtin_bswap16") == 0 ||
        strcmp(name, "__builtin_bswap32") == 0 ||
        strcmp(name, "__builtin_bswap64") == 0) {
        unsigned width = strcmp(name, "__builtin_bswap16") == 0 ? 16u :
            strcmp(name, "__builtin_bswap32") == 0 ? 32u : 64u;
        const ExprList* argument = expression->call_args;
        RccIrLowerValue source;
        RccIrLowerValue result;
        RccIrType type;
        uint64_t masks[8];
        unsigned shifts[8];
        bool left_shift[8];
        unsigned count;
        if (!argument || !argument->expr || argument->next ||
            !expression->type || !type_is_integer(expression->type) ||
            !lower_type(expression->type, &type) ||
            type.bit_width != width ||
            (width == 64u && g_opts.target_arch != ARCH_X64)) {
            context->unsupported = true;
            return lower_invalid_value();
        }
        source = lower_expression(context, argument->expr);
        source = lower_cast(context, source, expression->type);
        if (!source.valid || source.type.kind != RCC_IR_TYPE_INTEGER) {
            context->unsupported = true;
            return lower_invalid_value();
        }
        if (width == 16u) {
            masks[0] = UINT64_C(0x00ff);
            masks[1] = UINT64_C(0xff00);
            shifts[0] = 8u;
            shifts[1] = 8u;
            left_shift[0] = true;
            left_shift[1] = false;
            count = 2u;
        } else if (width == 32u) {
            masks[0] = UINT64_C(0x000000ff);
            masks[1] = UINT64_C(0x0000ff00);
            masks[2] = UINT64_C(0x00ff0000);
            masks[3] = UINT64_C(0xff000000);
            shifts[0] = 24u;
            shifts[1] = 8u;
            shifts[2] = 8u;
            shifts[3] = 24u;
            left_shift[0] = true;
            left_shift[1] = true;
            left_shift[2] = false;
            left_shift[3] = false;
            count = 4u;
        } else {
            masks[0] = UINT64_C(0x00000000000000ff);
            masks[1] = UINT64_C(0x000000000000ff00);
            masks[2] = UINT64_C(0x0000000000ff0000);
            masks[3] = UINT64_C(0x00000000ff000000);
            masks[4] = UINT64_C(0x000000ff00000000);
            masks[5] = UINT64_C(0x0000ff0000000000);
            masks[6] = UINT64_C(0x00ff000000000000);
            masks[7] = UINT64_C(0xff00000000000000);
            shifts[0] = 56u;
            shifts[1] = 40u;
            shifts[2] = 24u;
            shifts[3] = 8u;
            shifts[4] = 8u;
            shifts[5] = 24u;
            shifts[6] = 40u;
            shifts[7] = 56u;
            left_shift[0] = true;
            left_shift[1] = true;
            left_shift[2] = true;
            left_shift[3] = true;
            left_shift[4] = false;
            left_shift[5] = false;
            left_shift[6] = false;
            left_shift[7] = false;
            count = 8u;
        }
        result = lower_invalid_value();
        for (unsigned index = 0u; index < count; ++index) {
            RccIrLowerValue mask = lower_integer_constant(
                context, type, true, masks[index]);
            RccIrLowerValue shift = lower_integer_constant(
                context, type, true, shifts[index]);
            RccIrValue operands[2];
            RccIrInstruction* instruction;
            if (!mask.valid || !shift.valid) {
                context->unsupported = true;
                return lower_invalid_value();
            }
            operands[0] = source.value;
            operands[1] = mask.value;
            instruction = lower_append(context, RCC_IR_AND, type,
                                       operands, 2u, NULL, 0u);
            if (!instruction) return lower_invalid_value();
            operands[0] = instruction->result;
            operands[1] = shift.value;
            instruction = lower_append(
                context, left_shift[index] ? RCC_IR_SHL : RCC_IR_LSHR,
                type, operands, 2u, NULL, 0u);
            if (!instruction) return lower_invalid_value();
            if (!result.valid) {
                result = lower_value(instruction->result, type,
                                     expression->type->is_unsigned);
            } else {
                operands[0] = result.value;
                operands[1] = instruction->result;
                instruction = lower_append(context, RCC_IR_OR, type,
                                           operands, 2u, NULL, 0u);
                if (!instruction) return lower_invalid_value();
                result = lower_value(instruction->result, type,
                                     expression->type->is_unsigned);
            }
        }
        return result;
    }
    if (strcmp(name, "__builtin_expect") == 0 ||
        strcmp(name, "__builtin_expect_with_probability") == 0) {
        first = expression->call_args;
        second = first ? first->next : NULL;
        {
            const ExprList* probability = second ? second->next : NULL;
            bool has_probability = strcmp(
                name, "__builtin_expect_with_probability") == 0;
            if (!first || !first->expr || !second || !second->expr ||
                (has_probability
                    ? (!probability || probability->next)
                    : probability != NULL) ||
                !expression->type || !type_is_integer(expression->type)) {
                context->unsupported = true;
                return lower_invalid_value();
            }
        }
        /* Match the existing C/C++ backend contract: the prediction operand
         * is evaluated for language-level side effects, then the value
         * operand supplies the result.  The probability operand is a
         * validated compile-time constant, so it has no runtime lowering.
         * No unresolved builtin call symbol is emitted. */
        expected = lower_expression(context, second->expr);
        value = lower_expression(context, first->expr);
        if (!expected.valid || expected.type.kind != RCC_IR_TYPE_INTEGER ||
            !value.valid || value.type.kind != RCC_IR_TYPE_INTEGER) {
            context->unsupported = true;
            return lower_invalid_value();
        }
        return lower_cast(context, value, expression->type);
    }
    if (strcmp(name, "__builtin_constant_p") == 0) {
        const ExprList* argument = expression->call_args;
        int64_t constant = 0;
        if (!argument || argument->next || !expression->type ||
            !type_is_integer(expression->type)) {
            context->unsupported = true;
            return lower_invalid_value();
        }
        return lower_integer_constant(
            context, rcc_ir_type_integer(32u), true,
            argument->expr && expr_eval_integer_constant(
                argument->expr, &constant) ? 1u : 0u);
    }
    if (strcmp(name, "__builtin_assume_aligned") == 0) {
        first = expression->call_args;
        second = first ? first->next : NULL;
        if (!first || !first->expr || !second || !second->expr ||
            (second->next && second->next->next) || !expression->type ||
            expression->type->kind != TYPE_PTR) {
            context->unsupported = true;
            return lower_invalid_value();
        }
        value = lower_expression(context, first->expr);
        if (!value.valid || value.type.kind != RCC_IR_TYPE_POINTER) {
            context->unsupported = true;
            return lower_invalid_value();
        }
        return lower_cast(context, value, expression->type);
    }
    if (strcmp(name, "__builtin_unreachable") == 0 ||
        strcmp(name, "__builtin_trap") == 0) {
        if (expression->call_args || !expression->type ||
            expression->type->kind != TYPE_VOID ||
            !lower_append(context, RCC_IR_UNREACHABLE, rcc_ir_type_void(),
                          NULL, 0u, NULL, 0u)) {
            context->unsupported = true;
            return lower_invalid_value();
        }
        context->terminated = true;
        result.type = rcc_ir_type_void();
        result.is_unsigned = false;
        result.valid = true;
        return result;
    }
    context->unsupported = true;
    return lower_invalid_value();
}

static bool lower_wide_scalar_call(
    RccIrLowerContext* context, const Expr* expression,
    RccIrLowerWideValue* result) {
    const Decl* callee;
    Type* function_type;
    const ExprList* argument;
    TypeParam* parameter;
    size_t argument_count = 0u;
    size_t index = 0u;
    RccIrValue* operands = NULL;
    RccIrInstruction* allocation;
    RccIrInstruction* call;
    RccIrInstruction* capture;
    RccIrLowerValue address;
    if (result) memset(result, 0, sizeof(*result));
    if (!context || !expression || !result ||
        expression->cxx_close_call || !expression->call_func ||
        expression->call_func->kind != EXPR_IDENT ||
        !lower_i686_wide_scalar_type(expression->type)) {
        return false;
    }
    callee = expression->call_func->ident_decl;
    function_type = callee ? callee->type : NULL;
    if (!callee || callee->kind != DECL_FUNC || !function_type ||
        function_type->kind != TYPE_FUNC || function_type->variadic ||
        !function_type->ret_type ||
        !lower_i686_wide_scalar_type(function_type->ret_type) ||
        !type_is_compatible(function_type->ret_type, expression->type)) {
        return false;
    }
    parameter = function_type->params;
    for (argument = expression->call_args; argument;
         argument = argument->next) {
        size_t units;
        if (!parameter || !argument->expr || !argument->expr->type ||
            lower_abi_is_aggregate(parameter->type)) return false;
        units = lower_i686_wide_scalar_type(parameter->type) ? 2u : 1u;
        if (units > SIZE_MAX - argument_count) return false;
        argument_count += units;
        parameter = parameter->next;
    }
    if (parameter || argument_count > SIZE_MAX / sizeof(*operands)) {
        return false;
    }
    if (argument_count != 0u) {
        operands = rcc_alloc(argument_count * sizeof(*operands));
    }
    parameter = function_type->params;
    for (argument = expression->call_args; argument;
         argument = argument->next) {
        if (lower_i686_wide_scalar_type(parameter->type)) {
            RccIrLowerWideValue value;
            if (!lower_wide_scalar_expression(
                    context, argument->expr, &value)) {
                rcc_free(operands);
                return false;
            }
            operands[index++] = value.low.value;
            operands[index++] = value.high.value;
        } else {
            RccIrLowerValue value = lower_expression(
                context, argument->expr);
            RccIrType parameter_type;
            if (!lower_type(parameter->type, &parameter_type) ||
                (parameter_type.kind == RCC_IR_TYPE_INTEGER &&
                 parameter_type.bit_width > 32u) ||
                !value.valid) {
                rcc_free(operands);
                return false;
            }
            value = lower_cast(context, value, parameter->type);
            if (!value.valid) {
                rcc_free(operands);
                return false;
            }
            operands[index++] = value.value;
        }
        parameter = parameter->next;
    }
    allocation = lower_append(
        context, RCC_IR_ALLOCA, rcc_ir_type_pointer(0u),
        NULL, 0u, NULL, 0u);
    if (!allocation) {
        rcc_free(operands);
        return false;
    }
    rcc_ir_set_immediate(allocation, 8u);
    address = lower_value(allocation->result,
                          rcc_ir_type_pointer(0u), true);
    call = lower_append(
        context, RCC_IR_CALL, rcc_ir_type_void(), operands,
        argument_count, NULL, 0u);
    rcc_free(operands);
    if (!call) return false;
    rcc_ir_set_callee(call, decl_link_name(callee));
    capture = lower_append(
        context, RCC_IR_CAPTURE_RETURN_PAIR, rcc_ir_type_void(),
        &address.value, 1u, NULL, 0u);
    if (!capture) return false;
    rcc_ir_set_immediate(capture, 8u);
    return lower_wide_scalar_load(context, address,
                                  expression->type->is_unsigned, result);
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
        case EXPR_NOEXCEPT:
            if (!expression->cxx_noexcept_value_valid ||
                !lower_type(expression->type, &type) ||
                type.kind != RCC_IR_TYPE_INTEGER) {
                context->unsupported = true;
                return lower_invalid_value();
            }
            return lower_integer_constant(
                context, type, expression->type->is_unsigned,
                expression->cxx_noexcept_value ? 1u : 0u);
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
        case EXPR_SPACESHIP:
            return lower_spaceship(context, expression);
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
            if (lower_i686_wide_scalar_type(
                    expression->unary_operand->type)) {
                RccIrLowerWideValue wide_operand;
                if (!lower_wide_scalar_expression(
                        context, expression->unary_operand, &wide_operand)) {
                    return lower_invalid_value();
                }
                operand = lower_wide_scalar_truth(context, wide_operand);
            } else {
                operand = lower_expression(
                    context, expression->unary_operand);
                operand = lower_truth(context, operand);
            }
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
            if (expression->type && expression->type->is_reference &&
                (expression->cxx_cast_kind == CXX_CAST_NONE ||
                 expression->cxx_cast_kind == CXX_CAST_CONST ||
                 expression->cxx_cast_kind == CXX_CAST_DYNAMIC)) {
                RccIrLowerValue address = lower_lvalue_address(
                    context, expression);
                return lower_load_address(context, address,
                                          expression->type->base);
            }
            operand = lower_expression(context, expression->cast_expr);
            operand = lower_cast(context, operand, expression->type);
            if (expression->cxx_pointer_adjustment_valid) {
                operand = lower_adjusted_pointer(
                    context, operand, expression->cxx_pointer_adjustment);
            }
            return operand;
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
            if (expression->cxx_typeinfo_hash_code) {
                return lower_typeinfo_field(
                    context, expression, 0u,
                    g_opts.target_arch == ARCH_X64 ? type_ulong : type_uint);
            }
            if (expression->cxx_typeinfo_name) {
                return lower_typeinfo_field(
                    context, expression,
                    g_opts.target_arch == ARCH_X64 ? 8u : 4u,
                    type_ptr(type_char));
            }
            if (expression->cxx_typeinfo_before) {
                return lower_typeinfo_before(context, expression);
            }
            if (expression->call_func &&
                expression->call_func->kind == EXPR_IDENT &&
                expression->call_func->ident_name &&
                (strcmp(expression->call_func->ident_name,
                        "__builtin_expect") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_expect_with_probability") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_constant_p") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_assume_aligned") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_unreachable") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_trap") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_prefetch") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_clz") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_clzl") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_clzll") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_ctz") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_ctzl") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_ctzll") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_popcount") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_popcountl") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_popcountll") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_parity") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_parityl") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_parityll") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_ffs") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_ffsl") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_ffsll") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_clrsb") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_clrsbl") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_clrsbll") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_add_overflow") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_sub_overflow") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_mul_overflow") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_object_size") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_strlen") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_bswap16") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_bswap32") == 0 ||
                 strcmp(expression->call_func->ident_name,
                        "__builtin_bswap64") == 0)) {
                return lower_builtin_call(context, expression);
            }
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
            size = expression->str_length;
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
        case EXPR_CXX_REQUIRES:
        case EXPR_GENERIC:
        case EXPR_CXX_FOLD:
        case EXPR_VA_START:
        case EXPR_VA_END:
        case EXPR_VA_COPY:
        case EXPR_VA_ARG:
            context->unsupported = true;
            return lower_invalid_value();
        case EXPR_CXX_TYPEID:
            return lower_cxx_typeid_address(context, expression);
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
    RccIrLowerWideValue left_wide;
    RccIrLowerWideValue right_wide;
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
    if (lower_i686_wide_scalar_type(expression->binary_lhs->type)) {
        if (!lower_wide_scalar_expression(
                context, expression->binary_lhs, &left_wide)) {
            return lower_invalid_value();
        }
        left = lower_wide_scalar_truth(context, left_wide);
    } else {
        left = lower_expression(context, expression->binary_lhs);
    }
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
    if (lower_i686_wide_scalar_type(expression->binary_rhs->type)) {
        if (!lower_wide_scalar_expression(
                context, expression->binary_rhs, &right_wide)) {
            return lower_invalid_value();
        }
        right = lower_wide_scalar_truth(context, right_wide);
    } else {
        right = lower_expression(context, expression->binary_rhs);
        right = lower_truth(context, right);
    }
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
            return true;
        default:
            return false;
    }
}

static bool lower_nested_label_entries(RccIrLowerContext* context,
                                       const Stmt* statement) {
    if (!context || !statement) return false;
    switch (statement->kind) {
        case STMT_LABEL: {
            RccIrLowerLabel* label = lower_find_label_statement(
                context, statement);
            if (!label) {
                context->unsupported = true;
                return false;
            }
            context->current = label->block;
            context->terminated = false;
            return statement->label_stmt
                ? lower_statement(context, statement->label_stmt) : true;
        }
        case STMT_BLOCK:
            for (const StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                if (item->stmt &&
                    lower_statement_has_switch_label(item->stmt) &&
                    !lower_nested_label_entries(context, item->stmt)) {
                    return false;
                }
            }
            return true;
        case STMT_IF:
            if (statement->if_then &&
                lower_statement_has_switch_label(statement->if_then) &&
                !lower_nested_label_entries(context, statement->if_then)) {
                return false;
            }
            if (statement->if_else &&
                lower_statement_has_switch_label(statement->if_else) &&
                !lower_nested_label_entries(context, statement->if_else)) {
                return false;
            }
            return true;
        case STMT_WHILE:
        case STMT_DO:
            return statement->while_body &&
                lower_statement_has_switch_label(statement->while_body)
                ? lower_nested_label_entries(context, statement->while_body)
                : true;
        case STMT_FOR:
            return statement->for_body &&
                lower_statement_has_switch_label(statement->for_body)
                ? lower_nested_label_entries(context, statement->for_body)
                : true;
        case STMT_SWITCH:
            /* Nested case/default entries belong to lower_switch, not to a
             * goto that bypasses this switch. */
            return true;
        default:
            return true;
    }
}

static bool lower_block(RccIrLowerContext* context, const Stmt* block) {
    if (!block || block->kind != STMT_BLOCK) {
        return lower_statement(context, block);
    }
    for (const StmtList* item = block->block_stmts; item;
         item = item->next) {
        if (context->terminated && item->stmt->kind != STMT_LABEL &&
            !lower_statement_has_switch_label(item->stmt)) {
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
            if (statement->if_then &&
                !lower_collect_switch_labels(
                    context, switch_context, statement->if_then)) {
                return false;
            }
            if (statement->if_else &&
                !lower_collect_switch_labels(
                    context, switch_context, statement->if_else)) {
                return false;
            }
            return true;
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
        } else if (lower_i686_wide_scalar_type(parameter->type)) {
            units = 2u;
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
            if (lower_i686_wide_scalar_type(parameter->type)) {
                if (types) {
                    types[cursor] = rcc_ir_type_integer(32u);
                    types[cursor + 1u] = rcc_ir_type_integer(32u);
                }
                cursor += 2u;
                continue;
            }
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
        size_t text_size = string->str_length;
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
    bool wide_scalar = declaration &&
        lower_i686_wide_scalar_type(declaration->type);
    bool wide_ssa = wide_scalar && !declaration->type->is_volatile &&
        declaration->var_init != NULL;
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
             : (!wide_scalar && !lower_type(declaration->type, &type))) ||
        (!is_array && !is_struct && !is_union &&
         !wide_scalar && type.kind == RCC_IR_TYPE_VOID)) {
        context->unsupported = true;
        return false;
    }
    allocation = lower_append(context, RCC_IR_ALLOCA,
                              rcc_ir_type_pointer(0u), NULL, 0u, NULL, 0u);
    if (!allocation) return false;
    rcc_ir_set_immediate(allocation,
                         declaration->type->size > 0
                             ? (uint64_t)declaration->type->size : 1u);
    if (is_array || is_struct || is_union || wide_scalar) {
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
    if (wide_scalar && declaration->var_init) {
        RccIrLowerWideValue initializer;
        if (!lower_wide_scalar_expression(
                context, declaration->var_init, &initializer) ||
            !lower_wide_scalar_store(
                context,
                lower_value(allocation->result,
                            rcc_ir_type_pointer(0u), true),
                initializer)) {
            context->unsupported = true;
            return false;
        }
        if (wide_ssa && !lower_set_wide_local_ssa(
                context, declaration, initializer)) {
            context->unsupported = true;
            return false;
        }
        return true;
    }
    if (declaration->var_init) {
        RccIrLowerValue initializer;
        Expr target;
        if (declaration->type->is_reference) {
            initializer = lower_lvalue_address(context, declaration->var_init);
            if (!initializer.valid || initializer.type.kind !=
                    RCC_IR_TYPE_POINTER ||
                !lower_store_address(
                    context,
                    lower_value(allocation->result,
                                rcc_ir_type_pointer(0u), true),
                    initializer)) return false;
            return true;
        }
        initializer = lower_expression(context, declaration->var_init);
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
    if (handler->type && handler->type->kind == TYPE_PTR &&
        handler->type->is_reference && handler->type->base &&
        handler->type->base->kind != TYPE_STRUCT &&
        handler->type->base->kind != TYPE_UNION) {
        /* Scalar and pointer throws keep the value inline in the frame.  A
         * reference catch aliases that inline word, rather than treating the
         * word as an object address. */
        return lower_byte_offset_address(
            context, frame, value_offset);
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
            handler->parameter->type->kind == TYPE_PTR &&
            handler->parameter->type->is_reference) {
            if (!lower_store_address(
                    context,
                    lower_value(local->address,
                                rcc_ir_type_pointer(0u), true),
                    value)) return false;
        } else if (handler->parameter->type &&
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
    if (context && context->terminated && statement &&
        statement->kind != STMT_LABEL && statement->kind != STMT_CASE &&
        statement->kind != STMT_DEFAULT && !context->current_switch &&
        lower_statement_has_switch_label(statement)) {
        return lower_nested_label_entries(context, statement);
    }
    if (!context || context->unsupported || !statement) {
        if (context) context->unsupported = true;
        return false;
    }
    if (context->terminated && statement->kind != STMT_LABEL &&
        !lower_statement_has_switch_label(statement)) {
        return true;
    }
    switch (statement->kind) {
        case STMT_NULL:
            return true;
        case STMT_EXPR:
            if (statement->expr && lower_i686_wide_scalar_type(
                    statement->expr->type)) {
                RccIrLowerWideValue value;
                (void)lower_wide_scalar_expression(
                    context, statement->expr, &value);
            } else {
                (void)lower_expression(context, statement->expr);
            }
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
            } else if (lower_i686_wide_scalar_type(
                           context->ast_return_type)) {
                RccIrLowerWideValue value;
                RccIrValue return_values[2];
                RccIrInstruction* return_instruction;
                /* i686 cdecl returns an unsigned/signed 64-bit scalar in
                 * EDX:EAX.  The verified bridge represents the value as two
                 * real i32 SSA words and only accepts operations for which
                 * carry/borrow and memory layout are modeled explicitly. */
                if (!statement->return_val ||
                    !lower_wide_scalar_expression(
                        context, statement->return_val, &value)) {
                    context->unsupported = true;
                    return false;
                }
                if (!lower_cxx_exception_release_frame(
                        context, context->active_exception_frame)) return false;
                return_values[0] = value.low.value;
                return_values[1] = value.high.value;
                return_instruction = lower_append(
                    context, RCC_IR_RETURN, rcc_ir_type_void(),
                    return_values, 2u, NULL, 0u);
                if (!return_instruction) return false;
                rcc_ir_set_immediate(return_instruction, 8u);
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
        case STMT_GOTO: {
            RccIrLowerLabel* label;
            if (statement->goto_cleanup_count != 0u ||
                statement->goto_vla_count != 0u) {
                context->unsupported = true;
                return false;
            }
            label = lower_find_label_name(context, statement->goto_label);
            if (!label || !lower_branch(context, label->block->id)) {
                context->unsupported = true;
                return false;
            }
            return true;
        }
        case STMT_LABEL: {
            RccIrLowerLabel* label = lower_find_label_statement(
                context, statement);
            if (!label) {
                context->unsupported = true;
                return false;
            }
            if (!context->terminated && context->current != label->block &&
                !lower_branch(context, label->block->id)) {
                return false;
            }
            context->current = label->block;
            context->terminated = false;
            return statement->label_stmt
                ? lower_statement(context, statement->label_stmt) : true;
        }
        case STMT_SWITCH:
            return lower_switch(context, statement);
        case STMT_CASE:
        case STMT_DEFAULT:
            return lower_switch_case(context, statement);
        case STMT_TRY:
            return lower_cxx_try(context, statement);
        case STMT_THROW:
            return lower_cxx_throw(context, statement);
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
        bool wide_scalar = item && lower_i686_wide_scalar_type(item->type);
        size_t units = aggregate
            ? ((size_t)item->type->size + lower_abi_chunk_size() - 1u) /
                lower_abi_chunk_size()
            : (wide_scalar ? 2u : 1u);
        size_t allocation_size = aggregate
            ? units * lower_abi_chunk_size()
            : (wide_scalar ? 8u
               : (item && item->type && item->type->size > 0
                   ? (size_t)item->type->size : 1u));
        if (!item || item->kind != DECL_PARAM ||
            index > context->function->parameter_count ||
            units > context->function->parameter_count - index ||
            (aggregate
                 ? !lower_abi_aggregate_supported(item->type)
                 : wide_scalar
                 ? false
                 : !lower_abi_native_scalar_type(item->type, &type))) {
            context->unsupported = true;
            return false;
        }
        allocation = lower_append(context, RCC_IR_ALLOCA,
                                  rcc_ir_type_pointer(0u), NULL, 0u,
                                  NULL, 0u);
        if (!allocation) return false;
        rcc_ir_set_immediate(allocation, (uint64_t)allocation_size);
        if (aggregate || wide_scalar) type = rcc_ir_type_pointer(0u);
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
        } else if (wide_scalar) {
            RccIrLowerValue base = lower_value(
                allocation->result, rcc_ir_type_pointer(0u), true);
            for (size_t unit = 0u; unit < 2u; ++unit) {
                RccIrLowerValue address = lower_byte_offset_address(
                    context, base, (uint64_t)(unit * 4u));
                RccIrLowerValue value = lower_value(
                    context->function->parameters[index + unit],
                    context->function->parameter_types[index + unit], true);
                if (!address.valid || !lower_store_address(
                        context, address, value)) return false;
            }
            if (item->type->is_const && !item->type->is_volatile) {
                RccIrLowerWideValue value;
                value.low = lower_value(
                    context->function->parameters[index],
                    context->function->parameter_types[index], true);
                value.high = lower_value(
                    context->function->parameters[index + 1u],
                    context->function->parameter_types[index + 1u], true);
                value.is_unsigned = item->type->is_unsigned;
                value.valid = value.low.valid && value.high.valid;
                if (!lower_set_wide_local_ssa(context, item, value)) {
                    context->unsupported = true;
                    return false;
                }
            }
            index += 2u;
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
    } else if (g_opts.target_arch != ARCH_X64 &&
               type_is_integer(declaration->type->ret_type) &&
               declaration->type->ret_type->size == 8) {
        return_type = rcc_ir_type_integer(64u);
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
        !lower_collect_labels(&context, declaration->func_body) ||
        !lower_statement(&context, declaration->func_body)) {
        lower_release_locals(context.locals);
        lower_release_labels(context.labels);
        rcc_ir_module_destroy(module);
        return context.unsupported ? RCC_IR_LOWER_UNSUPPORTED
                                   : RCC_IR_LOWER_INVALID;
    }
    if (!context.terminated) {
        if (return_type.kind != RCC_IR_TYPE_VOID ||
            !lower_append(&context, RCC_IR_RETURN, rcc_ir_type_void(),
                          NULL, 0u, NULL, 0u)) {
            lower_release_locals(context.locals);
            lower_release_labels(context.labels);
            rcc_ir_module_destroy(module);
            return RCC_IR_LOWER_UNSUPPORTED;
        }
        context.terminated = true;
    }
    lower_release_locals(context.locals);
    lower_release_labels(context.labels);
    context.labels = NULL;
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
