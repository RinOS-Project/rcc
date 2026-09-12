struct Incomplete;

int invalid_compound_literal(void) {
    return (struct Incomplete){ 1 }.missing;
}
