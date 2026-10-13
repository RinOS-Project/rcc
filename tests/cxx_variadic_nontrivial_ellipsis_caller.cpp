extern "C" {
int nontrivial_variadic_copies = 0;
int nontrivial_variadic_destructions = 0;
}

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

int read_nontrivial_variadic_payload(
    int marker0, int marker1, int marker2, int marker3, int marker4,
    int marker5, ...);
int read_nontrivial_variadic_payload_registers(int marker, ...);

int main() {
    NonTrivialVariadicPayload payload(11, 3.5);
    int result = read_nontrivial_variadic_payload(
        2, 0, 0, 0, 0, 0, payload);
    int registers_result =
        read_nontrivial_variadic_payload_registers(2, payload, 7.5);
    return result == 13 && registers_result == 113 &&
                   nontrivial_variadic_copies == 2 &&
                   nontrivial_variadic_destructions == 2
        ? 0 : 1;
}
