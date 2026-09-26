template<typename T>
concept StandardModeConcept = true;

consteval int standard_mode_consteval() {
    return 1;
}

int standard_mode_requires() {
    return requires { 1; } ? 0 : 1;
}

struct StandardModeDesignated {
    int value;
};

int standard_mode_designated() {
    StandardModeDesignated value{.value = 1};
    return value.value;
}
