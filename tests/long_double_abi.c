#ifdef __cplusplus
static_assert(sizeof(long double) == 8,
              "RinOS long double uses the binary64 ABI");
#else
_Static_assert(sizeof(long double) == 8,
               "RinOS long double uses the binary64 ABI");
#endif

long double rin_long_double_identity(long double value) {
    return value;
}

long double rin_long_double_add(long double left, long double right) {
    return left + right;
}

long double rin_long_double_literal = 0.0L;

int main(void) {
    return rin_long_double_add(rin_long_double_identity(1.0L),
                               rin_long_double_literal) != 1.0L;
}
