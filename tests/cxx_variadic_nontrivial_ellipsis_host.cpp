extern "C" int nontrivial_variadic_copies;
extern "C" int nontrivial_variadic_destructions;

struct NonTrivialVariadicPayload {
    int integer;
    double floating;

    NonTrivialVariadicPayload(int integer_value, double floating_value)
        : integer(integer_value), floating(floating_value) {}

    NonTrivialVariadicPayload(const NonTrivialVariadicPayload& other)
        : integer(other.integer), floating(other.floating) {
        ++nontrivial_variadic_copies;
    }

    ~NonTrivialVariadicPayload() {
        ++nontrivial_variadic_destructions;
    }
};

#if defined(RCC_VARIADIC_HOST_CALLEE)
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
#else
extern int read_nontrivial_variadic_payload(
    int marker0, int marker1, int marker2, int marker3, int marker4,
    int marker5, ...);

extern "C" {
int nontrivial_variadic_copies = 0;
int nontrivial_variadic_destructions = 0;
}

int main() {
    NonTrivialVariadicPayload payload(11, 3.5);
    int result = read_nontrivial_variadic_payload(
        2, 0, 0, 0, 0, 0, payload);
    return result == 13 && nontrivial_variadic_copies == 1 &&
                   nontrivial_variadic_destructions == 1
        ? 0 : 1;
}
#endif
