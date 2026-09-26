int standard_mode_if_constexpr() {
    if constexpr (true) {
        return 1;
    }
    return 0;
}

int standard_mode_constexpr_lambda() {
    return []() constexpr { return 1; }();
}

template<typename... Values>
int standard_mode_fold(Values... values) {
    return (values + ...);
}

template<auto Value>
int standard_mode_auto_nttp() {
    return Value;
}

namespace standard_mode::nested {
int standard_mode_nested_namespace() {
    return 1;
}
}

inline int standard_mode_inline_variable = 1;

struct StandardModeDesignated {
    int value;
};

int standard_mode_designated() {
    StandardModeDesignated value{.value = 1};
    return value.value;
}
