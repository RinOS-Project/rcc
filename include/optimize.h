/*
 * RCC - Target-independent AST optimization passes
 */

#ifndef OPTIMIZE_H
#define OPTIMIZE_H

#include "ast.h"

void rcc_optimize(AST* ast);
bool rcc_optimize_inline_debug_info(const Expr* expression,
                                   const Decl** caller,
                                   const Decl** callee,
                                   SourceLoc* call_location,
                                   const ExprList** arguments);
size_t rcc_optimize_inline_debug_scope_count(const Expr* expression);
bool rcc_optimize_inline_debug_scope(const Expr* expression, size_t ordinal,
                                     const Decl** function,
                                     const Stmt** scope);
size_t rcc_optimize_inline_debug_parameter_value_count(
    const Expr* expression);
bool rcc_optimize_inline_debug_parameter_value(
    const Expr* expression, size_t ordinal, const Decl** callee,
    const Decl** parameter, const Expr** value, const Stmt** statement);
size_t rcc_optimize_inline_debug_statement_count(const Expr* expression);
bool rcc_optimize_inline_debug_statement(const Expr* expression,
                                         size_t ordinal,
                                         const Decl** function,
                                         const Stmt** statement);

#endif /* OPTIMIZE_H */
