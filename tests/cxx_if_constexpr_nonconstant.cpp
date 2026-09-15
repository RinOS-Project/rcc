int runtime_value(void);

int probe_if_constexpr_nonconstant(void) {
    if constexpr (runtime_value()) {
        return 1;
    }
    return 0;
}
