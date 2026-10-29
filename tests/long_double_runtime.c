#include <stdarg.h>

static long double read_long_double(double prefix, int marker, ...) {
    va_list args;
    long double value;
    double trailing;

    if (marker != 7)
        return -1.0L;

    va_start(args, marker);
    value = va_arg(args, long double);
    trailing = va_arg(args, double);
    va_end(args);
    return value + (prefix - 2.25) + (trailing - 4.5);
}

static long double return_long_double(long double value) {
    return value;
}

static long double rounding_tie = 0x1.00000000000008p+0L;

int main(void) {
    long double value = return_long_double(
        read_long_double(2.25, 7, 3.5L, 4.5));

    if (value != 3.5L)
        return 1;
    if (rounding_tie != 1.0L)
        return 2;
    return 0;
}
