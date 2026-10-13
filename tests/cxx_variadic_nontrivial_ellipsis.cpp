struct NonTrivialVariadicPayload {
    int integer;
    double floating;

    NonTrivialVariadicPayload(int integer_value, double floating_value)
        : integer(integer_value), floating(floating_value) {}

    NonTrivialVariadicPayload(const NonTrivialVariadicPayload& other)
        : integer(other.integer), floating(other.floating) {}

    ~NonTrivialVariadicPayload() {}
};

int read_nontrivial_variadic_payload(
    int marker0, int marker1, int marker2, int marker3, int marker4,
    int marker5, ...) {
    __builtin_va_list arguments;
    __builtin_va_start(arguments, marker5);
    int value = __builtin_va_arg(
        arguments, NonTrivialVariadicPayload).integer;
    __builtin_va_end(arguments);
    return marker0 + marker1 + marker2 + marker3 + marker4 + marker5 + value;
}
