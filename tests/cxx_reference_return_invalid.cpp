int& return_lvalue_reference_from_temporary() {
    return 1;
}

int&& return_rvalue_reference_from_lvalue() {
    int value = 2;
    return value;
}
