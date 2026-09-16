int free_noexcept(int value) noexcept {
    return value + 1;
}

int free_noexcept_condition(int value) noexcept(1) {
    return value;
}
