unsigned long long invalid_wide_compound_lvalue(unsigned long long value,
                                                unsigned long long operand) {
    return (value + operand) *= operand;
}
