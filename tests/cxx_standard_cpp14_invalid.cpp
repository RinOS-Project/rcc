int standard_mode_structured_binding() {
    int values[2] = {1, 2};
    auto [first, second] = values;
    return first + second;
}

int standard_mode_generic_lambda() {
    auto increment = [](auto value) { return value + 1; };
    return increment(1);
}

inline int standard_mode_inline_variable = 1;

struct StandardModeDesignated {
    int value;
};

int standard_mode_designated() {
    StandardModeDesignated value{.value = 1};
    return value.value;
}
