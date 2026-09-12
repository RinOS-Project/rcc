/*
 * RCC - C-only frontend boundary for C++ direct-list template parsing
 */

#include "ast.h"

Type* rcc_parse_cxx_direct_list_type(void) {
    return NULL;
}

Type* rcc_parse_cxx_type_name(void) {
    return NULL;
}

bool rcc_parse_cxx_type_start(void) {
    return false;
}

Expr* rcc_parse_cxx_template_call(void) {
    return NULL;
}

Stmt* rcc_parse_cxx_auto_local_declaration(void) {
    return NULL;
}

/*
 * The C parser shares its expression implementation with rcc++.  C mode
 * never reaches the C++-only token path, but it still needs a concrete
 * frontend boundary so the standalone rcc link remains independent of the
 * C++ parser object.
 */
Expr* rcc_parse_cxx_special_expression(void) {
    return NULL;
}
