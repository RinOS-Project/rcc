int reject_nullptr_arithmetic() {
    return nullptr + 1;
}

int reject_nullptr_integer_operator() {
    return ~nullptr;
}

int reject_nullptr_relational_comparison() {
    return nullptr < nullptr;
}

int reject_nullptr_variable_arithmetic() {
    auto value = nullptr;
    return value + 1;
}

int reject_nullptr_integer_assignment() {
    auto null_value = nullptr;
    int value = 1;
    value = null_value;
    return value;
}
