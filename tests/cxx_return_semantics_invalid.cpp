int& return_lvalue_reference_from_temporary() {
    return 1;
}

int&& return_rvalue_reference_from_lvalue() {
    int value = 2;
    return value;
}

int return_nonconvertible_value() {
    return "not an integer";
}

void return_value_from_void_function() {
    return 3;
}

int return_without_value() {
    return;
}
