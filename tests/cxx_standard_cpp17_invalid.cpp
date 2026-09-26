int standard_mode_if_constexpr() {
    if constexpr (true) {
        return 1;
    }
    return 0;
}
