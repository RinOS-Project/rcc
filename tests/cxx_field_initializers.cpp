/* C++ default member initializer coverage. */

struct Defaults {
    int first = 3;
    int second{4};
};

Defaults global_defaults;

static int local_defaults(void) {
    Defaults value;
    return value.first * 10 + value.second;
}

static int explicit_defaults(void) {
    Defaults value{9};
    return value.first * 10 + value.second;
}

static int value_initialized_defaults(void) {
    Defaults value{};
    return value.first * 10 + value.second;
}

int main(void) {
    return global_defaults.first == 3 && global_defaults.second == 4 &&
                   local_defaults() == 34 &&
                   explicit_defaults() == 94 &&
                   value_initialized_defaults() == 34
               ? 0
               : 1;
}
