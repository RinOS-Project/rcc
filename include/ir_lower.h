/*
 * RCC - AST to typed SSA IR lowering
 */

#ifndef RCC_IR_LOWER_H
#define RCC_IR_LOWER_H

#include "ast.h"
#include "ir.h"

typedef enum {
    RCC_IR_LOWER_OK,
    RCC_IR_LOWER_UNSUPPORTED,
    RCC_IR_LOWER_INVALID,
} RccIrLowerStatus;

RccIrLowerStatus rcc_ir_lower_function(const Decl* declaration,
                                       RccIrModule** module_out,
                                       char* error, size_t error_size);
bool rcc_ir_verify_ast_subset(const AST* ast, size_t* lowered_functions,
                              char* error, size_t error_size);

#endif /* RCC_IR_LOWER_H */
