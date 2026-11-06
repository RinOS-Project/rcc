using Function = void();

void function_designator() {}

constexpr int select_function_reference(Function&&) {
    return 22;
}

constexpr int select_function_reference(Function&) {
    return 11;
}

int accept_function_rvalue_reference(Function&&) {
    return 22;
}

int main() {
    return select_function_reference(function_designator) == 11 &&
                   accept_function_rvalue_reference(function_designator) == 22
        ? 0
        : 1;
}
