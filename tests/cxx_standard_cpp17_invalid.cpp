int standard_mode_if_constexpr() {
    if constexpr (true) {
        return 1;
    }
    return 0;
}

inline int standard_mode_inline_variable = 1;

struct StandardModeDesignated {
    int value;
};

int standard_mode_designated() {
    StandardModeDesignated value{.value = 1};
    return value.value;
}
