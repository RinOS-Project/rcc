int runtime_value(void);

constinit int nonconstant_global = runtime_value();

int invalid_local(void) {
    constinit int automatic_local = 3;
    return automatic_local;
}

constexpr constinit int invalid_combination = 4;
