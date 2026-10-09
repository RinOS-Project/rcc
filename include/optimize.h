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
                                   SourceLoc* call_location);

#endif /* OPTIMIZE_H */
