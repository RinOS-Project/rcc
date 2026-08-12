int reject_nullptr_arithmetic() {
    return nullptr + 1;
}

int reject_nullptr_integer_operator() {
    return ~nullptr;
}

int reject_nullptr_relational_comparison() {
    return nullptr < nullptr;
}

int reject_nullptr_auto_deduction() {
    auto value = nullptr;
    return value == 0;
}

int reject_nullptr_integer_assignment() {
    int value = 1;
    value = nullptr;
    return value;
}
