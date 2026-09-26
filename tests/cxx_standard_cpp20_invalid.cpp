template<typename T>
concept StandardModeConcept = true;

consteval int standard_mode_consteval() {
    return 1;
}

int standard_mode_requires() {
    return requires { 1; } ? 0 : 1;
}
